//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_VA_BASE_H__
#define __UMC_VA_BASE_H__

#include <stdio.h>
#include <vector>
#include "vm_types.h"
#include "vm_debug.h"
#include "vm_time.h"
#include "ipps.h"

#if defined (UMC_VA) || defined (MFX_VA)

#ifndef UMC_VA
#   define UMC_VA
#endif

#ifndef MFX_VA
#   define MFX_VA
#endif

#if defined(LINUX32) || defined(LINUX64) || defined(LINUX_VA_EMULATION)
#   ifndef UMC_VA_LINUX
#       define UMC_VA_LINUX          // HW acceleration through Linux VA
#   endif
#   ifndef SYNCHRONIZATION_BY_VA_SYNC_SURFACE
#       ifdef ANDROID
#           define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
#       else
#           define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
#       endif
#   endif
#elif defined(_WIN32) || defined(_WIN64)
#   ifndef UMC_VA_DXVA
#       define UMC_VA_DXVA           // HW acceleration through DXVA
#   endif
#elif defined(__APPLE__)
#   ifndef UMC_VA_OSX
#      define UMC_VA_OSX
#   endif
#else
    #error unsupported platform
#endif

#endif //  defined (MFX_VA) || defined (UMC_VA)

#include "ipps.h"
#include "vm_types.h"

#ifdef  __cplusplus
#include "umc_structures.h"
#include "umc_dynamic_cast.h"

#pragma warning(disable : 4201)

#ifdef UMC_VA_LINUX
#include <va/va.h>
#include <va/va_dec_vp8.h>
#include <va/va_vpp.h>
#include <va/va_dec_vp9.h>
#include <va/va_dec_hevc.h>

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(p) (p);
#endif
#endif


#ifdef UMC_VA_DXVA
#include <windows.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <initguid.h>
#endif

#ifdef UMC_VA_DXVA
#include <d3d9.h>
#include <dxva2api.h>
#include <dxva.h>
#endif


#if defined(LINUX32) || defined(LINUX64) || defined(__APPLE__)
#ifndef GUID_TYPE_DEFINED
typedef struct {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;
#define GUID_TYPE_DEFINED
#endif
#endif


namespace UMC
{

#define VA_COMBINATIONS(CODEC) \
    CODEC##_VLD     = VA_##CODEC | VA_VLD

enum VideoAccelerationProfile
{
    UNKNOWN         = 0,

    // Codecs
    VA_CODEC        = 0x00ff,
    VA_MPEG2        = 0x0001,
    VA_H264         = 0x0003,
    VA_VC1          = 0x0004,
    VA_JPEG         = 0x0005,
    VA_VP8          = 0x0006,
    VA_H265         = 0x0007,
    VA_VP9          = 0x0008,

    // Entry points
    VA_ENTRY_POINT  = 0xfff00,
    VA_VLD          = 0x00400,

    VA_PROFILE                  = 0xff000,
    VA_PROFILE_SVC_HIGH         = 0x02000,
    VA_PROFILE_SVC_BASELINE     = 0x03000,
    VA_PROFILE_MVC              = 0x04000,
    VA_PROFILE_MVC_MV           = 0x05000,
    VA_PROFILE_MVC_STEREO       = 0x06000,
    VA_PROFILE_MVC_STEREO_PROG  = 0x07000,
    VA_PROFILE_INTEL            = 0x08000,
    VA_PROFILE_WIDEVINE         = 0x09000,
    VA_PROFILE_422              = 0x0a000,
    VA_PROFILE_444              = 0x0b000,

    //profile amendments
    VA_PROFILE_10               = 0x10000,

    // configurations
    VA_CONFIGURATION            = 0x0ff00000,
    VA_LONG_SLICE_MODE          = 0x00100000,
    VA_SHORT_SLICE_MODE         = 0x00200000,
    VA_ANY_SLICE_MODE           = 0x00300000,

    MPEG2_VLD       = VA_MPEG2 | VA_VLD,
    H264_VLD        = VA_H264 | VA_VLD,
    H265_VLD        = VA_H265 | VA_VLD,
    VC1_VLD         = VA_VC1 | VA_VLD,
    JPEG_VLD        = VA_JPEG | VA_VLD,
    VP8_VLD         = VA_VP8 | VA_VLD,
    HEVC_VLD        = VA_H265 | VA_VLD,
    VP9_VLD         = VA_VP9 | VA_VLD,

    H264_VLD_MVC            = VA_H264 | VA_VLD | VA_PROFILE_MVC,
    H264_VLD_SVC_BASELINE   = VA_H264 | VA_VLD | VA_PROFILE_SVC_BASELINE,
    H264_VLD_SVC_HIGH       = VA_H264 | VA_VLD | VA_PROFILE_SVC_HIGH,

    H264_VLD_MVC_MULTIVIEW      = VA_H264 | VA_VLD | VA_PROFILE_MVC_MV,
    H264_VLD_MVC_STEREO         = VA_H264 | VA_VLD | VA_PROFILE_MVC_STEREO,
    H264_VLD_MVC_STEREO_PROG    = VA_H264 | VA_VLD | VA_PROFILE_MVC_STEREO_PROG,

    H265_VLD_422       = VA_H265 | VA_VLD | VA_PROFILE_422,
    H265_VLD_444       = VA_H265 | VA_VLD | VA_PROFILE_444,
    H265_10_VLD        = VA_H265 | VA_VLD | VA_PROFILE_10,
    H265_10_VLD_422    = VA_H265 | VA_VLD | VA_PROFILE_10 | VA_PROFILE_422,
    H265_10_VLD_444    = VA_H265 | VA_VLD | VA_PROFILE_10 | VA_PROFILE_444,

    VP9_VLD_422    = VA_VP9 | VA_VLD | VA_PROFILE_422,
    VP9_VLD_444    = VA_VP9 | VA_VLD | VA_PROFILE_444,
    VP9_10_VLD     = VA_VP9 | VA_VLD | VA_PROFILE_10,
    VP9_10_VLD_422 = VA_VP9 | VA_VLD | VA_PROFILE_10 | VA_PROFILE_422,
    VP9_10_VLD_444 = VA_VP9 | VA_VLD | VA_PROFILE_10 | VA_PROFILE_444,
};

#define MAX_BUFFER_TYPES    32
enum VideoAccelerationPlatform
{
    VA_UNKNOWN_PLATFORM = 0,

    VA_PLATFORM  = 0x0f0000,
    VA_DXVA1     = 0x010000,
    VA_DXVA2     = 0x020000,
    VA_LINUX     = 0x030000,
    VA_SOFTWARE  = 0x040000,
};

enum VideoAccelerationHW
{
    VA_HW_UNKNOWN   = 0,
    VA_HW_LAKE      = 0x010000,
    VA_HW_LRB       = 0x020000,
    VA_HW_SNB       = 0x030000,

    VA_HW_IVB       = 0x040000,

    VA_HW_HSW       = 0x050000,
    VA_HW_HSW_ULT   = 0x050001,

    VA_HW_VLV       = 0x060000,

    VA_HW_BDW       = 0x070000,

    VA_HW_CHV       = 0x080000,

    VA_HW_SCL       = 0x090000,

    VA_HW_KBL       = 0x100000,

    VA_HW_BXT       = 0x110000,

    VA_HW_CNL       = 0x120000,

    VA_HW_SOFIA     = 0x130000,

    VA_HW_ICL       = 0x140000,
    VA_HW_ICL_LP    = VA_HW_ICL + 1,
};

class UMCVACompBuffer;
class ProtectedVA;
class VideoProcessingVA;

enum eUMC_DirectX_Status
{
    E_FRAME_LOCKED = 0xC0262111
};

enum eUMC_VA_Status
{
    UMC_ERR_DEVICE_FAILED         = -2000,
    UMC_ERR_DEVICE_LOST           = -2001,
    UMC_ERR_FRAME_LOCKED          = -2002
};

///////////////////////////////////////////////////////////////////////////////////
class FrameAllocator;
class VideoAcceleratorParams
{
    DYNAMIC_CAST_DECL_BASE(VideoAcceleratorParams);
public:

    VideoAcceleratorParams(void)
    {
        m_pVideoStreamInfo = NULL;
        m_iNumberSurfaces  = 0; // 0 means use default value
        m_protectedVA = 0;
        m_needVideoProcessingVA = false;
        m_surf = 0;
        m_allocator = 0;
    }

    virtual ~VideoAcceleratorParams(void){}

    VideoStreamInfo *m_pVideoStreamInfo;
    Ipp32s          m_iNumberSurfaces;
    Ipp32s          m_protectedVA;
    bool            m_needVideoProcessingVA;

    FrameAllocator  *m_allocator;

    // if extended surfaces exist
    void** m_surf;
};


class VideoAccelerator
{
    DYNAMIC_CAST_DECL_BASE(VideoAccelerator);
public:
    VideoAccelerator() :
        m_Profile(UNKNOWN),
        m_Platform(VA_UNKNOWN_PLATFORM),
        m_HWPlatform(VA_HW_UNKNOWN),
        m_protectedVA(0),
        m_videoProcessingVA(0),
        m_allocator(0),
        m_bH264ShortSlice(false),
        m_bH264MVCSupport(false),
        m_isUseStatuReport(true),
        m_H265ScalingListScanOrder(1)
    {
    }

    virtual ~VideoAccelerator()
    {
        Close();
    }

    virtual Status Init(VideoAcceleratorParams* pInfo) = 0; // Initilize and allocate all resources
    virtual Status Close(void);
    virtual Status Reset(void);

    virtual Status BeginFrame   (Ipp32s  index) = 0; // Begin decoding for specified index
    virtual void*  GetCompBuffer(Ipp32s            buffer_type,
                                 UMCVACompBuffer** buf   = NULL,
                                 Ipp32s            size  = -1,
                                 Ipp32s            index = -1) = 0; // request buffer
    virtual Status Execute      (void) = 0;          // execute decoding
    virtual Status ExecuteExtensionBuffer(void * buffer) = 0;
    virtual Status ExecuteStatusReportBuffer(void * buffer, Ipp32s size) = 0;
    virtual Status SyncTask(Ipp32s index, void * error = NULL) = 0;
    virtual Status QueryTaskStatus(Ipp32s index, void * status, void * error) = 0;
    virtual Status ReleaseBuffer(Ipp32s type) = 0;   // release buffer
    virtual Status ReleaseAllBuffers() = 0;
    virtual Status EndFrame     (void * handle = 0) = 0;          // end frame

    virtual bool IsIntelCustomGUID() const = 0;
    /* TODO: is used on Linux only? On Linux there are isues with signed/unsigned return value. */
    virtual Ipp32s GetSurfaceID(Ipp32s idx) { return idx; }

    virtual ProtectedVA * GetProtectedVA() {return m_protectedVA;}
    virtual VideoProcessingVA * GetVideoProcessingVA() {return m_videoProcessingVA;}

    bool IsLongSliceControl() const { return (!m_bH264ShortSlice); };
    bool IsMVCSupport() const {return m_bH264MVCSupport; };
    bool IsUseStatusReport() { return m_isUseStatuReport; }
    void SetStatusReportUsing(bool isUseStatuReport) { m_isUseStatuReport = isUseStatuReport; }

    Ipp32s ScalingListScanOrder() const
    { return m_H265ScalingListScanOrder; }

    virtual void GetVideoDecoder(void **handle) = 0;

    VideoAccelerationProfile    m_Profile;          // entry point
    VideoAccelerationPlatform   m_Platform;         // DXVA, LinuxVA, etc
    VideoAccelerationHW         m_HWPlatform;

protected:
    ProtectedVA       *  m_protectedVA;
    VideoProcessingVA *  m_videoProcessingVA;
    FrameAllocator    *  m_allocator;

    bool            m_bH264ShortSlice;
    bool            m_bH264MVCSupport;
    bool            m_isUseStatuReport;
    Ipp32s          m_H265ScalingListScanOrder; //0 - up-right, 1 - raster (derived from DXVA2_ConfigPictureDecode.Config4GroupedCoefs). Default is 1 (raster) to conform old driver beh.
};

///////////////////////////////////////////////////////////////////////////////////

class UMCVACompBuffer //: public MediaData
{
public:
    UMCVACompBuffer()
    {
        type = -1;
        BufferSize = 0;
        DataSize = 0;
        ptr = NULL;
        PVPState = NULL;

        FirstMb = -1;
        NumOfMB = -1;
        FirstSlice = -1;
    }
    virtual ~UMCVACompBuffer(){}

    // Set
    virtual Status SetBufferPointer(Ipp8u *_ptr, size_t bytes)
    {
        ptr = _ptr;
        BufferSize = (Ipp32s)bytes;
        return UMC_OK;
    }
    virtual void SetDataSize(Ipp32s size);
    virtual void SetNumOfItem(Ipp32s num);
    virtual Status SetPVPState(void *buf, Ipp32u size);

    // Get
    Ipp32s  GetType()       const { return type; }
    void*   GetPtr()        const { return ptr; }
    Ipp32s  GetBufferSize() const { return BufferSize; }
    Ipp32s  GetDataSize()   const { return DataSize; }
    void*   GetPVPStatePtr()const { return PVPState; }
    Ipp32s   GetPVPStateSize()const { return (NULL == PVPState ? 0 : sizeof(PVPStateBuf)); }

    // public fields
    Ipp32s      FirstMb;
    Ipp32s      NumOfMB;
    Ipp32s      FirstSlice;
    Ipp32s      type;

protected:
    Ipp8u       PVPStateBuf[16];
    void*       ptr;
    void*       PVPState;
    Ipp32s      BufferSize;
    Ipp32s      DataSize;
};

} // namespace UMC

#endif // __cplusplus

#pragma warning(default : 4201)

#endif // __UMC_VA_BASE_H__
