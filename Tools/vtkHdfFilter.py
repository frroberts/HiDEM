import h5py
import numpy as np

# helper function to fill the empty parts of the poly file structure
def createEmptyPolyGroup(base, name):
    grp = base.create_group(name)
    grp["Connectivity"] = np.arange(0, 1, 1, dtype=np.int32)
    grp["NumberOfCells"] = np.arange(0, 1, 1, dtype=np.int32)
    grp["NumberOfConnectivityIds"] = np.arange(0, 1, 1, dtype=np.int32)
    grp["Offsets"] = np.arange(0, 1, 1, dtype=np.int32)

# write out coordinates and field data to vtkhdf
# cords is a [N][3] sized array of coordinates, fields is a dict of values for each point
def toVtkHdf(cords, fields, outFile):
    f = h5py.File(outFile, "w")

    base = f.create_group("VTKHDF")
    base.attrs['Version'] = [2,2]

    # this works dont touch
    base.attrs.create('Type', 'PolyData'.encode('ascii') ,dtype=h5py.string_dtype('ascii', 8))

    # set the number of points based on the shape of the cords
    shape = np.arange(0, 1, 1, dtype=np.int32)
    shape[0] = cords.shape[0]
    base["NumberOfPoints"] =  shape

    # we dont have any lines, strips, or polygons
    createEmptyPolyGroup(base, "Lines")
    createEmptyPolyGroup(base, "Polygons")
    createEmptyPolyGroup(base, "Strips")

    offsets = np.arange(0, cords.shape[0], 1, dtype=np.int32)
    vTexes = base.create_group("Vertices")
    vTexes["Connectivity"] = offsets
    vTexes["NumberOfCells"] = shape
    vTexes["NumberOfConnectivityIds"] = shape
    offsets = np.insert(offsets, 0, 0)
    vTexes["Offsets"] = offsets

    base["Points"] = cords
    pointData = base.create_group("PointData")
    for k in fields.keys():
        pointData[k] = fields[k]

    pass

filename = "../Data/pOut0.vtkhdf"

with h5py.File(filename, "r") as f:
    # Open vase group, in a vtkhdf file this is always named "VTKHDF"
    base = f["VTKHDF"]
    # in a vtkhdf file the fields for each point is stored as individual datasets in the "PointData" group
    # print the data labels for each point
    print("point data", base["PointData"].keys())

    # in a vtkhdf file the coordinates are stored in a points dataset
    # read coordinates, coordinates will be a 2d array [numpoints][cord], so [0][0] is x,[0][1] is y, [0][0] is z
    coordinates = np.array(base["Points"])

    # read in the fields for each point and put them in the fields dict
    fields = {}
    for k in base["PointData"].keys():
        fields[k] = (np.array(base["PointData"][k]))

    # boolean filters
    # starts off being default false
    filterArr = np.full(coordinates.shape[0], False)
    # --------------------------------------------------

    # iterate particle indexes, set true if you want it inclded
    for i in range(coordinates.shape[0]):
        # filter in every other particle
        if i%2 == 0:
            filterArr[i] = True
        # but filter out anything that has less than 11 neighbours
        if fields["iteratedNeighbours"][i] < 11:
            filterArr[i] = False


    # --------------------------------------------------

    # filter the fields and coordinates we read
    coordinates = coordinates[filterArr]
    for k in fields.keys():
        fields[k] = fields[k][filterArr]

    # write the filtered data
    toVtkHdf(coordinates, fields, "out.vtkhdf")

