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

#include "mfx_common.h"

#if defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE)

#ifndef __MFX_MPEG2_ENCODE_INTERFACE__H
#define __MFX_MPEG2_ENCODE_INTERFACE__H

#include <vector>
#include <assert.h>
#include "mfxdefs.h"
#include "mfxvideo++int.h"

#include "mfx_mpeg2_enc_common_hw.h"

#if   defined(MFX_VA_WIN)
#include "encoding_ddi.h"
#include "encoder_ddi.hpp"
#else
#include "mfx_h264_encode_struct_vaapi.h"
#include <va/va_enc_mpeg2.h>
#endif

#if defined (_WIN32)
#pragma warning (push)
#pragma warning (disable:4351)  // value initialization is intended and disabling this warning recommended by MS: https://msdn.microsoft.com/en-us/en-en/library/1ywe7hcy.aspx
#endif


namespace MfxHwMpeg2Encode
{
#ifdef PAVP_SUPPORT
    class AesCounter : public mfxAES128CipherCounter
    {
    public:
        AesCounter();

        void Init(mfxExtPAVPOption const & opt);

        bool Increment();

        bool IsWrapped() const { return m_wrapped; }

        void ResetWrappedFlag() { m_wrapped = false; }

    private:
        mfxExtPAVPOption m_opt;
        bool m_wrapped;
    };
#endif
    class Encryption
    {
    public:
        bool                   m_bEncryptionMode;
#ifdef PAVP_SUPPORT
        UINT                    m_CounterAutoIncrement;
        D3DAES_CTR_IV           m_InitialCounter;
        PAVP_ENCRYPTION_MODE    m_PavpEncryptionMode;
        AesCounter              m_aesCounter;
#endif
        Encryption()
            : m_bEncryptionMode (false)
#ifdef PAVP_SUPPORT
            , m_aesCounter()
#endif
        {
#ifdef PAVP_SUPPORT
            memset (&m_InitialCounter,0,sizeof(m_InitialCounter)) ;
            memset (&m_PavpEncryptionMode,0,sizeof(m_PavpEncryptionMode));
            m_CounterAutoIncrement = 0;
#endif
        }
        void Init(const mfxVideoParamEx_MPEG2* par, mfxU32 funcId)
        {
            m_bEncryptionMode = false;
            par;funcId;

 #ifdef PAVP_SUPPORT
            if (par->mfxVideoParams.Protected && (funcId == ENCODE_ENC_PAK_ID))
            {
                m_bEncryptionMode = true;
                m_CounterAutoIncrement  = par->sExtPAVPOption.CounterIncrement;
                m_InitialCounter.IV     = par->sExtPAVPOption.CipherCounter.IV;
                m_InitialCounter.Count  = par->sExtPAVPOption.CipherCounter.Count;
                m_PavpEncryptionMode.eEncryptionType    = par->sExtPAVPOption.EncryptionType;
                m_PavpEncryptionMode.eCounterMode    = par->sExtPAVPOption.CounterType;
                m_aesCounter.Init(par->sExtPAVPOption);
            }
            else
            {
                m_CounterAutoIncrement  = 0;
                m_InitialCounter.IV     = 0;
                m_InitialCounter.Count  = 0;
                m_PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;
                m_PavpEncryptionMode.eCounterMode    = PAVP_COUNTER_TYPE_A;
            }
#endif
        }
    };
    struct ExecuteBuffers
    {
        ExecuteBuffers()
            : m_caps()
            , m_sps()
            , m_vui()
            , m_pps()
            , m_pSlice()
            , m_pMBs()
            , m_mbqp_data()
            , m_SkipFrame()
            , m_quantMatrix()
            , m_pSurface()
            , m_pSurfacePair()
            , m_idxMb(DWORD(-1))
            , m_idxBs()
            , m_nSlices()
            , m_nMBs()
            , m_bAddSPS()
            , m_bAddDisplayExt()
            , m_RecFrameMemID()
            , m_RefFrameMemID()
            , m_CurrFrameMemID()
            , m_bExternalCurrFrame()
            , m_bExternalCurrFrameHDL()
            , m_bOutOfRangeMV()
            , m_bErrMBType()
            , m_bUseRawFrames()
#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
            , m_bTriggerGpuHang()
#endif
            , m_bDisablePanicMode()
            , m_GOPPictureSize()
            , m_GOPRefDist()
            , m_GOPOptFlag()
            , m_encrypt()
            , m_fFrameRate()
            , m_FrameRateExtN()
            , m_FrameRateExtD()
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
            , m_GpuEvent()
#endif
        {}

        mfxStatus Init (const mfxVideoParamEx_MPEG2* par, mfxU32 funcId, bool bAllowBRC = false);
        mfxStatus Close();

        mfxStatus InitPictureParameters(mfxFrameParamMPEG2* pParams, mfxU32 frameNum);
        void      InitFramesSet(mfxMemId curr, bool bExternal, mfxMemId rec, mfxMemId ref_0,mfxMemId ref_1);
        mfxStatus InitSliceParameters(mfxU8 qp, mfxU16 scale_type, mfxU8 * mbqp, mfxU32 numMB);

        ENCODE_ENC_CTRL_CAPS                    m_caps;

        ENCODE_SET_SEQUENCE_PARAMETERS_MPEG2    m_sps;
        ENCODE_SET_VUI_PARAMETER_MPEG2          m_vui;
        ENCODE_SET_PICTURE_PARAMETERS_MPEG2     m_pps;
        ENCODE_SET_SLICE_HEADER_MPEG2*          m_pSlice;
        ENCODE_ENC_MB_DATA_MPEG2*               m_pMBs;
        mfxU8*                                  m_mbqp_data;
        mfxU8                                   m_SkipFrame;

#if defined (MFX_VA_WIN)
        ENCODE_SET_PICTURE_QUANT                m_quantMatrix;
#else
        VAIQMatrixBufferMPEG2                   m_quantMatrix;
#endif

        mfxFrameSurface1*                       m_pSurface;
        mfxHDLPair                              m_pSurfacePair;

        DWORD                                   m_idxMb;
        DWORD                                   m_idxBs;
        DWORD                                   m_nSlices;
        DWORD                                   m_nMBs;
        DWORD                                   m_bAddSPS;
        bool                                    m_bAddDisplayExt;

        mfxMemId                                m_RecFrameMemID;
        mfxMemId                                m_RefFrameMemID[2];
        mfxMemId                                m_CurrFrameMemID;
        bool                                    m_bExternalCurrFrame;
        bool                                    m_bExternalCurrFrameHDL;
        bool                                    m_bOutOfRangeMV;
        bool                                    m_bErrMBType;
        bool                                    m_bUseRawFrames;
#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
        bool                                    m_bTriggerGpuHang;
#endif
        bool                                    m_bDisablePanicMode;
        USHORT                                  m_GOPPictureSize;
        UCHAR                                   m_GOPRefDist;
        UCHAR                                   m_GOPOptFlag;

        Encryption                              m_encrypt;
        Ipp64f                                  m_fFrameRate;

        mfxU32                                  m_FrameRateExtN;
        mfxU32                                  m_FrameRateExtD;

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        GPU_SYNC_EVENT_HANDLE       m_GpuEvent;
#endif
    };

    class DriverEncoder
    {
    public:
        virtual ~DriverEncoder(){}

        virtual void QueryEncodeCaps(ENCODE_CAPS & caps) = 0;
        virtual mfxStatus Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId) = 0;
        virtual mfxStatus CreateContext(ExecuteBuffers* /*pExecuteBuffers*/, mfxU32 /*numRefFrames*/, mfxU32 /*funcId*/) { return MFX_ERR_NONE; }

        virtual mfxStatus Execute(ExecuteBuffers* pExecuteBuffers, mfxU8* pUserData = 0, mfxU32 userDataLen = 0) = 0;

        virtual bool      IsFullEncode() const = 0;

        virtual mfxStatus Close() = 0;

        virtual mfxStatus RegisterRefFrames(const mfxFrameAllocResponse* pResponse) = 0;

        virtual mfxStatus FillMBBufferPointer(ExecuteBuffers* pExecuteBuffers) = 0;

        virtual mfxStatus FillBSBuffer(mfxU32 nFeedback, mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt) = 0;

        virtual mfxStatus SetFrames (ExecuteBuffers* pExecuteBuffers) = 0;

        virtual mfxStatus CreateAuxilliaryDevice(mfxU16 codecProfile) = 0;
    };

    DriverEncoder* CreatePlatformMpeg2Encoder( VideoCORE* core );

}; // namespace


#if defined (_WIN32)
#pragma warning (pop)
#endif

#endif //#ifndef __MFX_MPEG2_ENCODE_INTERFACE__H
#endif //(MFX_ENABLE_MPEG2_VIDEO_ENCODE)
/* EOF */
