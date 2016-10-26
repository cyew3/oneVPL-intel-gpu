//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2014 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_MJPEG_ENCODE_D3D11_H__
#define __MFX_MJPEG_ENCODE_D3D11_H__

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_WIN) && defined (MFX_D3D11_ENABLED)

#include <vector>
#include <assert.h>

#include <d3d11.h>

#include "mfx_ext_buffers.h"
#include "mfxpcp.h"

#include "mfx_mjpeg_encode_hw_utils.h"
#include "mfx_mjpeg_encode_interface.h"

#include "mfx_mjpeg_encode_d3d9.h" // suggest that the same syncop based on cache functionality is used


namespace MfxHwMJpegEncode
{
    // encoder
    class D3D11Encoder : public DriverEncoder
    {
    public:
        D3D11Encoder();

        virtual
        ~D3D11Encoder();

        virtual
        mfxStatus CreateAuxilliaryDevice(
            VideoCORE * core,
            mfxU32      width,
            mfxU32      height,
            bool        isTemporal = false);

        virtual
        mfxStatus CreateAccelerationService(
            mfxVideoParam const & par);

        virtual
        mfxStatus RegisterBitstreamBuffer(
            mfxFrameAllocResponse & response);

        virtual
        mfxStatus Execute(DdiTask &task, mfxHDL surface);

        virtual
        mfxStatus QueryBitstreamBufferInfo(
            mfxFrameAllocRequest & request);

        virtual
        mfxStatus QueryEncodeCaps(
            JpegEncCaps & caps);

        virtual
        mfxStatus QueryStatus(
            DdiTask & task);

        virtual
        mfxStatus UpdateBitstream(
            mfxMemId    MemId,
            DdiTask   & task);

        virtual
        mfxStatus Destroy();

    private:
        D3D11Encoder(D3D11Encoder const &);              // no implementation
        D3D11Encoder & operator =(D3D11Encoder const &); // no implementation

        mfxStatus Init(
            GUID guid,
            ID3D11VideoDevice *pVideoDevice,
            ID3D11VideoContext *pVideoContext,
            mfxU32      width,
            mfxU32      height);

        VideoCORE *                    m_core;
        ID3D11VideoDevice *            m_pVideoDevice;
        ID3D11VideoContext *           m_pVideoContext;
        ID3D11VideoDecoder *           m_pDecoder;
        GUID                           m_guid;

        mfxU32            m_width;
        mfxU32            m_height;
        ENCODE_CAPS_JPEG  m_caps;
        bool              m_infoQueried;

        std::vector<ENCODE_COMP_BUFFER_INFO>     m_compBufInfo;
        std::vector<D3DDDIFORMAT>                m_uncompBufInfo;
        std::vector<ENCODE_QUERY_STATUS_PARAMS>  m_feedbackUpdate;
        CachedFeedback                           m_feedbackCached;

        std::vector<mfxHDLPair>                  m_bsQueue;
    };

}; // namespace

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_WIN) && defined (MFX_D3D11_ENABLED)
#endif // __MFX_MJPEG_ENCODE_D3D11_H__
