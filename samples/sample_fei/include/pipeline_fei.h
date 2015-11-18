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

#ifndef __PIPELINE_FEI_H__
#define __PIPELINE_FEI_H__

#include "sample_defs.h"
#include "sample_fei_defs.h"
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
#include <ctime>

#define MFX_FRAMETYPE_IPB (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B)
#define MFX_FRAMETYPE_PB  (                  MFX_FRAMETYPE_P | MFX_FRAMETYPE_B)

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

//#define INPUT_ALLOC 1
//#define RECON_ALLOC 2

struct sInputParams
{
    mfxU16 nTargetUsage;
    mfxU32 DecodeId; // type of input coded video
    mfxU32 CodecId;
    mfxU32 ColorFormat;
    mfxU16 nPicStruct;
    mfxU16 nWidth; // source picture width
    mfxU16 nHeight; // source picture height
    mfxF64 dFrameRate;
    mfxU16 nBitRate;
    mfxU32 nNumFrames;
    mfxU32 nTimeout;
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
    mfxI16 DisableDeblockingIdc;
    mfxI16 SliceAlphaC0OffsetDiv2;
    mfxI16 SliceBetaOffsetDiv2;

    mfxU16 nDstWidth;  // destination picture width, specified if resizing required
    mfxU16 nDstHeight; // destination picture height, specified if resizing required

    MemType memType;
    bool bUseHWLib; // true if application wants to use HW MSDK library

    msdk_char strSrcFile[MSDK_MAX_FILENAME_LEN];

    std::vector<msdk_char*> srcFileBuff;
    std::vector<msdk_char*> dstFileBuff;

    bool bDECODE;
    bool bENCODE;
    bool bENCPAK;
    bool bOnlyENC;
    bool bOnlyPAK;
    bool bPREENC;
    bool EncodedOrder;
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
    bool bNPredSpecified;
    bool bFieldProcessingMode;
    msdk_char* mvinFile;
    msdk_char* mbctrinFile;
    msdk_char* mvoutFile;
    msdk_char* mbcodeoutFile;
    msdk_char* mbstatoutFile;
    msdk_char* mbQpFile;
};

struct ExtendedSurface
{
    mfxFrameSurface1 *pSurface;
    mfxSyncPoint      Syncp;
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
    virtual mfxStatus SetFieldToStore(mfxU32 fieldId);
    virtual mfxStatus DropENCODEoutput(mfxBitstream& bs, mfxU32 fieldId);
    virtual void Close();

protected:
    sTask* m_pTasks;
    mfxU32 m_nPoolSize;
    mfxU32 m_nTaskBufferStart;
    mfxU32 m_nFieldId;

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
    CSmplBitstreamReader m_BSReader;
    CEncTaskPool m_TaskPool;
    mfxU16 m_nAsyncDepth; // depth of asynchronous pipeline, this number can be tuned to achieve better performance
    mfxU16 m_refDist;
    mfxU16 m_decodePoolSize;
    mfxU16 m_gopSize;
    mfxU32 m_numOfFields;
    mfxI32 m_numMB;

    MFXVideoSession m_mfxSession;
    MFXVideoSession m_preenc_mfxSession;
    MFXVideoSession m_decode_mfxSession;
    MFXVideoSession* m_pVPP_mfxSession;
    MFXVideoDECODE*  m_pmfxDECODE;
    MFXVideoVPP*     m_pmfxVPP;
    MFXVideoENCODE*  m_pmfxENCODE;
    MFXVideoENC*     m_pmfxPREENC;
    MFXVideoENC*     m_pmfxENC;
    MFXVideoPAK*     m_pmfxPAK;

    mfxVideoParam m_mfxEncParams;
    mfxVideoParam m_mfxDecParams;
    mfxVideoParam m_mfxVppParams;
    sInputParams  m_encpakParams;

    mfxBitstream m_mfxBS;  // contains encoded input data

    MFXFrameAllocator* m_pMFXAllocator;
    mfxAllocatorParams* m_pmfxAllocatorParams;
    MemType m_memType;
    bool m_bExternalAlloc; // use memory allocator as external for Media SDK

    mfxSyncPoint   m_LastDecSyncPoint;

    mfxFrameSurface1* m_pDecSurfaces; // frames array for decoder input
    mfxFrameSurface1* m_pVppSurfaces; // frames array for vpp input
    mfxFrameSurface1* m_pEncSurfaces; // frames array for encoder input (vpp output)
    mfxFrameSurface1* m_pReconSurfaces; // frames array for reconstructed surfaces [FEI]
    mfxFrameAllocResponse m_DecResponse;  // memory allocation response for decoder
    mfxFrameAllocResponse m_VppResponse;  // memory allocation response for VPP input
    mfxFrameAllocResponse m_EncResponse;  // memory allocation response for encoder
    mfxFrameAllocResponse m_ReconResponse;  // memory allocation response for encoder for reconstructed surfaces [FEI]

    // for look ahead BRC configuration
    mfxExtCodingOption2 m_CodingOption2;

    mfxExtFeiParam  m_encpakInit;
    mfxExtFeiSliceHeader m_encodeSliceHeader[2]; // 0 - first, 1 - second fields

    // for disabling VPP algorithms
    mfxExtVPPDoNotUse m_VppDoNotUse;

    // external parameters for each component are stored in a vector
    std::vector<mfxExtBuffer*> m_EncExtParams;
    std::vector<mfxExtBuffer*> m_VppExtParams;

    std::list<iTask*> m_inputTasks; //used in PreENC, ENC, PAK
    iTask* m_last_task;

    std::list<bufSet*> m_preencBufs, m_encodeBufs;

    mfxStatus FreeBuffers(std::list<bufSet*> bufs);

    CHWDevice *m_hwdev;

    virtual mfxStatus InitInterfaces();

    virtual mfxStatus InitMfxEncParams(sInputParams *pParams);
    virtual mfxStatus InitMfxDecodeParams(sInputParams *pParams);
    virtual mfxStatus InitMfxVppParams(sInputParams *pParams);

    virtual mfxStatus InitFileWriters(sInputParams *pParams);
    virtual mfxStatus ResetIOFiles(sInputParams pParams);
    virtual void FreeFileWriters();
    virtual mfxStatus InitFileWriter(CSmplBitstreamWriter **ppWriter, const msdk_char *filename);

    virtual mfxStatus CreateAllocator();
    virtual void DeleteAllocator();

    virtual mfxStatus CreateHWDevice();
    virtual void DeleteHWDevice();

    virtual mfxStatus AllocFrames();
    virtual void DeleteFrames();

    virtual mfxStatus AllocateSufficientBuffer(mfxBitstream* pBS);
    virtual mfxStatus UpdateVideoParams();

    virtual mfxStatus GetFreeTask(sTask **ppTask);
    virtual PairU8 GetFrameType(mfxU32 pos);
    BiFrameLocation GetBiFrameLocation(mfxU32 frameOrder);

    virtual mfxStatus SynchronizeFirstTask();

    virtual mfxStatus PreencOneFrame(iTask* &eTask, mfxFrameSurface1* pSurf, bool is_buffered, bool &cont);
    virtual mfxStatus EncPakOneFrame(iTask* &eTask, mfxFrameSurface1* pSurf, sTask* pCurrentTask, bool is_buffered, bool &cont);
    virtual mfxStatus EncodeOneFrame(iTask* &eTask, mfxFrameSurface1* pSurf, sTask* pCurrentTask, bool is_buffered, bool &cont);
    virtual mfxStatus ProcessMultiPreenc(iTask* eTask, mfxU16 num_of_refs[2]);
    virtual mfxStatus SyncOneEncodeFrame(sTask* pCurrentTask, iTask* eTask, mfxU32 fieldProcessingCounter);

    virtual mfxStatus DecodeOneFrame(ExtendedSurface *pOutSurf);
    virtual mfxStatus DecodeLastFrame(ExtendedSurface *pOutSurf);

    virtual mfxStatus VPPOneFrame(mfxFrameSurface1 *pSurfaceIn, ExtendedSurface *pExtSurface);
    mfxStatus SwitchExtBufPayload(mfxU32 bufID, setElem *first_buf, int idx1, setElem *sec_buf, int idx2);
    mfxStatus ResetExtBufferPayload(mfxU32 bufID, int fieldId, setElem *buf);

    iTask* CreateAndInitTask();
    iTask* FindFrameToEncode(bool buffered_frames);
    iTask* ConfigureTask(iTask* eTask, bool is_buffered);
    mfxStatus UpdateTaskQueue(iTask* eTask);
    mfxStatus CopyState(iTask* eTask);
    mfxStatus RemoveOneTask();
    mfxStatus ClearTasks();
    mfxStatus ProcessLastB();
    mfxU32 CountUnencodedFrames();

    std::list<iTask*>::iterator ReorderFrame(std::list<iTask*>& unencoded_queue);
    mfxU32 CountFutureRefs(mfxU32 frameOrder);

    mfxStatus InitPreEncFrameParamsEx(iTask* eTask, iTask* refTask[2], int ref_fid[2][2]);
    mfxStatus InitEncFrameParams(iTask* eTask);
    mfxStatus InitEncodeFrameParams(mfxFrameSurface1* encodeSurface, sTask* pCurrentTask, int frameType);
    mfxStatus ReadPAKdata(iTask* eTask);
    mfxStatus DropENCPAKoutput(iTask* eTask);
    mfxStatus DropPREENCoutput(iTask* eTask);
    mfxStatus PassPreEncMVPred2EncEx(iTask* eTask, mfxU16 numMVP[2]);

    mfxFrameSurface1 ** GetCurrentL0SurfacesEnc(iTask* eTask, bool fair_reconstruct);
    mfxFrameSurface1 ** GetCurrentL1SurfacesEnc(iTask* eTask, bool fair_reconstruct);
    mfxFrameSurface1 ** GetCurrentL0SurfacesPak(iTask* eTask);
    mfxFrameSurface1 ** GetCurrentL1SurfacesPak(iTask* eTask);

    iTask* GetTaskByFrameOrder(mfxU32 frame_order);
    int GetRefTaskEx(iTask *eTask, unsigned int idx, int refIdx[2][2], int ref_fid[2][2], iTask *outRefTask[2][2]);
    unsigned GetBufSetDistortion(IObuffs* buf);

    //mfxStatus DumpPreEncMVP(setElem *outbuf, int frame_seq, int fieldId, int idx);
    //mfxStatus DumpEncodeMVP(setElem *inbuf, int frame_seq, int fieldId);
    void ShowDpbInfo(iTask *task, int frame_order);
    mfxEncodeCtrl* m_ctr;

    bool m_twoEncoders;
    bool m_disableMVoutPreENC;
    bool m_disableMBStatPreENC;

    mfxU16 m_maxQueueLength;
    mfxU16 m_refFrameCounter;
    mfxU16 m_frameNumMax;
    mfxU8  m_ffid, m_sfid;
    mfxU32 m_frameCount, m_frameOrderIdrInDisplayOrder;
    PairU8 m_frameType;
    mfxU8  m_isField;

    mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB* m_tmpForReading, *m_tmpMBpreenc;
    mfxExtFeiEncMV::mfxExtFeiEncMVMB*       m_tmpMBenc;
    mfxI16 *m_tmpForMedian;

    mfxU16 m_numOfRefs[2];
};

bufSet* getFreeBufSet(std::list<bufSet*> bufs);
mfxExtBuffer * getBufById(setElem* bufSet, mfxU32 id, mfxU32 fieldId);

PairU8 ExtendFrameType(mfxU32 type);
mfxU32 GetEncodingOrder(mfxU32 displayOrder, mfxU32 begin, mfxU32 end, mfxU32 counter, bool & ref);

mfxStatus repackPreenc2Enc(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *preencMVoutMB, mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB *EncMVPredMB, mfxU32 NumMB, mfxI16 *tmpBuf);
mfxStatus repackPreenc2EncEx(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *preencMVoutMB, mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB *EncMVPredMB, mfxU32 NumMB, mfxI16 *tmpBuf, int refIdx);
mfxI16 get16Median(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB* preencMB, mfxI16* tmpBuf, int xy, int L0L1);
const char* getPicType(mfxU8 type);

#endif // __PIPELINE_FEI_H__
