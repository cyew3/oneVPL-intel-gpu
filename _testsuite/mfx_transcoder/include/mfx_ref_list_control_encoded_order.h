/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include <fstream>
#include "mfx_ivideo_encode.h"

class RefListControlEncodedOrder
    : public InterfaceProxy<IVideoEncode>
{
public:
    RefListControlEncodedOrder(std::auto_ptr<IVideoEncode>& pTarget, const vm_char* par_file);
    virtual ~RefListControlEncodedOrder();

    virtual mfxStatus Init(mfxVideoParam * pInit);
    virtual mfxStatus EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp);
    virtual mfxStatus Close();

    mfxStatus ReorderFrame(mfxFrameSurface1 * in, mfxFrameSurface1 ** out, mfxU16 * frameType, mfxExtAVCRefLists * pRefLists);


protected:
    struct Frame
    {
        Frame()
            : order(-1)
            , type(0)
            , picStruct(0)
        {
            refLists = { {MFX_EXTBUFF_AVC_REFLISTS, sizeof(mfxExtAVCRefLists)}, };
        }

        mfxI32 order;
        mfxU16 type;
        mfxU16 picStruct;
        mfxExtAVCRefLists refLists;
    };

    typedef std::vector<mfxExtBuffer *> ExtBuffersVector;

    std::map<mfxFrameSurface1*, mfxEncodeCtrl *>     m_controls;
    std::map<mfxFrameSurface1*, mfxExtAVCRefLists *> m_refLists;
    std::map<mfxFrameSurface1*, ExtBuffersVector *>  m_extBuffers;

    const vm_char* m_file_name;

    std::ifstream  m_file;

    std::map<mfxI32, mfxFrameSurface1*> m_cachedFrames;
    Frame m_nextFrame;
};
