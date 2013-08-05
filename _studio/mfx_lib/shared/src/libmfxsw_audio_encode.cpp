/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

File Name: libmfxsw_audio_encode.cpp

\* ****************************************************************************** */

#include <mfxaudio.h>

#include <mfx_session.h>
#include <mfx_tools.h>
#include <mfx_common.h>

// scheduling and threading stuff
#include <mfx_task.h>

#include <libmfx_core.h>

#if defined (MFX_ENABLE_AAC_AUDIO_ENCODE)
#include "mfx_aac_encode.h"
#endif


AudioENCODE *CreateAudioENCODESpecificClass(mfxU32 CodecId, AudioCORE *core, mfxSession session)
{
    AudioENCODE *pENCODE = (AudioENCODE *) 0;
    mfxStatus mfxRes = MFX_ERR_MEMORY_ALLOC;

    // touch unreferenced parameter
    session = session;
    // create a codec instance
    switch (CodecId)
    {
#if defined(MFX_ENABLE_AAC_AUDIO_ENCODE)
    case MFX_CODEC_AAC:
        pENCODE = new AudioENCODEAAC(core, &mfxRes);
        break;
#endif // MFX_ENABLE_AAC_AUDIO_ENCODE

    default:
        break;
    }

    // check error(s)
    if (MFX_ERR_NONE != mfxRes)
    {
        delete pENCODE;
        pENCODE = (AudioENCODE *) 0;
    }

    return pENCODE;

} // AudioENCODE *CreateENCODESpecificClass(mfxU32 CodecId, AudioCORE *core, mfxSession session)

mfxStatus MFXAudioENCODE_Query(mfxSession session, mfxAudioParam *in, mfxAudioParam *out)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(out, MFX_ERR_NULL_PTR);

    mfxStatus mfxRes;
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, in);

    try
    {
        switch (out->mfx.CodecId)
        {
#ifdef MFX_ENABLE_AAC_AUDIO_ENCODE
        case MFX_CODEC_AAC:
            mfxRes = AudioENCODEAAC::Query(session->m_pAudioCORE.get(), in, out);
            break;
#endif

        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }
    
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, out);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXAudioENCODE_QueryIOSize(mfxSession session, mfxAudioParam *par, mfxAudioAllocRequest *request)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    MFX_CHECK(request, MFX_ERR_NULL_PTR);

    mfxStatus mfxRes;
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, par);

    try
    {
        switch (par->mfx.CodecId)
        {
#ifdef MFX_ENABLE_AAC_AUDIO_ENCODE
        case MFX_CODEC_AAC:
            mfxRes = AudioENCODEAAC::QueryIOSize(session->m_pAudioCORE.get(), par, request);
            break;
#endif // MFX_ENABLE_AAC_AUDIO_ENCODE



        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }



    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, request);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXAudioENCODE_Init(mfxSession session, mfxAudioParam *par)
{
    mfxStatus mfxRes;
    MFX_CHECK(par, MFX_ERR_NULL_PTR);

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, par);

    try
    {
        // check existence of component
        if (!session->m_pAudioENCODE.get())
        {
            // create a new instance
            session->m_bIsHWENCSupport = true;
            session->m_pAudioENCODE.reset(CreateAudioENCODESpecificClass(par->mfx.CodecId, session->m_pAudioCORE.get(), session));
        }

        if (0 == session->m_pAudioENCODE.get()) {
            return MFX_ERR_INVALID_AUDIO_PARAM;
        }

        mfxRes = session->m_pAudioENCODE->Init(par);

    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        } else if (0 == session->m_pAudioENCODE.get())
        {
            mfxRes = MFX_ERR_INVALID_AUDIO_PARAM;
        }

        if (0 == par)
        {
            mfxRes = MFX_ERR_NULL_PTR;
        }
    }

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;

} // mfxStatus MFXVideoENCODE_Init(mfxSession session, mfxVideoParam *par)

mfxStatus MFXAudioENCODE_Close(mfxSession session)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);

    try
    {
        if (!session->m_pAudioENCODE.get())
        {
            return MFX_ERR_NOT_INITIALIZED;
        }

        // wait until all tasks are processed
        session->m_pScheduler->WaitForTaskCompletion(session->m_pAudioENCODE.get());

        mfxRes = session->m_pAudioENCODE->Close();
        // delete the codec's instance
        session->m_pAudioENCODE.reset((AudioENCODE *) 0);
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

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;

} // mfxStatus MFXAudioENCODE_Close(mfxSession session)


mfxStatus MFXAudioENCODE_EncodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxBitstream *buffer_out, mfxSyncPoint *syncp)
{
    mfxStatus mfxRes;

#ifdef MFX_TRACE_ENABLE
    MFX_AUTO_LTRACE_WITHID(MFX_TRACE_LEVEL_API, "MFX_EncodeAudioFrameAsync");
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, bs);
#endif

    MFX_CHECK(bs, MFX_ERR_NULL_PTR);
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pAudioENCODE.get(), MFX_ERR_NOT_INITIALIZED);

    try
    {
        mfxSyncPoint syncPoint = NULL;
        MFX_TASK task;

        // Wait for the bit stream
        mfxRes = session->m_pScheduler->WaitForDependencyResolved(bs);
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }

        // reset the sync point
        *syncp = NULL;

        memset(&task, 0, sizeof(MFX_TASK));
        mfxRes = session->m_pAudioENCODE->EncodeFrameCheck(bs, buffer_out, &task.entryPoint);
        // source data is OK, go forward
        if (task.entryPoint.pRoutine)
        {
            mfxStatus mfxAddRes;

            task.pOwner = session->m_pAudioENCODE.get();
            task.priority = session->m_priority;
            task.threadingPolicy = session->m_pAudioENCODE->GetThreadingPolicy();
            // fill dependencies
            task.pDst[0] = buffer_out;

#ifdef MFX_TRACE_ENABLE
            task.nParentId = MFX_AUTO_TRACE_GETID();
            task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_DECODE;
#endif

            // register input and call the task
            mfxAddRes = session->m_pScheduler->AddTask(task, &syncPoint);
            if (MFX_ERR_NONE != mfxAddRes)
            {
                return mfxAddRes;
            }
        }

        // return pointer to synchronization point
        if (MFX_ERR_NONE == mfxRes)
        {
            *syncp = syncPoint;
        }
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            return MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pAudioENCODE.get())
        {
            return MFX_ERR_NOT_INITIALIZED;
        }
        else if (0 == syncp)
        {
            return MFX_ERR_NULL_PTR;
        }
    }

    if (mfxRes == MFX_ERR_NONE)
    {
        if (syncp)
        {
            MFX_LTRACE_P(MFX_TRACE_LEVEL_API, *syncp);
        }
    }
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);

    return mfxRes;

} // mfxStatus MFXAudioENCODE_EncodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxBitstream *buffer_out, mfxSyncPoint *syncp)

//
// THE OTHER ENCODE FUNCTIONS HAVE IMPLICIT IMPLEMENTATION
//

FUNCTION_AUDIO_RESET_IMPL(ENCODE, Reset, (mfxSession session, mfxAudioParam *par), (par))

FUNCTION_AUDIO_IMPL(ENCODE, GetAudioParam, (mfxSession session, mfxAudioParam *par), (par))

