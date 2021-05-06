add_library(mpeg2_dec_sw STATIC)

target_include_directories(mpeg2_dec_sw
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(mpeg2_dec_sw
  PRIVATE
    src/umc_mpeg1_dec.cpp
    src/umc_mpeg2_dec.cpp
    src/umc_mpeg2_dec_blk.cpp
    src/umc_mpeg2_dec_defs.cpp
    src/umc_mpeg2_dec_mb.cpp
    src/umc_mpeg2_dec_mb422.cpp
    src/umc_mpeg2_dec_mc.cpp
    src/umc_mpeg2_dec_mv.cpp
    src/umc_mpeg2_dec_pic.cpp
    src/umc_mpeg2_dec_slice.cpp
    src/umc_mpeg2_dec_tbl.cpp
    include/umc_mpeg2_dec_base.h
    include/umc_mpeg2_dec_bstream.h
    include/umc_mpeg2_dec_defs.h
    include/umc_mpeg2_dec_defs_sw.h
    include/umc_mpeg2_dec_sw.h
    include/umc_mpeg2_dec_tbl.h
)

target_link_libraries(mpeg2_dec_sw
  PUBLIC
    mfx_static_lib
    color_space_converter
    media_buffers
  PRIVATE
    mfx_sdl_properties
  )

set_property(TARGET mpeg2_dec_sw PROPERTY FOLDER "umc")
