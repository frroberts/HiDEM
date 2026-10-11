# Input

This file describes the configuration variables used by HiDEMLib. These are parsed internally inside HiDEMLib when you call the `void setup(ArgParser &parser, double scale)` function. 

## Scalar or map variables

For some input variables it is possible to give either a scalar or a 2d map as the value, these are marked in the readme below.

The 2d map must be given in the format `{type = "csv2d", path = "path to file", fieldID = 2}` where the type must be `csv2d`, the path is the file to the CSV file, and `fieldID` is the column to use.

## Command line arguments

The binary also accepts command line arguments for some parameters from the config file. The structure is such that the headings in the file is combined with the parameter with a . Some examples:

    --iteration.timeSteps = 2131 will set the number of iterations to run
    --restart.fromRestart = ../restart.h5 will set the program to restart from the provided file

Currently, everything that is not a toml array is settable from the command line, so you cannot set [[iceParameters]] or [[geometry]].

# General

The variables in the `[general]` section are only used in the main function, these are mostly controlling the physics parts, essentially entirely under the users control. 
Everything else in this document is parsed by `HiDEMLib.setup` and used internally. 

# Iteration

The main teration loop is controlled by the `iteration` block, and it contains two variables, the number of time steps to run and at what interval to run the output functions.

```toml
[iteration]
# number of iterations to run
timeSteps = 400000
# interval of how often to output
outputInterval = 1000
```

The interval at which output is written is also controlled from here, for more info on the output functions have a look at the [Output.md](Output.md) document. 

# Grid

The grid section controls the main acceleration structure for finding neighbours, most of the functionality is automated. The user needs to set the search distance for finding neighbours, using `searchDistanceScale`. In case the automatic regeneration missbehaves you can manually set the interval at which the grid will be regenerated with the `gridRegenInterval` variable.

The grid acceleration works by dividing each process simulation domain into a grid with each grid cell being a box of size `max particle diameter * searchDistanceScale`.
With this grid we know that the potential particles any given particles can interact with is within the grid cell of the current particle or any of the 26 neighboring grid cells.
`searchDistanceScale` is used to have a bit more buffer in the search distance so that we do not need to regenerate the grid every iteration.
There is a second step in the process that will use the grid to generate a neighbour list for each individual particle consisting of all the particles within the `max particle diameter * searchDistanceScale` distance. 
This neighbour list is then what is used to determine the nearest neighbours at each iteration, and it will be regenerated at the same time the grid is regenerated.

```toml
[grid]
# searchDistanceScale will compute max particle diameter * searchDistanceScale and use it for the distance
searchDistanceScale = 1.4
# interval at which to regenerate the neighbour grid
gridRegenInterval = -1
```

# Distribution

The distribution section controls how the code is distributed over multiple processes. 

```toml
[distribution]
# how big should the halos be, done as searchDistance * haloScale
haloScale = 1.2
# at what iteration to do the first rebalance, to get the particles a chance to move a bit for a better balance result
firstBalanceTimestep = 200
# tolerance to aim for at each split
reBalanceTolerance = 0.001
# number of points to sample from the geometry for the initial balance step
# reduce for faster initialization (not a significant change)
initBalancePoints = 100000
# tolerance to aim for at each split
initBalanceTolerance = 0.01
```

# World

The `[world]` section is used to limit the absolute simulation domain, and what to do at the limits of it.
The `x/y/zLower/Upper` values are used to set the limit.
this is however not enforced in any `HiDEMLib` code but rather used for the users simulation code. 
If you omit the boundary condition section of the `applyForce` function these will do nothing.

```toml
[world]
# limit geometry generation to this XYZ box
# these are optional, and if left out the geometry will be based on the geometry limits
xInputUpper = 1000
yInputUpper = 1000
zInputUpper = 1000.0
xInputLower = 0.0
yInputLower = 0.0
zInputLower = 0.0
```

# Restart

The `[restart]` section allows you to control how the code writes checkpoints to restart from, how often what to name them, if you want them all to have a unique timestamp and if you want the code to write out a final checkpoint at the end.

The `fromRestart` argument then controls if you want to start from a restart file or not, if left empty the code will start a new simulation. 
If not the code will restart from that file, however the restart file does not include the configuration file, so the rest of the configuration file should match the simulation you restart from.
However some arguments can be changed in case you want to steer the simulation. Anything changing related to link and particle generation wont obviously have any impact as the particles and links are already generated. 
Things like the water leve, external forces, parameter for the physics etc. can be changed and the run starting from the restart will use the updated values.

```toml
[restart]
# file to restart from if this is set it will look for the file to restart from
# fromRestart = ""
# enable checkpoint writing at end
writeEndCheckpoint = true
# interval at which to write the checkpoint -1 for no periodic
checkpointInterval = 1000
# directory name in the working path to create the checkpoints in
checkpointDirectory = "checkpoints"
# append timestep to filename, if false it will only keep one file
timestepCheckpoint = true
```

# Lattice types
The `[lattice]` segment allows you to set different types of lattices to genererate the geometry from. Currently supported lattice `type`s are `FCC` and `Block`. 

Any sizes given here are unscaled, so the true size will be computed as `size * scale`

Example of a lattice block:
```toml
# unscaled radius and spacing
[lattice]
type = "FCC"
# yeah these constants are a bit arbitrary
particleRadius = 0.5
fccSpacing = 1.41421
linkGenScale = 1.2

# seed for random generation
seed = 1
# offset to add to seed for each rank
seedOffset = 10
# center the generated geometry around zero
zeroCenter = false
```

## Common parameters

`linkGenScale` which is the range praticles should be linked in, computed as (r1+r2)*linkGenScale and anything below that will be linked.

`seed` the seed to use for random value generation

`seedOffset` the offset applied to the seed for each rank, so the seed for rank N would be `seed + (N * seedOffset)`

`zeroCenter` centers the simulation around zero. (currently disabled)

## FCC lattice
The FCC lattice needs the following specified:

`particleRadius` which is the radius of the particles

`fccSpacing` which is the spacing of the particles in the fcc lattice

## Block lattice

**This is a "feature preview" and is subject to change**

Block lattice allows you to load a file representing a unit cell that will then be used to generate the lattice, currently it reads a csv with the format id,xCord,yCord,zCord,radius.
The block lattice type needs the following specified

`path` which is the path where to read the CSV from

`blockSize` is the unscaled size of the block, the assumption is that the block is square

# Geometry and Ice types

## Ice types

Ice types are defined in the `[[iceParameters]]` array, even for simulations with just one type of ice you need to define at least one ice type.
The ice types may eventually end up being able to have more parameters that are specific for that type, 
however for now the only supported parameters are `linkGenerateProbability`, `particleGenerateProbability`, `boundingGeometry`, and `linkWith`.

```toml
[[iceParameters]]
# glacier porous
linkGenerateProbability = 1.0#{type = "csv2d", path = "../Samples/mass3.csv", fieldID = 5}
particleGenerateProbability = 1.0#{type = "csv2d", path = "../Samples/mass3.csv", fieldID = 6}
boundingGeometry = "glacier"
linkWith = [0]
```

`linkGenerateProbability` is the probability that a link will be generated, this can either be a scalar or a 2d map, see the [Scalar or map variables](##Scalar-or-map-variables) section for a description on how to define these 

`breakLinkProbability` used to be allowed, but that was changed to `linkGenerateProbability` which is the inverse from the previous one, config files with `breakLinkProbability` defined will not run

`particleGenerateProbability` is the probability that a particle will be generated, this can either be a scalar or a 2d map, see the [Scalar or map variables](##Scalar-or-map-variables) section for a description on how to define these 

`boundingGeometry` is a string description of the geometry inside which the particles for this ice type is generated, how this is constructed is described below.

`linkWith` is and array of integers, even if only one element is defined. This is the indexes of the different ice types the current type will be linked with. 
At the very least the list should include the type itself. To generate links between two ice types the other type needs to be listed for both types.
Assuming you have two ice types defined as the first and second element in the array, 
to get these to link to themselves and the other ice type they both need to be defined with `linkWith = [0,1]`. 
If you do not want them to link to each other the first one should be defined with `linkWith = [0]` and second with `linkWith = [1]`.

## Bounding geometry
## Primary geometry

There are currently 3 primary geometries that can be defined, which are further used to define compound geometries. 
Each primary geometry has a type field that describes what type of geometry it is.
They also have a name, the name needs to be unique for each geometry, these names are then used when constructing compound geometries. 
Note that the name is case-sensitive, if you call something "glacier", you will need to refer to it the same. 

### Box

A box geometry is just a rectangular box, it needs an upper and lower coordinate which then defines the upper and lower corners of the box. 
The coordinates are given as arrays of 3 numbers.

```toml
[[geometry]]
type = "box"
name = "b4"
lower = [0,370,50]
upper = [500,400,500]
```

### Dem

The Dem input is the basic input based on two heightmaps and will define a geometry between the two maps.
For arguments you need to define the file as well as what fields that are used for the top and the base maps.

```toml
[[geometry]]
type = "dem"
name = "glacier"
path = "../Data/mass3.csv"
topFieldID = 2
baseFieldID = 3
```

### Mesh

For the mesh geometry the input needs to be and .obj file, and the mesh needs to be solid. 
The mesh geometry is somewhat special in that it has optional scale and position arguments, if left undefined scale will default to {1,1,1} i.e. no scaling applied adn position will default to {0,0,0}.

```toml
[[geometry]]
type = "mesh"
name = "teapot"
path = "../Data/teapot.obj"
scale = [-10,-10,-10] # optional, otherwise 1
position = [250,250,500] # optional, otherwise 0
```

## Collision geometry

Collision geometry is any geometry that the particles will interact with. Currently the supported collision geometry types are a simple bed geometry, so a DEM of the bottom surface the particles interact with. 
In addition to this there is experimental support for collisions between particles and a static mesh geometry. 

Collision geomtry can be used as a bounding geometry as well, which must be used to avoid generating particles inside your geometry.

As with the bounding geometries, each primary geometry has a type field that describes what type of geometry it is, and follows the same rules.

How to use collision geometries in the code in described in XXX. # todo fix reference here

### Bed

It is mandatory to have a bed geometry defined it will be used for the bed layer of the simulation.
In addition to the name and type arguments a bed needs to have the path to the input defined, as well as the field of the inout used for the bed level as well as the friction.
In practice any coordinate above the bed plane is included in this geometry.

```toml
[[collisionGeometry]]
# mandatory to have one bed geometry
type = "bed"
name = "bed"
path = "../Data/mass3.csv"
baseFieldID = 3
frictionFieldID = 4
```

### Mesh

**This is a "feature preview" and is subject to change**

For the mesh geometry the input needs to be and .obj file, and the mesh needs to be solid. 
The mesh geometry is somewhat special in that it has optional scale and position arguments, if left undefined scale will default to {1,1,1} i.e. no scaling applied adn position will default to {0,0,0}.

```toml
[[collisionGeometry]]
type = "mesh"
name = "teapot"
path = "../Data/teapot.obj"
scale = [-10,-10,-10] # optional, otherwise 1
position = [250,250,500] # optional, otherwise 0
```

## Compound geometry

Compound geometries are then created for each ice type, this is defined in a string for the different types in the `[[iceParameters]]` array in the `boundingGeometry` field.
The string here is based on some very basic boolean algebra. It supports 3 operations, AND, OR, NOT, and parenthesis. 
The operations are always written in caps, otherwise they will be interpreted as geometry names. 
Compound geometries does not need to include any operations and can just include the name of a primary geometry. 

### OR

Or is the basic combination operation particles will be generated if the particle coordinate is part of either of the geometries.

### NOT

Not is the subtraction operation, this would generate particles for coordinates in the first geometry and not in the second one.

### AND

And will generate particles if the coordinates is inside both geometries.

### Examples

Assuming we have the following primary geometries defined:

```toml
[[geometry]]
type = "box"
name = "sea_ice"
lower = [50,50,0]
upper = [110,370,500]

[[geometry]]
type = "dem"
name = "glacier"
path = "../Data/mass3.csv"
topFieldID = 2
baseFieldID = 3

[[geometry]]
type = "mesh"
name = "teapot"
path = "../Data/teapot.obj"

## todo fix, this is now of type collision geometry
[[collisionGeometry]]
type = "bed"
name = "bed"
path = "../Data/mass3.csv"
baseFieldID = 3
frictionFieldID = 4
```

Assume we want to generate a basic DEM input we can just set `boundingGeometry = "glacier"`. 
Now if we would like to add a layer of sea ice to this we can do that by adding a block to the geometry `boundingGeometry = "glacier OR sea_ice"`. 
This would create particles for the combined geometry, note that these will then be place in the same ice type.

If you want the sea ice in another ice type you need to define the first ice type with `boundingGeometry = "glacier"`. 
The second then needs to be the sea ice *but* you need to remove the glacier ice from that ice type, otherwise double particles will be generated where these geometries overlap.
That is the second sea ice would need to be defined with `boundingGeometry = "sea_ice NOT glacier"`. 

To cut off any geometry that extend below the bed you can add an `AND bed` to the compound geometries defined. 
As an example `boundingGeometry = "(glacier OR teapot) AND bed"`, would generate particles for the combination of glacier and teapot but would cut off any that would be generated under the bed.

# External forces

In the `[externalForce]` secrion you can define what external forces will be applied to all particles at every iteration. The external forces can be either a floting point value, that will then be applied to all particles. Or in the form of a 2d map, where the particles position will be used to interpolate from the map what the force is. All of the external forces support either scalar or 2d maps, see the [Scalar or map variables](##Scalar-or-map-variables) section for a description on how to define these 

An example of external forces mixing both scalar and maps is given below.

```toml
[externalForce]
# external forces to be applied each time step
xForce =  {type = "csv2d", path = "../Samples/force.csv", fieldID = 2}
yForce = 0.0
zForce = 10.0
uForce = 0.0
vForce = 0.0
wForce = 0.0
```

External forces have to be set up correctly in the code to work, see XXX  # todo fix reference here

