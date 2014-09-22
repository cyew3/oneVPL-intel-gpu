//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2005-2014 Intel Corporation. All Rights Reserved.
//
//
//*/
#include <cm.h>


// constant from B-spec
#define big_pix_th          40

#define m_Good_Pixel_Th      5
#define good_pix_th          5

#define m_Num_Big_Pix_TH    35
#define num_big_pix_th      35

#define num_good_pix_th     10
#define m_Good_Intensity_TH 10

#define good_intensity_TH   10
#define m_Num_Good_Pix_TH   10

#define AVG_True             2
#define AVG_False            1
#define Grn_imbalance_th     0
#define Average_Color_TH   100

// other constant
#define WD     16
#define WIDTH   8
#define W      16

#define BGGR 0
#define RGGB 1
#define GBRG 2
#define GRBG 3

// implementation


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
ROWSWAP_8x16(matrix_ref<short, 8, 16> m)
{
    vector<short, 16> tmp_pixel_row;
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
ROWSWAP_20x32(matrix_ref<ushort, 20, 32> m)
{
    vector<ushort, 32> tmp_pixel_row;
#pragma unroll
    for(int i = 0; i < 10; i++ )
    {
        tmp_pixel_row = m.row(i);
        m.row(i)      = m.row(19-i);
        m.row(19-i)   = tmp_pixel_row;
    }
}

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
                rd_h_pos = sizeof(short);
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
            if(BayerType == GRBG || BayerType == GBRG)
            {
                ROWSWAP_8x8(rd_in_pixels);
            }

            // ALIGN TO LSB
            rd_in_pixels >>= (16-bitDepth);

            // FLIP RIGHT TO LEFT, IF ON LEFT OR RIGHT BOUNDARY
            if( wr_h_pos == 0 | wr_h_pos == (threadwidth - 1) * 8 * sizeof(short))
            {
#pragma unroll
                for(int j = 0; j < 8; j++) {
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
            // Flip
            if(BayerType == GRBG || BayerType == GBRG)
            {
                write(obuf, wr_h_pos, (InFrameHeight + 16 - wr_v_pos) - 8, out);
            } else {
                write(obuf, wr_h_pos, wr_v_pos, out);
            }
        }
    }

}

_GENX_ void inline
COMP_ACC0_ACC1(matrix_ref<ushort, 8,  16> premask, matrix_ref<uchar , 12, 32> src,
               matrix_ref<uchar , 16, 16> acc0   , matrix_ref<uchar , 16, 16> acc1,
               int y, int x, int y_offset)
{
    matrix< ushort, 8, 16 > diff, pixmask;

    pixmask = premask | (src.select<8,1,16,1>(y,x) <= good_intensity_TH);
    diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,16,1>(y,x), - src.select<8,1,16,1>(2,2)));
    diff.merge(good_pix_th, pixmask);

    acc0.select<8,1,16,1>(y_offset,0) += (diff > good_pix_th);
    acc1.select<8,1,16,1>(y_offset,0) += (diff >  big_pix_th);
}

extern "C" _GENX_MAIN_ void
GOOD_PIXEL_CHECK(SurfaceIndex ibuf , 
                 SurfaceIndex GSwap_bitshifted_buf, 
                 SurfaceIndex obuf0, SurfaceIndex obuf1,
                 int frameHeight,
                 int shift,
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

    if(firstflag == 0)
    {
        read(ibuf, rd_h_pos   , rd_v_pos   , rd_in.select<8,1,16,1>(0, 0 ));
        read(ibuf, rd_h_pos+32, rd_v_pos   , rd_in.select<8,1,16,1>(0, 16));
        read(ibuf, rd_h_pos   , rd_v_pos+8 , rd_in.select<8,1,16,1>(8, 0 ));
        read(ibuf, rd_h_pos+32, rd_v_pos+8 , rd_in.select<8,1,16,1>(8, 16));
        read(ibuf, rd_h_pos   , rd_v_pos+16, rd_in.select<4,1,16,1>(16,0 ));
        read(ibuf, rd_h_pos+32, rd_v_pos+16, rd_in.select<4,1,16,1>(16,16));
    }
    else
    {
        read(ibuf, rd_h_pos   , rd_v_pos   , rd_in.select<8,1,16,1>(0, 0 ));
        read(ibuf, rd_h_pos+32, rd_v_pos   , rd_in.select<8,1,16,1>(0, 16));
        read(ibuf, rd_h_pos   , rd_v_pos+8 , rd_in.select<8,1,16,1>(8, 0 ));
        read(ibuf, rd_h_pos+32, rd_v_pos+8 , rd_in.select<8,1,16,1>(8, 16));
        read(ibuf, rd_h_pos   , rd_v_pos+16, rd_in.select<4,1,16,1>(16,0 ));
        read(ibuf, rd_h_pos+32, rd_v_pos+16, rd_in.select<4,1,16,1>(16,16));
        if(BayerType == GRBG || BayerType == GBRG)
        {
            ROWSWAP_20x32(rd_in);
            wr_v_pos = (frameHeight + 16 - wr_v_pos) - 16;
        }
        rd_in >>= (16-bitDepth);
        write(GSwap_bitshifted_buf, (wr_h_pos*2), wr_v_pos  , rd_in.select<8,1,16,1>(2 ,2));
        write(GSwap_bitshifted_buf, (wr_h_pos*2), wr_v_pos+8, rd_in.select<8,1,16,1>(10,2));
    }
    ///////////   1ST 8x16 BLOCK  /////////////////////////////////////////////////////////////////////
    ///// ACCORDING TO B-SPEC: Precision: 16-bit pixels truncated to 8-bits before subtract & compares.
    src     = (rd_in.select<12,1,32,1>(0,0) >> (short)(shift));
    premask = (src.select<8,1,16,1>(2,2) <= good_intensity_TH);
    
    COMP_ACC0_ACC1(premask, src, acc0, acc1, 0, 2, 0);
    COMP_ACC0_ACC1(premask, src, acc0, acc1, 2, 0, 0);
    COMP_ACC0_ACC1(premask, src, acc0, acc1, 4, 2, 0);
    COMP_ACC0_ACC1(premask, src, acc0, acc1, 2, 4, 0);

    ///////////   2ND 8x16 BLOCK  /////////////////////////////////////////////////////////////////////
    ///// ACCORDING TO B-SPEC: Precision: 16-bit pixels truncated to 8-bits before subtract & compares.
    src     = (rd_in.select<12,1,32,1>(8,0) >> (short)(shift));
    premask = (src.select<8,1,16,1>(2,2) <= good_intensity_TH);

    COMP_ACC0_ACC1(premask, src, acc0, acc1, 0, 2, 8);
    COMP_ACC0_ACC1(premask, src, acc0, acc1, 2, 0, 8);
    COMP_ACC0_ACC1(premask, src, acc0, acc1, 4, 2, 8);
    COMP_ACC0_ACC1(premask, src, acc0, acc1, 2, 4, 8);

    /////////////  WRITE OUT  //////////////////////////////////////////////////////////////////////////
    matrix<uchar, 16, 8> out0, out1;

#if 1
    out0  = acc0.select<16, 1, 8, 2>(0,0) << 4;
    out0 += acc0.select<16, 1, 8, 2>(0,1);
    out1  = acc1.select<16, 1, 8, 2>(0,0) << 4;
    out1 += acc1.select<16, 1, 8, 2>(0,1);

    write(obuf0, (wr_h_pos >> 1), wr_v_pos, out0);
    write(obuf1, (wr_h_pos >> 1), wr_v_pos, out1);
#else
    write(obuf0, (wr_h_pos ), wr_v_pos, acc0);
    write(obuf1, (wr_h_pos ), wr_v_pos, acc1);
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
         SurfaceIndex obuf, int x , int y, int wr_x, int wr_y, ushort max_intensity)
{
    matrix< uchar, 12, 16 >   src0;
    matrix< uchar, 12, 16 >   src1;
    matrix< uchar, 8,  8  >   dst ;
    matrix<ushort, 8,  8  >   mask;
    matrix<ushort, 8,  8  >   acc0(0);
    matrix<ushort, 8,  8  >   acc1(0);

    int rd_h_pos = ((x + get_thread_origin_x()) * 8 - 2);
    int rd_v_pos = ((y + get_thread_origin_y()) * 8 - 2);

#if 1
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

    mask = (acc0 > num_good_pix_th) | (acc1 > num_big_pix_th);
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

    //M = (cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(0,2), -srcBayer.select<8,1,8,1>(2,2))) < 
    //     cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(2,2), -srcBayer.select<8,1,8,1>(4,2))));
    M = (cm_abs<ushort>(cm_add<int>(srcBayer.select<8,1,8,1>(0,2), -srcBayer.select<8,1,8,1>(2,2))) < 
         cm_abs<ushort>(cm_add<int>(srcBayer.select<8,1,8,1>(2,2), -srcBayer.select<8,1,8,1>(4,2))));

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

    //M = (cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(2,0), -srcBayer.select<8,1,8,1>(2,2))) < 
    //     cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(2,2), -srcBayer.select<8,1,8,1>(2,4))));
    M = (cm_abs<ushort>(cm_add<int>(srcBayer.select<8,1,8,1>(2,0), -srcBayer.select<8,1,8,1>(2,2))) < 
         cm_abs<ushort>(cm_add<int>(srcBayer.select<8,1,8,1>(2,2), -srcBayer.select<8,1,8,1>(2,4))));
    
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

    //matrix_ref<ushort, 8, 8> tmpA = tmp0.format<ushort, 8, 8>();
    //matrix_ref<ushort, 8, 8> tmpB = tmp1.format<ushort, 8, 8>();
    matrix_ref<uint, 8, 8> tmpA = tmp0.format<uint, 8, 8>();
    matrix_ref<uint, 8, 8> tmpB = tmp1.format<uint, 8, 8>();
    
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

extern "C" _GENX_MAIN_ void
RESTOREBandR(SurfaceIndex ibufBayer,
             SurfaceIndex ibufGHOR, SurfaceIndex ibufGVER, SurfaceIndex ibufGAVG,
             SurfaceIndex obufBHOR, SurfaceIndex obufBVER, SurfaceIndex obufBAVG,
             SurfaceIndex obufRHOR, SurfaceIndex obufRVER, SurfaceIndex obufRAVG,
             SurfaceIndex ibufAVGMASK,
             int x , int y, int wr_x, int wr_y, ushort max_intensity)
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
    matrix<int, 8, 12> BR32_c; // 12 registers
    matrix<int, 8,  8> BR33_c; //  8 registers

    matrix_ref<ushort, 8, 12> BR32_s = BR32_c.select<4,1,12,1>(0,0).format<ushort, 8, 12>(); //matrix<ushort, 8,12> BR32_s;
    matrix_ref<ushort, 12, 8> BR34_s = BR34_c.select<6,1,8,1>(0,0) .format<ushort, 12, 8>(); //matrix<ushort,12, 8> BR34_s;
    matrix_ref<ushort, 8,  8> BR33_s = BR33_c.select<4,1,8,1>(0,0) .format<ushort,  8, 8>(); //matrix<ushort, 8, 8> BR33_s;

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
    //G_c_x2 = Green_c << 1;//cm_mul<int>(Green_c, 2);
        
    BR32_c  = cm_mul<int>(Green_c.select<8,1,12,1>(2,0), 2); //BR32_c  = G_c_x2 .select<8,1,12,1>(2,0);
    BR32_c += dBG_c  .select<8,1,12,1>(1,0);
    BR32_c += dBG_c  .select<8,1,12,1>(3,0);
    BR32_c >>= 1;
    BR32_c = cm_max<int>(BR32_c, 0); 
    BR32_s = cm_min<int>(BR32_c, max_intensity); 
    out_B.select<4,2,4,2>(1,0) = BR32_s.select<4,2,4,2>(1,2);
    out_R.select<4,2,4,2>(0,1) = BR32_s.select<4,2,4,2>(0,3); 

    
    BR34_c  = cm_mul<int>(Green_c.select<12,1,8,1>(0,2), 2); //BR34_c  = G_c_x2 .select<12,1,8,1>(0,2); 
    BR34_c += dBG_c  .select<12,1,8,1>(0,1); 
    BR34_c += dBG_c  .select<12,1,8,1>(0,3);
    BR34_c >>= 1;
    BR34_c = cm_max<int>(BR34_c, 0); 
    BR34_s = cm_min<int>(BR34_c, max_intensity); 
    out_B.select<4,2,4,2>(0,1) = BR34_s.select<4,2,4,2>(2,1); 
    out_R.select<4,2,4,2>(1,0) = BR34_s.select<4,2,4,2>(3,0); 

    
    BR33_c  = cm_mul<int>(Green_c.select<8,1,8,1>(2,2), 2); //BR33_c  = G_c_x2 .select<8,1,8,1>(2,2);
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
    //G_c_x2 = Green_c << 1;//cm_mul<int>(Green_c, 2);
    
    BR32_c  = cm_mul<int>(Green_c.select<8,1,12,1>(2,0), 2); //BR32_c  = G_c_x2 .select<8,1,12,1>(2,0);
    BR32_c += dBG_c  .select<8,1,12,1>(1,0);
    BR32_c += dBG_c  .select<8,1,12,1>(3,0);
    BR32_c >>= 1;
    BR32_c.select<8,1,8,1>(0,2) = cm_max<int>(BR32_c.select<8,1,8,1>(0,2), 0); 
    BR32_s = cm_min<int>(BR32_c, max_intensity); 
    out_B.select<4,2,4,2>(1,0) = BR32_s.select<4,2,4,2>(1,2); 
    out_R.select<4,2,4,2>(0,1) = BR32_s.select<4,2,4,2>(0,3); 

    BR34_c  = cm_mul<int>(Green_c.select<12,1,8,1>(0,2), 2); //BR34_c  = G_c_x2 .select<12,1,8,1>(0,2);
    BR34_c += dBG_c  .select<12,1,8,1>(0,1); 
    BR34_c += dBG_c  .select<12,1,8,1>(0,3);
    BR34_c >>= 1;
    BR34_c = cm_max<int>(BR34_c, 0); 
    BR34_s = cm_min<int>(BR34_c, max_intensity); 
    out_B.select<4,2,4,2>(0,1) = BR34_s.select<4,2,4,2>(2,1); 
    out_R.select<4,2,4,2>(1,0) = BR34_s.select<4,2,4,2>(3,0); 

    
    BR33_c  = cm_mul<int>(Green_c.select<8,1,8,1>(2,2), 2); //BR33_c  = G_c_x2 .select<8,1,8,1>(2,2);
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
        //G_c_x2 = Green_c << 1;//cm_mul<int>(Green_c, 2);
    
        BR32_c  = cm_mul<int>(Green_c.select<8,1,12,1>(2,0), 2); //BR32_c  = G_c_x2.select<8,1,12,1>(2,0);
        BR32_c += dBG_c.select< 8,1,12,1>(1,0);
        BR32_c += dBG_c.select< 8,1,12,1>(3,0);
        BR32_c >>= 1;
        BR32_c = cm_max<int>(BR32_c, 0); 
        BR32_s = cm_min<int>(BR32_c, max_intensity); 
        out_B.select<4,2,4,2>(1,0) = BR32_s.select<4,2,4,2>(1,2); 
        out_R.select<4,2,4,2>(0,1) = BR32_s.select<4,2,4,2>(0,3); 


        BR34_c  = cm_mul<int>(Green_c.select<12,1,8,1>(0,2), 2); //BR34_c  = G_c_x2.select<12,1,8,1>(0,2);
        BR34_c += dBG_c .select<12,1,8,1>(0,1); 
        BR34_c += dBG_c .select<12,1,8,1>(0,3);
        BR34_c >>= 1;
        BR34_c = cm_max<int>(BR34_c, 0); 
        BR34_s = cm_min<int>(BR34_c, max_intensity); 
        out_B.select<4,2,4,2>(0,1) = BR34_s.select<4,2,4,2>(2,1); 
        out_R.select<4,2,4,2>(1,0) = BR34_s.select<4,2,4,2>(3,0);


        BR33_c  = cm_mul<int>(Green_c.select<8,1,8,1>(2,2), 4); //BR33_c  = cm_mul<int>(G_c_x2.select<8,1,8,1>(2,2), 2);
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
       int x , int y, int wr_x, int wr_y, int frameHeight, /* int bitshift,*/ int BayerType,
       SurfaceIndex obufROUT, SurfaceIndex obufGOUT, SurfaceIndex obufBOUT)
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

    read(ibufRHOR, rd_h_pos - 8, rd_v_pos - 4, tmp.select<8,1,16,1>(0,0)); // tmp  == R
    read(ibufRHOR, rd_h_pos - 8, rd_v_pos + 4, tmp.select<8,1,16,1>(8,0)); // tmp  == R
    R_H = tmp.select<8,1,8,1>(4,4);
    tmp >>= bitshift;                                                       //<- bitshift
    R = tmp;

    read(ibufGHOR, rd_h_pos - 8, rd_v_pos - 4, in .select<8,1,16,1>(0,0)); // in   == G
    read(ibufGHOR, rd_h_pos - 8, rd_v_pos + 4, in .select<8,1,16,1>(8,0)); // in   == G
    G_H = in .select<8,1,8,1>(4,4);
    in >>= bitshift;                                                       //<- bitshift
    tmp -= in;                                                               // tmp  == DRG
    
    HDSUM_H.select<8,1,W,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<8,1,W,1>(4,1), - tmp.select<8,1,W,1>(4,0)));
    VDSUM_H.select<W,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<W,1,8,1>(1,4), - tmp.select<W,1,8,1>(0,4)));

    read(ibufBHOR, rd_h_pos - 8, rd_v_pos - 4, tmp .select<8,1,16,1>(0,0)); // tmp == B
    read(ibufBHOR, rd_h_pos - 8, rd_v_pos + 4, tmp .select<8,1,16,1>(8,0)); // tmp == B
    B_H = tmp.select<8,1,8,1>(4,4);
    tmp >>= bitshift;                                                       //<- bitshift
    in -= tmp;                                                                //  in == DGB 

    HDSUM_H.select<8,1,W,1>(0,0) += cm_abs<ushort>(cm_add<short>(in.select<8,1,W,1>(4,1), - in.select<8,1,W,1>(4,0)));
    VDSUM_H.select<W,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(in.select<W,1,8,1>(1,4), - in.select<W,1,8,1>(0,4)));

    tmp -= R;                                                                // tmp == DBR

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
    Min_cost_H  = Min_cost_H2 >> 1;
    Min_cost_H += Min_cost_H1     ;

    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////    PART II: COMPUTE MIN_COST_V  ////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    matrix<short, 8, 8> R_V, G_V, B_V;
    matrix_ref<ushort, 8, 16> HDSUM_V = HDSUM_H;
    matrix_ref<ushort, 16, 8> VDSUM_V = VDSUM_H;

    read(ibufRVER, rd_h_pos - 8, rd_v_pos - 4, tmp.select<8,1,16,1>(0,0)); // tmp  == R
    read(ibufRVER, rd_h_pos - 8, rd_v_pos + 4, tmp.select<8,1,16,1>(8,0)); // tmp  == R
    R_V = tmp.select<8,1,8,1>(4,4);
    R = tmp >> bitshift;

    read(ibufGVER, rd_h_pos - 8, rd_v_pos - 4, in .select<8,1,16,1>(0,0)); // in   == G
    read(ibufGVER, rd_h_pos - 8, rd_v_pos + 4, in .select<8,1,16,1>(8,0)); // in   == G
    G_V = in .select<8,1,8,1>(4,4);
    in >>= bitshift;                                                       //<- bitshift
    tmp = (R - in);
    //tmp -= in;                                                               // tmp  == DRG
    
    HDSUM_V.select<8,1,W,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<8,1,W,1>(4,1), - tmp.select<8,1,W,1>(4,0)));
    VDSUM_V.select<W,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<W,1,8,1>(1,4), - tmp.select<W,1,8,1>(0,4)));

    read(ibufBVER, rd_h_pos - 8, rd_v_pos - 4, tmp .select<8,1,16,1>(0,0)); // tmp == B
    read(ibufBVER, rd_h_pos - 8, rd_v_pos + 4, tmp .select<8,1,16,1>(8,0)); // tmp == B
    B_V = tmp.select<8,1,8,1>(4,4);
    tmp >>= bitshift;                                                       //<- bitshift
    in -= tmp;                                                                //  in == DGB 

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
    Min_cost_V  = Min_cost_V2 >> 1;
    Min_cost_V += Min_cost_V1     ;

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

    if(BayerType == GRBG || BayerType == GBRG)
    {
        ROWSWAP_8x8(outR);
        ROWSWAP_8x8(outG);
        ROWSWAP_8x8(outB);
        wr_v_pos = (frameHeight - wr_v_pos) - 8;  
    }

    write(obufROUT, wr_h_pos, wr_v_pos, outR);
    write(obufGOUT, wr_h_pos, wr_v_pos, outG);
    write(obufBOUT, wr_h_pos, wr_v_pos, outB);
}

// BITDEPTH 14 VERSION OF SAD KERNEL
extern "C" _GENX_MAIN_ void
SAD(SurfaceIndex ibufRHOR, SurfaceIndex ibufGHOR, SurfaceIndex ibufBHOR,
    SurfaceIndex ibufRVER, SurfaceIndex ibufGVER, SurfaceIndex ibufBVER,
    int x , int y, int wr_x, int wr_y, int frameHeight, /*int bitshift,*/ int BayerType,
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
    tmp -= in;                                                               // tmp  == DRG

    HDSUM_H.select<8,1,W,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<8,1,W,1>(4,1), - tmp.select<8,1,W,1>(4,0)));
    VDSUM_H.select<W,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<W,1,8,1>(1,4), - tmp.select<W,1,8,1>(0,4)));

    read(ibufBHOR, rd_h_pos - 8, rd_v_pos - 4, tmp .select<8,1,16,1>(0,0)); // tmp == B
    read(ibufBHOR, rd_h_pos - 8, rd_v_pos + 4, tmp .select<8,1,16,1>(8,0)); // tmp == B
    B_H = tmp.select<8,1,8,1>(4,4);
    in -= tmp;                                                                //  in == DGB

    HDSUM_H.select<8,1,W,1>(0,0) += cm_abs<ushort>(cm_add<short>(in.select<8,1,W,1>(4,1), - in.select<8,1,W,1>(4,0)));
    VDSUM_H.select<W,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(in.select<W,1,8,1>(1,4), - in.select<W,1,8,1>(0,4)));

    tmp -= R;                                                                // tmp == DBR

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
    Min_cost_H  = Min_cost_H2 >> 1;
    Min_cost_H += Min_cost_H1     ;


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
    tmp -= in;                                                               // tmp  == DRG

    HDSUM_V.select<8,1,W,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<8,1,W,1>(4,1), - tmp.select<8,1,W,1>(4,0)));
    VDSUM_V.select<W,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(tmp.select<W,1,8,1>(1,4), - tmp.select<W,1,8,1>(0,4)));

    read(ibufBVER, rd_h_pos - 8, rd_v_pos - 4, tmp .select<8,1,16,1>(0,0)); // tmp == B
    read(ibufBVER, rd_h_pos - 8, rd_v_pos + 4, tmp .select<8,1,16,1>(8,0)); // tmp == B
    B_V = tmp.select<8,1,8,1>(4,4);
    in -= tmp;                                                                //  in == DGB

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
    Min_cost_V  = Min_cost_V2 >> 1;
    Min_cost_V += Min_cost_V1     ;

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
    matrix_ref<ushort, 8, 8> out  = tmp.select<4,1,16,1>(12,0).format<ushort, 8, 8>();
    matrix_ref<int,   8, 8> sum  = Min_cost_H;

    outR.merge(R_H, R_V, (HV_Flag  == 1));
    outG.merge(G_H, G_V, (HV_Flag  == 1));
    outB.merge(B_H, B_V, (HV_Flag  == 1));

    if(BayerType == GRBG || BayerType == GBRG)
    {
        ROWSWAP_8x8(outR);
        ROWSWAP_8x8(outG);
        ROWSWAP_8x8(outB);
        wr_v_pos = (frameHeight - wr_v_pos) - 8;  
    }

    write(obufROUT, wr_h_pos, wr_v_pos, outR);
    write(obufGOUT, wr_h_pos, wr_v_pos, outG);
    write(obufBOUT, wr_h_pos, wr_v_pos, outB);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////     DECIDE AVERAGE    ////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

const unsigned short idx_init[16] = {1,1,2,2,5,5,6,6,9,9,10,10,13,13,14,14};
extern "C" _GENX_ void inline
HORIZONTAL_CHECK(matrix_ref<short, 13, 32> srcG, matrix_ref<ushort, 8, 16> maskH)
{

    matrix< short, 4, 16 >   diff;
#pragma unroll
    for(int j = 0; j < 8; j++) // j == index of output registers
    {
        vector<short, 32> rowG = srcG.row(2+j);

        matrix_ref<short, 8, 4> m_rowG = rowG.format<short, 8, 4>();
        matrix<short, 5, 16> v;


        v.select<1,1,8,2>(0,0) = m_rowG.genx_select<4,4,2,1>(0,1);
        v.select<1,1,8,2>(0,1) = v.select<1,1,8,2>(0,0);

#pragma unroll
        for(int i = 1; i < 5; i++) {
            v.select<1,1,8,2>(i,0) = m_rowG.genx_select<4,4,2,1>(0,i+1);
            v.select<1,1,8,2>(i,1) = v.select<1,1,8,2>(i,0);

            diff.row(i-1) = v.row(i-1) - v.row(i);
        }

        maskH.row(j)  = (cm_abs<ushort>(diff.row(3)) > Grn_imbalance_th);
//#pragma unroll
        for(int i = 0; i < 3; i++) {
            maskH.row(j) &= (diff.row(i) ^ diff.row(i+1)) < 0;
//            maskH.row(j) &= (cm_mul<int> (diff.row(i),diff.row(i+1)) < 0);
            maskH.row(j) &= (cm_abs<ushort>(diff.row(i)) > Grn_imbalance_th);
        }
    }
}

extern "C" _GENX_ void inline
VERTICAL_CHECK(matrix_ref<short, 13, 16> cenG, matrix_ref<ushort, 8, 16> maskV)
{
    matrix< short, 4, 16 >   diff;
    //{0,1,4,5} row
#pragma unroll
    for(int j = 0; j <= 4; j+=4 ) {

        diff          = (cenG.select<4,1,16,1>(1+j,0) - cenG.select<4,1,16,1>(2+j,0));
        maskV.row(j)  = (cm_abs<ushort>(diff.row(3)) > Grn_imbalance_th);
//#pragma unroll
        for(int i = 0; i < 3; i++) {
//            maskV.row(j) &= (cm_mul<int> (diff.row(i),diff.row(i+1)) < 0);
            maskV.row(j) &= (diff.row(i) ^ diff.row(i+1)) < 0;
            maskV.row(j) &= (cm_abs<ushort>(diff.row(i)) > Grn_imbalance_th);
        }
        maskV.row(j+1)  = maskV.row(j);
    }

    //{2,3,6,7} row
#pragma unroll
    for(int j = 2; j <= 6; j += 4 ) {
        diff          = (cenG.select<4,1,16,1>(  j,0) - cenG.select<4,1,16,1>(1+j,0));

        maskV.row(j)  = (cm_abs<ushort>(diff.row(3)) > Grn_imbalance_th);
//#pragma unroll
        for(int i = 0; i < 3; i++) {
//            maskV.row(j) &= (cm_mul<int> (diff.row(i),diff.row(i+1)) < 0);
            maskV.row(j) &= (diff.row(i) ^ diff.row(i+1)) < 0;
            maskV.row(j) &= (cm_abs< uint >(diff.row(i)) > Grn_imbalance_th);
        }
        maskV.row(j+1) = maskV.row(j);
    }
}

extern "C" _GENX_ void inline
COLORDIFF_CHECK(matrix_ref<short , 8, 16> srcR, matrix_ref<short, 8, 16> srcG, matrix_ref<short, 8, 16> srcB,
                matrix_ref<ushort, 8, 16> maskC)
{
    matrix< ushort, 8, 16 > absv;     //matrix< short, 8, 16 > absv,
    matrix< short  , 8, 16 > difference;//matrix< short, 8, 16 > difference;

    difference = srcR - srcG;
    absv = cm_abs<ushort>(difference);
    maskC  = (absv > Average_Color_TH);

    difference = srcG - srcB;
    absv = cm_abs<ushort>(difference);
    maskC |= (absv > Average_Color_TH);

    difference = srcB - srcR;
    absv = cm_abs<ushort>(difference);
    maskC |= (absv > Average_Color_TH);
}

extern "C" _GENX_MAIN_ void
DECIDE_AVG( SurfaceIndex ibufRAVG, SurfaceIndex ibufGAVG, SurfaceIndex ibufBAVG,
                         SurfaceIndex ibufAVG_Flag,
                         SurfaceIndex obufROUT, SurfaceIndex obufGOUT, SurfaceIndex obufBOUT, 
                         int wr_x, int wr_y,
                         int frameHeight,
                         int BayerType)
{
    matrix< short, 13,32 >   srcG;
    matrix< short, 8, 16 >   srcR, srcB, absv, difference, AVG_Mask, out;

    matrix< uchar,16, 16 >   AVG_flag;
    matrix<ushort, 8, 16 >   mask, maskH, maskV, maskC;

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

    matrix<short, 13, 16> cenG;
    cenG = srcG.select<13,1,16,1>(0,2);

    AVG_Mask = (AVG_flag.select<8,1,16,1>(0,0) == AVG_True);
    if(AVG_Mask.any())
    {
        read(ibufRAVG, rd_h_pos+4 , rd_v_pos+2, srcR);
        read(ibufBAVG, rd_h_pos+4 , rd_v_pos+2, srcB);

        HORIZONTAL_CHECK(srcG, maskH);
        VERTICAL_CHECK  (cenG, maskV);
        COLORDIFF_CHECK (srcR, srcG.select<8,1,16,1>(2,2), srcB, maskC);
        mask  = maskH | maskV | maskC ;

        AVG_flag.select<8,1,16,1>(0,0).merge(AVG_False, mask);
    }

    int wr_v_pos_tmp = wr_v_pos;

    AVG_Mask = (AVG_flag.select<8,1,16,1>(0,0) == AVG_True);
    if(AVG_Mask.any())
    {
        if(BayerType == GRBG || BayerType == GBRG)
        {
            ROWSWAP_8x16(srcR);
            ROWSWAP_8x16(srcG.select<8,1,16,1>(2,2));
            ROWSWAP_8x16(srcB);
            ROWSWAP_8x16(AVG_Mask);

            wr_v_pos = (frameHeight - wr_v_pos) - 8;
        }

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
    //wr_v_pos += 8;
    wr_v_pos = wr_v_pos_tmp + 8;
    rd_v_pos += 8;

    if(BayerType == GRBG || BayerType == GBRG)
    {
        read(ibufGAVG, rd_h_pos   , rd_v_pos  , srcG.select<8,1,16,1>(0,0 ));
        read(ibufGAVG, rd_h_pos+32, rd_v_pos  , srcG.select<8,1,16,1>(0,16));
        read(ibufGAVG, rd_h_pos   , rd_v_pos+8, srcG.select<5,1,16,1>(8,0 ));
        read(ibufGAVG, rd_h_pos+32, rd_v_pos+8, srcG.select<5,1,16,1>(8,16));
    }
    else
    {
        srcG.select<5,1,16,1>(0,0 ) = srcG.select<5,1,16,1>(8,0 );
        srcG.select<5,1,16,1>(0,16) = srcG.select<5,1,16,1>(8,16);
        read(ibufGAVG, rd_h_pos   , rd_v_pos+5 , srcG.select<8,1,16,1>(5,0 ));
        read(ibufGAVG, rd_h_pos+32, rd_v_pos+5 , srcG.select<8,1,16,1>(5,16));
    }


    if((AVG_flag.select<8,1,16,1>(8,0) == AVG_True).any())
    {
        read(ibufRAVG, rd_h_pos+4 , rd_v_pos+2, srcR);
        read(ibufBAVG, rd_h_pos+4 , rd_v_pos+2, srcB);
        cenG = srcG.select<13,1,16,1>(0,2);

        HORIZONTAL_CHECK(srcG, maskH);
        VERTICAL_CHECK  (cenG, maskV);
        COLORDIFF_CHECK (srcR, srcG.select<8,1,16,1>(2,2), srcB, maskC);
        mask  = maskH | maskV | maskC ;

        AVG_flag.select<8,1,16,1>(8,0).merge(AVG_False, mask);
    }

    AVG_Mask = (AVG_flag.select<8,1,16,1>(8,0) == AVG_True);
    if(AVG_Mask.any())
    {
        if(BayerType == GRBG || BayerType == GBRG)
        {
            ROWSWAP_8x16(srcR);
            ROWSWAP_8x16(srcG.select<8,1,16,1>(2,2));
            ROWSWAP_8x16(srcB);
            ROWSWAP_8x16(AVG_Mask);

            wr_v_pos = (frameHeight - wr_v_pos) - 8;
        }

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
GAMMA_CORRECT_V5(SurfaceIndex BUF_R, SurfaceIndex BUF_G, SurfaceIndex BUF_B, SurfaceIndex ARGB8,
                      int h_pos, int v_pos, int slmX, int bitdepth)
{
    matrix<ushort, 8, 16> rd_in_R;
    matrix<ushort, 8, 16> rd_in_G;
    matrix<ushort, 8, 16> rd_in_B;
    //matrix<ushort, 8, 16> wr_out;

    matrix<uchar, 8, 64> out;

    int shift_amount = bitdepth - 8;;

    vector<ushort, 16> v_Addr;
    //vector<ushort, 16> rd_slmDataIn; //vector<uint,   16> rd_slmDataIn; // <- original supporting bitDepth upto 14
    vector<uint,   16> rd_slmDataIn; // for 3.0 compiler which doesn't support word granularity

    read(BUF_R, h_pos, v_pos, rd_in_R);
#pragma unroll
    for(int i = 0; i < 8; i++)
    {
        v_Addr = rd_in_R.row(i); 
        cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.select<1,1,16,4>(i,0) = (rd_slmDataIn >> shift_amount);
    }

    read(BUF_G, h_pos, v_pos, rd_in_G);
#pragma unroll
    for(int i = 0; i < 8; i++)
    {
        v_Addr = rd_in_G.row(i);
        cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.select<1,1,16,4>(i,1) = (rd_slmDataIn >> shift_amount);
    }

    read(BUF_B, h_pos, v_pos, rd_in_B);
#pragma unroll
    for(int i = 0; i < 8; i++)
    {
        v_Addr = rd_in_B.row(i); 
        cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.select<1,1,16,4>(i,2) = (rd_slmDataIn >> shift_amount);
    }

#pragma unroll
    for(int i = 0; i < 8; i++)
    {
        out.select<1,1,16,4>(i,3) = 0;
    }

    write(ARGB8, h_pos*2   , v_pos, out.select<8,1,32,1>(0,0 ));
    write(ARGB8, h_pos*2+32, v_pos, out.select<8,1,32,1>(0,32));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//////   KERNEL 'GAMMA+ARGB8_INTERLEAVE' FOR CANON    //////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
extern "C" _GENX_MAIN_ void 
GAMMA_GPUV4_ARGB8_2D(SurfaceIndex Correct_Index, SurfaceIndex Point_Index,
                     SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
                     SurfaceIndex ARGB8_Index,
                     int blocks_in_a_row, int BitDepth, int framewidth_in_bytes, int frameheight)
{
    cm_slm_init(65536);
    uint slmX = cm_slm_alloc(65536);

    //ENABLE LOOK-UP TABLE COMPUTING
    matrix<ushort, 4, 16> Correct, Point;
    matrix_ref<ushort, 1, 64> gamma_P = Point.format<ushort, 1, 64>();
    matrix_ref<ushort, 1, 64> gamma_C = Correct.format<ushort, 1, 64>();

    read(Correct_Index, 0, 0, Correct);
    read(Point_Index  , 0, 0, Point  );

    vector<uint, 16> index(0);
    vector<uint, 16> i_plus_1;
    vector<ushort, 16> v_in_initial(initSeq);

    vector<uint, 64> v_slmData;
    vector<uint, 64> v_slmDataIn;

    int id = cm_linear_local_id();
    int total_load_elements = (1 << BitDepth);                                     // in number of elements
    int max_input_level = total_load_elements - 1;                                    
    int load_elements_per_thread = total_load_elements / cm_linear_local_size(); // in number of elements
    int loop_count = load_elements_per_thread / 16;                              // Each loop handle 16 elements   
    
    vector<ushort, 16> v_in;

    for(int k = 0; k < loop_count; k++)
    { 
        v_in = v_in_initial + (id * load_elements_per_thread + k * 16);

        index = 0;
        vector<ushort, 16> mak;
        vector<ushort, 16> comp_index;

        comp_index = 32;
#pragma unroll
        for(int i = 5; i >= 0; i--)
        {
            int shift = (1 << (i - 1));
            mak     = (gamma_P.iselect(comp_index) <= v_in);
            index  += (mak << i);
            comp_index.merge((comp_index+shift), (comp_index-shift), mak);
        }
        index.merge(0, ((index == 1) & (v_in < gamma_P(0,1))));
        
        //vector<uint, 16> i_plus_1;
        vector<ushort, 16> c_i, p_i, c_i_plus_1, p_i_plus_1;
        i_plus_1.merge((NUM_CONTROL_POINTS-1), (index + 1), (index == (NUM_CONTROL_POINTS-1)));

        c_i = gamma_C.iselect(index); //c_i = Correct.iselect(index);
        p_i = gamma_P.iselect(index); //p_i = Point  .iselect(index);

        //c_i_plus_1.merge( max_input_level,Correct.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));
        //p_i_plus_1.merge( max_input_level,Point  .iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));
        c_i_plus_1.merge( max_input_level,  gamma_C.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));
        p_i_plus_1.merge( max_input_level,  gamma_P.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));

        vector<int, 16> v_o = cm_add<int>(c_i_plus_1, -c_i);//(c_i_plus_1 - c_i);

        v_o *= cm_add<int>(v_in, -p_i);      //(v_in - p_i);

        vector<int, 16> tmp = cm_add<int>(p_i_plus_1, -p_i);
        tmp = cm_max<int>(1, tmp);
        v_o /= tmp;
        
        //v_o /= cm_add<int>(p_i_plus_1, -p_i, SAT);//(p_i_plus_1 - p_i);

        v_o += c_i;

//        vector<ushort, 16> v_out;
        vector<uint, 16> v_out; // for 3.0 compiler which doesn't support word granularity
        v_o = cm_max<int>(0, v_o);
        v_out = cm_min<uint>(max_input_level, v_o);

        cm_slm_write(slmX, v_in, v_out);
    }

    cm_barrier();
    
    /////////////////////////////////////////////////////////////////////////////////
    //////////////////  LOOK-UP TABLE PREPARED   ////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////

    int rd_h_pos, rd_v_pos;
    
    int l_count = (frameheight % 128 == 0)? (frameheight / 128) : ((frameheight / 128)+1);
    for(int l = 0; l < l_count; l++)   // 17 = (frameHeight / (32(H/group)*4 groups))
    {                                   // Four Groups, jointly, one time does 128 pixel high. Need (height/128) loops 
        for(int m = 0; m < 4; m++)
        {
                //EACH thread in the threadgroup handle 256 byte-wide data
                rd_v_pos = cm_linear_group_id() * 32 + m * 8 + l * 128;
#pragma unroll
                for(int i = 0; i < 8; i++)
                {
                    rd_h_pos = cm_linear_local_id() * 256 + 32 * i;             
                    GAMMA_CORRECT_V5(R_I_Index, G_I_Index, B_I_Index, ARGB8_Index, rd_h_pos, rd_v_pos, slmX, BitDepth);
                }

        }
    }

}

_GENX_ void inline
GAMMA_ONLY_CORRECT(SurfaceIndex BUF_R, SurfaceIndex BUF_G, SurfaceIndex BUF_B,
                   int h_pos, int v_pos, int slmX)
{
    matrix<ushort, 8, 16> rd_in_R;
    matrix<ushort, 8, 16> rd_in_G;
    matrix<ushort, 8, 16> rd_in_B;
    matrix<ushort, 8, 16> wr_out;
    
    vector<ushort, 16> v_Addr;
    vector<uint,   16> rd_slmDataIn;

    read(BUF_R, h_pos, v_pos, rd_in_R);
#pragma unroll
    for(int i = 0; i < 8; i++)
    {
        v_Addr = rd_in_R.row(i); cm_slm_read(slmX, v_Addr, rd_slmDataIn); wr_out.row(i) = rd_slmDataIn;
    }
    write(BUF_R, h_pos, v_pos, wr_out);

    read(BUF_G, h_pos, v_pos, rd_in_G);
#pragma unroll
    for(int i = 0; i < 8; i++)
    {
        v_Addr = rd_in_G.row(i); cm_slm_read(slmX, v_Addr, rd_slmDataIn); wr_out.row(i) = rd_slmDataIn;
    }
    write(BUF_G, h_pos, v_pos, wr_out);

    read(BUF_B, h_pos, v_pos, rd_in_B);
#pragma unroll
    for(int i = 0; i < 8; i++)
    {
        v_Addr = rd_in_B.row(i); cm_slm_read(slmX, v_Addr, rd_slmDataIn); wr_out.row(i) = rd_slmDataIn;
    }
    write(BUF_B, h_pos, v_pos, wr_out);

}

#if 0
extern "C" _GENX_MAIN_ void 
GAMMA_ONLY(SurfaceIndex Correct_Index, SurfaceIndex Point_Index,
           SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
           vector<float, 9> ccm,
           int blocks_in_a_row, int BitDepth, int framewidth_in_bytes, int frameheight,
           SurfaceIndex LUT_Index)
{
    cm_slm_init(65536);
    uint slmX = cm_slm_alloc(65536);

    //ENABLE LOOK-UP TABLE COMPUTING
    matrix<ushort, 4, 16> Correct, Point;
    matrix_ref<ushort, 1, 64> gamma_P = Point.format<ushort, 1, 64>();
    matrix_ref<ushort, 1, 64> gamma_C = Correct.format<ushort, 1, 64>();

    read(Correct_Index, 0, 0, Correct);
    read(Point_Index  , 0, 0, Point  );

    vector<uint, 16> index(0);
    vector<uint, 16> i_plus_1;
    vector<ushort, 16> v_in_initial(initSeq);

    vector<uint, 64> v_slmData;
    vector<uint, 64> v_slmDataIn;

    int id = cm_linear_local_id();
    int total_load_elements = (1 << BitDepth);                                     // in number of elements
    int max_input_level = total_load_elements - 1;                                    
    int load_elements_per_thread = total_load_elements / cm_linear_local_size(); // in number of elements
    int loop_count = load_elements_per_thread / 16;                              // Each loop handle 16 elements   
    
    vector<ushort, 16> v_in;

    for(int k = 0; k < loop_count; k++)
    { 
        v_in = v_in_initial + (id * load_elements_per_thread + k * 16);

        index = 0;
        vector<ushort, 16> mak;
        vector<ushort, 16> comp_index;

        comp_index = 32;
#pragma unroll
        for(int i = 5; i >= 0; i--)
        {
            int shift = (1 << (i - 1));
            mak     = (gamma_P.iselect(comp_index) <= v_in);
            index  += (mak << i);
            comp_index.merge((comp_index+shift), (comp_index-shift), mak);
        }
        index.merge(0, ((index == 1) & (v_in < gamma_P(0,1))));   
        
        //vector<uint, 16> i_plus_1;
        vector<ushort, 16> c_i, p_i, c_i_plus_1, p_i_plus_1;
        i_plus_1.merge((NUM_CONTROL_POINTS-1), (index + 1), (index == (NUM_CONTROL_POINTS-1)));

        c_i = gamma_C.iselect(index); //c_i = Correct.iselect(index);
        p_i = gamma_P.iselect(index); //p_i = Point  .iselect(index);

        c_i_plus_1.merge( max_input_level,  gamma_C.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));
        p_i_plus_1.merge( max_input_level,  gamma_P.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));

        vector<int, 16> v_o = cm_add<int>(c_i_plus_1, -c_i);//(c_i_plus_1 - c_i);

        v_o *= cm_add<int>(v_in, -p_i);      //(v_in - p_i);

        vector<ushort, 16> tmp = p_i_plus_1 -p_i;
        tmp = cm_max<ushort>(1, tmp);
        v_o /= tmp;

//        v_o /= cm_add<int>(p_i_plus_1, -p_i);//(p_i_plus_1 - p_i);

        v_o += c_i;
        
        vector<uint, 16> v_out;
        v_o = cm_max<uint>(0, v_o);
        v_out = cm_min<uint>(max_input_level, v_o);    

        cm_slm_write(slmX, v_in, v_out);
    }

    cm_barrier();
    
    /////////////////////////////////////////////////////////////////////////////////
    //////////////////  LOOK-UP TABLE PREPARED   ////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////

    int rd_h_pos, rd_v_pos;
    
    int l_count = (frameheight % 128 == 0)? (frameheight / 128) : ((frameheight / 128)+1);
    for(int l = 0; l < l_count; l++)   // 17 = (frameHeight / (32(H/group)*4 groups))
    {                                   // Four Groups, jointly, one time does 128 pixel high. Need (height/128) loops 
        for(int m = 0; m < 4; m++)
        {
                //EACH thread in the threadgroup handle 256 byte-wide data
                rd_v_pos = cm_linear_group_id() * 32 + m * 8 + l * 128;
#pragma unroll
                for(int i = 0; i < 8; i++)
                {
                    rd_h_pos = cm_linear_local_id() * 256 + 32 * i;             
                    GAMMA_ONLY_CORRECT(R_I_Index, G_I_Index, B_I_Index, rd_h_pos, rd_v_pos, slmX);
                }

        }
    }

}

#else
extern "C" _GENX_MAIN_ void 
GAMMA_ONLY(SurfaceIndex Correct_Index, SurfaceIndex Point_Index,
           SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
           //vector<float, 9> ccm,
           SurfaceIndex Out_Index, // not used
           int blocks_in_a_row, int BitDepth, int framewidth_in_bytes, int frameheight)
           //SurfaceIndex LUT_Index)
{
    ////////////////////////////////////////////////////////////////////////////////////////
    
    cm_slm_init(65536);
    uint slmX = cm_slm_alloc(65536);

    //ENABLE LOOK-UP TABLE COMPUTING
    matrix<ushort, 4, 16> Correct, Point;
    matrix_ref<ushort, 1, 64> gamma_P = Point.format<ushort, 1, 64>();
    
    read(Correct_Index, 0, 0, Correct);
    read(Point_Index  , 0, 0, Point  );

    vector<uint, 16> index(0);
    vector<ushort, 16> v_in_initial(initSeq);

    vector<uint, 64> v_slmData;
    vector<uint, 64> v_slmDataIn;

    int id = cm_linear_local_id();
    int total_load_elements = (1 << BitDepth);                                     // in number of elements
    int max_input_level = total_load_elements - 1;                                    
    int load_elements_per_thread = total_load_elements / cm_linear_local_size(); // in number of elements
    int loop_count = load_elements_per_thread / 16;                              // Each loop handle 16 elements   

    vector<ushort, 16> v_in;

    for(int k = 0; k < loop_count; k++)
    {
        v_in = v_in_initial + (id * load_elements_per_thread + k * 16);
        index = 0;

        vector<ushort, 16> mak;
        vector<ushort, 16> comp_index;

        comp_index = 32;
#pragma unroll
        for(int i = 5; i >= 0; i--)
        {
            int shift = (1 << (i - 1));
            mak     = (gamma_P.iselect(comp_index) <= v_in);
            index  += (mak << i);
            comp_index.merge((comp_index+shift), (comp_index-shift), mak);
        }
        index.merge(0, ((index == 1) & (v_in < gamma_P(0,1))));   

        vector<ushort, 16> c_i, p_i, c_i_plus_1, p_i_plus_1;
        vector<uint, 16> i_plus_1;
        i_plus_1.merge((NUM_CONTROL_POINTS-1), (index + 1), (index == (NUM_CONTROL_POINTS-1)));

        c_i = Correct.iselect(index);
        p_i = Point  .iselect(index);

        c_i_plus_1.merge( max_input_level,  Correct.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));
        p_i_plus_1.merge( max_input_level,  Point  .iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));

        vector<int, 16> v_o;
        
        v_o  = (c_i_plus_1 - c_i);
        v_in = v_in_initial + (id  * load_elements_per_thread + k * 16);
        v_o *= (v_in - p_i);

        //v_o /= (p_i_plus_1 - p_i);

        vector<ushort, 16> tmp = p_i_plus_1 -p_i;
        tmp = cm_max<ushort>(1, tmp);
        v_o /= tmp;

        v_o += c_i;
        
        vector<uint, 16> v_out;
        v_o = cm_max<int>(0, v_o);
        v_out = cm_min<uint>(max_input_level, v_o);
        cm_slm_write(slmX, v_in, v_o);

    }
    cm_barrier();

    /////////////////////////////////////////////////////////////////////////////////
    //////////////////  LOOK-UP TABLE PREPARED   ////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////

    int rd_h_pos, rd_v_pos;
    
    int l_count = (frameheight % 128 == 0)? (frameheight / 128) : ((frameheight / 128)+1);
    for(int l = 0; l < l_count; l++)   // 17 = (frameHeight / (32(H/group)*4 groups))
    {                                   // Four Groups, jointly, one time does 128 pixel high. Need (height/128) loops 
        for(int m = 0; m < 4; m++)
        {
                //EACH thread in the threadgroup handle 256 byte-wide data
                rd_v_pos = cm_linear_group_id() * 32 + m * 8 + l * 128;
#pragma unroll
                for(int i = 0; i < 8; i++)
                {
                    rd_h_pos = cm_linear_local_id() * 256 + 32 * i;             
                    GAMMA_ONLY_CORRECT(R_I_Index, G_I_Index, B_I_Index, rd_h_pos, rd_v_pos, slmX);
                }

        }
    }
}
#endif

_GENX_ void inline
GAMMA_ONLY_CORRECT_16bits(SurfaceIndex BUF_R, SurfaceIndex BUF_G, SurfaceIndex BUF_B,
                          int h_pos, int v_pos, SurfaceIndex LUT16)
{
    matrix<ushort, 8, 16> rd_in_R;
    matrix<ushort, 8, 16> rd_in_G;
    matrix<ushort, 8, 16> rd_in_B;
    matrix<ushort, 8, 16> wr_out;
    
    vector<uint, 16> v_addr;          // Typed uint to fit into the interface
    vector<uint, 16> rd_bufferDataIn; // in 16 bits need 'DW' scatter read

    read(BUF_R, h_pos, v_pos, rd_in_R);
#pragma unroll
    for(int i = 0; i < 8; i++)
    {
        v_addr = rd_in_R.row(i); read(LUT16, 0, v_addr, rd_bufferDataIn); wr_out.row(i) = rd_bufferDataIn;
    }
    write(BUF_R, h_pos, v_pos, wr_out);

    read(BUF_G, h_pos, v_pos, rd_in_G);
#pragma unroll
    for(int i = 0; i < 8; i++)
    {
        v_addr = rd_in_G.row(i); read(LUT16, 0, v_addr, rd_bufferDataIn); wr_out.row(i) = rd_bufferDataIn;
    }
    write(BUF_G, h_pos, v_pos, wr_out);

    read(BUF_B, h_pos, v_pos, rd_in_B);
#pragma unroll
    for(int i = 0; i < 8; i++)
    {
        v_addr = rd_in_B.row(i); read(LUT16, 0, v_addr, rd_bufferDataIn); wr_out.row(i) = rd_bufferDataIn;
    }
    write(BUF_B, h_pos, v_pos, wr_out);
}

extern "C" _GENX_MAIN_ void 
GAMMA_ONLY_16bits(SurfaceIndex Correct_Index, SurfaceIndex Point_Index,
                  SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
                  vector<float, 9> ccm,
                  int blocks_in_a_row, int BitDepth, int framewidth_in_bytes, int frameheight,
                  SurfaceIndex LUT_Index)
{
    //cm_slm_init(65536);
    //uint slmX = cm_slm_alloc(65536);

    //ENABLE LOOK-UP TABLE COMPUTING
    matrix<ushort, 4, 16> Correct, Point;
    matrix_ref<ushort, 1, 64> gamma_P = Point.format<ushort, 1, 64>();
    matrix_ref<ushort, 1, 64> gamma_C = Correct.format<ushort, 1, 64>();

    read(Correct_Index, 0, 0, Correct);
    read(Point_Index  , 0, 0, Point  );

    vector<uint, 16> index(0);
    vector<uint, 16> i_plus_1;
    vector<ushort, 16> v_in_initial(initSeq);

    vector<uint, 64> v_slmData;
    vector<uint, 64> v_slmDataIn;

    int id = cm_linear_local_id();
    int total_load_elements = (1 << BitDepth);                                     // in number of elements
    int max_input_level = total_load_elements - 1;                                    
    int load_elements_per_thread = total_load_elements / cm_linear_local_size(); // in number of elements
    int loop_count = load_elements_per_thread / 16;                              // Each loop handle 16 elements   
    int group_id = cm_linear_group_id();

    vector<ushort, 16> v_in;

    for(int k = 0; k < loop_count; k++)
    { 
        v_in = v_in_initial + (id * load_elements_per_thread + k * 16);

        index = 0;
        vector<ushort, 16> mak;
        vector<ushort, 16> comp_index;

        comp_index = 32;
#pragma unroll
        for(int i = 5; i >= 0; i--)
        {
            int shift = (1 << (i - 1));
            mak     = (gamma_P.iselect(comp_index) <= v_in);
            index  += (mak << i);
            comp_index.merge((comp_index+shift), (comp_index-shift), mak);
        }
        index.merge(0, ((index == 1) & (v_in < gamma_P(0,1))));   
        
        //vector<uint, 16> i_plus_1;
        vector<ushort, 16> c_i, p_i, c_i_plus_1, p_i_plus_1;
        i_plus_1.merge((NUM_CONTROL_POINTS-1), (index + 1), (index == (NUM_CONTROL_POINTS-1)));

        c_i = gamma_C.iselect(index); //c_i = Correct.iselect(index);
        p_i = gamma_P.iselect(index); //p_i = Point  .iselect(index);

        //c_i_plus_1.merge( max_input_level,Correct.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));
        //p_i_plus_1.merge( max_input_level,Point  .iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));
        c_i_plus_1.merge( max_input_level,  gamma_C.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));
        p_i_plus_1.merge( max_input_level,  gamma_P.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));

        vector<int, 16> v_o = cm_add<int>(c_i_plus_1, -c_i);//(c_i_plus_1 - c_i);

        v_o *= cm_add<int>(v_in, -p_i);      //(v_in - p_i);

        vector<ushort, 16> tmp = p_i_plus_1 -p_i;
        tmp = cm_max<ushort>(1, tmp);
        v_o /= tmp;
       
//        v_o /= cm_add<int>(p_i_plus_1, -p_i);//(p_i_plus_1 - p_i);
        
        v_o += c_i;
        
        vector<uint, 16> v_out_dw;
        v_o = cm_max<uint>(0, v_o);
        v_out_dw = cm_min<uint>(max_input_level, v_o);    

        //cm_slm_write(slmX, v_in, v_out);
        int addr = group_id * 65536 * 4 + (id * loop_count + k) * 64;
        write(LUT_Index, addr, v_out_dw);
    }

    cm_barrier();
    
    /////////////////////////////////////////////////////////////////////////////////
    //////////////////  LOOK-UP TABLE PREPARED   ////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////

    int rd_h_pos, rd_v_pos;
    
    int l_count = (frameheight % 128 == 0)? (frameheight / 128) : ((frameheight / 128)+1);
    for(int l = 0; l < l_count; l++)   // 17 = (frameHeight / (32(H/group)*4 groups))
    {                                   // Four Groups, jointly, one time does 128 pixel high. Need (height/128) loops 
        for(int m = 0; m < 4; m++)
        {
                //EACH thread in the threadgroup handle 256 byte-wide data
                rd_v_pos = cm_linear_group_id() * 32 + m * 8 + l * 128;
#pragma unroll
                for(int i = 0; i < 8; i++)
                {
                    rd_h_pos = cm_linear_local_id() * 256 + 32 * i;             
                    GAMMA_ONLY_CORRECT_16bits(R_I_Index, G_I_Index, B_I_Index, rd_h_pos, rd_v_pos, LUT_Index);
                }

        }
    }

}

//  End of Forward Gamma Correction section
////////////////////////////////////////////////////////////////////

extern "C" _GENX_MAIN_  
void ARGB8(SurfaceIndex R_BUF,
           SurfaceIndex G_BUF,
           SurfaceIndex B_BUF,
           SurfaceIndex ARGB8,
           int BitDepth)
           //int stride)           // surface width in number of bytes
{
    int rd_h_pos  = (get_thread_origin_x() * 16) * sizeof(short);
    int rd_v_pos  = (get_thread_origin_y() * 16);

    int wr_h_pos  = (get_thread_origin_x() * 16) * (4 * sizeof(char));
    int wr_v_pos  = (get_thread_origin_y() * 16);
    //int wr_offset = wr_v_pos * stride + wr_h_pos;

    int shift_amount = BitDepth - 8;

    matrix<ushort, 16, 16> in ;
    matrix<uchar , 16, 64> out;
    matrix<uchar , 16, 32> in_byte = in.format<uchar, 16, 32>();

    read(R_BUF, rd_h_pos, rd_v_pos  , in.select<8,1,16,1>(0,0));
    read(R_BUF, rd_h_pos, rd_v_pos+8, in.select<8,1,16,1>(8,0));
    in >>= (shift_amount) ; 
    out.select<16,1,16,4>(0,0) = in;

    read(G_BUF, rd_h_pos, rd_v_pos  , in.select<8,1,16,1>(0,0));
    read(G_BUF, rd_h_pos, rd_v_pos+8, in.select<8,1,16,1>(8,0));
    in >>= (shift_amount) ; 
    out.select<16,1,16,4>(0,1) = in;

    read(B_BUF, rd_h_pos, rd_v_pos  , in.select<8,1,16,1>(0,0));
    read(B_BUF, rd_h_pos, rd_v_pos+8, in.select<8,1,16,1>(8,0));
    in >>= (shift_amount) ; 
    out.select<16,1,16,4>(0,2) = in;

    out.select<16,1,16,4>(0,3) = 0;
    write(ARGB8, wr_h_pos   , wr_v_pos  , out.select<8,1,32,1>(0, 0));
    write(ARGB8, wr_h_pos+32, wr_v_pos  , out.select<8,1,32,1>(0,32));
    write(ARGB8, wr_h_pos   , wr_v_pos+8, out.select<8,1,32,1>(8, 0));
    write(ARGB8, wr_h_pos+32, wr_v_pos+8, out.select<8,1,32,1>(8,32));
}


extern "C" _GENX_MAIN_  
void ARGB16(SurfaceIndex R_BUF,
            SurfaceIndex G_BUF,
            SurfaceIndex B_BUF,
            SurfaceIndex ARGB16,
            int BitDepth)
{
    int rd_h_pos  = (get_thread_origin_x() * 16) * sizeof(short);
    int rd_v_pos  = (get_thread_origin_y() * 16);

    int wr_h_pos  = (get_thread_origin_x() * 16) * (4 * sizeof(short));
    int wr_v_pos  = (get_thread_origin_y() * 16);
    //int wr_offset = wr_v_pos * stride + wr_h_pos;

    matrix<ushort, 16, 16> in ;
    matrix<ushort, 16, 64> out;
    //matrix<uchar , 16, 32> in_byte = in.format<uchar, 16, 32>();

    read(R_BUF, rd_h_pos, rd_v_pos  , in.select<8,1,16,1>(0,0));
    read(R_BUF, rd_h_pos, rd_v_pos+8, in.select<8,1,16,1>(8,0));
    out.select<16,1,16,4>(0,0) = in;

    read(G_BUF, rd_h_pos, rd_v_pos  , in.select<8,1,16,1>(0,0));
    read(G_BUF, rd_h_pos, rd_v_pos+8, in.select<8,1,16,1>(8,0));
    out.select<16,1,16,4>(0,1) = in;

    read(B_BUF, rd_h_pos, rd_v_pos  , in.select<8,1,16,1>(0,0));
    read(B_BUF, rd_h_pos, rd_v_pos+8, in.select<8,1,16,1>(8,0));
    out.select<16,1,16,4>(0,2) = in;

    out.select<16,1,16,4>(0,3) = 0;

    // SHIFT To MSB side
    out <<= (16-BitDepth);

    write(ARGB16, wr_h_pos   , wr_v_pos  , out.select<8,1,16,1>(0, 0));
    write(ARGB16, wr_h_pos+32, wr_v_pos  , out.select<8,1,16,1>(0,16));
    write(ARGB16, wr_h_pos+64, wr_v_pos  , out.select<8,1,16,1>(0,32));
    write(ARGB16, wr_h_pos+96, wr_v_pos  , out.select<8,1,16,1>(0,48));

    write(ARGB16, wr_h_pos   , wr_v_pos+8, out.select<8,1,16,1>(8, 0));
    write(ARGB16, wr_h_pos+32, wr_v_pos+8, out.select<8,1,16,1>(8,16));
    write(ARGB16, wr_h_pos+64, wr_v_pos+8, out.select<8,1,16,1>(8,32));
    write(ARGB16, wr_h_pos+96, wr_v_pos+8, out.select<8,1,16,1>(8,48));
}

///////////////////////////////////////////////////////////
// Black Level Correction
//                 ver 1.0
///////////////////////////////////////////////////////////
_GENX_ void inline
BLACK_LEVEL_CORRECT(matrix_ref<ushort, 16, 16> in,
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

///////////////////////////////////////////////////////////
// Vignette Correction
//                 ver 1.0
///////////////////////////////////////////////////////////
_GENX_ void inline
VIG_CORRECT(matrix_ref<ushort, 16, 16> m,
            matrix_ref<ushort, 16, 16> mask,
            ushort max_input)
{
    matrix<uint, 16, 16> tmp;
    tmp = cm_mul<uint>(m, mask);

    tmp = cm_add <uint>(tmp, 128 );      //8;
    tmp = cm_quot<uint>(tmp, 256);       //16;

    m.merge(max_input, tmp > max_input);
    m.merge(0,           m   < 0        );
}

///////////////////////////////////////////////////////////
// Gain Control ( White Balance )
//                 ver 1.0
///////////////////////////////////////////////////////////
_GENX_ void inline
BAYER_GAIN(matrix_ref<ushort, 16, 16> in,
            float Rscale, float Gtop_scale, float Gbot_scale, float Bscale, ushort max_input)
{
    matrix<uint, 16, 16> m;
    m.select<8, 2, 8, 2>(0,0) = cm_mul<uint>(in.select<8, 2, 8, 2>(0,0), Bscale    );//, SAT);
    m.select<8, 2, 8, 2>(0,1) = cm_mul<uint>(in.select<8, 2, 8, 2>(0,1), Gtop_scale);//, SAT);
    m.select<8, 2, 8, 2>(1,0) = cm_mul<uint>(in.select<8, 2, 8, 2>(1,0), Gbot_scale);//, SAT);
    m.select<8, 2, 8, 2>(1,1) = cm_mul<uint>(in.select<8, 2, 8, 2>(1,1), Rscale    );//, SAT);

    in = m;
    in.merge(0,            m < 0        );
    in.merge(max_input, m > max_input);
}

extern "C" _GENX_MAIN_
void BAYER_CORRECTION(SurfaceIndex PaddedInputIndex,
                      SurfaceIndex BayerIndex,
                      SurfaceIndex MaskIndex,
                      ushort Enable_BLC, ushort Enable_VIG, ushort Enable_WB,
                      short B_amount, short Gtop_amount, short Gbot_amount, short R_amount,
                      float  Rscale  , float  Gtop_scale , float  Gbot_scale , float  Bscale,
                      ushort max_input, int first, int bitDepth, int BayerType)
{
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

    /*
    if(first == 1) // If the first kernel executed
    {
        if(BayerType == GRBG || BayerType == GBRG)
        {
            ROWSWAP_16x16(BayerMatrix);
        }
        BayerMatrix >>= (16-bitDepth);
    }
    */
    /////////////////////////////////////////////////////////////////////////////////////////

    if(Enable_BLC == 1)
        BLACK_LEVEL_CORRECT( BayerMatrix, B_amount, Gtop_amount, Gbot_amount, R_amount, max_input);

     // Note : Mask is not implemented yet, Iwamoto
    if(Enable_VIG == 1)
    {
        read(MaskIndex, h_pos, v_pos  , MaskMatrix.select<8,1,16,1>(0,0));
        read(MaskIndex, h_pos, v_pos+8, MaskMatrix.select<8,1,16,1>(8,0));
        VIG_CORRECT( BayerMatrix, MaskMatrix, max_input);
    }

    if(Enable_WB == 1)
        BAYER_GAIN( BayerMatrix, Rscale,  Gtop_scale,  Gbot_scale,  Bscale,  max_input);

    write(BayerIndex, h_pos, v_pos  , BayerMatrix.select<8,1,16,1>(0,0));
    write(BayerIndex, h_pos, v_pos+8, BayerMatrix.select<8,1,16,1>(8,0));
}

///////////////////////////////////////////////////////////
// Color Correction Matrix
//                 ver 1.0
///////////////////////////////////////////////////////////

_GENX_ void inline
CCM_ONLY_CORRECT(SurfaceIndex BUF_R, SurfaceIndex BUF_G, SurfaceIndex BUF_B,
                 int h_pos, int v_pos, vector<float, 9> ccm, ushort max_input_level)
{
    matrix<ushort, 8, 16> wr_out;
    matrix<ushort, 8, 16> rd_in_R, rd_in_G, rd_in_B;

    read(BUF_R, h_pos, v_pos, rd_in_R);
    read(BUF_G, h_pos, v_pos, rd_in_G);
    read(BUF_B, h_pos, v_pos, rd_in_B);

    matrix<int   , 8, 16> tmp;

#pragma unroll
    for(int j = 0; j < 8; j++)
    {
        tmp.row(j)  = cm_mul<int>(rd_in_R.row(j), ccm(0));
        tmp.row(j) += cm_mul<int>(rd_in_G.row(j), ccm(1));
        tmp.row(j) += cm_mul<int>(rd_in_B.row(j), ccm(2));
        tmp.row(j)  = cm_max<int>(tmp.row(j), 0);//cm_max<ushort>(tmp.row(j), 0);
        wr_out.row(j)  = cm_min<int>(tmp.row(j), max_input_level);//cm_min<ushort>(wr_out.row(j), max_input_level);
    }
    write(BUF_R, h_pos, v_pos, wr_out);

#pragma unroll
    for(int j = 0; j < 8; j++)
    {
        tmp.row(j)  = cm_mul<int>(rd_in_R.row(j), ccm(3));
        tmp.row(j) += cm_mul<int>(rd_in_G.row(j), ccm(4));
        tmp.row(j) += cm_mul<int>(rd_in_B.row(j), ccm(5));
        tmp.row(j)  = cm_max<int>(tmp.row(j), 0);//cm_max<ushort>(tmp.row(j), 0);
        wr_out.row(j)  = cm_min<int>(tmp.row(j), max_input_level);//cm_min<ushort>(wr_out.row(j), max_input_level);
    }
    write(BUF_G, h_pos, v_pos, wr_out);

#pragma unroll
    for(int j = 0; j < 8; j++)
    {
        tmp.row(j)  = cm_mul<int>(rd_in_R.row(j), ccm(6));
        tmp.row(j) += cm_mul<int>(rd_in_G.row(j), ccm(7));
        tmp.row(j) += cm_mul<int>(rd_in_B.row(j), ccm(8));
        tmp.row(j)  = cm_max<int>(tmp.row(j), 0);//cm_max<ushort>(tmp.row(j), 0);
        wr_out.row(j)  = cm_min<int>(tmp.row(j), max_input_level);//cm_min<ushort>(wr_out.row(j), max_input_level);
    }
    write(BUF_B, h_pos, v_pos, wr_out);
}

extern "C" _GENX_MAIN_ void
CCM_ONLY(SurfaceIndex Correct_Index, SurfaceIndex Point_Index,
         SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
         vector<float, 9> ccm,
         int blocks_in_a_row, int BitDepth, int framewidth_in_bytes, int frameheight,
         SurfaceIndex LUT_Index)
{
    int l_count = (frameheight % 128 == 0)? (frameheight / 128) : ((frameheight / 128)+1);
    ushort max_input_level = (1 << BitDepth) - 1;

    int rd_v_pos, rd_h_pos;
    for(int l = 0; l < l_count; l++)    // 17 = (frameHeight / (32(H/group)*4 groups))
    {                                   // Four Groups, jointly, one time does 128 pixel high. Need (height/128) loops
        for(int m = 0; m < 4; m++)
        {
                //EACH thread in the threadgroup handle 256 byte-wide data
                rd_v_pos = cm_linear_group_id() * 32 + m * 8 + l * 128;
#pragma unroll
                for(int i = 0; i < 8; i++)
                {
                    rd_h_pos = cm_linear_local_id() * 256 + 32 * i;
                    CCM_ONLY_CORRECT(R_I_Index, G_I_Index, B_I_Index, rd_h_pos, rd_v_pos, ccm, max_input_level);
                }
        }
    }
}
