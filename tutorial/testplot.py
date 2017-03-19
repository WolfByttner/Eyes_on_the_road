from loadkey import load2d
from matplotlib import pyplot
import cPickle as pickle




def plot_sample(x, y, axis):
	img = x.reshape(96, 96)
	axis.imshow(img, cmap='gray')
	axis.scatter(y[0::2] * 48 + 48, y[1::2] * 48 + 48, marker='x', s=10)

X, = load2d(test=True)[0][6:7]

with open('net.pickle', 'rb') as f:
	net = pickle.load(f)
y_pred = net.predict(sample[0])

fig = pyplot.figure(figsize=(6,3))
ax = fig.add_subplot(1, 2, 1, xticks=[], yticks=[])
plot_sample(sample[0], y_pred, ax)
pyplot.show()
