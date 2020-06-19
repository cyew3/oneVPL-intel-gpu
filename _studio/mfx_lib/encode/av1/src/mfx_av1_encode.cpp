// Copyright (c) 2014-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include <new>
#include <numeric>
#include <stdexcept>
#include <math.h>
#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include <algorithm>
#include <thread>
#include <fstream>

#include "mfxdefs.h"
#include "mfx_task.h"
#include "vm_interlocked.h"
#include "mfxplugin++.h"

#include "mfx_av1_encode.h"
#include "mfx_av1_defs.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_lookahead.h"
#include "mfx_av1_fei.h"
#include "mfx_av1_core_iface_wrapper.h"
#include "mfx_av1_quant.h"
#include "mfx_av1_bit_count.h"
#include "mfx_av1_dispatcher.h"
#include "mfx_av1_brc.h"
#include "mfx_av1_cdef.h"
#include "mfx_av1_cdef.h"
#include "mfx_av1_deblocking.h"
#include "mfx_av1_tables.h"
#include "mfx_av1_superres.h"
#include "mfx_av1_frame.h"
#include "mfx_av1_dispatcher_wrappers.h"
#include "mfx_av1_scroll_detector.h"

#include <stdint.h>

#ifdef VT_TRACE
#include <ittnotify.h>
#endif

#if PROTOTYPE_GPU_MODE_DECISION
#include "mfx_av1_gpu_mode_decision.h"
#endif // PROTOTYPE_GPU_MODE_DECISION

#ifndef MFX_VA
#define H265FEI_Close(...) (MFX_ERR_NONE)
#define H265FEI_GetSurfaceDimensions(...) (MFX_ERR_NONE)
#define H265FEI_GetSurfaceDimensions_new(...) (MFX_ERR_NONE)
#define H265FEI_Init(...) (MFX_ERR_NONE)
#define H265FEI_ProcessFrameAsync(...) (MFX_ERR_NONE)
#define H265FEI_SyncOperation(...) (MFX_ERR_NONE)
#define H265FEI_DestroySavedSyncPoint(...) (MFX_ERR_NONE)
#endif

using namespace AV1Enc;
using namespace AV1Enc::MfxEnumShortAliases;

static  int32_t GetTileOffset(int32_t tileNum, int32_t sbs, int32_t tileSzLog2) {
    int32_t offset = ((tileNum * sbs) >> tileSzLog2);
    return std::min(offset, sbs);
}

// Find smallest k>=0 such that (blk_size << k) >= target
static int32_t TileLog2(int32_t blk_size, int32_t target) {
    int32_t k;
    for (k = 0; (blk_size << k) < target; k++);
    return k;
}

namespace AV1Enc {
    void SetupTileParamAv1(const AV1VideoParam *par, TileParam *tp, int32_t numTileRows, int32_t numTileCols, int32_t uniform)
    {
        const uint16_t miRows = uint16_t(par->miRows);
        const uint16_t miCols = uint16_t(par->miCols);
        const uint16_t sbCols = uint16_t((miCols + 7) >> 3);
        const uint16_t sbRows = uint16_t((miRows + 7) >> 3);
        const int32_t sbSizeLog2 = 6;
        int32_t maxTileAreaSb = MAX_TILE_AREA >> (2 * sbSizeLog2);

        tp->uniformSpacing = uniform;
        tp->cols = numTileCols;
        tp->rows = numTileRows;
        tp->numTiles = tp->cols * tp->rows;
        tp->log2Cols = TileLog2(1, numTileCols);
        tp->log2Rows = TileLog2(1, numTileRows);
        tp->maxTileWidthSb = MAX_TILE_WIDTH >> sbSizeLog2;
        tp->minLog2Cols = TileLog2(tp->maxTileWidthSb, sbCols);
        tp->maxLog2Cols = TileLog2(1, std::min(sbCols, MAX_TILE_COLS));
        tp->maxLog2Rows = TileLog2(1, std::min(sbRows, MAX_TILE_ROWS));
        tp->minLog2Tiles = TileLog2(maxTileAreaSb, sbCols * sbRows);
        tp->minLog2Tiles = std::max(tp->minLog2Tiles, tp->minLog2Cols);

        if (uniform) {
            const int32_t tileWidth  = (sbCols + tp->cols - 1) >> tp->log2Cols;
            const int32_t tileHeight = (sbRows + tp->rows - 1) >> tp->log2Rows;

            for (int32_t i = 0; i < tp->cols; i++) {
                tp->colStart[i] = uint16_t(i * tileWidth);
                tp->colEnd[i]   = std::min(uint16_t(tp->colStart[i] + tileWidth), sbCols);
                tp->colWidth[i] = uint16_t(tp->colEnd[i] - tp->colStart[i]);
            }

            tp->minLog2Rows = std::max(tp->minLog2Tiles - tp->log2Cols, 0);
            tp->maxTileHeightSb = sbRows >> tp->minLog2Rows;

            for (int32_t i = 0; i < tp->rows; i++) {
                tp->rowStart[i]  = uint16_t(i * tileHeight);
                tp->rowEnd[i]    = std::min(uint16_t(tp->rowStart[i] + tileHeight), sbRows);
                tp->rowHeight[i] = uint16_t(tp->rowEnd[i] - tp->rowStart[i]);
            }
        } else {
            int32_t widestTileWidth = 0;
            for (int32_t i = 0; i < tp->cols; i++) {
                tp->colStart[i] = uint16_t((i + 0) * sbCols / tp->cols);
                tp->colEnd[i]   = uint16_t((i + 1) * sbCols / tp->cols);
                tp->colWidth[i] = uint16_t(tp->colEnd[i] - tp->colStart[i]);
                widestTileWidth = std::max(widestTileWidth, (int32_t)tp->colWidth[i]);
            }

            if (tp->minLog2Tiles)
                maxTileAreaSb >>= (tp->minLog2Tiles + 1);
            tp->maxTileHeightSb = std::max(1, maxTileAreaSb / widestTileWidth);

            for (int32_t i = 0; i < tp->rows; i++) {
                tp->rowStart[i]  = uint16_t((i + 0) * sbRows / tp->rows);
                tp->rowEnd[i]    = uint16_t((i + 1) * sbRows / tp->rows);
                tp->rowHeight[i] = uint16_t(tp->rowEnd[i] - tp->rowStart[i]);
            }
        }

        for (int32_t i = 0; i < tp->cols; i++) {
            // 8x8 units
            tp->miColStart[i] = uint16_t(tp->colStart[i] << 3);
            tp->miColEnd[i]   = std::min(uint16_t(tp->colEnd[i] << 3), miCols);
            tp->miColWidth[i] = uint16_t(tp->miColEnd[i] - tp->miColStart[i]);

            for (int32_t j = tp->colStart[i]; j < tp->colEnd[i]; j++)
                tp->mapSb2TileCol[j] = i;
        }

        for (int32_t i = 0; i < tp->rows; i++) {
            // 8x8 units
            tp->miRowStart[i]  = uint16_t(tp->rowStart[i] << 3);
            tp->miRowEnd[i]    = std::min(uint16_t(tp->rowEnd[i] << 3), miRows);
            tp->miRowHeight[i] = uint16_t(tp->miRowEnd[i] - tp->miRowStart[i]);

            for (int32_t j = tp->rowStart[i]; j < tp->rowEnd[i]; j++)
                tp->mapSb2TileRow[j] = i;
        }
    }
};  // namespace AV1Enc

namespace {
#if TT_TRACE || defined(VT_TRACE) || TASK_LOG_ENABLE
    const char *TT_NAMES[] = {
        "NEWF",
        "SBMT",
        "WAIT",
        "ENC",
        "DBLK_CDEF",
        "CDEF_SYNC",
        "CDEF_APPLY",
        "ECOMP",
        "HUB",
        "LA_START",
        "LA",
        "LA_END",
        "COMP",
        "REPEAT",
        "PHEAD",
        "PTILE",
        "PROW",
        "SCROLL_START",
        "SCROLL",
        "SCROLL_END",
    };

    const char *GetFeiOpName(int32_t feiOp) {
        switch (feiOp) {
        case MFX_FEI_H265_OP_NOP:               return "NOP";
        case MFX_FEI_H265_OP_GPU_COPY_SRC:      return "CPSRC";
        case MFX_FEI_H265_OP_GPU_COPY_REF:      return "CPREF";
        case MFX_FEI_H265_OP_INTRA_MODE:        return "GRAD";
        case MFX_FEI_H265_OP_INTRA_DIST:        return "IDIST";
        case MFX_FEI_H265_OP_INTER_ME:          return "ME";
        case MFX_FEI_H265_OP_INTERPOLATE:       return "INTERP";
        case MFX_FEI_H265_OP_POSTPROC:          return "PP";
        case MFX_FEI_H265_OP_BIREFINE:          return "BIREFINE";
        case MFX_FEI_H265_OP_DEBLOCK:           return "PP_DBLK";
        default:                                return "<unk>";
        }
    }

    const char *NUMBERS[128] = {
        "000", "001", "002", "003", "004", "005", "006", "007", "008", "009", "010", "011", "012", "013", "014", "015",
        "016", "017", "018", "019", "020", "021", "022", "023", "024", "025", "026", "027", "028", "029", "030", "031",
        "032", "033", "034", "035", "036", "037", "038", "039", "040", "041", "042", "043", "044", "045", "046", "047",
        "048", "049", "050", "051", "052", "053", "054", "055", "056", "057", "058", "059", "060", "061", "062", "063",
        "064", "065", "066", "067", "068", "069", "070", "071", "072", "073", "074", "075", "076", "077", "078", "079",
        "080", "081", "082", "083", "084", "085", "086", "087", "088", "089", "090", "091", "092", "093", "094", "095",
        "096", "097", "098", "099", "100", "101", "102", "103", "104", "105", "106", "107", "108", "109", "110", "111",
        "112", "113", "114", "115", "116", "117", "118", "119", "120", "121", "122", "123", "124", "125", "126", "127"
    };
#endif //TT_TRACE

#if TASK_LOG_ENABLE
struct TaskLog {
    int16_t action;
    int16_t poc;                     // for all tasks, useful in debug
    int16_t col;             // for encode, postproc, lookahead
    int16_t row;
    int16_t num_pend;
    LONGLONG start;
    LONGLONG end;
    LONGLONG threadId;
};

static TaskLog *taskLog;
static std::atomic<uint32_t> taskLogIdx;

static void TaskLogInit()
{
    taskLogIdx = 0;
    if (taskLog == NULL)
    taskLog = new TaskLog[TASK_LOG_NUM];
}

static void TaskLogClose()
{
    if (taskLog)
    delete[] taskLog;
}

static uint32_t TaskLogStart(ThreadingTask *task, int num_pend)
{
    LARGE_INTEGER t;
    uint32_t idx = taskLogIdx.fetch_add(1)+1;
    if (idx > TASK_LOG_NUM) return idx;
    idx--;

    if (task) {
        taskLog[idx].action = task->action;
        taskLog[idx].poc = task->poc;
        taskLog[idx].col = task->col;
        taskLog[idx].row = task->row;
        taskLog[idx].num_pend = num_pend;
    } else {
        taskLog[idx].action = -1;
        taskLog[idx].poc = num_pend;
    }
    taskLog[idx].threadId = GetCurrentThreadId();

    QueryPerformanceCounter(&t);
    taskLog[idx].start = t.QuadPart;
    return idx;
}

static void TaskLogStop(uint32_t idx)
{
    LARGE_INTEGER t;
    if (idx >= TASK_LOG_NUM) return;
    QueryPerformanceCounter(&t);
    taskLog[idx].end = t.QuadPart;

    if (taskLog[idx].action >= 0) {
        printf("%5d %.3f-%.3f: %s %d %d %d [%d]\n",
            (int32_t)taskLog[idx].threadId,
            0.0,//(double)(taskLog[idx].start - start) * 1000.0 / freq,
            0.0,//(double)(taskLog[idx].end - start) * 1000.0 / freq,
            TT_NAMES[taskLog[idx].action],
            taskLog[idx].poc,
            taskLog[idx].row,
            taskLog[idx].col,
            taskLog[idx].num_pend);
    } else {
            printf("0 %.3f-%.3f: GLOBAL TASK %d\n",
                0.0,
                0.0,
                taskLog[idx].poc);
    }

    fflush(stdout);
}

static void TaskLogDump()
{
    FILE *fp = fopen("tasklog-bin.txt","wb");
    LARGE_INTEGER t1;
    QueryPerformanceFrequency(&t1);
    LONGLONG freq = t1.QuadPart;
    fwrite(&freq, sizeof(LONGLONG), 1, fp);
    fwrite(taskLog, sizeof(TaskLog), taskLogIdx.load(), fp);
    fclose(fp);
    fp = fopen("tasklog.txt","wt");
    LONGLONG start = taskLog[0].start;
    for (uint32_t ii = 0; ii < taskLogIdx.load(); ii++) {
        if (taskLog[ii].action >= 0) {
            if (taskLog[ii].action == TT_INIT_NEW_FRAME) taskLog[ii].row = taskLog[ii].col = 0;
            fprintf(fp, "%5d %.3f-%.3f: %s %d %d %d [%d]\n",
                (int32_t)taskLog[ii].threadId,
                (double)(taskLog[ii].start - start) * 1000.0 / freq,
                (double)(taskLog[ii].end - start) * 1000.0 / freq,
                TT_NAMES[taskLog[ii].action],
                taskLog[ii].poc,
                taskLog[ii].row,
                taskLog[ii].col,
                taskLog[ii].num_pend);
        } else {
            fprintf(fp, "0 %.3f-%.3f: GLOBAL TASK %d\n",
                (double)(taskLog[ii].start - start) * 1000.0 / freq,
                (double)(taskLog[ii].end - start) * 1000.0 / freq,
                taskLog[ii].poc);
        }
    }
    fclose(fp);
}
#endif // TASK_LOG_ENABLE
#ifdef VT_TRACE
    static mfxTraceStaticHandle _trace_static_handle2[64][TT_COUNT];
//    static char _trace_task_names[64][13][32];
#endif

    const int32_t MYRAND_MAX = 0xffff;
    int32_t myrand()
    {
        // feedback polynomial: x^16 + x^14 + x^13 + x^11 + 1
        static uint16_t val = 0xACE1u;
        uint32_t bit = ((val >> 0) ^ (val >> 2) ^ (val >> 3) ^ (val >> 5)) & 1;
        val = uint16_t((val >> 1) | (bit << 15));
        return val;
    }

    const CostType tab_defaultLambdaCorrIP[8]   = {1.0f, 1.06f, 1.09f, 1.13f, 1.17f, 1.25f, 1.27f, 1.28f};

    inline int32_t DivUp(int32_t n, int32_t d) { return (n + d - 1) / d; }


    void ConvertToInternalParam(AV1VideoParam &intParam, const mfxVideoParam &mfxParam)
    {
        const mfxInfoMFX &mfx = mfxParam.mfx;
        const mfxFrameInfo &fi = mfx.FrameInfo;
        const mfxExtOpaqueSurfaceAlloc &opaq = GetExtBuffer(mfxParam);
        const mfxExtCodingOptionAV1E &optHevc = GetExtBuffer(mfxParam);
        const mfxExtHEVCParam &hevcParam = GetExtBuffer(mfxParam);
        mfxExtDumpFiles &dumpFiles = GetExtBuffer(mfxParam);
        const mfxExtCodingOption2 &opt2 = GetExtBuffer(mfxParam);
        const mfxExtCodingOption3 &opt3 = GetExtBuffer(mfxParam);
        const mfxExtEncoderROI &roi = GetExtBuffer(mfxParam);
        const mfxExtAvcTemporalLayers &tlayers = GetExtBuffer(mfxParam);
        const mfxExtVP9Param &vp9param = GetExtBuffer(mfxParam);
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        const mfxExtAV1Param &av1param = GetExtBuffer(mfxParam);
#endif

        const uint32_t maxCUSize = 1 << optHevc.Log2MaxCUSize;

        Zero(intParam);
        intParam.encodedOrder = (uint8_t)mfxParam.mfx.EncodedOrder;
        // uncomment for test EncCtrl in display order
        //intParam.encodedOrder = 0;

#if USE_SRC10ENC8PAK10
        intParam.fourcc = MFX_FOURCC_NV12;
        intParam.bitDepthLuma = 8;
        intParam.bitDepthChroma = 8;
        intParam.chromaFormatIdc = MFX_CHROMAFORMAT_YUV420;
        if (fi.FourCC == MFX_FOURCC_P010) {
            intParam.src10Enable = 1;
        }
#else
        intParam.fourcc = fi.FourCC;
        intParam.bitDepthLuma = 8;
        intParam.bitDepthChroma = 8;
        intParam.chromaFormatIdc = MFX_CHROMAFORMAT_YUV420;
        if (fi.FourCC == MFX_FOURCC_P010 || fi.FourCC == MFX_FOURCC_P210) {
            intParam.bitDepthLuma = 10;
            intParam.bitDepthChroma = 10;
        }
#endif
        intParam.chromaFormatIdc = (uint8_t)fi.ChromaFormat;
        intParam.bitDepthLumaShift = intParam.bitDepthLuma - 8;
        intParam.bitDepthChromaShift = intParam.bitDepthChroma - 8;
        intParam.chroma422 = (intParam.chromaFormatIdc == MFX_CHROMAFORMAT_YUV422);
        intParam.chromaShiftW = (intParam.chromaFormatIdc != MFX_CHROMAFORMAT_YUV444);
        intParam.chromaShiftH = (intParam.chromaFormatIdc == MFX_CHROMAFORMAT_YUV420);
        intParam.chromaShiftWInv = 1 - intParam.chromaShiftW;
        intParam.chromaShiftHInv = 1 - intParam.chromaShiftH;
        intParam.chromaShift = intParam.chromaShiftW + intParam.chromaShiftH;

        intParam.Log2MaxCUSize = optHevc.Log2MaxCUSize;
        intParam.MaxCUDepth = optHevc.MaxCUDepth;
        intParam.QuadtreeTULog2MaxSize = optHevc.QuadtreeTULog2MaxSize;
        intParam.QuadtreeTULog2MinSize = optHevc.QuadtreeTULog2MinSize;
        intParam.QuadtreeTUMaxDepthIntra = optHevc.QuadtreeTUMaxDepthIntra;
        intParam.QuadtreeTUMaxDepthInter = optHevc.QuadtreeTUMaxDepthInter;
        intParam.QuadtreeTUMaxDepthInterRD = optHevc.QuadtreeTUMaxDepthInterRD;
        intParam.partModes = (uint8_t)optHevc.PartModes;
        intParam.TMVPFlag = (optHevc.TMVP == ON);
        intParam.QPI = (int8_t)(mfx.QPI - optHevc.ConstQpOffset);
        intParam.QPP = (int8_t)(mfx.QPP - optHevc.ConstQpOffset);
        intParam.QPB = (int8_t)(mfx.QPB - optHevc.ConstQpOffset);

        intParam.GopPicSize = mfx.GopPicSize;
        intParam.GopRefDist = mfx.GopRefDist;
        intParam.IdrInterval = mfx.IdrInterval;

        // FIX TO MEET VP9 SPEC
        intParam.IdrInterval = 1;

        intParam.GopClosedFlag = !!(mfx.GopOptFlag & MFX_GOP_CLOSED);
        intParam.GopStrictFlag = !!(mfx.GopOptFlag & MFX_GOP_STRICT);
        intParam.BiPyramidLayers = (optHevc.BPyramid == ON) ? (H265_CeilLog2(intParam.GopRefDist) + 1) : 1;

        const int32_t numRefFrame = MIN(4, mfx.NumRefFrame);
        intParam.longGop = (optHevc.BPyramid == ON && intParam.GopRefDist == 16 && numRefFrame == 5);
        intParam.GeneralizedPB = (opt3.GPB != OFF);
        intParam.AdaptiveRefs = (optHevc.AdaptiveRefs == ON);
        intParam.NumRefFrameB  = (uint8_t)optHevc.NumRefFrameB;
        if (intParam.NumRefFrameB < 2)
            intParam.NumRefFrameB = (uint8_t)numRefFrame;

        if (mfx.GopRefDist == 1) {
            if (tlayers.Layer[1].Scale == 2) {
                intParam.NumRefLayers = 2;
                if (tlayers.Layer[2].Scale == 4) intParam.NumRefLayers = 3;
            } else {
                intParam.NumRefLayers = 1;
            }
        } else {
            intParam.NumRefLayers = (uint8_t)optHevc.NumRefLayers;
        }
        //if (intParam.NumRefLayers < 2 || intParam.BiPyramidLayers<2)
        //    intParam.NumRefLayers = 2;
        intParam.IntraMinDepthSC = (uint8_t)optHevc.IntraMinDepthSC - 1;
        intParam.InterMinDepthSTC = (uint8_t)optHevc.InterMinDepthSTC - 1;
        intParam.MotionPartitionDepth = (uint8_t)optHevc.MotionPartitionDepth - 1;
        intParam.MaxDecPicBuffering = MAX(numRefFrame, intParam.BiPyramidLayers);
        if (fi.PicStruct != PROGR)
            intParam.MaxDecPicBuffering *= 2;
        intParam.MaxRefIdxP[0] = numRefFrame;
        intParam.MaxRefIdxP[1] = intParam.GeneralizedPB ? numRefFrame : 0;
        intParam.MaxRefIdxB[0] = 0;
        intParam.MaxRefIdxB[1] = 0;

        if (intParam.GopRefDist > 1) {
            if (intParam.longGop) {
                intParam.MaxRefIdxB[0] = 2;
                intParam.MaxRefIdxB[1] = 2;
            }
            else if (intParam.BiPyramidLayers > 1) {
                uint16_t NumRef = (uint16_t)std::min((int32_t)intParam.NumRefFrameB, numRefFrame);
                if (fi.PicStruct != PROGR)
                    NumRef = (uint16_t)std::min(2*intParam.NumRefFrameB, numRefFrame);
                intParam.MaxRefIdxB[0] = (NumRef + 1) / 2;
                intParam.MaxRefIdxB[1] = std::max(1, (NumRef + 0) / 2);
            } else {
                intParam.MaxRefIdxB[0] = numRefFrame - 1;
                intParam.MaxRefIdxB[1] = 1;
            }
        }

        intParam.PGopPicSize = (intParam.GopRefDist == 1 && intParam.MaxRefIdxP[0] > 1 && opt3.PRefType != 1) ? PGOP_PIC_SIZE : 1;

        intParam.AnalyseFlags = 0;
        if (optHevc.AnalyzeChroma == ON) intParam.AnalyseFlags |= HEVC_ANALYSE_CHROMA;
        if (optHevc.CostChroma == ON)    intParam.AnalyseFlags |= HEVC_COST_CHROMA;

        intParam.SplitThresholdStrengthCUIntra = (uint8_t)optHevc.SplitThresholdStrengthCUIntra - 1;
        intParam.SplitThresholdStrengthTUIntra = (uint8_t)optHevc.SplitThresholdStrengthTUIntra - 1;
        intParam.SplitThresholdStrengthCUInter = (uint8_t)optHevc.SplitThresholdStrengthCUInter - 1;
        intParam.SplitThresholdTabIndex        = (uint8_t)optHevc.SplitThresholdTabIndex;
        intParam.SplitThresholdMultiplier      = optHevc.SplitThresholdMultiplier / 10.0f;

        intParam.FastInterp = (optHevc.FastInterp == ON);
        intParam.cpuFeature = (uint8_t)optHevc.CpuFeature;
        intParam.IntraChromaRDO = (optHevc.IntraChromaRDO == ON);
        intParam.SBHFlag = (optHevc.SignBitHiding == ON);
        intParam.RDOQFlag = (optHevc.RDOQuant == ON);
        intParam.rdoqChromaFlag = (optHevc.RDOQuantChroma == ON);
        intParam.FastCoeffCost = (optHevc.FastCoeffCost == ON);
        intParam.rdoqCGZFlag = (optHevc.RDOQuantCGZ == ON);
        intParam.SAOFlag = (optHevc.SAO == ON);
        intParam.SAOChromaFlag = (optHevc.SAOChroma == ON);
        intParam.num_threads = mfx.NumThread;
        if (intParam.GopRefDist == 1) {
            if (intParam.NumRefLayers >= 3) {
                intParam.refLayerLimit[0] = 0;
                intParam.refLayerLimit[1] = 2;
                intParam.refLayerLimit[2] = 2;
                intParam.refLayerLimit[3] = 2;
            }
            else if (intParam.NumRefLayers == 2) {
                intParam.refLayerLimit[0] = 1;
                intParam.refLayerLimit[1] = 1;
                intParam.refLayerLimit[2] = 1;
                intParam.refLayerLimit[3] = 2;
            }
            else {
                intParam.refLayerLimit[0] = 4;
                intParam.refLayerLimit[1] = 4;
                intParam.refLayerLimit[2] = 4;
                intParam.refLayerLimit[3] = 4;
            }
        }
        else {
            if (intParam.NumRefLayers == 4) {
                intParam.refLayerLimit[0] = 0;
                intParam.refLayerLimit[1] = 1;
                intParam.refLayerLimit[2] = 1;
                intParam.refLayerLimit[3] = 2;
            }
            else if (intParam.NumRefLayers == 3) {
                intParam.refLayerLimit[0] = 1;
                intParam.refLayerLimit[1] = 1;
                intParam.refLayerLimit[2] = 1;
                intParam.refLayerLimit[3] = 2;
            }
            else {
                intParam.refLayerLimit[0] = 4;
                intParam.refLayerLimit[1] = 4;
                intParam.refLayerLimit[2] = 4;
                intParam.refLayerLimit[3] = 4;
            }
        }

        intParam.num_cand_0[0][2] = (uint8_t)optHevc.IntraNumCand0_2;
        intParam.num_cand_0[0][3] = (uint8_t)optHevc.IntraNumCand0_3;
        intParam.num_cand_0[0][4] = (uint8_t)optHevc.IntraNumCand0_4;
        intParam.num_cand_0[0][5] = (uint8_t)optHevc.IntraNumCand0_5;
        intParam.num_cand_0[0][6] = (uint8_t)optHevc.IntraNumCand0_6;
        Copy(intParam.num_cand_0[1], intParam.num_cand_0[0]);
        Copy(intParam.num_cand_0[2], intParam.num_cand_0[0]);
        Copy(intParam.num_cand_0[3], intParam.num_cand_0[0]);
        intParam.num_cand_1[2] = (uint8_t)optHevc.IntraNumCand1_2;
        intParam.num_cand_1[3] = (uint8_t)optHevc.IntraNumCand1_3;
        intParam.num_cand_1[4] = (uint8_t)optHevc.IntraNumCand1_4;
        intParam.num_cand_1[5] = (uint8_t)optHevc.IntraNumCand1_5;
        intParam.num_cand_1[6] = (uint8_t)optHevc.IntraNumCand1_6;
        intParam.num_cand_2[2] = (uint8_t)optHevc.IntraNumCand2_2;
        intParam.num_cand_2[3] = (uint8_t)optHevc.IntraNumCand2_3;
        intParam.num_cand_2[4] = (uint8_t)optHevc.IntraNumCand2_4;
        intParam.num_cand_2[5] = (uint8_t)optHevc.IntraNumCand2_5;
        intParam.num_cand_2[6] = (uint8_t)optHevc.IntraNumCand2_6;

        intParam.LowresFactor = (uint8_t)(optHevc.LowresFactor - 1);
        intParam.FullresMetrics = 0;
        intParam.DeltaQpMode = (uint8_t)(optHevc.DeltaQpMode - 1);
        if (mfx.RateControlMethod == CQP && opt3.EnableMBQP == ADAPT) {
            intParam.DeltaQpMode |= AMT_DQP_PSY;
        }
        if (mfx.EncodedOrder) {
            intParam.DeltaQpMode = intParam.DeltaQpMode&AMT_DQP_CAQ;
        }
#ifdef AMT_HROI_PSY_AQ
        //if(intParam.DeltaQpMode&AMT_DQP_HROI) {
        //    intParam.DeltaQpMode |= AMT_DQP_PSY;    // Now triggers EROI
        //}
        if (intParam.chromaFormatIdc != MFX_CHROMAFORMAT_YUV420) {
            intParam.DeltaQpMode = intParam.DeltaQpMode&~AMT_DQP_HROI;
        }
        if (intParam.bitDepthLuma != 8) {
            intParam.DeltaQpMode = intParam.DeltaQpMode&~AMT_DQP_HROI;
        }
        if (fi.PicStruct != PROGR) {
            intParam.DeltaQpMode = intParam.DeltaQpMode&~AMT_DQP_PSY_HROI;
        }
#endif
        intParam.LambdaCorrection = tab_defaultLambdaCorrIP[(intParam.DeltaQpMode&AMT_DQP_CAQ)?mfx.TargetUsage:0];
        intParam.ZeroDelay = ((mfx.GopPicSize == 1 || mfx.GopRefDist == 1) && mfxParam.AsyncDepth == 1) ? 1 : 0;
        intParam.SceneCut = (mfx.EncodedOrder || mfx.GopPicSize==1) ? 0 : 1;
        intParam.AdaptiveI = (!intParam.SceneCut) ? 0 : (opt2.AdaptiveI == ON);
        intParam.EnableALTR = intParam.SceneCut ? (opt3.ExtBrcAdaptiveLTR == ON) : 0;
        if (intParam.EnableALTR) {
            if (intParam.NumRefLayers == 1 && numRefFrame < 2) intParam.EnableALTR = 0;
            if (intParam.NumRefLayers == 2 && numRefFrame < 3) intParam.EnableALTR = 0;
            if (intParam.NumRefLayers == 3 && numRefFrame < 4) intParam.EnableALTR = 0;
        }
        intParam.LTRConfidenceLevel = opt3.ScenarioInfo == MFX_SCENARIO_VIDEO_CONFERENCE ? 3 : 1;

        intParam.AnalyzeCmplx = /*mfx.EncodedOrder ? 0 : */(uint8_t)(optHevc.AnalyzeCmplx - 1);
        //if(intParam.AnalyzeCmplx && intParam.BiPyramidLayers > 1 && intParam.LowresFactor && mfx.RateControlMethod != CQP)
        if (intParam.AnalyzeCmplx && intParam.LowresFactor && mfx.RateControlMethod != CQP)
            intParam.FullresMetrics = 1;

#ifdef LOW_COMPLX_PAQ
        if (intParam.LowresFactor && (intParam.DeltaQpMode&AMT_DQP_PAQ))
            intParam.FullresMetrics = 1;
#endif

        intParam.RateControlDepth = optHevc.RateControlDepth;
        intParam.TryIntra = (uint8_t)optHevc.TryIntra;
        intParam.FastAMPSkipME = (uint8_t)optHevc.FastAMPSkipME;
        intParam.FastAMPRD = (uint8_t)optHevc.FastAMPRD;
        intParam.SkipMotionPartition = (uint8_t)optHevc.SkipMotionPartition;
        intParam.cmIntraThreshold = optHevc.CmIntraThreshold;
        intParam.tuSplitIntra = optHevc.TUSplitIntra;
        intParam.cuSplit = optHevc.CUSplit;
        intParam.intraAngModes[0] = optHevc.IntraAngModes;
        intParam.intraAngModes[1] = optHevc.IntraAngModesP;
        intParam.intraAngModes[2] = optHevc.IntraAngModesBRef;
        intParam.intraAngModes[3] = optHevc.IntraAngModesBnonRef;
        intParam.fastSkip = (optHevc.FastSkip == ON);
        intParam.SkipCandRD = (optHevc.SkipCandRD == ON);
        intParam.fastCbfMode = (optHevc.FastCbfMode == ON);
        intParam.hadamardMe = optHevc.HadamardMe;
        intParam.TMVPFlag = (optHevc.TMVP == ON);
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        intParam.cdefFlag = (av1param.EnableCdef == ON);
        intParam.superResFlag = (av1param.EnableSuperres == ON);
#else
        intParam.cdefFlag = (optHevc.CDEF == ON);
        intParam.superResFlag = (optHevc.SRMode == ON);
#endif

#if USE_CMODEL_PAK
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        intParam.lrFlag = (av1param.EnableRestoration == ON);
#else
        intParam.lrFlag = (optHevc.LRMode == ON);
#endif
#else
        intParam.lrFlag = 0;
#endif

        intParam.cflFlag = (intParam.src10Enable == 0) ? (optHevc.CFLMode == ON) : 0;
        intParam.screenMode = (uint8_t)optHevc.ScreenMode;
        if (intParam.screenMode == 4) {
            intParam.screenMode = 7;// auto mode
        } else if (intParam.screenMode == 5) {
            intParam.screenMode = 0;// disable
        }
        if (intParam.src10Enable)
            intParam.screenMode = 0;// disable
        intParam.deblockingFlag = (optHevc.Deblocking == ON);
        intParam.deblockingSharpness = 0;
        intParam.deblockingLevelMethod = (uint8_t)(optHevc.DeblockingLevelMethod - 1);
        intParam.deblockBordersFlag = (intParam.deblockingFlag && optHevc.DeblockBorders == ON);
        intParam.saoOpt = optHevc.SaoOpt;
        intParam.saoSubOpt = optHevc.SaoSubOpt;
        intParam.patternIntPel = optHevc.PatternIntPel;
        intParam.patternSubPel = optHevc.PatternSubPel;
        intParam.numBiRefineIter = optHevc.NumBiRefineIter;
        intParam.puDecisionSatd = (optHevc.PuDecisionSatd == ON);
        intParam.minCUDepthAdapt = (optHevc.MinCUDepthAdapt == ON);
        intParam.maxCUDepthAdapt = (optHevc.MaxCUDepthAdapt == ON);
        intParam.cuSplitThreshold = optHevc.CUSplitThreshold;

        intParam.MaxTrSize = 1 << intParam.QuadtreeTULog2MaxSize;
        intParam.MaxCUSize = maxCUSize;

        // Computed sizes
        int ReMiW = ((fi.CropW + 7) >> 3) * 8;
        int ReMiH = ((fi.CropH + 7) >> 3) * 8;
        intParam.sourceWidth = fi.CropW;
        intParam.sourceHeight = fi.CropH;
        intParam.Width = ReMiW;
        // simple SuperResolution support: encoder works with (W/2 x H), but SequenceHeader writes (W x H)
        if (optHevc.SRMode == ON) {
            int denom = SUPERRES_SCALE_DENOMINATOR_DEFAULT;
            intParam.Width = (intParam.Width * SCALE_NUMERATOR + denom / 2) / denom;
        }
        intParam.CropLeft = fi.CropX;
        intParam.CropRight = intParam.Width - fi.CropW - fi.CropX;
        //if (fi.PicStruct == TFF || fi.PicStruct == BFF) {
        //    intParam.Height = (fi.CropH / 2 + 7) & ~7;
        //    intParam.CropTop = 0;
        //    intParam.CropBottom = 0;
        //} else 
        {
            intParam.Height = ReMiH;
            intParam.CropTop = fi.CropY;
            intParam.CropBottom = intParam.Height - fi.CropH - fi.CropY;
        }

        intParam.enableCmFlag = (optHevc.EnableCm == ON);
        intParam.enableCmPostProc = 0;//intParam.deblockingFlag && intParam.enableCmFlag && (intParam.bitDepthLuma == 8) && (intParam.chromaFormatIdc == MFX_CHROMAFORMAT_YUV420);
        intParam.enablePostProcFrame = intParam.deblockingFlag;

        intParam.CmInterpFlag = (optHevc.EnableCmInterp == ON);

        intParam.m_framesInParallel = optHevc.FramesInParallel;

        // intParam.m_lagBehindRefRows = 3;
        // intParam.m_meSearchRangeY = (intParam.m_lagBehindRefRows - 1) * intParam.MaxCUSize; // -1 row due to deblocking lag
        // New thread model sets up post proc (deblock) as ref task dependency; no deblocking lag, encode is leading.
        /*intParam.m_lagBehindRefRows = 2;
        intParam.m_meSearchRangeY = intParam.m_lagBehindRefRows * intParam.MaxCUSize;
        if (optHevc.EnableCm == ON)*/ {
            intParam.m_lagBehindRefRows = (intParam.Height + maxCUSize - 1) / maxCUSize + 1; // for GACC - PicHeightInCtbs+1
            intParam.m_meSearchRangeY = intParam.m_lagBehindRefRows * maxCUSize;
        }

        intParam.AddCUDepth  = 0;
        while ((intParam.MaxCUSize >> intParam.MaxCUDepth) > (1 << (intParam.QuadtreeTULog2MinSize + intParam.AddCUDepth)))
            intParam.AddCUDepth++;

        intParam.MaxCUDepth += intParam.AddCUDepth;
        intParam.AddCUDepth++;

        intParam.MinCUSize = intParam.MaxCUSize >> (intParam.MaxCUDepth - intParam.AddCUDepth);
        intParam.MinTUSize = intParam.MaxCUSize >> intParam.MaxCUDepth;

        intParam.Log2NumPartInCU = intParam.MaxCUDepth << 1;
        intParam.NumPartInCU = 1 << intParam.Log2NumPartInCU;
        intParam.NumPartInCUSize  = 1 << intParam.MaxCUDepth;
        intParam.PicWidthInMinCbs = intParam.Width / intParam.MinCUSize;
        intParam.PicHeightInMinCbs = intParam.Height / intParam.MinCUSize;
        intParam.PicWidthInCtbs = (intParam.Width + intParam.MaxCUSize - 1) / intParam.MaxCUSize;
        intParam.PicHeightInCtbs = (intParam.Height + intParam.MaxCUSize - 1) / intParam.MaxCUSize;

        intParam.num_thread_structs = intParam.num_threads;

        // deltaQP control
        intParam.MaxCuDQPDepth = 0;
        intParam.m_maxDeltaQP = 0;
        intParam.UseDQP = 0;

        intParam.ctrlMBQP = 0;
        if (opt3.EnableMBQP == ON) {
            intParam.ctrlMBQP = 1;
            //intParam.DeltaQpMode = 0;
        }

        intParam.numRoi = roi.NumROI;

        if (intParam.numRoi > 0) {
            int32_t maxAbsDeltaQp = -1;
            if (mfx.RateControlMethod == CBR)
                maxAbsDeltaQp = 1 * DEFAULT_DELTA_Q_RES;
            else if (mfx.RateControlMethod == VBR)
                maxAbsDeltaQp = 2 * DEFAULT_DELTA_Q_RES;
            else if (mfx.RateControlMethod != CQP)
                maxAbsDeltaQp = 3 * DEFAULT_DELTA_Q_RES;
            for (int32_t i = 0; i < intParam.numRoi; i++) {
                intParam.roi[i].left = (uint16_t)roi.ROI[i].Left;
                intParam.roi[i].top = (uint16_t)roi.ROI[i].Top;
                intParam.roi[i].right = (uint16_t)roi.ROI[i].Right;
                intParam.roi[i].bottom = (uint16_t)roi.ROI[i].Bottom;
                intParam.roi[i].priority = (uint16_t)roi.ROI[i].Priority;
                if (maxAbsDeltaQp > 0)
                    intParam.roi[i].priority = (int16_t)Saturate(-maxAbsDeltaQp, maxAbsDeltaQp, intParam.roi[i].priority);
            }
            intParam.DeltaQpMode = 0;
        }

        if (intParam.DeltaQpMode) {
            intParam.MaxCuDQPDepth = 0;
            intParam.m_maxDeltaQP = 0;
            intParam.UseDQP = 1;
#ifdef AMT_HROI_PSY_AQ
            if (intParam.DeltaQpMode & AMT_DQP_PSY_HROI) {
                intParam.AnalyzeCmplx = 1;
                intParam.FullresMetrics = 1;
            }
#endif
        }
        if (intParam.MaxCuDQPDepth > 0 || intParam.m_maxDeltaQP > 0 || intParam.numRoi > 0 || intParam.ctrlMBQP)
            intParam.UseDQP = 1;

        if (intParam.UseDQP)
            intParam.MinCuDQPSize = intParam.MaxCUSize >> intParam.MaxCuDQPDepth;
        else
            intParam.MinCuDQPSize = intParam.MaxCUSize;

        intParam.QuickStartPFrame = (intParam.DeltaQpMode || mfx.RateControlMethod != CQP) ? 0 : 1;
#ifndef ZERO_DELAY_ANALYZE_CMPLX
        if (intParam.ZeroDelay && intParam.AnalyzeCmplx)
            intParam.ZeroDelay = 0;
#endif

        intParam.NumMinTUInMaxCU = intParam.MaxCUSize >> intParam.QuadtreeTULog2MinSize;

        intParam.picStruct = fi.PicStruct;
        intParam.FrameRateExtN = fi.FrameRateExtN;
        intParam.FrameRateExtD = fi.FrameRateExtD;
        if (fi.PicStruct == TFF || fi.PicStruct == BFF) {
            if (intParam.FrameRateExtN < 0xffffffff / 2)
                intParam.FrameRateExtN *= 2;
            else
                intParam.FrameRateExtD = MAX(1, intParam.FrameRateExtD / 2);
        }

        intParam.hrdPresentFlag = (opt2.DisableVUI == OFF && (mfx.RateControlMethod == CBR || mfx.RateControlMethod == VBR));
        intParam.cbrFlag = (mfx.RateControlMethod == CBR);

        if (mfx.RateControlMethod != CQP) {
            intParam.targetBitrate = MIN(0xfffffed8, (uint64_t)mfx.TargetKbps * mfx.BRCParamMultiplier * 1000);
            if (intParam.hrdPresentFlag) {
                intParam.cpbSize = MIN(0xffffE380, (uint64_t)mfx.BufferSizeInKB * mfx.BRCParamMultiplier * 8000);
                intParam.initDelay = MIN(0xffffE380, (uint64_t)mfx.InitialDelayInKB * mfx.BRCParamMultiplier * 8000);
                intParam.hrdBitrate = intParam.cbrFlag ? intParam.targetBitrate : MIN(0xfffffed8, (uint64_t)mfx.MaxKbps * mfx.BRCParamMultiplier * 1000);
            }
            if (opt2.MaxFrameSize) {
                intParam.MaxFrameSizeInBits = opt2.MaxFrameSize * 8;  // bits
                const uint32_t avgFrameInBits = (double)intParam.targetBitrate * fi.FrameRateExtD / fi.FrameRateExtN;
                if (intParam.PGopPicSize > 1 && intParam.MaxFrameSizeInBits < 2 * avgFrameInBits)
                    intParam.PGopPicSize = 1;
#ifdef AMT_MAX_FRAME_SIZE
                intParam.FullresMetrics = 1;
#endif
            }
        }

        intParam.RepackForMaxFrameSize = (optHevc.RepackForMaxFrameSize == ON);

        intParam.AspectRatioW  = fi.AspectRatioW;
        intParam.AspectRatioH  = fi.AspectRatioH;

        intParam.Profile = mfx.CodecProfile - 1;
        intParam.Tier = (mfx.CodecLevel & MFX_TIER_HEVC_HIGH) ? 1 : 0;
        intParam.Level = (mfx.CodecLevel &~ MFX_TIER_HEVC_HIGH) * 1; // mult 3 it SetProfileLevel
        intParam.generalConstraintFlags = hevcParam.GeneralConstraintFlags;
        intParam.transquantBypassEnableFlag = 0;
        intParam.transformSkipEnabledFlag = 0;
        intParam.log2ParallelMergeLevel = 2;
        intParam.weightedPredFlag = 0;
        intParam.weightedBipredFlag = 0;
        intParam.constrainedIntrapredFlag = 0;
        intParam.strongIntraSmoothingEnabledFlag = 1;

        intParam.tcDuration90KHz = (mfxF64)fi.FrameRateExtD / fi.FrameRateExtN * 90000; // calculate tick duration

        intParam.doDumpRecon = (dumpFiles.ReconFilename[0] != 0);
        if (intParam.doDumpRecon)
            Copy(intParam.reconDumpFileName, dumpFiles.ReconFilename);

        intParam.doDumpSource = (dumpFiles.InputFramesFilename[0] != 0);
        if (intParam.doDumpSource)
            Copy(intParam.sourceDumpFileName, dumpFiles.InputFramesFilename);

        intParam.inputVideoMem = (mfxParam.IOPattern == VIDMEM) || (mfxParam.IOPattern == OPAQMEM && (opaq.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY) == 0);

        intParam.randomRepackThreshold = int32_t(optHevc.RepackProb / 100.0 * MYRAND_MAX);

        intParam.codecType = CODEC_AV1;

        intParam.subsamplingX = intParam.chromaShiftW;
        intParam.subsamplingY = intParam.chromaShiftH;
        intParam.miCols = (intParam.Width + 7) >> 3;
        intParam.miRows = (intParam.Height + 7) >> 3;
        intParam.sb64Cols = (intParam.miCols + 7) >> 3;
        intParam.sb64Rows = (intParam.miRows + 7) >> 3;
        intParam.miPitch = (intParam.sb64Cols << 3);
        intParam.txMode = TX_MODE_SELECT;
        intParam.interpolationFilter = SWITCHABLE;
        intParam.allowHighPrecisionMv = (optHevc.AllowHpMv == ON);
        intParam.errorResilientMode = 0;
        intParam.maxNumReorderPics = (mfx.GopRefDist > 1) ? MAX(1, intParam.BiPyramidLayers - 1) : 0;
        intParam.enableFwdProbUpdateCoef = (optHevc.FwdProbUpdateCoef == ON);
        intParam.enableFwdProbUpdateSyntax = (optHevc.FwdProbUpdateSyntax == ON);
        intParam.enableIntraEdgeFilter = 0;
        intParam.switchableMotionMode = 0;
        //intParam.seqParams.frameIdNumbersPresentFlag = 1;
        //if(intParam.EnableLTR)
        intParam.seqParams.frameIdNumbersPresentFlag = 0;

        intParam.seqParams.deltaFrameIdLength = 7;
        intParam.seqParams.additionalFrameIdLength = 1;
        intParam.seqParams.idLen = 8;
        //intParam.seqParams.orderHintBits = 5;   // should be enough for all supported types of GOP
        //if(intParam.EnableLTR)
        intParam.seqParams.orderHintBits = 8;   // extended for LTR

        intParam.seqParams.enableOrderHint = 1;
        intParam.seqParams.separateUvDeltaQ = 0;

        intParam.refineModeDecision = (optHevc.MaxTxDepthIntraRefine > 1 || optHevc.MaxTxDepthInterRefine > 1);
        intParam.maxTxDepthIntra = optHevc.MaxTxDepthIntra;
        intParam.maxTxDepthInter = optHevc.MaxTxDepthInter;
        intParam.maxTxDepthIntraRefine = optHevc.MaxTxDepthIntraRefine;
        intParam.maxTxDepthInterRefine = optHevc.MaxTxDepthInterRefine;
        intParam.chromaRDO = (optHevc.ChromaRDO == ON);
        intParam.intraRDO = (optHevc.IntraRDO == ON);
        intParam.interRDO = (optHevc.InterRDO == ON);
        intParam.intraInterRDO = (optHevc.IntraInterRDO == ON);
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        intParam.switchInterpFilter = (av1param.InterpFilter == MFX_AV1_INTERP_SWITCHABLE);
#else
        intParam.switchInterpFilter = (optHevc.InterpFilter == ON);
#endif
        intParam.switchInterpFilterRefine = (optHevc.InterpFilterRefine == ON);
        intParam.targetUsage = mfxParam.mfx.TargetUsage;
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        intParam.disableFrameEndUpdateCdf = (av1param.DisableFrameEndUpdateCdf == ON);
#else
        intParam.disableFrameEndUpdateCdf = (optHevc.DisableFrameEndUpdateCdf == ON);
#endif
        intParam.ibcModeDecision = 1;

        SetupTileParamAv1(&intParam, &intParam.tileParamKeyFrame, 1, optHevc.NumTileColumnsKeyFrame, 1);
        SetupTileParamAv1(&intParam, &intParam.tileParamInterFrame, 1, optHevc.NumTileColumnsInterFrame, 1);
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        intParam.writeIVFHeaders = (av1param.WriteIVFHeaders == MFX_CODINGOPTION_OFF) ? 0 : 1;
#else
        intParam.writeIVFHeaders = (vp9param.WriteIVFHeaders == MFX_CODINGOPTION_OFF) ? 0 : 1;
#endif

        LoopFilterInitThresh(intParam.lfts);

#if defined(DUMP_COSTS_CU) || defined (DUMP_COSTS_TU)
        char fname[100];
        intParam.fp_cu = intParam.fp_tu = NULL;
#ifdef DUMP_COSTS_CU
        sprintf(fname, "thres_cu_%d.bin",mfx.TargetUsage);
        if (!(intParam.fp_cu = fopen(fname,"ab"))) return;
#endif
#ifdef DUMP_COSTS_TU
        sprintf(fname, "thres_tu_%d.bin",mfx.TargetUsage);
        if (!(intParam.fp_tu = fopen(fname,"ab"))) return;
#endif
#endif
#if GPU_VARTX
        intParam.numGpuSlices = 1;
#else
        intParam.numGpuSlices = std::min(intParam.sb64Rows, (int32_t)optHevc.NumGpuSlices);
#endif

        const int32_t minMdSliceHeight = 64;
        const int32_t minHmeSliceHeight = 256;
        const int32_t minMd2SliceHeight = 128;
        const int32_t md2Delay = 64;
        const int32_t heightInUnits = DivUp(intParam.Height, minMdSliceHeight);
        const int32_t offset = (heightInUnits / intParam.numGpuSlices) / 2;

        Zero(intParam.mdKernelSliceStart);
        Zero(intParam.hmeKernelSliceStart);
        Zero(intParam.md2KernelSliceStart);

        // MD kernel first
        for (int32_t i = 1; i < intParam.numGpuSlices; i++)
            intParam.mdKernelSliceStart[i] = (i * heightInUnits / intParam.numGpuSlices + offset) * minMdSliceHeight;
        // HME kernel
        for (int32_t i = 1; i < intParam.numGpuSlices; i++)
            intParam.hmeKernelSliceStart[i] = (intParam.mdKernelSliceStart[i] + minHmeSliceHeight - 1) & ~(minHmeSliceHeight - 1);
        // MD2 kernel
        for (int32_t i = 1; i < intParam.numGpuSlices; i++)
            intParam.md2KernelSliceStart[i] = (intParam.mdKernelSliceStart[i] - md2Delay) & ~(minMd2SliceHeight - 1);
    }

    void ApplyMBDeltaQp(Frame* frame, const AV1VideoParam & par)
    {
        int32_t numCtb = par.PicHeightInCtbs * par.PicWidthInCtbs;
        int32_t qpLine = ((par.Width + 16 - 1) / 16);
        int32_t qpHigh = ((par.Height + 16 - 1) / 16);
        int32_t base_qp = frame->m_sliceQpY;
        for (int32_t i = 0; i < par.PicHeightInCtbs; i++) {
            for (int32_t j = 0; j < par.PicWidthInCtbs; j++) {
                int32_t dqp = 0;
                for (int mbi = 0;mbi < 4;mbi++)
                    for (int mbj = 0;mbj < 4;mbj++)
                        dqp += frame->mbqpctrl.DeltaQP[MIN(qpHigh-1, (i * 4 + mbi)) * qpLine + MIN(qpLine-1,(j * 4 + mbj))];
                dqp /= 16;
                int32_t qp = base_qp + dqp;
                qp = MAX(0, MIN(MAXQ, qp));
                dqp = abs(qp - base_qp);
                dqp >>= DEFAULT_DELTA_Q_RES_SHIFT;
                dqp <<= DEFAULT_DELTA_Q_RES_SHIFT;
                qp = (qp < base_qp) ? base_qp - dqp : base_qp + dqp;
                qp = MAX(0, MIN(MAXQ, qp));
                frame->m_lcuQps[i*par.PicWidthInCtbs + j] = qp;
            }
        }
    }

    void ApplyRoiDeltaQp(Frame* frame, const AV1VideoParam & par)
    {
        int32_t numCtb = par.PicHeightInCtbs * par.PicWidthInCtbs;

        for (int32_t i = par.numRoi - 1; i >= 0; i--) {
            int32_t start = par.roi[i].left / par.MaxCUSize + par.roi[i].top / par.MaxCUSize * par.PicWidthInCtbs;
            int32_t end = par.roi[i].right / par.MaxCUSize + par.roi[i].bottom / par.MaxCUSize * par.PicWidthInCtbs;

            int32_t base_qp = frame->m_sliceQpY;
            int32_t dqp = par.roi[i].priority;
            int32_t qp = base_qp + dqp;
            qp = MAX(0,MIN(MAXQ, qp));
            dqp = abs(qp - base_qp);
            dqp >>= DEFAULT_DELTA_Q_RES_SHIFT;
            dqp <<= DEFAULT_DELTA_Q_RES_SHIFT;
            qp = (qp < base_qp) ? base_qp - dqp : base_qp + dqp;
            qp = MAX(0, MIN(MAXQ, qp));

            for (int32_t ctb = start; ctb <= end; ctb++) {
                int32_t col = (ctb % par.PicWidthInCtbs) * par.MaxCUSize;
                int32_t row = (ctb / par.PicWidthInCtbs) * par.MaxCUSize;
                if (col >= par.roi[i].left && col < par.roi[i].right && row >= par.roi[i].top && row < par.roi[i].bottom) {
                    frame->m_lcuQps[ctb] = qp;
                }
            }
        }
    }

#ifdef AMT_HROI_PSY_AQ
    void setRoiDeltaQP(const AV1VideoParam *par, Frame *frame, uint8_t useBrc)
    {
        const float l2f = log(2.0f);
        const float dB[9] = { 1.0f, 0.891f, 0.794f, 0.707f, 0.623f, 0.561f, 0.5f, 0.445f, 0.39f };
        const float dF[9] = { 1.0f, 1.123f, 1.260f, 1.414f, 1.587f, 1.782f, 2.0f, 2.245f, 2.52f };
        int32_t numCtb = par->PicWidthInCtbs * par->PicHeightInCtbs;
        Statistics *stats = frame->m_stats[0];
        CtbRoiStats *ctbStats = stats->ctbStats;
        int32_t idxRsCs = (par->Log2MaxCUSize -2) -1; // Quarter
        int32_t pitchRsCsQ = stats->m_pitchRsCs[idxRsCs];
        int32_t nRsCs = (1 << (par->Log2MaxCUSize - 3))*(1 << (par->Log2MaxCUSize - 3));
        double avgMinblkRsCs = 0.0, avgRsCs = 0.0;
        int32_t luminanceAvg = stats->roi_pic.luminanceAvg;
        if (luminanceAvg < 32) luminanceAvg = 32;
        int32_t noRb = 0, noBb = 0;
        if (par->PicHeightInCtbs*(1 << par->Log2MaxCUSize) > par->Height) noBb = 1;
        if (par->PicWidthInCtbs*(1 << par->Log2MaxCUSize) > par->Width)  noRb = 1;

        int32_t bn = stats->roi_pic.numCtbRoi[0];
        int32_t fn = stats->roi_pic.numCtbRoi[1] + stats->roi_pic.numCtbRoi[2];
        int32_t bnnz = 0;

        int32_t fni = 0;
        int32_t numAct = 0;
        int32_t av1qp = frame->m_sliceQpY;
        float qc = vp9_dc_quant(av1qp, 0, par->bitDepthLuma) / 8.0f;
        int32_t qp = (int32_t) (log2f(qc) * 6.0f + 4.0f);
        int32_t qpa = qp - (frame->m_pyramidLayer - ((frame->m_picCodeType == MFX_FRAMETYPE_I) ? 1 : 0));
        int32_t iframe = frame->m_picCodeType == MFX_FRAMETYPE_I || frame->m_isLtrFrame || stats->m_sceneChange;
        if ((iframe && !(par->DeltaQpMode&AMT_DQP_PSY)) || (qpa < 16))
        {
            // reset
            for (int32_t i = 0; i < (int)par->PicHeightInCtbs; i++) {
                for (int32_t j = 0; j < (int)par->PicWidthInCtbs; j++) {
                    ctbStats[i*par->PicWidthInCtbs + j].dqp = 0;
                    ctbStats[i*par->PicWidthInCtbs + j].roiLevel = 0;
                }
            }
            return;
        }

        // Contrast
        for (int32_t i = 0; i < (int)par->PicHeightInCtbs; i++)
        {
            for (int32_t j = 0; j < (int)par->PicWidthInCtbs; j++)
            {
                // Border and Black Mask
                if (!((j == par->PicWidthInCtbs - 1 && noRb) || (i == par->PicHeightInCtbs - 1 && noBb)) && ctbStats[i*par->PicWidthInCtbs + j].luminance >= 32) {
                    float minblkRsCs = INT_MAX;
                    float ctbRsCs = 0.0;
                    for (int32_t k = 0; k < 2; k++) {
                        for (int32_t l = 0; l < 2; l++) {
                            float blkRsCs = stats->m_rs[idxRsCs][(i * 2 + k)*pitchRsCsQ + j * 2 + l] + stats->m_cs[idxRsCs][(i * 2 + k)*pitchRsCsQ + 2 * j + l];
                            blkRsCs /= (1 << (par->bitDepthLumaShift * 2));
                            minblkRsCs = MIN(blkRsCs, minblkRsCs);
                            ctbRsCs += blkRsCs;
                        }
                    }
                    minblkRsCs /= (nRsCs*2.0);
                    minblkRsCs += 1.0;
                    ctbStats[i*par->PicWidthInCtbs + j].spatMinQCmplx = minblkRsCs;
                    ctbStats[i*par->PicWidthInCtbs + j].spatCmplx = ctbRsCs / (nRsCs*4.0);
                    avgMinblkRsCs += minblkRsCs;
                    avgRsCs += ctbRsCs;
                    numAct++;
                }
            }
        }

        if (numAct) avgMinblkRsCs /= numAct;
        else       avgMinblkRsCs = 1.0;

        // Control
        int32_t maxFq = -1, minBq = 0;

        int32_t             qrange = 1;

        if (qpa < 18)      qrange = 2;
        else if (qpa < 23) qrange = 3;
        else if (qpa < 28) qrange = 4;
        else if (qpa < 34) qrange = 5;
        else if (qpa < 40) qrange = 6;
        else if (qpa < 44) qrange = 4;
        else               qrange = 2;

        if ((par->BiPyramidLayers > 1 && frame->m_pyramidLayer) || (par->PGopPicSize>1 && frame->m_pyramidLayer))
            qrange -= frame->m_pyramidLayer;
        if ((par->DeltaQpMode&AMT_DQP_PAQ) && par->BiPyramidLayers > 1 && frame->m_pyramidLayer == 0)
            qrange--;
        if (par->BiPyramidLayers > 1 && frame->m_pyramidLayer <= par->refLayerLimit[0])
            qrange--;

        if (qrange < 1)
            qrange = 1;

        if (iframe) {
            qrange = (qrange + 1) / 2;
        }

        //if (useBrc && fn == 0) qrange = ((par->enableCmFlag && par->bitDepthLuma == 8) ? MIN(qrange, 2) : 1);  // Till we solve MT Slowdown & 10 bit loss (Psy now auto on for BRC)

        // VSM (Visual Sensitivity Map)
        float mult = pow(2.0f, (float)qrange / 6.0f);

        for (int32_t i = 0; i < (int)par->PicHeightInCtbs; i++)
        {
            for (int32_t j = 0; j < (int)par->PicWidthInCtbs; j++)
            {
                int32_t val = 0;
                int32_t edge = 0;
                // Border and Black Mask
                if (!((j == par->PicWidthInCtbs - 1 && noRb) || (i == par->PicHeightInCtbs - 1 && noBb)) && ctbStats[i*par->PicWidthInCtbs + j].luminance >= 32) {
                    float minblkRsCs = ctbStats[i*par->PicWidthInCtbs + j].spatMinQCmplx;
                    float RsCsRatio = (mult * minblkRsCs + avgMinblkRsCs) / (minblkRsCs + mult * avgMinblkRsCs);
                    edge = (ctbStats[i*par->PicWidthInCtbs + j].spatCmplx / minblkRsCs > 8.0) ? 1 : 0;
                    float dDQp = (log(RsCsRatio) / l2f) * 6.0;
                    if ((par->screenMode & 0x6) && ctbStats[i*par->PicWidthInCtbs + j].spatCmplx>729) {
                        const int32_t pitchColorCount = stats->m_pitchColorCount16x16;
                        int32_t colorCount = 0;
                        for (int32_t k = 0;k < 4;k++) {
                            colorCount += stats->m_colorCount16x16[(i*4 + k) * pitchColorCount + j * 4];
                            colorCount += stats->m_colorCount16x16[(i*4 + k) * pitchColorCount + j * 4 + 1];
                            colorCount += stats->m_colorCount16x16[(i*4 + k) * pitchColorCount + j * 4 + 2];
                            colorCount += stats->m_colorCount16x16[(i*4 + k) * pitchColorCount + j * 4 + 3];
                        }
                        if (colorCount < 128) {
                            edge = 0;
                            dDQp = 0;
                        }
                    }
                    //dDQp = 0;Luminance only
                    float luminance = ctbStats[i*par->PicWidthInCtbs + j].luminance;
                    float lRatio = luminance / (float)luminanceAvg;
                    float dDQl = (0.3f * log(lRatio) / l2f) * (float)qrange;

                    val = int(floor(dDQp + dDQl + 0.5));
                }
                ctbStats[i*par->PicWidthInCtbs + j].sedge = edge;
                ctbStats[i*par->PicWidthInCtbs + j].sensitivity = val;
            }
        }

        if (fn == 0)
        {
            // VROI (Visual sensitivity ROI)
            for (int32_t i = 0; i < (int32_t)par->PicHeightInCtbs; i++) {
                for (int32_t j = 0; j < (int32_t)par->PicWidthInCtbs; j++) {
                    int32_t val = ctbStats[i*par->PicWidthInCtbs + j].sensitivity;
                    int32_t edge = ctbStats[i*par->PicWidthInCtbs + j].sedge;

                    ctbStats[i*par->PicWidthInCtbs + j].dqp = (val < -qrange) ? -qrange : ((val > qrange) ? qrange : val);
                    ctbStats[i*par->PicWidthInCtbs + j].roiLevel = (val < 0) ? 2 : 0;

                    if (ctbStats[i*par->PicWidthInCtbs + j].roiLevel > 0) {
                        if (edge) {
                            if (ctbStats[i*par->PicWidthInCtbs + j].dqp < -1) ctbStats[i*par->PicWidthInCtbs + j].dqp += 1;
                            ctbStats[i*par->PicWidthInCtbs + j].roiLevel = 1;
                        }
                        fni++;
                    }
                }
            }
            // EROI = VROI
            fn = fni;
            bn = numCtb - fn;
        }
        else
        {
            // HROI
            for (int32_t i = 0; i < (int32_t)par->PicHeightInCtbs; i++) {
                for (int32_t j = 0; j < (int32_t)par->PicWidthInCtbs; j++) {
                    int32_t val = ctbStats[i*par->PicWidthInCtbs + j].sensitivity;

                    if (ctbStats[i*par->PicWidthInCtbs + j].roiLevel < 1) {
                        ctbStats[i*par->PicWidthInCtbs + j].dqp = (val < minBq) ? minBq : ((val > qrange) ? qrange : val);
                    }
                    else {
                        ctbStats[i*par->PicWidthInCtbs + j].dqp = (val > maxFq) ? maxFq : ((val < -qrange) ? -qrange : val);
                    }
                }
            }

            // EROI (Enhanced) = HROI + VROI'
            const int32_t minEroi = 4;
            if ((par->DeltaQpMode&AMT_DQP_PSY) && fn < numAct / minEroi)
            {
                int32_t thresh = -5;
                // Add full
                while (fn < numAct / minEroi && thresh < maxFq)
                {
                    for (int32_t i = 0; i < (int)par->PicHeightInCtbs; i++) {
                        for (int32_t j = 0; j < (int)par->PicWidthInCtbs; j++) {
                            if (ctbStats[i*par->PicWidthInCtbs + j].roiLevel == 0) {
                                int32_t val = ctbStats[i*par->PicWidthInCtbs + j].sensitivity;
                                if (val < thresh && fn < numAct / minEroi && !ctbStats[i*par->PicWidthInCtbs + j].sedge) {
                                    ctbStats[i*par->PicWidthInCtbs + j].roiLevel = 2;
                                    ctbStats[i*par->PicWidthInCtbs + j].dqp = ((val < -qrange) ? -qrange : val);
                                    ctbStats[i*par->PicWidthInCtbs + j].dqp -= maxFq;   // rebase to maxfq
                                    fn++;
                                    fni++;
                                }
                            }
                        }
                    }
                    if (fn < numAct / minEroi && thresh < (maxFq - 1))
                    {
                        for (int32_t i = 0; i < (int)par->PicHeightInCtbs; i++) {
                            for (int32_t j = 0; j < (int)par->PicWidthInCtbs; j++) {
                                if (ctbStats[i*par->PicWidthInCtbs + j].roiLevel == 0) {
                                    int32_t val = ctbStats[i*par->PicWidthInCtbs + j].sensitivity;
                                    if (val < thresh && fn < numAct / minEroi && ctbStats[i*par->PicWidthInCtbs + j].sedge) {
                                        ctbStats[i*par->PicWidthInCtbs + j].roiLevel = 1;
                                        ctbStats[i*par->PicWidthInCtbs + j].dqp = ((val < -qrange) ? -qrange : val);
                                        ctbStats[i*par->PicWidthInCtbs + j].dqp -= (maxFq - 1); // rebase to maxfq
                                        fn++;
                                        fni++;
                                    }
                                }
                            }
                        }
                    }
                    thresh++;
                }

                bn = numCtb - fn;
            }
        }

        if (!par->AnalyzeCmplx) return;

        // Qp & Complexity
        float sumQpb = 0.0;
        float sumQpf = 0.0;
        float bcmplx = 1.0;
        float fcmplx = 1.0;
        float tcmplx = 1.0;
        float bcmplxd = 1.0;
        float bcmplxnnz = 1.0;
        for (int32_t i = 0; i < (int)par->PicHeightInCtbs; i++) {
            for (int32_t j = 0; j < (int)par->PicWidthInCtbs; j++) {
                if (ctbStats[i*par->PicWidthInCtbs + j].roiLevel < 1) {
                    sumQpb += ctbStats[i*par->PicWidthInCtbs + j].dqp;
                    bcmplx += ctbStats[i*par->PicWidthInCtbs + j].complexity;
                    bcmplxd += (ctbStats[i*par->PicWidthInCtbs + j].complexity*dB[ctbStats[i*par->PicWidthInCtbs + j].dqp]);
                    if (ctbStats[i*par->PicWidthInCtbs + j].dqp > 0) {
                        bcmplxnnz += ctbStats[i*par->PicWidthInCtbs + j].complexity;
                        bnnz++;
                    }
                }
                else {
                    sumQpf += ctbStats[i*par->PicWidthInCtbs + j].dqp;
                    fcmplx += ctbStats[i*par->PicWidthInCtbs + j].complexity;
                }
                tcmplx += ctbStats[i*par->PicWidthInCtbs + j].complexity;
            }
        }

        // Balance
        if (fn != 0) {
            float dqpb_act = bn ? sumQpb / bn : 0;
            float dqpb_nnz = bnnz ? sumQpb / bnnz : 0;
            float dqpf_act = sumQpf / fn;
            // compute
            float fcmplxd = tcmplx - bcmplxd;
            float dqf = fcmplxd / fcmplx;
            float dqpf = (log(dqf) / l2f) * -6.0f;

            int32_t iter = 0;
            int32_t num_changed = 1;

            if (abs(dqpf_act - dqpf) > 0.1) {
                // ROI
                int32_t fOff = ((dqpf < dqpf_act) ? 1 : -1);
                int32_t fOff1 = fOff;
                while (abs(dqpf_act - dqpf) > 0.01 && num_changed != 0 && iter < 14) {
                    float sumQpfLast = sumQpf;
                    sumQpf = 0.0;
                    num_changed = 0;
                    fcmplxd = 1.0;

                    for (int32_t i = (int)par->PicHeightInCtbs - 1; i >= 0; i--) {
                        for (int32_t j = (int)par->PicWidthInCtbs - 1; j >= 0; j--) {
                            if (ctbStats[i*par->PicWidthInCtbs + j].roiLevel > 0) {
                                int32_t dqpo = ctbStats[i*par->PicWidthInCtbs + j].dqp;
                                int32_t dqp = dqpo - fOff;
                                dqp = MAX(-1 * qrange, dqp);
                                dqp = MIN(ctbStats[i*par->PicWidthInCtbs + j].segCount ? -1 : maxFq, dqp);

                                float dqpd = dqp - dqpo;
                                ctbStats[i*par->PicWidthInCtbs + j].dqp = dqp;
                                sumQpf += dqp;
                                if (dqp != dqpo) num_changed++;
                                fcmplxd += (ctbStats[i*par->PicWidthInCtbs + j].complexity*dF[-1 * ctbStats[i*par->PicWidthInCtbs + j].dqp]);
                                // Run Time Exit
                                sumQpfLast += dqpd;
                                float dqpf_run = sumQpfLast / fn;
                                if (abs(dqpf_run - dqpf) <= 0.01)
                                    fOff = 0;
                            }
                        }
                    }
                    if (fni && dqpf > -1.0 && num_changed == 0 && maxFq != 0) {
                        // non HROI aq does not need -1 ROI guarantee
                        maxFq = 0;
                        num_changed = 1;
                    }
                    iter++;
                    dqpf_act = sumQpf / fn;
                    fOff = ((dqpf < dqpf_act) ? 1 : -1);
                    if (fOff != fOff1) iter = 14;
                }
                if (par->bitDepthLumaShift == 0) {
                    // BG 8bit Simple
                    if ((dqpf_act - dqpf) > 0.01 && (num_changed == 0 || iter == 14)) {
                        // compute
                        bcmplxd = tcmplx - fcmplxd;
                        float dqb = bcmplxd / bcmplx;
                        float dqpb = (log(dqb) / l2f) * -6.0f;

                        iter = 0;
                        num_changed = 1;

                        while ((dqpb_act - dqpb) > 0 && num_changed != 0 & iter < 14) {
                            sumQpb = 0.0;
                            num_changed = 0;
                            for (int32_t i = 0; i < (int)par->PicHeightInCtbs; i++) {
                                for (int32_t j = 0; j < (int)par->PicWidthInCtbs; j++) {
                                    if (ctbStats[i*par->PicWidthInCtbs + j].roiLevel < 1) {
                                        int32_t dqpo = ctbStats[i*par->PicWidthInCtbs + j].dqp;
                                        int32_t dqp = dqpo - 1;
                                        dqp = MAX(minBq, dqp);
                                        ctbStats[i*par->PicWidthInCtbs + j].dqp = dqp;
                                        sumQpb += dqp;
                                        if (dqp != dqpo) num_changed++;
                                    }
                                }
                            }
                            iter++;
                            dqpb_act = sumQpb / bn;
                        }
                    }
                }
                else {
                    // BG 10bit complex
                    if ((dqpf_act - dqpf) > 0.01 && (num_changed == 0 || iter == 14)) {
                        // compute
                        float bcmplxd1 = fcmplxd - fcmplx;
                        float dqb = (bcmplxnnz - bcmplxd1) / bcmplxnnz;
                        float dqpb = (log(dqb) / l2f) * -6.0f;

                        iter = 0;
                        num_changed = 1;
                        int32_t bOff = 1;
                        while (bnnz && (dqpb_nnz - dqpb) > 0 && num_changed != 0 & iter < 14) {
                            float sumQpbLast = sumQpb;
                            sumQpb = 0.0;
                            num_changed = 0;
                            for (int32_t i = 0; i < (int)par->PicHeightInCtbs; i++) {
                                for (int32_t j = 0; j < (int)par->PicWidthInCtbs; j++) {
                                    if (ctbStats[i*par->PicWidthInCtbs + j].roiLevel < 1) {
                                        int32_t dqpo = ctbStats[i*par->PicWidthInCtbs + j].dqp;
                                        int32_t dqp = dqpo - bOff;
                                        dqp = MAX(minBq, dqp);
                                        float dqpd = dqp - dqpo;
                                        ctbStats[i*par->PicWidthInCtbs + j].dqp = dqp;
                                        sumQpb += dqp;
                                        if (dqp != dqpo) num_changed++;
                                        // Run Time Exit
                                        sumQpbLast += dqpd;
                                        float dqpb_run = sumQpbLast / bnnz;
                                        if (dqpb_run - dqpb < 0)
                                            bOff = 0;
                                    }
                                }
                            }
                            iter++;
                            dqpb_nnz = sumQpb / bnnz;
                        }
                    }
                }
            }
        } // Balance
    }
//#define DEBUG_ROI_MAP
    void ApplyHRoiDeltaQp(Frame* frame, const AV1VideoParam &par, uint8_t useBrc)
    {
        int32_t numCtb = par.PicHeightInCtbs * par.PicWidthInCtbs;
        Statistics* stats = frame->m_stats[0];

        setRoiDeltaQP(&par, frame, useBrc);

        uint8_t *pLcuQP = &(frame->m_lcuQps[0]);
        int32_t avgq = 0;

        for (int32_t ctb = 0; ctb < numCtb; ctb++)
        {
            int32_t dqp = stats->ctbStats[ctb].dqp;
            int32_t qindex = frame->m_sliceQpY + (dqp << DEFAULT_DELTA_Q_RES_SHIFT);

            qindex = Saturate(0, MAXQ, qindex);
            dqp = abs(qindex - frame->m_sliceQpY);
            dqp >>= DEFAULT_DELTA_Q_RES_SHIFT;
            dqp <<= DEFAULT_DELTA_Q_RES_SHIFT;
            qindex = (qindex < frame->m_sliceQpY) ? frame->m_sliceQpY - dqp : frame->m_sliceQpY + dqp;
            qindex = MAX(0, MIN(MAXQ, qindex));
            pLcuQP[ctb] = (uint8_t)qindex;
            avgq += qindex;
        }
#ifdef DEBUG_ROI_MAP
        FILE *fp;
        fp = fopen("qpMap.seg", "ab");
        if (fp)
        {
            int32_t w32 = par.PicWidthInCtbs * 2;
            int32_t h32 = par.PicHeightInCtbs * 2;
            for (int i = 0; i < h32; i++)
            {
                for (int j = 0; j < w32; j++)
                {
                    int val = (int)pLcuQP[(i / 2)*par.PicWidthInCtbs + (j / 2)] - (int)frame->m_sliceQpY;
                    val >>= DEFAULT_DELTA_Q_RES_SHIFT;
                    uint8_t Idx = abs(val) * 2 - (val <= 0 ? 0 : 1);
                    Saturate(0, 255, Idx);
                    fwrite(&Idx, 1, 1, fp);
                }
            }
            fclose(fp);
        }
#endif
        avgq /= numCtb;
        if (avgq < frame->m_sliceQpY && !useBrc)
        {
            frame->m_sliceQpY -= DEFAULT_DELTA_Q_RES;
            int16_t q = vp9_dc_quant(frame->m_sliceQpY, 0, par.bitDepthLuma);
            frame->m_lambda = 88 * q * q / 24.f / 512 / 16 / 128;
            frame->m_lambdaSatd = sqrt(frame->m_lambda * 512) / 512;
            frame->m_lambdaSatdInt = int(frame->m_lambdaSatd * (1 << 24));
        }
        //ApplyDeltaQpOnRoi(frame, par);
    }

#endif

    const float CU_RSCS_TH[5][4][8] = {
        {{  4.0,  6.0,  8.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  8.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  9.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  9.0, 11.0, 14.0, 18.0, 26.0,65025.0}},
        {{  5.0,  6.0,  8.0, 11.0, 14.0, 18.0, 24.0,65025.0},{  5.0,  7.0,  9.0, 11.0, 14.0, 18.0, 25.0,65025.0},{  5.0,  7.0,  9.0, 12.0, 15.0, 19.0, 25.0,65025.0},{  5.0,  7.0,  9.0, 12.0, 14.0, 18.0, 25.0,65025.0}},
        {{  5.0,  7.0, 10.0, 12.0, 15.0, 19.0, 25.0,65025.0},{  5.0,  8.0, 10.0, 13.0, 15.0, 20.0, 26.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 14.0, 18.0, 24.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 15.0, 18.0, 24.0,65025.0}},
        {{  6.0,  8.0, 10.0, 13.0, 16.0, 19.0, 25.0,65025.0},{  5.0,  8.0, 10.0, 13.0, 15.0, 19.0, 26.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 15.0, 18.0, 24.0,65025.0},{  6.0,  9.0, 11.0, 13.0, 15.0, 19.0, 24.0,65025.0}},
        {{  6.0,  9.0, 11.0, 14.0, 17.0, 21.0, 27.0,65025.0},{  6.0,  9.0, 11.0, 13.0, 16.0, 19.0, 25.0,65025.0},{  7.0,  9.0, 12.0, 14.0, 16.0, 19.0, 25.0,65025.0},{  7.0,  9.0, 11.0, 13.0, 16.0, 19.0, 24.0,65025.0}}
    };

    const float LQ_M[5][8] = {
        {4.2415, 3.9818, 3.9818, 3.9818, 4.0684, 4.0684, 4.0684, 4.0684},   // I
        {4.5878, 4.5878, 4.5878, 4.5878, 4.5878, 4.2005, 4.2005, 4.2005},   // P
        {4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255},   // B1
        {4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052},   // B2
        {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005},    // B3
    };
    const float LQ_K[5][8] = {
        {12.8114, 13.8536, 13.8536, 13.8536, 13.8395, 13.8395, 13.8395, 13.8395},   // I
        {12.3857, 12.3857, 12.3857, 12.3857, 12.3857, 13.7122, 13.7122, 13.7122},   // P
        {13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286},   // B1
        {13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463},   // B2
        {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122}    // B3
    };

    int GetCalqDeltaQpLCU(Frame* frame, const AV1VideoParam & par, int32_t ctb_addr, float sliceLambda, float sliceQpY)
    {
        int32_t fcmplx = 2;
        float frscs = frame->m_stats[0]->m_frameCs*frame->m_stats[0]->m_frameCs;
        frscs += frame->m_stats[0]->m_frameRs*frame->m_stats[0]->m_frameRs;
        frscs = sqrt(frscs);
        if (frscs < 2.0f) fcmplx = 0;
        else if (frscs < 27.0f) fcmplx = 1;

        if (!fcmplx) return sliceQpY;

        int32_t picClass = 0;
        if (frame->m_picCodeType == MFX_FRAMETYPE_I || frame->m_isLtrFrame) {
            picClass = 0;   // I
        }
        else {
            picClass = frame->m_pyramidLayer + 1;
            if (frame->m_picCodeType == MFX_FRAMETYPE_P && frame->m_pyramidLayer && frame->m_temporalActv) picClass = 4;
        }

        float qc = vp9_dc_quant(sliceQpY, 0, par.bitDepthLuma)/8.0f;
        float hevcQpY = log2f(qc) * 6.0f + 4.0f;
        const int pQPi[5][4] = { {22,27,32,37}, {23,28,33,38}, {24,29,34,39}, {25,30,35,40}, {26,31,36,41} };
        int32_t qpClass = 0;
        if (hevcQpY < pQPi[picClass][0])
            qpClass = -1;
        else if (hevcQpY > pQPi[picClass][3])
            qpClass = 4;
        else
            qpClass = (int32_t)((hevcQpY - 22 - picClass) / 5);


        int32_t rscsClass = 0;
        if (picClass < 2) {
            int32_t rscs = 0;
            int32_t pitchRsCs = frame->m_stats[0]->m_pitchRsCs[par.Log2MaxCUSize - 2];
            int32_t col = (ctb_addr % par.PicWidthInCtbs);
            int32_t row = (ctb_addr / par.PicWidthInCtbs);
            int32_t idx = row*pitchRsCs + col;
            int32_t Rs2 = frame->m_stats[0]->m_rs[par.Log2MaxCUSize-2][idx];
            int32_t Cs2 = frame->m_stats[0]->m_cs[par.Log2MaxCUSize-2][idx];
            int32_t blkW4 = 1 << (par.Log2MaxCUSize - 2);
            int32_t blkH4 = 1 << (par.Log2MaxCUSize - 2);
            Rs2 /= (blkW4*blkH4);
            Cs2 /= (blkW4*blkH4);
            rscs = sqrt(Rs2 + Cs2);
            int32_t k = 7;
            for (int32_t i = 0; i < 8; i++) {
                if (rscs < CU_RSCS_TH[picClass][MIN(3, MAX(0, qpClass))][i] * (float)(1 << (par.bitDepthLumaShift))) {
                    k = i;
                    break;
                }
            }
            rscsClass = k;
        }

        float dLambda = sliceLambda * 512.f;
        float QP = 0.f;

        QP = LQ_M[picClass][rscsClass] * log(dLambda) + LQ_K[picClass][rscsClass];
        qc = pow(2.0f, (QP - 4.0f) / 6.0f);
        int32_t qindex = vp9_dc_qindex((int32_t)(qc * 8.0f + 0.5f));
        if(fcmplx == 2) qindex += DEFAULT_DELTA_Q_RES;
        if (qpClass < 0) qindex = MAX(qindex, sliceQpY);
        if (qpClass > 3) qindex = MIN(qindex, sliceQpY);

        return qindex;
    }

    void ApplyDeltaQp(Frame* frame, const AV1VideoParam & par)
    {
        int32_t numCtb = par.PicHeightInCtbs * par.PicWidthInCtbs;

        // assign PAQ deltaQp
        if (par.DeltaQpMode&AMT_DQP_PAQ) {// no pure CALQ
            for (size_t blk = 0; blk < frame->m_stats[0]->qp_mask.size(); blk++) {
                int32_t deltaQp = frame->m_stats[0]->qp_mask[blk];
                int32_t lcuQp = frame->m_lcuQps[blk] + deltaQp;
                lcuQp = Saturate(0, 51, lcuQp);
                frame->m_lcuQps[blk] = (uint8_t)lcuQp;
            }
#ifdef LOW_COMPLX_PAQ
            if (par.DeltaQpMode&AMT_DQP_PAQ) {
                float avgDQp = std::accumulate(frame->m_stats[0]->qp_mask.begin(), frame->m_stats[0]->qp_mask.end(), 0);
                avgDQp /= frame->m_stats[0]->qp_mask.size();
                frame->m_stats[0]->m_avgDPAQ = avgDQp;
            }
#endif
        }

        // assign CALQ deltaQp
        if (par.DeltaQpMode&AMT_DQP_CAQ) {
            float baseQP = frame->m_sliceQpY;
            float sliceLambda = frame->m_lambda;

            for (int32_t ctb_addr = 0; ctb_addr < numCtb; ctb_addr++) {
                int32_t qp = GetCalqDeltaQpLCU(frame, par, ctb_addr, sliceLambda, baseQP);
                qp = MAX(0, MIN(MAXQ, qp));
                int32_t dqp = abs(qp - baseQP);
                dqp >>= DEFAULT_DELTA_Q_RES_SHIFT;
                dqp <<= DEFAULT_DELTA_Q_RES_SHIFT;
                qp = (qp < baseQP) ? baseQP - dqp : baseQP + dqp;
                qp = MAX(0, MIN(MAXQ, qp));
                frame->m_lcuQps[ctb_addr] = qp;
            }
        }
    }

    uint8_t GetPAQOffset(const Frame &frame, const AV1VideoParam &par, uint8_t sliceQp)
    {
        uint8_t sliceQpPAQ = sliceQp;
        if (par.DeltaQpMode&AMT_DQP_PAQ) {// no pure CALQ
#if 0 // DEBUG_PAQ
            if (frame.m_pyramidLayer == 0) {
                printf("\n PAQ poc %d \n", frame.m_poc);
                for (int32_t row = 0; row < par.PicHeightInCtbs; row++) {
                    for (int32_t col = 0; col < par.PicWidthInCtbs; col++) {
                        printf(" %+d,", frame.m_stats[0]->qp_mask[row*par.PicWidthInCtbs + col]);
                    }
                    printf("\n");
                }
            }
#endif
            float avgDQp = (float)std::accumulate(frame.m_stats[0]->qp_mask.begin(), frame.m_stats[0]->qp_mask.end(), 0);
            float numBlks = (float)((MAX_DQP / 2)*frame.m_stats[0]->qp_mask.size());
            float mult = (float)(sliceQp - qindexMinus1[qindexMinus1[sliceQp]]);
            sliceQpPAQ = sliceQp + (uint8_t) ((avgDQp / numBlks) * mult - 0.5f);
            Saturate(MINQ, MAXQ, sliceQpPAQ);
        }
        return sliceQpPAQ;
    }

    uint8_t GetConstQp(const Frame &frame, const AV1VideoParam &param, int32_t repackCounter)
    {
        int32_t sliceQpY;
        if (frame.m_sliceQpY > 0 && frame.m_sliceQpY <= MAXQ)
            sliceQpY = frame.m_sliceQpY;    // ctrl
        else {
            switch (frame.m_picCodeType)
            {
            case MFX_FRAMETYPE_I:
                sliceQpY = param.QPI;
#if ENABLE_AV1_ALTR_AUTOQ
                if (frame.m_isLtrFrame) {
                    sliceQpY = MAX(1, qindexMinus1[MAX(1, qindexMinus1[MAX(1, qindexMinus1[MAX(1, qindexMinus1[MAX(1, qindexMinus1[param.QPI])])])])]);
                }
#endif
                break;
            case MFX_FRAMETYPE_P:
                if (param.BiPyramidLayers > 1 && param.longGop && frame.m_biFramesInMiniGop == 15)
                    sliceQpY = param.QPI;
                else if (param.PGopPicSize > 1) {
#ifdef USE_OLD_IPB_QINDEX
                    sliceQpY = param.QPP + frame.m_pyramidLayer * (param.QPP - param.QPI);
#else
                    if (frame.m_pyramidLayer == 0) {
                        sliceQpY = param.QPP;
                    }
                    else if (frame.m_pyramidLayer == 1) {
                        sliceQpY = MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[param.QPP])])])]);
                    }
                    else {
                        int32_t QPP1 = MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[param.QPP])])])]);
                        sliceQpY = MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[QPP1])])])]);
                    }
#endif
                }
                else
                    sliceQpY = param.QPP;

#if !ENABLE_AV1_ALTR_AUTOQ
                if (frame.m_isLtrFrame) {
                    sliceQpY = param.QPI;
                } else if (frame.m_shortTermRefFlag && param.PGopPicSize > 1
#if !ENABLE_AV1_ALTR_APQ0
                    && frame.m_allowPalette
#endif
                    ) {
                    sliceQpY = MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[param.QPP])])])]); // Needs to be Adaptive
                }
#else
                if (frame.m_isLtrFrame) {
                    sliceQpY = MAX(1, qindexMinus1[MAX(1, qindexMinus1[MAX(1, qindexMinus1[MAX(1, qindexMinus1[param.QPI])])])]);
                } else if (frame.m_shortTermRefFlag && param.PGopPicSize > 1
#if !ENABLE_AV1_ALTR_APQ0
                    && frame.m_allowPalette
#endif
                    ) {
                    sliceQpY = MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[param.QPP])])])]);
                }
#endif
                break;
            case MFX_FRAMETYPE_B:
                if (param.BiPyramidLayers > 1 && param.longGop && frame.m_biFramesInMiniGop == 15)
                    sliceQpY = param.QPI + frame.m_pyramidLayer;
                else if (param.BiPyramidLayers > 1)
                {
#ifdef USE_OLD_IPB_QINDEX
                    sliceQpY = param.QPP + frame.m_pyramidLayer * (param.QPB - param.QPP);
#else
                    if (frame.m_pyramidLayer == 1)
                        sliceQpY = param.QPB;
                    else if (frame.m_pyramidLayer == 2)
                        sliceQpY = MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[param.QPB])]);
                    else
                        sliceQpY = MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[MIN(MAXQ, qindexPlus1[param.QPB])])])] + (frame.m_pyramidLayer-3) * 8);
#endif
                }
                else
                    sliceQpY = param.QPB;
                break;
            default:
                // Unsupported Picture Type
                assert(0);
                sliceQpY = param.QPI;
                break;
            }
        }

        sliceQpY += repackCounter;
        return (uint8_t)Saturate(MINQ, MAXQ, sliceQpY);
    }

#if ENABLE_BRC
    uint8_t GetRateQp(const Frame &in, const AV1VideoParam &param, BrcIface *brc)
    {
        if (brc == NULL)
            return 128;

        Frame *frames[128] = {(Frame*)&in};
        int32_t framesCount = MIN(127, (int32_t)in.m_futureFrames.size());

        for (int32_t frmIdx = 0; frmIdx < framesCount; frmIdx++)
            frames[frmIdx+1] = in.m_futureFrames[frmIdx];

        int32_t QP = brc->GetQP(const_cast<AV1VideoParam *>(&param), frames, framesCount+1);

        return (uint8_t)QP;
    }
#endif

    void SetFrameDataAllocInfo(FrameData::AllocInfo &allocInfo, int32_t widthLu, int32_t heightLu, int32_t paddingLu, int32_t fourcc, void *m_fei, int32_t feiLumaPitch, int32_t feiChromaPitch)
    {
        int32_t bpp;
        int32_t heightCh;

        switch (fourcc) {
        case MFX_FOURCC_NV12:
            allocInfo.bitDepthLu = 8;
            allocInfo.bitDepthCh = 8;
            allocInfo.chromaFormat = MFX_CHROMAFORMAT_YUV420;
            allocInfo.paddingChH = paddingLu/2;
            heightCh = heightLu/2;
            bpp = 1;
            break;
        case MFX_FOURCC_NV16:
            allocInfo.bitDepthLu = 8;
            allocInfo.bitDepthCh = 8;
            allocInfo.chromaFormat = MFX_CHROMAFORMAT_YUV422;
            allocInfo.paddingChH = paddingLu;
            heightCh = heightLu;
            bpp = 1;
            break;
        case MFX_FOURCC_P010:
            allocInfo.bitDepthLu = 10;
            allocInfo.bitDepthCh = 10;
            allocInfo.chromaFormat = MFX_CHROMAFORMAT_YUV420;
            allocInfo.paddingChH = paddingLu/2;
            heightCh = heightLu/2;
            bpp = 2;
            break;
        case MFX_FOURCC_P210:
            allocInfo.bitDepthLu = 10;
            allocInfo.bitDepthCh = 10;
            allocInfo.chromaFormat = MFX_CHROMAFORMAT_YUV422;
            allocInfo.paddingChH = paddingLu;
            heightCh = heightLu;
            bpp = 2;
            break;
        default:
            assert(!"invalid fourcc");
            return;
        }

        int32_t widthCh = widthLu;
        allocInfo.width = widthLu;
        allocInfo.height = heightLu;
        allocInfo.paddingLu = paddingLu;
        allocInfo.paddingChW = paddingLu;

        allocInfo.feiHdl = m_fei;
        if (m_fei) {
            assert(feiLumaPitch >= mfx::align2_value<int32_t>(mfx::align2_value<int32_t>(paddingLu*bpp, 64) + widthLu*bpp + paddingLu*bpp, 64 ));
            assert(feiChromaPitch >= mfx::align2_value<int32_t>(mfx::align2_value<int32_t>(allocInfo.paddingChW*bpp, 64) + widthCh*bpp + allocInfo.paddingChW*bpp, 64 ));
            assert((feiLumaPitch & 63) == 0);
            assert((feiChromaPitch & 63) == 0);
            allocInfo.alignment = 0x1000;
            allocInfo.pitchInBytesLu = feiLumaPitch;
            allocInfo.pitchInBytesCh = feiChromaPitch;
            allocInfo.sizeInBytesLu = allocInfo.pitchInBytesLu * (heightLu + paddingLu * 2);
            allocInfo.sizeInBytesCh = allocInfo.pitchInBytesCh * (heightCh + allocInfo.paddingChH * 2);
        } else {
            allocInfo.alignment = 64;
            allocInfo.pitchInBytesLu = mfx::align2_value<int32_t>(mfx::align2_value<int32_t>(paddingLu*bpp, 64) + widthLu*bpp + paddingLu*bpp, 64 );
            allocInfo.pitchInBytesCh = mfx::align2_value<int32_t>(mfx::align2_value<int32_t>(allocInfo.paddingChW*bpp, 64) + widthCh*bpp + allocInfo.paddingChW*bpp, 64 );
            if ((allocInfo.pitchInBytesLu & (allocInfo.pitchInBytesLu - 1)) == 0)
                allocInfo.pitchInBytesLu += 64;
            if ((allocInfo.pitchInBytesCh & (allocInfo.pitchInBytesCh - 1)) == 0)
                allocInfo.pitchInBytesCh += 64;
            allocInfo.sizeInBytesLu = allocInfo.pitchInBytesLu * (heightLu + paddingLu * 2);
            allocInfo.sizeInBytesCh = allocInfo.pitchInBytesCh * (heightCh + allocInfo.paddingChH * 2);
        }
    }


    struct Deleter { template <typename T> void operator ()(T* p) const { delete p; } };

    const int32_t blkSizeInternal2Fei[4] = { MFX_FEI_H265_BLK_8x8, MFX_FEI_H265_BLK_16x16, MFX_FEI_H265_BLK_32x32, MFX_FEI_H265_BLK_64x64 };
    const int32_t blkSizeFei2Internal[12] = { -1, -1, -1, 2, -1, -1, 1, -1, -1, 0, -1, -1 };

}; // anonimous namespace


namespace AV1Enc {
    struct AV1EncodeTaskInputParams
    {
        mfxEncodeCtrl    *ctrl;
        mfxFrameSurface1 *surface;
        mfxBitstream     *bs;
        Frame            *m_targetFrame[2];  // this frame has to be encoded completely. it is our expected output.
        Frame            *m_newFrame[2];
        ThreadingTask     m_ttComplete;
        volatile uint32_t   m_taskID;
        std::atomic<uint32_t>   m_taskStage;
        std::atomic<uint32_t>   m_taskReencode;     // BRC repack

#if TASK_LOG_ENABLE
        uint32_t task_log_idx;
#endif
    };
};


AV1Encoder::AV1Encoder(MFXCoreInterface1 &core)
    : m_core(core)
    , m_frameCountSync(0)
    , m_frameCountBufferedSync(0)
    , m_useSysOpaq(false)
    , m_useVideoOpaq(false)
    , m_isOpaque(false)
    , m_spsBufSize(0)
    , m_profileIndex(0)
    , m_frameOrder(0)
    , m_frameOrderOfLastIdr(0)
    , m_frameOrderOfLastIntra(0)
    , m_frameOrderOfLastBaseTemporalLayer(0)
    , m_frameOrderOfLastIntraInEncOrder(0)
    , m_frameOrderOfLastAnchor(0)
    , m_frameOrderOfLastIdrB(0)
    , m_frameOrderOfLastIntraB(0)
    , m_frameOrderOfLastAnchorB(0)
    , m_LastbiFramesInMiniGop(0)
    , m_miniGopCount(0)
    , m_lastTimeStamp(0)
    , m_lastEncOrder(0)
    , m_sceneOrder(0)
    , m_frameOrderOfLastLtr(0)
    , m_frameOrderOfStartStr(0)
    , m_numberOfExternalLtrFrames(0)
    , m_frameOrderOfErrorResilienceFrame(0)
    , m_last_filter_level(0)
    , m_nextFrameToEncode(0)
    , m_inputTaskInProgress(nullptr)
    , m_lastShowFrame(0)
    , m_prevFrame(NULL)
    , m_memBuf(NULL)
    , m_cu(nullptr)
    , data_temp(nullptr)
    , m_pauseFeiThread(0)
    , m_stopFeiThread(0)
    , m_brc(NULL)
    , m_fei(NULL)
{
    ippStaticInit();
    Zero(m_videoParam);
    Zero(m_stat);
    Zero(m_nextFrameDpbVP9);
#ifdef AMT_HROI_PSY_AQ
    m_faceSkinDet = NULL;
#endif
#if TASK_LOG_ENABLE
    taskLog = NULL;
#endif
}


AV1Encoder::~AV1Encoder()
{
    m_feiCritSect.lock();
    m_pauseFeiThread = 0;
    m_stopFeiThread = 1;
    m_feiCritSect.unlock();
    m_feiCondVar.notify_one();
    if (m_feiThread.joinable())
        m_feiThread.join();

    // aya: issue on my HSW systems - not investigated yet
#if 1
    for_each(m_free.begin(), m_free.end(), Deleter());
    m_free.resize(0);
    for_each(m_inputQueue.begin(), m_inputQueue.end(), Deleter());
    m_inputQueue.resize(0);
    for_each(m_lookaheadQueue.begin(), m_lookaheadQueue.end(), Deleter());
    m_lookaheadQueue.resize(0);
    for_each(m_dpb.begin(), m_dpb.end(), Deleter());
    m_dpb.resize(0);
    // note: m_encodeQueue() & m_outputQueue() only "refer on tasks", not have real ptr. so we don't need to release ones
#endif

#if ENABLE_BRC
    delete m_brc;
#endif // ENABLE_BRC

    if (!m_frameEncoder.empty()) {
        for (size_t encIdx = 0; encIdx < m_frameEncoder.size(); encIdx++) {
            if (AV1FrameEncoder* fenc = m_frameEncoder[encIdx]) {
                fenc->Close();
                delete fenc;
            }
        }
        m_frameEncoder.resize(0);
    }

    m_inputFrameDataPool.Destroy();
    m_input10FrameDataPool.Destroy();
    m_reconFrameDataPool.Destroy();
    m_recon10FrameDataPool.Destroy();
    m_reconUpscaleFrameDataPool.Destroy();
    m_feiInputDataPool.Destroy();
    m_feiReconDataPool.Destroy();

    // destory FEI resources
    for (int32_t i = 0; i < 4; i++)
        m_feiInterDataPool[i].Destroy();
    m_feiModeInfoPool.Destroy();
#if GPU_VARTX
    m_feiAdzPool.Destroy();
    m_feiVarTxInfoPool.Destroy();
    m_feiCoefsPool.Destroy();
#endif // GPU_VARTX
    for (int32_t i = 0; i < 4; i++)
        m_feiRsCsPool[i].Destroy();

    if (m_fei)
        H265FEI_Close(m_fei);

    m_la.reset(0);
#ifdef AMT_HROI_PSY_AQ
    if(m_faceSkinDet)
        FS_Free(&m_faceSkinDet);
#endif

    if (m_memBuf) {
        AV1_Free(m_memBuf);
        m_memBuf = NULL;
    }

#if defined(DUMP_COSTS_CU) || defined (DUMP_COSTS_TU)
#ifdef DUMP_COSTS_CU
    if (m_videoParam.fp_cu) fclose(m_videoParam.fp_cu);
#endif
#ifdef DUMP_COSTS_TU
    if (m_videoParam.fp_tu) fclose(m_videoParam.fp_tu);
#endif
#endif

#if TASK_LOG_ENABLE
    TaskLogDump();
    TaskLogClose();
#endif
}

namespace {
    mfxHDL CreateAccelerationDevice(MFXCoreInterface1 &core, mfxHandleType &type)
    {
        mfxCoreParam par;
        mfxStatus sts = MFX_ERR_NONE;
        if ((sts = core.GetCoreParam(&par)) != MFX_ERR_NONE)
            return mfxHDL(0);
        mfxU32 impl = par.Impl & 0x0700;

        mfxHDL device = mfxHDL(0);
        mfxHandleType handleType = (impl == MFX_IMPL_VIA_D3D9)  ? MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9 :
                                   (impl == MFX_IMPL_VIA_D3D11) ? MFX_HANDLE_D3D11_DEVICE :
                                   (impl == MFX_IMPL_VIA_VAAPI) ? MFX_HANDLE_VA_DISPLAY :
                                                                  mfxHandleType(0);
        if (handleType == mfxHandleType(0))
            return mfxHDL(0);
        if ((sts = core.CreateAccelerationDevice(handleType, &device)) != MFX_ERR_NONE)
            return mfxHDL(0);
        type = handleType;
        return device;
    }
};

mfxStatus AV1Encoder::Init(const mfxVideoParam &par)
{
    ConvertToInternalParam(m_videoParam, par);

    mfxStatus sts = MFX_ERR_NONE;

    mfxHDL device;
    mfxHandleType deviceType = mfxHandleType(0);
    if (m_videoParam.enableCmFlag) {
        if ((device = CreateAccelerationDevice(m_core, deviceType)) == 0)
            return MFX_ERR_DEVICE_FAILED;
    }

#if ENABLE_BRC
    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        m_brc = CreateBrc(&par);
        if ((sts = m_brc->Init(&par, m_videoParam)) != MFX_ERR_NONE)
            return sts;
    }
#endif

    if ((sts = InitInternal()) != MFX_ERR_NONE)
        return sts;

    m_spsBufSize = 0;
    WriteIvfHeader(m_spsBuf, m_spsBufSize, m_videoParam);
    m_spsBufSize >>= 3;

    // memsize calculation
    int32_t sizeofAV1CU = (
#if ENABLE_10BIT
        m_videoParam.bitDepthLuma > 8 ? sizeof(AV1CU<uint16_t>) :
#endif
        sizeof(AV1CU<uint8_t>));

    int32_t memSize = 0;
    // cu
    memSize = sizeofAV1CU * m_videoParam.num_thread_structs + DATA_ALIGN;
    // data_temp
    int32_t data_temp_size = 4 << m_videoParam.Log2NumPartInCU;
    memSize += sizeof(ModeInfo) * data_temp_size * m_videoParam.num_thread_structs + DATA_ALIGN;//for ModeDecision try different cu
    // coeffs Work
    //memSize += sizeof(CoeffsType) * (numCtbs << (m_videoParam.Log2MaxCUSize << 1)) * 6 / (2 + m_videoParam.chromaShift) + DATA_ALIGN;

    // allocation
    m_memBuf = (uint8_t *)AV1_Malloc(memSize);
    MFX_CHECK_STS_ALLOC(m_memBuf);
    //ippsZero_8u(m_memBuf, memSize);
	memset(m_memBuf, 0, memSize);

    // mapping
    uint8_t *ptr = m_memBuf;
    m_cu = UMC::align_pointer<void*>(ptr, DATA_ALIGN);
    ptr += sizeofAV1CU * m_videoParam.num_thread_structs + DATA_ALIGN;
    //m_cu = UMC::align_pointer<void*>(ptr, 0x1000);
    //ptr += sizeofAV1CU * m_videoParam.num_thread_structs + 0x1000;

    // data_temp
    data_temp = UMC::align_pointer<ModeInfo *>(ptr, DATA_ALIGN);
    ptr += sizeof(ModeInfo) * data_temp_size * m_videoParam.num_thread_structs + DATA_ALIGN;

#ifdef AMT_HROI_PSY_AQ
    if (m_videoParam.DeltaQpMode & AMT_DQP_PSY_HROI) {
        uint16_t fs_cpu_feature = 0;
        if (m_videoParam.cpuFeature == AV1PP::CPU_FEAT_AUTO) {
            uint32_t cpuIdInfoRegs[4];
            uint64_t featuresMask;
            IppStatus err = ippGetCpuFeatures(&featuresMask, cpuIdInfoRegs);
            if (ippStsNoErr != err)                             fs_cpu_feature = 0;
            else if (featuresMask & (uint64_t)(ippCPUID_AVX2))    fs_cpu_feature = 2;
            else if (featuresMask & (uint64_t)(ippCPUID_SSE41))   fs_cpu_feature = 1;
        }
        else if (m_videoParam.cpuFeature == AV1PP::CPU_FEAT_AVX2) {
            fs_cpu_feature = 2;
        }
        /*else if (m_videoParam.cpuFeature == AV1PP::CPU_FEAT_SSE4) {
            fs_cpu_feature = 1;
        }*/
        if (FS_Init(&m_faceSkinDet, m_videoParam.Width, m_videoParam.Height, 1, 1, fs_cpu_feature, m_videoParam.GopRefDist + 1) != 0) sts = MFX_ERR_MEMORY_ALLOC;
        MFX_CHECK_STS(sts);
    }
#endif

    bitCount = {};
    EstimateBits(bitCount);

    InitQuantizer(m_videoParam);

    if (m_videoParam.src10Enable) {
        InitQuantizer10bit(m_videoParam);
    }

    // AllocFrameEncoders()
    m_frameEncoder.resize(m_videoParam.m_framesInParallel);
    for (size_t encIdx = 0; encIdx < m_frameEncoder.size(); encIdx++) {
        m_frameEncoder[encIdx] = new AV1FrameEncoder(*this);
        m_frameEncoder[encIdx]->Init();
    }

    m_la.reset(new Lookahead(*this));

    FrameData::AllocInfo frameDataAllocInfo;
    FrameData::AllocInfo frameDataAllocInfoSrc10;
    FrameData::AllocInfo frameDataAllocInfoReconUpscale;

    if (m_videoParam.enableCmFlag) {
        mfxFEIH265Param feiParams = {};
        feiParams.Width = m_videoParam.Width;
        feiParams.Height = m_videoParam.Height;
        feiParams.Padding = 64+16;
        feiParams.WidthChroma = m_videoParam.Width;
        feiParams.HeightChroma = m_videoParam.Height >> m_videoParam.chromaShiftH;
        feiParams.PaddingChroma = 64+16;
        feiParams.MaxCUSize = m_videoParam.MaxCUSize;
        feiParams.MPMode = 1;//m_videoParam.partModes;
        feiParams.NumRefFrames = m_videoParam.MaxDecPicBuffering;
        feiParams.TargetUsage = 0;
        feiParams.NumIntraModes = 1;
        feiParams.FourCC = m_videoParam.fourcc;
        feiParams.EnableChromaSao = m_videoParam.SAOChromaFlag;
        feiParams.InterpFlag = m_videoParam.CmInterpFlag;
        feiParams.IsAv1 = true;
        feiParams.NumSlices = m_videoParam.numGpuSlices;
        Copy(feiParams.HmeSliceStart, m_videoParam.hmeKernelSliceStart);
        Copy(feiParams.MdSliceStart, m_videoParam.mdKernelSliceStart);
        Copy(feiParams.Md2SliceStart, m_videoParam.md2KernelSliceStart);
        //feiParams.DualFilter = m_videoParam.DualFilter;

        mfxExtFEIH265Alloc feiAlloc = {};
        MFXCoreInterface core(m_core.m_core);
        sts = H265FEI_GetSurfaceDimensions_new(&core, &feiParams, &feiAlloc);
        if (sts != MFX_ERR_NONE)
            return sts;

        sts = H265FEI_Init(&m_fei, &feiParams, &core);
        if (sts != MFX_ERR_NONE)
            return sts;

        FeiOutData::AllocInfo feiOutAllocInfo;
        feiOutAllocInfo.feiHdl = m_fei;
        for (int32_t blksize = 0; blksize < 4; blksize++) {
            int32_t feiBlkSizeIdx = blkSizeInternal2Fei[blksize];
            feiOutAllocInfo.allocInfo = feiAlloc.InterData[feiBlkSizeIdx];
            m_feiInterDataPool[blksize].Init(feiOutAllocInfo, 0);
        }

        // patch allocInfo to meet CmSurface2D requirements
        SetFrameDataAllocInfo(frameDataAllocInfo, m_videoParam.Width, m_videoParam.Height, 64+16, m_videoParam.fourcc, m_fei, feiAlloc.SrcRefLuma.pitch, feiAlloc.SrcRefChroma.pitch);
        if (m_videoParam.superResFlag) {
            SetFrameDataAllocInfo(frameDataAllocInfoReconUpscale, m_videoParam.sourceWidth, m_videoParam.Height, 128 + 16, m_videoParam.fourcc, NULL, 0, 0);
        }

        if (m_videoParam.src10Enable) {
            SetFrameDataAllocInfo(frameDataAllocInfoSrc10, m_videoParam.Width, m_videoParam.Height, 64 + 16, MFX_FOURCC_P010, NULL, 0, 0);
        }
        FeiInputData::AllocInfo feiInputAllocaInfo;
        feiInputAllocaInfo.feiHdl = m_fei;
        m_feiInputDataPool.Init(feiInputAllocaInfo, 0);

        FeiReconData::AllocInfo feiReconAllocaInfo;
        feiReconAllocaInfo.feiHdl = m_fei;
        m_feiReconDataPool.Init(feiReconAllocaInfo, 0);

        FeiSurfaceUp::AllocInfo feiSurfaceUpAllocInfo;
        feiSurfaceUpAllocInfo.feiHdl = m_fei;
        feiSurfaceUpAllocInfo.allocInfo = feiAlloc.ModeInfo;
        m_feiModeInfoPool.Init(feiSurfaceUpAllocInfo, 0);

#if GPU_VARTX
        FeiBufferUp::AllocInfo feiVarTxInfoAllocInfo;
        feiVarTxInfoAllocInfo.feiHdl = m_fei;
        feiVarTxInfoAllocInfo.alignment = 0x1000;
        feiVarTxInfoAllocInfo.size = m_videoParam.sb64Cols * m_videoParam.sb64Rows * 16 * 16 * sizeof(uint16_t);
        m_feiVarTxInfoPool.Init(feiVarTxInfoAllocInfo, 0);

        FeiBufferUp::AllocInfo feiAdzAllocInfo;
        feiAdzAllocInfo.feiHdl = m_fei;
        feiAdzAllocInfo.alignment = 0x1000;
        feiAdzAllocInfo.size = m_videoParam.sb64Cols * m_videoParam.sb64Rows * sizeof(ADzCtx);
        m_feiAdzPool.Init(feiAdzAllocInfo, 0);

        FeiBufferUp::AllocInfo feiCoefsAllocInfo;
        feiCoefsAllocInfo.feiHdl = m_fei;
        feiCoefsAllocInfo.alignment = 0x1000;
        feiCoefsAllocInfo.size = m_videoParam.sb64Cols * m_videoParam.sb64Rows * 4096 * sizeof(int16_t);
        m_feiCoefsPool.Init(feiCoefsAllocInfo, 0);
#endif // GPU_VARTX

        feiSurfaceUpAllocInfo.feiHdl = m_fei;
        feiSurfaceUpAllocInfo.allocInfo = feiAlloc.RsCs64Info;
        m_feiRsCsPool[3].Init(feiSurfaceUpAllocInfo, 0);

        feiSurfaceUpAllocInfo.feiHdl = m_fei;
        feiSurfaceUpAllocInfo.allocInfo = feiAlloc.RsCs32Info;
        m_feiRsCsPool[2].Init(feiSurfaceUpAllocInfo, 0);

        feiSurfaceUpAllocInfo.feiHdl = m_fei;
        feiSurfaceUpAllocInfo.allocInfo = feiAlloc.RsCs16Info;
        m_feiRsCsPool[1].Init(feiSurfaceUpAllocInfo, 0);

        feiSurfaceUpAllocInfo.feiHdl = m_fei;
        feiSurfaceUpAllocInfo.allocInfo = feiAlloc.RsCs8Info;
        m_feiRsCsPool[0].Init(feiSurfaceUpAllocInfo, 0);
    } else {
        SetFrameDataAllocInfo(frameDataAllocInfo, m_videoParam.Width, m_videoParam.Height, 64+16, m_videoParam.fourcc, NULL, 0, 0);

        FeiSurfaceUp::AllocInfo feiSurfaceUpAllocInfo;
        feiSurfaceUpAllocInfo.feiHdl = m_fei;
        feiSurfaceUpAllocInfo.allocInfo.align = 64;
        feiSurfaceUpAllocInfo.allocInfo.pitch = m_videoParam.miPitch * sizeof(ModeInfo);
        feiSurfaceUpAllocInfo.allocInfo.size = m_videoParam.sb64Rows * 8 * feiSurfaceUpAllocInfo.allocInfo.pitch;
        m_feiModeInfoPool.Init(feiSurfaceUpAllocInfo, 0);
    }

    m_reconFrameDataPool.Init(frameDataAllocInfo, 0);
    if (m_videoParam.superResFlag) {
        m_reconUpscaleFrameDataPool.Init(frameDataAllocInfoReconUpscale, 0);
    }
    m_inputFrameDataPool.Init(frameDataAllocInfo, 0);
    if (m_videoParam.src10Enable) {
        m_input10FrameDataPool.Init(frameDataAllocInfoSrc10, 0);
        m_recon10FrameDataPool.Init(frameDataAllocInfoSrc10, 0);
    }

    if (m_la.get() && (m_videoParam.LowresFactor || m_videoParam.SceneCut)) {
        int32_t lowresFactor = m_videoParam.LowresFactor;
        if (m_videoParam.SceneCut && m_videoParam.LowresFactor == 0)
            lowresFactor = 1;
        SetFrameDataAllocInfo(frameDataAllocInfo, m_videoParam.Width>>lowresFactor, m_videoParam.Height>>lowresFactor, MAX(SIZE_BLK_LA, 16) + 16, m_videoParam.fourcc, NULL, 0, 0);
        m_frameDataLowresPool.Init(frameDataAllocInfo, 0);
    }

    Statistics::AllocInfo statsAllocInfo;
    statsAllocInfo.width = m_videoParam.Width;
    statsAllocInfo.height = m_videoParam.Height;
    statsAllocInfo.analyzeCmplx = m_videoParam.AnalyzeCmplx;
    statsAllocInfo.deltaQpMode = m_videoParam.DeltaQpMode;
    m_statsPool.Init(statsAllocInfo, 0);

    if (m_la.get()) {
        if (m_videoParam.LowresFactor) {
            statsAllocInfo.width = m_videoParam.Width >> m_videoParam.LowresFactor;
            statsAllocInfo.height = m_videoParam.Height >> m_videoParam.LowresFactor;
            m_statsLowresPool.Init(statsAllocInfo, 0);
        }
        if (m_videoParam.SceneCut) {
            SceneStats::AllocInfo sceneStatsAllocInfo;
            // it doesn't make width/height here
            m_sceneStatsPool.Init(sceneStatsAllocInfo, 0);
        }
    }

    m_stopFeiThread = 0;
    m_pauseFeiThread = 0;
    m_feiThreadRunning = 0;
    if (m_videoParam.enableCmFlag) {
        m_feiThread = std::thread([this]() { AV1Encoder::FeiThreadRoutineStarter(this); });
    }

    m_laFrame[0] = NULL;
    m_laFrame[1] = NULL;

    m_threadingTaskRunning = 0;
    m_frameCountSync = 0;
    m_frameCountBufferedSync = 0;

    m_ithreadPool.resize(m_videoParam.num_thread_structs, 0);

#if TASK_LOG_ENABLE
    TaskLogInit();
#endif
#ifdef VT_TRACE
    char ttname[256];
    for (int p = 0; p < 64; p++) {
        for (int a = 0; a < TT_COUNT; a++) {
            ttname[0] = 0;
            strcat(ttname, TT_NAMES[a]);
            strcat(ttname, " ");
            strcat(ttname, NUMBERS[p & 0x3f]);
//                       strcpy(_trace_task_names[p][a],ttname);
            wchar_t wstr[256];
            std::mbstowcs(wstr, ttname, strlen(ttname)+1);
            _trace_static_handle2[p][a].itt1.ptr = __itt_string_handle_create(wstr);
        }
    }
#endif

    m_doStage = THREADING_WORKING;
    m_threadCount = 0;
    m_reencode = 0;
    m_taskSubmitCount = 0;
    m_taskEncodeCount = 0;
    m_nextFrameToEncode = 0;
    m_inputTaskInProgress = NULL;
    Zero(m_nextFrameDpbVP9);
    Zero(m_actualDpbVP9);
    Zero(m_contexts);
    m_prevFrame = NULL;
    m_last_filter_level = 0;

    return MFX_ERR_NONE;
}

mfxStatus AV1Encoder::InitInternal()
{
    m_profileIndex = 0;
    m_frameOrder = 0;

    m_frameOrderOfLastIdr = 0;              // frame order of last IDR frame (in display order)
    m_frameOrderOfLastIntra = 0;            // frame order of last I-frame (in display order)
    m_frameOrderOfLastBaseTemporalLayer = 0;
    m_frameOrderOfLastIntraInEncOrder = 0;  // frame order of last I-frame (in encoding order)
    m_frameOrderOfLastAnchor = 0;           // frame order of last anchor (first in minigop) frame (in display order)
    m_frameOrderOfLastIdrB = 0;
    m_frameOrderOfLastIntraB = 0;
    m_frameOrderOfLastAnchorB  = 0;
    m_LastbiFramesInMiniGop  = 0;
    m_frameOrderOfLastLtr = MFX_FRAMEORDER_UNKNOWN;
    m_numberOfExternalLtrFrames = 0;
    m_frameOrderOfStartStr = MFX_FRAMEORDER_UNKNOWN;
    for(int i=0;i<16;i++)
        m_frameOrderOfLastExternalLTR[i] = MFX_FRAMEORDER_UNKNOWN;

    m_frameOrderOfErrorResilienceFrame = 0;

    //m_miniGopCount = -1;
    m_miniGopCount = m_videoParam.encodedOrder ? 0 : -1;
    m_lastTimeStamp = (mfxU64)MFX_TIMESTAMP_UNKNOWN;
    m_lastEncOrder = -1;
    m_sceneOrder = 0;

    Zero(m_videoParam.cu_split_threshold_cu_sentinel);
    Zero(m_videoParam.cu_split_threshold_tu_sentinel);
    Zero(m_videoParam.cu_split_threshold_cu);
    Zero(m_videoParam.cu_split_threshold_tu);

    int32_t qpMax = 42;
    for (int32_t QP = 10; QP <= qpMax; QP++) {
        for (int32_t isNotI = 0; isNotI <= 1; isNotI++) {
            for (int32_t i = 0; i < m_videoParam.MaxCUDepth; i++) {
                m_videoParam.cu_split_threshold_cu[QP][isNotI][i] = h265_calc_split_threshold(m_videoParam.SplitThresholdTabIndex, 0, isNotI, m_videoParam.Log2MaxCUSize - i,
                    isNotI ? m_videoParam.SplitThresholdStrengthCUInter : m_videoParam.SplitThresholdStrengthCUIntra, QP) * m_videoParam.SplitThresholdMultiplier;
                m_videoParam.cu_split_threshold_tu[QP][isNotI][i] = h265_calc_split_threshold(m_videoParam.SplitThresholdTabIndex, 1, isNotI, m_videoParam.Log2MaxCUSize - i,
                    m_videoParam.SplitThresholdStrengthTUIntra, QP);
            }
        }
    }
    for (int32_t QP = qpMax + 1; QP <= 51; QP++) {
        for (int32_t isNotI = 0; isNotI <= 1; isNotI++) {
            for (int32_t i = 0; i < m_videoParam.MaxCUDepth; i++) {
                m_videoParam.cu_split_threshold_cu[QP][isNotI][i] = m_videoParam.cu_split_threshold_cu[qpMax][isNotI][i];
                m_videoParam.cu_split_threshold_tu[QP][isNotI][i] = m_videoParam.cu_split_threshold_tu[qpMax][isNotI][i];
            }
        }
    }


    AV1PP::initDispatcher(m_videoParam.cpuFeature);

    return MFX_ERR_NONE;
}


mfxStatus AV1Encoder::Reset(const mfxVideoParam &par)
{
    return MFX_ERR_UNSUPPORTED;
}


void AV1Encoder::GetEncodeStat(mfxEncodeStat &stat)
{
    MfxAutoMutex guard(m_statMutex);
    Copy(stat, m_stat);
}

mfxStatus AV1Encoder::ResetTemporalLayers(const mfxVideoParam &par)
{
    if (m_videoParam.GopRefDist == 1) {
        const mfxExtAvcTemporalLayers &tlayers = GetExtBuffer(par);
        if (tlayers.Layer[1].Scale == 2) {
            m_videoParam.NumRefLayers = 2;
            if (tlayers.Layer[2].Scale == 4) m_videoParam.NumRefLayers = 3;
        }
        else {
            m_videoParam.NumRefLayers = 1;
        }
    }
    m_frameOrderOfLastBaseTemporalLayer = m_frameOrder;
    return MFX_ERR_NONE;
}

// --------------------------------------------------------
//   Utils
// --------------------------------------------------------

void ReorderBiFrames(FrameIter inBegin, FrameIter inEnd, FrameList &in, FrameList &out, int32_t layer = 1)
{
    if (inBegin == inEnd)
        return;
    FrameIter pivot = inBegin;
    //std::advance(pivot, (std::distance(inBegin, inEnd) - 1) / 2);
    std::advance(pivot, (std::distance(inBegin, inEnd) ) / 2);
    (*pivot)->m_pyramidLayer = layer;
    if (pivot != inBegin)
        (*pivot)->showFrame = 0; // hidden frame
    FrameIter rightHalf = pivot;
    ++rightHalf;
    if (inBegin == pivot)
        ++inBegin;
    out.splice(out.end(), in, pivot);
    ReorderBiFrames(inBegin, rightHalf, in, out, layer + 1);
    ReorderBiFrames(rightHalf, inEnd, in, out, layer + 1);
}


void ReorderBiFramesLongGop(FrameIter inBegin, FrameIter inEnd, FrameList &in, FrameList &out)
{
    assert(std::distance(inBegin, inEnd) == 15);

    // 3 anchors + 3 mini-pyramids
    for (int32_t i = 0; i < 3; i++) {
        FrameIter anchor = inBegin;
        std::advance(anchor, 3);
        FrameIter afterAnchor = anchor;
        ++afterAnchor;
        (*anchor)->m_pyramidLayer = 2;
        (*anchor)->showFrame = 0;
        out.splice(out.end(), in, anchor);
        ReorderBiFrames(inBegin, afterAnchor, in, out, 3);
        inBegin = afterAnchor;
    }
    // last 4th mini-pyramid
    ReorderBiFrames(inBegin, inEnd, in, out, 3);
}


void ReorderFrames(FrameList &input, FrameList &reordered, const AV1VideoParam &param, int32_t endOfStream)
{
    int32_t closedGop = param.GopClosedFlag;
    int32_t strictGop = param.GopStrictFlag;
    int32_t biPyramid = param.BiPyramidLayers > 1;

    if (input.empty())
        return;

    FrameIter anchor = input.begin();
    FrameIter end = input.end();
    while (anchor != end && (*anchor)->m_picCodeType == MFX_FRAMETYPE_B)
        ++anchor;
    if (anchor == end && !endOfStream)
        return; // minigop is not accumulated yet
    if (anchor == input.begin()) {
        reordered.splice(reordered.end(), input, anchor); // lone anchor frame
        return;
    }

    // B frames present
    // process different situations:
    //   (a) B B B <end_of_stream> -> B B B    [strictGop=true ]
    //   (b) B B B <end_of_stream> -> B B P    [strictGop=false]
    //   (c) B B B <new_sequence>  -> B B B    [strictGop=true ]
    //   (d) B B B <new_sequence>  -> B B P    [strictGop=false]
    //   (e) B B P                 -> B B P
    bool anchorExists = true;
    if (anchor == end) {
        if (strictGop)
            anchorExists = false; // (a) encode B frames without anchor
        else {
            --anchor;
            (*anchor)->m_picCodeType = MFX_FRAMETYPE_P; // (b) use last B frame of stream as anchor
        }
    }
    else {
        Frame *anchorFrm = (*anchor);
        if (anchorFrm->m_picCodeType == MFX_FRAMETYPE_I && (anchorFrm->m_isIdrPic || closedGop)) {
            if (strictGop)
                anchorExists = false; // (c) encode B frames without anchor
            else {
                --anchor;
                (*anchor)->m_picCodeType = MFX_FRAMETYPE_P; // (d) use last B frame of current sequence as anchor
            }
        }
    }

    // setup number of B frames
    int32_t numBiFrames = (int32_t)std::distance(input.begin(), anchor);
    for (FrameIter i = input.begin(); i != anchor; ++i)
        (*i)->m_biFramesInMiniGop = numBiFrames;

    // reorder anchor frame
    FrameIter afterBiFrames = anchor;
    if (anchorExists) {
        (*anchor)->m_pyramidLayer = 0; // anchor frames are from layer=0
        (*anchor)->m_biFramesInMiniGop = numBiFrames;
        if (numBiFrames > 0)
            (*anchor)->showFrame = 0; // anchor is hidden frame
        ++afterBiFrames;
        reordered.splice(reordered.end(), input, anchor);
    }

    if (biPyramid)
        if (numBiFrames == 15 && param.longGop)
            ReorderBiFramesLongGop(input.begin(), afterBiFrames, input, reordered); // B frames in long pyramid order
        else
            ReorderBiFrames(input.begin(), afterBiFrames, input, reordered); // B frames in pyramid order
    else
        reordered.splice(reordered.end(), input, input.begin(), afterBiFrames); // no pyramid, B frames in input order
}


void ReorderBiFields(FrameIter inBegin, FrameIter inEnd, FrameList &in, FrameList &out, int32_t maxLayer, int32_t layer = 1)
{
    assert(std::distance(inBegin, inEnd) % 2 == 0); // should be even number of fields
    if (inBegin == inEnd)
        return;
    FrameIter pivot1 = inBegin;
    std::advance(pivot1, (std::distance(inBegin, inEnd) / 2) & ~1);
    FrameIter pivot2 = pivot1;
    pivot2++;
    (*pivot1)->m_pyramidLayer = layer;
    (*pivot2)->m_pyramidLayer = layer;
    //if (maxLayer > 0 && layer == maxLayer)
    //    (*pivot1)->m_pyramidLayer--;
    FrameIter rightHalf = pivot2;
    ++rightHalf;
    if (inBegin == pivot1) {
        ++inBegin;
        ++inBegin;
    }
    out.splice(out.end(), in, pivot1);
    out.splice(out.end(), in, pivot2);
    ReorderBiFields(inBegin, rightHalf, in, out, maxLayer, layer + 1);
    ReorderBiFields(rightHalf, inEnd, in, out, maxLayer, layer + 1);
}


void ReorderFields(FrameList &input, FrameList &reordered, const AV1VideoParam &param, int32_t endOfStream)
{
    int32_t closedGop = param.GopClosedFlag;
    int32_t strictGop = param.GopStrictFlag;

    if (input.empty())
        return;

    FrameIter anchor = input.begin();
    FrameIter end = input.end();
    while (anchor != end && ((*anchor)->m_picCodeType == MFX_FRAMETYPE_B || (*anchor)->m_secondFieldFlag == 0))
        ++anchor;
    if (anchor != end) {
        --anchor; // to first field
        if (anchor == input.begin()) { // lone anchor fields to reorder
            reordered.splice(reordered.end(), input, input.begin()); // first field of anchor frame
            reordered.splice(reordered.end(), input, input.begin()); // second field of anchor frame
            return;
        }
    } else if (!endOfStream) {
        return; // minigop is not accumulated yet
    }

    // B frames present
    // process different situations:
    //   (a) B B B <end_of_stream> -> B B B    [strictGop=true ]
    //   (b) B B B <end_of_stream> -> B B P    [strictGop=false]
    //   (c) B B B <new_sequence>  -> B B B    [strictGop=true ]
    //   (d) B B B <new_sequence>  -> B B P    [strictGop=false]
    //   (e) B B P                 -> B B P
    bool anchorExists = true;
    if (anchor == end) {
        if (strictGop)
            anchorExists = false; // (a) encode B frames without anchor
        else {
            // (d) use last B frame of current sequence as anchor
            (*--anchor)->m_picCodeType = MFX_FRAMETYPE_P; // second field
            (*--anchor)->m_picCodeType = MFX_FRAMETYPE_P; // first field
        }
    }
    else {
        FrameIter tmp = anchor;
        Frame *anchorField1 = (*tmp);
        Frame *anchorField2 = (*++tmp);
        if (anchorField1->m_picCodeType == MFX_FRAMETYPE_I && (anchorField1->m_isIdrPic || closedGop) ||
            anchorField2->m_picCodeType == MFX_FRAMETYPE_I && (anchorField2->m_isIdrPic || closedGop)) {
            if (strictGop)
                anchorExists = false; // (c) encode B frames without anchor
            else {
                // (d) use last B frame of current sequence as anchor
                (*--anchor)->m_picCodeType = MFX_FRAMETYPE_P; // second field
                (*--anchor)->m_picCodeType = MFX_FRAMETYPE_P; // first field
            }
        }
    }

    // setup number of B frames
    int32_t numBiFrames = (int32_t)std::distance(input.begin(), anchor) / 2;
    for (FrameIter i = input.begin(); i != anchor; ++i)
        (*i)->m_biFramesInMiniGop = numBiFrames;

    // reorder anchor frame
    FrameIter afterBiFrames = anchor;
    if (anchorExists) {
        (*afterBiFrames)->m_pyramidLayer = 0; // anchor frames are from layer=0
        (*afterBiFrames)->m_biFramesInMiniGop = numBiFrames;
        ++afterBiFrames; // to second field of anchor
        (*afterBiFrames)->m_pyramidLayer = 0; // anchor frames are from layer=0
        (*afterBiFrames)->m_biFramesInMiniGop = numBiFrames;
        ++afterBiFrames; // to field after anchor
        FrameIter anchor2 = anchor;
        FrameIter anchor1 = anchor2++;
        if ((*anchor1)->m_isIdrPic || (*anchor2)->m_isIdrPic) {
            reordered.splice(reordered.end(), input, anchor1); // first field goes first
            reordered.splice(reordered.end(), input, anchor2); // second field
        } else {
            //reordered.splice(reordered.end(), input, anchor1); // first field goes first
            //reordered.splice(reordered.end(), input, anchor2); // second field
            reordered.splice(reordered.end(), input, anchor2); // second field goes first
            reordered.splice(reordered.end(), input, anchor1); // first field
        }
    }

    ReorderBiFields(input.begin(), afterBiFrames, input, reordered, param.BiPyramidLayers - 1); // B frames in pyramid order
}


Frame* FindOldestOutputTask(FrameList & encodeQueue)
{
    FrameIter begin = encodeQueue.begin();
    FrameIter end = encodeQueue.end();

    if (begin == end)
        return NULL;

    FrameIter oldest = begin;
    for (++begin; begin != end; ++begin) {
        if ( (*oldest)->m_encOrder > (*begin)->m_encOrder)
            oldest = begin;
    }

    return *oldest;

} //

// --------------------------------------------------------
//              AV1Encoder
// --------------------------------------------------------

void AV1Encoder::ReorderInput(int32_t endOfStream)
{
    FrameList & inputQueue = m_la.get() ? m_lookaheadQueue : m_inputQueue;
    // reorder as many frames as possible
    while (!inputQueue.empty()) {
        size_t reorderedSize = m_reorderedQueue.size();

        if (m_videoParam.encodedOrder) {
            if (!inputQueue.empty())
                m_reorderedQueue.splice(m_reorderedQueue.end(), inputQueue, inputQueue.begin());
        }
        else {
            if (m_videoParam.picStruct == PROGR)
                ReorderFrames(inputQueue, m_reorderedQueue, m_videoParam, endOfStream);
            else
                ReorderFields(inputQueue, m_reorderedQueue, m_videoParam, endOfStream);
        }

        if (reorderedSize == m_reorderedQueue.size())
            break; // nothing new, exit

        // configure newly reordered frames
        FrameIter i = m_reorderedQueue.begin();
        std::advance(i, reorderedSize);
        for (; i != m_reorderedQueue.end(); ++i) {
            ConfigureEncodeFrame(*i);
            m_lastEncOrder = (*i)->m_encOrder;
        }
    }

    while (!m_reorderedQueue.empty()
        && ((int32_t)m_reorderedQueue.size() >= m_videoParam.RateControlDepth - 1 || endOfStream)
        && (int32_t)(m_encodeQueue.size() + m_outputQueue.size()) < m_videoParam.m_framesInParallel) {

        // assign resources for encoding
        m_reorderedQueue.front()->m_recon = m_reconFrameDataPool.Allocate();
        if (m_videoParam.src10Enable) {
            m_reorderedQueue.front()->m_recon10 = m_recon10FrameDataPool.Allocate();
        }

        if (m_videoParam.superResFlag) {
            m_reorderedQueue.front()->m_reconUpscale = m_reconUpscaleFrameDataPool.Allocate();
        }
        if (m_videoParam.GopPicSize != 1)
            m_reorderedQueue.front()->m_feiRecon = m_feiReconDataPool.Allocate();
        if (!m_reorderedQueue.front()->IsIntra()) {
            m_reorderedQueue.front()->m_feiOrigin = m_feiInputDataPool.Allocate();
            m_reorderedQueue.front()->m_feiModeInfo1 = m_feiModeInfoPool.Allocate();
        }
        m_reorderedQueue.front()->m_feiModeInfo2 = m_feiModeInfoPool.Allocate();
#if GPU_VARTX
        m_reorderedQueue.front()->m_feiAdz = m_feiAdzPool.Allocate();
        m_reorderedQueue.front()->m_feiAdzDelta = m_feiAdzPool.Allocate();
        m_reorderedQueue.front()->m_feiVarTxInfo = m_feiVarTxInfoPool.Allocate();
        m_reorderedQueue.front()->m_feiCoefs = m_feiCoefsPool.Allocate();
#endif // GPU_VARTX
        for (int32_t i = 0; i < 4; i++) {
            m_reorderedQueue.front()->m_feiRs[i] = m_feiRsCsPool[i].Allocate();
            m_reorderedQueue.front()->m_feiCs[i] = m_feiRsCsPool[i].Allocate();
        }
        m_reorderedQueue.front()->m_modeInfo = (ModeInfo *)m_reorderedQueue.front()->m_feiModeInfo2->m_sysmem;

        m_dpb.splice(m_dpb.end(), m_reorderedQueue, m_reorderedQueue.begin());
        m_encodeQueue.push_back(m_dpb.back());

        if (m_brc && m_videoParam.AnalyzeCmplx > 0) {
            int32_t laDepth = m_videoParam.RateControlDepth - 1;
            if (m_videoParam.picStruct == TFF || m_videoParam.picStruct == BFF)
                laDepth = m_videoParam.RateControlDepth * 2 - 1;
            for (FrameIter i = m_reorderedQueue.begin(); i != m_reorderedQueue.end() && laDepth > 0; ++i, --laDepth)
                m_encodeQueue.back()->m_futureFrames.push_back(*i);
        }
    }
}

Frame *AV1Encoder::AcceptFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl *ctrl, mfxBitstream *mfxBS, int32_t fieldNum)
{
    Frame *inputFrame = NULL;

    if (surface) {
        if (m_free.empty()) {
            Frame *newFrame = new Frame;
            newFrame->Create(&m_videoParam);
            m_free.push_back(newFrame);
        }
        m_free.front()->ResetEncInfo();
        m_free.front()->m_timeStamp = surface->Data.TimeStamp;
        m_free.front()->m_picStruct = m_videoParam.picStruct;
        m_free.front()->m_secondFieldFlag = fieldNum;
        m_free.front()->m_bottomFieldFlag = (m_videoParam.picStruct == BFF) ? !fieldNum : fieldNum;
        m_free.front()->m_sliceQpY = 0;
        m_inputQueue.splice(m_inputQueue.end(), m_free, m_free.begin());
        inputFrame = m_inputQueue.back();
        inputFrame->AddRef();

        if (ctrl && ctrl->Payload && ctrl->NumPayload > 0) {
            inputFrame->m_userSeiMessages.resize(ctrl->NumPayload);
            size_t totalSize = 0;
            for (int32_t i = 0; i < ctrl->NumPayload; i++) {
                inputFrame->m_userSeiMessages[i] = *ctrl->Payload[i];
                inputFrame->m_userSeiMessages[i].BufSize = mfxU16((inputFrame->m_userSeiMessages[i].NumBit + 7) / 8);
                totalSize += inputFrame->m_userSeiMessages[i].BufSize;
            }
            inputFrame->m_userSeiMessagesData.resize(totalSize);
            uint8_t *buf = inputFrame->m_userSeiMessagesData.data();
            for (int32_t i = 0; i < ctrl->NumPayload; i++) {
                small_memcpy(buf, inputFrame->m_userSeiMessages[i].Data, inputFrame->m_userSeiMessages[i].BufSize);
                inputFrame->m_userSeiMessages[i].Data = buf;
                buf += inputFrame->m_userSeiMessages[i].BufSize;
            }
        }

        if (m_videoParam.encodedOrder) {
            ThrowIf(!inputFrame, std::runtime_error(""));
            ThrowIf(!ctrl, std::runtime_error(""));

            inputFrame->m_picCodeType = ctrl->FrameType;
#if ENABLE_BRC
            if (m_brc && m_brc->IsVMEBRC()) {
                const mfxExtLAFrameStatistics *vmeData = (mfxExtLAFrameStatistics *)GetExtBufferById(ctrl->ExtParam, ctrl->NumExtParam,MFX_EXTBUFF_LOOKAHEAD_STAT);
                ThrowIf(!vmeData, std::runtime_error(""));
                mfxStatus sts = m_brc->SetFrameVMEData(vmeData, m_videoParam.Width, m_videoParam.Height);
                ThrowIf(sts != MFX_ERR_NONE, std::runtime_error(""));
                mfxLAFrameInfo *pInfo = &vmeData->FrameStat[0];
                inputFrame->m_picCodeType = pInfo->FrameType;
                inputFrame->m_frameOrder = pInfo->FrameDisplayOrder;
                inputFrame->m_pyramidLayer = pInfo->Layer;
                ThrowIf(inputFrame->m_pyramidLayer >= m_videoParam.BiPyramidLayers, std::runtime_error(""));
            } else { // CQP or internal BRC
                inputFrame->m_frameOrder   = surface->Data.FrameOrder;
                inputFrame->m_pyramidLayer = 0;//will be set after
                if (ctrl->QP > 0 && ctrl->QP < 52)
                    inputFrame->m_sliceQpY = (uint8_t)ctrl->QP;
            }
#endif
            if (!(inputFrame->m_picCodeType & MFX_FRAMETYPE_B)) {
                m_frameOrderOfLastIdr = m_frameOrderOfLastIdrB;
                m_frameOrderOfLastIntra = m_frameOrderOfLastIntraB;
                m_frameOrderOfLastAnchor = m_frameOrderOfLastAnchorB;
            }
        }
        else if (ctrl) {
            const mfxExtAVCRefListCtrl *ltctrl = (mfxExtAVCRefListCtrl *)GetExtBufferById(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_AVC_REFLIST_CTRL);
            if (ltctrl) {
                inputFrame->refctrl = *ltctrl;
                //get cur frame long term only
                for (int i = 0; i < 16 && ltctrl->LongTermRefList[i].FrameOrder != 0xffffffff; i++) {
                    if (ltctrl->LongTermRefList[i].FrameOrder == (uint32_t)m_frameOrder) {
                        inputFrame->m_isLtrFrame = 1;
                        inputFrame->m_isExternalLTR = ltctrl->LongTermRefList[i].LongTermIdx + 1;
                        //printf("\nExternal LTR Idx %d\n", ltctrl->LongTermRefList[i].LongTermIdx);
                        if (m_frameOrderOfLastExternalLTR[ltctrl->LongTermRefList[i].LongTermIdx] == MFX_FRAMEORDER_UNKNOWN)
                            m_numberOfExternalLtrFrames++;
                        m_frameOrderOfLastExternalLTR[ltctrl->LongTermRefList[i].LongTermIdx] = m_frameOrder;
                    }
                }
                if (ltctrl->PreferredRefList[0].FrameOrder != 0xffffffff || ltctrl->RejectedRefList[0].FrameOrder != 0xffffffff) 
                {
                    //printf("\n In %d PreferredRefList 0 %d ", m_frameOrder, ltctrl->PreferredRefList[0].FrameOrder);
                    //printf("RejectedRefList 0 %d\n", ltctrl->RejectedRefList[0].FrameOrder);
                    inputFrame->m_hasRefCtrl = 1;
                }
            }

            if (((ctrl->FrameType&MFX_FRAMETYPE_IDR) || (ctrl->FrameType&MFX_FRAMETYPE_I))) {
                inputFrame->m_picCodeType = ctrl->FrameType;
                if (inputFrame->m_picCodeType & MFX_FRAMETYPE_IDR) {
                    inputFrame->m_picCodeType = MFX_FRAMETYPE_I;
                    inputFrame->m_isIdrPic = true;
                    inputFrame->m_poc = 0;
                    inputFrame->m_isRef = 1;
                }
                else if (inputFrame->m_picCodeType & MFX_FRAMETYPE_REF) {
                    inputFrame->m_picCodeType &= ~MFX_FRAMETYPE_REF;
                    inputFrame->m_isRef = 1;
                }
            }

            if (ctrl->QP > 0 && ctrl->QP <= 255) {
                inputFrame->m_sliceQpY = (uint8_t)ctrl->QP;
            }

            if (m_videoParam.ctrlMBQP) {
                const mfxExtMBQP *mbqpctrl = (mfxExtMBQP *)GetExtBufferById(ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_MBQP);
                if (mbqpctrl && mbqpctrl->NumQPAlloc && mbqpctrl->Mode == 1) {
                    // check
                    int32_t qpLine = ((m_videoParam.Width + 16 - 1) / 16);
                    int32_t numQP = qpLine * ((m_videoParam.Height + 16 - 1) / 16);
                    if (mbqpctrl->NumQPAlloc >= numQP) {
                        inputFrame->m_hasMbqpCtrl = 1;
                        inputFrame->mbqpctrl = *mbqpctrl;
                    }
                }
            }
        }

        bool bEncCtrl = m_videoParam.encodedOrder ||
                       !m_videoParam.encodedOrder && ctrl && ((ctrl->FrameType&MFX_FRAMETYPE_IDR) || (ctrl->FrameType&MFX_FRAMETYPE_I));

        ConfigureInputFrame(inputFrame, !!m_videoParam.encodedOrder, bEncCtrl);
        UpdateGopCounters(inputFrame, !!m_videoParam.encodedOrder);
    } else {
        // tmp fix for short sequences
        if (m_la.get()) {
            if (m_videoParam.AnalyzeCmplx) {
                for (std::list<Frame *>::iterator i = m_inputQueue.begin(); i != m_inputQueue.end(); ++i)
                    AverageComplexity(*i, m_videoParam);
            }
#if 0
            if (m_videoParam.DeltaQpMode & (AMT_DQP_PAQ | AMT_DQP_CAL)) {
                while (m_la.get()->BuildQpMap_AndTriggerState(NULL));
            }
#endif
            m_lookaheadQueue.splice(m_lookaheadQueue.end(), m_inputQueue);
            m_la->ResetState();
        }
    }

    ReorderInput(surface == NULL);

    return inputFrame;
}


namespace AV1PP {
    template <int w, int h> int sad_general_avx2(const uint8_t *src1, int pitch1, const uint8_t *src2, int pitch2);
};

void AV1Encoder::EnqueueFrameEncoder(AV1EncodeTaskInputParams *inputParam)
{
    Frame *frame = m_encodeQueue.front();

    for (size_t encIdx = 0; encIdx < m_frameEncoder.size(); encIdx++) {
        if (m_frameEncoder[encIdx]->IsFree()) {
            m_frameEncoder[encIdx]->SetFree(false);
            frame->m_fenc = m_frameEncoder[encIdx];
            break;
        }
    }
    ThrowIf(!frame->m_fenc, std::runtime_error(""));

#if ENABLE_BITSTREAM_MEM_REDUCTION
    if (m_videoParam.ZeroDelay && !m_brc) {
        frame->m_fenc->SetBitsreamPtr(*inputParam->bs);
    }
#endif

    CdefParamInit(frame->m_cdefParam);
    memset(frame->m_fenc->m_cdefStrenths, 0, m_videoParam.sb64Rows * m_videoParam.sb64Cols);


    if (m_videoParam.enableCmFlag) {
        if (!frame->IsIntra()) {
            frame->m_ttSubmitGpuCopySrc.CommonInit(TT_GPU_SUBMIT);
            frame->m_ttSubmitGpuCopySrc.numUpstreamDependencies = 1;  // guard, prevent task from running
            frame->m_ttSubmitGpuCopySrc.poc = frame->m_frameOrder;
            AddTaskDependencyThreaded(&frame->m_ttSubmitGpuCopySrc, &frame->m_ttInitNewFrame); // GPU_SUBMIT_COPY_SRC <- INIT_NEW_FRAME

            for (int32_t ref = LAST_FRAME; ref <= ALTREF_FRAME; ref++) {
                if (frame->isUniq[ref])
                    for (int32_t blksize = 0; blksize < 4; blksize++)
                        if (!frame->m_feiInterData[ref][blksize])
                            frame->m_feiInterData[ref][blksize] = m_feiInterDataPool[blksize].Allocate();
            }

            for (int32_t i = 0; i < m_videoParam.numGpuSlices; i++) {
                for (int32_t ref = LAST_FRAME; ref <= ALTREF_FRAME; ref++) {
                    if (frame->isUniq[ref]) {
                        frame->m_ttSubmitGpuMe[ref][i].InitGpuSubmit(frame, MFX_FEI_H265_OP_INTER_ME, frame->m_frameOrder);
                        frame->m_ttSubmitGpuMe[ref][i].refIdx = (int16_t)ref;
                        frame->m_ttSubmitGpuMe[ref][i].sliceIdx = (uint16_t)i;

                        if (i == 0)
                            AddTaskDependencyThreaded(&frame->m_ttSubmitGpuMe[ref][i], &frame->m_ttSubmitGpuCopySrc); // GPU_SUBMIT_ME <- GPU_SUBMIT_COPY_SRC
                        else
                            AddTaskDependencyThreaded(&frame->m_ttSubmitGpuMe[ref][i], &frame->m_ttSubmitGpuMd[i - 1]); // GPU_SUBMIT_ME(slice) <- MD(slice - 1)
                    }
                }

                frame->m_ttSubmitGpuMd[i].InitGpuSubmit(frame, MFX_FEI_H265_OP_INTER_MD, frame->m_frameOrder);
                frame->m_ttSubmitGpuMd[i].sliceIdx = (int16_t)i;
                frame->m_ttWaitGpuMd[i].InitGpuWait(MFX_FEI_H265_OP_INTER_MD, frame->m_frameOrder);
                frame->m_ttWaitGpuMd[i].sliceIdx = (int16_t)i;

                AddTaskDependencyThreaded(&frame->m_ttWaitGpuMd[i], &frame->m_ttSubmitGpuMd[i]); // GPU_WAIT_MD <- GPU_SUBMIT_MD

                if (i == 0) {
                    if (m_videoParam.ZeroDelay && m_videoParam.QuickStartPFrame && frame->m_picCodeType == MFX_FRAMETYPE_P)
                        AddTaskDependencyThreaded(&frame->m_ttSubmitGpuMd[0], &frame->m_ttLookahead[frame->m_numLaTasks - 1]); // MD <- TT_PREENC_END for rscs
                }

                AddTaskDependency(&frame->m_ttSubmitGpuMd[i], &frame->m_ttSubmitGpuMe[LAST_FRAME][i]); // MD <- ME(last)
                if (frame->compoundReferenceAllowed)
                    AddTaskDependency(&frame->m_ttSubmitGpuMd[i], &frame->m_ttSubmitGpuMe[ALTREF_FRAME][i]); // MD <- ME(golden)
                else if (frame->isUniq[GOLDEN_FRAME])
                    AddTaskDependency(&frame->m_ttSubmitGpuMd[i], &frame->m_ttSubmitGpuMe[GOLDEN_FRAME][i]); // MD <- ME(altref)
            }
        }

        if (m_videoParam.GopPicSize != 1 && frame->m_isRef) {
            frame->m_ttSubmitGpuCopyRec.CommonInit(TT_GPU_SUBMIT);
            frame->m_ttSubmitGpuCopyRec.poc = frame->m_frameOrder;
            frame->m_ttWaitGpuCopyRec.CommonInit(TT_GPU_WAIT);
            frame->m_ttWaitGpuCopyRec.poc = frame->m_frameOrder;
            frame->m_ttWaitGpuCopyRec.syncpoint = NULL;

            AddTaskDependencyThreaded(&frame->m_ttWaitGpuCopyRec, &frame->m_ttSubmitGpuCopyRec); // GPU_WAIT_COPY_REC <- GPU_SUBMIT_COPY_REC
        }
    }


    if (frame->m_sliceQpY == 0 || m_brc) {
#if ENABLE_BRC
        if (m_brc) {
            int minQp = ((AV1BRC*)m_brc)->mMinQp;
            frame->m_sliceQpBrc = GetRateQp(*frame, m_videoParam, m_brc);
            // Convert to Lambda
            CostType lambdaMult;
            bool extraMult = SliceLambdaMultiplier(lambdaMult, m_videoParam, frame->m_picCodeType, frame, false, false);
            double lambda = h265_lambda[frame->m_sliceQpBrc + 48] * lambdaMult;
            if (extraMult) {
                if (((AV1BRC*)m_brc)->mLayerRatio > 0.3) {
                    double val = 0.3 / ((AV1BRC*)m_brc)->mLayerRatio;
                    lambda *= Saturate(1.25, 4.0*val, val * (frame->m_sliceQpBrc - 12) / 6.f);
                } else {
                    lambda *= Saturate(2, 4, (frame->m_sliceQpBrc - 12) / 6.f);
                }
            }
            // Convert to Lambda QP
            float lqp = (4.2005f*log(lambda) + 13.7122f); // est
            // Convert to Qindex
            float qc = pow(2.0f, (lqp - 4.0f) / 6.0f);
            frame->m_sliceQpY = vp9_dc_qindex((int32_t)(qc * 8.0f + 0.5f));

            // Update LTR triggered by BRC Qp
            if (!frame->m_isLtrFrame && frame->m_picCodeType == MFX_FRAMETYPE_P && frame->m_pyramidLayer == 0) {
                Frame *ltrFrame = NULL;
                for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i) {
                    Frame *ref = (*i);
                    if (ref->m_isLtrFrame) {
                        if (!ltrFrame) ltrFrame = ref;
                        else if (ref->m_frameOrder > ltrFrame->m_frameOrder) ltrFrame = ref;
                    }
                }
                if (ltrFrame && frame->m_sliceQpY < ltrFrame->m_sliceQpY - 1 && ltrFrame->m_isLtrFrame && !ltrFrame->m_isExternalLTR) {
                    frame->m_isLtrFrame = 1;
                    frame->m_ltrConfidenceLevel = 0;
                    // Minor Boost
                    int refqp = MIN(ltrFrame->m_sliceQpBrc, frame->refFramesVp9[LAST_FRAME]->m_sliceQpBrc);
                    int32_t extra = (m_videoParam.MaxFrameSizeInBits && m_videoParam.RepackForMaxFrameSize) ? 0 : 1;
                    frame->m_sliceQpBrc = MAX(minQp, MAX(frame->m_qpMinForMaxFrameSize-extra, MIN(frame->m_sliceQpBrc, MAX(refqp-3, frame->m_sliceQpBrc-m_videoParam.LTRConfidenceLevel))));
                    // Convert to Lambda
                    extraMult = SliceLambdaMultiplier(lambdaMult, m_videoParam, frame->m_picCodeType, frame, false, false);
                    lambda = h265_lambda[frame->m_sliceQpBrc + 48] * lambdaMult;
                    if (extraMult)
                        lambda *= Saturate(2, 4, (frame->m_sliceQpBrc - 12) / 6.f);
                    // Convert to Lambda QP
                    lqp = (4.2005f*log(lambda) + 13.7122f); // est
                    // Convert to Qindex
                    qc = pow(2.0f, (lqp - 4.0f) / 6.0f);
                    frame->m_sliceQpY = vp9_dc_qindex((int32_t)(qc * 8.0f + 0.5f));
                }
            }
        }
        else
#endif
        {
            frame->m_sliceQpY = GetConstQp(*frame, m_videoParam, inputParam->m_taskReencode.load());
        }

        //frame->m_sliceQpY = GetPAQOffset(*frame, m_videoParam, frame->m_sliceQpY);
    }

    frame->bitCount = &frame->m_fenc->m_topEnc.bitCount;
    GetRefFrameContextsAv1(frame);

    int16_t q = vp9_dc_quant(frame->m_sliceQpY, 0, m_videoParam.bitDepthLuma);
    if (m_videoParam.DeltaQpMode&AMT_DQP_CAL) {
        // Place holder
        frame->m_lambda = (88 * q * q / 24.f / 512 / 16 / 128) / 1.08f;
    } else {
        frame->m_lambda = 88 * q * q / 24.f / 512 / 16 / 128;
    }
    frame->m_lambdaSatd = sqrt(frame->m_lambda * 512) / 512;
    //frame->m_lambdaSatdInt = q * 1386; // == int(frame->m_lambdaSatd * (1<<24))
    frame->m_lambdaSatdInt = int(frame->m_lambdaSatd * (1 << 24));

    // Use DQP
    if (m_videoParam.UseDQP) {
        std::fill(frame->m_lcuQps.begin(), frame->m_lcuQps.end(), frame->m_sliceQpY);
        if(frame->m_hasMbqpCtrl)
            ApplyMBDeltaQp(frame, m_videoParam);
        else if (m_videoParam.numRoi)
            ApplyRoiDeltaQp(frame, m_videoParam);
#ifdef AMT_HROI_PSY_AQ
        else if (m_videoParam.DeltaQpMode & AMT_DQP_PSY_HROI)
            ApplyHRoiDeltaQp(frame, m_videoParam, m_brc ? 1 : 0);
#endif
        else if (m_videoParam.DeltaQpMode)
            ApplyDeltaQp(frame, m_videoParam);
    }

    if (m_videoParam.deblockingFlag) {
        LoopFilterFrameParam *params = &frame->m_loopFilterParam;
        LoopFilterResetParams(params);
        params->sharpness = m_videoParam.deblockingSharpness;
        params->level = (uint8_t)FilterLevelFromQp(frame->m_sliceQpY, (uint8_t)frame->IsIntra());
    } else {
        Zero(frame->m_loopFilterParam);
    }

    int32_t nb_strength_bits = 3;
    int32_t predefined_strengths[2][8] = {
        { 0, 2, 4, 4, 6, 6, 8, 8 },
        { 0, 0, 0, 2, 2, 4, 4, 8 }
    };
    frame->m_cdefParam.cdef_bits = 3;
    frame->m_cdefParam.nb_cdef_strengths = 1 << nb_strength_bits;
    for (int i = 0; i < 8; i++) {
        frame->m_cdefParam.cdef_strengths[i] = 4 * predefined_strengths[0][i];
        frame->m_cdefParam.cdef_uv_strengths[i] = 4 * predefined_strengths[1][i];
    }

#if 1
    frame->m_cdefParam.mode = CDEF_DEFAULT;
    if (frame->m_sliceQpY < 90) {
        frame->m_cdefParam.cdef_bits = 2;
        frame->m_cdefParam.cdef_strengths[0] = 0;
        frame->m_cdefParam.cdef_strengths[1] = 4;
        frame->m_cdefParam.cdef_strengths[2] = 5;
        frame->m_cdefParam.cdef_strengths[3] = 6;

        frame->m_cdefParam.mode = CDEF_456;
    }
    else if (frame->m_sliceQpY < 130) {
        frame->m_cdefParam.cdef_bits = 2;
        frame->m_cdefParam.cdef_strengths[0] = 0;
        frame->m_cdefParam.cdef_strengths[1] = 4;
        frame->m_cdefParam.cdef_strengths[2] = 8;
        frame->m_cdefParam.cdef_strengths[3] = 9;

        frame->m_cdefParam.mode = CDEF_489;
    }
    else if (frame->m_sliceQpY < 140) {
        frame->m_cdefParam.cdef_bits = 2;
        frame->m_cdefParam.cdef_strengths[0] = 0;
        frame->m_cdefParam.cdef_strengths[1] = 32;
        frame->m_cdefParam.cdef_strengths[2] = 36;
        frame->m_cdefParam.cdef_strengths[3] = 63;
    }
    else /*if (frame->m_sliceQpY < 140)*/ {
        frame->m_cdefParam.cdef_bits = 2;
        frame->m_cdefParam.cdef_strengths[0] = 0;
        frame->m_cdefParam.cdef_strengths[1] = 24;
        frame->m_cdefParam.cdef_strengths[2] = 36;
        frame->m_cdefParam.cdef_strengths[3] = 63;
    }

    frame->m_cdefParam.nb_cdef_strengths = 1 << frame->m_cdefParam.cdef_bits;
    for (int i = 0; i < frame->m_cdefParam.nb_cdef_strengths; i++) {
        frame->m_cdefParam.cdef_uv_strengths[i] = frame->m_cdefParam.cdef_strengths[i];
    }
#endif

    if (m_videoParam.superResFlag) {
        frame->m_superResParam.use = 1;
        frame->m_superResParam.denom = SUPERRES_SCALE_DENOMINATOR_DEFAULT;
    }
    /*
    frame->m_allowIntraBc = 0;
    if (frame->m_picCodeType == MFX_FRAMETYPE_I && (m_videoParam.screenMode&0x1)) {
        frame->m_allowIntraBc = 1;
        if (m_videoParam.screenMode & 0x4) {
            frame->m_allowIntraBc = frame->hasTextContent;
        }
    }
    */
    frame->m_tileParam = (frame->m_picCodeType == MFX_FRAMETYPE_I) ? m_videoParam.tileParamKeyFrame : m_videoParam.tileParamInterFrame;

    if (frame->m_allowIntraBc) {
    //if (frame->m_picCodeType == MFX_FRAMETYPE_I && (m_videoParam.screenMode & 0x1)) {
        frame->m_fenc->m_tmpBufForHash.resize(frame->m_tileParam.numTiles);
        frame->m_fenc->m_tmpBufForHashSize.resize(frame->m_tileParam.numTiles);
        frame->m_fenc->m_hash[0].resize(frame->m_tileParam.numTiles);
		frame->m_fenc->m_hashIndexes[0].resize((m_videoParam.Width >> 3) * (m_videoParam.Height >> 3));
#if !ENABLE_ONLY_HASH8
        frame->m_fenc->m_hash[1].resize(frame->m_tileParam.numTiles);
        frame->m_fenc->m_hashIndexes[1].resize((m_videoParam.Width >> 4) * (m_videoParam.Height >> 4));
#endif

        for (int32_t i = 0, tileIdx = 0; i < frame->m_tileParam.rows; i++) {
            for (int32_t j = 0; j < frame->m_tileParam.cols; j++, tileIdx++) {
                TileBorders tb;
                tb.rowStart = frame->m_tileParam.miRowStart[i];
                tb.rowEnd   = frame->m_tileParam.miRowEnd[i];
                tb.colStart = frame->m_tileParam.miColStart[j];
                tb.colEnd   = frame->m_tileParam.miColEnd[j];
#if ENABLE_HASH_MEM_REDUCTION
                AllocTmpBuf(tb, &frame->m_fenc->m_tmpBufForHash[tileIdx], &frame->m_fenc->m_tmpBufForHashSize[tileIdx], &frame->m_fenc->m_workBufForHash);
#else
                AllocTmpBuf(tb, &frame->m_fenc->m_tmpBufForHash[tileIdx], &frame->m_fenc->m_tmpBufForHashSize[tileIdx]);
#endif
#if ENABLE_HASH_MEM_REDUCTION
                BuildHashTable_avx2_lowmem(*frame->m_origin, tb, frame->m_fenc->m_tmpBufForHash[tileIdx],
                    frame->m_fenc->m_hash[0][tileIdx],
                    frame->m_fenc->m_hashIndexes[0],
                    frame->m_fenc->m_workBufForHash
                );
#else
                BuildHashTable_avx2(*frame->m_origin, tb, frame->m_fenc->m_tmpBufForHash[tileIdx],
                                    frame->m_fenc->m_hash[0][tileIdx],
                                    frame->m_fenc->m_hashIndexes[0]
#if !ENABLE_ONLY_HASH8
                                    , frame->m_fenc->m_hash[1][tileIdx],
                                      frame->m_fenc->m_hashIndexes[1]
#elif ENABLE_HASH_MEM_REDUCTION
                                    , frame->m_fenc->m_workBufForHash
#endif
                                   );
#endif
            }
        }
    }
    else {
        for (int i = 0; i < frame->m_fenc->m_tmpBufForHash.size(); i++) {
            if (frame->m_fenc->m_tmpBufForHash[i]) {
                AV1_Free(frame->m_fenc->m_tmpBufForHash[i]);
                frame->m_fenc->m_tmpBufForHash[i] = nullptr;
                frame->m_fenc->m_tmpBufForHashSize[i] = 0;
            }
        }
#if ENABLE_HASH_MEM_REDUCTION
        AV1_SafeFree(frame->m_fenc->m_workBufForHash);
#endif
        for (int i = 0; i < frame->m_fenc->m_hash[0].size(); i++) {
            HashTable &h = frame->m_fenc->m_hash[0][i];
            h.Free();
        }
        frame->m_fenc->m_hashIndexes[0].resize(0);
    }

    frame->m_ttEncComplete.CommonInit(TT_ENC_COMPLETE);
    frame->m_ttEncComplete.numUpstreamDependencies = 1; // decremented when frame becomes a target frame
    frame->m_ttEncComplete.poc = frame->m_frameOrder;


    frame->m_ttDetectScrollStart.CommonInit(TT_DETECT_SCROLL_START);
    frame->m_ttDetectScrollStart.poc = frame->m_frameOrder;
    frame->m_ttDetectScrollStart.numUpstreamDependencies = 1;  // guard, prevent task from running
    frame->m_ttDetectScrollEnd.CommonInit(TT_DETECT_SCROLL_END);
    frame->m_ttDetectScrollEnd.poc = frame->m_frameOrder;
    AddTaskDependencyThreaded(&frame->m_ttDetectScrollStart, &frame->m_ttInitNewFrame);  // TT_DETECT_SCROLL_START <- TT_INIT_NEW_FRAME
    for (int32_t i = 0; i < frame->m_numDetectScrollRegions; i++) {
        frame->m_ttDetectScrollRoutine[i].CommonInit(TT_DETECT_SCROLL_ROUTINE);
        frame->m_ttDetectScrollRoutine[i].poc = frame->m_frameOrder;
        AddTaskDependency(&frame->m_ttDetectScrollRoutine[i], &frame->m_ttDetectScrollStart); // TT_DETECT_SCROLL_ROUTINE <- TT_DETECT_SCROLL_START
        AddTaskDependency(&frame->m_ttDetectScrollEnd, &frame->m_ttDetectScrollRoutine[i]); // TT_DETECT_SCROLL_END <- TT_DETECT_SCROLL_ROUTINE
    }
    if (frame->IsIntra())
        AddTaskDependency(&frame->m_ttEncComplete, &frame->m_ttDetectScrollEnd); // TT_ENC_COMPLETE <- TT_DETECT_SCROLL_END
    else
        AddTaskDependency(&frame->m_ttSubmitGpuMe[LAST_FRAME][0], &frame->m_ttDetectScrollEnd); // GPU_ME(last) <- TT_DETECT_SCROLL_END


    frame->m_fenc->SetEncodeFrame(frame, &m_pendingTasks);

    if (!m_outputQueue.empty())
        AddTaskDependencyThreaded(&frame->m_ttPackHeader, &m_outputQueue.back()->m_ttPackHeader);

    if (m_videoParam.enableCmFlag) {
        Frame *refMax = nullptr;
        for (int32_t i = LAST_FRAME; i <= ALTREF_FRAME; i++) {
            if (frame->isUniq[i]) {
                Frame *r = frame->refFramesVp9[i];
                if (!refMax || refMax->m_frameOrder < r->m_frameOrder) {
                    refMax = r;
                }
            }
        }

		if (!frame->IsIntra()) {
			for (int32_t i = LAST_FRAME; i <= ALTREF_FRAME; i++) {
				if (frame->isUniq[i]) {
					Frame *ref = frame->refFramesVp9[i];
					AddTaskDependencyThreaded(&frame->m_ttSubmitGpuMe[i][0], &ref->m_ttSubmitGpuCopyRec); // GPU_SUBMIT_HME <- GPU_COPY_REF //only after ref
					//if (ref != refMax) {
					//	AddTaskDependencyThreaded(&frame->m_ttSubmitGpuMe[i], &refMax->m_ttWaitGpuMd); // GPU_SUBMIT_HME <- GPU_WAIT_MD  //after MD for latest ref
					//}
				}
			}
		}

        if (frame->m_ttSubmitGpuCopySrc.numUpstreamDependencies.fetch_sub(1) == 1) {
            std::lock_guard<std::mutex> guard(m_feiCritSect);
            m_feiSubmitTasks.push_back(&frame->m_ttSubmitGpuCopySrc); // GPU_COPY_SRC is independent
            m_feiCondVar.notify_one();
        }

        if (frame->m_ttDetectScrollStart.numUpstreamDependencies.fetch_sub(1) == 1) {
            std::lock_guard<std::mutex> guard(m_critSect);
            m_pendingTasks.push_back(&frame->m_ttDetectScrollStart); // TT_DETECT_SCROLL_START is independent
            m_condVar.notify_one();
        }

        m_outputQueue.splice(m_outputQueue.end(), m_encodeQueue, m_encodeQueue.begin());
    } else {
        m_outputQueue.splice(m_outputQueue.end(), m_encodeQueue, m_encodeQueue.begin());
    }
}


mfxStatus AV1Encoder::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, MFX_ENTRY_POINT &entryPoint)
{
    m_frameCountSync++;

    if (!surface) {
        if (m_frameCountBufferedSync == 0) // buffered frames to be encoded
            return MFX_ERR_MORE_DATA;
        m_frameCountBufferedSync--;
    }

    int32_t buffering = 0;
    if (!m_videoParam.encodedOrder) {
        int32_t enc_buffering = 0;
        int32_t la_buffering = 0;                                        // num la frames needed to release one frame to reorder queue
        if (!m_videoParam.ZeroDelay) {
            enc_buffering += m_videoParam.GopRefDist - 1;
            enc_buffering += m_videoParam.m_framesInParallel - 1;
            enc_buffering += MAX(0, m_videoParam.RateControlDepth - 1); // Look ahead Analysis for BRC
            la_buffering += 1; // RsCs
            if (m_videoParam.SceneCut || m_videoParam.AnalyzeCmplx || m_videoParam.DeltaQpMode) {
                if (m_videoParam.AnalyzeCmplx) la_buffering = MAX(la_buffering, 2);       // only prev frame needed
                if (m_videoParam.DeltaQpMode&(AMT_DQP_PAQ | AMT_DQP_CAL)) la_buffering = MAX(la_buffering, 2 * m_videoParam.GopRefDist + 1);
            }
        }
        buffering += (enc_buffering + la_buffering);
    } else {
        buffering += m_videoParam.m_framesInParallel;
        buffering += 1; // RsCs or CAQ
        if (m_videoParam.AnalyzeCmplx) buffering += m_videoParam.RateControlDepth;
    }

#ifdef MFX_MAX_ENCODE_FRAMES
    if ((mfxI32)m_frameCountSync > MFX_MAX_ENCODE_FRAMES + buffering + 1)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
#endif // MFX_MAX_ENCODE_FRAMES

    mfxStatus status = MFX_ERR_NONE;
    if (surface && (int32_t)m_frameCountBufferedSync < buffering) {
        m_frameCountBufferedSync++;
        status = (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK;
    }

    if (surface) {
        m_core.IncreaseReference(&surface->Data);
        if (m_videoParam.picStruct != PROGR)
            m_core.IncreaseReference(&surface->Data);
        MfxAutoMutex guard(m_statMutex);
        m_stat.NumCachedFrame++;
    }

    AV1EncodeTaskInputParams *m_pTaskInputParams = (AV1EncodeTaskInputParams*)AV1_Malloc(sizeof(AV1EncodeTaskInputParams));
    // MFX_ERR_MORE_DATA_RUN_TASK means that frame will be buffered and will be encoded later.
    // Output bitstream isn't required for this task. it is marker for TaskRoutine() and TaskComplete()
    m_pTaskInputParams->bs = (status == (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK) ? 0 : bs;
    m_pTaskInputParams->ctrl = ctrl;
    m_pTaskInputParams->surface = surface;
    m_pTaskInputParams->m_targetFrame[0] = NULL;
    m_pTaskInputParams->m_targetFrame[1] = NULL;
    m_pTaskInputParams->m_taskID = m_taskSubmitCount++;
    m_pTaskInputParams->m_taskStage.exchange(THREADING_ITASK_INI);
    m_pTaskInputParams->m_taskReencode = 0;
    m_pTaskInputParams->m_newFrame[0] = NULL;
    m_pTaskInputParams->m_newFrame[1] = NULL;

    entryPoint.pRoutine = TaskRoutine;
    entryPoint.pCompleteProc = TaskCompleteProc;
    entryPoint.pState = this;
    entryPoint.requiredNumThreads = m_videoParam.num_threads;
    entryPoint.pRoutineName = "EncodeAV1";
    entryPoint.pParam = m_pTaskInputParams;

    return status;
}

mfxStatus AV1Encoder::SyncOnFrameCompletion(AV1EncodeTaskInputParams *inputParam, Frame *frame)
{
    mfxBitstream *mfxBs = inputParam->bs;

    AV1FrameEncoder *frameEnc = frame->m_fenc;
    int32_t overheadBytes = 0;
    mfxBitstream* bs = mfxBs;
    mfxU32 initialDataLength = bs->DataLength;
    uint32_t dataLength0 = mfxBs->DataLength;
    int32_t overheadBytes0 = overheadBytes;

    overheadBytes = frameEnc->GetOutputData(*bs);

#if ENABLE_BRC
    // BRC
    if (m_brc || m_videoParam.randomRepackThreshold) {
        std::unique_lock<std::mutex> guard(m_critSect);
        // THREADING_WORKING -> THREADING_PAUSE
        assert(m_doStage == THREADING_WORKING);
        m_doStage = THREADING_PAUSE;
        guard.unlock();

        const int32_t min_qp = 1;
        int32_t frameBytes = bs->DataLength - initialDataLength;
        mfxBRCStatus brcSts = m_brc ? m_brc->PostPackFrame(&m_videoParam, frame->m_sliceQpBrc, frame, frameBytes << 3, overheadBytes << 3, inputParam->m_taskReencode.load())
                                    : (myrand() < m_videoParam.randomRepackThreshold ? MFX_BRC_ERR_BIG_FRAME : MFX_BRC_OK);

        if (brcSts != MFX_BRC_OK ) {
            if ((brcSts & MFX_BRC_ERR_SMALL_FRAME) && (frame->m_sliceQpY < min_qp))
                brcSts |= MFX_BRC_NOT_ENOUGH_BUFFER;
            if (brcSts == MFX_BRC_ERR_BIG_FRAME && frame->m_sliceQpY == MAXQ)
                brcSts |= MFX_BRC_NOT_ENOUGH_BUFFER;

            if (!(brcSts & MFX_BRC_NOT_ENOUGH_BUFFER)) {
                bs->DataLength = dataLength0;
                overheadBytes = overheadBytes0;

                while (m_threadingTaskRunning.load() > 1) std::this_thread::yield();

                if (m_videoParam.enableCmFlag) {
                    m_feiCritSect.lock();
                    m_pauseFeiThread = 1;
                    m_feiCritSect.unlock();
                    while (m_feiThreadRunning) std::this_thread::yield();
                    m_feiSubmitTasks.resize(0);
                    for (std::deque<ThreadingTask *>::const_iterator i = m_feiWaitTasks.begin(); i != m_feiWaitTasks.end(); ++i) {
                        H265FEI_SyncOperation(m_fei, (*i)->syncpoint, 0xFFFFFFFF);
                        H265FEI_DestroySavedSyncPoint(m_fei, (*i)->syncpoint);
                    }
                    m_feiWaitTasks.resize(0);
                }

                m_pendingTasks.resize(0);
                m_encodeQueue.splice(m_encodeQueue.begin(), m_outputQueue);
                m_ttHubPool.ReleaseAll();
                inputParam->m_ttComplete.InitComplete(-1);
                inputParam->m_ttComplete.numUpstreamDependencies = 1;
                m_nextFrameToEncode--;
                if (frame->m_isLtrFrame && frame->m_ltrConfidenceLevel == 0) // revist Ltr update decision
                    frame->m_isLtrFrame = 0;
                PrepareToEncode(inputParam, (m_videoParam.ZeroDelay == 1) ? true : false);
                if (inputParam->m_ttComplete.numUpstreamDependencies.fetch_sub(1) == 1)
                    assert(0);

                if (m_videoParam.enableCmFlag) {
                    m_feiCritSect.lock();
                    m_pauseFeiThread = 0;
                    m_feiCritSect.unlock();
                    m_feiCondVar.notify_one();
                }

                guard.lock();
                inputParam->m_taskReencode++;
                m_reencode++;
                assert(m_threadingTaskRunning.load() == 1);
                // THREADING_PAUSE -> THREADING_WORKING
                assert(m_doStage == THREADING_PAUSE);
                m_doStage = THREADING_WORKING;
                guard.unlock();

                if (m_videoParam.num_threads > 1)
                    m_condVar.notify_all();

                return MFX_TASK_WORKING; // recode!!!

            } else if (brcSts & MFX_BRC_ERR_SMALL_FRAME) {

                int32_t maxSize, minSize, bitsize = frameBytes << 3;
                uint8_t *p = mfxBs->Data + mfxBs->DataOffset + mfxBs->DataLength;
                m_brc->GetMinMaxFrameSize(&minSize, &maxSize);

                // put rbsp_slice_segment_trailing_bits() which is a sequence of cabac_zero_words
                int32_t numTrailingBytes = std::max(0, ((minSize + 7) >> 3) - frameBytes);
                int32_t maxCabacZeroWords = (mfxBs->MaxLength - frameBytes) / 3;
                int32_t numCabacZeroWords = std::min(maxCabacZeroWords, (numTrailingBytes + 2) / 3);

                for (int32_t i = 0; i < numCabacZeroWords; i++) {
                    *p++ = 0x00;
                    *p++ = 0x00;
                    *p++ = 0x03;
                }
                bitsize += numCabacZeroWords * 24;

                m_brc->PostPackFrame(&m_videoParam,  frame->m_sliceQpBrc, frame, bitsize, (overheadBytes << 3) + bitsize - (frameBytes << 3), 1);
                mfxBs->DataLength += (bitsize >> 3) - frameBytes;

            } else {
                //return MFX_ERR_NOT_ENOUGH_BUFFER;
            }
        }

        // bs update on completion stage
        guard.lock();
        // THREADING_PAUSE -> THREADING_WORKING
        assert(m_doStage == THREADING_PAUSE);
        m_doStage = THREADING_WORKING;
        guard.unlock();
    }
#else // ENABLE_BRC
    dataLength0;
    overheadBytes0;
#endif // ENABLE_BRC

    if (m_videoParam.num_threads > 1)
        m_condVar.notify_all();

    MfxAutoMutex status_guard(m_statMutex);
    m_stat.NumCachedFrame--;
    m_stat.NumFrame++;
    m_stat.NumBit += (bs->DataLength - initialDataLength) * 8;

    return MFX_TASK_DONE;
}


class OnExitHelperRoutine
{
public:
    OnExitHelperRoutine(std::atomic<uint32_t>& arg) : m_arg(arg)
    {}
    ~OnExitHelperRoutine()
    {
        m_arg--;
    }

private:
    std::atomic<uint32_t>& m_arg;
};


namespace {
    Frame *FindFrameForLookaheadProcessing(std::list<Frame *> &queue)
    {
        for (FrameIter it = queue.begin(); it != queue.end(); it++)
            if (!(*it)->wasLAProcessed())
                return (*it);
        return NULL;
    }
};

void AV1Encoder::PrepareToEncodeNewTask(AV1EncodeTaskInputParams *inputParam)
{
    inputParam->m_newFrame[0] = inputParam->m_newFrame[1] = NULL;
    inputParam->m_newFrame[0] = AcceptFrame(inputParam->surface, inputParam->ctrl, inputParam->bs, 0);
    if (m_videoParam.picStruct != PROGR && inputParam->surface)
        inputParam->m_newFrame[1] = AcceptFrame(inputParam->surface, inputParam->ctrl, inputParam->bs, 1);
    if (m_la.get()) {
        m_laFrame[0] = FindFrameForLookaheadProcessing(m_inputQueue);
        if (Frame* f = m_laFrame[0])
            f->m_ttLookahead[f->m_numLaTasks - 1].finished = 0;
        if (m_videoParam.picStruct != PROGR) {
            if (m_laFrame[0])
                m_laFrame[0]->setWasLAProcessed();
            m_laFrame[1] = FindFrameForLookaheadProcessing(m_inputQueue);
            if (m_laFrame[0])
                m_laFrame[0]->unsetWasLAProcessed();
        }
    }
}

void AV1Encoder::PrepareToEncode(AV1EncodeTaskInputParams *inputParam, bool target_only)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "PrepareToEncode");
    if(!target_only){
    inputParam->m_ttComplete.InitComplete(-1);
    inputParam->m_ttComplete.numUpstreamDependencies = 1;

    if (inputParam->m_newFrame[0] && !inputParam->m_newFrame[0]->m_ttInitNewFrame.finished) {
        inputParam->m_newFrame[0]->m_ttInitNewFrame.InitNewFrame(inputParam->m_newFrame[0], inputParam->surface, inputParam->m_newFrame[0]->m_frameOrder);
        AddTaskDependencyThreaded(&inputParam->m_ttComplete, &inputParam->m_newFrame[0]->m_ttInitNewFrame); // COMPLETE <- INIT_NEW_FRAME
    }
    }
    bool isLookaheadConfigured = false;
    if (!target_only) {
    Frame* f = m_laFrame[0];
    if (f && !f->m_ttLookahead[f->m_numLaTasks - 1].finished) {
        if (m_la->ConfigureLookaheadFrame(m_laFrame[0], 0) == 1) {
            isLookaheadConfigured = true;
            AddTaskDependencyThreaded(&inputParam->m_ttComplete, &m_laFrame[0]->m_ttLookahead[f->m_numLaTasks - 1]); // COMPLETE <- TT_PREENC_END
            AddTaskDependencyThreaded(&f->m_ttLookahead[0], &m_laFrame[0]->m_ttInitNewFrame); // TT_PREENC_START <- INIT_NEW_FRAME
        }
    }
    }
    if (inputParam->bs) {
        while (m_outputQueue.size() < (size_t)m_videoParam.m_framesInParallel && !m_encodeQueue.empty())
            EnqueueFrameEncoder(inputParam);

        FrameIter targetFrameIter = m_outputQueue.begin();
        for (; targetFrameIter != m_outputQueue.end(); ++targetFrameIter)
            if ((**targetFrameIter).m_frameOrder == m_nextFrameToEncode)
                break;

        if (targetFrameIter != m_outputQueue.end()) {
            Frame &tf = **targetFrameIter;
            tf.showExistingFrame = 0;
            tf.m_hiddenFrames.clear();
            for (FrameIter i = m_outputQueue.begin(); i != targetFrameIter; ++i) {
                (**i).showExistingFrame = 1;
                tf.m_hiddenFrames.push_back(*i);
                AddTaskDependencyThreaded(&inputParam->m_ttComplete, &(**i).m_ttEncComplete); // COMPLETE <- SuperFrame[i].ENC_COMPLETE
            }
            if (!tf.m_hiddenFrames.empty()) {
                AddTaskDependencyThreaded(&tf.m_ttEncComplete, &tf.m_hiddenFrames.back()->m_ttEncComplete); // targetFrame.ENC_COMPLETE <- SuperFrame[last].ENC_COMPLETE
                for (int32_t i = 0; i < (int32_t)tf.m_hiddenFrames.size()-1; i++)
                    AddTaskDependencyThreaded(&tf.m_hiddenFrames[i+1]->m_ttEncComplete, &tf.m_hiddenFrames[i]->m_ttEncComplete); // SuperFrame[i+1].ENC_COMPLETE <- SuperFrame[i].ENC_COMPLETE
            }
            AddTaskDependencyThreaded(&inputParam->m_ttComplete, &tf.m_ttEncComplete); // COMPLETE <- TargetFrame.ENC_COMPLETE
            std::copy(tf.m_dpbVP9NextFrame, tf.m_dpbVP9NextFrame + NUM_REF_FRAMES, m_nextFrameDpbVP9);

            inputParam->m_targetFrame[0] = *targetFrameIter;
            inputParam->m_ttComplete.poc = inputParam->m_targetFrame[0]->m_frameOrder;
            m_nextFrameToEncode++;
        }
        else if (m_nextFrameDpbVP9[0] && m_videoParam.GopRefDist>1) {
            // find frame to repeat (idx in dpb)
            Frame **dpb = m_nextFrameDpbVP9;
            int32_t frameToShowMapIdx = 0;
            for (; frameToShowMapIdx < NUM_REF_FRAMES; frameToShowMapIdx++)
                if (dpb[frameToShowMapIdx]->m_frameOrder == m_nextFrameToEncode)
                    break;
            assert(frameToShowMapIdx < NUM_REF_FRAMES);
            Frame &tf = *dpb[frameToShowMapIdx];
            assert(tf.showExistingFrame == 1);
            tf.frameToShowMapIdx = frameToShowMapIdx;
            tf.m_ttFrameRepetition.poc = tf.m_frameOrder;
            tf.m_ttFrameRepetition.numDownstreamDependencies = 0;
            tf.m_ttFrameRepetition.numUpstreamDependencies = 1;
            tf.m_ttFrameRepetition.finished = 0;
            AddTaskDependencyThreaded(&tf.m_ttFrameRepetition, &tf.m_ttEncComplete); // REPEAT <- ENC_COMPLETE
            AddTaskDependencyThreaded(&inputParam->m_ttComplete, &tf.m_ttFrameRepetition); // COMPLETE <- REPEAT
            inputParam->m_targetFrame[0] = &tf;
            inputParam->m_ttComplete.poc = tf.m_frameOrder;
            m_nextFrameToEncode++;
        }

    } else {
        assert(inputParam->m_targetFrame[0] == NULL);
    }
    {
        std::lock_guard<std::mutex> guard(m_critSect);
        Frame *tframe = inputParam->m_targetFrame[0];
        if (m_videoParam.enableCmFlag && tframe && tframe->m_isRef && m_videoParam.GopPicSize != 1)
            AddTaskDependencyThreaded(&inputParam->m_ttComplete, &tframe->m_ttWaitGpuCopyRec); // COMPLETE <- GPU_WAIT_COPY_REC
        if(!target_only){
        if (inputParam->m_newFrame[0] && !inputParam->m_newFrame[0]->m_ttInitNewFrame.finished && inputParam->m_newFrame[0]->m_ttInitNewFrame.numUpstreamDependencies == 0)
            m_pendingTasks.push_back(&inputParam->m_newFrame[0]->m_ttInitNewFrame); // INIT_NEW_FRAME is independent
        }
        if (inputParam->bs && tframe && tframe->showExistingFrame == 0) {
            for (int32_t i = 0; i < (int32_t)tframe->m_hiddenFrames.size(); i++)
                if (tframe->m_hiddenFrames[i]->m_ttEncComplete.numUpstreamDependencies.fetch_sub(1) == 1) // unleash hidden frames since they will never become target frames
                    m_pendingTasks.push_front(&tframe->m_hiddenFrames[i]->m_ttEncComplete);
            if (tframe->m_ttEncComplete.numUpstreamDependencies.fetch_sub(1) == 1)
                m_pendingTasks.push_front(&tframe->m_ttEncComplete); // targetFrame.ENC_COMPLETE's dependencies are already resolved
        }

        if (inputParam->bs && tframe && tframe->showExistingFrame == 1
            && tframe->m_ttFrameRepetition.numUpstreamDependencies.fetch_sub(1) == 1)
            m_pendingTasks.push_front(&tframe->m_ttFrameRepetition); // targetFrame.REPEAT's dependencies are already resolved
        if(!target_only){
        if (isLookaheadConfigured && m_laFrame[0]->m_ttLookahead[0].numUpstreamDependencies.fetch_sub(1) == 1)
            m_pendingTasks.push_back(&m_laFrame[0]->m_ttLookahead[0]); // LA_START is independent

        if (inputParam->m_ttComplete.numUpstreamDependencies.fetch_sub(1) == 1)
            m_pendingTasks.push_front(&inputParam->m_ttComplete); // COMPLETE's dependencies are already resolved
        }
    }
}

static void SetTaskFinishedThreaded(ThreadingTask *task) {
    uint32_t expected = 0;
    while (!task->finished.compare_exchange_strong(expected, 1)) {
        expected = 0;
        // extremely rare case, use spinlock
        do {
            std::this_thread::yield();
        } while(task->finished.load());
    }
}


mfxStatus AV1Encoder::TaskRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)
{
    ThreadingTask *task = NULL;
    H265ENC_UNREFERENCED_PARAMETER(threadNumber);
    H265ENC_UNREFERENCED_PARAMETER(callNumber);

    if (pState == NULL || pParam == NULL) {
        return MFX_ERR_NULL_PTR;
    }

    AV1Encoder *th = static_cast<AV1Encoder *>(pState);
    AV1EncodeTaskInputParams *inputParam = (AV1EncodeTaskInputParams*)pParam;
    mfxStatus sts = MFX_ERR_NONE;

    // this operation doesn't affect m_condVar condition, so don't use mutex m_critSect
    // THREADING_ITASK_INI -> THREADING_ITASK_WORKING
    uint32_t taskStage = THREADING_ITASK_INI;
    inputParam->m_taskStage.compare_exchange_strong(taskStage, THREADING_ITASK_WORKING);

    if (THREADING_ITASK_INI == taskStage) {
        std::unique_lock<std::mutex> guard(th->m_prepCritSect);
        if (inputParam->m_taskID != th->m_taskEncodeCount) {
            // THREADING_ITASK_WORKING -> THREADING_ITASK_INI
            taskStage = THREADING_ITASK_WORKING;
            while (!inputParam->m_taskStage.compare_exchange_strong(taskStage, THREADING_ITASK_INI)) { taskStage = THREADING_ITASK_WORKING; };
            return MFX_TASK_BUSY;
        }
#if TASK_LOG_ENABLE
        inputParam->task_log_idx = TaskLogStart(NULL, inputParam->m_taskID);
#endif
        th->m_taskEncodeCount++;

        th->m_inputTasks.push_back(inputParam);

        if (!th->m_inputTaskInProgress) {
            th->m_inputTaskInProgress = th->m_inputTasks.front();
            th->PrepareToEncodeNewTask(th->m_inputTaskInProgress);
            th->PrepareToEncode(th->m_inputTaskInProgress);
            th->m_inputTasks.pop_front();
        }

        th->m_critSect.lock();

        size_t numPendingTasks = th->m_pendingTasks.size();

        th->m_critSect.unlock();
        guard.unlock();

        while (numPendingTasks--)
            th->m_condVar.notify_one();
    }

    if (inputParam->m_taskID > th->m_taskEncodeCount)
        return MFX_TASK_BUSY;

    // global thread count control
    uint32_t newThreadCount = th->m_threadCount.fetch_add(1)+1;
    OnExitHelperRoutine onExitHelper(th->m_threadCount);
    if (newThreadCount > th->m_videoParam.num_threads)
        return MFX_TASK_BUSY;

#if TT_TRACE
    char mainname[256] = "TaskRoutine ";
    if (inputParam->m_targetFrame[0])
        strcat(mainname, NUMBERS[inputParam->m_targetFrame[0]->m_frameOrder & 0x7f]);
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, mainname);
#endif // TT_TRACE

    while (true) {
        std::unique_lock<std::mutex> guard(th->m_critSect);
        while (true) {
            if (th->m_doStage == THREADING_WORKING && (th->m_pendingTasks.size()))
                break;
            // possible m_taskStage values here: THREADING_ITASK_INI (!), THREADING_ITASK_WORKING, THREADING_ITASK_COMPLETE
            if ((th->m_doStage != THREADING_PAUSE && inputParam->m_taskStage.load() == THREADING_ITASK_COMPLETE) ||
                th->m_doStage == THREADING_ERROR)
                break;
            th->m_condVar.wait(guard);
        }

        if (inputParam->m_taskStage.load() == THREADING_ITASK_COMPLETE || th->m_doStage == THREADING_ERROR) {
            break;
        }

        task = NULL;
        {
            std::deque<ThreadingTask *>::iterator t = th->m_pendingTasks.begin();
            if (th->m_videoParam.enableCmFlag) {
                ThreadingTask * tt = *t;
                if ((tt)->action == TT_ENCODE_CTU || (tt)->action == TT_DEBLOCK_AND_CDEF || (tt)->action == TT_CDEF_SYNC || (tt)->action == TT_CDEF_APPLY) {
                    Frame *targetFrame = th->m_outputQueue.front();
                    for (std::deque<ThreadingTask *>::iterator i = ++th->m_pendingTasks.begin(); i != th->m_pendingTasks.end(); ++i) {
                        ThreadingTask * ii = *i;
                        if ((ii)->action == TT_ENCODE_CTU || (ii)->action == TT_DEBLOCK_AND_CDEF || (ii)->action == TT_CDEF_SYNC || (ii)->action == TT_CDEF_APPLY) {
                            if ((tt)->frame != targetFrame && (ii)->frame == targetFrame ||
                                (ii)->frame->m_pyramidLayer < (tt)->frame->m_pyramidLayer ||
                                (ii)->frame->m_pyramidLayer == (tt)->frame->m_pyramidLayer && (ii)->frame->m_encOrder < (tt)->frame->m_encOrder) {
                                t = i;
                                tt = *i;
                            }
                        }
                    }
                }
            }
            task = *t;
            th->m_pendingTasks.erase(t);
        }
#if TASK_LOG_ENABLE
        int num_pend = th->m_pendingTasks.size();
#endif

        th->m_threadingTaskRunning++;
        guard.unlock();

        int32_t taskCount = 0;
        ThreadingTask *taskDepAll[MAX_NUM_DEPENDENCIES];

        int32_t distBest;

#ifdef VT_TRACE
        MFXTraceTask *tracetask = new MFXTraceTask(&_trace_static_handle2[MAX(0,task->poc) & 0x3f][task->action],
            __FILE__, __LINE__, __FUNCTION__, MFX_TRACE_CATEGORY, MFX_TRACE_LEVEL_API, "", false);

//        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, _trace_task_names[task->poc & 0x3f][task->action]);
#endif
#if TASK_LOG_ENABLE
        uint32_t idx = TaskLogStart(task, num_pend);
#endif
#if TT_TRACE
        char ttname[256] = "";
        strcat(ttname, TT_NAMES[task->action]);
        strcat(ttname, " ");
        strcat(ttname, NUMBERS[task->poc & 0x7f]);
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, ttname);
#endif // TT_TRACE

        sts = MFX_TASK_DONE;
        try {
            switch (task->action) {
            case TT_INIT_NEW_FRAME:
                th->InitNewFrame(task->frame, task->indata);
                break;
            case TT_PREENC_START:
                task->la->Execute(*task);
                if (th->m_videoParam.ZeroDelay == 1 && th->m_videoParam.QuickStartPFrame && th->m_laFrame[0]->m_picCodeType == MFX_FRAMETYPE_P) {
                    for (int32_t f = 0; f < (th->m_videoParam.picStruct == PROGR ? 1 : 2); f++) {
                        if (th->m_laFrame[f])
                            th->m_laFrame[f]->setWasLAProcessed();
                    }
                    th->OnLookaheadCompletion();
                    if (th->m_inputTaskInProgress) {
                        // start encoding
                        th->ReorderInput(th->m_inputTaskInProgress->surface == NULL);
                        th->PrepareToEncode(th->m_inputTaskInProgress, true);
                        th->m_condVar.notify_all();
                    }
                }
                break;
            case TT_PREENC_ROUTINE:
                task->la->Execute(*task);
                break;
            case TT_PREENC_END:
                task->la->Execute(*task);
                if (th->m_videoParam.ZeroDelay == 1 && (th->m_laFrame[0]->m_picCodeType == MFX_FRAMETYPE_I || !th->m_videoParam.QuickStartPFrame)) {
                    for (int32_t f = 0; f < (th->m_videoParam.picStruct == PROGR ? 1 : 2); f++) {
                        if (th->m_laFrame[f])
                            th->m_laFrame[f]->setWasLAProcessed();
                    }
                    th->OnLookaheadCompletion();
                    if (th->m_inputTaskInProgress) {
                        // start encoding
                        th->ReorderInput(th->m_inputTaskInProgress->surface == NULL);
                        th->PrepareToEncode(th->m_inputTaskInProgress, true);
                        th->m_condVar.notify_all();
                    }
                }
                break;
            case TT_DETECT_SCROLL_START:
                DetectScrollStart(*task);
                break;
            case TT_DETECT_SCROLL_ROUTINE:
                DetectScrollRoutine(*task);
                break;
            case TT_DETECT_SCROLL_END:
                DetectScrollEnd(*task);
                break;
            case TT_HUB:
                break;
            case TT_ENCODE_CTU:
                assert(task->frame->m_fenc != NULL);
                assert(task->frame->m_frameOrder == task->poc);
                sts =
#if ENABLE_10BIT
                    th->m_videoParam.bitDepthLuma > 8 ?
                    task->frame->m_fenc->PerformThreadingTask<uint16_t>(task->row, task->col):
#endif
                    task->frame->m_fenc->PerformThreadingTask<uint8_t>(task->row, task->col);
                assert(sts == MFX_TASK_DONE);
                break;
            case TT_DEBLOCK_AND_CDEF:
                {
#if !USE_CMODEL_PAK
                    if (th->m_videoParam.deblockingFlag && !task->frame->m_allowIntraBc) {
#ifdef PARALLEL_LOW_DELAY_FAST
                        if (task->frame->m_isRef || th->m_videoParam.doDumpRecon)
#endif
                        {
                            if (th->m_videoParam.src10Enable)
                                LoopFilterSbAV1<uint16_t>(task->frame, task->row, task->col, th->m_videoParam);
                            else
                                LoopFilterSbAV1<uint8_t>(task->frame, task->row, task->col, th->m_videoParam);
                        }
                        CdefStoreBorderSb(th->m_videoParam, task->frame, task->row, task->col);
                    }
                    if (th->m_videoParam.cdefFlag && !task->frame->m_allowIntraBc) {
                        if (task->row > 0 && task->col > 0) {
                            CdefOnePassSb(th->m_videoParam, task->frame, task->row - 1, task->col - 1);
                            if (task->col + 1 == th->m_videoParam.sb64Cols) {
                                CdefOnePassSb(th->m_videoParam, task->frame, task->row - 1, task->col);
                            }
                            if (task->row + 1 == th->m_videoParam.sb64Rows) {
                                CdefOnePassSb(th->m_videoParam, task->frame, task->row, task->col - 1);

                                if (task->col + 1 == th->m_videoParam.sb64Cols) {
                                    CdefOnePassSb(th->m_videoParam, task->frame, task->row, task->col);
                                }
                            }
                        }
                    }

                    if (th->m_videoParam.superResFlag && (task->frame->m_isRef || th->m_videoParam.doDumpRecon)) {
                        if (task->row + 1 == th->m_videoParam.sb64Rows && task->col + 1 == th->m_videoParam.sb64Cols) {
                            av1_upscale_normative_and_extend_frame(th->m_videoParam, task->frame->m_recon, task->frame->m_reconUpscale);
                            PadRectLumaAndChroma(*task->frame->m_reconUpscale, th->m_videoParam.fourcc, 0, 0, th->m_videoParam.sourceWidth, th->m_videoParam.sourceHeight);
                        }
                    }

                    if (!task->frame->IsNotRef()) {
                        if (task->row + 1 == th->m_videoParam.sb64Rows && task->col + 1 == th->m_videoParam.sb64Cols) {
                            PadRectLumaAndChroma(*task->frame->m_recon, th->m_videoParam.fourcc, 0, 0, th->m_videoParam.sourceWidth, th->m_videoParam.sourceHeight);
                        }
                        if (th->m_videoParam.src10Enable) {
                            if (task->row + 1 == th->m_videoParam.sb64Rows && task->col + 1 == th->m_videoParam.sb64Cols) {
                                PadRectLumaAndChroma(*task->frame->m_recon10, /*th->m_videoParam.fourcc*/MFX_FOURCC_P010, 0, 0, th->m_videoParam.sourceWidth, th->m_videoParam.sourceHeight);
                            }
                        }
                    }
#endif // CMODEL
                }
                break;
            case TT_PACK_TILE:
                assert(task->frame->m_fenc != NULL);
                task->frame->m_fenc->PackTile(task->tile);
                break;
            case TT_PACK_ROW:
                assert(task->frame->m_fenc != NULL);
                task->frame->m_fenc->PackRow(*task);
                break;
            case TT_ENC_COMPLETE:
                if (task->frame->showExistingFrame == 0) {
                    th->m_prepCritSect.lock();
                    assert(th->m_inputTaskInProgress);
                    sts = th->SyncOnFrameCompletion(th->m_inputTaskInProgress, task->frame);
                    th->m_prepCritSect.unlock();
                    if (sts != MFX_TASK_DONE) {
                        th->m_threadingTaskRunning--;
                        continue;
                    }
                }
                break;
            case TT_REPEAT_FRAME:
                assert(th->m_inputTaskInProgress);
                assert(th->m_inputTaskInProgress->bs);
                assert(task->frame);
                assert(task->poc == task->frame->m_frameOrder);
                WriteRepeatedFrame(th->m_videoParam, *task->frame, *th->m_inputTaskInProgress->bs);
                break;
            case TT_COMPLETE:
                {
                    // TT_COMPLETE shouldn't have dependent tasks
                    assert(task->numDownstreamDependencies == 0);
                    th->m_prepCritSect.lock();

                    AV1EncodeTaskInputParams *taskInProgress = th->m_inputTaskInProgress;
                    if (taskInProgress->bs && taskInProgress->m_targetFrame[0] && taskInProgress->m_targetFrame[0]->showExistingFrame == 0) {
                        for (int32_t i = 0; i < (int32_t)taskInProgress->m_targetFrame[0]->m_hiddenFrames.size(); i++) {
                            Frame* coded = taskInProgress->m_targetFrame[0]->m_hiddenFrames[i];
                            AV1FrameEncoder* frameEnc = coded->m_fenc;
                            coded->m_fenc = NULL;
                            th->OnEncodingQueried(coded); // remove coded frame from encodeQueue (outputQueue) and clean (release) dpb.
                            frameEnc->SetFree(true);
                        }
                        Frame* coded = taskInProgress->m_targetFrame[0];
                        AV1FrameEncoder* frameEnc = coded->m_fenc;
                        coded->m_fenc = NULL;
                        th->OnEncodingQueried(coded); // remove coded frame from encodeQueue (outputQueue) and clean (release) dpb.
                        frameEnc->SetFree(true);
                    }
                    if (!th->m_videoParam.ZeroDelay) {
                        for (int32_t f = 0; f < (th->m_videoParam.picStruct == PROGR ? 1 : 2); f++) {
                            if (th->m_laFrame[f])
                                th->m_laFrame[f]->setWasLAProcessed();
                        }
                        th->OnLookaheadCompletion();
                    }

                    if (th->m_inputTasks.size()) {
                        th->m_inputTaskInProgress = th->m_inputTasks.front();
                        th->PrepareToEncodeNewTask(th->m_inputTaskInProgress);
                        th->PrepareToEncode(th->m_inputTaskInProgress);
                        th->m_inputTasks.pop_front();
                    } else {
                        th->m_inputTaskInProgress = NULL;
                    }

                    th->m_prepCritSect.unlock();
                    // THREADING_ITASK_WORKING -> THREADING_ITASK_COMPLETE
                    guard.lock();
                    uint32_t eTaskStage = THREADING_ITASK_WORKING;
                    while (!inputParam->m_taskStage.compare_exchange_strong(eTaskStage, THREADING_ITASK_COMPLETE)) { eTaskStage = THREADING_ITASK_WORKING; };
                    th->m_condVar.notify_all();
                    guard.unlock();

                    th->m_threadingTaskRunning--;
                    continue;
                }
                break;
            default:
                assert(0);
                break;
            }
        } catch (...) {
            // to prevent SyncOnFrameCompletion hang
            th->m_threadingTaskRunning--;
            guard.lock();
            th->m_doStage = THREADING_ERROR;
            guard.unlock();
            inputParam->m_taskStage.exchange(THREADING_ITASK_COMPLETE);
            th->m_condVar.notify_all();
            throw;
        }
#if TASK_LOG_ENABLE
        TaskLogStop(idx);
#endif
#if TT_TRACE
        MFX_AUTO_TRACE_STOP();
#endif // TT_TRACE

#ifdef VT_TRACE
        if (tracetask) delete tracetask;
#endif

        //SetTaskFinishedThreaded(task);

        distBest = -1;

        for (int i = 0; i < task->numDownstreamDependencies; i++) {
            ThreadingTask *taskDep = task->downstreamDependencies[i];
            if (taskDep->numUpstreamDependencies.fetch_sub(1) == 1) {
                if (taskDep->action == TT_GPU_SUBMIT) {
                    th->m_feiCritSect.lock();
                    th->m_feiSubmitTasks.push_back(taskDep);
                    th->m_feiCritSect.unlock();
                    th->m_feiCondVar.notify_one();
                } else {
                    taskDepAll[taskCount++] = taskDep;
                }
            }
        }

        SetTaskFinishedThreaded(task);

        if (taskCount) {
            int c;
            guard.lock();
            for (c = 0; c < taskCount; c++) {
                th->m_pendingTasks.push_back(taskDepAll[c]);
            }
            guard.unlock();

            for (c = 0; c < taskCount; c++)
                th->m_condVar.notify_one();
        }

        th->m_threadingTaskRunning--;
    }

    return MFX_TASK_DONE;
}


mfxStatus AV1Encoder::TaskCompleteProc(void *pState, void *pParam, mfxStatus taskRes)
{
    H265ENC_UNREFERENCED_PARAMETER(taskRes);
    MFX_CHECK_NULL_PTR1(pState);
    MFX_CHECK_NULL_PTR1(pParam);

#if TASK_LOG_ENABLE
    AV1EncodeTaskInputParams *inputParam = static_cast<AV1EncodeTaskInputParams*>(pParam);
    TaskLogStop(inputParam->task_log_idx);
#endif

    AV1_Free(pParam);

    return MFX_TASK_DONE;
}

inline __m128i set1(uint8_t  v) { return _mm_set1_epi8(v); }
inline __m128i set1(uint16_t v) { return _mm_set1_epi16(v); }
inline __m128i set1(uint32_t v) { return _mm_set1_epi32(v); }

inline void FillVer(uint8_t *p, int32_t pitchInBytes, int32_t h, __m128i line)
{
    for (int32_t y = 0; y < h; y++, p += pitchInBytes)
        _mm_store_si128((__m128i*)p, line);
}

inline void FillVer32(uint8_t *p, int32_t pitchInBytes, int32_t h, __m256i line)
{
    for (int32_t y = 0; y < h; y++, p += pitchInBytes)
        _mm256_store_si256((__m256i*)p, line);
}

inline void FillVer8(uint8_t *p, int32_t pitchInBytes, int32_t h, uint64_t line)
{
    for (int32_t y = 0; y < h; y++, p += pitchInBytes)
        *(uint64_t*)p = line;
}

template <class T> inline void FillHor(T *p, int32_t w, T val)
{
    uint8_t *p8 = (uint8_t *)p;
    w *= sizeof(T);
    __m128i value = set1(val);
    for (int32_t x = 0; x < w; x += 16, p8 += 16)
        _mm_store_si128((__m128i*)p8, value);
}

template <class T> inline void FillHor32(T *p, int32_t w, T val)
{
    uint8_t *p8 = (uint8_t *)p;
    w *= sizeof(T);
    assert(sizeof(T) == 1);
    __m256i value = _mm256_set1_epi8((int8_t)val);
    for (int32_t x = 0; x < w; x += 32, p8 += 32)
        _mm256_store_si256((__m256i*)p8, value);
}

template <class T> inline void FillHor8(T *p, int32_t w, T val)
{
    uint8_t *p8 = (uint8_t *)p;
    w *= sizeof(T);
    __m128i value = set1(val);
    _mm_storel_epi64((__m128i*)p8, value);
    p8 += 8;
    for (int32_t x = 8; x < w; x += 16, p8 += 16)
        _mm_store_si128((__m128i*)p8, value);
}

template <class T> inline void FillCorner(T *p, int32_t pitchInBytes, int32_t w, int32_t h, T val)
{
    uint8_t *p8 = (uint8_t *)p;
    w *= sizeof(T);
    __m128i value = set1(val);
    for (int32_t y = 0; y < h; y++, p8 += pitchInBytes)
        for (int32_t x = 0; x < w; x += 16)
            _mm_store_si128((__m128i*)(p8+x), value);
}

template <class T> inline void FillCorner32(T *p, int32_t pitchInBytes, int32_t w, int32_t h, T val)
{
    uint8_t *p8 = (uint8_t *)p;
    w *= sizeof(T);
    assert(sizeof(T) == 1);
    __m256i value = _mm256_set1_epi8((int8_t)val);
    for (int32_t y = 0; y < h; y++, p8 += pitchInBytes)
        for (int32_t x = 0; x < w; x += 32)
            _mm256_store_si256((__m256i*)(p8 + x), value);
}

template <class T> inline void FillCorner8(T *p, int32_t pitchInBytes, int32_t w, int32_t h, T val)
{
    uint8_t *p8 = (uint8_t *)p;
    w *= sizeof(T);
    __m128i value = set1(val);
    for (int32_t y = 0; y < h; y++, p8 += pitchInBytes) {
        _mm_storel_epi64((__m128i*)p8, value);
        for (int32_t x = 8; x < w; x += 16)
            _mm_store_si128((__m128i*)(p8+x), value);
    }
}

template <class PixType>
void CopyPlane(const PixType *src, int32_t pitchSrc, PixType *dst, int32_t pitchDst,
               int32_t width, int32_t height)
{
    assert((size_t(dst) & 63) == 0);
    assert((size_t(src) & 15) == 0);
    assert((pitchDst*sizeof(PixType) & 63) == 0);
    assert((pitchSrc*sizeof(PixType) & 15) == 0);
    assert((width*sizeof(PixType) & 7) == 0);

    int32_t pitchSrcB = pitchSrc * sizeof(PixType);
    int32_t pitchDstB = pitchDst * sizeof(PixType);
    int32_t widthB = width * sizeof(PixType);
    uint8_t *dstB = (uint8_t *)dst;
    uint8_t *srcB = (uint8_t *)src;

    if (widthB & 7) {
        for (int32_t y = 0; y < height; y++, srcB += pitchSrcB, dstB += pitchDstB) {
            int32_t x = 0;
            for (; x < (widthB & ~7); x += 16)
                _mm_store_si128((__m128i *)(dstB+x), _mm_load_si128((__m128i *)(srcB+x)));
            *(uint64_t*)(dstB+x) = *(uint64_t*)(srcB+x);
        }
    } else {
        for (int32_t y = 0; y < height; y++, srcB += pitchSrcB, dstB += pitchDstB)
            for (int32_t x = 0; x < widthB; x += 16)
                _mm_store_si128((__m128i *)(dstB+x), _mm_load_si128((__m128i *)(srcB+x)));
    }
}

template <int widthInBytesMul, class PixType> void CopyAndPadPlane(
    const PixType *src, int32_t pitchSrc, PixType *dst, int32_t pitchDst,
    int32_t width, int32_t height, int32_t padL, int32_t padR, int32_t padV)
{
    assert(widthInBytesMul == 16 || widthInBytesMul == 8 || widthInBytesMul == 32);
    assert((size_t(dst) & 63) == 0);
    assert((size_t(src) & 15) == 0);
    assert((pitchDst*sizeof(PixType) & 63) == 0);
    assert((pitchSrc*sizeof(PixType) & 15) == 0);
    assert((width*sizeof(PixType) & 7) == 0);
    assert((padL*sizeof(PixType) & 15) == 0);
    assert((padR*sizeof(PixType) & 7) == 0);
    assert((padR*sizeof(PixType) & 7) == (width*sizeof(PixType) & 7));

    int32_t widthB = width * sizeof(PixType);
    int32_t pitchSrcB = pitchSrc * sizeof(PixType);
    int32_t pitchDstB = pitchDst * sizeof(PixType);

    uint8_t *dstB = (uint8_t *)dst;
    uint8_t *srcB = (uint8_t *)src;
    // top left corner
    if (widthInBytesMul == 32) {
        FillCorner32(dst - padL - pitchDst * padV, pitchDstB, padL, padV + 1, src[0]);
        // top border
        for (int32_t x = 0; x < widthB; x += 32)
            FillVer32(dstB + x - pitchDstB * padV, pitchDstB, padV + 1, _mm256_load_si256((__m256i*)(srcB+x)));
        // top right corner
        FillCorner32(dst + width - pitchDst * padV, pitchDstB, padR, padV + 1, src[width - 1]);
    }
    else if (widthInBytesMul == 16) {
        FillCorner(dst - padL - pitchDst * padV, pitchDstB, padL, padV + 1, src[0]);
        // top border
        for (int32_t x = 0; x < widthB; x += 16)
            FillVer(dstB + x - pitchDstB * padV, pitchDstB, padV + 1, _mm_load_si128((__m128i*)(srcB + x)));
        // top right corner
        FillCorner(dst + width - pitchDst * padV, pitchDstB, padR, padV + 1, src[width - 1]);
    }else{
        FillCorner8(dst - padL - pitchDst * padV, pitchDstB, padL, padV + 1, src[0]);
        // top border
        int32_t x = 0;
        for (; x < (widthB & ~15); x += 16)
            FillVer(dstB + x - pitchDstB * padV, pitchDstB, padV + 1, _mm_load_si128((__m128i*)(srcB+x)));
        FillVer8(dstB + x - pitchDstB * padV, pitchDstB, padV + 1, *(uint64_t*)(srcB+x));
        // top right corner
        FillCorner8(dst + width - pitchDst * padV, pitchDstB, padR, padV + 1, src[width - 1]);
    }

    srcB += pitchSrcB;
    dstB += pitchDstB;
    for (int32_t y = 1; y < height - 1; y++, srcB += pitchSrcB, dstB += pitchDstB) {
        // left border
        if (widthInBytesMul == 32) {
            FillHor32((PixType*)dstB - padL, padL, ((PixType*)srcB)[0]);
            // center part
            for (int x=0; x < (widthB & ~31); x += 32)
                 _mm256_storeu_si256((__m256i*)(dstB+x), _mm256_loadu_si256((__m256i*)(srcB+x)));
            // right border
            FillHor32((PixType*)dstB + width, padR, ((PixType*)srcB)[width-1]);
        }
        else if (widthInBytesMul == 16) {
            FillHor((PixType*)dstB - padL, padL, ((PixType*)srcB)[0]);
            // center part
            for (int32_t x = 0; x < widthB; x += 16)
                _mm_store_si128((__m128i*)(dstB + x), _mm_load_si128((__m128i*)(srcB + x)));
            // right border
            FillHor((PixType*)dstB + width, padR, ((PixType*)srcB)[width - 1]);
        }else{
            FillHor8((PixType*)dstB - padL, padL, ((PixType*)srcB)[0]);
            // center part
            int32_t x = 0;
            for (; x < (widthB & ~31); x += 32)
                _mm256_storeu_si256((__m256i*)(dstB + x), _mm256_loadu_si256((__m256i*)(srcB + x)));
            for (; x < (widthB & ~15); x += 16)
                _mm_store_si128((__m128i*)(dstB+x), _mm_load_si128((__m128i*)(srcB+x)));
            *(uint64_t*)(dstB+x) = *(uint64_t*)(srcB+x);
            // right border
            FillHor8((PixType*)dstB + width, padR, ((PixType*)srcB)[width-1]);
        }
    }


    // bottom left corner
    if (widthInBytesMul == 32) {
        FillCorner32((PixType*)dstB - padL, pitchDstB, padL, padV + 1, ((PixType*)srcB)[0]);
        // bottom border
        for (int32_t x = 0; x < widthB; x += 32)
            FillVer32(dstB + x, pitchDstB, padV + 1, _mm256_load_si256((__m256i*)(srcB + x)));
        // bottom right corner
        FillCorner32((PixType*)dstB + width, pitchDstB, padR, padV + 1, ((PixType*)srcB)[width-1]);
    }
    else if (widthInBytesMul == 16) {
        FillCorner((PixType*)dstB - padL, pitchDstB, padL, padV + 1, ((PixType*)srcB)[0]);
        // bottom border
        for (int32_t x = 0; x < widthB; x += 16)
            FillVer(dstB + x, pitchDstB, padV + 1, _mm_load_si128((__m128i*)(srcB + x)));
        // bottom right corner
        FillCorner((PixType*)dstB + width, pitchDstB, padR, padV + 1, ((PixType*)srcB)[width - 1]);
    }else{
        FillCorner8((PixType*)dstB - padL, pitchDstB, padL, padV + 1, ((PixType*)srcB)[0]);
        // bottom border
        int32_t x = 0;
        for (; x < (widthB & ~15); x += 16)
            FillVer(dstB + x, pitchDstB, padV + 1, _mm_load_si128((__m128i*)(srcB + x)));
        FillVer8(dstB + x, pitchDstB, padV + 1, *(uint64_t*)(srcB + x));
        // bottom right corner
        FillCorner8((PixType*)dstB + width, pitchDstB, padR, padV + 1, ((PixType*)srcB)[width-1]);
    }
}

template <class PixType> void AV1Enc::CopyAndPad(const mfxFrameSurface1 &src, FrameData &dst, uint32_t fourcc)
{
    // work-around for 8x and 16x downsampling on gpu
    // currently DS kernel expect right border is padded up to pitch
    int32_t paddingL = dst.padding;
    int32_t paddingR = dst.padding;
    int32_t paddingH = dst.padding;
    int32_t bppShift = sizeof(PixType) == 1 ? 0 : 1;
    if (dst.m_handle) {
        paddingL = mfx::align2_value<int32_t>(dst.padding << bppShift, 64) >> bppShift;
        paddingR = dst.pitch_luma_pix - dst.width - paddingL;
    }

    int32_t heightC = dst.height;
    if (fourcc == MFX_FOURCC_NV12 || fourcc == MFX_FOURCC_P010)
        heightC >>= 1;

    int32_t srcPitch = src.Data.Pitch;
    if (sizeof(PixType) == 1) {
        ((dst.width & 15) ? CopyAndPadPlane<8,uint8_t> : (dst.width & 31) ? CopyAndPadPlane<16,uint8_t> : CopyAndPadPlane<32, uint8_t>)(
                (uint8_t*)src.Data.Y, srcPitch, (uint8_t*)dst.y, dst.pitch_luma_pix,
                dst.width, dst.height, paddingL, paddingR, paddingH);
        CopyPlane((uint16_t*)src.Data.UV, srcPitch>>1, (uint16_t*)dst.uv, dst.pitch_chroma_pix>>1,
                    dst.width>>1, heightC);
    } else {
        srcPitch >>= 1;
        CopyAndPadPlane<16>((uint16_t*)src.Data.Y, srcPitch, (uint16_t*)dst.y, dst.pitch_luma_pix,
                            dst.width, dst.height, paddingL, paddingR, paddingH);
        CopyPlane((uint32_t*)src.Data.UV, srcPitch>>1, (uint32_t*)dst.uv, dst.pitch_chroma_pix>>1,
                    dst.width>>1, heightC);
    }
}
template void AV1Enc::CopyAndPad<uint8_t>(const mfxFrameSurface1 &src, FrameData &dst, uint32_t fourcc);
#if ENABLE_10BIT
template void AV1Enc::CopyAndPad<uint16_t>(const mfxFrameSurface1 &src, FrameData &dst, uint32_t fourcc);
#endif

static void convert16bTo8b_shift(Frame* frame)
{
    const uint16_t* src10Y = (uint16_t*)(frame->m_origin10->y);

    for (int32_t y = 0; y < frame->m_par->Height; y++) {
        for (int32_t x = 0; x < frame->m_par->Width; x++) {
            frame->m_origin->y[y * frame->m_origin->pitch_luma_pix + x] = uint8_t((src10Y[y * frame->m_origin10->pitch_luma_pix + x] + 2) >> 2);
        }
    }

    const uint16_t* src10Uv = (uint16_t*)(frame->m_origin10->uv);
    for (int32_t y = 0; y < frame->m_par->Height / 2; y++) {
        for (int32_t x = 0; x < frame->m_par->Width / 2; x++) {
            frame->m_origin->uv[y * frame->m_origin->pitch_chroma_pix + 2 * x] = uint8_t((src10Uv[y * frame->m_origin10->pitch_chroma_pix + 2 * x] + 2) >> 2);
            //frame->m_recon->uv[y * frame->m_recon->pitch_chroma_pix + 2 * x + 1] = frame->av1frameRecon.data_v16b[y * frame->av1frameRecon.pitch + x];
            frame->m_origin->uv[y * frame->m_origin->pitch_chroma_pix + 2 * x + 1] = uint8_t((src10Uv[y * frame->m_origin10->pitch_chroma_pix + 2 * x + 1] + 2) >> 2);
        }
    }
}




#if 0
static int get_down2_length(int length, int steps)
{
    for (int s = 0; s < steps; ++s) length = (length + 1) >> 1;
    return length;
}

static int get_down2_steps(int in_length, int out_length) {
    int steps = 0;
    int proj_in_length;
    while ((proj_in_length = get_down2_length(in_length, 1)) >= out_length) {
        ++steps;
        in_length = proj_in_length;
        if (in_length == 1) {
            // Special case: we break because any further calls to get_down2_length()
            // with be with length == 1, which return 1, resulting in an infinite
            // loop.
            break;
        }
    }
    return steps;
}

// Filters for factor of 2 downsampling.
static const int16_t av1_down2_symeven_half_filter[] = { 56, 12, -3, -1 };
static const int16_t av1_down2_symodd_half_filter[] = { 64, 35, 0, -3 };

#define FILTER_BITS 7
#define RS_SUBPEL_MASK ((1 << RS_SUBPEL_BITS) - 1)

static inline uint8_t clip_pixel(int val) {
    return uint8_t((val > 255) ? 255 : (val < 0) ? 0 : val);
}

static void down2_symodd(const uint8_t *const input, int length,
    uint8_t *output) {
    // Actual filter len = 2 * filter_len_half - 1.
    const int16_t *filter = av1_down2_symodd_half_filter;
    const int filter_len_half = sizeof(av1_down2_symodd_half_filter) / 2;
    int i, j;
    uint8_t *optr = output;
    int l1 = filter_len_half - 1;
    int l2 = (length - filter_len_half + 1);
    l1 += (l1 & 1);
    l2 += (l2 & 1);
    if (l1 > l2) {
        // Short input length.
        for (i = 0; i < length; i += 2) {
            int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
            for (j = 1; j < filter_len_half; ++j) {
                sum += (input[(i - j < 0 ? 0 : i - j)] +
                    input[(i + j >= length ? length - 1 : i + j)]) *
                    filter[j];
            }
            sum >>= FILTER_BITS;
            *optr++ = clip_pixel(sum);
        }
    }
    else {
        // Initial part.
        for (i = 0; i < l1; i += 2) {
            int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
            for (j = 1; j < filter_len_half; ++j) {
                sum += (input[(i - j < 0 ? 0 : i - j)] + input[i + j]) * filter[j];
            }
            sum >>= FILTER_BITS;
            *optr++ = clip_pixel(sum);
        }
        // Middle part.
        for (; i < l2; i += 2) {
            int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
            for (j = 1; j < filter_len_half; ++j) {
                sum += (input[i - j] + input[i + j]) * filter[j];
            }
            sum >>= FILTER_BITS;
            *optr++ = clip_pixel(sum);
        }
        // End part.
        for (; i < length; i += 2) {
            int sum = (1 << (FILTER_BITS - 1)) + input[i] * filter[0];
            for (j = 1; j < filter_len_half; ++j) {
                sum += (input[i - j] + input[(i + j >= length ? length - 1 : i + j)]) *
                    filter[j];
            }
            sum >>= FILTER_BITS;
            *optr++ = clip_pixel(sum);
        }
    }
}

static void down2_symeven(const uint8_t *const input, int length,
    uint8_t *output) {
    // Actual filter len = 2 * filter_len_half.
    const int16_t *filter = av1_down2_symeven_half_filter;
    const int filter_len_half = sizeof(av1_down2_symeven_half_filter) / 2;
    int i, j;
    uint8_t *optr = output;
    int l1 = filter_len_half;
    int l2 = (length - filter_len_half);
    l1 += (l1 & 1);
    l2 += (l2 & 1);
    if (l1 > l2) {
        // Short input length.
        for (i = 0; i < length; i += 2) {
            int sum = (1 << (FILTER_BITS - 1));
            for (j = 0; j < filter_len_half; ++j) {
                sum +=
                    (input[std::max(i - j, 0)] + input[std::min(i + 1 + j, length - 1)]) *
                    filter[j];
            }
            sum >>= FILTER_BITS;
            *optr++ = clip_pixel(sum);
        }
    }
    else {
        // Initial part.
        for (i = 0; i < l1; i += 2) {
            int sum = (1 << (FILTER_BITS - 1));
            for (j = 0; j < filter_len_half; ++j) {
                sum += (input[std::max(i - j, 0)] + input[i + 1 + j]) * filter[j];
            }
            sum >>= FILTER_BITS;
            *optr++ = clip_pixel(sum);
        }
        // Middle part.
        for (; i < l2; i += 2) {
            int sum = (1 << (FILTER_BITS - 1));
            for (j = 0; j < filter_len_half; ++j) {
                sum += (input[i - j] + input[i + 1 + j]) * filter[j];
            }
            sum >>= FILTER_BITS;
            *optr++ = clip_pixel(sum);
        }
        // End part.
        for (; i < length; i += 2) {
            int sum = (1 << (FILTER_BITS - 1));
            for (j = 0; j < filter_len_half; ++j) {
                sum +=
                    (input[i - j] + input[std::min(i + 1 + j, length - 1)]) * filter[j];
            }
            sum >>= FILTER_BITS;
            *optr++ = clip_pixel(sum);
        }
    }
}

// Filters for interpolation (0.5-band) - note this also filters integer pels.
static const InterpKernel filteredinterp_filters500[(1 << RS_SUBPEL_BITS)] = {
  { -3, 0, 35, 64, 35, 0, -3, 0 },    { -3, 0, 34, 64, 36, 0, -3, 0 },
  { -3, -1, 34, 64, 36, 1, -3, 0 },   { -3, -1, 33, 64, 37, 1, -3, 0 },
  { -3, -1, 32, 64, 38, 1, -3, 0 },   { -3, -1, 31, 64, 39, 1, -3, 0 },
  { -3, -1, 31, 63, 39, 2, -3, 0 },   { -2, -2, 30, 63, 40, 2, -3, 0 },
  { -2, -2, 29, 63, 41, 2, -3, 0 },   { -2, -2, 29, 63, 41, 3, -4, 0 },
  { -2, -2, 28, 63, 42, 3, -4, 0 },   { -2, -2, 27, 63, 43, 3, -4, 0 },
  { -2, -3, 27, 63, 43, 4, -4, 0 },   { -2, -3, 26, 62, 44, 5, -4, 0 },
  { -2, -3, 25, 62, 45, 5, -4, 0 },   { -2, -3, 25, 62, 45, 5, -4, 0 },
  { -2, -3, 24, 62, 46, 5, -4, 0 },   { -2, -3, 23, 61, 47, 6, -4, 0 },
  { -2, -3, 23, 61, 47, 6, -4, 0 },   { -2, -3, 22, 61, 48, 7, -4, -1 },
  { -2, -3, 21, 60, 49, 7, -4, 0 },   { -1, -4, 20, 60, 49, 8, -4, 0 },
  { -1, -4, 20, 60, 50, 8, -4, -1 },  { -1, -4, 19, 59, 51, 9, -4, -1 },
  { -1, -4, 19, 59, 51, 9, -4, -1 },  { -1, -4, 18, 58, 52, 10, -4, -1 },
  { -1, -4, 17, 58, 52, 11, -4, -1 }, { -1, -4, 16, 58, 53, 11, -4, -1 },
  { -1, -4, 16, 57, 53, 12, -4, -1 }, { -1, -4, 15, 57, 54, 12, -4, -1 },
  { -1, -4, 15, 56, 54, 13, -4, -1 }, { -1, -4, 14, 56, 55, 13, -4, -1 },
  { -1, -4, 14, 55, 55, 14, -4, -1 }, { -1, -4, 13, 55, 56, 14, -4, -1 },
  { -1, -4, 13, 54, 56, 15, -4, -1 }, { -1, -4, 12, 54, 57, 15, -4, -1 },
  { -1, -4, 12, 53, 57, 16, -4, -1 }, { -1, -4, 11, 53, 58, 16, -4, -1 },
  { -1, -4, 11, 52, 58, 17, -4, -1 }, { -1, -4, 10, 52, 58, 18, -4, -1 },
  { -1, -4, 9, 51, 59, 19, -4, -1 },  { -1, -4, 9, 51, 59, 19, -4, -1 },
  { -1, -4, 8, 50, 60, 20, -4, -1 },  { 0, -4, 8, 49, 60, 20, -4, -1 },
  { 0, -4, 7, 49, 60, 21, -3, -2 },   { -1, -4, 7, 48, 61, 22, -3, -2 },
  { 0, -4, 6, 47, 61, 23, -3, -2 },   { 0, -4, 6, 47, 61, 23, -3, -2 },
  { 0, -4, 5, 46, 62, 24, -3, -2 },   { 0, -4, 5, 45, 62, 25, -3, -2 },
  { 0, -4, 5, 45, 62, 25, -3, -2 },   { 0, -4, 5, 44, 62, 26, -3, -2 },
  { 0, -4, 4, 43, 63, 27, -3, -2 },   { 0, -4, 3, 43, 63, 27, -2, -2 },
  { 0, -4, 3, 42, 63, 28, -2, -2 },   { 0, -4, 3, 41, 63, 29, -2, -2 },
  { 0, -3, 2, 41, 63, 29, -2, -2 },   { 0, -3, 2, 40, 63, 30, -2, -2 },
  { 0, -3, 2, 39, 63, 31, -1, -3 },   { 0, -3, 1, 39, 64, 31, -1, -3 },
  { 0, -3, 1, 38, 64, 32, -1, -3 },   { 0, -3, 1, 37, 64, 33, -1, -3 },
  { 0, -3, 1, 36, 64, 34, -1, -3 },   { 0, -3, 0, 36, 64, 34, 0, -3 },
};

static const InterpKernel *choose_interp_filter(int in_length, int out_length) {
    int out_length16 = out_length * 16;
    if (out_length16 >= in_length * 16) {
        assert(0); return nullptr;// return filteredinterp_filters1000;
    }
    else if (out_length16 >= in_length * 13) {
        assert(0); return nullptr;// return filteredinterp_filters875;
    }
    else if (out_length16 >= in_length * 11) {
        assert(0); return nullptr;// return filteredinterp_filters750;
    }
    else if (out_length16 >= in_length * 9) {
        assert(0); return nullptr;// return filteredinterp_filters625;
    }
    else
        return filteredinterp_filters500;
}

static void interpolate_core(const uint8_t *const input, int in_length,
    uint8_t *output, int out_length,
    const int16_t *interp_filters, int interp_taps) {
    const int32_t delta =
        (((uint32_t)in_length << RS_SCALE_SUBPEL_BITS) + out_length / 2) /
        out_length;
    const int32_t offset =
        in_length > out_length
        ? (((int32_t)(in_length - out_length) << (RS_SCALE_SUBPEL_BITS - 1)) +
            out_length / 2) /
        out_length
        : -(((int32_t)(out_length - in_length)
            << (RS_SCALE_SUBPEL_BITS - 1)) +
            out_length / 2) /
        out_length;
    uint8_t *optr = output;
    int x, x1, x2, sum, k, int_pel, sub_pel;
    int32_t y;

    x = 0;
    y = offset + RS_SCALE_EXTRA_OFF;
    while ((y >> RS_SCALE_SUBPEL_BITS) < (interp_taps / 2 - 1)) {
        x++;
        y += delta;
    }
    x1 = x;
    x = out_length - 1;
    y = delta * x + offset + RS_SCALE_EXTRA_OFF;
    while ((y >> RS_SCALE_SUBPEL_BITS) + (int32_t)(interp_taps / 2) >=
        in_length) {
        x--;
        y -= delta;
    }
    x2 = x;
    if (x1 > x2) {
        for (x = 0, y = offset + RS_SCALE_EXTRA_OFF; x < out_length;
            ++x, y += delta) {
            int_pel = y >> RS_SCALE_SUBPEL_BITS;
            sub_pel = (y >> RS_SCALE_EXTRA_BITS) & RS_SUBPEL_MASK;
            const int16_t *filter = &interp_filters[sub_pel * interp_taps];
            sum = 0;
            for (k = 0; k < interp_taps; ++k) {
                const int pk = int_pel - interp_taps / 2 + 1 + k;
                sum += filter[k] * input[std::max(std::min(pk, in_length - 1), 0)];
            }
            *optr++ = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
        }
    }
    else {
        // Initial part.
        for (x = 0, y = offset + RS_SCALE_EXTRA_OFF; x < x1; ++x, y += delta) {
            int_pel = y >> RS_SCALE_SUBPEL_BITS;
            sub_pel = (y >> RS_SCALE_EXTRA_BITS) & RS_SUBPEL_MASK;
            const int16_t *filter = &interp_filters[sub_pel * interp_taps];
            sum = 0;
            for (k = 0; k < interp_taps; ++k)
                sum += filter[k] * input[std::max(int_pel - interp_taps / 2 + 1 + k, 0)];
            *optr++ = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
        }
        // Middle part.
        for (; x <= x2; ++x, y += delta) {
            int_pel = y >> RS_SCALE_SUBPEL_BITS;
            sub_pel = (y >> RS_SCALE_EXTRA_BITS) & RS_SUBPEL_MASK;
            const int16_t *filter = &interp_filters[sub_pel * interp_taps];
            sum = 0;
            for (k = 0; k < interp_taps; ++k)
                sum += filter[k] * input[int_pel - interp_taps / 2 + 1 + k];
            *optr++ = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
        }
        // End part.
        for (; x < out_length; ++x, y += delta) {
            int_pel = y >> RS_SCALE_SUBPEL_BITS;
            sub_pel = (y >> RS_SCALE_EXTRA_BITS) & RS_SUBPEL_MASK;
            const int16_t *filter = &interp_filters[sub_pel * interp_taps];
            sum = 0;
            for (k = 0; k < interp_taps; ++k)
                sum += filter[k] *
                input[std::min(int_pel - interp_taps / 2 + 1 + k, in_length - 1)];
            *optr++ = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
        }
    }
}

static void interpolate(const uint8_t *const input, int in_length,
    uint8_t *output, int out_length)
{
    const InterpKernel *interp_filters =
        choose_interp_filter(in_length, out_length);

    interpolate_core(input, in_length, output, out_length, &interp_filters[0][0],
        SUBPEL_TAPS);
}

static void resize_multistep(const uint8_t *const input, int length,
    uint8_t *output, int olength, uint8_t *otmp) {
    if (length == olength) {
        memcpy(output, input, sizeof(output[0]) * length);
        return;
    }
    const int steps = get_down2_steps(length, olength);

    if (steps > 0) {
        uint8_t *out = NULL;
        int filteredlength = length;

        //assert(otmp != NULL);
        uint8_t *otmp2 = otmp + get_down2_length(length, 1);
        for (int s = 0; s < steps; ++s) {
            const int proj_filteredlength = get_down2_length(filteredlength, 1);
            const uint8_t *const in = (s == 0 ? input : out);
            if (s == steps - 1 && proj_filteredlength == olength)
                out = output;
            else
                out = (s & 1 ? otmp2 : otmp);
            if (filteredlength & 1)
                down2_symodd(in, filteredlength, out);
            else
                down2_symeven(in, filteredlength, out);
            filteredlength = proj_filteredlength;
        }
        if (filteredlength != olength) {
            interpolate(out, filteredlength, output, olength);
        }
    }
    else {
        interpolate(input, length, output, olength);
    }
}
#endif 0

void DownScale(const mfxFrameSurface1 &src, FrameData &dst)
{
    const uint32_t pitchSrc = src.Data.Pitch;
    const uint32_t pitchDst = dst.pitch_luma_pix;
    const uint32_t widthSrc = src.Info.Width;
    const uint32_t widthDst = dst.width;
    assert(widthDst * 2 == widthSrc);

#if 0
    for (int i = 0; i < dst.height; ++i)
        resize_multistep(src.Data.Y + pitchSrc * i, widthSrc, dst.y + pitchDst * i, widthDst, nullptr);
#else // 0
    for (int y = 0; y < dst.height; y++) {
        for (int x = 0; x < dst.width; x++) {
            int sum = src.Data.Y[src.Data.Pitch * y + 2 * x] + src.Data.Y[src.Data.Pitch * y + 2 * x + 1];
            dst.y[y * dst.pitch_luma_pix + x] = uint8_t(sum >> 1);
        }
    }
#endif // 0

    for (int y = 0; y < dst.height/2; y++) {
        for (int x = 0; x < dst.width/2; x++) {
            int sum = src.Data.UV[src.Data.Pitch * y + 4 * x] + src.Data.UV[src.Data.Pitch * y + 4 * x + 2];
            dst.uv[y * dst.pitch_chroma_pix + 2 * x] = uint8_t(sum >> 1);

            sum = src.Data.UV[src.Data.Pitch * y + 4 * x + 1] + src.Data.UV[src.Data.Pitch * y + 4 * x + 3];
            dst.uv[y * dst.pitch_chroma_pix + 2 * x + 1] = uint8_t(sum >> 1);
        }
    }
}

void AV1Encoder::InitNewFrame(Frame *out, mfxFrameSurface1 *inExternal)
{
    mfxFrameAllocator &fa = m_core.FrameAllocator();
    mfxFrameSurface1 in = *inExternal;
    mfxStatus st = MFX_ERR_NONE;

    if (out->m_picStruct != PROGR) {
        if (out->m_bottomFieldFlag) {
            in.Data.Y += in.Data.Pitch;
            in.Data.UV += in.Data.Pitch;
        }
        in.Data.Pitch *= 2;
    }

    // attach original surface to frame
    out->m_origin = m_inputFrameDataPool.Allocate();
    if (m_videoParam.src10Enable) {
        out->m_origin10 = m_input10FrameDataPool.Allocate();
    }

    if (m_videoParam.inputVideoMem) { //   from d3d to internal frame in system memory
        if ((st = fa.Lock(fa.pthis,in.Data.MemId, &in.Data)) != MFX_ERR_NONE)
            Throw(std::runtime_error("LockExternalFrame failed"));
        //(m_videoParam.fourcc == MFX_FOURCC_NV12 || m_videoParam.fourcc == MFX_FOURCC_NV16)
        //    ? CopyAndPad<uint8_t>(in, *out->m_origin, m_videoParam.fourcc)
        //    : CopyAndPad<uint16_t>(in, *out->m_origin, m_videoParam.fourcc);
        //fa.Unlock(fa.pthis, in.Data.MemId, &in.Data);
        int32_t bpp = (m_videoParam.fourcc == P010 || m_videoParam.fourcc == P210) ? 2 : 1;
        int32_t width = out->m_origin->width * bpp;
        int32_t height = out->m_origin->height;
        IppiSize roi = { width, height };
        ippiCopyManaged_8u_C1R(in.Data.Y, in.Data.Pitch, out->m_origin->y, out->m_origin->pitch_luma_bytes, roi, 2);
        roi.height >>= m_videoParam.chromaShiftH;
        ippiCopyManaged_8u_C1R(in.Data.UV, in.Data.Pitch, out->m_origin->uv, out->m_origin->pitch_chroma_bytes, roi, 2);
        fa.Unlock(fa.pthis, in.Data.MemId, &in.Data);
        PadRectLuma(*out->m_origin, m_videoParam.fourcc, 0, 0, out->m_origin->width, out->m_origin->height);
    } else {
        bool locked = false;
        if (in.Data.Y == 0) {
            if ((st = fa.Lock(fa.pthis,in.Data.MemId, &in.Data)) != MFX_ERR_NONE)
                Throw(std::runtime_error("LockExternalFrame failed"));
            locked = true;
        }

        if (m_videoParam.src10Enable) {
            CopyAndPad<uint16_t>(in, *out->m_origin10, MFX_FOURCC_P010);
            convert16bTo8b_shift(out);
            PadRectLumaAndChroma(*out->m_origin, m_videoParam.fourcc, 0, 0, m_videoParam.Width, m_videoParam.Height);
        } else if (m_videoParam.superResFlag) {
            DownScale(in, *out->m_origin);
            PadRectLumaAndChroma(*out->m_origin, m_videoParam.fourcc, 0, 0, m_videoParam.Width, m_videoParam.Height);
        } else {
        (m_videoParam.fourcc == MFX_FOURCC_NV12 || m_videoParam.fourcc == MFX_FOURCC_NV16)
            ? CopyAndPad<uint8_t>(in, *out->m_origin, m_videoParam.fourcc)
            : CopyAndPad<uint16_t>(in, *out->m_origin, m_videoParam.fourcc);
        }

        if (locked)
            fa.Unlock(fa.pthis, in.Data.MemId, &in.Data);
    }
    m_core.DecreaseReference(&inExternal->Data); // do it here

    if (m_videoParam.doDumpSource && (m_videoParam.fourcc == MFX_FOURCC_NV12 || m_videoParam.fourcc == MFX_FOURCC_P010)) {
        std::ofstream outfile;
        std::ios_base::openmode mode(std::ios_base::app | std::ios_base::binary);
        if (out->m_frameOrder == 0)
             mode = std::ios_base::binary | std::ios_base::trunc;

            outfile.open(m_videoParam.sourceDumpFileName, mode);

        if (outfile.is_open()) {

            int32_t luSz = (m_videoParam.bitDepthLuma == 8) ? 1 : 2;
            int32_t luW = m_videoParam.Width - m_videoParam.CropLeft - m_videoParam.CropRight;
            int32_t luH = m_videoParam.Height - m_videoParam.CropTop - m_videoParam.CropBottom;
            int32_t luPitch = out->m_origin->pitch_luma_bytes;
            uint8_t *luPtr = out->m_origin->y + (m_videoParam.CropTop * luPitch + m_videoParam.CropLeft * luSz);
            for (int32_t y = 0; y < luH; y++, luPtr += luPitch)
                outfile.write((const char*)luPtr, luSz*luW);

            int32_t chSz = (m_videoParam.bitDepthChroma == 8) ? 1 : 2;
            int32_t chW = luW;
            int32_t chH = luH >> 1;
            int32_t chPitch = out->m_origin->pitch_luma_bytes;
            uint8_t *chPtr = out->m_origin->uv + (m_videoParam.CropTop / 2 * chPitch + m_videoParam.CropLeft * chSz);
            for (int32_t y = 0; y < chH; y++, chPtr += chPitch)
                outfile.write((const char*)chPtr, chSz*chW);

            outfile.close();
        }
    }

        // attach lowres surface to frame
    if (m_la.get() &&
        (m_videoParam.LowresFactor && (m_videoParam.AnalyzeCmplx || (m_videoParam.DeltaQpMode & (AMT_DQP_PAQ | AMT_DQP_CAL)))))
        out->m_lowres = m_frameDataLowresPool.Allocate();

    // attach statistics to frame
    Statistics* stats = out->m_stats[0] = m_statsPool.Allocate();
    stats->ResetAvgMetrics();
    std::fill(stats->qp_mask.begin(), stats->qp_mask.end(), 0);
    if (m_la.get()) {
        if (m_videoParam.LowresFactor && (m_videoParam.AnalyzeCmplx || (m_videoParam.DeltaQpMode & (AMT_DQP_PAQ | AMT_DQP_CAL)))) {
            out->m_stats[1] = m_statsLowresPool.Allocate();
            out->m_stats[1]->ResetAvgMetrics();
        }
        if (m_videoParam.SceneCut) {
            out->m_sceneStats = m_sceneStatsPool.Allocate();
        }
    }

    // each new frame should be analysed by lookahead algorithms family.
#ifdef ZERO_DELAY_ANALYZE_CMPLX
    uint32_t ownerCount = 0;
#else
    uint32_t ownerCount = (m_videoParam.AnalyzeCmplx ? 1 : 0);
#endif
    out->m_lookaheadRefCounter = ownerCount;

    // hardcoded params resulted in quality down (Q-1%) but speed gain due to 1 pass algorithm (search+apply)
    const int32_t nb_strength_bits = 3;
    const int32_t predefined_strengths[2][8] = {
        { 0, 2, 4, 4, 6, 6, 8, 8 },
        { 0, 0, 0, 2, 2, 4, 4, 8 }
    };
    out->m_cdefParam.cdef_bits = 3;
    out->m_cdefParam.nb_cdef_strengths = 1 << nb_strength_bits;
    for (int i = 0; i < 8; i++) {
        out->m_cdefParam.cdef_strengths[i]    = 4 * predefined_strengths[0][i];
        out->m_cdefParam.cdef_uv_strengths[i] = 4 * predefined_strengths[1][i];
    }
#if USE_CMODEL_PAK
    // c-model multi-tile wa
    out->m_numFinishedTiles = 0;
#endif
}

void AV1Encoder::PerformPadRecon(Frame *frame)
{
    PadRectLumaAndChroma(*frame->m_recon, m_videoParam.fourcc, 0, 0, frame->m_recon->width, frame->m_recon->height);
}

uint32_t VM_CALLCONVENTION AV1Encoder::FeiThreadRoutineStarter(void *p)
{
    ((AV1Encoder *)p)->FeiThreadRoutine();
    return 0;
}


static char* opName(int op) {
    switch (op) {
    case MFX_FEI_H265_OP_NOP: return "NOP";
    case MFX_FEI_H265_OP_GPU_COPY_SRC: return "CopySRC";
    case MFX_FEI_H265_OP_GPU_COPY_REF: return "CopyREF";
    case MFX_FEI_H265_OP_INTRA_MODE: return "INTRA";
    case MFX_FEI_H265_OP_INTRA_DIST: return "INTRA_DIST";
    case MFX_FEI_H265_OP_INTER_ME: return "ME";
    case MFX_FEI_H265_OP_INTERPOLATE: return "Interpol";
    case MFX_FEI_H265_OP_BIREFINE: return "BREFINE";
    case MFX_FEI_H265_OP_POSTPROC: return "POSTPROC";
    case MFX_FEI_H265_OP_DEBLOCK: return "DEBLOCK";
    case MFX_FEI_H265_OP_INTER_MD: return "MD";
    }
    return "UNKNW";
}

#define DPRINT 0
void AV1Encoder::FeiThreadRoutine()
{
    try {
        while (true) {
            ThreadingTask *task = NULL;

            std::unique_lock<std::mutex> guard(m_feiCritSect);

            if (!m_feiSubmitTasks.size()) {
                m_feiCondVar.wait(guard, [this] {
                    return m_stopFeiThread != 0 || (m_pauseFeiThread != 1 && (m_feiSubmitTasks.size() || m_feiWaitTasks.size())); });
            }
            if (m_stopFeiThread)
                break;

            if (!m_feiSubmitTasks.empty()) {
#if DPRINT
                fprintf(stderr, "q: ");
                for(auto ts: m_feiSubmitTasks) fprintf(stderr, "%d/%s/%d  ", ts->frame->m_frameOrder, opName(ts->feiOp),ts->frame->m_pyramidLayer);
                fprintf(stderr, "\n");
#endif
                std::deque<ThreadingTask *>::iterator t = m_feiSubmitTasks.begin();
                ThreadingTask *tt = *t;

                if (!m_outputQueue.empty()) {
                    Frame *targetFrame = m_outputQueue.front();

                    std::deque<ThreadingTask *>::iterator i = ++m_feiSubmitTasks.begin();
                    std::deque<ThreadingTask *>::iterator e = m_feiSubmitTasks.end();

                    for (; i != e; ++i) {
                        ThreadingTask *ii = *i;
                        if ((ii)->frame == targetFrame && (tt)->frame != targetFrame ||
                            (ii)->frame->m_pyramidLayer < (tt)->frame->m_pyramidLayer ||
                            (ii)->frame->m_pyramidLayer == (tt)->frame->m_pyramidLayer && (ii)->frame->m_encOrder < (tt)->frame->m_encOrder) {
                            tt = ii;
                            t = i;
                        }
                    }
                }
                task = (tt);
                m_feiSubmitTasks.erase(t);
            }
            m_feiThreadRunning = 1;
            guard.unlock();


            if (task) {
#if DPRINT
                fprintf(stderr,"task: %d : %s\n\n", task->frame->m_frameOrder, opName(task->feiOp));
#endif
                while (m_feiWaitTasks.size() > 0)
                    if (!FeiThreadWait(0))
                        break;
                FeiThreadSubmit(*task);
            } else {
                assert(!m_feiWaitTasks.empty());
                if (!FeiThreadWait(2000))
                    Throw(std::runtime_error(""));
            }
            m_feiThreadRunning = 0;
        }
    } catch (...) {
        assert(!"FeiThreadRoutine failed with exception");
        m_feiThreadRunning = 0;
        return;
    }
}

uint32_t GetDist(FeiOutData **data, int32_t size, int32_t x, int32_t y, int32_t distIdx)
{
    assert(size < 4);
    assert((size == 3 || size == 2) && distIdx < 9 || (size == 1 || size == 0) && distIdx == 0);
    uint32_t pitch = data[size]->m_pitch;
    uint8_t *p = data[size]->m_sysmem;
    int32_t recordSize = (size == 3 || size == 2) ? 64 : 8;
    return *(uint32_t *)(p + y * pitch + x * recordSize + 4 + distIdx * sizeof(uint32_t));
}

mfxI16Pair GetMv(FeiOutData **data, int32_t size, int32_t x, int32_t y)
{
    assert(size < 4);
    uint32_t pitch = data[size]->m_pitch;
    uint8_t *p = data[size]->m_sysmem;
    int32_t recordSize = (size == 3 || size == 2) ? 64 : 8;
    return *(mfxI16Pair *)(p + y * pitch + x * recordSize);
}


static const uint16_t compound_mode_ctx_map[3][COMP_NEWMV_CTXS] = {
    { 0, 1, 1, 1, 1 },
    { 1, 2, 3, 4, 4 },
    { 4, 4, 5, 6, 7 },
};

uint32_t GetRefConfig(const Frame &frame)
{
    if (frame.isUniq[LAST_FRAME] + frame.isUniq[GOLDEN_FRAME] + frame.isUniq[ALTREF_FRAME] == 1)
        return 0; // single ref
    if (!frame.compoundReferenceAllowed)
        return 1; // two refs, no compound
    return 2; // compound
}

static const float LUT_IEF_SPLIT_MT[4][4] = {
    { 2.587840f, 7.278976f, 10.856256f, 10.856256f },
    { 6.872320f, 7.278976f, 10.856256f, 10.856256f },
    { 7.239296f, 7.278976f, 10.856256f, 10.856256f },
    { 7.646336f, 9.961600f, 10.856256f, 10.856256f },
};

static const float LUT_IEF_SPLIT_ST[4][4] = {
    { 20.f,  77.150f, 142.184f, 142.184f },
    { 20.f,  89.805f, 142.184f, 142.184f },
    { 20.f,  89.805f, 249.454f, 249.454f },
    { 20.f, 249.454f, 249.454f, 249.454f },
};

static const float LUT_IEF_SPLIT_A[4][4] = {
    { 0.303627f, 0.423133f, 0.397384f, 0.397384f },
    { 0.315012f, 0.435791f, 0.397396f, 0.397396f },
    { 0.315445f, 0.443448f, 0.500000f, 0.500000f },
    { 0.314350f, 0.456550f, 0.500000f, 0.500000f },
};

static const float LUT_IEF_SPLIT_B[4][4] = {
    { 0.547588f, 0.022428f, 0.473092f, 0.473092f },
    { 0.572353f, 0.041593f, 0.527727f, 0.527727f },
    { 0.677961f, 0.203189f, 0.000000f, 0.000000f },
    { 0.794845f, 0.228990f, 0.000000f, 0.000000f },
};

static const float LUT_IEF_SPLIT_M[4][4] = {
    { 0.003458f, 0.013396f, 0.013396f, 0.013396f },
    { 0.006642f, 0.013410f, 0.013410f, 0.013410f },
    { 0.006441f, 0.013410f, 0.013410f, 0.013410f },
    { 0.010107f, 0.137010f, 0.137010f, 0.137010f },
};

const float LUT_IEF_LEAF_A[4][4] = {
    { 0.224285f, 0.233921f, 0.242109f, 0.265764f },
    { 0.225157f, 0.281690f, 0.286192f, 0.279662f },
    { 0.228965f, 0.276006f, 0.302635f, 0.336254f },
    { 0.244477f, 0.308719f, 0.324380f, 0.378376f },
};

const float LUT_IEF_LEAF_B[4][4] = {
    { 0.278397f, 0.413391f, 0.424680f, 0.517953f },
    { 0.437689f, 0.314939f, 0.365299f, 0.622422f },
    { 0.628257f, 0.525718f, 0.548104f, 0.617987f },
    { 0.688823f, 0.626691f, 0.605718f, 0.553362f },
};

const float LUT_IEF_LEAF_M[4][4] = {
    { 0.295610f, 0.218830f, 0.089055f, 0.049053f },
    { 0.201050f, 0.177870f, 0.028076f, 0.025316f },
    { 0.177870f, 0.146770f, 0.028076f, 0.023686f },
    { 0.146770f, 0.063581f, 0.028076f, 0.012673f },
};

void SetupModeDecisionControl(const AV1VideoParam &par, const Frame &frame, MdControl *ctrl)
{
    int32_t frmclass = (frame.IsIntra() ? 0 : (((par.BiPyramidLayers > 1 && frame.m_pyramidLayer >= 3) || frame.IsNotRef()) ? 2 : 1));
    float Dz0 = ((frmclass == 0) ? 48.0f : ((frmclass == 1) ? 44.0f : 40.0f));
    float Dz1 = ((frmclass == 0) ? 48.0f : ((frmclass == 1) ? 40.0f : 40.0f));
    Dz0 = Dz1 = 40.0f;

    ctrl->miCols = uint16_t(par.miCols);
    ctrl->miRows = uint16_t(par.miRows);
    ctrl->qparam.zbin[0] = par.qparamY[frame.m_sliceQpY].zbin[0];
    ctrl->qparam.zbin[1] = par.qparamY[frame.m_sliceQpY].zbin[1];
    ctrl->qparam.round[0] = uint16_t(uint32_t(Dz0 * par.qparamY[frame.m_sliceQpY].dequant[0]) >> 7);
    ctrl->qparam.round[1] = uint16_t(uint32_t(Dz1 * par.qparamY[frame.m_sliceQpY].dequant[1]) >> 7);
    ctrl->qparam.quant[0] = par.qparamY[frame.m_sliceQpY].quant[0];
    ctrl->qparam.quant[1] = par.qparamY[frame.m_sliceQpY].quant[1];
    ctrl->qparam.quantShift[0] = par.qparamY[frame.m_sliceQpY].quantShift[0];
    ctrl->qparam.quantShift[1] = par.qparamY[frame.m_sliceQpY].quantShift[1];
    ctrl->qparam.dequant[0] = par.qparamY[frame.m_sliceQpY].dequant[0];
    ctrl->qparam.dequant[1] = par.qparamY[frame.m_sliceQpY].dequant[1];
    ctrl->lambda = frame.m_lambda;
    ctrl->lambdaInt = uint16_t(frame.m_lambdaSatd * 2048 + 0.5);
    ctrl->compound_allowed = (uint8_t)frame.compoundReferenceAllowed;
    if (frame.compoundReferenceAllowed) {
        ctrl->bidir_compound_or_single_ref = (frame.m_picCodeType == MFX_FRAMETYPE_B);
    } else {
        ctrl->bidir_compound_or_single_ref = !frame.isUniq[GOLDEN_FRAME] && !frame.isUniq[ALTREF_FRAME];
    }
    const int32_t qctx = get_q_ctx(frame.m_sliceQpY);
    const TxbBitCounts &tbc = frame.bitCount->txb[qctx][TX_8X8][PLANE_TYPE_Y];
    for (int i = 0; i < 7; i++)
        ctrl->bc.eobMulti[i] = tbc.eobMulti[i];
    ctrl->bc.coeffEobDc[0] = 0;
    for (int i = 1; i < 16; i++)
        ctrl->bc.coeffEobDc[i] = tbc.coeffBaseEob[0][std::min(3,i) - 1] + tbc.coeffBrAcc[0][i];
    for (int i = 0; i < 3; i++)
        ctrl->bc.coeffBaseEobAc[i] = tbc.coeffBaseEob[1][i];
    for (int i = 0; i < 13; i++)
        for (int j = 0; j < 2; j++)
            ctrl->bc.txbSkip[i][j] = tbc.txbSkip[i][j];
    for (int ctx_skip = 0; ctx_skip < 3; ctx_skip++) {
        ctrl->bc.skip[ctx_skip][0] = frame.bitCount->skip[ctx_skip][0];
        ctrl->bc.skip[ctx_skip][1] = frame.bitCount->skip[ctx_skip][1];
    }
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 4; j++)
            ctrl->bc.coeffBase[i][j] = tbc.coeffBase[i][j];
    for (int i = 0; i < 21; i++)
        for (int j = 0; j < 16; j++)
            ctrl->bc.coeffBrAcc[i][j] = tbc.coeffBrAcc[i][j];
    memcpy(ctrl->bc.refFrames, frame.m_refFrameBitsAv1, sizeof(ctrl->bc.refFrames));

    const auto &newmvBits  = frame.bitCount->newMvBit;
    const auto &globmvBits = frame.bitCount->zeroMvBit;
    const auto &refmvBits  = frame.bitCount->refMvBit;

    for (int32_t nearestRefMatch = 0; nearestRefMatch < 3; nearestRefMatch++) {
        for (int32_t refMatch = 0; refMatch < 3; refMatch++) {
            for (int32_t hasNewMv = 0; hasNewMv < 2; hasNewMv++) {
                const int32_t modeCtx   = GetModeContext(nearestRefMatch, refMatch, hasNewMv, 1);
                const int32_t newmvCtx  = (modeCtx & NEWMV_CTX_MASK);
                const int32_t globmvCtx = (modeCtx >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;
                const int32_t refmvCtx  = (modeCtx >> REFMV_OFFSET) & REFMV_CTX_MASK;
                const int32_t compModeCtx = compound_mode_ctx_map[refmvCtx >> 1][std::min(newmvCtx, COMP_NEWMV_CTXS - 1)];

                auto &singleModes = ctrl->bc.singleModes[nearestRefMatch][refMatch][hasNewMv];
                auto &compModes   = ctrl->bc.compModes[nearestRefMatch][refMatch][hasNewMv];

                singleModes[NEARESTMV_] = newmvBits[newmvCtx][1] + globmvBits[globmvCtx][1] + refmvBits[refmvCtx][0];
                singleModes[NEARMV_   ] = newmvBits[newmvCtx][1] + globmvBits[globmvCtx][1] + refmvBits[refmvCtx][1];
                singleModes[ZEROMV_   ] = newmvBits[newmvCtx][1] + globmvBits[globmvCtx][0];
                singleModes[NEWMV_    ] = newmvBits[newmvCtx][0];

                compModes[NEARESTMV_] = frame.bitCount->interCompMode[compModeCtx][NEAREST_NEARESTMV - NEAREST_NEARESTMV];
                compModes[NEARMV_   ] = frame.bitCount->interCompMode[compModeCtx][NEAR_NEARMV       - NEAREST_NEARESTMV];
                compModes[ZEROMV_   ] = frame.bitCount->interCompMode[compModeCtx][ZERO_ZEROMV       - NEAREST_NEARESTMV];
                compModes[NEWMV_    ] = frame.bitCount->interCompMode[compModeCtx][NEW_NEWMV         - NEAREST_NEARESTMV];
            }
        }
    }

#if GPU_VARTX
    for (int txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
        const TxbBitCounts &tbc = frame.bitCount->txb[qctx][txSize][PLANE_TYPE_Y];
        for (int i = 0; i < 11; i++)
            ctrl->txb[txSize].eobMulti[i] = tbc.eobMulti[i];
        ctrl->txb[txSize].coeffEobDc[0] = 0;
        for (int i = 1; i < 16; i++)
            ctrl->txb[txSize].coeffEobDc[i] = tbc.coeffBaseEob[0][MIN(3, i) - 1] + tbc.coeffBrAcc[0][i];
        for (int i = 0; i < 3; i++)
            ctrl->txb[txSize].coeffBaseEobAc[i] = tbc.coeffBaseEob[1][i];
        for (int i = 0; i < 13; i++)
            for (int j = 0; j < 2; j++)
                ctrl->txb[txSize].txbSkip[i][j] = tbc.txbSkip[i][j];
        for (int i = 0; i < 16; i++)
            for (int j = 0; j < 4; j++)
                ctrl->txb[txSize].coeffBase[i][j] = tbc.coeffBase[i][j];
        for (int i = 0; i < 21; i++)
            for (int j = 0; j < 16; j++)
                ctrl->txb[txSize].coeffBrAcc[i][j] = tbc.coeffBrAcc[i][j];
    }

    ctrl->initDz[0] = Dz0;
    ctrl->initDz[1] = Dz1;

    const int qpi = get_q_ctx(frame.m_sliceQpY);
    ctrl->high_freq_coeff_curve[TX_4X4][0] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_4X4][0][0];
    ctrl->high_freq_coeff_curve[TX_4X4][1] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_4X4][0][1];
    ctrl->high_freq_coeff_curve[TX_8X8][0] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_8X8][0][0];
    ctrl->high_freq_coeff_curve[TX_8X8][1] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_8X8][0][1];
    ctrl->high_freq_coeff_curve[TX_16X16][0] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_16X16][0][0];
    ctrl->high_freq_coeff_curve[TX_16X16][1] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_16X16][0][1];
    ctrl->high_freq_coeff_curve[TX_32X32][0] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_32X32][0][0];
    ctrl->high_freq_coeff_curve[TX_32X32][1] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_32X32][0][1];
#endif // GPU_VARTX

#ifdef PARALLEL_LOW_DELAY_FAST
    if (par.SceneCut && (!frame.m_temporalSync || frame.m_temporalActv)) {
        ctrl->fastMode = 0;
    } else {
        ctrl->fastMode = 1;
    }
#else
    ctrl->fastMode = 0;
#endif
    for (int qIdx = 0; qIdx < 4; qIdx++) {
        for (int qpLayer = 0; qpLayer < 4; qpLayer++) {
            ctrl->lutIef[qIdx][qpLayer].split.mt = LUT_IEF_SPLIT_MT[qIdx][qpLayer];
            ctrl->lutIef[qIdx][qpLayer].split.st = LUT_IEF_SPLIT_ST[qIdx][qpLayer];
            ctrl->lutIef[qIdx][qpLayer].split.a = LUT_IEF_SPLIT_A[qIdx][qpLayer];
            ctrl->lutIef[qIdx][qpLayer].split.b = LUT_IEF_SPLIT_B[qIdx][qpLayer];
            ctrl->lutIef[qIdx][qpLayer].split.m = LUT_IEF_SPLIT_M[qIdx][qpLayer];

            ctrl->lutIef[qIdx][qpLayer].leaf.a = LUT_IEF_LEAF_A[qIdx][qpLayer];
            ctrl->lutIef[qIdx][qpLayer].leaf.b = LUT_IEF_LEAF_B[qIdx][qpLayer];
            ctrl->lutIef[qIdx][qpLayer].leaf.m = LUT_IEF_LEAF_M[qIdx][qpLayer];
        }
    }
}

#if GPU_VARTX
void SetupAdz(const Frame *curr, const Frame *prev)
{
    const AV1VideoParam &par = *curr->m_par;

    ADzCtx *currDz = (ADzCtx *)curr->m_feiAdz->m_sysmem;
    ADzCtx *currDdz = (ADzCtx *)curr->m_feiAdzDelta->m_sysmem;
    ADzCtx *prevDz  = (ADzCtx *)prev->m_feiAdz->m_sysmem;
    ADzCtx *prevDdz = (ADzCtx *)prev->m_feiAdzDelta->m_sysmem;

    int32_t frmclass = (curr->IsIntra() ? 0 : (((par.BiPyramidLayers > 1 && curr->m_pyramidLayer >= 3) || curr->IsNotRef()) ? 2 : 1));
    float Dz0 = ((frmclass == 0) ? 48.0f : ((frmclass == 1) ? 44.0 : 40.0));
    float Dz1 = ((frmclass == 0) ? 48.0f : ((frmclass == 1) ? 40.0 : 40.0));

    for (int sbr = 0, sbi = 0; sbr < par.sb64Rows; sbr++) {
        for (int sbc = 0; sbc < par.sb64Cols; sbc++, sbi++) {
            const TileBorders tb = GetTileBordersSb(curr->m_tileParam, sbr, sbc);

            if (curr->m_picCodeType == MFX_FRAMETYPE_P && prev->m_picCodeType == MFX_FRAMETYPE_P) {
                float varDz = (par.SceneCut) ? 10.0f : 8.0f; // only temporal
                currDz[sbi] = prevDz[sbi];
                if (sbc > tb.colStart)
                    currDz[sbi].AddWieghted(prevDdz[sbi - 1].aqroundFactorY,
                                            prevDdz[sbi - 1].aqroundFactorUv,
                                            prevDz[sbi - 1].aqroundFactorY,
                                            prevDz[sbi - 1].aqroundFactorUv);
                if (sbr > tb.rowStart)
                    currDz[sbi].AddWieghted(prevDdz[sbi - par.sb64Cols].aqroundFactorY,
                                            prevDdz[sbi - par.sb64Cols].aqroundFactorUv,
                                            prevDz[sbi - par.sb64Cols].aqroundFactorY,
                                            prevDz[sbi - par.sb64Cols].aqroundFactorUv);
                if (sbc + 1 < tb.colEnd && sbr > tb.rowStart)
                    currDz[sbi].AddWieghted(prevDdz[sbi + 1 - par.sb64Cols].aqroundFactorY,
                                            prevDdz[sbi + 1 - par.sb64Cols].aqroundFactorUv,
                                            prevDz[sbi + 1 - par.sb64Cols].aqroundFactorY,
                                            prevDz[sbi + 1 - par.sb64Cols].aqroundFactorUv);
                if (sbc + 1 < tb.colEnd && sbr + 1 < tb.rowEnd)
                    currDz[sbi].AddWieghted(prevDdz[sbi + 1 + par.sb64Cols].aqroundFactorY,
                                            prevDdz[sbi + 1 + par.sb64Cols].aqroundFactorUv,
                                            prevDz[sbi + 1 + par.sb64Cols].aqroundFactorY,
                                            prevDz[sbi + 1 + par.sb64Cols].aqroundFactorUv);

                currDz[sbi].SaturateAcDc(Dz0, Dz1, varDz); // Saturate
            } else {
                float varDz = 4.0f;
                currDz[sbi].SetDcAc(Dz0, Dz1); // Set
                currDz[sbi].SetMin(prevDz[sbi].aqroundFactorY, prevDz[sbi].aqroundFactorUv); // Use min
                currDz[sbi].SaturateAcDc(Dz0, Dz1, varDz); // Saturate
            }
        }
    }
}
#endif // GPU_VARTX

void AV1Encoder::FeiThreadSubmit(ThreadingTask &task)
{
#if TT_TRACE
    char ttname[256] = "Submit ";
    strcat(ttname, GetFeiOpName(task.feiOp));
    strcat(ttname, " ");
    strcat(ttname, NUMBERS[task.poc & 0x7f]);
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, ttname);
#endif // TT_TRACE

#ifdef VT_TRACE
    MFXTraceTask *tracetask = new MFXTraceTask(&_trace_static_handle2[task.poc & 0x3f][task.action],
    __FILE__, __LINE__, __FUNCTION__, MFX_TRACE_CATEGORY, MFX_TRACE_LEVEL_API, "", false);
#endif // VT_TRACE

    assert(task.action == TT_GPU_SUBMIT);
    assert(task.finished == 0);

    mfxFEIH265Input in = {};
    mfxExtFEIH265Output out = {};

    in.FrameType = task.frame->m_picCodeType;
    in.IsRef     = task.frame->m_isRef;
    in.PyramidLayer = task.frame->m_pyramidLayer;
    in.SliceQpY     = task.frame->m_sliceQpY;
    in.TemporalSync = task.frame->m_temporalSync;
    in.FEIOp = task.feiOp;
    in.SaveSyncPoint = 1;

    if (task.feiOp == MFX_FEI_H265_OP_GPU_COPY_SRC) {
        in.copyArgs.surfSys = task.frame->m_origin->m_handle;
        in.copyArgs.surfVid = task.frame->m_feiOrigin->m_handle;
        in.copyArgs.copyChroma = 0;
        in.SaveSyncPoint = 0;

    } else if (task.feiOp == MFX_FEI_H265_OP_GPU_COPY_REF) {
        in.copyArgs.surfSys = task.frame->m_recon->m_handle;
        in.copyArgs.surfVid = task.frame->m_feiRecon->m_handle;
        if (m_videoParam.enableCmPostProc == 0) {
            in.FEIOp = MFX_FEI_H265_OP_GPU_COPY_SRC;
            in.copyArgs.copyChroma = 1;
        }

    } else if (task.feiOp == MFX_FEI_H265_OP_INTER_ME) {
        Frame *ref = task.frame->refFramesVp9[task.refIdx];
        in.meArgs.surfSrc = task.frame->m_feiOrigin->m_handle;
        in.meArgs.surfRef = ref->m_feiRecon->m_handle;
        in.meArgs.lambda = task.frame->m_lambdaSatd;
        in.meArgs.global_mvy = task.refIdx == LAST_FRAME ? task.frame->m_globalMvy : 0;
        in.meArgs.sliceIdx = task.sliceIdx;
        //if (task.refIdx == LAST_FRAME)
        //    fprintf(stderr, "frame=%d ref=%d mvy=%d\n", task.frame->m_frameOrder, task.refIdx, in.meArgs.global_mvy);

        FeiOutData **feiMeData = task.frame->m_feiInterData[task.refIdx];
        for (int32_t blksize = 0; blksize < 4; blksize++) {
            int32_t feiBlkIdx = blkSizeInternal2Fei[blksize];
            out.SurfInterData[feiBlkIdx] = feiMeData[blksize]->m_handle;
        }
        in.SaveSyncPoint = 0;

    } else if (task.feiOp == MFX_FEI_H265_OP_INTER_MD) {
#if PROTOTYPE_GPU_MODE_DECISION
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        PrototypeGpuModeDecision(task.frame);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        in.mdArgs.surfSrc = task.frame->m_feiOrigin->m_handle;
        in.mdArgs.surfRef0 = task.frame->refFramesVp9[LAST_FRAME]->m_feiRecon->m_handle;
        in.mdArgs.surfReconSys = task.frame->m_recon->m_handle;
        in.mdArgs.surfInterData0[0] = task.frame->m_feiInterData[LAST_FRAME][0]->m_handle; // 8x8
        in.mdArgs.surfInterData0[1] = task.frame->m_feiInterData[LAST_FRAME][1]->m_handle; // 16x16
        in.mdArgs.surfInterData0[2] = task.frame->m_feiInterData[LAST_FRAME][2]->m_handle; // 32x32
        in.mdArgs.surfInterData0[3] = task.frame->m_feiInterData[LAST_FRAME][3]->m_handle; // 64x64

        in.mdArgs.global_mvy = task.frame->m_globalMvy;

        in.mdArgs.refConfig = GetRefConfig(*task.frame);
        const int32_t secondRef = task.frame->compoundReferenceAllowed
            ? ALTREF_FRAME
            : (task.frame->isUniq[GOLDEN_FRAME] ? GOLDEN_FRAME : LAST_FRAME);

        in.mdArgs.surfRef1 = task.frame->refFramesVp9[secondRef]->m_feiRecon->m_handle;
        in.mdArgs.surfInterData1[0] = task.frame->m_feiInterData[secondRef][0]->m_handle; // 8x8
        in.mdArgs.surfInterData1[1] = task.frame->m_feiInterData[secondRef][1]->m_handle; // 16x16
        in.mdArgs.surfInterData1[2] = task.frame->m_feiInterData[secondRef][2]->m_handle; // 32x32
        in.mdArgs.surfInterData1[3] = task.frame->m_feiInterData[secondRef][3]->m_handle; // 64x64

        in.mdArgs.modeInfo1 = task.frame->m_feiModeInfo1->m_handle;
        in.mdArgs.modeInfo2 = task.frame->m_feiModeInfo2->m_handle;

        const int32_t alignedWidth = ((task.frame->m_origin->width + 63)&~63);
        const int32_t alignedHeight = ((task.frame->m_origin->height + 63)&~63);

        for (int32_t log2Size = 3; log2Size <= 6; log2Size++) {
            const int32_t width = alignedWidth >> log2Size;
            const int32_t height = alignedHeight >> log2Size;
            const int32_t rscs_pitch = task.frame->m_stats[0]->m_pitchRsCs[log2Size - 2];
            const int32_t surf_pitch = task.frame->m_feiRs[log2Size - 3]->m_pitch;
            const int32_t *in_rs = task.frame->m_stats[0]->m_rs[log2Size - 2];
            const int32_t *in_cs = task.frame->m_stats[0]->m_cs[log2Size - 2];
            uint8_t *out_rs = task.frame->m_feiRs[log2Size - 3]->m_sysmem;
            uint8_t *out_cs = task.frame->m_feiCs[log2Size - 3]->m_sysmem;

            for (int i = 0; i < height; i++)
                memcpy(out_rs + i * surf_pitch, in_rs + rscs_pitch * i, sizeof(int32_t) *  width);
            for (int i = 0; i < height; i++)
                memcpy(out_cs + i * surf_pitch, in_cs + rscs_pitch * i, sizeof(int32_t) *  width);

            in.mdArgs.Rs[log2Size - 3] = task.frame->m_feiRs[log2Size - 3]->m_handle;
            in.mdArgs.Cs[log2Size - 3] = task.frame->m_feiCs[log2Size - 3]->m_handle;
        }

#if GPU_VARTX
        assert(task.frame->m_picCodeType != MFX_FRAMETYPE_I);
        Frame *temporalFrame = (task.frame->m_picCodeType == MFX_FRAMETYPE_P)
            ? task.frame->refFramesVp9[LAST_FRAME]
            : ((task.frame->refFramesVp9[LAST_FRAME]->m_pyramidLayer > task.frame->refFramesVp9[ALTREF_FRAME]->m_pyramidLayer)
                ? task.frame->refFramesVp9[LAST_FRAME]
                : task.frame->refFramesVp9[ALTREF_FRAME]);

        SetupAdz(task.frame, temporalFrame);

        in.mdArgs.prevAdz = temporalFrame->m_feiAdz->m_handle;
        in.mdArgs.prevAdzDelta = temporalFrame->m_feiAdzDelta->m_handle;
        in.mdArgs.currAdz = task.frame->m_feiAdz->m_handle;
        in.mdArgs.currAdzDelta = task.frame->m_feiAdzDelta->m_handle;
        in.mdArgs.varTxInfo = task.frame->m_feiVarTxInfo->m_handle;
        in.mdArgs.coefs = task.frame->m_feiCoefs->m_handle;
#endif // GPU_VARTX

        MdControl modeDecisionParam;
        SetupModeDecisionControl(*task.frame->m_par, *task.frame, &modeDecisionParam);
        in.mdArgs.param = (uint8_t *)&modeDecisionParam;
        in.mdArgs.sliceIdx = task.sliceIdx;
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
#endif // PROTOTYPE_GPU_MODE_DECISION

    } else {
        Throw(std::runtime_error(""));
    }

    mfxFEISyncPoint syncpoint = 0;
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
    if (task.feiOp != MFX_FEI_H265_OP_INTER_MD)
        if (H265FEI_ProcessFrameAsync(m_fei, &in, &out, &syncpoint) != MFX_ERR_NONE)
            Throw(std::runtime_error(""));
    if (task.feiOp == MFX_FEI_H265_OP_INTER_MEPU)
        mfxStatus sts = H265FEI_SyncOperation(m_fei, syncpoint, 2000);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
    if (H265FEI_ProcessFrameAsync(m_fei, &in, &out, &syncpoint) != MFX_ERR_NONE)
        Throw(std::runtime_error(""));
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

    // the following modifications should be guarded by m_feiCritSect
    // because finished, numDownstreamDependencies and downstreamDependencies[]
    // may be accessed from PrepareToEncode()
    {
        SetTaskFinishedThreaded(&task);

        for (int32_t i = 0; i < task.numDownstreamDependencies; i++) {
            ThreadingTask *taskDep = task.downstreamDependencies[i];
            if(taskDep->numUpstreamDependencies.fetch_sub(1) == 1) {
                std::lock_guard<std::mutex> guard(m_feiCritSect);
                if (taskDep->action == TT_GPU_SUBMIT) {
                    m_feiSubmitTasks.push_back(taskDep);
                }
                else if (taskDep->action == TT_GPU_WAIT) {
                    assert(taskDep->finished == 0);
                    assert(taskDep->syncpoint == NULL);
                    assert(in.SaveSyncPoint == 1);
                    taskDep->syncpoint = syncpoint;
                    m_feiWaitTasks.push_back(taskDep);
                }
                else {
                    Throw(std::runtime_error(""));
                }
            }
        }
    }

#ifdef VT_TRACE
    if (tracetask) delete tracetask;
#endif // VT_TRACE
}

bool AV1Encoder::FeiThreadWait(uint32_t timeout)
{
    ThreadingTask *task = m_feiWaitTasks.front();

#if TT_TRACE
    char ttname[256] = "";
    strcat(ttname, timeout ? "Wait " : "GetStatus ");
    strcat(ttname, GetFeiOpName(task->feiOp));
    strcat(ttname, " ");
    strcat(ttname, NUMBERS[task->poc & 0x7f]);
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, ttname);
#endif // TT_TRACE

#ifdef VT_TRACE
    MFXTraceTask *tracetask = new MFXTraceTask(&_trace_static_handle2[task->poc & 0x3f][task->action],
    __FILE__, __LINE__, __FUNCTION__, MFX_TRACE_CATEGORY, MFX_TRACE_LEVEL_API, "", false);
#endif // VT_TRACE

    mfxStatus sts = H265FEI_SyncOperation(m_fei, task->syncpoint, timeout);
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
    if (task->feiOp == MFX_FEI_H265_OP_INTER_MD)
        sts = MFX_ERR_NONE;
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

#ifdef VT_TRACE
    if (tracetask) delete tracetask;
#endif // VT_TRACE

    if (sts == MFX_WRN_IN_EXECUTION)
        return false;
    if (sts != MFX_ERR_NONE)
        Throw(std::runtime_error(""));

    H265FEI_DestroySavedSyncPoint(m_fei, task->syncpoint);
    task->syncpoint = NULL;

    m_feiWaitTasks.pop_front();

    // the following modifications should be guarded by m_critSect
    // because finished, numDownstreamDependencies and downstreamDependencies[]
    // may be accessed from PrepareToEncode() and TaskRoutine()
    SetTaskFinishedThreaded(task);

    {
        for (int32_t i = 0; i < task->numDownstreamDependencies; i++) {
            ThreadingTask *taskDep = task->downstreamDependencies[i];

#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
            assert(taskDep->action != TT_GPU_WAIT);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
            //assert(taskDep->action != TT_GPU_SUBMIT && taskDep->action != TT_GPU_WAIT);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
            if (taskDep->numUpstreamDependencies.fetch_sub(1) == 1) {
                if (taskDep->action == TT_GPU_SUBMIT) {
                    std::lock_guard<std::mutex> guard(m_feiCritSect);
                    m_feiSubmitTasks.push_back(taskDep);
                }
                else
                {
                    std::lock_guard<std::mutex> guard(m_critSect);
                    m_pendingTasks.push_back(taskDep);
                    m_condVar.notify_one();
                }
            }
        }
    }

    return true;
}

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
