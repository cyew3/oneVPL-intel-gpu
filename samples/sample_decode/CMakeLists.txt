add_executable(sample_decode)
set_property(TARGET sample_decode PROPERTY FOLDER "samples")

target_include_directories(sample_decode PRIVATE include)
target_link_libraries(sample_decode PRIVATE sample_common)

target_sources(sample_decode
  PRIVATE
    include/pipeline_decode.h

    src/pipeline_decode.cpp
    src/sample_decode.cpp
  )

install( TARGETS sample_decode RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )
