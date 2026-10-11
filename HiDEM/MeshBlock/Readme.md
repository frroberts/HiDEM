# Mesh block setup

This sample is set up to generate a block of ice based on a bounding mesh, that is it will generate particles inside a block defined by a bounding mesh.
The mesh used here is a double wedge shape. The mesh needs to be a manifold, that is a watertight mesh with no holes in it, the code will make no attempt to fill holes and meshes with holes in them, nor will it warn about it, the code will generate really strange geometries if the mesh has holes in it.

# Benchmarking

The real main use of this sample is to benchmark the main simulation loop of the code with as little variation as possible, as such there is no collision geometry included.

Further instructions on how to run the benchmarks are found in the Benchmark subdirectory here.

