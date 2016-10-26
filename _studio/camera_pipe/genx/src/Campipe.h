//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016 Intel Corporation. All Rights Reserved.
//

//#define	big_pix_th   40

#define GOOD_PIXEL_TH		 5
#define SHIFT_MINCOST		 1
#define AVG_COLOR_TH	   255

#define Grn_imbalance_th	 1
#define	GOOD_INTENSITY_TH   10
#define NUM_GOOD_PIXEL_TH   35
#define NUM_BIG_PIXEL_TH    10


#define AVG_True  2
#define AVG_False 1



#define DISPLAY_EXECUTIONTIME 1
#define DISP_API_PERF 1
#define CPU_CHECK 1

#define BR32_W 12
#define NUM_CONTROL_POINTS 64

#define BGGR 0
#define RGGB 1
#define GRBG 2
#define GBRG 3

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

const unsigned short init_gamma_point_14[64]   
= {0,255,510,765,1020,1275,1530,1785,2040,2295,2550,2805,3060,3315,3570,3825,4080,
   4335,4590,4845,5100,5355,5610,5865,6120,6375,6630,6885,7140,7395,7650,7905,8160,
   8415,8670,8925,9180,9435,9690,9945,10200,10455,10710,10965,11220,11475,11730,11985,
   12240,12495,12750,13005,13260,13515,13770,14025,14280,14535,14790,15045,15300,15555,
   15810,16065};
const unsigned short init_gamma_correct_14[64] 
= {0,2469,3384,4069,4637,5132,5576,5981,6355,6704,7033,7345,7641,7924,8196,8457,8709,8952,
   9187,9416,9638,9854,10065,10270,10471,10667,10859,11047,11231,11412,11589,11763,11934,
   12102,12267,12430,12590,12748,12904,13057,13208,13357,13504,13649,13793,13934,14074,14213,
   14349,14484,14618,14750,14881,15010,15138,15265,15391,15515,15638,15760,15881,16001,16120,16237};

const unsigned short init_gamma_point_10[64] =
     {   0,  94, 104, 114, 124, 134, 144, 154, 159, 164, 169, 174, 179, 184, 194, 199,
      204, 209, 214, 219, 224, 230, 236, 246, 256, 266, 276, 286, 296, 306, 316, 326,
      336, 346, 356, 366, 376, 386, 396, 406, 416, 426, 436, 446, 456, 466, 476, 486,
      496, 516, 526, 536, 546, 556, 566, 576, 586, 596, 606, 616, 626, 636, 646, 1022 };
//{ 0, 15, 30, 45, 60, 75, 90, 105, 120, 135, 150, 165, 180, 195, 210, 225, 240, 265, 280, 295,       // 20
//310, 325, 340, 355, 370, 385, 400, 415, 430, 445, 460, 475, 490, 505, 520, 535, 550, 565, 580, 595, // 20
//610, 625, 640, 665, 680, 695, 710, 725, 740, 755, 770, 785, 800, 815, 830, 845, 860, 875, 890, 905, // 20
//920, 935, 950, 965};																				// 4
const unsigned short init_gamma_correct_10[64] =
    {   0,   4,  20,  37,  56,  75,  96, 117, 128, 140, 150, 161, 171, 180, 198, 207,
      216, 224, 232, 240, 249, 258, 268, 283, 298, 310, 329, 344, 359, 374, 389, 404,
      420, 435, 451, 466, 482, 498, 515, 531, 548, 565, 582, 599, 617, 635, 653, 671,
      690, 729, 749, 769, 790, 811, 832, 854, 876, 899, 922, 945, 969, 994, 1015,1019 };


const float init_ccm_table[9] = { 0.70130002f, -0.14080000f, -0.063500002f, -0.52679998f,
                                  1.2902000f, 0.26400000f, -0.14700000f, 0.28009999f, 0.73790002f};