//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined(MFX_VA_LINUX) && defined(MFX_ENABLE_MFE)

#include "mfx_mfe_adapter.h"
#include <va/va_backend.h>
#include <dlfcn.h>
#include "vm_interlocked.h"
#include <assert.h>
#include <iterator>

#ifndef MFX_CHECK_WITH_ASSERT
#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }
#endif

#define CTX(dpy) (((VADisplayContextP)dpy)->pDriverContext)


MFEVAAPIEncoder::MFEVAAPIEncoder() :
  m_refCounter(1)
, m_vaDisplay(0)
, m_mfe_context(VA_INVALID_ID)
, m_framesToCombine(0)
, m_maxFramesToCombine(0)
, m_framesCollected(0)
, vaCreateMFEContext(NULL)
, vaAddContext(NULL)
, vaReleaseContext(NULL)
, vaMFESubmit(NULL)
{
    vm_cond_set_invalid(&m_mfe_wait);
    vm_mutex_set_invalid(&m_mfe_guard);

    m_contexts.reserve(MAX_FRAMES_TO_COMBINE);
    m_streams.reserve(MAX_FRAMES_TO_COMBINE);
}

MFEVAAPIEncoder::~MFEVAAPIEncoder()
{
    Destroy();
}

void MFEVAAPIEncoder::AddRef()
{
    vm_interlocked_inc32(&m_refCounter);
}

void MFEVAAPIEncoder::Release()
{
    vm_interlocked_dec32(&m_refCounter);

    if (0 == m_refCounter)
        delete this;
}

mfxStatus MFEVAAPIEncoder::Create(mfxExtMultiFrameParam  const & par, VADisplay vaDisplay)
{
    assert(vaDisplay);

    if (VA_INVALID_ID != m_mfe_context)
      return MFX_ERR_NONE;

    m_vaDisplay = vaDisplay;

    if (VM_OK != vm_mutex_init(&m_mfe_guard))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    if (VM_OK != vm_cond_init(&m_mfe_wait))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_framesToCombine = 0;

    m_maxFramesToCombine = par.MaxNumFrames ?
            par.MaxNumFrames : MAX_FRAMES_TO_COMBINE;

    m_streams_pool.clear();
    m_toSubmit.clear();

    VADriverContextP ctx = CTX(vaDisplay);
    void* handle = ctx->handle;

    if (handle)
    {
        vaCreateMFEContext = (vaExtCreateMfeContext)dlsym(handle, VPG_EXT_VA_CREATE_MFECONTEXT);

        vaAddContext = (vaExtAddContext)dlsym(handle, VPG_EXT_VA_ADD_CONTEXT);

        vaReleaseContext = (vaExtReleaseContext)dlsym(handle, VPG_EXT_VA_RELEASE_CONTEXT);

        vaMFESubmit = (vaExtMfeSubmit)dlsym(handle, VPG_EXT_VA_MFE_SUBMIT);

        VAStatus vaSts = vaCreateMFEContext(m_vaDisplay, &m_mfe_context);
        if (VA_STATUS_SUCCESS == vaSts)
            return MFX_ERR_NONE;
        else if (VA_STATUS_ERROR_UNIMPLEMENTED == vaSts)
            return MFX_ERR_UNSUPPORTED;
        else
            return MFX_ERR_DEVICE_FAILED;
    }
    else
    {
        return MFX_ERR_DEVICE_FAILED;
    }
}

mfxStatus MFEVAAPIEncoder::Join(VAContextID ctx)
{
    VAStatus vaSts = vaAddContext(m_vaDisplay, ctx, m_mfe_context);
    mfxStatus sts = MFX_ERR_NONE;
    switch(vaSts)
    {
        //invalid context means we are adding context
        //with entry point or codec conrudicting to already added
        //So it is not supported for single MFE adapter to use HEVC and AVC simultaneosly
        case VA_STATUS_ERROR_INVALID_CONTEXT:
            sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
        //entry point not supported by current driver implementation
        case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        //profile not supported
        case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
            sts = MFX_ERR_UNSUPPORTED;//keep separate for future possible expansion
            break;
        case VA_STATUS_SUCCESS:
            sts = MFX_ERR_NONE;
            break;
        default:
            sts = MFX_ERR_DEVICE_FAILED;
            break;
    }

    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == sts, sts);

    // append the pool with a new item;
    m_streams_pool.push_back(m_stream_ids_s());
    // to deal with the situation when a number of sessions < requested
    if (m_framesToCombine < m_maxFramesToCombine)
        ++m_framesToCombine;

    return MFX_ERR_NONE;
}

mfxStatus MFEVAAPIEncoder::Disjoin(VAContextID ctx)
{
    VAStatus vaSts = vaReleaseContext(m_vaDisplay, ctx, m_mfe_context);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    if (m_framesToCombine > 0)
        --m_framesToCombine;
    return MFX_ERR_NONE;
}

mfxStatus MFEVAAPIEncoder::Destroy()
{
    VAStatus vaSts = vaDestroyContext(m_vaDisplay, VAContextID(m_mfe_context));
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    m_mfe_context = VA_INVALID_ID;
    vm_mutex_destroy(&m_mfe_guard);
    vm_cond_destroy(&m_mfe_wait);
    m_streams_pool.clear();

    vaCreateMFEContext = NULL;
    vaAddContext = NULL;
    vaReleaseContext = NULL;
    vaMFESubmit = NULL;

    return MFX_ERR_NONE;
}

mfxStatus MFEVAAPIEncoder::Submit(VAContextID context, vm_tick timeToWait)
{
    vm_mutex_lock(&m_mfe_guard);

    // take next element from the pool and insert it to the end
    StreamsIter_t cur_stream;
    MFX_CHECK_WITH_ASSERT(!m_streams_pool.empty(), MFX_ERR_MEMORY_ALLOC);
    MFX_CHECK_WITH_ASSERT(m_framesToCombine > 0, MFX_ERR_UNDEFINED_BEHAVIOR);

    cur_stream = m_streams_pool.begin();
    // prepare it
    cur_stream->ctx = context;
    cur_stream->sts = MFX_ERR_NONE;
    m_toSubmit.splice(m_toSubmit.end(), m_streams_pool, m_streams_pool.begin());
    ++m_framesCollected;

    vm_tick start_tick = vm_time_get_tick();
    vm_tick spent_ticks = 0;

    // such a condition with ticks allows to go into loop if at least 1ms
    // is needed to wait
    while (m_framesCollected < m_framesToCombine &&
           !cur_stream->isSubmitted() &&
           timeToWait > spent_ticks){
        // now time is expressed in "vm_ticks", thus to pass ms need to convert
        vm_status res = vm_cond_timed_uwait(&m_mfe_wait, &m_mfe_guard,
                                          (timeToWait - spent_ticks));
        if ((VM_OK == res) || (VM_TIMEOUT == res)) {
            spent_ticks = (vm_time_get_tick() - start_tick);
        } else {
            vm_mutex_unlock(&m_mfe_guard);
            return MFX_ERR_UNKNOWN;
        }
    }
    //TODO: if timer expires || wait_time <=0, to move the current stream up-front
    if (!cur_stream->isSubmitted())
    {
        // To form a linear array of contexts
        for (StreamsIter_t it = m_toSubmit.begin(); it != m_toSubmit.end(); ++it){
            if (!it->isSubmitted()){
                m_contexts.push_back(it->ctx);
                m_streams.push_back(it);
            }
        }

        if (m_framesCollected < m_contexts.size() || m_contexts.empty())
        {
            for (std::vector<StreamsIter_t>::iterator it = m_streams.begin();
                 it != m_streams.end(); ++it)
            {
                (*it)->ctx = VA_INVALID_ID;
                (*it)->sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
            // if cur_stream is not in m_streams (somehow)
            cur_stream->ctx = VA_INVALID_ID;
            cur_stream->sts = MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        else
        {
            VAStatus vaSts = vaMFESubmit(m_vaDisplay, m_mfe_context,
                                         &m_contexts[0], m_contexts.size());
            // Proper error handling and passing to correct encoder
            // TBD with VPG and implementation
            mfxStatus tmp_res = VA_STATUS_SUCCESS == vaSts ? MFX_ERR_NONE : MFX_ERR_DEVICE_FAILED;
            for (std::vector<StreamsIter_t>::iterator it = m_streams.begin();
                 it != m_streams.end(); ++it)
            {
                (*it)->ctx = VA_INVALID_ID;
                (*it)->sts = tmp_res;
            }
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            m_framesCollected -= m_contexts.size();
        }
        // Broadcast is done before unlock for this case to simplify the code avoiding extra ifs
        vm_cond_broadcast(&m_mfe_wait);
        m_contexts.clear();
        m_streams.clear();
    }

    // This frame can be already submitted. in this case
    // we have to return strm back to the pool, release mutex and exit
    mfxStatus res = cur_stream->sts;
    m_streams_pool.splice(m_streams_pool.end(), m_toSubmit, cur_stream);
    vm_mutex_unlock(&m_mfe_guard);
    return res;
}

#endif //MFX_VA_LINUX && MFX_ENABLE_MFE
