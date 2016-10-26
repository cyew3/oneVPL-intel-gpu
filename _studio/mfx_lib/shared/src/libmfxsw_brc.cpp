//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include <mfxvideo.h>

#include <mfx_session.h>
#include <mfx_common.h>

#ifdef MFX_ENABLE_VC1_VIDEO_BRC
#include "mfx_vc1_enc_brc.h"
#endif

#ifdef MFX_ENABLE_MPEG2_VIDEO_BRC
#include "mfx_mpeg2_enc_brc.h"
#endif

#ifdef MFX_ENABLE_H264_VIDEO_BRC
#include "mfx_h264_enc_brc.h"
#endif

VideoBRC *CreateBRCSpecificClass(mfxU32 codecId, VideoCORE *pCore)
{
    VideoBRC *pBRC = (VideoBRC *) 0;
    mfxStatus mfxRes = MFX_ERR_MEMORY_ALLOC;

    switch (codecId)
    {
#ifdef MFX_ENABLE_VC1_VIDEO_BRC
    case MFX_CODEC_VC1:
        pBRC = new MFXVideoRcVc1(pCore, &mfxRes);
        break;
#endif

#ifdef MFX_ENABLE_MPEG2_VIDEO_BRC
    case MFX_CODEC_MPEG2:
        pBRC = new MFXVideoRcMpeg2(pCore, &mfxRes);
        break;
#endif

#ifdef MFX_ENABLE_H264_VIDEO_BRC
    case MFX_CODEC_AVC:
        pBRC = new MFXVideoRcH264(pCore, &mfxRes);
        break;
#endif

    default:
        break;
    }

    // check error(s)
    if (MFX_ERR_NONE != mfxRes)
    {
        delete pBRC;
        pBRC = (VideoBRC *) 0;
    }

    return pBRC;

} // VideoBRC *CreateBRCSpecificClass(mfxU32 codecId, VideoCORE *pCore)

mfxStatus MFXVideoBRC_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(out, MFX_ERR_NULL_PTR);

    mfxStatus mfxRes;
    try
    {
        switch (out->mfx.CodecId)
        {
#ifdef MFX_ENABLE_VC1_VIDEO_BRC
        case MFX_CODEC_VC1:
            mfxRes = MFXVideoRcVc1::Query(in, out);
            break;
#endif

#ifdef MFX_ENABLE_H264_VIDEO_BRC
        case MFX_CODEC_AVC:
            mfxRes = MFXVideoRcH264::Query(in, out);
            break;
#endif

#ifdef MFX_ENABLE_MPEG2_VIDEO_BRC
        case MFX_CODEC_MPEG2:
            mfxRes = MFXVideoRcMpeg2::Query(in, out);
            break;
#endif

        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }
    return mfxRes;
}

mfxStatus MFXVideoBRC_Init(mfxSession session, mfxVideoParam *par)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);

    try
    {
        // close the existing BRC,
        // if it is initialized.
        if (session->m_pBRC.get())
        {
            MFXVideoBRC_Close(session);
        }

        // create a new instance
        session->m_pBRC.reset(CreateBRCSpecificClass(par->mfx.CodecId, session->m_pCORE.get()));
        MFX_CHECK(session->m_pBRC.get(), MFX_ERR_INVALID_VIDEO_PARAM);
        mfxRes = session->m_pBRC->Init(par);
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pBRC.get())
        {
            mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else if (0 == par)
        {
            mfxRes = MFX_ERR_NULL_PTR;
        }
    }

    return mfxRes;

} // mfxStatus MFXVideoBRC_Init(mfxSession session, mfxVideoParam *par)

mfxStatus MFXVideoBRC_Close(mfxSession session)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);

    try
    {
        if (!session->m_pBRC.get())
        {
            return MFX_ERR_NOT_INITIALIZED;
        }

        mfxRes = session->m_pBRC->Close();
        // delete the codec's instance
        session->m_pBRC.reset((VideoBRC *) 0);

    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
    }

    return mfxRes;

} // mfxStatus MFXVideoBRC_Close(mfxSession session)

//
// THE OTHER BRC FUNCTIONS HAVE IMPLICIT IMPLEMENTATION
//

FUNCTION_IMPL(BRC, Reset, (mfxSession session, mfxVideoParam *par), (par))
FUNCTION_IMPL(BRC, FrameENCUpdate, (mfxSession session, mfxFrameCUC *cuc), (cuc))
FUNCTION_IMPL(BRC, FramePAKRefine, (mfxSession session, mfxFrameCUC *cuc), (cuc))
FUNCTION_IMPL(BRC, FramePAKRecord, (mfxSession session, mfxFrameCUC *cuc), (cuc))
