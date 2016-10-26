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

#ifndef _MFX_VC1_ENC_ENCODE_H_
#define _MFX_VC1_ENC_ENCODE_H_

#include "mfxvideo++int.h"
#include "mfxvideo.h"
#include "mfx_vc1_enc_common.h"
#include "mfx_memory_allocator.h"
#include "mfx_vc1_enc_brc.h"
#include "umc_defs.h"
#include "umc_vc1_video_encoder.h"
class MFXVideoENCODEVC1 : public VideoENCODE
{
public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf (mfxVideoParam *par,mfxFrameAllocRequest *fr) {return MFX_ERR_UNSUPPORTED;}

    MFXVideoENCODEVC1(VideoCORE *core, mfxStatus *status) ;
    virtual ~MFXVideoENCODEVC1(void);

    virtual mfxStatus Init(/*VideoBRC *rc, */mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    //virtual mfxStatus Query(mfxU8 *chklist, mfxU32 chksz);
    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);
    virtual mfxStatus GetSliceParam(mfxSliceParam *par);

    virtual mfxStatus EncodeFrame(mfxFrameSurface *surface, mfxBitstream *bs);
    virtual mfxStatus InsertUserData(mfxU8 *ud, mfxU32 len, mfxU64 ts);

    virtual mfxStatus GetEncodeStat   (mfxEncodeStat *) {return MFX_ERR_UNSUPPORTED;}
    virtual mfxStatus EncodeFrameCheck(mfxFrameSurface *surface)
    {
        if (surface && surface->Info.NumFrameData > 0)
        {
            surface->Data[0]->Locked ++;
        }
        return MFX_ERR_NONE;
    }


protected:
    UMC::VC1VideoEncoder *pEncoder;
    MFXVideoRcVc1 *m_pRC;

    mfxVideoParam m_mfxVideoParam;

    mfxHDL enco;
    VideoCORE* m_pMFXCore;

    mfx_UMC_MemAllocator* pUMC_MA;
    mfxHDL       m_UMC_MAID;
    Ipp8u*       m_pUMCMABuffer;

    mfxHDL       m_VideoEncoderID;
    Ipp8u*       m_pVideoEncoderBuffer;

    mfxU8*       m_UserDataBuffer;
    mfxHDL       m_UDBufferID;
    mfxU32       m_UserDataSize;
    mfxU32       m_UserDataBufferSize;

    Ipp32u       m_UMCLastFrameType;

    pGetChromaShift GetChromaShift;

};

#endif //_MFX_VC1_ENC_ENCODE_H_
#endif
