from pyevtk.hl import pointsToVTK
import h5py
import numpy as np
import argparse
import os
import glob
import re

parser = argparse.ArgumentParser(description='Convert h5 output to vtu files')
parser.add_argument('--timeRange',dest='timeRange', help='filter based on range of timesteps [min:max]',required=False)
parser.add_argument('--timeSteps',dest='timeSteps', help='filter based on listed timesteps [int(,int...)]',required=False)
parser.add_argument('--path',dest='path', help='path to look at files in use * for timesteps. Data/pOut*.h5 ',required=True)
parser.add_argument('--output',dest='output', help='path to write ouputs to will use the same name as input, replacing h5 with vtu',required=True)
parser.add_argument('--fields',dest='fields', help='fields to get from the h5 file [string(,string...)]',required=False)

args = parser.parse_args()

if args.timeRange != None and args.timeSteps != None:
    print("Time range and time steps can't be set at the same time")
    exit(1)


# get filenames
fileNames = []
if args.timeRange != None:
    # if range is set get anything in range
    limits = args.timeRange.split(":")
    lower = int(limits[0])
    upper = int(limits[1])
    for name in glob.glob(args.path):
        # get the digit part of the filename
        m = re.search('(\d+)', name)
        if int(m.group(1)) >= lower and int(m.group(1)) <=upper:
            fileNames.append(name)
        pass
elif args.timeSteps != None:
    # get steps if explicit steps is set
    steps = args.timeSteps.split(",")
    for name in glob.glob(args.path):
        m = re.search('(\d+)', name)
        # get the digit part of the filename
        if m.group(1) in steps:
            fileNames.append(name)
    pass
else :
    # else take all files
    fileNames = glob.glob(args.path)

for name in fileNames:

    readFields = {}
    cordFields = {}
    cordNames = ["xCord","yCord","zCord"]
    
    with h5py.File(name, "r") as dSet:
        print(name)

        # get field names, if args is none get all of them, otherwise add xyz cords and what we filter for
        fieldNames = []
        if (args.fields != None):
            for f in args.fields.split(','):
                # filter out cordniate field names
                if f not in cordNames:
                    fieldNames.append(f)
        else:
            for f in dSet.keys():
                # filter out cordniate field names
                if f not in cordNames:
                    fieldNames.append(f)

        # read the fields we want
        for f in fieldNames:
            if(dSet[f][:].dtype == np.float64):
                readFields[f] = np.array(dSet[f][:], dtype='f')
            else:
                readFields[f] = np.array(dSet[f][:])
            pass
        # ead the cordinate fields
        for f in cordNames:
            cordFields[f] = np.array(dSet[f][:], dtype='f')
            pass
        # write out
        if(cordFields['xCord'].size == 0):
            continue
        pointsToVTK(args.output+os.path.basename(name).split(".")[0], cordFields['xCord'], cordFields['yCord'], cordFields['zCord'], data = readFields)
