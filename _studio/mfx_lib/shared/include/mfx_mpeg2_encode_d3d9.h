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

#ifndef __MFX_MPEG2_ENCODE_D3D9__H
#define __MFX_MPEG2_ENCODE_D3D9__H

#include "mfx_common.h"

#if defined(MFX_VA)
#if defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE) || defined(MFX_ENABLE_MPEG2_VIDEO_ENC)

#include <vector>
#include <assert.h>

#include "encoding_ddi.h"
#include "auxiliary_device.h"

#include "mfx_ext_buffers.h"
#include "mfxpcp.h"

#include "mfx_mpeg2_enc_common_hw.h"
#include "mfx_mpeg2_encode_d3d_common.h"

namespace MfxHwMpeg2Encode
{
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    class D3D9Encoder : public D3DXCommonEncoder
#else
    class D3D9Encoder : public DriverEncoder
#endif
    {
    public:
        explicit D3D9Encoder(VideoCORE* core);

        virtual ~D3D9Encoder();
        virtual mfxStatus QueryEncodeCaps(ENCODE_CAPS & caps, mfxU16 codecProfileType);

        virtual mfxStatus Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId);

        virtual mfxStatus Execute(ExecuteBuffers* pExecuteBuffers, mfxU8* pUserData = 0, mfxU32 userDataLen = 0);

        virtual mfxStatus Close();

        virtual bool      IsFullEncode() const { return m_bENC_PAK; }

        virtual mfxStatus RegisterRefFrames(const mfxFrameAllocResponse* pResponse);

        virtual mfxStatus FillMBBufferPointer(ExecuteBuffers* pExecuteBuffers);

        virtual mfxStatus FillBSBuffer(mfxU32 nFeedback, mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt);

        virtual mfxStatus SetFrames (ExecuteBuffers* pExecuteBuffers);

        virtual mfxStatus QueryStatusAsync(mfxU32 nFeedback, mfxU32 &bitstreamSizemfxU32);

    private:

        // Declare private copy constructor to avoid accidental assignment
        // and klocwork complaining.
        D3D9Encoder(const D3D9Encoder &);
        D3D9Encoder & operator = (const D3D9Encoder &);

        mfxStatus Init_MPEG2_ENC(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames);
        mfxStatus Init_MPEG2_ENCODE (ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames);

        mfxStatus Execute_ENC    (ExecuteBuffers* pExecuteBuffers, mfxU8 *pUserData = 0, mfxU32 userDataLen = 0);
        mfxStatus Execute_ENCODE (ExecuteBuffers* pExecuteBuffers, mfxU8 *pUserData, mfxU32 userDataLen);

        mfxStatus QueryCompBufferInfo(D3DDDIFORMAT type,mfxFrameAllocRequest* pRequest);
        mfxStatus CreateCompBuffers  (ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames);
        mfxStatus CreateMBDataBuffer  (mfxU32 numRefFrames);
        mfxStatus CreateBSBuffer      (mfxU32 numRefFrames);
        //mfxStatus CreateBSBuffer      (mfxU32 numRefFrames);
        mfxStatus GetBuffersInfo();
        mfxStatus QueryMbDataLayout();
        mfxStatus Init(const GUID& guid, ENCODE_FUNC func, ExecuteBuffers* pExecuteBuffers);
        mfxStatus Execute(ExecuteBuffers* pExecuteBuffers, mfxU32 func, mfxU8* pUserData, mfxU32 userDataLen);
        mfxStatus Register (const mfxFrameAllocResponse* pResponse, D3DDDIFORMAT type);
        mfxI32    GetRecFrameIndex (mfxMemId memID);
        mfxI32    GetRawFrameIndex(mfxMemId memIDe, bool bAddFrames);


        VideoCORE*                          m_core;
        AuxiliaryDevice*                    m_pDevice;

        std::vector<ENCODE_COMP_BUFFER_INFO> m_compBufInfo;
        std::vector<D3DDDIFORMAT> m_uncompBufInfo;
        ENCODE_MBDATA_LAYOUT m_layout;
        mfxFeedback                             m_feedback;

        mfxFrameAllocResponse               m_allocResponseMB;
        mfxFrameAllocResponse               m_allocResponseBS;
        mfxRecFrames                        m_recFrames;
        mfxRawFrames                        m_rawFrames;
        bool                                m_bENC_PAK;

#ifdef MPEG2_ENC_HW_PERF
        vm_time lock_MB_data_time[3];
        vm_time copy_MB_data_time[3];
#endif

    };
};

#endif
#endif //#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE) && defined (MFX_VA_WIN)
#endif //#ifndef __MFX_MPEG2_ENCODE_D3D9__H
/* EOF */
