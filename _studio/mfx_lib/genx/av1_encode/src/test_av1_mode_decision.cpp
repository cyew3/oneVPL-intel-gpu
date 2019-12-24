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
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#include "cm_def.h"
#include "cm_vm.h"
#pragma warning(pop)

#include "vector"
#include "memory"
#include "fstream"
#include "new"
#include "../include/test_common.h"
#include "mode_decision/mfx_av1_defs.h"
#include "mode_decision/mfx_av1_ctb.h"
#include "mode_decision/mfx_av1_enc.h"
#include "mode_decision/mfx_av1_encode.h"
#include "mode_decision/mfx_av1_probabilities.h"
#include "mode_decision/mfx_av1_bit_count.h"
#include "mode_decision/mfx_av1_frame.h"
#include "mode_decision/mfx_av1_quant.h"
#include "mode_decision/mfx_av1_dispatcher.h"
#include "mode_decision/mfx_av1_dispatcher_wrappers.h"
#include "mode_decision/mfx_av1_dispatcher_wrappers.h"
#include "mode_decision/mfx_av1_copy.h"

#pragma comment(lib, "advapi32")
#pragma comment( lib, "ole32.lib" )

using namespace H265Enc;

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


#ifdef CMRT_EMU
extern "C" void ModeDecision(SurfaceIndex PARAM, SurfaceIndex SRC, SurfaceIndex REF0, SurfaceIndex MV_8,
                             SurfaceIndex MV_16, SurfaceIndex MV_32, SurfaceIndex MV_64, SurfaceIndex PRED_8,
                             SurfaceIndex PRED_16, SurfaceIndex PRED_32, SurfaceIndex PRED_64,
                             SurfaceIndex MODE_INFO, SurfaceIndex SCRATCHBUF);
const char genx_av1_mode_decision_hsw[1] = {};
const char genx_av1_mode_decision_bdw[1] = {};
const char genx_av1_mode_decision_skl[1] = {};
#else //CMRT_EMU
#include "../include/genx_av1_mode_decision_hsw_isa.h"
#include "../include/genx_av1_mode_decision_bdw_isa.h"
#include "../include/genx_av1_mode_decision_skl_isa.h"
#include "../include/genx_av1_mode_decision_icllp_isa.h"
#endif //CMRT_EMU

typedef decltype(&CM_ALIGNED_FREE) cm_aligned_deleter;

template <typename T>
std::unique_ptr<T, cm_aligned_deleter> make_aligned_ptr(size_t nelems, size_t align)
{
    if (void *p = CM_ALIGNED_MALLOC(nelems * sizeof(T), align))
        return std::unique_ptr<T, cm_aligned_deleter>(static_cast<T *>(p), CM_ALIGNED_FREE);
    throw std::bad_alloc();
}

struct new_mv_data_small {
    H265MV mv;
    uint32_t ref : 2;
    uint32_t sad : 30;
};

struct new_mv_data_large : public new_mv_data_small {
    int32_t reserved[14];
};

typedef std::unique_ptr<ModeInfo, cm_aligned_deleter> mode_info_ptr;

mode_info_ptr run_cpu(const H265VideoParam &par, Frame &curr, const Frame &ref0, const Frame &ref1);
mode_info_ptr run_gpu(const H265VideoParam &par, Frame &curr, const Frame &ref0, const Frame &ref1, bool print_time);
bool compare(const ModeInfo *mi_cpu, const ModeInfo *mi_gpu, bool print_error);

struct global_test_params_t {
    const char *yuv;
    int32_t width;
    int32_t height;
    int32_t compound_allowed;
    int32_t qp;
    int32_t layer;
    int32_t fast_test;

    global_test_params_t()
        : yuv("C:/yuv/1080p/park_joy_1920x1080_500_50.yuv")
        , width(1920)
        , height(1080)
        , compound_allowed(1)
        , qp(120)
        , layer(0)
        , fast_test(0)
    {}

    static void print_help(const char **argv)
    {
        char exe_name[256], exe_ext[256];
        _splitpath_s(argv[0], nullptr, 0, nullptr, 0, exe_name, 256, exe_ext, 256);
        printf("Usage examples:\n\n");
        printf("  1) To run with default parameters ("); global_test_params_t().print_params(); printf(")\n");
        printf("    %s%s\n\n", exe_name, exe_ext);
        printf("  2) Run with custom parameters\n");
        printf("    %s%s <yuvname> <width> <height> <compound_allowed> <qp> <layer> <fast_test>\n", exe_name, exe_ext);
    }

    void print_params() const
    {
        printf("%s %dx%d compound_allowed=%d qp=%d layer=%d fast=%d", yuv, width, height, compound_allowed, qp, layer, fast_test);
    }

    bool try_parse_params(int32_t argc, const char **argv)
    {
        if (argc == 8) {
            yuv = argv[1];
            width = atoi(argv[2]);
            height = atoi(argv[3]);
            compound_allowed = !!atoi(argv[4]);
            qp = atoi(argv[5]);
            layer = atoi(argv[6]);
            fast_test = atoi(argv[7]);
            return true;
        }
        else
            return false;
    }

} global_test_params;

const size_t ALIGNMENT = 32;

void parse_cmd(int argc, const char **argv)
{
    if (argc == 2) {
        if (_stricmp(argv[1], "--help") == 0 ||
            _stricmp(argv[1], "-h") == 0 ||
            _stricmp(argv[1], "-?") == 0)
        {
            global_test_params_t::print_help(argv);
            exit(0);
        }
    }

    global_test_params.try_parse_params(argc, argv);
}

void load_frame(std::fstream &f, const FrameData *frame)
{
    for (int32_t i = 0; i < frame->height; i++)
        if (!f.read((char *)frame->y + i * frame->pitch_luma_pix, frame->width))
            throw std::runtime_error("Failed to read from yuv file");

    for (int32_t i = 0; i < (frame->height + 1) / 2; i++)
        if (!f.read((char *)frame->uv + i * frame->pitch_chroma_pix, frame->width))
            throw std::runtime_error("Failed to read from yuv file");
}

void setup_video_params(H265VideoParam *par)
{
    par->Width = global_test_params.width;
    par->Height = global_test_params.height;
    par->miRows = (par->Height + 7) >> 3;
    par->miCols = (par->Width + 7) >> 3;
    par->sb64Rows = (par->miRows + 7) >> 3;
    par->sb64Cols = (par->miCols + 7) >> 3;
    par->miPitch = (par->sb64Cols << 3);
    par->codecType = CODEC_AV1;
    par->enableCmFlag = 1;
    par->hadamardMe = 0;
    par->PicWidthInCtbs = par->sb64Cols;
    par->PicHeightInCtbs = par->sb64Rows;
    par->MaxCUSize = 64;
    par->bitDepthLuma = 8;
    par->bitDepthChroma = 8;

    InitQuantizer(*par);

    SetupTileParamAv1EqualSize(par, 2, 1);
}

void randomize_mv_data(const H265VideoParam &par, int sz, int row, int col, Frame *cur)
{
    const int ref0_idx = LAST_FRAME;
    const int ref1_idx = cur->compoundReferenceAllowed ? ALTREF_FRAME : GOLDEN_FRAME;
    const int data_size = sz < 2 ? sizeof(new_mv_data_small) : sizeof(new_mv_data_large);
    const int pitch_mv_data = cur->m_feiInterData[0][sz]->m_pitch;
    new_mv_data_small *mv_data[2] = {
        (new_mv_data_small *)(cur->m_feiInterData[ref0_idx][sz]->m_sysmem + row * pitch_mv_data + col * data_size),
        (new_mv_data_small *)(cur->m_feiInterData[ref1_idx][sz]->m_sysmem + row * pitch_mv_data + col * data_size)
    };
    memset(mv_data[0], 0, data_size);
    memset(mv_data[1], 0, data_size);

    ALIGNED(32) uint8_t tmp_pred[64 * 64], tmp_pred2[64 * 64];

    mv_data[0]->mv.mvx = (rand() & 31) - 16;
    mv_data[0]->mv.mvy = (rand() & 31) - 16;
    mv_data[0]->ref = rand() % (cur->compoundReferenceAllowed ? 3 : 2);
    mv_data[1]->mv.mvx = (rand() & 31) - 16;
    mv_data[1]->mv.mvy = (rand() & 31) - 16;

    const int32_t y = row << (3 + sz);
    const int32_t x = col << (3 + sz);
    const int32_t pitch = cur->m_origin->pitch_luma_pix;
    const uint8_t *ref[2] = {
        cur->refFramesVp9[ref0_idx]->m_recon->y + y * pitch + x,
        cur->refFramesVp9[ref1_idx]->m_recon->y + y * pitch + x
    };
    const uint8_t *src = cur->m_origin->y + y * pitch + x;
    const int32_t best_ref = mv_data[0]->ref;

    if (best_ref < 2) {
        const H265MV mv = { int16_t(mv_data[best_ref]->mv.mvx << 1), int16_t(mv_data[best_ref]->mv.mvy << 1) };
        InterpolateAv1SingleRefLuma(ref[best_ref], pitch, tmp_pred, mv, 8 << sz, sz + 1, DEF_FILTER_DUAL);
    } else {
        const H265MV mv0 = { int16_t(mv_data[0]->mv.mvx << 1), int16_t(mv_data[0]->mv.mvy << 1) };
        const H265MV mv1 = { int16_t(mv_data[1]->mv.mvx << 1), int16_t(mv_data[1]->mv.mvy << 1) };
        InterpolateAv1SingleRefLuma(ref[0], pitch, tmp_pred,  mv0, 8 << sz, sz + 1, DEF_FILTER_DUAL);
        InterpolateAv1SingleRefLuma(ref[1], pitch, tmp_pred2, mv1, 8 << sz, sz + 1, DEF_FILTER_DUAL);
        VP9PP::average(tmp_pred, tmp_pred2, tmp_pred, sz + 1, sz + 1);
    }
    mv_data[0]->sad = VP9PP::sad_general(src, pitch, tmp_pred, 64, sz + 1, sz + 1);

    uint8_t *pred = cur->m_feiInterp[sz]->m_sysmem + y * cur->m_feiInterp[sz]->m_pitch + x;
    CopyNxN(tmp_pred, 64, pred, cur->m_feiInterp[sz]->m_pitch, 8 << sz);
}

void set_compound_qp_and_layer(const H265VideoParam &par, Frame *curr, Frame *ref0, Frame *ref1,
                               int32_t compound_allowed, int32_t qp, int32_t pyramidLayer)
{
    curr->m_sliceQpY = qp;
    int16_t q = vp9_dc_quant(qp, 0, par.bitDepthLuma);
    curr->m_lambda = 88 * q * q / 24.f / 512 / 16 / 128;
    curr->m_lambdaSatd = sqrt(curr->m_lambda * 512) / 512;
    curr->m_lambdaSatdInt = q * 1386; // == int(curr->m_lambdaSatd * (1<<24))
    curr->m_pyramidLayer = pyramidLayer;

    Zero(curr->isUniq);
#ifdef SINGLE_SIDED_COMPOUND
    curr->compoundReferenceAllowed = 1;
    curr->m_picCodeType = compound_allowed ? MFX_FRAMETYPE_B : MFX_FRAMETYPE_P;
    curr->refFramesVp9[0] = ref0;
    curr->refFramesVp9[1] = ref0;
    curr->refFramesVp9[2] = ref1;
    curr->isUniq[LAST_FRAME] = 1;
    curr->isUniq[GOLDEN_FRAME] = 0;
    curr->isUniq[ALTREF_FRAME] = 1;
#else
    curr->compoundReferenceAllowed = compound_allowed;
    if (curr->compoundReferenceAllowed) {
        curr->m_picCodeType = MFX_FRAMETYPE_B;
        curr->refFramesVp9[0] = ref0;
        curr->refFramesVp9[1] = ref0;
        curr->refFramesVp9[2] = ref1;
        curr->isUniq[LAST_FRAME] = 1;
        curr->isUniq[GOLDEN_FRAME] = 0;
        curr->isUniq[ALTREF_FRAME] = 1;
    }
    else {
        curr->m_picCodeType = MFX_FRAMETYPE_P;
        curr->refFramesVp9[0] = ref0;
        curr->refFramesVp9[1] = ref1;
        curr->refFramesVp9[2] = ref0;
        curr->isUniq[LAST_FRAME] = 1;
        curr->isUniq[GOLDEN_FRAME] = 1;
        curr->isUniq[ALTREF_FRAME] = 0;
    }
#endif
}

void setup_frames(const H265VideoParam &par, Frame *curr, Frame *ref0, Frame *ref1, std::fstream &f,
                  int32_t compound_allowed, int32_t qp, int32_t layer)
{
    curr->Create(&par);
    ref0->Create(&par);
    ref1->Create(&par);

    curr->ResetEncInfo();

    set_compound_qp_and_layer(par, curr, ref0, ref1, compound_allowed, qp, layer);

    curr->m_origin = new FrameData;
    ref0->m_recon = new FrameData;
    ref1->m_recon = new FrameData;

    FrameData::AllocInfo allocInfo;
    SetFrameDataAllocInfo(allocInfo, par.Width, par.Height, 128+16, par.fourcc, NULL, 0, 0);
    curr->m_origin->Create(allocInfo);
    ref0->m_recon->Create(allocInfo);
    ref1->m_recon->Create(allocInfo);

    curr->compFixedRef = ALTREF_FRAME;
    curr->compVarRef[0] = LAST_FRAME;
    curr->compVarRef[1] = GOLDEN_FRAME;

    load_frame(f, ref0->m_recon);
    load_frame(f, curr->m_origin);
    load_frame(f, ref1->m_recon);

    PadRectLuma(*ref0->m_recon, 0, 0, 0, par.Width, par.Height);
    PadRectLuma(*ref1->m_recon, 0, 0, 0, par.Width, par.Height);

    const int aligned_width  = (par.Width  + 63) & ~63;
    const int aligned_height = (par.Height + 63) & ~63;

    for (int sz = 0; sz < 4; sz++)
        for (int i = 0; i < (aligned_height >> (3 + sz)); i++)
            for (int j = 0; j < (aligned_width >> (3 + sz)); j++)
                randomize_mv_data(par, sz, i, j, curr);

    EstimateBits(curr->bitCount, curr->IsIntra(), par.codecType);
    GetRefFrameContextsAv1(curr);
}

void clear_tile_contexts(Frame *frame) {
    for (size_t i = 0; i < frame->m_tileContexts.size(); i++)
        frame->m_tileContexts[i].Clear();
}

int main(int argc, const char **argv)
{
    try {
        parse_cmd(argc, argv);
        print_hw_caps();

        printf("Running with ");
        global_test_params.print_params();
        printf("\n");

        const int32_t width = global_test_params.width;
        const int32_t height = global_test_params.height;
        const int32_t compound = global_test_params.compound_allowed;
        const int32_t layer = global_test_params.layer;
        const int32_t qp = global_test_params.qp;
        const int32_t frame_size = height * width;

        std::fstream f(global_test_params.yuv, std::ios_base::in | std::ios_base::binary);
        if (!f)
            throw std::runtime_error("Failed to open yuv file");

        srand(0x1234);
        VP9PP::initDispatcher(VP9PP::CPU_FEAT_AVX2, 1);

        H265VideoParam par;
        Frame curr;
        Frame ref0;
        Frame ref1;

        setup_video_params(&par);
        setup_frames(par, &curr, &ref0, &ref1, f, compound, qp, layer);

        int status = PASSED;

        printf("Running performance test with compound=%d qp=%3d layer=%d\n", global_test_params.compound_allowed, global_test_params.qp, global_test_params.layer);
        set_compound_qp_and_layer(par, &curr, &ref0, &ref1, global_test_params.compound_allowed, global_test_params.qp, global_test_params.layer);
        auto mi_gpu = run_gpu(par, curr, ref0, ref1, true);
        auto mi_cpu = run_cpu(par, curr, ref0, ref1);
        if (!compare(mi_cpu.get(), mi_gpu.get(), true)) {
            status = FAILED;
            printf("failed\n");
        }
        else {
            printf("passed\n");
        }

        if (!global_test_params.fast_test) {
            for (int32_t compound = 0; compound < 2; compound++) {
                for (int32_t qp = 30; qp < 255; qp += 50) {
                    for (int32_t layer = 0; layer < 4; layer++) {
                        printf("Testing compound=%d qp=%3d layer=%d: ", compound, qp, layer);
                        set_compound_qp_and_layer(par, &curr, &ref0, &ref1, compound, qp, layer);

                        auto mi_cpu = run_cpu(par, curr, ref0, ref1);
                        auto mi_gpu = run_gpu(par, curr, ref0, ref1, false);

                        if (!compare(mi_cpu.get(), mi_gpu.get(), false)) {
                            status = FAILED;
                            printf("failed\n");
                        }
                        else {
                            printf("passed\n");
                        }
                    }
                }
            }
        }

        delete curr.m_origin;
        delete ref0.m_recon;
        delete ref1.m_recon;

        printf(status == PASSED ? "PASSED\n" : "FAILED\n");
        return status;

    } catch (cm_error &e) {
        printf("CM_ERROR:\n");
        printf("  code: %d\n", e.errcode());
        printf("  file: %s\n", e.file());
        printf("  line: %d\n", e.line());
        if (const char *msg = e.what())
            printf("  message: %s\n", msg);
        return FAILED;
    } catch (std::exception &e) {
        printf("ERROR: %s\n", e.what());
        return FAILED;
    }
}


mode_info_ptr run_cpu(const H265VideoParam &par, Frame &curr, const Frame &ref0, const Frame &ref1)
{
    H265CU<uint8_t> *cu = (H265CU<uint8_t> *)H265_Malloc(sizeof(H265CU<uint8_t>));
    ModeInfo *temp_mi = (ModeInfo *)H265_Malloc(sizeof(ModeInfo) * 64 * 4); // 4 layers, 64 elements each
    int16_t *coefs = (CoeffsType *)H265_Malloc(sizeof(CoeffsType) * (64 * 64 * 3 / 2));

    Zero(*cu);

    mode_info_ptr mi = make_aligned_ptr<ModeInfo>(par.sb64Rows * 8 * par.miPitch, 32);
    memset(mi.get(), 0, sizeof(ModeInfo) * par.sb64Rows * 8 * par.miPitch);
    curr.cu_data = mi.get();
    clear_tile_contexts(&curr);

    for (int32_t row = 0, ctbAddr = 0; row < par.sb64Rows; row++) {
        for (int32_t col = 0; col < par.sb64Cols; col++, ctbAddr++) {
            ModeInfo *cu_data = curr.cu_data + ((col + row * par.miPitch) << 3);

            cu->InitCu(par, cu_data, temp_mi, row, col, curr, coefs);
            cu->ModeDecisionInterTu7<CODEC_AV1, 0>(row << 3, col << 3);

            const int32_t ctbColWithinTile = col - (cu->m_tileBorders.colStart >> 3);
            const int32_t ctbRowWithinTile = row - (cu->m_tileBorders.rowStart >> 3);
            const TileContexts &tileCtx = curr.m_tileContexts[ GetTileIndex(par.tileParam, row, col) ];
            As16B(tileCtx.aboveNonzero[0] + (ctbColWithinTile << 4)) = As16B(cu->m_contexts.aboveNonzero[0]);
            As8B (tileCtx.aboveNonzero[1] + (ctbColWithinTile << 3)) = As8B (cu->m_contexts.aboveNonzero[1]);
            As8B (tileCtx.aboveNonzero[2] + (ctbColWithinTile << 3)) = As8B (cu->m_contexts.aboveNonzero[2]);
            As16B(tileCtx.leftNonzero[0]  + (ctbRowWithinTile << 4)) = As16B(cu->m_contexts.leftNonzero[0]);
            As8B (tileCtx.leftNonzero[1]  + (ctbRowWithinTile << 3)) = As8B (cu->m_contexts.leftNonzero[1]);
            As8B (tileCtx.leftNonzero[2]  + (ctbRowWithinTile << 3)) = As8B (cu->m_contexts.leftNonzero[2]);
            As8B (tileCtx.abovePartition  + (ctbColWithinTile << 3)) = As8B (cu->m_contexts.abovePartition);
            As8B (tileCtx.leftPartition   + (ctbRowWithinTile << 3)) = As8B (cu->m_contexts.leftPartition);
        }
    }

    H265_Free(cu);
    H265_Free(temp_mi);
    H265_Free(coefs);

    return mi;
}

template <typename T> SurfaceIndex *get_index(T *cm_resource) {
    SurfaceIndex *index = 0;
    int res = cm_resource->GetIndex(index);
    THROW_CM_ERR(res);
    return index;
}

int get_mode_context(int nearest_ref_match, int ref_match, int has_new_mv, int coll_blk_avail)
{
    int mode_context = 0;
    if (coll_blk_avail == 0)
        mode_context = (1 << GLOBALMV_OFFSET);

    switch (nearest_ref_match) {
    case 0:
        mode_context |= (ref_match > 0) ? 1 : 0;
        if (ref_match == 1)
            mode_context |= (1 << REFMV_OFFSET);
        else if (ref_match >= 2)
            mode_context |= (2 << REFMV_OFFSET);
        break;
    case 1:
        mode_context |= (has_new_mv > 0) ? 2 : 3;
        if (ref_match == 1)
            mode_context |= (3 << REFMV_OFFSET);
        else if (ref_match >= 2)
            mode_context |= (4 << REFMV_OFFSET);
        break;
    case 2:
    default:
        mode_context |= (has_new_mv > 0) ? 4 : 6;
        mode_context |= (5 << REFMV_OFFSET);
        break;
    }
    return mode_context;
}

void setup_param_for_gpu(const H265VideoParam &par, const Frame &frame, H265VideoParam *param_sys)
{
    memcpy(param_sys, &par, sizeof(par));
    param_sys->qparam = param_sys->qparamY[frame.m_sliceQpY];
    param_sys->qparam.round[0] = (40 * param_sys->qparam.dequant[0]) >> 7;
    param_sys->qparam.round[1] = (40 * param_sys->qparam.dequant[1]) >> 7;
    param_sys->lambda = frame.m_lambda;
    param_sys->lambdaInt = uint32_t(frame.m_lambdaSatd * 2048 + 0.5);
    param_sys->compound_allowed = frame.compoundReferenceAllowed;
    if (frame.compoundReferenceAllowed) {
        param_sys->bidir_compound_or_single_ref = (frame.m_picCodeType == MFX_FRAMETYPE_B);
    } else {
        param_sys->bidir_compound_or_single_ref = 0;
    }
    const int qctx = get_q_ctx(frame.m_sliceQpY);
    const TxbBitCounts &tbc = frame.bitCount.txb[qctx][TX_8X8][PLANE_TYPE_Y];
    for (int i = 0; i < 7; i++)
        param_sys->bc.eobMulti[i] = tbc.eobMulti[i];
    param_sys->bc.coeffEobDc[0] = 0;
    for (int i = 1; i < 16; i++)
        param_sys->bc.coeffEobDc[i] = tbc.coeffBaseEob[0][IPP_MIN(3,i) - 1] + tbc.coeffBrAcc[0][i];
    for (int i = 0; i < 3; i++)
        param_sys->bc.coeffBaseEobAc[i] = tbc.coeffBaseEob[1][i];
    for (int i = 0; i < 13; i++)
        for (int j = 0; j < 2; j++)
            param_sys->bc.txbSkip[i][j] = tbc.txbSkip[i][j];
    for (int ctx_skip = 0; ctx_skip < 3; ctx_skip++) {
        param_sys->bc.skip[ctx_skip][0] = frame.bitCount.skip[ctx_skip][0];
        param_sys->bc.skip[ctx_skip][1] = frame.bitCount.skip[ctx_skip][1];
    }
    for (int i = 0; i < 16; i++)
        for (int j = 0; j < 4; j++)
            param_sys->bc.coeffBase[i][j] = tbc.coeffBase[i][j];
    for (int i = 0; i < 21; i++)
        for (int j = 0; j < 16; j++)
            param_sys->bc.coeffBrAcc[i][j] = tbc.coeffBrAcc[i][j];
    memcpy(param_sys->bc.refFrames, frame.m_refFrameBitsAv1, sizeof(param_sys->bc.refFrames));

    const auto &newmv_bits  = frame.bitCount.newMvBit;
    const auto &globmv_bits = frame.bitCount.zeroMvBit;
    const auto &refmv_bits  = frame.bitCount.refMvBit;

    for (int nearest_ref_match = 0; nearest_ref_match < 3; nearest_ref_match++) {
        for (int ref_match = 0; ref_match < 3; ref_match++) {
            for (int has_new_mv = 0; has_new_mv < 2; has_new_mv++) {
                const int mode_ctx   = get_mode_context(nearest_ref_match, ref_match, has_new_mv, 1);
                const int newmv_ctx  = (mode_ctx & NEWMV_CTX_MASK);
                const int globmv_ctx = (mode_ctx >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;
                const int refmv_ctx  = (mode_ctx >> REFMV_OFFSET) & REFMV_CTX_MASK;
                const int comp_mode_ctx = (refmv_ctx >> 1) * COMP_NEWMV_CTXS + IPP_MIN(newmv_ctx, COMP_NEWMV_CTXS - 1);

                auto &single_modes = param_sys->bc.singleModes[nearest_ref_match][ref_match][has_new_mv];
                auto &comp_modes   = param_sys->bc.compModes[nearest_ref_match][ref_match][has_new_mv];

                single_modes[NEARESTMV_] = newmv_bits[newmv_ctx][1] + globmv_bits[globmv_ctx][1] + refmv_bits[refmv_ctx][0];
                single_modes[NEARMV_   ] = newmv_bits[newmv_ctx][1] + globmv_bits[globmv_ctx][1] + refmv_bits[refmv_ctx][1];
                single_modes[ZEROMV_   ] = newmv_bits[newmv_ctx][1] + globmv_bits[globmv_ctx][0];
                single_modes[NEWMV_    ] = newmv_bits[newmv_ctx][0];

                comp_modes[NEARESTMV_] = frame.bitCount.interCompMode[comp_mode_ctx][NEAREST_NEARESTMV - NEAREST_NEARESTMV];
                comp_modes[NEARMV_   ] = frame.bitCount.interCompMode[comp_mode_ctx][NEAR_NEARMV       - NEAREST_NEARESTMV];
                comp_modes[ZEROMV_   ] = frame.bitCount.interCompMode[comp_mode_ctx][ZERO_ZEROMV       - NEAREST_NEARESTMV];
                comp_modes[NEWMV_    ] = frame.bitCount.interCompMode[comp_mode_ctx][NEW_NEWMV         - NEAREST_NEARESTMV];
            }
        }
    }

    const float LUT_IEF_SPLIT_MT[4][4] = {
        { 2.587840f, 7.278976f, 10.856256f, 10.856256f },
        { 6.872320f, 7.278976f, 10.856256f, 10.856256f },
        { 7.239296f, 7.278976f, 10.856256f, 10.856256f },
        { 7.646336f, 9.961600f, 10.856256f, 10.856256f },
    };

    const float LUT_IEF_SPLIT_ST[4][4] = {
        { 20.f,  77.150f, 142.184f, 142.184f },
        { 20.f,  89.805f, 142.184f, 142.184f },
        { 20.f,  89.805f, 249.454f, 249.454f },
        { 20.f, 249.454f, 249.454f, 249.454f },
    };

    const float LUT_IEF_SPLIT_A[4][4] = {
        { 0.303627f, 0.423133f, 0.397384f, 0.397384f },
        { 0.315012f, 0.435791f, 0.397396f, 0.397396f },
        { 0.315445f, 0.443448f, 0.500000f, 0.500000f },
        { 0.314350f, 0.456550f, 0.500000f, 0.500000f },
    };

    const float LUT_IEF_SPLIT_B[4][4] = {
        { 0.547588f, 0.022428f, 0.473092f, 0.473092f },
        { 0.572353f, 0.041593f, 0.527727f, 0.527727f },
        { 0.677961f, 0.203189f, 0.000000f, 0.000000f },
        { 0.794845f, 0.228990f, 0.000000f, 0.000000f },
    };

    const float LUT_IEF_SPLIT_M[4][4] = {
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
#ifdef FAST_MODE
    param_sys->fastMode = 1;
#else
    param_sys->fastMode = 0;
#endif
    for (int qIdx = 0; qIdx < 4; qIdx++) {
        for (int qpLayer = 0; qpLayer < 4; qpLayer++) {
            param_sys->lutIef[qIdx][qpLayer].split.mt = LUT_IEF_SPLIT_MT[qIdx][qpLayer];
            param_sys->lutIef[qIdx][qpLayer].split.st = LUT_IEF_SPLIT_ST[qIdx][qpLayer];
            param_sys->lutIef[qIdx][qpLayer].split.a  = LUT_IEF_SPLIT_A[qIdx][qpLayer];
            param_sys->lutIef[qIdx][qpLayer].split.b  = LUT_IEF_SPLIT_B[qIdx][qpLayer];
            param_sys->lutIef[qIdx][qpLayer].split.m  = LUT_IEF_SPLIT_M[qIdx][qpLayer];

            param_sys->lutIef[qIdx][qpLayer].leaf.a = LUT_IEF_LEAF_A[qIdx][qpLayer];
            param_sys->lutIef[qIdx][qpLayer].leaf.b = LUT_IEF_LEAF_B[qIdx][qpLayer];
            param_sys->lutIef[qIdx][qpLayer].leaf.m = LUT_IEF_LEAF_M[qIdx][qpLayer];
        }
    }
}

mode_info_ptr run_gpu(const H265VideoParam &par, Frame &frame_curr, const Frame &frame_ref0, const Frame &frame_ref1, bool print_time)
{
    using namespace H265Enc;

    mfxU32 version = 0;
    CmDevice *device = nullptr;
    CmProgram *program = nullptr;
    CmKernel *kernel = nullptr;
    CmTask *task = nullptr;
    CmQueue *queue = nullptr;
    CmThreadSpace *ts = nullptr;
    CmEvent *e = nullptr;
    CmSurface2D *scratchbuf = nullptr;
    CmSurface2D *src = nullptr;
    CmSurface2D *ref0 = nullptr;
    CmSurface2D *ref1 = nullptr;
    CmSurface2D *newmv[2][4] = {};
    CmSurface2D *newmv_pred[4] = {};
    CmSurface2D *Rs16 = nullptr;
    CmSurface2D *Cs16 = nullptr;
    CmSurface2D *Rs32 = nullptr;
    CmSurface2D *Cs32 = nullptr;
    CmSurface2D *Rs64 = nullptr;
    CmSurface2D *Cs64 = nullptr;
    CmSurface2DUP *mi = nullptr;
    CmBufferUP *param = nullptr;
    auto param_sys = make_aligned_ptr<H265VideoParam>(1, 4096);
    uint32_t tsw = par.tileParam.cols;
    uint32_t tsh = par.tileParam.rows;
    int32_t res = CM_SUCCESS;
    GPU_PLATFORM hw_type;
    size_t size = sizeof(mfxU32);

    THROW_CM_ERR(CreateCmDevice(device, version));
    THROW_CM_ERR(device->InitPrintBuffer());
    THROW_CM_ERR(device->GetCaps(CAP_GPU_PLATFORM, size, &hw_type));
    THROW_CM_ERR(device->CreateQueue(queue));
#ifdef CMRT_EMU
    hw_type = PLATFORM_INTEL_HSW;
#endif // CMRT_EMU
    switch (hw_type) {
    case PLATFORM_INTEL_HSW:
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_mode_decision_hsw, sizeof(genx_av1_mode_decision_hsw), program, "nojitter"));
        break;
    case PLATFORM_INTEL_BDW:
    case PLATFORM_INTEL_CHV:
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_mode_decision_bdw, sizeof(genx_av1_mode_decision_bdw), program, "nojitter"));
        break;
    case PLATFORM_INTEL_SKL:
        //case PLATFORM_INTEL_KBL:
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_mode_decision_skl, sizeof(genx_av1_mode_decision_skl), program, "nojitter"));
        break;
    case PLATFORM_INTEL_ICLLP:
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_mode_decision_icllp, sizeof(genx_av1_mode_decision_icllp), program, "nojitter"));
        break;
    default:
        throw cm_error(CM_FAILURE, __FILE__, __LINE__, "Unknown HW type");
    }
    THROW_CM_ERR(device->CreateKernel(program, CM_KERNEL_FUNCTION(ModeDecision), kernel));
    THROW_CM_ERR(device->CreateTask(task));
    THROW_CM_ERR(device->CreateBufferUP(sizeof(H265VideoParam), param_sys.get(), param));
    setup_param_for_gpu(par, frame_curr, param_sys.get());

    THROW_CM_ERR(device->CreateSurface2D(par.Width, par.Height*2, CM_SURFACE_FORMAT_A8, scratchbuf));
    THROW_CM_ERR(device->CreateSurface2D(par.Width, par.Height, CM_SURFACE_FORMAT_NV12, src));
    THROW_CM_ERR(device->CreateSurface2D(par.Width, par.Height, CM_SURFACE_FORMAT_NV12, ref0));
    THROW_CM_ERR(device->CreateSurface2D(par.Width, par.Height, CM_SURFACE_FORMAT_NV12, ref1));
    THROW_CM_ERR(src->WriteSurfaceStride(frame_curr.m_origin->y, nullptr, frame_curr.m_origin->pitch_luma_bytes));
    THROW_CM_ERR(ref0->WriteSurfaceStride(frame_ref0.m_recon->y, nullptr, frame_ref0.m_recon->pitch_luma_bytes));
    THROW_CM_ERR(ref1->WriteSurfaceStride(frame_ref1.m_recon->y, nullptr, frame_ref1.m_recon->pitch_luma_bytes));

    int refs[2] = { LAST_FRAME, frame_curr.compoundReferenceAllowed ? ALTREF_FRAME : GOLDEN_FRAME };
    for (int r = 0; r < 2; r++) {
        for (int sz = 0; sz < 4; sz++) {
            const int data_size = sz < 2 ? 8 : 64;
            const int width_in_blk  = par.sb64Cols << (3 - sz);
            const int height_in_blk = par.sb64Rows << (3 - sz);
            const FeiOutData *data = frame_curr.m_feiInterData[refs[r]][sz];
            THROW_CM_ERR(device->CreateSurface2D(width_in_blk * data_size, height_in_blk, CM_SURFACE_FORMAT_A8, newmv[r][sz]));
            THROW_CM_ERR(newmv[r][sz]->WriteSurfaceStride(data->m_sysmem, nullptr, data->m_pitch));
        }
    }

    for (int sz = 0; sz < 4; sz++) {
        const int size = par.sb64Cols * par.sb64Rows * 64 * 64;
        const FeiOutData *data = frame_curr.m_feiInterp[sz];
        THROW_CM_ERR(device->CreateSurface2D(par.sb64Cols * 64, par.sb64Rows * 64, CM_SURFACE_FORMAT_A8, newmv_pred[sz]));
        THROW_CM_ERR(newmv_pred[sz]->WriteSurfaceStride(data->m_sysmem, nullptr, data->m_pitch));

    }

    const uint32_t width_mi = par.sb64Cols * 8;
    const uint32_t height_mi = par.sb64Rows * 8;
    uint32_t pitch_mi;
    uint32_t size_mi;
    THROW_CM_ERR(device->GetSurface2DInfo(width_mi * sizeof(ModeInfo), height_mi, CM_SURFACE_FORMAT_A8, pitch_mi, size_mi));
    mode_info_ptr mi_sys = make_aligned_ptr<ModeInfo>(size_mi, 0x1000);
    memset(mi_sys.get(), 0, sizeof(ModeInfo) * size_mi);
    THROW_CM_ERR(device->CreateSurface2DUP(width_mi * sizeof(ModeInfo), height_mi, CM_SURFACE_FORMAT_A8, mi_sys.get(), mi));

    THROW_CM_ERR(device->CreateSurface2D(4 * par.sb64Cols * sizeof(int), 4 * par.sb64Rows, CM_SURFACE_FORMAT_A8, Rs16));
    THROW_CM_ERR(device->CreateSurface2D(4 * par.sb64Cols * sizeof(int), 4 * par.sb64Rows, CM_SURFACE_FORMAT_A8, Cs16));
    int32_t* rscsk16 = (int32_t*)malloc(4 * par.sb64Cols * 4 * par.sb64Rows * sizeof(int));
    for (int j = 0; j < 4 * par.sb64Rows; j++)
        for (int k = 0; k < 4 * par.sb64Cols; k++)
            rscsk16[(j * 4 * par.sb64Cols) + k] = 2560;// +k + j;
    THROW_CM_ERR(Rs16->WriteSurfaceStride((const unsigned char *)rscsk16, nullptr, 4 * par.sb64Cols * sizeof(int)));
    THROW_CM_ERR(Cs16->WriteSurfaceStride((const unsigned char *)rscsk16, nullptr, 4 * par.sb64Cols * sizeof(int)));
    free(rscsk16);

    THROW_CM_ERR(device->CreateSurface2D(2 * par.sb64Cols * sizeof(int), 2 * par.sb64Rows, CM_SURFACE_FORMAT_A8, Rs32));
    THROW_CM_ERR(device->CreateSurface2D(2 * par.sb64Cols * sizeof(int), 2 * par.sb64Rows, CM_SURFACE_FORMAT_A8, Cs32));
    int32_t* rscsk32 = (int32_t*)malloc(2 * par.sb64Cols * 2 * par.sb64Rows * sizeof(int));
    for (int j = 0; j < 2 * par.sb64Rows; j++)
        for (int k = 0; k < 2 * par.sb64Cols; k++)
            rscsk32[(j * 2 * par.sb64Cols) + k] = 5120;// +k + j;
    THROW_CM_ERR(Rs32->WriteSurfaceStride((const unsigned char *)rscsk32, nullptr, 2 * par.sb64Cols * sizeof(int)));
    THROW_CM_ERR(Cs32->WriteSurfaceStride((const unsigned char *)rscsk32, nullptr, 2 * par.sb64Cols * sizeof(int)));
    free(rscsk32);

    THROW_CM_ERR(device->CreateSurface2D(par.sb64Cols * sizeof(int), par.sb64Rows, CM_SURFACE_FORMAT_A8, Rs64));
    THROW_CM_ERR(device->CreateSurface2D(par.sb64Cols * sizeof(int), par.sb64Rows, CM_SURFACE_FORMAT_A8, Cs64));
    int32_t* rscsk64 = (int32_t*)malloc(par.sb64Cols*par.sb64Rows * sizeof(int));
    for (int k = 0; k < par.sb64Cols*par.sb64Rows; k++)
        rscsk64[k] = 20480;
    THROW_CM_ERR(Rs64->WriteSurfaceStride((const unsigned char *)rscsk64, nullptr, par.sb64Cols * sizeof(int)));
    THROW_CM_ERR(Cs64->WriteSurfaceStride((const unsigned char *)rscsk64, nullptr, par.sb64Cols * sizeof(int)));
    free(rscsk64);

    vector<SurfaceIndex, 8> newmv_ids;
    vector<SurfaceIndex, 8> ref_ids;
    vector<SurfaceIndex, 8> rscs_ids;
    rscs_ids[0] = *get_index(Rs16);
    rscs_ids[1] = *get_index(Cs16);
    rscs_ids[2] = *get_index(Rs32);
    rscs_ids[3] = *get_index(Cs32);
    rscs_ids[4] = *get_index(Rs64);
    rscs_ids[5] = *get_index(Cs64);

    newmv_ids[0] = *get_index(newmv[0][0]);
    newmv_ids[1] = *get_index(newmv[0][1]);
    newmv_ids[2] = *get_index(newmv[0][2]);
    newmv_ids[3] = *get_index(newmv[0][3]);
    newmv_ids[4] = *get_index(newmv[1][0]);
    newmv_ids[5] = *get_index(newmv[1][1]);
    newmv_ids[6] = *get_index(newmv[1][2]);
    newmv_ids[7] = *get_index(newmv[1][3]);
    ref_ids[0] = *get_index(ref0);
    ref_ids[1] = *get_index(ref1);
    ref_ids[2] = *get_index(newmv_pred[0]);
    ref_ids[3] = *get_index(newmv_pred[1]);
    ref_ids[4] = *get_index(newmv_pred[2]);
    ref_ids[5] = *get_index(newmv_pred[3]);
    ref_ids[6] = *get_index(scratchbuf);

    uint32_t qpLayer = (frame_curr.m_sliceQpY << 2) + frame_curr.m_pyramidLayer;

    THROW_CM_ERR(kernel->SetKernelArg(0, sizeof(SurfaceIndex), get_index(param)));
    THROW_CM_ERR(kernel->SetKernelArg(1, sizeof(SurfaceIndex), get_index(src)));
    THROW_CM_ERR(kernel->SetKernelArg(2, ref_ids.get_size_data(),   ref_ids.get_addr_data()));
    THROW_CM_ERR(kernel->SetKernelArg(3, newmv_ids.get_size_data(), newmv_ids.get_addr_data()));
    THROW_CM_ERR(kernel->SetKernelArg(4, sizeof(SurfaceIndex), get_index(mi)));
    THROW_CM_ERR(kernel->SetKernelArg(5, rscs_ids.get_size_data(), rscs_ids.get_addr_data()));
    THROW_CM_ERR(kernel->SetKernelArg(6, sizeof(qpLayer), &qpLayer));
    THROW_CM_ERR(task->AddKernel(kernel));
    THROW_CM_ERR(kernel->SetThreadCount(tsw * tsh));
    THROW_CM_ERR(device->CreateThreadSpace(tsw, tsh, ts));

    THROW_CM_ERR(ts->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY));

    THROW_CM_ERR(queue->Enqueue(task, e, ts));
    THROW_CM_ERR(e->WaitForTaskFinished());
    THROW_CM_ERR(device->FlushPrintBuffer());

#ifndef CMRT_EMU
    if (print_time)
        printf("TIME = %.3f ms\n", GetAccurateGpuTime(queue, task, ts) / 1000000.);
#endif //CMRT_EMU

    queue->DestroyEvent(e);
    device->DestroyThreadSpace(ts);
    for (int r = 0; r < 1; r++)
        for (int sz = 0; sz < 4; sz++)
            device->DestroySurface(newmv[r][sz]);
    for (int sz = 0; sz < 4; sz++)
        device->DestroySurface(newmv_pred[sz]);
    device->DestroySurface2DUP(mi);
    device->DestroySurface(scratchbuf);
    device->DestroySurface(ref0);
    device->DestroySurface(src);
    device->DestroySurface(Rs32);
    device->DestroySurface(Cs32);
    device->DestroySurface(Rs64);
    device->DestroySurface(Cs64);
    device->DestroyBufferUP(param);
    device->DestroyTask(task);
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    DestroyCmDevice(device);

    mode_info_ptr mi_out = make_aligned_ptr<ModeInfo>(par.sb64Rows * 8 * par.miPitch * sizeof(ModeInfo), 32);
    for (int i = 0; i < par.sb64Rows * 8; i++)
        memcpy(mi_out.get() + i * par.miPitch,
               (char *)mi_sys.get() + i * pitch_mi,
               par.sb64Cols * 8 * sizeof(ModeInfo));

    return mi_out;
}

void report_mode_info_difference(int32_t mi_row, int32_t mi_col, const ModeInfo &cpu, const ModeInfo &gpu)
{
    #define DUMP_MODE_INFO_FIELD(name) \
        printf("%-10s %6d %6d\n", #name, cpu.name, gpu.name)

    printf("\n");
    printf("ModeInfo at (mi_row=%d, mi_col=%d) differs\n", mi_row, mi_col);
    printf("%10s %6s %6s\n", "", "cpu", "gpu");
    DUMP_MODE_INFO_FIELD(mv[0].mvx);
    DUMP_MODE_INFO_FIELD(mv[0].mvy);
    DUMP_MODE_INFO_FIELD(mv[1].mvx);
    DUMP_MODE_INFO_FIELD(mv[1].mvy);
    DUMP_MODE_INFO_FIELD(refIdx[0]);
    DUMP_MODE_INFO_FIELD(refIdx[1]);
    DUMP_MODE_INFO_FIELD(refIdxComb);
    DUMP_MODE_INFO_FIELD(sbType);
    DUMP_MODE_INFO_FIELD(mode);
    DUMP_MODE_INFO_FIELD(skip & 1);
    DUMP_MODE_INFO_FIELD(sad);
    printf("\n");
}

bool compare(const ModeInfo *mi_cpu, const ModeInfo *mi_gpu, bool print_error)
{
    H265VideoParam par = {};
    setup_video_params(&par);

    for (int32_t i = 0; i < par.sb64Rows * 8; i++) {
        for (int32_t j = 0; j < par.sb64Cols * 8; j++) {
            const ModeInfo &cpu = mi_cpu[i * par.miPitch + j];
            const ModeInfo &gpu = mi_gpu[i * par.miPitch + j];
            if (cpu.mv.asInt64 != gpu.mv.asInt64 ||
                cpu.refIdx[0]  != gpu.refIdx[0]  ||
                cpu.refIdx[1]  != gpu.refIdx[1]  ||
                cpu.refIdxComb != gpu.refIdxComb ||
                cpu.sbType     != gpu.sbType     ||
                cpu.mode       != gpu.mode       ||
                (cpu.skip & 1) != (gpu.skip & 1) ||
                cpu.sad        != gpu.sad)
            {
                if (print_error)
                    report_mode_info_difference(i, j, cpu, gpu);
                return false;
            }
        }
    }

    return true;
}
