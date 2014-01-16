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
    T_RefIdx ref_idx[2];
    Ipp32s cost_1dir[2];
    Ipp32s cost_bidir;
    CostType cost_intra;
    Ipp32s cost_inter;
    Ipp32s me_parts[4]; // used if split
    Ipp32s details[2]; // [vert/horz]

    Ipp32u absPartIdx;
    Ipp8u inter_dir;   // INTER_DIR_PRED_LX
    Ipp8u splitMode;
    Ipp8u depth;
    Ipp8u must_split; // part of CU is out of the frame
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
    H265VideoParam *par;
    H265CUData *data;
    Ipp32u          ctb_addr;           ///< CU address in a slice
    Ipp32u          m_uiAbsIdxInLCU;      ///< absolute address in a CU. It's Z scan order
    Ipp32u          ctb_pelx;           ///< CU position in a pixel (X)
    Ipp32u          ctb_pely;           ///< CU position in a pixel (Y)
    Ipp32u          num_partition;     ///< total number of minimum partitions in a CU
    __ALIGN32 CoeffsType    tr_coeff_y[MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType    tr_coeff_u[MAX_CU_SIZE * MAX_CU_SIZE / 4];
    __ALIGN32 CoeffsType    tr_coeff_v[MAX_CU_SIZE * MAX_CU_SIZE / 4];
    __ALIGN32 CoeffsType    residuals_y[MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType    residuals_u[MAX_CU_SIZE * MAX_CU_SIZE / 4];
    __ALIGN32 CoeffsType    residuals_v[MAX_CU_SIZE * MAX_CU_SIZE / 4];
    __ALIGN32 PixType       pred_intra_all[35*32*32];
    __ALIGN32 PixType       tu_src_transposed[32*32];
    __ALIGN32 PixType       inter_pred[4][MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 PixType       inter_pred_best[4][MAX_CU_SIZE * MAX_CU_SIZE];
    PixType                 (*inter_pred_ptr)[MAX_CU_SIZE * MAX_CU_SIZE];

    CostType intra_best_costs[35];
    Ipp8u    intra_best_modes[35];
    Ipp64f   intra_mode_bitcost[35];

    Ipp32s pred_intra_all_width;
    Ipp8u           inNeighborFlags[4*MAX_CU_SIZE+1];
    Ipp8u           outNeighborFlags[4*MAX_CU_SIZE+1];
    H265CUData*   p_above;          ///< pointer of above CU
    H265CUData*   p_left;           ///< pointer of left CU
    H265CUData*   p_above_left;      ///< pointer of above-left CU
    H265CUData*   p_above_right;     ///< pointer of above-right CU
    H265CUData*   m_colocatedLCU[2];
    Ipp32s above_addr;
    Ipp32s left_addr;
    Ipp32s above_left_addr;
    Ipp32s above_right_addr;
    Ipp8s *m_ColFrmRefFramesTbList[2];
    bool  *m_ColFrmRefIsLongTerm[2];
    H265EdgeData m_edge[9][9][4];

    Ipp32u bakAbsPartIdxCu;
    Ipp32u bakAbsPartIdx;
    Ipp32u bakChromaOffset;

    // aya - may be used late to speed up SAD calculation
    //__ALIGN32 Ipp8u m_src_aligned_block[MAX_CU_SIZE*MAX_CU_SIZE];

    Ipp8u *y_src;
    Ipp8u *uv_src;
    Ipp32s pitch_src;
    Ipp8u *y_rec;
    Ipp8u *uv_rec;
    Ipp32s pitch_rec;
    H265CUData *data_save;
    H265CUData *data_best;
    H265CUData *data_temp;
    H265CUData *data_temp2;
    PixType     rec_luma_save_cu[6][MAX_CU_SIZE*MAX_CU_SIZE];
    PixType     rec_luma_save_tu[6][MAX_CU_SIZE*MAX_CU_SIZE];
    Ipp16s pred_buf_y[2][MAX_CU_SIZE*MAX_CU_SIZE];
    Ipp16s pred_buf_uv[2][MAX_CU_SIZE*MAX_CU_SIZE/2];

    // storing predictions for bidir
#define INTERP_BUF_SZ 8 // must be power of 2
    struct interp_store {
        __ALIGN32 Ipp16s predBufY[MAX_CU_SIZE*MAX_CU_SIZE];
        PixType * pY;
        H265MV MV;
    } interp_buf[INTERP_BUF_SZ];
    Ipp8u interp_idx_first, interp_idx_last;

    H265BsFake *bsf;
    Ipp8u rd_opt_flag;
    Ipp64f rd_lambda;
    Ipp64f rd_lambda_inter;
    Ipp64f rd_lambda_inter_mv;
    H265Slice *cslice;
    Ipp8u depth_min;
    SaoEncodeFilter m_saoEncodeFilter;

    inline bool  isIntra(Ipp32u partIdx)
    { return data[partIdx].predMode == MODE_INTRA; }

    inline Ipp8u getTransformSkip(Ipp32u idx,EnumTextType type)
    { return data[idx].transformSkipFlag[h265_type2idx[type]];}

    inline Ipp8u getCbf(Ipp32u idx, EnumTextType type, Ipp32u tr_depth )
    { return (Ipp8u)( ( data[idx].cbf[h265_type2idx[type]] >> tr_depth ) & 0x1 ); }

    inline void setCbfZero(Ipp32u idx, EnumTextType type, Ipp32u tr_depth )
    {  data[idx].cbf[h265_type2idx[type]] &= ~(1 << tr_depth); }

    inline void setCbfOne(Ipp32u idx, EnumTextType type, Ipp32u tr_depth )
    {  data[idx].cbf[h265_type2idx[type]] |= (1 << tr_depth); }

    inline Ipp8u getQtRootCbf(Ipp32u idx)
    { return getCbf( idx, TEXT_LUMA, 0 ) || getCbf( idx, TEXT_CHROMA_U, 0 ) || getCbf( idx, TEXT_CHROMA_V, 0 ); }

    void getPuLeft(H265CUPtr *cu,
                   Ipp32u currPartUnitIdx,
                   Ipp32s enforceSliceRestriction=true,
                   Ipp32s enforceDependentSliceRestriction=true,
                   Ipp32s enforceTileRestriction=true );

    void getPuAbove(H265CUPtr *cu,
                    Ipp32u currPartUnitIdx,
                    Ipp32s enforceSliceRestriction=true,
                    Ipp32s enforceDependentSliceRestriction=true,
                    Ipp32s motionDataCompresssion = false,
                    Ipp32s planarAtLcuBoundary = false,
                    Ipp32s enforceTileRestriction=true );

    bool GetColMVP(H265CUData* colLCU,
        Ipp32s blockZScanIdx,
        Ipp32s refPicListIdx,
        Ipp32s refIdx,
        H265MV& rcMv);
    H265CUData* GetNeighbour(Ipp32s& neighbourBlockZScanIdx,
        Ipp32s  neighbourBlockColumn,
        Ipp32s  neighbourBlockRow,
        Ipp32s  curBlockZScanIdx,
        bool isNeedTocheckCurLCU);
    bool AddMVPCand(MVPInfo* pInfo,
        H265CUData *data,
        Ipp32s blockZScanIdx,
        Ipp32s refPicListIdx,
        Ipp32s refIdx,
        bool order);
    void GetMvpCand(Ipp32s topLeftCUBlockZScanIdx,
        Ipp32s refPicListIdx,
        Ipp32s refIdx,
        Ipp32s partMode,
        Ipp32s partIdx,
        Ipp32s cuSize,
        MVPInfo* pInfo);
    void GetInterMergeCandidates(Ipp32s topLeftCUBlockZScanIdx,
        Ipp32s partMode,
        Ipp32s partIdx,
        Ipp32s cuSize,
        MVPInfo* pInfo);
    void GetPUMVInfo(Ipp32s blockZScanIdx,
        Ipp32s partAddr,
        Ipp32s partMode,
        Ipp32s curPUidx);
    void GetPUMVPredictorInfo(Ipp32s blockZScanIdx,
        Ipp32s partAddr,
        Ipp32s partMode,
        Ipp32s curPUidx,
        MVPInfo  *pInfo,
        MVPInfo& mergeInfo);
    void GetPartOffsetAndSize(Ipp32s idx,
        Ipp32s partMode,
        Ipp32s cuSize,
        Ipp32s& partX,
        Ipp32s& partY,
        Ipp32s& partWidth,
        Ipp32s& partHeight);
    void GetPartAddr(Ipp32s uPartIdx,
        Ipp32s partMode,
        Ipp32s m_NumPartition,
        Ipp32s& partAddr);

    bool          CheckIdenticalMotion(Ipp32u abs_part_idx);
    void          clipMV(H265MV& rcMv);
    Ipp32u        getSCUAddr            ();
    Ipp8u         getNumPartInter(Ipp32s abs_part_idx);

    Ipp32s        getLastValidPartIdx   ( Ipp32s abs_part_idx );
    Ipp8s         getLastCodedQP        ( Ipp32u abs_part_idx );

    Ipp32u        getQuadtreeTULog2MinSizeInCU(Ipp32u abs_part_idx );
    Ipp32u        getQuadtreeTULog2MinSizeInCU(Ipp32u abs_part_idx,
        Ipp32s size, Ipp8u partSize, Ipp8u pred_mode);


    H265CUData*   getQpMinCuLeft(Ipp32u&  uiLPartUnitIdx, Ipp32u uiCurrAbsIdxInLCU, bool bEnforceSliceRestriction=true, bool bEnforceDependentSliceRestriction=true);
    H265CUData*   getQpMinCuAbove(Ipp32u&  aPartUnitIdx, Ipp32u currAbsIdxInLCU, bool enforceSliceRestriction=true, bool enforceDependentSliceRestriction=true);
    Ipp8s         getRefQp(Ipp32u   uiCurrAbsIdxInLCU);

    Ipp32u        getIntraSizeIdx(Ipp32u absPartIdx);
    void          convertTransIdx(Ipp32u absPartIdx, Ipp32u tr_idx, Ipp32u& rluma_tr_mode, Ipp32u& rchroma_tr_mode);

    void          getAllowedChromaDir(Ipp32u absPartIdx, Ipp8u* mode_list);
    Ipp32s        getIntradirLumaPred(Ipp32u absPartIdx, Ipp32s* intra_dir_pred, Ipp32s* piMode = NULL);

    Ipp32u        getCtxSplitFlag(Ipp32u absPartIdx, Ipp32u depth);
    Ipp32u        getCtxQtCbf(Ipp32u absPartIdx, EnumTextType type, Ipp32u tr_depth);

    Ipp32u        getTransformIdx(Ipp32u absPartIdx) { return (Ipp32u)data[absPartIdx].trIdx; }
    Ipp32u        getCtxSkipFlag(Ipp32u absPartIdx);
    Ipp32u        getCtxInterDir(Ipp32u absPartIdx);

    Ipp32u        getCoefScanIdx(Ipp32u absPartIdx, Ipp32u width, Ipp32s bIsLuma, Ipp32s bIsIntra);

    template <class H265Bs>
    void h265_code_coeff_NxN(H265Bs *bs, H265CU* pCU, CoeffsType* coeffs, Ipp32u abs_part_idx,
        Ipp32u width, Ipp32u height, EnumTextType type);

    template <class H265Bs>
    void put_transform(H265Bs *bs, Ipp32u offset_luma, Ipp32u offset_chroma,
        Ipp32u abs_part_idx, Ipp32u abs_tu_part_idx, Ipp32u depth,
        Ipp32u width, Ipp32u height, Ipp32u tr_idx, Ipp8u& code_dqp,
        Ipp8u split_flag_only = 0);
    template <class H265Bs>
    void xEncodeSAO(
        H265Bs *bs,
        Ipp32u abs_part_idx,
        Ipp32s depth,
        Ipp8u rd_mode,
        SaoCtuParam& saoBlkParam,
        bool leftMergeAvail,
        bool aboveMergeAvail );

    template <class H265Bs>
    void xEncodeCU(H265Bs *bs, Ipp32u abs_part_idx, Ipp32s depth, Ipp8u rd_mode = 0 );

    template <class H265Bs>
    void encode_coeff(H265Bs *bs, Ipp32u abs_part_idx, Ipp32u depth, Ipp32u width, Ipp32u height, Ipp8u &code_dqp, Ipp8u split_flag_only = 0 );

    template <class H265Bs>
    void h265_code_intradir_luma_ang(H265Bs *bs, Ipp32u abs_part_idx, Ipp8u multiple);

    void PredInterUni(Ipp32u PartAddr, Ipp32s Width, Ipp32s Height, EnumRefPicList RefPicList,
                      Ipp32s PartIdx, PixType *dst, Ipp32s dst_pitch, bool bi, Ipp8u is_luma,
                      MFX_HEVC_PP::EnumAddAverageType eAddAverage);

    void InterPredCU(Ipp32s abs_part_idx, Ipp8u depth, PixType *dst, Ipp32s dst_pitch, Ipp8u is_luma);
    void IntraPred(Ipp32u abs_part_idx, Ipp8u depth);
    void IntraPredTU(Ipp32s abs_part_idx, Ipp32s width, Ipp32s pred_mode, Ipp8u is_luma);
    void IntraLumaModeDecision(Ipp32s abs_part_idx, Ipp32u offset, Ipp8u depth, Ipp8u tr_depth);
    void IntraLumaModeDecisionRDO(Ipp32s abs_part_idx, Ipp32u offset, Ipp8u depth, Ipp8u tr_depth, CABAC_CONTEXT_H265 * initCtx);
    Ipp8u GetTRSplitMode(Ipp32s abs_part_idx, Ipp8u depth, Ipp8u tr_depth, Ipp8u part_size, Ipp8u is_luma, Ipp8u strict = 1);
    void TransformInv(Ipp32s offset, Ipp32s width, Ipp8u is_luma, Ipp8u is_intra);
    void TransformInv2(PixType * dst, Ipp32s pitch_dst, Ipp32s offset, Ipp32s width, Ipp8u is_luma, Ipp8u is_intra);
    void TransformFwd(Ipp32s offset, Ipp32s width, Ipp8u is_luma, Ipp8u is_intra);

    void GetInitAvailablity();

    void EncAndRecLuma(Ipp32u abs_part_idx, Ipp32s offset, Ipp8u depth, Ipp8u *nz, CostType *cost);
    void EncAndRecChroma(Ipp32u abs_part_idx, Ipp32s offset, Ipp8u depth, Ipp8u *nz, CostType *cost);
    void EncAndRecLumaTU(Ipp32u abs_part_idx, Ipp32s offset, Ipp32s width, Ipp8u *nz, CostType *cost,
                         Ipp8u cost_pred_flag, IntraPredOpt intra_pred_opt);
    void EncAndRecChromaTU(Ipp32u abs_part_idx, Ipp32s offset, Ipp32s width, Ipp8u *nz, CostType *cost);
    void QuantInvTU(Ipp32u abs_part_idx, Ipp32s offset, Ipp32s width, Ipp32s is_luma);
    void QuantFwdTU(Ipp32u abs_part_idx, Ipp32s offset, Ipp32s width, Ipp32s is_luma);

    void Deblock();

    void DeblockOneCrossLuma(Ipp32s curPixelColumn,
                             Ipp32s curPixelRow);
    void DeblockOneCrossChroma(Ipp32s curPixelColumn,
                               Ipp32s curPixelRow);
    void SetEdges(Ipp32s width,
        Ipp32s height);
    void GetEdgeStrength(H265CUPtr *pcCUQ,
        H265EdgeData *edge,
        Ipp32s curPixelColumn,
        Ipp32s curPixelRow,
        Ipp32s crossSliceBoundaryFlag,
        Ipp32s crossTileBoundaryFlag,
        Ipp32s tcOffset,
        Ipp32s betaOffset,
        Ipp32s dir);

    // SAO
    void EstimateCtuSao(
        H265BsFake *bs,
        SaoCtuParam* saoParam,
        SaoCtuParam* saoParam_TotalFrame,
        const MFX_HEVC_PP::CTBBorders & borders,
        const Ipp8u* slice_ids);

    void GetStatisticsCtuSao_Predeblocked( const MFX_HEVC_PP::CTBBorders & borders );
    void FillSubPart(Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trIdx,
                     Ipp8u partSize, Ipp8u lumaDir, Ipp8u qp);
    void FillSubPartIntraLumaDir(Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth, Ipp8u lumaDir);
    void CopySubPartTo(H265CUData *dataCopy, Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth);
    void CalcCostLuma(Ipp32u absPartIdx, Ipp32s offset, Ipp8u depth,
                      Ipp8u trDepth, CostOpt costOpt,
                      Ipp8u partSize, Ipp8u lumaDir, CostType *cost);
    CostType CalcCostSkip(Ipp32u absPartIdx, Ipp8u depth);

    CostType MeCu(Ipp32u absPartIdx, Ipp8u depth, Ipp32s offset);
    void MePu(H265MEInfo* me_info);
    CostType CuCost(Ipp32u absPartIdx, Ipp8u depth, const H265MEInfo* bestInfo, Ipp32s offset, Ipp32s fastPUDecision);
    void TuGetSplitInter(Ipp32u absPartIdx, Ipp32s offset, Ipp8u trIdx, Ipp8u trIdxMax, Ipp8u *nz, CostType *cost);
    void DetailsXY(H265MEInfo* me_info) const;
    void ME_Interpolate_old(H265MEInfo* me_info, H265MV* MV, PixType *in_pSrc, Ipp32s in_SrcPitch, Ipp16s *buf, Ipp32s buf_pitch) const;
    void ME_Interpolate(H265MEInfo* me_info, H265MV* MV, PixType *in_pSrc, Ipp32s in_SrcPitch, Ipp8u *buf, Ipp32s buf_pitch) const;
    void ME_Interpolate_new_need_debug(H265MEInfo* me_info, H265MV* MV1, PixType *in_pSrc1, Ipp32s in_SrcPitch1, H265MV* MV2, PixType *in_pSrc2, Ipp32s in_SrcPitch2, Ipp8u *buf, Ipp32s buf_pitch) const;
    Ipp32s MatchingMetric_PU(PixType *pSrc, H265MEInfo* me_info, H265MV* MV, H265Frame *PicYUVRef) const;
    Ipp32s MatchingMetricBipred_PU(PixType *pSrc, H265MEInfo* meInfo, PixType *yFwd, Ipp32u pitchFwd, PixType *yBwd, Ipp32u pitchBwd, H265MV fullMV[2]);
    Ipp32s MVCost( H265MV MV[2], T_RefIdx ref_idx[2], MVPInfo pInfo[2], MVPInfo& mergeInfo) const;

    void InitCu(H265VideoParam *_par, H265CUData *_data, H265CUData *_dataTemp, Ipp32s cuAddr,
        PixType *_y, PixType *_uv, Ipp32s _pitch,
        PixType *_ySrc, PixType *uvSrc, Ipp32s _pitchSrc, H265BsFake *_bsf, H265Slice *cslice, Ipp32s initializeDataFlag);
    void FillRandom(Ipp32u abs_part_idx, Ipp8u depth);
    void FillZero(Ipp32u abs_part_idx, Ipp8u depth);
    void ModeDecision(Ipp32u abs_part_idx, Ipp32u offset, Ipp8u depth, CostType *cost);

    void SetRDOQ(bool mode ) { m_isRDOQ = mode; }
    bool IsRDOQ() const { return m_isRDOQ; }

    private:
        bool m_isRDOQ;
};

template <class H265Bs>
void h265_code_sao_ctb_offset_param(
    H265Bs *bs,
    int compIdx,
    SaoOffsetParam& ctbParam,
    bool sliceEnabled);

template <class H265Bs>
void h265_code_sao_ctb_param(
    H265Bs *bs,
    SaoCtuParam& saoBlkParam,
    bool* sliceEnabled,
    bool leftMergeAvail,
    bool aboveMergeAvail,
    bool onlyEstMergeInfo);

Ipp32s GetLumaOffset(H265VideoParam *par, Ipp32s abs_part_idx, Ipp32s pitch);

Ipp32s tuHad(const PixType *src, Ipp32s pitch_src,
             const PixType *rec, Ipp32s pitch_rec,
             Ipp32s width, Ipp32s height);

} // namespace

#endif // __MFX_H265_CTB_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
