/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_rnd_wrapper.h"


//orders surfaces by view
class MFXViewOrderedRender
    : public InterfaceProxy<IMFXVideoRender>
{
protected:
    typedef std::pair<mfxFrameSurface1*, mfxEncodeCtrl* > SurfaceAndCtrl;

protected:
    //maps view id to surface index in storage
    std::map<mfxU16, int>          m_order_map;
    std::vector<SurfaceAndCtrl>    m_buffered;
    size_t                         m_nActualViewNumber;
    //the vector abstraction doesn't allow to count elements if they we inserted not one by one
    size_t                         m_nFramesBuffered;
    bool                           m_isMVC;
    ///enable only if mvcExtSequencedesc structure is attached and auto_view isnot turnedon (child render isn't an autoview)
    bool                           m_enabled;
    //first frame like techink is used to determine actual number of views
    bool                           m_bFirstFrame;

protected:
    //delivers buffered frames to target render
    virtual mfxStatus FlushBufferedFrames();
public:
    MFXViewOrderedRender (IMFXVideoRender * pSingleRender);

    ///need to setup frame buffer array
    virtual mfxStatus Init(mfxVideoParam *par, const vm_char * /*pFilename*/);

    virtual mfxStatus RenderFrame( mfxFrameSurface1 *surface
                                 , mfxEncodeCtrl * pCtrl);

    /*if mvc and buffered one view it should be cleaned up*/
    virtual mfxStatus Reset(mfxVideoParam *par);

    virtual mfxStatus SetAutoView(bool bIsAutoViewRender);

    //since surfaces are buffered number of target surfaces is different than one in target render
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);
};
