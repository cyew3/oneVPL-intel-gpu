//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2013 Intel Corporation. All Rights Reserved.
//

#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#include "umc_vp8_mfx_decode.h"

#define CHECK_N_REALLOC_BUFFERS \
{ \
  Ipp32u mbPerCol, mbPerRow; \
  mbPerCol = m_frameInfo.frameHeight >> 4; \
  mbPerRow = m_frameInfo.frameWidth  >> 4; \
  if (m_frameInfo.mbPerCol * m_frameInfo.mbPerRow  < mbPerRow * mbPerCol) \
  { \
    if (m_pMbInfo) \
      vp8dec_Free(m_pMbInfo); \
      \
    m_pMbInfo = (vp8_MbInfo*)vp8dec_Malloc(mbPerRow * mbPerCol * sizeof(vp8_MbInfo)); \
  } \
  if (m_frameInfo.mbPerRow <  mbPerRow) \
  { \
    if (m_frameInfo.blContextUp) \
      vp8dec_Free(m_frameInfo.blContextUp); \
      \
    m_frameInfo.blContextUp = (Ipp8u*)vp8dec_Malloc(mbPerRow * mbPerCol * sizeof(vp8_MbInfo)); \
  } \
  m_frameInfo.mbPerCol = mbPerCol; \
  m_frameInfo.mbPerRow = mbPerRow; \
  m_frameInfo.mbStep   = mbPerRow; \
}

VP8VideoDecoderSoftware::VP8VideoDecoderSoftware(void)
{
    m_frameMID = 0;

} // VP8VideoDecoderSoftware::VP8VideoDecoderSoftware(void)

VP8VideoDecoderSoftware::~VP8VideoDecoderSoftware(void)
{
    Close();

} // VP8VideoDecoderSoftware::~VP8VideoDecoderSoftware(void)

Status VP8VideoDecoderSoftware::Close(void)
{
    if (0 != m_frameMID)
    {
        m_pMemoryAllocator->Unlock(m_frameMID);
        m_pMemoryAllocator->Free(m_frameMID);
        m_frameMID = 0;
    }

    m_isInitialized = false;

    //m_pVideoAccelerator = NULL;

    return UMC_OK;

} // Status VP8VideoDecoderSoftware::Close(void)

Status VP8VideoDecoderSoftware::Reset(void)
{
    if (0 != m_frameMID)
    {
        m_pMemoryAllocator->Unlock(m_frameMID);
        m_pMemoryAllocator->Free(m_frameMID);
        m_frameMID = 0;
    }

    m_isInitialized = true;

/*
    m_frameData.Close();
    m_internalFrame.Close();
    m_dec->Reset();
    m_local_frame_time = 0;
*/

    return UMC_OK;

} // Status VP8VideoDecoderSoftware::Reset(void)

Status VP8VideoDecoderSoftware::Init(BaseCodecParams *pInit)
{
    UMC::Status status;

    VideoDecoderParams* pDecoderParams;

    pDecoderParams = DynamicCast<VideoDecoderParams>(pInit);

    if(0 == pDecoderParams)
        return UMC_ERR_NULL_PTR;

    status = Close();
    if(UMC_OK != status)
        return UMC_ERR_INIT;

    m_decodedParams = *pDecoderParams;

    m_isInitialized = true;

    m_pMemoryAllocator = pDecoderParams->lpMemoryAllocator;
    //m_pVideoAccelerator = pDecoderParams->pVideoAccelerator;

    VM_ASSERT(m_pMemoryAllocator);

    m_frameInfo.mbPerCol         = 0;
    m_frameInfo.mbPerRow         = 0;
    m_frameInfo.frameSize.width  = 0;
    m_frameInfo.frameSize.height = 0;
    m_frameInfo.frameWidth       = 0;
    m_frameInfo.frameHeight      = 0;

    m_frameInfo.blContextUp    = 0;

    m_pMbInfo       = 0;

    for (Ipp8u i = 0; i < VP8_NUM_OF_REF_FRAMES; i++)
    {
        UMC_SET_ZERO(m_FrameData[i]);
        m_RefFrameIndx[i] = i;
    }

    m_currFrame = &m_FrameData[m_RefFrameIndx[VP8_INTRA_FRAME]];

    m_refreshInfo.copy2Golden          = 0;
    m_refreshInfo.copy2Altref          = 0;
    m_refreshInfo.refreshLastFrame     = 0;
    m_refreshInfo.refreshProbabilities = 0;
    m_refreshInfo.refreshRefFrame      = 0;

    return UMC_OK;

} // Status VP8VideoDecoderSoftware::Init(BaseCodecParams *pInit)

Status VP8VideoDecoderSoftware::DecodeFrameHeader(MediaData *in)
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
    m_frameInfo.frameType = (data_in[0] & 1) ? P_PICTURE : I_PICTURE; // 1 bits
    version               = (data_in[0] >> 1) & 0x7;                  // 3 bits
    m_frameInfo.showFrame = (data_in[0] >> 4) & 0x01;                 // 1 bits

    switch (version) 
    {
        case 1:
        case 2:
            m_frameInfo.interpolationFlags = VP8_BILINEAR_INTERP;
            break;

        case 3:
            m_frameInfo.interpolationFlags = VP8_BILINEAR_INTERP | VP8_CHROMA_FULL_PEL;
            break;

        case 0:
        default:
            m_frameInfo.interpolationFlags = 0;
            break;
    }

    Ipp32u first_partition_size = (data_in[0] >> 5) |           // 19 bit
                                  (data_in[1] << 3) |
                                  (data_in[2] << 11);

    m_frameInfo.partitionSize[VP8_FIRST_PARTITION] = first_partition_size;

    data_in   += 3;

    if (m_frameInfo.frameType == I_PICTURE)  // if VP8_KEY_FRAME
    {
        if (first_partition_size > in->GetDataSize() - 10)
            return UMC_ERR_NOT_ENOUGH_DATA; 

        if (!(VP8_START_CODE_FOUND(data_in))) // (0x9D && 0x01 && 0x2A)
            return UMC_ERR_FAILED;

        width               = ((data_in[4] << 8) | data_in[3]) & 0x3FFF;
        m_frameInfo.h_scale = data_in[4] >> 6;
        height              = ((data_in[6] << 8) | data_in[5]) & 0x3FFF;
        m_frameInfo.v_scale = data_in[6] >> 6;

        m_frameInfo.frameSize.width  = width;
        m_frameInfo.frameSize.height = height;

        width  = (m_frameInfo.frameSize.width  + 15) & ~0xF;
        height = (m_frameInfo.frameSize.height + 15) & ~0xF;

        if (width != m_frameInfo.frameWidth ||  height != m_frameInfo.frameHeight)
        {
            m_frameInfo.frameWidth  = width;
            m_frameInfo.frameHeight = height;

            // alloc frames 
            status = AllocateFrame();
            UMC_CHECK_STATUS(status);

            for(Ipp8u i = 0; i < VP8_NUM_OF_REF_FRAMES; i++)
            {
                m_RefFrameIndx[i] = i;
            }
        }

        CHECK_N_REALLOC_BUFFERS;

        data_in   += 7;


        MFX_INTERNAL_CPY((Ipp8u*)(m_frameProbs.coeff_probs),
               (Ipp8u*)vp8_default_coeff_probs,
               sizeof(vp8_default_coeff_probs)); //???

        UMC_SET_ZERO(m_frameInfo.segmentFeatureData);
        m_frameInfo.segmentAbsMode = 0;

        UMC_SET_ZERO(m_frameInfo.refLoopFilterDeltas);
        UMC_SET_ZERO(m_frameInfo.modeLoopFilterDeltas);

        m_refreshInfo.refreshRefFrame = 3; // refresh alt+gold
        m_refreshInfo.copy2Golden = 0;
        m_refreshInfo.copy2Altref = 0;

        // restore default probabilities for Inter frames
        for (i = 0; i < VP8_NUM_MB_MODES_Y - 1; i++)
            m_frameProbs.mbModeProbY[i] = vp8_mb_mode_y_probs[i];

        for (i = 0; i < VP8_NUM_MB_MODES_UV - 1; i++)
            m_frameProbs.mbModeProbUV[i] = vp8_mb_mode_uv_probs[i];

        // restore default MV contexts
        MFX_INTERNAL_CPY(m_frameProbs.mvContexts, vp8_default_mv_contexts, sizeof(vp8_default_mv_contexts));
        
    }
    else if (first_partition_size > in->GetDataSize() - 3)
        return UMC_ERR_NOT_ENOUGH_DATA; //???

    status = InitBooleanDecoder(data_in, data_in_end - data_in, 0); //???
    if (UMC_OK != status)
        return UMC_ERR_INIT;

    vp8BooleanDecoder *pBoolDec = &m_boolDecoder[0];

    Ipp8u bits;

    if (m_frameInfo.frameType == I_PICTURE)  // if VP8_KEY_FRAME
    {
        bits = DecodeValue_Prob128(pBoolDec, 2);

        m_frameInfo.color_space_type = (Ipp8u)bits >> 1;
        m_frameInfo.clamping_type    = (Ipp8u)bits & 1;

        // supported only color_space_type == 0
        // see "VP8 Data Format and Decoding Guide" ch.9.2
        if(m_frameInfo.color_space_type)
            return UMC_ERR_UNSUPPORTED;
    }

    VP8_DECODE_BOOL(pBoolDec, 128, m_frameInfo.segmentationEnabled);

    if (m_frameInfo.segmentationEnabled)
    {
        UpdateSegmentation(pBoolDec);
    }
    else
    {
        m_frameInfo.updateSegmentData = 0;
        m_frameInfo.updateSegmentMap  = 0;
    }

    bits = DecodeValue_Prob128(pBoolDec, 7);

    m_frameInfo.loopFilterType  = (Ipp8u)(bits >> 6);
    m_frameInfo.loopFilterLevel = (Ipp8u)(bits & 0x3F);

    bits = DecodeValue_Prob128(pBoolDec, 4);

    m_frameInfo.sharpnessLevel     = (Ipp8u)(bits >> 1);
    m_frameInfo.mbLoopFilterAdjust = (Ipp8u)(bits & 1);

    if (m_frameInfo.mbLoopFilterAdjust)
    {
        VP8_DECODE_BOOL(pBoolDec, 128,  bits);

        if (bits)
            UpdateLoopFilterDeltas(pBoolDec);
    }

    Ipp32u partitions;

    bits = DecodeValue_Prob128(pBoolDec, 2);

    partitions                     = 1 << bits;
    m_frameInfo.numTokenPartitions = 1 << bits;

    m_frameInfo.numPartitions = m_frameInfo.numTokenPartitions;// + 1; // ??? do we need 1st partition here?
    partitions =  m_frameInfo.numPartitions;
    Ipp8u *pTokenPartition = data_in + first_partition_size;
    
    if (partitions > 1)
    {
        Ipp32u i;

        m_frameInfo.partitionStart[0] = pTokenPartition + (partitions - 1) * 3;

        for (i = 0; i < partitions - 1; i++)
        {
            m_frameInfo.partitionSize[i] = (Ipp32s)(pTokenPartition[0])      |
                                             (pTokenPartition[1] << 8) |
                                             (pTokenPartition[2] << 16);
            pTokenPartition += 3;

            m_frameInfo.partitionStart[i+1] = m_frameInfo.partitionStart[i] + m_frameInfo.partitionSize[i];

            if (m_frameInfo.partitionStart[i+1] > data_in_end)
                return UMC_ERR_NOT_ENOUGH_DATA; //???

            InitBooleanDecoder(m_frameInfo.partitionStart[i], m_frameInfo.partitionSize[i], i+1);
        }
    }
    else
    {
        m_frameInfo.partitionStart[0] = pTokenPartition;
    }

    m_frameInfo.partitionSize[partitions - 1] = data_in_end - m_frameInfo.partitionStart[partitions - 1];

    status = InitBooleanDecoder(m_frameInfo.partitionStart[partitions - 1], m_frameInfo.partitionSize[partitions - 1], partitions);
    if (UMC_OK != status)
        return UMC_ERR_INIT;

    DecodeInitDequantization();

    if (m_frameInfo.frameType != I_PICTURE) // data in header for non-KEY frames
    {
        m_refreshInfo.refreshRefFrame = DecodeValue_Prob128(pBoolDec, 2);

        if (!(m_refreshInfo.refreshRefFrame & 2))
            m_refreshInfo.copy2Golden = DecodeValue_Prob128(pBoolDec, 2);

        if (!(m_refreshInfo.refreshRefFrame & 1))
            m_refreshInfo.copy2Altref = DecodeValue_Prob128(pBoolDec, 2);

        Ipp8u bias = DecodeValue_Prob128(pBoolDec, 2);

        m_refreshInfo.refFrameBiasTable[1] = (bias & 1)^(bias >> 1); // ALTREF and GOLD (3^2 = 1)
        m_refreshInfo.refFrameBiasTable[2] = (bias & 1);             // ALTREF and LAST
        m_refreshInfo.refFrameBiasTable[3] = (bias >> 1);            // GOLD and LAST
    }

    VP8_DECODE_BOOL(pBoolDec, 128, m_refreshInfo.refreshProbabilities);

    if (!m_refreshInfo.refreshProbabilities)
        MFX_INTERNAL_CPY(&m_frameProbs_saved, &m_frameProbs, sizeof(m_frameProbs));

    if (m_frameInfo.frameType != I_PICTURE)
    {
        VP8_DECODE_BOOL_PROB128(pBoolDec, m_refreshInfo.refreshLastFrame);
    }
    else
        m_refreshInfo.refreshLastFrame = 1;

    UpdateCoeffProbabilitites();

    memset(m_frameInfo.blContextUp, 0, m_frameInfo.mbPerRow*9);

    VP8_DECODE_BOOL(pBoolDec, 128, m_frameInfo.mbSkipEnabled);

    m_frameInfo.skipFalseProb = 0;

    if (m_frameInfo.mbSkipEnabled)
        m_frameInfo.skipFalseProb = DecodeValue_Prob128(pBoolDec, 8);

    m_frameInfo.intraProb = DecodeValue_Prob128(pBoolDec, 8);
    m_frameInfo.lastProb = DecodeValue_Prob128(pBoolDec, 8);
    m_frameInfo.goldProb = DecodeValue_Prob128(pBoolDec, 8);

    //set to zero Mb coeffs
    for (i = 0; i < m_frameInfo.mbPerCol; i++)
        for(j = 0; j < m_frameInfo.mbPerRow; j++)
            UMC_SET_ZERO(m_pMbInfo[i*m_frameInfo.mbStep + j].coeffs);

    return UMC_OK;

  /*
Token partitions decoding DEPENDS on the first partition
  via mb_skip_coeff !!! (???) - either synchronize or decode 1st partition single-threaded (???)
*/
/*  
  // ??? need this?
  bs->DataLength -= p - (bs->Data + bs->DataOffset);
  bs->DataOffset = p - bs->Data;
  */
} // Status VP8VideoDecoderSoftware::DecodeHeader(MediaData *in)

Status VP8VideoDecoderSoftware::GetFrame(MediaData* in, FrameData** out)
{
    Status sts = UMC_OK;

    in; out;

    return sts;

} // Status VP8VideoDecoderSoftware::GetFrame(MediaData* in, FrameData** out)

void VP8VideoDecoderSoftware::SetFrameAllocator(FrameAllocator * frameAllocator)
{
    VM_ASSERT(frameAllocator);
    m_pFrameAllocator = frameAllocator;

} // void VP8VideoDecoderSoftware::SetFrameAllocator(FrameAllocator * frameAllocator)

Status VP8VideoDecoderSoftware::FillVideoParam(mfxVideoParam *par, bool /*full*/)
{
    memset(par, 0, sizeof(mfxVideoParam));

    par->mfx.CodecId = MFX_CODEC_VP8;

    par->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

    //IppiSize size = m_frameDims;
    //AdjustFrameSize(size);

    par->mfx.FrameInfo.Width = 0;//mfxU16(size.width);
    par->mfx.FrameInfo.Height = 0;//mfxU16(size.height);

    par->mfx.FrameInfo.CropX = 0;
    par->mfx.FrameInfo.CropY = 0;
    par->mfx.FrameInfo.CropH = par->mfx.FrameInfo.Height;
    par->mfx.FrameInfo.CropW = par->mfx.FrameInfo.Width;

    par->mfx.FrameInfo.CropW = UMC::align_value<mfxU16>(par->mfx.FrameInfo.CropW, 2);
    par->mfx.FrameInfo.CropH = UMC::align_value<mfxU16>(par->mfx.FrameInfo.CropH, 2);

    par->mfx.FrameInfo.PicStruct = (mfxU8)(MFX_PICSTRUCT_PROGRESSIVE);
    par->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

    par->mfx.FrameInfo.AspectRatioW = 0;
    par->mfx.FrameInfo.AspectRatioH = 0;

    par->mfx.FrameInfo.FrameRateExtD = 1;
    par->mfx.FrameInfo.FrameRateExtN = 30;

    par->mfx.CodecProfile = 1;
    par->mfx.CodecLevel = 1;

    par->mfx.NumThread = 0;

    return UMC_OK;

} // Status VP8VideoDecoderSoftware::FillVideoParam(mfxVideoParam *par, bool /*full*/)

Status VP8VideoDecoderSoftware::AllocateFrame()
{
    IppiSize size = m_frameInfo.frameSize;

    VideoDataInfo info;
    info.Init(size.width, size.height, NV12, 8);

    FrameMemID frmMID;
    Status sts = m_pFrameAllocator->Alloc(&frmMID, &info, 0);

    if (sts != UMC_OK)
    {
        return UMC_ERR_ALLOC;
    }

    m_currFrame->Init(&info, frmMID, m_pFrameAllocator);

    return UMC_OK;

} // Status VP8VideoDecoderSoftware::AllocateFrame()

#endif