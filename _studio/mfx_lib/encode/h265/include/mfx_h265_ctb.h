//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2013 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_CTB_H__
#define __MFX_H265_CTB_H__

struct H265CUData
{
public:
    Ipp8u depth;
    Ipp8u size;
    Ipp8u part_size;
    Ipp8u pred_mode;
    Ipp8u tr_idx;
    Ipp8s qp;
    Ipp8u cbf[3];
    Ipp8u intra_luma_dir;
    Ipp8u intra_chroma_dir;
    Ipp8u inter_dir;
    Ipp8u merge_idx;
    Ipp8s mvp_idx[2];
    Ipp8s mvp_num[2];
    H265MV mv[2];
    H265MV mvd[2];
    T_RefIdx ref_idx[2];

    Ipp8u transform_skip_flag[3];
    union {
        struct {
            Ipp8u merge_flag : 1;
            Ipp8u ipcm_flag : 1;
            Ipp8u transquant_bypass_flag : 1;
            Ipp8u skipped_flag : 1;
        } flags;
        Ipp8u _flags;
    };
};

struct H265CUPtr {
    H265CUData *ctb_data_ptr;
    Ipp32s ctb_addr;
    Ipp32u abs_part_idx;
};
#define MAX_PU_IN_CTB (64/8*2 * 64/8*2)
#define MAX_PU_CASES_IN_CTB (MAX_PU_IN_CTB*4/3+2)
struct H265MEInfo
{
    Ipp8u posx, posy; // in pix inside LCU
    Ipp8u width, height;
    H265MV MV[2]; // [fwd/bwd]
    Ipp32s cost_1dir[2];
    Ipp32s cost_bidir;
    CostType cost_intra;
    Ipp32s cost_inter;
    Ipp32s me_parts[4]; // used if split
    Ipp32s details[2]; // [vert/horz]

    Ipp32u abs_part_idx;
    Ipp8u inter_dir;   // INTER_DIR_PRED_LX
    Ipp8u split_mode;
    Ipp8u depth;
    Ipp8u must_split; // part of CU is out of the frame
    Ipp8u excluded;   // completely out of the frame
};

#define IS_INTRA(data, part_idx) ((data)[part_idx].pred_mode == MODE_INTRA)

static inline Ipp8u isSkipped (H265CUData *data, Ipp32u part_idx)
{
    return data[part_idx].flags.skipped_flag;
}

#define MAX_TOTAL_DEPTH (MAX_CU_DEPTH+4)

class H265CU
{
public:
    H265VideoParam *par;
    H265CUData *data;
    Ipp32s          slice_id;
    Ipp32u          ctb_addr;           ///< CU address in a slice
    Ipp32u          m_uiAbsIdxInLCU;      ///< absolute address in a CU. It's Z scan order
    Ipp32u          ctb_pelx;           ///< CU position in a pixel (X)
    Ipp32u          ctb_pely;           ///< CU position in a pixel (Y)
    Ipp32u          num_partition;     ///< total number of minimum partitions in a CU
    CoeffsType      tr_coeff_y[MAX_CU_SIZE*MAX_CU_SIZE];
    CoeffsType      tr_coeff_u[MAX_CU_SIZE*MAX_CU_SIZE/4];
    CoeffsType      tr_coeff_v[MAX_CU_SIZE*MAX_CU_SIZE/4];
    CoeffsType      residuals_y[MAX_CU_SIZE*MAX_CU_SIZE];
    CoeffsType      residuals_u[MAX_CU_SIZE*MAX_CU_SIZE/4];
    CoeffsType      residuals_v[MAX_CU_SIZE*MAX_CU_SIZE/4];
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

    Ipp32u bak_abs_part_idxCU;
    Ipp32u bak_abs_part_idx;
    Ipp32u bak_chroma_offset;
    Ipp8u *y_src;
    Ipp8u *uv_src;
    Ipp32s pitch_src;
    Ipp8u *y_rec;
    Ipp8u *u_rec;
    Ipp8u *v_rec;
    Ipp32s pitch_rec_luma;
    Ipp32s pitch_rec_chroma;
    H265CUData *data_save;
    H265CUData *data_best;
    H265CUData *data_temp;
    PixType     rec_luma_save_cu[6][MAX_CU_SIZE*MAX_CU_SIZE];
    PixType     rec_luma_save_tu[6][MAX_CU_SIZE*MAX_CU_SIZE];
    Ipp16s pred_buf_y[2][MAX_CU_SIZE*MAX_CU_SIZE];
    Ipp16s pred_buf_u[2][MAX_CU_SIZE*MAX_CU_SIZE/4];
    Ipp16s pred_buf_v[2][MAX_CU_SIZE*MAX_CU_SIZE/4];

    H265BsFake *bsf;
    Ipp8u rd_opt_flag;
    Ipp64f rd_lambda;
    Ipp64f rd_lambda_inter;

    inline bool  isIntra(Ipp32u part_idx)
    { return data[part_idx].pred_mode == MODE_INTRA; }

    inline Ipp8u get_transform_skip(Ipp32u idx,EnumTextType type)
    { return data[idx].transform_skip_flag[h265_type2idx[type]];}

    inline Ipp8u get_cbf(Ipp32u idx, EnumTextType type, Ipp32u tr_depth )
    { return (Ipp8u)( ( data[idx].cbf[h265_type2idx[type]] >> tr_depth ) & 0x1 ); }

    inline void set_cbf_zero(Ipp32u idx, EnumTextType type, Ipp32u tr_depth )
    {  data[idx].cbf[h265_type2idx[type]] &= ~(1 << tr_depth); }

    inline void set_cbf_one(Ipp32u idx, EnumTextType type, Ipp32u tr_depth )
    {  data[idx].cbf[h265_type2idx[type]] |= (1 << tr_depth); }

    inline Ipp8u get_qt_root_cbf(Ipp32u idx)
    { return get_cbf( idx, TEXT_LUMA, 0 ) || get_cbf( idx, TEXT_CHROMA_U, 0 ) || get_cbf( idx, TEXT_CHROMA_V, 0 ); }

    void getPULeft(H265CUPtr *pCU,
        Ipp32u uiCurrPartUnitIdx,
        bool bEnforceSliceRestriction=true,
        bool bEnforceDependentSliceRestriction=true,
        bool bEnforceTileRestriction=true );

    void getPUAbove(H265CUPtr *pCU,
        Ipp32u uiCurrPartUnitIdx,
        bool bEnforceSliceRestriction=true,
        bool bEnforceDependentSliceRestriction=true,
        bool MotionDataCompresssion = false,
        bool planarAtLCUBoundary = false,
        bool bEnforceTileRestriction=true );

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
        MVPInfo  pInfo[2],
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


    H265CUData*   getQpMinCuLeft              ( Ipp32u&  uiLPartUnitIdx , Ipp32u uiCurrAbsIdxInLCU, bool bEnforceSliceRestriction=true, bool bEnforceDependentSliceRestriction=true );
    H265CUData*   getQpMinCuAbove             ( Ipp32u&  aPartUnitIdx , Ipp32u currAbsIdxInLCU, bool enforceSliceRestriction=true, bool enforceDependentSliceRestriction=true );
    Ipp8s         get_ref_qp                    ( Ipp32u   uiCurrAbsIdxInLCU                       );

    Ipp32u        getIntraSizeIdx                 ( Ipp32u abs_part_idx                                       );
    void          convert_trans_idx                 ( Ipp32u abs_part_idx, Ipp32u tr_idx, Ipp32u& rluma_tr_mode, Ipp32u& rchroma_tr_mode );

    void          get_allowed_chroma_dir             ( Ipp32u abs_part_idx, Ipp8u* mode_list );
    Ipp32s        get_intradir_luma_pred        ( Ipp32u abs_part_idx, Ipp32s* intra_dir_pred, Ipp32s* piMode = NULL );

    Ipp32u        get_ctx_split_flag                 (Ipp32u   abs_part_idx, Ipp32u depth                   );
    Ipp32u        get_ctx_qt_cbf                     (Ipp32u   abs_part_idx, EnumTextType type, Ipp32u tr_depth );

    Ipp32u         get_transform_idx                  (Ipp32u   abs_part_idx) { return (Ipp32u)data[abs_part_idx].tr_idx; }
    Ipp32u        get_ctx_skip_flag                  (Ipp32u   abs_part_idx                                 );
    Ipp32u        getCtxInterDir                  ( Ipp32u   abs_part_idx                                 );

    Ipp32u        get_coef_scan_idx(Ipp32u abs_part_idx, Ipp32u width, bool bIsLuma, bool bIsIpp32sra);

    template <class H265Bs>
    void h265_code_coeff_NxN(H265Bs *bs, H265CU* pCU, CoeffsType* coeffs, Ipp32u abs_part_idx,
        Ipp32u width, Ipp32u height, EnumTextType type);

    template <class H265Bs>
    void put_transform(H265Bs *bs, Ipp32u offset_luma, Ipp32u offset_chroma,
        Ipp32u abs_part_idx, Ipp32u abs_tu_part_idx, Ipp32u depth,
        Ipp32u width, Ipp32u height, Ipp32u tr_idx, Ipp8u& code_dqp,
        Ipp8u split_flag_only = 0);
    template <class H265Bs>
    void xEncodeCU(H265Bs *bs, Ipp32u abs_part_idx, Ipp32s depth, Ipp8u rd_mode = 0 );
    template <class H265Bs>
    void encode_coeff(H265Bs *bs, Ipp32u abs_part_idx, Ipp32u depth, Ipp32u width, Ipp32u height, Ipp8u &code_dqp );
    template <class H265Bs>
    void h265_code_intradir_luma_ang(H265Bs *bs, Ipp32u abs_part_idx, Ipp8u multiple);

    void CopySaturate(Ipp32u PartAddr, Ipp32s Width, Ipp32s Height, EnumRefPicList RefPicList, Ipp8u is_luma);
    void AddAverage(Ipp32u PartAddr, Ipp32s Width, Ipp32s Height, Ipp8u is_luma);

    void PredInterUni(Ipp32u PartAddr, Ipp32s Width, Ipp32s Height, EnumRefPicList RefPicList,
                                  Ipp32s PartIdx, bool bi, Ipp8u is_luma);

    void InterPredCU(Ipp32s abs_part_idx, Ipp8u depth, Ipp8u is_luma);
    void IntraPred(Ipp32u abs_part_idx, Ipp8u depth);
    void IntraPredTU(Ipp32s blockZScanIdx, Ipp32s width, Ipp32s pred_mode, Ipp8u is_luma);
    void TransformInv(Ipp32s offset, Ipp32s width, Ipp8u is_luma, Ipp8u is_intra);
    void TransformFwd(Ipp32s offset, Ipp32s width, Ipp8u is_luma, Ipp8u is_intra);

    void GetInitAvailablity();

    void EncAndRecLuma(Ipp32u abs_part_idx, Ipp32s offset, Ipp8u depth, Ipp8u *nz);
    void EncAndRecChroma(Ipp32u abs_part_idx, Ipp32s offset, Ipp8u depth, Ipp8u *nz, CostType *cost);
    void EncAndRecLumaTU(Ipp32u abs_part_idx, Ipp32s offset, Ipp32s width, Ipp8u *nz, CostType *cost,
        Ipp8u cost_pred_flag = 0, Ipp8u skip_pred_flag = 0);
    void EncAndRecChromaTU(Ipp32u abs_part_idx, Ipp32s offset, Ipp32s width, Ipp8u *nz, CostType *cost);
    void QuantInvTU(Ipp32u abs_part_idx, Ipp32s offset, Ipp32s width, Ipp32s is_luma);
    void QuantFwdTU(Ipp32u abs_part_idx, Ipp32s offset, Ipp32s width, Ipp32s is_luma);

    void Deblock();
    void FilterEdgeLuma(H265EdgeData *edge,
                            PixType *srcDst,
                            Ipp32s srcDstStride,
                            Ipp32s dir);
    void FilterEdgeChroma(H265EdgeData *edge,
                              PixType *srcDst,
                              Ipp32s srcDstStride,
                              Ipp32s chromaQpOffset,
                              Ipp32s dir);
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

    void FillSubPart(Ipp32s abs_part_idx, Ipp8u depth_cu, Ipp8u tr_idx,
        Ipp8u part_size, Ipp8u luma_dir, Ipp8u qp);
    void CopySubPartTo(H265CUData *data_copy, Ipp32s abs_part_idx, Ipp8u depth_cu, Ipp8u tr_depth);
    void CalcCostLuma(Ipp32u abs_part_idx, Ipp32s offset, Ipp8u depth,
                          Ipp8u tr_depth, CostOpt cost_opt,
                          Ipp8u part_size, Ipp8u luma_dir, CostType *cost);

    CostType ME_CU(Ipp32u abs_part_idx, Ipp8u depth);
    void ME_PU(H265MEInfo* me_info);
    CostType CU_cost(Ipp32u abs_part_idx, Ipp8u depth, const H265MEInfo* best_info);
    void TU_GetSplitInter(Ipp32u abs_part_idx, Ipp32s offset, Ipp8u tr_idx, Ipp8u tr_idx_max, Ipp8u *nz, CostType *cost);
    void DetailsXY(H265MEInfo* me_info) const;
    void ME_Interpolate_old(H265MEInfo* me_info, H265MV* MV, PixType *in_pSrc, Ipp32s in_SrcPitch, Ipp16s *buf, Ipp32s buf_pitch) const;
    void ME_Interpolate(H265MEInfo* me_info, H265MV* MV, PixType *in_pSrc, Ipp32s in_SrcPitch, Ipp8u *buf, Ipp32s buf_pitch) const;
    void ME_Interpolate_new_need_debug(H265MEInfo* me_info, H265MV* MV1, PixType *in_pSrc1, Ipp32s in_SrcPitch1, H265MV* MV2, PixType *in_pSrc2, Ipp32s in_SrcPitch2, Ipp8u *buf, Ipp32s buf_pitch) const;
    Ipp32s MatchingMetric_PU(H265MEInfo* me_info, H265MV* MV, H265Frame *PicYUVRef) const;
    Ipp32s MatchingMetricBipred_PU(H265MEInfo* me_info, PixType *y_fwd, Ipp32u pitch_fwd, PixType *y_bwd, Ipp32u pitch_bwd, H265MV MV[2]) const;
    Ipp32s MVCost( H265MV MV[2], T_RefIdx ref_idx[2], MVPInfo pInfo[2], MVPInfo& mergeInfo) const;

    void InitCU(H265VideoParam *_par, H265CUData *_data, H265CUData *_data_temp, Ipp32s iCUAddr,
        PixType *_y, PixType *_u, PixType *_v, Ipp32s _pitch_luma, Ipp32s _pitch_chroma,
        PixType *_y_src, PixType *uv_src, Ipp32s _pitch_src, H265BsFake *_bsf);
    void FillRandom(Ipp32u abs_part_idx, Ipp8u depth);
    void ModeDecision(Ipp32u abs_part_idx, Ipp32u offset, Ipp8u depth, CostType *cost);

    void SetRDOQ(bool mode ) { m_isRDOQ = mode; }
    bool IsRDOQ() const { return m_isRDOQ; }

    private:
        bool m_isRDOQ;
};


typedef IppStatus (__STDCALL *SADfunc8u)(
    const Ipp8u* pSrcCur,
    Ipp32s srcCurStep,
    const Ipp8u* pSrcRef,
    Ipp32s srcRefStep,
    Ipp32s* pDst,
    Ipp32s mcType);

extern SADfunc8u* SAD_8u[17];

#endif // __MFX_H265_CTB_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
