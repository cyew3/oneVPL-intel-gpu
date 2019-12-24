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
#include "av1_intra/mfx_av1_get_intra_pred_pels.h"
#include "av1_intra/mfx_av1_dispatcher_wrappers.h"

#if (defined(__INTEL_COMPILER) || defined(_MSC_VER)) && !defined(_WIN32_WCE)
  #define ALIGNED(X) __declspec (align(X))
#elif defined(__GNUC__)
  #define ALIGNED(X) __attribute__ ((aligned(X)))
#endif

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
extern "C" void CheckIntra(SurfaceIndex SRC, SurfaceIndex MODE_INFO);
const char genx_av1_intra_hsw[1] = {};
const char genx_av1_intra_bdw[1] = {};
const char genx_av1_intra_skl[1] = {};
const char genx_av1_intra_cnl[1] = {};
const char genx_av1_intra_icl[1] = {};
#else //CMRT_EMU
#include "../include/genx_av1_intra_hsw_isa.h"
#include "../include/genx_av1_intra_bdw_isa.h"
#include "../include/genx_av1_intra_skl_isa.h"
#include "../include/genx_av1_intra_icllp_isa.h"
//#include "../include/genx_av1_intra_tgl_isa.h"
#endif //CMRT_EMU

typedef decltype(&CM_ALIGNED_FREE) cm_aligned_deleter;

template <typename T>
std::unique_ptr<T, cm_aligned_deleter> make_aligned_ptr(size_t nelems, size_t align = 1)
{
    if (void *p = CM_ALIGNED_MALLOC(nelems * sizeof(T), align))
        return std::unique_ptr<T, cm_aligned_deleter>(static_cast<T *>(p), CM_ALIGNED_FREE);
    throw std::bad_alloc();
}

struct params {
    unsigned short miCols;
    unsigned short miRows;
    unsigned short lambdaInt;
};

struct intra_info {
    unsigned int mode : 4;
    unsigned int sad  : 28;
};

struct frame_t
{
    frame_t() : data(nullptr), pitch(0), width(0), height(0) {}

    frame_t(int width, int height) { create(width, height); }

    ~frame_t() { destroy(); }

    void create(int width, int height)
    {
        const int aligned_width  = (width  + 63) & ~63;
        const int aligned_height = (height + 63) & ~63;
        void *p = CM_ALIGNED_MALLOC(aligned_width * aligned_height, 0x1000);
        if (!p)
            throw std::bad_alloc();

        data = (unsigned char *)p;
        pitch = aligned_width;
        this->width = width;
        this->height = height;
    }

    void destroy()
    {
        CM_ALIGNED_FREE(data);
        data = nullptr;
        pitch = 0;
        width = 0;
        height = 0;
    }

    unsigned char *data;
    unsigned int   pitch;
    unsigned int   width;
    unsigned int   height;
};


union H265MV {
    struct { int16_t mvx, mvy; };
    uint32_t asInt;
};
inline bool operator==(const H265MV &l, const H265MV &r) { return l.asInt == r.asInt; }
inline bool operator!=(const H265MV &l, const H265MV &r) { return l.asInt != r.asInt; }

struct H265MVPair {
    H265MV &operator[](int32_t i) { assert(i==0 || i==1); return mv[i]; }
    const H265MV &operator[](int32_t i) const { assert(i==0 || i==1); return mv[i]; }
    operator H265MV *() { return mv; }
    operator const H265MV *() const { return mv; }
    union {
        H265MV mv[2];
        uint64_t asInt64;
    };
};
inline bool operator==(const H265MVPair &l, const H265MVPair &r) { return l.asInt64 == r.asInt64; }
inline bool operator!=(const H265MVPair &l, const H265MVPair &r) { return l.asInt64 != r.asInt64; }

typedef uint8_t BlockSize;
typedef uint8_t PredMode;
typedef uint8_t TxSize;
typedef uint8_t TxType;
typedef uint8_t InterpFilter;
typedef intra_info MemCtx;

struct ModeInfo
{
public:
    H265MVPair mv;
    H265MVPair mvd;
    int8_t      refIdx[2];
    int8_t      refIdxComb;   // -1..4

    // VP9 data
    BlockSize sbType;
    PredMode  mode;
    PredMode  modeUV;

    TxSize txSize;
    struct {
        TxSize minTxSize : 4;
        TxType txType    : 4;
    };

    uint8_t skip;

    union {
        struct {
                InterpFilter interp0 : 4;
                InterpFilter interp1 : 4;
        };
        InterpFilter interp;
    };

    struct {
        uint8_t refMvIdx       : 2;
        uint8_t angle_delta_y  : 3;
        uint8_t angle_delta_uv : 3;
    };

    struct {
        uint8_t drl0 : 2;
        uint8_t drl1 : 2;
        uint8_t nmv0 : 2;
        uint8_t nmv1 : 2;
    } memCtxAV1_;

    MemCtx memCtx; // 4 bytes
};

typedef ModeInfo mode_info_t;

typedef std::unique_ptr<mode_info_t, cm_aligned_deleter> mode_info_ptr;


void run_cpu(const params &par, const frame_t &frame, mode_info_t *mode_info, int early_exit);
void run_gpu(const params &par, const frame_t &frame, mode_info_t *mode_info, int early_exit);
bool compare(const params &par, const mode_info_t *ii_cpu, const mode_info_t *ii_gpu);

namespace global_test_params {
    char *yuv = "C:/yuv/1080p/park_joy_1920x1080_500_50.yuv";
    int32_t width  = 1920;
    int32_t height = 1080;
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
            printf("  1) Run with default parameters (yuv=%s width=%d height=%d)\n",
                global_test_params::yuv, global_test_params::width, global_test_params::height);
            printf("    %s%s\n\n", exe_name, exe_ext);
            printf("  2) Run with custom parameters\n");
            printf("    %s%s <yuvname> <width> <height>\n", exe_name, exe_ext);
            exit(0);
        }
    }

    if (argc == 4) {
        global_test_params::yuv    = argv[1];
        global_test_params::width  = atoi(argv[2]);
        global_test_params::height = atoi(argv[3]);
    }
}

void load_frame(std::fstream &f, frame_t *frame)
{
    for (int32_t i = 0; i < frame->height; i++)
        if (!f.read((char *)frame->data + i * frame->pitch, frame->width))
            throw std::runtime_error("Failed to read from yuv file");
}

void setup_video_params(params *par)
{
    par->miRows = (global_test_params::height + 7) >> 3;
    par->miCols = (global_test_params::width + 7) >> 3;
    par->lambdaInt = 5;
}

void setup_frames(const params &par, frame_t *frame, std::fstream &f)
{
    frame->create(par.miCols * 8, par.miRows * 8);

    load_frame(f, frame);
}


void generate_sbtype(const params &par, mode_info_t *mode_info, int mi_row, int mi_col, int depth)
{
    const int mi_pitch = (par.miCols + 7) & ~7;

    if (mi_row >= par.miRows || mi_col >= par.miCols) {
        mode_info[mi_row * mi_pitch + mi_col].sbType = (4 - depth) * 3;
        mode_info[mi_row * mi_pitch + mi_col].mode   = 255; // out of picture
        return;
    }

    const int s = 8 >> depth;
    const int ss = s >> 1;
    const int must_split = (mi_row + s > par.miRows || mi_col + s > par.miCols);
    const int non_split = depth >= 1 && !must_split && (rand() % (4 - depth) == 0);
    if (non_split) {
        mode_info[mi_row * mi_pitch + mi_col].sbType = (4 - depth) * 3;
        mode_info[mi_row * mi_pitch + mi_col].mode   = 0;
        *((int*)&mode_info[mi_row * mi_pitch + mi_col].memCtx) = rand() & 0xf;
        if (depth == 0) {
            mode_info[(mi_row +  0) * mi_pitch + mi_col + ss].sbType = (4 - depth) * 3;
            mode_info[(mi_row + ss) * mi_pitch + mi_col +  0].sbType = (4 - depth) * 3;
            mode_info[(mi_row + ss) * mi_pitch + mi_col + ss].sbType = (4 - depth) * 3;
            mode_info[(mi_row +  0) * mi_pitch + mi_col + ss].mode   = 0;
            mode_info[(mi_row + ss) * mi_pitch + mi_col +  0].mode   = 0;
            mode_info[(mi_row + ss) * mi_pitch + mi_col + ss].mode   = 0;
            *((int*)&mode_info[(mi_row +  0) * mi_pitch + mi_col + ss].memCtx) = *((int*)&mode_info[mi_row * mi_pitch + mi_col].memCtx);
            *((int*)&mode_info[(mi_row + ss) * mi_pitch + mi_col +  0].memCtx) = *((int*)&mode_info[mi_row * mi_pitch + mi_col].memCtx);
            *((int*)&mode_info[(mi_row + ss) * mi_pitch + mi_col + ss].memCtx) = *((int*)&mode_info[mi_row * mi_pitch + mi_col].memCtx);
        }
        return;
    }

    generate_sbtype(par, mode_info, mi_row + 0,  mi_col + 0,  depth + 1);
    generate_sbtype(par, mode_info, mi_row + 0,  mi_col + ss, depth + 1);
    generate_sbtype(par, mode_info, mi_row + ss, mi_col + 0,  depth + 1);
    generate_sbtype(par, mode_info, mi_row + ss, mi_col + ss, depth + 1);
}

void generate_mode_info(const params &par, mode_info_t *mode_info)
{
    const int sb_rows = (par.miRows + 7) >> 3;
    const int sb_cols = (par.miCols + 7) >> 3;
    const int mi_pitch = 8 * sb_cols;
    for (int sbr = 0; sbr < sb_rows; sbr++)
        for (int sbc = 0; sbc < sb_cols; sbc++)
            generate_sbtype(par, mode_info, 8 * sbr, 8 * sbc, 0);
}

int main(int argc, char **argv)
{
    try {
        parse_cmd(argc, argv);
        print_hw_caps();

        printf("Running with %s %dx%d\n", global_test_params::yuv,
            global_test_params::width, global_test_params::height);

        const int32_t width = global_test_params::width;
        const int32_t height = global_test_params::height;
        const int32_t frame_size = height * width;

        std::fstream f(global_test_params::yuv, std::ios_base::in | std::ios_base::binary);
        if (!f)
            throw std::runtime_error("Failed to open yuv file");

        srand(0x1234);

        params par;
        frame_t frame;

        setup_video_params(&par);
        setup_frames(par, &frame, f);

        const int sb_rows = (par.miRows + 7) >> 3;
        const int sb_cols = (par.miCols + 7) >> 3;

        auto mi_cpu = make_aligned_ptr<mode_info_t>(sb_rows * sb_cols * 64);
        auto mi_gpu = make_aligned_ptr<mode_info_t>(sb_rows * sb_cols * 64);

        int error = 0;
        for (int early_exit = 0; early_exit < 2; early_exit++) {
            memset(mi_cpu.get(), 0, sizeof(mode_info_t) * sb_rows * sb_cols * 64);
            generate_mode_info(par, mi_cpu.get());
            memcpy(mi_gpu.get(), mi_cpu.get(), sizeof(mode_info_t) * sb_rows * sb_cols * 64);

            printf("early_exit=%d ", early_exit);

            run_cpu(par, frame, mi_cpu.get(), early_exit);
            run_gpu(par, frame, mi_gpu.get(), early_exit);

            if (!compare(par, mi_cpu.get(), mi_gpu.get())) {
                printf(" !!! FAILED !!!\n");
                error = 1;
            } else {
                printf(" PASSED\n");
            }
        }

        return error;

    } catch (cm_error &e) {
        printf("CM_ERROR:\n");
        printf("  code: %d\n", e.errcode());
        printf("  file: %s\n", e.file());
        printf("  line: %d\n", e.line());
        if (const char *msg = e.what())
            printf("  message: %s\n", msg);
    } catch (std::exception &e) {
        printf("ERROR: %s\n", e.what());
        return 1;
    }
}


int HaveTopRight(uint8_t bsize, int32_t miRow, int32_t miCol, int32_t haveTop, int32_t haveRight, int32_t txsz, int32_t y, int32_t x, int32_t ss_x);
int HaveBottomLeft(uint8_t bsize, int32_t miRow, int32_t miCol, int32_t haveLeft, int32_t haveBottom, int32_t txsz, int32_t y, int32_t x, int32_t ss_y);
namespace VP9PP {
    void SetTargetPX(int isAV1);
    void SetTargetAVX2(int isAV1);
}

enum { DC_PRED=0, V_PRED=1, H_PRED=2, D45_PRED=3, D135_PRED=4, D117_PRED=5, D153_PRED=6, D207_PRED=7, D63_PRED=8,
       SMOOTH_PRED=9, SMOOTH_V_PRED=10, SMOOTH_H_PRED=11, PAETH_PRED=12, AV1_INTRA_MODES=PAETH_PRED+1 };

static const unsigned short INTRA_MODE_COST[3][AV1_INTRA_MODES] = {
    { 756, 2051+3*512, 2284+3*512, 3222+3*512, 2646+3*512, 2772+3*512, 2970+3*512, 2724+3*512, 2724+3*512, 2111, 2858, 2808,  887 }, // 8x8
    { 591, 2170+3*512, 2216+3*512, 3737+3*512, 3154+3*512, 3316+3*512, 3472+3*512, 3087+3*512, 2980+3*512, 1926, 2478, 2272, 1119 }, // 16x16
    { 405, 2592+3*512, 2589+3*512, 4356+3*512, 5505+3*512, 4720+3*512, 5844+3*512, 5013+3*512, 4993+3*512, 2652, 2784, 2292, 1001 }, // 32x32
};


const uint8_t h265_scan_z2r4[256] =
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

const unsigned short num_4x4_blocks_lookup[] = { 1, 2, 2, 4, 8, 8, 16, 32, 32, 64, 128, 128, 256, 512, 512, 1024, 4, 4, 16, 16, 64, 64, 256, 256 };
const unsigned char block_size_wide_4x4_log2[] = { 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 0, 2, 1, 3, 2, 4, 3, 5 };

static const char TEST_MODE_CTX[13][16] =
{
    { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 }, //mode,0
    { 1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,1
    { 1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,2
    { 1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,3
    { 1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,4
    { 1,1,1,0,1,0,0,0,0,0,0,0,0,1,1,1 }, //mode,5
    { 1,0,1,0,1,0,0,0,0,0,0,0,0,1,1,1 }, //mode,6
    { 1,0,1,1,0,0,0,0,0,0,0,0,0,1,1,1 }, //mode,7
    { 1,1,0,1,0,0,0,1,0,0,0,0,0,1,1,1 }, //mode,8
    { 1,1,1,1,1,1,1,1,1,0,0,0,0,1,1,1 }, //mode,9
    { 1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1 }, //mode,10
    { 1,1,1,1,1,1,1,1,1,1,0,0,0,1,1,1 }, //mode,11
    { 1,1,1,0,0,0,0,0,0,1,1,1,0,1,1,1 }, //mode,12
};

void run_cpu(const params &par, const frame_t &frame, mode_info_t *mode_info, int early_exit)
{
    VP9PP::SetTargetPX(1);

    const int sb_rows = (par.miRows + 7) >> 3;
    const int sb_cols = (par.miCols + 7) >> 3;
    const int mi_pitch = (par.miCols + 7) & ~7;
    const int width = par.miCols * 8;
    const int height = par.miRows * 8;
    const int mi_cols = par.miCols;
    const int mi_rows = par.miRows;

    VP9PP::IntraPredPels<unsigned char> pred_pels;
    ALIGNED(32) unsigned char pred[32 * 32];

    for (int sbr = 0; sbr < sb_rows; sbr++) {
        for (int sbc = 0; sbc < sb_cols; sbc++) {
            int num4x4;
            for (int i = 0; i < 256; i += num4x4) {
                const int raster_idx = h265_scan_z2r4[i];
                const int x4 = (raster_idx & 15);
                const int y4 = (raster_idx >> 4);
                const int mi_row = 8 * sbr + ((raster_idx >> 4) >> 1);
                const int mi_col = 8 * sbc + ((raster_idx & 15) >> 1);
                mode_info_t *mi = mode_info + mi_col + mi_row * mi_pitch;
                num4x4 = num_4x4_blocks_lookup[mi->sbType];
                int gpuIntraInfo = mi->memCtx.mode;
                bool tryIntra = (gpuIntraInfo < AV1_INTRA_MODES) ? true : false;
                if (mi->mode == 255 ||  // out of picture
                    mi->sbType == 12 ||   // no intra for 64x64
                     !tryIntra) // TryIntra
                    continue;

                const int log2size = block_size_wide_4x4_log2[mi->sbType];
                const int tx_size = log2size;
                const int mi_block_size = (1 << log2size) >> 1;
                const int block_size = mi_block_size << 3;

                const int have_top   = (mi_row > 0);
                const int have_right = (mi_col + mi_block_size < mi_cols);
                const int have_left  = (mi_col > 0);
                const int have_bottom = (mi_row + mi_block_size < mi_rows);
                const int have_top_right = HaveTopRight(mi->sbType, mi_row, mi_col, have_top, have_right, tx_size, 0, 0, 0);
                const int have_bottom_left = HaveBottomLeft(mi->sbType, mi_row, mi_col, have_left, have_bottom, tx_size, 0, 0, 0);

                const int y = mi_row * 8;
                const int x = mi_col * 8;
                const int num_pix_top_right = have_top_right ? MIN(block_size, width - x - block_size) : 0;
                const int num_pix_bottom_left = have_bottom_left ? MIN(block_size, height - y - block_size) : 0;

                const unsigned char *src = frame.data + x + y * frame.pitch;

                VP9PP::GetPredPelsAV1<0>(src, frame.pitch, pred_pels.top, pred_pels.left, block_size,
                                            have_top, have_left, num_pix_top_right, num_pix_bottom_left);

                int delta_angle = 0;
                unsigned best_cost = UINT_MAX;
                int best_mode = DC_PRED;

                const int pred_pitch = block_size;
                for (int mode = DC_PRED; mode < AV1_INTRA_MODES; mode++) {
                    if (mi->sbType == 3 && (mode == SMOOTH_H_PRED || mode == SMOOTH_V_PRED)) continue;
                    if (mi->sbType > 3 && !TEST_MODE_CTX[mode][best_mode]) continue;
                    if (mode == PAETH_PRED) continue;   // Loss less pred method with source can be a issue

                    VP9PP::predict_intra_av1(pred_pels.top, pred_pels.left, pred, pred_pitch, tx_size, have_left, have_top,
                                             mode, delta_angle, 0, 0);

                    const int sad = VP9PP::sad_general(src, frame.pitch, pred, pred_pitch, log2size, log2size);
                    const int rate = INTRA_MODE_COST[tx_size - 1][mode];
                    const unsigned int cost = (sad << 9) + par.lambdaInt * rate;

                    if (early_exit && mode > 0 && cost > (best_cost * 3 / 2))
                        break;

                    if (best_cost > cost) {
                        best_cost = cost;
                        best_mode = mode;
                    }
                }

                mi->memCtx.sad = best_cost;
                mi->memCtx.mode = best_mode;
            }
        }
    }
}

template <typename T> SurfaceIndex *get_index(T *cm_resource) {
    SurfaceIndex *index = 0;
    int res = cm_resource->GetIndex(index);
    THROW_CM_ERR(res);
    return index;
}

void run_gpu(const params &par, const frame_t &frame, mode_info_t *mode_info, int early_exit)
{
    const int sb_rows = (par.miRows + 7) >> 3;
    const int sb_cols = (par.miCols + 7) >> 3;
    const int width  = par.miCols * 8;
    const int height = par.miRows * 8;
    const int mi_pitch = ((par.miCols + 7) & ~7) * sizeof(mode_info_t);
    const unsigned int lambda = par.lambdaInt;
    mfxU32 version = 0;
    CmDevice *device = nullptr;
    CmProgram *program = nullptr;
    CmKernel *kernel = nullptr;
    CmTask *task = nullptr;
    CmQueue *queue = nullptr;
    CmThreadSpace *ts = nullptr;
    CmEvent *e = nullptr;
    CmSurface2D *src = nullptr;
    CmSurface2D *mi = nullptr;
    uint32_t tsw = sb_cols * 2;
    uint32_t tsh = sb_rows * 2;
    int32_t res = CM_SUCCESS;
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
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_intra_hsw, sizeof(genx_av1_intra_hsw), program, "nojitter"));
        break;
    case PLATFORM_INTEL_BDW:
    case PLATFORM_INTEL_CHV:
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_intra_bdw, sizeof(genx_av1_intra_bdw), program, "nojitter"));
        break;
    case PLATFORM_INTEL_SKL:
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_intra_skl, sizeof(genx_av1_intra_skl), program, "nojitter"));
        break;
    case PLATFORM_INTEL_ICLLP:
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_intra_icllp, sizeof(genx_av1_intra_icllp), program, "nojitter"));
        break;
    default:
        throw cm_error(CM_FAILURE, __FILE__, __LINE__, "Unknown HW type");
    }
    THROW_CM_ERR(device->CreateKernel(program, CM_KERNEL_FUNCTION(CheckIntra), kernel));
    THROW_CM_ERR(device->CreateTask(task));
    THROW_CM_ERR(device->CreateSurface2D(width, height, CM_SURFACE_FORMAT_A8, src));
    THROW_CM_ERR(device->CreateSurface2D(sb_cols * 8 * sizeof(mode_info_t), sb_rows * 8, CM_SURFACE_FORMAT_A8, mi));
    THROW_CM_ERR(src->WriteSurfaceStride((const unsigned char *)frame.data, nullptr, frame.pitch));
    THROW_CM_ERR(mi->WriteSurfaceStride((const unsigned char *)mode_info, nullptr, mi_pitch));

    const unsigned int yoff = 0;
    THROW_CM_ERR(kernel->SetKernelArg(0, sizeof(SurfaceIndex), get_index(src)));
    THROW_CM_ERR(kernel->SetKernelArg(1, sizeof(SurfaceIndex), get_index(mi)));
    THROW_CM_ERR(kernel->SetKernelArg(2, sizeof(unsigned int), &par.miCols));
    THROW_CM_ERR(kernel->SetKernelArg(3, sizeof(unsigned int), &par.miRows));
    THROW_CM_ERR(kernel->SetKernelArg(4, sizeof(unsigned int), &lambda));
    THROW_CM_ERR(kernel->SetKernelArg(5, sizeof(unsigned int), &early_exit));
    THROW_CM_ERR(kernel->SetKernelArg(6, sizeof(unsigned int), &yoff));
    THROW_CM_ERR(task->AddKernel(kernel));
    THROW_CM_ERR(kernel->SetThreadCount(tsw * tsh));
    THROW_CM_ERR(device->CreateThreadSpace(tsw, tsh, ts));
    THROW_CM_ERR(ts->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY));
    THROW_CM_ERR(queue->Enqueue(task, e, ts));
    THROW_CM_ERR(e->WaitForTaskFinished());
    THROW_CM_ERR(device->FlushPrintBuffer());

#ifndef CMRT_EMU
    printf("TIME=%.3fms", GetAccurateGpuTime(queue, task, ts) / 1000000.);
#endif //CMRT_EMU

    THROW_CM_ERR(mi->ReadSurfaceStride((unsigned char *)mode_info, nullptr, mi_pitch));

    queue->DestroyEvent(e);
    device->DestroyThreadSpace(ts);
    device->DestroySurface(mi);
    device->DestroySurface(src);
    device->DestroyTask(task);
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    DestroyCmDevice(device);
}

void report_mode_info_difference(int32_t mi_row, int32_t mi_col, const intra_info &cpu, const intra_info &gpu)
{
    #define DUMP_MODE_INFO_FIELD(name) \
        printf("%-11s %6d %6d\n", #name, cpu.name, gpu.name)

    printf("\n");
    printf("ModeInfo at (mi_row=%d, mi_col=%d) differs\n", mi_row, mi_col);
    printf("%11s %6s %6s\n", "", "cpu", "gpu");
    DUMP_MODE_INFO_FIELD(mode);
    DUMP_MODE_INFO_FIELD(sad);
    printf("\n");
}

bool compare(const params &par, const mode_info_t *mi_cpu, const mode_info_t *mi_gpu)
{
    const int mi_pitch = ((par.miCols + 7) & ~7);
    for (int32_t i = 0; i < par.miRows; i++) {
        for (int32_t j = 0; j < par.miCols; j++) {
            const intra_info &cpu = mi_cpu[i * mi_pitch + j].memCtx;
            const intra_info &gpu = mi_gpu[i * mi_pitch + j].memCtx;
            if (cpu.mode != gpu.mode ||
                cpu.sad  != gpu.sad)
            {
                report_mode_info_difference(i, j, cpu, gpu);
                return false;
            }
        }
    }

    return true;
}
