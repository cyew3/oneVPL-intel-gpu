add_library(vc1_enc STATIC)

target_include_directories(vc1_enc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_sources(vc1_enc
  PRIVATE
    include/umc_vc1_enc_adv.h
    include/umc_vc1_enc_bitplane.h
    include/umc_vc1_enc_bit_rate_control.h
    include/umc_vc1_enc_bitstream.h
    include/umc_vc1_enc_block.h
    include/umc_vc1_enc_block_template.h
    include/umc_vc1_enc_brc_gop.h
    include/umc_vc1_enc_common.h
    include/umc_vc1_enc_deblocking.h
    include/umc_vc1_enc_debug.h
    include/umc_vc1_enc_def.h
    include/umc_vc1_enc_hrd.h
    include/umc_vc1_enc_mb.h
    include/umc_vc1_enc_mb_template.h
    include/umc_vc1_enc_mode_decision.h
    include/umc_vc1_enc_picture_adv.h
    include/umc_vc1_enc_picture_sm.h
    include/umc_vc1_enc_planes.h
    include/umc_vc1_enc_pred.h
    include/umc_vc1_enc_sequence_adv.h
    include/umc_vc1_enc_sequence_sm.h
    include/umc_vc1_enc_sm.h
    include/umc_vc1_enc_smoothing_adv.h
    include/umc_vc1_enc_smoothing_sm.h
    include/umc_vc1_enc_statistic.h
    include/umc_vc1_enc_tables.h
    include/umc_vc1_video_encoder.h
    src/umc_vc1_enc_adv.cpp
    src/umc_vc1_enc_bitplane.cpp
    src/umc_vc1_enc_bit_rate_control.cpp
    src/umc_vc1_enc_bitstream.cpp
    src/umc_vc1_enc_block.cpp
    src/umc_vc1_enc_brc_gop.cpp
    src/umc_vc1_enc_common.cpp
    src/umc_vc1_enc_deblocking.cpp
    src/umc_vc1_enc_debug.cpp
    src/umc_vc1_enc_hrd.cpp
    src/umc_vc1_enc_mb.cpp
    src/umc_vc1_enc_mode_decision.cpp
    src/umc_vc1_encoder_smoothing.cpp
    src/umc_vc1_enc_picture_adv.cpp
    src/umc_vc1_enc_picture_sm.cpp
    src/umc_vc1_enc_planes.cpp
    src/umc_vc1_enc_pred.cpp
    src/umc_vc1_enc_sequence_adv.cpp
    src/umc_vc1_enc_sequence_sm.cpp
    src/umc_vc1_enc_sm.cpp
    src/umc_vc1_enc_statistic.cpp
    src/umc_vc1_enc_tables.cpp
    src/umc_vc1_video_encoder.cpp
  )

target_link_libraries(vc1_enc PUBLIC umc mfx_static_lib)
set_property(TARGET vc1_enc PROPERTY FOLDER "umc")
