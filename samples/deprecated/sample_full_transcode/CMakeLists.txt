include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/deprecated/sample_full_transcode/include
  ${CMAKE_SOURCE_DIR}/deprecated/sample_spl_mux/api
)

list(APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/av_alloc_request.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/circular_splitter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/cmdline_parser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/fileio.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/muxer_wrapper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/option_handler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline_factory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline_profile.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/pipeline_factory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/session_storage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/splitter_wrapper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/track_profile.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/transform_adec.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/transform_aenc.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/transform_plugin.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/transform_vdec.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/transform_venc.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/trace_levels.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/transform_vpp.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/video_session.cpp )

list( APPEND LIBS_VARIANT sample_common )
list( APPEND LIBS_NOVARIANT sample_spl_mux_dispatcher )

set(DEPENDENCIES libmfx dl pthread)
make_executable( shortname universal )
