# HiDEM

# Background

HiDEM is a code used for simulating fractures, mainly used for modeling ice fractures in glaciers and sea ice.

# Code structure

The code is structred such that the setup and compute parts are all incorporated into HiDEMLib, that is then used in main to glue together the kind of simulation you want to run. 

This directory incldues sample main files all doing some form of simulations of ice. The samples are not intended to be physically correct but are instead set up to showcase a functionality of the code and how that affects how the simulation is run.

The samples are located in:

* [IceBlock](IceBlock)

* [GlacierDem](GlacierDem)

* [GlacierDemExtData](GlacierDemExtData)

* [GlacierDemExtraIter](GlacierDemExtraIter)

* [SheetMeshCollision](SheetMeshCollision)

* [GlacierBlockPack](GlacierBlockPack)

If you want to create your own custom main there is a file set up to be built in:

* [MainCustom](MainCustom)

# Building

Dependendices: 
* A modernish c++ compiler that supports at the very least c++20 (the exception here is that the CUDA parts can be compiled with c++17 as cuda does not support c++20).
* MPI, even if you only plan to run a single rank you still need MPI.
* HDF5, a parallel implementation is needed.
* Cmake, currently set to 3.16 as a minimum, however for CPU builds lower versions may work.
* (optional) CUDA, tested with 12.x, older cuda versions that do not support c++20 are not supported
* (optional) AMD HIP support, tested with rocm 6.0.x

To get the code clone the repository recursively, `git clone --recursive git@github.com:frroberts/Phell.git` that will pull all the needed extra libraries.


To build the code you need to follow the basic cmake building scheme

```
mkdir build
cd build
cmake ..
make -j HiDEM
```
This will build the code in [MainCustom/MainCustom.cpp](MainCustom/MainCustom.cpp), which when you clone the repository is empty.

The code comes with samples located under the Samples directory, the code in these is set up to run different 
versions of the code, each comes with its own readme explaining what is being run. The idea is that you can base your own code on these, so copy whatever is closest to your case into the MainCustom.cpp file in this directory.

To change what sample to build use `-DHiDEMType=<Type>` where `Type` is the name of the sample directory to build.  

For the cmake there are a bunch of options that can be defined, however with no option it will build a CPU version of the code that should be fairly optimized.
* `-DENABLE_SIMD256` (OFF)
  * Use SIMD for the force calculations, using AVX2
* `-DENABLE_SIMD512` (OFF)
  * Use SIMD for the force calculations, using AVX512
* `-DENABLE_FMATH` (ON)
  * Enable fast math
* `-DENABLE_CUDA` (OFF)
  * Enable CUDA, cannot be enabled at the same time as SIMD
* `-DENABLE_HIP` (OFF)
  * Enable HIP, cannot be enabled with SIMD or CUDA
* `-DENABLE_OMP` (ON)
  * Disable or enable OpenMP
* `-DENABLE_GDR` (OFF)
  * Enable GPU aware MPI, improves performance for GPU based builds
* `-DENABLE_SMEM` (ON)
  * Enable shared memory caching for GPU builds
* `-DENABLE_PSTL` (ON)
  * Enable parallel STL, for sorting and other operations. Requires that the compiler has a parallel implementation of STL available

#### Precision flags
* `-DSET_FP32` (ON)
  * Set all but coordinates and CT to use FP32
* `-DSET_CT_FP32` (ON)
  * Set CT to use FP32
These are both default on, if you need FP64 you need to set these to OFF. 

#### Feature flags

#### Debugging flags
* `-DENABLE_CATCH` (OFF)
  * Enable catch, needed to get tests built
* `-DENABLE_ASAN` (OFF)
  * Enable address sanitizer (for development use)
* `-DENABLE_UBSAN` (OFF)
  * Enable undefined behavior sanitizer (for development use)

## CPUs with SIMD

SIMD comes in two different flavors 256 bit wide with AVX2 and 512 bit wide with AVX512. 
Which one to enable depends on the hardware you use, see [SIMD.md](Docs/SIMD.md) for more details on SIMD.
Generally if you can use it SIMD can give a small performance boost, depending on how you run the code.

AVX2 builds
```
mkdir build
cd build
cmake -DENABLE_SIMD256=1 ..
make -j HiDEM
```

AVX512 builds
```
mkdir build
cd build
cmake -DENABLE_SIMD512=1 ..
make -j HiDEM
```

Both cannot be enabled at the same time.

## Nvidia GPU support

For GPU builds SIMD needs to be disabled, and then your choice of GPU programming language needs to be set.

```
mkdir build
cd build
cmake -DENABLE_SIMD=0 -DENABLE_CUDA=1 -DENABLE_GDR=1 ..
make -j HiDEM
```

In case your MPI library is very very old and does not support GPU aware MPI set `-DENABLE_GDR` to 0.

## AMD GPU support om LUMI

If you are building the code for to use AMD GPUs on the LUMI supercomputer, use the [LumiBuild](../LumiBuild/Readme.md) instructions. 

## AMD GPU support

For GPU builds SIMD needs to be disabled, and then your choice of GPU programming language needs to be set.
Currently, AMD GPU support is only tested on HPE Cray EX systems with MI250x GPUs.
Shared memory caching is essential for good performance on AMD hardware.

```
mkdir build
cd build
cmake -DENABLE_HIP=1 -DENABLE_SIMD=0 -DENABLE_SMEM=1 -DENABLE_GDR=1 ..
make -j HiDEM
```
Running on AMD GPUs it is highly recommended that you ensure each process only has a single device exposed to it. I.e. use a helper script that sets `ROCR_VISIBLE_DEVICES` to the apropriate value before running HiDEM.

In case your MPI library is very very old and does not support GPU aware MPI set `-DENABLE_GDR` to 0.

# Running

The sample directories come with sample input files and associated files to run, and instructions for what is in the config file is located in [Docs/Input.md](Docs/Input.md)

# Output 

How to control the code output is described in [Docs/Output.md](Docs/Output.md)

# Extending / Modifying Main

For instructions on how to modify the code and how to use the functions of HiDEMLib have a look at [Docs/UsingHiDEMLib.md](Docs/UsingHiDEMLib.md)
