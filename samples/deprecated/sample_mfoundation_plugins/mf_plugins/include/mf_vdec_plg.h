/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

/*********************************************************************************

File: mf_vdec_plg.h

Purpose: define common code for MSDK based decoder MFTs.

Defined Types:
  * MFPluginVDec - [class] - MFT for MSDK encoders
  * MFDecState - [enum] - MFT states enum

Defined Macroses:
  * MF_FREE_SURF_WAIT_NUM - number of ties in search of free work surface
  * MF_RENDERING_SURF_NUM - additional number of surfaces to use

Registry Properties:
  * MfxMftDec_MSDK - [DWORD] - 0 = default behavior, 1 = use SW MediaSDK,
  2 = use HW MediaSDK
  * MfxMftDec_Memory - [DWORD] -  0 = default behavior, 1 = use SW memory,
  2 = use HW memory
  * MfxMftDec_in_fname - [REG_SZ] - file to store input bitstream before frame
  constructor
  * MfxMftDec_in_fname_fc - [REG_SZ] - file to store input bitstream after frame
  constructor
  * MfxMftDec_out_fname - [REG_SZ] - file to store output frames

*********************************************************************************/

#ifndef __MF_VDEC_PLG_H__
#define __MF_VDEC_PLG_H__

#include "mf_utils.h"
#include "mf_dxva_support.h"
#include "mf_plugin.h"
#include "mf_vdec_ibuf.h"
#include "mf_yuv_obuf.h"
#include "mf_samples_extradata.h"

#include "d3d_allocator.h"
#include "d3d11_allocator.h"

/*------------------------------------------------------------------------------*/

#define REG_DEC_ASYNC      "MfxMftDec_async"
#define REG_DEC_MSDK       "MfxMftDec_MSDK"
#define REG_DEC_MEMORY     "MfxMftDec_Memory"
#define REG_DEC_IN_FILE    "MfxMftDec_in_fname"
#define REG_DEC_IN_FILE_FC "MfxMftDec_in_fname_fc"
#define REG_DEC_OUT_FILE   "MfxMftDec_out_fname"
#define REG_VPP_NO_SW_FB   "MfxMftDec_no_sw_fallback"

/*------------------------------------------------------------------------------*/

// how much times try to find free surface
#define MF_FREE_SURF_WAIT_NUM 5
// EVR uses 3 surfaces for deinterlacing (see MS docs)
// this value should be at least 1
#define MF_RENDERING_SURF_NUM 4

// limitation on supported resolution (MPEG2 content only)
#define MF_DEC_MPEG2_PICTURE_COMPLEXITY_LIMIT (640*480)

// how long to sleep prior calling fake SampleAppeared
#define MF_DEC_FAKE_SAMPLE_APPEARED_SLEEP_TIME 50

/*------------------------------------------------------------------------------*/

enum MFDecState
{
    stDecoderNotCreated, stHeaderNotDecoded, stReady
};

/*------------------------------------------------------------------------------*/

class MFPluginVDec : public MFPlugin,
                     public MFDecoderParams,
                     public IMFSamplesPoolCallback
{
public:
    static HRESULT CreateInstance(REFIID iid,
                                  void **ppMFT,
                                  ClassRegData *pRegData);

    // IUnknown methods
    virtual STDMETHODIMP_(ULONG) AddRef(void);
    virtual STDMETHODIMP_(ULONG) Release(void);
    virtual STDMETHODIMP QueryInterface(REFIID iid, void** ppv);

    // IMFTransform methods
    virtual STDMETHODIMP GetAttributes          (IMFAttributes** pAttributes);

    virtual STDMETHODIMP GetInputAvailableType  (DWORD dwInputStreamID,
                                                 DWORD dwTypeIndex,
                                                 IMFMediaType** ppType);

    virtual STDMETHODIMP GetOutputAvailableType (DWORD dwOutputStreamID,
                                                 DWORD dwTypeIndex,
                                                 IMFMediaType** ppType);

    virtual STDMETHODIMP SetInputType           (DWORD dwInputStreamID,
                                                 IMFMediaType* pType,
                                                 DWORD dwFlags);

    virtual STDMETHODIMP SetOutputType          (DWORD dwOutputStreamID,
                                                 IMFMediaType* pType,
                                                 DWORD dwFlags);

    virtual STDMETHODIMP ProcessMessage         (MFT_MESSAGE_TYPE eMessage,
                                                 ULONG_PTR ulParam);

    virtual STDMETHODIMP ProcessInput           (DWORD dwInputStreamID,
                                                 IMFSample *pSample,
                                                 DWORD dwFlags);

    virtual STDMETHODIMP ProcessOutput          (DWORD dwFlags,
                                                 DWORD cOutputBufferCount,
                                                 MFT_OUTPUT_DATA_BUFFER* pOutputSamples,
                                                 DWORD* pdwStatus);

    // MFConfigureMfxCodec methods
    virtual LPWSTR GetCodecName(void){ return m_Reg.pPluginName; }

    // IMFSamplesPoolCallback methods
    virtual void SampleAppeared(bool bFakeSample, bool bReinitSample);

protected: // functions
    // Constructor is protected. Client should use static CreateInstance method.
    MFPluginVDec(HRESULT &hr, ClassRegData *pRegistrationData);

    // Destructor is protected. The object deletes itself when the reference count is zero.
    virtual ~MFPluginVDec(void);

    // Errors handling
    virtual void SetPlgError(HRESULT hr, mfxStatus sts = MFX_ERR_NONE);
    virtual void ResetPlgError(void);
    void HandlePlgError(MyAutoLock& lock, bool bCalledFromAsyncThread = false);
    bool ReturnPlgError(void);

    //Mfx Video session. implD3D is checked against flag MFX_IMPL_VIA_D3D11
    mfxStatus  InitMfxVideoSession(mfxIMPL implD3D);
    mfxStatus  CloseInitMfxVideoSession(mfxIMPL implD3D);
    mfxStatus  SetHandle(mfxHandleType handleType, bool bAllowCloseInit);

    // Initialization functions
    mfxStatus InitCodec (MyAutoLock& lock);
    mfxStatus ResetCodec(MyAutoLock& lock);
    void      CloseCodec(MyAutoLock& lock);
    mfxStatus InitFRA   (void);
    mfxStatus InitSRF   (MyAutoLock& lock);
    // Work functions
    HRESULT   DecodeFrame (MyAutoLock& lock, mfxStatus& sts);
    HRESULT   DecodeSample(MyAutoLock& lock, IMFSample* pSample, mfxStatus& sts);
    HRESULT   TryReinit   (MyAutoLock& lock, mfxStatus& sts, bool bReady);
    // Help functions
    bool      HandleDevBusy(mfxStatus& sts);
    HRESULT   SetFreeWorkSurface(MyAutoLock& lock);
    HRESULT   SetFreeOutSurface (MyAutoLock& lock);
    bool      CheckHwSupport(void);
    HRESULT   CheckInputMediaType(IMFMediaType* pType);
    mfxF64    GetCurrentFramerate(void);
    HRESULT   CopyAllAttributes(CComPtr<IMFAttributes> pSrc, CComPtr<IMFSample> pOutSample);
    mfxStatus TestDeviceAndReinit(MyAutoLock& lock);

    // Async thread functionality
    virtual HRESULT AsyncThreadFunc(void);

protected: // variables
    // Reference count
    long                   m_nRefCount;

    // Plug-in information
    const GUID_info*       m_pInputInfo;
    const GUID_info*       m_pOutputInfo;

    // MSDK components variables
    mfxIMPL                m_MSDK_impl;
    IMfxVideoSession*      m_pMfxVideoSession;
    MFXVideoDECODE*        m_pmfxDEC;

    // Work variables
    mfxVideoParam          m_VideoParams_input;
    mfxFrameInfo           m_FrameInfo_original;
    IMFMediaType*          m_pOutputTypeCandidate;  // Output media type candidate.

    mfxU32                 m_uiSurfacesNum;         // surf_num = work_surf_num >= out_surf_num
    MFYuvOutSurface**      m_pWorkSurfaces;
    MFYuvOutSurface*       m_pWorkSurface;          // pointer to current work surface
    MFYuvOutData*          m_pOutSurfaces;
    MFYuvOutData*          m_pOutSurface;           // pointer to current out surface
    MFYuvOutData*          m_pDispSurface;          // pointer to current display surface
    MFDecBitstream*        m_pMFBitstream;
    MFDecBitstream*        m_pReinitMFBitstream;
    mfxBitstream*          m_pBitstream;
    MFDecState             m_State;
    bool                   m_bSendDrainComplete;
    bool                   m_bEndOfInput;
    bool                   m_bNeedWorkSurface;
    bool                   m_bStartDrain;
    bool                   m_bChangeOutputType;
    bool                   m_bReinit;
    bool                   m_bReinitStarted;
    bool                   m_bSendFakeSrf;
    bool                   m_bOutputTypeChangeRequired;
    bool                   m_bSetDiscontinuityAttribute;
    MFSamplesPool*         m_pFreeSamplesPool;
    mfxU32                 m_uiHasOutputEventExists;
    IMFSample*             m_pPostponedInput;
    mfxU64                 m_LastPts;
    mfxF64                 m_LastFramerate;
    mfxStatus              m_SyncOpSts;
    MFT_MESSAGE_TYPE       m_eLastMessage;  //last processmessage type

    // HW support variables
    IMFDeviceDXVA*         m_pDeviceDXVA;           // vpp/enc connection interface

    MFTMemoryType          m_MemType;

    CComPtr<IUnknown>      m_pHWDevice;
    HANDLE                 m_deviceHandle;

    MFSampleExtradataTransport m_ExtradataTransport;

    MFFrameAllocator*      m_pFrameAllocator;
    mfxFrameAllocRequest   m_DecoderRequest;
    mfxFrameAllocResponse  m_DecAllocResponse;

    FILE*                  m_dbg_decin;
    FILE*                  m_dbg_decin_fc;
    FILE*                  m_dbg_decout;

    // Debug statistics variables
    int                    m_iNumberLockedSurfaces;
    int                    m_iNumberInputSurfaces;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFPluginVDec(const MFPluginVDec&);
    MFPluginVDec& operator=(const MFPluginVDec&);
};

#endif // #ifndef __MF_VDEC_PLG_H__
