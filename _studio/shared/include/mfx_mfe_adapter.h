//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2017 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_MFE_ADAPTER_
#define _MFX_MFE_ADAPTER_

#if defined(MFX_VA_LINUX) && defined(MFX_ENABLE_MFE)
#include <va/va.h>
#include <vector>
#include "vm_mutex.h"
#include "vm_cond.h"
#include "vm_time.h"
#include <mfxstructures.h>

class MFEVAAPIEncoder
{
    typedef struct m_stream_ids_s{
        volatile mfxU32   isSubmitted;
        VAContextID       ctx;
        mfxStatus         sts;
    } m_stream_ids_t;

public:
    MFEVAAPIEncoder();

    virtual
        ~MFEVAAPIEncoder();
    mfxStatus Create(mfxVideoParam const & par, VADisplay vaDisplay);

    mfxStatus Join(VAContextID ctx);
    mfxStatus Disjoin(VAContextID ctx);
    mfxStatus Destroy();
    mfxStatus Submit(VAContextID context, mfxU32 stream_id, mfxU32 timeToWait);

    virtual void AddRef();
    virtual void Release();

private:
    mfxU32      m_refCounter;

    vm_cond     m_mfe_wait;
    vm_mutex    m_mfe_guard;

    VADisplay      m_vaDisplay;
    VAMFEContextID m_mfe_context;

    std::vector<m_stream_ids_t> m_streams;

    //A number of frames which will be submitted together (Combined)
    mfxU32 m_framesToCombine;

    // A counter frames collected for the next submission. These
    // frames will be submitted together either when get equal to
    // m_pipelineStreams or when collection timeout elapse.
    mfxU32 m_framesCollected;

    // a number of tics per milisecond
    const vm_tick m_vmtick_msec_frequency;
};
#endif // MFX_VA_LINUX && MFX_ENABLE_MFE

#endif /* _MFX_MFE_ADAPTER */
