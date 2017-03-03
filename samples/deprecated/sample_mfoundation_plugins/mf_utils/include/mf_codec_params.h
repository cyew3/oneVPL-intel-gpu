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

#ifndef __MF_CODEC_PARAMS_H__
#define __MF_CODEC_PARAMS_H__

#include "mf_mfx_params.h"

//#include "mf_mfx_params_interface.h"
#include "mf_param_nominal_range.h"
#include "mf_param_slice_control.h"
#include "mf_param_cabac.h"
#include "mf_param_max_mb_per_sec.h"

/*--------------------------------------------------------------------*/

#define REG_PARAMS_PATH "Software\\Intel\\Intel MSDK MF Plug-ins"
#define REG_INFO_FILE "MfxCodecInfo"

/*--------------------------------------------------------------------*/

class MFConfigureMfxCodec : public IConfigureMfxCodec, public MFParamsManager
{
public:
    // configure
    MFConfigureMfxCodec(void);
    ~MFConfigureMfxCodec(void);

    // IConfigureMfxCodec methods
    virtual STDMETHODIMP GetParams(mfxVideoParam *params);
    virtual STDMETHODIMP GetInfo(mfxCodecInfo *info);

    // MFConfigureMfxCodec methods
    virtual void SetStartTick(mf_tick tick) { m_ticks_LiveTimeStartTick = tick; }
    virtual LPWSTR GetCodecName(void) = 0;
    virtual void* GetID(void) { return this; };
    virtual void PrintInfo(void);

    virtual void UpdateTimes(void);

protected: // functions
    void GetPerformance(void);

protected: // variables
    bool          m_bVpp;
    mfxVideoParam m_MfxParamsVideo;
    mfxCodecInfo  m_MfxCodecInfo;

    // statistics
    // times
    MFTicker*     m_pWorkTicker;
    mf_tick       m_ticks_LiveTimeStartTick;
    mf_tick       m_ticks_WorkTime;
    mf_tick       m_ticks_ProcessInput;
    mf_tick       m_ticks_ProcessOutput;
#ifdef GET_MEM_USAGE
    // memory
    mfxF64        m_AvgMemUsage;
    size_t        m_MaxMemUsage;
    mfxU32        m_MemCount;
#endif //GET_MEM_USAGE
    // cpu usage
    MFCpuUsager*  m_pCpuUsager;
    mfxF64        m_CpuUsage;
    mf_tick       m_TimeTotal;
    mf_cpu_tick   m_TimeKernel;
    mf_cpu_tick   m_TimeUser;
    // file to drop statistics
    FILE*     m_pInfoFile;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFConfigureMfxCodec(const MFConfigureMfxCodec&);
    MFConfigureMfxCodec& operator=(const MFConfigureMfxCodec&);
};

/*--------------------------------------------------------------------*/

class MFEncoderParams : public ICodecAPI,
                        public MFConfigureMfxCodec
{
public:
    // constructor
    MFEncoderParams(void);

    virtual ~MFEncoderParams() {};

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

    virtual STDMETHODIMP RegisterForEvent(const GUID *Api, LONG_PTR userData);

    virtual STDMETHODIMP UnregisterForEvent(const GUID *Api);

    virtual STDMETHODIMP SetAllDefaults(void);

    virtual STDMETHODIMP SetValueWithNotify(const GUID *Api,
        VARIANT *Value, GUID **ChangedParam, ULONG *ChangedParamCount);

    virtual STDMETHODIMP SetAllDefaultsWithNotify(GUID **ChangedParam,
        ULONG *ChangedParamCount);

    virtual STDMETHODIMP GetAllSettings(IStream *pStream);

    virtual STDMETHODIMP SetAllSettings(IStream *pStream);

    virtual STDMETHODIMP SetAllSettingsWithNotify(IStream *pStream,
        GUID **ChangedParam, ULONG *ChangedParamCount);

    // IConfigureCodec methods
    virtual STDMETHODIMP SetParams(mfxVideoParam *pattern, mfxVideoParam *params);

    // MFConfigureMfxCodec methods
    virtual void PrintInfo(void);

//protected:
//    virtual bool       IsInitialized() const { return false; }
//    virtual bool       InternalValueEquals(const GUID *Api, VARIANT *Value) { return false; }
//    virtual HRESULT    InternalSetDefaultValue(const GUID *Api) { return E_NOTIMPL; }
//    virtual HRESULT    InternalSetDefaultRCParamIfApplicable(const GUID *Api) { return E_NOTIMPL; }
//
//    CComPtr<MFScalableVideoCoding> m_pSVC;
protected:
    STDMETHOD(SetType)(IMFMediaType* pType, bool bIn);
    std::auto_ptr<MFParamNominalRange> m_pNominalRange;
    std::auto_ptr<MFParamSliceControl> m_pSliceControl;
    std::auto_ptr<MFParamCabac> m_pCabac;
    std::auto_ptr<MFParamMaxMBperSec> m_pMaxMBperSec;
};

/*--------------------------------------------------------------------*/

class MFDecoderParams :
#if MFX_D3D11_SUPPORT
                        public IMFQualityAdvise,
                        public IMFRealTimeClientEx,
#endif
                        public MFConfigureMfxCodec
{
public:
    // constructor
    MFDecoderParams(void);

#if MFX_D3D11_SUPPORT
    // IMFQualityAdvise methods
    virtual STDMETHODIMP DropTime(LONGLONG hnsAmountToDrop);

    virtual STDMETHODIMP GetDropMode(MF_QUALITY_DROP_MODE *peDropMode);

    virtual STDMETHODIMP GetQualityLevel(MF_QUALITY_LEVEL *peQualityLevel);

    virtual STDMETHODIMP SetDropMode(MF_QUALITY_DROP_MODE eDropMode);

    virtual STDMETHODIMP SetQualityLevel(MF_QUALITY_LEVEL eQualityLevel);

    // IMFRealTimeClientEx methods
    virtual STDMETHODIMP RegisterThreadsEx(DWORD *pdwTaskIndex, LPCWSTR wszClassName, LONG lBasePriority);

    virtual STDMETHODIMP SetWorkQueueEx(DWORD dwMultithreadedWorkQueueId, LONG lWorkItemBasePriority);

    virtual STDMETHODIMP UnregisterThreads();
#endif

    // IConfigureCodec methods
    virtual STDMETHODIMP SetParams(mfxVideoParam *pattern, mfxVideoParam *params);

    // MFConfigureMfxCodec methods
    virtual void PrintInfo(void);
};

/*--------------------------------------------------------------------*/

class MFVppParams : public MFConfigureMfxCodec
{
public:
    // constructor
    MFVppParams(void);

    // IConfigureCodec methods
    virtual STDMETHODIMP SetParams(mfxVideoParam *pattern, mfxVideoParam *params);

    // MFConfigureMfxCodec methods
    virtual void PrintInfo(void);
};

#endif // #ifndef __MF_CODEC_PARAMS_H__
