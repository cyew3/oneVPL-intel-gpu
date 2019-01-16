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

#include "mfx_av1_defs.h"

namespace H265Enc {
    extern const vpx_prob kf_partition_probs[VP9_PARTITION_CONTEXTS][PARTITION_TYPES - 1];
    extern const vpx_prob vp9_kf_y_mode_probs[INTRA_MODES][INTRA_MODES][INTRA_MODES - 1];
    extern const vpx_prob av1_kf_y_mode_probs[AV1_INTRA_MODES][AV1_INTRA_MODES][AV1_INTRA_MODES - 1];
    extern const vpx_prob kf_uv_mode_probs[INTRA_MODES][INTRA_MODES - 1];
    extern const vpx_prob default_cat_probs[7][14];
    extern const vpx_prob default_partition_probs[VP9_PARTITION_CONTEXTS][PARTITION_TYPES - 1];
    extern const vpx_prob default_skip_prob[SKIP_CONTEXTS];
    extern const vpx_prob default_is_inter_prob[IS_INTER_CONTEXTS];
    extern const vpx_prob default_tx_probs[TX_SIZES][TX_SIZE_CONTEXTS][TX_SIZES - 1];
    extern const vpx_prob default_txfm_partition_probs[TXFM_PARTITION_CONTEXTS];
    extern const vpx_prob vp9_default_y_mode_probs[BLOCK_SIZE_GROUPS][INTRA_MODES - 1];
    extern const vpx_prob vp9_default_uv_mode_probs[INTRA_MODES][INTRA_MODES - 1];
    extern const vpx_prob default_coef_probs[TX_SIZES][BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS][UNCONSTRAINED_NODES];
    extern const vpx_prob vp9_default_single_ref_prob[VP9_REF_CONTEXTS][2];
    extern const vpx_prob av1_default_single_ref_prob[VP9_REF_CONTEXTS][SINGLE_REFS - 1];
    extern const vpx_prob default_inter_mode_probs[INTER_MODE_CONTEXTS][INTER_MODES - 1];

    extern const vpx_prob default_interp_filter_probs_av1[AV1_INTERP_FILTER_CONTEXTS][SWITCHABLE_FILTERS - 1];
    extern const vpx_prob default_interp_filter_probs_vp9[VP9_INTERP_FILTER_CONTEXTS][SWITCHABLE_FILTERS - 1];

    extern const vpx_prob default_mv_joint_probs[3];
    extern const vpx_prob default_mv_sign_prob[2];
    extern const vpx_prob default_mv_class_probs[2][VP9_MV_CLASSES - 1];
    extern const vpx_prob default_mv_class0_bit_prob[2];
    extern const vpx_prob default_mv_class0_fr_probs[2][CLASS0_SIZE][3];
    extern const vpx_prob default_mv_bits_prob[2][MV_OFFSET_BITS];
    extern const vpx_prob default_mv_class0_hp_prob[2];
    extern const vpx_prob default_mv_fr_probs[2][3];
    extern const vpx_prob default_mv_hp_prob[2];
    extern const vpx_prob default_comp_mode_probs[COMP_MODE_CONTEXTS];
    extern const vpx_prob default_comp_ref_type_probs[COMP_REF_TYPE_CONTEXTS];
    extern const vpx_prob av1_default_comp_ref_probs[VP9_REF_CONTEXTS][FWD_REFS - 1];
    extern const vpx_prob default_comp_bwdref_probs[VP9_REF_CONTEXTS][BWD_REFS - 1];
    extern const vpx_prob vp9_default_comp_ref_probs[VP9_REF_CONTEXTS];
    extern const vpx_prob default_intra_ext_tx_prob[EXT_TX_SIZES][TX_TYPES][TX_TYPES - 1];
    extern const vpx_prob default_inter_ext_tx_prob[EXT_TX_SIZES][TX_TYPES - 1];
    extern const vpx_prob default_newmv_prob[NEWMV_MODE_CONTEXTS];
    extern const vpx_prob default_zeromv_prob[GLOBALMV_MODE_CONTEXTS];
    extern const vpx_prob default_refmv_prob[REFMV_MODE_CONTEXTS];
    extern const vpx_prob default_drl_prob[DRL_MODE_CONTEXTS];
    extern const vpx_prob default_obmc_prob[BLOCK_SIZES_ALL];
    extern const vpx_prob pareto_table[128][8];
    extern const vpx_prob pareto_full_table[256][8];
    extern const aom_cdf_prob av1_pareto8_tail_probs[COEFF_PROB_MODELS][TAIL_NODES];
    extern const MvContextsVp9 vp9_default_nmv_context;
    extern const MvContextsAv1 av1_default_nmv_context;
    extern const aom_cdf_prob default_if_y_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(AV1_INTRA_MODES)];
    extern const aom_cdf_prob default_uv_mode_cdf[AV1_INTRA_MODES][CDF_SIZE(AV1_UV_INTRA_MODES)];
    extern const aom_cdf_prob default_angle_delta_cdf[DIRECTIONAL_MODES][CDF_SIZE(2 * MAX_ANGLE_DELTA + 1)];
    extern const aom_cdf_prob default_filter_intra_cdfs[TX_SIZES_ALL][CDF_SIZE(2)];
    extern const aom_cdf_prob default_switchable_interp_cdf[SWITCHABLE_FILTER_CONTEXTS][CDF_SIZE(SWITCHABLE_FILTERS)];
    extern const aom_cdf_prob default_partition_cdf[AV1_PARTITION_CONTEXTS][CDF_SIZE(EXT_PARTITION_TYPES)];
    extern const aom_cdf_prob default_inter_mode_cdf[INTER_MODE_CONTEXTS][CDF_SIZE(INTER_MODES)];
    extern const aom_cdf_prob default_intra_ext_tx_cdf[EXT_TX_SETS_INTRA][EXT_TX_SIZES][AV1_INTRA_MODES][CDF_SIZE(TX_TYPES)];
    extern const aom_cdf_prob default_inter_ext_tx_cdf[EXT_TX_SETS_INTER][EXT_TX_SIZES][CDF_SIZE(TX_TYPES)];
    extern const aom_cdf_prob default_tx_size_cdf[MAX_TX_CATS][TX_SIZE_CONTEXTS][CDF_SIZE(MAX_TX_DEPTH + 1)];
    extern const aom_cdf_prob default_motion_mode_cdf[BLOCK_SIZES_ALL][CDF_SIZE(MOTION_MODES)];
    extern const aom_cdf_prob default_obmc_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)];
    extern const aom_cdf_prob default_txfm_partition_cdf[TXFM_PARTITION_CONTEXTS][CDF_SIZE(2)];
    extern const aom_cdf_prob default_txb_skip_cdfs[TOKEN_CDF_Q_CTXS][TX_SIZES][TXB_SKIP_CONTEXTS][CDF_SIZE(2)];
    extern const aom_cdf_prob default_eob_extra_cdfs[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES][EOB_COEF_CONTEXTS][CDF_SIZE(2)];
    extern const aom_cdf_prob default_dc_sign_cdfs[TOKEN_CDF_Q_CTXS][PLANE_TYPES][DC_SIGN_CONTEXTS][CDF_SIZE(2)];
    extern const aom_cdf_prob default_eob_multi16_cdfs[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(5)];
    extern const aom_cdf_prob default_eob_multi32_cdfs[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(6)];
    extern const aom_cdf_prob default_eob_multi64_cdfs[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(7)];
    extern const aom_cdf_prob default_eob_multi128_cdfs[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(8)];
    extern const aom_cdf_prob default_eob_multi256_cdfs[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(9)];
    extern const aom_cdf_prob default_eob_multi512_cdfs[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(10)];
    extern const aom_cdf_prob default_eob_multi1024_cdfs[TOKEN_CDF_Q_CTXS][PLANE_TYPES][2][CDF_SIZE(11)];
    extern const aom_cdf_prob default_coeff_base_eob_cdfs[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS_EOB] [CDF_SIZE(NUM_BASE_LEVELS + 1)];
    extern const aom_cdf_prob default_coeff_base_cdfs[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES][SIG_COEF_CONTEXTS][CDF_SIZE(NUM_BASE_LEVELS + 2)];
    extern const aom_cdf_prob default_coeff_lps_cdfs[TOKEN_CDF_Q_CTXS][TX_SIZES][PLANE_TYPES][LEVEL_CONTEXTS][CDF_SIZE(BR_CDF_SIZE)];
    extern const aom_cdf_prob default_skip_cdf[SKIP_CONTEXTS][CDF_SIZE(2)];
    extern const aom_cdf_prob default_intra_inter_cdf[INTRA_INTER_CONTEXTS][CDF_SIZE(2)];
    extern const aom_cdf_prob default_comp_inter_cdf[COMP_INTER_CONTEXTS][CDF_SIZE(2)];
    extern const aom_cdf_prob default_single_ref_cdf[AV1_REF_CONTEXTS][SINGLE_REFS - 1][CDF_SIZE(2)];
    extern const aom_cdf_prob default_comp_ref_type_cdf[COMP_REF_TYPE_CONTEXTS][CDF_SIZE(2)];
    extern const aom_cdf_prob default_comp_ref_cdf[AV1_REF_CONTEXTS][FWD_REFS - 1][CDF_SIZE(2)];
    extern const aom_cdf_prob default_comp_bwdref_cdf[AV1_REF_CONTEXTS][BWD_REFS - 1][CDF_SIZE(2)];
    extern const aom_cdf_prob default_newmv_cdf[NEWMV_MODE_CONTEXTS][CDF_SIZE(2)];
    extern const aom_cdf_prob default_zeromv_cdf[GLOBALMV_MODE_CONTEXTS][CDF_SIZE(2)];
    extern const aom_cdf_prob default_refmv_cdf[REFMV_MODE_CONTEXTS][CDF_SIZE(2)];
    extern const aom_cdf_prob default_drl_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)];
    extern const aom_cdf_prob default_av1_kf_y_mode_cdf[KF_MODE_CONTEXTS][KF_MODE_CONTEXTS][CDF_SIZE(AV1_INTRA_MODES)];
    extern const aom_cdf_prob default_coef_head_cdf_q0[TX_SIZES][PLANE_TYPES][REF_TYPES][COEF_BANDS][COEFF_CONTEXTS][CDF_SIZE(ENTROPY_TOKENS)];
    extern const aom_cdf_prob default_coef_head_cdf_q1[TX_SIZES][PLANE_TYPES][REF_TYPES][COEF_BANDS][COEFF_CONTEXTS][CDF_SIZE(ENTROPY_TOKENS)];
    extern const aom_cdf_prob default_coef_head_cdf_q2[TX_SIZES][PLANE_TYPES][REF_TYPES][COEF_BANDS][COEFF_CONTEXTS][CDF_SIZE(ENTROPY_TOKENS)];
    extern const aom_cdf_prob default_coef_head_cdf_q3[TX_SIZES][PLANE_TYPES][REF_TYPES][COEF_BANDS][COEFF_CONTEXTS][CDF_SIZE(ENTROPY_TOKENS)];
    extern const aom_cdf_prob default_inter_compound_mode_cdf[INTER_MODE_CONTEXTS][CDF_SIZE(AV1_INTER_COMPOUND_MODES)];

    inline int32_t pareto(int32_t node, int32_t prob) {
        assert(node >= 2);
        int32_t x = (prob - 1) / 2;
        if (prob & 1)
            return pareto_table[x][node - 2];
        else
            return (pareto_table[x][node - 2] + pareto_table[x + 1][node - 2]) >> 1;
    }
};
