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
#include <vaapi_ext_interface.h>
#include <vector>
#include <list>
#include "vm_mutex.h"
#include "vm_cond.h"
#include "vm_time.h"
#include <mfxstructures.h>

class MFEVAAPIEncoder
{
    typedef struct m_stream_ids_s{
        VAContextID       ctx;
        mfxStatus         sts;
        m_stream_ids_s():
                         ctx(VA_INVALID_ID)
                        ,sts(MFX_ERR_NONE) {};
        m_stream_ids_s( VAContextID _ctx,
                        mfxStatus _sts):
                                        ctx(_ctx)
                                       ,sts(_sts) {};
        inline bool isSubmitted() { return (VA_INVALID_ID == ctx); }
    } m_stream_ids_t;

public:
    MFEVAAPIEncoder();

    virtual
        ~MFEVAAPIEncoder();
    mfxStatus Create(mfxExtMultiFrameParam const & par, VADisplay vaDisplay);


    mfxStatus Join(VAContextID ctx);
    mfxStatus Disjoin(VAContextID ctx);
    mfxStatus Destroy();
    mfxStatus Submit(VAContextID context, mfxU32 timeToWait);

    virtual void AddRef();
    virtual void Release();

private:
    mfxU32      m_refCounter;

    vm_cond     m_mfe_wait;
    vm_mutex    m_mfe_guard;

    VADisplay      m_vaDisplay;
    VAMFEContextID m_mfe_context;

    // a pool (heap) of objects
    std::list<m_stream_ids_t> m_streams_pool;

    // a list of objects filled with context info ready to submit
    std::list<m_stream_ids_t> m_toSubmit;

    typedef std::list<m_stream_ids_t>::iterator StreamsIter_t;

    //A number of frames which will be submitted together (Combined)
    mfxU32 m_framesToCombine;

    //A desired number of frames which might be submitted. if
    // actual number of sessions less then it,  m_framesToCombine
    // will be adjusted
    mfxU32 m_maxFramesToCombine;

    // A counter frames collected for the next submission. These
    // frames will be submitted together either when get equal to
    // m_pipelineStreams or when collection timeout elapses.
    mfxU32 m_framesCollected;

    // We need contexts extracted from m_toSubmit to
    // place to a linear vector to pass them to vaMFESubmit
    std::vector<VAContextID> m_contexts;
    // store iterators to particular items
    std::vector<StreamsIter_t> m_streams;

    // a number of tics per milisecond
    const vm_tick m_mfe_vmtick_msec_frequency;

    // currently up-to-to 3 frames worth combining
    static const mfxU32 MAX_FRAMES_TO_COMBINE = 3;

    // symbol is pointed by  VPG_EXT_VA_CREATE_MFECONTEXT
    vaExtCreateMfeContext vaCreateMFEContext;
    // symbol is pointed by  VPG_EXT_VA_ADD_CONTEXT
    vaExtAddContext vaAddContext;
    // symbol is pointed by  VPG_EXT_VA_RELEASE_CONTEXT
    vaExtReleaseContext vaReleaseContext;
    // symbol is pointed by  VPG_EXT_VA_MFE_SUBMIT
    vaExtMfeSubmit vaMFESubmit;
};
#endif // MFX_VA_LINUX && MFX_ENABLE_MFE

#endif /* _MFX_MFE_ADAPTER */
