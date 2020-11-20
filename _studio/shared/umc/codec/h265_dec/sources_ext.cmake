if (OPEN_SOURCE)
    return()
endif()

# ==================================== umc_h265_sw ====================================
add_library(umc_h265_sw STATIC)

target_include_directories(umc_h265_sw
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
PRIVATE
    ${MSDK_UMC_ROOT}/io/umc_va/include
    ${MSDK_LIB_ROOT}/optimization/h264/include
    ${MSDK_LIB_ROOT}/optimization/h265/include
)

target_sources(umc_h265_sw
PRIVATE
    src/umc_h265_au_splitter.cpp
    src/umc_h265_bitstream_cabac.cpp
    src/umc_h265_bitstream.cpp
    src/umc_h265_bitstream_headers.cpp
    src/umc_h265_coding_unit.cpp
    src/umc_h265_debug.cpp
    src/umc_h265_frame_coding_data.cpp
    src/umc_h265_frame.cpp
    src/umc_h265_frame_info.cpp
    src/umc_h265_frame_list.cpp
    src/umc_h265_heap.cpp
    src/umc_h265_intrapred.cpp
    src/umc_h265_ipplevel.cpp
    src/umc_h265_mfx_supplier.cpp
    src/umc_h265_mfx_utils.cpp
    src/umc_h265_nal_spl.cpp
    src/umc_h265_prediction.cpp
    src/umc_h265_sao.cpp
    src/umc_h265_scaling_list.cpp
    src/umc_h265_segment_decoder.cpp
    src/umc_h265_segment_decoder_deblocking.cpp
    src/umc_h265_segment_decoder_dxva.cpp
    src/umc_h265_segment_decoder_mt.cpp
    src/umc_h265_sei.cpp
    src/umc_h265_slice_decoding.cpp
    src/umc_h265_tables.cpp
    src/umc_h265_task_broker.cpp
    src/umc_h265_task_supplier.cpp
    src/umc_h265_tr_quant.cpp
    src/umc_h265_va_packer_cenc.cpp
    src/umc_h265_va_packer.cpp
    src/umc_h265_va_packer_dxva.cpp
    src/umc_h265_va_packer_intel.cpp
    src/umc_h265_va_packer_ms.cpp
    src/umc_h265_va_packer_vaapi.cpp
    src/umc_h265_va_supplier.cpp
    src/umc_h265_yuv.cpp
)

target_compile_definitions(umc_h265_sw
PRIVATE
    AS_HEVCD_PLUGIN
)

target_link_libraries(umc_h265_sw PUBLIC mfx_static_lib umc vm vm_plus media_buffers)
set_property(TARGET umc_h265_sw PROPERTY FOLDER "umc")
