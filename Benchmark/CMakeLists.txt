add_executable(Bench BenchMain.cpp)

target_link_libraries(Bench PUBLIC Iterators)
target_link_libraries(Bench PUBLIC PLiB)
target_link_libraries(Bench PUBLIC MPI::MPI_CXX)

if(ENABLE_OMP)
    target_link_libraries(PLiB PUBLIC OpenMP::OpenMP_CXX)
endif()

target_include_directories(Bench PUBLIC ${HDF5_INCLUDE_DIRS})
target_link_libraries(Bench PUBLIC ${HDF5_LIBRARIES})
target_link_libraries(Bench PUBLIC ${HDF5_HL_LIBRARIES})

if (ENABLE_CUDA)
    set_source_files_properties(
            BenchMain.cpp
            PROPERTIES LANGUAGE CUDA)

    target_compile_definitions(Bench PUBLIC GPU)
    target_compile_definitions(Bench PUBLIC ENABLE_CUDA)
    target_link_libraries(Bench PRIVATE ${CUDA_LIBRARIES})
    target_include_directories(Bench PRIVATE ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --extended-lambda --expt-relaxed-constexpr -std=c++20" )

    if(ENABLE_GDR)
        target_compile_definitions(Bench PUBLIC GPU_AWARE_MPI)
    endif()
elseif (ENABLE_HIP)
    ##


    target_compile_definitions(Bench PUBLIC GPU)
    target_compile_definitions(Bench PUBLIC ENABLE_HIP)

    set(GPU_TARGETS "gfx90a" CACHE STRING "GPU targets to compile for")
    find_package(hip REQUIRED)
    target_link_libraries(Bench PUBLIC hip::host)

    #set_target_properties(hip::host PROPERTIES INTERFACE_COMPILE_DEFINITIONS "__HIP_PLATFORM_HCC__=1")

    find_package(rocprim REQUIRED CONFIG PATHS "/opt/rocm/rocprim")
    find_package(hipcub REQUIRED CONFIG PATHS "/opt/rocm/hipcub")

    # On ROCm: includes hipCUB headers and roc::rocprim_hip target
    # On CUDA: includes only hipCUB headers, user has to include CUB directory
    target_link_libraries(Bench PUBLIC hip::hipcub)
    target_include_directories(Bench PRIVATE "/opt/rocm/hip/include/hip/")


endif()