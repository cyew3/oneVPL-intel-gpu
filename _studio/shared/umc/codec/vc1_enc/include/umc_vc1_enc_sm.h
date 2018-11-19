// Copyright (c) 2008-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _ENCODER_VC1_SM_H_
#define _ENCODER_VC1_SM_H_

#include "umc_vc1_enc_sequence_sm.h"
#include "umc_vc1_enc_picture_sm.h"
//#include "umc_vc1_video_encoder.h"
#include "umc_vc1_enc_bit_rate_control.h"
#include "umc_vc1_enc_planes.h"
#include "umc_scene_analyzer.h"

#include "umc_me.h"
#include "umc_vme.h"

namespace UMC_VC1_ENCODER
{

#define N_FRAMES    10

class VC1EncoderSM
{
private:

    VC1EncoderBitStreamSM   *m_pCodedFrame;
    VC1EncoderSequenceSM    *m_SH;
    VC1EncoderPictureSM     *m_pCurrPicture;

    VC1EncoderMBs           *m_pMBs;
    UMC::MemID               m_MBsID;
    uint8_t*                   m_MBsBuffer;

    VC1EncoderCodedMB       *m_pCodedMB;

    ///////////////////
    VC1BitRateControl          *m_pBitRateControl;
    uint8_t                      *m_pBRCBuffer;
    UMC::MemID                  m_BRCID;;

#ifdef UMC_ENABLE_UMC_SCENE_ANALYZER
    UMC::SceneAnalyzerBase *m_pSceneAnalyzer;
#endif
    //////////////////

    uint32_t                   m_iFrameNumber;

    uint32_t                   m_uiGOPLength;
    uint32_t                   m_uiBFrmLength;

    int16_t*                  m_pSavedMV;

    vm_char                  m_cLastError[VC1_ENC_STR_LEN];

    int32_t                   m_iFrameCount;

    //int32_t                   m_uiNFrames;

    bool                     m_bSequenceHeader;
    uint8_t                    m_uiPictuteQuantIndex;
    bool                     m_bHalfQuant;
    uint8_t                    m_uiRoundControl;                // 0,1;

    StoredFrames*            m_pStoredFrames;
    UMC::MemID               m_StoredFramesID;
    uint8_t*                   m_pStoredFramesBuffer;
    GOP*                     m_pGOP;
    WaitingList*             m_pWaitingList;

    //motion estomation
    UMC::MeBase*             m_pME;
    int32_t                   m_MESearchSpeed;
    uint8_t*                   m_pMEBuffer;
    UMC::MemID               m_MEID;

    //MEMORY ALLOCATOR
    UMC::MemoryAllocator *m_pMemoryAllocator;
    bool            m_bOwnAllocator;
    UMC::MemID            m_SavedMVID;

    bool                  m_bFrameRecoding;
    bool                  m_bMixedMV;
    uint32_t                m_uiOrigFramesUsingFlag;

    Frame*                 m_pPlane;
    Frame*                 m_pForwardMEPlane;
    Frame*                 m_pBackwardMEPlane;
    Frame*                 m_pRaisedPlane;
    Frame*                 m_pForwardPlane;
    Frame*                 m_pBackwardPlane;

    UMC::MeFrame*          m_MeFrame[32];
    MeIndex                m_MeIndex;

    uint8_t                  m_LastQuant;

    bool                   m_bUseMeFeedback;
    bool                   m_bUseUpdateMeFeedback;
    bool                   m_bUseFastMeFeedback;
    bool                   m_bFastUVMC;

    bool                   m_bSelectVLCTables;
    bool                   m_bChangeInterpPixelType;
    bool                   m_bUseTreillisQuantization;
    bool                   m_bIntensityCompensation; 

    pIntensityCompChroma   IntensityCompChroma;
protected:

    static uint8_t    GetRoundControl(ePType pictureType, uint8_t roundControl);
public:

    VC1EncoderSM():
        m_pCodedFrame(0),
        m_pSavedMV(0),
        m_SH(0),
        m_pCurrPicture(0),
        m_bSequenceHeader(false),
        m_uiPictuteQuantIndex(0),
        m_bHalfQuant(false),
        m_iFrameCount(0),
        m_uiGOPLength(0),
        m_uiBFrmLength(0),
        m_pMBs(0),
        m_pBitRateControl(NULL),
        m_pBRCBuffer(NULL),
        m_BRCID((UMC::MemID)-1),
#ifdef UMC_ENABLE_UMC_SCENE_ANALYZER
        m_pSceneAnalyzer(NULL),
#endif
        m_uiRoundControl(0),
        m_pCodedMB(0),
        m_pME(NULL),
        m_MEID((UMC::MemID)-1),
        m_pStoredFrames(0),
        m_pStoredFramesBuffer(NULL),
        m_StoredFramesID((UMC::MemID)-1),
        m_pGOP(0),
        m_pWaitingList(0),
        m_pMemoryAllocator(NULL),
        m_bOwnAllocator(false),
        m_SavedMVID((UMC::MemID)-1),
        m_pMEBuffer(NULL),
        m_MESearchSpeed(66),
        m_bFrameRecoding(false),
        m_bMixedMV(false),
        m_uiOrigFramesUsingFlag(0),
        m_LastQuant(1),
        m_MBsID((UMC::MemID)-1),
        m_MBsBuffer(NULL),
        m_bUseMeFeedback(true),
        m_bUseUpdateMeFeedback(false),
        m_bUseFastMeFeedback(false),
        m_bFastUVMC(true),
        m_bSelectVLCTables(0),
        m_bChangeInterpPixelType(0),
        m_bUseTreillisQuantization(0),
        m_bIntensityCompensation(false)
    {
        IntensityCompChroma = IntensityCompChromaYV12;

        for(uint32_t i = 0; i < 32; i++)
            m_MeFrame[i] = NULL;

        m_MeIndex.MeCurrIndex = 0;
        m_MeIndex.MeRefFIndex = 0;
        m_MeIndex.MeRefBIndex = 0;
    }
    ~VC1EncoderSM()
    {
        Close();
    }

    UMC::Status     Init(UMC::VC1EncoderParams* pParams);
    UMC::Status     Close();
    UMC::Status     Reset();

    void SetMemoryAllocator(UMC::MemoryAllocator *pMemoryAllocator, bool bOwnAllocator);

    UMC::Status     GetInfo(UMC::VC1EncoderParams* pInfo);
    UMC::Status     GetFrame(UMC::MediaData *in, UMC::MediaData *out);

    UMC::Status     WriteFrame(ePType InputPictureType, uint32_t CodedSize);

private:
    UMC::Status     SetMEParams(UMC::MeParams* MEParams, InitPictureParams *pParams, uint8_t doubleQuant, bool Uniform);
    UMC::Status     SetMEParams_I(UMC::MeParams* MEParams);
    UMC::Status     SetMEParams_P(UMC::MeParams* MEParams);
    UMC::Status     SetMEParams_B(UMC::MeParams* MEParams, sFraction* pBFraction);
    UMC::Status     SetMEParams_PMixed(UMC::MeParams* MEParams);


    void        ResetPlanes()
    {
       m_pPlane                 = 0;
       m_pForwardMEPlane        = 0;
       m_pBackwardMEPlane       = 0;

       m_pRaisedPlane                 = 0;
       m_pForwardPlane                = 0;
       m_pBackwardPlane               = 0;
    }
    UMC::Status SetInitPictureParams(InitPictureParams*  pParam, ePType PicType);
    UMC::Status MEPictureParamsRefine(UMC::MeParams* MEParams,InitPictureParams* pInitPicParam);


};

}
#endif
#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
