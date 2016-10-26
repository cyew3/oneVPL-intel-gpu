//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2011 Intel Corporation. All Rights Reserved.
//

#include "mfx_vc1_enc_defs.h"

#if defined (MFX_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _MFX_VC1_ENC_ENC_H_
#define _MFX_VC1_ENC_ENC_H_

#include "mfxvideo++int.h"
#include "mfxvideo.h"
#include "umc_me.h"
#include "mfx_vc1_enc_common.h"
#include "mfx_vc1_enc_brc.h"
#include "mfx_vc1_enc_ex_param_buf.h"
#include "umc_vc1_enc_picture_adv.h"
#include "umc_vc1_enc_picture_sm.h"

#define N_EX_FRAMES 4

enum VC1FrameStruct
{
    VC1_FIELD_FIRST,
    VC1_FIELD_SECOND,
    VC1_FRAME
};
typedef struct _FrameInfo
{
  mfxU8             m_bReconData;   // 0: source data, 1: reconstructed data
  mfxU8             m_bHasData;     // 0: frame empty, 1: frame has a data
  mfxU8             m_bReference;   // 0: non-reference, 1: reference
  mfxU8             m_bLocked;      // 1 - frame is in use in this function
}FrameInfo;

typedef struct _MEFrameInfo
{
  VC1FrameStruct    m_frameStruct;
  mfxU16            m_MFXFrameOrder;
  FrameInfo         m_frameInfo;
}MEFrameInfo;

typedef struct _ExFrameInfo
{
   mfxU16           m_MFXFrameOrder;
   FrameInfo        firstFieldInfo; // or frameInfo
   FrameInfo        secondFieldInfo;
}ExFrameInfo;


inline mfxI32 GetMEFrameIndex(MEFrameInfo* pInfo, mfxU16 frameOrder ,mfxU32 numFrames, VC1FrameStruct frameStruct)
{
    for (mfxU32 i=0; i<numFrames; i++)
    {
        if (/*pInfo[i].m_frameInfo.m_bHasData != 0 &&*/
            pInfo[i].m_MFXFrameOrder == frameOrder &&
            pInfo[i].m_frameStruct == frameStruct)
        {
            return i;
        }
    }
    return -1;
}
inline mfxI32 GetExFrameIndex(ExFrameInfo* pInfo, mfxU16 frameOrder ,mfxU32 numFrames)
{
    for (mfxU32 i=0; i<numFrames; i++)
    {
        if ((pInfo[i].firstFieldInfo.m_bHasData != 0 || pInfo[i].secondFieldInfo.m_bHasData != 0) &&
             pInfo[i].m_MFXFrameOrder == frameOrder)
        {
            return i;
        }
    }
    return -1;
}
inline void UnLockFrames(ExFrameInfo* pInfo, mfxU32 numFrames)
{
    for (mfxU32 i=0; i<numFrames; i++)
    {
        pInfo[i].firstFieldInfo.m_bLocked  = 0;
        pInfo[i].secondFieldInfo.m_bLocked = 0;
    }
}
inline void UnLockFrames(MEFrameInfo* pInfo, mfxU32 numFrames)
{
    for (mfxU32 i=0; i<numFrames; i++)
    {
        pInfo[i].m_frameInfo.m_bLocked = 0;
    }
}
inline mfxI32 GetFreeIndex(MEFrameInfo* pInfo, mfxU32 numFrames)
{
    mfxI32 oldestReference    = 0x10000;
    mfxI32 oldestNonReference = 0x10000;

    for (mfxU32 i=0; i<numFrames; i++)
    {
        if (!pInfo[i].m_frameInfo.m_bLocked)
        {
            if (!pInfo[i].m_frameInfo.m_bHasData)
                return i;
            if (pInfo[i].m_frameInfo.m_bReference )
            {
                oldestReference = (oldestReference > (mfxI32) pInfo[i].m_MFXFrameOrder)?
                    pInfo[i].m_MFXFrameOrder:oldestReference;
            }
            else
            {
                oldestNonReference = (oldestNonReference > (mfxI32) pInfo[i].m_MFXFrameOrder)?
                    pInfo[i].m_MFXFrameOrder:oldestReference;
            }
        }
    }
    mfxI32 oldest = (oldestNonReference != 0x10000)? oldestNonReference:oldestReference;

    mfxI32 index = -1;
    for (mfxU32 i=0; i<numFrames; i++)
    {
        if (!pInfo[i].m_frameInfo.m_bLocked && (mfxI32) pInfo[i].m_MFXFrameOrder == oldest)
        {
            pInfo[i].m_frameInfo.m_bHasData = false;
            index = i;
        }
    }
    return index;
}
inline mfxI32 GetFreeIndex(ExFrameInfo* pInfo, mfxU32 numFrames)
{
    mfxI32 oldestReference    = 0x10000;
    mfxI32 oldestNonReference = 0x10000;

    for (mfxU32 i=0; i<numFrames; i++)
    {
        if (!pInfo[i].firstFieldInfo.m_bLocked && !pInfo[i].secondFieldInfo.m_bLocked )
        {
            if (pInfo[i].secondFieldInfo.m_bHasData == 0 && pInfo[i].firstFieldInfo.m_bHasData == 0)
                return i;
            if (pInfo[i].secondFieldInfo.m_bReference || pInfo[i].firstFieldInfo.m_bReference)
            {
                oldestReference = (oldestReference > (mfxI32) pInfo[i].m_MFXFrameOrder)?
                    pInfo[i].m_MFXFrameOrder:oldestReference;
            }
            else
            {
                oldestNonReference = (oldestNonReference > (mfxI32) pInfo[i].m_MFXFrameOrder)?
                    pInfo[i].m_MFXFrameOrder:oldestReference;
            }
        }
    }
    mfxI32 oldest = (oldestNonReference != 0x10000)? oldestNonReference:oldestReference;

    mfxI32 index = -1;
    for (mfxU32 i=0; i<numFrames; i++)
    {
        if (!pInfo[i].firstFieldInfo.m_bLocked &&
            !pInfo[i].secondFieldInfo.m_bLocked &&
            (mfxI32) pInfo[i].m_MFXFrameOrder == oldest)
        {
            pInfo[i].firstFieldInfo.m_bHasData = false;
            pInfo[i].secondFieldInfo.m_bHasData = false;

            index = i;
        }
    }
    return index;
}

class MFXVideoEncVc1 : public VideoENC
{
    VideoCORE*                  m_pMFXCore;
    UMC::MeBase*                m_pME;

    //MFXVideoRcVc1*            m_BRC;
    mfxHDL                      m_MEID;
    mfxHDL                      m_MEObjID;

    //Ipp8u*                      m_pMEBuffer;
    Ipp8u                       m_MESpeed;
    bool                        m_UseFB;
    bool                        m_FastFB;

    bool                        m_bIntensityCompensation;
    bool                        m_bChangeInterpolationType;
    bool                        m_bChangeVLCTables;
    bool                        m_bTrellisQuantization;

    bool                        m_bUsePadding;

    /* temporary buffer for ME */

    UMC::MeFrame*               m_MeFrame[VC1_MFX_MAX_FRAMES_FOR_UMC_ME ];
    MEFrameInfo                 m_pMEInfo [VC1_MFX_MAX_FRAMES_FOR_UMC_ME];
    Ipp32u                      m_numMEFrames;

    /* temporary frames with padding   */

    mfxFrameData                m_ExFrameData[N_EX_FRAMES];
    mfxFrameData*               m_ExFrames[N_EX_FRAMES];
    mfxFrameSurface             m_ExFrameSurface;
    ExFrameInfo                 m_ExFrameInfo[N_EX_FRAMES];


    mfxHDL                      m_ExFrameID[N_EX_FRAMES];


    Ipp16u                      m_picture_width;
    Ipp16u                      m_picture_height;

     mfxU8                      m_profile;

     mfxVideoParam              m_VideoParam;
     mfxFrameParam              m_FrameParam;
     mfxSliceParam              m_SliceParam;

     mfxU8                      m_InitFlag;


     UMC_VC1_ENCODER::VC1EncoderSequenceADV*   m_pVC1SequenceAdv;
     UMC_VC1_ENCODER::VC1EncoderSequenceSM*    m_pVC1SequenceSM;
     mfxHDL                                    m_SeqID;

     UMC_VC1_ENCODER::VC1EncoderPictureADV*    m_pVC1PictureAdv;
     UMC_VC1_ENCODER::VC1EncoderPictureSM*     m_pVC1PictureSM;
     mfxHDL                                    m_PicID;

     UMC_VC1_ENCODER::VC1EncoderCodedMB*       m_pVC1EncoderCodedMB;
     mfxHDL                                    m_CodedMBID;

     UMC_VC1_ENCODER::VC1EncoderMBs*           m_pVC1EncoderMBs;
     mfxHDL                                    m_MBID;
     mfxHDL                                    m_MBsID;

     pReplicateBorderChroma                    m_pPadChromaFunc;
     pSetFrame                                 SetFrame;
     pGetChromaShift                           GetChromaShift;
     pCopyFrame                                CopyFrame;
     pCopyField                                CopyField;
     pGetPlane_Prog                            GetPlane_Prog;
     pGetPlane_Field                           GetPlane_Field;
     pPadFrameProgressive                      PadFrameProgressive;
     pPadFrameField                            PadFrameField;

    //IC
    pIntensityCompChroma   IntensityCompChroma;

public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) ;
    static mfxStatus QueryIOSurf (mfxVideoParam *par,mfxFrameAllocRequest *fr) {return MFX_ERR_UNSUPPORTED;}

    MFXVideoEncVc1(VideoCORE *core, mfxStatus *status):VideoENC()
    {
        m_pMFXCore  = core;
        m_pME       = NULL;
        //m_pMEBuffer = NULL;
        m_MEID      = 0;
        m_MEObjID   = 0;
        //m_BRC       = NULL;
        m_MESpeed   = 0;
        //memset(&m_MeIndex,0,sizeof(UMC_VC1_ENCODER::MeIndex));
        //memset( m_MeFrame,0,sizeof(UMC::MeFrame*)*32);
        m_picture_width     = 0;
        m_picture_height    = 0;
        m_numMEFrames       = 0;
        m_profile = MFX_PROFILE_VC1_ADVANCED;
        m_InitFlag = 0;

        m_pVC1SequenceAdv = NULL;
        m_pVC1SequenceSM  = NULL;
        m_SeqID = 0;

        m_pVC1PictureAdv = NULL;
        m_pVC1PictureSM  = NULL;
        m_PicID = 0;

        m_pVC1EncoderCodedMB = NULL;
        m_CodedMBID = 0;

        m_pVC1EncoderMBs = NULL;
        m_MBID  = 0;
        m_MBsID = 0;

        m_bUsePadding = false;
        m_bIntensityCompensation = false;
        m_bChangeInterpolationType = false;
        m_bChangeVLCTables = false;
        m_bTrellisQuantization = false;

        /* temporary frames with padding */
        memset(&m_ExFrameData,    0, sizeof(mfxFrameData)*N_EX_FRAMES);
        memset(&m_ExFrames,       0, sizeof(mfxFrameData*)*N_EX_FRAMES);
        memset(&m_ExFrameSurface, 0, sizeof(mfxFrameSurface));
        memset(&m_ExFrameID,      0, sizeof(mfxHDL)*N_EX_FRAMES);
        memset(&m_ExFrameInfo,    0, sizeof(ExFrameInfo)*N_EX_FRAMES);


        /* temporary buffers for Motion Estimation        */
        memset(m_MeFrame, 0, sizeof (UMC::MeFrame* )*VC1_MFX_MAX_FRAMES_FOR_UMC_ME);
        memset(m_pMEInfo, 0, sizeof (MEFrameInfo)  * VC1_MFX_MAX_FRAMES_FOR_UMC_ME);

        memset(&m_VideoParam, 0, sizeof(mfxVideoParam));
        memset(&m_FrameParam, 0, sizeof(mfxFrameParam));
        memset(&m_SliceParam, 0, sizeof(mfxSliceParam));

        m_pPadChromaFunc   = ReplicateBorderChroma_YV12;
        GetChromaShift     = GetChromaShiftYV12;
        SetFrame           = SetFrameYV12;
        CopyFrame          = copyFrameYV12;
        CopyField          = copyFieldYV12;
        GetPlane_Prog      = GetPlane_ProgYV12;
        GetPlane_Field     = GetPlane_FieldYV12;
        PadFrameProgressive= PadFrameProgressiveYV12;
        PadFrameField      = PadFrameFieldYV12;
        IntensityCompChroma = IntensityCompChromaYV12;

        *status = MFX_ERR_NONE;
    };

    virtual ~MFXVideoEncVc1(void);

    virtual mfxStatus Init(/*VideoBRC *rc, */mfxVideoParam *params);
    virtual mfxStatus Close(void);

    virtual mfxStatus Reset(mfxVideoParam *params);
    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);
    virtual mfxStatus GetSliceParam(mfxSliceParam *par);

    virtual mfxStatus RunFrameVmeENC(mfxFrameCUC *cuc);
    virtual mfxStatus RunFramePredENC(mfxFrameCUC *cuc)    {return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus RunFrameFullENC(mfxFrameCUC *cuc);
    virtual mfxStatus RunFrameFTQ(mfxFrameCUC *cuc)        {return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus GetFrameResid(mfxFrameCUC *cuc)      {return MFX_ERR_UNSUPPORTED;}

    virtual mfxStatus RunSliceVmeENC(mfxFrameCUC *cuc) ;
    virtual mfxStatus RunSlicePredENC(mfxFrameCUC *cuc)    {return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus RunSliceFullENC(mfxFrameCUC *cuc)    {return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus RunSliceFTQ(mfxFrameCUC *cuc)        {return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus GetSliceResid(mfxFrameCUC *cuc)      {return MFX_ERR_UNSUPPORTED;}
protected:
    mfxStatus   PutMEIntoCUC(UMC::MeParams* pMEParams, mfxFrameCUC *cuc, UMC_VC1_ENCODER::ePType UMCtype);

    mfxStatus   SetMEParams(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams,mfxFrameData** refData=0, mfxFrameInfo*   pRefInfo=0,UMC::MeFrame**   pMEFrames=0);
    mfxStatus   SetMEParams_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, bool FieldPicture);
    mfxStatus   SetMEParams_SM(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams);

    mfxStatus   SetMEParams_I_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams,mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames);
    mfxStatus   SetMEParams_P_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames);
    mfxStatus   SetMEParams_PMixed_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames);
    mfxStatus   SetMEParams_B_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames);
    mfxStatus   SetMEParams_I_Field_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames);
    mfxStatus   SetMEParams_B_Field_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams,
        mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames, bool mixed);
    mfxStatus   SetMEParams_P_Field_ADV(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams,
        mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames, bool mixed);

    mfxStatus   SetMEParams_I_SM(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames);
    mfxStatus   SetMEParams_P_SM(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames);
    mfxStatus   SetMEParams_PMixed_SM(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames);
    mfxStatus   SetMEParams_B_SM(mfxFrameCUC *cuc, mfxVideoParam* pPar, UMC::MeParams* MEParams, mfxFrameData** refData, mfxFrameInfo*   pRefInfo,UMC::MeFrame**   pMEFrames);

    mfxStatus   PutMEIntoCUC_P(UMC::MeParams* pMEParams, mfxFrameCUC *cuc);
    mfxStatus   PutMEIntoCUC_B(UMC::MeParams* pMEParams, mfxFrameCUC *cuc);
    mfxStatus   PutMEIntoCUC_PField(UMC::MeParams* pMEParams, mfxFrameCUC *cuc);
    mfxStatus   PutMEIntoCUC_BField(UMC::MeParams* pMEParams, mfxFrameCUC *cuc);

    mfxStatus   PutResidualsIntoCuc_IFrame      (mfxFrameCUC *cuc, mfxI32 firstRow=0, mfxI32 nRows=0);
    mfxStatus   PutResidualsIntoCuc_IField      (mfxFrameCUC *cuc, mfxI32 firstRow=0, mfxI32 nRows=0);
    mfxStatus   PutResidualsIntoCuc_PFrame      (mfxFrameCUC *cuc, mfxI32 firstRow=0, mfxI32 nRows=0);
    mfxStatus   PutResidualsIntoCuc_PMixedFrame (mfxFrameCUC *cuc, mfxI32 firstRow=0, mfxI32 nRows=0);
    mfxStatus   PutResidualsIntoCuc_PField      (mfxFrameCUC *cuc, mfxI32 firstRow=0, mfxI32 nRows=0);
    mfxStatus   PutResidualsIntoCuc_BFrame      (mfxFrameCUC *cuc, mfxI32 firstRow=0, mfxI32 nRows=0);
    mfxStatus   PutResidualsIntoCuc_BField      (mfxFrameCUC *cuc, mfxI32 firstRow=0, mfxI32 nRows=0);

    //mfxStatus   PutResidualsIntoCuc(mfxFrameCUC *cuc, mfxI32 firstRow, mfxI32 nRows);
};

MFXVideoEncVc1* CreateMFXVideoEncVc1(VideoCORE *core, mfxStatus *status);



#endif //_MFX_VC1_ENC_ENC_H_
#endif //MFX_ENABLE_VC1_VIDEO_ENCODER
