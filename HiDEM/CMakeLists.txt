set(HiDEMType "Custom" CACHE STRING "Set what flavour of HiDEM to build")


set(Main_IceBlock "IceBlock/MainIceBlock.cpp")
set(Main_GlacierDem "GlacierDem/MainDem.cpp")
set(Main_GlacierDemExtData "GlacierDemExtData/MainDemExtData.cpp")
set(Main_GlacierDemExtraIter "GlacierDemExtraIter/MainDemExtraIter.cpp")
set(Main_SheetMesh "SheetMeshCollision/MainSheet.cpp")
set(Main_Scratch "Scratch/HiDEMMain.cpp")
set(Main_MeshBlock "MeshBlock/MainMeshBlock.cpp")
set(Main_Custom "MainCustom/MainCustom.cpp")

message("Key ${avar} is mapped into: ${mymap_${avar}}")

add_executable(HiDEM ${Main_${HiDEMType}})

if (ENABLE_CUDA)
    set_source_files_properties(${Main_${HiDEMType}}
            Scratch/HiDEMMain.cpp
            PROPERTIES LANGUAGE CUDA)

elseif (ENABLE_HIP)
    set_source_files_properties(${Main_${HiDEMType}}
            Scratch/HiDEMMain.cpp
            PROPERTIES LANGUAGE HIP)
endif()

target_link_libraries(HiDEM PUBLIC HiDEMLib)
target_include_directories(HiDEM PUBLIC ../HiDEMLib)

install(TARGETS HiDEM DESTINATION bin)

#set_target_properties(HiDEM PROPERTIES OUTPUT_NAME HiDEM_${HiDEMType})


## this is just here for clion
add_executable(HiDEM_Scratch Scratch/HiDEMMain.cpp)
target_link_libraries(HiDEM_Scratch PUBLIC HiDEMLib)
target_include_directories(HiDEM_Scratch PUBLIC ../HiDEMLib)