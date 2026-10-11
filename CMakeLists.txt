cmake_minimum_required(VERSION 3.16)

project(PHell LANGUAGES C CXX)
#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

include(CheckLanguage)
include(CheckCXXCompilerFlag)

set(CMAKE_CXX_STANDARD 23)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -O3 -g")# -lstdc++exp ")

option(ENABLE_CUDA "Enable CUDA" OFF)
option(ENABLE_HIP "Enable HIP" OFF)
option(ENABLE_OMP "Enable OpenMP" ON)

option(ENABLE_CATCH "Enable catch2 tests" OFF)
option(ENABLE_PSTL "Enable parallel stuff from stl" OFF)
option(ENABLE_FMATH "Enable fast math" ON)
option(ENABLE_RUN_REPRODUCE "Enable run to run reproducibility" ON)
option(ENABLE_HDF5 "Enables HDF5, disabling will break all HDF5 IO and checkpointing" ON)

option(ENABLE_ASAN "Enables address sanitizer builds" OFF)
option(ENABLE_UBSAN "Enables undefined behavior sanitizer builds" OFF)

option(ENABLE_STACKTRACE "Enable stacktraces from certain failures (requires GCC)" OFF)


## optimizations
option(ENABLE_SIMD256 "Enable AVX2 (256 bit vectors)" OFF)
option(ENABLE_SIMD512 "Enable AVX512 (512 bit vectors)" OFF)

option(ENABLE_ASYNC_COMM "Enable asynchronous communication" OFF)

option(ENABLE_GDR "Enable GPU aware MPI" OFF)
option(ENABLE_SMEM "Enable shared memory caching, only applicable for GPU builds. On by default" ON)

option(ENABLE_HIP_MANAGED "Enable managed memory for HIP" OFF)

## Layouts ?
SET(CT_LAYOUT "" CACHE STRING "Layout of CT (SOA/AOS/AOSOA)")

## Precision and sizes of values
option(SET_FP32 "Sets all but coordinates to fp32, not including CT" ON)
option(SET_CT_FP32 "Sets CT storage values to fp32" ON)
option(SET_LOCAL_ID_64 "Sets local indexes to be 64 bit" OFF)


option(ENABLE_STACKTRACE "Enable c++23 stacktrace (on supported compilers)" OFF)


if(ENABLE_SIMD)
    # message( FATAL_ERROR "SIMD disabled in current version" )
endif()

if(ENABLE_SIMD AND (ENABLE_CUDA OR ENABLE_HIP))
    message( FATAL_ERROR "SIMD and GPU cannot be enabled at the same time")
endif()

if(ENABLE_STACKTRACE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lstdc++exp")
endif()

if(ENABLE_FMATH)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")
endif()

if (ENABLE_ASAN)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")

    if (ENABLE_UBSAN)
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
    endif ()

endif ()

if (ENABLE_CATCH)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/External/Catch2/contrib/")
    #add_subdirectory(Include/Catch2/)
    include(CTest)
    include(Catch)
    include(ParseAndAddCatchTests)
endif ()

if (ENABLE_CUDA)
    check_language(CUDA REQUIRED)
    set(CMAKE_CUDA_ARCHITECTURES "75" "86" "80")
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --extended-lambda --expt-relaxed-constexpr -std=c++20 -lineinfo -g -allow-unsupported-compiler -lcurand" )

    enable_language(CUDA)

    find_package(CUDA REQUIRED)

endif ()

if (ENABLE_HIP)
    check_language(HIP REQUIRED)
    enable_language(HIP)
    set(CMAKE_HIP_FLAGS "${CMAKE_HIP_FLAGS} -g -O3 --offload-arch=gfx90a -Wno-unused-result")
    set(GPU_TARGETS "gfx90a" CACHE STRING "GPU targets to compile for")
    set(CMAKE_HIP_ARCHITECTURES "gfx90a" CACHE STRING "GPU targets to compile for")

endif ()

if (ENABLE_GDR)
    # if this is a cray system
    # and CPE is a POS
    #if(DEFINED ENV{CRAYPE_VERSION})
    #    message("Cray PE detected force linking GTL")
    #    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CRAY_XPMEM_POST_LINK_OPTS} –lxpmem -L${MPICH_DIR}/lib -lmpi –L$(GTL_ROOT) -lmpi_gtl_hsa")
    #    if (ENABLE_HIP)
    #        set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} ${CRAY_XPMEM_INCLUDE_OPTS} -I${MPICH_DIR}/include")
    #    endif ()
    #    if (ENABLE_CUDA)
    #        set(CMAKE_HIP_FLAGS "${CMAKE_HIP_FLAGS} ${CRAY_XPMEM_INCLUDE_OPTS} -I${MPICH_DIR}/include")
    #    endif ()
    #endif ()
endif ()

if(ENABLE_OMP)
    find_package(OpenMP)
    if (OpenMP_CXX_FOUND)
        # disable if not found, or just error out ?
    endif ()
endif()

find_package(MPI REQUIRED COMPONENTS C)
if (MPI_FOUND)
    include_directories(SYSTEM ${MPI_INCLUDE_PATH})
endif ()

if (ENABLE_HDF5)
    find_package(HDF5 COMPONENTS C)
    # if we dont find it, try setting it from the root path
    #if(DEFINED ENV{CRAYPE_VERSION})
    #    set(HDF5_INCLUDE_DIRS "${HDF5_ROOT}/include")
    #    set(HDF5_LIBRARIES "lhdf5")
    #    link_directories("${HDF5_ROOT}/lib")
    #endif()

endif()

add_definitions(-DFMT_HEADER_ONLY)

set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY true)

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}"
           CACHE PATH "default install path" FORCE)
endif()

include_directories(External/fmt/include/)
include_directories(External/tomlplusplus/include/)
include_directories(External/Catch2/single_include/)
include_directories(External/Vectorclass/)
include_directories(External/toml11/)

configure_file(${PROJECT_SOURCE_DIR}/PLiB/Global.h.in ${PROJECT_SOURCE_DIR}/PLiB/Global.h)

add_subdirectory(Iterators/)
add_subdirectory(PLiB/)
add_subdirectory(HiDEMLib/)
add_subdirectory(HiDEM/)
add_subdirectory(CatchTests/)
add_subdirectory(Benchmark/)

