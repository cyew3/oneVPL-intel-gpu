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

File: mf_vpp_plg.h

Purpose: define common code for MSDK based VPP MFTs.

Defined Types:
  * MFPluginVpp - MFT for MSDK encoders

Defined Macroses:
  * MF_DEFAULT_FRAMERATE_NOM/MF_DEFAULT_FRAMERATE_DEN - default framerate (if was
  not set by the splitter)
  * MF_RENDERING_SURF_NUM - additional number of surfaces to use
  * MF_FREE_SURF_WAIT_NUM - number of ties in search of free work surface
  * MF_ADDITIONAL_IN_SURF_NUM - number of additional input surfaces
  * MF_ADDITIONAL_OUT_SURF_NUM - number of additional output surfaces
  * MF_ADDITIONAL_SURF_MULTIPLIER - multiplier for the minimum number of surfaces

Registry Properties:
  * MfxMftVpp_MSDK - [DWORD] - 0 = default behavior, 1 = use SW MediaSDK,
  2 = use HW MediaSDK
  * MfxMftVpp_Memory - [DWORD] -  0 = default behavior, 1 = use SW memory,
  2 = use HW memory
  * MfxMftVpp_in_fname - [REG_SZ] - file to store input frames
  * MfxMftVpp_out_fname - [REG_SZ] - file to store output frames

*********************************************************************************/

#ifndef __MF_VPP_PLG_H__
#define __MF_VPP_PLG_H__

#include "mf_utils.h"
#include "mf_dxva_support.h"
#include "mf_plugin.h"
#include "mf_yuv_ibuf.h"
#include "mf_yuv_obuf.h"

#include "vpp_ex.h"

/*------------------------------------------------------------------------------*/

#define REG_VPP_ASYNC     "MfxMftVpp_async"
#define REG_VPP_MSDK      "MfxMftVpp_MSDK"
#define REG_VPP_MEMORY    "MfxMftVpp_Memory"
#define REG_VPP_IN_FILE   "MfxMftVpp_in_fname"
#define REG_VPP_OUT_FILE  "MfxMftVpp_out_fname"
#define REG_VPP_NO_SW_FB  "MfxMftVpp_no_sw_fallback"

/*------------------------------------------------------------------------------*/

// how much times try to find free surface
#define MF_FREE_SURF_WAIT_NUM 0
// number of additional input surfaces:
// = 1 - just to always have free surface
#define MF_ADDITIONAL_IN_SURF_NUM 1
// number of additional output surfaces
// = 1 - just to always have free surface
#define MF_ADDITIONAL_OUT_SURF_NUM 1
// multiplier for the minimum number of surfaces
#define MF_ADDITIONAL_SURF_MULTIPLIER 2

// limitation on supported resolution (for H.264 content only)
#define MF_VPP_H264_PICTURE_COMPLEXITY_LIMIT (1280*720)

// how long to sleep prior calling fake SampleAppeared
#define MF_VPP_FAKE_SAMPLE_APPEARED_SLEEP_TIME 50

/*------------------------------------------------------------------------------*/

class MFPluginVpp : public MFPlugin,
                    public MFVppParams,
                    public MFDeviceDXVA,
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

    // IMFDeviceDXVA methods
    virtual mfxStatus InitPlg(IMfxVideoSession* pSession,
                              mfxVideoParam*    pDecVideoParams,
                              mfxU32*           pDecSurfacesNum);

    // MFConfigureMfxCodec methods
    virtual LPWSTR GetCodecName(void){ return m_Reg.pPluginName; }

    // IMFSamplesPoolCallback
    virtual void SampleAppeared(bool bFakeSample, bool bReinitSample);

protected: // functions
    // Constructor is protected. Client should use static CreateInstance method.
    MFPluginVpp(HRESULT &hr, ClassRegData *pRegistrationData);

    // Destructor is protected. The object deletes itself when the reference count is zero.
    virtual ~MFPluginVpp(void);

    // Errors handling
    virtual void SetPlgError(HRESULT hr, mfxStatus sts = MFX_ERR_NONE);
    virtual void ResetPlgError(void);
    void HandlePlgError(MyAutoLock& lock, bool bCalledFromAsyncThread = false);
    bool ReturnPlgError(void);

    // Initialization functions
    mfxStatus  InitCodec(MyAutoLock& lock,
                         mfxU16  uiDecMemPattern = 0,
                         mfxU32* pDecSurfacesNum = NULL);
    mfxStatus  ResetCodec(MyAutoLock& lock);
    void       CloseCodec(MyAutoLock& lock);
    mfxStatus  InitFRA(void);
    mfxStatus  InitInSRF (mfxU32* pDecSurfacesNum);
    mfxStatus  InitOutSRF(MyAutoLock& lock);
    mfxStatus  InitVPP(MyAutoLock& lock, mfxU32* pDecSurfacesNum);
    // Work functions
    HRESULT    ProcessFrame(MyAutoLock& lock, mfxStatus& sts);
    HRESULT    ProcessSample(MyAutoLock& lock, IMFSample* pSample, mfxStatus& sts);
    HRESULT    TryReinit   (MyAutoLock& lock, mfxStatus& sts, bool bReady);
    // Help functions
    bool       HandleDevBusy(mfxStatus& sts);
    MFYuvInSurface* SetFreeInSurface(MFYuvInSurface* pSurfaces, mfxU32 nSurfacesNum);
    HRESULT    SetFreeWorkSurface(MyAutoLock& lock);
    HRESULT    SetFreeOutSurface (MyAutoLock& lock);
    bool       CheckHwSupport(void);
    HRESULT    CheckInputMediaType(IMFMediaType* pType);
    HRESULT    FlushDelayedOutputs(void);

    // Async thread functionality
    virtual HRESULT AsyncThreadFunc(void);

protected: // variables
    // Reference count, critical section
    long                   m_nRefCount;

    // Plug-in information
    const GUID_info*       m_pInputInfo;
    const GUID_info*       m_pOutputInfo;

    // MSDK components variables
    mfxIMPL                m_MSDK_impl;
    IMfxVideoSession*      m_pMfxVideoSession;
    IMfxVideoSession*      m_pInternalMfxVideoSession;
    MFXVideoVPPEx*         m_pmfxVPP;

    // Work variables
    mfxVideoParam          m_MfxParamsVideo_original;

    IMFMediaType*          m_pOutputTypeCandidate;  // Output media type candidate.
    mfxF64                 m_Framerate;
    MFSamplesPool*         m_pFreeSamplesPool;
    mfxU32                 m_uiHasOutputEventExists;
    mfxU32                 m_uiDelayedOutputsNum;

    // VPP in surfaces
    mfxU16                 m_uiInSurfacesMemType;
    mfxU32                 m_nInSurfacesNum;
    MFYuvInSurface*        m_pInSurfaces;
    MFYuvInSurface*        m_pInSurface;            // pointer to current surface
    // VPP out surfaces
    mfxU16                 m_uiOutSurfacesMemType;
    mfxU32                 m_nOutSurfacesNum;
    MFYuvOutSurface**      m_pWorkSurfaces;
    MFYuvOutSurface*       m_pWorkSurface;          // pointer to current work surface
    MFYuvOutData*          m_pOutSurfaces;
    MFYuvOutData*          m_pOutSurface;           // pointer to current surface
    MFYuvOutData*          m_pDispSurface;          // pointer to current display surface

    // HW support variables
    IMFDeviceDXVA*         m_pDeviceDXVA;           // enc connection interface
    mfxFrameAllocResponse  m_VppInAllocResponse;    // vpp alloc response
    mfxFrameAllocResponse  m_VppOutAllocResponse;   // vpp alloc response

    mfxStatus              m_LastSts;
    mfxStatus              m_SyncOpSts;

    bool                   m_bInitialized;
    bool                   m_bDirectConnectionMFX;
    bool                   m_bChangeOutputType;
    bool                   m_bEndOfInput;
    bool                   m_bNeedInSurface;
    bool                   m_bNeedOutSurface;
    bool                   m_bStartDrain;
    bool                   m_bSendDrainComplete;
    bool                   m_bReinit;
    bool                   m_bReinitStarted;
    bool                   m_bSendFakeSrf;
    bool                   m_bSetDiscontinuityAttribute;

    IMFSample*             m_pFakeSample;
    IMFSample*             m_pPostponedInput;

    FILE*                  m_dbg_vppin;
    FILE*                  m_dbg_vppout;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFPluginVpp(const MFPluginVpp&);
    MFPluginVpp& operator=(const MFPluginVpp&);
};

#endif // #ifndef __MF_VPP_PLG_H__
