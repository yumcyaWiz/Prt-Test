import numpy as np
import matplotlib.pyplot as plt


data = np.loadtxt("skyCoeffs.csv", delimiter=", ")
fig = plt.figure()
plt.imshow(data)
fig.savefig("skyCoeffs.png")
