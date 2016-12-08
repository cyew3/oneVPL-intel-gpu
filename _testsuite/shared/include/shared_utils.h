/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2008-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __SHARED_UTILS_H
#define __SHARED_UTILS_H

#include <stdio.h>
#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "mfxstructurespro.h"
#include "mfxjpeg.h"
#include "cc_utils.h"
#include "app_defs.h"
#include "memory_allocator.h"
#include "mfx_io_utils.h"

#if defined(_ENABLE_PRO_API)
#define _ENABLE_ENC_PAK_FUNCTIONS
#endif

struct sInputParams
{
  mfxU32 videoType;
  mfxU32 nWidth;
  mfxU32 nHeight;
  mfxF64 dFrameRate;
  mfxU32 nBitRate;
  mfxU32 nGOPsize;
  mfxU32 nGopOptFlag;
  mfxU32 nRefDist;
  mfxU32 nSeqSize;
  mfxU32 bEOS;       // 1 - Insert EOS after each seq; 0 - no EOS symbol;
  mfxU32 nFrames;
  mfxU32 picStruct; // 0-progressive, 1-tff, 2-bff, 3-field tff, 4-field bff
  mfxU32 entryMode; // 0-encode, 1-VME+FullPAK, 2-FullENC+BSP
  mfxU32 nThreads;
  mfxU32 HWAcceleration; // 0=SW, 1=HW+SW, 2=SW+HW, 3=HW
  mfxU32 targetUsage;
  mfxU32 numSlices;
  mfxU32 numRef;
  mfxU32 CabacCavlc;
  mfxU32 deblocking;
  mfxU32 nFourCC;
  mfxU32 memType;  // 0 - external memory allocator isn't used, type of input frames - system memory
                   // 1 - external memory allocator is    used, type of input frames - system memory
                   // 2 - external memory allocator is    used, type of input frames - D3D
  mfxU32 nDiff;    // mpeg2 difference tollerance
  mfxU32 EncodedOrder;  //0 - display order
                        //1 - encoded order
  bool   isDefaultFC;
  vm_char  strSrcFile[MAX_FILELEN];

  const vm_char* strDstFile;
  const vm_char* strParFile;
  const vm_char* perfFile;
  const vm_char* refFile;

  mfxU32 nHRDBufSizeInKB;
  mfxU32 nHRDInitDelayInKB;
  mfxU32 nMaxBitrate;
};

enum sEntryPoints
{
    MFX_ENCODE                     =   0,
    MFX_VME_ENC_FULL_PAK           =   1, /* frame level function */
    MFX_FULL_ENC_PAK_BSP           =   2, /* frame level function */
    MFX_VME_ENC_FULL_PAK_SLICES    =   3, /* slice level function */
    MFX_FULL_ENC_PAK_BSP_SLICES    =   4, /* slice level function */

};

enum
{
    MFX_FOURCC_UNKNOWN      = MFX_MAKEFOURCC('0','0','0','0'),
    MFX_FOURCC_YUV420_16    = MFX_MAKEFOURCC('M','0','1','6'),   // planar merged YUV 420 16 bits
    MFX_FOURCC_YUV422_16    = MFX_MAKEFOURCC('M','2','1','6'),   // planar merged YUV 422 16 bits
    MFX_FOURCC_YUV444_16    = MFX_MAKEFOURCC('M','4','1','6'),   // planar merged YUV 444 16 bits
    MFX_FOURCC_YV16         = MFX_MAKEFOURCC('Y','V','1','6'),   // planar merged YUV 422 8 bits
    MFX_FOURCC_YUV444_8     = MFX_MAKEFOURCC('I','4','4','4'),   // planar merged YUV 444 8 bits
};

struct sFrameEncoder
{
  MFXVideoSession       session;
  //MFXVideoBRC*          pmfxBRC;
  MFXVideoENCODE*       pmfxENC;
  mfxBufferAllocator*   pBufferAllocator;
  mfxFrameAllocator*    pFrameAllocator;

  sFrameEncoder () :
    //pmfxBRC(0),
    pmfxENC(0),
    pBufferAllocator(0),
    pFrameAllocator(0)
    {}
};

/*struct sFrameEncoderEx
{
  MFXVideoSession       session;
  MFXVideoBRC*          pmfxBRC;
  MFXVideoENC*          pmfxENC;
  MFXVideoPAK*          pmfxPAK;
  mfxBufferAllocator*   pBufferAllocator;
  mfxFrameAllocator*    pFrameAllocator;

  sFrameEncoderEx () :
    pmfxBRC(0),
    pmfxENC(0),
    pmfxPAK(0),
    pBufferAllocator(0),
    pFrameAllocator(0)
    {}
};*/

struct sMFXPlanes
{
    mfxU32              nPlanes;
    mfxU8*              pPlane[4];
    mfxU32              pWidth[4];
    mfxU32              pHeight[4];
    mfxU32              pPitch[4];
};

struct sFrameDecoder
{
    MFXVideoSession         session;
    MFXVideoDECODE*         pmfxDEC;
    mfxBufferAllocator*     pBufferAllocator;
    mfxFrameAllocator*      pFrameAllocator;

    sFrameDecoder() :
        pmfxDEC(0),
        pBufferAllocator(0),
        pFrameAllocator(0)
        {}
};

void PrintHelp(vm_char *strAppName, vm_char *strErrorMessage);
mfxStatus ParseInputString(vm_char* strInput[], int nArgNum, sInputParams* pParams);
mfxStatus PrintCurrentParams(sInputParams* pParams);
mfxStatus InitMFXParamsDec(mfxVideoParam* pMFXParams, sInputParams* pInParams);
mfxStatus InitMFXParams(mfxVideoParam* pMFXParams, sInputParams* pInParams);
mfxStatus InitMfxFrameSurface(mfxFrameSurface* pSurface, mfxFrameInfo* pFrameInfo, mfxU32* pFrameSize, bool bPadding = false);
mfxStatus InitMFXFrameSurfaceDec(mfxFrameSurface1* pSurface, mfxVideoParam* pParams);
mfxStatus InitMBParams(mfxMbParam* pMB, mfxU32 w, mfxU32 h);
mfxStatus InitSliceParams(mfxSliceParam** ppSlice, mfxU32 numSlices);
mfxStatus InitMFXBuffer(mfxU8*** pBuf, mfxU32 size, mfxU32 nData);
mfxStatus InitMfxFrameData(mfxFrameData*** pData, mfxFrameInfo* pParams, mfxU8** pBuffer, mfxU32 pBufferSize, mfxU32 nData);
mfxStatus InitMfxFrameData(mfxFrameData*** pData, mfxFrameAllocator* pFrameAllocator, mfxFrameAllocRequest *allocRequest, mfxU32 *nData);

mfxStatus InitMfxFrameSurfaces(mfxFrameSurface1** ppSurfaces, mfxFrameInfo* pParams, mfxU8** pBuffer, mfxU32 pBufferSize, mfxU32 nData);
mfxStatus InitMfxFrameSurfaces(mfxFrameSurface1** ppSurfaces, mfxFrameAllocator* pFrameAllocator, mfxFrameAllocRequest *allocRequest, mfxU32 *nData);

mfxStatus InitMFXEncode(sFrameEncoder* pEncoder, mfxVideoParam* pParams);

mfxStatus CreateMFXEncode(sFrameEncoder* pEncoder, mfxVideoParam* pParams, bool bMemoryAllocator, mfxIMPL impl = MFX_IMPL_SOFTWARE);

mfxStatus InitMFXDecoder(sFrameDecoder* pDecoder, mfxVideoParam* pParams, mfxIMPL libType);
mfxStatus ExtendMFXBitstream(mfxBitstream* pBS);
mfxStatus UpdateMFXParams(mfxVideoParam* pParams, sFrameDecoder* pDecoder);
mfxStatus FourCC2ChromaFormat(mfxU32 FourCC, mfxU16 *pChromaFormat);
mfxStatus SetDefaultProfileLevel (mfxU32 CodecId, mfxU16 *pProfile, mfxU16 *pLevel);
mfxStatus GetFreeFrameIndex(mfxFrameData** pData, mfxU32 nData, mfxU32 *pIndex);
mfxStatus GetFreeFrameIndex(mfxFrameSurface1* pSurface, mfxU32 nData, mfxU32 *pIndex);
mfxStatus GetFrameIndex(mfxFrameData* pFrame, mfxFrameData** pFrameArray, mfxU32 nFrames, mfxU32 *pIndex);
mfxU8 GetFrameType (mfxU16 FrameOrder, mfxInfoMFX* info);
void GetFrameType (mfxU16 FrameOrder, mfxInfoMFX* info, bool interlace, mfxU8* FrameType, mfxU8* FrameType2);

void mfxGetFrameSizeParam (mfxVideoParam* pMFXParams, mfxU16 w, mfxU16 h, bool bProg);

void WipeMFXBitstream(mfxBitstream *pBitstream);
void WipeMFXSurface(mfxFrameSurface1* pSurface);
void WipeMFXSurface(mfxFrameSurface*  pSurface);
void WipeMFXSurfaceDec(mfxFrameSurface1* pSurface );
void WipeMFXBuffer(mfxU8** pData, mfxU8 n);
void WipeMFXFrameData(mfxFrameData** pData, mfxU8 FrameNum);
void WipeMFXDataPlanes(sMFXPlanes* pPlanes);
void WipeMFXEncode(sFrameEncoder* pEncoder);
void WipeMFXMbParams(mfxMbParam *pMBParams);


#if defined(_ENABLE_ENC_PAK_FUNCTIONS)
inline void WipeMFXEncPak(sFrameEncoderEx* pEncoder)
{
    //check input params
    CHECK_POINTER_NO_RET(pEncoder);

    //free allocated memory
    SAFE_DELETE(pEncoder->pmfxBRC);
    SAFE_DELETE(pEncoder->pmfxENC);
    SAFE_DELETE(pEncoder->pmfxPAK);

    /*if (pEncoder->pFrameAllocator)
    {
        DeleteAllFrames(pEncoder->pFrameAllocator);
        DeleteFrameAllocator(pEncoder->pFrameAllocator);
        pEncoder->pFrameAllocator = 0;
    }
    if (pEncoder->pBufferAllocator)
    {
        DeleteBufferAllocator(pEncoder->pBufferAllocator);
        pEncoder->pBufferAllocator = 0; 
    }*/

    pEncoder->session.Close();
}
inline void WipeMemoryEx(mfxBitstream *pBitstream,sFrameEncoderEx* pEncoder, mfxU8** pBuf, mfxU8 numData, mfxFrameData** pData, mfxU8 FrameNum,/*sMFXPlanes * pPlanes,*/ mfxMbParam *pMBParams, mfxFrameCUC * /*cuc*/, mfxVideoParam * /*par*/)
{
    //wipe MFXFrameCUC
    WipeMFXBitstream(pBitstream);

    // wipe sFrameEncoder
    WipeMFXEncPak(pEncoder);

    // wipe MFXSurface
    WipeMFXBuffer(pBuf,numData);

    WipeMFXFrameData(pData, FrameNum);
    //WipeMFXDataPlanes(pPlanes);

    WipeMFXMbParams(pMBParams);
}

inline mfxStatus InitMFXEncPak(sFrameEncoderEx* pEncoder,
                        mfxVideoParam*  pParams,
                        mfxU32          HWAcceleration=0)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxIMPL ENC_impl = (HWAcceleration & 1) ? MFX_IMPL_HARDWARE : MFX_IMPL_SOFTWARE;
    mfxIMPL PAK_impl = (HWAcceleration & 2) ? MFX_IMPL_HARDWARE : MFX_IMPL_SOFTWARE;

    //check input params
    CHECK_POINTER(pEncoder, MFX_ERR_NULL_PTR);
    CHECK_POINTER(pParams,  MFX_ERR_NULL_PTR);

    if (PAK_impl != ENC_impl)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    //init MFXVideoCORE
    sts = pEncoder->session.Init(ENC_impl, 0);            //init
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncPak(pEncoder));  //check result

    pEncoder->pmfxBRC = new MFXVideoBRC(pEncoder->session);                 //no init, because internal BRC is used. Is used to avoid crash
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncPak(pEncoder));    //check result

    sts = pEncoder->pmfxBRC->Init(pParams);
    if(sts != MFX_ERR_UNSUPPORTED) {
        CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncPak(pEncoder));
    }

    //init MFXVideoENC
    pEncoder->pmfxENC = new MFXVideoENC(pEncoder->session);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncPak(pEncoder));  //check result

    sts = pEncoder->pmfxENC->Init(pParams);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncPak(pEncoder));  //check result

    //init MFXVideoPAK
    pEncoder->pmfxPAK = new MFXVideoPAK(pEncoder->session);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncPak(pEncoder));  //check result

    sts = pEncoder->pmfxPAK->Init(pParams);
    CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMFXEncPak(pEncoder));  //check result

    return MFX_ERR_NONE;
}

#endif // defined(_ENABLE_ENC_PAK_FUNCTIONS)

void WipeMfxDecoder(sFrameDecoder* pDecoder);
void WipeMFXSliceParams(mfxSliceParam *pSliceParams);
void WipeFrameCUCExBuffer(mfxFrameCUC *cuc);
void WipeVideoParamsExBuffer(mfxVideoParam *par);
void WipeMemory(mfxBitstream *pBitstream, sFrameEncoder* pEncoder, mfxU8** pBuf, mfxU8 numData, mfxMbParam * , mfxFrameCUC *cuc, mfxVideoParam *par);

mfxStatus SetDataInSurface(mfxFrameSurface* pSurface, mfxFrameData** pData, mfxU8 NumFrameData);
mfxStatus InitMfxDataPlanes(sMFXPlanes ** pPlanes, mfxFrameData** pData, mfxFrameInfo* pParams,mfxVideoParam* pVideoParams, mfxU32 nData);

#endif /*__SHARED_UTILS_H */
