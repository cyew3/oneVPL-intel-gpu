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

#define WR_BLK_HEIGHT      8
#define WR_BLK_WIDTH       8
#define RD_BLK_WIDTH      16

#define good_pix_th        5
#define big_pix_th        40

#define num_big_pix_th    35
#define num_good_pix_th   10
#define good_intensity_TH 10
//#define MAX_INTENSITY 1023
//#define SrcPrecision 10
#define AVG_True  2
#define AVG_False 1

/////
#ifndef HOT_RD_BLOCKWIDTH
#define HOT_RD_BLOCKWIDTH 12
#endif

#ifndef HOT_RD_BLOCKHEIGHT
#define HOT_RD_BLOCKHEIGHT 12
#endif

#ifndef HOT_RD_BLOCKWIDTH_HALF
#define HOT_RD_BLOCKWIDTH_HALF 6
#endif

#ifndef HOT_RD_BLOCKHEIGHT_HALF
#define HOT_RD_BLOCKHEIGHT_HALF 6
#endif

#ifndef HOT_WR_BLOCKWIDTH
#define HOT_WR_BLOCKWIDTH 8
#endif

#ifndef HOT_WR_BLOCKHEIGHT
#define HOT_WR_BLOCKHEIGHT 8
#endif

#ifndef HOT_WR_BLOCKWIDTH_HALF
#define HOT_WR_BLOCKWIDTH_HALF 4
#endif

#ifndef HOT_WR_BLOCKHEIGHT_HALF
#define HOT_WR_BLOCKHEIGHT_HALF 4
#endif

/*
#ifndef VG_BLOCK_WIDTH
#define VG_BLOCK_WIDTH 8
#endif

#ifndef VG_BLOCK_HEIGHT
#define VG_BLOCK_HEIGHT 8
#endif
*/

#define VG_BLOCKHEIGHT_HALF 4
#define VG_BLOCKWIDTH_HALF 4

#ifndef WB_BLOCK_WIDTH
#define WB_BLOCK_WIDTH 16
#endif

#ifndef WB_BLOCK_HEIGHT
#define WB_BLOCK_HEIGHT 8
#endif

/////



const int index_ini[8] = {7,6,5,4,3,2,1,0};

extern "C" _GENX_MAIN_ void
Padding_16bpp(SurfaceIndex ibuf,
              SurfaceIndex obuf,
              int threadwidth  , int threadheight,
              int InFrameWidth , int InFrameHeight,
              ushort bayer_bit_depth,
              ushort bayer_pattern)
{
    matrix<short, 8, 8> out;
    matrix<ushort, 8, 8> rd_in_pixels;
    int rd_h_pos, rd_v_pos;

    vector<int, 8> index(index_ini);
#if 0
    int wr_h_pos = get_thread_origin_x() * 8 * sizeof(short);
    int wr_v_pos = get_thread_origin_y() * 8 ;
#else
    int wr_hor_pos = get_thread_origin_x() * 16 * sizeof(short);
    int wr_ver_pos = get_thread_origin_y() * 16 ;
//#pragma unroll
    for(int wr_h_pos = wr_hor_pos; wr_h_pos < (wr_hor_pos + 32); wr_h_pos += 16)
    {
//#pragma unroll
        for(int wr_v_pos = wr_ver_pos; wr_v_pos < (wr_ver_pos + 16); wr_v_pos += 8)
        {
#endif
            // compute rd_h_pos, rd_v_pos
            if(wr_h_pos == 0)
                rd_h_pos = sizeof(short);
            else if(wr_h_pos == (threadwidth - 1) * 8 * sizeof(short))
                rd_h_pos = (InFrameWidth - 9)* sizeof(short);
            else
                rd_h_pos = wr_h_pos - 16;//rd_h_pos = (get_thread_origin_x() - 1) * 8 * sizeof(short);

            if(wr_v_pos == 0)
                rd_v_pos = 1;
            else if(wr_v_pos == (threadheight - 1) * 8)
                rd_v_pos = (InFrameHeight - 9);
            else
                rd_v_pos = wr_v_pos - 8;
            // other bayer support
            //(get_thread_origin_y() - 1) * 8;

            // read pixels from surface
            read(ibuf, rd_h_pos, rd_v_pos, rd_in_pixels);

            rd_in_pixels >>= (16-bayer_bit_depth);

            if ( bayer_pattern == 3) // BAYER_TYPE3_GBRG
            {
                vector<ushort,8> tmp_row;
                for (int i=0;i<8;i=i+2){
                    tmp_row = rd_in_pixels.row(i);
                    rd_in_pixels.row(i) = rd_in_pixels.row(i+1);
                    rd_in_pixels.row(i+1) = tmp_row;
                }

            }

#if 0

            write(obuf, wr_h_pos, wr_v_pos, rd_in_pixels);

#else

            vector_ref<ushort, 8> in_row0 = rd_in_pixels.row(0);
            vector_ref<ushort, 8> in_row1 = rd_in_pixels.row(1);
            vector_ref<ushort, 8> in_row2 = rd_in_pixels.row(2);
            vector_ref<ushort, 8> in_row3 = rd_in_pixels.row(3);
            vector_ref<ushort, 8> in_row4 = rd_in_pixels.row(4);
            vector_ref<ushort, 8> in_row5 = rd_in_pixels.row(5);
            vector_ref<ushort, 8> in_row6 = rd_in_pixels.row(6);
            vector_ref<ushort, 8> in_row7 = rd_in_pixels.row(7);

            matrix<short, 8, 8> tmp_out;
            //compute output matrix
            if( wr_h_pos == 0 | wr_h_pos == (threadwidth - 1) * 8 * sizeof(short))
            {
                tmp_out.row(0) = in_row0.iselect(index);
                tmp_out.row(1) = in_row1.iselect(index);
                tmp_out.row(2) = in_row2.iselect(index);
                tmp_out.row(3) = in_row3.iselect(index);
                tmp_out.row(4) = in_row4.iselect(index);
                tmp_out.row(5) = in_row5.iselect(index);
                tmp_out.row(6) = in_row6.iselect(index);
                tmp_out.row(7) = in_row7.iselect(index);
            }
            else
            {
                tmp_out.row(0) = in_row0;
                tmp_out.row(1) = in_row1;
                tmp_out.row(2) = in_row2;
                tmp_out.row(3) = in_row3;
                tmp_out.row(4) = in_row4;
                tmp_out.row(5) = in_row5;
                tmp_out.row(6) = in_row6;
                tmp_out.row(7) = in_row7;
            }

            if( wr_v_pos == 0 | wr_v_pos == (threadheight - 1) * 8 )
            {
                out.row(7) = tmp_out.row(0);
                out.row(6) = tmp_out.row(1);
                out.row(5) = tmp_out.row(2);
                out.row(4) = tmp_out.row(3);
                out.row(3) = tmp_out.row(4);
                out.row(2) = tmp_out.row(5);
                out.row(1) = tmp_out.row(6);
                out.row(0) = tmp_out.row(7);
            }
            else
            {
                out.row(7) = tmp_out.row(7);
                out.row(6) = tmp_out.row(6);
                out.row(5) = tmp_out.row(5);
                out.row(4) = tmp_out.row(4);
                out.row(3) = tmp_out.row(3);
                out.row(2) = tmp_out.row(2);
                out.row(1) = tmp_out.row(1);
                out.row(0) = tmp_out.row(0);
            }
            write(obuf, wr_h_pos, wr_v_pos, out);
#endif
        }
    }
}

////////////////////////////////////////////////////////////////////
//
//  Vignette Correction:
//  Arch Model : SKL - works on Bayer
//  Author: Aaron Hsu
//
//  The following bayer surfaces are supported: bggr, rggb, gbrg, grbg
//  Given an image, you can either provide a mask or generate a mask (based on the image).
//  To generate the mask, you can generate values based on the distance from the center of the image, subsample this mask and then upsample.
//  The mask is given to the Vcorrector (kernel function VCorrection_bmp) to correct the vignette or even add one.
//
//  On the other hand, (follows the ATE source),
//    you can have 4 mask surfaces and then extract the correct Bayer pattern from the mask (use kernel function Bayer_MaskReduce) to create one mask.
//    You can then use VCorrection_bmp to correct/add vignette.
//
//  Output images are nv12 files to represent bayer surfaces.
//
////////////////////////////////////////////////////////////////////

#ifndef BLOCK_WIDTH
#define BLOCK_WIDTH 8
#endif

#ifndef BLOCK_HEIGHT
#define BLOCK_HEIGHT 8
#endif

#define bggr 0
#define rggb 1
#define gbrg 2
#define grbg 3

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

////////////////////////////////////////////////////////////////////
//
//  Auto-White Balance:
//  Arch Model : SKL - works on Bayer
//  Author: Aaron Hsu
//
//  The following bayer surfaces are supported: bggr, rggb, gbrg, grbg
//  In WhiteBalance.h, there is a section of values a user can input/change depending on how to color balance the image.
//
//  A user can specify thresholds, image window, LMS scaling or not, bit precision, realistic color checking, video calibration, and bayer surface format.
//
//  The C code is taken from the ATE source code. The CM gpu code serves to match the C source code.
//
//  Output images are nv12 files to represent bayer surfaces.
//
////////////////////////////////////////////////////////////////////

//#include "cm/cm.h"
const unsigned int offset_init[8] = {0,1,2,3,4,5,6,7};
#define bggr 0
#define rggb 1
#define gbrg 2
#define grbg 3

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






////////////////////////////////////////////////////////////////////

extern "C" _GENX_MAIN_ void
BAYER_GAIN(SurfaceIndex ibuf ,
           SurfaceIndex obuf,
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


extern "C" _GENX_MAIN_ void
GPU_3x3CCM( SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
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


 //02~04: for supporting 4Kx2K image (16x16 block/thread)
 //01:    Not supporting 4Kx2K image (8 x 8 block/thread)
extern "C" _GENX_MAIN_ void
GOOD_PIXEL_CHECK(SurfaceIndex ibuf ,
                 SurfaceIndex obuf0,
                 SurfaceIndex obuf1,
                 int shift)
{
#if 0
    matrix<ushort, 12, 16 >   rd_in;
    matrix<ushort, 12, 16 >   src;
    matrix<ushort, 8 , 8  >   diff;
    matrix<ushort, 8 , 8  >   mask0, mask1;
    matrix<uchar , 8 , 8  >   acc0;//(0);
    matrix<uchar , 8 , 8  >   acc1;//(0);

    int rd_hor_pos = (get_thread_origin_x() * 16 - 2) * sizeof(short);
    int rd_ver_pos = (get_thread_origin_y() * 16 - 2);



#pragma unroll
    for(int rd_h_pos = rd_hor_pos; rd_h_pos < (rd_hor_pos + 32); rd_h_pos += 16)
    {
#pragma unroll
        for(int rd_v_pos = rd_ver_pos; rd_v_pos < (rd_ver_pos + 16); rd_v_pos += 8)
        {
            int dum = 1;
            if(rd_h_pos == 12 && rd_v_pos == 6)
                dum = 0;

            read(ibuf, rd_h_pos, rd_v_pos  , rd_in.select<8,1,16,1>(0,0));
            read(ibuf, rd_h_pos, rd_v_pos+8, rd_in.select<4,1,16,1>(8,0));

            ///// ACCORDING TO B-SPEC: Precision: 16-bit pixels truncated to 8-bits before subtract & compares.
            src = rd_in >> shift ;

            matrix<ushort, 8, 8> premask = (src.select<8,1,8,1>(2,2) <= good_intensity_TH);
            matrix<ushort, 8, 8> pixmask;

            pixmask = premask | (src.select<8,1,8,1>(0,2) <= good_intensity_TH);
            diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,8,1>(0,2), - src.select<8,1,8,1>(2,2)));
            diff.merge(good_pix_th, pixmask);

            acc0  = (diff > good_pix_th);
            acc1  = (diff >  big_pix_th);

            pixmask = premask | (src.select<8,1,8,1>(2,0) <= good_intensity_TH);
            diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,8,1>(2,0), - src.select<8,1,8,1>(2,2)));
            diff.merge(good_pix_th, pixmask);

            acc0 += (diff > good_pix_th);
            acc1 += (diff >  big_pix_th);

            pixmask = premask | (src.select<8,1,8,1>(4,2) <= good_intensity_TH);
            diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,8,1>(4,2), - src.select<8,1,8,1>(2,2)));
            diff.merge(good_pix_th, pixmask);

            acc0 += (diff > good_pix_th);
            acc1 += (diff >  big_pix_th);

            pixmask = premask | (src.select<8,1,8,1>(2,4) <= good_intensity_TH);
            diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,8,1>(2,4), - src.select<8,1,8,1>(2,2)));
            diff.merge(good_pix_th, pixmask);

            acc0 += (diff > good_pix_th);
            acc1 += (diff >  big_pix_th);

            ///////////////////////////////////////////////////////////
            int wr_h_pos = (rd_h_pos / 2) + 2;//(get_thread_origin_x() * 8 );
            int wr_v_pos = (rd_v_pos + 2)    ;//(get_thread_origin_y() * 8 );

            matrix< uchar, 8 , 8  >   ACC0 = acc0;
            matrix< uchar, 8 , 8  >   ACC1 = acc1;

            write(obuf0, wr_h_pos, wr_v_pos, ACC0);
            write(obuf1, wr_h_pos, wr_v_pos, ACC1);
        }
    }
#else

    matrix<ushort, 12, 32 >   rd_in;
    matrix<uchar , 12, 32 >   src;
    matrix<ushort,  8, 16 >   diff;

#if 0
    matrix<ushort , 16, 16 >   acc0;//(0);
    matrix<ushort , 16, 16 >   acc1;//(0);
#else
    matrix<uchar , 16, 16 >   acc0;//(0);
    matrix<uchar , 16, 16 >   acc1;//(0);
#endif

    int rd_hor_pos = (get_thread_origin_x() * 16 - 2) * sizeof(short);
    int rd_ver_pos = (get_thread_origin_y() * 16 - 2);

    read(ibuf, rd_hor_pos   , rd_ver_pos  , rd_in.select<8,1,16,1>(0,0 ));
    read(ibuf, rd_hor_pos+32, rd_ver_pos  , rd_in.select<8,1,16,1>(0,16));
    read(ibuf, rd_hor_pos   , rd_ver_pos+8, rd_in.select<4,1,16,1>(8,0 ));
    read(ibuf, rd_hor_pos+32, rd_ver_pos+8, rd_in.select<4,1,16,1>(8,16));

    ///// ACCORDING TO B-SPEC: Precision: 16-bit pixels truncated to 8-bits before subtract & compares.
    src = rd_in >> shift ;

    matrix<ushort, 8, 16> premask = (src.select<8,1,16,1>(2,2) <= good_intensity_TH);
    matrix<ushort, 8, 16> pixmask;

    pixmask = premask | (src.select<8,1,16,1>(0,2) <= good_intensity_TH);
    diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,16,1>(0,2), - src.select<8,1,16,1>(2,2)));
    diff.merge(good_pix_th, pixmask);

    acc0.select<8,1,16,1>(0,0)  = (diff > good_pix_th);
    acc1.select<8,1,16,1>(0,0)  = (diff >  big_pix_th);

    pixmask = premask | (src.select<8,1,16,1>(2,0) <= good_intensity_TH);
    diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,16,1>(2,0), - src.select<8,1,16,1>(2,2)));
    diff.merge(good_pix_th, pixmask);

    acc0.select<8,1,16,1>(0,0) += (diff > good_pix_th);
    acc1.select<8,1,16,1>(0,0) += (diff >  big_pix_th);

    pixmask = premask | (src.select<8,1,16,1>(4,2) <= good_intensity_TH);
    diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,16,1>(4,2), - src.select<8,1,16,1>(2,2)));
    diff.merge(good_pix_th, pixmask);

    acc0.select<8,1,16,1>(0,0) += (diff > good_pix_th);
    acc1.select<8,1,16,1>(0,0) += (diff >  big_pix_th);

    pixmask = premask | (src.select<8,1,16,1>(2,4) <= good_intensity_TH);
    diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,16,1>(2,4), - src.select<8,1,16,1>(2,2)));
    diff.merge(good_pix_th, pixmask);

    acc0.select<8,1,16,1>(0,0) += (diff > good_pix_th);
    acc1.select<8,1,16,1>(0,0) += (diff >  big_pix_th);

    ///////////////////////////////////////////////////////////

    read(ibuf, rd_hor_pos   , rd_ver_pos+8,  rd_in.select<8,1,16,1>(0,0 ));
    read(ibuf, rd_hor_pos+32, rd_ver_pos+8,  rd_in.select<8,1,16,1>(0,16));
    read(ibuf, rd_hor_pos   , rd_ver_pos+16, rd_in.select<4,1,16,1>(8,0 ));
    read(ibuf, rd_hor_pos+32, rd_ver_pos+16, rd_in.select<4,1,16,1>(8,16));

    src = rd_in >> shift;

    premask = (src.select<8,1,16,1>(2,2) <= good_intensity_TH);

    pixmask = premask | (src.select<8,1,16,1>(0,2) <= good_intensity_TH);
    diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,16,1>(0,2), - src.select<8,1,16,1>(2,2)));
    diff.merge(good_pix_th, pixmask);

    acc0.select<8,1,16,1>(8,0)  = (diff > good_pix_th);
    acc1.select<8,1,16,1>(8,0)  = (diff >  big_pix_th);

    pixmask = premask | (src.select<8,1,16,1>(2,0) <= good_intensity_TH);
    diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,16,1>(2,0), - src.select<8,1,16,1>(2,2)));
    diff.merge(good_pix_th, pixmask);

    acc0.select<8,1,16,1>(8,0) += (diff > good_pix_th);
    acc1.select<8,1,16,1>(8,0) += (diff >  big_pix_th);

    pixmask = premask | (src.select<8,1,16,1>(4,2) <= good_intensity_TH);
    diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,16,1>(4,2), - src.select<8,1,16,1>(2,2)));
    diff.merge(good_pix_th, pixmask);

    acc0.select<8,1,16,1>(8,0) += (diff > good_pix_th);
    acc1.select<8,1,16,1>(8,0) += (diff >  big_pix_th);

    pixmask = premask | (src.select<8,1,16,1>(2,4) <= good_intensity_TH);
    diff  = cm_abs<ushort>(cm_add<short>(src.select<8,1,16,1>(2,4), - src.select<8,1,16,1>(2,2)));
    diff.merge(good_pix_th, pixmask);

    acc0.select<8,1,16,1>(8,0) += (diff > good_pix_th);
    acc1.select<8,1,16,1>(8,0) += (diff >  big_pix_th);

    ///////////////////////////////////////////////////////////

    int wr_h_pos = (get_thread_origin_x() * 16 );
    int wr_v_pos = (get_thread_origin_y() * 16 );

#if 0
    acc0.select<16,1,8,1>(0,0) = acc0.select<16,1,8,2>(0,0);
    acc1.select<16,1,8,1>(0,0) = acc1.select<16,1,8,2>(0,0);

    write(obuf0, wr_h_pos, wr_v_pos  , acc0.select<16,1,8,1>(0,0));
    write(obuf1, wr_h_pos, wr_v_pos  , acc1.select<16,1,8,1>(0,0));
    //write(obuf0, wr_h_pos, wr_v_pos+8, acc0.select<8,1,8,1>(8,0));
    //write(obuf1, wr_h_pos, wr_v_pos+8, acc1.select<8,1,8,1>(8,0));
#else
    //write(obuf0, wr_h_pos, wr_v_pos  , acc0.select<8,1,16,1>(0,0));
    //write(obuf1, wr_h_pos, wr_v_pos  , acc1.select<8,1,16,1>(0,0));
    //write(obuf0, wr_h_pos, wr_v_pos+8, acc0.select<8,1,16,1>(8,0));
    //write(obuf1, wr_h_pos, wr_v_pos+8, acc1.select<8,1,16,1>(8,0));

    write(obuf0, wr_h_pos, wr_v_pos, acc0);
    write(obuf1, wr_h_pos, wr_v_pos, acc1);
#endif
#endif
}




const unsigned short spatial_mask_init[8][8] = {{0,1,0,1,0,1,0,1},{1,0,1,0,1,0,1,0},{0,1,0,1,0,1,0,1},{1,0,1,0,1,0,1,0},
                                                {0,1,0,1,0,1,0,1},{1,0,1,0,1,0,1,0},{0,1,0,1,0,1,0,1},{1,0,1,0,1,0,1,0}};
const signed short coef_init_0[8] = {-1,5,2,3,-1,0,0,0};
const signed short coef_init_1[8] = {-1,3,2,5,-1,0,0,0};
#if 0
extern "C" _GENX_MAIN_ void
RESTOREG(SurfaceIndex ibufBayer,
         SurfaceIndex ibuf0, SurfaceIndex ibuf1,
         SurfaceIndex obufHOR, SurfaceIndex obufVER, SurfaceIndex obufAVG,
         SurfaceIndex obuf, int x , int y, ushort max_intensity)

{
    matrix< uchar, 12, 16 >   src0;
    matrix< uchar, 12, 16 >   src1;
    matrix< uchar, 8,  8  >   dst ;
    matrix<ushort, 8,  8  >   mask;

    int rd_h_pos = ((x + get_thread_origin_x()) * 8 - 2);
    int rd_v_pos = ((y + get_thread_origin_y()) * 8 - 2);

    read(ibuf0, rd_h_pos, rd_v_pos, src0);
    read(ibuf1, rd_h_pos, rd_v_pos, src1);

    matrix< ushort, 8, 8 >   acc0(0);
    matrix< ushort, 8, 8 >   acc1(0);

    int dum = 1;
    if(rd_h_pos == 6 && rd_v_pos == 6)
        dum = 0;

#pragma unroll
    for(int i = 1; i <= 3; i++)
    {
#pragma unroll
        for(int j = 1; j <= 3; j++)
        {
            acc0 += src0.select<8,1,8,1>(i,j);
            acc1 += src1.select<8,1,8,1>(i,j);
        }
    }

    mask = (acc0 > num_good_pix_th) | (acc1 > num_big_pix_th);
    dst.merge(2, 1, mask);

    ///////////////////////////////////////////////////////////
    int wr_h_pos = ((get_thread_origin_x() + x) * 8 );
    int wr_v_pos = ((get_thread_origin_y() + y) * 8 );
    write(obuf, wr_h_pos, wr_v_pos, dst);

    /////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////

    wr_h_pos = ((get_thread_origin_x() + x) * 8 ) * sizeof(short);
    wr_v_pos = ((get_thread_origin_y() + y) * 8 );

    matrix<short, 8, 8 > G_H, G_V, G_A;
    matrix<short, 12,16> srcBayer;

    rd_h_pos = ((get_thread_origin_x() + x) * 8 - 2) * sizeof(short);
    read(ibufBayer, rd_h_pos, rd_v_pos  , srcBayer.select<8,1,16,1>(0,0));
    read(ibufBayer, rd_h_pos, rd_v_pos+8, srcBayer.select<4,1,16,1>(8,0));

    matrix<ushort, 8, 8> spatial_mask(spatial_mask_init);

    matrix<ushort, 8, 8> M;
    matrix< int, 8, 8> SUM0, SUM1;
    vector< int, 8 > coef_0(coef_init_0);
    vector< int, 8 > coef_1(coef_init_1);

    M = (cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(0,2), -srcBayer.select<8,1,8,1>(2,2))) <
         cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(2,2), -srcBayer.select<8,1,8,1>(4,2))));

    matrix<short, 8, 8> coef;
    matrix<short, 8, 8> tmp0, tmp1;

    tmp0 = srcBayer.select<8,1,8,1>(2, 2) - srcBayer.select<8,1,8,1>(0, 2);
    tmp1 = srcBayer.select<8,1,8,1>(2, 2) - srcBayer.select<8,1,8,1>(4, 2);
    SUM0 = tmp0 + tmp1;

    coef.merge(coef_0(1), coef_1(1), M);
    SUM0 += cm_mul<int>(srcBayer.select<8,1,8,1>(1, 2), coef);

    coef.merge(coef_0(3), coef_1(3), M);
    SUM0 += cm_mul<int>(srcBayer.select<8,1,8,1>(3, 2), coef);

    SUM0 >>= 3;
    SUM0 = cm_max<int>(SUM0, 0  );
    G_V  = cm_min<int>(SUM0, max_intensity);
    G_V.merge(srcBayer.select<8,1,8,1>(2,2), spatial_mask);
    write(obufVER, wr_h_pos, wr_v_pos, G_V);
    ///////////////////////////////////////////////////////////

    M = (cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(2,0), -srcBayer.select<8,1,8,1>(2,2))) <
         cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(2,2), -srcBayer.select<8,1,8,1>(2,4))));

    tmp0 = srcBayer.select<8,1,8,1>(2, 2) - srcBayer.select<8,1,8,1>(2, 0);
    tmp1 = srcBayer.select<8,1,8,1>(2, 2) - srcBayer.select<8,1,8,1>(2, 4);
    SUM0 = tmp0 + tmp1;

    coef.merge(coef_0(1), coef_1(1), M);
    SUM0 += cm_mul<int>(srcBayer.select<8,1,8,1>(2, 1), coef);

    coef.merge(coef_0(3), coef_1(3), M);
    SUM0 += cm_mul<int>(srcBayer.select<8,1,8,1>(2, 3), coef);

    SUM0 >>= 3;
    SUM0 = cm_max<int>(SUM0, 0  );
    G_H  = cm_min<int>(SUM0, max_intensity);
    G_H.merge(srcBayer.select<8,1,8,1>(2,2), spatial_mask);
    write(obufHOR, wr_h_pos, wr_v_pos, G_H);
    ///////////////////////////////////////////////////////////
    matrix<ushort, 8, 8> tmpA = tmp0.format<ushort, 8, 8>();
    matrix<ushort, 8, 8> tmpB = tmp1.format<ushort, 8, 8>();

    matrix<int, 8, 8> AVG_R, TMP_S; //cen == (2,2)

    matrix<int, 8, 8> AVG_G;
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
    write(obufAVG, wr_h_pos, wr_v_pos, G_A);
}
#else
extern "C" _GENX_MAIN_ void
RESTOREG(SurfaceIndex ibufBayer,
         SurfaceIndex ibuf0, SurfaceIndex ibuf1,
         SurfaceIndex obufHOR, SurfaceIndex obufVER, SurfaceIndex obufAVG,
         SurfaceIndex obuf, int x , int y, ushort max_intensity)
{
    matrix< uchar, 12, 16 >   src0;
    matrix< uchar, 12, 16 >   src1;
    matrix< uchar, 8,  8  >   dst ;
    matrix<ushort, 8,  8  >   mask;

    int rd_h_pos = ((x + get_thread_origin_x()) * 8 - 2);
    int rd_v_pos = ((y + get_thread_origin_y()) * 8 - 2);

    read(ibuf0, rd_h_pos, rd_v_pos, src0);
    read(ibuf1, rd_h_pos, rd_v_pos, src1);

    matrix< ushort, 8, 8 >   acc0(0);
    matrix< ushort, 8, 8 >   acc1(0);

    int dum = 1;
    if(rd_h_pos == 6 && rd_v_pos == 6)
        dum = 0;

#pragma unroll
    for(int i = 1; i <= 3; i++)
    {
#pragma unroll
        for(int j = 1; j <= 3; j++)
        {
            acc0 += src0.select<8,1,8,1>(i,j);
            acc1 += src1.select<8,1,8,1>(i,j);
        }
    }

    mask = (acc0 > num_good_pix_th) | (acc1 > num_big_pix_th);
    dst.merge(2, 1, mask);

    ///////////////////////////////////////////////////////////
    int wr_h_pos = ((get_thread_origin_x() + x) * 8 );
    int wr_v_pos = ((get_thread_origin_y() + y) * 8 );
    write(obuf, wr_h_pos, wr_v_pos, dst);

    /////////////////////////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////////////////////////

    matrix<short, 8, 8 > G_H, G_V, G_A;
    matrix<short, 12,16> srcBayer;

    rd_h_pos = ((get_thread_origin_x() + x) * 8 - 2) * sizeof(short);
    read(ibufBayer, rd_h_pos, rd_v_pos  , srcBayer.select<8,1,16,1>(0,0));
    read(ibufBayer, rd_h_pos, rd_v_pos+8, srcBayer.select<4,1,16,1>(8,0));

    matrix<ushort, 8, 8> spatial_mask(spatial_mask_init);

    matrix<ushort, 8, 8> M;
    matrix< int, 8, 8> SUM0, SUM1;
    vector< int, 8 > coef_0(coef_init_0);
    vector< int, 8 > coef_1(coef_init_1);

    M = (cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(0,2), -srcBayer.select<8,1,8,1>(2,2))) <
         cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(2,2), -srcBayer.select<8,1,8,1>(4,2))));

    SUM0  = cm_mul<int>(srcBayer.select<8,1,8,1>(0, 2), coef_0(0)) + cm_mul<int>(srcBayer.select<8,1,8,1>(2, 2), coef_0(2));
    SUM1  = cm_mul<int>(srcBayer.select<8,1,8,1>(4, 2), coef_0(4)) + SUM0;
    SUM0  = cm_mul<int>(srcBayer.select<8,1,8,1>(1, 2), coef_0(1)) + SUM1;
    SUM0 += cm_mul<int>(srcBayer.select<8,1,8,1>(3, 2), coef_0(3));
    SUM0 >>= 3;

    SUM1 += cm_mul<int>(srcBayer.select<8,1,8,1>(1, 2), coef_1(1));
    SUM1 += cm_mul<int>(srcBayer.select<8,1,8,1>(3, 2), coef_1(3));
    SUM1 >>= 3;

    SUM1.merge(SUM0, M);
    SUM1 = cm_max<int>(SUM1, 0  );
    G_V  = cm_min<int>(SUM1, max_intensity);
    G_V.merge(srcBayer.select<8,1,8,1>(2,2), spatial_mask);
    ///////////////////////////////////////////////////////////

    M = (cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(2,0), -srcBayer.select<8,1,8,1>(2,2))) <
         cm_abs<ushort>(cm_add<short>(srcBayer.select<8,1,8,1>(2,2), -srcBayer.select<8,1,8,1>(2,4))));

    SUM0  = cm_mul<int>(srcBayer.select<8,1,8,1>(2, 0), coef_0(0)) + cm_mul<int>(srcBayer.select<8,1,8,1>(2, 2), coef_0(2));
    SUM1  = cm_mul<int>(srcBayer.select<8,1,8,1>(2, 4), coef_0(4)) + SUM0;
    SUM0  = cm_mul<int>(srcBayer.select<8,1,8,1>(2, 1), coef_0(1)) + SUM1;
    SUM0 += cm_mul<int>(srcBayer.select<8,1,8,1>(2, 3), coef_0(3));
    SUM0  = SUM0 >> 3;

    SUM1 += cm_mul<int>(srcBayer.select<8,1,8,1>(2, 1), coef_1(1));
    SUM1 += cm_mul<int>(srcBayer.select<8,1,8,1>(2, 3), coef_1(3));
    SUM1  = SUM1 >> 3;

    SUM1.merge(SUM0, M);
    SUM1 = cm_max<int>(SUM1, 0  );
    G_H  = cm_min<int>(SUM1, max_intensity);
    G_H.merge(srcBayer.select<8,1,8,1>(2,2), spatial_mask);

    ///////////////////////////////////////////////////////////
    matrix<int, 8, 8> AVG_G, AVG_R, TMP_S; //cen == (2,2)
    AVG_G  = srcBayer.select<8,1,8,1>(0,1)         + srcBayer.select<8,1,8,1>(0,3);
    AVG_G += srcBayer.select<8,1,8,1>(1,0); AVG_G += srcBayer.select<8,1,8,1>(1,4);
    AVG_G += srcBayer.select<8,1,8,1>(3,0); AVG_G += srcBayer.select<8,1,8,1>(3,4);
    AVG_G += srcBayer.select<8,1,8,1>(4,1); AVG_G += srcBayer.select<8,1,8,1>(4,3);

    TMP_S  = srcBayer.select<8,1,8,1>(1,2)         + srcBayer.select<8,1,8,1>(2,1);
    TMP_S += srcBayer.select<8,1,8,1>(2,3); TMP_S += srcBayer.select<8,1,8,1>(3,2);

    AVG_G += cm_mul<int>(TMP_S, 2);
    AVG_G  = AVG_G >> 4;

    AVG_R  = srcBayer.select<8,1,8,1>(0,2)         + srcBayer.select<8,1,8,1>(0,4);
    AVG_R += srcBayer.select<8,1,8,1>(2,0); AVG_R += srcBayer.select<8,1,8,1>(2,2);
    AVG_R += srcBayer.select<8,1,8,1>(2,4); AVG_R += srcBayer.select<8,1,8,1>(4,0);
    AVG_R += srcBayer.select<8,1,8,1>(4,2); AVG_R += srcBayer.select<8,1,8,1>(4,4);
    AVG_R  = AVG_R >> 3;

    AVG_G += srcBayer.select<8,1,8,1>(2,2);
    AVG_G -= AVG_R;

    AVG_G = cm_max<int>(AVG_G, 0);
    AVG_G = cm_min<int>(AVG_G, max_intensity);

    G_A.merge(srcBayer.select<8,1,8,1>(2,2), AVG_G, spatial_mask);

    ///////////////////////////////////////////////////////////
    wr_h_pos = ((get_thread_origin_x() + x) * 8 ) * sizeof(short);
    wr_v_pos = ((get_thread_origin_y() + y) * 8 );

    write(obufAVG, wr_h_pos, wr_v_pos, G_A);
    write(obufHOR, wr_h_pos, wr_v_pos, G_H);
    write(obufVER, wr_h_pos, wr_v_pos, G_V);
}

#endif

extern "C" _GENX_MAIN_ void
RESTOREBandR(SurfaceIndex ibufBayer,
             SurfaceIndex ibufGHOR, SurfaceIndex ibufGVER, SurfaceIndex ibufGAVG,
             SurfaceIndex obufBHOR, SurfaceIndex obufBVER, SurfaceIndex obufBAVG,
             SurfaceIndex obufRHOR, SurfaceIndex obufRVER, SurfaceIndex obufRAVG,
             SurfaceIndex ibufAVGMASK,
             int x , int y, ushort max_intensity)
{
    int rd_h_pos = ((get_thread_origin_x() + x) * 8 - 2) * sizeof(short);
    int rd_v_pos = ((get_thread_origin_y() + y) * 8 - 2) ;

    int wr_h_pos = ((get_thread_origin_x() + x) * 8    ) * sizeof(short);
    int wr_v_pos = ((get_thread_origin_y() + y) * 8    ) ;

    int dum;
    if(wr_h_pos == 112 && wr_v_pos ==8)
        dum =1;

    matrix<ushort, 12, 16> bayer;
    read(ibufBayer, rd_h_pos, rd_v_pos  , bayer.select<8,1,16,1>(0,0));
    read(ibufBayer, rd_h_pos, rd_v_pos+8, bayer.select<4,1,16,1>(8,0));
    bayer = cm_max<short>(bayer, 0);
    bayer = cm_min<short>(bayer, max_intensity);

    matrix<short, 8, 8> out_B, out_R;
    //matrix<ushort, 8, 8> out_B, out_R;

    // fixed by samuel on 9/5/2013, from short to int
    matrix<int, 8,16> BR32_c; // B32_c,R23_c
    matrix<int,12, 8> BR34_c; // B23_c,R32_c
    matrix<int, 8, 8> BR33_c; // B33_c,R33_c

    matrix<short, 12, 16> dBG_c;
    matrix<short, 12, 16> Green_c, Green_c_x2;

    matrix<uchar, 8, 8> avg_mask;
    read(ibufAVGMASK, rd_h_pos / 2, rd_v_pos, avg_mask);

    /////// HORIZONTAL COMPONENT ////////////////////////////////////

    read(ibufGHOR, rd_h_pos, rd_v_pos  , Green_c.select<8,1,16,1>(0,0));
    read(ibufGHOR, rd_h_pos, rd_v_pos+8, Green_c.select<4,1,16,1>(8,0));

    Green_c_x2 = cm_mul<int>(Green_c, 2);  // fixed by samuel on 9/5/2013, from short to int
    dBG_c = bayer - Green_c;

    BR32_c  = Green_c_x2.select<8 ,1,16,1>(2,0);
    BR34_c  = Green_c_x2.select<12,1, 8,1>(0,2); // = B32_c;
    BR33_c  = Green_c_x2.select<8 ,1, 8,1>(2,2);

    BR32_c += dBG_c.select< 8,1,16,1>(1,0);
    BR32_c += dBG_c.select< 8,1,16,1>(3,0);
    BR32_c >>= 1;
    BR32_c = cm_max<ushort>(BR32_c, 0);  // fixed by samuel on 9/5/2013, from ushort to ushort x6 below
    BR32_c = cm_min<ushort>(BR32_c, max_intensity);

    BR34_c += dBG_c.select<12,1, 8,1>(0,1);
    BR34_c += dBG_c.select<12,1, 8,1>(0,3);
    BR34_c >>= 1;
    BR34_c = cm_max<ushort>(BR34_c, 0);
    BR34_c = cm_min<ushort>(BR34_c, max_intensity);

    BR33_c += BR32_c .select<8,1,8,1>(0,1);
    BR33_c -= Green_c.select<8,1,8,1>(2,1);
    BR33_c += BR32_c .select<8,1,8,1>(0,3);
    BR33_c -= Green_c.select<8,1,8,1>(2,3);
    BR33_c >>= 1;
    BR33_c = cm_max<ushort>(BR33_c, 0);
    BR33_c = cm_min<ushort>(BR33_c, max_intensity);

    out_B.select<4,2,4,2>(0,0) = bayer .select<4,2,4,2>(2,2);
    out_R.select<4,2,4,2>(1,1) = bayer .select<4,2,4,2>(3,3);

    out_B.select<4,2,4,2>(0,1) = BR34_c.select<4,2,4,2>(2,1);
    out_B.select<4,2,4,2>(1,0) = BR32_c.select<4,2,4,2>(1,2);
    out_B.select<4,2,4,2>(1,1) = BR33_c.select<4,2,4,2>(1,1);

    out_R.select<4,2,4,2>(0,1) = BR32_c.select<4,2,4,2>(0,3);
    out_R.select<4,2,4,2>(1,0) = BR34_c.select<4,2,4,2>(3,0);
    out_R.select<4,2,4,2>(0,0) = BR33_c.select<4,2,4,2>(0,0);

    write(obufBHOR, wr_h_pos, wr_v_pos, out_B);
    write(obufRHOR, wr_h_pos, wr_v_pos, out_R);

    /////// VERTICAL COMPONENT ////////////////////////////////////

    read(ibufGVER, rd_h_pos, rd_v_pos  , Green_c.select<8,1,16,1>(0,0));
    read(ibufGVER, rd_h_pos, rd_v_pos+8, Green_c.select<4,1,16,1>(8,0));

    // fixed by samuel on 9/5/2013, from short to int
    Green_c_x2 = cm_mul<int>(Green_c, 2);
    dBG_c = bayer - Green_c;

    BR32_c  = Green_c_x2.select<8 ,1,16,1>(2,0);
    BR34_c  = Green_c_x2.select<12,1, 8,1>(0,2); // = B32_c;
    BR33_c  = Green_c_x2.select<8 ,1, 8,1>(2,2);

    BR32_c += dBG_c.select< 8,1,16,1>(1,0);
    BR32_c += dBG_c.select< 8,1,16,1>(3,0);
    BR32_c >>= 1;
    BR32_c = cm_max<ushort>(BR32_c, 0);  // fixed by samuel on 9/5/2013, from short to ushort x6 below
    BR32_c = cm_min<ushort>(BR32_c, max_intensity);

    BR34_c += dBG_c.select<12,1, 8,1>(0,1);
    BR34_c += dBG_c.select<12,1, 8,1>(0,3);
    BR34_c >>= 1;
    BR34_c = cm_max<ushort>(BR34_c, 0);
    BR34_c = cm_min<ushort>(BR34_c, max_intensity);

    BR33_c += BR34_c .select<8,1,8,1>(1,0);
    BR33_c -= Green_c.select<8,1,8,1>(1,2);
    BR33_c += BR34_c .select<8,1,8,1>(3,0);
    BR33_c -= Green_c.select<8,1,8,1>(3,2);
    BR33_c >>= 1;
    BR33_c = cm_max<ushort>(BR33_c, 0);
    BR33_c = cm_min<ushort>(BR33_c, max_intensity);

    out_B.select<4,2,4,2>(0,0) = bayer .select<4,2,4,2>(2,2);
    out_R.select<4,2,4,2>(1,1) = bayer .select<4,2,4,2>(3,3);

    out_B.select<4,2,4,2>(0,1) = BR34_c.select<4,2,4,2>(2,1);
    out_B.select<4,2,4,2>(1,0) = BR32_c.select<4,2,4,2>(1,2);
    out_B.select<4,2,4,2>(1,1) = BR33_c.select<4,2,4,2>(1,1);

    out_R.select<4,2,4,2>(0,1) = BR32_c.select<4,2,4,2>(0,3);
    out_R.select<4,2,4,2>(1,0) = BR34_c.select<4,2,4,2>(3,0);
    out_R.select<4,2,4,2>(0,0) = BR33_c.select<4,2,4,2>(0,0);

    write(obufBVER, wr_h_pos, wr_v_pos, out_B);
    write(obufRVER, wr_h_pos, wr_v_pos, out_R);

    /////// AVERAGE COMPONENT ////////////////////////////////////
    if((avg_mask == 2).any())
    {
    read(ibufGAVG, rd_h_pos, rd_v_pos  , Green_c.select<8,1,16,1>(0,0));
    read(ibufGAVG, rd_h_pos, rd_v_pos+8, Green_c.select<4,1,16,1>(8,0));

    // fixed by samuel on 9/5/2013, from short to int
    Green_c_x2 = cm_mul<int>(Green_c, 2);
    dBG_c = bayer - Green_c;

    BR32_c  = Green_c_x2.select<8 ,1,16,1>(2,0);
    BR34_c  = Green_c_x2.select<12,1, 8,1>(0,2); // = B32_c;
    // fixed by samuel on 9/5/2013, from short to int
    BR33_c  = cm_mul<int>(Green_c_x2.select<8 ,1, 8,1>(2,2), 2);

    BR32_c += dBG_c.select< 8,1,16,1>(1,0);
    BR32_c += dBG_c.select< 8,1,16,1>(3,0);
    BR32_c >>= 1;

    // fixed by samuel on 9/5/2013, from short to ushort x6 below
    BR32_c = cm_max<ushort>(BR32_c, 0);
    BR32_c = cm_min<ushort>(BR32_c, max_intensity);

    BR34_c += dBG_c.select<12,1, 8,1>(0,1);
    BR34_c += dBG_c.select<12,1, 8,1>(0,3);
    BR34_c >>= 1;
    BR34_c = cm_max<ushort>(BR34_c, 0);
    BR34_c = cm_min<ushort>(BR34_c, max_intensity);

    BR33_c += BR34_c .select<8,1,8,1>(1,0);
    BR33_c -= Green_c.select<8,1,8,1>(1,2);
    BR33_c += BR34_c .select<8,1,8,1>(3,0);
    BR33_c -= Green_c.select<8,1,8,1>(3,2);
    BR33_c += BR32_c .select<8,1,8,1>(0,1);
    BR33_c -= Green_c.select<8,1,8,1>(2,1);
    BR33_c += BR32_c .select<8,1,8,1>(0,3);
    BR33_c -= Green_c.select<8,1,8,1>(2,3);
    BR33_c >>= 2;
    BR33_c = cm_max<ushort>(BR33_c, 0);
    BR33_c = cm_min<ushort>(BR33_c, max_intensity);

    out_B.select<4,2,4,2>(0,0) = bayer .select<4,2,4,2>(2,2);
    out_R.select<4,2,4,2>(1,1) = bayer .select<4,2,4,2>(3,3);

    out_B.select<4,2,4,2>(0,1) = BR34_c.select<4,2,4,2>(2,1);
    out_B.select<4,2,4,2>(1,0) = BR32_c.select<4,2,4,2>(1,2);
    out_B.select<4,2,4,2>(1,1) = BR33_c.select<4,2,4,2>(1,1);

    out_R.select<4,2,4,2>(0,1) = BR32_c.select<4,2,4,2>(0,3);
    out_R.select<4,2,4,2>(1,0) = BR34_c.select<4,2,4,2>(3,0);
    out_R.select<4,2,4,2>(0,0) = BR33_c.select<4,2,4,2>(0,0);

    write(obufBAVG, wr_h_pos, wr_v_pos, out_B);
    write(obufRAVG, wr_h_pos, wr_v_pos, out_R);
    }
}

/* OLD VERSION */
/*
extern "C" _GENX_MAIN_ void
SAD(SurfaceIndex ibufRHOR, SurfaceIndex ibufGHOR, SurfaceIndex ibufBHOR,
    SurfaceIndex ibufRVER, SurfaceIndex ibufGVER, SurfaceIndex ibufBVER,
    SurfaceIndex obufHV, int x , int y)
{
    int rd_h_pos = ((get_thread_origin_x() + x) * 8) * sizeof(short);
    int rd_v_pos = ((get_thread_origin_y() + y) * 8);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////    PART I: COMPUTE MIN_COST_H   ////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    matrix<short, 16, 16> R, G, B;
    read(ibufRHOR, rd_h_pos - 8, rd_v_pos - 4, R.select<8,1,16,1>(0,0));
    read(ibufRHOR, rd_h_pos - 8, rd_v_pos + 4, R.select<8,1,16,1>(8,0));
    read(ibufGHOR, rd_h_pos - 8, rd_v_pos - 4, G.select<8,1,16,1>(0,0));
    read(ibufGHOR, rd_h_pos - 8, rd_v_pos + 4, G.select<8,1,16,1>(8,0));
    read(ibufBHOR, rd_h_pos - 8, rd_v_pos - 4, B.select<8,1,16,1>(0,0));
    read(ibufBHOR, rd_h_pos - 8, rd_v_pos + 4, B.select<8,1,16,1>(8,0));

    matrix<short, 16, 16> tmp = R;
    matrix_ref<short, 16, 16> DRG = R; DRG -= G  ; // R gone
    matrix_ref<short, 16, 16> DGB = G; DGB -= B  ; // G gone
    matrix_ref<short, 16, 16> DBR = B; DBR -= tmp; // B gone
    //-------------------------------------------------------------------------------------
    matrix_ref<short, 8, 16> HDSUM_H = tmp.select<8,1,16,1>(0,0);
    //matrix<int, 8, 16> HDSUM_H = tmp.select<8,1,16,1>(0,0);
#ifdef CMRT_EMU
    HDSUM_H.select<8,1,15,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<8,1,15,1>(4,1), - DRG.select<8,1,15,1>(4,0)));
    HDSUM_H.select<8,1,15,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<8,1,15,1>(4,1), - DGB.select<8,1,15,1>(4,0)));
    HDSUM_H.select<8,1,15,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<8,1,15,1>(4,1), - DBR.select<8,1,15,1>(4,0)));
#else
    HDSUM_H.select<8,1,16,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<8,1,16,1>(4,1), - DRG.select<8,1,16,1>(4,0)));
    HDSUM_H.select<8,1,16,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<8,1,16,1>(4,1), - DGB.select<8,1,16,1>(4,0)));
    HDSUM_H.select<8,1,16,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<8,1,16,1>(4,1), - DBR.select<8,1,16,1>(4,0)));
#endif
    matrix_ref<short, 8, 8> HLSAD = tmp.select<4,1,16,1>(8 ,0).format<short, 8, 8>();//HDSUM_H.select<8,1,8,1>(0,0);
    matrix_ref<short, 8, 8> HRSAD = tmp.select<4,1,16,1>(12,0).format<short, 8, 8>();//HDSUM_H.select<8,1,8,1>(0,4);
    //matrix_ref<int, 8, 8> HLSAD = tmp.select<4,1,16,1>(8 ,0).format<int, 8, 8>();//HDSUM_H.select<8,1,8,1>(0,0);
    //matrix_ref<int, 8, 8> HRSAD = tmp.select<4,1,16,1>(12,0).format<int, 8, 8>();//HDSUM_H.select<8,1,8,1>(0,4);

    HLSAD  = HDSUM_H.select<8,1,8,1>(0,0);
    HLSAD += HDSUM_H.select<8,1,8,1>(0,1);
    HLSAD += HDSUM_H.select<8,1,8,1>(0,2);
    HLSAD += HDSUM_H.select<8,1,8,1>(0,3);

    HRSAD  = HDSUM_H.select<8,1,8,1>(0,4);
    HRSAD += HDSUM_H.select<8,1,8,1>(0,5);
    HRSAD += HDSUM_H.select<8,1,8,1>(0,6);
    HRSAD += HDSUM_H.select<8,1,8,1>(0,7);

    matrix<short, 8, 8> Min_cost_H1 = cm_min<short>(HLSAD, HRSAD);
    //matrix<int, 8, 8> Min_cost_H1 = cm_min<int>(HLSAD, HRSAD);
    //-------------------------------------------------------------------------------------
    matrix_ref<short, 16, 8> VDSUM_H = HDSUM_H.format<short, 16, 8>();
    //matrix_ref<int, 16, 8> VDSUM_H = HDSUM_H.format<int, 16, 8>();
#ifdef CMRT_EMU
    VDSUM_H.select<15,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<15,1,8,1>(1,4), - DRG.select<15,1,8,1>(0,4)));
    VDSUM_H.select<15,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<15,1,8,1>(1,4), - DGB.select<15,1,8,1>(0,4)));
    VDSUM_H.select<15,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<15,1,8,1>(1,4), - DBR.select<15,1,8,1>(0,4)));
#else
    VDSUM_H.select<16,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<16,1,8,1>(1,4), - DRG.select<16,1,8,1>(0,4)));
    VDSUM_H.select<16,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<16,1,8,1>(1,4), - DGB.select<16,1,8,1>(0,4)));
    VDSUM_H.select<16,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<16,1,8,1>(1,4), - DBR.select<16,1,8,1>(0,4)));
#endif

    matrix_ref<short, 8, 8> HUSAD = HLSAD;
    //matrix_ref<int, 8, 8> HUSAD = HLSAD;
    HUSAD  = VDSUM_H.select<8,1,8,1>(0,0);
    HUSAD += VDSUM_H.select<8,1,8,1>(1,0);
    HUSAD += VDSUM_H.select<8,1,8,1>(2,0);
    HUSAD += VDSUM_H.select<8,1,8,1>(3,0);

    matrix_ref<short, 8, 8> HDSAD = HRSAD;
    //matrix_ref<int, 8, 8> HDSAD = HRSAD;
    HDSAD  = VDSUM_H.select<8,1,8,1>(4,0);
    HDSAD += VDSUM_H.select<8,1,8,1>(5,0);
    HDSAD += VDSUM_H.select<8,1,8,1>(6,0);
    HDSAD += VDSUM_H.select<8,1,8,1>(7,0);

    matrix<short, 8, 8> Min_cost_H2 = cm_min<short>(HUSAD, HDSAD);
    matrix<short, 8, 8> Min_cost_H;
    //matrix<int, 8, 8> Min_cost_H2 = cm_min<int>(HUSAD, HDSAD);
    //matrix<int, 8, 8> Min_cost_H;
    Min_cost_H  = Min_cost_H2 >> 1;
    Min_cost_H += Min_cost_H1;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////    PART II: COMPUTE MIN_COST_V  ////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    read(ibufRVER, rd_h_pos - 8, rd_v_pos - 4, R.select<8,1,16,1>(0,0));
    read(ibufRVER, rd_h_pos - 8, rd_v_pos + 4, R.select<8,1,16,1>(8,0));
    read(ibufGVER, rd_h_pos - 8, rd_v_pos - 4, G.select<8,1,16,1>(0,0));
    read(ibufGVER, rd_h_pos - 8, rd_v_pos + 4, G.select<8,1,16,1>(8,0));
    read(ibufBVER, rd_h_pos - 8, rd_v_pos - 4, B.select<8,1,16,1>(0,0));
    read(ibufBVER, rd_h_pos - 8, rd_v_pos + 4, B.select<8,1,16,1>(8,0));

    tmp = R;
    DRG = R - G;
    DGB = G - B;
    DBR = B - tmp;

    matrix_ref<short, 8, 16> HDSUM_V = HDSUM_H;
    //matrix_ref<int, 8, 16> HDSUM_V = HDSUM_H;
#ifdef CMRT_EMU
    HDSUM_V.select<8,1,15,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<8,1,15,1>(4,1), - DRG.select<8,1,15,1>(4,0)));
    HDSUM_V.select<8,1,15,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<8,1,15,1>(4,1), - DGB.select<8,1,15,1>(4,0)));
    HDSUM_V.select<8,1,15,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<8,1,15,1>(4,1), - DBR.select<8,1,15,1>(4,0)));
#else
    HDSUM_V.select<8,1,16,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<8,1,16,1>(4,1), - DRG.select<8,1,16,1>(4,0)));
    HDSUM_V.select<8,1,16,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<8,1,16,1>(4,1), - DGB.select<8,1,16,1>(4,0)));
    HDSUM_V.select<8,1,16,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<8,1,16,1>(4,1), - DBR.select<8,1,16,1>(4,0)));
#endif

    matrix_ref<short, 8, 8> VLSAD = HLSAD;
    //matrix_ref<int, 8, 8> VLSAD = HLSAD;
    VLSAD  = HDSUM_V.select<8,1,8,1>(0,0);
    VLSAD += HDSUM_V.select<8,1,8,1>(0,1);
    VLSAD += HDSUM_V.select<8,1,8,1>(0,2);
    VLSAD += HDSUM_V.select<8,1,8,1>(0,3);

    matrix_ref<short, 8, 8> VRSAD = HRSAD;
    //matrix_ref<int, 8, 8> VRSAD = HRSAD;
    VRSAD  = HDSUM_V.select<8,1,8,1>(0,4);
    VRSAD += HDSUM_V.select<8,1,8,1>(0,5);
    VRSAD += HDSUM_V.select<8,1,8,1>(0,6);
    VRSAD += HDSUM_V.select<8,1,8,1>(0,7);

    matrix_ref<short, 8, 8> Min_cost_V2 = Min_cost_H2;
    //matrix_ref<int, 8, 8> Min_cost_V2 = Min_cost_H2;
    Min_cost_V2 = cm_min<short>(VLSAD, VRSAD);
    //Min_cost_V2 = cm_min<int>(VLSAD, VRSAD);

    //-------------------------------------------------------------------------------------
    matrix_ref<short, 16, 8> VDSUM_V = HDSUM_H.format<short, 16, 8>();
    //matrix_ref<int, 16, 8> VDSUM_V = HDSUM_H.format<int, 16, 8>();
#ifdef CMRT_EMU
    VDSUM_V.select<15,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<15,1,8,1>(1,4), - DRG.select<15,1,8,1>(0,4)));
    VDSUM_V.select<15,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<15,1,8,1>(1,4), - DGB.select<15,1,8,1>(0,4)));
    VDSUM_V.select<15,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<15,1,8,1>(1,4), - DBR.select<15,1,8,1>(0,4)));
#else
    VDSUM_V.select<16,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<16,1,8,1>(1,4), - DRG.select<16,1,8,1>(0,4)));
    VDSUM_V.select<16,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<16,1,8,1>(1,4), - DGB.select<16,1,8,1>(0,4)));
    VDSUM_V.select<16,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<16,1,8,1>(1,4), - DBR.select<16,1,8,1>(0,4)));
#endif

    matrix_ref<short, 8, 8> VUSAD = HLSAD;
    //matrix_ref<int, 8, 8> VUSAD = HLSAD;
    VUSAD  = VDSUM_V.select<8,1,8,1>(0,0);
    VUSAD += VDSUM_V.select<8,1,8,1>(1,0);
    VUSAD += VDSUM_V.select<8,1,8,1>(2,0);
    VUSAD += VDSUM_V.select<8,1,8,1>(3,0);

    matrix_ref<short, 8, 8> VDSAD = HRSAD;
    //matrix_ref<int, 8, 8> VDSAD = HRSAD;
    VDSAD  = VDSUM_V.select<8,1,8,1>(4,0);
    VDSAD += VDSUM_V.select<8,1,8,1>(5,0);
    VDSAD += VDSUM_V.select<8,1,8,1>(6,0);
    VDSAD += VDSUM_V.select<8,1,8,1>(7,0);

    matrix_ref<short, 8, 8> Min_cost_V1 = Min_cost_H1;
    Min_cost_V1 = cm_min<short>(VUSAD, VDSAD);
    //matrix_ref<int, 8, 8> Min_cost_V1 = Min_cost_H1;
    //Min_cost_V1 = cm_min<int>(VUSAD, VDSAD);

    matrix<short, 8, 8> Min_cost_V;
    //matrix<int, 8, 8> Min_cost_V;
    Min_cost_V  = Min_cost_V2 >> 1;
    Min_cost_V += Min_cost_V1;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////// PART III: COMPUTE FLAG(MIN_COST_H, MIN_COST_V)   ///////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    matrix<uchar, 8, 8> HV_Flag;
    HV_Flag.merge(1, 0, Min_cost_H < Min_cost_V);
    //HV_Flag = Min_cost_V & 0x8000;

    int wr_h_pos = rd_h_pos >> 1;//(get_thread_origin_x() * 8);
    int wr_v_pos = rd_v_pos;//(get_thread_origin_y() * 8);
    write(obufHV, wr_h_pos, wr_v_pos, HV_Flag);
}
*/

// New SAD combined with Confidence Check
extern "C" _GENX_MAIN_ void
SAD(SurfaceIndex ibufRHOR, SurfaceIndex ibufGHOR, SurfaceIndex ibufBHOR,
    SurfaceIndex ibufRVER, SurfaceIndex ibufGVER, SurfaceIndex ibufBVER,
    //SurfaceIndex obufHV,
    int x , int y,
    SurfaceIndex R_c , SurfaceIndex G_c , SurfaceIndex B_c)
{
    int rd_h_pos = ((get_thread_origin_x() + x) * 8) * sizeof(short);
    int rd_v_pos = ((get_thread_origin_y() + y) * 8);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////    PART I: COMPUTE MIN_COST_H   ////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    matrix<short, 16, 16> R, G, B;
    read(ibufRHOR, rd_h_pos - 8, rd_v_pos - 4, R.select<8,1,16,1>(0,0));
    read(ibufRHOR, rd_h_pos - 8, rd_v_pos + 4, R.select<8,1,16,1>(8,0));
    read(ibufGHOR, rd_h_pos - 8, rd_v_pos - 4, G.select<8,1,16,1>(0,0));
    read(ibufGHOR, rd_h_pos - 8, rd_v_pos + 4, G.select<8,1,16,1>(8,0));

    matrix<short, 8, 8> R_H, G_H, B_H;
    matrix<short, 16, 16> tmp = R;
    matrix_ref<short, 16, 16> DRG = R; R_H = R.select<8,1,8,1>(4,4); DRG -= G  ; // R gone

    matrix_ref<short, 16, 16> DGB = G; G_H = G.select<8,1,8,1>(4,4);

    read(ibufBHOR, rd_h_pos - 8, rd_v_pos - 4, B.select<8,1,16,1>(0,0));
    read(ibufBHOR, rd_h_pos - 8, rd_v_pos + 4, B.select<8,1,16,1>(8,0));

    DGB -= B  ; // G gone
    matrix_ref<short, 16, 16> DBR = B; B_H = B.select<8,1,8,1>(4,4); DBR -= tmp; // B gone


    //-------------------------------------------------------------------------------------
    matrix_ref<short, 8, 16> HDSUM_H = tmp.select<8,1,16,1>(0,0);
    //matrix<int, 8, 16> HDSUM_H = tmp.select<8,1,16,1>(0,0);
#ifdef CMRT_EMU
    HDSUM_H.select<8,1,15,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<8,1,15,1>(4,1), - DRG.select<8,1,15,1>(4,0)));
    HDSUM_H.select<8,1,15,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<8,1,15,1>(4,1), - DGB.select<8,1,15,1>(4,0)));
    HDSUM_H.select<8,1,15,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<8,1,15,1>(4,1), - DBR.select<8,1,15,1>(4,0)));
#else
    HDSUM_H.select<8,1,16,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<8,1,16,1>(4,1), - DRG.select<8,1,16,1>(4,0)));
    HDSUM_H.select<8,1,16,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<8,1,16,1>(4,1), - DGB.select<8,1,16,1>(4,0)));
    HDSUM_H.select<8,1,16,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<8,1,16,1>(4,1), - DBR.select<8,1,16,1>(4,0)));
#endif
    matrix_ref<short, 8, 8> HLSAD = tmp.select<4,1,16,1>(8 ,0).format<short, 8, 8>();//HDSUM_H.select<8,1,8,1>(0,0);
    matrix_ref<short, 8, 8> HRSAD = tmp.select<4,1,16,1>(12,0).format<short, 8, 8>();//HDSUM_H.select<8,1,8,1>(0,4);
    //matrix_ref<int, 8, 8> HLSAD = tmp.select<4,1,16,1>(8 ,0).format<int, 8, 8>();//HDSUM_H.select<8,1,8,1>(0,0);
    //matrix_ref<int, 8, 8> HRSAD = tmp.select<4,1,16,1>(12,0).format<int, 8, 8>();//HDSUM_H.select<8,1,8,1>(0,4);

    HLSAD  = HDSUM_H.select<8,1,8,1>(0,0);
    HLSAD += HDSUM_H.select<8,1,8,1>(0,1);
    HLSAD += HDSUM_H.select<8,1,8,1>(0,2);
    HLSAD += HDSUM_H.select<8,1,8,1>(0,3);

    HRSAD  = HDSUM_H.select<8,1,8,1>(0,4);
    HRSAD += HDSUM_H.select<8,1,8,1>(0,5);
    HRSAD += HDSUM_H.select<8,1,8,1>(0,6);
    HRSAD += HDSUM_H.select<8,1,8,1>(0,7);

    matrix<short, 8, 8> Min_cost_H1 = cm_min<short>(HLSAD, HRSAD);
    //matrix<int, 8, 8> Min_cost_H1 = cm_min<int>(HLSAD, HRSAD);
    //-------------------------------------------------------------------------------------
    matrix_ref<short, 16, 8> VDSUM_H = HDSUM_H.format<short, 16, 8>();
    //matrix_ref<int, 16, 8> VDSUM_H = HDSUM_H.format<int, 16, 8>();
#ifdef CMRT_EMU
    VDSUM_H.select<15,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<15,1,8,1>(1,4), - DRG.select<15,1,8,1>(0,4)));
    VDSUM_H.select<15,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<15,1,8,1>(1,4), - DGB.select<15,1,8,1>(0,4)));
    VDSUM_H.select<15,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<15,1,8,1>(1,4), - DBR.select<15,1,8,1>(0,4)));
#else
    VDSUM_H.select<16,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<16,1,8,1>(1,4), - DRG.select<16,1,8,1>(0,4)));
    VDSUM_H.select<16,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<16,1,8,1>(1,4), - DGB.select<16,1,8,1>(0,4)));
    VDSUM_H.select<16,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<16,1,8,1>(1,4), - DBR.select<16,1,8,1>(0,4)));
#endif

    matrix_ref<short, 8, 8> HUSAD = HLSAD;
    //matrix_ref<int, 8, 8> HUSAD = HLSAD;
    HUSAD  = VDSUM_H.select<8,1,8,1>(0,0);
    HUSAD += VDSUM_H.select<8,1,8,1>(1,0);
    HUSAD += VDSUM_H.select<8,1,8,1>(2,0);
    HUSAD += VDSUM_H.select<8,1,8,1>(3,0);

    matrix_ref<short, 8, 8> HDSAD = HRSAD;
    //matrix_ref<int, 8, 8> HDSAD = HRSAD;
    HDSAD  = VDSUM_H.select<8,1,8,1>(4,0);
    HDSAD += VDSUM_H.select<8,1,8,1>(5,0);
    HDSAD += VDSUM_H.select<8,1,8,1>(6,0);
    HDSAD += VDSUM_H.select<8,1,8,1>(7,0);

    matrix<short, 8, 8> Min_cost_H2 = cm_min<short>(HUSAD, HDSAD);
    matrix_ref<short, 8, 8> Min_cost_H = Min_cost_H2;
    //matrix<int, 8, 8> Min_cost_H2 = cm_min<int>(HUSAD, HDSAD);
    //matrix<int, 8, 8> Min_cost_H;
    Min_cost_H  = Min_cost_H2 >> 1;
    Min_cost_H += Min_cost_H1;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////    PART II: COMPUTE MIN_COST_V  ////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    read(ibufRVER, rd_h_pos - 8, rd_v_pos - 4, R.select<8,1,16,1>(0,0));
    read(ibufRVER, rd_h_pos - 8, rd_v_pos + 4, R.select<8,1,16,1>(8,0));
    read(ibufGVER, rd_h_pos - 8, rd_v_pos - 4, G.select<8,1,16,1>(0,0));
    read(ibufGVER, rd_h_pos - 8, rd_v_pos + 4, G.select<8,1,16,1>(8,0));
    read(ibufBVER, rd_h_pos - 8, rd_v_pos - 4, B.select<8,1,16,1>(0,0));
    read(ibufBVER, rd_h_pos - 8, rd_v_pos + 4, B.select<8,1,16,1>(8,0));

    matrix<short, 8, 8> R_V, G_V, B_V;

    // bug fix on 8/16/2013, by Samuel
    //tmp = R;
    //R_V = R.select<8,1,8,1>(4,4); DRG = R - G;
    //G_V = G.select<8,1,8,1>(4,4); DGB = G - B;
    //B_V = B.select<8,1,8,1>(4,4); DBR = B - tmp;

    tmp = R;
    DRG = R; R_V = R.select<8,1,8,1>(4,4); DRG -= G;
    DGB = G; G_V = G.select<8,1,8,1>(4,4); DGB -= B;
    DBR = B; B_V = B.select<8,1,8,1>(4,4); DBR -= tmp;


    matrix_ref<short, 8, 16> HDSUM_V = HDSUM_H;
    //matrix_ref<int, 8, 16> HDSUM_V = HDSUM_H;
#ifdef CMRT_EMU
    HDSUM_V.select<8,1,15,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<8,1,15,1>(4,1), - DRG.select<8,1,15,1>(4,0)));
    HDSUM_V.select<8,1,15,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<8,1,15,1>(4,1), - DGB.select<8,1,15,1>(4,0)));
    HDSUM_V.select<8,1,15,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<8,1,15,1>(4,1), - DBR.select<8,1,15,1>(4,0)));
#else
    HDSUM_V.select<8,1,16,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<8,1,16,1>(4,1), - DRG.select<8,1,16,1>(4,0)));
    HDSUM_V.select<8,1,16,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<8,1,16,1>(4,1), - DGB.select<8,1,16,1>(4,0)));
    HDSUM_V.select<8,1,16,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<8,1,16,1>(4,1), - DBR.select<8,1,16,1>(4,0)));
#endif

    matrix_ref<short, 8, 8> VLSAD = HLSAD;
    //matrix_ref<int, 8, 8> VLSAD = HLSAD;
    VLSAD  = HDSUM_V.select<8,1,8,1>(0,0);
    VLSAD += HDSUM_V.select<8,1,8,1>(0,1);
    VLSAD += HDSUM_V.select<8,1,8,1>(0,2);
    VLSAD += HDSUM_V.select<8,1,8,1>(0,3);

    matrix_ref<short, 8, 8> VRSAD = HRSAD;
    //matrix_ref<int, 8, 8> VRSAD = HRSAD;
    VRSAD  = HDSUM_V.select<8,1,8,1>(0,4);
    VRSAD += HDSUM_V.select<8,1,8,1>(0,5);
    VRSAD += HDSUM_V.select<8,1,8,1>(0,6);
    VRSAD += HDSUM_V.select<8,1,8,1>(0,7);

    // bug fix on 8/16/2013, by Samuel
    matrix<short, 8, 8> Min_cost_V2;// = Min_cost_H2;
    //matrix_ref<int, 8, 8> Min_cost_V2 = Min_cost_H2;
    Min_cost_V2 = cm_min<short>(VLSAD, VRSAD);
    //Min_cost_V2 = cm_min<int>(VLSAD, VRSAD);

    //-------------------------------------------------------------------------------------
    matrix_ref<short, 16, 8> VDSUM_V = HDSUM_H.format<short, 16, 8>();
    //matrix_ref<int, 16, 8> VDSUM_V = HDSUM_H.format<int, 16, 8>();
#ifdef CMRT_EMU
    VDSUM_V.select<15,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<15,1,8,1>(1,4), - DRG.select<15,1,8,1>(0,4)));
    VDSUM_V.select<15,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<15,1,8,1>(1,4), - DGB.select<15,1,8,1>(0,4)));
    VDSUM_V.select<15,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<15,1,8,1>(1,4), - DBR.select<15,1,8,1>(0,4)));
#else
    VDSUM_V.select<16,1,8,1>(0,0)  = cm_abs<ushort>(cm_add<short>(DRG.select<16,1,8,1>(1,4), - DRG.select<16,1,8,1>(0,4)));
    VDSUM_V.select<16,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DGB.select<16,1,8,1>(1,4), - DGB.select<16,1,8,1>(0,4)));
    VDSUM_V.select<16,1,8,1>(0,0) += cm_abs<ushort>(cm_add<short>(DBR.select<16,1,8,1>(1,4), - DBR.select<16,1,8,1>(0,4)));
#endif

    matrix_ref<short, 8, 8> VUSAD = HLSAD;
    //matrix_ref<int, 8, 8> VUSAD = HLSAD;
    VUSAD  = VDSUM_V.select<8,1,8,1>(0,0);
    VUSAD += VDSUM_V.select<8,1,8,1>(1,0);
    VUSAD += VDSUM_V.select<8,1,8,1>(2,0);
    VUSAD += VDSUM_V.select<8,1,8,1>(3,0);

    matrix_ref<short, 8, 8> VDSAD = HRSAD;
    //matrix_ref<int, 8, 8> VDSAD = HRSAD;
    VDSAD  = VDSUM_V.select<8,1,8,1>(4,0);
    VDSAD += VDSUM_V.select<8,1,8,1>(5,0);
    VDSAD += VDSUM_V.select<8,1,8,1>(6,0);
    VDSAD += VDSUM_V.select<8,1,8,1>(7,0);

    matrix_ref<short, 8, 8> Min_cost_V1 = Min_cost_H1;
    Min_cost_V1 = cm_min<short>(VUSAD, VDSAD);
    //matrix_ref<int, 8, 8> Min_cost_V1 = Min_cost_H1;
    //Min_cost_V1 = cm_min<int>(VUSAD, VDSAD);

    matrix_ref<short, 8, 8> Min_cost_V = Min_cost_V2;
    //matrix<int, 8, 8> Min_cost_V;
    Min_cost_V  = Min_cost_V2 >> 1;
    Min_cost_V += Min_cost_V1;

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////// PART III: COMPUTE FLAG(MIN_COST_H, MIN_COST_V)   ///////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    matrix<uchar, 8, 8> HV_Flag;
    HV_Flag.merge(1, 0, Min_cost_H < Min_cost_V);
    //HV_Flag = Min_cost_V & 0x8000;

    //int wr_h_pos = rd_h_pos >> 1;//(get_thread_origin_x() * 8);
    //int wr_v_pos = rd_v_pos;//(get_thread_origin_y() * 8);
    //write(obufHV, wr_h_pos, wr_v_pos, HV_Flag);

    int wr_h_pos = ((get_thread_origin_x() + x) * 8) * sizeof(short);
    int wr_v_pos = ((get_thread_origin_y() + y) * 8);

    wr_h_pos -= 8 * sizeof(short);
    wr_v_pos -= 8;

    if ( wr_h_pos>=0 && wr_v_pos>=0 ){

    matrix_ref<short, 8, 8> out_R = Min_cost_H1;
    out_R.merge(R_H, R_V, (HV_Flag  == 1));
    write(R_c, wr_h_pos, wr_v_pos, out_R);

    matrix_ref<short, 8, 8> out_G = Min_cost_H1;
    out_G.merge(G_H, G_V, (HV_Flag  == 1));
    write(G_c, wr_h_pos, wr_v_pos, out_G);

    //matrix_ref<short, 8, 8> out_B = Min_cost_H1;
    //out_B.merge(B_V.select<8,1,8,1>(4,4), B.select<8,1,8,1>(4,4), (HV_Flag  == 1));

    matrix_ref<short, 8, 8> out_B = Min_cost_H1;
    out_B.merge(B_H, B_V, (HV_Flag  == 1));
    write(B_c, wr_h_pos, wr_v_pos, out_B);

    }
}



#define AVG_False 1
const unsigned short idx_init[16] = {1,1,2,2,5,5,6,6,9,9,10,10,13,13,14,14};
extern "C" _GENX_ void inline
HORIZONTAL_CHECK(matrix_ref<short, 13, 32> srcG,
                 matrix_ref<ushort, 8, 16> maskH)
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

        index = idx + 1;
        v.row(1) = rowG.iselect(index);
        diff.row(0) = v.row(0) - v.row(1);

        index = idx + 2;
        v.row(2) = rowG.iselect(index);
        diff.row(1) = v.row(1) - v.row(2);

        index = idx + 3;
        v.row(3) = rowG.iselect(index);
        diff.row(2) = v.row(2) - v.row(3);

        index = idx + 4;
        v.row(4) = rowG.iselect(index);
        diff.row(3) = v.row(3) - v.row(4);
#else
        v.select<1,1,8,2>(0,0) = m_rowG.genx_select<4,4,2,1>(0,1);
        v.select<1,1,8,2>(0,1) = v.select<1,1,8,2>(0,0);

#pragma unroll
        for(int i = 1; i < 5; i++)
        {
            v.select<1,1,8,2>(i,0) = m_rowG.genx_select<4,4,2,1>(0,i+1);
            v.select<1,1,8,2>(i,1) = v.select<1,1,8,2>(i,0);

            diff.row(i-1) = v.row(i-1) - v.row(i);
        }
#endif

        maskH.row(j)  = (cm_abs<ushort>(diff.row(3)) > 0);//Grn_imbalance_th);
#pragma unroll
        for(int i = 0; i < 3; i++)
        {
            maskH.row(j) &= (cm_mul<int> (diff.row(i),diff.row(i+1)) < 0);
            maskH.row(j) &= (cm_abs<ushort>(diff.row(i)) > 0);//Grn_imbalance_th);
        }
    }
}

extern "C" _GENX_ void inline
VERTICAL_CHECK(matrix_ref<short, 13, 16> cenG,
               matrix_ref<ushort, 8, 16> maskV)
{
    matrix< short, 4, 16 >   diff;
    //{0,1,4,5} row
#pragma unroll
    for(int j = 0; j <= 4; j+=4 )
    {
        diff          = (cenG.select<4,1,16,1>(1+j,0) - cenG.select<4,1,16,1>(2+j,0));

        maskV.row(j)  = (cm_abs<ushort>(diff.row(3)) > 0);//Grn_imbalance_th);
#pragma unroll
        for(int i = 0; i < 3; i++)
        {
            maskV.row(j) &= (cm_mul<int> (diff.row(i),diff.row(i+1)) < 0);
            maskV.row(j) &= (cm_abs<ushort>(diff.row(i)) > 0);//Grn_imbalance_th);
        }

        maskV.row(j+1)  = maskV.row(j);
    }

    //{2,3,6,7} row
#pragma unroll
    for(int j = 2; j <= 6; j+=4 )
    {
        diff          = (cenG.select<4,1,16,1>(  j,0) - cenG.select<4,1,16,1>(1+j,0));

        maskV.row(j)  = (cm_abs<ushort>(diff.row(3)) > 0);//Grn_imbalance_th);
#pragma unroll
        for(int i = 0; i < 3; i++)
        {
            maskV.row(j) &= (cm_mul<int> (diff.row(i),diff.row(i+1)) < 0);
            maskV.row(j) &= (cm_abs<ushort>(diff.row(i)) > 0);//Grn_imbalance_th);
        }
        maskV.row(j+1) = maskV.row(j);
    }
}

// Old DecideAVG
/*
#if 1
extern "C" _GENX_MAIN_ void
DECIDE_AVG(SurfaceIndex ibufRAVG,
           SurfaceIndex ibufGAVG,
           SurfaceIndex ibufBAVG,
           SurfaceIndex ibufAVG_Flag,
           SurfaceIndex obufAVG_UpdatedFlag )
{
    int Grn_imbalance_th = 0;
    int Average_Color_TH  = 100;
    //int good_intensity_TH = 100

    matrix< short, 8, 16 >   srcR;
    matrix< short, 8, 16 >   srcB;
    matrix< short, 13,32 >   srcG;
    matrix< short, 4, 16 >   diff;
    vector<ushort, 16    >   idx(idx_init);

    matrix< uchar,16, 16 >   AVG_flag;
    matrix<ushort, 8, 16 >   mask0;
    matrix<ushort, 8, 16 >   maskH0;
    matrix<ushort, 8, 16 >   maskV0;

    matrix<ushort, 8, 16 >   mask1;
    matrix<ushort, 8, 16 >   maskH1;
    matrix<ushort, 8, 16 >   maskV1;

    int rd_h_pos = (get_thread_origin_x() * 16 - 2) * sizeof(short);
    int rd_v_pos = (get_thread_origin_y() * 16 - 2);//(get_thread_origin_y() * 8  - 2);

    int dum;
    if(rd_h_pos == 1916 && rd_v_pos == -2)
        dum = 1;

    read(ibufGAVG, rd_h_pos   , rd_v_pos  , srcG.select<8,1,16,1>(0,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos  , srcG.select<8,1,16,1>(0,16));
    read(ibufGAVG, rd_h_pos   , rd_v_pos+8, srcG.select<5,1,16,1>(8,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos+8, srcG.select<5,1,16,1>(8,16));

    matrix<short, 13, 16> cenG;
    cenG = srcG.select<13,1,16,1>(0,2);

    //Horizonal check (8-bit differences)
    HORIZONTAL_CHECK(srcG, maskH0);

    //Vertical check (8-bit differences)
       VERTICAL_CHECK  (cenG, maskV0);

    //The second condition is to check color difference between two colors in a pixel (8-bit diff):
    read(ibufRAVG, rd_h_pos+4 , rd_v_pos+2, srcR);
    read(ibufBAVG, rd_h_pos+4 , rd_v_pos+2, srcB);

    matrix<short, 8, 16> absv;
    matrix<short, 8, 16> difference;

    difference = srcR - cenG.select<8,1,16,1>(2,0);
    absv = cm_abs<ushort>(difference);
    mask0 = (absv > Average_Color_TH);

    difference = cenG.select<8,1,16,1>(2,0) - srcB;
    absv = cm_abs<ushort>(difference);
    mask0 |= (absv > Average_Color_TH);

    difference = srcB - srcR;
    absv = cm_abs<ushort>(difference);
    mask0 |= (absv > Average_Color_TH);

    mask0 |= maskH0;
    mask0 |= maskV0;

    ///////////// SECOND 16x8 BLOCK  ///////////////////////////////////////
    rd_v_pos += 8;

    read(ibufGAVG, rd_h_pos   , rd_v_pos , srcG.select<8,1,16,1>(0,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos , srcG.select<8,1,16,1>(0,16));
    read(ibufGAVG, rd_h_pos   , rd_v_pos+8, srcG.select<5,1,16,1>(8,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos+8, srcG.select<5,1,16,1>(8,16));

    cenG = srcG.select<13,1,16,1>(0,2);

    //Horizonal check (8-bit differences)
    HORIZONTAL_CHECK(srcG, maskH1);

    //Vertical check (8-bit differences)
       VERTICAL_CHECK  (cenG, maskV1);

    read(ibufRAVG, rd_h_pos+4 , rd_v_pos+2, srcR);
    read(ibufBAVG, rd_h_pos+4 , rd_v_pos+2, srcB);

    difference = srcR - cenG.select<8,1,16,1>(2,0);
    absv = cm_abs<ushort>(difference);
    mask1 = (absv > Average_Color_TH);

    difference = cenG.select<8,1,16,1>(2,0) - srcB;
    absv = cm_abs<ushort>(difference);
    mask1 |= (absv > Average_Color_TH);

    difference = srcB - srcR;
    absv = cm_abs<ushort>(difference);
    mask1 |= (absv > Average_Color_TH);

    mask1 |= maskH1;
    mask1 |= maskV1;


    ///////////////////////////////////////////////////////////
    int rd_x_pos = (get_thread_origin_x() * 16 );
    int rd_y_pos = (get_thread_origin_y() * 16 );
    read(ibufAVG_Flag, rd_x_pos, rd_y_pos, AVG_flag);
    AVG_flag.select<8,1,16,1>(0,0).merge(1, mask0);
    AVG_flag.select<8,1,16,1>(8,0).merge(1, mask1);

    ///////////////////////////////////////////////////////////
    int wr_h_pos = (get_thread_origin_x() * 16 );
    int wr_v_pos = (get_thread_origin_y() * 16 );

    write(obufAVG_UpdatedFlag, wr_h_pos, wr_v_pos, AVG_flag);
    //write(obufAVG_UpdatedFlag, wr_h_pos, wr_v_pos, AVG_flag.select<8,1,16,1>(0,0));
}
#else
extern "C" _GENX_MAIN_ void
DECIDE_AVG(SurfaceIndex ibufRAVG,
           SurfaceIndex ibufGAVG,
           SurfaceIndex ibufBAVG,
           SurfaceIndex ibufAVG_Flag,
           SurfaceIndex obufAVG_UpdatedFlag )
{
    int Grn_imbalance_th = 0;
    int Average_Color_TH  = 100;
    //int good_intensity_TH = 10;

    matrix< short, 8, 16 >   srcR;
    matrix< short, 13,32 >   srcG;
    matrix< short, 8, 16 >   srcB;
    matrix< short, 4, 16 >   diff;
    vector<ushort, 16    >   idx(idx_init);

    matrix< uchar, 8, 16 >   AVG_flag;
    matrix<ushort, 8, 16 >   mask;
    matrix<ushort, 8, 16 >   maskH;
    matrix<ushort, 8, 16 >   maskV;

    int rd_h_pos = (get_thread_origin_x() * 16 - 2) * sizeof(short);
    int rd_v_pos = (get_thread_origin_y() * 8  - 2);

    //int dum;
    //if(rd_h_pos == (40*8-2)*2 && rd_v_pos == (9*8-2))
     //    dum = 1;
    //for(int rd_v_pos = rd_ver_pos; rd_v_pos <= (rd_ver_pos + 8); rd_v_pos += 8 )
//    {

    read(ibufGAVG, rd_h_pos   , rd_v_pos  , srcG.select<8,1,16,1>(0,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos  , srcG.select<8,1,16,1>(0,16));
    read(ibufGAVG, rd_h_pos   , rd_v_pos+8, srcG.select<5,1,16,1>(8,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos+8, srcG.select<5,1,16,1>(8,16));

    matrix<short, 13, 16> cenG;
    cenG = srcG.select<13,1,16,1>(0,2);

    //Horizonal check (8-bit differences)
#pragma unroll
    for(int j = 0; j < 8; j++) // j == index of output registers
    {
        vector<short, 32> rowG = srcG.row(2+j);

        matrix_ref<short, 8, 4> m_rowG = rowG.format<short, 8, 4>();
        matrix<short, 5, 16> v;

#ifdef CMRT_EMU
        vector<ushort, 16> index;
        v.row(0) = rowG.iselect(idx);

        index = idx + 1;
        v.row(1) = rowG.iselect(index);
        diff.row(0) = v.row(0) - v.row(1);

        index = idx + 2;
        v.row(2) = rowG.iselect(index);
        diff.row(1) = v.row(1) - v.row(2);

        index = idx + 3;
        v.row(3) = rowG.iselect(index);
        diff.row(2) = v.row(2) - v.row(3);

        index = idx + 4;
        v.row(4) = rowG.iselect(index);
        diff.row(3) = v.row(3) - v.row(4);
#else
        v.select<1,1,8,2>(0,0) = m_rowG.genx_select<4,4,2,1>(0,1);
        v.select<1,1,8,2>(0,1) = v.select<1,1,8,2>(0,0);

#pragma unroll
        for(int i = 1; i < 5; i++)
        {
            v.select<1,1,8,2>(i,0) = m_rowG.genx_select<4,4,2,1>(0,i+1);
            v.select<1,1,8,2>(i,1) = v.select<1,1,8,2>(i,0);

            diff.row(i-1) = v.row(i-1) - v.row(i);
        }
#endif

        maskH.row(j)  = (cm_abs<ushort>(diff.row(3)) > Grn_imbalance_th);
#pragma unroll
        for(int i = 0; i < 3; i++)
        {
            maskH.row(j) &= (cm_mul<short> (diff.row(i),diff.row(i+1)) < 0);
            maskH.row(j) &= (cm_abs<ushort>(diff.row(i)) > Grn_imbalance_th);
        }
    }

    //Vertical check (8-bit differences)
    //{0,1,4,5} row
#pragma unroll
    for(int j = 0; j <= 4; j+=4 )
    {
        diff          = (cenG.select<4,1,16,1>(1+j,0) - cenG.select<4,1,16,1>(2+j,0));

        maskV.row(j)  = (cm_abs<ushort>(diff.row(3)) > Grn_imbalance_th);
#pragma unroll
        for(int i = 0; i < 3; i++)
        {
            maskV.row(j) &= (cm_mul<short> (diff.row(i),diff.row(i+1)) < 0);
            maskV.row(j) &= (cm_abs<ushort>(diff.row(i)) > Grn_imbalance_th);
        }

        maskV.row(j+1)  = maskV.row(j);
    }

    //{2,3,6,7} row
#pragma unroll
    for(int j = 2; j <= 6; j+=4 )
    {
        diff          = (cenG.select<4,1,16,1>(  j,0) - cenG.select<4,1,16,1>(1+j,0));

        maskV.row(j)  = (cm_abs<ushort>(diff.row(3)) > Grn_imbalance_th);
#pragma unroll
        for(int i = 0; i < 3; i++)
        {
            maskV.row(j) &= (cm_mul<short> (diff.row(i),diff.row(i+1)) < 0);
            maskV.row(j) &= (cm_abs<ushort>(diff.row(i)) > Grn_imbalance_th);
        }
        maskV.row(j+1) = maskV.row(j);
    }

    //The second condition is to check color difference between two colors in a pixel (8-bit diff):
    read(ibufRAVG, rd_h_pos+4 , rd_v_pos+2, srcR);
    read(ibufBAVG, rd_h_pos+4 , rd_v_pos+2, srcB);

    matrix<short, 8, 16> absv;
    matrix<short, 8, 16> difference;

    difference = srcR - cenG.select<8,1,16,1>(2,0);
    absv = cm_abs<ushort>(difference);
    mask  = (absv > Average_Color_TH);

    difference = cenG.select<8,1,16,1>(2,0) - srcB;
    absv = cm_abs<ushort>(difference);
    mask |= (absv > Average_Color_TH);

    difference = srcB - srcR;
    absv = cm_abs<ushort>(difference);
    mask |= (absv > Average_Color_TH);

    mask |= maskH;
    mask |= maskV;
    //AVG_flag.merge(1, 2, mask);
    int rd_x_pos = (get_thread_origin_x() * 16);
    int rd_y_pos = (get_thread_origin_y() * 8 );
    read(ibufAVG_Flag, rd_x_pos, rd_y_pos, AVG_flag);
    AVG_flag.merge(1, mask);

    ///////////////////////////////////////////////////////////
    int wr_h_pos = (get_thread_origin_x() * 16 );
    int wr_v_pos = (get_thread_origin_y() * 8  );

    write(obufAVG_UpdatedFlag, wr_h_pos, wr_v_pos, AVG_flag);
    //}
}
#endif
*/

// New Decide Avg combined with Confidence check
#if 1
extern "C" _GENX_MAIN_ void
DECIDE_AVG(SurfaceIndex ibufRAVG,
           SurfaceIndex ibufGAVG,
           SurfaceIndex ibufBAVG,
           SurfaceIndex ibufAVG_Flag,
           //SurfaceIndex obufAVG_UpdatedFlag )
           SurfaceIndex R_c , SurfaceIndex G_c , SurfaceIndex B_c)
{
    int Grn_imbalance_th = 0;
    int Average_Color_TH  = 100;
    //int good_intensity_TH = 100

    matrix< short, 8, 16 >   srcR;
    matrix< short, 8, 16 >   srcB;
    matrix< short, 13,32 >   srcG;
    matrix< short, 4, 16 >   diff;
    vector<ushort, 16    >   idx(idx_init);

    matrix< uchar,16, 16 >   AVG_flag;
    matrix<ushort, 8, 16 >   mask0;
    matrix<ushort, 8, 16 >   maskH0;
    matrix<ushort, 8, 16 >   maskV0;

    matrix<ushort, 8, 16 >   mask1;
    matrix<ushort, 8, 16 >   maskH1;
    matrix<ushort, 8, 16 >   maskV1;

    int rd_h_pos = (get_thread_origin_x() * 16 - 2) * sizeof(short);
    int rd_v_pos = (get_thread_origin_y() * 16 - 2);//(get_thread_origin_y() * 8  - 2);

    int dum;
    if(rd_h_pos == 1916 && rd_v_pos == -2)
        dum = 1;

    read(ibufGAVG, rd_h_pos   , rd_v_pos  , srcG.select<8,1,16,1>(0,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos  , srcG.select<8,1,16,1>(0,16));
    read(ibufGAVG, rd_h_pos   , rd_v_pos+8, srcG.select<5,1,16,1>(8,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos+8, srcG.select<5,1,16,1>(8,16));

    matrix<short, 13, 16> cenG;
    cenG = srcG.select<13,1,16,1>(0,2);

    //Horizonal check (8-bit differences)
    if(!(srcG == AVG_False).all())
        HORIZONTAL_CHECK(srcG, maskH0);
    else
        maskH0 = 0;
    //Vertical check (8-bit differences)
    if(!(cenG == AVG_False).all())
        VERTICAL_CHECK  (cenG, maskV0);
    else
        maskV0 = 0;
    //The second condition is to check color difference between two colors in a pixel (8-bit diff):
    read(ibufRAVG, rd_h_pos+4 , rd_v_pos+2, srcR);
    read(ibufBAVG, rd_h_pos+4 , rd_v_pos+2, srcB);

    matrix<short, 8, 16> absv;
    matrix<short, 8, 16> difference;

    difference = srcR - cenG.select<8,1,16,1>(2,0);
    absv = cm_abs<ushort>(difference);
    mask0 = (absv > Average_Color_TH);

    difference = cenG.select<8,1,16,1>(2,0) - srcB;
    absv = cm_abs<ushort>(difference);
    mask0 |= (absv > Average_Color_TH);

    difference = srcB - srcR;
    absv = cm_abs<ushort>(difference);
    mask0 |= (absv > Average_Color_TH);

    mask0 |= maskH0;
    mask0 |= maskV0;

    // bug fix on 8/16/2013, by Samuel, 3 lines added
    int rd_x_pos = (get_thread_origin_x() * 16 );
    int rd_y_pos = (get_thread_origin_y() * 16 );
    read(ibufAVG_Flag, rd_x_pos, rd_y_pos, AVG_flag);

    AVG_flag.select<8,1,16,1>(0,0).merge(1, mask0);

    int wr_h_pos = (get_thread_origin_x() * 16 ) * sizeof(short);
    int wr_v_pos = (get_thread_origin_y() * 16 );

    matrix<short, 8, 16> out;

    wr_h_pos -= 8 * sizeof(short);
    wr_v_pos -= 8;
    if ( wr_h_pos>=0 && wr_v_pos>=0 ){

    mask0 = (AVG_flag.select<8,1,16,1>(0,0) == AVG_True);

    if(mask0.any())
    {
        read(R_c,  wr_h_pos, wr_v_pos, out);
        out.merge(srcR, mask0);
        write(R_c, wr_h_pos, wr_v_pos, out);

        read(G_c,  wr_h_pos, wr_v_pos, out);
        out.merge(cenG.select<8,1,16,1>(2,0), mask0);
        write(G_c, wr_h_pos, wr_v_pos, out);

        read(B_c,  wr_h_pos, wr_v_pos, out);
        out.merge(srcB, mask0);
        write(B_c, wr_h_pos, wr_v_pos, out);
    }

    }

    ///////////// SECOND 16x8 BLOCK  ///////////////////////////////////////
    rd_v_pos += 8;

    read(ibufGAVG, rd_h_pos   , rd_v_pos , srcG.select<8,1,16,1>(0,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos , srcG.select<8,1,16,1>(0,16));
    read(ibufGAVG, rd_h_pos   , rd_v_pos+8, srcG.select<5,1,16,1>(8,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos+8, srcG.select<5,1,16,1>(8,16));

    cenG = srcG.select<13,1,16,1>(0,2);

    //Horizonal check (8-bit differences)
    if(!(srcG == AVG_False).all())
        HORIZONTAL_CHECK(srcG, maskH1);
    else
        maskH1 = 0;
    //Vertical check (8-bit differences)
       if(!(cenG == AVG_False).all())
        VERTICAL_CHECK  (cenG, maskV1);
    else
        maskV1 = 0;

    read(ibufRAVG, rd_h_pos+4 , rd_v_pos+2, srcR);
    read(ibufBAVG, rd_h_pos+4 , rd_v_pos+2, srcB);

    difference = srcR - cenG.select<8,1,16,1>(2,0);
    absv = cm_abs<ushort>(difference);
    mask1 = (absv > Average_Color_TH);

    difference = cenG.select<8,1,16,1>(2,0) - srcB;
    absv = cm_abs<ushort>(difference);
    mask1 |= (absv > Average_Color_TH);

    difference = srcB - srcR;
    absv = cm_abs<ushort>(difference);
    mask1 |= (absv > Average_Color_TH);

    mask1 |= maskH1;
    mask1 |= maskV1;


    ///////////////////////////////////////////////////////////
    // bug fix on 8/16/2013, by Samuel, commented out 3 lines
    //int rd_x_pos = (get_thread_origin_x() * 16 );
    //int rd_y_pos = (get_thread_origin_y() * 16 );
    //read(ibufAVG_Flag, rd_x_pos, rd_y_pos, AVG_flag);
    AVG_flag.select<8,1,16,1>(8,0).merge(1, mask1);

    ///////////////////////////////////////////////////////////
    //int wr_h_pos = (get_thread_origin_x() * 16 );
    //int wr_v_pos = (get_thread_origin_y() * 16 );

    //write(obufAVG_UpdatedFlag, wr_h_pos, wr_v_pos, AVG_flag);
    //write(obufAVG_UpdatedFlag, wr_h_pos, wr_v_pos, AVG_flag.select<8,1,16,1>(0,0));

    wr_h_pos = (get_thread_origin_x() * 16 ) * sizeof(short);
    wr_v_pos = (get_thread_origin_y() * 16 + 8);

    wr_h_pos -= 8 * sizeof(short);
    wr_v_pos -= 8;
    if ( wr_h_pos>=0 && wr_v_pos>=0 ){


    mask1 = (AVG_flag.select<8,1,16,1>(8,0) == AVG_True);

    if(mask1.any())
    {
        read(R_c,  wr_h_pos, wr_v_pos, out);
        out.merge(srcR, mask1);
        write(R_c, wr_h_pos, wr_v_pos, out);

        read(G_c,  wr_h_pos, wr_v_pos, out);
        out.merge(cenG.select<8,1,16,1>(2,0), mask1);
        write(G_c, wr_h_pos, wr_v_pos, out);

        read(B_c,  wr_h_pos, wr_v_pos, out);
        out.merge(srcB, mask1);
        write(B_c, wr_h_pos, wr_v_pos, out);
    }

    }
}
#else
// old one
extern "C" _GENX_MAIN_ void
DECIDE_AVG(SurfaceIndex ibufRAVG,
           SurfaceIndex ibufGAVG,
           SurfaceIndex ibufBAVG,
           SurfaceIndex ibufAVG_Flag,
           SurfaceIndex obufAVG_UpdatedFlag )
{
    int Grn_imbalance_th = 0;
    int Average_Color_TH  = 100;
    //int good_intensity_TH = 10;

    matrix< short, 8, 16 >   srcR;
    matrix< short, 13,32 >   srcG;
    matrix< short, 8, 16 >   srcB;
    matrix< short, 4, 16 >   diff;
    vector<ushort, 16    >   idx(idx_init);

    matrix< uchar, 8, 16 >   AVG_flag;
    matrix<ushort, 8, 16 >   mask;
    matrix<ushort, 8, 16 >   maskH;
    matrix<ushort, 8, 16 >   maskV;

    int rd_h_pos = (get_thread_origin_x() * 16 - 2) * sizeof(short);
    int rd_v_pos = (get_thread_origin_y() * 8  - 2);

    //int dum;
    //if(rd_h_pos == (40*8-2)*2 && rd_v_pos == (9*8-2))
     //    dum = 1;
    //for(int rd_v_pos = rd_ver_pos; rd_v_pos <= (rd_ver_pos + 8); rd_v_pos += 8 )
//    {

    read(ibufGAVG, rd_h_pos   , rd_v_pos  , srcG.select<8,1,16,1>(0,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos  , srcG.select<8,1,16,1>(0,16));
    read(ibufGAVG, rd_h_pos   , rd_v_pos+8, srcG.select<5,1,16,1>(8,0 ));
    read(ibufGAVG, rd_h_pos+32, rd_v_pos+8, srcG.select<5,1,16,1>(8,16));

    matrix<short, 13, 16> cenG;
    cenG = srcG.select<13,1,16,1>(0,2);

    //Horizonal check (8-bit differences)
#pragma unroll
    for(int j = 0; j < 8; j++) // j == index of output registers
    {
        vector<short, 32> rowG = srcG.row(2+j);

        matrix_ref<short, 8, 4> m_rowG = rowG.format<short, 8, 4>();
        matrix<short, 5, 16> v;

#ifdef CMRT_EMU
        vector<ushort, 16> index;
        v.row(0) = rowG.iselect(idx);

        index = idx + 1;
        v.row(1) = rowG.iselect(index);
        diff.row(0) = v.row(0) - v.row(1);

        index = idx + 2;
        v.row(2) = rowG.iselect(index);
        diff.row(1) = v.row(1) - v.row(2);

        index = idx + 3;
        v.row(3) = rowG.iselect(index);
        diff.row(2) = v.row(2) - v.row(3);

        index = idx + 4;
        v.row(4) = rowG.iselect(index);
        diff.row(3) = v.row(3) - v.row(4);
#else
        v.select<1,1,8,2>(0,0) = m_rowG.genx_select<4,4,2,1>(0,1);
        v.select<1,1,8,2>(0,1) = v.select<1,1,8,2>(0,0);

#pragma unroll
        for(int i = 1; i < 5; i++)
        {
            v.select<1,1,8,2>(i,0) = m_rowG.genx_select<4,4,2,1>(0,i+1);
            v.select<1,1,8,2>(i,1) = v.select<1,1,8,2>(i,0);

            diff.row(i-1) = v.row(i-1) - v.row(i);
        }
#endif

        maskH.row(j)  = (cm_abs<ushort>(diff.row(3)) > Grn_imbalance_th);
#pragma unroll
        for(int i = 0; i < 3; i++)
        {
            maskH.row(j) &= (cm_mul<short> (diff.row(i),diff.row(i+1)) < 0);
            maskH.row(j) &= (cm_abs<ushort>(diff.row(i)) > Grn_imbalance_th);
        }
    }

    //Vertical check (8-bit differences)
    //{0,1,4,5} row
#pragma unroll
    for(int j = 0; j <= 4; j+=4 )
    {
        diff          = (cenG.select<4,1,16,1>(1+j,0) - cenG.select<4,1,16,1>(2+j,0));

        maskV.row(j)  = (cm_abs<ushort>(diff.row(3)) > Grn_imbalance_th);
#pragma unroll
        for(int i = 0; i < 3; i++)
        {
            maskV.row(j) &= (cm_mul<short> (diff.row(i),diff.row(i+1)) < 0);
            maskV.row(j) &= (cm_abs<ushort>(diff.row(i)) > Grn_imbalance_th);
        }

        maskV.row(j+1)  = maskV.row(j);
    }

    //{2,3,6,7} row
#pragma unroll
    for(int j = 2; j <= 6; j+=4 )
    {
        diff          = (cenG.select<4,1,16,1>(  j,0) - cenG.select<4,1,16,1>(1+j,0));

        maskV.row(j)  = (cm_abs<ushort>(diff.row(3)) > Grn_imbalance_th);
#pragma unroll
        for(int i = 0; i < 3; i++)
        {
            maskV.row(j) &= (cm_mul<short> (diff.row(i),diff.row(i+1)) < 0);
            maskV.row(j) &= (cm_abs<ushort>(diff.row(i)) > Grn_imbalance_th);
        }
        maskV.row(j+1) = maskV.row(j);
    }

    //The second condition is to check color difference between two colors in a pixel (8-bit diff):
    read(ibufRAVG, rd_h_pos+4 , rd_v_pos+2, srcR);
    read(ibufBAVG, rd_h_pos+4 , rd_v_pos+2, srcB);

    matrix<short, 8, 16> absv;
    matrix<short, 8, 16> difference;

    difference = srcR - cenG.select<8,1,16,1>(2,0);
    absv = cm_abs<ushort>(difference);
    mask  = (absv > Average_Color_TH);

    difference = cenG.select<8,1,16,1>(2,0) - srcB;
    absv = cm_abs<ushort>(difference);
    mask |= (absv > Average_Color_TH);

    difference = srcB - srcR;
    absv = cm_abs<ushort>(difference);
    mask |= (absv > Average_Color_TH);

    mask |= maskH;
    mask |= maskV;
    //AVG_flag.merge(1, 2, mask);
    int rd_x_pos = (get_thread_origin_x() * 16);
    int rd_y_pos = (get_thread_origin_y() * 8 );
    read(ibufAVG_Flag, rd_x_pos, rd_y_pos, AVG_flag);
    AVG_flag.merge(1, mask);

    ///////////////////////////////////////////////////////////
    int wr_h_pos = (get_thread_origin_x() * 16 );
    int wr_v_pos = (get_thread_origin_y() * 8  );

    write(obufAVG_UpdatedFlag, wr_h_pos, wr_v_pos, AVG_flag);
    //}
}
#endif


extern "C" _GENX_MAIN_ void
CONFIDENCE_CHECK(SurfaceIndex RAVG, SurfaceIndex GAVG, SurfaceIndex BAVG,
                 SurfaceIndex RHOR, SurfaceIndex GHOR, SurfaceIndex BHOR,
                 SurfaceIndex RVER, SurfaceIndex GVER, SurfaceIndex BVER,
                 SurfaceIndex AVG_Flag, SurfaceIndex HV_Flag,
                 SurfaceIndex R_c , SurfaceIndex G_c , SurfaceIndex B_c)
{
    int rd_h_pos = get_thread_origin_x() * 16;
    int rd_x_pos = rd_h_pos * 2;
    int rd_v_pos = get_thread_origin_y() * 16;//get_thread_origin_y() * 8 ;

    matrix<uchar, 16, 16> avg_flag;
    read(AVG_Flag, rd_h_pos, rd_v_pos, avg_flag);

    matrix<uchar, 16, 16> hv_flag;
    read(HV_Flag,  rd_h_pos, rd_v_pos, hv_flag);

    matrix<short, 8, 16> out_R, out_G, out_B;
    matrix<short, 8, 16> avg, hor, ver;

    int wr_h_pos = get_thread_origin_x() * 32;
    int wr_v_pos = get_thread_origin_y() * 16;//get_thread_origin_y() * 8 ;

    /// RED COMPONENT /////
    read(RAVG, rd_x_pos, rd_v_pos, avg);
    read(RHOR, rd_x_pos, rd_v_pos, hor);
    read(RVER, rd_x_pos, rd_v_pos, ver);
    out_R.merge(avg, (avg_flag.select<8,1,16,1>(0,0) == AVG_True ));
    out_R.merge(hor, (avg_flag.select<8,1,16,1>(0,0) == AVG_False) & (hv_flag.select<8,1,16,1>(0,0)  == 1));
    out_R.merge(ver, (avg_flag.select<8,1,16,1>(0,0) == AVG_False) & (hv_flag.select<8,1,16,1>(0,0)  == 0));

    write(R_c, wr_h_pos, wr_v_pos, out_R);

    /// GREEN COMPONENT /////
    read(GAVG, rd_x_pos, rd_v_pos, avg);
    read(GHOR, rd_x_pos, rd_v_pos, hor);
    read(GVER, rd_x_pos, rd_v_pos, ver);
    out_G.merge(avg, (avg_flag.select<8,1,16,1>(0,0) == AVG_True ));
    out_G.merge(hor, (avg_flag.select<8,1,16,1>(0,0) == AVG_False) & (hv_flag.select<8,1,16,1>(0,0)  == 1));
    out_G.merge(ver, (avg_flag.select<8,1,16,1>(0,0) == AVG_False) & (hv_flag.select<8,1,16,1>(0,0)  == 0));

    write(G_c, wr_h_pos, wr_v_pos, out_G);

    /// BLUE COMPONENT /////
    read(BAVG, rd_x_pos, rd_v_pos, avg);
    read(BHOR, rd_x_pos, rd_v_pos, hor);
    read(BVER, rd_x_pos, rd_v_pos, ver);
    out_B.merge(avg, (avg_flag.select<8,1,16,1>(0,0) == AVG_True ));
    out_B.merge(hor, (avg_flag.select<8,1,16,1>(0,0) == AVG_False) & (hv_flag.select<8,1,16,1>(0,0)  == 1));
    out_B.merge(ver, (avg_flag.select<8,1,16,1>(0,0) == AVG_False) & (hv_flag.select<8,1,16,1>(0,0)  == 0));

    write(B_c, wr_h_pos, wr_v_pos, out_B);

    //////////////////////////////////////////////////////////////
    ///////////   LOWER 16x8 PIXEL BLOCK /////////////////////////
    //////////////////////////////////////////////////////////////
    rd_v_pos += 8 ;
    wr_v_pos += 8 ;

    /// RED COMPONENT /////
    read(RAVG, rd_x_pos, rd_v_pos, avg);
    read(RHOR, rd_x_pos, rd_v_pos, hor);
    read(RVER, rd_x_pos, rd_v_pos, ver);
    out_R.merge(avg, (avg_flag.select<8,1,16,1>(8,0) == AVG_True ));
    out_R.merge(hor, (avg_flag.select<8,1,16,1>(8,0) == AVG_False) & (hv_flag.select<8,1,16,1>(8,0) == 1));
    out_R.merge(ver, (avg_flag.select<8,1,16,1>(8,0) == AVG_False) & (hv_flag.select<8,1,16,1>(8,0) == 0));

    write(R_c, wr_h_pos, wr_v_pos, out_R);

    /// GREEN COMPONENT /////
    read(GAVG, rd_x_pos, rd_v_pos, avg);
    read(GHOR, rd_x_pos, rd_v_pos, hor);
    read(GVER, rd_x_pos, rd_v_pos, ver);
    out_G.merge(avg, (avg_flag.select<8,1,16,1>(8,0) == AVG_True ));
    out_G.merge(hor, (avg_flag.select<8,1,16,1>(8,0) == AVG_False) & (hv_flag.select<8,1,16,1>(8,0) == 1));
    out_G.merge(ver, (avg_flag.select<8,1,16,1>(8,0) == AVG_False) & (hv_flag.select<8,1,16,1>(8,0) == 0));

    write(G_c, wr_h_pos, wr_v_pos, out_G);

    /// BLUE COMPONENT /////
    read(BAVG, rd_x_pos, rd_v_pos, avg);
    read(BHOR, rd_x_pos, rd_v_pos, hor);
    read(BVER, rd_x_pos, rd_v_pos, ver);
    out_B.merge(avg, (avg_flag.select<8,1,16,1>(8,0) == AVG_True ));
    out_B.merge(hor, (avg_flag.select<8,1,16,1>(8,0) == AVG_False) & (hv_flag.select<8,1,16,1>(8,0) == 1));
    out_B.merge(ver, (avg_flag.select<8,1,16,1>(8,0) == AVG_False) & (hv_flag.select<8,1,16,1>(8,0) == 0));

    write(B_c, wr_h_pos, wr_v_pos, out_B);
}

#define Bad_TH1      100
#define Bad_TH2   175
#define Bad_TH3   10

#define NOT_SKL 0

extern "C" _GENX_ inline
void CalcIntResults(matrix_ref<short, 10, 16> Color1_in,         // 16 registers
                    matrix_ref<short, 10, 16> Color2_in,         // 16 registers  // Inputs
                    matrix_ref<ushort, 8, 8>  mask_all,
                    int flag,
                    matrix_ref<short, 8, 8>   Color1_out,        //  4 registers
                    matrix_ref<short, 8, 8>   Color2_out)        //  4 registers  // Outputs
{
    //Intermediate results:
    matrix< short, 8, 8> Color1_c = Color1_in.select<8,1,8,1>(1,1);
    matrix< short, 8, 8> Color2_c = Color2_in.select<8,1,8,1>(1,1);

    matrix< short, 8, 8> Color1_n, Color2_n, add_op;

    matrix< short, 8, 8> count_Color1(0);                         //Ex: count_R    //Stage1 output
    matrix< short, 8, 8> DColor1Color2_1(0);                     //Ex: DRG1       //Stage1 output
    matrix< short, 8, 8> DColor1Color2 = Color1_c - Color2_c;    //Ex: DRG

    matrix<ushort, 8, 8> mask;
    matrix<ushort, 8, 8> acc;

    // Repeat 8 times (for 8 neighbors in 3x3 neighborhood)
#pragma unroll
    for(int i = 0; i <= 2; i++)
    {
#pragma unroll
        for(int j = 0; j <= 2; j++)
        {

            if(! (j == 1 && i == 1))
            {
                Color1_n  = Color1_in.select<8,1,8,1>(j,i);
                Color2_n  = Color2_in.select<8,1,8,1>(j,i);
#if 1
                mask      = (cm_max<short>(cm_min<short>(Color1_c, Color1_n), cm_min<short>(Color2_c, Color2_n)) > Bad_TH2);
                // ADD code for Non-SKL+ platforms
                mask     &= mask_all;

                add_op = cm_mul<short>(cm_add<short>(Color1_n, -Color2_n), mask);

                DColor1Color2_1 += add_op;
                count_Color1    += mask;
#else
#pragma unroll
                for(int k = 0; k < 8; k++)
                {
                    mask.row(k)  = (cm_max<short>(cm_min<short>(Color1_c.row(k), Color1_n.row(k)), cm_min<short>(Color2_c.row(k), Color2_n.row(k))) > Bad_TH2);
                    mask.row(k) &= mask_all.row(k);

                    add_op.row(k) = cm_mul<short>(cm_add<short>(Color1_n.row(k), -Color2_n.row(k)), mask.row(k));

                    DColor1Color2_1.row(k) += add_op.row(k);
                    count_Color1.row(k)    += mask.row(k);
                }
#endif
            }

        }
    }

    ////////// END OF STAGE1 /////////////////////////////
    ////////// STAGE2 : CORRECT PIXELS ///////////////////
    matrix<short, 8, 8> out_corrected_value;
    matrix<short, 8, 8> tmp;
    tmp   = cm_mul<short>(DColor1Color2, -count_Color1);
    tmp  += DColor1Color2_1;

    mask  = (DColor1Color2_1 * DColor1Color2 < 0);
    mask |= (cm_abs<ushort>(tmp) > (cm_mul<short>(count_Color1, Bad_TH3)));

    //mask &= (count_Color1 != 0);
    //count_Color1   .merge(1, (count_Color1 == 0));
    //DColor1Color2_1.merge(0, (count_Color1 == 0));
    matrix<ushort, 8, 8> count_Color1_mask = (count_Color1 != 0);
    mask &= count_Color1_mask;

    //count_Color1   .merge(count_Color1, 1, count_Color1_mask);
    //DColor1Color2_1.merge(DColor1Color2_1, 0, count_Color1_mask);
    //add_op = DColor1Color2_1 / count_Color1;
    matrix<int, 8, 8> count_Color1_int, DColor1Color2_1_int;
    count_Color1_int.merge(count_Color1, 1, count_Color1_mask);
    DColor1Color2_1_int.merge(DColor1Color2_1, 0, count_Color1_mask);
    add_op = DColor1Color2_1_int / count_Color1_int  ;

    if(flag == 1)     //Color1 == R; Color2 == G
    {
       out_corrected_value = Color1_c - add_op;//DColor1Color2_1 / count_Color1;
       //out_corrected_value.select<4,2,4,2>(1,1)  = Color1_c.select<4,2,4,2>(1,1);
       out_corrected_value.merge(out_corrected_value, Color2_c, mask);
       Color2_out = Color2_c;
       Color2_out.select<4,2,4,2>(1,1) = out_corrected_value.select<4,2,4,2>(1,1);

       out_corrected_value = Color2_c + add_op;//DColor1Color2_1 / count_Color1;
       out_corrected_value.merge(out_corrected_value, Color1_c, mask);
       Color1_out = Color1_c;
       Color1_out.select<4,2,4,2>(0,1) = out_corrected_value.select<4,2,4,2>(0,1);
       Color1_out.select<4,2,4,2>(1,0) = out_corrected_value.select<4,2,4,2>(1,0);
    }
    else if(flag == 2) //Color1 == G; Color2 == B
    {
       out_corrected_value = Color2_c + add_op;//DColor1Color2_1 / count_Color1;
       out_corrected_value.merge(out_corrected_value, Color1_c, mask);
       //Color1_out = Color1_c; //Assuming subroutine has been invoke once
       Color1_out.select<4,2,4,2>(0,0) = out_corrected_value.select<4,2,4,2>(0,0);

       out_corrected_value = Color1_c - add_op;//DColor1Color2_1 / count_Color1;
       out_corrected_value.merge(out_corrected_value, Color2_c, mask);
       Color2_out = Color2_c;
       Color2_out.select<4,2,4,2>(0,1) = out_corrected_value.select<4,2,4,2>(0,1);
       Color2_out.select<4,2,4,2>(1,0) = out_corrected_value.select<4,2,4,2>(1,0);
    }
    else              //Color1 == B; Color2 == R
    {
       out_corrected_value = Color1_c - add_op;//DColor1Color2_1 / count_Color1;
       out_corrected_value.merge(out_corrected_value, Color2_c, mask);
       //Color2_out = Color2_c;  //Assuming subroutine has been invoke twice
       Color2_out.select<4,2,4,2>(0,0) = out_corrected_value.select<4,2,4,2>(0,0);

       out_corrected_value = Color2_c + add_op;//DColor1Color2_1 / count_Color1;
       out_corrected_value.merge(out_corrected_value, Color1_c, mask);
       //Color1_out = Color1_c;  //Assuming subroutine has been invoke twice
       Color1_out.select<4,2,4,2>(1,1) = out_corrected_value.select<4,2,4,2>(1,1);
    }

}


extern "C" _GENX_MAIN_
void BAD_PIXEL_CHECK(SurfaceIndex R_input_Index, SurfaceIndex G_input_Index, SurfaceIndex B_input_Index,
                     SurfaceIndex R_Output_Index,SurfaceIndex G_Output_Index,SurfaceIndex B_Output_Index,
                     //int m_Height,
                     //SurfaceIndex Y_Index,
                     //SurfaceIndex UV_Index,
                     int width, int height,
                     int x, int y, ushort max_intensity)
{
    matrix<short, 10, 16> R_in, G_in, B_in;
    matrix<short, 8 , 8 > R_out, G_out, B_out;

    int rd_x_pos = ((get_thread_origin_x() + x) * 8 - 1)  * sizeof(short);
    int rd_y_pos = ((get_thread_origin_y() + y) * 8 - 1)                 ;

    int dum;
    if(rd_x_pos == (68*8-1)*2  && rd_y_pos == (1*8-1))
        dum = 0;

    read(G_input_Index, rd_x_pos, rd_y_pos  , G_in.select<8, 1, 16, 1>(0, 0));
    read(G_input_Index, rd_x_pos, rd_y_pos+8, G_in.select<2, 1, 16, 1>(8, 0));

    read(R_input_Index, rd_x_pos, rd_y_pos  , R_in.select<8, 1, 16, 1>(0, 0));
    read(R_input_Index, rd_x_pos, rd_y_pos+8, R_in.select<2, 1, 16, 1>(8, 0));

    read(B_input_Index, rd_x_pos, rd_y_pos  , B_in.select<8, 1, 16, 1>(0, 0));
    read(B_input_Index, rd_x_pos, rd_y_pos+8, B_in.select<2, 1, 16, 1>(8, 0));

    matrix< short, 8, 8> diff;
    matrix<ushort, 8, 8> mask_all;

    diff = R_in.select<8,1,8,1>(1,1) - G_in.select<8,1,8,1>(1,1);
    mask_all  = (cm_abs<ushort>(diff) < Bad_TH1);
    diff = G_in.select<8,1,8,1>(1,1) - B_in.select<8,1,8,1>(1,1);
    mask_all &= (cm_abs<ushort>(diff) < Bad_TH1);
    diff = B_in.select<8,1,8,1>(1,1) - R_in.select<8,1,8,1>(1,1);
    mask_all &= (cm_abs<ushort>(diff) < Bad_TH1);

    CalcIntResults(R_in, G_in, mask_all, 1, R_out, G_out);
    CalcIntResults(G_in, B_in, mask_all, 2, G_out, B_out);
    CalcIntResults(B_in, R_in, mask_all, 3, B_out, R_out);


    R_out.merge(0, R_out < 0); R_out.merge(max_intensity, R_out > max_intensity);
    G_out.merge(0, G_out < 0); G_out.merge(max_intensity, G_out > max_intensity);
    B_out.merge(0, B_out < 0); B_out.merge(max_intensity, B_out > max_intensity);

    // modefied by Iwamoto to exclude padding area
#if 0
    int wr_x_pos = (get_thread_origin_x() + x) * 8 * sizeof(short);
    int wr_y_pos = (get_thread_origin_y() + y) * 8;

    write(R_Output_Index, wr_x_pos, wr_y_pos, R_out);
    write(G_Output_Index, wr_x_pos, wr_y_pos, G_out);
    write(B_Output_Index, wr_x_pos, wr_y_pos, B_out);
#else
    int wr_x_pos = (get_thread_origin_x() + x - 1 ) * 8 * sizeof(short);
    int wr_y_pos = (get_thread_origin_y() + y - 1 ) * 8;

    if ( wr_x_pos>=0 && wr_y_pos>=0 && wr_x_pos < width*sizeof(short) && wr_y_pos < height)
    {
        write(R_Output_Index, wr_x_pos, wr_y_pos, R_out);
        write(G_Output_Index, wr_x_pos, wr_y_pos, G_out);
        write(B_Output_Index, wr_x_pos, wr_y_pos, B_out);
    }
#endif

    /////////////////////////////////////////////////////////////
#if 0 // 0 : to disable FECSC
    matrix<short, 8, 8> trgY, trgU, trgV;
    trgY  = (R_out *(  306)) >> 10;
    trgY += (G_out *(  601)) >> 10;
    trgY += (B_out *(  117)) >> 10;

    trgU  = (R_out *( -151)) >> 10;
    trgU += (G_out *( -296)) >> 10;
    trgU += (B_out *(  446)) >> 10;

    trgV  = (R_out *(  630)) >> 10;
    trgV += (G_out *( -527)) >> 10;
    trgV += (B_out *( -102)) >> 10;

    matrix<uchar, 8, 8> Y_char;
    matrix<uchar, 4, 8> UV_char;

    //arrange data to the y_char, and uv_char container
    //Y_char = trgY + 16;
    trgY += 16;
    trgY = cm_min<short>(255, trgY);
    trgY = cm_max<short>(0,   trgY);

    Y_char = trgY;

    UV_char.select<4,1,4,2>(0,0) = trgU.select<4,2,4,2>(0,0) + 128;
    UV_char.select<4,1,4,2>(0,1) = trgV.select<4,2,4,2>(0,1) + 128;

    //int wr_h_pos = (wr_x_pos >> 1) - 8;// get_thread_origin_x() * 8 - 8 == (get_thread_origin_x() - 1) * 8
    //wr_y_pos -= 8;                       // get_thread_origin_y() * 8 - 8 == (get_thread_origin_y() - 1) * 8
    //int wr_v_pos = (wr_y_pos >> 1)    ;//                                == (get_thread_origin_y() - 1) * 4
    //write(Out_Index, wr_h_pos, wr_y_pos         , Y_char );
    //write(Out_Index, wr_h_pos, m_Height+wr_v_pos, UV_char);

    int x =  (get_thread_origin_x() - 1) * 8;
    int y =  (get_thread_origin_y() - 1) * 8;
    if(x >= 0 && y >= 0)
    {
        write(Y_Index,  x, y,   Y_char  );
        write(UV_Index, x, y/2, UV_char );
    }
#endif
}


#define GAMMA_NUM_CTRL_POINTS 64
extern "C" _GENX_MAIN_ void
FORWARD_GAMMA_SKL( SurfaceIndex Correct_Index, SurfaceIndex Slope_Index, SurfaceIndex Point_Index,
                   SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
                   SurfaceIndex R_O_Index, SurfaceIndex G_O_Index, SurfaceIndex B_O_Index,
                   ushort max_input_level)
{

        matrix<ushort, 4, 16> Correct, Slope, Point;

        vector_ref<ushort, 64> gamma_C = Correct.format<ushort, 1, 64>();
        vector_ref<ushort, 64> gamma_S = Slope  .format<ushort, 1, 64>();
        vector_ref<ushort, 64> gamma_P = Point  .format<ushort, 1, 64>();

        read(Correct_Index, 0, 0, Correct);
        read(Slope_Index  , 0, 0, Slope  );
        read(Point_Index  , 0, 0, Point  );

        matrix<ushort, 16, 16> r_in;
        matrix<ushort, 16, 16> r_out;

        int rd_v_pos = get_thread_origin_y() * 16;
        int rd_h_pos = get_thread_origin_x() * 16 * sizeof(short);

        read(R_I_Index, rd_h_pos, rd_v_pos  , r_in.select<8,1,16,1>(0,0));
        read(R_I_Index, rd_h_pos, rd_v_pos+8, r_in.select<8,1,16,1>(8,0));

        matrix<ushort, 16, 16> index(0);
        //matrix<uchar , 16, 16> index(0);
//#pragma unroll
        for(int i = 1; i < GAMMA_NUM_CTRL_POINTS; i++)
        {
            if((gamma_P(i) <= r_in).any())
            {
                index += (gamma_P(i) <= r_in);
                break; break; // bug.
            }
            //else // issue fixed by Samuel
            //{
            //    break;
            //}
        }

        vector<ushort, 16> index_plus;
        vector<ushort, 16> inR_Pi;
        vector<uint  , 16> product;
        vector<uint  , 16> diff;

        vector<ushort, 64> del_gammaC;
        del_gammaC     = gamma_C.select<64,1>(1) - gamma_C.select<64,1>(0);
        //del_gammaC(63) = 0xFFFF - gamma_C(63);
        del_gammaC(63) = max_input_level - gamma_C(63);

        vector<ushort, 64> del_gammaP;
        del_gammaP     = gamma_P.select<64,1>(1) - gamma_P.select<64,1>(0);
        //del_gammaP(63) = 0xFFFF - gamma_P(63);
        del_gammaP(63) = max_input_level - gamma_P(63);

#pragma unroll
        for(int j = 0; j < 16; j++)
        {
            //index_plus     = (index.row(j) + 1);
            r_out.row(j)   = del_gammaC.iselect(index.row(j));//gamma_C.iselect(index_plus) - gamma_C.iselect(index.row(j));
            inR_Pi         = r_in.row(j) - gamma_P.iselect(index.row(j));
            product        = cm_mul<uint>(r_out.row(j), inR_Pi);
            diff           = del_gammaP.iselect(index.row(j));//(gamma_P.iselect(index_plus) - gamma_P.iselect(index.row(j)));
            r_out.row(j)   = product / diff;
            r_out.row(j)  += gamma_C.iselect(index.row(j));
        }

        r_out = cm_max<short>(0     , r_out);
        //r_out = cm_min<short>(65535 , r_out);
        r_out = cm_min<short>(max_input_level , r_out);
        write(R_O_Index, rd_h_pos, rd_v_pos  , r_out.select<8,1,16,1>(0,0));
        write(R_O_Index, rd_h_pos, rd_v_pos+8, r_out.select<8,1,16,1>(8,0));

        ///////////////////////////////////////////////////////////////////////////////
        matrix_ref<ushort, 16, 16> g_in  = r_in ;
        matrix_ref<ushort, 16, 16> g_out = r_out;
        read(G_I_Index, rd_h_pos, rd_v_pos  , g_in.select<8,1,16,1>(0,0));
        read(G_I_Index, rd_h_pos, rd_v_pos+8, g_in.select<8,1,16,1>(8,0));

        index = 0;
//#pragma unroll
        for(int i = 1; i < GAMMA_NUM_CTRL_POINTS; i++)
        {
            if((gamma_P(i) <= g_in).any())
            {
                index += (gamma_P(i) <= g_in);
                break; break;
            }
        }

        vector_ref<ushort, 16> inG_Pi = inR_Pi;

#pragma unroll
        for(int j = 0; j < 16; j++)
        {
            //index_plus     = (index.row(j) + 1);
            g_out.row(j)   = del_gammaC.iselect(index.row(j));//gamma_C.iselect(index_plus) - gamma_C.iselect(index.row(j));
            inG_Pi         = g_in.row(j) - gamma_P.iselect(index.row(j));
            product        = cm_mul<uint>(g_out.row(j), inG_Pi);
            diff           = del_gammaP.iselect(index.row(j));//(gamma_P.iselect(index_plus) - gamma_P.iselect(index.row(j)));
            g_out.row(j)   = product / diff;
            g_out.row(j)  += gamma_C.iselect(index.row(j));
        }

        g_out = cm_max<short>(0     , g_out);
//        g_out = cm_min<short>(65535 , g_out);
        g_out = cm_min<short>(max_input_level , g_out);
        write(G_O_Index, rd_h_pos, rd_v_pos  , g_out.select<8,1,16,1>(0,0));
        write(G_O_Index, rd_h_pos, rd_v_pos+8, g_out.select<8,1,16,1>(8,0));

        ///////////////////////////////////////////////////////////////////////////////
        matrix_ref<ushort, 16, 16> b_in  = r_in ;
        matrix_ref<ushort, 16, 16> b_out = r_out;
        read(B_I_Index, rd_h_pos, rd_v_pos  , b_in.select<8,1,16,1>(0,0));
        read(B_I_Index, rd_h_pos, rd_v_pos+8, b_in.select<8,1,16,1>(8,0));

        index = 0;
//#pragma unroll
#if 1
        for(int i = 1; i < GAMMA_NUM_CTRL_POINTS; i++)
        {
            if((gamma_P(i) <= b_in).any())
            {
                index += (gamma_P(i) <= b_in);
                break; break;
            }
        }
#else
        vector<ushort, 16> mak;
        vector<short , 16> adv;

        vector<ushort, 16> comp_index;
//#pragma unroll
        for(int j = 0; j < 16; j++)
        {
            comp_index = 32;
            mak    = (gamma_P.iselect(comp_index) <= b_in.row(j));
            index.row(j)  = (mak << 5);
            //comp_index.merge(32, 0, mak);

            mak    = (gamma_P.iselect(comp_index) <= b_in.row(j));
            index.row(j) += (mak << 4);
            comp_index.merge((comp_index+16), (comp_index-16), mak);

            mak    = (gamma_P.iselect(comp_index) <= b_in.row(j));
            index.row(j) += (mak << 3);
            comp_index.merge((comp_index+8 ), (comp_index-8 ), mak);

            mak    = (gamma_P.iselect(comp_index) <= b_in.row(j));
            index.row(j) += (mak << 2);
            comp_index.merge((comp_index+4 ), (comp_index-4 ), mak);

            mak    = (gamma_P.iselect(comp_index) <= b_in.row(j));
            index.row(j) += (mak << 1);
            comp_index.merge((comp_index+2 ), (comp_index-2 ), mak);

            mak    = (gamma_P.iselect(comp_index) <= b_in.row(j));
            index.row(j) += (mak     );
            //comp_index.merge((comp_index+1 ), (comp_index-1 ), mak);

            index.row(j).merge(0, ((index.row(j) == 1) & (b_in.row(j) < gamma_P(1))));
        }
#endif
        vector_ref<ushort, 16> inB_Pi = inR_Pi;
#pragma unroll
        for(int j = 0; j < 16; j++)
        {
            b_out.row(j)   = del_gammaC.iselect(index.row(j));//(gamma_C.iselect(index_plus) - gamma_C.iselect(index.row(j)));
            inB_Pi         = b_in.row(j) - gamma_P.iselect(index.row(j));
            product        = cm_mul<uint>(b_out.row(j), inB_Pi);
            diff           = del_gammaP.iselect(index.row(j));//(gamma_P.iselect(index_plus) - gamma_P.iselect(index.row(j)));
            b_out.row(j)   = product / diff;
            b_out.row(j)  += gamma_C.iselect(index.row(j));
        }

        b_out = cm_max<short>(0     , b_out);
        //b_out = cm_min<short>(65535 , b_out);
        b_out = cm_min<short>(max_input_level , b_out);
        write(B_O_Index, rd_h_pos, rd_v_pos  , b_out.select<8,1,16,1>(0,0));
        write(B_O_Index, rd_h_pos, rd_v_pos+8, b_out.select<8,1,16,1>(8,0));
}

extern "C" _GENX_MAIN_ void
FORWARD_GAMMA_SKL2( SurfaceIndex Correct_Index, SurfaceIndex Slope_Index, SurfaceIndex Point_Index,
                    SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
                    SurfaceIndex R_O_Index, SurfaceIndex G_O_Index, SurfaceIndex B_O_Index,
                    ushort max_input_level)
{

        matrix<ushort, 4, 16> Correct, Slope, Point;

        vector_ref<ushort, 64> gamma_C = Correct.format<ushort, 1, 64>();
        vector_ref<ushort, 64> gamma_S = Slope  .format<ushort, 1, 64>();
        vector_ref<ushort, 64> gamma_P = Point  .format<ushort, 1, 64>();

        read(Correct_Index, 0, 0, Correct);
        read(Slope_Index  , 0, 0, Slope  );
        read(Point_Index  , 0, 0, Point  );

        matrix<ushort, 16, 16> r_in;

        int rd_v_pos = get_thread_origin_y() * 16;
        int rd_h_pos = get_thread_origin_x() * 16 * sizeof(short);

        read(R_I_Index, rd_h_pos, rd_v_pos  , r_in.select<8,1,16,1>(0,0));
        read(R_I_Index, rd_h_pos, rd_v_pos+8, r_in.select<8,1,16,1>(8,0));

        matrix<ushort, 16, 16> r_out(r_in); //16

        vector<ushort, 16> mask;        //1
        vector<ushort, 16> tmp;         //1
        vector<uint  , 16> tmp_int0;    //2
        vector<uint  , 16> tmp_int1;    //2

        unsigned int del_gammaC, del_gammaP;
//#pragma unroll
        for(int i = 0; i < GAMMA_NUM_CTRL_POINTS; i++)
        {
            del_gammaC = gamma_C(i+1) - gamma_C(i);
            del_gammaP = gamma_P(i+1) - gamma_P(i);
            int i1 = (i + 1);
#pragma unroll
            for(int j = 0; j < 16; j++)
            {
                mask     = (r_in.row(j) >= gamma_P(i ));
                mask    &= (r_in.row(j) <  gamma_P(i1));
                if(mask.any())
                {
                    tmp      = del_gammaC;//(gamma_C(i1) -  gamma_C(i ));
                    tmp_int0 = cm_mul<uint>(tmp, cm_add<short>(r_in.row(j), -gamma_P(i)));
                    //tmp_int1 = del_gammaP;//(gamma_P(i+1) - gamma_P(i));
                    tmp      = tmp_int0 / del_gammaP;
                    tmp     +=  gamma_C(i  );
                    r_out.row(j).merge(tmp, mask);
                }
            }
        }

        r_out = cm_max<short>(0     , r_out);
        //r_out = cm_min<short>(65535 , r_out);
        r_out = cm_min<short>(max_input_level , r_out);
        write(R_O_Index, rd_h_pos, rd_v_pos  , r_out.select<8,1,16,1>(0,0));
        write(R_O_Index, rd_h_pos, rd_v_pos+8, r_out.select<8,1,16,1>(8,0));
}


extern "C" _GENX_MAIN_ void
RGB_to_A8R8G8B8(SurfaceIndex ibufR, SurfaceIndex ibufG, SurfaceIndex ibufB, SurfaceIndex obufARGB, int src_bit_prec)
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

#if NOT_SKL     // NOT NEEDED for Non-SKL+ platforms!!
                matrix<short, 8, 8> DIFF_A, DIFF_B;
                mask      = (cm_abs<ushort>(Color2_c - Color2_n) < 50) & (cm_abs<ushort>(Color1_c - Color1_n) < 50);
                DIFF_A    = Color1_c - Color1_n;
                DIFF_B    = Color2_c - Color2_n;
                mask     |= (cm_abs<ushort>(DIFF_B) < 50) & (cm_abs<ushort>(DIFF_A) < 50);
#endif

/////////////////////////////////////////////////////////////////////////
// from BDW model

extern "C" _GENX_MAIN_ void HotPixel(SurfaceIndex InBayerIndex,
                                     SurfaceIndex OutBayerIndex,
                                     int thread_width,
                                     int thread_height,
                                     int ThreshPixelDiff,
                                     int threshNumPix)
{
    int rd_h_pos = 8 * get_thread_origin_x() - 2;
    int rd_v_pos = 8 * get_thread_origin_y() - 2;

    // read 16x16 data from 3 Input surfaces
    matrix<uchar, HOT_RD_BLOCKHEIGHT, HOT_RD_BLOCKWIDTH> readinMatrix_uchar;
    read(InBayerIndex, rd_h_pos, rd_v_pos, readinMatrix_uchar);

    matrix<ushort, HOT_RD_BLOCKHEIGHT, HOT_RD_BLOCKWIDTH> readinMatrix;
    readinMatrix = readinMatrix_uchar;

    if(rd_h_pos == (-2 + (thread_width - 1) * HOT_WR_BLOCKWIDTH ))    // 95 = (THREADWIDTH - 1)
    {
        readinMatrix.column(HOT_RD_BLOCKWIDTH - 2) = 0;
        readinMatrix.column(HOT_RD_BLOCKWIDTH - 1) = readinMatrix.column(HOT_RD_BLOCKWIDTH - 3);
    }
    else if(rd_h_pos == -2)
        readinMatrix.column(1) = 0;
    else if(rd_v_pos == -2)
        readinMatrix.row(1) = 0;
    else if(rd_v_pos == -2 + (thread_height - 1) * HOT_WR_BLOCKHEIGHT)
        readinMatrix.row(HOT_RD_BLOCKHEIGHT - 2) = 0;

    //Extract nine 12x12 matrices based on the 16x16 matrix
    matrix<short,HOT_WR_BLOCKHEIGHT,HOT_WR_BLOCKWIDTH> tmpM;

    matrix<short,HOT_WR_BLOCKHEIGHT,HOT_WR_BLOCKWIDTH> CN = readinMatrix.select<HOT_WR_BLOCKHEIGHT,1,HOT_WR_BLOCKWIDTH,1>(2,2);
    matrix<short,HOT_WR_BLOCKHEIGHT,HOT_WR_BLOCKWIDTH> DIFF_NUM(0);// = 0;

    tmpM = readinMatrix.select<HOT_WR_BLOCKHEIGHT,1,HOT_WR_BLOCKWIDTH,1>(0,0);
    matrix<short,HOT_WR_BLOCKHEIGHT,HOT_WR_BLOCKWIDTH> DIFF1;
#pragma unroll
    for(int k = 0; k < HOT_WR_BLOCKHEIGHT; k++)
        DIFF1.row(k) = tmpM.row(k) - CN.row(k);
    DIFF_NUM = (cm_abs<short>(DIFF1) > (short)ThreshPixelDiff);


    tmpM = readinMatrix.select<HOT_WR_BLOCKHEIGHT,1,HOT_WR_BLOCKWIDTH,1>(0,2);
    matrix<short,HOT_WR_BLOCKHEIGHT,HOT_WR_BLOCKWIDTH> DIFF2;
#pragma unroll
    for(int k = 0; k < HOT_WR_BLOCKHEIGHT; k++)
        DIFF2.row(k) = tmpM.row(k) - CN.row(k);
    DIFF_NUM += (cm_abs<short>(DIFF2) > (short)ThreshPixelDiff);


    tmpM = readinMatrix.select<HOT_WR_BLOCKHEIGHT,1,HOT_WR_BLOCKWIDTH,1>(0,4);
    matrix<short,HOT_WR_BLOCKHEIGHT,HOT_WR_BLOCKWIDTH> DIFF3;
#pragma unroll
    for(int k = 0; k < HOT_WR_BLOCKHEIGHT; k++)
        DIFF3.row(k) = tmpM.row(k) - CN.row(k);
    DIFF_NUM += (cm_abs<short>(DIFF3) > (short)ThreshPixelDiff);


    tmpM = readinMatrix.select<HOT_WR_BLOCKHEIGHT,1,HOT_WR_BLOCKWIDTH,1>(2,0);
    matrix<short,HOT_WR_BLOCKHEIGHT,HOT_WR_BLOCKWIDTH> DIFF4;
#pragma unroll
    for(int k = 0; k < HOT_WR_BLOCKHEIGHT; k++)
        DIFF4.row(k) = tmpM.row(k) - CN.row(k);
    DIFF_NUM += (cm_abs<short>(DIFF4) > (short)ThreshPixelDiff);


    tmpM = readinMatrix.select<HOT_WR_BLOCKHEIGHT,1,HOT_WR_BLOCKWIDTH,1>(2,4);
    matrix<short,HOT_WR_BLOCKHEIGHT,HOT_WR_BLOCKWIDTH> DIFF5;
#pragma unroll
    for(int k = 0; k < HOT_WR_BLOCKHEIGHT; k++)
        DIFF5.row(k) = tmpM.row(k) - CN.row(k);
    DIFF_NUM += (cm_abs<short>(DIFF5) > (short)ThreshPixelDiff);


    tmpM = readinMatrix.select<HOT_WR_BLOCKHEIGHT,1,HOT_WR_BLOCKWIDTH,1>(4,0);
    matrix<short,HOT_WR_BLOCKHEIGHT,HOT_WR_BLOCKWIDTH> DIFF6;
#pragma unroll
    for(int k = 0; k < HOT_WR_BLOCKHEIGHT; k++)
        DIFF6.row(k) = tmpM.row(k) - CN.row(k);
    DIFF_NUM += (cm_abs<short>(DIFF6) > (short)ThreshPixelDiff);


    tmpM = readinMatrix.select<HOT_WR_BLOCKHEIGHT,1,HOT_WR_BLOCKWIDTH,1>(4,2);
    matrix<short,HOT_WR_BLOCKHEIGHT,HOT_WR_BLOCKWIDTH> DIFF7;
#pragma unroll
    for(int k = 0; k < HOT_WR_BLOCKHEIGHT; k++)
        DIFF7.row(k) = tmpM.row(k) - CN.row(k);
    DIFF_NUM += (cm_abs<short>(DIFF7) > (short)ThreshPixelDiff);


    tmpM = readinMatrix.select<HOT_WR_BLOCKHEIGHT,1,HOT_WR_BLOCKWIDTH,1>(4,4);
    matrix<short,HOT_WR_BLOCKHEIGHT,HOT_WR_BLOCKWIDTH> DIFF8;
#pragma unroll
    for(int k = 0; k < HOT_WR_BLOCKHEIGHT; k++)
        DIFF8.row(k) = tmpM.row(k) - CN.row(k);
    DIFF_NUM += (cm_abs<short>(DIFF8) > (short)ThreshPixelDiff);

    // Base on the nine 12x12 matrices , obtain the 12x12 median matrix, and 12x12 diffnum matrix
    vector<short, HOT_WR_BLOCKWIDTH> min;
    vector<short, HOT_WR_BLOCKWIDTH> max;
    matrix<short, HOT_WR_BLOCKHEIGHT, HOT_WR_BLOCKWIDTH> DIFF0(0);


    matrix<uchar, HOT_WR_BLOCKHEIGHT, HOT_WR_BLOCKWIDTH> Median;
    matrix<short, HOT_WR_BLOCKHEIGHT, HOT_WR_BLOCKWIDTH> DIFF8_tmp;

#pragma unroll
    for(int i = 0; i < HOT_WR_BLOCKHEIGHT; i++){
        vector_ref<short, HOT_WR_BLOCKWIDTH> v0 = DIFF0.row(i);
        vector_ref<short, HOT_WR_BLOCKWIDTH> v1 = DIFF1.row(i);
        vector_ref<short, HOT_WR_BLOCKWIDTH> v2 = DIFF2.row(i);
        vector_ref<short, HOT_WR_BLOCKWIDTH> v3 = DIFF3.row(i);
        vector_ref<short, HOT_WR_BLOCKWIDTH> v4 = DIFF4.row(i);
        vector_ref<short, HOT_WR_BLOCKWIDTH> v5 = DIFF5.row(i);
        vector_ref<short, HOT_WR_BLOCKWIDTH> v6 = DIFF6.row(i);
        vector_ref<short, HOT_WR_BLOCKWIDTH> v7 = DIFF7.row(i);
        vector_ref<short, HOT_WR_BLOCKWIDTH> v8 = DIFF8.row(i);

        min = cm_min<short>(v0, v1);
        v1 = cm_max<short>(v0, v1);
        v0 = min;

        min = cm_min<short>(v0, v2);
        v2 = cm_max<short>(v0, v2);
        v0 = min;

        min = cm_min<short>(v1, v2);
        v2 = cm_max<short>(v1, v2);
        v1 = min;

        min = cm_min<short>(v3, v4);
        v4 = cm_max<short>(v3, v4);
        v3 = min;

        min = cm_min<short>(v3, v5);
        v5 = cm_max<short>(v3, v5);
        v3 = min;

        min = cm_min<short>(v4, v5);
        v5 = cm_max<short>(v4, v5);
        v4 = min;

        min = cm_min<short>(v6, v7);
        v7 = cm_max<short>(v6, v7);
        v6 = min;

        min = cm_min<short>(v6, v8);
        v8 = cm_max<short>(v6, v8);
        v6 = min;

        min = cm_min<short>(v7, v8);
        v8 = cm_max<short>(v7, v8);
        v7 = min;

        v3 = cm_max<short>(v0, v3);

        v6 = cm_max<short>(v3, v6);

        min = cm_min<short>(v1, v4);
        v4 = cm_max<short>(v1, v4);
        v1 = min;

        v7 = cm_max<short>(v1, v7);

        v4 = cm_min<short>(v4, v7);

        v2 = cm_min<short>(v2, v5);

        v2 = cm_min<short>(v2, v8);

        min = cm_min<short>(v2, v4);
        v4 = cm_max<short>(v2, v4);
        v2 = min;

        v6 = cm_max<short>(v2, v6);

        v4 = cm_min<short>(v4, v6);

        Median.row(i) = CN.row(i) + v4;
    }


    matrix<uchar, HOT_WR_BLOCKHEIGHT, HOT_WR_BLOCKWIDTH> DIFF_mask;
#pragma unroll
    for(int i = 0; i < HOT_WR_BLOCKHEIGHT; i++)
        DIFF_mask.row(i) = (DIFF_NUM.row(i) < (short)threshNumPix);

    matrix<uchar, HOT_WR_BLOCKHEIGHT, HOT_WR_BLOCKWIDTH> outMatrix;
    outMatrix.merge(CN, Median, DIFF_mask);

    int wr_h_pos = rd_h_pos + 2;
    int wr_v_pos = rd_v_pos + 2;

    write(OutBayerIndex, wr_h_pos, wr_v_pos, outMatrix);

}



const short init_gamma_point[13] = {0, 1, 2, 5, 9, 16, 26, 42, 65, 96, 136, 187, 256};
const short init_gamma_slope[13] = {3328, 2560, 1280, 960, 658, 512, 368, 278, 215, 179, 151, 124, 0};
const short init_gamma_bias[13] = {0, 13, 23, 38, 53, 71, 91, 114, 139, 165, 193, 223, 256};

// 14 bit case
/*
const SHORT init_gamma_point[64+1]   = {  0,  4,  8, 12,  16, 20,   24,  28,  32,  36,  40,  44,  48,  52,  56,  60,
                                         68, 76, 84, 92, 100, 108, 116, 124, 132, 140, 148, 156, 164, 172, 180, 188,
                                         220, 252, 284, 316, 348, 380, 412, 444, 476, 508, 540, 572, 604, 636, 668, 700,
                                         1212, 1724, 2236, 2748, 3260, 3772, 4284, 4796, 5820, 6844, 7868, 8892, 9916, 11940, 12964, 14964, 16383 };
const SHORT init_gamma_correct[64+1] = { 0, 374, 512, 616, 702, 777, 844, 905, 961, 1014, 1064, 111, 1156,1199,1240,
                                         1354, 1425, 1491, 1553, 1614, 1671, 1726, 1780, 1831, 1881, 1929, 1975, 2021, 2065, 2108, 2150,
                                         2309, 2456, 2593, 2723, 2844, 2961, 3072, 3178, 3280, 3378, 3473, 3566, 3655, 3742, 3826, 3908,
                                         5016, 5887, 6626, 7277, 7865, 8404, 8904, 9373, 10235, 11017, 11738, 12410, 13040, 14189, 14729, 15722, 16383 };
*/

extern "C" _GENX_MAIN_ void forward_gamma_correction(SurfaceIndex red_input,
                                                     SurfaceIndex green_input,
                                                     SurfaceIndex blue_input,
                                                     SurfaceIndex new_red_output,
                                                     SurfaceIndex new_green_output,
                                                     SurfaceIndex new_blue_output,
                                                     int new_height,
                                                     int new_width,
                                                     int src_bit_prec)
{
    int start_y = 0;
    int x = get_thread_origin_x() * 16;

// fixed several param by Iwamoto

//    int SrcPrecision = 8;
    //int SrcPrecision = src_bit_prec;
//    int outputprec = 12;
    int outputprec = src_bit_prec + 4;
    //int BitPrecision = 8;

    int NumOfSegments = 12;
    int PWLF_prec = 8;
//    int PWLF_prec = SRC_BIT_PRECISION; // what's this?
    int inverse = 0;
    int sRGB = 1;

    float max_input_level = float(1<<int(src_bit_prec))-1;
    float max_output_level = float(1<<int(src_bit_prec))-1;
    int PWLShift = src_bit_prec - PWLF_prec;
    int PWLF = 1;

    matrix<short, 8, 16> green_in = 0;
    matrix<short, 8, 16> blue_in = 0;
    matrix<short, 8, 16> red_in = 0;

    matrix<short, 8, 16> green = 0;
    matrix<short, 8, 16> blue = 0;
    matrix<short, 8, 16> red = 0;

    vector<short, 13> gamma_point(init_gamma_point);
    vector<short, 13> gamma_slope(init_gamma_slope);
    vector<short, 13> gamma_bias(init_gamma_bias);
    //vector<SHORT, 64+1> gamma_point(init_gamma_point);
    //vector<SHORT, 64+1> gamma_correct(init_gamma_correct);

    for (int y = start_y; y < new_height; y += 8) {
        read(green_input, x * sizeof(short), y, green_in);
        read(blue_input, x * sizeof(short), y, blue_in);
        read(red_input, x * sizeof(short), y, red_in);

        red = red_in;
        green = green_in;
        blue = blue_in;

        matrix<short, 8, 16> mask = 0;
        matrix<int, 8, 16> res = 0;

        matrix<short, 8, 16> green_out = 0;
        matrix<short, 8, 16> blue_out = 0;
        matrix<short, 8, 16> red_out = 0;

        if (PWLF) {
/*
            for(int Index = 0; Index < 64; Index++) {
                mask = (red >= (gamma_point(Index)) ) & (red < (gamma_point(Index+1) ));
                res = ( gamma_correct(Index) + (gamma_correct(Index+1)-gamma_correct(Index)) * (red-gamma_point(Index))/(gamma_point(Index+1)-gamma_point(Index)) );
                red_out.merge(res, red_out, mask);

                mask = (green >= (gamma_point(Index)) ) & (green < (gamma_point(Index+1) ));
                res = ( gamma_correct(Index) + (gamma_correct(Index+1)-gamma_correct(Index)) * (green-gamma_point(Index))/(gamma_point(Index+1)-gamma_point(Index)) );
                green_out.merge(res, green_out, mask);

                mask = (blue >= (gamma_point(Index)) )  & (blue < (gamma_point(Index+1)));
                res = ( gamma_correct(Index) + (gamma_correct(Index+1)-gamma_correct(Index)) * (blue-gamma_point(Index))/(gamma_point(Index+1)-gamma_point(Index)) );
                blue_out.merge(res, blue_out, mask);

*/
            // BDW PWLF
            for(int Index = 0; Index < NumOfSegments; Index++) {
                mask = (red >= (gamma_point(Index) << PWLShift) )
                    & (red < (gamma_point(Index+1) << PWLShift));
                res = ( (((red-(gamma_point(Index)<<PWLShift))*gamma_slope(Index))>>PWLF_prec) + (gamma_bias(Index)<<PWLShift));
                red_out.merge(res, red_out, mask);

                mask = (green >= (gamma_point(Index) << PWLShift) )
                    & (green < (gamma_point(Index+1) << PWLShift));
                res = ( (((green-(gamma_point(Index)<<PWLShift))*gamma_slope(Index))>>PWLF_prec) + (gamma_bias(Index)<<PWLShift));
                green_out.merge(res, green_out, mask);

                mask = (blue >= (gamma_point(Index) << PWLShift) )
                    & (blue < (gamma_point(Index+1) << PWLShift));
                res = ( (((blue-(gamma_point(Index)<<PWLShift))*gamma_slope(Index))>>PWLF_prec) + (gamma_bias(Index)<<PWLShift));
                blue_out.merge(res, blue_out, mask);
            }
        }
        else {
            if (inverse == 1)
            {
                matrix<float, 8, 16> red_float = red / (1<<src_bit_prec);
                matrix<float, 8, 16> green_float = green / (1<<src_bit_prec);
                matrix<float, 8, 16> blue_float = blue / (1<<src_bit_prec);

                red = cm_pow(red_float, 2.2)*(1<<src_bit_prec);
                green = cm_pow(green_float, 2.2)*(1<<src_bit_prec);
                blue = cm_pow(blue_float, 2.2)*(1<<src_bit_prec);
            }
            else
            {
                if (sRGB)
                {

                    mask = (red/max_input_level < 0.00313);
                    red_out.merge((red/max_input_level *12.92 * max_output_level+0.5),
                        (((cm_pow(matrix<float, 8, 16>(red)/max_input_level, 0.41667)*1.055-0.055)*max_output_level)+0.5),
                        mask);

                    mask = (green/max_input_level < 0.00313);
                    green_out.merge((green/max_input_level *12.92 * max_output_level+0.5),
                        (((cm_pow(matrix<float, 8, 16>(green)/max_input_level, 0.41667)*1.055-0.055)*max_output_level)+0.5),
                        mask);

                    mask = (blue/max_input_level < 0.00313);
                    blue_out.merge((blue/max_input_level *12.92 * max_output_level+0.5),
                        (((cm_pow(matrix<float, 8, 16>(blue)/max_input_level, 0.41667)*1.055-0.055)*max_output_level)+0.5),
                        mask);
                }
                else
                {
                    mask = ((red/float(1<<int(src_bit_prec))) < 0.018054);
                    red_out.merge(((red/float(1<<int(src_bit_prec))) *4.5 * (1<<outputprec)),
                        ((cm_pow((red/float(1<<int(src_bit_prec))), 0.45)*1.099297-0.099297)*(1<<outputprec)),
                        mask);

                    mask = ((green/float(1<<int(src_bit_prec))) < 0.018054);
                    green_out.merge(((green/float(1<<int(src_bit_prec))) *4.5 * (1<<outputprec)),
                        ((cm_pow((green/float(1<<int(src_bit_prec))), 0.45)*1.099297-0.099297)*(1<<outputprec)),
                        mask);

                    mask = ((blue/float(1<<int(src_bit_prec))) < 0.018054);
                    blue_out.merge(((blue/float(1<<int(src_bit_prec))) *4.5 * (1<<outputprec)),
                        ((cm_pow((blue/float(1<<int(src_bit_prec))), 0.45)*1.099297-0.099297)*(1<<outputprec)),
                        mask);
                }
            }
        }

        //red_out = cm_max<SHORT>(0, red_out);
        //green_out = cm_max<SHORT>(0, green_out);
        //blue_out = cm_max<SHORT>(0, blue_out);

        //red_out = cm_min<SHORT>((1<<outputprec)-1, red_out);
        //green_out = cm_min<SHORT>((1<<outputprec)-1, green_out);
        //blue_out = cm_min<SHORT>((1<<outputprec)-1, blue_out);

        write(new_red_output, x * sizeof(short), y, red_out);
        write(new_green_output, x * sizeof(short), y, green_out);
        write(new_blue_output, x * sizeof(short), y, blue_out);
    }
}





// The function front_end_csc() performs color space conversion.
#define OFFSET_PREC 10

const short init_coefs[14] = {263, 516, 100, -152, -298, 450, 450, -377, -73};
extern "C" _GENX_MAIN_ void
front_end_csc(SurfaceIndex red_input,
              SurfaceIndex green_input,
              SurfaceIndex blue_input,
              SurfaceIndex y_obuf,
              SurfaceIndex uv_obuf,
              int new_height,
              int new_width,
              int src_bit_prec)
{
    int start_y = 0;
    int x = get_thread_origin_x() * 8;

    // Input
//    int srcBitPrec = 8;
    int srcBitPrec = src_bit_prec;
    int max_input_level = 1 << src_bit_prec;
//    int maxRGB = 1023;
//    int inOffset1 = 0;
//    int inOffset2 = 0;
//    int inOffset3 = 0;
/*
    int maxRGB = (1<<(src_bit_prec+2))-1; // 8 bit src case = 1023
    int inOffset1 = 0;
    int inOffset2 = 0;
    int inOffset3 = 0;
    int preShift = OFFSET_PREC - srcBitPrec;

    //source image is more than 10 bit
    if(preShift < 0)
    {
        //raise precision of offsets
        inOffset1 <<= (-preShift);
        inOffset2 <<= (-preShift);
        inOffset3 <<= (-preShift);
        preShift = 0;
    }

    int outOffset1 = 64;  //16<<2
    int outOffset2 = 512; //128<<2
    int outOffset3 = 512; //128<<2

    //raise to 16 bit
    outOffset1 <<= 6;
    outOffset2 <<= 6;
    outOffset3 <<= 6;

    const int matrix_shift1 = 10 - (16 - cm_max<int>(srcBitPrec, 10));
    const int matrix_shift2 = 6;
    const int half = (matrix_shift2 > 0) ? (1 << (matrix_shift2-1)) : 0;
*/
    matrix<short, 16, 8> green = 0;
    matrix<short, 16, 8> blue = 0;
    matrix<short, 16, 8> red = 0;
// by Iwamoto
//    matrix<ushort, 16, 8> green = 0;
//    matrix<ushort, 16, 8> blue = 0;
//    matrix<ushort, 16, 8> red = 0;

    //vector<SHORT, 9> coefs(init_coefs);
/*
    vector<SHORT, 9> coefs;
    // 10bit
    coefs(0) = 263;
    coefs(1) = 516;
    coefs(2) = 100;
    coefs(3) = -152;
    coefs(4) = -298;
    coefs(5) = 450;
    coefs(6) = 450;
    coefs(7) = -377;
    coefs(8) = -73;
*/

     for (int y = start_y; y < new_height - 16; y += 16) {
        // Read input
        read(green_input, (x + 8) * sizeof(short), y + 8, green);
        read(blue_input, (x + 8)* sizeof(short), y + 8, blue);
        read(red_input, (x + 8)* sizeof(short), y + 8, red);

        //add input offset
#if 0
        matrix<SHORT, 16, 8> srcR = red;//(red << preShift) + inOffset1;
        matrix<SHORT, 16, 8> srcG = green;//(green << preShift) + inOffset2;
        matrix<SHORT, 16, 8> srcB = blue;//(blue << preShift) + inOffset3;
#else
        matrix<ushort, 16, 8> srcR = red>>(srcBitPrec-8);//(red << preShift) + inOffset1;
        matrix<ushort, 16, 8> srcG = green>>(srcBitPrec-8);//(green << preShift) + inOffset2;
        matrix<ushort, 16, 8> srcB = blue>>(srcBitPrec-8);//(blue << preShift) + inOffset3;
        // bit prec remapping to 8 bit by Iwamoto
        //matrix<ushort, 16, 8> srcR = red * ( 256/max_input_level);
        //matrix<ushort, 16, 8> srcG = green * ( 256/max_input_level);
        //matrix<ushort, 16, 8> srcB = blue * ( 256/max_input_level);
#endif


        //multiply by CSC matrix and lose bits to stay at x.16s bits (no rounding needed)
        //add output offsets (with UV optional biases) and clip the remaining bits
#if 0
        matrix<SHORT, 16, 8> trgR = ((((coefs(0) * srcR) + (coefs(1) * srcG) + (coefs(2) * srcB)) >> matrix_shift1) + outOffset1 + half) >> matrix_shift2;
        matrix<SHORT, 16, 8> trgG = ((((coefs(3) * srcR) + (coefs(4) * srcG) + (coefs(5) * srcB)) >> matrix_shift1) + outOffset2 + half) >> matrix_shift2;
        matrix<SHORT, 16, 8> trgB = ((((coefs(6) * srcR) + (coefs(7) * srcG) + (coefs(8) * srcB)) >> matrix_shift1) + outOffset3 + half) >> matrix_shift2;
#else
        /*
        matrix<short, 16, 8> trgR;
        trgR  = (srcR *( 306)) >> 10;
        trgR += (srcG *( 601)) >> 10;
        trgR += (srcB *( 117)) >> 10;

        matrix<short, 16, 8> trgG;
        trgG  = (srcR *( -151)) >> 10;
        trgG += (srcG *( -296)) >> 10;
        trgG += (srcB *( 446)) >> 10;

        matrix<short, 16, 8> trgB;
        trgB  = (srcR *( 630)) >> 10 ;
        trgB += (srcG *( -527)) >> 10;
        trgB += (srcB *( -102)) >> 10;
        */

        // BT.601
        matrix<float, 16, 8> trgRf;
        trgRf  = (srcR * 0.299f);
        trgRf += (srcG * 0.587f);
        trgRf += (srcB * 0.114f);

        matrix<float, 16, 8> trgGf;
        trgGf  = (srcR * -0.169f);
        trgGf += (srcG * -0.033f);
        trgGf += (srcB * 0.500f);

        matrix<float, 16, 8> trgBf;
        trgBf  = (srcR * 0.500f);
        trgBf += (srcG * -0.419f);
        trgBf += (srcB * -0.081f);

        matrix<uchar, 16, 8> trgR = matrix<uchar,16,8>(trgRf);
        matrix<uchar, 16, 8> trgG = matrix<uchar,16,8>(trgGf);
        matrix<uchar, 16, 8> trgB = matrix<uchar,16,8>(trgBf);


#endif
//        maxRGB = 255;
        //limit results
        /*trgR = cm_max<SHORT>(0, trgR);
        trgG = cm_max<SHORT>(0, trgG);
        trgB = cm_max<SHORT>(0, trgB);

        trgR = cm_min<SHORT>(maxRGB, trgR);
        trgG = cm_min<SHORT>(maxRGB, trgG);
        trgB = cm_min<SHORT>(maxRGB, trgB);*/

        // clip resumed by Iwamoto
        /*
        trgR = cm_max<SHORT>(0, trgR);
        trgG = cm_max<SHORT>(0, trgG);
        trgB = cm_max<SHORT>(0, trgB);

        trgR = cm_min<SHORT>(maxRGB, trgR);
        trgG = cm_min<SHORT>(maxRGB, trgG);
        trgB = cm_min<SHORT>(maxRGB, trgB);
        */

        // Write to YUV surfaces.
#if 0
        write(y_output, x * sizeof(SHORT), y, trgR);
        write(u_output, x * sizeof(SHORT), y, trgG);
        write(v_output, x * sizeof(SHORT), y, trgB);
#else
        matrix<uchar, 16, 8> y_char;
        matrix<uchar, 8, 8> uv_char;
        //matrix<uchar, 8, 4> u_char;
        //matrix<uchar, 8, 4> v_char;

        //arrange data to the y_char, and uv_char container
        y_char = trgR + 16;
//        y_char = trgR;
//        uv_char.select<8,1,4,2>(0,0) = trgG.select<8,2,4,2>(0,0) + 128;
//        uv_char.select<8,1,4,2>(0,1) = trgB.select<8,2,4,2>(0,1) + 128;
// fixed by Iwamoto
        uv_char.select<8,1,4,2>(0,1) = trgG.select<8,2,4,2>(0,0)+128;
        uv_char.select<8,1,4,2>(0,0) = trgB.select<8,2,4,2>(0,1)+128;
        //u_char = trgG.select<8,2,4,2>(0,0);
        //v_char = trgB.select<8,2,4,2>(0,1);

        write(y_obuf, x, y, y_char );
        write(uv_obuf, x, y/2, uv_char );
        //write(uv_obuf, x/2, y/2, u_char );
        //write(uv_obuf, x/2 + new_width/2, y/2, v_char );
#endif
    }
}

///////////////////////////////////////////////////////
////////////// 2 kernels SKL gamma latest /////////////
///////////////////////////////////////////////////////

const unsigned short v_in_init[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

extern "C" _GENX_MAIN_ void
PREPARE_LOOKUP_TABLE(SurfaceIndex Point_Index,
                     SurfaceIndex Lookup_Index,
                     SurfaceIndex Correct_Index,
                     ushort max_input_level)
{
    matrix<ushort, 4, 16> Correct, Point;
    vector_ref<ushort, 64> gamma_P = Point.format<ushort, 1, 64>();

    read(Correct_Index, 0, 0, Correct);
    read(Point_Index  , 0, 0, Point  );

    vector<uint, 16> index(0);
    vector<ushort, 16> v_in(v_in_init);

    int id = get_thread_origin_y() * 256 +  get_thread_origin_x();

    v_in += (id * 16);
#if 1
#pragma unroll
    for(int i = 1; i < GAMMA_NUM_CTRL_POINTS; i++)
    {
        if((gamma_P(i) <= v_in).any())
         {
             index += (gamma_P(i) <= v_in);
         }
    }
#else
     vector<ushort, 16> mak;
     vector<short , 16> adv;

     vector<ushort, 16> comp_index;
#pragma unroll
     for(int j = 0; j < 1; j++)
     {
            comp_index = 32;
            mak    = (gamma_P(32) <= v_in);
            index  = (mak << 5);
            comp_index.merge((comp_index+16), (comp_index-16), mak);

            mak    = (gamma_P.iselect(comp_index) <= v_in);
            index += (mak << 4);
            comp_index.merge((comp_index+8), (comp_index-8), mak);

            mak    = (gamma_P.iselect(comp_index) <= v_in);
            index += (mak << 3);
            comp_index.merge((comp_index+4 ), (comp_index-4), mak);

            mak    = (gamma_P.iselect(comp_index) <= v_in);
            index += (mak << 2);
            comp_index.merge((comp_index+2 ), (comp_index-2 ), mak);

            mak    = (gamma_P.iselect(comp_index) <= v_in);
            index += (mak << 1);
            comp_index.merge((comp_index+1 ), (comp_index-1 ), mak);

            mak    = (gamma_P.iselect(comp_index) <= v_in);
            index += (mak     );

            index.merge(0, ((index == 1) & (v_in < gamma_P(1))));
    }
#endif

    int wr_h_pos = (id * 16) * sizeof(int);
#if 0
    write(Lookup_Index, wr_h_pos, index);
#else
    vector<ushort, 16> m_i, b_i, c_i, p_i, c_i_plus_1, p_i_plus_1;
    vector<uint, 16> data; //MSB W for m_i, LSB W for bi
    vector<uint, 16> i_plus_1;
    i_plus_1.merge(max_input_level, (index + 1), (index == (GAMMA_NUM_CTRL_POINTS-1)));

    c_i = Correct.iselect(index);
    p_i = Point  .iselect(index);

    c_i_plus_1.merge( max_input_level,  Correct.iselect(i_plus_1), (index == (GAMMA_NUM_CTRL_POINTS-1)));
    p_i_plus_1.merge( max_input_level,  Point  .iselect(i_plus_1), (index == (GAMMA_NUM_CTRL_POINTS-1)));

    vector<uint, 16> v_out;


    v_out = (c_i_plus_1 - c_i);
    v_out *= (v_in - p_i);
    v_out /= (p_i_plus_1 - p_i);
    v_out += c_i;

    v_out = cm_max<uint>(0, v_out);
    v_out = cm_min<uint>(max_input_level, v_out);

    write(Lookup_Index, wr_h_pos, v_out);
#endif
}

extern "C" _GENX_MAIN_ void
FGC(SurfaceIndex Correct_Index, SurfaceIndex Slope_Index, SurfaceIndex Point_Index,
    SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
    SurfaceIndex R_O_Index, SurfaceIndex G_O_Index, SurfaceIndex B_O_Index,
    SurfaceIndex Lookup_Index)
{

    int rd_v_pos = get_thread_origin_y() * 16;
    int rd_h_pos = get_thread_origin_x() * 16 * sizeof(short);

    vector<ushort, 16> m_i, b_i, tmp;
    vector<uint, 16> data;
    vector<uint, 16> rd_data;
    vector<uint, 16> elem_offset;

    unsigned int max, min;

    //////// RED COMPONENT ////////////////////////////////////////////////
    matrix<ushort, 16, 16> r_in, r_out;
    read(R_I_Index, rd_h_pos, rd_v_pos  , r_in.select<8,1,16,1>(0,0));
    read(R_I_Index, rd_h_pos, rd_v_pos+8, r_in.select<8,1,16,1>(8,0));

#pragma unroll
    for(int j = 0; j < 16; j++)
    {
        //Derive index, by 1D scatter read
        elem_offset = r_in.row(j);
        read(Lookup_Index, 0, elem_offset, data);

        r_out.row(j) = data;
    }

    write(R_O_Index, rd_h_pos, rd_v_pos  , r_out.select<8,1,16,1>(0,0));
    write(R_O_Index, rd_h_pos, rd_v_pos+8, r_out.select<8,1,16,1>(8,0));

    //////// GREEN COMPONENT ////////////////////////////////////////////////
    matrix_ref<ushort, 16, 16> g_in  = r_in;
    matrix_ref<ushort, 16, 16> g_out = r_out;
    read(G_I_Index, rd_h_pos, rd_v_pos  , g_in.select<8,1,16,1>(0,0));
    read(G_I_Index, rd_h_pos, rd_v_pos+8, g_in.select<8,1,16,1>(8,0));

#pragma unroll
    for(int j = 0; j < 16; j++)
    {
        //Derive index, by 1D scatter read
        elem_offset = g_in.row(j);
        read(Lookup_Index, 0, elem_offset, data);

        g_out.row(j) = data;
    }

    write(G_O_Index, rd_h_pos, rd_v_pos  , g_out.select<8,1,16,1>(0,0));
    write(G_O_Index, rd_h_pos, rd_v_pos+8, g_out.select<8,1,16,1>(8,0));

    //////// BLUE COMPONENT ////////////////////////////////////////////////
    matrix_ref<ushort, 16, 16> b_in  = r_in;
    matrix_ref<ushort, 16, 16> b_out = r_out;
    read(B_I_Index, rd_h_pos, rd_v_pos  , b_in.select<8,1,16,1>(0,0));
    read(B_I_Index, rd_h_pos, rd_v_pos+8, b_in.select<8,1,16,1>(8,0));

#pragma unroll
    for(int j = 0; j < 16; j++)
    {
        //Derive index, by 1D scatter read
        elem_offset = b_in.row(j);
        read(Lookup_Index, 0, elem_offset, data);

        b_out.row(j) = data;
    }

    write(B_O_Index, rd_h_pos, rd_v_pos  , b_out.select<8,1,16,1>(0,0));
    write(B_O_Index, rd_h_pos, rd_v_pos+8, b_out.select<8,1,16,1>(8,0));
}

///////////////////////
//// FGC ARGB8
//////////////////////

extern "C" _GENX_MAIN_ void
FGAMMA_GPU_ARGB8(SurfaceIndex Correct_Index, SurfaceIndex Slope_Index, SurfaceIndex Point_Index,
                SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
                //SurfaceIndex ARGB8_Index,
                SurfaceIndex R_O_Index, SurfaceIndex G_O_Index, SurfaceIndex B_O_Index,
                SurfaceIndex Lookup_Index, int BitDepth)
{

    int rd_v_pos = get_thread_origin_y() * 16;
    int rd_h_pos = get_thread_origin_x() * 16 * sizeof(short);

    vector<ushort, 16> m_i, b_i, tmp;
    vector<uint, 16> data;
    vector<uint, 16> rd_data;
    vector<uint, 16> elem_offset;

    unsigned int max, min;

    //////// RED COMPONENT ////////////////////////////////////////////////
    matrix<ushort, 16, 16> r_in, r_out;
    read(R_I_Index, rd_h_pos, rd_v_pos  , r_in.select<8,1,16,1>(0,0));
    read(R_I_Index, rd_h_pos, rd_v_pos+8, r_in.select<8,1,16,1>(8,0));

#pragma unroll
    for(int j = 0; j < 16; j++)
    {
        //Derive index, by 1D scatter read
        elem_offset = r_in.row(j);
        read(Lookup_Index, 0, elem_offset, data);

        r_out.row(j) = data;
    }

    write(R_O_Index, rd_h_pos, rd_v_pos  , r_out.select<8,1,16,1>(0,0));
    write(R_O_Index, rd_h_pos, rd_v_pos+8, r_out.select<8,1,16,1>(8,0));

    //////// GREEN COMPONENT ////////////////////////////////////////////////
    matrix_ref<ushort, 16, 16> g_in  = r_in;
    matrix_ref<ushort, 16, 16> g_out = r_out;
    read(G_I_Index, rd_h_pos, rd_v_pos  , g_in.select<8,1,16,1>(0,0));
    read(G_I_Index, rd_h_pos, rd_v_pos+8, g_in.select<8,1,16,1>(8,0));

#pragma unroll
    for(int j = 0; j < 16; j++)
    {
        //Derive index, by 1D scatter read
        elem_offset = g_in.row(j);
        read(Lookup_Index, 0, elem_offset, data);

        g_out.row(j) = data;
    }

    write(G_O_Index, rd_h_pos, rd_v_pos  , g_out.select<8,1,16,1>(0,0));
    write(G_O_Index, rd_h_pos, rd_v_pos+8, g_out.select<8,1,16,1>(8,0));

    //////// BLUE COMPONENT ////////////////////////////////////////////////
    matrix_ref<ushort, 16, 16> b_in  = r_in;
    matrix_ref<ushort, 16, 16> b_out = r_out;
    read(B_I_Index, rd_h_pos, rd_v_pos  , b_in.select<8,1,16,1>(0,0));
    read(B_I_Index, rd_h_pos, rd_v_pos+8, b_in.select<8,1,16,1>(8,0));

#pragma unroll
    for(int j = 0; j < 16; j++)
    {
        //Derive index, by 1D scatter read
        elem_offset = b_in.row(j);
        read(Lookup_Index, 0, elem_offset, data);

        b_out.row(j) = data;
    }

    write(B_O_Index, rd_h_pos, rd_v_pos  , b_out.select<8,1,16,1>(0,0));
    write(B_O_Index, rd_h_pos, rd_v_pos+8, b_out.select<8,1,16,1>(8,0));
}



///////////////////////////////////////////////////////
////////////// NEW SLM ENABED GAMMA ///////////////////
///////////////////////////////////////////////////////

#define NUM_CONTROL_POINTS 64
const unsigned short initSeq[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

_GENX_ void inline
CORRECT(SurfaceIndex BUF_IN, SurfaceIndex BUF_OUT,
        int slmX, int blocks_in_a_row, int rd_h_pos)
{
    int rd_v_pos = 0;
    matrix<ushort, 16, 16> in, out;
    for(int k = 0; k < blocks_in_a_row; k++)
    {
        read(BUF_IN, rd_h_pos, rd_v_pos  , in.select<8,1,16,1>(0,0));
        read(BUF_IN, rd_h_pos, rd_v_pos+8, in.select<8,1,16,1>(8,0));

        vector<uint, 16> rd_slmDataIn; //vector<ushort, 16> rd_slmDataIn;
        vector<ushort, 16> v_Addr;
        v_Addr = in.row(0);  cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(0)  = rd_slmDataIn;
        v_Addr = in.row(1);  cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(1)  = rd_slmDataIn;
        v_Addr = in.row(2);  cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(2)  = rd_slmDataIn;
        v_Addr = in.row(3);  cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(3)  = rd_slmDataIn;
        v_Addr = in.row(4);  cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(4)  = rd_slmDataIn;
        v_Addr = in.row(5);  cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(5)  = rd_slmDataIn;
        v_Addr = in.row(6);  cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(6)  = rd_slmDataIn;
        v_Addr = in.row(7);  cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(7)  = rd_slmDataIn;
        v_Addr = in.row(8);  cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(8)  = rd_slmDataIn;
        v_Addr = in.row(9);  cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(9)  = rd_slmDataIn;
        v_Addr = in.row(10); cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(10) = rd_slmDataIn;
        v_Addr = in.row(11); cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(11) = rd_slmDataIn;
        v_Addr = in.row(12); cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(12) = rd_slmDataIn;
        v_Addr = in.row(13); cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(13) = rd_slmDataIn;
        v_Addr = in.row(14); cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(14) = rd_slmDataIn;
        v_Addr = in.row(15); cm_slm_read(slmX, v_Addr, rd_slmDataIn); out.row(15) = rd_slmDataIn;

        write(BUF_OUT, rd_h_pos, rd_v_pos  , out.select<8,1,16,1>(0,0));
        write(BUF_OUT, rd_h_pos, rd_v_pos+8, out.select<8,1,16,1>(8,0));

        rd_v_pos += 16;
    }
}



extern "C" _GENX_MAIN_ void
FGC_GPGPU(SurfaceIndex Correct_Index, SurfaceIndex Point_Index,
          SurfaceIndex R_I_Index, SurfaceIndex G_I_Index, SurfaceIndex B_I_Index,
          SurfaceIndex R_O_Index, SurfaceIndex G_O_Index, SurfaceIndex B_O_Index,
          int blocks_in_a_row, int BitDepth)
{
    //int rd_v_pos = (cm_group_id(0) * cm_group_count(0) + cm_local_id(1)) * 16;//get_thread_origin_y() * 16;
    int rd_v_pos = 0;//cm_linear_global_id() * 16;
    int rd_h_pos = cm_linear_global_id() * 32;//0;

    ////////////////////////////////////////////////////////////////////////////////////////
    cm_slm_init(65536);
    uint slmX = cm_slm_alloc(65536);

    matrix<ushort, 4, 16> Correct, Point;
    vector_ref<ushort, 64> gamma_P = Point.format<ushort, 1, 64>();

    read(Correct_Index, 0, 0, Correct);
    read(Point_Index  , 0, 0, Point  );

    vector<uint, 16> index(0);
    vector<ushort, 16> v_in_initial(initSeq);

    vector<uint, 64> v_slmData;
    vector<uint, 64> v_slmDataIn(7);

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
    loop_count = 16384 / 16;
    for(int k = 0; k < loop_count; k++)
    {
        v_in = v_in_initial +  k * 16;
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
        v_out.row(j) = cm_min<uint>(16383, v_out.row(j));

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
        v_o = cm_min<uint>(16383, v_o);
        cm_slm_write(slmX, v_in, v_o);

        //vector<ushort, 16> v_o_ushort;
        //v_o_ushort = cm_max<ushort>(0, v_o);
        //v_o_ushort = cm_min<ushort>(16383, v_o_ushort);
        //cm_slm_write(slmX, v_in, v_o_ushort);
#endif
    }
#ifndef CM_EMU
    cm_barrier();
#else
    }
#endif

    CORRECT(R_I_Index, R_O_Index, slmX, blocks_in_a_row, rd_h_pos);
    CORRECT(G_I_Index, G_O_Index, slmX, blocks_in_a_row, rd_h_pos);
    CORRECT(B_I_Index, B_O_Index, slmX, blocks_in_a_row, rd_h_pos);
}

//////// Gamma Correction v4 with ARGB8 conversion

#define NUM_CONTROL_POINTS 64
//const unsigned short initSeq[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};


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
GAMMA_GPUV4_ARGB8(SurfaceIndex Correct_Index, SurfaceIndex Point_Index,
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
