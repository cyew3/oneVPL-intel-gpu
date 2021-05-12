
if (OPEN_SOURCE)
  return()
endif()

target_include_directories(encode_hw PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/base
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g11lkf
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g12
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g12xehp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g12dg2
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/base
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g11lkf
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g12
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g12xehp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g12dg2
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g11lkf
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12xehp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12dg2
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/agnostic
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/linux
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/linux/g12
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/g12
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/g13
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/g13_1
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/agnostic/g12
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/agnostic/g13
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/agnostic/g13_1
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/embargo
)

target_sources(encode_hw
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g12/hevcehw_g12_scc_mode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g12/hevcehw_g12_scc_mode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g11lkf/hevcehw_g11lkf_caps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g11lkf/hevcehw_g11lkf_caps.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g11lkf/hevcehw_g11lkf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g11lkf/hevcehw_g11lkf.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g12xehp/hevcehw_g12xehp_caps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g12xehp/hevcehw_g12xehp_caps.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g12dg2/hevcehw_g12dg2_caps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g12dg2/hevcehw_g12dg2_caps.h

    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g11lkf/hevcehw_g11lkf_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g12xehp/hevcehw_g12xehp_lin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g12xehp/hevcehw_g12xehp_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g12dg2/hevcehw_g12dg2_lin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g12dg2/hevcehw_g12dg2_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g12/hevcehw_g12_embargo_lin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g12/hevcehw_g12_embargo_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/base/hevcehw_base_gpu_hang_lin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/base/hevcehw_base_gpu_hang_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/base/hevcehw_base_embargo_lin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/base/hevcehw_base_embargo_lin.h

    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g11lkf/hevcehw_g11lkf_caps.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g11lkf/hevcehw_g11lkf.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/g12dg2/hevcehw_g12dg2_caps.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/base/hevcehw_base_dump_files.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/base/hevcehw_base_extddi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g11lkf/hevcehw_g11lkf_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g12xehp/hevcehw_g12xehp_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g12dg2/hevcehw_g12dg2_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/g12/hevcehw_g12_embargo_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/linux/base/hevcehw_base_gpu_hang_lin.h

    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/hevcehw_ddi_trace.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g11lkf/hevcehw_g11lkf_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12xehp/hevcehw_g12xehp_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12dg2/hevcehw_g12dg2_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12/hevcehw_g12_qp_modulation_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12/hevcehw_g12_scc_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12/hevcehw_g12_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_alloc_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_blocking_sync_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_brc_sliding_window_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_d3d11_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_d3d9_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_ddi_packer_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_dirty_rect_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_encoded_frame_info_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_extddi_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_interlace_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_max_frame_size_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_mb_per_sec_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_protected_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_qmatrix_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_roi_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_units_info_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_weighted_prediction_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_lpla_analysis.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_lpla_enc.h

    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/base/hevcehw_base_dump_files.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/agnostic/base/hevcehw_base_extddi.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/hevcehw_ddi_trace.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12/hevcehw_g12_qp_modulation_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12/hevcehw_g12_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_alloc_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_blocking_sync_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_brc_sliding_window_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_d3d11_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_d3d9_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_ddi_packer_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_dirty_rect_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_encoded_frame_info_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_extddi_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_interlace_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_max_frame_size_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_mb_per_sec_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_qmatrix_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_roi_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_units_info_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_weighted_prediction_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_win.h

    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12/hevcehw_g12_qp_modulation_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12/hevcehw_g12_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_alloc_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_blocking_sync_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_brc_sliding_window_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_d3d11_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_d3d9_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_ddi_packer_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_dirty_rect_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_extddi_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_interlace_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_max_frame_size_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_mb_per_sec_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_protected_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_qmatrix_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_roi_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_units_info_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_weighted_prediction_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_lpla_analysis.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_lpla_enc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/base/hevcehw_base_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/hevcehw_ddi_trace.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g11lkf/hevcehw_g11lkf_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12xehp/hevcehw_g12xehp_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12xehp/hevcehw_g12xehp_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12dg2/hevcehw_g12dg2_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hevc/embargo/windows/g12dg2/hevcehw_g12dg2_win.h

    ${CMAKE_CURRENT_SOURCE_DIR}/shared/embargo/ehw_device_dx9.h
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/embargo/ehw_device_dx11.h
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/embargo/ehw_utils_ddi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/embargo/ehw_platforms.h
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/embargo/ehw_resources_pool_dx11.h

    ${CMAKE_CURRENT_SOURCE_DIR}/shared/embargo/ehw_device_dx9.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/embargo/ehw_device_dx11.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/embargo/ehw_utils_ddi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/embargo/ehw_resources_pool_dx11.cpp

    $<$<PLATFORM_ID:Windows>:${MSDK_LIB_ROOT}/shared/src/mfx_win_event_cache.cpp>

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/av1ehw_disp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/av1ehw_disp.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/av1ehw_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/av1ehw_block_queues.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/av1ehw_ddi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/av1ehw_decl_blocks.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/av1ehw_utils.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/av1ehw_base.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_lin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_va_lin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_va_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_va_packer_lin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_base_va_packer_lin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/linux/base/av1ehw_va_private.h

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/agnostic/g12/av1ehw_g12.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/agnostic/g12/av1ehw_g12_segmentation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/agnostic/g12/av1ehw_g12_segmentation.h

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/agnostic/g13/av1ehw_g13.h

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/agnostic/g13_1/av1ehw_g13_1_scc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/agnostic/g13_1/av1ehw_g13_1_scc.h

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/linux/g12/av1ehw_g12_lin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/linux/g12/av1ehw_g12_lin.h

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/av1ehw_ddi_trace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/av1ehw_ddi_trace.h

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base/av1ehw_base_alloc_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base/av1ehw_base_alloc_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base/av1ehw_base_blocking_sync_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base/av1ehw_base_blocking_sync_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base/av1ehw_base_d3d11_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base/av1ehw_base_d3d11_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base/av1ehw_base_ddi_packer_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base/av1ehw_base_ddi_packer_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base/av1ehw_base_dirty_rect_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base/av1ehw_base_dirty_rect_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base/av1ehw_base_encoded_frame_info_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base/av1ehw_base_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/base/av1ehw_base_win.h

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/g12/av1ehw_g12_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/g12/av1ehw_g12_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/g13/av1ehw_g13_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/g13/av1ehw_g13_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/g13_1/av1ehw_g13_1_win.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/embargo/windows/g13_1/av1ehw_g13_1_win.h

    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_alloc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_alloc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_constraints.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_constraints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_data.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_dirty_rect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_dirty_rect.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_general.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_general.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_general_defaults.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_iddi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_iddi_packer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_packer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_packer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_query_impl_desc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_query_impl_desc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_segmentation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_segmentation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_superres.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_superres.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_task.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_tile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_tile.h
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_encoded_frame_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/av1/agnostic/base/av1ehw_base_encoded_frame_info.h
    )

target_link_libraries(encode_hw
  PUBLIC
    $<$<BOOL:${MFX_ENABLE_USER_ENCTOOLS}>:lpla>
)

target_compile_definitions(encode_hw PRIVATE $<$<PLATFORM_ID:Windows>:MFX_DX9ON11>)
