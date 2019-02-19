// Copyright (c) 2014-2019 Intel Corporation
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

#include "mfxdefs.h"
#include "mfx_task.h"
#include "vm_interlocked.h"
#include "vm_file.h"
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
#include "mfx_av1_tables.h"

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

namespace {
    int32_t GetTileOffset(int32_t tileNum, int32_t sbs, int32_t tileSzLog2) {
        int32_t offset = ((tileNum * sbs) >> tileSzLog2);
        return std::min(offset, sbs);
    }

    // Find smallest k>=0 such that (blk_size << k) >= target
    int32_t TileLog2(int32_t blk_size, int32_t target) {
        int32_t k;
        for (k = 0; (blk_size << k) < target; k++);
        return k;
    }
};  // namespace

namespace AV1Enc {
    void SetupTileParamVp9(AV1VideoParam *par, int32_t numTileRows, int32_t numTileCols)
    {
        TileParam *tp = &par->tileParam;
        const int32_t miRows = par->miRows;
        const int32_t miCols = par->miCols;
        const int32_t sbCols = (miCols + 7) >> 3;
        const int32_t sbRows = (miRows + 7) >> 3;

        tp->rows = numTileRows;
        tp->cols = numTileCols;
        tp->log2Cols = BSR(numTileCols);
        tp->log2Rows = BSR(numTileRows);
        tp->minLog2Cols = 0;
        while ((MAX_TILE_WIDTH_B64 << tp->minLog2Cols) < sbCols)
            ++tp->minLog2Cols;

        tp->maxLog2Cols = 1;
        while ((sbCols >> tp->maxLog2Cols) >= MIN_TILE_WIDTH_B64)
            ++tp->maxLog2Cols;
        --tp->maxLog2Cols;

        for (int32_t tileCol = 0; tileCol < (1 << tp->log2Cols); tileCol++) {
            // 64x64 units
            tp->colStart[tileCol] = GetTileOffset(tileCol, sbCols, tp->log2Cols);
            tp->colEnd[tileCol]   = GetTileOffset(tileCol + 1, sbCols, tp->log2Cols);
            tp->colWidth[tileCol] = tp->colEnd[tileCol] - tp->colStart[tileCol];
            // 8x8 units
            tp->miColStart[tileCol] = tp->colStart[tileCol] << 3;
            tp->miColEnd[tileCol]   = std::min(tp->colEnd[tileCol] << 3, miCols);
            tp->miColWidth[tileCol] = tp->miColEnd[tileCol] - tp->miColStart[tileCol];

            std::fill(tp->mapSb2TileCol + tp->colStart[tileCol],
                      tp->mapSb2TileCol + tp->colEnd[tileCol], tileCol);
        }
        for (int32_t tileRow = 0; tileRow < (1 << tp->log2Rows); tileRow++) {
            // 64x64 units
            tp->rowStart[tileRow]  = GetTileOffset(tileRow, sbRows, tp->log2Rows);
            tp->rowEnd[tileRow]    = GetTileOffset(tileRow + 1, sbRows, tp->log2Rows);
            tp->rowHeight[tileRow] = tp->rowEnd[tileRow] - tp->rowStart[tileRow];
            // 8x8 units
            tp->miRowStart[tileRow]  = tp->rowStart[tileRow] << 3;
            tp->miRowEnd[tileRow]    = std::min(tp->rowEnd[tileRow] << 3, miRows);
            tp->miRowHeight[tileRow] = tp->miRowEnd[tileRow] - tp->miRowStart[tileRow];

            std::fill(tp->mapSb2TileRow + tp->rowStart[tileRow],
                      tp->mapSb2TileRow + tp->rowEnd[tileRow], tileRow);
        }
    }

    void SetupTileParamAv1(AV1VideoParam *par, int32_t numTileRows, int32_t numTileCols, int32_t uniform)
    {
        TileParam *tp = &par->tileParam;
        const int32_t miRows = par->miRows;
        const int32_t miCols = par->miCols;
        const int32_t sbCols = (miCols + 7) >> 3;
        const int32_t sbRows = (miRows + 7) >> 3;
        const int32_t sbSizeLog2 = 6;
        int32_t maxTileAreaSb = MAX_TILE_AREA >> (2 * sbSizeLog2);

        tp->uniformSpacing = uniform;
        tp->cols = numTileCols;
        tp->rows = numTileRows;
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
                tp->colStart[i] = i * tileWidth;
                tp->colEnd[i]   = std::min(tp->colStart[i] + tileWidth, sbCols);
                tp->colWidth[i] = tp->colEnd[i] - tp->colStart[i];
            }

            tp->minLog2Rows = std::max(tp->minLog2Tiles - tp->log2Cols, 0);
            tp->maxTileHeightSb = sbRows >> tp->minLog2Rows;

            for (int32_t i = 0; i < tp->rows; i++) {
                tp->rowStart[i]  = i * tileHeight;
                tp->rowEnd[i]    = std::min(tp->rowStart[i] + tileHeight, sbRows);
                tp->rowHeight[i] = tp->rowEnd[i] - tp->rowStart[i];
            }
        } else {
            int32_t widestTileWidth = 0;
            for (int32_t i = 0; i < tp->cols; i++) {
                tp->colStart[i] = (i + 0) * sbCols / tp->cols;
                tp->colEnd[i]   = (i + 1) * sbCols / tp->cols;
                tp->colWidth[i] = tp->colEnd[i] - tp->colStart[i];
                widestTileWidth = std::max(widestTileWidth, (int32_t)tp->colWidth[i]);
            }

            if (tp->minLog2Tiles)
                maxTileAreaSb >>= (tp->minLog2Tiles + 1);
            tp->maxTileHeightSb = std::max(1, maxTileAreaSb / widestTileWidth);

            for (int32_t i = 0; i < tp->rows; i++) {
                tp->rowStart[i]  = (i + 0) * sbRows / tp->rows;
                tp->rowEnd[i]    = (i + 1) * sbRows / tp->rows;
                tp->rowHeight[i] = tp->rowEnd[i] - tp->rowStart[i];
            }
        }

        for (int32_t i = 0; i < tp->cols; i++) {
            // 8x8 units
            tp->miColStart[i] = tp->colStart[i] << 3;
            tp->miColEnd[i]   = std::min(tp->colEnd[i] << 3, miCols);
            tp->miColWidth[i] = tp->miColEnd[i] - tp->miColStart[i];

            for (int32_t j = tp->colStart[i]; j < tp->colEnd[i]; j++)
                tp->mapSb2TileCol[j] = i;
        }

        for (int32_t i = 0; i < tp->rows; i++) {
            // 8x8 units
            tp->miRowStart[i]  = tp->rowStart[i] << 3;
            tp->miRowEnd[i]    = std::min(tp->rowEnd[i] << 3, miRows);
            tp->miRowHeight[i] = tp->miRowEnd[i] - tp->miRowStart[i];

            for (int32_t j = tp->rowStart[i]; j < tp->rowEnd[i]; j++)
                tp->mapSb2TileRow[j] = i;
        }
    }
};  // namespace AV1Enc

namespace {
#if TT_TRACE || VT_TRACE || TASK_LOG_ENABLE
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
        "PTILE"
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
static uint32_t taskLogIdx;

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
    uint32_t idx = vm_interlocked_inc32(&taskLogIdx);
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
    fwrite(taskLog, sizeof(TaskLog), taskLogIdx, fp);
    fclose(fp);
    fp = fopen("tasklog.txt","wt");
    LONGLONG start = taskLog[0].start;
    for (uint32_t ii = 0; ii < taskLogIdx; ii++) {
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
#if VT_TRACE
    static mfxTraceStaticHandle _trace_static_handle2[64][TT_COUNT];
//    static char _trace_task_names[64][13][32];
#endif

    const int32_t MYRAND_MAX = 0xffff;
    int32_t myrand()
    {
        // feedback polynomial: x^16 + x^14 + x^13 + x^11 + 1
        static uint16_t val = 0xACE1u;
        uint32_t bit = ((val >> 0) ^ (val >> 2) ^ (val >> 3) ^ (val >> 5)) & 1;
        val = (val >> 1) | (bit << 15);
        return val;
    }

    const CostType tab_defaultLambdaCorrIP[8]   = {1.0, 1.06, 1.09, 1.13, 1.17, 1.25, 1.27, 1.28};

    void ConvertToInternalParam(AV1VideoParam &intParam, const mfxVideoParam &mfxParam)
    {
        const mfxInfoMFX &mfx = mfxParam.mfx;
        const mfxFrameInfo &fi = mfx.FrameInfo;
        const mfxExtOpaqueSurfaceAlloc &opaq = GetExtBuffer(mfxParam);
        const mfxExtCodingOptionAV1E &optHevc = GetExtBuffer(mfxParam);
        const mfxExtHEVCRegion &region = GetExtBuffer(mfxParam);
        const mfxExtHEVCTiles &tiles = GetExtBuffer(mfxParam);
        const mfxExtHEVCParam &hevcParam = GetExtBuffer(mfxParam);
        mfxExtDumpFiles &dumpFiles = GetExtBuffer(mfxParam);
        const mfxExtCodingOption &opt = GetExtBuffer(mfxParam);
        const mfxExtCodingOption2 &opt2 = GetExtBuffer(mfxParam);
        const mfxExtCodingOption3 &opt3 = GetExtBuffer(mfxParam);
        const mfxExtEncoderROI &roi = GetExtBuffer(mfxParam);

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
        intParam.chromaFormatIdc = fi.ChromaFormat;
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
        intParam.partModes = optHevc.PartModes;
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
        intParam.GeneralizedPB = (opt3.GPB == ON);
        intParam.AdaptiveRefs = (optHevc.AdaptiveRefs == ON);
        intParam.NumRefFrameB  = optHevc.NumRefFrameB;
        if (intParam.NumRefFrameB < 2)
            intParam.NumRefFrameB = numRefFrame;
        intParam.NumRefLayers  = optHevc.NumRefLayers;
        if (intParam.NumRefLayers < 2 || intParam.BiPyramidLayers<2)
            intParam.NumRefLayers = 2;
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
                uint16_t NumRef = std::min((int32_t)intParam.NumRefFrameB, numRefFrame);
                if (fi.PicStruct != PROGR)
                    NumRef = std::min(2*intParam.NumRefFrameB, numRefFrame);
                intParam.MaxRefIdxB[0] = (NumRef + 1) / 2;
                intParam.MaxRefIdxB[1] = std::max(1, (NumRef + 0) / 2);
            } else {
                intParam.MaxRefIdxB[0] = numRefFrame - 1;
                intParam.MaxRefIdxB[1] = 1;
            }
        }

        intParam.PGopPicSize = (intParam.GopRefDist == 1 && intParam.MaxRefIdxP[0] > 1) ? PGOP_PIC_SIZE : 1;

        intParam.AnalyseFlags = 0;
        if (optHevc.AnalyzeChroma == ON) intParam.AnalyseFlags |= HEVC_ANALYSE_CHROMA;
        if (optHevc.CostChroma == ON)    intParam.AnalyseFlags |= HEVC_COST_CHROMA;

        intParam.SplitThresholdStrengthCUIntra = (uint8_t)optHevc.SplitThresholdStrengthCUIntra - 1;
        intParam.SplitThresholdStrengthTUIntra = (uint8_t)optHevc.SplitThresholdStrengthTUIntra - 1;
        intParam.SplitThresholdStrengthCUInter = (uint8_t)optHevc.SplitThresholdStrengthCUInter - 1;
        intParam.SplitThresholdTabIndex        = (uint8_t)optHevc.SplitThresholdTabIndex;
        intParam.SplitThresholdMultiplier      = optHevc.SplitThresholdMultiplier / 10.0;

        intParam.FastInterp = (optHevc.FastInterp == ON);
        intParam.cpuFeature = optHevc.CpuFeature;
        intParam.IntraChromaRDO = (optHevc.IntraChromaRDO == ON);
        intParam.SBHFlag = (optHevc.SignBitHiding == ON);
        intParam.RDOQFlag = (optHevc.RDOQuant == ON);
        intParam.rdoqChromaFlag = (optHevc.RDOQuantChroma == ON);
        intParam.FastCoeffCost = (optHevc.FastCoeffCost == ON);
        intParam.rdoqCGZFlag = (optHevc.RDOQuantCGZ == ON);
        intParam.SAOFlag = (optHevc.SAO == ON);
        intParam.SAOChromaFlag = (optHevc.SAOChroma == ON);
        intParam.num_threads = mfx.NumThread;

        if(intParam.NumRefLayers==4) {
            intParam.refLayerLimit[0] = 0;
            intParam.refLayerLimit[1] = 1;
            intParam.refLayerLimit[2] = 1;
            intParam.refLayerLimit[3] = 2;
        } else if(intParam.NumRefLayers==3) {
            intParam.refLayerLimit[0] = 1;
            intParam.refLayerLimit[1] = 1;
            intParam.refLayerLimit[2] = 1;
            intParam.refLayerLimit[3] = 2;
        } else {
            intParam.refLayerLimit[0] = 4;
            intParam.refLayerLimit[1] = 4;
            intParam.refLayerLimit[2] = 4;
            intParam.refLayerLimit[3] = 4;
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

        intParam.LowresFactor = optHevc.LowresFactor - 1;
        intParam.FullresMetrics = 0;
        intParam.DeltaQpMode  = (opt2.MBBRC == OFF) ? 0 : optHevc.DeltaQpMode  - 1;
        if (mfx.EncodedOrder) {
            intParam.DeltaQpMode = intParam.DeltaQpMode&AMT_DQP_CAQ;
        }

        intParam.LambdaCorrection = tab_defaultLambdaCorrIP[(intParam.DeltaQpMode&AMT_DQP_CAQ)?mfx.TargetUsage:0];

        intParam.SceneCut = mfx.EncodedOrder ? 0 : 1;
        intParam.AdaptiveI = (!intParam.SceneCut) ? 0 : (opt2.AdaptiveI == ON);
        intParam.AnalyzeCmplx = /*mfx.EncodedOrder ? 0 : */optHevc.AnalyzeCmplx - 1;
        if(intParam.AnalyzeCmplx && intParam.BiPyramidLayers > 1 && intParam.LowresFactor && mfx.RateControlMethod != CQP)
            intParam.FullresMetrics = 1;

#ifdef LOW_COMPLX_PAQ
        if (intParam.LowresFactor && (intParam.DeltaQpMode&AMT_DQP_PAQ))
            intParam.FullresMetrics = 1;
#endif

        intParam.RateControlDepth = optHevc.RateControlDepth;
        intParam.TryIntra = optHevc.TryIntra;
        intParam.FastAMPSkipME = optHevc.FastAMPSkipME;
        intParam.FastAMPRD = optHevc.FastAMPRD;
        intParam.SkipMotionPartition = optHevc.SkipMotionPartition;
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
        intParam.cdefFlag = (optHevc.CDEF == ON);
        intParam.lrFlag = (optHevc.LRMode == ON);
        intParam.deblockingFlag = (optHevc.Deblocking == ON);
        intParam.deblockingSharpness = 0;
        intParam.deblockingLevelMethod = optHevc.DeblockingLevelMethod - 1;
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

        intParam.sourceWidth = fi.CropW;
        intParam.sourceHeight = fi.CropH;
        intParam.Width = fi.CropW;
        intParam.CropLeft = 0;
        intParam.CropRight = 0;
        if (fi.PicStruct == TFF || fi.PicStruct == BFF) {
            intParam.Height = (fi.CropH / 2 + 7) & ~7;
            intParam.CropTop = 0;
            intParam.CropBottom = 0;
        } else {
            intParam.Height = fi.CropH;
            intParam.CropTop = 0;
            intParam.CropBottom = 0;
        }

        intParam.enableCmFlag = (optHevc.EnableCm == ON);
        intParam.enableCmPostProc = 0;//intParam.deblockingFlag && intParam.enableCmFlag && (intParam.bitDepthLuma == 8) && (intParam.chromaFormatIdc == MFX_CHROMAFORMAT_YUV420);
        intParam.enablePostProcFrame = intParam.deblockingFlag;
        intParam.CmBirefineFlag = (optHevc.EnableCmBiref == ON) && ((intParam.Width * intParam.Height) <= (4096 * 4096));  //GPU is out of memory for 8K !!!
        intParam.CmInterpFlag = intParam.CmBirefineFlag;

        intParam.m_framesInParallel = optHevc.FramesInParallel;

        // FIX TO SUPPORT SUPERFRAME ENCODING IN VP9
        intParam.m_framesInParallel = std::max(intParam.m_framesInParallel, 4);

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

        intParam.NumSlices = mfx.NumSlice;
        intParam.NumTileCols = tiles.NumTileColumns;
        intParam.NumTileRows = tiles.NumTileRows;
        intParam.NumTiles = tiles.NumTileColumns * tiles.NumTileRows;
        intParam.RegionIdP1 = (region.RegionEncoding == MFX_HEVC_REGION_ENCODING_ON && region.RegionType == MFX_HEVC_REGION_SLICE && intParam.NumSlices > 1) ? region.RegionId + 1 : 0;
        intParam.WPPFlag = (optHevc.WPP == ON);

        intParam.num_thread_structs = (intParam.WPPFlag) ? intParam.num_threads : MIN((uint32_t)MAX(intParam.NumSlices, intParam.NumTiles) * 2 * intParam.m_framesInParallel, intParam.num_threads);
        intParam.num_bs_subsets = (intParam.WPPFlag) ? intParam.PicHeightInCtbs : MAX(intParam.NumSlices, intParam.NumTiles);

        // deltaQP control
        intParam.MaxCuDQPDepth = 0;
        intParam.m_maxDeltaQP = 0;
        intParam.UseDQP = 0;

        intParam.numRoi = roi.NumROI;

        if (intParam.numRoi > 0) {
            int32_t maxAbsDeltaQp = -1;
            if (mfx.RateControlMethod == CBR)
                maxAbsDeltaQp = 1;
            else if (mfx.RateControlMethod == VBR)
                maxAbsDeltaQp = 2;
            else if (mfx.RateControlMethod != CQP)
                maxAbsDeltaQp = 3;
            for (int32_t i = 0; i < intParam.numRoi; i++) {
                intParam.roi[i].left = roi.ROI[i].Left;
                intParam.roi[i].top = roi.ROI[i].Top;
                intParam.roi[i].right = roi.ROI[i].Right;
                intParam.roi[i].bottom = roi.ROI[i].Bottom;
                intParam.roi[i].priority = roi.ROI[i].Priority;
                if (maxAbsDeltaQp > 0)
                    intParam.roi[i].priority = Saturate(-maxAbsDeltaQp, maxAbsDeltaQp, intParam.roi[i].priority);
            }
            intParam.DeltaQpMode = 0;
        }

        if (intParam.DeltaQpMode) {
            intParam.MaxCuDQPDepth = 0;
            intParam.m_maxDeltaQP = 0;
            intParam.UseDQP = 1;
        }
        if (intParam.MaxCuDQPDepth > 0 || intParam.m_maxDeltaQP > 0 || intParam.numRoi > 0)
            intParam.UseDQP = 1;

        if (intParam.UseDQP)
            intParam.MinCuDQPSize = intParam.MaxCUSize >> intParam.MaxCuDQPDepth;
        else
            intParam.MinCuDQPSize = intParam.MaxCUSize;

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
        }

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

        intParam.seqParams.frameIdNumbersPresentFlag = 1;
        intParam.seqParams.deltaFrameIdLength = 7;
        intParam.seqParams.additionalFrameIdLength = 1;
        intParam.seqParams.idLen = 8;
        intParam.seqParams.orderHintBits = 5;   // should be enough for all supported types of GOP
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
        intParam.switchInterpFilter = (optHevc.InterpFilter == ON);
        intParam.switchInterpFilterRefine = (optHevc.InterpFilterRefine == ON);
        intParam.targetUsage = mfxParam.mfx.TargetUsage;

        SetupTileParamAv1(&intParam, tiles.NumTileRows, tiles.NumTileColumns, 1);

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
            float avgDQp = std::accumulate(frame.m_stats[0]->qp_mask.begin(), frame.m_stats[0]->qp_mask.end(), 0);
            float numBlks = (MAX_DQP / 2)*frame.m_stats[0]->qp_mask.size();
            float mult = sliceQp - qindexMinus1[qindexMinus1[sliceQp]];
            sliceQpPAQ = sliceQp + (uint8_t) ((avgDQp / numBlks) * mult - 0.5f);
            Saturate(MINQ, MAXQ, sliceQpPAQ);
        }
        return sliceQpPAQ;
    }

    uint8_t GetConstQp(const Frame &frame, const AV1VideoParam &param, int32_t repackCounter)
    {
        int32_t sliceQpY;
        if (frame.m_sliceQpY > 0 && frame.m_sliceQpY <= MAXQ)
            sliceQpY = frame.m_sliceQpY;
        else {
            switch (frame.m_picCodeType)
            {
            case MFX_FRAMETYPE_I:
                sliceQpY = param.QPI;
                break;
            case MFX_FRAMETYPE_P:
                if (param.BiPyramidLayers > 1 && param.longGop && frame.m_biFramesInMiniGop == 15)
                    sliceQpY = param.QPI;
                else if (param.PGopPicSize > 1)
                    sliceQpY = param.QPP + frame.m_pyramidLayer * (param.QPP - param.QPI);
                else
                    sliceQpY = param.QPP;
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
        return Saturate(MINQ, MAXQ, sliceQpY);
    }


    uint8_t GetRateQp(const Frame &in, const AV1VideoParam &param, BrcIface *brc)
    {
        if (brc == NULL)
            return 51;

        Frame *frames[128] = {(Frame*)&in};
        int32_t framesCount = MIN(127, (int32_t)in.m_futureFrames.size());

        for (int32_t frmIdx = 0; frmIdx < framesCount; frmIdx++)
            frames[frmIdx+1] = in.m_futureFrames[frmIdx];

        return brc->GetQP(const_cast<AV1VideoParam *>(&param), frames, framesCount+1);
    }

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
            assert(feiLumaPitch >= UMC::align_value<int32_t>(UMC::align_value<int32_t>(paddingLu*bpp, 64) + widthLu*bpp + paddingLu*bpp, 64 ));
            assert(feiChromaPitch >= UMC::align_value<int32_t>(UMC::align_value<int32_t>(allocInfo.paddingChW*bpp, 64) + widthCh*bpp + allocInfo.paddingChW*bpp, 64 ));
            assert((feiLumaPitch & 63) == 0);
            assert((feiChromaPitch & 63) == 0);
            allocInfo.alignment = 0x1000;
            allocInfo.pitchInBytesLu = feiLumaPitch;
            allocInfo.pitchInBytesCh = feiChromaPitch;
            allocInfo.sizeInBytesLu = allocInfo.pitchInBytesLu * (heightLu + paddingLu * 2);
            allocInfo.sizeInBytesCh = allocInfo.pitchInBytesCh * (heightCh + allocInfo.paddingChH * 2);
        } else {
            allocInfo.alignment = 64;
            allocInfo.pitchInBytesLu = UMC::align_value<int32_t>(UMC::align_value<int32_t>(paddingLu*bpp, 64) + widthLu*bpp + paddingLu*bpp, 64 );
            allocInfo.pitchInBytesCh = UMC::align_value<int32_t>(UMC::align_value<int32_t>(allocInfo.paddingChW*bpp, 64) + widthCh*bpp + allocInfo.paddingChW*bpp, 64 );
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
        volatile uint32_t   m_taskStage;
        volatile uint32_t   m_taskReencode;     // BRC repack

#if TASK_LOG_ENABLE
        uint32_t task_log_idx;
#endif
    };
};


AV1Encoder::AV1Encoder(MFXCoreInterface1 &core)
    : m_core(core)
    , m_nextFrameToEncode(0)
    , m_lastShowFrame(0)
    , m_prevFrame(NULL)
    , m_memBuf(NULL)
    , m_brc(NULL)
    , m_fei(NULL)
{
    ippStaticInit();
    Zero(m_stat);
    Zero(m_nextFrameDpbVP9);
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

    delete m_brc;

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
    m_lfFrameDataPool.Destroy();
    m_feiInputDataPool.Destroy();
    m_feiReconDataPool.Destroy();

    // destory FEI resources
    for (int32_t i = 0; i < 4; i++)
        m_feiInterDataPool[i].Destroy();
    m_feiModeInfoPool.Destroy();
    m_feiVarTxInfoPool.Destroy();
    m_feiRsCs8Pool.Destroy();
    m_feiRsCs16Pool.Destroy();
    m_feiRsCs32Pool.Destroy();
    m_feiRsCs64Pool.Destroy();
    if (m_fei)
        H265FEI_Close(m_fei);

    m_la.reset(0);

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

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
        m_brc = CreateBrc(&par);
        if ((sts = m_brc->Init(&par, m_videoParam)) != MFX_ERR_NONE)
            return sts;
    }

    if ((sts = InitInternal()) != MFX_ERR_NONE)
        return sts;

    m_spsBufSize = 0;
    WriteIvfHeader(m_spsBuf, m_spsBufSize, m_videoParam);
    m_spsBufSize >>= 3;

    // memsize calculation
    int32_t sizeofAV1CU = (m_videoParam.bitDepthLuma > 8 ? sizeof(AV1CU<uint16_t>) : sizeof(AV1CU<uint8_t>));
    uint32_t numCtbs = m_videoParam.PicWidthInCtbs*m_videoParam.PicHeightInCtbs;
    int32_t memSize = 0;
    // cu
    memSize = sizeofAV1CU * m_videoParam.num_thread_structs + DATA_ALIGN;
    // data_temp
    int32_t data_temp_size = ((MAX_TOTAL_DEPTH * 2 + 2) << m_videoParam.Log2NumPartInCU);
    memSize += sizeof(ModeInfo) * data_temp_size * m_videoParam.num_thread_structs + DATA_ALIGN;//for ModeDecision try different cu
    // coeffs Work
    //memSize += sizeof(CoeffsType) * (numCtbs << (m_videoParam.Log2MaxCUSize << 1)) * 6 / (2 + m_videoParam.chromaShift) + DATA_ALIGN;

    // allocation
    m_memBuf = (uint8_t *)AV1_Malloc(memSize);
    MFX_CHECK_STS_ALLOC(m_memBuf);
    ippsZero_8u(m_memBuf, memSize);

    // mapping
    uint8_t *ptr = m_memBuf;
    m_cu = UMC::align_pointer<void*>(ptr, DATA_ALIGN);
    ptr += sizeofAV1CU * m_videoParam.num_thread_structs + DATA_ALIGN;
    //m_cu = UMC::align_pointer<void*>(ptr, 0x1000);
    //ptr += sizeofAV1CU * m_videoParam.num_thread_structs + 0x1000;

    // data_temp
    data_temp = UMC::align_pointer<ModeInfo *>(ptr, DATA_ALIGN);
    ptr += sizeof(ModeInfo) * data_temp_size * m_videoParam.num_thread_structs + DATA_ALIGN;

    InitQuantizer(m_videoParam);

    // AllocFrameEncoders()
    m_frameEncoder.resize(m_videoParam.m_framesInParallel);
    for (size_t encIdx = 0; encIdx < m_frameEncoder.size(); encIdx++) {
        m_frameEncoder[encIdx] = new AV1FrameEncoder(*this);
        m_frameEncoder[encIdx]->Init();
    }

    m_la.reset(new Lookahead(*this));

    FrameData::AllocInfo frameDataAllocInfo;
    FrameData::AllocInfo frameDataAllocInfoSrc10;

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

        mfxExtFEIH265Alloc feiAlloc = {};
        MFXCoreInterface core(m_core.m_core);
        mfxStatus sts = H265FEI_GetSurfaceDimensions_new(&core, &feiParams, &feiAlloc);
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

        FeiBufferUp::AllocInfo feiVarTxInfoAllocInfo;
        feiVarTxInfoAllocInfo.feiHdl = m_fei;
        feiVarTxInfoAllocInfo.alignment = 0x1000;
        feiVarTxInfoAllocInfo.size = m_videoParam.sb64Cols * m_videoParam.sb64Rows * 16 * 16 * sizeof(Ipp16u);
        m_feiVarTxInfoPool.Init(feiVarTxInfoAllocInfo, 0);

        feiSurfaceUpAllocInfo.feiHdl = m_fei;
        feiSurfaceUpAllocInfo.allocInfo = feiAlloc.RsCs64Info;
        m_feiRsCs64Pool.Init(feiSurfaceUpAllocInfo, 0);

        feiSurfaceUpAllocInfo.feiHdl = m_fei;
        feiSurfaceUpAllocInfo.allocInfo = feiAlloc.RsCs32Info;
        m_feiRsCs32Pool.Init(feiSurfaceUpAllocInfo, 0);

        feiSurfaceUpAllocInfo.feiHdl = m_fei;
        feiSurfaceUpAllocInfo.allocInfo = feiAlloc.RsCs16Info;
        m_feiRsCs16Pool.Init(feiSurfaceUpAllocInfo, 0);

        feiSurfaceUpAllocInfo.feiHdl = m_fei;
        feiSurfaceUpAllocInfo.allocInfo = feiAlloc.RsCs8Info;
        m_feiRsCs8Pool.Init(feiSurfaceUpAllocInfo, 0);
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
    m_inputFrameDataPool.Init(frameDataAllocInfo, 0);
    if (m_videoParam.src10Enable) {
        m_input10FrameDataPool.Init(frameDataAllocInfoSrc10, 0);
    }
    m_lfFrameDataPool.Init(frameDataAllocInfo, 0);

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
    m_statsPool.Init(statsAllocInfo, 0);

    if (m_la.get()) {
        if (m_videoParam.LowresFactor) {
            Statistics::AllocInfo statsAllocInfo;
            statsAllocInfo.width = m_videoParam.Width >> m_videoParam.LowresFactor;
            statsAllocInfo.height = m_videoParam.Height >> m_videoParam.LowresFactor;
            m_statsLowresPool.Init(statsAllocInfo, 0);
        }
        if (m_videoParam.SceneCut) {
            SceneStats::AllocInfo statsAllocInfo;
            // it doesn't make width/height here
            m_sceneStatsPool.Init(statsAllocInfo, 0);
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
#if VT_TRACE
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
    uint32_t memSize = 0;

    m_profileIndex = 0;
    m_frameOrder = 0;

    m_frameOrderOfLastIdr = 0;              // frame order of last IDR frame (in display order)
    m_frameOrderOfLastIntra = 0;            // frame order of last I-frame (in display order)
    m_frameOrderOfLastIntraInEncOrder = 0;  // frame order of last I-frame (in encoding order)
    m_frameOrderOfLastAnchor = 0;           // frame order of last anchor (first in minigop) frame (in display order)
    m_frameOrderOfLastIdrB = 0;
    m_frameOrderOfLastIntraB = 0;
    m_frameOrderOfLastAnchorB  = 0;
    m_LastbiFramesInMiniGop  = 0;
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
    int32_t biPyramid = param.BiPyramidLayers > 1;

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
        m_inputQueue.splice(m_inputQueue.end(), m_free, m_free.begin());
        inputFrame = m_inputQueue.back();
        inputFrame->AddRef();

        if (ctrl && ctrl->Payload && ctrl->NumPayload > 0) {
            inputFrame->m_userSeiMessages.resize(ctrl->NumPayload);
            size_t totalSize = 0;
            for (int32_t i = 0; i < ctrl->NumPayload; i++) {
                inputFrame->m_userSeiMessages[i] = *ctrl->Payload[i];
                inputFrame->m_userSeiMessages[i].BufSize = (inputFrame->m_userSeiMessages[i].NumBit + 7) / 8;
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
                    inputFrame->m_sliceQpY = ctrl->QP;
            }

            if (!(inputFrame->m_picCodeType & MFX_FRAMETYPE_B)) {
                m_frameOrderOfLastIdr = m_frameOrderOfLastIdrB;
                m_frameOrderOfLastIntra = m_frameOrderOfLastIntraB;
                m_frameOrderOfLastAnchor = m_frameOrderOfLastAnchorB;
            }
        } else if (/*displayOrder && */ctrl && ((ctrl->FrameType&MFX_FRAMETYPE_IDR) || (ctrl->FrameType&MFX_FRAMETYPE_I))) {
            inputFrame->m_picCodeType = ctrl->FrameType;
            if (inputFrame->m_picCodeType & MFX_FRAMETYPE_IDR) {
                inputFrame->m_picCodeType = MFX_FRAMETYPE_I;
                inputFrame->m_isIdrPic = true;
                inputFrame->m_poc = 0;
                inputFrame->m_isRef = 1;
            } else if (inputFrame->m_picCodeType & MFX_FRAMETYPE_REF) {
                inputFrame->m_picCodeType &= ~MFX_FRAMETYPE_REF;
                inputFrame->m_isRef = 1;
            }
            if (ctrl->QP > 0 && ctrl->QP < 52)
                inputFrame->m_sliceQpY = ctrl->QP;
        }

        bool bEncCtrl = m_videoParam.encodedOrder ||
                       !m_videoParam.encodedOrder && ctrl && ((ctrl->FrameType&MFX_FRAMETYPE_IDR) || (ctrl->FrameType&MFX_FRAMETYPE_I));

        ConfigureInputFrame(inputFrame, !!m_videoParam.encodedOrder, bEncCtrl);
        UpdateGopCounters(inputFrame, !!m_videoParam.encodedOrder);
    } else {
        // tmp fix for short sequences
        if (m_la.get()) {
            if (m_videoParam.AnalyzeCmplx)
                for (std::list<Frame *>::iterator i = m_inputQueue.begin(); i != m_inputQueue.end(); ++i)
                    AverageComplexity(*i, m_videoParam);
#if 0
            if (m_videoParam.DeltaQpMode & (AMT_DQP_PAQ | AMT_DQP_CAL)) {
                while (m_la.get()->BuildQpMap_AndTriggerState(NULL));
            }
#endif
            m_lookaheadQueue.splice(m_lookaheadQueue.end(), m_inputQueue);
            m_la->ResetState();
        }
    }

    FrameList & inputQueue = m_la.get() ? m_lookaheadQueue : m_inputQueue;
    // reorder as many frames as possible
    while (!inputQueue.empty()) {
        size_t reorderedSize = m_reorderedQueue.size();

        if (m_videoParam.encodedOrder) {
            if (!inputQueue.empty())
                m_reorderedQueue.splice(m_reorderedQueue.end(), inputQueue, inputQueue.begin());
        } else {
            if (m_videoParam.picStruct == PROGR)
                ReorderFrames(inputQueue, m_reorderedQueue, m_videoParam, surface == NULL);
            else
                ReorderFields(inputQueue, m_reorderedQueue, m_videoParam, surface == NULL);
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
        && ((int32_t)m_reorderedQueue.size() >= m_videoParam.RateControlDepth - 1 || surface == NULL)
        && (int32_t)(m_encodeQueue.size() + m_outputQueue.size()) < m_videoParam.m_framesInParallel) {

        // assign resources for encoding
        m_reorderedQueue.front()->m_recon = m_reconFrameDataPool.Allocate();
        m_reorderedQueue.front()->m_feiRecon = m_feiReconDataPool.Allocate();
        m_reorderedQueue.front()->m_feiOrigin = m_feiInputDataPool.Allocate();
        m_reorderedQueue.front()->m_feiModeInfo1 = m_feiModeInfoPool.Allocate();
        m_reorderedQueue.front()->m_feiModeInfo2 = m_feiModeInfoPool.Allocate();
        m_reorderedQueue.front()->m_feiVarTxInfo = m_feiVarTxInfoPool.Allocate();
        m_reorderedQueue.front()->m_feiRs8 = m_feiRsCs8Pool.Allocate();
        m_reorderedQueue.front()->m_feiCs8 = m_feiRsCs8Pool.Allocate();
        m_reorderedQueue.front()->m_feiRs16 = m_feiRsCs16Pool.Allocate();
        m_reorderedQueue.front()->m_feiCs16 = m_feiRsCs16Pool.Allocate();
        m_reorderedQueue.front()->m_feiRs32 = m_feiRsCs32Pool.Allocate();
        m_reorderedQueue.front()->m_feiCs32 = m_feiRsCs32Pool.Allocate();
        m_reorderedQueue.front()->m_feiRs64 = m_feiRsCs64Pool.Allocate();
        m_reorderedQueue.front()->m_feiCs64 = m_feiRsCs64Pool.Allocate();
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

    return inputFrame;
}

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


    if (m_videoParam.enableCmFlag) {
        frame->m_ttSubmitGpuCopySrc.CommonInit(TT_GPU_SUBMIT);
        frame->m_ttSubmitGpuCopySrc.numUpstreamDependencies = 1;  // guard, prevent task from running
        frame->m_ttSubmitGpuCopySrc.poc = frame->m_frameOrder;
        AddTaskDependencyThreaded(&frame->m_ttSubmitGpuCopySrc, &frame->m_ttInitNewFrame); // GPU_SUBMIT_COPY_SRC <- INIT_NEW_FRAME

        for (int32_t i = LAST_FRAME; i <= ALTREF_FRAME; i++) {
            if (frame->isUniq[i]) {
                for (int32_t blksize = 0; blksize < 4; blksize++)
                    if (!frame->m_feiInterData[i][blksize])
                        frame->m_feiInterData[i][blksize] = m_feiInterDataPool[blksize].Allocate();

                frame->m_ttSubmitGpuMe[i].CommonInit(TT_GPU_SUBMIT);
                frame->m_ttSubmitGpuMe[i].poc = frame->m_frameOrder;
                frame->m_ttSubmitGpuMe[i].refIdx = i;

                AddTaskDependencyThreaded(&frame->m_ttSubmitGpuMe[i], &frame->m_ttSubmitGpuCopySrc); // GPU_SUBMIT_HME <- GPU_SUBMIT_COPY_SRC
            }
        }
        if (!frame->IsIntra()) {
#if PROTOTYPE_GPU_MODE_DECISION
            frame->m_ttSubmitGpuMd.CommonInit(TT_GPU_SUBMIT);
            frame->m_ttSubmitGpuMd.poc = frame->m_frameOrder;
            frame->m_ttWaitGpuMd.CommonInit(TT_GPU_WAIT);
            frame->m_ttWaitGpuMd.poc = frame->m_frameOrder;
            frame->m_ttWaitGpuMd.syncpoint = NULL;
            AddTaskDependencyThreaded(&frame->m_ttWaitGpuMd, &frame->m_ttSubmitGpuMd); // GPU_WAIT_MD <- GPU_SUBMIT_MD
#else // PROTOTYPE_GPU_MODE_DECISION
            frame->m_ttWaitGpuMePu.CommonInit(TT_GPU_WAIT);
            frame->m_ttWaitGpuMePu.poc = frame->m_frameOrder;
            frame->m_ttWaitGpuMePu.syncpoint = NULL;
            const int32_t secondRef = ALTREF_FRAME;
            AddTaskDependencyThreaded(&frame->m_ttWaitGpuMePu, &frame->m_ttSubmitGpuMePu); // GPU_WAIT_MEPU <- GPU_SUBMIT_MEPU
#endif // PROTOTYPE_GPU_MODE_DECISION

            AddTaskDependency(&frame->m_ttSubmitGpuMd, &frame->m_ttSubmitGpuMe[LAST_FRAME]);
            if (frame->compoundReferenceAllowed)
                AddTaskDependency(&frame->m_ttSubmitGpuMd, &frame->m_ttSubmitGpuMe[ALTREF_FRAME]);
            else if (frame->isUniq[GOLDEN_FRAME])
                AddTaskDependency(&frame->m_ttSubmitGpuMd, &frame->m_ttSubmitGpuMe[GOLDEN_FRAME]);
        }

        if (frame->m_isRef) {
            frame->m_ttSubmitGpuCopyRec.CommonInit(TT_GPU_SUBMIT);
            frame->m_ttSubmitGpuCopyRec.poc = frame->m_frameOrder;
            frame->m_ttWaitGpuCopyRec.CommonInit(TT_GPU_WAIT);
            frame->m_ttWaitGpuCopyRec.poc = frame->m_frameOrder;
            frame->m_ttWaitGpuCopyRec.syncpoint = NULL;

            AddTaskDependencyThreaded(&frame->m_ttWaitGpuCopyRec, &frame->m_ttSubmitGpuCopyRec); // GPU_WAIT_COPY_REC <- GPU_SUBMIT_COPY_REC
        }
    }


    frame->m_sliceQpY = (m_brc)
        ? GetRateQp(*frame, m_videoParam, m_brc)
        : GetConstQp(*frame, m_videoParam, inputParam->m_taskReencode);

    frame->m_sliceQpY = GetPAQOffset(*frame, m_videoParam, frame->m_sliceQpY);

    EstimateBits(frame->bitCount, frame->IsIntra(), m_videoParam.codecType);
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

#if USE_CMODEL_PAK
    if (m_videoParam.deblockingFlag) {
        // deblocking
        LoopFilterFrameParam *params = &frame->m_loopFilterParam;
        LoopFilterResetParams(params);

        params->sharpness = m_videoParam.deblockingSharpness;
        params->level = FilterLevelFromQp(frame->m_sliceQpY, frame->IsIntra());
    }
    else {
        Zero(frame->m_loopFilterParam);
    }
    //----------------------------------------------------------------
#endif
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
    frame->m_ttEncComplete.CommonInit(TT_ENC_COMPLETE);
    frame->m_ttEncComplete.numUpstreamDependencies = 1; // decremented when frame becomes a target frame
    frame->m_ttEncComplete.poc = frame->m_frameOrder;

    frame->m_fenc->SetEncodeFrame(frame, &m_pendingTasks);

    if (!m_outputQueue.empty())
        AddTaskDependencyThreaded(&frame->m_ttPackHeader, &m_outputQueue.back()->m_ttPackHeader);

    if (m_videoParam.enableCmFlag) {
        // the following modifications should be guarded by m_feiCritSect
        // because finished, numDownstreamDependencies and downstreamDependencies[]
        // may be accessed from FeiThreadRoutine()
        std::lock_guard<std::mutex> guard(m_feiCritSect);
        for (int32_t i = LAST_FRAME; i <= ALTREF_FRAME; i++) {
            if (frame->isUniq[i]) {
                Frame *ref = frame->refFramesVp9[i];
                AddTaskDependencyThreaded(&frame->m_ttSubmitGpuMe[i], &ref->m_ttSubmitGpuCopyRec); // GPU_SUBMIT_HME <- GPU_COPY_REF
            }
        }
        if (vm_interlocked_dec32(&frame->m_ttSubmitGpuCopySrc.numUpstreamDependencies) == 0) {
            m_feiSubmitTasks.push_back(&frame->m_ttSubmitGpuCopySrc); // GPU_COPY_SRC is independent
            m_feiCondVar.notify_one();
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
        buffering += m_videoParam.GopRefDist - 1;
        buffering += m_videoParam.m_framesInParallel - 1;
        buffering += 1; // RsCs
        if (m_videoParam.SceneCut || m_videoParam.AnalyzeCmplx || m_videoParam.DeltaQpMode) {
            if (m_videoParam.SceneCut)     buffering += /*10 +*/ 1 + 1;
            if (m_videoParam.AnalyzeCmplx) buffering += m_videoParam.RateControlDepth;
            if (m_videoParam.DeltaQpMode) {
                if      (m_videoParam.DeltaQpMode&(AMT_DQP_PAQ|AMT_DQP_CAL)) buffering += 2 * m_videoParam.GopRefDist + 1;
                else if (m_videoParam.DeltaQpMode&(AMT_DQP_CAQ))             buffering += 1;
            }
        }
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
    m_pTaskInputParams->m_taskStage = THREADING_ITASK_INI;
    m_pTaskInputParams->m_taskReencode = 0;
    m_pTaskInputParams->m_newFrame[0] = NULL;
    m_pTaskInputParams->m_newFrame[1] = NULL;

    entryPoint.pRoutine = TaskRoutine;
    entryPoint.pCompleteProc = TaskCompleteProc;
    entryPoint.pState = this;
    entryPoint.requiredNumThreads = m_videoParam.num_threads;
    entryPoint.pRoutineName = "EncodeHEVC";
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
    int32_t bs_main_id = m_videoParam.num_bs_subsets;
    uint32_t dataLength0 = mfxBs->DataLength;
    int32_t overheadBytes0 = overheadBytes;

    overheadBytes = frameEnc->GetOutputData(*bs);

    // BRC
    if (m_brc || m_videoParam.randomRepackThreshold) {
        std::unique_lock<std::mutex> guard(m_critSect);
        // THREADING_WORKING -> THREADING_PAUSE
        assert(m_doStage == THREADING_WORKING);
        m_doStage = THREADING_PAUSE;
        guard.unlock();

        const int32_t min_qp = 1;
        int32_t frameBytes = bs->DataLength - initialDataLength;
        mfxBRCStatus brcSts = m_brc ? m_brc->PostPackFrame(&m_videoParam, frame->m_sliceQpY, frame, frameBytes << 3, overheadBytes << 3, inputParam->m_taskReencode ? 1 : 0)
                                    : (myrand() < m_videoParam.randomRepackThreshold ? MFX_BRC_ERR_BIG_FRAME : MFX_BRC_OK);

        if (brcSts != MFX_BRC_OK ) {
            if ((brcSts & MFX_BRC_ERR_SMALL_FRAME) && (frame->m_sliceQpY < min_qp))
                brcSts |= MFX_BRC_NOT_ENOUGH_BUFFER;
            if (brcSts == MFX_BRC_ERR_BIG_FRAME && frame->m_sliceQpY == MAXQ)
                brcSts |= MFX_BRC_NOT_ENOUGH_BUFFER;

            if (!(brcSts & MFX_BRC_NOT_ENOUGH_BUFFER)) {
                bs->DataLength = dataLength0;
                overheadBytes = overheadBytes0;

                while (m_threadingTaskRunning > 1) std::this_thread::yield();

                if (m_videoParam.enableCmFlag) {
                    m_feiCritSect.lock();
                    m_pauseFeiThread = 1;
                    m_feiCritSect.unlock();
                    while (m_feiThreadRunning) std::this_thread::yield();
                    m_feiSubmitTasks.resize(0);
                    for (std::deque<ThreadingTask *>::iterator i = m_feiWaitTasks.begin(); i != m_feiWaitTasks.end(); ++i) {
                        H265FEI_SyncOperation(m_fei, (*i)->syncpoint, 0xFFFFFFFF);
                        H265FEI_DestroySavedSyncPoint(m_fei, (*i)->syncpoint);
                    }
                    m_feiWaitTasks.resize(0);
                }

                m_pendingTasks.resize(0);
                m_encodeQueue.splice(m_encodeQueue.begin(), m_outputQueue);
                m_ttHubPool.ReleaseAll();

                PrepareToEncode(inputParam);

                if (m_videoParam.enableCmFlag) {
                    m_feiCritSect.lock();
                    m_pauseFeiThread = 0;
                    m_feiCritSect.unlock();
                    m_feiCondVar.notify_one();
                }

                guard.lock();
                vm_interlocked_inc32(&inputParam->m_taskReencode);
                vm_interlocked_inc32(&m_reencode);
                assert(m_threadingTaskRunning == 1);
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

                m_brc->PostPackFrame(&m_videoParam,  frame->m_sliceQpY, frame, bitsize, (overheadBytes << 3) + bitsize - (frameBytes << 3), 1);
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
    OnExitHelperRoutine(volatile uint32_t * arg) : m_arg(arg)
    {}
    ~OnExitHelperRoutine()
    {
        if (m_arg) {
            vm_interlocked_dec32(reinterpret_cast<volatile uint32_t *>(m_arg));
        }
    }

private:
    volatile uint32_t * m_arg;
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
        if (m_laFrame[0])
            m_laFrame[0]->m_ttLookahead.back().finished = 0;
        if (m_videoParam.picStruct != PROGR) {
            if (m_laFrame[0])
                m_laFrame[0]->setWasLAProcessed();
            m_laFrame[1] = FindFrameForLookaheadProcessing(m_inputQueue);
            if (m_laFrame[0])
                m_laFrame[0]->unsetWasLAProcessed();
        }
    }
}

void AV1Encoder::PrepareToEncode(AV1EncodeTaskInputParams *inputParam)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "PrepareToEncode");

    inputParam->m_ttComplete.InitComplete(-1);
    inputParam->m_ttComplete.numUpstreamDependencies = 1;

    if (inputParam->m_newFrame[0] && !inputParam->m_newFrame[0]->m_ttInitNewFrame.finished) {
        inputParam->m_newFrame[0]->m_ttInitNewFrame.InitNewFrame(inputParam->m_newFrame[0], inputParam->surface, inputParam->m_newFrame[0]->m_frameOrder);
        AddTaskDependencyThreaded(&inputParam->m_ttComplete, &inputParam->m_newFrame[0]->m_ttInitNewFrame); // COMPLETE <- INIT_NEW_FRAME
    }

    bool isLookaheadConfigured = false;

    if (m_laFrame[0] && !m_laFrame[0]->m_ttLookahead.back().finished) {
        if (m_la->ConfigureLookaheadFrame(m_laFrame[0], 0) == 1) {
            isLookaheadConfigured = true;
            AddTaskDependencyThreaded(&inputParam->m_ttComplete, &m_laFrame[0]->m_ttLookahead.back()); // COMPLETE <- TT_PREENC_END
            AddTaskDependencyThreaded(&m_laFrame[0]->m_ttLookahead.front(), &m_laFrame[0]->m_ttInitNewFrame); // TT_PREENC_START <- INIT_NEW_FRAME
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
        } else {
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
        }

        m_nextFrameToEncode++;

    } else {
        assert(inputParam->m_targetFrame[0] == NULL);
    }
    {
        std::lock_guard<std::mutex> guard(m_critSect);
        Frame *tframe = inputParam->m_targetFrame[0];
        if (m_videoParam.enableCmFlag && tframe && tframe->m_isRef)
            AddTaskDependencyThreaded(&inputParam->m_ttComplete, &tframe->m_ttWaitGpuCopyRec); // COMPLETE <- GPU_WAIT_COPY_REC

        if (inputParam->m_newFrame[0] && !inputParam->m_newFrame[0]->m_ttInitNewFrame.finished && inputParam->m_newFrame[0]->m_ttInitNewFrame.numUpstreamDependencies == 0)
            m_pendingTasks.push_back(&inputParam->m_newFrame[0]->m_ttInitNewFrame); // INIT_NEW_FRAME is independent

        if (inputParam->bs && tframe->showExistingFrame == 0) {
            for (int32_t i = 0; i < (int32_t)tframe->m_hiddenFrames.size(); i++)
                if (vm_interlocked_dec32(&tframe->m_hiddenFrames[i]->m_ttEncComplete.numUpstreamDependencies) == 0) // unleash hidden frames since they will never become target frames
                    m_pendingTasks.push_front(&tframe->m_hiddenFrames[i]->m_ttEncComplete);
            if (vm_interlocked_dec32(&tframe->m_ttEncComplete.numUpstreamDependencies) == 0)
                m_pendingTasks.push_front(&tframe->m_ttEncComplete); // targetFrame.ENC_COMPLETE's dependencies are already resolved
        }

        if (inputParam->bs && tframe->showExistingFrame == 1
            && vm_interlocked_dec32(&tframe->m_ttFrameRepetition.numUpstreamDependencies) == 0)
            m_pendingTasks.push_front(&tframe->m_ttFrameRepetition); // targetFrame.REPEAT's dependencies are already resolved

        if (isLookaheadConfigured && vm_interlocked_dec32(&m_laFrame[0]->m_ttLookahead.front().numUpstreamDependencies) == 0)
            m_pendingTasks.push_back(&m_laFrame[0]->m_ttLookahead.front()); // LA_START is independent

        if (vm_interlocked_dec32(&inputParam->m_ttComplete.numUpstreamDependencies) == 0)
            m_pendingTasks.push_front(&inputParam->m_ttComplete); // COMPLETE's dependencies are already resolved
    }
}

static void SetTaskFinishedThreaded(ThreadingTask *task) {
    while(vm_interlocked_cas32(&(task->finished), 1, 0)) {
        // extremely rare case, use spinlock
        do {
            std::this_thread::yield();
        } while(task->finished);
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
    int32_t taskStage = vm_interlocked_cas32(&inputParam->m_taskStage, THREADING_ITASK_WORKING, THREADING_ITASK_INI);

    if (THREADING_ITASK_INI == taskStage) {
        std::unique_lock<std::mutex> guard(th->m_prepCritSect);
        if (inputParam->m_taskID != th->m_taskEncodeCount) {
            // THREADING_ITASK_WORKING -> THREADING_ITASK_INI
            int32_t taskStage = vm_interlocked_cas32(&inputParam->m_taskStage, THREADING_ITASK_INI, THREADING_ITASK_WORKING);
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

        int32_t numPendingTasks = th->m_pendingTasks.size();

        th->m_critSect.unlock();
        guard.unlock();

        while (numPendingTasks--)
            th->m_condVar.notify_one();
    }

    if (inputParam->m_taskID > th->m_taskEncodeCount)
        return MFX_TASK_BUSY;

    // global thread count control
    uint32_t newThreadCount = vm_interlocked_inc32(&th->m_threadCount);
    OnExitHelperRoutine onExitHelper(&th->m_threadCount);
    if (newThreadCount > th->m_videoParam.num_threads)
        return MFX_TASK_BUSY;

#if TT_TRACE
    char mainname[256] = "TaskRoutine ";
    if (inputParam->m_targetFrame[0])
        strcat(mainname, NUMBERS[inputParam->m_targetFrame[0]->m_frameOrder & 0x7f]);
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, mainname);
#endif // TT_TRACE

    uint32_t reencodeCounter = 0;

    while (true) {
        std::unique_lock<std::mutex> guard(th->m_critSect);
        while (true) {
            if (th->m_doStage == THREADING_WORKING && (th->m_pendingTasks.size()))
                break;
            // possible m_taskStage values here: THREADING_ITASK_INI (!), THREADING_ITASK_WORKING, THREADING_ITASK_COMPLETE
            if ((th->m_doStage != THREADING_PAUSE && inputParam->m_taskStage == THREADING_ITASK_COMPLETE) ||
                th->m_doStage == THREADING_ERROR)
                break;
            th->m_condVar.wait(guard);
        }

        if (inputParam->m_taskStage == THREADING_ITASK_COMPLETE || th->m_doStage == THREADING_ERROR) {
            break;
        }

        task = NULL;
        {
            std::deque<ThreadingTask *>::iterator t = th->m_pendingTasks.begin();
            if (th->m_videoParam.enableCmFlag) {
                if ((*t)->action == TT_ENCODE_CTU || (*t)->action == TT_DEBLOCK_AND_CDEF || (*t)->action == TT_CDEF_SYNC || (*t)->action == TT_CDEF_APPLY) {
                    Frame *targetFrame = th->m_outputQueue.front();
                    std::deque<ThreadingTask *>::iterator i = t;
                    for (std::deque<ThreadingTask *>::iterator i = ++th->m_pendingTasks.begin(); i != th->m_pendingTasks.end(); ++i) {
                        if ((*i)->action == TT_ENCODE_CTU || (*i)->action == TT_DEBLOCK_AND_CDEF || (*i)->action == TT_CDEF_SYNC || (*i)->action == TT_CDEF_APPLY) {
                            if ((*t)->frame != targetFrame && (*i)->frame == targetFrame ||
                                (*i)->frame->m_pyramidLayer  < (*t)->frame->m_pyramidLayer ||
                                (*i)->frame->m_pyramidLayer == (*t)->frame->m_pyramidLayer && (*i)->frame->m_encOrder < (*t)->frame->m_encOrder)
                                t = i;
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

        vm_interlocked_inc32(&th->m_threadingTaskRunning);
        guard.unlock();

        int32_t taskCount = 0;
        ThreadingTask *taskDepAll[MAX_NUM_DEPENDENCIES];

        int32_t distBest;

#if VT_TRACE
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
            case TT_PREENC_ROUTINE:
            case TT_PREENC_END:
                task->la->Execute(*task);
                break;
            case TT_HUB:
                break;
            case TT_ENCODE_CTU:
                assert(task->frame->m_fenc != NULL);
                assert(task->frame->m_frameOrder == task->poc);
                sts = th->m_videoParam.bitDepthLuma > 8 ?
                    task->frame->m_fenc->PerformThreadingTask<uint16_t>(task->row, task->col):
                    task->frame->m_fenc->PerformThreadingTask<uint8_t>(task->row, task->col);
                assert(sts == MFX_TASK_DONE);
                break;
            case TT_DEBLOCK_AND_CDEF:
#if !USE_CMODEL_PAK
                if (th->m_videoParam.deblockingFlag) {
                    LoopFilterSbAV1(task->frame, task->row, task->col, th->m_videoParam);
                    CdefStoreBorderSb(th->m_videoParam, task->frame, task->row, task->col);
                }
                if (th->m_videoParam.cdefFlag) {
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

                if (task->frame->m_isRef) {
#if 0
                    // pad left/above borders
                    if (task->col - 1 == 0 || task->row - 1 == 0) {
                        PadRectLumaAndChroma(*task->frame->m_recon, th->m_videoParam.fourcc, (task->col - 1) * 64, (task->row - 1) * 64, 64, 64);
                    }

                    // pad last col (above & right border)
                    if (task->col + 1 == th->m_videoParam.sb64Cols && task->row > 0) {
                        PadRectLumaAndChroma(*task->frame->m_recon, th->m_videoParam.fourcc, task->col * 64, (task->row - 1) * 64, th->m_videoParam.Width - task->col * 64, 64);
                    }

                    // pad last row (left & bottom border)
                    if (task->row + 1 == th->m_videoParam.sb64Rows && task->col > 0) {
                        PadRectLumaAndChroma(*task->frame->m_recon, th->m_videoParam.fourcc, (task->col - 1) * 64, task->row * 64, 64, th->m_videoParam.Height - task->row * 64);
                    }

                    // pad last Sb
                    if (task->row + 1 == th->m_videoParam.sb64Rows && task->col + 1 == th->m_videoParam.sb64Cols) {
                        PadRectLumaAndChroma(*task->frame->m_recon, th->m_videoParam.fourcc, task->col * 64, task->row * 64, th->m_videoParam.Width - task->col * 64, th->m_videoParam.Height - task->row * 64);
                    }
#endif
                    if (task->row + 1 == th->m_videoParam.sb64Rows && task->col + 1 == th->m_videoParam.sb64Cols) {
                        PadRectLumaAndChroma(*task->frame->m_recon, th->m_videoParam.fourcc, 0, 0, th->m_videoParam.Width, th->m_videoParam.Height);
                    }
                }
#endif // CMODEL
                break;
            case TT_CDEF_SYNC:
                if (th->m_videoParam.cdefFlag)
                    CdefSearchSync(th->m_videoParam, task->frame);
                break;
            case TT_CDEF_APPLY:
                if (th->m_videoParam.cdefFlag && (task->frame->m_isRef || th->m_videoParam.doDumpRecon))
                    CdefApplyFrame(th->m_videoParam, task->frame);

                PadRectLumaAndChroma(*task->frame->m_recon, th->m_videoParam.fourcc, 0, 0, th->m_videoParam.Width, th->m_videoParam.Height);
                break;
            case TT_PACK_TILE:
                assert(task->frame->m_fenc != NULL);
                task->frame->m_fenc->PackTile(task->tile);
                break;
            case TT_ENC_COMPLETE:
                if (task->frame->showExistingFrame == 0) {
                    th->m_prepCritSect.lock();
                    assert(th->m_inputTaskInProgress);
                    sts = th->SyncOnFrameCompletion(th->m_inputTaskInProgress, task->frame);
                    th->m_prepCritSect.unlock();
                    if (sts != MFX_TASK_DONE) {
                        vm_interlocked_dec32(&th->m_threadingTaskRunning);
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
                    if (taskInProgress->bs && taskInProgress->m_targetFrame[0]->showExistingFrame == 0) {
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
                    for (int32_t f = 0; f < (th->m_videoParam.picStruct == PROGR ? 1 : 2); f++) {
                        if (th->m_laFrame[f])
                            th->m_laFrame[f]->setWasLAProcessed();
                    }
                    th->OnLookaheadCompletion();

                    if (th->m_inputTasks.size()) {
                        th->m_inputTaskInProgress = th->m_inputTasks.front();
                        th->PrepareToEncodeNewTask(th->m_inputTaskInProgress);
                        th->PrepareToEncode(th->m_inputTaskInProgress);
                        th->m_inputTasks.pop_front();
                    } else {
                        th->m_inputTaskInProgress = NULL;
                    }

                    guard.lock();
                    // THREADING_ITASK_WORKING -> THREADING_ITASK_COMPLETE
                    vm_interlocked_cas32(&(taskInProgress->m_taskStage), THREADING_ITASK_COMPLETE, THREADING_ITASK_WORKING);
                    guard.unlock();
                    th->m_prepCritSect.unlock();
                    th->m_condVar.notify_all();

                    vm_interlocked_dec32(&th->m_threadingTaskRunning);
                    continue;
                }
                break;
            default:
                assert(0);
                break;
            }
        } catch (...) {
            // to prevent SyncOnFrameCompletion hang
            vm_interlocked_dec32(&th->m_threadingTaskRunning);
            guard.lock();
            th->m_doStage = THREADING_ERROR;
            inputParam->m_taskStage = THREADING_ITASK_COMPLETE;
            guard.unlock();
            th->m_condVar.notify_all();
            throw;
        }
#if TASK_LOG_ENABLE
        TaskLogStop(idx);
#endif
#if TT_TRACE
        MFX_AUTO_TRACE_STOP();
#endif // TT_TRACE

#if VT_TRACE
        if (tracetask) delete tracetask;
#endif

        //SetTaskFinishedThreaded(task);

        distBest = -1;

        for (int i = 0; i < task->numDownstreamDependencies; i++) {
            ThreadingTask *taskDep = task->downstreamDependencies[i];
            if (vm_interlocked_dec32(&taskDep->numUpstreamDependencies) == 0) {
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

        reencodeCounter = th->m_reencode;
        vm_interlocked_dec32(&th->m_threadingTaskRunning);
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
    assert(widthInBytesMul == 16 || widthInBytesMul == 8);
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
    FillCorner(dst - padL - pitchDst * padV, pitchDstB, padL, padV + 1, src[0]);
    dstB = (uint8_t *)dst;
    srcB = (uint8_t *)src;
    if (widthInBytesMul == 16) {
        // top border
        for (int32_t x = 0; x < widthB; x += 16)
            FillVer(dstB + x - pitchDstB * padV, pitchDstB, padV + 1, _mm_load_si128((__m128i*)(srcB+x)));
        // top right corner
        FillCorner(dst + width - pitchDst * padV, pitchDstB, padR, padV + 1, src[width - 1]);
    } else {
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
        FillHor((PixType*)dstB - padL, padL, ((PixType*)srcB)[0]);
        if (widthInBytesMul == 16) {
            // center part
            for (int32_t x = 0; x < widthB; x += 16)
                _mm_store_si128((__m128i*)(dstB+x), _mm_load_si128((__m128i*)(srcB+x)));
            // right border
            FillHor((PixType*)dstB + width, padR, ((PixType*)srcB)[width-1]);
        } else {
            // center part
            int32_t x = 0;
            for (; x < (widthB & ~15); x += 16)
                _mm_store_si128((__m128i*)(dstB+x), _mm_load_si128((__m128i*)(srcB+x)));
            *(uint64_t*)(dstB+x) = *(uint64_t*)(srcB+x);
            // right border
            FillHor8((PixType*)dstB + width, padR, ((PixType*)srcB)[width-1]);
        }
    }


    // bottom left corner
    FillCorner((PixType*)dstB - padL, pitchDstB, padL, padV + 1, ((PixType*)srcB)[0]);
    if (widthInBytesMul == 16) {
        // bottom border
        for (int32_t x = 0; x < widthB; x += 16)
            FillVer(dstB + x, pitchDstB, padV + 1, _mm_load_si128((__m128i*)(srcB + x)));
        // bottom right corner
        FillCorner((PixType*)dstB + width, pitchDstB, padR, padV + 1, ((PixType*)srcB)[width-1]);
    } else {
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
        paddingL = UMC::align_value<int32_t>(dst.padding << bppShift, 64) >> bppShift;
        paddingR = dst.pitch_luma_pix - dst.width - paddingL;
    }

    int32_t heightC = dst.height;
    if (fourcc == MFX_FOURCC_NV12 || fourcc == MFX_FOURCC_P010)
        heightC >>= 1;

    int32_t srcPitch = src.Data.Pitch;
    if (sizeof(PixType) == 1) {
        ((dst.width & 15)
            ? CopyAndPadPlane<8,uint8_t>
            : CopyAndPadPlane<16,uint8_t>)(
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
template void AV1Enc::CopyAndPad<uint16_t>(const mfxFrameSurface1 &src, FrameData &dst, uint32_t fourcc);

static void convert16bTo8b_shift(Frame* frame)
{
    const uint16_t* src10Y = (uint16_t*)(frame->m_origin10->y);

    for (int32_t y = 0; y < frame->m_par->Height; y++) {
        for (int32_t x = 0; x < frame->m_par->Width; x++) {
            frame->m_origin->y[y * frame->m_origin->pitch_luma_pix + x] = src10Y[y * frame->m_origin10->pitch_luma_pix + x] + 2 >> 2;
        }
    }

    const uint16_t* src10Uv = (uint16_t*)(frame->m_origin10->uv);
    for (int32_t y = 0; y < frame->m_par->Height / 2; y++) {
        for (int32_t x = 0; x < frame->m_par->Width / 2; x++) {
            frame->m_origin->uv[y * frame->m_origin->pitch_chroma_pix + 2 * x] = src10Uv[y * frame->m_origin10->pitch_chroma_pix + 2 * x] + 2 >> 2;
            //frame->m_recon->uv[y * frame->m_recon->pitch_chroma_pix + 2 * x + 1] = frame->av1frameRecon.data_v16b[y * frame->av1frameRecon.pitch + x];
            frame->m_origin->uv[y * frame->m_origin->pitch_chroma_pix + 2 * x + 1] = src10Uv[y * frame->m_origin10->pitch_chroma_pix + 2 * x + 1] + 2 >> 2;
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
        if (vm_file *f = vm_file_fopen(m_videoParam.sourceDumpFileName, (out->m_frameOrder == 0) ? VM_STRING("wb") : VM_STRING("ab"))) {
            int32_t luSz = (m_videoParam.bitDepthLuma == 8) ? 1 : 2;
            int32_t luW = m_videoParam.Width - m_videoParam.CropLeft - m_videoParam.CropRight;
            int32_t luH = m_videoParam.Height - m_videoParam.CropTop - m_videoParam.CropBottom;
            int32_t luPitch = out->m_origin->pitch_luma_bytes;
            uint8_t *luPtr = out->m_origin->y + (m_videoParam.CropTop * luPitch + m_videoParam.CropLeft * luSz);
            for (int32_t y = 0; y < luH; y++, luPtr += luPitch)
                vm_file_fwrite(luPtr, luSz, luW, f);

            int32_t chSz = (m_videoParam.bitDepthChroma == 8) ? 1 : 2;
            int32_t chW = luW;
            int32_t chH = luH >> 1;
            int32_t chPitch = out->m_origin->pitch_luma_bytes;
            uint8_t *chPtr = out->m_origin->uv + (m_videoParam.CropTop / 2 * chPitch + m_videoParam.CropLeft * chSz);
            for (int32_t y = 0; y < chH; y++, chPtr += chPitch)
                vm_file_fwrite(chPtr, chSz, chW, f);

            vm_file_fclose(f);
        }
    }

        // attach lowres surface to frame
    if (m_la.get() && (m_videoParam.LowresFactor || m_videoParam.SceneCut))
        out->m_lowres = m_frameDataLowresPool.Allocate();

    // attach statistics to frame
    Statistics* stats = out->m_stats[0] = m_statsPool.Allocate();
    stats->ResetAvgMetrics();
    std::fill(stats->qp_mask.begin(), stats->qp_mask.end(), 0);
    if (m_la.get()) {
        if (m_videoParam.LowresFactor) {
            out->m_stats[1] = m_statsLowresPool.Allocate();
            out->m_stats[1]->ResetAvgMetrics();
        }
        if (m_videoParam.SceneCut) {
            out->m_sceneStats = m_sceneStatsPool.Allocate();
        }
    }

    // each new frame should be analysed by lookahead algorithms family.
    uint32_t ownerCount = (m_videoParam.DeltaQpMode ? 1 : 0) + (m_videoParam.SceneCut ? 1 : 0) + (m_videoParam.AnalyzeCmplx ? 1 : 0);
    out->m_lookaheadRefCounter = ownerCount;

    CdefParamInit(out->m_cdefParam);
    std::fill(out->m_sbIndex.begin(), out->m_sbIndex.end(), 0);
    memset(out->m_cdefStrenths, 0, m_videoParam.sb64Rows * m_videoParam.sb64Cols);
    out->m_sbCount = 0;

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


void AV1Encoder::FeiThreadRoutine()
{
    try {
        while (true) {
            ThreadingTask *task = NULL;

            std::unique_lock<std::mutex> guard(m_feiCritSect);

            while (m_stopFeiThread == 0 && (m_pauseFeiThread == 1 || m_feiSubmitTasks.empty() && m_feiWaitTasks.empty()))
                m_feiCondVar.wait(guard, [this] {
                return m_stopFeiThread != 0 || (m_pauseFeiThread != 1 && (m_feiSubmitTasks.size() || m_feiWaitTasks.size())); });


            if (m_stopFeiThread)
                break;

            if (!m_feiSubmitTasks.empty()) {
                Frame *targetFrame = m_outputQueue.front();
                std::deque<ThreadingTask *>::iterator t = m_feiSubmitTasks.begin();
                std::deque<ThreadingTask *>::iterator i = ++m_feiSubmitTasks.begin();
                std::deque<ThreadingTask *>::iterator e = m_feiSubmitTasks.end();
                for (; i != e; ++i) {
                    if ((*i)->frame == targetFrame && (*t)->frame != targetFrame ||
                        (*i)->frame->m_pyramidLayer < (*t)->frame->m_pyramidLayer ||
                        (*i)->frame->m_pyramidLayer == (*t)->frame->m_pyramidLayer && (*i)->frame->m_encOrder < (*t)->frame->m_encOrder)
                        t = i;
                }
                task = (*t);
                m_feiSubmitTasks.erase(t);
            }

            m_feiThreadRunning = 1;
            guard.unlock();

            if (task) {
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

void SetupModeDecisionControl(const AV1VideoParam &par, const Frame &frame, MdControl *ctrl)
{
    int32_t frmclass = (frame.IsIntra() ? 0 : (((par.BiPyramidLayers > 1 && frame.m_pyramidLayer >= 3) || !frame.m_isRef) ? 2 : 1));
    float Dz0 = ((frmclass == 0) ? 48.0f : ((frmclass == 1) ? 44.0 : 40.0));
    float Dz1 = ((frmclass == 0) ? 48.0f : ((frmclass == 1) ? 40.0 : 40.0));

    ctrl->miCols = par.miCols;
    ctrl->miRows = par.miRows;
    ctrl->qparam.zbin[0] = par.qparamY[frame.m_sliceQpY].zbin[0];
    ctrl->qparam.zbin[1] = par.qparamY[frame.m_sliceQpY].zbin[1];
    ctrl->qparam.round[0] = uint32_t(Dz0 * par.qparamY[frame.m_sliceQpY].dequant[0]) >> 7;
    ctrl->qparam.round[1] = uint32_t(Dz1 * par.qparamY[frame.m_sliceQpY].dequant[1]) >> 7;
    ctrl->qparam.quant[0] = par.qparamY[frame.m_sliceQpY].quant[0];
    ctrl->qparam.quant[1] = par.qparamY[frame.m_sliceQpY].quant[1];
    ctrl->qparam.quantShift[0] = par.qparamY[frame.m_sliceQpY].quantShift[0];
    ctrl->qparam.quantShift[1] = par.qparamY[frame.m_sliceQpY].quantShift[1];
    ctrl->qparam.dequant[0] = par.qparamY[frame.m_sliceQpY].dequant[0];
    ctrl->qparam.dequant[1] = par.qparamY[frame.m_sliceQpY].dequant[1];
    ctrl->lambda = frame.m_lambda;
    ctrl->lambdaInt = uint32_t(frame.m_lambdaSatd * 2048 + 0.5);
    ctrl->compound_allowed = frame.compoundReferenceAllowed;
#ifdef SINGLE_SIDED_COMPOUND
    ctrl->bidir_compund = (frame.m_picCodeType == MFX_FRAMETYPE_B);
#else
    ctrl->single_ref = !frame.isUniq[GOLDEN_FRAME] && !frame.isUniq[ALTREF_FRAME];
#endif
    const int32_t qctx = get_q_ctx(frame.m_sliceQpY);
    const TxbBitCounts &tbc = frame.bitCount.txb[qctx][TX_8X8][PLANE_TYPE_Y];
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
        ctrl->bc.skip[ctx_skip][0] = frame.bitCount.skip[ctx_skip][0];
        ctrl->bc.skip[ctx_skip][1] = frame.bitCount.skip[ctx_skip][1];
    }
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 4; j++)
            ctrl->bc.coeffBase[i][j] = tbc.coeffBase[i][j];
    for (int i = 0; i < 21; i++)
        for (int j = 0; j < 16; j++)
            ctrl->bc.coeffBrAcc[i][j] = tbc.coeffBrAcc[i][j];
    memcpy(ctrl->bc.refFrames, frame.m_refFrameBitsAv1, sizeof(ctrl->bc.refFrames));

    const auto &newmvBits  = frame.bitCount.newMvBit;
    const auto &globmvBits = frame.bitCount.zeroMvBit;
    const auto &refmvBits  = frame.bitCount.refMvBit;

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

                compModes[NEARESTMV_] = frame.bitCount.interCompMode[compModeCtx][NEAREST_NEARESTMV - NEAREST_NEARESTMV];
                compModes[NEARMV_   ] = frame.bitCount.interCompMode[compModeCtx][NEAR_NEARMV       - NEAREST_NEARESTMV];
                compModes[ZEROMV_   ] = frame.bitCount.interCompMode[compModeCtx][ZERO_ZEROMV       - NEAREST_NEARESTMV];
                compModes[NEWMV_    ] = frame.bitCount.interCompMode[compModeCtx][NEW_NEWMV         - NEAREST_NEARESTMV];
            }
        }
    }

    for (int txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
        const TxbBitCounts &tbc = frame.bitCount.txb[qctx][txSize][PLANE_TYPE_Y];
        for (int i = 0; i < 11; i++)
            ctrl->txb[txSize].eobMulti[i] = tbc.eobMulti[i];
        ctrl->txb[txSize].coeffEobDc[0] = 0;
        for (int i = 1; i < 16; i++)
            ctrl->txb[txSize].coeffEobDc[i] = tbc.coeffBaseEob[0][IPP_MIN(3, i) - 1] + tbc.coeffBrAcc[0][i];
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

    const int qpi = get_q_ctx(frame.m_sliceQpY);
    ctrl->high_freq_coeff_curve[TX_4X4][0] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_4X4][0][0];
    ctrl->high_freq_coeff_curve[TX_4X4][1] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_4X4][0][1];
    ctrl->high_freq_coeff_curve[TX_8X8][0] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_8X8][0][0];
    ctrl->high_freq_coeff_curve[TX_8X8][1] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_8X8][0][1];
    ctrl->high_freq_coeff_curve[TX_16X16][0] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_16X16][0][0];
    ctrl->high_freq_coeff_curve[TX_16X16][1] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_16X16][0][1];
    ctrl->high_freq_coeff_curve[TX_32X32][0] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_32X32][0][0];
    ctrl->high_freq_coeff_curve[TX_32X32][1] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_32X32][0][1];
}

void AV1Encoder::FeiThreadSubmit(ThreadingTask &task)
{
#if TT_TRACE
    char ttname[256] = "Submit ";
    strcat(ttname, GetFeiOpName(task.feiOp));
    strcat(ttname, " ");
    strcat(ttname, NUMBERS[task.poc & 0x7f]);
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, ttname);
#endif // TT_TRACE

#if VT_TRACE
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

    } else if (task.feiOp == MFX_FEI_H265_OP_INTRA_MODE) {
        in.meArgs.surfSrc = task.frame->m_feiOrigin->m_handle;
        for (int32_t i = 0; i < 4; i++)
            out.SurfIntraMode[i] = task.frame->m_feiIntraAngModes[i]->m_handle;

    } else if (task.feiOp == MFX_FEI_H265_OP_INTER_ME) {
        Frame *ref = task.frame->refFramesVp9[task.refIdx];
        in.meArgs.surfSrc = task.frame->m_feiOrigin->m_handle;
        in.meArgs.surfRef = ref->m_feiRecon->m_handle;
        in.meArgs.lambda = task.frame->m_lambdaSatd;

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

        in.mdArgs.compoundAllowed = task.frame->compoundReferenceAllowed;
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

        int32_t alignedWidth = ((task.frame->m_origin->width + 63)&~63);
        int32_t alignedHeight = ((task.frame->m_origin->height + 63)&~63);
        int32_t width = alignedWidth >> 3;
        int32_t height = alignedHeight >> 3;
        int32_t rscs_pitch = task.frame->m_stats[0]->m_pitchRsCs4 >> 1;
        int32_t surf_pitch = task.frame->m_feiRs8->m_pitch;

        in.mdArgs.Rs8 = task.frame->m_feiRs8->m_handle;
        in.mdArgs.Cs8 = task.frame->m_feiCs8->m_handle;

        for (int i = 0; i < height; i++)
            memcpy(task.frame->m_feiRs8->m_sysmem + i*surf_pitch, &task.frame->m_stats[0]->m_rs[1][rscs_pitch*i], sizeof(int32_t) *  width);

        for (int i = 0; i < height; i++)
            memcpy(task.frame->m_feiCs8->m_sysmem + i*surf_pitch, &task.frame->m_stats[0]->m_cs[1][rscs_pitch*i], sizeof(int32_t) *  width);

        width = alignedWidth >> 4;
        height = alignedHeight >> 4;
        rscs_pitch = task.frame->m_stats[0]->m_pitchRsCs4 >> 2;
        surf_pitch = task.frame->m_feiRs16->m_pitch;

        in.mdArgs.Rs16 = task.frame->m_feiRs16->m_handle;
        in.mdArgs.Cs16 = task.frame->m_feiCs16->m_handle;

        for (int i = 0; i < height; i++)
            memcpy(task.frame->m_feiRs16->m_sysmem + i*surf_pitch, &task.frame->m_stats[0]->m_rs[2][rscs_pitch*i], sizeof(int32_t) *  width);

        for (int i = 0; i < height; i++)
            memcpy(task.frame->m_feiCs16->m_sysmem + i*surf_pitch, &task.frame->m_stats[0]->m_cs[2][rscs_pitch*i], sizeof(int32_t) *  width);

        width = alignedWidth >> 5;
        height = alignedHeight >> 5;
        rscs_pitch = task.frame->m_stats[0]->m_pitchRsCs4 >> 3;
        surf_pitch = task.frame->m_feiRs32->m_pitch;

        in.mdArgs.Rs32 = task.frame->m_feiRs32->m_handle;
        in.mdArgs.Cs32 = task.frame->m_feiCs32->m_handle;

        for (int i = 0; i < height; i++)
            memcpy(task.frame->m_feiRs32->m_sysmem + i*surf_pitch, &task.frame->m_stats[0]->m_rs[3][rscs_pitch*i], sizeof(int32_t) *  width);

        for (int i = 0; i < height; i++)
            memcpy(task.frame->m_feiCs32->m_sysmem + i*surf_pitch, &task.frame->m_stats[0]->m_cs[3][rscs_pitch*i], sizeof(int32_t) *  width);

        width = alignedWidth >> 6;
        height = alignedHeight >> 6;
        rscs_pitch = task.frame->m_stats[0]->m_pitchRsCs4 >> 4;
        surf_pitch = task.frame->m_feiRs64->m_pitch;

        in.mdArgs.Rs64 = task.frame->m_feiRs64->m_handle;
        in.mdArgs.Cs64 = task.frame->m_feiCs64->m_handle;

        for (int i = 0; i < height; i++)
            memcpy(task.frame->m_feiRs64->m_sysmem + i*surf_pitch, &task.frame->m_stats[0]->m_rs[4][rscs_pitch*i], sizeof(int32_t) *  width);

        for (int i = 0; i < height; i++)
            memcpy(task.frame->m_feiCs64->m_sysmem + i*surf_pitch, &task.frame->m_stats[0]->m_cs[4][rscs_pitch*i], sizeof(int32_t) *  width);

        in.mdArgs.varTxInfo = task.frame->m_feiVarTxInfo->m_handle;

        MdControl modeDecisionParam;
        SetupModeDecisionControl(*task.frame->m_par, *task.frame, &modeDecisionParam);
        in.mdArgs.param = (uint8_t *)&modeDecisionParam;
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
        std::lock_guard<std::mutex> guard(m_feiCritSect);
        SetTaskFinishedThreaded(&task);

        for (int32_t i = 0; i < task.numDownstreamDependencies; i++) {
            ThreadingTask *taskDep = task.downstreamDependencies[i];
            if (vm_interlocked_dec32(&taskDep->numUpstreamDependencies) == 0) {
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

#if VT_TRACE
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

#if VT_TRACE
    MFXTraceTask *tracetask = new MFXTraceTask(&_trace_static_handle2[task->poc & 0x3f][task->action],
    __FILE__, __LINE__, __FUNCTION__, MFX_TRACE_CATEGORY, MFX_TRACE_LEVEL_API, "", false);
#endif // VT_TRACE


    mfxStatus sts = H265FEI_SyncOperation(m_fei, task->syncpoint, timeout);
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
    if (task->feiOp == MFX_FEI_H265_OP_INTER_MD)
        sts = MFX_ERR_NONE;
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

#if VT_TRACE
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
    {
        std::lock_guard<std::mutex> guard(m_critSect);
        SetTaskFinishedThreaded(task);

        for (int32_t i = 0; i < task->numDownstreamDependencies; i++) {
            ThreadingTask *taskDep = task->downstreamDependencies[i];

#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
            assert(taskDep->action != TT_GPU_WAIT);
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
            assert(taskDep->action != TT_GPU_SUBMIT && taskDep->action != TT_GPU_WAIT);
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

            if (vm_interlocked_dec32(&taskDep->numUpstreamDependencies) == 0) {
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                if (taskDep->action == TT_GPU_SUBMIT)
                    m_feiSubmitTasks.push_back(taskDep);
                else
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
                {
                    m_pendingTasks.push_back(taskDep);
                    m_condVar.notify_one();
                }
            }
        }
    }

    return true;
}

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
