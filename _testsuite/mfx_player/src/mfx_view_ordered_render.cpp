/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */
#include "mfx_pipeline_defs.h"
#include "mfx_view_ordered_render.h"
#include "mfx_extended_buffer.h"

MFXViewOrderedRender ::MFXViewOrderedRender (IMFXVideoRender * pSingleRender)
    : InterfaceProxy<IMFXVideoRender> (pSingleRender)
    , m_nActualViewNumber()
    , m_nFramesBuffered()
    , m_isMVC()
    , m_enabled()
    , m_bFirstFrame()
{
}

mfxStatus MFXViewOrderedRender::SetAutoView(bool bIsAutoViewRender)
{
    m_enabled = !bIsAutoViewRender;
    return InterfaceProxy<IMFXVideoRender>::SetAutoView(bIsAutoViewRender);
}

mfxStatus MFXViewOrderedRender::Init(mfxVideoParam *par, const vm_char * pFilename)
{ 
    if (par != NULL && m_enabled)
    {
        //TODO: very weird
        MFXExtBufferPtr<mfxExtMVCSeqDesc> mvc_seq_dsc(*par);
        
        if (mvc_seq_dsc.get())
        {
            MFXExtBufferPtrRef<mfxExtMVCSeqDesc> mvc_seq_dsc_ref(&mvc_seq_dsc);
            std::vector<mfxU16> views;

            //we resizing the vector right now
            m_buffered.resize(mvc_seq_dsc_ref->View.size());

            //we are saving order of views according to dependency structure
            for (size_t i = 0; i < mvc_seq_dsc_ref->View.size(); i++)
            {
                views.push_back(mvc_seq_dsc_ref->View[i].ViewId);
            }
            
            //creating map to indexes in current vector
            for (size_t i = 0; i < mvc_seq_dsc_ref->View.size(); i++)
            {
                m_order_map[views[i]] = i;
            }

            m_nActualViewNumber = m_buffered.size();
            m_isMVC = true;
        }
    }
    MFX_CHECK_STS(InterfaceProxy<IMFXVideoRender>::Init(par, pFilename));
    return MFX_ERR_NONE; 
}

mfxStatus MFXViewOrderedRender::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_STS(InterfaceProxy<IMFXVideoRender>::QueryIOSurf(par, request));

    if (par != NULL && NULL != request && m_enabled)
    {
        MFXExtBufferPtr<mfxExtMVCSeqDesc> mvc_seq_dsc(*par);

        if (mvc_seq_dsc.get())
        {
            request->NumFrameSuggested = request->NumFrameSuggested + (mfxU16)mvc_seq_dsc->View.size();
            request->NumFrameMin = request->NumFrameMin + (mfxU16)mvc_seq_dsc->View.size();
        }
    }
    
    return MFX_ERR_NONE;
}

mfxStatus MFXViewOrderedRender::RenderFrame( mfxFrameSurface1 *surface
                                           , mfxEncodeCtrl * pCtrl)
{
    //render wrapper mode
    if (!m_isMVC)
    {
        MFX_CHECK_STS(InterfaceProxy<IMFXVideoRender>::RenderFrame(surface, pCtrl));
        return MFX_ERR_NONE;
    }
    //nothing to buffer
    if (NULL == surface)
    {
        //it is possible that we processed only one frame and it contains only partial number of views
        if (!m_bFirstFrame)
        {
            m_nActualViewNumber = m_nFramesBuffered;
            MFX_CHECK_STS(FlushBufferedFrames());
        }
        //we have a non completed frame buffer. What should we do?
        MFX_CHECK_WITH_ERR(m_nFramesBuffered == 0, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK_STS(InterfaceProxy<IMFXVideoRender>::RenderFrame(surface, pCtrl));
        
        return MFX_ERR_NONE;
    }
    
    //lets buffer the frame according to order
    int nOrder = m_order_map[surface->Info.FrameId.ViewId];

    if (!m_bFirstFrame)
    {
        //additional buffer border check
        MFX_CHECK((size_t)nOrder < m_buffered.size());
        //if we have buffered view at order, it means next frame arrived
        if (m_buffered[nOrder].first != NULL)
        {
            m_nActualViewNumber = m_nFramesBuffered;
            MFX_CHECK_STS(FlushBufferedFrames());
        }
    }

    MFX_CHECK_WITH_ERR(m_buffered[nOrder].first == NULL, MFX_ERR_UNDEFINED_BEHAVIOR);
    m_buffered[nOrder].first  = surface;
    m_buffered[nOrder].second = pCtrl;
    IncreaseReference(&surface->Data);
    m_nFramesBuffered++;

    MFX_CHECK_STS(FlushBufferedFrames());
    
    return MFX_ERR_NONE;
}

mfxStatus MFXViewOrderedRender::Reset(mfxVideoParam *par)
{
    mfxStatus sts;
    if (m_isMVC)
    {
        m_nFramesBuffered = 0;
        m_bFirstFrame     = true;
    }

    for (size_t i = 0; i< m_buffered.size(); i++)
    {
        if (NULL == m_buffered[i].first)
            continue;

        DecreaseReference(&m_buffered[i].first->Data);
        m_buffered[i].first = NULL;
        m_buffered[i].second = NULL;
    }

    MFX_CHECK_STS(sts = InterfaceProxy<IMFXVideoRender>::Reset(par));
    return sts;
}

mfxStatus MFXViewOrderedRender::FlushBufferedFrames()
{
    //we buffered enough lets render them
    if (m_nFramesBuffered == m_nActualViewNumber)
    {
        for (size_t i = 0; i< m_buffered.size(); i++)
        {
            if (NULL == m_buffered[i].first)
                continue;

            MFX_CHECK_STS(InterfaceProxy<IMFXVideoRender>::RenderFrame(m_buffered[i].first, m_buffered[i].second));
            DecreaseReference(&m_buffered[i].first->Data);
            m_buffered[i].first = NULL;
            m_buffered[i].second = NULL;
        }
        m_nFramesBuffered = 0;
        m_bFirstFrame     = true;
    }
    
    return MFX_ERR_NONE;
}


