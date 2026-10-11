function(setLayouts TARGET_LAYOUT)
    target_compile_definitions(HiDEMLib PUBLIC ${TARGET_LAYOUT})
    message("Setting layout to " ${TARGET_LAYOUT})
endfunction()


add_library(HiDEMLib STATIC
        HiDEMParticleData.h
        HiDEMLinkData.h
        HiDEMDistribution.h
        HiDEMGlobal.h
        HiDEMH5Checkpoint.h
        ../PLiB/Accel/CombinedAccelSmem.h
        ../PLiB/Accel/LinkAccelSide.h
        ../PLiB/Accel/CombinedAccel.h
        ../PLiB/Accel/CombinedAccelSIMD.h
        HiDEMLib.h
        ../PLiB/Accel/CombinedAccelSmem.h
        HiDEMGeometry.cpp
        HiDEMGeometry.h
        ../HiDEM/Common/HiDEMDefaultFunctions.h
)
# setLayouts("COMPRESSED_BLOCK_CT")

if (ENABLE_CUDA)
    set_source_files_properties(
            HiDEMLinkData.h
            HiDEMGeometry.cpp

            PROPERTIES LANGUAGE CUDA)
    find_package(CUDAToolkit REQUIRED)

    target_link_libraries(HiDEMLib PRIVATE
            CUDA::cudart
            CUDA::curand
    )
    target_include_directories(HiDEMLib PRIVATE ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})

    target_compile_definitions(HiDEMLib PUBLIC NV${GPU_ARCH})


    setLayouts("BLOCK_CT")

elseif (ENABLE_HIP)

    set_source_files_properties(
            HiDEMLinkData.h
            HiDEMGeometry.cpp

            PROPERTIES LANGUAGE HIP)

    setLayouts("AOSOA_CT")

    set(GPU_TARGETS "gfx90a" CACHE STRING "GPU targets to compile for")
    set(CMAKE_HIP_ARCHITECTURES "gfx90a" CACHE STRING "GPU targets to compile for")

    find_package(hip REQUIRED)
    target_link_libraries(HiDEMLib PUBLIC hip::host)

endif()

# options
if(ENABLE_SIMD256 OR ENABLE_SIMD512)
    setLayouts("AOS_CT")
endif()


if(ENABLE_PSTL)
    # todo fix a better tbb detection if using gcc
    # is this still needed with modern compilers ?
    # target_link_libraries(HiDEMLib PUBLIC tbb)
endif()

if(ENABLE_OMP)
    target_link_libraries(HiDEMLib PUBLIC OpenMP::OpenMP_CXX)
endif()

if(ENABLE_UNWIND)
    target_link_libraries(HiDEMLib PUBLIC unwind unwind-x86_64)
endif()



target_link_libraries(HiDEMLib PUBLIC Iterators)
target_link_libraries(HiDEMLib PUBLIC PLiB)
target_link_libraries(HiDEMLib PUBLIC MPI::MPI_C)

target_include_directories(HiDEMLib INTERFACE .)

