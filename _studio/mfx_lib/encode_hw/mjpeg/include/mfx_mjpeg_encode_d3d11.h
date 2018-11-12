// Copyright (c) 2011-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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

#include "mfx_mjpeg_encode_d3d_common.h"

#include "mfx_mjpeg_encode_d3d9.h" // suggest that the same syncop based on cache functionality is used


namespace MfxHwMJpegEncode
{
    // encoder
    class D3D11Encoder : public D3DXCommonEncoder
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
        mfxStatus ExecuteImpl(DdiTask &task, mfxHDL surface);

        virtual
        mfxStatus QueryBitstreamBufferInfo(
            mfxFrameAllocRequest & request);

        virtual
        mfxStatus QueryEncodeCaps(
            JpegEncCaps & caps);

        virtual
        mfxStatus UpdateBitstream(
            mfxMemId    MemId,
            DdiTask   & task);

        virtual
        mfxStatus Destroy();

    protected:
        // async call
        virtual
        mfxStatus QueryStatusAsync(
            DdiTask & task);

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
