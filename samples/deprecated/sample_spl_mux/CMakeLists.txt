pkg_check_modules(PKG_LIBAVUTIL libavutil>=52.38.100)
pkg_check_modules(PKG_LIBAVCODEC libavcodec>=55.18.102)
pkg_check_modules(PKG_LIBAVFORMAT libavformat>=55.12.100)

if(PKG_LIBAVUTIL_FOUND AND
   PKG_LIBAVCODEC_FOUND AND
   PKG_LIBAVFORMAT_FOUND)
    set( FFMPEG_FOUND TRUE )
    message( STATUS "FFmpeg headers and libraries were found." )
endif( )

if(NOT DEFINED FFMPEG_FOUND)
  message( STATUS "FFmpeg headers and libraries were not found (optional)." )
endif( )

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/../../sample_common/include
  ${CMAKE_CURRENT_SOURCE_DIR}/api
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

list(APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/adts_muxer.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ffmpeg_mux_impl.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ffmpeg_reader_writer.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ffmpeg_splitter_impl.c
)

set(DEPENDENCIES libavutil libavcodec libavformat dl)
make_library( sample_spl_mux none shared)
