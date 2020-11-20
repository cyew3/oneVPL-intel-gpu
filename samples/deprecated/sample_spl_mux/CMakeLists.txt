
if (NOT FFMPEG_FOUND)
  return()
endif()

target_include_directories(sample_spl_mux
  PUBLIC 
    ${MSDK_SAMPLES_ROOT}/sample_common/include
    api
    include
)
target_include_directories(APPEND sources
  src/adts_muxer.c
  src/ffmpeg_mux_impl.c
  src/ffmpeg_reader_writer.c
  src/ffmpeg_splitter_impl.c
)


set(DEPENDENCIES libavutil libavcodec libavformat dl)

add_library(sample_spl_mux SHARED
  include/adts_muxer.h
  include/ffmpeg_mux_impl.h
  include/ffmpeg_reader_writer.h
  include/ffmpeg_splitter_impl.h
  include/inttypes.h
  include/spl_mux_defs.h`
  src/adts_muxer.c
  src/ffmpeg_mux_impl.c
  src/ffmpeg_reader_writer.c
  src/ffmpeg_splitter_impl.c
)

target_link_librariries(sample_spl_mux PRIVATE avutil avcodec avformat ${CMAKE_DL_LIBS})