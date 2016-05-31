/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_dx9.h

\* ****************************************************************************** */
#pragma once

#include "mfx_camera_plugin_utils.h"

#include <d3d9.h>
#include <dxva2api.h>
#include <dxvahd.h>
#include <ippi.h>

#define MFX_VA_WIN
#define MFX_D3D9_ENABLED
#define MFX_ENABLE_VPP

#include "libmfx_core.h"
#include "libmfx_core_interface.h"
#include "libmfx_core_d3d9.h"
#include "mfx_vpp_interface.h"
#include "cm_mem_copy.h"

#define MFX_FOURCC_R16_BGGR MAKEFOURCC('I','R','W','0')
#define MFX_FOURCC_R16_RGGB MAKEFOURCC('I','R','W','1')
#define MFX_FOURCC_R16_GRBG MAKEFOURCC('I','R','W','2')
#define MFX_FOURCC_R16_GBRG MAKEFOURCC('I','R','W','3')

#define SEPARATED_FROM_CORE 1

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(PTR)    { if (PTR) { delete [] PTR; PTR = 0; } }
#endif

using namespace MfxCameraPlugin;
using namespace MfxHwVideoProcessing;

class D39FrameAllocResponse : public mfxFrameAllocResponse
{
public:
    D39FrameAllocResponse(): 
        m_numFrameActualReturnedByAllocFrames(0)
    {
        Zero((mfxFrameAllocResponse &)*this);
    }

    mfxStatus AllocateSurfaces(VideoCORE *core, mfxFrameAllocRequest & req)
    {
        if (! core)
            return MFX_ERR_MEMORY_ALLOC;

        req.NumFrameSuggested = req.NumFrameMin;
        mfxFrameAllocResponse response;
        m_mids.resize(req.NumFrameMin, 0);
        m_locked.resize(req.NumFrameMin, 0);
        core->AllocFrames(&req, &response, true);
        for (int i = 0; i < req.NumFrameMin; i++)
            m_mids[i] = response.mids[i];

        NumFrameActual = req.NumFrameMin;
        mids = &m_mids[0];

        return MFX_ERR_NONE;
    }

    mfxU32 Lock(mfxU32 idx)
    {
    if (idx >= m_locked.size())
        return 0;
    assert(m_locked[idx] < 0xffffffff);
    return ++m_locked[idx];
    }

    void Unlock()
    {
        std::fill(m_locked.begin(), m_locked.end(), 0);
    }

    mfxU32 Unlock(mfxU32 idx)
    {
        if (idx >= m_locked.size())
        return mfxU32(-1);
        assert(m_locked[idx] > 0);
        return --m_locked[idx];
    }

    mfxU32 Locked(mfxU32 idx) const
    {
        return (idx < m_locked.size()) ? m_locked[idx] : 1;
    }

    void Free(VideoCORE *core)
    {
        if (core)
        {
            if (MFX_HW_D3D9  == core->GetVAType())
            {
                for (size_t i = 0; i < m_responseQueue.size(); i++)
                    core->FreeFrames(&m_responseQueue[i]);
            }
            else
            {
                if (mids)
                {
                    NumFrameActual = m_numFrameActualReturnedByAllocFrames;
                    core->FreeFrames(this);
                }
            }
        }
    }

private:

    mfxU16      m_numFrameActualReturnedByAllocFrames;

    std::vector<mfxFrameAllocResponse> m_responseQueue;
    std::vector<mfxMemId>              m_mids;
    std::vector<mfxU32>                m_locked;

    mfxU16 m_width;
    mfxU16 m_height;
    mfxU32 m_fourCC;
};

static const GUID DXVAHD_VPE_QUERY_GUID = { 0x4FCE08E4, 0xB5FD, 0x435e, { 0xB5, 0x2C, 0x19, 0xB8, 0xD5, 0xEC, 0x33, 0x87 } };
static const GUID DXVAHD_VPE_EXEC_GUID = { 0xEDD1D4B9, 0x8659, 0x4CBC, { 0xA4, 0xD6, 0x98, 0x31, 0xA2, 0x16, 0x3A, 0xC3 } };

class DXVAHDVideoProcessor
{
    #define MAX_ROW                3
    #define MAX_COLUMN            3
    #define MAX_OFFSETS            3
    #define RGB_CHANNEL_MATRIX    3
    #define MAX_FGC_SEGMENTS    64

    enum TDxvahdSurfaceType 
    {
        DXVAHD_SURF_VIDEO_OUTPUT = 0,
        DXVAHD_SURF_VIDEO_INPUT = 1,
        DXVAHD_SURF_OUTPUT = 2,
        DXVAHD_SURF_INPUT = 3
    };

    struct dxvahdvpcaps
    {
        GUID VPGuid;
    
        UINT uiPastFrames;
        UINT uiFutureFrames;
    
        UINT ProcessorCaps;
        UINT ITelecineCaps;

        UINT uiCustomRateCount;
        DXVAHD_CUSTOM_RATE_DATA *pCustomRates;
    };

    struct dxvahddevicecaps
    {
        UINT            DeviceCaps;
        
        // supported filters 
        UINT            FilterCaps;            // combined filter caps 
        BOOL            bContrastFilter;
        BOOL            bBrightnessFilter;
        BOOL            bHueFilter;
        BOOL            bSaturationFilter;
        BOOL            bNoiseFilter;
        BOOL            bDetailFilter;    
        BOOL            bEdgeEnhancementFilter;

        // filter range data
        DXVAHD_FILTER_RANGE_DATA    BrightnessRange;
        DXVAHD_FILTER_RANGE_DATA    ContrastRange;
        DXVAHD_FILTER_RANGE_DATA    HueRange;
        DXVAHD_FILTER_RANGE_DATA    SaturationRange;
        DXVAHD_FILTER_RANGE_DATA    NoiseReductionRange;
        DXVAHD_FILTER_RANGE_DATA    EdgeEnhancementRange;
        DXVAHD_FILTER_RANGE_DATA    AnamorphicScalingRange;

        // supported dxvahd device caps feature caps 
        BOOL            bConstriction;    
        BOOL            bLumaKey;
        BOOL            bAlphaBlending; 
        BOOL            bAlphaPalette; 

        D3DFORMAT        *pInputFormats;        
        UINT            uiInputFormatCount;    

        D3DFORMAT        *pOutputFormats;        
        UINT            uiOutputFormatCount;    
    
        UINT            uiMaxInputSurfaces;        
        UINT            uiMaxStreamStates;
    
        D3DFORMAT        *pRenderTargetFormats;
        UINT            uiRenderTargetFormatCount;

        UINT            uiForwardSampleCount;
        UINT            uiBackwardSampleCount;

        // video processor caps
        dxvahdvpcaps    *pVPCaps;
        UINT            uiVideoProcessorCount;
    };

    // Vprep variances
    enum VPE_VPREP_VARIANCE_TYPE
    {
        VPREP_VARIANCE_TYPE_NONE = 0,
        VPREP_VARIANCE_TYPE_1
    };

    typedef struct VPE_VPREP_VARIANCE_CAPS
    {
        VPE_VPREP_VARIANCE_TYPE        Type;
        UINT                        VarianceSize;
        UINT                        VarianceCount;
    }VPE_VPREP_VARIANCE_CAPS, *PVPE_VPREP_VARIANCE_CAPS;

    typedef struct VPE_VPREP_CAPS
    {
        UINT        bImageStabilization : 1;
        UINT        bInterlacedScaling : 1;
        UINT        bFieldWeavingControl : 1;
        UINT        bVariances : 1;
        UINT        Reserved : 28;
    }VPE_VPREP_CAPS, *PVPE_VPREP_CAPS;

    struct VPE_GUID_INFO
    {
        GUID    Guid;
        PUINT   pVersion;
        UINT    VersionCount;
    };
    struct VPE_GUID_ENUM
    {
        UINT            Function : 16;    // [IN]
        UINT            Version : 16;    // [IN]
        UINT            AllocSize;            // [IN/OUT]
        UINT            GuidCount;            // [OUT]
        VPE_GUID_INFO    *pGuidArray;        // [OUT]
    };


    enum TD3DHD_VPE_FUNCTION
    {
        VPE_FN_GUID_ENUM = 0x0
    };

    typedef struct _VPE_VERSION
    {
        UINT    Version;
    } VPE_VERSION;

    enum TD3DHD_VPREP_FUNCTION
    {
        VPE_FN_SET_VERSION_PARAM = 0x1,
        VPE_FN_SET_STATUS_PARAM = 0x5,
        VPE_FN_GET_STATUS_PARAM = 0x6,
        VPE_FN_PROC_QUERY_CAPS = 0x10,
        VPE_FN_MODE_PARAM = 0x20,
        VPE_FN_CPU_GPU_COPY_QUERY_CAPS = 0x2A,
        VPE_FN_SET_CPU_GPU_COPY_PARAM = 0x2B,
        VPE_FN_VPREP_QUERY_CAPS = 0x30,
        VPE_FN_VPREP_ISTAB_PARAM = 0x31,
        VPE_FN_VPREP_INTERLACE_PARAM = 0x32,
        VPE_FN_VPREP_QUERY_VARIANCE_CAPS = 0x33,
        VPE_FN_VPREP_SET_VARIANCE_PARAM = 0x34,
        VPE_FN_VPREP_GET_VARIANCE_PARAM = 0x35,
        VPE_FN_VPREP_YUV_RANGE_PARAM = 0x36
    };

    enum VPE_MODE
    {
        VPE_MODE_NONE = 0x0,
        VPE_MODE_PREPROC = 0x1,
        VPE_MODE_CAM_PIPE = 0x2,
        VPE_MODE_CPU_GPU_COPY = 0x3,
        VPE_MODE_MAX_VALUE = VPE_MODE_CPU_GPU_COPY,
    };

    typedef struct VPE_MODE_PARAM
    {
        VPE_MODE    Mode;
    }VPE_MODE_PARAM, *PVPE_MODE_PARAM;
    
    typedef struct VPE_SET_STATUS_PARAM
    {
        UINT        bEnableStatusCollection : 1;
        UINT        Reserved : 31;
    }VPE_SET_STATUS_PARAM, *PVPE_SET_STATUS_PARAM;

    enum VPE_STATUS
    {
        VPE_STATUS_COMPLETED = 0,
        VPE_STATUS_NOT_READY,
        VPE_STATUS_NOT_AVAILABLE
    };

    struct VPE_STATUS_PARAM
    {
        UINT        FrameId;
        VPE_STATUS    Status;
    };

    struct VPE_GET_STATUS_PARAMS
    {
        UINT                    StatusCount;
        VPE_STATUS_PARAM        *pStatusBuffer;
    };
    
    enum VPE_PROC_FEATURE
    {
        VPE_PROC_FRAME_RATE_CONVERSATION = 0x1,
        VPE_PROC_IMAGE_STABILIZATION = 0x2
    };

    enum FORMAT
    {
        FORMAT_D3DDDIFORMAT,
        FORMAT_DXGIFORMAT
    };

    struct VPE_PROC_CONTENT_DESC
    {
        UINT                InputFrameWidth;
        UINT                InputFrameHeight;
        FORMAT                InputFormat;
        FORMAT                OutputFormat;
        VPE_PROC_FEATURE    ProcFeature;
        UINT                IsProgressive : 1;
        UINT                Reserved : 31;
    };

    struct VPE_PROC_CAPS
    {
        UINT            MaxInputFrameWidth;
        UINT            MaxInputFrameHeight;
        union
        {
            struct
            {
                UINT    IsInputWidthNotSupported : 1;
                UINT    IsInputHeightNotSupported : 1;
                UINT    IsInputFormatNotSupported : 1;
                UINT    IsInterlaceNotSupported : 1;
                UINT    Reserved : 28;
            };
            UINT        FeatureNotSupported;
        };
    };
    
    typedef struct VPE_PROC_QUERY_CAPS
    {
        VPE_PROC_CONTENT_DESC    ContentDesc;
        VPE_PROC_CAPS            Caps;
    }VPE_PROC_QUERY_CAPS, *PVPE_PROC_QUERY_CAPS;

    typedef struct VPE_CPU_GPU_COPY_QUERY_CAPS
    {
        UINT        bIsSupported : 1;
        UINT        Reserved : 31;
    }VPE_CPU_GPU_COPY_QUERY_CAPS, *PVPE_CPU_GPU_COPY_QUERY_CAPS;

    enum VPE_CPU_GPU_COPY_DIRECTION
    {
        VPE_CPU_GPU_COPY_DIRECTION_CPU_TO_GPU,
        VPE_CPU_GPU_COPY_DIRECTION_GPU_TO_CPU
    };

    typedef struct VPE_CPU_GPU_COPY_PARAM
    {
        VPE_CPU_GPU_COPY_DIRECTION        Direction;
        UINT                            MemSize;
        VOID                            *SystemMem;
    }VPE_CPU_GPU_COPY_PARAM, *PVPE_CPU_GPU_COPY_PARAM;

    enum VPE_VPREP_ISTAB_MODE
    {
        VPE_VPREP_ISTAB_MODE_NONE,
        VPE_VPREP_ISTAB_MODE_CROP_UPSCALE,
        VPE_VPREP_ISTAB_MODE_CROP
    };

    typedef struct VPE_VPREP_ISTAB_PARAM
    {
        VPE_VPREP_ISTAB_MODE    Mode;
    }VPE_VPREP_ISTAB_PARAM, *PVPE_VPREP_ISTAB_PARAM;

    // Interlaced Scaling/Field Weaving 
    enum VPE_VPREP_INTERLACE_MODE
    {
        VPE_VPREP_INTERLACE_MODE_NONE,
        VPE_VPREP_INTERLACE_MODE_ISCALE,
        VPE_VPREP_INTERLACE_MODE_FIELD_WEAVE
    };


    typedef struct VPE_VPREP_INTERLACE_PARAM
    {
        VPE_VPREP_INTERLACE_MODE    Mode;
    }VPE_VPREP_INTERLACE_PARAM, *PVPE_VPREP_INTERLACE_PARAM;


    struct VPE_VPREP_SET_VARIANCE_PARAM
    {
        VPE_VPREP_VARIANCE_TYPE        Type;
    };

    struct VPE_VPREP_GET_VARIANCE_PARAMS
    {
        UINT        FrameCount;
        UINT        BufferSize;
        VOID        *pBuffer;
    };

    // YUV Range
    struct VPE_VPREP_YUV_RANGE_PARAM
    {
        UINT        bFullRangeEnabled : 1;
        UINT        Reserved : 31;
    };

    struct VPE_FUNCTION
    {
        UINT                                Function : 16; // [IN]
        union
        {
            VPE_VERSION                        *pVersion;
            VPE_MODE_PARAM                    *pModeParam;
            VPE_SET_STATUS_PARAM            *pSetStatusParam;
            VPE_GET_STATUS_PARAMS            *pGetStatusParams;
            VPE_PROC_QUERY_CAPS                *pProcCaps;
            VPE_CPU_GPU_COPY_QUERY_CAPS        *pCpuGpuCopyCaps;
            VPE_CPU_GPU_COPY_PARAM            *pCpuGpuCopyParam;
            VPE_VPREP_CAPS                    *pVprepCaps;
            VPE_VPREP_ISTAB_PARAM            *pIStabParam;
            VPE_VPREP_INTERLACE_PARAM        *pInterlaceParam;
            VPE_VPREP_VARIANCE_CAPS            *pVarianceCaps;
            VPE_VPREP_SET_VARIANCE_PARAM    *pSetVarianceParam;
            VPE_VPREP_GET_VARIANCE_PARAMS    *pGetVarianceParams;
            VPE_VPREP_YUV_RANGE_PARAM        *pYUVRangeParam;
        };
    };

    struct TCameraPipeEnable
    {
        UINT bActive;                /**< @brief [in/out]  Whether CamPipe active (1) or de-active (0)*/
    };

    struct THotPixel
    {
        UINT      bActive;                      /**< @brief [in/out]  Whether CamPipe Functional Block active (1) or de-active (0)*/
        UINT      PixelThresholdDifference;     /**< @brief [in/out] Pixel threshold difference.  */
        UINT      PixelCountThreshold;          /**< @brief [in/out] Pixel count threshold.       */
    };

    struct UNSIGNED_8_8_STRUCT
    {
        USHORT      integer : 8;  /**< @brief integer component     */
        USHORT      mantissa : 8;  /**< @brief fractional component  */
    };

    struct TVignetteCorrectionElem
    {
        UNSIGNED_8_8_STRUCT   R;         /**< @brief Correction factor for the Red channel   */
        UNSIGNED_8_8_STRUCT   G0;        /**< @brief Correction factor for the First Green channel */
        UNSIGNED_8_8_STRUCT   B;         /**< @brief Correction factor for the Blue channel  */
        UNSIGNED_8_8_STRUCT   G1;        /**< @brief Correction factor for the Second Green channel */
        USHORT                reserved; /**< @brief Reserved */
    };

    struct TVignetteCorrection
    {
        UINT      bActive;                          /**< @brief [in/out] Whether CamPipe Functional Block active (1) or de-active (0)*/
        UINT      Width;                          /**< @brief [in]     Width of the correction map    */
        UINT      Height;                         /**< @brief [in]     Height of the correction map   */
        UINT      Stride;                         /**< @brief [in]     Stride of the correction map   */
        TVignetteCorrectionElem *pCorrectionMap;  /**< @brief [in]     Pointer to the correction map  */
    };

    struct TBlackLevelCorrection
    {
        UINT      bActive;                          /**< @brief [in/out]  Whether CamPipe Functional Block active (1) or de-active (0)*/
        UINT      R;                              /**< @brief [in/out] Black level subtraction value for Red channel    */
        UINT      G0;                             /**< @brief [in/out] Black level subtraction value for Top Green channel  */
        UINT      B;                              /**< @brief [in/out] Black level subtraction value for Blue channel   */
        UINT      G1;                              /**< @brief [in/out] Black level subtraction value for Bottom Green channel  */
    };

    enum TWhiteBalanceMode
    {
        WHITE_BALANCE_MODE_NONE = 0,      /**< @brief White Balance Disabled                */
        WHITE_BALANCE_MODE_MANUAL,        /**< @brief User Defined White Balance parameters */
        WHITE_BALANCE_MODE_AUTO_IMAGE,    /**< @brief Auto White Balance for Image          */
    };

    struct TWhiteBalance
    {
        UINT                bActive;                        /**< @brief [in/out]  Whether CamPipe Functional Block active (1) or de-active (0)*/
        TWhiteBalanceMode    Mode;                           /**< @brief [in/out]  White balance mode (0: None 1: Manual 2: Auto-Image ) */
        FLOAT                RedCorrection;                    /**< @brief [in/out]  White Balance Red component parameter*/
        FLOAT                GreenTopCorrection;                /**< @brief [in/out]  White Balance Top Green component parameter*/
        FLOAT                BlueCorrection;                    /**< @brief [in/out]  White Balance Blue component parameter*/
        FLOAT                GreenBottomCorrection;            /**< @brief [in/out]  White Balance Bottom Green component parameter*/
    };

    struct TColorCorrection
    {
        UINT      bActive;                          /**< @brief [in/out]  Whether CamPipe Functional Block active (1) or de-active (0)*/
        FLOAT     CCM[MAX_ROW][MAX_COLUMN];       /**< @brief [in/out]  Color space conversion matrix  */
    };

    struct TForwardGammaSeg
    {
        SHORT PixelValue;                                /**< @brief [in/out]  Fwd Gamma Pixel Value         */
        SHORT RedChannelCorrectedValue;                /**< @brief [in/out]  Fwd Gamma Red Value         */
        SHORT GreenChannelCorrectedValue;                /**< @brief [in/out]  Fwd Gamma Green Value         */
        SHORT BlueChannelCorrectedValue;                /**< @brief [in/out]  Fwd Gamma Blue Value         */
    };

    struct TForwardGamma
    {
        UINT             bActive;                        /**< @brief [in/out]  Whether CamPipe Functional Block active (1) or de-active (0)*/
        TForwardGammaSeg *pSegment;    /**< @brief [in/out]  Array  of Fwd Gamma Segments    */
    };

    struct TFrontEndRgbToYuvCSC
    {
        UINT      bActive;                        /**< @brief [in/out]  Whether CamPipe Functional Block active (1) or de-active (0)*/
        FLOAT     PreOffset[MAX_OFFSETS];       /**< @brief [in/out]  Pre-transform offset          */
        FLOAT     Matrix[MAX_ROW][MAX_COLUMN];  /**< @brief [in/out]  Color space conversion matrix */
        FLOAT     PostOffset[MAX_OFFSETS];      /**< @brief [in/out]  Post-transform offset         */
    };

    struct TBackEndYuvToRgbCSC
    {
        UINT      bActive;                        /**< @brief [in/out]  Whether CamPipe Functional Block active (1) or de-active (0)*/
        FLOAT     PreOffset[MAX_OFFSETS];       /**< @brief [in/out]  Pre-transform offset          */
        FLOAT     Matrix[MAX_ROW][MAX_COLUMN];  /**< @brief [in/out]  Color space conversion matrix */
        FLOAT     PostOffset[MAX_OFFSETS];      /**< @brief [in/out]  Post-transform offset         */
    };

    struct TSkinToneEnhancement
    {
        UINT    bActive;                    /**< @brief [in/out]  Whether CamPipe Functional Block active (1) or de-active (0)*/
        UINT  STEFactor;                    /**< @brief [in/out]  STE Factor                         */
    };

    struct TTotalColorControl
    {
        UINT bActive;                        /**< @brief [in/out]  Whether CamPipe Functional Block active (1) or de-active (0)*/
        BYTE Red;                            /**< @brief [in/out]  Set Red Component of TCC           */
        BYTE Green;                        /**< @brief [in/out]  Set Green Component of TCC         */
        BYTE Blue;                        /**< @brief [in/out]  Set Blue Component of TCC          */
        BYTE Cyan;                        /**< @brief [in/out]  Set Cyan Component of TCC          */
        BYTE Magenta;                        /**< @brief [in/out]  Set Magenta Component of TCC       */
        BYTE Yellow;                        /**< @brief [in/out]  Set Yellow Component of TCC        */
    };

    enum TGamutCompressMode
    {
        GAMUT_COMPRESS_BASIC = 0,            /**< @brief [in/out]  Basic Gamut Compress Mode          */
        GAMUT_COMPRESS_ADVANCED,              /**< @brief [in/out]  Advanced Gamut Compress Mode          */
    };

    struct TGamutCompress
    {
        UINT bActive;                        /**< @brief [in/out]  Whether CamPipe Functional Block active (1) or de-active (0) */
        TGamutCompressMode Mode;            /**< @brief [in/out]  Gamut Compress Mode                */
    };

    struct TLensGeometryDistortionCorrection
    {
        UINT      bActive;                                              /**< @brief [in/out] Whether CamPipe Functional Block active (1) or de-active (0) */
        FLOAT     a[RGB_CHANNEL_MATRIX];                              /**< @brief [in/out] Value for parameter a  */
        FLOAT     b[RGB_CHANNEL_MATRIX];                              /**< @brief [in/out] Value for parameter b  */
        FLOAT     c[RGB_CHANNEL_MATRIX];                              /**< @brief [in/out] Value for parameter c  */
        FLOAT     d[RGB_CHANNEL_MATRIX];                              /**< @brief [in/out] Value for parameter d  */
    };

    typedef struct _TLUTParams
    {
        UINT  bActive;
        UINT  LUTSize;
        union
        {
            MfxHwVideoProcessing::LUT17 *p17;
            MfxHwVideoProcessing::LUT33 *p33;
            MfxHwVideoProcessing::LUT65 *p65;
        };
    } TLUTParams;

    struct TCameraPipeCaps
    {
        union
        {
            struct
            {
                UINT    bHotPixel : 1;
                UINT    bBlackLevelCorrection : 1;
                UINT    bWhiteBalanceManual : 1;
                UINT    bWhiteBalanceAuto : 1;
                UINT    bColorCorrection : 1;
                UINT    bForwardGamma : 1;
                UINT    bLensDistortionCorrection : 1;
                UINT    bVignetteCorrection : 1;
                UINT    bRgbToYuvCSC : 1;
                UINT    bYuvToRgbCSC : 1;
                UINT    b3DLUT       : 1; // GEN10
                UINT    bPipeOrder   : 1; // GEN10
                UINT    Reserved : 19;
            };
            struct
            {
                UINT    Value;
            };
        };
    };

    struct TCameraPipeMode_2_6
    {
        UINT                                Function : 16; // [IN]
        union
        {

            TCameraPipeCaps                     *pCPCaps;     /**< @brief [in/out] Camera Pipe Capabilities Structure */
            TCameraPipeEnable                   *pActive;                /**< @brief [in/out] CP Alerts the driver that the VEBOX is being used for camera workflow operations.   */
            THotPixel                           *pHotPixel;           /**< @brief [in/out] CP Hot Pixel Removal Structure */
            TVignetteCorrection                 *pVignette;           /**< @brief [in/out] CP Vignette Correction Structure */
            TBlackLevelCorrection                *pBlackLevel;         /**< @brief [in/out] CP Black Level Subtraction Structure */
            TWhiteBalance                       *pWhiteBalance;       /**< @brief [in/out] CP White Balance Structure */
            TColorCorrection                    *pColorCorrection;    /**< @brief [in/out] CP Color Correction Structure */
            TForwardGamma                       *pForwardGamma;       /**< @brief [in/out] CP Forward Gamma Correction Structure */
            TFrontEndRgbToYuvCSC                *pRgbToYuvCSC;        /**< @brief [in/out] CP RGB To YUV Front End Color Space Conversion Structure */
            TBackEndYuvToRgbCSC                    *pYuvToRgbCSC;        /**< @brief [in/out] CP YUV To RGB Back  End Color Space Conversion Structure */
            TSkinToneEnhancement                *pSkinToneEnhance;            /**< @brief [in/out] CP IE Skin Tone Enhancement Structure */
            TTotalColorControl                    *pTotalColorControl;  /**< @brief [in/out] CP IE Total Color Control Structure */
            TGamutCompress                        *pGamutCompress;        /**< @brief [in/out] CP IE Gamut Compress Structure */
            TLensGeometryDistortionCorrection   *pLensCorrection;      /**< @brief [in/out] CP Lens Geometric Distortion Correction Structure */
            TLUTParams                          *pLUT;
        };
    };

    enum TD3D9HDVPECPFunction
    {
        // Camera Pipe (CP) Function IDs: - new changes for SKL - will add comments later
        VPE_FN_CP_QUERY_CAPS = 0x0100,      /**< @brief Query Capabilities Function.                  */
        VPE_FN_CP_ACTIVATE_CAMERA_PIPE = 0x0101,      /**< @brief Activate CP capabilities Function.            */
        VPE_FN_CP_BLACK_LEVEL_CORRECTION = 0x0102,   /**< @brief Black Level Subtraction Function.             */
        VPE_FN_CP_VIGNETTE_CORRECTION = 0x0103,   /**< @brief Vignette Correction Function.                 */
        VPE_FN_CP_WHITE_BALANCE = 0x0104,   /**< @brief White Balance Function.                       */
        VPE_FN_CP_HOT_PIXEL = 0x0105,   /**< @brief Hot Pixel Removal Function.                   */
        VPE_FN_CP_COLOR_CORRECTION = 0x0106,   /**< @brief Color Correction Function.                    */
        VPE_FN_CP_FORWARD_GAMMA_CORRECTION = 0x0107,   /**< @brief Forward Gamma Correction Function.            */
        VPE_FN_IE_RGB_TO_YUV_CSC = 0x0108,   /**< @brief RGB To YUV CSC Function.                        */
        VPE_FN_IE_SKIN_TONE_ENHANCEMENT = 0x0109,   /**< @brief Skin Tone Enhancement Function.                */
        VPE_FN_IE_GAMUT_COMPRESS = 0x010A,   /**< @brief Gamut Compress Function.                        */
        VPE_FN_IE_TOTAL_COLOR_CONTROL = 0x010B,   /**< @brief Total Color Control Function.                    */
        VPE_FN_YUV_TO_RGB_CSC = 0x010C,   /**< @brief YUV To RGB CSC Function.                        */
        VPE_FN_CP_LENS_GEOMETRY_DISTORTION_CORRECTION = 0x010D,    /**< @brief Lens Geometry Distortion Correction Function. */
        VPE_FN_CP_3DLUT                               = 0x010E,
        VPE_FN_CP_PIPE_ORDER                          = 0x010F
    };

public:
    DXVAHDVideoProcessor()    
    {
        m_cameraFGC.pSegment = new TForwardGammaSeg[64];
        m_bIsSet = false;
        m_cachedReadyTaskIndex.clear();
        m_camera3DLUT17        = 0;
        m_camera3DLUT33        = 0;
        m_camera3DLUT65        = 0;
    };

    ~DXVAHDVideoProcessor()    
    {
        delete [] m_cameraFGC.pSegment;
        m_cachedReadyTaskIndex.clear();
        std::map<void *, DXVAHD_STREAM_DATA *>::iterator it;
        for (it = m_Streams.begin() ; it != m_Streams.end(); it++)
        {
            SAFE_DELETE_ARRAY(it->second);
        }

        if (m_camera3DLUT17)
            free(m_camera3DLUT17);

        if(m_camera3DLUT33)
            free(m_camera3DLUT33);

        if(m_camera3DLUT65)
            free(m_camera3DLUT65);
    };
    mfxStatus DestroyDevice(void);
    mfxStatus CreateDevice(VideoCORE *core, mfxVideoParam *par, bool temporary);
    mfxStatus QueryCapabilities( MfxHwVideoProcessing::mfxVppCaps &caps );
    mfxStatus Execute(MfxHwVideoProcessing::mfxExecuteParams *executeParams);
    mfxStatus Register(mfxHDLPair* pSurfaces, mfxU32 num, BOOL bRegister);
    mfxStatus QueryTaskStatus(mfxU32 idx);

    IDirect3DSurface9* SurfaceCreate(unsigned int width, 
                                     unsigned int height, 
                                                       D3DFORMAT fmt,
                                                       mfxU32 type);

private:
    // Do not allow copying of the object
    DXVAHDVideoProcessor(const DXVAHDVideoProcessor& from) {};
    DXVAHDVideoProcessor& operator=(const DXVAHDVideoProcessor&) {return *this; };

    mfxStatus CreateInternalDevice();
    mfxStatus EnumerateCaps();
    HRESULT   GetDxvahdVPFormatsAndRanges(
                    dxvahddevicecaps          *pDeviceCaps,
                    D3DFORMAT                 *pInputFormats,
                    D3DFORMAT                 *pOutputFormats,
                    DXVAHD_FILTER_RANGE_DATA  *pBrightnessRange,
                    DXVAHD_FILTER_RANGE_DATA  *pContrastRange,
                    DXVAHD_FILTER_RANGE_DATA  *pHueRange,
                    DXVAHD_FILTER_RANGE_DATA  *pSaturationRange,
                    DXVAHD_FILTER_RANGE_DATA  *pNoiseReductionRange,
                    DXVAHD_FILTER_RANGE_DATA  *pEdgeEnhancementRange);
    HRESULT   SetVideoProcessBltState(DXVAHD_BLT_STATE State, 
                                      UINT uiDataSize, 
                                      const void *pData);
    HRESULT   SetVideoProcessStreamState(UINT uiStreamIndex, 
                                         DXVAHD_STREAM_STATE State,
                                         UINT uiDataSize,
                                         const void *pData);
    HRESULT   SetCameraPipeWhiteBalance(CameraWhiteBalanceParams *params);
    HRESULT   SetCameraPipeForwardGamma(CameraForwardGammaCorrectionParams *params);
    HRESULT   SetCameraPipeColorCorrection(CameraCCMParams *params);
    HRESULT   SetCameraPipeLensCorrection(CameraLensCorrectionParams *params);
    HRESULT   SetCameraPipeBlackLevelCorrection(CameraBlackLevelParams *params);
    HRESULT   SetCameraPipeVignetteCorrection(CameraVignetteCorrectionParams *params);
    HRESULT   SetCameraPipe3DLUTCorrection(Camera3DLUTParams *params);
    HRESULT   SetCameraPipeHotPixel(CameraHotPixelRemovalParams *params);
    HRESULT   SetCameraPipeEnable(TCameraPipeEnable bActive, BOOL bCheckOnSet/*=FALSE*/);
    HRESULT   GetSetOutputExtension(DXVAHD_BLT_STATE state, UINT uiDataSize, void *pData, BOOL bSet, BOOL bCheckOnSet);
    HRESULT   ExecuteVPrepFunction(TD3DHD_VPREP_FUNCTION vprepFunction, void* pData, UINT uiDataSize);
    void      QueryVPEGuids();
    HRESULT   InitVersion();

    

    IDirect3DDeviceManager9 *m_pD3Dmanager;
    IDirect3D9              *m_pD3D;
    IDirect3DDevice9        *m_pD3DDevice;
    IDirect3DDevice9Ex      *m_pD3DDeviceEx;
    IDXVAHD_Device          *m_pDXVADevice;
    IDXVAHD_VideoProcessor  *m_pDXVAVideoProcessor;
    UINT                     m_uiVPExtGuidCount;
    dxvahddevicecaps         m_dxva_caps;
    DXVAHD_CONTENT_DESC      m_videoDesc;

    VPE_VPREP_CAPS          m_vprepCaps;
    VPE_VPREP_VARIANCE_CAPS m_vprepVarianceCaps;
    VPE_GUID_INFO*          m_pVPEGuids;
    bool                    m_bIsSet;
    TForwardGamma           m_cameraFGC;
    std::map<void *, DXVAHD_STREAM_DATA *> m_Streams;
    UMC::Mutex              m_mutex;
    std::set<mfxU32>        m_cachedReadyTaskIndex;

    LUT17                  *m_camera3DLUT17;
    LUT33                  *m_camera3DLUT33;
    LUT65                  *m_camera3DLUT65;
};

class D3D9CameraProcessor: public CameraProcessor
{
public:
    D3D9CameraProcessor()
    {
        m_ddi.reset(0);
        m_pCmCopy.reset(0);
        m_executeParams = 0;
        m_executeSurf   = 0;
        m_counter       = 0;
        m_paddedInput   = false;
    };

    ~D3D9CameraProcessor() {
        Close();
    };

    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out)
    {
        if (in)
        {
            for (int i = 0; i < in->NumExtParam; i++)
            {
                if (in->ExtParam[i])
                {
                    if (MFX_EXTBUF_CAM_VIGNETTE_CORRECTION == in->ExtParam[i]->BufferId && in->vpp.In.CropW > 8*1024)
                    {
                        if (out)
                        {
                            out->vpp.In.CropW = 0;
                            out->vpp.In.Width = 0;
                        }
                        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
                    }
                }
            }
        }

        return MFX_ERR_NONE;
    }

    virtual mfxStatus Reset(mfxVideoParam *par, CameraParams * FrameParams);

    virtual mfxStatus Init(CameraParams *CameraParams);

    virtual mfxStatus Init(mfxVideoParam *par)
    {
        par;
        return MFX_ERR_NONE;
    }

    virtual mfxStatus Close()
    {
        if(m_pCmCopy.get()){
            m_pCmCopy.get()->Release();
        }

        m_pCmCopy.reset(0);

        if (m_ddi.get())
            m_ddi.get()->DestroyDevice();

        m_ddi.release();

        if (m_systemMemIn)
            for (int i = 0; i < m_inputSurf.size(); i++)
            {
                m_inputSurf[i].Release();
            }

        m_inputSurf.resize(0);

        for (int i = 0; i < m_outputSurf.size(); i++)
        {
            m_outputSurf[i].Release();
        }

        m_outputSurf.resize(0);

        m_inputSurf.clear();
        m_outputSurf.clear();
        if (m_executeParams )
            free(m_executeParams);
        m_executeParams = 0;
        if (m_executeSurf)
            free(m_executeSurf);
        m_executeSurf = 0;
        return MFX_ERR_NONE;
    }

    virtual mfxStatus AsyncRoutine(AsyncParams *pParam);
    virtual mfxStatus CompleteRoutine(AsyncParams * pParam);

protected:
    mfxStatus CheckIOPattern(mfxU16  IOPattern);
    mfxStatus PreWorkOutSurface(mfxFrameSurface1 *surf, mfxU32 *poolIndex, MfxHwVideoProcessing::mfxExecuteParams *params);
    mfxStatus PreWorkInSurface(mfxFrameSurface1 *surf, mfxU32 *poolIndex, MfxHwVideoProcessing::mfxDrvSurface *drvSurf);
    mfxU32    BayerToFourCC(mfxU32);

private:

    // Do not allow copying of the object
    D3D9CameraProcessor(const D3D9CameraProcessor& from) {};
    D3D9CameraProcessor& operator=(const D3D9CameraProcessor&) {return *this; };

    mfxU32 FindFreeResourceIndex(
    D39FrameAllocResponse const & pool,
    mfxU32                        *index)
    {
        *index = NO_INDEX;
        for (mfxU32 i = 0; i < pool.NumFrameActual; i++)
            if (pool.Locked(i) == 0)
            {
                *index = i;
                return i;
            }
        return NO_INDEX;
    }

    mfxMemId AcquireResource(
        D39FrameAllocResponse & pool,
        mfxU32                  index)
    {
        if (index > pool.NumFrameActual)
            return 0; /// MID_INVALID; ???
        pool.Lock(index);
        return pool.mids[index];
    }

    mfxMemId AcquireResource(
        D39FrameAllocResponse & pool,
        mfxU32 *index)
    {
        return AcquireResource(pool, FindFreeResourceIndex(pool, index));
    }

    bool                                             m_systemMemOut;
    bool                                             m_systemMemIn;
    bool                                             m_paddedInput;
    CameraParams                                     m_CameraParams;
    UMC::Mutex                                       m_guard;
    mfxU16                                           m_AsyncDepth;
    MfxHwVideoProcessing::mfxExecuteParams          *m_executeParams;
    MfxHwVideoProcessing::mfxDrvSurface             *m_executeSurf;
    std::auto_ptr<DXVAHDVideoProcessor>              m_ddi;
    mfxU32                                           m_counter;
    mfxU16                                           m_width;
    mfxU16                                           m_height;
    D3DFORMAT                                        m_inFormat;
    D3DFORMAT                                        m_outFormat;

    template <class T, bool isSingle>
    class s_ptr
    {
    public:
        s_ptr():m_ptr(0)
        {
        };
        ~s_ptr()
        {
            reset(0);
        }
        T* get()
        {
            return m_ptr;
        }
        T* pop()
        {
            T* ptr = m_ptr;
            m_ptr = 0;
            return ptr;
        }
        void reset(T* ptr = NULL)
        {
            if (m_ptr)
            {
                if (isSingle)
                    delete m_ptr;
                else
                    delete[] m_ptr;
            }
            m_ptr = ptr;
        }
    protected:
        T* m_ptr;
    };
    struct s_InternalSurf {
        IDirect3DSurface9 *surf;
        bool               locked;
        s_InternalSurf() {locked=false;};
        void Release(void) { surf->Release(); };
    };
    s_ptr<CmCopyWrapper, true>    m_pCmCopy;
    IDirect3DDeviceManager9      *m_pD3Dmanager;
    std::vector<s_InternalSurf>   m_inputSurf;
    std::vector<s_InternalSurf>   m_outputSurf;

};
