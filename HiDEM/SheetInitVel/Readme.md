# Ice sheet with initial velocity set

This sample illustrates how you can set initial values for particles (the same idea can be used for links).
In this case we use a 2d map to set the initial x velocity for the particles.

The key part is two steps first load the 2d map, the map specifications in the config file follows the same specifications as Scalar or map variables described in the input documentations. So currently 
```toml
variable = {type = "csv2d", path = "path to file", fieldID = 2}
```

This can then be read through the arg parser by calling 
```c++
template<class T>std::optional<Map2d<T>*> getMap2dPtr(std::string name)
```

This will return an optional pointer if the map is found, `T`  is the type of the map.

To then set the initial value for the particles we simply iterate over all of them before the timestepping part starts. in the sample this is done using:

```c++
pData.iterateOwnedParticles(OffloadExec(), [=]DEVCALLABLE(pInd_t pInd, HiDEMParticleData &pData){
    pData.xDiff[pInd] = velMap->interp(pData.xCord[pInd], pData.xCord[pInd])*velMapScale;
}, pData);
```

So simply iterating over all the particles, setting the xDiff based on the value we interpolate from the map, in this case when we get the map from the arg parser we grab out the actual pointer beforehand.
