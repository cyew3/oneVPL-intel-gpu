//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#include "umc_vp8_decoder.h"

using namespace UMC;

#define CHECK_N_REALLOC_BUFFERS \
{ \
  Ipp32u mbPerCol, mbPerRow; \
  mbPerCol = m_FrameInfo.frameHeight >> 4; \
  mbPerRow = m_FrameInfo.frameWidth  >> 4; \
  if (m_FrameInfo.mbPerCol * m_FrameInfo.mbPerRow  < mbPerRow * mbPerCol) \
  { \
    if (m_pMbInfo) \
      vp8dec_Free(m_pMbInfo); \
      \
    m_pMbInfo = (vp8_MbInfo*)vp8dec_Malloc(mbPerRow * mbPerCol * sizeof(vp8_MbInfo)); \
  } \
  if (m_FrameInfo.mbPerRow <  mbPerRow) \
  { \
    if (m_FrameInfo.blContextUp) \
      vp8dec_Free(m_FrameInfo.blContextUp); \
      \
    m_FrameInfo.blContextUp = (Ipp8u*)vp8dec_Malloc(mbPerRow * mbPerCol * sizeof(vp8_MbInfo)); \
  } \
  m_FrameInfo.mbPerCol = mbPerCol; \
  m_FrameInfo.mbPerRow = mbPerRow; \
  m_FrameInfo.mbStep   = mbPerRow; \
}

VP8VideoDecoder::VP8VideoDecoder()
{
  m_IsInitialized      = 0;

  m_FrameInfo.mbPerCol         = 0;
  m_FrameInfo.mbPerRow         = 0;
  m_FrameInfo.frameSize.width  = 0;
  m_FrameInfo.frameSize.height = 0;
  m_FrameInfo.frameWidth       = 0;
  m_FrameInfo.frameHeight      = 0;

  m_FrameInfo.blContextUp    = 0;
//  m_FrameInfo.partitionStart = 0;

  m_pMbInfo       = 0;

  for(Ipp8u i = 0; i < VP8_NUM_OF_REF_FRAMES; i++)
  {
    UMC_SET_ZERO(m_FrameData[i]);
    m_RefFrameIndx[i] = i;
  }

  m_CurrFrame = &m_FrameData[m_RefFrameIndx[VP8_INTRA_FRAME]];


  //?????
  m_RefreshInfo.copy2Golden          = 0;
  m_RefreshInfo.copy2Altref          = 0;
  m_RefreshInfo.refreshLastFrame     = 0;
  m_RefreshInfo.refreshProbabilities = 0;
  m_RefreshInfo.refreshRefFrame      = 0;

} // VP8VideoDecoder::VP8VideoDecoder(void)

VP8VideoDecoder::~VP8VideoDecoder(void)
{
  Close();
} // VP8VideoDecoder::~VP8VideoDecoder(void)


Status VP8VideoDecoder::Init(BaseCodecParams* init)
{
  Ipp32u i;
  Ipp32u j;

  Status status = UMC_OK;

  VideoDecoderParams* params = DynamicCast<VideoDecoderParams, BaseCodecParams>(init);

  if(!params) return UMC_ERR_INIT;

  m_Params = *params;

  for (i = 0; i < 16; i++)
  {
    m_MbExternal.blockMode[i] = VP8_B_DC_PRED;
    m_MbExternal.blockMV[i].mvx = 0;
    m_MbExternal.blockMV[i].mvy = 0;
    m_MbExternal.blockMV[i].s32 = 0;
  }

  m_RefreshInfo.refFrameBiasTable[0] = 0;

  m_MbExternal.refFrame = VP8_INTRA_FRAME;
  m_MbExternal.mv.mvx   = 0;
  m_MbExternal.mv.mvy   = 0;
  m_MbExternal.mv.s32   = 0;

  //set to zero Mb coeffs
  for(i = 0; i < m_FrameInfo.mbPerCol; i++)
  {
    for(j = 0; j < m_FrameInfo.mbPerRow; j++)
    {
      UMC_SET_ZERO(m_pMbInfo[i*m_FrameInfo.mbStep + j].blockMV);

      m_pMbInfo[i*m_FrameInfo.mbStep + j].mv.s32 = 0;
    }
  }

  m_IsInitialized = 1;

  return status;
} // VP8VideoDecoder::Init();


Status VP8VideoDecoder::GetFrame(MediaData* in, MediaData* out)
{

  out;
    
  Status status         = UMC_OK;
  Ipp32s part_number = 0;

  if(m_IsInitialized)
  {
    status = DecodeFrameHeader(/*m_Params.m_pData*/in);
    if(UMC_OK != status)
      return UMC_ERR_INIT;
  }
  else
    return UMC_ERR_INIT;

  for(Ipp8u i = 0; i < VP8_NUM_OF_REF_FRAMES; i++)
  {
    if( i != m_RefFrameIndx[VP8_LAST_FRAME]   && 
        i != m_RefFrameIndx[VP8_GOLDEN_FRAME] && 
        i != m_RefFrameIndx[VP8_ALTREF_FRAME])
    {
      m_CurrFrame = &m_FrameData[i];
      m_RefFrameIndx[VP8_INTRA_FRAME] = i;
      break;
    }
  }

  DecodeFirstPartition();

  for(Ipp32u mb_row = 0; mb_row < m_FrameInfo.mbPerCol; mb_row++)
  {
    if(m_FrameInfo.numTokenPartitions >= 1)
    {
      part_number++;

      if(part_number > m_FrameInfo.numTokenPartitions)
        part_number = 1;
    }

    DecodeMbRow(&m_BoolDecoder[part_number], mb_row);

    ReconstructMbRow(&m_BoolDecoder[part_number], mb_row); // Dequant + IDCT + Predict)
  }

  //???
  if(m_FrameInfo.frameType == I_PICTURE || m_RefreshInfo.refreshRefFrame)
    m_QuantInfo.lastGoldenKeyQP = m_QuantInfo.yacQP;


  if(m_FrameInfo.loopFilterLevel != 0)
  {
    if(m_FrameInfo.loopFilterType)
      LoopFilterSimple();
    else
      LoopFilterNormal();
  }

  ExtendFrameBorders(m_CurrFrame);

  // swap frame buffers to update Gold/AltRef/Last frames
  RefreshFrames();

  if (!m_RefreshInfo.refreshProbabilities)
    MFX_INTERNAL_CPY(&m_FrameProbs, &m_FrameProbs_saved, sizeof(m_FrameProbs));

  //out->SetFlags(m_FrameInfo.showFrame);

  /*
// temp frame saving
  if(m_FrameInfo.showFrame)
  {
    Ipp32u y_size;
    Ipp32u uv_size;
    Ipp32u yuv_size;
    Ipp32s i;

    y_size  = m_FrameInfo.frameSize.width*m_FrameInfo.frameSize.height;
    uv_size = ((m_FrameInfo.frameSize.width + 1)>>1)* ((m_FrameInfo.frameSize.height + 1)>>1);

    yuv_size = y_size + uv_size + uv_size;

    if(yuv_size != out->GetBufferSize())
    {
      out->Alloc(yuv_size);
    }

    Ipp8u* ptr = (Ipp8u*)out->GetDataPointer();

    Ipp8u* ptr_y = ptr;
    Ipp8u* ptr_u = ptr   + y_size;
    Ipp8u* ptr_v = ptr_u + uv_size;//(size>>2); //???

    for(i = 0; i < m_FrameInfo.frameSize.height; i++)
    {
      ippsCopy_8u(m_CurrFrame->data_y + m_CurrFrame->step_y*i,
                  ptr_y + i*m_FrameInfo.frameSize.width,
                  m_FrameInfo.frameSize.width);
    }


    for(i = 0; i < (m_FrameInfo.frameSize.height + 1)>> 1; i++)
    {
      ippsCopy_8u(m_CurrFrame->data_u + m_CurrFrame->step_uv*i,
                  ptr_u + i*((m_FrameInfo.frameSize.width + 1)>> 1),
                 (m_FrameInfo.frameSize.width + 1) >> 1);
    }

    for(i = 0; i < (m_FrameInfo.frameSize.height + 1) >> 1; i++)
    {
      ippsCopy_8u(m_CurrFrame->data_v + m_CurrFrame->step_uv*i,
                  ptr_v + i*((m_FrameInfo.frameSize.width +1) >> 1),
                  (m_FrameInfo.frameSize.width + 1) >> 1);
    }

    out->SetDataSize(yuv_size);//size + (size >> 2) + (size >> 2));

    out->SetFrameType(m_FrameInfo.frameType);
  }*/
// end temp saving


  return UMC_OK;
} // VP8VideoDecoder::GetFrame()

//#include <iostream>

Status VP8VideoDecoder::GetFrame(MediaData* in, FrameData **out)
{
  Status status         = UMC_OK;
  Ipp32s part_number = 0;

  if(m_IsInitialized)
  {
    status = DecodeFrameHeader(/*m_Params.m_pData*/in);
    if(UMC_OK != status)
      return UMC_ERR_INIT;
  }
  else
    return UMC_ERR_INIT;

  for(Ipp8u i = 0; i < VP8_NUM_OF_REF_FRAMES; i++)
  {
    if( i != m_RefFrameIndx[VP8_LAST_FRAME]   && 
        i != m_RefFrameIndx[VP8_GOLDEN_FRAME] && 
        i != m_RefFrameIndx[VP8_ALTREF_FRAME])
    {
      m_CurrFrame = &m_FrameData[i];
      m_RefFrameIndx[VP8_INTRA_FRAME] = i;
      break;
    }
  }

  DecodeFirstPartition();

  for(Ipp32u mb_row = 0; mb_row < m_FrameInfo.mbPerCol; mb_row++)
  {
    if(m_FrameInfo.numTokenPartitions >= 1)
    {
      part_number++;

      if(part_number > m_FrameInfo.numTokenPartitions)
        part_number = 1;
    }

    DecodeMbRow(&m_BoolDecoder[part_number], mb_row);

    ReconstructMbRow(&m_BoolDecoder[part_number], mb_row); // Dequant + IDCT + Predict)
  }

  //???
  if(m_FrameInfo.frameType == I_PICTURE || m_RefreshInfo.refreshRefFrame)
    m_QuantInfo.lastGoldenKeyQP = m_QuantInfo.yacQP;


  if(m_FrameInfo.loopFilterLevel != 0)
  {
    if(m_FrameInfo.loopFilterType)
      LoopFilterSimple();
    else
      LoopFilterNormal();
  }

  ExtendFrameBorders(m_CurrFrame);

  // swap frame buffers to update Gold/AltRef/Last frames
  RefreshFrames();

  if (!m_RefreshInfo.refreshProbabilities)
    MFX_INTERNAL_CPY(&m_FrameProbs, &m_FrameProbs_saved, sizeof(m_FrameProbs));

   VideoDataInfo info;
   info.Init(m_Params.info.clip_info.width, m_Params.info.clip_info.height, NV12, 8);
   info.SetPictureStructure(VideoDataInfo::PS_FRAME);

   
   FrameMemID frmMID;
   status = m_frameAllocator->Alloc(&frmMID, &info, 0);

   if (status != UMC_OK)
   {
       return UMC_ERR_ALLOC;
   }

    const FrameData *frmData = m_frameAllocator->Lock(frmMID);
    if (!frmData)
        return UMC_ERR_LOCK;

    *out = (FrameData *)frmData;

    (*out)->SetPlanePointer(m_CurrFrame->data_y, 0, m_CurrFrame->step_y);
    (*out)->SetPlanePointer(m_CurrFrame->data_u, 1, m_CurrFrame->step_uv);

// temp frame saving
/*  if(m_FrameInfo.showFrame)
  {
    Ipp32s i;

    FILE *pFile = fopen("c:\\_mediasdk\\dump.yuv", "ab+");

    for(i = 0; i < m_FrameInfo.frameSize.height; i++)
    {
      fwrite(m_CurrFrame->data_y + m_CurrFrame->step_y*i, 1 ,m_FrameInfo.frameSize.width, pFile);
    }


    for(i = 0; i < (m_FrameInfo.frameSize.height + 1)>> 1; i++)
    {
      fwrite(m_CurrFrame->data_u + m_CurrFrame->step_uv*i, 1, (m_FrameInfo.frameSize.width + 1) >> 1, pFile);
    }

    for(i = 0; i < (m_FrameInfo.frameSize.height + 1) >> 1; i++)
    {
      fwrite(m_CurrFrame->data_v + m_CurrFrame->step_uv*i, 1, (m_FrameInfo.frameSize.width + 1) >> 1, pFile);
    }

    fclose(pFile);
  }*/
// end temp saving


  return UMC_OK;
} // VP8VideoDecoder::GetFrame()

Status VP8VideoDecoder::GetInfo(BaseCodecParams* info)
{
  info;
  return UMC_OK;
} // VP8VideoDecoder::GetInfo()


Status VP8VideoDecoder::Close(void)
{
  for(Ipp32s i = 0; i < VP8_NUM_OF_REF_FRAMES; i++)
  {
    if(m_FrameData[i].base_y != 0)
      vp8dec_Free(m_FrameData[i].base_y);

    if(m_FrameData[i].base_u != 0)
      vp8dec_Free(m_FrameData[i].base_u);

    if(m_FrameData[i].base_v != 0)
      vp8dec_Free(m_FrameData[i].base_v);
  }

  return UMC_OK;
} // VP8VideoDecoder::Close()


Status VP8VideoDecoder::Reset(void)
{
  return UMC_OK;
} // VP8VideoDecoder::Reset()


Status VP8VideoDecoder::SetParams(BaseCodecParams* params)
{
  params;
  return UMC_OK;
} // VP8VideoDecoder::SetParams()


Status VP8VideoDecoder::GetPerformance(Ipp64f *perf)
{
  perf;
  return UMC_OK;
} // VP8VideoDecoder::GetPerformance()


Status VP8VideoDecoder::ResetSkipCount(void)
{
  return UMC_OK;
} // VP8VideoDecoder::ResetSkipCount()


Ipp32u VP8VideoDecoder::GetNumOfSkippedFrames(void)
{
  return 0;
} // VP8VideoDecoder::GetNumOfSkippedFrames()


Status VP8VideoDecoder::SkipVideoFrame(Ipp32s count)
{
  count;
  return UMC_OK;
} // P8VideoDecoder::SkipVideoFrame()


//} // end namespace UMC

Status VP8VideoDecoder::DecodeFrameHeader(MediaData *in)
{
  Status status = UMC_OK;

  Ipp8u* data_in     = 0;
  Ipp8u* data_in_end = 0;
  Ipp8u  version;
  Ipp32u i           = 0;
  Ipp32u j           = 0;
  Ipp16s width       = 0;
  Ipp16s height      = 0;

  data_in = (Ipp8u*)in->GetDataPointer();
  if(!data_in)
    return UMC_ERR_NULL_PTR;

  data_in_end = data_in + in->GetDataSize();

  //suppose that Intraframes -> I_PICTURE ( == VP8_KEY_FRAME)
  //             Interframes -> P_PICTURE
  m_FrameInfo.frameType = (data_in[0] & 1) ? P_PICTURE : I_PICTURE; // 1 bits
  version               = (data_in[0] >> 1) & 0x7;                  // 3 bits
  m_FrameInfo.showFrame = (data_in[0] >> 4) & 0x01;                 // 1 bits

  switch (version) 
  {
  case 1:
  case 2:
    m_FrameInfo.interpolationFlags = VP8_BILINEAR_INTERP;
    break;

  case 3:
    m_FrameInfo.interpolationFlags = VP8_BILINEAR_INTERP | VP8_CHROMA_FULL_PEL;
    break;

  case 0:
  default:
    m_FrameInfo.interpolationFlags = 0;
    break;
  }

  Ipp32u first_partition_size = (data_in[0] >> 5) |           // 19 bit
                                (data_in[1] << 3) |
                                (data_in[2] << 11);

  m_FrameInfo.partitionSize[VP8_FIRST_PARTITION] = first_partition_size;

  data_in   += 3;
//  data_size -= 3;

  if (m_FrameInfo.frameType == I_PICTURE)  // if VP8_KEY_FRAME
  {
    if (first_partition_size > in->GetDataSize() - 10)
      return UMC_ERR_NOT_ENOUGH_DATA; 

    if (!(VP8_START_CODE_FOUND(data_in))) // (0x9D && 0x01 && 0x2A)
      return UMC_ERR_FAILED;

    width               = ((data_in[4] << 8) | data_in[3]) & 0x3FFF;
    m_FrameInfo.h_scale = data_in[4] >> 6;
    height              = ((data_in[6] << 8) | data_in[5]) & 0x3FFF;
    m_FrameInfo.v_scale = data_in[6] >> 6;

    m_FrameInfo.frameSize.width  = width;
    m_FrameInfo.frameSize.height = height;

    width  = (m_FrameInfo.frameSize.width  + 15) & ~0xF;
    height = (m_FrameInfo.frameSize.height + 15) & ~0xF;

    if(width != m_FrameInfo.frameWidth ||  height != m_FrameInfo.frameHeight)
    {
      m_FrameInfo.frameWidth  = width;
      m_FrameInfo.frameHeight = height;

      status = AllocFrames();
      if(UMC_OK != status)
        return UMC_ERR_ALLOC;

      for(Ipp8u i = 0; i < VP8_NUM_OF_REF_FRAMES; i++)
      {
        m_RefFrameIndx[i] = i;
      }
    }

    CHECK_N_REALLOC_BUFFERS;

    data_in   += 7;
//    data_size -= 7;

    MFX_INTERNAL_CPY((Ipp8u*)(m_FrameProbs.coeff_probs),
           (Ipp8u*)vp8_default_coeff_probs,
           sizeof(vp8_default_coeff_probs)); //???

    UMC_SET_ZERO(m_FrameInfo.segmentFeatureData);
    m_FrameInfo.segmentAbsMode = 0;

    UMC_SET_ZERO(m_FrameInfo.refLoopFilterDeltas);
    UMC_SET_ZERO(m_FrameInfo.modeLoopFilterDeltas);

    m_RefreshInfo.refreshRefFrame = 3; // refresh alt+gold
    m_RefreshInfo.copy2Golden = 0;
    m_RefreshInfo.copy2Altref = 0;

    // restore default probabilities for Inter frames
    for (i = 0; i < VP8_NUM_MB_MODES_Y - 1; i++)
      m_FrameProbs.mbModeProbY[i] = vp8_mb_mode_y_probs[i];

    for (i = 0; i < VP8_NUM_MB_MODES_UV - 1; i++)
      m_FrameProbs.mbModeProbUV[i] = vp8_mb_mode_uv_probs[i];

    // restore default MV contexts
    MFX_INTERNAL_CPY(m_FrameProbs.mvContexts, vp8_default_mv_contexts, sizeof(vp8_default_mv_contexts));
    
  }
//  else if (first_partition_size > data_size/* - 3*/)
  else if (first_partition_size > in->GetDataSize() - 3)
    return UMC_ERR_NOT_ENOUGH_DATA; //???

  status = InitBooleanDecoder(data_in, Ipp32s(data_in_end - data_in), 0); //???
  if(UMC_OK != status)
    return UMC_ERR_INIT;

  vp8BooleanDecoder *pBoolDec = &m_BoolDecoder[0];

  Ipp8u bits;

  if (m_FrameInfo.frameType == I_PICTURE)  // if VP8_KEY_FRAME
  {
    bits = DecodeValue_Prob128(pBoolDec, 2);

    m_FrameInfo.color_space_type = (Ipp8u)bits >> 1;
    m_FrameInfo.clamping_type    = (Ipp8u)bits & 1;

    // supported only color_space_type == 0
    // see "VP8 Data Format and Decoding Guide" ch.9.2
    if(m_FrameInfo.color_space_type)
      return UMC_ERR_UNSUPPORTED;
  }

  VP8_DECODE_BOOL(pBoolDec, 128, m_FrameInfo.segmentationEnabled);

  if (m_FrameInfo.segmentationEnabled)
  {
    UpdateSegmentation(pBoolDec);
  }
  else
  {
    m_FrameInfo.updateSegmentData = 0;
    m_FrameInfo.updateSegmentMap  = 0;
  }

    bits = DecodeValue_Prob128(pBoolDec, 7);

  m_FrameInfo.loopFilterType  = (Ipp8u)(bits >> 6);
  m_FrameInfo.loopFilterLevel = (Ipp8u)(bits & 0x3F);

  bits = DecodeValue_Prob128(pBoolDec, 4);

  m_FrameInfo.sharpnessLevel     = (Ipp8u)(bits >> 1);
  m_FrameInfo.mbLoopFilterAdjust = (Ipp8u)(bits & 1);

  if (m_FrameInfo.mbLoopFilterAdjust)
  {
    VP8_DECODE_BOOL(pBoolDec, 128,  bits);

    if (bits)
      UpdateLoopFilterDeltas(pBoolDec);
  }

  Ipp32u partitions;

  bits = DecodeValue_Prob128(pBoolDec, 2);

  partitions                     = 1 << bits;
  m_FrameInfo.numTokenPartitions = 1 << bits;

  m_FrameInfo.numPartitions = m_FrameInfo.numTokenPartitions + 1; // ??? do we need 1at partition here?

  Ipp8u *pTokenPartition = data_in + first_partition_size;

  if (partitions > 1)
  {
    Ipp32u i;

    m_FrameInfo.partitionStart[0] = pTokenPartition + (partitions - 1) * 3;

    for (i = 0; i < partitions - 1; i++)
    {
      m_FrameInfo.partitionSize[i] = (Ipp32s)(pTokenPartition[0])      |
                                             (pTokenPartition[1] << 8) |
                                             (pTokenPartition[2] << 16);
      pTokenPartition += 3;

      m_FrameInfo.partitionStart[i+1] = m_FrameInfo.partitionStart[i] + m_FrameInfo.partitionSize[i];

      if (m_FrameInfo.partitionStart[i+1] > data_in_end)
        return UMC_ERR_NOT_ENOUGH_DATA; //???

      InitBooleanDecoder(m_FrameInfo.partitionStart[i], m_FrameInfo.partitionSize[i], i+1);
    }
  }
  else
  {
    m_FrameInfo.partitionStart[0] = pTokenPartition;
  }

  m_FrameInfo.partitionSize[partitions - 1] = Ipp32s(data_in_end - m_FrameInfo.partitionStart[partitions - 1]);

  status = InitBooleanDecoder(m_FrameInfo.partitionStart[partitions - 1], m_FrameInfo.partitionSize[partitions - 1], partitions);
  if(UMC_OK != status)
    return UMC_ERR_INIT;

  DecodeInitDequantization();

  if (m_FrameInfo.frameType != I_PICTURE) // data in header for non-KEY frames
  {
    m_RefreshInfo.refreshRefFrame = DecodeValue_Prob128(pBoolDec, 2);

    if (!(m_RefreshInfo.refreshRefFrame & 2))
      m_RefreshInfo.copy2Golden = DecodeValue_Prob128(pBoolDec, 2);

    if (!(m_RefreshInfo.refreshRefFrame & 1))
      m_RefreshInfo.copy2Altref = DecodeValue_Prob128(pBoolDec, 2);

    Ipp8u bias = DecodeValue_Prob128(pBoolDec, 2);

    m_RefreshInfo.refFrameBiasTable[1] = (bias & 1)^(bias >> 1); // ALTREF and GOLD (3^2 = 1)
    m_RefreshInfo.refFrameBiasTable[2] = (bias & 1);             // ALTREF and LAST
    m_RefreshInfo.refFrameBiasTable[3] = (bias >> 1);            // GOLD and LAST
  }

  VP8_DECODE_BOOL(pBoolDec, 128, m_RefreshInfo.refreshProbabilities);

  if (!m_RefreshInfo.refreshProbabilities)
    MFX_INTERNAL_CPY(&m_FrameProbs_saved, &m_FrameProbs, sizeof(m_FrameProbs));

  if (m_FrameInfo.frameType != I_PICTURE)
  {
    VP8_DECODE_BOOL_PROB128(pBoolDec, m_RefreshInfo.refreshLastFrame);
  }
  else
    m_RefreshInfo.refreshLastFrame = 1;

  UpdateCoeffProbabilitites();

  memset(m_FrameInfo.blContextUp, 0, m_FrameInfo.mbPerRow*9);

  VP8_DECODE_BOOL(pBoolDec, 128, m_FrameInfo.mbSkipEnabled);

  if (m_FrameInfo.mbSkipEnabled)
    m_FrameInfo.skipFalseProb = DecodeValue_Prob128(pBoolDec, 8);


  //set to zero Mb coeffs
  for(i = 0; i < m_FrameInfo.mbPerCol; i++)
    for(j = 0; j < m_FrameInfo.mbPerRow; j++)
      UMC_SET_ZERO(m_pMbInfo[i*m_FrameInfo.mbStep + j].coeffs);


  /*
Token partitions decoding DEPENDS on the first partition
  via mb_skip_coeff !!! (???) - either synchronize or decode 1st partition single-threaded (???)
*/
/*  
  // ??? need this?
  bs->DataLength -= p - (bs->Data + bs->DataOffset);
  bs->DataOffset = p - bs->Data;
  */
  return UMC_OK;
}


Status VP8VideoDecoder::DecodeFirstPartition(void)
{
  if (m_FrameInfo.frameType == I_PICTURE) // if VP8_KEY_FRAME
    DecodeMbModes_Intra(&m_BoolDecoder[0]);
  else
    DecodeMbModesMVs_Inter(&m_BoolDecoder[0]);

  return UMC_OK;
}


#define VP8_FRAME_BORDER_SIZE 32

Status VP8VideoDecoder::AllocFrames(void)
{
  Ipp32s i = 0;
  Ipp32u size;

  for(i = 0; i < VP8_NUM_OF_REF_FRAMES; i++)
  {
    if(m_FrameData[i].base_y != 0)
      vp8dec_Free(m_FrameData[i].base_y);

    if(m_FrameData[i].base_u != 0)
      vp8dec_Free(m_FrameData[i].base_u);

    if(m_FrameData[i].base_v != 0)
      vp8dec_Free(m_FrameData[i].base_v);

    m_FrameData[i].size.width  = m_FrameInfo.frameSize.width;
    m_FrameData[i].size.height = m_FrameInfo.frameSize.height;

    // Y buffers
    m_FrameData[i].step_y = m_FrameInfo.frameWidth + 2*VP8_FRAME_BORDER_SIZE;

    size = m_FrameData[i].step_y * (m_FrameInfo.frameHeight + 2*VP8_FRAME_BORDER_SIZE);

    m_FrameData[i].base_y = (Ipp8u*)vp8dec_Malloc(size);
    if(0 == m_FrameData[i].base_y)
      return UMC_ERR_ALLOC;

    m_FrameData[i].data_y = m_FrameData[i].base_y + 
                            m_FrameData[i].step_y*VP8_FRAME_BORDER_SIZE + 
                            VP8_FRAME_BORDER_SIZE;

    // UV buffers
    m_FrameData[i].step_uv = (m_FrameInfo.frameWidth >> 1) + 2*VP8_FRAME_BORDER_SIZE;

    size = m_FrameData[i].step_uv * ((m_FrameInfo.frameHeight >> 1) + 2*VP8_FRAME_BORDER_SIZE);

    m_FrameData[i].base_u = (Ipp8u*)vp8dec_Malloc(size);
    if(0 == m_FrameData[i].base_u)
      return UMC_ERR_ALLOC;

    m_FrameData[i].base_v = (Ipp8u*)vp8dec_Malloc(size);
    if(0 == m_FrameData[i].base_v)
      return UMC_ERR_ALLOC;

    m_FrameData[i].data_u = m_FrameData[i].base_u + 
                            m_FrameData[i].step_uv*VP8_FRAME_BORDER_SIZE + 
                            VP8_FRAME_BORDER_SIZE;

    m_FrameData[i].data_v = m_FrameData[i].base_v + 
                            m_FrameData[i].step_uv*VP8_FRAME_BORDER_SIZE + 
                            VP8_FRAME_BORDER_SIZE;

    m_FrameData[i].border_size = VP8_FRAME_BORDER_SIZE;
  }

  return UMC_OK;
} // VP8VideoDecoder::AllocFrames()


void VP8VideoDecoder::RefreshFrames(void)
{

  if(m_FrameInfo.frameType == I_PICTURE)
  {
    m_RefFrameIndx[VP8_ALTREF_FRAME] = m_RefFrameIndx[VP8_INTRA_FRAME];
    m_RefFrameIndx[VP8_GOLDEN_FRAME] = m_RefFrameIndx[VP8_INTRA_FRAME];
  }
  else
  {
    Ipp8u gold_indx = m_RefFrameIndx[VP8_GOLDEN_FRAME];
    Ipp8u alt_indx  = m_RefFrameIndx[VP8_ALTREF_FRAME];

    if(!(m_RefreshInfo.refreshRefFrame & 2)) // refresh golden
    {
      switch(m_RefreshInfo.copy2Golden)
      {
      case 1: gold_indx= m_RefFrameIndx[VP8_LAST_FRAME];
        break;

      case 2: gold_indx = m_RefFrameIndx[VP8_ALTREF_FRAME];
        break;

      case 0:
      default:
          break;
      }
    }
    else
      gold_indx = m_RefFrameIndx[VP8_INTRA_FRAME];


    if(!(m_RefreshInfo.refreshRefFrame & 1)) // refresh alt
    {
      switch(m_RefreshInfo.copy2Altref)
      {
      case 1: alt_indx = m_RefFrameIndx[VP8_LAST_FRAME];
        break;

      case 2: alt_indx = m_RefFrameIndx[VP8_GOLDEN_FRAME];
        break;

      case 0:
      default:
          break;
      }
    }
    else
       alt_indx = m_RefFrameIndx[VP8_INTRA_FRAME];

    m_RefFrameIndx[VP8_GOLDEN_FRAME] = gold_indx;
    m_RefFrameIndx[VP8_ALTREF_FRAME] = alt_indx;
  }

  if(m_RefreshInfo.refreshLastFrame)
    m_RefFrameIndx[VP8_LAST_FRAME] = m_RefFrameIndx[VP8_INTRA_FRAME];

  return;
} // VP8VideoDecoder::RefreshFrames


void VP8VideoDecoder::ExtendFrameBorders(vp8_FrameData* currFrame)
{
  Ipp32s i;

  Ipp8u* ptr1;
  Ipp8u* ptr2;

  //extend Y LEFT/RIGHT border
  ptr1 = currFrame->data_y;

  for(i = 0; i < m_FrameInfo.frameHeight; i++)
  {
    memset(ptr1 - currFrame->border_size,
           ptr1[0],
           currFrame->border_size);

    memset(ptr1 + m_FrameInfo.frameWidth,
           ptr1[m_FrameInfo.frameWidth - 1],
           currFrame->border_size);

    ptr1 += currFrame->step_y;
  }

  //extend Y TOP/BOTTOM border
  ptr1 = currFrame->data_y - currFrame->border_size;
  ptr2 = currFrame->data_y + currFrame->step_y*(m_FrameInfo.frameHeight-1) - currFrame->border_size;

  for(i = 0; i < currFrame->border_size; i++)
  {
    ippsCopy_8u(ptr1, currFrame->base_y + i*currFrame->step_y, currFrame->step_y);
    ippsCopy_8u(ptr2, ptr2 + (i + 1)*currFrame->step_y, currFrame->step_y);
  }


  //extend UV LEFT/RIGHT border
  ptr1 = currFrame->data_u;
  ptr2 = currFrame->data_v;

  for(i = 0; i < m_FrameInfo.frameHeight >> 1; i++)
  {
    memset(ptr1 - currFrame->border_size,
           ptr1[0],
           currFrame->border_size);

    memset(ptr1 + (m_FrameInfo.frameWidth >> 1),
           ptr1[(m_FrameInfo.frameWidth >> 1) - 1],
           currFrame->border_size);

    memset(ptr2 - currFrame->border_size,
           ptr2[0],
           currFrame->border_size);

    memset(ptr2 + (m_FrameInfo.frameWidth >> 1),
           ptr2[(m_FrameInfo.frameWidth >> 1) - 1],
           currFrame->border_size);

    ptr1 += currFrame->step_uv;
    ptr2 += currFrame->step_uv;
  }


  //extend UV TOP/BOTTOM border
  ptr1 = currFrame->data_u - currFrame->border_size;
  ptr2 = currFrame->data_u + currFrame->step_uv*((m_FrameInfo.frameHeight>>1)-1) - currFrame->border_size;

  for(i = 0; i < currFrame->border_size; i++)
  {
    ippsCopy_8u(ptr1, currFrame->base_u + i*currFrame->step_uv, currFrame->step_uv);
    ippsCopy_8u(ptr2, ptr2 + (i + 1)*currFrame->step_uv, currFrame->step_uv);
  }

  ptr1 = currFrame->data_v - currFrame->border_size;
  ptr2 = currFrame->data_v + currFrame->step_uv*((m_FrameInfo.frameHeight>>1)-1) - currFrame->border_size;

  for(i = 0; i < currFrame->border_size; i++)
  {
    ippsCopy_8u(ptr1, currFrame->base_v + i*currFrame->step_uv, currFrame->step_uv);
    ippsCopy_8u(ptr2, ptr2 + (i + 1)*currFrame->step_uv, currFrame->step_uv);
  }


  return;
} // VP8VideoDecoder::ExtendFrameBorders()

void VP8VideoDecoder::SetFrameAllocator(FrameAllocator * frameAllocator)
{
    VM_ASSERT(frameAllocator);
    m_frameAllocator = frameAllocator;
} // void VP8VideoDecoder::SetFrameAllocator(FrameAllocator * frameAllocator)


#endif // UMC_ENABLE_VP8_VIDEO_DECODER
