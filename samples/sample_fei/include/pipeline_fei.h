//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2005-2015 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __PIPELINE_ENCODE_H__
#define __PIPELINE_ENCODE_H__

#include "sample_defs.h"
#include "hw_device.h"

#include <mfxfei.h>

#ifdef D3D_SURFACES_SUPPORT
#pragma warning(disable : 4201)
#endif

#include "sample_utils.h"
#include "base_allocator.h"

#include "mfxmvc.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"

#include <vector>
#include <memory>
#include <algorithm>


enum {
    MVC_DISABLED          = 0x0,
    MVC_ENABLED           = 0x1,
    MVC_VIEWOUTPUT        = 0x2,    // 2 output bitstreams
};

enum MemType {
    SYSTEM_MEMORY = 0x00,
    D3D9_MEMORY   = 0x01,
    D3D11_MEMORY  = 0x02,
};

//#define PREENC_ALLOC 1
//#define PAK_ALLOC    2

struct sInputParams
{
    mfxU16 nTargetUsage;
    mfxU32 CodecId;
    mfxU32 ColorFormat;
    mfxU16 nPicStruct;
    mfxU16 nWidth; // source picture width
    mfxU16 nHeight; // source picture height
    mfxF64 dFrameRate;
    mfxU16 nBitRate;
    mfxU32 nNumFrames;
    mfxU16 refDist; //number of frames to next I,P
    mfxU16 gopSize; //number of frames to next I
    mfxU8  QP;
    mfxU16 numSlices;
    mfxU16 numRef;
    mfxU16 bRefType;
    mfxU16 nIdrInterval;

    mfxU16 SearchWindow;
    mfxU16 LenSP;
    mfxU16 SearchPath;
    mfxU16 RefWidth;
    mfxU16 RefHeight;
    mfxU16 SubMBPartMask;
    mfxU16 SubPelMode;
    mfxU16 IntraPartMask;
    mfxU16 IntraSAD;
    mfxU16 InterSAD;
    mfxU16 NumMVPredictors;
    mfxU16 GopOptFlag;
    mfxU16 CodecProfile;
    mfxU16 CodecLevel;
    mfxU16 Trellis;

    mfxU16 nDstWidth; // destination picture width, specified if resizing required
    mfxU16 nDstHeight; // destination picture height, specified if resizing required

    MemType memType;
    bool bUseHWLib; // true if application wants to use HW MSDK library

    msdk_char strSrcFile[MSDK_MAX_FILENAME_LEN];

    std::vector<msdk_char*> srcFileBuff;
    std::vector<msdk_char*> dstFileBuff;

    bool bENCODE;
    bool bENCPAK;
    bool bOnlyENC;
    bool bOnlyPAK;
    bool bPREENC;
    bool bMBSize;
    bool bPassHeaders;
    bool Enable8x8Stat;
    bool AdaptiveSearch;
    bool FTEnable;
    bool RepartitionCheckEnable;
    bool MultiPredL0;
    bool MultiPredL1;
    bool DistortionType;
    bool ColocatedMbDistortion;
    bool bRepackPreencMV;
    msdk_char* mvinFile;
    msdk_char* mbctrinFile;
    msdk_char* mvoutFile;
    msdk_char* mbcodeoutFile;
    msdk_char* mbstatoutFile;
    msdk_char* mbQpFile;
};

typedef struct{
    mfxU16      NumExtParam;
    mfxExtBuffer **ExtParam;
} setElem;

typedef struct{
    setElem in;
    setElem out;
} IObuffs;

typedef struct
{
    bool vacant;
    IObuffs I_bufs;
    IObuffs PB_bufs;
} bufSet;

bufSet* getFreeBufSet(std::list<bufSet*> bufs);

//for PreEnc reordering
struct iTask{
    mfxENCInput in;
    mfxENCOutput out;
    mfxPAKInput inPAK;
    mfxPAKOutput outPAK;
    mfxU16 frameType;
    mfxU32 frameDisplayOrder;
    mfxSyncPoint EncSyncP;
    mfxI32 encoded;
    bufSet* bufs;
    bufSet* preenc_bufs;
    iTask* prevTask;
    iTask* nextTask;
};

struct sTask
{
    mfxBitstream mfxBS;
    mfxSyncPoint EncSyncP;
    std::list<mfxSyncPoint> DependentVppTasks;
    CSmplBitstreamWriter *pWriter;

    sTask();
    mfxStatus WriteBitstream();
    mfxStatus Reset();
    mfxStatus Init(mfxU32 nBufferSize, CSmplBitstreamWriter *pWriter = NULL);
    mfxStatus Close();

    std::list<std::pair<bufSet*, mfxFrameSurface1*> > bufs;
};

class CEncTaskPool
{
public:
    CEncTaskPool();
    virtual ~CEncTaskPool();

    virtual mfxStatus Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize, CSmplBitstreamWriter *pOtherWriter = NULL);
    virtual mfxStatus GetFreeTask(sTask **ppTask);
    virtual mfxStatus SynchronizeFirstTask();
    virtual void Close();

protected:
    sTask* m_pTasks;
    mfxU32 m_nPoolSize;
    mfxU32 m_nTaskBufferStart;

    MFXVideoSession* m_pmfxSession;

    virtual mfxU32 GetFreeTaskIndex();
};

/* This class implements a pipeline with 2 mfx components: vpp (video preprocessing) and encode */
class CEncodingPipeline
{
public:
    CEncodingPipeline();
    virtual ~CEncodingPipeline();

    virtual mfxStatus Init(sInputParams *pParams);
    virtual mfxStatus Run();
    virtual void Close();
    virtual mfxStatus ResetMFXComponents(sInputParams* pParams);
    virtual mfxStatus ResetDevice();

    virtual void  PrintInfo();

protected:
    std::pair<CSmplBitstreamWriter *,
              CSmplBitstreamWriter *> m_FileWriters;
    CSmplYUVReader m_FileReader;
    CEncTaskPool m_TaskPool;
    mfxU16 m_nAsyncDepth; // depth of asynchronous pipeline, this number can be tuned to achieve better performance
    mfxU16 m_refDist;
    mfxU16 m_gopSize;
    mfxU32 numOfFields;
    mfxI32 numMB;

    MFXVideoSession m_mfxSession;
    MFXVideoSession m_preenc_mfxSession;
    MFXVideoENCODE* m_pmfxENCODE;
    MFXVideoENC* m_pmfxPREENC;
    MFXVideoENC* m_pmfxENC;
    MFXVideoPAK* m_pmfxPAK;

    mfxVideoParam m_mfxEncParams;
    sInputParams  m_encpakParams;

    MFXFrameAllocator* m_pMFXAllocator;
    mfxAllocatorParams* m_pmfxAllocatorParams;
    MemType m_memType;
    bool m_bExternalAlloc; // use memory allocator as external for Media SDK

    mfxFrameSurface1* m_pEncSurfaces; // frames array for encoder input (vpp output)
    mfxFrameSurface1* m_pReconSurfaces; // frames array for reconstructed surfaces [FEI]
    mfxFrameAllocResponse m_EncResponse;  // memory allocation response for encoder
    mfxFrameAllocResponse m_ReconResponse;  // memory allocation response for encoder for reconstructed surfaces [FEI]

    // for look ahead BRC configuration
    mfxExtCodingOption2 m_CodingOption2;
    mfxExtFeiParam  m_encpakInit;

    // external parameters for each component are stored in a vector
    std::vector<mfxExtBuffer*> m_EncExtParams;

    std::list<iTask*> inputTasks; //used in PreENC

    std::list<bufSet*> preencBufs, encodeBufs;

    void freeBuffers(std::list<bufSet*> bufs);

    CHWDevice *m_hwdev;

    virtual mfxStatus InitMfxEncParams(sInputParams *pParams);

    virtual mfxStatus InitFileWriters(sInputParams *pParams);
    virtual void FreeFileWriters();
    virtual mfxStatus InitFileWriter(CSmplBitstreamWriter **ppWriter, const msdk_char *filename);

    virtual mfxStatus CreateAllocator();
    virtual void DeleteAllocator();

    virtual mfxStatus CreateHWDevice();
    virtual void DeleteHWDevice();

    virtual mfxStatus AllocFrames();
    virtual void DeleteFrames();

    virtual mfxStatus AllocateSufficientBuffer(mfxBitstream* pBS);

    virtual mfxStatus GetFreeTask(sTask **ppTask);
    virtual mfxU16 GetFrameType(mfxU32 pos);

    virtual mfxStatus SynchronizeFirstTask();

    iTask* findFrameToEncode();
    mfxStatus initPreEncFrameParams(iTask* eTask);
    mfxStatus initEncFrameParams(iTask* eTask);
    mfxStatus initEncodeFrameParams(mfxFrameSurface1* encodeSurface, sTask* pCurrentTask);

    mfxEncodeCtrl* ctr;

    bool disableMVoutPreENC;
    bool disableMBStatPreENC;

    mfxU16 m_refFrameCounter;

    mfxExtFeiPreEncMV::mfxMB* tmpForReading;
    mfxI16 *tmpForMedian;
};

void repackPreenc2Enc(mfxExtFeiPreEncMV::mfxMB *preencMVoutMB, mfxExtFeiEncMVPredictors::mfxMB *EncMVPredMB, mfxU32 NumMB, mfxI16 *tmpBuf);
mfxI16 get16Median(mfxExtFeiPreEncMV::mfxMB* preencMB, mfxI16* tmpBuf, int xy, int L0L1);

mfxU32 GetEncodingOrder(mfxU32 displayOrder, mfxU32 begin, mfxU32 end, mfxU32 counter, bool & ref);

#endif // __PIPELINE_ENCODE_H__
