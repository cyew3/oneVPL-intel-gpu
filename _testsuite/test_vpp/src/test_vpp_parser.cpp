//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 - 2013 Intel Corporation. All Rights Reserved.
//

#include "test_vpp_utils.h"
#include "mfxvideo++int.h"

#define VAL_CHECK(val) {if (val) return MFX_ERR_UNKNOWN;}

void vppPrintHelp(vm_char *strAppName, vm_char *strErrorMessage)
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
    vm_string_printf(VM_STRING("   [-gpu_copy]         - Specify GPU copy mode. This option triggers using of InitEX instead of Init.\n\n"));

    vm_string_printf(VM_STRING("   [-sw   width]     - width  of src video (def: 352)\n"));
    vm_string_printf(VM_STRING("   [-sh   height]    - height of src video (def: 288)\n"));
    vm_string_printf(VM_STRING("   [-scrX  x]        - cropX  of src video (def: 0)\n"));
    vm_string_printf(VM_STRING("   [-scrY  y]        - cropY  of src video (def: 0)\n"));
    vm_string_printf(VM_STRING("   [-scrW  w]        - cropW  of src video (def: width)\n"));
    vm_string_printf(VM_STRING("   [-scrH  h]        - cropH  of src video (def: height)\n"));
    vm_string_printf(VM_STRING("   [-sf   frameRate] - frame rate of src video (def: 30.0)\n"));
    vm_string_printf(VM_STRING("   [-scc  format]    - format (FourCC) of src video (def: nv12. support nv12|yv12|yuy2|rgb3|rgb4|imc3|yuv400|yuv411|yuv422h|yuv422v|yuv444|uyvy)\n"));
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
    vm_string_printf(VM_STRING("   [-scaling_mode (mode)] - specify type of scaling to be used for resize.\n"));
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

    vm_string_printf(VM_STRING("   [-reset_start (frame number)] - after reaching this frame, encoder will be reset with new parameters, followed after this command and before -reset_end \n"));
    vm_string_printf(VM_STRING("   [-reset_end]                  - specifies end of reset related options \n\n"));

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
    mfxU32 fourcc = 0;//default

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

mfxStatus vppParseResetPar(vm_char* strInput[], mfxU8 nArgNum, mfxU8& curArg, sInputParams* pParams, mfxU32 paramID, sFiltersParam* pDefaultFiltersParam)
{
    CHECK_POINTER(pParams,  MFX_ERR_NULL_PTR);
    CHECK_POINTER(strInput, MFX_ERR_NULL_PTR);

    sOwnFrameInfo info = pParams->frameInfoIn.back();
    pParams->frameInfoIn.push_back(info);
    info = pParams->frameInfoOut.back();
    pParams->frameInfoOut.push_back(info);

    pParams->deinterlaceParam.push_back(    *pDefaultFiltersParam->pDIParam            );
    pParams->denoiseParam.push_back(        *pDefaultFiltersParam->pDenoiseParam       );
    pParams->detailParam.push_back(         *pDefaultFiltersParam->pDetailParam        );
    pParams->procampParam.push_back(        *pDefaultFiltersParam->pProcAmpParam       );
    pParams->vaParam.push_back(             *pDefaultFiltersParam->pVAParam            );
    pParams->varianceParam.push_back(       *pDefaultFiltersParam->pVarianceParam      );
    pParams->idetectParam.push_back(        *pDefaultFiltersParam->pIDetectParam       );
    pParams->frcParam.push_back(            *pDefaultFiltersParam->pFRCParam           );
    pParams->multiViewParam.push_back(      *pDefaultFiltersParam->pMultiViewParam     );
    pParams->gamutParam.push_back(          *pDefaultFiltersParam->pGamutParam         );
    pParams->tccParam.push_back(            *pDefaultFiltersParam->pClrSaturationParam );
    pParams->aceParam.push_back(            *pDefaultFiltersParam->pContrastParam      );
    pParams->steParam.push_back(            *pDefaultFiltersParam->pSkinParam          );
    pParams->istabParam.push_back(          *pDefaultFiltersParam->pImgStabParam       );
    pParams->svcParam.push_back(            *pDefaultFiltersParam->pSVCParam           );
    pParams->videoSignalInfoParam.push_back(*pDefaultFiltersParam->pVideoSignalInfo    );
    pParams->rotate.push_back(               0                                         );

    mfxU32 readData;
    mfxU32 ioStatus;

    for (mfxU8& i = curArg; i < nArgNum; i++ ) 
    {
        CHECK_POINTER(strInput[i], MFX_ERR_NULL_PTR);
        {
            //-----------------------------------------------------------------------------------
            //                   Video Enhancement Algorithms
            //-----------------------------------------------------------------------------------
            if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-ssinr")) )
            {
                VAL_CHECK(1 + i == nArgNum);

                pParams->videoSignalInfoParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;

                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->videoSignalInfoParam[paramID].In.NominalRange);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-dsinr")) )
            {
                VAL_CHECK(1 + i == nArgNum);

                pParams->videoSignalInfoParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;

                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->videoSignalInfoParam[paramID].Out.NominalRange);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-ssitm")) )
            {
                VAL_CHECK(1 + i == nArgNum);

                pParams->videoSignalInfoParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;

                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->videoSignalInfoParam[paramID].In.TransferMatrix);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-dsitm")) )
            {
                VAL_CHECK(1 + i == nArgNum);

                pParams->videoSignalInfoParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;

                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->videoSignalInfoParam[paramID].Out.TransferMatrix);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-sw")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoIn.back().nWidth);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-dw")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoOut.back().nWidth);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-sh")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoIn.back().nHeight);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-dh")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoOut.back().nHeight);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-denoise")))
            {
                pParams->denoiseParam[paramID].mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {                
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);                
                    if ( ioStatus > 0 )
                    {
                        pParams->denoiseParam[paramID].factor = (mfxU16)readData;
                        pParams->denoiseParam[paramID].mode   = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-di_mode")))
            {
                pParams->deinterlaceParam[paramID].mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                    if ( ioStatus > 0 )
                    {
                        pParams->deinterlaceParam[paramID].algorithm = (mfxU16)readData;
                        pParams->deinterlaceParam[paramID].mode   = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tc_pattern")))
            {
                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                    if ( ioStatus > 0 )
                    {
                        pParams->deinterlaceParam[paramID].tc_pattern   = (mfxU16)readData;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tc_pos")))
            {
                //pParams->deinterlaceParam.mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                    if ( ioStatus > 0 )
                    {
                        pParams->deinterlaceParam[paramID].tc_pos   = (mfxU16)readData;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-detail")))
            {
                pParams->detailParam[paramID].mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                    if ( ioStatus > 0 )
                    {
                        pParams->detailParam[paramID].factor = (mfxU16)readData;
                        pParams->detailParam[paramID].mode   = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-vanalysis")))
            {
                pParams->vaParam[paramID].mode = VPP_FILTER_ENABLED_DEFAULT;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-rotate")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->rotate[paramID]);
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-variance")))
            {
                pParams->varianceParam[paramID].mode = VPP_FILTER_ENABLED_DEFAULT;
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-idetect")))
            {
                pParams->idetectParam[paramID].mode = VPP_FILTER_ENABLED_DEFAULT;
            }
            // different modes of MFX FRC
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-frc:advanced")))
            {
                pParams->frcParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
                pParams->frcParam[paramID].algorithm = MFX_FRCALGM_DISTRIBUTED_TIMESTAMP;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-frc:interp")))
            {
                pParams->frcParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
                pParams->frcParam[paramID].algorithm = MFX_FRCALGM_FRAME_INTERPOLATION;
            }
            //---------------------------------------------
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pa_hue")))
            {
                pParams->procampParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->procampParam[paramID].hue);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pa_bri")))
            {
                pParams->procampParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->procampParam[paramID].brightness);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pa_con")))
            {
                pParams->procampParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->procampParam[paramID].contrast);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pa_sat")))
            {
                pParams->procampParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->procampParam[paramID].saturation);
            }

            //MSDK 3.0
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-gamut:compression")))
            {
                pParams->gamutParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-gamut:bt709")))
            {
                pParams->gamutParam[paramID].bBT709 = true;
            }
            else if( 0 == vm_string_strcmp(strInput[i], VM_STRING("-view:count")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;

                mfxU16 viewCount;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &viewCount);
                if( viewCount > 1 )
                {
                    pParams->multiViewParam[paramID].viewCount = (mfxU16)viewCount;
                    pParams->multiViewParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
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

                    pParams->svcParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
                    pParams->svcParam[paramID].descr[layer] = descr;

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
                pParams->istabParam[paramID].mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                    if ( ioStatus > 0 )
                    {
                        pParams->istabParam[paramID].istabMode = (mfxU8)readData;
                        pParams->istabParam[paramID].mode    = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;

                        if( pParams->istabParam[paramID].istabMode != 1 && pParams->istabParam[paramID].istabMode != 2 )
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
                pParams->aceParam[paramID].mode = VPP_FILTER_ENABLED_DEFAULT;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-ste")))
            {
                pParams->steParam[paramID].mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                    if ( ioStatus > 0 )
                    {
                        pParams->steParam[paramID].SkinToneFactor = (mfxU8)readData;
                        pParams->steParam[paramID].mode           = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:red")))
            {
                pParams->tccParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->tccParam[paramID].Red);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:green")))
            {
                pParams->tccParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->tccParam[paramID].Green);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:blue")))
            {
                pParams->tccParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->tccParam[paramID].Blue);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:magenta")))
            {
                pParams->tccParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->tccParam[paramID].Magenta);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:yellow")))
            {
                pParams->tccParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->tccParam[paramID].Yellow);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:cyan")))
            {
                pParams->tccParam[paramID].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->tccParam[paramID].Cyan);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-reset_end")))
            {
                break;
            }
            else
            {
                vm_string_printf(VM_STRING("Unknow reset option: %s\n"), strInput[i]);

                return MFX_ERR_UNKNOWN;
            }
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus vppParseResetPar( ... )

mfxStatus vppParseInputString(vm_char* strInput[], mfxU8 nArgNum, sInputParams* pParams, sFiltersParam* pDefaultFiltersParam)
{
    CHECK_POINTER(pParams,  MFX_ERR_NULL_PTR);
    CHECK_POINTER(strInput, MFX_ERR_NULL_PTR);

    mfxU32 readData;
    mfxU32 ioStatus;
    if (nArgNum < 4)
    {
        vppPrintHelp(strInput[0], VM_STRING("Not enough parameters"));

        return MFX_ERR_MORE_DATA;
    }

    pParams->frameInfoIn.back().CropX = 0;
    pParams->frameInfoIn.back().CropY = 0;
    pParams->frameInfoIn.back().CropW = NOT_INIT_VALUE;
    pParams->frameInfoIn.back().CropH = NOT_INIT_VALUE;
    // zeroize destination crops
    pParams->frameInfoOut.back().CropX = 0;
    pParams->frameInfoOut.back().CropY = 0;
    pParams->frameInfoOut.back().CropW = NOT_INIT_VALUE;
    pParams->frameInfoOut.back().CropH = NOT_INIT_VALUE;

    bool isD3D11Required = false;

    for (mfxU8 i = 1; i < nArgNum; i++ ) 
    {
        CHECK_POINTER(strInput[i], MFX_ERR_NULL_PTR);
        {      
            if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-ssinr")) )
            {
                VAL_CHECK(1 + i == nArgNum);

                pParams->videoSignalInfoParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;

                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->videoSignalInfoParam[0].In.NominalRange);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-dsinr")) )
            {
                VAL_CHECK(1 + i == nArgNum);

                pParams->videoSignalInfoParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;

                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->videoSignalInfoParam[0].Out.NominalRange);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-ssitm")) )
            {
                VAL_CHECK(1 + i == nArgNum);

                pParams->videoSignalInfoParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;

                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->videoSignalInfoParam[0].In.TransferMatrix);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-dsitm")) )
            {
                VAL_CHECK(1 + i == nArgNum);

                pParams->videoSignalInfoParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;

                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->videoSignalInfoParam[0].Out.TransferMatrix);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-gpu_copy")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                pParams->bInitEx = true;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->GPUCopyValue);
            }
            else if ( 0 == vm_string_strcmp(strInput[i], VM_STRING("-sw")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoIn[0].nWidth);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-sh")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoIn[0].nHeight);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-scrX")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoIn[0].CropX);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-scrY")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoIn[0].CropY);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-scrW")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoIn[0].CropW);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-scrH")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoIn[0].CropH);
            } 
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-spic")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoIn[0].PicStruct);
                pParams->frameInfoIn[0].PicStruct = GetPicStruct(pParams->frameInfoIn[0].PicStruct);
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-sf")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->frameInfoIn[0].dFrameRate);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dw")) ) 
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoOut[0].nWidth);
            } 
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dh")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoOut[0].nHeight);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dcrX")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoOut[0].CropX);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dcrY")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoOut[0].CropY);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dcrW")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoOut[0].CropW);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dcrH")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoOut[0].CropH);
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-dpic")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoOut[0].PicStruct);
                pParams->frameInfoOut[0].PicStruct = GetPicStruct(pParams->frameInfoOut[0].PicStruct);
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-df")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->frameInfoOut[0].dFrameRate);
            }          
            //-----------------------------------------------------------------------------------
            //                   Video Enhancement Algorithms
            //-----------------------------------------------------------------------------------
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-denoise")))
            {
                pParams->denoiseParam[0].mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                    if ( ioStatus > 0 )
                    {
                        pParams->denoiseParam[0].factor = (mfxU16)readData;
                        pParams->denoiseParam[0].mode   = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;
                    }
                }
            }
            // aya: altenative and simple way to enable deinterlace
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-deinterlace")))
            {
                pParams->frameInfoOut[0].PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
                pParams->frameInfoIn[0].PicStruct  = MFX_PICSTRUCT_FIELD_TFF;

                if( i+1 < nArgNum )
                {                
                    if(0 == vm_string_strcmp(strInput[i+1], VM_STRING("bff")))
                    {
                        pParams->frameInfoOut[0].PicStruct = MFX_PICSTRUCT_FIELD_BFF;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-di_mode")))
            {
                pParams->deinterlaceParam[0].mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                    if ( ioStatus > 0 )
                    {
                        pParams->deinterlaceParam[0].algorithm = (mfxU16)readData;
                        pParams->deinterlaceParam[0].mode   = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tc_pattern")))
            {
                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                    if ( ioStatus > 0 )
                    {
                        pParams->deinterlaceParam[0].tc_pattern   = (mfxU16)readData;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tc_pos")))
            {
                //pParams->deinterlaceParam.mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                    if ( ioStatus > 0 )
                    {
                        pParams->deinterlaceParam[0].tc_pos   = (mfxU16)readData;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-detail")))
            {
                pParams->detailParam[0].mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                    if ( ioStatus > 0 )
                    {
                        pParams->detailParam[0].factor = (mfxU16)readData;
                        pParams->detailParam[0].mode   = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-vanalysis")))
            {
                pParams->vaParam[0].mode = VPP_FILTER_ENABLED_DEFAULT;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-rotate")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->rotate[0]);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-scaling_mode")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                pParams->bScaling = true;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->scalingMode);
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-variance")))
            {
                pParams->varianceParam[0].mode = VPP_FILTER_ENABLED_DEFAULT;
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-idetect")))
            {
                pParams->idetectParam[0].mode = VPP_FILTER_ENABLED_DEFAULT;
            }
            // different modes of MFX FRC
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-frc:advanced")))
            {
                pParams->frcParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
                pParams->frcParam[0].algorithm = MFX_FRCALGM_DISTRIBUTED_TIMESTAMP;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-frc:interp")))
            {
                pParams->frcParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
                pParams->frcParam[0].algorithm = MFX_FRCALGM_FRAME_INTERPOLATION;
            }
            //---------------------------------------------
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pa_hue")))
            {
                pParams->procampParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->procampParam[0].hue);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pa_bri")))
            {
                pParams->procampParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->procampParam[0].brightness);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pa_con")))
            {
                pParams->procampParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->procampParam[0].contrast);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pa_sat")))
            {
                pParams->procampParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->procampParam[0].saturation);
            }

            //MSDK 3.0
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-gamut:compression")))
            {
                pParams->gamutParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-gamut:bt709")))
            {
                pParams->gamutParam[0].bBT709 = true;
            }
            else if( 0 == vm_string_strcmp(strInput[i], VM_STRING("-view:count")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;

                mfxU16 viewCount;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &viewCount);
                if( viewCount > 1 )
                {
                    pParams->multiViewParam[0].viewCount = (mfxU16)viewCount;
                    pParams->multiViewParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
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

                    pParams->svcParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
                    pParams->svcParam[0].descr[layer] = descr;

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
                pParams->istabParam[0].mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                    if ( ioStatus > 0 )
                    {
                        pParams->istabParam[0].istabMode = (mfxU8)readData;
                        pParams->istabParam[0].mode    = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;

                        if( pParams->istabParam[0].istabMode != 1 && pParams->istabParam[0].istabMode != 2 )
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
                pParams->aceParam[0].mode = VPP_FILTER_ENABLED_DEFAULT;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-ste")))
            {
                pParams->steParam[0].mode = VPP_FILTER_ENABLED_DEFAULT;

                if( i+1 < nArgNum )
                {
                    ioStatus = vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                    if ( ioStatus > 0 )
                    {
                        pParams->steParam[0].SkinToneFactor = (mfxU8)readData;
                        pParams->steParam[0].mode           = VPP_FILTER_ENABLED_CONFIGURED;
                        i++;
                    }
                }
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:red")))
            {
                pParams->tccParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->tccParam[0].Red);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:green")))
            {
                pParams->tccParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->tccParam[0].Green);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:blue")))
            {
                pParams->tccParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->tccParam[0].Blue);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:magenta")))
            {
                pParams->tccParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->tccParam[0].Magenta);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:yellow")))
            {
                pParams->tccParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->tccParam[0].Yellow);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-tcc:cyan")))
            {
                pParams->tccParam[0].mode = VPP_FILTER_ENABLED_CONFIGURED;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->tccParam[0].Cyan);
            }
            //-----------------------------------------------------------------------------------
            //                   Region of Interest Testing
            //-----------------------------------------------------------------------------------
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-roi_check")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                pParams->roiCheckParam.mode = Str2ROIMode( strInput[i] );

                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->roiCheckParam.srcSeed);

                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->roiCheckParam.dstSeed);
            }
            //-----------------------------------------------------------------------------------
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-i")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_strcpy(pParams->strSrcFile, strInput[i]);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-crc")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_strcpy(pParams->strCRCFile, strInput[i]);
                pParams->need_crc = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-o")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_strcpy(pParams->strDstFile, strInput[i]);
                pParams->isOutput = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pf")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_strcpy(pParams->strPerfFile, strInput[i]);

            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-scc")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                pParams->frameInfoIn[0].FourCC = Str2FourCC( strInput[i] );
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-dcc")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                pParams->frameInfoOut[0].FourCC = Str2FourCC( strInput[i] );
                pParams->isOutYV12 = false;
                if(MFX_FOURCC_YV12 == pParams->frameInfoOut[0].FourCC)
                {
                    pParams->frameInfoOut[0].FourCC = MFX_FOURCC_NV12;
                    pParams->isOutYV12 = true;
                }
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-dbitshift")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoOut[0].Shift);
            }
            else if(0 == vm_string_strcmp(strInput[i], VM_STRING("-sbitshift")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->frameInfoIn[0].Shift);
            }
            else if( 0 == vm_string_strcmp(strInput[i], VM_STRING("-iopattern")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                pParams->IOPattern = Str2IOpattern( strInput[i] );
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-lib")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                if (0 == vm_string_strcmp(strInput[i], VM_STRING("sw")) )
                    pParams->ImpLib = MFX_IMPL_SOFTWARE;
                else if (0 == vm_string_strcmp(strInput[i], VM_STRING("hw")) )
                {
                    //pParams->ImpLib = (isD3D11Required) ? (MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11): (MFX_IMPL_HARDWARE|MFX_IMPL_VIA_D3D9);
                    pParams->ImpLib = MFX_IMPL_HARDWARE;
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
                    pParams->sptr = INPUT_PTR;
                else if (0 == vm_string_strcmp(strInput[i], VM_STRING("out")) )
                    pParams->sptr = OUTPUT_PTR;
                else if (0 == vm_string_strcmp(strInput[i], VM_STRING("all")) )
                    pParams->sptr = ALL_PTR;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-defalloc")) )
            {
                pParams->bDefAlloc = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-async")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->asyncNum);

            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-perf_opt")) )
            {
                if (pParams->numFrames)
                    return MFX_ERR_UNKNOWN;

                VAL_CHECK(1 + i == nArgNum);
                pParams->bPerf = true;
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->numFrames);
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->numRepeat);
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pts_check")) )
            {
                pParams->ptsCheck = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pts_jump")) )
            {
                pParams->ptsJump = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pts_fr")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%lf"), &pParams->ptsFR);  
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-pts_advanced")) )
            {
                pParams->ptsAdvanced = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-n")) )
            {
                if (pParams->bPerf)
                    return MFX_ERR_UNKNOWN;

                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_sscanf(strInput[i], VM_STRING("%hd"), &pParams->numFrames);

            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-plugin_guid")))
            {
                VAL_CHECK(1 + i == nArgNum);
                i++;
                vm_string_strcpy(pParams->strPlgGuid, strInput[i]);
                pParams->need_plugin = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-extapi")) )
            {
                pParams->use_extapi = true;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("-reset_start")) )
            {
                VAL_CHECK(1 + i == nArgNum);
                vm_string_sscanf(strInput[i+1], VM_STRING("%hd"), &readData);
                i += 2;

                pParams->resetFrmNums.push_back((mfxU16)readData);

                if (MFX_ERR_NONE != vppParseResetPar(strInput, nArgNum, i, pParams, (mfxU32)pParams->resetFrmNums.size(), pDefaultFiltersParam))
                    return MFX_ERR_UNKNOWN;
            }
            else
            {
                vm_string_printf(VM_STRING("Unknown option: %s\n"), strInput[i]);

                return MFX_ERR_UNKNOWN;
            }
        }
    }

    /*if ((pParams->ImpLib & MFX_IMPL_HARDWARE) && !(pParams->ImpLib & MFX_IMPL_VIA_D3D11))
    {
    pParams->ImpLib = MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D9;
    }*/
    std::vector<sOwnFrameInfo>::iterator it = pParams->frameInfoIn.begin();
    while(it != pParams->frameInfoIn.end())
    {
        if (NOT_INIT_VALUE == it->CropW)
    {
            it->CropW = it->nWidth;
    }

        if (NOT_INIT_VALUE == it->CropH)
    {
            it->CropH = it->nHeight;
        }
        it++;
    }

    it = pParams->frameInfoOut.begin();
    while(it != pParams->frameInfoOut.end())
    {
        if (NOT_INIT_VALUE == it->CropW)
    {
            it->CropW = it->nWidth;
    }

        if (NOT_INIT_VALUE == it->CropH)
    {
            it->CropH = it->nHeight;
        }
        it++;
    }

    if (0 == vm_string_strlen(pParams->strSrcFile)) 
    {
        vppPrintHelp(strInput[0], VM_STRING("Source file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if (0 == vm_string_strlen(pParams->strDstFile)) 
    {
        vppPrintHelp(strInput[0], VM_STRING("Destination file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if (0 == pParams->IOPattern)
    {
        vppPrintHelp(strInput[0], VM_STRING("Incorrect IOPattern"));
        return MFX_ERR_UNSUPPORTED;
    }

    // aya: fix for windows only
    if( pParams->IOPattern != (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY) )
    {
        pParams->vaType = isD3D11Required ? ALLOC_IMPL_VIA_D3D11 : ALLOC_IMPL_VIA_D3D9;
    }

    if (pParams->ImpLib & MFX_IMPL_HARDWARE) 
    {
#if defined(_WIN32) || defined(_WIN64)
        pParams->ImpLib |= (isD3D11Required)? MFX_IMPL_VIA_D3D11 : MFX_IMPL_VIA_D3D9;
#elif defined (LIBVA_SUPPORT)
        pParams->ImpLib |= MFX_IMPL_VIA_VAAPI;
#endif
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

    for (mfxU32 i = 0; i < pParams->rotate.size(); i++)
    {
        if (pParams->rotate[i] != 0 && pParams->rotate[i] != 90 && pParams->rotate[i] != 180 && pParams->rotate[i] != 270)
        {
            vppPrintHelp(strInput[0], VM_STRING("Invalid -rotate parameter: supported values 0, 90, 180, 270\n"));
            return false;
        }
    }

    return true;

} // bool CheckInputParams(vm_char* strInput[], sInputVppParams* pParams )

/* EOF */
