function(setLayouts DEFAULT_LAYOUT)
    if (CT_LAYOUT STREQUAL "")
        target_compile_definitions(HiDEMLib PUBLIC ${DEFAULT_LAYOUT})
        message("Setting layout to " ${DEFAULT_LAYOUT})
    elseif (CT_LAYOUT STREQUAL "SOA")
        target_compile_definitions(HiDEMLib PUBLIC SOA_CT)
        message("Setting layout to SOA")
    elseif (CT_LAYOUT STREQUAL "AOS")
        target_compile_definitions(HiDEMLib PUBLIC AOS_CT)
        message("Setting layout to AOS")
    elseif (CT_LAYOUT STREQUAL "AOSOA")
        target_compile_definitions(HiDEMLib PUBLIC AOSOA_CT)
        message("Setting layout to AOSOA")
    endif ()
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
)

if (ENABLE_CUDA)
    set_source_files_properties(
            HiDEMLinkData.h
            HiDEMGeometry.cpp

            PROPERTIES LANGUAGE CUDA)

    target_link_libraries(HiDEMLib PRIVATE ${CUDA_LIBRARIES})
    target_include_directories(HiDEMLib PRIVATE ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})

    setLayouts("AOSOA_CT")

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

target_link_libraries(HiDEMLib PUBLIC Iterators)
target_link_libraries(HiDEMLib PUBLIC PLiB)
target_link_libraries(HiDEMLib PUBLIC MPI::MPI_C)

target_include_directories(HiDEMLib INTERFACE .)

