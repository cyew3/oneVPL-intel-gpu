//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 - 2016 Intel Corporation. All Rights Reserved.
//

#include "test_vpp_utils.h"
#include "mfxvideo++int.h"

#define VAL_CHECK(val) {if (val) return MFX_ERR_UNKNOWN;}




void vppPrintHelp(const vm_char *strAppName, const vm_char *strErrorMessage)
{
    if (strErrorMessage)
    {
        vm_string_printf(VM_STRING("Error: %s\n"), strErrorMessage);
    } 
    else 
    {
        vm_string_printf(VM_STRING("Intel(R) Media SDK VPP Sample\n"));
    }

    vm_string_printf(VM_STRING("Usage1: %s [Options] -i InputFile -o OutputFile\n"), strAppName);

    vm_string_printf(VM_STRING("Options: \n"));
    vm_string_printf(VM_STRING("   [-lib  type]        - type of used library. sw, hw (def: sw)\n"));
    vm_string_printf(VM_STRING("   [-d3d11]            - force d3d11 interface using (def for HW is d3d9)\n"));
    vm_string_printf(VM_STRING("   [-crc CrcFile]      - calculate CRC32 and write it to specified file\n\n"));
    vm_string_printf(VM_STRING("   [-plugin_guid GUID] - use VPP plug-in with specified GUID\n\n"));
    vm_string_printf(VM_STRING("   [-extapi]           - use RunFrameVPPAsyncEx instead of RunFrameVPPAsync. Need for PTIR.\n\n"));
    vm_string_printf(VM_STRING("   [-gpu_copu]          - Specify GPU copy mode. This option triggers using of InitEX instead of Init.\n\n"));

    vm_string_printf(VM_STRING("   [-sw   width]     - width  of src video (def: 352)\n"));
    vm_string_printf(VM_STRING("   [-sh   height]    - height of src video (def: 288)\n"));
    vm_string_printf(VM_STRING("   [-scrX  x]        - cropX  of src video (def: 0)\n"));
    vm_string_printf(VM_STRING("   [-scrY  y]        - cropY  of src video (def: 0)\n"));
    vm_string_printf(VM_STRING("   [-scrW  w]        - cropW  of src video (def: width)\n"));
    vm_string_printf(VM_STRING("   [-scrH  h]        - cropH  of src video (def: height)\n"));
    vm_string_printf(VM_STRING("   [-sf   frameRate] - frame rate of src video (def: 30.0)\n"));
    vm_string_printf(VM_STRING("   [-scc  format]    - format (FourCC) of src video (def: nv12. support nv12|yv12|yuy2|rgb3|rgb4|imc3|yuv400|yuv411|yuv422h|yuv422v|yuv444)\n"));
    vm_string_printf(VM_STRING("   [-sbitshift 0|1]  - shift data to right or keep it the same way as in Microsoft's P010\n"));

    vm_string_printf(VM_STRING("   [-spic value]     - picture structure of src video\n")); 
    vm_string_printf(VM_STRING("                        0 - interlaced top    field first\n"));
    vm_string_printf(VM_STRING("                        2 - interlaced bottom field first\n")); 
    vm_string_printf(VM_STRING("                        1 - progressive (default)\n"));
    vm_string_printf(VM_STRING("                       -1 - unknown\n\n"));

    vm_string_printf(VM_STRING("   [-dw  width]      - width  of dst video (def: 352)\n"));
    vm_string_printf(VM_STRING("   [-dh  height]     - height of dst video (def: 288)\n"));
    vm_string_printf(VM_STRING("   [-dcrX  x]        - cropX  of dst video (def: 0)\n"));
    vm_string_printf(VM_STRING("   [-dcrY  y]        - cropY  of dst video (def: 0)\n"));
    vm_string_printf(VM_STRING("   [-dcrW  w]        - cropW  of dst video (def: width)\n"));
    vm_string_printf(VM_STRING("   [-dcrH  h]        - cropH  of dst video (def: height)\n"));
    vm_string_printf(VM_STRING("   [-df  frameRate]  - frame rate of dst video (def: 30.0)\n"));
    vm_string_printf(VM_STRING("   [-dcc format]     - format (FourCC) of dst video (def: nv12. support nv12|yuy2|rgb4|yv12)\n"));
    vm_string_printf(VM_STRING("   [-dbitshift 0|1]  - shift data to right or keep it the same way as in Microsoft's P010\n"));

    vm_string_printf(VM_STRING("   [-dpic value]     - picture structure of dst video\n")); 
    vm_string_printf(VM_STRING("                        0 - interlaced top    field first\n"));
    vm_string_printf(VM_STRING("                        2 - interlaced bottom field first\n")); 
    vm_string_printf(VM_STRING("                        1 - progressive (default)\n"));
    vm_string_printf(VM_STRING("                       -1 - unknown\n\n"));

    vm_string_printf(VM_STRING("   Video Enhancement Algorithms\n"));

    vm_string_printf(VM_STRING("   [-di_mode (mode)] - set type of deinterlace algorithm\n"));
    vm_string_printf(VM_STRING("                        8 - reverse telecine for a selected telecine pattern (use -tc_pattern). For PTIR plug-in\n"));
    vm_string_printf(VM_STRING("                        2 - advanced or motion adaptive (default)\n"));
    vm_string_printf(VM_STRING("                        1 - simple or BOB\n\n"));

    vm_string_printf(VM_STRING("   [-deinterlace (type)] - enable deinterlace algorithm (alternative way: -spic 0 -dpic 1) \n"));
    vm_string_printf(VM_STRING("                         type is tff (default) or bff \n"));

    vm_string_printf(VM_STRING("   [-vanalysis]        - enable video analysis algorithm \n"));
    vm_string_printf(VM_STRING("   [-variance]         - enable variance report algorithm \n"));
    vm_string_printf(VM_STRING("   [-idetect]          - enable picstruct detection algorithm \n"));
    vm_string_printf(VM_STRING("   [-rotate (angle)]   - enable rotation. Supported angles: 0, 90, 180, 270.\n"));
    vm_string_printf(VM_STRING("   [-denoise (level)]  - enable denoise algorithm. Level is optional \n"));
    vm_string_printf(VM_STRING("                         range of  noise level is [0, 100]\n"));
    vm_string_printf(VM_STRING("   [-detail  (level)]  - enable detail enhancement algorithm. Level is optional \n"));
    vm_string_printf(VM_STRING("                         range of detail level is [0, 100]\n\n"));
    vm_string_printf(VM_STRING("   [-pa_hue  hue]        - procamp hue property.         range [-180.0, 180.0] (def: 0.0)\n"));
    vm_string_printf(VM_STRING("   [-pa_sat  saturation] - procamp satursation property. range [   0.0,  10.0] (def: 1.0)\n"));
    vm_string_printf(VM_STRING("   [-pa_con  contrast]   - procamp contrast property.    range [   0.0,  10.0] (def: 1.0)\n"));
    vm_string_printf(VM_STRING("   [-pa_bri  brightness] - procamp brightness property.  range [-100.0, 100.0] (def: 0.0)\n\n"));
    vm_string_printf(VM_STRING("   [-gamut:compression]  - enable gamut compression algorithm (xvYCC->sRGB) \n"));
    vm_string_printf(VM_STRING("   [-gamut:bt709]        - enable BT.709 matrix transform (RGB->YUV conversion)(def: BT.601)\n\n"));
    vm_string_printf(VM_STRING("   [-frc:advanced]       - enable advanced FRC algorithm (based on PTS) \n"));
    vm_string_printf(VM_STRING("   [-frc:interp]         - enable FRC based on frame interpolation algorithm\n\n"));

    vm_string_printf(VM_STRING("   [-tcc:red]            - enable color saturation algorithm (R component) \n"));
    vm_string_printf(VM_STRING("   [-tcc:green]          - enable color saturation algorithm (G component)\n"));
    vm_string_printf(VM_STRING("   [-tcc:blue]           - enable color saturation algorithm (B component)\n"));
    vm_string_printf(VM_STRING("   [-tcc:cyan]           - enable color saturation algorithm (C component)\n"));
    vm_string_printf(VM_STRING("   [-tcc:magenta]        - enable color saturation algorithm (M component)\n"));
    vm_string_printf(VM_STRING("   [-tcc:yellow]         - enable color saturation algorithm (Y component)\n\n"));

    vm_string_printf(VM_STRING("   [-ace]                - enable auto contrast enhancement algorithm \n\n"));
    vm_string_printf(VM_STRING("   [-ste (level)]        - enable Skin Tone Enhancement algorithm.  Level is optional \n"));
    vm_string_printf(VM_STRING("                           range of ste level is [0, 9] (def: 4)\n\n"));
    vm_string_printf(VM_STRING("   [-istab (mode)]       - enable Image Stabilization algorithm.  Mode is optional \n"));
    vm_string_printf(VM_STRING("                           mode of istab can be [1, 2] (def: 2)\n"));
    vm_string_printf(VM_STRING("                           where: 1 means upscale mode, 2 means croppping mode\n"));
    vm_string_printf(VM_STRING("   [-view:count value]   - enable Multi View preprocessing. range of views [1, 1024] (def: 1)\n\n"));
    vm_string_printf(VM_STRING("   [-svc id width height]- enable Scalable Video Processing mode\n"));
    vm_string_printf(VM_STRING("                           id-layerId, width/height-resolution \n\n"));

    vm_string_printf(VM_STRING("   [-n frames] - number of frames to VPP process\n\n"));

    vm_string_printf(VM_STRING("   [-iopattern IN/OUT surface type] -  IN/OUT surface type: sys_to_sys, sys_to_d3d, d3d_to_sys, d3d_to_d3d    (def: sys_to_sys)\n"));
    vm_string_printf(VM_STRING("   [-sptr frame type] -  inpur or output allocated frames with predefined pointers. in - input surfaces, out - out surfaces, all input/output  (def: all frames with MemIDs)\n"));
    vm_string_printf(VM_STRING("   [-defalloc] - using of default allocator. Valid with -sptr all only \n"));
    vm_string_printf(VM_STRING("   [-async n] - maximum number of asynchronious tasks. def: -async 1 \n"));
    vm_string_printf(VM_STRING("   [-perf_opt n m] - n: number of prefetech frames. m : number of passes. In performance mode app preallocates bufer and load first n frames,  def: no performace 1 \n"));
    vm_string_printf(VM_STRING("   [-pts_check] - checking of time stampls. Default is OFF \n"));
    vm_string_printf(VM_STRING("   [-pts_jump ] - checking of time stamps jumps. Jump for random value since 13-th frame. Also, you can change input frame rate (via pts). Default frame_rate = sf \n"));
    vm_string_printf(VM_STRING("   [-pts_fr ]   - input frame rate which used for pts. Default frame_rate = sf \n"));
    vm_string_printf(VM_STRING("   [-pts_advanced]   - enable FRC checking mode based on PTS \n"));
    vm_string_printf(VM_STRING("   [-pf file for performance data] -  file to save performance data. Default is off \n\n\n"));

    vm_string_printf(VM_STRING("   [-roi_check mode seed1 seed2] - checking of ROI processing. Default is OFF \n"));
    vm_string_printf(VM_STRING("               mode - usage model of cropping\n"));
    vm_string_printf(VM_STRING("                      var_to_fix - variable input ROI and fixed output ROI\n")); 
    vm_string_printf(VM_STRING("                      fix_to_var - fixed input ROI and variable output ROI\n")); 
    vm_string_printf(VM_STRING("                      var_to_var - variable input ROI and variable output ROI\n")); 
    vm_string_printf(VM_STRING("               seed1 - seed for init of rand generator for src\n"));
    vm_string_printf(VM_STRING("               seed2 - seed for init of rand generator for dst\n"));
    vm_string_printf(VM_STRING("                       range of seed [1, 65535]. 0 reserved for random init\n\n"));

    vm_string_printf(VM_STRING("   [-tc_pattern (pattern)] - set telecine pattern\n"));
    vm_string_printf(VM_STRING("                        4 - provide a position inside a sequence of 5 frames where the artifacts starts. Use to -tc_pos to provide position\n"));
    vm_string_printf(VM_STRING("                        3 - 4:1 pattern\n"));
    vm_string_printf(VM_STRING("                        2 - frame repeat pattern\n"));
    vm_string_printf(VM_STRING("                        1 - 2:3:3:2 pattern\n"));
    vm_string_printf(VM_STRING("                        0 - 3:2 pattern\n\n"));

    vm_string_printf(VM_STRING("   [-tc_pos (position)] - Position inside a telecine sequence of 5 frames where the artifacts starts - Value [0 - 4]\n\n"));

    vm_string_printf(VM_STRING("\n"));

    vm_string_printf(VM_STRING("Usage2: %s -sw 352 -sh 144 -scc rgb3 -dw 320 -dh 240 -dcc nv12 -denoise 32 -vanalysis -iopattern d3d_to_d3d -i in.rgb -o out.yuv -roi_check var_to_fix 7 7\n"), strAppName);

    vm_string_printf(VM_STRING("\n"));

} // void vppPrintHelp(vm_char *strAppName, vm_char *strErrorMessage)


mfxU8 GetPicStruct( mfxI8 picStruct )
{  
    if ( 0 == picStruct ) 
    {
        return MFX_PICSTRUCT_FIELD_TFF;
    } 
    else if( 2 == picStruct )
    {
        return MFX_PICSTRUCT_FIELD_BFF;
    } 
    else if( -1 == picStruct )
    {
        return MFX_PICSTRUCT_UNKNOWN;
    } 
    else 
    {
        return MFX_PICSTRUCT_PROGRESSIVE;
    }

} // mfxU8 GetPicStruct( mfxI8 picStruct )


mfxU32 Str2FourCC( vm_char* strInput )
{
    mfxU32 fourcc = MFX_FOURCC_YV12;//default

    if ( 0 == vm_string_strcmp(strInput, VM_STRING("yv12")) ) 
    {
        fourcc = MFX_FOURCC_YV12;
    } 
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("rgb3")) ) 
    {
        fourcc = MFX_FOURCC_RGB3;
    } 
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("rgb4")) ) 
    {
        fourcc = MFX_FOURCC_RGB4;
    } 
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("yuy2")) )
    {
        fourcc = MFX_FOURCC_YUY2;
    } 
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("nv12")) ) 
    {
        fourcc = MFX_FOURCC_NV12;
    }
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("imc3")) ) 
    {
        fourcc = MFX_FOURCC_IMC3;
    }
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("yuv400")) ) 
    {
        fourcc = MFX_FOURCC_YUV400;
    }
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("yuv411")) ) 
    {
        fourcc = MFX_FOURCC_YUV411;
    }
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("yuv422h")) ) 
    {
        fourcc = MFX_FOURCC_YUV422H;
    }
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("yuv422v")) ) 
    {
        fourcc = MFX_FOURCC_YUV422V;
    }
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("yuv444")) ) 
    {
        fourcc = MFX_FOURCC_YUV444;
    }
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("p010")) ) 
    {
        fourcc = MFX_FOURCC_P010;
    }
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("p210")) ) 
    {
        fourcc = MFX_FOURCC_P210;
    }
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("nv16")) )
    {
        fourcc = MFX_FOURCC_NV16;
    }
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("a2rgb10")) )
    {
        fourcc = MFX_FOURCC_A2RGB10;
    }
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("uyvy")) )
    {
        fourcc = MFX_FOURCC_UYVY;
    }

    return fourcc;

} // mfxU32 Str2FourCC( vm_char* strInput )


eROIMode Str2ROIMode( vm_char* strInput )
{
    eROIMode mode;

    if ( 0 == vm_string_strcmp(strInput, VM_STRING("var_to_fix")) ) 
    {
        mode = ROI_VAR_TO_FIX;
    } 
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("var_to_var")) ) 
    {
        mode = ROI_VAR_TO_VAR;
    } 
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("fix_to_var")) ) 
    {
        mode = ROI_FIX_TO_VAR;
    }
    else
    {
        mode = ROI_FIX_TO_FIX; // default mode
    }

    return mode;

} // eROIMode Str2ROIMode( vm_char* strInput )


static mfxU16 Str2IOpattern( vm_char* strInput )
{
    mfxU16 IOPattern = 0;

    if ( 0 == vm_string_strcmp(strInput, VM_STRING("d3d_to_d3d")) ) 
    {
        IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    } 
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("d3d_to_sys")) ) 
    {
        IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    } 
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("sys_to_d3d")) ) 
    {
        IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    } 
    else if ( 0 == vm_string_strcmp(strInput, VM_STRING("sys_to_sys")) )
    {
        IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    } 
    return IOPattern;

} // static mfxU16 Str2IOpattern( vm_char* strInput )

static
void vppDefaultInitParams( sInputParams* pParams )
{
    pParams->frameInfo[VPP_IN] = defaultOwnFrameInfo;
    pParams->frameInfo[VPP_OUT] = defaultOwnFrameInfo;

    pParams->IOPattern    = MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    pParams->ImpLib       = MFX_IMPL_AUTO;
    pParams->sptr         = NO_PTR;
    pParams->bDefAlloc   = false;
    pParams->asyncNum     = 1;
    pParams->bPerf       = false;
    pParams->isOutput     = false;
    pParams->ptsCheck     = false;
    pParams->ptsFR        = 0;
    pParams->vaType       = ALLOC_IMPL_VIA_SYS;
    pParams->rotate       = 0;

    // Optional video processing features
    pParams->denoiseParam = defaultDenoiseParam;
    pParams->detailParam  = defaultDetailParam;
    pParams->procampParam = defaultProcAmpParam;
      // analytics
    pParams->vaParam      = defaultVAParam;
    pParams->varianceParam= defaultVarianceParam; 
    pParams->idetectParam = defaultIDetectParam; 
    pParams->frcParam     = defaultFRCParam;
    // MSDK 3.0
    pParams->multiViewParam     = defaultMultiViewParam;
    // MSDK API 1.5
    pParams->gamutParam         = defaultGamutParam;
    pParams->tccParam      = defaultClrSaturationParam;
    pParams->aceParam      = defaultContrastParam;
    pParams->steParam          = defaultSkinParam;
    pParams->istabParam       = defaultImgStabParam;

    //SVC
    memset( (void*)&(pParams->svcParam), 0, sizeof(sSVCParam));
    pParams->svcParam.mode = VPP_FILTER_DISABLED;

    // ROI check
    pParams->roiCheckParam.mode = ROI_FIX_TO_FIX; // ROI check is disabled
    pParams->roiCheckParam.srcSeed = 0;
    pParams->roiCheckParam.dstSeed = 0;
    pParams->isOutYV12 = false;
    pParams->need_crc  = false;

    // plug-in GUID
    pParams->need_plugin = false;

    // Use RunFrameVPPAsyncEx
    pParams->use_extapi  = false;

    pParams->bInitEx      = false;
    pParams->GPUCopyValue = MFX_GPUCOPY_DEFAULT;

    pParams->frameInfo[VPP_IN].CropX = 0;
    pParams->frameInfo[VPP_IN].CropY = 0;
    pParams->frameInfo[VPP_IN].CropW = NOT_INIT_VALUE;
    pParams->frameInfo[VPP_IN].CropH = NOT_INIT_VALUE;
    // zeroize destination crops
    pParams->frameInfo[VPP_OUT].CropX = 0;
    pParams->frameInfo[VPP_OUT].CropY = 0;
    pParams->frameInfo[VPP_OUT].CropW = NOT_INIT_VALUE;
    pParams->frameInfo[VPP_OUT].CropH = NOT_INIT_VALUE;
    return;

} // void vppDefaultInitParams( sInputParams* pParams )

mfxStatus ZeroInputParams(sInputParams *params)
{
    CHECK_POINTER(params, MFX_ERR_NULL_PTR);
    
    return MFX_ERR_NONE;
}

mfxStatus vppParseInputString(vm_char* strInput[], mfxU8 nArgNum, std::vector<sInputParams *> &pParams)
{
    //CHECK_POINTER(pParams,  MFX_ERR_NULL_PTR);
    CHECK_POINTER(strInput, MFX_ERR_NULL_PTR);

    mfxU32 readData;
    mfxU32 ioStatus;
    mfxU32 linkIndex = 0;
    if (nArgNum < 4)
    {
        vppPrintHelp(strInput[0], VM_STRING("Not enough parameters"));

        return MFX_ERR_MORE_DATA;
    }

    sInputParams *baseParams = new sInputParams;
    CHECK_POINTER(baseParams, MFX_ERR_NULL_PTR);
    vppDefaultInitParams(baseParams);
    
    pParams.push_back(baseParams);

    bool isD3D11Required = false;

    for (mfxU8 i = 1; i < nArgNum; i++ ) 
    {
        CHECK_POINTER(strInput[i], MFX_ERR_NULL_PTR);
        {      
            if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-gpu_copy")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                pParams[linkIndex]->bInitEx = true;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->GPUCopyValue);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-link")) )
            {
                linkIndex++;
                sInputParams *newParams = new sInputParams;
                CHECK_POINTER(newParams, MFX_ERR_NULL_PTR);
                vppDefaultInitParams(newParams);
                pParams.push_back(newParams);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-sw")) ) 
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_IN].nWidth);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-sh")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_IN].nHeight);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-scrX")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_IN].CropX);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-scrY")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_IN].CropY);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-scrW")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_IN].CropW);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-scrH")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_IN].CropH);
            } 
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-spic")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), reinterpret_cast<short int*>(&pParams[linkIndex]->frameInfo[VPP_IN].PicStruct));
                pParams[linkIndex]->frameInfo[VPP_IN].PicStruct = GetPicStruct(pParams[linkIndex]->frameInfo[VPP_IN].PicStruct);
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-sf")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams[linkIndex]->frameInfo[VPP_IN].dFrameRate);          
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dw")) ) 
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_OUT].nWidth);         
            } 
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dh")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_OUT].nHeight);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dcrX")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_OUT].CropX);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dcrY")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_OUT].CropY);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dcrW")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_OUT].CropW);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dcrH")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_OUT].CropH);
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-dpic")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), reinterpret_cast<short int*>(&pParams[linkIndex]->frameInfo[VPP_OUT].PicStruct));
                pParams[linkIndex]->frameInfo[VPP_OUT].PicStruct = GetPicStruct(pParams[linkIndex]->frameInfo[VPP_OUT].PicStruct);
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-df")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams[linkIndex]->frameInfo[VPP_OUT].dFrameRate);          
            }          
            //-----------------------------------------------------------------------------------
            //                   Video Enhancement Algorithms
            //-----------------------------------------------------------------------------------
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-denoise")))
            {
                pParams[linkIndex]->denoiseParam.mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {                
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), reinterpret_cast<short int*>(&readData));
                    if ( ioStatus > 0 )
                    {
                        pParams[linkIndex]->denoiseParam.factor = (mfxU16)readData;
                        pParams[linkIndex]->denoiseParam.mode   = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;
                    }
                }
            }
            // aya: altenative and simple way to enable deinterlace
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-deinterlace")))
            {
                pParams[linkIndex]->frameInfo[VPP_OUT].PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
                pParams[linkIndex]->frameInfo[VPP_IN].PicStruct  = MFX_PICSTRUCT_FIELD_TFF;

                if( i+1 < nArgNum )
                {                
                    if(0 == vm_string_strcmp(strInput[i+1], VM_STRING("bff")))
                    {
                        pParams[linkIndex]->frameInfo[VPP_OUT].PicStruct = MFX_PICSTRUCT_FIELD_BFF;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-di_mode")))
            {
                pParams[linkIndex]->deinterlaceParam.mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), reinterpret_cast<short int*>(&readData));
                    if ( ioStatus > 0 )
                    {
                        pParams[linkIndex]->deinterlaceParam.algorithm = (mfxU16)readData;
                        pParams[linkIndex]->deinterlaceParam.mode   = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tc_pattern")))
            {
                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), reinterpret_cast<short int*>(&readData));
                    if ( ioStatus > 0 )
                    {
                        pParams[linkIndex]->deinterlaceParam.tc_pattern   = (mfxU16)readData;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tc_pos")))
            {
                //pParams->deinterlaceParam.mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), reinterpret_cast<short int*>(&readData));
                    if ( ioStatus > 0 )
                    {
                        pParams[linkIndex]->deinterlaceParam.tc_pos   = (mfxU16)readData;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-detail")))
            {
                pParams[linkIndex]->detailParam.mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), reinterpret_cast<short int*>(&readData));
                    if ( ioStatus > 0 )
                    {
                        pParams[linkIndex]->detailParam.factor = (mfxU16)readData;
                        pParams[linkIndex]->detailParam.mode   = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-vanalysis")))
            {
                pParams[linkIndex]->vaParam.mode = VPP_FILTER_ENABLED_DEFAULT;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-rotate")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->rotate);
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-variance")))
            {
                pParams[linkIndex]->varianceParam.mode = VPP_FILTER_ENABLED_DEFAULT;
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-idetect")))
            {
                pParams[linkIndex]->idetectParam.mode = VPP_FILTER_ENABLED_DEFAULT;
            }
            // different modes of MFX FRC
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-frc:advanced")))
            {
                pParams[linkIndex]->frcParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                pParams[linkIndex]->frcParam.algorithm = MFX_FRCALGM_DISTRIBUTED_TIMESTAMP;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-frc:interp")))
            {
                pParams[linkIndex]->frcParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                pParams[linkIndex]->frcParam.algorithm = MFX_FRCALGM_FRAME_INTERPOLATION;
            }
            //---------------------------------------------
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pa_hue")))
            {
                pParams[linkIndex]->procampParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams[linkIndex]->procampParam.hue);    
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pa_bri")))
            {
                pParams[linkIndex]->procampParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams[linkIndex]->procampParam.brightness);    
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pa_con")))
            {
                pParams[linkIndex]->procampParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams[linkIndex]->procampParam.contrast);    
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pa_sat")))
            {
                pParams[linkIndex]->procampParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams[linkIndex]->procampParam.saturation);    
            }

            //MSDK 3.0
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-gamut:compression")))
            {
                pParams[linkIndex]->gamutParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-gamut:bt709")))
            {
                pParams[linkIndex]->gamutParam.bBT709 = true;
            }
            else if( 0 == vm_string_strcmp(strInput[i], VM_STRING("-view:count")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;

                mfxU16 viewCount;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &viewCount);
                if( viewCount > 1 )
                {
                    pParams[linkIndex]->multiViewParam.viewCount = (mfxU16)viewCount;
                    pParams[linkIndex]->multiViewParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                }
            }
            //---------------------------------------------
            // Scalable Video Mode
            else if( 0 == vm_string_strcmp(strInput[i], VM_STRING("-svc")) )
            {
                if( 3 + i > nArgNum )
                {
                    vppPrintHelp(strInput[0], VM_STRING("invalid SVC configuration"));
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }
                mfxU16 layer;
                vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &layer);
                if( layer < 8 )
                {
                    sSVCLayerDescr descr = {1, 0, 0};//active
                    vm_string_sscanf(strInput[i+2], VM_STRING("%hd"), &descr.width);
                    vm_string_sscanf(strInput[i+3], VM_STRING("%hd"), &descr.height);

                    pParams[linkIndex]->svcParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                    pParams[linkIndex]->svcParam.descr[layer] = descr;

                    i += 3;
                }
                else
                {
                    vppPrintHelp(strInput[0], VM_STRING("Invalid SVC configuration"));
                    return MFX_ERR_UNSUPPORTED;
                }
            }
            //---------------------------------------------
            // MSDK API 1.5
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-istab")))
            {
                pParams[linkIndex]->istabParam.mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), reinterpret_cast<short int*>(&readData));
                    if ( ioStatus > 0 )
                    {
                        pParams[linkIndex]->istabParam.istabMode = (mfxU8)readData;
                        pParams[linkIndex]->istabParam.mode    = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;

                        if( pParams[linkIndex]->istabParam.istabMode != 1 && pParams[linkIndex]->istabParam.istabMode != 2 )
                        {
                            vppPrintHelp(strInput[0], VM_STRING("Invalid IStab configuration"));
                            return MFX_ERR_UNSUPPORTED;
                        }
                    }
                }
            }
            //---------------------------------------------
            // IECP
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-ace")))
            {
                pParams[linkIndex]->aceParam.mode = VPP_FILTER_ENABLED_DEFAULT;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-ste")))
            {
                pParams[linkIndex]->steParam.mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), reinterpret_cast<short int*>(&readData));
                    if ( ioStatus > 0 )
                    {
                        pParams[linkIndex]->steParam.SkinToneFactor = (mfxU8)readData;
                        pParams[linkIndex]->steParam.mode           = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:red")))
            {
                pParams[linkIndex]->tccParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), reinterpret_cast<double*>(&pParams[linkIndex]->tccParam.Red));
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:green")))
            {
                pParams[linkIndex]->tccParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), reinterpret_cast<double*>(&pParams[linkIndex]->tccParam.Green));
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:blue")))
            {
                pParams[linkIndex]->tccParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), reinterpret_cast<double*>(&pParams[linkIndex]->tccParam.Blue));
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:magenta")))
            {
                pParams[linkIndex]->tccParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), reinterpret_cast<double*>(&pParams[linkIndex]->tccParam.Magenta));
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:yellow")))
            {
                pParams[linkIndex]->tccParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), reinterpret_cast<double*>(&pParams[linkIndex]->tccParam.Yellow));
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:cyan")))
            {
                pParams[linkIndex]->tccParam.mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), reinterpret_cast<double*>(&pParams[linkIndex]->tccParam.Cyan));
            }
            //-----------------------------------------------------------------------------------
            //                   Region of Interest Testing
            //-----------------------------------------------------------------------------------
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-roi_check")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                pParams[linkIndex]->roiCheckParam.mode = Str2ROIMode( strInput[i] );

                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), reinterpret_cast<short int*>(&pParams[linkIndex]->roiCheckParam.srcSeed));

                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), reinterpret_cast<short int*>(&pParams[linkIndex]->roiCheckParam.dstSeed));
            }
            //-----------------------------------------------------------------------------------
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-i")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_strcpy(pParams[linkIndex]->strSrcFile, strInput[i]);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-crc")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_strcpy(pParams[linkIndex]->strCRCFile, strInput[i]);
                pParams[linkIndex]->need_crc = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-o")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_strcpy(pParams[linkIndex]->strDstFile, strInput[i]);
                pParams[linkIndex]->isOutput = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pf")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_strcpy(pParams[linkIndex]->strPerfFile, strInput[i]);

            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-scc")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                pParams[linkIndex]->frameInfo[VPP_IN].FourCC = Str2FourCC( strInput[i] );
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dcc")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                pParams[linkIndex]->frameInfo[VPP_OUT].FourCC = Str2FourCC( strInput[i] );
                pParams[linkIndex]->isOutYV12 = false;
                if(MFX_FOURCC_YV12 == pParams[linkIndex]->frameInfo[VPP_OUT].FourCC)
                {
                    pParams[linkIndex]->frameInfo[VPP_OUT].FourCC = MFX_FOURCC_NV12;
                    pParams[linkIndex]->isOutYV12 = true;
                }
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-dbitshift")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_OUT].Shift);
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-sbitshift")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->frameInfo[VPP_IN].Shift);
            }
            else if( 0 == vm_string_strcmp(strInput[i], VM_STRING("-iopattern")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                pParams[linkIndex]->IOPattern = Str2IOpattern( strInput[i] );
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-lib")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                if (0 == vm_string_strcmp(strInput[i], VM_STRING("sw")) )
                    pParams[linkIndex]->ImpLib = MFX_IMPL_SOFTWARE;
                else if (0 == vm_string_strcmp(strInput[i], VM_STRING("hw")) )
                {
                    //pParams->ImpLib = (isD3D11Required) ? (MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11): (MFX_IMPL_HARDWARE|MFX_IMPL_VIA_D3D9);
                    pParams[linkIndex]->ImpLib = MFX_IMPL_HARDWARE;
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-d3d11")) )
            {
                //pParams->ImpLib = MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11;
                isD3D11Required = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-sptr")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                if (0 == vm_string_strcmp(strInput[i], VM_STRING("in")) )
                    pParams[linkIndex]->sptr = INPUT_PTR;
                else if (0 == vm_string_strcmp(strInput[i], VM_STRING("out")) )
                    pParams[linkIndex]->sptr = OUTPUT_PTR;
                else if (0 == vm_string_strcmp(strInput[i], VM_STRING("all")) )
                    pParams[linkIndex]->sptr = ALL_PTR;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-defalloc")) )
            {
                pParams[linkIndex]->bDefAlloc = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-async")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->asyncNum);

            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-perf_opt")) )
            {
                if (pParams[linkIndex]->numFrames)
                    return MFX_ERR_UNKNOWN;

                VAL_CHECK(1 + i == nArgNum);
                pParams[linkIndex]->bPerf = true;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), reinterpret_cast<short int*>(&pParams[linkIndex]->numFrames));
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams[linkIndex]->numRepeat);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pts_check")) )
            {
                pParams[linkIndex]->ptsCheck = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pts_jump")) )
            {
                pParams[linkIndex]->ptsJump = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pts_fr")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams[linkIndex]->ptsFR);  
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pts_advanced")) )
            {
                pParams[linkIndex]->ptsAdvanced = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-n")) )
            {
                if (pParams[linkIndex]->bPerf)
                    return MFX_ERR_UNKNOWN;

                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), reinterpret_cast<short int*>(&pParams[linkIndex]->numFrames));

            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-plugin_guid")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_strcpy(pParams[linkIndex]->strPlgGuid, strInput[i]);
                pParams[linkIndex]->need_plugin = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-extapi")) )
            {
                pParams[linkIndex]->use_extapi = true;
            }
            else
            {
                vm_string_printf(VM_STRING("Unknow option: %s\n"), strInput[i]);

                return MFX_ERR_UNKNOWN;
            }
        }
    }

    /*if ((pParams->ImpLib & MFX_IMPL_HARDWARE) && !(pParams->ImpLib & MFX_IMPL_VIA_D3D11))
    {
    pParams->ImpLib = MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D9;
    }*/

    for ( linkIndex = 0; linkIndex < pParams.size(); linkIndex++)
    {
        if (NOT_INIT_VALUE == pParams[linkIndex]->frameInfo[VPP_IN].CropW)
        {
            pParams[linkIndex]->frameInfo[VPP_IN].CropW = pParams[linkIndex]->frameInfo[VPP_IN].nWidth;
        }

        if (NOT_INIT_VALUE == pParams[linkIndex]->frameInfo[VPP_IN].CropH)
        {
            pParams[linkIndex]->frameInfo[VPP_IN].CropH = pParams[linkIndex]->frameInfo[VPP_IN].nHeight;
        }

        if (NOT_INIT_VALUE == pParams[linkIndex]->frameInfo[VPP_OUT].CropW)
        {
            pParams[linkIndex]->frameInfo[VPP_OUT].CropW = pParams[linkIndex]->frameInfo[VPP_OUT].nWidth;
        }

        if (NOT_INIT_VALUE == pParams[linkIndex]->frameInfo[VPP_OUT].CropH)
        {
            pParams[linkIndex]->frameInfo[VPP_OUT].CropH = pParams[linkIndex]->frameInfo[VPP_OUT].nHeight;
        }

        if (linkIndex  == 0 && 0 == vm_string_strlen(pParams[linkIndex]->strSrcFile)) 
        {
            vppPrintHelp(strInput[0], VM_STRING("Source file name not found"));
            return MFX_ERR_UNSUPPORTED;
        };

        if (linkIndex + 1 == pParams.size() && 0 == vm_string_strlen(pParams[linkIndex]->strDstFile)) 
        {
            vppPrintHelp(strInput[0], VM_STRING("Destination file name not found"));
            return MFX_ERR_UNSUPPORTED;
        };

        if (0 == pParams[linkIndex]->IOPattern)
        {
            vppPrintHelp(strInput[0], VM_STRING("Incorrect IOPattern"));
            return MFX_ERR_UNSUPPORTED;
        }

        // aya: fix for windows only
        if( pParams[linkIndex]->IOPattern != (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY) )
        {
            pParams[linkIndex]->vaType = isD3D11Required ? ALLOC_IMPL_VIA_D3D11 : ALLOC_IMPL_VIA_D3D9;
        }

        if (pParams[linkIndex]->ImpLib & MFX_IMPL_HARDWARE) 
        {
    #if defined(_WIN32) || defined(_WIN64)
            pParams[linkIndex]->ImpLib |= (isD3D11Required)? MFX_IMPL_VIA_D3D11 : MFX_IMPL_VIA_D3D9;
    #elif defined (LIBVA_SUPPORT)
            pParams[linkIndex]->ImpLib |= MFX_IMPL_VIA_VAAPI;
    #endif
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus vppParseInputString( ... )

bool CheckInputParams(vm_char* strInput[], sInputParams* pParams )
{
    if ((pParams->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) &&
        (pParams->sptr & INPUT_PTR))
    {
        vppPrintHelp(strInput[0], VM_STRING("Incompatible parameters: [IOpattern and sptr]\n"));
        return false;
    }
    if ((pParams->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) &&
        (pParams->sptr & OUTPUT_PTR))
    {
        vppPrintHelp(strInput[0], VM_STRING("Incompatible parameters: [IOpattern and sptr]\n"));
        return false;
    }
    if (pParams->sptr != ALL_PTR && pParams->bDefAlloc)
    {
        vppPrintHelp(strInput[0], VM_STRING("Incompatible parameters: [defalloc and sptr]\n"));
        return false;
    }
    if (0 == pParams->asyncNum)
    {
        vppPrintHelp(strInput[0], VM_STRING("Incompatible parameters: [ayncronous number must exceed 0]\n"));
        return false;
    }

    if (pParams->rotate != 0 && pParams->rotate != 90 && pParams->rotate != 180 && pParams->rotate != 270)
    {
        vppPrintHelp(strInput[0], VM_STRING("Invalid -rotate parameter: supported values 0, 90, 180, 270\n"));
        return false;
    }

    return true;

} // bool CheckInputParams(vm_char* strInput[], sInputVppParams* pParams )

/* EOF */
