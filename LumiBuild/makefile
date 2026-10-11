INCL_PATHS = -I ../Iterators/ -I ../PLiB/ -I ../HiDEMLib/ -I ../External/fmt/include/ -I ../External/toml11/ -I ../External/Vectorclass/

MPI_BASE_PATH = ${MPICH_DIR}/
HDF5_BASE_PATH = ${HDF5_DIR}/

MPI_INCL = -I ${MPI_BASE_PATH}include
HDF5_INCL = -I ${HDF5_BASE_PATH}include

DEFS = -DMAKEFILE_BUILD
DEFS += -DFMT_HEADER_ONLY
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
DEFS += -DENABLE_ASYNC_COMM=1
DEFS += -DMITIGATE_HDF5_DEV_WRITE_ISSUE=1

COMP = hipcc -Wno-unused-result
FLAGS = ${INCL_PATHS} ${MPI_INCL} ${HDF5_INCL} ${DEFS} --std=c++20 -O3 -g --offload-arch=gfx90a:xnack+
LINK = hipcc

LINKFLAGS = ${LINK} ${DEFS} -o HiDEM Util.o Balancer.o NeighbourGrid.o RangeCollection.o GeometryCollection.o Map2dLoader.o \
InputGeometry.o CompoundInput.o MapLoader.o InputMesh.o HiDEMDistribution.o HiDEMH5Checkpoint.o \
HiDEMGeometry.o -lhiprand -L ${MPI_BASE_PATH}lib  -L ${HDF5_BASE_PATH}lib -lmpi -lhdf5 -lgomp \
-L${MPICH_DIR}/lib -lmpi ${CRAY_XPMEM_POST_LINK_OPTS} -lxpmem ${PE_MPICH_GTL_DIR_amd_gfx90a} ${PE_MPICH_GTL_LIBS_amd_gfx90a} -I${MPICH_DIR}/include -o HiDEM

OBJECTS = Util.o Balancer.o NeighbourGrid.o RangeCollection.o GeometryCollection.o Map2dLoader.o \
InputGeometry.o CompoundInput.o MapLoader.o InputMesh.o HiDEMDistribution.o HiDEMH5Checkpoint.o \
HiDEMGeometry.o


MainCustom : MainCustom.o ${OBJECTS}
	${LINKFLAGS} MainCustom.o
GlacierDem : MainDem.o  ${OBJECTS}
	${LINKFLAGS} MainDem.o
GlacierDemExtData : MainDemExtData.o  ${OBJECTS}
	${LINKFLAGS} MainDemExtData.o
IceBlock : MainIceBlock.o  ${OBJECTS}
	${LINKFLAGS} MainIceBlock.o
MeshBlock : MainMeshBlock.o  ${OBJECTS}
	${LINKFLAGS} MainMeshBlock.o
SheetMeshCollision : MainSheet.o ${OBJECTS}
	${LINKFLAGS} MainSheet.o
LinkRegen : MainRegen.o ${OBJECTS}
	${LINKFLAGS} MainRegen.o

Global.h : ../PLiB/Global.h.in
	cp ../PLiB/Global.h.in ../PLiB/Global.h

Util.o : ../PLiB/Util/Util.cpp Global.h
	${COMP} ${FLAGS} -c $<
Balancer.o : ../PLiB/Balancer/Balancer.cpp Global.h
	${COMP} ${FLAGS} -c $<
NeighbourGrid.o : ../PLiB/NeighbourGrid.cpp Global.h
	${COMP} ${FLAGS} -c $<
RangeCollection.o : ../PLiB/LoggerV2/RangeCollection.cpp Global.h
	${COMP} ${FLAGS} -c $<
GeometryCollection.o : ../PLiB/Geometry/GeometryCollection.cpp Global.h
	${COMP} ${FLAGS} -c $<
Map2dLoader.o : ../PLiB/Geometry/Map2dLoader.cpp Global.h
	${COMP} ${FLAGS} -c $<
InputGeometry.o : ../PLiB/Geometry/InputGeometry.cpp Global.h
	${COMP} ${FLAGS} -c $<
CompoundInput.o : ../PLiB/Geometry/CompoundInput.cpp Global.h
	${COMP} ${FLAGS} -c $<
MapLoader.o : ../PLiB/Geometry/MapLoader.cpp Global.h
	${COMP} ${FLAGS} -c $<

HiDEMGeometry.o : ../HiDEMLib/HiDEMGeometry.cpp Global.h
	${COMP} ${FLAGS} -c $<

MainCustom.o : ../HiDEM/MainCustom/MainCustom.cpp Global.h
	${COMP} ${FLAGS} -c $<
MainDem.o : ../HiDEM/GlacierDem/MainDem.cpp Global.h
	${COMP} ${FLAGS} -c $<
MainDemExtData.o : ../HiDEM/GlacierDemExtData/MainDemExtData.cpp Global.h
	${COMP} ${FLAGS} -c $<
MainIceBlock.o : ../HiDEM/IceBlock/MainIceBlock.cpp Global.h
	${COMP} ${FLAGS} -c $<
MainMeshBlock.o : ../HiDEM/MeshBlock/MainMeshBlock.cpp Global.h
	${COMP} ${FLAGS} -c $<
MainSheet.o : ../HiDEM/SheetMeshCollision/MainSheet.cpp Global.h
	${COMP} ${FLAGS} -c $<
MainRegen.o : ../HiDEM/LinkRegen/MainRegen.cpp Global.h
	${COMP} ${FLAGS} -c $<


clean:
	rm *.o
