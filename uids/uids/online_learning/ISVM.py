from uids.utils.DataAnalysis import *
from sklearn.svm import SVC
from uids.data_models.HullCluster import HullCluster
from uids.data_models.MeanShiftCluster import MeanShiftCluster
from uids.data_models.ClusterBase import ClusterBase


class ISVM:

    __verbose = False

    clf = None
    uncertainty_thresh = 0.7

    random_data = None
    data_cluster = None

    # prediction
    prediction = None
    probability = None

    def __init__(self, random_data, cluster=None):
        # load random data
        self.random_data = random_data
        self.clf = SVC(kernel='linear', probability=True, C=1)
        if cluster is None:
            self.data_cluster = MeanShiftCluster()
        else:
            assert issubclass(cluster, ClusterBase)
            self.data_cluster = cluster

    def decision_function(self, samples):
        pass

    def get_proba(self):
        # probability that it is the class (uncertain samples not counted)
        prob = 0
        prob += np.sum(self.probability[:,1][self.prediction == 1])
        prob += np.sum(1-self.probability[:,1][self.prediction == -1])
        prob /= len(self.probability[:,1][self.prediction != 0])
        return prob

    def mean_dist(self, samples, metric='cosine'):
        return self.data_cluster.mean_dist(samples, metric)

    def class_mean_dist(self, samples, metric='cosine'):
        return self.data_cluster.class_mean_dist(samples, metric)

    def predict(self, samples):
        proba = self.clf.predict_proba(samples)
        self.probability = proba
        mask_1 = np.sum(proba < self.uncertainty_thresh, axis=1) == 2
        pred = np.array([-1 if r[0] > 0.5 else 1 for r in proba])
        pred[mask_1] = 0
        self.prediction = pred
        return pred

    def __fit_vs_random(self, class_data):
        label_class = np.repeat(1, np.shape(class_data)[0])
        label_unknown = np.repeat(-1, np.shape(self.random_data)[0])
        training_embeddings = np.concatenate((class_data, self.random_data))
        training_labels = np.concatenate((label_class, label_unknown))
        self.clf.fit(training_embeddings, training_labels)

    def partial_fit(self, samples):
        self.data_cluster.update(samples)
        reduced_data = self.data_cluster.get_data()
        # refit SVM one vs random
        self.__fit_vs_random(reduced_data)

