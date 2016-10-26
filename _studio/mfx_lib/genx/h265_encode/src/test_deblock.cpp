//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015-2016 Intel Corporation. All Rights Reserved.
//

#include "stdio.h"
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "../include/test_common.h"
#include "../include/genx_hevce_deblock_hsw_isa.h"
#include "../include/genx_hevce_deblock_bdw_isa.h"

#ifdef CMRT_EMU
extern "C" void Deblock(SurfaceIndex SURF_SRC, SurfaceIndex SURF_FRAME_CU_DATA, SurfaceIndex SURF_PARAM);
#endif //CMRT_EMU

const mfxI32 Width  = WIDTH;
const mfxI32 Height = HEIGHT;

struct VideoParam
{
    int Width;
    int Height;

    Ipp32s crossSliceBoundaryFlag;
    Ipp32s crossTileBoundaryFlag;
    Ipp32s tcOffset;
    Ipp32s betaOffset;
    Ipp32s TULog2MinSize;
    Ipp32s MaxCUDepth;
    Ipp32s PicWidthInCtbs;
    Ipp32s PicHeightInCtbs;
    mfxU32 Log2MaxCUSize;
    mfxU32 Log2NumPartInCU;
    mfxU8 chromaFormatIdc;
    Ipp32s MaxCUSize;

    int QuadtreeTUMaxDepthIntra;
    int QuadtreeTUMaxDepthInter;
    int QuadtreeTULog2MinSize;
    int QuadtreeTULog2MaxSize;
    int NumMinTUInMaxCU;
    int NumPartInCU;
    int AddCUDepth;
    int MinTUSize;
    int MinCUSize;

    // per frame simulation???
    int QP;
    int slice_type;
    int amp_enabled_flag;
    int num_ref_idx_l0_active;
    int num_ref_idx_l1_active;
    int log2_parallel_merge_level;
};

struct PostProcParam // size = 352 B = 88 DW
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
    Ipp8u reserved[4];             // +87 DW
};

static const Ipp32s tcTable[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,
    3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  8,  9, 10, 11, 13,
    14, 16, 18, 20, 22, 24
};

static const Ipp32s betaTable[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24,
    26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56,
    58, 60, 62, 64
};

const Ipp8u h265_QPtoChromaQP[3][58]=
{
    {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 29, 30, 31, 32,
        33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 39, 40, 41, 42, 43, 44,
        45, 46, 47, 48, 49, 50, 51
    },
    {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
        51, 51, 51, 51, 51, 51, 51
    },
    {
         0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
        34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
        51, 51, 51, 51, 51, 51, 51
     }
};

struct H265MV
{
    mfxI16  mvx;
    mfxI16  mvy;
};

struct H265CUData
{
    H265MV mv[2];
    H265MV mvd[2];
    mfxI8 refIdx[2];
    mfxU8 depth;
    mfxU8 predMode;
    mfxU8 trIdx;
    mfxI8 qp;
    mfxU8 cbf[3];
    mfxU8 intraLumaDir;
    mfxU8 intraChromaDir;
    mfxU8 interDir;
    mfxU8 size;
    mfxU8 partSize;
    mfxU8 mergeIdx;

    union {
        struct {
            mfxU8 mergeFlag : 1;
            mfxU8 ipcmFlag : 1;
            mfxU8 transquantBypassFlag : 1;
            mfxU8 skippedFlag : 1;
            mfxU8 mvpIdx0 : 1;
            mfxU8 mvpIdx1 : 1;
        } flags;
        mfxU8 _flags;
    };
};

struct AddrNode
{
    Ipp32s ctbAddr;
    Ipp32s sameTile;
    Ipp32s sameSlice;
};

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

void Deblocking_Kernel_cpu(mfxU8 *inData, Ipp32s inPitch, mfxU8 *outData, Ipp32s outPitch, int* list0, int* list1, int listLength, PostProcParam & dblkPar, H265CUData* frame_cu_data, int m_ctbAddr, AddrInfo* frame_addr_info);
void SetVideoParam(VideoParam& par, int width, int height);
void AllocateFrameCuData(VideoParam& par, H265CUData** frame_cu_data);
void FillRandomModeDecision(int seed, VideoParam& param, int* list0, int* list1, int listLength, H265CUData* frame_cu_data, AddrInfo* addrInfo);
void FillDeblockParam(PostProcParam & deblockParam, const VideoParam & videoParam, const int list0[33], const int list1[33]);
void FillRandom(Ipp32u abs_part_idx, Ipp8u depth, VideoParam* par, int ctbAddr, H265CUData* cu_data);

namespace {
    int RunGpu(const mfxU8 *inData, Ipp32s inPitch, mfxU8 *outData, Ipp32s outPitch, int* list0, int* list1, int listLength, PostProcParam & dblkPar, H265CUData* frame_cu_data, int m_ctbAddr, AddrInfo* frame_addr_info, bool useUP);
    int RunCpu(const mfxU8 *inData, Ipp32s inPitch, mfxU8 *outData, Ipp32s outPitch, int* list0, int* list1, int listLength, PostProcParam & dblkPar, H265CUData* frame_cu_data, int m_ctbAddr, AddrInfo* frame_addr_info);
    int Compare(const mfxU8 *data1, Ipp32s pitch1, const mfxU8 *data2, Ipp32s pitch2, Ipp32s width, Ipp32s height);
};

int Dump(Ipp8u* data, size_t frameSize, const char* fileName)
{
    FILE *fout = fopen(fileName, "wb");
    if (!fout)
        return FAILED;

    fwrite(data, 1, frameSize, fout);
    fclose(fout);

    return PASSED;
}


namespace {

    // adapter's structures
    struct BorderInfo
    {
        Ipp32u region_border_right, region_border_bottom;
        Ipp32u region_border_left, region_border_top;
    };

    struct AvailInfo
    {
        union {
            struct {
                Ipp8u left : 1;
                Ipp8u above : 1;
                Ipp8u right : 1;
                Ipp8u bottom : 1;
                Ipp8u bottomRight : 1;
            } flags;
            Ipp8u _flag;
        };
    };

    int RunGpu(const mfxU8 *inData, Ipp32s inPitch, mfxU8 *outData, Ipp32s outPitch, int* /*list0*/, int* /*list1*/, int listLength, PostProcParam & dblkPar, H265CUData* frame_cu_data, int m_ctbAddr, AddrInfo* frame_addr_info, bool useUP)
    {
        const Ipp32u paddingLu = useUP ? 96 : 0;
        const Ipp32u paddingCh = useUP ? 96 : 0;

        mfxU32 version = 0;
        CmDevice *device = 0;
        Ipp32s res = ::CreateCmDevice(device, version);
        CHECK_CM_ERR(res);

        CmProgram *program = 0;
        res = device->LoadProgram((void *)genx_hevce_deblock_hsw, sizeof(genx_hevce_deblock_hsw), program);
        CHECK_CM_ERR(res);

        CmKernel *kernel = 0;
        res = device->CreateKernel(program, CM_KERNEL_FUNCTION(Deblock), kernel);
        CHECK_CM_ERR(res);

        // debug printf info
        //res = device->InitPrintBuffer();
        //CHECK_CM_ERR(res);

        // SRC_LU, SRC_CH
        CmSurface2D *inputLuma = 0;
        CmSurface2D *inputChroma = 0;
        CmSurface2DUP *inputLumaUp = 0;
        CmSurface2DUP *inputChromaUp = 0;
        Ipp8u *inputLumaSys = 0;
        Ipp8u *inputChromaSys = 0;
        Ipp32u inputLumaPitch = 0;
        Ipp32u inputChromaPitch = 0;
        if (useUP) {
            Ipp32u size = 0;
            res = device->GetSurface2DInfo(Width+2*paddingLu, Height, CM_SURFACE_FORMAT_P8, inputLumaPitch, size);
            CHECK_CM_ERR(res);
            inputLumaSys = (Ipp8u *)CM_ALIGNED_MALLOC(size, 0x1000);
            res = device->CreateSurface2DUP(Width+2*paddingLu, Height, CM_SURFACE_FORMAT_P8, inputLumaSys, inputLumaUp);
            CHECK_CM_ERR(res);
            for (Ipp32s y = 0; y < Height; y++)
                memcpy(inputLumaSys + paddingLu + y * inputLumaPitch, inData + y * inPitch, Width);
            size = 0;
            res = device->GetSurface2DInfo(Width+2*paddingCh, Height/2, CM_SURFACE_FORMAT_P8, inputChromaPitch, size);
            CHECK_CM_ERR(res);
            inputChromaSys = (Ipp8u *)CM_ALIGNED_MALLOC(size, 0x1000);
            res = device->CreateSurface2DUP(Width+2*paddingCh, Height/2, CM_SURFACE_FORMAT_P8, inputChromaSys, inputChromaUp);
            CHECK_CM_ERR(res);
            for (Ipp32s y = 0; y < Height/2; y++)
                memcpy(inputChromaSys + paddingCh + y * inputChromaPitch, inData + (y + Height) * inPitch, Width);
        } else {
            res = device->CreateSurface2D(Width, Height, CM_SURFACE_FORMAT_P8, inputLuma);
            CHECK_CM_ERR(res);
            res = inputLuma->WriteSurfaceStride(inData, NULL, inPitch);
            CHECK_CM_ERR(res);
            res = device->CreateSurface2D(Width, Height/2, CM_SURFACE_FORMAT_P8, inputChroma);
            CHECK_CM_ERR(res);
            res = inputChroma->WriteSurfaceStride(inData + Height * inPitch, NULL, inPitch);
            CHECK_CM_ERR(res);
        }

        // DST
        CmSurface2D *dst;
        res = device->CreateSurface2D(Width, Height, CM_SURFACE_FORMAT_NV12, dst);
        CHECK_CM_ERR(res);

        // CU_DATA
        int numCtbs = dblkPar.PicWidthInCtbs * dblkPar.PicHeightInCtbs;
        int numCtbs_parts = numCtbs << dblkPar.Log2NumPartInCU;
        Ipp32s size = numCtbs_parts * sizeof(H265CUData);
        CmBufferUP* video_frame_cu_data = 0;
        res = device->CreateBufferUP(size, frame_cu_data, video_frame_cu_data);        
        CHECK_CM_ERR(res);
        //video_frame_cu_data->WriteSurface((const Ipp8u*)frame_cu_data, NULL);

        // PARAM
        size = sizeof(PostProcParam);
        CmBuffer* video_param = 0;
        res = device->CreateBuffer(size, video_param);
        CHECK_CM_ERR(res);
        video_param->WriteSurface((const Ipp8u*)&dblkPar, NULL);

        //-------------------------------------------------------
        SurfaceIndex *idxInputLuma = 0;
        SurfaceIndex *idxInputChroma = 0;
        if (useUP) {
            res = inputLumaUp->GetIndex(idxInputLuma);
            CHECK_CM_ERR(res);
            res = inputChromaUp->GetIndex(idxInputChroma);
            CHECK_CM_ERR(res);
        } else {
            res = inputLuma->GetIndex(idxInputLuma);
            CHECK_CM_ERR(res);
            res = inputChroma->GetIndex(idxInputChroma);
            CHECK_CM_ERR(res);
        }

        SurfaceIndex *idxDst = 0;
        res = dst->GetIndex(idxDst);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxFrameCuData = 0;
        res = video_frame_cu_data->GetIndex(idxFrameCuData);
        CHECK_CM_ERR(res);

        SurfaceIndex *idxParam = 0;
        res = video_param->GetIndex(idxParam);
        CHECK_CM_ERR(res);

        //-------------------------------------------------------
        res = kernel->SetKernelArg(0, sizeof(SurfaceIndex), idxInputLuma);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(1, sizeof(SurfaceIndex), idxInputChroma);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(2, sizeof(Ipp32u), &paddingLu);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(3, sizeof(Ipp32u), &paddingCh);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(4, sizeof(SurfaceIndex), idxDst);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(5, sizeof(SurfaceIndex), idxFrameCuData);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(6, sizeof(SurfaceIndex), idxParam);
        CHECK_CM_ERR(res);

        // set start mb
        Ipp32u start_mbX = 0, start_mbY = 0;
        res = kernel->SetKernelArg(7, sizeof(start_mbX), &start_mbX);
        CHECK_CM_ERR(res);
        res = kernel->SetKernelArg(8, sizeof(start_mbY), &start_mbY);
        CHECK_CM_ERR(res);
        //-------------------------------------------------------

        // for YUV420 we process 16x16 block (4x 8x8_Luma and 1x 16x16_Chroma)
        mfxU32 tsWidthFull  = (dblkPar.Width + 12 + 15) / 16;
        mfxU32 tsWidth = tsWidthFull;
        mfxU32 tsHeight = (dblkPar.Height + 12 + 15) / 16;

        if (tsWidthFull > CM_MAX_THREADSPACE_WIDTH) {
            tsWidth = tsWidthFull / 2;
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

            //-------------------------------------------------------
            res = kernel->SetKernelArg(0, sizeof(SurfaceIndex), idxInputLuma);
            CHECK_CM_ERR(res);
            res = kernel->SetKernelArg(1, sizeof(SurfaceIndex), idxInputChroma);
            CHECK_CM_ERR(res);
            res = kernel->SetKernelArg(2, sizeof(Ipp32u), &paddingLu);
            CHECK_CM_ERR(res);
            res = kernel->SetKernelArg(3, sizeof(Ipp32u), &paddingCh);
            CHECK_CM_ERR(res);
            res = kernel->SetKernelArg(4, sizeof(SurfaceIndex), idxDst);
            CHECK_CM_ERR(res);
            res = kernel->SetKernelArg(5, sizeof(SurfaceIndex), idxFrameCuData);
            CHECK_CM_ERR(res);
            res = kernel->SetKernelArg(6, sizeof(SurfaceIndex), idxParam);
            CHECK_CM_ERR(res);

            res = kernel->SetKernelArg(7, sizeof(start_mbX), &start_mbX);
            CHECK_CM_ERR(res);
            res = kernel->SetKernelArg(8, sizeof(start_mbY), &start_mbY);
            CHECK_CM_ERR(res);
            //-------------------------------------------------------

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

        res = dst->ReadSurfaceStride(outData, NULL, outPitch);
        CHECK_CM_ERR(res);

        //-------------------------------------------------
        // OUTPUT DEBUG
        //-------------------------------------------------
        //res = device->FlushPrintBuffer();

#ifndef CMRT_EMU
        printf("TIME=%.3fms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

        device->DestroyThreadSpace(threadSpace);
        device->DestroyTask(task);
        queue->DestroyEvent(e);
        if (useUP) {
            device->DestroySurface2DUP(inputLumaUp);
            device->DestroySurface2DUP(inputChromaUp);
            CM_ALIGNED_FREE(inputLumaSys);
            CM_ALIGNED_FREE(inputChromaSys);
        } else {
            device->DestroySurface(inputLuma);
            device->DestroySurface(inputChroma);
        }
        device->DestroyBufferUP(video_frame_cu_data);
        device->DestroySurface(video_param);
        device->DestroyKernel(kernel);
        device->DestroyProgram(program);
        ::DestroyCmDevice(device);

        return PASSED;
    }


    int g_strengthNonZero = 0;
    int g_strengthVeryStrong = 0;
    int g_filtWeak = 0;
    int g_filtStrong = 0;

    int RunCpu(const mfxU8 *inData, Ipp32s inPitch, mfxU8 *outData, Ipp32s outPitch, int* list0, int* list1, int listLength, PostProcParam & dblkPar, H265CUData* frame_cu_data, int m_ctbAddr, AddrInfo* frame_addr_info)
    {
        // CPU version is implace, so we use extra copy here
        Ipp32s frameSize = 3 * (dblkPar.Width * dblkPar.Height) >> 1;
        mfxU8 *inputCpu = new mfxU8[frameSize];

        memcpy(inputCpu, inData, frameSize);

        DWORD        tickStart, tickEnd;

        tickStart = GetTickCount();
        Deblocking_Kernel_cpu(inputCpu, inPitch, outData, outPitch, list0, list1, listLength, dblkPar, frame_cu_data, m_ctbAddr, frame_addr_info);
        tickEnd = GetTickCount();

         //printf("\nCPU Timing = %d ms\n", (tickEnd-tickStart));

         //--------------
         // stat data
         /*int g_strengthNonZero = 0;
         int g_strengthVeryStrong = 0;
         int g_filtWeak = 0;
         int g_filtStrong = 0;*/
         /*{
             int totEdg = 2*(dblkPar.Width/8-1) * (dblkPar.Height/8-1)*2;
             float nonZeroPercent = (float)g_strengthNonZero / totEdg * 100.f;
             float veryStrongPercent = (float)g_strengthVeryStrong / totEdg * 100.f;
             float weakPercent = (float)g_filtWeak / totEdg * 100.f;
             float strongPercent = (float)g_filtStrong / totEdg * 100.f;

             printf("\n stat report: nonzero = %4.2f veryStrong = %4.2f weak = %4.2f strong = %4.2f\n", nonZeroPercent, veryStrongPercent, weakPercent, strongPercent);
         }*/
         //--------------

        memcpy(outData, inputCpu, frameSize);

        delete [] inputCpu;

        return PASSED;
    }

    int Compare(const mfxU8 *data1, Ipp32s pitch1, const mfxU8 *data2, Ipp32s pitch2, Ipp32s width, Ipp32s height)
    {
        // Luma
        for (Ipp32s y = 0; y < height; y++) {
            for (Ipp32s x = 0; x < width; x++) {
                Ipp32s diff = abs(data1[y * pitch1 + x] - data2[y * pitch2 + x]);
                if (diff) {
                    printf("Luma bad pixels (x,y)=(%i,%i)\n", x, y);
                    return FAILED;
                }
            }
        }

#if 1
        // Chroma
        const mfxU8* refChroma =  data1 + height*pitch1;
        const mfxU8* tstChroma =  data2 + height*pitch2;

        for (Ipp32s y = 0; y < height/2; y++) {
            for (Ipp32s x = 0; x < width; x++) {
                Ipp32s diff = abs(refChroma[y * pitch1 + x] - tstChroma[y * pitch2 + x]);
                if (diff) {
                    printf("Chroma %s bad pixels (x,y)=(%i,%i)\n", (x & 1) ? "U" : "V", x >> 1, y );
                    return FAILED;
                }
            }
        }
#endif

        return PASSED;
    }
};



void SetDefaultAddrNode(AddrNode & info)
{
    info.ctbAddr = -1;
    info.sameSlice = 0;
    info.sameTile = 0;
}

void SetDefaultAddrInfo(AddrInfo& info)
{
    SetDefaultAddrNode(info.above);
    SetDefaultAddrNode(info.below);
    SetDefaultAddrNode(info.belowRight);
    SetDefaultAddrNode(info.left);
    SetDefaultAddrNode(info.right);
    info.region_border_bottom = 0;
    info.region_border_left = 0;
    info.region_border_right = 0;
    info.region_border_top = 0;
}

#define MRG_MAX_NUM_CANDS           5           // from 2 to 5

static
const Ipp8u h265_scan_z2r4[] =
{
      0,   1,  16,  17,   2,   3,  18,  19,  32,  33,  48,  49,  34,  35,  50,  51,
      4,   5,  20,  21,   6,   7,  22,  23,  36,  37,  52,  53,  38,  39,  54,  55,
     64,  65,  80,  81,  66,  67,  82,  83,  96,  97, 112, 113,  98,  99, 114, 115,
     68,  69,  84,  85,  70,  71,  86,  87, 100, 101, 116, 117, 102, 103, 118, 119,
      8,   9,  24,  25,  10,  11,  26,  27,  40,  41,  56,  57,  42,  43,  58,  59,
     12,  13,  28,  29,  14,  15,  30,  31,  44,  45,  60,  61,  46,  47,  62,  63,
     72,  73,  88,  89,  74,  75,  90,  91, 104, 105, 120, 121, 106, 107, 122, 123,
     76,  77,  92,  93,  78,  79,  94,  95, 108, 109, 124, 125, 110, 111, 126, 127,
    128, 129, 144, 145, 130, 131, 146, 147, 160, 161, 176, 177, 162, 163, 178, 179,
    132, 133, 148, 149, 134, 135, 150, 151, 164, 165, 180, 181, 166, 167, 182, 183,
    192, 193, 208, 209, 194, 195, 210, 211, 224, 225, 240, 241, 226, 227, 242, 243,
    196, 197, 212, 213, 198, 199, 214, 215, 228, 229, 244, 245, 230, 231, 246, 247,
    136, 137, 152, 153, 138, 139, 154, 155, 168, 169, 184, 185, 170, 171, 186, 187,
    140, 141, 156, 157, 142, 143, 158, 159, 172, 173, 188, 189, 174, 175, 190, 191,
    200, 201, 216, 217, 202, 203, 218, 219, 232, 233, 248, 249, 234, 235, 250, 251,
    204, 205, 220, 221, 206, 207, 222, 223, 236, 237, 252, 253, 238, 239, 254, 255
};

static
    const Ipp8u h265_scan_r2z4[] =
{
    0,   1,   4,   5,  16,  17,  20,  21,  64,  65,  68,  69,  80,  81,  84,  85,
    2,   3,   6,   7,  18,  19,  22,  23,  66,  67,  70,  71,  82,  83,  86,  87,
    8,   9,  12,  13,  24,  25,  28,  29,  72,  73,  76,  77,  88,  89,  92,  93,
    10,  11,  14,  15,  26,  27,  30,  31,  74,  75,  78,  79,  90,  91,  94,  95,
    32,  33,  36,  37,  48,  49,  52,  53,  96,  97, 100, 101, 112, 113, 116, 117,
    34,  35,  38,  39,  50,  51,  54,  55,  98,  99, 102, 103, 114, 115, 118, 119,
    40,  41,  44,  45,  56,  57,  60,  61, 104, 105, 108, 109, 120, 121, 124, 125,
    42,  43,  46,  47,  58,  59,  62,  63, 106, 107, 110, 111, 122, 123, 126, 127,
    128, 129, 132, 133, 144, 145, 148, 149, 192, 193, 196, 197, 208, 209, 212, 213,
    130, 131, 134, 135, 146, 147, 150, 151, 194, 195, 198, 199, 210, 211, 214, 215,
    136, 137, 140, 141, 152, 153, 156, 157, 200, 201, 204, 205, 216, 217, 220, 221,
    138, 139, 142, 143, 154, 155, 158, 159, 202, 203, 206, 207, 218, 219, 222, 223,
    160, 161, 164, 165, 176, 177, 180, 181, 224, 225, 228, 229, 240, 241, 244, 245,
    162, 163, 166, 167, 178, 179, 182, 183, 226, 227, 230, 231, 242, 243, 246, 247,
    168, 169, 172, 173, 184, 185, 188, 189, 232, 233, 236, 237, 248, 249, 252, 253,
    170, 171, 174, 175, 186, 187, 190, 191, 234, 235, 238, 239, 250, 251, 254, 255
};

enum EnumSliceType {
    B_SLICE     = 0,
    P_SLICE     = 1,
    I_SLICE     = 2,
};

// supported prediction type
enum PredictionMode
{
    MODE_INTER,           ///< inter-prediction mode
    MODE_INTRA,           ///< intra-prediction mode
    MODE_NONE = 15
};

static double rand_val = 1.239847855923875843957;

static unsigned int myrand()
{
    rand_val *= 1.204839573950674;
    if (rand_val > 1024) rand_val /= 1024;
    return ((Ipp32s *)&rand_val)[0];
}

static
int random(int a,int b)
{
    int result;
    if (a >= 0)
        result = a + (rand() % (b - a));
    else
        result = a + (rand() % (abs(a) + b));

    return result;
}

void FillRandomModeDecision(
    int seed,
    VideoParam& param,
    int* list0,
    int* list1,
    int listLength,
    H265CUData* frame_cu_data,
    AddrInfo* frame_addr_info)
{

    // frameOrder
    int frameOrder = 12; // random??? and ref list distribution based on regular GOP structure???

    // list0/1
    memset(list0, 0, sizeof(int)*listLength);
    memset(list1, 0, sizeof(int)*listLength);

    for (int i = 0; i < 15; i++) {
        list0[i] = random(0, frameOrder);
        list1[i] = random(0, frameOrder);
    }

    // frame_cu_data
    int numCtbs = param.PicWidthInCtbs * param.PicHeightInCtbs;

    srand(seed);

    for (int ctbAddr = 0; ctbAddr < numCtbs; ctbAddr++) {

        //H265CUData* m_data = frame_cu_data + (ctbAddr << param.Log2NumPartInCU);
        int m_ctbAddr = ctbAddr;
        VideoParam* m_par = &param;
        int m_ctbPelX = (ctbAddr % m_par->PicWidthInCtbs) * m_par->MaxCUSize;
        int m_ctbPelY = (ctbAddr / m_par->PicWidthInCtbs) * m_par->MaxCUSize;

        AddrInfo & info = frame_addr_info[ctbAddr];
        SetDefaultAddrInfo(info);

        Ipp32u widthInCU = m_par->PicWidthInCtbs;
        if (m_ctbAddr % widthInCU)
        {
            //m_left = m_data - m_par->NumPartInCU;
            info.left.ctbAddr = m_ctbAddr - 1;
            //if (m_par->NumTiles == 1 || m_par->m_tile_ids[m_ctbAddr] == m_par->m_tile_ids[m_leftAddr])
            info.left.sameTile = 1;
            //if (m_par->NumSlices == 1 || m_par->m_slice_ids[m_ctbAddr] == m_par->m_slice_ids[m_leftAddr])
            info.left.sameSlice = 1;
        }

        if (m_ctbAddr >= widthInCU)
        {
            //m_above = m_data - (widthInCU << m_par->Log2NumPartInCU);
            info.above.ctbAddr = m_ctbAddr - widthInCU;
            //if (m_par->NumTiles == 1 || m_par->m_tile_ids[m_ctbAddr] == m_par->m_tile_ids[m_aboveAddr])
            info.above.sameTile = 1;
            //if (m_par->NumSlices == 1 || m_par->m_slice_ids[m_ctbAddr] == m_par->m_slice_ids[m_aboveAddr])
            info.above.sameSlice = 1;
        }

        if ((m_ctbPelY >> m_par->Log2MaxCUSize) < m_par->PicHeightInCtbs - 1) {
            info.below.ctbAddr = m_ctbAddr + widthInCU;
            //if (m_par->NumTiles == 1 || m_par->m_tile_ids[m_ctbAddr] == m_par->m_tile_ids[m_belowAddr])
            info.below.sameTile = 1;
            //if (m_par->NumSlices == 1 || m_par->m_slice_ids[m_ctbAddr] == m_par->m_slice_ids[m_belowAddr])
            info.below.sameSlice = 1;
        }

        if ((m_ctbPelX >> m_par->Log2MaxCUSize) < widthInCU - 1) {
            info.right.ctbAddr = m_ctbAddr + 1;
            //if (m_par->NumTiles == 1 || m_par->m_tile_ids[m_ctbAddr] == m_par->m_tile_ids[m_rightAddr])
            info.right.sameTile = 1;
            //if (m_par->NumSlices == 1 || m_par->m_slice_ids[m_ctbAddr] == m_par->m_slice_ids[m_rightAddr])
            info.right.sameSlice = 1;
        }

        if ((m_ctbPelY >> m_par->Log2MaxCUSize) < m_par->PicHeightInCtbs - 1 &&
            (m_ctbPelX >> m_par->Log2MaxCUSize) < widthInCU - 1)
        {
            info.belowRight.ctbAddr = m_ctbAddr + widthInCU + 1;
            //if (m_par->NumTiles == 1 || m_par->m_tile_ids[m_ctbAddr] == m_par->m_tile_ids[m_belowRightAddr])
            info.belowRight.sameTile = 1;
            //if (m_par->NumSlices == 1 || m_par->m_slice_ids[m_ctbAddr] == m_par->m_slice_ids[m_belowRightAddr])
            info.belowRight.sameSlice = 1;
        }

        //if ((m_par->NumTiles == 1 && m_par->NumSlices == 1) || m_par->deblockBordersFlag)
        {
            info.region_border_right = m_par->Width;
            info.region_border_bottom = m_par->Height;
            info.region_border_left = 0;
            info.region_border_top = 0;
        }
        /*else
        {
        m_region_border_left = m_par->tileColStart[tile_col] << m_par->Log2MaxCUSize;
        m_region_border_right = (m_par->tileColStart[tile_col] + m_par->tileColWidth[tile_col]) << m_par->Log2MaxCUSize;
        if (m_region_border_right > m_par->Width)
        m_region_border_right = m_par->Width;

        m_region_border_top = m_par->NumTiles > 1 ? (m_par->tileRowStart[tile_row] << m_par->Log2MaxCUSize) :
        (m_cslice->row_first << m_par->Log2MaxCUSize);
        m_region_border_bottom = m_par->NumTiles > 1 ? ((m_par->tileRowStart[tile_row] + m_par->tileRowHeight[tile_row]) << m_par->Log2MaxCUSize) :
        ((m_cslice->row_last + 1)  << m_par->Log2MaxCUSize);
        if (m_region_border_bottom > m_par->Height)
        m_region_border_bottom = m_par->Height;
        }*/
        //-------------------------------

        H265CUData* cu_data = frame_cu_data + (ctbAddr << param.Log2NumPartInCU);
        //FillRandom(0, 0, &param, ctbAddr, cu_data);

        //if (ctbAddr % 3)
        { // special patch for better deblock code coverage
            for (int idx = 0; idx < 1 << param.Log2NumPartInCU; idx++) {
                cu_data[idx].depth = random(0, m_par->MaxCUDepth);//0;
                cu_data[idx].trIdx = random(0, m_par->MaxCUDepth - cu_data[idx].depth + 1);//0;
                cu_data[idx].cbf[0] = random(0, 2) << cu_data[idx].trIdx;

                //???
                cu_data[idx].size = random(0, m_par->MaxCUSize);
                cu_data[idx].partSize = random(0, m_par->MaxCUSize);
                //--

                cu_data[idx].predMode = (mfxU8)random(0, 2);//MODE_INTER;
                cu_data[idx].qp = random(1, 50);

                cu_data[idx].refIdx[0] = random(-1, 15);//(0, 15);
                cu_data[idx].refIdx[1] = random(-1, 15);//(0, 15);

                if (cu_data[idx].predMode > 0) // inter
                {
                    if (cu_data[idx].refIdx[0] == -1 &&  cu_data[idx].refIdx[1] == -1)
                    {
                        int pos = random(0, 1);
                        cu_data[idx].refIdx[pos] = random(0, 15);
                    }
                }

                cu_data[idx].mv[0].mvx = myrand();
                cu_data[idx].mv[0].mvy = myrand();
                cu_data[idx].mv[1].mvx = myrand();
                cu_data[idx].mv[1].mvy = myrand();

            }
        }
    }
}



#if defined(NEED_ENC_SIMULATION)

/// supported partition shape
enum PartitionSize
{
    PART_SIZE_2Nx2N,           ///< symmetric motion partition,  2Nx2N
    PART_SIZE_2NxN,            ///< symmetric motion partition,  2Nx N
    PART_SIZE_Nx2N,            ///< symmetric motion partition,   Nx2N
    PART_SIZE_NxN,             ///< symmetric motion partition,   Nx N
    PART_SIZE_2NxnU,           ///< asymmetric motion partition, 2Nx( N/2) + 2Nx(3N/2)
    PART_SIZE_2NxnD,           ///< asymmetric motion partition, 2Nx(3N/2) + 2Nx( N/2)
    PART_SIZE_nLx2N,           ///< asymmetric motion partition, ( N/2)x2N + (3N/2)x2N
    PART_SIZE_nRx2N,           ///< asymmetric motion partition, (3N/2)x2N + ( N/2)x2N
    PART_SIZE_NONE
};


#define INTRA_PLANAR              0
#define INTRA_DC                  1
#define INTRA_VER                26
#define INTRA_HOR                10
#define INTRA_DM_CHROMA          36
#define NUM_CHROMA_MODE        5

enum InterDir {
    INTER_DIR_PRED_L0 = 1,
    INTER_DIR_PRED_L1 = 2,
};


enum EnumSliceType {
    B_SLICE     = 0,
    P_SLICE     = 1,
    I_SLICE     = 2,
};


const Ipp8u h265_scan_z2r4[256] =
{
    0,   1,  16,  17,   2,   3,  18,  19,  32,  33,  48,  49,  34,  35,  50,  51,
    4,   5,  20,  21,   6,   7,  22,  23,  36,  37,  52,  53,  38,  39,  54,  55,
    64,  65,  80,  81,  66,  67,  82,  83,  96,  97, 112, 113,  98,  99, 114, 115,
    68,  69,  84,  85,  70,  71,  86,  87, 100, 101, 116, 117, 102, 103, 118, 119,
    8,   9,  24,  25,  10,  11,  26,  27,  40,  41,  56,  57,  42,  43,  58,  59,
    12,  13,  28,  29,  14,  15,  30,  31,  44,  45,  60,  61,  46,  47,  62,  63,
    72,  73,  88,  89,  74,  75,  90,  91, 104, 105, 120, 121, 106, 107, 122, 123,
    76,  77,  92,  93,  78,  79,  94,  95, 108, 109, 124, 125, 110, 111, 126, 127,
    128, 129, 144, 145, 130, 131, 146, 147, 160, 161, 176, 177, 162, 163, 178, 179,
    132, 133, 148, 149, 134, 135, 150, 151, 164, 165, 180, 181, 166, 167, 182, 183,
    192, 193, 208, 209, 194, 195, 210, 211, 224, 225, 240, 241, 226, 227, 242, 243,
    196, 197, 212, 213, 198, 199, 214, 215, 228, 229, 244, 245, 230, 231, 246, 247,
    136, 137, 152, 153, 138, 139, 154, 155, 168, 169, 184, 185, 170, 171, 186, 187,
    140, 141, 156, 157, 142, 143, 158, 159, 172, 173, 188, 189, 174, 175, 190, 191,
    200, 201, 216, 217, 202, 203, 218, 219, 232, 233, 248, 249, 234, 235, 250, 251,
    204, 205, 220, 221, 206, 207, 222, 223, 236, 237, 252, 253, 238, 239, 254, 255
};


extern const Ipp32s h265_numPu[8] =
{
    1, // PART_SIZE_2Nx2N
    2, // PART_SIZE_2NxN
    2, // PART_SIZE_Nx2N
    4, // PART_SIZE_NxN
    2, // PART_SIZE_2NxnU
    2, // PART_SIZE_2NxnD
    2, // PART_SIZE_nLx2N
    2, // PART_SIZE_nRx2N
};


void GetPartOffsetAndSize(Ipp32s idx,
                          Ipp32s partMode,
                          Ipp32s cuSize,
                          Ipp32s& partX,
                          Ipp32s& partY,
                          Ipp32s& partWidth,
                          Ipp32s& partHeight)
{
    switch (partMode)
    {
    case PART_SIZE_2NxN:
        partWidth = cuSize;
        partHeight = cuSize >> 1;
        partX = 0;
        partY = idx * (cuSize >> 1);
        break;
    case PART_SIZE_Nx2N:
        partWidth = cuSize >> 1;
        partHeight = cuSize;
        partX = idx * (cuSize >> 1);
        partY = 0;
        break;
    case PART_SIZE_NxN:
        partWidth = cuSize >> 1;
        partHeight = cuSize >> 1;
        partX = (idx & 1) * (cuSize >> 1);
        partY = (idx >> 1) * (cuSize >> 1);
        break;
    case PART_SIZE_2NxnU:
        partWidth = cuSize;
        partHeight = (cuSize >> 2) + idx * (cuSize >> 1);
        partX = 0;
        partY = idx * (cuSize >> 2);
        break;
    case PART_SIZE_2NxnD:
        partWidth = cuSize;
        partHeight = (cuSize >> 2) + (1 - idx) * (cuSize >> 1);
        partX = 0;
        partY = idx * ((cuSize >> 1) + (cuSize >> 2));
        break;
    case PART_SIZE_nLx2N:
        partWidth = (cuSize >> 2) + idx * (cuSize >> 1);
        partHeight = cuSize;
        partX = idx * (cuSize >> 2);
        partY = 0;
        break;
    case PART_SIZE_nRx2N:
        partWidth = (cuSize >> 2) + (1 - idx) * (cuSize >> 1);
        partHeight = cuSize;
        partX = idx * ((cuSize >> 1) + (cuSize >> 2));
        partY = 0;
        break;
    case PART_SIZE_2Nx2N:
    default:
        partWidth = cuSize;
        partHeight = cuSize;
        partX = 0;
        partY = 0;
        break;
    }
}

void GetPartAddr(Ipp32s partIdx,
                 Ipp32s partMode,
                 Ipp32s numPartition,
                 Ipp32s& partAddr)
{
    if (partIdx == 0) {
        partAddr = 0;
        return;
    }
    switch (partMode)
    {
    case PART_SIZE_2NxN:
        partAddr = numPartition >> 1;
        break;
    case PART_SIZE_Nx2N:
        partAddr = numPartition >> 2;
        break;
    case PART_SIZE_NxN:
        partAddr = (numPartition >> 2) * partIdx;
        break;
    case PART_SIZE_2NxnU:
        partAddr = numPartition >> 3;
        break;
    case PART_SIZE_2NxnD:
        partAddr = (numPartition >> 1) + (numPartition >> 3);
        break;
    case PART_SIZE_nLx2N:
        partAddr = numPartition >> 4;
        break;
    case PART_SIZE_nRx2N:
        partAddr = (numPartition >> 2) + (numPartition >> 4);
        break;
    case PART_SIZE_2Nx2N:
    default:
        partAddr = 0;
        break;
    }
}

static
    const Ipp8u h265_scan_z2r0[] =
{
    0
};

static
    const Ipp8u h265_scan_z2r1[] =
{
    0,  1,  2,  3
};

static
    const Ipp8u h265_scan_z2r2[] =
{
    0,  1,  4,  5,  2,  3,  6,  7,  8,  9, 12, 13, 10, 11, 14, 15
};

static
    const Ipp8u h265_scan_z2r3[] =
{
    0,  1,  8,  9,  2,  3, 10, 11, 16, 17, 24, 25, 18, 19, 26, 27,
    4,  5, 12, 13,  6,  7, 14, 15, 20, 21, 28, 29, 22, 23, 30, 31,
    32, 33, 40, 41, 34, 35, 42, 43, 48, 49, 56, 57, 50, 51, 58, 59,
    36, 37, 44, 45, 38, 39, 46, 47, 52, 53, 60, 61, 54, 55, 62, 63
};

//static
//const Ipp8u h265_scan_z2r4[] =
//{
//      0,   1,  16,  17,   2,   3,  18,  19,  32,  33,  48,  49,  34,  35,  50,  51,
//      4,   5,  20,  21,   6,   7,  22,  23,  36,  37,  52,  53,  38,  39,  54,  55,
//     64,  65,  80,  81,  66,  67,  82,  83,  96,  97, 112, 113,  98,  99, 114, 115,
//     68,  69,  84,  85,  70,  71,  86,  87, 100, 101, 116, 117, 102, 103, 118, 119,
//      8,   9,  24,  25,  10,  11,  26,  27,  40,  41,  56,  57,  42,  43,  58,  59,
//     12,  13,  28,  29,  14,  15,  30,  31,  44,  45,  60,  61,  46,  47,  62,  63,
//     72,  73,  88,  89,  74,  75,  90,  91, 104, 105, 120, 121, 106, 107, 122, 123,
//     76,  77,  92,  93,  78,  79,  94,  95, 108, 109, 124, 125, 110, 111, 126, 127,
//    128, 129, 144, 145, 130, 131, 146, 147, 160, 161, 176, 177, 162, 163, 178, 179,
//    132, 133, 148, 149, 134, 135, 150, 151, 164, 165, 180, 181, 166, 167, 182, 183,
//    192, 193, 208, 209, 194, 195, 210, 211, 224, 225, 240, 241, 226, 227, 242, 243,
//    196, 197, 212, 213, 198, 199, 214, 215, 228, 229, 244, 245, 230, 231, 246, 247,
//    136, 137, 152, 153, 138, 139, 154, 155, 168, 169, 184, 185, 170, 171, 186, 187,
//    140, 141, 156, 157, 142, 143, 158, 159, 172, 173, 188, 189, 174, 175, 190, 191,
//    200, 201, 216, 217, 202, 203, 218, 219, 232, 233, 248, 249, 234, 235, 250, 251,
//    204, 205, 220, 221, 206, 207, 222, 223, 236, 237, 252, 253, 238, 239, 254, 255
//};

const Ipp8u *h265_scan_z2r[] =
{
    h265_scan_z2r0,
    h265_scan_z2r1,
    h265_scan_z2r2,
    h265_scan_z2r3,
    h265_scan_z2r4
};

typedef Ipp8s T_RefIdx;

//void FillRandom(int ctbAddr, mfxU32 absPartIdx, mfxU8 depth, H265CUData* m_data, VideoParam* m_par)
// FillRandom(0, 0)

Ipp8u getNumPartInter(Ipp32s partSize)
{
    Ipp8u iNumPart = 0;

    //switch (data[abs_part_idx].part_size)
    switch (partSize)
    {
    case PART_SIZE_2Nx2N:
        iNumPart = 1;
        break;
    case PART_SIZE_2NxN:
        iNumPart = 2;
        break;
    case PART_SIZE_Nx2N:
        iNumPart = 2;
        break;
    case PART_SIZE_NxN:
        iNumPart = 4;
        break;
    case PART_SIZE_2NxnU:
        iNumPart = 2;
        break;
    case PART_SIZE_2NxnD:
        iNumPart = 2;
        break;
    case PART_SIZE_nLx2N:
        iNumPart = 2;
        break;
    case PART_SIZE_nRx2N:
        iNumPart = 2;
        break;
    default:
        //assert(0);
        break;
    }

    return  iNumPart;
}

//static double rand_val = 1.239847855923875843957;
//
//static unsigned int myrand()
//{
//    rand_val *= 1.204839573950674;
//    if (rand_val > 1024) rand_val /= 1024;
//    return ((Ipp32s *)&rand_val)[0];
//}


typedef struct
{
    H265MV   mvCand[10];
    T_RefIdx refIdx[10];
    Ipp32s      numCand;
} MVPInfo;

static
    const Ipp8u h265_scan_r2z0[] =
{
    0
};

static
    const Ipp8u h265_scan_r2z1[] =
{
    0,  1,  2,  3
};

const Ipp8u h265_scan_r2z2[] =
{
    0,  1,  4,  5,  2,  3,  6,  7,  8,  9, 12, 13, 10, 11, 14, 15
};

static
    const Ipp8u h265_scan_r2z3[] =
{
    0,  1,  4,  5, 16, 17, 20, 21,  2,  3,  6,  7, 18, 19, 22, 23,
    8,  9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31,
    32, 33, 36, 37, 48, 49, 52, 53, 34, 35, 38, 39, 50, 51, 54, 55,
    40, 41, 44, 45, 56, 57, 60, 61, 42, 43, 46, 47, 58, 59, 62, 63
};

static
    const Ipp8u h265_scan_r2z4[] =
{
    0,   1,   4,   5,  16,  17,  20,  21,  64,  65,  68,  69,  80,  81,  84,  85,
    2,   3,   6,   7,  18,  19,  22,  23,  66,  67,  70,  71,  82,  83,  86,  87,
    8,   9,  12,  13,  24,  25,  28,  29,  72,  73,  76,  77,  88,  89,  92,  93,
    10,  11,  14,  15,  26,  27,  30,  31,  74,  75,  78,  79,  90,  91,  94,  95,
    32,  33,  36,  37,  48,  49,  52,  53,  96,  97, 100, 101, 112, 113, 116, 117,
    34,  35,  38,  39,  50,  51,  54,  55,  98,  99, 102, 103, 114, 115, 118, 119,
    40,  41,  44,  45,  56,  57,  60,  61, 104, 105, 108, 109, 120, 121, 124, 125,
    42,  43,  46,  47,  58,  59,  62,  63, 106, 107, 110, 111, 122, 123, 126, 127,
    128, 129, 132, 133, 144, 145, 148, 149, 192, 193, 196, 197, 208, 209, 212, 213,
    130, 131, 134, 135, 146, 147, 150, 151, 194, 195, 198, 199, 210, 211, 214, 215,
    136, 137, 140, 141, 152, 153, 156, 157, 200, 201, 204, 205, 216, 217, 220, 221,
    138, 139, 142, 143, 154, 155, 158, 159, 202, 203, 206, 207, 218, 219, 222, 223,
    160, 161, 164, 165, 176, 177, 180, 181, 224, 225, 228, 229, 240, 241, 244, 245,
    162, 163, 166, 167, 178, 179, 182, 183, 226, 227, 230, 231, 242, 243, 246, 247,
    168, 169, 172, 173, 184, 185, 188, 189, 232, 233, 236, 237, 248, 249, 252, 253,
    170, 171, 174, 175, 186, 187, 190, 191, 234, 235, 238, 239, 250, 251, 254, 255
};

const Ipp8u *h265_scan_r2z[] =
{
    h265_scan_r2z0,
    h265_scan_r2z1,
    h265_scan_r2z2,
    h265_scan_r2z3,
    h265_scan_r2z4
};


H265CUData* GetNeighbour(
    Ipp32s& neighbourBlockZScanIdx,
    Ipp32s  neighbourBlockColumn,
    Ipp32s  neighbourBlockRow,
    Ipp32s  curBlockZScanIdx,
    bool isNeedTocheckCurLCU,

    VideoParam* par,
    int ctbAddr,
    H265CUData* data)
{
    int iCUAddr = ctbAddr;
    int ctb_pelx           = ( iCUAddr % par->PicWidthInCtbs ) * par->MaxCUSize;
    int ctb_pely           = ( iCUAddr / par->PicWidthInCtbs ) * par->MaxCUSize;

    Ipp32s maxDepth = par->MaxCUDepth;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    //    Ipp32s maxCUSize = par->MaxCUSize;
    Ipp32s minCUSize = par->MinCUSize;
    Ipp32s minTUSize = par->MinTUSize;
    Ipp32s frameWidthInSamples = par->Width;
    Ipp32s frameHeightInSamples = par->Height;
    //    Ipp32s frameWidthInLCUs = par->PicWidthInCtbs;
    Ipp32s sliceStartAddr = 0;//cslice->slice_segment_address;
    Ipp32s tmpNeighbourBlockRow = neighbourBlockRow;
    Ipp32s tmpNeighbourBlockColumn = neighbourBlockColumn;
    Ipp32s neighbourLCUAddr = 0;
    H265CUData* neighbourLCU;


    if ((((Ipp32s)ctb_pelx + neighbourBlockColumn * minTUSize) >= frameWidthInSamples) ||
        (((Ipp32s)ctb_pely + neighbourBlockRow * minTUSize) >= frameHeightInSamples))
    {
        neighbourBlockZScanIdx = 256; /* doesn't matter */
        return NULL;
    }

    if ((((Ipp32s)ctb_pelx + neighbourBlockColumn * minTUSize) < 0) ||
        (((Ipp32s)ctb_pely + neighbourBlockRow * minTUSize) < 0))
    {
        neighbourBlockZScanIdx = 256; /* doesn't matter */
        return NULL;
    }

    if (neighbourBlockColumn < 0)
    {
        tmpNeighbourBlockColumn += numMinTUInLCU;
    }
    else if (neighbourBlockColumn >= numMinTUInLCU)
    {
        tmpNeighbourBlockColumn -= numMinTUInLCU;
    }

    if (neighbourBlockRow < 0)
    {
        /* Constrained motion data compression */
        if(0)        if ((minCUSize == 8) && (minTUSize == 4))
        {
            tmpNeighbourBlockColumn = ((tmpNeighbourBlockColumn >> 1) << 1) + ((tmpNeighbourBlockColumn >> 1) & 1);
        }

        tmpNeighbourBlockRow += numMinTUInLCU;
    }
    else if (neighbourBlockRow >= numMinTUInLCU)
    {
        tmpNeighbourBlockRow -= numMinTUInLCU;
    }

    neighbourBlockZScanIdx = h265_scan_r2z[maxDepth][(tmpNeighbourBlockRow << maxDepth) + tmpNeighbourBlockColumn];

    if (neighbourBlockRow < 0)
    {
        if (neighbourBlockColumn < 0)
        {
            neighbourLCU = data - par->NumPartInCU;//p_above_left;
            neighbourLCUAddr = ctbAddr - 1;//above_left_addr;
        }
        else if (neighbourBlockColumn >= numMinTUInLCU)
        {
            /*    m_aboveRight = m_data - (widthInCU << m_par->Log2NumPartInCU) + m_par->NumPartInCU;
            m_aboveRightAddr = m_ctbAddr - widthInCU + 1;*/

            neighbourLCU = data - (par->PicWidthInCtbs << par->Log2NumPartInCU) + par->NumPartInCU;//p_above_right;
            neighbourLCUAddr = ctbAddr - par->PicWidthInCtbs + 1;//above_right_addr;
        }
        /*else
        {
        neighbourLCU = p_above;
        neighbourLCUAddr = above_addr;
        }*/
    }
    else if (neighbourBlockRow >= numMinTUInLCU)
    {
        neighbourLCU = NULL;
    }
    else
    {
        if (neighbourBlockColumn < 0)
        {
            /*m_left = m_data - m_par->NumPartInCU;
            m_leftAddr = m_ctbAddr - 1;*/

            neighbourLCU = data - par->NumPartInCU;//p_left;
            neighbourLCUAddr = ctbAddr - 1;//left_addr;
        }
        else if (neighbourBlockColumn >= numMinTUInLCU)
        {
            neighbourLCU = NULL;
        }
        else
        {
            neighbourLCU = data;
            neighbourLCUAddr = ctbAddr;

            if ((isNeedTocheckCurLCU) && (curBlockZScanIdx <= neighbourBlockZScanIdx))
            {
                neighbourLCU = NULL;
            }
        }
    }

    if (neighbourLCU != NULL)
    {
        if (neighbourLCUAddr < sliceStartAddr)
        {
            neighbourLCU = NULL;
        }
    }

    return neighbourLCU;
}

static bool HasEqualMotion(
    Ipp32s blockZScanIdx,
    H265CUData* blockLCU,
    Ipp32s candZScanIdx,
    H265CUData* candLCU)
{
    Ipp32s i;

    for (i = 0; i < 2; i++)
    {
        if (blockLCU[blockZScanIdx].refIdx[i] != candLCU[candZScanIdx].refIdx[i])
        {
            return false;
        }

        if (blockLCU[blockZScanIdx].refIdx[i] >= 0)
        {
            if ((blockLCU[blockZScanIdx].mv[i].mvx != candLCU[candZScanIdx].mv[i].mvx) ||
                (blockLCU[blockZScanIdx].mv[i].mvy != candLCU[candZScanIdx].mv[i].mvy))
            {
                return false;
            }
        }
    }

    return true;
}


static bool IsDiffMER(
    Ipp32s xN,
    Ipp32s yN,
    Ipp32s xP,
    Ipp32s yP,
    Ipp32s log2ParallelMergeLevel)
{
    if ((xN >> log2ParallelMergeLevel) != (xP >> log2ParallelMergeLevel))
    {
        return true;
    }
    if ((yN >> log2ParallelMergeLevel) != (yP >> log2ParallelMergeLevel))
    {
        return true;
    }
    return false;
}



static
    void GetMergeCandInfo(
    MVPInfo* pInfo,
    bool *abCandIsInter,
    Ipp32s *puhInterDirNeighbours,
    Ipp32s blockZScanIdx,
    H265CUData* blockLCU)
{
    Ipp32s iCount = pInfo->numCand;

    abCandIsInter[iCount] = true;

    pInfo->mvCand[2*iCount] = blockLCU[blockZScanIdx].mv[0];
    pInfo->mvCand[2*iCount+1] = blockLCU[blockZScanIdx].mv[1];

    pInfo->refIdx[2*iCount] = blockLCU[blockZScanIdx].refIdx[0];
    pInfo->refIdx[2*iCount+1] = blockLCU[blockZScanIdx].refIdx[1];

    puhInterDirNeighbours[iCount] = 0;

    if (pInfo->refIdx[2*iCount] >= 0)
        puhInterDirNeighbours[iCount] += 1;

    if (pInfo->refIdx[2*iCount+1] >= 0)
        puhInterDirNeighbours[iCount] += 2;

    pInfo->numCand++;
}


void GetInterMergeCandidates(
    Ipp32s topLeftCUBlockZScanIdx,
    Ipp32s partMode,
    Ipp32s partIdx,
    Ipp32s cuSize,
    MVPInfo* pInfo,

    VideoParam* par,
    int ctbAddr,
    H265CUData* data)
{
    int iCUAddr = ctbAddr;
    int ctb_pelx           = ( iCUAddr % par->PicWidthInCtbs ) * par->MaxCUSize;
    int ctb_pely           = ( iCUAddr / par->PicWidthInCtbs ) * par->MaxCUSize;

    bool abCandIsInter[5];
    bool isCandInter[5];
    bool isCandAvailable[5];
    Ipp32s  puhInterDirNeighbours[5];
    Ipp32s  candColumn[5], candRow[5], canXP[5], canYP[5], candZScanIdx[5];
    bool checkCurLCU[5];
    H265CUData* candLCU[5];
    Ipp32s maxDepth = par->MaxCUDepth;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s minTUSize = par->MinTUSize;
    Ipp32s topLeftBlockZScanIdx;
    Ipp32s topLeftRasterIdx;
    Ipp32s topLeftRow;
    Ipp32s topLeftColumn;
    Ipp32s partWidth, partHeight, partX, partY;
    Ipp32s xP, yP, nPSW, nPSH;
    Ipp32s i;

    for (i = 0; i < 5; i++)
    {
        abCandIsInter[i] = false;
        isCandAvailable[i] = false;
    }

    GetPartOffsetAndSize(partIdx, partMode, cuSize, partX, partY, partWidth, partHeight);

    topLeftRasterIdx = h265_scan_z2r[maxDepth][topLeftCUBlockZScanIdx] + partX + numMinTUInLCU * partY;
    topLeftRow = topLeftRasterIdx >> maxDepth;
    topLeftColumn = topLeftRasterIdx & (numMinTUInLCU - 1);
    topLeftBlockZScanIdx = h265_scan_r2z[maxDepth][topLeftRasterIdx];

    xP = ctb_pelx + topLeftColumn * minTUSize;
    yP = ctb_pely + topLeftRow * minTUSize;
    nPSW = partWidth * minTUSize;
    nPSH = partHeight * minTUSize;

    /* spatial candidates */

    /* left */
    candColumn[0] = topLeftColumn - 1;
    candRow[0] = topLeftRow + partHeight - 1;
    canXP[0] = xP - 1;
    canYP[0] = yP + nPSH - 1;
    checkCurLCU[0] = false;

    /* above */
    candColumn[1] = topLeftColumn + partWidth - 1;
    candRow[1] = topLeftRow - 1;
    canXP[1] = xP + nPSW - 1;
    canYP[1] = yP - 1;
    checkCurLCU[1] = false;

    /* above right */
    candColumn[2] = topLeftColumn + partWidth;
    candRow[2] = topLeftRow - 1;
    canXP[2] = xP + nPSW;
    canYP[2] = yP - 1;
    checkCurLCU[2] = true;

    /* below left */
    candColumn[3] = topLeftColumn - 1;
    candRow[3] = topLeftRow + partHeight;
    canXP[3] = xP - 1;
    canYP[3] = yP + nPSH;
    checkCurLCU[3] = true;

    /* above left */
    candColumn[4] = topLeftColumn - 1;
    candRow[4] = topLeftRow - 1;
    canXP[4] = xP - 1;
    canYP[4] = yP - 1;
    checkCurLCU[4] = false;

    pInfo->numCand = 0;

    for (i = 0; i < 5; i++)
    {
        if (pInfo->numCand < 4)
        {
            candLCU[i] = GetNeighbour(candZScanIdx[i], candColumn[i], candRow[i], topLeftBlockZScanIdx, checkCurLCU[i], par, ctbAddr, data);

            if (candLCU[i])
            {
                if (!IsDiffMER(canXP[i], canYP[i], xP, yP, par->log2_parallel_merge_level))
                {
                    candLCU[i] = NULL;
                }
            }

            isCandInter[i] = true;

            if ((candLCU[i] == NULL) || (candLCU[i][candZScanIdx[i]].predMode == MODE_INTRA))
            {
                isCandInter[i] = false;
            }

            if (isCandInter[i])
            {
                bool getInfo = false;

                /* getMergeCandInfo conditions */
                switch (i)
                {
                case 0: /* left */
                    if (!((partIdx == 1) && ((partMode == PART_SIZE_Nx2N) ||
                        (partMode == PART_SIZE_nLx2N) ||
                        (partMode == PART_SIZE_nRx2N))))
                    {
                        getInfo = true;
                        isCandAvailable[0] = true;
                    }
                    break;
                case 1: /* above */
                    if (!((partIdx == 1) && ((partMode == PART_SIZE_2NxN) ||
                        (partMode == PART_SIZE_2NxnU) ||
                        (partMode == PART_SIZE_2NxnD))))
                    {
                        isCandAvailable[1] = true;
                        if (!isCandAvailable[0] || !HasEqualMotion(candZScanIdx[0], candLCU[0],
                            candZScanIdx[1], candLCU[1]))
                        {
                            getInfo = true;
                        }
                    }
                    break;
                case 2: /* above right */
                    isCandAvailable[2] = true;
                    if (!isCandAvailable[1] || !HasEqualMotion(candZScanIdx[1], candLCU[1],
                        candZScanIdx[2], candLCU[2]))
                    {
                        getInfo = true;
                    }
                    break;
                case 3: /* below left */
                    isCandAvailable[3] = true;
                    if (!isCandAvailable[0] || !HasEqualMotion(candZScanIdx[0], candLCU[0],
                        candZScanIdx[3], candLCU[3]))
                    {
                        getInfo = true;
                    }
                    break;
                case 4: /* above left */
                default:
                    isCandAvailable[4] = true;
                    if ((!isCandAvailable[0] || !HasEqualMotion(candZScanIdx[0], candLCU[0],
                        candZScanIdx[4], candLCU[4])) &&
                        (!isCandAvailable[1] || !HasEqualMotion(candZScanIdx[1], candLCU[1],
                        candZScanIdx[4], candLCU[4])))
                    {
                        getInfo = true;
                    }
                    break;
                }

                if (getInfo)
                {
                    GetMergeCandInfo(pInfo, abCandIsInter, puhInterDirNeighbours,
                        candZScanIdx[i], candLCU[i]);

                }
            }
        }
    }

    if (pInfo->numCand > MRG_MAX_NUM_CANDS-1)
        pInfo->numCand = MRG_MAX_NUM_CANDS-1;

#if defined(AYA_FULL_IMPL)
    /* temporal candidate */
    if (par->TMVPFlag)
    {
        Ipp32s frameWidthInSamples = par->Width;
        Ipp32s frameHeightInSamples = par->Height;
        Ipp32s bottomRightCandRow = topLeftRow + partHeight;
        Ipp32s bottomRightCandColumn = topLeftColumn + partWidth;
        Ipp32s centerCandRow = topLeftRow + (partHeight >> 1);
        Ipp32s centerCandColumn = topLeftColumn + (partWidth >> 1);
        T_RefIdx refIdx = 0;
        bool existMV = false;
        H265MV colCandMv;

        if ((((Ipp32s)ctb_pelx + bottomRightCandColumn * minTUSize) >= frameWidthInSamples) ||
            (((Ipp32s)ctb_pely + bottomRightCandRow * minTUSize) >= frameHeightInSamples) ||
            (bottomRightCandRow >= numMinTUInLCU))
        {
            candLCU[0] = NULL;
        }
        else if (bottomRightCandColumn < numMinTUInLCU)
        {
            candLCU[0] = m_colocatedLCU[0];
            candZScanIdx[0] = h265_scan_r2z[maxDepth][(bottomRightCandRow << maxDepth) + bottomRightCandColumn];
        }
        else
        {
            candLCU[0] = m_colocatedLCU[1];
            candZScanIdx[0] = h265_scan_r2z[maxDepth][(bottomRightCandRow << maxDepth) + bottomRightCandColumn - numMinTUInLCU];
        }

        candLCU[1] = m_colocatedLCU[0];
        candZScanIdx[1] = h265_scan_r2z[maxDepth][(centerCandRow << maxDepth) + centerCandColumn];

        if ((candLCU[0] != NULL) &&
            GetColMVP(candLCU[0], candZScanIdx[0], 0, refIdx, colCandMv))
        {
            existMV = true;
        }

        if (!existMV)
        {
            existMV = GetColMVP(candLCU[1], candZScanIdx[1], 0, refIdx, colCandMv);
        }

        if (existMV)
        {
            abCandIsInter[pInfo->numCand] = true;

            pInfo->mvCand[2*pInfo->numCand].mvx = colCandMv.mvx;
            pInfo->mvCand[2*pInfo->numCand].mvy = colCandMv.mvy;

            pInfo->refIdx[2*pInfo->numCand] = refIdx;
            puhInterDirNeighbours[pInfo->numCand] = 1;

            if (cslice->slice_type == B_SLICE)
            {
                existMV = false;

                if ((candLCU[0] != NULL) &&
                    GetColMVP(candLCU[0], candZScanIdx[0], 1, refIdx, colCandMv))
                {
                    existMV = true;
                }

                if (!existMV)
                {
                    existMV = GetColMVP(candLCU[1], candZScanIdx[1], 1, refIdx, colCandMv);
                }

                if (existMV)
                {
                    pInfo->mvCand[2*pInfo->numCand+1].mvx = colCandMv.mvx;
                    pInfo->mvCand[2*pInfo->numCand+1].mvy = colCandMv.mvy;

                    pInfo->refIdx[2*pInfo->numCand+1] = refIdx;
                    puhInterDirNeighbours[pInfo->numCand] += 2;
                }
            }

            pInfo->numCand++;
        }
    }
#endif

    /* combined bi-predictive merging candidates */
    if (par->slice_type == B_SLICE)
    {
        Ipp32s uiPriorityList0[12] = {0 , 1, 0, 2, 1, 2, 0, 3, 1, 3, 2, 3};
        Ipp32s uiPriorityList1[12] = {1 , 0, 2, 0, 2, 1, 3, 0, 3, 1, 3, 2};
        Ipp32s limit = pInfo->numCand * (pInfo->numCand - 1);

        for (i = 0; i < limit && pInfo->numCand != MRG_MAX_NUM_CANDS; i++)
        {
            Ipp32s l0idx = uiPriorityList0[i];
            Ipp32s l1idx = uiPriorityList1[i];

            if (abCandIsInter[l0idx] && (puhInterDirNeighbours[l0idx] & 1) &&
                abCandIsInter[l1idx] && (puhInterDirNeighbours[l1idx] & 2))
            {
                abCandIsInter[pInfo->numCand] = true;
                puhInterDirNeighbours[pInfo->numCand] = 3;

                pInfo->mvCand[2*pInfo->numCand] = pInfo->mvCand[2*l0idx];
                pInfo->refIdx[2*pInfo->numCand] = pInfo->refIdx[2*l0idx];

                pInfo->mvCand[2*pInfo->numCand+1] = pInfo->mvCand[2*l1idx+1];
                pInfo->refIdx[2*pInfo->numCand+1] = pInfo->refIdx[2*l1idx+1];

                /*if ((cslice->m_pRefPicList->m_RefPicListL0.m_Tb[pInfo->refIdx[2*pInfo->numCand]] ==
                cslice->m_pRefPicList->m_RefPicListL1.m_Tb[pInfo->refIdx[2*pInfo->numCand+1]]) &&
                (pInfo->mvCand[2*pInfo->numCand].mvx == pInfo->mvCand[2*pInfo->numCand+1].mvx) &&
                (pInfo->mvCand[2*pInfo->numCand].mvy == pInfo->mvCand[2*pInfo->numCand+1].mvy))
                {
                abCandIsInter[pInfo->numCand] = false;
                }
                else
                {
                pInfo->numCand++;
                }*/
                pInfo->numCand++;
                //-------------------
            }
        }
    }

    /* zero motion vector merging candidates */
    {
        Ipp32s numRefIdx = par->num_ref_idx_l0_active;
        T_RefIdx r = 0;
        T_RefIdx refCnt = 0;

        if (par->slice_type == B_SLICE)
        {
            if (par->num_ref_idx_l1_active < par->num_ref_idx_l0_active)
            {
                numRefIdx = par->num_ref_idx_l1_active;
            }
        }

        while (pInfo->numCand < MRG_MAX_NUM_CANDS)
        {
            r = (refCnt < numRefIdx) ? refCnt : 0;
            pInfo->mvCand[2*pInfo->numCand].mvx = 0;
            pInfo->mvCand[2*pInfo->numCand].mvy = 0;
            pInfo->refIdx[2*pInfo->numCand]   = r;

            pInfo->mvCand[2*pInfo->numCand+1].mvx = 0;
            pInfo->mvCand[2*pInfo->numCand+1].mvy = 0;
            pInfo->refIdx[2*pInfo->numCand+1]   = r;

            pInfo->numCand++;
            refCnt++;
        }
    }
}

static bool IsCandFound(
    T_RefIdx *curRefIdx,
    H265MV* curMV,
    MVPInfo* pInfo,
    Ipp32s candIdx,
    Ipp32s numRefLists)
{
    Ipp32s i;

    for (i = 0; i < numRefLists; i++)
    {
        if (curRefIdx[i] != pInfo->refIdx[2*candIdx+i])
        {
            return false;
        }

        if (curRefIdx[i] >= 0)
        {
            if ((curMV[i].mvx != pInfo->mvCand[2*candIdx+i].mvx) ||
                (curMV[i].mvy != pInfo->mvCand[2*candIdx+i].mvy))
            {
                return false;
            }
        }
    }

    return true;
}

bool AddMVPCand(MVPInfo* pInfo,
                H265CUData *p_data,
                Ipp32s blockZScanIdx,
                Ipp32s refPicListIdx,
                Ipp32s refIdx,
                bool order)
{

    return true;//aya fixme???

    //EncoderRefPicListStruct* refPicList[2];
    Ipp8u isCurrRefLongTerm;
    Ipp32s  currRefTb;
    Ipp32s  i;

    //refPicList[0] = &cslice->m_pRefPicList->m_RefPicListL0;
    //refPicList[1] = &cslice->m_pRefPicList->m_RefPicListL1;

    if (!p_data || (p_data[blockZScanIdx].predMode == MODE_INTRA))
    {
        return false;
    }

    if (!order)
    {
        if (p_data[blockZScanIdx].refIdx[refPicListIdx] == refIdx)
        {
            pInfo->mvCand[pInfo->numCand] = p_data[blockZScanIdx].mv[refPicListIdx];
            pInfo->numCand++;

            return true;
        }

#if defined(AYA_FULL_IMPL)
        currRefTb = refPicList[refPicListIdx]->m_Tb[refIdx];
        refPicListIdx = 1 - refPicListIdx;

        if (p_data[blockZScanIdx].ref_idx[refPicListIdx] >= 0)
        {
            Ipp32s neibRefTb = refPicList[refPicListIdx]->m_Tb[p_data[blockZScanIdx].ref_idx[refPicListIdx]];

            if (currRefTb == neibRefTb)
            {
                pInfo->mvCand[pInfo->numCand] = p_data[blockZScanIdx].mv[refPicListIdx];
                pInfo->numCand++;

                return true;
            }
        }
#endif
    }
    else
    {
        /*currRefTb = refPicList[refPicListIdx]->m_Tb[refIdx];
        isCurrRefLongTerm = refPicList[refPicListIdx]->m_IsLongTermRef[refIdx];

        for (i = 0; i < 2; i++)
        {
        if (p_data[blockZScanIdx].ref_idx[refPicListIdx] >= 0)
        {
        Ipp32s neibRefTb = refPicList[refPicListIdx]->m_Tb[p_data[blockZScanIdx].ref_idx[refPicListIdx]];
        Ipp8u isNeibRefLongTerm = refPicList[refPicListIdx]->m_IsLongTermRef[p_data[blockZScanIdx].ref_idx[refPicListIdx]];

        H265MV cMvPred, rcMv;

        cMvPred = p_data[blockZScanIdx].mv[refPicListIdx];

        if (isCurrRefLongTerm || isNeibRefLongTerm)
        {
        rcMv = cMvPred;
        }
        else
        {
        Ipp32s scale = GetDistScaleFactor(currRefTb, neibRefTb);

        if (scale == 4096)
        {
        rcMv = cMvPred;
        }
        else
        {
        rcMv.mvx = (Ipp16s)Saturate(-32768, 32767, (scale * cMvPred.mvx + 127 + (scale * cMvPred.mvx < 0)) >> 8);
        rcMv.mvy = (Ipp16s)Saturate(-32768, 32767, (scale * cMvPred.mvy + 127 + (scale * cMvPred.mvy < 0)) >> 8);
        }
        }

        pInfo->mvCand[pInfo->numCand] = rcMv;
        pInfo->numCand++;

        return true;
        }

        refPicListIdx = 1 - refPicListIdx;
        }*/
    }

    return false;
}


void GetMvpCand(
    Ipp32s topLeftCUBlockZScanIdx,
    Ipp32s refPicListIdx,
    Ipp32s refIdx,
    Ipp32s partMode,
    Ipp32s partIdx,
    Ipp32s cuSize,
    MVPInfo* pInfo,

    VideoParam* par,
    int ctbAddr,
    H265CUData* data)
{
    Ipp32s maxDepth = par->MaxCUDepth;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s topLeftBlockZScanIdx;
    Ipp32s topLeftRasterIdx;
    Ipp32s topLeftRow;
    Ipp32s topLeftColumn;
    Ipp32s belowLeftCandBlockZScanIdx;
    Ipp32s leftCandBlockZScanIdx = 0;
    Ipp32s aboveRightCandBlockZScanIdx;
    Ipp32s aboveCandBlockZScanIdx = 0;
    Ipp32s aboveLeftCandBlockZScanIdx = 0;
    H265CUData* belowLeftCandLCU = NULL;
    H265CUData* leftCandLCU = NULL;
    H265CUData* aboveRightCandLCU = NULL;
    H265CUData* aboveCandLCU = NULL;
    H265CUData* aboveLeftCandLCU = NULL;
    bool bAdded = false;
    Ipp32s partWidth, partHeight, partX, partY;

    pInfo->numCand = 0;

    if (refIdx < 0)
    {
        return;
    }

    GetPartOffsetAndSize(partIdx, partMode, cuSize, partX, partY, partWidth, partHeight);

    topLeftRasterIdx = h265_scan_z2r[maxDepth][topLeftCUBlockZScanIdx] + partX + numMinTUInLCU * partY;
    topLeftRow = topLeftRasterIdx >> maxDepth;
    topLeftColumn = topLeftRasterIdx & (numMinTUInLCU - 1);
    topLeftBlockZScanIdx = h265_scan_r2z[maxDepth][topLeftRasterIdx];

    /* Get Spatial MV */

    /* Left predictor search */

    belowLeftCandLCU = GetNeighbour(belowLeftCandBlockZScanIdx, topLeftColumn - 1,
        topLeftRow + partHeight, topLeftBlockZScanIdx, true, par, ctbAddr, data);
    bAdded = AddMVPCand(pInfo, belowLeftCandLCU, belowLeftCandBlockZScanIdx, refPicListIdx, refIdx, false);

    if (!bAdded)
    {
        leftCandLCU = GetNeighbour(leftCandBlockZScanIdx, topLeftColumn - 1,
            topLeftRow + partHeight - 1, topLeftBlockZScanIdx, false, par, ctbAddr, data);
        bAdded = AddMVPCand(pInfo, leftCandLCU, leftCandBlockZScanIdx, refPicListIdx, refIdx, false);
    }

    if (!bAdded)
    {
        bAdded = AddMVPCand(pInfo, belowLeftCandLCU, belowLeftCandBlockZScanIdx, refPicListIdx, refIdx, true);

        if (!bAdded)
        {
            bAdded = AddMVPCand(pInfo, leftCandLCU, leftCandBlockZScanIdx, refPicListIdx, refIdx, true);
        }
    }

    /* Above predictor search */

    aboveRightCandLCU = GetNeighbour(aboveRightCandBlockZScanIdx, topLeftColumn + partWidth,
        topLeftRow - 1, topLeftBlockZScanIdx, true, par, ctbAddr, data);
    bAdded = AddMVPCand(pInfo, aboveRightCandLCU, aboveRightCandBlockZScanIdx, refPicListIdx, refIdx, false);

    if (!bAdded)
    {
        aboveCandLCU = GetNeighbour(aboveCandBlockZScanIdx, topLeftColumn + partWidth - 1,
            topLeftRow - 1, topLeftBlockZScanIdx, false, par, ctbAddr, data);
        bAdded = AddMVPCand(pInfo, aboveCandLCU, aboveCandBlockZScanIdx, refPicListIdx, refIdx, false);
    }

    if (!bAdded)
    {
        aboveLeftCandLCU = GetNeighbour(aboveLeftCandBlockZScanIdx, topLeftColumn - 1,
            topLeftRow - 1, topLeftBlockZScanIdx, false, par, ctbAddr, data);
        bAdded = AddMVPCand(pInfo, aboveLeftCandLCU, aboveLeftCandBlockZScanIdx, refPicListIdx, refIdx, false);
    }

    if (pInfo->numCand == 2)
    {
        bAdded = true;
    }
    else
    {
        bAdded = ((belowLeftCandLCU != NULL) &&
            (belowLeftCandLCU[belowLeftCandBlockZScanIdx].predMode != MODE_INTRA));

        if (!bAdded)
        {
            bAdded = ((leftCandLCU != NULL) &&
                (leftCandLCU[leftCandBlockZScanIdx].predMode != MODE_INTRA));
        }
    }

    if (!bAdded)
    {
        bAdded = AddMVPCand(pInfo, aboveRightCandLCU, aboveRightCandBlockZScanIdx, refPicListIdx, refIdx, true);
        if (!bAdded)
        {
            bAdded = AddMVPCand(pInfo, aboveCandLCU, aboveCandBlockZScanIdx, refPicListIdx, refIdx, true);
        }

        if (!bAdded)
        {
            bAdded = AddMVPCand(pInfo, aboveLeftCandLCU, aboveLeftCandBlockZScanIdx, refPicListIdx, refIdx, true);
        }
    }

    if (pInfo->numCand == 2)
    {
        if ((pInfo->mvCand[0].mvx == pInfo->mvCand[1].mvx) &&
            (pInfo->mvCand[0].mvy == pInfo->mvCand[1].mvy))
        {
            pInfo->numCand = 1;
        }
    }

#if defined(AYA_FULL_IMPL)
    if (par->TMVPFlag)
    {
        Ipp32s frameWidthInSamples = par->Width;
        Ipp32s frameHeightInSamples = par->Height;
        Ipp32s topLeftRow = topLeftRasterIdx >> maxDepth;
        Ipp32s topLeftColumn = topLeftRasterIdx & (numMinTUInLCU - 1);
        Ipp32s bottomRightCandRow = topLeftRow + partHeight;
        Ipp32s bottomRightCandColumn = topLeftColumn + partWidth;
        Ipp32s minTUSize = par->MinTUSize;
        Ipp32s colCandBlockZScanIdx = 0;
        H265CUData* colCandLCU;
        H265MV colCandMv;

        if ((((Ipp32s)ctb_pelx + bottomRightCandColumn * minTUSize) >= frameWidthInSamples) ||
            (((Ipp32s)ctb_pely + bottomRightCandRow * minTUSize) >= frameHeightInSamples) ||
            (bottomRightCandRow >= numMinTUInLCU))
        {
            colCandLCU = NULL;
        }
        else if (bottomRightCandColumn < numMinTUInLCU)
        {
            colCandLCU = m_colocatedLCU[0];
            colCandBlockZScanIdx = h265_scan_r2z[maxDepth][(bottomRightCandRow << maxDepth) + bottomRightCandColumn];
        }
        else
        {
            colCandLCU = m_colocatedLCU[1];
            colCandBlockZScanIdx = h265_scan_r2z[maxDepth][(bottomRightCandRow << maxDepth) + bottomRightCandColumn - numMinTUInLCU];
        }

        if ((colCandLCU != NULL) &&
            GetColMVP(colCandLCU, colCandBlockZScanIdx, refPicListIdx, refIdx, colCandMv))
        {
            pInfo->mvCand[pInfo->numCand] = colCandMv;
            pInfo->numCand++;
        }
        else
        {
            Ipp32s centerCandRow = topLeftRow + (partHeight >> 1);
            Ipp32s centerCandColumn = topLeftColumn + (partWidth >> 1);

            colCandLCU = m_colocatedLCU[0];
            colCandBlockZScanIdx = h265_scan_r2z[maxDepth][(centerCandRow << maxDepth) + centerCandColumn];

            if (GetColMVP(colCandLCU, colCandBlockZScanIdx, refPicListIdx, refIdx, colCandMv))
            {
                pInfo->mvCand[pInfo->numCand] = colCandMv;
                pInfo->numCand++;
            }
        }
    }
#endif

    if (pInfo->numCand > 2)
    {
        pInfo->numCand = 2;
    }

    while (pInfo->numCand < 2)
    {
        pInfo->mvCand[pInfo->numCand].mvx = 0;
        pInfo->mvCand[pInfo->numCand].mvy = 0;
        pInfo->numCand++;
    }

    return;
}


void GetPUMVInfo(Ipp32s blockZScanIdx,
                 Ipp32s partAddr,
                 Ipp32s partMode,
                 Ipp32s curPUidx,
                 H265CUData* data,
                 VideoParam* par,
                 int ctbAddr)
{
    Ipp32s CUSizeInMinTU = data[blockZScanIdx].size >> par->QuadtreeTULog2MinSize;
    MVPInfo pInfo;
    T_RefIdx curRefIdx[2];
    H265MV   curMV[2];
    Ipp32s numRefLists = 1;
    Ipp8u i;
    MVPInfo mergeInfo;
    Ipp32s topLeftBlockZScanIdx = blockZScanIdx;
    blockZScanIdx += partAddr;

    pInfo.numCand = 0;
    mergeInfo.numCand = 0;

    // aya
    //if (cslice->slice_type == B_SLICE)
    //{
    //    numRefLists = 2;

    //    if (BIPRED_RESTRICT_SMALL_PU)
    //    {
    //        if ((data[blockZScanIdx].size == 8) && (partMode != PART_SIZE_2Nx2N) &&
    //            (data[blockZScanIdx].inter_dir & INTER_DIR_PRED_L0))
    //        {
    //            numRefLists = 1;
    //        }
    //    }
    //}

    data[blockZScanIdx].flags.mergeFlag = 0;
    //data[blockZScanIdx].mvp_num[0] = data[blockZScanIdx].mvp_num[1] = 0;

    curRefIdx[0] = -1;
    curRefIdx[1] = -1;

    curMV[0].mvx = data[blockZScanIdx].mv[0].mvx;//   curCUData->MVxl0[curPUidx];
    curMV[0].mvy = data[blockZScanIdx].mv[0].mvy;//   curCUData->MVyl0[curPUidx];
    curMV[1].mvx = data[blockZScanIdx].mv[1].mvx;//   curCUData->MVxl1[curPUidx];
    curMV[1].mvy = data[blockZScanIdx].mv[1].mvy;//   curCUData->MVyl1[curPUidx];

    if (data[blockZScanIdx].interDir & INTER_DIR_PRED_L0)
    {
        curRefIdx[0] = data[blockZScanIdx].refIdx[0];
    }

    if ((par->slice_type == B_SLICE) && (data[blockZScanIdx].interDir & INTER_DIR_PRED_L1))
    {
        curRefIdx[1] = data[blockZScanIdx].refIdx[1];
    }

    if ((par->log2_parallel_merge_level > 2) && (data[blockZScanIdx].size == 8))
    {
        if (curPUidx == 0)
        {
            GetInterMergeCandidates(topLeftBlockZScanIdx, PART_SIZE_2Nx2N, curPUidx, CUSizeInMinTU, &mergeInfo, par, ctbAddr, data);
        }
    }
    else
    {
        GetInterMergeCandidates(topLeftBlockZScanIdx, partMode, curPUidx, CUSizeInMinTU, &mergeInfo, par, ctbAddr, data);
    }

    for (i = 0; i < mergeInfo.numCand; i++)
    {
        if (IsCandFound(curRefIdx, curMV, &mergeInfo, i, numRefLists))
        {
            data[blockZScanIdx].flags.mergeFlag = 1;
            data[blockZScanIdx].mergeIdx = i;
            break;
        }
    }

    if (!data[blockZScanIdx].flags.mergeFlag)
    {
        Ipp32s refList;

        for (refList = 0; refList < 2; refList++)
        {
            if (curRefIdx[refList] >= 0)
            {
                Ipp32s minDist = 0;

                GetMvpCand(topLeftBlockZScanIdx, refList, curRefIdx[refList], partMode, curPUidx, CUSizeInMinTU, &pInfo, par, ctbAddr, data);

                //data[blockZScanIdx].mvp_num[refList] = (Ipp8s)pInfo.numCand;

                /* find the best MV cand */

                minDist = abs(curMV[refList].mvx - pInfo.mvCand[0].mvx) + abs(curMV[refList].mvy - pInfo.mvCand[0].mvy);
                data[blockZScanIdx].mvpIdx[refList] = 0;

                for (i = 1; i < pInfo.numCand; i++)
                {
                    Ipp32s dist = abs(curMV[refList].mvx - pInfo.mvCand[i].mvx) + abs(curMV[refList].mvy - pInfo.mvCand[i].mvy);

                    if (dist < minDist) {
                        minDist = dist;
                        data[blockZScanIdx].mvpIdx[refList] = i;
                    }
                }

                data[blockZScanIdx].mvd[refList].mvx = curMV[refList].mvx - pInfo.mvCand[data[blockZScanIdx].mvpIdx[refList]].mvx;
                data[blockZScanIdx].mvd[refList].mvy = curMV[refList].mvy - pInfo.mvCand[data[blockZScanIdx].mvpIdx[refList]].mvy;
            }
        }
    }
}


//---------------------------------------------------------
// Simulation Param
//---------------------------------------------------------

void FillRandom(Ipp32u abs_part_idx, Ipp8u depth,
                VideoParam* par, int ctbAddr,
                H265CUData* cu_data)
{
    H265CUData* data = cu_data;
    int iCUAddr = ctbAddr;
    int ctb_pelx           = ( iCUAddr % par->PicWidthInCtbs ) * par->MaxCUSize;
    int ctb_pely           = ( iCUAddr / par->PicWidthInCtbs ) * par->MaxCUSize;

    //--------------------------------------------------------
    int p_left  = 0;
    int p_above = 0;
    Ipp32u widthInCU = par->PicWidthInCtbs;
    if (ctbAddr % widthInCU)
    {
        p_left = 1;
    }

    if (ctbAddr >= widthInCU)
    {
        p_above = 1;
    }
    //--------------------------------------------------------

    Ipp32u num_parts = ( par->NumPartInCU >> (depth<<1) );
    Ipp32u i;
    Ipp32u lpel_x   = ctb_pelx + ((h265_scan_z2r[par->MaxCUDepth][abs_part_idx] & (par->NumMinTUInMaxCU - 1)) << par->QuadtreeTULog2MinSize);
    Ipp32u rpel_x   = lpel_x + (par->MaxCUSize>>depth)  - 1;
    Ipp32u tpel_y   = ctb_pely + ((h265_scan_z2r[par->MaxCUDepth][abs_part_idx] >> par->MaxCUDepth) << par->QuadtreeTULog2MinSize);
    Ipp32u bpel_y   = tpel_y + (par->MaxCUSize>>depth) - 1;

    if ((depth < par->MaxCUDepth - par->AddCUDepth) &&
        (((myrand() & 1) && (par->Log2MaxCUSize - depth > par->QuadtreeTULog2MinSize)) ||
        rpel_x >= par->Width  || bpel_y >= par->Height ||
        par->Log2MaxCUSize - depth - (par->slice_type == I_SLICE ? par->QuadtreeTUMaxDepthIntra :
        MIN(par->QuadtreeTUMaxDepthIntra, par->QuadtreeTUMaxDepthInter)) > par->QuadtreeTULog2MaxSize))
    {
        // split
        for (i = 0; i < 4; i++)
            FillRandom(abs_part_idx + (num_parts >> 2) * i, depth+1, par, ctbAddr, cu_data);
    } else {
        Ipp8u pred_mode = MODE_INTRA;
        if ( par->slice_type != I_SLICE && (myrand() & 15)) {
            pred_mode = MODE_INTER;
        }
        Ipp8u size = (Ipp8u)(par->MaxCUSize >> depth);
        Ipp32u MaxDepth = pred_mode == MODE_INTRA ? par->QuadtreeTUMaxDepthIntra : par->QuadtreeTUMaxDepthInter;
        Ipp8u part_size;

        if (pred_mode == MODE_INTRA) {
            part_size = (Ipp8u)
                (depth == par->MaxCUDepth - par->AddCUDepth &&
                depth + 1 <= par->MaxCUDepth &&
                ((par->Log2MaxCUSize - depth - MaxDepth == par->QuadtreeTULog2MaxSize) ||
                (myrand() & 1)) ? PART_SIZE_NxN : PART_SIZE_2Nx2N);
        } else {
            if (depth == par->MaxCUDepth - par->AddCUDepth) {
                if (size == 8) {
                    part_size = (Ipp8u)(myrand() % 3);
                } else {
                    part_size = (Ipp8u)(myrand() % 4);
                }
            } else {
                if (par->amp_enabled_flag) {
                    part_size = (Ipp8u)(myrand() % 7);
                    if (part_size == 3) part_size = 7;
                } else {
                    part_size = (Ipp8u)(myrand() % 3);
                }
            }
        }
        Ipp8u intra_split = ((pred_mode == MODE_INTRA && (part_size == PART_SIZE_NxN)) ? 1 : 0);
        Ipp8u inter_split = ((MaxDepth == 1) && (pred_mode == MODE_INTER) && (part_size != PART_SIZE_2Nx2N) ) ? 1 : 0;

        Ipp8u tr_depth = intra_split + inter_split;
        while ((par->MaxCUSize >> (depth + tr_depth)) > 32) tr_depth++;
        while (tr_depth < (MaxDepth - 1 + intra_split + inter_split) &&
            (par->MaxCUSize >> (depth + tr_depth)) > 4 &&
            (par->Log2MaxCUSize - depth - tr_depth > par->QuadtreeTULog2MinSize) &&
            ((myrand() & 1) ||
            (par->Log2MaxCUSize - depth - tr_depth > par->QuadtreeTULog2MaxSize))) tr_depth++;

        for (i = 0; i < num_parts; i++) {
            data[abs_part_idx + i].cbf[0] = 0;
            data[abs_part_idx + i].cbf[1] = 0;
            data[abs_part_idx + i].cbf[2] = 0;
        }

        if (tr_depth > par->QuadtreeTUMaxDepthIntra) {
            printf("FillRandom err 1\n"); exit(1);
        }
        if (depth + tr_depth > par->MaxCUDepth) {
            printf("FillRandom err 2\n"); exit(1);
        }
        if (par->Log2MaxCUSize - (depth + tr_depth) < par->QuadtreeTULog2MinSize) {
            printf("FillRandom err 3\n"); exit(1);
        }
        if (par->Log2MaxCUSize - (depth + tr_depth) > par->QuadtreeTULog2MaxSize) {
            printf("FillRandom err 4\n"); exit(1);
        }
        if (pred_mode == MODE_INTRA) {
            Ipp8u luma_dir = (p_left && p_above) ? (Ipp8u) (myrand() % 35) : INTRA_DC;
            for (i = abs_part_idx; i < abs_part_idx + num_parts; i++) {
                data[i].predMode = pred_mode;
                data[i].depth = depth;
                data[i].size = size;
                data[i].partSize = part_size;
                data[i].intraLumaDir = luma_dir;
                data[i].intraChromaDir = INTRA_DM_CHROMA;
                data[i].qp = par->QP;
                data[i].trIdx = tr_depth;
                data[i].interDir = 0;
            }
        } else {
            Ipp16s mvmax = (Ipp16s)(10 + myrand()%100);
            Ipp16s mvx0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvy0 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvx1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);
            Ipp16s mvy1 = (Ipp16s)((myrand() % (mvmax*2+1)) - mvmax);

            T_RefIdx ref_idx0 = (T_RefIdx)(myrand() % par->num_ref_idx_l0_active);

            Ipp8u inter_dir;
            if (par->slice_type == B_SLICE && part_size != PART_SIZE_2Nx2N && size == 8) {
                inter_dir = (Ipp8u)(1 + myrand()%2);
            } else if (par->slice_type == B_SLICE) {
                inter_dir = (Ipp8u)(1 + myrand()%3);
            } else {
                inter_dir = 1;
            }

            T_RefIdx ref_idx1 = (T_RefIdx)((inter_dir & INTER_DIR_PRED_L1) ? (myrand() % par->num_ref_idx_l1_active) : -1);

            for (i = abs_part_idx; i < abs_part_idx + num_parts; i++) {
                data[i].predMode = pred_mode;
                data[i].depth = depth;
                data[i].size = size;
                data[i].partSize = part_size;
                data[i].intraLumaDir = 0;//luma_dir;
                data[i].qp = par->QP;
                data[i].trIdx = tr_depth;
                data[i].interDir = inter_dir;
                data[i].refIdx[0] = -1;
                data[i].refIdx[1] = -1;
                data[i].mv[0].mvx = 0;
                data[i].mv[0].mvy = 0;
                data[i].mv[1].mvx = 0;
                data[i].mv[1].mvy = 0;
                if (inter_dir & INTER_DIR_PRED_L0) {
                    data[i].refIdx[0] = ref_idx0;
                    data[i].mv[0].mvx = mvx0;
                    data[i].mv[0].mvy = mvy0;
                }
                if (inter_dir & INTER_DIR_PRED_L1) {
                    data[i].refIdx[1] = ref_idx1;
                    data[i].mv[1].mvx = mvx1;
                    data[i].mv[1].mvy = mvy1;
                }
                data[i].flags.mergeFlag = 0;
                data[i].mvd[0].mvx = data[i].mvd[0].mvy = data[i].mvd[1].mvx = data[i].mvd[1].mvy = 0;
                data[i].mvpIdx[0] = data[i].mvpIdx[1] = 0;
                //data[i].mvpNum[0] = data[i].mvpNum[1] = 0;
            }

            for (Ipp32s i = 0; i < getNumPartInter(data[abs_part_idx].partSize); i++)
            {
                Ipp32s PartAddr;
                Ipp32s PartX, PartY, Width, Height;

                GetPartOffsetAndSize(i, data[abs_part_idx].partSize, data[abs_part_idx].size, PartX, PartY, Width, Height);

                GetPartAddr(i, data[abs_part_idx].partSize, num_parts, PartAddr);

                GetPUMVInfo(abs_part_idx, PartAddr, data[abs_part_idx].partSize, i, data, par, ctbAddr);
            }

            if ((num_parts < 4 && part_size != PART_SIZE_2Nx2N) ||
                (num_parts < 16 && part_size > 3)) {
                    printf("FillRandom err 5\n"); exit(1);
            }
        }
    }
}
#endif //#if defined(NEED_ENC_SIMULATION)

//---------------------------------------------------------
// SetParam
//---------------------------------------------------------

void SetVideoParam(VideoParam& par, int width, int height)
{
    par.Width  = width;
    par.Height = height;

    // tu7 sw based
    par.crossSliceBoundaryFlag = 0;
    par.crossTileBoundaryFlag  = 0;
    par.tcOffset               = 0;
    par.betaOffset             = 0;
    par.TULog2MinSize          = 2;
    par.MaxCUDepth             = 4;
    par.Log2MaxCUSize          = 6;
    par.Log2NumPartInCU        = 8;
    par.chromaFormatIdc        = 1;

    par.QuadtreeTUMaxDepthIntra = 2;
    par.QuadtreeTUMaxDepthInter = 2;
    par.QuadtreeTULog2MinSize = 2;
    par.QuadtreeTULog2MaxSize = 5;

    // derrived
    par.MaxCUSize = (1 << par.Log2MaxCUSize);
    par.NumMinTUInMaxCU = par.MaxCUSize >> par.QuadtreeTULog2MinSize;
    par.NumPartInCU = 1 << par.Log2NumPartInCU;
    par.PicWidthInCtbs = (par.Width + par.MaxCUSize - 1) / par.MaxCUSize;
    par.PicHeightInCtbs = (par.Height + par.MaxCUSize - 1) / par.MaxCUSize;


    par.AddCUDepth  = 0;
    while ((par.MaxCUSize >> par.MaxCUDepth) > (1u << (par.QuadtreeTULog2MinSize + par.AddCUDepth)))
        par.AddCUDepth++;

    par.MaxCUDepth += par.AddCUDepth;
    par.AddCUDepth++;


    par.MinTUSize = par.MaxCUSize >> par.MaxCUDepth;
    par.MinCUSize = par.MaxCUSize >> (par.MaxCUDepth - par.AddCUDepth);

    // hard coded??? may be simulation need???
    par.QP = 27;
    par.slice_type = I_SLICE;
    par.amp_enabled_flag = 0;
    par.num_ref_idx_l0_active = 2;
    par.num_ref_idx_l1_active = 2;
    par.log2_parallel_merge_level = 2;
}

void AllocateFrameCuData(VideoParam& par, H265CUData** frame_cu_data)
{
    int numCtbs = par.PicWidthInCtbs * par.PicHeightInCtbs;
    int numCtbs_parts = numCtbs << par.Log2NumPartInCU;

    //*frame_cu_data = new H265CUData [numCtbs_parts];
    int numBytes = sizeof(H265CUData) * numCtbs_parts;
    *frame_cu_data = static_cast<H265CUData *>(CM_ALIGNED_MALLOC(numBytes, 0x1000)); // 4K aligned
}

void FillDeblockParam(PostProcParam & deblockParam, const VideoParam & videoParam, const int list0[33], const int list1[33])
{
    deblockParam.Width  = videoParam.Width;
    deblockParam.Height = videoParam.Height;
    deblockParam.PicWidthInCtbs = videoParam.PicWidthInCtbs;
    deblockParam.PicHeightInCtbs = videoParam.PicHeightInCtbs;
    deblockParam.tcOffset = videoParam.tcOffset;
    deblockParam.betaOffset = videoParam.betaOffset;
    deblockParam.crossSliceBoundaryFlag = videoParam.crossSliceBoundaryFlag;
    deblockParam.crossTileBoundaryFlag = videoParam.crossTileBoundaryFlag;
    deblockParam.TULog2MinSize = videoParam.TULog2MinSize;
    deblockParam.MaxCUDepth = videoParam.MaxCUDepth;
    deblockParam.Log2MaxCUSize = videoParam.Log2MaxCUSize;
    deblockParam.Log2NumPartInCU = videoParam.Log2NumPartInCU;
    deblockParam.MaxCUSize = videoParam.MaxCUSize;
    deblockParam.chromaFormatIdc = videoParam.chromaFormatIdc;

    memcpy(deblockParam.list0, list0, sizeof(deblockParam.list0));
    memcpy(deblockParam.list1, list1, sizeof(deblockParam.list1));
    memcpy(deblockParam.scan2z, h265_scan_r2z4, sizeof(deblockParam.scan2z));

    for (int i = 0; i < 54; i++)
        deblockParam.tabTc[i] = tcTable[i];
    for (int i = 0; i < 52; i++)
        deblockParam.tabBeta[i] = betaTable[i];
    for (int i = 0; i < 58; i++)
        deblockParam.chromaQp[i] = h265_QPtoChromaQP[0][i];
}

//---------------------------------------------------------
//     CPU DEBLOCKING CODE
//---------------------------------------------------------

typedef Ipp8u PixType;

struct FrameData
{
    PixType* y;
    PixType* uv;
    int pitch_luma_pix;
    int pitch_chroma_pix;
};

#define VERT_FILT 0
#define HOR_FILT  1

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

/// reference list index
enum EnumRefPicList
{
    REF_PIC_LIST_0 = 0,   ///< reference list 0
    REF_PIC_LIST_1 = 1,   ///< reference list 1
    REF_PIC_LIST_C = 2,   ///< combined reference list for uni-prediction in B-Slices
    REF_PIC_LIST_X = 100  ///< special mark
};

struct H265EdgeData
{
    Ipp8u deblockP  : 1;
    Ipp8u deblockQ  : 1;
    Ipp8s strength  : 3;

    Ipp8s qp;
    Ipp8s tcOffset;
    Ipp8s betaOffset;
}; // sizeof - 4 bytes

struct H265CUPtr {
    H265CUData *ctbData;
    Ipp32s ctbAddr;
    Ipp32s absPartIdx;
};

const int PITCH_TU = 16;

void SetEdges(Ipp32s width, Ipp32s height, Ipp32s* list0, Ipp32s* list1, H265EdgeData* edge, PostProcParam* dblkPar, AddrInfo* info, H265CUData* cu_data, Ipp32u ctbAddr);

#define Clip3( m_Min, m_Max, m_Value) ( (m_Value) < (m_Min) ? \
    (m_Min) : \
    ( (m_Value) > (m_Max) ? \
    (m_Max) : \
    (m_Value) ) )

//int g_strengthNonZero = 0;
//int g_strengthVeryStrong = 0;
//int g_filtWeak = 0;
//int g_filtStrong = 0;

template <int bitDepth, typename PixType>
static Ipp32s h265_FilterEdgeLuma_Kernel(H265EdgeData *edge, PixType *srcDst, Ipp32s srcDstStride, Ipp32s dir)
{
    //
    g_strengthNonZero++;
    //

    Ipp32s tcIdx, bIdx, tc, beta, sideThreshhold;
    Ipp32s i;
    Ipp32s dp0, dp3, dq0, dq3, d0, d3, dq, dp, d;
    Ipp32s ds0, ds3, dm0, dm3, delta;
    Ipp32s p0, p1, p2, p3, q0, q1, q2, q3, tmp;
    Ipp32s strongFiltering = 2; /* 2 = no filtering, only for counting statistics */
    PixType *r[8];

    Ipp32s max_value = (1 << bitDepth) - 1;
    Ipp32s bitDepthScale = 1 << (bitDepth - 8);

    tcIdx = Clip3(0, 53, edge->qp + 2 * (edge->strength - 1) + edge->tcOffset);
    bIdx = Clip3(0, 51, edge->qp + edge->betaOffset);
    tc =  tcTable[tcIdx]*bitDepthScale;
    beta = betaTable[bIdx]*bitDepthScale;
    sideThreshhold = (beta + (beta >> 1)) >> 3;

    if (dir == VERT_FILT) {
        r[0] = srcDst + 0*srcDstStride - 4;
        r[1] = r[0] + srcDstStride;
        r[2] = r[1] + srcDstStride;
        r[3] = r[2] + srcDstStride;

        dp0 = abs(r[0][1] - 2*r[0][2] + r[0][3]);
        dq0 = abs(r[0][4] - 2*r[0][5] + r[0][6]);

        dp3 = abs(r[3][1] - 2*r[3][2] + r[3][3]);
        dq3 = abs(r[3][4] - 2*r[3][5] + r[3][6]);

        d0 = dp0 + dq0;
        d3 = dp3 + dq3;
        d = d0 + d3;

        if (d >= beta) {
            g_strengthVeryStrong++;
            return strongFiltering;
        }

        dq = dq0 + dq3;
        dp = dp0 + dp3;

        /* since this is abs, can reverse the subtraction */
        ds0  = abs(r[0][0] - r[0][3]);
        ds0 += abs(r[0][4] - r[0][7]);

        ds3  = abs(r[3][0] - r[3][3]);
        ds3 += abs(r[3][4] - r[3][7]);

        dm0  = abs(r[0][4] - r[0][3]);
        dm3  = abs(r[3][4] - r[3][3]);

        strongFiltering = 0;
        if ((ds0 < (beta >> 3)) && (2*d0 < (beta >> 2)) && (dm0 < ((tc * 5 + 1) >> 1)) && (ds3 < (beta >> 3)) && (2*d3 < (beta >> 2)) && (dm3 < ((tc * 5 + 1) >> 1)))
            strongFiltering = 1;

        if (strongFiltering) {

            g_filtStrong++;

            for (i = 0; i < 4; i++) {
                p0 = srcDst[-1];
                p1 = srcDst[-2];
                p2 = srcDst[-3];
                q0 = srcDst[0];
                q1 = srcDst[1];
                q2 = srcDst[2];

                if (edge->deblockP)
                {
                    p3 = srcDst[-4];
                    tmp = p1 + p0 + q0;
                    srcDst[-1] = (PixType)(Clip3(p0 - 2 * tc, p0 + 2 * tc, (p2 + 2 * tmp + q1 + 4) >> 3));     //p0
                    srcDst[-2] = (PixType)(Clip3(p1 - 2 * tc, p1 + 2 * tc, (p2 + tmp + 2) >> 2));              //p1
                    srcDst[-3] = (PixType)(Clip3(p2 - 2 * tc, p2 + 2 * tc, (2 * p3 + 3 * p2 + tmp + 4) >> 3)); //p2
                }

                if (edge->deblockQ)
                {
                    q3 = srcDst[3];

                    tmp = q1 + q0 + p0;
                    srcDst[0] = (PixType)(Clip3(q0 - 2 * tc, q0 + 2 * tc, (q2 + 2 * tmp + p1 + 4) >> 3));     //q0
                    srcDst[1] = (PixType)(Clip3(q1 - 2 * tc, q1 + 2 * tc, (q2 + tmp + 2) >> 2));              //q1
                    srcDst[2] = (PixType)(Clip3(q2 - 2 * tc, q2 + 2 * tc, (2 * q3 + 3 * q2 + tmp + 4) >> 3)); //q2
                }
                srcDst += srcDstStride;
            }
        } else {



            for (i = 0; i < 4; i++) {
                p0 = srcDst[-1];
                p1 = srcDst[-2];
                p2 = srcDst[-3];
                q0 = srcDst[0];
                q1 = srcDst[1];
                q2 = srcDst[2];

                delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;

                if (abs(delta) < tc * 10)
                {
                    g_filtWeak++;

                    delta = Clip3(-tc, tc, delta);

                    if (edge->deblockP)
                    {
                        srcDst[-1] = (PixType)(Clip3(0, max_value, (p0 + delta)));

                        if (dp < sideThreshhold)
                        {
                            tmp = Clip3(-(tc >> 1), (tc >> 1), ((((p2 + p0 + 1) >> 1) - p1 + delta) >> 1));
                            srcDst[-2] = (PixType)(Clip3(0, max_value, (p1 + tmp)));
                        }
                    }

                    if (edge->deblockQ)
                    {
                        srcDst[0] = (PixType)(Clip3(0, max_value, (q0 - delta)));

                        if (dq < sideThreshhold)
                        {
                            tmp = Clip3(-(tc >> 1), (tc >> 1), ((((q2 + q0 + 1) >> 1) - q1 - delta) >> 1));
                            srcDst[1] = (PixType)(Clip3(0, max_value, (q1 + tmp)));
                        }
                    }
                }

                srcDst += srcDstStride;
            }
        }
    } else {
        r[0] = srcDst - 4*srcDstStride;
        r[1] = r[0] + srcDstStride;
        r[2] = r[1] + srcDstStride;
        r[3] = r[2] + srcDstStride;
        r[4] = r[3] + srcDstStride;
        r[5] = r[4] + srcDstStride;
        r[6] = r[5] + srcDstStride;
        r[7] = r[6] + srcDstStride;

        dp0 = abs(r[1][0] - 2*r[2][0] + r[3][0]);
        dq0 = abs(r[4][0] - 2*r[5][0] + r[6][0]);

        dp3 = abs(r[1][3] - 2*r[2][3] + r[3][3]);
        dq3 = abs(r[4][3] - 2*r[5][3] + r[6][3]);

        d0 = dp0 + dq0;
        d3 = dp3 + dq3;

        dq = dq0 + dq3;
        dp = dp0 + dp3;

        d = d0 + d3;

        if (d >= beta) {
            g_strengthVeryStrong++;
            return strongFiltering;
        }

        /* since this is abs, can reverse the subtraction */
        ds0  = abs(r[0][0] - r[3][0]);
        ds0 += abs(r[4][0] - r[7][0]);

        ds3  = abs(r[0][3] - r[3][3]);
        ds3 += abs(r[4][3] - r[7][3]);

        dm0  = abs(r[4][0] - r[3][0]);
        dm3  = abs(r[4][3] - r[3][3]);

        strongFiltering = 0;
        if ((ds0 < (beta >> 3)) && (2 * d0 < (beta >> 2)) && (dm0 < ((tc * 5 + 1) >> 1)) && (ds3 < (beta >> 3)) && (2 * d3 < (beta >> 2)) && (dm3 < ((tc * 5 + 1) >> 1)))
            strongFiltering = 1;

        if (strongFiltering) {
            g_filtStrong++;
            for (i = 0; i < 4; i++)
            {
                p0 = srcDst[-1*srcDstStride];
                p1 = srcDst[-2*srcDstStride];
                p2 = srcDst[-3*srcDstStride];
                q0 = srcDst[0*srcDstStride];
                q1 = srcDst[1*srcDstStride];
                q2 = srcDst[2*srcDstStride];

                if (edge->deblockP)
                {
                    p3 = srcDst[-4*srcDstStride];
                    tmp = p1 + p0 + q0;
                    srcDst[-1*srcDstStride] = (PixType)(Clip3(p0 - 2 * tc, p0 + 2 * tc, (p2 + 2 * tmp + q1 + 4) >> 3));     //p0
                    srcDst[-2*srcDstStride] = (PixType)(Clip3(p1 - 2 * tc, p1 + 2 * tc, (p2 + tmp + 2) >> 2));              //p1
                    srcDst[-3*srcDstStride] = (PixType)(Clip3(p2 - 2 * tc, p2 + 2 * tc, (2 * p3 + 3 * p2 + tmp + 4) >> 3)); //p2
                }

                if (edge->deblockQ)
                {
                    q3 = srcDst[3*srcDstStride];

                    tmp = q1 + q0 + p0;
                    srcDst[0*srcDstStride] = (PixType)(Clip3(q0 - 2 * tc, q0 + 2 * tc, (q2 + 2 * tmp + p1 + 4) >> 3));     //q0
                    srcDst[1*srcDstStride] = (PixType)(Clip3(q1 - 2 * tc, q1 + 2 * tc, (q2 + tmp + 2) >> 2));              //q1
                    srcDst[2*srcDstStride] = (PixType)(Clip3(q2 - 2 * tc, q2 + 2 * tc, (2 * q3 + 3 * q2 + tmp + 4) >> 3)); //q2
                }
                srcDst += 1;
            }
        } else {
            for (i = 0; i < 4; i++)
            {
                p0 = srcDst[-1*srcDstStride];
                p1 = srcDst[-2*srcDstStride];
                p2 = srcDst[-3*srcDstStride];
                q0 = srcDst[0*srcDstStride];
                q1 = srcDst[1*srcDstStride];
                q2 = srcDst[2*srcDstStride];
                delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;

                if (abs(delta) < tc * 10)
                {

                    g_filtWeak++;

                    delta = Clip3(-tc, tc, delta);

                    if (edge->deblockP)
                    {
                        srcDst[-1*srcDstStride] = (PixType)(Clip3(0, max_value, (p0 + delta)));

                        if (dp < sideThreshhold)
                        {
                            tmp = Clip3(-(tc >> 1), (tc >> 1), ((((p2 + p0 + 1) >> 1) - p1 + delta) >> 1));
                            srcDst[-2*srcDstStride] = (PixType)(Clip3(0, max_value, (p1 + tmp)));
                        }
                    }

                    if (edge->deblockQ)
                    {
                        srcDst[0] = (PixType)(Clip3(0, max_value, (q0 - delta)));

                        if (dq < sideThreshhold)
                        {
                            tmp = Clip3(-(tc >> 1), (tc >> 1), ((((q2 + q0 + 1) >> 1) - q1 - delta) >> 1));
                            srcDst[1*srcDstStride] = (PixType)(Clip3(0, max_value, (q1 + tmp)));
                        }
                    }
                }
                srcDst += 1;
            }
        }
    }

    return strongFiltering;

} // Ipp32s h265_FilterEdgeLuma_8u_I_C(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir)

Ipp32s h265_FilterEdgeLuma_I(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir)
{
    return h265_FilterEdgeLuma_Kernel<8, Ipp8u>(edge, srcDst, srcDstStride, dir);
}

template <typename PixType>
void DeblockOneCrossLuma(
    PixType* m_yRec,
    Ipp32s m_pitchRecLuma,
    Ipp32s curPixelColumn,
    Ipp32s curPixelRow,
    H265EdgeData* edge_,
    Ipp32s m_region_border_top,
    Ipp32s m_region_border_bottom,
    Ipp32s m_region_border_left,
    Ipp32s m_region_border_right,
    Ipp32s m_ctbPelX,
    Ipp32s m_ctbPelY)
{
    Ipp32s x = curPixelColumn >> 3;
    Ipp32s y = curPixelRow >> 3;
    PixType* baseSrcDst;
    Ipp32s srcDstStride;
    H265EdgeData *edge;
    Ipp32s i, start, end;
    //Ipp32s bitDepthLuma = sizeof(PixType) == 1 ? 8 : 10;

    srcDstStride = m_pitchRecLuma;
    baseSrcDst = m_yRec + curPixelRow * srcDstStride + curPixelColumn;

    start = 0;
    end = 2;

#if 1
    if (m_ctbPelY + curPixelRow == m_region_border_top + 8)
        start = -1;
    if (m_ctbPelY + curPixelRow >= m_region_border_bottom)
        end = 1;

    for (i = start; i < end; i++) {
        edge = &edge_[ (y + ((i + 1) >> 1) - 1) * (9*4) + x *(4) + ((i + 1) & 1)];

        if (edge->strength > 0) {
            h265_FilterEdgeLuma_I(edge, baseSrcDst + 4 * (i - 1) * srcDstStride, srcDstStride, VERT_FILT/*, bitDepthLuma*/);
        }
    }
#endif

#if 1
    start = 0;
    end = 2;
    if (m_ctbPelX + curPixelColumn == m_region_border_left + 8)
        start = -1;
    if (m_ctbPelX + curPixelColumn >= m_region_border_right)
        end = 1;

    for (i = start; i < end; i++) {
        edge = &edge_[y * (9*4)+ (x + ((i + 1) >> 1) - 1) *(4) + (((i + 1) & 1) + 2) ];

        if (edge->strength > 0) {
            h265_FilterEdgeLuma_I(edge, baseSrcDst + 4 * (i - 1), srcDstStride, HOR_FILT/*, bitDepthLuma*/);
        }
    }
#endif
}

//---------------------------------------------------------
static Ipp8u GetChromaQP(
    Ipp8u in_QPy,
    Ipp32s in_QPOffset,
    Ipp8u chromaFormatIdc,
    Ipp8u in_BitDepthChroma)
{
    Ipp32s qPc;
    Ipp32s QpBdOffsetC = 6  *(in_BitDepthChroma - 8);

    qPc = Clip3(-QpBdOffsetC, 57, in_QPy + in_QPOffset);

    if (qPc >= 0)
    {
        qPc = h265_QPtoChromaQP[chromaFormatIdc - 1][qPc];
    }

    return (Ipp8u)(qPc + QpBdOffsetC);
}

template <int bitDepth, typename PixType>
static void h265_FilterEdgeChroma_Interleaved_Kernel(H265EdgeData *edge, PixType *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr)
{
    //Ipp32s qpCb = QpUV(edge->qp + chromaCbQpOffset);
    Ipp32s qpCb = chromaQpCb;

    //Ipp32s qpCr = QpUV(edge->qp + chromaCrQpOffset);
    Ipp32s qpCr = chromaQpCr;

    Ipp32s tcIdxCb = Clip3(0, 53, qpCb + 2 * (edge->strength - 1) + edge->tcOffset);
    Ipp32s tcIdxCr = Clip3(0, 53, qpCr + 2 * (edge->strength - 1) + edge->tcOffset);
    Ipp32s tcCb =  tcTable[tcIdxCb] * (1 << (bitDepth - 8));
    Ipp32s tcCr =  tcTable[tcIdxCr] * (1 << (bitDepth - 8));
    Ipp32s offset, strDstStep;
    Ipp32s i;

    Ipp32s max_value = (1 << bitDepth) - 1;

    if (dir == VERT_FILT)
    {
        offset = 2;
        strDstStep = srcDstStride;
    }
    else
    {
        offset = srcDstStride;
        strDstStep = 2;
    }

    for (i = 0; i < 4; i++)
    {
        Ipp32s p0Cb = srcDst[-1*offset];
        Ipp32s p1Cb = srcDst[-2*offset];
        Ipp32s q0Cb = srcDst[0*offset];
        Ipp32s q1Cb = srcDst[1*offset];
        Ipp32s deltaCb = ((((q0Cb - p0Cb) << 2) + p1Cb - q1Cb + 4) >> 3);
        deltaCb = Clip3(-tcCb, tcCb, deltaCb);

        Ipp32s p0Cr = srcDst[-1*offset + 1];
        Ipp32s p1Cr = srcDst[-2*offset + 1];
        Ipp32s q0Cr = srcDst[0*offset + 1];
        Ipp32s q1Cr = srcDst[1*offset + 1];
        Ipp32s deltaCr = ((((q0Cr - p0Cr) << 2) + p1Cr - q1Cr + 4) >> 3);
        deltaCr = Clip3(-tcCr, tcCr, deltaCr);

        if (edge->deblockP)
        {
            srcDst[-offset] = (PixType)(Clip3(0, max_value, (p0Cb + deltaCb)));
            srcDst[-offset + 1] = (PixType)(Clip3(0, max_value, (p0Cr + deltaCr)));
        }

        if (edge->deblockQ)
        {
            srcDst[0] = (PixType)(Clip3(0, max_value, (q0Cb - deltaCb)));
            srcDst[1] = (PixType)(Clip3(0, max_value, (q0Cr - deltaCr)));
        }

        srcDst += strDstStep;
    }
}

void h265_FilterEdgeChroma_Interleaved_8u_I(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr)
{
    h265_FilterEdgeChroma_Interleaved_Kernel<8, Ipp8u>(edge, srcDst, srcDstStride, dir, chromaQpCb, chromaQpCr);
}

static inline void h265_FilterEdgeChroma_Interleaved_I(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir, Ipp32s chromaQpCb, Ipp32s chromaQpCr, Ipp32u/* bit_depth*/)
{
    h265_FilterEdgeChroma_Interleaved_8u_I(edge, srcDst, srcDstStride, dir, chromaQpCb, chromaQpCr);
}

void DeblockOneCrossChroma(PixType* m_uvRec,
                           Ipp32s m_pitchRecChroma,
                           Ipp32s curPixelColumn, Ipp32s curPixelRow, H265EdgeData* edge_,
                           Ipp32s m_region_border_top,
                           Ipp32s m_region_border_bottom,
                           Ipp32s m_region_border_left,
                           Ipp32s m_region_border_right,
                           Ipp32s m_ctbPelX,
                           Ipp32s m_ctbPelY,
                           Ipp32s chromaFormatIdc)
{
    Ipp32s bitDepthChroma = sizeof(PixType) == 1 ? 8 : 10;
    Ipp32s x = curPixelColumn >> 3;
    Ipp32s y = curPixelRow >> 3;
    PixType* baseSrcDst;
    Ipp32s srcDstStride;
    H265EdgeData *edge;
    Ipp32s i, start, end;

    Ipp32s chromaShiftW = (chromaFormatIdc != MFX_CHROMAFORMAT_YUV444);
    Ipp32s chromaShiftH = (chromaFormatIdc == MFX_CHROMAFORMAT_YUV420);
    Ipp32s chromaShiftWInv = 1 - chromaShiftW;
    Ipp32s chromaShiftHInv = 1 - chromaShiftH;


    srcDstStride = m_pitchRecChroma;
    baseSrcDst = m_uvRec + (curPixelRow >> chromaShiftH) * srcDstStride + (curPixelColumn << chromaShiftWInv);

    start = 0;
    end = 2;

    if (m_ctbPelY + curPixelRow == m_region_border_top + (8 << chromaShiftH))
        start = -1;
    if (m_ctbPelY + curPixelRow >= m_region_border_bottom)
        end = 1;

    for (i = start; i < end; i++) {
        Ipp32s idxY, idxN;
        if (chromaFormatIdc == MFX_CHROMAFORMAT_YUV420) {
            idxY = y + i;
            idxN = 0;
        } else {
            idxY = y + ((i + 1) >> 1);
            idxN = (i + 1) & 1;
        }

        edge = &edge_[(idxY - 1) * (9*4) + x*(4) + idxN];

        if (edge->strength > 1) {
            h265_FilterEdgeChroma_Interleaved_I(
                edge,
                baseSrcDst + 4 * (i - 1) * srcDstStride,
                srcDstStride,
                VERT_FILT,
                GetChromaQP(edge->qp, 0, chromaFormatIdc, 8),
                GetChromaQP(edge->qp, 0, chromaFormatIdc, 8),
                bitDepthChroma);
        }
    }

    start = 0;
    end = 2;
    if (m_ctbPelX + curPixelColumn == m_region_border_left + (8 << chromaShiftW))
        start = -1;
    if (m_ctbPelX + curPixelColumn >= m_region_border_right)
        end = 1;

    for (i = start; i < end; i++) {
        Ipp32s idxX, idxN;
        if ( chromaFormatIdc != MFX_CHROMAFORMAT_YUV444) {
            idxX = x + i;
            idxN = 2;
        } else {
            idxX = x + ((i + 1) >> 1);
            idxN = ((i + 1) & 1) + 2;
        }

        edge = &edge_[y*(9*4) + (idxX - 1)*4 + idxN];

        if (edge->strength > 1) {
            h265_FilterEdgeChroma_Interleaved_I(
                edge,
                baseSrcDst + 4 * 2 * (i - 1),
                srcDstStride,
                HOR_FILT,
                GetChromaQP(edge->qp, 0, chromaFormatIdc, 8),
                GetChromaQP(edge->qp, 0, chromaFormatIdc, 8),
                bitDepthChroma);
        }
    }
}

//---------------------------------------------------------

void Deblocking_Kernel_cpu(
    mfxU8 *inData,
    Ipp32s inPitch,
    mfxU8 *outData,
    Ipp32s /*outPitch*/,
    int* list0, // CmBuffer
    int* list1, // cmBuffer
    int /*listLength*/,
    PostProcParam & dblkPar, // CmBuffer
    H265CUData* frame_cu_data, //CmSurface2D (width = PicWidthInCtbs * {sizeof(H265CUData)*numInCtb}, height = PicHeightInCtbs)
    int /*m_ctbAddr*/,
    AddrInfo* frame_addr_info)
{
    outData;

    int numCtbs = dblkPar.PicWidthInCtbs * dblkPar.PicHeightInCtbs;

    FrameData frameData;
    frameData.y = inData;
    frameData.pitch_luma_pix = inPitch;
    frameData.uv = inData + inPitch * dblkPar.Height;
    frameData.pitch_chroma_pix = inPitch;

    FrameData* recon = &frameData;

    AddrInfo* info = frame_addr_info;


    for (int ctbAddr = 0; ctbAddr < numCtbs; ctbAddr++) {

        H265CUData* cu_data = frame_cu_data + (ctbAddr << dblkPar.Log2NumPartInCU);

        Ipp32s chromaShiftW = (dblkPar.chromaFormatIdc != MFX_CHROMAFORMAT_YUV444);
        Ipp32s chromaShiftH = (dblkPar.chromaFormatIdc == MFX_CHROMAFORMAT_YUV420);
        Ipp32s chromaShiftWInv = 1 - chromaShiftW;
        //Ipp32s chromaShiftHInv = 1 - chromaShiftH;

        Ipp32s ctbPelX = (ctbAddr % dblkPar.PicWidthInCtbs) * dblkPar.MaxCUSize;
        Ipp32s ctbPelY = (ctbAddr / dblkPar.PicWidthInCtbs) * dblkPar.MaxCUSize;
        Ipp32s pitchRecLuma = recon->pitch_luma_pix;
        Ipp32s pitchRecChroma = recon->pitch_chroma_pix;
        PixType* yRec = (PixType*)recon->y + ctbPelX + ctbPelY * pitchRecLuma;
        PixType* uvRec = (PixType*)recon->uv + (ctbPelX << chromaShiftWInv) + (ctbPelY * pitchRecChroma >> chromaShiftH);

        Ipp32s widthInSamples  = info[ctbAddr].region_border_right;
        Ipp32s heightInSamples = info[ctbAddr].region_border_bottom;
        Ipp32s width  = MIN(widthInSamples - ctbPelX,  dblkPar.MaxCUSize);
        Ipp32s height = MIN(heightInSamples - ctbPelY, dblkPar.MaxCUSize);

        H265EdgeData edge[9*9*4] = {0};
        /*printf("\n ctbAddr = %i \n", ctbAddr);fflush(stderr);
        if (ctbAddr == 1981) {
        printf("\n debug \n");
        }*/
        SetEdges(width, height, list0, list1, edge, &dblkPar, info, cu_data, ctbAddr);

        Ipp8s edge_strength[9*9*4] = {0};
        Ipp8s edge_qp[9*9*4] = {0};
        for (int _idx = 0; _idx < 9*9*4; _idx++) {
            edge_strength[_idx] = edge[_idx].strength;
            edge_qp[_idx] = edge[_idx].qp;
        }


        Ipp32u region_border_top    = info[ctbAddr].region_border_top;
        Ipp32u region_border_bottom = info[ctbAddr].region_border_bottom;
        Ipp32u region_border_left   = info[ctbAddr].region_border_left;
        Ipp32u region_border_right = info[ctbAddr].region_border_right;

        int i,j;

        for (j = 8; j <= height; j += 8) {
            for (i = 8; i <= width; i += 8) {
                DeblockOneCrossLuma<PixType>(yRec, pitchRecLuma, i, j, edge, region_border_top, region_border_bottom, region_border_left, region_border_right, ctbPelX, ctbPelY);
            }
        }

        for (j = (8 << chromaShiftH); j <= height; j += (8 << chromaShiftH)) {
            for (i = (8 << chromaShiftW); i <= width; i += (8 << chromaShiftW)) {
                DeblockOneCrossChroma(
                    uvRec, pitchRecChroma,
                    i, j, edge,
                    region_border_top,
                    region_border_bottom,
                    region_border_left,
                    region_border_right,
                    ctbPelX,
                    ctbPelY,
                    dblkPar.chromaFormatIdc);
            }
        }

    }
}


void GetPuAboveOf(H265CUPtr *cu,
                  H265CUPtr *cuBase,
                  Ipp32s absPartIdx,
                  Ipp32s enforceSliceRestriction,
                  Ipp32s enforceTileRestriction,
                  Ipp32u absIdxInLcu,
                  Ipp32s MaxCUDepth,
                  Ipp32s PicWidthInCtbs)
{
    if (absPartIdx > 15) {
        cu->absPartIdx = h265_scan_r2z4[ absPartIdx - PITCH_TU ];
        cu->ctbData = cuBase->ctbData;
        cu->ctbAddr = cuBase->ctbAddr;
        Ipp32u absZorderCuIdx   = h265_scan_z2r4[absIdxInLcu];
        if ( (absPartIdx ^ absZorderCuIdx) > 15 ) {
            cu->absPartIdx -= absIdxInLcu;
        }
        return;
    }

    Ipp32u numPartInCuWidth  = 1 << MaxCUDepth;
    cu->absPartIdx = h265_scan_r2z4[ absPartIdx + PITCH_TU * (numPartInCuWidth-1) ];

    if (cuBase->ctbAddr < PicWidthInCtbs) {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }

    cu->ctbData = cuBase->ctbData - (PicWidthInCtbs << (MaxCUDepth << 1));
    cu->ctbAddr = cuBase->ctbAddr - PicWidthInCtbs;

    if (enforceSliceRestriction || enforceTileRestriction) {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }
}

void GetPuLeftOf(H265CUPtr *cu,
                 H265CUPtr *cuBase,
                 Ipp32s absPartIdx,
                 Ipp32s enforceSliceRestriction,
                 Ipp32s enforceTileRestriction,
                 Ipp32u absIdxInLcu,
                 Ipp32s MaxCUDepth,
                 Ipp32s PicWidthInCtbs)
{
    Ipp32u absZorderCUIdx   = h265_scan_z2r4[absIdxInLcu];
    Ipp32u numPartInCuWidth = 1 << MaxCUDepth;

    if (absPartIdx & 15) {
        cu->absPartIdx = h265_scan_r2z4[ absPartIdx - 1 ];
        cu->ctbData = cuBase->ctbData;
        cu->ctbAddr = cuBase->ctbAddr;
        if ((absPartIdx ^ absZorderCUIdx) & 15) {
            cu->absPartIdx -= absIdxInLcu;
        }
        return;
    }

    cu->absPartIdx = h265_scan_r2z4[ absPartIdx + numPartInCuWidth - 1 ];

    if (!(cuBase->ctbAddr % PicWidthInCtbs)) {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }

    Ipp32u NumPartInCU = 1 << (MaxCUDepth << 1);
    cu->ctbData = cuBase->ctbData - NumPartInCU;
    cu->ctbAddr = cuBase->ctbAddr - 1;

    if (enforceSliceRestriction || enforceTileRestriction) {
        cu->ctbData = NULL;
        cu->ctbAddr = -1;
        return;
    }
}


static
bool MVIsnotEq(H265MV mv0, H265MV mv1)
{
    //Ipp16s mvxd = (Ipp16s)ABS(mv0.mvx - mv1.mvx);
    //Ipp16s mvyd = (Ipp16s)ABS(mv0.mvy - mv1.mvy);

    Ipp32u mvxd = (Ipp32u)ABS(mv0.mvx - mv1.mvx);
    Ipp32u mvyd = (Ipp32u)ABS(mv0.mvy - mv1.mvy);

    if ((mvxd >= 4) || (mvyd >= 4)) {
        return true;
    } else {
        return false;
    }
}

void GetEdgeStrength(H265CUPtr *pcCUQptr,
                     H265CUPtr & CUPPtr,
                     H265EdgeData *edge,
                     Ipp32u uiPartQ,
                     Ipp32s tcOffset,
                     Ipp32s betaOffset,
                     Ipp32s* list0,
                     Ipp32s* list1,
                     Ipp32s MaxCUDepth)
{
    H265CUData* pcCUQ = pcCUQptr->ctbData;
    edge->strength = 0;

    if (CUPPtr.ctbData == NULL) {
        return;
    }

    Ipp32u uiPartP = CUPPtr.absPartIdx;
    H265CUData* pcCUP = CUPPtr.ctbData;

    edge->deblockP = 1;
    edge->deblockQ = 1;

    edge->tcOffset = (Ipp8s)tcOffset;
    edge->betaOffset = (Ipp8s)betaOffset;

    edge->qp = (pcCUQ[uiPartQ].qp + pcCUP[uiPartP].qp + 1) >> 1;
    if (edge->qp == -52)
    {
        printf("\n -52 \n");
    }
    Ipp32s depthp = pcCUP[uiPartP].depth + pcCUP[uiPartP].trIdx;
    Ipp32s depthq = pcCUQ[uiPartQ].depth + pcCUQ[uiPartQ].trIdx;

#if 1
    if ((pcCUQ != pcCUP) ||
        (pcCUP[uiPartP].depth != pcCUQ[uiPartQ].depth) ||
        (depthp != depthq) ||
        ((uiPartP ^ uiPartQ) >> ((MaxCUDepth - depthp) << 1)))
    {
        if ((pcCUQ[uiPartQ].predMode == MODE_INTRA) ||
            (pcCUP[uiPartP].predMode == MODE_INTRA))
        {
            edge->strength = 2;
            return;
        }

        if (((pcCUQ[uiPartQ].cbf[0] >> pcCUQ[uiPartQ].trIdx) != 0) ||
            ((pcCUP[uiPartP].cbf[0] >> pcCUP[uiPartP].trIdx) != 0))
        {
            edge->strength = 1;
            return;
        }
    } else {
        if ((pcCUQ[uiPartQ].predMode == MODE_INTRA) ||
            (pcCUP[uiPartP].predMode == MODE_INTRA))
        {
            edge->strength = 0;
            return;
        }
    }
#endif

    /* Check MV */
    {
        Ipp32s RefPOCQ0, RefPOCQ1, RefPOCP0, RefPOCP1;
        Ipp32s numRefsP, numRefsQ;
        H265MV MVQ0, MVQ1, MVP0, MVP1;
        Ipp32s i;

        numRefsP = 0;

        for (i = 0; i < 2; i++) {
            EnumRefPicList RefPicList = (i ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
            if (pcCUP[uiPartP].refIdx[RefPicList] >= 0) {
                numRefsP++;
            }
        }

        numRefsQ = 0;

        for (i = 0; i < 2; i++) {
            EnumRefPicList RefPicList = (i ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
            if (pcCUQ[uiPartQ].refIdx[RefPicList] >= 0) {
                numRefsQ++;
            }
        }

        if (numRefsP != numRefsQ) {
            edge->strength = 1;
            return;
        }

        if (numRefsQ == 2) {
            RefPOCQ0 = list0[pcCUQ[uiPartQ].refIdx[0]];
            RefPOCQ1 = list1[pcCUQ[uiPartQ].refIdx[1]];
            MVQ0 = pcCUQ[uiPartQ].mv[0];
            MVQ1 = pcCUQ[uiPartQ].mv[1];
            RefPOCP0 = list0[pcCUP[uiPartP].refIdx[0]];
            RefPOCP1 = list1[pcCUP[uiPartP].refIdx[1]];
            MVP0 = pcCUP[uiPartP].mv[0];
            MVP1 = pcCUP[uiPartP].mv[1];

            if (((RefPOCQ0 == RefPOCP0) && (RefPOCQ1 == RefPOCP1)) ||
                ((RefPOCQ0 == RefPOCP1) && (RefPOCQ1 == RefPOCP0)))

            {
                if (RefPOCP0 != RefPOCP1) {
                    if (RefPOCP0 == RefPOCQ0) {
                        Ipp32s bs = ((MVIsnotEq(MVP0, MVQ0) | MVIsnotEq(MVP1, MVQ1)) ? 1 : 0);
                        edge->strength = (Ipp8u)bs;
                        return;
                    } else {
                        Ipp32s bs = ((MVIsnotEq(MVP0, MVQ1) | MVIsnotEq(MVP1, MVQ0)) ? 1 : 0);
                        edge->strength = (Ipp8u)bs;
                        return;
                    }
                } else {
                    Ipp32s bs = (((MVIsnotEq(MVP0, MVQ0) | MVIsnotEq(MVP1, MVQ1)) &&
                        (MVIsnotEq(MVP0, MVQ1) | MVIsnotEq(MVP1, MVQ0))) ? 1 : 0);
                    edge->strength = (Ipp8u)bs;
                    return;
                }
            }

            edge->strength = 1;
            return;
        }
#if 1
        else
        {
            if (pcCUQ[uiPartQ].refIdx[REF_PIC_LIST_0] >= 0) {
                RefPOCQ0 = list0[pcCUQ[uiPartQ].refIdx[0]];
                MVQ0 = pcCUQ[uiPartQ].mv[0];
            } else {
                RefPOCQ0 = list1[pcCUQ[uiPartQ].refIdx[1]];
                MVQ0 = pcCUQ[uiPartQ].mv[1];
            }

            if (pcCUP[uiPartP].refIdx[0] >= 0) {
                RefPOCP0 = list0[pcCUP[uiPartP].refIdx[0]];
                MVP0 = pcCUP[uiPartP].mv[0];
            } else {
                RefPOCP0 = list1[pcCUP[uiPartP].refIdx[1]];
                MVP0 = pcCUP[uiPartP].mv[1];
            }

            if (RefPOCQ0 == RefPOCP0) {
                Ipp32s bs = (MVIsnotEq(MVP0, MVQ0) ? 1 : 0);
                edge->strength = (Ipp8u)bs;
                return;
            }

            edge->strength = 1;
            return;
        }
#endif
    }

    edge->strength = 1;
}

void SetEdgesCTU(
    H265CUPtr *curLCU,
    Ipp32s width,
    Ipp32s height,
    Ipp32s x_inc,
    Ipp32s y_inc,
    Ipp32s crossSliceBoundaryFlag,
    Ipp32s crossTileBoundaryFlag,
    Ipp32s tcOffset,
    Ipp32s betaOffset,
    Ipp32s* list0,
    Ipp32s* list1,
    Ipp32s sameSliceAbove,
    Ipp32s sameTileAbove,
    Ipp32s sameSliceLeft,
    Ipp32s sameTileLeft,
    Ipp32s QuadtreeTULog2MinSize,
    Ipp32s MaxCUDepth,
    Ipp32s PicWidthInCtbs,
    Ipp32u absIdxInLcu,
    H265EdgeData* out_edge)
{
    Ipp32s curPixelColumn, curPixelRow;
    H265EdgeData edge;
    Ipp32s dir;
    Ipp32s i, j, e;
    Ipp32s log2MinTUSize = QuadtreeTULog2MinSize;
    H265CUPtr CUPPtr = {NULL, 0, 0};

    if (curLCU->ctbData) {
        for (j = 0; j < height; j += 8) {
            for (i = 0; i < width; i += 8) {
                for (e = 0; e < 4; e++) {
                    if (e < 2) {
                        curPixelColumn = i;
                        curPixelRow = (j + 4 * e);
                        dir = VERT_FILT;
                    } else {
                        curPixelColumn = (i + 4 * (e - 2));
                        curPixelRow = j;
                        dir = HOR_FILT;
                    }

                    Ipp32s pos_y = (j >> 3) + y_inc;
                    Ipp32s pos_x = (i >> 3) + x_inc;
                    Ipp32s pos_e = e;
                    Ipp32s pos = pos_y * (9*4) + pos_x * (4) + pos_e;

                    H265CUPtr *pcCUQptr = curLCU;
                    Ipp32u uiPartQ = h265_scan_r2z4[((curPixelRow >> log2MinTUSize) * PITCH_TU) + (curPixelColumn >> log2MinTUSize)];

                    if ((dir == HOR_FILT) && (curPixelRow >> log2MinTUSize) != ((curPixelRow - 1) >> log2MinTUSize)) {
                        GetPuAboveOf(&CUPPtr, pcCUQptr, h265_scan_z2r4[uiPartQ], (!crossSliceBoundaryFlag && !sameSliceAbove), (!crossTileBoundaryFlag && !sameTileAbove), absIdxInLcu, MaxCUDepth, PicWidthInCtbs);
                    } else if ((curPixelColumn >> log2MinTUSize) != ((curPixelColumn - 1) >> log2MinTUSize)) {
                        GetPuLeftOf(&CUPPtr, pcCUQptr, h265_scan_z2r4[uiPartQ], (!crossSliceBoundaryFlag && !sameSliceLeft), (!crossTileBoundaryFlag && !sameTileLeft), absIdxInLcu, MaxCUDepth, PicWidthInCtbs);
                    } else {
                        CUPPtr.ctbData    = NULL;
                        CUPPtr.absPartIdx = -1;
                        CUPPtr.ctbAddr    = -1;
                    }

                    edge.strength = 0;
                    GetEdgeStrength(
                        curLCU,
                        CUPPtr,
                        &edge,
                        uiPartQ,
                        tcOffset,
                        betaOffset,
                        list0,
                        list1,
                        MaxCUDepth);

                    //m_edge[(j >> 3) + y_inc][(i >> 3) + x_inc][e] = edge;

                    out_edge[pos_y * (9*4) + pos_x * (4) + pos_e] = edge;
                }
            }
        }
    } else {
        for (j = 0; j < height; j += 8) {
            for (i = 0; i < width; i += 8) {
                for (e = 0; e < 4; e++) {
                    //m_edge[(j >> 3) + y_inc][(i >> 3) + x_inc][e].strength = 0;
                    Ipp32s pos_y = (j >> 3) + y_inc;
                    Ipp32s pos_x = (i >> 3) + x_inc;
                    Ipp32s pos_e = e;
                    out_edge[pos_y * (9*4) + pos_x * (4) + pos_e].strength = 0;
                }

            }
        }
    }
}


inline
    void GetNNDetails(const AddrInfo& info, Ipp32s& sameSliceAbove, Ipp32s& sameTileAbove, Ipp32s& sameSliceLeft, Ipp32s& sameTileLeft)
{
    sameSliceAbove = info.above.sameSlice;
    sameTileAbove  = info.above.sameTile;

    sameSliceLeft = info.left.sameSlice;
    sameTileLeft  = info.left.sameTile;
}

//Edges

//  0
//2_|_3
//  |
//  1

void SetEdges(Ipp32s width, Ipp32s height, Ipp32s* list0, Ipp32s* list1, H265EdgeData* edge, PostProcParam* dblkPar, AddrInfo* info, H265CUData* cu_data, Ipp32u ctbAddr)
{
    Ipp32s crossSliceBoundaryFlag = dblkPar->crossSliceBoundaryFlag;
    Ipp32s crossTileBoundaryFlag  = dblkPar->crossTileBoundaryFlag;
    Ipp32s tcOffset = dblkPar->tcOffset;
    Ipp32s betaOffset = dblkPar->betaOffset;
    Ipp32s TULog2MinSize = dblkPar->TULog2MinSize;
    Ipp32s MaxCUDepth = dblkPar->MaxCUDepth;
    Ipp32s PicWidthInCtbs = dblkPar->PicWidthInCtbs;
    //Ipp32u Log2MaxCUSize = dblkPar->Log2MaxCUSize;
    Ipp32u Log2NumPartInCU = dblkPar->Log2NumPartInCU;
    Ipp32u absIdxInLcu = 0;//dblkPar->absIdxInLcu;//CU
    Ipp32s sameSliceAbove = 0, sameTileAbove = 0, sameSliceLeft = 0, sameTileLeft = 0;
    AddrInfo& infoCurr = info[ctbAddr];


    H265CUPtr curLCU;
    curLCU.ctbAddr = ctbAddr;
    curLCU.ctbData = cu_data;
    curLCU.absPartIdx = 0;
    GetNNDetails(info[curLCU.ctbAddr], sameSliceAbove, sameTileAbove, sameSliceLeft, sameTileLeft);
    SetEdgesCTU(&curLCU, width, height, 0, 0, crossSliceBoundaryFlag, crossTileBoundaryFlag, tcOffset, betaOffset, list0, list1, sameSliceAbove, sameTileAbove, sameSliceLeft, sameTileLeft, TULog2MinSize, MaxCUDepth, PicWidthInCtbs, absIdxInLcu, edge);


    // below
    curLCU.ctbAddr = -1;
    curLCU.ctbData = NULL;
    curLCU.absPartIdx = 0;
    sameSliceAbove = sameTileAbove = sameSliceLeft = sameTileLeft = 0;
    if (infoCurr.below.ctbAddr >= 0 &&
        !((crossSliceBoundaryFlag && (!infoCurr.below.sameSlice)) ||
        (crossTileBoundaryFlag  && (!infoCurr.below.sameTile)))) {

            curLCU.ctbData = cu_data + (PicWidthInCtbs << Log2NumPartInCU);
            curLCU.ctbAddr = infoCurr.below.ctbAddr;
            GetNNDetails(info[curLCU.ctbAddr], sameSliceAbove, sameTileAbove, sameSliceLeft, sameTileLeft);
    }
    SetEdgesCTU(&curLCU, width, 1, 0, height >> 3, crossSliceBoundaryFlag, crossTileBoundaryFlag, tcOffset, betaOffset, list0, list1, sameSliceAbove, sameTileAbove, sameSliceLeft, sameTileLeft,  TULog2MinSize, MaxCUDepth, PicWidthInCtbs, absIdxInLcu, edge);


    // right
    curLCU.ctbAddr = -1;
    curLCU.ctbData = NULL;
    curLCU.absPartIdx = 0;
    sameSliceAbove = sameTileAbove = sameSliceLeft = sameTileLeft = 0;
    if (infoCurr.right.ctbAddr >= 0 &&
        !((crossSliceBoundaryFlag && (!infoCurr.right.sameSlice)) ||
        (crossTileBoundaryFlag  && (!infoCurr.right.sameTile)))) {

            curLCU.ctbData = cu_data + (1 << Log2NumPartInCU);
            curLCU.ctbAddr = infoCurr.right.ctbAddr;
            GetNNDetails(info[curLCU.ctbAddr], sameSliceAbove, sameTileAbove, sameSliceLeft, sameTileLeft);
    }
    SetEdgesCTU(&curLCU, 1, height, width >> 3, 0, crossSliceBoundaryFlag, crossTileBoundaryFlag, tcOffset, betaOffset, list0, list1, sameSliceAbove, sameTileAbove, sameSliceLeft, sameTileLeft, TULog2MinSize, MaxCUDepth, PicWidthInCtbs, absIdxInLcu, edge);


    // below right
    curLCU.ctbAddr = -1;
    curLCU.ctbData = NULL;
    curLCU.absPartIdx = 0;
    sameSliceAbove = sameTileAbove = sameSliceLeft = sameTileLeft = 0;
    if (infoCurr.belowRight.ctbAddr >= 0 &&
        !((crossSliceBoundaryFlag && (!infoCurr.belowRight.sameSlice)) ||
        (crossTileBoundaryFlag  && (!infoCurr.belowRight.sameTile)))) {

            curLCU.ctbData = cu_data + ((PicWidthInCtbs + 1) << Log2NumPartInCU);
            curLCU.ctbAddr = infoCurr.belowRight.ctbAddr;
            GetNNDetails(info[curLCU.ctbAddr], sameSliceAbove, sameTileAbove, sameSliceLeft, sameTileLeft);
    }
    SetEdgesCTU(&curLCU, 1, 1, width >> 3, height >> 3, crossSliceBoundaryFlag, crossTileBoundaryFlag, tcOffset, betaOffset, list0, list1, sameSliceAbove, sameTileAbove, sameSliceLeft, sameTileLeft, TULog2MinSize, MaxCUDepth, PicWidthInCtbs, absIdxInLcu, edge);
}

int main()
{
    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;

    // const per frame params
    Ipp32s width = Width;
    Ipp32s height = Height;

    Ipp32s frameSize = 3 * (width * height) >> 1;
    mfxU8 *input = new mfxU8[frameSize];
    Ipp32s inPitch = width;
    size_t bytesRead = fread(input, 1, frameSize, f);
    if (bytesRead != frameSize)
        return printf("FAILED to read frame from yuv file\n"), 1;
    fclose(f);

    mfxU8 *outputGpu = new mfxU8[frameSize];
    Ipp32s outPitchGpu = width;
    mfxU8 *outputCpu = new mfxU8[frameSize];
    memset(outputCpu, 0, frameSize);
    Ipp32s outPitchCpu = width;

    // structure generation
    VideoParam videoParam;
    SetVideoParam( videoParam, width, height );

    H265CUData* frame_cu_data = NULL;
    AllocateFrameCuData(videoParam, &frame_cu_data);


    int seed = 1234567;
    int list0[33] = {0};
    int list1[33] = {0};
    int listLength = 32;
    AddrInfo* frame_addr_info = new AddrInfo[videoParam.PicWidthInCtbs * videoParam.PicHeightInCtbs];
    FillRandomModeDecision(seed, videoParam, list0, list1, 33, frame_cu_data, frame_addr_info);

    PostProcParam deblockParam;
    FillDeblockParam(deblockParam, videoParam, list0, list1);

    Ipp32s res;
    res = RunCpu(input, inPitch, outputCpu, outPitchCpu, list0, list1, listLength, deblockParam, frame_cu_data, 0/* no matter*/, frame_addr_info);
    CHECK_ERR(res);

    printf("Surface2D: ");
    res = RunGpu(input, inPitch, outputGpu, outPitchGpu, list0, list1, listLength, deblockParam, frame_cu_data, 0/* no matter*/, frame_addr_info, false);
    CHECK_ERR(res);
    res = Compare(outputGpu, outPitchGpu, outputCpu, outPitchCpu, width, height);
    CHECK_ERR(res);
    printf("passed ");
    printf("Surface2DUP: ");
    res = RunGpu(input, inPitch, outputGpu, outPitchGpu, list0, list1, listLength, deblockParam, frame_cu_data, 0/* no matter*/, frame_addr_info, true);
    CHECK_ERR(res);
    res = Compare(outputGpu, outPitchGpu, outputCpu, outPitchCpu, width, height);
    CHECK_ERR(res);
    printf("passed\n");

    delete [] input;
    delete [] outputGpu;
    delete [] outputCpu;
    //delete [] frame_cu_data;
    CM_ALIGNED_FREE(frame_cu_data);

    delete [] frame_addr_info;

    return 0;
}
