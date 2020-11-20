add_library(jpeg_enc STATIC)

target_include_directories(jpeg_enc
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(jpeg_enc
  PRIVATE
    include/enchtbl.h
    include/encqtbl.h
    include/jpegenc.h
    include/umc_mjpeg_video_encoder.h
    src/enchtbl.cpp
    src/encqtbl.cpp
    src/jpegenc.cpp
    src/jpegencrst.cpp
    src/umc_mjpeg_video_encoder.cpp
)

target_link_libraries(jpeg_enc PUBLIC mfx_static_lib jpeg_common)

set_property(TARGET jpeg_enc PROPERTY FOLDER "umc")
