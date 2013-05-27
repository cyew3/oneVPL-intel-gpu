/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_REORDER_RENDER_H
#define __MFX_REORDER_RENDER_H

#include "mfx_rnd_wrapper.h"

//decorator that does reordering before rendering
class MFXDecodeOrderedRender 
    : public InterfaceProxy<IMFXVideoRender>
{
public:
    MFXDecodeOrderedRender(IMFXVideoRender * pDecorated);
    //init examination for attached MVC buffers
    virtual mfxStatus Init(mfxVideoParam *par, const vm_char *pFilename = NULL);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface , mfxEncodeCtrl * pCtrl = NULL);
    virtual mfxStatus Close();
    virtual void      PushToDispMap(mfxFrameSurface1 *surface);

protected:
    typedef std::map<mfxU32, mfxFrameSurface1*> DisplayedMap;
    std::list<mfxFrameSurface1*> m_ReorderedList;
    // Map of already displayed surfaces for VC-1 Skip frames support
    DisplayedMap                 m_DisplayedMap;
    mfxU32                       m_nCurOrder;
    //for mvc every frameorder exists in all wiewids
    std::map<mfxU16, bool>          m_viewIds;
};

#endif//__MFX_REORDER_RENDER_H
