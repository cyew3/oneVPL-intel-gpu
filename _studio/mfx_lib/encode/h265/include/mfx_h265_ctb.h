//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_CTB_H__
#define __MFX_H265_CTB_H__

#include "mfx_h265_defs.h"
#include "mfx_h265_set.h"
#include "mfx_h265_bitstream.h"
#include "mfx_h265_optimization.h"
#include "mfx_h265_sao_filter.h"
#include "mfx_h265_tables.h"
#include "mfx_h265_paq.h"

using namespace MFX_HEVC_PP;

namespace H265Enc {

struct H265MV
{
    Ipp16s  mvx;
    Ipp16s  mvy;
};

template <int MAX_NUM_CAND>
struct MvPredInfo
{
    H265MV mvCand[2 * MAX_NUM_CAND];
    Ipp8s  refIdx[2 * MAX_NUM_CAND];
    Ipp32s numCand;
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
    Ipp8u depth;
    Ipp8u size;
    Ipp8u partSize;
    Ipp8u predMode;
    Ipp8u trIdx;
    Ipp8s qp;
    Ipp8u cbf[3];
    Ipp8u intraLumaDir;
    Ipp8u intraChromaDir;
    Ipp8u interDir;
    Ipp8u mergeIdx;
    Ipp8s mvpIdx[2];
    Ipp8s mvpNum[2];
    H265MV mv[2];
    H265MV mvd[2];
    Ipp8s refIdx[2];
    Ipp8u curIdx; // index to interleaving cur/best buffers in ME
    Ipp8u curRecIdx; // index to interleaving Inter recinstruct buffers
    Ipp8u bestRecIdx; // index to interleaving Inter recinstruct buffers


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
};

typedef struct {
    Ipp64u cost[4];
    Ipp32u num[4];
} costStat;

struct H265CUPtr {
    H265CUData *ctbData;
    Ipp32s ctbAddr;
    Ipp32u absPartIdx;
};
#define MAX_PU_IN_CTB (64/8*2 * 64/8*2)
#define MAX_PU_CASES_IN_CTB (MAX_PU_IN_CTB*4/3+2)
struct H265MEInfo
{
    Ipp8u posx, posy; // in pix inside LCU
    Ipp8u width, height;
    H265MV MV[2]; // [fwd/bwd]
    Ipp8s refIdx[2];
    Ipp32u absPartIdx;
    Ipp8u interDir;   // INTER_DIR_PRED_LX
    Ipp8u splitMode;
    Ipp8u depth;
};

#define IS_INTRA(data, partIdx) ((data)[partIdx].predMode == MODE_INTRA)

static inline Ipp8u isSkipped (H265CUData *data, Ipp32u partIdx)
{
    return data[partIdx].flags.skippedFlag;
}

#define MAX_TOTAL_DEPTH (MAX_CU_DEPTH+4)

template <typename PixType>
class H265CU
{
public:
    H265VideoParam *m_par;
    H265Frame      *m_currFrame;
    H265CUData     *m_data;
    Ipp32u          m_ctbAddr;           ///< CU address in a slice
    Ipp32u          m_absIdxInLcu;      ///< absolute address in a CU. It's Z scan order
    Ipp32u          m_ctbPelX;           ///< CU position in a pixel (X)
    Ipp32u          m_ctbPelY;           ///< CU position in a pixel (Y)
    Ipp32u          m_numPartition;     ///< total number of minimum partitions in a CU
    __ALIGN32 CoeffsType    m_trCoeffY[MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType    m_trCoeffU[MAX_CU_SIZE * MAX_CU_SIZE / 4];
    __ALIGN32 CoeffsType    m_trCoeffV[MAX_CU_SIZE * MAX_CU_SIZE / 4];
    __ALIGN32 CoeffsType    m_residualsY[MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType    m_residualsU[MAX_CU_SIZE * MAX_CU_SIZE / 4];
    __ALIGN32 CoeffsType    m_residualsV[MAX_CU_SIZE * MAX_CU_SIZE / 4];
    __ALIGN32 PixType       m_predIntraAll[35*32*32];
    __ALIGN32 PixType       m_tuSrcTransposed[32*32];

    //[idxBest][depth][raster CTU]
    __ALIGN32 PixType       m_interPred[2][4][MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 PixType       m_interPredChroma[2][4][MAX_CU_SIZE * MAX_CU_SIZE/2];
    __ALIGN32 CoeffsType    m_interResidualsY[2][4][MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType    m_interResidualsU[2][4][MAX_CU_SIZE * MAX_CU_SIZE/4];
    __ALIGN32 CoeffsType    m_interResidualsV[2][4][MAX_CU_SIZE * MAX_CU_SIZE/4];

    __ALIGN32 PixType       m_interRec[3][5][MAX_CU_SIZE*MAX_CU_SIZE];
    __ALIGN32 PixType       m_interRecChroma[3][5][MAX_CU_SIZE*MAX_CU_SIZE/2];

    __ALIGN32 PixType       m_interRecBest[5][MAX_CU_SIZE*MAX_CU_SIZE];
    __ALIGN32 PixType       m_interRecBestChroma[5][MAX_CU_SIZE*MAX_CU_SIZE / 2];

    Ipp32s                  m_interPredReady;
    //Ipp32s                  m_curIdx[4];

    CostType m_intraCosts[35];
    Ipp8u    m_intraModes[35];
    Ipp32s   m_intraBits[35];
    Ipp32s   m_predIntraAllWidth;

    Ipp8u         m_inNeighborFlags[4*MAX_CU_SIZE+1];
    Ipp8u         m_outNeighborFlags[4*MAX_CU_SIZE+1];
    H265CUData*   m_above;          ///< pointer of above CU
    H265CUData*   m_left;           ///< pointer of left CU
    H265CUData*   m_aboveLeft;      ///< pointer of above-left CU
    H265CUData*   m_aboveRight;     ///< pointer of above-right CU
    H265CUData*   m_colocatedCu[2];

    // merge and AMVP candidates shared between function calls for one CU depth, one part mode and all PUs (1, 2, or 4)
    MvPredInfo<5> m_mergeCand[4];
    MvPredInfo<2> m_amvpCand[4][2 * MAX_NUM_REF_IDX];

    Ipp32s m_aboveAddr;
    Ipp32s m_leftAddr;
    Ipp32s m_aboveLeftAddr;
    Ipp32s m_aboveRightAddr;
    Ipp8s *m_colFrmRefFramesTbList[2];
    bool  *m_colFrmRefIsLongTerm[2];
    H265EdgeData m_edge[9][9][4];

    Ipp32u m_bakAbsPartIdxCu;
    Ipp32u m_bakAbsPartIdx;
    Ipp32u m_bakChromaOffset;

    // aya - may be used late to speed up SAD calculation
    //__ALIGN32 Ipp8u m_src_aligned_block[MAX_CU_SIZE*MAX_CU_SIZE];

    PixType *m_ySrc;
    PixType *m_uvSrc;
    Ipp32s m_pitchSrc;
    PixType *m_yRec;
    PixType *m_uvRec;
    Ipp32s m_pitchRec;
    H265CUData *m_dataSave;  // All CU array, final best, the only source for neibours
    H265CUData *m_dataBest;  // depth array, for ModeDecision tree
    H265CUData *m_dataTemp;  // depth array
    H265CUData *m_dataTemp2;
    H265CUData *m_dataInter; // best Inter for current depth
    H265CUData* m_ctbData;//all data
    __ALIGN32 PixType m_recLumaSaveCu[6][MAX_CU_SIZE*MAX_CU_SIZE];
    __ALIGN32 PixType m_recChromaSaveCu[6][MAX_CU_SIZE*MAX_CU_SIZE/2];
    __ALIGN32 PixType m_recLumaSaveTu[6][MAX_CU_SIZE*MAX_CU_SIZE];
    __ALIGN32 PixType m_recChromaSaveTu[6][MAX_CU_SIZE*MAX_CU_SIZE/2];
    //__ALIGN32 PixType m_recLumaInter[6][MAX_CU_SIZE*MAX_CU_SIZE];
    //__ALIGN32 PixType m_recChromaInter[6][MAX_CU_SIZE*MAX_CU_SIZE/2];
    __ALIGN32 Ipp16s  m_tmpPredBuf[MAX_CU_SIZE*MAX_CU_SIZE]; // buffer contains the result of 1-ref interpolation between 2 PredInterUni calls

    H265BsFake *m_bsf;

    Ipp8u m_rdOptFlag;
    Ipp64f m_rdLambda;
    Ipp64f m_rdLambdaInter;
    Ipp64f m_rdLambdaInterMv;
    //kolya
    //to match HM's HAD search for Intra modes
    template <class H265Bs> void xEncIntraHeaderChroma(H265Bs* bs);
    Ipp64f m_rdLambdaSqrt;
    Ipp64f m_ChromaDistWeight;

    H265Slice *m_cslice;
    Ipp8u m_depthMin;            // for Cu-tree branch to know if there is not SPLIT_MUST cu at lower depth
    Ipp8u m_depthMinCollocated;  // min CU depth from collocated CUs
    Ipp32s HorMax;             // MV common limits in CU
    Ipp32s HorMin;
    Ipp32s VerMax;
    Ipp32s VerMin;
    const Ipp8u *m_logMvCostTable;
    costStat *m_costStat;

    SaoEncodeFilter m_saoEncodeFilter;

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

    inline Ipp8u GetQtRootCbf(Ipp32u idx)
    { return GetCbf( idx, TEXT_LUMA, 0 ) || GetCbf( idx, TEXT_CHROMA_U, 0 ) || GetCbf( idx, TEXT_CHROMA_V, 0 ); }

    void GetPuLeft(H265CUPtr *cu, Ipp32u currPartUnitIdx, Ipp32s enforceSliceRestriction = true
                   //,Ipp32s enforceDependentSliceRestriction = true
                   //,Ipp32s enforceTileRestriction = true
                   );

    void GetPuAbove(H265CUPtr *cu, Ipp32u currPartUnitIdx, Ipp32s enforceSliceRestriction = true,
//                    Ipp32s enforceDependentSliceRestriction = true, Ipp32s motionDataCompresssion = false,
                    Ipp32s planarAtLcuBoundary = false
//                    , Ipp32s enforceTileRestriction = true
                    );

    bool GetTempMvPred(const H265CUData *currPb, Ipp32s xPb, Ipp32s yPb, Ipp32s nPbW, Ipp32s nPbH,
                       Ipp32s listIdx, Ipp32s refIdx, H265MV *mvLxCol);

    bool GetColMv(const H265CUData *currPb, Ipp32s listIdxCurr, Ipp32s refIdxCurr,
                  const H265Frame *colPic, const H265CUData *colPb, H265MV *mvLxCol);

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

    Ipp32u GetIntraSizeIdx(Ipp32u absPartIdx);

    void ConvertTransIdx(Ipp32u trIdx, Ipp32u &lumaTrMode, Ipp32u &chromaTrMode)  {
        lumaTrMode   = trIdx;
        chromaTrMode = trIdx;
        return;
    }

    void GetAllowedChromaDir(Ipp32u absPartIdx, Ipp8u *modeList);

    Ipp32s GetIntradirLumaPred(Ipp32u absPartIdx, Ipp8u *intraDirPred);

    Ipp32u GetCtxSplitFlag(Ipp32u absPartIdx, Ipp32u depth);

    Ipp32u GetCtxQtCbf(EnumTextType type, Ipp32u trDepth) const;

    Ipp32u GetTransformIdx(Ipp32u absPartIdx) { return (Ipp32u)m_data[absPartIdx].trIdx; }

    Ipp32u GetCtxSkipFlag(Ipp32u absPartIdx);

    Ipp32u GetCtxInterDir(Ipp32u absPartIdx);

    Ipp32u GetCoefScanIdx(Ipp32u absPartIdx, Ipp32u log2Width, Ipp32s bIsLuma, Ipp32s bIsIntra) const;

    template <class H265Bs>
    void CodeCoeffNxN(H265Bs *bs, H265CU *pCU, CoeffsType *coeffs, Ipp32u absPartIdx, Ipp32u width, EnumTextType type);

    template <class H265Bs>
    void PutTransform(H265Bs *bs, Ipp32u offsetLuma, Ipp32u offsetChroma, Ipp32u absPartIdx,
                      Ipp32u absTuPartIdx, Ipp32u depth, Ipp32u width, Ipp32u trIdx,
                      Ipp8u& codeDqp, Ipp8u splitFlagOnly = 0);

    template <class H265Bs>
    void EncodeSao(H265Bs *bs, Ipp32u absPartIdx, Ipp32s depth, Ipp8u rdMode,
                   SaoCtuParam &saoBlkParam, bool leftMergeAvail, bool aboveMergeAvail);

    template <class H265Bs>
    void EncodeCU(H265Bs *bs, Ipp32u absPartIdx, Ipp32s depth, Ipp8u rdMode = 0);

    void UpdateCuQp(void);

    template <class H265Bs>
    void EncodeCoeff(H265Bs *bs, Ipp32u absPartIdx, Ipp32u depth, Ipp32u width,
                     Ipp8u &codeDqp);

    template <class H265Bs>
    void CodeIntradirLumaAng(H265Bs *bs, Ipp32u absPartIdx, Ipp8u multiple);

    template <EnumTextType PLANE_TYPE>
    void PredInterUni(Ipp32s puX, Ipp32s puY, Ipp32s puW, Ipp32s puH, Ipp32s listIdx,
                      const Ipp8s refIdx[2], const H265MV mvs[2], PixType *dst, Ipp32s dstPitch,
                      Ipp32s isBiPred, MFX_HEVC_PP::EnumAddAverageType eAddAverage);

    template <EnumTextType PLANE_TYPE>
    void InterPredCu(Ipp32s absPartIdx, Ipp8u depth, PixType *dst, Ipp32s pitchDst);

    void IntraPred(Ipp32u absPartIdx, Ipp8u depth);

    void IntraPredTu(Ipp32s absPartIdx, Ipp32s width, Ipp32s predMode, Ipp8u isLuma);

    void IntraLumaModeDecision(Ipp32s absPartIdx, Ipp32u offset, Ipp8u depth, Ipp8u trDepth);

    void IntraLumaModeDecisionRDO(Ipp32s absPartIdx, Ipp32u offset, Ipp8u depth, Ipp8u trDepth);

    Ipp8u GetTrSplitMode(Ipp32s absPartIdx, Ipp8u depth, Ipp8u trDepth, Ipp8u partSize, Ipp8u strict = 1);

    void TransformInv(Ipp32s offset, Ipp32s width, Ipp8u isLuma, Ipp8u isIntra, Ipp8u bitDepth);

    void TransformInv2(void *dst, Ipp32s pitch_dst, Ipp32s offset, Ipp32s width, Ipp8u isLuma,
                       Ipp8u isIntra, Ipp8u bitDepth);

    void TransformFwd(Ipp32s offset, Ipp32s width, Ipp8u isLuma, Ipp8u isIntra);

    void GetInitAvailablity();

    void EncAndRecLuma(Ipp32u absPartIdx, Ipp32s offset, Ipp8u depth, Ipp8u *nz, CostType *cost);

    void EncAndRecChroma(Ipp32u absPartIdx, Ipp32s offset, Ipp8u depth, Ipp8u *nz, CostType *cost);

    void EncAndRecLumaTu(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp8u *nz, CostType *cost,
                         Ipp8u cost_pred_flag, IntraPredOpt pred_opt);

    void EncAndRecChromaTu(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp8u *nz,
                           CostType *cost, IntraPredOpt pred_opt);

    void QuantInvTu(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp32s isLuma);

    void QuantFwdTu(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp32s isLuma);

    void Deblock();

    void DeblockOneCrossLuma(Ipp32s curPixelColumn, Ipp32s curPixelRow);

    void DeblockOneCrossChroma(Ipp32s curPixelColumn, Ipp32s curPixelRow);

    void SetEdges(Ipp32s width, Ipp32s height);

    void GetEdgeStrength(H265CUPtr *pcCUQ, H265EdgeData *edge, Ipp32s curPixelColumn,
                         Ipp32s curPixelRow, Ipp32s crossSliceBoundaryFlag,
                         Ipp32s crossTileBoundaryFlag, Ipp32s tcOffset, Ipp32s betaOffset,
                         Ipp32s dir);

    void EstimateCtuSao(H265BsFake *bs, SaoCtuParam *saoParam, SaoCtuParam *saoParam_TotalFrame,
                        const MFX_HEVC_PP::CTBBorders &borders, const Ipp8u *slice_ids);

    void GetStatisticsCtuSaoPredeblocked(const MFX_HEVC_PP::CTBBorders &borders);

    void FillSubPart(Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trIdx, Ipp8u partSize, Ipp8u lumaDir,
                     Ipp8u qp);

    void FillSubPartIntraLumaDir(Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth, Ipp8u lumaDir);

    void CopySubPartTo(H265CUData *dataCopy, Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth);

    void CalcCostLuma(Ipp32u absPartIdx, Ipp32s offset, Ipp8u depth, Ipp8u trDepth, CostOpt costOpt,
                      Ipp8u partSize, Ipp8u lumaDir, CostType *cost);

    //CostType CalcCostSkipExperimental(Ipp32u absPartIdx, Ipp8u depth);

    CostType MeCu(Ipp32u absPartIdx, Ipp8u depth, Ipp32s offset);

    void MePu(H265MEInfo *meInfo, Ipp32s lastPredIdx);
    Ipp32s PuCost(H265MEInfo *meInfo);

#if defined (MFX_ENABLE_CM)
    void MePuGacc(H265MEInfo *meInfo, Ipp32s lastPredIdx);

    bool CheckGpuIntraCost(Ipp32s leftPelX, Ipp32s topPelY, Ipp32s depth) const;
#endif // MFX_ENABLE_CM

    void MeIntPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, Ipp32s list,
                  Ipp32s refIdx, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;

    void MeIntPelFullSearch(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, Ipp32s list,
                            Ipp32s refIdx, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;

    void MeIntPelLog(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, Ipp32s list,
                     Ipp32s refIdx, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;

    void MeSubPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, Ipp32s meDir,
                  Ipp32s refIdx, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;

    CostType CuCost(Ipp32u absPartIdx, Ipp8u depth, const H265MEInfo* bestInfo, CostType bestCost);

    void TuGetSplitInter(Ipp32u absPartIdx, Ipp32s offset, Ipp8u trIdx, Ipp8u trIdxMax, Ipp8u nz[3],
                         CostType *cost, Ipp8u cbf[256][3], Ipp32s level);

    void TuMaxSplitInter(Ipp32u absPartIdx, Ipp8u trIdxMax, CostType *cost, Ipp8u cbf[256][3]);

    void DetailsXY(H265MEInfo *meInfo) const;

    void MeInterpolate(const H265MEInfo* meInfo, H265MV* MV, PixType *in_pSrc, Ipp32s in_SrcPitch,
                       PixType *buf, Ipp32s buf_pitch) const;

    Ipp32s MatchingMetricPu(PixType *src, const H265MEInfo* meInfo, H265MV* mv, H265Frame *refPic,
                            Ipp32s useHadamard) const;

    Ipp32s MatchingMetricBipredPu(const PixType *src, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                  const H265MV mvs[2], Ipp32s useHadamard);

    Ipp32s MvCost1RefLog(H265MV mv, Ipp8s refIdx, const MvPredInfo<2> *predInfo, Ipp32s rlist) const;

    void InitCu(H265VideoParam *_par, H265CUData *_data, H265CUData *_dataTemp, Ipp32s cuAddr,
                PixType *_y, PixType *_uv, Ipp32s _pitch, H265Frame *currFrame, H265BsFake *_bsf,
                H265Slice *cslice, Ipp32s initializeDataFlag, const Ipp8u *logMvCostTable);

    void ModeDecision(Ipp32u absPartIdx, Ipp32u offset, Ipp8u depth, CostType *cost);

    bool getdQPFlag           ()                        { return m_bEncodeDQP;        }
    void setdQPFlag           ( bool b )                { m_bEncodeDQP = b;           }
    int GetCalqDeltaQp(TAdapQP* sliceAQP, Ipp64f sliceLambda);
private:
    Ipp32s m_isRdoq;
    bool m_bEncodeDQP;

    // random generation code, for development purposes
    //void FillRandom(Ipp32u absPartIdx, Ipp8u depth);
    //void FillZero(Ipp32u absPartIdx, Ipp8u depth);
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
Ipp8s GetLastCodedQP(H265CuBase* cu, Ipp32u absPartIdx);
} // namespace

#endif // __MFX_H265_CTB_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
