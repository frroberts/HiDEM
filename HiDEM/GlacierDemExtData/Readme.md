# Glacier DEM sample with extended data

This sample illustrates how the basic `HiDEMParticle` and `HiDEMLinkData` can be extended, the config is the same as the `config_dem.toml` from the `GlacierDem` sample.
The big changes is in the code that introduces extended `HiDEMParticleDataExt` and `HiDEMLinkDataExt` data classes. 
These add som fields but nothing real is done with these other than some data being written to them in different places, they do not affect the simulation outcome in any way. 

