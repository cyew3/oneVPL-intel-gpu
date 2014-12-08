/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_cpu.h

\* ****************************************************************************** */


#ifndef _MFX_CAMERA_PLUGIN_CPU_H
#define _MFX_CAMERA_PLUGIN_CPU_H

#include "Campipe.h"

#define NUM_CONTROL_POINTS_SKL 64


typedef struct _CamInfo {
    int cpu_status;

    int InWidth;
    int InHeight;
    int FrameWidth;
    int FrameHeight;
    int bitDepth;
    bool enable_padd;
    int BayerType;
    unsigned short* cpu_Input;
    unsigned short* cpu_PaddedBayerImg;
    unsigned char* cpu_AVG_flag;
    unsigned char* cpu_AVG_new_flag;
    unsigned char* cpu_HV_flag;

    short* cpu_G_h; short* cpu_B_h; short* cpu_R_h;
    short* cpu_G_v; short* cpu_B_v; short* cpu_R_v;
    short* cpu_G_a; short* cpu_B_a; short* cpu_R_a;
    unsigned short* cpu_G_c; unsigned short* cpu_B_c; unsigned short* cpu_R_c;
    short* cpu_G_o; short* cpu_B_o; short* cpu_R_o;

    short* cpu_G_fgc_in;  short* cpu_B_fgc_in;  short* cpu_R_fgc_in;
    short* cpu_G_fgc_out; short* cpu_B_fgc_out; short* cpu_R_fgc_out;
} CamInfo;

int InitCamera_CPU(CamInfo *dmi, int InWidth, int InHeight, int BitDepth, bool enable_padd, int BayerType);
void FreeCamera_CPU(CamInfo *dmi);

int CPU_Padding_16bpp(unsigned short* bayer_original, //input  of Padding module, bayer image of 16bpp format
                      unsigned short* bayer_input,    //output of Padding module, Paddded bayer image of 16bpp format
                      int width,                      //input framewidth 
                      int height,                     //input frameheight
                      int bitDepth);

int CPU_BLC(unsigned short* Bayer,
            int PaddedWidth, int PaddedHeight, int bitDepth, int BayerType,
            short B_amount, short Gtop_amount, short Gbot_amount, short R_amount, bool firstflag);

int CPU_WB(unsigned short* Bayer,
           int PaddedWidth, int Paddedheight, int bitDepth, int BayerType,
           float B_scale, float Gtop_scale, float Gbot_scale, float R_scale, bool firstflag);

int Demosaic_CPU(CamInfo *dmi, unsigned short *BayerImg, mfxU16 pitch);

int CPU_CCM(unsigned short* R_i, unsigned short* G_i, unsigned short* B_i,
            int framewidth, int frameheight, int bitDepth, mfxF64 CCM[3][3]);

int CPU_Gamma_SKL(unsigned short* R_i, unsigned short* G_i, unsigned short* B_i,
                  int framewidth, int frameheight, int bitDepth,
                  unsigned short* Correct,         // Pointer to 64 Words
                  unsigned short* Point);          // Pointer to 64 Words

int CPU_ARGB8Interleave(unsigned char* ARGB8, int OutWidth, int OutHeight, int OutPitch, int BitDepth, int BayerType,
                        short* R, short* G, short* B);

int CPU_ARGB16Interleave(unsigned short* ARGB16, int OutWidth, int OutHeight, int OutPitch, int BitDepth, int BayerType,
                         short* R, short* G, short* B);
int CPU_Bufferflip(unsigned short* buffer,
                   int width, int height, int bitdepth, bool firstflag);
#endif  /* _MFX_CAMERA_PLUGIN_CPU_H */
