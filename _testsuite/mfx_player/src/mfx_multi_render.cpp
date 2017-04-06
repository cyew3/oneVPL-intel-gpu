/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2017 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_multi_render.h"

MFXMultiRender::MFXMultiRender (IMFXVideoRender * pSingleRender)
    : InterfaceProxy<IMFXVideoRender> (pSingleRender)
{
}

MFXMultiRender :: ~MFXMultiRender ()
{
    //input render will be deleted as part of view array
    if (!m_Renders.empty())
        m_pTarget.release();

    for_each(m_Renders.begin(), m_Renders.end(), deleter<MFXViewRender<_TView>*>());
}

mfxStatus MFXMultiRender::RenderFrame( mfxFrameSurface1 *surface
                                     , mfxEncodeCtrl * pCtrl)
{
    //cleaning up all existed renderers
    if (NULL == surface)
    {
        _CollectionType  :: iterator it;

        for (it = m_Renders.begin(); it != m_Renders.end(); it++)
        {
            MFX_CHECK_STS((*it)->RenderFrame(surface, pCtrl));
        }

        //no gcc compliant code
        /*for each(MFXViewRender<_TView> * viewRender in m_Renders)
        {
            MFX_CHECK_STS(viewRender->RenderFrame(surface, pCtrl));
        }*/
        return MFX_ERR_NONE;
    }

    mfxStatus sts = MFX_ERR_NONE;

    _CollectionType::iterator it = std::find_if( m_Renders.begin(), m_Renders.end(), bind1st(MFXViewRenderCompare<_TView>(), surface->Info.FrameId.ViewId));

    if (it == m_Renders.end())
    {
        std::auto_ptr<IMFXVideoRender> pRenderToView;
        if (m_Renders.empty())
        {
            //no clone
            pRenderToView.reset(m_pTarget.get());
        }
        else
        {
            pRenderToView.reset(m_pTarget.get()->Clone());
        }

        m_Renders.push_back(new MFXViewRender<_TView>(pRenderToView.release(), surface->Info.FrameId.ViewId));

        _CollectionType::iterator it_end = m_Renders.end();
        it = --it_end;
    }

    MFX_CHECK_STS((*it)->RenderFrame(surface, pCtrl));

    return sts;
}
