include( ${CMAKE_HOME_DIRECTORY}/../builder_s/FindFFmpeg.cmake )

include_directories(
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/deprecated/sample_spl_mux/api
  ${CMAKE_SOURCE_DIR}/deprecated/sample_spl_mux/include
)

list(APPEND sources
  ${CMAKE_SOURCE_DIR}/deprecated/sample_spl_mux/src/adts_muxer.c
  ${CMAKE_SOURCE_DIR}/deprecated/sample_spl_mux/src/ffmpeg_mux_impl.c
  ${CMAKE_SOURCE_DIR}/deprecated/sample_spl_mux/src/ffmpeg_reader_writer.c
  ${CMAKE_SOURCE_DIR}/deprecated/sample_spl_mux/src/ffmpeg_splitter_impl.c
)

set(DEPENDENCIES libavutil libavcodec libavformat dl)
make_library( sample_spl_mux none shared)
