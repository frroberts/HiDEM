
add_library(Iterators INTERFACE )
target_sources(Iterators INTERFACE
        DevCall.h
        ManVec.h
        Allocator.h
        ScratchMem.h
        ParallelReduce.h
        ExecutionPolicy.h
        Iterator.h
        ParallelScan.h
        ParallelSort.h
        SIMDHelper.h
        DevPortabilityLayer.h
        ParallelSelect.h
        )

#set_target_properties(Iterators PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(Iterators INTERFACE .)

if (ENABLE_CATCH)

    set(ITER_TEST_FILES
            Tests/IterTestMain.cpp
            Tests/ManVecTester.cpp
            Tests/ReducerTests.cpp
            Tests/SortTester.cpp
            Tests/BaseIteratorTests.cpp)
    add_executable(IterHostTests 
        ${ITER_TEST_FILES})
    catch_discover_tests(IterHostTests)
    target_link_libraries(IterHostTests PUBLIC Iterators)
    target_link_libraries(IterHostTests PUBLIC OpenMP::OpenMP_CXX)

    if (ENABLE_CUDA)
        add_executable(IterDevTests
                ${ITER_TEST_FILES})
        catch_discover_tests(IterDevTests)


        target_link_libraries(IterDevTests PUBLIC Iterators)
        target_link_libraries(IterDevTests PUBLIC OpenMP::OpenMP_CXX)

        set_source_files_properties(
                ${ITER_TEST_FILES}
                PROPERTIES LANGUAGE CUDA)

        target_link_libraries(IterDevTests PRIVATE ${CUDA_LIBRARIES})
        target_include_directories(IterDevTests PRIVATE ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
    endif ()
endif ()