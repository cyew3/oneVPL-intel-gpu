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

#define WR_BLK_HEIGHT 8
#define WR_BLK_WIDTH  8
#define RD_BLK_WIDTH  16

#define    big_pix_th   40

#define m_Good_Pixel_Th      5
#define    good_pix_th          5

#define m_Num_Big_Pix_TH    35
#define    num_big_pix_th      35

#define    num_good_pix_th     10
#define m_Good_Intensity_TH 10

#define    good_intensity_TH   10
#define m_Num_Good_Pix_TH   10

//#define Max_Intensity 16383
//#define MAX_INTENSITY 16383

//#define m_SrcPrecision   14
//#define SrcPrecision     14

#define AVG_True  2
#define AVG_False 1
#define Grn_imbalance_th 0
#define NORMALWALKER 0
#define Average_Color_TH 100

#define Grn_imbalance_th 0

#define m_Bad_TH1    100
#define Bad_TH1        100
#define m_Bad_TH2   175
#define Bad_TH2     175
#define m_Bad_TH3   10
#define Bad_TH3     10

#define NOT_SKL 0

#define DISPLAY_EXECUTIONTIME 1
#define DISP_API_PERF 0
#define CPU_CHECK 1

#define BR32_W 12

#ifdef CMRT_EMU

extern "C" void
Padding_16bpp(SurfaceIndex ibuf, SurfaceIndex obuf,
              int threadwidth  , int threadheight,
              int InFrameWidth , int InFrameHeight,
              int bitDepth);

extern "C" void
GOOD_PIXEL_CHECK(SurfaceIndex ibuf ,
                 SurfaceIndex obuf0, SurfaceIndex obuf1,
                 int shift);

extern "C"  void
RESTOREG(SurfaceIndex ibufBayer,
         SurfaceIndex ibuf0, SurfaceIndex ibuf1,
         SurfaceIndex obufHOR, SurfaceIndex obufVER, SurfaceIndex obufAVG,
         SurfaceIndex obuf, int x , int y, int wr_x, int wr_y, unsigned short max_intensity);

extern "C"  void
GEN_AVGMASK(SurfaceIndex ibufBayer,
            SurfaceIndex ibuf0, SurfaceIndex ibuf1,
            SurfaceIndex obufHOR, SurfaceIndex obufVER, SurfaceIndex obufAVG,
            SurfaceIndex obuf, int x , int y, unsigned short max_intensity);

extern "C"  void
RESTOREBandR(SurfaceIndex ibufBayer,
             SurfaceIndex ibufGHOR, SurfaceIndex ibufGVER, SurfaceIndex ibufGAVG,
             SurfaceIndex obufBHOR, SurfaceIndex obufBVER, SurfaceIndex obufBAVG,
             SurfaceIndex obufRHOR, SurfaceIndex obufRVER, SurfaceIndex obufRAVG,
             SurfaceIndex ibufAVGMASK,
             int x , int y, int wr_x, int wr_y, unsigned short max_intensity);

extern "C"  void
RESTORERGB(SurfaceIndex ibufBayer,
           SurfaceIndex obufGHOR, SurfaceIndex obufGVER, SurfaceIndex obufGAVG,
           SurfaceIndex obufBHOR, SurfaceIndex obufBVER, SurfaceIndex obufBAVG,
           SurfaceIndex obufRHOR, SurfaceIndex obufRVER, SurfaceIndex obufRAVG,
           SurfaceIndex ibufAVGMASK,
           int x , int y, unsigned short max_intensity);

extern "C"  void
SAD(SurfaceIndex ibufRHOR, SurfaceIndex ibufGHOR, SurfaceIndex ibufBHOR,
    SurfaceIndex ibufRVER, SurfaceIndex ibufGVER, SurfaceIndex ibufBVER,
    int x , int y, int wr_x, int wr_y,
    SurfaceIndex R_c , SurfaceIndex G_c , SurfaceIndex B_c);


extern "C"  void
DECIDE_AVG(SurfaceIndex ibufRAVG, SurfaceIndex ibufGAVG, SurfaceIndex ibufBAVG,
           SurfaceIndex ibufAVG_Flag,
           SurfaceIndex R_c , SurfaceIndex G_c , SurfaceIndex B_c, int wr_x, int wr_y);


#endif

#ifdef CMRT_EMU
#define WD     12
#define WIDTH  6
#define W 15
#else
#define WD 16
#define WIDTH  8
#define W 16
#endif

#ifndef CLIP_VPOS_FLDRPT        // Vertical position based on field-based pixel repetition
#define CLIP_VPOS_FLDRPT(val, _height_) (((val)<0) ? ((val)*(-1)) : (((val)>=(_height_)) ? ((_height_-1)-(val-_height_+1)) : (val)))
#endif //#ifndef CLIP_VPOS_FLDRPT

#ifndef CLIP_VAL
#define CLIP_VAL(val, _min_, _max_) (((val)<(_min_)) ? (_min_) : (((val)>(_max_)) ? (_max_) : (val)))
#endif //#ifndef CLIP_VAL

short inline eval (short *p, int x, int y, int pitch)
{
    int xin;

    xin = ((x < 0) ? (x*(-1)) : ((x >= pitch ) ? ((pitch -1)-(x-pitch +1)) : x));

    return p[y*pitch+xin];
}

typedef struct _CamInfo {
    int cpu_status;

    int InWidth;
    int InHeight;
    int FrameWidth;
    int FrameHeight;
    int bitDepth;
    bool enable_padd;
    int BayerType;

    short* cpu_PaddedBayerImg;
    unsigned char* cpu_AVG_flag;
    unsigned char* cpu_AVG_new_flag;
    unsigned char* cpu_HV_flag;

    short* cpu_G_h; short* cpu_B_h; short* cpu_R_h;
    short* cpu_G_v; short* cpu_B_v; short* cpu_R_v;
    short* cpu_G_a; short* cpu_B_a; short* cpu_R_a;
    short* cpu_G_c; short* cpu_B_c; short* cpu_R_c;
    short* cpu_G_o; short* cpu_B_o; short* cpu_R_o;

    short* cpu_G_fgc_in;  short* cpu_B_fgc_in;  short* cpu_R_fgc_in;
    short* cpu_G_fgc_out; short* cpu_B_fgc_out; short* cpu_R_fgc_out;
} CamInfo;

int InitCamera_CPU(CamInfo *dmi, int InWidth, int InHeight, int BitDepth, bool enable_padd, int BayerType);
void FreeCamera_CPU(CamInfo *dmi);

int Demosaic_CPU(CamInfo *dmi, unsigned short *BayerImg, mfxU16 pitch);
int FwdGammaCorr_CPU(CamInfo *dmi, unsigned short* Correct, unsigned short* Point, int numPoints);
void ConvertARGB8_CPU(CamInfo *dmi, unsigned char *outBuf, int useFGC);
void ConvertARGB16_CPU(CamInfo *dmi, unsigned short *outBuf, int useFGC);

#endif  /* _MFX_CAMERA_PLUGIN_CPU_H */
