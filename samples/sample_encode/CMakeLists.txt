add_executable(sample_encode)
set_property(TARGET sample_encode PROPERTY FOLDER "samples")

target_include_directories(sample_encode
  PRIVATE
    include
    ${MSDK_SAMPLES_ROOT}/sample_plugins/rotate_cpu/include
  )

target_link_libraries(sample_encode PRIVATE MFX::mfx sample_common)

target_sources(sample_encode
  PRIVATE
    include/pipeline_encode.h
    include/pipeline_region_encode.h
    include/pipeline_user.h

    src/pipeline_encode.cpp
    src/pipeline_region_encode.cpp
    src/pipeline_user.cpp
    src/sample_encode.cpp
  )

install( TARGETS sample_encode RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )
