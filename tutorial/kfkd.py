# file kfkd.py
import os

import numpy as np
from pandas.io.parsers import read_csv
from sklearn.utils import shuffle


FTRAIN = '../data/training.csv'
FTEST = '../data/test.csv'


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


## Step 2
from lasagne import layers
from lasagne.updates import nesterov_momentum
from nolearn.lasagne import NeuralNet

net1 = NeuralNet(
	layers=[  # three layers: one hidden layer
		('input', layers.InputLayer),
		('hidden', layers.DenseLayer),
		('output', layers.DenseLayer),
		],
	# layer parameters:
	input_shape=(None, 9216),  # 96x96 input pixels per batch
	hidden_num_units=100,  # number of units in hidden layer
	output_nonlinearity=None,  # output layer uses identity function
	output_num_units=30,  # 30 target values

	# optimization method:
	update=nesterov_momentum,
	update_learning_rate=0.01,
	update_momentum=0.9,

	regression=True,  # flag to indicate we're dealing with regression problem
	max_epochs=400,  # we want to train this many epochs
	verbose=1,
	)


#X, y = load()

#print("X.shape == {}; X.min == {:.3f}; X.max == {:.3f}".format(
#	 X.shape, X.min(), X.max()))
#print("y.shape == {}; y.min == {:.3f}; y.max == {:.3f}".format(
#	 y.shape, y.min(), y.max()))
print "Data loaded"


#net1.fit(X, y)



#train_loss = np.array([i["train_loss"] for i in net1.train_history_])
#valid_loss = np.array([i["valid_loss"] for i in net1.train_history_])
from matplotlib import pyplot

def plot_fit():
	pyplot.plot(train_loss, linewidth=3, label="train")
	pyplot.plot(valid_loss, linewidth=3, label="valid")
	pyplot.grid()
	pyplot.legend()
	pyplot.xlabel("epoch")
	pyplot.ylabel("loss")
	pyplot.ylim(1e-3, 1e-2)
	pyplot.yscale("log")
	pyplot.show()

def plot_sample(x, y, axis):
	img = x.reshape(96, 96)
	axis.imshow(img, cmap='gray')
	axis.scatter(y[0::2] * 48 + 48, y[1::2] * 48 + 48, marker='x', s=10)

def plot_ex():
	X, _ = load(test=True)
	y_pred = net1.predict(X)

	fig = pyplot.figure(figsize=(6, 6))
	fig.subplots_adjust(
		left=0, right=1, bottom=0, top=1, hspace=0.05, wspace=0.05)

	for i in range(16):
		ax = fig.add_subplot(4, 4, i + 1, xticks=[], yticks=[])
		plot_sample(X[i], y_pred[i], ax)

#plot_ex()
#pyplot.show()



def load2d(test=False, cols=None):
    X, y = load(test=test)
    X = X.reshape(-1, 1, 96, 96)
    return X, y


net2 = NeuralNet(
    layers=[
        ('input', layers.InputLayer),
        ('conv1', layers.Conv2DLayer),
        ('pool1', layers.MaxPool2DLayer),
        ('conv2', layers.Conv2DLayer),
        ('pool2', layers.MaxPool2DLayer),
        ('conv3', layers.Conv2DLayer),
        ('pool3', layers.MaxPool2DLayer),
        ('hidden4', layers.DenseLayer),
        ('hidden5', layers.DenseLayer),
        ('output', layers.DenseLayer),
        ],
    input_shape=(None, 1, 96, 96),
    conv1_num_filters=32, conv1_filter_size=(3, 3), pool1_pool_size=(2, 2),
    conv2_num_filters=64, conv2_filter_size=(2, 2), pool2_pool_size=(2, 2),
    conv3_num_filters=128, conv3_filter_size=(2, 2), pool3_pool_size=(2, 2),
    hidden4_num_units=500, hidden5_num_units=500,
    output_num_units=30, output_nonlinearity=None,

    update_learning_rate=0.01,
    update_momentum=0.9,

    regression=True,
    max_epochs=1000,
    verbose=1,
    )

X, y = load2d()  # load 2-d data
net2.fit(X, y)

# Training for 1000 epochs will take a while.  We'll pickle the
# trained model so that we can load it back later:
import cPickle as pickle
with open('net2.pickle', 'wb') as f:
    pickle.dump(net2, f, -1)
