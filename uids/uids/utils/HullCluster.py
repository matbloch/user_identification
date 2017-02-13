import numpy as np
import matplotlib.pyplot as plt
import time
from sklearn.metrics.pairwise import pairwise_distances
from uids.utils.DataAnalysis import *
from scipy.spatial import Delaunay
from uids.utils.Logger import Logger as log
from uids.utils.KNFilter import KNFilter


class HullCluster:

    # ========= parameters
    # dimension reduction
    max_size = 100
    dim_reduction = 6   # 0: automatic reduction
    dim_removal = 5     # dimension reduction when we exceed the maximum cluster size

    # ========= options
    __remove_near_pts = False
    __verbose = False

    # ========= internal representation
    data = []

    # ========= logging
    __log = True
    log_intra_deleted = []
    log_cl_size_orig = []
    log_cl_size_reduced = []
    log_min_cl_sep = []
    log_dist_delete = []
    log_expl_var = []

    def __init__(self):
        pass

    def update(self, samples):

        # init - add all samples if no data yet
        if len(self.data) == 0:
            self.data = samples

        # =======================================
        # 1.  Reduce data/sample data

        if self.dim_reduction > 0:
            basis, mean, var = ExtractMaxVarComponents(self.data, self.dim_reduction)
            self.log_expl_var.append(var)
        else:
            basis, mean = ExtractSubspace(self.data, 0.8)

        cluster_reduced = ProjectOntoSubspace(self.data, mean, basis)
        samples_reduced = ProjectOntoSubspace(samples, mean, basis)
        dims = np.shape(cluster_reduced)
        # select minimum data to build convex hull
        # min_nr_elems = dims[1] + 4
        if self.__verbose:
            print "Reducing dimension: {}->{}".format(np.shape(self.data)[1], dims[1])

        # =======================================
        # 2.  Calculate Convex Hull in subspace

        data_hull = cluster_reduced     # take all samples of data
        hull = Delaunay(data_hull)
        if self.__verbose:
            print "Calculating data hull using {}/{} points".format(len(data_hull), len(self.data))

        # =======================================
        # 3.  Select new samples from outside convex hull

        outside_mask = np.array([False if hull.find_simplex(sample) >= 0 else True for sample in samples_reduced])
        if self.__verbose:
            # Todo: use outside mask counting
            nr_elems_outside_hull = np.sum([0 if hull.find_simplex(sample) >= 0 else 1 for sample in samples_reduced])
            print "Elements OUTSIDE hull (to include): {}/{}".format(nr_elems_outside_hull, len(samples))

        # add samples (samples need to be np.array)
        self.data = np.concatenate((self.data, samples[outside_mask]))

        # =======================================
        # 4.  Recalculate hull with newly added points

        # If memory exceeded: Perform unrefinement process -
        # discharge sampling directions with lowest variance contribution
        if self.dim_reduction > 0:
            nr_comps = self.dim_reduction if len(self.data) <= self.max_size else self.dim_removal
            if len(self.data) > 150:
                nr_comps = self.dim_removal - 1
            basis, mean, var = ExtractMaxVarComponents(self.data, nr_comps)
        else:
            # automatic dimension selection (based on containing certain variance)
            basis, mean = ExtractSubspace(self.data, 0.75)

        cluster_reduced = ProjectOntoSubspace(self.data, mean, basis)
        print "Recuding dimension: {}->{}".format(np.shape(self.data)[1], np.shape(cluster_reduced)[1])
        hull = Delaunay(cluster_reduced)

        # =======================================
        # 5.  Discharge samples inside hull

        # select samples inside hull
        cl_to_delete = np.array(list(set(range(0, len(cluster_reduced))) - set(np.unique(hull.convex_hull))))
        # set(range(len(data_hull))).difference(hull.convex_hull)

        # print "Points building convex hull: {}".format(set(np.unique(hull.convex_hull)))
        # print "To delete: {}".format(cl_to_delete)

        if len(cl_to_delete[cl_to_delete < 0]) > 0:
            print set(np.unique(hull.convex_hull))
            log.warning("Index elements smaller than 0: {}".format(cl_to_delete[cl_to_delete < 0]))

        if self.__log:
            self.log_intra_deleted.append(len(cl_to_delete))
            self.log_cl_size_orig.append(len(self.data))

        if self.__verbose:
            log.severe("4. Cleaning up samples from inside hull")

        print "Cleaning {} points from inside data".format(len(cl_to_delete))

        # Remove points from inside hull
        self.data = np.delete(self.data, cl_to_delete, axis=0)

        # =======================================
        # 6.  delete very similar points

        filter = KNFilter(self.data, k=3, threshold=0.25)
        max_removal = 10 if len(self.data) > 30 else 0
        tmp = filter.filter_x_samples(max_removal)
        print "--- Removing {} knn points".format(len(self.data)-len(tmp))
        self.data = tmp

        if self.__log:
            self.log_cl_size_reduced.append(len(self.data))

        print "Cluster size: {}".format(len(self.data))

    def plot_log(self):
        if not self.__log:
            print "Log is disabled."
            return

        plt.figure()
        x = range(0, len(self.log_cl_size_orig))
        plt.title("Cluster Update")
        plt.plot(x, self.log_cl_size_orig, label="Cluster size (samples added) before Cleanup")
        plt.plot(x, self.log_cl_size_reduced, label="Cluster Size after total Cleanup")
        plt.plot(x, self.log_intra_deleted, label="Inside Hull Cleanup")
        if len(self.log_dist_delete) > 0:
            plt.plot(x, self.log_dist_delete, label="Separation Cleanup")
        plt.xlabel("Iteration")
        plt.ylabel("Size [-]")
        plt.legend()

        if self.dim_reduction > 0:
            plt.figure()
            plt.title("Explained Variance")
            plt.plot(x, self.log_expl_var, label="Variance (sample inclusion)")
            plt.xlabel("Iteration")
            plt.ylabel("Explained Variance [%]")

        if len(self.log_min_cl_sep) > 0:
            plt.figure()
            plt.title("Minimum intra-data separation")
            plt.plot(x, self.log_min_cl_sep, label="Minimum separation")
            plt.xlabel("Iteration")
            plt.ylabel("Distance [euclidean]")

        plt.show()
