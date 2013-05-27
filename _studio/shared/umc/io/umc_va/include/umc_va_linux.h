/*
 *                  INTEL CORPORATION PROPRIETARY INFORMATION
 *     This software is supplied under the terms of a license agreement or
 *     nondisclosure agreement with Intel Corporation and may not be copied
 *     or disclosed except in accordance with the terms of that agreement.
 *          Copyright(c) 2006-2012 Intel Corporation. All Rights Reserved.
 *
 */

#ifndef __UMC_VA_LINUX_H__
#define __UMC_VA_LINUX_H__

#include "umc_va_base.h"

#ifdef UMC_VA_LINUX

#include "umc_mutex.h"
#include "umc_event.h"

namespace UMC
{

#define UMC_VA_LINUX_INDEX_UNDEF -1

/* VACompBuffer --------------------------------------------------------------*/

class VACompBuffer : public UMCVACompBuffer
{
public:
    // constructor
    VACompBuffer(void);
    // destructor
    virtual ~VACompBuffer(void);

    // UMCVACompBuffer methods
    virtual void SetNumOfItem(Ipp32s num) { m_NumOfItem = num; };

    // VACompBuffer methods
    virtual Status SetBufferInfo   (Ipp32s _type, Ipp32s _id, Ipp32s _index = -1);
    virtual Status SetDestroyStatus(bool _destroy);

    virtual Ipp32s GetIndex(void)    { return m_index; }
    virtual Ipp32s GetID(void)       { return m_id; }
    virtual Ipp32s GetNumOfItem(void){ return m_NumOfItem; }
    virtual bool   NeedDestroy(void) { return m_bDestroy; }
    virtual Status CreateFakeBuffer(void);
    virtual Status SwapBuffers(void);

protected:
    Ipp32s m_NumOfItem; //number of items in buffer
    Ipp32s m_index;
    Ipp32s m_id;
    bool   m_bDestroy;
    // debug bufferization
    Ipp8u  m_CompareByte;
    Ipp32u m_uiFakeBufferLeftSize;
    Ipp32u m_uiFakeBufferRightSize;
    Ipp32u m_uiFakeBufferSize;
    Ipp32u m_uiRealBufferSize;
    Ipp8u* m_pFakeBuffer; // fake buffer
    Ipp8u* m_pFakeData;   // requested buffer within fake one
    Ipp8u* m_pRealBuffer;
};

/* LinuxVideoAcceleratorParams -----------------------------------------------*/

class LinuxVideoAcceleratorParams : public VideoAcceleratorParams
{
    DYNAMIC_CAST_DECL(LinuxVideoAcceleratorParams, VideoAcceleratorParams);
public:
    LinuxVideoAcceleratorParams(void)
    {
        m_Display             = NULL;
        m_bCheckBuffers       = false;
        m_bComputeVAFncsInfo  = false;
    }
    VADisplay m_Display;
    bool      m_bCheckBuffers;
    bool      m_bComputeVAFncsInfo;
};

/* LinuxVideoAccelerator -----------------------------------------------------*/

enum lvaFrameState
{
    lvaBeforeBegin = 0,
    lvaBeforeEnd   = 1,
    lvaNeedUnmap   = 2
};

class LinuxVideoAccelerator : public VideoAccelerator
{
    DYNAMIC_CAST_DECL(LinuxVideoAccelerator, VideoAccelerator);
public:
    // constructor
    LinuxVideoAccelerator (void);
    // destructor
    virtual ~LinuxVideoAccelerator(void);

    // VideoAccelerator methods
    virtual Status Init         (VideoAcceleratorParams* pInfo);
    virtual Status Close        (void);
    virtual Status BeginFrame   (Ipp32s FrameBufIndex);
    // gets buffer from cache if it exists or HW otherwise, buffers will be released in EndFrame
    virtual void* GetCompBuffer(Ipp32s buffer_type, UMCVACompBuffer **buf, Ipp32s size, Ipp32s index);
    virtual Status Execute      (void);
    virtual Status EndFrame     (void*);
    virtual Status DisplayFrame (Ipp32s index, VideoData *pOutputVideoData);
    virtual Ipp32s GetSurfaceID (Ipp32s idx);

    // NOT implemented functions:
    virtual Status ReleaseBuffer(Ipp32s /*type*/)
    { return UMC_OK; };

    // LinuxVideoAccelerator methods
    virtual Ipp32s GetIndex (void);

    const UMCVAFncEntryInfo& GetVACreateBufferInfo()
    { return m_VACreateBufferInfo;}
    const UMCVAFncEntryInfo& GetVARenderPictureInfo()
    { return m_VARenderPictureInfo;}

    // Following functions are absent in menlow!!!!!!!!!!!!!!!!!!!!!!
    virtual Status FindConfiguration(UMC::VideoStreamInfo* /*x*/) { return UMC_ERR_UNSUPPORTED;}
    virtual Status ExecuteExtensionBuffer(void* /*x*/) { return UMC_ERR_UNSUPPORTED;}
    virtual Status ExecuteStatusReportBuffer(void* /*x*/, Ipp32s /*y*/)  { return UMC_ERR_UNSUPPORTED;}
    virtual bool IsIntelCustomGUID() const { return false;}
    virtual GUID GetDecoderGuid(){return m_guidDecoder;};


protected:
    // VideoAcceleratorExt methods
    virtual Status AllocCompBuffers(void);
    virtual VACompBuffer* GetCompBufferHW(Ipp32s type, Ipp32s size, Ipp32s index = -1);

protected:
    VADisplay     m_dpy;
    VAConfigID    m_config_id;
    VASurfaceID*  m_surfaces;
    VAContextID   m_context;
    Ipp32s        m_iIndex;
    lvaFrameState m_FrameState;

    Ipp32s   m_NumOfFrameBuffers;
    Ipp32u   m_uiCompBuffersNum;
    Ipp32u   m_uiCompBuffersUsed;
    vm_mutex m_SyncMutex;
    VACompBuffer** m_pCompBuffers;
    bool     m_bCheckBuffers;

    // NOT used variables:
    bool m_bLongSliceControl;
    UMCVAFncEntryInfo m_VACreateBufferInfo;
    UMCVAFncEntryInfo m_VARenderPictureInfo;

    // introduced for MediaSDK
    bool    m_bIsExtSurfaces;

    GUID m_guidDecoder;
};

}; // namespace UMC

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    extern UMC::Status va_to_umc_res(VAStatus va_res);

#ifdef __cplusplus
}
#endif // __cplusplus

#define UMC_VA_DBG_NONE 0
#define UMC_VA_DBG_VM   1
#define UMC_VA_DBG_IO   2
#define UMC_VA_DBG_FILE 3

#define UMC_VA_USE_DEBUG UMC_VA_DBG_NONE
#define UMC_VA_LOG_FILE  "umc_va_logfile.txt"

#if UMC_VA_USE_DEBUG == UMC_VA_DBG_NONE
    #define UMC_VA_DBG(STRING)
    #define UMC_VA_DBG_1(STRING, P1)
    #define UMC_VA_DBG_2(STRING, P1, P2)
    #define UMC_VA_DBG_3(STRING, P1, P2, P3)
#elif UMC_VA_USE_DEBUG == UMC_VA_DBG_VM
    #include <vm_debug.h>
    #define UMC_VA_DBG(STRING)               vm_debug_trace (VM_DEBUG_ALL, STRING)
    #define UMC_VA_DBG_1(STRING, P1)         vm_debug_trace1(VM_DEBUG_ALL, STRING, P1)
    #define UMC_VA_DBG_2(STRING, P1, P2)     vm_debug_trace2(VM_DEBUG_ALL, STRING, P1, P2)
    #define UMC_VA_DBG_3(STRING, P1, P2, P3) vm_debug_trace3(VM_DEBUG_ALL, STRING, P1, P2, P3)
#elif UMC_VA_USE_DEBUG == UMC_VA_DBG_IO
    #include <stdio.h>
    #define UMC_VA_DBG(STRING)               { printf(STRING);             fflush(NULL); }
    #define UMC_VA_DBG_1(STRING,P1)          { printf(STRING, P1);         fflush(NULL); }
    #define UMC_VA_DBG_2(STRING,P1,P2)       { printf(STRING, P1, P2);     fflush(NULL); }
    #define UMC_VA_DBG_3(STRING, P1, P2, P3) { printf(STRING, P1, P2, P3); fflush(NULL); }
#elif UMC_VA_USE_DEBUG == UMC_VA_DBG_FILE
    #define UMC_VA_DBG(STRING) \
    { \
        FILE *f = fopen(UMC_VA_LOG_FILE, "a"); \
        fprintf(f, STRING); fflush(f); \
        fclose(f); \
    }
    #define UMC_VA_DBG_1(STRING,P1) \
    { \
        FILE *f = fopen(UMC_VA_LOG_FILE, "a"); \
        fprintf(f, STRING, P1); fflush(f); \
        fclose(f); \
    }
    #define UMC_VA_DBG_2(STRING,P1,P2) \
    { \
        FILE *f = fopen(UMC_VA_LOG_FILE, "a"); \
        fprintf(f, STRING, P1, P2); fflush(f); \
        fclose(f); \
    }
    #define UMC_VA_DBG_3(STRING, P1, P2, P3) \
    { \
        FILE *f = fopen(UMC_VA_LOG_FILE, "a"); \
        fprintf(f, STRING, P1, P2, P3); fflush(f); \
        fclose(f); \
    }
#endif // #ifdef UMC_VA_USE_DEBUG

#endif // #ifdef UMC_VA_LINUX

#endif // #ifndef __UMC_VA_LINUX_H__
