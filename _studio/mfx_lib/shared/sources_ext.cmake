if (MFX_DISABLE_SW_FALLBACK)
  return()
endif()

# ====================================== mfx_common_sw ======================================

add_library(mfx_common_sw STATIC)
set_property(TARGET mfx_common_sw PROPERTY FOLDER "sw_fallback")

target_sources(mfx_common_sw
  PRIVATE
    include/mfx_brc_common.h
    include/mfx_common_decode_int.h
    include/mfx_common_int.h
    include/mfx_critical_error_handler.h
    include/mfx_enc_common.h
    include/mfx_h264_enc_common.h
    include/mfx_interface.h
    include/mfx_mpeg2_dec_common.h
    include/mfx_mpeg2_enc_common.h
    include/mfx_session.h
    include/mfx_user_plugin.h
    include/mfx_vc1_dec_common.h
    include/mfx_vpx_dec_common.h

    src/mfx_brc_common.cpp
    src/mfx_common_decode_int.cpp
    src/mfx_common_int.cpp
    src/mfx_enc_common.cpp
    src/mfx_mpeg2_dec_common.cpp
    src/mfx_h264_enc_common.cpp
    src/mfx_vc1_dec_common.cpp
    src/mfx_critical_error_handler.cpp
    src/mfx_vpx_dec_common.cpp
  )

target_compile_definitions(mfx_common_sw PRIVATE MFX_NO_VA)

target_include_directories(mfx_common_sw
  PUBLIC
    include
    ${MSDK_UMC_ROOT}/codec/brc/include
    ${MSDK_UMC_ROOT}/codec/h264_enc/include
    ${MSDK_UMC_ROOT}/codec/vc1_common/include
    ${MSDK_UMC_ROOT}/codec/vc1_dec/include
)

target_link_libraries(mfx_common_sw PUBLIC mfx_static_lib umc_io umc)

# ====================================== mfx_common_hw ======================================

target_link_libraries(mfx_common_hw PUBLIC mfx_common_sw lpla)

target_compile_definitions(mfx_common_hw PRIVATE $<$<PLATFORM_ID:Windows>:MFX_DX9ON11>)
