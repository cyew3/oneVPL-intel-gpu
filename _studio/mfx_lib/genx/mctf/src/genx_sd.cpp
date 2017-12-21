/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2018 Intel Corporation. All Rights Reserved.
//
*/


#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#include <cm/cm.h>
#include <cm/cmtl.h>
#include "../include/genx_me_common.h"
#include "../include/genx_blend_mc.h"
#include "../include/genx_sd_common.h"

#if !defined(target_gen7_5) && !defined(target_gen8) && !defined(target_gen9) && !defined(target_gen9_5) && !defined(target_gen10) && !defined(CMRT_EMU)
#error One of macro should be defined: target_gen7_5, target_gen8, target_gen9, target_gen9_5, target_gen10
#endif

#ifdef target_gen7_5
typedef matrix<uchar, 3, 32> UniIn;
#else
typedef matrix<uchar, 4, 32> UniIn;
#endif

extern "C" _GENX_MAIN_
void SpatialDenoiser_8x8(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC, SurfaceIndex SURF_OUT, uint start_xy) {
	vector<ushort, 2>
		start_mbXY = start_xy;
	uint
		mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0],
		mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1],
		x    = mbX * 8,
		y    = mbY * 8;
	vector<uchar, 96> control;
    read(SURF_CONTROL, 0, control);
	ushort width  = control.format<ushort>()[30];
	ushort height = control.format<ushort>()[31];
	short th      = control.format<short>()[32];
    ushort picWidthInMB  = width >> 4;
	ushort picHeightInMB = height >> 4;
    uint  mbIndex       = picWidthInMB * mbY + mbX;

	matrix<uchar, 12, 12>
		src  = 0;
	matrix<uchar, 8, 8>
		out = 0;
	read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x - 2, y - 2, src);
	matrix<uint, 4, 4>
		Disp = DispersionCalculator(src);
	
	matrix<float, 4, 4>
		f_disp = Disp,
		h1 = 0.0,
		h2 = 0.0,
		hh = 0.0,
		k0 = 0.0,
		k1 = 0.0,
		k2 = 0.0;
	h1 = cm_exp(-(f_disp / (th / 10)));
	f_disp = Disp * 2;
	h2 = cm_exp(-(f_disp / (th / 10)));
	hh = 1 + 4 * (h1 + h2);
	k0 = 1/hh;
	k1 = h1/hh;
	k2 = h2/hh;

#pragma unroll
	for(uint i = 0; i < 4; i++) {
#pragma unroll
		for(uint j = 0; j < 4; j++) {
			out.select<2,1,2,1>(i * 2, j * 2) = (src.select<2,1,2,1>(2 + 2 * i, 2 + 2 * j) * k0(i,j)) + (
										        (src.select<2,1,2,1>(2 + 2 * i, 1 + 2 * j) + src.select<2,1,2,1>(2 + 2 * i, 3 + 2 * j) + src.select<2,1,2,1>(1 + 2 * i, 2 + 2 * j) + src.select<2,1,2,1>(3 + 2 * i, 2 + 2 * j))*k1(i,j)) + (
										        (src.select<2,1,2,1>(1 + 2 * i, 1 + 2 * j) + src.select<2,1,2,1>(1 + 2 * i, 3 + 2 * j) + src.select<2,1,2,1>(3 + 2 * i, 1 + 2 * j) + src.select<2,1,2,1>(3 + 2 * i, 3 + 2 * j))*k2(i,j)) + 0.5;
		}
	}

	write_plane(SURF_OUT, GENX_SURFACE_Y_PLANE, x, y, out);
}


extern "C" _GENX_MAIN_
void SpatialDenoiser_8x8_NV12(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC, SurfaceIndex SURF_OUT, uint start_xy) {
	vector<ushort, 2>
		start_mbXY = start_xy;
	uint
		mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0],
		mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1],
		x    = mbX * 8,
		y    = mbY * 8,
        xch  = mbX * 8,
        ych  = mbY * 4;
	vector<uchar, 96> control;
    read(SURF_CONTROL, 0, control);
	ushort width  = control.format<ushort>()[30];
	ushort height = control.format<ushort>()[31];
    short  Th     = control.format<short>()[32];
    short  sTh    = control.format<short>()[35];
    ushort picWidthInMB  = width >> 4;
	ushort picHeightInMB = height >> 4;
    uint  mbIndex       = picWidthInMB * mbY + mbX;

	matrix<uchar, 12, 12>
		src = 0;
    matrix<uchar, 6, 12>
        scm = 0;
	matrix<uchar, 8, 8>
		out = 0;
    matrix<uchar, 4, 8>
		och = 0;
    if(sTh > 0) {
	    read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE , x   - 2, y   - 2, src);
        read_plane(SURF_SRC, GENX_SURFACE_UV_PLANE, xch - 2, ych - 1, scm);
	    matrix<uint, 4, 4>
		    Disp = DispersionCalculator(src);

	    matrix<float, 4, 4>
		    f_disp = Disp,
		    h1 = 0.0,
		    h2 = 0.0,
		    hh = 0.0,
		    k0 = 0.0,
		    k1 = 0.0,
		    k2 = 0.0;
	    h1 = cm_exp(-(f_disp / sTh));
	    f_disp = Disp * 2;
	    h2 = cm_exp(-(f_disp / sTh));
	    hh = 1 + 4 * (h1 + h2);
	    k0 = 1/hh;
	    k1 = h1/hh;
	    k2 = h2/hh;

    #pragma unroll
	    for(uint i = 0; i < 4; i++) {
    #pragma unroll
		    for(uint j = 0; j < 4; j++) {
			    out.select<2,1,2,1>(i * 2, j * 2) = (src.select<2,1,2,1>(2 + 2 * i, 2 + 2 * j) * k0(i,j)) + (
										            (src.select<2,1,2,1>(2 + 2 * i, 1 + 2 * j) + src.select<2,1,2,1>(2 + 2 * i, 3 + 2 * j) + src.select<2,1,2,1>(1 + 2 * i, 2 + 2 * j) + src.select<2,1,2,1>(3 + 2 * i, 2 + 2 * j))*k1(i,j)) + (
										            (src.select<2,1,2,1>(1 + 2 * i, 1 + 2 * j) + src.select<2,1,2,1>(1 + 2 * i, 3 + 2 * j) + src.select<2,1,2,1>(3 + 2 * i, 1 + 2 * j) + src.select<2,1,2,1>(3 + 2 * i, 3 + 2 * j))*k2(i,j)) + 0.5;
		    }
	    }

        matrix<float, 4, 8>
            k0c = 0.0,
            k1c = 0.0,
            k2c = 0.0;
        k0c.select<4,1,4,2>(0,0) = k0c.select<4,1,4,2>(0,1) = k0;
        k1c.select<4,1,4,2>(0,0) = k1c.select<4,1,4,2>(0,1) = k1;
        k2c.select<4,1,4,2>(0,0) = k2c.select<4,1,4,2>(0,1) = k2;

        och = (scm.select<4,1,8,1>(1,2) * k0c) + (
              (scm.select<4,1,8,1>(1,0) + scm.select<4,1,8,1>(1,4) + scm.select<4,1,8,1>(0,2) + scm.select<4,1,8,1>(2,2)) * k1c) + (
              (scm.select<4,1,8,1>(0,0) + scm.select<4,1,8,1>(0,4) + scm.select<4,1,8,1>(2,0) + scm.select<4,1,8,1>(2,4)) * k2c) + 0.5;
    } else {
        read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE , x, y, out);
        read_plane(SURF_SRC, GENX_SURFACE_UV_PLANE, xch, ych, och);
    }
	write_plane(SURF_OUT, GENX_SURFACE_Y_PLANE , x  , y  , out);
    write_plane(SURF_OUT, GENX_SURFACE_UV_PLANE, xch, ych, och);
}

extern "C" _GENX_MAIN_
void SpatialDenoiser_8x8_YV12(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC, SurfaceIndex SURF_OUT, uint start_xy) {
	vector<ushort, 2>
		start_mbXY = start_xy;
	uint
		mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0],
		mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1];
    
	vector<uchar, 96> control;
    read(SURF_CONTROL, 0, control);
	ushort
        width  = control.format<ushort>()[30],
        height = control.format<ushort>()[31];
	short
        th     = control.format<short>()[32];
    ushort
        picWidthInMB  = width >> 4,
        picHeightInMB = height >> 4;
    uint
        mbIndex       = picWidthInMB * mbY + mbX;
    uint
        offsetY       = height >> 4;
    uint
		x   = mbX << 3,
		y   = mbY << 3,
        xcu = mbX << 2,
        ycu = mbY << 2,
        xcv = mbX << 2,
        ycv = offsetY + (mbY << 2);

	matrix<uchar, 12, 12>
		src = 0;
    matrix<uchar, 6, 6>
        scu = 0,
        scv = 0;
	matrix<uchar, 8, 8>
		out = 0;
    matrix<uchar, 4, 4>
		ocu = 0,
        ocv = 0;
	read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE , x   - 2, y   - 2, src);
    read_plane(SURF_SRC, GENX_SURFACE_UV_PLANE, xcu - 1, ycu - 1, scu);
    read_plane(SURF_SRC, GENX_SURFACE_UV_PLANE, xcv - 1, ycv - 1, scv);
	matrix<uint, 4, 4>
		Disp = DispersionCalculator(src);
	matrix<float, 4, 4>
		f_disp = Disp,
		h1 = 0.0,
		h2 = 0.0,
		hh = 0.0,
		k0 = 0.0,
		k1 = 0.0,
		k2 = 0.0;
	h1 = cm_exp(-(f_disp / (th / 10)));
	f_disp = Disp * 2;
	h2 = cm_exp(-(f_disp / (th / 10)));
	hh = 1 + 4 * (h1 + h2);
	k0 = 1/hh;
	k1 = h1/hh;
	k2 = h2/hh;

#pragma unroll
	for(uint i = 0; i < 4; i++) {
#pragma unroll
		for(uint j = 0; j < 4; j++) {
			out.select<2,1,2,1>(i * 2, j * 2) = (src.select<2,1,2,1>(2 + 2 * i, 2 + 2 * j) * k0(i,j)) + (
										        (src.select<2,1,2,1>(2 + 2 * i, 1 + 2 * j) + src.select<2,1,2,1>(2 + 2 * i, 3 + 2 * j) + src.select<2,1,2,1>(1 + 2 * i, 2 + 2 * j) + src.select<2,1,2,1>(3 + 2 * i, 2 + 2 * j))*k1(i,j)) + (
										        (src.select<2,1,2,1>(1 + 2 * i, 1 + 2 * j) + src.select<2,1,2,1>(1 + 2 * i, 3 + 2 * j) + src.select<2,1,2,1>(3 + 2 * i, 1 + 2 * j) + src.select<2,1,2,1>(3 + 2 * i, 3 + 2 * j))*k2(i,j)) + 0.5;
		}
	}

    ocu = (scu.select<4,1,4,1>(1,1) * k0) + (
          (scu.select<4,1,4,1>(1,0) + scu.select<4,1,4,1>(1,2) + scu.select<4,1,4,1>(0,1) + scu.select<4,1,4,1>(2,1)) * k1) + (
          (scu.select<4,1,4,1>(0,0) + scu.select<4,1,4,1>(0,2) + scu.select<4,1,4,1>(2,0) + scu.select<4,1,4,1>(2,2)) * k2) + 0.5;

    ocv = (scv.select<4,1,4,1>(1,1) * k0) + (
          (scv.select<4,1,4,1>(1,0) + scv.select<4,1,4,1>(1,2) + scv.select<4,1,4,1>(0,1) + scv.select<4,1,4,1>(2,1)) * k1) + (
          (scv.select<4,1,4,1>(0,0) + scv.select<4,1,4,1>(0,2) + scv.select<4,1,4,1>(2,0) + scv.select<4,1,4,1>(2,2)) * k2) + 0.5;

	write_plane(SURF_OUT, GENX_SURFACE_Y_PLANE , x  , y  , out);
    write_plane(SURF_OUT, GENX_SURFACE_UV_PLANE, xcu, ycu, ocu);
    write_plane(SURF_OUT, GENX_SURFACE_UV_PLANE, xcv, ycv, ocv);
}

extern "C" _GENX_MAIN_
void SpatialDenoiser_4x4(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC, SurfaceIndex SURF_OUT, uint start_xy) {
	vector<ushort, 2>
		start_mbXY = start_xy;
	uint
		mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0],
		mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1],
		x    = mbX * 4,
		y    = mbY * 4;
	vector<uchar, 96> control;
    read(SURF_CONTROL, 0, control);
	ushort width  = control.format<ushort>()[30];
	ushort height = control.format<ushort>()[31];
	short th      = control.format<short>()[32] / 10;
    ushort picWidthInMB  = width >> 4;
	ushort picHeightInMB = height >> 4;
    uint  mbIndex       = picWidthInMB * mbY + mbX;

	matrix<uchar, 6, 6>
		src  = 0;
	matrix<uchar, 4, 4>
		out = 0;
	read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x - 1, y - 1, src);
	ushort
		mean4x4 = cm_sum<ushort>(src.select<4,1,4,1>(1,1));
	mean4x4 >>= 4;

	matrix<uint, 2, 2>
		Disp = 0;
	matrix<ushort, 4, 4>
		test2 = 0;
	int4
		data = 0;

	test2 = cm_abs<ushort>(src.select<4,1,4,1>(0,0) - mean4x4);
	test2 *= test2;

	for(int i = 0; i < 2; i++)
		for(int j = 0; j < 2; j++)
			Disp(i,j) = cm_sum<uint>(test2.select<2,1,2,1>(i * 2, j * 2));

	Disp = Disp >> 4;
	matrix<float, 2, 2>
		f_disp = Disp,
		h1 = 0.0,
		h2 = 0.0,
		hh = 0.0,
		k0 = 0.0,
		k1 = 0.0,
		k2 = 0.0;
	h1 = cm_exp(-(f_disp / th));
	f_disp = Disp * 2;
	h2 = cm_exp(-(f_disp / th));
	hh = 1 + 4 * (h1 + h2);
	k0 = 1/hh;
	k1 = h1/hh;
	k2 = h2/hh;

	for(uint i = 0; i < 2; i++) {
		for(uint j = 0; j < 2; j++) {
			out.select<2,1,2,1>(i * 2, j * 2) = (src.select<2,1,2,1>(1 + 2 * i, 1 + 2 * j) * k0(i,j)) + (
										        (src.select<2,1,2,1>(1 + 2 * i,     2 * j) + src.select<2,1,2,1>(1 + 2 * i, 2 + 2 * j) + src.select<2,1,2,1>(    2 * i, 1 + 2 * j) + src.select<2,1,2,1>(2 + 2 * i, 1 + 2 * j))*k1(i,j)) + (
										        (src.select<2,1,2,1>(    2 * i,     2 * j) + src.select<2,1,2,1>(    2 * i, 2 + 2 * j) + src.select<2,1,2,1>(2 + 2 * i,     2 * j) + src.select<2,1,2,1>(2 + 2 * i, 2 + 2 * j))*k2(i,j)) + 0.5;
		}
	}
	write_plane(SURF_OUT, GENX_SURFACE_Y_PLANE, x, y, out);
}