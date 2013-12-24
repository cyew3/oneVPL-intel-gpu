/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION

This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement

Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

File Name: d3d11_video_processor.h

\* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_VA_WIN)
#if defined (MFX_D3D11_ENABLED)

#ifndef _D3D11_VIDEO_PROCESSOR_H_
#define _D3D11_VIDEO_PROCESSOR_H_

#define UMC_VA_DXVA

#include <atlbase.h>
#include <d3d11.h>
#include "ippdefs.h"
#include "vm_file.h"

#include "mfxvideo.h"
#include "mfxdefs.h"
#include "mfx_vpp_interface.h"

#include <set>
#include <algorithm>

//---------------------------------------------------------
//    VPE INTERFACE Version 1 (SNB, IVB, HSW)
//---------------------------------------------------------

// GUID {4FCE08E4-B5FD-435e-B52C-19B8D5EC3387}
static const GUID VPE_GUID = {0x4fce08e4, 0xb5fd, 0x435e, { 0xb5, 0x2c, 0x19, 0xb8, 0xd5, 0xec, 0x33, 0x87 } };

// GUID for Interface V1
static const GUID VPE_GUID_INTERFACE_V1 = {0x4E61148B, 0x46D7, 0x4179, {0x81, 0xAA, 0x11, 0x63, 0xEE, 0x8E, 0xA1, 0xE9} };

// GUID for Interface V2
static const GUID VPE_GUID_INTERFACE_V2 = {0xEDD1D4B9, 0x8659, 0x4CBC, {0xA4, 0xD6, 0x98, 0x31, 0xA2, 0x16, 0x3A, 0xC3} };

typedef struct _VPE_GUID_INFO
{
    GUID Guid;       
    UINT *pVersion;  
    UINT VersionCount;
} VPE_GUID_INFO;

typedef struct _VPE_GUID_ENUM
{
    UINT Reserved;            //[IN]
    UINT AllocSize;           //[IN/OUT]
    UINT GuidCount;           //[OUT] 
    VPE_GUID_INFO *pGuidArray;//[OUT]

} VPE_GUID_ENUM;

typedef struct _VPE_GUID_QUERY_STRUCT
{
    UINT Function: 16;
    UINT Version: 16;
    UINT GuidCount;
    VPE_GUID_INFO *pGuidArray;

} VPE_GUID_QUERY_STRUCT;

typedef struct PREPROC_QUERY_SCENE_DETECTION_D3D11
{
    UINT FrameNumber;
    UINT QueryMode:1;
    UINT: 31;
    union
    {
        SHORT FrameSceneChangRate;
        struct
        {
            SHORT PreFieldSceneChangeRate;
            SHORT CurFieldSceneChangeRate;
        };
    };

} PREPROC_QUERY_SCENE_DETECTION_D3D11;

typedef struct PREPROC_STATUS_QUERY_BUFFER
{
    UINT StatusReportID;
    UINT Status : 8;
    UINT        : 24;

} PREPROC_STATUS_QUERY_BUFFER;

typedef struct PREPROC_STATUS_QUERY_PARAMS
{
    UINT StatusReportCount;
    PREPROC_STATUS_QUERY_BUFFER *pStatusBuffer;

} PREPROC_STATUS_QUERY_PARAMS;

typedef struct tagPREPROC_QUERYCAPS
{
    UINT bInterlacedScaling  : 1;
    UINT bTargetSysMemory    : 1;
    UINT bSceneDetection     : 1;
    UINT bIS                 : 1;
    UINT bVariance           : 1;
    UINT bFieldWeavingControl: 1;
    UINT Reserved            : 26;

} PREPROC_QUERYCAPS;

typedef struct _SET_PREPROC_PARAMS
{
    UINT bEnablePreProcMode     : 1;
    UINT iTargetInterlacingMode : 1;
    UINT bTargetInSysMemory     : 1;
    UINT bSceneDetectionEnable  : 1;
    UINT bVarianceQuery         : 1;
    UINT bFieldWeaving          : 1;
    UINT Reserved               : 26;
    UINT StatusReportID;

} SET_PREPROC_PARAMS;


typedef enum tagVPREP_ISTAB_MODE
{
    VPREP_ISTAB_MODE_NONE = 0,
    VPREP_ISTAB_MODE_CROPUPSCALE,
    VPREP_ISTAB_MODE_CROP,

} VPREP_ISTAB_MODE;


typedef struct tagSET_IS_PARAMS
{
    VPREP_ISTAB_MODE   IStabMode;
    UINT               Reserved1;
    UINT               Reserved2;

} SET_IS_PARAMS;


typedef struct tagPREPROC_QUERY_VARIANCE_CAPS
{
    VARIANCE_TYPE Type;
    UINT          VarianceSize;
    UINT          VarianceCount;

} PREPROC_QUERY_VARIANCE_CAPS;


typedef struct tagPREPROC_QUERY_VARIANCE_PARAMS
{
    UINT    FrameNumber; // it is legal to set StatusID due to issue in DX11 interface
    VOID    *pVariances;
    UINT    VarianceBufferSize;

}PREPROC_QUERY_VARIANCE_PARAMS;

typedef struct tagPREPROC_MODE
{
    UINT Function: 16;
    UINT Version: 16;

    union
    {
        PREPROC_QUERYCAPS                   *pPreprocQueryCaps;
        SET_PREPROC_PARAMS                  *pPreprocParams;
        PREPROC_QUERY_SCENE_DETECTION_D3D11 *pQuerySCD;
        PREPROC_STATUS_QUERY_PARAMS         *pPreprocQueryStatus;
        SET_IS_PARAMS                       *pISParams;
        PREPROC_QUERY_VARIANCE_CAPS         *pVarianceCaps;
        PREPROC_QUERY_VARIANCE_PARAMS       *pVarianceParams;
    };

} PREPROC_MODE;

enum
{
    VPE_FN_QUERY_PREPROC_CAPS     = 0x0000,
    VPE_FN_SET_PREPROC_MODE       = 0x0001,
    VPE_FN_QUERY_SCENE_DETECTION  = 0x0002,
    VPE_FN_QUERY_PREPROC_STATUS   = 0x0003,
    VPE_FN_SET_IS_PARAMS          = 0x0004,
    VPE_FN_QUERY_VARIANCE_CAPS    = 0x0005,
    VPE_FN_QUERY_VARIANCE         = 0x0006,
};

//---------------------------------------------------------
//    VPE INTERFACE Version 2 (BDW platform)
//---------------------------------------------------------

enum
{
    VPE_FN_SET_VERSION_PARAM = 0x1,
    VPE_FN_SET_STATUS_PARAM  = 0x5,
    VPE_FN_GET_STATUS_PARAM  = 0x6,

    VPE_FN_PROC_QUERY_CAPS   = 0x10,

    VPE_FN_MODE_PARAM              = 0x20,
    VPE_FN_CPU_GPU_COPY_QUERY_CAPS = 0x2A,
    VPE_FN_SET_CPU_GPU_COPY_PARAM  = 0x2B,

    VPE_FN_VPREP_QUERY_CAPS          = 0x30,
    VPE_FN_VPREP_ISTAB_PARAM         = 0x31,
    VPE_FN_VPREP_INTERLACE_PARAM     = 0x32,
    VPE_FN_VPREP_QUERY_VARIANCE_CAPS = 0x33,
    VPE_FN_VPREP_SET_VARIANCE_PARAM  = 0x34,
    VPE_FN_VPREP_GET_VARIANCE_PARAM  = 0x35,
    VPE_FN_VPREP_YUV_RANGE_PARAM     = 0x36,

    VPE_FN_CP_QUERY_CAPS                     = 0x100,
    VPE_FN_CP_BLACK_LEVEL_CORRECTION_PARAM   = 0x101, 
    VPE_FN_CP_VIGNETTE_CORRECTION_PARAM      = 0x102,
    VPE_FN_CP_WHITE_BALANCE_PARAM            = 0x103,
    VPE_FN_CP_HOT_PIXEL_PARAM                = 0x104,
    VPE_FN_CP_COLOR_CORRECTION_PARAM         = 0x105,
    VPE_FN_CP_FORWARD_GAMMA_CORRECTION_PARAM = 0x106,
    VPE_FN_CP_RGB_TO_YUV_CSC_PARAM           = 0x107,

    VPE_FN_IE_SKIN_TONE_ENHANCEMENT_PARAM = 0x150,
    VPE_FN_IE_CONTRAST_ENHANCEMENT_PARAM  = 0x151,
    VPE_FN_IE_TOTAL_COLOR_CONTROL_PARAM   = 0x152,
    VPE_FN_IE_YUV_TO_RGB_CSC_PARAM        = 0x153,

}; // FUNCTION ID


typedef struct _VPE_VERSION 
{
    UINT    Version;
} VPE_VERSION;


typedef struct _VPE_SET_STATUS_PARAM
{
    UINT bEnableStatusCollection : 1;
    UINT Reserved                : 31;
} VPE_SET_STATUS_PARAM;


typedef enum _VPE_STATUS
{
    VPE_STATUS_COMPLETED,
    VPE_STATUS_NOT_READY,
    VPE_STATUS_NOT_AVAILABLE
} VPE_STATUS;


typedef struct _VPE_STATUS_PARAM
{
    UINT                FrameId;
    VPE_STATUS          Status;
} VPE_STATUS_PARAM;


typedef struct _VPE_GET_STATUS_PARAMS
{
    UINT                StatusCount;                                    // [in]
    VPE_STATUS_PARAM    *pStatusBuffer;                                 // [in]
} VPE_GET_STATUS_PARAMS;


typedef enum _VPE_MODE
{
    VPE_MODE_NONE            = 0x0,
    VPE_MODE_PREPROC         = 0x1,
    VPE_MODE_CAM_PIPE        = 0x2,
    VPE_MODE_CPU_GPU_COPY    = 0x3  
} VPE_MODE;


typedef struct _VPE_MODE_PARAM
{
    VPE_MODE    Mode;
} VPE_MODE_PARAM;

// CAPABILITIES

typedef enum _VPE_PROC_FEATURE
{
    VPE_PROC_FRAME_RATE_CONVERSATION   = 0x1,
    VPE_PROC_IMAGE_STABILIZATION       = 0x2
} VPE_PROC_FEATURE; 


typedef struct _VPE_PROC_CONTENT_DESC
{
    UINT                    InputFrameWidth;
    UINT                    InputFrameHeight;
    UINT                    OutputFrameWidth;
    UINT                    OutpuFrameHeight;
    DXGI_FORMAT             InputFormat;
    DXGI_FORMAT             OutpuFormat;
    VPE_PROC_FEATURE        ProcFeature;
    UINT                    IsProgressive   : 1;
    UINT                    Reserved        : 31;
} VPE_PROC_CONTENT_DESC;


typedef struct _VPE_PROC_CAPS
{
    UINT                    MaxInputFrameWidth;
    UINT                    MaxInputFrameHeight;
    UINT                    NumSupportedInputFormat;
    union
    {
        struct 
        { 
            UINT IsInputWidthNotSupported  : 1;
            UINT IsInputHeightNotSupported : 1;
            UINT IsInputFormatNotSupported : 1;
            UINT IsInterlacedNotSupported  : 1;
            UINT Reserved                  : 28;
        };
        UINT                FeatureNotSupported;
    };
} VPE_PROC_CAPS;


typedef struct _VPE_VPROC_QUERY_CAPS
{
    VPE_PROC_CONTENT_DESC   ContentDesc;                                //[in]
    VPE_PROC_CAPS           Caps;                                       //[out]
} VPE_PROC_QUERY_CAPS; 

// CPU_GPU_COPY_INTERFACE

typedef struct _VPE_CPU_GPU_COPY_QUERY_CAPS
{
    UINT    bIsSupported : 1;
    UINT    Reserved     : 31;
 } VPE_CPU_GPU_COPY_QUERY_CAPS;


typedef enum _VPE_CPU_GPU_COPY_DIRECTION
{
    VPE_CPU_GPU_COPY_DIRECTION_CPU_TO_GPU,
    VPE_CPU_GPU_COPY_DIRECTION_GPU_TO_CPU
} VPE_CPU_GPU_COPY_DIRECTION;


typedef struct _VPE_CPU_GPU_COPY_PARAM
{
    VPE_CPU_GPU_COPY_DIRECTION  Direction;                              // [in]
    UINT                        MemSize;                                // [in]
    VOID                        *pSystemMem;                            // [in]
} VPE_CPU_GPU_COPY_PARAM;

//   Pre-processing Interface

typedef struct _VPE_VPREP_CAPS
{
    UINT    bImageStabilization     :  1;
    UINT    bInterlacedScaling      :  1;
    UINT    bFieldWeavingControl    :  1;
    UINT    bVariances              :  1;
    UINT    Reserved                : 28;
} VPE_VPREP_CAPS;


typedef enum _VPE_VPREP_ISTAB_MODE
{
    VPE_VPREP_ISTAB_MODE_NONE,
    VPE_VPREP_ISTAB_MODE_CROP_UPSCALE,
    VPE_VPREP_ISTAB_MODE_CROP
} VPE_VPREP_ISTAB_MODE;


typedef struct VPE_VPREP_ISTAB_PARAM
{
    VPE_VPREP_ISTAB_MODE      Mode;
} VPE_VPREP_ISTAB_PARAM;


typedef enum _VPE_VPREP_INTERLACE_MODE
{
    VPE_VPREP_INTERLACE_MODE_NONE,
    VPE_VPREP_INTERLACE_MODE_ISCALE,
    VPE_VPREP_INTERLACE_MODE_FIELD_WEAVE
} VPE_VPREP_INTERLACE_MODE;


typedef struct _VPE_VPREP_INTERLACE_PARAM
{
    VPE_VPREP_INTERLACE_MODE Mode;
} VPE_VPREP_INTERLACE_PARAM;


typedef enum _VPE_VPREP_VARIANCE_TYPE
{
    VPREP_VARIANCE_TYPE_NONE = 0,
    VPREP_VARIANCE_TYPE_1
} VPE_VPREP_VARIANCE_TYPE;


typedef struct _VPE_VPREP_VARIANCE_CAPS
{
    VPE_VPREP_VARIANCE_TYPE     Type;
    UINT                        VarianceSize;
    UINT                        VarianceCount;
} VPE_VPREP_VARIANCE_CAPS;


typedef struct _VPE_VPREP_SET_VARIANCE_PARAM
{
    VPE_VPREP_VARIANCE_TYPE Type;
} VPE_VPREP_SET_VARIANCE_PARAM;


typedef struct _VPE_VPREP_GET_VARIANCE_PARAMS
{
    UINT                FrameCount;                                     // [in]
    UINT                BufferSize;                                     // [in]
    VOID                *pBuffer;                                       // [in]
} VPE_VPREP_GET_VARIANCE_PARAMS;


typedef struct _VPE_VPREP_YUV_RANGE_PARAM
{
    UINT    bFullRangeEnabled    : 1;                              // [in/out]
    UINT    Reserved             : 31;
} VPE_VPREP_YUV_RANGE_PARAM;

// CameraPipe Interface

typedef struct _VPE_CP_CAPS
{
    UINT    bHotPixel                         : 1;
    UINT    bBlackLevelCorrection             : 1;
    UINT    bWhiteBalanceManual               : 1;
    UINT    bWhiteBalanceAuto                 : 1;
    UINT    bColorCorrection                  : 1;
    UINT    bForwardGamma                     : 1;
    UINT    bLensDistortionCorrection         : 1;
    UINT    bVignetteCorrection               : 1;
    UINT    bRgbToYuvCSC                      : 1;
    UINT    bYuvToRgbCSC                      : 1;
    UINT    Reserved                          : 21;
} VPE_CP_CAPS; 


typedef struct _VPE_CP_BLACK_LEVEL_CORRECTION_PARAMS
{
    UINT    bActive;
    UINT    R; 
    UINT    G0;
    UINT    B;
    UINT    G1;
} VPE_CP_BLACK_LEVEL_CORRECTION_PARAMS;

typedef struct _VPE_UNSIGNED_8_8_STRUCT
{
    USHORT      Integer   : 8;  // integer component
    USHORT      Mantissa  : 8;  // fractional component
} VPE_UNSIGNED_8_8_STRUCT;


typedef struct tagUNSIGNED_8_8_STRUCT
{
    USHORT    integer  : 8;  // integer component
    USHORT    mantissa : 8;  // fractional component
} UNSIGNED_8_8_STRUCT;


typedef struct _VPE_CP_VIGNETTE_CORRECTION_ELEM
{
    VPE_UNSIGNED_8_8_STRUCT R;      // correction factor for the red channel
    VPE_UNSIGNED_8_8_STRUCT G0;     // correction factor for the first green channel
    VPE_UNSIGNED_8_8_STRUCT B;      // correction factor for the blue channel
    VPE_UNSIGNED_8_8_STRUCT G1;     // correction factor for the second green channel
    USHORT                  Reserved;
} VPE_CP_VIGNETTE_CORRECTION_ELEM;


typedef struct _VPE_CP_VIGNETTE_CORRECTION_PARAMS
{
    UINT                                bActive;
    UINT                                Width; 
    UINT                                Height;
    UINT                                Stride;
    VPE_CP_VIGNETTE_CORRECTION_ELEM     *pCorrectionMap;
} VPE_CP_VIGNETTE_CORRECTION_PARAMS;


typedef struct _VPE_CP_WHITE_BALANCE_PARAMS
{
    UINT        bActive;
    UINT        Mode;
    FLOAT       Matrix[3][3];
    FLOAT       RedCorrection;
    FLOAT       GreenTopCorrection;
    FLOAT       BlueCorrection;
    FLOAT       GreenBottomCorrection;
} VPE_CP_WHITE_BALANCE_PARAMS;


typedef struct _VPE_CP_HOT_PIXEL_PARAMS
{
    UINT    bActive;
    UINT    PixelThresholdDifference; 
    UINT    PixelCountThreshold;
} VPE_CP_HOT_PIXEL_PARAMS;


typedef struct _VPE_CP_COLOR_CORRECTION_PARAMS
{
    UINT    bActive;
    float   CCM[3][3];
} VPE_CP_COLOR_CORRECTION_PARAMS;


typedef struct _VPE_CP_FORWARD_GAMMA_PARAMS
{
    UINT    bActive;
    float   Slope[12];          // U4.8
    UINT    Bias[11];           // 0..255
    UINT    Points[11];         // 0..255
} VPE_CP_FORWARD_GAMMA_PARAMS;

// CSC Param

typedef struct _VPE_CSC_PARAMS
{
    UINT    bActive;
    float   PreOffset[3];
    float   Matrix[3][3];
    float   PostOffset[3];
} VPE_CSC_PARAMS;

// Main interface Structure

typedef struct _VPE_FUNCTION
{
    UINT                                        Function;               // [in]
    union                                                               // [in]
    {
        VPE_VERSION                             *pVersion;
        VPE_MODE_PARAM                          *pModeParam;
        VPE_SET_STATUS_PARAM                    *pSetStatusParam;
        VPE_GET_STATUS_PARAMS                   *pGetStatusParams;

        VPE_PROC_QUERY_CAPS                     *pProcCaps;
        
        VPE_CPU_GPU_COPY_QUERY_CAPS             *pCpuGpuCopyCaps;
        VPE_CPU_GPU_COPY_PARAM                  *pCpuGpuCopyParam;

        VPE_VPREP_CAPS                          *pVprepCaps;
        VPE_VPREP_ISTAB_PARAM                   *pIStabParam;
        VPE_VPREP_INTERLACE_PARAM               *pInterlacePram;
        VPE_VPREP_VARIANCE_CAPS                 *pVarianceCaps;
        VPE_VPREP_SET_VARIANCE_PARAM            *pSetVarianceParam;
        VPE_VPREP_GET_VARIANCE_PARAMS           *pGetVarianceParam;
        VPE_VPREP_YUV_RANGE_PARAM               *pYUVRangeParam;

        VPE_CP_CAPS                             *pCPCaps;
        VPE_CP_HOT_PIXEL_PARAMS                 *pHotPixel;
        VPE_CP_VIGNETTE_CORRECTION_PARAMS       *pVignette;
        VPE_CP_BLACK_LEVEL_CORRECTION_PARAMS    *pBlackLevel;
        VPE_CP_WHITE_BALANCE_PARAMS             *pWhiteBalance;
        VPE_CP_COLOR_CORRECTION_PARAMS          *pColorCorrection;
        VPE_CP_FORWARD_GAMMA_PARAMS             *pForwardGamma;

        VPE_CSC_PARAMS                          *pRgbToYuvCSC;
        VPE_CSC_PARAMS                          *pYuvToRgbCSC;
    };
} VPE_FUNCTION;


//---------------------------------------------------------
// DriverVideoProcessing::DX11
//---------------------------------------------------------

struct ID3D11VideoDevice;
struct ID3D11VideoContext;
struct ID3D11VideoProcessor;

namespace MfxHwVideoProcessing
{

    class D3D11VideoProcessor : public DriverVideoProcessing
    {
    public:

        D3D11VideoProcessor(void);
        virtual ~D3D11VideoProcessor(void);

        mfxStatus CreateDevice(VideoCORE *pCore, mfxVideoParam* par, bool isTemporal = false);
        mfxStatus ReconfigDevice(mfxU32 indx);
        mfxStatus DestroyDevice(void);                        

        mfxStatus Register(mfxHDLPair* pSurfaces, mfxU32 num, BOOL bRegister);

        mfxStatus QueryTaskStatus(mfxU32 idx);

        mfxStatus Execute(mfxExecuteParams *pParams);

        mfxStatus QueryCapabilities(mfxVppCaps& caps);            

        //BOOL IsRunning() {return true; };

        mfxStatus QueryVariance(
            mfxU32 frameIndex,
            std::vector<UINT> &variance);

    private:
        mfxStatus Init(
            ID3D11Device *pDevice, 
            ID3D11VideoDevice *pVideoDevice, 
            ID3D11DeviceContext *pDeviceContext, 
            ID3D11VideoContext *pVideoContext, 
            mfxVideoParam* videoParam);

        mfxStatus Close(void);

        //void GetVideoProcessorCapabilities(
        //    D3D11_VIDEO_PROCESSOR_CAPS & videoProcessorCaps_, 
        //    D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS & videoProcessorRateConvCaps_);

        void SetOutputTargetRect(BOOL Enable, RECT *pRect);
        void SetOutputBackgroundColor(BOOL YCbCr, D3D11_VIDEO_COLOR *pColor);
        void SetOutputColorSpace(D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace);
        void SetOutputAlphaFillMode(D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE AlphaFillMode, UINT StreamIndex);
        void SetOutputConstriction(BOOL Enable, SIZE Size);
        void SetStreamFrameFormat(UINT StreamIndex, D3D11_VIDEO_FRAME_FORMAT FrameFormat);
        void SetStreamColorSpace(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace);
        void SetStreamOutputRate(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE OutputRate, BOOL RepeatFrame, DXGI_RATIONAL *pCustomRate);
        void SetStreamSourceRect(UINT StreamIndex, BOOL Enable, RECT* pRect);
        void SetStreamDestRect(UINT StreamIndex, BOOL Enable, RECT* pRect);
        void SetStreamAlpha(UINT StreamIndex, BOOL Enable, FLOAT Alpha);
        void SetStreamPalette(UINT StreamIndex, UINT Count, UINT* pEntries);
        void SetStreamPixelAspectRatio(UINT StreamIndex, BOOL Enable, DXGI_RATIONAL *pSourceAspectRatio, DXGI_RATIONAL *pDestinationAspectRatio);
        void SetStreamAutoProcessingMode(UINT StreamIndex, BOOL Enable);
        void SetStreamFilter(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_FILTER Filter, BOOL Enable, int Level);

        HRESULT SetOutputExtension(const GUID* pExtensionGuid, UINT DataSize, void* pData);
        HRESULT SetStreamExtension(UINT StreamIndex, const GUID* pExtensionGuid, UINT DataSize, void* pData);

        void GetOutputTargetRect(BOOL *Enabled, RECT *pRect);
        void GetOutputBackgroundColor(BOOL* pYCbCr, D3D11_VIDEO_COLOR *pColor);
        void GetOutputColorSpace(D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace);
        void GetOutputAlphaFillMode(D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE *pAlphaFillMode, UINT *pStreamIndex);
        void GetOutputConstriction(BOOL *pEnabled, SIZE *pSize);
        void GetStreamFrameFormat(UINT StreamIndex, D3D11_VIDEO_FRAME_FORMAT* pFrameFormat);
        void GetStreamColorSpace(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_COLOR_SPACE* pColorSpace);
        void GetStreamOutputRate(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE* pOutputRate, BOOL* pRepeatFrame, DXGI_RATIONAL* pCustomRate);
        void GetStreamSourceRect(UINT StreamIndex, BOOL* pEnabled, RECT* pRect);
        void GetStreamDestRect(UINT StreamIndex, BOOL* pEnabled, RECT* pRect);
        void GetStreamAlpha(UINT StreamIndex, BOOL* pEnabled, FLOAT* pAlpha);
        void GetStreamPalette(UINT StreamIndex, UINT Count, UINT* pEntries);
        void GetStreamPixelAspectRatio(UINT StreamIndex, BOOL* pEnabled, DXGI_RATIONAL* pSourceAspectRatio, DXGI_RATIONAL* pDestinationAspectRatio);
        void GetStreamAutoProcessingMode(UINT StreamIndex, BOOL* pEnabled);
        void GetStreamFilter(UINT StreamIndex, D3D11_VIDEO_PROCESSOR_FILTER Filter, BOOL* pEnabled, int* pLevel);

        HRESULT GetOutputExtension(const GUID* pExtensionGuid, UINT DataSize, void* pData);
        HRESULT GetStreamExtension(UINT streamIndex, const GUID* pExtensionGuid, UINT DataSize, void* pData);

        mfxStatus QueryVideoProcessorCaps(void);
        mfxStatus QueryVarianceCaps(PREPROC_QUERY_VARIANCE_CAPS& varianceCaps);
        mfxStatus QueryVPE_AndExtCaps(void);


        mfxStatus ExecuteBlt(
            ID3D11Texture2D *pOutputSurface, 
            mfxU32 outIndex, 
            D3D11_VIDEO_PROCESSOR_STREAM *pStreams,
            mfxU32 statusReportID);

        HRESULT VideoProcessorBlt(
            ID3D11VideoProcessorOutputView *pView,
            UINT OutputFrame,
            UINT StreamCount,
            const D3D11_VIDEO_PROCESSOR_STREAM *pStreams);

        VideoCORE *                                 m_core;
        ID3D11Device*                               m_pDevice;
        ID3D11VideoDevice*                          m_pVideoDevice;
        ID3D11DeviceContext*                        m_pDeviceContext;
        ID3D11VideoContext*                         m_pVideoContext;

        D3D11_VIDEO_PROCESSOR_CONTENT_DESC          m_videoProcessorDescr;

        ID3D11VideoProcessor*                       m_pVideoProcessor;
        ID3D11VideoProcessorEnumerator*             m_pVideoProcessorEnum;

        ID3D11Resource*                             m_pOutputResource;
                
        std::vector<ID3D11VideoProcessorInputView*>   m_pInputView;// past + 1(cur) + future
        ID3D11VideoProcessorOutputView*               m_pOutputView;

        // new approach of dx11 VPE processing
        struct 
        {
            GUID                                        guid;
            UINT                                        version;
        } m_iface;
        //---------------------------------------------
        
        bool                                        m_isInterlacedScaling;
        std::set<mfxU32>                            m_cachedReadyTaskIndex;
        PREPROC_QUERYCAPS                           m_vpreCaps;
        PREPROC_QUERY_VARIANCE_CAPS                 m_varianceCaps;
        std::vector<CustomRateData>                 m_customRateData;

        mfxVideoParam                               m_video;
        UMC::Mutex                                  m_mutex;

        class Multiplier
        {
        public:
            mfxF32  saturation;
            mfxF32  hue;
            mfxF32  contrast;
            mfxF32  brightness;

            Multiplier()
                : saturation(1.f)
                , hue(1.f)
                , contrast(1.f)
                , brightness(1.f)
            {   }
        }   m_multiplier;

        class Caps
        {
        public:
            bool m_procAmpEnable;
            bool m_simpleDIEnable;
            bool m_advancedDIEnable;
            bool m_denoiseEnable;
            bool m_detailEnable;

            Caps()
                : m_procAmpEnable(false)
                //, m_deinterlaceEnable(false)
                , m_denoiseEnable(false)
                , m_detailEnable(false)
            {   }
        } m_caps;
        class D3D11Frc
        {
        public:
            mfxU32 m_inputIndx;
            mfxU32 m_outputIndx;

            mfxU32 m_inputFramesOrFieldsPerCycle;
            mfxU32 m_outputIndexCountPerCycle;

            bool m_isInited;

            //Init();
            
        } m_frcState;

        vm_file*  m_file;
    };
};




#endif // MFX_VA_WIN
#endif // _D3D11_VIDEO_PROCESSOR_H_
#endif // MFX_ENABLE_VPP
