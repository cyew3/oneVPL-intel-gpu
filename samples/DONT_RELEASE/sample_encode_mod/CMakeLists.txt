add_executable(sample_encode_mod)
set_property(TARGET sample_encode_mod PROPERTY FOLDER "samples")

set(sample_encode_dir ${MSDK_SAMPLES_ROOT}/sample_encode)

target_include_directories(sample_encode_mod
  PRIVATE
    include
    ${sample_encode_dir}/include
    ${MSDK_SAMPLES_ROOT}/sample_plugins/rotate_cpu/include
  )

target_compile_definitions(sample_encode_mod PUBLIC MOD_ENC)

target_link_libraries(sample_encode_mod PRIVATE MFX::mfx sample_common)

target_sources(sample_encode_mod
  PRIVATE
    ${sample_encode_dir}/include/pipeline_encode.h
    ${sample_encode_dir}/include/pipeline_region_encode.h
    ${sample_encode_dir}/include/pipeline_user.h

    ${sample_encode_dir}/src/pipeline_encode.cpp
    ${sample_encode_dir}/src/pipeline_region_encode.cpp
    ${sample_encode_dir}/src/pipeline_user.cpp
    ${sample_encode_dir}/src/sample_encode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/modified_encoding_pipeline.cpp
  )

install( TARGETS sample_encode_mod RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )
