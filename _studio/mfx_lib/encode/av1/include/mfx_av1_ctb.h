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


#pragma once

#include "mfx_common.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include <limits.h> /* for INT_MAX on Linux/Android */
#include "mfx_av1_defs.h"

#include <algorithm>

namespace AV1Enc {

    struct AV1VideoParam;
    struct FrameData;
    class Frame;

    struct MvLimits {
        int col_min;
        int col_max;
        int row_min;
        int row_max;
    };
#ifdef MEMOIZE_NUMCAND
    template <typename PixType> struct MemCand {
        int32_t   count;
        int32_t  *satd8[MEMOIZE_NUMCAND];
        AV1MV   mv[MEMOIZE_NUMCAND][2];
        int8_t    refIdx[MEMOIZE_NUMCAND][2];
        int32_t   list[MEMOIZE_NUMCAND];
        PixType *predBuf [MEMOIZE_NUMCAND];
        void Init() { count = 0; }
        void Init(uint32_t idx, int32_t *buf, PixType *pBuf) { if (idx < MEMOIZE_NUMCAND) { satd8[idx] = buf; predBuf[idx] = pBuf; } }
    };
#endif

    template <typename PixType> struct MemPred {
        bool    done;
        AV1MV  mv;
        int16_t *predBufHi;
        PixType *predBuf;
        void Init() {done = false;}
        void Init(int16_t *bufHi, PixType *buf) { done = false; predBufHi = bufHi; predBuf = buf; }
    };

    struct MemHad {
        int32_t  count;
        AV1MV  mv[4];
        int32_t *satd8[4];
        void Init() { count = 0; }
        void Init(int32_t *buf0, int32_t *buf1, int32_t *buf2, int32_t *buf3) {
            count = 0; satd8[0] = buf0; satd8[1] = buf1; satd8[2] = buf2;  satd8[3] = buf3;
        }
    };

    struct MemBest {
        int32_t  done;
        AV1MV  mv;
        void Init() {done = 0;}
    };

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

    struct AV1MvRefs {
        int16_t interModeCtx[COMP_VAR0 + 1];
        int32_t refMvCount[COMP_VAR0 + 1];
        int32_t nearestMvCount[COMP_VAR0 + 1];
        AV1MVCand refMvStack[COMP_VAR0 + 1][MAX_REF_MV_STACK_SIZE];
    };

    struct MvRefsAv1Fast {
        AV1MV     singleMvs[3][4];     // 4 mvs for LAST,GOLD,ALT
        AV1MVPair compMvs[4];          // 4 mvs for LAST+ALT=COMP_VAR0
        int32_t     numMvs[4];           // total number of mv
        int32_t     numNearestMvs[4];    // number of mv from top, left and top-right
                                        // not counting top-left, bottom-left and colocated
        int32_t     modeContext[4];      // mode contexts for LAST,GOLD,ALT,COMP
    };

    struct MvRefsNew {
        AV1MVPair mvs[5][3];   // 5 refs: L G A V0 V1; 3 pred modes (NEAREST, NEAR, ZERO); second is 0 for single-ref
        AV1MVPair bestMv[5];   // predictor for NEWMV (refIdx[j]=LAST,GOLD,ALT,COMP0,COMP1)
        int32_t useMvHp[3];
        uint32_t refFrameBitCount[5]; // 5 refs: L G A V0 V1
        MemCtx memCtx;
        AV1MvRefs mvRefsAV1;
    };

    struct MvRefs : MvRefsNew {
        // old
        AV1MV mv[15][2];
        RefIdx refIdx[15][2];
        PredMode mode[15];
        int32_t numMvRefs;
        // /old
    };

    struct IntraLumaMode
    {
        uint8_t    mode;      // 0..34
        int32_t   satd;
        CostType cost;      // satd + bitCost or full RD cost
        int32_t   numBits;   // estimated number of bits * 512
        CostType bitCost;   // numBits * lambda
    };

    struct PaletteInfo
    {
        uint8_t *color_map;
        uint8_t *color_map_uv;
        alignas(8) uint8_t palette_y[8];
        alignas(8) uint8_t palette_u[8];
        alignas(8) uint8_t palette_v[8];
        uint32_t palette_bits;
        uint32_t sse;
        uint32_t sseUV;
        uint8_t palette_size_y;
        uint8_t palette_size_uv;
        uint8_t true_colors_y;
        uint8_t true_colors_uv;
    };

    struct ModeInfo
    {
    public:
        AV1MVPair mv;
        AV1MVPair mvd;
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

    extern const int SizeOfMemCtxMustBe4[sizeof(MemCtx) == 4 ? 1 : -1];
    extern const int SizeOfModeInfoMustBe32[sizeof(ModeInfo) == 32 ? 1 : -1];

    struct Contexts {
        const Contexts& operator=(const Contexts &other);
        alignas(16) uint8_t aboveNonzero[3][16];
        alignas(16) uint8_t leftNonzero[3][16];
        union {
            struct {
                alignas(16) uint8_t abovePartition[8];
                alignas(8)  uint8_t leftPartition[8];
            };
            alignas(16) uint8_t aboveAndLeftPartition[16];
        };
        alignas(16) uint8_t aboveTxfm[16];
        alignas(16) uint8_t leftTxfm[16];
    };

    struct SuperBlock {
        const AV1VideoParam *par;
        Frame *frame;
        int32_t sbIndex;
        int32_t sbRow;
        int32_t sbCol;
        int32_t pelX;
        int32_t pelY;
        int32_t tileRow;
        int32_t tileCol;
        int32_t tileIndex;
        ModeInfo *mi;
        const CoeffsType *coeffs[3];
        Contexts ctx;
        int32_t cdefPreset;
        int32_t code_delta;
    };
    void InitSuperBlock(SuperBlock *sb, int32_t sbRow, int32_t sbCol, const AV1VideoParam &par, Frame *frame, const CoeffsType *coefs);

    struct AV1MEInfo
    {
        AV1MVPair mv;  // [fwd/bwd]
        int8_t refIdx[2];
        int8_t refIdxComb;
        uint8_t depth;
        int32_t posx, posy;  // in pix inside LCU
        int32_t width, height;
        int32_t satd;
        uint32_t mvBits;
    };


#define CHROMA_SIZE_DIV 2  // 4 - 4:2:0 ; 2 - 4:2:2,4:2:0 ; 1 - 4:4:4,4:2:2,4:2:0

    struct RdCost {
        int32_t ssz;
        int32_t sse;
        uint32_t modeBits;
        uint32_t coefBits;
        int32_t eob;

        RdCost & operator +=(const RdCost &o) {
            ssz += o.ssz;
            sse += o.sse;
            modeBits += o.modeBits;
            coefBits += o.coefBits;
            eob += o.eob;
            return *this;
        }
    };

    struct VarTxCoefs {
        int16_t coef[32 * 32];
        int16_t qcoef[32 * 32];
        int16_t dqcoef[32 * 32];
#if ENABLE_HIBIT_COEFS
        int32_t coef_hbd[32 * 32];
        int32_t dqcoef_hbd[32 * 32];
#endif
    };

    template <typename PixType>
    class AV1CU
    {
    public:
        const AV1VideoParam *m_par;

        const uint8_t *m_lcuQps;
        const Frame  *m_currFrame;
        int32_t       m_ctbAddr;           ///< CU address in a slice
        int32_t       m_ctbPelX;           ///< CU position in a pixel (X)
        int32_t       m_ctbPelY;           ///< CU position in a pixel (Y)
        TileBorders   m_tileBorders;
        uint32_t      m_numPartition;     ///< total number of minimum partitions in a CU

        alignas(64) PixType m_predIntraAll[((AV1_INTRA_MODES + 1 + 1) & ~1) * 32 * 32 * 2]; // +1 for Palette
        alignas(64) uint16_t m_predIntraAll10bit[((AV1_INTRA_MODES + 1 + 1) & ~1) * 32 * 32 * 2]; // +1 for Palette

        // working/final coeffs
        CoeffsType    *m_coeffWorkY;
        CoeffsType    *m_coeffWorkU;
        CoeffsType    *m_coeffWorkV;
        // temporarily best coeffs for lower depths
        alignas(64) CoeffsType    m_coeffStoredY[4][MAX_CU_SIZE * MAX_CU_SIZE];
        alignas(64) CoeffsType    m_coeffStoredU[4][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
        alignas(64) CoeffsType    m_coeffStoredV[4][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
        // inter reconstruct pixels
        alignas(64) PixType       m_recStoredY[4][MAX_CU_SIZE * MAX_CU_SIZE];
        alignas(64) PixType       m_recStoredC[4][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV * 2];

        // Palette tokens
        alignas(64) uint8_t       m_paletteMapY[4][MAX_CU_SIZE * MAX_CU_SIZE];
        alignas(64) PaletteInfo   m_paletteStoredY[4][(MAX_CU_SIZE / MI_SIZE) * (MAX_CU_SIZE / MI_SIZE)];
        alignas(64) uint8_t       m_paletteMapUV[4][MAX_CU_SIZE * MAX_CU_SIZE];
        alignas(64) PaletteInfo   m_paletteStoredUV[4][(MAX_CU_SIZE / MI_SIZE) * (MAX_CU_SIZE / MI_SIZE)];

        Contexts m_contextsInitSb;  // contexts at SB start
        Contexts m_contexts;
        Contexts m_contextsStored[5];

        CostType m_costStored[4];  // stored RD costs
        CostType m_costCurr;       // current RD cost

        union {
            struct {
                ModeInfo *curr;
                ModeInfo *left;
                ModeInfo *above;
                ModeInfo *aboveLeft;
                ModeInfo *colocated;
            };
            ModeInfo *neighbours[5];
        } m_availForPred;

        MvRefs m_mvRefs;

        alignas(64) PixType m_ySrc[64*64];
        alignas(64) PixType m_uvSrc[64*32];
        alignas(64) uint16_t m_ySrc10[64 * 64];
        alignas(64) uint16_t m_uvSrc10[64 * 32];
        PixType *m_yRec;
        PixType *m_uvRec;
        int32_t m_pitchRecLuma;
        int32_t m_pitchRecChroma;
        uint16_t *m_yRec10;
        uint16_t *m_uvRec10;
        int32_t m_pitchRecLuma10;
        int32_t m_pitchRecChroma10;

        ModeInfo *m_data;         // 1 layer,
        ModeInfo *m_dataStored;   // depth array, current best for CU

        CostType m_rdLambda;
        CostType m_rdLambdaChroma;
        CostType m_rdLambdaSqrt;
        uint64_t m_rdLambdaSqrtInt;
        uint32_t m_rdLambdaSqrtInt32;
        CostType m_ChromaDistWeight;
        int32_t m_lumaQp;
        int32_t m_chromaQp;

        QuantParam m_aqparamY;
        QuantParam m_aqparamUv[2];
        float m_roundFAdjY[2];
        float m_roundFAdjUv[2][2];

        uint8_t m_intraMinDepth;      // min CU depth from spatial analysis
        uint8_t m_adaptMinDepth;      // min CU depth from collocated CUs
        uint8_t m_projMinDepth;      // min CU depth from root CU proj
        uint8_t m_adaptMaxDepth;      // max CU depth from Projected CUs
        int16_t HorMax;              // MV common limits in CU
        int16_t HorMin;
        int16_t VerMax;
        int16_t VerMin;

        int32_t  m_SCid[5][MAX_NUM_PARTITIONS];
        float  m_SCpp[5][MAX_NUM_PARTITIONS];
        int32_t  m_STC[5][MAX_NUM_PARTITIONS];
        int32_t  m_mvdMax;
        int32_t  m_mvdCost;
        bool    m_bIntraCandInBuf;
        int32_t  m_IntraCandMaxSatd;

        union {
            struct {
                struct {
                    alignas(64) int16_t tmpBuf[(64+16)*(64+8)];
                } interpWithAvg;
                uint8_t antialiasing0;

                struct {
                    alignas(64) PixType predBuf[64*64];
                    uint8_t antialiasing1;
                } matchingMetric;

            } interp;

            uint8_t antialiasing2;
        } m_scratchPad;
#if 0
        alignas(64) int16_t  m_predBufHi3[MAX_NUM_REF_IDX][4][4][(MAX_CU_SIZE+32) * (MAX_CU_SIZE+MEMOIZE_SUBPEL_EXT_H)];
        alignas(64) PixType m_predBuf3  [MAX_NUM_REF_IDX][4][4][(MAX_CU_SIZE+32) * (MAX_CU_SIZE+MEMOIZE_SUBPEL_EXT_H)];
        alignas(64) int16_t  m_predBufHi2[MAX_NUM_REF_IDX][4][4][((MAX_CU_SIZE>>1)+32) * ((MAX_CU_SIZE>>1)+MEMOIZE_SUBPEL_EXT_H)];
        alignas(64) PixType m_predBuf2  [MAX_NUM_REF_IDX][4][4][((MAX_CU_SIZE>>1)+32) * ((MAX_CU_SIZE>>1)+MEMOIZE_SUBPEL_EXT_H)];
        alignas(64) int16_t  m_predBufHi1[MAX_NUM_REF_IDX][4][4][((MAX_CU_SIZE>>2)+16) * ((MAX_CU_SIZE>>2)+MEMOIZE_SUBPEL_EXT_H)];
        alignas(64) PixType m_predBuf1  [MAX_NUM_REF_IDX][4][4][((MAX_CU_SIZE>>2)+16) * ((MAX_CU_SIZE>>2)+MEMOIZE_SUBPEL_EXT_H)];
        alignas(64) int16_t  m_predBufHi0[MAX_NUM_REF_IDX][4][4][((MAX_CU_SIZE>>3)+8) * ((MAX_CU_SIZE>>3)+MEMOIZE_SUBPEL_EXT_H)];
        alignas(64) PixType m_predBuf0  [MAX_NUM_REF_IDX][4][4][((MAX_CU_SIZE>>3)+8) * ((MAX_CU_SIZE>>3)+MEMOIZE_SUBPEL_EXT_H)];
        int32_t m_satd8Buf1[MAX_NUM_REF_IDX][4][4][4][((MAX_CU_SIZE>>3)>>2) * ((MAX_CU_SIZE>>3)>>2)];
        int32_t m_satd8Buf2[MAX_NUM_REF_IDX][4][4][4][((MAX_CU_SIZE>>3)>>1) * ((MAX_CU_SIZE>>3)>>1)];
        int32_t m_satd8Buf3[MAX_NUM_REF_IDX][4][4][4][((MAX_CU_SIZE>>3))    * ((MAX_CU_SIZE>>3))];
#endif

#if 0
        MemPred<PixType>        m_memSubpel[4][MAX_NUM_REF_IDX][4][4];  // [size][Refs][hPh][vPh]
        MemHad                  m_memSubHad[4][MAX_NUM_REF_IDX][4][4];  // [size][Refs][hPh][vPh]
        MemBest                 m_memBestMV[4][MAX_NUM_REF_IDX];
#endif
#ifdef MEMOIZE_NUMCAND
        MemCand<PixType>        m_memCandSubHad[4];  // [sizes]

        int32_t m_satd8CandBuf0[MEMOIZE_NUMCAND][((MAX_CU_SIZE>>3)>>3) * ((MAX_CU_SIZE>>3)>>3)];
        int32_t m_satd8CandBuf1[MEMOIZE_NUMCAND][((MAX_CU_SIZE>>3)>>2) * ((MAX_CU_SIZE>>3)>>2)];
        int32_t m_satd8CandBuf2[MEMOIZE_NUMCAND][((MAX_CU_SIZE>>3)>>1) * ((MAX_CU_SIZE>>3)>>1)];
        int32_t m_satd8CandBuf3[MEMOIZE_NUMCAND][((MAX_CU_SIZE>>3))    * ((MAX_CU_SIZE>>3))];
        alignas(64) PixType m_predBufCand3[MEMOIZE_NUMCAND][(MAX_CU_SIZE)    * (MAX_CU_SIZE)];
        alignas(32) PixType m_predBufCand2[MEMOIZE_NUMCAND][(MAX_CU_SIZE>>1) * (MAX_CU_SIZE>>1)];
        alignas(32) PixType m_predBufCand1[MEMOIZE_NUMCAND][(MAX_CU_SIZE>>2) * (MAX_CU_SIZE>>2)];
        alignas(32) PixType m_predBufCand0[MEMOIZE_NUMCAND][(MAX_CU_SIZE>>3) * (MAX_CU_SIZE>>3)];
#endif

        struct {
            // 5 refs: L,G,A,V0+F,V1+F; 4 depths; 6 inter modes (NEAREST, NEAR0, ZERO, NEW, NEAR1, NEAR2); 4 new comp modes (NEW_NEAREST, NEAREST_NEW, NEW_NEAR0, NEAR0_NEW)
#if ENABLE_SW_ME
            alignas(64) PixType interpBufs[5][4][6+4+4]    [1];//[64 * 64];
#endif
            alignas(64) PixType predY[2][64*64];
            alignas(64) PixType predUv[2][64*32];
            alignas(64) uint16_t predY10[2][64 * 64];
            alignas(64) uint16_t predUv10[2][64 * 32];

            alignas(64) PixType recY[2][64*64];
            alignas(64) PixType recUv[2][64*32];
            alignas(64) uint16_t recY10[2][64 * 64];
            alignas(64) uint16_t recUv10[2][64 * 32];

            alignas(64) int16_t diffY[64*64];
            alignas(64) int16_t diffU[32*32];
            alignas(64) int16_t diffV[32*32];

            alignas(64) int16_t coefY[64*64];
            alignas(64) int16_t coefU[32*32];
            alignas(64) int16_t coefV[32*32];
#if ENABLE_HIBIT_COEFS
            alignas(64) int32_t coefY_hbd[64 * 64];
#endif
            alignas(64) int16_t qcoefY[2][64*64];
            alignas(64) int16_t qcoefU[2][32*32];
            alignas(64) int16_t qcoefV[2][32*32];

            alignas(64) int16_t dqcoefY[64*64];
            alignas(64) int16_t dqcoefU[32*32];
            alignas(64) int16_t dqcoefV[32*32];
#if ENABLE_HIBIT_COEFS
            alignas(64) int32_t dqcoefY_hbd[64 * 64];
#endif

            alignas(64) int16_t compPredIm[64*64];


            // storage for txk search feature local optimization
            alignas(64) VarTxCoefs varTxCoefs[MAX_VARTX_DEPTH + 1][ADST_ADST + 2];// one more [[ADST_ADST + 1] for IDTX

        } vp9scratchpad;

        alignas(64) PixType *m_interp[5][4][6+4+4]; // 5 refs: L,G,A,V0+F,V1+F; 4 depths; 6 MVs, points to actual memory buffers in interpBufs + 4 new comp modes

        alignas(64) AV1MVPair m_nonZeroMvp[5][4][6+4+4];  // 5 refs: L,G,A,V0+F,V1+F; 4 depths; 6 MVs (NEAREST, NEAR0, ZERO, NEW, NEAR1, NEAR2)
        alignas(32) int32_t m_nonZeroMvpSatd[5][4][6+4+4]     [1];// [8 * 8];  // 5 refs; 4 depths; 2 MVs (NEAREST, NEAR0, ZERO, NEW, NEAR1, NEAR2); 64 8x8 blocks
#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        alignas(16) int32_t m_zeroMvpSatd8[5][8 * 8];  // 5 refs; 64 8x8 blocks
        alignas(16) int32_t m_zeroMvpSatd16[5][4 * 4];  // 5 refs; 16 16x16 blocks, accumulated SATDs (summed up satd for 8x8 blocks)
        alignas(16) int32_t m_zeroMvpSatd32[5][2 * 2];  // 5 refs; 4 32x32 blocks
        alignas(16) int32_t m_zeroMvpSatd64[5][1 * 1];  // 5 refs; 1 64x64 block
#endif
        int32_t *m_zeroMvpSatd[5][4]; // 5 refs; 4 depth, pointers to m_zeroMvSatd64 - m_zeroMvSatd8

        alignas(64) PixType *m_bestInterp[4][8][8];  // [4 depths][miRow][miCol] used by RefineDecision()
        alignas(64) int32_t m_bestInterSatd[4][8][8]; // [4 depths][miRow][miCol] used by RefineDecision()

#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        PixType m_newMvInterp8[4096];
        PixType m_newMvInterp16[4096];
        PixType m_newMvInterp32[4096];
        PixType m_newMvInterp64[4096];
#else // PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        PixType *m_newMvInterp8;
        PixType *m_newMvInterp16;
        PixType *m_newMvInterp32;
        PixType *m_newMvInterp64;
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

        AV1MV GetMvRefsAV1TU7I(int32_t sbType, int32_t miRow, int32_t miCol);
        void GetMvRefsAV1TU7P(int32_t sbType, int32_t miRow, int32_t miCol, MvRefs *mvRefs, int32_t pass = 99);
        void GetMvRefsAV1TU7B(int32_t sbType, int32_t miRow, int32_t miCol, MvRefs *mvRefs, int32_t negate, int32_t pass = 99);

        alignas(64) AV1MVPair m_newMvFromGpu8[8 * 8];
        alignas(64) AV1MVPair m_newMvFromGpu16[4 * 4];
        alignas(64) AV1MVPair m_newMvFromGpu32[2 * 2];
        alignas(64) AV1MVPair m_newMvFromGpu64[1 * 1];

        alignas(64) int32_t m_newMvSatd8[8 * 8];
        alignas(64) int32_t m_newMvSatd16[4 * 4];
        alignas(64) int32_t m_newMvSatd32[2 * 2];
        alignas(64) int32_t m_newMvSatd64[1 * 1];

        alignas(64) RefIdx m_newMvRefIdx8[8 * 8][2];
        alignas(64) RefIdx m_newMvRefIdx16[4 * 4][2];
        alignas(64) RefIdx m_newMvRefIdx32[2 * 2][2];
        alignas(64) RefIdx m_newMvRefIdx64[1 * 1][2];
        void CalculateNewMvPredAndSatd();
        int32_t GetCmDistMv(int32_t sbx, int32_t sby, int32_t log2w, int32_t& mvd);
        bool IsGoodPred(float SCpp, float SADpp, float mvdAvg) const;
        bool IsBadPred(float SCpp, float SADpp_best, float mvdAvg) const;

        //void GetMvRefsFasterP(BlockSize sbType, int32_t leftx, int32_t topy, int32_t rightx, int32_t bottomy, MvRefs *mvRefs);
        //void GetMvRefsFasterB(BlockSize sbType, int32_t leftx, int32_t topy, int32_t rightx, int32_t bottomy, MvRefs *mvRefs);
        //void GetMvRefsOld(BlockSize sbType, int32_t miRow, int32_t miCol, MvRefs *mvRefs);

        inline int ClipMV_NR_shift(const AV1MV& rcMv) const
        {
            return std::max(HorMin, std::min(HorMax, int16_t(rcMv.mvx << 1))) | (std::max(VerMin, std::min(VerMax, int16_t(rcMv.mvy << 1)))<<1);
        }

        bool CheckFrameThreadingSearchRange(const AV1MEInfo *meInfo, const AV1MV *mv) const;


        void RefineDecisionAV1();
        void MakeRecon10bitAV1();  // 10bit for INTER frames
        void RefineDecisionIAV1(); // for INTRA frames
        void MakeRecon10bitIAV1(); // 10bit for INTRA frames

        bool JoinRefined32x32MI(ModeInfo mi64, bool check);
        void JoinMI();
        bool Split64();

        int32_t CheckIntra(int32_t absPartIdx, int32_t depth, PartitionType partition, uint32_t &ret_bits);
        uint64_t CheckIntra64x64(PredMode *bestIntraMode);
        RdCost CheckIntraLumaNonRdAv1(int32_t absPartIdx, int32_t depth, PartitionType partition);

        RdCost CheckIntraChromaNonRdAv1(int32_t absPartIdx, int32_t depth, PartitionType partition, const PixType* bestLuma, int lumaStride);
        RdCost CheckIntraChromaNonRdAv1_10bit(int32_t absPartIdx, int32_t depth, PartitionType partition, const uint16_t* bestLuma, int lumaStride);


        int32_t CheckIntraBlockCopy(int32_t absPartIdx, int16_t *qcoef, RdCost& rd, uint16_t* varTxInfo, bool search, CostType bestCost);
        int32_t TransformInterChroma(int32_t absPartIdx, uint16_t* varTxInfo);

        uint32_t GetPaletteCache(uint32_t miRow, bool isLuma, uint32_t miCol, uint8_t *cache);

        void InitCu(const AV1VideoParam &_par, ModeInfo *_data, ModeInfo *_dataTemp, int32_t ctbRow,
                    int32_t ctbCol, const Frame &frame, CoeffsType *m_coeffWork, uint8_t* lcuRound);

        void GetSpatialComplexity(int32_t absPartIdx, int32_t depth, int32_t partAddr, int32_t partDepth);
        int32_t GetSpatialComplexity(int32_t absPartIdx, int32_t depth, int32_t partAddr, int32_t partDepth, float *SCpp) const;
        int32_t GetSpatialComplexity(int32_t posx, int32_t posy, int32_t log2w, float *SCpp) const;

        int32_t GetSpatioTemporalComplexity(int32_t absPartIdx, int32_t depth, int32_t partAddr, int32_t partDepth);
        int32_t GetSpatioTemporalComplexity(int32_t absPartIdx, int32_t depth, int32_t partAddr, int32_t partDepth, int32_t *scVal);
        int32_t GetSpatioTemporalComplexityColocated(int32_t absPartIdx, int32_t depth, int32_t partAddr, int32_t partDepth, FrameData *ref);
        bool TuMaxSplitInterHasNonZeroCoeff(uint32_t absPartIdx, uint8_t trIdxMax);
        bool tryIntraICRA(int32_t absPartIdx, int32_t depth);
        ModeInfo *GetCuDataXY(int32_t x, int32_t y, Frame *ref);
        void GetProjectedDepth(int32_t absPartIdx, int32_t depth, uint8_t allowSplit);
        int32_t GetNumIntraRDModes(int32_t absPartIdx, int32_t depth, int32_t trDepth, IntraLumaMode *modes, int32_t numModes);
        void ModeDecision(int32_t absPartIdx, int32_t depth);

        bool IsTuSplitIntra() const;

        ModeInfo *StoredCuData(int32_t depth) const;

        void SaveFinalCuDecision(int32_t absPartIdx, int32_t depth, PartitionType partition, int32_t storageIndex, int32_t chroma);
        void LoadFinalCuDecision(int32_t absPartIdx, int32_t depth, PartitionType partition, int32_t storageIndex, int32_t chroma);
        const ModeInfo *GetBestCuDecision(int32_t absPartIdx, int32_t depth) const;

        uint8_t GetAdaptiveIntraMinDepth(int32_t absPartIdx, int32_t depth, int32_t *maxSC);

        void GetAdaptiveMinDepth(int32_t absPartIdx, int32_t depth);

        bool JoinCU(int32_t absPartIdx, int32_t depth);

#if PROTOTYPE_GPU_MODE_DECISION_SW_PATH
        template <int32_t depth> void MePuGacc_SwPath(AV1MEInfo *meInfo);
        void InitCu_SwPath(const AV1VideoParam &_par, ModeInfo *_data, ModeInfo *_dataTemp, int32_t ctbRow,
            int32_t ctbCol, const Frame &frame, CoeffsType *m_coeffWork);
        void CalculateZeroMvPredAndSatd();
#endif // PROTOTYPE_GPU_MODE_DECISION_SW_PATH

        AV1MV m_cachedBestMv8x8;
        int32_t m_cachedBestSad8x8Raw[2][2];
        int32_t m_cachedBestSad8x8Rec[2][2];
        int32_t m_isSCC[4];
#if ENABLE_SW_ME
        void   MemoizeInit();
        void   MemoizeClear(int32_t depth);

        int32_t tuHadSave(const PixType* src, int32_t pitchSrc, const PixType* rec, int32_t pitchRec, int32_t width, int32_t height, int32_t *satd, int32_t memPitch);
        int32_t tuHadUse(int32_t width, int32_t height, int32_t *satd8, int32_t memPitch) const;
        int32_t tuHad(const PixType* src, int32_t pitchSrc, const PixType* rec, int32_t pitchRec, int32_t width, int32_t height);

        bool MemHadFirst(int32_t size, const AV1MEInfo *meInfo, int32_t refIdxMem);
        bool MemHadUse(int32_t size, int32_t hPh, int32_t vPh, const AV1MEInfo* meInfo, int32_t refIdxMem, const AV1MV *mv, int32_t& satd, int32_t& foundSize);
        bool MemHadSave(int32_t size, int32_t hPh, int32_t vPh, const AV1MEInfo* meInfo, int32_t refIdxMem, const AV1MV *mv, int32_t*& satd8, int32_t& memPitch);
        void MemHadGetBuf(int32_t size, int32_t hPh, int32_t vPh, int32_t refIdxMem, const AV1MV *mv, int32_t **satd8);
        bool MemBestMV(int32_t size, int32_t refIdxMem, AV1MV *mv);
        void SetMemBestMV(int32_t size, const AV1MEInfo* meInfo, int32_t refIdxMem, AV1MV mv);

        bool MemSubpelUse(int32_t size, int32_t hPh, int32_t vPh, const AV1MEInfo* meInfo, int32_t refIdxMem, const AV1MV *mv, const PixType*& predBuf, int32_t& memPitch);
        bool MemSubpelSave(int32_t size, int32_t hPh, int32_t vPh, const AV1MEInfo* meInfo, int32_t refIdxMem, const AV1MV *mv, PixType*& predBuf, int32_t& memPitch);
        bool MemSubpelInRange(int32_t size, const AV1MEInfo *meInfo, int32_t refIdxMem, const AV1MV *mv);
        void MemSubpelGetBufSetMv(int32_t size, AV1MV *mv, int32_t refIdxMem, PixType **predBuf, int16_t **predBufHi);
        void MemSubpelGetBufSetMv(int32_t size, AV1MV *mv, int32_t refIdxMem, PixType **predBuf);

        void MeCu(int32_t absPartIdx, int32_t depth, PartitionType partition);
        template <int32_t depth> void MeCuNonRdAV1(int32_t miRow, int32_t miCol);
        template <int32_t codecType, int32_t depth> void ModeDecisionInterTu7(int32_t miRow, int32_t miCol);
        void ModeDecisionInterTu7_SecondPass(int32_t miRow, int32_t miCol);
        void MePu(AV1MEInfo *meInfo);
        void MePuGacc(AV1MEInfo *meInfo);
        template <int32_t depth> void MePuGacc(AV1MEInfo *meInfo);

        void MeIntPel(const AV1MEInfo *meInfo, const AV1MV &mvp, const FrameData *ref,
            AV1MV *mv, int32_t *cost, int32_t *mvCost, int32_t meStepRange = INT_MAX);
        int16_t MeIntSeed(const AV1MEInfo *meInfo, const AV1MV &mvp, int32_t list,
            int32_t refIdx, AV1MV mvRefBest0, AV1MV mvRefBest00,
            AV1MV *mvBest, int32_t *costBest, int32_t *mvCostBest, AV1MV *mvBestSub);

        void MeIntPelFullSearch(const AV1MEInfo *meInfo, const AV1MV &mvp, const FrameData *ref, AV1MV *mv, int32_t *cost, int32_t *mvCost, MvLimits& limits);

        void MeIntPelLog(const AV1MEInfo *meInfo, const AV1MV &mvp, const FrameData *ref,
            AV1MV *mv, int32_t *cost, int32_t *mvCost, int32_t meStepRange);

        void MeSubPel(const AV1MEInfo *meInfo, const AV1MV &mvp, const FrameData *ref,
            AV1MV *mv, int32_t *cost, int32_t *mvCost, int32_t refIdxMem);

        bool MeSubPelSeed(const AV1MEInfo *meInfo, const AV1MV &mvp,
            const FrameData *ref, AV1MV *mvBest, int32_t *costBest, int32_t *mvCostBest,
            int32_t refIdxMem, const AV1MV &mvInt, const AV1MV &mvBestSub);

        void AddMvCost(const AV1MV &mvp, int32_t log2Step, const int32_t *dists, AV1MV *mv,
            int32_t *costBest, int32_t *mvCostBest, int32_t patternSubpel) const;

#ifdef USE_BATCHED_FASTORTH_SEARCH
        void MemSubPelBatchedFastBoxDiaOrth(const AV1MEInfo *meInfo, const AV1MV &mvp,
            const FrameData *ref, AV1MV *mv, int32_t *cost, int32_t *mvCost, int32_t refIdxMem, int32_t size);
#endif

        void MeInterpolate(const AV1MEInfo *meInfo, const AV1MV *MV, PixType *in_pSrc,
            int32_t in_SrcPitch, PixType *buf, int32_t buf_pitch) const;

        void MeInterpolateSave(const AV1MEInfo* me_info, const AV1MV *MV, PixType *src,
            int32_t srcPitch, PixType *dst, int32_t dstPitch) const;

        int32_t MatchingMetricPu(const PixType *src, const AV1MEInfo *meInfo, const AV1MV* mv,
            const FrameData *refPic, int32_t useHadamard);

        int32_t MatchingMetricPuMem(const PixType *src, const AV1MEInfo *meInfo, const AV1MV *mv,
            const FrameData *refPic, int32_t useHadamard, int32_t refIdxMem, int32_t size,
            int32_t& hadFoundSize);
#endif
    };

    int32_t GetLumaOffset(const AV1VideoParam *par, int32_t absPartIdx, int32_t pitch);

    int32_t GetChromaOffset(const AV1VideoParam *par, int32_t absPartIdx, int32_t pitch);

    void PropagateSubPart(ModeInfo *mi, int32_t miPitch, int32_t miWidth, int32_t miHeight);

    void PropagatePaletteInfo(PaletteInfo *pi, int32_t miRow, int32_t miCol, int32_t miPitch, int32_t miWidth, int32_t miHeight);

    struct BitCounts;

    template <typename PixType>
    uint8_t GetColorCount(PixType* srcSb, uint32_t width, uint32_t height, uint8_t *palette, uint32_t bitdepth, CostType rdLambda, PaletteInfo *pi, uint8_t *color_map, uint32_t &bits, bool optimize=true);
    template <typename PixType>
    uint8_t GetColorCountUV(PixType srcSb, uint32_t width, uint32_t height, uint8_t *ret_palette, uint32_t bitdepth, CostType rdLambda, PaletteInfo *pi, uint8_t *color_map, uint32_t &bits);

    uint32_t GetPaletteCacheIndicesCost(uint8_t *palette, uint32_t palette_size, uint8_t *cache, uint32_t cache_size, uint8_t *colors_xmit, uint32_t &index_bits);
    uint32_t GetPaletteDeltaCost(uint8_t *colors_xmit, uint32_t colors_xmit_size, uint32_t bitdepth, uint32_t min_delta);
    template <typename PixType>
    uint32_t GetPaletteTokensCost(const BitCounts &bc, PixType *src, int32_t width, int32_t height, uint8_t *palette, uint32_t palette_size, uint8_t *color_map);

    BlockSize GetPlaneBlockSize(BlockSize subsize, int32_t plane, const AV1VideoParam &par);
    TxSize GetUvTxSize(int32_t miSize, TxSize txSize, const AV1VideoParam &par);
    int32_t UseMvHp(const AV1MV &deltaMv);
    int32_t FindBestRefMvs(AV1MV refListMv[2], const AV1VideoParam &par);

    int32_t GetDcCtx(const uint8_t *aboveNonzeroCtx, const uint8_t *leftNonzeroCtx, int32_t step);
    int32_t GetDcCtx(const uint8_t *aboveNonzeroCtx, const uint8_t *leftNonzeroCtx, int32_t stepX, int32_t stepY);
    template <int32_t step> int32_t GetDcCtx(const uint8_t *aboveNonzeroCtx, const uint8_t *leftNonzeroCtx);

    int32_t GetTxbSkipCtx(BlockSize planeBsize, TxSize txSize, int32_t plane, const uint8_t *aboveCtx, const uint8_t *leftCtx);

    template <typename PixType, typename TCoeffType>
    RdCost TransformIntraYSbAv1(BlockSize bsz, int32_t mode, int32_t haveTop, int32_t haveLeft, TxSize txSize, TxType txType,
                                uint8_t *aboveNzCtx, uint8_t *leftNzCtx, const PixType* src_, PixType *rec_, int32_t pitchRec,
                                int16_t *diff, TCoeffType *coeff_, int16_t *qcoeff_, TCoeffType *coefWork, const int32_t qp, const BitCounts &bc,
                                CostType lambda, int32_t miRow, int32_t miCol, int32_t miColEnd, int32_t deltaAngle, int32_t filtType,
                                const AV1VideoParam &par, const QuantParam &qpar, float *roundFAdj, PaletteInfo *pi, int16_t *retEob);

    template<typename PixType>
    RdCost TransformInterYSbVarTxByRdo(
        BlockSize bsz, uint8_t *aboveNzCtx, uint8_t *leftNzCtx, const PixType* src, PixType *rec,
        const int16_t *diff, int16_t *qcoef, int32_t qp, const BitCounts &bc, CostType lambda,
        uint8_t *aboveTxfmCtx, uint8_t *leftTxfmCtx, uint16_t *vartxInfo, const QuantParam &qpar,
        float *roundFAdj, VarTxCoefs varTxCoefs[MAX_VARTX_DEPTH + 1][ADST_ADST + 2], int32_t max_vartx_depth, uint32_t max_tx_ids);

    template <typename PixType>
    int32_t TransformInterUvSbTxType(BlockSize bsz, TxSize txSize, TxType txType_, uint8_t *aboveU, uint8_t *aboveV,
        uint8_t *leftU, uint8_t *leftV, const PixType* src_, PixType *rec_,
        int32_t pitchRec, const int16_t *diffU_, const int16_t *diffV_, int16_t *coeffU_,
        int16_t *coeffV_, int16_t *qcoeffU_, int16_t *qcoeffV_, int16_t *coefWork, const AV1VideoParam &par,
        const BitCounts &bc, int32_t qp, const uint16_t* txk_types, const QuantParam(&qpar)[2], float **roundFAdj, CostType lambda, int32_t trySkip);

    template<typename PixType>
    void InterpolateAv1SingleRefLuma(const PixType *refColoc, int32_t refPitch, PixType *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t interp);
    template<typename PixType>
    void InterpolateAv1SingleRefChroma(const PixType *refColoc, int32_t refPitch, PixType *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t interp);

    template<typename PixType>
    void InterpolateAv1FirstRefLuma(const PixType *refColoc, int32_t refPitch, int16_t *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t interp);
    template<typename PixType>
    void InterpolateAv1FirstRefChroma(const PixType *refColoc, int32_t refPitch, int16_t *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t interp);

    template<typename PixType>
    void InterpolateAv1SecondRefLuma(const PixType *refColoc, int32_t refPitch, int16_t *ref0, PixType *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t interp);
    template<typename PixType>
    void InterpolateAv1SecondRefChroma(const PixType *refColoc, int32_t refPitch, int16_t *ref0, PixType *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t interp);

    inline uint64_t RD(int32_t d, uint32_t r, uint64_t lambda) {
        return ((uint64_t)d << 24) + lambda * r;
    }

    inline uint32_t RD32(int32_t d, uint32_t r, uint32_t lambda) {
        assert(d < 0x100000);
        assert(uint64_t(d << 11) + uint64_t(lambda * r) < 0x100000000);
        return (d << 11) + lambda * r;
    }

    int32_t HaveTopRight  (int32_t log2BlockWidth, int32_t miRow, int32_t miCol, int32_t txSize, int32_t y, int32_t x);
    int32_t HaveBottomLeft(int32_t log2BlockWidth, int32_t miRow, int32_t miCol, int32_t txSize, int32_t y, int32_t x);

    AV1_FORCEINLINE int32_t av1_is_directional_mode(PredMode mode) {return mode >= V_PRED && mode <= D63_PRED;}
    int32_t get_filt_type(const ModeInfo *above, const ModeInfo *left, int32_t plane);

    int32_t write_uniform_cost(int n, int v);

    int32_t have_nearmv_in_inter_mode(PredMode mode);
    int32_t have_newmv_in_inter_mode(PredMode mode);

    inline int32_t get_q_ctx(int32_t q)
    {
        if (q <= 20) return 0;
        if (q <= 60) return 1;
        if (q <= 120) return 2;
        return 3;
    }

    int16_t GetModeContext(int32_t nearestMatchCount, int32_t refMatchCount, int32_t newMvCount, int32_t colBlkAvail);

    inline int32_t is_intrabc_block(const ModeInfo *mi) {
        return mi->mode == DC_PRED && mi->interp;
    }

    void CopyVarTxInfo(const uint16_t *src, uint16_t *dst, int32_t w);
    void CopyTxSizeInfo(const uint16_t *varTxInfo, ModeInfo *mi, int32_t miPitch, int32_t w);

    int32_t MvCost1RefLog(AV1MV mv, const AV1MV &mvp, CostType lambda);
    int32_t ClipMV_special(AV1MV *rcMv, MvLimits& limits);
    int32_t MvCost(AV1MV mv, AV1MV mvp, int32_t useMvHp);

    int32_t GetPaletteDeltaBitsV(const uint8_t* palette_v, int32_t palette_size_uv, int32_t bitDepth, int32_t *zeroCount, int32_t *minBits);
    uint32_t GetPaletteColorContext(const uint8_t *colorMap, int32_t pitch, int32_t row, int32_t col, int32_t &new_color_idx);
}  // namespace AV1Enc

#endif  // MFX_ENABLE_AV1_VIDEO_ENCODE
