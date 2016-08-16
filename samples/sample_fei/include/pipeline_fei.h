/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

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

// Extensions for internal use, normally these macros are blank
#ifdef MOD_FEI
#include "extension_macros.h"
#else
    #define MSDK_DEBUG
#endif

#define MFX_FRAMETYPE_IPB (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B)
#define MFX_FRAMETYPE_IP  (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P                  )
#define MFX_FRAMETYPE_PB  (                  MFX_FRAMETYPE_P | MFX_FRAMETYPE_B)

#define NOT_IN_SINGLE_FIELD_MODE 4

#define MaxNumActiveRefP      4
#define MaxNumActiveRefBL0    4
#define MaxNumActiveRefBL1    1
#define MaxNumActiveRefBL1_i  2


#if _DEBUG
    #define mdprintf fprintf
#else
    #define mdprintf(...)
#endif

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

struct sInputParams
{
    mfxU32 DecodeId; // type of input coded video
    mfxU32 CodecId;
    mfxU32 ColorFormat;
    mfxU16 nPicStruct;
    mfxU16 nWidth;  // source picture width
    mfxU16 nHeight; // source picture height
    mfxF64 dFrameRate;
    mfxU32 nNumFrames;
    mfxU32 nTimeout;
    mfxU16 refDist; //number of frames to next I,P
    mfxU16 gopSize; //number of frames to next I
    mfxU8  QP;
    mfxU16 numSlices;
    mfxU16 numRef;           // number of reference frames (DPB size)
    mfxU16 NumRefActiveP;    // maximal number of references for P frames
    mfxU16 NumRefActiveBL0;  // maximal number of backward references for B frames
    mfxU16 NumRefActiveBL1;  // maximal number of forward references for B frames
    mfxU16 bRefType;         // B-pyramid ON/OFF/UNKNOWN (default, let MSDK lib to decide)
    mfxU16 nIdrInterval;     // distance between IDR frames in GOPs
    mfxU8  preencDSstrength; // downsample input before passing to preenc (2/4/8x are supported)
    mfxU32 nResetStart;
    mfxU16 nDRCdefautW;
    mfxU16 nDRCdefautH;
    mfxU16 MaxDrcWidth;
    mfxU16 MaxDrcHeight;

    mfxU16 SearchWindow; // search window size and search path from predifined presets
    mfxU16 LenSP;        // search path length
    mfxU16 SearchPath;   // search path type
    mfxU16 RefWidth;     // search window width
    mfxU16 RefHeight;    // search window height
    mfxU16 SubMBPartMask;
    mfxU16 SubPelMode;
    mfxU16 IntraPartMask;
    mfxU16 IntraSAD;
    mfxU16 InterSAD;
    mfxU16 NumMVPredictors[2];  // number of [0] - L0 predictors, [1] - L1 predictors
    bool PreencMVPredictors[2]; // use PREENC predictor [0] - L0, [1] - L1
    mfxU16 GopOptFlag;          // STRICT | CLOSED, default is OPEN GOP
    mfxU16 CodecProfile;
    mfxU16 CodecLevel;
    mfxU16 Trellis;             // switch on trellis 2 - I | 4 - P | 8 - B, 1 - off, 0 - default
    mfxU16 DisableDeblockingIdc;
    mfxI16 SliceAlphaC0OffsetDiv2;
    mfxI16 SliceBetaOffsetDiv2;
    mfxI16 ChromaQPIndexOffset;
    mfxI16 SecondChromaQPIndexOffset;

    mfxU16 nDstWidth;  // destination picture width, specified if resizing required
    mfxU16 nDstHeight; // destination picture height, specified if resizing required

    mfxU16 nInputSurf;
    mfxU16 nReconSurf;

    MemType memType;

    msdk_char strSrcFile[MSDK_MAX_FILENAME_LEN];

    std::vector<msdk_char*> srcFileBuff;
    std::vector<const msdk_char*> dstFileBuff;
    std::vector<mfxU16> nDrcWidth;//Dynamic Resolution Change Picture Width,specified if DRC requied
    std::vector<mfxU16> nDrcHeight;//Dynamic Resolution Change Picture Height,specified if DRC requied
    std::vector<mfxU32> nDrcStart; //Start Frame No. of Dynamic Resolution Change,specified if DRC requied

    bool bDECODE;
    bool bENCODE;
    bool bENCPAK;
    bool bOnlyENC;
    bool bOnlyPAK;
    bool bPREENC;
    bool bDECODESTREAMOUT;
    bool EncodedOrder;
    bool DecodedOrder;
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
    bool ConstrainedIntraPredFlag;
    bool Transform8x8ModeFlag;
    bool bRepackPreencMV;
    bool bNPredSpecified_l0;
    bool bNPredSpecified_l1;
    bool bPreencPredSpecified_l0;
    bool bPreencPredSpecified_l1;
    bool bFieldProcessingMode;
    bool bPerfMode;
    bool bDynamicRC;
    bool bRawRef;
    bool bNRefPSpecified;
    bool bNRefBL0Specified;
    bool bNRefBL1Specified;
    msdk_char* mvinFile;
    msdk_char* mbctrinFile;
    msdk_char* mvoutFile;
    msdk_char* mbcodeoutFile;
    msdk_char* mbstatoutFile;
    msdk_char* mbQpFile;
    msdk_char* repackctrlFile;
    msdk_char* decodestreamoutFile;
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

struct MVP_elem
{
    MVP_elem(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB * MVMB, mfxU8 idx, mfxU16 dist)
    {
        preenc_MVMB = MVMB;
        refIdx      = idx;
        distortion  = dist;
    };

    MVP_elem()
    {
        preenc_MVMB = NULL;
        refIdx      = 0xff;
        distortion  = 0xffff;
    };

    mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB * preenc_MVMB;
    mfxU8 refIdx;
    mfxU16 distortion;
};

struct BestMVset
{
    BestMVset(mfxU16 num_pred[2][2])
    {
        bestL0.reserve((std::min)((std::max)(num_pred[0][0], num_pred[1][0]), MaxFeiEncMVPNum));
        bestL1.reserve((std::min)((std::max)(num_pred[0][1], num_pred[1][1]), MaxFeiEncMVPNum));
    };

    void Clear()
    {
        bestL0.clear();
        bestL1.clear();

    };

    std::vector<MVP_elem> bestL0;
    std::vector<MVP_elem> bestL1;
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

enum SurfStrategy
{
    PREFER_FIRST_FREE = 1,
    PREFER_NEW
};

struct ExtSurfPool
{
    mfxFrameSurface1* SurfacesPool;
    mfxU16 LastPicked;
    mfxU16 PoolSize;
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
    virtual mfxStatus ResetMFXComponents(sInputParams* pParams, bool realloc_frames);
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
    mfxU16 m_heightMB;
    mfxU16 m_widthMB;
    mfxU16 m_widthMBpreenc;
    mfxU16 m_numMB;
    mfxU16 m_numMBpreenc; // number of MBs in input for PreEnc surfaces (preenc may use downsampling, so m_numMBpreenc could be <= m_numMB)

    mfxU16 m_numMB_frame;      // is needed for mixed picstructs (holds numMB for progressive frame)
    mfxU16 m_numMBpreenc_frame;// is needed for mixed picstructs (holds numMB for progressive frame)

    MFXVideoSession  m_mfxSession;
    MFXVideoSession  m_preenc_mfxSession;
    MFXVideoSession* m_pPreencSession;

    MFXVideoDECODE*  m_pmfxDECODE;
    MFXVideoVPP*     m_pmfxVPP;
    MFXVideoVPP*     m_pmfxDS;
    MFXVideoENCODE*  m_pmfxENCODE;
    MFXVideoENC*     m_pmfxPREENC;
    MFXVideoENC*     m_pmfxENC;
    MFXVideoPAK*     m_pmfxPAK;
    mfxEncodeCtrl*   m_ctr;

    mfxVideoParam m_mfxEncParams;
    mfxVideoParam m_mfxDecParams;
    mfxVideoParam m_mfxVppParams;
    mfxVideoParam m_mfxDSParams;
    sInputParams  m_encpakParams;

    mfxBitstream m_mfxBS;  // contains encoded input data

    MFXFrameAllocator*  m_pMFXAllocator;
    mfxAllocatorParams* m_pmfxAllocatorParams;
    MemType m_memType;
    bool m_bExternalAlloc; // use memory allocator as external for Media SDK

    CHWDevice *m_hwdev;

    mfxSyncPoint   m_LastDecSyncPoint;

    ExtSurfPool m_pDecSurfaces;   // frames array for decoder input
    ExtSurfPool m_pDSSurfaces;    // frames array for downscaled surfaces for PREENC
    ExtSurfPool m_pVppSurfaces;   // frames array for vpp input
    ExtSurfPool m_pEncSurfaces;   // frames array for encoder input (vpp output)
    ExtSurfPool m_pReconSurfaces; // frames array for reconstructed surfaces [FEI]
    mfxFrameAllocResponse m_DecResponse;  // memory allocation response for decoder
    mfxFrameAllocResponse m_VppResponse;  // memory allocation response for VPP input
    mfxFrameAllocResponse m_dsResponse;   // memory allocation response for VPP input
    mfxFrameAllocResponse m_EncResponse;  // memory allocation response for encoder
    mfxFrameAllocResponse m_ReconResponse;  // memory allocation response for encoder for reconstructed surfaces [FEI]
    mfxU32 m_BaseAllocID;
    mfxU32 m_EncPakReconAllocID;

    std::vector<mfxExtFeiDecStreamOut*> m_StreamoutBufs;
    mfxExtFeiDecStreamOut* m_pExtBufDecodeStreamout; // current streamout buffer

    // for trellis, B-pyramid, RAW-reference settings    mfxExtCodingOption2 m_CodingOption2;
    mfxExtCodingOption2 m_CodingOption2;

    // for P/B reference number settings
    mfxExtCodingOption3 m_CodingOption3;

    mfxExtFeiParam  m_encpakInit;
    mfxExtFeiSliceHeader m_encodeSliceHeader[2]; // 0 - first, 1 - second fields

    // for disabling VPP algorithms
    mfxExtVPPDoNotUse m_VppDoNotUse;

    // external parameters for each component are stored in a vector
    std::vector<mfxExtBuffer*> m_EncExtParams;
    std::vector<mfxExtBuffer*> m_VppExtParams;

    std::list<iTask*> m_inputTasks; //used in PreENC, ENC, PAK, ENCODE (in EncodedOrder)
    iTask* m_last_task;

    RefInfo m_ref_info;

    std::list<bufSet*> m_preencBufs, m_encodeBufs;
    mfxStatus FreeBuffers(std::list<bufSet*> bufs);

    bool m_bEndOfFile;
    bool m_insertIDR;
    bool m_twoEncoders;
    bool m_disableMVoutPreENC;
    bool m_disableMBStatPreENC;
    bool m_bSeparatePreENCSession;
    bool m_enableMVpredPreENC;

    mfxU16 m_maxQueueLength;
    mfxU16 m_log2frameNumMax;
    mfxU8  m_ffid, m_sfid; // holds parity of first / second field: 0 - top_field, 1 - bottom_field
    mfxU32 m_frameCount, m_frameOrderIdrInDisplayOrder;
    mfxU16 m_frameIdrCounter;
    PairU8 m_frameType;
    bool  m_isField;

    // Dynamic Resolution Change workflow
    mfxU16 m_numMB_drc;
    bool m_bDRCReset;
    bool m_bNeedDRC;   //True if Dynamic Resolution Change requied
    bool m_bVPPneeded; //True if we have VPP in pipeline
    std::vector<mfxU32> m_drcStart;
    std::vector<mfxU16> m_drcWidth;
    std::vector<mfxU16> m_drcHeight;
    mfxU16 m_drcDftW;
    mfxU16 m_drcDftH;

    mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB* m_tmpForReading, *m_tmpMBpreenc;
    mfxExtFeiEncMV::mfxExtFeiEncMVMB*       m_tmpMBenc;
    mfxI16 *m_tmpForMedian;

    mfxU16 m_numOfRefs[2][2]; // [fieldId][L0L1]

    SurfStrategy m_surfPoolStrategy;

    virtual mfxStatus InitInterfaces();
    virtual mfxStatus FillPipelineParameters();

    virtual mfxStatus InitMfxEncParams(sInputParams *pParams);
    virtual mfxStatus InitMfxDecodeParams(sInputParams *pParams);
    virtual mfxStatus InitMfxVppParams(sInputParams *pParams);

    virtual mfxStatus InitFileWriters(sInputParams *pParams);
    virtual mfxStatus ResetIOFiles(const sInputParams & pParams);
    virtual void FreeFileWriters();
    virtual mfxStatus InitFileWriter(CSmplBitstreamWriter **ppWriter, const msdk_char *filename);

    virtual mfxStatus CreateAllocator();
    virtual void DeleteAllocator();

    virtual mfxStatus CreateHWDevice();
    virtual void DeleteHWDevice();

    virtual mfxStatus AllocFrames();
    virtual mfxStatus FillSurfacePool(mfxFrameSurface1* & surfacesPool, mfxFrameAllocResponse* allocResponse, mfxFrameInfo* FrameInfo);
    virtual void DeleteFrames();

    virtual mfxStatus ReleaseResources();
    virtual mfxStatus UnlockResources();

    virtual mfxStatus AllocateSufficientBuffer(mfxBitstream* pBS);
    virtual mfxStatus UpdateVideoParams();

    virtual mfxStatus GetFreeTask(sTask **ppTask);
    virtual PairU8 GetFrameType(mfxU32 pos);
    BiFrameLocation GetBiFrameLocation(mfxU32 frameOrder);

    virtual mfxStatus SynchronizeFirstTask();

    virtual mfxStatus GetOneFrame(mfxFrameSurface1* & pSurf);
    virtual mfxStatus ResizeFrame(mfxU32 frameNum, size_t &rctime, iTask* &eTask, sTask *pCurrentTask);
    virtual mfxStatus ResetExtBufMBnum(bufSet* bufs, mfxU16 new_numMB);

    virtual mfxStatus PreProcessOneFrame(mfxFrameSurface1* & pSurf, bool &cont);
    virtual mfxStatus PreencOneFrame(iTask* &eTask, mfxFrameSurface1* pSurf, bool is_buffered, bool &cont);
    virtual mfxStatus ProcessMultiPreenc(iTask* eTask);
    virtual mfxStatus EncPakOneFrame(iTask* &eTask, mfxFrameSurface1* pSurf, sTask* pCurrentTask, bool is_buffered, bool &cont);
    virtual mfxStatus EncodeOneFrame(iTask* &eTask, mfxFrameSurface1* pSurf, sTask* pCurrentTask, bool is_buffered, bool &cont);
    virtual mfxStatus SyncOneEncodeFrame(sTask* pCurrentTask, iTask* eTask, mfxU32 fieldProcessingCounter);

    virtual mfxStatus DecodeOneFrame(ExtendedSurface *pOutSurf);
    virtual mfxStatus DecodeLastFrame(ExtendedSurface *pOutSurf);

    virtual mfxStatus VPPOneFrame(MFXVideoVPP* VPPobj, MFXVideoSession* session, mfxFrameSurface1 *pSurfaceIn, ExtendedSurface *pExtSurface);

    virtual mfxStatus doGPUHangRecovery();

    virtual mfxU16 GetFreeSurfaceFEI(ExtSurfPool & SurfacesPool);
    mfxU16 GetFreeSurface_FirstNew(ExtSurfPool & SurfacesPool);

    iTask* CreateAndInitTask();
    iTask* FindFrameToEncode(bool buffered_frames);
    iTask* ConfigureTask(iTask* eTask, bool is_buffered);
    mfxStatus UpdateTaskQueue(iTask* eTask);
    mfxStatus CopyState(iTask* eTask);
    mfxStatus RemoveOneTask();
    mfxStatus ReleasePreencMVPinfo(iTask* eTask);
    mfxStatus ClearTasks();
    mfxStatus ClearDecoderBuffers();
    mfxStatus ResetBuffers();
    mfxStatus ProcessLastB();
    mfxU32 CountUnencodedFrames();
    mfxU16 GetCurPicType(mfxU32 fieldId);

    std::list<iTask*>::iterator ReorderFrame(std::list<iTask*>& unencoded_queue);
    mfxU32 CountFutureRefs(mfxU32 frameOrder);

    mfxStatus InitPreEncFrameParamsEx(iTask* eTask, iTask* refTask[2][2], mfxU8 ref_fid[2][2], bool isDownsamplingNeeded);
    mfxStatus InitEncPakFrameParams(iTask* eTask);
    mfxStatus InitEncodeFrameParams(mfxFrameSurface1* encodeSurface, sTask* pCurrentTask, PairU8 frameType, bool is_buffered);
    mfxStatus ReadPAKdata(iTask* eTask);
    mfxStatus DropENCPAKoutput(iTask* eTask);
    mfxStatus DropPREENCoutput(iTask* eTask);
    mfxStatus DropDecodeStreamoutOutput(mfxFrameSurface1* pSurf);
    mfxStatus PassPreEncMVPred2EncEx(iTask* eTask);
    mfxStatus PassPreEncMVPred2EncExPerf(iTask* eTask);
    void UpsampleMVP(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB * preenc_MVMB, mfxU32 MBindex_DS, mfxExtFeiEncMVPredictors* mvp, mfxU32 predIdx, mfxU8 refIdx, mfxU32 L0L1);

    /* ENC(PAK) reflists */
    mfxStatus FillRefInfo(iTask* eTask);
    mfxStatus ResetRefInfo();
    mfxU32 GetNBackward(iTask* eTask, mfxU32 fieldId);
    mfxU32 GetNForward(iTask* eTask, mfxU32 fieldId);

    iTask* GetTaskByFrameOrder(mfxU32 frame_order);
    mfxStatus GetRefTaskEx(iTask *eTask, mfxU8 l0_idx, mfxU8 l1_idx, mfxU8 refIdx[2][2], mfxU8 ref_fid[2][2], iTask *outRefTask[2][2]);

    mfxStatus GetBestSetsByDistortion(std::list<PreEncOutput>& preenc_output, BestMVset & BestSet, mfxU32 nPred[2], mfxU32 fieldId, mfxU32 MB_idx);

    void ShowDpbInfo(iTask *task, int frame_order);
};

bufSet* getFreeBufSet(std::list<bufSet*> bufs);
mfxExtBuffer * getBufById(setElem* bufSet, mfxU32 id, mfxU32 fieldId);

PairU8 ExtendFrameType(mfxU32 type);
mfxU32 GetEncodingOrder(mfxU32 displayOrder, mfxU32 begin, mfxU32 end, mfxU32 counter, bool & ref);

mfxStatus repackPreenc2Enc(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *preencMVoutMB, mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB *EncMVPredMB, mfxU32 NumMB, mfxI16 *tmpBuf);
mfxI16 get16Median(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB* preencMB, mfxI16* tmpBuf, int xy, int L0L1);
mfxI16 get4Median(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB* preencMB, mfxI16* tmpBuf, int xy, int L0L1, int offset);

inline bool compareDistortion(MVP_elem frst, MVP_elem scnd);
const char* getFrameType(mfxU8 type);

#endif // __PIPELINE_FEI_H__
