/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ivideo_encode.h"
#include "mfx_frame_reorderer.h"

class EncodeOrderEncode
    : public InterfaceProxy<IVideoEncode>
{
public:

    EncodeOrderEncode(std::auto_ptr<IVideoEncode>& pTarget, bool useParFile, const vm_char* m_par_file)
        : InterfaceProxy<IVideoEncode>(pTarget)
        , m_useParFile(useParFile)
        , m_par_file(m_par_file)
    {
    }

    virtual mfxStatus Init(mfxVideoParam * pInit)
    {
        mfxStatus sts = MFX_ERR_NONE;
        mfxVideoParam getVParams = {};
        MFX_CHECK_POINTER(pInit);
        MFX_CHECK_STS(InterfaceProxy<IVideoEncode>::Init(pInit));

        getVParams.NumExtParam = pInit->NumExtParam;
        getVParams.ExtParam = pInit->ExtParam;
        MFX_CHECK_STS(GetVideoParam(&getVParams));

        if (m_useParFile)
        {
            MFX_CHECK_POINTER((m_reorderer.reset(new MFXParFileFrameReorderer(m_par_file, sts)), m_reorderer.get()));
        }
        else
        {
            switch (pInit->mfx.CodecId)
            {
            case MFX_CODEC_AVC:
            case MFX_CODEC_HEVC:
                MFX_CHECK_POINTER((m_reorderer.reset(new MFXH264FrameReorderer(getVParams)), m_reorderer.get()));
                break;
            case MFX_CODEC_MPEG2:
                MFX_CHECK_STS((m_reorderer.reset(new MFXMPEG2FrameReorderer(getVParams, sts)), sts));
                break;
            default:
                MFX_CHECK_TRACE(false, VM_STRING("unsupported CodecId for EncodeOrder"));
            }
        }
        
        return sts;
    }
    virtual mfxStatus EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
    {
        mfxFrameSurface1* pSurfToEncode = 0;
        mfxU16 frameTypeToEncode = 0;
        mfxStatus sts_reorder = MFX_ERR_NONE;
        mfxStatus sts_enc = MFX_ERR_NONE;

        MFX_CHECK_STS_SKIP(sts_reorder = m_reorderer->ReorderFrame(surface, &pSurfToEncode, &frameTypeToEncode), MFX_ERR_MORE_DATA);

        if (sts_reorder == MFX_ERR_NONE)
        {
            mfxEncodeCtrl * newControl = NULL;
            if (NULL == ctrl)
            {
                if (NULL == (newControl = m_Controls[pSurfToEncode]))
                {
                    MFX_CHECK_POINTER(newControl = new mfxEncodeCtrl());
                    memset(newControl, 0, sizeof(mfxEncodeCtrl));
                    m_Controls[pSurfToEncode] = newControl;
                }
            }
            else
            {
                newControl = ctrl;
            }

            newControl->FrameType = frameTypeToEncode;

            MFX_CHECK_STS_SKIP(sts_enc = InterfaceProxy<IVideoEncode>::EncodeFrameAsync(newControl, pSurfToEncode, bs, syncp), MFX_ERR_MORE_DATA);
        }
        else if (surface == 0)
        {
            MFX_CHECK_STS_SKIP(sts_enc = InterfaceProxy<IVideoEncode>::EncodeFrameAsync(0, 0, bs, syncp), MFX_ERR_MORE_DATA);
        }

        //pull out frames from reorderer as well
        if (NULL == surface && MFX_ERR_MORE_DATA == sts_enc && MFX_ERR_NONE == sts_reorder)
        {
            sts_enc = MFX_ERR_NONE;
        }
        
        return sts_enc;
    }

    virtual mfxStatus Close()
    {
        mfxStatus sts = InterfaceProxy<IVideoEncode>::Close();

        for_each(m_Controls.begin(), m_Controls.end(), second_of(deleter<mfxEncodeCtrl *>()));
        m_Controls.clear();

        return sts; 
    }


protected:
    std::auto_ptr<MFXFrameReorderer> m_reorderer;
    std::map<mfxFrameSurface1*, mfxEncodeCtrl *> m_Controls;
    bool m_useParFile;
    const vm_char* m_par_file;
};