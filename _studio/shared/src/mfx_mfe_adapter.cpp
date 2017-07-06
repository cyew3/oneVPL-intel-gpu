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
#include "vm_interlocked.h"
#include <assert.h>

#ifndef MFX_CHECK_WITH_ASSERT
#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }
#endif

MFEVAAPIEncoder::MFEVAAPIEncoder() :
  m_refCounter(1)
, m_vaDisplay(0)
, m_mfe_context(VA_INVALID_ID)
, m_framesToCombine(1)
, m_framesCollected(0)
, m_mfe_vmtick_msec_frequency(vm_time_get_frequency()/1000)
{
    m_streams.resize(64);
    vm_cond_set_invalid(&m_mfe_wait);
    vm_mutex_set_invalid(&m_mfe_guard);
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

mfxStatus MFEVAAPIEncoder::Create(mfxVideoParam const & par, VADisplay vaDisplay)
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

    m_framesToCombine = par.mfx.m_numPipelineStreams ? par.mfx.m_numPipelineStreams : 2;

    for (std::vector<m_stream_ids_s>::iterator it = m_streams.begin(); it != m_streams.end(); it++)
    {
        it->sts = MFX_ERR_NONE;
        it->ctx = VA_INVALID_ID;
        it->isSubmitted = 0;
    }

    VAStatus vaSts = vaCreateMFEContext(m_vaDisplay, &m_mfe_context);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    return MFX_ERR_NONE;
}

mfxStatus MFEVAAPIEncoder::Join(VAContextID ctx)
{
    VAStatus vaSts = vaAddContext(m_vaDisplay, ctx, m_mfe_context);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    return MFX_ERR_NONE;
}

mfxStatus MFEVAAPIEncoder::Disjoin(VAContextID ctx)
{
    VAStatus vaSts = vaReleaseContext(m_vaDisplay, ctx, m_mfe_context);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    return MFX_ERR_NONE;
}

mfxStatus MFEVAAPIEncoder::Destroy()
{
    VAStatus vaSts = vaDestroyContext(m_vaDisplay, m_mfe_context);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    m_mfe_context = VA_INVALID_ID;
    vm_mutex_destroy(&m_mfe_guard);
    vm_cond_destroy(&m_mfe_wait);
    return MFX_ERR_NONE;
}

mfxStatus MFEVAAPIEncoder::Submit(VAContextID context, mfxU32 stream_id, mfxU32 timeToWait)
{
    vm_mutex_lock(&m_mfe_guard);

    std::vector<m_stream_ids_t>::iterator strm = m_streams.begin() + stream_id;

    strm->ctx = context;
    strm->isSubmitted = 0;

    m_framesCollected++;


    vm_tick start_tick = vm_time_get_tick(), end_tick;
    mfxU32 spent_ms, wait_time = timeToWait;

    while (m_framesCollected < m_framesToCombine && 0 == strm->isSubmitted &&  wait_time > 0)
    {
        vm_status res = vm_cond_timedwait(&m_mfe_wait, &m_mfe_guard, wait_time);
        if ((VM_OK == res) || (VM_TIMEOUT == res)) {
            end_tick = vm_time_get_tick();

            spent_ms = (end_tick - start_tick)/m_mfe_vmtick_msec_frequency;
            if (spent_ms >= wait_time)
            {
                break;
            }
            wait_time -= spent_ms;
            start_tick = end_tick;
        } else {
            vm_mutex_unlock(&m_mfe_guard);

            return MFX_ERR_UNKNOWN;
        }
    }

    if (strm->isSubmitted)
    {
        vm_mutex_unlock(&m_mfe_guard);
        return MFX_ERR_NONE;
    }
    else if(strm->sts != MFX_ERR_NONE)
    {
        vm_mutex_unlock(&m_mfe_guard);
        return strm->sts;
    }

    std::vector<m_stream_ids_t*> toSubmit;
    std::vector<VAContextID> contexts;

    if (VA_INVALID_ID != m_streams[stream_id].ctx)
    {
        toSubmit.push_back(&m_streams[stream_id]);
        contexts.push_back(m_streams[stream_id].ctx);
    }

    for (mfxU32 idx = 0; idx < m_streams.size() && toSubmit.size() < (mfxU32)m_framesToCombine; idx++)
    {
        if (idx == stream_id)
            continue;

        if (VA_INVALID_ID != m_streams[idx].ctx)
        {
            toSubmit.push_back(&m_streams[idx]);
            contexts.push_back(m_streams[idx].ctx);
        }
    }

    VAStatus vaSts = vaMFESubmit(m_vaDisplay, m_mfe_context, &contexts[0], contexts.size());
    //proper error handling and passing to correct encoder TBD with VPG and implementation
    if(VA_STATUS_SUCCESS == vaSts)
    {
        for (std::vector<m_stream_ids_s*>::iterator it = toSubmit.begin(); it != toSubmit.end(); it++)
        {
            (*it)->isSubmitted = 1;
            (*it)->ctx = VA_INVALID_ID;
            (*it)->sts = MFX_ERR_NONE;
        }
    }
    else
    {
        for (std::vector<m_stream_ids_s*>::iterator it = toSubmit.begin(); it != toSubmit.end(); it++)
        {
            (*it)->isSubmitted = 0;
            (*it)->ctx = VA_INVALID_ID;
            (*it)->sts = MFX_ERR_DEVICE_FAILED;
        }
    }

    m_framesCollected -= toSubmit.size();
    assert(m_framesCollected >= 0);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vm_cond_broadcast(&m_mfe_wait);
    vm_mutex_unlock(&m_mfe_guard);

    return MFX_ERR_NONE;
}

#endif //MFX_VA_LINUX && MFX_ENABLE_MFE
