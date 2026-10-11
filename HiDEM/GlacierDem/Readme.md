# Glacier DEM sample

This sample recreates what HiDEM used to simulate, so a glacier based on DEM input files for the bottom and base of the glacier.

# Code

The code is set up to handle both interaction with the bottom surface and have external forces applied from either scalar or mapped values. 

# Configs

The sample includes a few different sample config files illustrating different use cases. 
The code is set up so that any of these samples can be ran without any modification to the code.

## Basic DEM

The `config_dem.toml` file illustrates a very basic setup with just a base and surface DEM and particles being generated between them.

The `config_dem_large.toml` is a larger example doing the same thing just generating a lot more particles for multi rank testing.

## Using maps for external forces and generation probabilities

The `config_dem_maps.toml` extends the basic example by adding a map for external forces as well as maps for controlling the probability for particles and/or links to be generated.

## Multi Ice types

The `config_dem_multi.toml` file illustrates having multiple different types of ice types, each with different parameters for how many particles are generated and how many links start broken.


