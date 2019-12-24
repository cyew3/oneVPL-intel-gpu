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
#pragma warning(pop)
#include "cm_rt.h"
#include "cm_def.h"
#include "cm_vm.h"

#include <assert.h>
#include <stdexcept>
#include <map>
#include <vector>
#include <memory>
#include "../include/test_common.h"

#ifdef CMRT_EMU
extern "C"
    void InterpolateDecision(SurfaceIndex SRC, vector<SurfaceIndex,2> REF, SurfaceIndex MODE_INFO, uint32_t lambda_int, float lambda);

const char genx_av1_interpolate_decision_hsw[1] = {};
const char genx_av1_interpolate_decision_bdw[1] = {};
const char genx_av1_interpolate_decision_skl[1] = {};
#else //CMRT_EMU
#include "../include/genx_av1_interpolate_decision_single_hsw_isa.h"
#include "../include/genx_av1_interpolate_decision_single_bdw_isa.h"
#include "../include/genx_av1_interpolate_decision_single_skl_isa.h"
#endif //CMRT_EMU

#pragma comment(lib, "advapi32")
#pragma comment( lib, "ole32.lib" )

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

struct MdControl {
    // gathered here for gpu
    uint16_t miCols;                      // off=0    size=2
    uint16_t miRows;                      // off=2    size=2
    float lambda;                      // off=4    size=4
    struct {
        int16_t zbin[2];
        int16_t round[2];
        int16_t quant[2];
        int16_t quantShift[2];
        int16_t dequant[2];
    } qparam;                           // off=8    size=20
    uint16_t lambdaInt;                   // off=28   size=2
    uint8_t compound_allowed;             // off=30   size=1
    uint8_t single_ref;                   // off=31   size=1
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
                                        // off=1500
    } bc;
};

const int32_t LOG2_MAX_NUM_PARTITIONS = 8;
const int32_t MAX_NUM_PARTITIONS      = 1 << LOG2_MAX_NUM_PARTITIONS;
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

const uint16_t num_4x4_blocks_lookup[] = { 1, 2, 2, 4, 8, 8, 16, 32, 32, 64, 128, 128, 256, 512, 512, 1024, 4, 4, 16, 16, 64, 64, 256, 256 };
const uint8_t block_size_wide_4x4_log2[] = { 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 0, 2, 1, 3, 2, 4, 3, 5 };
const uint8_t block_size_high_4x4_log2[] = { 0, 1, 0, 1, 2, 1, 2, 3, 2, 3, 4, 3, 4, 5, 4, 5, 2, 0, 3, 1, 4, 2, 5, 3 };
const uint8_t block_size_wide_8x8[] = { 1, 1, 1, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16, 16, 1, 2, 1, 4, 2, 8, 4, 16 };
const uint8_t block_size_high_8x8[] = { 1, 1, 1, 1, 2, 1, 2, 4, 2, 4, 8, 4, 8, 16, 8, 16, 2, 1, 4, 1, 8, 2, 16, 4 };

const int32_t MV_BORDER_AV1          = 128; // Value used when clipping motion vectors
const int32_t MI_SIZE                = 8;   // Smallest size of a mode info block


#define __ALIGN8 __declspec (align(8))
#define __ALIGN4 __declspec (align(4))

__ALIGN4 union H265MV {
    struct { int16_t mvx, mvy; };
    uint32_t asInt;
};
inline bool operator==(const H265MV &l, const H265MV &r) { return l.asInt == r.asInt; }
inline bool operator!=(const H265MV &l, const H265MV &r) { return l.asInt != r.asInt; }
const H265MV MV_ZERO = {};
const H265MV INVALID_MV = { (int16_t)-0x8000, (int16_t)-0x8000 };

__ALIGN8 struct H265MVPair {
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

enum BlockSize_ {BLOCK_8X8 = 3, BLOCK_16X16 = 6,  BLOCK_32X32 = 9, BLOCK_64X64 = 12};

struct MemCtx {
    union {
        struct {
            uint32_t singleRefP1  : 3;
            uint32_t singleRefP2  : 3;
            uint32_t compMode     : 3;
            uint32_t compRef      : 3;
            uint32_t skip         : 2;
            uint32_t interp       : 2;
            uint32_t isInter      : 3;
            uint32_t txSize       : 1;
            uint32_t interMode    : 3;
            uint32_t reserved     : 9;
        };
        struct {
            uint32_t singleRefP1  : 3;
            uint32_t singleRefP2  : 3;
            uint32_t compMode     : 3;
            uint32_t compRef      : 3;
            uint32_t skip         : 2;
            uint32_t interp       : 2;
            uint32_t isInter      : 3;
            uint32_t txSize       : 1;
            uint32_t interMode    : 9;
            uint32_t numDrlBits   : 2; // 0,1 or 2
            uint32_t reserved     : 1;
        } AV1;
    };
};

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

struct H265VideoParam {
    uint16_t miCols;
    uint16_t miRows;

    int32_t Width;
    int32_t Height;

    int32_t sb64Cols;
    int32_t sb64Rows;
    int32_t miPitch;
    int32_t MaxCUSize;

    int32_t PicWidthInCtbs;
    int32_t PicHeightInCtbs;
    int32_t PAD;

    int32_t lambdaInt32;
    float lambda;
};

enum { INTRA_FRAME=-1, NONE_FRAME=-1, LAST_FRAME=0, GOLDEN_FRAME=1, ALTREF_FRAME=2, COMP_VAR0=3, COMP_VAR1=4 };
enum ePredMode {
    // Intra modes
    DC_PRED=0, V_PRED=1, H_PRED=2, D45_PRED=3, D135_PRED=4, D117_PRED=5, D153_PRED=6, D207_PRED=7, D63_PRED=8, TM_PRED=9,
    // Inter modes
    NEARESTMV=10, NEARMV=11, ZEROMV=12, NEWMV=13,
    // helpers
    INTRA_MODES=TM_PRED+1,
    INTER_MODES=NEWMV-NEARESTMV+1,
    INTER_MODE_OFFSET=INTRA_MODES,
    MB_MODE_COUNT=NEWMV+1,
    OUT_OF_PIC=0xff,
    NEARESTMV_=0,
    NEARMV_=1,
    ZEROMV_=2,
    NEWMV_=3,
    NEARMV1_=4,
    NEARMV2_=5,
    NEAREST_NEWMV_=6,
    NEW_NEARESTMV_=7,
    NEAR_NEWMV_   =8,
    NEW_NEARMV_   =9,

    NEAR_NEWMV1_   =10,
    NEW_NEARMV1_   =11,
    NEAR_NEWMV2_   =12,
    NEW_NEARMV2_   =13,

    // AV1 new Intra modes
    SMOOTH_PRED=9, SMOOTH_V_PRED=10, SMOOTH_H_PRED=11, PAETH_PRED=12,
    UV_CFL_PRED=13,
    // AV1 shifted Inter modes
    AV1_NEARESTMV=13, AV1_NEARMV=14, AV1_ZEROMV=15, AV1_NEWMV=16,   // AV1
    //// AV1 compound Inter modes
    NEAREST_NEARESTMV, NEAR_NEARMV, NEAREST_NEWMV, NEW_NEARESTMV,
    NEAR_NEWMV, NEW_NEARMV, ZERO_ZEROMV, NEW_NEWMV,
    // AV1 helpers
    AV1_INTRA_MODES=PAETH_PRED+1,
    AV1_UV_INTRA_MODES=UV_CFL_PRED+1,
    //AV1_INTER_MODES=AV1_NEWMV-AV1_NEARESTMV+1,
    AV1_INTER_MODES=NEW_NEWMV-AV1_NEARESTMV+1,

    AV1_INTER_MODE_OFFSET=AV1_INTRA_MODES,
    AV1_MB_MODE_COUNT=NEW_NEWMV+1,
    AV1_INTER_COMPOUND_MODES=NEW_NEWMV-NEAREST_NEARESTMV+1,
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


int Compare(const uint8_t *data1, const uint8_t *data2, int32_t pitch, int32_t width, int32_t height, const char *error_msg)
{
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            if (data1[y * pitch + x] - data2[y * pitch + x]) {
                printf("%s at (x,y)=(%d,%d) %d vs %d\n", error_msg, x, y, data1[y * pitch + x], data2[y * pitch + x]);
                return FAILED;
            }

    return PASSED;
}


int CompareFilterType(const ModeInfo *frameCuData1, const ModeInfo *frameCuData2, H265VideoParam& par)
{
    for (int32_t miRow = 0; miRow < par.miRows; miRow++) {
        for (int32_t miCol = 0; miCol < par.miCols; miCol++) {
            const ModeInfo *mi1 = frameCuData1 + miCol + miRow * par.miPitch;
            const ModeInfo *mi2 = frameCuData2 + miCol + miRow * par.miPitch;

            if (mi1->sbType != mi2->sbType) {
                printf("!!! Diff sbType (%i vs %i) at (miRow,miCol) = (%i,%i)\n", mi1->sbType, mi2->sbType, miRow, miCol);
                return FAILED;
            }

            const int size8x8 = mi1->sbType >> 2;
            if ((miCol | miRow) & ((1 << size8x8) - 1))
                continue;

            if (mi1->interp != mi2->interp) {
                printf("!!! Diff interpolation types: (miRow,miCol,sbType,filt0,filt1) = (%i,%i,%i,%i,%i)\n",
                    miRow, miCol, mi1->sbType, mi1->interp, mi2->interp);
                return FAILED;
            }
        }
    }

    return PASSED;
}

int Dump(uint8_t* data, size_t frameSize, const char* fileName)
{
    FILE *fout = fopen(fileName, "wb");
    if (!fout)
        return FAILED;

    fwrite(data, 1, frameSize, fout);
    fclose(fout);

    return PASSED;
}

enum
{
    MFX_BLK_8x8   = 0,
    MFX_BLK_16x16 = 1,
    MFX_BLK_32x32 = 2,
    MFX_BLK_64x64 = 3,
    MFX_BLK_MAX   = 4,
};



template<class T> inline T AlignValue(T value, mfxU32 alignment)
{
    //assert((alignment & (alignment - 1)) == 0); // should be 2^n
    return static_cast<T>((value + alignment - 1) & ~(alignment - 1));
}

struct rand16u {
    rand16u() : val(0) {}
    void init(uint16_t ival) { val = ival; }
    uint16_t val;
    uint16_t operator()() {
        uint32_t bit = ((val >> 0) ^ (val >> 2) ^ (val >> 3) ^ (val >> 5)) & 1;
        val = uint16_t((val >> 1) | (bit << 15));
        return val;
    }
};

H265MV GenRandMv(rand16u &localrand, int32_t min, int32_t max)
{
    H265MV mv;
    mv.mvx = int16_t((localrand() % (max - min + 1)) + min);
    mv.mvy = int16_t((localrand() % (max - min + 1)) + min);

    //
    mv.mvx = (mv.mvx >> 1) << 1;
    mv.mvy = (mv.mvy >> 1) << 1;

    return mv;
}

int32_t GenRand(rand16u &localrand, int32_t min, int32_t max)
{
    int32_t dist = (int32_t)((localrand() % (max - min + 1)) + min);

    return dist;
}

void PropagateSubPart(ModeInfo* mi0, ModeInfo* mi, int32_t miPitch, int32_t miWidth, int32_t miHeight)
{

    for (int32_t mir = 0; mir < miHeight; mir++) {
        for (int32_t mic = 0; mic < miWidth; mic++) {
            mi[mir*miPitch + mic].sbType = mi0->sbType;
            mi[mir*miPitch + mic].interp = mi0->interp;
            mi[mir*miPitch + mic].refIdx[0] = mi0->refIdx[0];
            mi[mir*miPitch + mic].refIdx[1] = mi0->refIdx[1];

            mi[mir*miPitch + mic].mv[0] = mi0->mv[0];
            mi[mir*miPitch + mic].mv[1] = mi0->mv[1];
            //mi[mir*miPitch + mic] = *mi;
        }
    }
}

const int32_t MAX_CU_SIZE = 64;



inline BlockSize GetSbType(int32_t depth, int32_t partition) {
    return BlockSize(3 * (4 - depth) - partition);
}

#define COST_MAX ( 255 )
const int32_t PARTITION_NONE = 0;

void FillOneModeInfo(int32_t depth, rand16u& localrand, ModeInfo* mi)
{
    mi->sbType = GetSbType(depth, PARTITION_NONE);
    mi->mode = AV1_NEARESTMV + GenRand(localrand, 0, 3);

    int32_t isCompound = GenRand(localrand, 0, 1);
    mi->refIdx[0] = uint8_t(isCompound ? 0 : GenRand(localrand, 0, 2));
    mi->refIdx[1] = uint8_t(isCompound ? 2 : NONE_FRAME);

    mi->mv[0] = GenRandMv(localrand, -64, 63);
    if (mi->refIdx[1] != NONE_FRAME)
        mi->mv[1] = GenRandMv(localrand, -64, 63);
    else
        mi->mv[1].asInt = 0;

    mi->skip = (uint8_t)GenRand(localrand, 0, 1);
}

void ModeDecisionInterTu7(int32_t depth, rand16u& localrand, H265VideoParam& par, ModeInfo* frameCuData, int32_t miRow, int32_t miCol)
{
    if (depth == 3) {
        ModeInfo *mi = frameCuData + miCol + miRow * par.miPitch;
        FillOneModeInfo(depth, localrand, mi);
        return;
    }

    const int32_t left = miCol << 3;
    const int32_t top  = miRow << 3;
    const int32_t size = MAX_CU_SIZE >> depth;
    const int32_t halfSize8 = size >> 4;
    const int32_t allowNonSplit = (left + size <= par.Width && top + size <= par.Height);

    const int32_t halfSize = size >> 1;
    ModeInfo *mi = frameCuData + miCol + miRow * par.miPitch;
    ModeInfo miNonSplit;

    if (allowNonSplit) {
        FillOneModeInfo(depth, localrand, &miNonSplit);
    }

    // in case the second, third or fourth parts are out of the picture
    mi[halfSize8].sbType = GetSbType(depth + 1, PARTITION_NONE);
    mi[halfSize8].mode = OUT_OF_PIC;
    mi[halfSize8 * par.miPitch] = mi[halfSize8];
    mi[halfSize8 * par.miPitch + halfSize8] = mi[halfSize8];

    ModeDecisionInterTu7(depth + 1, localrand, par, frameCuData, miRow, miCol);
    if (left + halfSize < par.Width)
        ModeDecisionInterTu7(depth + 1, localrand, par, frameCuData, miRow, miCol + halfSize8);
    if (top + halfSize < par.Height) {
        ModeDecisionInterTu7(depth + 1, localrand, par, frameCuData, miRow + halfSize8, miCol);
        if (left + halfSize < par.Width)
            ModeDecisionInterTu7(depth + 1, localrand, par, frameCuData, miRow + halfSize8, miCol + halfSize8);
    }

    // decision & propagate
    if (!allowNonSplit) return;

    //int32_t isNonSplitWin = depth == 3;//(GenRand(localrand, 0, 2) != 0);
    int32_t isNonSplitWin = (GenRand(localrand, 0, 2) != 0);
    if (/*costNonSplit < costSplit*/isNonSplitWin) {
        PropagateSubPart(&miNonSplit, mi, par.miPitch, size >> 3, size >> 3);
    } else if (((miRow | miCol) & 1) == 0 && mi->sbType == BLOCK_8X8) {
        const int32_t makeJoinable8x8 = (GenRand(localrand, 0, 2) == 0);
        /*if (makeJoinable8x8) {
            mi[1].mv = mi[par.miPitch].mv = mi[par.miPitch + 1].mv = mi[0].mv;
            mi[1].refIdx[0] = mi[par.miPitch].refIdx[0] = mi[par.miPitch + 1].refIdx[0] = mi[0].refIdx[0];
            mi[1].refIdx[1] = mi[par.miPitch].refIdx[1] = mi[par.miPitch + 1].refIdx[1] = mi[0].refIdx[1];
        }*/
    }
}

void RandomModeDecision(rand16u& localrand, H265VideoParam& par, ModeInfo* frameCuData)
{
    for (int32_t row = 0, ctbAddr = 0; row < par.sb64Rows; row++) {
        for (int32_t col = 0; col < par.sb64Cols; col++, ctbAddr++) {
            ModeDecisionInterTu7(0, localrand, par, frameCuData, row << 3, col << 3);
        }
    }
}

void CheckModeInfo(H265VideoParam& par, ModeInfo* frameCuData)
{
    int32_t statsReport[4] = {0, 0, 0, 0};
    int32_t statsReportComp[2] = {0, 0};

    std::map<int/*Width*/,int/*sbType*/> mapWidth2Idx;
    mapWidth2Idx[8]  = 0;
    mapWidth2Idx[16] = 1;
    mapWidth2Idx[32] = 2;
    mapWidth2Idx[64] = 3;

    for (int32_t row = 0, ctbAddr = 0; row < par.sb64Rows; row++) {
        for (int32_t col = 0; col < par.sb64Cols; col++, ctbAddr++) {

            ModeInfo *m_data = frameCuData + (col << 3) + (row << 3) * par.miPitch;

            for (int32_t i = 0; i < MAX_NUM_PARTITIONS;) {
                const int32_t rasterIdx = h265_scan_z2r4[i];
                const int32_t x4 = (rasterIdx & 15);
                const int32_t y4 = (rasterIdx >> 4);
                const int32_t sbx = x4 << 2;
                const int32_t sby = y4 << 2;
                ModeInfo *mi = m_data + (sbx >> 3) + (sby >> 3) * par.miPitch;

                const int32_t num4x4 = num_4x4_blocks_lookup[mi->sbType];

                // early exit
                if (mi->mode == 255) {i += num4x4; continue;}

                int32_t szIdx = mi->sbType >> 2;
                statsReport[szIdx]++;

                int32_t isCompound = mi->refIdx[1] > NONE_FRAME;
                statsReportComp[isCompound]++;

                // update
                i += num4x4;
            }
        }
    }

    // stats
    {
        int32_t blocksCount = statsReport[0] + statsReport[1] + statsReport[2] + statsReport[3];
        float persen8 = (float)statsReport[0] / blocksCount * 100.f;
        float persen16 = (float)statsReport[1] / blocksCount* 100.f;
        float persen32 = (float)statsReport[2] / blocksCount* 100.f;
        float persen64 = (float)statsReport[3] / blocksCount* 100.f;
        float percentComp = (float)statsReportComp[1] / (statsReportComp[0] + statsReportComp[1])* 100;

        printf("Statistics: 8x8-%4.2f%% | 16x16-%4.2f%% | 32x32-%4.2f%% | 64x64-%4.2f%% | comp-%4.2f%% \n",
            persen8, persen16, persen32, persen64, percentComp);
    }
}

#define __ALIGN32 __declspec (align(32))
#define __ALIGN64 __declspec (align(64))

enum { EIGHTTAP=0, EIGHTTAP_SMOOTH=1, EIGHTTAP_SHARP=2, BILINEAR=3, SWITCHABLE_FILTERS=3,
    SWITCHABLE_FILTER_CONTEXTS=(SWITCHABLE_FILTERS + 1), SWITCHABLE=4, EIGHTTAP_REGULAR=EIGHTTAP };


void copyROI(uint8_t* src, int srcPitch, uint8_t* dst, int dstPitch, int w, int h)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            dst[y*dstPitch + x] = src[y*srcPitch + x];
        }
    }
}

static const int32_t SUBPEL_BITS   = 4;
static const int32_t SUBPEL_MASK   = (1 << SUBPEL_BITS) - 1;
static const int32_t SUBPEL_SHIFTS = 1 << SUBPEL_BITS;
static const int32_t SUBPEL_TAPS   = 8;
typedef int16_t InterpKernel[SUBPEL_TAPS];

__ALIGN32 static const InterpKernel bilinear_filters[SUBPEL_SHIFTS] = {
    { 0, 0, 0, 128, 0, 0, 0, 0 },  { 0, 0, 0, 120, 8, 0, 0, 0 },
    { 0, 0, 0, 112, 16, 0, 0, 0 }, { 0, 0, 0, 104, 24, 0, 0, 0 },
    { 0, 0, 0, 96, 32, 0, 0, 0 },  { 0, 0, 0, 88, 40, 0, 0, 0 },
    { 0, 0, 0, 80, 48, 0, 0, 0 },  { 0, 0, 0, 72, 56, 0, 0, 0 },
    { 0, 0, 0, 64, 64, 0, 0, 0 },  { 0, 0, 0, 56, 72, 0, 0, 0 },
    { 0, 0, 0, 48, 80, 0, 0, 0 },  { 0, 0, 0, 40, 88, 0, 0, 0 },
    { 0, 0, 0, 32, 96, 0, 0, 0 },  { 0, 0, 0, 24, 104, 0, 0, 0 },
    { 0, 0, 0, 16, 112, 0, 0, 0 }, { 0, 0, 0, 8, 120, 0, 0, 0 }
};

__ALIGN32 const InterpKernel sub_pel_filters_8_av1[SUBPEL_SHIFTS] = {
    { 0, 0, 0, 128, 0, 0, 0, 0 },      { 0, 2, -6, 126, 8, -2, 0, 0 },
    { 0, 2, -10, 122, 18, -4, 0, 0 },  { 0, 2, -12, 116, 28, -8, 2, 0 },
    { 0, 2, -14, 110, 38, -10, 2, 0 }, { 0, 2, -14, 102, 48, -12, 2, 0 },
    { 0, 2, -16, 94, 58, -12, 2, 0 },  { 0, 2, -14, 84, 66, -12, 2, 0 },
    { 0, 2, -14, 76, 76, -14, 2, 0 },  { 0, 2, -12, 66, 84, -14, 2, 0 },
    { 0, 2, -12, 58, 94, -16, 2, 0 },  { 0, 2, -12, 48, 102, -14, 2, 0 },
    { 0, 2, -10, 38, 110, -14, 2, 0 }, { 0, 2, -8, 28, 116, -12, 2, 0 },
    { 0, 0, -4, 18, 122, -10, 2, 0 },  { 0, 0, -2, 8, 126, -6, 2, 0 }
};

__ALIGN32 const InterpKernel sub_pel_filters_8s_av1[SUBPEL_SHIFTS] = {
    { 0, 0, 0, 128, 0, 0, 0, 0 },         { -2, 2, -6, 126, 8, -2, 2, 0 },
    { -2, 6, -12, 124, 16, -6, 4, -2 },   { -2, 8, -18, 120, 26, -10, 6, -2 },
    { -4, 10, -22, 116, 38, -14, 6, -2 }, { -4, 10, -22, 108, 48, -18, 8, -2 },
    { -4, 10, -24, 100, 60, -20, 8, -2 }, { -4, 10, -24, 90, 70, -22, 10, -2 },
    { -4, 12, -24, 80, 80, -24, 12, -4 }, { -2, 10, -22, 70, 90, -24, 10, -4 },
    { -2, 8, -20, 60, 100, -24, 10, -4 }, { -2, 8, -18, 48, 108, -22, 10, -4 },
    { -2, 6, -14, 38, 116, -22, 10, -4 }, { -2, 6, -10, 26, 120, -18, 8, -2 },
    { -2, 4, -6, 16, 124, -12, 6, -2 },   { 0, 2, -2, 8, 126, -6, 2, -2 }
};

__ALIGN32 const InterpKernel sub_pel_filters_8lp_av1[SUBPEL_SHIFTS] = {
    { 0, 0, 0, 128, 0, 0, 0, 0 },     { 0, 2, 28, 62, 34, 2, 0, 0 },
    { 0, 0, 26, 62, 36, 4, 0, 0 },    { 0, 0, 22, 62, 40, 4, 0, 0 },
    { 0, 0, 20, 60, 42, 6, 0, 0 },    { 0, 0, 18, 58, 44, 8, 0, 0 },
    { 0, 0, 16, 56, 46, 10, 0, 0 },   { 0, -2, 16, 54, 48, 12, 0, 0 },
    { 0, -2, 14, 52, 52, 14, -2, 0 }, { 0, 0, 12, 48, 54, 16, -2, 0 },
    { 0, 0, 10, 46, 56, 16, 0, 0 },   { 0, 0, 8, 44, 58, 18, 0, 0 },
    { 0, 0, 6, 42, 60, 20, 0, 0 },    { 0, 0, 4, 40, 62, 22, 0, 0 },
    { 0, 0, 4, 36, 62, 26, 0, 0 },    { 0, 0, 2, 34, 62, 28, 2, 0 }
};

const InterpKernel *av1_filter_kernels[4] = {
    sub_pel_filters_8_av1, sub_pel_filters_8lp_av1, sub_pel_filters_8s_av1, bilinear_filters
};

__ALIGN32 const InterpKernel av1_sub_pel_filters_4[SUBPEL_SHIFTS] = {
    { 0, 0, 0, 128, 0, 0, 0, 0 },     { 0, 0, -4, 126, 8, -2, 0, 0 },
    { 0, 0, -8, 122, 18, -4, 0, 0 },  { 0, 0, -10, 116, 28, -6, 0, 0 },
    { 0, 0, -12, 110, 38, -8, 0, 0 }, { 0, 0, -12, 102, 48, -10, 0, 0 },
    { 0, 0, -14, 94, 58, -10, 0, 0 }, { 0, 0, -12, 84, 66, -10, 0, 0 },
    { 0, 0, -12, 76, 76, -12, 0, 0 }, { 0, 0, -10, 66, 84, -12, 0, 0 },
    { 0, 0, -10, 58, 94, -14, 0, 0 }, { 0, 0, -10, 48, 102, -12, 0, 0 },
    { 0, 0, -8, 38, 110, -12, 0, 0 }, { 0, 0, -6, 28, 116, -10, 0, 0 },
    { 0, 0, -4, 18, 122, -8, 0, 0 },  { 0, 0, -2, 8, 126, -4, 0, 0 }
};
__ALIGN32 const InterpKernel av1_sub_pel_filters_4smooth[SUBPEL_SHIFTS] = {
    { 0, 0, 0, 128, 0, 0, 0, 0 },   { 0, 0, 30, 62, 34, 2, 0, 0 },
    { 0, 0, 26, 62, 36, 4, 0, 0 },  { 0, 0, 22, 62, 40, 4, 0, 0 },
    { 0, 0, 20, 60, 42, 6, 0, 0 },  { 0, 0, 18, 58, 44, 8, 0, 0 },
    { 0, 0, 16, 56, 46, 10, 0, 0 }, { 0, 0, 14, 54, 48, 12, 0, 0 },
    { 0, 0, 12, 52, 52, 12, 0, 0 }, { 0, 0, 12, 48, 54, 14, 0, 0 },
    { 0, 0, 10, 46, 56, 16, 0, 0 }, { 0, 0, 8, 44, 58, 18, 0, 0 },
    { 0, 0, 6, 42, 60, 20, 0, 0 },  { 0, 0, 4, 40, 62, 22, 0, 0 },
    { 0, 0, 4, 36, 62, 26, 0, 0 },  { 0, 0, 2, 34, 62, 30, 0, 0 }
};

const InterpKernel *av1_short_filter_kernels[4] = {
    av1_sub_pel_filters_4, av1_sub_pel_filters_4smooth, av1_sub_pel_filters_4, bilinear_filters
};

typedef void (*convolve_fn_t)(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *filtx, const int16_t *filty, int w, int h);



typedef int16_t int16_t;
typedef uint8_t  uint8_t;
#define FILTER_BITS 7
#define SUBPEL_BITS 4
#define SUBPEL_MASK ((1 << SUBPEL_BITS) - 1)
#define SUBPEL_SHIFTS (1 << SUBPEL_BITS)
#define SUBPEL_TAPS 8
static const InterpKernel *get_filter_base(const int16_t *filter)
{
    // NOTE: This assumes that the filter table is 256-byte aligned.
    // TODO(agrange) Modify to make independent of table alignment.
    return (const InterpKernel *)(((intptr_t)filter) & ~((intptr_t)0xFF));
}

static int get_filter_offset(const int16_t *f, const InterpKernel *base)
{
    return (int)((const InterpKernel *)(intptr_t)f - base);
}

/* Shift down with rounding */
#define ROUND_POWER_OF_TWO(value, n) (((value) + (1 << ((n)-1))) >> (n))

static inline uint8_t clip_pixel(int val)
{
    return uint8_t((val > 255) ? 255 : (val < 0) ? 0 : val);
}

template <bool avg> void convolve8_copy_px(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
{
    int r;

    (void)filter_x;
    (void)filter_y;

    for (r = h; r > 0; --r) {
        if (avg)
            for (int x = 0; x < w; ++x) dst[x] = ROUND_POWER_OF_TWO(dst[x] + src[x], 1);
        else
            memcpy(dst, src, w);
        src += src_stride;
        dst += dst_stride;
    }
}

static void core_convolve_avg_vert(const uint8_t *src, int src_stride,
                                   uint8_t *dst, int dst_stride,
                                   const InterpKernel *y_filters, int y0_q4,
                                   int y_step_q4, int w, int h)
{
    int x, y;
    src -= src_stride * (SUBPEL_TAPS / 2 - 1);

    for (x = 0; x < w; ++x) {
        int y_q4 = y0_q4;
        for (y = 0; y < h; ++y) {
            const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
            const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
            int k, sum = 0;
            for (k = 0; k < SUBPEL_TAPS; ++k)
                sum += src_y[k * src_stride] * y_filter[k];
            dst[y * dst_stride] = ROUND_POWER_OF_TWO(
                dst[y * dst_stride] +
                clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)),
                1);
            y_q4 += y_step_q4;
        }
        ++src;
        ++dst;
    }
}

static void core_convolve_vert(
    const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride,
    const InterpKernel *y_filters, int y0_q4, int y_step_q4, int w, int h)
{
    int x, y;
    src -= src_stride * (SUBPEL_TAPS / 2 - 1);

    for (x = 0; x < w; ++x) {
        int y_q4 = y0_q4;
        for (y = 0; y < h; ++y) {
            const unsigned char *src_y = &src[(y_q4 >> SUBPEL_BITS) * src_stride];
            const int16_t *const y_filter = y_filters[y_q4 & SUBPEL_MASK];
            int k, sum = 0;
            for (k = 0; k < SUBPEL_TAPS; ++k)
                sum += src_y[k * src_stride] * y_filter[k];
            dst[y * dst_stride] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
            y_q4 += y_step_q4;
        }
        ++src;
        ++dst;
    }
}

template <bool avg> void convolve8_vert_px(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
{
    const InterpKernel *const filters_y = get_filter_base(filter_y);
    const int y0_q4 = get_filter_offset(filter_y, filters_y);

    (void)filter_x;

    (avg)
        ? core_convolve_avg_vert(src, src_stride, dst, dst_stride, filters_y, y0_q4, 16, w, h)
        : core_convolve_vert(src, src_stride, dst, dst_stride, filters_y, y0_q4, 16, w, h);
}

static void core_convolve_avg_horz(const uint8_t *src, int src_stride,
                                   uint8_t *dst, int dst_stride,
                                   const InterpKernel *x_filters, int x0_q4,
                                   int x_step_q4, int w, int h)
{
    int x, y;
    src -= SUBPEL_TAPS / 2 - 1;
    for (y = 0; y < h; ++y) {
        int x_q4 = x0_q4;
        for (x = 0; x < w; ++x) {
            const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
            const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
            int k, sum = 0;
            for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
            dst[x] = ROUND_POWER_OF_TWO(
                dst[x] + clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS)), 1);
            x_q4 += x_step_q4;
        }
        src += src_stride;
        dst += dst_stride;
    }
}

static void core_convolve_horz(const uint8_t *src, int src_stride,
                               uint8_t *dst, int dst_stride,
                               const InterpKernel *x_filters, int x0_q4,
                               int x_step_q4, int w, int h)
{
    int x, y;
    src -= SUBPEL_TAPS / 2 - 1;
    for (y = 0; y < h; ++y) {
        int x_q4 = x0_q4;
        for (x = 0; x < w; ++x) {
            const uint8_t *const src_x = &src[x_q4 >> SUBPEL_BITS];
            const int16_t *const x_filter = x_filters[x_q4 & SUBPEL_MASK];
            int k, sum = 0;
            for (k = 0; k < SUBPEL_TAPS; ++k) sum += src_x[k] * x_filter[k];
            dst[x] = clip_pixel(ROUND_POWER_OF_TWO(sum, FILTER_BITS));
            x_q4 += x_step_q4;
        }
        src += src_stride;
        dst += dst_stride;
    }
}

template <bool avg> void convolve8_horz_px(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
{
    const InterpKernel *const filters_x = get_filter_base(filter_x);
    const int x0_q4 = get_filter_offset(filter_x, filters_x);

    (void)filter_y;

    (avg)
        ? core_convolve_avg_horz(src, src_stride, dst, dst_stride, filters_x, x0_q4, 16, w, h)
        : core_convolve_horz(src, src_stride, dst, dst_stride, filters_x, x0_q4, 16, w, h);
}

template <bool avg> void convolve8_both_px(const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, const int16_t *filter_x, const int16_t *filter_y, int w, int h)
{
    const InterpKernel *const filters_x = get_filter_base(filter_x);
    const int x0_q4 = get_filter_offset(filter_x, filters_x);

    const InterpKernel *const filters_y = get_filter_base(filter_y);
    const int y0_q4 = get_filter_offset(filter_y, filters_y);

    // Note: Fixed size intermediate buffer, temp, places limits on parameters.
    // 2d filtering proceeds in 2 steps:
    //   (1) Interpolate horizontally into an intermediate buffer, temp.
    //   (2) Interpolate temp vertically to derive the sub-pixel result.
    // Deriving the maximum number of rows in the temp buffer (135):
    // --Smallest scaling factor is x1/2 ==> y_step_q4 = 32 (Normative).
    // --Largest block size is 64x64 pixels.
    // --64 rows in the downscaled frame span a distance of (64 - 1) * 32 in the
    //   original frame (in 1/16th pixel units).
    // --Must round-up because block may be located at sub-pixel position.
    // --Require an additional SUBPEL_TAPS rows for the 8-tap filter tails.
    // --((64 - 1) * 32 + 15) >> 4 + 8 = 135.
    uint8_t temp[135 * 64];
    int intermediate_height = (((h - 1) * 16 + y0_q4) >> SUBPEL_BITS) + SUBPEL_TAPS;

    assert(w <= 64);
    assert(h <= 64);

    if (avg) {
        convolve8_both_px<false>(src, src_stride, temp, 64, filter_x, filter_y, w, h);
        convolve8_copy_px<true>(temp, 64, dst, dst_stride, NULL, NULL, w, h);
    } else {
        core_convolve_horz(src - src_stride * (SUBPEL_TAPS / 2 - 1), src_stride, temp, 64, filters_x, x0_q4, 16, w, intermediate_height);
        core_convolve_vert(temp + 64 * (SUBPEL_TAPS / 2 - 1), 64, dst, dst_stride, filters_y, y0_q4, 16, w, h);
    }
}

void interpolate_luma_px(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, int subpel_x, int subpel_y, int w, int h, int ref, int32_t interp_filter)
{
    convolve_fn_t predict_px[2][2][2]; // horiz, vert, avg
    predict_px[0][0][0] = convolve8_copy_px<false>;
    predict_px[0][1][0] = convolve8_vert_px<false>;
    predict_px[1][0][0] = convolve8_horz_px<false>;
    predict_px[1][1][0] = convolve8_both_px<false>;
    predict_px[0][0][1] = convolve8_copy_px<true>;
    predict_px[0][1][1] = convolve8_vert_px<true>;
    predict_px[1][0][1] = convolve8_horz_px<true>;
    predict_px[1][1][1] = convolve8_both_px<true>;

    int interp0 = interp_filter & 0x3;
    int interp1 = (interp_filter >> 4) & 0x3;
    const InterpKernel *kernel_y = av1_filter_kernels[interp0];
    const InterpKernel *kernel_x = av1_filter_kernels[interp1];

    //const InterpKernel *kernel = isAV1 ? av1_filter_kernels[interp_filter] : vp9_filter_kernels[interp_filter];
    predict_px[subpel_x != 0][subpel_y != 0][ref](src, pitchSrc, dst, pitchDst, kernel_x[subpel_x], kernel_y[subpel_y], w, h);
}

void Interpolate(const uint8_t *refColoc, int32_t refPitch, uint8_t *dst, int32_t dstPitch, H265MV mv, int32_t w, int32_t h, int32_t avg, int32_t interp)
{
    const int32_t intx = mv.mvx >> 3;
    const int32_t inty = mv.mvy >> 3;
    const int32_t dx = (mv.mvx << 1) & 15;
    const int32_t dy = (mv.mvy << 1) & 15;
    refColoc += intx + inty * refPitch;
    interpolate_luma_px(refColoc, refPitch, dst, dstPitch, dx, dy, w, h, avg, interp);
}

static const int MAX_BLOCK_SIZE = 96;
static const int ROUND_STAGE0 = 3;
static const int ROUND_STAGE1 = 11;
static const int COMPOUND_ROUND1_BITS = 7;
static const int OFFSET_BITS = 2 * FILTER_BITS - ROUND_STAGE0 - COMPOUND_ROUND1_BITS;

int round_power_of_two(int value, int n) { return (value + ((1 << n) >> 1)) >> n; };

void interp_av1_precise(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h, int round1)
{
    assert(w <= MAX_BLOCK_SIZE && h <= MAX_BLOCK_SIZE);
    const int pitchTmp = w;
    int32_t tmpBuf[MAX_BLOCK_SIZE * (MAX_BLOCK_SIZE + SUBPEL_TAPS - 1)];
    int32_t *tmp = tmpBuf;

    const int im_height = h + SUBPEL_TAPS - 1;
    src -= (SUBPEL_TAPS / 2 - 1) * pitchSrc + (SUBPEL_TAPS / 2 - 1);

    for (int y = 0; y < im_height; y++) {
        for (int x = 0; x < w; x++) {
            int sum = 0;
            for (int k = 0; k < SUBPEL_TAPS; ++k)
                sum += fx[k] * src[y * pitchSrc + x + k];
            tmp[y * pitchTmp + x] = round_power_of_two(sum, ROUND_STAGE0);
        }
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sum = 0;
            for (int k = 0; k < SUBPEL_TAPS; ++k)
                sum += fy[k] * tmp[(y + k) * pitchTmp + x];
            int res = round_power_of_two(sum, round1);
            dst[y * pitchDst + x] = (int16_t)res;
        }
    }
}


void interp_av1_precise(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h)
{
    const int pitchTmp = w;
    int16_t tmp[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

    interp_av1_precise(src, pitchSrc, tmp, w, fx, fy, w, h, ROUND_STAGE1);

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            dst[y * pitchDst + x] = clip_pixel(tmp[y * pitchTmp + x]);
}


void interp_av1_precise(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst,
                        int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h)
{
    const int pitchTmp = w;
    int16_t tmp[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

    interp_av1_precise(src, pitchSrc, tmp, w, fx, fy, w, h, COMPOUND_ROUND1_BITS);

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            dst[y * pitchDst + x] = clip_pixel(round_power_of_two(ref0[y * pitchRef0 + x] + tmp[y * pitchTmp + x], OFFSET_BITS + 1));
}

template <typename T>
void nv12_to_yv12(const T *src, int pitchSrc, T *dstU, int pitchDstU, T *dstV, int pitchDstV, int w, int h)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            dstU[y * pitchDstU + x] = src[y * pitchSrc + 2 * x + 0];
            dstV[y * pitchDstV + x] = src[y * pitchSrc + 2 * x + 1];
        }
    }
}

template <typename T>
void yv12_to_nv12(const T *srcU, int pitchSrcU, const T *srcV, int pitchSrcV, T *dst, int pitchDst, int w, int h)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            dst[y * pitchDst + 2 * x + 0] = srcU[y * pitchSrcU + x];
            dst[y * pitchDst + 2 * x + 1] = srcV[y * pitchSrcV + x];
        }
    }
}

void interp_nv12_av1_precise(const uint8_t *src, int pitchSrc, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h)
{
    uint8_t bufSrcU[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
    uint8_t bufSrcV[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
    uint8_t dstU[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
    uint8_t dstV[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

    const int32_t pitchSrcYv12 = MAX_BLOCK_SIZE;
    const int32_t extW = w + SUBPEL_TAPS - 1;
    const int32_t extH = h + SUBPEL_TAPS - 1;
    const int32_t shiftW = SUBPEL_TAPS / 2 - 1;
    const int32_t shiftH = SUBPEL_TAPS / 2 - 1;

    uint8_t *srcU = bufSrcU;
    uint8_t *srcV = bufSrcV;
    src -= shiftH * pitchSrc + shiftW * 2;
    nv12_to_yv12(src, pitchSrc, srcU, pitchSrcYv12, srcV, pitchSrcYv12, extW, extH);

    srcU += shiftH * pitchSrcYv12 + shiftW;
    srcV += shiftH * pitchSrcYv12 + shiftW;
    interp_av1_precise(srcU, pitchSrcYv12, dstU, w, fx, fy, w, h);
    interp_av1_precise(srcV, pitchSrcYv12, dstV, w, fx, fy, w, h);

    yv12_to_nv12(dstU, w, dstV, w, dst, pitchDst, w, h);
}

void interp_nv12_av1_precise(const uint8_t *src, int pitchSrc, int16_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h)
{
    uint8_t bufSrcU[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
    uint8_t bufSrcV[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
    int16_t dstU[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
    int16_t dstV[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

    const int32_t pitchSrcYv12 = MAX_BLOCK_SIZE;
    const int32_t extW = w + SUBPEL_TAPS - 1;
    const int32_t extH = h + SUBPEL_TAPS - 1;
    const int32_t shiftW = SUBPEL_TAPS / 2 - 1;
    const int32_t shiftH = SUBPEL_TAPS / 2 - 1;

    uint8_t *srcU = bufSrcU;
    uint8_t *srcV = bufSrcV;
    src -= shiftH * pitchSrc + shiftW * 2;
    nv12_to_yv12(src, pitchSrc, srcU, pitchSrcYv12, srcV, pitchSrcYv12, extW, extH);

    srcU += shiftH * pitchSrcYv12 + shiftW;
    srcV += shiftH * pitchSrcYv12 + shiftW;
    interp_av1_precise(srcU, pitchSrcYv12, dstU, w, fx, fy, w, h, COMPOUND_ROUND1_BITS);
    interp_av1_precise(srcV, pitchSrcYv12, dstV, w, fx, fy, w, h, COMPOUND_ROUND1_BITS);

    yv12_to_nv12(dstU, w, dstV, w, dst, pitchDst, w, h);
}

void interp_nv12_av1_precise(const uint8_t *src, int pitchSrc, const int16_t *ref0, int pitchRef0, uint8_t *dst, int pitchDst, const int16_t *fx, const int16_t *fy, int w, int h)
{
    uint8_t bufSrcU[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
    uint8_t bufSrcV[(MAX_BLOCK_SIZE + SUBPEL_TAPS) * (MAX_BLOCK_SIZE + SUBPEL_TAPS)];
    int16_t ref0U[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
    int16_t ref0V[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
    uint8_t dstU[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];
    uint8_t dstV[MAX_BLOCK_SIZE * MAX_BLOCK_SIZE];

    const int32_t pitchSrcYv12 = MAX_BLOCK_SIZE;
    const int32_t extW = w + SUBPEL_TAPS - 1;
    const int32_t extH = h + SUBPEL_TAPS - 1;
    const int32_t shiftW = SUBPEL_TAPS / 2 - 1;
    const int32_t shiftH = SUBPEL_TAPS / 2 - 1;

    uint8_t *srcU = bufSrcU;
    uint8_t *srcV = bufSrcV;
    src -= shiftH * pitchSrc + shiftW * 2;
    nv12_to_yv12(src, pitchSrc, srcU, pitchSrcYv12, srcV, pitchSrcYv12, extW, extH);
    nv12_to_yv12(ref0, pitchRef0, ref0U, w, ref0V, w, w, h);

    srcU += shiftH * pitchSrcYv12 + shiftW;
    srcV += shiftH * pitchSrcYv12 + shiftW;
    interp_av1_precise(srcU, pitchSrcYv12, ref0U, w, dstU, w, fx, fy, w, h);
    interp_av1_precise(srcV, pitchSrcYv12, ref0V, w, dstV, w, fx, fy, w, h);

    yv12_to_nv12(dstU, w, dstV, w, dst, pitchDst, w, h);
}

void InterpolateSingleRef(const uint8_t *refColoc, int32_t refPitch, uint8_t *dst, int32_t dstPitch,
                          H265MV mv, int32_t w, int32_t h, int32_t interp)
{
    const int32_t intx = mv.mvx >> 3;
    const int32_t inty = mv.mvy >> 3;
    const int32_t dx = (mv.mvx << 1) & 15;
    const int32_t dy = (mv.mvy << 1) & 15;
    refColoc += intx + inty * refPitch;

    const int interp0 = (interp & 0x3);
    const int interp1 = (interp >> 4) & 0x3;
    const short *fy = av1_filter_kernels[interp0][dy];
    const short *fx = av1_filter_kernels[interp1][dx];

    interp_av1_precise(refColoc, refPitch, dst, dstPitch, fx, fy, w, h);
}
void InterpolateSingleRefChroma(const uint8_t *refColoc, int32_t refPitch, uint8_t *dst, int32_t dstPitch,
                                H265MV mv, int32_t w, int32_t h, int32_t interp)
{
    const int32_t intx = (mv.mvx >> 4) << 1;
    const int32_t inty = mv.mvy >> 4;
    const int32_t dx = mv.mvx & 15;
    const int32_t dy = mv.mvy & 15;
    refColoc += intx + inty * refPitch;

    const int interp0 = (interp & 0x3);
    const int interp1 = (interp >> 4) & 0x3;
    const short *fy = (h <= 4 ? av1_short_filter_kernels : av1_filter_kernels)[interp0][dy];
    const short *fx = (w <= 4 ? av1_short_filter_kernels : av1_filter_kernels)[interp1][dx];

    interp_nv12_av1_precise(refColoc, refPitch, dst, dstPitch, fx, fy, w, h);
}

void InterpolateFirstRef(const uint8_t *refColoc, int32_t refPitch, int16_t *dst, int32_t dstPitch,
                         H265MV mv, int32_t w, int32_t h, int32_t interp)
{
    const int32_t intx = mv.mvx >> 3;
    const int32_t inty = mv.mvy >> 3;
    const int32_t dx = (mv.mvx << 1) & 15;
    const int32_t dy = (mv.mvy << 1) & 15;
    refColoc += intx + inty * refPitch;

    const int interp0 = (interp & 0x3);
    const int interp1 = (interp >> 4) & 0x3;
    const short *fy = av1_filter_kernels[interp0][dy];
    const short *fx = av1_filter_kernels[interp1][dx];

    interp_av1_precise(refColoc, refPitch, dst, dstPitch, fx, fy, w, h, COMPOUND_ROUND1_BITS);
}
void InterpolateFirstRefChroma(const uint8_t *refColoc, int32_t refPitch, int16_t *dst, int32_t dstPitch,
                               H265MV mv, int32_t w, int32_t h, int32_t interp)
{
    const int32_t intx = (mv.mvx >> 4) << 1;
    const int32_t inty = mv.mvy >> 4;
    const int32_t dx = mv.mvx & 15;
    const int32_t dy = mv.mvy & 15;
    refColoc += intx + inty * refPitch;

    const int interp0 = (interp & 0x3);
    const int interp1 = (interp >> 4) & 0x3;
    const short *fy = (h <= 4 ? av1_short_filter_kernels : av1_filter_kernels)[interp0][dy];
    const short *fx = (w <= 4 ? av1_short_filter_kernels : av1_filter_kernels)[interp1][dx];

    interp_nv12_av1_precise(refColoc, refPitch, dst, dstPitch, fx, fy, w, h);
}

void InterpolateSecondRef(const uint8_t *refColoc, int32_t refPitch, const int16_t *ref0, int32_t ref0Pitch,
                          uint8_t *dst, int32_t dstPitch, H265MV mv, int32_t w, int32_t h, int32_t interp)
{
    const int32_t intx = mv.mvx >> 3;
    const int32_t inty = mv.mvy >> 3;
    const int32_t dx = (mv.mvx << 1) & 15;
    const int32_t dy = (mv.mvy << 1) & 15;
    refColoc += intx + inty * refPitch;

    const int interp0 = (interp & 0x3);
    const int interp1 = (interp >> 4) & 0x3;
    const short *fy = av1_filter_kernels[interp0][dy];
    const short *fx = av1_filter_kernels[interp1][dx];

    interp_av1_precise(refColoc, refPitch, ref0, ref0Pitch, dst, dstPitch, fx, fy, w, h);
}
void InterpolateSecondRefChroma(const uint8_t *refColoc, int32_t refPitch, const int16_t *ref0, int32_t ref0Pitch,
                                uint8_t *dst, int32_t dstPitch, H265MV mv, int32_t w, int32_t h, int32_t interp)
{
    const int32_t intx = (mv.mvx >> 4) << 1;
    const int32_t inty = mv.mvy >> 4;
    const int32_t dx = mv.mvx & 15;
    const int32_t dy = mv.mvy & 15;
    refColoc += intx + inty * refPitch;

    const int interp0 = (interp & 0x3);
    const int interp1 = (interp >> 4) & 0x3;
    const short *fy = (h <= 4 ? av1_short_filter_kernels : av1_filter_kernels)[interp0][dy];
    const short *fx = (w <= 4 ? av1_short_filter_kernels : av1_filter_kernels)[interp1][dx];

    interp_nv12_av1_precise(refColoc, refPitch, ref0, ref0Pitch, dst, dstPitch, fx, fy, w, h);
}

int sad_general_px(const uint8_t *p1, int pitch1, const uint8_t *p2, int pitch2, int W, int H)
{
    int sum = 0;
    for (int i = 0; i < H; i++, p1+=pitch1, p2+=pitch2)
        for (int j = 0; j < W; j++)
            sum += ABS(p1[j] - p2[j]);
    return sum;
}

int sse_px(const uint8_t *p1, int pitch1, const uint8_t *p2, int pitch2, int W, int H)
{
    int sum = 0;
    for (int i = 0; i < H; i++, p1+=pitch1, p2+=pitch2)
        for (int j = 0; j < W; j++)
            sum += ABS(p1[j] - p2[j])*ABS(p1[j] - p2[j]);
    return sum;
}

void average_px(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int w, int h)
{
    for (int y = 0; y < h; y++, src1 += pitchSrc1, src2 += pitchSrc2, dst += pitchDst)
        for (int x = 0; x < w; x++)
            dst[x] = (uint8_t)((unsigned(src1[x]) + unsigned(src2[x]) + 1) >> 1);
}

struct Limits {
    int16_t HorMin, HorMax, VerMin, VerMax;
};

static const int16_t DXDY1[9][2] = {
    {-1, -1}, {0, -1}, {1, -1},
    {-1,  0}, {0,  0}, {1,  0},
    {-1,  1}, {0,  1}, {1,  1}
};
static const int16_t DXDY2[9][2] = {
    {-2, -2}, {0, -2}, {2, -2},
    {-2,  0}, {0,  0}, {2,  0},
    {-2,  2}, {0,  2}, {2,  2}
};

#define IPP_ABS( a ) ( ((a) < 0) ? (-(a)) : (a) )
typedef uint8_t PixType;
int32_t SATD8x8(const uint8_t* pSrcCur, int srcCurStep, const uint8_t* pSrcRef, int srcRefStep)
{
    int32_t i, j;
    uint32_t satd = 0;
    int16_t diff[8][8];

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++)
            diff[i][j] = (int16_t)(pSrcCur[j] - pSrcRef[j]);
        pSrcCur += srcCurStep;
        pSrcRef += srcRefStep;
    }

    for (i = 0; i < 8; i++) {
        int16_t t0 = diff[i][0] + diff[i][4];
        int16_t t4 = diff[i][0] - diff[i][4];
        int16_t t1 = diff[i][1] + diff[i][5];
        int16_t t5 = diff[i][1] - diff[i][5];
        int16_t t2 = diff[i][2] + diff[i][6];
        int16_t t6 = diff[i][2] - diff[i][6];
        int16_t t3 = diff[i][3] + diff[i][7];
        int16_t t7 = diff[i][3] - diff[i][7];
        int16_t s0 = t0 + t2;
        int16_t s2 = t0 - t2;
        int16_t s1 = t1 + t3;
        int16_t s3 = t1 - t3;
        int16_t s4 = t4 + t6;
        int16_t s6 = t4 - t6;
        int16_t s5 = t5 + t7;
        int16_t s7 = t5 - t7;
        diff[i][0] = s0 + s1;
        diff[i][1] = s0 - s1;
        diff[i][2] = s2 + s3;
        diff[i][3] = s2 - s3;
        diff[i][4] = s4 + s5;
        diff[i][5] = s4 - s5;
        diff[i][6] = s6 + s7;
        diff[i][7] = s6 - s7;
    }
    for (i = 0; i < 8; i++) {
        int16_t t0 = diff[0][i] + diff[4][i];
        int16_t t4 = diff[0][i] - diff[4][i];
        int16_t t1 = diff[1][i] + diff[5][i];
        int16_t t5 = diff[1][i] - diff[5][i];
        int16_t t2 = diff[2][i] + diff[6][i];
        int16_t t6 = diff[2][i] - diff[6][i];
        int16_t t3 = diff[3][i] + diff[7][i];
        int16_t t7 = diff[3][i] - diff[7][i];
        int16_t s0 = t0 + t2;
        int16_t s2 = t0 - t2;
        int16_t s1 = t1 + t3;
        int16_t s3 = t1 - t3;
        int16_t s4 = t4 + t6;
        int16_t s6 = t4 - t6;
        int16_t s5 = t5 + t7;
        int16_t s7 = t5 - t7;
        satd += IPP_ABS(s0 + s1);
        satd += IPP_ABS(s0 - s1);
        satd += IPP_ABS(s2 + s3);
        satd += IPP_ABS(s2 - s3);
        satd += IPP_ABS(s4 + s5);
        satd += IPP_ABS(s4 - s5);
        satd += IPP_ABS(s6 + s7);
        satd += IPP_ABS(s6 - s7);
    }

    return satd;
}

void SATD8x8Pair(const uint8_t* pSrcCur, int srcCurStep, const uint8_t* pSrcRef, int srcRefStep, int32_t* satdPair)
{
    satdPair[0] = SATD8x8(pSrcCur + 0, srcCurStep, pSrcRef + 0, srcRefStep);
    satdPair[1] = SATD8x8(pSrcCur + 8, srcCurStep, pSrcRef + 8, srcRefStep);
}

int32_t satdNxN(const PixType *src, int32_t pitchSrc, const PixType *rec, int32_t pitchRec, int32_t size)
{
    uint32_t satdTotal = 0;
    int32_t satd[2] = {0, 0};

    /* assume height and width are multiple of 4 */
    assert(size == 4 || size == 8 || size == 16 || size == 32 || size == 64);

    switch (size) {
    case 64:
        for (int32_t j = 0; j < 64; j += 8, src += pitchSrc * 8, rec += pitchRec * 8) {
            SATD8x8Pair(src + 0,  pitchSrc, rec + 0,  pitchRec, satd);
            //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
            satdTotal += satd[0] + satd[1];
            SATD8x8Pair(src + 16, pitchSrc, rec + 16, pitchRec, satd);
            //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
            satdTotal += satd[0] + satd[1];
            SATD8x8Pair(src + 32, pitchSrc, rec + 32, pitchRec, satd);
            //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
            satdTotal += satd[0] + satd[1];
            SATD8x8Pair(src + 48, pitchSrc, rec + 48, pitchRec, satd);
            //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
            satdTotal += satd[0] + satd[1];
        }
        //satdTotal = (satdTotal + 2) >> 2;
        break;
    case 32:
        for (int32_t j = 0; j < 32; j += 8, src += pitchSrc * 8, rec += pitchRec * 8) {
            SATD8x8Pair(src + 0,  pitchSrc, rec + 0,  pitchRec, satd);
            //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
            satdTotal += satd[0] + satd[1];
            SATD8x8Pair(src + 16, pitchSrc, rec + 16, pitchRec, satd);
            //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
            satdTotal += satd[0] + satd[1];
        }
        //satdTotal = (satdTotal + 2) >> 2;
        break;
    case 16:
        SATD8x8Pair(src, pitchSrc, rec, pitchRec, satd);
        //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
        satdTotal += satd[0] + satd[1];
        src += pitchSrc * 8;
        rec += pitchRec * 8;
        SATD8x8Pair(src, pitchSrc, rec, pitchRec, satd);
        satdTotal += satd[0] + satd[1];
        //satdTotal += ((satd[0] + 2) >> 2) + ((satd[1] + 2) >> 2);
        //satdTotal = (satdTotal + 2) >> 2;
        break;
    case 8:
        satd[0] = SATD8x8(src, pitchSrc, rec, pitchRec);
        //satdTotal += (satd[0] + 2) >> 2;
        satdTotal += (satd[0]);
        break;
    default: assert(0);
    }
    return satdTotal;
}

//void average_px(const uint8_t *src1, int pitchSrc1, const uint8_t *src2, int pitchSrc2, uint8_t *dst, int pitchDst, int w, int h)
//{
//    for (int y = 0; y < h; y++, src1 += pitchSrc1, src2 += pitchSrc2, dst += pitchDst)
//        for (int x = 0; x < w; x++)
//            dst[x] = (uint8_t)((unsigned(src1[x]) + unsigned(src2[x]) + 1) >> 1);
//}

uint16_t g_interp_bits[] = {
    12, 5071, 4182,
    1813, 3141, 4162,
    3072, 3650, 18,
    54, 2385, 2585,
    9, 6033, 3504,
    1748, 3660, 4169,
    3584, 4292, 9,
    190, 2468, 1226,
    18, 5093, 3640,
    2048, 3335, 4144,
    3284, 3173, 21,
    92, 1948, 2324,
    26, 5395, 2592,
    1513, 4207, 3386,
    4096, 4566, 6,
    467, 4243, 602
};

static uint32_t RD32(uint32_t d, uint32_t r, uint32_t lambda) {
    assert(d < 0x100000);
    assert(uint64_t(d << 11) + uint64_t(lambda * r) < 0x100000000);
    return (d << 11) + lambda * r;
}

int32_t InterpolateDecision_px(const H265VideoParam &par, int32_t width, H265MV mv[2], int8_t _refIdx[2],
                              int32_t ctxInterpZ, int32_t skip, const uint8_t *src, const uint8_t *ref0Luma,
                              const uint8_t *ref1Luma, const uint8_t *ref0Chroma, const uint8_t *ref1Chroma,
                              int32_t framePitch, uint8_t *outLuma, uint8_t *outChroma, int32_t outPitch)
{
    const int32_t w = width;
    const int32_t h = w;
    const uint8_t *refLuma[2] = { ref0Luma, ref1Luma };
    const uint8_t *refChroma[2] = { ref0Chroma, ref1Chroma };
    __ALIGN32 uint8_t prediction[64*64], tmp[64*64];
    __ALIGN32 int16_t tmp16s[64*64];

    uint32_t bestSad = UINT_MAX;
    int32_t bestInterpType = 0;

    int32_t refIdx = (_refIdx[0] + 1) >> 1;
    int32_t useCompound = !(_refIdx[1] == NONE_FRAME);

    int32_t bestInterp0 = 0;

    static int counter = 0;

    int32_t ctx0 = (ctxInterpZ & 15) *3;
    int32_t ctx1 = (ctxInterpZ >> 8) *3 + 24;

    // pass1
    for (int32_t interp0 = EIGHTTAP; interp0 < SWITCHABLE_FILTERS; interp0++)
    {
        int32_t interp1 = interp0;//EIGHTTAP;
        int32_t interp = interp0 + (interp1 << 4);

        Interpolate(refLuma[refIdx], framePitch, prediction, 64, mv[0], w, h, 0, interp);
        if (useCompound) {
            Interpolate(refLuma[1], framePitch, tmp, 64, mv[1], w, h, 0, interp);
            average_px(prediction, 64, tmp, 64, prediction, 64, w, h);
        }

        uint32_t cost;
        if (skip) {
            uint32_t sse = sse_px(src, framePitch, prediction, 64, w, h);
            cost = sse + uint32_t(par.lambda * (g_interp_bits[ctx0 + interp0] + g_interp_bits[ctx1 + interp1]) + 0.5f);
        } else {
            uint32_t satd = satdNxN(src, framePitch, prediction, 64, w);
            cost = RD32((satd + 2) >> 2, g_interp_bits[ctx0 + interp0] + g_interp_bits[ctx1 + interp1], par.lambdaInt32);
        }

        if (bestSad > cost) {
            bestInterpType = interp;
            bestSad = cost;
            bestInterp0 = interp0;
            bestInterpType = interp0 + (interp1 << 4);
        }
    }
#if 0
    // pass2
    int32_t interp0 = bestInterp0;
    for (int32_t interp1 = EIGHTTAP+1; interp1 < SWITCHABLE_FILTERS; interp1++)
    {
        int32_t interp = interp0 + (interp1 << 4);

        Interpolate(refLuma[refIdx], framePitch, prediction, 64, mv[0], w, h, 0, interp);
        if (useCompound) {
            Interpolate(refLuma[1], framePitch, tmp, 64, mv[1], w, h, 0, interp);
            average_px(prediction, 64, tmp, 64, prediction, 64, w, h);
        }

        uint32_t cost;
        if (skip) {
            uint32_t sse = sse_px(src, framePitch, prediction, 64, w, h);
            cost = sse + uint32_t(par.lambda * (g_interp_bits[ctx0 + interp0] + g_interp_bits[ctx1 + interp1]) + 0.5f);
        } else {
            int32_t satd = satdNxN(src, framePitch, prediction, 64, w);
            cost = RD32((satd + 2) >> 2, g_interp_bits[ctx0 + interp0] + g_interp_bits[ctx1 + interp1], par.lambdaInt32);
        }

        if (bestSad > cost) {
            bestInterpType = interp;
            bestSad = cost;
        }
    }
#endif

    // make predictor
    if (!useCompound) {
        InterpolateSingleRef(refLuma[refIdx], framePitch, outLuma, outPitch, mv[0], w, h, bestInterpType);
        InterpolateSingleRefChroma(refChroma[refIdx], framePitch, outChroma, outPitch, mv[0], w/2, h/2, bestInterpType);
    } else {
        InterpolateFirstRef(refLuma[0], framePitch, tmp16s, 64, mv[0], w, h, bestInterpType);
        InterpolateSecondRef(refLuma[1], framePitch, tmp16s, 64, outLuma, outPitch, mv[1], w, h, bestInterpType);
        InterpolateFirstRefChroma(refChroma[1], framePitch, tmp16s, 64, mv[1], w/2, h/2, bestInterpType); // debug
        InterpolateFirstRefChroma(refChroma[0], framePitch, tmp16s, 64, mv[0], w/2, h/2, bestInterpType);
        InterpolateSecondRefChroma(refChroma[1], framePitch, tmp16s, 64, outChroma, outPitch, mv[1], w/2, h/2, bestInterpType);
    }

    return bestInterpType;
}

#define IPP_MIN_16S    (-32768 )
#define IPP_MAX_16S    ( 32767 )



#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))

int16_t clampMvRowAV1(int16_t mvy, int32_t border, BlockSize sbType, int32_t miRow, int32_t miRows) {
    int32_t bh = block_size_high_8x8[sbType];
    int32_t mbToTopEdge = -((miRow * MI_SIZE) * 8);
    int32_t mbToBottomEdge = ((miRows/* - bh*/ - miRow) * MI_SIZE) * 8;
    return (int16_t)Saturate(mbToTopEdge - border - bh * MI_SIZE * 8, mbToBottomEdge + border, mvy);
}

int16_t clampMvColAV1(int16_t mvx, int32_t border, BlockSize sbType, int32_t miCol, int32_t miCols) {
    int32_t bw = block_size_wide_8x8[sbType];
    int32_t mbToLeftEdge = -((miCol * MI_SIZE) * 8);
    int32_t mbToRightEdge = ((miCols/* - bw*/ - miCol) * MI_SIZE) * 8;
    return (int16_t)Saturate(mbToLeftEdge - border - bw * MI_SIZE * 8, mbToRightEdge + border, mvx);
}

static inline void ClampMvRefAV1(H265MV *mv, BlockSize sbType, int32_t miRow, int32_t miCol, const H265VideoParam *m_par)
{
    mv->mvy = clampMvRowAV1(mv->mvy, MV_BORDER_AV1, sbType, miRow, m_par->miRows);
    mv->mvx = clampMvColAV1(mv->mvx, MV_BORDER_AV1, sbType, miCol, m_par->miCols);
}

static const int32_t INTER_FILTER_DIR_OFFSET = (SWITCHABLE_FILTERS + 1) * 2;
static const int32_t INTER_FILTER_COMP_OFFSET = SWITCHABLE_FILTERS + 1;

static const int32_t DEF_FILTER = 0;

int32_t GetCtxInterpBothAV1Fast3(const ModeInfo *above, const ModeInfo *left, const int8_t refIdx[2], int isAboveReal, int isLeftReal)
{
    isAboveReal;
    isLeftReal;
    int32_t left_type0 = DEF_FILTER;
    int32_t left_type1 = DEF_FILTER;
    int32_t above_type0 = DEF_FILTER;
    int32_t above_type1 = DEF_FILTER;
    int32_t filter_ctx0 = SWITCHABLE_FILTERS;
    int32_t filter_ctx1 = SWITCHABLE_FILTERS;

    if (left && (left->refIdx[0] == refIdx[0] || left->refIdx[1] == refIdx[0])) {
        left_type0 = left->interp & 15;
        left_type1 = left->interp >> 4;
    }

    if (above && (above->refIdx[0] == refIdx[0] || above->refIdx[1] == refIdx[0])) {
        above_type0 = above->interp & 15;
        above_type1 = above->interp >> 4;
    }

    const int32_t comp_offset = (refIdx[1] > INTRA_FRAME) ? INTER_FILTER_COMP_OFFSET * 257 : 0;

    if      (left_type0 == above_type0)         filter_ctx0 = left_type0;
    //else if (left_type0 == SWITCHABLE_FILTERS)  filter_ctx0 = above_type0;
    //else if (above_type0 == SWITCHABLE_FILTERS) filter_ctx0 = left_type0;

    if      (left_type1 == above_type1)         filter_ctx1 = left_type1;
    //else if (left_type1 == SWITCHABLE_FILTERS)  filter_ctx1 = above_type1;
    //else if (above_type1 == SWITCHABLE_FILTERS) filter_ctx1 = left_type1;

    return comp_offset;// + filter_ctx0 + ((filter_ctx1 + INTER_FILTER_DIR_OFFSET) << 8);
}

bool IsJoinable(const ModeInfo *mi1, const ModeInfo *mi2) {
    return mi1->sbType == mi2->sbType && mi1->refIdx[0] == mi2->refIdx[0] && mi1->refIdx[1] == mi2->refIdx[1] && mi1->mv == mi2->mv;
}

int RunCpu(const H265VideoParam& par, const uint8_t *src, const uint8_t *ref0Luma, const uint8_t *ref1Luma,
           const uint8_t *ref0Chroma, const uint8_t *ref1Chroma, int32_t framePitch, ModeInfo* frameCuData,
           uint8_t *outLuma, uint8_t *outChroma, int32_t outPitch)
{
    for (int32_t row = 0, ctbAddr = 0; row < par.sb64Rows; row++) {
        for (int32_t col = 0; col < par.sb64Cols; col++, ctbAddr++) {

            int32_t m_ctbPelX = col * par.MaxCUSize;
            int32_t m_ctbPelY = row * par.MaxCUSize;
            ModeInfo *m_data = frameCuData + (col << 3) + (row << 3) * par.miPitch;

            for (int32_t i = 0; i < MAX_NUM_PARTITIONS;) {
                const int32_t rasterIdx = h265_scan_z2r4[i];
                const int32_t x4 = (rasterIdx & 15);
                const int32_t y4 = (rasterIdx >> 4);
                const int32_t sbx = x4 << 2;
                const int32_t sby = y4 << 2;
                ModeInfo *mi = m_data + (sbx >> 3) + (sby >> 3) * par.miPitch;
                int32_t num4x4 = num_4x4_blocks_lookup[mi->sbType];

                if (mi->mode == OUT_OF_PIC) {
                    i += num4x4;
                    continue;
                }
#ifdef JOIN_MI
                if (mi[0].sbType == BLOCK_8X8 && ((x4 | y4) & 3) == 0 && IsJoinable(mi, mi + 1) &&
                    IsJoinable(mi, mi+par.miPitch) && IsJoinable(mi, mi+par.miPitch + 1))
                {
                    mi[0].sbType = BLOCK_16X16;
                    mi[0].skip &= mi[1].skip & mi[par.miPitch].skip & mi[par.miPitch + 1].skip;
                    num4x4 *= 4;
                }
#endif
                const BlockSize bsz = mi->sbType;
                const int32_t log2w = block_size_wide_4x4_log2[bsz];
                const int32_t log2h = block_size_high_4x4_log2[bsz];
                const int32_t num4x4w = 1 << log2w;
                const int32_t num4x4h = 1 << log2h;
                const int32_t sbw = num4x4w << 2;
                const int32_t sbh = num4x4h << 2;
                const int32_t miRow = (m_ctbPelY >> 3) + (y4 >> 1);
                const int32_t miCol = (m_ctbPelX >> 3) + (x4 >> 1);

                const int32_t miRowStart = (m_ctbPelY >> 3);
                const int32_t miColStart = (m_ctbPelX >> 3);

                const ModeInfo *aboveZ = (miRow > miRowStart) ? mi - par.miPitch : NULL;
                const ModeInfo *leftZ  = (miCol > miColStart) ? mi - 1 : NULL;
                const int32_t ctxInterpZ = GetCtxInterpBothAV1Fast3(aboveZ, leftZ, mi->refIdx, miRow > 0, miCol > 0);

                H265MV clampedMV[2] = {mi->mv[0], mi->mv[1]};
                //ClampMvRefAV1(&clampedMV[0], bsz, miRow, miCol, &par);
                //ClampMvRefAV1(&clampedMV[1], bsz, miRow, miCol, &par);

                int32_t filterType = InterpolateDecision_px(
                    par, sbw, clampedMV, mi->refIdx, ctxInterpZ, mi->skip,
                    src        + m_ctbPelX + sbx + (m_ctbPelY + sby) * framePitch,
                    ref0Luma   + m_ctbPelX + sbx + (m_ctbPelY + sby) * framePitch,
                    ref1Luma   + m_ctbPelX + sbx + (m_ctbPelY + sby) * framePitch,
                    ref0Chroma + m_ctbPelX + sbx + (m_ctbPelY + sby) * framePitch / 2,
                    ref1Chroma + m_ctbPelX + sbx + (m_ctbPelY + sby) * framePitch / 2,
                    framePitch,
                    outLuma    + m_ctbPelX + sbx + (m_ctbPelY + sby) * outPitch,
                    outChroma  + m_ctbPelX + sbx + (m_ctbPelY + sby) * outPitch / 2,
                    outPitch);

                mi->interp0 = filterType & 0x3;
                mi->interp1 = (filterType >> 4) & 0x3;

                //printf("\n %i %i \n", i, filterType);
                //PropagateSubPart(mi, mi, par.miPitch, sbw >> 3, sbh >> 3);

                i += num4x4;
            }
        }
    }

    return 0;
}

template <typename T> SurfaceIndex *get_index(T *cm_resource) {
    SurfaceIndex *index = 0;
    int res = cm_resource->GetIndex(index);
    THROW_CM_ERR(res);
    return index;
}

void RunGpu(const H265VideoParam& par, const uint8_t *src, const uint8_t *ref0Luma, const uint8_t *ref1Luma,
            const uint8_t *ref0Chroma, const uint8_t *ref1Chroma, int32_t framePitch, ModeInfo* frameCuData,
            uint8_t *outLuma, uint8_t *outChroma, int32_t outPitch)
{
    mfxU32 version = 0;
    CmDevice *device = 0;
    THROW_CM_ERR(CreateCmDevice(device, version));
    THROW_CM_ERR(device->InitPrintBuffer());

    CmProgram *program = 0;
    GPU_PLATFORM hw_type;
    size_t size = sizeof(mfxU32);
    THROW_CM_ERR(device->GetCaps(CAP_GPU_PLATFORM, size, &hw_type));

#ifdef CMRT_EMU
    hw_type = PLATFORM_INTEL_HSW;
#endif // CMRT_EMU
    switch (hw_type) {
    case PLATFORM_INTEL_HSW:
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_interpolate_decision_single_hsw, sizeof(genx_av1_interpolate_decision_single_hsw), program, "nojitter"));
        break;
    case PLATFORM_INTEL_BDW:
    case PLATFORM_INTEL_CHV:
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_interpolate_decision_single_bdw, sizeof(genx_av1_interpolate_decision_single_bdw), program, "nojitter"));
        break;
    case PLATFORM_INTEL_SKL:
        THROW_CM_ERR(device->LoadProgram((void *)genx_av1_interpolate_decision_single_skl, sizeof(genx_av1_interpolate_decision_single_skl), program, "nojitter"));
        break;
    default:
        throw cm_error(CM_FAILURE, __FILE__, __LINE__, "Unknown HW type");
    }

    CmKernel *kernel = 0;
    THROW_CM_ERR(device->CreateKernel(program, CM_KERNEL_FUNCTION(InterpolateDecisionSingle), kernel));

    MdControl control = {};
    control.lambda = par.lambda;
    control.lambdaInt = (uint16_t)par.lambdaInt32;
    CmBuffer *param = 0;
    CmSurface2D *inSrc = 0;
    CmSurface2D *inRef0 = 0;
    CmSurface2D *inRef1 = 0;
    CmSurface2D *outLumaCm = 0;
    CmSurface2D *outChromaCm = 0;

    THROW_CM_ERR(device->CreateBuffer(sizeof(MdControl), param));
    THROW_CM_ERR(device->CreateSurface2D(par.Width, par.Height, CM_SURFACE_FORMAT_NV12, inSrc));
    THROW_CM_ERR(device->CreateSurface2D(par.Width, par.Height, CM_SURFACE_FORMAT_NV12, inRef0));
    THROW_CM_ERR(device->CreateSurface2D(par.Width, par.Height, CM_SURFACE_FORMAT_NV12, inRef1));
    THROW_CM_ERR(device->CreateSurface2D(par.Width, par.Height, CM_SURFACE_FORMAT(CM_SURFACE_FORMAT_P8), outLumaCm));
    THROW_CM_ERR(device->CreateSurface2D(par.Width, par.Height/2, CM_SURFACE_FORMAT(CM_SURFACE_FORMAT_P8), outChromaCm));

    THROW_CM_ERR(param->WriteSurface((const unsigned char*)&control, NULL));
    THROW_CM_ERR(inSrc->WriteSurfaceStride(src, NULL, framePitch));

    std::vector<uint8_t> tmp(par.Width * par.Height * 3 / 2);
    for (int i = 0; i < par.Height; i++)
        memcpy(tmp.data() + i * par.Width, ref0Luma + i * framePitch, par.Width);
    for (int i = 0; i < par.Height/2; i++)
        memcpy(tmp.data() + (par.Height + i) * par.Width, ref0Chroma + i * framePitch, par.Width);

    THROW_CM_ERR(inRef0->WriteSurface(tmp.data(), NULL));

    for (int i = 0; i < par.Height; i++)
        memcpy(tmp.data() + i * par.Width, ref1Luma + i * framePitch, par.Width);
    for (int i = 0; i < par.Height/2; i++)
        memcpy(tmp.data() + (par.Height + i) * par.Width, ref1Chroma + i * framePitch, par.Width);

    THROW_CM_ERR(inRef1->WriteSurface(tmp.data(), NULL));

    // modeinfo
    CmSurface2DUP *mi_up = nullptr;
    const uint32_t width_mi = par.sb64Cols * 8;
    const uint32_t height_mi = par.sb64Rows * 8;
    uint32_t pitch_mi;
    uint32_t size_mi;
    THROW_CM_ERR(device->GetSurface2DInfo(width_mi * sizeof(ModeInfo), height_mi, CM_SURFACE_FORMAT_P8, pitch_mi, size_mi));
    mode_info_ptr mi_sys = make_aligned_ptr<ModeInfo>(size_mi, 0x1000);
    THROW_CM_ERR(device->CreateSurface2DUP(width_mi * sizeof(ModeInfo), height_mi, CM_SURFACE_FORMAT_P8, mi_sys.get(), mi_up));

    memset(mi_sys.get(), 0, sizeof(ModeInfo) * size_mi);
    for (int i = 0; i < par.sb64Rows * 8; i++)
        memcpy((char *)mi_sys.get() + i * pitch_mi,
            frameCuData +  + i * par.miPitch,
            par.sb64Cols * 8 * sizeof(ModeInfo));

    vector<SurfaceIndex,8> ref_ids;
    ref_ids[0] = *get_index(inRef0);
    ref_ids[1] = *get_index(inRef1);
    ref_ids[2] = *get_index(inRef1);
    ref_ids[3] = *get_index(inRef1);
    ref_ids[4] = *get_index(inRef1);
    ref_ids[5] = *get_index(inRef1);
    ref_ids[6] = *get_index(inRef1);

    const mfxU16 BlockW = 32;
    const mfxU16 BlockH = 32;
    mfxU32 tsWidth = (par.Width + BlockW - 1) / BlockW;
    mfxU32 tsHeight = (par.Height + BlockH - 1) / BlockH;

    CmThreadSpace * threadSpace = 0;
    THROW_CM_ERR(kernel->SetThreadCount(tsWidth * tsHeight));
    THROW_CM_ERR(device->CreateThreadSpace(tsWidth, tsHeight, threadSpace));
    THROW_CM_ERR(threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY));

    int pred_padding = 0;
    int argIdx = 0;
    const uint32_t yoff = 0;
    THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), get_index(param)));
    THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), get_index(inSrc)));
    THROW_CM_ERR(kernel->SetKernelArg(argIdx++, ref_ids.get_size_data(), ref_ids.get_addr_data()));
    THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), get_index(mi_up)));
    THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), get_index(outLumaCm)));
    THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(SurfaceIndex), get_index(outChromaCm)));
    THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(pred_padding), &pred_padding));
    THROW_CM_ERR(kernel->SetKernelArg(argIdx++, sizeof(yoff), &yoff));

    CmTask * task = 0;
    THROW_CM_ERR(device->CreateTask(task));
    THROW_CM_ERR(task->AddKernel(kernel));

    CmQueue *queue = 0;
    CmEvent *e = 0;
    THROW_CM_ERR(device->CreateQueue(queue));

    //res = queue->Enqueue(task, e, threadSpace);
    THROW_CM_ERR(queue->Enqueue(task, e, threadSpace));
    THROW_CM_ERR(e->WaitForTaskFinished());
    THROW_CM_ERR(device->FlushPrintBuffer());

    // copy updated data: GPU -> CPU
    for (int i = 0; i < par.sb64Rows * 8; i++)
        memcpy(frameCuData + i * par.miPitch,
        (char *)mi_sys.get() + i * pitch_mi,
        par.sb64Cols * 8 * sizeof(ModeInfo));


    THROW_CM_ERR(outLumaCm->ReadSurfaceStride(outLuma, NULL, outPitch));
    THROW_CM_ERR(outChromaCm->ReadSurfaceStride(outChroma, NULL, outPitch));

    //device->FlushPrintBuffer();

#if !defined CMRT_EMU
    printf("TIME = %.3f ms ",  GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif

    THROW_CM_ERR(device->DestroyTask(task));
    THROW_CM_ERR(device->DestroySurface2DUP(mi_up));
    THROW_CM_ERR(device->DestroySurface(outLumaCm));
    THROW_CM_ERR(device->DestroySurface(outChromaCm));
    THROW_CM_ERR(device->DestroySurface(inRef1));
    THROW_CM_ERR(device->DestroySurface(inRef0));
    THROW_CM_ERR(device->DestroySurface(inSrc));
    THROW_CM_ERR(device->DestroyThreadSpace(threadSpace));
    THROW_CM_ERR(device->DestroyKernel(kernel));
    THROW_CM_ERR(device->DestroyProgram(program));

    THROW_CM_ERR(::DestroyCmDevice(device));
}

int ReadFromFile(FILE* f, uint8_t* src, int32_t pitch, int32_t width, int32_t height)
{
    for (int32_t y = 0; y < height; y++) {
        size_t bytesRead = fread(src + y * pitch, 1, width, f);
        if (bytesRead != width)
            return FAILED;
    }

    return 0;
}

template <typename T>
void PadOnePix(T *frame, int32_t pitch, int32_t width, int32_t height, int32_t padding)
{
    T *ptr = frame;
    for (int32_t i = 0; i < height; i++, ptr += pitch) {

        for (int32_t x = 1; x < padding; x++) {
            ptr[-x] = ptr[0];
            ptr[width - 1 + x] = ptr[width - 1];
        }
    }

    for (int32_t x = 1; x < padding; x++) {
        ptr = frame - padding;
        memcpy(ptr - x*pitch, ptr, pitch * sizeof(T));
    }

    for (int32_t x = 1; x < padding; x++) {
        ptr = frame + (height - 1 + x) * pitch - padding;
        memcpy(ptr, ptr - pitch, pitch * sizeof(T));
    }
}

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

void setup_video_params(H265VideoParam *par)
{
    par->Width = global_test_params::width;
    par->Height = global_test_params::height;
    par->miRows = uint16_t((par->Height + 7) >> 3);
    par->miCols = uint16_t((par->Width + 7) >> 3);
    par->sb64Rows = (par->miRows + 7) >> 3;
    par->sb64Cols = (par->miCols + 7) >> 3;
    par->miPitch = (par->sb64Cols << 3);
    par->PicWidthInCtbs = par->sb64Cols;
    par->PicHeightInCtbs = par->sb64Rows;
    par->MaxCUSize = 64;
    par->PAD = MV_BORDER_AV1;
    par->lambdaInt32 = 15;
    int q = 24;
    par->lambda = 88 * q * q / 24.f / 512 / 16 / 128;
}

void CopyMI(ModeInfo* dst, ModeInfo* src, int miRows, int miCols, int miPitch)
{
    for (int row = 0; row < miRows; row++) {
        for (int col = 0; col < miCols; col++) {
            dst[row*miPitch + col] = src[row*miPitch + col];
        }
    }
}

int main(int argc, char **argv)
{
    try {
        parse_cmd(argc, argv);
        print_hw_caps();

        printf("Running with %s %dx%d\n", global_test_params::yuv, global_test_params::width, global_test_params::height);

        FILE *f = fopen(global_test_params::yuv, "rb");
        if (!f)
            return FAILED;

        H265VideoParam par;
        setup_video_params(&par);

        // input data (src, 2 references, MeData)
        int32_t lumaFrameSizePadded = (par.Width + 2 * par.PAD) * (par.Height + 2 * par.PAD);
        int32_t chromaFrameSizePadded = (par.Width + 2 * par.PAD) * (par.Height/2 + 2 * par.PAD);

        std::vector<uint8_t> srcBuf(lumaFrameSizePadded + chromaFrameSizePadded);
        std::vector<uint8_t> ref0LumaBuf(lumaFrameSizePadded + chromaFrameSizePadded);
        std::vector<uint8_t> ref1LumaBuf(lumaFrameSizePadded + chromaFrameSizePadded);

        int32_t framePitch = par.Width + 2*par.PAD;
        uint8_t *src        = srcBuf.data()      + par.PAD + par.PAD * framePitch;
        uint8_t *ref0Luma   = ref0LumaBuf.data() + par.PAD + par.PAD * framePitch;
        uint8_t *ref1Luma   = ref1LumaBuf.data() + par.PAD + par.PAD * framePitch;
        uint8_t *srcChroma  = src + lumaFrameSizePadded;
        uint8_t *ref0Chroma = ref0Luma + lumaFrameSizePadded;
        uint8_t *ref1Chroma = ref1Luma + lumaFrameSizePadded;

        CHECK_ERR(ReadFromFile(f, src,        framePitch, par.Width,   par.Height));
        CHECK_ERR(ReadFromFile(f, srcChroma,  framePitch, par.Width/2, par.Height/2)); // not used
        CHECK_ERR(ReadFromFile(f, ref0Luma,   framePitch, par.Width,   par.Height));
        CHECK_ERR(ReadFromFile(f, ref0Chroma, framePitch, par.Width/2, par.Height/2));
        CHECK_ERR(ReadFromFile(f, ref1Luma,   framePitch, par.Width,   par.Height));
        CHECK_ERR(ReadFromFile(f, ref1Chroma, framePitch, par.Width/2, par.Height/2));
        fclose(f);

        PadOnePix(src,        framePitch, par.Width,   par.Height,   par.PAD);
        PadOnePix(ref0Luma,   framePitch, par.Width,   par.Height,   par.PAD);
        PadOnePix(ref1Luma,   framePitch, par.Width,   par.Height,   par.PAD);
        PadOnePix((uint16_t*)ref0Chroma, framePitch/2, par.Width/2, par.Height/2, par.PAD/2);
        PadOnePix((uint16_t*)ref1Chroma, framePitch/2, par.Width/2, par.Height/2, par.PAD/2);

        int32_t outPitch = par.PicWidthInCtbs * 64;
        std::vector<uint8_t> outLumaCpu(outPitch * par.Height);
        std::vector<uint8_t> outLumaGpu(outPitch * par.Height);
        std::vector<uint8_t> outChromaCpu(outPitch * par.Height/2);
        std::vector<uint8_t> outChromaGpu(outPitch * par.Height/2);

        rand16u localrand;
        localrand.init(12345);

        mode_info_ptr mi_gold = make_aligned_ptr<ModeInfo>(par.sb64Rows * 8 * par.miPitch, 32); // [read-only] to check integrity after kernel processing
        memset(mi_gold.get(), 0, sizeof(ModeInfo) * par.sb64Rows * 8 * par.miPitch);

        RandomModeDecision(localrand, par, mi_gold.get());
        CheckModeInfo(par, mi_gold.get());

        mode_info_ptr mi_cpu = make_aligned_ptr<ModeInfo>(par.sb64Rows * 8 * par.miPitch, 32);
        mode_info_ptr mi_gpu = make_aligned_ptr<ModeInfo>(par.sb64Rows * 8 * par.miPitch, 32);

        memcpy(mi_cpu.get(), mi_gold.get(), sizeof(ModeInfo) * par.sb64Rows * 8 * par.miPitch);
        memcpy(mi_gpu.get(), mi_gold.get(), sizeof(ModeInfo) * par.sb64Rows * 8 * par.miPitch);

        RunCpu(par, src, ref0Luma, ref1Luma, ref0Chroma, ref1Chroma, framePitch, mi_cpu.get(), outLumaCpu.data(), outChromaCpu.data(), outPitch);
        RunGpu(par, src, ref0Luma, ref1Luma, ref0Chroma, ref1Chroma, framePitch, mi_gpu.get(), outLumaGpu.data(), outChromaGpu.data(), outPitch);

        int32_t res = PASSED;
        res |= CompareFilterType(mi_cpu.get(), mi_gpu.get(), par);
        res |= Compare(outLumaCpu.data(), outLumaGpu.data(), outPitch, par.Width, par.Height, "BAD Luma pixels");
        res |= Compare(outChromaCpu.data(), outChromaGpu.data(), outPitch, par.Width, par.Height/2, "BAD Chroma pixels");

        printf(res == PASSED ? "passed\n" : "FAILED\n");
        return res;

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
