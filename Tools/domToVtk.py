import os
from pyevtk.hl import polyLinesToVTK
import numpy as np

FILE_PATH = "poly_lines"
def clean():
    try:
        os.remove(FILE_PATH + ".vtu")
    except:
        pass


x = np.zeros(0)
y = np.zeros(0)
z = np.zeros(0)
owner = np.zeros(0)
pointsPerLine = np.zeros(0)


def newPoint(cordA, cordB, own):

    global x
    global y
    global z
    global owner
    global pointsPerLine

    npoints = 18
    xN = np.zeros(npoints)
    yN = np.zeros(npoints)
    zN = np.zeros(npoints)
    # Front face line
    xN[0], yN[0], zN[0] = cordA[0], cordA[1], cordA[2]
    xN[1], yN[1], zN[1] = cordA[0], cordA[1], cordB[2]
    xN[2], yN[2], zN[2] = cordA[0], cordB[1], cordB[2]
    xN[3], yN[3], zN[3] = cordA[0], cordB[1], cordA[2]
    xN[4], yN[4], zN[4] = cordA[0], cordA[1], cordA[2]

    # Rear face line
    xN[5], yN[5], zN[5] = cordB[0], cordA[1], cordA[2]
    xN[6], yN[6], zN[6] = cordB[0], cordA[1], cordB[2]
    xN[7], yN[7], zN[7] = cordB[0], cordB[1], cordB[2]
    xN[8], yN[8], zN[8] = cordB[0], cordB[1], cordA[2]
    xN[9], yN[9], zN[9] = cordB[0], cordA[1], cordA[2]

    # Connecting lines
    xN[10], yN[10], zN[10] = cordB[0], cordA[1], cordA[2]
    xN[11], yN[11], zN[11] = cordA[0], cordA[1], cordA[2]
    xN[12], yN[12], zN[12] = cordB[0], cordB[1], cordA[2]
    xN[13], yN[13], zN[13] = cordA[0], cordB[1], cordA[2]
    xN[14], yN[14], zN[14] = cordB[0], cordA[1], cordB[2]
    xN[15], yN[15], zN[15] = cordA[0], cordA[1], cordB[2]
    xN[16], yN[16], zN[16] = cordB[0], cordB[1], cordB[2]
    xN[17], yN[17], zN[17] = cordA[0], cordB[1], cordB[2]

    # Connectivity of the lines
    pointsPerLineN = np.zeros(6)
    pointsPerLineN[0] = 5
    pointsPerLineN[1] = 5
    pointsPerLineN[2] = 2
    pointsPerLineN[3] = 2
    pointsPerLineN[4] = 2
    pointsPerLineN[5] = 2

    # Some variables
    ownerN = np.zeros(6)
    ownerN.fill(own)

    x = np.append(x, xN)
    y = np.append(y, yN)
    z = np.append(z, zN)
    pointsPerLine = np.append(pointsPerLine, pointsPerLineN)
    owner = np.append(owner, ownerN)

def run():
    # Positions of points that define lines

    newPoint([396.85,476.22,359.228],[-10,-10,5.874],0)
    newPoint([429.177,508.547,391.555],[-42.3269,-42.3269,-26.4529],0)
    newPoint([396.85,1010.06,359.228],[-10,476.22,5.874],1)
    newPoint([429.177,1042.39,391.555],[-42.3269,443.893,-26.4529],1)
    newPoint([1010.06,301.606,359.228],[396.85,-10,5.874],2)
    newPoint([1042.39,333.933,391.555],[364.523,-42.3269,-26.4529],2)
    newPoint([1010.06,634.96,359.228],[396.85,301.606,5.874],4)
    newPoint([1042.39,667.287,391.555],[364.523,269.279,-26.4529],4)
    newPoint([1010.06,1010.06,359.228],[396.85,634.96,5.874],3)
    newPoint([1042.39,1042.39,391.555],[364.523,602.633,-26.4529],3)

    print(owner)


    polyLinesToVTK(FILE_PATH, x, y, z, pointsPerLine = pointsPerLine, cellData = {"owner" : owner})

if __name__ == "__main__":
    run()