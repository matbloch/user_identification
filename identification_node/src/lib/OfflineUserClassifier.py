#!/usr/bin/env python2

import os
import time

import numpy as np
import pickle

import openface
import openface.helper
from openface.data import iterImgs

# path managing
fileDir = os.path.dirname(os.path.realpath(__file__))
modelDir = os.path.join(fileDir, '../..', 'models')	# path to the model directory
dlibModelDir = os.path.join(modelDir, 'dlib')		# dlib face detector model
openfaceModelDir = os.path.join(modelDir, 'openface')
classifierModelDir = os.path.join(modelDir, 'classification')

# classifiers
from sklearn.svm import SVC
from sklearn.preprocessing import LabelEncoder

# argument container
# TODO: refactor this properly!
class Arguments:
    def __init__(self):
        self.dlibFacePredictor = "shape_predictor_68_face_landmarks.dat"
        self.landmarks = "outerEyesAndNose"
        self.size = 96
        self.skipMulti = True
        self.verbose = True
        # embedding calculation
        self.networkModel = os.path.join(openfaceModelDir,'nn4.small2.v1.t7') # torch network model
        self.cuda = False


class OfflineUserClassifier:
    # key: user id, value: list of embeddings
    user_embeddings = {}    # raw embeddings
    user_list = {}    # user ids to nice name
    id_increment = 1        # user id increment

    neural_net = None       # torch network
    dlib_aligner = None     # dlib face aligner
    classifier = None       # classifier
    label_encoder = None    # classifier label encoder
    training_status = False

    def __init__(self):
        args = Arguments()

        start = time.time()
        print "--- loading models..."
        # load neural net
        self.neural_net = openface.TorchNeuralNet(args.networkModel, imgDim=args.size, cuda=args.cuda)
        # load dlib model
        self.dlib_aligner = openface.AlignDlib(dlibModelDir + "/" + args.dlibFacePredictor)
        # initialize classifier
        self.classifier = SVC(C=1, kernel='linear', probability=True)

        # load stored classifier
        self.training_status = self.load_classifier()

        print("--- identifier initialization took {} seconds".format(time.time() - start))

    def load_classifier(self):
        filename = "{}/svm_classifier.pkl".format(classifierModelDir)
        if os.path.isfile(filename):
            with open(filename, 'r') as f:
                (self.id_increment, self.user_list, self.label_encoder, self.classifier) = pickle.load(f)
            return True
        return False

    def store_classifier(self):
        if self.training_status is False:
            print("--- Classifier is not trained yet")
            return

        filename = "{}/svm_classifier.pkl".format(classifierModelDir)
        print("--- Saving classifier to '{}'".format(filename))
        with open(filename, 'wb') as f:
            pickle.dump((self.id_increment, self.user_list, self.label_encoder, self.classifier), f)

    def collect_embeddings_for_specific_id(self, images, user_id):
        """collect embeddings of faces to train detect - for a single user id"""

        # force integer id
        user_id = int(user_id)

        args = Arguments()

        print "--- Starting normalization..."
        # normalize images
        images_normalized = []
        start = time.time()
        if len(images) > 0:
            for imgObject in images:
                # align face - ignore images with multiple bounding boxes
                aligned = self.align_face(imgObject, args.landmarks, args.size)
                if aligned is not None:
                    images_normalized.append(aligned)

        if len(images_normalized) > 0:
            print("--- Alignment took {} seconds - " + str(len(images_normalized)) + "/" + str(len(images)) + " images suitable".format(time.time() - start))

        else:
            print "--- No suitable images (no faces detected)"

        # generate embeddings
        reps = []
        for img in images_normalized:
            start = time.time()
            rep = self.neural_net.forward(img)
            print("--- = Neural network forward pass took {} seconds.".format(
                time.time() - start))
            reps.append(rep)

        # save
        if user_id in self.user_embeddings:
            # append
            print "--- Appending "+str(len(reps))+" embeddings"
            #self.user_embeddings[user_id].append(reps)
            self.user_embeddings[user_id] = np.concatenate((self.user_embeddings[user_id], reps))
        else:
            self.user_embeddings[user_id] = reps

        # display current representations
        self.print_embedding_status()
        self.print_users()

    def identify_user(self, user_img):

        if self.training_status is False:
            return (None, None, None)

        start = time.time()
        embedding = self.get_embedding(user_img)

        if embedding is None:
            return (None, None, None)

        embedding = embedding.reshape(1, -1)

        # alternative - predicts index of label array
        # user_id = self.classifier.predict(embedding)

        # prediction probabilities
        probabilities = self.classifier.predict_proba(embedding).ravel()
        maxI = np.argmax(probabilities)
        confidence = probabilities[maxI]
        user_id_pred = self.label_encoder.inverse_transform(maxI)


        nice_name = self.user_list[int(user_id_pred)]

        if np.shape(probabilities)[0] > 1:
            for i, prob in enumerate(probabilities):
                # label encoder id: np.int64()
                label = self.label_encoder.inverse_transform(np.int64(i))
                print "    label: "+ str(label) + " | prob: " + str(prob)

        print("--- Identification took {} seconds.".format(time.time() - start))
        return (user_id_pred, nice_name, confidence)

    #  ----------- OFFLINE CLASSIFICATION UTILITIES

    def get_user_id_from_name(self, search_name):
        for user_id, name in self.user_list.iteritems():
            if name == search_name:
                return user_id
        return 0

    #  ----------- UTILITIES

    def create_new_user(self, nice_name):
        # generate unique id
        new_id = self.id_increment
        self.id_increment += 1  # increment user id
        # save nice name
        self.user_list[int(new_id)] = nice_name
        return new_id

    def get_nice_name(self, user_id):
        if user_id in self.user_list:
            return self.user_list[user_id]
        else:
            return ""

    def get_embedding(self, user_img):
        args = Arguments()
        # align image
        normalized = self.align_face(user_img, args.landmarks, args.size)
        if normalized is None:
            return None

        # generate embedding
        rep = self.neural_net.forward(normalized)
        return rep

    def print_embedding_status(self):
        print "--- Current embeddings:"
        for user_id, embeddings in self.user_embeddings.iteritems():
            print "     User" + str(user_id) + ": " + str(len(embeddings)) + " representations"

    def print_users(self):
        print "--- Current users:"
        for user_id, name in self.user_list.iteritems():
            print "     User: ID(" + str(user_id) + "): " + name

    def trigger_training(self):
        """triggers the detector training from the collected faces"""
        print "--- Triggered classifier training"

        if len(self.user_embeddings) < 2:
            print "--- Number of users must be greater than one. Trying to load stored model..."

            if self.load_classifier() is True:
                print "--- Classifier loaded from file."
            else:
                print "--- Could not find classifier model."
            return

        start = time.time()
        embeddings_accumulated = np.array([])
        labels = []
        # label encoder id: np.int64()
        for user_id, user_embeddings in self.user_embeddings.iteritems():
            # add label
            labels = np.append(labels, np.repeat(user_id, len(user_embeddings)))
            # print(user_embeddings)
            embeddings_accumulated = np.concatenate((embeddings_accumulated, user_embeddings)) if embeddings_accumulated.size else np.array(user_embeddings)

        # transform to numerical labels
        self.label_encoder = LabelEncoder().fit(labels)

        # get numerical labels
        labels_numeric = self.label_encoder.transform(labels)

        # train classifier
        self.classifier.fit(embeddings_accumulated, labels_numeric)
        print("    Classifier training took {} seconds".format(time.time() - start))
        self.training_status = True

        # store classifier
        self.store_classifier()

    def align_face(self, image, landmark, output_size, skip_multi=False):

        landmarkMap = {
            'outerEyesAndNose': openface.AlignDlib.OUTER_EYES_AND_NOSE,
            'innerEyesAndBottomLip': openface.AlignDlib.INNER_EYES_AND_BOTTOM_LIP
        }
        if landmark not in landmarkMap:
            raise Exception("Landmarks unrecognized: {}".format(landmark))

        landmarkIndices = landmarkMap[landmark]

        # TODO: check if is really output size or input size
        # align image
        outRgb = self.dlib_aligner.align(output_size, image,
                             landmarkIndices=landmarkIndices,
                             skipMulti=skip_multi)
        if outRgb is None:
            print("--- Unable to align.")

        return outRgb