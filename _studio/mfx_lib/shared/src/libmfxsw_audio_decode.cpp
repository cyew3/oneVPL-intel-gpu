//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include <mfxaudio.h>

#include <mfx_session.h>
#include <mfx_tools.h>
#include <mfx_common.h>

// scheduling and threading stuff
#include <mfx_task.h>

#include <libmfx_core.h>

#if defined (MFX_ENABLE_AAC_AUDIO_DECODE)
#include "mfx_aac_dec_decode.h"
#endif

#if defined (MFX_ENABLE_MP3_AUDIO_DECODE)
#include "mfx_mp3_dec_decode.h"
#endif

mfxStatus MFXAudioUSER_Register(mfxSession session, mfxU32 type,
                                const mfxPlugin *par)
{
    mfxStatus mfxRes = MFX_ERR_UNSUPPORTED;
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);

    session;
    type;
    par;

    return mfxRes;
}

mfxStatus MFXAudioUSER_Unregister(mfxSession session, mfxU32 type)
{
    mfxStatus mfxRes = MFX_ERR_UNSUPPORTED;
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);

    session;
    type;

    return mfxRes;
}

mfxStatus MFXAudioUSER_ProcessFrameAsync(mfxSession session,
                                         const mfxHDL *in, mfxU32 in_num,
                                         const mfxHDL *out, mfxU32 out_num,
                                         mfxSyncPoint *syncp)
{
    mfxStatus mfxRes = MFX_ERR_UNSUPPORTED;
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);

    session;
    in; out; in_num; out_num; syncp;

    return mfxRes;
}

AudioDECODE *CreateAudioDECODESpecificClass(mfxU32 CodecId, AudioCORE *core, mfxSession session)
{
    AudioDECODE *pDECODE = (AudioDECODE *) 0;
    mfxStatus mfxRes = MFX_ERR_MEMORY_ALLOC;

    // touch unreferenced parameter
    session = session;

    // create a codec instance
    switch (CodecId)
    {
#if defined (MFX_ENABLE_AAC_AUDIO_DECODE)
    case MFX_CODEC_AAC:
        pDECODE = new AudioDECODEAAC(core, &mfxRes);
        break;
#endif

#if defined (MFX_ENABLE_MP3_AUDIO_DECODE)
    case MFX_CODEC_MP3:
        pDECODE = new AudioDECODEMP3(core, &mfxRes);
        break;
#endif

    default:
        break;
    }

    // check error(s)
    if (MFX_ERR_NONE != mfxRes)
    {
        delete pDECODE;
        pDECODE = (AudioDECODE *) 0;
    }

    return pDECODE;

} // AudioDECODE *CreateDECODESpecificClass(mfxU32 CodecId, AudioCORE *core)

mfxStatus MFXAudioDECODE_Query(mfxSession session, mfxAudioParam *in, mfxAudioParam *out)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(out, MFX_ERR_NULL_PTR);

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, in);

    mfxStatus mfxRes;

    try
    {
        switch (out->mfx.CodecId)
        {
#ifdef MFX_ENABLE_AAC_AUDIO_DECODE
        case MFX_CODEC_AAC:
            mfxRes = AudioDECODEAAC::Query(session->m_pAudioCORE.get(), in, out);
            break;
#endif

#ifdef MFX_ENABLE_MP3_AUDIO_DECODE
        case MFX_CODEC_MP3:
            mfxRes = AudioDECODEMP3::Query(session->m_pAudioCORE.get(), in, out);
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

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, out);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXAudioDECODE_QueryIOSize(mfxSession session, mfxAudioParam *par, mfxAudioAllocRequest *request)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    MFX_CHECK(request, MFX_ERR_NULL_PTR);

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, par);



    mfxStatus mfxRes;
    try
    {
        switch (par->mfx.CodecId)
        {
#ifdef MFX_ENABLE_AAC_AUDIO_DECODE
        case MFX_CODEC_AAC:
            mfxRes = AudioDECODEAAC::QueryIOSize(session->m_pAudioCORE.get(), par, request);
            break;
#endif

#ifdef MFX_ENABLE_MP3_AUDIO_DECODE
        case MFX_CODEC_MP3:
            mfxRes = AudioDECODEMP3::QueryIOSize(session->m_pAudioCORE.get(), par, request);
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

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, request);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXAudioDECODE_DecodeHeader(mfxSession session, mfxBitstream *bs, mfxAudioParam *par)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(bs, MFX_ERR_NULL_PTR);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, bs);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, par);

    mfxStatus mfxRes;
    try
    {
        switch (par->mfx.CodecId)
        {
#ifdef MFX_ENABLE_AAC_AUDIO_DECODE
        case MFX_CODEC_AAC:
            mfxRes = AudioDECODEAAC::DecodeHeader(session->m_pAudioCORE.get(), bs, par);
            break;
#endif

#ifdef MFX_ENABLE_MP3_AUDIO_DECODE
        case MFX_CODEC_MP3:
            mfxRes = AudioDECODEMP3::DecodeHeader(session->m_pAudioCORE.get(), bs, par);
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
        if (0 == session)
        {
            return MFX_ERR_INVALID_HANDLE;
        }
    }

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXAudioDECODE_Init(mfxSession session, mfxAudioParam *par)
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, par);

    try
    {
        AudioDECODE* tmp = session->m_pAudioDECODE.get();
        // check existence of component
        if (tmp == NULL)
        {
            // create a new instance
            tmp = CreateAudioDECODESpecificClass(par->mfx.CodecId, session->m_pAudioCORE.get(), session);
            session->m_pAudioDECODE.reset(tmp);
        }
        
        if (tmp != NULL)
        {
            mfxRes = tmp->Init(par);
        }
        else
        {
            return MFX_ERR_INVALID_AUDIO_PARAM;
        }

    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;

} // mfxStatus MFXAudioDECODE_Init(mfxSession session, mfxVideoParam *par)

mfxStatus MFXAudioDECODE_Close(mfxSession session)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);

    try
    {
        if (!session)
        {
            throw;
        } else if (!session->m_pAudioDECODE.get())
        {
            return MFX_ERR_NOT_INITIALIZED;
        }
        else
        {
            // wait until all tasks are processed
            session->m_pScheduler->WaitForTaskCompletion(session->m_pDECODE.get());

            mfxRes = session->m_pAudioDECODE->Close();
            // delete the codec's instance
            session->m_pAudioDECODE.reset((AudioDECODE *) 0);
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

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;

} // mfxStatus MFXAudioDECODE_Close(mfxSession session)

mfxStatus MFXAudioDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs ,mfxAudioFrame *aFrame, mfxSyncPoint *syncp)
{
    mfxStatus mfxRes;

#ifdef MFX_TRACE_ENABLE
    MFX_AUTO_LTRACE_WITHID(MFX_TRACE_LEVEL_API, "MFX_DecodeAudioFrameAsync");
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, bs);
#endif

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pAudioDECODE.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(bs, MFX_ERR_MORE_DATA);
    MFX_CHECK(aFrame, MFX_ERR_NULL_PTR);
    MFX_CHECK(syncp, MFX_ERR_NULL_PTR);
   
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
        if (syncp)
        {
            *syncp = NULL;
        }

        memset(&task, 0, sizeof(MFX_TASK));
        mfxRes = session->m_pAudioDECODE->DecodeFrameCheck(bs, aFrame, &task.entryPoint);
        // source data is OK, go forward
        if (task.entryPoint.pRoutine)
        {
            mfxStatus mfxAddRes;

            task.pOwner = session->m_pAudioDECODE.get();
            task.priority = session->m_priority;
            task.threadingPolicy = session->m_pAudioDECODE->GetThreadingPolicy();
            // fill dependencies
            task.pDst[0] = aFrame;

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
        if (MFX_ERR_NONE == mfxRes && syncp)
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
        else if (0 == session->m_pDECODE.get())
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

} // mfxStatus MFXAudioDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs ,mfxBitstream *buffer_out, mfxSyncPoint *syncp)

//
// THE OTHER DECODE FUNCTIONS HAVE IMPLICIT IMPLEMENTATION
//

FUNCTION_AUDIO_RESET_IMPL(DECODE, Reset, (mfxSession session, mfxAudioParam *par), (par))

FUNCTION_AUDIO_IMPL(DECODE, GetAudioParam, (mfxSession session, mfxAudioParam *par), (par))

