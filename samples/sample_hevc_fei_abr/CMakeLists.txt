find_path( FEI_INCLUDE mfxfeihevc.h PATHS ${MFX_INCLUDE} )
find_path( VA_HEVC_FEI va/va_fei_hevc.h PATHS ${PKG_LIBVA_INCLUDEDIR} )

if( (FEI_INCLUDE MATCHES NOTFOUND)
 OR (VA_HEVC_FEI MATCHES NOTFOUND)
 OR (API_VERSION VERSION_LESS "1.27"))
  message (STATUS "Pre-conditions missed, the following will not be built: sample_hevc_fei_abr.")
  return()
endif()

add_executable(sample_hevc_fei_abr)
set_property(TARGET sample_hevc_fei_abr PROPERTY FOLDER "samples")

target_include_directories(sample_hevc_fei_abr
  PRIVATE 
    include
    include/dso
  )
target_link_libraries(sample_hevc_fei_abr PRIVATE sample_common)

if(CMAKE_SYSTEM_NAME MATCHES Linux)
  target_compile_definitions(sample_hevc_fei_abr PRIVATE LIBVA_DRM_SUPPORT LIBVA_SUPPORT)
endif()

target_sources(sample_hevc_fei_abr
  PRIVATE
    include/dso/bs_def.h
    include/dso/bs_mem+.h
    include/dso/bs_parser.h
    include/dso/bs_parser++.h
    include/dso/bs_reader2.h
    include/dso/bs_reader.h
    include/dso/bs_thread.h
    include/dso/common_cabac.h
    include/dso/hevc2_parser.h
    include/dso/hevc2_struct.h
    include/dso/hevc2_trace.h
    include/dso/hevc_cabac.h
    include/dso/hevc_struct.h
    
    include/abr_brc.h
    include/buffer_pool.h
    include/cmd_processor.h
    include/fei_buffer_allocator.h
    include/fei_utils.h
    include/fei_worker.h
    include/hevc_fei_encode.h
    include/hevc_sw_dso.h
    include/pipeline_hevc_fei.h
    include/sample_hevc_fei_defs.h
    include/task.h
    
    src/dso/bs_reader2.cpp
    src/dso/bs_reader.cpp
    src/dso/bs_thread.cpp
    src/dso/common_cabac.cpp
    src/dso/dll_main.cpp
    src/dso/hevc2_cabac.cpp
    src/dso/hevc2_dec.cpp
    src/dso/hevc2_headers.cpp
    src/dso/hevc2_parser.cpp
    src/dso/hevc2_ssdata.cpp
    src/dso/hevc2_trace.cpp
    src/dso/hevc_cabac.cpp
    src/dso/hevc_cabac_tables.cpp
    src/dso/hevc_sdec_ctx.cpp
    
    src/abr_brc.cpp
    src/cmd_processor.cpp
    src/fei_buffer_allocator.cpp
    src/fei_utils.cpp
    src/fei_worker.cpp
    src/hevc_fei_encode.cpp
    src/hevc_sw_dso.cpp
    src/pipeline_hevc_fei.cpp
    src/sample_hevc_fei_abr.cpp
  )

install( TARGETS sample_hevc_fei_abr RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )
