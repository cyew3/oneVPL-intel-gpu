/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_iyuv_source.h"
#include "mfx_ibitstream_reader.h"
#include "mfx_ivideo_render.h"
#include "mfx_ivpp.h"
#include "mfx_itime.h"
#include "mfx_icurrent_frame_ctrl.h"
#include "mfx_component_params.h"

//interface for pipeline however it very different from pipeline politics - 
//user shouldn't interact with this interface, 
class IPipelineControl
{
public:
    virtual ~IPipelineControl(){}
    virtual mfxStatus GetYUVSource(IYUVSource**ppSource) = 0;
    virtual mfxStatus GetSplitter (IBitstreamReader**ppReader) = 0;
    virtual mfxStatus GetRender   (IMFXVideoRender**ppRender) = 0;
    virtual mfxStatus GetVPP      (IMFXVideoVPP **ppVpp) = 0;
    virtual mfxStatus GetTimer    (ITime**ppTimer) = 0;
    virtual mfxStatus GetEncoder  (IVideoEncode **ppCtrl) = 0;

    //number of buffers need to gets cleared after seeking
    virtual mfxStatus ResetAfterSeek() = 0;
    //
    virtual mfxU32 GetNumDecodedFrames(void) = 0;

    //dynamic resolution for vpp support
    virtual mfxStatus GetVppParams(ComponentParams *& pParams) = 0;
};
