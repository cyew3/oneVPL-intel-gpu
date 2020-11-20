find_path( FEI_INCLUDE mfxfeihevc.h PATHS ${MFX_INCLUDE} )
find_path( VA_HEVC_FEI va/va_fei_hevc.h PATHS ${PKG_LIBVA_INCLUDEDIR} )

if( (FEI_INCLUDE MATCHES NOTFOUND)
 OR (VA_HEVC_FEI MATCHES NOTFOUND)
 OR (API_VERSION VERSION_LESS "1.27"))
  message (STATUS "Pre-conditions missed, the following will not be built: sample_hevc_fei.")
  return()
endif()

add_executable(sample_hevc_fei)
set_property(TARGET sample_hevc_fei PROPERTY FOLDER "samples")

target_include_directories(sample_hevc_fei PRIVATE include)
target_link_libraries(sample_hevc_fei PRIVATE MFX::mfx sample_common)

if(CMAKE_SYSTEM_NAME MATCHES Linux)
  target_compile_definitions(sample_hevc_fei PRIVATE LIBVA_DRM_SUPPORT LIBVA_SUPPORT)
endif()

target_sources(sample_hevc_fei
  PRIVATE
    include/fei_buffer_allocator.h
    include/fei_predictors_repacking.h
    include/fei_utils.h
    include/hevc_fei_encode.h
    include/hevc_fei_preenc.h
    include/pipeline_hevc_fei.h
    include/ref_list_manager.h
    include/sample_hevc_fei_defs.h

    src/fei_buffer_allocator.cpp
    src/fei_predictors_repaking.cpp
    src/fei_utils.cpp
    src/hevc_fei_encode.cpp
    src/hevc_fei_preenc.cpp
    src/pipeline_hevc_fei.cpp
    src/ref_list_manager.cpp
    src/sample_hevc_fei.cpp
  )

install( TARGETS sample_hevc_fei RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )
