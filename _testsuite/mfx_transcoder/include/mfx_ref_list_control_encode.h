/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_icurrent_frame_ctrl.h"
#include "mfx_ivideo_encode.h"
#include "mfx_extended_buffer.h"


class RefListControlEncode
    : public InterfaceProxy<IVideoEncode>
    , public ICurrentFrameControl
{
public:
    RefListControlEncode (std::unique_ptr<IVideoEncode> &&pTarget);

    //ICurrentFrameControl
    virtual void AddExtBuffer(mfxExtBuffer &buffer);
    virtual void RemoveExtBuffer(mfxU32 mfxExtBufferId);


    //buffers attached to encode control structure
    mfxStatus EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp) ;

    virtual bool QueryInterface(int interface_id_registered_with_interface_map, void **ppInterface) {
        if (interface_id_registered_with_interface_map == QueryInterfaceMap<ICurrentFrameControl>::id) {
            *((ICurrentFrameControl**)ppInterface) = (ICurrentFrameControl*)this;
            return true;
        }
        return false;
    }

protected:
    //updates frame orders based on current order
    virtual void UpdateRefList(mfxExtAVCRefListCtrl * pCurrent);

    bool m_bAttach;
    mfxExtAVCRefListCtrl m_currentPattern;
    mfxExtAVCRefListCtrl *m_pRefList;//pointer in actual buffer to reduce search overhead
    mfxEncodeCtrl m_ctrl; // if no control attached
    MFXExtBufferVector m_extParams;
    mfxU32 m_nFramesEncoded;
};
