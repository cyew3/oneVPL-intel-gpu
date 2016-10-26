//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2009 Intel Corporation. All Rights Reserved.
//

#include "mfx_vc1_enc_defs.h"

#if defined (MFX_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _MFX_VC1_ENC_PAK_ADV_H_
#define _MFX_VC1_ENC_PAK_ADV_H_

#include "mfxvideo++int.h"
#include "mfxvideo.h"
#include "umc_vc1_enc_picture_adv.h"
#include "mfx_vc1_enc_common.h"
#include "mfx_vc1_enc_brc.h"

class MFXVideoPakVc1ADV : public VideoPAK
{
private:

    UMC_VC1_ENCODER::VC1EncoderPictureADV*    m_pVC1EncoderPicture;
    mfxHDL                                    m_PicID;

    UMC_VC1_ENCODER::VC1EncoderSequenceADV*   m_pVC1EncoderSequence;
    mfxHDL                                    m_SeqID;

    UMC_VC1_ENCODER::VC1EncoderCodedMB*       m_pVC1EncoderCodedMB;
    mfxHDL                                    m_CodedMBID;

    UMC_VC1_ENCODER::VC1EncoderMBs*           m_pVC1EncoderMBs;
    mfxHDL                                    m_MBID;
    mfxHDL                                    m_MBsID;

    mfxU8*                                    m_UserDataBuffer;
    mfxHDL                                    m_UDBufferID;
    mfxU32                                    m_UserDataSize;
    mfxU32                                    m_UserDataBufferSize;

    VideoCORE*                                m_pCore;

    bool                                      m_InitFlag;
    mfxVideoParam                             m_mfxVideoParam;
    mfxFrameParam                             m_mfxFrameParam;
    mfxSliceParam                             m_mfxSliceParam;

    UMC::MeBase*                              m_pME;
    UMC::MeFrame*                             m_pMeFrame;

    mfxHDL                                    m_MEID;
    bool                                      m_bUsePadding;
    bool                                      m_bUsedTrellisQuantization;

    pReplicateBorderChroma                    m_pPadChromaFunc;
    pSetFrame                                 SetFrame;
    pGetChromaShift                           GetChromaShift;
    pCopyFrame                                CopyFrame;
    pCopyField                                CopyField;
    pGetPlane_Prog                            GetPlane_Prog;
    pGetPlane_Field                           GetPlane_Field;
    pPadFrameProgressive                      PadFrameProgressive;
    pPadFrameField                            PadFrameField;



public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf (mfxVideoParam *par,mfxFrameAllocRequest *fr) {return MFX_ERR_UNSUPPORTED;}

    MFXVideoPakVc1ADV(VideoCORE *core, mfxStatus *status):VideoPAK()
    {
        m_pVC1EncoderPicture  = 0;
        m_pVC1EncoderSequence = 0;
        m_pVC1EncoderCodedMB  = 0;
        m_pVC1EncoderMBs      = 0;
        m_pCore               = core;
        m_MBsID               = 0;
        *status               = MFX_ERR_NONE;
        m_PicID               = 0;
        m_SeqID               = 0;
        m_CodedMBID           = 0;
        m_MBID                = 0;
        m_pME                 = 0;
        m_pMeFrame            = 0;
        m_MEID                = 0;

        m_InitFlag            = false;
        m_bUsePadding         = false;
        m_bUsedTrellisQuantization = false;
        m_UserDataBuffer      = 0;
        m_UDBufferID          = 0;
        m_UserDataSize        = 0;
        m_UserDataBufferSize  = 0;

        memset(&m_mfxVideoParam, 0, sizeof(mfxVideoParam));
        memset(&m_mfxFrameParam, 0, sizeof(mfxFrameParam));
        memset(&m_mfxSliceParam, 0, sizeof(mfxSliceParam));

        m_pPadChromaFunc = ReplicateBorderChroma_YV12;
        GetChromaShift   = GetChromaShiftYV12;
        SetFrame         = SetFrameYV12;
        CopyFrame        = copyFrameYV12;
        CopyField        = copyFieldYV12;
        GetPlane_Prog    = GetPlane_ProgYV12;
        GetPlane_Field   = GetPlane_FieldYV12;
        PadFrameProgressive = PadFrameProgressiveYV12;
        PadFrameField    = PadFrameFieldYV12;

    }

    virtual ~MFXVideoPakVc1ADV(void) {}

    virtual mfxStatus Init(/*VideoBRC *rc,*/ mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);
    virtual mfxStatus GetSliceParam(mfxSliceParam *par);

    virtual mfxStatus RunSeqHeader(mfxFrameCUC *cuc);
    virtual mfxStatus RunGopHeader(mfxFrameCUC *cuc);
    virtual mfxStatus RunPicHeader(mfxFrameCUC *cuc);
    virtual mfxStatus RunSliceHeader(mfxFrameCUC *cuc);

    virtual mfxStatus InsertUserData(mfxU8 *ud,mfxU32 len,mfxU64 ts,mfxBitstream *bs);
    virtual mfxStatus InsertBytes(mfxU8 *data, mfxU32 len, mfxBitstream *bs);
    virtual mfxStatus AlignBytes(mfxU8 pattern, mfxU32 len, mfxBitstream *bs);

    virtual mfxStatus RunSlicePAK(mfxFrameCUC *cuc);
    virtual mfxStatus RunSliceBSP(mfxFrameCUC *cuc);
    virtual mfxStatus RunFramePAK(mfxFrameCUC *cuc);
    virtual mfxStatus RunFrameBSP(mfxFrameCUC *cuc);

    mfxStatus SetMEParam_P( mfxFrameCUC *cuc, UMC::MeParams* pMEParams, mfxI32 firstRow = 0, mfxI32 nRows = 0);
    mfxStatus SetMEParam_P_Mixed( mfxFrameCUC *cuc, UMC::MeParams* pMEParams, mfxI32 firstRow = 0, mfxI32 nRows = 0);
    mfxStatus SetMEParam_B( mfxFrameCUC *cuc, UMC::MeParams* pMEParams, mfxI32 firstRow = 0, mfxI32 nRows = 0);
    mfxStatus SetMEParam_P_Field( mfxFrameCUC *cuc, UMC::MeParams* pMEParams, bool mixed, mfxI32 firstRow = 0, mfxI32 nRows = 0);
    mfxStatus SetMEParam_B_Field( mfxFrameCUC *cuc, UMC::MeParams* pMEParams, bool mixed, mfxI32 firstRow = 0, mfxI32 nRows = 0);

    mfxStatus GetResiduals_IFrame( mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB, mfxI32 firstRow = 0, mfxI32 nRows = 0);
    mfxStatus GetResiduals_IField( mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB, mfxI32 firstRow = 0, mfxI32 nRows = 0);
    mfxStatus GetResiduals_PFrame( mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB, mfxI32 firstRow = 0, mfxI32 nRows = 0);
    mfxStatus GetResiduals_PMixedFrame( mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB, mfxI32 firstRow = 0, mfxI32 nRows = 0);
    mfxStatus GetResiduals_PField( mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB, mfxI32 firstRow = 0, mfxI32 nRows = 0);
    mfxStatus GetResiduals_BFrame( mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB, mfxI32 firstRow = 0, mfxI32 nRows = 0);
    mfxStatus GetResiduals_BField( mfxFrameCUC *cuc, UMC_VC1_ENCODER::VC1EncoderCodedMB* pVC1EncoderCodedMB, mfxI32 firstRow = 0, mfxI32 nRows = 0);
};

#endif //_MFX_VC1_ENC_PAK_ADV_H_
#endif //MFX_ENABLE_VC1_VIDEO_ENCODER
