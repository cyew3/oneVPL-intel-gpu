include( ${CMAKE_HOME_DIRECTORY}/../builder/FindFFmpeg.cmake )

if(FFMPEG_FOUND)

include_directories(
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_spl_mux/api
  ${CMAKE_SOURCE_DIR}/sample_spl_mux/include
)

list(APPEND sources
  ${CMAKE_SOURCE_DIR}/sample_spl_mux/src/adts_muxer.c
  ${CMAKE_SOURCE_DIR}/sample_spl_mux/src/ffmpeg_mux_impl.c
  ${CMAKE_SOURCE_DIR}/sample_spl_mux/src/ffmpeg_reader_writer.c
  ${CMAKE_SOURCE_DIR}/sample_spl_mux/src/ffmpeg_splitter_impl.c
)

set( LIBS "" )
list( APPEND LIBS
  dl
  avutil
  avcodec
  avformat
)

make_library( sample_spl_mux none shared)

append_property( sample_spl_mux COMPILE_FLAGS "${PKG_LIBAVUTIL} ${PKG_LIBAVCODEC} ${PKG_LIBAVFORMAT}" )
foreach(libpath ${PKG_LIBAVUTIL_LIBRARY_DIRS} ${PKG_LIBAVCODEC_LIBRARY_DIRS} ${PKG_LIBAVFORMAT_LIBRARY_DIRS})
  append_property( sample_spl_mux LINK_FLAGS "-L${libpath}" )
endforeach()

endif ( )
