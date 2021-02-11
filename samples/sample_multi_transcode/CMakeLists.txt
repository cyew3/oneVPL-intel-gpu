add_executable(sample_multi_transcode)
set_property(TARGET sample_multi_transcode PROPERTY FOLDER "samples")

target_include_directories(sample_multi_transcode
  PRIVATE
    include
    ${MSDK_SAMPLES_ROOT}/sample_misc/wayland/include
    ${MSDK_SAMPLES_ROOT}/sample_plugins/rotate_cpu/include
    ${MSDK_SAMPLES_ROOT}/sample_plugins/vpp_plugin/include
  )

target_link_libraries(sample_multi_transcode
    PRIVATE
    sample_common
    $<$<NOT:$<BOOL:${API_USE_VPL}>>:vpp_plugin>
  )

target_sources(sample_multi_transcode
  PRIVATE
    include/pipeline_transcode.h
    include/sample_multi_transcode.h
    include/transcode_utils.h

    src/pipeline_transcode.cpp
    src/sample_multi_transcode.cpp
    src/transcode_utils.cpp
  )

install( TARGETS sample_multi_transcode RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )
