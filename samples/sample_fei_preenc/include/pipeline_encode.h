//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2005-2014 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __PIPELINE_ENCODE_H__
#define __PIPELINE_ENCODE_H__

#include "sample_defs.h"
#include "hw_device.h"

#ifdef D3D_SURFACES_SUPPORT
#pragma warning(disable : 4201)
#endif

#include "sample_utils.h"
#include "base_allocator.h"

#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "mfxfei.h"

#include <vector>
#include <memory>

enum MemType {
    SYSTEM_MEMORY = 0x00,
    D3D9_MEMORY   = 0x01,
    D3D11_MEMORY  = 0x02,
};

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

    MemType memType;

    msdk_char strSrcFile[MSDK_MAX_FILENAME_LEN];

    std::vector<msdk_char*> srcFileBuff;
    msdk_char* dstMBFileBuff;
    msdk_char* dstMVFileBuff;
    msdk_char* inMVPredFile;
    msdk_char* inQPFile;
    msdk_char* ctrlFile;

    mfxU8 nRotationAngle; // if specified, enables rotation plugin in mfx pipeline
    msdk_char strPluginDLLPath[MSDK_MAX_FILENAME_LEN]; // plugin dll path and name

    mfxI32 refDist; //number of frames to next I,P
    mfxI32 gopSize; //number of frames to next I
    bool useSw;  //use HW acceleration, hw - by default
    bool preENC;  //use preENC only call otherwise ENC+PAK call
    bool bLABRC; // use look ahead bitrate control algorithm
    mfxU16 nLADepth; // depth of the look ahead bitrate control  algorithm
};

//input task structure
struct iTask{
    mfxENCInput in;
    mfxENCOutput out;
    mfxU16 frameType;
    mfxU32 frameDisplayOrder;
    mfxSyncPoint EncSyncP;
    mfxI32 encoded;
};

//async task structure
struct sTask
{
    //mfxBitstream mfxBS;
    mfxSyncPoint EncSyncP;

    CSmplBitstreamWriter *pWriter;

    sTask();
    mfxStatus WriteBitstream();
    mfxStatus WriteMbData(mfxENCOutput& out);
    mfxStatus WriteMVData(mfxENCOutput& out);
    mfxStatus Reset();
    mfxStatus Init(mfxU32 nBufferSize, CSmplBitstreamWriter *pWriter = NULL);
    mfxStatus Close();
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
    sInputParams m_params;

    std::pair<CSmplBitstreamWriter *,
              CSmplBitstreamWriter *> m_FileWriters;
    CSmplYUVReader m_FileReader;
    CEncTaskPool m_TaskPool;
    mfxU16 m_nAsyncDepth; // depth of asynchronous pipeline, this number can be tuned to achieve better performance
    mfxI32 m_refDist;
    mfxI32 m_gopSize;

    MFXVideoSession m_mfxSession;
    MFXVideoENC* m_pmfxENC;

    mfxVideoParam m_mfxEncParams;

    MFXFrameAllocator* m_pMFXAllocator;
    mfxAllocatorParams* m_pmfxAllocatorParams;
    MemType m_memType;
    bool m_bExternalAlloc; // use memory allocator as external for Media SDK

    mfxFrameSurface1* m_pEncSurfaces; // frames array for encoder input
    mfxFrameAllocResponse m_EncResponse;  // memory allocation response for encoder

    mfxExtCodingOption m_CodingOption;
    // for look ahead BRC configuration
    mfxExtCodingOption2 m_CodingOption2;

    std::vector<mfxExtBuffer*> m_EncExtParams;

    CHWDevice *m_hwdev;

    std::vector<sTask> m_tasks;

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

    void AllocMVPredictors();

    virtual mfxStatus AllocateSufficientBuffer(mfxBitstream* pBS);

    virtual mfxStatus GetFreeTask(sTask **ppTask);
    virtual mfxU16 GetFrameType(mfxU32 pos);

    virtual mfxStatus SynchronizeFirstTask();
};

#endif // __PIPELINE_ENCODE_H__
