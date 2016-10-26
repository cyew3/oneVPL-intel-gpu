//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_quant.h"
#include "mfx_h265_enc.h"

namespace H265Enc {

template <class H265Bs>
static
void h265_write_unary_max_symbol(H265Bs *bs, Ipp32u symbol,
                          CABAC_CONTEXT_H265 *ctx, Ipp32s offset, Ipp32u max_symbol )
{
    if (!max_symbol) {
        return;
    }

    bs->EncodeSingleBin_CABAC(ctx, symbol ? 1 : 0);

    if (!symbol) {
        return;
    }

    Ipp8u code_last = max_symbol > symbol;

    while (--symbol)
    {
        bs->EncodeSingleBin_CABAC(ctx + offset, 1);
    }
    if (code_last)
    {
        bs->EncodeSingleBin_CABAC(ctx + offset, 0);
    }

    return;
}

template <class H265Bs>
static
void h265_write_ep_ex_golomb(H265Bs *bs,  Ipp32u symbol, Ipp32u count )
{
    Ipp32u bins = 0;
    Ipp32s num_bins = 0;

    while (symbol >= (Ipp32u)(1<<count))
    {
        bins = 2 * bins + 1;
        num_bins++;
        symbol -= 1 << count;
        count  ++;
    }
    bins = 2 * bins + 0;
    num_bins++;

    bins = (bins << count) | symbol;
    num_bins += count;

    assert( num_bins <= 32 );
    bs->EncodeBinsEP_CABAC(bins, num_bins);
}

template <class H265Bs>
static
void h265_write_coef_remain_ex_golomb (H265Bs *bs, Ipp32u symbol, Ipp32u &param )
{
    Ipp32s code_number  = (Ipp32s)symbol;
    Ipp32u length;
    if (code_number < (COEF_REMAIN_BIN << param))
    {
        length = code_number>>param;
        bs->EncodeBinsEP_CABAC( (1<<(length+1))-2 , length+1);
        bs->EncodeBinsEP_CABAC((code_number&((1<<param)-1)), param);
    }
    else
    {
        length = param;
        code_number  = code_number - (COEF_REMAIN_BIN << param);
        while (code_number >= (1<<length))
        {
            code_number -=  (1<<(length++));
        }
        bs->EncodeBinsEP_CABAC((1<<(COEF_REMAIN_BIN+length+1-param))-2,COEF_REMAIN_BIN+length+1-param);
        bs->EncodeBinsEP_CABAC(code_number,length);
    }
}

template <typename PixType, class H265Bs>
static
void h265_code_mvp_idx (H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx, EnumRefPicList ref_list )
{
    Ipp32s symbol = ref_list ? pCU->m_data[abs_part_idx].flags.mvpIdx1 : pCU->m_data[abs_part_idx].flags.mvpIdx0;
    Ipp32s num = MAX_NUM_AMVP_CANDS;

    h265_write_unary_max_symbol(bs, symbol, CTX(bs,MVP_IDX_HEVC), 1, num-1);
}

template <typename PixType, class H265Bs>
static
void h265_code_part_size(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx, Ipp32s depth )
{
    H265VideoParam *par = pCU->m_par;
    Ipp8u size = pCU->m_data[abs_part_idx].partSize;
    if (pCU->m_data[abs_part_idx].predMode == MODE_INTRA)
    {
        if(depth == par->MaxCUDepth - par->AddCUDepth)
        {
            bs->EncodeSingleBin_CABAC(CTX(bs,PART_SIZE_HEVC), size == PART_SIZE_2Nx2N? 1 : 0);
        }
        return;
    }

    switch(size)
    {
    case PART_SIZE_2Nx2N:
        bs->EncodeSingleBin_CABAC(CTX(bs,PART_SIZE_HEVC), 1);
        break;
    case PART_SIZE_2NxN:
    case PART_SIZE_2NxnU:
    case PART_SIZE_2NxnD:
        bs->EncodeSingleBin_CABAC(CTX(bs,PART_SIZE_HEVC), 0);
        bs->EncodeSingleBin_CABAC(CTX(bs,PART_SIZE_HEVC)+1, 1);
        if (par->AMPAcc[depth])
        {
            if (size == PART_SIZE_2NxN)
            {
                bs->EncodeSingleBin_CABAC(CTX(bs,AMP_SPLIT_POSITION_HEVC), 1);
            }
            else
            {
                bs->EncodeSingleBin_CABAC(CTX(bs,AMP_SPLIT_POSITION_HEVC), 0);
                bs->EncodeBinEP_CABAC(size == PART_SIZE_2NxnU? 0: 1);
            }
        }
        break;
    case PART_SIZE_Nx2N:
    case PART_SIZE_nLx2N:
    case PART_SIZE_nRx2N:
        bs->EncodeSingleBin_CABAC(CTX(bs,PART_SIZE_HEVC),0);
        bs->EncodeSingleBin_CABAC(CTX(bs,PART_SIZE_HEVC)+1,0);

        if (depth == par->MaxCUDepth - par->AddCUDepth &&
            !(pCU->m_data[abs_part_idx].size == 8))
        {
            bs->EncodeSingleBin_CABAC(CTX(bs,PART_SIZE_HEVC)+2,1);
        }
        if (par->AMPAcc[depth])
        {
            if (size == PART_SIZE_Nx2N)
            {
                bs->EncodeSingleBin_CABAC(CTX(bs,AMP_SPLIT_POSITION_HEVC),1);
            }
            else
            {
                bs->EncodeSingleBin_CABAC(CTX(bs,AMP_SPLIT_POSITION_HEVC),0);
                bs->EncodeBinEP_CABAC(size == PART_SIZE_nLx2N? 0: 1);
            }
        }
        break;
    case PART_SIZE_NxN:
        if (depth == par->MaxCUDepth - par->AddCUDepth &&
            !(pCU->m_data[abs_part_idx].size == 8))
        {
            bs->EncodeSingleBin_CABAC(CTX(bs,PART_SIZE_HEVC),0);
            bs->EncodeSingleBin_CABAC(CTX(bs,PART_SIZE_HEVC)+1,0);
            bs->EncodeSingleBin_CABAC(CTX(bs,PART_SIZE_HEVC)+2,0);
        }
        break;
    default:
        assert(0);
    }
}

template <typename PixType, class H265Bs>
static
void h265_code_pred_mode(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx )
{
    Ipp32s predMode = pCU->m_data[abs_part_idx].predMode;
    bs->EncodeSingleBin_CABAC(CTX(bs,PRED_MODE_HEVC), predMode == MODE_INTER ? 0 : 1);
}

template <typename PixType, class H265Bs>
static
void h265_code_transquant_bypass_flag(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx)
{
    Ipp32u symbol = pCU->m_data[abs_part_idx].flags.transquantBypassFlag;
    bs->EncodeSingleBin_CABAC(CTX(bs,CU_TRANSQUANT_BYPASS_FLAG_CTX),symbol);
}

template <typename PixType, class H265Bs>
static
void h265_code_skip_flag(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx)
{
    Ipp32u symbol = isSkipped(pCU->m_data, abs_part_idx) ? 1 : 0;
    Ipp32u ctx_skip = pCU->GetCtxSkipFlag(abs_part_idx);
    bs->EncodeSingleBin_CABAC(CTX(bs,SKIP_FLAG_HEVC)+ctx_skip, symbol);
}

template <typename PixType, class H265Bs>
static
void h265_code_merge_flag(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx)
{
    const Ipp32u symbol = pCU->m_data[abs_part_idx].flags.mergeFlag ? 1 : 0;
    bs->EncodeSingleBin_CABAC(CTX(bs,MERGE_FLAG_HEVC),symbol);
}

template <typename PixType, class H265Bs>
static
void h265_code_merge_index(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx)
{
    Ipp32u num_cand = MAX_NUM_MERGE_CANDS;
    Ipp32u unary_idx = pCU->m_data[abs_part_idx].mergeIdx;
    num_cand = MAX_NUM_MERGE_CANDS;
    if (num_cand > 1)
    {
        for (Ipp32u i = 0; i < num_cand - 1; ++i)
        {
            const Ipp32u symbol = i == unary_idx ? 0 : 1;
            if ( i==0 )
            {
                bs->EncodeSingleBin_CABAC(CTX(bs,MERGE_IDX_HEVC),symbol);
            }
            else
            {
                bs->EncodeBinEP_CABAC(symbol);
            }
            if( symbol == 0 )
            {
                break;
            }
        }
    }
}

template <typename PixType, class H265Bs>
static
void h265_code_split_flag(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx, Ipp32s depth)
{
    H265VideoParam *par = pCU->m_par;
    if (depth == par->MaxCUDepth - par->AddCUDepth)
        return;

    Ipp32u ctx = pCU->GetCtxSplitFlag(abs_part_idx, depth);
    Ipp32u split_flag = (pCU->m_data[abs_part_idx].depth > depth) ? 1 : 0;

    assert(ctx < 3);
    bs->EncodeSingleBin_CABAC(CTX(bs,SPLIT_CODING_UNIT_FLAG_HEVC)+ctx,split_flag);
}

template <class H265Bs>
static
void h265_code_transform_subdiv_flag(H265Bs *bs, Ipp32u symbol, Ipp32u ctx)
{
    bs->EncodeSingleBin_CABAC(CTX(bs,TRANS_SUBDIV_FLAG_HEVC)+ctx,symbol);
}

template <typename PixType>
template <class H265Bs>
void H265CU<PixType>::CodeIntradirLumaAng(H265Bs *bs, Ipp32s abs_part_idx, Ipp8u multiple)
{
    H265CU* pCU = this;
    H265VideoParam *par = pCU->m_par;
    Ipp8u dir[4];

    Ipp8u predictors[4][3] = {
        {0xFF, 0xFF, 0xFF},
        {0xFF, 0xFF, 0xFF},
        {0xFF, 0xFF, 0xFF},
        {0xFF, 0xFF, 0xFF}
    };

    Ipp32s pred_idx[4] = { -1,-1,-1,-1};
    const Ipp8u mode = pCU->m_data[abs_part_idx].partSize;
    const Ipp32s part_num = multiple ? (mode==PART_SIZE_NxN ? 4 : 1) : 1;
    const Ipp32u part_offset = ( par->NumPartInCU >> ( pCU->m_data[abs_part_idx].depth << 1) ) >> 2;

    for (Ipp32s j=0; j<part_num; j++)
    {
        dir[j] = pCU->m_data[ abs_part_idx + part_offset*j ].intraLumaDir;
        pCU->GetIntradirLumaPred( abs_part_idx + part_offset*j, predictors[j] );
        Ipp32s found = 0;
        for(Ipp32s i = 0; i < 3; i++) 
            if(dir[j] == predictors[j][i]) {
                pred_idx[j] = i;
                found = 1;
            }

        bs->EncodeSingleBin_CABAC(CTX(bs,INTRA_LUMA_PRED_MODE_HEVC), found );
    }

    for (Ipp32s j=0; j<part_num; j++)
    {
        if(pred_idx[j] != -1)
        {
            bs->EncodeBinEP_CABAC(pred_idx[j] ? 1 : 0);
            if (pred_idx[j])
                bs->EncodeBinEP_CABAC( pred_idx[j]-1 );
        }
        else // 8.4.2.4
        {
            if (predictors[j][0] > predictors[j][1])
                std::swap(predictors[j][0], predictors[j][1]);

            if (predictors[j][0] > predictors[j][2])
                std::swap(predictors[j][0], predictors[j][2]);

            if (predictors[j][1] > predictors[j][2])
                std::swap(predictors[j][1], predictors[j][2]);

            for(Ipp32s i = 2; i >= 0; i--)
                dir[j] = dir[j] > predictors[j][i] ? dir[j] - 1 : dir[j];

            bs->EncodeBinsEP_CABAC(dir[j],5);
        }
    }
}

template <typename PixType>
CostType H265CU<PixType>::GetIntraChromaModeCost(Ipp32s absPartIdx)
{
    m_bsf->Reset();
    h265_code_intradir_chroma(m_bsf, this, absPartIdx);
    return BIT_COST(m_bsf->GetNumBits());
}

template <typename PixType, class H265Bs>
static void h265_code_intradir_chroma(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx)
{
    Ipp8u i;
    Ipp8u intra_dir_chroma = pCU->m_data[abs_part_idx].intraChromaDir;
/*
    if (tca == 1)
        printf("");
    printf("tca = %d\n",tca++);
*/
    if (intra_dir_chroma == INTRA_DM_CHROMA)
    {
        bs->EncodeSingleBin_CABAC(CTX(bs,INTRA_CHROMA_PRED_MODE_HEVC),0);
    }
    else
    {
        Ipp8u allowed_chroma_dir[NUM_CHROMA_MODE];
        pCU->GetAllowedChromaDir(abs_part_idx, allowed_chroma_dir);

        for (i = 0; i < NUM_CHROMA_MODE - 1; i++)
        {
            if (intra_dir_chroma == allowed_chroma_dir[i])
            {
                intra_dir_chroma = i;
                break;
            }
        }
        assert(i < NUM_CHROMA_MODE - 1);
        bs->EncodeSingleBin_CABAC(CTX(bs,INTRA_CHROMA_PRED_MODE_HEVC),1);
        bs->EncodeBinsEP_CABAC(intra_dir_chroma, 2);
    }
}

template <typename PixType, class H265Bs>
static
void h265_code_interdir(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx)
{
    const Ipp32u inter_dir = pCU->m_data[abs_part_idx].interDir - 1;
    const Ipp32u ctx      = pCU->m_data[abs_part_idx].depth;

    if (pCU->m_data[abs_part_idx].partSize == PART_SIZE_2Nx2N || pCU->m_data[abs_part_idx].size != 8 )
    {
        bs->EncodeSingleBin_CABAC(CTX(bs,INTER_DIR_HEVC)+ctx,inter_dir == 2 ? 1 : 0);
    }

    if (inter_dir < 2)
    {
        bs->EncodeSingleBin_CABAC(CTX(bs,INTER_DIR_HEVC)+4,inter_dir);
    }
}

template <typename PixType, class H265Bs>
static
void h265_code_reffrm_idx(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx, EnumRefPicList ref_list)
{
    Ipp32s j;
    H265ENC_UNREFERENCED_PARAMETER(abs_part_idx);

    Ipp8s ref_idx = pCU->m_data[abs_part_idx].refIdx[ref_list];
    bs->EncodeSingleBin_CABAC(CTX(bs,REF_FRAME_IDX_HEVC),( ref_idx == 0 ? 0 : 1 ));

    j = 0;

    if (ref_idx > 0)
    {
        j++;
        ref_idx--;
        for (Ipp32s i = 0; i < pCU->m_cslice->num_ref_idx[ref_list] - 2; i++)
        {
            Ipp32u code = ((ref_idx == i) ? 0 : 1);

            if (i == 0)
            {
                bs->EncodeSingleBin_CABAC(CTX(bs,REF_FRAME_IDX_HEVC) + j, code);
            }
            else
            {
                bs->EncodeBinEP_CABAC(code);
            }

            if (code == 0)
            {
                break;
            }
        }
//      h265_write_unary_max_symbol(bs, ref_idx - 1, CTX(bs,REF_FRAME_IDX_HEVC) + 1, 1,
//          cslice->num_ref_idx[ref_list] - 2);
    }
}

template <typename PixType, class H265Bs>
static
void h265_code_mvd(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx, EnumRefPicList ref_list)
{
    if(pCU->m_cslice->mvd_l1_zero_flag && ref_list == REF_PIC_LIST_1 && pCU->m_data[abs_part_idx].interDir==3)
    {
        return;
    }

    const Ipp32s hor = pCU->m_data[abs_part_idx].mvd[ref_list].mvx;
    const Ipp32s ver = pCU->m_data[abs_part_idx].mvd[ref_list].mvy;

    bs->EncodeSingleBin_CABAC(CTX(bs,MVD_HEVC),hor != 0 ? 1 : 0);
    bs->EncodeSingleBin_CABAC(CTX(bs,MVD_HEVC),ver != 0 ? 1 : 0);

    const Ipp8u hor_abs_gr0 = hor != 0;
    const Ipp8u ver_abs_gr0 = ver != 0;
    const Ipp32u hor_abs   = 0 > hor ? -hor : hor;
    const Ipp32u ver_abs   = 0 > ver ? -ver : ver;

    if (hor_abs_gr0)
    {
        bs->EncodeSingleBin_CABAC(CTX(bs,MVD_HEVC)+1,hor_abs > 1 ? 1 : 0);
    }

    if (ver_abs_gr0)
    {
        bs->EncodeSingleBin_CABAC(CTX(bs,MVD_HEVC)+1,ver_abs > 1 ? 1 : 0);
    }

    if (hor_abs_gr0)
    {
        if( hor_abs > 1 )
        {
            h265_write_ep_ex_golomb(bs, hor_abs-2, 1 );
        }

        bs->EncodeBinEP_CABAC(0 > hor ? 1 : 0);
    }

    if (ver_abs_gr0)
    {
        if (ver_abs > 1)
        {
            h265_write_ep_ex_golomb(bs, ver_abs-2, 1 );
        }

        bs->EncodeBinEP_CABAC(0 > ver ? 1 : 0);
    }
}

template <typename PixType, class H265Bs>
static
void h265_code_delta_qp(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx, Ipp32s bitdepth)
{
    Ipp32s dqp  = pCU->m_data[abs_part_idx].qp - pCU->GetRefQp( abs_part_idx );

    Ipp32s qp_bd_offset_y = 6 * (bitdepth - 8);
    dqp = dqp + 78 + qp_bd_offset_y + qp_bd_offset_y / 2;
    while (dqp >= 52 + qp_bd_offset_y)
        dqp -= 52 + qp_bd_offset_y;
    dqp -= 26 + qp_bd_offset_y/2;

    Ipp32u abs_dqp = ABS(dqp);
    Ipp32u tu_value = MIN((Ipp32u)abs_dqp, CU_DQP_TU_CMAX);
    h265_write_unary_max_symbol(bs, tu_value, CTX(bs,DQP_HEVC), 1, CU_DQP_TU_CMAX);

    if( abs_dqp >= CU_DQP_TU_CMAX )
    {
        h265_write_ep_ex_golomb(bs, abs_dqp - CU_DQP_TU_CMAX, CU_DQP_EG_k);
    }

    if (abs_dqp > 0)
    {
        bs->EncodeBinEP_CABAC(dqp > 0 ? 0 : 1);
    }
}

#if 0 // prevent build failures on Linux
template <class H265Bs>
static
void h265_code_qt_cbf(H265Bs *bs, H265CU* pCU, Ipp32s abs_part_idx, EnumTextType type, Ipp32u tr_depth)
{
    Ipp32u uiCbf = pCU->get_cbf     ( abs_part_idx, type, tr_depth);
    Ipp32u uiCtx = pCU->get_ctx_qt_cbf(abs_part_idx, type, tr_depth);
    bs->EncodeSingleBin_CABAC(CTX(bs,QT_CBF_HEVC) + (type ? NUM_QT_CBF_CTX : 0) + uiCtx, uiCbf);
}
#endif
template <typename PixType, class H265Bs>
static
void h265_code_transform_skip_flags(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx,
                             Ipp32s width, EnumTextType type)
{
    if (!pCU->m_data[abs_part_idx].flags.transquantBypassFlag &&
//        pCU->m_data->pred_mode[abs_part_idx] == MODE_INTRA &&
        width == 4)
    {
        bs->EncodeSingleBin_CABAC( CTX(bs,TRANSFORMSKIP_FLAG_HEVC) +
            (type ? TEXT_CHROMA: TEXT_LUMA), 0/*pCU->GetTransformSkip(abs_part_idx,type)*/);
    }
}

template <typename PixType, class H265Bs>
static inline
void h265_code_qt_root_cbf(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx)
{
    bs->EncodeSingleBin_CABAC(CTX(bs,QT_ROOT_CBF_HEVC), pCU->GetQtRootCbf(abs_part_idx));
}

static Ipp32s h265_count_nonzero_coeffs(CoeffsType* coeff, Ipp32u size)
{
    __m128i zero = _mm_setzero_si128();
    __m128i cnt = _mm_set1_epi16(size>>3);
    __m128i coefs, z;
    for (Ipp32u i = 0; i < size; i += 16) {
        coefs = _mm_load_si128((__m128i*)(coeff+i));
        z = _mm_cmpeq_epi16(coefs, zero);
        cnt = _mm_add_epi16(cnt, z);
        coefs = _mm_load_si128((__m128i*)(coeff+i+8));
        z = _mm_cmpeq_epi16(coefs, zero);
        cnt = _mm_add_epi16(cnt, z);
    }
    cnt = _mm_hadd_epi16(cnt, cnt);
    cnt = _mm_hadd_epi16(cnt, cnt);
    cnt = _mm_hadd_epi16(cnt, cnt);
    Ipp32s r = _mm_cvtsi128_si32(cnt) & 0xffff;
    return r;
    //Ipp32s count = 0;
    //for(Ipp32u i = 0; i < size; i++)
    //    count += coeff[i] != 0;
    //return count;
}

template <class H265Bs>
static
void h265_code_last_significant_xy(H265Bs *bs, Ipp32u pos_x, Ipp32u pos_y,
                           Ipp32s width, EnumTextType type, Ipp32u scan_idx )
{
    if (scan_idx == COEFF_SCAN_VER)
    {
        Ipp32u t = pos_x;
        pos_x = pos_y;
        pos_y = t;
    }

    Ipp32u ctx_last;
    Ipp32u group_idx_x    = h265_group_idx[ pos_x ];
    Ipp32u group_idx_y    = h265_group_idx[ pos_y ];

    Ipp32s blksize_offset = type ? 0 : (h265_log2m2[ width ] *3 + ((h265_log2m2[ width ] +1)>>2));
    Ipp32s shift = type ? h265_log2m2[ width ] : ((h265_log2m2[ width ]+3)>>2);

    // posX
    for (ctx_last = 0; ctx_last < group_idx_x; ctx_last++)
    {
        bs->EncodeSingleBin_CABAC(CTX(bs,LAST_X_HEVC) + (type ? NUM_CTX_LAST_FLAG_XY : 0)
            + blksize_offset + (ctx_last >>shift), 1);
    }
    if (group_idx_x < h265_group_idx[width - 1])
    {
        bs->EncodeSingleBin_CABAC(CTX(bs,LAST_X_HEVC) + (type ? NUM_CTX_LAST_FLAG_XY : 0)
            + blksize_offset + (ctx_last >>shift), 0);
    }

    // posY
    for (ctx_last = 0; ctx_last < group_idx_y; ctx_last++)
    {
        bs->EncodeSingleBin_CABAC(CTX(bs,LAST_Y_HEVC) + (type ? NUM_CTX_LAST_FLAG_XY : 0)
            + blksize_offset + (ctx_last >>shift), 1);
    }
    if (group_idx_y < h265_group_idx[ width - 1 ])
    {
        bs->EncodeSingleBin_CABAC(CTX(bs,LAST_Y_HEVC) + (type ? NUM_CTX_LAST_FLAG_XY : 0)
            + blksize_offset + (ctx_last >>shift), 0);
    }
    if (group_idx_x > 3)
    {
        Ipp32u count = ( group_idx_x - 2 ) >> 1;
        pos_x       = pos_x - h265_min_in_group[ group_idx_x ];
        for (Ipp32s i = count - 1 ; i >= 0; i-- )
        {
            bs->EncodeBinEP_CABAC(( pos_x >> i ) & 1);
        }
    }
    if (group_idx_y > 3)
    {
        Ipp32u count = ( group_idx_y - 2 ) >> 1;
        pos_y       = pos_y - h265_min_in_group[ group_idx_y ];
        for ( Ipp32s i = count - 1 ; i >= 0; i-- )
        {
            bs->EncodeBinEP_CABAC(( pos_y >> i ) & 1);
        }
    }
}


inline Ipp32u gr_bits (Ipp32u symbol, Ipp32s s)
{
    /* get prefix: */
    register Ipp32u k = symbol >> s;
    /* return # of bits: */
    return k + 1 + s;
}
static const char SCAN4X4[16][2] =
{
    { 0, 0},{ 1, 0},{ 0, 1},{ 0, 2},
    { 1, 1},{ 2, 0},{ 3, 0},{ 2, 1},
    { 1, 2},{ 0, 3},{ 1, 3},{ 2, 2},
    { 3, 1},{ 3, 2},{ 2, 3},{ 3, 3},
};

static const char SCAN8X8[64][2] =
{
    { 0, 0},{ 1, 0},{ 0, 1},{ 0, 2},{ 1, 1},{ 2, 0},{ 3, 0},{ 2, 1},
    { 1, 2},{ 0, 3},{ 0, 4},{ 1, 3},{ 2, 2},{ 3, 1},{ 4, 0},{ 5, 0},
    { 4, 1},{ 3, 2},{ 2, 3},{ 1, 4},{ 0, 5},{ 0, 6},{ 1, 5},{ 2, 4},
    { 3, 3},{ 4, 2},{ 5, 1},{ 6, 0},{ 7, 0},{ 6, 1},{ 5, 2},{ 4, 3},
    { 3, 4},{ 2, 5},{ 1, 6},{ 0, 7},{ 1, 7},{ 2, 6},{ 3, 5},{ 4, 4},
    { 5, 3},{ 6, 2},{ 7, 1},{ 7, 2},{ 6, 3},{ 5, 4},{ 4, 5},{ 3, 6},
    { 2, 7},{ 3, 7},{ 4, 6},{ 5, 5},{ 6, 4},{ 7, 3},{ 7, 4},{ 6, 5},
    { 5, 6},{ 4, 7},{ 5, 7},{ 6, 6},{ 7, 5},{ 7, 6},{ 6, 7},{ 7, 7},
};

static const char SCAN16X16[256][2] =
{
    { 0, 0},{ 1, 0},{ 0, 1},{ 0, 2},{ 1, 1},{ 2, 0},{ 3, 0},{ 2, 1},{ 1, 2},{ 0, 3},{ 0, 4},{ 1, 3},{ 2, 2},{ 3, 1},{ 4, 0},{ 5, 0},
    { 4, 1},{ 3, 2},{ 2, 3},{ 1, 4},{ 0, 5},{ 0, 6},{ 1, 5},{ 2, 4},{ 3, 3},{ 4, 2},{ 5, 1},{ 6, 0},{ 7, 0},{ 6, 1},{ 5, 2},{ 4, 3},
    { 3, 4},{ 2, 5},{ 1, 6},{ 0, 7},{ 0, 8},{ 1, 7},{ 2, 6},{ 3, 5},{ 4, 4},{ 5, 3},{ 6, 2},{ 7, 1},{ 8, 0},{ 9, 0},{ 8, 1},{ 7, 2},
    { 6, 3},{ 5, 4},{ 4, 5},{ 3, 6},{ 2, 7},{ 1, 8},{ 0, 9},{ 0,10},{ 1, 9},{ 2, 8},{ 3, 7},{ 4, 6},{ 5, 5},{ 6, 4},{ 7, 3},{ 8, 2},
    { 9, 1},{10, 0},{11, 0},{10, 1},{ 9, 2},{ 8, 3},{ 7, 4},{ 6, 5},{ 5, 6},{ 4, 7},{ 3, 8},{ 2, 9},{ 1,10},{ 0,11},{ 0,12},{ 1,11},
    { 2,10},{ 3, 9},{ 4, 8},{ 5, 7},{ 6, 6},{ 7, 5},{ 8, 4},{ 9, 3},{10, 2},{11, 1},{12, 0},{13, 0},{12, 1},{11, 2},{10, 3},{ 9, 4},
    { 8, 5},{ 7, 6},{ 6, 7},{ 5, 8},{ 4, 9},{ 3,10},{ 2,11},{ 1,12},{ 0,13},{ 0,14},{ 1,13},{ 2,12},{ 3,11},{ 4,10},{ 5, 9},{ 6, 8},
    { 7, 7},{ 8, 6},{ 9, 5},{10, 4},{11, 3},{12, 2},{13, 1},{14, 0},{15, 0},{14, 1},{13, 2},{12, 3},{11, 4},{10, 5},{ 9, 6},{ 8, 7},
    { 7, 8},{ 6, 9},{ 5,10},{ 4,11},{ 3,12},{ 2,13},{ 1,14},{ 0,15},{ 1,15},{ 2,14},{ 3,13},{ 4,12},{ 5,11},{ 6,10},{ 7, 9},{ 8, 8},
    { 9, 7},{10, 6},{11, 5},{12, 4},{13, 3},{14, 2},{15, 1},{15, 2},{14, 3},{13, 4},{12, 5},{11, 6},{10, 7},{ 9, 8},{ 8, 9},{ 7,10},
    { 6,11},{ 5,12},{ 4,13},{ 3,14},{ 2,15},{ 3,15},{ 4,14},{ 5,13},{ 6,12},{ 7,11},{ 8,10},{ 9, 9},{10, 8},{11, 7},{12, 6},{13, 5},
    {14, 4},{15, 3},{15, 4},{14, 5},{13, 6},{12, 7},{11, 8},{10, 9},{ 9,10},{ 8,11},{ 7,12},{ 6,13},{ 5,14},{ 4,15},{ 5,15},{ 6,14},
    { 7,13},{ 8,12},{ 9,11},{10,10},{11, 9},{12, 8},{13, 7},{14, 6},{15, 5},{15, 6},{14, 7},{13, 8},{12, 9},{11,10},{10,11},{ 9,12},
    { 8,13},{ 7,14},{ 6,15},{ 7,15},{ 8,14},{ 9,13},{10,12},{11,11},{12,10},{13, 9},{14, 8},{15, 7},{15, 8},{14, 9},{13,10},{12,11},
    {11,12},{10,13},{ 9,14},{ 8,15},{ 9,15},{10,14},{11,13},{12,12},{13,11},{14,10},{15, 9},{15,10},{14,11},{13,12},{12,13},{11,14},
    {10,15},{11,15},{12,14},{13,13},{14,12},{15,11},{15,12},{14,13},{13,14},{12,15},{13,15},{14,14},{15,13},{15,14},{14,15},{15,15},
};

static const char SCAN32X32[1024][2] =
{
    { 0, 0},{ 1, 0},{ 0, 1},{ 0, 2},{ 1, 1},{ 2, 0},{ 3, 0},{ 2, 1},{ 1, 2},{ 0, 3},{ 0, 4},{ 1, 3},{ 2, 2},{ 3, 1},{ 4, 0},{ 5, 0},{ 4, 1},{ 3, 2},{ 2, 3},{ 1, 4},{ 0, 5},{ 0, 6},{ 1, 5},{ 2, 4},{ 3, 3},{ 4, 2},{ 5, 1},{ 6, 0},{ 7, 0},{ 6, 1},{ 5, 2},{ 4, 3},
    { 3, 4},{ 2, 5},{ 1, 6},{ 0, 7},{ 0, 8},{ 1, 7},{ 2, 6},{ 3, 5},{ 4, 4},{ 5, 3},{ 6, 2},{ 7, 1},{ 8, 0},{ 9, 0},{ 8, 1},{ 7, 2},{ 6, 3},{ 5, 4},{ 4, 5},{ 3, 6},{ 2, 7},{ 1, 8},{ 0, 9},{ 0,10},{ 1, 9},{ 2, 8},{ 3, 7},{ 4, 6},{ 5, 5},{ 6, 4},{ 7, 3},{ 8, 2},
    { 9, 1},{10, 0},{11, 0},{10, 1},{ 9, 2},{ 8, 3},{ 7, 4},{ 6, 5},{ 5, 6},{ 4, 7},{ 3, 8},{ 2, 9},{ 1,10},{ 0,11},{ 0,12},{ 1,11},{ 2,10},{ 3, 9},{ 4, 8},{ 5, 7},{ 6, 6},{ 7, 5},{ 8, 4},{ 9, 3},{10, 2},{11, 1},{12, 0},{13, 0},{12, 1},{11, 2},{10, 3},{ 9, 4},
    { 8, 5},{ 7, 6},{ 6, 7},{ 5, 8},{ 4, 9},{ 3,10},{ 2,11},{ 1,12},{ 0,13},{ 0,14},{ 1,13},{ 2,12},{ 3,11},{ 4,10},{ 5, 9},{ 6, 8},{ 7, 7},{ 8, 6},{ 9, 5},{10, 4},{11, 3},{12, 2},{13, 1},{14, 0},{15, 0},{14, 1},{13, 2},{12, 3},{11, 4},{10, 5},{ 9, 6},{ 8, 7},
    { 7, 8},{ 6, 9},{ 5,10},{ 4,11},{ 3,12},{ 2,13},{ 1,14},{ 0,15},{ 0,16},{ 1,15},{ 2,14},{ 3,13},{ 4,12},{ 5,11},{ 6,10},{ 7, 9},{ 8, 8},{ 9, 7},{10, 6},{11, 5},{12, 4},{13, 3},{14, 2},{15, 1},{16, 0},{17, 0},{16, 1},{15, 2},{14, 3},{13, 4},{12, 5},{11, 6},
    {10, 7},{ 9, 8},{ 8, 9},{ 7,10},{ 6,11},{ 5,12},{ 4,13},{ 3,14},{ 2,15},{ 1,16},{ 0,17},{ 0,18},{ 1,17},{ 2,16},{ 3,15},{ 4,14},{ 5,13},{ 6,12},{ 7,11},{ 8,10},{ 9, 9},{10, 8},{11, 7},{12, 6},{13, 5},{14, 4},{15, 3},{16, 2},{17, 1},{18, 0},{19, 0},{18, 1},
    {17, 2},{16, 3},{15, 4},{14, 5},{13, 6},{12, 7},{11, 8},{10, 9},{ 9,10},{ 8,11},{ 7,12},{ 6,13},{ 5,14},{ 4,15},{ 3,16},{ 2,17},{ 1,18},{ 0,19},{ 0,20},{ 1,19},{ 2,18},{ 3,17},{ 4,16},{ 5,15},{ 6,14},{ 7,13},{ 8,12},{ 9,11},{10,10},{11, 9},{12, 8},{13, 7},
    {14, 6},{15, 5},{16, 4},{17, 3},{18, 2},{19, 1},{20, 0},{21, 0},{20, 1},{19, 2},{18, 3},{17, 4},{16, 5},{15, 6},{14, 7},{13, 8},{12, 9},{11,10},{10,11},{ 9,12},{ 8,13},{ 7,14},{ 6,15},{ 5,16},{ 4,17},{ 3,18},{ 2,19},{ 1,20},{ 0,21},{ 0,22},{ 1,21},{ 2,20},
    { 3,19},{ 4,18},{ 5,17},{ 6,16},{ 7,15},{ 8,14},{ 9,13},{10,12},{11,11},{12,10},{13, 9},{14, 8},{15, 7},{16, 6},{17, 5},{18, 4},{19, 3},{20, 2},{21, 1},{22, 0},{23, 0},{22, 1},{21, 2},{20, 3},{19, 4},{18, 5},{17, 6},{16, 7},{15, 8},{14, 9},{13,10},{12,11},
    {11,12},{10,13},{ 9,14},{ 8,15},{ 7,16},{ 6,17},{ 5,18},{ 4,19},{ 3,20},{ 2,21},{ 1,22},{ 0,23},{ 0,24},{ 1,23},{ 2,22},{ 3,21},{ 4,20},{ 5,19},{ 6,18},{ 7,17},{ 8,16},{ 9,15},{10,14},{11,13},{12,12},{13,11},{14,10},{15, 9},{16, 8},{17, 7},{18, 6},{19, 5},
    {20, 4},{21, 3},{22, 2},{23, 1},{24, 0},{25, 0},{24, 1},{23, 2},{22, 3},{21, 4},{20, 5},{19, 6},{18, 7},{17, 8},{16, 9},{15,10},{14,11},{13,12},{12,13},{11,14},{10,15},{ 9,16},{ 8,17},{ 7,18},{ 6,19},{ 5,20},{ 4,21},{ 3,22},{ 2,23},{ 1,24},{ 0,25},{ 0,26},
    { 1,25},{ 2,24},{ 3,23},{ 4,22},{ 5,21},{ 6,20},{ 7,19},{ 8,18},{ 9,17},{10,16},{11,15},{12,14},{13,13},{14,12},{15,11},{16,10},{17, 9},{18, 8},{19, 7},{20, 6},{21, 5},{22, 4},{23, 3},{24, 2},{25, 1},{26, 0},{27, 0},{26, 1},{25, 2},{24, 3},{23, 4},{22, 5},
    {21, 6},{20, 7},{19, 8},{18, 9},{17,10},{16,11},{15,12},{14,13},{13,14},{12,15},{11,16},{10,17},{ 9,18},{ 8,19},{ 7,20},{ 6,21},{ 5,22},{ 4,23},{ 3,24},{ 2,25},{ 1,26},{ 0,27},{ 0,28},{ 1,27},{ 2,26},{ 3,25},{ 4,24},{ 5,23},{ 6,22},{ 7,21},{ 8,20},{ 9,19},
    {10,18},{11,17},{12,16},{13,15},{14,14},{15,13},{16,12},{17,11},{18,10},{19, 9},{20, 8},{21, 7},{22, 6},{23, 5},{24, 4},{25, 3},{26, 2},{27, 1},{28, 0},{29, 0},{28, 1},{27, 2},{26, 3},{25, 4},{24, 5},{23, 6},{22, 7},{21, 8},{20, 9},{19,10},{18,11},{17,12},
    {16,13},{15,14},{14,15},{13,16},{12,17},{11,18},{10,19},{ 9,20},{ 8,21},{ 7,22},{ 6,23},{ 5,24},{ 4,25},{ 3,26},{ 2,27},{ 1,28},{ 0,29},{ 0,30},{ 1,29},{ 2,28},{ 3,27},{ 4,26},{ 5,25},{ 6,24},{ 7,23},{ 8,22},{ 9,21},{10,20},{11,19},{12,18},{13,17},{14,16},
    {15,15},{16,14},{17,13},{18,12},{19,11},{20,10},{21, 9},{22, 8},{23, 7},{24, 6},{25, 5},{26, 4},{27, 3},{28, 2},{29, 1},{30, 0},{31, 0},{30, 1},{29, 2},{28, 3},{27, 4},{26, 5},{25, 6},{24, 7},{23, 8},{22, 9},{21,10},{20,11},{19,12},{18,13},{17,14},{16,15},
    {15,16},{14,17},{13,18},{12,19},{11,20},{10,21},{ 9,22},{ 8,23},{ 7,24},{ 6,25},{ 5,26},{ 4,27},{ 3,28},{ 2,29},{ 1,30},{ 0,31},{ 1,31},{ 2,30},{ 3,29},{ 4,28},{ 5,27},{ 6,26},{ 7,25},{ 8,24},{ 9,23},{10,22},{11,21},{12,20},{13,19},{14,18},{15,17},{16,16},
    {17,15},{18,14},{19,13},{20,12},{21,11},{22,10},{23, 9},{24, 8},{25, 7},{26, 6},{27, 5},{28, 4},{29, 3},{30, 2},{31, 1},{31, 2},{30, 3},{29, 4},{28, 5},{27, 6},{26, 7},{25, 8},{24, 9},{23,10},{22,11},{21,12},{20,13},{19,14},{18,15},{17,16},{16,17},{15,18},
    {14,19},{13,20},{12,21},{11,22},{10,23},{ 9,24},{ 8,25},{ 7,26},{ 6,27},{ 5,28},{ 4,29},{ 3,30},{ 2,31},{ 3,31},{ 4,30},{ 5,29},{ 6,28},{ 7,27},{ 8,26},{ 9,25},{10,24},{11,23},{12,22},{13,21},{14,20},{15,19},{16,18},{17,17},{18,16},{19,15},{20,14},{21,13},
    {22,12},{23,11},{24,10},{25, 9},{26, 8},{27, 7},{28, 6},{29, 5},{30, 4},{31, 3},{31, 4},{30, 5},{29, 6},{28, 7},{27, 8},{26, 9},{25,10},{24,11},{23,12},{22,13},{21,14},{20,15},{19,16},{18,17},{17,18},{16,19},{15,20},{14,21},{13,22},{12,23},{11,24},{10,25},
    { 9,26},{ 8,27},{ 7,28},{ 6,29},{ 5,30},{ 4,31},{ 5,31},{ 6,30},{ 7,29},{ 8,28},{ 9,27},{10,26},{11,25},{12,24},{13,23},{14,22},{15,21},{16,20},{17,19},{18,18},{19,17},{20,16},{21,15},{22,14},{23,13},{24,12},{25,11},{26,10},{27, 9},{28, 8},{29, 7},{30, 6},
    {31, 5},{31, 6},{30, 7},{29, 8},{28, 9},{27,10},{26,11},{25,12},{24,13},{23,14},{22,15},{21,16},{20,17},{19,18},{18,19},{17,20},{16,21},{15,22},{14,23},{13,24},{12,25},{11,26},{10,27},{ 9,28},{ 8,29},{ 7,30},{ 6,31},{ 7,31},{ 8,30},{ 9,29},{10,28},{11,27},
    {12,26},{13,25},{14,24},{15,23},{16,22},{17,21},{18,20},{19,19},{20,18},{21,17},{22,16},{23,15},{24,14},{25,13},{26,12},{27,11},{28,10},{29, 9},{30, 8},{31, 7},{31, 8},{30, 9},{29,10},{28,11},{27,12},{26,13},{25,14},{24,15},{23,16},{22,17},{21,18},{20,19},
    {19,20},{18,21},{17,22},{16,23},{15,24},{14,25},{13,26},{12,27},{11,28},{10,29},{ 9,30},{ 8,31},{ 9,31},{10,30},{11,29},{12,28},{13,27},{14,26},{15,25},{16,24},{17,23},{18,22},{19,21},{20,20},{21,19},{22,18},{23,17},{24,16},{25,15},{26,14},{27,13},{28,12},
    {29,11},{30,10},{31, 9},{31,10},{30,11},{29,12},{28,13},{27,14},{26,15},{25,16},{24,17},{23,18},{22,19},{21,20},{20,21},{19,22},{18,23},{17,24},{16,25},{15,26},{14,27},{13,28},{12,29},{11,30},{10,31},{11,31},{12,30},{13,29},{14,28},{15,27},{16,26},{17,25},
    {18,24},{19,23},{20,22},{21,21},{22,20},{23,19},{24,18},{25,17},{26,16},{27,15},{28,14},{29,13},{30,12},{31,11},{31,12},{30,13},{29,14},{28,15},{27,16},{26,17},{25,18},{24,19},{23,20},{22,21},{21,22},{20,23},{19,24},{18,25},{17,26},{16,27},{15,28},{14,29},
    {13,30},{12,31},{13,31},{14,30},{15,29},{16,28},{17,27},{18,26},{19,25},{20,24},{21,23},{22,22},{23,21},{24,20},{25,19},{26,18},{27,17},{28,16},{29,15},{30,14},{31,13},{31,14},{30,15},{29,16},{28,17},{27,18},{26,19},{25,20},{24,21},{23,22},{22,23},{21,24},
    {20,25},{19,26},{18,27},{17,28},{16,29},{15,30},{14,31},{15,31},{16,30},{17,29},{18,28},{19,27},{20,26},{21,25},{22,24},{23,23},{24,22},{25,21},{26,20},{27,19},{28,18},{29,17},{30,16},{31,15},{31,16},{30,17},{29,18},{28,19},{27,20},{26,21},{25,22},{24,23},
    {23,24},{22,25},{21,26},{20,27},{19,28},{18,29},{17,30},{16,31},{17,31},{18,30},{19,29},{20,28},{21,27},{22,26},{23,25},{24,24},{25,23},{26,22},{27,21},{28,20},{29,19},{30,18},{31,17},{31,18},{30,19},{29,20},{28,21},{27,22},{26,23},{25,24},{24,25},{23,26},
    {22,27},{21,28},{20,29},{19,30},{18,31},{19,31},{20,30},{21,29},{22,28},{23,27},{24,26},{25,25},{26,24},{27,23},{28,22},{29,21},{30,20},{31,19},{31,20},{30,21},{29,22},{28,23},{27,24},{26,25},{25,26},{24,27},{23,28},{22,29},{21,30},{20,31},{21,31},{22,30},
    {23,29},{24,28},{25,27},{26,26},{27,25},{28,24},{29,23},{30,22},{31,21},{31,22},{30,23},{29,24},{28,25},{27,26},{26,27},{25,28},{24,29},{23,30},{22,31},{23,31},{24,30},{25,29},{26,28},{27,27},{28,26},{29,25},{30,24},{31,23},{31,24},{30,25},{29,26},{28,27},
    {27,28},{26,29},{25,30},{24,31},{25,31},{26,30},{27,29},{28,28},{29,27},{30,26},{31,25},{31,26},{30,27},{29,28},{28,29},{27,30},{26,31},{27,31},{28,30},{29,29},{30,28},{31,27},{31,28},{30,29},{29,30},{28,31},{29,31},{30,30},{31,29},{31,30},{30,31},{31,31},
};

static Ipp32s log2(Ipp32u v)
{
    assert(v > 0);
#if defined(_WIN32) || defined(_WIN64)
    unsigned long index;
#else
    unsigned int index;
#endif
    unsigned char dst = _BitScanReverse(&index, v);
    return index;
}

static Ipp32s log2_by2(Ipp32u v)
{
    return log2(v*v);
}

inline void H265_FASTCALL Rast2Scan4x4(const Ipp16s *src, Ipp32s pitch, Ipp16s *dst)
{
    __m128i a,b,c,d,e,f;
    a = _mm_loadl_epi64((__m128i*)(src+0*pitch));
    b = _mm_loadl_epi64((__m128i*)(src+1*pitch));
    c = _mm_loadl_epi64((__m128i*)(src+2*pitch));
    d = _mm_loadl_epi64((__m128i*)(src+3*pitch));
    e = _mm_unpacklo_epi16(b, c);
    f = _mm_unpacklo_epi16(a, e);
    f = _mm_insert_epi16(f, src[3*pitch], 6);
    f = _mm_shufflehi_epi16(f, _MM_SHUFFLE(3,2,0,1));
    _mm_store_si128((__m128i*)dst, f);
    e = _mm_unpackhi_epi64(e, e);
    e = _mm_unpacklo_epi16(e, d);
    e = _mm_shufflelo_epi16(e, _MM_SHUFFLE(2,3,1,0));
    e = _mm_insert_epi16(e, src[3], 1);
    _mm_store_si128((__m128i*)dst+1, e);
}

static Ipp32s FastParametricCoeffCostEstimator(CoeffsType *x, Ipp32u n, Ipp32s num_sig, Ipp32s qp, bool isIntra, Ipp16s *nzcoeffs)
{
    CoeffsType d;
    Ipp32s z;
    Ipp32s c=0, zero_run=0;
    Ipp32u i=0;
    const Ipp32s incVlc[] = {0,4,8,12,24,48,32768};      // maximum vlc = 6!
    CoeffsType *iscancoeff = nzcoeffs;
    Ipp32s levels=0;
    Ipp32s vlcnum=0;
    Ipp32s u=0, v=0;
    Ipp32u N= n*n;
    const unsigned short *scan;
    if(n==32)       scan = h265_sig_last_scan[2][4];
    else if(n==16)  scan = h265_sig_last_scan[2][3];
    else if(n==8)   scan = h265_sig_last_scan[2][2];
    else            scan = h265_sig_last_scan[2][1];

    const Ipp32s EffciencyParameter[4] = {200, 196, 192, 188};
    Ipp32s ctx0 = 0;
    if(qp<25)           ctx0 = 0;
    else if(qp<31)      ctx0 = 1;
    else if(qp<37)      ctx0 = 2;
    else                ctx0 = 3;

   
    for (i=0; i<N; i++) 
    {
        /* # bits required for vlc coding gen lap dist */ 
        d = x[scan[i]];

        if (!d) {
            zero_run++;
        } else {
            if (d < 0)
                d = -d;

            c++;
            if (zero_run)
                c += log2_by2(zero_run+1);

            zero_run=0;
            iscancoeff[levels] = d;
            levels++;
            if (levels >= num_sig)
                break;
        }
    }

    if (i<N)
        c += log2(N-i+1)+1;
    
    for (i=levels; i>0; i--) {
        d = iscancoeff[i-1];
        c += gr_bits(d-1, vlcnum);
        c++; // sign
        if (d+1 > incVlc[vlcnum+1])
            vlcnum++;
    }
    
    return c * EffciencyParameter[ctx0];
}

template <typename PixType>
template <class H265Bs>
void H265CU<PixType>::CodeCoeffNxN(H265Bs *bs, H265CU<PixType>* pCU, CoeffsType* coeffs, Ipp32s abs_part_idx,
                                   Ipp32u width, EnumTextType type )
{
    const H265VideoParam *par = pCU->m_par;
    assert(width <= par->MaxTrSize);

    Ipp32u num_sig = h265_count_nonzero_coeffs(coeffs, width * width);
    assert(num_sig > 0);

    if(!bs->isReal() && m_par->FastCoeffCost && !m_isRdoq) {
        // A radical approach, est coeff cost & match efficiency. (suboptimal only for higher TUs)
        Ipp32s est_bits=FastParametricCoeffCostEstimator(coeffs, width, num_sig, pCU->m_data[abs_part_idx].qp,
            pCU->m_data[abs_part_idx].predMode == MODE_INTRA, m_scratchPad.fastParametricCoeffCostEstimator.nzcoeffs);
        bs->m_base.m_bitOffset += est_bits;
        return;
    }

    if (par->transformSkipEnabledFlag)
        h265_code_transform_skip_flags( bs, pCU, abs_part_idx, width, type );

    type = type == TEXT_LUMA ? TEXT_LUMA : ( type == TEXT_NONE ? TEXT_NONE : TEXT_CHROMA );

    const Ipp32u log2_block_size = h265_log2m2[width] + 2;
    Ipp32u scan_idx = pCU->GetCoefScanIdx(abs_part_idx, log2_block_size, type==TEXT_LUMA,
        pCU->m_data[abs_part_idx].predMode == MODE_INTRA);
    if (scan_idx == COEFF_SCAN_ZIGZAG)
    {
        scan_idx = COEFF_SCAN_DIAG;
    }
    Ipp32s block_type = log2_block_size;

    const Ipp16u *scan = h265_sig_last_scan[scan_idx - 1][log2_block_size - 1];

    Ipp8u sig_coeff_group_flag[MLS_GRP_NUM];
    const Ipp32u shift = MLS_CG_SIZE >> 1;
    const Ipp32u num_blk_side = width >> shift;
    const Ipp32u log2_num_blk_side = log2_block_size - shift;
    const Ipp32u mask_num_blk_side_x = num_blk_side - 1;
    const Ipp32u mask_num_blk_side_y = mask_num_blk_side_x << log2_num_blk_side;
    memset(sig_coeff_group_flag, 0, sizeof(sig_coeff_group_flag[0]) << (log2_num_blk_side * 2));

    // Find position of last coefficient
    Ipp32s scan_pos_last = -1;
    Ipp32s pos_last;

    for(;;)
    {
        pos_last = scan[ ++scan_pos_last ];

        if (coeffs[pos_last])
        {
            // get L1 sig map
            Ipp32u blk_idx  = ((pos_last >> 2*shift) & mask_num_blk_side_y) + ((pos_last >> shift) & mask_num_blk_side_x);
            sig_coeff_group_flag[blk_idx] = 1;
            num_sig --;
            if (!num_sig)
                break;
        }
    }

    // Code position of last coefficient
    Ipp32s pos_last_y = pos_last >> log2_block_size;
    Ipp32s pos_last_x = pos_last & (width - 1);
    h265_code_last_significant_xy(bs, pos_last_x, pos_last_y, width, type, scan_idx);

    //===== code significance flag =====
    CABAC_CONTEXT_H265 * const baseCoeffGroupCtx = CTX(bs,SIG_COEFF_GROUP_FLAG_HEVC) + 2 * type;
    CABAC_CONTEXT_H265 * const baseCtx = (type==TEXT_LUMA) ?
        CTX(bs,SIG_FLAG_HEVC) : CTX(bs,SIG_FLAG_HEVC) + NUM_SIG_FLAG_CTX_LUMA;

    const Ipp16u *scanCG;
    if (log2_block_size == 3)
        scanCG = h265_sig_last_scan_8x8[ scan_idx ];
    else if (log2_block_size == 5)
        scanCG = h265_sig_scan_CG32x32;
    else
        scanCG = h265_sig_last_scan[scan_idx - 1][log2_block_size > 3 ? log2_block_size-2-1 : 0];

    bool valid_flag;
    if (pCU->m_data[abs_part_idx].flags.transquantBypassFlag)
        valid_flag = false;
    else
        valid_flag = par->SBHFlag;

    const Ipp32s  last_scan_set = scan_pos_last >> LOG2_SCAN_SET_SIZE;
    Ipp32u c1 = 1;
    Ipp32u go_rice_param = 0;
    Ipp32s  scan_pos_sig = scan_pos_last;

    for (Ipp32s subset = last_scan_set; subset >= 0; subset--)
    {
        Ipp32s num_nonzero = 0;
        Ipp32s sub_pos = subset << LOG2_SCAN_SET_SIZE;
        go_rice_param = 0;
        Ipp32u abs_coeff[16];
        Ipp32u coeff_signs = 0;

        Ipp32s last_nz_pos_in_CG = -1, first_nz_pos_in_CG = SCAN_SET_SIZE;

        if (scan_pos_sig == scan_pos_last)
        {
            abs_coeff[0] = abs(coeffs[ pos_last ]);
            coeff_signs    = (coeffs[ pos_last ] < 0);
            num_nonzero    = 1;
            last_nz_pos_in_CG  = scan_pos_sig;
            first_nz_pos_in_CG = scan_pos_sig;
            scan_pos_sig--;
        }

        // encode significant_coeffgroup_flag
        Ipp32s CG_blk_pos = scanCG[ subset ];
        Ipp32s CG_pos_y   = CG_blk_pos >> log2_num_blk_side;
        Ipp32s CG_pos_x   = CG_blk_pos & (num_blk_side - 1);
        if (subset == last_scan_set || subset == 0)
        {
            sig_coeff_group_flag[ CG_blk_pos ] = 1;
        }
        else
        {
            Ipp32u ctx_sig  = h265_quant_getSigCoeffGroupCtxInc(sig_coeff_group_flag, CG_pos_x, CG_pos_y, scan_idx, width);
            bs->EncodeSingleBin_CABAC(baseCoeffGroupCtx + ctx_sig, sig_coeff_group_flag[ CG_blk_pos ]);
        }

        // encode significant_coeff_flag
        if (sig_coeff_group_flag[ CG_blk_pos ])
        {
            Ipp32s pattern_sig_ctx = h265_quant_calcpattern_sig_ctx( sig_coeff_group_flag, CG_pos_x, CG_pos_y, width );
            for (; scan_pos_sig >= sub_pos; scan_pos_sig--)
            {
                Ipp32u blk_pos, sig;
                blk_pos  = scan[ scan_pos_sig ];
                sig     = (coeffs[blk_pos] != 0);
                if (scan_pos_sig > sub_pos || subset == 0 || num_nonzero)
                {
                    Ipp32u pos_y = blk_pos >> log2_block_size;
                    Ipp32u pos_x = blk_pos & (width - 1);
                    Ipp32u ctx_sig = h265_quant_getSigCtxInc( pattern_sig_ctx, scan_idx, pos_x, pos_y, block_type, type );
                    bs->EncodeSingleBin_CABAC(baseCtx + ctx_sig, sig);
                }
                if (sig)
                {
                    abs_coeff[num_nonzero] = abs( coeffs[blk_pos] );
                    coeff_signs = 2 * coeff_signs + (coeffs[blk_pos] < 0);
                    num_nonzero++;
                    if (last_nz_pos_in_CG == -1)
                    {
                        last_nz_pos_in_CG = scan_pos_sig;
                    }
                    first_nz_pos_in_CG = scan_pos_sig;
                }
            }
        }
        else
        {
            scan_pos_sig = sub_pos - 1;
        }

        if (num_nonzero > 0)
        {
            bool sign_hidden = (last_nz_pos_in_CG - first_nz_pos_in_CG >= SBH_THRESHOLD);

            Ipp32u ctx_set = (subset > 0 && type==TEXT_LUMA) ? 2 : 0;

            if (c1 == 0)
            {
                ctx_set++;
            }

            c1 = 1;

            CABAC_CONTEXT_H265 *base_ctx_mod = ( type == TEXT_LUMA ) ?
                CTX(bs,ONE_FLAG_HEVC)  + 4 * ctx_set :
                CTX(bs,ONE_FLAG_HEVC)  + NUM_ONE_FLAG_CTX_LUMA + 4 * ctx_set;

            Ipp32s num_c1_flag = IPP_MIN(num_nonzero, C1FLAG_NUMBER);
            Ipp32s first_c2_flag_idx = -1;
            for (Ipp32s idx = 0; idx < num_c1_flag; idx++)
            {
                Ipp32u symbol = abs_coeff[ idx ] > 1;
                bs->EncodeSingleBin_CABAC(base_ctx_mod + c1, symbol);
                if( symbol )
                {
                    c1 = 0;

                    if (first_c2_flag_idx == -1)
                    {
                        first_c2_flag_idx = idx;
                    }
                }
                else if( (c1 < 3) && (c1 > 0) )
                {
                    c1++;
                }
            }

            if (c1 == 0)
            {

                base_ctx_mod = ( type == TEXT_LUMA ) ?
                    CTX(bs,ABS_FLAG_HEVC)  + ctx_set :
                CTX(bs,ABS_FLAG_HEVC)  + NUM_ABS_FLAG_CTX_LUMA + ctx_set;
                if ( first_c2_flag_idx != -1)
                {
                    Ipp32u symbol = abs_coeff[ first_c2_flag_idx ] > 2;
                    bs->EncodeSingleBin_CABAC(base_ctx_mod, symbol);
                }
            }

            if (valid_flag && sign_hidden)
            {
                bs->EncodeBinsEP_CABAC( (coeff_signs >> 1), num_nonzero-1 );
            }
            else
            {
                bs->EncodeBinsEP_CABAC( coeff_signs, num_nonzero );
            }

            Ipp32s fist_coeff2 = 1;
            if (c1 == 0 || num_nonzero > C1FLAG_NUMBER)
            {
                for (Ipp32s idx = 0; idx < num_nonzero; idx++)
                {
                    Ipp32u base_level  = (idx < C1FLAG_NUMBER)? (2 + fist_coeff2 ) : 1;

                    if (abs_coeff[ idx ] >= base_level)
                    {
                        h265_write_coef_remain_ex_golomb(bs, abs_coeff[ idx ] - base_level, go_rice_param );
                        if (abs_coeff[idx] > 3*(1u<<go_rice_param))
                        {
                            go_rice_param = IPP_MIN(go_rice_param+ 1, 4);
                        }
                    }
                    if (abs_coeff[ idx ] >= 2)
                    {
                        fist_coeff2 = 0;
                    }
                }
            }
        }
    }
}

#if 0  // prevent build failures on Linux
// NOTE: ALF_CU_FLAG_HEVC is not defined at the moment
template <class H265Bs>
static inline
void h265_code_alf_ctrl_flag(H265Bs *bs, Ipp32u code)
{
    bs->EncodeSingleBin_CABAC(CTX(bs,ALF_CU_FLAG_HEVC), code);
}

template <class H265Bs>
void h265_encode_qp(H265Bs *bs, H265CU* pCU, Ipp32s qp_bd_offset_y, Ipp32s abs_part_idx, Ipp8u bRD)
{
    if (bRD)
    {
        abs_part_idx = 0;
    }

    if (pCU->m_par->UseDQP)
    {
        Ipp32s iDQp  = pCU->m_data[abs_part_idx].qp - pCU->get_ref_qp( abs_part_idx );

        //      Ipp32s qp_bd_offset_y =  pCU->getSlice()->getSPS()->getqp_bd_offset_y();
        iDQp = (iDQp + 78 + qp_bd_offset_y + (qp_bd_offset_y/2)) % (52 + qp_bd_offset_y) - 26 - (qp_bd_offset_y/2);

        if ( iDQp == 0 )
        {
            bs->EncodeSingleBin_CABAC( CTX(bs,DQP_HEVC), 0);
        }
        else
        {
            bs->EncodeSingleBin_CABAC( CTX(bs,DQP_HEVC), 1);

            Ipp32u sign = (iDQp > 0 ? 0 : 1);

            bs->EncodeBinEP_CABAC(sign);

            assert(iDQp >= -(26+(qp_bd_offset_y/2)));
            assert(iDQp <=  (25+(qp_bd_offset_y/2)));

            Ipp32u max_abs_dqp_m1 = 24 + (qp_bd_offset_y/2) + (sign);
            Ipp32u abs_dqp_m1 = (Ipp32u)((iDQp > 0)? iDQp  : (-iDQp)) - 1;
            h265_write_unary_max_symbol( bs, abs_dqp_m1, CTX(bs,DQP_HEVC), 1, max_abs_dqp_m1);
        }
    }
}
#endif

template <typename PixType>
void H265CU<PixType>::PutLumaTu(H265BsFake *bs, Ipp32u offset_luma, Ipp32s abs_part_idx,
                                Ipp32u depth, Ipp32u width, Ipp32u tr_idx)
{
    const Ipp32u Log2TrafoSize = m_par->Log2MaxCUSize - depth;
    Ipp32u part_num_mask = (4 << ( (m_par->MaxCUDepth - depth) * 2 )) - 1;

    if (Log2TrafoSize == m_par->QuadtreeTULog2MinSize)
        ;
    else if (Log2TrafoSize == H265Enc::GetQuadtreeTuLog2MinSizeInCu(
            m_par, m_par->Log2MaxCUSize - m_data[abs_part_idx].depth,
            m_data[abs_part_idx].partSize, m_data[abs_part_idx].predMode))
        ;
    else {
        assert( Log2TrafoSize > H265Enc::GetQuadtreeTuLog2MinSizeInCu(m_par, m_par->Log2MaxCUSize - m_data[abs_part_idx].depth,
            m_data[abs_part_idx].partSize, m_data[abs_part_idx].predMode) );
        bs->EncodeSingleBin_CABAC(CTX(bs,TRANS_SUBDIV_FLAG_HEVC) + 5 - Log2TrafoSize, 0);
    }

    const Ipp32u tr_depth_cur = depth - m_data[abs_part_idx].depth;
    const bool first_cbf_of_cu = tr_depth_cur == 0;
    if (first_cbf_of_cu) {
        bs->EncodeSingleBin_CABAC(CTX(bs,QT_CBF_HEVC) + NUM_QT_CBF_CTX + GetCtxQtCbfChroma(tr_depth_cur), 0);
        if (m_par->chroma422)
            bs->EncodeSingleBin_CABAC(CTX(bs,QT_CBF_HEVC) + NUM_QT_CBF_CTX + GetCtxQtCbfChroma(tr_depth_cur), 0);
        bs->EncodeSingleBin_CABAC(CTX(bs,QT_CBF_HEVC) + NUM_QT_CBF_CTX + GetCtxQtCbfChroma(tr_depth_cur), 0);
        if (m_par->chroma422)
            bs->EncodeSingleBin_CABAC(CTX(bs,QT_CBF_HEVC) + NUM_QT_CBF_CTX + GetCtxQtCbfChroma(tr_depth_cur), 0);
    }

    if (m_data[abs_part_idx].predMode != MODE_INTRA && depth == m_data[abs_part_idx].depth) {
        assert(GetCbf(abs_part_idx, TEXT_LUMA, 0));
    } else {
        bs->EncodeSingleBin_CABAC(CTX(bs,QT_CBF_HEVC) + GetCtxQtCbfLuma(m_data[abs_part_idx].trIdx), 1);
    }

    if (m_par->UseDQP) // dQP: only for LCU once
        h265_code_delta_qp(bs, this, abs_part_idx, m_par->bitDepthLuma);

    CodeCoeffNxN(bs, this, m_coeffWorkY + offset_luma, abs_part_idx, width, TEXT_LUMA);
}


template <typename PixType>
template <class H265Bs>
void H265CU<PixType>::PutTransform(H265Bs* bs,Ipp32u offset_luma, Ipp32u offset_chroma,
                                   Ipp32s abs_part_idx, Ipp32u abs_tu_part_idx,
                                   Ipp32u depth, Ipp32u width,
                                   Ipp32u tr_idx, Ipp8u& code_dqp,
                                   Ipp8u rd_mode )
{
    const Ipp32u subdiv = (Ipp8u)(m_data[abs_part_idx].trIdx + m_data[abs_part_idx].depth) > depth;
    const Ipp32u Log2TrafoSize = m_par->Log2MaxCUSize - depth;
    Ipp32u cbfY = GetCbf( abs_part_idx, TEXT_LUMA    , tr_idx );
    Ipp32u cbfU = GetCbf( abs_part_idx, TEXT_CHROMA_U, tr_idx );
    Ipp32u cbfV = GetCbf( abs_part_idx, TEXT_CHROMA_V, tr_idx );
    Ipp32u cbfU1 = 0, cbfV1 = 0;
    Ipp32u part_num_mask = (4 << ( (m_par->MaxCUDepth - depth) * 2 )) - 1;

    if (tr_idx==0)
        m_bakAbsPartIdxCu = abs_part_idx;
    
    if (Log2TrafoSize == 2)
        //&& ((abs_part_idx & part_num_mask) == part_num_mask)) // Need CBF for DQP -N
    {
        Ipp32s abs_part_idx_0 = abs_part_idx & ~part_num_mask;
        cbfU = GetCbf( abs_part_idx_0, TEXT_CHROMA_U, tr_idx );
        cbfV = GetCbf( abs_part_idx_0, TEXT_CHROMA_V, tr_idx );
        if (m_par->chroma422) {
            cbfU1 = GetCbf( abs_part_idx_0 + 2, TEXT_CHROMA_U, tr_idx );
            cbfV1 = GetCbf( abs_part_idx_0 + 2, TEXT_CHROMA_V, tr_idx );
        }
    } 

    if( m_data[abs_part_idx].predMode == MODE_INTRA &&
        m_data[abs_part_idx].partSize == PART_SIZE_NxN && depth == m_data[abs_part_idx].depth )
    {
        assert( subdiv );
    }
    else if (m_data[abs_part_idx].predMode == MODE_INTER &&
        (m_data[abs_part_idx].partSize != PART_SIZE_2Nx2N) &&
        depth == m_data[abs_part_idx].depth &&
        (m_par->QuadtreeTUMaxDepthInter == 1))
    {
        if (Log2TrafoSize > H265Enc::GetQuadtreeTuLog2MinSizeInCu(m_par, m_par->Log2MaxCUSize - m_data[abs_part_idx].depth,
            m_data[abs_part_idx].partSize, m_data[abs_part_idx].predMode))
        {
            assert( subdiv );
        }
        else
        {
            assert(!subdiv );
        }
    }
    else if (Log2TrafoSize > m_par->QuadtreeTULog2MaxSize)
    {
        assert( subdiv );
    }
    else if (Log2TrafoSize == m_par->QuadtreeTULog2MinSize)
    {
        assert( !subdiv );
    }
    else if (Log2TrafoSize == H265Enc::GetQuadtreeTuLog2MinSizeInCu(m_par, m_par->Log2MaxCUSize - m_data[abs_part_idx].depth,
            m_data[abs_part_idx].partSize, m_data[abs_part_idx].predMode))
    {
        assert( !subdiv );
    }
    else
    {
        assert( Log2TrafoSize > H265Enc::GetQuadtreeTuLog2MinSizeInCu(m_par, m_par->Log2MaxCUSize - m_data[abs_part_idx].depth,
            m_data[abs_part_idx].partSize, m_data[abs_part_idx].predMode) );
        bs->EncodeSingleBin_CABAC(CTX(bs,TRANS_SUBDIV_FLAG_HEVC) + 5 - Log2TrafoSize, subdiv);
    }

    const Ipp32u tr_depth_cur = depth - m_data[abs_part_idx].depth;
    const bool first_cbf_of_cu = tr_depth_cur == 0;
    if (first_cbf_of_cu || (Log2TrafoSize > 2 && m_par->chromaFormatIdc != MFX_CHROMAFORMAT_YUV444))
    {
        Ipp32s part_offset =  (m_par->NumPartInCU >> (((depth + 1) << 1) - 1));
        if (first_cbf_of_cu || GetCbf( abs_part_idx, TEXT_CHROMA_U, tr_depth_cur - 1 ))
        {
            bs->EncodeSingleBin_CABAC(CTX(bs,QT_CBF_HEVC) +
                NUM_QT_CBF_CTX +
                GetCtxQtCbfChroma(tr_depth_cur),
                GetCbf     ( abs_part_idx, TEXT_CHROMA_U, tr_depth_cur ));
            if (m_par->chroma422 && (!subdiv || width == 8)) {
                cbfU1 = GetCbf( abs_part_idx + part_offset, TEXT_CHROMA_U, tr_depth_cur );
                bs->EncodeSingleBin_CABAC(CTX(bs,QT_CBF_HEVC) +
                    NUM_QT_CBF_CTX +
                    GetCtxQtCbfChroma(tr_depth_cur),
                    cbfU1);
            }
        }
        if (first_cbf_of_cu || GetCbf( abs_part_idx, TEXT_CHROMA_V, tr_depth_cur - 1 ))
        {
            bs->EncodeSingleBin_CABAC(CTX(bs,QT_CBF_HEVC) +
                NUM_QT_CBF_CTX +
                GetCtxQtCbfChroma(tr_depth_cur),
                GetCbf     ( abs_part_idx, TEXT_CHROMA_V, tr_depth_cur ));
            if (m_par->chroma422 && (!subdiv || width == 8)) {
                cbfV1 = GetCbf( abs_part_idx + part_offset, TEXT_CHROMA_V, tr_depth_cur );
                bs->EncodeSingleBin_CABAC(CTX(bs,QT_CBF_HEVC) +
                    NUM_QT_CBF_CTX +
                    GetCtxQtCbfChroma(tr_depth_cur),
                    cbfV1);
            }
        }
    }
    else if(Log2TrafoSize == 2)
    {
        Ipp32s abs_part_idx_0 = abs_part_idx & ~part_num_mask;
        assert( GetCbf( abs_part_idx_0, TEXT_CHROMA_U, tr_depth_cur ) == GetCbf( abs_part_idx_0, TEXT_CHROMA_U, tr_depth_cur - 1 ) );
        assert( GetCbf( abs_part_idx_0, TEXT_CHROMA_V, tr_depth_cur ) == GetCbf( abs_part_idx_0, TEXT_CHROMA_V, tr_depth_cur - 1 ) );
    }

    if (subdiv)
    {
        Ipp32u size;
        width  >>= 1;
        size = width*width;
        tr_idx++;
        ++depth;
        const Ipp32u part_num = m_par->NumPartInCU >> (depth << 1);

        Ipp32u nsAddr = abs_part_idx;
        PutTransform( bs, offset_luma, offset_chroma, abs_part_idx, nsAddr, depth, width, tr_idx, code_dqp, rd_mode );

        abs_part_idx += part_num;  offset_luma += size;  offset_chroma += (size>>m_par->chromaShift);
        PutTransform( bs, offset_luma, offset_chroma, abs_part_idx, nsAddr, depth, width, tr_idx, code_dqp, rd_mode );

        abs_part_idx += part_num;  offset_luma += size;  offset_chroma += (size>>m_par->chromaShift);
        PutTransform( bs, offset_luma, offset_chroma, abs_part_idx, nsAddr, depth, width, tr_idx, code_dqp, rd_mode );

        abs_part_idx += part_num;  offset_luma += size;  offset_chroma += (size>>m_par->chromaShift);
        PutTransform( bs, offset_luma, offset_chroma, abs_part_idx, nsAddr, depth, width, tr_idx, code_dqp, rd_mode );
    }
    else
    {
        Ipp32u luma_tr_mode, chroma_tr_mode;
        ConvertTransIdx( m_data[abs_part_idx].trIdx, luma_tr_mode, chroma_tr_mode );

        if (m_data[abs_part_idx].predMode != MODE_INTRA && depth == m_data[abs_part_idx].depth &&
            !GetCbf( abs_part_idx, TEXT_CHROMA_U, 0 ) && !GetCbf( abs_part_idx, TEXT_CHROMA_V, 0 ) && (Log2TrafoSize == 2 || (!cbfU1 && !cbfV1)))
        {
            assert( GetCbf( abs_part_idx, TEXT_LUMA, 0 ) );
        }
        else
        {
            bs->EncodeSingleBin_CABAC(CTX(bs,QT_CBF_HEVC) +
                GetCtxQtCbfLuma(luma_tr_mode),
                GetCbf     ( abs_part_idx, TEXT_LUMA, luma_tr_mode ));
        }
//    printf("CBFY: %d\n",cbfY);

        if (cbfY || cbfU || cbfV || (m_par->chroma422 && (cbfU1 || cbfV1)) )
        {
            // dQP: only for LCU once
            if (m_par->UseDQP)
            {
                if (code_dqp)
                {
                    h265_code_delta_qp(bs, this, m_bakAbsPartIdxCu, m_par->bitDepthLuma);
                    code_dqp = 0;
                }
            }
        }

        if (rd_mode == RD_CU_ALL_EXCEPT_COEFFS) return;

        if (cbfY)
        {
            Ipp32s tr_width = width;
            CodeCoeffNxN(bs, this, m_coeffWorkY + offset_luma, abs_part_idx, tr_width, TEXT_LUMA );
        }
        if (Log2TrafoSize > 2)
        {
            Ipp32s tr_width = width >> 1;
            Ipp32s tr_block_size = tr_width * tr_width;
            if (cbfU)
            {
                CodeCoeffNxN(bs, this, m_coeffWorkU + offset_chroma, abs_part_idx, tr_width, TEXT_CHROMA_U );
            }
            if (m_par->chroma422&& cbfU1) {
                CodeCoeffNxN(bs, this, m_coeffWorkU + offset_chroma + tr_block_size, abs_part_idx, tr_width, TEXT_CHROMA_U );
            }
            if (cbfV)
            {
                CodeCoeffNxN(bs, this, m_coeffWorkV + offset_chroma, abs_part_idx, tr_width, TEXT_CHROMA_V );
            }
            if (m_par->chroma422 && cbfV1) {
                CodeCoeffNxN(bs, this, m_coeffWorkV + offset_chroma + tr_block_size, abs_part_idx, tr_width, TEXT_CHROMA_V );
            }
        }
        else
        {
            Ipp32u offset_mask = part_num_mask << (4 - m_par->chromaShift);
            if ((abs_part_idx & part_num_mask ) == part_num_mask)
            {
                Ipp32s tr_width = width;
                Ipp32s tr_block_size = tr_width * tr_width;
                if (cbfU)
                {
                    CodeCoeffNxN(bs, this, m_coeffWorkU + (offset_chroma & ~offset_mask), (abs_part_idx & ~part_num_mask), tr_width, TEXT_CHROMA_U);
                }
                if (m_par->chroma422 && cbfU1) {
                    CodeCoeffNxN(bs, this, m_coeffWorkU + (offset_chroma & ~offset_mask) + tr_block_size, (abs_part_idx & ~part_num_mask), tr_width, TEXT_CHROMA_U );
                }
                if (cbfV)
                {
                    CodeCoeffNxN(bs, this, m_coeffWorkV + (offset_chroma & ~offset_mask), (abs_part_idx & ~part_num_mask), tr_width, TEXT_CHROMA_V);
                }
                if (m_par->chroma422 && cbfV1) {
                    CodeCoeffNxN(bs, this, m_coeffWorkV + (offset_chroma & ~offset_mask) + tr_block_size, (abs_part_idx & ~part_num_mask), tr_width, TEXT_CHROMA_V );
                }
            }
        }
    }
}

template <typename PixType>
template <class H265Bs>
void H265CU<PixType>::EncodeCoeff(H265Bs* bs, Ipp32s abs_part_idx, Ipp32u depth, Ipp32u width, Ipp8u &code_dqp, Ipp8u rd_mode)
{
    Ipp32u min_coeff_size = m_par->MinTUSize * m_par->MinTUSize;
    Ipp32u luma_offset   = min_coeff_size*abs_part_idx;
    Ipp32u chroma_offset = luma_offset>>m_par->chromaShift;

    Ipp32u luma_tr_mode, chroma_tr_mode;
    luma_tr_mode = chroma_tr_mode = m_data[abs_part_idx].trIdx;

    if (m_data[abs_part_idx].predMode == MODE_INTER)
    {
        if (!(m_data[abs_part_idx].flags.mergeFlag && m_data[abs_part_idx].partSize == PART_SIZE_2Nx2N ))
        {
            h265_code_qt_root_cbf(bs, this, abs_part_idx );
            if (!GetQtRootCbf( abs_part_idx ))
            {
                return;
            }
        }
    }

    PutTransform(bs, luma_offset, chroma_offset, abs_part_idx, abs_part_idx, depth, width, 0, code_dqp, rd_mode);
}


static
Ipp32u h265_PU_offset[8] = { 0, 8, 4, 4, 2, 10, 1, 5};

template <typename PixType, class H265Bs>
static
void h265_encode_PU_inter(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx)
{
  Ipp8u part_size = pCU->m_data[abs_part_idx].partSize;
  Ipp32u num_PU = (part_size == PART_SIZE_2Nx2N ? 1 : (part_size == PART_SIZE_NxN ? 4 : 2));
  Ipp32u depth = pCU->m_data[abs_part_idx].depth;
  assert(part_size < PART_SIZE_NONE);
  Ipp32u offset = (h265_PU_offset[part_size] << ((pCU->m_par->MaxCUDepth - depth) << 1)) >> 4;

  Ipp32u sub_part_idx = abs_part_idx;
  for (Ipp32u part_idx = 0; part_idx < num_PU; part_idx++)
  {
    h265_code_merge_flag(bs, pCU, sub_part_idx);
    if (pCU->m_data[sub_part_idx].flags.mergeFlag)
    {
        h265_code_merge_index(bs, pCU, sub_part_idx);
    }
    else
    {
        if (pCU->m_cslice->slice_type == B_SLICE) {
            h265_code_interdir(bs, pCU, sub_part_idx);
        }
        for (Ipp8u ref_list_idx = 0; ref_list_idx < 2; ref_list_idx++)
        {
            Ipp32s num_ref_idx = pCU->m_cslice->num_ref_idx[ref_list_idx];
            if (num_ref_idx > 0 && (pCU->m_data[sub_part_idx].interDir & (1 << ref_list_idx))) {
                if (num_ref_idx > 1) {
                    h265_code_reffrm_idx(bs, pCU, sub_part_idx, (EnumRefPicList)ref_list_idx);
                }
                h265_code_mvd(bs, pCU, sub_part_idx, (EnumRefPicList)ref_list_idx);
                h265_code_mvp_idx(bs, pCU, sub_part_idx, (EnumRefPicList)ref_list_idx);
            }
        }
    }
    sub_part_idx += offset;
  }
}

template <typename PixType, class H265Bs>
static
void h265_encode_pred_info(H265Bs *bs, H265CU<PixType>* pCU, Ipp32s abs_part_idx)
{
    if (pCU->IsIntra( abs_part_idx ))
    {
        pCU->CodeIntradirLumaAng(bs, abs_part_idx, true);
        h265_code_intradir_chroma(bs, pCU, abs_part_idx);
    }
    else
    {
        h265_encode_PU_inter(bs, pCU, abs_part_idx);
    }
}


template <typename PixType>
CostType H265CU<PixType>::GetSkipModeCost(H265CUData *data, Ipp32s absPartIdx, Ipp32s depth)
{
    m_bsf->Reset();
    m_data = data;

    if ((m_par->MaxCUSize>>depth) >= m_par->MinCuDQPSize && m_par->UseDQP)
        setdQPFlag(true);

    Ipp32u ctx;
    H265CUPtr left, above;
    GetPuLeft(&left, m_absIdxInLcu + absPartIdx);
    GetPuAbove(&above, m_absIdxInLcu + absPartIdx);

    //h265_code_split_flag(m_bsf, this, absPartIdx, depth);
    if (depth != m_par->MaxCUDepth - m_par->AddCUDepth) {
        ctx = (left.ctbData && left.ctbData[left.absPartIdx].depth > depth);
        ctx += (above.ctbData && above.ctbData[above.absPartIdx].depth > depth);
        m_bsf->EncodeSingleBin_CABAC(CTX(m_bsf,SPLIT_CODING_UNIT_FLAG_HEVC)+ctx , 0);
    }

    //h265_code_skip_flag(m_bsf, this, absPartIdx);
    ctx = (left.ctbData && isSkipped(left.ctbData, left.absPartIdx));
    ctx += (above.ctbData && isSkipped(above.ctbData, above.absPartIdx));
    m_bsf->EncodeSingleBin_CABAC(CTX(m_bsf,SKIP_FLAG_HEVC)+ctx, 1);

    h265_code_merge_index(m_bsf, this, absPartIdx);

    return BIT_COST(m_bsf->GetNumBits());
}
template CostType H265CU<Ipp8u>::GetSkipModeCost(H265CUData *data, Ipp32s absPartIdx, Ipp32s depth);
template CostType H265CU<Ipp16u>::GetSkipModeCost(H265CUData *data, Ipp32s absPartIdx, Ipp32s depth);


template <typename PixType>
CostType H265CU<PixType>::GetInterModeCost(H265CUData *data, Ipp32s absPartIdx, Ipp32s depth)
{
    m_bsf->Reset();
    m_data = data;

    if ((m_par->MaxCUSize>>depth) >= m_par->MinCuDQPSize && m_par->UseDQP)
        setdQPFlag(true);

    Ipp32u ctx;
    H265CUPtr left, above;
    GetPuLeft(&left, m_absIdxInLcu + absPartIdx);
    GetPuAbove(&above, m_absIdxInLcu + absPartIdx);

    //h265_code_split_flag(m_bsf, this, absPartIdx, depth );
    if (depth != m_par->MaxCUDepth - m_par->AddCUDepth) {
        ctx = (left.ctbData && left.ctbData[left.absPartIdx].depth > depth);
        ctx += (above.ctbData && above.ctbData[above.absPartIdx].depth > depth);
        m_bsf->EncodeSingleBin_CABAC(CTX(m_bsf,SPLIT_CODING_UNIT_FLAG_HEVC)+ctx, 0);
    }

    //h265_code_skip_flag(m_bsf, this, absPartIdx );
    ctx = (left.ctbData && isSkipped(left.ctbData, left.absPartIdx));
    ctx += (above.ctbData && isSkipped(above.ctbData, above.absPartIdx));
    m_bsf->EncodeSingleBin_CABAC(CTX(m_bsf,SKIP_FLAG_HEVC)+ctx, 0);

    h265_code_pred_mode(m_bsf, this, absPartIdx );
    h265_code_part_size(m_bsf, this, absPartIdx, depth );
    h265_encode_pred_info(m_bsf, this, absPartIdx );

    return BIT_COST(m_bsf->GetNumBits());
}
template CostType H265CU<Ipp8u>::GetInterModeCost(H265CUData *data, Ipp32s absPartIdx, Ipp32s depth);
template CostType H265CU<Ipp16u>::GetInterModeCost(H265CUData *data, Ipp32s absPartIdx, Ipp32s depth);


template <typename PixType>
template <class H265Bs>
void H265CU<PixType>::EncodeCU(H265Bs *bs, Ipp32s abs_part_idx, Ipp32s depth, Ipp8u rd_mode )
{
    H265CU *pCU = this;

    Ipp8u boundary = false;
    Ipp32u lPelX   = m_ctbPelX +
        ((h265_scan_z2r4[abs_part_idx] & 15) << m_par->QuadtreeTULog2MinSize);
    Ipp32u rPelX   = lPelX + (m_par->MaxCUSize>>depth)  - 1;
    Ipp32u tPelY   = m_ctbPelY +
        ((h265_scan_z2r4[abs_part_idx] >> 4) << m_par->QuadtreeTULog2MinSize);
    Ipp32u bPelY   = tPelY + (m_par->MaxCUSize>>depth) - 1;
    /*
    if( getCheckBurstIPCMFlag() )
    {
    setLastCUSucIPCMFlag( checkLastCUSucIPCM( pCU, abs_part_idx ));
    setNumSucIPCM( countNumSucIPCM ( pCU, abs_part_idx ) );
    }
    */
    //  TComSlice * pcSlice = getPic()->getSlice(getPic()->getCurrSliceIdx());

    // If slice start is within this cu...
    Ipp8u slice_start = false;//pcSlice->getDependentSliceCurStartCUAddr() > pcPic->getPicSym()->getInverseCUOrderMap(getAddr())*getPic()->getNumPartInCU()+abs_part_idx &&
    //    pcSlice->getDependentSliceCurStartCUAddr() < pcPic->getPicSym()->getInverseCUOrderMap(getAddr())*getPic()->getNumPartInCU()+abs_part_idx+( pcPic->getNumPartInCU() >> (depth<<1) );
    // We need to split, so don't try these modes.
    if (!slice_start && (rPelX < m_par->Width) && (bPelY < m_par->Height))
    {
        h265_code_split_flag(bs, pCU, abs_part_idx, depth );
    }
    else
    {
        boundary = true;
    }

    {
        if ((m_par->MaxCUSize>>depth) >= m_par->MinCuDQPSize && m_par->UseDQP)
        {
            setdQPFlag(true);
        }
    }
    if (rd_mode == RD_CU_SPLITFLAG) return;

    if (((depth < m_data[abs_part_idx].depth) && (depth < (m_par->MaxCUDepth-m_par->AddCUDepth))) || boundary)
    {
        Ipp32u num_parts = ( m_par->NumPartInCU >> (depth<<1) )>>2;
        if ((m_par->MaxCUSize>>depth) == m_par->MinCuDQPSize  && m_par->UseDQP)
        {
            //      setdQPFlag(true);
        }
        //    setNumSucIPCM(0);
        //    setLastCUSucIPCMFlag(false);
        for (Ipp32u part_unit_idx = 0; part_unit_idx < 4; part_unit_idx++, abs_part_idx+=num_parts)
        {
            lPelX   = m_ctbPelX +
                ((h265_scan_z2r4[abs_part_idx] & 15) << m_par->QuadtreeTULog2MinSize);
            tPelY   = m_ctbPelY +
                ((h265_scan_z2r4[abs_part_idx] >> 4) << m_par->QuadtreeTULog2MinSize);
            if ((lPelX < m_par->Width) && (tPelY < m_par->Height))
            {
                EncodeCU(bs, abs_part_idx, depth+1, rd_mode);
            }
        }
        return;
    }

    if ((m_par->MaxCUSize>>depth) >= m_par->MinCuDQPSize && m_par->UseDQP)
    {
        //    setdQPFlag(true);
    }

    if (m_par->transquantBypassEnableFlag)
        h265_code_transquant_bypass_flag(bs, pCU, abs_part_idx );

    Ipp8u skipped_flag;
    Ipp32u i, num_parts;
    if(m_cslice->slice_type != I_SLICE)
    {
        skipped_flag = m_data[abs_part_idx].predMode == MODE_INTER &&
            m_data[abs_part_idx].partSize == PART_SIZE_2Nx2N && m_data[abs_part_idx].flags.mergeFlag &&
            !GetQtRootCbf(abs_part_idx);
        num_parts = ( m_par->NumPartInCU >> (depth<<1) );

        for (Ipp32u i = 0; i < num_parts; i++)
            m_data[abs_part_idx + i].flags.skippedFlag = skipped_flag;

        h265_code_skip_flag(bs, pCU, abs_part_idx );
        if (skipped_flag)
        {
            h265_code_merge_index(bs, pCU, abs_part_idx);
            //    finishCU(pCU,abs_part_idx,depth);
            return;
        }
        h265_code_pred_mode(bs, pCU, abs_part_idx );
    }

    h265_code_part_size(bs, pCU, abs_part_idx, depth );

    if (m_data[abs_part_idx].predMode == MODE_INTRA &&
        m_data[abs_part_idx].partSize == PART_SIZE_2Nx2N )
    {
        // IPCM fixme
    }

    if (rd_mode == RD_CU_MODES && m_data[abs_part_idx].predMode == MODE_INTRA) return;

    // prediction Info ( Intra : direction mode, Inter : Mv, reference idx )
    h265_encode_pred_info(bs, pCU, abs_part_idx );

    if (rd_mode == RD_CU_MODES) return;

    // Encode Coefficients
    Ipp8u code_dqp = getdQPFlag();
    EncodeCoeff( bs, abs_part_idx, depth, m_data[abs_part_idx].size, code_dqp, rd_mode );
    setdQPFlag( code_dqp );

    // --- write terminating bit ---
    //  finishCU(pCU,abs_part_idx,depth);
}

template void H265CU<Ipp8u>::EncodeCU(H265BsReal *bs, Ipp32s abs_part_idx, Ipp32s depth, Ipp8u rd_mode );
template void H265CU<Ipp8u>::EncodeCU(H265BsFake *bs, Ipp32s abs_part_idx, Ipp32s depth, Ipp8u rd_mode );
template void H265CU<Ipp16u>::EncodeCU(H265BsReal *bs, Ipp32s abs_part_idx, Ipp32s depth, Ipp8u rd_mode );
template void H265CU<Ipp16u>::EncodeCU(H265BsFake *bs, Ipp32s abs_part_idx, Ipp32s depth, Ipp8u rd_mode );


// ========================================================
// CABAC SAO
// ========================================================

template <class H265Bs>
void h265_code_sao_merge(H265Bs *bs, Ipp32u code)
{
    bs->EncodeSingleBin_CABAC(CTX(bs,SAO_MERGE_FLAG_HEVC),code);
}

template <class H265Bs>
void CodeSaoCtbParam(
    H265Bs *bs,
    SaoCtuParam& saoBlkParam,
    bool* sliceEnabled,
    bool leftMergeAvail,
    bool aboveMergeAvail,
    bool onlyEstMergeInfo)
{
    bool isLeftMerge = false;
    bool isAboveMerge= false;
    Ipp32u code = 0;

    if(leftMergeAvail)
    {
        isLeftMerge = ((saoBlkParam[SAO_Y].mode_idx == SAO_MODE_MERGE) && (saoBlkParam[SAO_Y].type_idx == SAO_MERGE_LEFT));
        code = isLeftMerge ? 1 : 0;
        h265_code_sao_merge(bs, code);
    }

    if( aboveMergeAvail && !isLeftMerge)
    {
        isAboveMerge = ((saoBlkParam[SAO_Y].mode_idx == SAO_MODE_MERGE) && (saoBlkParam[SAO_Y].type_idx == SAO_MERGE_ABOVE));
        code = isAboveMerge ? 1 : 0;
        h265_code_sao_merge(bs, code);
    }

    if(onlyEstMergeInfo)
    {
        return; //only for RDO
    }

    if(!isLeftMerge && !isAboveMerge) //not merge mode
    {
        for(int compIdx=0; compIdx < NUM_SAO_COMPONENTS; compIdx++)
        {
            CodeSaoCtbOffsetParam(bs, compIdx, saoBlkParam[compIdx], sliceEnabled[compIdx]);
        }
    }

} // void CodeSaoCtbParam( ... )


template <class H265Bs>
void h265_code_sao_sign(H265Bs *bs,  Ipp32u code )
{
    bs->EncodeBinEP_CABAC(code);
}

template <class H265Bs>
void h265_code_sao_max_uvlc (H265Bs *bs,  Ipp32u code, Ipp32u max_symbol)
{
    if (max_symbol == 0)
    {
        return;
    }

    Ipp32u i;
    Ipp8u code_last = (max_symbol > code);

    if (code == 0)
    {
        bs->EncodeBinEP_CABAC(0);
    }
    else
    {
        bs->EncodeBinEP_CABAC(1);
        for (i=0; i < code-1; i++)
        {
            bs->EncodeBinEP_CABAC(1);
        }
        if (code_last)
        {
            bs->EncodeBinEP_CABAC(0);
        }
    }
}

template <class H265Bs>
void h265_code_sao_uvlc(H265Bs *bs, Ipp32u code)
{
    bs->EncodeBinsEP_CABAC(code, 5);
}


template <class H265Bs>
void h265_code_sao_uflc(H265Bs *bs, Ipp32u code, Ipp32u length)
{
    bs->EncodeBinsEP_CABAC(code, length);
}


template <class H265Bs>
void h265_code_sao_type_idx(H265Bs *bs, Ipp32u code)
{
    if (code == 0)
    {
        bs->EncodeSingleBin_CABAC(CTX(bs,SAO_TYPE_IDX_HEVC),0);
    }
    else
    {
        bs->EncodeSingleBin_CABAC(CTX(bs,SAO_TYPE_IDX_HEVC),1);
        {
            bs->EncodeBinEP_CABAC( code == 1 ? 0 : 1 );
        }
    }

} // void h265_code_sao_type_idx(H265Bs *bs, Ipp32u code)


template <class H265Bs>
void CodeSaoCtbOffsetParam(H265Bs *bs, int compIdx, SaoOffsetParam& ctbParam, bool sliceEnabled)
{
    Ipp32u code;
    if(!sliceEnabled)
    {
        return;
    }

    //type
    if(compIdx == SAO_Y || compIdx == SAO_Cb)
    {
        if(ctbParam.mode_idx == SAO_MODE_OFF)
        {
            code =0;
        }
        else if(ctbParam.type_idx == SAO_TYPE_BO) //BO
        {
            code = 1;
        }
        else
        {
            assert(ctbParam.type_idx < SAO_TYPE_BO); //EO
            code = 2;
        }
        h265_code_sao_type_idx(bs, code);
    }

    if(ctbParam.mode_idx == SAO_MODE_ON)
    {
        int numClasses = (ctbParam.type_idx == SAO_TYPE_BO)?4:NUM_SAO_EO_CLASSES;
        int offset[5]; // only 4 are used, 5th is added for KW
        int k=0;
        for(int i=0; i< numClasses; i++)
        {
            if(ctbParam.type_idx != SAO_TYPE_BO && i == SAO_CLASS_EO_PLAIN)
            {
                continue;
            }
            int classIdx = (ctbParam.type_idx == SAO_TYPE_BO)?(  (ctbParam.typeAuxInfo+i)% NUM_SAO_BO_CLASSES   ):i;
            offset[k] = ctbParam.offset[classIdx];
            k++;
        }

        for(int i=0; i< 4; i++)
        {

            code = (Ipp32u)( offset[i] < 0) ? (-offset[i]) : (offset[i]);
            h265_code_sao_max_uvlc (bs,  code, ctbParam.saoMaxOffsetQVal);
        }


        if(ctbParam.type_idx == SAO_TYPE_BO)
        {
            for(int i=0; i< 4; i++)
            {
                if(offset[i] != 0)
                {
                    h265_code_sao_sign(bs,  (offset[i] < 0) ? 1 : 0);
                }
            }

            h265_code_sao_uflc(bs, ctbParam.typeAuxInfo, NUM_SAO_BO_CLASSES_LOG2);
        }
        else //EO
        {
            if(compIdx == SAO_Y || compIdx == SAO_Cb)
            {
                assert(ctbParam.type_idx - SAO_TYPE_EO_0 >=0);
                h265_code_sao_uflc(bs, ctbParam.type_idx - SAO_TYPE_EO_0, NUM_SAO_EO_TYPES_LOG2);
            }
        }
    }

} // Void CodeSaoCtbOffsetParam(Int compIdx, SaoOffsetParam& ctbParam, Bool sliceEnabled)


template <typename PixType>
template <class H265Bs>
void H265CU<PixType>::EncodeSao(
    H265Bs *bs,
    Ipp32s abs_part_idx,
    Ipp32s depth,
    Ipp8u rd_mode,
    SaoCtuParam& saoBlkParam,
    bool leftMergeAvail,
    bool aboveMergeAvail)
{
    bool sliceEnabled[NUM_SAO_COMPONENTS] = {false, false, false};
    bool onlyEstMergeInfo = false;

    if( m_cslice->slice_sao_luma_flag )
    {
        sliceEnabled[SAO_Y] = true;
    }
    if( m_cslice->slice_sao_chroma_flag )
    {
        sliceEnabled[SAO_Cb] = sliceEnabled[SAO_Cr] = true;
    }

    CodeSaoCtbParam(
        bs,
        saoBlkParam,
        sliceEnabled,
        leftMergeAvail,
        aboveMergeAvail,
        onlyEstMergeInfo);

} // template <class H265Bs> void EncodeSao( ... )

template void CodeSaoCtbParam(H265BsReal *bs, SaoCtuParam& saoBlkParam, bool* sliceEnabled, bool leftMergeAvail, bool aboveMergeAvail, bool onlyEstMergeInfo);
template void CodeSaoCtbParam(H265BsFake *bs, SaoCtuParam& saoBlkParam, bool* sliceEnabled, bool leftMergeAvail, bool aboveMergeAvail, bool onlyEstMergeInfo);
template void CodeSaoCtbOffsetParam(H265BsReal *bs, int compIdx, SaoOffsetParam& ctbParam, bool sliceEnabled);
template void CodeSaoCtbOffsetParam(H265BsFake *bs, int compIdx, SaoOffsetParam& ctbParam, bool sliceEnabled);

template void H265CU<Ipp8u>::EncodeSao(H265BsReal *bs, Ipp32s abs_part_idx, Ipp32s depth, Ipp8u rd_mode, SaoCtuParam& blkParam, bool leftMergeAvail, bool aboveMergeAvail );
template void H265CU<Ipp8u>::EncodeSao(H265BsFake *bs, Ipp32s abs_part_idx, Ipp32s depth, Ipp8u rd_mode, SaoCtuParam& blkParam, bool leftMergeAvail, bool aboveMergeAvail );
template void H265CU<Ipp16u>::EncodeSao(H265BsReal *bs, Ipp32s abs_part_idx, Ipp32s depth, Ipp8u rd_mode, SaoCtuParam& blkParam, bool leftMergeAvail, bool aboveMergeAvail );
template void H265CU<Ipp16u>::EncodeSao(H265BsFake *bs, Ipp32s abs_part_idx, Ipp32s depth, Ipp8u rd_mode, SaoCtuParam& blkParam, bool leftMergeAvail, bool aboveMergeAvail );
template void H265CU<Ipp8u>::CodeIntradirLumaAng(H265BsReal *bs, Ipp32s absPartIdx, Ipp8u multiple);
template void H265CU<Ipp8u>::CodeIntradirLumaAng(H265BsFake *bs, Ipp32s absPartIdx, Ipp8u multiple);
template void H265CU<Ipp16u>::CodeIntradirLumaAng(H265BsReal *bs, Ipp32s absPartIdx, Ipp8u multiple);
template void H265CU<Ipp16u>::CodeIntradirLumaAng(H265BsFake *bs, Ipp32s absPartIdx, Ipp8u multiple);
template CostType H265CU<Ipp8u>::GetIntraChromaModeCost(Ipp32s absPartIdx);
template CostType H265CU<Ipp16u>::GetIntraChromaModeCost(Ipp32s absPartIdx);
template void H265CU<Ipp8u>::PutLumaTu(H265BsFake *bs, Ipp32u offsetLuma, Ipp32s absPartIdx, Ipp32u depth, Ipp32u width, Ipp32u trIdx);
template void H265CU<Ipp16u>::PutLumaTu(H265BsFake *bs, Ipp32u offsetLuma, Ipp32s absPartIdx, Ipp32u depth, Ipp32u width, Ipp32u trIdx);

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE

