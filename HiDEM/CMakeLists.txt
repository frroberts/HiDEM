set(HiDEMType "Custom" CACHE STRING "Set what flavour of HiDEM to build")


set(Main_IceBlock "IceBlock/MainIceBlock.cpp")
set(Main_GlacierDem "GlacierDem/MainDem.cpp")
set(Main_GlacierDemExtData "GlacierDemExtData/MainDemExtData.cpp")
set(Main_GlacierDemExtraIter "GlacierDemExtraIter/MainDemExtraIter.cpp")
set(Main_SheetMesh "SheetMeshCollision/MainSheet.cpp")
set(Main_SheetInit "SheetInitVel/MainSheetInit.cpp")
set(Main_MeshBlock "MeshBlock/MainMeshBlock.cpp")
set(Main_Custom "MainCustom/MainCustom.cpp")
set(Main_LinkRegen "LinkRegen/MainRegen.cpp")
set(Main_MeshDem "MeshConstrainedDem/MainMeshDem.cpp")

add_executable(HiDEM ${Main_${HiDEMType}})

if (ENABLE_CUDA)
    set_source_files_properties(${Main_${HiDEMType}}
            PROPERTIES LANGUAGE CUDA)

elseif (ENABLE_HIP)
    set_source_files_properties(${Main_${HiDEMType}}
            PROPERTIES LANGUAGE HIP)
endif()

target_link_libraries(HiDEM PUBLIC HiDEMLib)
target_include_directories(HiDEM PUBLIC ../HiDEMLib)

install(TARGETS HiDEM DESTINATION bin)

#set_target_properties(HiDEM PROPERTIES OUTPUT_NAME HiDEM_${HiDEMType})
