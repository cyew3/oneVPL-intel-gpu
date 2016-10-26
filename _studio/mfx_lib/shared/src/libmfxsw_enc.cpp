//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#include <mfxvideo.h>

#include <mfx_session.h>
#include <mfx_common.h>

// sheduling and threading stuff
#include <mfx_task.h>

#include "mfxenc.h"
#include "mfx_enc_ext.h"

#ifdef MFX_VA

#ifdef MFX_ENABLE_H264_VIDEO_ENC_HW
#include "mfx_h264_enc_hw.h"
#endif

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_LA_H264_VIDEO_HW)
#include "mfx_h264_la.h"
#endif

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
#include "mfx_h264_preenc.h"
#endif

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)
#include "mfx_h264_enc.h"
#endif

#ifdef MFX_ENABLE_H265FEI_HW
#include "mfx_h265_enc_cm_plugin.h"
#endif

#else //MFX_VA

#ifdef MFX_ENABLE_VC1_VIDEO_ENC
#include "mfx_vc1_enc_defs.h"
#include "mfx_vc1_enc_enc.h"
#endif

#ifdef MFX_ENABLE_MPEG2_VIDEO_ENC
#include "mfx_mpeg2_enc.h"
#endif

#ifdef MFX_ENABLE_H264_VIDEO_ENC
#include "mfx_h264_enc_enc.h"
#endif


#endif //MFX_VA

VideoENC *CreateENCSpecificClass(mfxVideoParam *par, VideoCORE *pCore)
{
    VideoENC *pENC = (VideoENC *) 0;
    mfxStatus mfxRes = MFX_ERR_MEMORY_ALLOC;
    mfxU32 codecId = par->mfx.CodecId;

    switch (codecId)
    {
#if defined (MFX_ENABLE_H264_VIDEO_ENC) && !defined (MFX_VA) || (defined (MFX_ENABLE_H264_VIDEO_ENC_HW) || defined(MFX_ENABLE_LA_H264_VIDEO_HW) || defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC) || defined(MFX_ENABLE_H264_VIDEO_FEI_ENC))&& defined (MFX_VA)
    case MFX_CODEC_AVC:
#ifdef MFX_VA
#if defined (MFX_ENABLE_H264_VIDEO_ENC_HW)
        pENC = new MFXHWVideoENCH264(pCore, &mfxRes);
#endif
#if defined(MFX_ENABLE_LA_H264_VIDEO_HW)
        if (bEnc_LA(par))
            pENC = (VideoENC*) new VideoENC_LA(pCore, &mfxRes);
#endif
#if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
        if (bEnc_PREENC(par))
            pENC = (VideoENC*) new VideoENC_PREENC(pCore, &mfxRes);
#endif
#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENC) && defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW)
        if (bEnc_ENC(par))
            pENC = (VideoENC*) new VideoENC_ENC(pCore, &mfxRes);
#endif
#else //MFX_VA
        pENC = new MFXVideoEncH264(pCore, &mfxRes);
#endif //MFX_VA
        break;
#endif // MFX_ENABLE_H264_VIDEO_ENC || MFX_ENABLE_H264_VIDEO_ENC_H

#if defined (MFX_VA) && defined (MFX_ENABLE_H265FEI_HW)
    case MFX_CODEC_HEVC:
        pENC = (VideoENC*) new VideoENC_H265FEI(pCore, &mfxRes);
        break;
#endif

#ifdef MFX_ENABLE_VC1_VIDEO_ENC
    case MFX_CODEC_VC1:
        pENC = new MFXVideoEncVc1(pCore, &mfxRes);
        break;
#endif // MFX_ENABLE_VC1_VIDEO_ENC

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENC) && !defined(MFX_VA)
    case MFX_CODEC_MPEG2:
        pENC = new MFXVideoENCMPEG2(pCore, &mfxRes);
        break;
#endif

    case 0: pCore;
    default:
        break;
    }

    // check error(s)
    if (MFX_ERR_NONE != mfxRes)
    {
        delete pENC;
        pENC = (VideoENC *) 0;
    }

    return pENC;

} // VideoENC *CreateENCSpecificClass(mfxU32 codecId, VideoCORE *pCore)

mfxStatus MFXVideoENC_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(out, MFX_ERR_NULL_PTR);

    mfxStatus mfxRes = MFX_ERR_UNSUPPORTED;
    try
    {

#ifdef MFX_ENABLE_USER_ENC
        mfxRes = MFX_ERR_UNSUPPORTED;

        _mfxSession_1_10 * versionedSession = (_mfxSession_1_10 *)(session);
        MFXIPtr<MFXISession_1_10> newSession(versionedSession->QueryInterface(MFXISession_1_10_GUID));

        if (newSession && newSession->GetPreEncPlugin().get())
        {
            mfxRes = newSession->GetPreEncPlugin()->Query(session->m_pCORE.get(), in, out);
        }
        // unsupported reserved to codecid != requested codecid
        if (MFX_ERR_UNSUPPORTED == mfxRes)
#endif
        switch (out->mfx.CodecId)
        {
#ifdef MFX_ENABLE_VC1_VIDEO_ENC
        case MFX_CODEC_VC1:
            mfxRes = MFXVideoEncVc1::Query(in, out);
            break;
#endif

#if (defined (MFX_ENABLE_H264_VIDEO_ENC) && !defined (MFX_VA)) || \
    (defined (MFX_ENABLE_H264_VIDEO_ENC_HW) || defined(MFX_ENABLE_LA_H264_VIDEO_HW) || defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC))&& defined (MFX_VA)
        case MFX_CODEC_AVC:
#ifdef MFX_VA
#if defined (MFX_ENABLE_H264_VIDEO_ENC_HW)
            mfxRes = MFXHWVideoENCH264::Query(in, out);
#endif
#if defined(MFX_ENABLE_LA_H264_VIDEO_HW)
        if (bEnc_LA(in))
            mfxRes = VideoENC_LA::Query(session->m_pCORE.get(), in, out);
#endif

#if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
        if (bEnc_PREENC(out))
            mfxRes = VideoENC_PREENC::Query(session->m_pCORE.get(),in, out);
#endif
#else //MFX_VA
            mfxRes = MFXVideoEncH264::Query(in, out);
#endif //MFX_VA
            break;
#endif // MFX_ENABLE_H264_VIDEO_ENC || MFX_ENABLE_H264_VIDEO_ENC_H

#if defined (MFX_VA) && defined (MFX_ENABLE_H265FEI_HW)
        case MFX_CODEC_HEVC:
            mfxRes = VideoENC_H265FEI::Query(session->m_pCORE.get(), in, out);
            break;
#endif

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENC) && !defined (MFX_VA)
        case MFX_CODEC_MPEG2:
            mfxRes = MFXVideoENCMPEG2::Query(in, out);
            break;
#endif // MFX_ENABLE_MPEG2_VIDEO_ENC && !MFX_VA

        case 0: in;
        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }
    return mfxRes;
}

mfxStatus MFXVideoENC_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    MFX_CHECK(request, MFX_ERR_NULL_PTR);

    mfxStatus mfxRes = MFX_ERR_UNSUPPORTED;
    try
    {
#ifdef MFX_ENABLE_USER_ENC
        mfxRes = MFX_ERR_UNSUPPORTED;
        _mfxSession_1_10 * versionedSession = (_mfxSession_1_10 *)(session);
        MFXIPtr<MFXISession_1_10> newSession(versionedSession->QueryInterface(MFXISession_1_10_GUID));
        if (newSession && newSession->GetPreEncPlugin().get())
        {
            mfxRes = newSession->GetPreEncPlugin()->QueryIOSurf(session->m_pCORE.get(), par, request, 0);
        }
        // unsupported reserved to codecid != requested codecid
        if (MFX_ERR_UNSUPPORTED == mfxRes)
#endif
        switch (par->mfx.CodecId)
        {

#ifdef MFX_ENABLE_VC1_VIDEO_ENC
        case MFX_CODEC_VC1:
            mfxRes = MFXVideoEncVc1::QueryIOSurf(par, request);
            break;
#endif

#if defined (MFX_ENABLE_H264_VIDEO_ENC) && !defined (MFX_VA) || (defined (MFX_ENABLE_H264_VIDEO_ENC_HW) || defined(MFX_ENABLE_LA_H264_VIDEO_HW))&& defined (MFX_VA)
        case MFX_CODEC_AVC:
#ifdef MFX_VA
#if defined (MFX_ENABLE_H264_VIDEO_ENC_HW)
            mfxRes = MFXHWVideoENCH264::QueryIOSurf(par, request);
#endif
#if defined(MFX_ENABLE_LA_H264_VIDEO_HW)
        if (bEnc_LA(par))
            mfxRes = VideoENC_LA::QueryIOSurf(session->m_pCORE.get(),par, request);
#endif
#else //MFX_VA
            mfxRes = MFXVideoEncH264::QueryIOSurf(par, request);
#endif //MFX_VA
            break;
#endif // MFX_ENABLE_H264_VIDEO_ENC || MFX_ENABLE_H264_VIDEO_ENC_H

#if defined (MFX_VA) && defined (MFX_ENABLE_H265FEI_HW)
        case MFX_CODEC_HEVC:
            mfxRes = VideoENC_H265FEI::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENC) && !defined (MFX_VA)
        case MFX_CODEC_MPEG2:
            mfxRes = MFXVideoENCMPEG2::QueryIOSurf(par, request);
            break;
#endif // MFX_ENABLE_MPEG2_VIDEO_ENC && !MFX_VA

        case 0:
        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }
    return mfxRes;
}

mfxStatus MFXVideoENC_Init(mfxSession session, mfxVideoParam *par)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    try
    {
#if !defined (MFX_RT)
        // check existence of component
        if (!session->m_pENC.get())
        {
            // create a new instance
            session->m_bIsHWENCSupport = true;
            session->m_pENC.reset(CreateENCSpecificClass(par, session->m_pCORE.get()));
            MFX_CHECK(session->m_pENC.get(), MFX_ERR_INVALID_VIDEO_PARAM);
        }
#endif

        mfxRes = session->m_pENC->Init(par);

#if !defined (MFX_RT)
        if (MFX_WRN_PARTIAL_ACCELERATION == mfxRes)
        {
            session->m_bIsHWENCSupport = false;
            session->m_pENC.reset(CreateENCSpecificClass(par, session->m_pCORE.get()));
            MFX_CHECK(session->m_pENC.get(), MFX_ERR_NULL_PTR);
            mfxRes = session->m_pENC->Init(par);
        }
        else if (mfxRes >= MFX_ERR_NONE)
            session->m_bIsHWENCSupport = true;
#endif

        // SW fallback if EncodeGUID is absence
        if (MFX_PLATFORM_HARDWARE == session->m_currentPlatform &&
            !session->m_bIsHWENCSupport &&
            MFX_ERR_NONE <= mfxRes)
        {
            mfxRes = MFX_WRN_PARTIAL_ACCELERATION;
        } 
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
        else if (0 == session->m_pENC.get())
        {
            mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else if (0 == par)
        {
            mfxRes = MFX_ERR_NULL_PTR;
        }
    }

    return mfxRes;

} // mfxStatus MFXVideoENC_Init(mfxSession session, mfxVideoParam *par)

mfxStatus MFXVideoENC_Close(mfxSession session)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);

    try
    {
        if (!session->m_pENC.get())
        {
            return MFX_ERR_NOT_INITIALIZED;
        }

        // wait until all tasks are processed
        session->m_pScheduler->WaitForTaskCompletion(session->m_pENC.get());

        mfxRes = session->m_pENC->Close();
        // delete the codec's instance if not plugin
        _mfxSession_1_10 * versionedSession = (_mfxSession_1_10 *)(session);
        MFXIPtr<MFXISession_1_10> newSession(versionedSession->QueryInterface(MFXISession_1_10_GUID));

        if (!newSession || newSession->GetPreEncPlugin().get())
        {
            session->m_pENC.reset((VideoENC *) 0);
        }
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

} // mfxStatus MFXVideoENC_Close(mfxSession session)

static
mfxStatus MFXVideoENCLegacyRoutine(void *pState, void *pParam,
                                   mfxU32 threadNumber, mfxU32 callNumber)
{
    VideoENC *pENC = (VideoENC *) pState;
    VideoBRC *pBRC;
    MFX_THREAD_TASK_PARAMETERS *pTaskParam = (MFX_THREAD_TASK_PARAMETERS *) pParam;
    mfxStatus mfxRes;

    // touch unreferenced parameter(s)
    callNumber = callNumber;

    // check error(s)
    if ((NULL == pState) ||
        (NULL == pParam) ||
        (0 != threadNumber))
    {
        return MFX_ERR_NULL_PTR;
    }

    // get the BRC pointer
    pBRC = (VideoBRC *) pTaskParam->enc.pBRC;

    // call the obsolete method
    mfxRes = pBRC->FrameENCUpdate(pTaskParam->enc.cuc);
    if (MFX_ERR_NONE == mfxRes)
    {
        mfxRes = pENC->RunFrameVmeENC(pTaskParam->enc.cuc);
    }

    return mfxRes;

} // mfxStatus MFXVideoENCLegacyRoutine(void *pState, void *pParam,

static
mfxStatus MFXVideoENCLegacyRoutineExt(void *pState, void *pParam,
                                   mfxU32 threadNumber, mfxU32 callNumber)
{
    VideoENC_Ext * pENC = (VideoENC_Ext  *) pState;
    MFX_THREAD_TASK_PARAMETERS *pTaskParam = (MFX_THREAD_TASK_PARAMETERS *) pParam;

    // touch unreferenced parameter(s)
    callNumber = callNumber;

    // check error(s)
    if ((NULL == pState) ||
        (NULL == pParam) ||
        (0 != threadNumber))
    {
        return MFX_ERR_NULL_PTR;
    }
    return pENC->RunFrameVmeENC(pTaskParam->enc_ext.in, pTaskParam->enc_ext.out);
} // mfxStatus MFXVideoENCLegacyRoutine(void *pState, void *pParam,


mfxStatus MFXVideoENC_RunFrameVmeENCAsync(mfxSession session, mfxFrameCUC *cuc, mfxSyncPoint *syncp)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pENC.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(syncp, MFX_ERR_NULL_PTR);

    try
    {
        mfxSyncPoint syncPoint = NULL;
        MFX_TASK task;

        // unfortunately, we have to check error(s),
        // because several members are not used in the sync part
        if (NULL == session->m_pENC.get())
        {
            return MFX_ERR_NOT_INITIALIZED;
        }

        memset(&task, 0, sizeof(MFX_TASK));
        mfxRes = session->m_pENC->RunFrameVmeENCCheck(cuc, &task.entryPoint);
        // source data is OK, go forward
        if (MFX_ERR_NONE == mfxRes)
        {
            // prepare the absolete kind of task.
            // it is absolete and must be removed.
            if (NULL == task.entryPoint.pRoutine)
            {
                // BEGIN OF OBSOLETE PART
                task.bObsoleteTask = true;
                // fill task info
                task.entryPoint.pRoutine = &MFXVideoENCLegacyRoutine;
                task.entryPoint.pState = session->m_pENC.get();
                task.entryPoint.requiredNumThreads = 1;

                // fill legacy parameters
                task.obsolete_params.enc.cuc = cuc;
                task.obsolete_params.enc.pBRC = session->m_pBRC.get();

            } // END OF OBSOLETE PART

            task.pOwner = session->m_pENC.get();
            task.priority = session->m_priority;
            task.threadingPolicy = session->m_pENC->GetThreadingPolicy();
            // fill dependencies
            task.pSrc[0] = cuc;
            task.pDst[0] = cuc;

            // register input and call the task
            mfxRes = session->m_pScheduler->AddTask(task, &syncPoint);

        }

        // return pointer to synchronization point
        *syncp = syncPoint;
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
        else if (0 == session->m_pENC.get())
        {
            mfxRes = MFX_ERR_NOT_INITIALIZED;
        }
        else if (0 == syncp)
        {
            return MFX_ERR_NULL_PTR;
        }
    }

    return mfxRes;

} // mfxStatus MFXVideoENC_RunFrameVmeENCAsync(mfxSession session, mfxFrameCUC *cuc, mfxSyncPoint *syncp)



enum
{
    MFX_NUM_ENTRY_POINTS = 2
};


mfxStatus  MFXVideoENC_ProcessFrameAsync(mfxSession session, mfxENCInput *in, mfxENCOutput *out, mfxSyncPoint *syncp)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pENC.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(syncp, MFX_ERR_NULL_PTR);

    VideoENC_Ext *pEnc = dynamic_cast<VideoENC_Ext *>(session->m_pENC.get());
    MFX_CHECK(pEnc, MFX_ERR_INVALID_HANDLE);

    try
    {
        mfxSyncPoint syncPoint = NULL;
        MFX_ENTRY_POINT entryPoints[MFX_NUM_ENTRY_POINTS];
        mfxU32 numEntryPoints = MFX_NUM_ENTRY_POINTS;
        memset(&entryPoints, 0, sizeof(entryPoints));

        mfxRes = pEnc->RunFrameVmeENCCheck(in,out,entryPoints,numEntryPoints);

        if ((MFX_ERR_NONE == mfxRes) ||
            (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfxRes) ||
            (MFX_WRN_OUT_OF_RANGE == mfxRes) ||
            (MFX_ERR_MORE_DATA_RUN_TASK == static_cast<int>(mfxRes)) ||
            (MFX_ERR_MORE_BITSTREAM == mfxRes))
        {
            // prepare the absolete kind of task.
            // it is absolete and must be removed.
            if (NULL == entryPoints[0].pRoutine)
            {
                MFX_TASK task;
                memset(&task, 0, sizeof(task));
                // BEGIN OF OBSOLETE PART
                task.bObsoleteTask = true;
                // fill task info
                task.entryPoint.pRoutine = &MFXVideoENCLegacyRoutineExt;
                task.entryPoint.pState = pEnc;
                task.entryPoint.requiredNumThreads = 1;

                // fill legacy parameters
                task.obsolete_params.enc_ext.in = in;
                task.obsolete_params.enc_ext.out = out;

                task.priority = session->m_priority;
                task.threadingPolicy = pEnc->GetThreadingPolicy();
                // fill dependencies
                task.pSrc[0] = in;
                task.pDst[0] = out;
                task.pOwner= pEnc;  
                mfxRes = session->m_pScheduler->AddTask(task, &syncPoint);

            } // END OF OBSOLETE PART
            else if (1 == numEntryPoints)
            {
                MFX_TASK task;

                memset(&task, 0, sizeof(task));
                task.pOwner = pEnc;
                task.entryPoint = entryPoints[0];
                task.priority = session->m_priority;
                task.threadingPolicy = pEnc->GetThreadingPolicy();
                // fill dependencies

                task.pSrc[0] =  out; // for LA plugin
                task.pSrc[1] =  in->InSurface;
                task.pDst[0] =  out? out->ExtParam : 0;

                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));
            }
            else
            {
                MFX_TASK task;

                memset(&task, 0, sizeof(task));
                task.pOwner = pEnc;
                task.entryPoint = entryPoints[0];
                task.priority = session->m_priority;
                task.threadingPolicy = MFX_TASK_THREADING_DEDICATED;
                // fill dependencies
                
                task.pSrc[0] = in->InSurface;
                task.pDst[0] = entryPoints[0].pParam;

                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));

                memset(&task, 0, sizeof(task));
                task.pOwner = pEnc;
                task.entryPoint = entryPoints[1];
                task.priority = session->m_priority;
                task.threadingPolicy = MFX_TASK_THREADING_DEDICATED_WAIT;
                // fill dependencies
                task.pSrc[0] = entryPoints[0].pParam;
                task.pDst[0] = (MFX_ERR_NONE == mfxRes) ? out:0; // sync point for LA plugin
                task.pDst[1] = in->InSurface; 


                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));
            }

            // IT SHOULD BE REMOVED
            if (MFX_ERR_MORE_DATA_RUN_TASK == static_cast<int>(mfxRes))
            {
                mfxRes = MFX_ERR_MORE_DATA;
                syncPoint = NULL;
            }          

        }

        // return pointer to synchronization point
        *syncp = syncPoint;
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
        else if (0 == pEnc)
        {
            mfxRes = MFX_ERR_NOT_INITIALIZED;
        }
        else if (0 == syncp)
        {
            return MFX_ERR_NULL_PTR;
        }
    }

    return mfxRes;

} // mfxStatus MFXVideoENC_RunFrameVmeENCAsync(mfxSession session, mfxFrameCUC *cuc, mfxSyncPoint *syncp)


//
// THE OTHER ENC FUNCTIONS HAVE IMPLICIT IMPLEMENTATION
//

FUNCTION_RESET_IMPL(ENC, Reset, (mfxSession session, mfxVideoParam *par), (par))

FUNCTION_IMPL(ENC, GetVideoParam, (mfxSession session, mfxVideoParam *par), (par))
FUNCTION_IMPL(ENC, GetFrameParam, (mfxSession session, mfxFrameParam *par), (par))
