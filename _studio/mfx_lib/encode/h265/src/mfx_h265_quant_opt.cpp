//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "ipp.h"
#include "mfx_h265_quant_opt.h"
#include "mfx_h265_optimization.h"

typedef Ipp8u PixType; 
typedef Ipp16s CoeffsType; 

//typedef signed char     Ipp8s;
//typedef unsigned char   Ipp8u;
//typedef signed short   Ipp16s;
//typedef unsigned short Ipp16u;
//typedef signed int     Ipp32s;

#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))
#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#include "mfx_h265_quant_opt.h"

static const Ipp8u h265_qp_rem[90]=
{
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5,
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5,
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5,
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5,
    0,   1,   2,   3,   4,   5,   0,   1,   2,   3,   4,   5,   0,   1,   2,  3,   4,   5
};

static const Ipp8u h265_qp6[90] =
{
    0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   2,   2,   2,  2,   2,   2,
    3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   5,   5,   5,  5,   5,   5,
    6,   6,   6,   6,   6,   6,   7,   7,   7,   7,   7,   7,   8,   8,   8,  8,   8,   8,
    9,   9,   9,   9,   9,   9,  10,  10,  10,  10,  10,  10,  11,  11,  11, 11,  11,  11,
   12,  12,  12,  12,  12,  12,  13,  13,  13,  13,  13,  13,  14,  14,  14, 14,  14,  14
};

static const Ipp8u h265_quant_table_inv[] =
{
    40, 45, 51, 57, 64, 72
};

static const Ipp16u h265_quant_table_fwd[] =
{
    26214, 23302, 20560, 18396, 16384, 14564
};

void h265_quant_inv(const CoeffsType *qcoeffs,
                    const Ipp16s *scaling_list, // aya: replaced 32s->16s to align with decoder/hevc_pp
                    CoeffsType *coeffs,
                    Ipp32s log2_tr_size,
                    Ipp32s bit_depth,
                    Ipp32s QP)
{
    Ipp32s qp_rem = h265_qp_rem[QP];
    Ipp32s qp6 = h265_qp6[QP];
    Ipp32s shift = bit_depth + log2_tr_size - 9;
    Ipp32s add;
    Ipp32s len = 1 << (log2_tr_size << 1);

    if (scaling_list)
    {
        shift += 4;
        if (shift > qp6)
        {
            add = 1 << (shift - qp6 - 1);
        }
        else
        {
            add = 0;
        }

//        const Ipp32s bitRange = MIN(15, ((Ipp32s)(12 + log2_tr_size + bit_depth - qp6)));
//        const Ipp32s levelLimit = 1 << bitRange;

        if (shift > qp6)
        {
            MFX_HEVC_COMMON::h265_QuantInv_ScaleList_RShift_16s(qcoeffs, scaling_list, coeffs, len, add, (shift -  qp6));
        }
        else
        {

            MFX_HEVC_COMMON::h265_QuantInv_ScaleList_LShift_16s(qcoeffs, scaling_list, coeffs, len, (qp6 - shift));
        }
    }
    else
    {
        add = 1 << (shift - 1);
        Ipp32s scale = h265_quant_table_inv[qp_rem] << qp6;
        
        MFX_HEVC_COMMON::h265_QuantInv_16s(qcoeffs, coeffs, len, scale, add, shift);
    }
}

void h265_quant_fwd_base(
    const CoeffsType *coeffs,
    CoeffsType *qcoeffs,
    Ipp32s log2_tr_size,
    Ipp32s bit_depth,
    Ipp32s is_slice_i,
    Ipp32s QP,

    Ipp32s*  delta,
    Ipp32u&  abs_sum )
{
    Ipp32s qp_rem = h265_qp_rem[QP];
    Ipp32s qp6 = h265_qp6[QP];
    Ipp32s len = 1 << (log2_tr_size << 1);
    //Ipp8s  sign;
    Ipp32s scaleLevel;
    Ipp32s scaleOffset;
    //Ipp32s qval;

    Ipp32s scale = 29 + qp6 - bit_depth - log2_tr_size;

    scaleLevel = h265_quant_table_fwd[qp_rem];
    scaleOffset = (is_slice_i ? 171 : 85) << (scale - 9);


    abs_sum = 0;

    if( delta )
    {
        abs_sum = (Ipp32u)MFX_HEVC_ENCODER::h265_QuantFwd_SBH_16s(coeffs, qcoeffs, delta, len, scaleLevel, scaleOffset, scale);
    }
    else
    {
        MFX_HEVC_ENCODER::h265_QuantFwd_16s(coeffs, qcoeffs, len, scaleLevel, scaleOffset, scale);
    }

} // void h265_quant_fwd_base(...)


void h265_sign_bit_hiding(
    Ipp16s* levels,
    Ipp16s* coeffs,
    Ipp16u const *scan,
    Ipp32s* delta_u,
    Ipp32s width,
    Ipp32s height )
{
    Ipp32s lastCG = -1;

    const Ipp32s last_scan_set = (width * height - 1) >> LOG2_SCAN_SET_SIZE;

    for(Ipp32s subset = last_scan_set; subset >= 0; subset-- )
    {
        Ipp32s sub_pos     = subset << LOG2_SCAN_SET_SIZE;
        Ipp32s last_nz_pos_in_CG = -1, first_nz_pos_in_CG = SCAN_SET_SIZE;
        Ipp32s abs_sum = 0;
        Ipp32s pos_in_CG;

        for(pos_in_CG = SCAN_SET_SIZE-1; pos_in_CG >= 0; pos_in_CG-- )
        {
            if( levels[ scan[sub_pos+pos_in_CG] ] )
            {
                last_nz_pos_in_CG = pos_in_CG;
                break;
            }
        }

        for(pos_in_CG = 0; pos_in_CG <SCAN_SET_SIZE; pos_in_CG++ )
        {
            if( levels[ scan[sub_pos+pos_in_CG] ] )
            {
                first_nz_pos_in_CG = pos_in_CG;
                break;
            }
        }

        for(pos_in_CG = first_nz_pos_in_CG; pos_in_CG <=last_nz_pos_in_CG; pos_in_CG++ )
        {
            abs_sum += levels[ scan[ pos_in_CG + sub_pos ] ];
        }

        if( (last_nz_pos_in_CG >= 0) && (-1 == lastCG) )
        {
            lastCG = 1;
        }

        bool sign_hidden = (last_nz_pos_in_CG - first_nz_pos_in_CG >= SBH_THRESHOLD);
        if( sign_hidden )
        {
            Ipp8u sign_bit  = (levels[ scan[sub_pos + first_nz_pos_in_CG] ] > 0 ? 0 : 1);
            Ipp8u sum_parity= (Ipp8u)(abs_sum & 0x1);

            if( sign_bit != sum_parity )
            {
                Ipp32s min_cost_inc = IPP_MAX_32S,  min_pos =-1;
                Ipp32s cur_cost = IPP_MAX_32S, cur_adjust = 0, final_adjust = 0;

                Ipp32s last_pos_in_CG = (lastCG==1 ? last_nz_pos_in_CG : SCAN_SET_SIZE-1);
                for( pos_in_CG =  last_pos_in_CG; pos_in_CG >= 0; pos_in_CG-- )
                {
                    Ipp32u blk_pos   = scan[ sub_pos + pos_in_CG ];
                    if(levels[ blk_pos ] != 0)
                    {
                        if(delta_u[blk_pos] > 0)
                        {
                            cur_cost = - delta_u[blk_pos];
                            cur_adjust=1 ;
                        }
                        else
                        {
                            if(pos_in_CG == first_nz_pos_in_CG && 1 == abs(levels[blk_pos]))
                            {
                                cur_cost=IPP_MAX_32S ;
                            }
                            else
                            {
                                cur_cost = delta_u[blk_pos];
                                cur_adjust =-1;
                            }
                        }
                    }
                    else
                    {
                        if(pos_in_CG < first_nz_pos_in_CG)
                        {
                            Ipp8u local_sign_bit = (coeffs[blk_pos] >= 0 ? 0 : 1);
                            if(local_sign_bit != sign_bit )
                            {
                                cur_cost = IPP_MAX_32S;
                            }
                            else
                            {
                                cur_cost = - (delta_u[blk_pos]);
                                cur_adjust = 1;
                            }
                        }
                        else
                        {
                            cur_cost = - (delta_u[blk_pos])  ;
                            cur_adjust = 1 ;
                        }
                    }

                    if( cur_cost < min_cost_inc)
                    {
                        min_cost_inc = cur_cost ;
                        final_adjust = cur_adjust ;
                        min_pos      = blk_pos ;
                    }
                } // for( pos_in_CG =

                if(levels[min_pos] == 32767 || levels[min_pos] == -32768)
                {
                    final_adjust = -1;
                }

                levels[min_pos] = (Ipp16s)(levels[min_pos] + (coeffs[min_pos] >= 0 ? final_adjust : -final_adjust));
            } // if( sign_bit != sum_parity )
        } // if( sign_hidden )

        if(lastCG == 1)
        {
            lastCG = 0 ;
        }

    } // for(Ipp32s subset = last_scan_set; subset >= 0; subset-- )

    return;

} // void h265_sign_bit_hiding(...)
//EOF

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
