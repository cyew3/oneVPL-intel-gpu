//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015 Intel Corporation. All Rights Reserved.
//

#include <math.h>
#include <stdio.h>
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "vector"
#include "../include/test_common.h"
#include "../include/genx_hevce_sao_hsw_isa.h"
#include "../include/genx_hevce_sao_bdw_isa.h"

#ifdef CMRT_EMU
extern "C" void SaoStatAndEstimate(SurfaceIndex SURF_SRC, SurfaceIndex SURF_RECON, SurfaceIndex SURF_VIDEO_PARAM, Ipp32u recPaddingLu) {};
extern "C" void SaoStat(SurfaceIndex SRC, SurfaceIndex RECON, SurfaceIndex PARAM, SurfaceIndex DIFF_EO, SurfaceIndex COUNT_EO, SurfaceIndex DIFF_BO, SurfaceIndex COUNT_BO, Ipp32u recPaddingLu);
extern "C" void SaoApply(SurfaceIndex SRC, SurfaceIndex DST, SurfaceIndex PARAM, SurfaceIndex SAO_MODES);
extern "C" void SaoStatChroma(SurfaceIndex SRC, SurfaceIndex RECON, SurfaceIndex PARAM, SurfaceIndex DIFF_EO, SurfaceIndex COUNT_EO, SurfaceIndex DIFF_BO, SurfaceIndex COUNT_BO, Ipp32u recPaddingLu);
extern "C" void SaoEstimate(SurfaceIndex PARAM, SurfaceIndex STATS, SurfaceIndex SAO_MODES);
extern "C" void SaoEstimateAndApply(SurfaceIndex SRC, SurfaceIndex DST, SurfaceIndex PARAM, SurfaceIndex STATS) {}; 
#endif //CMRT_EMU

enum { GPU_STAT_BLOCK_WIDTH = 16, GPU_STAT_BLOCK_HEIGHT = 16 };

// CHROMA_SEPARATIST means: (1) use separate kernel (2) use alternative threadspace (vs luma)
#define CHROMA_SEPARATIST

#if defined CHROMA_SEPARATIST
enum { GPU_STAT_CHROMA_BLOCK_WIDTH = 8, GPU_STAT_CHROMA_BLOCK_HEIGHT = 16 };
#else
enum { GPU_STAT_CHROMA_BLOCK_WIDTH = GPU_STAT_BLOCK_WIDTH/2, GPU_STAT_CHROMA_BLOCK_HEIGHT = GPU_STAT_BLOCK_HEIGHT/2 };
#endif
#define SAO_MODE_MERGE_ENABLED


struct VideoParam // size = (352+128)B = (88+32) DW
{
    Ipp8u  tabBeta[52];            // +0 B
    Ipp16u Width;                  // +26 W
    Ipp16u Height;                 // +27 W
    Ipp16u PicWidthInCtbs;         // +28 W
    Ipp16u PicHeightInCtbs;        // +29 W
    Ipp16s tcOffset;               // +30 W
    Ipp16s betaOffset;             // +31 W
    Ipp8u  chromaQp[58];           // +64 B
    Ipp8u  crossSliceBoundaryFlag; // +122 B
    Ipp8u  crossTileBoundaryFlag;  // +123 B
    Ipp8u  TULog2MinSize;          // +124 B
    Ipp8u  MaxCUDepth;             // +125 B
    Ipp8u  Log2MaxCUSize;          // +126 B
    Ipp8u  Log2NumPartInCU;        // +127 B
    Ipp8u  tabTc[54];              // +128 B
    Ipp8u  MaxCUSize;              // +182 B
    Ipp8u  chromaFormatIdc;        // +183 B
    Ipp8u  alignment0[8];          // +184 B
    Ipp32s list0[16];              // +48 DW
    Ipp32s list1[16];              // +64 DW
    Ipp8u  scan2z[16];             // +320 B
    // sao extension
    Ipp32f m_rdLambda;             // +84 DW
    Ipp32s SAOChromaFlag;          // +85 DW
    Ipp32s enableBandOffset;       // +86 DW
    //Ipp8u reserved[4];             // +87 DW
    Ipp32s offsetChroma;           // +87 DW
    // tile/slice restriction (avail LeftAbove)
    Ipp8u availLeftAbove[196];     // +88 DW
};

struct AddrNode
{
    Ipp32s ctbAddr;
    Ipp32s sameTile;
    Ipp32s sameSlice;
};

struct SaoCtuParam;

struct AddrInfo
{
    AddrNode left;
    AddrNode right;
    AddrNode above;
    AddrNode below;
    AddrNode belowRight;
    Ipp32u region_border_right, region_border_bottom;
    Ipp32u region_border_left, region_border_top;
};

enum SaoComponentIdx
{
  SAO_Y = 0,
  SAO_Cb,
  SAO_Cr,
  NUM_SAO_COMPONENTS
};

#define NUM_SAO_BO_CLASSES_LOG2  5
#define NUM_SAO_BO_CLASSES  (1<<NUM_SAO_BO_CLASSES_LOG2)

#define NUM_SAO_EO_TYPES_LOG2 2

enum
{
    SAO_OPT_ALL_MODES       = 1,
    SAO_OPT_FAST_MODES_ONLY = 2
};


enum SaoEOClasses
{
  SAO_CLASS_EO_FULL_VALLEY = 0,
  SAO_CLASS_EO_HALF_VALLEY = 1,
  SAO_CLASS_EO_PLAIN       = 2,
  SAO_CLASS_EO_HALF_PEAK   = 3,
  SAO_CLASS_EO_FULL_PEAK   = 4,
  NUM_SAO_EO_CLASSES,
};


enum SaoModes
{
  SAO_MODE_OFF = 0,
  SAO_MODE_ON,
  SAO_MODE_MERGE_LEFT,
  SAO_MODE_MERGE_ABOVE,
  NUM_SAO_MODES
};


enum SaoBaseTypes
{
  SAO_TYPE_EO_0 = 0,
  SAO_TYPE_EO_90,
  SAO_TYPE_EO_135,
  SAO_TYPE_EO_45,

  SAO_TYPE_BO,

  NUM_SAO_BASE_TYPES
};


#define MAX_NUM_SAO_CLASSES  32
struct SaoOffsetParam
{
    int mode_idx;     //ON, OFF, MERGE
    int type_idx;     //EO_0, EO_90, EO_135, EO_45, BO. MERGE: left, above
    int startBand_idx; //BO: starting band index
    int saoMaxOffsetQVal;
    int offset[MAX_NUM_SAO_CLASSES];
};


struct SaoCtuParam
{
    SaoOffsetParam& operator[](int compIdx){ return m_offsetParam[compIdx];}
    const SaoOffsetParam& operator[](int compIdx) const { return m_offsetParam[compIdx];}

private:
    //SaoCtuParam& operator= (const SaoCtuParam& src);
    SaoOffsetParam m_offsetParam[NUM_SAO_COMPONENTS];
};

#if 0
void SetVideoParam(VideoParam & videoParam, Ipp32s width, Ipp32s height);
int RunGpuStatAndEstimate(const Ipp8u *frameOrigin, const Ipp8u *frameRecon, const VideoParam *m_par, SaoCtuParam *frame_sao_param, bool useUP);
//int RunGpuStat(const Ipp8u *frameOrigin, const Ipp8u *frameRecon, const VideoParam *m_par, Ipp16s *blockStats, bool useUP);
//int RunGpuEstimate(const Ipp16s *blockStats, const VideoParam *videoParam, SaoCtuParam *frame_sao_param);
int RunGpuApply(const Ipp8u *frameRecon, Ipp8u *frameDst, const VideoParam *m_par, SaoCtuParam *frame_sao_param);
int RunGpuEstimateAndApply(const Ipp8u *frameRecon, Ipp8u *frameDst, const Ipp16s *blockStats, SaoCtuParam *frame_sao_param, const VideoParam *videoParam);
//int RunCpuStatAndEstimate(const Ipp8u *frameOrigin, Ipp8u *frameRecon, const VideoParam *m_par, const AddrInfo *frame_addr_info, SaoCtuParam *frame_sao_param, Ipp32s *diffEO, Ipp16u *countEO, Ipp32s *diffBO, Ipp16u *countBO);
int RunCpuApply(const Ipp8u *frameRecon, Ipp8u *frameDst, const VideoParam *m_par, const SaoCtuParam *frame_sao_param);
int CompareStats(Ipp32s *diffEO, Ipp16u *countEO, Ipp32s *diffBO, Ipp16u *countBO, Ipp32s *diffEO_ref, Ipp16u *countEO_ref, Ipp32s *diffBO_ref, Ipp16u *countBO_ref, int   numCtbs, int enableChroma = 0); // E0, 90, 135, 45 & BO

void AccumulateStats(const VideoParam *m_par, const Ipp16s *blockStats, Ipp32s *diffEO, Ipp16u *countEO, Ipp32s *diffBO, Ipp16u *countBO, Ipp8u enableChroma = 0);
int Dump(Ipp8u *data, size_t frameSize, const char *fileName);
void SimulateRecon(Ipp8u *input, Ipp32s inPitch, Ipp8u *recon, Ipp32s reconPitch, Ipp32s width, Ipp32s height, Ipp32s maxAbsPixDiff);
#endif
void EstimateSao(const Ipp8u* frameOrigin, int pitchOrigin, Ipp8u* frameRecon, int pitchRecon, const VideoParam* m_par, const AddrInfo* frame_addr_info, SaoCtuParam* frame_sao_param,
                 Ipp32s* diffEO, Ipp16u* countEO, Ipp32s* diffBO, Ipp16u* countBO,
                 Ipp8u* availLeft, Ipp8u* availAbove);

struct SaoOffsetOut_gfx
{
    Ipp8u mode_idx;
    Ipp8u type_idx;
    Ipp8u startBand_idx;
    Ipp8u saoMaxOffsetQVal;
    Ipp16s offset[4];
    Ipp8u reserved[4];
}; // sizeof() == 16 bytes

int RunGpuStatAndEstimate(const Ipp8u* frameOrigin, const Ipp8u* frameRecon, const VideoParam* m_par, SaoCtuParam* frame_sao_param, bool useUP)
{
    int pitchOrigin = m_par->Width;
    int pitchRecon = m_par->Width;
    mfxU32 version = 0;
    CmDevice *device = 0;
    Ipp32s res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_hevce_sao_hsw, sizeof(genx_hevce_sao_hsw), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(SaoStatAndEstimate), kernel);
    CHECK_CM_ERR(res);

    //-------------------------------------------------------
    // debug printf info
    //-------------------------------------------------------
    //res = device->InitPrintBuffer();
    //CHECK_CM_ERR(res);

    // RESOURCES //

    // arg[0] original
    CmSurface2D *input = 0;
    res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, input);
    CHECK_CM_ERR(res);
    res = input->WriteSurfaceStride(frameOrigin, NULL, pitchOrigin);
    CHECK_CM_ERR(res);

    const Ipp32u padding = (useUP) ? 96 : 0;

    // arg[1] recon
    CmSurface2DUP *reconUp = NULL;
    CmSurface2D *recon = NULL;
    Ipp8u *reconSys = NULL;
    if (useUP) {
        Ipp32u reconPitch = 0;
        Ipp32u reconSize = 0;
        res = device->GetSurface2DInfo(m_par->Width+2*padding, m_par->Height, CM_SURFACE_FORMAT_P8, reconPitch, reconSize);
        CHECK_CM_ERR(res);
        Ipp8u *reconSys = (Ipp8u *)CM_ALIGNED_MALLOC(reconSize, 0x1000);
        res = device->CreateSurface2DUP(m_par->Width+2*padding, m_par->Height, CM_SURFACE_FORMAT_P8, reconSys, reconUp);
        CHECK_CM_ERR(res);
        for (Ipp32s y = 0; y < m_par->Height; y++) {
            memcpy(reconSys + y * reconPitch + padding, frameRecon + y * pitchRecon, m_par->Width);
            memset(reconSys + y * reconPitch + padding + m_par->Width, frameRecon[m_par->Width - 1 + y * pitchRecon], padding);
            memset(reconSys + y * reconPitch, frameRecon[y * pitchRecon], padding);
        }
    } else {
        res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, recon);
        CHECK_CM_ERR(res);
        res = recon->WriteSurfaceStride(frameRecon, NULL, pitchRecon);
        CHECK_CM_ERR(res);
    }

    // arg[2] output
    CmSurface2D *output = 0;
    res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, output);
    CHECK_CM_ERR(res);

    // arg[3]
    int size = sizeof(VideoParam);
    CmBuffer* video_param = 0;
    res = device->CreateBuffer(size, video_param);
    CHECK_CM_ERR(res);
    res = video_param->WriteSurface((const Ipp8u*)m_par, NULL);
    CHECK_CM_ERR(res);

    int numCtb = m_par->PicHeightInCtbs * m_par->PicWidthInCtbs;

    // arg[4]
    size = numCtb * (4 * 4) * sizeof(Ipp32s);
    CmBuffer* diffEO_gfx = 0;
    res = device->CreateBuffer(size, diffEO_gfx);
    CHECK_CM_ERR(res);

    // arg [5]
    CmBuffer* countEO_gfx = 0;
    res = device->CreateBuffer(size, countEO_gfx);
    CHECK_CM_ERR(res);

    // arg [6]
    size = numCtb * 32 * sizeof(Ipp32s);
    CmBuffer* diffBO_gfx = 0;
    res = device->CreateBuffer(size, diffBO_gfx);
    CHECK_CM_ERR(res);

    // arg [7]
    CmBuffer* countBO_gfx = 0;
    res = device->CreateBuffer(size, countBO_gfx);
    CHECK_CM_ERR(res);

    // arg [8]
    CmBuffer* frame_sao_offset_gfx = 0;
    size = numCtb * sizeof(SaoOffsetOut_gfx);
    res = device->CreateBuffer(size, frame_sao_offset_gfx);
    CHECK_CM_ERR(res);

    // IDX //

    SurfaceIndex *idxInput = 0;
    res = input->GetIndex(idxInput);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxRecon = 0;
    res = (useUP) ? reconUp->GetIndex(idxRecon) : recon->GetIndex(idxRecon);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxOutput = 0;
    res = output->GetIndex(idxOutput);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxParam = 0;
    res = video_param->GetIndex(idxParam);
    CHECK_CM_ERR(res);

    SurfaceIndex *idx_diffEO = 0;
    res = diffEO_gfx->GetIndex(idx_diffEO);
    CHECK_CM_ERR(res);

    SurfaceIndex *idx_countEO = 0;
    res = countEO_gfx->GetIndex(idx_countEO);
    CHECK_CM_ERR(res);

    SurfaceIndex *idx_diffBO = 0;
    res = diffBO_gfx->GetIndex(idx_diffBO);
    CHECK_CM_ERR(res);

    SurfaceIndex *idx_countBO = 0;
    res = countBO_gfx->GetIndex(idx_countBO);
    CHECK_CM_ERR(res);
    SurfaceIndex *idx_frame_sao_offset = 0;
    res = frame_sao_offset_gfx->GetIndex(idx_frame_sao_offset);
    CHECK_CM_ERR(res);

    // SET ARGS //

    res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(*idxRecon), idxRecon);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(2, sizeof(*idxParam), idxParam);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(3, sizeof(*idx_frame_sao_offset), idx_frame_sao_offset);
    CHECK_CM_ERR(res);

    // set start mb
    Ipp32u start_mbX = 0, start_mbY = 0;
    res = kernel->SetKernelArg(4, sizeof(start_mbX), &start_mbX);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(5, sizeof(start_mbY), &start_mbY);
    CHECK_CM_ERR(res);

    mfxU32 MaxCUSize = m_par->MaxCUSize;

    // for YUV420 we process (MaxCuSize x MaxCuSize) block
    mfxU32 tsWidthFull   = m_par->PicWidthInCtbs;
    mfxU32 tsWidth = tsWidthFull;
    mfxU32 tsHeight  = m_par->PicHeightInCtbs;
    if (tsWidthFull > CM_MAX_THREADSPACE_WIDTH) {
        tsWidth = tsWidth / 2;
    }
    res = kernel->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);

    res = threadSpace->SelectThreadDependencyPattern(CM_WAVEFRONT);
    CHECK_CM_ERR(res);

    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);

    res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

    CmQueue *queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);

    CmEvent * e = 0;
    res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);

    if (tsWidthFull > CM_MAX_THREADSPACE_WIDTH) {
        start_mbX = tsWidth;

        tsWidth = tsWidthFull - tsWidth;
        res = kernel->SetThreadCount(tsWidth * tsHeight);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(1, sizeof(*idxRecon), idxRecon);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(2, sizeof(*idxParam), idxParam);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(3, sizeof(*idx_frame_sao_offset), idx_frame_sao_offset);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(4, sizeof(start_mbX), &start_mbX);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(5, sizeof(start_mbY), &start_mbY);
        CHECK_CM_ERR(res);

        res = device->DestroyThreadSpace(threadSpace);
        CHECK_CM_ERR(res);

        // the rest of frame TS
        res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
        CHECK_CM_ERR(res);

        res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
        CHECK_CM_ERR(res);

        res = task->Reset();
        CHECK_CM_ERR(res);

        res = task->AddKernel(kernel);
        CHECK_CM_ERR(res);

        res = device->CreateQueue(queue);
        CHECK_CM_ERR(res);

        res = queue->Enqueue(task, e, threadSpace);
        CHECK_CM_ERR(res);
    }

    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);


    // conversion from gfx to cpu format here
    {
        int size = sizeof(SaoOffsetOut_gfx);
        Ipp8u* p_out_gfx = new Ipp8u[numCtb * size];
        SaoOffsetOut_gfx* sao_gfx = (SaoOffsetOut_gfx*)p_out_gfx;

        frame_sao_offset_gfx->ReadSurface( (Ipp8u*)p_out_gfx, NULL);

        for (int ctb = 0; ctb < numCtb; ctb++) {
            frame_sao_param[ctb][0].mode_idx = sao_gfx[ctb].mode_idx;
            frame_sao_param[ctb][0].type_idx = sao_gfx[ctb].type_idx;
            frame_sao_param[ctb][0].startBand_idx = sao_gfx[ctb].startBand_idx;
            frame_sao_param[ctb][0].saoMaxOffsetQVal = sao_gfx[ctb].saoMaxOffsetQVal;
            // offsets
            frame_sao_param[ctb][0].offset[0] = sao_gfx[ctb].offset[0];
            frame_sao_param[ctb][0].offset[1] = sao_gfx[ctb].offset[1];
            frame_sao_param[ctb][0].offset[2] = sao_gfx[ctb].offset[2];
            frame_sao_param[ctb][0].offset[3] = sao_gfx[ctb].offset[3];
        }

        delete [] p_out_gfx;
    }
    //-------------------------------------------------
    // OUTPUT DEBUG
    //-------------------------------------------------
    //res = device->FlushPrintBuffer();

    //mfxU64 mintime = mfxU64(-1);

#ifndef CMRT_EMU
    printf("TIME=%.3f ms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

    device->DestroyTask(task);

    //printf("\nGPU TIME=%.3f ms\n", mintime / 1000000.0);

    //output->ReadSurfaceStride(outData, NULL, outPitch);
    //input->ReadSurfaceStride(outData, NULL, outPitch);

    queue->DestroyEvent(e);


    device->DestroySurface(input);
    if (useUP) {
        device->DestroySurface2DUP(reconUp);
        CM_ALIGNED_FREE(reconSys);
    } else {
        device->DestroySurface(recon);
    }
    device->DestroySurface(output);
    device->DestroySurface(video_param);
    device->DestroySurface(diffEO_gfx);
    device->DestroySurface(countEO_gfx);
    device->DestroySurface(diffBO_gfx);
    device->DestroySurface(countBO_gfx);
    device->DestroySurface(frame_sao_offset_gfx);

    device->DestroyThreadSpace(threadSpace);

    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    ::DestroyCmDevice(device);

    return PASSED;
}

int RunGpuStat(const Ipp8u* frameOrigin, const Ipp8u* frameRecon, const VideoParam* m_par, Ipp16s *blockStats, Ipp32s blockStatsSize, bool useUP)
{
    int pitchOrigin = m_par->Width;
    int pitchRecon = m_par->Width;
    // DEVICE //
    mfxU32 version = 0;
    CmDevice *device = 0;
    Ipp32s res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_hevce_sao_hsw, sizeof(genx_hevce_sao_hsw), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(SaoStat), kernel);
    CHECK_CM_ERR(res);

    //-------------------------------------------------------
    // debug printf info
    //-------------------------------------------------------
    //res = device->InitPrintBuffer();
    //CHECK_CM_ERR(res);

    // arg[0] original
    CmSurface2D *input = 0;
    res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, input);
    CHECK_CM_ERR(res);
    res = input->WriteSurfaceStride(frameOrigin, NULL, pitchOrigin);
    CHECK_CM_ERR(res);

    const Ipp32u padding = (useUP) ? 96 : 0;

    // arg[1] recon
    CmSurface2DUP *reconUp = NULL;
    CmSurface2D *recon = NULL;
    Ipp8u *reconSys = NULL;
    if (useUP) {
        Ipp32u reconPitch = 0;
        Ipp32u reconSize = 0;
        res = device->GetSurface2DInfo(m_par->Width+2*padding, m_par->Height, CM_SURFACE_FORMAT_P8, reconPitch, reconSize);
        CHECK_CM_ERR(res);
        Ipp8u *reconSys = (Ipp8u *)CM_ALIGNED_MALLOC(reconSize, 0x1000);
        res = device->CreateSurface2DUP(m_par->Width+2*padding, m_par->Height, CM_SURFACE_FORMAT_P8, reconSys, reconUp);
        CHECK_CM_ERR(res);
        for (Ipp32s y = 0; y < m_par->Height; y++) {
            memcpy(reconSys + y * reconPitch + padding, frameRecon + y * pitchRecon, m_par->Width);
            memset(reconSys + y * reconPitch + padding + m_par->Width, frameRecon[m_par->Width - 1 + y * pitchRecon], padding);
            memset(reconSys + y * reconPitch, frameRecon[y * pitchRecon], padding);
        }
    } else {
        res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, recon);
        CHECK_CM_ERR(res);
        res = recon->WriteSurfaceStride(frameRecon, NULL, pitchRecon);
        CHECK_CM_ERR(res);
    }

    // arg[2] output
    CmSurface2D *output = 0;
    res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, output);
    CHECK_CM_ERR(res);

    // arg[3]
    int size = sizeof(VideoParam);
    CmBuffer* video_param = 0;
    res = device->CreateBuffer(size, video_param);
    CHECK_CM_ERR(res);
    res = video_param->WriteSurface((const Ipp8u*)m_par, NULL);
    CHECK_CM_ERR(res);
    
    CmBuffer *stats = 0;
    int compCount = m_par->SAOChromaFlag ? 3 : 1;
    res = device->CreateBuffer(blockStatsSize, stats);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInput = 0;
    res = input->GetIndex(idxInput);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxRecon = 0;
    res = (useUP) ? reconUp->GetIndex(idxRecon) : recon->GetIndex(idxRecon);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxOutput = 0;
    res = output->GetIndex(idxOutput);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxParam = 0;
    res = video_param->GetIndex(idxParam);
    CHECK_CM_ERR(res);

    SurfaceIndex *idx_stats = 0;
    res = stats->GetIndex(idx_stats);
    CHECK_CM_ERR(res);

    res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(*idxRecon), idxRecon);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(2, sizeof(*idxParam), idxParam);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(3, sizeof(*idx_stats), idx_stats);
    CHECK_CM_ERR(res);

    // set start mb
    Ipp32u start_mbX = 0, start_mbY = 0;
    res = kernel->SetKernelArg(4, sizeof(start_mbX), &start_mbX);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(5, sizeof(start_mbY), &start_mbY);
    CHECK_CM_ERR(res);

    mfxU32 MaxCUSize = m_par->MaxCUSize;

    // for YUV420 we process (MaxCuSize x MaxCuSize) block
    int numCtb = m_par->PicHeightInCtbs * m_par->PicWidthInCtbs;
    Ipp32s blockW = GPU_STAT_BLOCK_WIDTH;
    Ipp32s blockH = GPU_STAT_BLOCK_HEIGHT;
    mfxU32 tsWidthFull   = m_par->PicWidthInCtbs * (m_par->MaxCUSize / blockW);
    mfxU32 tsWidth = tsWidthFull;
    mfxU32 tsHeight  = m_par->PicHeightInCtbs * (m_par->MaxCUSize / blockH);
    if (tsWidthFull > CM_MAX_THREADSPACE_WIDTH) {
        tsWidth = tsWidth / 2;
    }
    res = kernel->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);

    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
    CHECK_CM_ERR(res);

    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);

    res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

    CmQueue *queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);

    CmEvent * e = 0;
    res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);

    if (tsWidthFull > CM_MAX_THREADSPACE_WIDTH) {
        start_mbX = tsWidth;

        tsWidth = tsWidthFull - tsWidth;
        res = kernel->SetThreadCount(tsWidth * tsHeight);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(1, sizeof(*idxRecon), idxRecon);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(2, sizeof(*idxParam), idxParam);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(3, sizeof(*idx_stats), idx_stats);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(4, sizeof(start_mbX), &start_mbX);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(5, sizeof(start_mbY), &start_mbY);
        CHECK_CM_ERR(res);

        res = device->DestroyThreadSpace(threadSpace);
        CHECK_CM_ERR(res);

        // the rest of frame TS
        res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
        CHECK_CM_ERR(res);

        res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
        CHECK_CM_ERR(res);

        res = task->Reset();
        CHECK_CM_ERR(res);

        res = task->AddKernel(kernel);
        CHECK_CM_ERR(res);

        res = device->CreateQueue(queue);
        CHECK_CM_ERR(res);

        res = queue->Enqueue(task, e, threadSpace);
        CHECK_CM_ERR(res);
    }

    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);

    // READ OUT DATA

    stats->ReadSurface((Ipp8u *)blockStats, NULL);

    //-------------------------------------------------
    // OUTPUT DEBUG
    //-------------------------------------------------
    //res = device->FlushPrintBuffer();

    //mfxU64 mintime = mfxU64(-1);

#ifndef CMRT_EMU
printf("TIME=%.3f ms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

    device->DestroyTask(task);
    queue->DestroyEvent(e);
    device->DestroySurface(input);
    if (useUP) {
        device->DestroySurface2DUP(reconUp);
        CM_ALIGNED_FREE(reconSys);
    } else {
        device->DestroySurface(recon);
    }
    device->DestroySurface(output);
    device->DestroySurface(video_param);
    device->DestroySurface(stats);
    device->DestroyThreadSpace(threadSpace);
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    ::DestroyCmDevice(device);

    return PASSED;
}
int RunGpuStatChroma(const Ipp8u* frameOrigin, const Ipp8u* frameRecon, const VideoParam* m_par, Ipp16s *blockStats, Ipp32s blockStatsSize, bool useUP, Ipp32s copyChromaOnly)
{
    int pitchOrigin = m_par->Width;
    int pitchRecon = m_par->Width;
    // DEVICE //
    mfxU32 version = 0;
    CmDevice *device = 0;
    Ipp32s res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);
    
    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_hevce_sao_hsw, sizeof(genx_hevce_sao_hsw), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(SaoStatChroma), kernel);
    CHECK_CM_ERR(res);

    //-------------------------------------------------------
    // debug printf info
    //-------------------------------------------------------
    //res = device->InitPrintBuffer();
    //CHECK_CM_ERR(res);

    // arg[0] original
    CmSurface2D *input = 0;
    res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, input);
    CHECK_CM_ERR(res);
    res = input->WriteSurfaceStride(frameOrigin, NULL, pitchOrigin);
    CHECK_CM_ERR(res);

    const Ipp32u padding = (useUP) ? 96 : 0;

    // arg[1] recon
    CmSurface2DUP *reconUp = NULL;
    CmSurface2D *recon = NULL;
    Ipp8u *reconSys = NULL;
    if (useUP) {
        Ipp32u reconPitch = 0;
        Ipp32u reconSize = 0;
        res = device->GetSurface2DInfo(m_par->Width+2*padding, m_par->Height, CM_SURFACE_FORMAT_P8, reconPitch, reconSize);
        CHECK_CM_ERR(res);
        /*Ipp8u **/reconSys = (Ipp8u *)CM_ALIGNED_MALLOC(reconSize, 0x1000);
        res = device->CreateSurface2DUP(m_par->Width+2*padding, m_par->Height, CM_SURFACE_FORMAT_P8, reconSys, reconUp);
        CHECK_CM_ERR(res);
        for (Ipp32s y = 0; y < m_par->Height; y++) {
            memcpy(reconSys + y * reconPitch + padding, frameRecon + y * pitchRecon, m_par->Width);
            memset(reconSys + y * reconPitch + padding + m_par->Width, frameRecon[m_par->Width - 1 + y * pitchRecon], padding);
            memset(reconSys + y * reconPitch, frameRecon[y * pitchRecon], padding);
        }
    } else {
        res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, recon);
        CHECK_CM_ERR(res);
        res = recon->WriteSurfaceStride(frameRecon, NULL, pitchRecon);
        CHECK_CM_ERR(res);
    }

    // arg[2] output
    CmSurface2D *output = 0;
    res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, output);
    CHECK_CM_ERR(res);

    // arg[3]
    int size = sizeof(VideoParam);
    CmBuffer* video_param = 0;
    res = device->CreateBuffer(size, video_param);
    CHECK_CM_ERR(res);
    res = video_param->WriteSurface((const Ipp8u*)m_par, NULL);
    CHECK_CM_ERR(res);

    int numCtb = m_par->PicHeightInCtbs * m_par->PicWidthInCtbs;

    CmBuffer *stats = 0;
    res = device->CreateBuffer(blockStatsSize, stats);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInput = 0;
    res = input->GetIndex(idxInput);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxRecon = 0;
    res = (useUP) ? reconUp->GetIndex(idxRecon) : recon->GetIndex(idxRecon);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxOutput = 0;
    res = output->GetIndex(idxOutput);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxParam = 0;
    res = video_param->GetIndex(idxParam);
    CHECK_CM_ERR(res);

    SurfaceIndex *idx_stats = 0;
    res = stats->GetIndex(idx_stats);
    CHECK_CM_ERR(res);

    res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(*idxRecon), idxRecon);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(2, sizeof(*idxParam), idxParam);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(3, sizeof(*idx_stats), idx_stats);
    CHECK_CM_ERR(res);

    // set start mb
    Ipp32u start_mbX = 0, start_mbY = 0;
    res = kernel->SetKernelArg(4, sizeof(start_mbX), &start_mbX);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(5, sizeof(start_mbY), &start_mbY);
    CHECK_CM_ERR(res);

    mfxU32 MaxCUSize = m_par->MaxCUSize;

    // for YUV420 we process (MaxCuSize x MaxCuSize) block
    Ipp32s blockW = GPU_STAT_CHROMA_BLOCK_WIDTH;
    Ipp32s blockH = GPU_STAT_CHROMA_BLOCK_HEIGHT;
    mfxU32 tsWidthFull   = m_par->PicWidthInCtbs *  ((m_par->MaxCUSize >> 1) / blockW);
    mfxU32 tsWidth = tsWidthFull;
    mfxU32 tsHeight  = m_par->PicHeightInCtbs * ((m_par->MaxCUSize >> 1) / blockH);
    if (tsWidthFull > CM_MAX_THREADSPACE_WIDTH) {
        tsWidth = tsWidth / 2;
    }
    res = kernel->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);

    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
    CHECK_CM_ERR(res);

    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);

    res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

    CmQueue *queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);

    CmEvent * e = 0;
    res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);

    if (tsWidthFull > CM_MAX_THREADSPACE_WIDTH) {
        start_mbX = tsWidth;

        tsWidth = tsWidthFull - tsWidth;
        res = kernel->SetThreadCount(tsWidth * tsHeight);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(1, sizeof(*idxRecon), idxRecon);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(2, sizeof(*idxParam), idxParam);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(3, sizeof(*idx_stats), idx_stats);
        CHECK_CM_ERR(res);

        res = kernel->SetKernelArg(4, sizeof(start_mbX), &start_mbX);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(5, sizeof(start_mbY), &start_mbY);
        CHECK_CM_ERR(res);

        res = device->DestroyThreadSpace(threadSpace);
        CHECK_CM_ERR(res);

        // the rest of frame TS
        res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
        CHECK_CM_ERR(res);

        res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
        CHECK_CM_ERR(res);

        res = task->Reset();
        CHECK_CM_ERR(res);

        res = task->AddKernel(kernel);
        CHECK_CM_ERR(res);

        res = device->CreateQueue(queue);
        CHECK_CM_ERR(res);

        res = queue->Enqueue(task, e, threadSpace);
        CHECK_CM_ERR(res);
    }

    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);

    // READ OUT DATA
    if (!copyChromaOnly ) {
        stats->ReadSurface((Ipp8u *)blockStats, NULL);
    } else {
        std::vector<Ipp8u> tmp(blockStatsSize);
        stats->ReadSurface((Ipp8u *)tmp.data(), NULL);
        memcpy((Ipp8u *)blockStats + m_par->offsetChroma, tmp.data() + m_par->offsetChroma, blockStatsSize - m_par->offsetChroma);
    }

    //-------------------------------------------------
    // OUTPUT DEBUG
    //-------------------------------------------------
    //res = device->FlushPrintBuffer();

    //mfxU64 mintime = mfxU64(-1);

#ifndef CMRT_EMU
printf("SaoStatChroma TIME=%.3f ms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

    device->DestroyTask(task);
    queue->DestroyEvent(e);
    device->DestroySurface(input);
    if (useUP) {
        device->DestroySurface2DUP(reconUp);
        CM_ALIGNED_FREE(reconSys);
    } else {
        device->DestroySurface(recon);
    }
    device->DestroySurface(output);
    device->DestroySurface(video_param);
    device->DestroySurface(stats);
    device->DestroyThreadSpace(threadSpace);
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    ::DestroyCmDevice(device);

    return PASSED;
}

int RunGpuEstimate(const Ipp16s *blockStats, int blockStatsSize, const VideoParam *m_par, SaoCtuParam *frame_sao_param)
{
    int pitchOrigin = m_par->Width;
    int pitchRecon = m_par->Width;
    int numCtb = m_par->PicHeightInCtbs * m_par->PicWidthInCtbs;

    // DEVICE //
    mfxU32 version = 0;
    CmDevice *device = 0;
    Ipp32s res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_hevce_sao_hsw, sizeof(genx_hevce_sao_hsw), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(SaoEstimate), kernel);
    CHECK_CM_ERR(res);

    //-------------------------------------------------------
    // debug printf info
    //-------------------------------------------------------
    res = device->InitPrintBuffer();
    CHECK_CM_ERR(res);

    CmBuffer *video_param = 0;
    res = device->CreateBuffer(sizeof(VideoParam), video_param);
    CHECK_CM_ERR(res);
    res = video_param->WriteSurface((const Ipp8u*)m_par, NULL);
    CHECK_CM_ERR(res);
    
    int compCount = m_par->SAOChromaFlag ? 3 : 1;
    
    CmBuffer *stats = 0;
    res = device->CreateBuffer(blockStatsSize, stats);
    CHECK_CM_ERR(res);
    res = stats->WriteSurface((const Ipp8u *)blockStats, NULL);
    CHECK_CM_ERR(res);

    // arg [8]
    CmBuffer* frame_sao_offset_gfx = 0;
    
    res = device->CreateBuffer(compCount * numCtb * sizeof(SaoOffsetOut_gfx), frame_sao_offset_gfx);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxParam = 0;
    res = video_param->GetIndex(idxParam);
    CHECK_CM_ERR(res);

    SurfaceIndex *idx_stats = 0;
    res = stats->GetIndex(idx_stats);
    CHECK_CM_ERR(res);

    SurfaceIndex *idx_frame_sao_offset = 0;
    res = frame_sao_offset_gfx->GetIndex(idx_frame_sao_offset);
    CHECK_CM_ERR(res);

    res = kernel->SetKernelArg(0, sizeof(SurfaceIndex), idxParam);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(SurfaceIndex), idx_stats);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(2, sizeof(SurfaceIndex), idx_frame_sao_offset);
    CHECK_CM_ERR(res);

    mfxU32 tsWidth   = m_par->PicWidthInCtbs;
    mfxU32 tsHeight  = m_par->PicHeightInCtbs;
    res = kernel->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);

    res = threadSpace->SelectThreadDependencyPattern(CM_WAVEFRONT);
    CHECK_CM_ERR(res);

    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);

    res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

    CmQueue *queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);

    CmEvent * e = 0;
    res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);

    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);

    // READ OUT DATA

    // conversion from gfx to cpu format here
    std::vector<SaoOffsetOut_gfx> sao_out(compCount * numCtb);
    frame_sao_offset_gfx->ReadSurface((Ipp8u *)sao_out.data(), NULL);

    for (int ctb = 0; ctb < numCtb; ctb++) {
        for (int compIdx = 0; compIdx < compCount; compIdx++) {
            SaoOffsetOut_gfx* sao_gfx = sao_out.data() + numCtb * compIdx;
            frame_sao_param[ctb][compIdx].mode_idx = sao_gfx[ctb].mode_idx;
            frame_sao_param[ctb][compIdx].type_idx = sao_gfx[ctb].type_idx;
            frame_sao_param[ctb][compIdx].startBand_idx = sao_gfx[ctb].startBand_idx;
            frame_sao_param[ctb][compIdx].saoMaxOffsetQVal = sao_gfx[ctb].saoMaxOffsetQVal;
            // offsets
            frame_sao_param[ctb][compIdx].offset[0] = sao_gfx[ctb].offset[0];
            frame_sao_param[ctb][compIdx].offset[1] = sao_gfx[ctb].offset[1];
            frame_sao_param[ctb][compIdx].offset[2] = sao_gfx[ctb].offset[2];
            frame_sao_param[ctb][compIdx].offset[3] = sao_gfx[ctb].offset[3];
        }
    }

    //-------------------------------------------------
    // OUTPUT DEBUG
    //-------------------------------------------------
    res = device->FlushPrintBuffer();

    //mfxU64 mintime = mfxU64(-1);

#ifndef CMRT_EMU
    printf("TIME=%.3f ms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

    device->DestroyTask(task);
    queue->DestroyEvent(e);
    device->DestroySurface(video_param);
    device->DestroySurface(stats);
    device->DestroySurface(frame_sao_offset_gfx);
    device->DestroyThreadSpace(threadSpace);
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    ::DestroyCmDevice(device);

    return PASSED;
}

int RunGpuApply(const Ipp8u *frameRecon, Ipp8u *frameDst, const VideoParam* m_par, SaoCtuParam* offsets)
{
    int pitchRecon = m_par->Width;
    int pitchDst = m_par->Width;

    // DEVICE //
    const Ipp32u paddingLu = 96;
    const Ipp32u paddingCh = 96;

    mfxU32 version = 0;
    CmDevice *device = 0;
    Ipp32s res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_hevce_sao_hsw, sizeof(genx_hevce_sao_hsw), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(SaoApply), kernel);
    CHECK_CM_ERR(res);

    //-------------------------------------------------------
    // debug printf info
    //-------------------------------------------------------
    //res = device->InitPrintBuffer();
    //CHECK_CM_ERR(res);

    // src = deblocked reconstruct
    CmSurface2D *recon = 0;
    res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, recon);
    CHECK_CM_ERR(res);
    res = recon->WriteSurfaceStride(frameRecon, NULL, pitchRecon);
    CHECK_CM_ERR(res);

    // output luma
    CmSurface2D *output = 0;
    res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, output);
    CHECK_CM_ERR(res);

    // output luma UP
    Ipp8u *outputSysLu = 0;
    Ipp32u outputPitchLu = 0;
    CmSurface2DUP *outputLu = 0;
    Ipp32u outputSizeLu = 0;
    res = device->GetSurface2DInfo(m_par->Width+2*paddingLu, m_par->Height, CM_SURFACE_FORMAT_P8, outputPitchLu, outputSizeLu);
    CHECK_CM_ERR(res);
    outputSysLu = (Ipp8u *)CM_ALIGNED_MALLOC(outputSizeLu, 0x1000);
    res = device->CreateSurface2DUP(m_par->Width+2*paddingLu, m_par->Height, CM_SURFACE_FORMAT_P8, outputSysLu, outputLu);
    CHECK_CM_ERR(res);

    // output chroma UP
    Ipp8u *outputSysCh = 0;
    Ipp32u outputPitchCh = 0;
    CmSurface2DUP *outputCh = 0;
    Ipp32u outputSizeCh = 0;
    res = device->GetSurface2DInfo(m_par->Width+2*paddingCh, m_par->Height/2, CM_SURFACE_FORMAT_P8, outputPitchCh, outputSizeCh);
    CHECK_CM_ERR(res);
    outputSysCh = (Ipp8u *)CM_ALIGNED_MALLOC(outputSizeCh, 0x1000);
    res = device->CreateSurface2DUP(m_par->Width+2*paddingCh, m_par->Height/2, CM_SURFACE_FORMAT_P8, outputSysCh, outputCh);
    CHECK_CM_ERR(res);

    // param
    int size = sizeof(VideoParam);
    CmBuffer* video_param = 0;
    res = device->CreateBuffer(size, video_param);
    CHECK_CM_ERR(res);
    res = video_param->WriteSurface((const Ipp8u*)m_par, NULL);
    CHECK_CM_ERR(res);

    int numCtb = m_par->PicHeightInCtbs * m_par->PicWidthInCtbs;       

    // sao modes
    CmBuffer* frame_sao_offset_gfx = 0;
    size = numCtb * sizeof(SaoOffsetOut_gfx);
    int compCount =  m_par->SAOChromaFlag ? 3 : 1;
    res = device->CreateBuffer(compCount * size, frame_sao_offset_gfx);
    CHECK_CM_ERR(res);
    std::vector<SaoOffsetOut_gfx> tmp(compCount*numCtb);
    for (size_t i = 0; i < numCtb; i++) {
        for (int compIdx = 0; compIdx < compCount; compIdx++) {
            int pos = numCtb * compIdx + i;
            tmp[pos].mode_idx = offsets[i][compIdx].mode_idx;
            tmp[pos].type_idx = offsets[i][compIdx].type_idx;
            tmp[pos].startBand_idx = offsets[i][compIdx].startBand_idx;
            tmp[pos].saoMaxOffsetQVal = offsets[i][compIdx].saoMaxOffsetQVal;
            tmp[pos].offset[0] = offsets[i][compIdx].offset[0];
            tmp[pos].offset[1] = offsets[i][compIdx].offset[1];
            tmp[pos].offset[2] = offsets[i][compIdx].offset[2];
            tmp[pos].offset[3] = offsets[i][compIdx].offset[3];
        }
    }
    frame_sao_offset_gfx->WriteSurface((const Ipp8u*)tmp.data(), NULL);

    // IDX //
    SurfaceIndex *idxRecon = 0;    
    res = recon->GetIndex(idxRecon);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxOutputLu = 0;
    res = outputLu->GetIndex(idxOutputLu);
    CHECK_CM_ERR(res);    

    SurfaceIndex *idxOutputCh = 0;
    res = outputCh->GetIndex(idxOutputCh);
    CHECK_CM_ERR(res);    

    SurfaceIndex *idxOutput = 0;
    res = output->GetIndex(idxOutput);
    CHECK_CM_ERR(res);    

    SurfaceIndex *idxParam = 0;    
    res = video_param->GetIndex(idxParam);
    CHECK_CM_ERR(res);    
    
    SurfaceIndex *idx_frame_sao_offset = 0;    
    res = frame_sao_offset_gfx->GetIndex(idx_frame_sao_offset);
    CHECK_CM_ERR(res);

    // SET ARGS // 
    res = kernel->SetKernelArg(0, sizeof(SurfaceIndex), idxRecon);
    CHECK_CM_ERR(res);		
    res = kernel->SetKernelArg(1, sizeof(SurfaceIndex), idxOutput);
    CHECK_CM_ERR(res);		
    res = kernel->SetKernelArg(2, sizeof(SurfaceIndex), idxParam);
    CHECK_CM_ERR(res);		    
    res = kernel->SetKernelArg(3, sizeof(SurfaceIndex), idx_frame_sao_offset);
    CHECK_CM_ERR(res);

    mfxU32 MaxCUSize = m_par->MaxCUSize;

    // for YUV420 we process (MaxCuSize x MaxCuSize) block        
    mfxU32 tsWidth   = m_par->PicWidthInCtbs;
    mfxU32 tsHeight  = m_par->PicHeightInCtbs;
    res = kernel->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);        

    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);	
    CHECK_CM_ERR(res);

    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);

    res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

    CmQueue *queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);

    CmEvent * e = 0;
    res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);

    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);      

    // READ OUT DATA
    output->ReadSurfaceStride(frameDst, NULL, pitchDst);
    //res = device->FlushPrintBuffer();

#ifndef CMRT_EMU
    printf("TIME=%.3f ms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

    device->DestroyTask(task);

    queue->DestroyEvent(e);

    device->DestroySurface(recon);
    device->DestroySurface(output);
    device->DestroySurface2DUP(outputLu);
    device->DestroySurface2DUP(outputCh);
    CM_ALIGNED_FREE(outputSysLu);
    CM_ALIGNED_FREE(outputSysCh);
    device->DestroySurface(video_param);
    device->DestroySurface(frame_sao_offset_gfx);
    device->DestroyThreadSpace(threadSpace);
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    ::DestroyCmDevice(device);

    return PASSED;
}


int RunGpuEstimateAndApply(const Ipp8u *frameRecon, Ipp8u *frameDst, const Ipp16s *blockStats, SaoCtuParam *frame_sao_param, const VideoParam *m_par)
{
    int pitchRecon = m_par->Width;
    int pitchDst = m_par->Width;

    // DEVICE //
    mfxU32 version = 0;
    CmDevice *device = 0;
    Ipp32s res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_hevce_sao_hsw, sizeof(genx_hevce_sao_hsw), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(SaoEstimateAndApply), kernel);
    CHECK_CM_ERR(res);

    //-------------------------------------------------------
    // debug printf info
    //-------------------------------------------------------
    //res = device->InitPrintBuffer();
    //CHECK_CM_ERR(res);

    // src = deblocked reconstruct
    CmSurface2D *recon = 0;
    res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, recon);
    CHECK_CM_ERR(res);
    res = recon->WriteSurfaceStride(frameRecon, NULL, pitchRecon);
    CHECK_CM_ERR(res);

    // output luma
    CmSurface2D *output = 0;
    res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, output);
    CHECK_CM_ERR(res);

    CmBuffer *video_param = 0;
    res = device->CreateBuffer(sizeof(VideoParam), video_param);
    CHECK_CM_ERR(res);
    res = video_param->WriteSurface((const Ipp8u*)m_par, NULL);
    CHECK_CM_ERR(res);

    Ipp32s blockW = GPU_STAT_BLOCK_WIDTH;
    Ipp32s blockH = GPU_STAT_BLOCK_HEIGHT;
    mfxU32 numBlocks = m_par->PicWidthInCtbs * (m_par->MaxCUSize / blockW) * m_par->PicHeightInCtbs * (m_par->MaxCUSize / blockH);

    CmBuffer *stats = 0;
    res = device->CreateBuffer(numBlocks * (16+16+32+32) * sizeof(Ipp16s), stats);
    CHECK_CM_ERR(res);
    res = stats->WriteSurface((const Ipp8u *)blockStats, NULL);
    CHECK_CM_ERR(res);

    Ipp32s numCtb = m_par->PicWidthInCtbs * m_par->PicHeightInCtbs;
    CmBuffer *saoModes = 0;
    res = device->CreateBuffer(numCtb * sizeof(SaoOffsetOut_gfx), saoModes);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxRecon = 0;
    res = recon->GetIndex(idxRecon);
    CHECK_CM_ERR(res);
    SurfaceIndex *idxOutput = 0;
    res = output->GetIndex(idxOutput);
    CHECK_CM_ERR(res);
    SurfaceIndex *idxParam = 0;
    res = video_param->GetIndex(idxParam);
    CHECK_CM_ERR(res);
    SurfaceIndex *idxStats = 0;
    res = stats->GetIndex(idxStats);
    CHECK_CM_ERR(res);
    SurfaceIndex *idxSaoModes = 0;
    res = saoModes->GetIndex(idxSaoModes);
    CHECK_CM_ERR(res);

    res = kernel->SetKernelArg(0, sizeof(SurfaceIndex), idxRecon);
    CHECK_CM_ERR(res);		
    res = kernel->SetKernelArg(1, sizeof(SurfaceIndex), idxOutput);
    CHECK_CM_ERR(res);		
    res = kernel->SetKernelArg(2, sizeof(SurfaceIndex), idxParam);
    CHECK_CM_ERR(res);		    
    res = kernel->SetKernelArg(3, sizeof(SurfaceIndex), idxStats);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(4, sizeof(SurfaceIndex), idxSaoModes);
    CHECK_CM_ERR(res);

    mfxU32 tsWidth   = m_par->PicWidthInCtbs;
    mfxU32 tsHeight  = m_par->PicHeightInCtbs;
    res = kernel->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);        

    res = threadSpace->SelectThreadDependencyPattern(CM_WAVEFRONT);	
    CHECK_CM_ERR(res);

    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);

    res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

    CmQueue *queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);

    CmEvent * e = 0;
    res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);

    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);      

    // READ OUT DATA
    output->ReadSurfaceStride(frameDst, NULL, pitchDst);

    // conversion from gfx to cpu format here
    {
        int size = sizeof(SaoOffsetOut_gfx);
        Ipp8u* p_out_gfx = new Ipp8u[numCtb * size];
        SaoOffsetOut_gfx* sao_gfx = (SaoOffsetOut_gfx*)p_out_gfx;

        saoModes->ReadSurface( (Ipp8u*)p_out_gfx, NULL);

        for (int ctb = 0; ctb < numCtb; ctb++) {
            frame_sao_param[ctb][0].mode_idx = sao_gfx[ctb].mode_idx;
            frame_sao_param[ctb][0].type_idx = sao_gfx[ctb].type_idx;
            frame_sao_param[ctb][0].startBand_idx = sao_gfx[ctb].startBand_idx;
            frame_sao_param[ctb][0].saoMaxOffsetQVal = sao_gfx[ctb].saoMaxOffsetQVal;
            // offsets
            frame_sao_param[ctb][0].offset[0] = sao_gfx[ctb].offset[0];
            frame_sao_param[ctb][0].offset[1] = sao_gfx[ctb].offset[1];
            frame_sao_param[ctb][0].offset[2] = sao_gfx[ctb].offset[2];
            frame_sao_param[ctb][0].offset[3] = sao_gfx[ctb].offset[3];
        }

        delete [] p_out_gfx;
    }

#ifndef CMRT_EMU
    printf("TIME=%.3f ms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

    device->DestroyTask(task);

    queue->DestroyEvent(e);

    device->DestroySurface(recon);
    device->DestroySurface(output);
    device->DestroySurface(video_param);
    device->DestroySurface(stats);
    device->DestroySurface(saoModes);
    device->DestroyThreadSpace(threadSpace);
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    ::DestroyCmDevice(device);

    return PASSED;
}


void PadOnePix(Ipp8u *frame, Ipp32s pitch, Ipp32s width, Ipp32s height)
{
    Ipp8u *ptr = frame;
    for (Ipp32s i = 0; i < height; i++, ptr += pitch) {
        ptr[-1] = ptr[0];
        ptr[width] = ptr[width - 1];
    }

    ptr = frame - 1;
    memcpy(ptr - pitch, ptr, width + 2);
    ptr = frame + height * pitch - 1;
    memcpy(ptr, ptr - pitch, width + 2);
}

int RunCpuStatAndEstimate(const Ipp8u* frameOrigin, Ipp8u* frameRecon, const VideoParam* m_par, const AddrInfo* frame_addr_info,
                          SaoCtuParam* frame_sao_param, Ipp32s* diffEO, Ipp16u* countEO, Ipp32s* diffBO, Ipp16u* countBO, Ipp8u* availLeft, Ipp8u* availAbove)
{
    int pitchOrigin = m_par->Width;
    int pitchRecon = m_par->Width;

    EstimateSao(
        frameOrigin, pitchOrigin,
        frameRecon, pitchRecon,
        m_par,
        frame_addr_info,
        frame_sao_param,
        diffEO,
        countEO,
        diffBO,
        countBO,
        availLeft,
        availAbove);

    return PASSED;
}

union Avail
{
    struct
    {
        Ipp8u m_left         : 1;
        Ipp8u m_top          : 1;
        Ipp8u m_right        : 1;
        Ipp8u m_bottom       : 1;
        Ipp8u m_top_left     : 1;
        Ipp8u m_top_right    : 1;
        Ipp8u m_bottom_left  : 1;
        Ipp8u m_bottom_right : 1;
    };
    Ipp8u data;
};

int Compare(const Ipp8u *data1, const Ipp8u *data2, Ipp32s width, Ipp32s height, VideoParam* par, SaoCtuParam* sao)
{
    Ipp32s pitch1 = width;
    Ipp32s pitch2 = width;

#if 1
    // Luma
    for (Ipp32s y = 0; y < height; y++) {
        for (Ipp32s x = 0; x < width; x++) {
            Ipp32s diff = abs(data1[y * pitch1 + x] - data2[y * pitch2 + x]);
            if (diff) {
                printf("Luma bad pixels (x,y)=(%i,%i) %d != %d(ref)\n", x, y, data2[y * pitch2 + x], data1[y * pitch1 + x]);
                return FAILED;
            }
        }
    }
#endif

    // Chroma
    const Ipp8u* refChroma =  data1 + height*pitch1;
    const Ipp8u* tstChroma =  data2 + height*pitch2;

    for (Ipp32s y = 0; y < height/2; y++) {
        for (Ipp32s x = 0; x < width/2; x++) {

            int ctb_y = y / 32;
            int ctb_x = x / 32;
            int ctb = ctb_y * par->PicWidthInCtbs + ctb_x;

            /*if ( !(sao[ctb][1].mode_idx == SAO_MODE_OFF || sao[ctb][1].type_idx == 0 || sao[ctb][1].type_idx == 1 || sao[ctb][1].type_idx == 2) ) {
                continue;
            }*/

            // U
            Ipp32s diff = abs(refChroma[y * pitch1 + 2 * x] - tstChroma[y * pitch2 + 2 * x]);
            if (diff) {
                printf("Chroma_U bad pixels (x,y)=(%i,%i) %d != %d(ref)\n", 2*x, y, tstChroma[y * pitch2 + 2 * x], refChroma[y * pitch1 + 2 * x]);
                return FAILED;
            }
#if 1
            // V
            diff = abs(refChroma[y * pitch1 + 2 * x + 1] - tstChroma[y * pitch2 + 2 * x + 1]);
            if (diff) {
                printf("Chroma_V bad pixels (x,y)=(%i,%i) %d != %d(ref)\n", 2*x + 1, y, tstChroma[y * pitch2 + 2 * x + 1], refChroma[y * pitch1 + 2 * x + 1]);
                return FAILED;
            }
#endif
        }
    }

    return PASSED;
}

int Dump(Ipp8u* data, size_t frameSize, const char* fileName)
{
    FILE *fout = fopen(fileName, "wb");
    if (!fout)
        return FAILED;

    fwrite(data, 1, frameSize, fout);
    fclose(fout);

    return PASSED;
}

int CompareParam(SaoCtuParam* param_cpu, SaoCtuParam* param_gfx, Ipp32s numCtbs, int enableChroma)
{
    int countEO[3][4] = {0};
    int countBO[3] = {0};
    int countMergeLeft[3] = {0};
    int countMergeAbove[3] = {0};

    int compCount = enableChroma ? 3 : 1;
    char* compName[3] = {"Y", "U", "V"};

    for (Ipp32s ctb = 0; ctb < numCtbs; ctb++) {
        for (Ipp32s compIdx = 0; compIdx < compCount; compIdx++)
        //for (Ipp32s compIdx = 1; compIdx < compCount; compIdx++)
        //Ipp32s compIdx = 0;
        {
            SaoOffsetParam & ref = param_cpu[ctb][compIdx];
            SaoOffsetParam & tst = param_gfx[ctb][compIdx];

            if (param_cpu[ctb][compIdx].mode_idx != param_gfx[ctb][compIdx].mode_idx) {
                printf("\ncomp: %s\n", compName[compIdx]);
                printf("param[ctb = %i] mode_idx not equal\n", ctb);
                int a = param_cpu[ctb][compIdx].mode_idx;
                int b = param_gfx[ctb][compIdx].mode_idx;
                printf("cpu %i, gfx %i\n", a, b);
                return FAILED;
            }

            if (param_cpu[ctb][compIdx].type_idx != param_gfx[ctb][compIdx].type_idx) {
                printf("\ncomp: %s\n", compName[compIdx]);
                printf("param[ctb = %i] type_idx not equal\n", ctb);
                int a = param_cpu[ctb][compIdx].type_idx;
                int b = param_gfx[ctb][compIdx].type_idx;
                printf("cpu %i, gfx %i\n", a, b);
                return FAILED;
            }

            if (param_cpu[ctb][compIdx].startBand_idx != param_gfx[ctb][compIdx].startBand_idx) {
                printf("\ncomp: %s\n", compName[compIdx]);
                printf("param[ctb = %i] typeAuxInfo not equal\n", ctb);
                return FAILED;
            }

            int startBand = param_cpu[ctb][compIdx].startBand_idx;

            // EO
            if (param_gfx[ctb][compIdx].type_idx < 4 && param_cpu[ctb][compIdx].mode_idx == 1)
            {
                for (int offset_idx = 0; offset_idx < 4; offset_idx++) {
                    int step = offset_idx >= 2 ? 1 : 0;

                    if (param_cpu[ctb][compIdx].offset[offset_idx + step] != param_gfx[ctb][compIdx].offset[offset_idx]) {
                        printf("\ncomp: %s\n", compName[compIdx]);
                        printf("param[ctb = %i] offset[%i] not equal\n", ctb, offset_idx);
                        return FAILED;
                    }
                }
            }
            // BO
            else if (param_gfx[ctb][compIdx].type_idx == 4)
            {
                for (int offset_idx = 0; offset_idx < 4; offset_idx++) {
                    if (param_cpu[ctb][compIdx].offset[startBand + offset_idx] != param_gfx[ctb][compIdx].offset[/*startBand + */offset_idx]) {
                        printf("\ncomp: %s\n", compName[compIdx]);
                        printf("param[ctb = %i] offset[%i] not equal\n", ctb, offset_idx);
                        return FAILED;
                    }
                }
            }

            if (param_cpu[ctb][compIdx].mode_idx != SAO_MODE_OFF && param_gfx[ctb][compIdx].type_idx < 4 ) {//ON
                countEO[compIdx][ param_cpu[ctb][compIdx].type_idx ]++;
            } else if (param_cpu[ctb][compIdx].mode_idx != SAO_MODE_OFF && param_gfx[ctb][compIdx].type_idx == 4) {
                countBO[compIdx]++;
            }

            // once only!!!
            if (compIdx == 0) {
                if (param_cpu[ctb][compIdx].mode_idx == SAO_MODE_MERGE_LEFT) {
                    countMergeLeft[compIdx]++;
                } 
                if (param_cpu[ctb][compIdx].mode_idx == SAO_MODE_MERGE_ABOVE) {
                    countMergeAbove[compIdx]++;
                }
            }
        }
    }

    for (Ipp32s compIdx = 0; compIdx < compCount; compIdx++) {
        //printf("\n(stats [%s]: E0 %i E90 %i E135 %i E45 %i BO %i TOTAL %i) ", compName[compIdx], countEO[compIdx][0], countEO[compIdx][1], countEO[compIdx][2], countEO[compIdx][3], countBO[compIdx], numCtbs);
        if (compIdx == 0) {
            //printf("\n stats [%s]: Left %i Above %i Total: %i\n", compName[compIdx], countMergeLeft[compIdx], countMergeAbove[compIdx], numCtbs);
        }
    }

    return PASSED;
}

int CompareStats(Ipp32s* diffEO, Ipp16u* countEO, Ipp32s* diffBO, Ipp16u* countBO,
                 Ipp32s* diffEO_ref, Ipp16u* countEO_ref, Ipp32s* diffBO_ref, Ipp16u* countBO_ref, int   numCtbs, int enableChroma)
{
    // validation  part
    // EO - 4 stats with 5 elements
    // BO - 1 stats with 32 elements

    int compCount = enableChroma ? 3 : 1;
    char* compName[3] = {"Y", "U", "V"};
    for (int ctb = 0; ctb < numCtbs; ctb++) {
        for (int compIdx = 0; compIdx < compCount; compIdx++) {

            // EO
            int startOffset_ref =  numCtbs * (5*4) * compIdx;
            int startOffset_ref2 =  compIdx == 1 ? numCtbs * (5*4) * 2 : numCtbs * (5*4) * 1;//aya_dbg
            int startOffset     =  numCtbs * (4*4) * compIdx;
            for (int stat = 0; stat < 4; stat++)
            //int stat = 0;
            {
                for(int idx = 0; idx < 5; idx++) {

                if (idx == 2) {
                    continue;
                }

                    Ipp32s a = diffEO_ref[ startOffset_ref + (ctb*5*4) + 5*stat + idx];
                    int idx_gfx = idx > 2 ? idx - 1 : idx;
                    Ipp32s b = diffEO[ startOffset + (ctb*4*4) + 4*stat + idx_gfx];
                    Ipp32s a2 = diffEO_ref[ startOffset_ref2 + (ctb*5*4) + 5*stat + idx];

                    if (a != b) {
                        printf("\n diffEO[ctb=%i][stat=%i][%i]: %d != %d(ref)   [%s]\n", ctb, stat, idx, b, a, compName[compIdx]);
                        return FAILED;
                    }

                    if (1) {
                        Ipp32s c = countEO_ref[startOffset_ref + (ctb*5*4) + 5*stat + idx];
                        Ipp32s d = countEO[startOffset + (ctb*4*4) + 4*stat + idx_gfx];

                        if (c != d) {
                            printf("\n countEO[ctb=%i][stat=%i][%i]: %d != %d(ref)   [%s]\n", ctb, stat, idx, d, c, compName[compIdx]);
                            return FAILED;
                        }
                    }
                }
            }

            //BO
            startOffset = numCtbs * 32 * compIdx;
            if (diffBO && countBO && diffBO_ref && countBO_ref) {
                for(int idx = 0; idx < 32; idx++) {
                    Ipp32s a = diffBO[startOffset + (ctb*32) + idx];
                    Ipp32s b = diffBO_ref[startOffset + (ctb*32) + idx];

                    if (a != b) {
                        printf("\n diffBO[ctb = %i][%i][%s] \n", ctb, idx, compName[compIdx]);
                        return FAILED;
                    }

                    Ipp32s c = countBO[startOffset + (ctb*32) + idx];
                    Ipp32s d = countBO_ref[startOffset + (ctb*32) + idx];

                    if (c != d) {
                        printf("\n countBO[ctb = %i][%i][%s] \n", ctb, idx, compName[compIdx]);
                        return FAILED;
                    }
                }
            }
        }
    }

    return PASSED;
}


//---------------------------------------------------------
// Test Utils/Sim
//---------------------------------------------------------
static
int random(int a,int b)
{
    if (a > 0) return a + rand() % (b - a);
    else return a + rand() % (abs(a) + b);
}

#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))

// 8b / 420 only!!!
void SimulateRecon(Ipp8u* input, Ipp32s inPitch, Ipp8u* recon, Ipp32s reconPitch, Ipp32s width, Ipp32s height, Ipp32s maxAbsPixDiff)
{
    int pix;
    for (int row = 0; row < height; row++) {
        for (int x = 0; x < width; x++) {
            pix = input[row * inPitch + x] + random(-maxAbsPixDiff, maxAbsPixDiff);
            recon[row * reconPitch + x] = Saturate(0, 255, pix);
        }
    }


    Ipp8u* inputUV = input + height * inPitch;
    Ipp8u* reconUV = recon + height * reconPitch;
    for (int row = 0; row < height/2; row++) {
        for (int x = 0; x < width; x += 2) {
            pix = inputUV[row * inPitch + x] + random(-maxAbsPixDiff, maxAbsPixDiff);
            reconUV[row * reconPitch + x] = Saturate(0, 255, pix);

            pix = inputUV[row * inPitch + x + 1] + random(-maxAbsPixDiff, maxAbsPixDiff);
            reconUV[row * reconPitch + x + 1] = Saturate(0, 255, pix);
        }
    }
}

void SetVideoParam(VideoParam & par, Ipp32s width, Ipp32s height)
{
    par.Width = width;
    par.Height= height;

    par.MaxCUSize = 64;// !!!

    par.PicWidthInCtbs  = (par.Width + par.MaxCUSize - 1) / par.MaxCUSize;
    par.PicHeightInCtbs = (par.Height + par.MaxCUSize - 1) / par.MaxCUSize;

    par.chromaFormatIdc = 1;// !!!

    //par.bitDepthLuma = 8;// !!!

    //par.saoOpt = 1;
    par.SAOChromaFlag = 0;
    par.enableBandOffset = 1;

    Ipp32s qp = 27; // !!!
    par.m_rdLambda = pow(2.0f, (qp - 12) * (1 / 3.0f)) * (1 / 256.0f);
}

//---------------------------------------------------------
// SAO CPU CODE
//---------------------------------------------------------

struct SaoCtuStatistics //data structure for SAO statistics
{
    //static const int MAX_NUM_SAO_CLASSES = 32;

    Ipp32s diff[MAX_NUM_SAO_CLASSES];
    Ipp32s count[MAX_NUM_SAO_CLASSES];

    void Reset()
    {
        memset(diff, 0, sizeof(Ipp32s)*MAX_NUM_SAO_CLASSES);
        memset(count, 0, sizeof(Ipp32s)*MAX_NUM_SAO_CLASSES);
    }
};

struct SaoEstimator
{
    Ipp32s   m_numSaoModes;
    Ipp32s   m_bitDepth;
    Ipp32s   m_saoMaxOffsetQVal;
    SaoCtuStatistics m_statData[NUM_SAO_COMPONENTS][NUM_SAO_BASE_TYPES];
    Ipp32f   m_labmda[NUM_SAO_COMPONENTS];
};

union CTBBorders
{
    struct
    {
        Ipp8u m_left         : 1;
        Ipp8u m_top          : 1;
        Ipp8u m_right        : 1;
        Ipp8u m_bottom       : 1;
        Ipp8u m_top_left     : 1;
        Ipp8u m_top_right    : 1;
        Ipp8u m_bottom_left  : 1;
        Ipp8u m_bottom_right : 1;
    };
    Ipp8u data;
};

typedef Ipp8u Pel;
typedef Ipp8s Char;
typedef int Int;

#define Clip3(m_Min, m_Max, m_Value) ((m_Value) < (m_Min) ? (m_Min) : (m_Value) > (m_Max) ? (m_Max) : (m_Value))

inline Ipp16s sgn(Ipp32s x)
{
    return ((x >> 31) | ((Ipp32s)( (((Ipp32u) -x)) >> 31)));
}


void ApplyBlockSao(const int channelBitDepth, int typeIdx, int startBand_idx, Ipp16s* offset4,// [4]
    Pel* srcBlk, Int srcStride, Pel* resBlk, Int resStride,  
    Int width, Int height, Avail & avail, int MaxCuSize)
{
    int buf_offset[32];
    for (int idx = 0; idx < 32; idx++) {
        buf_offset[idx] = 0;
    }

    if (typeIdx == SAO_TYPE_BO) {
        buf_offset[(startBand_idx + 0) & 31] = offset4[0];
        buf_offset[(startBand_idx + 1) & 31] = offset4[1];
        buf_offset[(startBand_idx + 2) & 31] = offset4[2];
        buf_offset[(startBand_idx + 3) & 31] = offset4[3];
    } else { // any EO
        buf_offset[0] = offset4[0];
        buf_offset[1] = offset4[1];
        buf_offset[2] = 0;
        buf_offset[3] = offset4[2];
        buf_offset[4] = offset4[3];
    }
    int* offset = buf_offset;

    bool isLeftAvail = avail.m_left;  
    bool isRightAvail = avail.m_right; 
    bool isAboveAvail = avail.m_top; 
    bool isBelowAvail = avail.m_bottom; 
    bool isAboveLeftAvail = avail.m_top_left; 
    bool isAboveRightAvail = avail.m_top_right;  
    bool isBelowLeftAvail = avail.m_bottom_left;
    bool isBelowRightAvail = avail.m_bottom_right;

    Char m_signLineBuf1[64 + 1];
    Char m_signLineBuf2[64 + 1];
    int m_lineBufWidth = MaxCuSize;

    const Int maxSampleValueIncl = (1<< channelBitDepth )-1;

    Int x,y, startX, startY, endX, endY, edgeType;
    Int firstLineStartX, firstLineEndX, lastLineStartX, lastLineEndX;
    Char signLeft, signRight, signDown;

    Pel* srcLine = srcBlk;
    Pel* resLine = resBlk;

    switch(typeIdx)
    {
    case SAO_TYPE_EO_0:
        {
            offset += 2;
            startX = isLeftAvail ? 0 : 1;
            endX   = isRightAvail ? width : (width -1);
            for (y=0; y< height; y++)
            {
                signLeft = (Char)sgn(srcLine[startX] - srcLine[startX-1]);
                for (x=startX; x< endX; x++)
                {
                    signRight = (Char)sgn(srcLine[x] - srcLine[x+1]); 
                    edgeType =  signRight + signLeft;
                    signLeft  = -signRight;

                    resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);
                }
                srcLine  += srcStride;
                resLine += resStride;
            }

        }
        break;
    case SAO_TYPE_EO_90:
        {
            offset += 2;
            Char *signUpLine = m_signLineBuf1;

            startY = isAboveAvail ? 0 : 1;
            endY   = isBelowAvail ? height : height-1;
            if (!isAboveAvail)
            {
                srcLine += srcStride;
                resLine += resStride;
            }

            Pel* srcLineAbove= srcLine- srcStride;
            for (x=0; x< width; x++)
            {
                signUpLine[x] = (Char)sgn(srcLine[x] - srcLineAbove[x]);
            }

            Pel* srcLineBelow;
            for (y=startY; y<endY; y++)
            {
                srcLineBelow= srcLine+ srcStride;

                for (x=0; x< width; x++)
                {
                    signDown  = (Char)sgn(srcLine[x] - srcLineBelow[x]);
                    edgeType = signDown + signUpLine[x];
                    signUpLine[x]= -signDown;

                    resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);
                }
                srcLine += srcStride;
                resLine += resStride;
            }

        }
        break;
    case SAO_TYPE_EO_135:
        {
            offset += 2;
            Char *signUpLine, *signDownLine, *signTmpLine;

            signUpLine  = m_signLineBuf1;
            signDownLine= m_signLineBuf2;

            startX = isLeftAvail ? 0 : 1 ;
            endX   = isRightAvail ? width : (width-1);

            //prepare 2nd line's upper sign
            Pel* srcLineBelow= srcLine+ srcStride;
            for (x=startX; x< endX+1; x++)
            {
                signUpLine[x] = (Char)sgn(srcLineBelow[x] - srcLine[x- 1]);
            }

            //1st line
            Pel* srcLineAbove= srcLine- srcStride;
            firstLineStartX = isAboveLeftAvail ? 0 : 1;
            firstLineEndX   = isAboveAvail? endX: 1;
            for(x= firstLineStartX; x< firstLineEndX; x++)
            {
                edgeType  =  sgn(srcLine[x] - srcLineAbove[x- 1]) - signUpLine[x+1];

                resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);
            }
            srcLine  += srcStride;
            resLine  += resStride;


            //middle lines
            for (y= 1; y< height-1; y++)
            {
                srcLineBelow= srcLine+ srcStride;

                for (x=startX; x<endX; x++)
                {
                    signDown =  (Char)sgn(srcLine[x] - srcLineBelow[x+ 1]);
                    edgeType =  signDown + signUpLine[x];
                    resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);

                    signDownLine[x+1] = -signDown;
                }
                signDownLine[startX] = (Char)sgn(srcLineBelow[startX] - srcLine[startX-1]);

                signTmpLine  = signUpLine;
                signUpLine   = signDownLine;
                signDownLine = signTmpLine;

                srcLine += srcStride;
                resLine += resStride;
            }

            //last line
            srcLineBelow= srcLine+ srcStride;
            lastLineStartX = isBelowAvail ? startX : (width -1);
            lastLineEndX   = isBelowRightAvail ? width : (width -1);
            for(x= lastLineStartX; x< lastLineEndX; x++)
            {
                edgeType =  sgn(srcLine[x] - srcLineBelow[x+ 1]) + signUpLine[x];
                resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);

            }
        }
        break;
    case SAO_TYPE_EO_45:
        {
            offset += 2;
            Char *signUpLine = m_signLineBuf1+1;

            startX = isLeftAvail ? 0 : 1;
            endX   = isRightAvail ? width : (width -1);

            //prepare 2nd line upper sign
            Pel* srcLineBelow= srcLine+ srcStride;
            for (x=startX-1; x< endX; x++)
            {
                signUpLine[x] = (Char)sgn(srcLineBelow[x] - srcLine[x+1]);
            }


            //first line
            Pel* srcLineAbove= srcLine- srcStride;
            firstLineStartX = isAboveAvail ? startX : (width -1 );
            firstLineEndX   = isAboveRightAvail ? width : (width-1);
            for(x= firstLineStartX; x< firstLineEndX; x++)
            {
                edgeType = sgn(srcLine[x] - srcLineAbove[x+1]) -signUpLine[x-1];
                resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);
            }
            srcLine += srcStride;
            resLine += resStride;

            //middle lines
            for (y= 1; y< height-1; y++)
            {
                srcLineBelow= srcLine+ srcStride;

                for(x= startX; x< endX; x++)
                {
                    signDown =  (Char)sgn(srcLine[x] - srcLineBelow[x-1]);
                    edgeType =  signDown + signUpLine[x];
                    resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);
                    signUpLine[x-1] = -signDown;
                }
                signUpLine[endX-1] = (Char)sgn(srcLineBelow[endX-1] - srcLine[endX]);
                srcLine  += srcStride;
                resLine += resStride;
            }

            //last line
            srcLineBelow= srcLine+ srcStride;
            lastLineStartX = isBelowLeftAvail ? 0 : 1;
            lastLineEndX   = isBelowAvail ? endX : 1;
            for(x= lastLineStartX; x< lastLineEndX; x++)
            {
                edgeType = sgn(srcLine[x] - srcLineBelow[x-1]) + signUpLine[x];
                resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);

            }
        }
        break;
    case SAO_TYPE_BO:
        {
            const Int shiftBits = channelBitDepth - NUM_SAO_BO_CLASSES_LOG2;
            for (y=0; y< height; y++)
            {
                for (x=0; x< width; x++)
                {
                    resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[srcLine[x] >> shiftBits] );
                }
                srcLine += srcStride;
                resLine += resStride;
            }
        }
        break;
    default:
        {
            printf("Not a supported SAO types\n");
            //assert(0);
            //exit(-1);
        }
    }
}

template <class T>
void h265_SplitChromaCtb_px(const T *nv12, Ipp32s pitchNv12, T *u, Ipp32s pitchU, T *v, Ipp32s pitchV, Ipp32s width, Ipp32s height)
{
    for (Ipp32s y = 0; y < height; y++, nv12 += pitchNv12, u += pitchU, v += pitchV) {
        for (Ipp32s x = 0; x < width; x++) {
            u[x] = nv12[2 * x];
            v[x] = nv12[2 * x + 1];
        }
    }
}

template <class T>
void InterleavePlanes(T *u, Ipp32s pitchU, T* v, Ipp32s pitchV, T* nv12, Ipp32s pitchNv12, Ipp32s width, Ipp32s height)
{
    for (Ipp32s y = 0; y < height; y++, nv12 += pitchNv12, u += pitchU, v += pitchV) {
        for (Ipp32s x = 0; x < width; x++) {
            nv12[2 * x]     = u[x];
            nv12[2 * x + 1] = v[x];
        }
    }
}
int RunCpuApply(const Ipp8u *frameRecon, Ipp8u *frameDst, const VideoParam *m_par, const SaoCtuParam *offsets)
{     
    int pitchRecon = m_par->Width;
    int pitchDst = m_par->Width;
    int numCtb = m_par->PicWidthInCtbs * m_par->PicHeightInCtbs;
    for (int y = 0; y < 3*m_par->Height/2; y++)
        memcpy(frameDst + y * pitchDst, frameRecon + y * pitchRecon, m_par->Width);
    // split NV12 plane to reuse the same func
    Ipp8u* reconUV  = (Ipp8u*)frameRecon + m_par->Height * pitchRecon;
    int chromaSize = m_par->Width * m_par->Height >> 2;
    std::vector<Ipp8u> reconPlaneU(chromaSize);
    std::vector<Ipp8u> reconPlaneV(chromaSize);
    std::vector<Ipp8u> dstPlaneU(chromaSize);
    std::vector<Ipp8u> dstPlaneV(chromaSize);
    int planePitchChroma = m_par->Width >> 1;
    Ipp32s pitchRecChroma = pitchRecon;

    h265_SplitChromaCtb_px( (Ipp8u*)reconUV,  pitchRecChroma, reconPlaneU.data(),  planePitchChroma, reconPlaneV.data(),  planePitchChroma, m_par->Width >> 1, m_par->Height >> 1);
    memcpy(dstPlaneU.data(), reconPlaneU.data(), reconPlaneU.size());
    memcpy(dstPlaneV.data(), reconPlaneV.data(), reconPlaneV.size());

    for (int ctbAddr = 0; ctbAddr < numCtb; ctbAddr++) {
        Ipp32s ctbPelX = (ctbAddr % m_par->PicWidthInCtbs) * m_par->MaxCUSize;
        Ipp32s ctbPelY = (ctbAddr / m_par->PicWidthInCtbs) * m_par->MaxCUSize;
        // apply for pixels.
        Ipp32s height  = (ctbPelY + m_par->MaxCUSize > m_par->Height) ? (m_par->Height- ctbPelY) : m_par->MaxCUSize;
        Ipp32s width   = (ctbPelX + m_par->MaxCUSize > m_par->Width)  ? (m_par->Width - ctbPelX) : m_par->MaxCUSize;
        
        Avail avail;
        if(1)
        {
            Ipp32s ctbAddrEnd = m_par->PicWidthInCtbs * m_par->PicHeightInCtbs;
            avail.m_left = (ctbAddr % m_par->PicWidthInCtbs) > 0 ? 1 : 0;
            avail.m_top  = (ctbAddr >= m_par->PicWidthInCtbs) ? 1 : 0;
            avail.m_top_left = avail.m_top & avail.m_left;
            //avail.m_right = ((ctbAddr % m_par->PicWidthInCtbs) < (m_par->PicWidthInCtbs - 1)) ? 1 : 0;
            avail.m_right = ((ctbPelX + width) < m_par->Width) ? 1 : 0;

            avail.m_top_right = avail.m_top & avail.m_right;
            //avail.m_bottom = (ctbAddr < (ctbAddrEnd - m_par->PicWidthInCtbs)) ? 1 : 0;
            avail.m_bottom = ((ctbPelY + height) < m_par->Height) ? 1 : 0;

            avail.m_bottom_left = avail.m_bottom & avail.m_left;
            avail.m_bottom_right = avail.m_bottom & avail.m_right;
        }

        // luma
        Ipp8u *reconY = (Ipp8u *)frameRecon + ctbPelX + ctbPelY * pitchRecon;	
        Ipp8u *dstY = frameDst + ctbPelX + ctbPelY * pitchDst;	

        SaoOffsetOut_gfx offset;
        offset.mode_idx = offsets[ctbAddr][SAO_Y].mode_idx;
        offset.type_idx = offsets[ctbAddr][SAO_Y].type_idx;
        offset.startBand_idx = offsets[ctbAddr][SAO_Y].startBand_idx;
        offset.saoMaxOffsetQVal = offsets[ctbAddr][SAO_Y].saoMaxOffsetQVal;
        if (offset.type_idx == SAO_TYPE_BO) {
            offset.offset[0] = offsets[ctbAddr][SAO_Y].offset[offset.startBand_idx+0];
            offset.offset[1] = offsets[ctbAddr][SAO_Y].offset[offset.startBand_idx+1];
            offset.offset[2] = offsets[ctbAddr][SAO_Y].offset[offset.startBand_idx+2];
            offset.offset[3] = offsets[ctbAddr][SAO_Y].offset[offset.startBand_idx+3];
        } else if (offset.mode_idx != SAO_MODE_OFF) {
            offset.offset[0] = offsets[ctbAddr][SAO_Y].offset[0];
            offset.offset[1] = offsets[ctbAddr][SAO_Y].offset[1];
            offset.offset[2] = offsets[ctbAddr][SAO_Y].offset[3];
            offset.offset[3] = offsets[ctbAddr][SAO_Y].offset[4];
        }

        if (offset.mode_idx != SAO_MODE_OFF) {
            ApplyBlockSao(8, offset.type_idx, offset.startBand_idx, offset.offset, reconY, pitchRecon, dstY, pitchDst, width, height, avail, m_par->MaxCUSize);  
        }

        // chroma
        if (m_par->SAOChromaFlag) {
            Ipp8u* buffRec[3] = {NULL, reconPlaneU.data(), reconPlaneV.data()};
            Ipp8u* buffDst[3] = {NULL, dstPlaneU.data(), dstPlaneV.data()};

            for (int compIdx = 1; compIdx < 3; compIdx++) {
                Ipp8u *recon = buffRec[compIdx] + (ctbPelX >> 1) + (ctbPelY >> 1) * planePitchChroma;
                Ipp8u *dst   = buffDst[compIdx] + (ctbPelX >> 1) + (ctbPelY >> 1) * planePitchChroma;
                
                offset.mode_idx = offsets[ctbAddr][compIdx].mode_idx;
                offset.type_idx = offsets[ctbAddr][compIdx].type_idx;
                offset.startBand_idx = offsets[ctbAddr][compIdx].startBand_idx;
                offset.saoMaxOffsetQVal = offsets[ctbAddr][compIdx].saoMaxOffsetQVal;
                if (offset.type_idx == SAO_TYPE_BO) {
                    offset.offset[0] = offsets[ctbAddr][compIdx].offset[offset.startBand_idx+0];
                    offset.offset[1] = offsets[ctbAddr][compIdx].offset[offset.startBand_idx+1];
                    offset.offset[2] = offsets[ctbAddr][compIdx].offset[offset.startBand_idx+2];
                    offset.offset[3] = offsets[ctbAddr][compIdx].offset[offset.startBand_idx+3];
                } else if (offset.mode_idx != SAO_MODE_OFF) {
                    offset.offset[0] = offsets[ctbAddr][compIdx].offset[0];
                    offset.offset[1] = offsets[ctbAddr][compIdx].offset[1];
                    offset.offset[2] = offsets[ctbAddr][compIdx].offset[3];
                    offset.offset[3] = offsets[ctbAddr][compIdx].offset[4];
                }

                if (offset.mode_idx != SAO_MODE_OFF) {
                    ApplyBlockSao(8, offset.type_idx, offset.startBand_idx, offset.offset, recon, planePitchChroma, dst, planePitchChroma, width >> 1, height >> 1, avail, m_par->MaxCUSize >> 1);
                }
            }
        }
    }
    // restore UV interleaved frame
    if (m_par->SAOChromaFlag) {
        Ipp8u* dstUV  = (Ipp8u*)frameDst + m_par->Height * pitchRecon;
        InterleavePlanes(dstPlaneU.data(), planePitchChroma, dstPlaneV.data(), planePitchChroma, dstUV, pitchDst, m_par->Width >> 1, m_par->Height >> 1);
    }

    return PASSED;
}

typedef Ipp8u PixType;

#if (defined(__INTEL_COMPILER) || defined(_MSC_VER)) && !defined(_WIN32_WCE)
#define __ALIGN32 __declspec (align(32))
//#define __ALIGN16 __declspec (align(16))
//#define __ALIGN8 __declspec (align(8))
#elif defined(__GNUC__)
#define __ALIGN32 __attribute__ ((aligned (32)))
//#define __ALIGN16 __attribute__ ((aligned (16)))
//#define __ALIGN8 __attribute__ ((aligned (8)))
#else
#define __ALIGN32
//#define __ALIGN16
//#define __ALIGN8
#endif

/* ColorFormat */
enum {
    MFX_CHROMAFORMAT_MONOCHROME =0,
    MFX_CHROMAFORMAT_YUV420     =1,
    MFX_CHROMAFORMAT_YUV422     =2,
    MFX_CHROMAFORMAT_YUV444     =3,
    MFX_CHROMAFORMAT_YUV400     = MFX_CHROMAFORMAT_MONOCHROME,
    MFX_CHROMAFORMAT_YUV411     = 4,
    MFX_CHROMAFORMAT_YUV422H    = MFX_CHROMAFORMAT_YUV422,
    MFX_CHROMAFORMAT_YUV422V    = 5
};


#define MAX_DOUBLE                  1.7e+308    ///< max. value of double-type value
#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))

// see IEEE "Sample Adaptive Offset in the HEVC standard", p1760 Fast Distortion Estimation
inline Ipp64s EstimateDeltaDist(Ipp64s count, Ipp64s offset, Ipp64s diffSum, int shift)
{
    return (( count*offset*offset - diffSum*offset*2 ) >> shift);
}

Ipp64s GetDistortion( int typeIdx, int typeAuxInfo, int* quantOffset,  SaoCtuStatistics& statData, int shift)
{
    Ipp64s dist=0;
    int startFrom = 0;
    int end = NUM_SAO_EO_CLASSES;

    if (SAO_TYPE_BO == typeIdx) {
        startFrom = typeAuxInfo;
        end = startFrom + 4;
    }
    for (int offsetIdx=startFrom; offsetIdx < end; offsetIdx++) {
        dist += EstimateDeltaDist( statData.count[offsetIdx], quantOffset[offsetIdx], statData.diff[offsetIdx], shift);
    }
    return dist;
}


// try on each offset from [0, inputOffset]
inline int GetBestOffset(int typeIdx, int classIdx, Ipp64f lambda, int inputOffset,
                         Ipp64s count, Ipp64s diffSum, int shift, int offsetTh,
                         Ipp64f* bestCost = NULL, Ipp64s* bestDist = NULL)
{
    int iterOffset = inputOffset < 0 ? -8 : 8; // SAO MOD
    int outputOffset = 0;
    int testOffset;
    Ipp64s tempDist, tempRate;
    Ipp64f cost, minCost;

    minCost = FLT_MAX;
    while (iterOffset != 0) {
        iterOffset = (iterOffset > 0) ? (iterOffset-1) : (iterOffset+1);
        tempRate = abs(iterOffset) + 1;
        if (abs(iterOffset) == offsetTh) {
            tempRate --;
        }
        if (typeIdx == SAO_TYPE_BO && iterOffset != 0)
            tempRate++; // sign bit

        testOffset  = iterOffset;
        tempDist    = EstimateDeltaDist( count, testOffset, diffSum, shift);
        cost    = (Ipp64f)tempDist + lambda * (Ipp64f) (tempRate * 256);

        if (cost < minCost) {
            minCost = cost;
            outputOffset = iterOffset;
            if (bestDist) *bestDist = tempDist;
            if (bestCost) *bestCost = cost;
        }
    }
    return outputOffset;
}

static int tab_numSaoClass[] = {NUM_SAO_EO_CLASSES, NUM_SAO_EO_CLASSES, NUM_SAO_EO_CLASSES, NUM_SAO_EO_CLASSES, NUM_SAO_BO_CLASSES};


double floor1(double x)
{
    if (x > 0)
        return (int)x;
    else
        return (int)(x-0.9999999999999999);
}

void GetQuantOffsets(int typeIdx,  SaoCtuStatistics& statData, int* quantOffsets, int& typeAuxInfo, Ipp64f lambda, int bitDepth, int saoMaxOffsetQval, int shift)
{
    memset(quantOffsets, 0, sizeof(int)*MAX_NUM_SAO_CLASSES);
    //memset(quantOffsets, 0, sizeof(int)*5);//AYA_DEBUG!!!

    int numClasses = tab_numSaoClass[typeIdx];
    int classIdx;

    // calculate 'native' offset
    for (classIdx = 0; classIdx < numClasses; classIdx++) {
        if ( 0 == statData.count[classIdx] || (SAO_CLASS_EO_PLAIN == classIdx && SAO_TYPE_BO != typeIdx) ) {
            continue; //offset will be zero
        }

        Ipp64f meanDiff = (Ipp64f)(statData.diff[classIdx] << (bitDepth - 8)) / (Ipp64f)(statData.count[classIdx]);

        int offset;
        if (bitDepth > 8) {
            offset = (int)meanDiff;
            offset += offset >= 0 ? (1<<(bitDepth-8-1)) : -(1<<(bitDepth-8-1));
            offset >>= (bitDepth-8);
        } else {
            offset = (int)floor1(meanDiff + 0.5);
        }

        quantOffsets[classIdx] = Saturate(-saoMaxOffsetQval, saoMaxOffsetQval, offset);
    }

    // try on to improve a 'native' offset
    if (SAO_TYPE_BO == typeIdx) {
        Ipp64f cost[NUM_SAO_BO_CLASSES];
        for (int classIdx = 0; classIdx < NUM_SAO_BO_CLASSES; classIdx++) {
            cost[classIdx] = lambda;
			// fix from NS
            if (quantOffsets[classIdx] == 0)
                quantOffsets[classIdx] = 7;
			//	
            if (quantOffsets[classIdx] != 0 ) {
                quantOffsets[classIdx] = GetBestOffset( typeIdx, classIdx, lambda, quantOffsets[classIdx],
                                                        statData.count[classIdx], statData.diff[classIdx],
                                                        shift, saoMaxOffsetQval, cost + classIdx);
            }
        }

        Ipp64f minCost = MAX_DOUBLE; 
        int band, startBand = 0;
#if 0
        Ipp64f bandCost;
        for (band = 0; band < (NUM_SAO_BO_CLASSES - 4 + 1); band++) {
            bandCost  = cost[band  ] + cost[band+1] + cost[band+2] + cost[band+3];

            if (bandCost < minCost) {
                minCost = bandCost;
                startBand = band;
            }
        }
#else
        Ipp64f bandCost[32];
        Ipp64f bandIdx[32];
        bandCost[29] = bandCost[30] = bandCost[31] = MAX_DOUBLE;
        for (band = 0; band < (NUM_SAO_BO_CLASSES - 4 + 1); band++) {
            bandCost[band]  = cost[band  ] + cost[band+1] + cost[band+2] + cost[band+3];
            bandIdx[band] = band;
        }

        // simulation MinHalf like in kernel
        Ipp32s numElements = 32;
        for (int i = 0; i < 5; i++) {
            Ipp32s half = numElements / 2;
            for (band = 0; band < half; band++) {
                Ipp32s mask = bandCost[band] > bandCost[half + band];
                if (mask) {
                    bandCost[band] = bandCost[half + band];
                    bandIdx[band]  = bandIdx[half + band];
                }
            }

            numElements >>= 1;
        }

        startBand = bandIdx[0];
#endif

        // clear unused bands
        for (band = 0; band < startBand; band++)
            quantOffsets[band] = 0;

        for (band = startBand + 4; band < NUM_SAO_BO_CLASSES; band++)
            quantOffsets[band] = 0;

        typeAuxInfo = startBand;

    } else { // SAO_TYPE_E[0/45/90/135]
        for (classIdx = 0; classIdx < NUM_SAO_EO_CLASSES; classIdx++) {
		
            // equations modified by NS
            if (SAO_CLASS_EO_FULL_PEAK == classIdx && quantOffsets[classIdx] >= 0   ||
                SAO_CLASS_EO_HALF_PEAK == classIdx && quantOffsets[classIdx] >= 0) {
                quantOffsets[classIdx] = -7;
            }
			if (SAO_CLASS_EO_FULL_VALLEY == classIdx && quantOffsets[classIdx] <= 0 ||
                SAO_CLASS_EO_HALF_VALLEY == classIdx && quantOffsets[classIdx] <= 0) {
                quantOffsets[classIdx] = 7;
            }

            if ( quantOffsets[classIdx] != 0 ) {
                quantOffsets[classIdx] = GetBestOffset( typeIdx, classIdx, lambda, quantOffsets[classIdx],
                                                        statData.count[classIdx], statData.diff[classIdx], shift, saoMaxOffsetQval);
            }
        }
        typeAuxInfo = 0;
    }
}

Ipp32s CodeSaoCtbOffsetParam_BitCost(int compIdx, SaoOffsetParam& ctbParam, bool sliceEnabled)
{
    Ipp32u code;
    Ipp32s bitsCount = 0;
    if (!sliceEnabled) {
        return 0;
    }

    if (compIdx == SAO_Y || compIdx == SAO_Cb) {
        if (ctbParam.mode_idx == SAO_MODE_OFF) {
            code =0;
            bitsCount++;
        } else if(ctbParam.type_idx == SAO_TYPE_BO) {
            code = 1;
            bitsCount++;
            bitsCount++;
        } else {
            //VM_ASSERT(ctbParam.type_idx < SAO_TYPE_BO); //EO
            code = 2;
            bitsCount += 2;
        }
    }

    if (ctbParam.mode_idx == SAO_MODE_ON) {
        int numClasses = (ctbParam.type_idx == SAO_TYPE_BO)?4:NUM_SAO_EO_CLASSES;
        int offset[5]; // only 4 are used, 5th is added for KW
        int k=0;
        for (int i=0; i< numClasses; i++) {
            if (ctbParam.type_idx != SAO_TYPE_BO && i == SAO_CLASS_EO_PLAIN) {
                continue;
            }
            int classIdx = (ctbParam.type_idx == SAO_TYPE_BO)?(  (ctbParam.startBand_idx+i)% NUM_SAO_BO_CLASSES   ):i;
            offset[k] = ctbParam.offset[classIdx];
            k++;
        }

        for (int i=0; i< 4; i++) {

            code = (Ipp32u)( offset[i] < 0) ? (-offset[i]) : (offset[i]);
            if (ctbParam.saoMaxOffsetQVal != 0) {
                Ipp32u i;
                Ipp8u code_last = ((Ipp32u)ctbParam.saoMaxOffsetQVal > code);
                if (code == 0) {
                    bitsCount++;
                } else {
                    bitsCount++;
                    for (i=0; i < code-1; i++) {
                        bitsCount++;
                    }
                    if (code_last) {
                        bitsCount++;
                    }
                }
            }
        }


        if (ctbParam.type_idx == SAO_TYPE_BO) {
            for (int i=0; i< 4; i++) {
                if (offset[i] != 0) {
                    bitsCount++;
                }
            }
            bitsCount += NUM_SAO_BO_CLASSES_LOG2;
        } else {
            if(compIdx == SAO_Y || compIdx == SAO_Cb) {
                //VM_ASSERT(ctbParam.type_idx - SAO_TYPE_EO_0 >=0);
                bitsCount +=  NUM_SAO_EO_TYPES_LOG2;
            }
        }
    }

    return bitsCount << 8;
}


Ipp32s CodeSaoCtbParam_BitCost(SaoCtuParam& saoBlkParam, bool* sliceEnabled, bool leftMergeAvail, bool aboveMergeAvail, bool onlyEstMergeInfo)
{
    bool isLeftMerge = false;
    bool isAboveMerge= false;
    Ipp32u code = 0;
    Ipp32s bitsCount = 0;

    if(leftMergeAvail) {
        isLeftMerge = ((saoBlkParam[SAO_Y].mode_idx == SAO_MODE_MERGE_LEFT));
        code = isLeftMerge ? 1 : 0;
        bitsCount++;
    }

    if( aboveMergeAvail && !isLeftMerge) {
        isAboveMerge = ((saoBlkParam[SAO_Y].mode_idx == SAO_MODE_MERGE_ABOVE));
        code = isAboveMerge ? 1 : 0;
        bitsCount++;
    }

    bitsCount <<= 8;
    if(onlyEstMergeInfo) {
        return bitsCount;
    }

    if(!isLeftMerge && !isAboveMerge) {

        for (int compIdx=0; compIdx < NUM_SAO_COMPONENTS; compIdx++) {
            bitsCount += CodeSaoCtbOffsetParam_BitCost(compIdx, saoBlkParam[compIdx], sliceEnabled[compIdx]);
        }
    }

    return bitsCount;
}

#define ENABLE_BAND_OFFSET

void GetBestSao_BitCost(bool* sliceEnabled, SaoCtuParam* mergeList[2], SaoCtuParam* codedParam, SaoEstimator & state, Ipp32s enableBandOffset, Ipp32s enableMergeMode, int ctbAddr)
{
    Ipp32s   m_numSaoModes = enableBandOffset ? 5 : 4;

    Ipp32s   m_bitDepth = state.m_bitDepth;
    Ipp32s   m_saoMaxOffsetQVal = state.m_saoMaxOffsetQVal;
    Ipp32f   m_labmda[NUM_SAO_COMPONENTS] = {state.m_labmda[0], state.m_labmda[1], state.m_labmda[2]};
    //Ipp64f   m_labmda[NUM_SAO_COMPONENTS] = {1.67, 1.67, 1.67};

    SaoCtuParam &bestParam = *codedParam;

    const Ipp32s shift = 2 * (m_bitDepth - 8);
    Ipp64f cost = 0.0;
    Ipp64s dist[NUM_SAO_COMPONENTS] = {0};
    Ipp64s modeDist[NUM_SAO_COMPONENTS] = {0};
    SaoOffsetParam testOffset[NUM_SAO_COMPONENTS];

    // reset before we start RDO
    bestParam[SAO_Y].mode_idx = SAO_MODE_OFF;
    bestParam[SAO_Y].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
    bestParam[SAO_Y].startBand_idx = 0;//will be rewritten in case of BO
    bestParam[SAO_Y].type_idx = 0;
    {
        bestParam[SAO_Y].offset[0] = 0;
        bestParam[SAO_Y].offset[1] = 0;
        bestParam[SAO_Y].offset[2] = 0;
        bestParam[SAO_Y].offset[3] = 0;
        bestParam[SAO_Y].offset[4] = 0;
    }


    Ipp32s bitCostMerge = CodeSaoCtbParam_BitCost(bestParam, sliceEnabled, mergeList[0] != NULL, mergeList[1] != NULL, true);

    Ipp32s bitCost = 0;

    bitCost += CodeSaoCtbOffsetParam_BitCost(SAO_Y, bestParam[SAO_Y], sliceEnabled[SAO_Y]);

    Ipp32f minCost = m_labmda[SAO_Y] * (bitCost + bitCostMerge);

    if (sliceEnabled[SAO_Y]) {
        for (Ipp32s type_idx = 0; type_idx < m_numSaoModes; type_idx++) {
        //for (Ipp32s type_idx = 4; type_idx < 5; type_idx++) {
            testOffset[SAO_Y].mode_idx = SAO_MODE_ON;
            testOffset[SAO_Y].type_idx = type_idx;
            testOffset[SAO_Y].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

            GetQuantOffsets(type_idx, state.m_statData[SAO_Y][type_idx], testOffset[SAO_Y].offset,
                testOffset[SAO_Y].startBand_idx, m_labmda[SAO_Y], m_bitDepth, m_saoMaxOffsetQVal, shift);

            dist[SAO_Y] = GetDistortion(testOffset[SAO_Y].type_idx, testOffset[SAO_Y].startBand_idx,
                testOffset[SAO_Y].offset, state.m_statData[SAO_Y][type_idx], shift);

            bitCost = CodeSaoCtbOffsetParam_BitCost(SAO_Y, testOffset[SAO_Y], sliceEnabled[SAO_Y]);

            cost = dist[SAO_Y] + m_labmda[SAO_Y]*(bitCost+bitCostMerge);
            if (cost < minCost) {
                minCost = cost;
                modeDist[SAO_Y] = dist[SAO_Y];
                memcpy(&bestParam[SAO_Y], testOffset + SAO_Y, sizeof(SaoOffsetParam));
            }
        }
    }

    Ipp32f chromaLambda = m_labmda[SAO_Cb];
    if ( sliceEnabled[SAO_Cb] ) {
        //VM_ASSERT(m_labmda[SAO_Cb] == m_labmda[SAO_Cr]);
        //VM_ASSERT(sliceEnabled[SAO_Cb] == sliceEnabled[SAO_Cr]);

        bestParam[SAO_Cb].mode_idx = SAO_MODE_OFF;
        bestParam[SAO_Cb].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
        bitCost = CodeSaoCtbOffsetParam_BitCost(SAO_Cb, bestParam[SAO_Cb], sliceEnabled[SAO_Cb]);

        bestParam[SAO_Cr].mode_idx = SAO_MODE_OFF;
        bestParam[SAO_Cr].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
        bitCost += CodeSaoCtbOffsetParam_BitCost(SAO_Cr, bestParam[SAO_Cr], sliceEnabled[SAO_Cr]);

        minCost= chromaLambda * bitCost;
        for (Ipp32s type_idx = 0; type_idx < m_numSaoModes; type_idx++) {
            for (Ipp32s compIdx = SAO_Cb; compIdx < NUM_SAO_COMPONENTS; compIdx++) {
                testOffset[compIdx].mode_idx = SAO_MODE_ON;
                testOffset[compIdx].type_idx = type_idx;
                testOffset[compIdx].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

                GetQuantOffsets(type_idx, state.m_statData[compIdx][type_idx], testOffset[compIdx].offset,
                    testOffset[compIdx].startBand_idx, m_labmda[compIdx], m_bitDepth, m_saoMaxOffsetQVal, shift);

                dist[compIdx]= GetDistortion(type_idx, testOffset[compIdx].startBand_idx,
                    testOffset[compIdx].offset, state.m_statData[compIdx][type_idx], shift);
            }

            bitCost = CodeSaoCtbOffsetParam_BitCost(SAO_Cb, testOffset[SAO_Cb], sliceEnabled[SAO_Cb]);
            bitCost += CodeSaoCtbOffsetParam_BitCost(SAO_Cr, testOffset[SAO_Cr], sliceEnabled[SAO_Cr]);

            cost = (dist[SAO_Cb]+ dist[SAO_Cr]) + chromaLambda*bitCost;
            if (cost < minCost) {
                minCost = cost;
                modeDist[SAO_Cb] = dist[SAO_Cb];
                modeDist[SAO_Cr] = dist[SAO_Cr];
                memcpy(&bestParam[SAO_Cb], testOffset + SAO_Cb, 2*sizeof(SaoOffsetParam));
            }
        }
    }


    if (enableMergeMode) {
        bitCost = CodeSaoCtbParam_BitCost(bestParam, sliceEnabled, (mergeList[0]!= NULL), (mergeList[1]!= NULL), false);

        // we use the same lambda for YUV

        //Ipp64f bestCost = modeDist[SAO_Y] / m_labmda[SAO_Y] + (modeDist[SAO_Cb]+ modeDist[SAO_Cr])/chromaLambda + bitCost;
        Ipp64f bestCost = (modeDist[SAO_Y]  + modeDist[SAO_Cb]+ modeDist[SAO_Cr]) + bitCost * m_labmda[0];

        /*if (ctbAddr == 19) {
            printf("\n stop \n");
        }*/

        int compCount = sliceEnabled[SAO_Cb] ? 3 : 1;
        SaoCtuParam testParam;
        for (Ipp32s mode = 0; mode < 2; mode++) {
            if (NULL == mergeList[mode])
                continue;

            memcpy(&testParam, mergeList[mode], sizeof(SaoCtuParam));

            Ipp64s distortion=0;
            for (int compIdx = 0; compIdx < compCount; compIdx++) {
                testParam[compIdx].mode_idx = mode == 0 ? SAO_MODE_MERGE_LEFT : SAO_MODE_MERGE_ABOVE;

                SaoOffsetParam& mergedOffset = (*(mergeList[mode]))[compIdx];

                if ( SAO_MODE_OFF != mergedOffset.mode_idx ) {
                    distortion += GetDistortion( mergedOffset.type_idx, mergedOffset.startBand_idx,
                        mergedOffset.offset, state.m_statData[compIdx][mergedOffset.type_idx], shift);
                }
            }

            bitCost = CodeSaoCtbParam_BitCost(testParam, sliceEnabled, (NULL != mergeList[0]), (NULL != mergeList[1]), false);

            cost = distortion + bitCost * m_labmda[0];
            if (cost < bestCost) {
                bestCost = cost;
                memcpy(&bestParam, &testParam, sizeof(SaoCtuParam));
            }
        }
    }

}

template <typename PixType>
void h265_SplitChromaCtb( PixType* in, Ipp32s inPitch, PixType* out1, Ipp32s outPitch1, PixType* out2, Ipp32s outPitch2, Ipp32s width, Ipp32s height)
{
    PixType* src = in;
    PixType* dst1 = out1;
    PixType* dst2 = out2;

    for (Ipp32s h = 0; h < height; h++) {
        for (Ipp32s w = 0; w < width; w++) {
            dst1[w] = src[2*w];
            dst2[w] = src[2*w+1];
        }
        src += inPitch;
        dst1 += outPitch1;
        dst2 += outPitch2;
    }
}

    enum SAOType
    {
        SAO_EO_0 = 0,
        SAO_EO_1,
        SAO_EO_2,
        SAO_EO_3,
        SAO_BO,
        MAX_NUM_SAO_TYPE
    };

    /** get the sign of input variable
    * \param   x
    */
    inline Ipp32s getSign(Ipp32s x)
    {
        return ((x >> 31) | ((Ipp32s)( (((Ipp32u) -x)) >> 31)));
    }

    const int   g_skipLinesR[3] = {1, 1, 1};//YCbCr
    const int   g_skipLinesB[3] = {1, 1, 1};//YCbCr

// --------------------------------------------------------
// crude stats calculation due to simplification boundary limitations
// --------------------------------------------------------

void h265_GetCtuStatistics_8u_asis(int compIdx, const Ipp8u* recBlk, int recStride, const Ipp8u* orgBlk, int orgStride, int width,
        int height, int shift,  CTBBorders& borders, int numSaoModes, SaoCtuStatistics* statsDataTypes)
    {
        Ipp16s signLineBuf1[64+1];
        Ipp16s signLineBuf2[64+1];

        int x, y, startX, startY, endX, endY;
        int firstLineStartX, firstLineEndX;
        int edgeType;
        Ipp16s signLeft, signRight, signDown;
        Ipp32s *diff, *count;

        //const int compIdx = SAO_Y;
        int skipLinesR = g_skipLinesR[compIdx];
        int skipLinesB = g_skipLinesB[compIdx];

        const PixType *recLine, *orgLine;
        const PixType* recLineAbove;
        const PixType* recLineBelow;


        for(int typeIdx=0; typeIdx< numSaoModes; typeIdx++)
        {
            SaoCtuStatistics& statsData= statsDataTypes[typeIdx];
            statsData.Reset();

            recLine = recBlk;
            orgLine = orgBlk;
            diff    = statsData.diff;
            count   = statsData.count;
            switch(typeIdx)
            {
            case SAO_EO_0://SAO_TYPE_EO_0:
                {
                    diff +=2;
                    count+=2;
                    endY   = height - skipLinesB;

                    startX = borders.m_left ? 0 : 1;
                    endX   = width - skipLinesR;

                    for (y=0; y<endY; y++)
                    {
                        signLeft = getSign(recLine[startX] - recLine[startX-1]);
                        for (x=startX; x<endX; x++)
                        {
                            signRight =  getSign(recLine[x] - recLine[x+1]);
                            edgeType  =  signRight + signLeft;
                            signLeft  = -signRight;

                            diff [edgeType] += (orgLine[x << shift] - recLine[x]);
                            count[edgeType] ++;
                        }
                        recLine  += recStride;
                        orgLine  += orgStride;
                    }
                }
                break;

            case SAO_EO_1: //SAO_TYPE_EO_90:
                {
                    diff +=2;
                    count+=2;
                    endX   = width - skipLinesR;

                    startY = borders.m_top ? 0 : 1;
                    endY   = height - skipLinesB;
                    if ( 0 == borders.m_top )
                    {
                        recLine += recStride;
                        orgLine += orgStride;
                    }

                    recLineAbove = recLine - recStride;
                    Ipp16s *signUpLine = signLineBuf1;
                    for (x=0; x< endX; x++)
                    {
                        signUpLine[x] = getSign(recLine[x] - recLineAbove[x]);
                    }

                    for (y=startY; y<endY; y++)
                    {
                        recLineBelow = recLine + recStride;

                        for (x=0; x<endX; x++)
                        {
                            signDown  = getSign(recLine[x] - recLineBelow[x]);
                            edgeType  = signDown + signUpLine[x];
                            signUpLine[x]= -signDown;

                            diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                            count[edgeType] ++;
                        }
                        recLine += recStride;
                        orgLine += orgStride;
                    }
                }
                break;

            case SAO_EO_2: //SAO_TYPE_EO_135:
                {
                    diff +=2;
                    count+=2;
                    startX = borders.m_left ? 0 : 1 ;
                    endX   = width - skipLinesR;
                    endY   = height - skipLinesB;

                    //prepare 2nd line's upper sign
                    Ipp16s *signUpLine, *signDownLine, *signTmpLine;
                    signUpLine  = signLineBuf1;
                    signDownLine= signLineBuf2;
                    recLineBelow = recLine + recStride;
                    for (x=startX; x<endX+1; x++)
                    {
                        signUpLine[x] = getSign(recLineBelow[x] - recLine[x-1]);
                    }

                    //1st line
                    recLineAbove = recLine - recStride;
                    firstLineStartX = borders.m_top_left ? 0 : 1;
                    firstLineEndX   = borders.m_top      ? endX : 1;

                    for(x=firstLineStartX; x<firstLineEndX; x++)
                    {
                        edgeType = getSign(recLine[x] - recLineAbove[x-1]) - signUpLine[x+1];
                        diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                        count[edgeType] ++;
                    }
                    recLine  += recStride;
                    orgLine  += orgStride;


                    //middle lines
                    for (y=1; y<endY; y++)
                    {
                        recLineBelow = recLine + recStride;

                        for (x=startX; x<endX; x++)
                        {
                            signDown = getSign(recLine[x] - recLineBelow[x+1]);
                            edgeType = signDown + signUpLine[x];
                            diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                            count[edgeType] ++;

                            signDownLine[x+1] = -signDown;
                        }
                        signDownLine[startX] = getSign(recLineBelow[startX] - recLine[startX-1]);

                        signTmpLine  = signUpLine;
                        signUpLine   = signDownLine;
                        signDownLine = signTmpLine;

                        recLine += recStride;
                        orgLine += orgStride;
                    }
                }
                break;

            case SAO_EO_3: //SAO_TYPE_EO_45:
                {
                    diff +=2;
                    count+=2;
                    endY   = height - skipLinesB;

                    startX = borders.m_left ? 0 : 1;
                    endX   = width - skipLinesR;

                    //prepare 2nd line upper sign
                    recLineBelow = recLine + recStride;
                    Ipp16s *signUpLine = signLineBuf1+1;
                    for (x=startX-1; x<endX; x++)
                    {
                        signUpLine[x] = getSign(recLineBelow[x] - recLine[x+1]);
                    }

                    //first line
                    recLineAbove = recLine - recStride;

                    firstLineStartX = borders.m_top ? startX : endX;
                    firstLineEndX   = endX;

                    for(x=firstLineStartX; x<firstLineEndX; x++)
                    {
                        edgeType = getSign(recLine[x] - recLineAbove[x+1]) - signUpLine[x-1];
                        diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                        count[edgeType] ++;
                    }

                    recLine += recStride;
                    orgLine += orgStride;

                    //middle lines
                    for (y=1; y<endY; y++)
                    {
                        recLineBelow = recLine + recStride;

                        for(x=startX; x<endX; x++)
                        {
                            signDown = getSign(recLine[x] - recLineBelow[x-1]) ;
                            edgeType = signDown + signUpLine[x];

                            diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                            count[edgeType] ++;

                            signUpLine[x-1] = -signDown;
                        }
                        signUpLine[endX-1] = getSign(recLineBelow[endX-1] - recLine[endX]);
                        recLine  += recStride;
                        orgLine  += orgStride;
                    }
                }
                break;

            case SAO_BO: //SAO_TYPE_BO:
                {
                    endX = width- skipLinesR;
                    endY = height- skipLinesB;
                    const int shiftBits = 3;
                    for (y=0; y<endY; y++)
                    {
                        for (x=0; x<endX; x++)
                        {
                            int bandIdx= recLine[x] >> shiftBits;
                            diff [bandIdx] += (orgLine[x<<shift] - recLine[x]);
                            count[bandIdx]++;
                        }
                        recLine += recStride;
                        orgLine += orgStride;
                    }
                }
                break;

            default:
                {
                    //VM_ASSERT(!"Not a supported SAO types\n");
                }
            }
        }

    } // void GetCtuStatistics_Internal(...)


// --------------------------------------------------------
// take into consideration boundary limitations (usefull to support tile/slice)
// --------------------------------------------------------

void h265_GetCtuStatistics_8u_accurate(
    int compIdx,
    const Ipp8u* recBlk, int recStride,
    const Ipp8u* orgBlk, int orgStride,
    int width, int height, int shift,
    CTBBorders& borders,
    int numSaoModes,
    SaoCtuStatistics* statsDataTypes)
    {
        Ipp16s signLineBuf1[64+1];

        int x, y, startX, startY, endX, endY;
        int edgeType;
        Ipp16s signLeft, signRight, signDown;
        Ipp32s *diff, *count;

        const PixType *recLine, *orgLine;
        const PixType* recLineAbove;
        const PixType* recLineBelow;

        for(int typeIdx=0; typeIdx< numSaoModes; typeIdx++)
        {
            SaoCtuStatistics& statsData= statsDataTypes[typeIdx];
            statsData.Reset();

            recLine = recBlk;
            orgLine = orgBlk;
            diff    = statsData.diff;
            count   = statsData.count;
            switch(typeIdx)
            {
            case SAO_EO_0://SAO_TYPE_EO_0:
                {
                    diff +=2;
                    count+=2;
                    endY   = height;

                    startX = borders.m_left ? 0 : 1;
                    endX   = width - (borders.m_right ? 0 : 1);//skipLinesR;

                    for (y=0; y<endY; y++)
                    {
                        signLeft = getSign(recLine[startX] - recLine[startX-1]);
                        for (x=startX; x<endX; x++)
                        {
                            signRight =  getSign(recLine[x] - recLine[x+1]);
                            edgeType  =  signRight + signLeft;
                            signLeft  = -signRight;

                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;
                        }
                        recLine  += recStride;
                        orgLine  += orgStride;
                    }
                }
                break;

            case SAO_EO_1: //SAO_TYPE_EO_90:
                {
                    diff +=2;
                    count+=2;
                    endX   = width;// - skipLinesR;

                    // to compare with DEFAULT
                    /*{
                        endX   = width - 1;
                    }*/

                    startY = borders.m_top ? 0 : 1;
                    endY   = height - (borders.m_bottom ? 0 : 1);//skipLinesB;
                    if ( 0 == borders.m_top )
                    {
                        recLine += recStride;
                        orgLine += orgStride;
                    }

                    recLineAbove = recLine - recStride;
                    Ipp16s *signUpLine = signLineBuf1;
                    for (x=0; x< endX; x++)
                    {
                        signUpLine[x] = getSign(recLine[x] - recLineAbove[x]);
                    }

                    for (y=startY; y<endY; y++)
                    {
                        recLineBelow = recLine + recStride;

                        for (x=0; x<endX; x++)
                        {
                            signDown  = getSign(recLine[x] - recLineBelow[x]);
                            edgeType  = signDown + signUpLine[x];
                            signUpLine[x]= -signDown;

                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;
                        }
                        recLine += recStride;
                        orgLine += orgStride;
                    }
                }
                break;


            case SAO_EO_2: //SAO_TYPE_EO_135:
                {
                    diff +=2;
                    count+=2;
                    startX = 0;
                    endX   = width;
                    endY   = height;

                    //first line
                    if (borders.m_top)
                    {
                        startX = borders.m_top_left ? 0 : 1;
                        endX = borders.m_right ? width : width - 1;

                        recLineAbove = recLine - recStride;
                        recLineBelow = recLine + recStride;
                        for (x=startX; x<endX; x++)
                        {
                            int signUp = getSign(recLine[x] - recLineAbove[x-1]);
                            signDown = getSign(recLine[x] - recLineBelow[x+1]);
                            edgeType = signDown + signUp;
                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;
                        }
                    }

                    // midd lines
                    recLine += recStride;
                    orgLine += orgStride;

                    startX = borders.m_left ? 0 : 1;
                    endX = borders.m_right ? width : width - 1;

                    for (y=1; y < height-1; y++)
                    {
                        recLineAbove = recLine - recStride;
                        recLineBelow = recLine + recStride;

                        for (x=startX; x<endX; x++)
                        {
                            int signUp = getSign(recLine[x] - recLineAbove[x-1]);
                            signDown = getSign(recLine[x] - recLineBelow[x+1]);
                            edgeType = signDown + signUp;
                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;
                        }

                        recLine += recStride;
                        orgLine += orgStride;
                    }

                    //last line
                    if (borders.m_bottom) {
                        startX = borders.m_left ? 0 : 1;
                        endX = borders.m_bottom_right ? width : width - 1;
                        recLineAbove = recLine - recStride;
                        recLineBelow = recLine + recStride;
                        for (x=startX; x<endX; x++)
                        {
                            int signUp = getSign(recLine[x] - recLineAbove[x-1]);
                            signDown = getSign(recLine[x] - recLineBelow[x+1]);
                            edgeType = signDown + signUp;
                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;
                        }
                    }
                }
                break;


            case SAO_EO_3: //SAO_TYPE_EO_45:
                {
                    diff +=2;
                    count+=2;

                    // first line
                    if (borders.m_top) {
                        startX = borders.m_left ? 0 : 1;
                        endX   = borders.m_top_right ? width : width -1;
                        recLineAbove = recLine - recStride;
                        recLineBelow = recLine + recStride;
                        for (x=startX; x<endX; x++)
                        {
                            int signUp = getSign(recLine[x] - recLineAbove[x+1]);
                            signDown = getSign(recLine[x] - recLineBelow[x-1]);
                            edgeType = signDown + signUp;
                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;
                        }
                    }

                    // midd lines
                    recLine += recStride;
                    orgLine += orgStride;

                    startX = borders.m_left ? 0 : 1;
                    endX = borders.m_right ? width : width - 1;

                    for (y=1; y < height-1; y++)
                    {
                        recLineAbove = recLine - recStride;
                        recLineBelow = recLine + recStride;

                        for (x=startX; x<endX; x++)
                        {
                            int signUp = getSign(recLine[x] - recLineAbove[x+1]);
                            signDown = getSign(recLine[x] - recLineBelow[x-1]);
                            edgeType = signDown + signUp;
                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;
                        }

                        recLine += recStride;
                        orgLine += orgStride;
                    }

                    //last line
                    if (borders.m_bottom) {
                        startX = borders.m_bottom_left ? 0 : 1;
                        endX = borders.m_bottom_right ? width : width - 1;
                        recLineAbove = recLine - recStride;
                        recLineBelow = recLine + recStride;
                        for (x=startX; x<endX; x++)
                        {
                            int signUp = getSign(recLine[x] - recLineAbove[x+1]);
                            signDown = getSign(recLine[x] - recLineBelow[x-1]);
                            edgeType = signDown + signUp;
                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;
                        }
                    }
                }
                break;

            case SAO_BO: //SAO_TYPE_BO:
                {
                    endX = width;//- skipLinesR;
                    endY = height;//- skipLinesB;

                    // TO CMP WITH DEF
                    /*{
                        endX--;
                        endY--;
                    }*/

                    const int shiftBits = 3;
                    for (y=0; y<endY; y++)
                    {
                        for (x=0; x<endX; x++)
                        {
                            int bandIdx= recLine[x] >> shiftBits;
                            diff [bandIdx] += (orgLine[x] - recLine[x]);
                            count[bandIdx]++;
                        }
                        recLine += recStride;
                        orgLine += orgStride;
                    }
                }
                break;

            default:
                {
                    //VM_ASSERT(!"Not a supported SAO types\n");
                }
            }
        }

    } // void GetCtuStatistics_Internal(...)

// --------------------------------------------------------
// fast version means : not to take into consediration boundaries limitation
// recon should be padded +1 (LR TB) with zero
// --------------------------------------------------------
void h265_GetCtuStatistics_8u_fast(
    int compIdx,
    const Ipp8u* recBlk, int recStride,
    const Ipp8u* orgBlk, int orgStride,
    int width, int height, int shift,
    CTBBorders& borders,
    int numSaoModes,
    SaoCtuStatistics* statsDataTypes,
    int ctbAddr)
    {
        int x, y, startX, endX, endY;
        int edgeType;
        Ipp16s signLeft, signRight, signDown;
        Ipp32s *diff, *count;

        const PixType *recLine, *orgLine;
        const PixType* recLineAbove;
        const PixType* recLineBelow;


        for(int typeIdx=0; typeIdx< numSaoModes; typeIdx++)
        {
            SaoCtuStatistics& statsData= statsDataTypes[typeIdx];
            statsData.Reset();

            recLine = recBlk;
            orgLine = orgBlk;
            diff    = statsData.diff;
            count   = statsData.count;
            switch(typeIdx)
            {
#if 1
            case SAO_EO_0://SAO_TYPE_EO_0:
                {
                    diff +=2;
                    count+=2;
                    endY   = height;

                    startX = 0;//borders.m_left ? 0 : 1;
                    endX   = width;// - (borders.m_right ? 0 : 1);//skipLinesR;

                    for (y=0; y<endY; y++)
                    {
                        signLeft = getSign(recLine[startX] - recLine[startX-1]);
                        for (x=startX; x<endX; x++)
                        {
                            signRight =  getSign(recLine[x] - recLine[x+1]);
                            edgeType  =  signRight + signLeft;
                            signLeft  = -signRight;

                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;
                        }
                        recLine  += recStride;
                        orgLine  += orgStride;
                    }
                }
                // DEBUG!!!
                //return;
                break;

            case SAO_EO_1: //SAO_TYPE_EO_90:
                {
                    int signUp;
                    diff +=2;
                    count+=2;
#if 0
                    for (y=0; y<height; y++)
                    {
                        recLineAbove = recLine - recStride;
                        recLineBelow = recLine + recStride;

                        for (x=0; x<width; x++)
                        {
                            signDown  = getSign(recLine[x] - recLineBelow[x]);
                            signUp    = getSign(recLine[x] - recLineAbove[x]);
                            edgeType  = signDown + signUp;

                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;
                        }
                        recLine += recStride;
                        orgLine += orgStride;
                    }
#else
                    const int stepY = 8;
                    const int stepX = 16;
                    int blk = 0;
                    int diff_8x16[5] = {0};
                    int mat_origin[8][16];
                    for (int gy=0; gy<height; gy += stepY)
                    {
                        for (int gx=0; gx<width; gx += stepX) {
                            diff_8x16[0] = diff_8x16[1] = diff_8x16[3] = diff_8x16[4] = 0;

                            for(y = 0; y < stepY; y++) {
                                recLine = recBlk + (gy+y) * recStride;
                                orgLine = orgBlk + (gy+y) * orgStride;
                                recLineAbove = recLine - recStride;
                                recLineBelow = recLine + recStride;

                                for(x = 0; x < stepX; x++) {

                                    signDown  = getSign(recLine[gx + x] - recLineBelow[gx + x]);
                                    signUp    = getSign(recLine[gx + x] - recLineAbove[gx + x]);
                                    edgeType  = signDown + signUp;

                                    diff [edgeType] += (orgLine[gx+x] - recLine[gx+x]);
                                    count[edgeType] ++;

                                    diff_8x16[2 + edgeType] += (orgLine[gx+x] - recLine[gx+x]);
                                    mat_origin[y][x] = orgLine[gx+x];
                                }
                                //recLine += recStride;
                                //orgLine += orgStride;
                            }

                            /*if (ctbAddr == 2 && blk < 5) {
                                printf("\n CPU blk %i E90_diff %i, %i, %i, %i \n", blk, diff_8x16[0], diff_8x16[1], diff_8x16[3], diff_8x16[4]);

                                if (blk == 2) {
                                    for (int y1 = 0; y1 < 8; y1++) {
                                        for(int x1 = 0; x1 < 16; x1++) {
                                            printf(" %i ", mat_origin[y1][x1]);
                                        }
                                        printf("\n");
                                    }
                                }
                            }
                            blk++;*/
                        }
                    }
#endif
                }
                break;
#endif

#if 1
            case SAO_EO_2: //SAO_TYPE_EO_135:
                {
                    diff +=2;
                    count+=2;
                    startX = 0;
                    endX   = width;
                    endY   = height;

                    for (y=0; y<endY; y++)
                    {
                        recLineAbove = recLine - recStride;
                        recLineBelow = recLine + recStride;

                        for (x=startX; x<endX; x++)
                        {
                            int signUp = getSign(recLine[x] - recLineAbove[x-1]);
                            signDown = getSign(recLine[x] - recLineBelow[x+1]);
                            edgeType = signDown + signUp;
                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;
                        }

                        recLine += recStride;
                        orgLine += orgStride;
                    }
                }
                break;

            case SAO_EO_3: //SAO_TYPE_EO_45:
                {
                    diff +=2;
                    count+=2;
                    endY   = height;

                    startX = 0;
                    endX   = width;

                    for (y=0; y<endY; y++)
                    {
                        recLineBelow = recLine + recStride;
                        recLineAbove = recLine - recStride;

                        for (x = startX; x<endX; x++)
                        {
                            signDown = getSign(recLine[x] - recLineBelow[x-1]) ;
                            int signUp = getSign(recLine[x] - recLineAbove[x+1]) ;
                            edgeType = signDown + signUp;

                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;
                        }
                        recLine  += recStride;
                        orgLine  += orgStride;
                    }
                }
                break;
#endif

#if 1
            case SAO_BO: //SAO_TYPE_BO:
                {
                    endX = width;//- skipLinesR;
                    endY = height;//- skipLinesB;

                    // TO CMP WITH DEF
                    /*{
                        endX--;
                        endY--;
                    }*/

                    const int shiftBits = 3;
                    for (y=0; y<endY; y++)
                    {
                        for (x=0; x<endX; x++)
                        {
                            int bandIdx= recLine[x] >> shiftBits;
                            diff [bandIdx] += (orgLine[x] - recLine[x]);
                            count[bandIdx]++;
                        }
                        recLine += recStride;
                        orgLine += orgStride;
                    }
                }
                break;
#endif

            default:
                {
                    //VM_ASSERT(!"Not a supported SAO types\n");
                }
            }
        }

    }
// --------------------------------------------------------

void EstimateSao(const Ipp8u* frameOrigin, int pitchOrigin, Ipp8u* frameRecon, int pitchRecon, const VideoParam* m_par, const AddrInfo* frame_addr_info, SaoCtuParam* frame_sao_param,
                 Ipp32s* diffEO, Ipp16u* countEO, Ipp32s* diffBO, Ipp16u* countBO,
                 Ipp8u* availLeft, Ipp8u* availAbove)
{
    SaoEstimator m_saoEst;

    m_saoEst.m_bitDepth = 8;//m_par->bitDepthLuma;// need to fix in case of chroma != luma bitDepth
    m_saoEst.m_saoMaxOffsetQVal = (1<<(MIN(m_saoEst.m_bitDepth,10)-5))-1;
    int saoOpt = 1;
#ifndef AMT_SAO_MIN
    m_saoEst.m_numSaoModes = (saoOpt == SAO_OPT_ALL_MODES) ? NUM_SAO_BASE_TYPES : NUM_SAO_BASE_TYPES - 1;
#else
    m_saoEst.m_numSaoModes = (saoOpt == SAO_OPT_ALL_MODES) ? NUM_SAO_BASE_TYPES : ((m_par->saoOpt == SAO_OPT_FAST_MODES_ONLY)? NUM_SAO_BASE_TYPES - 1: 1);
#endif

    int numCtbs = m_par->PicWidthInCtbs * m_par->PicHeightInCtbs;

    PixType* reconY  = frameRecon;
    PixType* originY = (PixType*)frameOrigin;
    Ipp32s pitchRecLuma = pitchRecon;
    Ipp32s pitchSrcLuma = pitchOrigin;

    const PixType* reconUV  = frameRecon + m_par->Height * pitchRecon;
    const PixType* originUV = frameOrigin + m_par->Height * pitchOrigin;

    int chromaSize = m_par->Width * m_par->Height >> 2;
    std::vector<PixType> reconPlaneU(chromaSize);
    std::vector<PixType> reconPlaneV(chromaSize);
    std::vector<PixType> originPlaneU(chromaSize);
    std::vector<PixType> originPlaneV(chromaSize);
    int planePitchChroma = m_par->Width >> 1;

    Ipp32s pitchRecChroma = pitchRecon;
    Ipp32s pitchSrcChroma = pitchOrigin;

    h265_SplitChromaCtb_px( (PixType*)reconUV,  pitchRecChroma, reconPlaneU.data(),  planePitchChroma, reconPlaneV.data(),  planePitchChroma, m_par->Width >> 1, m_par->Height >> 1);
    h265_SplitChromaCtb_px( (PixType*)originUV, pitchSrcChroma, originPlaneU.data(), planePitchChroma, originPlaneV.data(), planePitchChroma, m_par->Width >> 1, m_par->Height >> 1);
    // CPU kernel
    for (int ctbAddr = 0; ctbAddr < numCtbs; ctbAddr++)
    {
        Ipp32s chromaShiftW = (m_par->chromaFormatIdc != MFX_CHROMAFORMAT_YUV444);
        Ipp32s chromaShiftH = (m_par->chromaFormatIdc == MFX_CHROMAFORMAT_YUV420);
        Ipp32s chromaShiftWInv = 1 - chromaShiftW;

        Ipp32s ctbPelX = (ctbAddr % m_par->PicWidthInCtbs) * m_par->MaxCUSize;
        Ipp32s ctbPelY = (ctbAddr / m_par->PicWidthInCtbs) * m_par->MaxCUSize;
        Ipp32s ctbX = (ctbAddr % m_par->PicWidthInCtbs);
        Ipp32s ctbY = (ctbAddr / m_par->PicWidthInCtbs);

        PixType* m_yRec = (PixType*)reconY + ctbPelX + ctbPelY * pitchRecLuma;
        PixType* m_ySrc = (PixType*)originY + ctbPelX + ctbPelY * pitchSrcLuma;

        const AddrInfo& info = frame_addr_info[ctbAddr];
        int m_leftAddr  = info.left.ctbAddr;
        int m_aboveAddr = info.above.ctbAddr;
        int m_leftSameTile = info.left.sameTile;
        int m_aboveSameTile = info.above.sameTile;
        int m_leftSameSlice = info.left.sameSlice;
        int m_aboveSameSlice = info.above.sameSlice;

        CTBBorders borders = {0};
        borders.m_left = (-1 == m_leftAddr)  ? 0 : (m_leftSameSlice  && m_leftSameTile) ? 1 : 0;
        borders.m_top  = (-1 == m_aboveAddr) ? 0 : (m_aboveSameSlice && m_aboveSameTile) ? 1 : 0;

        // workaround (better to rewrite optimized SAO estimate functions to use tmpU and tmpL)
        borders.m_left = 0;
        borders.m_top  = 0;

        // aya: in this section we set real restriction for current ctb
        if(1)
        {
            Ipp32s ctbAddrEnd = m_par->PicWidthInCtbs * m_par->PicHeightInCtbs;
            borders.m_left = (ctbAddr % m_par->PicWidthInCtbs) > 0 ? 1 : 0;
            borders.m_top  = (ctbAddr >= m_par->PicWidthInCtbs) ? 1 : 0;
            borders.m_top_left = borders.m_top & borders.m_left;
            borders.m_right = ((ctbAddr % m_par->PicWidthInCtbs) < (m_par->PicWidthInCtbs - 1)) ? 1 : 0;
            borders.m_top_right = borders.m_top & borders.m_right;
            borders.m_bottom = (ctbAddr < (ctbAddrEnd - m_par->PicWidthInCtbs)) ? 1 : 0;
            borders.m_bottom_left = borders.m_bottom & borders.m_left;
            borders.m_bottom_right = borders.m_bottom & borders.m_right;
        }

        SaoCtuParam tmpMergeParam;
        SaoCtuParam* mergeList[2] = {NULL, NULL};
        //if (borders.m_top) 
        if (availAbove[ctbY])
        { 
            mergeList[1] = frame_sao_param + ctbAddr - m_par->PicWidthInCtbs;
        }
        //if (borders.m_left) 
        if (availLeft[ctbX])
        {
            mergeList[0] = frame_sao_param + ctbAddr - 1;
        }

        Ipp32s height = (ctbPelY + m_par->MaxCUSize > m_par->Height) ? (m_par->Height- ctbPelY) : m_par->MaxCUSize;
        Ipp32s width  = (ctbPelX + m_par->MaxCUSize  > m_par->Width) ? (m_par->Width - ctbPelX) : m_par->MaxCUSize;

        Ipp32s offset = 0;// YV12/NV12 Chroma dispatcher
        //h265_GetCtuStatistics_8u_fast(SAO_Y, (PixType*)m_yRec, pitchRecLuma, (PixType*)m_ySrc, pitchSrcLuma, width, height, offset, borders, m_saoEst.m_numSaoModes, m_saoEst.m_statData[SAO_Y], ctbAddr);
        h265_GetCtuStatistics_8u_accurate(SAO_Y, (PixType*)m_yRec, pitchRecLuma, (PixType*)m_ySrc, pitchSrcLuma, width, height, offset, borders, m_saoEst.m_numSaoModes, m_saoEst.m_statData[SAO_Y]);

        //FLUSH STATS
        {
            for(int idx = 0; idx <4; idx++) {
                //memcpy( (Ipp8u*)diffEO + (ctbAddr*4*4*5) + idx*(4*5), (Ipp8u*)m_saoEst.m_statData[SAO_Y][idx].diff, 4*5);
                //memcpy( (Ipp8u*)countEO + (ctbAddr*4*4*5) + idx*(4*5), (Ipp8u*)m_saoEst.m_statData[SAO_Y][idx].count, 4*5);
                for (Ipp32s i = 0; i < 5; i++) {
                    diffEO [ctbAddr*4*5+idx*5+i] = m_saoEst.m_statData[SAO_Y][idx].diff[i];
                    countEO[ctbAddr*4*5+idx*5+i] = m_saoEst.m_statData[SAO_Y][idx].count[i];
                }
            }
            //memcpy( (Ipp8u*)diffBO + (ctbAddr*4*32),  (Ipp8u*)m_saoEst.m_statData[SAO_Y][4].diff, 4*32);
            //memcpy( (Ipp8u*)countBO + (ctbAddr*4*32), (Ipp8u*)m_saoEst.m_statData[SAO_Y][4].count, 4*32);
            for (Ipp32s i = 0; i < 32; i++) {
                diffBO [ctbAddr*32+i] = m_saoEst.m_statData[SAO_Y][4].diff[i];
                countBO[ctbAddr*32+i] = m_saoEst.m_statData[SAO_Y][4].count[i];
            }
        }

        if (m_par->SAOChromaFlag) {
            width  >>= 1;
            height >>= 1;

            PixType* reconU  = reconPlaneU.data()  + (ctbPelX >> 1) + (ctbPelY >> 1) * planePitchChroma;
            PixType* reconV  = reconPlaneV.data()  + (ctbPelX >> 1) + (ctbPelY >> 1) * planePitchChroma;
            PixType* originU = originPlaneU.data() + (ctbPelX >> 1) + (ctbPelY >> 1) * planePitchChroma;
            PixType* originV = originPlaneV.data() + (ctbPelX >> 1) + (ctbPelY >> 1) * planePitchChroma;

            Ipp32s compIdx = 1;
            h265_GetCtuStatistics_8u_accurate(compIdx, reconU, planePitchChroma, originU, planePitchChroma, width, height, offset, borders, m_saoEst.m_numSaoModes, m_saoEst.m_statData[compIdx]);
            compIdx = 2;
            h265_GetCtuStatistics_8u_accurate(compIdx, reconV, planePitchChroma, originV, planePitchChroma, width, height, offset, borders, m_saoEst.m_numSaoModes, m_saoEst.m_statData[compIdx]);

            //FLUSH STATS
            for (compIdx = 1; compIdx <=2; compIdx++) {
                int startOffset = compIdx * m_par->PicWidthInCtbs * m_par->PicHeightInCtbs * (4 * 5);
                for(int idx = 0; idx <4; idx++) {
                    for (Ipp32s i = 0; i < 5; i++) {
                        diffEO [startOffset + ctbAddr*4*5+idx*5+i] = m_saoEst.m_statData[compIdx][idx].diff[i];
                        countEO[startOffset + ctbAddr*4*5+idx*5+i] = m_saoEst.m_statData[compIdx][idx].count[i];
                    }
                }
                startOffset = compIdx * m_par->PicWidthInCtbs * m_par->PicHeightInCtbs * 32;
                for (Ipp32s i = 0; i < 32; i++) {
                    diffBO [startOffset + ctbAddr*32+i] = m_saoEst.m_statData[compIdx][4].diff[i];
                    countBO[startOffset + ctbAddr*32+i] = m_saoEst.m_statData[compIdx][4].count[i];
                }
            }
        }


        m_saoEst.m_labmda[0] = m_par->m_rdLambda;
        m_saoEst.m_labmda[1] = m_saoEst.m_labmda[2] = m_saoEst.m_labmda[0];

        bool sliceEnabled[NUM_SAO_COMPONENTS] = {true, false, false};
        if (m_par->SAOChromaFlag)
            sliceEnabled[1] = sliceEnabled[2] = true;

        /*if (ctbAddr == 410) {
            printf ("\n stop \n");
        }*/
        Ipp32s enableMergeMode = 1;//m_par->enableBandOffset >> 1;
        Ipp32s enableBandOffset = m_par->enableBandOffset & 0x1;
        GetBestSao_BitCost(sliceEnabled, mergeList, &frame_sao_param[ctbAddr], m_saoEst, enableBandOffset, enableMergeMode, ctbAddr);

    } // foreach ctbAddr

}

void AccumulateStats(const VideoParam *m_par, const Ipp16s *blockStats, Ipp32s *diffEO, Ipp16u *countEO, Ipp32s *diffBO, Ipp16u *countBO, Ipp8u enableChroma)
{
    int numCtb = m_par->PicWidthInCtbs * m_par->PicHeightInCtbs;
    memset(diffEO, 0, numCtb * 16 * sizeof(*diffEO));
    memset(diffBO, 0, numCtb * 32 * sizeof(*diffBO));
    memset(countEO, 0, numCtb * 16 * sizeof(*countEO));
    memset(countBO, 0, numCtb * 32 * sizeof(*countBO));

    int MaxCUSize = m_par->MaxCUSize;
    Ipp32s blockW = GPU_STAT_BLOCK_WIDTH;
    Ipp32s blockH = GPU_STAT_BLOCK_HEIGHT;
    if (enableChroma) {
        MaxCUSize >>= 1;
        blockW = GPU_STAT_CHROMA_BLOCK_WIDTH;
        blockH = GPU_STAT_CHROMA_BLOCK_HEIGHT;
    }
    Ipp32s heightInBlocks = m_par->PicHeightInCtbs * MaxCUSize / blockH;
    Ipp32s widthInBlocks  = m_par->PicWidthInCtbs  * MaxCUSize / blockW;
    for (Ipp32s blky = 0; blky < heightInBlocks; blky++) {
        for (Ipp32s blkx = 0; blkx < widthInBlocks; blkx++) {
            Ipp32s blk = blkx + blky * widthInBlocks;
            Ipp32s ctb = blkx * blockW / MaxCUSize + blky  * blockH / MaxCUSize * m_par->PicWidthInCtbs;
            for (Ipp32s i = 0; i < 16; i++) {
                diffEO[16*ctb+i] += blockStats[(16+16+32+32)*blk+i];
                countEO[16*ctb+i] += blockStats[(16+16+32+32)*blk+16+i];
            }
            for (Ipp32s i = 0; i < 32; i++) {
                diffBO[32*ctb+i] += blockStats[(16+16+32+32)*blk+16+16+i];
                countBO[32*ctb+i] += blockStats[(16+16+32+32)*blk+16+16+32+i];
            }
        }
    }
}

void SimulateTiles(Ipp8u* availLeftAboveMask, Ipp32s numCtbX, Ipp32s numCtbY, Ipp32s percentX = 25, Ipp32s percentY = 10)
{
    Ipp32s maxNumTileCols = numCtbX * percentX / 100;
    Ipp32s maxNumTileRows = numCtbY * percentY / 100;

    Ipp32s NumTileCols = 4;//random(1, maxNumTileCols+1);
    Ipp32s NumTileRows = 3;//random(1, maxNumTileRows+1);
    std::vector<Ipp32s> vecTileColStart(NumTileCols);
    std::vector<Ipp32s> vecTileRowStart(NumTileRows);
    Ipp32s PicWidthInCtbs  = numCtbX;
    Ipp32s PicHeightInCtbs = numCtbY;
    
    memset(availLeftAboveMask, 1, numCtbX + numCtbY);

    Ipp8u* availLeft = availLeftAboveMask;
    Ipp32s tileColStart = 0;
    for (Ipp32s i = 0; i < NumTileCols; i++) {
        Ipp32s tileColWidth = ((i + 1) * PicWidthInCtbs) / NumTileCols - (i * PicWidthInCtbs / NumTileCols);

        vecTileColStart[i] = tileColStart;
        availLeft[tileColStart] = 0;

        tileColStart += tileColWidth;
    }

    Ipp8u* availAbove = availLeftAboveMask + numCtbX;
    Ipp32s tileRowStart = 0;
    for (Ipp32s i = 0; i < NumTileRows; i++) {
        Ipp32s tileRowHeight = ((i + 1) * PicHeightInCtbs) / NumTileRows - (i * PicHeightInCtbs / NumTileRows);

        vecTileRowStart[i] = tileRowStart;
        availAbove[tileRowStart] = 0;

        tileRowStart += tileRowHeight;
    }
}

int main()
{
    Ipp32s width = WIDTH;
    Ipp32s height = HEIGHT;
    VideoParam videoParam;
    SetVideoParam(videoParam, width, height);
    Ipp32s numCtbs = videoParam.PicHeightInCtbs * videoParam.PicWidthInCtbs;

    std::vector<Ipp8u> input(width * height * 3 / 2);
    std::vector<Ipp8u> recon(width * height * 3 / 2);
    std::vector<Ipp8u> outputGpu(width * height * 3 / 2);
    std::vector<Ipp8u> outputCpu(width * height * 3 / 2);
    std::vector<SaoCtuParam> saoModesCpu(numCtbs);
    std::vector<SaoCtuParam> saoModesGpu(numCtbs);
    std::vector<AddrInfo> frame_addr_info(numCtbs, AddrInfo());
    std::vector<Ipp32s> diffEoCpu(3*numCtbs*4*5, 0);
    std::vector<Ipp32s> diffBoCpu(3*numCtbs*1*32);
    std::vector<Ipp16u> countEoCpu(3*numCtbs*4*5);
    std::vector<Ipp16u> countBoCpu(3*numCtbs*1*32);

    std::vector<Ipp32s> diffEoGpu(3*numCtbs*4*4, 0);
    std::vector<Ipp32s> diffBoGpu(3*numCtbs*1*32, 0);
    std::vector<Ipp16u> countEoGpu(3*numCtbs*4*4, 0);
    std::vector<Ipp16u> countBoGpu(3*numCtbs*1*32, 0);
    Ipp32s numBlocks = videoParam.PicWidthInCtbs * videoParam.PicHeightInCtbs * videoParam.MaxCUSize * videoParam.MaxCUSize / GPU_STAT_BLOCK_WIDTH / GPU_STAT_BLOCK_HEIGHT;
    Ipp32s numBlocksChroma = numBlocks;
#if defined(CHROMA_SEPARATIST)
    numBlocksChroma = videoParam.PicWidthInCtbs * videoParam.PicHeightInCtbs * ((videoParam.MaxCUSize >> 1) / GPU_STAT_CHROMA_BLOCK_WIDTH) * ((videoParam.MaxCUSize >> 1) /  GPU_STAT_CHROMA_BLOCK_HEIGHT);
#endif
    std::vector<Ipp16s> blockStats(numBlocks * (16+16+32+32) + (2 * (numBlocksChroma * (16+16+32+32))));

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return FAILED;
    if (fread(input.data(), 1, input.size(), f) != input.size())
        return FAILED;
    fclose(f);

    SimulateRecon(input.data(), width, recon.data(), width, width, height, 10);

    // this params hard coded in genx kernel SaoEstimate()
    //Ipp32s maxWidth  = 3840;
    //Ipp32s maxHeight = 2160;
    Ipp32s maxWidth  = 8192;
    Ipp32s maxHeight = 4320;
    Ipp32s maxCuSize = 64;
    Ipp32s maxNumCtbX = (maxWidth  + (maxCuSize - 1)) / maxCuSize;
    Ipp32s maxNumCtbY = (maxHeight + (maxCuSize - 1)) / maxCuSize;

    std::vector<Ipp8u> availLeftAboveMask(maxNumCtbX + maxNumCtbY, 0);

    SimulateTiles(availLeftAboveMask.data(), maxNumCtbX, maxNumCtbY);
    memcpy(videoParam.availLeftAbove, availLeftAboveMask.data(), maxNumCtbX + maxNumCtbY);

    // send offset to correct ChromaBlockStats writing
    videoParam.offsetChroma = numBlocks * (16+16+32+32) * sizeof(Ipp16s);
    // DUMP
    //Dump(input, frameSize, "input.yuv");
    //Dump(recon, frameSize, "recon.yuv");

    Ipp32s res = 0;

    videoParam.enableBandOffset = 0;
    //videoParam.enableBandOffset |= (1<<1); // merge mode enabled (case for debug only)
    videoParam.SAOChromaFlag = 1;

    printf("Stat EO:       ");
    res = RunCpuStatAndEstimate(input.data(), recon.data(), &videoParam, frame_addr_info.data(), saoModesCpu.data(), diffEoCpu.data(), countEoCpu.data(), diffBoCpu.data(), countBoCpu.data(),
        videoParam.availLeftAbove, videoParam.availLeftAbove + maxNumCtbX);
    CHECK_ERR(res);
#if defined(CHROMA_SEPARATIST)
    res = RunGpuStat(input.data(), recon.data(), &videoParam, blockStats.data(), blockStats.size() * 2, false);
    CHECK_ERR(res);
    res = RunGpuStatChroma(input.data(), recon.data(), &videoParam, blockStats.data(), blockStats.size() * 2, false, 1);
#else
    res = RunGpuStat(input.data(), recon.data(), &videoParam, blockStats.data(), blockStats.size() * 2, false);
#endif
    CHECK_ERR(res);
    AccumulateStats(&videoParam, blockStats.data(), diffEoGpu.data(), countEoGpu.data(), diffBoGpu.data(), countBoGpu.data(), 0); // <- LUMA (compIdx = 0)
    if (videoParam.SAOChromaFlag) {
        int offset[5];
        offset[0] = numBlocks * (16+16+32+32);
        offset[1] = offset[2] = numCtbs*4*4;
        offset[3] = offset[4] = numCtbs*1*32;
        AccumulateStats(&videoParam, blockStats.data() + offset[0], diffEoGpu.data() + offset[1], countEoGpu.data()  + offset[2], diffBoGpu.data() + offset[3], countBoGpu.data() + offset[4], 1);//enableChromaU

        //offset[0] <<= 1;
        offset[0] += numBlocksChroma * (16+16+32+32);
        offset[1] <<= 1;offset[2] <<= 1;
        offset[3] <<= 1;offset[4] <<= 1;
        AccumulateStats(&videoParam, blockStats.data() + offset[0], diffEoGpu.data() + offset[1], countEoGpu.data()  + offset[2], diffBoGpu.data() + offset[3], countBoGpu.data() + offset[4], 2);//enableChromaV
    }

    res = CompareStats(diffEoGpu.data(), countEoGpu.data(), diffBoGpu.data(), countBoGpu.data(), diffEoCpu.data(), countEoCpu.data(), 
        videoParam.enableBandOffset & 0x01 ? diffBoCpu.data() : nullptr, videoParam.enableBandOffset & 0x01 ? countBoCpu.data() : nullptr, numCtbs, videoParam.SAOChromaFlag);
    CHECK_ERR(res);
    printf("passed\n");

    printf("Est EO:        ");
    res = RunGpuEstimate(blockStats.data(), blockStats.size()*2, &videoParam, saoModesGpu.data());
    CHECK_ERR(res);
    res = CompareParam(saoModesCpu.data(), saoModesGpu.data(), numCtbs, videoParam.SAOChromaFlag);
    CHECK_ERR(res);
    printf("passed\n");

    printf("Apply:         ");
    res = RunCpuApply(recon.data(), outputCpu.data(), &videoParam, saoModesCpu.data());
    CHECK_ERR(res);
    res = RunGpuApply(recon.data(), outputGpu.data(), &videoParam, saoModesGpu.data());
    CHECK_ERR(res);
    res = Compare(outputCpu.data(), outputGpu.data(), width, height, &videoParam, saoModesGpu.data());
    CHECK_ERR(res);
    printf("passed\n");

    //printf("Est&Apply EO:  ");
    //memset(outputGpu.data(), 0, sizeof(outputGpu[0]) * outputGpu.size());
    //res = RunGpuEstimateAndApply(recon.data(), outputGpu.data(), blockStats.data(), saoModesGpu.data(), &videoParam);
    //CHECK_ERR(res);
    //res = CompareParam(saoModesCpu.data(), saoModesGpu.data(), numCtbs);
    //CHECK_ERR(res);
    //res = Compare(outputCpu.data(), outputGpu.data(), width, height);
    //CHECK_ERR(res);
    //printf("passed\n");

    //printf("Stats&Est EO:  ");
    //memset(saoModesGpu.data(), 0, sizeof(saoModesGpu[0]) * saoModesGpu.size());
    //res = RunGpuStatAndEstimate(input.data(), recon.data(), &videoParam, saoModesGpu.data(), false);
    //CHECK_ERR(res);
    //res = CompareParam(saoModesCpu.data(), saoModesGpu.data(), numCtbs);
    //CHECK_ERR(res);
    //printf("passed\n");

    videoParam.enableBandOffset |= 1;

    printf("Stats all:     ");
    memset(blockStats.data(), 0, sizeof(blockStats[0]) * blockStats.size());
    res = RunCpuStatAndEstimate(input.data(), recon.data(), &videoParam, frame_addr_info.data(), saoModesCpu.data(), diffEoCpu.data(), countEoCpu.data(), diffBoCpu.data(), countBoCpu.data(),
        availLeftAboveMask.data(), availLeftAboveMask.data() + maxNumCtbX);
    CHECK_ERR(res);

#if defined(CHROMA_SEPARATIST)
    res = RunGpuStat(input.data(), recon.data(), &videoParam, blockStats.data(), blockStats.size() * 2, false);
    CHECK_ERR(res);
    res = RunGpuStatChroma(input.data(), recon.data(), &videoParam, blockStats.data(), blockStats.size() * 2, false, 1);
#else
    res = RunGpuStat(input.data(), recon.data(), &videoParam, blockStats.data(), blockStats.size() * 2, false);
#endif
    CHECK_ERR(res);
    AccumulateStats(&videoParam, blockStats.data(), diffEoGpu.data(), countEoGpu.data(), diffBoGpu.data(), countBoGpu.data(), 0);// <- LUMA (compIdx = 0)
    if (videoParam.SAOChromaFlag) {
        int offset[5];
        offset[0] = numBlocks * (16+16+32+32);
        offset[1] = offset[2] = numCtbs*4*4;
        offset[3] = offset[4] = numCtbs*1*32;
        AccumulateStats(&videoParam, blockStats.data() + offset[0], diffEoGpu.data() + offset[1], countEoGpu.data()  + offset[2], diffBoGpu.data() + offset[3], countBoGpu.data() + offset[4], 1);//enableChromaU

        //offset[0] <<= 1;
        offset[0] += numBlocksChroma * (16+16+32+32);
        offset[1] <<= 1;offset[2] <<= 1;
        offset[3] <<= 1;offset[4] <<= 1;
        AccumulateStats(&videoParam, blockStats.data() + offset[0], diffEoGpu.data() + offset[1], countEoGpu.data()  + offset[2], diffBoGpu.data() + offset[3], countBoGpu.data() + offset[4], 2);//enableChromaV
    }
    res = CompareStats(diffEoGpu.data(), countEoGpu.data(), diffBoGpu.data(), countBoGpu.data(), diffEoCpu.data(), countEoCpu.data(), diffBoCpu.data(), countBoCpu.data(), numCtbs, videoParam.SAOChromaFlag);
    CHECK_ERR(res);
    printf("passed\n");

    printf("Est all:       ");
    memset(saoModesGpu.data(), 0, sizeof(saoModesGpu[0]) * saoModesGpu.size());
    res = RunGpuEstimate(blockStats.data(), blockStats.size()*2, &videoParam, saoModesGpu.data());
    CHECK_ERR(res);
    res = CompareParam(saoModesCpu.data(), saoModesGpu.data(), numCtbs, videoParam.SAOChromaFlag);
    CHECK_ERR(res);
    printf("passed\n");

    printf("Apply:         ");
    memset(outputGpu.data(), 0, sizeof(outputGpu[0]) * outputGpu.size());
    res = RunCpuApply(recon.data(), outputCpu.data(), &videoParam, saoModesCpu.data());
    CHECK_ERR(res);
    res = RunGpuApply(recon.data(), outputGpu.data(), &videoParam, saoModesGpu.data());
    CHECK_ERR(res);
    res = Compare(outputCpu.data(), outputGpu.data(), width, height, &videoParam, saoModesGpu.data());
    CHECK_ERR(res);
    printf("passed\n");

    //printf("Est&Apply all: ");
    //memset(outputGpu.data(), 0, sizeof(outputGpu[0]) * outputGpu.size());
    //res = RunGpuEstimateAndApply(recon.data(), outputGpu.data(), blockStats.data(), saoModesGpu.data(), &videoParam);
    //CHECK_ERR(res);
    //res = CompareParam(saoModesCpu.data(), saoModesGpu.data(), numCtbs);
    //CHECK_ERR(res);
    //res = Compare(outputCpu.data(), outputGpu.data(), width, height);
    //CHECK_ERR(res);
    //printf("passed\n");

    //printf("Stats&Est all: ");
    //memset(saoModesGpu.data(), 0, sizeof(saoModesGpu[0]) * saoModesGpu.size());
    //res = RunGpuStatAndEstimate(input.data(), recon.data(), &videoParam, saoModesGpu.data(), false);
    //CHECK_ERR(res);
    //res = CompareParam(saoModesCpu.data(), saoModesGpu.data(), numCtbs);
    //CHECK_ERR(res);
    //printf("passed\n");

    printf("\n");
    return 0;
}

/* EOF */
