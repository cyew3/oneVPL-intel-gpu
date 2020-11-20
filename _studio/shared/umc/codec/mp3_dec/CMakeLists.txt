if(OPEN_SOURCE)
  return()
endif()

add_library(mp3_dec)

target_include_directories(mp3_dec
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(mp3_dec
  PRIVATE
    src/mp3dec_api_fp.c
    src/mp3dec_huftabs_fp.c
    src/mp3dec_layer1_fp.c
    src/mp3dec_layer2_fp.c
    src/mp3dec_layer3_fp.c
    src/umc_mp3_decoder.cpp
    include/mp3dec_own_fp.h
    include/umc_mp3_decoder.h
)

target_link_libraries(mp3_dec
  PUBLIC
    mfx_static_lib
    mp3_common
)

set_property(TARGET mp3_dec PROPERTY FOLDER "umc")
