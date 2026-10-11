add_executable(SPH SPH/SPHMain.cpp ${BASE_FILES} SPH/SPHParticleData.h)


target_include_directories(SPH PUBLIC ${HDF5_INCLUDE_DIRS})
target_link_libraries(SPH PUBLIC ${HDF5_LIBRARIES})
target_link_libraries(SPH PUBLIC ${HDF5_HL_LIBRARIES})

target_link_libraries(SPH PUBLIC MPI::MPI_CXX)


if (ENABLE_CUDA)

    # set all files to pass through nvcc
    set_source_files_properties(SPH/SPHMain.cpp PROPERTIES LANGUAGE CUDA)

    target_link_libraries(SPH PRIVATE ${CUDA_LIBRARIES})
    target_include_directories(SPH PRIVATE ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
    target_compile_definitions(SPH PUBLIC GPU)

    # need extended lambda support to be able to pass lambdas to interation functions
elseif (ENABLE_HIP)

else ()

endif ()
