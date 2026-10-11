

set(TESTER_FILES_MPI
        CommunicatorTesterMulti.cpp
        DistributedSetupTest.cpp
        TransporterTesterMulti.cpp
        StaticDistributionTesterMulti.cpp
        HDF5ReadWriteTestMulti.cpp)



if (ENABLE_CATCH)
    add_executable(Tester testMain.cpp
            # IteratorTester.cpp
            # TestHelpers.h
            # ParticleDataTester.cpp
            # TestHelpers.cpp
            # NeighbourGridTester.cpp
            # GeomtryTester.cpp
            # ParticleGeneratorTester.cpp
            # GeometryLoaderTester.cpp
            # LinkDataTester.cpp
            # LinkGeneratorTester.cpp
            # IteratorBencher.cpp
            # CSVTester.cpp
            # DomainTest.cpp
            # CommunicatorTesterSingle.cpp
            # GPUIteratorTest.cpp
            # TransporterTester.cpp
            # LoggerTester.cpp
            # HDF5ReadWriteTest.cpp
            BalancerTester.cpp
            # MeshTester.cpp
            # TomlTest.cpp
            # GeometryBuilderTest.cpp
             ../External/fmt/src/os.cc
             ../External/fmt/src/format.cc
            # LayoutTester.cpp
            # ManVecTester.cpp
            # UnitBlockGeneratorTester.cpp
            # VTUOutTester.cpp
            # SimdTester.cpp
            # GlobalLocalMappterTest.cpp
            # HiDEMCheckpointTester.cpp
    )

    if (ENABLE_SIMD)
        target_compile_definitions(Tester PUBLIC SIMD)
        setLayouts("AOS_CT")

    endif ()

    catch_discover_tests(Tester)

    add_executable(Tester_mpi testMain.cpp ${TESTER_FILES_MPI})
    catch_discover_tests(Tester_mpi)

    target_link_libraries(Tester PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(Tester PUBLIC SOA_CT)

    target_include_directories(Tester PUBLIC ${HDF5_INCLUDE_DIRS})
    target_link_libraries(Tester PUBLIC MPI::MPI_CXX)
    target_link_libraries(Tester PUBLIC ${HDF5_LIBRARIES})
    target_link_libraries(Tester PUBLIC ${HDF5_HL_LIBRARIES})
    target_link_libraries(Tester PUBLIC PLiB)
    target_link_libraries(Tester PUBLIC Iterators)


    target_include_directories(Tester_mpi PUBLIC ${HDF5_INCLUDE_DIRS})
    target_link_libraries(Tester_mpi PUBLIC MPI::MPI_CXX)
    target_link_libraries(Tester_mpi PUBLIC ${HDF5_LIBRARIES})
    target_link_libraries(Tester_mpi PUBLIC ${HDF5_HL_LIBRARIES})
    target_link_libraries(Tester_mpi PUBLIC PLiB)
    target_link_libraries(Tester_mpi PUBLIC Iterators)

    if (ENABLE_CUDA)
        target_link_libraries(Tester PRIVATE ${CUDA_LIBRARIES})
        target_include_directories(Tester PRIVATE ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
        set_source_files_properties(
                Tests/testMain.cpp
                Tests/IteratorTester.cpp
                Tests/NeighbourGridTester.cpp
                Tests/ParticleGeneratorTester.cpp
                Tests/LinkDataTester.cpp
                Tests/LinkGeneratorTester.cpp
                Tests/CommunicatorTesterSingle.cpp
                Tests/TransporterTester.cpp
                Tests/HDF5ReadWriteTest.cpp
                BalancerTester.cpp
                GlobalLocalMappterTest.cpp
                HiDEMCheckpointTester.cpp
                PROPERTIES LANGUAGE CUDA)
    endif ()

endif ()

