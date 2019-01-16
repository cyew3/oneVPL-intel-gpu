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

#include <limits.h> /* for INT_MAX on Linux/Android */
#include "mfx_av1_defs.h"
#include "mfx_av1_frame.h"

namespace H265Enc {

    struct H265VideoParam;
    struct FrameData;
    class Frame;

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
        H265MVCand refMvStack[COMP_VAR0 + 1][MAX_REF_MV_STACK_SIZE];
    };

    struct MvRefsAv1Fast {
        H265MV     singleMvs[3][4];     // 4 mvs for LAST,GOLD,ALT
        H265MVPair compMvs[4];          // 4 mvs for LAST+ALT=COMP_VAR0
        int32_t     numMvs[4];           // total number of mv
        int32_t     numNearestMvs[4];    // number of mv from top, left and top-right
                                        // not counting top-left, bottom-left and colocated
        int32_t     modeContext[4];      // mode contexts for LAST,GOLD,ALT,COMP
    };

    struct MvRefsNew {
        H265MVPair mvs[5][3];   // 5 refs: L G A V0 V1; 3 pred modes (NEAREST, NEAR, ZERO); second is 0 for single-ref
        H265MVPair bestMv[5];   // predictor for NEWMV (refIdx[j]=LAST,GOLD,ALT,COMP0,COMP1)
        int32_t useMvHp[3];
        uint32_t refFrameBitCount[5]; // 5 refs: L G A V0 V1
        MemCtx memCtx;
        AV1MvRefs mvRefsAV1;
    };

    struct MvRefs : MvRefsNew {
        // old
        H265MV mv[15][2];
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

        union {
            MemCtx memCtx; // 4 bytes
            uint32_t sad;
        };
    };

    extern const int SizeOfMemCtxMustBe4[sizeof(MemCtx) == 4 ? 1 : -1];
    extern const int SizeOfModeInfoMustBe32[sizeof(ModeInfo) == 32 ? 1 : -1];

    struct Contexts {
        const Contexts& operator=(const Contexts &other);
        __ALIGN16 uint8_t aboveNonzero[3][16];
        __ALIGN16 uint8_t leftNonzero[3][16];
        union {
            struct {
                __ALIGN16 uint8_t abovePartition[8];
                __ALIGN8  uint8_t leftPartition[8];
            };
            __ALIGN16 uint8_t aboveAndLeftPartition[16];
        };
        __ALIGN16 uint8_t aboveTxfm[16];
        __ALIGN16 uint8_t leftTxfm[16];
    };

    struct SuperBlock {
        const H265VideoParam *par;
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
        union {
            void     *tokens;
            TokenVP9 *tokensVP9;
            TokenAV1 *tokensAV1;
        };
        Contexts ctx;
        int32_t cdefPreset;
    };
    void InitSuperBlock(SuperBlock *sb, int32_t sbRow, int32_t sbCol, const H265VideoParam &par, Frame *frame, const CoeffsType *coefs, void *tokens);

    struct H265MEInfo
    {
        H265MVPair mv;  // [fwd/bwd]
        int8_t refIdx[2];
        int8_t refIdxComb;
        uint8_t depth;
        int32_t posx, posy;  // in pix inside LCU
        int32_t width, height;
        int32_t satd;
        uint32_t mvBits;
    };


#define CHROMA_SIZE_DIV 2  // 4 - 4:2:0 ; 2 - 4:2:2,4:2:0 ; 1 - 4:4:4,4:2:2,4:2:0

    template <class PixType> struct GetHistogramType;
    template <> struct GetHistogramType<uint8_t> { typedef uint16_t type; };
    template <> struct GetHistogramType<uint16_t> { typedef uint32_t type; };

    struct RdCost {
        int32_t sse;
        uint32_t modeBits;
        uint32_t coefBits;
        int32_t eob;
    };

    template <typename PixType>
    class H265CU
    {
    public:
        typedef typename GetHistogramType<PixType>::type HistType;

        const H265VideoParam *m_par;

        uint8_t  m_sliceQpY;
        const int8_t *m_lcuQps;

        const Frame    *m_currFrame;
        int32_t          m_ctbAddr;           ///< CU address in a slice
        int32_t          m_ctbPelX;           ///< CU position in a pixel (X)
        int32_t          m_ctbPelY;           ///< CU position in a pixel (Y)
        TileBorders     m_tileBorders;
        uint32_t          m_numPartition;     ///< total number of minimum partitions in a CU
        __ALIGN64 CoeffsType    m_residualsY[MAX_CU_SIZE * MAX_CU_SIZE];
        __ALIGN64 CoeffsType    m_residualsU[MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
        __ALIGN64 CoeffsType    m_residualsV[MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
        __ALIGN64 PixType       m_predIntraAll[((AV1_INTRA_MODES + 1) & ~1) * 32 * 32 * 2];
        __ALIGN64 PixType       m_srcTr[32*32];  // transposed src block

        // working/final coeffs
        CoeffsType    *m_coeffWorkY;
        CoeffsType    *m_coeffWorkU;
        CoeffsType    *m_coeffWorkV;
        // temporarily best coeffs for lower depths
        __ALIGN64 CoeffsType    m_coeffStoredY[5+1][MAX_CU_SIZE * MAX_CU_SIZE];  // (+1 for Intra_NxN)
        __ALIGN64 CoeffsType    m_coeffStoredU[5+1][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
        __ALIGN64 CoeffsType    m_coeffStoredV[5+1][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
        // inter reconstruct pixels
        __ALIGN64 PixType       m_recStoredY[5+1][MAX_CU_SIZE * MAX_CU_SIZE];  // (+1 for Intra_NxN)
        __ALIGN64 PixType       m_recStoredC[5+1][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV * 2];

        // inter prediction pixels
        __ALIGN64 PixType       m_interPredBufsY[5][2][MAX_CU_SIZE * MAX_CU_SIZE];
        __ALIGN64 PixType       m_interPredBufsC[5][2][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV * 2];
        // inter residual
        __ALIGN64 CoeffsType    m_interResidBufsY[5][2][MAX_CU_SIZE * MAX_CU_SIZE];
        __ALIGN64 CoeffsType    m_interResidBufsU[5][2][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
        __ALIGN64 CoeffsType    m_interResidBufsV[5][2][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];


        PixType    *m_interPredY;
        PixType    *m_interPredC;
        CoeffsType *m_interResidY;
        CoeffsType *m_interResidU;
        CoeffsType *m_interResidV;

        PixType    *m_interPredSavedY[5][MAX_NUM_PARTITIONS];
        PixType    *m_interPredSavedC[5][MAX_NUM_PARTITIONS];
        CoeffsType *m_interResidSavedY[5][MAX_NUM_PARTITIONS];
        CoeffsType *m_interResidSavedU[5][MAX_NUM_PARTITIONS];
        CoeffsType *m_interResidSavedV[5][MAX_NUM_PARTITIONS];

        PixType    *m_interPredBestY;
        PixType    *m_interPredBestC;
        CoeffsType *m_interResidBestY;
        CoeffsType *m_interResidBestU;
        CoeffsType *m_interResidBestV;

        Contexts m_contextsInitSb;  // contexts at SB start
        Contexts m_contexts;
        Contexts m_contextsStored[5];

        CostType m_costStored[5+1];  // stored RD costs (+1 for Intra_NxN)
        CostType m_costCurr;         // current RD cost

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

        __ALIGN64 PixType m_ySrc[64*64];
        __ALIGN64 PixType m_uvSrc[64*32];
        //PixType *m_yRec;
        //PixType *m_uvRec;
        int32_t m_pitchRecLuma;
        int32_t m_pitchRecChroma;

        static const int32_t NUM_DATA_STORED_LAYERS = 5 + 1;
        ModeInfo *m_data;         // 1 layer,
        ModeInfo *m_dataStored;   // depth array, current best for CU

        __ALIGN32 HistType m_hist4[40 * MAX_CU_SIZE / 4 * MAX_CU_SIZE / 4];
        __ALIGN32 HistType m_hist8[40 * MAX_CU_SIZE / 8 * MAX_CU_SIZE / 8];
        bool m_isHistBuilt;

        CostType m_rdLambda;
        CostType m_rdLambdaChroma;
        CostType m_rdLambdaSqrt;
        uint64_t m_rdLambdaSqrtInt;
        uint32_t m_rdLambdaSqrtInt32;
        CostType m_ChromaDistWeight;
        int32_t m_lumaQp;
        int32_t m_chromaQp;

        uint8_t m_intraMinDepth;      // min CU depth from spatial analysis
        uint8_t m_adaptMinDepth;      // min CU depth from collocated CUs
        uint8_t m_projMinDepth;      // min CU depth from root CU proj
        uint8_t m_adaptMaxDepth;      // max CU depth from Projected CUs
        int16_t HorMax;              // MV common limits in CU
        int16_t HorMin;
        int16_t VerMax;
        int16_t VerMin;

        bool m_split64;

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
                    __ALIGN64 int16_t tmpBuf[(64+16)*(64+8)];
                } interpWithAvg;
                uint8_t antialiasing0;

                struct {
                    __ALIGN64 int16_t tmpBuf[64*72];
                    __ALIGN64 int16_t interpBuf[64*64];  // buffer contains the result of 1-ref interpolation between 2 PredInterUni calls
                } predInterUni;

                struct {
                    __ALIGN64 PixType predBuf[64*64];
                    uint8_t antialiasing1;
                } matchingMetric;

            } interp;

            uint8_t antialiasing2;
        } m_scratchPad;

        struct {
            // 5 refs: L,G,A,V0+F,V1+F; 4 depths; 6 inter modes (NEAREST, NEAR0, ZERO, NEW, NEAR1, NEAR2); 4 new comp modes (NEW_NEAREST, NEAREST_NEW, NEW_NEAR0, NEAR0_NEW)
            __ALIGN64 PixType interpBufs[5][4][6+4+4][64*64];

            __ALIGN64 PixType predY[2][64*64];
            __ALIGN64 PixType predUv[2][64*32];

            __ALIGN64 PixType recY[2][64*64];
            __ALIGN64 PixType recUv[2][64*32];

            __ALIGN64 int16_t diffY[64*64];
            __ALIGN64 int16_t diffU[32*32];
            __ALIGN64 int16_t diffV[32*32];

            __ALIGN64 int16_t coefY[64*64];
            __ALIGN64 int16_t coefU[32*32];
            __ALIGN64 int16_t coefV[32*32];

            __ALIGN64 int16_t qcoefY[2][64*64];
            __ALIGN64 int16_t qcoefU[2][32*32];
            __ALIGN64 int16_t qcoefV[2][32*32];

            __ALIGN64 int16_t dqcoefY[64*64];
            __ALIGN64 int16_t dqcoefU[32*32];
            __ALIGN64 int16_t dqcoefV[32*32];

            __ALIGN64 int16_t compPredIm[64*64];

            // storage for txk search feature local optimization
            __ALIGN64 int16_t coefWork[9*32*32];

        } vp9scratchpad;

        __ALIGN64 PixType *m_interp[5][4][6+4+4]; // 5 refs: L,G,A,V0+F,V1+F; 4 depths; 6 MVs, points to actual memory buffers in interpBufs + 4 new comp modes

        __ALIGN16 H265MVPair m_nonZeroMvp[5][4][6+4+4];  // 5 refs: L,G,A,V0+F,V1+F; 4 depths; 6 MVs (NEAREST, NEAR0, ZERO, NEW, NEAR1, NEAR2)
        __ALIGN32 int32_t m_nonZeroMvpSatd[5][4][6+4+4][8 * 8];  // 5 refs; 4 depths; 2 MVs (NEAREST, NEAR0, ZERO, NEW, NEAR1, NEAR2); 64 8x8 blocks

        __ALIGN16 int32_t m_zeroMvpSatd8[5][8 * 8];  // 5 refs; 64 8x8 blocks
        __ALIGN16 int32_t m_zeroMvpSatd16[5][4 * 4];  // 5 refs; 16 16x16 blocks, accumulated SATDs (summed up satd for 8x8 blocks)
        __ALIGN16 int32_t m_zeroMvpSatd32[5][2 * 2];  // 5 refs; 4 32x32 blocks
        __ALIGN16 int32_t m_zeroMvpSatd64[5][1 * 1];  // 5 refs; 1 64x64 block
        int32_t *m_zeroMvpSatd[5][4]; // 5 refs; 4 depth, pointers to m_zeroMvSatd64 - m_zeroMvSatd8

        __ALIGN64 PixType *m_bestInterp[4][8][8];  // [4 depths][miRow][miCol] used by RefineDecision()
        __ALIGN64 int32_t m_bestInterSatd[4][8][8]; // [4 depths][miRow][miCol] used by RefineDecision()

        PixType *m_newMvInterp8;
        PixType *m_newMvInterp16;
        PixType *m_newMvInterp32;
        PixType *m_newMvInterp64;

        void GetMvRefsAV1TU7P(int32_t sbType, int32_t miRow, int32_t miCol, MvRefs *mvRefs, int32_t pass = 99);
        void GetMvRefsAV1TU7B(int32_t sbType, int32_t miRow, int32_t miCol, MvRefs *mvRefs, int32_t negate, int32_t pass = 99);

        __ALIGN64 H265MVPair m_newMvFromGpu8[8 * 8];
        __ALIGN64 H265MVPair m_newMvFromGpu16[4 * 4];
        __ALIGN64 H265MVPair m_newMvFromGpu32[2 * 2];
        __ALIGN64 H265MVPair m_newMvFromGpu64[1 * 1];

        __ALIGN64 int32_t m_newMvSatd8[8 * 8];
        __ALIGN64 int32_t m_newMvSatd16[4 * 4];
        __ALIGN64 int32_t m_newMvSatd32[2 * 2];
        __ALIGN64 int32_t m_newMvSatd64[1 * 1];

        __ALIGN64 RefIdx m_newMvRefIdx8[8 * 8][2];
        __ALIGN64 RefIdx m_newMvRefIdx16[4 * 4][2];
        __ALIGN64 RefIdx m_newMvRefIdx32[2 * 2][2];
        __ALIGN64 RefIdx m_newMvRefIdx64[1 * 1][2];
        void CalculateZeroMvPredAndSatd();
        void CalculateNewMvPredAndSatd();

        inline int32_t ClipMV(H265MV *rcMv) const  // returns 1 if changed, otherwise 0
        {
            int32_t change = 0;
            if (rcMv->mvx < HorMin) {
                change = 1;
                rcMv->mvx = (int16_t)HorMin;
            } else if (rcMv->mvx > HorMax) {
                change = 1;
                rcMv->mvx = (int16_t)HorMax;
            }
            if (rcMv->mvy < VerMin) {
                change = 1;
                rcMv->mvy = (int16_t)VerMin;
            } else if (rcMv->mvy > VerMax) {
                change = 1;
                rcMv->mvy = (int16_t)VerMax;
            }

            return change;
        }

        inline void ClipMV_NR(H265MV *rcMv) const
        {
            rcMv->mvx = Saturate(HorMin, HorMax, rcMv->mvx);
            rcMv->mvy = Saturate(VerMin, VerMax, rcMv->mvy);
        }

        template <int32_t depth> void MeCuNonRdAV1(int32_t miRow, int32_t miCol);

        template <int32_t depth> void MePuGacc(H265MEInfo *meInfo);

        void InitCu(const H265VideoParam &_par, ModeInfo *_data, ModeInfo *_dataTemp, int32_t ctbRow,
                    int32_t ctbCol, const Frame &frame, CoeffsType *m_coeffWork);

        int32_t  GetCmDistMv(int32_t sbx, int32_t sby, int32_t log2w, int32_t& mvd, H265MVPair predMv[5]);
        bool IsBadPred(float SCpp, float SADpp_best, float mvdAvg) const;
        bool IsGoodPred(float SCpp, float SADpp, float mvdAvg) const;
        void JoinMI();

        template <int32_t codecType, int32_t depth> void ModeDecisionInterTu7(int32_t miRow, int32_t miCol);
        void ModeDecisionInterTu7_SecondPass(int32_t miRow, int32_t miCol);

        ModeInfo *StoredCuData(int32_t depth) const;
    };

    int32_t GetLumaOffset(const H265VideoParam *par, int32_t absPartIdx, int32_t pitch);

    int32_t GetChromaOffset(const H265VideoParam *par, int32_t absPartIdx, int32_t pitch);

    void PropagateSubPart(ModeInfo *mi, int32_t miPitch, int32_t miWidth, int32_t miHeight);

    BlockSize GetPlaneBlockSize(BlockSize subsize, int32_t plane, const H265VideoParam &par);
    TxSize GetUvTxSize(int32_t miSize, TxSize txSize, const H265VideoParam &par);
    int32_t UseMvHp(const H265MV &deltaMv);
    int32_t FindBestRefMvs(H265MV refListMv[2], const H265VideoParam &par);

    int32_t GetDcCtx(const uint8_t *aboveNonzeroCtx, const uint8_t *leftNonzeroCtx, int32_t step);
    int32_t GetDcCtx(const uint8_t *aboveNonzeroCtx, const uint8_t *leftNonzeroCtx, int32_t stepX, int32_t stepY);
    template <int32_t step> int32_t GetDcCtx(const uint8_t *aboveNonzeroCtx, const uint8_t *leftNonzeroCtx);

    int32_t GetTxbSkipCtx(BlockSize planeBsize, TxSize txSize, int32_t plane, const uint8_t *aboveCtx, const uint8_t *leftCtx);

    struct BitCounts;
    template <typename PixType>
    RdCost TransformIntraYSbVp9(int32_t bsz, int32_t mode, int32_t haveAbove, int32_t haveLeft, int32_t notOnRight4x4,
                                TxSize txSize, uint8_t *aboveNzCtx, uint8_t *leftNzCtx, const PixType* src_, PixType *rec_,
                                int32_t pitchRec, int16_t *diff, int16_t *coeff_, int16_t *qcoeff_, const QuantParam &qpar,
                                const BitCounts &bc, int32_t fastCoeffCost, CostType lambda, int32_t miRow, int32_t miCol,
                                int32_t miColEnd, int32_t miRows, int32_t miCols);

    template <typename PixType>
    RdCost TransformIntraYSbAv1(int32_t bsz, int32_t mode, int32_t haveTop, int32_t haveLeft, TxSize txSize, TxType txType,
                                uint8_t *aboveNzCtx, uint8_t *leftNzCtx, const PixType* src_, PixType *rec_, int32_t pitchRec,
                                int16_t *diff, int16_t *coeff_, int16_t *qcoeff_, const int32_t qp, const BitCounts &bc,
                                CostType lambda, int32_t miRow, int32_t miCol, int32_t miColEnd, int32_t deltaAngle, int32_t filtType,
                                const H265VideoParam &par);

    template<typename PixType>
    RdCost TransformIntraYSbAv1_viaTxkSearch(int32_t bsz, int32_t mode, int32_t haveTop, int32_t haveLeft, TxSize txSize, TxType txType,
                                uint8_t *aboveNzCtx, uint8_t *leftNzCtx, const PixType* src_, PixType *rec_, int32_t pitchRec,
                                int16_t *diff, int16_t *coeff_, int16_t *qcoeff_, int32_t qp, const BitCounts &bc,
                                int32_t fastCoeffCost, CostType lambda, int32_t miRow, int32_t miCol, int32_t miColEnd,
                                int32_t miRows, int32_t miCols, int32_t deltaAngle, int32_t filtType, int32_t* txkTypes, const H265VideoParam &par, int16_t* coeffWork_);
    template<typename PixType>
    void InterpolateVp9Luma(const PixType *refColoc, int32_t refPitch, PixType *dst, H265MV mv, int32_t h, int32_t log2w, int32_t avg, int32_t interp);
    template<typename PixType>
    void InterpolateVp9Chroma(const PixType *refColoc, int32_t refPitch, PixType *dst, H265MV mv, int32_t h, int32_t log2w, int32_t avg, int32_t interp);

    template<typename PixType>
    void InterpolateAv1SingleRefLuma(const PixType *refColoc, int32_t refPitch, PixType *dst, H265MV mv, int32_t h, int32_t log2w, int32_t interp);
    template<typename PixType>
    void InterpolateAv1SingleRefChroma(const PixType *refColoc, int32_t refPitch, PixType *dst, H265MV mv, int32_t h, int32_t log2w, int32_t interp);

    template<typename PixType>
    void InterpolateAv1FirstRefLuma(const PixType *refColoc, int32_t refPitch, int16_t *dst, H265MV mv, int32_t h, int32_t log2w, int32_t interp);
    template<typename PixType>
    void InterpolateAv1FirstRefChroma(const PixType *refColoc, int32_t refPitch, int16_t *dst, H265MV mv, int32_t h, int32_t log2w, int32_t interp);

    template<typename PixType>
    void InterpolateAv1SecondRefLuma(const PixType *refColoc, int32_t refPitch, int16_t *ref0, PixType *dst, H265MV mv, int32_t h, int32_t log2w, int32_t interp);
    template<typename PixType>
    void InterpolateAv1SecondRefChroma(const PixType *refColoc, int32_t refPitch, int16_t *ref0, PixType *dst, H265MV mv, int32_t h, int32_t log2w, int32_t interp);

    inline uint64_t RD(int32_t d, uint32_t r, uint64_t lambda) {
        return ((uint64_t)d << 24) + lambda * r;
    }

    inline uint32_t RD32(int32_t d, uint32_t r, uint32_t lambda) {
        assert(d < 0x100000);
        assert(uint64_t(d << 11) + uint64_t(lambda * r) < 0x100000000);
        return (d << 11) + lambda * r;
    }

    int32_t HaveTopRight  (BlockSize bsize, int32_t miRow, int32_t miCol, int32_t haveTop,  int32_t haveRight,  int32_t txsz, int32_t y, int32_t x, int32_t ss_x);
    int32_t HaveBottomLeft(BlockSize bsize, int32_t miRow, int32_t miCol, int32_t haveLeft, int32_t haveBottom, int32_t txsz, int32_t y, int32_t x, int32_t ss_y);

    inline int32_t av1_is_directional_mode(PredMode mode) {return mode >= V_PRED && mode <= D63_PRED;}
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

}  // namespace H265Enc

