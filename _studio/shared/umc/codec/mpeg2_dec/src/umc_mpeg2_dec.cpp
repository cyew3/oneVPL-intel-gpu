//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include <vector>

#include "vm_sys_info.h"
#include "vm_time.h"
#include "umc_video_data.h"
#include "umc_memory_allocator.h"
#include "umc_mpeg2_dec_base.h"
#include "umc_mpeg2_dec.h"

#include "mfx_trace.h"
#include "mfx_common_int.h"

//#ifdef OTLAD
//FILE *otl = NULL;
//int frame_count = 0;
//int slice_count = 0;
//#endif

//int frame_count = 0;

using namespace UMC;

static void ClearUserDataVector(sVideoFrameBuffer::UserDataVector & data)
{
    for (sVideoFrameBuffer::UserDataVector::iterator
            it = data.begin(), et = data.end(); it != et; ++it)
    {
        free(it->first);
    }

    data.clear();
}


bool MPEG2VideoDecoderBase::InitTables()
{
  return true;
}

void MPEG2VideoDecoderBase::DeleteHuffmanTables()
{
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

        ClearUserDataVector(frame_user_data_v[i]);
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
        }
    }
    m_nNumberOfAllocatedThreads = m_nNumberOfThreads;

    return UMC_OK;
}


Status MPEG2VideoDecoderBase::Init(BaseCodecParams *pInit)
{
    int i;

    VideoDecoderParams *init = DynamicCast<VideoDecoderParams, BaseCodecParams>(pInit);
    if(!init)
        return UMC_ERR_INIT;

    // checks or create memory allocator;
    m_pMemoryAllocator = init->lpMemoryAllocator;

    if((init->lFlags & FLAG_VDEC_4BYTE_ACCESS) != 0)
      return UMC_ERR_UNSUPPORTED;

    Reset();

    m_ClipInfo = init->info;
    m_lFlags = init->lFlags;
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

    if(UMC_OK != DecodeSequenceHeader( 0 , 0))
        return (UMC_ERR_INVALID_PARAMS);

    // work-arounf for mpeg1
    m_ClipInfo.stream_type = UNDEF_VIDEO;

    if (false == InitTables())
        return UMC_ERR_INIT;

    sequenceHeader.frame_count    = 0;
    sequenceHeader.stream_time    = 0;

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
        ClearUserDataVector(frame_user_data_v[i]);

        frame_buffer.frame_p_c_n[i].va_index = -1;
        task_locked[i] = -1;
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

    m_picture_coding_type_save = MPEG2_I_PICTURE;
    m_IsLastFrameProcessed = false;

    m_InitClipInfo = m_ClipInfo;
    return UMC_OK;
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
          return true;
      }
    }
    else if(!sequenceHeader.first_i_occure)
    {
        // To support only P frames streams this check is disabled
        //return true;
    }
    else if(PictureHeader[task_num].picture_coding_type == MPEG2_P_PICTURE) {
      sequenceHeader.first_p_occure = 1;
      if(m_SkipLevel >= SKIP_PB)
      {
          return true;
      }
    }
    else if(PictureHeader[task_num].picture_coding_type == MPEG2_B_PICTURE) {
      if(m_SkipLevel >= SKIP_B)
      {
          return true;
      }
    }
  }

  return false;
}

//$$$$$$$$$$$$$$$$
//additional functions for UMC-MFX interaction
Status MPEG2VideoDecoderBase::GetCCData(Ipp8u* ptr, Ipp32u *size, Ipp64u *time, Ipp32u bufsize)
{
    Status umcRes = UMC_OK;

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


    if (0 == m_user_data.size())
    {
        *size = 0;
        *time = 0;

        return UMC_OK;
    }

    Ipp8u *p_user_data = m_user_data.front().first;
    Ipp32u user_data_size = (Ipp32u) m_user_data.front().second;
    *size = user_data_size;

    if (*size == 0 || p_user_data == NULL)
    {
        m_user_ts_data.erase(m_user_ts_data.begin());
        m_user_data.erase(m_user_data.begin());
        *size = 0;
        *time = 0;
        return UMC_OK;
    }

    if (bufsize < *size)
    {
        *size *= 8;
        *time = 0;
        return UMC_ERR_NOT_ENOUGH_BUFFER;
    }

    ippsCopy_8u(p_user_data, ptr, *size);
    *size *= 8;

    free(p_user_data);

    m_user_ts_data.erase(m_user_ts_data.begin());
    m_user_data.erase(m_user_data.begin());

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
    {
      if(GET_OFFSET(video->bs) > 0) // some data was decoded
        return UMC_ERR_NOT_ENOUGH_DATA;
      else                          // no start codes found
        return UMC_ERR_SYNC;
    }
    if(code == SEQUENCE_END_CODE)
      continue;
    if(code != PICTURE_START_CODE) {
      Status s = DecodeHeader(code, video, task_num);
      if (s != UMC_OK)
        return s;
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

  if (m_IsFrameSkipped)
  {
    return UMC_ERR_NOT_ENOUGH_DATA;
  }//if (m_IsFrameSkipped)

  return (umcRes);
}

Status MPEG2VideoDecoderBase::ProcessRestFrame(int task_num)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MPEG2VideoDecoderBase::ProcessRestFrame");
    Status umcRes = UMC_OK;
    IppVideoContext* video = Video[task_num][0];
    Ipp32s i;

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
      {
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

          size_t userDataCount = frame_user_data_v[display_index].size();

          for (Ipp32u i = 0; i < userDataCount; i += 1)
          {
              m_user_data.push_back(frame_user_data_v[display_index][i]);
              m_user_ts_data.push_back(std::make_pair(m_dTime[display_index].time, sizeof(Ipp64f)));
          }

          // memory ownership transfered to m_user_data, so just clear()
          frame_user_data_v[display_index].clear();
      }

    } // if(frame_buffer.frame_p_c_n[display_index].IsUserDataDecoded)

    return umcRes;
}

// Close decoding
Status MPEG2VideoDecoderBase::Close()
{
    DeleteTables();

    ClearUserDataVector(m_user_data);
    m_user_ts_data.clear();

    if (shMask.memMask)
    {
        free(shMask.memMask);
    }

    shMask.memMask = NULL;
    shMask.memSize = 0;

    return UMC_OK;
}


MPEG2VideoDecoderBase::MPEG2VideoDecoderBase()
    : m_pMemoryAllocator(NULL)
    , m_dTimeCalc(0)
    , m_picture_coding_type_save(MPEG2_I_PICTURE)
    , bNeedNewFrame(false)
    , m_SkipLevel(SKIP_NONE)
    , sequenceHeader()
    , sHSaved()
    , shMask()
    , m_signalInfo()
    , b_curr_number_backup(0)
    , first_i_occure_backup(0)
    , frame_count_backup(0)
    , m_lFlags(0)
    , m_nNumberOfThreads(1)
    , m_nNumberOfAllocatedThreads(1)
    , m_IsFrameSkipped(false)
    , m_IsSHDecoded(false)
    , m_FirstHeaderAfterSequenceHeaderParsed(false)
    , m_IsDataFirst(true)
    , m_IsLastFrameProcessed(false)
    , m_isFrameRateFromInit(false)
    , m_isAspectRatioFromInit(false)
    , m_InitClipInfo()
{
    for (int i = 0; i < 2*DPB_SIZE; i++)
    {
        Video[i] = NULL;
        memset(&PictureHeader[i], 0, sizeof(sPictureHeader));
    }

    memset(task_locked, 0, sizeof(task_locked));
    memset(m_nNumberOfThreadsTask, 0, sizeof(m_nNumberOfThreadsTask));

    m_ClipInfo.clip_info.width = m_ClipInfo.clip_info.height= 100;
    m_ClipInfo.stream_type = UNDEF_VIDEO;
    sequenceHeader.profile = MPEG2_PROFILE_MAIN;
    sequenceHeader.level = MPEG2_LEVEL_MAIN;

    frame_buffer.allocated_cformat = NONE;
    frame_buffer.mid_context_data = MID_INVALID;


    m_user_data.clear();
    m_user_ts_data.clear();
}

MPEG2VideoDecoderBase::~MPEG2VideoDecoderBase()
{
    m_isFrameRateFromInit = false;
    m_isAspectRatioFromInit = false;
    Close();
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
        ClearUserDataVector(frame_user_data_v[i]);

        frame_buffer.frame_p_c_n[i].va_index = -1;
        task_locked[i] = -1;

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

    if (shMask.memMask)
    {
        free(shMask.memMask);
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
    pParams->lFlags = m_lFlags;
    pParams->lpMemoryAllocator = 0;
    pParams->pPostProcessing = 0;
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

Status MPEG2VideoDecoderBase::GetFrame(MediaData* , MediaData* )
{
    return UMC_OK;
}
void MPEG2VideoDecoderBase::ReadCCData(int task_num)
{
      Ipp8u* readptr;
      Ipp32s input_size;
      Ipp32s code;
      Ipp32s t_num = task_num < DPB_SIZE? task_num : task_num - DPB_SIZE;

      IppVideoContext* video = Video[task_num][0];

      input_size = (Ipp32s)(GET_REMAINED_BYTES(video->bs));
      readptr = GET_BYTE_PTR(video->bs);
      FIND_START_CODE(video->bs, code);

      if (code != UMC_ERR_NOT_ENOUGH_DATA)
      {
        input_size = (Ipp32s)(GET_BYTE_PTR(video->bs) - readptr);
      }
      // -------------------
      {
        Ipp8u *p = (Ipp8u *) malloc(input_size + 4);
        if (!p)
        {
            frame_buffer.frame_p_c_n[t_num].IsUserDataDecoded = false;
            return;
        }
        p[0] = 0; p[1] = 0; p[2] = 1; p[3] = 0xb2;
        ippsCopy_8u(readptr, p + 4, input_size);

        frame_user_data_v[t_num].push_back(std::make_pair(p, input_size + 4));

        frame_buffer.frame_p_c_n[t_num].IsUserDataDecoded = true;
      }
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
        if(task_locked[i] == -1)
            return i;

    return -1;
}

void MPEG2VideoDecoderBase::LockTask(int index)
{
    task_locked[index] = 1;
}

void MPEG2VideoDecoderBase::UnLockTask(int index)
{
    task_locked[index] = -1;
    task_locked[index+DPB_SIZE] = -1;
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

