/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_VPP_DDI_H
#define __MFX_VPP_DDI_H

#include <vector>
#include "mfxdefs.h"
#include "libmfx_core.h"
#include "mfx_platform_headers.h"

// AYA: temporal solution. PREPROC_QUERY_STATUS is used only!!!!
#if   defined(MFX_VA_WIN)
    #include "encoding_ddi.h"
#elif defined(MFX_VA_LINUX) || defined(MFX_VA_OSX)
    typedef unsigned int   UINT;

    typedef struct tagVPreP_StatusParams_V1_0
    {
        UINT StatusReportID;
        UINT Reserved1;

    } VPreP_StatusParams_V1_0;

    typedef struct tagFASTCOMP_QUERY_STATUS
    {
        UINT QueryStatusID;
        UINT Status;

        UINT Reserved0;
        UINT Reserved1;
        UINT Reserved2;
        UINT Reserved3;
        UINT Reserved4;

    } FASTCOMP_QUERY_STATUS;

    typedef enum _VPREP_ISTAB_MODE
    {
        ISTAB_MODE_NONE,
        ISTAB_MODE_BLACKEN,
        ISTAB_MODE_UPSCALE
    } VPREP_ISTAB_MODE;
#endif

namespace MfxHwVideoProcessing
{
    const mfxU32 g_TABLE_SUPPORTED_FOURCC [] =
    {
        MFX_FOURCC_NV12      ,
        MFX_FOURCC_YV12      ,
        MFX_FOURCC_NV16      ,
        MFX_FOURCC_YUY2      ,
        MFX_FOURCC_RGB3      ,
        MFX_FOURCC_RGB4      ,
        MFX_FOURCC_P8        ,
        MFX_FOURCC_P8_TEXTURE,
        MFX_FOURCC_P010      ,
        MFX_FOURCC_P210      ,
        MFX_FOURCC_BGR4      ,
        MFX_FOURCC_A2RGB10   ,
        MFX_FOURCC_ARGB16    ,
        MFX_FOURCC_R16       ,
        MFX_FOURCC_AYUV      ,
        MFX_FOURCC_AYUV_RGB4 ,
        MFX_FOURCC_UYVY
    };

    typedef enum mfxFormatSupport {
        MFX_FORMAT_SUPPORT_INPUT   = 1,
        MFX_FORMAT_SUPPORT_OUTPUT  = 2
    } mfxFormatSupport;

    struct RateRational
    {
        mfxU32  FrameRateExtN;
        mfxU32  FrameRateExtD;
    };

    struct CustomRateData
    {
        RateRational customRate;
        mfxU32       inputFramesOrFieldPerCycle;
        mfxU32       outputIndexCountPerCycle;
        mfxU32       bkwdRefCount;
        mfxU32       fwdRefCount;

        mfxU32       indexRateConversion;
    };

    struct FrcCaps
    {
        std::vector<CustomRateData> customRateData;
    };

    struct DstRect
    {
        mfxU32 DstX;
        mfxU32 DstY;
        mfxU32 DstW;
        mfxU32 DstH;
        mfxU16 LumaKeyEnable;
        mfxU16 LumaKeyMin;
        mfxU16 LumaKeyMax;
        mfxU16 GlobalAlphaEnable;
        mfxU16 GlobalAlpha;
        mfxU16 PixelAlphaEnable;
        /* NB! Experimental part*/
        mfxU16 IOPattern; //sys/video/opaque memory
        mfxU16 ChromaFormat;
        mfxU32 FourCC;
    };

    typedef struct _CameraCaps
    {
        mfxU32 uBlackLevelCorrection;
        mfxU32 uHotPixelCheck;
        mfxU32 uWhiteBalance;
        mfxU32 uColorCorrectionMatrix;
        mfxU32 uGammaCorrection;
        mfxU32 uVignetteCorrection;
        mfxU32 u3DLUT;
    } CameraCaps;

    typedef struct _CameraBlackLevelParams
    {
        mfxU32 uR;
        mfxU32 uG0;
        mfxU32 uG1;
        mfxU32 uB;
    } CameraBlackLevelParams;

    typedef struct _CameraWhiteBalanceParams
    {
        mfxF32 fR;
        mfxF32 fG0;
        mfxF32 fG1;
        mfxF32 fB;
    } CameraWhiteBalanceParams;

    typedef struct _CameraHotPixelRemovalParams
    {
        mfxU32 uPixelThresholdDifference;
        mfxU32 uPixelCountThreshold;
    } CameraHotPixelRemovalParams;

    typedef struct _CameraCCM
    {
        mfxF32 CCM[3][3];
    } CameraCCMParams;

    typedef struct _CameraLensCorrectionParams
    {
        mfxF32 a[3];
        mfxF32 b[3];
        mfxF32 c[3];
        mfxF32 d[3];
    } CameraLensCorrectionParams;

    typedef struct _CameraForwardGammaCorrectionSeg
    {
        mfxU16 PixelValue;
        mfxU16 RedChannelCorrectedValue;
        mfxU16 GreenChannelCorrectedValue;
        mfxU16 BlueChannelCorrectedValue;
    } CameraForwardGammaCorrectionSeg;

    typedef struct _CameraForwardGammaCorrectionParams
    {
        CameraForwardGammaCorrectionSeg Segment[64];
    } CameraForwardGammaCorrectionParams;

    typedef struct _LUT_ENTRY
    {
        USHORT R;
        USHORT G;
        USHORT B;
        USHORT Reserved;
    } LUT_ENTRY;

    const int LUT17_SEG = 17;
    const int LUT17_MUL = 32;
    const int LUT33_SEG = 33;
    const int LUT33_MUL = 64;
    const int LUT65_SEG = 65;
    const int LUT65_MUL = 128;
    typedef LUT_ENTRY LUT17[LUT17_SEG][LUT17_SEG][LUT17_MUL];
    typedef LUT_ENTRY LUT33[LUT33_SEG][LUT33_SEG][LUT33_MUL];
    typedef LUT_ENTRY LUT65[LUT65_SEG][LUT65_SEG][LUT65_MUL];

    typedef struct _Camera3DLUTParams
    {
        UINT  bActive;
        UINT  LUTSize;
        LUT_ENTRY *lut;
    } Camera3DLUTParams;

    typedef struct _CameraVignette_unsigned_8_8
    {
        USHORT      integer   : 8;
        USHORT      mantissa  : 8;
    } CameraVignette_unsigned_8_8;

  
    typedef struct _CameraVignetteCorrectionElem
    {
        CameraVignette_unsigned_8_8   R;
        CameraVignette_unsigned_8_8   G0;
        CameraVignette_unsigned_8_8   B;
        CameraVignette_unsigned_8_8   G1;
        USHORT                reserved;
    } CameraVignetteCorrectionElem;

    typedef struct _CameraVignetteCorrectionParams
    {
        UINT      bActive;
        UINT      Width;
        UINT      Height;
        UINT      Stride;
        CameraVignetteCorrectionElem *pCorrectionMap;
    } CameraVignetteCorrectionParams;


    struct mfxVppCaps
    {
        mfxU32 uAdvancedDI;
        mfxU32 uSimpleDI;
        mfxU32 uInverseTC;
        mfxU32 uDenoiseFilter;
        mfxU32 uDetailFilter;
        mfxU32 uProcampFilter;
        mfxU32 uSceneChangeDetection;
        CameraCaps cameraCaps;

        // MSDK 2013
        mfxU32 uFrameRateConversion;
        mfxU32 uDeinterlacing;
        mfxU32 uVideoSignalInfo;
        FrcCaps frcCaps;

        mfxU32 uIStabFilter;
        mfxU32 uVariance;

        mfxI32 iNumBackwardSamples;
        mfxI32 iNumForwardSamples;

        mfxU32 uMaxWidth;
        mfxU32 uMaxHeight;

        mfxU32 uFieldWeavingControl;

        mfxU32 uRotation;

        mfxU32 uScaling;

        std::map <mfxU32, mfxU32> mFormatSupport;

        mfxVppCaps()
            : uAdvancedDI(0)
            , uSimpleDI(0)
            , uInverseTC(0)
            , uDenoiseFilter(0)
            , uDetailFilter(0)
            , uProcampFilter(0)
            , uSceneChangeDetection(0)
            , uFrameRateConversion(0)
            , uDeinterlacing(0)
            , uVideoSignalInfo(0)
            , frcCaps()
            , uIStabFilter(0)
            , uVariance(0)
            , iNumBackwardSamples(0)
            , iNumForwardSamples(0)
            , uMaxWidth(0)
            , uMaxHeight(0)
            , uFieldWeavingControl(0)
            , uRotation(0)
            , uScaling(0)
            , mFormatSupport()
        {
            memset(&cameraCaps, 0, sizeof(CameraCaps));
        };

    private:
        mfxVppCaps(mfxVppCaps& caps);
        mfxVppCaps & operator= (mfxVppCaps & other);
    };


    typedef struct _mfxDrvSurface
    {
        mfxFrameInfo frameInfo;
        mfxHDLPair   hdl;
        // aya: driver fix
        mfxMemId     memId;
        bool         bExternal;

        mfxU64       startTimeStamp;
        mfxU64       endTimeStamp;

    } mfxDrvSurface;


    typedef struct _mfxExecuteParams
    {
        //surfaces
        mfxDrvSurface  targetSurface;
        mfxU64         targetTimeStamp;

        mfxDrvSurface* pRefSurfaces;
        int            refCount; // refCount == bkwdRefCount + 1 + fwdRefCount

        int            bkwdRefCount; // filled from DdiTask
        int            fwdRefCount;  //

        // params
        mfxI32         iDeinterlacingAlgorithm; //0 - none, 1 - BOB, 2 - advanced (means reference need)
        bool           bFMDEnable;

        bool           bDenoiseAutoAdjust;
        mfxU16         denoiseFactor;

        bool           bDetailAutoAdjust;
        mfxU16         detailFactor;

        mfxU32         iTargetInterlacingMode;

        bool           bEnableProcAmp;
        mfxF64         Brightness;
        mfxF64         Contrast;
        mfxF64         Hue;
        mfxF64         Saturation;

        bool           bSceneDetectionEnable;

        bool           bVarianceEnable;
        bool           bImgStabilizationEnable;
        mfxU32         istabMode;

        bool           bFRCEnable;
        CustomRateData customRateData;

        bool           bComposite;
        std::vector<DstRect> dstRects;
        mfxU32         iBackgroundColor;

        mfxU32         statusReportID;

        bool           bFieldWeaving;

        mfxU32         iFieldProcessingMode;

        //  Camera Pipe specific params
        bool                     bCameraPipeEnabled;
        bool                     bCameraBlackLevelCorrection;
        CameraBlackLevelParams   CameraBlackLevel;
        bool                     bCameraWhiteBalaceCorrection;
        CameraWhiteBalanceParams CameraWhiteBalance;
        bool                     bCameraHotPixelRemoval;
        CameraHotPixelRemovalParams CameraHotPixel;
        bool                     bCCM;
        CameraCCMParams          CCMParams;
        bool                     bCameraGammaCorrection;
        CameraForwardGammaCorrectionParams CameraForwardGammaCorrection;

        bool                     bCameraVignetteCorrection;
        CameraVignetteCorrectionParams     CameraVignetteCorrection;

        bool                     bCameraLensCorrection;
        CameraLensCorrectionParams         CameraLensCorrection;

        bool                     bCamera3DLUT;
        Camera3DLUTParams        Camera3DLUT;
        int         rotation;

        mfxU16      scalingMode;

        bool        bEOS;

        bool        bVideoSignalInfo;
        struct {
            mfxU16 transferMatrix;
            mfxU16 nominalRange;
        } VidoSignalInfoIn, VidoSignalInfoOut;
    } mfxExecuteParams;


    class DriverVideoProcessing
    {
    public:

        virtual ~DriverVideoProcessing(void){}

        virtual mfxStatus CreateDevice(VideoCORE * core, mfxVideoParam* par, bool isTemporal = false) = 0;

        virtual mfxStatus ReconfigDevice(mfxU32 indx) = 0;

        virtual mfxStatus DestroyDevice( void ) = 0;

        virtual mfxStatus Register(mfxHDLPair* pSurfaces, mfxU32 num, BOOL bRegister) = 0;

        virtual mfxStatus QueryTaskStatus(mfxU32 idx) = 0;

        virtual mfxStatus Execute(mfxExecuteParams *pParams) = 0;

        virtual mfxStatus QueryCapabilities( mfxVppCaps& caps ) = 0;

        virtual mfxStatus QueryVariance(
            mfxU32 frameIndex,
            std::vector<UINT> &variance) = 0;

    }; // DriverVideoProcessing

    DriverVideoProcessing* CreateVideoProcessing( VideoCORE* core );

}; // namespace

#endif // __MFX_VPP_BASE_DDI_H
#endif // MFX_ENABLE_VPP
/* EOF */
