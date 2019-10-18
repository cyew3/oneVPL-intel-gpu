/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include <functional>

#include "mfx_pipeline_defs.h"
#include "mfx_reorder_render.h"
#include "mfx_extended_buffer.h"

//Predicates for searching in lists
struct ReorderedListPred1 : public std::binary_function<mfxFrameSurface1*, mfxU32, bool>
{
    bool operator () (const mfxFrameSurface1* surface, const mfxU32 &count) const 
    {
        if (surface->Data.Locked >= 1  && surface->Data.FrameOrder == count)
            return true;
        return false;
    }
};

struct ReorderedListPred2 : public std::binary_function<mfxFrameSurface1*, mfxFrameSurface1*, bool>
{
    bool operator () (const mfxFrameSurface1* surface1, const mfxFrameSurface1* surface2) const 
    {
        if (surface1 == surface2)
            return true;
        return false;
    }
};


MFXDecodeOrderedRender::MFXDecodeOrderedRender(IMFXVideoRender * pDecorated)
: InterfaceProxy<IMFXVideoRender>(pDecorated)
, m_nCurOrder()
{

}

mfxStatus MFXDecodeOrderedRender::Init(mfxVideoParam *par, const vm_char *pFilename )
{
    if (NULL != par)
    {
        bool bFound = false;
        mfxU16 j;
        for(j = 0;j<par->NumExtParam; j++)
        {
            if (par->ExtParam[j]->BufferId == BufferIdOf<mfxExtMVCSeqDesc>::id)
            {
                bFound = true;
                break;
            }
        }
        if (bFound)
        {
            mfxExtMVCSeqDesc * pExtSequence = reinterpret_cast<mfxExtMVCSeqDesc *>(par->ExtParam[j]);
            for (mfxU32 i = 0; i < pExtSequence->NumView; i++)
            {
                m_viewIds[pExtSequence->View[i].ViewId] = false;
            }
        }
    }

    return InterfaceProxy<IMFXVideoRender>::Init(par, pFilename);
}

mfxStatus MFXDecodeOrderedRender::RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl)
{
    if (NULL != surface)
    {
        m_ReorderedList.push_back(surface);
        IncreaseReference(&surface->Data);
    }
    //several frames could be ready at time
    Timeout<MFX_RENDER_DEFAULT_TIMEOUT> timeout;

    for ( ;!timeout; )
    {
        mfxFrameSurface1 *pReorderedSrf = NULL;

        // let find surface in main list
        //it = find_if(m_ReorderedList.begin(), m_ReorderedList.end(), std::bind2nd(ReorderedListPred1(), m_nCurOrder));
        std::list<mfxFrameSurface1*>::iterator it;
        for (it = m_ReorderedList.begin(); it!= m_ReorderedList.end(); it++)
        {
            if ((*it)->Data.Locked >= 1 &&
                (*it)->Data.FrameOrder == m_nCurOrder &&
                //either not mvc or current view wasn't pulled
                (m_viewIds.empty() || !m_viewIds[(*it)->Info.FrameId.ViewId]))
            {
                break;
            }
        }

        if (it != m_ReorderedList.end())
        {
            if (!m_viewIds.empty())
            {
                m_viewIds[(*it)->Info.FrameId.ViewId] = true;
            }

            DisplayedMap::iterator disp_it = m_DisplayedMap.find(m_nCurOrder);
            if (disp_it != m_DisplayedMap.end())
                m_DisplayedMap.erase(disp_it);

            pReorderedSrf = *it;
            m_ReorderedList.erase(it);
            //m_nCurOrder++;
        }
        if (NULL == pReorderedSrf)
        {
            // may be needed surface in already displayed list
            DisplayedMap::iterator disp_it = m_DisplayedMap.find(m_nCurOrder);
            if (disp_it != m_DisplayedMap.end())
            {
                // But the surface should already processed (to be in Reordered list)
                it = find_if(m_ReorderedList.begin(), m_ReorderedList.end(), std::bind2nd(ReorderedListPred2(), disp_it->second));
                if (it != m_ReorderedList.end())
                {
                    pReorderedSrf = disp_it->second;
                    m_DisplayedMap.erase(disp_it);
                    m_ReorderedList.erase(it);
                    //m_nCurOrder++;
                }
            }
        }
        if (NULL == pReorderedSrf)
        {

            if (NULL != surface)
            {
                return MFX_ERR_NONE;
            }
            else
            {
                if (m_ReorderedList.empty())
                    return MFX_ERR_NONE;
                
                timeout.wait("FramesReordering:Sleep");
                continue;
            }
        }

        MFX_CHECK_STS(InterfaceProxy<IMFXVideoRender>::RenderFrame(pReorderedSrf, pCtrl));

        if (NULL != pReorderedSrf)
        {
            DecreaseReference(&pReorderedSrf->Data);
            
             if (m_viewIds.end() == std::find_if(m_viewIds.begin(), m_viewIds.end(), second_of(std::bind1st(std::equal_to<bool>(), false))))
             {

                 std::map<mfxU16, bool> ::iterator it2;
                 for (it2 = m_viewIds.begin(); it2 != m_viewIds.end(); it2++)
                 {
                     it2->second = false;
                 }

                 m_nCurOrder++;
             }
        }
    }

    MFX_CHECK(timeout);

    if (NULL == surface)
    {
        MFX_CHECK_STS(InterfaceProxy<IMFXVideoRender>::RenderFrame(NULL, pCtrl));
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXDecodeOrderedRender::Close()
{
    m_nCurOrder = 0;
    m_ReorderedList.clear();
    return InterfaceProxy<IMFXVideoRender>::Close();
}

void MFXDecodeOrderedRender::PushToDispMap(mfxFrameSurface1 *surface)
{
    // VC1 only. Skip frames support
    if (surface && surface->Data.FrameOrder >= m_nCurOrder)
    {
        m_DisplayedMap.insert(std::pair<mfxU32, mfxFrameSurface1 *>(surface->Data.FrameOrder, surface));
    }
}
