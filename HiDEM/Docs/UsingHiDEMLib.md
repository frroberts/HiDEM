# Intro

This document tries to describe all you need to know to start building your own simulations using HiDEMLib and its support functions. 
The intention is that any functions described in this document should be mostly stable going forward, new functions might be added in addition to what is described here.

This document starts by describing how the input arguments are parsed in [Arguments](#arguments)

The [HiDEMLib main compute functions](#hidemlib-main-compute-functions) section covers how to set up the main iteration loop.
It also covers adding more data to the particle and link data classes, how the data is accessed and how to set up the functions used for calculating and applying forces to particles and how to update the links.

The next section covers, [Geometry functions](#geometry-functions) the functions for having the particles interact with some form of geometry, currently either a DEM based base or a mesh.

The [External force](#external-force) section covers the different ways to apply external forces to the particles as part of the simulation.

Finally the last section, [Iterate over particles / links outside HiDEMLib](#iterate-over-particles--links-outside-hidemlib), covers the helper functions that exsist in case you want to build your own functions that iterate over the particles and / or links.

# Arguments

The `ArgParser` class takes care of parsing input arguments, both from the config file and from what is passed trhough the command line.
The `ArgParser` constructor takes as arguments the `int argc, char *argv[]` from main and from these it will parse the arguments and figure out what config file to use.

```c++
int main(int argc, char *argv[]) {

    ArgParser arguments(argc, argv);
```

To access arguments the class provides one function `getArg` that can be used to get scalar `float (lpReal_t), double (hpReal_t), int, bool, and std::string` arguments.
It will return and `std::optional<T>` value of the type you specified, if the argument was not found in either the config file or the command line arguments the optional value will be empty. 

```c++
template<class T>
std::optional<T> ArgParser::getArg(std::string name) {
```

As the function returns an optional you need to either extract its value using the `value()` member function, with which the code will throw and exception if the value is not found.
Or using `value_or(<the default value to assign>)` which in case it is not found will return the value given to the function, used as a default for cases where the value is not given in the config or as a command line argument. 

Example usage getting a `bool, int, lpReal_t`:
```c++
bool xBoundaryMin = arguments.getArg<bool>("world.xBoundaryMin").value_or(false);
int timeSteps = arguments.getArg<int>("iteration.timeSteps").value_or(10000);
lpReal_t breakTime = arguments.getArg<lpReal_t >("general.breakTime").value_or(10.0);
```

The format for the variable to extract from the config file is what section it is dot the variable name, so to access `timeSteps` in the `[iteration]` section you would be getting the value for `iteration.timeSteps` like in the example.

# HiDEMLib main compute functions

Understanding of this is needed if you want to modify the way the forces are computed, and links are updated, there are ample samples you can take to start from if you can accomplish what you need with them.

The goal is that all functions described here should be stable between updates of the underlying library, any exceptions are marked in the text below, mainly it is related to things that are currently meant as feature previews.

The timestep of HiDEM is essentially 5 steps:
- Call the grid service function, that will update the grid and other acceleration things as needed 
- Compute forces between neighbouring particles
- Compute forces between linked particles
- Apply the forces computed to the particles and move them, as well as apply any external force, boundary conditions etc.
- Update the links based on the new particle positions

From the codes standpoint the computation of forces from linked and neighboring particles are merged into one iteration over the particles.
A basic in the code timestep looks as follows:

```c++
hiDem.gridService
hiDem.computePartForce
hiDem.applyForce
hiDem.linkUpdate
hiDem.finalizeTs
```

If you dont want to start breaking links from the start add an if statement around linkUpdate. And the full sample code has some additional code added for timing the timestep.

```c++
while (hiDem.run()) {
    if (hiDem.runOutput()) {
        //
    }
    //
}
```

## Adding data to particles/links

The particle and link data structures used by HiDEM can easily be extended in case you want to add some extra variables to either the particles or the links. The idea behind how it is done is the same for both, you define your own class that inhereits from the HiDEMParticle/LinkData class, you add your new ManVec to it with the appropriate type, and then you need to provide new functions that specify what the fields are used as, more details on how that works below.

### HiDEMParticleData

`HiDEMParticleData` inherits the following per particle fields from ParticleData, that is it has one value in each of those ManVecs for each particle on each rank.

`ParticleData` defines the following fields:

```c++
    ManVec<hpReal_t> xCord;
    ManVec<hpReal_t> yCord;
    ManVec<hpReal_t> zCord;
    ManVec<int> owners;
    ManVec<gKey_t> globalKeys;
    ManVec<uint8_t> type;
    ManVec<lpReal_t> radius;
```

`HiDEMParticleData` then adds the following fields:

```c++
    ManVec<lpReal_t> xTheta;
    ManVec<lpReal_t> yTheta;
    ManVec<lpReal_t> zTheta;
    ManVec<lpReal_t> xForce;
    ManVec<lpReal_t> yForce;
    ManVec<lpReal_t> zForce;
    ManVec<lpReal_t> uForce;
    ManVec<lpReal_t> vForce;
    ManVec<lpReal_t> wForce;
    ManVec<lpReal_t> xDiff;
    ManVec<lpReal_t> yDiff;
    ManVec<lpReal_t> zDiff;
    ManVec<lpReal_t> uDiff;
    ManVec<lpReal_t> vDiff;
    ManVec<lpReal_t> wDiff;
    ManVec<int> iteratedNeighbours;
    ManVec<int> iteratedLinks;
```

You can use any of these for your functions in main. 

If you want to add new fields you should create your own Particle data class that inherits from `HiDEMParticleData`, you will then need to define both a `getFieldTupleInput` and `getFieldTupleOutput` function. These shoudl return a call to `packVecPtrs` for the fields you use, the `getFieldTupleInput` should be the fields that are persistent between iterations and `getFieldTupleOutput` should be the fields where the result can be discarded between iterations and will not be stored in checkpoints (in practice the difference is just that the oputput fields are not maintained if the particle data needs to be resized or reshuffled). 

The fnctions defined in `HiDEMParticleData` looks as follows
```c++
    auto getFieldTupleInput() {
        return packVecPtrs(
                xCord, yCord, zCord, xTheta, yTheta, zTheta,
                xDiff, yDiff, zDiff, uDiff, vDiff, wDiff,
                radius, owners, type, globalKeys);
    }
```

```c++
    auto getFieldTupleOutput() {
        return packVecPtrs(
                xForce,yForce,zForce,uForce,vForce,wForce,
                iteratedNeighbours, iteratedLinks, iteratedOrder);
    }
```

To define your own, just copy these to your own data class and add the fields you added to the call to `packVecPtrs`, the order of fields in the call are irrelevant.

Additionally if the data you added needs to be initialized to a specific value you will need to add your own particle intiialization function. This can also be used if you want to change how data is initialized, just create a new data calss that just overwrites the `processInitParticle` function, however the recomended way to do things like settining an initial veloctiy would be to add an iteration pass over all the particles before the simulation starts, check the [Iterate over particles / links](Iterate-over-particles-/-links) heading.

The processInitParticle looks as follows in `HiDEMParticleData`:

```c++
    DEVCALLABLE
    void processInitParticle(ElementaryParticle &p, HiDEMParticleData &pData, pInd_t outInd) {
        pData.xCord[outInd] = p.cord.x;
        pData.yCord[outInd] = p.cord.y;
        pData.zCord[outInd] = p.cord.z;

        pData.type[outInd] = p.type;
        pData.radius[outInd] = p.radius;

        pData.owners[outInd] = p.owner;
        pData.globalKeys[outInd] = p.globalKey;

        pData.xTheta[outInd] = 0;
        pData.yTheta[outInd] = 0;
        pData.zTheta[outInd] = 0;

        pData.xDiff[outInd] = 0;
        pData.yDiff[outInd] = 0;
        pData.zDiff[outInd] = 0;
        pData.uDiff[outInd] = 0;
        pData.vDiff[outInd] = 0;
        pData.wDiff[outInd] = 0;

        pData.xForce[outInd] = 0;
        pData.yForce[outInd] = 0;
        pData.zForce[outInd] = 0;
        pData.uForce[outInd] = 0;
        pData.vForce[outInd] = 0;
        pData.wForce[outInd] = 0;

        pData.iteratedNeighbours[outInd] = 0;
        pData.iteratedLinks[outInd] = 0;
    }
```

This function is called as part of the setup of the code to initialize the particle data, yhe user does not need to call it separately.

### HiDEMLinkData

`HiDEMLinkData` works the same as `HiDEMParticleData` where `LinkData` defines the following fields:

```c++
    ManVec<gKey_t> globalKeys;
    ManVec<int> particleLinkNum;

    ManVec<int> owners;
    ManVec<uint8_t> state; // 0 is inactive everything else is seen as state
```

`HiDEMLinkData` then adds the following fields:

```c++
    ManVec<ctReal_t> CT0_flat;
    ManVec<ctReal_t> CT1_flat;
    ManVec<ctReal_t> CT2_flat;
    ManVec<ctReal_t> CT3_flat;
    ManVec<ctReal_t> CT4_flat;
    ManVec<ctReal_t> CT5_flat;

    ManVec<lpReal_t> initialLen;
```

And again you need do define functions that dictate what the fields are used for, unlike the particle data you need to define the input for the link, `getFieldTupleInput`, so one value per link and then the sides `getFieldTupleSideInput`, so one value per end of the particle. 

`HiDEMLinkData` defines the functions as:

```c++
    auto getFieldTupleInput() {
        return packVecPtrs(owners, state, globalKeys, globalKeyEndA, globalKeyEndB, 
            initialLen, maxLen, particleLinkNum);
    }
```

```c++
    auto getFieldTupleSideInput(){
        return packVecPtrs(CT0_flat, CT1_flat, CT2_flat, CT3_flat, CT4_flat, CT5_flat);
    }
```

And as with the particles the fields for the links need to be initialized. To initialize the data for the particle ends you need to get the side indexes, these are available at `lData.sideIndEndA[outInd]` and `lData.sideIndEndB[outInd]`, you can then use these to set the values at the link ends. For more dynamic intialization of link data check the [Iterate over particles / links](#iterate-over-particles--links) heading.

```c++
    DEVCALLABLE
    void processInitLink(ElementaryLink &l, HiDEMLinkData &lData, lInd_t outInd) {
        lData.globalKeyEndA[outInd] = (l.globalKeyParticleA);
        lData.globalKeyEndB[outInd] = (l.globalKeyParticleB);
        lData.owners[outInd] = (l.owner);
        lData.state[outInd] = (l.state);
        lData.globalKeys[outInd] = (l.globalKey);
        lData.particleLinkNum[outInd] = (l.particleLinkNum);
        lData.initialLen[outInd] = l.initialLen;

        pInd_t sideIndEndA = (outInd * 2);
        pInd_t sideIndEndB = (outInd * 2) + 1;

        lData.sideIndEndA[outInd] = sideIndEndA;
        lData.sideIndEndB[outInd] = sideIndEndB;

        lData.CT0_flat[sideIndEndA] = 0;
        lData.CT1_flat[sideIndEndA] = 0;
        lData.CT2_flat[sideIndEndA] = 0;
        lData.CT3_flat[sideIndEndA] = 0;
        lData.CT4_flat[sideIndEndA] = 0;
        lData.CT5_flat[sideIndEndA] = 0;

        lData.CT0_flat[sideIndEndB] = 0;
        lData.CT1_flat[sideIndEndB] = 0;
        lData.CT2_flat[sideIndEndB] = 0;
        lData.CT3_flat[sideIndEndB] = 0;
        lData.CT4_flat[sideIndEndB] = 0;
        lData.CT5_flat[sideIndEndB] = 0;
    }
```

This function is called as part of the setup of the code to initialize the link data, yhe user does not need to call it separately.

## Initializing HiDEMLib

HiDEMLib needs to be initialized and given the Particle and Link data types that are being used as template arguments.

```c++
template <class PDataT, class LDataT>
class HiDEMLib ...
```

The `setup` function from HiDEMLib then needs to be called to actually do all the setup, it needs the arguments object and the scale to used passed to it.

An example using the default `HiDEMParticleData` and `HiDEMLinkDat` classes would look as follows:

```c++
HiDEMLib<HiDEMParticleData, HiDEMLinkData> hiDem;
hiDem.setup(arguments, scale);
```

## Accesing data

Whe you delcare `HiDEMLib` in your main file you need to give it the particle and link datatypes you use as template arguments.

After HiDEMLib has been initialized you can retrive pointers to the resulting Link and Particle data. These will actually not be HiDEMParticleData and HiDEMLinkData, nor whatever custom classes you passed in, but instead they will be pointers to a `ParticleDataHandler` and `LinkDataHandler`, in practice these are classes that inherit from whatever was fed in when declaring `HiDEMLib`. From the point of the user it does not really matter you can refer to them as HiDEMParticle/LinkData or your custom data classes. It is easiest to just declare these as `auto`, if you further want to simplify your code you can also assign the values to a refernce if you directly dereference the pointer.

```c++
auto &pData = *hiDem.getParticlePtr();
auto &lData = *hiDem.getLinkPtr();
```

For more details look at the [GlacierDemExtData](../GlacierDemExtData/) sample. 

## Apply force 

The apply force is the most likely space user modifications are needed.

The function called every iteration is:

```c++
template<class ForceFun>
void HiDEMLib::applyForce(ForceFun applyForce) 
```

So a function that takes a function that will do the time integration etc to one particle, the definition of that function looks like:

```c++
auto applyForce = [=] DEVCALLABLE(pInd_t pInd, HiDEMParticleData &pData){
    // rest of the function
}
```
So the function applying the force get as arguments the current particle id, and a reference to the particle data. You can then use the ID to update any field in the particle data for that ID, and only that ID.
Additionally the lambda function captures the environment by value so any scalars read from the config file can be accessed. As well as external force pointers, bed pointers, collision geometry pointers, how to set these up is covered in the [Geometry functions](#geometry-functions) and [Extrnal force](#external-force) sections. 

## Force compute

The most complex part is how the neighbour and link forces are computed, again look at the provided samples and see if there is anything for you already there.

The function called every iteration is:

```c++
template<class NFunc, class LFunc, class ... NeighArgs, class ... LinkArgs, class ... LinkSideArgs, class ... ForceArgs>
void HiDEMLib::computePartForce(NFunc &neighbourForceFunc, LFunc &linkForceFunc,
                      std::tuple<NeighArgs...> neighArgs, std::tuple<LinkArgs...> linkArgs, std::tuple<LinkSideArgs...> linkSideArgs, std::tuple<ForceArgs...> forceArgs
```

A function taking 6 different templated arguments. The first one is the function to apply when computing forces between neighbours, the second the function for forces between linked particles. 
The rest of the arugments are tuples containing the arguments to use for the neigbour and link functions. The basic idea is you choose what arguments the force functions will get by passing different partricle and link data fields to these. These tuples can be easily constructed with the `packVecPtrs` function.

The first tuple represents the fields that will be taken from both particles when computing the neighbor force or the link force. 
The second and third one are only passed to the link force function the second one is what value to grab from the link.
The third one is the value to grab from the current side of the link, and wil be writable.
And finally the forth one is the value that will be grabbed from only the current particle that the force is computed for, these will be writable as well.

If you dont need any field from some of these you can just call `packVecPtrs` with no arguments, so if you don't need to know the link length the second one can be replaced with `packVecPtrs()`.

A full example of calling `computePartForce` looks as follows:

```c++
hiDem.computePartForce(neighbourForceFunc, linkForceFunc,
                       packVecPtrs(pData.xDiff, pData.yDiff, pData.zDiff,
                                   pData.uDiff, pData.vDiff, pData.wDiff, pData.radius),
                       packVecPtrs(),
                       packVecPtrs(lData.CT0_flat, lData.CT1_flat, lData.CT2_flat,
                                   lData.CT3_flat, lData.CT4_flat, lData.CT5_flat),
                       packVecPtrs(pData.xForce, pData.yForce, pData.zForce,
                                   pData.uForce, pData.vForce, pData.wForce));
```

The fields fed into the tuples directly affects what values are passed to the `NFunc` and `LFunc` functions. 
Both of these functions automatically get, a mask argument passed to them as well as the x,y,z distance between the two particles.
for the above example their definitions would look as follows (linebreaks added for clarity):

The neighbour force function, defined as a lambda, you can capture by value, `[=]` but not by reference, the `DEVCALLABLE` macro is also needed for the code to be compilable for running on a GPU. The `REALT_TEMPL` macro currently does nothing but will be needed for the SIMD version of the code.

```c++
auto neighbourForceFunc = [=] DEVCALLABLE REALT_TEMPL (
                
    //automatically added
    lpReal_t mask,
    const lpReal_t &xDist, const lpReal_t &yDist, const lpReal_t &zDist,

    // from first tuple, own particle
    const lpReal_t &xDiff, const lpReal_t &yDiff, const lpReal_t &zDiff,
    const lpReal_t &uDiff, const lpReal_t &vDiff, const lpReal_t &wDiff,
    const lpReal_t &radius,

    // from first tuple, neighbour particle
    const lpReal_t &xDiffOther, const lpReal_t &yDiffOther, const lpReal_t &zDiffOther,
    const lpReal_t &uDiffOther, const lpReal_t &vDiffOther, const lpReal_t &wDiffOther,
    const lpReal_t &radiusOther,

    // from last tuple the forces
    lpReal_t &xForce, lpReal_t &yForce, lpReal_t &zForce,
    lpReal_t &uForce, lpReal_t &vForce, lpReal_t &wForce)
```

So mask and x,y,z distance are added automatically, then values for the current and neighboring particles from the fields from the first tuple are added, and then the force fields from the forth tuple are added as a place you can write to. 

The link force function, again defined as a lambda, and same rules, capture by value, `[=]` and add the `DEVCALLABLE` macro. The `REALT_TEMPL` macro currently does nothing but will be needed for the SIMD version of the code.

```c++
auto linkForceFunc = [=] DEVCALLABLE REALT_TEMPL (
                
    //automatically added
    lpReal_t &mask,
    const lpReal_t &xDist, const lpReal_t &yDist, const lpReal_t &zDist,

    // from first tuple, own particle
    const lpReal_t &xDiff, const lpReal_t &yDiff, const lpReal_t &zDiff,
    const lpReal_t &uDiff, const lpReal_t &vDiff, const lpReal_t &wDiff,
    const lpReal_t &radius,

    // from first tuple, neighbour particle
    const lpReal_t &xDiffOther, const lpReal_t &yDiffOther, const lpReal_t &zDiffOther,
    const lpReal_t &uDiffOther, const lpReal_t &vDiffOther, const lpReal_t &wDiffOther,
    const lpReal_t &radiusOther,

    // from second tuple
    const lpReal_t maxLenght,

    // from third tuple
    lpReal_t &CT0, lpReal_t &CT1, lpReal_t &CT2,
    lpReal_t &CT3, lpReal_t &CT4, lpReal_t &CT5,

    // from last tuple the forces
    lpReal_t &xForce, lpReal_t &yForce, lpReal_t &zForce,
    lpReal_t &uForce, lpReal_t &vForce, lpReal_t &wForce)
```

Again mask and x,y,z distance are added automatically. Next anything from the link itself is added, so in this case the maxLength, then anything from the current side of the link, the CT values in this case. And finally again the forces to write out to.

For both of these you are responsible for accumulating the forces, so any writing to them should be done with adding in the original value.

To make another example if we wanted to grossly simplify the force calculations the whole setup could look as follows:

```c++
<...>

auto neighbourForceFunc = [=] DEVCALLABLE(
    //automatically added
    lpReal_t mask,
    const lpReal_t &xDist, const lpReal_t &yDist, const lpReal_t &zDist,
    // from first tuple, own particle
    const lpReal_t &xDiff, const lpReal_t &yDiff, const lpReal_t &zDiff
    // from first tuple, neighbour particle
    const lpReal_t &xDiffOther, const lpReal_t &yDiffOther, const lpReal_t &zDiffOther,
    // from last tuple the forces
    lpReal_t &xForce, lpReal_t &yForce, lpReal_t &zForce) 
    
<...>
    
auto linkForceFunc = [=] DEVCALLABLE(
    //automatically added
    lpReal_t &mask,
    const lpReal_t &xDist, const lpReal_t &yDist, const lpReal_t &zDist,
    // from first tuple, own particle
    const lpReal_t &xDiff, const lpReal_t &yDiff, const lpReal_t &zDiff,
    // from first tuple, neighbour particle
    const lpReal_t &xDiffOther, const lpReal_t &yDiffOther, const lpReal_t &zDiffOther,
    // nothing from second tuple
    // nothing from third tuple
    // from last tuple the forces
    lpReal_t &xForce, lpReal_t &yForce, lpReal_t &zForce)
            
<...>

hiDem.computePartForce(neighbourForceFunc, linkForceFunc,
                       packVecPtrs(pData.xDiff, pData.yDiff, pData.zDiff),
                       packVecPtrs(),
                       packVecPtrs(),
                       packVecPtrs(pData.xForce, pData.yForce, pData.zForce));
```

## Update links

Updating links works broadly in the same way as the neighbour and link force calculation, you feed in a bunch tuple of pointers and the function updating the links will be called with values extracted from those pointers for the current link.

The function called every iteration to update the links looks as follows:

```c++
template<class LinkFun, class ... NeighArgs, class ... LinkSideArgs, class ... LinkArgs>
void linkUpdate(LinkFun breakLinks,
                std::tuple<NeighArgs...> neighArgs, std::tuple<LinkSideArgs...> linkSideArgs, std::tuple<LinkArgs...> linkArgs)
```

It takes the function to apply to each link and the aguments to unpack. 

```c++
hiDem.linkUpdate(breakLinks,
             packVecPtrs(pData.xCord, pData.yCord, pData.zCord,
                         pData.xTheta, pData.yTheta, pData.zTheta),
             packVecPtrs(),
             packVecPtrs(lData.state, lData.initialLen));
);
```
The first tuple names what argments to fetch from the particles at each end of the link from the particle data, the second what data to fetch from each side of the link from the link data, and finally the least one is what data to fetch for each link.

# Geometry functions

// todo provide intro here
Geometry is the other thing you want the particles to run into. Currently two types are supported, a basic DEM based bed and a mesh, how to set these up in the input file is covered in [Input.md](Input.md), this section will only conver how to interact with them from the code side.

## Bed

If you have the bed configured correctly in the input config file you can retrive the bed object from HiDEMLib using:

```c++
auto bed = hiDem.getBedPtrs();
```

The bed project then has two function one to get the height of the bed at a given x and y coordinate and then one function to get the friction at a given x and y coordnate.

```c++
DEVCALLABLE
lpReal_t getHeight(lpReal_t xCord, lpReal_t yCord)

DEVCALLABLE
lpReal_t getFric(lpReal_t xCord, lpReal_t yCord)
```

An example of how to use it, the code below would be added to `applyForce` and called for each particle, computing the possible interaction between the particle and the bed.

```c++
lpReal_t bedzCord = bed->getHeight(xCord, yCord);

// Set dragcoeffcients
lpReal_t dragCoeff;
if ((zCord + radius) - bedzCord < bedInteract) { // should be - radius ?
    dragCoeff = std::min(bedDrag * bed->getFric(xCord, yCord), lpReal_t(1e+5) * scale * scale);
} else if (zCord > waterLevel) {
    dragCoeff = airDrag;
} else {
    dragCoeff = waterDrag;
}
```        

## Mesh

**This is a "feature preview" and is subject to change**

If you have some collision meshes configured properly you can retrive a combined mesh object from HiDEMLib using the `getCollisionMeshPtr` function. This will return a mesh object that includes all the meshes defined in collisionGeometry in the config file. Once retrived you need to generate an acceleration structure for the mesh using `generateAccel` that takes as an argument the search distance to use, a good part to start from is what is used in the example below, that is twice the particle radius

```c++
InputMesh* mesh = hiDem.getCollisionMeshPtr();

mesh->generateAccel(hiDem.geometry.maxRadius*2);
```

Interaction between the partcles and the mesh would be done in `applyForce`, each particle calling `closestTriFun` which will then call the supplied lambda for the face closest to the particle if the particle is close enough to interact. The function takes the x,y,z cordinates of the current particle, the distance at which interaction can happen and the particle index. The lambda function then gets called with the index of the closest triangle and the actual distance to it from the point given. 

You can use this to compute any forces on the particle from the triangle, if you want to save the force on the triangle the mesh class has a function called `addForce` that takes the triangle index and the force to add, currently only x,y,z force is supported.
The actual code provided in the example is illustrative and not necessarily an accurate implementation of the physics of the collision.

```c++
mesh->closestTriFun(xCord,yCord, zCord, bedInteract, pInd, [&] (int tri, lpReal_t dist){
    lpReal_t xForceCol = -mesh->dataPtr->xNorm[tri] * (bedStiffness * (-dist + bedInteract));
    lpReal_t yForceCol = -mesh->dataPtr->yNorm[tri] * (bedStiffness * (-dist + bedInteract));
    lpReal_t zForceCol = -mesh->dataPtr->zNorm[tri] * (bedStiffness * (-dist + bedInteract));

    mesh->addForce(tri, xForceCol, yForceCol, zForceCol);

    xForce += xForceCol;
    yForce += yForceCol;
    zForce += zForceCol;
});
```

If you add forces to the mesh you need to zero them yourself, this is done simply by calling `zeroForce()` on the mesh object.

# External force

If external forces are set up properly in the inout file you can retrive a pointer to the `ExternalForces` object from HiDMELIb using:

```c++
ExternalForces *extForcePtr = hiDem.getExternalForcePtr();
```

And then in the `applyForce` function you can retrive the force calling the `interp(xCord, yCord)` function on the x,y,z,u,v,wForce members in the `ExternalForces` object. The `interp` function is called regardless if the force is a scalar or a map, both are handled the same way from the code side.

```c++
xForce += extForcePtr->xForce->interp(xCord, yCord);
yForce += extForcePtr->yForce->interp(xCord, yCord);
zForce += extForcePtr->zForce->interp(xCord, yCord);
uForce += extForcePtr->uForce->interp(xCord, yCord);
vForce += extForcePtr->vForce->interp(xCord, yCord);
wForce += extForcePtr->wForce->interp(xCord, yCord);
```

# Iterate over particles / links outside HiDEMLib

## Particles

If you want to have additonal passes of iterating over the particles or links, for instance to apply some initial condition or compute some statisitcs at the output steps there are a few helper functions that will simplify the process. Note that these are not intended to be used every iteration they will not do all the fancy under the hood optimizations that the functions described above. The functions to iterate particles and links come in two variaties, iterating all elements on a rank or iterating just the owned ones, generally you probably want to iterate just the owned elements, that will exclude the halo elements each rank has that are actually owned bu some other rank.

To iterate over the partiles use the iterateOwned particles from the `ParticleData` class (or any class inheretingn from it):

```c++
template<class Policy, class Fun, class... Args>
void ParticleData::iterateOwnedParticles(Policy policy, Fun fun, Args &... args)
```

Which takes an execution policy, where you generally want to use `OffloadExec`, and a lambda function to apply to each particle, this needs to be annotated with `DEVCALLABLE` to work on code offloaded to a GPU.

As an example if you want to assign an initial velocity to the particles you can add an extra iteration over the particles before the time stepping starts:

```c++
pData.iterateOwnedParticles(OffloadExec(), [=] DEVCALLABLE (pInd_t pInd, HiDEMParticleData &pData){
    pData.xDiff[pInd] = 10.f;
}, pData);
```

The lambda function applied to each particle gets called with the particle index and any arguments passed after the lambda to the `iterateOwnedParticles`, in this case we pass in a reference to `HiDEMParticleData` to have easy access to the particle data fields.

## Links

For the links there is a similar function in the `LinkData` class:

```c++
template<class Fun, class... Args>
void LinkData::iterateOwnedLinks(OffloadExec policy, Fun fun, Args &... args)    
```

Which can be used as follows:

```c++
lData.iterateOwnedLinks(OffloadExec(), [=] DEVCALLABLE (lInd_t lInd, pInd_t aParticleInd, pInd_t bParticleInd, HiDEMLinkData &lData, HiDEMParticleData &pData){
    lpReal_t linkLenX = std::abs(pData.xCord[aParticleInd] - pData.xCord[bParticleInd]);
    lpReal_t linkLenY = std::abs(pData.yCord[aParticleInd] - pData.yCord[bParticleInd]);
    lpReal_t linkLenZ = std::abs(pData.zCord[aParticleInd] - pData.zCord[bParticleInd]);
}, lData, pData);
```
The lambda function applied to each link gets called with the link index, and the particle indexes for the particles at both ends and any arguments passed after the lambda to the `iterateOwnedLinks`, in this case we pass in a reference to `HiDEMLinkData` and `HiDEMParticleData` to have easy access to the particle and link data fields.

### Links for a given particle
// todo will be added later

### Neighbours for a given particle
// todo will be added later
