add_library(mpeg2_dec_hw STATIC)

target_include_directories(mpeg2_dec_hw
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_sources(mpeg2_dec_hw
  PRIVATE
    src/umc_mpeg2_bitstream.cpp
    src/umc_mpeg2_decoder.cpp
    src/umc_mpeg2_decoder_va.cpp
    src/umc_mpeg2_frame.cpp
    src/umc_mpeg2_slice.cpp
    src/umc_mpeg2_splitter.cpp
    src/umc_mpeg2_utils.cpp
    src/umc_mpeg2_va_packer.cpp
    src/umc_mpeg2_dxva_packer.cpp
    include/umc_mpeg2_bitstream.h
    include/umc_mpeg2_decoder.h
    include/umc_mpeg2_decoder_va.h
    include/umc_mpeg2_defs.h
    include/umc_mpeg2_frame.h
    include/umc_mpeg2_slice.h
    include/umc_mpeg2_splitter.h
    include/umc_mpeg2_utils.h
    include/umc_mpeg2_va_packer.h
    include/umc_mpeg2_dxva_packer.h
)

target_compile_definitions(mpeg2_dec_hw PRIVATE MFX_VA)

target_link_libraries(mpeg2_dec_hw PUBLIC mfx_static_lib color_space_converter)

set_property(TARGET mpeg2_dec_hw PROPERTY FOLDER "umc")
