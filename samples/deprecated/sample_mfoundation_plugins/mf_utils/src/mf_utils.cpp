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

#include "mf_utils.h"
#include "sample_utils.h"
#include "mf_param_brc_multiplier.h"

#undef  MFX_TRACE_CATEGORY
#define MFX_TRACE_CATEGORY _T("mfx_mft_unk")

/*------------------------------------------------------------------------------*/

mf_tick m_gFrequency = mf_get_frequency();

/*------------------------------------------------------------------------------*/

MyCritSec::MyCritSec(void)
{
    InitializeCriticalSection(&m_CritSec);
}

MyCritSec::~MyCritSec(void)
{
    DeleteCriticalSection(&m_CritSec);
}

void MyCritSec::Lock(void)
{
    EnterCriticalSection(&m_CritSec);
}

void MyCritSec::Unlock(void)
{
    LeaveCriticalSection(&m_CritSec);
}

int MyCritSec::Try(void)
{
    return TryEnterCriticalSection(&m_CritSec);
}

/*------------------------------------------------------------------------------*/

MyAutoLock::MyAutoLock(MyCritSec& crit_sec)
{
    m_pCritSec = &crit_sec;
    m_bLocked  = false;
    Lock();
}

MyAutoLock::~MyAutoLock(void)
{
    Unlock();
}

void MyAutoLock::Lock(void)
{
    if (!m_bLocked)
    {
        if (!m_pCritSec->Try())
        {
            MFX_AUTO_LTRACE(MF_TL_PERF, "Mutex");
            m_pCritSec->Lock();
        }
        m_bLocked = true;
    }
}

void MyAutoLock::Unlock(void)
{
    if (m_bLocked)
    {
        m_pCritSec->Unlock();
        m_bLocked = false;
    }
}

/*------------------------------------------------------------------------------*/

MyNamedMutex::MyNamedMutex(HRESULT &hr, TCHAR* name):
    m_mt_handle(NULL)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    hr = E_FAIL;
    m_mt_handle = CreateMutex(NULL, FALSE, name);
    if (m_mt_handle) hr = S_OK;
    MFX_LTRACE_D(MF_TL_PERF, hr);
}

MyNamedMutex::~MyNamedMutex(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    if (m_mt_handle) CloseHandle(m_mt_handle);
}

void MyNamedMutex::Lock(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    if (m_mt_handle) WaitForSingleObject(m_mt_handle, INFINITE);
}

void MyNamedMutex::Unlock(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    if (m_mt_handle) ReleaseMutex(m_mt_handle);
}

/*------------------------------------------------------------------------------*/

MyEvent::MyEvent(HRESULT &hr):
    m_ev_handle(NULL)
{
    hr = E_FAIL;
    m_ev_handle = CreateEvent(NULL, 0, 0, NULL);
    if (m_ev_handle) hr = S_OK;
}

MyEvent::~MyEvent(void)
{
    if (m_ev_handle) CloseHandle(m_ev_handle);
}

void MyEvent::Signal(void)
{
    if (m_ev_handle) SetEvent(m_ev_handle);
}

void MyEvent::Reset(void)
{
    if (m_ev_handle) ResetEvent(m_ev_handle);
}

void MyEvent::Wait(void)
{
    if (m_ev_handle) WaitForSingleObject(m_ev_handle, INFINITE);
}

DWORD MyEvent::TimedWait(mfxU32 msec)
{
    DWORD res = ERROR_SUCCESS;

    if (m_ev_handle) res = WaitForSingleObject(m_ev_handle, msec);
    return res;
}

/*------------------------------------------------------------------------------*/

MySemaphore::MySemaphore(HRESULT &hr, LONG count):
    m_sm_handle(NULL)
{
    hr = E_FAIL;
    m_sm_handle = CreateSemaphore(NULL, count, LONG_MAX, 0);
    if (m_sm_handle) hr = S_OK;
}

MySemaphore::~MySemaphore(void)
{
    if (m_sm_handle) CloseHandle(m_sm_handle);
}

void MySemaphore::Post(void)
{
    if (m_sm_handle) ReleaseSemaphore(m_sm_handle, 1, NULL);
}

void MySemaphore::Wait(void)
{
    if (m_sm_handle) WaitForSingleObject(m_sm_handle, INFINITE);
}

DWORD MySemaphore::TimedWait(mfxU32 msec)
{
    DWORD res = ERROR_SUCCESS;

    if (m_sm_handle) res = WaitForSingleObject(m_sm_handle, msec);
    return res;
}

void MySemaphore::Reset(void)
{
    if (m_sm_handle)
    {
        while (WAIT_TIMEOUT != WaitForSingleObject(m_sm_handle, 0)) ;
    }
}

/*------------------------------------------------------------------------------*/

MyThread::MyThread(HRESULT &hr, my_thread_callback func, void* arg):
    m_th_handle(NULL)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    hr = E_FAIL;
    m_th_handle = (void*)_beginthreadex(NULL, 0, func, arg, 0, NULL);
    if (m_th_handle) hr = S_OK;
    MFX_LTRACE_D(MF_TL_PERF, hr);
}

MyThread::~MyThread(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    if (m_th_handle) CloseHandle(m_th_handle);
}

void MyThread::Wait(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    if (m_th_handle) WaitForSingleObject(m_th_handle, INFINITE);
}

/*------------------------------------------------------------------------------*/
// CSamplesPool methods

MFSamplesPool::MFSamplesPool(void):
    m_nRefCount(0),
    m_bInitialized(false),
    m_uiSamplesNum(0),
    m_uiSamplesSize(0),
    m_pSamples(NULL),
    m_pCallback(NULL)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
}

/*------------------------------------------------------------------------------*/

MFSamplesPool::~MFSamplesPool(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    Close();
}

/*------------------------------------------------------------------------------*/
// IUnknown methods

ULONG MFSamplesPool::AddRef(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MFX_LTRACE_I(MF_TL_GENERAL, m_nRefCount+1);
    return InterlockedIncrement(&m_nRefCount);
}

ULONG MFSamplesPool::Release(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    MFX_LTRACE_I(MF_TL_GENERAL, uCount);
    if (uCount == 0) delete this;
    // For thread safety, return a temporary variable.
    return uCount;
}

HRESULT MFSamplesPool::QueryInterface(REFIID iid, void** ppv)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    if (!ppv) return E_POINTER;
    if ((iid == IID_IUnknown) || (iid == IID_IMFAsyncCallback))
    {
        MFX_LTRACE_S(MF_TL_GENERAL, "IMFAsyncCallback");
        *ppv = static_cast<IMFAsyncCallback*>(this);
    }
    else
    {
        MFX_LTRACE_GUID(MF_TL_GENERAL, iid);
        return E_NOINTERFACE;
    }
    AddRef();
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFSamplesPool::Init(mfxU32 uiSamplesSize,
                            IMFSamplesPoolCallback* pCallback)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);

    CHECK_EXPRESSION(!m_bInitialized, E_FAIL);
    CHECK_EXPRESSION(uiSamplesSize, E_FAIL);

    m_pSamples = (IMFSample**)calloc(uiSamplesSize,sizeof(IMFSample*));
    CHECK_POINTER(m_pSamples, E_OUTOFMEMORY);

    m_uiSamplesSize = uiSamplesSize;
    m_pCallback = pCallback;

    m_bInitialized = true;

    MFX_LTRACE_2(MF_TL_GENERAL, NULL, "m_uiSamplesNum = %d, m_uiSamplesSize = %d", m_uiSamplesNum, m_uiSamplesSize);
    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

void MFSamplesPool::Close(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock lock(m_CritSec);

    mfxU32 i = 0;

    if (!m_bInitialized) return;
    for (i = 0; i < m_uiSamplesNum; ++i)
    {
        SAFE_RELEASE(m_pSamples[i]);
    }
    m_uiSamplesNum = 0;
    SAFE_FREE(m_pSamples);
    m_uiSamplesSize = 0;
    m_pCallback = NULL;
    m_bInitialized = false;
}

/*------------------------------------------------------------------------------*/

void MFSamplesPool::RemoveCallback(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MyAutoLock callback_lock(m_CallbackCritSec);

    m_pCallback = NULL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFSamplesPool::AddSample(IMFSample* pSample)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MFX_LTRACE_P(MF_TL_PERF, pSample);
    HRESULT hr = S_OK;
    IMFTrackedSample* pTSample = NULL;

    CHECK_EXPRESSION(m_bInitialized, E_FAIL);
    CHECK_POINTER(pSample, E_POINTER);

    hr = pSample->QueryInterface(IID_IMFTrackedSample, (void**)&pTSample);
    if (SUCCEEDED(hr)) hr = pTSample->SetAllocator(this, NULL);
    SAFE_RELEASE(pTSample);
    MFX_LTRACE_2(MF_TL_PERF, NULL, "m_uiSamplesNum = %d, m_uiSamplesSize = %d", m_uiSamplesNum, m_uiSamplesSize);
    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

IMFSample* MFSamplesPool::GetSample(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    IMFSample* pSample = NULL;

    CHECK_EXPRESSION(m_bInitialized, NULL);
    CHECK_EXPRESSION(m_uiSamplesNum, NULL);
    --m_uiSamplesNum;
    pSample = m_pSamples[m_uiSamplesNum];
    m_pSamples[m_uiSamplesNum] = NULL;
    MFX_LTRACE_2(MF_TL_PERF, NULL, "m_uiSamplesNum = %d, m_uiSamplesSize = %d", m_uiSamplesNum, m_uiSamplesSize);
    MFX_LTRACE_P(MF_TL_PERF, pSample);
    return pSample;
}

/*------------------------------------------------------------------------------*/

HRESULT MFSamplesPool::GetParameters(DWORD* /*pdwFlags*/, DWORD* /*pdwQueue*/)
{
    return E_NOTIMPL;
}

/*------------------------------------------------------------------------------*/

HRESULT MFSamplesPool::Invoke(IMFAsyncResult *pAsyncResult)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    MyAutoLock lock(m_CritSec);
    MyAutoLock callback_lock(m_CallbackCritSec);
    HRESULT hr = S_OK;
    IMFSample*  pSample  = NULL;
    IMFSample** pSamples = NULL;
    IUnknown*   pUnk = NULL;
    IMFSamplesPoolCallback* pCallback = m_pCallback;
    bool bReinitSample = false;

    CHECK_EXPRESSION(m_bInitialized, S_OK);
    CHECK_POINTER(pAsyncResult, E_POINTER);

    hr = pAsyncResult->GetObject(&pUnk);
    if (SUCCEEDED(hr)) hr = pUnk->QueryInterface(IID_IMFSample, (void**)&pSample);
    if (SUCCEEDED(hr) && pSample)
    {
        UINT32 bFakeSample = FALSE;

        if (FAILED(pSample->GetUINT32(MF_MT_FAKE_SRF, &bFakeSample))) bFakeSample = FALSE;
        bReinitSample = (TRUE == bFakeSample)? true: false;

        if (m_uiSamplesSize == m_uiSamplesNum)
        {
            pSamples = (IMFSample**)realloc(m_pSamples, (m_uiSamplesSize+1)*sizeof(IMFSample*));
            if (!pSamples) hr = E_OUTOFMEMORY;
            else
            {
                m_pSamples = pSamples;
                m_pSamples[m_uiSamplesSize] = NULL;
                ++m_uiSamplesSize;
            }
        }
        if (SUCCEEDED(hr))
        {
            pSample->AddRef();
            m_pSamples[m_uiSamplesNum] = pSample;
            ++m_uiSamplesNum;
        }
        MFX_LTRACE_2(MF_TL_PERF, NULL, "m_uiSamplesNum = %d, m_uiSamplesSize = %d", m_uiSamplesNum, m_uiSamplesSize);
    }
    SAFE_RELEASE(pUnk);
    SAFE_RELEASE(pSample);

    lock.Unlock();
    if (SUCCEEDED(hr) && pCallback) pCallback->SampleAppeared(false, bReinitSample);

    MFX_LTRACE_D(MF_TL_PERF, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/
// MFCpuUsager

MFCpuUsager::MFCpuUsager(mf_tick*     pTotalTime,
                         mf_cpu_tick* pKernelTime,
                         mf_cpu_tick* pUserTime,
                         mfxF64*      pCpuUsage)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    m_StartTick = mf_get_tick();
    mf_get_cpu_usage(&m_StartKernelTime, &m_StartUserTime);

    if (pCpuUsage) m_pCpuUsage = pCpuUsage;
    else m_pCpuUsage = NULL;
    if (pTotalTime) m_pTotalTime = pTotalTime;
    else m_pTotalTime = NULL;
    if (pKernelTime) m_pKernelTime = pKernelTime;
    else m_pKernelTime = NULL;
    if (pUserTime) m_pUserTime = pUserTime;
    else m_pUserTime = NULL;
}

/*------------------------------------------------------------------------------*/

MFCpuUsager::~MFCpuUsager(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    mf_tick total_time;
    mf_cpu_tick kernel_time_cur, user_time_cur;
    mf_cpu_tick kernel_time, user_time;

    total_time = mf_get_tick() - m_StartTick;
    mf_get_cpu_usage(&kernel_time_cur, &user_time_cur);

    kernel_time = kernel_time_cur - m_StartKernelTime;
    user_time   = user_time_cur - m_StartUserTime;
    if (m_pTotalTime)  *m_pTotalTime  += total_time;
    if (m_pKernelTime) *m_pKernelTime += kernel_time;
    if (m_pUserTime)   *m_pUserTime   += user_time;
    // Cpu usage is calculated for the current period
    if (m_pCpuUsage) *m_pCpuUsage = 100.0*GET_CPU_TIME(kernel_time+user_time,0,MF_CPU_TIME_FREQUENCY) /
                                          (mf_get_cpu_num()*GET_TIME(total_time,0,m_gFrequency));
}

/*------------------------------------------------------------------------------*/

const mfxU32 Ms2MfxValue(ValueItem *table, mfxU32 table_size,
                                mfxU32 ms_value)
{
    mfxU32 i = 0;
    for (i = 0; i < table_size; ++i)
    {
        if (table[i].ms_value == ms_value) return table[i].mfx_value;
    }
    return 0;
}

/*------------------------------------------------------------------------------*/

const mfxU32 Mfx2MsValue(ValueItem *table, mfxU32 table_size,
                                mfxU32 mfx_value)
{
    mfxU32 i = 0;
    for (i = 0; i < table_size; ++i)
    {
        if (table[i].mfx_value == mfx_value) return table[i].ms_value;
    }
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxF64 mf_get_framerate(mfxU32 fr_n, mfxU32 fr_d)
{
    if (fr_n && fr_d) return (mfxF64)fr_n / (mfxF64)fr_d;
    return 0.0;
}

/*------------------------------------------------------------------------------*/

void FramerateNorm(mfxU32& fr_n, mfxU32& fr_d)
{
    if (!fr_n || !fr_d) return;

    mfxF64 f_fr = (mfxF64)fr_n / (mfxF64)fr_d;
    mfxU32 i_fr = (mfxU32)(f_fr + .5);

    if (fabs(i_fr - f_fr) < 0.0001)
    {
        fr_n = i_fr;
        fr_d = 1;
        return;
    }

    i_fr = (mfxU32)(f_fr * 1.001 + .5);
    if (fabs(i_fr * 1000 - f_fr * 1001) < 10)\
    {
        fr_n = i_fr * 1000;
        fr_d = 1001;
        return;
    }

    fr_n = (mfxU32)(f_fr * 10000 + .5);
    fr_d = 10000;
    return;
}

/*------------------------------------------------------------------------------*/

bool mf_are_framerates_equal(mfxU32 fr1_n, mfxU32 fr1_d,
                             mfxU32 fr2_n, mfxU32 fr2_d)
{
    FramerateNorm(fr1_n, fr1_d);
    FramerateNorm(fr2_n, fr2_d);
    return (fr1_n == fr2_n) && (fr1_d == fr2_d);
}

/*------------------------------------------------------------------------------*/

static const struct
{
    mfxU32 color_format;
    GUID   guid;
    mfxU32 fourcc;
    mfxU16 chroma_format;
}
color_formats[] =
{
    { MFX_FOURCC_NV12, MEDIASUBTYPE_NV12, MAKEFOURCC('N','V','1','2'), MFX_CHROMAFORMAT_YUV420 },
    { MFX_FOURCC_YV12, MEDIASUBTYPE_YV12, MAKEFOURCC('Y','V','1','2'), MFX_CHROMAFORMAT_YUV420 },
    { MFX_FOURCC_YUY2, MEDIASUBTYPE_YUY2, MAKEFOURCC('Y','U','Y','2'), MFX_CHROMAFORMAT_YUV422 },
};

mfxU32 mf_get_color_format(DWORD fourcc)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(color_formats); i++)
    {
        if (color_formats[i].fourcc == fourcc) return color_formats[i].color_format;
    }
    return 0;
}

mfxU16 mf_get_chroma_format(DWORD fourcc)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(color_formats); i++)
    {
        if (color_formats[i].fourcc == fourcc) return color_formats[i].chroma_format;
    }
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxU16 mf_ms2mfx_imode(UINT32 imode)
{
    mfxU16 mfx_imode = MFX_PICSTRUCT_UNKNOWN;
    switch(imode)
    {
    case MFVideoInterlace_Progressive:
    case MFVideoInterlace_FieldSingleUpper: // treating as progressive
    case MFVideoInterlace_FieldSingleLower: // treating as progressive
        mfx_imode = MFX_PICSTRUCT_PROGRESSIVE;
        break;
    case MFVideoInterlace_FieldInterleavedUpperFirst:
        mfx_imode = MFX_PICSTRUCT_FIELD_TFF;
        break;
    case MFVideoInterlace_FieldInterleavedLowerFirst:
        mfx_imode = MFX_PICSTRUCT_FIELD_BFF;
        break;
    case MFVideoInterlace_MixedInterlaceOrProgressive:
        mfx_imode = MFX_PICSTRUCT_UNKNOWN;
        break;
    }
    return mfx_imode;
}

/*------------------------------------------------------------------------------*/

UINT32 mf_mfx2ms_imode(mfxU16 imode)
{
    mfxU16 ms_imode = MFVideoInterlace_Unknown;
    switch(imode)
    {
    case MFX_PICSTRUCT_PROGRESSIVE:
        ms_imode = MFVideoInterlace_Progressive;
        break;
    case MFX_PICSTRUCT_FIELD_TFF:
        ms_imode = MFVideoInterlace_FieldInterleavedUpperFirst;
        break;
    case MFX_PICSTRUCT_FIELD_BFF:
        ms_imode = MFVideoInterlace_FieldInterleavedLowerFirst;
        break;
    case MFX_PICSTRUCT_UNKNOWN:
        ms_imode = MFVideoInterlace_MixedInterlaceOrProgressive;
        break;
    }
    return ms_imode;
}

/*------------------------------------------------------------------------------*/

// Media Foundation works with Quality in percents (0..100%)
// http://msdn.microsoft.com/en-us/library/dd317841%28v=VS.85%29.aspx
// But MSDK works with Quantization Parameter 1..51
// Below is conversion: MF Quality (0...100%) => MSDK Mfx QP (1..51)
mfxU16 mf_ms_quality_percent2mfx_qp(ULONG nMfQualityPercent)
{
    mfxU16 nMfxQp = 0;
    if (nMfQualityPercent <= 100)
        nMfxQp = (mfxU16) (44.3 - 0.28 * nMfQualityPercent + 0.5);
    return nMfxQp;
}

/*------------------------------------------------------------------------------*/

mfxU16 mf_ms2mfx_rate_control(ULONG nMfRateControlMode)
{
    mfxU16 nMfxRateControlMethod = MFX_RATECONTROL_UNKNOWN;
    switch (nMfRateControlMode)
    {
    case eAVEncCommonRateControlMode_CBR:
        nMfxRateControlMethod = MFX_RATECONTROL_CBR;
        break;
    case eAVEncCommonRateControlMode_PeakConstrainedVBR:
        nMfxRateControlMethod = MFX_RATECONTROL_VBR;
        break;
    case eAVEncCommonRateControlMode_UnconstrainedVBR:
        nMfxRateControlMethod = MFX_RATECONTROL_VBR;
        break;
    case eAVEncCommonRateControlMode_Quality:
        nMfxRateControlMethod = MFX_RATECONTROL_CQP;
        break;
    default:
        ATLASSERT(false);
    }
    return nMfxRateControlMethod;
}

/*------------------------------------------------------------------------------*/

mfxU16 mf_ms_quality_vs_speed2mfx_target_usage(ULONG nMfQualityVsSpeed)
{
    //TODO: map to 1...7, not to 1,4,7
    mfxU16 nMfxTargetUsage = MFX_TARGETUSAGE_UNKNOWN;
    if (QUALITY_VS_SPEED_MIN <= nMfQualityVsSpeed && nMfQualityVsSpeed < QUALITY_VS_SPEED_BALANCED)
        nMfxTargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
    else if (QUALITY_VS_SPEED_BALANCED <= nMfQualityVsSpeed && nMfQualityVsSpeed < QUALITY_VS_SPEED_BEST_QUALITY)
        nMfxTargetUsage = MFX_TARGETUSAGE_BALANCED;
    else if (QUALITY_VS_SPEED_BEST_QUALITY <= nMfQualityVsSpeed && nMfQualityVsSpeed <= QUALITY_VS_SPEED_MAX)
        nMfxTargetUsage = MFX_TARGETUSAGE_BEST_QUALITY;
    return nMfxTargetUsage;
}

/*------------------------------------------------------------------------------*/

HRESULT mf_mftype2mfx_frame_info(IMFMediaType* pType, mfxFrameInfo* pMfxInfo)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    UINT32 width = 0, height = 0;
    UINT32 framerate_nom = MF_DEFAULT_FRAMERATE_NOM;
    UINT32 framerate_den = MF_DEFAULT_FRAMERATE_DEN;
    UINT32 imode = 0;
    UINT32 aratio_nom = 0, aratio_den = 0;
    MFVideoArea area;
    GUID subtype = GUID_NULL;

    // checking errors
    CHECK_POINTER(pType, E_POINTER);
    CHECK_POINTER(pMfxInfo, E_POINTER);

    memset(&area, 0, sizeof(MFVideoArea));

    if (SUCCEEDED(hr) && SUCCEEDED(pType->GetGUID(MF_MT_SUBTYPE, &subtype)))
    {
        mfxU32 fourCC = mf_get_color_format (subtype.Data1);
        if (0 != fourCC)
        {
            pMfxInfo->FourCC = fourCC;
        }
        mfxU16 chromaFormat = mf_get_chroma_format(subtype.Data1);
        if (0 != chromaFormat)
        {
            pMfxInfo->ChromaFormat = chromaFormat;
        }
    }
    if (SUCCEEDED(hr) && SUCCEEDED(pType->GetUINT32(MF_MT_INTERLACE_MODE, &imode)))
    { // this parameter may absent on the given type
        pMfxInfo->PicStruct = mf_ms2mfx_imode(imode);
    }
    if (SUCCEEDED(hr) && SUCCEEDED(hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height)))
    { // this parameter should present on the given type
        pMfxInfo->Width  = (mfxU16)width;
        pMfxInfo->Height = (mfxU16)height;
        pMfxInfo->CropX  = 0;
        pMfxInfo->CropY  = 0;
        pMfxInfo->CropW  = (mfxU16)width;
        pMfxInfo->CropH  = (mfxU16)height;
    }
    if (SUCCEEDED(hr) && SUCCEEDED(pType->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&area, sizeof(MFVideoArea), NULL)))
    { // this parameter may absent on the given type
        pMfxInfo->CropX = (mfxU16)area.OffsetX.value;
        pMfxInfo->CropY = (mfxU16)area.OffsetY.value;
        pMfxInfo->CropW = (mfxU16)area.Area.cx;
        pMfxInfo->CropH = (mfxU16)area.Area.cy;
        MFX_LTRACE_I(MF_TL_GENERAL, pMfxInfo->CropX);
        MFX_LTRACE_I(MF_TL_GENERAL, pMfxInfo->CropY);
        MFX_LTRACE_I(MF_TL_GENERAL, pMfxInfo->CropW);
        MFX_LTRACE_I(MF_TL_GENERAL, pMfxInfo->CropH);
    }
    if (SUCCEEDED(hr))
    {
        if (FAILED(MFGetAttributeRatio(pType, MF_MT_FRAME_RATE, &framerate_nom, &framerate_den)) ||
            !framerate_nom || !framerate_den)
        { // if parameter is absent it will be corrected
            framerate_nom = MF_DEFAULT_FRAMERATE_NOM;
            framerate_den = MF_DEFAULT_FRAMERATE_DEN;
            // correcting this parameter on the given type
            hr = MFSetAttributeRatio(pType, MF_MT_FRAME_RATE, framerate_nom, framerate_den);
        }
        if (SUCCEEDED(hr))
        {
            pMfxInfo->FrameRateExtN = framerate_nom;
            pMfxInfo->FrameRateExtD = framerate_den;
        }
    }
    if (SUCCEEDED(hr) &&
        SUCCEEDED(MFGetAttributeRatio(pType, MF_MT_PIXEL_ASPECT_RATIO, &aratio_nom, &aratio_den)) &&
        aratio_nom && aratio_den)
    { // this parameter may absent on the given type
        pMfxInfo->AspectRatioW = (mfxU16)aratio_nom;
        pMfxInfo->AspectRatioH = (mfxU16)aratio_den;
    }
    else
    {
        pMfxInfo->AspectRatioW = 1u;
        pMfxInfo->AspectRatioH = 1u;
    }
    MFX_LTRACE_2(MF_TL_GENERAL, "frate = ", "%d:%d", framerate_nom, framerate_den);
    MFX_LTRACE_I(MF_TL_GENERAL, imode);
    MFX_LTRACE_2(MF_TL_GENERAL, "w x h  = ", "%d x %d", width, height);
    MFX_LTRACE_2(MF_TL_GENERAL, "aratio = ", "%d:%d", (aratio_nom)? aratio_nom: 1, (aratio_den)? aratio_den: 1);
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT mf_mftype2mfx_info(IMFMediaType* pType, mfxInfoMFX* pMfxInfo)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    UINT32 bitrate = 0;
    UINT32 profile = 0;
    UINT32 level = 0;

    // checking errors
    CHECK_POINTER(pType, E_POINTER);
    CHECK_POINTER(pMfxInfo, E_POINTER);

    hr = mf_mftype2mfx_frame_info(pType, &(pMfxInfo->FrameInfo));
    if (SUCCEEDED(hr) && SUCCEEDED(pType->GetUINT32(MF_MT_MPEG2_PROFILE, &profile)) && profile)
    {
        mfxU32 mfx_profile = MS_2_MFX_VALUE(ProfilesTbl, profile);
        if (MFX_PROFILE_UNKNOWN != mfx_profile)
        {
            pMfxInfo->CodecProfile = (mfxU16)mfx_profile;
#if MFX_D3D11_SUPPORT
#ifdef BYT_WA

            if (GetModuleHandle(L"MFCaptureEngine.dll") != NULL)
            {
                pMfxInfo->CodecProfile = MFX_PROFILE_AVC_BASELINE;
            }
#endif //#ifdef BYT_WA
#endif //#if MFX_D3D11_SUPPORT
            hr = S_OK;
        }
    }
    if (SUCCEEDED(hr) && SUCCEEDED(pType->GetUINT32(MF_MT_MPEG2_LEVEL, &level)) && level)
    {
        mfxU32 mfx_level = MS_2_MFX_VALUE(LevelsTbl, level);
        if (MFX_LEVEL_UNKNOWN != mfx_level)
        {
            pMfxInfo->CodecLevel = (mfxU16)mfx_level;
            hr = S_OK;
        }
    }

    switch ( pMfxInfo->RateControlMethod )
    {
    case MFX_RATECONTROL_CBR:
    case MFX_RATECONTROL_VBR:
    case MFX_RATECONTROL_AVBR:
        if (SUCCEEDED(hr) && SUCCEEDED(pType->GetUINT32(MF_MT_AVG_BITRATE, &bitrate)) && bitrate)
        {
            mfxU32 nBitrate = (mfxU32)ceil((mfxF32)bitrate/1000);
            MfxSetTargetKbps(*pMfxInfo, nBitrate);
        }
        MFX_LTRACE_I(MF_TL_GENERAL, bitrate);
        break;
    case MFX_RATECONTROL_CQP:
        break;
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT mf_align_geometry(mfxFrameInfo* pMfxInfo)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxU16 width = 0, height = 0;

    CHECK_POINTER(pMfxInfo, E_POINTER);

    width  = pMfxInfo->Width;
    height = pMfxInfo->Height;
    // making MFX geometry corrections
    pMfxInfo->Width  = (mfxU16)(((width+15)>>4)<<4);
    pMfxInfo->Height = (MFX_PICSTRUCT_PROGRESSIVE == pMfxInfo->PicStruct)?
                           (mfxU16)(((height + 15)>>4)<<4) :
                           (mfxU16)(((height + 31)>>5)<<5);

    MFX_LTRACE_S(MF_TL_GENERAL, "S_OK");
    return S_OK;
}

/*------------------------------------------------------------------------------*/

void mf_set_cropping(mfxInfoVPP* pVppInfo)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    if (!pVppInfo) return;

    if (pVppInfo->In.AspectRatioW &&
        pVppInfo->In.AspectRatioH &&
        pVppInfo->In.CropW &&
        pVppInfo->In.CropH &&
        pVppInfo->Out.AspectRatioW &&
        pVppInfo->Out.AspectRatioH &&
        pVppInfo->Out.CropW &&
        pVppInfo->Out.CropH)
    {
        mfxF64 dFrameAR         = ((mfxF64)pVppInfo->In.AspectRatioW * pVppInfo->In.CropW) /
                                   (mfxF64)pVppInfo->In.AspectRatioH /
                                   (mfxF64)pVppInfo->In.CropH;

        mfxF64 dPixelAR         = pVppInfo->Out.AspectRatioW / (mfxF64)pVppInfo->Out.AspectRatioH;

        mfxU16 dProportionalH   = (mfxU16)(pVppInfo->Out.CropW * dPixelAR / dFrameAR + 1) & -2; //round to closest odd (values are always positive)

        if (dProportionalH < pVppInfo->Out.CropH)
        {
            pVppInfo->Out.CropY = (mfxU16)((pVppInfo->Out.CropH - dProportionalH) / 2. + 1) & -2;
            pVppInfo->Out.CropH = pVppInfo->Out.CropH - 2 * pVppInfo->Out.CropY;
        }
        else if (dProportionalH > pVppInfo->Out.CropH)
        {
            mfxU16 dProportionalW = (mfxU16)(pVppInfo->Out.CropH * dFrameAR / dPixelAR + 1) & -2;

            pVppInfo->Out.CropX = (mfxU16)((pVppInfo->Out.CropW - dProportionalW) / 2 + 1) & -2;
            pVppInfo->Out.CropW = pVppInfo->Out.CropW - 2 * pVppInfo->Out.CropX;
        }
    }
}

/*------------------------------------------------------------------------------*/

mfxStatus mf_copy_extbuf(mfxExtBuffer* pIn, mfxExtBuffer*& pOut)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

    CHECK_POINTER(pIn, MFX_ERR_NULL_PTR);
    if (MFX_EXTBUFF_VPP_DONOTUSE == pIn->BufferId)
    {
        mfxExtVPPDoNotUse* pInVppExtBuf = (mfxExtVPPDoNotUse*)pIn;
        mfxExtVPPDoNotUse* pVppExtBuf = (mfxExtVPPDoNotUse*)calloc(1, sizeof(mfxExtVPPDoNotUse));
        mfxU32 num_alg = pInVppExtBuf->NumAlg;

        if (pVppExtBuf)
        {
            pVppExtBuf->Header.BufferId = MFX_EXTBUFF_VPP_DONOTUSE;
            pVppExtBuf->Header.BufferSz = sizeof(mfxExtVPPDoNotUse);
            pVppExtBuf->NumAlg  = num_alg;
            pVppExtBuf->AlgList = (mfxU32*)malloc(num_alg * sizeof(mfxU32));
            if (pVppExtBuf->AlgList)
            {
                memcpy_s(pVppExtBuf->AlgList, num_alg * sizeof(mfxU32), pInVppExtBuf->AlgList, num_alg * sizeof(mfxU32));
            }
            else sts = MFX_ERR_MEMORY_ALLOC;
        }
        else sts = MFX_ERR_MEMORY_ALLOC;
        if (MFX_ERR_NONE == sts) pOut = (mfxExtBuffer*)pVppExtBuf;
    }
    else if (pIn->BufferSz > 0)
    {
        pOut = (mfxExtBuffer*)calloc(1, pIn->BufferSz);
        if (pOut)
        {
            memcpy_s(pOut, pIn->BufferSz, pIn, pIn->BufferSz);
        }
        else sts = MFX_ERR_MEMORY_ALLOC;
    }
    else sts = MFX_ERR_UNSUPPORTED;
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxStatus mf_free_extbuf(mfxExtBuffer*& pBuf)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;

    CHECK_POINTER(pBuf, MFX_ERR_NONE);
    if (MFX_EXTBUFF_VPP_DONOTUSE == pBuf->BufferId)
    {
        mfxExtVPPDoNotUse* pVppBuf = (mfxExtVPPDoNotUse*)pBuf;

        SAFE_FREE(pVppBuf->AlgList);
        SAFE_FREE(pBuf);
    }
    else
    {
        SAFE_FREE(pBuf);
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxStatus mf_alloc_same_extparam(mfxVideoParam* pIn, mfxVideoParam* pOut)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 i = 0;

    CHECK_POINTER(pIn,  MFX_ERR_NONE);
    CHECK_POINTER(pOut, MFX_ERR_NONE);
    if (pIn->NumExtParam)
    {
        pOut->NumExtParam = pIn->NumExtParam;
        pOut->ExtParam = (mfxExtBuffer**)calloc(pIn->NumExtParam, sizeof(mfxExtBuffer*));
        if (pOut->ExtParam)
        {
            for (i = 0; i < pIn->NumExtParam; ++i)
            {
                if (NULL == pIn->ExtParam[i])
                {
                    sts = MFX_ERR_NULL_PTR;
                }
                else if (pIn->ExtParam[i]->BufferSz == 0)
                {
                    sts = MFX_ERR_UNSUPPORTED;
                }
                else
                {
                    pOut->ExtParam[i] = (mfxExtBuffer*)calloc(1, pIn->ExtParam[i]->BufferSz);
                    if (pOut->ExtParam[i])
                    {
                        memset(pOut->ExtParam[i], 0, pIn->ExtParam[i]->BufferSz);
                        memcpy_s(pOut->ExtParam[i], pIn->ExtParam[i]->BufferSz, pIn->ExtParam[i], sizeof(mfxExtBuffer));
                    }
                    else sts = MFX_ERR_MEMORY_ALLOC;
                }
                if (MFX_ERR_NONE != sts) break;
            }
        }
        else sts = MFX_ERR_MEMORY_ALLOC;
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/
mfxStatus mf_copy_extparam(mfxVideoParam* pIn, mfxVideoParam* pOut)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 i = 0;

    CHECK_POINTER(pIn,  MFX_ERR_NONE);
    CHECK_POINTER(pOut, MFX_ERR_NONE);
    if (pIn->NumExtParam)
    {
        pOut->NumExtParam = pIn->NumExtParam;
        pOut->ExtParam = (mfxExtBuffer**)calloc(pIn->NumExtParam, sizeof(mfxExtBuffer*));
        if (pOut->ExtParam)
        {
            for (i = 0; i < pIn->NumExtParam; ++i)
            {
                sts = mf_copy_extbuf(pIn->ExtParam[i], pOut->ExtParam[i]);
                if (MFX_ERR_NONE != sts) break;
            }
        }
        else sts = MFX_ERR_MEMORY_ALLOC;
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

mfxStatus mf_free_extparam(mfxVideoParam* pParam)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE, res = MFX_ERR_NONE;
    mfxU32 i = 0;

    CHECK_POINTER(pParam, MFX_ERR_NONE);
    if (pParam->NumExtParam)
    {
        for (i = 0; i < pParam->NumExtParam; ++i)
        {
            res = mf_free_extbuf(pParam->ExtParam[i]);
            if ((MFX_ERR_NONE == sts) && (MFX_ERR_NONE != res)) sts = res;
        }
        SAFE_FREE(pParam->ExtParam);
        pParam->NumExtParam = 0;
    }
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

extern mfxStatus mf_compare_videoparam(mfxVideoParam* pIn, mfxVideoParam* pOut, bool bResult)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 i = 0;

    CHECK_POINTER(pIn,  MFX_ERR_NULL_PTR);
    CHECK_POINTER(pOut, MFX_ERR_NULL_PTR);

    bResult = true;
    if (pIn->NumExtParam != pOut->NumExtParam)
    {
        bResult = false;
    }
    if (bResult)
    {
        size_t nSize = sizeof(*pIn)-sizeof(pIn->ExtParam)-sizeof(pIn->NumExtParam)-sizeof(pIn->reserved2);
        if (0 != memcmp(pIn, pOut, nSize))
        {
            bResult = false;
        }
    }
    if (bResult)
    {
        for (i = 0; i < pIn->NumExtParam; ++i)
        {
            CHECK_POINTER(pIn->ExtParam[i], MFX_ERR_NULL_PTR);
            mfxExtBuffer* pOutExtParam = GetExtBuffer(pOut->ExtParam, pOut->NumExtParam, pIn->ExtParam[i]->BufferId);
            if (NULL == pOutExtParam)
            {
                bResult = false;
                break;
            }
            else
            {
                if (pIn->ExtParam[i]->BufferSz != pOutExtParam->BufferSz ||
                    0 != memcmp(pIn->ExtParam[i], pOutExtParam, sizeof(*pOutExtParam)))
                {
                    bResult = false;
                    break;
                }
            }
        }
    }

    MFX_LTRACE_I(MF_TL_GENERAL, bResult);
    MFX_LTRACE_I(MF_TL_GENERAL, sts);
    return sts;
}

/*------------------------------------------------------------------------------*/

void mf_dump_YUV_from_NV12_data(FILE* f,
                                mfxU8* buf,
                                mfxFrameInfo* info,
                                mfxU32 p)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxU32 i = 0, j = 0;
    mfxU32 h = 0, cx = 0, cy = 0, cw = 0, ch = 0;

    if (f && buf && info)
    {
        h = info->Height;
        cx = info->CropX;
        cy = info->CropY;
        cw = info->CropW;
        ch = info->CropH;
        // dumping Y
        for (i = 0; i < ch; ++i)
        {
            fwrite(buf + cx + (cy + i)*p, 1, cw, f);
            fflush(f);
        }
        buf += p*h + cx + cy*p/2;
        // dumping U
        for (i = 0; i < ch/2; ++i)
        for (j = 0; j < cw/2; ++j)
        {
            fwrite(buf + p*i + 2*j, 1, 1, f);
        }
        fflush(f);
        buf += 1;
        // dumping V
        for (i = 0; i < ch/2; ++i)
        for (j = 0; j < cw/2; ++j)
        {
            fwrite(buf + p*i + 2*j, 1, 1, f);
        }
        fflush(f);
    }
}

/*------------------------------------------------------------------------------*/

mf_tick mf_get_frequency(void)
{
    LARGE_INTEGER t;

    QueryPerformanceFrequency(&t);
    return t.QuadPart;
}

/*------------------------------------------------------------------------------*/

mf_tick mf_get_tick(void)
{
    LARGE_INTEGER t;

    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

/*------------------------------------------------------------------------------*/

mfxU32 mf_get_cpu_num(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    SYSTEM_INFO s;

    memset(&s, 0, sizeof(SYSTEM_INFO));
    GetSystemInfo(&s);
    MFX_LTRACE_I(MF_TL_GENERAL, s.dwNumberOfProcessors);
    return s.dwNumberOfProcessors;
}

/*------------------------------------------------------------------------------*/

size_t mf_get_mem_usage(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    PROCESS_MEMORY_COUNTERS m;

    memset(&m, 0, sizeof(PROCESS_MEMORY_COUNTERS));
    {
        MFX_AUTO_LTRACE(MF_TL_GENERAL, "GetProcessMemoryInfo");
        GetProcessMemoryInfo(GetCurrentProcess(), &m, sizeof(PROCESS_MEMORY_COUNTERS));
    }
    return m.WorkingSetSize;
}

/*------------------------------------------------------------------------------*/

void mf_get_cpu_usage(mf_cpu_tick* pTimeKernel, mf_cpu_tick* pTimeUser)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
    HANDLE process = GetCurrentProcess();
    FILETIME ftime_start, ftime_exit, ftime_kernel, ftime_user;
    mf_cpu_tick time_kernel, time_user;

    if (GetProcessTimes(process, &ftime_start, &ftime_exit, &ftime_kernel, &ftime_user))
    {
        time_kernel = (mf_cpu_tick)((((ULONGLONG) ftime_kernel.dwHighDateTime) << 32) + ftime_kernel.dwLowDateTime);
        time_user   = (mf_cpu_tick)((((ULONGLONG) ftime_user.dwHighDateTime) << 32) + ftime_user.dwLowDateTime);

        if (pTimeKernel) *pTimeKernel = time_kernel;
        if (pTimeUser)   *pTimeUser   = time_user;
    }
    CloseHandle(process);
    return;
}

/*------------------------------------------------------------------------------*/

HRESULT mf_create_reg_key(const TCHAR *pPath, const TCHAR* pKey)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    HKEY key = NULL, subkey = NULL;
    LSTATUS res = ERROR_SUCCESS;

    if (SUCCEEDED(hr))
    {
        res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, pPath, 0, KEY_CREATE_SUB_KEY, &key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        LSTATUS res = RegCreateKeyEx(key, pKey,      // [in] parent key, name of subkey
            0, NULL,                                 // [in] reserved, class string (can be NULL)
            REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, // [in] options, access rights
            NULL,                                    // [in] security attributes
            &subkey,                                 // [out] handled of opened/created key
            NULL);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
        else RegCloseKey(subkey);
        RegCloseKey(key);
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT mf_delete_reg_key(const TCHAR *pPath, const TCHAR* pKey)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    HKEY key = NULL;
    LSTATUS res = ERROR_SUCCESS;

    if (SUCCEEDED(hr))
    {
        res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, pPath, 0, KEY_READ, &key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        LSTATUS res = RegDeleteKey(key, pKey);
        if (ERROR_FILE_NOT_FOUND == res) hr = ERROR_FILE_NOT_FOUND;
        else if (ERROR_SUCCESS != res) hr = E_FAIL;
        RegCloseKey(key);
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT mf_delete_reg_value(const TCHAR *pPath, const TCHAR* pName)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    HKEY key = NULL;
    LSTATUS res = ERROR_SUCCESS;

    if (SUCCEEDED(hr))
    {
        res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, pPath, 0, KEY_SET_VALUE, &key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        res = RegDeleteValue(key, pName);
        RegCloseKey(key);
        if (ERROR_FILE_NOT_FOUND == res) hr = ERROR_FILE_NOT_FOUND;
        else if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT mf_create_reg_string(const TCHAR *pPath, const TCHAR* pName, TCHAR* pValue)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    HKEY key = NULL;
    LSTATUS res = ERROR_SUCCESS;

    if (SUCCEEDED(hr))
    {
        res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, pPath, 0, KEY_SET_VALUE, &key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        res = RegSetValueEx(key, pName, 0, REG_SZ, (BYTE*)pValue, (DWORD)(_tcslen(pValue)+1)*sizeof(TCHAR));
        RegCloseKey(key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT mf_get_reg_string(const TCHAR *pPath, const TCHAR* pName,
                          TCHAR* pValue, UINT32 cValueMaxSize)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    HKEY key = NULL;
    LSTATUS res = ERROR_SUCCESS;
    DWORD size = cValueMaxSize*sizeof(TCHAR), type = 0;

    if (SUCCEEDED(hr))
    {
        res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, pPath, 0, KEY_QUERY_VALUE, &key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr))
    {
        res = RegQueryValueEx(key, pName, NULL, &type, (LPBYTE)pValue, &size);
        RegCloseKey(key);
        if (ERROR_SUCCESS != res) hr = E_FAIL;
    }
    if (SUCCEEDED(hr) && (REG_SZ != type)) hr = E_FAIL;
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT mf_get_reg_dword(const TCHAR *pPath, const TCHAR* pName, DWORD* pValue)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    HRESULT hr = S_OK;
    HKEY key = NULL;
    LSTATUS res = ERROR_SUCCESS;
    DWORD size = sizeof(DWORD), type = 0;

    if (SUCCEEDED(hr))
    {
        res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, pPath, 0, KEY_QUERY_VALUE, &key);
        if (ERROR_SUCCESS != res) {
            hr = E_FAIL;
            TCHAR *buffer;
            FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, res, LANG_SYSTEM_DEFAULT, (LPTSTR)&buffer, 1, NULL);
#ifdef UNICODE
            MFX_LTRACE_2(MF_TL_GENERAL, "RegOpenKeyEx at :", "%S , failed: %S", pPath, buffer);
#else
            MFX_LTRACE_2(MF_TL_GENERAL, "RegOpenKeyEx at :", "%s , failed: %s", pPath, buffer);
#endif
        }
    }
    if (SUCCEEDED(hr))
    {
        res = RegQueryValueEx(key, pName, NULL, &type, (LPBYTE)pValue, &size);
        RegCloseKey(key);
        if (ERROR_SUCCESS != res) {
            hr = E_FAIL;

            TCHAR *buffer;
            FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER, 0, res, LANG_SYSTEM_DEFAULT, (LPTSTR)&buffer, 1, NULL);
#ifdef UNICODE
            MFX_LTRACE_3(MF_TL_GENERAL, "RegQueryValueEx at :", "%S\\%S , failed: %S", pPath, pName, buffer);
#else
            MFX_LTRACE_3(MF_TL_GENERAL, "RegQueryValueEx at :", "%s\\%s , failed: %s", pPath, pName, buffer);
#endif
        }

    }
    if (SUCCEEDED(hr) && (REG_DWORD != type)) hr = E_FAIL;
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT mf_test_createdevice(void)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_PERF);
    HRESULT hr = E_FAIL;

    IDirect3D9Ex* pD3D;
    Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D);

    if (pD3D)
    {
        D3DPRESENT_PARAMETERS params;
        IDirect3DDevice9Ex* pDevice = NULL;
        POINT point = {0, 0};
        HWND  hWindow = 0;

        hWindow = WindowFromPoint(point);

        memset(&params, 0, sizeof(D3DPRESENT_PARAMETERS));

        params.BackBufferFormat           = D3DFMT_X8R8G8B8;
        params.BackBufferCount            = 1;
        params.SwapEffect                 = D3DSWAPEFFECT_DISCARD;
        params.hDeviceWindow              = hWindow;
        params.Windowed                   = true;
        params.Flags                      = D3DPRESENTFLAG_VIDEO |
                                            D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
        params.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
        params.PresentationInterval       = D3DPRESENT_INTERVAL_ONE;

        hr = pD3D->CreateDeviceEx(D3DADAPTER_DEFAULT,
                                D3DDEVTYPE_HAL,
                                hWindow,
                                D3DCREATE_SOFTWARE_VERTEXPROCESSING |
                                D3DCREATE_FPU_PRESERVE |
                                D3DCREATE_MULTITHREADED,
                                &params,
                                NULL,
                                &pDevice);

        SAFE_RELEASE(pDevice);
        SAFE_RELEASE(pD3D);
    }
    MFX_LTRACE_D(MF_TL_GENERAL, hr);
    return hr;
};