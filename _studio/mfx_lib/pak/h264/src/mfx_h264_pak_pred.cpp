//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined (MFX_ENABLE_H264_VIDEO_PAK)

#include <assert.h>
#include "ippvc.h"
#include "mfx_h264_pak_utils.h"
#include "mfx_h264_pak_pred.h"

using namespace H264Pak;

enum
{
    PRED16x16_VERT   = 0,
    PRED16x16_HORZ   = 1,
    PRED16x16_DC     = 2,
    PRED16x16_PLANAR = 3
};

enum
{
    PRED8x8_DC     = 0,
    PRED8x8_HORZ   = 1,
    PRED8x8_VERT   = 2,
    PRED8x8_PLANAR = 3
};

enum
{
    LUMA_MB_MAX_COST            = 6,
    LUMA_8X8_MAX_COST           = 4,
    LUMA_COEFF_8X8_MAX_COST     = 4,
    LUMA_COEFF_MB_8X8_MAX_COST  = 6,
    CHROMA_COEFF_MAX_COST       = 7
};

static const mfxI16 g_ScanMatrixEnc[2][16] =
{
    {0,1,5,6,2,4,7,12,3,8,11,13,9,10,14,15},
    {0,2,8,12,1,5,9,13,3,6,10,14,4,7,11,15}
};

static const mfxI16 g_ScanMatrixEnc8x8[2][64] =
{
    {
         0,  1,  5,  6, 14, 15, 27, 28,
         2,  4,  7, 13, 16, 26, 29, 42,
         3,  8, 12, 17, 25, 30, 41, 43,
         9, 11, 18, 24, 31, 40, 44, 53,
        10, 19, 23, 32, 39, 45, 52, 54,
        20, 22, 33, 38, 46, 51, 55, 60,
        21, 34, 37, 47, 50, 56, 59, 61,
        35, 36, 48, 49, 57, 58, 62, 63
    },
    {
         0,  3,  8, 15, 22, 30, 38, 52,
         1,  4, 14, 21, 29, 37, 45, 53,
         2,  7, 16, 23, 31, 39, 46, 58,
         5,  9, 20, 28, 36, 44, 51, 59,
         6, 13, 24, 32, 40, 47, 54, 60,
        10, 17, 25, 33, 41, 48, 55, 61,
        11, 18, 26, 34, 42, 49, 56, 62,
        12, 19, 27, 35, 43, 50, 57, 63
    }
};

static const mfxI32 g_QpDiv6[88] = {
    0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9,
    10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13,
    13, 13, 14, 14, 14, 14
};

static const mfxI32 g_QpMod6[88] = {
    0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5,
    0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5,
    0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3
};

static const mfxU8 uBlockURPredOK[] =
{
    0xff, 0xff, 0xff, 0,
    0xff, 0xff, 0xff, 0,
    0xff, 0xff, 0xff, 0,
    0xff, 0,    0xff, 0
};

const mfxU8 g_CoeffImportance[16] =
{
    3,2,2,1,1,1,0,0,0,0,0,0,0,0,0,0
};

const mfxU8 g_CoeffImportance8x8[64] =
{
    3,3,3,3,2,2,2,2,2,2,2,2,1,1,1,1,
    1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

#define P_Z predPels[0]
#define P_A predPels[1]
#define P_B predPels[2]
#define P_C predPels[3]
#define P_D predPels[4]
#define P_E predPels[5]
#define P_F predPels[6]
#define P_G predPels[7]
#define P_H predPels[8]
#define P_I predPels[9]
#define P_J predPels[10]
#define P_K predPels[11]
#define P_L predPels[12]
#define P_M predPels[13]
#define P_N predPels[14]
#define P_O predPels[15]
#define P_P predPels[16]
#define P_Q predPels[17]
#define P_R predPels[18]
#define P_S predPels[19]
#define P_T predPels[20]
#define P_U predPels[21]
#define P_V predPels[22]
#define P_W predPels[23]
#define P_X predPels[24]

#define P_a dst[0 + 0 * step]
#define P_b dst[1 + 0 * step]
#define P_c dst[2 + 0 * step]
#define P_d dst[3 + 0 * step]
#define P_e dst[0 + 1 * step]
#define P_f dst[1 + 1 * step]
#define P_g dst[2 + 1 * step]
#define P_h dst[3 + 1 * step]
#define P_i dst[0 + 2 * step]
#define P_j dst[1 + 2 * step]
#define P_k dst[2 + 2 * step]
#define P_l dst[3 + 2 * step]
#define P_m dst[0 + 3 * step]
#define P_n dst[1 + 3 * step]
#define P_o dst[2 + 3 * step]
#define P_p dst[3 + 3 * step]

mfxI32 CalculateCoeffsCost(
    mfxI16* coeffs,
    mfxU32 count,
    const mfxI32* scan)
{
    mfxU32 cost = 0;
    const mfxU8* coeff_cost = (count == 64) ? g_CoeffImportance8x8 : g_CoeffImportance;

    for (mfxU32 k = 0; k < count; k++)
    {
        mfxI32 run = 0;

        for (; k < count && coeffs[scan[k]] == 0; k++, run++)
        {
        }

        if (k == count)
        {
            break;
        }

        if (abs(coeffs[scan[k]]) > 1)
        {
            return 9;
        }

        cost += coeff_cost[run];
    }

    return cost;
}

static void GetBlock4x4PredPels(
    bool leftMbAvail,
    bool aboveMbAvail,
    bool aboveRightMbAvail,
    mfxU8* recon,
    mfxU32 reconPitch,
    mfxU32 block,
    mfxU8* predPels)
{
    mfxI32 raster_block = g_Geometry[block].offBlk4x4;

    bool bLeft = IsOnLeftEdge(block) ? !leftMbAvail : false;
    bool bTop = IsOnTopEdge(block) ? !aboveMbAvail : false;

    if (bTop && bLeft)
    {
        P_A = P_B = P_C = P_D = P_I = P_J = P_K = P_L = 1 << 7;
    }
    else if (bTop && !bLeft)
    {
        for (mfxI32 i = 0; i < 4; i++)
        {
            predPels[i+1] = predPels[i+9] = (recon + i*reconPitch)[-1];
        }
    }
    else if (!bTop && bLeft)
    {
        for (mfxI32 i = 0; i < 4; i++)
        {
            predPels[i+1] = predPels[i+9] = (recon - reconPitch)[i];
        }
    }
    else
    {
        for (mfxI32 i = 0; i < 4; i++)
        {
            predPels[i+1] = (recon - reconPitch)[i];
            predPels[i+9] = (recon + i*reconPitch)[-1];
        }
        // for diagonal modes
        P_Z = (recon - reconPitch)[-1];
    }

    if (!bTop)
    {
        // Get EFGH, predictors pels above and to the right of the block.
        // Use D when EFGH are not valid, which is when the block is on
        // the right edge of the MB and the MB is at the right edge of
        // the picture; or when the block is on the right edge of the MB
        // but not in the top row.
        bool bRight = raster_block == 3 ? !aboveRightMbAvail : false;
        if (uBlockURPredOK[block] == 0 || bRight)
        {
            P_E = P_F = P_G = P_H = P_D;
        }
        else
        {
            P_E = (recon - reconPitch)[4];
            P_F = (recon - reconPitch)[5];
            P_G = (recon - reconPitch)[6];
            P_H = (recon - reconPitch)[7];
        }
    }
}

static void GetIntra4x4Prediction(
    mfxU8* dst,
    mfxU32 step,
    mfxU32 uMode,
    mfxU8* predPels)
{
    mfxU32 sum = 0;
    mfxU8 dcVal = 0;

    switch (uMode)
    {
    case 0: // vertical prediction (from above)
        P_a = P_e = P_i = P_m = P_A;
        P_b = P_f = P_j = P_n = P_B;
        P_c = P_g = P_k = P_o = P_C;
        P_d = P_h = P_l = P_p = P_D;
        break;

    case 1: // horizontal prediction (from left)
        P_a = P_b = P_c = P_d = P_I;
        P_e = P_f = P_g = P_h = P_J;
        P_i = P_j = P_k = P_l = P_K;
        P_m = P_n = P_o = P_p = P_L;
        break;

    case 2: // DC prediction
        {
            sum = 0;
            for (mfxU32 i = 0; i < 4; i++)
            {
                sum += predPels[i + 9]; // IJKL
                sum += predPels[i + 1]; // ABCD
            }

            dcVal = (mfxU8)((sum + 4) >> 3);
            P_a = P_b = P_c = P_d = dcVal;
            P_e = P_f = P_g = P_h = dcVal;
            P_i = P_j = P_k = P_l = dcVal;
            P_m = P_n = P_o = P_p = dcVal;
        }
        break;

    case 3:
        // mode 3: diagonal down/left prediction
        P_a =                   (P_A + ((P_B) << 1) + P_C + 2) >> 2;
        P_b = P_e =             (P_B + ((P_C) << 1) + P_D + 2) >> 2;
        P_c = P_f = P_i =       (P_C + ((P_D) << 1) + P_E + 2) >> 2;
        P_d = P_g = P_j = P_m = (P_D + ((P_E) << 1) + P_F + 2) >> 2;
        P_h = P_k = P_n =       (P_E + ((P_F) << 1) + P_G + 2) >> 2;
        P_l = P_o =             (P_F + ((P_G) << 1) + P_H + 2) >> 2;
        P_p =                   (P_G + ((P_H) << 1) + P_H + 2) >> 2;
        break;
    case 4:
        // mode 4: diagonal down/right prediction
        P_m =                   (P_J + ((P_K) << 1) + P_L + 2) >> 2;
        P_i = P_n =             (P_I + ((P_J) << 1) + P_K + 2) >> 2;
        P_e = P_j = P_o =       (P_Z + ((P_I) << 1) + P_J + 2) >> 2;
        P_a = P_f = P_k = P_p = (P_A + ((P_Z) << 1) + P_I + 2) >> 2;
        P_b = P_g = P_l =       (P_Z + ((P_A) << 1) + P_B + 2) >> 2;
        P_c = P_h =             (P_A + ((P_B) << 1) + P_C + 2) >> 2;
        P_d =                   (P_B + ((P_C) << 1) + P_D + 2) >> 2;
        break;
    case 5 :
        // mode 5: vertical-right prediction
        P_a = P_j = (P_Z + P_A + 1) >> 1;
        P_b = P_k = (P_A + P_B + 1) >> 1;
        P_c = P_l = (P_B + P_C + 1) >> 1;
        P_d =       (P_C + P_D + 1) >> 1;
        P_e = P_n = (P_I + ((P_Z) << 1) + P_A + 2) >> 2;
        P_f = P_o = (P_Z + ((P_A) << 1) + P_B + 2) >> 2;
        P_g = P_p = (P_A + ((P_B) << 1) + P_C + 2) >> 2;
        P_h =       (P_B + ((P_C) << 1) + P_D + 2) >> 2;
        P_i =       (P_Z + ((P_I) << 1) + P_J + 2) >> 2;
        P_m =       (P_I + ((P_J) << 1) + P_K + 2) >> 2;
        break;
    case 6 :
        // mode 6: horizontal-down prediction
        P_a = P_g = (P_Z + P_I + 1) >> 1;
        P_b = P_h = (P_I + ((P_Z) << 1) + P_A + 2) >> 2;
        P_c =       (P_Z + ((P_A) << 1) + P_B + 2) >> 2;
        P_d =       (P_A + ((P_B) << 1) + P_C + 2) >> 2;
        P_e = P_k = (P_I + P_J + 1) >> 1;
        P_f = P_l = (P_Z + ((P_I) << 1) + P_J + 2) >> 2;
        P_i = P_o = (P_J + P_K + 1) >> 1;
        P_j = P_p = (P_I + ((P_J) << 1) + P_K + 2) >> 2;
        P_m =       (P_K + P_L + 1) >> 1;
        P_n =       (P_J + ((P_K) << 1) + P_L + 2) >> 2;
        break;
    case 7 :
        // mode 7: vertical-left prediction
        P_a =       (P_A + P_B + 1) >> 1;
        P_b = P_i = (P_B + P_C + 1) >> 1;
        P_c = P_j = (P_C + P_D + 1) >> 1;
        P_d = P_k = (P_D + P_E + 1) >> 1;
        P_l =       (P_E + P_F + 1) >> 1;
        P_e =       (P_A + ((P_B) << 1) + P_C + 2) >> 2;
        P_f = P_m = (P_B + ((P_C) << 1) + P_D + 2) >> 2;
        P_g = P_n = (P_C + ((P_D) << 1) + P_E + 2) >> 2;
        P_h = P_o = (P_D + ((P_E) << 1) + P_F + 2) >> 2;
        P_p =       (P_E + ((P_F) << 1) + P_G + 2) >> 2;
        break;
    case 8 :
        // mode 8: horizontal-up prediction
        P_a =       (P_I + P_J + 1) >> 1;
        P_b =       (P_I + ((P_J) << 1) + P_K + 2) >> 2;
        P_c = P_e = (P_J + P_K + 1) >> 1;
        P_d = P_f = (P_J + ((P_K) << 1) + P_L + 2) >> 2;
        P_g = P_i = (P_K + P_L + 1) >> 1;
        P_h = P_j = (P_K + ((P_L) << 1) + P_L + 2) >> 2;
        P_k = P_l = P_m = P_n = P_o = P_p = (P_L);
        break;
    default:
        break;
    }
}

static void GetIntra8x8Prediction(
    mfxU8* dst,
    mfxU32 pitch,
    mfxU32 mode,
    mfxU8* predPels,
    mfxU32 predPelsMask)
{
#define PIX(X, Y) dst[X + Y * pitch]
    mfxU32 value = 0;

    switch (mode)
    {
    case 0: /*Vertical*/
        PIX(0, 0) = PIX(0, 1) = PIX(0, 2) = PIX(0, 3) = PIX(0, 4) = PIX(0, 5) = PIX(0, 6) = PIX(0, 7) = P_A;
        PIX(1, 0) = PIX(1, 1) = PIX(1, 2) = PIX(1, 3) = PIX(1, 4) = PIX(1, 5) = PIX(1, 6) = PIX(1, 7) = P_B;
        PIX(2, 0) = PIX(2, 1) = PIX(2, 2) = PIX(2, 3) = PIX(2, 4) = PIX(2, 5) = PIX(2, 6) = PIX(2, 7) = P_C;
        PIX(3, 0) = PIX(3, 1) = PIX(3, 2) = PIX(3, 3) = PIX(3, 4) = PIX(3, 5) = PIX(3, 6) = PIX(3, 7) = P_D;
        PIX(4, 0) = PIX(4, 1) = PIX(4, 2) = PIX(4, 3) = PIX(4, 4) = PIX(4, 5) = PIX(4, 6) = PIX(4, 7) = P_E;
        PIX(5, 0) = PIX(5, 1) = PIX(5, 2) = PIX(5, 3) = PIX(5, 4) = PIX(5, 5) = PIX(5, 6) = PIX(5, 7) = P_F;
        PIX(6, 0) = PIX(6, 1) = PIX(6, 2) = PIX(6, 3) = PIX(6, 4) = PIX(6, 5) = PIX(6, 6) = PIX(6, 7) = P_G;
        PIX(7, 0) = PIX(7, 1) = PIX(7, 2) = PIX(7, 3) = PIX(7, 4) = PIX(7, 5) = PIX(7, 6) = PIX(7, 7) = P_H;
        break;
    case 1: /* Horizontal */
        PIX(0, 0) = PIX(1, 0) = PIX(2, 0) = PIX(3, 0) = PIX(4, 0) = PIX(5, 0) = PIX(6, 0) = PIX(7, 0) = P_Q;
        PIX(0, 1) = PIX(1, 1) = PIX(2, 1) = PIX(3, 1) = PIX(4, 1) = PIX(5, 1) = PIX(6, 1) = PIX(7, 1) = P_R;
        PIX(0, 2) = PIX(1, 2) = PIX(2, 2) = PIX(3, 2) = PIX(4, 2) = PIX(5, 2) = PIX(6, 2) = PIX(7, 2) = P_S;
        PIX(0, 3) = PIX(1, 3) = PIX(2, 3) = PIX(3, 3) = PIX(4, 3) = PIX(5, 3) = PIX(6, 3) = PIX(7, 3) = P_T;
        PIX(0, 4) = PIX(1, 4) = PIX(2, 4) = PIX(3, 4) = PIX(4, 4) = PIX(5, 4) = PIX(6, 4) = PIX(7, 4) = P_U;
        PIX(0, 5) = PIX(1, 5) = PIX(2, 5) = PIX(3, 5) = PIX(4, 5) = PIX(5, 5) = PIX(6, 5) = PIX(7, 5) = P_V;
        PIX(0, 6) = PIX(1, 6) = PIX(2, 6) = PIX(3, 6) = PIX(4, 6) = PIX(5, 6) = PIX(6, 6) = PIX(7, 6) = P_W;
        PIX(0, 7) = PIX(1, 7) = PIX(2, 7) = PIX(3, 7) = PIX(4, 7) = PIX(5, 7) = PIX(6, 7) = PIX(7, 7) = P_X;
        break;
    case 2: /* DC */
        if ((predPelsMask & 0x01fe01fe) == 0x01fe01fe)
        {
            value = (
                P_A + P_B + P_C + P_D + P_E + P_F + P_G + P_H +
                P_Q + P_R + P_S + P_T + P_U + P_V + P_W + P_X + 8) >> 4;
        }
        else if ((predPelsMask & 0x01fe0000) == 0x01fe0000)
        {
            value = (P_Q + P_R + P_S + P_T + P_U + P_V + P_W + P_X + 4) >> 3;
        }
        else if ((predPelsMask & 0x000001fe) == 0x000001fe)
        {
            value = (P_A + P_B + P_C + P_D + P_E + P_F + P_G + P_H + 4) >> 3;
        }
        else
        {
            value = 1 << 7;
        }

        for (mfxU32 i = 0; i < 8; i++)
        {
            memset(dst + i * pitch, value, 8);
        }

        break;
    case 3: /* Diagonal Down Left */
        PIX(0, 0) = (P_A + 2 * P_B + P_C + 2) >> 2;
        PIX(0, 1) = PIX(1, 0) = (P_B + 2 * P_C + P_D + 2) >> 2;
        PIX(0, 2) = PIX(1, 1) = PIX(2, 0) = (P_C + 2*P_D + P_E + 2) >> 2;
        PIX(0, 3) = PIX(1, 2) = PIX(2, 1) = PIX(3, 0) = (P_D + 2 * P_E + P_F + 2) >> 2;
        PIX(0, 4) = PIX(1, 3) = PIX(2, 2) = PIX(3, 1) = PIX(4, 0) = (P_E + 2 * P_F + P_G + 2) >> 2;
        PIX(0, 5) = PIX(1, 4) = PIX(2, 3) = PIX(3, 2) = PIX(4, 1) = PIX(5, 0) = (P_F + 2 * P_G + P_H + 2) >> 2;
        PIX(0, 6) = PIX(1, 5) = PIX(2, 4) = PIX(3, 3) = PIX(4, 2) = PIX(5, 1) = PIX(6, 0) = (P_G + 2 * P_H + P_I + 2) >> 2;
        PIX(0, 7) = PIX(1, 6) = PIX(2, 5) = PIX(3, 4) = PIX(4, 3) = PIX(5, 2) = PIX(6, 1) = PIX(7, 0) = (P_H + 2 * P_I + P_J + 2) >> 2;
        PIX(1, 7) = PIX(2, 6) = PIX(3, 5) = PIX(4, 4) = PIX(5, 3) = PIX(6, 2) = PIX(7, 1) = (P_I + 2 * P_J + P_K + 2) >> 2;
        PIX(2, 7) = PIX(3, 6) = PIX(4, 5) = PIX(5, 4) = PIX(6, 3) = PIX(7, 2) = (P_J + 2 * P_K + P_L + 2) >> 2;
        PIX(3, 7) = PIX(4, 6) = PIX(5, 5) = PIX(6, 4) = PIX(7, 3) = (P_K + 2 * P_L + P_M + 2) >> 2;
        PIX(4, 7) = PIX(5, 6) = PIX(6, 5) = PIX(7, 4) = (P_L + 2 * P_M + P_N + 2) >> 2;
        PIX(5, 7) = PIX(6, 6) = PIX(7, 5) = (P_M + 2 * P_N + P_O + 2) >> 2;
        PIX(6, 7) = PIX(7, 6) = (P_N + 2 * P_O + P_P + 2) >> 2;
        PIX(7, 7) = (P_O + 3 * P_P + 2) >> 2;
        break;
    case 4: /* Diagonal Down Right */
        PIX(1, 0) = PIX(2, 1) = PIX(3, 2) = PIX(4, 3) = PIX(5, 4) = PIX(6, 5) = PIX(7, 6) = (P_Z + 2 * P_A + P_B + 2) >> 2;
        PIX(2, 0) = PIX(3, 1) = PIX(4, 2) = PIX(5, 3) = PIX(6, 4) = PIX(7, 5) = (P_A + 2 * P_B + P_C + 2) >> 2;
        PIX(3, 0) = PIX(4, 1) = PIX(5, 2) = PIX(6, 3) = PIX(7, 4) = (P_B + 2 * P_C + P_D + 2) >> 2;
        PIX(4, 0) = PIX(5, 1) = PIX(6, 2) = PIX(7, 3) = (P_C + 2 * P_D + P_E + 2) >> 2;
        PIX(5, 0) = PIX(6, 1) = PIX(7, 2) = (P_D + 2 * P_E + P_F + 2) >> 2;
        PIX(6, 0) = PIX(7, 1) = (P_E + 2 * P_F + P_G + 2) >> 2;
        PIX(7, 0) = (P_F + 2 * P_G + P_H + 2) >> 2;
        PIX(0, 0) = PIX(1, 1) = PIX(2, 2) = PIX(3, 3) = PIX(4, 4) = PIX(5, 5) = PIX(6, 6) = PIX(7, 7) = (P_A + 2 * P_Z + P_Q + 2) >> 2;
        PIX(0, 1) = PIX(1, 2) = PIX(2, 3) = PIX(3, 4) = PIX(4, 5) = PIX(5, 6) = PIX(6, 7) = (P_Z + 2 * P_Q + P_R + 2) >> 2;
        PIX(0, 2) = PIX(1, 3) = PIX(2, 4) = PIX(3, 5) = PIX(4, 6) = PIX(5, 7) = (P_Q + 2 * P_R + P_S + 2) >> 2;
        PIX(0, 3) = PIX(1, 4) = PIX(2, 5) = PIX(3, 6) = PIX(4, 7) = (P_R + 2 * P_S + P_T + 2) >> 2;
        PIX(0, 4) = PIX(1, 5) = PIX(2, 6) = PIX(3, 7) = (P_S + 2 * P_T + P_U + 2) >> 2;
        PIX(0, 5) = PIX(1, 6) = PIX(2, 7) = (P_T + 2 * P_U + P_V + 2) >> 2;
        PIX(0, 6) = PIX(1, 7) = (P_U + 2 * P_V + P_W + 2) >> 2;
        PIX(0, 7) = (P_V + 2 * P_W + P_X + 2) >> 2;
        break;
    case 5: /* Vertical Right */
        PIX(1, 1) = PIX(2, 3) = PIX(3, 5) = PIX(4, 7) = (P_Z + 2 * P_A + P_B + 2) >> 2;
        PIX(0, 0) = PIX(1, 2) = PIX(2, 4) = PIX(3, 6) = (P_Z + P_A + 1) >> 1;
        PIX(2, 1) = PIX(3, 3) = PIX(4, 5) = PIX(5, 7) = (P_A + 2 * P_B + P_C + 2) >> 2;
        PIX(1, 0) = PIX(2, 2) = PIX(3, 4) = PIX(4, 6) = (P_A + P_B + 1) >> 1;
        PIX(3, 1) = PIX(4, 3) = PIX(5, 5) = PIX(6, 7) = (P_B + 2 * P_C + P_D + 2) >> 2;
        PIX(2, 0) = PIX(3, 2) = PIX(4, 4) = PIX(5, 6) = (P_B + P_C + 1) >> 1;
        PIX(4, 1) = PIX(5, 3) = PIX(6, 5) = PIX(7, 7) = (P_C + 2 * P_D + P_E + 2) >> 2;
        PIX(3, 0) = PIX(4, 2) = PIX(5, 4) = PIX(6, 6) = (P_C + P_D + 1) >> 1;
        PIX(5, 1) = PIX(6, 3) = PIX(7, 5) = (P_D + 2 * P_E + P_F + 2) >> 2;
        PIX(4, 0) = PIX(5, 2) = PIX(6, 4) = PIX(7, 6) = (P_D + P_E + 1) >> 1;
        PIX(6, 1) = PIX(7, 3) = (P_E + 2 * P_F + P_G + 2) >> 2;
        PIX(5, 0) = PIX(6, 2) = PIX(7, 4) = (P_E + P_F + 1) >> 1;
        PIX(7, 1) = (P_F + 2 * P_G + P_H + 2) >> 2;
        PIX(6, 0) = PIX(7, 2) = (P_F + P_G + 1) >> 1;
        PIX(7, 0) = (P_G + P_H + 1) >> 1;
        PIX(0, 1) = PIX(1, 3) = PIX(2, 5) = PIX(3, 7) = (P_Q + 2 * P_Z + P_A + 2) >> 2;
        PIX(0, 2) = PIX(1, 4) = PIX(2, 6) = (P_R + 2 * P_Q + P_Z + 2) >> 2;
        PIX(0, 3) = PIX(1, 5) = PIX(2, 7) = (P_S + 2 * P_R + P_Q + 2) >> 2;
        PIX(0, 4) = PIX(1, 6) = (P_T + 2 * P_S + P_R + 2) >> 2;
        PIX(0, 5) = PIX(1, 7) = (P_U + 2 * P_T + P_S + 2) >> 2;
        PIX(0, 6) = (P_V + 2 * P_U + P_T + 2) >> 2;
        PIX(0, 7) = (P_W + 2 * P_V + P_U + 2) >> 2;
        break;
    case 6: /* Horizontal Down */
        PIX(1, 1) = PIX(3, 2) = PIX(5, 3) = PIX(7, 4) = (P_Z + 2 * P_Q + P_R + 2) >> 2;
        PIX(0, 0) = PIX(2, 1) = PIX(4, 2) = PIX(6, 3) = (P_Z + P_Q + 1) >> 1;
        PIX(2, 0) = PIX(4, 1) = PIX(6, 2) = (P_B + 2 * P_A + P_Z + 2) >> 2;
        PIX(3, 0) = PIX(5, 1) = PIX(7, 2) = (P_C + 2 * P_B + P_A + 2) >> 2;
        PIX(4, 0) = PIX(6, 1) = (P_D + 2 * P_C + P_B + 2) >> 2;
        PIX(5, 0) = PIX(7, 1) = (P_E + 2 * P_D + P_C + 2) >> 2;
        PIX(6, 0) = (P_F + 2 * P_E + P_D + 2) >> 2;
        PIX(7, 0) = (P_G + 2 * P_F + P_E + 2) >> 2;
        PIX(1, 0) = PIX(3, 1) = PIX(5, 2) = PIX(7, 3) = (P_Q + 2 * P_Z + P_A + 2) >> 2;
        PIX(1, 2) = PIX(3, 3) = PIX(5, 4) = PIX(7, 5) = (P_Q + 2 * P_R + P_S + 2) >> 2;
        PIX(0, 1) = PIX(2, 2) = PIX(4, 3) = PIX(6, 4) = (P_Q + P_R + 1) >> 1;
        PIX(1, 3) = PIX(3, 4) = PIX(5, 5) = PIX(7, 6) = (P_R + 2 * P_S + P_T + 2) >> 2;
        PIX(0, 2) = PIX(2, 3) = PIX(4, 4) = PIX(6, 5) = (P_R + P_S + 1) >> 1;
        PIX(1, 4) = PIX(3, 5) = PIX(5, 6) = PIX(7, 7) = (P_S + 2 * P_T + P_U + 2) >> 2;
        PIX(0, 3) = PIX(2, 4) = PIX(4, 5) = PIX(6, 6) = (P_S + P_T + 1) >> 1;
        PIX(1, 5) = PIX(3, 6) = PIX(5, 7) = (P_T + 2 * P_U + P_V + 2) >> 2;
        PIX(0, 4) = PIX(2, 5) = PIX(4, 6) = PIX(6, 7) = (P_T + P_U + 1) >> 1;
        PIX(1, 6) = PIX(3, 7) = (P_U + 2 * P_V + P_W + 2) >> 2;
        PIX(0, 5) = PIX(2, 6) = PIX(4, 7) = (P_U + P_V + 1) >> 1;
        PIX(1, 7) = (P_V + 2 * P_W + P_X + 2) >> 2;
        PIX(0, 6) = PIX(2, 7) = (P_V + P_W + 1) >> 1;
        PIX(0, 7) = (P_W + P_X + 1) >> 1;
        break;
    case 7: /* Vertical Left */
        PIX(0, 1) = (P_A + 2 * P_B + P_C + 2) >> 2;
        PIX(0, 0) = (P_A + P_B + 1) >> 1;
        PIX(1, 1) = PIX(0, 3) = (P_B + 2 * P_C + P_D + 2) >> 2;
        PIX(1, 0) = PIX(0, 2) = (P_B + P_C + 1) >> 1;
        PIX(2, 1) = PIX(1, 3) = PIX(0, 5) = (P_C + 2 * P_D + P_E + 2) >> 2;
        PIX(2, 0) = PIX(1, 2) = PIX(0, 4) = (P_C + P_D + 1) >> 1;
        PIX(3, 1) = PIX(2, 3) = PIX(1, 5) = PIX(0, 7) = (P_D + 2 * P_E + P_F + 2) >> 2;
        PIX(3, 0) = PIX(2, 2) = PIX(1, 4) = PIX(0, 6) = (P_D + P_E + 1) >> 1;
        PIX(4, 1) = PIX(3, 3) = PIX(2, 5) = PIX(1, 7) = (P_E + 2 * P_F + P_G + 2) >> 2;
        PIX(4, 0) = PIX(3, 2) = PIX(2, 4) = PIX(1, 6) = (P_E + P_F + 1) >> 1;
        PIX(5, 1) = PIX(4, 3) = PIX(3, 5) = PIX(2, 7) = (P_F + 2 * P_G + P_H + 2) >> 2;
        PIX(5, 0) = PIX(4, 2) = PIX(3, 4) = PIX(2, 6) = (P_F + P_G + 1) >> 1;
        PIX(6, 1) = PIX(5, 3) = PIX(4, 5) = PIX(3, 7) = (P_G + 2 * P_H + P_I + 2) >> 2;
        PIX(6, 0) = PIX(5, 2) = PIX(4, 4) = PIX(3, 6) = (P_G + P_H + 1) >> 1;
        PIX(7, 1) = PIX(6, 3) = PIX(5, 5) = PIX(4, 7) = (P_H + 2 * P_I + P_J + 2) >> 2;
        PIX(7, 0) = PIX(6, 2) = PIX(5, 4) = PIX(4, 6) = (P_H + P_I + 1) >> 1;
        PIX(7, 3) = PIX(6, 5) = PIX(5, 7) = (P_I + 2 * P_J + P_K + 2) >> 2;
        PIX(7, 2) = PIX(6, 4) = PIX(5, 6) = (P_I + P_J + 1) >> 1;
        PIX(7, 5) = PIX(6, 7) = (P_J + 2 * P_K + P_L + 2) >> 2;
        PIX(7, 4) = PIX(6, 6) = (P_J + P_K + 1) >> 1;
        PIX(7, 7) = (P_K + 2 * P_L + P_M + 2) >> 2;
        PIX(7, 6) = (P_K + P_L + 1) >> 1;
        break;
    case 8: /* Horizontal Up */
        PIX(1, 0) = (P_Q + 2 * P_R + P_S + 2) >> 2;
        PIX(0, 0) = (P_Q + P_R + 1) >> 1;
        PIX(3, 0) = PIX(1, 1) = (P_R + 2 * P_S + P_T + 2) >> 2;
        PIX(2, 0) = PIX(0, 1) = (P_R + P_S + 1) >> 1;
        PIX(5, 0) = PIX(3, 1) = PIX(1, 2) = (P_S + 2 * P_T + P_U + 2) >> 2;
        PIX(4, 0) = PIX(2, 1) = PIX(0, 2) = (P_S + P_T + 1) >> 1;
        PIX(7, 0) = PIX(5, 1) = PIX(3, 2) = PIX(1, 3) = (P_T + 2 * P_U + P_V + 2) >> 2;
        PIX(6, 0) = PIX(4, 1) = PIX(2, 2) = PIX(0, 3) = (P_T + P_U + 1) >> 1;
        PIX(7, 1) = PIX(5, 2) = PIX(3, 3) = PIX(1, 4) = (P_U + 2 * P_V + P_W + 2) >> 2;
        PIX(6, 1) = PIX(4, 2) = PIX(2, 3) = PIX(0, 4) = (P_U + P_V + 1) >> 1;
        PIX(7, 2) = PIX(5, 3) = PIX(3, 4) = PIX(1, 5) = (P_V + 2 * P_W + P_X + 2) >> 2;
        PIX(6, 2) = PIX(4, 3) = PIX(2, 4) = PIX(0, 5) = (P_V + P_W + 1) >> 1;
        PIX(6, 3) = PIX(4, 4) = PIX(2, 5) = PIX(0, 6) = (P_W + P_X + 1) >> 1;
        PIX(7, 3) = PIX(5, 4) = PIX(3, 5) = PIX(1, 6) = (P_W + 3 * P_X + 2) >> 2;
        PIX(7, 7) = PIX(7, 6) = PIX(7, 5) = PIX(7, 4) =
        PIX(6, 7) = PIX(6, 6) = PIX(6, 5) = PIX(6, 4) =
        PIX(5, 7) = PIX(5, 6) = PIX(5, 5) = PIX(4, 7) =
        PIX(4, 6) = PIX(4, 5) = PIX(3, 7) = PIX(3, 6) =
        PIX(2, 7) = PIX(2, 6) = PIX(1, 7) = PIX(0, 7) = P_X;
        break;
    }
#undef PIX
}

void Filter8x8Pels(
    mfxU8* predPels,
    mfxU32 predPelsMask)
{
    mfxU8 predPelsFiltered[25] = { 0 };

    if ((predPelsMask & 0x1FE) == 0x1FE)
    {
        /* 0..7 */
        if (predPelsMask & 0x01)
        {
            predPelsFiltered[1 + 0] = (predPels[0] + 2 * predPels[1 + 0] + predPels[1 + 1] + 2) >> 2;
        }
        else
        {
            predPelsFiltered[1 + 0] = (3 * predPels[1 + 0] + predPels[1 + 1] + 2) >> 2;
        }

        for (mfxU32 i = 1; i < 8; i++)
        {
            predPelsFiltered[1 + i] = (predPels[1 + i - 1] + 2 * predPels[1 + i] + predPels[1 + i + 1] + 2) >> 2;
        }
    }

    if ((predPelsMask & 0x0001FF00) == 0x0001FF00)
    {
        /* 7..15 */
        for (mfxU32 i = 0; i < 7; i++)
        {
            predPelsFiltered[9 + i] = (predPels[9 + i - 1] + 2 * predPels[9 + i] + predPels[9 + i + 1] + 2) >> 2;
        }

        predPelsFiltered[9 + 7]=(predPels[9 + 6] + 3 * predPels[9 + 7] + 2) >> 2;
    }

    if (predPelsMask & 0x01)
    {
        /* 0 */
        if ((predPelsMask & 0x00020002) == 0x00020002)
        {
            predPelsFiltered[0] = (predPels[1 + 0] + 2 * predPels[0] + predPels[17 + 0] + 2) >> 2;
        }
        else
        {
            if (predPelsMask & 0x02)
            {
                predPelsFiltered[0] = (3 * predPels[0] + predPels[1 + 0] + 2) >> 2;
            }
            else if (predPelsMask & 0x00020000)
            {
                predPelsFiltered[0] = (3 * predPels[0] + predPels[17 + 0] + 2) >> 2;
            }
        }
    }

    if ((predPelsMask & 0x01FE0000) == 0x01FE0000)
    {
        /* y 0..7 */
        if (predPelsMask & 0x01)
        {
            predPelsFiltered[17 + 0] = (predPels[0] + 2 * predPels[17 + 0] + predPels[17 + 1] + 2) >> 2;
        }
        else
        {
            predPelsFiltered[17 + 0] = (3 * predPels[17 + 0] + predPels[17 + 1] + 2) >> 2;
        }

        for (mfxU32 i = 1; i < 7; i++)
        {
            predPelsFiltered[17 + i] = (predPels[17 + i-1] + 2 * predPels[17 + i] + predPels[17 + i + 1] + 2) >> 2;
        }

        predPelsFiltered[17 + 7] = (predPels[17 + 6] + 3 * predPels[17 + 7] + 2) >> 2;
    }

    MFX_INTERNAL_CPY(predPels, predPelsFiltered, 25);
}

static void PlanarPredictLuma(
    mfxU8* recon,
    mfxU32 reconPitch,
    mfxU8* pred,
    mfxU32 predPitch,
    mfxU32 bitDepth)
{
    mfxI32 iH = 0;
    mfxI32 iV = 0;
    for (mfxI32 x = 1; x <= 8; x ++)
    {
        iH += x*((recon - mfxI32(reconPitch) - 1)[8 + x] - (recon - mfxI32(reconPitch) - 1)[8 - x]);
        iV += x*((recon + mfxI32(reconPitch) * (8 + x - 1))[-1] - (recon + mfxI32(reconPitch) * (8 - x - 1))[-1]);
    }

    mfxI32 a = 16 * ((recon - mfxI32(reconPitch) - 1)[16] + (recon + mfxI32(reconPitch) * (16 - 1))[-1]);
    mfxI32 b = (5 * iH + 32) >> 6;
    mfxI32 c = (5 * iV + 32) >> 6;

    mfxI32 max_pix_value = (1 << bitDepth) - 1;
    for (mfxI32 y = 0; y < 16; y++)
    {
        for (mfxI32 x = 0; x < 16; x++)
        {
            mfxI32 temp = (a + (x - 7) * b + (y - 7) * c + 16) >> 5;
            temp = (temp > max_pix_value) ? max_pix_value : temp;
            temp = (temp < 0) ? 0 : temp;
            pred[x + y * predPitch] = (mfxU8)temp;
        }
    }
}

static void GetIntra16x16Prediction(
    mfxU32 lumaBitDepth,
    bool topAvailable,
    bool leftAvailable,
    mfxU8* pRef,
    mfxU32 pitchPixels,
    mfxU8 mode,
    mfxU8* pPredBuf,
    mfxU32 predPitch)
{
    mfxU8* pAbove;
    mfxU8* pLeft;
    mfxU32 row;
    mfxU32 uSum = 0;

    switch (mode)
    {
    case PRED16x16_DC:
        if (!leftAvailable && !topAvailable)
        {
            for (mfxU32 i = 0; i < 16; i++)
            {
                memset(pPredBuf + predPitch * i, 1 << (lumaBitDepth - 1), 16);
            }
        }
        else
        {
            if (topAvailable)
            {
                pAbove = pRef - pitchPixels;
                for (row = 0; row < 16; row++)
                {
                    uSum += pAbove[row];
                }
            }

            if (leftAvailable)
            {
                pLeft = pRef - 1;
                for (row = 0; row < 16; row++, pLeft += pitchPixels)
                {
                    uSum += *pLeft;
                }
            }

            if (!topAvailable || !leftAvailable)
            {
                uSum <<= 1;
            }
            uSum = (uSum + 16) >> 5;

            for (mfxU32 i = 0; i < 16; i++)
            {
                memset(pPredBuf + predPitch * i, uSum, 16);
            }
        }
        break;

    case PRED16x16_VERT:
        if (topAvailable)
        {
            pAbove = pRef - pitchPixels;
            for (row = 0; row < 16; row++)
            {
                MFX_INTERNAL_CPY(pPredBuf + predPitch * row, pAbove, 16);
            }
        }
        break;

    case PRED16x16_HORZ:
        if (leftAvailable)
        {
            pLeft = pRef - 1;
            for (row = 0; row < 16; row++, pLeft += pitchPixels)
            {
                memset(pPredBuf + predPitch * row, *pLeft, 16);
            }
        }
        break;

    case PRED16x16_PLANAR:
        PlanarPredictLuma(pRef, pitchPixels, pPredBuf, predPitch, lumaBitDepth);
        break;

    default:
        assert(!"bad intra prediction mode"); // Can't find a suitable intra prediction mode!!!
        break;
    }
}

static void PlanarPredictChromaNV12(
    mfxU8* pBlock,
    mfxU32 uPitch,
    mfxU8* pPredBuf,
    mfxU32 bitDepth,
    mfxU32 idc)
{
    mfxU8 ap[17] = { 0 };
    mfxU8 lp[17] = { 0 };
    mfxI32 iH;
    mfxI32 iV;
    mfxI32 i, j;
    mfxI32 a,b,c;
    mfxI32 temp;
    mfxI32 xCF,yCF;
    mfxI32 max_pix_value = (1 << bitDepth) - 1;

    xCF = 4*(idc == 3);
    yCF = 4*(idc != 1);
    for (i = 0; i < (9 + (xCF << 1)); i++)
    {
        ap[i] = (pBlock - mfxI32(uPitch) - 2)[2 * i];
    }

    for (i = 0;i < (9 + (yCF << 1)); i++)
    {
        lp[i] = (pBlock + mfxI32(uPitch) * (i - 1))[-2];
    }

    iH = 0;
    iV = 0;

    for (i = 1; i <= 4; i++)
    {
        iH += i * (ap[4 + xCF + i] - ap[4 + xCF - i]);
        iV += i * (lp[4 + yCF + i] - lp[4 + yCF - i]);
    }

    for (i = 5; i <= 4 + xCF; i++)
    {
        iH += i * (ap[4 + xCF + i] - ap[4 + xCF - i]);
    }

    for (i = 5; i <= 4 + yCF; i++)
    {
        iV += i * (lp[4 + yCF + i] - lp[4 + yCF - i]);
    }

    a = (ap[8 + (xCF << 1)] + lp[8 + (yCF << 1)]) * 16;
    b = ((34 - 29 * (idc == 3)) * iH + 32) >> 6;
    c = ((34 - 29 * (idc != 1)) * iV + 32) >> 6;

    for (j = 0; j < (8 + (yCF << 1)); j++)
    {
        for (i = 0; i < (8 + (xCF << 1)); i++)
        {
            temp = (a + (i - 3 - xCF) * b + (j - 3 - yCF) * c + 16) >> 5;
            temp = (temp > max_pix_value) ? max_pix_value : temp;
            temp = (temp < 0) ? 0 : temp;
            pPredBuf[i + j * 16] = (mfxU8)temp;
        }
    }
}

static void GetIntraChromaNV12Prediction(
    mfxU32 chromaBitDepth,
    mfxU32 chromaFormatIdc,
    bool topAvailable,
    bool leftAvailable,
    mfxU8* pURef,
    mfxU32 uPitch,
    mfxU8 pMode,
    mfxU8* pUPredBuf,
    mfxU8* pVPredBuf)
{
    mfxU32 uSum[2][8] =  {{0,0,0,0},{0,0,0,0}};
    mfxU8 *pAbove, *pLeft, *pPred = 0;

    mfxU32 i,plane;

    switch (pMode)
    {
    case PRED8x8_DC:
        if (!topAvailable && !leftAvailable)
        {
            mfxI32 dc = 1 << (chromaBitDepth - 1);
            mfxU8* UPlane = pUPredBuf;
            mfxU8* VPlane = pVPredBuf;

            for (i = 0; i < 8; ++i)
            {
                memset(UPlane, dc, 8);  // Fill the block, both Planes
                memset(VPlane, dc, 8);  // Fill the block, both Planes
                UPlane += 16;
                VPlane += 16;
            }
        }
        else
        {
            for (plane = 0; plane < 2; plane++)
            {
                mfxU32* pSum = uSum[plane];
                if (topAvailable)
                {
                    pAbove = plane ? pURef - uPitch + 1 : pURef - uPitch;
                    pSum[2] = pSum[0] = pAbove[0] + pAbove[2] + pAbove[4] + pAbove[6];
                    pSum[3] = pSum[1] = pAbove[8] + pAbove[10] + pAbove[12] + pAbove[14];
                }

                if (leftAvailable)
                {
                    mfxU32 tmpSum = pSum[1];
                    pSum[2] = 0;     // Reset Block C to zero in this case.
                    // Get predictors from the left and copy into 4x4 blocks for SAD calculations
                    pLeft = plane ? pURef - 2 + 1: pURef - 2;
                    mfxU32 sum = 0;
                    for (i = 0; i < 4; i++, pLeft += uPitch)
                    {
                        sum += *pLeft;
                    }

                    pSum[0] += sum; pSum[1] += sum;
                    sum = 0;
                    for (i = 0; i < 4; i++, pLeft += uPitch)
                    {
                        sum += *pLeft;
                    }

                    pSum[2] += sum; pSum[3] += sum;
                    if (topAvailable)
                    {
                        pSum[1] = tmpSum;
                    }
                }
                // Divide & round A & D properly, depending on how many terms are in the sum.
                if (topAvailable && leftAvailable)
                {
                    // 8 Pixels
                    uSum[plane][0] = (uSum[plane][0] + 4) >> 3;
                    uSum[plane][3] = (uSum[plane][3] + 4) >> 3;
                }
                else
                {
                    // 4 pixels
                    uSum[plane][0] = (uSum[plane][0] + 2) >> 2;
                    uSum[plane][3] = (uSum[plane][3] + 2) >> 2;
                }

                // Always 4 pixels
                uSum[plane][1] = (uSum[plane][1] + 2) >> 2;
                uSum[plane][2] = (uSum[plane][2] + 2) >> 2;

                // Fill the correct pixel values into the uDCPred buffer
                pPred = plane ? pVPredBuf : pUPredBuf;
                for (i = 0; i < 4; i++)
                {
                    memset(pPred + (i * 16),                (mfxU8)pSum[0], 4);
                    memset(pPred + (i * 16) + 4,            (mfxU8)pSum[1], 4);
                    memset(pPred + (i * 16) + (4 * 16),     (mfxU8)pSum[2], 4);
                    memset(pPred + (i * 16) + (4 * 16) + 4, (mfxU8)pSum[3], 4);
                }
            }
        }
        break;
    case PRED8x8_VERT:
        if (topAvailable)
        {
            for (plane = 0; plane < 2; plane++)
            {
                pAbove = plane ? pURef - uPitch + 1: pURef - uPitch;
                pPred = plane ? pVPredBuf : pUPredBuf;
                mfxU8* pPred0 = pPred;

                for (i = 0; i < 8; i++)
                {
                    pPred[i] = pAbove[2*i];
                }

                pPred += 16;
                for (i = 0; i < 7; i++)
                {
                    MFX_INTERNAL_CPY(pPred, pPred0, 8);
                    pPred += 16;
                }
            }
        }
        break;
    case PRED8x8_HORZ:
        if (leftAvailable)
        {
            for (plane = 0; plane < 2; plane++)
            {
                pLeft = plane ? pURef - 2 + 1 : pURef - 2;
                pPred = plane ? pVPredBuf : pUPredBuf;

                for (i = 0; i < 8; i++)
                {
                    memset(pPred, (mfxU8)*pLeft, 8);
                    pPred += 16;
                    pLeft += uPitch;
                }
            }
        }
        break;
    case PRED8x8_PLANAR:
        ///    if (topAvailable && leftAvailable && left_above_aval) {
        PlanarPredictChromaNV12(
            pURef,
            uPitch,
            pUPredBuf,
            chromaBitDepth,
            chromaFormatIdc);

        PlanarPredictChromaNV12(
            pURef + 1,
            uPitch,
            pVPredBuf,
            chromaBitDepth,
            chromaFormatIdc);
        break;
    }
}

static void McMbLuma(
    mfxFrameParamAVC& fp,
    mfxExtAvcRefSliceInfo& sliceInfo,
    mfxFrameSurface& fs,
    MbDescriptor& mb,
    mfxU8* dst,
    mfxU32 dstPitch)
{
    H264PAK_ALIGN16 mfxU8 tmp[256];

    for (mfxU32 i = 0; i < mb.parts.m_numPart; i++)
    {
        const GeometryBlock4x4& geom = g_Geometry[mb.parts.parts[i].block];

        mfxU8 idx0 = mb.mbN.RefPicSelect[0][geom.offBlk8x8];
        mfxU8 idx1 = mb.mbN.RefPicSelect[1][geom.offBlk8x8];
        mfxU8* predL0 = dst;
        mfxU8* predL1 = (idx0 == 0xff) ? dst : tmp;
        mfxU32 predL0Pitch = dstPitch;
        mfxU32 predL1Pitch = (idx0 == 0xff) ? dstPitch : 16;

        predL0 += 4 * (geom.yBlk * predL0Pitch + geom.xBlk);
        predL1 += 4 * (geom.yBlk * predL1Pitch + geom.xBlk);

        IppVCInterpolateBlock_8u interpolateInfo;
        interpolateInfo.sizeFrame.width = fs.Info.Width;
        interpolateInfo.sizeFrame.height = fs.Info.Height >> mb.mbN.FieldMbFlag;
        interpolateInfo.pointBlockPos.x = 16 * mb.mbN.MbXcnt + 4 * geom.xBlk;
        interpolateInfo.pointBlockPos.y = 16 * mb.mbN.MbYcnt + 4 * geom.yBlk;
        interpolateInfo.sizeBlock.width = 4 * mb.parts.parts[i].width;
        interpolateInfo.sizeBlock.height = 4 * mb.parts.parts[i].height;

        if (idx0 != 0xff)
        {
            assert(idx0 <= fp.NumRefIdxL0Minus1);
            mfxU8 field0 = sliceInfo.RefPicList[0][idx0] >> 7;
            mfxU8 dpbIdx = sliceInfo.RefPicList[0][idx0] & 0x7f;
            assert(dpbIdx < 16);
            assert(fp.RefFrameListP[dpbIdx] < fs.NumFrameData);

            mfxU32 pitch = fs.Data[fp.RefFrameListP[dpbIdx]]->Pitch;
            interpolateInfo.srcStep = pitch << fp.FieldPicFlag;
            interpolateInfo.dstStep = predL0Pitch;
            interpolateInfo.pSrc[0] = fs.Data[fp.RefFrameListP[dpbIdx]]->Y + field0 * pitch;
            interpolateInfo.pDst[0] = predL0;
            interpolateInfo.pointVector.x = mb.mvN->mv[0][geom.offBlk4x4].x;
            interpolateInfo.pointVector.y = mb.mvN->mv[0][geom.offBlk4x4].y;
            ippiInterpolateLumaBlock_H264_8u_P1R(&interpolateInfo);
        }

        if (idx1 != 0xff)
        {
            assert(idx1 <= fp.NumRefIdxL1Minus1);
            mfxU8 field1 = sliceInfo.RefPicList[1][idx1] >> 7;
            mfxU8 dpbIdx = sliceInfo.RefPicList[1][idx1] & 0x7f;
            assert(dpbIdx < 16);
            assert(fp.RefFrameListP[dpbIdx] < fs.NumFrameData);

            mfxU32 pitch = fs.Data[fp.RefFrameListP[dpbIdx]]->Pitch;
            interpolateInfo.srcStep = pitch << fp.FieldPicFlag;
            interpolateInfo.dstStep = predL1Pitch;
            interpolateInfo.pSrc[0] = fs.Data[fp.RefFrameListP[dpbIdx]]->Y + field1 * pitch;
            interpolateInfo.pDst[0] = predL1;
            interpolateInfo.pointVector.x = mb.mvN->mv[1][geom.offBlk4x4].x;
            interpolateInfo.pointVector.y = mb.mvN->mv[1][geom.offBlk4x4].y;
            ippiInterpolateLumaBlock_H264_8u_P1R(&interpolateInfo);
        }

        if (idx0 != 0xff && idx1 != 0xff)
        {
            mfxU8* biPred = dst + 4 * (geom.yBlk * dstPitch + geom.xBlk);

            ippiInterpolateBlock_H264_8u_P3P1R(
                predL0,
                predL1,
                biPred,
                4 * mb.parts.parts[i].width,
                4 * mb.parts.parts[i].height,
                predL0Pitch,
                predL1Pitch,
                dstPitch);
        }
    }

}

static void McMbChroma(
    mfxFrameParamAVC& fp,
    mfxExtAvcRefSliceInfo& sliceInfo,
    mfxFrameSurface& fs,
    MbDescriptor& mb,
    mfxU8* dst,
    mfxU32 dstPitch)
{
    H264PAK_ALIGN16 mfxU8 tmp[128];

    for (mfxU32 i = 0; i < mb.parts.m_numPart; i++)
    {
        const GeometryBlock4x4& geom = g_Geometry[mb.parts.parts[i].block];

        mfxU8 bottomMbFlag = fp.FrameType & MFX_FRAMETYPE_BFF ? 1 : 0;
        mfxU8 idx0 = mb.mbN.RefPicSelect[0][geom.offBlk8x8];
        mfxU8 idx1 = mb.mbN.RefPicSelect[1][geom.offBlk8x8];
        mfxU8* predL0 = dst;
        mfxU8* predL1 = (idx0 == 0xff) ? dst : tmp;
        mfxU32 predL0Pitch = dstPitch;
        mfxU32 predL1Pitch = (idx0 == 0xff) ? dstPitch : 16;

        predL0 += 2 * (geom.yBlk * predL0Pitch + geom.xBlk);
        predL1 += 2 * (geom.yBlk * predL1Pitch + geom.xBlk);

        IppVCInterpolateBlock_8u interpolateInfo;
        interpolateInfo.sizeFrame.width = fs.Info.Width / 2;
        interpolateInfo.sizeFrame.height = (fs.Info.Height / 2) >> mb.mbN.FieldMbFlag;
        interpolateInfo.pointBlockPos.x = 8 * mb.mbN.MbXcnt + 2 * geom.xBlk;
        interpolateInfo.pointBlockPos.y = 8 * mb.mbN.MbYcnt + 2 * geom.yBlk;
        interpolateInfo.sizeBlock.width = 2 * mb.parts.parts[i].width;
        interpolateInfo.sizeBlock.height = 2 * mb.parts.parts[i].height;

        if (idx0 != 0xff)
        {
            assert(idx0 <= fp.NumRefIdxL0Minus1);
            mfxU8 field0 = sliceInfo.RefPicList[0][idx0] >> 7;
            mfxU8 dpbIdx = sliceInfo.RefPicList[0][idx0] & 0x7f;
            assert(dpbIdx < 16);
            assert(fp.RefFrameListP[dpbIdx] < fs.NumFrameData);

            mfxU32 pitch = fs.Data[fp.RefFrameListP[dpbIdx]]->Pitch;
            interpolateInfo.srcStep = pitch << fp.FieldPicFlag;
            interpolateInfo.dstStep = predL0Pitch;
            interpolateInfo.pSrc[0] = fs.Data[fp.RefFrameListP[dpbIdx]]->UV + field0 * pitch;
            interpolateInfo.pSrc[1] = fs.Data[fp.RefFrameListP[dpbIdx]]->UV + field0 * pitch + 1;
            interpolateInfo.pDst[0] = predL0;
            interpolateInfo.pDst[1] = predL0 + 8;
            interpolateInfo.pointVector.x = mb.mvN->mv[0][geom.offBlk4x4].x;
            interpolateInfo.pointVector.y = mb.mvN->mv[0][geom.offBlk4x4].y;

            if (!bottomMbFlag && field0)
            {
                interpolateInfo.pointVector.y -= 2;
            }
            else if (bottomMbFlag && !field0)
            {
                interpolateInfo.pointVector.y += 2;
            }

            ippiInterpolateChromaBlock_H264_8u_C2P2R(&interpolateInfo);
        }

        if (idx1 != 0xff)
        {
            assert(idx1 <= fp.NumRefIdxL1Minus1);
            mfxU8 field1 = sliceInfo.RefPicList[1][idx1] >> 7;
            mfxU8 dpbIdx = sliceInfo.RefPicList[1][idx1] & 0x7f;
            assert(dpbIdx < 16);
            assert(fp.RefFrameListP[dpbIdx] < fs.NumFrameData);

            mfxU32 pitch = fs.Data[fp.RefFrameListP[dpbIdx]]->Pitch;
            interpolateInfo.srcStep = pitch << fp.FieldPicFlag;
            interpolateInfo.dstStep = predL1Pitch;
            interpolateInfo.pSrc[0] = fs.Data[fp.RefFrameListP[dpbIdx]]->UV + field1 * pitch;
            interpolateInfo.pSrc[1] = fs.Data[fp.RefFrameListP[dpbIdx]]->UV + field1 * pitch + 1;
            interpolateInfo.pDst[0] = predL1;
            interpolateInfo.pDst[1] = predL1 + 8;
            interpolateInfo.pointVector.x = mb.mvN->mv[1][geom.offBlk4x4].x;
            interpolateInfo.pointVector.y = mb.mvN->mv[1][geom.offBlk4x4].y;

            if (!bottomMbFlag && field1)
            {
                interpolateInfo.pointVector.y -= 2;
            }
            else if (bottomMbFlag && !field1)
            {
                interpolateInfo.pointVector.y += 2;
            }

            ippiInterpolateChromaBlock_H264_8u_C2P2R(&interpolateInfo);
        }

        if (idx0 != 0xff && idx1 != 0xff)
        {
            mfxU8* biPred = dst + 2 * (geom.yBlk * dstPitch + geom.xBlk);

            ippiInterpolateBlock_H264_8u_P2P1R(
                predL0,
                predL1,
                biPred,
                2 * mb.parts.parts[i].width,
                2 * mb.parts.parts[i].height,
                16);

            ippiInterpolateBlock_H264_8u_P2P1R(
                predL0 + 8,
                predL1 + 8,
                biPred + 8,
                2 * mb.parts.parts[i].width,
                2 * mb.parts.parts[i].height,
                16);
        }
    }
}

void ScanCopy(const mfxI16* src, const mfxI32* scan, mfxU32 numCoeff, mfxI16* dst)
{
    for (mfxU32 i = 0; i < numCoeff; i++)
    {
        dst[i] = src[scan[i]];
    }
}

static void PredictMbIntra4x4(
    mfxFrameParamAVC& fp,
    mfxFrameSurface& fs,
    MbDescriptor& mb,
    CoeffData& coeffData)
{
    H264PAK_ALIGN16 mfxI16 difference[16];
    H264PAK_ALIGN16 mfxI16 transform[16];
    mfxU8 predPels[13];

    mfxFrameData& curFrame = *fs.Data[fp.CurrFrameLabel];
    mfxFrameData& recFrame = *fs.Data[fp.RecFrameLabel];
    mfxU32 curPitch = curFrame.Pitch << fp.FieldPicFlag;
    mfxU32 recPitch = recFrame.Pitch << fp.FieldPicFlag;
    mfxU16 uCBPLuma = mb.mbN.CodedPattern4x4Y;

    mfxU32 bottomMbFlag = fp.FrameType & MFX_FRAMETYPE_BFF ? 1 : 0;
    mfxU8* curMb = curFrame.Y + 16 * (mb.mbN.MbYcnt * curPitch + mb.mbN.MbXcnt) +
        bottomMbFlag * curFrame.Pitch;
    mfxU8* recMb = recFrame.Y + 16 * (mb.mbN.MbYcnt * recPitch + mb.mbN.MbXcnt) +
        bottomMbFlag * recFrame.Pitch;

    for (mfxU32 block4x4 = 0; block4x4 < 16; block4x4++)
    {
        const GeometryBlock4x4& geom = g_Geometry[block4x4];
        mfxU8* curBlk = curMb + 4 * (geom.yBlk * curPitch + geom.xBlk);
        mfxU8* recBlk = recMb + 4 * (geom.yBlk * recPitch + geom.xBlk);

        if (fp.EntropyCodingModeFlag)
        {
            coeffData.cabac.luma4x4Aux[block4x4].numSigCoeff = 0;
        }
        else
        {
            SetNumCavlcCoeffLuma(mb.mbN, block4x4, 0);
            coeffData.cavlc.lumaAux[block4x4].lastSigCoeff = 0xff;
        }

        GetBlock4x4PredPels(
            IsLeftAvail(mb),
            IsAboveAvail(mb),
            IsAboveRightAvail(mb),
            recBlk,
            recPitch,
            block4x4,
            predPels);

        GetIntra4x4Prediction(
            recBlk,
            recPitch,
            Get4x4IntraMode(mb.mbN, block4x4),
            predPels);

        mfxI32 iNumCoeffs = 0;
        mfxI32 iLastCoeff = 0;

        if (uCBPLuma & (1 << block4x4))
        {
            ippiSub4x4_8u16s_C1R(
                curBlk,
                curPitch,
                recBlk,
                recPitch,
                difference,
                8);

            ippiTransformQuantFwd4x4_H264_16s_C1(
                difference,
                transform,
                mb.mbN.QpPrimeY,
                &iNumCoeffs,
                1,
                g_ScanMatrixEnc[fp.FieldPicFlag],
                &iLastCoeff,
                0);
        }

        if (iNumCoeffs)
        {
            // Preserve the absolute number of coeffs.
            if (fp.EntropyCodingModeFlag)
            {
                coeffData.cabac.luma4x4Aux[block4x4].numSigCoeff = (mfxU8)abs(iNumCoeffs);
                ScanCopy(
                    transform,
                    g_ScanMatrixDec4x4[fp.FieldPicFlag],
                    16,
                    coeffData.cabac.luma4x4[block4x4].coeffs);
            }
            else
            {
                coeffData.cavlc.lumaAux[block4x4].lastSigCoeff = (mfxU8)iLastCoeff;
                MFX_INTERNAL_CPY(coeffData.cavlc.luma[block4x4].coeffs, transform, 16 * sizeof(mfxI16));
            }

            ippiTransformQuantInvAddPred4x4_H264_16s_C1IR(
                recBlk,
                recPitch,
                transform,
                NULL,
                recBlk,
                recPitch,
                mb.mbN.QpPrimeY,
                iNumCoeffs < -1 || iNumCoeffs > 0,
                0);
        }
        else
        {
            uCBPLuma &= ~(1 << block4x4);
        }
    }

    mb.mbN.CodedPattern4x4Y = uCBPLuma;
    mb.mbN.DcBlockCodedYFlag = 0;
}

static void PredictMbIntra8x8(
    mfxFrameParamAVC& fp,
    mfxFrameSurface& fs,
    MbDescriptor& mb,
    mfxI16 scaleMatrix8x8[2][6][64],
    mfxI16 scaleMatrix8x8Inv[2][6][64],
    CoeffData& coeffData)
{
    H264PAK_ALIGN16 mfxI16 difference[64];
    H264PAK_ALIGN16 mfxI16 transform[64];
    mfxU8 predPels[25];

    mfxFrameData& curFrame = *fs.Data[fp.CurrFrameLabel];
    mfxFrameData& recFrame = *fs.Data[fp.RecFrameLabel];
    mfxU32 curPitch = curFrame.Pitch << fp.FieldPicFlag;
    mfxU32 recPitch = recFrame.Pitch << fp.FieldPicFlag;
    mfxU16 uCBPLuma = mb.mbN.CodedPattern4x4Y;

    mfxU32 bottomMbFlag = fp.FrameType & MFX_FRAMETYPE_BFF ? 1 : 0;
    mfxU8* curMb = curFrame.Y + 16 * (mb.mbN.MbYcnt * curPitch + mb.mbN.MbXcnt) +
        bottomMbFlag * curFrame.Pitch;
    mfxU8* recMb = recFrame.Y + 16 * (mb.mbN.MbYcnt * recPitch + mb.mbN.MbXcnt) +
        bottomMbFlag * recFrame.Pitch;

    for (mfxU32 block8x8 = 0; block8x8 < 4; block8x8++)
    {
        const GeometryBlock4x4& geom = g_Geometry[4 * block8x8];
        mfxU8* curBlk = curMb + 4 * (geom.yBlk * curPitch + geom.xBlk);
        mfxU8* recBlk = recMb + 4 * (geom.yBlk * recPitch + geom.xBlk);

        mfxU32 predPelsMask = 0;
        bool isAboveAvail = (block8x8 & 0x2) || IsAboveAvail(mb);
        bool isLeftAvail = (block8x8 & 0x1) || IsLeftAvail(mb);

        bool isAboveLeftAvail =
            (block8x8 == 0 && IsAboveLeftAvail(mb)) ||
            (block8x8 == 1 && isAboveAvail) ||
            (block8x8 == 2 && isLeftAvail) ||
            (block8x8 == 3);

        bool isAboveRightAvail =
            (block8x8 == 0 && isAboveAvail) ||
            (block8x8 == 1 && IsAboveRightAvail(mb)) ||
            (block8x8 == 2);

        //Copy pels
        //TOP
        if (isAboveAvail)
        {
            for (mfxU32 i = 0; i < 8; i++)
            {
                predPels[1 + i] = *(recBlk - recPitch + i);
            }

            predPelsMask |= 0x000001fe;
        }

        //LEFT
        if (isLeftAvail)
        {
            for (mfxU32 i = 0; i < 8; i++)
            {
                predPels[17 + i] = *(recBlk + i * recPitch - 1);
            }

            predPelsMask |= 0x1fe0000;
        }

        //LEFT_ABOVE
        if (isAboveLeftAvail)
        {
            predPels[0] = *(recBlk - recPitch - 1);
            predPelsMask |= 0x01;
        }

        //RIGHT_ABOVE
        if (isAboveRightAvail)
        {
            for (mfxU32 i = 0; i < 8; i++)
            {
                predPels[9 + i] = *(recBlk - recPitch + i + 8);
            }

            predPelsMask |= 0x0001fe00;
        }

        if (!((predPelsMask & 0x0001FE00) == 0x0001FE00) && (predPelsMask & 0x00000100))
        {
            predPelsMask |= 0x0001FE00;
            for (mfxU32 i = 0; i < 8; i++)
            {
                predPels[9 + i] = predPels[1 + 7];
            }
        }

        Filter8x8Pels(predPels, predPelsMask);

        GetIntra8x8Prediction(
            recBlk,
            recPitch,
            Get8x8IntraMode(mb.mbN, block8x8),
            predPels,
            predPelsMask);

        if (fp.EntropyCodingModeFlag)
        {
            coeffData.cabac.luma8x8Aux[block8x8].numSigCoeff = 0;
        }
        else
        {
            for (mfxU32 idx = 4 * block8x8; idx < 4 * block8x8 + 4; idx++)
            {
                SetNumCavlcCoeffLuma(mb.mbN, idx, 0);
                coeffData.cavlc.lumaAux[idx].lastSigCoeff = 0xff;
           }
        }

        mfxI32 iNumCoeffs = 0;
        mfxI32 iLastCoeff = 0;

        if (uCBPLuma & (0xf << 4 * block8x8))
        {
            ippiSub8x8_8u16s_C1R(
                curBlk,
                curPitch,
                recBlk,
                recPitch,
                difference,
                16);

            ippiTransformFwdLuma8x8_H264_16s_C1(
                difference,
                transform);

            ippiQuantLuma8x8_H264_16s_C1(
                transform,
                transform,
                g_QpDiv6[mb.mbN.QpPrimeY],
                1,
                g_ScanMatrixEnc8x8[fp.FieldPicFlag],
                scaleMatrix8x8[0][g_QpMod6[mb.mbN.QpPrimeY]],
                &iNumCoeffs,
                &iLastCoeff);
        }

        if (iNumCoeffs)
        {
            // record RLE info
            if (fp.EntropyCodingModeFlag)
            {
                coeffData.cabac.luma8x8Aux[block8x8].numSigCoeff = (mfxU8)abs(iNumCoeffs);
                ScanCopy(
                    transform,
                    g_ScanMatrixDec8x8[fp.FieldPicFlag],
                    64,
                    coeffData.cabac.luma8x8[block8x8].coeffs);
            }
            else
            {
                mfxI16 buf4x4[4][16];

                // Reorder 8x8 block for coding with CAVLC
                for (mfxU32 i4x4 = 0; i4x4 < 4; i4x4++)
                {
                    for(mfxU32 i = 0; i < 16; i++)
                    {
                        buf4x4[i4x4][g_ScanMatrixDec4x4[fp.FieldPicFlag][i]] =
                            transform[g_ScanMatrixDec8x8[fp.FieldPicFlag][4 * i + i4x4]];
                    }
                }

                //Encode each block with CAVLC 4x4
                for (mfxU32 i4x4 = 0; i4x4 < 4; i4x4++)
                {
                    iLastCoeff = 0;
                    mfxU32 idx = 4 * block8x8 + i4x4;

                    //Check for last coeff
                    for (mfxU32 i = 0; i < 16; i++)
                    {
                        if (buf4x4[i4x4][g_ScanMatrixDec4x4[fp.FieldPicFlag][i]] != 0)
                        {
                            iLastCoeff = i;
                        }
                    }

                    coeffData.cavlc.lumaAux[idx].lastSigCoeff = (mfxU8)iLastCoeff;
                    MFX_INTERNAL_CPY(coeffData.cavlc.luma[idx].coeffs, buf4x4[i4x4], 16 * sizeof(mfxI16));
                }
            }

            ippiQuantLuma8x8Inv_H264_16s_C1I(
                transform,
                g_QpDiv6[mb.mbN.QpPrimeY],
                scaleMatrix8x8Inv[0][g_QpMod6[mb.mbN.QpPrimeY]]);

            ippiTransformLuma8x8InvAddPred_H264_16s8u_C1R(
                recBlk,
                recPitch,
                transform,
                recBlk,
                recPitch);
        }
        else
        {
            uCBPLuma &= ~(0xf << 4 * block8x8);
        }
    }

    mb.mbN.CodedPattern4x4Y = uCBPLuma;
    mb.mbN.DcBlockCodedYFlag = 0;
}

static void PredictMbIntra16x16(
    mfxFrameParamAVC& fp,
    mfxFrameSurface& fs,
    MbDescriptor& mb,
    CoeffData& coeffData)
{
    H264PAK_ALIGN16 mfxI16 massDiff[256];
    H264PAK_ALIGN16 mfxI16 dcDiff[16];
    H264PAK_ALIGN16 mfxI16 transform[16];

    mfxFrameData& curFrame = *fs.Data[fp.CurrFrameLabel];
    mfxFrameData& recFrame = *fs.Data[fp.RecFrameLabel];
    mfxU32 curPitch = curFrame.Pitch << fp.FieldPicFlag;
    mfxU32 recPitch = recFrame.Pitch << fp.FieldPicFlag;
    mfxU16 uCBPLuma = mb.mbN.CodedPattern4x4Y;

    mfxU32 bottomMbFlag = fp.FrameType & MFX_FRAMETYPE_BFF ? 1 : 0;
    mfxU8* curMb = curFrame.Y + 16 * (mb.mbN.MbYcnt * curPitch + mb.mbN.MbXcnt) +
        bottomMbFlag * curFrame.Pitch;
    mfxU8* recMb = recFrame.Y + 16 * (mb.mbN.MbYcnt * recPitch + mb.mbN.MbXcnt) +
        bottomMbFlag * recFrame.Pitch;

    GetIntra16x16Prediction(
        fp.BitDepthLumaMinus8 + 8,
        IsAboveAvail(mb),
        IsLeftAvail(mb),
        recMb,
        recPitch,
        Get16x16IntraMode(mb.mbN),
        recMb,
        recPitch);

    ippiSumsDiff16x16Blocks4x4_8u16s_C1(
        curMb,
        curPitch,
        recMb,
        recPitch,
        dcDiff,
        massDiff);

    // apply second transform on the luma DC transform coeffs
    mfxI32 iNumCoeffs = 0; // number of nonzero coeffs after quant (negative if DC is nonzero)
    mfxI32 iLastCoeff = 0; // number of nonzero coeffs after quant (negative if DC is nonzero)
    ippiTransformQuantFwdLumaDC4x4_H264_16s_C1I(
        dcDiff,
        transform,
        mb.mbN.QpPrimeY,
        &iNumCoeffs,
        1,
        g_ScanMatrixEnc[fp.FieldPicFlag],
        &iLastCoeff,
        0);

    mb.mbN.DcBlockCodedYFlag = iNumCoeffs != 0;

    if (fp.EntropyCodingModeFlag)
    {
        coeffData.cabac.dcAux[BLOCK_DC_Y].numSigCoeff = (mfxU8)(abs(iNumCoeffs));
        ScanCopy(
            dcDiff,
            g_ScanMatrixDec4x4[fp.FieldPicFlag],
            16,
            coeffData.cabac.dc[BLOCK_DC_Y].coeffs);
    }
    else
    {
        coeffData.cavlc.dcAux[BLOCK_DC_Y].lastSigCoeff = (mfxU8)iLastCoeff;
        MFX_INTERNAL_CPY(coeffData.cavlc.dc[BLOCK_DC_Y].coeffs, dcDiff, 16 * sizeof(mfxI16));
    }

    ippiTransformQuantInvLumaDC4x4_H264_16s_C1I(
        dcDiff,
        mb.mbN.QpPrimeY,
        0);

    for (mfxU32 block4x4 = 0; block4x4 < 16; block4x4++)
    {
        const GeometryBlock4x4& geom = g_Geometry[block4x4];
        mfxU8* recBlk = recMb + 4 * (geom.yBlk * recPitch + geom.xBlk);

        if (fp.EntropyCodingModeFlag)
        {
            coeffData.cabac.luma4x4Aux[block4x4].numSigCoeff = 0;
        }
        else
        {
            SetNumCavlcCoeffLuma(mb.mbN, block4x4, 0);
            coeffData.cavlc.lumaAux[block4x4].lastSigCoeff = 0xff;
        }

        iNumCoeffs = 0;
        iLastCoeff = 0;
        if (uCBPLuma & (1 << block4x4))
        {
            // block not declared empty, encode
            ippiTransformQuantFwd4x4_H264_16s_C1(
                massDiff + 16 * geom.offBlk4x4,
                transform,
                mb.mbN.QpPrimeY,
                &iNumCoeffs,
                1, //Always use f for INTRA
                g_ScanMatrixEnc[fp.FieldPicFlag],
                &iLastCoeff,
                0);
        }

        // if everything quantized to zero, skip RLE
        if (iNumCoeffs < -1 || iNumCoeffs > 0)
        {
            // Preserve the absolute number of coeffs.
            if (fp.EntropyCodingModeFlag)
            {
                coeffData.cabac.luma4x4Aux[block4x4].numSigCoeff = (mfxU8)((iNumCoeffs < 0) ? -(iNumCoeffs+1) : iNumCoeffs);
                ScanCopy(
                    transform,
                    g_ScanMatrixDec4x4[fp.FieldPicFlag] + 1,
                    15,
                    coeffData.cabac.luma4x4[block4x4].coeffs);
            }
            else
            {
                coeffData.cavlc.lumaAux[block4x4].lastSigCoeff = (mfxU8)iLastCoeff;
                MFX_INTERNAL_CPY(coeffData.cavlc.luma[block4x4].coeffs, transform, 16 * sizeof(mfxI16));
            }
        }

        if (!(iNumCoeffs < -1 || iNumCoeffs > 0))
        {
            uCBPLuma &= ~(1 << block4x4);
        }

        // If the block wasn't coded and the DC coefficient is zero
        if ((iNumCoeffs < -1 || iNumCoeffs > 0) || dcDiff[g_Geometry[block4x4].offBlk4x4])
        {
            ippiTransformQuantInvAddPred4x4_H264_16s_C1IR(
                recBlk,
                recPitch,
                transform,
                &dcDiff[g_Geometry[block4x4].offBlk4x4],
                recBlk,
                recPitch,
                mb.mbN.QpPrimeY,
                ((iNumCoeffs < -1) || (iNumCoeffs > 0)),
                0);
        }
    }

    mb.mbN.CodedPattern4x4Y = uCBPLuma;
}

static mfxU32 PredictMbInter(
    mfxFrameParamAVC& fp,
    mfxExtAvcRefSliceInfo& sliceInfo,
    mfxFrameSurface& fs,
    MbDescriptor& mb,
    mfxI16 scaleMatrix8x8[2][6][64],
    mfxI16 scaleMatrix8x8Inv[2][6][64],
    CoeffData& coeffData)
{
    H264PAK_ALIGN16 mfxI16 transform[256];
    H264PAK_ALIGN16 mfxI16 difference[64];

    mfxU8* coeffInfo = (mfxU8 *)mb.mbN.reserved1a;

    mfxFrameData& curFrame = *fs.Data[fp.CurrFrameLabel];
    mfxFrameData& recFrame = *fs.Data[fp.RecFrameLabel];
    mfxU32 curPitch = curFrame.Pitch << fp.FieldPicFlag;
    mfxU32 recPitch = recFrame.Pitch << fp.FieldPicFlag;
    mfxU16 uCBPLuma = mb.mbN.CodedPattern4x4Y;

    mfxU32 bottomMbFlag = fp.FrameType & MFX_FRAMETYPE_BFF ? 1 : 0;
    mfxU8* curMb = curFrame.Y + 16 * (mb.mbN.MbYcnt * curPitch + mb.mbN.MbXcnt) +
        bottomMbFlag * curFrame.Pitch;
    mfxU8* recMb = recFrame.Y + 16 * (mb.mbN.MbYcnt * recPitch + mb.mbN.MbXcnt) +
        bottomMbFlag * recFrame.Pitch;

    McMbLuma(fp, sliceInfo, fs, mb, recMb, recPitch);

    if (fp.EntropyCodingModeFlag)
    {
        for (mfxU32 block4x4 = 0; block4x4 < 16; block4x4++)
        {
            coeffData.cabac.luma4x4Aux[block4x4].numSigCoeff = 0;
        }
    }
    else
    {
        for (mfxU32 block4x4 = 0; block4x4 < 16; block4x4++)
        {
            SetNumCavlcCoeffLuma(mb.mbN, block4x4, 0);
            coeffData.cavlc.lumaAux[block4x4].lastSigCoeff = 0xff;
        }
    }

    if (mb.mbN.TransformFlag)
    {
        mfxI32 mbCost = 0;

        for (mfxU32 block8x8 = 0; block8x8 < 4; block8x8++)
        {
            const GeometryBlock4x4& geom = g_Geometry[4 * block8x8];
            mfxU8* curBlk = curMb + 4 * (geom.yBlk * curPitch + geom.xBlk);
            mfxU8* recBlk = recMb + 4 * (geom.yBlk * recPitch + geom.xBlk);
            mfxI16* transform8x8 = transform + 64 * block8x8;

            mfxI32 iNumCoeffs = 0;
            mfxI32 iLastCoeff = 0;
            mfxI32 coeffCost = 0;

            if (uCBPLuma & (0xf << 4 * block8x8))
            {
                ippiSub8x8_8u16s_C1R(
                    curBlk,
                    curPitch,
                    recBlk,
                    recPitch,
                    difference,
                    16);

                ippiTransformFwdLuma8x8_H264_16s_C1(
                    difference,
                    transform8x8);

                ippiQuantLuma8x8_H264_16s_C1(
                    transform8x8,
                    transform8x8,
                    g_QpDiv6[mb.mbN.QpPrimeY],
                    0,
                    g_ScanMatrixEnc8x8[fp.FieldPicFlag],
                    scaleMatrix8x8[1][g_QpMod6[mb.mbN.QpPrimeY]], // INTER scaling matrix
                    &iNumCoeffs,
                    &iLastCoeff);

                coeffCost = iNumCoeffs == 0
                    ? 0
                    : CalculateCoeffsCost(transform8x8, 64, g_ScanMatrixDec8x8[fp.FieldPicFlag]);
                mbCost += coeffCost;
            }

            // if everything quantized to zero, skip RLE
            if (coeffCost >= LUMA_COEFF_8X8_MAX_COST)
            {
                // record RLE info
                if (fp.EntropyCodingModeFlag)
                {
                    coeffData.cabac.luma8x8Aux[block8x8].numSigCoeff = (mfxU8)abs(iNumCoeffs);
                    ScanCopy(
                        transform8x8,
                        g_ScanMatrixDec8x8[fp.FieldPicFlag],
                        64,
                        coeffData.cabac.luma8x8[block8x8].coeffs);
                }
                else
                {
                    mfxI16 buf4x4[4][16];
                    mfxU8 iLastCoeff;

                    //Reorder 8x8 block for coding with CAVLC
                    for (mfxU32 i4x4=0; i4x4 < 4; i4x4++)
                    {
                        for (mfxU32 i = 0; i < 16; i++)
                        {
                            buf4x4[i4x4][g_ScanMatrixDec4x4[fp.FieldPicFlag][i]] =
                                transform8x8[g_ScanMatrixDec8x8[fp.FieldPicFlag][4 * i + i4x4]];
                        }
                    }

                    //Encode each block with CAVLC 4x4
                    mfxU32 idx = block8x8 * 4;
                    for (mfxU32 i4x4 = 0; i4x4<4; i4x4++, idx++)
                    {
                        //Check for last coeff
                        iLastCoeff = 0;
                        for (mfxU8 i = 0; i < 16; i++)
                        {
                            if (buf4x4[i4x4][g_ScanMatrixDec4x4[fp.FieldPicFlag][i]] != 0)
                            {
                                iLastCoeff = i;
                            }
                        }

                        coeffData.cavlc.lumaAux[idx].lastSigCoeff = (mfxU8)iLastCoeff;
                        MFX_INTERNAL_CPY(coeffData.cavlc.luma[idx].coeffs, buf4x4[i4x4], 16 * sizeof(mfxI16));
                    }
                }
            }
            else
            {
                uCBPLuma &= ~(0xf << 4 * block8x8);
            }
        }

        if (mbCost < LUMA_COEFF_MB_8X8_MAX_COST)
        {
            uCBPLuma = 0;
            memset(coeffInfo, 0, 16);
        }

        for (mfxU32 block8x8 = 0; block8x8 < 4; block8x8++)
        {
            const GeometryBlock4x4& geom = g_Geometry[4 * block8x8];
            mfxU8* recBlk = recMb + 4 * (geom.yBlk * recPitch + geom.xBlk);

            if (uCBPLuma & (0xf << 4 * block8x8))
            {
                ippiQuantLuma8x8Inv_H264_16s_C1I(
                    transform + block8x8 * 64,
                    g_QpDiv6[mb.mbN.QpPrimeY],
                    scaleMatrix8x8Inv[1][g_QpMod6[mb.mbN.QpPrimeY]]); //scaling matrix for INTER slice

                ippiTransformLuma8x8InvAddPred_H264_16s8u_C1R(
                    recBlk,
                    recPitch,
                    transform + block8x8 * 64,
                    recBlk,
                    recPitch);
            }
        }
    }
    else
    {
        mfxI32 iNumCoeffs[16] = { 0 };
        mfxI32 CoeffsCost[16] = {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};
        for (mfxU32 block4x4 = 0; block4x4 < 16; block4x4++)
        {
            const GeometryBlock4x4& geom = g_Geometry[block4x4];
            mfxU8* curBlk = curMb + 4 * (geom.yBlk * curPitch + geom.xBlk);
            mfxU8* recBlk = recMb + 4 * (geom.yBlk * recPitch + geom.xBlk);
            mfxI16* transform4x4 = transform + 16 * block4x4;

            mfxI32 iLastCoeff = 0;
            if (uCBPLuma & (1 << block4x4))
            {
                ippiSub4x4_8u16s_C1R(
                    curBlk,
                    curPitch,
                    recBlk,
                    recPitch,
                    difference,
                    8);

                // forward transform and quantization, in place in difference
                ippiTransformQuantFwd4x4_H264_16s_C1(
                    difference,
                    transform4x4,
                    mb.mbN.QpPrimeY,
                    &iNumCoeffs[block4x4],
                    0,
                    g_ScanMatrixEnc[fp.FieldPicFlag],
                    &iLastCoeff,
                    0);

                CoeffsCost[block4x4] = iNumCoeffs[block4x4] == 0
                    ? 0
                    : CalculateCoeffsCost(transform4x4, 16, g_ScanMatrixDec4x4[fp.FieldPicFlag]);
            }

            if (iNumCoeffs[block4x4])
            {
                // Preserve the absolute number of coeffs.
                if (fp.EntropyCodingModeFlag)
                {
                    coeffData.cabac.luma4x4Aux[block4x4].numSigCoeff = (mfxU8)abs(iNumCoeffs[block4x4]);
                    ScanCopy(
                        transform4x4,
                        g_ScanMatrixDec4x4[fp.FieldPicFlag],
                        16,
                        coeffData.cabac.luma4x4[block4x4].coeffs);
                }
                else
                {
                    coeffData.cavlc.lumaAux[block4x4].lastSigCoeff = (mfxU8)iLastCoeff;
                    MFX_INTERNAL_CPY(coeffData.cavlc.luma[block4x4].coeffs, transform4x4, 16 * sizeof(mfxI16));
                }
            }
            else
            {
                uCBPLuma &= ~(1 << block4x4);
            }
        }

        //Skip subblock 8x8 if it cost is < 4 or skip MB if it's cost is < 5
        mfxI32 mbCost = 0;
        for (mfxU32 block4x4 = 0; block4x4 < 4; block4x4++)
        {
            mfxI32 block8x8cost =
                CoeffsCost[block4x4 * 4 + 0] +
                CoeffsCost[block4x4 * 4 + 1] +
                CoeffsCost[block4x4 * 4 + 2] +
                CoeffsCost[block4x4 * 4 + 3];
            mbCost += block8x8cost;

            if (block8x8cost <= LUMA_8X8_MAX_COST)
            {
                uCBPLuma &= ~(0xf << 4 * block4x4);
                memset(coeffInfo + 4 * block4x4, 0, 4);
            }
        }

        if (mbCost <= LUMA_MB_MAX_COST)
        {
            uCBPLuma = 0;
            memset(coeffInfo, 0, 16);
        }

        //Make inverse quantization and transform for non zero blocks
        for (mfxU32 block4x4 = 0; block4x4 < 16; block4x4++)
        {
            const GeometryBlock4x4& geom = g_Geometry[block4x4];
            mfxU8* recBlk = recMb + 4 * (geom.yBlk * recPitch + geom.xBlk);

            if (uCBPLuma & (1 << block4x4))
            {
                ippiTransformQuantInvAddPred4x4_H264_16s_C1IR(
                    recBlk,
                    recPitch,
                    transform + block4x4 * 16,
                    0,
                    recBlk,
                    recPitch,
                    mb.mbN.QpPrimeY,
                    ((iNumCoeffs[block4x4] < -1) || (iNumCoeffs[block4x4] > 0)),
                    0);
            }
        }
    }

    mb.mbN.CodedPattern4x4Y = uCBPLuma;
    mb.mbN.DcBlockCodedYFlag = 0;
    return 1;
}

static void PredictMbChroma(
    mfxFrameParamAVC& fp,
    mfxExtAvcRefSliceInfo& sliceInfo,
    mfxFrameSurface& fs,
    MbDescriptor& mb,
    CoeffData& coeffData)
{
    H264PAK_ALIGN16 mfxU8 predAndRecon[128];
    H264PAK_ALIGN16 mfxI16 transform[64];
    H264PAK_ALIGN16 mfxI16 massDiff[128];
    H264PAK_ALIGN16 mfxI16 dcDiff[8];

    mfxFrameData& curFrame = *fs.Data[fp.CurrFrameLabel];
    mfxFrameData& recFrame = *fs.Data[fp.RecFrameLabel];
    mfxU32 curPitch = curFrame.Pitch << fp.FieldPicFlag;
    mfxU32 recPitch = recFrame.Pitch << fp.FieldPicFlag;

    mfxU32 bottomMbFlag = fp.FrameType & MFX_FRAMETYPE_BFF ? 1 : 0;
    mfxU8* curMb = curFrame.UV + 8 * mb.mbN.MbYcnt * curPitch + 16 * mb.mbN.MbXcnt +
        bottomMbFlag * curFrame.Pitch;
    mfxU8* recMb = recFrame.UV + 8 * mb.mbN.MbYcnt * recPitch + 16 * mb.mbN.MbXcnt +
        bottomMbFlag * recFrame.Pitch;

    if (mb.mbN.IntraMbFlag)
    {
        GetIntraChromaNV12Prediction(
            fp.BitDepthChromaMinus8 + 8,
            fp.ChromaFormatIdc,
            IsAboveAvail(mb),
            IsLeftAvail(mb),
            recMb,
            recPitch,
            mb.mbN.ChromaIntraPredMode,
            predAndRecon,
            predAndRecon + 8);
    }
    else
    {
        McMbChroma(fp, sliceInfo, fs, mb, predAndRecon, 16);
    }

    mfxU16 uCBPChroma = mb.mbN.CodedPattern4x4U | (mb.mbN.CodedPattern4x4V << 4);

    ippiSumsDiff8x8Blocks4x4_8u16s_C2P2(
        curMb,
        curPitch,
        predAndRecon,
        16,
        predAndRecon + 8,
        16,
        dcDiff,
        massDiff,
        dcDiff + 4,
        massDiff + 64);

    for (mfxU32 plane = 0; plane < 2; plane++)
    {
        mfxI32 iNumCoeffs[4] = { 0, 0, 0, 0 };
        mfxI32 iLastCoeff = 0;
        ippiTransformQuantFwdChromaDC2x2_H264_16s_C1I(
            dcDiff + 4 * plane,
            transform,
            g_MapLumaQpToChromaQp[mb.mbN.QpPrimeY],
            iNumCoeffs,
            fp.IntraPicFlag,
            1,
            0);

        iLastCoeff = 3;
        for (mfxI32 i = 3; i >= 0; i--)
        {
            if (dcDiff[i + 4 * plane] != 0)
            {
                iLastCoeff = i;
                break;
            }
        }

        if (plane == 0)
        {
            mb.mbN.DcBlockCodedCbFlag = (iNumCoeffs[0] != 0);
        }
        else
        {
            mb.mbN.DcBlockCodedCrFlag = (iNumCoeffs[0] != 0);
        }

        if (fp.EntropyCodingModeFlag)
        {
            coeffData.cabac.dcAux[BLOCK_DC_U + plane].numSigCoeff = (mfxU8)abs(iNumCoeffs[0]);
            ScanCopy(
                dcDiff + 4 * plane,
                g_ScanMatrixDec2x2,
                4,
                coeffData.cabac.dc[BLOCK_DC_U + plane].coeffs);
        }
        else
        {
            coeffData.cavlc.dcAux[BLOCK_DC_U + plane].lastSigCoeff = (mfxU8)iLastCoeff;
            MFX_INTERNAL_CPY(coeffData.cavlc.dc[BLOCK_DC_U + plane].coeffs, dcDiff + 4 * plane, 4 * sizeof(mfxI16));
        }

        // Inverse transform and dequantize for chroma DC
        ippiTransformQuantInvChromaDC2x2_H264_16s_C1I(
            dcDiff + 4 * plane,
            g_MapLumaQpToChromaQp[mb.mbN.QpPrimeY],
            0);

        mfxI32 coeffsCost = 0;
        for (mfxU32 chromaBlock = 0; chromaBlock < 4; chromaBlock++)
        {
            if (fp.EntropyCodingModeFlag)
            {
                coeffData.cabac.chromaAux[4 * plane + chromaBlock].numSigCoeff = 0;
            }
            else
            {
                SetNumCavlcCoeffChroma(mb.mbN, 4 * plane + chromaBlock, 0);
                coeffData.cavlc.chromaAux[4 * plane + chromaBlock].lastSigCoeff = 0xff;
            }

            if (uCBPChroma & (1 << (chromaBlock + 4 * plane)))
            {
                // block not declared empty, encode
                mfxI16* pTempDiffBuf = massDiff + chromaBlock * 16 + 64 * plane;
                mfxI16* pTransformResult = transform + chromaBlock * 16;

                ippiTransformQuantFwd4x4_H264_16s_C1(
                    pTempDiffBuf,
                    pTransformResult,
                    g_MapLumaQpToChromaQp[mb.mbN.QpPrimeY],
                    &iNumCoeffs[chromaBlock],
                    0,
                    g_ScanMatrixEnc[fp.FieldPicFlag],
                    &iLastCoeff,
                    0);

                coeffsCost += iNumCoeffs[chromaBlock] != 0
                    ? CalculateCoeffsCost(pTransformResult, 15, &g_ScanMatrixDec4x4[fp.FieldPicFlag][1])
                    : 0;

                if (iNumCoeffs[chromaBlock] < 0)
                {
                    iNumCoeffs[chromaBlock] = -iNumCoeffs[chromaBlock] - 1;
                }

                if (iNumCoeffs[chromaBlock])
                {
                    // the block is empty so it is not coded
                    if (fp.EntropyCodingModeFlag)
                    {
                        coeffData.cabac.chromaAux[4 * plane + chromaBlock].numSigCoeff = (mfxU8)iNumCoeffs[chromaBlock];
                        ScanCopy(
                            pTransformResult,
                            g_ScanMatrixDec4x4[fp.FieldPicFlag] + 1,
                            15,
                            coeffData.cabac.chroma[4 * plane + chromaBlock].coeffs);
                    }
                    else
                    {
                        coeffData.cavlc.chromaAux[4 * plane + chromaBlock].lastSigCoeff = (mfxU8)iLastCoeff;
                        MFX_INTERNAL_CPY(coeffData.cavlc.chroma[4 * plane + chromaBlock].coeffs, pTransformResult, 16 * sizeof(mfxI16));
                    }
                }
            }
        }

        if (coeffsCost <= (CHROMA_COEFF_MAX_COST << (fp.ChromaFormatIdc - 1)))
        {
            for (mfxU32 chromaBlock = 0; chromaBlock < 4; chromaBlock++)
            {
                iNumCoeffs[chromaBlock] = 0;
                if (fp.EntropyCodingModeFlag)
                {
                    coeffData.cabac.chromaAux[4 * plane + chromaBlock].numSigCoeff = 0;
                }
                else
                {
                    SetNumCavlcCoeffChroma(mb.mbN, 4 * plane + chromaBlock, 0);
                    coeffData.cavlc.chromaAux[4 * plane + chromaBlock].lastSigCoeff = 0xff;
                }
            }
        }

        for (mfxU32 chromaBlock = 0; chromaBlock < 4; chromaBlock++)
        {
            const GeometryBlock4x4& geom = g_Geometry[chromaBlock];

            mfxU32 numAcCoeff = iNumCoeffs[chromaBlock];

            if (numAcCoeff == 0)
            {
                uCBPChroma &= ~(1 << (chromaBlock + 4 * plane));
            }

            if (numAcCoeff != 0 || dcDiff[chromaBlock + 4 * plane] != 0)
            {
                ippiTransformQuantInvAddPred4x4_H264_16s_C1IR(
                    predAndRecon + 64 * geom.yBlk + 4 * geom.xBlk + 8 * plane,
                    16,
                    transform + 16 * chromaBlock,
                    &dcDiff[chromaBlock + 4 * plane],
                    predAndRecon + 64 * geom.yBlk + 4 * geom.xBlk + 8 * plane,
                    16,
                    g_MapLumaQpToChromaQp[mb.mbN.QpPrimeY],
                    numAcCoeff != 0,
                    0);
            }
        }
    }

    //Reset other chroma
    uCBPChroma &= ~(0xffffffff << 8);
    mb.mbN.CodedPattern4x4U = uCBPChroma & 0xf;
    mb.mbN.CodedPattern4x4V = (uCBPChroma >> 4) & 0xf;

    for (mfxU32 i = 0; i < 8; i++)
    {
        for(mfxU32 j = 0; j < 8; j++)
        {
            recMb[recPitch * i + 2 * j    ] = predAndRecon[16 * i + j];
            recMb[recPitch * i + 2 * j + 1] = predAndRecon[16 * i + j + 8];
        }
    }
}

void H264Pak::PredictMb(
    mfxFrameParamAVC& fp,
    mfxExtAvcRefSliceInfo& sliceInfo,
    mfxFrameSurface& fs,
    MbDescriptor& mb,
    mfxI16 scaleMatrix8x8[2][6][64],
    mfxI16 scaleMatrix8x8Inv[2][6][64],
    CoeffData& coeffData)
{
    if (mb.mbN.IntraMbFlag)
    {
        if (mb.mbN.MbType5Bits == MBTYPE_I_4x4)
        {
            if (mb.mbN.TransformFlag)
            {
                PredictMbIntra8x8(fp, fs, mb, scaleMatrix8x8, scaleMatrix8x8Inv, coeffData);
            }
            else
            {
                PredictMbIntra4x4(fp, fs, mb, coeffData);
            }
        }
        else if (mb.mbN.MbType5Bits == MBTYPE_I_IPCM)
        {
            assert(!"unsupported PCM");
        }
        else // intra 16x16
        {
            PredictMbIntra16x16(fp, fs, mb, coeffData);
        }

        if (fp.ChromaFormatIdc > 0)
        {
            PredictMbChroma(fp, sliceInfo, fs, mb, coeffData);
        }

    }
    else
    {
        PredictMbInter(
            fp,
            sliceInfo,
            fs,
            mb,
            scaleMatrix8x8,
            scaleMatrix8x8Inv,
            coeffData);

        if (fp.ChromaFormatIdc > 0)
        {
            PredictMbChroma(fp, sliceInfo, fs, mb, coeffData);
        }

        if (mb.mbN.CodedPattern4x4Y == 0)
        {
            mb.mbN.TransformFlag = 0;
        }

        if (mb.mbN.CodedPattern4x4Y == 0 &&
            mb.mbN.CodedPattern4x4U == 0 &&
            mb.mbN.CodedPattern4x4V == 0 &&
            mb.mbN.DcBlockCodedCbFlag == 0 &&
            mb.mbN.DcBlockCodedCrFlag == 0)
        {
            if ((fp.FrameType & MFX_FRAMETYPE_B) &&
                mb.mbN.Skip8x8Flag == 0xf)
            {
                mb.mbN.reserved3c = 1;
            }
            else if ((fp.FrameType & MFX_FRAMETYPE_P) &&
                mb.mbN.MbType5Bits == MBTYPE_BP_L0_16x16 &&
                mb.mbN.RefPicSelect[0][0] == 0)
            {
                mfxI16Pair skipMv =
                    mb.mbA == 0 || mb.mbB == 0 ||
                    (mb.mbA->IntraMbFlag == 0 && mb.mbA->RefPicSelect[0][1] == 0 && mb.mvA->mv[0][3] == nullMv) ||
                    (mb.mbB->IntraMbFlag == 0 && mb.mbB->RefPicSelect[0][2] == 0 && mb.mvB->mv[0][12] == nullMv)
                    ? nullMv
                    : GetMvPred(mb, 0, 0, 4, 4);

                if (mb.mvN->mv[0][0] == skipMv)
                {
                    mb.mbN.Skip8x8Flag = 0xf;
                    mb.mbN.reserved3c = 1;
                }
            }
        }
    }
}

#endif // MFX_ENABLE_H264_VIDEO_ENCODER
