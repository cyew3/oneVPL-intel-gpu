/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include <vector>

#include "vm_sys_info.h"
#include "vm_time.h"
#include "umc_video_data.h"
#include "umc_video_processing.h"
#include "umc_memory_allocator.h"
#include "umc_mpeg2_dec_base.h"
#include "umc_mpeg2_dec.h"
#include "umc_mpeg2_dec_tbl.h"

#include "mfx_trace.h"
#include "mfx_common_int.h"

//#ifdef OTLAD
//FILE *otl = NULL;
//int frame_count = 0;
//int slice_count = 0;
//#endif

//int frame_count = 0;

using namespace UMC;
#define OFFSET_PTR(p, n) (((Ipp8u*)(p) - (Ipp8u*)(0)) & ((n)-1))
#define ALIGN_PTR(p, n) ((Ipp8u*)(p) - OFFSET_PTR(p, n))

const Ipp32s ALIGN_VALUE  = 16;

namespace UMC
{
    VideoDecoder *CreateMPEG2Decoder()
    {
        return new MPEG2VideoDecoder();
    }
}

static void mp2_HuffmanTableFree(mp2_VLCTable *vlc) {
    if (vlc->table0) {
        ippsFree(vlc->table0);
        vlc->table0 = NULL;
    }
    if (vlc->table1) {
        ippsFree(vlc->table1);
        vlc->table1 = NULL;
    }
}

static IppStatus mp2_HuffmanTableInitAlloc_HandleError(Ipp32s *buffer, Ipp16s *table0, Ipp16s *table1)
{
  if (buffer)
    delete[] buffer;
  if (table0)
    ippsFree(table0);
  if (table1)
    ippsFree(table1);
  return ippStsErr;
}

static IppStatus mp2_HuffmanTableInitAlloc(Ipp32s *tbl, Ipp32s bits_table0, mp2_VLCTable *vlc)
{
  Ipp32s *ptbl;
  Ipp16s bad_value = 0;
  Ipp32s max_bits;
  Ipp32s num_tbl;
  Ipp32s i, j, k, m, n;
  Ipp32s bits, code, value;
  Ipp32s bits0, bits1;
  Ipp32s min_value, max_value, spec_value;
  Ipp32s min_code0, min_code1;
  Ipp32s max_code0, max_code1;
  Ipp32s prefix_code1 = -1;
  Ipp32s bits_table1 = 0;
  Ipp32s *buffer = NULL;
  Ipp32s *codes;
  Ipp32s *cbits;
  Ipp32s *values;
  Ipp16s *table0 = NULL;
  Ipp16s *table1 = NULL;

  /* get number of entries (n) */
  max_bits = *tbl++;
  num_tbl = *tbl++;
  for (i = 0; i < num_tbl; i++) {
    *tbl++;
  }
  n = 0;
  ptbl = tbl;
  for (bits = 1; bits <= max_bits; bits++) {
    m = *ptbl;
    if (m < 0) break;
    n += m;
    ptbl += 2*m + 1;
  }

  /* alloc internal table */
  buffer = new Ipp32s[3*n];
  if (!buffer) return ippStsErr;
  codes = buffer;
  cbits = buffer + n;
  values = buffer + 2*n;

  /* read VLC to internal table */
  min_value = 0x7fffffff;
  max_value = 0;
  spec_value = 0;
  ptbl = tbl;
  k = 0;
  for (bits = 1; bits <= max_bits; bits++) {
    if (*ptbl < 0) break;
    m = *ptbl++;
    for (i = 0; i < m; i++) {
      code = *ptbl++;
      value = *ptbl++;
      code &= ((1 << bits) - 1);
      if (value < min_value) min_value = value;
      if (value > max_value) {
        if (!spec_value && value >= 0xffff) {
          spec_value = value;
        } else {
          max_value = value;
        }
      }
#ifdef M2D_DEBUG
      if (vlc_print) {
        for (j = 0; j < bits; j++) {
          printf("%c", (code & (1 << (bits - 1 - j))) ? '1' : '0');
        }
        for (j = bits; j < max_bits; j++) {
          printf("%c", ' ');
        }
        printf("  -> %d", value);
        printf("\n");
      }
#endif
      codes[k] = code << (30 - bits);
      cbits[k] = bits;
      values[k] = value;
      k++;
    }
  }
#ifdef M2D_DEBUG
  if (vlc_print) {
    printf("Values: [%d, %d]", min_value, max_value);
    if (spec_value) printf("   special value = %x\n", spec_value);
    printf("\n");
    printf("---------------------------\n");
  }
#endif

  if (!bits_table0) {
    bits_table0 = max_bits;
    bits_table1 = 0;
    vlc->threshold_table0 = 0;
  }
  bits0 = bits_table0;
  //for (bits0 = 1; bits0 < max_bits; bits0++)
  if (bits0 > 0 && bits0 < max_bits) {
    min_code0 = min_code1 = 0x7fffffff;
    max_code0 = max_code1 = 0;
    for (i = 0; i < n; i++) {
      code = codes[i];
      bits = cbits[i];
      if (bits <= bits0) {
        if (code > max_code0) max_code0 = code;
        if (code < min_code0) min_code0 = code;
      } else {
        if (code > max_code1) max_code1 = code;
        if (code < min_code1) min_code1 = code;
      }
    }
#ifdef M2D_DEBUG
    if (vlc_print) {
      printf("bits0 = %2d: table0 [0x%3x,0x%3x], table1 [0x%3x,0x%3x]",
        bits0,
        max_code0 >> (30 - max_bits),
        min_code0 >> (30 - max_bits),
        max_code1 >> (30 - max_bits),
        min_code1 >> (30 - max_bits));
    }
#endif
    if ((max_code0 < min_code1) || (max_code1 < min_code0)) {
      for (j = 0; j < 29; j++) {
        if ((min_code1 ^ max_code1) & (1 << (29 - j))) break;
      }
      bits1 = max_bits - j;
      if (bits0 == bits_table0) {
        bits_table1 = bits1;
        prefix_code1 = min_code1 >> (30 - bits0);
        vlc->threshold_table0 = min_code0 >> (30 - max_bits);
      }
#ifdef M2D_DEBUG
      if (vlc_print) {
        printf(" # Ok: Tables: %2d + %2d = %d bits", bits0, bits1, bits0 + bits1);
      }
#endif
    }
  }

  if (bits_table0 > 0 && bits_table0 < max_bits && !bits_table1) {
    if (buffer) delete[] buffer;
    return ippStsErr;
  }

  bad_value = (Ipp16s)((bad_value << 8) | VLC_BAD);

  table0 = ippsMalloc_16s(1 << bits_table0);
  if (NULL == table0)
    return mp2_HuffmanTableInitAlloc_HandleError(buffer, table0, table1);

  ippsSet_16s(bad_value, table0, 1 << bits_table0);
  if (bits_table1) {
    table1 = ippsMalloc_16s(1 << bits_table1);
    if (NULL == table1)
      return mp2_HuffmanTableInitAlloc_HandleError(buffer, table0, table1);

    ippsSet_16s(bad_value, table1, 1 << bits_table1);
  }
  for (i = 0; i < n; i++) {
    code = codes[i];
    bits = cbits[i];
    value = values[i];
    if (bits <= bits_table0) {
      code = code >> (30 - bits_table0);
      for (j = 0; j < (1 << (bits_table0 - bits)); j++) {
        table0[code + j] = (Ipp16s)((value << 8) | bits);
      }
    } else if(table1){
      code = code >> (30 - max_bits);
      code = code & ((1 << bits_table1) - 1);
      for (j = 0; j < (1 << (max_bits - bits)); j++) {
        table1[code + j] = (Ipp16s)((value << 8) | bits);
      }
    }
  }

  if (bits_table1) { // fill VLC_NEXTTABLE
    if (prefix_code1 == -1)
      return mp2_HuffmanTableInitAlloc_HandleError(buffer, table0, table1);

    bad_value = (Ipp16s)((bad_value &~ 255) | VLC_NEXTTABLE);
    for (j = 0; j < (1 << ((bits_table0 - (max_bits - bits_table1)))); j++) {
      table0[prefix_code1 + j] = bad_value;
    }
  }

  vlc->max_bits = max_bits;
  vlc->bits_table0 = bits_table0;
  vlc->bits_table1 = bits_table1;
  vlc->table0 = table0;
  vlc->table1 = table1;

  if (buffer) delete[] buffer;
  return ippStsNoErr;
}

#ifdef UMC_VA_DXVA
extern Ipp32u SliceSize(mfxEncryptedData *first, PAVP_COUNTER_TYPE counterMode, Ipp32u &overlap);
extern Ipp32u DistToNextSlice(mfxEncryptedData *encryptedData, PAVP_COUNTER_TYPE counterMode);
#endif


static void ClearUserDataVector(sVideoFrameBuffer::UserDataVector & data)
{
    for (sVideoFrameBuffer::UserDataVector::iterator 
            it = data.begin(), et = data.end(); it != et; ++it)
    {
#if defined(_WIN32) || defined(_WIN64)
        _aligned_free(it->first);
#else
        free(it->first);
#endif
    }

    data.clear();
}


bool MPEG2VideoDecoderBase::InitTables()
{
  if (ippStsNoErr != mp2_HuffmanTableInitAlloc(MBAdressing, 5, &vlcMBAdressing))
    return false;

  if (ippStsNoErr != mp2_HuffmanTableInitAlloc(IMBType, 0, &vlcMBType[0]))
    return false;

  if (ippStsNoErr != mp2_HuffmanTableInitAlloc(PMBType, 0, &vlcMBType[1]))
    return false;

  if (ippStsNoErr != mp2_HuffmanTableInitAlloc(BMBType, 0, &vlcMBType[2]))
    return false;

  if (ippStsNoErr != mp2_HuffmanTableInitAlloc(MBPattern, 5, &vlcMBPattern))
    return false;

  if (ippStsNoErr != mp2_HuffmanTableInitAlloc(MotionVector, 5, &vlcMotionVector))
    return false;

  return true;
}

void MPEG2VideoDecoderBase::DeleteHuffmanTables()
{
    // release tables
    mp2_HuffmanTableFree(&vlcMBAdressing);
    mp2_HuffmanTableFree(&vlcMBType[0]);
    mp2_HuffmanTableFree(&vlcMBType[1]);
    mp2_HuffmanTableFree(&vlcMBType[2]);
    mp2_HuffmanTableFree(&vlcMBPattern);
    mp2_HuffmanTableFree(&vlcMotionVector);
}

bool MPEG2VideoDecoderBase::DeleteTables()
{
  // release tools

    for(int i = 0; i < 2*DPB_SIZE; i++)
    {
        if(Video[i])
        {
            ippsFree(Video[i][0]);
            Video[i][0] = NULL;
            ippsFree(Video[i]);
            Video[i] = NULL;
        }
        
        ClearUserDataVector(frame_buffer.frame_p_c_n[i].user_data_v);
    }
    if(frame_buffer.ptr_context_data)
    {
        //ippsFree(frame_buffer.ptr_context_data);
      m_pMemoryAllocator->Free(frame_buffer.mid_context_data);
      frame_buffer.ptr_context_data = NULL;
    }

    // release tables
    DeleteHuffmanTables();

    return UMC_OK;
}

Status MPEG2VideoDecoderBase::ThreadingSetup(Ipp32s maxThreads)
{
    Ipp32s i,j;
    Ipp32s aligned_size;

    m_nNumberOfThreads = maxThreads;

    if (0 >= m_nNumberOfThreads)
        m_nNumberOfThreads = 1;
    else if (8 < m_nNumberOfThreads)
        m_nNumberOfThreads = 8;

    for(j = 0; j < 2*DPB_SIZE; j++)
    {
        Video[j] = (IppVideoContext**)ippsMalloc_8u(m_nNumberOfThreads*(Ipp32s)sizeof(IppVideoContext*));
        if(!Video[j]) return UMC_ERR_ALLOC;

        aligned_size = (Ipp32s)((sizeof(IppVideoContext) + 15) &~ 15);

        Video[j][0] = (IppVideoContext*)ippsMalloc_8u(m_nNumberOfThreads*aligned_size);
        if(!Video[j][0]) return UMC_ERR_ALLOC;
        memset(Video[j][0], 0, m_nNumberOfThreads*aligned_size);
        for(i = 0; i < m_nNumberOfThreads; i++)
        {
          if (i)
            Video[j][i] = (IppVideoContext*)((Ipp8u *)Video[j][0] + i*aligned_size);

          // Intra&inter spec
          memset(&Video[j][i]->decodeIntraSpec, 0, sizeof(IppiDecodeIntraSpec_MPEG2));
          memset(&Video[j][i]->decodeInterSpec, 0, sizeof(IppiDecodeInterSpec_MPEG2));
          ippiDecodeInterInit_MPEG2(NULL, IPPVC_MPEG1_STREAM, &Video[j][i]->decodeInterSpec);
          Video[j][i]->decodeInterSpecChroma = Video[j][i]->decodeInterSpec;
          Video[j][i]->decodeInterSpec.idxLastNonZero = 63;
          Video[j][i]->decodeIntraSpec.intraVLCFormat = PictureHeader[0].intra_vlc_format;
          Video[j][i]->decodeIntraSpec.intraShiftDC = PictureHeader[0].curr_intra_dc_multi;
        }
        memset(&m_Spec.decodeIntraSpec, 0, sizeof(IppiDecodeIntraSpec_MPEG2));
        memset(&m_Spec.decodeInterSpec, 0, sizeof(IppiDecodeInterSpec_MPEG2));
        ippiDecodeInterInit_MPEG2(NULL, IPPVC_MPEG1_STREAM, &m_Spec.decodeInterSpec);
        m_Spec.decodeInterSpecChroma = m_Spec.decodeInterSpec;
        m_Spec.decodeInterSpec.idxLastNonZero = 63;
        m_Spec.decodeIntraSpec.intraVLCFormat = PictureHeader[0].intra_vlc_format;
        m_Spec.decodeIntraSpec.intraShiftDC = PictureHeader[0].curr_intra_dc_multi;
    }
    m_nNumberOfAllocatedThreads = m_nNumberOfThreads;

    return UMC_OK;
}


Status MPEG2VideoDecoderBase::Init(BaseCodecParams *pInit)
{
    MediaData *data;
    Status ret;
    int i;

    VideoDecoderParams *init = DynamicCast<VideoDecoderParams, BaseCodecParams>(pInit);
    if(!init)
        return UMC_ERR_INIT;

    data = init->m_pData;
    if (data != 0 && data->GetDataSize() == 0)
      data = 0;

    // checks or create memory allocator;
    ret = BaseCodec::Init(pInit);
    if(ret != UMC_OK)
      return ret;

    if((init->lFlags & FLAG_VDEC_4BYTE_ACCESS) != 0)
      return UMC_ERR_UNSUPPORTED;

    Reset();

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
    if(!pack_w.SetVideoAccelerator(init->pVideoAccelerator))
        return UMC_ERR_UNSUPPORTED;
#endif

    m_ClipInfo = init->info;
    m_lFlags = init->lFlags;
    m_PostProcessing = init->pPostProcessing;
    //m_bExternalSurface = (init->lFlags & FLAG_VDEC_EXTERNAL_SURFACE_USE) != 0;

    // creates thread's structures (Video[])

    if(UMC_OK != ThreadingSetup(init->numThreads))
        return UMC_ERR_ALLOC;

    if (0 != init->info.framerate)
    {
        m_isFrameRateFromInit = true;

        sequenceHeader.delta_frame_time = init->info.framerate;
    }

    if (0 != init->info.aspect_ratio_width && 0 != init->info.aspect_ratio_height)
    {
        m_isAspectRatioFromInit = true;
    }

    if (data != 0) {
      PrepareBuffer(data, 0);

      // get sequence header start code
      // search while not found or EOD
      if(UMC_OK != FindSequenceHeader(Video[0][0]))
        return (UMC_ERR_SYNC);

      if(UMC_OK != DecodeSequenceHeader(Video[0][0], 0))
          return (UMC_ERR_INVALID_STREAM);

      FlushBuffer(data, 0);//task_num = 0
    } else { // get stream parameters from sVideoInfo
      if(UMC_OK != DecodeSequenceHeader( 0 , 0))
        return (UMC_ERR_INVALID_PARAMS);

      // work-arounf for mpeg1
      m_ClipInfo.stream_type = UNDEF_VIDEO;
    }
/*
    m_ccCurrData.SetBufferPointer(NULL, 0);
    m_pCCData = new SampleBuffer;
    if(m_pCCData)
    {
        Status res = UMC_OK;
        MediaBufferParams initPar;
        initPar.m_prefInputBufferSize = initPar.m_prefOutputBufferSize = 1024;
        initPar.m_numberOfFrames = 20;
        initPar.m_pMemoryAllocator = m_pMemoryAllocator;

        res = m_pCCData->Init(&initPar);
        if(res != UMC_OK)
        {
            m_pCCData->Close();
            delete m_pCCData;
            m_pCCData = NULL;
        }
    }

    m_ccCurrDataTS.SetBufferPointer(NULL, 0);
    m_pCCDataTS = new SampleBuffer;
    if(m_pCCDataTS)
    {
        Status res = UMC_OK;
        MediaBufferParams initPar;
        initPar.m_prefInputBufferSize = initPar.m_prefOutputBufferSize = 10;
        initPar.m_numberOfFrames = 20;
        initPar.m_pMemoryAllocator = m_pMemoryAllocator;

        res = m_pCCDataTS->Init(&initPar);
        if(res != UMC_OK)
        {
            m_pCCDataTS->Close();
            delete m_pCCDataTS;
            m_pCCDataTS = NULL;
        }
    }
*/

    if (false == InitTables())
        return UMC_ERR_INIT;

    sequenceHeader.frame_count    = 0;
    sequenceHeader.stream_time    = 0;

    sequenceHeader.num_of_skipped   = 0;

    frame_buffer.latest_prev =  -1;
    frame_buffer.common_curr_index = -1;
    frame_buffer.latest_next = -1;
    frame_buffer.retrieve = -1;

    for(i = 0; i < 2*DPB_SIZE; i++)
    {
        frame_buffer.ret_array[i] = -1;
        frame_buffer.curr_index[i] =  -1;
        frame_buffer.field_buffer_index[i]  = 0;
        frame_buffer.frame_p_c_n[i].frame_time = -1;
        frame_buffer.frame_p_c_n[i].duration = 0;
        frame_buffer.frame_p_c_n[i].IsUserDataDecoded = false;
        frame_buffer.frame_p_c_n[i].us_data_size = 0;        
        ClearUserDataVector(frame_buffer.frame_p_c_n[i].user_data_v);

        frame_buffer.frame_p_c_n[i].va_index = -1;
        frame_buffer.task_locked[i]          = -1;
        m_dTime[i].time = -1.;
        m_dTime[i].isOriginal = false;
        m_nNumberOfThreadsTask[i] = m_nNumberOfThreads;
    }
    m_dTimeCalc     = -1.;

    frame_buffer.ret_array_curr = 0;
    frame_buffer.ret_index = -1;
    frame_buffer.ret_array_free = 0;
    frame_buffer.ret_array_len = 0;

    sequenceHeader.stream_time_temporal_reference = -1;
    sequenceHeader.stream_time = 0;
    m_SkipLevel = SKIP_NONE;
    m_IsSHDecoded = false;
    m_FirstHeaderAfterSequenceHeaderParsed = false;
    m_IsDataFirst = true;
    m_InputTime = -1.;

    m_picture_coding_type_save = MPEG2_I_PICTURE;
    m_IsLastFrameProcessed = false;

    m_InitClipInfo = m_ClipInfo;
    /*
//for user data:

  Ipp32s buff_size = 1024;
  Ipp32s size = 2*DPB_SIZE*buff_size;
  if(UMC_OK != m_pMemoryAllocator->Alloc(&frame_buffer.mid_context_data,
    size, UMC_ALLOC_PERSISTENT, ALIGN_VALUE)) {
    vm_debug_trace(VM_DEBUG_ERROR, VM_STRING("External allocation failed\n"));
    return UMC_ERR_ALLOC;
  }

  frame_buffer.ptr_context_data = (Ipp8u*)m_pMemoryAllocator->Lock(frame_buffer.mid_context_data);
  if(!frame_buffer.ptr_context_data) {
    vm_debug_trace(VM_DEBUG_ERROR, VM_STRING("External Lock failed\n"));
    return UMC_ERR_ALLOC;
  }

  Ipp8u* ptr = frame_buffer.ptr_context_data;
  ptr = align_pointer<Ipp8u*>(ptr, ALIGN_VALUE);

  for (i = 0; i < DPB_SIZE*2; i++) {
    frame_buffer.frame_p_c_n[i].user_data = ptr;
    frame_buffer.frame_p_c_n[i].us_buf_size = buff_size;
    frame_buffer.frame_p_c_n[i].us_data_size = 0;
    ptr += buff_size;
  }

  if (UMC_OK != m_pMemoryAllocator->Unlock(frame_buffer.mid_context_data)) {
    vm_debug_trace(VM_DEBUG_ERROR, VM_STRING("External Unlock failed\n"));
  }
  */

    return UMC_OK;
}


Status MPEG2VideoDecoderBase::FlushBuffer(MediaData* data, int task_num)
{
    if(task_num >= DPB_SIZE)
        return UMC_ERR_FAILED;
    IppVideoContext *video = Video[task_num][0]; // it decodes tail of frame
    Ipp8u* plast = GET_BYTE_PTR(video->bs);
    Ipp8u* pend = GET_END_PTR(video->bs);
    if (plast > pend)
        plast = pend;
    return data->MoveDataPointer((Ipp32s)(plast - (Ipp8u*)data->GetDataPointer()));
}


Status MPEG2VideoDecoderBase::PrepareBuffer(MediaData* data, int task_num)
{
    IppVideoContext *video = Video[task_num][0];
    Ipp8u  *ptr;
    size_t size;

    if (data == 0) {
      return UMC_OK;
    }
    ptr = (Ipp8u*)data->GetDataPointer();
    size = data->GetDataSize();

    INIT_BITSTREAM(video->bs, ptr, ptr + size);

    return UMC_OK;
}

bool MPEG2VideoDecoderBase::IsPictureToSkip(int task_num)
{
  if(frame_buffer.field_buffer_index[task_num] == 0) // not for second field
  {
   // sequenceHeader.stream_time_temporal_reference++;
    if(PictureHeader[task_num].picture_coding_type == MPEG2_I_PICTURE)
    {
      if(sequenceHeader.first_i_occure)
        sequenceHeader.first_p_occure = 1;
      sequenceHeader.first_i_occure = 1;
      if(m_SkipLevel == SKIP_ALL)
      {
          sequenceHeader.num_of_skipped++;
          return true;
      }
    }
    else if(!sequenceHeader.first_i_occure)
    {
        // To support only P frames streams this check is disabled
        //sequenceHeader.num_of_skipped++;
        //return true;
    }
    else if(PictureHeader[task_num].picture_coding_type == MPEG2_P_PICTURE) {
      sequenceHeader.first_p_occure = 1;
      if(m_SkipLevel >= SKIP_PB)
      {
          sequenceHeader.num_of_skipped++;
          return true;
      }
    }
    else if(PictureHeader[task_num].picture_coding_type == MPEG2_B_PICTURE) {
      /*
      //fix for VCSD100019408. Don't skip B frame with no forward prediction
      if(!sequenceHeader.first_p_occure &&
        (!sequenceHeader.closed_gop || sequenceHeader.broken_link ))
      {
          sequenceHeader.num_of_skipped++;
          return true;
      }
      //end of fix for VCSD100019408 */
      if(m_SkipLevel >= SKIP_B)
      {
          sequenceHeader.num_of_skipped++;
          return true;
      }
    }
  }

  return false;
}

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
Status MPEG2VideoDecoderBase::BeginVAFrame(int task_num)
#else
Status MPEG2VideoDecoderBase::BeginVAFrame(int)
#endif
{
#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
    int curr_index;
    PackVA *pack = &pack_w;
#ifdef UMC_VA_DXVA
    int size_bs = 0;
    int size_sl = 0;
#elif defined UMC_VA_LINUX
    int size_bs = GET_END_PTR(Video[task_num][0]->bs) - GET_START_PTR(Video[task_num][0]->bs)+4;
    int size_sl = m_ClipInfo.clip_info.width*m_ClipInfo.clip_info.height/256;
#endif

    if (pack->va_mode == VA_NO)
    {
        return UMC_OK;
    }

    {
        curr_index = frame_buffer.curr_index[task_num];
    }

    pack->m_SliceNum = 0;
    frame_buffer.frame_p_c_n[curr_index].va_index = pack->va_index;

    pack->field_buffer_index = frame_buffer.field_buffer_index[task_num];

    return pack->InitBuffers(size_bs, size_sl); //was "size" only
#else
    return UMC_OK;
#endif
}

//$$$$$$$$$$$$$$$$
//additional functions for UMC-MFX interaction
Status MPEG2VideoDecoderBase::GetCCData(Ipp8u* ptr, Ipp32u *size, Ipp64u *time, Ipp32u bufsize)
{
    Status umcRes = UMC_OK;
    
    /*
    MediaData ccData;

    umcRes = m_pCCDataTS->LockOutputBuffer(&ccData);
    if(umcRes != UMC_OK)
    {
        *size = 0;
        *time = 0;
        return UMC_OK;
    }
    Ipp32s sz = (Ipp32s)ccData.GetDataSize();

    if(sz > 0)
        ippsCopy_8u((Ipp8u*)ccData.GetDataPointer(), (Ipp8u*)time, sz);

    ccData.SetDataSize(0);
    umcRes = m_pCCDataTS->UnLockOutputBuffer(&ccData);
    
    umcRes = m_pCCData->LockOutputBuffer(&ccData);
    if(umcRes != UMC_OK)
    {
        *size = 0;
        *time = 0;
        return UMC_OK;
    }
    *size = (Ipp32s)ccData.GetDataSize();

    if(*size > 0)
    {
        if(bufsize < *size)
        {
            return UMC_ERR_NOT_ENOUGH_BUFFER;
        }
        ippsCopy_8u((Ipp8u*)ccData.GetDataPointer(), ptr, *size);
    }

    //*time = (Ipp64u)ccData.GetTime();
    *size *= 8;//in bits

    ccData.SetDataSize(0);
    umcRes = m_pCCData->UnLockOutputBuffer(&ccData);
    */

    if (0 == m_user_ts_data.size())
    {
        *size = 0;
        *time = 0;

        return UMC_OK;
    }

    if (0 < m_user_ts_data.front().second)
    {
        ippsCopy_8u((Ipp8u *)&m_user_ts_data.front().first, (Ipp8u *)time, (Ipp32u)m_user_ts_data.front().second);
    }

    m_user_ts_data.erase(m_user_ts_data.begin());

    if (0 == m_user_data.size())
    {
        *size = 0;
        *time = 0;

        return UMC_OK;
    }

    Ipp8u *p_user_data = m_user_data.front().first;
    Ipp32u user_data_size = (Ipp32u) m_user_data.front().second;
    m_user_data.erase(m_user_data.begin());

    *size = user_data_size;

    if (*size <= 0 || p_user_data == NULL)
    {
        *size = 0;
        *time = 0;
        return UMC_OK;
    }

    if (bufsize < *size)
    {
        return UMC_ERR_NOT_ENOUGH_BUFFER;
    }

    ippsCopy_8u(p_user_data, ptr, *size);
    *size *= 8;

#if defined(_WIN32) || defined(_WIN64)
    _aligned_free(p_user_data);
#else
    free(p_user_data);
#endif

    return umcRes;
}
Status MPEG2VideoDecoderBase::GetPictureHeader(MediaData* input, int task_num, int prev_task_num)
{
  Status umcRes = UMC_OK;

  m_IsFrameSkipped = false;
  m_IsSHDecoded = false;

  if(task_num >= DPB_SIZE)
      frame_buffer.field_buffer_index[task_num] = 1;
  else
      frame_buffer.field_buffer_index[task_num] = 0;

  if(prev_task_num != -1)
  {
      sequenceHeader.mb_width[task_num] = sHSaved.mb_width;
      sequenceHeader.mb_height[task_num] = sHSaved.mb_height;
      sequenceHeader.numMB[task_num] = sHSaved.numMB;
      sequenceHeader.extension_start_code_ID[task_num] = sHSaved.extension_start_code_ID;
      sequenceHeader.scalable_mode[task_num] = sHSaved.scalable_mode;
  }

  frame_buffer.frame_p_c_n[task_num].IsUserDataDecoded = false;
  frame_buffer.frame_p_c_n[task_num].us_data_size = 0;

  if(input == NULL)
  {
      //if (m_lFlags & FLAG_VDEC_REORDER)
      {
        if(!m_IsLastFrameProcessed)
        {
            Ipp64f currentTime = -1;
            bool isOriginal = false;
            frame_buffer.ret_index = frame_buffer.latest_next;
            frame_buffer.curr_index[task_num] = frame_buffer.latest_next;
            if(frame_buffer.latest_next >= 0)
                CalculateFrameTime(currentTime, &m_dTime[frame_buffer.ret_index].time, &isOriginal, task_num, true);
            frame_buffer.ret_index = -1;
            if(frame_buffer.latest_next >= 0)
            {
                frame_buffer.ret_array[frame_buffer.ret_array_free] = frame_buffer.latest_next;
                frame_buffer.ret_index = frame_buffer.latest_next;
                frame_buffer.ret_array_free++;
                if(frame_buffer.ret_array_free >= DPB_SIZE)
                    frame_buffer.ret_array_free = 0;
                frame_buffer.ret_array_len++;
            }
        }
        m_IsLastFrameProcessed = true;
      }
      return UMC_OK;
  }

  PrepareBuffer(input, task_num);
  IppVideoContext  *video = Video[task_num][0];

  Ipp32u code;

#ifdef UMC_STREAM_ANALYZER
   Ipp32s bit_ptr;
#endif

  if (!sequenceHeader.is_decoded) {
    if(UMC_OK != FindSequenceHeader(Video[task_num][0]))
      return (UMC_ERR_NOT_ENOUGH_DATA);
    umcRes = DecodeSequenceHeader(Video[task_num][0], task_num);
    if(UMC_OK != umcRes)
      return umcRes;
  }


  SHOW_BITS(video->bs, 32, code);
  do {
    GET_START_CODE(video->bs, code);
    // some headers are possible here
    if(code == (Ipp32u)UMC_ERR_NOT_ENOUGH_DATA)
      if(GET_OFFSET(video->bs) > 0) // some data was decoded
        return UMC_ERR_NOT_ENOUGH_DATA;
      else                          // no start codes found
        return UMC_ERR_SYNC;
    if(code == SEQUENCE_END_CODE)
      continue;
    if(code != PICTURE_START_CODE) {
      if (DecodeHeader(code, video, task_num) == UMC_ERR_NOT_ENOUGH_DATA) {
        return (UMC_ERR_NOT_ENOUGH_DATA);
      }
    }
  } while (code != PICTURE_START_CODE);

  umcRes = DecodeHeader(PICTURE_START_CODE, video, task_num);
  if(umcRes != UMC_OK)
      return umcRes;

  if(!m_IsFrameSkipped)
  {
      Ipp64f currentTime = input->GetTime();
      bool isOriginal = false;
      CalculateFrameTime(currentTime, &m_dTimeCalc, &isOriginal, task_num, false);

      if (0 <= frame_buffer.ret_index)
      {
          m_dTime[frame_buffer.ret_index].time = m_dTimeCalc;
          m_dTime[frame_buffer.ret_index].isOriginal = isOriginal;
      }

      frame_buffer.ret_index = -1;
  }

   if(m_IsDataFirst && PictureHeader[task_num].picture_coding_type != MPEG2_I_PICTURE)
   {
      m_IsDataFirst = false;
      // To support only P frames streams this check is disabled
      // return UMC_WRN_INVALID_STREAM;
   }
   m_IsDataFirst = false;

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
  pack_w.IsFrameSkipped = false;
#endif
  if (m_IsFrameSkipped)
  {

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
    pack_w.IsFrameSkipped = true;
#endif

    return UMC_ERR_NOT_ENOUGH_DATA;
  }//if (m_IsFrameSkipped)

  return (umcRes);
}

Status MPEG2VideoDecoderBase::DoDecodeSlices(Ipp32s threadID, int task_num)
{
    return DecodeSlices(threadID, task_num);
}

void MPEG2VideoDecoderBase::SetOutputPointers(MediaData *output, int task_num)
{
    VideoData *out_data = DynamicCast<VideoData, MediaData>(output);
    Ipp32s curr_index = frame_buffer.curr_index[task_num];
    Ipp32s pitch_l, pitch_c;

    Ipp32u threadsNum = GetCurrThreadsNum(task_num);

    if(frame_buffer.field_buffer_index[task_num] == 1)
    {
        if(curr_index < DPB_SIZE)
            return;
        frame_buffer.frame_p_c_n[curr_index].Y_comp_data = frame_buffer.frame_p_c_n[curr_index-DPB_SIZE].Y_comp_data;
        frame_buffer.frame_p_c_n[curr_index].U_comp_data = frame_buffer.frame_p_c_n[curr_index-DPB_SIZE].U_comp_data;
        frame_buffer.frame_p_c_n[curr_index].V_comp_data = frame_buffer.frame_p_c_n[curr_index-DPB_SIZE].V_comp_data;
    //    frame_buffer.frame_p_c_n[curr_index].IsUserDataDecoded = false;
     //   frame_buffer.frame_p_c_n[curr_index].us_data_size = 0;
        pitch_l = frame_buffer.Y_comp_pitch;
        pitch_c = frame_buffer.U_comp_pitch;

        frame_buffer.frame_p_c_n[curr_index].isCorrupted = false;

        for (Ipp32u i = 0; i < threadsNum; i += 1)
        {
            IppVideoContext* video = Video[task_num][i];

            video->Y_comp_pitch = pitch_l;
            video->U_comp_pitch = pitch_c;
            video->V_comp_pitch = pitch_c;

            video->Y_comp_height = out_data->GetHeight();

            video->pic_size = sequenceHeader.mb_height[task_num]*16*pitch_l;

            video->blkOffsets[0][0] = 0;
            video->blkOffsets[0][1] = 8;
            video->blkOffsets[0][2] = 8*pitch_l;
            video->blkOffsets[0][3] = 8*pitch_l + 8;
            video->blkOffsets[0][4] = 0;
            video->blkOffsets[0][5] = 0;
            video->blkOffsets[0][6] = 8*pitch_c;
            video->blkOffsets[0][7] = 8*pitch_c;
            video->blkOffsets[1][0] = 0;
            video->blkOffsets[1][1] = 8;
            video->blkOffsets[1][2] = pitch_l;
            video->blkOffsets[1][3] = pitch_l + 8;
            video->blkOffsets[1][4] = 0;
            video->blkOffsets[1][5] = 0;
            video->blkOffsets[1][6] = pitch_c;
            video->blkOffsets[1][7] = pitch_c;
            video->blkOffsets[2][0] = 0;
            video->blkOffsets[2][1] = 8;
            video->blkOffsets[2][2] = 16*pitch_l;
            video->blkOffsets[2][3] = 16*pitch_l + 8;
            video->blkOffsets[2][4] = 0;
            video->blkOffsets[2][5] = 0;
            video->blkOffsets[2][6] = 16*pitch_c;
            video->blkOffsets[2][7] = 16*pitch_c;

            video->blkPitches[0][0] = pitch_l;
            video->blkPitches[0][1] = pitch_c;
            video->blkPitches[1][0] = 2*pitch_l;
            video->blkPitches[1][1] = pitch_c;
            video->blkPitches[2][0] = 2*pitch_l;
            video->blkPitches[2][1] = 2*pitch_c;
        }

        return;
    }
    else
    {
        if(curr_index >= DPB_SIZE)
            return;
        frame_buffer.frame_p_c_n[curr_index].Y_comp_data = (Ipp8u*)out_data->GetPlanePointer(0);
        frame_buffer.frame_p_c_n[curr_index].U_comp_data = (Ipp8u*)out_data->GetPlanePointer(1);
        frame_buffer.frame_p_c_n[curr_index].V_comp_data = (Ipp8u*)out_data->GetPlanePointer(2);
      //  frame_buffer.frame_p_c_n[curr_index].IsUserDataDecoded = false;
      //  frame_buffer.frame_p_c_n[curr_index].us_data_size = 0;
        pitch_l = (Ipp32s)out_data->GetPlanePitch(0);
        pitch_c = pitch_l >> 1;
        frame_buffer.Y_comp_pitch = pitch_l;
        frame_buffer.U_comp_pitch = pitch_c;
        frame_buffer.V_comp_pitch = pitch_c;
        frame_buffer.Y_comp_height = out_data->GetHeight();
        Ipp32s l_size, c_size;
        //frame_buffer.pic_size = (frame_buffer.Y_comp_pitch*frame_buffer.Y_comp_height*3)/2;
        frame_buffer.pic_size = l_size = sequenceHeader.mb_height[task_num]*16*pitch_l;

// ------------------------------------------------

        for (Ipp32u i = 0; i < threadsNum; i += 1)
        {
            IppVideoContext* video = Video[task_num][i];

            video->Y_comp_pitch = pitch_l;
            video->U_comp_pitch = pitch_c;
            video->V_comp_pitch = pitch_c;
            video->Y_comp_height = out_data->GetHeight();

            video->pic_size = sequenceHeader.mb_height[task_num]*16*pitch_l;
        }
// ------------------------------------------------

        c_size = sequenceHeader.mb_height[task_num]*8*pitch_c;
        if (m_ClipInfo.color_format == YUV422)
            c_size *= 2;
        else if(m_ClipInfo.color_format == YUV444)
            c_size = l_size;

        memset(frame_buffer.frame_p_c_n[curr_index].Y_comp_data,0,l_size);
        memset(frame_buffer.frame_p_c_n[curr_index].U_comp_data,0,c_size);
        memset(frame_buffer.frame_p_c_n[curr_index].V_comp_data,0,c_size);
    }


// ------------------------------------------------

    for (Ipp32u i = 0; i < threadsNum; i += 1)
    {
        IppVideoContext* video = Video[task_num][i];

        video->blkOffsets[0][0] = 0;
        video->blkOffsets[0][1] = 8;
        video->blkOffsets[0][2] = 8*pitch_l;
        video->blkOffsets[0][3] = 8*pitch_l + 8;
        video->blkOffsets[0][4] = 0;
        video->blkOffsets[0][5] = 0;
        video->blkOffsets[0][6] = 8*pitch_c;
        video->blkOffsets[0][7] = 8*pitch_c;
        video->blkOffsets[1][0] = 0;
        video->blkOffsets[1][1] = 8;
        video->blkOffsets[1][2] = pitch_l;
        video->blkOffsets[1][3] = pitch_l + 8;
        video->blkOffsets[1][4] = 0;
        video->blkOffsets[1][5] = 0;
        video->blkOffsets[1][6] = pitch_c;
        video->blkOffsets[1][7] = pitch_c;
        video->blkOffsets[2][0] = 0;
        video->blkOffsets[2][1] = 8;
        video->blkOffsets[2][2] = 16*pitch_l;
        video->blkOffsets[2][3] = 16*pitch_l + 8;
        video->blkOffsets[2][4] = 0;
        video->blkOffsets[2][5] = 0;
        video->blkOffsets[2][6] = 16*pitch_c;
        video->blkOffsets[2][7] = 16*pitch_c;

        video->blkPitches[0][0] = pitch_l;
        video->blkPitches[0][1] = pitch_c;
        video->blkPitches[1][0] = 2*pitch_l;
        video->blkPitches[1][1] = pitch_c;
        video->blkPitches[2][0] = 2*pitch_l;
        video->blkPitches[2][1] = 2*pitch_c;
    }
}


Status MPEG2VideoDecoderBase::ProcessRestFrame(int task_num)
{
    Status umcRes = UMC_OK;
    IppVideoContext* video = Video[task_num][0];
    Ipp32s i;

    #if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
    if (pack_w.m_va && pack_w.bNeedNewFrame)
    {
        umcRes = BeginVAFrame(task_num);
        if (UMC_OK != umcRes)
            return umcRes;
        pack_w.bNeedNewFrame = false;
    }
    #endif


#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
    if (pack_w.m_va != NULL)
    {
        Video[task_num][0]->stream_type = m_ClipInfo.stream_type;
        Video[task_num][0]->color_format = m_ClipInfo.color_format;
        Video[task_num][0]->clip_info_width = m_ClipInfo.clip_info.width;
        Video[task_num][0]->clip_info_height = m_ClipInfo.clip_info.height;
    }
    else//if (pack_w.m_va == NULL)
#endif
    {
        for (i = 0; i < m_nNumberOfThreads; i++)
        {
            int j;

            //Intra
            Video[task_num][i]->decodeIntraSpec.intraVLCFormat = m_Spec.decodeIntraSpec.intraVLCFormat;
            Video[task_num][i]->decodeIntraSpec.intraShiftDC = m_Spec.decodeIntraSpec.intraShiftDC;

            Video[task_num][i]->stream_type = m_ClipInfo.stream_type;
            Video[task_num][i]->color_format = m_ClipInfo.color_format;
            Video[task_num][i]->clip_info_width = m_ClipInfo.clip_info.width;
            Video[task_num][i]->clip_info_height = m_ClipInfo.clip_info.height;

            for (j = 0; j < 64; j++)
                Video[task_num][i]->decodeIntraSpec._quantMatrix[j] = m_Spec.decodeIntraSpec._quantMatrix[j];

            if (m_Spec.decodeIntraSpec.quantMatrix)
            {
                Video[task_num][i]->decodeIntraSpec.quantMatrix = Video[task_num][i]->decodeIntraSpec._quantMatrix;
            }
            else
            {
                Video[task_num][i]->decodeIntraSpec.quantMatrix = NULL;
            }

            Video[task_num][i]->decodeIntraSpec.scanMatrix = m_Spec.decodeIntraSpec.scanMatrix;

            //IntraChroma
            Video[task_num][i]->decodeIntraSpecChroma.intraVLCFormat = m_Spec.decodeIntraSpecChroma.intraVLCFormat;
            Video[task_num][i]->decodeIntraSpecChroma.intraShiftDC = m_Spec.decodeIntraSpecChroma.intraShiftDC;

            for (j = 0; j < 64; j++)
                Video[task_num][i]->decodeIntraSpecChroma._quantMatrix[j] = m_Spec.decodeIntraSpecChroma._quantMatrix[j];

            if (m_Spec.decodeIntraSpecChroma.quantMatrix)
            {
                Video[task_num][i]->decodeIntraSpecChroma.quantMatrix = Video[task_num][i]->decodeIntraSpecChroma._quantMatrix;
            }
            else
            {
                Video[task_num][i]->decodeIntraSpecChroma.quantMatrix = NULL;
            }

            Video[task_num][i]->decodeIntraSpecChroma.scanMatrix = m_Spec.decodeIntraSpecChroma.scanMatrix;

            //Inter
            for (j = 0; j < 64; j++)
                Video[task_num][i]->decodeInterSpec._quantMatrix[j] = m_Spec.decodeInterSpec._quantMatrix[j];

            if (m_Spec.decodeInterSpec.quantMatrix)
            {
                Video[task_num][i]->decodeInterSpec.quantMatrix = Video[task_num][i]->decodeInterSpec._quantMatrix;
            }
            else
            {
                Video[task_num][i]->decodeInterSpec.quantMatrix = NULL;
            }

            Video[task_num][i]->decodeInterSpec.scanMatrix = m_Spec.decodeInterSpec.scanMatrix;

            //InterChroma
            for (j = 0; j < 64; j++)
                Video[task_num][i]->decodeInterSpecChroma._quantMatrix[j] = m_Spec.decodeInterSpecChroma._quantMatrix[j];

            if (m_Spec.decodeInterSpecChroma.quantMatrix)
            {
                Video[task_num][i]->decodeInterSpecChroma.quantMatrix = Video[task_num][i]->decodeInterSpecChroma._quantMatrix;
            }
            else
            {
                Video[task_num][i]->decodeInterSpecChroma.quantMatrix = NULL;
            }

            Video[task_num][i]->decodeInterSpecChroma.scanMatrix = m_Spec.decodeInterSpecChroma.scanMatrix;

        } // for
    }

    Video[task_num][0]->slice_vertical_position = 1;

    m_nNumberOfThreadsTask[task_num] = m_nNumberOfThreads;

    sHSaved.mb_width =  sequenceHeader.mb_width[task_num];
    sHSaved.mb_height =  sequenceHeader.mb_height[task_num];
    sHSaved.numMB = sequenceHeader.numMB[task_num];
    sHSaved.extension_start_code_ID =  sequenceHeader.extension_start_code_ID[task_num];
    sHSaved.scalable_mode = sequenceHeader.scalable_mode[task_num];

    if (m_nNumberOfThreadsTask[task_num] > 1)
    {
        #define MAX_START_CODES 1024

        Ipp8u *start_ptr = GET_BYTE_PTR(video->bs);
        Ipp8u *end_ptr = GET_END_PTR(video->bs)-3;
        Ipp8u *ptr = start_ptr, *ptrz = start_ptr;
        Ipp8u *prev_ptr;
        Ipp32s curr_thread;
        Ipp32s len = (Ipp32s)(end_ptr - start_ptr);
        Ipp32s j, start_count = 0;
        Ipp32s start_pos[MAX_START_CODES];

        memset(start_pos,0,sizeof(Ipp32s)*MAX_START_CODES);

        for (start_count = 0; start_count < MAX_START_CODES; start_count++)
        {
            Ipp32s code;

            do
            {
                while (ptr < end_ptr && (ptr[0] || ptr[1] || ptr[2] > 1)) ptr++;

                ptrz = ptr;

                while (ptr < end_ptr && !ptr[2]) ptr++;
            }

            while (ptr < end_ptr && ptr[2] != 1);

            if (ptr >= end_ptr)
            {
                ptr = GET_END_PTR(video->bs);
                break;
            }

            code = ptr[3];

            if (code > 0 && code < 0xb0)
            {
                // start of slice
                start_pos[start_count] = (Ipp32s)(ptrz - start_ptr);
                ptr += 4;
            }
            else
                break;
        }

        if (start_count == MAX_START_CODES)
        {
            while (ptr < end_ptr && (ptr[0] || ptr[1] || ptr[2] != 1 || (ptr[3] > 0 && ptr[3] < 0xb0)))
                ptr++;

            ptrz = ptr;
        }

        len = (Ipp32s)(ptrz - start_ptr);

        prev_ptr = start_ptr;

        curr_thread = 1; // 0th will be last

        Ipp32s taskNumThreads = m_nNumberOfThreadsTask[task_num];

        for (i = 0, j = 0; i < taskNumThreads; i++)
        {
            Ipp32s approx = len * (i + 1) / m_nNumberOfThreadsTask[task_num];

            if (start_pos[j] > approx)
            {
                m_nNumberOfThreadsTask[task_num] -= 1; // no data for thread - covered by previous
                continue;
            }

            while (j < start_count && start_pos[j] < approx)
                j++;

            if(j == start_count)
            {
                // it will be last thread -> to 0th
                SET_PTR(Video[task_num][0]->bs, prev_ptr)
                m_nNumberOfThreadsTask[task_num] = curr_thread;
                break;
            }

            INIT_BITSTREAM(Video[task_num][curr_thread]->bs, prev_ptr, start_ptr + start_pos[j]);

            curr_thread++;
            prev_ptr = start_ptr + start_pos[j];
        }

        INIT_BITSTREAM(Video[task_num][0]->bs, prev_ptr, end_ptr + 3);
    }

    return umcRes;
}

Status MPEG2VideoDecoderBase::PostProcessUserData(int display_index)
{
    Status umcRes = UMC_OK;

    if(frame_buffer.frame_p_c_n[display_index].IsUserDataDecoded)
    {
      MediaData ccData;
     // if(frame_buffer.curr_index[task_num] != display_index)
      {
          /*
          if(m_pCCData->LockInputBuffer(&ccData) == UMC_OK)
          {
              Ipp8u* ptr, *p;
              Ipp32s size, s;
              p  = (Ipp8u*)ccData.GetDataPointer();
              s = (Ipp32s)ccData.GetDataSize();
              ptr = p + s;

              p = (Ipp8u*)ccData.GetBufferPointer();
              s = (Ipp32s)ccData.GetBufferSize();
              size = (Ipp32s)(p + s - ptr);

              if(0 != ptr && size >= frame_buffer.frame_p_c_n[display_index].us_data_size)
              {
                  ippsCopy_8u((Ipp8u*)frame_buffer.frame_p_c_n[display_index].user_data,
                               ptr, 
                               frame_buffer.frame_p_c_n[display_index].us_data_size);

                 ccData.SetDataSize(ccData.GetDataSize() + frame_buffer.frame_p_c_n[display_index].us_data_size);

                 umcRes = m_pCCData->UnLockInputBuffer(&ccData);

                 frame_buffer.frame_p_c_n[display_index].us_data_size = 0;
              }
          }
            */

          // TODO: implement payload limitation according to spec when it be ready. Currently ~10 seconds
          if (m_user_data.size() >= 300) 
          {
              // assert(m_user_data.size() == m_user_ts_data.size());
              int items_to_discard = (int)m_user_data.size() - 300;
              sVideoFrameBuffer::UserDataVector tmpvec(m_user_data.begin(), m_user_data.begin() + items_to_discard);
              ClearUserDataVector(tmpvec);
              m_user_data.erase(m_user_data.begin(), m_user_data.begin() + items_to_discard);
              m_user_ts_data.erase(m_user_ts_data.begin(), m_user_ts_data.begin() + items_to_discard);
          }

          size_t userDataCount = frame_buffer.frame_p_c_n[display_index].user_data_v.size();
          
          sVideoFrameBuffer *p_buffer = &frame_buffer.frame_p_c_n[display_index];
          
          for (Ipp32u i = 0; i < userDataCount; i += 1)
          {
              m_user_data.push_back(p_buffer->user_data_v[i]);
              m_user_ts_data.push_back(std::make_pair(m_dTime[display_index].time, sizeof(Ipp64f)));
          }

          // memory ownership transfered to m_user_data, so just clear()
          p_buffer->user_data_v.clear();
      }
    /*
      if(m_pCCDataTS->LockInputBuffer(&ccData) == UMC_OK)
      {
          Ipp8u* ptr, *p;
          Ipp32s size, s;
          p  = (Ipp8u*)ccData.GetDataPointer();
          s = (Ipp32s)ccData.GetDataSize();
          ptr = p + s;
          p = (Ipp8u*)ccData.GetBufferPointer();
          s = (Ipp32s)ccData.GetBufferSize();
          size = (Ipp32s)(p + s - ptr);
          
          if(ptr!=0 && size >= sizeof(Ipp64f))
          {
             ippsCopy_8u((Ipp8u*)(&m_dTime[display_index].time), ptr, sizeof(Ipp64f));
             ccData.SetDataSize(ccData.GetDataSize() + sizeof(Ipp64f));
             umcRes = m_pCCDataTS->UnLockInputBuffer(&ccData);
          }
      }*/

    } // if(frame_buffer.frame_p_c_n[display_index].IsUserDataDecoded)

    return umcRes;
}

Status MPEG2VideoDecoderBase::PostProcessFrame(int display_index, int task_num)
{
    Status umcRes = UMC_OK;

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)

      if(pack_w.va_mode != VA_NO)
      {
          bool executeCalled = false;
          if(pack_w.pSliceInfoBuffer < pack_w.pSliceInfo && pack_w.va_mode == VA_VLD_W)
          {
             // printf("save data at the end of frame\n");
#   if defined(UMC_VA_DXVA)   // part 1
             if(!pack_w.IsProtectedBS)
             {
                pack_w.bs_size = pack_w.pSliceInfo[-1].dwSliceDataLocation +
                              pack_w.pSliceInfo[-1].dwSliceBitsInBuffer/8;
             }
             else
             {
                 pack_w.bs_size += SliceSize(
                     pack_w.curr_encryptedData,
                     (PAVP_COUNTER_TYPE)(pack_w.m_va->GetProtectedVA())->GetCounterMode(),
                     pack_w.overlap);
                 pack_w.is_bs_aligned_16 = true;
             }

             // printf("pack_w.bs_size = %d\n", pack_w.bs_size);
             // printf("pack_w.bs_size_getting = %d\n", pack_w.bs_size_getting);
            if ((pack_w.bs_size >= pack_w.bs_size_getting) ||
                ((Ipp32s)((Ipp8u*)pack_w.pSliceInfo - (Ipp8u*)pack_w.pSliceInfoBuffer) >=
                    (pack_w.slice_size_getting-(Ipp32s)sizeof(DXVA_SliceInfo))))
              {
                DXVA_SliceInfo s_info;
                bool slice_split = false;
                Ipp32s sz = 0, sz_align = 0;

                memcpy_s(&s_info,sizeof(DXVA_SliceInfo),&pack_w.pSliceInfo[-1],sizeof(DXVA_SliceInfo));

                pack_w.pSliceInfo--;

                int size_bs = 0;
                int size_sl = 0;
                if(pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)
                {

                    if(pack_w.IsProtectedBS)
                    {
                        Ipp32s NumSlices = (Ipp32s)(pack_w.pSliceInfo - pack_w.pSliceInfoBuffer);
                        mfxEncryptedData *encryptedData = pack_w.curr_bs_encryptedData;
                        pack_w.bs_size = pack_w.add_to_slice_start;//0;
                        pack_w.overlap = 0;

                        for (int i = 0; i < NumSlices; i++)
                        {
                            if (!encryptedData)
                                return UMC_ERR_DEVICE_FAILED;

                            pack_w.bs_size += SliceSize(encryptedData,
                                (PAVP_COUNTER_TYPE)(pack_w.m_va->GetProtectedVA())->GetCounterMode(),
                                pack_w.overlap);

                            encryptedData = encryptedData->Next;
                        }

                        sz = sz_align = pack_w.bs_size;
                        pack_w.is_bs_aligned_16 = true;

                        if(pack_w.bs_size & 0xf)
                        {
                            sz_align = ((pack_w.bs_size >> 4) << 4) + 16;
                            pack_w.bs_size = sz_align;
                            pack_w.is_bs_aligned_16 = false;
                        }

                        if(!pack_w.is_bs_aligned_16)
                        {
                            std::vector<Ipp8u> buf(sz_align);
                            NumSlices++;
                            encryptedData = pack_w.curr_bs_encryptedData;
                            Ipp8u *ptr = &buf[0];
                            Ipp8u *pBuf=ptr;
                            for (int i = 0; i < NumSlices; i++)
                            {
                                if (!encryptedData)
                                    return UMC_ERR_DEVICE_FAILED;
                                Ipp32u alignedSize = encryptedData->DataLength + encryptedData->DataOffset;
                                if(alignedSize & 0xf)
                                    alignedSize = alignedSize+(0x10 - (alignedSize & 0xf));
                                if(i < NumSlices - 1)
                                {
                                    ippsCopy_8u(encryptedData->Data ,pBuf, alignedSize);
                                    Ipp32u diff = (Ipp32u)(ptr - pBuf);
                                    ptr += alignedSize - diff;

                                    (unsigned char*)pBuf += DistToNextSlice(encryptedData,
                                        (PAVP_COUNTER_TYPE)(pack_w.m_va->GetProtectedVA())->GetCounterMode());
                                }
                                else
                                {
                                    ippsCopy_8u(encryptedData->Data,pBuf, sz_align - sz);
                                }

                                encryptedData = encryptedData->Next;
                            }
                            ptr = &buf[0] + sz_align - 16;
                            ippsCopy_8u(pack_w.add_bytes,ptr, (sz - (sz_align - 16)));
                        }
                    }
#   elif defined UMC_VA_LINUX  // (part1)

            pack_w.bs_size = pack_w.pSliceInfo[-1].slice_data_offset
                           + pack_w.pSliceInfo[-1].slice_data_size;
            if (pack_w.bs_size >= pack_w.bs_size_getting)
            {
                VASliceParameterBufferMPEG2  s_info;
                bool slice_split = false;
                Ipp32s sz = 0, sz_align = 0;
                memcpy_s(&s_info, sizeof(VASliceParameterBufferMPEG2), &pack_w.pSliceInfo[-1], sizeof(VASliceParameterBufferMPEG2));
                pack_w.pSliceInfo--;
                int size_bs = GET_END_PTR(Video[task_num][0]->bs) - GET_START_PTR(Video[task_num][0]->bs)+4;   // ao: I copied from BeginVAFrame. Is it ok? or == bs_size
                int size_sl = m_ClipInfo.clip_info.width*m_ClipInfo.clip_info.height/256;      // ao: I copied from BeginVAFrame. Is it ok?
                if(pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)
                {
#   endif  //UMC_VA_LINUX (part 1)
                  //  printf("Save previous slices\n");
mm:
                    Ipp32s numMB = (PictureHeader[task_num].picture_structure == FRAME_PICTURE)
                        ? sequenceHeader.numMB[task_num]
                        : sequenceHeader.numMB[task_num]/2;

                    if (pack_w.SetBufferSize(
                        numMB,
                        PictureHeader[task_num].picture_coding_type,
                        size_bs,
                        size_sl)!= UMC_OK)
                    {
                      return UMC_ERR_NOT_ENOUGH_BUFFER;
                    }

                    umcRes = pack_w.SaveVLDParameters(
                        &sequenceHeader,
                        &PictureHeader[task_num],
                        pack_w.pSliceStart,
                        &frame_buffer,task_num,
                        ((m_ClipInfo.clip_info.height + 15) >> 4));
                    if (UMC_OK != umcRes)
                        return umcRes;

                    umcRes = pack_w.m_va->Execute();
                    if (UMC_OK != umcRes)
                        return umcRes;

                    pack_w.add_to_slice_start = 0;

                    if(pack_w.IsProtectedBS)
                    {
                        if(!pack_w.is_bs_aligned_16)
                        {
                            pack_w.add_to_slice_start = sz - (sz_align - 16);
                        }
                    }
                }//if(pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)

#    if defined(UMC_VA_DXVA)   // part 2
                pack_w.InitBuffers(0, 0);

                memcpy_s(&pack_w.pSliceInfo[0], sizeof(s_info), &s_info, sizeof(s_info));

                pack_w.pSliceStart += pack_w.pSliceInfo[0].dwSliceDataLocation;
                pack_w.pSliceInfo[0].dwSliceDataLocation = 0;

                if(!pack_w.IsProtectedBS)
                {
                    pack_w.bs_size = pack_w.pSliceInfo[0].dwSliceBitsInBuffer/8;
                }
                else
                {
                    pack_w.bs_size = pack_w.add_to_slice_start;
                    pack_w.bs_size += SliceSize(
                        pack_w.curr_encryptedData,
                        (PAVP_COUNTER_TYPE)(pack_w.m_va->GetProtectedVA())->GetCounterMode(),
                        pack_w.overlap);
                    pack_w.curr_encryptedData = pack_w.curr_encryptedData->Next;
                }

                if(pack_w.bs_size >= pack_w.bs_size_getting)
                {
                    slice_split = true;
                    pack_w.pSliceInfo->dwSliceBitsInBuffer = (pack_w.bs_size_getting - 1) * 8;
                    pack_w.pSliceInfo->wBadSliceChopping = 1;
                    pack_w.bs_size -= (pack_w.bs_size_getting - 1);
                    s_info.dwSliceDataLocation = (pack_w.bs_size_getting - 1);
                    s_info.dwSliceBitsInBuffer -= pack_w.pSliceInfo->dwSliceBitsInBuffer;
                    pack_w.pSliceInfo++;
                    goto mm;
                }
                else
                {
                    if(slice_split)
                        pack_w.pSliceInfo->wBadSliceChopping = 2;
                }

#elif defined(UMC_VA_LINUX)    // part 2

                pack_w.InitBuffers(size_bs, size_sl);

                memcpy_s(&pack_w.pSliceInfo[0], sizeof(s_info), &s_info, sizeof(s_info));

                pack_w.pSliceStart += pack_w.pSliceInfo[0].slice_data_offset;
                pack_w.pSliceInfo[0].slice_data_offset = 0;

                if(!pack_w.IsProtectedBS)
                {
                    pack_w.bs_size = pack_w.pSliceInfo[0].slice_data_size;
                }
                else
                {
                    // AO: add PAVP technologies
                }

                if(pack_w.bs_size >= pack_w.bs_size_getting)
                {
                    slice_split = true;
                    pack_w.pSliceInfo->slice_data_size = pack_w.bs_size_getting - 1;
                    pack_w.pSliceInfo->slice_data_flag = VA_SLICE_DATA_FLAG_BEGIN;
                    pack_w.bs_size -= (pack_w.bs_size_getting - 1);
                    s_info.slice_data_offset = pack_w.bs_size_getting - 1;
                    s_info.slice_data_size -= pack_w.pSliceInfo->slice_data_size;
                    pack_w.pSliceInfo++;

                    goto mm;
                }
                else
                {
                    if(slice_split)
                        pack_w.pSliceInfo->slice_data_flag = VA_SLICE_DATA_FLAG_END;
                }
#   endif   // UMC_VA_LINUX (part 2)

                pack_w.pSliceInfo++;
            }
            //  printf("Save last slice\n");

            umcRes = pack_w.SaveVLDParameters(
                &sequenceHeader,
                &PictureHeader[task_num],
                pack_w.pSliceStart,
                &frame_buffer,
                task_num);
            if (UMC_OK != umcRes)
            {
                return umcRes;
            }

            Ipp32s numMB = (PictureHeader[task_num].picture_structure == FRAME_PICTURE)
                ? sequenceHeader.numMB[task_num]
                : sequenceHeader.numMB[task_num]/2;

              if(pack_w.SetBufferSize(numMB,PictureHeader[task_num].picture_coding_type)!= UMC_OK)
                  return UMC_ERR_NOT_ENOUGH_BUFFER;

              umcRes = pack_w.m_va->Execute();
              executeCalled = true;
              if (UMC_OK != umcRes)
                  return UMC_ERR_DEVICE_FAILED;
          }
          if (!executeCalled)
          {
              umcRes = pack_w.m_va->ReleaseAllBuffers();
              if (UMC_OK != umcRes)
                  return UMC_ERR_DEVICE_FAILED;
          }
          umcRes = pack_w.m_va->EndFrame();
          if (UMC_OK != umcRes)
              return UMC_ERR_DEVICE_FAILED;

          if(display_index < 0)
          {
              return UMC_ERR_NOT_ENOUGH_DATA;
          }

          umcRes = PostProcessUserData(display_index);
          //pack_w.m_va->DisplayFrame(frame_buffer.frame_p_c_n[frame_buffer.retrieve    ].va_index,out_data);

          return UMC_OK;
      }//if(pack_w.va_mode != VA_NO)

#endif // defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)

    if (PictureHeader[task_num].picture_structure != FRAME_PICTURE &&
          frame_buffer.field_buffer_index[task_num] == 0)
    {
        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    if (display_index < 0)
    {
        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    umcRes = PostProcessUserData(display_index);

   return umcRes;
}


// Close decoding
Status MPEG2VideoDecoderBase::Close()
{
   // Ipp32s i;

    DeleteTables();

    m_dPlaybackRate = 1;

    if (m_pCCData)
    {
        delete m_pCCData;
        m_pCCData = NULL;
    }
    if (m_pCCDataTS)
    {
        delete m_pCCDataTS;
        m_pCCDataTS = NULL;
    }

    ClearUserDataVector(m_user_data);
    m_user_ts_data.clear();

    if (shMask.memMask)
    {
#if defined(_WIN32) || defined(_WIN64)
        _aligned_free(shMask.memMask);
#else
        free(shMask.memMask);
#endif
    }

    shMask.memMask = NULL;
    shMask.memSize = 0;

    return VideoDecoder::Close();
}


MPEG2VideoDecoderBase::MPEG2VideoDecoderBase()
    : m_InitClipInfo()
{
    for (int i = 0; i < 2*DPB_SIZE; i++)
    {
        Video[i] = NULL;
        memset(&PictureHeader[i], 0, sizeof(sPictureHeader));
    }

    m_lFlags                   = 0;
    m_ClipInfo.framerate       = 0;
    m_ClipInfo.clip_info.width = m_ClipInfo.clip_info.height= 100;

    memset(&sequenceHeader, 0, sizeof(sequenceHeader));

    m_ClipInfo.stream_type = UNDEF_VIDEO;
    sequenceHeader.profile = MPEG2_PROFILE_MAIN;
    sequenceHeader.level = MPEG2_LEVEL_MAIN;
    frame_buffer.allocated_mb_width = 0;
    frame_buffer.allocated_mb_height = 0;
    frame_buffer.allocated_cformat = NONE;
    frame_buffer.mid_context_data = MID_INVALID;
    frame_buffer.ptr_context_data = NULL; // internal buffer not allocated

    m_nNumberOfThreads  = 1;
    m_nNumberOfAllocatedThreads  = 1;

    m_pCCData           = NULL;
    m_pCCDataTS         = NULL;

    m_user_data.clear();
    m_user_ts_data.clear();

    m_dPlaybackRate     = 1;
    m_IsDataFirst       = true;

    m_IsLastFrameProcessed = false;
    m_isFrameRateFromInit = false;
    m_isAspectRatioFromInit = false;

    vlcMBAdressing.table0 = vlcMBAdressing.table1 = NULL;
    vlcMBType[0].table0 = vlcMBType[0].table1 = NULL;
    vlcMBType[1].table0 = vlcMBType[1].table1 = NULL;
    vlcMBType[2].table0 = vlcMBType[2].table1 = NULL;
    vlcMBPattern.table0 = vlcMBPattern.table1 = NULL;
    vlcMotionVector.table0 = vlcMotionVector.table1 = NULL;

    shMask.memMask = NULL;
    shMask.memSize = 0;
}

MPEG2VideoDecoderBase::~MPEG2VideoDecoderBase()
{
    m_isFrameRateFromInit = false;
    m_isAspectRatioFromInit = false;
    Close();
}

Ipp32u MPEG2VideoDecoderBase::GetNumOfSkippedFrames()
{
    return sequenceHeader.num_of_skipped;
}

Status MPEG2VideoDecoderBase::Reset()
{
    int i;
    sequenceHeader.first_i_occure  = 0;
    sequenceHeader.first_p_occure  = 0;
    sequenceHeader.broken_link = 0;
    sequenceHeader.closed_gop  = 0;
    sequenceHeader.time_code.gop_picture = 0;
    sequenceHeader.time_code.gop_seconds = 0;
    sequenceHeader.time_code.gop_minutes = 0;
    sequenceHeader.time_code.gop_hours   = 0;
    sequenceHeader.time_code.gop_drop_frame_flag = 0;
    sequenceHeader.stream_time = 0; //-sequenceHeader.delta_frame_time;
    sequenceHeader.frame_rate_extension_d = 0;
    sequenceHeader.frame_rate_extension_n = 0;
    sequenceHeader.frame_rate_code = 0;
    sequenceHeader.aspect_ratio_code = 0;
    sequenceHeader.chroma_format = 0;
    sequenceHeader.width = 0;
    sequenceHeader.height = 0;

    frame_buffer.latest_prev =  -1;
    frame_buffer.common_curr_index = -1;
    frame_buffer.latest_next =  -1;
    m_picture_coding_type_save = MPEG2_I_PICTURE;

    m_IsLastFrameProcessed = false;
    m_isFrameRateFromInit = false;
    m_isAspectRatioFromInit = false;

    for(i = 0; i < 2*DPB_SIZE; i++)
    {
        PictureHeader[i].intra_vlc_format    = 0;
        PictureHeader[i].curr_intra_dc_multi = intra_dc_multi[0];

        frame_buffer.ret_array[i] = -1;
        frame_buffer.curr_index[i] =  -1;
        frame_buffer.retrieve = -1;
        frame_buffer.field_buffer_index[i]  = 0;
        frame_buffer.frame_p_c_n[i].frame_time = -1;
        frame_buffer.frame_p_c_n[i].duration = 0;
        frame_buffer.frame_p_c_n[i].IsUserDataDecoded = false;
        frame_buffer.frame_p_c_n[i].us_data_size = 0;
        ClearUserDataVector(frame_buffer.frame_p_c_n[i].user_data_v);

        frame_buffer.frame_p_c_n[i].va_index = -1;
        frame_buffer.task_locked[i]          = -1;

        m_dTime[i].time     = -1.;
        m_dTime[i].isOriginal = false;
        m_nNumberOfThreadsTask[i] = m_nNumberOfThreads;
    }

    m_dTimeCalc     = -1.;

    frame_buffer.ret_array_curr = 0;
    frame_buffer.ret_index = -1;
    frame_buffer.ret_array_free = 0;
    frame_buffer.ret_array_len  = 0;

    m_SkipLevel = SKIP_NONE;

    if (m_pCCData)
    {
      m_ccCurrData.SetBufferPointer(0,0);
      m_pCCData->Reset();
    }
    if (m_pCCDataTS)
    {
      m_ccCurrDataTS.SetBufferPointer(0,0);
      m_pCCDataTS->Reset();
    }

    if (shMask.memMask)
    {
#if defined(_WIN32) || defined(_WIN64)
        _aligned_free(shMask.memMask);
#else
        free(shMask.memMask);
#endif
    }

    shMask.memMask = NULL;
    shMask.memSize = 0;

    return UMC_OK;
}

Status MPEG2VideoDecoderBase::GetInfo(BaseCodecParams* info)
{
  VideoDecoderParams *pParams;
  if(info == NULL)
    return UMC_ERR_NULL_PTR;

  // BaseCodecParams
  info->profile = sequenceHeader.profile;
  info->level = sequenceHeader.level;

  pParams = DynamicCast<VideoDecoderParams> (info);

  if (NULL != pParams) {
    pParams->info = m_ClipInfo;
    //VideoDecoder::GetInfo(pParams);
    pParams->lFlags = m_lFlags;
    if(m_bOwnAllocator)
      pParams->lpMemoryAllocator = 0;
    else
      pParams->lpMemoryAllocator = m_pMemoryAllocator;
    pParams->pPostProcessing = m_PostProcessing;
    pParams->numThreads = m_nNumberOfThreads;
  }
  return UMC_OK;
}

Status MPEG2VideoDecoderBase::GetSequenceHeaderMemoryMask(Ipp8u *buffer, Ipp16u &size)
{
    if (NULL == buffer)
        return UMC_ERR_NULL_PTR;
    if (size < shMask.memSize)
        return UMC_ERR_NOT_ENOUGH_BUFFER;

    memcpy_s(buffer, size, shMask.memMask, shMask.memSize);
    size = shMask.memSize;

    return UMC_OK;
}

Status MPEG2VideoDecoderBase::GetSignalInfoInformation(mfxExtVideoSignalInfo *buffer)
{
    buffer->ColourDescriptionPresent = m_signalInfo.ColourDescriptionPresent;
    buffer->ColourPrimaries = m_signalInfo.ColourPrimaries;
    buffer->VideoFormat = m_signalInfo.VideoFormat;
    buffer->TransferCharacteristics = m_signalInfo.TransferCharacteristics;
    buffer->MatrixCoefficients = m_signalInfo.MatrixCoefficients;

    return UMC_OK;
}

Status MPEG2VideoDecoderBase::GetPerformance    (Ipp64f * /*perf*/)
{
    return UMC_OK;
}

Status MPEG2VideoDecoderBase::GetFrame(MediaData* , MediaData* )
{
    return UMC_OK;
}
void MPEG2VideoDecoderBase::ReadCCData(int task_num)
{
//      Ipp8u* p;
      Ipp8u* readptr;
//      Ipp32s size;
      Ipp32s input_size;
      Ipp32s code;
      Ipp32s t_num = task_num < DPB_SIZE? task_num : task_num - DPB_SIZE;

      IppVideoContext* video = Video[task_num][0];

      /*
      size = frame_buffer.frame_p_c_n[t_num].us_buf_size - frame_buffer.frame_p_c_n[t_num].us_data_size;
      
      if (size < 4)
      {
        return;
      }

      p = frame_buffer.frame_p_c_n[t_num].user_data + frame_buffer.frame_p_c_n[t_num].us_data_size;
      */

      input_size = (Ipp32s)(GET_REMAINED_BYTES(video->bs));
      readptr = GET_BYTE_PTR(video->bs);
      FIND_START_CODE(video->bs, code);
      
      if (code != UMC_ERR_NOT_ENOUGH_DATA)
      {
        input_size = (Ipp32s)(GET_BYTE_PTR(video->bs) - readptr);
      }
      // -------------------
      {
#if defined(_WIN32) || defined(_WIN64)
        Ipp8u *p = (Ipp8u *) _aligned_malloc(input_size + 4, 16);
#else
        Ipp8u *p = (Ipp8u *) malloc(input_size + 4);
#endif
        if (!p)
        {
            frame_buffer.frame_p_c_n[t_num].IsUserDataDecoded = false;
            return;
        }
        p[0] = 0; p[1] = 0; p[2] = 1; p[3] = 0xb2;
        ippsCopy_8u(readptr, p + 4, input_size);
        
        frame_buffer.frame_p_c_n[t_num].user_data_v.push_back(std::make_pair(p, input_size + 4));

        frame_buffer.frame_p_c_n[t_num].IsUserDataDecoded = true;
      }
      // -------------------

      /*
      p[0] = 0; p[1] = 0; p[2] = 1; p[3] = 0xb2;
      size -= 4;
      size = IPP_MIN(size, inputsize);

      ippsCopy_8u(readptr, p+4, size);
      
      frame_buffer.frame_p_c_n[t_num].us_data_size += (size+4);
      
      frame_buffer.frame_p_c_n[t_num].IsUserDataDecoded = true;

      if(code == UMC_ERR_NOT_ENOUGH_DATA) {
        p = GET_END_PTR(video->bs);
        if(p[-1] <= 1) p--;
        if(p[-1] == 0) p--;
        if(p[-1] == 0) p--;
        SET_PTR(video->bs, p);
      }
      */
}

MPEG2VideoDecoder::MPEG2VideoDecoder()
{
    m_pDec = NULL;
}

MPEG2VideoDecoder::~MPEG2VideoDecoder(void)
{
    Close();
}

Status MPEG2VideoDecoder::Init(BaseCodecParams *init)
{
    Close();

    m_pDec = new MPEG2VideoDecoderBase;
    if(!m_pDec) return UMC_ERR_ALLOC;

    Status res = m_pDec->Init(init);

    if (UMC_OK != res) {
      Close();
    }

    return res;
}


Status MPEG2VideoDecoder::GetFrame(MediaData* in, MediaData* out)
{
    if(m_pDec)
    {
        return m_pDec->GetFrame(in, out);
    }

    return UMC_ERR_NOT_INITIALIZED;
}

Status MPEG2VideoDecoder::Close()
{
    if(m_pDec)
    {
        m_pDec->Close();
        delete m_pDec;
        m_pDec = NULL;
    }

    return UMC_OK;
}

Status MPEG2VideoDecoder::Reset()
{
    if(m_pDec)
    {
        return m_pDec->Reset();
    }

    return UMC_ERR_NOT_INITIALIZED;
}

Status MPEG2VideoDecoder::GetInfo(BaseCodecParams* info)
{
    if(m_pDec)
    {
        return m_pDec->GetInfo(info);
    }

    return UMC_ERR_NOT_INITIALIZED;
}

Status MPEG2VideoDecoder::GetPerformance(Ipp64f *perf)
{
    if(m_pDec)
    {
        return m_pDec->GetPerformance(perf);
    }

    return UMC_ERR_NOT_INITIALIZED;
}

Status MPEG2VideoDecoder::ResetSkipCount()
{
    return UMC_OK;
}

Status MPEG2VideoDecoder::SkipVideoFrame(Ipp32s count)
{
    (count);
    return UMC_OK;
}

Ipp32u MPEG2VideoDecoder::GetNumOfSkippedFrames()
{
    if(m_pDec)
    {
        return m_pDec->GetNumOfSkippedFrames();
    }

    return (Ipp32u)(-1); // nobody check for -1 really

}


Status MPEG2VideoDecoder::GetUserData(MediaData* pCC)
{
    if(m_pDec)
    {
        return m_pDec->GetUserData(pCC);
    }

    return UMC_ERR_NOT_INITIALIZED;
}

Status MPEG2VideoDecoder::SetParams(BaseCodecParams* params)
{
    if(m_pDec)
    {
        return m_pDec->SetParams(params);
    }
    return UMC_ERR_NOT_INITIALIZED;

}

Status MPEG2VideoDecoderBase::SetParams(BaseCodecParams* params)
{
    VideoDecoderParams *pParams = DynamicCast<VideoDecoderParams>(params);

    if (NULL == pParams)
        return UMC_ERR_NULL_PTR;

    if(pParams->lTrickModesFlag == 7)
    {
        m_dPlaybackRate = pParams->dPlaybackRate;
    }

    return UMC_OK;
}

Status MPEG2VideoDecoderBase::UpdateFrameBuffer(int task_num)
{
  Ipp32s pitch_l, pitch_c;
  Ipp32s height_l, height_c;
  Ipp32s size_l, size_c;
  //Ipp8u* ptr;
 // Ipp32s i;
  //Ipp32s buff_size;

  if(frame_buffer.ptr_context_data &&
    frame_buffer.allocated_mb_width == sequenceHeader.mb_width[task_num] &&
    frame_buffer.allocated_mb_height == sequenceHeader.mb_height[task_num] &&
    frame_buffer.allocated_cformat == m_ClipInfo.color_format)
    return UMC_OK; // all is the same


  pitch_l = align_value<Ipp32s>(sequenceHeader.mb_width[task_num]*16, ALIGN_VALUE);
  height_l = align_value<Ipp32s>(sequenceHeader.mb_height[task_num]*16, ALIGN_VALUE);
  size_l = height_l*pitch_l;
  if (m_ClipInfo.color_format != YUV444) {

    pitch_c = pitch_l >> 1;
    height_c = height_l >> 1;
    size_c = height_c*pitch_c;
    if (m_ClipInfo.color_format == YUV422)
      size_c *= 2;
  } else {
    pitch_c = pitch_l;
    size_c = size_l;
  }

  frame_buffer.Y_comp_pitch = pitch_l;
  frame_buffer.U_comp_pitch = pitch_c;
  frame_buffer.V_comp_pitch = pitch_c;
  frame_buffer.pic_size = size_l;

  Ipp32u threadsNum = GetCurrThreadsNum(task_num);

    for (Ipp32u i = 0; i < threadsNum; i += 1)
    {
        IppVideoContext* video = Video[task_num][i];

        video->Y_comp_pitch = pitch_l;
        video->U_comp_pitch = pitch_c;
        video->V_comp_pitch = pitch_c;
        video->pic_size = size_l;
    }

  for (Ipp32u i = 0; i < threadsNum; i += 1)
    {
      IppVideoContext* video = Video[task_num][i];

      video->blkOffsets[0][0] = 0;
      video->blkOffsets[0][1] = 8;
      video->blkOffsets[0][2] = 8*pitch_l;
      video->blkOffsets[0][3] = 8*pitch_l + 8;
      video->blkOffsets[0][4] = 0;
      video->blkOffsets[0][5] = 0;
      video->blkOffsets[0][6] = 8*pitch_c;
      video->blkOffsets[0][7] = 8*pitch_c;
      video->blkOffsets[1][0] = 0;
      video->blkOffsets[1][1] = 8;
      video->blkOffsets[1][2] = pitch_l;
      video->blkOffsets[1][3] = pitch_l + 8;
      video->blkOffsets[1][4] = 0;
      video->blkOffsets[1][5] = 0;
      video->blkOffsets[1][6] = pitch_c;
      video->blkOffsets[1][7] = pitch_c;
      video->blkOffsets[2][0] = 0;
      video->blkOffsets[2][1] = 8;
      video->blkOffsets[2][2] = 16*pitch_l;
      video->blkOffsets[2][3] = 16*pitch_l + 8;
      video->blkOffsets[2][4] = 0;
      video->blkOffsets[2][5] = 0;
      video->blkOffsets[2][6] = 16*pitch_c;
      video->blkOffsets[2][7] = 16*pitch_c;

      video->blkPitches[0][0] = pitch_l;
      video->blkPitches[0][1] = pitch_c;
      video->blkPitches[1][0] = 2*pitch_l;
      video->blkPitches[1][1] = pitch_c;
      if (video->color_format != YUV420) video->blkPitches[1][1] = 2 * pitch_c;
      video->blkPitches[2][0] = 2*pitch_l;
      video->blkPitches[2][1] = 2*pitch_c;

    }

  if(frame_buffer.ptr_context_data &&
    frame_buffer.allocated_mb_width >= sequenceHeader.mb_width[task_num] &&
    frame_buffer.allocated_mb_height >= sequenceHeader.mb_height[task_num] &&
    frame_buffer.allocated_cformat >= m_ClipInfo.color_format)
    return UMC_OK; // use allocated before

  frame_buffer.allocated_mb_width = sequenceHeader.mb_width[task_num];
  frame_buffer.allocated_mb_height = sequenceHeader.mb_height[task_num];
  frame_buffer.allocated_cformat = m_ClipInfo.color_format;

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)
  if(pack_w.va_mode != VA_NO)
  {
      return UMC_OK;
  }
#endif

  // Alloc frames

  if(frame_buffer.mid_context_data != MID_INVALID)
    m_pMemoryAllocator->Free(frame_buffer.mid_context_data);

  return UMC_OK;
}


#define SHIFT_PTR(oldptr, oldbase, newbase) \
  ( (newbase) + ( (Ipp8u*)(oldptr) - (Ipp8u*)(oldbase) ) )

//Common functions
Ipp32s MPEG2VideoDecoderBase::GetCurrDecodingIndex(int task_num)
{
    int index = frame_buffer.curr_index[task_num] < DPB_SIZE?
        frame_buffer.curr_index[task_num] : frame_buffer.curr_index[task_num]-DPB_SIZE;
    return index;
}

Ipp32s MPEG2VideoDecoderBase::GetPrevDecodingIndex(int index)
{
    return frame_buffer.frame_p_c_n[index].prev_index;
}

Ipp32s MPEG2VideoDecoderBase::GetNextDecodingIndex(int index)
{
    return frame_buffer.frame_p_c_n[index].next_index;
}

Ipp32s MPEG2VideoDecoderBase::GetFrameType(int index)
{
    return frame_buffer.frame_p_c_n[index].frame_type;
}

bool MPEG2VideoDecoderBase::IsSHDecoded()
{
    return m_IsSHDecoded;
}

bool MPEG2VideoDecoderBase::IsFrameSkipped()
{
    return m_IsFrameSkipped;
}

void MPEG2VideoDecoderBase::SetSkipMode(Ipp32s SkipMode)
{
    m_SkipLevel = SkipMode;
}

Ipp64f MPEG2VideoDecoderBase::GetCurrDecodedTime(int index)
{
    return m_dTime[index].time;
}

bool MPEG2VideoDecoderBase::isOriginalTimeStamp(int index)
{
    return m_dTime[index].isOriginal;
}

Ipp32s MPEG2VideoDecoderBase::GetCurrThreadsNum(int task_num)
{
    return m_nNumberOfThreadsTask[task_num];
}

bool MPEG2VideoDecoderBase::IsFramePictureStructure(int task_num)
{
    return(PictureHeader[task_num].picture_structure == FRAME_PICTURE);
}

Ipp32s MPEG2VideoDecoderBase::GetDisplayIndex()
{
    if(frame_buffer.ret_array[frame_buffer.ret_array_curr] != -1)
    {
      frame_buffer.retrieve     = frame_buffer.ret_array[frame_buffer.ret_array_curr];
      frame_buffer.ret_array[frame_buffer.ret_array_curr] = -1;
      frame_buffer.ret_array_curr++;
      if(frame_buffer.ret_array_curr >= DPB_SIZE)
          frame_buffer.ret_array_curr = 0;
      frame_buffer.ret_array_len--;
    }
    else
    {
        frame_buffer.retrieve = -1;
    }

    return frame_buffer.retrieve;
}
Ipp32s MPEG2VideoDecoderBase::GetRetBufferLen()
{
    return frame_buffer.ret_array_len;
}

int MPEG2VideoDecoderBase::FindFreeTask()
{
    int i;
    for(i = 0; i < DPB_SIZE; i++)
    {
        if(frame_buffer.task_locked[i] == -1)
            return i;
    }
    return -1;
}

void MPEG2VideoDecoderBase::LockTask(int index)
{
    frame_buffer.task_locked[index] = 1;
}

void MPEG2VideoDecoderBase::UnLockTask(int index)
{
    //frame_buffer.frame_p_c_n[index].Y_comp_data = NULL;
   //frame_buffer.frame_p_c_n[index].U_comp_data = NULL;
   // frame_buffer.frame_p_c_n[index].V_comp_data = NULL;

    frame_buffer.task_locked[index] = -1;
    frame_buffer.task_locked[index+DPB_SIZE] = -1;
}

void MPEG2VideoDecoderBase::SetCorruptionFlag(int index)
{
    if (index < 0)
    {
        return;
    }

    frame_buffer.frame_p_c_n[index].isCorrupted = true;
}

bool MPEG2VideoDecoderBase::GetCorruptionFlag(int index)
{
    return frame_buffer.frame_p_c_n[index].isCorrupted;
}

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER

