add_library(h264_spl STATIC)

target_sources(h264_spl
  PRIVATE
    src/umc_byte_iterator.cpp
    src/umc_h264_au_stream.cpp
    src/umc_h264_nalu_stream.cpp
    src/umc_h264_spl.cpp
    src/umc_h264_syntax_reader.cpp
    include/umc_byte_iterator.h
    include/umc_h264_au_stream.h
    include/umc_h264_nalu_stream.h
    include/umc_h264_parse_def.h
    include/umc_h264_spl.h
    include/umc_h264_syntax_reader.h
  )

target_include_directories(h264_spl
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(h264_spl PUBLIC umc mfx_static_lib color_space_converter bitrate_control)
set_property(TARGET h264_spl PROPERTY FOLDER "umc")
