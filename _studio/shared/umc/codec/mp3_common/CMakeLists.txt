if(OPEN_SOURCE)
  return()
endif()

add_library(mp3_common)

target_include_directories(mp3_common
  PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(mp3_common
  PRIVATE
    src/mp3dec_api.c
    src/mp3dec_common.c
    src/mp3dec_layer1.c
    src/mp3dec_layer2.c
    src/mp3dec_layer3.c
    src/mp3enc_bitstream.c
    src/mp3enc_common.c
    src/mp3enc_huff.c
    src/mp3enc_hufftables.c
    src/mp3enc_quantization.c
    src/mp3enc_tables.c
    src/mp3_alloc_tab.c
    src/mp3_common.c
    include/mp3dec.h
    include/mp3dec_huftabs.h
    include/mp3dec_own.h
    include/mp3enc.h
    include/mp3enc_hufftables.h
    include/mp3enc_own.h
    include/mp3enc_psychoacoustic.h
    include/mp3enc_tables.h
    include/mp3_alloc_tab.h
    include/mp3_own.h
    include/mp3_status.h
)

target_link_libraries(mp3_common
  PUBLIC
    mfx_static_lib
    umc
    common
)

set_property(TARGET mp3_common PROPERTY FOLDER "umc")
