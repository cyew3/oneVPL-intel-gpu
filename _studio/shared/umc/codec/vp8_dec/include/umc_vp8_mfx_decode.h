//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#ifndef __UMC_VP8_MFX_DECODER__
#define __UMC_VP8_MFX_DECODER__

#include "umc_video_decoder.h"
#include "umc_media_buffer.h"
#include "umc_frame_data.h"
#include "umc_frame_allocator.h"

#include "vp8_dec_defs.h"
#include "vp8_bool_dec.h"
#include "ipps.h"

#include "mfxvideo++int.h"

namespace UMC
{

class VP8VideoDecoderSoftware
{

public:
    // Default constructor
    VP8VideoDecoderSoftware(void);

    // Destructor
    virtual ~VP8VideoDecoderSoftware(void);

    // Initialize for subsequent frame decoding.
    virtual Status Init(BaseCodecParams* pInit);

    // Reset decoder to initial state
    virtual Status Reset(void);

    // Close decoding & free all allocated resources
    virtual Status Close(void);

    // Get next frame
    virtual Status GetFrame(MediaData* in, FrameData** out);

    virtual void SetFrameAllocator(FrameAllocator * frameAllocator);

    Status DecodeFrameHeader(MediaData* in);

    Status FillVideoParam(mfxVideoParam *par, bool full);

protected:

    Status InitBooleanDecoder(Ipp8u *pBitStream, Ipp32s dataSize, Ipp32s dec_number);

    Status UpdateSegmentation(vp8BooleanDecoder *pBooldec);
    void UpdateCoeffProbabilitites(void);

    Status UpdateLoopFilterDeltas(vp8BooleanDecoder *pBooldec);

    Status AllocateFrame();

    Status DecodeInitDequantization(void);

    // Status DecodeMbModes_Intra(vp8BooleanDecoder *pBoolDec);
    // Status DecodeMbModesMVs_Inter(vp8BooleanDecoder *pBoolDec);

    void SetMbQuant(vp8_MbInfo *pMb);
    void DequantMbCoeffs(vp8_MbInfo* pMb);

    void LoopFilterNormal(void);
    void LoopFilterSimple(void);
    void LoopFilterInit(void);

    void RefreshFrames(void);

    void ExtendFrameBorders(vp8_FrameData* currFrame);

    Ipp8u DecodeValue_Prob128(vp8BooleanDecoder *pBooldec, Ipp32u numbits);
    Ipp8u DecodeValue(vp8BooleanDecoder *pBooldec, Ipp8u prob, Ipp32u numbits);

protected:

    Ipp8u                  m_isInitialized;
    vp8_MbInfo            *m_pMbInfo;
    vp8_MbInfo             m_mbExternal;
    vp8_FrameInfo          m_frameInfo;
    vp8_QuantInfo          m_quantInfo;
    vp8BooleanDecoder      m_boolDecoder[VP8_MAX_NUMBER_OF_PARTITIONS];
    vp8_RefreshInfo        m_refreshInfo;
    vp8_FrameProbabilities m_frameProbs;
    vp8_FrameProbabilities m_frameProbs_saved;

    MemID                  m_frameMID;
    FrameMemID             m_mid;
    VideoDecoderParams     m_decodedParams;

    FrameData             m_FrameData[VP8_NUM_OF_REF_FRAMES];
    FrameData*            m_currFrame;
    Ipp8u                 m_RefFrameIndx[VP8_NUM_OF_REF_FRAMES];

    FrameAllocator        *m_pFrameAllocator;
    MemoryAllocator       *m_pMemoryAllocator;
};

} // namespace UMC


#endif // __UMC_VP8_MFX_DECODER__
#endif // UMC_ENABLE_VP8_VIDEO_DECODER