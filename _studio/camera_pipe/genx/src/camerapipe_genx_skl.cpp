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


/////////////////////////////////////////////////////////////////////
// SKL-light Demosaic GPU implementation
// Unit test : Demosaic_SKL_4K2K.cpp
/////////////////////////////////////////////////////////////////////

// constant for EU exec
#define WR_BLK_HEIGHT        8
#define WR_BLK_WIDTH         8
#define RD_BLK_WIDTH        16

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
#define NORMALWALKER 0
#define Average_Color_TH   100

#define Grn_imbalance_th     0

#define m_Bad_TH1          100
#define Bad_TH1            100
#define m_Bad_TH2          175
#define Bad_TH2            175
#define m_Bad_TH3           10
#define Bad_TH3             10

// other constant
#ifdef CMRT_EMU
#define WD     12
#define WIDTH   6
#define W      15
#else
#define WD     16
#define WIDTH   8
#define W      16
#endif

#define bggr 0
#define rggb 1
#define gbrg 2
#define grbg 3

// implementation

_GENX_ inline
int get_new_origin_x(int thW, int thH, int CW)
{
    int y = get_thread_origin_y();
    int x = get_thread_origin_x();

    int NC = (thW % CW == 0)? (thW / CW) : ((thW / CW)+1);
    int id = y * thW + x;
    int col_id = id / (CW * thH);
    int col_W = (col_id != (NC - 1) || (thW % CW == 0))? CW : (thW % CW);

    int id_in_col = id % (col_W * thH);

    int out_x = (id_in_col % col_W) + col_id * CW;

    return out_x;
}

_GENX_ inline
int get_new_origin_y(int thW, int thH, int CW)
{
    int y = get_thread_origin_y();
    int x = get_thread_origin_x();

    int NC = (thW % CW == 0)? (thW / CW) : ((thW / CW)+1);
    int id = y * thW + x;
    int col_id = id / (CW * thH);
    int col_W = (col_id != (NC - 1) || (thW % CW == 0))? CW : (thW % CW);

    int id_in_col = id % (col_W * thH);

    int out_y = (id_in_col / col_W);

    return out_y;
}

extern "C" _GENX_MAIN_ void
Unpack(SurfaceIndex ibuf, SurfaceIndex obuf)
{
    int rd_h_pos = get_thread_origin_x() * 32;
    int wr_h_pos = get_thread_origin_x() * 48;
    int v_pos    = get_thread_origin_y() * 16;

    matrix<uint  , 16, 8 > in;   //<16, 24> pixels
    matrix<ushort, 16, 24> out;

    read(ibuf, rd_h_pos   , v_pos, in.select<16,1,4,1>(0,0));
    read(ibuf, rd_h_pos+16, v_pos, in.select<16,1,4,1>(0,4));

    out.select<16,1,8,3>(0,0) = (in & 0x00000FFC) >> ( 2);
    out.select<16,1,8,3>(0,1) = (in & 0x003FF000) >> (12);
    out.select<16,1,8,3>(0,2) = (in & 0xFFC00000) >> (22);

    write(obuf, wr_h_pos   , v_pos, out.select<16,1,8,1>(0,0 ));
    write(obuf, wr_h_pos+16, v_pos, out.select<16,1,8,1>(0,8 ));
    write(obuf, wr_h_pos+32, v_pos, out.select<16,1,8,1>(0,16));
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
    int byte_shift_amount = (BayerType == grbg || BayerType == gbrg)? 1 : 0;

    for(int wr_h_pos = wr_hor_pos; wr_h_pos < (wr_hor_pos + 32); wr_h_pos += 16) {
        for(int wr_v_pos = wr_ver_pos; wr_v_pos < (wr_ver_pos + 16); wr_v_pos += 8) {

            // compute rd_h_pos, rd_v_pos
            if(wr_h_pos == 0)
                rd_h_pos =  byte_shift_amount * sizeof(short);
            else if(wr_h_pos == (threadwidth - 1) * 8 * sizeof(short))
                rd_h_pos = (InFrameWidth - 8 - byte_shift_amount) * sizeof(short);//(InFrameWidth - 9)* sizeof(short);
            else
                rd_h_pos = wr_h_pos - 16 + byte_shift_amount * sizeof(short);//rd_h_pos = (get_thread_origin_x() - 1) * 8 * sizeof(short);

            if(wr_v_pos == 0)
                rd_v_pos = 0;//1;
            else if(wr_v_pos == (threadheight - 1) * 8)
                rd_v_pos = (InFrameHeight - 8);//(InFrameHeight - 9);
            else
                rd_v_pos = wr_v_pos - 8;//wr_v_pos - 7; (get_thread_origin_y() - 1) * 8;

            // READ FROM SURFACE
            read(ibuf, rd_h_pos, rd_v_pos, rd_in_pixels);

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
            // SWAP IF NEEDED
            write(obuf, wr_h_pos, wr_v_pos, out);
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
GPU_GOOD_PIXEL_CHECK_16x16_SKL( SurfaceIndex ibuf ,
                                SurfaceIndex obuf0, SurfaceIndex obuf1,
                                int shift)
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

    read(ibuf, rd_h_pos   , rd_v_pos   , rd_in.select<8,1,16,1>(0, 0 ));
    read(ibuf, rd_h_pos+32, rd_v_pos   , rd_in.select<8,1,16,1>(0, 16));
    read(ibuf, rd_h_pos   , rd_v_pos+8 , rd_in.select<8,1,16,1>(8, 0 ));
    read(ibuf, rd_h_pos+32, rd_v_pos+8 , rd_in.select<8,1,16,1>(8, 16));
    read(ibuf, rd_h_pos   , rd_v_pos+16, rd_in.select<4,1,16,1>(16,0 ));
    read(ibuf, rd_h_pos+32, rd_v_pos+16, rd_in.select<4,1,16,1>(16,16));

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
GPU_RESTORE_GREEN_SKL( SurfaceIndex ibufBayer,
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

    matrix<short, 12,16> srcBayer;
    matrix<short, 8, 8 > G_H, G_V, G_A;

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
    matrix<short, 8, 8> tmp0, tmp1;

    /////////////  COMPUTE GREEN VERTICAL COMPONENT /////////////////////////////////

    M = (cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(0,2), -srcBayer.select<8,1,8,1>(2,2))) <
         cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(2,2), -srcBayer.select<8,1,8,1>(4,2))));

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

    M = (cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(2,0), -srcBayer.select<8,1,8,1>(2,2))) <
         cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(2,2), -srcBayer.select<8,1,8,1>(2,4))));

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

    matrix_ref<ushort, 8, 8> tmpA = tmp0.format<ushort, 8, 8>();
    matrix_ref<ushort, 8, 8> tmpB = tmp1.format<ushort, 8, 8>();

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
GPU_RESTORE_BR_SKL( SurfaceIndex ibufBayer,
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

    matrix<short, 8, 8> out_B, out_R; //matrix<ushort, 8, 8> out_B, out_R;

     matrix< int  , 8,12> BR32_c;
    matrix< int  ,12, 8> BR34_c;
    matrix< int  , 8, 8> BR33_c;

    matrix<ushort, 8,12> BR32_s;
    matrix<ushort,12, 8> BR34_s;
    matrix<ushort, 8, 8> BR33_s;

    matrix<short, 12, 16> dBG_c;
    matrix<short, 12, 16> Green_c, G_c_x2;

    /////// HORIZONTAL COMPONENT ////////////////////////////////////

    read(ibufGHOR, rd_h_pos, rd_v_pos  , Green_c.select<8,1,16,1>(0,0));
    read(ibufGHOR, rd_h_pos, rd_v_pos+8, Green_c.select<4,1,16,1>(8,0));

    dBG_c = bayer - Green_c;
    G_c_x2 = cm_mul<int>(Green_c, 2);

    BR32_c  = G_c_x2 .select<8,1,12,1>(2,0);
    BR32_c += dBG_c  .select<8,1,12,1>(1,0);
    BR32_c += dBG_c  .select<8,1,12,1>(3,0);
    BR32_c >>= 1;
    BR32_s = cm_max<ushort>(BR32_c, 0);
    BR32_s = cm_min<ushort>(BR32_s, max_intensity);

    BR34_c  = G_c_x2 .select<12,1,8,1>(0,2);
    BR34_c += dBG_c  .select<12,1,8,1>(0,1);
    BR34_c += dBG_c  .select<12,1,8,1>(0,3);
    BR34_c >>= 1;
    BR34_s = cm_max<ushort>(BR34_c, 0);
    BR34_s = cm_min<ushort>(BR34_s, max_intensity);

    BR33_c  = G_c_x2 .select<8,1,8,1>(2,2);
    BR33_c += BR32_s .select<8,1,8,1>(0,1);
    BR33_c -= Green_c.select<8,1,8,1>(2,1);
    BR33_c += BR32_s .select<8,1,8,1>(0,3);
    BR33_c -= Green_c.select<8,1,8,1>(2,3);
    BR33_c >>= 1;
    BR33_s = cm_max<ushort>(BR33_c, 0);
    BR33_s = cm_min<ushort>(BR33_s, max_intensity);

    out_B.select<4,2,4,2>(0,0) = bayer .select<4,2,4,2>(2,2);
    out_R.select<4,2,4,2>(1,1) = bayer .select<4,2,4,2>(3,3);

    out_B.select<4,2,4,2>(0,1) = BR34_s.select<4,2,4,2>(2,1);
    out_B.select<4,2,4,2>(1,0) = BR32_s.select<4,2,4,2>(1,2);
    out_B.select<4,2,4,2>(1,1) = BR33_s.select<4,2,4,2>(1,1);

    out_R.select<4,2,4,2>(0,1) = BR32_s.select<4,2,4,2>(0,3);
    out_R.select<4,2,4,2>(1,0) = BR34_s.select<4,2,4,2>(3,0);
    out_R.select<4,2,4,2>(0,0) = BR33_s.select<4,2,4,2>(0,0);

    write(obufBHOR, wr_h_pos, wr_v_pos, out_B);
    write(obufRHOR, wr_h_pos, wr_v_pos, out_R);

    /////// VERTICAL COMPONENT ////////////////////////////////////

    read(ibufGVER, rd_h_pos, rd_v_pos  , Green_c.select<8,1,16,1>(0,0));
    read(ibufGVER, rd_h_pos, rd_v_pos+8, Green_c.select<4,1,16,1>(8,0));

    dBG_c = bayer - Green_c;
    G_c_x2 = cm_mul<int>(Green_c, 2);

    BR32_c  = G_c_x2 .select<8,1,12,1>(2,0);
    BR32_c += dBG_c  .select<8,1,12,1>(1,0);
    BR32_c += dBG_c  .select<8,1,12,1>(3,0);
    BR32_c >>= 1;
    BR32_s.select<8,1,8,1>(0,2) = cm_max<ushort>(BR32_c.select<8,1,8,1>(0,2), 0);
    BR32_s = cm_min<ushort>(BR32_s, max_intensity);

    BR34_c  = G_c_x2 .select<12,1,8,1>(0,2);
    BR34_c += dBG_c  .select<12,1,8,1>(0,1);
    BR34_c += dBG_c  .select<12,1,8,1>(0,3);
    BR34_c >>= 1;
    BR34_s = cm_max<ushort>(BR34_c, 0);
    BR34_s = cm_min<ushort>(BR34_s, max_intensity);

    BR33_c  = G_c_x2 .select<8,1,8,1>(2,2);
    BR33_c += BR34_s .select<8,1,8,1>(1,0);
    BR33_c -= Green_c.select<8,1,8,1>(1,2);
    BR33_c += BR34_s .select<8,1,8,1>(3,0);
    BR33_c -= Green_c.select<8,1,8,1>(3,2);
    BR33_c >>= 1;
    BR33_s = cm_max<ushort>(BR33_c, 0);
    BR33_s = cm_min<ushort>(BR33_s, max_intensity);

    out_B.select<4,2,4,2>(0,0) = bayer .select<4,2,4,2>(2,2);
    out_R.select<4,2,4,2>(1,1) = bayer .select<4,2,4,2>(3,3);

    out_B.select<4,2,4,2>(0,1) = BR34_s.select<4,2,4,2>(2,1);
    out_B.select<4,2,4,2>(1,0) = BR32_s.select<4,2,4,2>(1,2);
    out_B.select<4,2,4,2>(1,1) = BR33_s.select<4,2,4,2>(1,1);

    out_R.select<4,2,4,2>(0,1) = BR32_s.select<4,2,4,2>(0,3);
    out_R.select<4,2,4,2>(1,0) = BR34_s.select<4,2,4,2>(3,0);
    out_R.select<4,2,4,2>(0,0) = BR33_s.select<4,2,4,2>(0,0);

    write(obufBVER, wr_h_pos, wr_v_pos, out_B);
    write(obufRVER, wr_h_pos, wr_v_pos, out_R);
    /////// AVERAGE COMPONENT ////////////////////////////////////
    matrix_ref<uchar, 8, 8> avg_mask = dBG_c.select<2,1,16,1>(0,0).format<uchar, 8, 8>();

    read(ibufAVGMASK, wr_h_pos / 2, wr_v_pos, avg_mask);
    if((avg_mask == AVG_True).any())
    {
    read(ibufGAVG, rd_h_pos, rd_v_pos  , Green_c.select<8,1,16,1>(0,0));
    read(ibufGAVG, rd_h_pos, rd_v_pos+8, Green_c.select<4,1,16,1>(8,0));

    dBG_c = bayer - Green_c;
    G_c_x2 = cm_mul<int>(Green_c, 2);

    BR32_c  = G_c_x2.select<8,1,12,1>(2,0);
    BR32_c += dBG_c.select< 8,1,12,1>(1,0);
    BR32_c += dBG_c.select< 8,1,12,1>(3,0);
    BR32_c >>= 1;
    BR32_s = cm_max<ushort>(BR32_c, 0);
    BR32_s = cm_min<ushort>(BR32_s, max_intensity);

    BR34_c  = G_c_x2.select<12,1,8,1>(0,2);
    BR34_c += dBG_c .select<12,1,8,1>(0,1);
    BR34_c += dBG_c .select<12,1,8,1>(0,3);
    BR34_c >>= 1;
    BR34_s = cm_max<ushort>(BR34_c, 0);
    BR34_s = cm_min<ushort>(BR34_s, max_intensity);

    BR33_c  = cm_mul<int>(G_c_x2.select<8,1,8,1>(2,2), 2);
    BR33_c += BR34_s .select<8,1,8,1>(1,0);
    BR33_c -= Green_c.select<8,1,8,1>(1,2);
    BR33_c += BR34_s .select<8,1,8,1>(3,0);
    BR33_c -= Green_c.select<8,1,8,1>(3,2);
    BR33_c += BR32_s .select<8,1,8,1>(0,1);
    BR33_c -= Green_c.select<8,1,8,1>(2,1);
    BR33_c += BR32_s .select<8,1,8,1>(0,3);
    BR33_c -= Green_c.select<8,1,8,1>(2,3);
    BR33_c >>= 2;
    BR33_s = cm_max<ushort>(BR33_c, 0);
    BR33_s = cm_min<ushort>(BR33_s, max_intensity);

    out_B.select<4,2,4,2>(0,0) = bayer .select<4,2,4,2>(2,2);
    out_R.select<4,2,4,2>(1,1) = bayer .select<4,2,4,2>(3,3);

    out_B.select<4,2,4,2>(0,1) = BR34_s.select<4,2,4,2>(2,1);
    out_B.select<4,2,4,2>(1,0) = BR32_s.select<4,2,4,2>(1,2);
    out_B.select<4,2,4,2>(1,1) = BR33_s.select<4,2,4,2>(1,1);

    out_R.select<4,2,4,2>(0,1) = BR32_s.select<4,2,4,2>(0,3);
    out_R.select<4,2,4,2>(1,0) = BR34_s.select<4,2,4,2>(3,0);
    out_R.select<4,2,4,2>(0,0) = BR33_s.select<4,2,4,2>(0,0);

    write(obufBAVG, wr_h_pos, wr_v_pos, out_B);
    write(obufRAVG, wr_h_pos, wr_v_pos, out_R);
    }

}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////     SAD     //////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// BITDEPTH 14 VERSION OF SAD KERNEL
extern "C" _GENX_MAIN_ void
GPU_SAD_CONFCHK_SKL( SurfaceIndex ibufRHOR, SurfaceIndex ibufGHOR, SurfaceIndex ibufBHOR,
                     SurfaceIndex ibufRVER, SurfaceIndex ibufGVER, SurfaceIndex ibufBVER,
                     int x , int y, int wr_x, int wr_y,
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

    matrix_ref<short, 8, 8> outR = tmp.select<4,1,16,1>(0,0) .format<short, 8, 8>();
    matrix_ref<short, 8, 8> outG = tmp.select<4,1,16,1>(4,0) .format<short, 8, 8>();
    matrix_ref<short, 8, 8> outB = tmp.select<4,1,16,1>(8,0) .format<short, 8, 8>();
    matrix_ref<short, 8, 8> out  = tmp.select<4,1,16,1>(12,0).format<short, 8, 8>();
    matrix_ref<int,   8, 8> sum  = Min_cost_H;

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
HORIZONTAL_CHECK(matrix_ref<short, 13, 32> srcG, matrix_ref<ushort, 8, 16> maskH)
{

    matrix< short, 4, 16 >   diff;
#pragma unroll
    for(int j = 0; j < 8; j++) // j == index of output registers
    {
        vector<short, 32> rowG = srcG.row(2+j);

        matrix_ref<short, 8, 4> m_rowG = rowG.format<short, 8, 4>();
        matrix<short, 5, 16> v;

#ifdef CMRT_EMU
        vector<short, 16> idx(idx_init);
        vector<ushort, 16> index;
        v.row(0) = rowG.iselect(idx);
#pragma unroll
        for(int j = 0; j < 4; j++) {
            index = idx + (j + 1);
            v.row(j+1) = rowG.iselect(index);
            diff.row(j) = v.row(j) - v.row(j+1);
        }
#else

        v.select<1,1,8,2>(0,0) = m_rowG.genx_select<4,4,2,1>(0,1);
        v.select<1,1,8,2>(0,1) = v.select<1,1,8,2>(0,0);

#pragma unroll
        for(int i = 1; i < 5; i++) {
            v.select<1,1,8,2>(i,0) = m_rowG.genx_select<4,4,2,1>(0,i+1);
            v.select<1,1,8,2>(i,1) = v.select<1,1,8,2>(i,0);

            diff.row(i-1) = v.row(i-1) - v.row(i);
        }
#endif

        maskH.row(j)  = (cm_abs<ushort>(diff.row(3)) > Grn_imbalance_th);
#pragma unroll
        for(int i = 0; i < 3; i++) {
            maskH.row(j) &= (cm_mul<int> (diff.row(i),diff.row(i+1)) < 0);
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
#pragma unroll
        for(int i = 0; i < 3; i++) {
            maskV.row(j) &= (cm_mul<int> (diff.row(i),diff.row(i+1)) < 0);
            maskV.row(j) &= (cm_abs<ushort>(diff.row(i)) > Grn_imbalance_th);
        }
        maskV.row(j+1)  = maskV.row(j);
    }

    //{2,3,6,7} row
#pragma unroll
    for(int j = 2; j <= 6; j += 4 ) {
        diff          = (cenG.select<4,1,16,1>(  j,0) - cenG.select<4,1,16,1>(1+j,0));

        maskV.row(j)  = (cm_abs<ushort>(diff.row(3)) > Grn_imbalance_th);
#pragma unroll
        for(int i = 0; i < 3; i++) {
            maskV.row(j) &= (cm_mul<int> (diff.row(i),diff.row(i+1)) < 0);
            maskV.row(j) &= (cm_abs<ushort>(diff.row(i)) > Grn_imbalance_th);
        }
        maskV.row(j+1) = maskV.row(j);
    }
}

extern "C" _GENX_ void inline
COLORDIFF_CHECK(matrix_ref<short , 8, 16> srcR, matrix_ref<short, 8, 16> srcG, matrix_ref<short, 8, 16> srcB,
                matrix_ref<ushort, 8, 16> maskC)
{
    matrix< short, 8, 16 > absv, difference;

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
GPU_DECIDE_AVG16x16_SKL( SurfaceIndex ibufRAVG, SurfaceIndex ibufGAVG, SurfaceIndex ibufBAVG,
                         SurfaceIndex ibufAVG_Flag,
                         SurfaceIndex obufROUT, SurfaceIndex obufGOUT, SurfaceIndex obufBOUT, int wr_x, int wr_y)
{
    matrix< short, 13,32 >   srcG;
    matrix< short, 8, 16 >   srcR, srcB, absv, difference;

    matrix< uchar,16, 16 >   AVG_flag;
    matrix<ushort, 8, 16 >     AVG_Mask, out;
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

    AVG_Mask = (AVG_flag.select<8,1,16,1>(0,0) == AVG_True);
    if(AVG_Mask.any())
    {
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

    srcG.select<5,1,16,1>(0,0 ) = srcG.select<5,1,16,1>(8,0 );
    srcG.select<5,1,16,1>(0,16) = srcG.select<5,1,16,1>(8,16);
    read(ibufGAVG, rd_h_pos   , rd_v_pos+5 , srcG.select<8,1,16,1>(5,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos+5 , srcG.select<8,1,16,1>(5,16));

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
// SKL Vignette Correction GPU implementation
// Unit test : Vignette.cpp
/////////////////////////////////////////////////////////////////////

#ifndef BLOCK_WIDTH
#define BLOCK_WIDTH 8
#endif

#ifndef BLOCK_HEIGHT
#define BLOCK_HEIGHT 8
#endif

//#include "cm/cm.h"
//const float offset_x[BLOCK_HEIGHT][BLOCK_WIDTH] = {{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7}
//                                                  ,{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7}};
const short offset_x[BLOCK_HEIGHT][BLOCK_WIDTH] = {{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7}
                                                  ,{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7},{0,1,2,3,4,5,6,7}};
//const float offset_y[BLOCK_HEIGHT][BLOCK_WIDTH] = {{0,0,0,0,0,0,0,0},{1,1,1,1,1,1,1,1},{2,2,2,2,2,2,2,2},{3,3,3,3,3,3,3,3}
//                                                  ,{4,4,4,4,4,4,4,4},{5,5,5,5,5,5,5,5},{6,6,6,6,6,6,6,6},{7,7,7,7,7,7,7,7}};
const short offset_y[BLOCK_HEIGHT][BLOCK_WIDTH] = {{0,0,0,0,0,0,0,0},{1,1,1,1,1,1,1,1},{2,2,2,2,2,2,2,2},{3,3,3,3,3,3,3,3}
                                                  ,{4,4,4,4,4,4,4,4},{5,5,5,5,5,5,5,5},{6,6,6,6,6,6,6,6},{7,7,7,7,7,7,7,7}};

extern "C" _GENX_MAIN_ void GenVMask(SurfaceIndex MaskIndex,
                                     SurfaceIndex Mask4x4Index,
                                     int Width,
                                     int Height,
                                     //float angle,
                                     float MAX_ANGLE,
                                     int effect,
                                     int h_pos)
{
    short img_center_x = (short)(Width / 2);
    short img_center_y = (short)(Height / 2);
    //float max_ImgRad = sqrt( pow((double)Width/2, 2)+pow((double)Height/2, 2) );
    float max_ImgRad = cm_sqrt((float)(Width/2)*(float)(Width/2) + (float)(Height/2)*(float)(Height/2));
    //float MAX_ANGLE = 3.14159 / (180/(angle));

    matrix<ushort, BLOCK_HEIGHT, BLOCK_WIDTH> MaskMatrix;
    matrix<ushort, BLOCK_HEIGHT/4, BLOCK_WIDTH/4> Mask4x4Matrix;
    //double Distance;
    matrix<float, BLOCK_HEIGHT, BLOCK_WIDTH> Distance;
    //double DistanceRatio;
    //matrix<float, BLOCK_HEIGHT, BLOCK_WIDTH> DistRatio;
    //double EffectiveScale;
    matrix<float, BLOCK_HEIGHT, BLOCK_WIDTH> EffectScale;

    //matrix<float, BLOCK_HEIGHT, BLOCK_WIDTH> x_offset(offset_x);
    matrix<short, BLOCK_HEIGHT, BLOCK_WIDTH> x_offset(offset_x);
    //matrix<float, BLOCK_HEIGHT, BLOCK_WIDTH> y_offset(offset_y);
    matrix<short, BLOCK_HEIGHT, BLOCK_WIDTH> y_offset(offset_y);


    //matrix<float, BLOCK_HEIGHT, BLOCK_WIDTH> x_cord;
    matrix<short, BLOCK_HEIGHT, BLOCK_WIDTH> x_cord;

    x_cord = x_offset + (short)(h_pos/sizeof(short));
    //x_offset += (h_pos/sizeof(short));
    //x_cord += x_offset;

    int dummy =  0;
    for(int rowpos = 0 ; rowpos < Height ; rowpos += BLOCK_HEIGHT){
        //if(h_pos == 176 && rowpos == 152)
        //    dummy = 1;
        //matrix<float, BLOCK_HEIGHT, BLOCK_WIDTH> y_cord;
        matrix<short, BLOCK_HEIGHT, BLOCK_WIDTH> y_cord;

        y_cord =  y_offset + (short)rowpos;
        matrix<short, BLOCK_HEIGHT, BLOCK_WIDTH> dx = x_cord - img_center_x;
        matrix<short, BLOCK_HEIGHT, BLOCK_WIDTH> dy = y_cord - img_center_y;
#if 1
         Distance = dx * dx + dy * dy;
#else
        Distance  = cm_mul<short>(dx,dx);
        Distance += cm_mul<short>(dy,dy);
#endif
        Distance = cm_sqrt(Distance);
        Distance /= max_ImgRad;
        if (effect == 0){
            EffectScale = cm_cos(Distance * MAX_ANGLE);
            EffectScale = cm_pow(EffectScale ,4);
            EffectScale = EffectScale * (float)(1024.0) + (float)0.5;
        }
        else if(effect == 1){
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
                                                      //{0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3},{0,1,2,3,4,5,2,3,0,1,2,3,0,1,2,3},
                                                      //{0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3},{0,1,2,3,4,5,2,3,0,1,2,3,0,1,2,3},
                                                      //{0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3},{0,1,2,3,4,5,2,3,0,1,2,3,0,1,2,3},
                                                      //{0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3},{0,1,2,3,4,5,2,3,0,1,2,3,0,1,2,3},
                                                      //{0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3},{0,1,2,3,4,5,2,3,0,1,2,3,0,1,2,3},
                                                      //{0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3},{0,1,2,3,4,5,2,3,0,1,2,3,0,1,2,3}};
const short four_minus_modXini[BLOCKHEIGHT*BLOCKWIDTH] = {4,3,2,1,4,3,2,1,4,3,2,1,4,3,2,1};
const short modY_ini[BLOCKHEIGHT][BLOCKWIDTH] = {{0,0,0,0},{1,1,1,1},{2,2,2,2},{3,3,3,3}};
                                                      //{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
                                                       //{2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2},{3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3},
                                                      //{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
                                                      //{2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2},{3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3},
                                                      //{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
                                                      //{2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2},{3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3}};

extern "C" _GENX_MAIN_ void GenUpSampledMask(SurfaceIndex Mask4x4Index,
                                              SurfaceIndex UpSampleMaskIndex,
                                              int Width,   // Width  of Output Mask surface
                                              int Height,  // Height of Output Mask surface
                                              int h_pos){  // Horizontal position on Mask Output surface, //thread argument

     //In a single 16x16 block the modX, four_minus_modX, modY, four_minus_modY are the same
     vector<ushort, 16> modX(modXini);
     vector<ushort, 16> four_minus_modX(four_minus_modXini);
     vector<ushort, 16> modY;
     vector<ushort, 16> four_minus_modY;
     matrix<ushort, 16, 16> output_Matrix;
     //int dummy = 0;

     for(int rowpos = 0; rowpos < Height; rowpos += (BLOCKHEIGHT*4)){    //loop over 'column' of blocks of size 16x16
     //int rowpos = 0;
     //if(rowpos == 0 && h_pos == 1504)
        //dummy = 1;
        //Read 5x5 data from Mask4x4Index surface
        matrix<ushort, 5, 6> data5x5;
        //printf("h_pos: %d, h_pos/4: %d\n",h_pos,h_pos/4);
        read(Mask4x4Index, h_pos/4, rowpos/4, data5x5);

        //Generate a4x4, b4x4, c4x4, d4x4 matrices for computing the 16x16 output matrix
        matrix<ushort, 4, 4> a4x4; matrix<ushort, 4, 4> b4x4; matrix<ushort, 4, 4> c4x4; matrix<ushort, 4, 4> d4x4;
        a4x4 = data5x5.select<4,1,4,1>(0,0);
        b4x4 = data5x5.select<4,1,4,1>(0,1);
        c4x4 = data5x5.select<4,1,4,1>(1,0);
        d4x4 = data5x5.select<4,1,4,1>(1,1);

        if(h_pos == (Width - 16) * sizeof(short)){
            b4x4.column(3) = b4x4.column(2);
            d4x4.column(3) = d4x4.column(2);
        }
        if(rowpos == (Height - 4)){
            c4x4.row(3) = c4x4.row(2);
            d4x4.row(3) = d4x4.row(2);
        }

        //REPEAT 16 TIMES:
        //Use a16, b16, c16, d16 to compute a row (1x16) of the 16x16 output matrix
        vector<ushort, 16> a16; vector<ushort, 16> b16; vector<ushort, 16> c16; vector<ushort, 16> d16;

        //First 4 times ===============================================================================================================
        //Upper quarter of the frame: a16, b16, c16, d16 are common
        a16.select<4,1>(0) = a4x4(0,0); a16.select<4,1>(4) = a4x4(0,1); a16.select<4,1>(8) = a4x4(0,2); a16.select<4,1>(12) = a4x4(0,3);
        b16.select<4,1>(0) = b4x4(0,0); b16.select<4,1>(4) = b4x4(0,1); b16.select<4,1>(8) = b4x4(0,2); b16.select<4,1>(12) = b4x4(0,3);
        c16.select<4,1>(0) = c4x4(0,0); c16.select<4,1>(4) = c4x4(0,1); c16.select<4,1>(8) = c4x4(0,2); c16.select<4,1>(12) = c4x4(0,3);
        d16.select<4,1>(0) = d4x4(0,0); d16.select<4,1>(4) = d4x4(0,1); d16.select<4,1>(8) = d4x4(0,2); d16.select<4,1>(12) = d4x4(0,3);


        vector<ushort, 16> op1 = ( a16 * (four_minus_modX) + b16 * (modX) + 2) / 4;
        vector<ushort, 16> op2 = ( c16 * (four_minus_modX) + d16 * (modX) + 2) / 4;
#pragma unroll
        for(int i = 0; i < 4; i++){
            modY = i;
            four_minus_modY = (4 - i);
            vector<ushort, 16> output_row;
            output_row  = (four_minus_modY) * (op1);
            output_row += (modY) * (op2) + 2;
            output_row /= 4;
            output_Matrix.row(i) = output_row;
        }
        /*modY = 0;
        four_minus_modY = 4; //Will differ once we shift to another row

        output_row  = (four_minus_modY) * (( a16 * (four_minus_modX) + b16 * (modX) + 2) / 4);
        output_row += (modY) * (( c16 * (four_minus_modX) + d16 * (modX) + 2) / 4) + 2;
        output_row /= 4;
        output_Matrix.row(0) = output_row;


        modY = 1;
        four_minus_modY = 3; //Will differ once we shift to another row

        output_row  = (four_minus_modY) * (( a16 * (four_minus_modX) + b16 * (modX) + 2) / 4);
        output_row += (modY) * (( c16 * (four_minus_modX) + d16 * (modX) + 2) / 4) + 2;
        output_row /= 4;
        output_Matrix.row(1) = output_row;

        modY = 2;
        four_minus_modY = 2; //Will differ once we shift to another row

        output_row  = (four_minus_modY) * (( a16 * (four_minus_modX) + b16 * (modX) + 2) / 4);
        output_row += (modY) * (( c16 * (four_minus_modX) + d16 * (modX) + 2) / 4) + 2;
        output_row /= 4;
        output_Matrix.row(2) = output_row;

        modY = 3;
        four_minus_modY = 1; //Will differ once we shift to another row

        output_row  = (four_minus_modY) * (( a16 * (four_minus_modX) + b16 * (modX) + 2) / 4);
        output_row += (modY) * (( c16 * (four_minus_modX) + d16 * (modX) + 2) / 4) + 2;
        output_row /= 4;
        output_Matrix.row(3) = output_row;*/
#if 1
        //First 4 times ===============================================================================================================
        a16.select<4,1>(0) = a4x4(1,0); a16.select<4,1>(4) = a4x4(1,1); a16.select<4,1>(8) = a4x4(1,2); a16.select<4,1>(12) = a4x4(1,3);
        b16.select<4,1>(0) = b4x4(1,0); b16.select<4,1>(4) = b4x4(1,1); b16.select<4,1>(8) = b4x4(1,2); b16.select<4,1>(12) = b4x4(1,3);
        c16.select<4,1>(0) = c4x4(1,0); c16.select<4,1>(4) = c4x4(1,1); c16.select<4,1>(8) = c4x4(1,2); c16.select<4,1>(12) = c4x4(1,3);
        d16.select<4,1>(0) = d4x4(1,0); d16.select<4,1>(4) = d4x4(1,1); d16.select<4,1>(8) = d4x4(1,2); d16.select<4,1>(12) = d4x4(1,3);

        op1 = ( a16 * (four_minus_modX) + b16 * (modX) + 2) / 4;
        op2 = ( c16 * (four_minus_modX) + d16 * (modX) + 2) / 4;
#pragma unroll
        for(int i = 0; i < 4; i++){
            modY = i;
            four_minus_modY = (4 - i);
            vector<ushort, 16> output_row;
            output_row  = (four_minus_modY) * (op1);
            output_row += (modY) * (op2) + 2;
            output_row /= 4;
            output_Matrix.row(i+4) = output_row;
        }
        //Third 4 times ===============================================================================================================
        a16.select<4,1>(0) = a4x4(2,0); a16.select<4,1>(4) = a4x4(2,1); a16.select<4,1>(8) = a4x4(2,2); a16.select<4,1>(12) = a4x4(2,3);
        b16.select<4,1>(0) = b4x4(2,0); b16.select<4,1>(4) = b4x4(2,1); b16.select<4,1>(8) = b4x4(2,2); b16.select<4,1>(12) = b4x4(2,3);
        c16.select<4,1>(0) = c4x4(2,0); c16.select<4,1>(4) = c4x4(2,1); c16.select<4,1>(8) = c4x4(2,2); c16.select<4,1>(12) = c4x4(2,3);
        d16.select<4,1>(0) = d4x4(2,0); d16.select<4,1>(4) = d4x4(2,1); d16.select<4,1>(8) = d4x4(2,2); d16.select<4,1>(12) = d4x4(2,3);

        op1 = ( a16 * (four_minus_modX) + b16 * (modX) + 2) / 4;
        op2 = ( c16 * (four_minus_modX) + d16 * (modX) + 2) / 4;
#pragma unroll
        for(int i = 0; i < 4; i++){
            modY = i;
            four_minus_modY = (4 - i);
            vector<ushort, 16> output_row;
            output_row  = (four_minus_modY) * (op1);
            output_row += (modY) * (op2) + 2;
            output_row /= 4;
            output_Matrix.row(i+8) = output_row;
        }
        //Fourth 4 times ===============================================================================================================
        a16.select<4,1>(0) = a4x4(3,0); a16.select<4,1>(4) = a4x4(3,1); a16.select<4,1>(8) = a4x4(3,2); a16.select<4,1>(12) = a4x4(3,3);
        b16.select<4,1>(0) = b4x4(3,0); b16.select<4,1>(4) = b4x4(3,1); b16.select<4,1>(8) = b4x4(3,2); b16.select<4,1>(12) = b4x4(3,3);
        c16.select<4,1>(0) = c4x4(3,0); c16.select<4,1>(4) = c4x4(3,1); c16.select<4,1>(8) = c4x4(3,2); c16.select<4,1>(12) = c4x4(3,3);
        d16.select<4,1>(0) = d4x4(3,0); d16.select<4,1>(4) = d4x4(3,1); d16.select<4,1>(8) = d4x4(3,2); d16.select<4,1>(12) = d4x4(3,3);

        op1 = ( a16 * (four_minus_modX) + b16 * (modX) + 2) / 4;
        //op1 += cm_mul<ushort>(a16, four_minus_modX);
        //op1 += cm_mul<ushort>(b16, modX);
        //op1 += 2;
        //op1 /= 4;
        op2 = ( c16 * (four_minus_modX) + d16 * (modX) + 2) / 4;
#pragma unroll
        for(int i = 0; i < 4; i++){
            modY = i;
            four_minus_modY = (4 - i);
            vector<ushort, 16> output_row;
            output_row  = (four_minus_modY) * (op1);
            output_row += (modY) * (op2) + 2;
            output_row /= 4;
            output_Matrix.row(i+12) = output_row;
        }
#endif

        //after the loops are done, the 16x16 output matrix is ready to be written to the output surface
#if 0
        write(UpSampleMaskIndex, h_pos, rowpos, output_Matrix);
#else
        write(UpSampleMaskIndex, h_pos, rowpos, output_Matrix.select<8, 1, 16, 1>(0,0));
        write(UpSampleMaskIndex, h_pos, rowpos + 8, output_Matrix.select<8, 1, 16, 1>(8,0));
#endif
     }

}

extern "C" _GENX_MAIN_ void
VCorrection_buffer(SurfaceIndex RIndex,
                   SurfaceIndex GIndex,
                   SurfaceIndex BIndex,
                   SurfaceIndex VMaskIndex,
                   SurfaceIndex OutputRIndex,
                   SurfaceIndex OutputGIndex,
                   SurfaceIndex OutputBIndex,
                   int correction_format,
                   unsigned int h_pos,
                   unsigned int v_pos)
{
    matrix<ushort, 8, 16> inR; matrix<ushort, 8, 16> outR;
    matrix<ushort, 8, 16> inG; matrix<ushort, 8, 16> outG;
    matrix<ushort, 8, 16> inB; matrix<ushort, 8, 16> outB;
    matrix<ushort, 8, 16> Mask;

    read(RIndex, h_pos, v_pos, inR);
    read(GIndex, h_pos, v_pos, inG);
    read(BIndex, h_pos, v_pos, inB);
    read(VMaskIndex, h_pos, v_pos, Mask);

    outR = cm_mul<short>(inR, Mask);
    outG = cm_mul<short>(inG, Mask);
    outB = cm_mul<short>(inB, Mask);

    if (correction_format == 0){ // Vignette correction extreme, U12.4 (integer 12-bits, fractional 4-bits)
        //shift according to U12.4
        //tmpR = CLIP_VAL((tmpR + 8) / 16, 0, 4095);//pixel_outR >>= 4;  // ADDED by Shih-Hsuan Hsu
        outR += 8;
        outR /= 16;

        outG += 8;
        outG /= 16;

        outB += 8;
        outB /= 16;
    }
    else{
        outR += 128;
        outR /= 256;

        outG += 128;
        outG /= 256;

        outB += 128;
        outB /= 256;
    }
    outR.merge(0, outR < 0);
    outR.merge(4095, outR > 4095);
    outG.merge(0, outG < 0);
    outG.merge(4095, outG > 4095);
    outB.merge(0, outB < 0);
    outB.merge(4095, outB > 4095);

    write(OutputRIndex, h_pos, v_pos, outR);
    write(OutputGIndex, h_pos, v_pos, outG);
    write(OutputBIndex, h_pos, v_pos, outB);
}

#define BLOCKWIDTH_BYTE 24
#define BLOCKWIDTH_PIXEL 8   //BLOCKWIDTH_PIXEL = BLOCKWIDTH_BYTE * 3
#define BLOCKHEIGHT_BYTE 8

//const unsigned short idx1[16] = {0, 8, 16, 1, 9, 17, 2, 10, 18, 3, 11, 19, 4, 12, 20, 5};
//const unsigned short idx2[8] = {13, 21, 6, 14, 22, 7, 15, 23};
const short idx3[24] = {0, 8, 16, 1, 9, 17, 2, 10, 18, 3, 11, 19, 4, 12, 20, 5, 13, 21, 6, 14, 22, 7, 15, 23};

//const unsigned short idx1[8] = {0, 0, 0, 1, 1, 1, 2, 2};
//const unsigned short idx2[8] = {2, 3, 3, 3, 4, 4, 4, 5};
//const unsigned short idx3[8] = {5, 5, 6, 6, 6, 7, 7, 7};

extern "C" _GENX_MAIN_ void
VCorrection_bmp(SurfaceIndex ibuf,
                SurfaceIndex obuf,
                SurfaceIndex MaskIndex,
                unsigned int Correction,
                unsigned int h_pos,
                unsigned int v_pos)
{
    matrix<ushort, BLOCKHEIGHT_BYTE, BLOCKWIDTH_PIXEL> in;
    matrix<ushort, BLOCKHEIGHT_BYTE, BLOCKWIDTH_PIXEL> mask;
    matrix<ushort, BLOCKHEIGHT_BYTE, BLOCKWIDTH_PIXEL> out;
    //matrix<ushort, BLOCKHEIGHT_BYTE, BLOCKWIDTH_BYTE> tmpmask;
    matrix<ushort, BLOCKHEIGHT_BYTE, BLOCKWIDTH_PIXEL> tmp;

    int rd_v_pos = v_pos * BLOCKHEIGHT_BYTE;

    // read input matrix and convert to short
    read(ibuf, h_pos*BLOCKWIDTH_PIXEL*sizeof(short), rd_v_pos, in);
    //matrix<ushort, BLOCKHEIGHT_BYTE, BLOCKWIDTH_BYTE> inM = in;

    // read mask matrix and convert to short
    read(MaskIndex, h_pos*BLOCKWIDTH_PIXEL*sizeof(short), rd_v_pos, mask);
    //matrix<ushort, BLOCKHEIGHT_BYTE, BLOCKWIDTH_BYTE> Mask;

    /*
    vector<ushort, BLOCKWIDTH_BYTE> v;
    vector<ushort, BLOCKWIDTH_PIXEL> w;
    vector<ushort, 24> vidx3(idx3);

    Mask.row(0) = mask.row(0).replicate<3>().iselect(vidx3);
    Mask.row(1) = mask.row(1).replicate<3>().iselect(vidx3);
    Mask.row(2) = mask.row(2).replicate<3>().iselect(vidx3);
    Mask.row(3) = mask.row(3).replicate<3>().iselect(vidx3);
    Mask.row(4) = mask.row(4).replicate<3>().iselect(vidx3);
    Mask.row(5) = mask.row(5).replicate<3>().iselect(vidx3);
    Mask.row(6) = mask.row(6).replicate<3>().iselect(vidx3);
    Mask.row(7) = mask.row(7).replicate<3>().iselect(vidx3);
    //*/

#if 0

    vector_ref<ushort, 24> vec = v.iselect(vidx3);
    vector_ref<ushort, 24> row0 = Mask.row(0);
    vector_ref<ushort, 24> row1 = Mask.row(1);
    vector_ref<ushort, 24> row2 = Mask.row(2);
    vector_ref<ushort, 24> row3 = Mask.row(3);
    vector_ref<ushort, 24> row4 = Mask.row(4);
    vector_ref<ushort, 24> row5 = Mask.row(5);
    vector_ref<ushort, 24> row6 = Mask.row(6);
    vector_ref<ushort, 24> row7 = Mask.row(7);

    w = MaskMatrix.row(0);
    v = w.replicate<3>();

    row0 = vec;

    w = MaskMatrix.row(1);
    v = w.replicate<3>();

    row1 = vec;

    w = MaskMatrix.row(2);
    v = w.replicate<3>();

    row2 = vec;

    w = MaskMatrix.row(3);
    v = w.replicate<3>();

    vector<ushort, 24> r3 = vec;
    Mask.row(3) = r3;

    w = MaskMatrix.row(4);
    v = w.replicate<3>();

    row4 = vec;

    w = MaskMatrix.row(5);
    v = w.replicate<3>();

    row5 = vec;

    w = MaskMatrix.row(6);
    v = w.replicate<3>();

    row6 = vec;

    w = MaskMatrix.row(7);
    v = w.replicate<3>();

    row7 = vec;
#endif


    tmp = cm_mul<short>(in, mask);

    if (Correction == 0) {    // Extreme Correction is 0 so U12.4
        tmp += 8;
        out = tmp >> 4;
        out.merge(255, out > 255);
        out.merge(0, out < 0);
    }
    else {    // Correction is 1 so U8.8
        tmp += 128;
        out = tmp >> 8;
        out.merge(4095, out > 4095);
        out.merge(0, out < 0);
    }

    write(obuf, h_pos*BLOCKWIDTH_PIXEL*sizeof(short), rd_v_pos, out);
}


extern "C" _GENX_MAIN_ void
Bayer_MaskReduce(SurfaceIndex maskbuf,
                SurfaceIndex MaskIndexR,
                SurfaceIndex MaskIndexG1,
                SurfaceIndex MaskIndexG2,
                SurfaceIndex MaskIndexB,
                unsigned int BayerPattern,
                unsigned int Correction,
                unsigned int h_pos,
                unsigned int v_pos)
{
    matrix<ushort, BLOCKHEIGHT_BYTE, BLOCKWIDTH_PIXEL> maskR;
    matrix<ushort, BLOCKHEIGHT_BYTE, BLOCKWIDTH_PIXEL> maskG1;
    matrix<ushort, BLOCKHEIGHT_BYTE, BLOCKWIDTH_PIXEL> maskG2;
    matrix<ushort, BLOCKHEIGHT_BYTE, BLOCKWIDTH_PIXEL> maskB;

    int rd_v_pos = v_pos * BLOCKHEIGHT_BYTE;
    int rd_h_pos = h_pos * BLOCKWIDTH_PIXEL * sizeof(short);

    // read mask matrix and convert to short
    read(MaskIndexR, rd_h_pos, rd_v_pos, maskR);
    read(MaskIndexG1, rd_h_pos, rd_v_pos, maskG1);
    read(MaskIndexG2, rd_h_pos, rd_v_pos, maskG2);
    read(MaskIndexB, rd_h_pos, rd_v_pos, maskB);


    matrix<ushort, BLOCKHEIGHT_BYTE, BLOCKWIDTH_PIXEL> Mask;

    if (BayerPattern == rggb) {
        Mask.select<4,2,4,2>(0,0) = maskR.select<4,2,4,2>(0,0);
        Mask.select<4,2,4,2>(0,1) = maskG1.select<4,2,4,2>(0,1);
        Mask.select<4,2,4,2>(1,0) = maskG2.select<4,2,4,2>(1,0);
        Mask.select<4,2,4,2>(1,1) = maskB.select<4,2,4,2>(1,1);
    }
    else if (BayerPattern == bggr) {
        Mask.select<4,2,4,2>(0,0) = maskB.select<4,2,4,2>(0,0);
        Mask.select<4,2,4,2>(0,1) = maskG1.select<4,2,4,2>(0,1);
        Mask.select<4,2,4,2>(1,0) = maskG2.select<4,2,4,2>(1,0);
        Mask.select<4,2,4,2>(1,1) = maskR.select<4,2,4,2>(1,1);
    }
    else if (BayerPattern == grbg) {
        Mask.select<4,2,4,2>(0,0) = maskG1.select<4,2,4,2>(0,0);
        Mask.select<4,2,4,2>(0,1) = maskR.select<4,2,4,2>(0,1);
        Mask.select<4,2,4,2>(1,0) = maskB.select<4,2,4,2>(1,0);
        Mask.select<4,2,4,2>(1,1) = maskG2.select<4,2,4,2>(1,1);
    }
    else if (BayerPattern == gbrg) {
        Mask.select<4,2,4,2>(0,0) = maskG1.select<4,2,4,2>(0,0);
        Mask.select<4,2,4,2>(0,1) = maskB.select<4,2,4,2>(0,1);
        Mask.select<4,2,4,2>(1,0) = maskR.select<4,2,4,2>(1,0);
        Mask.select<4,2,4,2>(1,1) = maskG2.select<4,2,4,2>(1,1);
    }
    //*/

    write(maskbuf, rd_h_pos, rd_v_pos, Mask);
}

//  End of Vignette Correction section
////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////
// SKL Auto White Balance GPU implementation
// Unit test :
/////////////////////////////////////////////////////////////////////

//#include "cm/cm.h"
const unsigned int offset_init[8] = {0,1,2,3,4,5,6,7};

#define BLOCK_WIDTH 8
#define BLOCK_HEIGHT 8

#define HIST_SIZE 256
#define MAX_HIST_VAL HIST_SIZE-1
#define BitPrecision 8
#define shift_to_8bit BitPrecision - 8

// Calculates Histograms RGBY for Bayer Surface BGGR
extern "C" _GENX_MAIN_ void CalcHistoBGGR(SurfaceIndex BayerIndex,
                                     int h_pos,
                                     int v_pos,
                                     SurfaceIndex HistRIndex,
                                     SurfaceIndex HistGIndex,
                                     SurfaceIndex HistBIndex,
                                     SurfaceIndex HistYIndex,
                                     unsigned int max_pixels_in_bin)
{
    matrix<ushort, BLOCK_HEIGHT, BLOCK_WIDTH> BayerMatrix;
    uint h_loadstart = h_pos*BLOCK_WIDTH*2;
    uint v_loadstart = v_pos*BLOCK_HEIGHT;
    read(BayerIndex, h_loadstart, v_loadstart, BayerMatrix);

    matrix<ushort, BLOCK_HEIGHT/2, BLOCK_WIDTH/2> colorM;

    vector<int, HIST_SIZE> histo (0);
    matrix<ushort, 2, 2> bayerpatch;
    vector<uint, 8> src;
    vector<uint, 8> ret;
#if 0
    vector<uint, 8> offset(offset_init);
#else
    vector<uint, 8> offset;
#pragma unroll
    for(int i = 0; i < 8; i ++)
        offset(i) = i;
#endif

    // blue
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(0,0);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistBIndex, ATOMIC_ADD, i, offset, src, ret);
    }

    // green 1
    histo = 0;
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(1,0);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }
    // green 2
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(0,1);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistGIndex, ATOMIC_ADD, i, offset, src, ret);
    }

    // red
    histo = 0;
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(1,1);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistRIndex, ATOMIC_ADD, i, offset, src, ret);
    }

    // Y
    histo = 0;
    for (int y = 0; y < BLOCK_HEIGHT; y += 2) {
        for (int x = 0; x < BLOCK_WIDTH; x += 2) {
            bayerpatch = BayerMatrix.select<2,1,2,1>(y,x);
            int valB = bayerpatch(0,0);
            int valG1 = bayerpatch(0,1);
            int valG2 = bayerpatch(1,0);
            int valR = bayerpatch(1,1);
            int valY = (((valR*306)>>10)+(((valG1+valG2)/2*601)>>10)+((valB*117)>>10));
            if (histo[valY] < max_pixels_in_bin) {
                histo(valY) += 1;
            }
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistYIndex, ATOMIC_ADD, i, offset, src, ret);
    }
}

// Calculates Histograms RGBY for Bayer Surface RGGB
extern "C" _GENX_MAIN_ void CalcHistoRGGB(SurfaceIndex BayerIndex,
                                     int h_pos,
                                     int v_pos,
                                     SurfaceIndex HistRIndex,
                                     SurfaceIndex HistGIndex,
                                     SurfaceIndex HistBIndex,
                                     SurfaceIndex HistYIndex,
                                     unsigned int max_pixels_in_bin)
{
    matrix<ushort, BLOCK_HEIGHT, BLOCK_WIDTH> BayerMatrix;
    uint h_loadstart = h_pos*BLOCK_WIDTH*2;
    uint v_loadstart = v_pos*BLOCK_HEIGHT;
    read(BayerIndex, h_loadstart, v_loadstart, BayerMatrix);

    matrix<ushort, BLOCK_HEIGHT/2, BLOCK_WIDTH/2> colorM;

    vector<int, HIST_SIZE> histo (0);
    matrix<ushort, 2, 2> bayerpatch;
    vector<uint, 8> src;
    vector<uint, 8> ret;
#if 0
    vector<uint, 8> offset(offset_init);
#else
    vector<uint, 8> offset;
#pragma unroll
    for(int i = 0; i < 8; i ++)
        offset(i) = i;
#endif
    // red
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(0,0);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistRIndex, ATOMIC_ADD, i, offset, src, ret);
    }

    // green 1
    histo = 0;
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(1,0);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }
    // green 2
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(0,1);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistGIndex, ATOMIC_ADD, i, offset, src, ret);
    }

    // blue
    histo = 0;
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(1,1);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistBIndex, ATOMIC_ADD, i, offset, src, ret);
    }

    // Y
    histo = 0;
    for (int y = 0; y < BLOCK_HEIGHT; y += 2) {
        for (int x = 0; x < BLOCK_WIDTH; x += 2) {
            bayerpatch = BayerMatrix.select<2,1,2,1>(y,x);
            int valB = bayerpatch(1,1);
            int valG1 = bayerpatch(0,1);
            int valG2 = bayerpatch(1,0);
            int valR = bayerpatch(0,0);
            int valY = (((valR*306)>>10)+(((valG1+valG2)/2*601)>>10)+((valB*117)>>10));
            if (histo(valY) < max_pixels_in_bin) {
                histo(valY) += 1;
            }
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistYIndex, ATOMIC_ADD, i, offset, src, ret);
    }
}

// Calculates Histograms RGBY for Bayer Surface GBRG
extern "C" _GENX_MAIN_ void CalcHistoGBRG(SurfaceIndex BayerIndex,
                                     int h_pos,
                                     int v_pos,
                                     SurfaceIndex HistRIndex,
                                     SurfaceIndex HistGIndex,
                                     SurfaceIndex HistBIndex,
                                     SurfaceIndex HistYIndex,
                                     unsigned int max_pixels_in_bin)
{
    matrix<ushort, BLOCK_HEIGHT, BLOCK_WIDTH> BayerMatrix;
    uint h_loadstart = h_pos*BLOCK_WIDTH*2;
    uint v_loadstart = v_pos*BLOCK_HEIGHT;
    read(BayerIndex, h_loadstart, v_loadstart, BayerMatrix);

    matrix<ushort, BLOCK_HEIGHT/2, BLOCK_WIDTH/2> colorM;

    vector<int, HIST_SIZE> histo (0);
    matrix<ushort, 2, 2> bayerpatch;
    vector<uint, 8> src;
    vector<uint, 8> ret;
#if 0
    vector<uint, 8> offset(offset_init);
#else
    vector<uint, 8> offset;
#pragma unroll
    for(int i = 0; i < 8; i ++)
        offset(i) = i;
#endif

    // green1
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(0,0);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }

    // green2
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(1,1);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistGIndex, ATOMIC_ADD, i, offset, src, ret);
    }

    // blue
    histo = 0;
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(0,1);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistBIndex, ATOMIC_ADD, i, offset, src, ret);
    }
    // red
    histo = 0;
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(1,0);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin) {
                histo(colorM(i,j)) += 1;
            }
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistRIndex, ATOMIC_ADD, i, offset, src, ret);
    }

    // Y
    histo = 0;
    for (int y = 0; y < BLOCK_HEIGHT; y += 2) {
        for (int x = 0; x < BLOCK_WIDTH; x += 2) {
            bayerpatch = BayerMatrix.select<2,1,2,1>(y,x);
            int valB = bayerpatch(0,1);
            int valG1 = bayerpatch(0,0);
            int valG2 = bayerpatch(1,1);
            int valR = bayerpatch(1,0);
            int valY = (((valR*306)>>10)+(((valG1+valG2)/2*601)>>10)+((valB*117)>>10));
            if (histo(valY) < max_pixels_in_bin) {
                histo(valY) += 1;
            }
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistYIndex, ATOMIC_ADD, i, offset, src, ret);
    }
}

// Calculates Histograms RGBY for Bayer Surface GRBG
extern "C" _GENX_MAIN_ void CalcHistoGRBG(SurfaceIndex BayerIndex,
                                     int h_pos,
                                     int v_pos,
                                     SurfaceIndex HistRIndex,
                                     SurfaceIndex HistGIndex,
                                     SurfaceIndex HistBIndex,
                                     SurfaceIndex HistYIndex,
                                     unsigned int max_pixels_in_bin)
{
    matrix<ushort, BLOCK_HEIGHT, BLOCK_WIDTH> BayerMatrix;
    uint h_loadstart = h_pos*BLOCK_WIDTH*2;
    uint v_loadstart = v_pos*BLOCK_HEIGHT;
    read(BayerIndex, h_loadstart, v_loadstart, BayerMatrix);

    matrix<ushort, BLOCK_HEIGHT/2, BLOCK_WIDTH/2> colorM;

    vector<int, HIST_SIZE> histo (0);
    matrix<ushort, 2, 2> bayerpatch;
    vector<uint, 8> src;
    vector<uint, 8> ret;
#if 0
    vector<uint, 8> offset(offset_init);
#else
    vector<uint, 8> offset;
#pragma unroll
    for(int i = 0; i < 8; i ++)
        offset(i) = i;
#endif

    // green1
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(0,0);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }

    // green2
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(1,1);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistGIndex, ATOMIC_ADD, i, offset, src, ret);
    }

    // red
    histo = 0;
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(0,1);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistRIndex, ATOMIC_ADD, i, offset, src, ret);
    }
    // blue
    histo = 0;
    colorM = BayerMatrix.select<BLOCK_HEIGHT/2,2,BLOCK_WIDTH/2,2>(1,0);
    colorM.merge(0, colorM < 0);
    colorM.merge(MAX_HIST_VAL, colorM > MAX_HIST_VAL);
    for (int i = 0; i < BLOCK_HEIGHT/2; i++) {
        for (int j = 0; j < BLOCK_WIDTH/2; j++) {
            if (histo(colorM(i,j)) < max_pixels_in_bin)
                histo(colorM(i,j)) += 1;
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistBIndex, ATOMIC_ADD, i, offset, src, ret);
    }

    // Y
    histo = 0;
    for (int y = 0; y < BLOCK_HEIGHT; y += 2) {
        for (int x = 0; x < BLOCK_WIDTH; x += 2) {
            bayerpatch = BayerMatrix.select<2,1,2,1>(y,x);
            int valR = bayerpatch(0,1);
            int valG1 = bayerpatch(0,0);
            int valG2 = bayerpatch(1,1);
            int valB = bayerpatch(1,0);
            int valY = (((valR*306)>>10)+(((valG1+valG2)/2*601)>>10)+((valB*117)>>10));
            if (histo(valY) < max_pixels_in_bin) {
                histo(valY) += 1;
            }
        }
    }
    #pragma unroll
    for(int i = 0; i < HIST_SIZE; i += 8) {
        src =  histo.select<8,1>(i);
        write<uint, 8>(HistYIndex, ATOMIC_ADD, i, offset, src, ret);
    }
}

// Calculates Number of Bright & Gray Points and Sums (requires Bayer Format parameter)
extern "C" _GENX_MAIN_ void CountBrightGrayPts(SurfaceIndex BayerIndex,
                                      int h_pos,
                                      int v_pos,
                                      int YBright,
                                      int YOutlier,
                                      int UVThresh,
                                      SurfaceIndex RGBUVBIndex,
                                      int BayerFormat)
                                      //SurfaceIndex OutputIndex)
{
    matrix<ushort, BLOCK_HEIGHT, BLOCK_WIDTH> BayerMatrix;
    vector<uint, 16>  RGBUVB (0);

    uint h_pos1 = get_thread_origin_x();
    uint v_pos1 = get_thread_origin_y();

    uint h_loadstart = h_pos*BLOCK_WIDTH*2;
    uint v_loadstart = v_pos*BLOCK_HEIGHT;
    read(BayerIndex, h_loadstart, v_loadstart, BayerMatrix);
    //write(OutputIndex, h_loadstart, v_loadstart, BayerMatrix);

    matrix<ushort, 2, 2> bayerpatch;

#pragma unroll
    for (int y = 0; y < BLOCK_HEIGHT; y += 2) {
#pragma unroll
        for (int x = 0; x < BLOCK_WIDTH; x += 2) {
            bayerpatch = BayerMatrix.select<2,1,2,1>(y,x);
            int valB, valG1, valG2, valR;
            if (BayerFormat == bggr) {
                valB = bayerpatch(0,0);
                valG1 = bayerpatch(0,1);
                valG2 = bayerpatch(1,0);
                valR = bayerpatch(1,1);
            } else if (BayerFormat == rggb) {
                valR = bayerpatch(0,0);
                valG1 = bayerpatch(0,1);
                valG2 = bayerpatch(1,0);
                valB = bayerpatch(1,1);
            } else if (BayerFormat == gbrg) {
                valG1 = bayerpatch(0,0);
                valB = bayerpatch(0,1);
                valR = bayerpatch(1,0);
                valG2 = bayerpatch(1,1);
            } else if (BayerFormat == grbg) {
                valG1 = bayerpatch(0,0);
                valR = bayerpatch(0,1);
                valB = bayerpatch(1,0);
                valG2 = bayerpatch(1,1);
            }
            int valG = (valG1 + valG2) >> 1;
            int valY = (((valR*306)>>10) + ((valG*601)>>10) + ((valB*117)>>10));
            int valU = ((valR*-151)>>10) + ((valG*-296)>>10) + ((valB*446)>>10);
            int valV = ((valR*630)>>10) + ((valG*-527)>>10) + ((valB*-102)>>10);

            // Is it a bright point?
            //if ((valY >= (YBright<<8)) && (valY < (YOutlier<<8))) {
            if ((valY >= YBright) && (valY < YOutlier)) {
                RGBUVB(0) += 1;        // numBrightPts
                RGBUVB(1) += valR;    // sumBrightR
                RGBUVB(2) += valG;    // sumBrightG
                RGBUVB(3) += valB;    // sumBrightB
            }
            // Is it a gray point?
            if ( ((cm_abs<int>(valU)>>shift_to_8bit) + (cm_abs<int>(valV)>>shift_to_8bit)) < (UVThresh * (valY>>shift_to_8bit))>>8 ) {
                RGBUVB(4) += 1;        // numGrayPts
                RGBUVB(5) += valR;    // sumGrayR
                RGBUVB(6) += valG;    // sumGrayG
                RGBUVB(7) += valB;    // sumGrayB
            }
        }
    }
    vector<uint, 8> src;
    vector<uint, 8> ret;
#if 0
    vector<uint, 8> offset(offset_init);
#else
    vector<uint, 8> offset;
#pragma unroll
    for(int i = 0; i < 8; i ++)
        offset(i) = i;
#endif
    src = RGBUVB.select<8,1>(0);
    write<uint, 8>(RGBUVBIndex, ATOMIC_ADD, 0, offset, src, ret);
    src = RGBUVB.select<8,1>(8);
    write<uint, 8>(RGBUVBIndex, ATOMIC_ADD, 8, offset, src, ret);
}



// Applies float coefficients to scale an input image (requires Bayer Format parameter)
extern "C" _GENX_MAIN_ void ApplyScaling(SurfaceIndex BayerIndex,
                                      int h_pos,
                                      int v_pos,
                                      float Rscale,
                                      float Gtop_scale,
                                      float Gbot_scale,
                                      float Bscale,
                                      int BayerFormat,
                                      SurfaceIndex OutputIndex) {
    matrix<ushort, BLOCK_HEIGHT, BLOCK_WIDTH> BayerMatrix;
    matrix<ushort, BLOCK_HEIGHT, BLOCK_WIDTH> OutMatrix;

    uint h_loadstart = h_pos*BLOCK_WIDTH*2;
    uint v_loadstart = v_pos*BLOCK_HEIGHT;

    read(BayerIndex, h_loadstart, v_loadstart, BayerMatrix);

    if (BayerFormat == bggr) {
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,0) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,0), Bscale, SAT);
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,1) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,1), Gtop_scale, SAT);
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,0) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,0), Gbot_scale, SAT);
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,1) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,1), Rscale, SAT);
    } else if (BayerFormat == rggb) {
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,0) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,0), Rscale, SAT);
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,1) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,1), Gtop_scale, SAT);
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,0) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,0), Gbot_scale, SAT);
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,1) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,1), Bscale, SAT);
    } else if (BayerFormat == gbrg) {
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,0) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,0), Gtop_scale, SAT);
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,1) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,1), Bscale, SAT);
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,0) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,0), Rscale, SAT);
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,1) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,1), Gbot_scale, SAT);
    } else { // grbg
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,0) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,0), Gtop_scale, SAT);
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,1) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(0,1), Rscale, SAT);
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,0) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,0), Bscale, SAT);
        OutMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,1) = cm_mul<ushort>(BayerMatrix.select<BLOCK_HEIGHT/2, 2, BLOCK_WIDTH/2, 2>(1,1), Gbot_scale, SAT);
    }

    OutMatrix.merge(0, OutMatrix < 0);
    OutMatrix.merge(255, OutMatrix > 255);
    //OutMatrix.merge(65535, OutMatrix > 65535);
    write(OutputIndex, h_loadstart, v_loadstart, OutMatrix);
}

//  End of Auto-White Balance section
////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////
// SKL Manual White Balance GPU implementation
// Unit test :
/////////////////////////////////////////////////////////////////////

// tentative Manual White Balance by Iwamoto
// need to be optimized and validated
extern "C" _GENX_MAIN_ void
GPU_BAYER_GAIN( SurfaceIndex ibuf, SurfaceIndex obuf,
                float R, float G1, float B, float G2, int max_input_level)
{

    matrix<ushort, 16, 16> bayer_in;
    matrix<ushort, 16, 16> bayer_out;
    matrix<int, 16, 16> bayer_tmp;
    matrix<float, 16, 16> gain;

    int rd_v_pos = get_thread_origin_y() * 16;
    int rd_h_pos = get_thread_origin_x() * 16 * sizeof(short);

    read(ibuf, rd_h_pos, rd_v_pos  , bayer_in.select<8,1,16,1>(0,0));
    read(ibuf, rd_h_pos, rd_v_pos+8, bayer_in.select<8,1,16,1>(8,0));

    gain.select<8,2,8,2>(0,0) = R;
    gain.select<8,2,8,2>(1,1) = B;
    gain.select<8,2,8,2>(0,1) = G1;
    gain.select<8,2,8,2>(1,0) = G2;

    //bayer_out = bayer_in * gain;
    bayer_tmp = bayer_in * gain;
    //bayer_out = matrix<ushort, 16,16>(bayer_tmp);

    bayer_tmp = cm_max<int>(bayer_tmp, 0);
    bayer_tmp = cm_min<int>(bayer_tmp, max_input_level);
    bayer_out = matrix<ushort,16,16>(bayer_tmp);
    write(obuf, rd_h_pos, rd_v_pos  , bayer_out.select<8,1,16,1>(0,0));
    write(obuf, rd_h_pos, rd_v_pos+8, bayer_out.select<8,1,16,1>(8,0));

}
//  End of Manual White Balance section
////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////
// SKL CCM(Color Conversion Matrix) GPU implementation
// Unit test :
/////////////////////////////////////////////////////////////////////

// tentative 3x3 CCM by Iwamoto
// need to be optimized and validated
extern "C" _GENX_MAIN_ void
GPU_3x3CCM_SKL( SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
                SurfaceIndex R_O_Index, SurfaceIndex G_O_Index, SurfaceIndex B_O_Index,
                SurfaceIndex CCM_Index, int max_input_level)
{

    int rd_v_pos = get_thread_origin_y() * 16;
    int rd_h_pos = get_thread_origin_x() * 16 * sizeof(short);

    vector<float,9> ccm_matrix;
    read(CCM_Index, 0, ccm_matrix);


    matrix<int, 8, 8> out;

    matrix<ushort, 8, 8> r_in;
    matrix<ushort, 8, 8> g_in;
    matrix<ushort, 8, 8> b_in;

    matrix<float,8,8> ccm_matrix_c0 = ccm_matrix(0);
    matrix<float,8,8> ccm_matrix_c1 = ccm_matrix(1);
    matrix<float,8,8> ccm_matrix_c2 = ccm_matrix(2);
    matrix<float,8,8> ccm_matrix_c3 = ccm_matrix(3);
    matrix<float,8,8> ccm_matrix_c4 = ccm_matrix(4);
    matrix<float,8,8> ccm_matrix_c5 = ccm_matrix(5);
    matrix<float,8,8> ccm_matrix_c6 = ccm_matrix(6);
    matrix<float,8,8> ccm_matrix_c7 = ccm_matrix(7);
    matrix<float,8,8> ccm_matrix_c8 = ccm_matrix(8);


// debug
/*
    matrix<float,8,8> ccm_matrix_c0 = 0.70130002f;
    matrix<float,8,8> ccm_matrix_c1 = -0.14080000f;
    matrix<float,8,8> ccm_matrix_c2 = -0.063500002f;
    matrix<float,8,8> ccm_matrix_c3 = -0.52679998f;
    matrix<float,8,8> ccm_matrix_c4 = 1.2902000f;
    matrix<float,8,8> ccm_matrix_c5 = 0.26400000f;
    matrix<float,8,8> ccm_matrix_c6 = -0.14700000f;
    matrix<float,8,8> ccm_matrix_c7 = 0.28009999f;
    matrix<float,8,8> ccm_matrix_c8 = 0.73790002f;
    max_input_level = 16383;
*/

    for (int j=0;j<16;j=j+8)
    {
    for (int i=0;i<16;i=i+8)
    {

        read(R_I_Index, rd_h_pos+i* sizeof(short), rd_v_pos+j  , r_in);
        read(G_I_Index, rd_h_pos+i* sizeof(short), rd_v_pos+j  , g_in);
        read(B_I_Index, rd_h_pos+i* sizeof(short), rd_v_pos+j  , b_in);

        out = r_in*ccm_matrix_c0 + g_in*ccm_matrix_c1 + b_in*ccm_matrix_c2;
        out = cm_max<int>(out, 0);
        out = cm_min<int>(out, max_input_level);
        write(R_O_Index, rd_h_pos+i* sizeof(short), rd_v_pos+j  , matrix<ushort, 8, 8>(out));

        out = r_in*ccm_matrix_c3 + g_in*ccm_matrix_c4 + b_in*ccm_matrix_c5;
        out = cm_max<int>(out, 0);
        out = cm_min<int>(out, max_input_level);
        write(G_O_Index, rd_h_pos+i* sizeof(short), rd_v_pos+j  , matrix<ushort, 8, 8>(out));

        out = r_in*ccm_matrix_c6 + g_in*ccm_matrix_c7 + b_in*ccm_matrix_c8;
        out = cm_max<int>(out, 0);
        out = cm_min<int>(out, max_input_level);
        write(B_O_Index, rd_h_pos+i* sizeof(short), rd_v_pos+j  , matrix<ushort, 8, 8>(out));
    }
    }

}

//  End of CCM section
////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////
// SKL Forward Gamma Correction GPU implementation
// Unit test :
/////////////////////////////////////////////////////////////////////

//////// Gamma Correction v5 with ARGB8 conversion

#define NUM_CONTROL_POINTS 64
const unsigned short initSeq[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

_GENX_ void inline
GAMMA_CORRECT_V5(SurfaceIndex BUF_IN,
        int rd_h_pos, int rd_v_pos,
        matrix_ref<uchar, 8, 32> out,
        int slmX,
        int bit_shift_amount)
{
    matrix<ushort, 8, 16> rd_in;

    read(BUF_IN, rd_h_pos, rd_v_pos, rd_in);

    vector<ushort, 16> v_Addr;
    vector<uint,   16> rd_slmDataIn;

    v_Addr = rd_in.row(0); cm_slm_read(slmX, v_Addr, rd_slmDataIn); rd_in.row(0) = rd_slmDataIn;
    v_Addr = rd_in.row(1); cm_slm_read(slmX, v_Addr, rd_slmDataIn); rd_in.row(1) = rd_slmDataIn;
    v_Addr = rd_in.row(2); cm_slm_read(slmX, v_Addr, rd_slmDataIn); rd_in.row(2) = rd_slmDataIn;
    v_Addr = rd_in.row(3); cm_slm_read(slmX, v_Addr, rd_slmDataIn); rd_in.row(3) = rd_slmDataIn;
    v_Addr = rd_in.row(4); cm_slm_read(slmX, v_Addr, rd_slmDataIn); rd_in.row(4) = rd_slmDataIn;
    v_Addr = rd_in.row(5); cm_slm_read(slmX, v_Addr, rd_slmDataIn); rd_in.row(5) = rd_slmDataIn;
    v_Addr = rd_in.row(6); cm_slm_read(slmX, v_Addr, rd_slmDataIn); rd_in.row(6) = rd_slmDataIn;
    v_Addr = rd_in.row(7); cm_slm_read(slmX, v_Addr, rd_slmDataIn); rd_in.row(7) = rd_slmDataIn;
#if 1
    rd_in.row(0) = rd_in.row(0) >> bit_shift_amount; // bit_shift_amount = (bitdepth - 8)
    rd_in.row(1) = rd_in.row(1) >> bit_shift_amount;
    rd_in.row(2) = rd_in.row(2) >> bit_shift_amount;
    rd_in.row(3) = rd_in.row(3) >> bit_shift_amount;
    rd_in.row(4) = rd_in.row(4) >> bit_shift_amount;
    rd_in.row(5) = rd_in.row(5) >> bit_shift_amount;
    rd_in.row(6) = rd_in.row(6) >> bit_shift_amount;
    rd_in.row(7) = rd_in.row(7) >> bit_shift_amount;
#else
    rd_in.merge(0, (rd_in > 1024));
#endif
    out = rd_in.format<uchar, 8, 32>();
}


extern "C" _GENX_MAIN_ void
GAMMA_GPUV5_ARGB8_LINEAR(SurfaceIndex Correct_Index, SurfaceIndex Point_Index,
          SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
          SurfaceIndex ARGB8_Index,
          //SurfaceIndex R_O_Index, SurfaceIndex G_O_Index, SurfaceIndex B_O_Index,
          int blocks_in_a_row, int BitDepth, int framewidth_in_bytes, int frameheight)
{
    ////////////////////////////////////////////////////////////////////////////////////////

    cm_slm_init(65536);
    uint slmX = cm_slm_alloc(65536);
#if 1
    matrix<ushort, 4, 16> Correct, Point;
    vector_ref<ushort, 64> gamma_P = Point.format<ushort, 1, 64>();

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
    int loop_count = load_elements_per_thread / 16;                              // Each loop do 16 elements

    vector<ushort, 16> v_Offsets(initSeq);
    v_Offsets += id * load_elements_per_thread;

    vector<ushort, 16> v_in;
    matrix<uint, 4, 16> v_out; //64 DW (64 element)

#ifndef CM_EMU
    for(int k = 0; k < loop_count; k++)
    {
        v_in = v_in_initial + (id * load_elements_per_thread + k * 16);
        index = 0;
#else
    if(id == 0)
    {
    for(int idx = 0; idx < cm_linear_local_size(); idx++)
    {
    for(int k = 0; k < loop_count; k++)
    {
        v_in = v_in_initial + (idx * load_elements_per_thread + k * 16);
        index = 0;
#endif


#if 0
#pragma unroll
        for(int i = 1; i < NUM_CONTROL_POINTS; i++)
        {
            if((gamma_P(i) <= v_in).any())
             {
                 index += (gamma_P(i) <= v_in);
             }
            else
                break;
        }
#else
        vector<ushort, 16> mak;
        vector<short , 16> adv;

        vector<ushort, 16> comp_index;

        comp_index = 32;
        mak    = (gamma_P(32) <= v_in);
        index  = (mak << 5);
        comp_index.merge((comp_index+16), (comp_index-16), mak);

        mak    = (gamma_P.iselect(comp_index) <= v_in);
        index += (mak << 4);
        comp_index.merge((comp_index+8 ), (comp_index-8 ), mak);

        mak    = (gamma_P.iselect(comp_index) <= v_in);
        index += (mak << 3);
        comp_index.merge((comp_index+4 ), (comp_index-4 ), mak);

        mak    = (gamma_P.iselect(comp_index) <= v_in);
        index += (mak << 2);
        comp_index.merge((comp_index+2 ), (comp_index-2 ), mak);

        mak    = (gamma_P.iselect(comp_index) <= v_in);
        index += (mak << 1);
        comp_index.merge((comp_index+1 ), (comp_index-1 ), mak);

        mak    = (gamma_P.iselect(comp_index) <= v_in);
        index += (mak     );
        index.merge(0, ((index == 1) & (v_in < gamma_P(1))));

#endif
        vector<ushort, 16> m_i, b_i, c_i, p_i, c_i_plus_1, p_i_plus_1;
        vector<uint, 16> data; //MSB W for m_i, LSB W for bi
        vector<uint, 16> i_plus_1;
        i_plus_1.merge((NUM_CONTROL_POINTS-1), (index + 1), (index == (NUM_CONTROL_POINTS-1)));

        c_i = Correct.iselect(index);
        p_i = Point  .iselect(index);

        c_i_plus_1.merge( max_input_level,  Correct.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));
        p_i_plus_1.merge( max_input_level,  Point  .iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));

#if 0
        int j = k % 4;
        v_out.row(j) =  (c_i_plus_1 - c_i);
        v_in = v_in_initial + (id * load_elements_per_thread + k * 16);
        v_out.row(j) *= (v_in - p_i);
        v_out.row(j) /= (p_i_plus_1 - p_i);
        v_out.row(j) += c_i;

        //v_out.row(j) = c_i;

        v_out.row(j) = cm_max<uint>(0, v_out.row(j));
        //v_out.row(j) = cm_min<uint>(16383, v_out.row(j));
        v_out.row(j) = cm_min<uint>((1 << BitDepth)-1, v_out.row(j));
        if(j == 3)
        {
            vector<uint, 64> rowTrans;
            vector<uint, 64> data_out =  v_out;
            rowTrans.select<8,1>(0)  = data_out.select<8,4>(0);
            rowTrans.select<8,1>(16) = data_out.select<8,4>(1);
            rowTrans.select<8,1>(32) = data_out.select<8,4>(2);
            rowTrans.select<8,1>(48) = data_out.select<8,4>(3);

            rowTrans.select<8,1>(8)  = data_out.select<8,4>(32);
            rowTrans.select<8,1>(24) = data_out.select<8,4>(33);
            rowTrans.select<8,1>(40) = data_out.select<8,4>(34);
            rowTrans.select<8,1>(56) = data_out.select<8,4>(35);

            //rowTrans = 100;
            vector<ushort, 16> v_Addr = v_Offsets << 2;
            cm_slm_write4 (slmX, v_Addr, rowTrans, SLM_ABGR_ENABLE);
            //cm_slm_write4 (slmX, v_Offsets, data_out, SLM_ABGR_ENABLE);
            v_Offsets += 64;  // in unit of elements
        }
#else
        vector<uint, 16> v_o;

        v_o =  (c_i_plus_1 - c_i);
        v_in = v_in_initial + (id * load_elements_per_thread + k * 16);
        v_o *= (v_in - p_i);
        v_o /= (p_i_plus_1 - p_i);
        v_o += c_i;

        v_o = cm_max<uint>(0, v_o);
        v_o = cm_min<uint>(max_input_level, v_o);
        cm_slm_write(slmX, v_in, v_o);
#endif

    }
#ifndef CM_EMU
    cm_barrier();
#else
        }
    }
#endif
#endif

    matrix<uchar, 8, 32 > color;
    matrix<uchar, 8, 128> out;
    int rd_h_pos, rd_v_pos;
    int wr_linear_offset;
    int bit_shift_amount = (BitDepth - 8);

    int l_count = (frameheight % 128 == 0)? (frameheight / 128) : ((frameheight / 128)+1);
    for(int l = 0; l < l_count; l++)   // 17 = (frameHeight / (32(H/gourp)*4groups))
    {
        for(int m = 0; m < 4; m++)
        {
            rd_h_pos = cm_linear_local_id()  * 128;                    //EACH thread in the threadgroup handle 128 byte-wide data
            rd_v_pos = cm_linear_group_id()  * 32 + m * 8 + l * 128;

            GAMMA_CORRECT_V5(R_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,0) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(G_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,1) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(B_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,2) = color.format<ushort, 8, 16>();

            ///////////////////////////////////////////////////////////////////////

            rd_h_pos = cm_linear_local_id() * 128 + 32;

            GAMMA_CORRECT_V5(R_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,64) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(G_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,65) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(B_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,66) = color.format<ushort, 8, 16>();

            wr_linear_offset = rd_v_pos * framewidth_in_bytes + cm_linear_local_id() * 256;

            write(ARGB8_Index, wr_linear_offset                            , out.row(0));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 1), out.row(1));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 2), out.row(2));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 3), out.row(3));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 4), out.row(4));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 5), out.row(5));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 6), out.row(6));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 7), out.row(7));

            ///////////////////////////////////////////////////////////////////////

            rd_h_pos = cm_linear_local_id() * 128 + 64;

            GAMMA_CORRECT_V5(R_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,0) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(G_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,1) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(B_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,2) = color.format<ushort, 8, 16>();

            ///////////////////////////////////////////////////////////////////////

            rd_h_pos = cm_linear_local_id() * 128 + 96;

            GAMMA_CORRECT_V5(R_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,64) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(G_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,65) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(B_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,66) = color.format<ushort, 8, 16>();

            wr_linear_offset = rd_v_pos * framewidth_in_bytes + cm_linear_local_id() * 256 + 128;

            write(ARGB8_Index, wr_linear_offset                            , out.row(0));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 1), out.row(1));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 2), out.row(2));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 3), out.row(3));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 4), out.row(4));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 5), out.row(5));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 6), out.row(6));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 7), out.row(7));
        }
    }


}

//  End of Forward Gamma Correction section
////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////
// SKL Forward Gamma Correction GPU implementation 2D out
// Unit test :
/////////////////////////////////////////////////////////////////////

extern "C" _GENX_MAIN_ void
GAMMA_GPUV4_ARGB8_2D(SurfaceIndex Correct_Index, SurfaceIndex Point_Index,
          SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
          SurfaceIndex ARGB8_Index,
          //SurfaceIndex R_O_Index, SurfaceIndex G_O_Index, SurfaceIndex B_O_Index,
          int blocks_in_a_row, int BitDepth, int framewidth_in_bytes, int frameheight)
{
    ////////////////////////////////////////////////////////////////////////////////////////

    cm_slm_init(65536);
    uint slmX = cm_slm_alloc(65536);
#if 1
    matrix<ushort, 4, 16> Correct, Point;
    vector_ref<ushort, 64> gamma_P = Point.format<ushort, 1, 64>();

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
    int loop_count = load_elements_per_thread / 16;                              // Each loop do 16 elements

    vector<ushort, 16> v_Offsets(initSeq);
    v_Offsets += id * load_elements_per_thread;

    vector<ushort, 16> v_in;
    matrix<uint, 4, 16> v_out; //64 DW (64 element)

#ifndef CM_EMU
    for(int k = 0; k < loop_count; k++)
    {
        v_in = v_in_initial + (id * load_elements_per_thread + k * 16);
        index = 0;
#else
    if(id == 0)
    {
    for(int idx = 0; idx < cm_linear_local_size(); idx++)
    {
    for(int k = 0; k < loop_count; k++)
    {
        v_in = v_in_initial + (idx * load_elements_per_thread + k * 16);
        index = 0;
#endif


#if 0
#pragma unroll
        for(int i = 1; i < NUM_CONTROL_POINTS; i++)
        {
            if((gamma_P(i) <= v_in).any())
             {
                 index += (gamma_P(i) <= v_in);
             }
            else
                break;
        }
#else
        vector<ushort, 16> mak;
        vector<short , 16> adv;

        vector<ushort, 16> comp_index;

        comp_index = 32;
        mak    = (gamma_P(32) <= v_in);
        index  = (mak << 5);
        comp_index.merge((comp_index+16), (comp_index-16), mak);

        mak    = (gamma_P.iselect(comp_index) <= v_in);
        index += (mak << 4);
        comp_index.merge((comp_index+8 ), (comp_index-8 ), mak);

        mak    = (gamma_P.iselect(comp_index) <= v_in);
        index += (mak << 3);
        comp_index.merge((comp_index+4 ), (comp_index-4 ), mak);

        mak    = (gamma_P.iselect(comp_index) <= v_in);
        index += (mak << 2);
        comp_index.merge((comp_index+2 ), (comp_index-2 ), mak);

        mak    = (gamma_P.iselect(comp_index) <= v_in);
        index += (mak << 1);
        comp_index.merge((comp_index+1 ), (comp_index-1 ), mak);

        mak    = (gamma_P.iselect(comp_index) <= v_in);
        index += (mak     );
        index.merge(0, ((index == 1) & (v_in < gamma_P(1))));

#endif
        vector<ushort, 16> m_i, b_i, c_i, p_i, c_i_plus_1, p_i_plus_1;
        vector<uint, 16> data; //MSB W for m_i, LSB W for bi
        vector<uint, 16> i_plus_1;
        i_plus_1.merge((NUM_CONTROL_POINTS-1), (index + 1), (index == (NUM_CONTROL_POINTS-1)));

        c_i = Correct.iselect(index);
        p_i = Point  .iselect(index);

        c_i_plus_1.merge( max_input_level,  Correct.iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));
        p_i_plus_1.merge( max_input_level,  Point  .iselect(i_plus_1), (index == (NUM_CONTROL_POINTS-1)));

#if 0
        int j = k % 4;
        v_out.row(j) =  (c_i_plus_1 - c_i);
        v_in = v_in_initial + (id * load_elements_per_thread + k * 16);
        v_out.row(j) *= (v_in - p_i);
        v_out.row(j) /= (p_i_plus_1 - p_i);
        v_out.row(j) += c_i;

        //v_out.row(j) = c_i;

        v_out.row(j) = cm_max<uint>(0, v_out.row(j));
        //v_out.row(j) = cm_min<uint>(16383, v_out.row(j));
        v_out.row(j) = cm_min<uint>((1 << BitDepth)-1, v_out.row(j));
        if(j == 3)
        {
            vector<uint, 64> rowTrans;
            vector<uint, 64> data_out =  v_out;
            rowTrans.select<8,1>(0)  = data_out.select<8,4>(0);
            rowTrans.select<8,1>(16) = data_out.select<8,4>(1);
            rowTrans.select<8,1>(32) = data_out.select<8,4>(2);
            rowTrans.select<8,1>(48) = data_out.select<8,4>(3);

            rowTrans.select<8,1>(8)  = data_out.select<8,4>(32);
            rowTrans.select<8,1>(24) = data_out.select<8,4>(33);
            rowTrans.select<8,1>(40) = data_out.select<8,4>(34);
            rowTrans.select<8,1>(56) = data_out.select<8,4>(35);

            //rowTrans = 100;
            vector<ushort, 16> v_Addr = v_Offsets << 2;
            cm_slm_write4 (slmX, v_Addr, rowTrans, SLM_ABGR_ENABLE);
            //cm_slm_write4 (slmX, v_Offsets, data_out, SLM_ABGR_ENABLE);
            v_Offsets += 64;  // in unit of elements
        }
#else
        vector<uint, 16> v_o;

        v_o =  (c_i_plus_1 - c_i);
        v_in = v_in_initial + (id * load_elements_per_thread + k * 16);
        v_o *= (v_in - p_i);
        v_o /= (p_i_plus_1 - p_i);
        v_o += c_i;

        v_o = cm_max<uint>(0, v_o);
        v_o = cm_min<uint>(max_input_level, v_o);
        cm_slm_write(slmX, v_in, v_o);
#endif

    }
#ifndef CM_EMU
    cm_barrier();
#else
        }
    }
#endif
#endif

    matrix<uchar, 8, 32 > color;
    matrix<uchar, 8, 128> out;
    int rd_h_pos, rd_v_pos;
    //int wr_linear_offset;
    int wr_h_pos, wr_v_pos;
    int bit_shift_amount = (BitDepth - 8);

    int l_count = (frameheight % 128 == 0)? (frameheight / 128) : ((frameheight / 128)+1);
    for(int l = 0; l < l_count; l++)   // 17 = (frameHeight / (32(H/gourp)*4groups))
    {
        for(int m = 0; m < 4; m++)
        {
            rd_h_pos = cm_linear_local_id()  * 128;                    //EACH thread in the threadgroup handle 128 byte-wide data
            rd_v_pos = cm_linear_group_id()  * 32 + m * 8 + l * 128;

            GAMMA_CORRECT_V5(R_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,0) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(G_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,1) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(B_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,2) = color.format<ushort, 8, 16>();

            ///////////////////////////////////////////////////////////////////////

            rd_h_pos = cm_linear_local_id() * 128 + 32;

            GAMMA_CORRECT_V5(R_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,64) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(G_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,65) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(B_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,66) = color.format<ushort, 8, 16>();
            /*
            wr_linear_offset = rd_v_pos * framewidth_in_bytes + cm_linear_local_id() * 256;

            write(ARGB8_Index, wr_linear_offset                            , out.row(0));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 1), out.row(1));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 2), out.row(2));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 3), out.row(3));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 4), out.row(4));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 5), out.row(5));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 6), out.row(6));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 7), out.row(7));
            */

            wr_h_pos = cm_linear_local_id() * 256;

            write(ARGB8_Index, wr_h_pos+32*0, rd_v_pos, out.select<8,1,32,1>(0,32*0));
            write(ARGB8_Index, wr_h_pos+32*1, rd_v_pos, out.select<8,1,32,1>(0,32*1));
            write(ARGB8_Index, wr_h_pos+32*2, rd_v_pos, out.select<8,1,32,1>(0,32*2));
            write(ARGB8_Index, wr_h_pos+32*3, rd_v_pos, out.select<8,1,32,1>(0,32*3));

            ///////////////////////////////////////////////////////////////////////

            rd_h_pos = cm_linear_local_id() * 128 + 64;

            GAMMA_CORRECT_V5(R_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,0) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(G_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,1) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(B_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,2) = color.format<ushort, 8, 16>();

            ///////////////////////////////////////////////////////////////////////

            rd_h_pos = cm_linear_local_id() * 128 + 96;

            GAMMA_CORRECT_V5(R_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,64) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(G_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,65) = color.format<ushort, 8, 16>();

            GAMMA_CORRECT_V5(B_I_Index, rd_h_pos, rd_v_pos, color, slmX, bit_shift_amount);
            out.select<8,1,16,4>(0,66) = color.format<ushort, 8, 16>();

            /*
            wr_linear_offset = rd_v_pos * framewidth_in_bytes + cm_linear_local_id() * 256 + 128;

            write(ARGB8_Index, wr_linear_offset                            , out.row(0));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 1), out.row(1));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 2), out.row(2));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 3), out.row(3));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 4), out.row(4));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 5), out.row(5));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 6), out.row(6));
            write(ARGB8_Index, wr_linear_offset + (framewidth_in_bytes * 7), out.row(7));
            */

            wr_h_pos = cm_linear_local_id() * 256 + 128;

            write(ARGB8_Index, wr_h_pos+32*0, rd_v_pos, out.select<8,1,32,1>(0,32*0));
            write(ARGB8_Index, wr_h_pos+32*1, rd_v_pos, out.select<8,1,32,1>(0,32*1));
            write(ARGB8_Index, wr_h_pos+32*2, rd_v_pos, out.select<8,1,32,1>(0,32*2));
            write(ARGB8_Index, wr_h_pos+32*3, rd_v_pos, out.select<8,1,32,1>(0,32*3));
        }
    }


}

//  End of Forward Gamma Correction section
////////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////////
// RGB16 to ARGB8 GPU implementation
// Unit test :
/////////////////////////////////////////////////////////////////////
extern "C" _GENX_MAIN_ void
RGB16_to_A8R8G8B8(SurfaceIndex ibufR, SurfaceIndex ibufG, SurfaceIndex ibufB, SurfaceIndex obufARGB, int src_bit_prec)
{


    int rd_x_pos = get_thread_origin_x() * 16 * sizeof(short);// + 8;
    int rd_y_pos = get_thread_origin_y() * 16;// + 8;

    matrix<short, 16, 16> R_in, G_in, B_in;
    matrix<uchar, 16, 16> A8,R8,G8,B8;
    matrix<uchar, 16, 64> ARGB_out;

    read(ibufR, rd_x_pos, rd_y_pos  , R_in.select<8, 1, 16, 1>(0, 0));
    read(ibufR, rd_x_pos, rd_y_pos+8, R_in.select<8, 1, 16, 1>(8, 0));
    R_in >>= (src_bit_prec-8);
    R8 = matrix<uchar,16,16>(R_in);

    read(ibufG, rd_x_pos, rd_y_pos  , G_in.select<8, 1, 16, 1>(0, 0));
    read(ibufG, rd_x_pos, rd_y_pos+8, G_in.select<8, 1, 16, 1>(8, 0));
    G_in >>= (src_bit_prec-8);
    G8 = matrix<uchar,16,16>(G_in);

    read(ibufB, rd_x_pos, rd_y_pos  , B_in.select<8, 1, 16, 1>(0, 0));
    read(ibufB, rd_x_pos, rd_y_pos+8, B_in.select<8, 1, 16, 1>(8, 0));
    B_in >>= (src_bit_prec-8);
    B8 = matrix<uchar,16,16>(B_in);


    ARGB_out.select<16,1,16,4>(0,0) = B8;
    ARGB_out.select<16,1,16,4>(0,1) = G8;
    ARGB_out.select<16,1,16,4>(0,2) = R8;
    ARGB_out.select<16,1,16,4>(0,3) = 0; //A8

    int wr_x_pos = get_thread_origin_x() * 16 * sizeof(uchar) * 4;
    int wr_y_pos = get_thread_origin_y() * 16;

    write(obufARGB, wr_x_pos,    wr_y_pos  , ARGB_out.select<8,1,32,1>(0,0));
    write(obufARGB, wr_x_pos,    wr_y_pos+8, ARGB_out.select<8,1,32,1>(8,0));
    write(obufARGB, wr_x_pos+32, wr_y_pos  , ARGB_out.select<8,1,32,1>(0,32));
    write(obufARGB, wr_x_pos+32, wr_y_pos+8, ARGB_out.select<8,1,32,1>(8,32));
}


//  End of RGB16 to ARGB8 section
////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////
// Unpack 10bit pack to 16bit planar GPU implementation
// Unit test :
/////////////////////////////////////////////////////////////////////

extern "C" _GENX_MAIN_ void
GPU_UNPACK_101010to16P(SurfaceIndex ibuf, SurfaceIndex obuf)
{
    int rd_h_pos = get_thread_origin_x() * 32;
    int wr_h_pos = get_thread_origin_x() * 48;
    int v_pos    = get_thread_origin_y() * 16;

    matrix<uint  , 16, 8 > in;   //<16, 24> pixels
    matrix<ushort, 16, 24> out;

    read(ibuf, rd_h_pos   , v_pos, in.select<16,1,4,1>(0,0));
    read(ibuf, rd_h_pos+16, v_pos, in.select<16,1,4,1>(0,4));

    out.select<16,1,8,3>(0,0) = (in & 0x00000FFC) >> ( 2);
    out.select<16,1,8,3>(0,1) = (in & 0x003FF000) >> (12);
    out.select<16,1,8,3>(0,2) = (in & 0xFFC00000) >> (22);

    write(obuf, wr_h_pos   , v_pos, out.select<16,1,8,1>(0,0 ));
    write(obuf, wr_h_pos+16, v_pos, out.select<16,1,8,1>(0,8 ));
    write(obuf, wr_h_pos+32, v_pos, out.select<16,1,8,1>(0,16));
}

//  End of Unpack section
////////////////////////////////////////////////////////////////////
