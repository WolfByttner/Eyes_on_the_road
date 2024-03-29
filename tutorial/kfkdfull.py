"""
To use this script, first run this to fit your first model:
  python kfkd.py fit
Then train a bunch of specialists that intiliaze their weights from
your first model:
  python kfkd.py fit_specialists net.pickle
Plot their error curves:
  python kfkd.py plot_learning_curves net-specialists.pickle
And finally make predictions to submit to Kaggle:
  python kfkd.py predict net-specialists.pickle
"""

try:
	import cPickle as pickle
except ImportError:  # Python 3
	import pickle
from datetime import datetime
import os
import sys

from matplotlib import pyplot
import numpy as np
from lasagne import layers
from nolearn.lasagne import BatchIterator
from nolearn.lasagne import NeuralNet
from pandas import DataFrame
from pandas.io.parsers import read_csv
from sklearn.utils import shuffle
import theano

try:
	from lasagne.layers.cuda_convnet import Conv2DCCLayer as Conv2DLayer
	from lasagne.layers.cuda_convnet import MaxPool2DCCLayer as MaxPool2DLayer
except ImportError:
	Conv2DLayer = layers.Conv2DLayer
	MaxPool2DLayer = layers.MaxPool2DLayer


sys.setrecursionlimit(10000)  # for pickle...
np.random.seed(42)

FTRAIN = '../data/training.csv'
FTEST = '../data/test.csv'
FLOOKUP = '../data/IdLookup.csv'


def float32(k):
	return np.cast['float32'](k)


def load(test=False, cols=None):
	"""Loads data from FTEST if *test* is True, otherwise from FTRAIN.
	Pass a list of *cols* if you're only interested in a subset of the
	target columns.
	"""
	fname = FTEST if test else FTRAIN
	df = read_csv(os.path.expanduser(fname))  # load pandas dataframe

	# The Image column has pixel values separated by space; convert
	# the values to numpy arrays:
	df['Image'] = df['Image'].apply(lambda im: np.fromstring(im, sep=' '))

	if cols:  # get a subset of columns
		df = df[list(cols) + ['Image']]

	print(df.count())  # prints the number of values for each column
	df = df.dropna()  # drop all rows that have missing values in them

	X = np.vstack(df['Image'].values) / 255.  # scale pixel values to [0, 1]
	X = X.astype(np.float32)

	if not test:  # only FTRAIN has any target columns
		y = df[df.columns[:-1]].values
		y = (y - 48) / 48  # scale target coordinates to [-1, 1]
		X, y = shuffle(X, y, random_state=42)  # shuffle train data
		y = y.astype(np.float32)
	else:
		y = None

	return X, y


def load2d(test=False, cols=None):
	X, y = load(test=test, cols=cols)
	X = X.reshape(-1, 1, 96, 96)
	return X, y


def plot_sample(x, y, axis):
	img = x.reshape(96, 96)
	axis.imshow(img, cmap='gray')
	if y is not None:
		axis.scatter(y[0::2] * 48 + 48, y[1::2] * 48 + 48, marker='x', s=10)


def plot_weights(weights):
	fig = pyplot.figure(figsize=(6, 6))
	fig.subplots_adjust(
		left=0, right=1, bottom=0, top=1, hspace=0.05, wspace=0.05)

	for i in range(16):
		ax = fig.add_subplot(4, 4, i + 1, xticks=[], yticks=[])
		ax.imshow(weights[:, i].reshape(96, 96), cmap='gray')
	pyplot.show()


class FlipBatchIterator(BatchIterator):
	flip_indices = [
		(0, 2), (1, 3),
		(4, 8), (5, 9), (6, 10), (7, 11),
		(12, 16), (13, 17), (14, 18), (15, 19),
		(22, 24), (23, 25),
		]

	def transform(self, Xb, yb):
		Xb, yb = super(FlipBatchIterator, self).transform(Xb, yb)

		# Flip half of the images in this batch at random:
		bs = Xb.shape[0]
		indices = np.random.choice(bs, bs / 2, replace=False)
		Xb[indices] = Xb[indices, :, :, ::-1]

		if yb is not None:
			# Horizontal flip of all x coordinates:
			yb[indices, ::2] = yb[indices, ::2] * -1

			# Swap places, e.g. left_eye_center_x -> right_eye_center_x
			for a, b in self.flip_indices:
				yb[indices, a], yb[indices, b] = (
					yb[indices, b], yb[indices, a])

		return Xb, yb


class AdjustVariable(object):
	def __init__(self, name, start=0.03, stop=0.001):
		self.name = name
		self.start, self.stop = start, stop
		self.ls = None

	def __call__(self, nn, train_history):
		if self.ls is None:
			self.ls = np.linspace(self.start, self.stop, nn.max_epochs)

		epoch = train_history[-1]['epoch']
		new_value = np.cast['float32'](self.ls[epoch - 1])
		getattr(nn, self.name).set_value(new_value)


class EarlyStopping(object):
	def __init__(self, patience=100):
		self.patience = patience
		self.best_valid = np.inf
		self.best_valid_epoch = 0
		self.best_weights = None

	def __call__(self, nn, train_history):
		current_valid = train_history[-1]['valid_loss']
		current_epoch = train_history[-1]['epoch']
		if current_valid < self.best_valid:
			self.best_valid = current_valid
			self.best_valid_epoch = current_epoch
			self.best_weights = nn.get_all_params_values()
		elif self.best_valid_epoch + self.patience < current_epoch:
			print("Early stopping.")
			print("Best valid loss was {:.6f} at epoch {}.".format(
				self.best_valid, self.best_valid_epoch))
			nn.load_params_from(self.best_weights)
			raise StopIteration()


net = NeuralNet(
	layers=[
		('input', layers.InputLayer),
		('conv1', Conv2DLayer),
		('pool1', MaxPool2DLayer),
		('dropout1', layers.DropoutLayer),
		('conv2', Conv2DLayer),
		('pool2', MaxPool2DLayer),
		('dropout2', layers.DropoutLayer),
		('conv3', Conv2DLayer),
		('pool3', MaxPool2DLayer),
		('dropout3', layers.DropoutLayer),
		('hidden4', layers.DenseLayer),
		('dropout4', layers.DropoutLayer),
		('hidden5', layers.DenseLayer),
		('output', layers.DenseLayer),
		],
	input_shape=(None, 1, 96, 96),
	conv1_num_filters=32, conv1_filter_size=(3, 3), pool1_pool_size=(2, 2),
	dropout1_p=0.1,
	conv2_num_filters=64, conv2_filter_size=(2, 2), pool2_pool_size=(2, 2),
	dropout2_p=0.2,
	conv3_num_filters=128, conv3_filter_size=(2, 2), pool3_pool_size=(2, 2),
	dropout3_p=0.3,
	hidden4_num_units=1000,
	dropout4_p=0.5,
	hidden5_num_units=1000,
	output_num_units=30, output_nonlinearity=None,

	update_learning_rate=theano.shared(float32(0.03)),
	update_momentum=theano.shared(float32(0.9)),

	regression=True,
	batch_iterator_train=FlipBatchIterator(batch_size=128),
	on_epoch_finished=[
		AdjustVariable('update_learning_rate', start=0.03, stop=0.0001),
		AdjustVariable('update_momentum', start=0.9, stop=0.999),
		EarlyStopping(patience=200),
		],
	max_epochs=3000,
	verbose=1,
	)


def fit(fname_pretrain = None):
	X, y = load2d()
	num = 0
	if fname_pretrain:
		print "Loading from", fname_pretrain
		with open(fname_pretrain, 'rb') as f:
			net_pretrain = pickle.load(f)
	else:
		net_pretrain = None
		print "Starting new file"
	if (net_pretrain):
		net.load_params_from(net_pretrain)

	try:
		for i in range(60):
			net.fit(X, y, 50)
			with open('net.pickle', 'wb') as f:
				pickle.dump(net, f, -1)
			num += 50
	except StopIteration:
		print "Training done for", num, "layers"


from collections import OrderedDict

from sklearn.base import clone


SPECIALIST_SETTINGS = [
	dict(
		columns=(
			'left_eye_center_x', 'left_eye_center_y',
			'right_eye_center_x', 'right_eye_center_y',
			),
		flip_indices=((0, 2), (1, 3)),
		),

	dict(
		columns=(
			'nose_tip_x', 'nose_tip_y',
			),
		flip_indices=(),
		),

	dict(
		columns=(
			'mouth_left_corner_x', 'mouth_left_corner_y',
			'mouth_right_corner_x', 'mouth_right_corner_y',
			'mouth_center_top_lip_x', 'mouth_center_top_lip_y',
			),
		flip_indices=((0, 2), (1, 3)),
		),

	dict(
		columns=(
			'mouth_center_bottom_lip_x',
			'mouth_center_bottom_lip_y',
			),
		flip_indices=(),
		),

	dict(
		columns=(
			'left_eye_inner_corner_x', 'left_eye_inner_corner_y',
			'right_eye_inner_corner_x', 'right_eye_inner_corner_y',
			'left_eye_outer_corner_x', 'left_eye_outer_corner_y',
			'right_eye_outer_corner_x', 'right_eye_outer_corner_y',
			),
		flip_indices=((0, 2), (1, 3), (4, 6), (5, 7)),
		),

	dict(
		columns=(
			'left_eyebrow_inner_end_x', 'left_eyebrow_inner_end_y',
			'right_eyebrow_inner_end_x', 'right_eyebrow_inner_end_y',
			'left_eyebrow_outer_end_x', 'left_eyebrow_outer_end_y',
			'right_eyebrow_outer_end_x', 'right_eyebrow_outer_end_y',
			),
		flip_indices=((0, 2), (1, 3), (4, 6), (5, 7)),
		),
	]


def fit_specialists(fname_pretrain=None):
	if fname_pretrain:
		with open(fname_pretrain, 'rb') as f:
			net_pretrain = pickle.load(f)
	else:
		net_pretrain = None

	specialists = OrderedDict()

	for setting in SPECIALIST_SETTINGS:
		cols = setting['columns']
		X, y = load2d(cols=cols)

		model = clone(net)
		model.output_num_units = y.shape[1]
		model.batch_iterator_train.flip_indices = setting['flip_indices']
		model.max_epochs = int(4e6 / y.shape[0])
		if 'kwargs' in setting:
			# an option 'kwargs' in the settings list may be used to
			# set any other parameter of the net:
			vars(model).update(setting['kwargs'])

		if net_pretrain is not None:
			# if a pretrain model was given, use it to initialize the
			# weights of our new specialist model:
			model.load_params_from(net_pretrain)

		print("Training model for columns {} for {} epochs".format(
			cols, model.max_epochs))
		model.fit(X, y)
		specialists[cols] = model

	with open('net-specialists.pickle', 'wb') as f:
		# this time we're persisting a dictionary with all models:
		pickle.dump(specialists, f, -1)


def predict(fname_specialists='net-specialists.pickle'):
	with open(fname_specialists, 'rb') as f:
		specialists = pickle.load(f)

	X = load2d(test=True)[0]
	y_pred = np.empty((X.shape[0], 0))

	for model in specialists.values():
		y_pred1 = model.predict(X)
		y_pred = np.hstack([y_pred, y_pred1])

	columns = ()
	for cols in specialists.keys():
		columns += cols

	y_pred2 = y_pred * 48 + 48
	y_pred2 = y_pred2.clip(0, 96)
	df = DataFrame(y_pred2, columns=columns)

	lookup_table = read_csv(os.path.expanduser(FLOOKUP))
	values = []

	for index, row in lookup_table.iterrows():
		values.append((
			row['RowId'],
			df.ix[row.ImageId - 1][row.FeatureName],
			))

	now_str = datetime.now().isoformat().replace(':', '-')
	submission = DataFrame(values, columns=('RowId', 'Location'))
	filename = 'submission-{}.csv'.format(now_str)
	submission.to_csv(filename, index=False)
	print("Wrote {}".format(filename))


def rebin( a, newshape ):
	from numpy import mgrid
	assert len(a.shape) == len(newshape)

	slices = [ slice(0,old, float(old)/new) for old,new in zip(a.shape,newshape) ]
	coordinates = mgrid[slices]
	indices = coordinates.astype('i')	#choose the biggest smaller integer index
	return a[tuple(indices)]


def plot_sample(x, y, axis):
	img = x.reshape(96, 96)
	axis.imshow(img, cmap='gray')
	axis.scatter(y[0::2] * 48 + 48, y[1::2] * 48 + 48, marker='x', s=10)




def plot_face(fname_net='net.pickle'):

	sample = load2d(test=True)[0][8:9]

	print sample.shape
	with open(fname_net, 'rb') as f:
		net = pickle.load(f)
	y_pred = net.predict(sample)[0]

	fig = pyplot.figure(figsize=(6,3))
	ax = fig.add_subplot(1, 2, 2, xticks=[], yticks=[])
	plot_sample(sample[0], y_pred, ax)
	pyplot.show()


from math import floor
def sliding_window(frame, net, width, height, step):
	ret = []
	print  int(floor( frame.shape[0] / width)) 
	for i in range(0, frame.shape[0]-width, step):
		for j in range(0, frame.shape[1]-height, step):
			window = frame[i:i+width,j:j+height]
			tmp = np.zeros((width, height), dtype=np.float32)
			#print tmp
			#print "Loading sample"
			#sample = load2d(test=True)[0][8:9]
			#print sample
			#print tmp	
			print i, j, frame.shape
			for row in range(width):
				for col in range(height):
					#print window[row,col]
					tmp[row,col] = sum(window[row,col]) / (3*256);
			#print "fixing tmp"
			#print tmp
			tmp = tmp.reshape(-1, 1, width, height)
			#print (type(frame[i:i+width,j:j+height]), frame.shape) 
			#print frame[i:i+width,j:j+height].shape 
			#print frame
			print "sliding", width, height, i, j
			ret.append (net.predict(tmp))
	return (ret);

from time import sleep
def plot_instance(frame, preds, width, height, step): 
	#print list(preds)[0].shape
	preds = list(preds)
	fig = pyplot.figure(figsize=(6,3))
	ax = fig.add_subplot(1, 2, 2, xticks=[], yticks=[])
	x, y = frame.shape[0]/2,frame.shape[1]/2
	window = frame[x:x+width,y:y+height]
	print window.shape
	tmp = np.zeros((width, height), dtype=np.float32)
	for row in range(width):
		for col in range(height):
			tmp[row,col] = sum(window[row,col])

	plot_sample(tmp, preds[int(len(preds)/2)][0], ax)
	pyplot.show()
	#plot_sample(sample[0], y_pred, ax)
	#for i in range(0, frame.shape[0]-width, step):
	#	for j in range(0, frame.shape[1]-height, step):
	#		window = frame[i:i+width,j:j+height]
	#		tmp = np.zeros((width, height), dtype=np.float32)
	#		for row in range(width):
	#			for col in range(height):
	#				tmp[row,col] = sum(window[row,col])
	#		#print tmp.shape, preds.shape
	#		print len(preds), i, j
	#		plot_sample(tmp, preds[int(len(preds)/2)][0], ax)
	#		#plot_sample(tmp, preds, ax)
	#		pyplot.show(block=False)
	#		#pyplot.draw()
	#		#print "looping"
	#		sleep(1)


from time import sleep
def plot_cam(fname_net='net.pickle'):
	import cv2
	with open(fname_net, 'rb') as f:
		net = pickle.load(f)
	cap = cv2.VideoCapture(1);
	while (True):
		ret, frame = cap.read()
		detections = sliding_window(frame, net, 96, 96, 48)
		plot_instance(frame, detections, 96, 96, 48)
		print "I ran"
		sleep(.1);


def detect_keypoint_in_face(net, image, face):
	detection = image[face[0]:face[0]+96, face[1]:face[1]+96]
	detection = detection / 255.
	detection = detection.reshape(-1, 1, 96, 96)
	detection = detection.astype(np.float32)
	return net.predict(detection)


def detection_distance(det1, det2, factor = 2):
	res = 0
	for i in range(4):
		res += (det1[i] - det2[i]) ** factor
	return (res)


def pick_favourite(detections, last, img):
	if len(detections) == 0:
		return (last)
	current = last
	minv = 12000000
	mid = [img.shape[0] - 48, img.shape[1] - 48,
			img.shape[0] + 47, img.shape[1] + 47]
	for detection in detections:
		tmpv = detection_distance(detection, last, 3) + detection_distance(detection, mid)
		#print tmpv
		if tmpv < minv:
			minv = tmpv
			current = detection
	return current


import threading

import Queue 

videoPollData = Queue.LifoQueue(maxsize=500)
class CameraPollThread(threading.Thread): 
	def __init__(self, cv2):
		self.stream = cv2.VideoCapture(1)
		self.grabbed, self.frame = self.stream.read()
		threading.Thread.__init__(self)
	def run(self):
		while True:
			self.grabbed, self.frame = self.stream.read()
			if (videoPollData.full()):
				pass
				with videoPollData.mutex:
					del videoPollData.queue[:]
					videoPollData.queue = []
			videoPollData.put(self.frame)


import time


global camera_confidence
camera_confidence = 0
#def keypoints_are_good
def  plot_video(fname_net='net.pickle', data_queue=None):
	with open(fname_net, 'rb') as f:
		net = pickle.load(f)
	import cv2
	#cv2.CAP_PROP_BUFFERSIZE = 1
	cascPath = "haarcascade_frontalface_default.xml"
	faceCascade = cv2.CascadeClassifier(cascPath)
	#cap = cv2.VideoCapture(1);
	pollThread = CameraPollThread(cv2);
	pollThread.start()
	import matplotlib.pyplot as plt
	last = [400,350,400,350]
	last_detection = 0
	ltma = 0
	confidence = 0
	ticks = 0
	activations = 0
	while (True):
		if videoPollData.empty():
			time.sleep(0.02)
			continue;
		frame = videoPollData.get()
		gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

		faces = faceCascade.detectMultiScale(
			gray,
			scaleFactor=1.1,
			minNeighbors=5,
			minSize=(30, 30),
			flags = cv2.CASCADE_SCALE_IMAGE
		)
		implot = plt.imshow(gray, cmap='gray')
		tmp = pick_favourite(faces, last, gray)
		if (not (tmp is last)):
			if len(tmp) != 4:
				print "Error in tmp, was", tmp
				continue
			if gray.shape != (480, 640):
				print "Error in Gray, was", gray, gray.shape
				continue
			if tmp[0] < gray.shape[0] - 96 and tmp[1] < gray.shape[1] - 96:
				last = tmp
				tmp = detect_keypoint_in_face(net, gray, tmp)[0]
				plt.scatter(tmp[0::2] * 48 + 48 + last[0], tmp[1::2] * 48 + 48 + last[1], marker='x', s=10)

		last_detection *= .99
		sval =  sum(tmp[1::2])
		last_detection += sval / (100 * 15)
		ltma *= .9999
		ltma += sval / (100 * 15)
		#print "LTMA is ", abs(sval - ltma)
		#if (abs(sval - ltma) > 
		confidence = abs(sval - last_detection)
		confidence += abs(sval - ltma) - 0.3
		if confidence > 1 and confidence < 3:
			if (data_queue):
				data_queue[0] = 1
			activations += 1
			if activations == 1:
				ticks = 0
		elif ticks > 6:
			activations = 0
			ticks = 0
			#if (data_queue):
			#	data_queue[0] = 0
		ticks += 1
		if activations > 3:
			if (data_queue):
				data_queue[0] = 1
			#print "Activated at", activations, confidence
		else:
			if data_queue:
				data_queue[0] = 0
		#print "Confidence is", confidence
		#for face in faces:
		#	if face[0] < gray.shape[0] - 96 and face[1] < gray.shape[1] - 96:
		#		pass
		#		tmp = detect_keypoint_in_face(net, gray, face)[0]
		#		plt.scatter(tmp[0::2] * 48 + 48 + face[0], tmp[1::2] * 48 + 48 + face[1], marker='x', s=10)
		#cv2.imshow("Faces found", gray)
		#plt.scatter
		plt.pause(0.01)
		#plt.waitKey(25)
		#cv2.waitKey(25)
		plt.clf()

#	axis.scatter(y[0::2] * 48 + 48, y[1::2] * 48 + 48, marker='x', s=10)



def plot_learning_curves(fname_specialists='net-specialists.pickle'):
	with open(fname_specialists, 'rb') as f:
		models = pickle.load(f)

	fig = pyplot.figure(figsize=(10, 6))
	ax = fig.add_subplot(1, 1, 1)
	ax.set_color_cycle(
		['c', 'c', 'm', 'm', 'y', 'y', 'k', 'k', 'g', 'g', 'b', 'b'])

	valid_losses = []
	train_losses = []

	for model_number, (cg, model) in enumerate(models.items(), 1):
		valid_loss = np.array([i['valid_loss'] for i in model.train_history_])
		train_loss = np.array([i['train_loss'] for i in model.train_history_])
		valid_loss = np.sqrt(valid_loss) * 48
		train_loss = np.sqrt(train_loss) * 48

		valid_loss = rebin(valid_loss, (100,))
		train_loss = rebin(train_loss, (100,))

		valid_losses.append(valid_loss)
		train_losses.append(train_loss)
		ax.plot(valid_loss,
				label='{} ({})'.format(cg[0], len(cg)), linewidth=3)
		ax.plot(train_loss,
				linestyle='--', linewidth=3, alpha=0.6)
		ax.set_xticks([])

	weights = np.array([m.output_num_units for m in models.values()],
					   dtype=float)
	weights /= weights.sum()
	mean_valid_loss = (
		np.vstack(valid_losses) * weights.reshape(-1, 1)).sum(axis=0)
	ax.plot(mean_valid_loss, color='r', label='mean', linewidth=4, alpha=0.8)

	ax.legend()
	ax.set_ylim((1.0, 4.0))
	ax.grid()
	pyplot.ylabel("RMSE")
	pyplot.show()


if __name__ == '__main__':
	if len(sys.argv) < 2:
		print(__doc__)
	else:
		func = globals()[sys.argv[1]]
	func(*sys.argv[2:])
