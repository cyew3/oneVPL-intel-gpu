//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//
#if defined(_WIN32) || defined(_WIN64)
#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#include "cm/cm.h"
#include <cm/cmtl.h>
//#include <cm/genx_vme.h>

#define INTERDATA_SIZE_SMALL    8
#define INTERDATA_SIZE_BIG      64   // 32x32 and 64x64 blocks
#define MVDATA_SIZE             4    // mfxI16Pair
#define SINGLE_MVDATA_SIZE      2    // mfxI16Pair
#define MBDIST_SIZE             64   // 16*mfxU32
#define DIST_SIZE               4
#define RF_DECISION_LEVEL       10

#define SLICE(VEC, FROM, HOWMANY, STEP) ((VEC).select<HOWMANY, STEP>(FROM))
#define SLICE1(VEC, FROM, HOWMANY) SLICE(VEC, FROM, HOWMANY, 1)
#define SELECT_N_ROWS(m, from, nrows) m.select<nrows, 1, m.COLS, 1>(from)
#define SELECT_N_COLS(m, from, ncols) m.select<m.ROWS, 1, ncols, 1>(0, from)
#define OUT_BLOCK       16  // output pixels computed per thread

/*----------------------------------------------------------------------*/
#define ROUND_UP(offset, round_to)   ( ( (offset) + (round_to) - 1) &~ ((round_to) - 1 ))
#define ROUND_DOWN(offset, round_to)   ( (offset) &~ ( ( round_to) - 1 ) )

#define BLOCK_PIXEL_WIDTH    (32)
#define BLOCK_HEIGHT        (8)
#define BLOCK_HEIGHT_NV12   (4)

#define SUB_BLOCK_PIXEL_WIDTH (8)
#define SUB_BLOCK_HEIGHT      (8)
#define SUB_BLOCK_HEIGHT_NV12 (4)

#define BLOCK_WIDTH                        (64)
#define PADDED_BLOCK_WIDTH                (128)
#define PADDED_BLOCK_WIDTH_CPU_TO_GPU    (80)

#define MIN(x, y)    (x < y ? x:y)

/*32 index*/
const ushort indexTable[32] = {0x1f,0x1e,0x1d,0x1c,0x1b,0x1a,0x19,0x18,0x17,0x16,0x15,0x14,0x13,0x12,0x11,0x10,
                               0x0f,0x0e,0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00};

_GENX_MAIN_  void
SubSamplePoint_p(SurfaceIndex ibuf, SurfaceIndex obuf, uint in_width, uint in_height, uint out_width, uint out_height)
{
    vector<uchar, OUT_BLOCK>
        out = 0;
    uint
        ix = get_thread_origin_x(),
        iy = get_thread_origin_y(),
        step_h = in_height / out_height,
        step_w = in_width / out_width,
        nc = !(step_h % 2),
        cor = (iy % 2) & nc, //To deal with interlace content
        offset_x = (ix * step_w * OUT_BLOCK),
        offset_y = (iy * step_h + cor);
    matrix<uchar, 1, 1>
        pxl;
    vector<uint, 4>
        lumaVal = 0;
#pragma unroll(OUT_BLOCK)
    for (int i = 0; i < OUT_BLOCK; i++) {
        read(ibuf, offset_x, offset_y, pxl);
        out(i) = pxl(0, 0);
        offset_x += step_w;
    }

    write_plane(obuf, GENX_SURFACE_Y_PLANE, (ix * OUT_BLOCK), iy, out);
}

_GENX_MAIN_  void
SubSamplePoint_t(SurfaceIndex ibuf, SurfaceIndex obuf, uint in_width, uint in_height, uint out_width, uint out_height)
{
    vector<uchar, OUT_BLOCK>
        out = 0;
    uint
        ix = get_thread_origin_x(),
        iy = get_thread_origin_y(),
        step_h = in_height / out_height,
        step_w = in_width / out_width,
        nc = (step_h % 2),
        cor = (iy % 2) & nc,
        offset_x = (ix * step_w * OUT_BLOCK),
        offset_y = (iy * step_h + cor);
    matrix<uchar, 1, 1>
        pxl;
    matrix<uint, 1, 1>
        lumaVal;
#pragma unroll(OUT_BLOCK)
    for (int i = 0; i < OUT_BLOCK; i++) {
        read(ibuf, offset_x, offset_y, pxl);
        out(i) = pxl(0, 0);
        offset_x += step_w;
    }

    write_plane(obuf, GENX_SURFACE_Y_PLANE, (ix * OUT_BLOCK), iy, out);
}

_GENX_MAIN_  void
SubSamplePoint_b(SurfaceIndex ibuf, SurfaceIndex obuf, uint in_width, uint in_height, uint out_width, uint out_height)
{
    vector<uchar, OUT_BLOCK>
        out = 0;
    uint
        ix          = get_thread_origin_x(),
        iy          = get_thread_origin_y(),
        step_h      = in_height / out_height,
        step_w      = in_width / out_width,
        nc          = !(step_h % 2),
        cor         = !(iy % 2) | nc,
        offset_x    = (ix * step_w * OUT_BLOCK),
        offset_y    = (iy * step_h + cor);
    matrix<uchar, 1, 1>
        pxl;
    matrix<uint, 1, 1>
        lumaVal;
#pragma unroll(OUT_BLOCK)
    for (int i = 0; i < OUT_BLOCK; i++) {
        read(ibuf, offset_x, offset_y, pxl);
        out(i) = pxl(0, 0);
        offset_x += step_w;
    }

    write_plane(obuf, GENX_SURFACE_Y_PLANE, (ix * OUT_BLOCK), iy, out);
}

_GENX_MAIN_  void  
surfaceCopy_Y(SurfaceIndex INBUF_IDX, SurfaceIndex OUTBUF_IDX, uint width_dword, uint height, uint width_stride)
{
	//write Y plane
	matrix<uchar, BLOCK_HEIGHT, BLOCK_PIXEL_WIDTH> inData_m;
	vector_ref<uchar, BLOCK_PIXEL_WIDTH> inData0(inData_m.row(0));
	vector_ref<uchar, BLOCK_PIXEL_WIDTH> inData1(inData_m.row(1));
	vector_ref<uchar, BLOCK_PIXEL_WIDTH> inData2(inData_m.row(2));
	vector_ref<uchar, BLOCK_PIXEL_WIDTH> inData3(inData_m.row(3));
	vector_ref<uchar, BLOCK_PIXEL_WIDTH> inData4(inData_m.row(4));
	vector_ref<uchar, BLOCK_PIXEL_WIDTH> inData5(inData_m.row(5));
	vector_ref<uchar, BLOCK_PIXEL_WIDTH> inData6(inData_m.row(6));
	vector_ref<uchar, BLOCK_PIXEL_WIDTH> inData7(inData_m.row(7));

	matrix<uchar, SUB_BLOCK_HEIGHT, SUB_BLOCK_PIXEL_WIDTH> outData0;
	matrix<uchar, SUB_BLOCK_HEIGHT, SUB_BLOCK_PIXEL_WIDTH> outData1;
	matrix<uchar, SUB_BLOCK_HEIGHT, SUB_BLOCK_PIXEL_WIDTH> outData2;
	matrix<uchar, SUB_BLOCK_HEIGHT, SUB_BLOCK_PIXEL_WIDTH> outData3;

	int horizOffset = get_thread_origin_x() * BLOCK_PIXEL_WIDTH;
	int vertOffset = get_thread_origin_y() * BLOCK_HEIGHT;
	
	read(INBUF_IDX, horizOffset, vertOffset,     inData0);
	read(INBUF_IDX, horizOffset, vertOffset + 1, inData1);
	read(INBUF_IDX, horizOffset, vertOffset + 2, inData2);
	read(INBUF_IDX, horizOffset, vertOffset + 3, inData3);
	read(INBUF_IDX, horizOffset, vertOffset + 4, inData4);
	read(INBUF_IDX, horizOffset, vertOffset + 5, inData5);
	read(INBUF_IDX, horizOffset, vertOffset + 6, inData6);
	read(INBUF_IDX, horizOffset, vertOffset + 7, inData7);

	outData0 = inData_m.select<SUB_BLOCK_HEIGHT, 1, SUB_BLOCK_PIXEL_WIDTH, 1>(0, 0);
	outData1 = inData_m.select<SUB_BLOCK_HEIGHT, 1, SUB_BLOCK_PIXEL_WIDTH, 1>(0, 8);
	outData2 = inData_m.select<SUB_BLOCK_HEIGHT, 1, SUB_BLOCK_PIXEL_WIDTH, 1>(0, 16);
	outData3 = inData_m.select<SUB_BLOCK_HEIGHT, 1, SUB_BLOCK_PIXEL_WIDTH, 1>(0, 24);

	write_plane(OUTBUF_IDX, GENX_SURFACE_Y_PLANE, horizOffset,                           vertOffset, outData0);
	write_plane(OUTBUF_IDX, GENX_SURFACE_Y_PLANE, horizOffset + SUB_BLOCK_PIXEL_WIDTH,   vertOffset, outData1);
	write_plane(OUTBUF_IDX, GENX_SURFACE_Y_PLANE, horizOffset + SUB_BLOCK_PIXEL_WIDTH*2, vertOffset, outData2);
	write_plane(OUTBUF_IDX, GENX_SURFACE_Y_PLANE, horizOffset + SUB_BLOCK_PIXEL_WIDTH*3, vertOffset, outData3);
}
#endif
