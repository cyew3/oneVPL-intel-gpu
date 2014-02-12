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

using namespace MFX_HEVC_PP;

namespace H265Enc {

typedef struct
{
    Ipp16s  mvx;
    Ipp16s  mvy;
} H265MV;

typedef struct
{
    H265MV   mvCand[10];
    T_RefIdx refIdx[10];
    Ipp32s      numCand;
} MVPInfo;

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
    T_RefIdx refIdx[2];

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
    T_RefIdx refIdx[2];
    Ipp32s costOneDir[2];
    Ipp32s costBiDir;
    CostType costIntra;
    Ipp32s costInter;
    Ipp32s meParts[4]; // used if split
    Ipp32s details[2]; // [vert/horz]

    Ipp32u absPartIdx;
    Ipp8u interDir;   // INTER_DIR_PRED_LX
    Ipp8u splitMode;
    Ipp8u depth;
    Ipp8u mustSplit; // part of CU is out of the frame
    Ipp8u excluded;   // completely out of the frame
};

#define IS_INTRA(data, partIdx) ((data)[partIdx].predMode == MODE_INTRA)

static inline Ipp8u isSkipped (H265CUData *data, Ipp32u partIdx)
{
    return data[partIdx].flags.skippedFlag;
}

#define MAX_TOTAL_DEPTH (MAX_CU_DEPTH+4)

class H265CU
{
public:
    H265VideoParam *m_par;
    H265CUData *m_data;
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
    __ALIGN32 PixType       m_interPred[4][MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 PixType       m_interPredBest[4][MAX_CU_SIZE * MAX_CU_SIZE];
    PixType                 (*m_interPredPtr)[MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType       m_interResidualsY[4][MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType       m_interResidualsYBest[4][MAX_CU_SIZE * MAX_CU_SIZE];
    CoeffsType                 (*m_interResidualsYPtr)[MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 PixType       m_interRecBest[5][MAX_CU_SIZE*MAX_CU_SIZE];
    Ipp32s                  m_interPredReady;

    CostType m_intraBestCosts[35];
    Ipp8u    m_intraBestModes[35];
    Ipp64f   m_intraModeBitcost[35];

    Ipp32s m_predIntraAllWidth;
    Ipp8u           m_inNeighborFlags[4*MAX_CU_SIZE+1];
    Ipp8u           m_outNeighborFlags[4*MAX_CU_SIZE+1];
    H265CUData*   m_above;          ///< pointer of above CU
    H265CUData*   m_left;           ///< pointer of left CU
    H265CUData*   m_aboveLeft;      ///< pointer of above-left CU
    H265CUData*   m_aboveRight;     ///< pointer of above-right CU
    H265CUData*   m_colocatedLcu[2];
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

    Ipp8u *m_ySrc;
    Ipp8u *m_uvSrc;
    Ipp32s m_pitchSrc;
    Ipp8u *m_yRec;
    Ipp8u *m_uvRec;
    Ipp32s m_pitchRec;
    H265CUData *m_dataSave;
    H265CUData *m_dataBest;
    H265CUData *m_dataTemp;
    H265CUData *m_dataTemp2;
    PixType     m_recLumaSaveCu[6][MAX_CU_SIZE*MAX_CU_SIZE];
    PixType     m_recLumaSaveTu[6][MAX_CU_SIZE*MAX_CU_SIZE];
    Ipp16s m_predBufY[2][MAX_CU_SIZE*MAX_CU_SIZE];
    Ipp16s m_predBufUv[2][MAX_CU_SIZE*MAX_CU_SIZE/2];

    // storing predictions for bidir
#define INTERP_BUF_SZ 8 // must be power of 2
    struct interp_store {
        __ALIGN32 Ipp16s predBufY[MAX_CU_SIZE*MAX_CU_SIZE];
        PixType * pY;
        H265MV mv;
    } m_interpBuf[INTERP_BUF_SZ];
    Ipp8u m_interpIdxFirst, m_interpIdxLast;

    H265BsFake *m_bsf;
    Ipp8u m_rdOptFlag;
    Ipp64f m_rdLambda;
    Ipp64f m_rdLambdaInter;
    Ipp64f m_rdLambdaInterMv;
    H265Slice *m_cslice;
    Ipp8u m_depthMin;          // for Cu-tree branch to know if there is not SPLIT_MUST cu at lower depth
    Ipp32s HorMax;             // MV common limits in CU
    Ipp32s HorMin;
    Ipp32s VerMax;
    Ipp32s VerMin;

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

    void GetPuLeft(H265CUPtr *cu,
                   Ipp32u currPartUnitIdx,
                   Ipp32s enforceSliceRestriction=true,
                   Ipp32s enforceDependentSliceRestriction=true,
                   Ipp32s enforceTileRestriction=true );

    void GetPuAbove(H265CUPtr *cu,
                    Ipp32u currPartUnitIdx,
                    Ipp32s enforceSliceRestriction=true,
                    Ipp32s enforceDependentSliceRestriction=true,
                    Ipp32s motionDataCompresssion = false,
                    Ipp32s planarAtLcuBoundary = false,
                    Ipp32s enforceTileRestriction=true );

    bool GetColMvp(H265CUData *colLCU, Ipp32s blockZScanIdx, Ipp32s refPicListIdx, Ipp32s refIdx,
                   H265MV &rcMv);

    H265CUData *GetNeighbour(Ipp32s &neighbourBlockZScanIdx, Ipp32s neighbourBlockColumn,
                             Ipp32s neighbourBlockRow, Ipp32s curBlockZScanIdx,
                             bool isNeedTocheckCurLCU);

    bool AddMvpCand(MVPInfo *pInfo, H265CUData *data, Ipp32s blockZScanIdx, Ipp32s refPicListIdx,
                    Ipp32s refIdx, bool order);

    void GetMvpCand(Ipp32s topLeftCUBlockZScanIdx, Ipp32s refPicListIdx, Ipp32s refIdx,
                    Ipp32s partMode, Ipp32s partIdx, Ipp32s cuSize, MVPInfo *pInfo);

    void GetInterMergeCandidates(Ipp32s topLeftCUBlockZScanIdx, Ipp32s partMode, Ipp32s partIdx,
                                 Ipp32s cuSize, MVPInfo *pInfo);

    void GetPuMvInfo(Ipp32s blockZScanIdx, Ipp32s partAddr, Ipp32s partMode, Ipp32s curPUidx);

    void GetPuMvPredictorInfo(Ipp32s blockZScanIdx, Ipp32s partAddr, Ipp32s partMode,
                              Ipp32s curPUidx, MVPInfo *pInfo, MVPInfo & mergeInfo);

    void GetPartOffsetAndSize(Ipp32s idx, Ipp32s partMode, Ipp32s cuSize, Ipp32s &partX,
                              Ipp32s &partY, Ipp32s &partWidth, Ipp32s &partHeight);

    void GetPartAddr(Ipp32s uPartIdx, Ipp32s partMode, Ipp32s m_NumPartition, Ipp32s &partAddr);

    bool CheckIdenticalMotion(Ipp32u absPartIdx);

    Ipp32s ClipMV(H265MV &rcMv) const; // returns 1 if changed, otherwise 0

    Ipp32u GetSCuAddr();

    Ipp8u GetNumPartInter(Ipp32s absPartIdx);

    Ipp32s GetLastValidPartIdx(Ipp32s absPartIdx);

    Ipp8s GetLastCodedQP(Ipp32u absPartIdx);

    Ipp32u GetQuadtreeTuLog2MinSizeInCu(Ipp32u absPartIdx);

    Ipp32u GetQuadtreeTuLog2MinSizeInCu(Ipp32u absPartIdx, Ipp32s size, Ipp8u partSize,
                                        Ipp8u predMode);

    H265CUData *GetQpMinCuLeft(Ipp32u &uiLPartUnitIdx, Ipp32u uiCurrAbsIdxInLCU,
                               bool bEnforceSliceRestriction = true,
                               bool bEnforceDependentSliceRestriction = true);

    H265CUData *GetQpMinCuAbove(Ipp32u &aPartUnitIdx, Ipp32u currAbsIdxInLCU,
                                bool enforceSliceRestriction = true,
                                bool enforceDependentSliceRestriction = true);

    Ipp8s GetRefQp(Ipp32u uiCurrAbsIdxInLCU);

    Ipp32u GetIntraSizeIdx(Ipp32u absPartIdx);

    void ConvertTransIdx(Ipp32u absPartIdx, Ipp32u trIdx, Ipp32u &lumaTrMode, Ipp32u &chromaTrMode);

    void GetAllowedChromaDir(Ipp32u absPartIdx, Ipp8u *modeList);

    Ipp32s GetIntradirLumaPred(Ipp32u absPartIdx, Ipp32s *intraDirPred, Ipp32s *mode = NULL);

    Ipp32u GetCtxSplitFlag(Ipp32u absPartIdx, Ipp32u depth);

    Ipp32u GetCtxQtCbf(Ipp32u absPartIdx, EnumTextType type, Ipp32u trDepth);

    Ipp32u GetTransformIdx(Ipp32u absPartIdx) { return (Ipp32u)m_data[absPartIdx].trIdx; }

    Ipp32u GetCtxSkipFlag(Ipp32u absPartIdx);

    Ipp32u GetCtxInterDir(Ipp32u absPartIdx);

    Ipp32u GetCoefScanIdx(Ipp32u absPartIdx, Ipp32u width, Ipp32s bIsLuma, Ipp32s bIsIntra);

    template <class H265Bs>
    void CodeCoeffNxN(H265Bs *bs, H265CU *pCU, CoeffsType *coeffs, Ipp32u absPartIdx, Ipp32u width,
                      Ipp32u height, EnumTextType type);

    template <class H265Bs>
    void PutTransform(H265Bs *bs, Ipp32u offsetLuma, Ipp32u offsetChroma, Ipp32u absPartIdx,
                      Ipp32u absTuPartIdx, Ipp32u depth, Ipp32u width, Ipp32u height, Ipp32u trIdx,
                      Ipp8u& codeDqp, Ipp8u splitFlagOnly = 0);

    template <class H265Bs>
    void EncodeSao(H265Bs *bs, Ipp32u absPartIdx, Ipp32s depth, Ipp8u rdMode,
                   SaoCtuParam &saoBlkParam, bool leftMergeAvail, bool aboveMergeAvail);

    template <class H265Bs>
    void EncodeCU(H265Bs *bs, Ipp32u absPartIdx, Ipp32s depth, Ipp8u rdMode = 0);

    template <class H265Bs>
    void EncodeCoeff(H265Bs *bs, Ipp32u absPartIdx, Ipp32u depth, Ipp32u width, Ipp32u height,
                     Ipp8u &codeDqp);

    template <class H265Bs>
    void CodeIntradirLumaAng(H265Bs *bs, Ipp32u absPartIdx, Ipp8u multiple);

    void PredInterUni(Ipp32u PartAddr, Ipp32s Width, Ipp32s Height, EnumRefPicList RefPicList,
                      Ipp32s PartIdx, PixType *dst, Ipp32s dst_pitch, bool bi, Ipp8u isLuma,
                      MFX_HEVC_PP::EnumAddAverageType eAddAverage);

    void InterPredCu(Ipp32s absPartIdx, Ipp8u depth, PixType *dst, Ipp32s pitchDst, Ipp8u isLuma);

    void IntraPred(Ipp32u absPartIdx, Ipp8u depth);

    void IntraPredTu(Ipp32s absPartIdx, Ipp32s width, Ipp32s predMode, Ipp8u isLuma);

    void IntraLumaModeDecision(Ipp32s absPartIdx, Ipp32u offset, Ipp8u depth, Ipp8u trDepth);

    void IntraLumaModeDecisionRDO(Ipp32s absPartIdx, Ipp32u offset, Ipp8u depth, Ipp8u trDepth,
                                  CABAC_CONTEXT_H265 * initCtx);

    Ipp8u GetTrSplitMode(Ipp32s absPartIdx, Ipp8u depth, Ipp8u trDepth, Ipp8u partSize,
                         Ipp8u isLuma, Ipp8u strict = 1);

    void TransformInv(Ipp32s offset, Ipp32s width, Ipp8u isLuma, Ipp8u isIntra);

    void TransformInv2(PixType * dst, Ipp32s pitch_dst, Ipp32s offset, Ipp32s width, Ipp8u isLuma,
                       Ipp8u isIntra);

    void TransformFwd(Ipp32s offset, Ipp32s width, Ipp8u isLuma, Ipp8u isIntra);

    void GetInitAvailablity();

    void EncAndRecLuma(Ipp32u absPartIdx, Ipp32s offset, Ipp8u depth, Ipp8u *nz, CostType *cost);

    void EncAndRecChroma(Ipp32u absPartIdx, Ipp32s offset, Ipp8u depth, Ipp8u *nz, CostType *cost);

    void EncAndRecLumaTu(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp8u *nz, CostType *cost,
                         Ipp8u cost_pred_flag, IntraPredOpt intra_pred_opt);

    void EncAndRecChromaTu(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp8u *nz,
                           CostType *cost);

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

    CostType CalcCostSkip(Ipp32u absPartIdx, Ipp8u depth);

    CostType MeCu(Ipp32u absPartIdx, Ipp8u depth, Ipp32s offset);

    void MePu(H265MEInfo *meInfo);

    Ipp32s GetMvPredictors(H265MV *mvPred, const H265MEInfo* meInfo, const MVPInfo *predInfo,
                           const MVPInfo *mergeInfo, H265MV mvLast, Ipp32s meDir,
                           Ipp32s refIdx) const;

    void MeSubpel(H265MV mvIntpel, Ipp32s costIntpel, const H265MEInfo *meInfo,
                  const MVPInfo *predInfo, const MVPInfo *mergeInfo, Ipp32s meDir,
                  Ipp32s refIdx, H265MV *mvSubpel, Ipp32s *costSubpel) const;

    CostType CuCost(Ipp32u absPartIdx, Ipp8u depth, const H265MEInfo* bestInfo, Ipp32s offset,
                    Ipp32s fastPuDecision);

    void TuGetSplitInter(Ipp32u absPartIdx, Ipp32s offset, Ipp8u trIdx, Ipp8u trIdxMax, Ipp8u *nz,
                         CostType *cost, Ipp8u *cbf);

    void DetailsXY(H265MEInfo *meInfo) const;

    void MeInterpolateOld(H265MEInfo *meInfo, H265MV *MV, PixType *in_pSrc, Ipp32s in_SrcPitch,
                          Ipp16s *buf, Ipp32s buf_pitch) const;

    void MeInterpolate(const H265MEInfo* meInfo, H265MV* MV, PixType *in_pSrc, Ipp32s in_SrcPitch,
                       Ipp8u *buf, Ipp32s buf_pitch) const;

    Ipp32s MatchingMetricPu(PixType *src, const H265MEInfo* meInfo, H265MV* mv, H265Frame *refPic,
                            Ipp32s useHadamard) const;

    Ipp32s MatchingMetricBipredPu(PixType *src, H265MEInfo* meInfo, PixType *fwd, Ipp32u pitchFwd,
                                  PixType *bwd, Ipp32u pitchBwd, H265MV fullMV[2],
                                  Ipp32s useHadamard);

    Ipp32s MvCost(H265MV MV[2], T_RefIdx ref_idx[2], MVPInfo pInfo[2], MVPInfo *mergeInfo) const;

    Ipp32s MvCost1Ref(H265MV *mv, Ipp8s refIdx, const MVPInfo *predInfo, const MVPInfo *mergeInfo,
                      Ipp32s rlist) const;

    void InitCu(H265VideoParam *_par, H265CUData *_data, H265CUData *_dataTemp, Ipp32s cuAddr,
                PixType *_y, PixType *_uv, Ipp32s _pitch, PixType *_ySrc, PixType *uvSrc,
                Ipp32s _pitchSrc, H265BsFake *_bsf, H265Slice *cslice, Ipp32s initializeDataFlag);

    void ModeDecision(Ipp32u absPartIdx, Ipp32u offset, Ipp8u depth, CostType *cost);

private:
    Ipp32s m_isRdoq;
    // random generation code, for development purposes
    //void FillRandom(Ipp32u absPartIdx, Ipp8u depth);
    //void FillZero(Ipp32u absPartIdx, Ipp8u depth);
};

template <class H265Bs>
void CodeSaoCtbOffsetParam(H265Bs *bs, int compIdx, SaoOffsetParam& ctbParam, bool sliceEnabled);

template <class H265Bs>
void CodeSaoCtbParam(H265Bs *bs, SaoCtuParam &saoBlkParam, bool *sliceEnabled, bool leftMergeAvail,
                     bool aboveMergeAvail, bool onlyEstMergeInfo);

Ipp32s GetLumaOffset(H265VideoParam *par, Ipp32s absPartIdx, Ipp32s pitch);

Ipp32s tuHad(const PixType *src, Ipp32s pitch_src, const PixType *rec, Ipp32s pitch_rec,
             Ipp32s width, Ipp32s height);

} // namespace

#endif // __MFX_H265_CTB_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
