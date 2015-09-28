/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
*/


#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#include <cm/cm.h>
#include <cm/cmtl.h>
#include <cm/genx_vme.h>
#include "../include/utility_genx.h"

enum {
    MFX_CHROMAFORMAT_YUV420 = 1,
    MFX_CHROMAFORMAT_YUV422 = 2,
};

#define DISABLE_GPU_POSTPROC_FOR_REXT

template<uint R, uint C> inline _GENX_ void Average2x2(matrix_ref<uint1,R,C> in, matrix_ref<uint1,R/2,C/2> out)
{
    matrix<uint2,R,C/2> tmp = in.template select<R,1,C/2,2>(0,0) + in.template select<R,1,C/2,2>(0,1);
    matrix<uint2,R/2,C/2> tmp2 = tmp.template select<R/2,2,C/2,1>(0,0) + tmp.template select<R/2,2,C/2,1>(1,0);
    tmp2 += 2;
    out = tmp2 >> 2;
} 

template<uint chromaFormat, uint bitDepth> inline _GENX_ void CopyChroma(SurfaceIndex SRC, SurfaceIndex DST_REXT, SurfaceIndex DST_NV12, uint ox, uint oy, uint padding)
{
    static_assert((chromaFormat == MFX_CHROMAFORMAT_YUV420 || chromaFormat == MFX_CHROMAFORMAT_YUV422) && (bitDepth == 8 || bitDepth == 10), "Bad combination of chromaFormat and bitDepth");

    const bool outputNv12 = (chromaFormat == MFX_CHROMAFORMAT_YUV420 && bitDepth == 8);
    const uint W = (bitDepth > 8) ? 128 : 64;
    const uint H = (chromaFormat == MFX_CHROMAFORMAT_YUV422) ? 16 : 8;

    ox *= W;
    oy *= H;
    if (bitDepth > 8)
        padding *= 2;

    matrix<uint1,8,32> block;

    #pragma unroll
    for (uint y = 0; y < H; y += 8) {
        #pragma unroll
        for (uint x = 0; x < W; x += 32) {
            read(SRC, padding + ox+x, oy+y, block);
            if (outputNv12)
                write_plane(DST_NV12, GENX_SURFACE_UV_PLANE, ox+x, oy+y, block.select_all());
#ifndef DISABLE_GPU_POSTPROC_FOR_REXT
            else
                write(DST_REXT, ox+x, oy+y, block.select_all());
#endif
        }
    }
}

template<uint bitDepth> inline _GENX_ void CopyLuma(SurfaceIndex SRC, SurfaceIndex DST10B, SurfaceIndex DST8B, uint ox, uint oy, uint padding, matrix_ref<uint1,16,64> lumaNv12)
{
    static_assert(bitDepth == 8 || bitDepth == 10, "Bad bitDepth");

    const uint W = (bitDepth > 8) ? 128 : 64;
    const uint H = 16;

    ox *= W;
    oy *= H;
    if (bitDepth > 8)
        padding *= 2;

    if (bitDepth == 8) {
        #pragma unroll
        for (uint y = 0; y < H; y += 8) {
            #pragma unroll
            for (uint x = 0; x < W; x += 32) {
                read(SRC, padding + ox+x, oy+y, lumaNv12.select<8,1,32,1>(y,x));
                write_plane(DST8B, GENX_SURFACE_Y_PLANE, ox+x, oy+y, lumaNv12.select<8,1,32,1>(y,x));
            }
        }
    } else {
        matrix<uint1,8,64> block;
        #pragma unroll
        for (uint y = 0; y < H; y += 8) {
            #pragma unroll
            for (uint x = 0; x < W; x += 64) {
                read(SRC, padding + ox+x, oy+y, block.select<8,1,32,1>(0,0));
                read(SRC, padding + ox+x+32, oy+y, block.select<8,1,32,1>(0,32));
#ifndef DISABLE_GPU_POSTPROC_FOR_REXT
                write(DST10B, ox+x, oy+y, block.select<8,1,32,1>(0,0));
                write(DST10B, ox+x+32, oy+y, block.select<8,1,32,1>(0,32));
#endif
                lumaNv12.select<8,1,32,1>(y,x/2) = cm_asr<uint1>(cm_add<uint2>(block.format<uint2,8,32>(), 2), 2, SAT);
                write_plane(DST8B, GENX_SURFACE_Y_PLANE, (ox+x)/2, oy+y, lumaNv12.select<8,1,32,1>(y,x/2));
            }
        }
    }
}

template<uint chromaFormat, uint bitDepth> _GENX_ void PrepareSrc(
    SurfaceIndex SRC_LU, SurfaceIndex SRC_CH, uint srcPaddingLu, uint srcPaddingCh, SurfaceIndex DST_LU, SurfaceIndex DST_CH,
    SurfaceIndex DST_1X, SurfaceIndex DST_2X, SurfaceIndex DST_4X, SurfaceIndex DST_8X, SurfaceIndex DST_16X)
{
    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    CopyChroma<chromaFormat, bitDepth>(SRC_CH, DST_CH, DST_1X, x, y, srcPaddingCh);

    matrix<uint1,16,64> lumaNv12;
    CopyLuma<bitDepth> (SRC_LU, DST_LU, DST_1X, x, y, srcPaddingLu, lumaNv12);

    matrix<uint1,8,32> out2x;
    Average2x2(lumaNv12.select_all(), out2x.select_all());
    write_plane(DST_2X, GENX_SURFACE_Y_PLANE, 32*x, 8*y, out2x);

    matrix<uint1,4,16> out4x;
    Average2x2(out2x.select_all(), out4x.select_all());
    write_plane(DST_4X, GENX_SURFACE_Y_PLANE, 16*x, 4*y, out4x);

    matrix<uint1,2,8> out8x;
    Average2x2(out4x.select_all(), out8x.select_all());
    write_plane(DST_8X, GENX_SURFACE_Y_PLANE, 8*x, 2*y, out8x);

    matrix<uint1,1,4> out16x;
    Average2x2(out8x.select_all(), out16x.select_all());
    write_plane(DST_16X, GENX_SURFACE_Y_PLANE, 4*x, y, out16x);
}


extern "C" _GENX_MAIN_ void PrepareSrcNv12(SurfaceIndex SRC_LU, SurfaceIndex SRC_CH, uint srcPaddingLu, uint srcPaddingCh,
                                           SurfaceIndex DST_1X, SurfaceIndex DST_2X, SurfaceIndex DST_4X, SurfaceIndex DST_8X, SurfaceIndex DST_16X)
{
    PrepareSrc<MFX_CHROMAFORMAT_YUV420, 8>(SRC_LU, SRC_CH, srcPaddingLu, srcPaddingCh, DST_1X, DST_1X, DST_1X, DST_2X, DST_4X, DST_8X, DST_16X);
}

extern "C" _GENX_MAIN_ void PrepareSrcP010(SurfaceIndex SRC_LU, SurfaceIndex SRC_CH, uint srcPaddingLu, uint srcPaddingCh, SurfaceIndex DST_LU, SurfaceIndex DST_CH,
                                           SurfaceIndex DST_1X, SurfaceIndex DST_2X, SurfaceIndex DST_4X, SurfaceIndex DST_8X, SurfaceIndex DST_16X)
{
    PrepareSrc<MFX_CHROMAFORMAT_YUV420, 10>(SRC_LU, SRC_CH, srcPaddingLu, srcPaddingCh, DST_LU, DST_CH, DST_1X, DST_2X, DST_4X, DST_8X, DST_16X);
}

extern "C" _GENX_MAIN_ void PrepareSrcNv16(SurfaceIndex SRC_LU, SurfaceIndex SRC_CH, uint srcPaddingLu, uint srcPaddingCh, SurfaceIndex DST_CH,
                                          SurfaceIndex DST_1X, SurfaceIndex DST_2X, SurfaceIndex DST_4X, SurfaceIndex DST_8X, SurfaceIndex DST_16X)
{
    PrepareSrc<MFX_CHROMAFORMAT_YUV422, 8>(SRC_LU, SRC_CH, srcPaddingLu, srcPaddingCh, DST_1X, DST_CH, DST_1X, DST_2X, DST_4X, DST_8X, DST_16X);
}

extern "C" _GENX_MAIN_ void PrepareSrcP210(SurfaceIndex SRC_LU, SurfaceIndex SRC_CH, uint srcPaddingLu, uint srcPaddingCh, SurfaceIndex DST_LU, SurfaceIndex DST_CH,
                                           SurfaceIndex DST_1X, SurfaceIndex DST_2X, SurfaceIndex DST_4X, SurfaceIndex DST_8X, SurfaceIndex DST_16X)
{
    PrepareSrc<MFX_CHROMAFORMAT_YUV422, 10>(SRC_LU, SRC_CH, srcPaddingLu, srcPaddingCh, DST_LU, DST_CH, DST_1X, DST_2X, DST_4X, DST_8X, DST_16X);
}

