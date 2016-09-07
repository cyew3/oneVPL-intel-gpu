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

#include "encoding_task_pool.h"
#include "sample_defs.h"

#include "base_allocator.h"

#include "vaapi_allocator.h"
#include "vaapi_device.h"

#include "hw_device.h"

#include "sysmem_allocator.h"
#include "mfx_itt_trace.h"

#include "math.h"
#include <ctime>

// Extensions for internal use, normally these macros are blank
#ifdef MOD_FEI
#include "extension_macros.h"
#else
    #define MSDK_DEBUG
#endif

#define NOT_IN_SINGLE_FIELD_MODE 4

#define MaxNumActiveRefP      4
#define MaxNumActiveRefBL0    4
#define MaxNumActiveRefBL1    1
#define MaxNumActiveRefBL1_i  2

struct AppConfig
{

    AppConfig()
        : DecodeId(0)            // Default (invalid) value
        , CodecId(MFX_CODEC_AVC) // Only AVC is supported
        , ColorFormat(MFX_FOURCC_YV12)
        , nPicStruct(MFX_PICSTRUCT_PROGRESSIVE)
        , nWidth(0)
        , nHeight(0)
        , dFrameRate(30.0)
        , nNumFrames(0)          // Unlimited
        , nTimeout(0)            // Unlimited
        , refDist(1)             // Only I frames
        , gopSize(1)             // Only I frames
        , QP(26)
        , numSlices(1)
        , numRef(1)              // One ref by default
        , NumRefActiveP(0)
        , NumRefActiveBL0(0)
        , NumRefActiveBL1(0)
        , bRefType(MFX_B_REF_UNKNOWN) // Let MSDK library to decide wheather to use B-pyramid or not
        , nIdrInterval(0xffff)        // Infinite IDR interval
        , preencDSstrength(0)         // No Downsampling
        , bDynamicRC(false)
        , nResetStart(0)
        , nDRCdefautW(0)
        , nDRCdefautH(0)
        , MaxDrcWidth(0)
        , MaxDrcHeight(0)

        , SearchWindow(5)             // 48x40 (48 SUs)
        , LenSP(57)
        , SearchPath(0)               // exhaustive (full search)
        , RefWidth(32)
        , RefHeight(32)
        , SubMBPartMask(0x00)         // all enabled
        , IntraPartMask(0x00)         // all enabled
        , SubPelMode(0x03)            // quarter-pixel
        , IntraSAD(0x02)              // Haar transform
        , InterSAD(0x02)              // Haar transform
        , GopOptFlag(0)               // None
        , CodecProfile(MFX_PROFILE_AVC_HIGH)
        , CodecLevel(MFX_LEVEL_AVC_41)
        , Trellis(MFX_TRELLIS_UNKNOWN)
        , DisableDeblockingIdc(0)
        , SliceAlphaC0OffsetDiv2(0)
        , SliceBetaOffsetDiv2(0)
        , ChromaQPIndexOffset(0)
        , SecondChromaQPIndexOffset(0)
        , nDstWidth(0)
        , nDstHeight(0)
        , nInputSurf(0)
        , nReconSurf(0)

        , bUseHWmemory(true)          // only HW memory is supported (ENCODE supports SW memory)

        , bDECODE(false)
        , bENCODE(false)
        , bENCPAK(false)
        , bOnlyENC(false)
        , bOnlyPAK(false)
        , bPREENC(false)
        , bDECODESTREAMOUT(false)
        , EncodedOrder(false)
        , DecodedOrder(false)
        , bMBSize(false)
        , Enable8x8Stat(false)
        , AdaptiveSearch(false)
        , FTEnable(false)
        , RepartitionCheckEnable(false)
        , MultiPredL0(false)
        , MultiPredL1(false)
        , DistortionType(false)
        , ColocatedMbDistortion(false)
        , ConstrainedIntraPredFlag(false)
        , Transform8x8ModeFlag(false)
        , bRepackPreencMV(false)
        , bNPredSpecified_l0(false)
        , bNPredSpecified_l1(false)
        , bPreencPredSpecified_l0(false)
        , bPreencPredSpecified_l1(false)
        , bFieldProcessingMode(false)
        , bPerfMode(false)
        , bRawRef(false)
        , bNRefPSpecified(false)
        , bNRefBL0Specified(false)
        , bNRefBL1Specified(false)
        , mvinFile(NULL)
        , mbctrinFile(NULL)
        , mvoutFile(NULL)
        , mbcodeoutFile(NULL)
        , mbstatoutFile(NULL)
        , mbQpFile(NULL)
        , repackctrlFile(NULL)
        , decodestreamoutFile(NULL)
    {
        NumMVPredictors[0] = 1;
        NumMVPredictors[1] = 0;
        PreencMVPredictors[0] = true;
        PreencMVPredictors[1] = true;
    };

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
    bool   bDynamicRC;
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
    mfxU16 IntraPartMask;
    mfxU16 SubPelMode;
    mfxU16 IntraSAD;
    mfxU16 InterSAD;
    mfxU16 NumMVPredictors[2];    // number of [0] - L0 predictors, [1] - L1 predictors
    bool   PreencMVPredictors[2]; // use PREENC predictor [0] - L0, [1] - L1
    mfxU16 GopOptFlag;            // STRICT | CLOSED, default is OPEN GOP
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

    bool   bUseHWmemory;

    msdk_char strSrcFile[MSDK_MAX_FILENAME_LEN];

    std::vector<msdk_char*> srcFileBuff;
    std::vector<const msdk_char*> dstFileBuff;
    std::vector<mfxU16> nDrcWidth; //Dynamic Resolution Change Picture Width,specified if DRC requied
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
    static mfxStatus WriteBitstream(CSmplBitstreamWriter *pWriter, mfxBitstream* mfxBS);
    mfxStatus Reset();
    mfxStatus Init(mfxU32 nBufferSize, CSmplBitstreamWriter *pWriter = NULL);
    mfxStatus Close();

    std::list<std::pair<bufSet*, mfxFrameSurface1*> > bufs;
};

struct MVP_elem
{
    MVP_elem(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB * MVMB = NULL, mfxU8 idx = 0xff, mfxU16 dist = 0xffff)
        : preenc_MVMB(MVMB)
        , refIdx(idx)
        , distortion(dist)
    {}

    mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB * preenc_MVMB;
    mfxU8  refIdx;
    mfxU16 distortion;
};

struct BestMVset
{
    BestMVset(mfxU16 num_pred[2][2])
    {
        bestL0.reserve((std::min)((std::max)(num_pred[0][0], num_pred[1][0]), MaxFeiEncMVPNum));
        bestL1.reserve((std::min)((std::max)(num_pred[0][1], num_pred[1][1]), MaxFeiEncMVPNum));
    }

    void Clear()
    {
        bestL0.clear();
        bestL1.clear();
    }

    std::vector<MVP_elem> bestL0;
    std::vector<MVP_elem> bestL1;
};

class CEncTaskPool
{
public:
    CEncTaskPool();
    virtual ~CEncTaskPool();

    virtual mfxStatus Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize);
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
    mfxU16            LastPicked;
    mfxU16            PoolSize;
    SurfStrategy      Strategy;

    ExtSurfPool()
        : SurfacesPool(NULL)
        , LastPicked(0xffff)
        , PoolSize(0)
        , Strategy(PREFER_FIRST_FREE)
    {}

    ~ExtSurfPool()
    {
        DeleteFrames();
    }

    void DeleteFrames()
    {
        MSDK_SAFE_DELETE_ARRAY(SurfacesPool);
        PoolSize   = 0;
        LastPicked = 0xffff;
    }

    mfxFrameSurface1* GetFreeSurface_FEI()
    {
        mfxU16 idx;

        switch (Strategy)
        {
        case PREFER_NEW:
            idx = GetFreeSurface_FirstNew();
            break;

        case PREFER_FIRST_FREE:
        default:
            idx = GetFreeSurface(SurfacesPool, PoolSize);
            break;
        }

        return (idx != MSDK_INVALID_SURF_IDX) ? SurfacesPool + idx : NULL;
    }

    mfxU16 GetFreeSurface_FirstNew()
    {
        mfxU32 SleepInterval = 10; // milliseconds

        mfxU16 idx = MSDK_INVALID_SURF_IDX;

        //wait if there's no free surface
        for (mfxU32 i = 0; i < MSDK_SURFACE_WAIT_INTERVAL; i += SleepInterval)
        {
            // watch through the buffer for unlocked surface, start with last picked one
            if (SurfacesPool)
            {
                for (mfxU16 j = ((mfxU16)(LastPicked + 1)) % PoolSize, n_watched = 0;
                    n_watched < PoolSize;
                    ++j, j %= PoolSize, ++n_watched)
                {
                    if (0 == SurfacesPool[j].Data.Locked)
                    {
                        LastPicked = j;
                        mdprintf(stderr, "\n\n Picking surface %u\n\n", j);
                        return j;
                    }
                }
            }
            else
            {
                msdk_printf(MSDK_STRING("ERROR: Surface Pool is NULL\n"));
                return MSDK_INVALID_SURF_IDX;
            }

            if (MSDK_INVALID_SURF_IDX != idx)
            {
                break;
            }
            else
            {
                MSDK_SLEEP(SleepInterval);
            }
        }

        if (idx == MSDK_INVALID_SURF_IDX)
        {
            msdk_printf(MSDK_STRING("ERROR: No free surfaces in pool (during long period)\n"));
        }

        return idx;
    }
};

/* This class implements a FEI pipeline */
class CEncodingPipeline
{
public:
    CEncodingPipeline();
    virtual ~CEncodingPipeline();

    virtual mfxStatus Init(AppConfig *pConfig);
    virtual mfxStatus Run();
    virtual void Close();
    virtual mfxStatus ResetMFXComponents(AppConfig* pConfig, bool realloc_frames);
    virtual mfxStatus InitSessions();
    virtual mfxStatus ResetSessions();
    virtual mfxStatus ResetDevice();

    virtual void  PrintInfo();

protected:
    CSmplBitstreamWriter * m_FileWriter;
    CSmplYUVReader         m_FileReader;
    CSmplBitstreamReader   m_BSReader;
    CEncTaskPool m_TaskPool;
    mfxU16 m_nAsyncDepth; // depth of asynchronous pipeline, this number can be tuned to achieve better performance
    mfxU16 m_refDist;
    mfxU16 m_decodePoolSize;
    mfxU16 m_gopSize;
    mfxU16 m_numOfFields;
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
    AppConfig     m_appCfg;

    mfxBitstream m_mfxBS;  // contains encoded input data

    MFXFrameAllocator*  m_pMFXAllocator;
    mfxAllocatorParams* m_pmfxAllocatorParams;
    bool m_bUseHWmemory;   // indicates whether hardware or software memory used
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

    iTaskPool m_inputTasks; //used in PreENC, ENC, PAK, ENCODE (in EncodedOrder)

    iTaskParams m_taskInitializationParams; // holds all necessery data for task initializing

    RefInfo m_ref_info;

    std::list<bufSet*> m_preencBufs, m_encodeBufs;
    mfxStatus FreeBuffers(std::list<bufSet*> bufs);

    bool m_bEndOfFile;
    bool m_insertIDR;
    bool m_disableMVoutPreENC;
    bool m_disableMBStatPreENC;
    bool m_bSeparatePreENCSession;
    bool m_enableMVpredPreENC;

    mfxU16 m_maxQueueLength;
    mfxU16 m_log2frameNumMax;
    mfxU8  m_ffid, m_sfid; // holds parity of first / second field: 0 - top_field, 1 - bottom_field
    mfxU32 m_frameCount, m_frameOrderIdrInDisplayOrder;
    PairU8 m_frameType;
    bool   m_isField;

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

    virtual mfxStatus InitMfxEncParams(AppConfig *pConfig);
    virtual mfxStatus InitMfxDecodeParams(AppConfig *pConfig);
    virtual mfxStatus InitMfxVppParams(AppConfig *pConfig);

    virtual mfxStatus ResetIOFiles(const AppConfig & Config);
    virtual void FreeFileWriter();
    virtual mfxStatus InitFileWriter(CSmplBitstreamWriter * & pWriter, const msdk_char *filename);

    virtual mfxStatus CreateAllocator();
    virtual void DeleteAllocator();

    virtual mfxStatus CreateHWDevice();
    virtual void DeleteHWDevice();

    virtual mfxStatus AllocFrames();
    virtual mfxStatus FillSurfacePool(mfxFrameSurface1* & surfacesPool, mfxFrameAllocResponse* allocResponse, mfxFrameInfo* FrameInfo);
    virtual void DeleteFrames();

    virtual mfxStatus ReleaseResources();

    virtual mfxStatus AllocateSufficientBuffer(mfxBitstream* pBS);
    virtual mfxStatus UpdateVideoParams();

    virtual mfxStatus GetFreeTask(sTask **ppTask);
    virtual PairU8 GetFrameType(mfxU32 pos);

    virtual mfxStatus SynchronizeFirstTask();

    virtual mfxStatus GetOneFrame(mfxFrameSurface1* & pSurf);
    virtual mfxStatus ResizeFrame(mfxU32 frameNum, size_t &rctime);
    virtual mfxStatus ResetExtBufMBnum(bufSet* bufs, mfxU16 new_numMB, bool both_fields);

    virtual mfxStatus PreProcessOneFrame(mfxFrameSurface1* & pSurf);
    virtual mfxStatus PreencOneFrame(iTask* eTask);
    virtual mfxStatus ProcessMultiPreenc(iTask* eTask);
    virtual mfxStatus EncPakOneFrame(iTask* eTask);
    virtual mfxStatus EncodeOneFrame(iTask* eTask, mfxFrameSurface1* pSurf);
    virtual mfxStatus SyncOneEncodeFrame(sTask* pCurrentTask, mfxU32 fieldProcessingCounter);

    virtual mfxStatus DecodeOneFrame(ExtendedSurface *pOutSurf);
    virtual mfxStatus DecodeLastFrame(ExtendedSurface *pOutSurf);

    virtual mfxStatus VPPOneFrame(MFXVideoVPP* VPPobj, MFXVideoSession* session, mfxFrameSurface1 *pSurfaceIn, ExtendedSurface *pExtSurface, bool sync);

    virtual mfxStatus doGPUHangRecovery();

    mfxStatus ClearDecoderBuffers();
    mfxStatus ResetBuffers();
    mfxU16 GetCurPicType(mfxU32 fieldId);

    mfxStatus InitPreEncFrameParamsEx(iTask* eTask, iTask* refTask[2][2], mfxU8 ref_fid[2][2], bool isDownsamplingNeeded);
    mfxStatus InitEncPakFrameParams(iTask* eTask);
    mfxStatus InitEncodeFrameParams(mfxFrameSurface1* encodeSurface, sTask* pCurrentTask, PairU8 frameType, iTask* eTask);
    mfxStatus ReadPAKdata(iTask* eTask);
    mfxStatus DropENCPAKoutput(iTask* eTask);
    mfxStatus DropPREENCoutput(iTask* eTask);
    mfxStatus DropDecodeStreamoutOutput(mfxFrameSurface1* pSurf);
    mfxStatus PassPreEncMVPred2EncEx(iTask* eTask);
    mfxStatus PassPreEncMVPred2EncExPerf(iTask* eTask);
    void UpsampleMVP(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB * preenc_MVMB, mfxU32 MBindex_DS, mfxExtFeiEncMVPredictors* mvp, mfxU32 predIdx, mfxU8 refIdx, mfxU32 L0L1);

    /* ENC(PAK) reflists */
    mfxStatus FillRefInfo(iTask* eTask);

    mfxStatus GetRefTaskEx(iTask *eTask, mfxU8 l0_idx, mfxU8 l1_idx, mfxU8 refIdx[2][2], mfxU8 ref_fid[2][2], iTask *outRefTask[2][2]);

    mfxStatus GetBestSetsByDistortion(std::list<PreEncOutput>& preenc_output, BestMVset & BestSet, mfxU32 nPred[2], mfxU32 fieldId, mfxU32 MB_idx);
};

bufSet* getFreeBufSet(std::list<bufSet*> bufs);

PairU8 ExtendFrameType(mfxU32 type);

mfxStatus repackPreenc2Enc(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *preencMVoutMB, mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB *EncMVPredMB, mfxU32 NumMB, mfxI16 *tmpBuf);
mfxI16 get16Median(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB* preencMB, mfxI16* tmpBuf, int xy, int L0L1);
mfxI16 get4Median(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB* preencMB, mfxI16* tmpBuf, int xy, int L0L1, int offset);

inline bool compareDistortion(MVP_elem frst, MVP_elem scnd);

#endif // __PIPELINE_FEI_H__
