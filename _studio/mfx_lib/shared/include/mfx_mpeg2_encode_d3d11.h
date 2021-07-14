// Copyright (c) 2011-2020 Intel Corporation
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

#ifndef __MFX_MPEG2_ENCODE_D3D11__H
#define __MFX_MPEG2_ENCODE_D3D11__H

#include "mfx_common.h"

#if defined(MFX_D3D11_ENABLED)
#if defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE)

#include <vector>
#include <assert.h>

#include "encoding_ddi.h"
#include "auxiliary_device.h"

#include "mfx_ext_buffers.h"
#include "mfxpcp.h"

#include "mfx_mpeg2_enc_common_hw.h"
#include "mfx_mpeg2_encode_d3d_common.h"

#include <d3d11.h>

namespace MfxHwMpeg2Encode
{
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_ENCODE
    class D3D11Encoder : public D3DXCommonEncoder
#else
    class D3D11Encoder : public DriverEncoder
#endif
    {
    public:
        explicit D3D11Encoder(VideoCORE* core);

        virtual ~D3D11Encoder();

        virtual void QueryEncodeCaps(ENCODE_CAPS & caps) override;

        virtual mfxStatus Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId) override;

        virtual mfxStatus Execute(ExecuteBuffers* pExecuteBuffers, mfxU8* pUserData = 0, mfxU32 userDataLen = 0) override;

        virtual mfxStatus Close() override;

        virtual bool      IsFullEncode() const override { return m_bENC_PAK;}
        virtual mfxStatus RegisterRefFrames(const mfxFrameAllocResponse* pResponse) override;

        virtual mfxStatus FillMBBufferPointer(ExecuteBuffers* pExecuteBuffers) override;

        virtual mfxStatus FillBSBuffer(mfxU32 nFeedback, mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt) override;

        virtual mfxStatus SetFrames (ExecuteBuffers* pExecuteBuffers) override;

        virtual mfxStatus QueryStatusAsync(mfxU32 nFeedback, mfxU32 &bitstreamSize) override;

        virtual mfxStatus CreateAuxilliaryDevice(mfxU16 codecProfile) override;

        D3D11Encoder(const D3D11Encoder &) = delete;
        D3D11Encoder & operator = (const D3D11Encoder &) = delete;

    private:
        mfxStatus Init_MPEG2_ENC(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames);
        mfxStatus Init_MPEG2_ENCODE (ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames);

        mfxStatus Execute_ENC    (ExecuteBuffers* pExecuteBuffers, mfxU8* pUserData, mfxU32 userDataLen);
        mfxStatus Execute_ENCODE (ExecuteBuffers* pExecuteBuffers, mfxU8* pUserData, mfxU32 userDataLen);

        mfxStatus Init(
            GUID guid,
            ENCODE_FUNC func,
            ID3D11VideoDevice  *pVideoDevice,
            ID3D11VideoContext *pVideoContext,
            ExecuteBuffers* pExecuteBuffers);

        mfxStatus Execute(
            ExecuteBuffers* pExecuteBuffers,
            mfxU32 func,
            mfxU8* pUserData, mfxU32 userDataLen);

        mfxStatus GetBuffersInfo();

        mfxStatus QueryMbDataLayout();

        mfxStatus QueryCompBufferInfo(
            D3D11_DDI_VIDEO_ENCODER_BUFFER_TYPE type,
            mfxFrameAllocRequest* pRequest);

        mfxStatus CreateMBDataBuffer(
            mfxU32 numRefFrames);

        mfxStatus CreateBSBuffer(
            mfxU32 numRefFrames);


        mfxStatus CreateCompBuffers(
            ExecuteBuffers* pExecuteBuffers, 
            mfxU32 numRefFrames);

        mfxStatus Register(
            const mfxFrameAllocResponse* pResponse, 
            D3D11_DDI_VIDEO_ENCODER_BUFFER_TYPE type);

        mfxI32    GetRecFrameIndex (mfxMemId memID);
        mfxI32    GetRawFrameIndex (mfxMemId memIDe, bool bAddFrames);


        VideoCORE* m_core;

        // d3d11 specific
        ID3D11VideoDevice *                       m_pVideoDevice;
        ID3D11VideoContext *                  m_pVideoContext;
        ID3D11VideoDecoder *                      m_pDecoder;
        GUID                                      m_guid;
        ID3D11VideoDecoderOutputView *            m_pVDOView;

        ENCODE_CAPS                               m_caps;

        std::vector<ENCODE_COMP_BUFFER_INFO>      m_compBufInfo;
        std::vector<D3DDDIFORMAT>                 m_uncompBufInfo;
        ENCODE_MBDATA_LAYOUT                      m_layout;
        bool                                      m_infoQueried;

        mfxFrameAllocResponse                     m_allocResponseMB;
        std::vector<mfxFrameAllocResponse>        m_realResponseMB;
        std::vector<mfxMemId>                     m_midMB;

        mfxFrameAllocResponse                     m_allocResponseBS;
        std::vector<mfxFrameAllocResponse>        m_realResponseBS;
        std::vector<mfxMemId>                     m_midBS;

        mfxRecFrames                              m_reconFrames;
        mfxRawFrames                              m_rawFrames;

        std::vector<mfxHDLPair>                   m_bsQueue;
        std::vector<mfxHDLPair>                   m_mbQueue;
        std::vector<mfxHDLPair>                   m_reconQueue;

        static const mfxU32 MAX_PACKED_SPSPPS_SIZE = 1024;        
        mfxU8 m_packedSpsPpsBuffer[MAX_PACKED_SPSPPS_SIZE]; // sps, pps
        bool                                      m_bENC_PAK; 
        mfxFeedback                               m_feedback;

    };
};

#endif // #if defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE)
#endif // #if defined(MFX_D3D11_ENABLED)
#endif // #ifndef __MFX_MPEG2_ENCODE_D3D11__H
/* EOF */
