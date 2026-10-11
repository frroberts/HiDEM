# Output

In the config file the output parameters are controlled from the output section in the config file. The block looks as follows:

```toml
[output]
# path to work from
outWorkingPath = "../Data/"
# The program will add the appropriate prefix to this name, wont be written if left empty or not present
outParticleFile = "pOut"
# the type of output we use, CSV or HDF5, VTK (buggy), VKTHDF
outParticleType = "VTKHDF"
# The program will add the appropriate prefix to this name, wont be written if left empty or not present
outLinkFile = "lOut"
# the type of output we use, CSV or HDF5
outLinkType = "HDF5"
# path for where to output the resulting mesh, will inherit working path from output
# ignored if no collision mesh is provided
outMeshFile = "mOut"
# the type of output we use, mehses only supported with VTKHDF
outMeshType = "VTKHDF"
```

If either `outXXFile` or `outXXType` are not defiend no output will be written, even if the code calls any of the output functions defined below. Additionally if `outXXFile` is an empty string no output will be written. `outWorkingPath` is the base path that all output will be dumped into.

### Supported output types 

| Output type | outXXType | Supported by | Description  | Note  | 
|---|---|---|---|---|
| HDF5 | HDF5  | Particles, Links | Output data in HDF5 format, HiDEM specific format, combines data from all ranks into one file | Fast production ready | 
| VtkHdf  | VTKHDF  | Particles, Mesh |Output data in VKTHDF, VTK specific format, combines data from all ranks into one file | Fast production ready, needs fairly new paraview to open, currently everything is an unstructured mesh | 
| Vtk  | VTK  | Particles | Output data in native VTK format, binary output, combines data from all ranks into one file | Buggy, use VtkHdf instead, to be removed |
| Csv  | CSV  |  Particles, Links | Output Ascii data into CSV files, one file per rank | Slow, creates large files, don't use for production runs |

# Output functions

The oputut functions are all member functions of of `HiDEMLib`. Calling these will output particles, links, or mesh data, to do this at only the output interval they should wrap them in an if statement that checks if the current timestep is the output interval.

```c++
if (ts % outputInterval == 0) {
    hiDem.snapshotParticles(std::make_tuple("iteratedNeighbours", "iteratedLinks"), packVecPtrs(pData.iteratedNeighbours, pData.iteratedLinks));
    hiDem.snapshotLinks(std::make_tuple("state", "keyEndA", "keyEndB", "globalKey"), packVecPtrs(lData.state, lData.globalKeyEndA, lData.globalKeyEndA, lData.globalKeys));
    hiDem.snapshotMesh(std::make_tuple("xFaceForce", "yFaceForce", "zFaceForce"), packVecPtrs(mesh->dataPtr->xFaceForce, mesh->dataPtr->yFaceForce, mesh->dataPtr->zFaceForce));    
}
```

Below is a more detailed list of the output functions and how to use them.

## Statistics

The code automatically updates the `iteratedNeighbours` and `iteratedLinks` arrays in `HiDEMParticleData` everytime runtime statistics are printed.
However to ensure the statistics are up to date when output is written `HiDEMLib` has the following function that can be used to update these arrays.

### Active links

To get the active links use:

```c++
template<class T>
void HiDEMLib::computeActiveLinks(ManVec<T> &out)
```

Which will output the active links per particle into `out`, and `out` should be a field in `ParticleData`. `HiDEMParticleData` already has a field called `iteratedLinks` that can be used for this

### Evaluated neighbours

To get the iterated neighbor particles use:

```c++
template<class T>
void HiDEMLib::computeIteratedNeighbours(ManVec<T> &out)
```

Again `out` should be a member of `ParticleData`, and `HiDEMParticleData` already has a field called `iteratedNeighbours` that can be used here.

## Particles

Outputting the particles in the code can be done with either of these functions

```c++
// outputs owned particles from each rank 
template<typename ...Names, typename ...Fields>
void HiDEMLib::snapshotParticles(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, std::string prefix = "")

// outputs particles whos local index is in the indexes array
template<typename... Names, typename... Fields>
void HiDEMLib::snapshotParticlesIndex(std::tuple<Names...> &fieldNames, std::tuple<Fields...> &outFields, ManVec<pInd_t> &indexes, std::string prefix = "")
                                
// filtered output based on the filter function
template<typename ...Names, typename ...Fields, typename FilterFun>
void HiDEMLib::snapshotParticlesFilter(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, FilterFun filter, std::string prefix = "")
```

The first one will output all the owned particles, this is probably the main output function to use. The second one takes an array of indexes to output, if you have a list of specific particles to output this can be used to output only those. The last one takes a filter function and outputs the praticles filtered by that function, details are covered below.

The parameters to the functions are all the same, apart from the indexes and filter function. Essentially the function takes a tuple of names for the output fileds and and a tuple of what fields to actually output. The names tuple can easily be created by just calling `std::make_tuple` from the standard library. For the fields there is a helper function called `packVecPtrs` where you just pass in the fields form pData and it will create the appropriate tuple. The field names does not need to match those used in ParticleData.

Particle coordinates are always outputted and named xCords, yCords, zCords, so you do not need to include them. So to output how many links and neighbours each particle has you can use the following:

```c++
hiDem.snapshotParticles(ts,
                        std::make_tuple("iteratedNeighbours", "iteratedLinks"),
                        packVecPtrs(pData.iteratedNeighbours, pData.iteratedLinks));
```

## Links

Outputting the links works in the same way as the particles, there are three functions, one for the owned, one for arbitrary indexes and one for a filtered output.

```c++
template<typename ...Names, typename ...Fields>
void HiDEMLib::snapshotLinks(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, std::string prefix = "")
    
template<typename... Names, typename... Fields>
void HiDEMLib::snapshotLinksIndex(const std::tuple<Names...> &fieldNames, const std::tuple<Fields...> &outFields, ManVec<pInd_t> &indexes, std::string prefix = "")

template<typename ...Names, typename ...Fields,  typename FilterFun>
void HiDEMLib::snapshotLinksFilter(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, FilterFun filter, std::string prefix = "")
```

The parameters are the same as for the particles, field names as a tuple, the fields as a tuple built with `packVecPtrs`. An example looks as follows:

```c++
hiDem.snapshotLinks(
                    std::make_tuple("state", "keyEndA", "keyEndB", "globalKey"),
                    packVecPtrs(lData.state, lData.globalKeyEndA, lData.globalKeyEndA, lData.globalKeys));
```

## Mesh

For outputting the mesh data the interface is the same, but there is no filtered or owned output functions. In practice the mesh output will combine the mesh data from all ranks, and then a single rank will actually output it. Any filtering needs to be done afterwards.

```c++
void snapshotMesh(std::tuple<Names...> fieldNames, std::tuple<Fields...> outFields, std::string prefix = "")
```

An example that outputs the face force values for the mesh looks as follows:

```c++
hiDem.snapshotMesh(
                   std::make_tuple("xFaceForce", "yFaceForce", "zFaceForce"),
                   packVecPtrs(mesh->dataPtr->xFaceForce, mesh->dataPtr->yFaceForce, mesh->dataPtr->zFaceForce));
```

## Filtered output

Filtered outputs allows you to output subsets of the data based on the filter function, particularly useful if you are interested in particles in a certain area or similar. The filter function can take a different set of fields than the normal output function.

The filter function will be called for owned particles/links in a rank. The filter function needs to accept a particle/link id, and a reference to the appropriate particle/linkData class, and return true or false based on if the index should be included or not.

As an example the following function would filter out every other particle based on their global Id, but you can substitute the check with whatever you want and you can read any data you need from the particle/linkData class.

```c++
hiDem.snapshotParticlesFilter(std::make_tuple("iteratedNeighbours", "iteratedLinks"), packVecPtrs(pData.iteratedNeighbours, pData.iteratedLinks),
    [](pInd_t ind, HiDEMParticleData &pData) {
        if (pData.globalKeys[ind] % 2 == 0)
            return true;
        return false;
    }, "odd");
```