
add_library(PLiB STATIC
        Domain.h
        Generators/FCCParticleGenerator.h
        Global.h
        GlobalLocalMapper.h
        LinkData.h
        Generators/LinkGenerator.h
        NeighbourGrid.cpp
        NeighbourGrid.h
        ParticleData.h
        World.h

        Geometry/Map2d.h
        Geometry/Map2dLoader.cpp
        Geometry/Map2dLoader.h
        Util/Util.h
        Util/Util.cpp
        Transport/CommunicatorIndividual.h
        Transport/Transporter.h
        Balancer/Balancer.cpp
        Logger/Logger.h
        Util/ArgParser.h
        Transport/ParticleTransporter.h
        H5IO/H5Checkpoint.h
        Geometry/InputGeometry.cpp
        Geometry/InputGeometry.h
        Geometry/MeshLoader.h
        Geometry/CompoundInput.cpp
        Geometry/CompoundInput.h
        Geometry/GeometryCollection.cpp
        Geometry/GeometryCollection.h
        Geometry/MeshAccel.h
        Util/TomlParser.h

        DomainBuilder.h
        Accel/NeighAccel.h
        Util/Layout.h
        Geometry/Map3d.h
        Geometry/Map.h
        Geometry/MapLoader.cpp
        Geometry/MapLoader.h
        ExternalForces.h
        Geometry/InputMesh.h
        Geometry/InputMesh.cpp
        Util/OwnedListGen.h
        Generators/BlockParticleGenerator.h
        Generators/ParticleGenerator.h
        Util/Unique.h
        OutputWriters/VtkParticles.h
        Generators/ElementaryParticle.h
        Util/MPIHelpers.h
        Generators/ElementrayLink.h
        Accel/CombinedAccelSmem.h
        ParticleDataHandler.h
        ParticleDataHandler.h
        LinkDataHandler.h
        LoggerV2/RangeCollection.cpp
        LoggerV2/RangeCollection.h
        Accel/CombinedAccelSIMD.h
        Transport/CommunicatorPacked.h
        Util/GPUSelect.h
)

if(ENABLE_OMP)
    target_link_libraries(PLiB PUBLIC OpenMP::OpenMP_CXX)
endif()

target_link_libraries(PLiB PUBLIC MPI::MPI_CXX)

if (ENABLE_CUDA)
    set_source_files_properties(
            NeighbourGrid.cpp
            Util/Util.cpp
            Geometry/MapLoader.cpp
            Balancer/Balancer.cpp
            Geometry/GeometryCollection.cpp
            Geometry/InputMesh.cpp
            LoggerV2/RangeCollection.cpp

            PROPERTIES LANGUAGE CUDA)

    target_link_libraries(PLiB PRIVATE ${CUDA_LIBRARIES})
    target_include_directories(PLiB PRIVATE ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --extended-lambda --expt-relaxed-constexpr -std=c++20" )
    # set_target_properties(PLiB PROPERTIES CUDA_ARCHITECTURES "80")

elseif (ENABLE_HIP)
    ##

    if(ENABLE_HIP_MANAGED)
        message("Using managed alloc")
    else ()
        message("Using device alloc")
    endif()

    set(GPU_TARGETS "gfx90a" CACHE STRING "GPU targets to compile for")
    set(CMAKE_HIP_ARCHITECTURES "gfx90a" CACHE STRING "GPU targets to compile for")
    target_link_libraries(PLiB PUBLIC hip::host)

    set_source_files_properties(
            NeighbourGrid.cpp
            Util/Util.cpp
            Geometry/MapLoader.cpp
            Balancer/Balancer.cpp
            Geometry/GeometryCollection.cpp
            Geometry/InputMesh.cpp
            LoggerV2/RangeCollection.cpp

            PROPERTIES LANGUAGE HIP)

    find_package(rocprim REQUIRED)
    find_package(hipcub REQUIRED)
    find_package(rocthrust REQUIRED)

    target_link_libraries(PLiB PUBLIC hip::hipcub)
    target_link_libraries(PLiB PUBLIC roc::rocthrust)
    target_link_libraries(PLiB PUBLIC roc::rocprim)


endif()

if (ENABLE_HDF5)
    target_include_directories(PLiB PUBLIC ${HDF5_INCLUDE_DIRS})
    target_link_libraries(PLiB PUBLIC ${HDF5_LIBRARIES})
    target_link_libraries(PLiB PUBLIC ${HDF5_HL_LIBRARIES})
endif ()

target_link_libraries(PLiB PUBLIC Iterators)
target_include_directories(PLiB INTERFACE .)

