//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_CTB_H__
#define __MFX_H265_CTB_H__

#include <limits.h> /* for INT_MAX on Linux/Android */
#include "mfx_h265_defs.h"
#include "mfx_h265_set.h"
#include "mfx_h265_bitstream.h"
#include "mfx_h265_optimization.h"
#include "mfx_h265_sao_filter.h"
#include "mfx_h265_tables.h"
#include "mfx_h265_frame.h"

using namespace MFX_HEVC_PP;

namespace H265Enc {

struct H265VideoParam;

//struct H265MV
//{
//    Ipp16s  mvx;
//    Ipp16s  mvy;
//};

#ifdef MEMOIZE_SUBPEL

#ifdef MEMOIZE_CAND
template <typename PixType>
class MemCand
{
public:
    Ipp32s count;
    Ipp32s  *satd8  [MEMOIZE_NUMCAND];
    H265MV  mv[MEMOIZE_NUMCAND][2];
    Ipp32s  refIdx[MEMOIZE_NUMCAND][2];
    Ipp32s  list[MEMOIZE_NUMCAND];
#ifdef MEMOIZE_CAND_SUBPEL
    PixType *predBuf [MEMOIZE_NUMCAND];
#endif
    void    Init() { count = 0; }
#ifdef MEMOIZE_CAND_SUBPEL
    void    Init(Ipp32u idx, Ipp32s *buf, PixType *pBuf) { if(idx<MEMOIZE_NUMCAND) { satd8[idx] = buf; predBuf[idx] = pBuf; } }
#else
    void    Init(Ipp32u idx, Ipp32s *buf) { if(idx<MEMOIZE_NUMCAND) { satd8[idx] = buf; } }
#endif
};
#endif

template <typename PixType>
class MemPred
{
public:
    bool    done;
    H265MV  mv;
    Ipp16s *predBufHi;
    PixType *predBuf;
    void Init() {done = false;}
    void Init(Ipp16s *bufHi, PixType *buf) {done = false; predBufHi=bufHi; predBuf=buf;}
};
class MemHad
{
public:
    Ipp32s  count;
    H265MV  mv[4];
    Ipp32s *satd8[4];
    void Init() {count = 0;}
    void Init(Ipp32s *buf0, Ipp32s *buf1, Ipp32s *buf2, Ipp32s *buf3) { count = 0; satd8[0]=buf0; satd8[1]=buf1; satd8[2]=buf2;  satd8[3]=buf3;}
};
class MemBest
{
public:
    Ipp32s  done;
    H265MV  mv;
    void Init() {done = 0;}
};
#endif

template <int MAX_NUM_CAND>
struct MvPredInfo
{
    H265MV mvCand[2 * MAX_NUM_CAND];
    Ipp8s  refIdx[2 * MAX_NUM_CAND];
    Ipp32s numCand;
};

struct IntraLumaMode
{
    Ipp8u    mode;      // 0..34
    Ipp32s   numBits;   // estimated number of bits * 256
    Ipp32s   satd;
    CostType cost;      // satd + bitCost or full RD cost
    CostType bitCost;   // numBits * lambda
};

const H265MV MV_ZERO = {};

Ipp32s operator == (const H265MV &mv1, const H265MV &mv2);
Ipp32s operator != (const H265MV &mv1, const H265MV &mv2);
Ipp32s qdiff(const H265MV *mv1, const H265MV *mv2);
Ipp32s qcost(const H265MV *mv);
Ipp32s qdist(const H265MV *mv1, const H265MV *mv2);

struct H265CUData
{
public:
    H265MV mv[2];
    H265MV mvd[2];
    Ipp8s refIdx[2];
    Ipp8u depth;
    Ipp8u predMode;
    Ipp8u trIdx;
    Ipp8s qp;
    Ipp8u cbf[3];
    Ipp8u intraLumaDir;
    Ipp8u intraChromaDir;
    Ipp8u interDir;
    Ipp8u size;
    Ipp8u partSize;
    Ipp8u mergeIdx;
    Ipp8s mvpIdx[2];

    Ipp8u transformSkipFlag[3];
    union {
        struct {
            Ipp8u mergeFlag : 1;
            Ipp8u ipcmFlag : 1;
            Ipp8u transquantBypassFlag : 1;
            Ipp8u skippedFlag : 1;
        } flags;
        Ipp8u _flags;
    };
	Ipp8u reserved[2];
};

typedef struct {
    Ipp64u cost[4];
    Ipp32u num[4];
} costStat;

struct H265CUPtr {
    H265CUData *ctbData;
    Ipp32s ctbAddr;
    Ipp32s absPartIdx;
};
#define MAX_PU_IN_CTB (64/8*2 * 64/8*2)
#define MAX_PU_CASES_IN_CTB (MAX_PU_IN_CTB*4/3+2)
struct H265MEInfo
{
    Ipp8u posx, posy; // in pix inside LCU
    Ipp8u width, height;
    H265MV MV[2]; // [fwd/bwd]
    Ipp8s refIdx[2];
    Ipp32s absPartIdx;
    Ipp8u interDir;   // INTER_DIR_PRED_LX
    Ipp8u splitMode;
    Ipp8u depth;
};

#define IS_INTRA(data, partIdx) ((data)[partIdx].predMode == MODE_INTRA)

static inline Ipp8u isSkipped (H265CUData *data, Ipp32u partIdx)
{
    return data[partIdx].flags.skippedFlag;
}

//#define MAX_TOTAL_DEPTH (MAX_CU_DEPTH+4)

#define CHROMA_SIZE_DIV 2 // 4 - 4:2:0 ; 2 - 4:2:2,4:2:0 ; 1 - 4:4:4,4:2:2,4:2:0

template <class PixType> struct GetHistogramType;
template <> struct GetHistogramType<Ipp8u> { typedef Ipp16u type; };
template <> struct GetHistogramType<Ipp16u> { typedef Ipp32u type; };

template <typename PixType>
class H265CU
{
public:
#ifdef AMT_ALT_ENCODE
    Ipp32s m_isRdoq;
#endif
    typedef typename GetHistogramType<PixType>::type HistType;

    H265VideoParam *m_par;

    Ipp8s  m_sliceQpY;
    Ipp8s* m_lcuQps;

    Frame      *m_currFrame;
    Ipp32u          m_ctbAddr;           ///< CU address in a slice
    Ipp32u          m_absIdxInLcu;      ///< absolute address in a CU. It's Z scan order
    Ipp32u          m_ctbPelX;           ///< CU position in a pixel (X)
    Ipp32u          m_ctbPelY;           ///< CU position in a pixel (Y)
    Ipp32u          m_numPartition;     ///< total number of minimum partitions in a CU
    __ALIGN32 CoeffsType    m_residualsY[MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType    m_residualsU[MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
    __ALIGN32 CoeffsType    m_residualsV[MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
    __ALIGN32 PixType       m_predIntraAll[35*32*32];
    __ALIGN32 PixType       m_srcTr[32*32]; // transposed src block

    // working/final coeffs
    __ALIGN32 CoeffsType    *m_coeffWorkY;//[MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType    *m_coeffWorkU;//[MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
    __ALIGN32 CoeffsType    *m_coeffWorkV;//[MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
    // temporarily best coeffs for lower depths
    __ALIGN32 CoeffsType    m_coeffStoredY[5+1][MAX_CU_SIZE * MAX_CU_SIZE];     // (+1 for Intra_NxN)
    __ALIGN32 CoeffsType    m_coeffStoredU[5+1][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV]; // (+1 for Intra Chroma, temp until code is cleaned up)
    __ALIGN32 CoeffsType    m_coeffStoredV[5+1][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV]; // (+1 for Intra Chroma, temp until code is cleaned up)
    // inter reconstruct pixels
    __ALIGN32 PixType       m_recStoredY[5+1][MAX_CU_SIZE * MAX_CU_SIZE];       // (+1 for Intra_NxN)
    __ALIGN32 PixType       m_recStoredC[5+1][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV * 2];   // (+1 for Intra Chroma, temp until code is cleaned up)
    __ALIGN32 PixType       m_interRecWorkY[MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 PixType       m_interRecWorkC[MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
#ifdef AMT_ALT_ENCODE
    // inter prediction pixels
    __ALIGN32 PixType       m_interPredBufsY[5][2][MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 PixType       m_interPredBufsC[5][2][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV * 2];
    // inter residual
    __ALIGN32 CoeffsType    m_interResidBufsY[5][2][MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType    m_interResidBufsU[5][2][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
    __ALIGN32 CoeffsType    m_interResidBufsV[5][2][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
#else
    // inter prediction pixels
    __ALIGN32 PixType       m_interPredBufsY[2][MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 PixType       m_interPredBufsC[2][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV * 2];
    // inter residual
    __ALIGN32 CoeffsType    m_interResidBufsY[2][MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType    m_interResidBufsU[2][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
    __ALIGN32 CoeffsType    m_interResidBufsV[2][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
#endif

    // buffer contains the result of 1-ref interpolation between 2 PredInterUni calls
    __ALIGN32 Ipp16s    m_interpBuf[MAX_CU_SIZE*MAX_CU_SIZE];

    PixType    *m_interPredY;
    PixType    *m_interPredC;
    CoeffsType *m_interResidY;
    CoeffsType *m_interResidU;
    CoeffsType *m_interResidV;
#ifdef AMT_ALT_ENCODE
    PixType    *m_interPredSavedY[5][MAX_NUM_PARTITIONS];
    PixType    *m_interPredSavedC[5][MAX_NUM_PARTITIONS];
    CoeffsType *m_interResidSavedY[5][MAX_NUM_PARTITIONS];
    CoeffsType *m_interResidSavedU[5][MAX_NUM_PARTITIONS];
    CoeffsType *m_interResidSavedV[5][MAX_NUM_PARTITIONS];
#endif
    PixType    *m_interPredBestY;
    PixType    *m_interPredBestC;
    CoeffsType *m_interResidBestY;
    CoeffsType *m_interResidBestU;
    CoeffsType *m_interResidBestV;

    // stored CABAC contexts for each CU level (temp best)
    CABAC_CONTEXT_H265  m_ctxStored[5+1][NUM_CABAC_CONTEXT]; // (+1 for Intra_NxN)

    CostType m_costStored[5+1]; // stored RD costs (+1 for Intra_NxN)
    CostType m_costCurr;        // current RD cost

    Ipp8u         m_inNeighborFlags[4*MAX_CU_SIZE+1];
    Ipp8u         m_outNeighborFlags[4*MAX_CU_SIZE+1];
    H265CUData*   m_above;          ///< pointer of above CU
    H265CUData*   m_left;           ///< pointer of left CU
    H265CUData*   m_aboveLeft;      ///< pointer of above-left CU
    H265CUData*   m_aboveRight;     ///< pointer of above-right CU

    Ipp32u m_region_border_right, m_region_border_bottom;
    Ipp32u m_region_border_left, m_region_border_top;

    // merge and AMVP candidates shared between function calls for one CU depth, one part mode and all PUs (1, 2, or 4)
    MvPredInfo<5> m_mergeCand[4];
    MvPredInfo<2> m_amvpCand[4][2 * MAX_NUM_REF_IDX];
    Ipp32s        m_skipCandBest;

    Ipp32s m_aboveAddr;
    Ipp32s m_leftAddr;
    Ipp32s m_aboveLeftAddr;
    Ipp32s m_aboveRightAddr;
    Ipp32s m_aboveSameTile;
    Ipp32s m_leftSameTile;
    Ipp32s m_aboveLeftSameTile;
    Ipp32s m_aboveRightSameTile;
    Ipp8s *m_colFrmRefFramesTbList[2];
    bool  *m_colFrmRefIsLongTerm[2];
    H265EdgeData m_edge[9][9][4];

    Ipp32u m_bakAbsPartIdxCu;

    EnumIntraAngMode m_cuIntraAngMode;
    Ipp8u            m_mpm[3];          // list of most probable intra modes, shared within CheckIntraLuma
    Ipp8u            m_mpmSorted[3];    // mpm sorted as in 8.4.2.4

    PixType *m_ySrc;
    PixType *m_uvSrc;
    Ipp32s m_pitchSrcLuma;
    Ipp32s m_pitchSrcChroma;
    PixType *m_yRec;
    PixType *m_uvRec;
    Ipp32s m_pitchRecLuma;
    Ipp32s m_pitchRecChroma;

    static const Ipp32s NUM_DATA_STORED_LAYERS = 5 + 1;
    H265CUData *m_data; // 1 layer,
    H265CUData *m_dataStored; // depth array, current best for CU

    H265CUData* m_ctbData;//all data

    __ALIGN32 HistType m_hist4[40 * MAX_CU_SIZE / 4 * MAX_CU_SIZE / 4];
    __ALIGN32 HistType m_hist8[40 * MAX_CU_SIZE / 8 * MAX_CU_SIZE / 8];
    bool m_isHistBuilt;

    H265BsFake *m_bsf;

    Ipp8u m_rdOptFlag;
    Ipp64f m_rdLambda;
    Ipp64f m_rdLambdaChroma;
    Ipp64f m_rdLambdaInter;
    Ipp64f m_rdLambdaInterMv;
    Ipp64f m_rdLambdaSqrt;
    Ipp64f m_ChromaDistWeight;

    H265Slice *m_cslice;
    Ipp8u m_intraMinDepth;      // min CU depth from spatial analysis
    Ipp8u m_adaptMinDepth;      // min CU depth from collocated CUs
    Ipp8u m_projMinDepth;      // min CU depth from root CU proj
    Ipp8u m_adaptMaxDepth;      // max CU depth from Projected CUs
    Ipp32s HorMax;              // MV common limits in CU
    Ipp32s HorMin;
    Ipp32s VerMax;
    Ipp32s VerMin;
    costStat *m_costStat;

    SaoEstimator m_saoEst;

#if defined(AMT_ICRA_OPT)
    Ipp32s  m_lcuCs[(MAX_CU_SIZE/4)*(MAX_CU_SIZE/4)];
    Ipp32s  m_lcuRs[(MAX_CU_SIZE/4)*(MAX_CU_SIZE/4)];
    Ipp32s  m_SCid[5][MAX_NUM_PARTITIONS];
    Ipp32f  m_SCpp[5][MAX_NUM_PARTITIONS];
    Ipp32s  m_STC[5][MAX_NUM_PARTITIONS];
    Ipp32s  m_mvdMax;
    Ipp32s  m_mvdCost;
    bool    m_bIntraCandInBuf;
    Ipp32s  m_IntraCandMaxSatd;
#endif

#ifdef MEMOIZE_SUBPEL
    __ALIGN32 Ipp16s  m_predBufHi3[MAX_NUM_REF_IDX][4][4][(MAX_CU_SIZE+MEMOIZE_SUBPEL_EXT_W) * (MAX_CU_SIZE+MEMOIZE_SUBPEL_EXT_H)];
    __ALIGN32 PixType m_predBuf3  [MAX_NUM_REF_IDX][4][4][(MAX_CU_SIZE+MEMOIZE_SUBPEL_EXT_W) * (MAX_CU_SIZE+MEMOIZE_SUBPEL_EXT_H)];
    __ALIGN32 Ipp16s  m_predBufHi2[MAX_NUM_REF_IDX][4][4][((MAX_CU_SIZE>>1)+MEMOIZE_SUBPEL_EXT_W) * ((MAX_CU_SIZE>>1)+MEMOIZE_SUBPEL_EXT_H)];
    __ALIGN32 PixType m_predBuf2  [MAX_NUM_REF_IDX][4][4][((MAX_CU_SIZE>>1)+MEMOIZE_SUBPEL_EXT_W) * ((MAX_CU_SIZE>>1)+MEMOIZE_SUBPEL_EXT_H)];
    __ALIGN32 Ipp16s  m_predBufHi1[MAX_NUM_REF_IDX][4][4][((MAX_CU_SIZE>>2)+MEMOIZE_SUBPEL_EXT_W) * ((MAX_CU_SIZE>>2)+MEMOIZE_SUBPEL_EXT_H)];
    __ALIGN32 PixType m_predBuf1  [MAX_NUM_REF_IDX][4][4][((MAX_CU_SIZE>>2)+MEMOIZE_SUBPEL_EXT_W) * ((MAX_CU_SIZE>>2)+MEMOIZE_SUBPEL_EXT_H)];
    __ALIGN32 Ipp16s  m_predBufHi0[MAX_NUM_REF_IDX][4][4][((MAX_CU_SIZE>>3)+MEMOIZE_SUBPEL_EXT_W) * ((MAX_CU_SIZE>>3)+MEMOIZE_SUBPEL_EXT_H)];
    __ALIGN32 PixType m_predBuf0  [MAX_NUM_REF_IDX][4][4][((MAX_CU_SIZE>>3)+MEMOIZE_SUBPEL_EXT_W) * ((MAX_CU_SIZE>>3)+MEMOIZE_SUBPEL_EXT_H)];

    Ipp32s m_satd8Buf1[MAX_NUM_REF_IDX][4][4][4][((MAX_CU_SIZE>>3)>>2) *((MAX_CU_SIZE>>3)>>2)];
    Ipp32s m_satd8Buf2[MAX_NUM_REF_IDX][4][4][4][((MAX_CU_SIZE>>3)>>1) *((MAX_CU_SIZE>>3)>>1)];
    Ipp32s m_satd8Buf3[MAX_NUM_REF_IDX][4][4][4][((MAX_CU_SIZE>>3)   ) *((MAX_CU_SIZE>>3)   )];

    MemPred<PixType>        m_memSubpel[4][MAX_NUM_REF_IDX][4][4];  // [size][Refs][hPh][vPh]
    MemHad                  m_memSubHad[4][MAX_NUM_REF_IDX][4][4];  // [size][Refs][hPh][vPh]
    MemBest                 m_memBestMV[4][MAX_NUM_REF_IDX];

#define INTERP_BUF_SZ 16 // must be power of 2, need min 10 bufs for effectiveness -NG
    struct interp_store {
        __ALIGN32 Ipp16s predBufY[MAX_CU_SIZE*MAX_CU_SIZE];
        Ipp8s refIdx;
        H265MV mv;
    } m_interpBufBi[INTERP_BUF_SZ];
    Ipp8u m_interpIdxFirst, m_interpIdxLast;

#ifdef MEMOIZE_CAND
    MemCand<PixType>        m_memCandSubHad[4];  // [sizes]

    Ipp32s m_satd8CandBuf0[MEMOIZE_NUMCAND][((MAX_CU_SIZE>>3)>>3) *((MAX_CU_SIZE>>3)>>3)];
    Ipp32s m_satd8CandBuf1[MEMOIZE_NUMCAND][((MAX_CU_SIZE>>3)>>2) *((MAX_CU_SIZE>>3)>>2)];
    Ipp32s m_satd8CandBuf2[MEMOIZE_NUMCAND][((MAX_CU_SIZE>>3)>>1) *((MAX_CU_SIZE>>3)>>1)];
    Ipp32s m_satd8CandBuf3[MEMOIZE_NUMCAND][((MAX_CU_SIZE>>3)   ) *((MAX_CU_SIZE>>3)   )];
#ifdef MEMOIZE_CAND_SUBPEL
    __ALIGN32 PixType m_predBufCand3  [MEMOIZE_NUMCAND][(MAX_CU_SIZE) * (MAX_CU_SIZE)];
    __ALIGN32 PixType m_predBufCand2  [MEMOIZE_NUMCAND][(MAX_CU_SIZE>>1) * (MAX_CU_SIZE>>1)];
    __ALIGN32 PixType m_predBufCand1  [MEMOIZE_NUMCAND][(MAX_CU_SIZE>>2) * (MAX_CU_SIZE>>2)];
    __ALIGN32 PixType m_predBufCand0  [MEMOIZE_NUMCAND][(MAX_CU_SIZE>>3) * (MAX_CU_SIZE>>3)];
#endif
#endif
#endif

    Ipp32s m_SlicePixRowFirst;
    Ipp32s m_SlicePixRowLast;

    inline bool  IsIntra(Ipp32u partIdx)
    { return m_data[partIdx].predMode == MODE_INTRA; }

    inline Ipp8u GetTransformSkip(Ipp32u idx,EnumTextType type)
    { return m_data[idx].transformSkipFlag[h265_type2idx[type]];}

    inline Ipp8u GetCbf(Ipp32u idx, EnumTextType type, Ipp32u trDepth )
    { return (Ipp8u)( ( m_data[idx].cbf[h265_type2idx[type]] >> trDepth ) & 0x1 ); }

    inline void SetCbfZero(Ipp32u idx, EnumTextType type, Ipp32u trDepth )
    {  m_data[idx].cbf[h265_type2idx[type]] &= ~(1 << trDepth); }

    inline void SetCbfOne(Ipp32u idx, EnumTextType type, Ipp32u trDepth )
    {  m_data[idx].cbf[h265_type2idx[type]] |= (1 << trDepth); }

    Ipp8u GetQtRootCbf(Ipp32u idx);

    void GetPuLeft(H265CUPtr *cu, Ipp32u currPartUnitIdx, Ipp32s enforceSliceRestriction = true,
                   Ipp32s enforceTileRestriction = true);
    void GetPuLeftOf(H265CUPtr *cu, H265CUPtr *cuBase, Ipp32u currPartUnitIdx, Ipp32s enforceSliceRestriction = true,
                   Ipp32s enforceTileRestriction = true);

    void GetPuAbove(H265CUPtr *cu, Ipp32u currPartUnitIdx, Ipp32s enforceSliceRestriction = true,
                    Ipp32s planarAtLcuBoundary = false,
                    Ipp32s enforceTileRestriction = true);
    void GetPuAboveOf(H265CUPtr *cu, H265CUPtr *cuBase, Ipp32u currPartUnitIdx, Ipp32s enforceSliceRestriction = true,
                    Ipp32s planarAtLcuBoundary = false,
                    Ipp32s enforceTileRestriction = true);

    void GetCtuBelow(H265CUPtr *cu,
                        Ipp32s enforceSliceRestriction = true,
                        Ipp32s enforceTileRestriction = true);

    void GetCtuRight(H265CUPtr *cu,
                       Ipp32s enforceSliceRestriction = true,
                       Ipp32s enforceTileRestriction = true);

    void GetCtuBelowRight(H265CUPtr *cu,
        Ipp32s enforceSliceRestriction = true,
        Ipp32s enforceTileRestriction = true);

    bool GetTempMvPred(const H265CUData *currPb, Ipp32s xPb, Ipp32s yPb, Ipp32s nPbW, Ipp32s nPbH,
                       Ipp32s listIdx, Ipp32s refIdx, H265MV *mvLxCol);

    bool GetColMv(const H265CUData *currPb, Ipp32s listIdxCurr, Ipp32s refIdxCurr,
                  const Frame *colPic, const H265CUData *colPb, H265MV *mvLxCol);

    H265CUData *GetNeighbour(Ipp32s &neighbourBlockZScanIdx, Ipp32s neighbourBlockColumn,
                             Ipp32s neighbourBlockRow, Ipp32s curBlockZScanIdx,
                             bool isNeedTocheckCurLCU);

    bool AddMvpCand(MvPredInfo<2> *info, H265CUData *data, Ipp32s blockZScanIdx, Ipp32s refPicListIdx,
                    Ipp32s refIdx, bool order);

    void GetMvpCand(Ipp32s topLeftCUBlockZScanIdx, Ipp32s refPicListIdx, Ipp32s refIdx,
                    Ipp32s partMode, Ipp32s partIdx, Ipp32s cuSize, MvPredInfo<2> *info);

    void GetMergeCand(Ipp32s topLeftCUBlockZScanIdx, Ipp32s partMode, Ipp32s partIdx, Ipp32s cuSize,
                      MvPredInfo<5> *mergeInfo);

    void GetAmvpCand(Ipp32s topLeftBlockZScanIdx, Ipp32s partMode, Ipp32s partIdx,
                     MvPredInfo<2> amvpInfo[2 * MAX_NUM_REF_IDX]);

    void SetupMvPredictionInfo(Ipp32s blockZScanIdx, Ipp32s partMode, Ipp32s curPUidx);

    void GetPartOffsetAndSize(Ipp32s idx, Ipp32s partMode, Ipp32s cuSize, Ipp32s &partX,
                              Ipp32s &partY, Ipp32s &partWidth, Ipp32s &partHeight);

    bool CheckIdenticalMotion(const Ipp8s refIdx[2], const H265MV mvs[2]) const;

    Ipp32s ClipMV(H265MV &rcMv) const; // returns 1 if changed, otherwise 0

    H265CUData *GetQpMinCuLeft(Ipp32u &uiLPartUnitIdx, Ipp32u uiCurrAbsIdxInLCU,
                               bool bEnforceSliceRestriction = true,
                               bool bEnforceDependentSliceRestriction = true);

    H265CUData *GetQpMinCuAbove(Ipp32u &aPartUnitIdx, Ipp32u currAbsIdxInLCU,
                                bool enforceSliceRestriction = true,
                                bool enforceDependentSliceRestriction = true);

    Ipp8s GetRefQp(Ipp32u uiCurrAbsIdxInLCU);
    void SetQpSubParts(int qp, int absPartIdx, int depth);
    void SetQpSubCUs(int qp, int absPartIdx, int depth, bool &foundNonZeroCbf);
    void CheckDeltaQp(void);

    void ConvertTransIdx(Ipp32u trIdx, Ipp32u &lumaTrMode, Ipp32u &chromaTrMode)  {
        lumaTrMode   = trIdx;
        chromaTrMode = trIdx;
        return;
    }

    void GetAllowedChromaDir(Ipp32s absPartIdx, Ipp8u *modeList);

    Ipp32s GetIntradirLumaPred(Ipp32s absPartIdx, Ipp8u *intraDirPred);

    Ipp32u GetCtxSplitFlag(Ipp32s absPartIdx, Ipp32u depth);

    Ipp32u GetCtxQtCbf(EnumTextType type, Ipp32u trDepth) const;

    Ipp32u GetTransformIdx(Ipp32s absPartIdx) { return (Ipp32u)m_data[absPartIdx].trIdx; }

    Ipp32u GetCtxSkipFlag(Ipp32s absPartIdx);

    Ipp32u GetCtxInterDir(Ipp32s absPartIdx);

    Ipp32u GetCoefScanIdx(Ipp32s absPartIdx, Ipp32u log2Width, Ipp32s bIsLuma, Ipp32s bIsIntra) const;

    template <class H265Bs>
    void CodeCoeffNxN(H265Bs *bs, H265CU *pCU, CoeffsType *coeffs, Ipp32s absPartIdx, Ipp32u width, EnumTextType type);

    template <class H265Bs>
    void PutTransform(H265Bs *bs, Ipp32u offsetLuma, Ipp32u offsetChroma, Ipp32s absPartIdx,
                      Ipp32u absTuPartIdx, Ipp32u depth, Ipp32u width, Ipp32u trIdx,
                      Ipp8u& codeDqp, Ipp8u rd_mode = RD_CU_ALL);

    template <class H265Bs>
    void EncodeSao(H265Bs *bs, Ipp32s absPartIdx, Ipp32s depth, Ipp8u rdMode,
                   SaoCtuParam &saoBlkParam, bool leftMergeAvail, bool aboveMergeAvail);

    template <class H265Bs>
    void EncodeCU(H265Bs *bs, Ipp32s absPartIdx, Ipp32s depth, Ipp8u rdMode = 0);

    void UpdateCuQp(void);

    template <class H265Bs>
    void EncodeCoeff(H265Bs *bs, Ipp32s absPartIdx, Ipp32u depth, Ipp32u width,
                     Ipp8u &codeDqp, Ipp8u rd_mode);

    template <class H265Bs>
    void CodeIntradirLumaAng(H265Bs *bs, Ipp32s absPartIdx, Ipp8u multiple);

    Ipp32s GetIntraLumaModeCost(Ipp8u mode, const CABAC_CONTEXT_H265 ctx); // don't update m_bsf's contexts

    Ipp32s GetIntraLumaModeCost(Ipp8u mode); // update cabac m_bsf's contexts

    CostType GetIntraChromaModeCost(Ipp32s absPartIdx);

    CostType GetTransformSubdivFlagCost(Ipp32s depth, Ipp32s trDepth);
    
    CostType GetCuSplitFlagCost(H265CUData *data, Ipp32s absPartIdx, Ipp32s depth);

    CostType GetCuModesCost(H265CUData *data, Ipp32s absPartIdx, Ipp32s depth);

    template <EnumTextType PLANE_TYPE>
    void PredInterUni(Ipp32s puX, Ipp32s puY, Ipp32s puW, Ipp32s puH, Ipp32s listIdx,
                      const Ipp8s refIdx[2], const H265MV mvs[2], PixType *dst, Ipp32s dstPitch,
                      Ipp32s isBiPred, MFX_HEVC_PP::EnumAddAverageType eAddAverage, Ipp32s isFast);
    template <EnumTextType PLANE_TYPE>
    void InterPredCu(Ipp32s absPartIdx, Ipp8u depth, PixType *dst, Ipp32s pitchDst);

    void IntraPred(Ipp32s absPartIdx, Ipp8u depth);

    void IntraPredTu(Ipp32s absPartIdx, Ipp32u idx422, Ipp32s width, Ipp32s predMode, Ipp8u isLuma);

    void GetAngModesFromHistogram(Ipp32s xPu, Ipp32s yPu, Ipp32s puSize, Ipp8s *modes, Ipp32s numModes);

    Ipp8u GetTrSplitMode(Ipp32s absPartIdx, Ipp8u depth, Ipp8u trDepth, Ipp8u partSize, Ipp8u strict = 1);

    void TransformInv(Ipp32s offset, Ipp32s width, Ipp8u isLuma, Ipp8u isIntra, Ipp8u bitDepth);

    void TransformInv2(void *dst, Ipp32s pitch_dst, Ipp32s offset, Ipp32s width, Ipp8u isLuma,
                       Ipp8u isIntra, Ipp8u bitDepth);

    void TransformFwd(CoeffsType *src, Ipp32s srcPitch, CoeffsType *dst, Ipp32s width, Ipp32s bitDepth, Ipp8u isIntra);

    void GetInitAvailablity();

    bool EncAndRecLuma(Ipp32s absPartIdx, Ipp32s offset, Ipp8u depth, CostType *cost);

    void EncAndRecChroma(Ipp32s absPartIdx, Ipp32s offset, Ipp8u depth, CostType *cost, IntraPredOpt pred_opt);

    void EncAndRecChromaUpdate(Ipp32s absPartIdx, Ipp32s offset, Ipp8u depth, bool interMod);
    
    void EncAndRecLumaTu(Ipp32s absPartIdx, Ipp32s offset, Ipp32s width, CostType *cost,
                         Ipp8u costPredFlag, IntraPredOpt predOpt);

    void EncAndRecChromaTu(Ipp32s absPartIdx, Ipp32s idx422, Ipp32s offset, Ipp32s width, CostType *cost,
                           IntraPredOpt pred_opt, Ipp8u costPredFlag);

    void QuantInvTu(Ipp32s absPartIdx, Ipp32s offset, Ipp32s width, Ipp32s qp, Ipp32s isLuma);
#ifdef AMT_DZ_RDOQ
    void QuantFwdTu(Ipp32s absPartIdx, Ipp32s offset, Ipp32s width, Ipp32s qp, Ipp32s isLuma, Ipp32s isIntra);
#else
    void QuantFwdTu(Ipp32s absPartIdx, Ipp32s offset, Ipp32s width, Ipp32s qp, Ipp32s isLuma);
#endif

    void QuantFwdTuBase(Ipp32s abs_part_idx, Ipp32s offset, Ipp32s width, Ipp32s qp, Ipp32s is_luma);

    void Deblock();

    void DeblockOneCrossLuma(Ipp32s curPixelColumn, Ipp32s curPixelRow);

    void DeblockOneCrossChroma(Ipp32s curPixelColumn, Ipp32s curPixelRow);

    void SetEdgesCTU(H265CUPtr *curLCU,
                    Ipp32s width,
                    Ipp32s height,
                    Ipp32s x_inc,
                    Ipp32s y_inc);
    void SetEdges(Ipp32s width, Ipp32s height);

    void GetEdgeStrength(H265CUPtr *pcCUQ, H265EdgeData *edge, Ipp32s curPixelColumn,
                         Ipp32s curPixelRow, Ipp32s crossSliceBoundaryFlag,
                         Ipp32s crossTileBoundaryFlag, Ipp32s tcOffset, Ipp32s betaOffset,
                         Ipp32s dir);
            
    void EstimateSao(H265BsReal* bs, SaoCtuParam* saoParam);
    void PackSao(H265BsReal* bs, SaoCtuParam* saoParam);

    void FillSubPart(Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trIdx, Ipp8u partSize, Ipp8u lumaDir,
                     Ipp8u qp);

    void FillSubPartIntraLumaDir(Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth, Ipp8u lumaDir);

    void CopySubPartTo(H265CUData *dataCopy, Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth);

    void CalcCostLuma(Ipp32s absPartIdx, Ipp8u depth, Ipp8u trDepth, CostOpt costOpt, IntraPredOpt intraPredOpt);

    //CostType CalcCostSkipExperimental(Ipp32s absPartIdx, Ipp8u depth);

    Ipp32s CheckMergeCand(const H265MEInfo *meInfo, const MvPredInfo<5> *mergeCand,
                          Ipp32s useHadamard, Ipp32s *mergeCandIdx);

    bool CheckFrameThreadingSearchRange(const H265MEInfo *meInfo, const H265MV *mv) const;
    bool CheckIndepRegThreadingSearchRange(const H265MEInfo *meInfo, const H265MV *mv, Ipp8u subpel = 0) const;

    void RefineBiPred(const H265MEInfo *meInfo, const Ipp8s refIdxs[2], Ipp32s curPUidx,
                      H265MV mvs[2], Ipp32s *cost, Ipp32s *mvCost);

    void MeCu(Ipp32s absPartIdx, Ipp8u depth);
#ifdef AMT_ALT_FAST_SKIP
    bool CheckMerge2Nx2N(Ipp32s absPartIdx, Ipp8u depth);
#else
    void CheckMerge2Nx2N(Ipp32s absPartIdx, Ipp8u depth);
#endif

    void CheckInter(Ipp32s absPartIdx, Ipp8u depth, Ipp32s partMode, const H265MEInfo *meInfo2Nx2N);

    void CheckIntra(Ipp32s absPartIdx, Ipp32s depth);

    void CheckIntraLuma(Ipp32s absPartIdx, Ipp32s depth);

    void CheckIntraChroma(Ipp32s absPartIdx, Ipp32s depth);

    void FilterIntraPredPels(const PixType *predPel, PixType *predPelFilt, Ipp32s width);

    Ipp32s InitIntraLumaModes(Ipp32s absPartIdx, Ipp32s depth, Ipp32s trDepth,
                              IntraLumaMode *modes);

    Ipp32s FilterIntraLumaModesBySatd(Ipp32s absPartIdx, Ipp32s depth, Ipp32s trDepth,
                                      IntraLumaMode *modes, Ipp32s numModes);

    Ipp32s FilterIntraLumaModesByRdoTr0(Ipp32s absPartIdx, Ipp32s depth, Ipp32s trDepth,
                                        IntraLumaMode *modes, Ipp32s numModes);

    Ipp32s FilterIntraLumaModesByRdoTrAll(Ipp32s absPartIdx, Ipp32s depth, Ipp32s trDepth,
                                          IntraLumaMode *modes, Ipp32s numModes);
#ifdef AMT_ICRA_OPT
    Ipp32s MePu(H265MEInfo *meInfos, Ipp32s partIdx, bool doME=true);
    Ipp32s MePuGacc(H265MEInfo *meInfos, Ipp32s partIdx);
#else
    void MePu(H265MEInfo *meInfos, Ipp32s partIdx);
    void MePuGacc(H265MEInfo *meInfos, Ipp32s partIdx);
#endif

    Ipp32s PuCost(H265MEInfo *meInfo);

    bool CheckGpuIntraCost(Ipp32s absPartIdx, Ipp32s depth) const;

#ifdef AMT_INT_ME_SEED
    void MeIntPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, const FrameData *ref,
                  H265MV *mv, Ipp32s *cost, Ipp32s *mvCost, Ipp32s meStepRange = INT_MAX) const;
    Ipp16s MeIntSeed(const H265MEInfo *meInfo, const MvPredInfo<2> *amvp, Ipp32s list, 
                     Ipp32s refIdx, H265MV mvRefBest0, H265MV mvRefBest00, 
                     H265MV &mvBest, Ipp32s &costBest, Ipp32s &mvCostBest, H265MV &mvBestSub);
#else
    void MeIntPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, const Frame *ref,
                  H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;
#endif

    void MeIntPelFullSearch(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                            const FrameData *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;

#ifdef AMT_INT_ME_SEED
    void MeIntPelLog(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, const FrameData *ref,
                     H265MV *mv, Ipp32s *cost, Ipp32s *mvCost, Ipp32s meStepRange) const;
#else
    void MeIntPelLog(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, const Frame *ref,
                     H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;
#endif

#ifdef MEMOIZE_SUBPEL
    void MeSubPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, const FrameData *ref,
                  H265MV *mv, Ipp32s *cost, Ipp32s *mvCost, Ipp32s refIdxMem);
#ifdef AMT_FAST_SUBPEL_SEED
    bool MeSubPelSeed(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, 
                      const FrameData *ref, H265MV &mvBest, Ipp32s &costBest, Ipp32s &mvCostBest,
                      Ipp32s refIdxMem, H265MV &mvInt, H265MV &mvBestSub);
#endif
#else
    void MeSubPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, const Frame *ref,
                  H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;
#endif


    void AddMvCost(const MvPredInfo<2> *predInfo, Ipp32s log2Step, const Ipp32s *dists, H265MV *mv,
                   Ipp32s *costBest, Ipp32s *mvCostBest, Ipp32s patternSubpel) const;

    void MeSubPelBatchedBox(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, const FrameData *ref,
                            H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;

#ifdef MEMOIZE_SUBPEL
    void MemSubPelBatchedBox(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                                         const FrameData *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost, Ipp32s refIdxMem, Ipp32s size);
    void MemSubPelBatchedFastBoxDiaOrth(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                                         const FrameData *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost, Ipp32s refIdxMem, Ipp32s size);
#endif
    void MeSubPelBatchedDia(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, const FrameData *ref,
                            H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;

    void CuCost(Ipp32s absPartIdx, Ipp8u depth, const H265MEInfo* bestInfo);

    void TuGetSplitInter(Ipp32s absPartIdx, Ipp8u trDepth, Ipp8u trDepthMax, CostType *cost);

    void TuMaxSplitInter(Ipp32s absPartIdx, Ipp8u trIdxMax);

    void DetailsXY(H265MEInfo *meInfo) const;

    void MeInterpolate(const H265MEInfo* meInfo, const H265MV *MV, PixType *in_pSrc,
                       Ipp32s in_SrcPitch, PixType *buf, Ipp32s buf_pitch, Ipp32s isFast) const;

    void MeInterpolateCombine(const H265MEInfo* meInfo, const H265MV *MV, PixType *in_pSrc,
                       Ipp32s in_SrcPitch, Ipp16s *bufHi, Ipp32s buf_pitch) const;

    Ipp32s MatchingMetricPu(const PixType *src, const H265MEInfo* meInfo, const H265MV* mv,
                            const FrameData *refPic, Ipp32s useHadamard) const;

    void MatchingMetricPuCombine(Ipp32s *had, const PixType *src, const H265MEInfo* meInfo, const H265MV* mv, const H265MV* mvB,
                            const Ipp8s refIdx[2], const Ipp8s refIdxB[2], Ipp32s useHadamard) const;
#ifdef MEMOIZE_SUBPEL
    void   MemoizeInit();
    void   MemoizeClear(Ipp8u depth);
    Ipp32s MatchingMetricPuMem(const PixType *src, const H265MEInfo *meInfo, const H265MV *mv, 
                            const FrameData *refPic, Ipp32s useHadamard, Ipp32s refIdxMem, Ipp32s size, 
                            Ipp32s& hadFoundSize);
    Ipp32s MatchingMetricPuMemSubpelUse(const PixType *src, const H265MEInfo *meInfo, const H265MV *mv,
                                         const FrameData *refPic, Ipp32s useHadamard, Ipp32s refIdxMem, Ipp32s size);

    Ipp32s tuHadSave(const PixType* src, Ipp32s pitchSrc, const PixType* rec, Ipp32s pitchRec,
             Ipp32s width, Ipp32s height, Ipp32s *satd, Ipp32s memPitch);
    /*
    Ipp32s tuHadSave(const Ipp16u* src, Ipp32s pitchSrc, const Ipp16u* rec, Ipp32s pitchRec,
             Ipp32s width, Ipp32s height, Ipp32s *satd, Ipp32s memPitch);
    */
    Ipp32s tuHadUse(Ipp32s width, Ipp32s height, Ipp32s *satd8, Ipp32s memPitch) const;

    bool MemHadUse(Ipp32s size, Ipp32s hPh, Ipp32s vPh, const H265MEInfo* meInfo, Ipp32s refIdxMem, 
                    const H265MV *mv, Ipp32s& satd, Ipp32s& foundSize);

    bool MemHadFirst(Ipp32s size, const H265MEInfo* meInfo, Ipp32s refIdxMem);

    bool MemHadSave(Ipp32s size, Ipp32s hPh, Ipp32s vPh, const H265MEInfo* meInfo, Ipp32s refIdxMem, 
                    const H265MV *mv, Ipp32s*& satd8, Ipp32s& memPitch);

    void MemHadGetBuf(Ipp32s size, Ipp32s hPh, Ipp32s vPh, Ipp32s refIdxMem, const H265MV *mv, Ipp32s*& satd8);

    bool MemSubpelUse(Ipp32s size, Ipp32s hPh, Ipp32s vPh, const H265MEInfo* meInfo, Ipp32s refIdxMem, 
                    const H265MV *mv, const PixType*& predBuf, Ipp32s& memPitch);
#ifdef AMT_INT_ME_SEED
    bool  MemBestMV(Ipp32s size, Ipp32s refIdxMem, H265MV *mv);
    void  SetMemBestMV(Ipp32s size, const H265MEInfo* meInfo, Ipp32s refIdxMem, H265MV mv);
#endif
    bool  MemSubpelUse(Ipp32s size, Ipp32s hPh, Ipp32s vPh, const H265MEInfo* meInfo, Ipp32s refIdxMem, 
                    const H265MV *mv, Ipp16s*& predBuf, Ipp32s& memPitch);

    bool  MemSubpelInRange(Ipp32s size, const H265MEInfo* meInfo, Ipp32s refIdxMem, const H265MV *mv);
    
    void  MemSubpelGetBufSetMv(Ipp32s size, H265MV *mv, Ipp32s refIdxMem, PixType*& predBuf, 
                            Ipp16s *&predBufHi);
    //void  MemSubpelSetMv(Ipp32s size, Ipp32s hPh, Ipp32s vPh, Ipp32s refIdxMem, H265MV *mv);

    bool  MemSubpelSave(Ipp32s size, Ipp32s hPh, Ipp32s vPh, 
                        const H265MEInfo* meInfo, Ipp32s refIdxMem, 
                        const H265MV *mv, PixType*& predBuf, Ipp32s& memPitch,
                        PixType*& hpredBuf, Ipp16s*& predBufHi, Ipp16s*& hpredBufHi);

    void MeInterpolateSave(const H265MEInfo* me_info, const H265MV *MV, PixType *src,
                            Ipp32s srcPitch, PixType *dst, Ipp32s dstPitch, 
                            PixType *hdst, Ipp16s *dstHi, Ipp16s *hdstHi) const;

    Ipp32s MatchingMetricBipredPuSearch(const PixType *src, const H265MEInfo *meInfo, const Ipp8s refIdx[2], 
                                        const H265MV mvs[2], Ipp32s useHadamard);

    void InterploateBipredUseHi(Ipp32s size, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                      const H265MV mvs[2], PixType* predBuf);

    void MeInterpolateUseHi(const H265MEInfo* meInfo, const H265MV *mv, PixType *src,
                                    Ipp32s srcPitch, Ipp16s*& dst, Ipp32s& dstPitch, Ipp32s size, Ipp32s refIdxMem);

    void MeInterpolateUseSaveHi(const H265MEInfo* meInfo, const H265MV *mv, PixType *src,
                                    Ipp32s srcPitch, Ipp16s*& dst, Ipp32s& dstPitch, Ipp32s size, Ipp32s refIdxMem);
#ifdef MEMOIZE_CAND
#ifdef MEMOIZE_CAND_SUBPEL
    bool  MemCandUseSubpel(const H265MEInfo* meInfo, const Ipp8s listIdx, const Ipp8s *refIdx, 
                            const H265MV *mv,  const PixType*& predBuf, Ipp32s& memPitch);
    bool  MemSCandSave(const H265MEInfo* meInfo, Ipp32s listIdx, const Ipp8s *refIdx, 
                        const H265MV *mv, Ipp32s **satd8, PixType **predBuf, Ipp32s *memPitch, 
                        bool useHadamard);
    bool  MemBiCandSave(const H265MEInfo* meInfo, const Ipp8s *refIdx, const H265MV *mv, 
                        Ipp32s **satd8, PixType **predBuf, Ipp32s *memPitch, bool useHadamard);
    bool  MemSCandUse(const H265MEInfo* meInfo, Ipp32s listIdx, const Ipp8s *refIdx, const H265MV *mv, const PixType*& predBuf, Ipp32s **satd8, 
                        Ipp32s *memPitch, bool useHadamard);
    bool  MemBiCandUse(const H265MEInfo* meInfo, const Ipp8s *refIdx, const H265MV *mv,  const PixType*& predBuf, Ipp32s **satd8, 
                        Ipp32s *memPitch, bool useHadamard);
#else
    bool  MemSCandSave(const H265MEInfo* meInfo, Ipp32s listIdx, const Ipp8s *refIdx, const H265MV *mv,
                        Ipp32s **satd8, Ipp32s *memPitch, bool useHadamard);
    bool  MemBiCandSave(const H265MEInfo* meInfo, const Ipp8s *refIdx, const H265MV *mv, Ipp32s **satd8, 
                        Ipp32s *memPitch, bool useHadamard);
    bool  MemSCandUse(const H265MEInfo* meInfo, Ipp32s listIdx, const Ipp8s *refIdx, const H265MV *mv, Ipp32s **satd8, 
                        Ipp32s *memPitch, bool useHadamard);
    bool  MemBiCandUse(const H265MEInfo* meInfo, const Ipp8s *refIdx, const H265MV *mv,  Ipp32s **satd8, 
                        Ipp32s *memPitch, bool useHadamard);
#endif


#ifdef MEMOIZE_CAND_SUBPEL
    Ipp32s MatchingMetricBipredPuUse(const PixType *src, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                     const H265MV mvs[2], Ipp32s useHadamard, const PixType *predBuf, Ipp32s memPitch);
    Ipp32s MatchingMetricBipredPuSave(const PixType *src, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                      const H265MV mvs[2], Ipp32s useHadamard, Ipp32s *satd8, PixType *predBuf, Ipp32s memPitch);
    Ipp32s MatchingMetricPuUse(const PixType *src, const H265MEInfo *meInfo, const H265MV *mv,
                                const FrameData *refPic, Ipp32s useHadamard, const PixType *predBuf, Ipp32s memPitch);
    Ipp32s MatchingMetricPuSave(const PixType *src, const H265MEInfo *meInfo, const H265MV *mv,
                                const FrameData *refPic, Ipp32s useHadamard, Ipp32s *satd8, PixType *predBuf, Ipp32s memPitch);
    
#else
    Ipp32s MatchingMetricPuSave(const PixType *src, const H265MEInfo *meInfo, const H265MV *mv,
                                const Frame *refPic, Ipp32s useHadamard, Ipp32s *satd8, Ipp32s memPitch);
    Ipp32s MatchingMetricBipredPuSave(const PixType *src, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                      const H265MV mvs[2], Ipp32s useHadamard, Ipp32s *satd8, Ipp32s memPitch);
#endif

    Ipp32s MatchingMetricPuMemSCand(const PixType *src, const H265MEInfo *meInfo, Ipp32s listIdx, 
                    const Ipp8s *refIdx, const H265MV *mv, const FrameData *refPic, Ipp32s useHadamard);
    Ipp32s MatchingMetricBiPredPuMemCand(const PixType *src, const H265MEInfo *meInfo, 
                    const Ipp8s *refIdx, const H265MV *mv, Ipp32s useHadamard);
#endif
#endif

    Ipp32s MatchingMetricBipredPu(const PixType *src, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                  const H265MV mvs[2], Ipp32s useHadamard);

    Ipp32s MvCost1RefLog(H265MV mv, const MvPredInfo<2> *predInfo) const;

    Ipp32s MvCost1RefLog(Ipp16s mvx, Ipp16s mvy, const MvPredInfo<2> *predInfo) const;

    void InitCu(H265VideoParam *_par, H265CUData *_data, H265CUData *_dataTemp, Ipp32s cuAddr,
                H265BsFake *_bsf, H265Slice *cslice, ThreadingTaskSpecifier stage,
                costStat* _costStat, const Frame* frame, CoeffsType *m_coeffWork);

#if defined(AMT_ICRA_OPT)
    //void CalcRsCs(void);
    void GetSpatialComplexity(Ipp32s absPartIdx, Ipp32s depth, Ipp32s partAddr, Ipp32s partDepth);
    Ipp32s GetSpatialComplexity(Ipp32s absPartIdx, Ipp32s depth, Ipp32s partAddr, Ipp32s partDepth, Ipp32f& SCpp) const;
    Ipp32s GetSpatioTemporalComplexity(Ipp32s absPartIdx, Ipp32s depth, Ipp32s partAddr, Ipp32s partDepth);
    Ipp32s GetSpatioTemporalComplexity(Ipp32s absPartIdx, Ipp32s depth, Ipp32s partAddr, Ipp32s partDepth, Ipp32s& scVal);
    Ipp32s GetSpatioTemporalComplexityColocated(Ipp32s absPartIdx, Ipp32s depth, Ipp32s partAddr, Ipp32s partDepth, FrameData *ref) const;
    Ipp8u EncInterLumaTuGetBaseCBF(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width);
    void EncInterChromaTuGetBaseCBF(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp8u *nz);
    bool TuMaxSplitInterHasNonZeroCoeff(Ipp32u absPartIdx, Ipp8u trIdxMax);
    bool tryIntraICRA(Ipp32s absPartIdx, Ipp32s depth);
    bool tryIntraRD(Ipp32s absPartIdx, Ipp32s depth, IntraLumaMode *modes);
    H265CUData* GetCuDataXY(Ipp32s x, Ipp32s y, Frame *ref);
    void GetProjectedDepth(Ipp32s absPartIdx, Ipp32s depth, Ipp8u splitMode);
    void FastCheckAMP(Ipp32s absPartIdx, Ipp8u depth, const H265MEInfo *meInfo2Nx2N);
    void CheckSkipCandFullRD(const H265MEInfo *meInfo, const MvPredInfo<5> *mergeCand, Ipp32s *mergeCandIdx);
#ifdef AMT_ADAPTIVE_INTRA_RD
    Ipp32s GetNumIntraRDModes(Ipp32s absPartIdx, Ipp32s depth, Ipp32s trDepth, IntraLumaMode *modes, Ipp32s numModes);
#endif
#endif
    CostType ModeDecision(Ipp32s absPartIdx, Ipp8u depth);

    bool getdQPFlag           ()                        { return m_bEncodeDQP;        }
    void setdQPFlag           ( bool b )                { m_bEncodeDQP = b;           }

    bool HaveChromaRec() const;
    bool IsTuSplitIntra() const;
    bool UpdateChromaRec() const;

    H265CUData *StoredCuData(Ipp32s depth) const;

    void SaveFinalCuDecision(Ipp32s absPartIdx, Ipp32s depth, bool Chroma);
    void LoadFinalCuDecision(Ipp32s absPartIdx, Ipp32s depth, bool Chroma);
    void LoadWorkingCuDecision(Ipp32s absPartIdx, Ipp32s depth);
    const H265CUData *GetBestCuDecision(Ipp32s absPartIdx, Ipp32s depth) const;

    void SaveInterTuDecision(Ipp32s absPartIdx, Ipp32s depth);
    void LoadInterTuDecision(Ipp32s absPartIdx, Ipp32s depth);

    void SaveIntraLumaDecision(Ipp32s absPartIdx, Ipp32s depth);
    void LoadIntraLumaDecision(Ipp32s absPartIdx, Ipp32s depth);

    void SaveIntraChromaDecision(Ipp32s absPartIdx, Ipp32s depth);
    void LoadIntraChromaDecision(Ipp32s absPartIdx, Ipp32s depth);

    void SaveBestInterPredAndResid(Ipp32s absPartIdx, Ipp32s depth);
    void LoadBestInterPredAndResid(Ipp32s absPartIdx, Ipp32s depth);
#ifdef AMT_ADAPTIVE_INTRA_DEPTH
    Ipp8u GetAdaptiveIntraMinDepth(Ipp32s absPartIdx, Ipp32s depth, Ipp32s& maxSC);
#endif
    void GetAdaptiveMinDepth(Ipp32s absPartIdx, Ipp32s depth);
#ifdef AMT_ADAPTIVE_INTER_MIN_DEPTH
    bool JoinCU(Ipp32s absPartIdx, Ipp32s depth);
#endif

    void SetCuLambda(Frame* frame);

    void SetCuLambdaRoi(Frame* frame);

private:
#ifndef AMT_ALT_ENCODE
    Ipp32s m_isRdoq;
#endif
    bool m_bEncodeDQP;

    // random generation code, for development purposes
    //void FillRandom(Ipp32s absPartIdx, Ipp8u depth);
    //void FillZero(Ipp32s absPartIdx, Ipp8u depth);
};

template <class H265Bs>
void CodeSaoCtbOffsetParam(H265Bs *bs, int compIdx, SaoOffsetParam& ctbParam, bool sliceEnabled);

template <class H265Bs>
void CodeSaoCtbParam(H265Bs *bs, SaoCtuParam &saoBlkParam, bool *sliceEnabled, bool leftMergeAvail,
                     bool aboveMergeAvail, bool onlyEstMergeInfo);

Ipp32s tuHad(const Ipp8u *src, Ipp32s pitch_src, const Ipp8u *rec, Ipp32s pitch_rec,
             Ipp32s width, Ipp32s height);
Ipp32s tuHad(const Ipp16u *src, Ipp32s pitch_src, const Ipp16u *rec, Ipp32s pitch_rec,
             Ipp32s width, Ipp32s height);

Ipp32u GetQuadtreeTuLog2MinSizeInCu(const H265VideoParam *par, Ipp32u log2CbSize,
                                    Ipp8u partSize, Ipp8u predMode);

void GetPartAddr(Ipp32s uPartIdx, Ipp32s partMode, Ipp32s m_NumPartition, Ipp32s &partAddr);


template <class H265CuBase>
Ipp32s GetLastValidPartIdx(H265CuBase* cu, Ipp32s absPartIdx);

template <class H265CuBase>
Ipp8s GetLastCodedQP(H265CuBase* cu, Ipp32s absPartIdx);

Ipp32s GetLumaOffset(const H265VideoParam *par, Ipp32s absPartIdx, Ipp32s pitch);

Ipp32s GetChromaOffset(const H265VideoParam *par, Ipp32s absPartIdx, Ipp32s pitch);

void CopySubPartTo_(H265CUData *dst, const H265CUData *src, Ipp32s absPartIdx, Ipp32u numParts);

void FillSubPartIntraCuModes_(H265CUData *data, Ipp32s numParts, Ipp8u cuWidth, Ipp8u cuDepth, Ipp8u partMode);
void FillSubPartIntraPartMode_(H265CUData *data, Ipp32s numParts, Ipp8u partMode);
void FillSubPartCuQp_(H265CUData *data, Ipp32s numParts, Ipp8u qp);
void FillSubPartIntraPredModeY_(H265CUData *data, Ipp32s numParts, Ipp8u mode);
void FillSubPartIntraPredModeC_(H265CUData *data, Ipp32s numParts, Ipp8u mode);
void FillSubPartTrDepth_(H265CUData *data, Ipp32s numParts, Ipp8u trDepth);
void FillSubPartSkipFlag_(H265CUData *data, Ipp32s numParts, Ipp8u skipFlag);

template <size_t COMP_IDX> void SetCbfBit(H265CUData *data, Ipp32u trDepth)
{
    VM_ASSERT(COMP_IDX <= 2);
    data->cbf[COMP_IDX] |= (1 << trDepth);
}
template <size_t COMP_IDX> void SetCbf(H265CUData *data, Ipp32u trDepth)
{
    VM_ASSERT(COMP_IDX <= 2);
    data->cbf[COMP_IDX] = (1 << trDepth);
}
template <size_t COMP_IDX> void ResetCbf(H265CUData *data)
{
    VM_ASSERT(COMP_IDX <= 2);
    data->cbf[COMP_IDX] = 0;
}

Ipp8s GetChromaQP(Ipp8s qpy, Ipp32s chromaQpOffset, Ipp8u chromaFormatIdc, Ipp8u bitDepthChroma);

template<class T>
Ipp32u CheckSum(const T *buf, Ipp32s size, Ipp32u initialSum = 0)
{ return CheckSum((const Ipp8u *)buf, sizeof(T) * size, initialSum); }

template<class T>
Ipp32u CheckSum(const T *buf, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32u initialSum = 0)
{ return CheckSum((const Ipp8u *)buf, sizeof(T) * pitch, sizeof(T) * width, height, initialSum); }

template<>
Ipp32u CheckSum<Ipp8u>(const Ipp8u *buf, Ipp32s size, Ipp32u initialSum);

template<>
Ipp32u CheckSum<Ipp8u>(const Ipp8u *buf, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32u initialSum);

Ipp32u CheckSumCabacCtx(const Ipp8u buf[188]);

Ipp32u CheckSumCabacCtxChroma(const Ipp8u buf[188]);

} // namespace

#endif // __MFX_H265_CTB_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
