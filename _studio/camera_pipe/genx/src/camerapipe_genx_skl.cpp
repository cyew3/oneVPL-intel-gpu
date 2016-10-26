//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2005-2014 Intel Corporation. All Rights Reserved.
//

#include <cm.h>
#include "Campipe.h"

_GENX_ void inline
ROWSWAP_8x8(matrix_ref<ushort, 8, 8> m)
{
    vector<ushort, 8> tmp_pixel_row;
#pragma unroll
    for(int i = 0; i < 4; i++)
    {
        tmp_pixel_row = m.row(i);
        m.row(i)      = m.row(7-i);
        m.row(7-i)    = tmp_pixel_row;
    }
}

_GENX_ void inline
ROWSWAP_8x16(matrix_ref<ushort, 8, 16> m)
{
    vector<ushort, 16> tmp_pixel_row;
#pragma unroll
    for(int i = 0; i < 4; i++)
    {
        tmp_pixel_row = m.row(i);
        m.row(i)      = m.row(7-i);
        m.row(7-i)   = tmp_pixel_row;
    }
}

_GENX_ void inline
ROWSWAP_16x16(matrix_ref<ushort, 16, 16> m)
{
    vector<ushort, 16> tmp_pixel_row;
#pragma unroll
    for(int i = 0; i < 8; i++)
    {
        tmp_pixel_row = m.row(i);
        m.row(i)      = m.row(15-i);
        m.row(15-i)   = tmp_pixel_row;
    }
}


_GENX_ void inline
ROWSWAP_12x24(matrix_ref<ushort, 12, 24> m)
{
    vector<ushort, 24> tmp_pixel_row;
#pragma unroll
    for(int i = 0; i < 6; i++)
    {
        tmp_pixel_row = m.row(i);
        m.row(i)      = m.row(11-i);
        m.row(11-i)   = tmp_pixel_row;
    }
}

_GENX_ void inline
ROWSWAP_16x24(matrix_ref<ushort, 16, 24> m)
{
    vector<ushort, 24> tmp_pixel_row;
#pragma unroll
    for(int i = 0; i < 8; i++)
    {
        tmp_pixel_row = m.row(i);
        m.row(i)      = m.row(15-i);
        m.row(15-i)   = tmp_pixel_row;
    }
}

_GENX_ void inline
ROWSWAP_20x32(matrix_ref<ushort, 20, 32> m)
{
    vector<ushort, 32> tmp_pixel_row;
#pragma unroll
    for(int i = 0; i < 10; i++)
    {
        tmp_pixel_row = m.row(i);
        m.row(i)      = m.row(19-i);
        m.row(19-i)   = tmp_pixel_row;
    }
}


// For bggr/rggb un-padded input
const unsigned int index_ini[8] = {7,6,5,4,3,2,1,0};
extern "C" _GENX_MAIN_ void
Padding_16bpp(SurfaceIndex ibuf, SurfaceIndex obuf,
                       int threadwidth  , int threadheight,
                       int InFrameWidth , int InFrameHeight,
                       int bitDepth, int BayerType)
{
    matrix<short,  8, 8> out, tmp_out;
    matrix<ushort, 8, 8> rd_in_pixels;
    int rd_h_pos, rd_v_pos;

    vector<uint, 8> index(index_ini);

    int wr_hor_pos = get_thread_origin_x() * 16 * sizeof(short);
    int wr_ver_pos = get_thread_origin_y() * 16 ;

    for(int wr_h_pos = wr_hor_pos; wr_h_pos < (wr_hor_pos + 32); wr_h_pos += 16) {
        for(int wr_v_pos = wr_ver_pos; wr_v_pos < (wr_ver_pos + 16); wr_v_pos += 8) {

            // compute rd_h_pos, rd_v_pos
            if(wr_h_pos == 0)
                rd_h_pos = 2;
            else if(wr_h_pos == (threadwidth - 1) * 8 * sizeof(short))
                rd_h_pos = (InFrameWidth - 9) * sizeof(short);
            else
                rd_h_pos = wr_h_pos - 16;//rd_h_pos = (get_thread_origin_x() - 1) * 8 * sizeof(short);

            if(wr_v_pos == 0)
                rd_v_pos = 1; //0
            else if(wr_v_pos == (threadheight - 1) * 8)
                rd_v_pos = (InFrameHeight - 9);//(InFrameHeight - 9);
            else
                rd_v_pos = wr_v_pos - 8;//wr_v_pos - 7; (get_thread_origin_y() - 1) * 8;

            // READ FROM SURFACE
            read(ibuf, rd_h_pos, rd_v_pos, rd_in_pixels);

            // ROW SWAP FOR GRBG/GBRG TYPE
            vector<ushort, 8> tmp_pixel_row;

            // ALIGN TO LSB
            rd_in_pixels >>= (16-bitDepth);

            // FLIP RIGHT TO LEFT, IF ON LEFT OR RIGHT BOUNDARY
            if( wr_h_pos == 0 | wr_h_pos == (threadwidth - 1) * 8 * sizeof(short))
            {
#pragma unroll
                for(int j = 0; j < 8; j++)
                {
                    vector_ref<ushort, 8> in_row = rd_in_pixels.row(j);
                    rd_in_pixels.row(j) = in_row.iselect(index); //tmp_out.row(j) = in_row.iselect(index);
                }
            }

            // COPY FROM INPUT TO tmp_out matrix

            // FLIP DOWN TO UP IF ON UPPER OR LOWER BOUNDARY
            if( wr_v_pos == 0 | wr_v_pos == (threadheight - 1) * 8 )
            {
#pragma unroll
                for(int j = 0; j < 8; j++)
                    out.row(7-j) = rd_in_pixels.row(j);
            }

            // COPY FROM INPUT
            else
            {
#pragma unroll
                for(int j = 0; j < 8; j++)
                    out.row(j) = rd_in_pixels.row(j);
            }
            // SWAP IF NEEDED
            write(obuf, wr_h_pos, wr_v_pos, out);
        }
    }
}

// For grbg/gbrg un-padded input
extern "C" _GENX_MAIN_ void
PaddingandFlipping_16bpp(SurfaceIndex ibuf, SurfaceIndex obuf,
                         int threadwidth  , int threadheight,
                         int InFrameWidth , int InFrameHeight,
                         int bitDepth, int BayerType)
{
    matrix<ushort,  8, 8> out, tmp_out;
    matrix<ushort, 8, 8> rd_in_pixels;
    int rd_h_pos, rd_v_pos;
    int OutFrameHeight = (InFrameHeight + 16);


    vector<uint, 8> index(index_ini);

    int wr_hor_pos = get_thread_origin_x() * 16 * sizeof(short);
    int wr_ver_pos = get_thread_origin_y() * 16 ;

    for(int wr_h_pos = wr_hor_pos; wr_h_pos < (wr_hor_pos + 32); wr_h_pos += 16) {
        for(int wr_v_pos = wr_ver_pos; wr_v_pos < (wr_ver_pos + 16); wr_v_pos += 8) {

            // compute rd_h_pos, rd_v_pos
            if(wr_h_pos == 0)
                rd_h_pos = 2;
            else if(wr_h_pos == (threadwidth - 1) * 8 * sizeof(short))
                rd_h_pos = (InFrameWidth - 9) * sizeof(short);
            else
                rd_h_pos = wr_h_pos - 16;//rd_h_pos = (get_thread_origin_x() - 1) * 8 * sizeof(short);

            if(wr_v_pos == 0)
                rd_v_pos = 1; //0
            else if(wr_v_pos == (threadheight - 1) * 8)
                rd_v_pos = (InFrameHeight - 9);//(InFrameHeight - 9);
            else
                rd_v_pos = wr_v_pos - 8;//wr_v_pos - 7; (get_thread_origin_y() - 1) * 8;

            // READ FROM SURFACE
            read(ibuf, rd_h_pos, rd_v_pos, rd_in_pixels);

            // ROW SWAP FOR GRBG/GBRG TYPE
            vector<ushort, 8> tmp_pixel_row;

            // ALIGN TO LSB
            rd_in_pixels >>= (16-bitDepth);

            // FLIP RIGHT TO LEFT, IF ON LEFT OR RIGHT BOUNDARY
            if( wr_h_pos == 0 | wr_h_pos == (threadwidth - 1) * 8 * sizeof(short))
            {
#pragma unroll
                for(int j = 0; j < 8; j++)
                {
                    vector_ref<ushort, 8> in_row = rd_in_pixels.row(j);
                    rd_in_pixels.row(j) = in_row.iselect(index); //tmp_out.row(j) = in_row.iselect(index);
                }
            }

            // COPY FROM INPUT TO tmp_out matrix

            // FLIP DOWN TO UP IF ON UPPER OR LOWER BOUNDARY
            if( wr_v_pos == 0 | wr_v_pos == (threadheight - 1) * 8 )
            {
#pragma unroll
                for(int j = 0; j < 8; j++)
                    out.row(7-j) = rd_in_pixels.row(j);
            }

            // COPY FROM INPUT
            else
            {
#pragma unroll
                for(int j = 0; j < 8; j++)
                    out.row(j) = rd_in_pixels.row(j);
            }

            // GRBG/GBRG Type
             ROWSWAP_8x8(out);

            // SWAP IF NEEDED
            int write_v_pos = (OutFrameHeight - wr_v_pos) - 8;
            write(obuf, wr_h_pos, write_v_pos, out);
            // write(obuf, wr_h_pos, wr_v_pos, out);
        }
    }
}

//handling 16x16  // For grbg/gbrg padded input
extern "C" _GENX_MAIN_ void
Flipping_16bpp(SurfaceIndex ibuf, SurfaceIndex obuf,
               int threadwidth  , int threadheight,
               int InFrameWidth , int InFrameHeight,
               int bitDepth, int BayerType)
{
    matrix<ushort, 16, 16> in, out;
    int OutFrameHeight = (InFrameHeight + 16);

    int rd_h_pos = get_thread_origin_x() * 16 * sizeof(short);
    int rd_v_pos = get_thread_origin_y() * 16;

    read(ibuf, rd_h_pos, rd_v_pos  , in.select<8,1,16,1>(0,0));
    read(ibuf, rd_h_pos, rd_v_pos+8, in.select<8,1,16,1>(8,0));

    in >>= (16-bitDepth);

    // GRBG/GBRG Type
#pragma unroll
    for(int i = 0; i < 16; i++)
        out.row(15-i) = in.row(i);

    int wr_v_pos = (OutFrameHeight - rd_v_pos) - 16;
    write(obuf, rd_h_pos, wr_v_pos  , out.select<8,1,16,1>(0,0));
    write(obuf, rd_h_pos, wr_v_pos+8, out.select<8,1,16,1>(8,0));
}

_GENX_ void inline
BLC_CORRECT(matrix_ref<ushort, 16, 16> in,
            short B_amount, short Gtop_amount, short Gbot_amount, short R_amount, ushort max_input)
{
    matrix<int, 16, 16> m, tmp;
    m.select<8, 2, 8, 2>(0,0) = cm_add<int>(in.select<8, 2, 8, 2>(0,0), -B_amount   );
    m.select<8, 2, 8, 2>(0,1) = cm_add<int>(in.select<8, 2, 8, 2>(0,1), -Gtop_amount);
    m.select<8, 2, 8, 2>(1,0) = cm_add<int>(in.select<8, 2, 8, 2>(1,0), -Gbot_amount);
    m.select<8, 2, 8, 2>(1,1) = cm_add<int>(in.select<8, 2, 8, 2>(1,1), -R_amount   );

    m.merge(0, m < 0        );
    tmp.merge(max_input, m, m > max_input);

    in = tmp;
}

_GENX_ void inline
VIG_CORRECT(matrix_ref<ushort, 16, 16> m,
            matrix_ref<ushort, 16, 16> mask,
            ushort max_input)
{
    matrix<uint, 16, 16> tmp;
    tmp = cm_mul<uint>(m, mask);

    tmp = cm_add <uint>(tmp, 128 );      //8;
    tmp = cm_quot<uint>(tmp, 256);       //16;

    m = tmp;
    m.merge(0,         tmp < 0        );
    m.merge(max_input, tmp > max_input);
    //tmp.merge(0, tmp   < 0        );
    //m.merge(max_input, tmp, tmp > max_input);
    //m = tmp;
}

_GENX_ void inline
AWB_CORRECT(matrix_ref<ushort, 16, 16> in,
            float B_scale, float Gtop_scale, float Gbot_scale, float R_scale, ushort max_input)
{
    matrix<uint, 16, 16> m;
    m.select<8, 2, 8, 2>(0,0) = cm_mul<uint>(in.select<8, 2, 8, 2>(0,0), B_scale    );//, SAT);
    m.select<8, 2, 8, 2>(0,1) = cm_mul<uint>(in.select<8, 2, 8, 2>(0,1), Gtop_scale);//, SAT);
    m.select<8, 2, 8, 2>(1,0) = cm_mul<uint>(in.select<8, 2, 8, 2>(1,0), Gbot_scale);//, SAT);
    m.select<8, 2, 8, 2>(1,1) = cm_mul<uint>(in.select<8, 2, 8, 2>(1,1), R_scale    );//, SAT);

    in = m;
    in.merge(0,            m < 0        );
    in.merge(max_input, m > max_input);
}

extern "C" _GENX_MAIN_
void BAYER_CORRECTION(SurfaceIndex PaddedInputIndex,
                      SurfaceIndex BayerIndex,
                      SurfaceIndex MaskIndex,
                      ushort Enable_BLC, ushort Enable_VIG, ushort Enable_AWB,
                      short B_amount, short Gtop_amount, short Gbot_amount, short R_amount,
                      float  B_scale, float Gtop_scale , float Gbot_scale , float R_scale,
                      ushort max_input, int first, int bitDepth, int BayerType)
{
    cm_fsetround(CM_RTZ);

    matrix<ushort, 16, 16> BayerMatrix;
    matrix<ushort, 16, 16> MaskMatrix ;
    matrix<ushort, 16, 16> OutMatrix  ;


    uint h_pos = get_thread_origin_x() * 16 * sizeof(short);
    uint v_pos = get_thread_origin_y() * 16;

    if(first == 0) // if not first kernel
    {
        read(BayerIndex, h_pos, v_pos  , BayerMatrix.select<8,1,16,1>(0,0));
        read(BayerIndex, h_pos, v_pos+8, BayerMatrix.select<8,1,16,1>(8,0));
    }
    else           // if first kernel, input from paddedinput
    {
        read(PaddedInputIndex, h_pos, v_pos  , BayerMatrix.select<8,1,16,1>(0,0));
        read(PaddedInputIndex, h_pos, v_pos+8, BayerMatrix.select<8,1,16,1>(8,0));

        if(BayerType == GRBG || BayerType == GBRG)
        {
            ROWSWAP_16x16(BayerMatrix);
        }
        BayerMatrix >>= (16-bitDepth);
    }

    /////////////////////////////////////////////////////////////////////////////////////////

    if(Enable_BLC == 1)
        BLC_CORRECT( BayerMatrix, B_amount, Gtop_amount, Gbot_amount, R_amount, max_input);

    if(Enable_VIG == 1)
    {
        read(MaskIndex, h_pos, v_pos  , MaskMatrix.select<8,1,16,1>(0,0));
        read(MaskIndex, h_pos, v_pos+8, MaskMatrix.select<8,1,16,1>(8,0));
        VIG_CORRECT( BayerMatrix, MaskMatrix, max_input);
    }

    if(Enable_AWB == 1)
        AWB_CORRECT( BayerMatrix, B_scale,  Gtop_scale,  Gbot_scale,  R_scale,  max_input);

    write(BayerIndex, h_pos, v_pos  , BayerMatrix.select<8,1,16,1>(0,0));
    write(BayerIndex, h_pos, v_pos+8, BayerMatrix.select<8,1,16,1>(8,0));
}


_GENX_ void inline
HOT_CORRECT_AND_DYN(matrix_ref<ushort, 12, 24> in,
                    matrix_ref<ushort,  8, 16> out,
                    matrix_ref<ushort,  8, 16> dyn_range,
                    int threshPixelDiff, int threshNumPix, int amount)
{
    ushort ThreshPixelDiff = threshPixelDiff;
    ushort ThreshNumPix    = threshNumPix;

    vector<ushort, 16> diff_num, mask;
    vector<ushort, 16> tmp, min, max;
    vector<ushort, 16> v0, v1, v2, v3, v4, v5, v6, v7, v8;
    vector<ushort, 16> mul_amount = amount;

#pragma unroll
    for(int j = 0; j < 8; j++)
    {
        v0.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j  ,0), mul_amount);
        v1.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j  ,2), mul_amount);
        v2.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j  ,4), mul_amount);
        v3.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+2,0), mul_amount);
        v4.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+2,2), mul_amount);
        v5.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+2,4), mul_amount);
        v6.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+4,0), mul_amount);
        v7.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+4,2), mul_amount);
        v8.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+4,4), mul_amount);

        diff_num  = 0;
        mask = (cm_abs<uint>(cm_add<int>(v0, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v1, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v2, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v3, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v5, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v6, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v7, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v8, -v4)) > ThreshPixelDiff);
        diff_num += mask;

        tmp = cm_min<ushort>(v0, v1);
        v1  = cm_max<ushort>(v0, v1);
        v0  = tmp;

        tmp = cm_min<ushort>(v0, v2);
        v2  = cm_max<ushort>(v0, v2);
        v0  = tmp;

        tmp = cm_min<ushort>(v1, v2);
        v2  = cm_max<ushort>(v1, v2);
        v1  = tmp;

        tmp = cm_min<ushort>(v3, v4);
        v4  = cm_max<ushort>(v3, v4);
        v3  = tmp;

        tmp = cm_min<ushort>(v3, v5);
        v5  = cm_max<ushort>(v3, v5);
        v3  = tmp;

        tmp = cm_min<ushort>(v4, v5);
        v5  = cm_max<ushort>(v4, v5);
        v4  = tmp;

        tmp = cm_min<ushort>(v6, v7);
        v7  = cm_max<ushort>(v6, v7);
        v6  = tmp;

        tmp = cm_min<ushort>(v6, v8);
        v8  = cm_max<ushort>(v6, v8);
        v6  = tmp;

        tmp = cm_min<ushort>(v7, v8);
        v8  = cm_max<ushort>(v7, v8);
        v7  = tmp;

        max = cm_max<ushort>(v2, v5);
        max = cm_max<ushort>(max,v8);

        min = cm_min<ushort>(v0, v3);
        min = cm_min<ushort>(min,v6);

        v3  = cm_max<ushort>(v0, v3);
        v6  = cm_max<ushort>(v3, v6);  // V6 > V3, V0

        tmp = cm_min<ushort>(v1, v4);
        v4  = cm_max<ushort>(v1, v4);
        v1  = tmp;
        v7  = cm_max<ushort>(v1, v7);
        v4  = cm_min<ushort>(v4, v7);  // V1 < V4 < V7

        v2  = cm_min<ushort>(v2, v5);
        v2  = cm_min<ushort>(v2, v8);  // V2 < V5, V8

        tmp = cm_min<ushort>(v2, v4);
        v4  = cm_max<ushort>(v2, v4);
        v2  = tmp;

        v6  = cm_max<ushort>(v2, v6);

        v4  = cm_min<ushort>(v4, v6);  // V4 = MEDIAN(V2, V4, V6);

        out.row(j).merge(v4, in.select<1,1,16,1>(j+2,2), (diff_num >= ThreshNumPix));
        dyn_range.row(j) = max - min;
    }

}

_GENX_ void inline
HOT_CORRECT(matrix_ref<ushort, 12, 24> in,
            matrix_ref<ushort,  8, 16> out,
            int threshPixelDiff, int threshNumPix, int amount)
{
    ushort ThreshPixelDiff = threshPixelDiff;
    ushort ThreshNumPix    = threshNumPix;

    vector<ushort, 16> diff_num, mask;
    vector<ushort, 16> tmp;
    vector<ushort, 16> v0, v1, v2, v3, v4, v5, v6, v7, v8;
    vector<ushort, 16> mul_amount = amount;

#pragma unroll
    for(int j = 0; j < 8; j++)
    {
        v0.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j  ,0), mul_amount);
        v1.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j  ,2), mul_amount);
        v2.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j  ,4), mul_amount);
        v3.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+2,0), mul_amount);
        v4.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+2,2), mul_amount);
        v5.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+2,4), mul_amount);
        v6.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+4,0), mul_amount);
        v7.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+4,2), mul_amount);
        v8.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+4,4), mul_amount);

        diff_num  = 0;
        mask = (cm_abs<uint>(cm_add<int>(v0, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v1, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v2, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v3, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v5, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v6, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v7, -v4)) > ThreshPixelDiff);
        diff_num += mask;
        mask = (cm_abs<uint>(cm_add<int>(v8, -v4)) > ThreshPixelDiff);
        diff_num += mask;

        tmp = cm_min<ushort>(v0, v1);
        v1  = cm_max<ushort>(v0, v1);
        v0  = tmp;

        tmp = cm_min<ushort>(v0, v2);
        v2  = cm_max<ushort>(v0, v2);
        v0  = tmp;

        tmp = cm_min<ushort>(v1, v2);
        v2  = cm_max<ushort>(v1, v2);
        v1  = tmp;

        tmp = cm_min<ushort>(v3, v4);
        v4  = cm_max<ushort>(v3, v4);
        v3  = tmp;

        tmp = cm_min<ushort>(v3, v5);
        v5  = cm_max<ushort>(v3, v5);
        v3  = tmp;

        tmp = cm_min<ushort>(v4, v5);
        v5  = cm_max<ushort>(v4, v5);
        v4  = tmp;

        tmp = cm_min<ushort>(v6, v7);
        v7  = cm_max<ushort>(v6, v7);
        v6  = tmp;

        tmp = cm_min<ushort>(v6, v8);
        v8  = cm_max<ushort>(v6, v8);
        v6  = tmp;

        tmp = cm_min<ushort>(v7, v8);
        v8  = cm_max<ushort>(v7, v8);
        v7  = tmp;

        v3  = cm_max<ushort>(v0, v3);
        v6  = cm_max<ushort>(v3, v6);  // V6 > V3, V0

        tmp = cm_min<ushort>(v1, v4);
        v4  = cm_max<ushort>(v1, v4);
        v1  = tmp;
        v7  = cm_max<ushort>(v1, v7);
        v4  = cm_min<ushort>(v4, v7);  // V1 < V4 < V7

        v2  = cm_min<ushort>(v2, v5);
        v2  = cm_min<ushort>(v2, v8);  // V2 < V5, V8

        tmp = cm_min<ushort>(v2, v4);
        v4  = cm_max<ushort>(v2, v4);
        v2  = tmp;

        v6  = cm_max<ushort>(v2, v6);

        v4  = cm_min<ushort>(v4, v6);  // V4 = MEDIAN(V2, V4, V6);

        //out.row(j) = v4;
        out.row(j).merge(v4, in.select<1,1,16,1>(j+2,2), (diff_num >= ThreshNumPix));
    }

}

_GENX_ void inline
DYNRANGE(matrix_ref<ushort, 12, 24> in,
         matrix_ref<ushort,  8, 16> dyn_range,
         int threshPixelDiff, int threshNumPix, int amount)
{
    ushort ThreshPixelDiff = threshPixelDiff;
    ushort ThreshNumPix    = threshNumPix;

    vector<ushort, 16> diff_num, mask;
    vector<ushort, 16> tmp, min, max;
    vector<ushort, 16> v0, v1, v2, v3, v4, v5, v6, v7, v8;
    vector<ushort, 16> mul_amount = amount;

#pragma unroll
    for(int j = 0; j < 8; j++)
    {
        v0.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j  ,0), mul_amount);
        v1.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j  ,2), mul_amount);
        v2.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j  ,4), mul_amount);
        v3.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+2,0), mul_amount);
        v4.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+2,2), mul_amount);
        v5.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+2,4), mul_amount);
        v6.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+4,0), mul_amount);
        v7.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+4,2), mul_amount);
        v8.select<16,1>(0) = cm_mul<ushort>(in.select<1,1,16,1>(j+4,4), mul_amount);

        tmp = cm_min<ushort>(v0, v1);
        v1  = cm_max<ushort>(v0, v1);
        v0  = tmp;

        tmp = cm_min<ushort>(v0, v2);
        v2  = cm_max<ushort>(v0, v2);
        v0  = tmp;

        tmp = cm_min<ushort>(v1, v2);
        v2  = cm_max<ushort>(v1, v2);
        v1  = tmp;

        tmp = cm_min<ushort>(v3, v4);
        v4  = cm_max<ushort>(v3, v4);
        v3  = tmp;

        tmp = cm_min<ushort>(v3, v5);
        v5  = cm_max<ushort>(v3, v5);
        v3  = tmp;

        tmp = cm_min<ushort>(v4, v5);
        v5  = cm_max<ushort>(v4, v5);
        v4  = tmp;

        tmp = cm_min<ushort>(v6, v7);
        v7  = cm_max<ushort>(v6, v7);
        v6  = tmp;

        tmp = cm_min<ushort>(v6, v8);
        v8  = cm_max<ushort>(v6, v8);
        v6  = tmp;

        tmp = cm_min<ushort>(v7, v8);
        v8  = cm_max<ushort>(v7, v8);
        v7  = tmp;

        max = cm_max<ushort>(v2, v5);
        max = cm_max<ushort>(max,v8);

        min = cm_min<ushort>(v0, v3);
        min = cm_min<ushort>(min,v6);

        dyn_range.row(j) = max - min;
    }

}

extern "C" _GENX_MAIN_ void
HotPixel_DYN_SKL(SurfaceIndex InBayerIndex,
                   SurfaceIndex OutBayerIndex,
                 SurfaceIndex OutDynRangeIndex,
                 int ThreshPixelDiff, int ThreshNumPix, int amount,
                 int firstflag, int bitDepth, int BayerType)
{
    int rd_h_pos = (get_thread_origin_x() * 16 - 2) * sizeof(short) ;
    int rd_v_pos = (get_thread_origin_y() * 16 - 2)                    ;
    int wr_h_pos = (get_thread_origin_x() * 16    ) * sizeof(short) ;
    int wr_v_pos = (get_thread_origin_y() * 16    )                    ;

    matrix<ushort, 12, 24> bayer_in ;
    matrix<ushort, 16, 16> bayer_out;
    matrix<ushort, 16, 16> dyn_range;

    //////     READ_IN 12x24 BLOCK   ///////////////////////
    read(InBayerIndex, rd_h_pos   , rd_v_pos   , bayer_in.select< 8,1,16,1>(0,0 ));
    read(InBayerIndex, rd_h_pos   , rd_v_pos+8 , bayer_in.select< 4,1,16,1>(8,0 ));
    read(InBayerIndex, rd_h_pos+32, rd_v_pos   , bayer_in.select<12,1, 8,1>(0,16));

    if(firstflag == 1)
    {
        if(BayerType == GRBG || BayerType == GBRG)
        {
            ROWSWAP_12x24(bayer_in);
        }
        bayer_in >>= (16-bitDepth);
    }
    //////   PROCESSING  TOP  8x16   ///////////////////////
    HOT_CORRECT_AND_DYN(bayer_in, bayer_out.select<8,1,16,1>(0,0), dyn_range.select<8,1,16,1>(0,0), ThreshPixelDiff, ThreshNumPix, amount);

    //////   WRITE OUT 16x16 BLOCK   ///////////////////////
    write(OutBayerIndex   , wr_h_pos, wr_v_pos, bayer_out.select<8,1,16,1>(0,0));
    write(OutDynRangeIndex, wr_h_pos, wr_v_pos, dyn_range.select<8,1,16,1>(0,0));

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////     2ND 8x16 BLOCK      /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    rd_v_pos += 12;

    //////     READ_IN 20x16 BLOCK   ///////////////////////
    bayer_in.row(3) = bayer_in.row(11);
    bayer_in.row(2) = bayer_in.row(10);
    bayer_in.row(1) = bayer_in.row( 9);
    bayer_in.row(0) = bayer_in.row( 8);

    read(InBayerIndex, rd_h_pos   , rd_v_pos, bayer_in.select<8,1,16,1>(4,0 ));
    read(InBayerIndex, rd_h_pos+32, rd_v_pos, bayer_in.select<8,1, 8,1>(4,16));

    if(firstflag == 1)
    {
        if(BayerType == GRBG || BayerType == GBRG)
        {
            ROWSWAP_12x24(bayer_in);
        }
        bayer_in.select<8,1,24,1>(4,0) >>= (16-bitDepth);
    }
    //////   PROCESSING  RIGHT 16x8  ///////////////////////
    HOT_CORRECT_AND_DYN(bayer_in, bayer_out.select<8,1,16,1>(8,0), dyn_range.select<8,1,16,1>(8,0), ThreshPixelDiff, ThreshNumPix, amount);

    //////   WRITE OUT 16x16 BLOCK   ///////////////////////
    write(OutBayerIndex   , wr_h_pos, wr_v_pos+8, bayer_out.select<8,1,16,1>(8,0));
    write(OutDynRangeIndex, wr_h_pos, wr_v_pos+8, dyn_range.select<8,1,16,1>(8,0));
}

extern "C" _GENX_MAIN_ void
HotPixel_SKL(SurfaceIndex InBayerIndex,
               SurfaceIndex OutBayerIndex,
             SurfaceIndex OutDynRangeIndex,
             int ThreshPixelDiff, int ThreshNumPix, int amount,
             int firstflag, int bitDepth, int BayerType)
{
    int rd_h_pos = (get_thread_origin_x() * 16 - 2) * sizeof(short) ;
    int rd_v_pos = (get_thread_origin_y() * 16 - 2)                    ;
    int wr_h_pos = (get_thread_origin_x() * 16    ) * sizeof(short) ;
    int wr_v_pos = (get_thread_origin_y() * 16    )                    ;

    matrix<ushort, 12, 24> bayer_in ;
    matrix<ushort, 16, 16> bayer_out;
    matrix<ushort, 16, 16> dyn_range;

    //////     READ_IN 12x24 BLOCK   ///////////////////////

    read(InBayerIndex, rd_h_pos   , rd_v_pos   , bayer_in.select< 8,1,16,1>(0,0 ));
    read(InBayerIndex, rd_h_pos   , rd_v_pos+8 , bayer_in.select< 4,1,16,1>(8,0 ));
    read(InBayerIndex, rd_h_pos+32, rd_v_pos   , bayer_in.select<12,1, 8,1>(0,16));

    if(firstflag == 1)
    {
        if(BayerType == GRBG || BayerType == GBRG)
        {
            ROWSWAP_12x24(bayer_in);
        }
        bayer_in >>= (16-bitDepth);
    }

    //////   PROCESSING  TOP  8x16   ///////////////////////
    HOT_CORRECT(bayer_in, bayer_out.select<8,1,16,1>(0,0), ThreshPixelDiff, ThreshNumPix, amount);

    //////   WRITE OUT 16x16 BLOCK   ///////////////////////
    write(OutBayerIndex   , wr_h_pos, wr_v_pos, bayer_out.select<8,1,16,1>(0,0));

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////     2ND 8x16 BLOCK      /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    rd_v_pos += 12;

    //////     READ_IN 20x16 BLOCK   ///////////////////////
    bayer_in.row(3) = bayer_in.row(11);
    bayer_in.row(2) = bayer_in.row(10);
    bayer_in.row(1) = bayer_in.row( 9);
    bayer_in.row(0) = bayer_in.row( 8);

    read(InBayerIndex, rd_h_pos   , rd_v_pos, bayer_in.select<8,1,16,1>(4,0 ));
    read(InBayerIndex, rd_h_pos+32, rd_v_pos, bayer_in.select<8,1, 8,1>(4,16));

    if(firstflag == 1)
    {
        if(BayerType == GRBG || BayerType == GBRG)
        {
            ROWSWAP_12x24(bayer_in);
        }
        bayer_in.select<8,1,24,1>(4,0) >>= (16-bitDepth);
    }
    //////   PROCESSING  RIGHT 16x8  ///////////////////////
    HOT_CORRECT(bayer_in, bayer_out.select<8,1,16,1>(8,0), ThreshPixelDiff, ThreshNumPix, amount);

    //////   WRITE OUT 16x16 BLOCK   ///////////////////////
    write(OutBayerIndex   , wr_h_pos, wr_v_pos+8, bayer_out.select<8,1,16,1>(8,0));
}

extern "C" _GENX_MAIN_ void
DYNRANGE_CALC_SKL(SurfaceIndex InBayerIndex,
                    SurfaceIndex OutBayerIndex,
                  SurfaceIndex OutDynRangeIndex,
                  int ThreshPixelDiff, int ThreshNumPix, int amount,
                  int firstflag, int bitDepth, int BayerType)
{
    int rd_h_pos = (get_thread_origin_x() * 16 - 2) * sizeof(short) ;
    int rd_v_pos = (get_thread_origin_y() * 16 - 2)                    ;
    int wr_h_pos = (get_thread_origin_x() * 16    ) * sizeof(short) ;
    int wr_v_pos = (get_thread_origin_y() * 16    )                    ;

    matrix<ushort, 12, 24> bayer_in ;
    matrix<ushort, 16, 16> bayer_out;
    matrix<ushort, 16, 16> dyn_range;

    //////     READ_IN 12x24 BLOCK   ///////////////////////

    read(InBayerIndex, rd_h_pos   , rd_v_pos   , bayer_in.select< 8,1,16,1>(0,0 ));
    read(InBayerIndex, rd_h_pos   , rd_v_pos+8 , bayer_in.select< 4,1,16,1>(8,0 ));
    read(InBayerIndex, rd_h_pos+32, rd_v_pos   , bayer_in.select<12,1, 8,1>(0,16));

    if(firstflag == 1)
    {
        if(BayerType == GRBG || BayerType == GBRG)
        {
            ROWSWAP_12x24(bayer_in);
        }
        bayer_in >>= (16-bitDepth);
    }
    //////   PROCESSING  TOP  8x16   ///////////////////////
    DYNRANGE(bayer_in, dyn_range.select<8,1,16,1>(0,0), ThreshPixelDiff, ThreshNumPix, amount);

    //////   WRITE OUT 16x16 BLOCK   ///////////////////////
    write(OutDynRangeIndex, wr_h_pos, wr_v_pos, dyn_range.select<8,1,16,1>(0,0));

    if(firstflag == 1)
        write(OutBayerIndex, wr_h_pos, wr_v_pos, bayer_in.select<8,1,16,1>(2,2));

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////     2ND 8x16 BLOCK      /////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    rd_v_pos += 12;

    //////     READ_IN 20x16 BLOCK   ///////////////////////
    bayer_in.row(3) = bayer_in.row(11);
    bayer_in.row(2) = bayer_in.row(10);
    bayer_in.row(1) = bayer_in.row( 9);
    bayer_in.row(0) = bayer_in.row( 8);

    read(InBayerIndex, rd_h_pos   , rd_v_pos, bayer_in.select<8,1,16,1>(4,0 ));
    read(InBayerIndex, rd_h_pos+32, rd_v_pos, bayer_in.select<8,1, 8,1>(4,16));

    if(firstflag == 1)
    {
        if(BayerType == GRBG || BayerType == GBRG)
        {
            ROWSWAP_12x24(bayer_in);
        }
        bayer_in.select<8,1,24,1>(4,0) >>= (16-bitDepth);
    }
    //////   PROCESSING  RIGHT 16x8  ///////////////////////
    DYNRANGE(bayer_in, dyn_range.select<8,1,16,1>(8,0), ThreshPixelDiff, ThreshNumPix, amount);

    //////   WRITE OUT 16x16 BLOCK   ///////////////////////
    write(OutDynRangeIndex, wr_h_pos, wr_v_pos+8, dyn_range.select<8,1,16,1>(8,0));

    if(firstflag == 1)
        write(OutBayerIndex, wr_h_pos, wr_v_pos+8, bayer_in.select<8,1,16,1>(2,2));

}


_GENX_ void inline
DENOISE(matrix_ref<ushort, 9, 24> in,        // 36 registers
        matrix_ref<ushort, 1, 16> thrscale,  //  1 registers
        matrix_ref<ushort, 1, 16> out,         //  1 registers
        matrix_ref<ushort, 5, 5> dw,
        vector_ref<ushort, 8 > prt,
        vector_ref<ushort, 8 > prw,
        matrix_ref<ushort, 6, 32> prt_table,
        matrix_ref<ushort, 4, 8 > prw_table,
        ushort thrmin, int shift_amount)
{
    vector<ushort, 16> debug;
    vector<int   , 16> tmp_int;
    vector<uint  , 16> tmp;
    vector<uint  , 16> sum(0);
    vector<uint  , 16> wgt(0);
    vector<ushort,  8> prw_v ;
    vector<ushort, 16> tmpw, dw_tmp, index;
    vector<ushort, 16> diff, tpix, mask;
    matrix<ushort, 1, 16> centerpix = in.select<1,1,16,1>(4,4) >> shift_amount;
    matrix<ushort, 6, 16> comp_operand;   // <= thrmin

#if 1
    comp_operand.row(0) = cm_max<ushort>((prt(1) * thrscale + 8) >> 4, ( thrmin >> 1 ));
    comp_operand.row(1) = cm_max<ushort>((prt(2) * thrscale + 8) >> 4, ( thrmin      ));
    comp_operand.row(2) = cm_max<ushort>((prt(3) * thrscale + 8) >> 4, ( thrmin      ));
    comp_operand.row(3) = cm_max<ushort>((prt(4) * thrscale + 8) >> 4, ( thrmin         ));
    comp_operand.row(4) = cm_max<ushort>((prt(5) * thrscale + 8) >> 4, ( thrmin         ));
    comp_operand.row(5) = cm_max<ushort>((prt(6) * thrscale + 8) >> 4, ( thrmin         ));
#else
#pragma unroll
    for(int i = 0; i < 6; i++) {
        vector<uint, 16> v = thrscale;
        comp_operand.row(i) = prt_table.row(i).iselect(v);
    }
#endif

    vector<ushort, 24> v_in;

#pragma unroll
    for(int j = 0; j < 5; j++) {
#pragma unroll
        for(int i = 0; i < 5; i++) {

            tpix  = in.select<1,1,16,1>(i*2,j*2)        ;
            diff  = cm_abs<ushort>( centerpix - tpix )  ;
            prw_v = cm_mul<ushort>( prw,   dw(i,j)) >> 2;

            tmpw.merge(prw_v(6), prw_v(0), diff < comp_operand.row(5));
            tmpw.merge(prw_v(5),           diff < comp_operand.row(4));
            tmpw.merge(prw_v(4),           diff < comp_operand.row(3));
            tmpw.merge(prw_v(3),           diff < comp_operand.row(2));
            tmpw.merge(prw_v(2),           diff < comp_operand.row(1));
            tmpw.merge(prw_v(1),           diff < comp_operand.row(0));
            sum += (tpix * tmpw) >> shift_amount;
            wgt += tmpw;
        }
    }

    mask = (wgt <= 0);
    wgt.merge(1, mask);
    out = cm_quot(sum, wgt);
    out.merge(centerpix, mask);
}



unsigned short const in_init[32] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
                                    16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31};

extern "C" _GENX_MAIN_
void BayerDenoise(SurfaceIndex inIndex,      // surface to be denoised
                  SurfaceIndex rangeIndex,   // surface of local dyn range
                  SurfaceIndex outIndex,     // output surface
                  vector<ushort, 25> dw_v,   // distance weights
                  vector<ushort, 6> prt_v,   // threashold
                  vector<ushort, 6> prw_v,   // range weights
                  ushort dn_thmax,           // 5x5BLF_RangeThrDynRMax
                  ushort dn_thmin,           // 5x5BLF_RangeThrDynRMin
                  int firstflag,
                  int bitDepth,
                  int BayerType)
{
    int rd_h_pos = (get_thread_origin_x() * 16 - 4) * sizeof(short);
    int rd_v_pos = (get_thread_origin_y() * 16 - 4);

    int wr_h_pos = (get_thread_origin_x() * 16 + 0) * sizeof(short);
    int wr_v_pos = (get_thread_origin_y() * 16 + 0);

    ushort thrmin   = 256;                     // m_5x5BLF_RangeThrDynRMin;
    int shift_amount = 0;
    matrix<ushort, 8,  16> out;
    matrix<ushort, 8,  16> thrscale;
    matrix<ushort, 8,  16> local_dyn_range;
    matrix<ushort, 16, 24> rd_in;
    matrix_ref<ushort, 5, 5> dw = dw_v.format<ushort, 5, 5>();
    vector<ushort, 32> in(in_init);
    vector<ushort, 8 > prt(0);
    vector<ushort, 8 > prw(0);

    prt.select<6,1>(1) = prt_v;
    prw.select<6,1>(1) = prw_v;

    matrix<ushort, 6, 32> prt_table;
    prt_table.row(0)     = cm_max<ushort>((prt(1  ) * in + 8) >> 4, ( thrmin >> 1 ));
#pragma unroll
    for(int j = 1; j < 6; j++)
        prt_table.row(j) = cm_max<ushort>((prt(j+1) * in + 8) >> 4, ( thrmin      ));


    matrix<ushort, 4, 8> prw_table;
    //////////////////////////////////////////////////////////////////////////////////////
    //////////////    1st 8x16 BLOCK  ////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////

    read(inIndex, rd_h_pos   , rd_v_pos   , rd_in.select<8 ,1,16,1>(0 ,0 ));
    read(inIndex, rd_h_pos   , rd_v_pos+8 , rd_in.select<8 ,1,16,1>(8 ,0 ));
    read(inIndex, rd_h_pos+32, rd_v_pos   , rd_in.select<16,1,8 ,1>(0 ,16));

    if(firstflag == 1)
    {
        if(BayerType == GRBG || BayerType == GBRG)
        {
            ROWSWAP_16x24(rd_in);
        }
        rd_in >>= (16-bitDepth);
    }

    read(rangeIndex, wr_h_pos, wr_v_pos, local_dyn_range);
    thrscale = cm_min<ushort>(cm_max<ushort>(256, local_dyn_range), 2048);   //    m_RangeThrScale_Shift;
    thrscale = thrscale >> 7;

//#pragma unroll
    for(int i = 0; i < 8; i++)
        DENOISE(rd_in.select<9,1,24,1>(i,0), thrscale.row(i), out.row(i), dw, prt, prw, prt_table, prw_table, thrmin, shift_amount);

    write(outIndex, wr_h_pos, wr_v_pos  , out);

    //////////////////////////////////////////////////////////////////////////////////////
    //////////////    2nd 8x16 BLOCK  ////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////
    rd_in.select<8,1,24,1>(0,0) = rd_in.select<8,1,24,1>(8,0);

    read(inIndex, rd_h_pos   , rd_v_pos+16, rd_in.select<8 ,1,16,1>(8,0 ));
    read(inIndex, rd_h_pos+32, rd_v_pos+16, rd_in.select<8 ,1,8 ,1>(8,16));

    if(firstflag == 1)
    {
        if(BayerType == GRBG || BayerType == GBRG)
        {
            ROWSWAP_16x24(rd_in);
        }
        rd_in.select<8,1,24,1>(8,0) >>= (16-bitDepth);
    }

    read(rangeIndex, wr_h_pos, wr_v_pos+8, local_dyn_range);
    thrscale = cm_min<ushort>(cm_max<ushort>(256, local_dyn_range), 2048);            // <= 2048
    thrscale = thrscale >> 7;                            //    m_RangeThrScale_Shift;  // <= 16

//#pragma unroll
    for(int i = 0; i < 8; i++)
        DENOISE(rd_in.select<9,1,24,1>(i,0), thrscale.row(i), out.row(i), dw, prt, prw, prt_table, prw_table, thrmin, shift_amount);

    write(outIndex, wr_h_pos, wr_v_pos+8, out);
}

_GENX_ void inline
COMP_ACC0_ACC1(matrix_ref<ushort, 8,  16> premask, matrix_ref<uchar , 12, 32> src,
               matrix_ref<uchar , 16, 16> acc0   , matrix_ref<uchar , 16, 16> acc1,
               int y, int x, int y_offset)
{
    matrix< ushort, 8, 16 > diff, pixmask;

    pixmask = premask | (src.select<8,1,16,1>(y,x) <= GOOD_INTENSITY_TH);
    diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,16,1>(y,x), - src.select<8,1,16,1>(2,2)));
    diff.merge(GOOD_PIXEL_TH, pixmask);

    acc0.select<8,1,16,1>(y_offset,0) += (diff < GOOD_PIXEL_TH);    //(diff > GOOD_PIXEL_TH);
    acc1.select<8,1,16,1>(y_offset,0) += (diff > (GOOD_PIXEL_TH*8));
}
#define COMPRESS_BUF 1
extern "C" _GENX_MAIN_ void
GOOD_PIXEL_CHECK(SurfaceIndex ibuf ,
                 SurfaceIndex GSwap_bitshifted_buf,
                 SurfaceIndex obuf0, SurfaceIndex obuf1,
                 int shift, int outframeheight,
                 int firstflag, int bitDepth, int BayerType)
{
    matrix<ushort, 20, 32 >   rd_in;
    matrix<uchar , 12, 32 >   src;
    matrix<ushort,  8, 16 >   diff, pixmask, premask;
    matrix<uchar , 16, 16 >   acc0(0);
    matrix<uchar , 16, 16 >   acc1(0);

    int rd_h_pos = (get_thread_origin_x() * 16 - 2) * sizeof(short);
    int rd_v_pos = (get_thread_origin_y() * 16 - 2) ;
    int wr_h_pos = (get_thread_origin_x() * 16    ) ;
    int wr_v_pos = (get_thread_origin_y() * 16    ) ;
    int write_v_pos = (BayerType == GRBG || BayerType == GBRG)? ((outframeheight - wr_v_pos) - 16) : wr_v_pos;

    read(ibuf, rd_h_pos   , rd_v_pos   , rd_in.select<8,1,16,1>(0, 0 ));
    read(ibuf, rd_h_pos+32, rd_v_pos   , rd_in.select<8,1,16,1>(0, 16));
    read(ibuf, rd_h_pos   , rd_v_pos+8 , rd_in.select<8,1,16,1>(8, 0 ));
    read(ibuf, rd_h_pos+32, rd_v_pos+8 , rd_in.select<8,1,16,1>(8, 16));
    read(ibuf, rd_h_pos   , rd_v_pos+16, rd_in.select<4,1,16,1>(16,0 ));
    read(ibuf, rd_h_pos+32, rd_v_pos+16, rd_in.select<4,1,16,1>(16,16));

    if(firstflag == 1)
    {
        if(BayerType == GRBG || BayerType == GBRG)
            ROWSWAP_20x32(rd_in);

        rd_in >>= (unsigned short)(16-bitDepth);
        write(GSwap_bitshifted_buf, (wr_h_pos*2), write_v_pos  , rd_in.select<8,1,16,1>(2 ,2));  // center 16 rows
        write(GSwap_bitshifted_buf, (wr_h_pos*2), write_v_pos+8, rd_in.select<8,1,16,1>(10,2));  // center 16 rows
    }
    else
    {
        // rd_in is in format of either RGGB or BGGR (already flipped by previous kernel
    }

    ///////////   1ST 8x16 BLOCK  /////////////////////////////////////////////////////////////////////
    ///// ACCORDING TO B-SPEC: Precision: 16-bit pixels truncated to 8-bits before subtract & compares.
    src     = (rd_in.select<12,1,32,1>(0,0) >> (unsigned short)(shift));
    premask = (src.select<8,1,16,1>(2,2) <= GOOD_INTENSITY_TH);  // center 8 rows

    COMP_ACC0_ACC1(premask, src, acc0, acc1, 0, 2, 0);
    COMP_ACC0_ACC1(premask, src, acc0, acc1, 2, 0, 0);
    COMP_ACC0_ACC1(premask, src, acc0, acc1, 4, 2, 0);
    COMP_ACC0_ACC1(premask, src, acc0, acc1, 2, 4, 0);

    ///////////   2ND 8x16 BLOCK  /////////////////////////////////////////////////////////////////////
    ///// ACCORDING TO B-SPEC: Precision: 16-bit pixels truncated to 8-bits before subtract & compares.
    src     = (rd_in.select<12,1,32,1>(8,0) >> (unsigned short)(shift));
    premask = (src.select<8,1,16,1>(2,2) <= GOOD_INTENSITY_TH);  // center 8 rows

    COMP_ACC0_ACC1(premask, src, acc0, acc1, 0, 2, 8);
    COMP_ACC0_ACC1(premask, src, acc0, acc1, 2, 0, 8);
    COMP_ACC0_ACC1(premask, src, acc0, acc1, 4, 2, 8);
    COMP_ACC0_ACC1(premask, src, acc0, acc1, 2, 4, 8);

    /////////////  WRITE OUT  //////////////////////////////////////////////////////////////////////////
    matrix<uchar, 16, 8> out0, out1;

#if COMPRESS_BUF
    out0  = acc0.select<16, 1, 8, 2>(0,0) << 4;
    out0 += acc0.select<16, 1, 8, 2>(0,1);
    out1  = acc1.select<16, 1, 8, 2>(0,0) << 4;
    out1 += acc1.select<16, 1, 8, 2>(0,1);

    write(obuf0, (wr_h_pos >> 1), wr_v_pos, out0);
    write(obuf1, (wr_h_pos >> 1), wr_v_pos, out1);
#else
    write(obuf0, (wr_h_pos )    , write_v_pos, acc0);
    write(obuf1, (wr_h_pos )    , write_v_pos, acc1);
#endif

}

const unsigned short spatial_mask_init[8][8]
= {{0,1,0,1,0,1,0,1},{1,0,1,0,1,0,1,0},{0,1,0,1,0,1,0,1},{1,0,1,0,1,0,1,0},
   {0,1,0,1,0,1,0,1},{1,0,1,0,1,0,1,0},{0,1,0,1,0,1,0,1},{1,0,1,0,1,0,1,0}};

const signed short coef_init_0[8] = {-1,5,2,3,-1,0,0,0};
const signed short coef_init_1[8] = {-1,3,2,5,-1,0,0,0};


extern "C" _GENX_MAIN_ void
RESTOREG(SurfaceIndex ibufBayer,
         SurfaceIndex ibuf0, SurfaceIndex ibuf1,
         SurfaceIndex obufHOR, SurfaceIndex obufVER, SurfaceIndex obufAVG,
         SurfaceIndex obuf, int x , int y, int wr_x, int wr_y, ushort max_intensity,
         int BayerType)
{
    matrix< uchar, 12, 16 >   src0;
    matrix< uchar, 12, 16 >   src1;
    matrix< uchar, 8,  8  >   dst ;
    matrix<ushort, 8,  8  >   mask;
    matrix<ushort, 8,  8  >   acc0(0);
    matrix<ushort, 8,  8  >   acc1(0);

    int rd_h_pos = ((x + get_thread_origin_x()) * 8 - 2);
    int rd_v_pos = ((y + get_thread_origin_y()) * 8 - 2);

#if COMPRESS_BUF
    matrix< uchar, 12, 8 > rd0, rd1;

    read(ibuf0, rd_h_pos >> 1, rd_v_pos, rd0);
    read(ibuf1, rd_h_pos >> 1, rd_v_pos, rd1);

    src0.select<12, 1, 8, 2>(0,0)   = rd0 & 0xF0;
    src0.select<12, 1, 8, 2>(0,0) >>= 4         ;
    src0.select<12, 1, 8, 2>(0,1)   = rd0 & 0x0F;
    src1.select<12, 1, 8, 2>(0,0)   = rd1 & 0xF0;
    src1.select<12, 1, 8, 2>(0,0) >>= 4         ;
    src1.select<12, 1, 8, 2>(0,1)   = rd1 & 0x0F;
#else
    read(ibuf0, rd_h_pos, rd_v_pos, src0);
    read(ibuf1, rd_h_pos, rd_v_pos, src1);
#endif

#pragma unroll
    for(int i = 1; i <= 3; i++) {
#pragma unroll
        for(int j = 1; j <= 3; j++) {
            acc0 += src0.select<8,1,8,1>(i,j);
            acc1 += src1.select<8,1,8,1>(i,j);
        }
    }

    mask = (acc0 > NUM_GOOD_PIXEL_TH) | (acc1 > NUM_BIG_PIXEL_TH);
    dst.merge(2, 1, mask);

    ///////////////////////////////////////////////////////////
    int wr_h_pos = ((get_thread_origin_x() + wr_x) * 8 );
    int wr_v_pos = ((get_thread_origin_y() + wr_y) * 8 );

    write(obuf, wr_h_pos, wr_v_pos, dst);

    /////////////////////////////////////////////////////////////////////////////////
    ///////////    COMPUTE GREEN COMPONENTS  ////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////

    matrix<ushort, 12,16> srcBayer;
    matrix<ushort, 8, 8 > G_H, G_V, G_A;

    wr_h_pos = ((get_thread_origin_x() + wr_x) * 8 ) * sizeof(short);
    wr_v_pos = ((get_thread_origin_y() + wr_y) * 8 );

    rd_h_pos = ((get_thread_origin_x() + x) * 8 - 2) * sizeof(short);

    read(ibufBayer, rd_h_pos, rd_v_pos  , srcBayer.select<8,1,16,1>(0,0));
    read(ibufBayer, rd_h_pos, rd_v_pos+8, srcBayer.select<4,1,16,1>(8,0));

    matrix<ushort, 8, 8> M;
    matrix< int, 8, 8> SUM0;
    vector< int, 8 > coef_0(coef_init_0);
    vector< int, 8 > coef_1(coef_init_1);
    matrix<ushort, 8, 8> spatial_mask(spatial_mask_init);

    matrix<short, 8, 8> coef;
    matrix<int, 8, 8> tmp0, tmp1;//matrix<short, 8, 8> tmp0, tmp1;

    /////////////  COMPUTE GREEN VERTICAL COMPONENT /////////////////////////////////

    M = (cm_abs<uint>(cm_add<int>(srcBayer.select<8,1,8,1>(0,2), -srcBayer.select<8,1,8,1>(2,2))) <
         cm_abs<uint>(cm_add<int>(srcBayer.select<8,1,8,1>(2,2), -srcBayer.select<8,1,8,1>(4,2))));

    tmp0 = srcBayer.select<8,1,8,1>(2, 2) - srcBayer.select<8,1,8,1>(0, 2);
    tmp1 = srcBayer.select<8,1,8,1>(2, 2) - srcBayer.select<8,1,8,1>(4, 2);
    SUM0 = tmp0 + tmp1;

    coef.merge(5, 3, M);
    SUM0 += cm_mul<int>(srcBayer.select<8,1,8,1>(1, 2), coef);

    coef.merge(3, 5, M);
    SUM0 += cm_mul<int>(srcBayer.select<8,1,8,1>(3, 2), coef);

    SUM0 >>= 3;
    SUM0 = cm_max<int>(SUM0, 0  );
    G_V  = cm_min<int>(SUM0, max_intensity);
    G_V.merge(srcBayer.select<8,1,8,1>(2,2), spatial_mask);

    /////////////  COMPUTE GREEN HORIZONTAL COMPONENT /////////////////////////////////

    M = (cm_abs<uint>(cm_add<int>(srcBayer.select<8,1,8,1>(2,0), -srcBayer.select<8,1,8,1>(2,2))) <
         cm_abs<uint>(cm_add<int>(srcBayer.select<8,1,8,1>(2,2), -srcBayer.select<8,1,8,1>(2,4))));

    tmp0 = srcBayer.select<8,1,8,1>(2, 2) - srcBayer.select<8,1,8,1>(2, 0);
    tmp1 = srcBayer.select<8,1,8,1>(2, 2) - srcBayer.select<8,1,8,1>(2, 4);
    SUM0 = tmp0 + tmp1;

    coef.merge(5, 3, M);
    SUM0 += cm_mul<int>(srcBayer.select<8,1,8,1>(2, 1), coef);

    coef.merge(3, 5, M);
    SUM0 += cm_mul<int>(srcBayer.select<8,1,8,1>(2, 3), coef);

    SUM0 >>= 3;
    SUM0 = cm_max<int>(SUM0, 0  );
    G_H  = cm_min<int>(SUM0, max_intensity);
    G_H.merge(srcBayer.select<8,1,8,1>(2,2), spatial_mask);

    /////////////  COMPUTE GREEN AVERAGE COMPONENT /////////////////////////////////

    matrix_ref<uint, 8, 8> tmpA = tmp0.format<uint, 8, 8>(); //matrix_ref<ushort, 8, 8> tmpA = tmp0.format<ushort, 8, 8>();
    matrix_ref<uint, 8, 8> tmpB = tmp1.format<uint, 8, 8>(); //matrix_ref<ushort, 8, 8> tmpB = tmp1.format<ushort, 8, 8>();

    matrix<int, 8, 8> AVG_R;
    matrix_ref<int, 8, 8> AVG_G = SUM0;
    matrix_ref<int, 8, 8> TMP_S = AVG_R;

    tmpA  = srcBayer.select<8,1,8,1>(0,1)        + srcBayer.select<8,1,8,1>(0,3);
    tmpA += srcBayer.select<8,1,8,1>(1,0); tmpA += srcBayer.select<8,1,8,1>(1,4);
    tmpB  = srcBayer.select<8,1,8,1>(3,0); tmpB += srcBayer.select<8,1,8,1>(3,4);
    tmpB += srcBayer.select<8,1,8,1>(4,1); tmpB += srcBayer.select<8,1,8,1>(4,3);
    AVG_G = tmpA + tmpB;

    tmpA  = srcBayer.select<8,1,8,1>(1,2)         + srcBayer.select<8,1,8,1>(2,1);
    tmpA += srcBayer.select<8,1,8,1>(2,3); TMP_S = tmpA + srcBayer.select<8,1,8,1>(3,2);

    AVG_G += cm_mul<int>(TMP_S, 2);
    AVG_G  = AVG_G >> 4;

    tmpA  = srcBayer.select<8,1,8,1>(0,2)        + srcBayer.select<8,1,8,1>(0,4);
    tmpA += srcBayer.select<8,1,8,1>(2,0); tmpA += srcBayer.select<8,1,8,1>(2,2);
    tmpB  = srcBayer.select<8,1,8,1>(2,4); tmpB += srcBayer.select<8,1,8,1>(4,0);
    tmpB += srcBayer.select<8,1,8,1>(4,2); tmpB += srcBayer.select<8,1,8,1>(4,4);
    AVG_R = tmpA + tmpB;

    AVG_R  = AVG_R >> 3;

    AVG_G += srcBayer.select<8,1,8,1>(2,2);
    AVG_G -= AVG_R;

    AVG_G = cm_max<int>(AVG_G, 0);
    AVG_G = cm_min<int>(AVG_G, max_intensity);

    G_A.merge(srcBayer.select<8,1,8,1>(2,2), AVG_G, spatial_mask);

    ///////////////////////////////////////////////////////////

    write(obufAVG, wr_h_pos, wr_v_pos, G_A);
    write(obufHOR, wr_h_pos, wr_v_pos, G_H);
    write(obufVER, wr_h_pos, wr_v_pos, G_V);

}

#define D 16

extern "C" _GENX_MAIN_ void
RESTOREBandR(SurfaceIndex ibufBayer,
             SurfaceIndex ibufGHOR, SurfaceIndex ibufGVER, SurfaceIndex ibufGAVG,
             SurfaceIndex obufBHOR, SurfaceIndex obufBVER, SurfaceIndex obufBAVG,
             SurfaceIndex obufRHOR, SurfaceIndex obufRVER, SurfaceIndex obufRAVG,
             SurfaceIndex ibufAVGMASK,
             int x , int y, int wr_x, int wr_y, ushort max_intensity,
             int BayerType)
{
    int rd_h_pos = ((get_thread_origin_x() + x) * 8 - 2) * sizeof(short);
    int rd_v_pos = ((get_thread_origin_y() + y) * 8 - 2) ;

    int wr_h_pos = ((get_thread_origin_x() + x) * 8    ) * sizeof(short);
    int wr_v_pos = ((get_thread_origin_y() + y) * 8    ) ;

    ////// ADDED TO TEST ///////
    int rd_x_pos = ((get_thread_origin_x() + wr_x) * 8 - 2) * sizeof(short);
    int rd_y_pos = ((get_thread_origin_y() + wr_y) * 8 - 2) ;

    matrix<ushort, 12, 16> bayer;
    read(ibufBayer, rd_x_pos, rd_y_pos  , bayer.select<8,1,16,1>(0,0));
    read(ibufBayer, rd_x_pos, rd_y_pos+8, bayer.select<4,1,16,1>(8,0));

    matrix<ushort, 8, 8> out_B, out_R;
    out_B.select<4,2,4,2>(0,0) = bayer .select<4,2,4,2>(2,2);
    out_R.select<4,2,4,2>(1,1) = bayer .select<4,2,4,2>(3,3);

    matrix<int, 12, 8> BR34_c; // 12 registers
    matrix<int, 8,    D> BR32_c; // 12 registers
    matrix<int, 8,  8> BR33_c; //  8 registers

    matrix_ref<ushort, 8,  D> BR32_s = BR32_c.select<4,1,D,1>(0,0).format<ushort, 8, D>(); //matrix<ushort, 8,12> BR32_s;
    matrix_ref<ushort, 12, 8> BR34_s = BR34_c.select<6,1,8,1>(0,0).format<ushort,12, 8>(); //matrix<ushort,12, 8> BR34_s;
    matrix_ref<ushort, 8,  8> BR33_s = BR33_c.select<4,1,8,1>(0,0).format<ushort, 8, 8>(); //matrix<ushort, 8, 8> BR33_s;

    //matrix<short, 12, 16> dBG_c, Green_c, G_c_x2;
    matrix<   int, 12, 16> dBG_c;    // 12 registers
    matrix<ushort, 12, 16> Green_c;  // 12 registers
    //matrix< short, 12, 16> G_c_x2; // 12 registers

    ////////////////////////////////////////////////////////////////////////////////////////////
    /////// HORIZONTAL COMPONENT ///////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////

    read(ibufGHOR, rd_h_pos, rd_v_pos  , Green_c.select<8,1,16,1>(0,0));
    read(ibufGHOR, rd_h_pos, rd_v_pos+8, Green_c.select<4,1,16,1>(8,0));

    dBG_c = bayer - Green_c;

    BR32_c  = cm_mul<int>(Green_c.select<8,1,D,1>(2,0), 2);
    BR32_c += bayer.select<8,1,D,1>(1,0);
    BR32_c -= Green_c.select<8,1,D,1>(1,0);
    BR32_c += bayer.select<8,1,D,1>(3,0);
    BR32_c -= Green_c.select<8,1,D,1>(3,0);
    BR32_c >>= 1;
    BR32_c = cm_max<int>(BR32_c, 0);
    BR32_s = cm_min<int>(BR32_c, max_intensity);
    out_B.select<4,2,4,2>(1,0) = BR32_s.select<4,2,4,2>(1,2);
    out_R.select<4,2,4,2>(0,1) = BR32_s.select<4,2,4,2>(0,3);

    BR34_c  = cm_mul<int>(Green_c.select<12,1,8,1>(0,2), 2);
    BR34_c += bayer.select<12,1,8,1>(0,1);
    BR34_c -= Green_c.select<12,1,8,1>(0,1);
    BR34_c += bayer.select<12,1,8,1>(0,3);
    BR34_c -= Green_c.select<12,1,8,1>(0,3);
    BR34_c >>= 1;
    BR34_c = cm_max<int>(BR34_c, 0);
    BR34_s = cm_min<int>(BR34_c, max_intensity);
    out_B.select<4,2,4,2>(0,1) = BR34_s.select<4,2,4,2>(2,1);
    out_R.select<4,2,4,2>(1,0) = BR34_s.select<4,2,4,2>(3,0);

    BR33_c  = cm_mul<int>(Green_c.select<8,1,8,1>(2,2), 2);
    BR33_c += BR32_s .select<8,1,8,1>(0,1);
    BR33_c -= Green_c.select<8,1,8,1>(2,1);
    BR33_c += BR32_s .select<8,1,8,1>(0,3);
    BR33_c -= Green_c.select<8,1,8,1>(2,3);
    BR33_c >>= 1;
    BR33_c = cm_max<int>(BR33_c, 0);
    BR33_s = cm_min<int>(BR33_c, max_intensity);
    out_B.select<4,2,4,2>(1,1) = BR33_s.select<4,2,4,2>(1,1);
    out_R.select<4,2,4,2>(0,0) = BR33_s.select<4,2,4,2>(0,0);

    write(obufBHOR, wr_h_pos, wr_v_pos, out_B);
    write(obufRHOR, wr_h_pos, wr_v_pos, out_R);

    ////////////////////////////////////////////////////////////////////////////////////////////
    /////// VERTICAL COMPONENT /////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////

    read(ibufGVER, rd_h_pos, rd_v_pos  , Green_c.select<8,1,16,1>(0,0));
    read(ibufGVER, rd_h_pos, rd_v_pos+8, Green_c.select<4,1,16,1>(8,0));

    dBG_c = bayer - Green_c;

    BR32_c  = cm_mul<int>(Green_c.select<8,1,D,1>(2,0), 2);
    BR32_c += bayer.select<8,1,D,1>(1,0);
    BR32_c -= Green_c.select<8,1,D,1>(1,0);
    BR32_c += bayer.select<8,1,D,1>(3,0);
    BR32_c -= Green_c.select<8,1,D,1>(3,0);
    BR32_c >>= 1;
    BR32_c.select<8,1,8,1>(0,2) = cm_max<int>(BR32_c.select<8,1,8,1>(0,2), 0);
    BR32_s = cm_min<int>(BR32_c, max_intensity);
    out_B.select<4,2,4,2>(1,0) = BR32_s.select<4,2,4,2>(1,2);
    out_R.select<4,2,4,2>(0,1) = BR32_s.select<4,2,4,2>(0,3);

    BR34_c  = cm_mul<int>(Green_c.select<12,1,8,1>(0,2), 2);
    BR34_c += bayer.select<12,1,8,1>(0,1);
    BR34_c -= Green_c.select<12,1,8,1>(0,1);
    BR34_c += bayer.select<12,1,8,1>(0,3);
    BR34_c -= Green_c.select<12,1,8,1>(0,3);
    BR34_c >>= 1;
    BR34_c = cm_max<int>(BR34_c, 0);
    BR34_s = cm_min<int>(BR34_c, max_intensity);
    out_B.select<4,2,4,2>(0,1) = BR34_s.select<4,2,4,2>(2,1);
    out_R.select<4,2,4,2>(1,0) = BR34_s.select<4,2,4,2>(3,0);

    BR33_c  = cm_mul<int>(Green_c.select<8,1,8,1>(2,2), 2);
    BR33_c += BR34_s .select<8,1,8,1>(1,0);
    BR33_c -= Green_c.select<8,1,8,1>(1,2);
    BR33_c += BR34_s .select<8,1,8,1>(3,0);
    BR33_c -= Green_c.select<8,1,8,1>(3,2);
    BR33_c >>= 1;
    BR33_c = cm_max<int>(BR33_c, 0);
    BR33_s = cm_min<int>(BR33_c, max_intensity);
    out_B.select<4,2,4,2>(1,1) = BR33_s.select<4,2,4,2>(1,1);
    out_R.select<4,2,4,2>(0,0) = BR33_s.select<4,2,4,2>(0,0);

    write(obufBVER, wr_h_pos, wr_v_pos, out_B);
    write(obufRVER, wr_h_pos, wr_v_pos, out_R);

    ////////////////////////////////////////////////////////////////////////////////////////////
    /////// AVERAGE COMPONENT //////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////////////////
    matrix_ref<uchar, 8, 8> avg_mask = dBG_c.select<2,1,16,1>(0,0).format<uchar, 8, 8>();
    read(ibufAVGMASK, wr_h_pos / 2, wr_v_pos, avg_mask);

    if((avg_mask == AVG_True).any())
    {
        read(ibufGAVG, rd_h_pos, rd_v_pos  , Green_c.select<8,1,16,1>(0,0));
        read(ibufGAVG, rd_h_pos, rd_v_pos+8, Green_c.select<4,1,16,1>(8,0));

        dBG_c = bayer - Green_c;

        BR32_c  = cm_mul<int>(Green_c.select<8,1,D,1>(2,0), 2);
        BR32_c += bayer.select< 8,1,D,1>(1,0);
        BR32_c -= Green_c.select< 8,1,D,1>(1,0);
        BR32_c += bayer.select< 8,1,D,1>(3,0);
        BR32_c -= Green_c.select< 8,1,D,1>(3,0);
        BR32_c >>= 1;
        BR32_c = cm_max<int>(BR32_c, 0);
        BR32_s = cm_min<int>(BR32_c, max_intensity);
        out_B.select<4,2,4,2>(1,0) = BR32_s.select<4,2,4,2>(1,2);
        out_R.select<4,2,4,2>(0,1) = BR32_s.select<4,2,4,2>(0,3);

        BR34_c  = cm_mul<int>(Green_c.select<12,1,8,1>(0,2), 2);
        BR34_c += bayer.select<12,1,8,1>(0,1);
        BR34_c -= Green_c.select<12,1,8,1>(0,1);
        BR34_c += bayer.select<12,1,8,1>(0,3);
        BR34_c -= Green_c.select<12,1,8,1>(0,3);
        BR34_c >>= 1;
        BR34_c = cm_max<int>(BR34_c, 0);
        BR34_s = cm_min<int>(BR34_c, max_intensity);
        out_B.select<4,2,4,2>(0,1) = BR34_s.select<4,2,4,2>(2,1);
        out_R.select<4,2,4,2>(1,0) = BR34_s.select<4,2,4,2>(3,0);

        BR33_c  = cm_mul<int>(Green_c.select<8,1,8,1>(2,2), 4);
        BR33_c += BR34_s .select<8,1,8,1>(1,0);
        BR33_c -= Green_c.select<8,1,8,1>(1,2);
        BR33_c += BR34_s .select<8,1,8,1>(3,0);
        BR33_c -= Green_c.select<8,1,8,1>(3,2);
        BR33_c += BR32_s .select<8,1,8,1>(0,1);
        BR33_c -= Green_c.select<8,1,8,1>(2,1);
        BR33_c += BR32_s .select<8,1,8,1>(0,3);
        BR33_c -= Green_c.select<8,1,8,1>(2,3);
        BR33_c >>= 2;
        BR33_c = cm_max<int>(BR33_c, 0);
        BR33_s = cm_min<int>(BR33_c, max_intensity);
        out_B.select<4,2,4,2>(1,1) = BR33_s.select<4,2,4,2>(1,1);
        out_R.select<4,2,4,2>(0,0) = BR33_s.select<4,2,4,2>(0,0);


        write(obufBAVG, wr_h_pos, wr_v_pos, out_B);
        write(obufRAVG, wr_h_pos, wr_v_pos, out_R);
    }

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////     SAD     //////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BITDEPTH 16 VERSION OF SAD KERNEL
extern "C" _GENX_MAIN_ void
SAD_16(SurfaceIndex ibufRHOR, SurfaceIndex ibufGHOR, SurfaceIndex ibufBHOR,
       SurfaceIndex ibufRVER, SurfaceIndex ibufGVER, SurfaceIndex ibufBVER,
        int x , int y, int wr_x, int wr_y, int BayerType,
       SurfaceIndex obufROUT ,SurfaceIndex obufGOUT, SurfaceIndex obufBOUT)
{
    int rd_h_pos = ((get_thread_origin_x() + x) * 8 + 8) * sizeof(short);
    int rd_v_pos = ((get_thread_origin_y() + y) * 8 + 8) ;
    int bitshift = 2;
    //bitshift = 0; // Assume BitDepth = 16;
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////    PART I: COMPUTE MIN_COST_H   ////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    matrix<short, 16, 16> in, R;
    matrix<ushort, 8, 8> R_H, G_H, B_H;
    matrix<short, 16, 16> tmp;

    matrix<ushort, 8, 16> HDSUM_H;
    matrix<ushort, 16, 8> VDSUM_H;

    read(ibufRHOR, rd_h_pos - 8, rd_v_pos - 4, tmp.select<8,1,16,1>(0,0));  // tmp  == R
    read(ibufRHOR, rd_h_pos - 8, rd_v_pos + 4, tmp.select<8,1,16,1>(8,0));  // tmp  == R
    R_H = tmp.select<8,1,8,1>(4,4);
    tmp >>= bitshift;                                                       // <- bitshift
    R = tmp;

    read(ibufGHOR, rd_h_pos - 8, rd_v_pos - 4, in .select<8,1,16,1>(0,0));  // in   == G
    read(ibufGHOR, rd_h_pos - 8, rd_v_pos + 4, in .select<8,1,16,1>(8,0));  // in   == G
    G_H = in .select<8,1,8,1>(4,4);
    in >>= bitshift;                                                        // <- bitshift
    tmp -= in;                                                              //tmp  == DRG

    HDSUM_H.select<8,1,W,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<8,1,W,1>(4,1), - tmp.select<8,1,W,1>(4,0)));
    VDSUM_H.select<W,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<W,1,8,1>(1,4), - tmp.select<W,1,8,1>(0,4)));

    read(ibufBHOR, rd_h_pos - 8, rd_v_pos - 4, tmp .select<8,1,16,1>(0,0)); // tmp == B
    read(ibufBHOR, rd_h_pos - 8, rd_v_pos + 4, tmp .select<8,1,16,1>(8,0)); // tmp == B
    B_H = tmp.select<8,1,8,1>(4,4);
    tmp >>= bitshift;                                                       // <- bitshift
    in -= tmp;                                                              //  in == DGB

    HDSUM_H.select<8,1,W,1>(0,0) += cm_abs<ushort>(cm_add<short>(in.select<8,1,W,1>(4,1), - in.select<8,1,W,1>(4,0)));
    VDSUM_H.select<W,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(in.select<W,1,8,1>(1,4), - in.select<W,1,8,1>(0,4)));

    tmp -= R;                                                               // tmp == DBR

    HDSUM_H.select<8,1,W,1>(0,0) += cm_abs<ushort>(cm_add<short>(tmp.select<8,1,W,1>(4,1), - tmp.select<8,1,W,1>(4,0)));
    VDSUM_H.select<W,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(tmp.select<W,1,8,1>(1,4), - tmp.select<W,1,8,1>(0,4)));

    matrix_ref<int, 8, 8> HLSAD = tmp.select<8,1,16,1>(0,0).format<int, 8, 8>();
    matrix_ref<int, 8, 8> HRSAD = tmp.select<8,1,16,1>(8,0).format<int, 8, 8>();

    HLSAD  = HDSUM_H.select<8,1,8,1>(0,0);  HRSAD  = HDSUM_H.select<8,1,8,1>(0,4);
    HLSAD += HDSUM_H.select<8,1,8,1>(0,1);  HRSAD += HDSUM_H.select<8,1,8,1>(0,5);
    HLSAD += HDSUM_H.select<8,1,8,1>(0,2);  HRSAD += HDSUM_H.select<8,1,8,1>(0,6);
    HLSAD += HDSUM_H.select<8,1,8,1>(0,3);  HRSAD += HDSUM_H.select<8,1,8,1>(0,7);

    matrix_ref<int, 8, 8> Min_cost_H1 = in.select<8,1,16,1>(0,0).format<int, 8, 8>();
    Min_cost_H1 = cm_min<int>(HLSAD, HRSAD);

    matrix_ref<int, 8, 8> HUSAD = tmp.select<8,1,16,1>(0,0).format<int, 8, 8>();
    matrix_ref<int, 8, 8> HDSAD = tmp.select<8,1,16,1>(8,0).format<int, 8, 8>();

    HUSAD  = VDSUM_H.select<8,1,8,1>(0,0);  HDSAD  = VDSUM_H.select<8,1,8,1>(4,0);
    HUSAD += VDSUM_H.select<8,1,8,1>(1,0);  HDSAD += VDSUM_H.select<8,1,8,1>(5,0);
    HUSAD += VDSUM_H.select<8,1,8,1>(2,0);  HDSAD += VDSUM_H.select<8,1,8,1>(6,0);
    HUSAD += VDSUM_H.select<8,1,8,1>(3,0);  HDSAD += VDSUM_H.select<8,1,8,1>(7,0);

    matrix_ref<int, 8, 8> Min_cost_H2 = in.select<8,1,16,1>(8,0).format<int, 8, 8>();
    Min_cost_H2 = cm_min<int>(HUSAD, HDSAD);

    matrix<int, 8, 8> Min_cost_H = Min_cost_H1;
    Min_cost_H  = Min_cost_H2 >> SHIFT_MINCOST;
    Min_cost_H += Min_cost_H1                  ;


    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////    PART II: COMPUTE MIN_COST_V  ////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    matrix<ushort, 8, 8> R_V, G_V, B_V;
    matrix_ref<ushort, 8, 16> HDSUM_V = HDSUM_H;
    matrix_ref<ushort, 16, 8> VDSUM_V = VDSUM_H;

    read(ibufRVER, rd_h_pos - 8, rd_v_pos - 4, tmp.select<8,1,16,1>(0,0)); // tmp  == R
    read(ibufRVER, rd_h_pos - 8, rd_v_pos + 4, tmp.select<8,1,16,1>(8,0)); // tmp  == R
    R_V = tmp.select<8,1,8,1>(4,4);
    R = tmp >> bitshift;

    read(ibufGVER, rd_h_pos - 8, rd_v_pos - 4, in .select<8,1,16,1>(0,0)); // in   == G
    read(ibufGVER, rd_h_pos - 8, rd_v_pos + 4, in .select<8,1,16,1>(8,0)); // in   == G
    G_V = in .select<8,1,8,1>(4,4);
    in >>= bitshift;                                                       // <- bitshift
    tmp = (R - in);                                                        // tmp  == DRG

    HDSUM_V.select<8,1,W,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<8,1,W,1>(4,1), - tmp.select<8,1,W,1>(4,0)));
    VDSUM_V.select<W,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<W,1,8,1>(1,4), - tmp.select<W,1,8,1>(0,4)));

    read(ibufBVER, rd_h_pos - 8, rd_v_pos - 4, tmp .select<8,1,16,1>(0,0)); // tmp == B
    read(ibufBVER, rd_h_pos - 8, rd_v_pos + 4, tmp .select<8,1,16,1>(8,0)); // tmp == B
    B_V = tmp.select<8,1,8,1>(4,4);
    tmp >>= bitshift;                                                       //<- bitshift
    in -= tmp;                                                              //  in == DGB

    HDSUM_V.select<8,1,W,1>(0,0) += cm_abs<ushort>(cm_add<short>(in.select<8,1,W,1>(4,1), - in.select<8,1,W,1>(4,0)));
    VDSUM_V.select<W,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(in.select<W,1,8,1>(1,4), - in.select<W,1,8,1>(0,4)));

    tmp -= R;                                                                // tmp == DBR

    HDSUM_V.select<8,1,W,1>(0,0) += cm_abs<ushort>(cm_add<short>(tmp.select<8,1,W,1>(4,1), - tmp.select<8,1,W,1>(4,0)));
    VDSUM_V.select<W,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(tmp.select<W,1,8,1>(1,4), - tmp.select<W,1,8,1>(0,4)));

    matrix_ref<int, 8, 8> VLSAD = tmp.select<8,1,16,1>(0,0).format<int, 8, 8>();
    matrix_ref<int, 8, 8> VRSAD = tmp.select<8,1,16,1>(8,0).format<int, 8, 8>();

    VLSAD  = HDSUM_V.select<8,1,8,1>(0,0);  VRSAD  = HDSUM_V.select<8,1,8,1>(0,4);
    VLSAD += HDSUM_V.select<8,1,8,1>(0,1);  VRSAD += HDSUM_V.select<8,1,8,1>(0,5);
    VLSAD += HDSUM_V.select<8,1,8,1>(0,2);  VRSAD += HDSUM_V.select<8,1,8,1>(0,6);
    VLSAD += HDSUM_V.select<8,1,8,1>(0,3);  VRSAD += HDSUM_V.select<8,1,8,1>(0,7);

    matrix_ref<int, 8, 8> Min_cost_V2 = in.select<8,1,16,1>(0,0).format<int, 8, 8>();
    Min_cost_V2 = cm_min<int>(VLSAD, VRSAD);

    matrix_ref<int, 8, 8> VUSAD = tmp.select<8,1,16,1>(0,0).format<int, 8, 8>();
    matrix_ref<int, 8, 8> VDSAD = tmp.select<8,1,16,1>(8,0).format<int, 8, 8>();

    VUSAD  = VDSUM_V.select<8,1,8,1>(0,0);  VDSAD  = VDSUM_V.select<8,1,8,1>(4,0);
    VUSAD += VDSUM_V.select<8,1,8,1>(1,0);  VDSAD += VDSUM_V.select<8,1,8,1>(5,0);
    VUSAD += VDSUM_V.select<8,1,8,1>(2,0);  VDSAD += VDSUM_V.select<8,1,8,1>(6,0);
    VUSAD += VDSUM_V.select<8,1,8,1>(3,0);  VDSAD += VDSUM_V.select<8,1,8,1>(7,0);

    matrix_ref<int, 8, 8> Min_cost_V1 = in.select<8,1,16,1>(8,0).format<int, 8, 8>();
    Min_cost_V1 = cm_min<int>(VUSAD, VDSAD);

    matrix<int, 8, 8> Min_cost_V = Min_cost_V1;
    Min_cost_V  = Min_cost_V2 >> SHIFT_MINCOST;
    Min_cost_V += Min_cost_V1                  ;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////// PART III: COMPUTE FLAG(MIN_COST_H, MIN_COST_V)   ///////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    int wr_h_pos = ((get_thread_origin_x() + wr_x) * 8) * sizeof(short);
    int wr_v_pos = ((get_thread_origin_y() + wr_y) * 8) ;

    matrix_ref<ushort, 8, 8> outR = tmp.select<4,1,16,1>(0,0) .format<ushort, 8, 8>();
    matrix_ref<ushort, 8, 8> outG = tmp.select<4,1,16,1>(4,0) .format<ushort, 8, 8>();
    matrix_ref<ushort, 8, 8> outB = tmp.select<4,1,16,1>(8,0) .format<ushort, 8, 8>();

    outR.merge(R_H, R_V, (Min_cost_H < Min_cost_V));
    outG.merge(G_H, G_V, (Min_cost_H < Min_cost_V));
    outB.merge(B_H, B_V, (Min_cost_H < Min_cost_V));

    write(obufROUT, wr_h_pos, wr_v_pos, outR);
    write(obufGOUT, wr_h_pos, wr_v_pos, outG);
    write(obufBOUT, wr_h_pos, wr_v_pos, outB);
}

// BITDEPTH 14 VERSION OF SAD KERNEL
extern "C" _GENX_MAIN_ void
SAD(SurfaceIndex ibufRHOR, SurfaceIndex ibufGHOR, SurfaceIndex ibufBHOR,
    SurfaceIndex ibufRVER, SurfaceIndex ibufGVER, SurfaceIndex ibufBVER,
    int x , int y, int wr_x, int wr_y, int BayerType,
    SurfaceIndex obufROUT ,SurfaceIndex obufGOUT, SurfaceIndex obufBOUT)
{
    int rd_h_pos = ((get_thread_origin_x() + x) * 8 + 8) * sizeof(short);
    int rd_v_pos = ((get_thread_origin_y() + y) * 8 + 8) ;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////    PART I: COMPUTE MIN_COST_H   ////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    matrix<short, 16, 16> in, R;
    matrix<short, 8, 8> R_H, G_H, B_H;
    matrix<short, 16, 16> tmp;

    matrix<ushort, 8, 16> HDSUM_H;
    matrix<ushort, 16, 8> VDSUM_H;

    read(ibufRHOR, rd_h_pos - 8, rd_v_pos - 4, tmp.select<8,1,16,1>(0,0)); // tmp  == R
    read(ibufRHOR, rd_h_pos - 8, rd_v_pos + 4, tmp.select<8,1,16,1>(8,0)); // tmp  == R
    R_H = tmp.select<8,1,8,1>(4,4);
    R = tmp;

    read(ibufGHOR, rd_h_pos - 8, rd_v_pos - 4, in .select<8,1,16,1>(0,0)); // in   == G
    read(ibufGHOR, rd_h_pos - 8, rd_v_pos + 4, in .select<8,1,16,1>(8,0)); // in   == G
    G_H = in .select<8,1,8,1>(4,4);
    tmp -= in;                                                             // tmp  == DRG

    HDSUM_H.select<8,1,W,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<8,1,W,1>(4,1), - tmp.select<8,1,W,1>(4,0)));
    VDSUM_H.select<W,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<W,1,8,1>(1,4), - tmp.select<W,1,8,1>(0,4)));

    read(ibufBHOR, rd_h_pos - 8, rd_v_pos - 4, tmp .select<8,1,16,1>(0,0)); // tmp == B
    read(ibufBHOR, rd_h_pos - 8, rd_v_pos + 4, tmp .select<8,1,16,1>(8,0)); // tmp == B
    B_H = tmp.select<8,1,8,1>(4,4);
    in -= tmp;                                                              //  in == DGB

    HDSUM_H.select<8,1,W,1>(0,0) += cm_abs<ushort>(cm_add<short>(in.select<8,1,W,1>(4,1), - in.select<8,1,W,1>(4,0)));
    VDSUM_H.select<W,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(in.select<W,1,8,1>(1,4), - in.select<W,1,8,1>(0,4)));

    tmp -= R;                                                               // tmp == DBR

    HDSUM_H.select<8,1,W,1>(0,0) += cm_abs<ushort>(cm_add<short>(tmp.select<8,1,W,1>(4,1), - tmp.select<8,1,W,1>(4,0)));
    VDSUM_H.select<W,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(tmp.select<W,1,8,1>(1,4), - tmp.select<W,1,8,1>(0,4)));

    matrix_ref<int, 8, 8> HLSAD = tmp.select<8,1,16,1>(0,0).format<int, 8, 8>();
    matrix_ref<int, 8, 8> HRSAD = tmp.select<8,1,16,1>(8,0).format<int, 8, 8>();

    HLSAD  = HDSUM_H.select<8,1,8,1>(0,0);  HRSAD  = HDSUM_H.select<8,1,8,1>(0,4);
    HLSAD += HDSUM_H.select<8,1,8,1>(0,1);  HRSAD += HDSUM_H.select<8,1,8,1>(0,5);
    HLSAD += HDSUM_H.select<8,1,8,1>(0,2);  HRSAD += HDSUM_H.select<8,1,8,1>(0,6);
    HLSAD += HDSUM_H.select<8,1,8,1>(0,3);  HRSAD += HDSUM_H.select<8,1,8,1>(0,7);

    matrix_ref<int, 8, 8> Min_cost_H1 = in.select<8,1,16,1>(0,0).format<int, 8, 8>();
    Min_cost_H1 = cm_min<int>(HLSAD, HRSAD);

    matrix_ref<int, 8, 8> HUSAD = tmp.select<8,1,16,1>(0,0).format<int, 8, 8>();
    matrix_ref<int, 8, 8> HDSAD = tmp.select<8,1,16,1>(8,0).format<int, 8, 8>();

    HUSAD  = VDSUM_H.select<8,1,8,1>(0,0);  HDSAD  = VDSUM_H.select<8,1,8,1>(4,0);
    HUSAD += VDSUM_H.select<8,1,8,1>(1,0);  HDSAD += VDSUM_H.select<8,1,8,1>(5,0);
    HUSAD += VDSUM_H.select<8,1,8,1>(2,0);  HDSAD += VDSUM_H.select<8,1,8,1>(6,0);
    HUSAD += VDSUM_H.select<8,1,8,1>(3,0);  HDSAD += VDSUM_H.select<8,1,8,1>(7,0);

    matrix_ref<int, 8, 8> Min_cost_H2 = in.select<8,1,16,1>(8,0).format<int, 8, 8>();
    Min_cost_H2 = cm_min<int>(HUSAD, HDSAD);

    matrix<int, 8, 8> Min_cost_H = Min_cost_H1;
    Min_cost_H  = Min_cost_H2 >> SHIFT_MINCOST;
    Min_cost_H += Min_cost_H1                  ;


    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////    PART II: COMPUTE MIN_COST_V  ////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    matrix<short, 8, 8> R_V, G_V, B_V;
    matrix_ref<ushort, 8, 16> HDSUM_V = HDSUM_H;
    matrix_ref<ushort, 16, 8> VDSUM_V = VDSUM_H;

    read(ibufRVER, rd_h_pos - 8, rd_v_pos - 4, tmp.select<8,1,16,1>(0,0)); // tmp  == R
    read(ibufRVER, rd_h_pos - 8, rd_v_pos + 4, tmp.select<8,1,16,1>(8,0)); // tmp  == R
    R_V = tmp.select<8,1,8,1>(4,4);
    R = tmp;

    read(ibufGVER, rd_h_pos - 8, rd_v_pos - 4, in .select<8,1,16,1>(0,0)); // in   == G
    read(ibufGVER, rd_h_pos - 8, rd_v_pos + 4, in .select<8,1,16,1>(8,0)); // in   == G
    G_V = in .select<8,1,8,1>(4,4);
    tmp -= in;                                                             // tmp  == DRG

    HDSUM_V.select<8,1,W,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<8,1,W,1>(4,1), - tmp.select<8,1,W,1>(4,0)));
    VDSUM_V.select<W,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<W,1,8,1>(1,4), - tmp.select<W,1,8,1>(0,4)));

    read(ibufBVER, rd_h_pos - 8, rd_v_pos - 4, tmp .select<8,1,16,1>(0,0)); // tmp == B
    read(ibufBVER, rd_h_pos - 8, rd_v_pos + 4, tmp .select<8,1,16,1>(8,0)); // tmp == B
    B_V = tmp.select<8,1,8,1>(4,4);
    in -= tmp;                                                              //  in == DGB

    HDSUM_V.select<8,1,W,1>(0,0) += cm_abs<ushort>(cm_add<short>(in.select<8,1,W,1>(4,1), - in.select<8,1,W,1>(4,0)));
    VDSUM_V.select<W,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(in.select<W,1,8,1>(1,4), - in.select<W,1,8,1>(0,4)));

    tmp -= R;                                                               // tmp == DBR

    HDSUM_V.select<8,1,W,1>(0,0) += cm_abs<ushort>(cm_add<short>(tmp.select<8,1,W,1>(4,1), - tmp.select<8,1,W,1>(4,0)));
    VDSUM_V.select<W,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(tmp.select<W,1,8,1>(1,4), - tmp.select<W,1,8,1>(0,4)));

    matrix_ref<int, 8, 8> VLSAD = tmp.select<8,1,16,1>(0,0).format<int, 8, 8>();
    matrix_ref<int, 8, 8> VRSAD = tmp.select<8,1,16,1>(8,0).format<int, 8, 8>();

    VLSAD  = HDSUM_V.select<8,1,8,1>(0,0);  VRSAD  = HDSUM_V.select<8,1,8,1>(0,4);
    VLSAD += HDSUM_V.select<8,1,8,1>(0,1);  VRSAD += HDSUM_V.select<8,1,8,1>(0,5);
    VLSAD += HDSUM_V.select<8,1,8,1>(0,2);  VRSAD += HDSUM_V.select<8,1,8,1>(0,6);
    VLSAD += HDSUM_V.select<8,1,8,1>(0,3);  VRSAD += HDSUM_V.select<8,1,8,1>(0,7);

    matrix_ref<int, 8, 8> Min_cost_V2 = in.select<8,1,16,1>(0,0).format<int, 8, 8>();
    Min_cost_V2 = cm_min<int>(VLSAD, VRSAD);

    matrix_ref<int, 8, 8> VUSAD = tmp.select<8,1,16,1>(0,0).format<int, 8, 8>();
    matrix_ref<int, 8, 8> VDSAD = tmp.select<8,1,16,1>(8,0).format<int, 8, 8>();

    VUSAD  = VDSUM_V.select<8,1,8,1>(0,0);  VDSAD  = VDSUM_V.select<8,1,8,1>(4,0);
    VUSAD += VDSUM_V.select<8,1,8,1>(1,0);  VDSAD += VDSUM_V.select<8,1,8,1>(5,0);
    VUSAD += VDSUM_V.select<8,1,8,1>(2,0);  VDSAD += VDSUM_V.select<8,1,8,1>(6,0);
    VUSAD += VDSUM_V.select<8,1,8,1>(3,0);  VDSAD += VDSUM_V.select<8,1,8,1>(7,0);

    matrix_ref<int, 8, 8> Min_cost_V1 = in.select<8,1,16,1>(8,0).format<int, 8, 8>();
    Min_cost_V1 = cm_min<int>(VUSAD, VDSAD);

    matrix<int, 8, 8> Min_cost_V = Min_cost_V1;
    Min_cost_V  = Min_cost_V2 >> SHIFT_MINCOST;
    Min_cost_V += Min_cost_V1                  ;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////// PART III: COMPUTE FLAG(MIN_COST_H, MIN_COST_V)   ///////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    matrix<ushort, 8, 8> HV_Flag;
    HV_Flag.merge(1, 0, Min_cost_H < Min_cost_V);

    int wr_h_pos = ((get_thread_origin_x() + wr_x) * 8) * sizeof(short);
    int wr_v_pos = ((get_thread_origin_y() + wr_y) * 8) ;

    matrix_ref<ushort, 8, 8> outR = tmp.select<4,1,16,1>(0,0) .format<ushort, 8, 8>();
    matrix_ref<ushort, 8, 8> outG = tmp.select<4,1,16,1>(4,0) .format<ushort, 8, 8>();
    matrix_ref<ushort, 8, 8> outB = tmp.select<4,1,16,1>(8,0) .format<ushort, 8, 8>();

    outR.merge(R_H, R_V, (HV_Flag  == 1));
    outG.merge(G_H, G_V, (HV_Flag  == 1));
    outB.merge(B_H, B_V, (HV_Flag  == 1));

    write(obufROUT, wr_h_pos, wr_v_pos, outR);
    write(obufGOUT, wr_h_pos, wr_v_pos, outG);
    write(obufBOUT, wr_h_pos, wr_v_pos, outB);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////     DECIDE AVERAGE    ////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned short idx_init[16] = {1,1,2,2,5,5,6,6,9,9,10,10,13,13,14,14};

extern "C" _GENX_ void inline
HORIZONTAL_CHECK(matrix_ref<uchar, 13, 32> srcG, matrix_ref<ushort, 8, 16> maskH)
{
    vector<uchar, 32> rowG;
    matrix<short, 4, 16> diff;
    vector_ref<short, 16> D0 = diff.row(0);
    vector_ref<short, 16> D1 = diff.row(1);
    vector_ref<short, 16> D2 = diff.row(2);
    vector_ref<short, 16> D3 = diff.row(3);

#pragma unroll
    for(int j = 0; j < 8; j++) // j is index of output register
    {
        rowG = srcG.row(2+j);

        matrix_ref<uchar, 8, 4> m_rowG = rowG.format<uchar, 8, 4>();
        D0.select<8,2>(0) = m_rowG.genx_select<4,4,2,1>(0,1) - m_rowG.genx_select<4,4,2,1>(0,2);
        D1.select<8,2>(0) = m_rowG.genx_select<4,4,2,1>(0,2) - m_rowG.genx_select<4,4,2,1>(0,3);
        D2.select<8,2>(0) = m_rowG.genx_select<4,4,2,1>(0,3) - m_rowG.genx_select<4,4,2,1>(0,4);
        D3.select<8,2>(0) = m_rowG.genx_select<4,4,2,1>(0,4) - m_rowG.genx_select<4,4,2,1>(0,5);

        diff.select<2,1,8,2>(0,1) = diff.select<2,1,8,2>(0,0);  //D0.select<8,2>(1) = D0.select<8,2>(0); //D1.select<8,2>(1) = D1.select<8,2>(0);
        diff.select<2,1,8,2>(2,1) = diff.select<2,1,8,2>(2,0);  //D2.select<8,2>(1) = D2.select<8,2>(0); //D3.select<8,2>(1) = D3.select<8,2>(0);

        maskH.row(j)  = ( cm_abs<short>(D0) > Grn_imbalance_th);
        maskH.row(j) &= ( cm_abs<short>(D1) > Grn_imbalance_th);
        maskH.row(j) &= ( cm_abs<short>(D2) > Grn_imbalance_th);
        maskH.row(j) &= ( cm_abs<short>(D3) > Grn_imbalance_th);
        maskH.row(j) &= ( cm_mul<short>(D0, D1) < 0 );
        maskH.row(j) &= ( cm_mul<short>(D1, D2) < 0 );
        maskH.row(j) &= ( cm_mul<short>(D2, D3) < 0 );
    }
}

extern "C" _GENX_ void inline
VERTICAL_CHECK(matrix_ref<uchar, 13, 16> cenG, matrix_ref<ushort, 8, 16> maskV)
{
    matrix< short, 9, 16 > diff = cenG.select<9,1,16,1>(1,0) - cenG.select<9,1,16,1>(2,0);
    vector<ushort, 16> tmp_mask;

    tmp_mask      = (cm_mul< short>(diff.row(1), diff.row(2)) < 0);
    tmp_mask     &= (cm_mul< short>(diff.row(2), diff.row(3)) < 0);
    tmp_mask     &= (cm_abs<ushort>(diff.row(1)) > Grn_imbalance_th);
    tmp_mask     &= (cm_abs<ushort>(diff.row(2)) > Grn_imbalance_th);
    tmp_mask     &= (cm_abs<ushort>(diff.row(3)) > Grn_imbalance_th);

    maskV.row(0)  = tmp_mask & (cm_abs<ushort>(diff.row(0)) > Grn_imbalance_th);
    maskV.row(0) &= (cm_mul< short>(diff.row(0), diff.row(1)) < 0);

    maskV.row(2)  = tmp_mask & (cm_abs<ushort>(diff.row(4)) > Grn_imbalance_th);
    maskV.row(2) &= (cm_mul< short>(diff.row(3), diff.row(4)) < 0);

    maskV.row(1)  = maskV.row(0);
    maskV.row(3)  = maskV.row(2);

    /////////////////////////////////////////////////////////////////////////////////

    tmp_mask      = (cm_mul< short>(diff.row(5), diff.row(6)) < 0);
    tmp_mask     &= (cm_mul< short>(diff.row(6), diff.row(7)) < 0);
    tmp_mask     &= (cm_abs<ushort>(diff.row(5)) > Grn_imbalance_th);
    tmp_mask     &= (cm_abs<ushort>(diff.row(6)) > Grn_imbalance_th);
    tmp_mask     &= (cm_abs<ushort>(diff.row(7)) > Grn_imbalance_th);

    maskV.row(4)  = tmp_mask & (cm_abs<ushort>(diff.row(4)) > Grn_imbalance_th);
    maskV.row(4) &= (cm_mul< short>(diff.row(4), diff.row(5)) < 0);

    maskV.row(6)  = tmp_mask & (cm_abs<ushort>(diff.row(8)) > Grn_imbalance_th);
    maskV.row(6) &= (cm_mul< short>(diff.row(7), diff.row(8)) < 0);

    maskV.row(5)  = maskV.row(4);
    maskV.row(7)  = maskV.row(6);
}

/*
extern "C" _GENX_ void inline
COLORDIFF_CHECK(matrix_ref<uchar, 8, 16> srcR, matrix_ref<uchar, 8, 16> srcG, matrix_ref<uchar, 8, 16> srcB,
                matrix_ref<ushort, 8, 16> maskC, int AdjustedAvgTH)
{
    matrix< short , 8, 16 > diff;

    diff   =  cm_add< short>(srcR, -srcG);
    maskC  = (cm_abs<ushort>(diff) > AdjustedAvgTH);

    diff   =  cm_add< short>(srcG, -srcB);
    maskC |= (cm_abs<ushort>(diff) > AdjustedAvgTH);

    diff   =  cm_add< short>(srcB, -srcR);
    maskC |= (cm_abs<ushort>(diff) > AdjustedAvgTH);
}
*/
extern "C" _GENX_MAIN_ void
DECIDE_AVG( SurfaceIndex ibufRAVG, SurfaceIndex ibufGAVG, SurfaceIndex ibufBAVG,
            SurfaceIndex ibufAVG_Flag,
            SurfaceIndex obufROUT, SurfaceIndex obufGOUT, SurfaceIndex obufBOUT, int wr_x, int wr_y,
            int BayerType, int bitDepth)
{
    matrix< ushort, 13, 32 > srcG;
    matrix< ushort,  8, 16 > srcR, srcB;

    matrix< uchar , 13, 32 > srcG_tmp;
    //matrix< uchar ,  8, 16 > srcR_tmp, srcB_tmp;

    matrix< uchar,16, 16 >   AVG_flag;
    matrix<ushort, 8, 16 >   AVG_Mask, out;
    matrix<ushort, 8, 16 >   maskH, maskV, maskC;

    unsigned short bitshift = (bitDepth - 8);
    int AdjustAvgTH = AVG_COLOR_TH << bitshift;

    int rd_h_pos = (get_thread_origin_x() * 16 + 8 - 2) * sizeof(short); // + 8 for Output is Unpadded
    int rd_v_pos = (get_thread_origin_y() * 16 + 8 - 2);                 // + 8 for Output is Unpadded
    int rd_x_pos = (get_thread_origin_x() * 16 + 8);
    int rd_y_pos = (get_thread_origin_y() * 16 + 8);
    int wr_h_pos = ((get_thread_origin_x() + wr_x) * 16 ) * sizeof(short);
    int wr_v_pos = ((get_thread_origin_y() + wr_y) * 16 );

    read(ibufGAVG, rd_h_pos   , rd_v_pos  , srcG.select<8,1,16,1>(0,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos  , srcG.select<8,1,16,1>(0,16));
    read(ibufGAVG, rd_h_pos   , rd_v_pos+8, srcG.select<5,1,16,1>(8,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos+8, srcG.select<5,1,16,1>(8,16));
    read(ibufAVG_Flag, rd_x_pos, rd_y_pos, AVG_flag);

    ////////////////////////////////////////////////////////////////////////
    ///////////// FIRST 16x8 BLOCK  ////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////

    AVG_Mask = (AVG_flag.select<8,1,16,1>(0,0) == AVG_True);
    if(AVG_Mask.any())
    {
        srcG_tmp = (srcG >> bitshift);
        HORIZONTAL_CHECK(srcG_tmp, maskH);
        VERTICAL_CHECK  (srcG_tmp.select<13,1,16,1>(0,2), maskV);
        /*
        read(ibufRAVG, rd_h_pos+4 , rd_v_pos+2, srcR);
        read(ibufBAVG, rd_h_pos+4 , rd_v_pos+2, srcB);

        srcR_tmp = (srcR >> bitshift);
        srcB_tmp = (srcB >> bitshift);

        COLORDIFF_CHECK (srcR_tmp, srcG_tmp.select<8,1,16,1>(2,2), srcB_tmp, maskC, AdjustAvgTH);
        mask |= maskC;
        */
        maskH |= (maskV);
        AVG_flag.select<8,1,16,1>(0,0).merge(AVG_False, maskH);
    }

    AVG_Mask = (AVG_flag.select<8,1,16,1>(0,0) == AVG_True);

    if(AVG_Mask.any())
    {
        read(ibufRAVG, rd_h_pos+4 , rd_v_pos+2, srcR);
        read(ibufBAVG, rd_h_pos+4 , rd_v_pos+2, srcB);

        read (obufROUT, wr_h_pos, wr_v_pos, out);
        out.merge(srcR, AVG_Mask);
        write(obufROUT, wr_h_pos, wr_v_pos, out);

        read (obufGOUT, wr_h_pos, wr_v_pos, out);
        out.merge(srcG.select<8,1,16,1>(2,2), AVG_Mask);
        write(obufGOUT, wr_h_pos, wr_v_pos, out);

        read (obufBOUT, wr_h_pos, wr_v_pos, out);
        out.merge(srcB, AVG_Mask);
        write(obufBOUT, wr_h_pos, wr_v_pos, out);
    }

    ////////////////////////////////////////////////////////////////////////
    ///////////// SECOND 16x8 BLOCK  ///////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    wr_v_pos += 8;
    rd_v_pos += 8;

    if((AVG_flag.select<8,1,16,1>(8,0) == AVG_True).any())
    {
        srcG.select<5,1,16,1>(0,0 ) = srcG.select<5,1,16,1>(8,0 );
        srcG.select<5,1,16,1>(0,16) = srcG.select<5,1,16,1>(8,16);
        read(ibufGAVG, rd_h_pos   , rd_v_pos+5 , srcG.select<8,1,16,1>(5,0 ));
        read(ibufGAVG, rd_h_pos+32, rd_v_pos+5 , srcG.select<8,1,16,1>(5,16));

        srcG_tmp = (srcG >> bitshift);
        HORIZONTAL_CHECK(srcG_tmp, maskH);
        VERTICAL_CHECK  (srcG_tmp.select<13,1,16,1>(0,2), maskV);
        /*
        read(ibufRAVG, rd_h_pos+4 , rd_v_pos+2, srcR);
        read(ibufBAVG, rd_h_pos+4 , rd_v_pos+2, srcB);

        srcR_tmp = (srcR >> bitshift);
        srcB_tmp = (srcB >> bitshift);

        COLORDIFF_CHECK (srcR_tmp, srcG_tmp.select<8,1,16,1>(2,2), srcB_tmp, maskC, AdjustAvgTH);
        mask |= maskC;
        */
        maskH |= (maskV);
        AVG_flag.select<8,1,16,1>(8,0).merge(AVG_False, maskH);
    }

    AVG_Mask = (AVG_flag.select<8,1,16,1>(8,0) == AVG_True);

    if(AVG_Mask.any())
    {
        read(ibufRAVG, rd_h_pos+4 , rd_v_pos+2, srcR);
        read(ibufBAVG, rd_h_pos+4 , rd_v_pos+2, srcB);

        read (obufROUT, wr_h_pos, wr_v_pos, out);
        out.merge(srcR, AVG_Mask);
        write(obufROUT, wr_h_pos, wr_v_pos, out);

        read (obufGOUT, wr_h_pos, wr_v_pos, out);
        out.merge(srcG.select<8,1,16,1>(2,2), AVG_Mask);
        write(obufGOUT, wr_h_pos, wr_v_pos, out);

        read (obufBOUT, wr_h_pos, wr_v_pos, out);
        out.merge(srcB, AVG_Mask);
        write(obufBOUT, wr_h_pos, wr_v_pos, out);
    }
}


/////////////////////////////////////////////////////////////////////
// SKL Forward Gamma Correction GPU implementation
// Unit test :
/////////////////////////////////////////////////////////////////////

//////// Gamma Correction v5 with ARGB8 conversion

#define NUM_CONTROL_POINTS 64
const unsigned short initSeq[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

_GENX_ void inline
LOAD_LUT(SurfaceIndex Correct_Index, SurfaceIndex Point_Index,
		 int slmX,  SurfaceIndex LUTIndex, int BitDepth, int enableSLM)
{
    matrix<ushort, 4, 16> Point;   matrix_ref<ushort, 1, 64> gamma_P = Point  .format<ushort, 1, 64>();
    matrix<ushort, 4, 16> Correct; matrix_ref<ushort, 1, 64> gamma_C = Correct.format<ushort, 1, 64>();

    read(Correct_Index, 0, 0, Correct);
    read(Point_Index  , 0, 0, Point  );

    vector<uint,   16> index(0);
    vector<uint,   16> i_plus_1;
    vector<ushort, 16> v_in_initial(initSeq);
    vector<ushort, 16> c_i, p_i, c_i_plus_1, p_i_plus_1;
	vector<ushort, 16> v_in, mak, comp_index, v_out;
    vector<int,    16> v_o, tmp;

    int id = cm_linear_local_id();
    int total_load_elements = (1 << BitDepth);                                   // in number of elements
    int max_input_level = total_load_elements - 1;
    int load_elements_per_thread = total_load_elements / cm_linear_local_size(); // in number of elements
        load_elements_per_thread = (load_elements_per_thread + 15) & 0xFFFFFFF0;

    int loop_count = load_elements_per_thread / 16;                              // Each loop handle 16 elements

    for(int k = 0; k < loop_count; k++) {

        v_in = v_in_initial + (id * load_elements_per_thread + k * 16);


        index = 0; comp_index = 32;
#pragma unroll
        for(int i = 5; i >= 0; i--)
        {
            int shift = (1 << (i - 1));
            mak     = (gamma_P.iselect(comp_index) <= v_in);
            index  += (mak << i);
            comp_index.merge((comp_index+shift), (comp_index-shift), mak);
        }
        index.merge(0, ((index == 1) & (v_in < gamma_P(0,1))));

        i_plus_1.merge((NUM_CONTROL_POINTS-1), (index + 1), (index == (NUM_CONTROL_POINTS-1)));

        c_i = gamma_C.iselect(index);
        p_i = gamma_P.iselect(index);
        c_i_plus_1.merge( max_input_level,  gamma_C.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));
        p_i_plus_1.merge( max_input_level,  gamma_P.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));

        v_o  = cm_add<int>(c_i_plus_1, -c_i); // (c_i_plus_1 - c_i);
        v_o *= cm_add<int>(v_in, -p_i);       // (v_in - p_i);
        tmp  = cm_add<int>(p_i_plus_1, -p_i); // (p_i_plus_1 - p_i);
        tmp  = cm_max<int>(1, tmp);              // To avoid divide by 0
        v_o /= tmp;                              // v_o /= cm_add<int>(p_i_plus_1, -p_i, SAT);
        v_o += c_i;


        v_o = cm_max<uint>(0, v_o);
		if( enableSLM == 1)
		{
        v_out = cm_min<uint>(max_input_level, v_o);

        cm_slm_write(slmX, v_in, v_out);
    }
		else
		{
			v_o   = cm_min<uint>(max_input_level, v_o);    
			int addr = cm_linear_group_id() * 65536 * 4 + v_in(0) * 4; //(id * loop_count) * 64;
			write(LUTIndex, addr, v_o);
		}
	}
    cm_barrier();
}

_GENX_ void inline
GAMMA_ONLY_ARGB8_OUT(SurfaceIndex BUF_R, SurfaceIndex BUF_G, SurfaceIndex BUF_B, SurfaceIndex ARGB8, SurfaceIndex LUTIndex,
                     int h_pos, int v_pos, int slmX, int bitdepth, int BayerType, int outFrameHeight, int EnableSLM)
{
	int shift_amount = bitdepth - 8;;
    //matrix<ushort, 8, 16> wr_out;

    matrix<uchar, 8, 64> out;
	matrix<ushort, 24, 16> rd_in;
	vector<ushort, 16> v_Addr, rd_slmDataIn; //<uint,   16> rd_slmDataIn; 3.0 compiler doesn't support word granularity

	vector<uint  , 16> v_Addr_dw;

	vector< int  , 16> rd_bufferDataIn;
    //vector<uint,   16> rd_slmDataIn;   // for 3.0 compiler which doesn't support word granularity

    read(BUF_R, h_pos, v_pos, rd_in.select<8,1,16,1>(0, 0));
	read(BUF_G, h_pos, v_pos, rd_in.select<8,1,16,1>(8, 0));
	read(BUF_B, h_pos, v_pos, rd_in.select<8,1,16,1>(16,0));
	if(EnableSLM == 1)
	{
#pragma unroll
		for(int k = 0; k < 3; k++) {	

#pragma unroll
			for(int i = 0; i < 8; i++) {
				v_Addr = rd_in.row(8*k+i);
				cm_slm_read(slmX, v_Addr, rd_slmDataIn); 
				out.select<1,1,16,4>(i,k) = (rd_slmDataIn >> shift_amount);
    }
		}
	}
	else
	{
#pragma unroll
		for(int k = 0; k < 3; k++) {	
#pragma unroll
			for(int i = 0; i < 8; i++) {
				v_Addr_dw = rd_in.row(8*k+i);
				read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
				out.select<1,1,16,4>(i,k) = (rd_bufferDataIn >> shift_amount);
    }
		}
	}

#pragma unroll
    for(int i = 0; i < 8; i++)
        out.select<1,1,16,4>(i,3) = 0;
	

    vector<uchar, 64> tmp;
	if(BayerType == GRBG || BayerType == GBRG) {
#pragma unroll
		for(int j = 0; j < 4; j++) {
            tmp = out.row(j);
            out.row(j) = out.row(7-j);
            out.row(7-j) = tmp;
        }
    }

    // Enable if choose to do tile processing for Gamma module
    //h_pos += (wr_x * 2);
    //v_pos += wr_y;

    if(BayerType == GRBG || BayerType == GBRG)
        v_pos = (outFrameHeight - 16) - v_pos + 8;

    write(ARGB8, h_pos*2   , v_pos, out.select<8,1,32,1>(0,0 ));
    write(ARGB8, h_pos*2+32, v_pos, out.select<8,1,32,1>(0,32));
}

_GENX_ void inline
GAMMA_3D_ONLY_ARGB8_OUT(SurfaceIndex BUF_R, SurfaceIndex BUF_G, SurfaceIndex BUF_B, SurfaceIndex ARGB8, SurfaceIndex LUTIndex,
                     int h_pos, int v_pos, int slmX, int R, int G, int B, int bitdepth, int BayerType, int outFrameHeight, int EnableSLM, matrix_ref<uchar, 8, 64> out)
{
    int shift_amount = bitdepth - 8;;

    matrix<ushort, 24, 16> rd_in;
    vector<ushort, 16> v_Addr, rd_slmDataIn;
    vector<uint  , 16> v_Addr_dw;
    vector< int  , 16> rd_bufferDataIn;

    if ( R == 1 ) read(BUF_R, h_pos, v_pos, rd_in.select<8,1,16,1>(0, 0));
    if ( G == 1 ) read(BUF_G, h_pos, v_pos, rd_in.select<8,1,16,1>(8, 0));
    if ( B == 1 ) read(BUF_B, h_pos, v_pos, rd_in.select<8,1,16,1>(16,0));

    if ( R == 1 )
    {
        if(EnableSLM == 1)
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                v_Addr = rd_in.row(i);
                cm_slm_read(slmX, v_Addr, rd_slmDataIn); 
                out.select<1,1,16,4>(i,0) = (rd_slmDataIn >> shift_amount);
            }
        }
        else
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                v_Addr_dw = rd_in.row(i);
                read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
                out.select<1,1,16,4>(i,0) = (rd_bufferDataIn >> shift_amount);
            }
        }
    }

    if ( G == 1 )
    {
        if(EnableSLM == 1)
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                v_Addr = rd_in.row(8+i);
                cm_slm_read(slmX, v_Addr, rd_slmDataIn); 
                out.select<1,1,16,4>(i,1) = (rd_slmDataIn >> shift_amount);
            }
        }
        else
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                v_Addr_dw = rd_in.row(8+i);
                read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
                out.select<1,1,16,4>(i,1) = (rd_bufferDataIn >> shift_amount);
            }
        }
    }


    if ( B == 1 )
    {
        if(EnableSLM == 1)
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                v_Addr = rd_in.row(8*2+i);
                cm_slm_read(slmX, v_Addr, rd_slmDataIn); 
                out.select<1,1,16,4>(i,2) = (rd_slmDataIn >> shift_amount);
            }
        }
        else
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                v_Addr_dw = rd_in.row(8*2+i);
                read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
                out.select<1,1,16,4>(i,2) = (rd_bufferDataIn >> shift_amount);
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////   KERNEL 'GAMMA+ARGB8_INTERLEAVE' FOR CANON    //////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
_GENX_ void inline
                     //SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
GAMMA_ONLY_ARGB16_OUT(SurfaceIndex BUF_R, SurfaceIndex BUF_G, SurfaceIndex BUF_B, SurfaceIndex ARGB16, SurfaceIndex LUTIndex,
                         int h_pos, int v_pos, int slmX, int bitdepth, int BayerType, int outFrameHeight, int EnableSLM)
{
	int shift_amount = 16 - bitdepth;

     //ENABLE LOOK-UP TABLE COMPUTING
    matrix<ushort, 8,  64> out;


	matrix<ushort, 24, 16> rd_in;

	vector<ushort, 16> v_Addr, rd_slmDataIn; //<uint, 16> rd_slmDataIn; 3.0 compiler doesn't support word granularity
	vector<uint  , 16> v_Addr_dw, rd_bufferDataIn;

    read(BUF_R, h_pos, v_pos, rd_in.select<8,1,16,1>(0, 0));
	read(BUF_G, h_pos, v_pos, rd_in.select<8,1,16,1>(8, 0));
	read(BUF_B, h_pos, v_pos, rd_in.select<8,1,16,1>(16,0));

	if(EnableSLM == 1)



    {
#pragma unroll
		for(int k = 0; k < 3; k++) {	

#pragma unroll
			for(int i = 0; i < 8; i++) {
				v_Addr = rd_in.row(8*k+i);
				cm_slm_read(slmX, v_Addr, rd_slmDataIn); 
				out.select<1,1,16,4>(i,k) = (rd_slmDataIn << shift_amount);
			}
        }

        //vector<uint, 16> i_plus_1;


        //c_i_plus_1.merge( max_input_level,Correct.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));
        //p_i_plus_1.merge( max_input_level,Point  .iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));




        //v_o /= cm_add<int>(p_i_plus_1, -p_i, SAT);//(p_i_plus_1 - p_i);


        //vector<ushort, 16> v_out;

    }

	else


    /////////////////////////////////////////////////////////////////////////////////
    //////////////////  LOOK-UP TABLE PREPARED   ////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////

    {                                   // Four Groups, jointly, one time does 128 pixel high. Need (height/128) loops
#pragma unroll
		for(int k = 0; k < 3; k++) {	
                //EACH thread in the threadgroup handle 256 byte-wide data
#pragma unroll
			for(int i = 0; i < 8; i++) {
				v_Addr_dw = rd_in.row(8*k+i);
				read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
				out.select<1,1,16,4>(i,k) = (rd_bufferDataIn << shift_amount);
                }

        }
    }


    // INITIALIZE rd_h_pos, rd_v_pos s.t. rd_h_pos is 32x and rd_v_pos is 8y

    //while(rd_v_pos < frameheight)

    //for(int k = 0; k < 540; k++)

         // UPDAYE process position

         //if(rd_v_pos > frameheight)
        //     break;




#pragma unroll
    for(int i = 0; i < 8; i++)
        out.select<1,1,16,4>(i,3) = 0;
	
	vector<ushort, 64> tmp;
	if(BayerType == GRBG || BayerType == GBRG) {
#pragma unroll
		for(int j = 0; j < 4; j++) {
			tmp = out.row(j);
			out.row(j) = out.row(7-j);
			out.row(7-j) = tmp;
    }

    }
	if(BayerType == GRBG || BayerType == GBRG)
		v_pos = (outFrameHeight - 16) - v_pos + 8;

    write(ARGB16, h_pos*4   , v_pos, out.select<8,1,16,1>(0,0 ));
    write(ARGB16, h_pos*4+32, v_pos, out.select<8,1,16,1>(0,16));
	write(ARGB16, h_pos*4+64, v_pos, out.select<8,1,16,1>(0,32));
    write(ARGB16, h_pos*4+96, v_pos, out.select<8,1,16,1>(0,48));
}


_GENX_ void inline
GAMMA_3D_ONLY_ARGB16_OUT(SurfaceIndex BUF_R, SurfaceIndex BUF_G, SurfaceIndex BUF_B, SurfaceIndex ARGB16, SurfaceIndex LUTIndex,
                         int h_pos, int v_pos, int slmX, int R, int G, int B, int bitdepth, int BayerType, int outFrameHeight, int EnableSLM, matrix_ref<ushort, 8, 64> out)
{
    int shift_amount = 16 - bitdepth;

    //ENABLE LOOK-UP TABLE COMPUTING
    matrix<ushort, 24, 16> rd_in;
    vector<ushort, 16> v_Addr, rd_slmDataIn; //<uint, 16> rd_slmDataIn; 3.0 compiler doesn't support word granularity
    vector<uint  , 16> v_Addr_dw, rd_bufferDataIn;

    if ( R == 1 ) read(BUF_R, h_pos, v_pos, rd_in.select<8,1,16,1>(0, 0));
    if ( G == 1 ) read(BUF_G, h_pos, v_pos, rd_in.select<8,1,16,1>(8, 0));
    if ( B == 1 ) read(BUF_B, h_pos, v_pos, rd_in.select<8,1,16,1>(16,0));

    if(EnableSLM == 1)
    {
        if ( R == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                v_Addr = rd_in.row(i);
                cm_slm_read(slmX, v_Addr, rd_slmDataIn); 
                out.select<1,1,16,4>(i,0) = (rd_slmDataIn << shift_amount);
            }
        }

        if ( G == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                v_Addr = rd_in.row(8+i);
                cm_slm_read(slmX, v_Addr, rd_slmDataIn); 
                out.select<1,1,16,4>(i,1) = (rd_slmDataIn << shift_amount);
            }
        }

        if ( B == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                v_Addr = rd_in.row(16+i);
                cm_slm_read(slmX, v_Addr, rd_slmDataIn); 
                out.select<1,1,16,4>(i,2) = (rd_slmDataIn << shift_amount);
            }
        }
    }
    else
    /////////////////////////////////////////////////////////////////////////////////
    //////////////////  LOOK-UP TABLE PREPARED   ////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////
    {                                   // Four Groups, jointly, one time does 128 pixel high. Need (height/128) loops
        if ( R == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                v_Addr_dw = rd_in.row(i);
                read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
                out.select<1,1,16,4>(i,0) = (rd_bufferDataIn << shift_amount);
            }
        }

        if ( G == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                v_Addr_dw = rd_in.row(8+i);
                read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
                out.select<1,1,16,4>(i,1) = (rd_bufferDataIn << shift_amount);
            }
        }

        if ( B == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                v_Addr_dw = rd_in.row(16+i);
                read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
                out.select<1,1,16,4>(i,2) = (rd_bufferDataIn << shift_amount);
            }
        }
    }
}

extern "C" _GENX_MAIN_ void
GAMMA_ONLY_ARGB_OUT(SurfaceIndex Correct_Index_R, SurfaceIndex Correct_Index_G, SurfaceIndex Correct_Index_B, 
                    SurfaceIndex Point_Index,
					SurfaceIndex B_I_Index, SurfaceIndex G_I_Index, SurfaceIndex R_I_Index,
                    SurfaceIndex ARGB_Index,
           //SurfaceIndex Out_Index, // not used
                    int blocks_in_a_row, int BitDepth, int framewidth_in_bytes, 
					int frameheight, int BayerType, int Enable_ARGB8,
                    SurfaceIndex LUTIndex_R, SurfaceIndex LUTIndex_G, SurfaceIndex LUTIndex_B)
{
    ////////////////////////////////////////////////////////////////////////////////////////

    cm_slm_init(65536);
    uint slmX = cm_slm_alloc(65536);

    //ENABLE LOOK-UP TABLE COMPUTING




	int enableSLM = (BitDepth < 16)? 1 : 0;
	LOAD_LUT(Correct_Index_R, Point_Index, slmX, LUTIndex_R, BitDepth, enableSLM);
    //int loop_count = ((load_elements_per_thread % 16) == 0)? (load_elements_per_thread / 16) : ((load_elements_per_thread / 16) + 1);










        //v_o /= (p_i_plus_1 - p_i);



        //vector<uint, 16> v_out;

        //v_out = cm_min<uint>(max_input_level, v_o);



    /////////////////////////////////////////////////////////////////////////////////
    //////////////////  LOOK-UP TABLE PREPARED   ////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////

	int framewidth		= framewidth_in_bytes / 4;
	int threadswidth	= (framewidth % 16 == 0)? (framewidth / 16) : ((framewidth / 16) + 1);
	int threadheight	= (frameheight % 8 == 0)? (frameheight / 8) : ((frameheight / 8) + 1);
	int threadspacesize = threadswidth * threadheight;
	int T_id = cm_linear_global_id();

	int Total_Thread_Num = cm_linear_global_size();
	int loop_count = (threadspacesize % Total_Thread_Num == 0)? 
					 (threadspacesize / Total_Thread_Num) : ((threadspacesize / Total_Thread_Num) + 1);
	
	if(Enable_ARGB8 == 1) {
		for(int k = 0; k < loop_count; k++) {
			 int rd_h_pos = (T_id % threadswidth) * 16 * 2; // position in units of bytes
			 int rd_v_pos = (T_id / threadswidth) * 8 ;		// position in units of bytes
			 GAMMA_ONLY_ARGB8_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUTIndex_R,
		  				 		  rd_h_pos, rd_v_pos, slmX, BitDepth, BayerType, frameheight, enableSLM);

			 T_id += cm_linear_global_size();
    }
}


	else {


		for(int k = 0; k < loop_count; k++) {
			 int rd_h_pos = (T_id % threadswidth) * 16 * 2; // position in units of bytes
			 int rd_v_pos = (T_id / threadswidth) * 8 ;		// position in units of bytes

			 GAMMA_ONLY_ARGB16_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUTIndex_R,
		  				 		   rd_h_pos, rd_v_pos, slmX, BitDepth, BayerType, frameheight, enableSLM);

			 T_id += cm_linear_global_size();
    }

    }
}

extern "C" _GENX_MAIN_ void
GAMMA_3D_ONLY_ARGB_OUT(SurfaceIndex Correct_Index_R, SurfaceIndex Correct_Index_G, SurfaceIndex Correct_Index_B,
                    SurfaceIndex Point_Index,
                    SurfaceIndex B_I_Index, SurfaceIndex G_I_Index, SurfaceIndex R_I_Index,
                    SurfaceIndex ARGB_Index,
                    int blocks_in_a_row, int BitDepth, int framewidth_in_bytes, 
                    int frameheight, int BayerType, int Enable_ARGB8, 
                    SurfaceIndex LUTIndex_R, SurfaceIndex LUTIndex_G, SurfaceIndex LUTIndex_B)
{
    ////////////////////////////////////////////////////////////////////////////////////////

    cm_slm_init(65536);
    uint slmXR = cm_slm_alloc(65536);

    matrix<uchar, 8, 64>   out;
    matrix<ushort, 8,  64> out16;
    //ENABLE LOOK-UP TABLE COMPUTING

    int enableSLM = (BitDepth < 16) ? 1 : 0;

    /////////////////////////////////////////////////////////////////////////////////
    //////////////////  LOOK-UP TABLE PREPARED   ////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////

    int framewidth      = framewidth_in_bytes / 4;
    int threadswidth    = (framewidth % 16 == 0)? (framewidth / 16) : ((framewidth / 16) + 1);
    int threadheight    = (frameheight % 8 == 0)? (frameheight / 8) : ((frameheight / 8) + 1);
    int threadspacesize = threadswidth * threadheight;
    int T_id = cm_linear_global_id();

    int Total_Thread_Num = cm_linear_global_size();
    int loop_count = (threadspacesize % Total_Thread_Num == 0)? 
                     (threadspacesize / Total_Thread_Num) : ((threadspacesize / Total_Thread_Num) + 1);

    if(Enable_ARGB8 == 1)
    {
        for(int k = 0; k < loop_count; k++)
        {
            int rd_h_pos = (T_id % threadswidth) * 16 * 2; // position in units of bytes
            int rd_v_pos = (T_id / threadswidth) * 8 ;     // position in units of bytes
            LOAD_LUT(Correct_Index_R, Point_Index, slmXR, LUTIndex_R, BitDepth, enableSLM);
            GAMMA_3D_ONLY_ARGB8_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUTIndex_R,
                                rd_h_pos, rd_v_pos, slmXR, 1,0,0,BitDepth, BayerType, frameheight, enableSLM, out);
            LOAD_LUT(Correct_Index_G, Point_Index, slmXR, LUTIndex_G, BitDepth, enableSLM);
            GAMMA_3D_ONLY_ARGB8_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUTIndex_G,
                                rd_h_pos, rd_v_pos, slmXR, 0,1,0,BitDepth, BayerType, frameheight, enableSLM, out);
            LOAD_LUT(Correct_Index_B, Point_Index, slmXR, LUTIndex_B, BitDepth, enableSLM);
            GAMMA_3D_ONLY_ARGB8_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUTIndex_B,
                                rd_h_pos, rd_v_pos, slmXR, 0,0,1,BitDepth, BayerType, frameheight, enableSLM, out);

            T_id += cm_linear_global_size();

#pragma unroll
            for(int i = 0; i < 8; i++)
                out.select<1,1,16,4>(i,3) = 0;

            vector<uchar, 64> tmp;
            if(BayerType == GRBG || BayerType == GBRG)
            {
#pragma unroll
                for(int j = 0; j < 4; j++) {
                    tmp = out.row(j);
                    out.row(j) = out.row(7-j);
                    out.row(7-j) = tmp;
                }
            }

            if(BayerType == GRBG || BayerType == GBRG)
                rd_v_pos = (frameheight - 16) - rd_v_pos + 8;

            write(ARGB_Index, rd_h_pos*2   , rd_v_pos, out.select<8,1,32,1>(0,0 ));
            write(ARGB_Index, rd_h_pos*2+32, rd_v_pos, out.select<8,1,32,1>(0,32));
        }
    }
    else
    {
        for(int k = 0; k < loop_count; k++)
        {
            int rd_h_pos = (T_id % threadswidth) * 16 * 2; // position in units of bytes
            int rd_v_pos = (T_id / threadswidth) * 8 ;     // position in units of bytes

            LOAD_LUT(Correct_Index_R, Point_Index, slmXR, LUTIndex_R, BitDepth, enableSLM);
            GAMMA_3D_ONLY_ARGB16_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUTIndex_R,
                                     rd_h_pos, rd_v_pos, slmXR, 1,0,0, BitDepth, BayerType, frameheight, enableSLM, out16);

            LOAD_LUT(Correct_Index_G, Point_Index, slmXR, LUTIndex_G, BitDepth, enableSLM);
            GAMMA_3D_ONLY_ARGB16_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUTIndex_G,
                                     rd_h_pos, rd_v_pos, slmXR, 0,1,0, BitDepth, BayerType, frameheight, enableSLM, out16);

            LOAD_LUT(Correct_Index_B, Point_Index, slmXR, LUTIndex_B, BitDepth, enableSLM);
            GAMMA_3D_ONLY_ARGB16_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUTIndex_B,
                                     rd_h_pos, rd_v_pos, slmXR, 0,0,1, BitDepth, BayerType, frameheight, enableSLM, out16);

            T_id += cm_linear_global_size();

            #pragma unroll
            for(int i = 0; i < 8; i++)
                out16.select<1,1,16,4>(i,3) = 0;

            vector<ushort, 64> tmp;
            if(BayerType == GRBG || BayerType == GBRG)
            {
                #pragma unroll
                for(int j = 0; j < 4; j++)
                {
                    tmp = out16.row(j);
                    out16.row(j) = out16.row(7-j);
                    out16.row(7-j) = tmp;
                }
            }

            if(BayerType == GRBG || BayerType == GBRG)
                rd_v_pos = (frameheight - 16) - rd_v_pos + 8;

            write(ARGB_Index, rd_h_pos*4   , rd_v_pos, out16.select<8,1,16,1>(0,0 ));
            write(ARGB_Index, rd_h_pos*4+32, rd_v_pos, out16.select<8,1,16,1>(0,16));
            write(ARGB_Index, rd_h_pos*4+64, rd_v_pos, out16.select<8,1,16,1>(0,32));
            write(ARGB_Index, rd_h_pos*4+96, rd_v_pos, out16.select<8,1,16,1>(0,48));
        }
    }
}

_GENX_ void inline
GAMMA_3D_CCM_ARGB8_OUT(SurfaceIndex BUF_R, SurfaceIndex BUF_G, SurfaceIndex BUF_B, SurfaceIndex ARGB8, SurfaceIndex LUTIndex,
                    int h_pos, int v_pos, int slmX, int R, int G, int B, int bitdepth, int BayerType, int outFrameHeight, vector<float, 9> ccm, int EnableSLM, matrix_ref<uchar ,  8, 64> out)
{
    int shift_amount = bitdepth - 8;
    ushort max_input_level = (1 << bitdepth) - 1;

    matrix<ushort, 24, 16> rd_in;
    vector<ushort, 16> tmp_u16, v_Addr, rd_slmDataIn;
    vector<uint  , 16> v_Addr_dw;
    vector< int  , 16> rd_bufferDataIn;
    vector<float , 16> acc_f, tmp1_f, tmp2_f, tmp3_f, in_f;
    vector<int   , 16> tmp_int;

    if ( R == 1 ) read(BUF_R, h_pos, v_pos, rd_in.select<8,1,16,1>(0, 0));
    if ( G == 1 ) read(BUF_G, h_pos, v_pos, rd_in.select<8,1,16,1>(8, 0));
    if ( B == 1 ) read(BUF_B, h_pos, v_pos, rd_in.select<8,1,16,1>(16,0));

    if(EnableSLM == 1)
    {
        if ( R == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2)  ));
                tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2)+1));
                tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2)+2));
                acc_f   = cm_add<float>(tmp1_f, tmp2_f);
                acc_f   = cm_add<float>(acc_f , tmp3_f);
                tmp_int = cm_rndz<int>(acc_f);
                tmp_int = cm_max<int>(tmp_int, 0);
                tmp_u16 = cm_min<int>(tmp_int, max_input_level);
                v_Addr  = tmp_u16;
                cm_slm_read(slmX, v_Addr, rd_slmDataIn);
                out.select<1,1,16,4>(i,0) = (rd_slmDataIn >> shift_amount);
            }
        }

        if ( G == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(1)  ));
                tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(1)+1));
                tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(1)+2));
                acc_f   = cm_add<float>(tmp1_f, tmp2_f);
                acc_f   = cm_add<float>(acc_f , tmp3_f);
                tmp_int = cm_rndz<int>(acc_f);
                tmp_int = cm_max<int>(tmp_int, 0);
                tmp_u16 = cm_min<int>(tmp_int, max_input_level);
                v_Addr  = tmp_u16;
                cm_slm_read(slmX, v_Addr, rd_slmDataIn);
                out.select<1,1,16,4>(i,1) = (rd_slmDataIn >> shift_amount);
            }
        }

        if ( B == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2-2)  ));
                tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2-2)+1));
                tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2-2)+2));
                acc_f   = cm_add<float>(tmp1_f, tmp2_f);
                acc_f   = cm_add<float>(acc_f , tmp3_f);
                tmp_int = cm_rndz<int>(acc_f);
                tmp_int = cm_max<int>(tmp_int, 0);
                tmp_u16 = cm_min<int>(tmp_int, max_input_level);
                v_Addr  = tmp_u16;
                cm_slm_read(slmX, v_Addr, rd_slmDataIn); 
                out.select<1,1,16,4>(i,2) = (rd_slmDataIn >> shift_amount);
            }
        }
    }
    else
    {
        if ( R == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2)  ));
                tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2)+1));
                tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2)+2));
                acc_f   = cm_add<float>(tmp1_f, tmp2_f);
                acc_f   = cm_add<float>(acc_f , tmp3_f);
                tmp_int = cm_rndz<int>(acc_f);
                tmp_int = cm_max<int>(tmp_int, 0);
                tmp_u16 = cm_min<int>(tmp_int, max_input_level);
                v_Addr_dw = tmp_u16;
                read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
                out.select<1,1,16,4>(i,0) = (rd_bufferDataIn >> shift_amount);
            }
        }

        if ( G == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2-1)  ));
                tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2-1)+1));
                tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2-1)+2));
                acc_f   = cm_add<float>(tmp1_f, tmp2_f);
                acc_f   = cm_add<float>(acc_f , tmp3_f);
                tmp_int = cm_rndz<int>(acc_f);
                tmp_int = cm_max<int>(tmp_int, 0);
                tmp_u16 = cm_min<int>(tmp_int, max_input_level);
                v_Addr_dw = tmp_u16;
                read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
                out.select<1,1,16,4>(i,1) = (rd_bufferDataIn >> shift_amount);
            }
        }

        if ( B == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2-2)  ));
                tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2-2)+1));
                tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2-2)+2));
                acc_f   = cm_add<float>(tmp1_f, tmp2_f);
                acc_f   = cm_add<float>(acc_f , tmp3_f);
                tmp_int = cm_rndz<int>(acc_f);
                tmp_int = cm_max<int>(tmp_int, 0);
                tmp_u16 = cm_min<int>(tmp_int, max_input_level);
                v_Addr_dw = tmp_u16;
                read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
                out.select<1,1,16,4>(i,2) = (rd_bufferDataIn >> shift_amount);
            }
        }
    }
}

_GENX_ void inline
GAMMA_CCM_ARGB8_OUT(SurfaceIndex BUF_R, SurfaceIndex BUF_G, SurfaceIndex BUF_B, SurfaceIndex ARGB8, SurfaceIndex LUTIndex,
                    int h_pos, int v_pos, int slmX, int bitdepth, int BayerType, int outFrameHeight, vector<float, 9> ccm, int EnableSLM)
{
	int shift_amount = bitdepth - 8;
	ushort max_input_level = (1 << bitdepth) - 1;
    matrix<uchar ,  8, 64> out;
	matrix<ushort, 24, 16> rd_in;
	vector<ushort, 16> tmp_u16, v_Addr, rd_slmDataIn; //vector<uint,   16> rd_slmDataIn;   // for 3.0 compiler which doesn't support word granularity
	vector<uint  , 16> v_Addr_dw;
	vector< int  , 16> rd_bufferDataIn;
	vector<float , 16> acc_f, tmp1_f, tmp2_f, tmp3_f, in_f;
	vector<int   , 16> tmp_int;

    read(BUF_R, h_pos, v_pos, rd_in.select<8,1,16,1>(0, 0));
	read(BUF_G, h_pos, v_pos, rd_in.select<8,1,16,1>(8, 0));
	read(BUF_B, h_pos, v_pos, rd_in.select<8,1,16,1>(16,0));

	if(EnableSLM == 1)

    {

#pragma unroll
		for(int k = 0; k < 3; k++) {	

#pragma unroll
			for(int i = 0; i < 8; i++) {
				tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2-k)  ));
				tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2-k)+1));








				tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2-k)+2));
				acc_f   = cm_add<float>(tmp1_f, tmp2_f);
				acc_f   = cm_add<float>(acc_f , tmp3_f);
				tmp_int = cm_rndz<int>(acc_f);
				tmp_int = cm_max<int>(tmp_int, 0);				 
				tmp_u16 = cm_min<int>(tmp_int, max_input_level);
				v_Addr  = tmp_u16;
				cm_slm_read(slmX, v_Addr, rd_slmDataIn); 
				out.select<1,1,16,4>(i,k) = (rd_slmDataIn >> shift_amount);
    }

    }

    }
	else

    {
#pragma unroll
		for(int k = 0; k < 3; k++) {	









#pragma unroll
			for(int i = 0; i < 8; i++) {
				tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2-k)  ));
				tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2-k)+1));
				tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2-k)+2));
				acc_f   = cm_add<float>(tmp1_f, tmp2_f);
				acc_f   = cm_add<float>(acc_f , tmp3_f);
				tmp_int = cm_rndz<int>(acc_f);
				tmp_int = cm_max<int>(tmp_int, 0);				 
				tmp_u16 = cm_min<int>(tmp_int, max_input_level);
				v_Addr_dw = tmp_u16;
				read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
				out.select<1,1,16,4>(i,k) = (rd_bufferDataIn >> shift_amount);
    }

    }

    }

#pragma unroll
    for(int i = 0; i < 8; i++)
        out.select<1,1,16,4>(i,3) = 0;

	vector<uchar, 64> tmp;
	if(BayerType == GRBG || BayerType == GBRG) {
#pragma unroll
		for(int j = 0; j < 4; j++) {
			tmp = out.row(j);
			out.row(j) = out.row(7-j);
			out.row(7-j) = tmp;
        }
    }

	if(BayerType == GRBG || BayerType == GBRG)
		v_pos = (outFrameHeight - 16) - v_pos + 8;

    write(ARGB8, h_pos*2   , v_pos, out.select<8,1,32,1>(0,0 ));
    write(ARGB8, h_pos*2+32, v_pos, out.select<8,1,32,1>(0,32));
}

_GENX_ void inline
GAMMA_3D_CCM_ARGB16_OUT(SurfaceIndex BUF_R, SurfaceIndex BUF_G, SurfaceIndex BUF_B, SurfaceIndex ARGB16, SurfaceIndex LUTIndex,
                     int h_pos, int v_pos, int slmX, int R, int G, int B, int bitdepth, int BayerType, int outFrameHeight, vector<float, 9> ccm, int EnableSLM, matrix_ref<ushort,  8, 64> out)
{
    int shift_amount = 16 - bitdepth;
    ushort max_input_level = (1 << bitdepth) - 1;
    matrix<ushort, 24, 16> rd_in;
    vector<ushort, 16> tmp_u16, v_Addr, rd_slmDataIn; //vector<uint,   16> rd_slmDataIn;   // for 3.0 compiler which doesn't support word granularity

    vector<uint  , 16> v_Addr_dw, rd_bufferDataIn;
    vector<float , 16> acc_f, tmp1_f, tmp2_f, tmp3_f, in_f;
    vector<int   , 16> tmp_int;

    if ( R == 1 ) read(BUF_R, h_pos, v_pos, rd_in.select<8,1,16,1>(0, 0));
    if ( G == 1 ) read(BUF_G, h_pos, v_pos, rd_in.select<8,1,16,1>(8, 0));
    if ( B == 1 ) read(BUF_B, h_pos, v_pos, rd_in.select<8,1,16,1>(16,0));

    if(EnableSLM == 1)
    {
        if ( R == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2)  ));
                tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2)+1));
                tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2)+2));
                acc_f   = cm_add<float>(tmp1_f, tmp2_f);
                acc_f   = cm_add<float>(acc_f , tmp3_f);
                tmp_int = cm_rndz<int>(acc_f);
                tmp_int = cm_max<int>(tmp_int, 0);
                tmp_u16 = cm_min<int>(tmp_int, max_input_level);
                v_Addr  = tmp_u16;
                cm_slm_read(slmX, v_Addr, rd_slmDataIn);
                out.select<1,1,16,4>(i,0) = (rd_slmDataIn << shift_amount);
            }
        }

        if ( G == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2-1)  ));
                tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2-1)+1));
                tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2-1)+2));
                acc_f   = cm_add<float>(tmp1_f, tmp2_f);
                acc_f   = cm_add<float>(acc_f , tmp3_f);
                tmp_int = cm_rndz<int>(acc_f);
                tmp_int = cm_max<int>(tmp_int, 0);
                tmp_u16 = cm_min<int>(tmp_int, max_input_level);
                v_Addr  = tmp_u16;
                cm_slm_read(slmX, v_Addr, rd_slmDataIn);
                out.select<1,1,16,4>(i,1) = (rd_slmDataIn << shift_amount);
            }
        }

        if ( B == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++)
            {
                tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2-2)  ));
                tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2-2)+1));
                tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2-2)+2));
                acc_f   = cm_add<float>(tmp1_f, tmp2_f);
                acc_f   = cm_add<float>(acc_f , tmp3_f);
                tmp_int = cm_rndz<int>(acc_f);
                tmp_int = cm_max<int>(tmp_int, 0);
                tmp_u16 = cm_min<int>(tmp_int, max_input_level);
                v_Addr  = tmp_u16;
                cm_slm_read(slmX, v_Addr, rd_slmDataIn);
                out.select<1,1,16,4>(i,2) = (rd_slmDataIn << shift_amount);
            }
        }
    }
    else
    {
        if ( R == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++) 
            {
                tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2)  ));
                tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2)+1));
                tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2)+2));
                acc_f   = cm_add<float>(tmp1_f, tmp2_f);
                acc_f   = cm_add<float>(acc_f , tmp3_f);
                tmp_int = cm_rndz<int>(acc_f);
                tmp_int = cm_max<int>(tmp_int, 0);
                tmp_u16 = cm_min<int>(tmp_int, max_input_level);
                v_Addr_dw = tmp_u16;
                read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
                out.select<1,1,16,4>(i,0) = (rd_bufferDataIn << shift_amount);
            }
        }

        if ( G == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++) 
            {
                tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2-1)  ));
                tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2-1)+1));
                tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2-1)+2));
                acc_f   = cm_add<float>(tmp1_f, tmp2_f);
                acc_f   = cm_add<float>(acc_f , tmp3_f);
                tmp_int = cm_rndz<int>(acc_f);
                tmp_int = cm_max<int>(tmp_int, 0);
                tmp_u16 = cm_min<int>(tmp_int, max_input_level);
                v_Addr_dw = tmp_u16;
                read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
                out.select<1,1,16,4>(i,1) = (rd_bufferDataIn << shift_amount);
            }
        }

        if ( B == 1 )
        {
            #pragma unroll
            for(int i = 0; i < 8; i++) 
            {
                tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2-2)  ));
                tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2-2)+1));
                tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2-2)+2));
                acc_f   = cm_add<float>(tmp1_f, tmp2_f);
                acc_f   = cm_add<float>(acc_f , tmp3_f);
                tmp_int = cm_rndz<int>(acc_f);
                tmp_int = cm_max<int>(tmp_int, 0);
                tmp_u16 = cm_min<int>(tmp_int, max_input_level);
                v_Addr_dw = tmp_u16;
                read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
                out.select<1,1,16,4>(i,2) = (rd_bufferDataIn << shift_amount);
            }
        }
    }
}

_GENX_ void inline
GAMMA_CCM_ARGB16_OUT(SurfaceIndex BUF_R, SurfaceIndex BUF_G, SurfaceIndex BUF_B, SurfaceIndex ARGB16, SurfaceIndex LUTIndex,
                     int h_pos, int v_pos, int slmX, int bitdepth, int BayerType, int outFrameHeight, vector<float, 9> ccm, int EnableSLM)
{
	int shift_amount = 16 - bitdepth;
	ushort max_input_level = (1 << bitdepth) - 1;
    matrix<ushort,  8, 64> out;
	matrix<ushort, 24, 16> rd_in;
	vector<ushort, 16> tmp_u16, v_Addr, rd_slmDataIn; //vector<uint,   16> rd_slmDataIn;   // for 3.0 compiler which doesn't support word granularity

	vector<uint  , 16> v_Addr_dw, rd_bufferDataIn;
	vector<float , 16> acc_f, tmp1_f, tmp2_f, tmp3_f, in_f;
	vector<int   , 16> tmp_int;

    read(BUF_R, h_pos, v_pos, rd_in.select<8,1,16,1>(0, 0));
	read(BUF_G, h_pos, v_pos, rd_in.select<8,1,16,1>(8, 0));
	read(BUF_B, h_pos, v_pos, rd_in.select<8,1,16,1>(16,0));

	if(EnableSLM == 1)
    {

#pragma unroll
		for(int k = 0; k < 3; k++) {	

#pragma unroll
			for(int i = 0; i < 8; i++) {
				tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2-k)  ));
				tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2-k)+1));
				tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2-k)+2));
				acc_f   = cm_add<float>(tmp1_f, tmp2_f);
				acc_f   = cm_add<float>(acc_f , tmp3_f);
				tmp_int = cm_rndz<int>(acc_f);
				tmp_int = cm_max<int>(tmp_int, 0);
				tmp_u16 = cm_min<int>(tmp_int, max_input_level);
				v_Addr  = tmp_u16;
				cm_slm_read(slmX, v_Addr, rd_slmDataIn); 
				out.select<1,1,16,4>(i,k) = (rd_slmDataIn << shift_amount);
    }
}

	}
	else


    {

#pragma unroll
		for(int k = 0; k < 3; k++) {	

#pragma unroll
			for(int i = 0; i < 8; i++) {
				tmp1_f  = cm_mul<float>(rd_in.row(   i), ccm(3*(2-k)  ));
				tmp2_f  = cm_mul<float>(rd_in.row(8 +i), ccm(3*(2-k)+1));
				tmp3_f  = cm_mul<float>(rd_in.row(16+i), ccm(3*(2-k)+2));
				acc_f   = cm_add<float>(tmp1_f, tmp2_f);
				acc_f   = cm_add<float>(acc_f , tmp3_f);
				tmp_int = cm_rndz<int>(acc_f);
				tmp_int = cm_max<int>(tmp_int, 0);				 
				tmp_u16 = cm_min<int>(tmp_int, max_input_level);
				v_Addr_dw = tmp_u16;
				read(LUTIndex, 0, v_Addr_dw, rd_bufferDataIn);
				out.select<1,1,16,4>(i,k) = (rd_bufferDataIn << shift_amount);
}

    //cm_slm_init(65536);
    //uint slmX = cm_slm_alloc(65536);

    //ENABLE LOOK-UP TABLE COMPUTING




    //int loop_count = ((load_elements_per_thread % 16) == 0)? (load_elements_per_thread / 16) : ((load_elements_per_thread / 16) + 1);




        }

        //vector<uint, 16> i_plus_1;


        //c_i_plus_1.merge( max_input_level,Correct.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));
        //p_i_plus_1.merge( max_input_level,Point  .iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));







        //cm_slm_write(slmX, v_in, v_out);
    }


    /////////////////////////////////////////////////////////////////////////////////
    //////////////////  LOOK-UP TABLE PREPARED   ////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////


                //EACH thread in the threadgroup handle 256 byte-wide data
#pragma unroll
                for(int i = 0; i < 8; i++)
        out.select<1,1,16,4>(i,3) = 0;
	



	vector<ushort, 64> tmp;
	if(BayerType == GRBG || BayerType == GBRG) {



                //EACH thread in the threadgroup handle 256 byte-wide data
#pragma unroll
		for(int j = 0; j < 4; j++) {
			tmp = out.row(j);
			out.row(j) = out.row(7-j);
			out.row(7-j) = tmp;
                }

        }

	if(BayerType == GRBG || BayerType == GBRG)
		v_pos = (outFrameHeight - 16) - v_pos + 8;

    write(ARGB16, h_pos*4   , v_pos, out.select<8,1,16,1>(0,0 ));
    write(ARGB16, h_pos*4+32, v_pos, out.select<8,1,16,1>(0,16));
	write(ARGB16, h_pos*4+64, v_pos, out.select<8,1,16,1>(0,32));
    write(ARGB16, h_pos*4+96, v_pos, out.select<8,1,16,1>(0,48));
}

extern "C" _GENX_MAIN_ void
CCM_AND_GAMMA(SurfaceIndex Correct_Index_R, SurfaceIndex Correct_Index_G, SurfaceIndex Correct_Index_B,
              SurfaceIndex Point_Index,
              SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
              vector<float, 9> ccm,
              int blocks_in_a_row, int BitDepth, int framewidth_in_bytes, int frameheight,
              SurfaceIndex LUT_Index_R, SurfaceIndex LUT_Index_G, SurfaceIndex LUT_Index_B,
              int Enable_ARGB8, SurfaceIndex ARGB_Index, int BayerType)
{
    cm_slm_init(65536);
    uint slmX = cm_slm_alloc(65536);

    int enableSLM = (BitDepth < 16)? 1 : 0;
    LOAD_LUT(Correct_Index_R, Point_Index, slmX, LUT_Index_R, BitDepth, enableSLM);

    ///////////////////////////////////////////////////////////////////////////////////////////////////

    int framewidth		= framewidth_in_bytes / 4;
    int threadswidth	= (framewidth % 16 == 0)? (framewidth / 16) : ((framewidth / 16) + 1);
    int threadheight	= (frameheight % 8 == 0)? (frameheight / 8) : ((frameheight / 8) + 1);
    int threadspacesize = threadswidth * threadheight;

    int T_id = cm_linear_global_id();

    int Total_Thread_Num = cm_linear_global_size();
    int loop_count = (threadspacesize % Total_Thread_Num == 0)?
                     (threadspacesize / Total_Thread_Num) : ((threadspacesize / Total_Thread_Num) + 1);

    if(Enable_ARGB8 == 1)
    {
        for(int k = 0; k < loop_count; k++)
        {
            int rd_h_pos = (T_id % threadswidth) * 16 * 2; // position in units of bytes
            int rd_v_pos = (T_id / threadswidth) * 8 ;     // position in units of bytes

            GAMMA_CCM_ARGB8_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUT_Index_R,
                                rd_h_pos, rd_v_pos, slmX, BitDepth, BayerType, frameheight, ccm, enableSLM);

            T_id += cm_linear_global_size();
        }
    }
    else
    {
        for(int k = 0; k < loop_count; k++)
        {
            //EACH thread in the threadgroup handle 256 byte-wide data
            int rd_h_pos = (T_id % threadswidth) * 16 * 2; // position in units of bytes
            int rd_v_pos = (T_id / threadswidth) * 8 ;		// position in units of bytes
            GAMMA_CCM_ARGB16_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUT_Index_R,
                                 rd_h_pos, rd_v_pos, slmX, BitDepth, BayerType, frameheight, ccm, enableSLM);                
            T_id += cm_linear_global_size();
        }
    }
}

extern "C" _GENX_MAIN_ void
CCM_AND_GAMMA_3D(SurfaceIndex Correct_Index_R, SurfaceIndex Correct_Index_G, SurfaceIndex Correct_Index_B,
                 SurfaceIndex Point_Index,
                 SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
                 vector<float, 9> ccm,
                 int blocks_in_a_row, int BitDepth, int framewidth_in_bytes, int frameheight,
                 SurfaceIndex LUT_Index_R, SurfaceIndex LUT_Index_G, SurfaceIndex LUT_Index_B,
                 int Enable_ARGB8, SurfaceIndex ARGB_Index, int BayerType)
{
    cm_slm_init(65536);
    uint slmX = cm_slm_alloc(65536);
    matrix<uchar, 8, 64>   out;
    matrix<ushort, 8,  64> out16;
    int enableSLM = (BitDepth < 16)? 1 : 0;

    ///////////////////////////////////////////////////////////////////////////////////////////////////

    int framewidth		= framewidth_in_bytes / 4;
    int threadswidth	= (framewidth % 16 == 0)? (framewidth / 16) : ((framewidth / 16) + 1);
    int threadheight	= (frameheight % 8 == 0)? (frameheight / 8) : ((frameheight / 8) + 1);
    int threadspacesize = threadswidth * threadheight;

    int T_id = cm_linear_global_id();

    int Total_Thread_Num = cm_linear_global_size();
    int loop_count = (threadspacesize % Total_Thread_Num == 0)? 
                     (threadspacesize / Total_Thread_Num) : ((threadspacesize / Total_Thread_Num) + 1);

    if(Enable_ARGB8 == 1)
    {
        for(int k = 0; k < loop_count; k++)
        {
            int rd_h_pos = (T_id % threadswidth) * 16 * 2; // position in units of bytes
            int rd_v_pos = (T_id / threadswidth) * 8 ;     // position in units of bytes

            LOAD_LUT(Correct_Index_R, Point_Index, slmX, LUT_Index_R, BitDepth, enableSLM);
            GAMMA_3D_CCM_ARGB8_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUT_Index_R,
                                rd_h_pos, rd_v_pos, slmX, 1, 0, 0, BitDepth, BayerType, frameheight, ccm, enableSLM, out);

            LOAD_LUT(Correct_Index_G, Point_Index, slmX, LUT_Index_G, BitDepth, enableSLM);
            GAMMA_3D_CCM_ARGB8_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUT_Index_G,
                                rd_h_pos, rd_v_pos, slmX, 0, 1, 0, BitDepth, BayerType, frameheight, ccm, enableSLM, out);

            LOAD_LUT(Correct_Index_B, Point_Index, slmX, LUT_Index_B, BitDepth, enableSLM);
            GAMMA_3D_CCM_ARGB8_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUT_Index_B,
                                rd_h_pos, rd_v_pos, slmX, 0, 0, 1, BitDepth, BayerType, frameheight, ccm, enableSLM, out);

            T_id += cm_linear_global_size();

            #pragma unroll
            for(int i = 0; i < 8; i++)
                out.select<1,1,16,4>(i,3) = 0;

            vector<uchar, 64> tmp;
            if(BayerType == GRBG || BayerType == GBRG)
            {
                #pragma unroll
                for(int j = 0; j < 4; j++)
                {
                    tmp = out.row(j);
                    out.row(j) = out.row(7-j);
                    out.row(7-j) = tmp;
                }
            }

            if(BayerType == GRBG || BayerType == GBRG)
                rd_v_pos = (frameheight - 16) - rd_v_pos + 8;

            write(ARGB_Index, rd_h_pos*2   , rd_v_pos, out.select<8,1,32,1>(0,0 ));
            write(ARGB_Index, rd_h_pos*2+32, rd_v_pos, out.select<8,1,32,1>(0,32));
        }
    }
    else
    {
        for(int k = 0; k < loop_count; k++)
        {
            //EACH thread in the threadgroup handle 256 byte-wide data
            int rd_h_pos = (T_id % threadswidth) * 16 * 2; // position in units of bytes
            int rd_v_pos = (T_id / threadswidth) * 8 ;		// position in units of bytes
            LOAD_LUT(Correct_Index_R, Point_Index, slmX, LUT_Index_R, BitDepth, enableSLM);
            GAMMA_3D_CCM_ARGB16_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUT_Index_R,
                                 rd_h_pos, rd_v_pos, slmX, 1, 0 , 0, BitDepth, BayerType, frameheight, ccm, enableSLM, out16);

            LOAD_LUT(Correct_Index_G, Point_Index, slmX, LUT_Index_G, BitDepth, enableSLM);
            GAMMA_3D_CCM_ARGB16_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUT_Index_G,
                                 rd_h_pos, rd_v_pos, slmX, 0, 1, 0, BitDepth, BayerType, frameheight, ccm, enableSLM, out16);

            LOAD_LUT(Correct_Index_B, Point_Index, slmX, LUT_Index_B, BitDepth, enableSLM);
            GAMMA_3D_CCM_ARGB16_OUT(R_I_Index, G_I_Index, B_I_Index, ARGB_Index, LUT_Index_B,
                                 rd_h_pos, rd_v_pos, slmX, 0, 0, 1, BitDepth, BayerType, frameheight, ccm, enableSLM, out16);

            T_id += cm_linear_global_size();

            #pragma unroll
            for(int i = 0; i < 8; i++)
                out16.select<1,1,16,4>(i,3) = 0;

            vector<ushort, 64> tmp;
            if(BayerType == GRBG || BayerType == GBRG)
            {
                #pragma unroll
                for(int j = 0; j < 4; j++)
                {
                    tmp = out16.row(j);
                    out16.row(j) = out16.row(7-j);
                    out16.row(7-j) = tmp;
                }

            }

            if(BayerType == GRBG || BayerType == GBRG)
                rd_v_pos = (frameheight - 16) - rd_v_pos + 8;

            write(ARGB_Index, rd_h_pos*4   , rd_v_pos, out16.select<8,1,16,1>(0,0 ));
            write(ARGB_Index, rd_h_pos*4+32, rd_v_pos, out16.select<8,1,16,1>(0,16));
            write(ARGB_Index, rd_h_pos*4+64, rd_v_pos, out16.select<8,1,16,1>(0,32));
            write(ARGB_Index, rd_h_pos*4+96, rd_v_pos, out16.select<8,1,16,1>(0,48));
        }
    }
}

_GENX_ void inline
FINALIZE_ARGB8(matrix_ref<ushort, 8, 16> R, matrix_ref<ushort, 8, 16> G, matrix_ref<ushort, 8, 16> B,
			   matrix_ref<uchar , 8, 64> ARGB8,
			   vector_ref<float, 9> ccm, int BitDepth, int EnableCCM)
{
	vector<float, 16> acc_f, tmp1_f, tmp2_f, tmp3_f, in_f;




	vector<int  , 16> tmp_int;

	int m;
	int shift_amount = BitDepth - 8;
	ushort max_input_level = ((1 << BitDepth) - 1);
    //int loop_count = ((load_elements_per_thread % 16) == 0)? (load_elements_per_thread / 16) : ((load_elements_per_thread / 16) + 1);



	if(EnableCCM == 1)
    {
#pragma unroll
		for(int k = 0; k < 3; k++) {


#pragma unroll
			for(int j = 0; j < 8; j++) {
				tmp1_f  = cm_mul<float>(R.row(j), ccm((2-k)*3  ));
				tmp2_f  = cm_mul<float>(G.row(j), ccm((2-k)*3+1)); 
				tmp3_f  = cm_mul<float>(B.row(j), ccm((2-k)*3+2));





        //v_o /= (p_i_plus_1 - p_i);
				acc_f   = cm_add<float>(tmp1_f, tmp2_f);
				acc_f   = cm_add<float>(acc_f , tmp3_f);
				tmp_int = cm_rndz<int>(acc_f);

				tmp_int = cm_max<int>(tmp_int, 0);				 
				tmp_int = cm_min<int>(tmp_int, max_input_level);
				ARGB8.select<1,1,16,4>(j,k) = tmp_int >> shift_amount;

    }
        }
    }
	else

    {                                   // Four Groups, jointly, one time does 128 pixel high. Need (height/128) loops
		ARGB8.select<8,1,16,4>(0,0) = B >> shift_amount;
		ARGB8.select<8,1,16,4>(0,1) = G >> shift_amount;
		ARGB8.select<8,1,16,4>(0,2) = R >> shift_amount;
                }

}

extern "C" _GENX_MAIN_
void ARGB8(SurfaceIndex R_BUF, SurfaceIndex G_BUF, SurfaceIndex B_BUF,
           SurfaceIndex ARGB8,
		   int BayerType, int outFrameHeight, int BitDepth, int EnableCCM,
		   float coef0, float coef1, float coef2, float coef3, float coef4, float coef5, float coef6, float coef7, float coef8)
{
	cm_fsetround(CM_RTZ);
    int rd_h_pos  = (get_thread_origin_x() * 16) * sizeof(short);
    int rd_v_pos  = (get_thread_origin_y() * 16);

    int wr_h_pos  = (get_thread_origin_x() * 16) * (4 * sizeof(char));
    int wr_v_pos  = (get_thread_origin_y() * 16);

    if(BayerType == GRBG || BayerType == GBRG)
        wr_v_pos = (outFrameHeight - 16) - wr_v_pos;

    int shift_amount = BitDepth - 8;

    matrix<ushort, 8, 16> in_R, in_G, in_B;
    matrix<uchar , 16, 64> out;
	vector<float, 9> ccm_matrix;

	ccm_matrix(0) = coef0; 	ccm_matrix(1) = coef1; 	ccm_matrix(2) = coef2; 	ccm_matrix(3) = coef3; ccm_matrix(4) = coef4;
	ccm_matrix(5) = coef5; 	ccm_matrix(6) = coef6; 	ccm_matrix(7) = coef7; 	ccm_matrix(8) = coef8;
    read(R_BUF, rd_h_pos, rd_v_pos  , in_R);
	read(G_BUF, rd_h_pos, rd_v_pos  , in_G);

	read(B_BUF, rd_h_pos, rd_v_pos  , in_B);
	FINALIZE_ARGB8(in_R, in_G, in_B, out.select<8,1,64,1>(0,0), ccm_matrix, BitDepth, EnableCCM);
    read(R_BUF, rd_h_pos, rd_v_pos+8, in_R);

	read(G_BUF, rd_h_pos, rd_v_pos+8, in_G);
	read(B_BUF, rd_h_pos, rd_v_pos+8, in_B);

	FINALIZE_ARGB8(in_R, in_G, in_B, out.select<8,1,64,1>(8,0), ccm_matrix, BitDepth, EnableCCM);

    out.select<16,1,16,4>(0,3) = 0;

    vector<uchar, 64> tmp;
    if(BayerType == GRBG || BayerType == GBRG)
    {
#pragma unroll
        for(int j = 0; j < 8; j++)
        {
            tmp = out.row(j);
            out.row(j) = out.row(15-j);
            out.row(15-j) = tmp;
        }
    }

    write(ARGB8, wr_h_pos   , wr_v_pos  , out.select<8,1,32,1>(0, 0));
    write(ARGB8, wr_h_pos+32, wr_v_pos  , out.select<8,1,32,1>(0,32));
    write(ARGB8, wr_h_pos   , wr_v_pos+8, out.select<8,1,32,1>(8, 0));
    write(ARGB8, wr_h_pos+32, wr_v_pos+8, out.select<8,1,32,1>(8,32));
}


_GENX_ void inline
FINALIZE_ARGB16(matrix_ref<ushort, 8, 16> R, matrix_ref<ushort, 8, 16> G, matrix_ref<ushort, 8, 16> B,
			    matrix_ref<ushort, 8, 64> ARGB16,
			    vector_ref<float, 9> ccm, int BitDepth, int EnableCCM)
{
	vector<float, 16> acc_f, tmp1_f, tmp2_f, tmp3_f, in_f;
	vector<int  , 16> tmp_int;
	int shift_amount = 16 - BitDepth;
	ushort max_input_level = ((1 << BitDepth) - 1);
	if(EnableCCM == 1)
	{
#pragma unroll
		for(int k = 0; k < 3; k++) {
#pragma unroll
			for(int j = 0; j < 8; j++) {
				tmp1_f  = cm_mul<float>(R.row(j), ccm((2-k)*3  ));
				tmp2_f  = cm_mul<float>(G.row(j), ccm((2-k)*3+1)); 
				tmp3_f  = cm_mul<float>(B.row(j), ccm((2-k)*3+2));
				acc_f   = cm_add<float>(tmp1_f, tmp2_f);
				acc_f   = cm_add<float>(acc_f , tmp3_f);
				tmp_int = cm_rndz<int>(acc_f);
				tmp_int = cm_max<int>(tmp_int, 0);				 
				tmp_int = cm_min<int>(tmp_int, max_input_level);
				ARGB16.select<1,1,16,4>(j,k) = tmp_int << shift_amount;
			}
		}
	}
	else
	{
		ARGB16.select<8,1,16,4>(0,0) = B << shift_amount;
		ARGB16.select<8,1,16,4>(0,1) = G << shift_amount;
		ARGB16.select<8,1,16,4>(0,2) = R << shift_amount;
	}
}
extern "C" _GENX_MAIN_
void ARGB16(SurfaceIndex R_BUF, SurfaceIndex G_BUF, SurfaceIndex B_BUF,
            SurfaceIndex ARGB16,
		    int BayerType, int outFrameHeight, int BitDepth, int EnableCCM,
		    float coef0, float coef1, float coef2, float coef3, float coef4, float coef5, float coef6, float coef7, float coef8)
{
	cm_fsetround(CM_RTZ);
    int rd_h_pos  = (get_thread_origin_x() * 16) * sizeof(short);
    int rd_v_pos  = (get_thread_origin_y() * 16);

    int wr_h_pos  = (get_thread_origin_x() * 16) * (4 * sizeof(short));
    int wr_v_pos  = (get_thread_origin_y() * 16);

    if(BayerType == GRBG || BayerType == GBRG)
        wr_v_pos = (outFrameHeight - 16) - wr_v_pos;

    int shift_amount = BitDepth - 8;
    matrix<ushort, 8, 16> in_R, in_G, in_B;
    matrix<ushort, 16, 64> out;
	vector<float, 9> ccm_matrix;

	ccm_matrix(0) = coef0; 	ccm_matrix(1) = coef1; 	ccm_matrix(2) = coef2; 	ccm_matrix(3) = coef3; ccm_matrix(4) = coef4;
	ccm_matrix(5) = coef5; 	ccm_matrix(6) = coef6; 	ccm_matrix(7) = coef7; 	ccm_matrix(8) = coef8;
    read(R_BUF, rd_h_pos, rd_v_pos  , in_R);
	read(G_BUF, rd_h_pos, rd_v_pos  , in_G);

	read(B_BUF, rd_h_pos, rd_v_pos  , in_B);
	FINALIZE_ARGB16(in_R, in_G, in_B, out.select<8,1,64,1>(0,0), ccm_matrix, BitDepth, EnableCCM);
    read(R_BUF, rd_h_pos, rd_v_pos+8, in_R);

	read(G_BUF, rd_h_pos, rd_v_pos+8, in_G);
	read(B_BUF, rd_h_pos, rd_v_pos+8, in_B);

	FINALIZE_ARGB16(in_R, in_G, in_B, out.select<8,1,64,1>(8,0), ccm_matrix, BitDepth, EnableCCM);

    out.select<16,1,16,4>(0,3) = 0;

    // SHIFT To MSB side

    vector<ushort, 64> tmp;
    if(BayerType == GRBG || BayerType == GBRG)
    {
#pragma unroll
        for(int j = 0; j < 8; j++)
        {
            tmp = out.row(j);
            out.row(j) = out.row(15-j);
            out.row(15-j) = tmp;
        }
    }

    write(ARGB16, wr_h_pos   , wr_v_pos  , out.select<8,1,16,1>(0, 0));
    write(ARGB16, wr_h_pos+32, wr_v_pos  , out.select<8,1,16,1>(0,16));
    write(ARGB16, wr_h_pos+64, wr_v_pos  , out.select<8,1,16,1>(0,32));
    write(ARGB16, wr_h_pos+96, wr_v_pos  , out.select<8,1,16,1>(0,48));

    write(ARGB16, wr_h_pos   , wr_v_pos+8, out.select<8,1,16,1>(8, 0));
    write(ARGB16, wr_h_pos+32, wr_v_pos+8, out.select<8,1,16,1>(8,16));
    write(ARGB16, wr_h_pos+64, wr_v_pos+8, out.select<8,1,16,1>(8,32));
    write(ARGB16, wr_h_pos+96, wr_v_pos+8, out.select<8,1,16,1>(8,48));
}

#ifndef BLOCK_WIDTH
#define BLOCK_WIDTH 8
#endif

#ifndef BLOCK_HEIGHT
#define BLOCK_HEIGHT 8
#endif

const short offset_x[BLOCK_HEIGHT][BLOCK_WIDTH] = {{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7}
                                                  ,{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7}};
const short offset_y[BLOCK_HEIGHT][BLOCK_WIDTH] = {{0,0,0,0,0,0,0,0},{1,1,1,1,1,1,1,1},{2,2,2,2,2,2,2,2},{3,3,3,3,3,3,3,3}
                                                  ,{4,4,4,4,4,4,4,4},{5,5,5,5,5,5,5,5},{6,6,6,6,6,6,6,6},{7,7,7,7,7,7,7,7}};
extern "C" _GENX_MAIN_
void GenVMask(SurfaceIndex MaskIndex,
              SurfaceIndex Mask4x4Index,
              int Width, int Height,  //float angle,
              float MAX_ANGLE, int effect)
{
    short img_center_x = (short)(Width / 2);
    short img_center_y = (short)(Height / 2);
    float max_ImgRad = cm_sqrt((float)(Width/2)*(float)(Width/2) + (float)(Height/2)*(float)(Height/2));

    //float max_ImgRad = sqrt( pow((double)Width/2, 2)+pow((double)Height/2, 2) );
    //float MAX_ANGLE = 3.14159 / (180/(angle));

    //double Distance;
    //double DistanceRatio;
    //matrix<float, BLOCK_HEIGHT, BLOCK_WIDTH> DistRatio;
    //double EffectiveScale;

    matrix<ushort, BLOCK_HEIGHT, BLOCK_WIDTH> MaskMatrix;
    matrix<ushort, BLOCK_HEIGHT/4, BLOCK_WIDTH/4> Mask4x4Matrix;

    matrix<float, BLOCK_HEIGHT, BLOCK_WIDTH> Distance, EffectScale;

    matrix<short, BLOCK_HEIGHT, BLOCK_WIDTH> x_cord;             //matrix<float, BLOCK_HEIGHT, BLOCK_WIDTH> x_cord;
    matrix<short, BLOCK_HEIGHT, BLOCK_WIDTH> x_offset(offset_x); //matrix<float, BLOCK_HEIGHT, BLOCK_WIDTH> x_offset(offset_x);
    matrix<short, BLOCK_HEIGHT, BLOCK_WIDTH> y_offset(offset_y); //matrix<float, BLOCK_HEIGHT, BLOCK_WIDTH> y_offset(offset_y);

    uint h_pos = get_thread_origin_x() * BLOCK_WIDTH * sizeof(short);;
    uint v_pos = get_thread_origin_y();

    x_cord = x_offset + (short)(h_pos/sizeof(short));

    int dummy =  0;
    for(int rowpos = 0 ; rowpos < Height ; rowpos += BLOCK_HEIGHT)
    {
        matrix<short, BLOCK_HEIGHT, BLOCK_WIDTH> y_cord; //matrix<float, BLOCK_HEIGHT, BLOCK_WIDTH> y_cord;

        y_cord =  y_offset + (short)rowpos;
        matrix<short, BLOCK_HEIGHT, BLOCK_WIDTH> dx = x_cord - img_center_x;
        matrix<short, BLOCK_HEIGHT, BLOCK_WIDTH> dy = y_cord - img_center_y;

         Distance = dx * dx + dy * dy;
        Distance = cm_sqrt(Distance);
        Distance /= max_ImgRad;

        if (effect == 0)
        {
            EffectScale = cm_cos(Distance * MAX_ANGLE);
            EffectScale = cm_pow(EffectScale ,4);
            EffectScale = EffectScale * (float)(1024.0) + (float)0.5;
        }
        else if(effect == 1)
        {
            EffectScale = cm_cos(Distance * MAX_ANGLE);
            EffectScale = cm_pow(EffectScale , -4);
            EffectScale = EffectScale * (float)(256.0) + (float)0.5;
        }
        else
            EffectScale = 0;

        EffectScale.merge(4095, EffectScale > 4095);
        EffectScale.merge(0, EffectScale < 0);

        MaskMatrix = EffectScale;
        Mask4x4Matrix = MaskMatrix.select<BLOCK_HEIGHT/4, 4, BLOCK_WIDTH/4, 4>(0,0);

        write(MaskIndex, h_pos, rowpos, MaskMatrix);
        write(Mask4x4Index, h_pos/4, rowpos/4, Mask4x4Matrix);
    }
}

#define BLOCKHEIGHT 4
#define BLOCKWIDTH 4
const short modXini[BLOCKHEIGHT*BLOCKWIDTH] = {0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3};
const short four_minus_modXini[BLOCKHEIGHT*BLOCKWIDTH] = {4,3,2,1,4,3,2,1,4,3,2,1,4,3,2,1};
const short modY_ini[BLOCKHEIGHT][BLOCKWIDTH] = {{0,0,0,0},{1,1,1,1},{2,2,2,2},{3,3,3,3}};

extern "C" _GENX_MAIN_
void GenUpSampledMask(SurfaceIndex Mask4x4Index,
                      SurfaceIndex UpSampleMaskIndex,
                      int Width,   // Width  of Output Mask surface
                      int Height)  // Height of Output Mask surface
{
     //In a single 16x16 block the modX, four_minus_modX, modY, four_minus_modY are the same
     vector<ushort, 16> modX(modXini);
     vector<ushort, 16> four_minus_modX(four_minus_modXini);
     vector<ushort, 16> modY;
     vector<ushort, 16> four_minus_modY;
     matrix<ushort, 16, 16> output_Matrix;
     matrix<ushort, 5, 6> data5x5;

     uint h_pos = get_thread_origin_x();
     uint v_pos = get_thread_origin_y();
     h_pos = h_pos * BLOCKWIDTH * 4 * sizeof(short);

     for(int rowpos = 0; rowpos < Height; rowpos += (BLOCKHEIGHT*4))    //loop over 'column' of blocks of size 16x16
     {
            //Read 5x5 data from Mask4x4Index surface
            read(Mask4x4Index, h_pos/4, rowpos/4, data5x5);

            //Generate a4x4, b4x4, c4x4, d4x4 matrices for computing the 16x16 output matrix
            matrix<ushort, 4, 4> a4x4, b4x4, c4x4, d4x4;
            a4x4 = data5x5.select<4,1,4,1>(0,0);
            b4x4 = data5x5.select<4,1,4,1>(0,1);
            c4x4 = data5x5.select<4,1,4,1>(1,0);
            d4x4 = data5x5.select<4,1,4,1>(1,1);

            if(h_pos == (Width - 16) * sizeof(short))
            {
                b4x4.column(3) = b4x4.column(2);
                d4x4.column(3) = d4x4.column(2);
            }
            if(rowpos == (Height - 4))
            {
                c4x4.row(3) = c4x4.row(2);
                d4x4.row(3) = d4x4.row(2);
            }

            //Use a16, b16, c16, d16 to compute a row (1x16) of the 16x16 output matrix
            vector<ushort, 16> a16, b16, c16, d16;
            vector<ushort, 16> op1, op2;
#pragma unroll
            for(int j = 0; j < 4; j++)
            {
                a16.select<4,1>(0) = a4x4(j,0); a16.select<4,1>(4) = a4x4(j,1); a16.select<4,1>(8) = a4x4(j,2); a16.select<4,1>(12) = a4x4(j,3);
                b16.select<4,1>(0) = b4x4(j,0); b16.select<4,1>(4) = b4x4(j,1); b16.select<4,1>(8) = b4x4(j,2); b16.select<4,1>(12) = b4x4(j,3);
                c16.select<4,1>(0) = c4x4(j,0); c16.select<4,1>(4) = c4x4(j,1); c16.select<4,1>(8) = c4x4(j,2); c16.select<4,1>(12) = c4x4(j,3);
                d16.select<4,1>(0) = d4x4(j,0); d16.select<4,1>(4) = d4x4(j,1); d16.select<4,1>(8) = d4x4(j,2); d16.select<4,1>(12) = d4x4(j,3);

                op1 = ( a16 * (four_minus_modX) + b16 * (modX) + 2) / 4;
                op2 = ( c16 * (four_minus_modX) + d16 * (modX) + 2) / 4;
#pragma unroll
                for(int i = 0; i < 4; i++)
                {
                    modY = i;
                    four_minus_modY = (4 - i);
                    vector<ushort, 16> output_row;
                    output_row  = (four_minus_modY) * (op1);
                    output_row += (modY) * (op2) + 2;
                    output_row /= 4;
                    output_Matrix.row(i+j*4) = output_row;
                }
            }

            //after the loops are done, the 16x16 output matrix is ready to be written to the output surface
            write(UpSampleMaskIndex, h_pos, rowpos,     output_Matrix.select<8, 1, 16, 1>(0,0));
            write(UpSampleMaskIndex, h_pos, rowpos + 8, output_Matrix.select<8, 1, 16, 1>(8,0));
     }

}





const short x_init[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

extern "C" _GENX_MAIN_ void
Bulge_8bits(SamplerIndex SamplerConfig,
            SurfaceIndex ibuf,         //input  surface in R8G8B8A8 FMT
            SurfaceIndex obuf,           //output surface in R8G8B8A8 FMT.
            int frame_width,
            int frame_height, float a, float b)
{
    int wr_hpos_pix = get_thread_origin_x() * 16;
    int wr_vpos_pix = get_thread_origin_y() * 16;   // total number of threads is (FrameWidth / 16) x (FrameHeight/16).
                                                    // since we assign every thread to render 16x16 R8G8B8A8 pixels (64 x 16 bytes)
    matrix<uchar, 16, 64> data_to_be_written;
    matrix<float, 4, 16> m;
    vector<short, 16> x_tmp(x_init);
    vector<float, 16> x, y;
    vector<float, 16> out_x, out_y;
    vector<float, 16> r, r_xcomponent;

    float width_reciproc  = (1/(float)frame_width );
    float height_reciproc = (1/(float)frame_height);
    float x_c = (frame_width  >> 1) - 0.5f;
    float y_c = (frame_height >> 1) - 0.5f;

    x  = x_tmp + wr_hpos_pix;
    x -= x_c;
    x *= width_reciproc;

    r_xcomponent =  cm_mul<float>(x, x);

#pragma unroll
    for(int j = 0; j < 16; j++)
    {
        //initializing coordinate grid (with pixel units)
        y = j + wr_vpos_pix;
        y = y - y_c;//y_c - y;
        y *= height_reciproc;

        //converting to polar coordinate
        r =  r_xcomponent;
        r += cm_mul<float>(y, y);
        r = cm_sqrt(r);

        vector<float, 16> r_ratio;
        //r_ratio = cm_exp(cm_mul<float>(r,1.0887f)) - 1;
        r_ratio = cm_pow(r, b);
        r_ratio *= a;

        r_ratio *= cm_inv(r);

        //converting back to 'normalized' cartesian coordinate
        out_x = cm_mul<float>(x, r_ratio) + 0.5f;
        out_y = cm_mul<float>(y, r_ratio) + 0.5f;

        sample16(m, CM_BGR_ENABLE, ibuf, SamplerConfig, out_x, out_y);
        m = cm_mul<float>(m, 255.0f);

        data_to_be_written.select<1,1,16,4>(j,2) = m.row(0);
        data_to_be_written.select<1,1,16,4>(j,1) = m.row(1);
        data_to_be_written.select<1,1,16,4>(j,0) = m.row(2);
    }

    write(obuf, wr_hpos_pix*4     , wr_vpos_pix, data_to_be_written.select<16,1,16,1>(0,0 ));
    write(obuf, wr_hpos_pix*4 + 16, wr_vpos_pix, data_to_be_written.select<16,1,16,1>(0,16));
    write(obuf, wr_hpos_pix*4 + 32, wr_vpos_pix, data_to_be_written.select<16,1,16,1>(0,32));
    write(obuf, wr_hpos_pix*4 + 48, wr_vpos_pix, data_to_be_written.select<16,1,16,1>(0,48));
}

extern "C" _GENX_MAIN_ void
Bulge_16bits(SamplerIndex SamplerConfig,
             SurfaceIndex ibuf,        //input  surface in R16G16B16A16 FMT
             SurfaceIndex obuf,           //output surface in R16G16B16A16 FMT.
             int frame_width,
             int frame_height, float a, float b)
{
    int wr_hpos_pix = get_thread_origin_x() * 16;
    int wr_vpos_pix = get_thread_origin_y() * 16;   // total number of threads is (FrameWidth / 16) x (FrameHeight/16).
                                                    // since we assign every thread to render 16x16 R8G8B8A8 pixels (64 x 16 bytes)
    matrix<ushort, 16, 64> data_to_be_written;      // 16bpp, thus use ushort type
    matrix<float, 4, 16> m;
    vector<short, 16> x_tmp(x_init);
    vector<float, 16> x, y;
    vector<float, 16> out_x, out_y;
    vector<float, 16> r, r_xcomponent;

    float width_reciproc  = (1/(float)frame_width );
    float height_reciproc = (1/(float)frame_height);
    float x_c = (frame_width  >> 1) - 0.5f;
    float y_c = (frame_height >> 1) - 0.5f;

    x  = x_tmp + wr_hpos_pix;
    x -= x_c;
    x *= width_reciproc;

    r_xcomponent =  cm_mul<float>(x, x);

#pragma unroll
    for(int j = 0; j < 16; j++)
    {
        //initializing coordinate grid (with pixel units)
        y = j + wr_vpos_pix;
        y = y - y_c;//y_c - y;
        y *= height_reciproc;

        //converting to polar coordinate
        r =  r_xcomponent;
        r += cm_mul<float>(y, y);
        r = cm_sqrt(r);

        vector<float, 16> r_ratio;
        //r_ratio = cm_exp(cm_mul<float>(r,1.0887f)) - 1;
        r_ratio = cm_pow(r, b);
        r_ratio *= a;

        r_ratio *= cm_inv(r);

        //converting back to 'normalized' cartesian coordinate
        out_x = cm_mul<float>(x, r_ratio) + 0.5f;
        out_y = cm_mul<float>(y, r_ratio) + 0.5f;

        sample16(m, CM_ABGR_ENABLE, ibuf, SamplerConfig, out_x, out_y);
        m = cm_mul<float>(m, 1023.0f);//255.0f);

        data_to_be_written.select<1,1,16,4>(j,2) = m.row(1);
        data_to_be_written.select<1,1,16,4>(j,1) = m.row(2);
        data_to_be_written.select<1,1,16,4>(j,0) = m.row(3);
    }

    write(obuf, wr_hpos_pix*8 + 0, wr_vpos_pix  , data_to_be_written.select<8,1,16,1>(0,0 ));
    write(obuf, wr_hpos_pix*8 + 0, wr_vpos_pix+8, data_to_be_written.select<8,1,16,1>(8,0 ));

    write(obuf, wr_hpos_pix*8 +32, wr_vpos_pix  , data_to_be_written.select<8,1,16,1>(0,16));
    write(obuf, wr_hpos_pix*8 +32, wr_vpos_pix+8, data_to_be_written.select<8,1,16,1>(8,16));

    write(obuf, wr_hpos_pix*8 +64, wr_vpos_pix  , data_to_be_written.select<8,1,16,1>(0,32));
    write(obuf, wr_hpos_pix*8 +64, wr_vpos_pix+8, data_to_be_written.select<8,1,16,1>(8,32));

    write(obuf, wr_hpos_pix*8+ 96, wr_vpos_pix  , data_to_be_written.select<8,1,16,1>(0,48));
    write(obuf, wr_hpos_pix*8+ 96, wr_vpos_pix+8, data_to_be_written.select<8,1,16,1>(8,48));
}

