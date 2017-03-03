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

File: mf_venc_plg.h

Purpose: define common code for MSDK based encoder MFTs.

Defined Types:
  * MFEncVideoFilter - [class] - MFT for MSDK encoders
  * MFEncState - [enum] - MFT states enum

Defined Macroses:
  * MF_BITSTREAMS_NUM - initial number of ouput bitstreams (samples)

Registry Properties:
  * MfxMftEnc_MSDK - [DWORD] - 0 = default behavior, 1 = use SW MediaSDK,
  2 = use HW MediaSDK
  * MfxMftEnc_Memory - [DWORD] -  0 = default behavior, 1 = use SW memory,
  2 = use HW memory
  * MfxMftEnc_vppin_fname - [REG_SZ] - file to store input frames
  * MfxMftEnc_vppout_fname - [REG_SZ] - file to store vpp output frames
  * MfxMftEnc_out_fname - [REG_SZ] - file to store output bitstream

*********************************************************************************/

#ifndef __MF_VENC_PLG_H__
#define __MF_VENC_PLG_H__

#include "mf_utils.h"
#include "mf_dxva_support.h"
#if MFX_D3D11_SUPPORT
#include "mf_dx11_support.h"
#endif
#include "mf_plugin.h"
#include "mf_yuv_ibuf.h"
#include "mf_venc_obuf.h"

#include "vpp_ex.h"
#include "temporal_scalablity.h"
#include "mf_rate_control_mode.h"
#include "mf_ltr.h"

/*------------------------------------------------------------------------------*/

#define REG_ENC_ASYNC       "MfxMftEnc_async"
#define REG_ENC_MSDK        "MfxMftEnc_MSDK"
#define REG_ENC_MEMORY      "MfxMftEnc_Memory"
#define REG_ENC_VPPIN_FILE  "MfxMftEnc_vppin_fname"
#define REG_ENC_VPPOUT_FILE "MfxMftEnc_vppout_fname"
#define REG_ENC_OUT_FILE    "MfxMftEnc_out_fname"

/*------------------------------------------------------------------------------*/

// number of additional input surfaces:
// = 1 - just to always have free surface
#define MF_ADDITIONAL_IN_SURF_NUM 1
// number of additional output surfaces
// = 1 - just to always have free surface
#define MF_ADDITIONAL_OUT_SURF_NUM 1
// multiplier for the minimum number of surfaces
#define MF_ADDITIONAL_SURF_MULTIPLIER 2

/*------------------------------------------------------------------------------*/

enum MFEncState
{
    stHeaderNotSet, stHeaderSetAwaiting, stHeaderSampleNotSent, stReady
};

/*------------------------------------------------------------------------------*/

enum MFEncDynamicConfigurationChange
{
    dccNotRequired,             // Initial, normal state.

    dccResetWoIdrRequired,      // Bitrate / BufferSize changed.

    dccTypeChangeRequired,      // InputType != OutputType. Need to request new SetOutputType by returning
                                // MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE / MF_E_TRANSFORM_STREAM_CHANGE
                                // from ProcessOutput, after returning cached frames.

    dccResetWithIdrRequired     // Both Input/Output types equally changed to new resolution.
};

/*------------------------------------------------------------------------------*/

class MFPluginVEnc : public MFPlugin,
                     public MFEncoderParams,
#if MFX_D3D11_SUPPORT
                     public MFDeviceD3D11
#else
                     public MFDeviceDXVA
#endif

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

    // ICodecAPI methods
    virtual STDMETHODIMP IsSupported (const GUID *Api);

    virtual STDMETHODIMP IsModifiable(const GUID *Api);

    virtual STDMETHODIMP GetParameterRange (const GUID *Api,
        VARIANT *ValueMin, VARIANT *ValueMax, VARIANT *SteppingDelta);

    virtual STDMETHODIMP GetParameterValues(const GUID *Api,
        VARIANT **Values, ULONG *ValuesCount);

    virtual STDMETHODIMP GetDefaultValue(const GUID *Api, VARIANT *Value);

    virtual STDMETHODIMP GetValue(const GUID *Api, VARIANT *Value);

    virtual STDMETHODIMP SetValue(const GUID *Api, VARIANT *Value);

    virtual STDMETHODIMP SetAllDefaults(void);

    // MFConfigureMfxCodec methods
    virtual LPWSTR GetCodecName(void){ return m_Reg.pPluginName; }

protected: // functions
    // Constructor is protected. Client should use static CreateInstance method.
    MFPluginVEnc(HRESULT &hr, ClassRegData *pRegistrationData);

    // Destructor is protected. The object deletes itself when the reference count is zero.
    virtual ~MFPluginVEnc(void);

    // Errors handling
    virtual void SetPlgError(HRESULT hr, mfxStatus sts = MFX_ERR_NONE);
    void HandlePlgError(MyAutoLock& lock, bool bCalledFromAsyncThread = false);
    bool ReturnPlgError(void);

    //Mfx Video session. implD3D is checked against flag MFX_IMPL_VIA_D3D11
    mfxStatus  InitMfxVideoSession(mfxIMPL implD3D);
    mfxStatus  CloseInitMfxVideoSession(mfxIMPL implD3D);
    //mfxStatus  SetHandle(mfxHandleType handleType, bool bAllowCloseInit);

#ifdef MFX_D3D11_SUPPORT
    CComPtr<ID3D11Device> GetD3D11Device();
#endif

    // Initialization functions
    mfxStatus  InitCodec(mfxU16  uiMemPattern = 0,
                         mfxU32* pSurfacesNum = NULL);
    void       CloseCodec(void);
    mfxStatus  ResetCodec(void);
    mfxStatus  InitFRA(void);
    mfxStatus  InitInSRF(mfxU32* pSurfacesNum);
    mfxStatus  InitVPP(mfxU32* pSurfacesNum);
    mfxStatus  InitENC(mfxU32* pSurfacesNum);
    mfxStatus  QueryENC(mfxVideoParam* pParam, mfxVideoParam* pCorrectedParams);
    bool       InternalValueEquals(const GUID *Api, VARIANT *Value);
    HRESULT    InternalSetDefaultValue(const GUID *Api);
    HRESULT    InternalSetDefaultRCParamIfApplicable(const GUID *Api);
    // Work functions
    HRESULT    ProcessFrame(mfxStatus& sts);
    HRESULT    ProcessFrameEnc(mfxStatus& sts);
    HRESULT    ProcessFrameVpp(mfxStatus& sts);
    HRESULT    TryReinit(mfxStatus& sts);
    // Help functions
    HRESULT    GetHeader(mfxBitstream* pBst);
    MFYuvInSurface* SetFreeSurface(MFYuvInSurface* pSurfaces,
                                   mfxU32 nSurfacesNum);

    bool SendDrainCompleteIfNeeded();

    //Stops processing of input, start returning cached frames
    void StopProcessingDuringDCC();

    //MFParamsMessagePeer::
    virtual HRESULT HandleMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender);

    //Makes MediaSDK to prepare for future configuration change:
    //this may include obtaining buffered frames
    HRESULT RequireDynamicConfigurationChange(MFEncDynamicConfigurationChange newDccState);

    mfxU32 CheckDccType(mfxVideoParam& oldParam, mfxVideoParam& newParam);

    //Should be called when there is no buffered frames inside MediaSDK
    //and it is ready to be restarted or closed/inited;
    HRESULT PerformDynamicConfigurationChange(void);

    //Dynamic resolution change require changing frame info (in case of Reset)
    //or reallocating buffers (in case of Close-Init)
    HRESULT ResizeSurfaces();

    HRESULT ResizeBitstreamBuffer();

    bool IsInvalidResolutionChange(IMFMediaType* pType1, IMFMediaType* pType2);

    HRESULT UpdateAVCRefListCtrl(mfxEncodeCtrl* &pEncodeCtrl);

    void TraceMediaType(mfxTraceLevel level, IMFMediaType* pType1) const;

    bool       CheckHwSupport(void);
    HRESULT    CheckInputMediaType(IMFMediaType* pType);
    mfxStatus  CheckMfxParams(void);

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
    MFXVideoENCODE*        m_pmfxENC;
    MFXVideoVPPEx*         m_pmfxVPP;

    // Work variables
    MFExtBufCollection     m_arrEncExtBuf;

    mfxVideoParam          m_VPPInitParams;
    mfxVideoParam          m_VPPInitParams_original;

    //saved version of m_MfxParamsVideo stored after SetInputType / SetOutputType and restored at CloseCodec
    //also copy is saved on PerformDynamicConfigurationChange before CloseCodec
    //to keep all changes introduced through ICodecAPI (and as a result adjusted on InitENC).
    mfxInfoMFX             m_ENCInitInfo_original;
    mfxU16                 m_AsyncDepth_original; //TODO: remove this workaround by keeping mfxVideoParam instead of mfxInfoMFX

    mfxFrameAllocResponse  m_VppAllocResponse;      // vpp alloc response
    mfxFrameAllocResponse  m_EncAllocResponse;      // enc alloc response

    MFEncState             m_State;
    bool                   m_TypeDynamicallyChanged;
    mfxStatus              m_LastSts;
    mfxStatus              m_SyncOpSts;
    MFT_MESSAGE_TYPE       m_eLastMessage;  //last processmessage type

    // VPP in surfaces
    mfxU16                 m_uiInSurfacesMemType;
    mfxU32                 m_nInSurfacesNum;
    MFYuvInSurface*        m_pInSurfaces;
    MFYuvInSurface*        m_pInSurface;             // pointer to current surface
    // VPP out surfaces
    mfxU16                 m_uiOutSurfacesMemType;
    mfxU32                 m_nOutSurfacesNum;
    MFYuvInSurface*        m_pOutSurfaces;
    MFYuvInSurface*        m_pOutSurface;            // pointer to current surface
    // encoded bitstreams
    // Stored in "circular buffer" with "one slot always open".
    // m_nOutBitstreamsNum - number of allocated elements. m_pOutBitstreams - allocated buffer.
    // m_pDispBitstream - "start"
    // m_pOutBitstream - next element after the "end", This is "open slot"

    MFEncDynamicConfigurationChange m_DccState;
    bool                   m_bRejectProcessInputWhileDCC; //Yet Another flag which blocks processing TODO: remove it once ProcessInput buffering is implemented
    //HCK VisVal 600-604 uses following approach to to dynamically change type:
    //DRAIN - END_STREAMING - END_OF_STREAM - DrainComplete - SetOutputType - SetInputType - START_STREAMING - START_OF_STREAM
    bool                   m_bTypeChangeAfterEndOfStream;

    mfxSyncPoint*          m_pSyncPoint;             // VPP sync point (is not used by default)

    bool                   m_bInitialized;
    bool                   m_bDirectConnectionMFX;
    bool                   m_bConnectedWithVpp;
    bool                   m_bEndOfInput;
    bool                   m_bNeedInSurface;
    bool                   m_bNeedOutSurface;

    bool                   m_bStartDrain;
    bool                   m_bStartDrainEnc;
    bool                   m_bSendDrainComplete;
    bool                   m_bReinit;

    IMFSample*             m_pFakeSample;

    MFOutputBitstream      m_OutputBitstream;

    TemporalScalablity     m_TemporalScalablity;
    MFRateControlMode      m_RateControlMode;
    std::auto_ptr<MFLongTermReference>    m_Ltr;
    //CComPtr<MFScalableVideoCoding> m_pSVC;

    FILE*                  m_dbg_vppin;
    FILE*                  m_dbg_vppout;

    // Debug statistics variables
    int                    m_iNumberInputSamplesBeforeVPP;
    int                    m_iNumberInputSamples;
    int                    m_iNumberOutputSamples;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFPluginVEnc(const MFPluginVEnc&);
    MFPluginVEnc& operator=(const MFPluginVEnc&);
};

#endif // #ifndef __MF_VENC_PLG_H__
