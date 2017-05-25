/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "mfx_icurrent_frame_ctrl.h"
#include "mfx_ivideo_encode.h"
#include "mfx_extended_buffer.h"


class PerFrameExtBufEncode 
    : public InterfaceProxy<IVideoEncode>
    , public ICurrentFrameControl
{
public:
    PerFrameExtBufEncode(std::auto_ptr<IVideoEncode>& pTarget)
        : InterfaceProxy<IVideoEncode>(pTarget)
        , m_bAttach()
        , m_ctrl()
        , m_nFramesEncoded()
    {
    }

    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
    {
        mfxStatus sts = InterfaceProxy<IVideoEncode>::QueryIOSurf(par, request);

        if (!sts && request)
        {
            m_buf.resize(request->NumFrameMin + 1);
        }

        return sts;
    }

    virtual void AddExtBuffer(mfxExtBuffer &buffer)
    {
        if (!m_bAttach)
        {
            m_buf.splice(m_buf.end(), m_buf, m_buf.begin());
            m_buf.front().clear();
            m_bAttach = true;
        }

        m_buf.front().push_back(buffer);
    }
    virtual void RemoveExtBuffer(mfxU32 /*mfxExtBufferId*/) {}


    mfxStatus EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
    {
        if (!m_bAttach)
        {
            if (0 != surface)
            {
                m_nFramesEncoded++;
            }
            return InterfaceProxy<IVideoEncode>::EncodeFrameAsync(ctrl, surface, bs, syncp);
        }

        mfxExtBuffer ** ppExtParams = 0;

        if (0 == ctrl)
        {
            ctrl = &m_ctrl;
        }
        else
        {
            ppExtParams = ctrl->ExtParam;

            for (mfxU32 i = 0; i < ctrl->NumExtParam; i++)
            {
                mfxExtBuffer* pBufExt = ctrl->ExtParam[i];
                MFXExtBufferPtrBase* pBufInt = m_buf.front().get_by_id(pBufExt->BufferId);

                if (!pBufInt)
                {
                    m_buf.front().push_back(pBufExt);
                }
            }
        }

        ctrl->NumExtParam = (mfxU16)m_buf.front().size();
        ctrl->ExtParam = &m_buf.front();


        if (NULL != surface)
        {
            m_nFramesEncoded++;
        }

        mfxStatus sts = InterfaceProxy<IVideoEncode>::EncodeFrameAsync(ctrl, surface, bs, syncp);

        //detaching extbuffer
        if (ctrl != &m_ctrl)
        {
            ctrl->ExtParam = ppExtParams;
        }

        m_bAttach = false;

        return sts;
    }

    virtual bool QueryInterface(int interface_id_registered_with_interface_map, void **ppInterface) {
        if (interface_id_registered_with_interface_map == QueryInterfaceMap<ICurrentFrameControl>::id) {
            *((ICurrentFrameControl**)ppInterface) = (ICurrentFrameControl*)this;
            return true;
        }
        return false;
    }

protected:
    bool m_bAttach;
    mfxEncodeCtrl m_ctrl; // if no control attached
    mfxU32 m_nFramesEncoded;
    std::list<MFXExtBufferVector> m_buf;
};
