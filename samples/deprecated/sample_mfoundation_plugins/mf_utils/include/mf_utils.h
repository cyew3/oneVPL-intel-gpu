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

/********************************************************************************

File: mf_utils.h

Purpose: define utilities code for plug-ins support.

Defined Classes, Structures & Enumerations:
  * MyCritSec - wraps a critical section
  * MyAutoLock - provides automatic lock/unlock of a critical section
  * MyEvent - provides events suport
  * MySemaphore - provides semaphores support
  * MyThread - provides threads support
  * MFSamplesPool - provides pool for free samples
  * MFTicker - provides functions execition time measurement

Defined Types:
  * mf_tick - timer tick type
  * my_thread_callback - threads callback function

Defined Macroses:
  # Behavior
  * MF_MFX_IMPL - defines MSFK library to use (SW, HW, AUTO)
  * MF_ENABLE_PICTURE_COMPLEXITY_LIMITS - enables limits on supported features
  * MF_NO_SW_FALLBACK - disables SW fallback for HW MSDK library
  * MF_DEFAULT_FRAMERATE_NOM - default frame rate nomenator
  * MF_DEFAULT_FRAMERATE_DEN - default frame rate denominator
  # Debugging
  * DBG_NO/DBG_YES - debug switchers
  # MSDK switchers
  * MF_FORCE_SW_MSDK - force SW MSDK usage
  * MF_FORCE_HW_MSDK - force HW MSDK usage
  * MF_FORCE_SW_MEMORY - force SW memory usage (not always possible)
  * MF_FORCE_HW_MEMORY - force HW memory usage
  # Other defines
  * MAX - calculates max value
  * MF_TIME_STAMP_FREQUENCY - MSDK time stamp frequency
  * MF_TIME_STAMP_INVALID - sets invalid time stamp
  * REF2MFX_TIME - converts reference to MSDK time
  * MFX2REF_TIME - converts MSDK to reference time
  * SEC2REF_TIME - converts seconds to reference time
  * REF2SEC_TIME - converts reference time to seconds
  * CHECK_POINTER - checks pointer validity
  * CHECK_EXPRESSION - checks expression
  * CHECK_RESULT - checks result
  * ARRAY_SIZE - calculates array size
  * GET_TIME - calculates time from ticks and frequency
  * IS_MEM_IN - checks whether MSDK memory is "IN"
  * IS_MEM_SYSTEM - checks whether MSDK memory is "SYSTEM"
  * IS_MEM_VIDEO - checks whether MSDK memory is "HW VIDEO"
  * MEM_IN_TO_OUT - converts MSDK in memory to out
  * MEM_OUT_TO_IN - converts MSDK out memory to in
  * MEM_ALIGN - aligns memory to specified amount of bytes

Defined Global Variables:
  * m_gFrequency - CPU frequency
  * g_dbg_file - debug variable: file to store debug trace

Defined Global Functions:
  * mf_get_tick - gets timer tick
  * mf_get_frequency - gets CPU frequency
  * mf_get_cpu_num - gets number of CPUs
  * mf_get_color_format - gets color format code from FOURCC
  * mf_get_chroma_format - gets chroma format code from FOURCC
  * mf_ms2mfx_imode - converts MS imode to MSDK one
  * mf_mfx2ms_imode - converts MSDK imode to MS one
  * mf_mftype2mfx_frame_info - converts IMFMediaType to mfxFrameInfo
  * mf_mftype2mfx_info - converts IMFMediaType to mfxInfoMFX
  * mf_align_geometry - aligns geometry with according to MFX requirements
  * mf_set_cropping - sets cropping parameters for vpp
  * mf_dump_YUV_from_NV12_data - debug function: dumps YUV data to file
  * mf_create_reg_key - creates key in the registry
  * mf_delete_reg_key - deletes key from the registry
  * mf_delete_reg_value - deletes value from the registry
  * mf_create_reg_string - sets string to registry
  * mf_get_reg_string - gets string from registry
  * mf_get_reg_dword - gets DWORD form registry

*********************************************************************************/

#ifndef __MF_UTILS_H__
#define __MF_UTILS_H__

#include "mfxdefs.h"
#include "mfxstructures.h"
#include "mfxjpeg.h"

#include "mf_guids.h"

/*------------------------------------------------------------------------------*/

#if MFX_D3D11_SUPPORT
    //since ms decoder create surfaces with inappropriate bins flags we cannot pas this surface directly to decoder, we will use owr own surface and copysubresource
    #define MF_D3D11_COPYSURFACES
#endif
#define MF_MFX_IMPL MFX_IMPL_HARDWARE

//#define MF_ENABLE_PICTURE_COMPLEXITY_LIMITS
// NOTE: the following define should be enabled for product MFTs with HW only support
//#define MF_NO_SW_FALLBACK

/*------------------------------------------------------------------------------*/

#define MF_D3D_MUTEX "mfx_d3d_mutex"

// default framerate (if was not set by other MF component in Media Type)
#define MF_DEFAULT_FRAMERATE_NOM 25
#define MF_DEFAULT_FRAMERATE_DEN 1

/*------------------------------------------------------------------------------*/

#define DBG_NO  0
#define DBG_YES 1

#define USE_DBG_TOOLS DBG_NO

/*------------------------------------------------------------------------------*/

#ifndef MAX
#define MAX(A, B) (((A) > (B)) ? (A) : (B))
#endif

#ifndef MIN
#define MIN(A, B) (((A) < (B)) ? (A) : (B))
#endif

#define MF_TIME_STAMP_FREQUENCY 90000
#define MF_CPU_TIME_FREQUENCY   10000000
#define MF_TIME_STAMP_INVALID   ((mfxU64)-1.0)
#define MS_TIME_STAMP_INVALID   -10000000; //-1e7
#define MF_INVALID_IDX 0xFFFF

#ifndef REF2MFX_TIME
#define REF2MFX_TIME(REF_TIME) (-1e7 == (REF_TIME))? MF_TIME_STAMP_INVALID: (mfxU64)(((REF_TIME) / 1e7) * MF_TIME_STAMP_FREQUENCY)
#endif

#ifndef MFX2REF_TIME
#define MFX2REF_TIME(MFX_TIME) (REFERENCE_TIME)(((mfxF64)(MFX_TIME) / (mfxF64)MF_TIME_STAMP_FREQUENCY) * 1e7)
#endif

#ifndef SEC2REF_TIME
#define SEC2REF_TIME(SEC_TIME) (REFERENCE_TIME)((SEC_TIME)*1e7)
#endif

#ifndef REF2SEC_TIME
#define REF2SEC_TIME(REF_TIME) ((mfxF64)(REF_TIME)/(mfxF64)1e7)
#endif

#ifndef CHECK_POINTER
#define CHECK_POINTER(P, ERR) {if (!(P)) {MFX_LTRACE_S(MF_TL_MACROSES, "CHECK_POINTER: (" #P ") = nil; return " #ERR " : -"); return ERR;}}
#endif

#ifndef CHECK_EXPRESSION
#define CHECK_EXPRESSION(E, ERR) {if (!(E)) {MFX_LTRACE_S(MF_TL_MACROSES, "CHECK_EXPRESSION: (" #E ") != true; return " #ERR " : -"); return ERR;}}
#endif

#ifndef CHECK_RESULT
#define CHECK_RESULT(P, X, ERR) {if ((X) != (P)) {MFX_LTRACE_S(MF_TL_MACROSES, "CHECK_RESULT: (" #P ") = (" #X "); return " #ERR " : -"); return ERR;}}
#endif

#ifndef SET_HR_ERROR
#define SET_HR_ERROR(E, STS, ERR) {if (SUCCEEDED(STS) && !(E)) {MFX_LTRACE_S(MF_TL_MACROSES, "SET_ERROR: (" #E ") != true; " #STS " = " #ERR " : -"); (STS) = (ERR);}}
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(P) (sizeof(P)/sizeof(P[0]))
#endif

#ifndef GET_TIME
#define GET_TIME(T,S,F) ((mfxF64)(mfxI64)((T)-(S))/(mfxF64)(mfxI64)(F))
#endif

#ifndef MAKE_ULONG
#define MAKE_ULONG(lhs, rhs) (ULONG)(lhs) << 16 | (rhs & 0xFFFF)
#endif

/*------------------------------------------------------------------------------*/

template<class T> inline void Zero(T & obj)                { memset(&obj, 0, sizeof(obj)); }

/*------------------------------------------------------------------------------*/

#ifndef GET_CPU_TIME
#define GET_CPU_TIME(T,S,F) GET_TIME(T,S,F)
#endif

/*------------------------------------------------------------------------------*/

#define IS_MEM_IN(M) ((MFX_IOPATTERN_IN_VIDEO_MEMORY == (M)) || (MFX_IOPATTERN_IN_SYSTEM_MEMORY == (M)))
#define IS_MEM_OUT(M) ((MFX_IOPATTERN_OUT_VIDEO_MEMORY == (M)) || (MFX_IOPATTERN_OUT_SYSTEM_MEMORY == (M)))
#define IS_MEM_SYSTEM(M) ((MFX_IOPATTERN_IN_SYSTEM_MEMORY == (M)) || (MFX_IOPATTERN_OUT_SYSTEM_MEMORY == (M)))
#define IS_MEM_VIDEO(M)  ((MFX_IOPATTERN_IN_VIDEO_MEMORY == (M)) || (MFX_IOPATTERN_OUT_VIDEO_MEMORY == (M)))
#define MEM_IN_TO_OUT(M) \
    ((IS_MEM_IN(M))? \
        ((MFX_IOPATTERN_IN_VIDEO_MEMORY == (M))? \
            MFX_IOPATTERN_OUT_VIDEO_MEMORY: \
            MFX_IOPATTERN_OUT_SYSTEM_MEMORY) \
        : \
        (M))
#define MEM_OUT_TO_IN(M) \
    ((IS_MEM_OUT(M))? \
        ((MFX_IOPATTERN_OUT_VIDEO_MEMORY == (M))? \
            MFX_IOPATTERN_IN_VIDEO_MEMORY: \
            MFX_IOPATTERN_IN_SYSTEM_MEMORY) \
        : \
        (M))

#define MEM_ALIGN(X, N) (0 == ((N) & ((N)-1)))? (((X)+(N)-1) & (~((N)-1))): (X)

/*------------------------------------------------------------------------------*/

typedef LONGLONG mf_tick;
typedef mf_tick mf_cpu_tick;

extern mf_tick m_gFrequency;

extern mf_tick mf_get_tick(void);
extern mf_tick mf_get_frequency(void);
extern mfxU32  mf_get_cpu_num(void);
extern size_t  mf_get_mem_usage(void);
extern void    mf_get_cpu_usage(mf_cpu_tick* pTimeKernel, mf_cpu_tick* pTimeUser);

typedef struct _ValueItem
{
    mfxU32 ms_value;
    mfxU32 mfx_value;
} ValueItem;

const ValueItem ProfilesTbl[] =
{
    { eAVEncMPVProfile_unknown,     MFX_PROFILE_UNKNOWN },
    { eAVEncH264VProfile_unknown,   MFX_PROFILE_UNKNOWN },
    // MP profiles
    { eAVEncMPVProfile_Simple,      MFX_PROFILE_MPEG2_SIMPLE },
    { eAVEncMPVProfile_Main,        MFX_PROFILE_MPEG2_MAIN },
    { eAVEncMPVProfile_High,        MFX_PROFILE_MPEG2_HIGH },
    { eAVEncMPVProfile_422,         MFX_PROFILE_UNKNOWN },
    // H.264 profiles
    { eAVEncH264VProfile_Simple,    MFX_PROFILE_AVC_BASELINE }, // currently simple=base in MS
    { eAVEncH264VProfile_Base,      MFX_PROFILE_AVC_BASELINE},
    { eAVEncH264VProfile_Main,      MFX_PROFILE_AVC_MAIN},
    { eAVEncH264VProfile_High,      MFX_PROFILE_AVC_HIGH},
    { eAVEncH264VProfile_422,       MFX_PROFILE_UNKNOWN },
    { eAVEncH264VProfile_High10,    MFX_PROFILE_UNKNOWN },
    { eAVEncH264VProfile_444,       MFX_PROFILE_UNKNOWN },
    { eAVEncH264VProfile_Extended,  MFX_PROFILE_UNKNOWN },
    { eAVEncH264VProfile_ConstrainedBase, MFX_PROFILE_AVC_CONSTRAINED_BASELINE },
    { eAVEncH264VProfile_UCConstrainedHigh, MFX_PROFILE_AVC_CONSTRAINED_HIGH }
};

const ValueItem LevelsTbl[] =
{
    { 0,                        MFX_LEVEL_UNKNOWN },
    // MP levels
    { eAVEncMPVLevel_Low,       MFX_LEVEL_MPEG2_LOW },
    { eAVEncMPVLevel_Main,      MFX_LEVEL_MPEG2_MAIN },
    { eAVEncMPVLevel_High1440,  MFX_LEVEL_MPEG2_HIGH1440 },
    { eAVEncMPVLevel_High,      MFX_LEVEL_MPEG2_HIGH },
    // H.264 levels
    { eAVEncH264VLevel1,        MFX_LEVEL_AVC_1 },
    { eAVEncH264VLevel1_b,      MFX_LEVEL_AVC_1b },
    { eAVEncH264VLevel1_1,      MFX_LEVEL_AVC_11 },
    { eAVEncH264VLevel1_2,      MFX_LEVEL_AVC_12 },
    { eAVEncH264VLevel1_3,      MFX_LEVEL_AVC_13 },
    { eAVEncH264VLevel2,        MFX_LEVEL_AVC_2 },
    { eAVEncH264VLevel2_1,      MFX_LEVEL_AVC_21 },
    { eAVEncH264VLevel2_2,      MFX_LEVEL_AVC_22 },
    { eAVEncH264VLevel3,        MFX_LEVEL_AVC_3 },
    { eAVEncH264VLevel3_1,      MFX_LEVEL_AVC_31 },
    { eAVEncH264VLevel3_2,      MFX_LEVEL_AVC_32 },
    { eAVEncH264VLevel4,        MFX_LEVEL_AVC_4 },
    { eAVEncH264VLevel4_1,      MFX_LEVEL_AVC_41 },
    { eAVEncH264VLevel4_2,      MFX_LEVEL_AVC_42 },
    { eAVEncH264VLevel5,        MFX_LEVEL_AVC_5 },
    { eAVEncH264VLevel5_1,      MFX_LEVEL_AVC_51 },
    { eAVEncH264VLevel5_2,      MFX_LEVEL_AVC_52 }
};

enum MFTMemoryType
{
    MFT_MEM_UNSPECIFIED = 0,
    MFT_MEM_SW,
    MFT_MEM_DX9,
    MFT_MEM_DX11,
};

extern const mfxU32 Ms2MfxValue(ValueItem *table, mfxU32 table_size,
                                mfxU32 ms_value);

extern const mfxU32 Mfx2MsValue(ValueItem *table, mfxU32 table_size,
                                mfxU32 mfx_value);

#define MFX_2_MS_VALUE(table, mfx_value) \
    Mfx2MsValue((ValueItem*)(table), sizeof(table)/sizeof(ValueItem), mfx_value)

#define MS_2_MFX_VALUE(table, ms_value) \
    Ms2MfxValue((ValueItem*)(table), sizeof(table)/sizeof(ValueItem), ms_value)

extern mfxU32 mf_get_color_format (DWORD fourcc);
extern mfxU16 mf_get_chroma_format(DWORD fourcc);

extern mfxF64 mf_get_framerate(mfxU32 fr_n, mfxU32 fr_d);
extern bool mf_are_framerates_equal(mfxU32 fr1_n, mfxU32 fr1_d,
                                    mfxU32 fr2_n, mfxU32 fr2_d);

extern mfxU16 mf_ms2mfx_imode(UINT32 imode);
extern UINT32 mf_mfx2ms_imode(mfxU16 imode);

extern mfxU16 mf_ms_quality_percent2mfx_qp(ULONG nMfQualityPercent);

const mfxU16 MFX_RATECONTROL_UNKNOWN = 0;
extern mfxU16 mf_ms2mfx_rate_control(ULONG nMfRateControlMode);

const mfxU16 QUALITY_VS_SPEED_MIN = 0;
//const mfxU16 QUALITY_VS_SPEED_BEST_SPEED = 0;
const mfxU16 QUALITY_VS_SPEED_BALANCED = 33;
const mfxU16 QUALITY_VS_SPEED_BEST_QUALITY = 66;
const mfxU16 QUALITY_VS_SPEED_MAX = 100;

extern mfxU16 mf_ms_quality_vs_speed2mfx_target_usage(ULONG nMfQualityVsSpeed);

const mfxU16 MFX_GOPSIZE_INFINITE = 0xfff0ui16; //maximum value aligned to 16 to avoid issues with temporal scalability
const ULONG MF_GOPSIZE_INFINITE = 0xffffffffUL;

extern HRESULT mf_mftype2mfx_frame_info(IMFMediaType* pType, mfxFrameInfo* pMfxInfo);
extern HRESULT mf_mftype2mfx_info(IMFMediaType* pType, mfxInfoMFX* pMfxInfo);
extern HRESULT mf_align_geometry(mfxFrameInfo* pMfxInfo);

extern void mf_set_cropping(mfxInfoVPP* pVppInfo);

extern mfxStatus mf_copy_extbuf(mfxExtBuffer* pIn, mfxExtBuffer*& pOut);
extern mfxStatus mf_free_extbuf(mfxExtBuffer*& pBuf);

extern mfxStatus mf_alloc_same_extparam(mfxVideoParam* pIn, mfxVideoParam* pOut);
extern mfxStatus mf_copy_extparam(mfxVideoParam* pIn, mfxVideoParam* pOut);
extern mfxStatus mf_free_extparam(mfxVideoParam* pParam);

extern mfxStatus mf_compare_videoparam(mfxVideoParam* pIn, mfxVideoParam* pOut, bool bResult);

extern void mf_dump_YUV_from_NV12_data(FILE* f, mfxU8* buf, mfxFrameInfo* info, mfxU32 p);

extern HRESULT mf_create_reg_key(const TCHAR *pPath, const TCHAR* pKey);
extern HRESULT mf_delete_reg_key(const TCHAR *pPath, const TCHAR* pKey);
extern HRESULT mf_delete_reg_value(const TCHAR *pPath, const TCHAR* pName);
extern HRESULT mf_create_reg_string(const TCHAR *pPath, const TCHAR* pName,
                                    TCHAR* pValue);
extern HRESULT mf_get_reg_string(const TCHAR *pPath, const TCHAR* pName,
                                 TCHAR* pValue, UINT32 cValueMaxSize);
extern HRESULT mf_get_reg_dword(const TCHAR *pPath, const TCHAR* pName,
                                DWORD* pValue);

extern HRESULT mf_test_createdevice();

/*------------------------------------------------------------------------------*/

class MyCritSec
{
public:
    MyCritSec(void);
    ~MyCritSec(void);

    void Lock(void);
    void Unlock(void);
    int  Try(void);
private:
    CRITICAL_SECTION m_CritSec;
};

/*------------------------------------------------------------------------------*/

class MyAutoLock
{
public:
    MyAutoLock(MyCritSec& crit_sec);
    ~MyAutoLock(void);

    void Lock(void);
    void Unlock(void);
private:
    MyCritSec* m_pCritSec;
    bool       m_bLocked;
    // avoiding possible problems by defining operator= and copy constructor
    MyAutoLock(const MyAutoLock&);
    MyAutoLock& operator=(const MyAutoLock&);
};


/*------------------------------------------------------------------------------*/

class MyNamedMutex
{
public:
    MyNamedMutex(HRESULT &hr, TCHAR* name);
    ~MyNamedMutex(void);

    void Lock(void);
    void Unlock(void);
private:
    HANDLE m_mt_handle;
    // avoiding possible problems by defining operator= and copy constructor
    MyNamedMutex(const MyNamedMutex&);
    MyNamedMutex& operator=(const MyNamedMutex&);
};

/*------------------------------------------------------------------------------*/

class MyEvent
{
public:
    MyEvent(HRESULT &hr);
    ~MyEvent(void);

    void Signal(void);
    void Reset(void);
    void Wait(void);

    DWORD TimedWait(mfxU32 msec);

private:
    void* m_ev_handle;
    // avoiding possible problems by defining operator= and copy constructor
    MyEvent(const MyEvent&);
    MyEvent& operator=(const MyEvent&);
};

/*------------------------------------------------------------------------------*/

class MySemaphore
{
public:
    MySemaphore(HRESULT &hr, LONG count = 0);
    ~MySemaphore(void);

    void Post(void);
    void Wait(void);
    void Reset(void);

    DWORD TimedWait(mfxU32 msec);

private:
    void* m_sm_handle;
    // avoiding possible problems by defining operator= and copy constructor
    MySemaphore(const MySemaphore&);
    MySemaphore& operator=(const MySemaphore&);
};

/*------------------------------------------------------------------------------*/

#define MY_THREAD_CALLCONVENTION __stdcall
typedef unsigned int (MY_THREAD_CALLCONVENTION * my_thread_callback)(void*);

class MyThread
{
public:
    MyThread(HRESULT &hr, my_thread_callback func, void* arg);
    ~MyThread(void);

    void Wait(void);
private:
    void* m_th_handle;
    // avoiding possible problems by defining operator= and copy constructor
    MyThread(const MyThread&);
    MyThread& operator=(const MyThread&);
};

/*------------------------------------------------------------------------------*/

class IMFSamplesPoolCallback
{
public:
    virtual void SampleAppeared(bool bFakeSample, bool bReinitSample) = 0;
};

/*------------------------------------------------------------------------------*/

class MFSamplesPool: public IMFAsyncCallback
{
public:
    MFSamplesPool(void);
    virtual ~MFSamplesPool(void);

    // IUnknown methods
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv);

    // IMFAsyncCallback methods
    STDMETHODIMP GetParameters(DWORD *pdwFlags, DWORD *pdwQueue);
    STDMETHODIMP Invoke(IMFAsyncResult *pAsyncResult);

    // MFSamplesPool methods
    HRESULT Init(mfxU32 uiSamplesSize, IMFSamplesPoolCallback* pCallback = NULL);
    void    Close(void);
    // NOTE: do not call RemoveCallback inside plug-in critical section if
    // Invoke can be called during SetCallback execution
    void RemoveCallback(void);

    HRESULT    AddSample(IMFSample* pSample);
    IMFSample* GetSample(void);

protected:
    long        m_nRefCount;     // reference count
    MyCritSec   m_CritSec;
    MyCritSec   m_CallbackCritSec;

    bool        m_bInitialized;  // flag to indicate initialization
    mfxU32      m_uiSamplesNum;  // number of free samples
    mfxU32      m_uiSamplesSize; // size of samples array
    IMFSample** m_pSamples;      // list of free samples

    // optional callback interface
    IMFSamplesPoolCallback* m_pCallback;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFSamplesPool(const MFSamplesPool&);
    MFSamplesPool& operator=(const MFSamplesPool&);
};

/*------------------------------------------------------------------------------*/

class MFTicker
{
public:
    MFTicker(mf_tick* pTime)
    {
        m_StartTick = mf_get_tick();
        if (pTime) m_pTime = pTime;
        else m_pTime = NULL;
    }
    ~MFTicker(void)
    {
        if (m_pTime) *m_pTime += mf_get_tick() - m_StartTick;
    }
protected:
    mf_tick  m_StartTick;
    mf_tick* m_pTime;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFTicker(const MFTicker&);
    MFTicker& operator=(const MFTicker&);
};

/*------------------------------------------------------------------------------*/

class MFCpuUsager
{
public:
    MFCpuUsager(mf_tick* pTotalTime, mf_cpu_tick* pKernelTime, mf_cpu_tick* pUserTime,
                mfxF64* pCpuUsage = NULL);
    ~MFCpuUsager(void);

protected:
    mf_tick      m_StartTick;
    mf_cpu_tick  m_StartKernelTime;
    mf_cpu_tick  m_StartUserTime;

    mfxF64*      m_pCpuUsage;
    mf_tick*     m_pTotalTime;
    mf_cpu_tick* m_pKernelTime;
    mf_cpu_tick* m_pUserTime;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFCpuUsager(const MFCpuUsager&);
    MFCpuUsager& operator=(const MFCpuUsager&);
};

/*------------------------------------------------------------------------------*/

class MFDebugDump
{
public:
    MFDebugDump() : m_dbg_file(NULL)
    {
    }

    virtual ~MFDebugDump() {};

    void SetFile(FILE* file) { m_dbg_file = file; }

    void WriteSample(void* pBuffer, size_t nDataLength)
    {
        if (m_dbg_file)
        {
            fwrite(pBuffer, 1, nDataLength, m_dbg_file);
            fflush(m_dbg_file);
        }
    }

    void DumpYuvFromNv12Data(mfxU8* buf, mfxFrameInfo* pInfo, mfxU32 p)
    {
        if (m_dbg_file)
        {
            mf_dump_YUV_from_NV12_data(m_dbg_file, buf, pInfo, p);
        }
    }

    void DumpYuvFromYUY2Data(mfxFrameData * pData, mfxFrameInfo* pInfo)
    {
        if (m_dbg_file)
        {
            mfxU8* plane = pData->Y + pInfo->CropY * pData->Pitch + pInfo->CropX * 2;
            for (int i = 0; i <pInfo->CropH; i++)
            {
                fwrite(plane, 1, pInfo->CropW * 2, m_dbg_file);
                fflush(m_dbg_file);
                plane += pData->Pitch;
            }
        }
    }

    bool IsFileSet() const { return NULL != m_dbg_file; }
private:
    FILE* m_dbg_file;
};

/*------------------------------------------------------------------------------*/

#endif // #ifndef __MF_UTILS_H__
