//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_camera_plugin_cpu.h"

int CPUCameraProcessor::CPU_Padding_16bpp(unsigned short* bayer_original, //input  of Padding module, bayer image of 16bpp format
                      unsigned short* bayer_input,      //output of Padding module, Paddded bayer image of 16bpp format
                      int width,                      //input framewidth
                      int height,                       //input frameheight
                      int bitDepth)
{

    int new_width  = width + 16;    //(width / 8 + 2) * 8;
    int new_height = height + 16;  //(height / 8 + 2) * 8;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int ori_index = y * width + x;
            int new_index = (y + 8) * new_width + (x + 8);

            bayer_input[new_index] = bayer_original[ori_index] >> (16 - bitDepth);
        }
    }
    // Pad top.
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < new_width; x++) {
            bayer_input[new_width * y + x] = bayer_input[new_width * (16 - y) + x];
        }
    }

    // Pad left.
    for (int y = 0; y < new_height; y++) {
        for (int x = 0; x < 8; x++) {
            bayer_input[new_width * y + x] = bayer_input[new_width * y + (16 - x)];
        }
    }

    // Pad bottom.
    for (int y = height + 8; y < new_height; y++) {
        for (int x = 0; x < new_width; x++) {
            bayer_input[new_width * y + x] = bayer_input[new_width * (((height + 8) - (y - (height + 8))) - 2) + x];
        }
    }

    // Pad right.
    for (int y = 0; y < new_height; y++) {
        for (int x = width + 8; x < new_width; x++) {
            bayer_input[new_width * y + x] = bayer_input[new_width * y + (((width + 8) - (x - (width + 8))) - 2)];
        }
    }

    return 0;

}

int CPUCameraProcessor::CPU_Bufferflip(unsigned short* buffer,
                   int width, int height, int bitdepth, bool firstflag)
{
    unsigned short tmp;
    int shiftamount = (firstflag == true)? (16-bitdepth) : 0;

    for (int y = 0; y < (height/2); y++)
    {
        for (int x = 0; x < width; x++)
        {
            tmp = buffer[y * width + x];
            buffer[y * width + x] = buffer[(height-y-1) * width + x] >> shiftamount;

            buffer[(height-y-1) * width + x] = tmp;
            buffer[(height-y-1) * width + x] >>= shiftamount;
        }
    }

    return 0;
}

int CPUCameraProcessor::CPU_BLC(unsigned short* Bayer,
            int PaddedWidth, int PaddedHeight, int bitDepth, int BayerType,
            short B_amount, short Gtop_amount, short Gbot_amount, short R_amount, bool firstflag)

{
    int index = 0, tmp = 0;
    unsigned short max_input = (1 << bitDepth) - 1;
    int shiftamount = (firstflag == true)? (16-bitDepth) : 0;

    short B  = B_amount;
    short G0 = Gtop_amount;
    short G1 = Gbot_amount;
    short R  = R_amount;

    if(BayerType == BAYER_RGGB)
    {
        B  = R_amount;
        G0 = Gtop_amount;
        G1 = Gbot_amount;
        R  = B_amount;
    }
    else if(BayerType == BAYER_GRBG)
    {
        B  = Gtop_amount;
        G0 = R_amount;
        G1 = B_amount;
        R  = Gbot_amount;
    }
    else if(BayerType == BAYER_GBRG)
    {
        B  = Gtop_amount;
        G0 = B_amount;
        G1 = R_amount;
        R  = Gbot_amount;
    }

    for (int y = 0;y < PaddedHeight; y++)
    {
        for (int x = 0;x < PaddedWidth; x++)
        {
            index = y * PaddedWidth + x;
            Bayer[index] >>= shiftamount;

            if     ((x % 2 == 0 ) && (y % 2 == 0))
                tmp = Bayer[index] - B;
            else if((x % 2 == 1 ) && (y % 2 == 0))
                tmp = Bayer[index] - G0;
            else if((x % 2 == 0 ) && (y % 2 == 1))
                tmp = Bayer[index] - G1;
            else //((x % 2 == 1 ) && (y % 2 == 1))
                tmp = Bayer[index] - R;

            tmp = (tmp < 0)?         0 : tmp;
            tmp = (tmp > max_input)? max_input : tmp;
            Bayer[index] = (unsigned short)tmp;


        }
    }


    return 0;
}

int CPUCameraProcessor::CPU_VIG(unsigned short* Bayer,
            unsigned short* Mask,
            int PaddedWidth, int PaddedHeight, int bitDepth, bool firstflag)
{
    unsigned short max_input = (1 << bitDepth) - 1;
    int index = 0;
    unsigned int tmp = 0;
    int shiftamount = (firstflag == true)? (16-bitDepth) : 0;

    for (int y = 0;y < PaddedHeight; y++)
    {
        for (int x = 0;x < PaddedWidth; x++)
        {
            index = y * PaddedWidth + x;
            Bayer[index] >>= shiftamount;

            tmp  = Bayer[index] * Mask[index];
            tmp += 128;
            tmp  = tmp / 256;

            tmp  = (tmp < 0)?         0 : tmp;
            tmp  = (tmp > max_input)? max_input : tmp;

            Bayer[index] = (unsigned short)tmp;
        }
    }

    return 0;
}

int CPUCameraProcessor::CPU_WB(unsigned short* Bayer,
           int PaddedWidth, int Paddedheight, int bitDepth, int BayerType,
           float B_scale, float Gtop_scale, float Gbot_scale, float R_scale, bool firstflag)
{
    unsigned short max_input = (1 << bitDepth) - 1;
    unsigned int tmp;
    int index = 0;
    int shiftamount = (firstflag == true)? (16-bitDepth) : 0;

    float B  = B_scale;
    float G0 = Gtop_scale;
    float G1 = Gbot_scale;
    float R  = R_scale;

    if(BayerType == BAYER_RGGB)
    {
        B  = R_scale;
        G0 = Gtop_scale;
        G1 = Gbot_scale;
        R  = B_scale;
    }
    else if(BayerType == BAYER_GRBG)
    {
        B  = Gtop_scale;
        G0 = R_scale;
        G1 = B_scale;
        R  = Gbot_scale;
    }
    else if(BayerType == BAYER_GBRG)
    {
        B  = Gtop_scale;
        G0 = B_scale;
        G1 = R_scale;
        R  = Gbot_scale;
    }


    for (int y = 0;y < Paddedheight; y++)
    {
        for (int x = 0;x < PaddedWidth; x++)
        {
            index = y * PaddedWidth + x;
            Bayer[index] >>= shiftamount;

            if     ((x % 2 == 0 ) && (y % 2 == 0))
                tmp = static_cast<unsigned int>(Bayer[index] * B);
            else if((x % 2 == 1 ) && (y % 2 == 0))
                tmp = static_cast<unsigned int>(Bayer[index] * G0);
            else if((x % 2 == 0 ) && (y % 2 == 1))
                tmp = static_cast<unsigned int>(Bayer[index] * G1);
            else //((x % 2 == 1 ) && (y % 2 == 1))
                tmp = static_cast<unsigned int>(Bayer[index] * R);

            tmp = (tmp < 0)?         0 : tmp;
            tmp = (tmp > max_input)? max_input : tmp;

            Bayer[index] = (unsigned short)tmp;
        }
    }

    return 0;
}

/* TODO - add correct offset for BayerType, see Cm code (need test content) */
int CPUCameraProcessor::CPU_GoodPixelCheck(unsigned short *pSrcBayer16bpp, int m_Width, int m_Height,
                       unsigned char *pDstFlag8bpp, int Diff_Prec, bool , int bitshiftamount, bool firstflag)
{
    int index;
    int index01;
    int index02;
    //int Diff_Prec = 0;

    //int dum;

    if(firstflag == true)
    {
            index = 0;
            for(int y=0;y<m_Height;y++)
            for(int x=0;x<m_Width;x++)
            {
                index = y * m_Width + x;
                pSrcBayer16bpp[index] >>= bitshiftamount;
            }
    }

    for(int y=0;y<m_Height;y++)
    for(int x=0;x<m_Width;x++)
    {
        int Good_Pixel = 0;
        int Bad_Pixel = 0;

        int Diff;

        /*if(x == 14 && y == 8)
        {
            dum = 1;
            for(int dely = 0; dely < 7; dely++)
            {
                for(int delx = 0; delx < 7; delx++)
                {
                    printf("%4d ", pSrcBayer16bpp[(y+dely-3)*m_Width+(x+delx-3)]);
                }
                printf("\n");
            }
        }*/
        index = y*m_Width + x;

        for(int i=-1;i<2;i++)
        for(int j=-1;j<2;j++)
        {
            int x1 = CLIP_VPOS_FLDRPT(x+j,m_Width);
            int y1 = CLIP_VPOS_FLDRPT(y+i,m_Height);

            index01 = y1*m_Width + x1;

            int Odd = 0;

            for(int ii=-2;ii<3;ii+=2)
            for(int jj=-2;jj<3;jj+=2)
            {
                if(Odd%2 == 1)
                {
                    int x2 = CLIP_VPOS_FLDRPT(x1+jj,m_Width);
                    int y2 = CLIP_VPOS_FLDRPT(y1+ii,m_Height);

                    index02 = y2*m_Width + x2;

                    if     (x1%2 == 1 && y1%2 == 1)
                    {
                        if((pSrcBayer16bpp[index01]>>Diff_Prec) > GOOD_INTENSITY_TH
                        && (pSrcBayer16bpp[index02]>>Diff_Prec) > GOOD_INTENSITY_TH)
                                Diff = abs((pSrcBayer16bpp[index01]>>Diff_Prec) - (pSrcBayer16bpp[index02]>>Diff_Prec));
                        else
                                Diff = GOOD_PIXEL_TH;
                    }
                    else if(x1%2 ==0 && y1%2 == 0)
                    {
                        if((pSrcBayer16bpp[index01]>>Diff_Prec) > GOOD_INTENSITY_TH &&
                           (pSrcBayer16bpp[index02]>>Diff_Prec) > GOOD_INTENSITY_TH)
                                Diff = abs((pSrcBayer16bpp[index01]>>Diff_Prec) - (pSrcBayer16bpp[index02]>>Diff_Prec));
                        else
                                Diff = GOOD_PIXEL_TH;
                    }
                    else
                    {
                        if((pSrcBayer16bpp[index01]>>Diff_Prec) > GOOD_INTENSITY_TH &&
                           (pSrcBayer16bpp[index02]>>Diff_Prec) > GOOD_INTENSITY_TH)
                                Diff = abs((pSrcBayer16bpp[index01]>>Diff_Prec) - (pSrcBayer16bpp[index02]>>Diff_Prec));
                        else
                                Diff = GOOD_PIXEL_TH;
                    }
                    if(Diff < GOOD_PIXEL_TH) //if(Diff > GOOD_PIXEL_TH)
                        Good_Pixel++;
                    if(Diff > GOOD_PIXEL_TH*8)
                        Bad_Pixel++;
                }
                Odd++;
            }

        }
        //if(Good_Pixel == 81)
        if(Good_Pixel > NUM_GOOD_PIXEL_TH)
        {
            pDstFlag8bpp[index] = 2;//AVG_True;
        }
        else if(Bad_Pixel > NUM_BIG_PIXEL_TH)
        {
            pDstFlag8bpp[index] = 2;//AVG_True;
        }
        else
        {
            pDstFlag8bpp[index] = 1;//AVG_False;
        }
    }

    return 0;
}

int CPUCameraProcessor::CPU_RestoreG(unsigned short *m_Pin,         //Pointer to Bayer data
                 int m_Width, int m_Height,
                  short *m_Pout_H__m_G,
                 short *m_Pout_V__m_G,
                 short *m_Pout_G__m_G,
                 int Max_Intensity)
{
    int Gamma_n, Gamma_s;
    int Gamma_w, Gamma_e;

    for(int y=2;y<m_Height-2;y++)  //y=7;y<m_Height-7;y++   Input size already include mirrored pixels
    for(int x=2;x<m_Width-2;x++)   //x=7;x<m_Width-7;x++
    {

        int G12 = m_Pin[(y-2)*m_Width+(x-1)];
        int G14 = m_Pin[(y-2)*m_Width+(x+1)];
        int G21 = m_Pin[(y-1)*m_Width+(x-2)];
        int G25 = m_Pin[(y-1)*m_Width+(x+2)];
        int G41 = m_Pin[(y+1)*m_Width+(x-2)];
        int G45 = m_Pin[(y+1)*m_Width+(x+2)];
        int G52 = m_Pin[(y+2)*m_Width+(x-1)];
        int G54 = m_Pin[(y+2)*m_Width+(x+1)];

        int G32 = m_Pin[ y   *m_Width+(x-1)];
        int G34 = m_Pin[ y   *m_Width+(x+1)];
        int G23 = m_Pin[(y-1)*m_Width+ x   ];
        int G43 = m_Pin[(y+1)*m_Width+ x   ];

        int dum;
        if(y == 8 && x == 8)
            dum = 1;

        int Avg_G = ((G12+G14+G21+G23+G25+G32+G34+G41+G43+G45+G52+G54+G32+G34+G23+G43)>>4);

        if(y%2 == 1 && x%2 == 1) //-7 to +7
        {
            int B15 = m_Pin[(y-2)*m_Width+(x+2)];
            int B51 = m_Pin[(y+2)*m_Width+(x-2)];
            int B55 = m_Pin[(y+2)*m_Width+(x+2)];

            int B13 = m_Pin[(y-2)*m_Width+ x   ];
            int B31 = m_Pin[ y   *m_Width+(x-2)];
            int B33 = m_Pin[ y   *m_Width+ x   ];
            int B35 = m_Pin[ y   *m_Width+(x+2)];
            int B53 = m_Pin[(y+2)*m_Width+ x   ];

            int Avg_B = ((B13+B15+B31+B33+B35+B51+B53+B55)>>3);//B11+

            //Along Vertical
            Gamma_n = abs(B13-B33);
            Gamma_s = abs(B33-B53);

            Gamma_w = abs(B31-B33);
            Gamma_e = abs(B33-B35);

            //Horizontal output
            if(Gamma_w < Gamma_e)
                m_Pout_H__m_G[y*m_Width+x] = static_cast<short>((5*G32+3*G34-B31+2*B33-B35)>>3);
            else
                m_Pout_H__m_G[y*m_Width+x] = static_cast<short>((5*G34+3*G32-B31+2*B33-B35)>>3);

            //Vertical output
            if(Gamma_n < Gamma_s)
                m_Pout_V__m_G[y*m_Width+x] = static_cast<short>((5*G23+3*G43-B13+2*B33-B53)>>3);
            else
                m_Pout_V__m_G[y*m_Width+x] = static_cast<short>((5*G43+3*G23-B13+2*B33-B53)>>3);

            m_Pout_G__m_G[y*m_Width+x] = static_cast<short>(B33 + Avg_G - Avg_B);
        }
        else if(y%2 == 0 && x%2 == 0)
        {
            int R15 = m_Pin[(y-2)*m_Width+(x+2)];
            int R51 = m_Pin[(y+2)*m_Width+(x-2)];
            int R55 = m_Pin[(y+2)*m_Width+(x+2)];

            int R13 = m_Pin[(y-2)*m_Width+ x   ];
            int R31 = m_Pin[ y   *m_Width+(x-2)];
            int R33 = m_Pin[ y   *m_Width+ x   ];
            int R35 = m_Pin[ y   *m_Width+(x+2)];
            int R53 = m_Pin[(y+2)*m_Width+ x   ];

            int Avg_R = ((R13+R15+R31+R33+R35+R51+R53+R55)>>3);//R11+


            Gamma_n = abs(R13-R33);
            Gamma_s = abs(R33-R53);

            Gamma_w = abs(R31-R33);
            Gamma_e = abs(R33-R35);

            //For Horizontal
            if(Gamma_w < Gamma_e)
                m_Pout_H__m_G[y*m_Width+x] = static_cast<short>((5*G32+3*G34-R31+2*R33-R35)>>3);
            else
                m_Pout_H__m_G[y*m_Width+x] = static_cast<short>((5*G34+3*G32-R31+2*R33-R35)>>3);

            //For Vertical
            if(Gamma_n < Gamma_s)
                m_Pout_V__m_G[y*m_Width+x] = static_cast<short>((5*G23+3*G43-R13+2*R33-R53)>>3);
            else
                m_Pout_V__m_G[y*m_Width+x] = static_cast<short>((5*G43+3*G23-R13+2*R33-R53)>>3);

            m_Pout_G__m_G[y*m_Width+x] = static_cast<short>(R33 + Avg_G - Avg_R);
        }
        else
        {
            m_Pout_H__m_G[y*m_Width+x] = m_Pin[y*m_Width+x];
            m_Pout_V__m_G[y*m_Width+x] = m_Pin[y*m_Width+x];
            m_Pout_G__m_G[y*m_Width+x] = m_Pin[y*m_Width+x];
        }
        m_Pout_H__m_G[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_H__m_G[y*m_Width+x], 0, Max_Intensity));
        m_Pout_V__m_G[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_V__m_G[y*m_Width+x], 0, Max_Intensity));
        m_Pout_G__m_G[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_G__m_G[y*m_Width+x], 0, Max_Intensity));
    }

    return 0;
}

int CPUCameraProcessor::CPU_RestoreBandR(unsigned short *m_Pin,         //Pointer to Bayer data
                     int m_Width, int m_Height,
                      short *m_Pout_H__m_G, short *m_Pout_V__m_G, short *m_Pout_G__m_G,  //Input,  G_H, G_V, G_A component
                     short *m_Pout_H__m_B, short *m_Pout_V__m_B, short *m_Pout_G__m_B,  //Output, B_H, B_V, B_A    component
                     short *m_Pout_H__m_R, short *m_Pout_V__m_R, short *m_Pout_G__m_R,  //Output, R_H, R_V, R_A component
                     int Max_Intensity)
{
    int dum;

    //For B
    for(int y=2;y<m_Height-2;y++)
    for(int x=2;x<m_Width-2;x++)
    {
        if(y%2==1 && x%2 == 0)
        {
            int B22= m_Pin[y*m_Width+(x-1)];
            int B24= m_Pin[y*m_Width+(x+1)];

            int G22_H= m_Pout_H__m_G[y*m_Width+(x-1)];
            int G23_H= m_Pout_H__m_G[y*m_Width+ x   ];
            int G24_H= m_Pout_H__m_G[y*m_Width+(x+1)];

            int G22_V= m_Pout_V__m_G[y*m_Width+(x-1)];
            int G23_V= m_Pout_V__m_G[y*m_Width+ x   ];
            int G24_V= m_Pout_V__m_G[y*m_Width+(x+1)];

            m_Pout_V__m_B[y*m_Width+x] = static_cast<short>(B22+B24-G22_V+2*G23_V-G24_V)/2;  // B32_V
            m_Pout_H__m_B[y*m_Width+x] = static_cast<short>(B22+B24-G22_H+2*G23_H-G24_H)/2;  // B32_H

            int G22_G= m_Pout_G__m_G[y*m_Width+(x-1)];
            int G23_G= m_Pout_G__m_G[y*m_Width+ x   ];
            int G24_G= m_Pout_G__m_G[y*m_Width+(x+1)];

            m_Pout_G__m_B[y*m_Width+x] = static_cast<short>(B22+B24-G22_G+2*G23_G-G24_G)/2;  // B32_A
        }
        else if(y%2 ==0 && x%2==1)
        {
            int B22= m_Pin[(y-1)*m_Width+x];
            int B42= m_Pin[(y+1)*m_Width+x];

            int G22_H = m_Pout_H__m_G[(y-1)*m_Width+x];
            int G32_H = m_Pout_H__m_G[ y   *m_Width+x];
            int G42_H = m_Pout_H__m_G[(y+1)*m_Width+x];

            int G22_V = m_Pout_V__m_G[(y-1)*m_Width+x];
            int G32_V = m_Pout_V__m_G[ y   *m_Width+x];
            int G42_V = m_Pout_V__m_G[(y+1)*m_Width+x];

            m_Pout_H__m_B[y*m_Width+x] = static_cast<short>(B22+B42-G22_H+2*G32_H-G42_H)/2;  // B23_H
            m_Pout_V__m_B[y*m_Width+x] = static_cast<short>(B22+B42-G22_V+2*G32_V-G42_V)/2;  // B23_V


            int G22_G = m_Pout_G__m_G[(y-1)*m_Width+x];
            int G32_G = m_Pout_G__m_G[ y   *m_Width+x];
            int G42_G = m_Pout_G__m_G[(y+1)*m_Width+x];

            m_Pout_G__m_B[y*m_Width+x] = static_cast<short>(B22+B42-G22_G+2*G32_G-G42_G)/2;  // B23_A

        }
        else if(y%2 == 1 && x%2 == 1)
        {
            m_Pout_H__m_B[y*m_Width+x] = m_Pin[y*m_Width+x];               // B22_H
            m_Pout_V__m_B[y*m_Width+x] = m_Pin[y*m_Width+x];               // B22_V
            m_Pout_G__m_B[y*m_Width+x] = m_Pin[y*m_Width+x];               // B22_A
        }
        m_Pout_H__m_B[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_H__m_B[y*m_Width+x], 0, Max_Intensity));
        m_Pout_V__m_B[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_V__m_B[y*m_Width+x], 0, Max_Intensity));
        m_Pout_G__m_B[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_G__m_B[y*m_Width+x], 0, Max_Intensity));
    }
    for(int y=2;y<m_Height-2;y++)
    for(int x=2;x<m_Width-2;x++)
    {
        if(y%2 ==0 && x%2 == 0)
        {
            int B32_H = m_Pout_H__m_B[ y   *m_Width+(x-1)];
            int B34_H = m_Pout_H__m_B[ y   *m_Width+(x+1)];

            int G32_H = m_Pout_H__m_G[ y   *m_Width+(x-1)];
            int G33_H = m_Pout_H__m_G[ y   *m_Width+ x   ];
            int G34_H = m_Pout_H__m_G[ y   *m_Width+(x+1)];

            int B23_V = m_Pout_V__m_B[(y-1)*m_Width+ x   ];
            int B43_V = m_Pout_V__m_B[(y+1)*m_Width+ x   ];

            int G23_V = m_Pout_V__m_G[(y-1)*m_Width+ x   ];
            int G33_V = m_Pout_V__m_G[ y   *m_Width+ x   ];
            int G43_V = m_Pout_V__m_G[(y+1)*m_Width+ x   ];

            //For Horizontal
            m_Pout_H__m_B[y*m_Width+x] = static_cast<short>(B32_H + B34_H - G32_H + 2* G33_H - G34_H)/2;   // B33_H
            //FOr vertical
            m_Pout_V__m_B[y*m_Width+x] = static_cast<short>(B23_V + B43_V - G23_V + 2* G33_V - G43_V)/2;   // B33_V

            int B32_G = m_Pout_G__m_B[ y   *m_Width+(x-1)];
            int B34_G = m_Pout_G__m_B[ y   *m_Width+(x+1)];

            int G32_G = m_Pout_G__m_G[ y   *m_Width+(x-1)];
            int G33_G = m_Pout_G__m_G[ y   *m_Width+ x   ];
            int G34_G = m_Pout_G__m_G[ y   *m_Width+(x+1)];

            int B23_G = m_Pout_G__m_B[(y-1)*m_Width+ x   ];
            int B43_G = m_Pout_G__m_B[(y+1)*m_Width+ x   ];

            int G23_G = m_Pout_G__m_G[(y-1)*m_Width+ x   ];
            int G43_G = m_Pout_G__m_G[(y+1)*m_Width+ x   ];

            m_Pout_G__m_B[y*m_Width+x] = static_cast<short>((B32_G + B34_G + B23_G + B43_G - G32_G + 2* G33_G - G34_G -G23_G + 2* G33_G - G43_G)/4); // B33_A
        }
        m_Pout_H__m_B[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_H__m_B[y*m_Width+x], 0, Max_Intensity));
        m_Pout_V__m_B[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_V__m_B[y*m_Width+x], 0, Max_Intensity));
        m_Pout_G__m_B[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_G__m_B[y*m_Width+x], 0, Max_Intensity));
    }

    //For R
    for(int y=2;y<m_Height-2;y++)
    for(int x=2;x<m_Width-2;x++)
    {
        if(y%2==0 && x%2 == 1)
        {
            int R22= m_Pin[y*m_Width+(x-1)];
            int R24= m_Pin[y*m_Width+(x+1)];

            int G22_H = m_Pout_H__m_G[y*m_Width+(x-1)];
            int G23_H = m_Pout_H__m_G[y*m_Width+ x   ];
            int G24_H = m_Pout_H__m_G[y*m_Width+(x+1)];

            int G22_V = m_Pout_V__m_G[y*m_Width+(x-1)];
            int G23_V = m_Pout_V__m_G[y*m_Width+ x   ];
            int G24_V = m_Pout_V__m_G[y*m_Width+(x+1)];

            m_Pout_V__m_R[y*m_Width+x] = static_cast<short>((R22+R24-G22_V+2*G23_V-G24_V)/2);
            m_Pout_H__m_R[y*m_Width+x] = static_cast<short>((R22+R24-G22_H+2*G23_H-G24_H)/2);

            int G22_G = m_Pout_G__m_G[y*m_Width+(x-1)];
            int G23_G = m_Pout_G__m_G[y*m_Width+ x   ];
            int G24_G = m_Pout_G__m_G[y*m_Width+(x+1)];

            m_Pout_G__m_R[y*m_Width+x] = static_cast<short>((R22+R24-G22_G+2*G23_G-G24_G)/2);
        }
        else if(y%2 ==1 && x%2==0)
        {
            int R22= m_Pin[(y-1)*m_Width+x];
            int R42= m_Pin[(y+1)*m_Width+x];

            int G22_H = m_Pout_H__m_G[(y-1)*m_Width+x];
            int G32_H = m_Pout_H__m_G[ y   *m_Width+x];
            int G42_H = m_Pout_H__m_G[(y+1)*m_Width+x];

            int G22_V = m_Pout_V__m_G[(y-1)*m_Width+x];
            int G32_V = m_Pout_V__m_G[ y   *m_Width+x];
            int G42_V = m_Pout_V__m_G[(y+1)*m_Width+x];

            m_Pout_H__m_R[y*m_Width+x] = static_cast<short>((R22+R42-G22_H+2*G32_H-G42_H)/2);
            m_Pout_V__m_R[y*m_Width+x] = static_cast<short>((R22+R42-G22_V+2*G32_V-G42_V)/2);

            int G22_G = m_Pout_G__m_G[(y-1)*m_Width+x];
            int G32_G = m_Pout_G__m_G[ y   *m_Width+x];
            int G42_G = m_Pout_G__m_G[(y+1)*m_Width+x];

            m_Pout_G__m_R[y*m_Width+x] = static_cast<short>((R22+R42-G22_G+2*G32_G-G42_G)/2);
        }
        else if(y%2 ==0 && x%2 ==0 )
        {
            m_Pout_H__m_R[y*m_Width+x] = m_Pin[y*m_Width+x];
            m_Pout_V__m_R[y*m_Width+x] = m_Pin[y*m_Width+x];
            m_Pout_G__m_R[y*m_Width+x] = m_Pin[y*m_Width+x];
        }
        m_Pout_H__m_R[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_H__m_R[y*m_Width+x], 0, Max_Intensity));
        m_Pout_V__m_R[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_V__m_R[y*m_Width+x], 0, Max_Intensity));
        m_Pout_G__m_R[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_G__m_R[y*m_Width+x], 0, Max_Intensity));
    }

    for(int y=2;y<m_Height-2;y++)
    for(int x=2;x<m_Width-2;x++)
    {
        if(y%2 ==1 && x%2 ==1)
        {
            int R32_H = m_Pout_H__m_R[ y   *m_Width+(x-1)];
            int R34_H = m_Pout_H__m_R[ y   *m_Width+(x+1)];

            int G32_H = m_Pout_H__m_G[ y   *m_Width+(x-1)];
            int G33_H = m_Pout_H__m_G[ y   *m_Width+ x   ];
            int G34_H = m_Pout_H__m_G[ y   *m_Width+(x+1)];

            int R23_V = m_Pout_V__m_R[(y-1)*m_Width+ x   ];
            int R43_V = m_Pout_V__m_R[(y+1)*m_Width+ x   ];

            int G23_V = m_Pout_V__m_G[(y-1)*m_Width+ x   ];
            int G33_V = m_Pout_V__m_G[ y   *m_Width+ x   ];
            int G43_V = m_Pout_V__m_G[(y+1)*m_Width+ x   ];

            if(y == 9 && x == 61)
                dum = 1;

            //For Horizontal
            m_Pout_H__m_R[y*m_Width+x] = static_cast<short>((R32_H + R34_H - G32_H + 2* G33_H - G34_H)/2);
            //For Vertical
            m_Pout_V__m_R[y*m_Width+x] = static_cast<short>((R23_V + R43_V - G23_V + 2* G33_V - G43_V)/2);

            int R32_G = m_Pout_G__m_R[ y   *m_Width+(x-1)];
            int R34_G = m_Pout_G__m_R[ y   *m_Width+(x+1)];

            int G32_G = m_Pout_G__m_G[ y   *m_Width+(x-1)];
            int G33_G = m_Pout_G__m_G[ y   *m_Width+ x   ];
            int G34_G = m_Pout_G__m_G[ y   *m_Width+(x+1)];

            int R23_G = m_Pout_G__m_R[(y-1)*m_Width+ x   ];
            int R43_G = m_Pout_G__m_R[(y+1)*m_Width+ x   ];

            int G23_G = m_Pout_G__m_G[(y-1)*m_Width+ x   ];

            int G43_G = m_Pout_G__m_G[(y+1)*m_Width+ x   ];

            m_Pout_G__m_R[y*m_Width+x] = static_cast<short>((R32_G + R34_G + R23_G + R43_G - G32_G + 2* G33_G - G34_G - G23_G + 2* G33_G - G43_G)/4);
        }
        m_Pout_H__m_R[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_H__m_R[y*m_Width+x], 0, Max_Intensity));
        m_Pout_V__m_R[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_V__m_R[y*m_Width+x], 0, Max_Intensity));
        m_Pout_G__m_R[y*m_Width+x] = static_cast<short>(CLIP_VAL(m_Pout_G__m_R[y*m_Width+x], 0, Max_Intensity));
    }

    return 0;
}


int CPUCameraProcessor::CPU_SAD (short *m_Pout_H__m_G, short *m_Pout_V__m_G, //Input, G_H, G_V component
             short *m_Pout_H__m_B, short *m_Pout_V__m_B, //Input, B_H, B_V component
             short *m_Pout_H__m_R, short *m_Pout_V__m_R,
             int m_Width, int m_Height,
             short *m_Pout_O__m_G,
             short *m_Pout_O__m_B,
             short *m_Pout_O__m_R)
             //unsigned char *pHVFlag8bpp)
             //short *m_Pout_G_o, short *m_Pout_B_o, short *m_Pout_R_o)
             //int* Debug_A, int* Debug_B)                 //Output, HV flag
{
    int DRG_H_H[9];
    int DGB_H_H[9];
    int DBR_H_H[9];

    int DRG_H_V[9];
    int DGB_H_V[9];
    int DBR_H_V[9];

    int DRG_V_H[9];
    int DGB_V_H[9];
    int DBR_V_H[9];

    int DRG_V_V[9];
    int DGB_V_V[9];
    int DBR_V_V[9];

    int HDSUM_H[8] = {0,0,0,0,0,0,0,0};
    int VDSUM_H[8] = {0,0,0,0,0,0,0,0};
    int HDSUM_V[8] = {0,0,0,0,0,0,0,0};
    int VDSUM_V[8] = {0,0,0,0,0,0,0,0};

    int Left = -4; Left;
    int Right = 5; Right;
    int out_index, in_index;

    for(int y=8; y < m_Height-8; y++)
    for(int x=8; x < m_Width-8; x++)
    {
        int dum = 0;
        if(x == (0+8) && y == (0+8))
            dum = 1;
        //if(x == (2902+8) && y == (759+8))

        int index = 0;

        for(int x_i = (x-4); x_i <= (x+4); x_i++)
        {
            DRG_H_H[index] = (m_Pout_H__m_R[y*m_Width+x_i]) - (m_Pout_H__m_G[y*m_Width+x_i]); //R_G
            DGB_H_H[index] = (m_Pout_H__m_G[y*m_Width+x_i]) - (m_Pout_H__m_B[y*m_Width+x_i]); //G_B
            DBR_H_H[index] = (m_Pout_H__m_B[y*m_Width+x_i]) - (m_Pout_H__m_R[y*m_Width+x_i]); //B_R
            index++;
        }

        index = 0;
        for(int y_i = (y-4); y_i <= (y+4); y_i++)
        {
            DRG_H_V[index] = (m_Pout_H__m_R[y_i*m_Width+x]) - (m_Pout_H__m_G[y_i*m_Width+x]); //R_G
            DGB_H_V[index] = (m_Pout_H__m_G[y_i*m_Width+x]) - (m_Pout_H__m_B[y_i*m_Width+x]); //G_B
            DBR_H_V[index] = (m_Pout_H__m_B[y_i*m_Width+x]) - (m_Pout_H__m_R[y_i*m_Width+x]); //B_R
            index++;
        }

        index = 0;
        for(int x_i = (x-4); x_i <= (x+4); x_i++)
        {
            DRG_V_H[index] = (m_Pout_V__m_R[y*m_Width+x_i]) - (m_Pout_V__m_G[y*m_Width+x_i]); //R_G
            DGB_V_H[index] = (m_Pout_V__m_G[y*m_Width+x_i]) - (m_Pout_V__m_B[y*m_Width+x_i]); //G_B
            DBR_V_H[index] = (m_Pout_V__m_B[y*m_Width+x_i]) - (m_Pout_V__m_R[y*m_Width+x_i]); //B_R
            index++;
        }

        index = 0;
        for(int y_i = (y-4); y_i <= (y+4); y_i++)
        {
            DRG_V_V[index] = (m_Pout_V__m_R[y_i*m_Width+x]) - (m_Pout_V__m_G[y_i*m_Width+x]); //R_G
            DGB_V_V[index] = (m_Pout_V__m_G[y_i*m_Width+x]) - (m_Pout_V__m_B[y_i*m_Width+x]); //G_B
            DBR_V_V[index] = (m_Pout_V__m_B[y_i*m_Width+x]) - (m_Pout_V__m_R[y_i*m_Width+x]); //B_R
            index++;
        }
        unsigned short HDSUM;
        HDSUM = static_cast<unsigned short>(abs(DRG_H_H[5] - DRG_H_H[4]) + abs(DGB_H_H[5] - DGB_H_H[4]) + abs(DBR_H_H[5] - DBR_H_H[4]));

        int index_plus;
        for(index = 0; index <= 7; index++)
        {
            index_plus = index + 1;
            HDSUM_H[index] = (abs(DRG_H_H[index_plus] - DRG_H_H[index]) + abs(DGB_H_H[index_plus] - DGB_H_H[index]) + abs(DBR_H_H[index_plus] - DBR_H_H[index]));
            VDSUM_H[index] = (abs(DRG_H_V[index_plus] - DRG_H_V[index]) + abs(DGB_H_V[index_plus] - DGB_H_V[index]) + abs(DBR_H_V[index_plus] - DBR_H_V[index]));
            HDSUM_V[index] = (abs(DRG_V_H[index_plus] - DRG_V_H[index]) + abs(DGB_V_H[index_plus] - DGB_V_H[index]) + abs(DBR_V_H[index_plus] - DBR_V_H[index]));
            VDSUM_V[index] = (abs(DRG_V_V[index_plus] - DRG_V_V[index]) + abs(DGB_V_V[index_plus] - DGB_V_V[index]) + abs(DBR_V_V[index_plus] - DBR_V_V[index]));
        }

        int HLSAD = 0; int HUSAD = 0; int VLSAD = 0; int VUSAD = 0;
        for(index = 0; index <= 3; index ++)
        {
            HLSAD += HDSUM_H[index];
            VLSAD += HDSUM_V[index];
            HUSAD += VDSUM_H[index];
            VUSAD += VDSUM_V[index];
        }

        int HRSAD = 0; int HDSAD = 0; int VRSAD = 0; int VDSAD = 0;
        for(index = 4; index <= 7; index ++)
        {
            HRSAD += HDSUM_H[index];
            VRSAD += HDSUM_V[index];
            HDSAD += VDSUM_H[index];
            VDSAD += VDSUM_V[index];
        }
        int shift_mincost = 1;//Shift_mincost is a state variable (default: 1).

        int Min_cost_H1 = (HLSAD < HRSAD)? HLSAD : HRSAD ;
        int Min_cost_H2 = (HUSAD < HDSAD)? HUSAD : HDSAD ;
        int Min_cost_H  = Min_cost_H1 + (Min_cost_H2>>shift_mincost);

        int Min_cost_V1 = (VUSAD < VDSAD)? VUSAD : VDSAD ;
        int Min_cost_V2 = (VLSAD < VRSAD)? VLSAD : VRSAD ;
        int Min_cost_V  = Min_cost_V1 + (Min_cost_V2>>shift_mincost);
#if 0
        pHVFlag8bpp[y*m_Width+x] = (Min_cost_H < Min_cost_V) ? 1 : 0;
#else
        out_index = (y-8)*(m_Width-16)+(x-8);
        in_index  = (y-0)*(m_Width-0 )+(x-0);
        m_Pout_O__m_G[out_index] = (Min_cost_H < Min_cost_V)? m_Pout_H__m_G[in_index] :
                                                              m_Pout_V__m_G[in_index] ;
        m_Pout_O__m_B[out_index] = (Min_cost_H < Min_cost_V)? m_Pout_H__m_B[in_index] :
                                                              m_Pout_V__m_B[in_index] ;
        m_Pout_O__m_R[out_index] = (Min_cost_H < Min_cost_V)? m_Pout_H__m_R[in_index] :
                                                              m_Pout_V__m_R[in_index] ;
#endif
    }

    return 0;
}

int CPUCameraProcessor::CPU_Decide_Average(short *R_A8,       // Restored average version of R component
                       short *G_A8,          // Restored average version of G component
                       short *B_A8,       // Restored average version of B component
                       int m_Width, int m_Height,
                       unsigned char *A,
                       unsigned char *updated_A,
                       short *m_Pout_O__m_R,
                       short *m_Pout_O__m_G,
                       short *m_Pout_O__m_B,
                       int bitdepth)                  // Flag to indicate using average version or not
{
    //int index;
    //int index01;
    //int index02;
    int Diff_Prec = bitdepth - 8;
    int w = m_Width;
    int h = m_Height; h;
    int out_index, in_index;

    int yp1, yp2, yp3, ym1, ym2;

    int dum;
    for(int y = 0; y < m_Height; y++)
    for(int x = 0; x < m_Width; x++)
    {
        updated_A[y*w+x] = A[y*w+x];
    }

    //for(int y = 0; y < m_Height; y++)
    //for(int x = 0; x < m_Width; x++)
    //read from interior part of input image
    for(int y = 8; y < (m_Height - 8); y++)
    for(int x = 8; x < (m_Width - 8 ); x++)
    {

        if(x == 967 && y == 10)
            dum = 1;

        yp1 = CLIP_VPOS_FLDRPT(y+1,m_Height);
        yp2 = CLIP_VPOS_FLDRPT(y+2,m_Height);
        yp3 = CLIP_VPOS_FLDRPT(y+3,m_Height);
        ym1 = CLIP_VPOS_FLDRPT(y-1,m_Height);
        ym2 = CLIP_VPOS_FLDRPT(y-2,m_Height);

        //Horizonal check (8-bit differences), maskH0
        if (x % 4 == 0)       // x modulo 4 = 0
        {
            if( ( (eval(G_A8, x-1, y, w)>>Diff_Prec) - (eval(G_A8, x  , y, w)>>Diff_Prec) ) * ( (eval(G_A8, x  , y, w)>>Diff_Prec) - (eval(G_A8, x+1, y, w)>>Diff_Prec) ) < 0 &&
                ( (eval(G_A8, x  , y, w)>>Diff_Prec) - (eval(G_A8, x+1, y, w)>>Diff_Prec) ) * ( (eval(G_A8, x+1, y, w)>>Diff_Prec) - (eval(G_A8, x+2, y, w)>>Diff_Prec) ) < 0 &&
                ( (eval(G_A8, x+1, y, w)>>Diff_Prec) - (eval(G_A8, x+2, y, w)>>Diff_Prec) ) * ( (eval(G_A8, x+2, y, w)>>Diff_Prec) - (eval(G_A8, x+3, y, w)>>Diff_Prec) ) < 0 &&
             abs( (eval(G_A8, x-1, y, w)>>Diff_Prec) - (eval(G_A8, x  , y, w)>>Diff_Prec) ) > Grn_imbalance_th                                    &&
             abs( (eval(G_A8, x  , y, w)>>Diff_Prec) - (eval(G_A8, x+1, y, w)>>Diff_Prec) ) > Grn_imbalance_th                                    &&
             abs( (eval(G_A8, x+1, y, w)>>Diff_Prec) - (eval(G_A8, x+2, y, w)>>Diff_Prec) ) > Grn_imbalance_th                                    &&
             abs( (eval(G_A8, x+2, y, w)>>Diff_Prec) - (eval(G_A8, x+3, y, w)>>Diff_Prec) ) > Grn_imbalance_th )
            {
               updated_A[y*w+(x  )] = AVG_False;
               updated_A[y*w+(x+1)] = AVG_False;
            }
        }

        else if (x % 4 == 2) // x modulo 4 = 2
        {
           if( ( (eval(G_A8, x-2, y, w)>>Diff_Prec)  - (eval(G_A8, x-1, y, w)>>Diff_Prec) ) * ( (eval(G_A8, x-1, y, w)>>Diff_Prec) - (eval(G_A8, x  , y, w)>>Diff_Prec) ) < 0 &&
               ( (eval(G_A8, x-1, y, w)>>Diff_Prec)  - (eval(G_A8, x  , y, w)>>Diff_Prec) ) * ( (eval(G_A8, x  , y, w)>>Diff_Prec) - (eval(G_A8, x+1, y, w)>>Diff_Prec) ) < 0 &&
               ( (eval(G_A8, x  , y, w)>>Diff_Prec)  - (eval(G_A8, x+1, y, w)>>Diff_Prec) ) * ( (eval(G_A8, x+1, y, w)>>Diff_Prec) - (eval(G_A8, x+2, y, w)>>Diff_Prec) ) < 0 &&
            abs( (eval(G_A8, x-2, y, w)>>Diff_Prec)  - (eval(G_A8, x-1, y, w)>>Diff_Prec) ) > Grn_imbalance_th                                     &&
            abs( (eval(G_A8, x-1, y, w)>>Diff_Prec)  - (eval(G_A8, x  , y, w)>>Diff_Prec) ) > Grn_imbalance_th                                     &&
            abs( (eval(G_A8, x  , y, w)>>Diff_Prec)  - (eval(G_A8, x+1, y, w)>>Diff_Prec) ) > Grn_imbalance_th                                     &&
            abs( (eval(G_A8, x+1, y, w)>>Diff_Prec)  - (eval(G_A8, x+2, y, w)>>Diff_Prec) ) > Grn_imbalance_th )
           {
               updated_A[y*w+(x  )] = AVG_False;
               updated_A[y*w+(x+1)] = AVG_False;
           }
        }


        //Vertical check (8-bit differences), maskV0
        if (y % 4 == 0)      // y modulo 4 = 0
        {
           if( ( (eval(G_A8, x, ym1, w)>>Diff_Prec) - (eval(G_A8, x, y  , w)>>Diff_Prec) ) * ( (eval(G_A8, x, y  , w)>>Diff_Prec) - (eval(G_A8, x, yp1, w)>>Diff_Prec) ) < 0 &&
               ( (eval(G_A8, x, y  , w)>>Diff_Prec) - (eval(G_A8, x, yp1, w)>>Diff_Prec) ) * ( (eval(G_A8, x, yp1, w)>>Diff_Prec) - (eval(G_A8, x, yp2, w)>>Diff_Prec) ) < 0 &&
               ( (eval(G_A8, x, yp1, w)>>Diff_Prec) - (eval(G_A8, x, yp2, w)>>Diff_Prec) ) * ( (eval(G_A8, x, yp2, w)>>Diff_Prec) - (eval(G_A8, x, yp3, w)>>Diff_Prec) ) < 0 &&
            abs( (eval(G_A8, x, ym1, w)>>Diff_Prec) - (eval(G_A8, x, y  , w)>>Diff_Prec) ) > Grn_imbalance_th                  &&
            abs( (eval(G_A8, x, y  , w)>>Diff_Prec) - (eval(G_A8, x, yp1, w)>>Diff_Prec) ) > Grn_imbalance_th                  &&
            abs( (eval(G_A8, x, yp1, w)>>Diff_Prec) - (eval(G_A8, x, yp2, w)>>Diff_Prec) ) > Grn_imbalance_th                  &&
            abs( (eval(G_A8, x, yp2, w)>>Diff_Prec) - (eval(G_A8, x, yp3, w)>>Diff_Prec) ) > Grn_imbalance_th )
           {
               updated_A[ y   *w+x] = AVG_False;
               updated_A[(y+1)*w+x] = AVG_False;
           }
        }

        else if (y % 4 == 2) // y modulo 4 = 2
        {
           if( ( (eval(G_A8, x, ym2, w)>>Diff_Prec) - (eval(G_A8, x, ym1, w)>>Diff_Prec) ) * ( (eval(G_A8, x, ym1, w)>>Diff_Prec) - (eval(G_A8, x, y  , w)>>Diff_Prec) ) < 0 &&
               ( (eval(G_A8, x, ym1, w)>>Diff_Prec) - (eval(G_A8, x, y  , w)>>Diff_Prec) ) * ( (eval(G_A8, x, y  , w)>>Diff_Prec) - (eval(G_A8, x, yp1, w)>>Diff_Prec) ) < 0 &&
               ( (eval(G_A8, x, y  , w)>>Diff_Prec) - (eval(G_A8, x, yp1, w)>>Diff_Prec) ) * ( (eval(G_A8, x, yp1, w)>>Diff_Prec) - (eval(G_A8, x, yp2, w)>>Diff_Prec) ) < 0 &&
            abs( (eval(G_A8, x, ym2, w)>>Diff_Prec) - (eval(G_A8, x, ym1, w)>>Diff_Prec) ) > Grn_imbalance_th                  &&
            abs( (eval(G_A8, x, ym1, w)>>Diff_Prec) - (eval(G_A8, x, y  , w)>>Diff_Prec) ) > Grn_imbalance_th                  &&
            abs( (eval(G_A8, x, y  , w)>>Diff_Prec) - (eval(G_A8, x, yp1, w)>>Diff_Prec) ) > Grn_imbalance_th                  &&
            abs( (eval(G_A8, x, yp1, w)>>Diff_Prec) - (eval(G_A8, x, yp2, w)>>Diff_Prec) ) > Grn_imbalance_th )
             {
               updated_A[ y   *w+x] = AVG_False;
               updated_A[(y+1)*w+x] = AVG_False;
           }
        }

        //The second condition is to check color difference between two colors in a pixel (8-bit diff):
        //mask0

        if (abs((eval(R_A8, x, y, w)>>Diff_Prec) - (eval(G_A8, x, y, w)>>Diff_Prec)) > AVG_COLOR_TH  ||
            abs((eval(G_A8, x, y, w)>>Diff_Prec) - (eval(B_A8, x, y, w)>>Diff_Prec)) > AVG_COLOR_TH  ||
            abs((eval(B_A8, x, y, w)>>Diff_Prec) - (eval(R_A8, x, y, w)>>Diff_Prec)) > AVG_COLOR_TH )
        {
            updated_A[y*w+x] = AVG_False;
        }

        out_index = (y-8)*(m_Width-16)+(x-8);
        in_index  = (y-0)*(m_Width- 0)+(x-0);

        if(updated_A[y*w+x] == AVG_True)
        {
            m_Pout_O__m_G[out_index] = G_A8[in_index];
            m_Pout_O__m_B[out_index] = B_A8[in_index];
            m_Pout_O__m_R[out_index] = R_A8[in_index];
        }

    }

    return 0;
}

int CPUCameraProcessor::Demosaic_CPU(CamInfo *dmi, unsigned short *, mfxU16)
{
    int Max_Intensity = ((1 << dmi->bitDepth) - 1);


    dmi->cpu_status = CPU_GoodPixelCheck(dmi->cpu_PaddedBayerImg, dmi->FrameWidth, dmi->FrameHeight, dmi->cpu_AVG_flag, (dmi->bitDepth - 8), dmi->enable_padd, (16 - dmi->bitDepth), false);
    if(dmi->cpu_status != 0)
        return -1;

    dmi->cpu_status = CPU_RestoreG(dmi->cpu_PaddedBayerImg, dmi->FrameWidth, dmi->FrameHeight, dmi->cpu_G_h, dmi->cpu_G_v, dmi->cpu_G_a, Max_Intensity);
    if(dmi->cpu_status != 0)
        return -1;

    dmi->cpu_status = CPU_RestoreBandR(dmi->cpu_PaddedBayerImg, dmi->FrameWidth, dmi->FrameHeight,
                                       dmi->cpu_G_h, dmi->cpu_G_v, dmi->cpu_G_a,
                                       dmi->cpu_R_h, dmi->cpu_R_v, dmi->cpu_R_a,
                                       dmi->cpu_B_h, dmi->cpu_B_v, dmi->cpu_B_a,
                                       Max_Intensity);
    if(dmi->cpu_status != 0)
        return -1;

    dmi->cpu_status = CPU_SAD(dmi->cpu_G_h, dmi->cpu_G_v,
                              dmi->cpu_B_h, dmi->cpu_B_v,
                              dmi->cpu_R_h, dmi->cpu_R_v,
                              dmi->FrameWidth, dmi->FrameHeight,
                              dmi->cpu_G_o , dmi->cpu_B_o , dmi->cpu_R_o);
    if(dmi->cpu_status != 0)
        return -1;

    dmi->cpu_status = CPU_Decide_Average(dmi->cpu_R_a, dmi->cpu_G_a, dmi->cpu_B_a,
                                         dmi->FrameWidth, dmi->FrameHeight,
                                         dmi->cpu_AVG_flag, dmi->cpu_AVG_new_flag ,
                                         dmi->cpu_R_o , dmi->cpu_G_o , dmi->cpu_B_o,
                                         dmi->bitDepth);

    if(dmi->cpu_status != 0)
        return -1;

    return 0;
}

int CPUCameraProcessor::CPU_CCM(unsigned short* R_i, unsigned short* G_i, unsigned short* B_i,
            int framewidth, int frameheight, int bitDepth, mfxF64 CCM[3][3])
{
    int MaxIntensity = (1 << (bitDepth)) - 1;
    int index;

    int tmpR, tmpG, tmpB;
#if 1
    unsigned short R, G, B;

    //float R, G, B, tmpR, tmpG, tmpB;
#else
    int tmpint;
    float R, G, B;
#endif

    int dum = 0;

    for (int y = 0;y < frameheight; y++)
    {
        for (int x = 0;x < framewidth; x++)
        {
            index = ( y * framewidth + x );

            if( y == 1 && x == 337)
                dum = 1;
#if 1
            R = R_i[index];
            G = G_i[index];
            B = B_i[index];

            tmpR  = (int)((float)R * CCM[0][0]);
            tmpR += (int)((float)G * CCM[0][1]);
            tmpR += (int)((float)B * CCM[0][2]);

            tmpG  = (int)((float)R * CCM[1][0]);
            tmpG += (int)((float)G * CCM[1][1]);
            tmpG += (int)((float)B * CCM[1][2]);

            tmpB  = (int)((float)R * CCM[2][0]);
            tmpB += (int)((float)G * CCM[2][1]);
            tmpB += (int)((float)B * CCM[2][2]);

#else
            R = (float)R_i[index];
            G = (float)G_i[index];
            B = (float)B_i[index];

            tmpint = (R * CCM[0][0]);
            tmpR   = tmpint;
            tmpint = (G * CCM[1][0]);
            tmpR  += tmpint;
            tmpint = (B * CCM[2][0]);
            tmpR  += tmpint;

            tmpint = (R * CCM[0][1]);
            tmpG   = tmpint;
            tmpint = (G * CCM[1][1]);
            tmpG  += tmpint;
            tmpint = (B * CCM[2][1]);
            tmpG  += tmpint;

            tmpint = (R * CCM[0][2]);
            tmpB   = tmpint;
            tmpint = (G * CCM[1][2]);
            tmpB  += tmpint;
            tmpint = (B * CCM[2][2]);
            tmpB  += tmpint;

#endif
            R_i[index] = static_cast<unsigned short>(CLIP_VAL(tmpR, 0, MaxIntensity));
            G_i[index] = static_cast<unsigned short>(CLIP_VAL(tmpG, 0, MaxIntensity));
            B_i[index] = static_cast<unsigned short>(CLIP_VAL(tmpB, 0, MaxIntensity));
        }
    }

    return 0;
}

int CPUCameraProcessor::CPU_Gamma_SKL(unsigned short* R_i, unsigned short* G_i, unsigned short* B_i,
                  //unsigned short* R_o, unsigned short* G_o, unsigned short* B_o,
                  int framewidth, int frameheight, int bitDepth,
                  mfxCamFwdGammaSegment* gamma_segment)
{
    int SrcPrecision = bitDepth;
    int outputprec = bitDepth;

    //float max_input_level  = float(1<<int(SrcPrecision))-1;
    //float max_output_level = float(1<<int(outputprec  ))-1;

    int max_input_level   = (int)(1<<int(SrcPrecision))-1; max_input_level;
    int max_output_level  = (int)(1<<int(outputprec))-1;

    int Interpolation_R = 0;
    int Interpolation_G = 0;
    int Interpolation_B = 0;
    int in_R, in_G, in_B;

#if 0 // C-model
    // N= 64
    if (input[15:0] < Point[0])
        Interpolation = Correct[0] * Input[15:0] / Point[0] ;
    else if (Input[15:0] >= Point[N-1])
        Interpolation = Correct[63] + (0xFFFF - Correct[63]) * (Input[15:0] - Point[63]) / (0xFFFF - Point[63])     ;
    else
    {
        //find i such that Point[i] <= Input[15:0] <Point[i+1];
        //Interpolation = Correct[i] + (Correct[i+1] - Correct[i]) * (Input[15:0] - Point[i]) / (Point[i+1] - Point[i]);
        for( i = 0; i < 64; i++)
        {
            if(Point[i] <= Input[15:0] <Point[i+1])
                Interpolation = Correct[i] + (Correct[i+1] - Correct[i]) * (Input[15:0] - Point[i]) / (Point[i+1] - Point[i]);
        }
    }
#endif

    int dum, ind;
    for (int y = 0;y < frameheight; y++)
    {
        for (int x = 0;x < framewidth; x++)
        {
            //if(y == 8 && x == 8)
            //    dum = 1;

            ind = (y * framewidth + x);
            if(ind == 8849376)
                dum = 1;

            in_R = R_i[ind];
            in_G = G_i[ind];
            in_B = B_i[ind];
            Interpolation_R  = R_i[ind];
            Interpolation_G  = G_i[ind];
            Interpolation_B  = B_i[ind];

            int r_ind = 0;
            int g_ind = 0;
            int b_ind = 0;
            int divide = 0;
            for(int i = 0; i < NUM_CONTROL_POINTS_SKL; i++)
            {
                //if( (OrgValue >= (init_gamma_point[Index] <<PWLShift) ) && (OrgValue < (init_gamma_point[Index+1] << PWLShift)) )
                //    Interpolation = ( (((OrgValue-(init_gamma_point[Index]<<PWLShift))*init_gamma_slope[Index])>>PWLF_prec) + (init_gamma_bias[Index]<<PWLShift));
                if( i < (NUM_CONTROL_POINTS_SKL - 1) )
                {
                    if((gamma_segment[i].Pixel <= in_R) && (in_R < gamma_segment[i+1].Pixel))
                    {
                        divide = ((gamma_segment[i+1].Pixel - gamma_segment[i].Pixel) == 0)? 1 : (gamma_segment[i+1].Pixel - gamma_segment[i].Pixel);
                        Interpolation_R = gamma_segment[i].Red + (gamma_segment[i+1].Red - gamma_segment[i].Red) * (in_R - gamma_segment[i].Pixel) / divide;
                        r_ind = i;
                    }
                    if((gamma_segment[i].Pixel <= in_G) && (in_G < gamma_segment[i+1].Pixel))
                    {
                        divide = ((gamma_segment[i+1].Pixel - gamma_segment[i].Pixel) == 0)? 1 : (gamma_segment[i+1].Pixel - gamma_segment[i].Pixel);
                        Interpolation_G = gamma_segment[i].Green + (gamma_segment[i+1].Green - gamma_segment[i].Green) * (in_G - gamma_segment[i].Pixel) / divide;
                        g_ind = i;
                    }
                    if((gamma_segment[i].Pixel <= in_B) && (in_B < gamma_segment[i+1].Pixel))
                    {
                        divide = ((gamma_segment[i+1].Pixel - gamma_segment[i].Pixel) == 0)? 1 : (gamma_segment[i+1].Pixel - gamma_segment[i].Pixel);
                        Interpolation_B = gamma_segment[i].Blue + (gamma_segment[i+1].Blue - gamma_segment[i].Blue) * (in_B - gamma_segment[i].Pixel) / divide;
                        b_ind = i;
                    }
                }
                else // i == (NUM_CONTROL_POINTS - 1)
                {
                    divide = ((max_output_level - gamma_segment[i].Pixel) == 0)? 1 : (max_output_level - gamma_segment[i].Pixel);
                    if(gamma_segment[NUM_CONTROL_POINTS_SKL - 1].Pixel <= in_R)
                        Interpolation_R = gamma_segment[i].Red + (max_output_level - gamma_segment[i].Red) * (in_R - gamma_segment[i].Pixel) / divide;
                    if(gamma_segment[NUM_CONTROL_POINTS_SKL - 1].Pixel <= in_G)
                        Interpolation_G = gamma_segment[i].Green + (max_output_level - gamma_segment[i].Green) * (in_G - gamma_segment[i].Pixel) / divide;
                    if(gamma_segment[NUM_CONTROL_POINTS_SKL - 1].Pixel <= in_B)
                        Interpolation_B = gamma_segment[i].Blue + (max_output_level - gamma_segment[i].Blue) * (in_B - gamma_segment[i].Pixel) / divide;
                }
            }

            R_i[ind] = static_cast<unsigned short>(CLIP_VAL(Interpolation_R, 0, ((1<<outputprec) -1)));
            G_i[ind] = static_cast<unsigned short>(CLIP_VAL(Interpolation_G, 0, ((1<<outputprec) -1)));
            B_i[ind] = static_cast<unsigned short>(CLIP_VAL(Interpolation_B, 0, ((1<<outputprec) -1)));
        }
    }

    return 0;
}

int CPUCameraProcessor::CPU_ARGB8Interleave(unsigned char* ARGB8, int OutWidth, int OutHeight, int OutPitch, int BitDepth, int BayerType,
                        short* R, short* G, short* B)
{

    int offset       = (BayerType == BAYER_GBRG || BayerType == BAYER_GRBG)? (OutHeight - 1) : 0;
    int multiplier = (BayerType == BAYER_GBRG || BayerType == BAYER_GRBG)? (          - 1) : 1;

    int out_index, in_index;
    for(int j = 0; j < OutHeight; j++) {
        for(int i = 0; i < OutWidth*4; i += 4) {

            in_index  = j *  OutWidth    + (i/4);
            out_index = (offset + (multiplier)*j) * (OutPitch) + i;
            ARGB8[out_index+0] = static_cast<unsigned char>(B[in_index] >> (BitDepth-8)); // >> 8;
            ARGB8[out_index+1] = static_cast<unsigned char>(G[in_index] >> (BitDepth-8)); // >> 8;
            ARGB8[out_index+2] = static_cast<unsigned char>(R[in_index] >> (BitDepth-8)); // >> 8;
            ARGB8[out_index+3] = 0;
        }
    }

    return 0;
}

int CPUCameraProcessor::CPU_ARGB16Interleave(unsigned short* ARGB16, int OutWidth, int OutHeight, int OutPitch, int BitDepth, int BayerType,
                         short* R, short* G, short* B)
{
    int offset       = (BayerType == BAYER_GBRG || BayerType == BAYER_GRBG)? (OutHeight - 1) : 0;
    int multiplier = (BayerType == BAYER_GBRG || BayerType == BAYER_GRBG)? (          - 1) : 1;

    int out_index, in_index;
    for(int j = 0; j < OutHeight; j++) {
        for(int i = 0; i < OutWidth*4; i += 4) {

            in_index  = j *  OutWidth    + (i/4);
            out_index = (offset + (multiplier)*j) * (OutPitch>>1) + i;
            ARGB16[out_index+0] = B[in_index] << (16-BitDepth); // << 0;
            ARGB16[out_index+1] = G[in_index] << (16-BitDepth); // << 0;
            ARGB16[out_index+2] = R[in_index] << (16-BitDepth); // << 0;
            ARGB16[out_index+3] = 0;
        }
    }

    return 0;
}

int CPUCameraProcessor::InitCamera_CPU(CamInfo *dmi, int InWidth, int InHeight, int BitDepth, bool enable_padd, int BayerType)
{
    int FrameWidth  = InWidth + 16;
    int FrameHeight = InHeight + 16;

    dmi->InWidth     = InWidth;
    dmi->InHeight    = InHeight;
    dmi->FrameWidth  = FrameWidth;
    dmi->FrameHeight = FrameHeight;
    dmi->bitDepth    = BitDepth;
    dmi->enable_padd = enable_padd;
    dmi->BayerType   = BayerType;

    dmi->cpu_Input          = new unsigned short[InWidth*InHeight];
    dmi->cpu_PaddedBayerImg = new unsigned short[FrameWidth*FrameHeight];
    dmi->cpu_AVG_flag = new unsigned char[FrameWidth*FrameHeight];
    dmi->cpu_AVG_new_flag = new unsigned char[FrameWidth*FrameHeight];
    dmi->cpu_HV_flag = new unsigned char[FrameWidth*FrameHeight];

    dmi->cpu_G_h = new short[FrameWidth*FrameHeight];
    dmi->cpu_B_h = new short[FrameWidth*FrameHeight];
    dmi->cpu_R_h = new short[FrameWidth*FrameHeight];

    dmi->cpu_G_v = new short[FrameWidth*FrameHeight];
    dmi->cpu_B_v = new short[FrameWidth*FrameHeight];
    dmi->cpu_R_v = new short[FrameWidth*FrameHeight];

    dmi->cpu_G_a = new short[FrameWidth*FrameHeight];
    dmi->cpu_B_a = new short[FrameWidth*FrameHeight];
    dmi->cpu_R_a = new short[FrameWidth*FrameHeight];

    dmi->cpu_G_c = new unsigned short[FrameWidth*FrameHeight];
    dmi->cpu_B_c = new unsigned short[FrameWidth*FrameHeight];
    dmi->cpu_R_c = new unsigned short[FrameWidth*FrameHeight];

    dmi->cpu_G_o = new short[InWidth*InHeight];
    dmi->cpu_B_o = new short[InWidth*InHeight];
    dmi->cpu_R_o = new short[InWidth*InHeight];

    /* output from fwd gamma correction */
    dmi->cpu_G_fgc_out = new short[InWidth*InHeight];
    dmi->cpu_B_fgc_out = new short[InWidth*InHeight];
    dmi->cpu_R_fgc_out = new short[InWidth*InHeight];

    return 0;
}

/* TODO - call this from correct place */
void CPUCameraProcessor::FreeCamera_CPU(CamInfo *dmi)
{
    if (!dmi)
        return;

    delete [] dmi->cpu_Input;
    delete [] dmi->cpu_PaddedBayerImg;
    delete [] dmi->cpu_AVG_flag;
    delete [] dmi->cpu_AVG_new_flag;
    delete [] dmi->cpu_HV_flag;

    delete [] dmi->cpu_G_h;
    delete [] dmi->cpu_B_h;
    delete [] dmi->cpu_R_h;

    delete [] dmi->cpu_G_v;
    delete [] dmi->cpu_B_v;
    delete [] dmi->cpu_R_v;

    delete [] dmi->cpu_G_a;
    delete [] dmi->cpu_B_a;
    delete [] dmi->cpu_R_a;

    delete [] dmi->cpu_G_c;
    delete [] dmi->cpu_B_c;
    delete [] dmi->cpu_R_c;

    delete [] dmi->cpu_G_o;
    delete [] dmi->cpu_B_o;
    delete [] dmi->cpu_R_o;

    delete [] dmi->cpu_G_fgc_out;
    delete [] dmi->cpu_B_fgc_out;
    delete [] dmi->cpu_R_fgc_out;
}