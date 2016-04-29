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

File: mf_plugin.h

Purpose: define base class for Intel MSDK based MediaFoundation MFTs.

Defined Types:
  * MFPlugin - [class] - base class for MSDK based MFTs
  * MFDbgMsdk - [enum] - enum to force SW/HW MediaSDK usage
  * MFDbgMemory - [emum] - enum to force SW/HW memory usage

Defined Macroses:
  * MF_DEV_BUSY_SLEEP_TIME - sleep time on device busy statuses
  * MF_DEFAULT_FRAMERATE_NOM/MF_DEFAULT_FRAMERATE_DEN - default framerate

Registry Properties:
  * MemAlignDec - [DWORD] - mem alignment for vpp & dec output surfaces;
  DWORD value treated as mfxU16
  * MemAlignVpp - [DWORD] - mem alignment for vpp & enc input surfaces;
  DWORD value treated as mfxU16
  * MemAlignEnc - [DWORD] - mem alignment for vpp-enc surfaces; DWORD value
  treated as mfxU16

Notes:
  MFPlugin class enables MFT which has the following properties:
  * MFT is ASync
  * MFT has 1 input and 1 output stream, number of streams can't be increased

*********************************************************************************/

#ifndef __MF_PLUGIN_H__
#define __MF_PLUGIN_H__

#include "mf_utils.h"
#include "mf_codec_params.h"
#include "mf_vbuf.h"

/*------------------------------------------------------------------------------*/

#define REG_ALIGN_DEC "MemAlignDec"
#define REG_ALIGN_VPP "MemAlignVpp"
#define REG_ALIGN_ENC "MemAlignEnc"

/*------------------------------------------------------------------------------*/

// how much to sleep (in milliseconds) on device busy status from MFX codecs
#define MF_DEV_BUSY_SLEEP_TIME 1

/*------------------------------------------------------------------------------*/

enum MFDbgMsdk { dbgDefMsdk = 0, dbgSwMsdk = 1, dbgHwMsdk = 2 };
enum MFDbgMemory { dbgDefMemory = 0, dbgSwMemory = 1, dbgHwMemory = 2 };

/*------------------------------------------------------------------------------*/

struct MFEventsInfo
{
    mfxU32 m_requested;
    mfxU32 m_sent;
};

/*------------------------------------------------------------------------------*/

class MFPlugin : public IMFTransform,
                 public IMFMediaEventGenerator,
                 public IMFShutdown

{
public:
    // IMFTransform methods
    virtual STDMETHODIMP GetStreamLimits            (DWORD* pdwInputMinimum,
                                                     DWORD* pdwInputMaximum,
                                                     DWORD* pdwOutputMinimum,
                                                     DWORD* pdwOutputMaximum);

    virtual STDMETHODIMP GetStreamCount             (DWORD* pcInputStreams,
                                                     DWORD* pcOutputStreams);

    virtual STDMETHODIMP GetStreamIDs               (DWORD dwInputIDArraySize,
                                                     DWORD* pdwInputIDs,
                                                     DWORD dwOutputIDArraySize,
                                                     DWORD* pdwOutputIDs);

    virtual STDMETHODIMP GetInputStreamInfo         (DWORD dwInputStreamID,
                                                     MFT_INPUT_STREAM_INFO* pStreamInfo);

    virtual STDMETHODIMP GetOutputStreamInfo        (DWORD dwOutputStreamID,
                                                     MFT_OUTPUT_STREAM_INFO* pStreamInfo);

    virtual STDMETHODIMP GetAttributes              (IMFAttributes** pAttributes);

    virtual STDMETHODIMP GetInputStreamAttributes   (DWORD dwInputStreamID,
                                                     IMFAttributes** ppAttributes);

    virtual STDMETHODIMP GetOutputStreamAttributes  (DWORD dwOutputStreamID,
                                                     IMFAttributes** ppAttributes);

    virtual STDMETHODIMP DeleteInputStream          (DWORD dwStreamID);

    virtual STDMETHODIMP AddInputStreams            (DWORD cStreams,
                                                     DWORD* adwStreamIDs);

    virtual STDMETHODIMP GetInputAvailableType      (DWORD dwInputStreamID,
                                                     DWORD dwTypeIndex,
                                                     IMFMediaType** ppType);

    virtual STDMETHODIMP GetOutputAvailableType     (DWORD dwOutputStreamID,
                                                     DWORD dwTypeIndex,
                                                     IMFMediaType** ppType);

    virtual STDMETHODIMP SetInputType               (DWORD dwInputStreamID,
                                                     IMFMediaType* pType,
                                                     DWORD dwFlags);

    virtual STDMETHODIMP SetOutputType              (DWORD dwOutputStreamID,
                                                     IMFMediaType* pType,
                                                     DWORD dwFlags);

    virtual STDMETHODIMP GetInputCurrentType        (DWORD dwInputStreamID,
                                                     IMFMediaType** ppType);

    virtual STDMETHODIMP GetOutputCurrentType       (DWORD dwOutputStreamID,
                                                     IMFMediaType** ppType);

    virtual STDMETHODIMP GetInputStatus             (DWORD dwInputStreamID,
                                                     DWORD* pdwFlags);

    virtual STDMETHODIMP GetOutputStatus            (DWORD *pdwFlags);

    virtual STDMETHODIMP SetOutputBounds            (LONGLONG hnsLowerBound,
                                                     LONGLONG hnsUpperBound);

    virtual STDMETHODIMP ProcessEvent               (DWORD dwInputStreamID,
                                                     IMFMediaEvent* pEvent);

    virtual STDMETHODIMP ProcessMessage             (MFT_MESSAGE_TYPE eMessage,
                                                     ULONG_PTR ulParam);

    virtual STDMETHODIMP ProcessInput               (DWORD dwInputStreamID,
                                                     IMFSample *pSample,
                                                     DWORD dwFlags);

    virtual STDMETHODIMP ProcessOutput              (DWORD dwFlags,
                                                     DWORD cOutputBufferCount,
                                                     MFT_OUTPUT_DATA_BUFFER* pOutputSamples,
                                                     DWORD* pdwStatus);

    // IMFMediaEventGenerator methods
    virtual STDMETHODIMP GetEvent(DWORD dwFlags,
                                  IMFMediaEvent **ppEvent);

    virtual STDMETHODIMP BeginGetEvent(IMFAsyncCallback *pCallback,
                                       IUnknown *pUnkState);

    virtual STDMETHODIMP EndGetEvent(IMFAsyncResult *pResult,
                                     IMFMediaEvent **ppEvent);

    virtual STDMETHODIMP QueueEvent(MediaEventType met,
                                    REFGUID guidExtendedType,
                                    HRESULT hrStatus,
                                    const PROPVARIANT *pvValue);

    // IMFShutdown methods
    virtual STDMETHODIMP Shutdown(void);

    virtual STDMETHODIMP GetShutdownStatus(MFSHUTDOWN_STATUS* pStatus);

protected: // functions
    // Constructor
    MFPlugin(HRESULT &hr, ClassRegData *pRegistrationData);
    virtual ~MFPlugin(void);

    // Errors handling
    virtual void SetPlgError(HRESULT hr, mfxStatus sts = MFX_ERR_NONE);
    virtual void ResetPlgError(void);

    // Functions which generates MFT events
    HRESULT RequestInput (void); // METransformNeedInput
    HRESULT SendOutput   (void); // METransformHaveOutput
    HRESULT DrainComplete(void); // METransformDrainComplete
    HRESULT SendMarker   (UINT64 ulParam); // METransformMarker
    HRESULT ReportError  (void); // MEError

    // Checks whether Asyn MFT was unlocked by the client
    bool IsMftUnlocked(void);

    // Checks whether media type is supported by plug-in
    HRESULT CheckMediaType(IMFMediaType*     pType,
                           const GUID_info*  pArray,
                           DWORD             arraySize,
                           const GUID_info** pEntry);
    void ReleaseMediaType(IMFMediaType*& pType);
    bool IsVppNeeded(IMFMediaType* pType, IMFMediaType* pCType);

    HRESULT IncrementEventsCount(IMFMediaEvent* pEvent);

    // Checks for HW support
    bool CheckHwSupport(void) { return true; }

    // Async thread functionality
    virtual HRESULT AsyncThreadFunc(void) { return S_OK; }
    friend unsigned int MY_THREAD_CALLCONVENTION thAsyncThreadFunc(void* arg);

    void AsyncThreadPush (void);
    void AsyncThreadWait (void);

    UINT GetAdapterNum(mfxIMPL* pImpl);

protected: // variables
    // Common critical section
    MyCritSec           m_CritSec;
    // System-wide mutex
    MyNamedMutex*       m_pD3DMutex;
    // Plug-in registration information
    ClassRegData        m_Reg;

    // Plug-in status variables
    IMFAttributes*      m_pAttributes;           // Plug-in attributes
    IPropertyStore*     m_pPropertyStore;
    bool                m_bUnlocked;             // Async MFT unlocked flag
    bool                m_bIsShutdown;           // Async MFT shutdown flag
    bool                m_bStreamingStarted;
    bool                m_bDoNotRequestInput;
    MFEventsInfo        m_NeedInputEventInfo;
    MFEventsInfo        m_HasOutputEventInfo;

    // Plug-in async thread variables
    MyThread*           m_pAsyncThread;
    MySemaphore*        m_pAsyncThreadSemaphore;
    MyEvent*            m_pAsyncThreadEvent;
    bool                m_bAsyncThreadStop;
    MyEvent*            m_pDevBusyEvent;
    MyEvent*            m_pErrorFoundEvent;
    MyEvent*            m_pErrorHandledEvent;

    // Event queue & async methods error codes
    IMFMediaEventQueue* m_pEventQueue;
    HRESULT             m_hrError;
    mfxU32              m_uiErrorResetCount;
    bool                m_bErrorHandlingStarted;
    bool                m_bErrorHandlingFinished;

    // Input & Output types
    IMFMediaType*       m_pInputType;
    IMFMediaType*       m_pOutputType;
    GUID                m_MajorType;

    // Debug variables
    MFDbgMsdk           m_dbg_MSDK;
    MFDbgMemory         m_dbg_Memory;
    bool                m_dbg_no_SW_fallback;
    bool                m_dbg_return_errors;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFPlugin(const MFPlugin&);
    MFPlugin& operator=(const MFPlugin&);
};

#endif // #ifndef __MF_PLUGIN_H__
