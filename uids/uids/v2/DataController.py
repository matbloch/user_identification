import numpy as np
from uids.utils.Logger import Logger as log
from uids.data_models.StandardCluster import StandardCluster


class DataController:

    # raw CNN embeddings in clusters
    class_clusters = {}

    def __init__(self):
        pass

    # --------- CLASS HASHING

    def classes_in_range(self, samples, thresh=0.7, metric='cosine'):
        class_ids = []
        for id, c in self.class_clusters.iteritems():
            # only predict for "reasonable"/near classes
            range = np.mean(c.class_mean_dist(samples, metric))
            if range < thresh:
                class_ids.append(id)
            else:
                log.info('db', "Class {} out of range (0.7 [ref] < {:.3f})".format(id, range))

        return class_ids

    def get_class_means(self):
        return [c.mean() for id, c in self.class_clusters.iteritems()]

    # --------- DATA MANAGEMENT

    def add_samples(self, user_id, new_samples):
        """embeddings: array of embeddings"""
        if user_id not in self.class_clusters:
            # initialize
            self.class_clusters[user_id] = StandardCluster(max_size=60)
            self.class_clusters[user_id].update(new_samples)
        else:
            # update
            self.class_clusters[user_id].update(new_samples)

    def get_class_samples(self, class_id):
        if class_id in self.class_clusters:
            return self.class_clusters[class_id].get_data()
        else:
            return None

    def get_class_cluster(self, class_id):
        if class_id in self.class_clusters:
            return self.class_clusters[class_id]
        else:
            return None

    # --------- MANIFOLD LEARNING
    # TODO: implement

