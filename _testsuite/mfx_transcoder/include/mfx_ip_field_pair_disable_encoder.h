/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfxstructures.h"
#include "mfx_ivideo_encode.h"

//class prevents encoding of interlaced I frames as i/p pair, instead fame encodes as i/i pair
class IPFieldPairDisableEncode
    : public InterfaceProxy<IVideoEncode>
{
    typedef InterfaceProxy<IVideoEncode> base;
public:
    IPFieldPairDisableEncode(std::unique_ptr<IVideoEncode> &&pTarget)
        : base(std::move(pTarget))
        , m_gopPicSize()
        , m_nPos()
        , m_encCtl()
    {
    }
    virtual mfxStatus Init(mfxVideoParam *par)
    {
        mfxStatus sts = base::Init(par);

        if (sts >=0)
        {
            mfxVideoParam vg = {0};
            mfxStatus sts2 = base::GetVideoParam(&vg);
            if (sts2 >= 0)
            {
                m_gopPicSize = vg.mfx.GopPicSize;
            }
        }

        return sts;
    }

    virtual mfxStatus EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
    {
        mfxEncodeCtrl *pctrl = ctrl;
        if (NULL != surface)
        {
            MFX_CHECK(0 != m_gopPicSize);
            if (0 == m_nPos)
            {
                if (NULL == pctrl)
                {
                    pctrl = &m_encCtl;
                }
                pctrl->FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_xI;
            }
            m_nPos = (m_nPos + 1) % m_gopPicSize;
        }
        return base::EncodeFrameAsync(pctrl, surface, bs, syncp);
    }

protected:
    //only gop parameter we need to predict i frame position
    mfxU32 m_gopPicSize;
    mfxU32 m_nPos;
    //if encodectrl not passed from upstream
    mfxEncodeCtrl m_encCtl;
};
