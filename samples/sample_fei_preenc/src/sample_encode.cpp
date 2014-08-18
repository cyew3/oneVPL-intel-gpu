//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2014 Intel Corporation. All Rights Reserved.
//

#include <memory>
#include "pipeline_encode.h"

void PrintHelp(msdk_char *strAppName, msdk_char *strErrorMessage)
{
    msdk_printf(MSDK_STRING("Intel(R) Media SDK Encoding Sample FEI Version %s\n\n"), MSDK_SAMPLE_VERSION);

    if (strErrorMessage)
    {
        msdk_printf(MSDK_STRING("Error: %s\n\n"), strErrorMessage);
    }
    msdk_printf(MSDK_STRING("Usage: %s [Options] -i InputYUVFile -o OutputDataFile -w width -h height\n"), strAppName);
    msdk_printf(MSDK_STRING("Options: \n"));
    msdk_printf(MSDK_STRING("   [-n number] - number of frames to process"));
    msdk_printf(MSDK_STRING("   [-r distance] - Distance between I- or P- key frames (1 means no B-frames) (0 - by default(I frames))\n"));
    msdk_printf(MSDK_STRING("   [-g size] - GOP size (1(default) means I-frames only)\n"));
//    msdk_printf(MSDK_STRING("   [-c file] - config file to be used to change default control input\n"));
    msdk_printf(MSDK_STRING("   [-p file] - use this input file as MV predictors for each frame\n"));
    msdk_printf(MSDK_STRING("   [-q file] - use this input file as QP values for each frame\n"));
    msdk_printf(MSDK_STRING("   [-o file] - use this file to output resulting MVs per frame\n"));
    msdk_printf(MSDK_STRING("   [-m file] - use this file to output MB statistics for each frame\n"));
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

    pParams->CodecId = MFX_CODEC_AVC; //only AVC for now
    // parse command line parameters
    for (mfxU8 i = 1; i < nArgNum; i++)
    {
        MSDK_CHECK_POINTER(strInput[i], MFX_ERR_NULL_PTR);

        // process multi-character options
        if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-nv12")))
        {
            pParams->ColorFormat = MFX_FOURCC_NV12;
        }
        else // 1-character options
        {
            switch (strInput[i][1])
            {
#if 0
            case MSDK_CHAR('c'):
                GET_OPTION_POINTER(strArgument);
                pParams->ctrlFile = strArgument;
                break;
#endif
            case MSDK_CHAR('w'):
                GET_OPTION_POINTER(strArgument);
                pParams->nWidth = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('h'):
                GET_OPTION_POINTER(strArgument);
                pParams->nHeight = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('q'):
                GET_OPTION_POINTER(strArgument);
                pParams->inQPFile = strArgument;
                break;
            case MSDK_CHAR('n'):
                GET_OPTION_POINTER(strArgument);
                pParams->nNumFrames = (mfxF64)msdk_strtod(strArgument, &stopCharacter);
                break;
            case MSDK_CHAR('p'):
                GET_OPTION_POINTER(strArgument);
                pParams->inMVPredFile = strArgument;
                break;
            case MSDK_CHAR('i'):
                GET_OPTION_POINTER(strArgument);
                msdk_strcopy(pParams->strSrcFile, strArgument);
                break;
            case MSDK_CHAR('o'):
                GET_OPTION_POINTER(strArgument);
                pParams->dstMVFileBuff = strArgument;
                break;
            case MSDK_CHAR('m'):
                GET_OPTION_POINTER(strArgument);
                pParams->dstMBFileBuff = strArgument;
                break;
            case MSDK_CHAR('g'):
                GET_OPTION_POINTER(strArgument);
                pParams->gopSize = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('r'):
                GET_OPTION_POINTER(strArgument);
                pParams->refDist = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
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

    if (0 == pParams->nWidth || 0 == pParams->nHeight)
    {
        PrintHelp(strInput[0], MSDK_STRING("-w, -h must be specified"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (MFX_TARGETUSAGE_BEST_QUALITY != pParams->nTargetUsage && MFX_TARGETUSAGE_BEST_SPEED != pParams->nTargetUsage)
    {
        pParams->nTargetUsage = MFX_TARGETUSAGE_BALANCED;
    }

    if (pParams->dFrameRate <= 0)
    {
        pParams->dFrameRate = 30;
    }

    // calculate default bitrate based on the resolution (a parameter for encoder, so Dst resolution is used)
    if (pParams->nBitRate == 0)
    {
        pParams->nBitRate = CalculateDefaultBitrate(pParams->CodecId, pParams->nTargetUsage, pParams->nWidth,
            pParams->nHeight, pParams->dFrameRate);
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

    if (pParams->nLADepth && (pParams->nLADepth < 10 || pParams->nLADepth > 100))
    {
        PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -lad parameter, must be in range [10, 100]!"));
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
#if _DEBUG
    //print out cmd
    for(int i=0; i<argc; i++){
        fprintf(stderr,"%s ",argv[i]);
    }
    fprintf(stderr,"\n\n");
#endif
    sInputParams Params = {0};   // input parameters from command line
    std::auto_ptr<CEncodingPipeline>  pPipeline;

    mfxStatus sts = MFX_ERR_NONE; // return value check

    //set default params
    Params.refDist = 1; //only I frames
    Params.gopSize = 1; //only I frames
    Params.preENC = 1; //only preENC is supported for now
    Params.memType = D3D9_MEMORY; //use video memory by default

    //Input and output files
    Params.dstMBFileBuff = NULL;
    Params.dstMVFileBuff = NULL;
    Params.inMVPredFile = NULL;
    Params.inQPFile = NULL;

    sts = ParseInputString(argv, (mfxU8)argc, &Params);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    pPipeline.reset(new CEncodingPipeline);

    MSDK_CHECK_POINTER(pPipeline.get(), MFX_ERR_MEMORY_ALLOC);

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
