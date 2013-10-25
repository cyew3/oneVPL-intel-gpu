if (__ARCH MATCHES ia32)
  set( FFmpeg_arch ${__ARCH})
else( )
  set( FFmpeg_arch em64t )
endif( )

set( FFmpeg_root $ENV{INTELMEDIASDK_FFMPEG_ROOT} )

set( FFmpeg_root ${FFmpeg_root}/linux/${FFmpeg_arch} )

find_path( FFMPEG_INCLUDE libavformat/avformat.h PATHS ${FFmpeg_root}/include )
find_library( FFMPEG_LIBRARY libavformat.so PATHS ${FFmpeg_root}/lib )

if(NOT FFMPEG_INCLUDE MATCHES NOTFOUND)
  if(NOT FFMPEG_LIBRARY MATCHES NOTFOUND)
    set( FFMPEG_FOUND TRUE )
    include_directories( ${FFmpeg_root}/include )

    get_filename_component( FFMPEG_LIBRARY_PATH ${FFMPEG_LIBRARY} PATH )
    link_directories( ${FFMPEG_LIBRARY_PATH} )
  endif( )
endif( )

if(NOT DEFINED FFMPEG_FOUND)
  message( STATUS "FFmpeg was not found. sample_spl_mux is skipped. Set/check INTELMEDIASDK_FFMPEG_ROOT environment variable." )
endif( )

