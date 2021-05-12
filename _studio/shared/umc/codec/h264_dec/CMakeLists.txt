add_library(h264_dec STATIC)

target_sources(h264_dec
  PRIVATE
    include/umc_h264_au_splitter.h
    include/umc_h264_bitstream_headers.h
    include/umc_h264_dec.h
    include/umc_h264_dec_debug.h
    include/umc_h264_dec_defs_dec.h
    include/umc_h264_dec_defs_yuv.h
    include/umc_h264_dec_mfx.h
    include/umc_h264_frame.h
    include/umc_h264_frame_info.h
    include/umc_h264_frame_list.h
    include/umc_h264_headers.h
    include/umc_h264_heap.h
    include/umc_h264_mfx_supplier.h
    include/umc_h264_nal_spl.h
    include/umc_h264_notify.h
    include/umc_h264_segment_decoder_base.h
    include/umc_h264_segment_decoder_dxva.h
    include/umc_h264_slice_decoding.h
    include/umc_h264_task_broker.h
    include/umc_h264_task_supplier.h
    include/umc_h264_va_packer.h
    include/umc_h264_va_supplier.h

    src/umc_h264_au_splitter.cpp
    src/umc_h264_dec_bitstream_headers.cpp
    src/umc_h264_dec_debug.cpp
    src/umc_h264_dec_defs_yuv.cpp
    src/umc_h264_dec_mfx.cpp
    src/umc_h264_dec_slice_decoder_decode_pic.cpp
    src/umc_h264_frame.cpp
    src/umc_h264_frame_list.cpp
    src/umc_h264_heap.cpp
    src/umc_h264_mfx_supplier.cpp
    src/umc_h264_nal_spl.cpp
    src/umc_h264_segment_decoder_dxva.cpp
    src/umc_h264_slice_decoding.cpp
    src/umc_h264_task_broker.cpp
    src/umc_h264_task_supplier.cpp
    src/umc_h264_va_packer.cpp
    src/umc_h264_va_supplier.cpp
  )

target_include_directories(h264_dec
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${MSDK_LIB_ROOT}/optimization/h264/include
  )

target_link_libraries(h264_dec PUBLIC vm_plus PRIVATE umc_va_hw)

set_property(TARGET h264_dec PROPERTY FOLDER "umc")

