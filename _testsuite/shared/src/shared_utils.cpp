//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008-2016 Intel Corporation. All Rights Reserved.
//
#include <math.h>
#include <memory.h>
#include "assert.h"
#include "shared_utils.h"
#include "app_defs.h"
#include "memory_allocator.h"

void PrintHelp(vm_char *strAppName, vm_char *strErrorMessage)
{
    if(strErrorMessage)
    vm_string_printf(VM_STRING("Error: %s\n"), strErrorMessage);
    vm_string_printf(VM_STRING("Usage1: %s [Options] -i InputYUVFile -o OutputEncodedFile\n"), strAppName);
    vm_string_printf(VM_STRING("Usage2 (vc1 only): %s parfileName [Options] -i InputYUVFile -o OutputEncodedFile\n"), strAppName);
    vm_string_printf(VM_STRING("Options: \n"));
    vm_string_printf(VM_STRING("   [-w width]     - video width\n"));
    vm_string_printf(VM_STRING("   [-h height]    - video height\n"));
    vm_string_printf(VM_STRING("   [-f frameRate] - video frame rate (frames per second)\n"));
    vm_string_printf(VM_STRING("   [-b bitRate]   - encoded bit rate (bits per second)\n"));
    vm_string_printf(VM_STRING("   [-n numFrames] - maximum number of frames to be encoded\n"));
    vm_string_printf(VM_STRING("   [-r refDist]   - interval of two reference frames (B in row+1)\n"));
    vm_string_printf(VM_STRING("   [-g gopSize]   - number of frames in open GOP\n"));
    vm_string_printf(VM_STRING("   [-gc ]         - all GOPs are closed\n"));
    vm_string_printf(VM_STRING("   [-gs ]         - strict GOP structure\n"));
    vm_string_printf(VM_STRING("   [-q seqSize]   - sequence size in GOPs or IdrInterval\n"));
    vm_string_printf(VM_STRING("   [-qc]          - add EOS symbol\n"));
    vm_string_printf(VM_STRING("   [-s picStruct] - 0=progressive, 1=tff, 2=bff, 3=field tff, 4=field bff\n"));
    vm_string_printf(VM_STRING("   [-e entryMode] - 0=encode, 1=VME+FullPAK, 2=FullENC+BSP, 3=VME+FullPAK (slice level), 4=FullENC+BSP (slice level)\n"));
    vm_string_printf(VM_STRING("   [-t numThreads]- number of threads to be used\n"));
    vm_string_printf(VM_STRING("   [-hw HWAcceleration]- 0=SW, 1=HW+SW, 2=SW+HW, 3=HW\n"));
    vm_string_printf(VM_STRING("   [-u targetUsage]- 0=ignored, 1=best_quality, 2=hi_quality, 3=opt_quality, 4=realtime, 5=ok_quality, 6=hi_speed, 7=best_speed\n"));
    vm_string_printf(VM_STRING("   [-p perfFile]  - file name to append performance results\n"));
    vm_string_printf(VM_STRING("   [-c [0|1]]     - set no CABAC limit (only AVC)\n"));
    vm_string_printf(VM_STRING("   [-l numSlices] - number of slices (only AVC)\n"));
    vm_string_printf(VM_STRING("   [-x numRefs]   - number of reference frames (only AVC PRO)\n"));
    vm_string_printf(VM_STRING("   [-d [0|1]]     - deblocking off/on (only AVC PRO)\n"));
    vm_string_printf(VM_STRING("   [-m [0|1|2]]   - memory allocator type: 0 - default MA & SM input frames, 1 - external MA & SM input frames, 2 - external MA & D3D input frames\n"));
    vm_string_printf(VM_STRING("   [-y [0|1]]     - frame order: 0 - display order, 1 - encoded order\n"));
    vm_string_printf(VM_STRING("   [-bs bufferSize] - HRD buffer size in KBytes, K=1000\n"));
    vm_string_printf(VM_STRING("   [-id initialDelay] - HRD buffer initial delay (fullness) in KBytes, K=1000\n"));
    vm_string_printf(VM_STRING("   [-bm maxBitRate] - maximal bit rate (bits per second)\n"));
    vm_string_printf(VM_STRING("\n"));
}

#define GET_OPTION_POINTER(PTR)     \
{                                   \
    if (2 == vm_string_strlen(strInput[i]))  \
{                               \
    if ((i + 1) >= nArgNum || strInput[i + 1][0] == '-')  \
{                           \
    strArgument = VM_STRING("");   \
}                           \
        else                        \
{                           \
    PTR = strInput[++i];    \
}                           \
}                               \
    else                            \
{                               \
    PTR = strInput[i] + 2;      \
}                               \
}

mfxStatus ParseInputString(vm_char* strInput[], int nArgNum, sInputParams* pParams)
{
    mfxStatus sts = MFX_ERR_NONE;
    const vm_char* strArgument = VM_STRING("");
    CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    if (nArgNum < 2)
    {
        PrintHelp(strInput[0], 0);
        return MFX_ERR_NULL_PTR;
    }

    for (mfxU8 i = 1; i < nArgNum; i++)
    {

        if ('-' != strInput[i][0])
        {
            if (0 == vm_string_strcmp(strInput[i], VM_STRING("m2")))
            {
                pParams->videoType = MFX_CODEC_MPEG2;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("h264")))
            {
                pParams->videoType = MFX_CODEC_AVC;
            }
            else if (0 == vm_string_strcmp(strInput[i], VM_STRING("vc1")))
            {
                pParams->videoType = MFX_CODEC_VC1;
            }
            else if (!pParams->strParFile)
            {
                pParams->strParFile = strInput[i];
            }
            continue;
        }
        else
        {
            switch (strInput[i][1])
            {
            case 'w':
                GET_OPTION_POINTER(strArgument);
                pParams->nWidth = vm_string_atol(strArgument);
                break;
            case 'h':
                if (strInput[i][2] == 'w' && (i + 1) < nArgNum)
                {
                    pParams->HWAcceleration = vm_string_atol(strInput[++i]);
                }
                else
                {
                    GET_OPTION_POINTER(strArgument);
                    if(strArgument[0])
                    {
                        pParams->nHeight = vm_string_atol(strArgument);
                    } else {
                        PrintHelp(strInput[0], 0);
                        return MFX_ERR_NULL_PTR;
                    }
                }
                break;
            case 'f':
                GET_OPTION_POINTER(strArgument);
                vm_string_sscanf(strArgument, VM_STRING("%lf"), &pParams->dFrameRate);
                break;
            case 'b':
                if (strInput[i][2] == 'm' && (i + 1) < nArgNum)
                {
                  pParams->nMaxBitrate = vm_string_atol(strInput[++i]);
                  break;
                }
                if (strInput[i][2] == 's' && (i + 1) < nArgNum)
                {
                  pParams->nHRDBufSizeInKB = vm_string_atol(strInput[++i]);
                  break;
                }
                GET_OPTION_POINTER(strArgument);
                pParams->nBitRate = vm_string_atol(strArgument);
                break;
            case 'n':
                GET_OPTION_POINTER(strArgument);
                pParams->nFrames = vm_string_atol(strArgument);
                break;
            case 'r':
                GET_OPTION_POINTER(strArgument);
                pParams->nRefDist = vm_string_atol(strArgument);
                break;
            case 'g':
                if (strInput[i][2] == 'c')
                {
                    pParams->nGopOptFlag |= MFX_GOP_CLOSED;
                }
                else if (strInput[i][2] == 's')
                {
                    pParams->nGopOptFlag |= MFX_GOP_STRICT;
                }
                else
                {
                    GET_OPTION_POINTER(strArgument);
                    pParams->nGOPsize = vm_string_atol(strArgument);
                }
                break;
            case 'q':
                if (strInput[i][2] == 'c')
                {
                    pParams->bEOS = 1;
                }
                else
                {
                    GET_OPTION_POINTER(strArgument);
                    pParams->nSeqSize = vm_string_atol(strArgument);
                    break;
                }
            case 's':
                GET_OPTION_POINTER(strArgument);
                pParams->picStruct = vm_string_atol(strArgument);
                break;
            case 'e':
                GET_OPTION_POINTER(strArgument);
                pParams->entryMode = vm_string_atol(strArgument);
                break;
            case 't':
                GET_OPTION_POINTER(strArgument);
                pParams->nThreads = vm_string_atol(strArgument);
                break;
            case 'i':
                if (strInput[i][2] == 'd' && (i + 1) < nArgNum)
                {
                  pParams->nHRDInitDelayInKB = vm_string_atol(strInput[++i]);
                  break;
                }
                GET_OPTION_POINTER(strArgument);
                vm_string_strcpy(pParams->strSrcFile, strArgument);
                break;
            case 'o':
                GET_OPTION_POINTER(strArgument);
                pParams->strDstFile = strArgument;
                break;
            case 'p':
                GET_OPTION_POINTER(strArgument);
                pParams->perfFile = strArgument;
                break;
            case 'u':
                GET_OPTION_POINTER(strArgument);
                pParams->targetUsage = vm_string_atol(strArgument);
                break;
            case 'c':
                GET_OPTION_POINTER(strArgument);
                pParams->CabacCavlc = vm_string_atol(strArgument);
                break;
            case 'l':
                GET_OPTION_POINTER(strArgument);
                pParams->numSlices = vm_string_atol(strArgument);
                break;
            case 'x':
                GET_OPTION_POINTER(strArgument);
                pParams->numRef = vm_string_atol(strArgument);
                break;
            case 'd':
                GET_OPTION_POINTER(strArgument);
                pParams->deblocking = vm_string_atol(strArgument);
                break;
            case 'm':
                GET_OPTION_POINTER(strArgument);
                pParams->memType = vm_string_atol(strArgument);
                break;
            case 'y':
                GET_OPTION_POINTER(strArgument);
                pParams->EncodedOrder = vm_string_atol(strArgument);
                break;
            case '?':
                PrintHelp(strInput[0], 0);
                return MFX_ERR_NULL_PTR;
            default:
                PrintHelp(strInput[0], const_cast<vm_char*>(VM_STRING("Unknown options")));
                return MFX_ERR_UNSUPPORTED;
            }
        }
    }

    //if (!pParams->strDstFile)
    //{
    //    PrintHelp(strInput[0], VM_STRING("Destination file name not found"));
    //    return 1;
    //}

    //if (0 == pParams->nWidth || 0 == pParams->nHeight)
    //{
    //    PrintHelp(strInput[0], VM_STRING("-w,-h should be specified"));
    //    return 1;
    //}

    //if (pParams->videoType != MFX_CODEC_MPEG2&& pParams->videoType != MFX_CODEC_AVC && pParams->videoType != MFX_CODEC_VC1)
    //{
    //  pParams->videoType = MFX_CODEC_MPEG2;
    //PrintHelp(strInput[0], VM_STRING("Unknown codec"));
    //return 1;
    //}

    //if (0 >= pParams->dFrameRate)
    //{
    //    pParams->dFrameRate = 30;
    //}

    //if (pParams->nBitRate <= 0)
    //{
    //    pParams->nBitRate = 500;
    //}

    return sts;
}

mfxStatus PrintCurrentParams(sInputParams* pParams)
{
    char* p = (char*)&pParams->videoType;
    vm_char tp[5] = {p[0], p[1], p[2], p[3], 0};
    vm_string_printf(VM_STRING("videoType       = %s\n"), tp);
    vm_string_printf(VM_STRING("Width           = %d\n"), pParams->nWidth);
    vm_string_printf(VM_STRING("Height          = %d\n"), pParams->nHeight);
    vm_string_printf(VM_STRING("FrameRate       = %.2f\n"), pParams->dFrameRate);
    vm_string_printf(VM_STRING("BitRate         = %d\n"), pParams->nBitRate);
    vm_string_printf(VM_STRING("GOPsize         = %d\n"), pParams->nGOPsize);
    vm_string_printf(VM_STRING("RefDist         = %d\n"), pParams->nRefDist);
    vm_string_printf(VM_STRING("NumberFrames    = %d\n"), pParams->nFrames);
    vm_string_printf(VM_STRING("PictureStruct   = %d\n"), pParams->picStruct);
    vm_string_printf(VM_STRING("EntryMode       = %d\n"), pParams->entryMode);
    vm_string_printf(VM_STRING("NumberThreads   = %d\n"), pParams->nThreads);
    vm_string_printf(VM_STRING("HWAcceleration  = %d\n"), pParams->HWAcceleration);
    vm_string_printf(VM_STRING("TargetUsage     = %d\n"), pParams->targetUsage);
    vm_string_printf(VM_STRING("SrcFile         = %s\n"), pParams->strSrcFile);
    vm_string_printf(VM_STRING("Coding frame order %d\n"), pParams->EncodedOrder);
   
    if (pParams->strDstFile)
        vm_string_printf(VM_STRING("DstFile         = %s\n"), pParams->strDstFile);
    if (pParams->strParFile)
        vm_string_printf(VM_STRING("ParFile         = %s\n"), pParams->strParFile);
    if (pParams->perfFile)
        vm_string_printf(VM_STRING("perfFile        = %s\n"), pParams->perfFile);

    return MFX_ERR_NONE;
}
void mfxGetFrameSizeParam (mfxVideoParam* pMFXParams, mfxU16 w, mfxU16 h, bool bProg)
{
    pMFXParams->mfx.FrameInfo.Width     = ((w + 15)>>4)<<4;
    pMFXParams->mfx.FrameInfo.Height    = (bProg)? ((h + 15)>>4)<<4 : ((h + 31)>>5)<<5;
    pMFXParams->mfx.FrameInfo.CropX     = 0;
    pMFXParams->mfx.FrameInfo.CropY     = 0;
    pMFXParams->mfx.FrameInfo.CropW     = w;
    pMFXParams->mfx.FrameInfo.CropH     = h;

}



mfxStatus InitMFXParams(mfxVideoParam* pMFXParams, sInputParams* pInParams)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 i;
    mfxExtCodingOption *codingOpt = 0;
    //check input params
    CHECK_POINTER(pMFXParams, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pInParams,  MFX_ERR_NULL_PTR);

    //prepare mfxVideoParam
    //memset(pMFXParams, 0, sizeof(mfxVideoParam));                   //Do not zero memory to save ExtParams!
    pMFXParams->mfx.CodecId = pInParams->videoType;                 //set encoder
    //pMFXParams->mfx.FrameInfo.FourCC = MFX_FOURCC_YV12;             //set FourCC
    pMFXParams->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;           //set FourCC

    for(i=0; i<pMFXParams->NumExtParam; i++) {
      if(pMFXParams->ExtParam[i]->BufferId == MFX_EXTBUFF_CODING_OPTION) {
        codingOpt = (mfxExtCodingOption*)pMFXParams->ExtParam[i];
        break;
      }
    }

    //set picture structure
    if(pInParams->picStruct == 1 || pInParams->picStruct == 3)
        pMFXParams->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
    else if(pInParams->picStruct == 2 || pInParams->picStruct == 4)
        pMFXParams->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_FIELD_BFF;
    else
        pMFXParams->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    if (codingOpt) {
        if(pInParams->picStruct != 3 && pInParams->picStruct != 4)
            codingOpt->FramePicture= MFX_CODINGOPTION_ON;
        else
            codingOpt->FramePicture= MFX_CODINGOPTION_OFF;
    }
    //pMFXParams->mfx.FrameInfo.PicStruct |= MFX_PICSTRUCT_OUTPUT_FRAME;
    
    pMFXParams->mfx.EncodedOrder = (pInParams->EncodedOrder !=0)? 1:0;

    FrameRate2Code(pInParams->dFrameRate, &pMFXParams->mfx.FrameInfo);            // set frame rate

    mfxGetFrameSizeParam (pMFXParams, (mfxU16)pInParams->nWidth, (mfxU16)pInParams->nHeight, pMFXParams->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE);

    pMFXParams->mfx.TargetKbps = (mfxU16)(pInParams->nBitRate/1000);    // set bitrate
    if(pMFXParams->mfx.TargetKbps <= 0) 
      pMFXParams->mfx.TargetKbps = (mfxU16)(pMFXParams->mfx.FrameInfo.Height * pMFXParams->mfx.FrameInfo.Width * (pInParams->dFrameRate > 0 ? pInParams->dFrameRate : 30) / 2 / 1000);
    
    /* GOP structure */
    /* to be changed in further versions */

    pMFXParams->mfx.IdrInterval = (mfxU16)pInParams->nSeqSize;
    pMFXParams->mfx.GopPicSize  = (mfxU16)pInParams->nGOPsize;
    pMFXParams->mfx.GopRefDist  = (mfxU8)pInParams->nRefDist;
    pMFXParams->mfx.GopOptFlag  = (mfxU16)pInParams->nGopOptFlag;

    pMFXParams->mfx.NumThread = (pInParams->nThreads>0) ? (mfxU8)pInParams->nThreads : 0; // as undefined - codec to decide
    pMFXParams->mfx.NumSlice = (mfxU16)pInParams->numSlices;

    pMFXParams->mfx.TargetUsage = (mfxU8)pInParams->targetUsage;
    //pMFXParams->mfx.CAVLC = !pInParams->CabacCavlc;

    pMFXParams->mfx.BufferSizeInKB = (mfxU16)pInParams->nHRDBufSizeInKB;
    pMFXParams->mfx.InitialDelayInKB = (mfxU16)pInParams->nHRDInitDelayInKB;
    if (pInParams->nMaxBitrate > pInParams->nBitRate) {
        pMFXParams->mfx.MaxKbps = (mfxU16)(pInParams->nMaxBitrate/1000);
        pMFXParams->mfx.RateControlMethod = MFX_RATECONTROL_VBR;
    } else {
        pMFXParams->mfx.RateControlMethod = MFX_RATECONTROL_CBR;
        pMFXParams->mfx.MaxKbps = pMFXParams->mfx.TargetKbps;
    }

    pMFXParams->IOPattern = (mfxU16)((pInParams->memType > 1) ? MFX_IOPATTERN_IN_VIDEO_MEMORY : MFX_IOPATTERN_IN_SYSTEM_MEMORY);

    sts = FourCC2ChromaFormat(pMFXParams->mfx.FrameInfo.FourCC, &pMFXParams->mfx.FrameInfo.ChromaFormat);
    CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, sts)

    sts = SetDefaultProfileLevel (pMFXParams->mfx.CodecId, &pMFXParams->mfx.CodecProfile, &pMFXParams->mfx.CodecLevel);
    CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, sts)

    return MFX_ERR_NONE;
}

mfxStatus InitMFXParamsDec(mfxVideoParam* pMfxParams, sInputParams* pInParams)
{
    CHECK_POINTER(pMfxParams, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pInParams,  MFX_ERR_NULL_PTR);

    pMfxParams->mfx.CodecId                     = pInParams->videoType;
    pMfxParams->mfx.FrameInfo.FourCC            = pInParams->nFourCC;
    pMfxParams->mfx.FrameInfo.Height            = (mfxU16)pInParams->nHeight;
    pMfxParams->mfx.FrameInfo.Width             = (mfxU16)pInParams->nWidth;

    pMfxParams->mfx.FrameInfo.CropW             = (mfxU16)pInParams->nWidth;
    pMfxParams->mfx.FrameInfo.CropH             = (mfxU16)pInParams->nHeight;

    pMfxParams->mfx.FrameInfo.ChromaFormat      = MFX_CHROMAFORMAT_YUV420;

    switch (pMfxParams->mfx.CodecId)
    {
    case MFX_CODEC_MPEG2:
        pMfxParams->mfx.CodecProfile = MFX_PROFILE_MPEG2_HIGH;
        pMfxParams->mfx.CodecLevel = MFX_LEVEL_MPEG2_HIGH;
        break;    
    case MFX_CODEC_VC1:
        pMfxParams->mfx.CodecProfile = MFX_PROFILE_VC1_ADVANCED;
        pMfxParams->mfx.CodecLevel = MFX_LEVEL_VC1_4;
        break;    
    case MFX_CODEC_AVC:
        pMfxParams->mfx.CodecProfile = MFX_PROFILE_AVC_BASELINE;
        pMfxParams->mfx.CodecLevel = MFX_LEVEL_AVC_5;
        break;
    case MFX_CODEC_JPEG:
        pMfxParams->mfx.CodecProfile = MFX_PROFILE_JPEG_BASELINE;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

mfxStatus InitMfxBitstream(mfxBitstream *pBitstream, mfxVideoParam* pParams)
{
    mfxI32 maxSz;
    //check input params
    CHECK_POINTER(pBitstream, MFX_ERR_NULL_PTR);

    //prepare pCUC
    WipeMFXBitstream(pBitstream);
    memset(pBitstream, 0, sizeof(mfxBitstream));   // zero memory
    CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    //prepare buffer
    maxSz = pParams->mfx.FrameInfo.Height * pParams->mfx.FrameInfo.Width * 4;
    if (maxSz < pParams->mfx.BufferSizeInKB*1024)
      maxSz = pParams->mfx.BufferSizeInKB*1024;
    pBitstream->Data = new mfxU8[maxSz];
    CHECK_POINTER_SAFE(pBitstream->Data, MFX_ERR_MEMORY_ALLOC, WipeMFXBitstream(pBitstream)); //check alloc and safe exit in case of error

    pBitstream->MaxLength = maxSz;

    return MFX_ERR_NONE;
}

mfxStatus ExtendMFXBitstream(mfxBitstream* pBS)
{
    CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);

    mfxU8* pData = new mfxU8[pBS->MaxLength * 2];

    CHECK_POINTER(pData, MFX_ERR_MEMORY_ALLOC);

    memcpy(pData, pBS->Data + pBS->DataOffset, pBS->DataLength);

    WipeMFXBitstream(pBS);

    pBS->Data       = pData;
    pBS->DataOffset = 0;
    pBS->MaxLength  = pBS->MaxLength * 2;

    return MFX_ERR_NONE;
}

mfxStatus InitMfxFrameSurface(mfxFrameSurface1* pSurface, const mfxFrameInfo* pFrameInfo, mfxU32* pFrameSize, bool bPadding)
{
    mfxU8  nBitsPerPixel = 0;
    mfxU8  padding = (bPadding)? 32 : 0;

    //check input params
    CHECK_POINTER(pSurface, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pFrameInfo,  MFX_ERR_NULL_PTR);

    //prepare pSurface
    WipeMFXSurface(pSurface);

    //prepare pSurface->Info
    memset(&pSurface->Info, 0, sizeof(mfxFrameInfo));                       // zero memory
    pSurface->Info.FourCC           = pFrameInfo->FourCC;                    // FourCC type
    pSurface->Info.Width            = ((pFrameInfo->Width  + 2*padding + 15)>>4)<<4;
    pSurface->Info.Height           = ((pFrameInfo->Height + 2*padding + 31)>>5)<<5;
    pSurface->Info.PicStruct        = pFrameInfo->PicStruct;               // picture structure

    pSurface->Info.CropX = padding;
    pSurface->Info.CropY = padding;
    pSurface->Info.CropW = (pFrameInfo->CropW != 0)? pFrameInfo->CropW : pFrameInfo->Width;
    pSurface->Info.CropH = (pFrameInfo->CropH != 0)? pFrameInfo->CropH : pFrameInfo->Height;

    /*-------- Color format parameters---------------------*/

    switch(pFrameInfo->FourCC) {
    case MFX_FOURCC_YV12:
        nBitsPerPixel                 = 12;
        break;
    case MFX_FOURCC_YV16:
        nBitsPerPixel                 = 16;
        break;
    case MFX_FOURCC_RGB3:
        nBitsPerPixel                 = 24;
        break;
    case MFX_FOURCC_NV12:
        nBitsPerPixel                 = 12;
        break;
    case MFX_FOURCC_NV16:
        nBitsPerPixel                 = 16;
        break;
    case MFX_FOURCC_YUY2:
        nBitsPerPixel                 = 16;
        break;
    case MFX_FOURCC_YUV420_16:
    case MFX_FOURCC_P010:
    case MFX_FOURCC_YUV444_8:
        nBitsPerPixel                 = 24;
        break;
    case MFX_FOURCC_YUV422_16:
    case MFX_FOURCC_P210:
    case MFX_FOURCC_AYUV:
        nBitsPerPixel                 = 32;
    case MFX_FOURCC_YUV444_16:
    //case MFX_FOURCC_Y416:
        nBitsPerPixel                 = 64;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    if(pFrameSize != 0)
        *pFrameSize = (pSurface->Info.Width*pSurface->Info.Height*nBitsPerPixel)>>3;

    return MFX_ERR_NONE;
}
mfxStatus InitMfxFrameSurface(mfxFrameSurface* pSurface, mfxFrameInfo* pFrameInfo, mfxU32* pFrameSize, bool bPadding)
{
    mfxU8  nBitsPerPixel = 0;
    mfxU8  padding = (bPadding)? 32 : 0;

    //check input params
    CHECK_POINTER(pSurface, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pFrameInfo,  MFX_ERR_NULL_PTR);

    //prepare pSurface
    WipeMFXSurface(pSurface);

    //prepare pSurface->Info
    memset(&pSurface->Info, 0, sizeof(mfxFrameInfo));                       // zero memory
    pSurface->Info.FourCC           = pFrameInfo->FourCC;                    // FourCC type
    pSurface->Info.Width            = ((pFrameInfo->Width  + 2*padding + 15)>>4)<<4;
    pSurface->Info.Height           = ((pFrameInfo->Height + 2*padding + 31)>>5)<<5;
    pSurface->Info.PicStruct        = pFrameInfo->PicStruct;               // picture structure

    pSurface->Info.CropX = padding;
    pSurface->Info.CropY = padding;
    pSurface->Info.CropW = (pFrameInfo->CropW != 0)? pFrameInfo->CropW : pFrameInfo->Width;
    pSurface->Info.CropH = (pFrameInfo->CropH != 0)? pFrameInfo->CropH : pFrameInfo->Height;

    /*-------- Color format parameters---------------------*/

    switch(pFrameInfo->FourCC) {
    case MFX_FOURCC_YV12:
        nBitsPerPixel                 = 12;
        break;
    case MFX_FOURCC_RGB3:
        nBitsPerPixel                 = 24;
        break;
    case MFX_FOURCC_NV12:
        nBitsPerPixel                 = 12;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    if(pFrameSize != 0)
        *pFrameSize = (pSurface->Info.Width*pSurface->Info.Height*nBitsPerPixel)>>3;

    return MFX_ERR_NONE;
}
mfxStatus InitMFXFrameSurfaceDec(mfxFrameSurface1* pSurface, mfxVideoParam* pParams)
{
    CHECK_POINTER(pSurface, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams,  MFX_ERR_NULL_PTR);

    //prepare pSurface
    WipeMFXSurfaceDec(pSurface);

    //prepare pSurface->Info
    ZERO_MEMORY(pSurface->Info);
    pSurface->Info.FourCC           = pParams->mfx.FrameInfo.FourCC;
    pSurface->Info.Width            = pParams->mfx.FrameInfo.Width;
    pSurface->Info.Height           = pParams->mfx.FrameInfo.Height;
    pSurface->Info.ChromaFormat     = pParams->mfx.FrameInfo.ChromaFormat;
    pSurface->Info.CropW            = pSurface->Info.Width;
    pSurface->Info.CropH            = pSurface->Info.Height;

    ZERO_MEMORY(pSurface->Data);

    if (pSurface->Info.Width * pSurface->Info.Height)
    {
        //prepare planes
        pSurface->Data.Y = new mfxU8[pSurface->Info.Width * pSurface->Info.Height];
        CHECK_POINTER_SAFE(pSurface->Data.Y, MFX_ERR_MEMORY_ALLOC, WipeMFXSurfaceDec(pSurface));

        pSurface->Data.U = new mfxU8[pSurface->Info.Width * pSurface->Info.Height / 4]; 
        CHECK_POINTER_SAFE(pSurface->Data.U, MFX_ERR_MEMORY_ALLOC, WipeMFXSurfaceDec(pSurface));

        pSurface->Data.V = new mfxU8[pSurface->Info.Width * pSurface->Info.Height / 4];
        CHECK_POINTER_SAFE(pSurface->Data.V, MFX_ERR_MEMORY_ALLOC, WipeMFXSurfaceDec(pSurface));

        pSurface->Data.A = 0;  // no alpha is used        

        pSurface->Data.PitchLow = pSurface->Info.Width;
    }

    return MFX_ERR_NONE;
}
mfxStatus CreateMFXEncode(sFrameEncoder* pEncoder, mfxVideoParam* pParams, bool bMemoryAllocator, mfxIMPL impl)
{
    mfxStatus sts = MFX_ERR_NONE;

    //check input params
    CHECK_POINTER(pEncoder, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams,  MFX_ERR_NULL_PTR);

    //init mfxHDL
    sts = pEncoder->session.Init(impl, 0);                                 //init
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncode(pEncoder));    //check result

    //init MFXVideoENCODE
    pEncoder->pmfxENC = new MFXVideoENCODE(pEncoder->session);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncode(pEncoder));    //check result

    if (bMemoryAllocator)
    {
        sts = CreateBufferAllocator(&pEncoder->pBufferAllocator);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncode(pEncoder));   

        sts = CreateFrameAllocator(&pEncoder->pFrameAllocator);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncode(pEncoder));    

        sts = pEncoder->session.SetBufferAllocator(pEncoder->pBufferAllocator);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncode(pEncoder));    

        sts = pEncoder->session.SetFrameAllocator(pEncoder->pFrameAllocator);
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncode(pEncoder));  
   
    }  
    return MFX_ERR_NONE;
}
mfxStatus InitMFXEncode(sFrameEncoder* pEncoder, mfxVideoParam* pParams)
{
    mfxStatus sts = MFX_ERR_NONE;

    //check input params
    CHECK_POINTER(pEncoder, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams,  MFX_ERR_NULL_PTR);

    sts = pEncoder->pmfxENC->Init(pParams);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncode(pEncoder));    //check result
    
    return MFX_ERR_NONE;
}

mfxStatus InitMFXDecoder(sFrameDecoder* pDecoder, mfxVideoParam* pParams, mfxIMPL libType)
{
    CHECK_POINTER(pDecoder, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams,  MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    sts = pDecoder->session.Init(libType, 0);                               //init
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxDecoder(pDecoder));    //check result
 
    //init MFXVideoDECODE
    pDecoder->pmfxDEC = new MFXVideoDECODE(pDecoder->session);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxDecoder(pDecoder));

    sts = pDecoder->pmfxDEC->Init(pParams);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxDecoder(pDecoder));

    return MFX_ERR_NONE;
}

mfxStatus UpdateMFXParams(mfxVideoParam* pParams, sFrameDecoder* pDecoder)
{
    CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pDecoder, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pDecoder->pmfxDEC, MFX_ERR_NULL_PTR);

    mfxVideoParam mfxCurrentParams;
    mfxStatus     sts = MFX_ERR_NONE;

    sts = pDecoder->pmfxDEC->GetVideoParam(&mfxCurrentParams);
    if (MFX_ERR_NONE == sts) 
    {
        *pParams = mfxCurrentParams;
    }

    return sts;
}

mfxStatus InitMBParams(mfxMbParam* pMB, mfxU32 w, mfxU32 h)
{
    CHECK_POINTER(pMB,   MFX_ERR_NULL_PTR);

    pMB->NumMb = ((w + 15)>>4)*(((h+31)>>5)<<1); // MBs, not pixels!
    pMB->Mb = new mfxMbCode [pMB->NumMb];
    CHECK_POINTER(pMB->Mb, MFX_ERR_NULL_PTR);
    memset (pMB->Mb,0,sizeof(mfxMbCode)*pMB->NumMb);
    return MFX_ERR_NONE;
}

mfxStatus InitSliceParams(mfxSliceParam** ppSlice, mfxU32 numSlices)
{
    CHECK_POINTER(ppSlice,   MFX_ERR_NULL_PTR);
    *ppSlice = new mfxSliceParam [numSlices];
    CHECK_POINTER(*ppSlice, MFX_ERR_NULL_PTR);
    memset (*ppSlice, 0, sizeof(mfxSliceParam)*numSlices);
    return MFX_ERR_NONE;
}

mfxStatus InitMFXBuffer(mfxU8*** pBuf,mfxU32 size,mfxU32 nData)
{
    mfxU32 i;
    mfxU8**  p = 0;
    CHECK_POINTER(pBuf, MFX_ERR_NULL_PTR);
    p = new mfxU8* [nData];
    CHECK_POINTER(p, MFX_ERR_NULL_PTR);

    for (i = 0; i<nData; i++)
    {
        p[i] = new mfxU8 [size];
        CHECK_POINTER_SAFE(p[i], MFX_ERR_MEMORY_ALLOC, WipeMFXBuffer(p, (mfxU8)nData));
        memset(p[i], 0, size);
    }
    *pBuf = p;
    return MFX_ERR_NONE;
}

mfxStatus InitMfxFrameData(mfxFrameData*** pData, mfxFrameInfo* pParams, mfxU8** pBuffer, mfxU32 /*pBufferSize*/, mfxU32 nData)
{
    mfxFrameData        **p = 0;
    mfxU32              i = 0;

    CHECK_POINTER(pData,   MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pBuffer, MFX_ERR_NULL_PTR);

    p = new mfxFrameData* [nData];
    CHECK_POINTER(p, MFX_ERR_NULL_PTR);
    for(i = 0; i < nData; i++)
    {
        p[i] = new mfxFrameData;
        CHECK_POINTER(p[i], MFX_ERR_NULL_PTR);
    }

    for (i = 0 ; i < nData; i++)
    {
        memset(p[i], 0, sizeof(mfxFrameData));

        //prepare frames
        //sts = GetFramePointers(pParams->FourCC,pParams->Width,pParams->Height, pBuffer[i], p[i].Ch);
        //CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, sts);
        p[i]->Y = pBuffer[i];
        p[i]->PitchHigh = 0;
        p[i]->PitchLow = pParams->Width;
        switch(pParams->FourCC)
        {
        case MFX_FOURCC_YV12:
            p[i]->V = pBuffer[i] + pParams->Width*pParams->Height;
            p[i]->U = p[i]->V + ((pParams->Width*pParams->Height)>>2);
            break;
        case MFX_FOURCC_RGB3:
            p[i]->R = pBuffer[i] + 0;
            p[i]->G = pBuffer[i] + 1;
            p[i]->B = pBuffer[i] + 2;
            p[i]->PitchHigh = (mfxU16)(((mfxU32)pParams->Width * 3) / (1 << 16));
            p[i]->PitchLow = (mfxU16)(((mfxU32)pParams->Width * 3) % (1 << 16));
            break;
        case MFX_FOURCC_NV12:
            p[i]->U = pBuffer[i] + pParams->Width*pParams->Height;
            p[i]->V = p[i]->U + 1;
            break;
        default:
            p[i]->Y = 0; // restore 0
            p[i]->PitchLow = 0;
            return MFX_ERR_UNSUPPORTED;
        }

        p[i]->A = 0;

        //data length
        //p[i]->DataLength = pBufferSize;
    }
    *pData   = p;
    return MFX_ERR_NONE;
}

mfxStatus InitMfxFrameData(mfxFrameData*** pData, mfxFrameAllocator* pFrameAllocator, mfxFrameAllocRequest *allocRequest, mfxU32 *nData)
{
    mfxFrameData            **p     = 0;
    mfxU32                  i       = 0;
    mfxStatus               sts     = MFX_ERR_NONE;
    mfxFrameAllocResponse   response;

    CHECK_POINTER(pData,            MFX_ERR_NULL_PTR);
    CHECK_POINTER(pFrameAllocator,  MFX_ERR_NULL_PTR);
    CHECK_POINTER(allocRequest,     MFX_ERR_NULL_PTR);
    CHECK_POINTER(nData,            MFX_ERR_NULL_PTR);

    sts = CreateExternalFrames(pFrameAllocator->pthis, allocRequest, & response);
    CHECK_RESULT(sts, MFX_ERR_NONE, sts); 

    *nData = response.NumFrameActual;

    p = new mfxFrameData* [*nData];
    CHECK_POINTER(p, MFX_ERR_NULL_PTR);

    for(i = 0; i < *nData; i++)
    {
        p[i] = new mfxFrameData;
        CHECK_POINTER(p[i], MFX_ERR_NULL_PTR);
        memset(p[i],0,sizeof(mfxFrameData));
        p[i]->MemId = response.mids[i];

    } 
    *pData = p;
    return MFX_ERR_NONE;
}


mfxStatus InitMfxFrameSurfaces(mfxFrameSurface1** ppSurfaces, mfxFrameInfo* pParams, mfxU8** pBuffer, mfxU32 , mfxU32 nData)
{
  mfxU32              i = 0;

  CHECK_POINTER(ppSurfaces, MFX_ERR_NULL_PTR);
  CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);
  CHECK_POINTER(pBuffer,    MFX_ERR_NULL_PTR);

  *ppSurfaces = new mfxFrameSurface1 [nData];
  CHECK_POINTER(*ppSurfaces, MFX_ERR_NULL_PTR);
  memset(*ppSurfaces, 0, nData*sizeof(mfxFrameSurface1));

  for(i = 0; i < nData; i++)
  {
    (*ppSurfaces)[i].Info = *pParams;
  }

  for (i = 0 ; i < nData; i++)
  {
    mfxFrameData  *p = &(*ppSurfaces)[i].Data;

    //prepare frames
    //sts = GetFramePointers(pParams->FourCC,pParams->Width,pParams->Height, pBuffer[i], p[i].Ch);
    //CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, sts);
    p->Y = pBuffer[i];
    p->PitchHigh = 0;
    p->PitchLow = pParams->Width;
    switch(pParams->FourCC)
    {
    case MFX_FOURCC_YV12:
      p->V = pBuffer[i] + pParams->Width*pParams->Height;
      p->U = p->V + ((pParams->Width*pParams->Height)>>2);
      break;
    case MFX_FOURCC_RGB3:
      p->R = pBuffer[i] + 0;
      p->G = pBuffer[i] + 1;
      p->B = pBuffer[i] + 2;
      p->PitchHigh = (mfxU16)(((mfxU32)pParams->Width * 3) / (1 << 16));
      p->PitchLow = (mfxU16)(((mfxU32)pParams->Width * 3) % (1 << 16));
      break;
    case MFX_FOURCC_NV12:
      p->U = pBuffer[i] + pParams->Width*pParams->Height;
      p->V = p->U + 1;
      break;
    default:
      p->Y = 0; // restore 0
      p->PitchLow = 0;
      return MFX_ERR_UNSUPPORTED;
    }

    p->A = 0;

    //data length
    //p[i]->DataLength = pBufferSize;
  }
  return MFX_ERR_NONE;
}


mfxStatus InitMfxFrameSurfaces(mfxFrameSurface1** ppSurfaces, mfxFrameAllocator* pFrameAllocator, mfxFrameAllocRequest *allocRequest, mfxU32 *nData)
{
  mfxU32                  i       = 0;
  mfxStatus               sts     = MFX_ERR_NONE;
  mfxFrameAllocResponse   response;

  CHECK_POINTER(ppSurfaces,       MFX_ERR_NULL_PTR);
  CHECK_POINTER(pFrameAllocator,  MFX_ERR_NULL_PTR);
  CHECK_POINTER(allocRequest,     MFX_ERR_NULL_PTR);
  CHECK_POINTER(nData,            MFX_ERR_NULL_PTR);

  sts = CreateExternalFrames(pFrameAllocator->pthis, allocRequest, & response);
  CHECK_RESULT(sts, MFX_ERR_NONE, sts); 

  *nData = response.NumFrameActual;

  *ppSurfaces = new mfxFrameSurface1 [*nData];
  CHECK_POINTER(*ppSurfaces, MFX_ERR_NULL_PTR);
  memset(*ppSurfaces, 0, *nData*sizeof(mfxFrameSurface1));

  for(i = 0; i < *nData; i++)
  {
    (*ppSurfaces)[i].Data.MemId = response.mids[i];
    (*ppSurfaces)[i].Info = allocRequest->Info;
  } 
  return MFX_ERR_NONE;
}



mfxStatus FourCC2ChromaFormat(mfxU32 FourCC, mfxU16 *pChromaFormat)
{
    CHECK_POINTER(pChromaFormat, MFX_ERR_NULL_PTR);

    switch(FourCC)
    {
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_NV12:
        *pChromaFormat = MFX_CHROMAFORMAT_YUV420;
        break;
    case MFX_FOURCC_RGB3:
        *pChromaFormat = MFX_CHROMAFORMAT_YUV444;
        break;
    default:
        *pChromaFormat = 0;
        return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}

mfxStatus SetDefaultProfileLevel (mfxU32 CodecId, mfxU16 *pProfile, mfxU16 *pLevel)
{
    CHECK_POINTER(pProfile, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pLevel,  MFX_ERR_NULL_PTR);

    switch (CodecId)
    {
    case  MFX_CODEC_AVC:
        *pProfile = MFX_PROFILE_AVC_MAIN;
        *pLevel   = MFX_LEVEL_AVC_5;
        break;
    case  MFX_CODEC_VC1:
        *pProfile = MFX_PROFILE_VC1_ADVANCED;
        *pLevel   = MFX_LEVEL_VC1_4;
        break;
    case  MFX_CODEC_MPEG2:
        *pProfile = MFX_PROFILE_MPEG2_HIGH;
        *pLevel   = MFX_LEVEL_MPEG2_HIGH;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}

mfxStatus GetFreeFrameIndex(mfxFrameData** pData, mfxU32 nData, mfxU32 *pIndex)
{
    for (mfxU32 i=0; i<nData; i++)
    {
        if (!pData[i]->Locked)
        {
            *pIndex = i;
            return MFX_ERR_NONE;
        }

    }
    return MFX_ERR_NOT_ENOUGH_BUFFER;
}
mfxStatus GetFreeFrameIndex(mfxFrameSurface1* pSurface, mfxU32 nData, mfxU32 *pIndex)
{
    for (mfxU32 i=0; i<nData; i++)
    {
        if (!pSurface[i].Data.Locked)
        {
            *pIndex = i;
            return MFX_ERR_NONE;
        }

    }
    return MFX_ERR_NOT_ENOUGH_BUFFER;
}
mfxStatus GetFrameIndex(mfxFrameData* pFrame, mfxFrameData** pFrameArray, mfxU32 nFrames, mfxU32 *pIndex)
{
    for (mfxU32 i=0; i< nFrames; i++)
    {
        if (pFrameArray[i] == pFrame)
        {
            *pIndex = i;
            return MFX_ERR_NONE;
        }
    }
    return MFX_ERR_NOT_ENOUGH_BUFFER;
}

mfxU8 GetFrameType (mfxU16 FrameOrder, mfxInfoMFX* info)
{
    mfxU16 GOPSize = info->GopPicSize;
    mfxU16 IPDist = info->GopRefDist;
    mfxU16 pos = (GOPSize)? FrameOrder %(GOPSize):FrameOrder;

    if (pos == 0 || IPDist == 0) {
        return MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
    }
    pos = pos % (IPDist);
    return (mfxU8)((pos != 0) ? MFX_FRAMETYPE_B : MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);
}

void GetFrameType (mfxU16 FrameOrder, mfxInfoMFX* info, bool interlace, mfxU8* FrameType, mfxU8* FrameType2)
{
    mfxU16 GOPSize = info->GopPicSize;
    mfxU16 IPDist = info->GopRefDist;
    mfxU16 pos = (GOPSize)? FrameOrder %(GOPSize):FrameOrder;

    *FrameType2 = 0;

    if (pos == 0 || IPDist == 0)
    {
        *FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;

        if(interlace)
            *FrameType2 =  *FrameType;
    }
    else
    {
        pos = pos % (IPDist);
        (pos != 0) ? *FrameType = MFX_FRAMETYPE_B : *FrameType = (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);

        if(interlace)
            *FrameType2 =  *FrameType;
    }
}

void WipeMFXBitstream(mfxBitstream *pBitstream)
{
    //check input params
    CHECK_POINTER_NO_RET(pBitstream);
    CHECK_POINTER_NO_RET(pBitstream->Data);

    //free allocated memory
    SAFE_DELETE_ARRAY(pBitstream->Data);
}

void WipeMFXSurface(mfxFrameSurface1* pSurface)
{
    //check input params
    CHECK_POINTER_NO_RET(pSurface);
}
void WipeMFXSurface(mfxFrameSurface* pSurface)
{
    //check input params
    CHECK_POINTER_NO_RET(pSurface);
}
void WipeMFXSurfaceDec( mfxFrameSurface1* pSurface )
{
    CHECK_POINTER_NO_RET(pSurface);

    //free allocated memory
    SAFE_DELETE_ARRAY(pSurface->Data.Y);
    SAFE_DELETE_ARRAY(pSurface->Data.U);
    SAFE_DELETE_ARRAY(pSurface->Data.V);
}

void WipeMFXBuffer(mfxU8** pData,mfxU8 n)
{
    mfxU32 i;
    CHECK_POINTER_NO_RET(pData);
    for (i = 0; i<n; i++)
    {
        SAFE_DELETE_ARRAY (pData[i]);
    }
    SAFE_DELETE_ARRAY(pData);
}
void WipeMFXFrameData(mfxFrameData** pData, mfxU8 FrameNum)
{
    CHECK_POINTER_NO_RET(pData);
    for(mfxU8 i = 0; i < FrameNum; i++)
        SAFE_DELETE(pData[i]);

    SAFE_DELETE_ARRAY(pData);
}
void WipeMFXDataPlanes(sMFXPlanes* pPlanes)
{
    CHECK_POINTER_NO_RET(pPlanes);
    SAFE_DELETE_ARRAY(pPlanes);
}

void WipeMFXEncode(sFrameEncoder* pEncoder)
{
    //check input params
    CHECK_POINTER_NO_RET(pEncoder);

    //free allocated memory
    SAFE_DELETE(pEncoder->pmfxENC);

    if (pEncoder->pFrameAllocator)
    {
        DeleteAllFrames(pEncoder->pFrameAllocator->pthis);
        DeleteFrameAllocator(&pEncoder->pFrameAllocator);
    }
    if (pEncoder->pBufferAllocator)
    {
        DeleteBufferAllocator(&pEncoder->pBufferAllocator);
    }
    pEncoder->session.Close();
}


void WipeMFXDecoder( sFrameDecoder* pDecoder )
{
    CHECK_POINTER_NO_RET(pDecoder);

    //free allocated memory
    SAFE_DELETE(pDecoder->pmfxDEC);
    /*if (pDecoder->pFrameAllocator)
    {
        DeleteAllFrames(pDecoder->pFrameAllocator);
        DeleteFrameAllocator(pDecoder->pFrameAllocator);
        pDecoder->pFrameAllocator = 0;
    }
    if (pDecoder->pBufferAllocator)
    {
        DeleteBufferAllocator(pDecoder->pBufferAllocator);
        pDecoder->pBufferAllocator = 0; 
    }*/
    pDecoder->session.Close();
}

void WipeMFXMbParams(mfxMbParam *pMBParams)
{
    CHECK_POINTER_NO_RET(pMBParams);
    SAFE_DELETE_ARRAY(pMBParams->Mb);
}
void WipeMFXSliceParams(mfxSliceParam *pSliceParams)
{
    CHECK_POINTER_NO_RET(pSliceParams);
    SAFE_DELETE_ARRAY(pSliceParams);
}

void WipeFrameCUCExBuffer(mfxFrameCUC *cuc)
{
    if (cuc->NumExtBuffer && cuc->ExtBuffer)
    {
        for (mfxU32 i = 0; i < cuc->NumExtBuffer; i ++)
        {
            delete []  (cuc->ExtBuffer[i]);
            cuc->ExtBuffer[i] = 0;
        }
        delete [] cuc->ExtBuffer;
        cuc->ExtBuffer = 0;
    }
}
void WipeVideoParamsExBuffer(mfxVideoParam * /*par*/)
{
    assert(0);
    //if (par->mfx.NumExtBuffer && par->mfx.ExtBuffer)
    //{
    //  for (mfxU32 i = 0; i < par->mfx.NumExtBuffer; i ++)
    //  {
    //    delete []  (par->mfx.ExtBuffer[i]);
    //    par->mfx.ExtBuffer[i] = 0;
    //  }
    //  delete [] par->mfx.ExtBuffer;
    //  par->mfx.ExtBuffer = 0;
    //}
}
void WipeMemory(mfxBitstream *pBitstream,sFrameEncoder* pEncoder, mfxU8** pBuf, mfxU8 numData,  mfxMbParam * /*pMBParams*/, mfxFrameCUC * /*cuc*/, mfxVideoParam * /*par*/)
{
    WipeMFXEncode(pEncoder);    
    
    WipeMFXBitstream(pBitstream);

    WipeMFXBuffer(pBuf,numData);

    //WipeMFXDataPlanes(pPlanes);

}

void WipeMfxDecoder( sFrameDecoder* pDecoder )
{
    CHECK_POINTER_NO_RET(pDecoder);

    //free allocated memory
    SAFE_DELETE(pDecoder->pmfxDEC);
    pDecoder->session.Close();
}

mfxStatus InitMfxDataPlanes(sMFXPlanes ** pPlanes, mfxFrameData** pData, mfxFrameInfo* pParams,mfxVideoParam* pVideoParams, mfxU32 nData)
{
    mfxStatus sts           = MFX_ERR_NONE;
    mfxU8     pNPlanes      = 0;
    mfxU8     pFrameNum[4]  = {0};

    if (pVideoParams->mfx.FrameInfo.FourCC != MFX_FOURCC_YV12 && pVideoParams->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12)
        return MFX_ERR_UNSUPPORTED;

    sMFXPlanes *p = 0;

    CHECK_POINTER(pData,        MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams,      MFX_ERR_NULL_PTR);
    CHECK_POINTER(pVideoParams, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pPlanes,      MFX_ERR_NULL_PTR);

    p = new sMFXPlanes [nData];
    CHECK_POINTER(p, MFX_ERR_NULL_PTR);

    sts = GetPlanes (pParams->FourCC, &pNPlanes, pFrameNum);
    CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, sts);

    if (pNPlanes >= 4)
        return MFX_ERR_UNSUPPORTED;

    sts = GetPitch(pVideoParams->mfx.FrameInfo.FourCC, pParams->Width, &pData[0]->PitchLow);
    CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, sts);

    mfxU32  h = (pVideoParams->mfx.FrameInfo.CropH) ? pVideoParams->mfx.FrameInfo.CropH : pVideoParams->mfx.FrameInfo.Height;
    mfxU32  w = (pVideoParams->mfx.FrameInfo.CropW) ? pVideoParams->mfx.FrameInfo.CropW : pVideoParams->mfx.FrameInfo.Width;

    mfxU16 step   = 1;
    mfxU32 scaleX = 1;
    mfxU32 pitch = 0;

    if (pVideoParams->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12)
    {
        step    = 2;
        scaleX  = 0;
    }

    for (mfxU32 i = 0 ; i < nData; i++)
    {
        p[i].nPlanes = pNPlanes;

        p[i].pPlane[0] = pData[i]->Y;
        p[i].pPlane[pFrameNum[1]] = pData[i]->U;
        p[i].pPlane[pFrameNum[2]] = pData[i]->V;

        pitch = pData[i]->PitchLow + ((mfxU32)pData[i]->PitchHigh << 16);

        for ( mfxU32 j = 0; j<pNPlanes; j++)
        {
            if (pFrameNum[j] == 0)
            {
                p[i].pPitch[j]  = pitch;
                p[i].pHeight[j] = h;
                p[i].pWidth[j]  = w;
                p[i].pPlane[j] += GetSubPicOfset(pVideoParams->mfx.FrameInfo.CropX,pVideoParams->mfx.FrameInfo.CropY, (mfxU16)pitch, 1);
            }
            else
            {
                p[i].pPitch[j]  = pitch>>scaleX;
                p[i].pHeight[j] = h >> 1;
                p[i].pWidth[j]  =(w >>1) * step ;
                p[i].pPlane[j] += GetSubPicOfset((mfxI16)(pVideoParams->mfx.FrameInfo.CropX>>scaleX),
                                                 (mfxI16)(pVideoParams->mfx.FrameInfo.CropY>>1), (mfxU16)p[i].pPitch[j], step);
            }
        }
    }
    * pPlanes = p ;
    return MFX_ERR_NONE;
}

mfxStatus SetDataInSurface(mfxFrameSurface* pSurface, mfxFrameData** pData, mfxU8 NumFrameData)
{

    pSurface->NumFrameData = NumFrameData;
    pSurface->Data = pData;

    return MFX_ERR_NONE;
}
