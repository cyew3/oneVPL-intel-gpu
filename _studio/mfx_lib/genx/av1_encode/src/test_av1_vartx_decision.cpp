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

#include <math.h>
#include <stdio.h>

#pragma warning(push)
#pragma warning(disable: 4100 4201 4505)
#include "cm_rt.h"
#include "cm_def.h"
#include "cm_vm.h"
void suppress_unused_static_function_warning() { &CmEmulSys::abs; };
#pragma warning(pop)

#include <assert.h>
#include <stdexcept>
#include <map>
#include <vector>
#include <memory>
#include "../include/test_common.h"

#pragma warning(push)
#pragma warning(disable: 4201)
#include "vartx/mfx_av1_ctb.h"
#include "vartx/mfx_av1_tables.h"
#include "vartx/mfx_av1_bit_count.h"
#include "vartx/mfx_av1_quant.h"
#include "vartx/mfx_av1_enc.h"
#include "vartx/mfx_av1_dispatcher.h"
#include "vartx/mfx_av1_dispatcher_wrappers.h"
#include "vartx/mfx_av1_copy.h"
#pragma warning(pop)

using namespace H265Enc;

#ifdef CMRT_EMU
extern "C" void VarTxDecision();

const char genx_av1_vartx_decision_hsw[1] = {};
const char genx_av1_vartx_decision_bdw[1] = {};
const char genx_av1_vartx_decision_skl[1] = {};
#else //CMRT_EMU
#include "../include/genx_av1_vartx_decision_hsw_isa.h"
#include "../include/genx_av1_vartx_decision_bdw_isa.h"
#include "../include/genx_av1_vartx_decision_skl_isa.h"
#endif //CMRT_EMU


class cm_error : public std::runtime_error
{
    int m_errcode;
    int m_line;
    const char *m_file;
public:
    cm_error(int errcode, const char *file, int line, const char *message)
        : runtime_error(message), m_errcode(errcode), m_line(line), m_file(file) {}
    int errcode() const { return m_errcode; }
    int line() const { return m_line; }
    const char *file() const { return m_file; }
};

#define THROW_CM_ERR(ERR) if ((ERR) != CM_SUCCESS) { throw cm_error(ERR, __FILE__, __LINE__, nullptr); }
#define THROW_CM_ERR_MSG(ERR, MSG) if ((ERR) != CM_SUCCESS) { throw cm_error(ERR, __FILE__, __LINE__, MSG); }

namespace H265Enc {
    template <typename PixType>
        RdCost TransformInterYSbVarTxByRdo(int32_t bsz, TxSize txSize, uint8_t *aboveNzCtx, uint8_t *leftNzCtx,
                                           const PixType* src_, PixType *rec_, const int16_t *diff_, int16_t *coeff_, int16_t *qcoeff_,
                                           int32_t qp, const BitCounts &bc, const H265VideoParam &par, TxType *chromaTxType,
                                           CostType lambda, int16_t *coeffWork_, uint8_t* aboveTxfmCtx, uint8_t* leftTxfmCtx,
                                           uint16_t* vartxInfo, const QuantParam &qpar, float *roundFAdj, int miRow, int miCol);

    void SetVarTxInfo(uint16_t *vartxInfo, uint16_t value, int32_t w);
};

struct GpuQuantParam {
    int16_t zbin[2];
    int16_t round[2];
    int16_t quant[2];
    int16_t quantShift[2];
    int16_t dequant[2];
};

struct GpuParam {
    // gathered here for gpu
    uint16_t miCols;                      // off=0    size=2
    uint16_t miRows;                      // off=2    size=2
    float lambda;                      // off=4    size=4
    GpuQuantParam qparam;               // off=8    size=20
    uint16_t lambdaInt;                   // off=28   size=2
    uint8_t compound_allowed;             // off=30   size=1
#ifdef SINGLE_SIDED_COMPOUND
    uint8_t bidir_compound;               // off=31   size=1 (single_ref = !compound_allowed)
#else
    uint8_t single_ref;                   // off=31   size=1
#endif
    struct LutBitCost {
        uint16_t eobMulti[7];             // off=32   size=2*7
        uint16_t coeffEobDc[16];          // off=46   size=2*16
        uint16_t coeffBaseEobAc[3];       // off=78   size=2*3
        uint16_t txbSkip[7][2];           // off=84   size=2*14
        uint16_t skip[3][2];              // off=112  size=2*6
        uint16_t coeffBase[16][4];        // off=124  size=2*64
        uint16_t coeffBrAcc[21][16];      // off=252  size=2*336
        uint16_t refFrames[6][6][4];      // off=924  size=2*144
        uint16_t singleModes[3][3][2][4]; // off=1212 size=2*72
        uint16_t compModes[3][3][2][4];   // off=1356 size=2*72
    } bc;

    struct LutCoefBitCost {
        uint16_t eobMulti[11];            // off=1500 size=2*11
        uint16_t coeffEobDc[16];          // off=1522 size=2*16
        uint16_t coeffBaseEobAc[3];       // off=1554 size=2*3
        uint16_t txbSkip[7][2];           // off=1560 size=2*14
        uint16_t coeffBase[16][4];        // off=1588 size=2*64
        uint16_t coeffBrAcc[21][16];      // off=1716 size=2*336
                                        // off=2388
    } txb[4];

    uint16_t reserved[2];                 // off=5052 size=2*2
    uint16_t high_freq_coeff_curve[4][2]; // off=5056 size=2*4*2
};

typedef decltype(&CM_ALIGNED_FREE) cm_aligned_deleter;

template <typename T>
std::unique_ptr<T, cm_aligned_deleter> make_aligned_ptr(size_t nelems, size_t align)
{
    if (void *p = CM_ALIGNED_MALLOC(nelems * sizeof(T), align))
        return std::unique_ptr<T, cm_aligned_deleter>(static_cast<T *>(p), CM_ALIGNED_FREE);
    throw std::bad_alloc();
}

typedef std::unique_ptr<ModeInfo, cm_aligned_deleter> mode_info_ptr;

inline BlockSize GetSbType(int32_t depth, int32_t partition) {
    return BlockSize(3 * (4 - depth) - partition);
}

int stat_sbType[4] = {};
int stat_skip[2] = {};

void RandomizeSb(const GpuParam &par, ModeInfo *mi, int pitchMi, int mi_row, int mi_col, int depth)
{
    if (mi_row >= par.miRows || mi_col >= par.miCols) {
        mi[mi_row * pitchMi + mi_col].sbType = (4 - depth) * 3;
        mi[mi_row * pitchMi + mi_col].mode   = 255; // out of picture
        return;
    }

    const int s = 8 >> depth;
    const int ss = s >> 1;
    const int must_split = (mi_row + s > par.miRows || mi_col + s > par.miCols);
    const int non_split = depth == 3 || !must_split && (rand() % 6 > 0);
    if (non_split) {
        const int skip = (rand() & 7) == 0; // 1/8 MIs are skipped
        mi[mi_row * pitchMi + mi_col].sbType = (4 - depth) * 3;
        mi[mi_row * pitchMi + mi_col].mode   = 0;
        mi[mi_row * pitchMi + mi_col].skip   = skip;
        for (int i = 0; i < s; i++)
            for (int j = 0; j < s; j++)
                mi[(mi_row + i) * pitchMi + (mi_col + j)] = mi[mi_row * pitchMi + mi_col];
        stat_sbType[3-depth]++;
        stat_skip[skip]++;
        if (depth == 0) {
            mi[(mi_row +  0) * pitchMi + mi_col + ss].sbType = (4 - depth) * 3;
            mi[(mi_row + ss) * pitchMi + mi_col +  0].sbType = (4 - depth) * 3;
            mi[(mi_row + ss) * pitchMi + mi_col + ss].sbType = (4 - depth) * 3;
            mi[(mi_row +  0) * pitchMi + mi_col + ss].mode   = 0;
            mi[(mi_row + ss) * pitchMi + mi_col +  0].mode   = 0;
            mi[(mi_row + ss) * pitchMi + mi_col + ss].mode   = 0;
        }
        return;
    }

    RandomizeSb(par, mi, pitchMi, mi_row + 0,  mi_col + 0,  depth + 1);
    RandomizeSb(par, mi, pitchMi, mi_row + 0,  mi_col + ss, depth + 1);
    RandomizeSb(par, mi, pitchMi, mi_row + ss, mi_col + 0,  depth + 1);
    RandomizeSb(par, mi, pitchMi, mi_row + ss, mi_col + ss, depth + 1);
}

void RandomizeModeInfo(const GpuParam &par, ModeInfo *mi, int pitchMi)
{
    const int sbRows = (par.miRows + 7) / 8;
    const int sbCols = (par.miCols + 7) / 8;

    for (int sbr = 0; sbr < sbRows; sbr++) {
        for (int sbc = 0; sbc < sbCols; sbc++) {
            RandomizeSb(par, mi, pitchMi, sbr * 8, sbc * 8, 0);
        }
    }

    const float sum_stat_sbType = (stat_sbType[0] + stat_sbType[1] + stat_sbType[2] + stat_sbType[3]) / 100.0f;
    const float sum_stat_skip = (stat_skip[0] + stat_skip[1]) / 100.0f;
    printf("ModeInfo statistics: 8|16|32|64 skip|nonskip %.1f|%.1f|%.1f|%.1f%% %.1f|%.1f%%\n",
        stat_sbType[0] / sum_stat_sbType, stat_sbType[1] / sum_stat_sbType,
        stat_sbType[2] / sum_stat_sbType, stat_sbType[3] / sum_stat_sbType,
        stat_skip[1] / sum_stat_skip, stat_skip[0] / sum_stat_skip);
}

std::unique_ptr<uint16_t, cm_aligned_deleter>
    RunCpu(const GpuParam &par, const uint8_t *srcFrame, int pitchSrc, const uint8_t *predFrame,
           int pitchPred, int qp, const ModeInfo *modeInfo, int pitchMi)
{
    const int sbRows = (par.miRows + 7) / 8;
    const int sbCols = (par.miCols + 7) / 8;
    const int numSb = sbRows * sbCols;

    auto varTxInfo = make_aligned_ptr<uint16_t>(numSb * 16 * 16, 32);
    memset(varTxInfo.get(), 0, sizeof(uint16_t) * numSb * 16 * 16);

    BitCounts bc;
    EstimateBits(bc, 0, CODEC_AV1);

    H265VideoParam videoPar = {};
    QuantParam qpar;
    qpar.zbin[0] = par.qparam.zbin[0];
    qpar.zbin[1] = par.qparam.zbin[1];
    qpar.round[0] = par.qparam.round[0];
    qpar.round[1] = par.qparam.round[1];
    qpar.quant[0] = par.qparam.quant[0];
    qpar.quant[1] = par.qparam.quant[1];
    qpar.quantShift[0] = par.qparam.quantShift[0];
    qpar.quantShift[1] = par.qparam.quantShift[1];
    qpar.dequant[0] = par.qparam.dequant[0];
    qpar.dequant[1] = par.qparam.dequant[1];

    for (int sbr = 0, sbIdx = 0; sbr < sbRows; sbr++) {
        for (int sbc = 0; sbc < sbCols; sbc++, sbIdx++) {

            Contexts contexts = {}; // zero contexts in the beginning of each SB

            uint16_t *sbVarTxInfo = varTxInfo.get() + sbIdx * 16 * 16;

            small_memset0(contexts.aboveNonzero[0], 16);
            small_memset0(contexts.leftNonzero[0], 16);
            small_memset0(contexts.aboveTxfm, 16);
            small_memset0(contexts.leftTxfm, 16);

            int num4x4 = 256;
            for (int i = 0; i < 256; i += num4x4) {
                const int rasterIdx = h265_scan_z2r4[i];
                const int x4 = (rasterIdx & 15);
                const int y4 = (rasterIdx >> 4);
                const int miRow = sbr * 8 + (y4 >> 1);
                const int miCol = sbc * 8 + (x4 >> 1);
                uint8_t *anz = contexts.aboveNonzero[0] + x4;
                uint8_t *lnz = contexts.leftNonzero[0] + y4;
                uint8_t *atxfm = contexts.aboveTxfm + x4;
                uint8_t *ltxfm = contexts.leftTxfm + y4;

                const ModeInfo *mi = modeInfo + miRow * pitchMi + miCol;
                num4x4 = num_4x4_blocks_lookup[mi->sbType];
                if (mi->mode == OUT_OF_PIC)
                    continue;

                const int log2w = block_size_wide_4x4_log2[mi->sbType];
                const int num4x4w = 1 << log2w;
                const int sbw = num4x4w << 2;

                uint16_t *miVarTxInfo = sbVarTxInfo + y4 * 16 + x4; // [0-1] bits - txType, [2-3] - txSize, [4] - eobFlag or [4-15] - eob/num non zero coefs
                const TxSize maxTxSize = IPP_MIN(TX_32X32, max_txsize_lookup[mi->sbType]);

                if (mi->skip) {
                    SetVarTxInfo(miVarTxInfo, DCT_DCT + (maxTxSize << 2), num4x4w);
                } else {
                    ALIGNED(32) uint8_t rec[64 * 64];
                    ALIGNED(32) int16_t diff[64 * 64];
                    ALIGNED(32) int16_t coef[64 * 64];
                    ALIGNED(32) int16_t qcoef[64 * 64];
                    ALIGNED(32) int16_t coefWork[32 * 32 * 6];
                    float roundFAdj[2] = {};

                    const uint8_t *src = srcFrame + miRow * 8 * pitchSrc + miCol * 8;
                    const uint8_t *pred = predFrame + miRow * 8 * pitchPred + miCol * 8;

                    CopyNxN(predFrame, pitchPred, rec, sbw);
                    VP9PP::diff_nxn(src, pitchSrc, pred, pitchPred, diff, sbw, log2w);

                    RdCost rd = TransformInterYSbVarTxByRdo(
                        mi->sbType, maxTxSize, anz, lnz, src, rec, diff, coef, qcoef, qp, bc, videoPar,
                        NULL, par.lambda, coefWork, atxfm, ltxfm, miVarTxInfo, qpar, roundFAdj, miRow, miCol);
                    int sseSkip = VP9PP::sse(src, pitchSrc, pred, pitchPred, log2w, log2w);

                    uint32_t modeBits = 0;
                    uint32_t coefBits = rd.coefBits;
                    CostType cost = rd.sse + par.lambda * (modeBits + ((coefBits * 3) >> 2));
                    CostType costSkip = sseSkip/* + par.lambda * skipModeBits*/;
                    if (costSkip <= cost || rd.eob == 0)
                        SetVarTxInfo(miVarTxInfo, DCT_DCT + (maxTxSize << 2), num4x4w);
                }
            }
        }
    }

    return varTxInfo;
}

template <typename T> SurfaceIndex *get_index(T *cm_resource) {
    SurfaceIndex *index = 0;
    int res = cm_resource->GetIndex(index);
    THROW_CM_ERR(res);
    return index;
}

std::unique_ptr<uint16_t, cm_aligned_deleter>
    RunGpu(const GpuParam &par, const uint8_t *srcFrame, int pitchSrc, const uint8_t *predFrame, int pitchPred,
           int qp, const ModeInfo *modeInfo, int pitchMi)
{
    qp;
    const int pred_padding = 128;
    const int width = par.miCols * 8;
    const int height = par.miRows * 8;
    const int sbRows = (par.miRows + 7) / 8;
    const int sbCols = (par.miCols + 7) / 8;
    const int numSb = sbRows * sbCols;
    uint32_t tsw = sbCols;
    uint32_t tsh = sbRows;

    auto varTxInfoSys = make_aligned_ptr<uint16_t>(numSb * 16 * 16, 0x1000);
    memset(varTxInfoSys.get(), 0, sizeof(uint16_t) * numSb * 16 * 16);

    mfxU32 version = 0;
    CmDevice *device = nullptr;
    CmProgram *program = nullptr;
    CmKernel *kernel = nullptr;
    CmTask *task = nullptr;
    CmQueue *queue = nullptr;
    CmThreadSpace *ts = nullptr;
    CmEvent *e = nullptr;
    CmBufferUP *param = nullptr;
    CmSurface2D *src = nullptr;
    CmSurface2DUP *pred = nullptr;
    CmSurface2D *mi = nullptr;
    CmSurface2D *scratchbuf = nullptr;
    CmBufferUP *vartxinfo = nullptr;

    GPU_PLATFORM hw_type;
    size_t size = sizeof(mfxU32);

    THROW_CM_ERR(CreateCmDevice(device, version));
    THROW_CM_ERR(device->InitPrintBuffer());
    THROW_CM_ERR(device->CreateQueue(queue));
    THROW_CM_ERR(device->GetCaps(CAP_GPU_PLATFORM, size, &hw_type));
#ifdef CMRT_EMU
    hw_type = PLATFORM_INTEL_HSW;
#endif // CMRT_EMU
    switch (hw_type) {
    case PLATFORM_INTEL_HSW:
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_vartx_decision_hsw, sizeof(genx_av1_vartx_decision_hsw), program, "nojitter"));
        break;
    case PLATFORM_INTEL_BDW:
    case PLATFORM_INTEL_CHV:
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_vartx_decision_bdw, sizeof(genx_av1_vartx_decision_bdw), program, "nojitter"));
        break;
    case PLATFORM_INTEL_SKL:
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_vartx_decision_skl, sizeof(genx_av1_vartx_decision_skl), program, "nojitter"));
        break;
    default:
        throw cm_error(CM_FAILURE, __FILE__, __LINE__, "Unknown HW type");
    }
    THROW_CM_ERR(device->CreateKernel(program, CM_KERNEL_FUNCTION(VarTxDecision), kernel));
    THROW_CM_ERR(device->CreateTask(task));

    auto param_sys = make_aligned_ptr<uint8_t>(sizeof(GpuParam), 4096);
    THROW_CM_ERR(device->CreateBufferUP(sizeof(GpuParam), param_sys.get(), param));
    memcpy(param_sys.get(), &par, sizeof(par));

    THROW_CM_ERR(device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_P8, src));
    int pitchPredSys = (width + 2 * pred_padding + 63) & ~63;
    auto predSys_ = make_aligned_ptr<uint8_t>(pitchPredSys * height, 4096);
    auto predSys = predSys_.get();
    THROW_CM_ERR(device->CreateSurface2DUP(pitchPredSys, height, CM_SURFACE_FORMAT_P8, predSys, pred));
    THROW_CM_ERR(device->CreateSurface2D(sbCols * 8 * sizeof(ModeInfo), sbRows * 8, CM_SURFACE_FORMAT_P8, mi));
    THROW_CM_ERR(device->CreateBufferUP(numSb * 16 * 16 * sizeof(uint16_t), varTxInfoSys.get(), vartxinfo));
    THROW_CM_ERR(device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_P8, scratchbuf));
    THROW_CM_ERR(src->WriteSurfaceStride(srcFrame, nullptr, pitchSrc));
    //THROW_CM_ERR(pred->WriteSurfaceStride(predFrame, nullptr, pitchPred));
    for (int i = 0; i < height; i++) {
        memset(predSys + i * pitchPredSys, *(predFrame + i * pitchPred), pred_padding);
        memcpy(predSys + i * pitchPredSys + pred_padding, predFrame + i * pitchPred, width);
        memset(predSys + i * pitchPredSys + pred_padding + width, *(predFrame + i * pitchPred + width - 1), pred_padding);
    }
    THROW_CM_ERR(mi->WriteSurfaceStride((const uint8_t *)modeInfo, nullptr, pitchMi * sizeof(ModeInfo)));
    THROW_CM_ERR(kernel->SetKernelArg(0, sizeof(SurfaceIndex), get_index(param)));
    THROW_CM_ERR(kernel->SetKernelArg(1, sizeof(SurfaceIndex), get_index(src)));
    THROW_CM_ERR(kernel->SetKernelArg(2, sizeof(SurfaceIndex), get_index(pred)));
    THROW_CM_ERR(kernel->SetKernelArg(3, sizeof(SurfaceIndex), get_index(mi)));
    THROW_CM_ERR(kernel->SetKernelArg(4, sizeof(SurfaceIndex), get_index(scratchbuf)));
    THROW_CM_ERR(kernel->SetKernelArg(5, sizeof(SurfaceIndex), get_index(vartxinfo)));
    THROW_CM_ERR(kernel->SetKernelArg(6, sizeof(int), &pred_padding));
    THROW_CM_ERR(task->AddKernel(kernel));
    THROW_CM_ERR(kernel->SetThreadCount(tsw * tsh));
    THROW_CM_ERR(device->CreateThreadSpace(tsw, tsh, ts));
    THROW_CM_ERR(ts->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY));
    THROW_CM_ERR(queue->Enqueue(task, e, ts));
    THROW_CM_ERR(e->WaitForTaskFinished());
    THROW_CM_ERR(device->FlushPrintBuffer());

#ifndef CMRT_EMU
    printf("TIME = %.3f ms\n", GetAccurateGpuTime(queue, task, ts) / 1000000.);
#endif //CMRT_EMU

    device->DestroyBufferUP(param);
    device->DestroySurface(src);
    device->DestroySurface2DUP(pred);
    device->DestroySurface(mi);
    device->DestroyBufferUP(vartxinfo);
    device->DestroyTask(task);
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    DestroyCmDevice(device);

    return varTxInfoSys;
}

void report_var_tx_info_difference(int sbr, int sbc, int y4, int x4, const ModeInfo &mi, uint16_t cpu, uint16_t gpu)
{
    #define DUMP_MODE_INFO_FIELD(name) \
        printf("%-10s %6d\n", #name, mi.name)

    const int mi_row = sbr * 8 + y4 / 2;
    const int mi_col = sbc * 8 + x4 / 2;

    printf("\n");
    printf("ModeInfo at (mi_row=%d, mi_col=%d)\n", mi_row, mi_col);
    DUMP_MODE_INFO_FIELD(sbType);
    DUMP_MODE_INFO_FIELD(mode);
    DUMP_MODE_INFO_FIELD(skip & 1);
    printf("\n");

    printf("VarTxInfo at (y4=%d, x4=%d) differ\n", y4, x4);
    printf("%10s %6s %6s\n", "", "cpu", "gpu");
    printf("%-10s %6d %6d\n", "txType", cpu & 3, gpu & 3);
    printf("%-10s %6d %6d\n", "txSize", (cpu >> 2) & 3, (gpu >> 2) & 3);
    printf("%-10s %6d %6d\n", "eob",    cpu >> 4, gpu >> 4);
}

int Compare(const GpuParam &par, const ModeInfo *modeInfo, int pitchMi, const uint16_t *cpuVarTxInfo, const uint16_t *gpuVarTxInfo)
{
    const int sbRows = (par.miRows + 7) / 8;
    const int sbCols = (par.miCols + 7) / 8;

    for (int sbr = 0, sbIdx = 0; sbr < sbRows; sbr++) {
        for (int sbc = 0; sbc < sbCols; sbc++, sbIdx++) {

            const uint16_t *sbCpuVarTxInfo = cpuVarTxInfo + sbIdx * 16 * 16;
            const uint16_t *sbGpuVarTxInfo = gpuVarTxInfo + sbIdx * 16 * 16;

            for (int y = 0; y < 16; y++) {
                for (int x = 0; x < 16; x++) {
                    const uint16_t cpu = sbCpuVarTxInfo[y * 16 + x];
                    const uint16_t gpu = sbGpuVarTxInfo[y * 16 + x];
                    if (cpu != gpu) {
                        const int miRow = sbr * 8 + y / 2;
                        const int miCol = sbc * 8 + x / 2;
                        const ModeInfo &mi = modeInfo[miRow * pitchMi + miCol];
                        report_var_tx_info_difference(sbr, sbc, y, x, mi, cpu, gpu);
                        return FAILED;
                    }

                }
            }
        }
    }

    return PASSED;
}

void PrintVarTxStat(const GpuParam &par, const uint16_t *varTxInfo)
{
    const int sbRows = (par.miRows + 7) / 8;
    const int sbCols = (par.miCols + 7) / 8;

    uint32_t size_type[4][4] = {};
    uint32_t eob[2] = {};

    for (int sbr = 0, sbIdx = 0; sbr < sbRows; sbr++) {
        for (int sbc = 0; sbc < sbCols; sbc++, sbIdx++) {

            const uint16_t *sbVarTxInfo = varTxInfo + sbIdx * 16 * 16;
            for (int y = 0; y < 16; y++) {
                for (int x = 0; x < 16; x++) {
                    const uint16_t vti = sbVarTxInfo[y * 16 + x];
                    const int size = (vti >> 2) & 3;
                    const int type = vti & 3;
                    size_type[size][type]++;
                    eob[!!(vti >> 4)]++;
                }
            }
        }
    }

    float norm = 0.0f;
    for (int size = 0; size < 4; size++)
        for (int type = 0; type < 4; type++) {
            size_type[size][type] >>= (size << 1); // area-normalized
            norm += size_type[size][type];
        }
    norm /= 100.0f;
    const float sum_eob = (eob[0] + eob[1]) / 100.0f;

    printf("VarTx statistics:\n");
    printf("        DCT_DCT ADST_DCT DCT_ADST ADST_ADST\n");
    printf("   4x4  %6.2f%% %7.2f%% %7.2f%% %8.2f%%\n", size_type[0][0] / norm, size_type[0][1] / norm, size_type[0][2] / norm, size_type[0][3] / norm);
    printf("   8x8  %6.2f%% %7.2f%% %7.2f%% %8.2f%%\n", size_type[1][0] / norm, size_type[1][1] / norm, size_type[1][2] / norm, size_type[1][3] / norm);
    printf("  16x16 %6.2f%% %7.2f%% %7.2f%% %8.2f%%\n", size_type[2][0] / norm, size_type[2][1] / norm, size_type[2][2] / norm, size_type[2][3] / norm);
    printf("  32x32 %6.2f%%\n", size_type[3][0] / norm);
    printf("\n");
    printf("  eob==0|eob>0 %.1f|%.1f%%\n", eob[0] / sum_eob, eob[1] / sum_eob);
}

namespace global_test_params {
    const char *yuv = "C:/yuv/1080p/park_joy_1920x1080_500_50.yuv";
    int32_t width  = 1920;
    int32_t height = 1080;
    int32_t qp     = 140;
};

void parse_cmd(int argc, char **argv) {
    if (argc == 2) {
        if (_stricmp(argv[1], "--help") == 0 ||
            _stricmp(argv[1], "-h") == 0 ||
            _stricmp(argv[1], "-?") == 0)
        {

            char exe_name[256], exe_ext[256];
            _splitpath_s(argv[0], nullptr, 0, nullptr, 0, exe_name, 256, exe_ext, 256);
            printf("Usage examples:\n\n");
            printf("  1) Run with default parameters (yuv=%s width=%d height=%d qp=%d)\n",
                global_test_params::yuv, global_test_params::width, global_test_params::height, global_test_params::qp);
            printf("    %s%s\n\n", exe_name, exe_ext);
            printf("  2) Run with custom parameters\n");
            printf("    %s%s <yuvname> <width> <height> <qp>\n", exe_name, exe_ext);
            exit(0);
        }
    }

    if (argc == 5) {
        global_test_params::yuv    = argv[1];
        global_test_params::width  = atoi(argv[2]);
        global_test_params::height = atoi(argv[3]);
        global_test_params::qp     = atoi(argv[4]);
    }
}

void setup_gpu_param(GpuParam *par, int qp)
{
    const int q = vp9_dc_quant(qp, 0, 8);

    BitCounts bc;
    EstimateBits(bc, 0, CODEC_AV1);

    H265VideoParam videoPar = {};
    InitQuantizer(videoPar);

    par->miCols = (global_test_params::width + 7) / 8;
    par->miRows = (global_test_params::height + 7) / 8;
    par->lambda = 88 * q * q / 24.f / 512 / 16 / 128;
    par->qparam.zbin[0] = videoPar.qparamY[qp].zbin[0];
    par->qparam.zbin[1] = videoPar.qparamY[qp].zbin[1];
    par->qparam.round[0] = videoPar.qparamY[qp].round[0];
    par->qparam.round[1] = videoPar.qparamY[qp].round[1];
    par->qparam.quant[0] = videoPar.qparamY[qp].quant[0];
    par->qparam.quant[1] = videoPar.qparamY[qp].quant[1];
    par->qparam.quantShift[0] = videoPar.qparamY[qp].quantShift[0];
    par->qparam.quantShift[1] = videoPar.qparamY[qp].quantShift[1];
    par->qparam.dequant[0] = videoPar.qparamY[qp].dequant[0];
    par->qparam.dequant[1] = videoPar.qparamY[qp].dequant[1];
    par->lambdaInt = 0;
    par->compound_allowed = 1;
    par->bidir_compound = 1;
    const int qctx = get_q_ctx(qp);

    for (int ctx_skip = 0; ctx_skip < 3; ctx_skip++) {
        par->bc.skip[ctx_skip][0] = bc.skip[ctx_skip][0];
        par->bc.skip[ctx_skip][1] = bc.skip[ctx_skip][1];
    }

    for (int txSize = TX_4X4; txSize <= TX_32X32; txSize++) {
        const TxbBitCounts &tbc = bc.txb[qctx][txSize][PLANE_TYPE_Y];
        for (int i = 0; i < 11; i++)
            par->txb[txSize].eobMulti[i] = tbc.eobMulti[i];
        par->txb[txSize].coeffEobDc[0] = 0;
        for (int i = 1; i < 16; i++)
            par->txb[txSize].coeffEobDc[i] = tbc.coeffBaseEob[0][IPP_MIN(3, i) - 1] + tbc.coeffBrAcc[0][i];
        for (int i = 0; i < 3; i++)
            par->txb[txSize].coeffBaseEobAc[i] = tbc.coeffBaseEob[1][i];
        for (int i = 0; i < 13; i++)
            for (int j = 0; j < 2; j++)
                par->txb[txSize].txbSkip[i][j] = tbc.txbSkip[i][j];
        for (int i = 0; i < 16; i++)
            for (int j = 0; j < 4; j++)
                par->txb[txSize].coeffBase[i][j] = tbc.coeffBase[i][j];
        for (int i = 0; i < 21; i++)
            for (int j = 0; j < 16; j++)
                par->txb[txSize].coeffBrAcc[i][j] = tbc.coeffBrAcc[i][j];
    }

    // bits = a * BSR(level + 1) + b
    const int32_t HIGH_FREQ_COEFF_FIT_CURVE[TOKEN_CDF_Q_CTXS][TX_32X32 + 1][PLANE_TYPES][2] = {
        { // q <= 20
          //     luma          chroma
            { { 895, 240 },{ 991, 170 } }, // TX_4X4
            { { 895, 240 },{ 991, 170 } }, // TX_8X8
            { { 1031, 177 },{ 1110, 125 } }, // TX_16X16
            { { 1005, 192 },{ 1559,  72 } }, // TX_32X32
        },
        { // q <= 60
            { { 895, 240 },{ 991, 170 } }, // TX_4X4
            { { 895, 240 },{ 991, 170 } }, // TX_8X8
            { { 1031, 177 },{ 1110, 125 } }, // TX_16X16
            { { 1005, 192 },{ 1559,  72 } }, // TX_32X32
        },
        { // q <= 120
            { { 895, 240 },{ 991, 170 } }, // TX_4X4
            { { 895, 240 },{ 991, 170 } }, // TX_8X8
            { { 1031, 177 },{ 1110, 125 } }, // TX_16X16
            { { 1005, 192 },{ 1559,  72 } }, // TX_32X32
        },
        { // q > 120
            { { 882, 203 },{ 1273, 100 } }, // TX_4X4
            { { 882, 203 },{ 1273, 100 } }, // TX_8X8
            { { 1094, 132 },{ 1527,  69 } }, // TX_16X16
            { { 1234, 102 },{ 1837,  61 } }, // TX_32X32
        },
    };

    const int qpi = get_q_ctx(qp);
    par->high_freq_coeff_curve[TX_4X4][0] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_4X4][0][0];
    par->high_freq_coeff_curve[TX_4X4][1] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_4X4][0][1];
    par->high_freq_coeff_curve[TX_8X8][0] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_8X8][0][0];
    par->high_freq_coeff_curve[TX_8X8][1] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_8X8][0][1];
    par->high_freq_coeff_curve[TX_16X16][0] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_16X16][0][0];
    par->high_freq_coeff_curve[TX_16X16][1] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_16X16][0][1];
    par->high_freq_coeff_curve[TX_32X32][0] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_32X32][0][0];
    par->high_freq_coeff_curve[TX_32X32][1] = HIGH_FREQ_COEFF_FIT_CURVE[qpi][TX_32X32][0][1];
}

void load_frame(FILE *f, uint8_t *frame, int pitch, int width, int height)
{
    for (int i = 0; i < height; i++)
        if (fread(frame + i * pitch, 1, width, f) != width)
            throw std::exception("Failed to read frame from file");
    fseek(f, width * ((height + 1) / 2), SEEK_CUR); // skip chroma
}

void make_pred_frame(const uint8_t *src, int pitchSrc, uint8_t *pred, int pitchPred, int width, int height)
{
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            const int qstep = (rand() % 30) + 1;
            const int offset = qstep / 2;
            pred[i * pitchPred + j] = Saturate(0, 255, (src[i * pitchSrc + j] + offset) / qstep * qstep);
        }
    }
}

int main(int argc, char **argv)
{
    try {
        parse_cmd(argc, argv);
        print_hw_caps();

        const int qp = global_test_params::qp;
        const int width = global_test_params::width;
        const int height = global_test_params::height;

        printf("Running with %s %dx%d qp=%d\n", global_test_params::yuv, width, height, qp);

        GpuParam param = {};
        setup_gpu_param(&param, qp);

        const int pitchMi = ((param.miCols + 7) & ~7);
        const int numMi = ((param.miRows + 7) & ~7) * pitchMi;

        const int pitchFrame = AlignValue(width, 64);
        const int frameSize = height * pitchFrame;
        auto srcFrameBuf = make_aligned_ptr<uint8_t>(frameSize, 64);
        auto predFrameBuf = make_aligned_ptr<uint8_t>(frameSize, 64);

        uint8_t *srcFrame = srcFrameBuf.get();
        uint8_t *predFrame = predFrameBuf.get();

        FILE *f = fopen(global_test_params::yuv, "rb");
        if (!f)
            throw std::exception("Failed to open input YUV file");
        load_frame(f, srcFrame, pitchFrame, width, height);
        fclose(f);

        make_pred_frame(srcFrame, pitchFrame, predFrame, pitchFrame, width, height);

        auto mi = make_aligned_ptr<ModeInfo>(numMi, 0x1000);
        memset(mi.get(), 0, sizeof(ModeInfo) * numMi);

        VP9PP::initDispatcher(VP9PP::CPU_FEAT_AVX2, 1);

        RandomizeModeInfo(param, mi.get(), pitchMi);
        auto cpuVarTxInfo = RunCpu(param, srcFrame, pitchFrame, predFrame, pitchFrame, qp, mi.get(), pitchMi);
        auto gpuVarTxInfo = RunGpu(param, srcFrame, pitchFrame, predFrame, pitchFrame, qp, mi.get(), pitchMi);
        PrintVarTxStat(param, cpuVarTxInfo.get());
        if (Compare(param, mi.get(), pitchMi, cpuVarTxInfo.get(), gpuVarTxInfo.get()) != PASSED)
            return printf("FAILED\n"), FAILED;

    } catch (cm_error &e) {
        printf("CM_ERROR:\n");
        printf("  code: %d\n", e.errcode());
        printf("  file: %s\n", e.file());
        printf("  line: %d\n", e.line());
        if (const char *msg = e.what())
            printf("  message: %s\n", msg);
    } catch (std::exception &e) {
        printf("ERROR: %s\n", e.what());
        return FAILED;
    }

    return printf("PASSED\n"), PASSED;
}
