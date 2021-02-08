
find_path( FEI_INCLUDE mfxfei.h PATHS ${MFX_INCLUDE} )

if(FEI_INCLUDE MATCHES NOTFOUND)
  message (STATUS "mfxfei.h was not found, the following will not be built: sample_fei.")
  return()
endif()

add_executable(sample_fei)
set_property(TARGET sample_fei PROPERTY FOLDER "samples")

target_include_directories(sample_fei PUBLIC include)
target_link_libraries(sample_fei PRIVATE sample_common)

if(CMAKE_SYSTEM_NAME MATCHES Linux)
  target_compile_definitions(sample_fei PRIVATE LIBVA_DRM_SUPPORT LIBVA_SUPPORT)
endif()

target_sources(sample_fei
  PRIVATE
    include/auxiliary_interfaces.h
    include/encoding_task.h
    include/encoding_task_pool.h
    include/fei_encode.h
    include/fei_encpak.h
    include/fei_preenc.h
    include/pipeline_fei.h
    include/predictors_repacking.h
    include/refListsManagement_fei.h
    include/sample_fei_defs.h
    
    src/auxiliary_interfaces.cpp
    src/fei_encode.cpp
    src/fei_encpak.cpp
    src/fei_preenc.cpp
    src/fei_streamout_pak_repacking.cpp
    src/pipeline_fei.cpp
    src/refListsManagement_fei.cpp
    src/sample_fei.cpp   
  )

install( TARGETS sample_fei RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )
