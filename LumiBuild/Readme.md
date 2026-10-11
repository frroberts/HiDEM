This is a temporary workaround for building GPU binaries on LUMI, this is not intended for any CPU builds or anything else. The makefile is crude and simple and does not replicate all the functionality of the cmake build system.

As rocm is loaded from lustre expect compile times to be longer than previously.

# Modules

Load the following modules :

`PrgEnv-gnu LUMI/23.09 partition/G rocm cray-hdf5-parallel`

Generally you want to use the newest version of rocm if possible, and this assumes the MPICH module is loaded by default.

# Building

Build with `make -j <sample>`

Where `<sample>` is any of the samples in the HiDEM directory.

This will create a `HiDEM` binary built from the sample code you chose.

# Options

The following options are set, generally I suggest you let them be, unless you want to build a double precision version of the code 
```makefile
DEFS += -DENABLE_HIP=1
DEFS += -DENABLE_OMP=1
DEFS += -DENABLE_HDF5=1
DEFS += -DENABLE_FMATH=1
DEFS += -DENABLE_RUN_REPRODUCE=1
DEFS += -DENABLE_SMEM=1
DEFS += -DSET_FP32=1
DEFS += -DSET_CT_FP32=1
DEFS += -DAOSOA_CT=1
DEFS += -DENABLE_GDR=1
```

# Running

Generally follow the instructions set out in: https://docs.lumi-supercomputer.eu/runjobs/scheduled-jobs/lumig-job/

The code has issues on AMD hardware if each rank sees more than one GPU, so use the `select_gpu` script that is provided in the lumi documentation pages.


