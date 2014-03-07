//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2014 Intel Corporation. All Rights Reserved.
//

#include <memory>
#include "pipeline_encode.h"
#include "pipeline_user.h"

void PrintHelp(msdk_char *strAppName, const msdk_char *strErrorMessage)
{
    msdk_printf(MSDK_STRING("Intel(R) Media SDK Decoding Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);

    if (strErrorMessage)
    {
        msdk_printf(MSDK_STRING("Error: %s\n"), strErrorMessage);
    }

    msdk_printf(MSDK_STRING("Usage: %s <msdk-codecid> [<options>] -i InputYUVFile -o OutputEncodedFile -w width -h height\n"), strAppName);
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Supported codecs, <msdk-codecid>:\n"));
    msdk_printf(MSDK_STRING("   <codecid>=h264|mpeg2|vc1|mvc|jpeg - built-in Media SDK codecs\n"));
    msdk_printf(MSDK_STRING("   <codecid>=h265                    - in-box Media SDK plugins (may require separate downloading and installation)\n"));
    msdk_printf(MSDK_STRING("Options: \n"));
    msdk_printf(MSDK_STRING("   [-nv12] - input is in NV12 color format, if not specified YUV420 is expected\n"));
    msdk_printf(MSDK_STRING("   [-tff|bff] - input stream is interlaced, top|bottom fielf first, if not specified progressive is expected\n"));
    msdk_printf(MSDK_STRING("   [-f frameRate] - video frame rate (frames per second)\n"));
    msdk_printf(MSDK_STRING("   [-b bitRate] - encoded bit rate (Kbits per second), valid for H.264, H.265, MPEG2 and MVC encoders \n"));
    msdk_printf(MSDK_STRING("   [-u speed|quality|balanced] - target usage, valid for H.264, H.265, MPEG2 and MVC encoders\n"));
    msdk_printf(MSDK_STRING("   [-q quality] - quality parameter for JPEG encoder. In range [1,100]. 100 is the best quality. \n"));
    msdk_printf(MSDK_STRING("   [-la] - use the look ahead bitrate control algorithm (LA BRC) for H.264, H.265 encoder. Supported only with -hw option on 4th Generation Intel Core processors. \n"));
    msdk_printf(MSDK_STRING("   [-lad depth] - depth parameter for the LA BRC, the number of frames to be analyzed before encoding. In range [10,100].\n"));
    msdk_printf(MSDK_STRING("   [-dstw width] - destination picture width, invokes VPP resizing\n"));
    msdk_printf(MSDK_STRING("   [-dsth height] - destination picture height, invokes VPP resizing\n"));
    msdk_printf(MSDK_STRING("   [-hw] - use platform specific SDK implementation, if not specified software implementation is used\n"));
#if D3D_SURFACES_SUPPORT
    msdk_printf(MSDK_STRING("   [-d3d] - work with d3d surfaces\n"));
    msdk_printf(MSDK_STRING("   [-d3d11] - work with d3d11 surfaces\n"));
    msdk_printf(MSDK_STRING("Example: %s h264|h265|mpeg2|jpeg -i InputYUVFile -o OutputEncodedFile -w width -h height -d3d -hw \n"), strAppName);
    msdk_printf(MSDK_STRING("Example for MVC: %s mvc -i InputYUVFile_1 -i InputYUVFile_2 -o OutputEncodedFile -w width -h height \n"), strAppName);
#endif
#ifdef LIBVA_SUPPORT
    msdk_printf(MSDK_STRING("   [-vaapi] - work with vaapi surfaces\n"));
    msdk_printf(MSDK_STRING("Example: %s h264|mpeg2|mvc -i InputYUVFile -o OutputEncodedFile -w width -h height -angle 180 -g 300 -r 1 \n"), strAppName);
#endif
    msdk_printf(MSDK_STRING("   [-viewoutput] - instruct the MVC encoder to output each view in separate bitstream buffer. Depending on the number of -o options behaves as follows:\n"));
    msdk_printf(MSDK_STRING("                   1: two views are encoded in single file\n"));
    msdk_printf(MSDK_STRING("                   2: two views are encoded in separate files\n"));
    msdk_printf(MSDK_STRING("                   3: behaves like 2 -o opitons was used and then one -o\n\n"));
    msdk_printf(MSDK_STRING("Example: %s mvc -i InputYUVFile_1 -i InputYUVFile_2 -o OutputEncodedFile_1 -o OutputEncodedFile_2 -viewoutput -w width -h height \n"), strAppName);
    // user module options
    msdk_printf(MSDK_STRING("User module options: \n"));
    msdk_printf(MSDK_STRING("   [-angle 180] - enables 180 degrees picture rotation before encoding, CPU implementation by default. Rotation requires NV12 input. Options -tff|bff, -dstw, -dsth, -d3d are not effective together with this one, -nv12 is required.\n"));
    msdk_printf(MSDK_STRING("   [-opencl] - rotation implementation through OPENCL\n"));
    msdk_printf(MSDK_STRING("Example: %s h264|h265|mpeg2|mvc|jpeg -i InputYUVFile -o OutputEncodedFile -w width -h height -angle 180 -opencl \n"), strAppName);

    msdk_printf(MSDK_STRING("\n"));
}

#define GET_OPTION_POINTER(PTR)         \
{                                       \
    if (2 == msdk_strlen(strInput[i]))      \
    {                                   \
        i++;                            \
        if (strInput[i][0] == MSDK_CHAR('-'))  \
        {                               \
            i = i - 1;                  \
        }                               \
        else                            \
        {                               \
            PTR = strInput[i];          \
        }                               \
    }                                   \
    else                                \
    {                                   \
        PTR = strInput[i] + 2;          \
    }                                   \
}

mfxStatus ParseInputString(msdk_char* strInput[], mfxU8 nArgNum, sInputParams* pParams)
{
    msdk_char* strArgument = MSDK_STRING("");
    msdk_char* stopCharacter;

    if (1 == nArgNum)
    {
        PrintHelp(strInput[0], NULL);
        return MFX_ERR_UNSUPPORTED;
    }

    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    msdk_strcopy(pParams->strPluginDLLPath, MSDK_STRING(PLUGIN_NAME));

    // parse command line parameters
    for (mfxU8 i = 1; i < nArgNum; i++)
    {
        MSDK_CHECK_POINTER(strInput[i], MFX_ERR_NULL_PTR);

        if (MSDK_CHAR('-') != strInput[i][0])
        {
            if (0 == msdk_strcmp(strInput[i], MSDK_STRING("h264")))
            {
                pParams->CodecId = MFX_CODEC_AVC;
            }
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("h265")))
            {
                pParams->CodecId = MFX_CODEC_HEVC;
            }
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("mpeg2")))
            {
                pParams->CodecId = MFX_CODEC_MPEG2;
            }
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("mvc")))
            {
                pParams->CodecId = MFX_CODEC_AVC;
                pParams->MVC_flags |= MVC_ENABLED;
            }
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("jpeg")))
            {
                pParams->CodecId = MFX_CODEC_JPEG;
            }
            else
            {
                PrintHelp(strInput[0], MSDK_STRING("Unknown codec"));
                return MFX_ERR_UNSUPPORTED;
            }
            continue;
        }

        // process multi-character options
        if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dstw")))
        {
            i++;
            pParams->nDstWidth = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dsth")))
        {
            i++;
            pParams->nDstHeight = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-hw")))
        {
            pParams->bUseHWLib = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-nv12")))
        {
            pParams->ColorFormat = MFX_FOURCC_NV12;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-tff")))
        {
            pParams->nPicStruct = MFX_PICSTRUCT_FIELD_TFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bff")))
        {
            pParams->nPicStruct = MFX_PICSTRUCT_FIELD_BFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-angle")))
        {
            i++;
            pParams->nRotationAngle = (mfxU8)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-opencl")))
        {
#if defined(_WIN32) || defined(_WIN64)
            msdk_strcopy(pParams->strPluginDLLPath, MSDK_STRING("sample_plugin_opencl.dll"));
#else
            msdk_strcopy(pParams->strPluginDLLPath, MSDK_STRING("libsample_plugin_opencl.so"));
#endif
            pParams->nRotationAngle = 180;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-viewoutput")))
        {
            if (!(MVC_ENABLED & pParams->MVC_flags))
            {
                PrintHelp(strInput[0], MSDK_STRING("-viewoutput option is supported only when mvc codec specified"));
                return MFX_ERR_UNSUPPORTED;
            }
            pParams->MVC_flags |= MVC_VIEWOUTPUT;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-la")))
        {
            pParams->bLABRC = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-lad")))
        {
            i++;
            pParams->nLADepth = (mfxU8)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
#if D3D_SURFACES_SUPPORT
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d")))
        {
            pParams->memType = D3D9_MEMORY;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d11")))
        {
            pParams->memType = D3D11_MEMORY;
        }
#endif
#ifdef LIBVA_SUPPORT
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-vaapi")))
        {
            pParams->memType = D3D9_MEMORY;
        }
#endif
        else // 1-character options
        {
            switch (strInput[i][1])
            {
            case MSDK_CHAR('u'):
                GET_OPTION_POINTER(strArgument);
                pParams->nTargetUsage = StrToTargetUsage(strArgument);
                break;
            case MSDK_CHAR('w'):
                GET_OPTION_POINTER(strArgument);
                pParams->nWidth = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('h'):
                GET_OPTION_POINTER(strArgument);
                pParams->nHeight = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('f'):
                GET_OPTION_POINTER(strArgument);
                pParams->dFrameRate = (mfxF64)msdk_strtod(strArgument, &stopCharacter);
                break;
            case MSDK_CHAR('b'):
                GET_OPTION_POINTER(strArgument);
                pParams->nBitRate = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('i'):
                GET_OPTION_POINTER(strArgument);
                msdk_strcopy(pParams->strSrcFile, strArgument);
                if (MVC_ENABLED & pParams->MVC_flags)
                {
                    pParams->srcFileBuff.push_back(strArgument);
                }
                break;
            case MSDK_CHAR('o'):
                GET_OPTION_POINTER(strArgument);
                pParams->dstFileBuff.push_back(strArgument);
                break;
            case MSDK_CHAR('q'):
                GET_OPTION_POINTER(strArgument);
                pParams->nQuality = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('?'):
                PrintHelp(strInput[0], NULL);
                return MFX_ERR_UNSUPPORTED;
            default:
                PrintHelp(strInput[0], MSDK_STRING("Unknown options"));
            }
        }
    }

    // check if all mandatory parameters were set
    if (0 == msdk_strlen(pParams->strSrcFile))
    {
        PrintHelp(strInput[0], MSDK_STRING("Source file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if (pParams->dstFileBuff.empty())
    {
        PrintHelp(strInput[0], MSDK_STRING("Destination file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if (0 == pParams->nWidth || 0 == pParams->nHeight)
    {
        PrintHelp(strInput[0], MSDK_STRING("-w, -h must be specified"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (MFX_CODEC_MPEG2 != pParams->CodecId && MFX_CODEC_AVC != pParams->CodecId && MFX_CODEC_JPEG != pParams->CodecId && MFX_CODEC_HEVC != pParams->CodecId)
    {
        PrintHelp(strInput[0], MSDK_STRING("Unknown codec"));
        return MFX_ERR_UNSUPPORTED;
    }

    // check parameters validity
    if (pParams->nRotationAngle != 0 && pParams->nRotationAngle != 180)
    {
        PrintHelp(strInput[0], MSDK_STRING("Angles other than 180 degrees are not supported."));
        return MFX_ERR_UNSUPPORTED; // other than 180 are not supported
    }

    if (pParams->nQuality && (MFX_CODEC_JPEG != pParams->CodecId))
    {
        PrintHelp(strInput[0], MSDK_STRING("-q option is supported only for JPEG encoder"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pParams->nTargetUsage || pParams->nBitRate) && (MFX_CODEC_JPEG == pParams->CodecId))
    {
        PrintHelp(strInput[0], MSDK_STRING("-u and -b options are supported only for H.264, MPEG2 and MVC encoders. For JPEG encoder use -q"));
        return MFX_ERR_UNSUPPORTED;
    }

    // set default values for optional parameters that were not set or were set incorrectly
    mfxU32 nviews = (mfxU32)pParams->srcFileBuff.size();
    if ((nviews <= 1) || (nviews > 2))
    {
        if (!(MVC_ENABLED & pParams->MVC_flags))
        {
            pParams->numViews = 1;
        }
        else
        {
            PrintHelp(strInput[0], MSDK_STRING("Only 2 views are supported right now in this sample."));
            return MFX_ERR_UNSUPPORTED;
        }
    }
    else
    {
        pParams->numViews = nviews;
    }

    if (MFX_TARGETUSAGE_BEST_QUALITY != pParams->nTargetUsage && MFX_TARGETUSAGE_BEST_SPEED != pParams->nTargetUsage)
    {
        pParams->nTargetUsage = MFX_TARGETUSAGE_BALANCED;
    }

    if (pParams->dFrameRate <= 0)
    {
        pParams->dFrameRate = 30;
    }

    // if no destination picture width or height wasn't specified set it to the source picture size
    if (pParams->nDstWidth == 0)
    {
        pParams->nDstWidth = pParams->nWidth;
    }

    if (pParams->nDstHeight == 0)
    {
        pParams->nDstHeight = pParams->nHeight;
    }

    // calculate default bitrate based on the resolution (a parameter for encoder, so Dst resolution is used)
    if (pParams->nBitRate == 0)
    {
        pParams->nBitRate = CalculateDefaultBitrate(pParams->CodecId, pParams->nTargetUsage, pParams->nDstWidth,
            pParams->nDstHeight, pParams->dFrameRate);
    }

    // if nv12 option wasn't specified we expect input YUV file in YUV420 color format
    if (!pParams->ColorFormat)
    {
        pParams->ColorFormat = MFX_FOURCC_YV12;
    }

    if (!pParams->nPicStruct)
    {
        pParams->nPicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    if ((pParams->bLABRC || pParams->nLADepth) && (!pParams->bUseHWLib))
    {
        PrintHelp(strInput[0], MSDK_STRING("Look ahead BRC is supported only with -hw option!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pParams->bLABRC || pParams->nLADepth) && (pParams->CodecId != MFX_CODEC_AVC))
    {
        PrintHelp(strInput[0], MSDK_STRING("Look ahead BRC is supported only with H.264 encoder!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->nLADepth && (pParams->nLADepth < 10 || pParams->nLADepth > 100))
    {
        PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -lad parameter, must be in range [10, 100]!"));
        return MFX_ERR_UNSUPPORTED;
    }

    // not all options are supported if rotate plugin is enabled
    if (pParams->nRotationAngle == 180 && (
        MFX_PICSTRUCT_PROGRESSIVE != pParams->nPicStruct ||
        pParams->nDstWidth != pParams->nWidth ||
        pParams->nDstHeight != pParams->nHeight ||
        MVC_ENABLED & pParams->MVC_flags ||
        pParams->memType & D3D11_MEMORY ||
        pParams->bLABRC ||
        pParams->nLADepth))
    {
        PrintHelp(strInput[0], MSDK_STRING("Some of the command line options are not supported with rotation plugin!"));
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

#if defined(_WIN32) || defined(_WIN64)
int _tmain(int argc, msdk_char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    sInputParams Params = {};   // input parameters from command line
    std::auto_ptr<CEncodingPipeline>  pPipeline;

    mfxStatus sts = MFX_ERR_NONE; // return value check

    sts = ParseInputString(argv, (mfxU8)argc, &Params);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    pPipeline.reset((Params.nRotationAngle) ? new CUserPipeline : new CEncodingPipeline);

    MSDK_CHECK_POINTER(pPipeline.get(), MFX_ERR_MEMORY_ALLOC);

    if (MVC_ENABLED & Params.MVC_flags)
    {
        pPipeline->SetMultiView();
        pPipeline->SetNumView(Params.numViews);
    }

    sts = pPipeline->Init(&Params);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

    pPipeline->PrintInfo();

    msdk_printf(MSDK_STRING("Processing started\n"));

    for (;;)
    {
        sts = pPipeline->Run();

        if (MFX_ERR_DEVICE_LOST == sts || MFX_ERR_DEVICE_FAILED == sts)
        {
            msdk_printf(MSDK_STRING("\nERROR: Hardware device was lost or returned an unexpected error. Recovering...\n"));
            sts = pPipeline->ResetDevice();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

            sts = pPipeline->ResetMFXComponents(&Params);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            continue;
        }
        else
        {
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            break;
        }
    }

    pPipeline->Close();
    msdk_printf(MSDK_STRING("\nProcessing finished\n"));

    return 0;
}
