add_executable(sample_multi_transcode_mod)
set_property(TARGET sample_multi_transcode_mod PROPERTY FOLDER "samples")

set(smt_dir ${MSDK_SAMPLES_ROOT}/sample_multi_transcode)


target_include_directories(sample_multi_transcode_mod
  PRIVATE
    include
    ${smt_dir}/include
    ${MSDK_SAMPLES_ROOT}/sample_misc/wayland/include
    ${MSDK_SAMPLES_ROOT}/sample_plugins/rotate_cpu/include
    ${MSDK_SAMPLES_ROOT}/sample_plugins/vpp_plugin/include
  )


target_compile_definitions(sample_multi_transcode_mod PUBLIC MOD_SMT)

target_link_libraries(sample_multi_transcode_mod
    PRIVATE
    MFX::mfx
    sample_common
    vpp_plugin
    )

target_sources(sample_multi_transcode_mod
  PRIVATE
    ${smt_dir}/include/pipeline_transcode.h
    ${smt_dir}/include/sample_multi_transcode.h
    ${smt_dir}/include/transcode_utils.h

    ${smt_dir}/src/pipeline_transcode.cpp
    ${smt_dir}/src/sample_multi_transcode.cpp
    ${smt_dir}/src/transcode_utils.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/modified_transcoding_pipeline.cpp
  )

install( TARGETS sample_multi_transcode_mod RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )
