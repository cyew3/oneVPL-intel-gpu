/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#if defined (AS_VP9E_PLUGIN)

#if defined (_WIN32) || defined (_WIN64)

#include "mfx_vp9_encode_hw_d3d9.h"

namespace MfxHwVP9Encode
{
#if defined (MFX_VA_WIN)

DriverEncoder* CreatePlatformVp9Encoder()
{
    return new D3D9Encoder;
}

// temporal version of header packing
#define vp9_zero(dest) memset(&(dest), 0, sizeof(dest))
#define ALIGN_POWER_OF_TWO(value, n) \
  (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

#define MI_SIZE_LOG2 3
#define MI_BLOCK_SIZE_LOG2 (6 - MI_SIZE_LOG2)  // 64 = 2^6
#define MI_BLOCK_SIZE (1 << MI_BLOCK_SIZE_LOG2)  // mi-units per max block

static int calc_mi_size(int len) {
  // len is in mi units.
  return len + MI_BLOCK_SIZE;
}

mfxStatus InitVp9VideoParam(VP9MfxParam const &video, VP9_COMP & libvpxVP9Par)
{
    VP9_COMP *const cpi = &libvpxVP9Par;
    VP9_COMMON *const cm = &cpi->common;

    vp9_zero(*cpi);

    cm->profile                 =  PROFILE_0;
    cm->bit_depth               =  VPX_BITS_8;
    cm->color_space             =  VPX_CS_UNKNOWN;
    cm->width                   = video.mfx.FrameInfo.Width;
    cm->height                  = video.mfx.FrameInfo.Height;
    cm->render_width           = cm->width;
    cm->render_height          = cm->height;
    cm->refresh_frame_context   = 1;
    cm->reset_frame_context     = 0;
    cm->subsampling_x           = 1; // 4:2:0 support
    cm->subsampling_y           = 1;
    cpi->initial_width = cm->width;
    cpi->initial_height = cm->height;
    cpi->refresh_last_frame = cpi->refresh_golden_frame = cpi->refresh_alt_ref_frame = 0;
    cpi->refresh_frames_mask = 0;

    const int aligned_width = ALIGN_POWER_OF_TWO(cm->width, MI_SIZE_LOG2);
    const int aligned_height = ALIGN_POWER_OF_TWO(cm->height, MI_SIZE_LOG2);

    cm->mi_cols = aligned_width >> MI_SIZE_LOG2;
    cm->mi_rows = aligned_height >> MI_SIZE_LOG2;
    cm->mi_stride = calc_mi_size(cm->mi_cols);

    cm->mb_cols = (cm->mi_cols + 1) >> 1;
    cm->mb_rows = (cm->mi_rows + 1) >> 1;
    cm->MBs = cm->mb_rows * cm->mb_cols;

    cpi->common.allow_high_precision_mv = 1;  // Default mv precision

    return MFX_ERR_NONE;
}

mfxStatus SetNextFrame(sFrameParams const &frameParams, VP9_COMP & libvpxVP9Par)
{
    VP9_COMP    *const cpi = &libvpxVP9Par;
    VP9_COMMON  *const cm = &cpi->common;

    struct loopfilter *const lf = &cm->lf;
    struct segmentation *const seg = &cm->seg;

    cm->frame_type = (frameParams.bIntra) ? KEY_FRAME : INTER_FRAME;
    cm->base_qindex = frameParams.QIndex;

    cm->show_frame = 1;
    cm->intra_only = 0;

    cpi->refresh_last_frame = cpi->refresh_golden_frame = cpi->refresh_alt_ref_frame = 0;
    cpi->refresh_frames_mask = 0;
    for (mfxU8 i = 0; i < DPB_SIZE; i ++)
    {
        cpi->refresh_frames_mask |= (frameParams.refreshRefFrames[i] << i);
    }

    if (cm->frame_type == KEY_FRAME || cm->intra_only) {
        //vp9_setup_past_independence(cm);

        seg->enabled = 0;
        seg->update_map = 0;
        seg->update_data = 0;

        //vp9_clearall_segfeatures(seg);
        lf->mode_ref_delta_enabled = 0;
        lf->mode_ref_delta_update = 0;
        vp9_zero(lf->last_ref_deltas);
        vp9_zero(lf->last_mode_deltas);

        cm->error_resilient_mode = 0;
        cm->frame_parallel_decoding_mode = 0;
        if (cm->error_resilient_mode) {
            cm->frame_parallel_decoding_mode = 1;
            cm->reset_frame_context = 0;
            cm->refresh_frame_context = 0;
        } else if (cm->intra_only) {
            cm->reset_frame_context = 2;
        }

        lf->ref_deltas[INTRA_FRAME] = frameParams.LFRefDelta[INTRA_FRAME];
        lf->ref_deltas[LAST_FRAME] = frameParams.LFRefDelta[LAST_FRAME];
        lf->ref_deltas[GOLDEN_FRAME] = frameParams.LFRefDelta[GOLDEN_FRAME];
        lf->ref_deltas[ALTREF_FRAME] = frameParams.LFRefDelta[ALTREF_FRAME];

        lf->mode_deltas[0] = frameParams.LFModeDelta[0];
        lf->mode_deltas[1] = frameParams.LFModeDelta[1];;
    } else {
        cpi->lst_fb_idx = frameParams.refList[REF_LAST];
        cpi->gld_fb_idx = frameParams.refList[REF_GOLD];
        cpi->alt_fb_idx = frameParams.refList[REF_ALT];

        lf->mode_ref_delta_enabled = 0;
        lf->mode_ref_delta_update = 0;
    }

#if 0 // waiting for support in BSP
    if (frameParams.NumSegments > 1)
    {
        seg->enabled = 1;
        for (mfxU8 i = 0; i < frameParams.NumSegments; i ++)
        {
            if (frameParams.QIndexDeltaSeg[i])
            {
                vp9_enable_segfeature(seg, i, SEG_LVL_ALT_Q);
                vp9_set_segdata(seg, i, SEG_LVL_ALT_Q, frameParams.QIndexDeltaSeg[i]);
            }
            if (frameParams.LFDeltaSeg[i])
            {
                vp9_enable_segfeature(seg, i, SEG_LVL_ALT_LF);
                vp9_set_segdata(seg, i, SEG_LVL_ALT_LF, frameParams.LFDeltaSeg[i]);
            }
        }
    }
#endif

    cm->y_dc_delta_q = 0;
    cm->uv_dc_delta_q = 0;
    cm->uv_ac_delta_q = 0;

    lf->filter_level = frameParams.LFLevel;
    lf->sharpness_level = cm->frame_type == KEY_FRAME ? 0 : 0;

    return MFX_ERR_NONE;
}

struct vpx_write_bit_buffer {
  uint8_t *bit_buffer;
  size_t bit_offset;
};

// header packing from libvpx
void vpx_wb_write_bit(struct vpx_write_bit_buffer *wb, int bit) {
  const int off = (int)wb->bit_offset;
  const int p = off / CHAR_BIT;
  const int q = CHAR_BIT - 1 - off % CHAR_BIT;
  if (q == CHAR_BIT - 1) {
    wb->bit_buffer[p] = uint8_t(bit << q);
  } else {
    wb->bit_buffer[p] &= ~(1 << q);
    wb->bit_buffer[p] |= bit << q;
  }
  wb->bit_offset = off + 1;
}

void vpx_wb_write_literal(struct vpx_write_bit_buffer *wb, int data, int bits) {
  int bit;
  for (bit = bits - 1; bit >= 0; bit--) vpx_wb_write_bit(wb, (data >> bit) & 1);
}

#define VP9_FRAME_MARKER 0x2

static void write_profile(BITSTREAM_PROFILE profile,
                          struct vpx_write_bit_buffer *wb) {
  switch (profile) {
    case PROFILE_0: vpx_wb_write_literal(wb, 0, 2); break;
    case PROFILE_1: vpx_wb_write_literal(wb, 2, 2); break;
    case PROFILE_2: vpx_wb_write_literal(wb, 1, 2); break;
    case PROFILE_3: vpx_wb_write_literal(wb, 6, 3); break;
    default: assert(0);
  }
}

#define VP9_SYNC_CODE_0 0x49
#define VP9_SYNC_CODE_1 0x83
#define VP9_SYNC_CODE_2 0x42

static void write_sync_code(struct vpx_write_bit_buffer *wb) {
  vpx_wb_write_literal(wb, VP9_SYNC_CODE_0, 8);
  vpx_wb_write_literal(wb, VP9_SYNC_CODE_1, 8);
  vpx_wb_write_literal(wb, VP9_SYNC_CODE_2, 8);
}

static void write_bitdepth_colorspace_sampling(
    VP9_COMMON *const cm, struct vpx_write_bit_buffer *wb) {
  if (cm->profile >= PROFILE_2) {
    assert(cm->bit_depth > VPX_BITS_8);
    vpx_wb_write_bit(wb, cm->bit_depth == VPX_BITS_10 ? 0 : 1);
  }
  vpx_wb_write_literal(wb, cm->color_space, 3);
  if (cm->color_space != VPX_CS_SRGB) {
    // 0: [16, 235] (i.e. xvYCC), 1: [0, 255]
    vpx_wb_write_bit(wb, cm->color_range);
    if (cm->profile == PROFILE_1 || cm->profile == PROFILE_3) {
      assert(cm->subsampling_x != 1 || cm->subsampling_y != 1);
      vpx_wb_write_bit(wb, cm->subsampling_x);
      vpx_wb_write_bit(wb, cm->subsampling_y);
      vpx_wb_write_bit(wb, 0);  // unused
    } else {
      assert(cm->subsampling_x == 1 && cm->subsampling_y == 1);
    }
  } else {
    assert(cm->profile == PROFILE_1 || cm->profile == PROFILE_3);
    vpx_wb_write_bit(wb, 0);  // unused
  }
}

static void write_render_size(const VP9_COMMON *cm,
                              struct vpx_write_bit_buffer *wb) {
  const int scaling_active =
      cm->width != cm->render_width || cm->height != cm->render_height;
  vpx_wb_write_bit(wb, scaling_active);
  if (scaling_active) {
    vpx_wb_write_literal(wb, cm->render_width - 1, 16);
    vpx_wb_write_literal(wb, cm->render_height - 1, 16);
  }
}

static void write_frame_size(const VP9_COMMON *cm,
                             struct vpx_write_bit_buffer *wb) {
  vpx_wb_write_literal(wb, cm->width - 1, 16);
  vpx_wb_write_literal(wb, cm->height - 1, 16);

  write_render_size(cm, wb);
}

#define INVALID_IDX -1  // Invalid buffer index.

static int get_ref_frame_map_idx(const VP9_COMP *cpi,
                                        MV_REFERENCE_FRAME ref_frame) {
  if (ref_frame == LAST_FRAME) {
    return cpi->lst_fb_idx;
  } else if (ref_frame == GOLDEN_FRAME) {
    return cpi->gld_fb_idx;
  } else {
    return cpi->alt_fb_idx;
  }
}

/*static int get_ref_frame_buf_idx(const VP9_COMP *const cpi,
                                        int ref_frame) {
  const VP9_COMMON *const cm = &cpi->common;
  const int map_idx = get_ref_frame_map_idx(cpi, (MV_REFERENCE_FRAME)ref_frame);
  return (map_idx != INVALID_IDX) ? cm->ref_frame_map[map_idx] : INVALID_IDX;
}*/

static void write_frame_size_with_refs(VP9_COMP *cpi,
                                       struct vpx_write_bit_buffer *wb) {
  VP9_COMMON *const cm = &cpi->common;
  int found = 0;

  for (int ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {

    /*found = get_ref_frame_buf_idx(cpi, ref_frame) == INVALID_IDX ? 0 : 1;*/

    vpx_wb_write_bit(wb, found);
    if (found) {
      break;
    }
  }

  if (!found) {
    vpx_wb_write_literal(wb, cm->width - 1, 16);
    vpx_wb_write_literal(wb, cm->height - 1, 16);
  }

  write_render_size(cm, wb);
}

static void write_interp_filter(INTERP_FILTER filter,
                                struct vpx_write_bit_buffer *wb) {
  const int filter_to_literal[] = { 1, 0, 2, 3 };

  vpx_wb_write_bit(wb, filter == SWITCHABLE);
  if (filter != SWITCHABLE)
    vpx_wb_write_literal(wb, filter_to_literal[filter], 2);
}

#define FRAME_CONTEXTS_LOG2 2
#define FRAME_CONTEXTS (1 << FRAME_CONTEXTS_LOG2)

static void encode_loopfilter(struct loopfilter *lf,
                              struct vpx_write_bit_buffer *wb,
                              mfxU16 &bitOffsetModeDelta,
                              mfxU16 &bitOffsetRefDelta) {
  int i;

  // Encode the loop filter level and type
  vpx_wb_write_literal(wb, lf->filter_level, 6);
  vpx_wb_write_literal(wb, lf->sharpness_level, 3);

  // Write out loop filter deltas applied at the MB level based on mode or
  // ref frame (if they are enabled).
  vpx_wb_write_bit(wb, lf->mode_ref_delta_enabled);

  if (lf->mode_ref_delta_enabled) {
    vpx_wb_write_bit(wb, lf->mode_ref_delta_update);
    if (lf->mode_ref_delta_update) {
        bitOffsetRefDelta = (mfxU16)wb->bit_offset;
      for (i = 0; i < MAX_REF_LF_DELTAS; i++) {
        const int delta = lf->ref_deltas[i];
        const int changed = delta != lf->last_ref_deltas[i];
        vpx_wb_write_bit(wb, changed);
        if (changed) {
          lf->last_ref_deltas[i] = (char)delta;
          vpx_wb_write_literal(wb, abs(delta) & 0x3F, 6);
          vpx_wb_write_bit(wb, delta < 0);
        }
      }

      bitOffsetModeDelta = (mfxU16)wb->bit_offset;
      for (i = 0; i < MAX_MODE_LF_DELTAS; i++) {
        const int delta = lf->mode_deltas[i];
        const int changed = delta != lf->last_mode_deltas[i];
        vpx_wb_write_bit(wb, changed);
        if (changed) {
          lf->last_mode_deltas[i] = (char)delta;
          vpx_wb_write_literal(wb, abs(delta) & 0x3F, 6);
          vpx_wb_write_bit(wb, delta < 0);
        }
      }
    }
  }
}

#define QINDEX_BITS 8

static void write_delta_q(struct vpx_write_bit_buffer *wb, int delta_q) {
  if (delta_q != 0) {
    vpx_wb_write_bit(wb, 1);
    vpx_wb_write_literal(wb, abs(delta_q), 4);
    vpx_wb_write_bit(wb, delta_q < 0);
  } else {
    vpx_wb_write_bit(wb, 0);
  }
}

static void encode_quantization(const VP9_COMMON *const cm,
                                struct vpx_write_bit_buffer *wb) {
  vpx_wb_write_literal(wb, cm->base_qindex, QINDEX_BITS);
  write_delta_q(wb, cm->y_dc_delta_q);
  write_delta_q(wb, cm->uv_dc_delta_q);
  write_delta_q(wb, cm->uv_ac_delta_q);
}

static void encode_segmentation(VP9_COMMON *cm, struct vpx_write_bit_buffer *wb) {
    cm;
    vpx_wb_write_bit(wb, 0);
    return;
}

#define MAX_TILE_WIDTH_B64 64
#define MIN_TILE_WIDTH_B64 4

static int get_min_log2_tile_cols(const int sb64_cols) {
  int min_log2 = 0;
  while ((MAX_TILE_WIDTH_B64 << min_log2) < sb64_cols) ++min_log2;
  return min_log2;
}

static int get_max_log2_tile_cols(const int sb64_cols) {
  int max_log2 = 1;
  while ((sb64_cols >> max_log2) >= MIN_TILE_WIDTH_B64) ++max_log2;
  return max_log2 - 1;
}

#define MI_SIZE_LOG2 3
#define MI_BLOCK_SIZE_LOG2 (6 - MI_SIZE_LOG2)  // 64 = 2^6
#define MI_BLOCK_SIZE (1 << MI_BLOCK_SIZE_LOG2)  // mi-units per max block

#define ALIGN_POWER_OF_TWO(value, n) \
  (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

static int mi_cols_aligned_to_sb(int n_mis) {
  return ALIGN_POWER_OF_TWO(n_mis, MI_BLOCK_SIZE_LOG2);
}

void vp9_get_tile_n_bits(int mi_cols, int *min_log2_tile_cols,
                         int *max_log2_tile_cols) {
  const int sb64_cols = mi_cols_aligned_to_sb(mi_cols) >> MI_BLOCK_SIZE_LOG2;
  *min_log2_tile_cols = get_min_log2_tile_cols(sb64_cols);
  *max_log2_tile_cols = get_max_log2_tile_cols(sb64_cols);
  assert(*min_log2_tile_cols <= *max_log2_tile_cols);
}

static void write_tile_info(const VP9_COMMON *const cm,
                            struct vpx_write_bit_buffer *wb) {
  int min_log2_tile_cols, max_log2_tile_cols, ones;
  vp9_get_tile_n_bits(cm->mi_cols, &min_log2_tile_cols, &max_log2_tile_cols);

  // columns
  ones = cm->log2_tile_cols - min_log2_tile_cols;
  while (ones--) vpx_wb_write_bit(wb, 1);

  if (cm->log2_tile_cols < max_log2_tile_cols) vpx_wb_write_bit(wb, 0);

  // rows
  vpx_wb_write_bit(wb, cm->log2_tile_rows != 0);
  if (cm->log2_tile_rows != 0) vpx_wb_write_bit(wb, cm->log2_tile_rows != 1);
}

struct BitOffsets
{
    mfxU16 BitOffsetForLFRefDelta;
    mfxU16 BitOffsetForLFModeDelta;
    mfxU16 BitOffsetForLFLevel;
    mfxU16 BitOffsetForQIndex;
    mfxU16 BitOffsetForFirstPartitionSize;
    mfxU16 BitOffsetForSegmentation;
    mfxU16 BitSizeForSegmentation;
};

static void write_uncompressed_header(VP9_COMP *cpi,
                                      struct vpx_write_bit_buffer *wb,
                                      BitOffsets &offsets) {
  Zero(offsets);
  VP9_COMMON *const cm = &cpi->common;
  //MACROBLOCKD *const xd = &cpi->td.mb.e_mbd; // no segment info for now

  vpx_wb_write_literal(wb, VP9_FRAME_MARKER, 2);

  write_profile(cm->profile, wb);

  vpx_wb_write_bit(wb, 0);  // show_existing_frame
  vpx_wb_write_bit(wb, cm->frame_type);
  vpx_wb_write_bit(wb, cm->show_frame);
  vpx_wb_write_bit(wb, cm->error_resilient_mode);

  if (cm->frame_type == KEY_FRAME) {
    write_sync_code(wb);
    write_bitdepth_colorspace_sampling(cm, wb);
    write_frame_size(cm, wb);
  } else {
    // In spatial svc if it's not error_resilient_mode then we need to code all
    // visible frames as invisible. But we need to keep the show_frame flag so
    // that the publisher could know whether it is supposed to be visible.
    // So we will code the show_frame flag as it is. Then code the intra_only
    // bit here. This will make the bitstream incompatible. In the player we
    // will change to show_frame flag to 0, then add an one byte frame with
    // show_existing_frame flag which tells the decoder which frame we want to
    // show.
    if (!cm->show_frame) vpx_wb_write_bit(wb, cm->intra_only);

    if (!cm->error_resilient_mode)
      vpx_wb_write_literal(wb, cm->reset_frame_context, 2);

    if (cm->intra_only) {
      write_sync_code(wb);

      // Note for profile 0, 420 8bpp is assumed.
      if (cm->profile > PROFILE_0) {
        write_bitdepth_colorspace_sampling(cm, wb);
      }

      vpx_wb_write_literal(wb, cpi->refresh_frames_mask, REF_FRAMES);
      write_frame_size(cm, wb);
    } else {
      vpx_wb_write_literal(wb, cpi->refresh_frames_mask, REF_FRAMES);
      for (int ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame = ref_frame) {
        assert(get_ref_frame_map_idx(cpi, (MV_REFERENCE_FRAME)ref_frame) != INVALID_IDX);
        vpx_wb_write_literal(wb, get_ref_frame_map_idx(cpi, (MV_REFERENCE_FRAME)ref_frame),
                             REF_FRAMES_LOG2);
        vpx_wb_write_bit(wb, cm->ref_frame_sign_bias[ref_frame]);
      }

      write_frame_size_with_refs(cpi, wb);

      vpx_wb_write_bit(wb, cm->allow_high_precision_mv);

      //fix_interp_filter(cm, cpi->td.counts);

      write_interp_filter(cm->interp_filter, wb);
    }
  }

  if (!cm->error_resilient_mode) {
    vpx_wb_write_bit(wb, cm->refresh_frame_context);
    vpx_wb_write_bit(wb, cm->frame_parallel_decoding_mode);
  }

  vpx_wb_write_literal(wb, cm->frame_context_idx, FRAME_CONTEXTS_LOG2);

  offsets.BitOffsetForLFLevel = (mfxU16)wb->bit_offset;
  encode_loopfilter(&cm->lf, wb, offsets.BitOffsetForLFModeDelta, offsets.BitOffsetForLFRefDelta);
  offsets.BitOffsetForQIndex = (mfxU16)wb->bit_offset;
  encode_quantization(cm, wb);
  offsets.BitOffsetForSegmentation = (mfxU16)wb->bit_offset;;
  encode_segmentation(cm, wb);
  offsets.BitSizeForSegmentation = (mfxU16)wb->bit_offset - offsets.BitOffsetForSegmentation;

  write_tile_info(cm, wb);

  offsets.BitOffsetForFirstPartitionSize = (mfxU16)wb->bit_offset;
  vpx_wb_write_literal(wb, 0, 16);
}

// header packing from libvpx

void FillSpsBuffer(
    VP9MfxParam const & par,
    ENCODE_CAPS_VP9 const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_VP9 & sps)
{
    Zero(sps);

    sps.wMaxFrameWidth    = par.mfx.FrameInfo.Width;
    sps.wMaxFrameHeight   = par. mfx.FrameInfo.Height;
    sps.GopPicSize        = par.mfx.GopPicSize;
    sps.TargetUsage       = (UCHAR)par.mfx.TargetUsage;
    sps.RateControlMethod = (UCHAR)par.mfx.RateControlMethod; // TODO: make correct mapping for mfx->DDI

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        sps.TargetBitRate[0]           = par.mfx.TargetKbps; // no support for temporal scalability. TODO: add support for temporal scalability
        sps.MaxBitRate                 = par.mfx.MaxKbps; // TODO: understand how to use MaxBitRate for temporal scalability
        sps.VBVBufferSizeInBit         = par.mfx.BufferSizeInKB * 8000; // TODO: understand how to use for temporal scalability
        sps.InitVBVBufferFullnessInBit = par.mfx.InitialDelayInKB * 8000; // TODO: understand how to use for temporal scalability
    }

    sps.FrameRate[0].Numerator   =  par.mfx.FrameInfo.FrameRateExtN; // no support for temporal scalability. TODO: add support for temporal scalability
    sps.FrameRate[0].Denominator = par.mfx.FrameInfo.FrameRateExtD;  // no support for temporal scalability. TODO: add support for temporal scalability

    sps.NumTemporalLayersMinus1 = 0;
}

void FillPpsBuffer(
    VP9MfxParam const & par,
    Task const & task,
    ENCODE_SET_PICTURE_PARAMETERS_VP9 & pps)
{
    // const part of pps structure
    Zero(pps);

    pps.LumaACQIndex        = task.m_sFrameParams.QIndex;
    pps.LumaDCQIndexDelta   = (CHAR)task.m_sFrameParams.QIndexDeltaLumaDC;
    pps.ChromaACQIndexDelta = (CHAR)task.m_sFrameParams.QIndexDeltaChromaAC;
    pps.ChromaDCQIndexDelta = (CHAR)task.m_sFrameParams.QIndexDeltaChromaDC;

    //pps.MinLumaACQIndex = 0;
    //pps.MaxLumaACQIndex = 127;

    // per-frame part of pps structure
    pps.SrcFrameWidthMinus1 =  par.mfx.FrameInfo.Width - 1; // it's workaround Actual value shuld be talen from task to support dynamic scaling. TODO: fix for dynamic scaling.
    pps.SrcFrameHeightMinus1 = par.mfx.FrameInfo.Height - 1; // it's workaround Actual value shuld be talen from task to support dynamic scaling.
    pps.DstFrameWidthMinus1 =  par.mfx.FrameInfo.Width - 1; // it's workaround Actual value shuld be talen from task to support dynamic scaling. TODO: fix for dynamic scaling.
    pps.DstFrameHeightMinus1 = par.mfx.FrameInfo.Height - 1; // it's workaround Actual value shuld be talen from task to support dynamic scaling.

    // for VP9 encoder driver uses CurrOriginalPic to get input frame from raw surfaces chain. It's incorrect behavior. Workaround on MSDK level is to set CurrOriginalPic = 0.
    // this WA works for synchronous encoding only. For async encoding fix in driver is required.
    pps.CurrOriginalPic      = 0;
    pps.CurrReconstructedPic = (UCHAR)task.m_pRecFrame->idInPool;

    mfxU16 refIdx = 0;

    while (refIdx < 8)
    {
        pps.RefFrameList[refIdx++] = 0xff;
    }

    refIdx = 0;

    pps.RefFlags.fields.refresh_frame_flags = 0;
    for (mfxU8 i = 0; i < DPB_SIZE; i++)
    {
        pps.RefFlags.fields.refresh_frame_flags |= (task.m_sFrameParams.refreshRefFrames[i] << i);
    }

    if (task.m_pRecRefFrames[REF_LAST])
    {
        pps.RefFrameList[refIdx] = (UCHAR)task.m_pRecRefFrames[REF_LAST]->idInPool;
        pps.RefFlags.fields.LastRefIdx = refIdx;
        pps.RefFlags.fields.ref_frame_ctrl_l0 |= 0x01;
        //pps.RefFlags.fields.ref_frame_ctrl_l1 |= 0x01;
        refIdx ++;
    }

    if (task.m_pRecRefFrames[REF_GOLD])
    {
        pps.RefFrameList[refIdx] = (UCHAR)task.m_pRecRefFrames[REF_GOLD]->idInPool;
        pps.RefFlags.fields.GoldenRefIdx = refIdx;
        pps.RefFlags.fields.ref_frame_ctrl_l0 |= 0x02;
        //pps.RefFlags.fields.ref_frame_ctrl_l1 |= 0x02;
        refIdx ++;
    }

    if (task.m_pRecRefFrames[REF_ALT])
    {
        pps.RefFrameList[refIdx] = (UCHAR)task.m_pRecRefFrames[REF_ALT]->idInPool;
        pps.RefFlags.fields.AltRefIdx = refIdx;
        pps.RefFlags.fields.ref_frame_ctrl_l0 |= 0x04;
        //pps.RefFlags.fields.ref_frame_ctrl_l1 |= 0x04;
        refIdx ++;
    }

    pps.PicFlags.fields.frame_type           = (task.m_sFrameParams.bIntra) ? 0 : 1;
    pps.PicFlags.fields.show_frame           = 1;
    pps.PicFlags.fields.error_resilient_mode = 0;
    pps.PicFlags.fields.intra_only           = 0;
    pps.PicFlags.fields.refresh_frame_context = 1;
    pps.PicFlags.fields.allow_high_precision_mv = 1;
    pps.PicFlags.fields.segmentation_enabled = 0; // segmentation isn't supported for now. TODO: enable segmentation

    pps.LumaACQIndex         = task.m_sFrameParams.QIndex;
    pps.LumaDCQIndexDelta   = (mfxI8)task.m_sFrameParams.QIndexDeltaLumaDC;
    pps.ChromaACQIndexDelta = (mfxI8)task.m_sFrameParams.QIndexDeltaChromaAC;
    pps.ChromaACQIndexDelta = (mfxI8)task.m_sFrameParams.QIndexDeltaChromaDC;

    pps.filter_level = task.m_sFrameParams.LFLevel;
    pps.sharpness_level = task.m_sFrameParams.Sharpness;

    for (mfxU16 i = 0; i < 4; i ++)
    {
        pps.LFRefDelta[i] = (mfxI8)task.m_sFrameParams.LFRefDelta[i];
    }

    for (mfxU16 i = 0; i < 2; i ++)
    {
        pps.LFModeDelta[i] = (mfxI8)task.m_sFrameParams.LFRefDelta[i];
    }

    /*pps.BitOffsetForLFRefDelta         = 0;
    pps.BitOffsetForLFModeDelta        = 4 * 4 + pps.BitOffsetForLFModeDelta;
    pps.BitOffsetForLFLevel            = 4 * 2 + pps.BitOffsetForLFLevel;
    pps.BitOffsetForQIndex             = 4 * 1 + pps.BitOffsetForQIndex;
    pps.BitOffsetForFirstPartitionSize = 4 * 1 + pps.BitOffsetForFirstPartitionSize;
    pps.BitOffsetForSegmentation       = 4 * 1 + pps.BitOffsetForSegmentation;*/

    pps.log2_tile_columns   = 0; // no tiles support
    pps.log2_tile_rows      = 0; // no tiles support

    pps.StatusReportFeedbackNumber = task.m_frameOrder; // TODO: fix to unique value
}

void CachedFeedback::Reset(mfxU32 cacheSize)
{
    Feedback init;
    Zero(init);
    init.bStatus = ENCODE_NOTAVAILABLE;

    m_cache.resize(cacheSize, init);
}

void CachedFeedback::Update(const FeedbackStorage& update)
{
    for (size_t i = 0; i < update.size(); i++)
    {
        if (update[i].bStatus != ENCODE_NOTAVAILABLE)
        {
            Feedback* cache = 0;

            for (size_t j = 0; j < m_cache.size(); j++)
            {
                if (m_cache[j].StatusReportFeedbackNumber == update[i].StatusReportFeedbackNumber)
                {
                    cache = &m_cache[j];
                    break;
                }
                else if (cache == 0 && m_cache[j].bStatus == ENCODE_NOTAVAILABLE)
                {
                    cache = &m_cache[j];
                }
            }

            if (cache == 0)
            {
                assert(!"no space in cache");
                throw std::logic_error(": no space in cache");
            }

            *cache = update[i];
        }
    }
}

const CachedFeedback::Feedback* CachedFeedback::Hit(mfxU32 feedbackNumber) const
{
    for (size_t i = 0; i < m_cache.size(); i++)
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
            return &m_cache[i];

    return 0;
}

void CachedFeedback::Remove(mfxU32 feedbackNumber)
{
    for (size_t i = 0; i < m_cache.size(); i++)
    {
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
        {
            m_cache[i].bStatus = ENCODE_NOTAVAILABLE;
            return;
        }
    }

    assert(!"wrong feedbackNumber");
}

D3D9Encoder::D3D9Encoder()
: m_pmfxCore(NULL)
{
} // D3D9Encoder::D3D9Encoder(VideoCORE* core)


D3D9Encoder::~D3D9Encoder()
{
    Destroy();

} // D3D9Encoder::~D3D9Encoder()

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

mfxStatus D3D9Encoder::CreateAuxilliaryDevice(
    mfxCoreInterface* pCore,
    GUID guid,
    mfxU32 width,
    mfxU32 height)
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::CreateAuxilliaryDevice +");

    if (pCore == 0)
    {
        return MFX_ERR_NULL_PTR;
    }

    m_pmfxCore = pCore;
    m_guid = guid;

    IDirect3DDeviceManager9* device = 0;

    mfxStatus sts = pCore->GetHandle(pCore->pthis, MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&device);

    if (sts == MFX_ERR_NOT_FOUND)
        sts = m_pmfxCore->CreateAccelerationDevice(m_pmfxCore->pthis, MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&device);

    MFX_CHECK_STS(sts);
    MFX_CHECK(device, MFX_ERR_DEVICE_FAILED);

    std::auto_ptr<AuxiliaryDevice> auxDevice(new AuxiliaryDevice());
    sts = auxDevice->Initialize(device);
    MFX_CHECK_STS(sts);

    sts = auxDevice->IsAccelerationServiceExist(guid);
    MFX_CHECK_STS(sts);

    Zero(m_caps);

    HRESULT hr = auxDevice->Execute(AUXDEV_QUERY_ACCEL_CAPS, &guid, sizeof(guid),& m_caps, sizeof(m_caps));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    // MFX_CHECK(m_caps.EncodeFunc, MFX_ERR_DEVICE_FAILED); TODO: need to uncomment for machine with support of VP9 HW Encode

    m_width  = width;
    m_height = height;

    m_auxDevice = auxDevice;

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::CreateAuxilliaryDevice -");
    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::CreateAuxilliaryDevice(VideoCORE* core, GUID guid, mfxU32 width, mfxU32 height)

#define VP9_MAX_UNCOMPRESSED_HEADER_SIZE 1000

mfxStatus D3D9Encoder::CreateAccelerationService(VP9MfxParam const & par)
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::CreateAccelerationService +");

    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    m_video = par;

    DXVADDI_VIDEODESC desc = {};
    desc.SampleWidth  = m_width;
    desc.SampleHeight = m_height;
    desc.Format = D3DDDIFMT_NV12;

    ENCODE_CREATEDEVICE encodeCreateDevice = {};
    encodeCreateDevice.pVideoDesc     = &desc;
    encodeCreateDevice.CodingFunction = ENCODE_ENC_PAK;
    encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

    HRESULT hr = m_auxDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, m_guid, encodeCreateDevice);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    FillSpsBuffer(par, m_caps, m_sps);

    m_uncompressedHeaderBuf.resize(VP9_MAX_UNCOMPRESSED_HEADER_SIZE);
    InitVp9VideoParam(m_video, m_libvpxBasedVP9Param);

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::CreateAccelerationService -");
    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::CreateAccelerationService(MfxVideoParam const & par)


mfxStatus D3D9Encoder::Reset(VP9MfxParam const & par)
{
    m_video = par;

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::Reset(MfxVideoParam const & par)

mfxU32 D3D9Encoder::GetReconSurfFourCC()
{
    return MFX_FOURCC_NV12;
} // mfxU32 D3D9Encoder::GetReconSurfFourCC()

mfxStatus D3D9Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)
{
    frameWidth; frameHeight;
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::QueryCompBufferInfo +");

    ENCODE_FORMAT_COUNT encodeFormatCount;
    encodeFormatCount.CompressedBufferInfoCount = 0;
    encodeFormatCount.UncompressedFormatCount = 0;

    GUID guid = m_auxDevice->GetCurrentGuid();
    HRESULT hr = m_auxDevice->Execute(ENCODE_FORMAT_COUNT_ID, guid, encodeFormatCount);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    std::vector<ENCODE_COMP_BUFFER_INFO> compBufferInfo;
    std::vector<D3DDDIFORMAT> uncompBufferInfo;
    compBufferInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
    uncompBufferInfo.resize(encodeFormatCount.UncompressedFormatCount);

    ENCODE_FORMATS encodeFormats;
    encodeFormats.CompressedBufferInfoSize = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
    encodeFormats.UncompressedFormatSize = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
    encodeFormats.pCompressedBufferInfo = &compBufferInfo[0];
    encodeFormats.pUncompressedFormats = &uncompBufferInfo[0];

    hr = m_auxDevice->Execute(ENCODE_FORMATS_ID, (void *)0, encodeFormats);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK(encodeFormats.CompressedBufferInfoSize > 0, MFX_ERR_DEVICE_FAILED);
    MFX_CHECK(encodeFormats.UncompressedFormatSize > 0, MFX_ERR_DEVICE_FAILED);

    size_t i = 0;
    for (; i < compBufferInfo.size(); i++)
        if (compBufferInfo[i].Type == type)
            break;

    MFX_CHECK(i < compBufferInfo.size(), MFX_ERR_DEVICE_FAILED);

    request.Info.Width  = compBufferInfo[i].CreationWidth;
    request.Info.Height = compBufferInfo[i].CreationHeight;
    request.Info.FourCC = compBufferInfo[i].CompressedFormats;

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::QueryCompBufferInfo -");

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)


mfxStatus D3D9Encoder::QueryEncodeCaps(ENCODE_CAPS_VP9& caps)
{
    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::QueryEncodeCaps(ENCODE_CAPS& caps)

mfxStatus D3D9Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Register +");

    mfxFrameAllocator & fa = m_pmfxCore->FrameAllocator;
    EmulSurfaceRegister surfaceReg = {};
    surfaceReg.type = type;
    surfaceReg.num_surf = response.NumFrameActual;

    MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxStatus sts = fa.GetHDL(fa.pthis, response.mids[i], (mfxHDL *)&surfaceReg.surface[i]);
        MFX_CHECK_STS(sts);
        MFX_CHECK(surfaceReg.surface[i], MFX_ERR_UNSUPPORTED);
    }

    HRESULT hr = m_auxDevice->BeginFrame(surfaceReg.surface[0], &surfaceReg);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_auxDevice->EndFrame(0);

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        m_feedbackUpdate.resize(response.NumFrameActual);
        m_feedbackCached.Reset(response.NumFrameActual);
    }

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Register -");

    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)


mfxStatus D3D9Encoder::Register(mfxMemId memId, D3DDDIFORMAT type)
{
    memId;
    type;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus D3D9Encoder::Register(mfxMemId memId, D3DDDIFORMAT type)

#define NUM_COMP_BUFFERS_VP9 5 // SPS, PPS,

ENCODE_PACKEDHEADER_DATA MakePackedByteBuffer(mfxU8 * data, mfxU32 size)
{
    ENCODE_PACKEDHEADER_DATA desc = { 0 };
    desc.pData                  = data;
    desc.BufferSize             = size;
    desc.DataLength             = size;
    return desc;
}

inline void AddSeqHeader(unsigned int width,
        unsigned int   height,
        unsigned int   FrameRateN,
        unsigned int   FrameRateD,
        unsigned int   numFramesInFile,
        unsigned char *pBitstream)
{
    mfxU32   ivf_file_header[8]  = {0x46494B44, 0x00200000, 0x30395056, width + (height << 16), FrameRateN, FrameRateD, numFramesInFile, 0x00000000};
    memcpy(pBitstream, ivf_file_header, sizeof (ivf_file_header));
};

inline void AddPictureHeader(unsigned char *pBitstream)
{
    mfxU32 ivf_frame_header[3] = {0x00000000, 0x00000000, 0x00000000};
    memcpy(pBitstream, ivf_frame_header, sizeof (ivf_frame_header));
};

mfxStatus D3D9Encoder::Execute(
    Task const & task,
    mfxHDL          surface)
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Execute +");

    std::vector<ENCODE_COMPBUFFERDESC> compBufferDesc;
    compBufferDesc.resize(NUM_COMP_BUFFERS_VP9);

    ENCODE_EXECUTE_PARAMS encodeExecuteParams = { 0 };
    encodeExecuteParams.NumCompBuffers                     = 0;
    encodeExecuteParams.pCompressedBuffers                 = &compBufferDesc[0];
    encodeExecuteParams.pCipherCounter                     = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode    = 0;
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;
    UINT & bufCnt = encodeExecuteParams.NumCompBuffers;

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_SPSDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_sps));
    compBufferDesc[bufCnt].pCompBuffer = &m_sps;
    bufCnt++;

    FillPpsBuffer(m_video, task, m_pps);

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_PPSDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(m_pps));
    compBufferDesc[bufCnt].pCompBuffer = &m_pps;
    bufCnt++;

    mfxU32 bitstream = task.m_pOutBs->idInPool;
    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_BITSTREAMDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(bitstream));
    compBufferDesc[bufCnt].pCompBuffer = &bitstream;
    bufCnt++;

    // writing IVF and uncompressed header
    vpx_write_bit_buffer localBuf;

    SetNextFrame(task.m_sFrameParams, m_libvpxBasedVP9Param);
    Zero(m_uncompressedHeaderBuf);
    localBuf.bit_buffer = &m_uncompressedHeaderBuf[0];
    localBuf.bit_offset = 0;

    mfxExtCodingOptionVP9 *pOpt = GetExtBuffer(m_video);
    //if (pOpt->WriteIVFHeaders)
    {
        if (InsertSeqHeader(task))
        {
            AddSeqHeader(m_video.mfx.FrameInfo.Width,
                m_video.mfx.FrameInfo.Height,
                m_video.mfx.FrameInfo.FrameRateExtN,
                m_video.mfx.FrameInfo.FrameRateExtD,
                pOpt->NumFramesForIVF,
                localBuf.bit_buffer);
            localBuf.bit_offset += IVF_SEQ_HEADER_SIZE_BYTES * 8;
        }

        AddPictureHeader(localBuf.bit_buffer + localBuf.bit_offset);
        localBuf.bit_offset += IVF_PIC_HEADER_SIZE_BYTES * 8;
    }

    BitOffsets offsets;
    write_uncompressed_header(&m_libvpxBasedVP9Param, &localBuf, offsets);

    m_pps.BitOffsetForLFLevel = offsets.BitOffsetForLFLevel;
    m_pps.BitOffsetForLFModeDelta = offsets.BitOffsetForLFModeDelta;
    m_pps.BitOffsetForLFRefDelta = offsets.BitOffsetForLFRefDelta;
    m_pps.BitOffsetForQIndex = offsets.BitOffsetForQIndex;
    m_pps.BitOffsetForFirstPartitionSize = offsets.BitOffsetForFirstPartitionSize;
    m_pps.BitOffsetForSegmentation = offsets.BitOffsetForSegmentation;
    m_pps.BitSizeForSegmentation = offsets.BitSizeForSegmentation;

    mfxU32 bytesWritten = mfxU32((localBuf.bit_offset + 7) / 8);
    m_descForFrameHeader = MakePackedByteBuffer(localBuf.bit_buffer, bytesWritten);

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
    compBufferDesc[bufCnt].DataSize = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
    compBufferDesc[bufCnt].pCompBuffer = &m_descForFrameHeader;
    bufCnt++;

    try
    {
        HRESULT hr = m_auxDevice->BeginFrame((IDirect3DSurface9 *)surface, 0);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        hr = m_auxDevice->Execute(ENCODE_ENC_PAK_ID, encodeExecuteParams, (void *)0);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        HANDLE handle;
        m_auxDevice->EndFrame(&handle);
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Execute -");

    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus D3D9Encoder::QueryStatus(
    Task & task)
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::QueryStatus +");

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_frameOrder); // TODO: fix to unique status report number

    // if task is not in cache then query its status
    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {
        try
        {
            ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr;
            feedbackDescr.SizeOfStatusParamStruct = sizeof(m_feedbackUpdate[0]);
            feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;

            HRESULT hr = m_auxDevice->Execute(
                ENCODE_QUERY_STATUS_ID,
                (void *)&feedbackDescr,
                sizeof(feedbackDescr),
                &m_feedbackUpdate[0],
                (mfxU32)m_feedbackUpdate.size() * sizeof(m_feedbackUpdate[0]));

            MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
        catch (...)
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        // Put all with ENCODE_OK into cache.
        m_feedbackCached.Update(m_feedbackUpdate);

        feedback = m_feedbackCached.Hit(task.m_frameOrder);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    mfxStatus sts;
    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        task.m_bsDataLength = feedback->bitstreamSize; // TODO: save bitstream size here
        m_feedbackCached.Remove(task.m_frameOrder);
        sts = MFX_ERR_NONE;
        break;
    case ENCODE_NOTREADY:
        sts = MFX_WRN_DEVICE_BUSY;
        break;
    case ENCODE_ERROR:
        assert(!"task status ERROR");
        sts = MFX_ERR_DEVICE_FAILED;
        break;
    case ENCODE_NOTAVAILABLE:
    default:
        assert(!"bad feedback status");
        sts = MFX_ERR_DEVICE_FAILED;
    }

    VP9_LOG("\n (VP9_LOG) D3D9Encoder::QueryStatus -");
    return sts;
} // mfxStatus D3D9Encoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)


mfxStatus D3D9Encoder::Destroy()
{
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Destroy +");
    m_auxDevice.reset(0);
    VP9_LOG("\n (VP9_LOG) D3D9Encoder::Destroy -");

    return MFX_ERR_NONE;
} // mfxStatus D3D9Encoder::Destroy()

#endif // (MFX_VA_WIN)

} // MfxHwVP9Encode

#endif // (_WIN32) || (_WIN64)

#endif // AS_VP9E_PLUGIN
