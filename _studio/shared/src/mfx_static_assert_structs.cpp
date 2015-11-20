/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_static_assert_structs.cpp

\* ****************************************************************************** */

#include "mfxstructures.h"
#include "mfxplugin.h"
#include "mfxenc.h"
#include "mfxpak.h"
#include "mfxfei.h"
#include "mfxcamera.h"

/* .cpp instead of .h to avoid changing of include files dependencies graph
    and not to include unnecessary includes into libmfx library             */

/*  VC's IntelliSense calculates sizeof(object*) wrongly and marks several expressions by red lines. This is IntelliSense bug not ours*/

#define MSDK_STATIC_ASSERT(COND, MSG)  typedef char static_assertion_##MSG[ (COND) ? 1 : -1];
#define MSDK_STATIC_ASSERT_STRUCT_SIZE(STRUCT, SIZE) MSDK_STATIC_ASSERT(sizeof(STRUCT) == SIZE, size_of_##STRUCT_is_fixed);
//mfxstructures.h
#if defined (__MFXSTRUCTURES_H__)
    #if defined(_WIN64) || defined(LINUX64)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxVideoParam                ,208 ) //IntelliSense wrongly marks this line
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameId                   ,8   )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameInfo                 ,68  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameData                 ,96  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameSurface1             ,184 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxInfoMFX                   ,136 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxInfoVPP                   ,168 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCodingOption           ,64  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCodingOption2          ,68  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPDoNotUse            ,24  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPDenoise             ,12  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPDetail              ,12  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPProcAmp             ,40  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxEncodeStat                ,88  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxDecodeStat                ,80  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxVPPStat                   ,72  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVppAuxData             ,20  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxPayload                   ,32  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxEncodeCtrl                ,56  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameAllocRequest         ,92  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameAllocResponse        ,32  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCodingOptionSPSPPS     ,32  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVideoSignalInfo        ,20  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPDoUse               ,24  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtOpaqueSurfaceAlloc     ,80  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtAVCRefListCtrl         ,1068)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPFrameRateConversion ,72  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPImageStab           ,32  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtPictureTimingSEI       ,160 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtAvcTemporalLayers      ,92  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtEncoderCapability      ,128 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtEncoderResetOption     ,32  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtAVCEncodedFrameInfo    ,1056)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxVPPCompInputStream        ,64  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPVideoSignalInfo     ,48  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtEncoderROI             ,8224)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPDeinterlacing       ,32  )
    #elif defined(_WIN32) || defined(LINUX32)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxVideoParam                ,196 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameId                   ,8   )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameInfo                 ,68  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameData                 ,72  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameSurface1             ,160 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxInfoMFX                   ,136 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxInfoVPP                   ,168 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCodingOption           ,64  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCodingOption2          ,68  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPDoNotUse            ,16  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPDenoise             ,12  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPDetail              ,12  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPProcAmp             ,40  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxEncodeStat                ,88  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxDecodeStat                ,80  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxVPPStat                   ,72  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVppAuxData             ,20  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxPayload                   ,28  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxEncodeCtrl                ,48  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameAllocRequest         ,92  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameAllocResponse        ,24  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCodingOptionSPSPPS     ,24  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVideoSignalInfo        ,20  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPDoUse               ,16  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtOpaqueSurfaceAlloc     ,72  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtAVCRefListCtrl         ,1068)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPFrameRateConversion ,72  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPImageStab           ,32  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtPictureTimingSEI       ,160 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtAvcTemporalLayers      ,92  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtEncoderCapability      ,128 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtEncoderResetOption     ,32  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtAVCEncodedFrameInfo    ,1056)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxVPPCompInputStream        ,64  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPVideoSignalInfo     ,48  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtEncoderROI             ,8224)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtVPPDeinterlacing       ,32  )
    #endif
#endif //defined (__MFXSTRUCTURES_H__)

//mfxvideo.h
#if defined (__MFXVIDEO_H__)
    #if defined(_WIN64) || defined(LINUX64)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxBufferAllocator ,56)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameAllocator  ,64)
    #elif defined(_WIN32) || defined(LINUX32)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxBufferAllocator ,36)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFrameAllocator  ,40)
    #endif
#endif //defined (__MFXVIDEO_H__)

//mfxplugin.h
#if defined (__MFXPLUGIN_H__)
    #if defined(_WIN64) || defined(LINUX64)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxPluginParam      ,64 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxPluginUID        ,16 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxCoreParam        ,64 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxCoreInterface    ,256)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxVideoCodecPlugin ,160)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxPlugin           ,128)
    #elif defined(_WIN32) || defined(LINUX32)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxPluginParam      ,64 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxPluginUID        ,16 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxCoreParam        ,64 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxCoreInterface    ,144)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxVideoCodecPlugin ,96 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxPlugin           ,64 )
    #endif
#endif //defined (__MFXPLUGIN_H__)

//mfxenc.h
#if defined (__MFXENC_H__)
    #if defined(_WIN64) || defined(LINUX64)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(_mfxENCInput               ,184)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(_mfxENCOutput              ,152)
    #elif defined(_WIN32) || defined(LINUX32)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(_mfxENCInput               ,156)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(_mfxENCOutput              ,140)
    #endif
#endif //defined (__MFXENC_H__)

//mfxfei.h
#if defined (__MFXFEI_H__)
    #if defined(_WIN64) || defined(LINUX64)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiPreEncCtrl        ,128 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiPreEncMVPredictors,72 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiEncQP             ,72 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiPreEncMV          ,72 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiPreEncMBStat      ,72 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiEncFrameCtrl      ,128 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiEncMVPredictors   ,72 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiEncMBCtrl         ,72 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiEncMV             ,72 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiEncMBStat         ,72 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFeiPakMBCtrl            ,64 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiPakMBCtrl         ,72 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFeiFunction             ,4  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiParam             ,128)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxPAKInput                ,184)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxPAKOutput               ,32)
    #elif defined(_WIN32) || defined(LINUX32)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiPreEncCtrl        ,120 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiPreEncMVPredictors,68 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiEncQP             ,68 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiPreEncMV          ,68 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiPreEncMBStat      ,68 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiEncFrameCtrl      ,128 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiEncMVPredictors   ,68 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiEncMBCtrl         ,68 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiEncMV             ,68 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiEncMBStat         ,68 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFeiPakMBCtrl            ,64 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiPakMBCtrl         ,68 )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxFeiFunction             ,4  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtFeiParam             ,128)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxPAKInput                ,156)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxPAKOutput               ,16 )
    #endif
#endif //defined (__MFXFEI_H__)

//mfxcamera.h
#if defined (__MFXCAMERA_H__)
    #if defined(_WIN64) || defined(LINUX64)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxCamGammaParam             ,4   )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamGammaCorrection     ,4144)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxCamWhiteBalanceMode       ,4   )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamWhiteBalance        ,80  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamHotPixelRemoval     ,12  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamBlackLevelCorrection,32  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxCamVignetteCorrectionParam,8   )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamVignetteCorrection  ,56  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamBayerDenoise        ,64  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamColorCorrection3x3  ,96  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamPadding             ,32  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxCamBayerFormat            ,4   )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamPipeControl         ,32  )
    #elif defined(_WIN32) || defined(LINUX32)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxCamGammaParam             ,4   )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamGammaCorrection     ,4144)
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxCamWhiteBalanceMode       ,4   )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamWhiteBalance        ,80  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamHotPixelRemoval     ,12  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamBlackLevelCorrection,32  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxCamVignetteCorrectionParam  ,8   )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamVignetteCorrection  ,52  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamBayerDenoise        ,64  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamColorCorrection3x3  ,96  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamPadding             ,32  )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxCamBayerFormat            ,4   )
        MSDK_STATIC_ASSERT_STRUCT_SIZE(mfxExtCamPipeControl         ,32  )
    #endif
#endif //defined (__MFXCAMERA_H__)
