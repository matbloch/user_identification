# 27.02 - 3.03

## Work Done


## Notes/Remarks

**Possible Improvements**
- Test Update Batch with Local Outlier Factor (new SKLearn implementation) or Clustering for inconsistency
- Inter-Class metric learning, when clusters become too similar (Fowlkes Mallows Index, Calinski and Harabaz score)
- Template Face Landmark coordinates for Kinect SDK Bounding Box (Kinect face detection is however jittery)
- Idea: Track face representation on manifold for each frame (or 10th frame). If a frame is too different from last one: tracking has switched
	- possible if: representation generation and networking fast enough
	- implementation: possibly on a very reduced subspace (e.g. 2-3 features, that are decorrelated)


**Todo**
- Tune ABOD Threshold: Regression against LFW dataset or two very similar faces (large amount of pictures needed)
- Evaluate feature activation in case two features are very similar (how can we differentiate best between these faces)
	- idea: outlier detection in subspace, if very similar
	- feature selection: (incremental-)PCA, Chi-Squared Test etc. [SKLearn tutorial](http://machinelearningmastery.com/feature-selection-machine-learning-python/)
	- Confusion matrix: evaluate in original 128dim space and reduced class space (approx 30dims)
- Keep connection alive during whole application (increase message timeout and implement disconnection message)
- Avoid too much drift in mean-shift data cluster
- For larger set size: Consider only those clusters, where the mean is in near range of the input set
- Evaluate feature variance component wise
	
	
## Challenges/Problems

## Literature/Personal Notes

- Comparison of Manifold Learning Methods (SKlearn): [SKlearn](http://scikit-learn.org/dev/auto_examples/manifold/plot_compare_methods.html#sphx-glr-auto-examples-manifold-plot-compare-methods-py)

- [Model evaluation metrics in SKLearn](http://scikit-learn.org/stable/modules/model_evaluation.html)
- [Face Recognition with eigenfaces in SKLearn](http://scikit-learn.org/stable/auto_examples/applications/face_recognition.html#sphx-glr-auto-examples-applications-face-recognition-py)