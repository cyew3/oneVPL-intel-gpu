//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 - 2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"
#include "mfx_h265_quant.h"
#include "mfx_h265_quant_rdo.h"
#include "mfx_h265_enc.h"

namespace H265Enc {

#define MAX_INT                   2147483647  ///< max. value of signed 32-bit integer
#define MAX_INT64                 0x7FFFFFFFFFFFFFFFLL  ///< max. value of signed 64-bit integer

#define COEF_REMAIN_BIN_REDUCTION        3u ///< indicates the level at which the VLC

//---------------------------------------------------------


#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))
#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))



static const Ipp32s  g_eTTable[4] = {0,3,1,2};

#define RDOQ_DoAlgorithm(CGZ,SBH) \
    quant.template DoAlgorithm<CGZ,SBH>(pSrc, pDst, log2_tr_size, bit_depth, is_slice_i, is_blk_i, type, abs_part_idx, QP)

template <typename PixType>
void h265_quant_fwd_rdo(
    H265CU<PixType>*  pCU,
    Ipp16s*  pSrc,
    Ipp16s*  pDst,
    Ipp32s   log2_tr_size,
    Ipp32s   bit_depth,
    Ipp32s   is_slice_i,
    Ipp32s   is_blk_i,
    EnumTextType  type,
    Ipp32u   abs_part_idx,
    Ipp32s   QP,
    H265BsFake *bs)
{
    RDOQuant<PixType> quant(pCU, bs, type);

    switch((pCU->m_par->rdoqCGZFlag << 1) | pCU->m_par->SBHFlag) {
    case 0: RDOQ_DoAlgorithm(0, 0); break;
    case 1: RDOQ_DoAlgorithm(0, 1); break;
    case 2: RDOQ_DoAlgorithm(1, 0); break;
    default:
    case 3: RDOQ_DoAlgorithm(1, 1); break;
    }
} // void h265_quant_fwd_rdo(...)

template void h265_quant_fwd_rdo<Ipp8u>(H265CU<Ipp8u>*  pCU,
    Ipp16s*  pSrc,
    Ipp16s*  pDst,
    Ipp32s   log2_tr_size,
    Ipp32s   bit_depth,
    Ipp32s   is_slice_i,
    Ipp32s   is_blk_i,
    EnumTextType  type,
    Ipp32u   abs_part_idx,
    Ipp32s   QP,
    H265BsFake *bs);

template void h265_quant_fwd_rdo<Ipp16u>(H265CU<Ipp16u>*  pCU,
    Ipp16s*  pSrc,
    Ipp16s*  pDst,
    Ipp32s   log2_tr_size,
    Ipp32s   bit_depth,
    Ipp32s   is_slice_i,
    Ipp32s   is_blk_i,
    EnumTextType  type,
    Ipp32u   abs_part_idx,
    Ipp32s   QP,
    H265BsFake *bs);

//---------------------------------------------------------
// RDOQuant
//---------------------------------------------------------

template <typename PixType>
RDOQuant<PixType>::RDOQuant(H265CU<PixType>* pCU, H265BsFake* bs, EnumTextType type)
{
    m_pCU    = pCU;
    m_bs     = bs;
    m_lambda = type == TEXT_LUMA ? m_pCU->m_rdLambda : m_pCU->m_rdLambdaChroma;
    m_bitDepth = type == TEXT_LUMA ? pCU->m_par->bitDepthLuma : pCU->m_par->bitDepthChroma;
    m_textType = type;

    m_lambda *= 256;//to meet HM10 code

    qlevels =              pCU->m_scratchPad.rdoq.qlevels;
    zeroCosts =            pCU->m_scratchPad.rdoq.zeroCosts;
    cost_nz_level =        pCU->m_scratchPad.rdoq.cost_nz_level;
    cost_zero_level =      pCU->m_scratchPad.rdoq.cost_zero_level;
    cost_sig =             pCU->m_scratchPad.rdoq.cost_sig;
    rate_inc_up =          pCU->m_scratchPad.rdoq.rate_inc_up;
    rate_inc_down =        pCU->m_scratchPad.rdoq.rate_inc_down;
    sig_rate_delta =       pCU->m_scratchPad.rdoq.sig_rate_delta;
    delta_u =              pCU->m_scratchPad.rdoq.delta_u;
    srcScanOrder =         pCU->m_scratchPad.rdoq.srcScanOrder;
    sig_coeff_group_flag = pCU->m_scratchPad.rdoq.sig_coeff_group_flag;
    sbh_possible =         pCU->m_scratchPad.rdoq.sbh_possible;
    cost_coeff_group_sig = pCU->m_scratchPad.rdoq.cost_coeff_group_sig;
} // RDOQuant::RDOQuant(H265CU* pcCU, H265BsFake* bs)

template <typename PixType>
RDOQuant<PixType>::~RDOQuant()
{
    m_pCU = NULL;
    m_bs   = NULL;

} // RDOQuant::~RDOQuant()


Ipp32s GetEntropyBits(
    CABAC_CONTEXT_H265 *ctx,
    Ipp32s code)
{
    register Ipp8u pStateIdx = *ctx;

    Ipp32s bits = tab_cabacPBits[pStateIdx ^ (code << 6)];

    //return (128 * bits);
    return (bits << 7);
}

template <typename PixType>
void RDOQuant<PixType>::EstimateLastXYBits(
    CabacBits* pBits,
    Ipp32s width)
{
    Ipp32s bits_x = 0, bits_y = 0;

    Ipp32s blk_size_offset =  m_textType ? 0 : (h265_log2m2[ width ]*3 + ((h265_log2m2[ width ]+1)>>2));
    Ipp32s shift = m_textType ? h265_log2m2[ width ] : ((h265_log2m2[ width ]+3)>>2);

    Ipp32u ctx_inc, ctx_offset;
    CABAC_CONTEXT_H265 *ctx_base_x = CTX(m_bs,LAST_X_HEVC) + (m_textType ? NUM_CTX_LAST_FLAG_XY : 0);
    CABAC_CONTEXT_H265 *ctx_base_y = CTX(m_bs,LAST_Y_HEVC) + (m_textType ? NUM_CTX_LAST_FLAG_XY : 0);
    for (ctx_inc = 0; ctx_inc < h265_group_idx[ width - 1 ]; ctx_inc++)
    {
        ctx_offset = blk_size_offset + (ctx_inc >> shift);
        pBits->lastXBits[ ctx_inc ] = bits_x + GetEntropyBits(ctx_base_x + ctx_offset, 0);
        bits_x += GetEntropyBits(ctx_base_x + ctx_offset, 1);
        pBits->lastYBits[ ctx_inc ] = bits_y + GetEntropyBits(ctx_base_y + ctx_offset, 0);
        bits_y += GetEntropyBits(ctx_base_y + ctx_offset, 1);
    }
    pBits->lastXBits[ctx_inc] = bits_x;
    pBits->lastYBits[ctx_inc] = bits_y;

    return;

} // void RDOQuant::EstimateLastXYBits( ... )

template <typename PixType>
void RDOQuant<PixType>::EstimateCabacBits(Ipp32s log2_tr_size)
{
    Ipp32s ctx_inc;
    CABAC_CONTEXT_H265* ctx_base;

    ctx_base = CTX(m_bs,QT_CBF_HEVC);
    for( ctx_inc = 0; ctx_inc < NUM_QT_CBF_CTX; ctx_inc++ )
    {
        m_cabacBits.blkCbfBits[ ctx_inc ][ 0 ] = GetEntropyBits(ctx_base + ctx_inc, 0);
        m_cabacBits.blkCbfBits[ ctx_inc ][ 1 ] = GetEntropyBits(ctx_base + ctx_inc, 1);
    }

    ctx_base = CTX(m_bs,QT_ROOT_CBF_HEVC);
    for( ctx_inc = 0; ctx_inc < 1; ctx_inc++ )
    {
        m_cabacBits.blkRootCbfBits[ ctx_inc ][ 0 ] = GetEntropyBits(ctx_base + ctx_inc, 0);
        m_cabacBits.blkRootCbfBits[ ctx_inc ][ 1 ] = GetEntropyBits(ctx_base + ctx_inc, 1);
    }

    ctx_base = CTX(m_bs,SIG_COEFF_GROUP_FLAG_HEVC) + 2 * m_textType;
    for ( ctx_inc = 0; ctx_inc < NUM_SIG_CG_FLAG_CTX; ctx_inc++ )
    {
        m_cabacBits.significantCoeffGroupBits[ ctx_inc ][ 0 ] = GetEntropyBits(ctx_base + ctx_inc, 0);
        m_cabacBits.significantCoeffGroupBits[ ctx_inc ][ 1 ] = GetEntropyBits(ctx_base + ctx_inc, 1);
    }

    ctx_base = CTX(m_bs,SIG_FLAG_HEVC) + (m_textType ? NUM_SIG_FLAG_CTX_LUMA : 0);
    for ( ctx_inc = 0; ctx_inc < (m_textType ? 15 : 27); ctx_inc++ )
    {
        m_cabacBits.significantBits[ ctx_inc ][ 0 ] = GetEntropyBits(ctx_base + ctx_inc, 0);
        m_cabacBits.significantBits[ ctx_inc ][ 1 ] = GetEntropyBits(ctx_base + ctx_inc, 1);
    }

    ctx_base = CTX(m_bs,ONE_FLAG_HEVC) + (m_textType ? NUM_ONE_FLAG_CTX_LUMA : 0);
    for (ctx_inc = 0; ctx_inc < (m_textType ? NUM_ONE_FLAG_CTX_CHROMA : NUM_ONE_FLAG_CTX_LUMA); ctx_inc++)
    {
        m_cabacBits.greaterOneBits[ ctx_inc ][ 0 ] = GetEntropyBits(ctx_base + ctx_inc, 0);
        m_cabacBits.greaterOneBits[ ctx_inc ][ 1 ] = GetEntropyBits(ctx_base + ctx_inc, 1);
    }

    ctx_base = CTX(m_bs,ABS_FLAG_HEVC) + (m_textType ? NUM_ABS_FLAG_CTX_LUMA : 0);
    for (ctx_inc = 0; ctx_inc < (m_textType ? NUM_ABS_FLAG_CTX_CHROMA : NUM_ABS_FLAG_CTX_LUMA); ctx_inc++)
    {
        m_cabacBits.levelAbsBits[ ctx_inc ][ 0 ] = GetEntropyBits(ctx_base + ctx_inc, 0);
        m_cabacBits.levelAbsBits[ ctx_inc ][ 1 ] = GetEntropyBits(ctx_base + ctx_inc, 1);
    }

    EstimateLastXYBits( &m_cabacBits, (1 << log2_tr_size));

    return;

} // void RDOQuant::EstimateCabacBits(...)


typedef Ipp32u UInt;
typedef int Int;

struct RDOQ_CoeffGroup_Report
{
    Ipp32s num_nz_before_pos0;
    Ipp64f cost_nz_levels;
    Ipp64f cost_zero_levels;
    Ipp64f cost_sig;
    Ipp64f cost_sig_pos0;
};


//only state update
void EncodeOneCoeff(
    Ipp32u qlevel,
    Ipp32s scan_pos,
    EnumTextType type,
    RDOQCabacState* par)
{

    if (qlevel) {
        par->c1Idx++;

        if( qlevel > 1 )
        {
            if(qlevel > (3u<<par->go_rice_param))
            {
                par->go_rice_param = IPP_MIN(par->go_rice_param + 1, 4);
            }
            par->c1 = 0;
            par->c2 += (par->c2 < 2);
            par->c2Idx++;
        }
        //else if( (par->c1 < 3) && (par->c1 > 0) )
        else if( (Ipp32u)(par->c1 - 1) < 2)
        {
            par->c1++;
        }
    }

    if( (scan_pos & ~(SCAN_SET_SIZE-1)) == scan_pos && scan_pos > 0)
    {
        par->c2            = 0;
        par->go_rice_param = 0;

        par->c1Idx   = 0;
        par->c2Idx   = 0;
        par->ctx_set = (scan_pos == SCAN_SET_SIZE || type) ? 0 : 2;
        if( par->c1 == 0 )
        {
            par->ctx_set++;
        }
        par->c1 = 1;
    }

    return;

} // void EncodeOneCoeff(...)

void Rast2Scan_px(const Ipp16s *src, Ipp16s *dst, Ipp32s scan_idx, Ipp32s log2_tr_size)
{
    Ipp32s numCoeff = (1 << (log2_tr_size << 1));
    const Ipp16u *scan = H265Enc::h265_sig_last_scan[scan_idx - 1][log2_tr_size - 1];
    for (Ipp32s i = 0; i < numCoeff; i++)
        dst[i] = src[scan[i]];
}

template <Ipp32s pitch>
inline void Rast2Scan8x4(const Ipp16s *src, Ipp16s *dst0, Ipp16s *dst1)
{
    __m128i a,b,c,d,e,f;
    a = _mm_load_si128((__m128i*)(src+0*pitch));
    b = _mm_load_si128((__m128i*)(src+1*pitch));
    c = _mm_load_si128((__m128i*)(src+2*pitch));
    d = _mm_load_si128((__m128i*)(src+3*pitch));
    e = _mm_unpacklo_epi16(b, c);
    f = _mm_unpacklo_epi16(a, e);
    f = _mm_insert_epi16(f, src[3*pitch], 6);
    f = _mm_shufflehi_epi16(f, _MM_SHUFFLE(3,2,0,1));
    _mm_store_si128((__m128i*)dst0, f);
    e = _mm_unpackhi_epi64(e, e);
    e = _mm_unpacklo_epi16(e, d);
    e = _mm_shufflelo_epi16(e, _MM_SHUFFLE(2,3,1,0));
    e = _mm_insert_epi16(e, src[3], 1);
    _mm_store_si128((__m128i*)dst0+1, e);

    e = _mm_unpackhi_epi16(b, c); // 4 8 5 9 6 10 7 11
    f = _mm_unpacklo_epi64(e, e); // 4 8 5 9 4 8 5 9 
    f = _mm_unpackhi_epi16(a, f); // 0 4 1 8 2 5 3 9
    f = _mm_insert_epi16(f, src[3*pitch+4], 6);
    f = _mm_shufflehi_epi16(f, _MM_SHUFFLE(3,2,0,1));
    _mm_store_si128((__m128i*)dst1+0, f);
    e = _mm_unpackhi_epi16(e, d);
    e = _mm_shufflelo_epi16(e, _MM_SHUFFLE(2,3,1,0));
    e = _mm_insert_epi16(e, src[7], 1);
    _mm_store_si128((__m128i*)dst1+1, e);
}

void Rast2Scan(const Ipp16s *src, Ipp16s *dst, Ipp32s scan_idx, Ipp32s log2_tr_size)
{
    if (scan_idx == 1) {
        if (log2_tr_size == 2) {
            _mm_store_si128((__m128i*)dst, _mm_load_si128((__m128i*)src));
            _mm_store_si128((__m128i*)dst+1, _mm_load_si128((__m128i*)src+1));
        } else if (log2_tr_size == 3) {
            __m128i s0 = _mm_load_si128((__m128i*)src);     //  0..7
            __m128i s1 = _mm_load_si128((__m128i*)src+1);   //  8..15
            __m128i s2 = _mm_load_si128((__m128i*)src+2);   // 16..23
            __m128i s3 = _mm_load_si128((__m128i*)src+3);   // 24..31
            _mm_store_si128((__m128i*)dst+0, _mm_unpacklo_epi64(s0, s1)); // 0 1 2 3 8 9 10 11
            _mm_store_si128((__m128i*)dst+1, _mm_unpacklo_epi64(s2, s3)); // 16 17 18 19 24 25 26 27
            _mm_store_si128((__m128i*)dst+2, _mm_unpackhi_epi64(s0, s1)); // 4 5 6 7 12 13 14 15
            _mm_store_si128((__m128i*)dst+3, _mm_unpackhi_epi64(s2, s3)); // 20 21 22 23 28 29 30 31
            s0 = _mm_load_si128((__m128i*)src+4);
            s1 = _mm_load_si128((__m128i*)src+5);
            s2 = _mm_load_si128((__m128i*)src+6);
            s3 = _mm_load_si128((__m128i*)src+7);
            _mm_store_si128((__m128i*)dst+4, _mm_unpacklo_epi64(s0, s1));
            _mm_store_si128((__m128i*)dst+5, _mm_unpacklo_epi64(s2, s3));
            _mm_store_si128((__m128i*)dst+6, _mm_unpackhi_epi64(s0, s1));
            _mm_store_si128((__m128i*)dst+7, _mm_unpackhi_epi64(s2, s3));
        } else if (log2_tr_size == 4) {
            for (Ipp32s i = 0; i < 256; i += 64, src += 64, dst += 64) {
                __m128i s0 = _mm_load_si128((__m128i*)src);     //  0..7
                __m128i s1 = _mm_load_si128((__m128i*)src+1);   //  8..15
                __m128i s2 = _mm_load_si128((__m128i*)src+2);   // 16..23
                __m128i s3 = _mm_load_si128((__m128i*)src+3);   // 24..31
                __m128i s4 = _mm_load_si128((__m128i*)src+4);   // 32..39
                __m128i s5 = _mm_load_si128((__m128i*)src+5);   // 40..47
                __m128i s6 = _mm_load_si128((__m128i*)src+6);   // 48..55
                __m128i s7 = _mm_load_si128((__m128i*)src+7);   // 56..63
                _mm_store_si128((__m128i*)dst+0, _mm_unpacklo_epi64(s0, s2)); //  0..3  16..19
                _mm_store_si128((__m128i*)dst+1, _mm_unpacklo_epi64(s4, s6)); // 32..35 48..51
                _mm_store_si128((__m128i*)dst+2, _mm_unpackhi_epi64(s0, s2)); //  4..7  20..23
                _mm_store_si128((__m128i*)dst+3, _mm_unpackhi_epi64(s4, s6)); // 36..39 52..55
                _mm_store_si128((__m128i*)dst+4, _mm_unpacklo_epi64(s1, s3));
                _mm_store_si128((__m128i*)dst+5, _mm_unpacklo_epi64(s5, s7));
                _mm_store_si128((__m128i*)dst+6, _mm_unpackhi_epi64(s1, s3));
                _mm_store_si128((__m128i*)dst+7, _mm_unpackhi_epi64(s5, s7));
            }
        } else { assert(log2_tr_size == 5);
            for (Ipp32s i = 0; i < 1024; i += 128, src += 128, dst += 128) {
                __m128i s0 = _mm_load_si128((__m128i*)src);     //  0..7
                __m128i s1 = _mm_load_si128((__m128i*)src+1);   //  8..15
                __m128i s2 = _mm_load_si128((__m128i*)src+2);   // 16..23
                __m128i s3 = _mm_load_si128((__m128i*)src+3);   // 24..31
                __m128i s4 = _mm_load_si128((__m128i*)src+4);   // 32..39
                __m128i s5 = _mm_load_si128((__m128i*)src+5);   // 40..47
                __m128i s6 = _mm_load_si128((__m128i*)src+6);   // 48..55
                __m128i s7 = _mm_load_si128((__m128i*)src+7);   // 56..63
                __m128i s8 = _mm_load_si128((__m128i*)src+8);
                __m128i s9 = _mm_load_si128((__m128i*)src+9);
                __m128i s10 = _mm_load_si128((__m128i*)src+10);
                __m128i s11 = _mm_load_si128((__m128i*)src+11);
                __m128i s12 = _mm_load_si128((__m128i*)src+12);
                __m128i s13 = _mm_load_si128((__m128i*)src+13);
                __m128i s14 = _mm_load_si128((__m128i*)src+14);
                __m128i s15 = _mm_load_si128((__m128i*)src+15);
                _mm_store_si128((__m128i*)dst+0, _mm_unpacklo_epi64(s0, s4)); //  0..3  32..35
                _mm_store_si128((__m128i*)dst+1, _mm_unpacklo_epi64(s8, s12));
                _mm_store_si128((__m128i*)dst+2, _mm_unpackhi_epi64(s0, s4)); //  4..7  36..39
                _mm_store_si128((__m128i*)dst+3, _mm_unpackhi_epi64(s8, s12));
                _mm_store_si128((__m128i*)dst+4, _mm_unpacklo_epi64(s1, s5));
                _mm_store_si128((__m128i*)dst+5, _mm_unpacklo_epi64(s9, s13));
                _mm_store_si128((__m128i*)dst+6, _mm_unpackhi_epi64(s1, s5));
                _mm_store_si128((__m128i*)dst+7, _mm_unpackhi_epi64(s9, s13));
                _mm_store_si128((__m128i*)dst+8, _mm_unpacklo_epi64(s2, s6));
                _mm_store_si128((__m128i*)dst+9, _mm_unpacklo_epi64(s10, s14));
                _mm_store_si128((__m128i*)dst+10, _mm_unpackhi_epi64(s2, s6));
                _mm_store_si128((__m128i*)dst+11, _mm_unpackhi_epi64(s10, s14));
                _mm_store_si128((__m128i*)dst+12, _mm_unpacklo_epi64(s3, s7));
                _mm_store_si128((__m128i*)dst+13, _mm_unpacklo_epi64(s11, s15));
                _mm_store_si128((__m128i*)dst+14, _mm_unpackhi_epi64(s3, s7));
                _mm_store_si128((__m128i*)dst+15, _mm_unpackhi_epi64(s11, s15));
            }
        }
    } else if (scan_idx == 2) {
        if (log2_tr_size == 2) {
            __m128i a = _mm_load_si128((__m128i*)src);
            __m128i b = _mm_load_si128((__m128i*)src+1);
            __m128i c = _mm_unpacklo_epi16(a, b);
            __m128i d = _mm_unpackhi_epi16(a, b);
            a = _mm_unpacklo_epi16(c, d);
            b = _mm_unpackhi_epi16(c, d);
            _mm_store_si128((__m128i*)dst+0, a);
            _mm_store_si128((__m128i*)dst+1, b);
        } else if (log2_tr_size == 3) {
            __m128i s0 = _mm_load_si128((__m128i*)src);                 //  0..7
            __m128i s1 = _mm_load_si128((__m128i*)src+1);               //  8..15
            __m128i s2 = _mm_load_si128((__m128i*)src+2);               // 16..23
            __m128i s3 = _mm_load_si128((__m128i*)src+3);               // 24..31
            __m128i s4 = _mm_load_si128((__m128i*)src+4);               // 32..39
            __m128i s5 = _mm_load_si128((__m128i*)src+5);               // 40..47
            __m128i s6 = _mm_load_si128((__m128i*)src+6);               // 48..55
            __m128i s7 = _mm_load_si128((__m128i*)src+7);               // 56..63
            __m128i a = _mm_unpacklo_epi16(s0, s1);                     // 00 08 01 09 02 10 03 11
            __m128i b = _mm_unpacklo_epi16(s2, s3);                     // 16 24 17 25 18 26 19 27
            _mm_store_si128((__m128i*)dst, _mm_unpacklo_epi32(a, b));   // 00 08 16 24 01 09 17 25
            _mm_store_si128((__m128i*)dst+1, _mm_unpackhi_epi32(a, b)); // 02 10 18 26 03 11 19 27
            a = _mm_unpacklo_epi16(s4, s5);
            b = _mm_unpacklo_epi16(s6, s7);
            _mm_store_si128((__m128i*)dst+2, _mm_unpacklo_epi32(a, b));
            _mm_store_si128((__m128i*)dst+3, _mm_unpackhi_epi32(a, b));
            a = _mm_unpackhi_epi16(s0, s1);
            b = _mm_unpackhi_epi16(s2, s3);
            _mm_store_si128((__m128i*)dst+4, _mm_unpacklo_epi32(a, b));
            _mm_store_si128((__m128i*)dst+5, _mm_unpackhi_epi32(a, b));
            a = _mm_unpackhi_epi16(s4, s5);
            b = _mm_unpackhi_epi16(s6, s7);
            _mm_store_si128((__m128i*)dst+6, _mm_unpacklo_epi32(a, b));
            _mm_store_si128((__m128i*)dst+7, _mm_unpackhi_epi32(a, b));
        } else if (log2_tr_size == 4) {
            for (Ipp32s i = 0; i < 256; i += 64, src += 64, dst += 16) {
                __m128i s0 = _mm_load_si128((__m128i*)src);                 //  0..7
                __m128i s1 = _mm_load_si128((__m128i*)src+1);               //  8..15
                __m128i s2 = _mm_load_si128((__m128i*)src+2);               // 16..23
                __m128i s3 = _mm_load_si128((__m128i*)src+3);               // 24..31
                __m128i s4 = _mm_load_si128((__m128i*)src+4);               // 32..39
                __m128i s5 = _mm_load_si128((__m128i*)src+5);               // 40..47
                __m128i s6 = _mm_load_si128((__m128i*)src+6);               // 48..55
                __m128i s7 = _mm_load_si128((__m128i*)src+7);               // 56..63
                __m128i a = _mm_unpacklo_epi16(s0, s2);                     // 00 16 01 17 02 18 03 19
                __m128i b = _mm_unpacklo_epi16(s4, s6);                     // 32 48 33 49 34 50 35 51
                _mm_store_si128((__m128i*)dst+0, _mm_unpacklo_epi32(a, b)); // 00 16 32 48 01 17 33 49
                _mm_store_si128((__m128i*)dst+1, _mm_unpackhi_epi32(a, b)); // 02 18 34 50 03 19 35 51
                a = _mm_unpackhi_epi16(s0, s2);
                b = _mm_unpackhi_epi16(s4, s6);
                _mm_store_si128((__m128i*)dst+8, _mm_unpacklo_epi32(a, b));
                _mm_store_si128((__m128i*)dst+9, _mm_unpackhi_epi32(a, b));
                a = _mm_unpacklo_epi16(s1, s3);
                b = _mm_unpacklo_epi16(s5, s7);
                _mm_store_si128((__m128i*)dst+16, _mm_unpacklo_epi32(a, b));
                _mm_store_si128((__m128i*)dst+17, _mm_unpackhi_epi32(a, b));
                a = _mm_unpackhi_epi16(s1, s3);
                b = _mm_unpackhi_epi16(s5, s7);
                _mm_store_si128((__m128i*)dst+24, _mm_unpacklo_epi32(a, b));
                _mm_store_si128((__m128i*)dst+25, _mm_unpackhi_epi32(a, b));
            }
        } else { assert(log2_tr_size == 5);
            for (Ipp32s i = 0; i < 1024; i += 128, src += 128, dst += 16) {
                __m128i s0  = _mm_load_si128((__m128i*)src);     //  0..7
                __m128i s1  = _mm_load_si128((__m128i*)src+1);   //  8..15
                __m128i s2  = _mm_load_si128((__m128i*)src+2);  // 16..23
                __m128i s3  = _mm_load_si128((__m128i*)src+3);  // 24..31
                __m128i s4  = _mm_load_si128((__m128i*)src+4);   // 32..39
                __m128i s5  = _mm_load_si128((__m128i*)src+5);   // 40..47
                __m128i s6  = _mm_load_si128((__m128i*)src+6); 
                __m128i s7  = _mm_load_si128((__m128i*)src+7); 
                __m128i s8  = _mm_load_si128((__m128i*)src+8);   // 64..71
                __m128i s9  = _mm_load_si128((__m128i*)src+9);   // 72..79
                __m128i s10 = _mm_load_si128((__m128i*)src+10); 
                __m128i s11 = _mm_load_si128((__m128i*)src+11); 
                __m128i s12 = _mm_load_si128((__m128i*)src+12);  // 96..103
                __m128i s13 = _mm_load_si128((__m128i*)src+13);  //104..111
                __m128i s14 = _mm_load_si128((__m128i*)src+14);
                __m128i s15 = _mm_load_si128((__m128i*)src+15);
                __m128i a = _mm_unpacklo_epi16(s0, s4);
                __m128i b = _mm_unpacklo_epi16(s8, s12);
                _mm_store_si128((__m128i*)dst+0, _mm_unpacklo_epi32(a, b));
                _mm_store_si128((__m128i*)dst+1, _mm_unpackhi_epi32(a, b));
                a = _mm_unpackhi_epi16(s0, s4);
                b = _mm_unpackhi_epi16(s8, s12);
                _mm_store_si128((__m128i*)dst+16, _mm_unpacklo_epi32(a, b));
                _mm_store_si128((__m128i*)dst+17, _mm_unpackhi_epi32(a, b));
                a = _mm_unpacklo_epi16(s1, s5);
                b = _mm_unpacklo_epi16(s9, s13);
                _mm_store_si128((__m128i*)dst+32, _mm_unpacklo_epi32(a, b));
                _mm_store_si128((__m128i*)dst+33, _mm_unpackhi_epi32(a, b));
                a = _mm_unpackhi_epi16(s1, s5);
                b = _mm_unpackhi_epi16(s9, s13);
                _mm_store_si128((__m128i*)dst+48, _mm_unpacklo_epi32(a, b));
                _mm_store_si128((__m128i*)dst+49, _mm_unpackhi_epi32(a, b));
                a = _mm_unpacklo_epi16(s2, s6);
                b = _mm_unpacklo_epi16(s10, s14);
                _mm_store_si128((__m128i*)dst+64, _mm_unpacklo_epi32(a, b));
                _mm_store_si128((__m128i*)dst+65, _mm_unpackhi_epi32(a, b));
                a = _mm_unpackhi_epi16(s2, s6);
                b = _mm_unpackhi_epi16(s10, s14);
                _mm_store_si128((__m128i*)dst+80, _mm_unpacklo_epi32(a, b));
                _mm_store_si128((__m128i*)dst+81, _mm_unpackhi_epi32(a, b));
                a = _mm_unpacklo_epi16(s3, s7);
                b = _mm_unpacklo_epi16(s11, s15);
                _mm_store_si128((__m128i*)dst+96, _mm_unpacklo_epi32(a, b));
                _mm_store_si128((__m128i*)dst+97, _mm_unpackhi_epi32(a, b));
                a = _mm_unpackhi_epi16(s3, s7);
                b = _mm_unpackhi_epi16(s11, s15);
                _mm_store_si128((__m128i*)dst+112, _mm_unpacklo_epi32(a, b));
                _mm_store_si128((__m128i*)dst+113, _mm_unpackhi_epi32(a, b));
            }
        }
    } else { assert(scan_idx == 3);
        if (log2_tr_size == 2) {
            __m128i a,b,c,d,e;
            a = _mm_load_si128((__m128i*)src);
            b = _mm_load_si128((__m128i*)src+1);
            c = _mm_unpackhi_epi16(a, b);                       // x | 12 | 5 | ...
            c = _mm_shufflelo_epi16(c, _MM_SHUFFLE(0,0,1,2));   // 5 | 12 | ...
            c = _mm_unpacklo_epi32(a, c);                       // 0 | 1 | 5 | 12 | ...
            d = _mm_shuffle_epi32(a, _MM_SHUFFLE(0,3,2,1));     // 2 | 3 | 4 | 5 | 6 | 7 | 0 | 1
            e = _mm_shufflelo_epi16(d, _MM_SHUFFLE(0,0,0,2));   // 4 | 2 | ....
            e = _mm_unpacklo_epi16(e, b);                       // 4 | 8 | 2 | 9 | ...
            c = _mm_unpacklo_epi16(c, e);                       // 0 | 4 | 1 | 8 | 5 | 2 | 12 | 9
            _mm_store_si128((__m128i*)dst, c);
            c = _mm_shuffle_epi32(b, _MM_SHUFFLE(2,1,0,3));     // 14 | 15 | 8 | 9 | 10 | 11 | 12 | 13
            e = _mm_shufflehi_epi16(c, _MM_SHUFFLE(2,0,1,3));   // 14 | 15 | 8 | 9 | 13 | 11 | 10 | 12
            e = _mm_unpackhi_epi16(d, e);                       // 6 | 13 | 7 | 11 | ...
            d = _mm_unpacklo_epi32(a, b);                       // 0 | 1 | 8 | 9 | 2 | 3 | 10 | 11
            d = _mm_shufflehi_epi16(d, _MM_SHUFFLE(3,0,2,1));   // 0 | 1 | 8 | 9 | 3 | 10 | 2 | 11
            d = _mm_shuffle_epi32(d, _MM_SHUFFLE(3,1,0,2));     // 3 | 10 | 0 | 1 | 8 | 9 | 2 | 11
            d = _mm_unpacklo_epi32(d, c);                       // 3 | 10 | 14 | 15 | ...
            d = _mm_unpacklo_epi16(e, d);                       // 6 | 3 | 13 | 10 | 7 | 14 | 11 | 15 
            _mm_store_si128((__m128i*)dst+1, d);
        } else if (log2_tr_size == 3) {
            Rast2Scan8x4<8>(src,     dst+0,  dst+32);
            Rast2Scan8x4<8>(src+4*8, dst+16, dst+48);
        } else if (log2_tr_size == 4) {
            Rast2Scan8x4<16>(src,         dst+0*16,  dst+2*16);
            Rast2Scan8x4<16>(src+8,       dst+5*16,  dst+9*16);
            Rast2Scan8x4<16>(src+4*16,    dst+1*16,  dst+4*16);
            Rast2Scan8x4<16>(src+4*16+8,  dst+8*16,  dst+12*16);
            Rast2Scan8x4<16>(src+8*16,    dst+3*16,  dst+7*16);
            Rast2Scan8x4<16>(src+8*16+8,  dst+11*16, dst+14*16);
            Rast2Scan8x4<16>(src+12*16,   dst+6*16,  dst+10*16);
            Rast2Scan8x4<16>(src+12*16+8, dst+13*16, dst+15*16);
        } else { assert(log2_tr_size == 5);
            Rast2Scan8x4<32>(src,          dst+0*16,  dst+2*16);
            Rast2Scan8x4<32>(src+8,        dst+5*16,  dst+9*16);
            Rast2Scan8x4<32>(src+16,       dst+14*16, dst+20*16);
            Rast2Scan8x4<32>(src+24,       dst+27*16, dst+35*16);
            Rast2Scan8x4<32>(src+4*32,     dst+1*16,  dst+4*16);
            Rast2Scan8x4<32>(src+4*32+8,   dst+8*16,  dst+13*16);
            Rast2Scan8x4<32>(src+4*32+16,  dst+19*16, dst+26*16);
            Rast2Scan8x4<32>(src+4*32+24,  dst+34*16, dst+42*16);
            Rast2Scan8x4<32>(src+8*32,     dst+3*16,  dst+7*16);
            Rast2Scan8x4<32>(src+8*32+8,   dst+12*16, dst+18*16);
            Rast2Scan8x4<32>(src+8*32+16,  dst+25*16, dst+33*16);
            Rast2Scan8x4<32>(src+8*32+24,  dst+41*16, dst+48*16);
            Rast2Scan8x4<32>(src+12*32,    dst+6*16,  dst+11*16);
            Rast2Scan8x4<32>(src+12*32+8,  dst+17*16, dst+24*16);
            Rast2Scan8x4<32>(src+12*32+16, dst+32*16, dst+40*16);
            Rast2Scan8x4<32>(src+12*32+24, dst+47*16, dst+53*16);
            Rast2Scan8x4<32>(src+16*32,    dst+10*16, dst+16*16);
            Rast2Scan8x4<32>(src+16*32+8,  dst+23*16, dst+31*16);
            Rast2Scan8x4<32>(src+16*32+16, dst+39*16, dst+46*16);
            Rast2Scan8x4<32>(src+16*32+24, dst+52*16, dst+57*16);
            Rast2Scan8x4<32>(src+20*32,    dst+15*16, dst+22*16);
            Rast2Scan8x4<32>(src+20*32+8,  dst+30*16, dst+38*16);
            Rast2Scan8x4<32>(src+20*32+16, dst+45*16, dst+51*16);
            Rast2Scan8x4<32>(src+20*32+24, dst+56*16, dst+60*16);
            Rast2Scan8x4<32>(src+24*32,    dst+21*16, dst+29*16);
            Rast2Scan8x4<32>(src+24*32+8,  dst+37*16, dst+44*16);
            Rast2Scan8x4<32>(src+24*32+16, dst+50*16, dst+55*16);
            Rast2Scan8x4<32>(src+24*32+24, dst+59*16, dst+62*16);
            Rast2Scan8x4<32>(src+28*32,    dst+28*16, dst+36*16);
            Rast2Scan8x4<32>(src+28*32+8,  dst+43*16, dst+49*16);
            Rast2Scan8x4<32>(src+28*32+16, dst+54*16, dst+58*16);
            Rast2Scan8x4<32>(src+28*32+24, dst+61*16, dst+63*16);
        }
    }
}

template <typename PixType>
template <Ipp8u rdoqCGZ, Ipp8u SBH>
void RDOQuant<PixType>::DoAlgorithm(
    Ipp16s*  pSrc,
    Ipp16s*  pDst,
    Ipp32s   log2_tr_size,
    Ipp32s   bit_depth,
    Ipp32s   is_slice_i,
    Ipp32s   is_blk_i,
    EnumTextType  type,
    Ipp32u   abs_part_idx,
    Ipp32s   QP)
{
    Ipp32u  abs_sum = 0;
    
    EstimateCabacBits( log2_tr_size );

    const Ipp32u width       = (Ipp32u)(1 << log2_tr_size);
    const Ipp32u coeffCount  = (Ipp32u)(1 << 2*log2_tr_size);
    const Ipp32u cgCount     = (Ipp32u)(1 << (2*log2_tr_size-MLS_CG_SIZE));
    const Ipp32s qbits = 29 + h265_qp6[QP] - bit_depth - log2_tr_size;
    const Ipp32s qshift = 1 << (qbits - 1);

    Ipp32s qshiftOff = qshift;
    if (!m_pCU->m_par->RDOQFlag) {
        if (is_slice_i) {
            qshiftOff = qshift;
        } else {
            qshiftOff = (Ipp32s)(0.66*(double)(qshift));    // Match RD (base) DZ
        }
    } else {
        if (is_slice_i || is_blk_i) {
            qshiftOff = qshift;
        } else if (m_pCU->m_cslice->slice_type == B_SLICE && !m_pCU->m_currFrame->m_isRef) {
            qshiftOff = (Ipp32s)(0.60*(double)(qshift));    // Non Ref only
        } else {
            qshiftOff = (Ipp32s)(0.76*(double)(qshift));    // laplacian centroid (safe for inter coding)
        }
    }
    const Ipp8u  qp_rem = h265_qp_rem[QP];
    const Ipp32s uiQ    = h265_quant_table_fwd[qp_rem];
    const Ipp64f errScale = 2.0 * coeffCount * h265_quant_table_fwd_pow_minus2[qp_rem];
    const Ipp32u scan_idx = GetCoefScanIdx(abs_part_idx, log2_tr_size, type==TEXT_LUMA);

    memset(pDst, 0, sizeof(Ipp16s)*coeffCount);

    if (SBH) {
        memset( rate_inc_up,    0, sizeof(Ipp32s) * coeffCount );
        memset( rate_inc_down,  0, sizeof(Ipp32s) * coeffCount );
        memset( sig_rate_delta, 0, sizeof(Ipp32s) * coeffCount );
        memset( delta_u,        0, sizeof(Ipp32s) * coeffCount );
    }

    const Ipp16u * scanCG = h265_sig_last_scan[ scan_idx - 1 ][ log2_tr_size > 3 ? log2_tr_size-2-1 : 0  ];
    if( log2_tr_size == 3 ) {
        scanCG = h265_sig_last_scan_8x8[ scan_idx ];
    } else if( log2_tr_size == 5 ) {
        scanCG = h265_sig_scan_CG32x32;
    }

    memset( sig_coeff_group_flag,   0, sizeof(*sig_coeff_group_flag) * cgCount );

    const Ipp32u shift        = MLS_CG_SIZE >> 1;
    const Ipp32u num_blk_side = width >> shift;
    const Ipp32u log2_num_blk_side = log2_tr_size - shift;
    const Ipp32u sizeCG = (1 << MLS_CG_SIZE);//16

    memset(cost_coeff_group_sig,   0, sizeof(Ipp64f) * cgCount );

    const Ipp16u *scan = h265_sig_last_scan[ scan_idx -1 ][ log2_tr_size - 1 ];
    const Ipp32s last_scan_set = (coeffCount - 1) >> LOG2_SCAN_SET_SIZE;

    //--------------------------------------------------------------------------------------------------
    //  FIRST PASS: determines which coeff should be last_nz_coeff, calc optimal levels, try to zerro CG
    //--------------------------------------------------------------------------------------------------
    RDOQCabacState local_cabac;
    local_cabac.ctx_set = 0;
    local_cabac.c1      = 1;
    local_cabac.c2      = 0;
    local_cabac.c1Idx   = 0;
    local_cabac.c2Idx   = 0;
    local_cabac.go_rice_param = 0;

    Ipp64f cost_base = 0;

    Ipp32s last_nz_subset = -1;
    Ipp32s last_nz_pos    = -1;// in scan order

    Rast2Scan(pSrc, srcScanOrder, scan_idx, log2_tr_size);
    //for (Ipp32u i = 0; i < coeffCount; i++)
    //    srcScanOrder[i] = pSrc[scan[i]];

    // PreQuantize optimization
    Ipp64f cost_zero_blk = MFX_HEVC_PP::NAME(h265_Quant_zCost_16s)(srcScanOrder, qlevels, zeroCosts, coeffCount, uiQ, qshiftOff, qbits, coeffCount<<1);
    if (!SBH) {
        for (Ipp32u scan_pos = 0; scan_pos < coeffCount; scan_pos++) {
            cost_zero_level[scan_pos] = zeroCosts[scan_pos];
            cost_base += cost_zero_level[ scan_pos ];

            if (qlevels[scan_pos]) {
                last_nz_pos = scan_pos;
                sig_coeff_group_flag[scanCG[scan_pos >> LOG2_SCAN_SET_SIZE]] = 1;
                cost_base = 0.0;
            }
        }

        if (last_nz_pos >= 0) {
            local_cabac.ctx_set = (last_nz_pos < SCAN_SET_SIZE || type != TEXT_LUMA) ? 0 : 2;
            last_nz_subset      = last_nz_pos >> LOG2_SCAN_SET_SIZE;
        }
    } else {
        for (Ipp32s subset = last_scan_set; subset >= 0; subset--) {//subset means CG
            Ipp32s sub_pos    = subset << LOG2_SCAN_SET_SIZE;
            Ipp32u CG_blk_pos = scanCG[ subset ];
            Ipp32u first_nz_pos_in_CG=0; 
            Ipp32u last_nz_pos_in_CG=0;
            for (Ipp32s pos_in_CG = sizeCG-1; pos_in_CG >= 0; pos_in_CG--) {
                Ipp32s scan_pos = sub_pos + pos_in_CG;

                cost_zero_level[scan_pos] = zeroCosts[scan_pos];

                if ( qlevels[ scan_pos ] > 0 && last_nz_pos < 0 ) {
                    last_nz_pos         = scan_pos;
                    local_cabac.ctx_set = (scan_pos < SCAN_SET_SIZE || type != TEXT_LUMA) ? 0 : 2;
                    last_nz_subset      = subset;
                }
                if ( last_nz_pos < 0 ) {
                    cost_base += cost_zero_level[ scan_pos ];
                }
                if (qlevels[ scan_pos ]) {
                    sig_coeff_group_flag[ CG_blk_pos ] = 1;
                    if (SBH) {
                        if(!last_nz_pos_in_CG) last_nz_pos_in_CG = scan_pos;
                        first_nz_pos_in_CG = scan_pos;
                    }
                }            
            } //end for (Ipp32s pos_in_CG = sizeCG-1; pos_in_CG >= 0; pos_in_CG--)             
            if(SBH) sbh_possible[CG_blk_pos] = ((last_nz_pos_in_CG - first_nz_pos_in_CG)>=SBH_THRESHOLD)?1:0;
        } //end for (Ipp32s subset = last_scan_set; subset >= 0; subset--)//subset means CG
    }
    
    for (Ipp32s subset = last_nz_subset; subset >= 0; subset--) {//subset means CG
        Ipp32s sub_pos    = subset << LOG2_SCAN_SET_SIZE;
        Ipp32u CG_blk_pos = scanCG[ subset ];
        Ipp32u CG_pos_y   = CG_blk_pos >> log2_num_blk_side;
        Ipp32u CG_pos_x   = CG_blk_pos & (num_blk_side - 1);
        RDOQ_CoeffGroup_Report CG_report;
        if (sig_coeff_group_flag[ CG_blk_pos ] == 0) {
            Ipp32u  ctx_sig_inc = h265_quant_getSigCoeffGroupCtxInc  (
                    sig_coeff_group_flag,
                    CG_pos_x,
                    CG_pos_y,
                    0,
                    width);
            cost_base += GetCost_SigCoeffGroup(0, ctx_sig_inc);
            cost_coeff_group_sig[ subset ] = GetCost_SigCoeffGroup(0, ctx_sig_inc);
            for (Ipp32s pos_in_CG = sizeCG-1; pos_in_CG >= 0; pos_in_CG--) {
                Ipp32s scan_pos = sub_pos + pos_in_CG;
                cost_base += cost_zero_level[ scan_pos ];
                EncodeOneCoeff(0, scan_pos, type, &local_cabac); //update localc_cabac ctx
            }
            continue;
        }
        sig_coeff_group_flag[ CG_blk_pos ] = 0;
        if (rdoqCGZ) {
            //memset( &CG_report, 0, sizeof (RDOQ_CoeffGroup_Report));
            CG_report.num_nz_before_pos0 = 0;
            CG_report.cost_nz_levels = 0;
            CG_report.cost_zero_levels = 0;
            CG_report.cost_sig = 0;
            CG_report.cost_sig_pos0 = 0;
        }
        const Ipp32s pattern_sig_ctx = (Ipp32s)h265_quant_calcpattern_sig_ctx(
            sig_coeff_group_flag,
            CG_pos_x,
            CG_pos_y,
            width);

        if(SBH && sbh_possible[CG_blk_pos]) 
        {
            for (Ipp32s pos_in_CG = sizeCG-1; pos_in_CG >= 0; pos_in_CG--) {
                Ipp32s scan_pos = sub_pos + pos_in_CG;
                
                Ipp32s level_float        = srcScanOrder[ scan_pos ];
                level_float               = (Ipp32s)IPP_MIN((Ipp32s)abs(level_float) * uiQ , MAX_INT - qshift);

                Ipp32u max_abs_level = qlevels[ scan_pos ];

                if ( scan_pos<=last_nz_pos ) {
                    Ipp32u blk_pos  = scan[scan_pos];
                    Ipp32u  ctx_sig_inc = 0;
                    bool    isLast = true;

                    if( scan_pos != last_nz_pos ) {
                        isLast = false;
                        Ipp32u pos_y = blk_pos >> log2_tr_size;
                        Ipp32u pos_x = blk_pos & (width - 1);

                        ctx_sig_inc = (Ipp32u)h265_quant_getSigCtxInc(
                            pattern_sig_ctx,
                            scan_idx,
                            pos_x,
                            pos_y,
                            log2_tr_size,
                            type);
                        if (SBH)  {
                            sig_rate_delta[blk_pos] = m_cabacBits.significantBits[ctx_sig_inc][1] -
                                m_cabacBits.significantBits[ctx_sig_inc][0];
                        }
                    }

                    Ipp32u qval = GetBestQuantLevel(
                        cost_nz_level[ scan_pos ],
                        cost_zero_level[ scan_pos ],
                        cost_sig[ scan_pos ],
                        level_float,
                        max_abs_level,
                        ctx_sig_inc,
                        local_cabac,
                        qbits,
                        errScale,
                        isLast);

                    if (SBH) {
                        delta_u[blk_pos] = (level_float - ((Ipp32s)qval << qbits)) >> (qbits-8);
                        if( qval > 0 ) {
                            Ipp32s rate_cur = GetCost_EncodeOneCoeff(qval, local_cabac);
                            rate_inc_up   [blk_pos] = GetCost_EncodeOneCoeff(qval+1, local_cabac) - rate_cur;
                            rate_inc_down [blk_pos] = GetCost_EncodeOneCoeff(qval-1, local_cabac) - rate_cur;
                        } else {
                            rate_inc_up   [blk_pos] = m_cabacBits.greaterOneBits[4 * local_cabac.ctx_set + local_cabac.c1][0];
                        }
                    }

                    qlevels[ scan_pos ] = qval;
                    cost_base += cost_nz_level [ scan_pos ];

                    EncodeOneCoeff(qval, scan_pos, type, &local_cabac); //update localc_cabac ctx
                }
                if (rdoqCGZ) {
                    CG_report.cost_sig += cost_sig[ scan_pos ];

                    if (pos_in_CG == 0 ) {
                        CG_report.cost_sig_pos0 = cost_sig[ scan_pos ];
                    }
                }

                if (qlevels[ scan_pos ] ) {
                    sig_coeff_group_flag[ CG_blk_pos ] = 1;
                    if (rdoqCGZ) {
                        CG_report.cost_nz_levels   += cost_nz_level[ scan_pos ] - cost_sig[ scan_pos ];
                        CG_report.cost_zero_levels += cost_zero_level[ scan_pos ];
                        if ( pos_in_CG != 0 ) {
                            CG_report.num_nz_before_pos0++;
                        }
                    }
                }
            } //end for (Ipp32s pos_in_CG = sizeCG-1; pos_in_CG >= 0; pos_in_CG--)
        } 
        else 
        {
            for (Ipp32s pos_in_CG = sizeCG-1; pos_in_CG >= 0; pos_in_CG--) {
                Ipp32s scan_pos = sub_pos + pos_in_CG;

                Ipp32s level_float        = srcScanOrder[ scan_pos ];
                level_float               = (Ipp32s)IPP_MIN((Ipp32s)abs(level_float) * uiQ , MAX_INT - qshift);

                Ipp32u max_abs_level = qlevels[ scan_pos ];

                if ( scan_pos<=last_nz_pos ) {
                    Ipp32u  ctx_sig_inc = 0;
                    bool    isLast = true;

                    if( scan_pos != last_nz_pos ) {
                        isLast = false;
                        Ipp32u blk_pos  = scan[scan_pos];
                        Ipp32u pos_y = blk_pos >> log2_tr_size;
                        Ipp32u pos_x = blk_pos & (width - 1);

                        ctx_sig_inc = (Ipp32u)h265_quant_getSigCtxInc(
                            pattern_sig_ctx,
                            scan_idx,
                            pos_x,
                            pos_y,
                            log2_tr_size,
                            type);
                    }

                    Ipp32u qval = GetBestQuantLevel(
                        cost_nz_level[ scan_pos ],
                        cost_zero_level[ scan_pos ],
                        cost_sig[ scan_pos ],
                        level_float,
                        max_abs_level,
                        ctx_sig_inc,
                        local_cabac,
                        qbits,
                        errScale,
                        isLast);

                    qlevels[ scan_pos ] = qval;
                    cost_base += cost_nz_level [ scan_pos ];

                    EncodeOneCoeff(qval, scan_pos, type, &local_cabac); //update localc_cabac ctx
                }

                if (rdoqCGZ) {
                    CG_report.cost_sig += cost_sig[ scan_pos ];

                    if (pos_in_CG == 0 ) {
                        CG_report.cost_sig_pos0 = cost_sig[ scan_pos ];
                    }
                }

                if (qlevels[ scan_pos ] ) {
                    sig_coeff_group_flag[ CG_blk_pos ] = 1;
                    if (rdoqCGZ) {
                        CG_report.cost_nz_levels   += cost_nz_level[ scan_pos ] - cost_sig[ scan_pos ];
                        CG_report.cost_zero_levels += cost_zero_level[ scan_pos ];
                        if ( pos_in_CG != 0 ) {
                            CG_report.num_nz_before_pos0++;
                        }
                    }
                }
            } //end for (Ipp32s pos_in_CG = sizeCG-1; pos_in_CG >= 0; pos_in_CG--)
        } // SBH

        if (!rdoqCGZ) {
            if(last_nz_subset >= 0 && 0 == subset) {
                sig_coeff_group_flag[ CG_blk_pos ] = 1;
                continue;
            }
        } else {
            //-----------------------------------------------------
            //  try to zero current CG
            //-----------------------------------------------------
            if (last_nz_subset >= 0) {
                Ipp32u  ctx_sig_inc = h265_quant_getSigCoeffGroupCtxInc  (
                    sig_coeff_group_flag,
                    CG_pos_x,
                    CG_pos_y,
                    0,
                    width);

                if (sig_coeff_group_flag[ CG_blk_pos ] == 0) {
                    cost_base += GetCost_SigCoeffGroup(0, ctx_sig_inc) - CG_report.cost_sig;
                    cost_coeff_group_sig[ subset ] = GetCost_SigCoeffGroup(0, ctx_sig_inc);
                } else {
                    if (subset < last_nz_subset) { //skip the last coefficient group
                        if ( CG_report.num_nz_before_pos0 == 0 ) {
                            cost_base -= CG_report.cost_sig_pos0;
                            CG_report.cost_sig -= CG_report.cost_sig_pos0;
                        }

                        // try to zero current CG
                        Ipp64f cost_zero_CG = cost_base;

                        cost_coeff_group_sig[ subset ] = GetCost_SigCoeffGroup(1, ctx_sig_inc);
                        cost_base    += cost_coeff_group_sig[ subset ];

                        cost_zero_CG += GetCost_SigCoeffGroup(0, ctx_sig_inc);
                        cost_zero_CG += CG_report.cost_zero_levels;
                        cost_zero_CG -= CG_report.cost_nz_levels;
                        cost_zero_CG -= CG_report.cost_sig;// sig cost for all coeffs

                        // zerroed this block in case of better _zero_cost
                        if ( cost_zero_CG < cost_base ) {
                            sig_coeff_group_flag[ CG_blk_pos ] = 0;
                            cost_base = cost_zero_CG;

                            cost_coeff_group_sig[ subset ] = GetCost_SigCoeffGroup(0, ctx_sig_inc);

                            for (Ipp32s pos_in_CG = sizeCG-1; pos_in_CG >= 0; pos_in_CG--) {
                                Ipp32s scan_pos = sub_pos + pos_in_CG;

                                if (qlevels[ scan_pos ]) {
                                    qlevels [ scan_pos ] = 0;
                                    cost_nz_level[ scan_pos ] = cost_zero_level[ scan_pos ];
                                    cost_sig  [ scan_pos ] = 0;
                                }
                            }
                        } // end if ( cost_zero_CG < cost_base )
                    }
                }
            } // try to zero current CG
        }
    } //end for (Ipp32s subset = last_scan_set; subset >= 0; subset--)//subset means CG

    if ( last_nz_pos < 0 ) // empty blk
    {
        return;
    }

    //-----------------------------------------------------
    //  take into consediration CBF flag
    //-----------------------------------------------------
    Ipp64f cost_best   = 0;
    Ipp32u ctx_cbf_inc = 0;

    bool isRootCbf = (!m_pCU->IsIntra(abs_part_idx) && m_pCU->GetTransformIdx( abs_part_idx ) == 0);
    if( !isRootCbf )
    {
        ctx_cbf_inc = GetCtxQtCbf(type, m_pCU->GetTransformIdx(abs_part_idx));
    }

    cost_best  = cost_zero_blk;// + GetCost_Cbf(0, ctx_cbf_inc, isRootCbf);
//    cost_base += GetCost_Cbf(1, ctx_cbf_inc, isRootCbf);

    //-----------------------------------------------------
    //  SECOND PASS: determines lastXY in TU
    //-----------------------------------------------------
    Ipp32s scan_pos_best_lastp1 = 0;
    Ipp32s last_nz_subsetp1 = 0;
    bool is_found_last = false;
    for (Ipp32s subset = last_nz_subset; subset >= 0; subset--) {
        Ipp32u CG_blk_pos = scanCG[ subset ];

        cost_base -= cost_coeff_group_sig [ subset ];
        if (sig_coeff_group_flag[ CG_blk_pos ]) {
            for (Ipp32s pos_in_CG = sizeCG-1; pos_in_CG >= 0; pos_in_CG--) {
                Ipp32s scan_pos = (subset<<MLS_CG_SIZE) + pos_in_CG;

                if (scan_pos > last_nz_pos) {
                    continue;
                }

                if( qlevels[ scan_pos ] ) {
                    Ipp32u blk_pos  = scan[scan_pos];
                    Ipp32u   pos_y       = blk_pos >> log2_tr_size;
                    Ipp32u   pos_x       = blk_pos & ( width - 1 );

                    Ipp64f cost_last_xy = (scan_idx == COEFF_SCAN_VER) ? GetCost_LastXY( pos_y, pos_x) : GetCost_LastXY( pos_x, pos_y);
                    Ipp64f cost_total   = cost_base + cost_last_xy - cost_sig[ scan_pos ];

                    if( cost_total < cost_best ) {
                        scan_pos_best_lastp1  = scan_pos + 1;
                        cost_best     = cost_total;
                        last_nz_subsetp1 = subset;
                    }
                    if( qlevels[ scan_pos ] > 1 ) { //early termination
                        is_found_last = true;
                        break;
                    }

                    cost_base += cost_zero_level[ scan_pos ] - cost_nz_level[ scan_pos ];
                } else {
                    cost_base -= cost_sig[ scan_pos ];
                }
            }

            if (is_found_last) {
                break;
            }
        }
    }

    for ( Ipp32s scan_pos = 0; scan_pos < scan_pos_best_lastp1; scan_pos++ ) {
        Ipp32s blk_pos = scan[ scan_pos ];
        Ipp32s qval    = qlevels[ scan_pos ];
        pDst[blk_pos]  = (CoeffsType)Saturate(-32768, 32767, srcScanOrder[ scan_pos ] < 0 ? -qval : qval);

        if (SBH)
            abs_sum += qval;
    }

    if(SBH) {
        if (abs_sum < 2) return;

        Ipp32s qp_rem = h265_qp_rem[QP];
        Ipp32s qp6 = h265_qp6[QP];
        Ipp32s scale = h265_quant_table_inv[qp_rem] * h265_quant_table_inv[qp_rem] * (1<<(qp6<<1));
        Ipp64s rd_factor = (Ipp64s) (scale / m_lambda / 16 / (1<<(2*(m_bitDepth-8))) + 0.5);
        Ipp32s lastCG = -1;
        const Ipp32s last_scan_set = last_nz_subsetp1;
        for(Ipp32s subset = last_scan_set; subset >= 0; subset--) {
            Ipp32s sub_pos     = subset << LOG2_SCAN_SET_SIZE;
            Ipp32s last_nz_pos_in_CG = -1, first_nz_pos_in_CG = SCAN_SET_SIZE;
            Ipp32s pos_in_CG;
            abs_sum = 0;
            if (!sig_coeff_group_flag[ scanCG[ subset ] ])
                continue;

            for(pos_in_CG = SCAN_SET_SIZE-1; pos_in_CG >= 0; pos_in_CG--) {
                if(pDst[scan[sub_pos + pos_in_CG]]) {
                    last_nz_pos_in_CG = pos_in_CG;
                    break;
                }
            }

            if (last_nz_pos_in_CG == -1)
                continue;

            for(pos_in_CG = 0; pos_in_CG < SCAN_SET_SIZE; pos_in_CG++) {
                if(pDst[scan[sub_pos + pos_in_CG]]) {
                    first_nz_pos_in_CG = pos_in_CG;
                    break;
                }
            }

            for(pos_in_CG = first_nz_pos_in_CG; pos_in_CG <=last_nz_pos_in_CG; pos_in_CG++) {
                abs_sum += pDst[scan[sub_pos + pos_in_CG]];
            }

            if(lastCG==-1) {
                lastCG = 1; 
            } 

            if(last_nz_pos_in_CG - first_nz_pos_in_CG >= SBH_THRESHOLD) {
                Ipp8u sign_bit  = (pDst[scan[sub_pos + first_nz_pos_in_CG]] > 0 ? 0 : 1);
                Ipp8u sum_parity= (Ipp8u)(abs_sum & 0x1);
                if(sign_bit != sum_parity) {
                    Ipp64s min_cost_inc = IPP_MAX_64S, cur_cost = IPP_MAX_64S;
                    Ipp16s min_pos = -1, final_adjust = 0, cur_adjust = 0;

                    Ipp32s last_pos_in_CG = (lastCG == 1 ? last_nz_pos_in_CG : SCAN_SET_SIZE - 1);

                    for(pos_in_CG = last_pos_in_CG; pos_in_CG >= 0; pos_in_CG--) {
                        Ipp32u blk_pos   = scan[ sub_pos + pos_in_CG ];
                        if(pDst[blk_pos] != 0) {
                            Ipp64s cost_up   = rd_factor * ( - delta_u[blk_pos] ) + rate_inc_up  [blk_pos];
                            Ipp64s cost_down = rd_factor * (   delta_u[blk_pos] ) + rate_inc_down[blk_pos] 
                            -   ((abs(pDst[blk_pos]) == 1) ? sig_rate_delta[blk_pos] : 0);

                            if(lastCG == 1 && last_nz_pos_in_CG == pos_in_CG && abs(pDst[blk_pos]) == 1) {
                                cost_down -= (4<<15) ;
                            }

                            if(cost_up < cost_down) {  
                                cur_cost = cost_up;
                                cur_adjust = 1;
                            } else {
                                cur_adjust = -1;
                                if(pos_in_CG == first_nz_pos_in_CG && abs(pDst[blk_pos]) == 1) {
                                    cur_cost = IPP_MAX_64S;
                                } else {
                                    cur_cost = cost_down ; 
                                }
                            }
                        } else {
                            cur_cost = rd_factor * ( - (abs(delta_u[blk_pos])) ) + (1<<15) + rate_inc_up[blk_pos] + sig_rate_delta[blk_pos]; 
                            cur_adjust = 1 ;

                            if(pos_in_CG < first_nz_pos_in_CG) {
                                Ipp32u sign_bit_cur = (pSrc[blk_pos] >=0 ? 0 : 1);
                                if(sign_bit_cur != sign_bit) {
                                    cur_cost = IPP_MAX_64S;
                                }
                            }
                        }

                        if(cur_cost < min_cost_inc) {
                            min_cost_inc = cur_cost;
                            final_adjust = cur_adjust;
                            min_pos = blk_pos;
                        }
                    }

                    if(pDst[min_pos] == 32767 || pDst[min_pos] == -32768) {
                        final_adjust = -1;
                    }

                    pDst[min_pos] += pSrc[min_pos] >= 0 ? final_adjust : -final_adjust;
                }
            }

            if(lastCG == 1) {
                lastCG = 0;
            }
        }
    }

    return;

} // void RDOQuant::DoAlgorithm(...)


inline Ipp64f GetEPBits( )
{
    return 256 << 7;
}

template <typename PixType>
Ipp64f RDOQuant<PixType>::GetCost_EncodeOneCoeff(
    Ipp32u  qlevel,
    const RDOQCabacState & cabac)
{
    Ipp32u ctx_one_inc  = 4 * cabac.ctx_set + cabac.c1;
    Ipp32u ctx_abs_inc  = cabac.ctx_set;

    Ipp64f bit_cost  = GetEPBits();
    Ipp32u baseLevel =  (cabac.c1Idx < C1FLAG_NUMBER)? (2 + (cabac.c2Idx < C2FLAG_NUMBER)) : 1;

    if ( qlevel >= baseLevel ) {
        Ipp32u symbol = qlevel - baseLevel;
        Ipp32u length;

        if (symbol < (COEF_REMAIN_BIN_REDUCTION << cabac.go_rice_param)) {
            length = symbol>>cabac.go_rice_param;
            bit_cost += (length + 1 + cabac.go_rice_param) << 15;
        } else {
            length = cabac.go_rice_param;
            symbol  = symbol - ( COEF_REMAIN_BIN_REDUCTION << cabac.go_rice_param);
            Ipp32u val = 1u<<length;
            while (symbol >= val) {
                symbol -= val;
                val += val;
                length++;
            }

            bit_cost += (COEF_REMAIN_BIN_REDUCTION + length + 1 - cabac.go_rice_param + length) << 15;
        }

        if (cabac.c1Idx < C1FLAG_NUMBER) {
            bit_cost += m_cabacBits.greaterOneBits[ ctx_one_inc ][ 1 ];

            if (cabac.c2Idx < C2FLAG_NUMBER) {
                bit_cost += m_cabacBits.levelAbsBits[ ctx_abs_inc ][ 1 ];
            }
        }
    } else if( qlevel == 1 ) {
        bit_cost += m_cabacBits.greaterOneBits[ ctx_one_inc ][ 0 ];
    } else if( qlevel == 2 ) {
        bit_cost += m_cabacBits.greaterOneBits[ ctx_one_inc ][ 1 ];

        bit_cost += m_cabacBits.levelAbsBits[ ctx_abs_inc ][ 0 ];
    } else {
        bit_cost = 0;
    }

    return bit_cost;
} // Ipp64f RDOQuant::GetCost_EncodeOneCoeff(


template <typename PixType>
Ipp32u RDOQuant<PixType>::GetBestQuantLevel (
    Ipp64f&  cost_nz_level,
    Ipp64f&  cost_zero_level,
    Ipp64f&  cost_sig,
    Ipp32s   level_float,
    Ipp32u   max_abs_level,
    Ipp32u  ctx_sig_inc,
    const RDOQCabacState& local_cabac,
    Ipp32s  qbits,
    Ipp64f  errScale,
    bool    isLast)
{
    cost_nz_level       = MAX_DOUBLE;

    if( !isLast && max_abs_level < 3 ) {
        cost_sig      = GetCost_SigCoef(0, ctx_sig_inc);
        cost_nz_level = cost_zero_level + cost_sig;
        if( max_abs_level == 0 ) {
            return 0;
        }
    }

    Ipp64f curr_cost_sig = isLast ? 0 : GetCost_SigCoef( 1, ctx_sig_inc);
    Ipp32u best_qlevel  = 0;

    Ipp32u min_abs_level = ( max_abs_level > 1 ? max_abs_level - 1 : 1 );
    for( Ipp32s qlevel = (Ipp32s)max_abs_level; qlevel >= (Ipp32s)min_abs_level ; qlevel-- ) {
        Ipp64f quant_err = (Ipp64f)(level_float  - (qlevel << qbits ));

        Ipp64f bit_cost = GetCost(GetCost_EncodeOneCoeff( qlevel, local_cabac));

        Ipp64f curr_cost = (quant_err *quant_err)*errScale + bit_cost;
        curr_cost       += curr_cost_sig;

        if( curr_cost < cost_nz_level ) {
            best_qlevel   = qlevel;
            cost_nz_level = curr_cost;
            cost_sig  = curr_cost_sig;
        }
    }

    return best_qlevel;

} // Ipp32u RDOQuant::GetBestQuantLevel (...)


//template <typename PixType>
//Ipp64f RDOQuant<PixType>::GetCost(Ipp64f bit_cost)
//{
//    return m_lambda * bit_cost;
//} // Ipp64f RDOQuant::GetCost(Ipp64f bit_cost)


//template <typename PixType>
//Ipp64f RDOQuant<PixType>::GetCost_SigCoef(
//    Ipp16u  code,
//    Ipp32u  ctx_inc)
//{
//    return GetCost( m_cabacBits.significantBits[ ctx_inc ][ code ] );
//} // Ipp64f RDOQuant::GetCost_SigCoef(...)


//template <typename PixType>
//Ipp64f RDOQuant<PixType>::GetCost_SigCoeffGroup  (
//    Ipp16u  code,
//    Ipp32u  ctx_inc )
//{
//    return GetCost( m_cabacBits.significantCoeffGroupBits[ ctx_inc ][ code ] );
//} // Ipp64f RDOQuant::GetCost_SigCoeffGroup(...)


template <typename PixType>
Ipp64f RDOQuant<PixType>::GetCost_Cbf(
    Ipp16u  code,
    Ipp32u  ctx_inc,
    bool    isRootCbf)
{
    return GetCost( isRootCbf ? m_cabacBits.blkRootCbfBits[ctx_inc][code] : m_cabacBits.blkCbfBits[ctx_inc][code] );
} // Ipp64f RDOQuant::GetCost_Cbf(...)


template <typename PixType>
Ipp64f RDOQuant<PixType>::GetCost_LastXY(
    Ipp32u pos_x,
    Ipp32u pos_y)
{
    Ipp32u ctx_lastX_inc = h265_group_idx[pos_x];
    Ipp32u ctx_lastY_inc = h265_group_idx[pos_y];
    Ipp64f cost = m_cabacBits.lastXBits[ ctx_lastX_inc ] + m_cabacBits.lastYBits[ ctx_lastY_inc ];

    if( ctx_lastX_inc > 3 ) {
        cost += GetEPBits() * ((ctx_lastX_inc-2)>>1);
    }
    if( ctx_lastY_inc > 3 ) {
        cost += GetEPBits() * ((ctx_lastY_inc-2)>>1);
    }

    return GetCost( cost );

} // Ipp64f RDOQuant::GetCost_LastXY(...)


//---------------------------------------------------------
//            SBH TOOL
//---------------------------------------------------------
void h265_sign_bit_hiding(
    Ipp16s* levels,
    Ipp16s* coeffs,
    Ipp16u const *scan,
    Ipp32s* delta_u,
    Ipp32s width )
{
    Ipp32s lastCG = -1;

    const Ipp32s last_scan_set = (width * width - 1) >> LOG2_SCAN_SET_SIZE;

    for(Ipp32s subset = last_scan_set; subset >= 0; subset-- ) {
        Ipp32s sub_pos     = subset << LOG2_SCAN_SET_SIZE;
        Ipp32s last_nz_pos_in_CG = -1, first_nz_pos_in_CG = SCAN_SET_SIZE;
        Ipp32s pos_in_CG;

#ifdef __INTEL_COMPILER
        #pragma unroll(SCAN_SET_SIZE)
#endif
        for(pos_in_CG = 0; pos_in_CG <SCAN_SET_SIZE; pos_in_CG++ ) {
            if( levels[ scan[sub_pos+pos_in_CG] ] ) {
                first_nz_pos_in_CG = pos_in_CG;
                break;
            }
        }

#ifdef __INTEL_COMPILER
        #pragma unroll(SCAN_SET_SIZE)
#endif
        for(pos_in_CG = SCAN_SET_SIZE-1; pos_in_CG >= 0; pos_in_CG-- ) {
            if( levels[ scan[sub_pos+pos_in_CG] ] ) {
                last_nz_pos_in_CG = pos_in_CG;
                break;
            }
        }

        // if( (last_nz_pos_in_CG >= 0) && (-1 == lastCG) ) lastCG = 1;
        lastCG &= (last_nz_pos_in_CG >> 31) | 0x1;

        bool sign_hidden = (last_nz_pos_in_CG - first_nz_pos_in_CG >= SBH_THRESHOLD);
        if( sign_hidden ) {
            Ipp8u sign_bit  = (levels[ scan[sub_pos + first_nz_pos_in_CG] ] >= 0 ? 0 : 1);

            Ipp32s abs_sum = 0;
            for(pos_in_CG = first_nz_pos_in_CG; pos_in_CG <=last_nz_pos_in_CG; pos_in_CG++ ) {
                abs_sum += levels[ scan[ pos_in_CG + sub_pos ] ];
            }

            Ipp8u sum_parity= (Ipp8u)(abs_sum & 0x1);

            if( sign_bit != sum_parity ) {
                Ipp32s min_cost_inc = IPP_MAX_32S,  min_pos =-1;
                Ipp32s cur_cost = IPP_MAX_32S, cur_adjust = 0, final_adjust = 0;

                Ipp32s last_pos_in_CG = (lastCG==1 ? last_nz_pos_in_CG : SCAN_SET_SIZE-1);
                for( pos_in_CG =  last_pos_in_CG; pos_in_CG >= 0; pos_in_CG-- ) {
                    Ipp32u blk_pos   = scan[ sub_pos + pos_in_CG ];
                    if(levels[ blk_pos ] != 0) {
                        if(delta_u[blk_pos] > 0) {
                            cur_cost = - delta_u[blk_pos];
                            cur_adjust=1 ;
                        } else {
                            if(pos_in_CG == first_nz_pos_in_CG && 1 == abs(levels[blk_pos])) {
                                cur_cost=IPP_MAX_32S ;
                            } else {
                                cur_cost = delta_u[blk_pos];
                                cur_adjust =-1;
                            }
                        }
                    } else {
                        if(pos_in_CG < first_nz_pos_in_CG) {
                            Ipp8u local_sign_bit = (coeffs[blk_pos] >= 0 ? 0 : 1);
                            if(local_sign_bit != sign_bit ) {
                                cur_cost = IPP_MAX_32S;
                            } else {
                                cur_cost = - (delta_u[blk_pos]);
                                cur_adjust = 1;
                            }
                        } else {
                            cur_cost = - (delta_u[blk_pos])  ;
                            cur_adjust = 1 ;
                        }
                    }

                    if( cur_cost < min_cost_inc) {
                        min_cost_inc = cur_cost ;
                        final_adjust = cur_adjust ;
                        min_pos      = blk_pos ;
                    }
                } // for( pos_in_CG =

                if(levels[min_pos] == 32767 || levels[min_pos] == -32768) {
                    final_adjust = -1;
                }

                levels[min_pos] = (Ipp16s)(levels[min_pos] + (coeffs[min_pos] >= 0 ? final_adjust : -final_adjust));
            } // if( sign_bit != sum_parity )
        } // if( sign_hidden )

        if(lastCG == 1) {
            lastCG = 0 ;
        }

    } // for(Ipp32s subset = last_scan_set; subset >= 0; subset-- )

    return;

} // void h265_sign_bit_hiding(...)

} // namespace

#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
