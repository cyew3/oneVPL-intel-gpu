
# =============================================================================

add_library(h264_common STATIC
  h264_common/fei_common.h
  h264_common/fei_common.cpp
  )

target_include_directories(h264_common
  PUBLIC
    include
    h264_common
  )

target_compile_definitions(h264_common
  PRIVATE
    MFX_VA
  )

target_link_libraries(h264_common
  PUBLIC
    mfx_static_lib
    umc_va_hw
    encode_hw
  PRIVATE
    mfx_sdl_properties
  )

# =============================================================================

add_library(h264_la STATIC
  h264_la/mfx_h264_la.h
  h264_la/mfx_h264_la.cpp
  )

target_include_directories(h264_la PUBLIC h264_la)

target_compile_definitions(h264_la
  PRIVATE
    AS_H264LA_PLUGIN
    MFX_VA
  )

target_link_libraries(h264_la PUBLIC h264_common)

# =============================================================================

if (MFX_ENABLE_H264_VIDEO_FEI_ENCODE)
  add_library(h264_enc STATIC
    h264_enc/mfx_h264_enc.h
    h264_enc/mfx_h264_enc.cpp
    )

  target_include_directories(h264_enc PUBLIC h264_enc)

  target_compile_definitions(h264_enc
    PRIVATE
      MFX_VA
    )

  target_link_libraries(h264_enc PUBLIC h264_common)

  # =============================================================================

  add_library(h264_preenc STATIC
    h264_preenc/mfx_h264_preenc.h
    h264_preenc/mfx_h264_preenc.cpp
  )

  target_include_directories(h264_preenc PUBLIC h264_preenc)

  target_compile_definitions(h264_preenc
    PRIVATE
      MFX_VA
    )

  target_link_libraries(h264_preenc PUBLIC h264_common)

  # =============================================================================

  add_library(h264_pak STATIC
    h264_pak/mfx_h264_fei_pak.h
    h264_pak/mfx_h264_fei_pak.cpp
  )

  target_include_directories(h264_pak PUBLIC h264_pak)

  target_compile_definitions(h264_pak
    PRIVATE
      MFX_VA
    )

  target_link_libraries(h264_pak PUBLIC h264_common)
endif()


include(sources_ext.cmake OPTIONAL)