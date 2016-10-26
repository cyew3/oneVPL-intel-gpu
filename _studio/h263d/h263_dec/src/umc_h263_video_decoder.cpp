//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2005-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#include <string.h>
#include "vm_debug.h"
#include "umc_h263_video_decoder.h"
#include "umc_video_data.h"
#include "h263.h"

namespace UMC
{
VideoDecoder *CreateH263Decoder() { return (new H263VideoDecoder()); }

Status H263VideoDecoder::AllocateInitFrame(h263_Frame* pFrame)
{
  Status  status;
  Ipp32s  w, h;
  Ipp8u  *p;

  w = (pFrame->mbPerRow + 2 * H263_NUM_EXT_MB) << 4;
  h = (pFrame->mbPerCol + 2 * H263_NUM_EXT_MB) << 4;
  status = m_pMemoryAllocator->Alloc(&pFrame->mid, w * h + (w * h >> 1), UMC_ALLOC_PERSISTENT);
  if (status != UMC_OK)
    return status;
  p = (Ipp8u*)m_pMemoryAllocator->Lock(pFrame->mid);
  ippsSet_8u(0, p, w * h);
  ippsSet_8u(128, p + w * h, w * h >> 1);
  status = m_pMemoryAllocator->Unlock(pFrame->mid);
  if (status != UMC_OK)
    return status;
  pFrame->stepY = w;
  pFrame->stepCb = w >> 1;
  pFrame->stepCr = w >> 1;
  return UMC_OK;
}

void H263VideoDecoder::LockFrame(h263_Frame* pFrame)
{
  Ipp32s  w, h;

  w = (pFrame->mbPerRow + 2 * H263_NUM_EXT_MB) << 4;
  h = (pFrame->mbPerCol + 2 * H263_NUM_EXT_MB) << 4;
  pFrame->apY = (Ipp8u*)m_pMemoryAllocator->Lock(pFrame->mid);
  pFrame->pY = pFrame->apY + w * 16 + 16;
  pFrame->apCb = pFrame->apY + w * h;
  w >>= 1;
  h >>= 1;
  pFrame->pCb = pFrame->apCb + w * 8 + 8;
  pFrame->apCr = pFrame->apCb + w * h;
  pFrame->pCr = pFrame->apCr + w * 8 + 8;
}

Status H263VideoDecoder::AllocateBuffers()
{
  Status  status = UMC_OK;

  // allocate only frame memory, may be extended for whole buffers
  // current frame
  status = AllocateInitFrame(&m_decInfo->VideoSequence.cFrame);
  if (status != UMC_OK)
    return status;
    // ref in past frame
  status = AllocateInitFrame(&m_decInfo->VideoSequence.rFrame);
  if (status != UMC_OK)
    return status;
  /* not referenced frame (B-part of PB-frame or B-frame) */
  status = AllocateInitFrame(&m_decInfo->VideoSequence.nFrame);
  if (status != UMC_OK)
    return status;

  /* not referenced frame (B-part of PB-frame or B-frame) */
  status = AllocateInitFrame(&m_decInfo->VideoSequence.bFrame);
  if (status != UMC_OK)
    return status;

  return status;
}

Status H263VideoDecoder::FreeBuffers()
{
  Status  status = UMC_OK;
  Ipp32s i;

  if (m_decInfo->VideoSequence.cFrame.mid) {
    status = m_pMemoryAllocator->Free(m_decInfo->VideoSequence.cFrame.mid);
    if (status != UMC_OK)
      return status;
  }
  if (m_decInfo->VideoSequence.rFrame.mid) {
    status = m_pMemoryAllocator->Free(m_decInfo->VideoSequence.rFrame.mid);
    if (status != UMC_OK)
      return status;
  }
  if (m_decInfo->VideoSequence.nFrame.mid) {
    status = m_pMemoryAllocator->Free(m_decInfo->VideoSequence.nFrame.mid);
    if (status != UMC_OK)
      return status;
  }
  if (m_decInfo->VideoSequence.bFrame.mid) {
    status = m_pMemoryAllocator->Free(m_decInfo->VideoSequence.bFrame.mid);
    if (status != UMC_OK)
      return status;
  }

  for (i = 0; i < H263_MAX_ENH_LAYERS; i++) {
    h263_EnhancedLayer *layer = m_decInfo->VideoSequence.enhLayers[i];
    if (layer) {
      if (layer->c_Frame.mid) {
        status = m_pMemoryAllocator->Free(layer->c_Frame.mid);
        if (status != UMC_OK)
          return status;
      }
      if (layer->r_Frame.mid) {
        status = m_pMemoryAllocator->Free(layer->r_Frame.mid);
        if (status != UMC_OK)
          return status;
      }
      if (layer->n_Frame.mid) {
        status = m_pMemoryAllocator->Free(layer->n_Frame.mid);
        if (status != UMC_OK)
          return status;
      }
      if (layer->a_Frame.mid) {
        status = m_pMemoryAllocator->Free(layer->a_Frame.mid);
        if (status != UMC_OK)
          return status;
      }
      ippsFree(layer);
      m_decInfo->VideoSequence.enhLayers[i] = NULL;
    }
  }


  return status;
}

void H263VideoDecoder::LockBuffers()
{
  LockFrame(&m_decInfo->VideoSequence.cFrame);
  LockFrame(&m_decInfo->VideoSequence.rFrame);
  LockFrame(&m_decInfo->VideoSequence.nFrame);
  LockFrame(&m_decInfo->VideoSequence.bFrame);
  if (m_decInfo->VideoSequence.aFrame.mid)
    LockFrame(&m_decInfo->VideoSequence.aFrame);
}

Status H263VideoDecoder::UnlockBuffers()
{
  Status status = UMC_OK;

  status = m_pMemoryAllocator->Unlock(m_decInfo->VideoSequence.cFrame.mid);
  if (status != UMC_OK)
    return status;
  status = m_pMemoryAllocator->Unlock(m_decInfo->VideoSequence.rFrame.mid);
  if (status != UMC_OK)
    return status;
  status = m_pMemoryAllocator->Unlock(m_decInfo->VideoSequence.nFrame.mid);
  if (status != UMC_OK)
    return status;
  status = m_pMemoryAllocator->Unlock(m_decInfo->VideoSequence.bFrame.mid);
  if (status != UMC_OK)
    return status;
  if (m_decInfo->VideoSequence.aFrame.mid) {
    status = m_pMemoryAllocator->Unlock(m_decInfo->VideoSequence.aFrame.mid);
    if (status != UMC_OK)
      return status;
  }
  return status;
}

void H263VideoDecoder::LockEnhancedLayerBuffers(Ipp32s enh_layer_num, Ipp32s ref_layer_num, Ipp32s lockedMask)
{
  h263_EnhancedLayer *layer = m_decInfo->VideoSequence.enhLayers[enh_layer_num - 2];

  if (!(lockedMask & 4))
    LockFrame(&layer->c_Frame);
  if (!(lockedMask & 1))
    LockFrame(&layer->r_Frame);
  if (!(lockedMask & 2))
    LockFrame(&layer->n_Frame);
  if (!(lockedMask & 8))
    LockFrame(&layer->a_Frame);
  if (ref_layer_num > 1) {
    layer = m_decInfo->VideoSequence.enhLayers[ref_layer_num - 2];
    LockFrame(&layer->c_Frame);
    LockFrame(&layer->r_Frame);
    LockFrame(&layer->n_Frame);
    LockFrame(&layer->a_Frame);
  }
}

Status H263VideoDecoder::UnlockEnhancedLayerBuffers(Ipp32s enh_layer_num, Ipp32s ref_layer_num)
{
  Status status = UMC_OK;
  h263_EnhancedLayer *layer = m_decInfo->VideoSequence.enhLayers[enh_layer_num - 2];

  if (layer->c_Frame.mid) {
    status = m_pMemoryAllocator->Unlock(layer->c_Frame.mid);
    if (status != UMC_OK)
      return status;
  }
  if (layer->r_Frame.mid) {
    status = m_pMemoryAllocator->Unlock(layer->r_Frame.mid);
    if (status != UMC_OK)
      return status;
  }
  if (layer->n_Frame.mid) {
    status = m_pMemoryAllocator->Unlock(layer->n_Frame.mid);
    if (status != UMC_OK)
      return status;
  }
  if (layer->a_Frame.mid) {
    status = m_pMemoryAllocator->Unlock(layer->a_Frame.mid);
    if (status != UMC_OK)
      return status;
  }

  if (ref_layer_num > 1) {
    layer = m_decInfo->VideoSequence.enhLayers[ref_layer_num - 2];
    if (layer->c_Frame.mid) {
      status = m_pMemoryAllocator->Unlock(layer->c_Frame.mid);
      if (status != UMC_OK)
        return status;
    }
    if (layer->r_Frame.mid) {
      status = m_pMemoryAllocator->Unlock(layer->r_Frame.mid);
      if (status != UMC_OK)
        return status;
    }
    if (layer->n_Frame.mid) {
      status = m_pMemoryAllocator->Unlock(layer->n_Frame.mid);
      if (status != UMC_OK)
        return status;
    }
    if (layer->a_Frame.mid) {
      status = m_pMemoryAllocator->Unlock(layer->a_Frame.mid);
      if (status != UMC_OK)
        return status;
    }
  }
  return status;
}


Status H263VideoDecoder::ReAlLockFrame(h263_Frame* pFrame)
{
  Ipp32s  w, h;
  Status status = UMC_OK;
  Ipp32s mbPerRow, mbPerCol;

  mbPerRow = m_decInfo->VideoSequence.VideoPicture.MacroBlockPerRow;
  mbPerCol = m_decInfo->VideoSequence.VideoPicture.MacroBlockPerCol;

  if (pFrame->mid) {
    status = m_pMemoryAllocator->Unlock(pFrame->mid);
    if (status != UMC_OK)
      return status;
    status = m_pMemoryAllocator->Free(pFrame->mid);
    if (status != UMC_OK)
      return status;
  }

  w = (mbPerRow + 2 * H263_NUM_EXT_MB) << 4;
  h = (mbPerCol + 2 * H263_NUM_EXT_MB) << 4;

  pFrame->mbPerRow = mbPerRow;
  pFrame->mbPerCol = mbPerCol;
  pFrame->width = m_decInfo->VideoSequence.VideoPicture.width;
  pFrame->height = m_decInfo->VideoSequence.VideoPicture.height;
  pFrame->stepY = w;
  pFrame->stepCb = w >> 1;
  pFrame->stepCr = w >> 1;

  status = m_pMemoryAllocator->Alloc(&pFrame->mid, w * h + (w * h >> 1), UMC_ALLOC_PERSISTENT);
  if (status != UMC_OK)
    return status;
  pFrame->apY = (Ipp8u*)m_pMemoryAllocator->Lock(pFrame->mid);
  ippsSet_8u(0, pFrame->apY, w * h);
  pFrame->pY = pFrame->apY + w * 16 + 16;
  pFrame->apCb = pFrame->apY + w * h;
  ippsSet_8u(128, pFrame->apCb, w * h >> 1);
  w >>= 1;
  h >>= 1;
  pFrame->pCb = pFrame->apCb + w * 8 + 8;
  pFrame->apCr = pFrame->apCb + w * h;
  pFrame->pCr = pFrame->apCr + w * 8 + 8;

  return status;
}


Status H263VideoDecoder::ReAlLockBuffers(Ipp32s resizeMask)
{
  Status status = UMC_OK;

  // mbinfo realloc
  if (resizeMask & 0x100) {
    if (m_decInfo->VideoSequence.MBinfo)
      ippsFree(m_decInfo->VideoSequence.MBinfo);
    m_decInfo->VideoSequence.num_of_MBs = m_decInfo->VideoSequence.VideoPicture.MacroBlockPerCol * m_decInfo->VideoSequence.VideoPicture.MacroBlockPerRow;
    m_decInfo->VideoSequence.MBinfo = (h263_MacroBlock*)ippsMalloc_8u(m_decInfo->VideoSequence.num_of_MBs * sizeof(h263_MacroBlock));
    if (!m_decInfo->VideoSequence.MBinfo)
      return UMC_ERR_ALLOC;
  }

  if (resizeMask & 1) {
    status = ReAlLockFrame(&m_decInfo->VideoSequence.rFrame);
    if (status != UMC_OK)
      return status;
  }
  if (resizeMask & 2) {
    status = ReAlLockFrame(&m_decInfo->VideoSequence.nFrame);
    if (status != UMC_OK)
      return status;
  }
  if (resizeMask & 4) {
    status = ReAlLockFrame(&m_decInfo->VideoSequence.bFrame);
    if (status != UMC_OK)
      return status;
  }
  if (resizeMask & 8) {
    status = ReAlLockFrame(&m_decInfo->VideoSequence.aFrame);
    if (status != UMC_OK)
      return status;
  }

  if (resizeMask & 0xF0) {
    Ipp32s layer_ind = m_decInfo->VideoSequence.VideoPicture.enh_layer_num - 2;
    h263_EnhancedLayer *enh_layer = m_decInfo->VideoSequence.enhLayers[layer_ind];

    if (enh_layer == NULL) {
      enh_layer = (h263_EnhancedLayer *)ippsMalloc_8u(sizeof(h263_EnhancedLayer));
      if (enh_layer == NULL)
        return UMC_ERR_ALLOC;
      m_decInfo->VideoSequence.enhLayers[layer_ind] = enh_layer;
      ippsZero_8u((Ipp8u *)enh_layer, sizeof(h263_EnhancedLayer));
    }

    if (resizeMask & 0x10) {
      status = ReAlLockFrame(&enh_layer->r_Frame);
      if (status != UMC_OK)
        return status;
    }
    if (resizeMask & 0x20) {
      status = ReAlLockFrame(&enh_layer->n_Frame);
      if (status != UMC_OK)
        return status;
    }
    if (resizeMask & 0x40) {
      status = ReAlLockFrame(&enh_layer->c_Frame);
      if (status != UMC_OK)
        return status;
    }
    if (resizeMask & 0x80) {
      status = ReAlLockFrame(&enh_layer->a_Frame);
      if (status != UMC_OK)
        return status;
    }
  }

  return status;
}


Status H263VideoDecoder::Init(BaseCodecParams *lpInit)
{
  Status  status = UMC_OK;
  VideoDecoderParams *pParam = DynamicCast<VideoDecoderParams> (lpInit);

  Close();
  if (NULL == pParam)
    return UMC_ERR_NULL_PTR;

  try {
    // create default memory allocator
    status = BaseCodec::Init(lpInit);
    if (status != UMC_OK)
      return status;
    m_dec_time_fr = 0.0;
    m_dec_time_base = -1.0;
    m_dec_time_prev = 0.0;
    m_dec_time_frinc = (pParam->info.framerate > 0) ? 1.0 / pParam->info.framerate : 0.0;
    m_is_skipped_b = m_skipped_fr = m_b_prev = 0;
    m_Param = *pParam;
    m_IsInit = false;
    memset(m_decInfo, 0, sizeof(h263_Info));
    if (NULL != pParam->m_pData) {
      if (pParam->m_pData->GetTime() >= 0.0)
        m_dec_time_base = pParam->m_pData->GetTime();
      m_decInfo->buflen = pParam->m_pData->GetDataSize();
      if (m_decInfo->buflen > 0) {
        m_decInfo->bufptr = m_decInfo->buffer = reinterpret_cast<Ipp8u *> (pParam->m_pData->GetDataPointer());
  //      m_decInfo->bitoff = 0;
        status = InsideInit(&m_Param);
      }
    }
    if (UMC_OK == status) {
      m_PostProcessing = pParam->pPostProcessing;
      if (pParam->info.clip_info.width == 0 || pParam->info.clip_info.height == 0) {
        if (m_decInfo->VideoSequence.VideoPicture.width == 0 && m_decInfo->VideoSequence.VideoPicture.height== 0)
          return UMC_ERR_INIT;
        else {
          pParam->info.clip_info.width = m_Param.info.clip_info.width; // = m_decInfo->VideoSequence.VideoPicture.width;
          pParam->info.clip_info.height = m_Param.info.clip_info.height; // = m_decInfo->VideoSequence.VideoPicture.height;
        }
      }
      if (pParam->info.framerate <= 0) {
        Ipp32s frameInterval;
        if (m_decInfo->VideoSequence.VideoPicture.clock_divisor)
          frameInterval = m_decInfo->VideoSequence.VideoPicture.clock_divisor * (1000 + m_decInfo->VideoSequence.VideoPicture.clock_conversion_code);
        else
          frameInterval = H263_DEFAULT_FRAME_INTERVAL;
        pParam->info.framerate = m_Param.info.framerate = (Ipp64f)m_decInfo->VideoSequence.picture_time_increment_resolution / (Ipp64f)frameInterval;
      }
    }
    m_ClipInfo = m_Param.info;
    m_IsInitBase = true;
  } catch(...) {
    status = UMC_ERR_INVALID_STREAM;
  }

  return status;
}

Status H263VideoDecoder::InsideInit(VideoDecoderParams *pParam)
{
  Status status = UMC_OK;
  m_P_part_of_PB_pending = false; /* can be true only if !(m_Param.lFlags & FLAG_VDEC_REORDER) */

#ifndef UMC_H263_DISABLE_SORENSON
  if (m_Param.info.stream_subtype == H263_VIDEO_SORENSON) {
    m_decInfo->sorenson = 1;
    m_buffered_frame = false;
    m_Param.lFlags &= ~FLAG_VDEC_REORDER;
  } else {
    m_decInfo->sorenson = 0;
    m_buffered_frame = m_Param.lFlags & FLAG_VDEC_REORDER;
  }
#else
  m_buffered_frame = m_Param.lFlags & FLAG_VDEC_REORDER;
#endif

  m_decInfo->number_threads = m_Param.numThreads;

  if (!h263_SeekStartCodePtr(m_decInfo)) {
    vm_debug_trace(VM_DEBUG_INFO,__VM_STRING("Error: Can't find picture start code\n"));
    return UMC_ERR_SYNC;
  }

#ifndef UMC_H263_DISABLE_SORENSON
  if (m_decInfo->sorenson) {
    if (h263_Parse_PictureHeader_Sorenson(m_decInfo) != H263_STATUS_OK)
      return UMC_ERR_INVALID_STREAM;
  } else
#endif
  {
    if (h263_Parse_PictureHeader(m_decInfo) != H263_STATUS_OK)
      return UMC_ERR_INVALID_STREAM;
  }

  if (h263_InitVSeq(m_decInfo, -1, -1) != H263_STATUS_OK) {
    vm_debug_trace(VM_DEBUG_INFO,__VM_STRING("Error: No memory to allocate internal buffers\n"));
    return UMC_ERR_ALLOC;
  }

  pParam->info.clip_info.width = m_decInfo->VideoSequence.VideoPicture.width;
  pParam->info.clip_info.height = m_decInfo->VideoSequence.VideoPicture.height;

  status = AllocateBuffers();
  if (status != UMC_OK) {
    vm_debug_trace(VM_DEBUG_INFO,__VM_STRING("Error: Memory allocation failed\n"));
    return UMC_ERR_ALLOC;
  }

  switch (m_decInfo->VideoSequence.VideoPicture.format.PAR_code) {
    case H263_ASPECT_RATIO_1_1:
      m_Param.info.aspect_ratio_width  = 1;
      m_Param.info.aspect_ratio_height = 1;
      break;
    case H263_ASPECT_RATIO_FORBIDDEN:
    case H263_ASPECT_RATIO_12_11:
      m_Param.info.aspect_ratio_width  = 12;
      m_Param.info.aspect_ratio_height = 11;
      break;
    case H263_ASPECT_RATIO_10_11:
      m_Param.info.aspect_ratio_width  = 10;
      m_Param.info.aspect_ratio_height = 11;
      break;
    case H263_ASPECT_RATIO_16_11:
      m_Param.info.aspect_ratio_width  = 16;
      m_Param.info.aspect_ratio_height = 11;
      break;
    case H263_ASPECT_RATIO_40_33:
      m_Param.info.aspect_ratio_width  = 40;
      m_Param.info.aspect_ratio_height = 33;
      break;
    default:
      m_Param.info.aspect_ratio_width  = m_decInfo->VideoSequence.VideoPicture.format.PAR_width;
      m_Param.info.aspect_ratio_height = m_decInfo->VideoSequence.VideoPicture.format.PAR_height;
  }

  m_decInfo->bufptr = m_decInfo->buffer;
  m_decInfo->bitoff = 0;

  m_IsInit = true;
  return status;
}

Status H263VideoDecoder::GetFrame(MediaData* in, MediaData* out)
{
  Status status = UMC_OK;
  Ipp64f pts = 0;
  Ipp32s resizeMask;

#ifdef _OMP_KARABAS
  m_decInfo->mTreadPriority = vm_get_current_thread_priority();
#endif // _OMP_KARABAS

  out->SetInvalid(1);

  if (!m_IsInitBase)
    return UMC_ERR_NOT_INITIALIZED;
  if (out == NULL)
    return UMC_ERR_NULL_PTR;

  if (in) {
    m_decInfo->bitoff = 0;
    m_decInfo->bufptr = m_decInfo->buffer = (Ipp8u *)in->GetDataPointer();
    m_decInfo->buflen = in->GetDataSize();
  }
  if (!m_IsInit) {
    if (in == NULL)
      return UMC_ERR_NULL_PTR;
    try {
      m_Param.m_pData = in;
      status = InsideInit(&m_Param);
      if (status != UMC_OK) {
        in->MoveDataPointer((Ipp32s)(in->GetDataSize() - m_decInfo->buflen));
        return UMC_ERR_INIT;
      }
    } catch(...) {
      return UMC_ERR_INIT;
    }
  }
  LockBuffers();

  out->SetInvalid(0);

  try {
  for (;;) {
    if (in == NULL) {
        // show last frame (it can be only if (m_Param.lFlags & FLAG_VDEC_REORDER))
      if (!m_buffered_frame) {
        UnlockBuffers();
        return UMC_ERR_NOT_ENOUGH_DATA;
      }
      if (m_decInfo->VideoSequence.Ppics_to_show > 0) {
        m_decInfo->VideoSequence.vFrame = &m_decInfo->VideoSequence.rFrame;
        m_decInfo->VideoSequence.PicIndex++;
        m_decInfo->VideoSequence.Ppics_to_show = 0;
      } else if (m_decInfo->VideoSequence.Bpics_to_show > 0) {
        m_decInfo->VideoSequence.vFrame = &m_decInfo->VideoSequence.nFrame;
        m_decInfo->VideoSequence.PicIndex++;
        m_decInfo->VideoSequence.Bpics_to_show = 0;
      } else {
        m_decInfo->VideoSequence.vFrame = &m_decInfo->VideoSequence.cFrame;
        m_buffered_frame = false;
      }
    } else {
      if (m_decInfo->VideoSequence.Ppics_to_show < 1 && !m_P_part_of_PB_pending) {
        /* Seek the picture start code, and then begin the picture decoding */
        if (!h263_SeekStartCodePtr(m_decInfo)) {
          vm_debug_trace(VM_DEBUG_INFO,__VM_STRING("Error: Can't find picture start code\n"));
          status = UMC_ERR_SYNC;
          out->SetInvalid(1);
          break;
        }
#ifndef UMC_H263_DISABLE_SORENSON
        if (m_decInfo->sorenson) {
          if (h263_Parse_PictureHeader_Sorenson(m_decInfo) != H263_STATUS_OK) {
            status = UMC_OK;
            out->SetInvalid(1);
            break;
          }
        } else
#endif
        {
          if (h263_Parse_PictureHeader(m_decInfo) != H263_STATUS_OK) {
            status = UMC_OK;
            out->SetInvalid(1);
            break;
          }
        }

        if (m_decInfo->VideoSequence.VideoPicture.picture_coding_type == H263_PIC_TYPE_B) {
          if (0 < m_is_skipped_b && !m_b_prev) {
            m_is_skipped_b--;
            m_skipped_fr++;
            m_b_prev = 1;
            m_decInfo->bufptr = m_decInfo->buffer + m_decInfo->buflen;
            status = UMC_ERR_NOT_ENOUGH_DATA;
            break;
          } else
            m_b_prev = 0;
        }

        resizeMask = h263_Check_Resize(m_decInfo);
        if (resizeMask < 0) {
          out->SetInvalid(1);
          status = UMC_OK;
          break;
        }
        if (resizeMask) {
          status = ReAlLockBuffers(resizeMask);
          if (status != UMC_OK) {
            out->SetInvalid(1);
            break;
          }
        }
        if (m_decInfo->VideoSequence.VideoPicture.enh_layer_num > 1)
          LockEnhancedLayerBuffers(m_decInfo->VideoSequence.VideoPicture.enh_layer_num, m_decInfo->VideoSequence.VideoPicture.ref_layer_num, (resizeMask >> 4));
      }

      if (!m_P_part_of_PB_pending) {
        if ((h263_DecodeVideoPicture(m_decInfo)) != H263_STATUS_OK) {
          out->SetInvalid(1);
          status = UMC_OK;
        }
        if (!(m_Param.lFlags & FLAG_VDEC_REORDER)) {
          m_decInfo->VideoSequence.Ppics_to_show = 0;
          m_decInfo->VideoSequence.Bpics_to_show = 0;
          m_P_part_of_PB_pending = (m_decInfo->VideoSequence.VideoPicture.picture_coding_type == H263_PIC_TYPE_PB || m_decInfo->VideoSequence.VideoPicture.picture_coding_type == H263_PIC_TYPE_iPB);
        }
      } else
        m_P_part_of_PB_pending = false;

      // TODO: add enh layer output optional processing and skipping ???
      if (m_decInfo->VideoSequence.VideoPicture.enh_layer_num > 1) {
        m_decInfo->VideoSequence.enhLayers[m_decInfo->VideoSequence.VideoPicture.enh_layer_num - 2]->picIndex++;
        UnlockEnhancedLayerBuffers(m_decInfo->VideoSequence.VideoPicture.enh_layer_num, m_decInfo->VideoSequence.VideoPicture.ref_layer_num);
        status = UMC_ERR_NOT_ENOUGH_DATA;
        break;
      }

      m_decInfo->VideoSequence.PicIndex++;

      if ((m_Param.lFlags & FLAG_VDEC_REORDER) && (m_decInfo->VideoSequence.vFrame == NULL)) {
          // buffer first frame in VDEC_REORDER mode
          status = UMC_ERR_NOT_ENOUGH_DATA;
          break;
      }

      if (!(m_Param.lFlags & FLAG_VDEC_REORDER)) {
        if (m_P_part_of_PB_pending || m_decInfo->VideoSequence.VideoPicture.picture_coding_type == H263_PIC_TYPE_B) {
          m_decInfo->VideoSequence.vFrame = &m_decInfo->VideoSequence.nFrame;
        } else {
          m_decInfo->VideoSequence.vFrame = &m_decInfo->VideoSequence.cFrame;
        }
      }
    }
    break;
  }
  } catch(...) {
    status = UMC_ERR_INVALID_STREAM;
    out->SetInvalid(1);
  }
  if (in)
    if (in->GetTime() >= 0.0 && m_dec_time_base < 0.0)
      m_dec_time_base = in->GetTime();

  if (m_dec_time_frinc > 0.0) {
    if (m_Param.lFlags & FLAG_VDEC_REORDER)
      pts = m_dec_time_prev;
    if (in) {
      if (in->GetTime() >= 0.0) {
        // take right PTS for I-, P- frames
        m_dec_time_prev = in->GetTime();
      } else {
        // when PB...  are in one AVI chunk, first PTS from in->GetTime() is right and second is -1.0
        m_dec_time_prev += m_dec_time_frinc;
      }
    }
    if (!(m_Param.lFlags & FLAG_VDEC_REORDER))
      pts = m_dec_time_prev;
  } else {
    if (in && in->GetTime() >= 0.0)
      pts = in->GetTime();
    else {
      // internal pts
      if (m_decInfo->VideoSequence.vFrame)
        pts = (Ipp64f)m_decInfo->VideoSequence.vFrame->time / m_decInfo->VideoSequence.picture_time_increment_resolution;
      else
        pts = 0.0;
      if (m_dec_time_base > 0.0)
        pts += m_dec_time_base;
    }
  }
  if (UMC_OK == status && m_decInfo->VideoSequence.vFrame != NULL) {
    try {
    FrameType ft = (m_decInfo->VideoSequence.vFrame->type) == H263_PIC_TYPE_I ? I_PICTURE :
                    (m_decInfo->VideoSequence.vFrame->type) == H263_PIC_TYPE_B ? B_PICTURE :
                                                                                P_PICTURE;
    if (m_LastDecodedFrame.GetColorFormat() != YUV420) {
      m_LastDecodedFrame.Init(m_decInfo->VideoSequence.vFrame->width,
                              m_decInfo->VideoSequence.vFrame->height,
                              YUV420);
    }
    m_LastDecodedFrame.SetFrameType(ft);
    m_LastDecodedFrame.SetTime(pts);
    m_LastDecodedFrame.SetPlanePointer(m_decInfo->VideoSequence.vFrame->pY, 0);
    m_LastDecodedFrame.SetPlanePointer(m_decInfo->VideoSequence.vFrame->pCb, 1);
    m_LastDecodedFrame.SetPlanePointer(m_decInfo->VideoSequence.vFrame->pCr, 2);
    m_LastDecodedFrame.SetPlanePitch(m_decInfo->VideoSequence.vFrame->stepY, 0);
    m_LastDecodedFrame.SetPlanePitch(m_decInfo->VideoSequence.vFrame->stepCb, 1);
    m_LastDecodedFrame.SetPlanePitch(m_decInfo->VideoSequence.vFrame->stepCr, 2);
    if (!m_PostProcessing) {
      m_PostProcessing = m_allocatedPostProcessing = createVideoProcessing();
    }
    status = m_PostProcessing->GetFrame(&m_LastDecodedFrame, out);
    } catch(...) {
      status = UMC_ERR_INVALID_STREAM;
      out->SetInvalid(1);
    }

  }
  if (in) {
    size_t stDecidedData;
    if ((size_t)m_decInfo->bufptr - (size_t)m_decInfo->buffer < m_decInfo->buflen)
      stDecidedData = m_decInfo->buflen - ((size_t)m_decInfo->bufptr - (size_t)m_decInfo->buffer);
    else
      stDecidedData = 0;

    in->MoveDataPointer((Ipp32s)(in->GetDataSize() - static_cast<Ipp32u>(stDecidedData)));
    // can't calculate time for the next frame
    in->SetTime(-1.0);
  }
  UnlockBuffers();

  return status;
}

Status H263VideoDecoder::Close(void)
{
  m_IsInitBase = false;
  if (m_IsInit) {
    try {
      FreeBuffers();
      // close default memory allocator
      BaseCodec::Close();
      h263_FreeVSeq(m_decInfo);
      m_IsInit = false;
    } catch(...) {
    }
  }
  return UMC_OK;
}

H263VideoDecoder::H263VideoDecoder(void)
{
  m_IsInit = m_IsInitBase = false;
  m_decInfo = new h263_Info;
}

H263VideoDecoder::~H263VideoDecoder(void)
{
  Close();
  delete m_decInfo;
}

Status H263VideoDecoder::GetInfo(BaseCodecParams* info)
{
  VideoDecoderParams *pParams;
  if (!m_IsInitBase)
    return UMC_ERR_NOT_INITIALIZED;
  if (info == NULL)
    return UMC_ERR_NULL_PTR;
  pParams = DynamicCast<VideoDecoderParams> (info);
  if (NULL != pParams) {
    *pParams = m_Param;
  } else {
    if (!m_IsInit)
      return UMC_ERR_NOT_INITIALIZED;
  }
  return UMC_OK;
}

Status  H263VideoDecoder::ResetSkipCount()
{
  if (!m_IsInitBase)
    return UMC_ERR_NOT_INITIALIZED;
  m_is_skipped_b = m_skipped_fr = m_b_prev = 0;
  return UMC_OK;
}

Status  H263VideoDecoder::SkipVideoFrame(Ipp32s count)
{
  if (!m_IsInitBase)
    return UMC_ERR_NOT_INITIALIZED;
  if (count < 0) {
    m_is_skipped_b = 0;
    return UMC_OK;
  }
  m_is_skipped_b += count;
  return UMC_OK;
}

Ipp32u H263VideoDecoder::GetNumOfSkippedFrames(void)
{
    return m_skipped_fr;
}

Status H263VideoDecoder::Reset(void)
{
  if (!m_IsInitBase) {
    return UMC_ERR_NOT_INITIALIZED;
  } else if (m_IsInit) {
    m_decInfo->VideoSequence.PicIndex = 0;
    m_decInfo->VideoSequence.Bpics_to_show = m_decInfo->VideoSequence.Ppics_to_show = 0;
    m_decInfo->VideoSequence.prevP_pic_time = m_decInfo->VideoSequence.ref_pic_time = 0;
    m_decInfo->VideoSequence.prevP_temporal_reference = 0;
    m_P_part_of_PB_pending = false;
    m_dec_time_base = -1.0;
    m_dec_time_prev = 0.0;
    m_decInfo->VideoSequence.vFrame = NULL;
  }
  return UMC_OK;
}

Status H263VideoDecoder::GetPerformance(Ipp64f *perf)
{
  return UMC_ERR_NOT_IMPLEMENTED;
}

} // end namespace UMC
