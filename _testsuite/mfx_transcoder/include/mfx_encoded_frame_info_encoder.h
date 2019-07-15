/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once
#include "mfx_ifactory.h"
#include "mfx_serializer.h"


class EncodedFrameInfoEncoder
    : public InterfaceProxy<IVideoEncode>
    , public ICurrentFrameControl
{
    typedef InterfaceProxy<IVideoEncode> base;
public:
    EncodedFrameInfoEncoder(std::unique_ptr<IVideoEncode> &&pEncode)
        : base (std::move(pEncode))
        , m_bAttach()
        , m_extBufferToAttach() {
    }
    virtual void AddExtBuffer(mfxExtBuffer &buffer) {
        m_bAttach = true;
        m_extBufferToAttach  = (mfxExtAVCEncodedFrameInfo&)buffer;
    }
    virtual void RemoveExtBuffer(mfxU32 /*mfxExtBufferId*/) {
        m_bAttach = false;
    }
    mfxStatus EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp) {
        if (!m_bAttach) {
            return base::EncodeFrameAsync(ctrl, surface, bs, syncp);
        }

        auto_ext_buffer auto_buf(*bs);
        auto_buf.insert((mfxExtBuffer*)&m_extBufferToAttach);

        mfxStatus sts = base::EncodeFrameAsync(ctrl, surface, bs, syncp);

        if (MFX_ERR_NONE >= sts) {
            PipelineTrace ((VM_STRING("%s"), Serialize(m_extBufferToAttach).c_str()));
        }

        return sts;
    }
protected:
    bool m_bAttach;
    mfxExtAVCEncodedFrameInfo m_extBufferToAttach;
};
