//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

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
    YUV420 = 1,
    YUV422 = 2,
};

#define NO_GPU_POSTPROC_FOR_REXT

template<uint R, uint C> inline _GENX_ void Average2x2(matrix_ref<uint1,R,C> in, matrix_ref<uint1,R/2,C/2> out)
{
    matrix<uint2,R,C/2> tmp = in.template select<R,1,C/2,2>(0,0) + in.template select<R,1,C/2,2>(0,1);
    matrix<uint2,R/2,C/2> tmp2 = tmp.template select<R/2,2,C/2,1>(0,0) + tmp.template select<R/2,2,C/2,1>(1,0);
    tmp2 += 2;
    out = tmp2 >> 2;
} 


/*
    fromSys luma   NV12 SYS_LU -> VID_NV12
    fromSys luma   NV16 SYS_LU -> VID_NV12
    fromSys luma   P010 SYS_LU -> VID_LU, VID_NV12
    fromSys luma   P210 SYS_LU -> VID_LU, VID_NV12
    fromSys chroma NV12 SYS_CH -> VID_NV12
    fromSys chroma NV16 SYS_CH -> VID_CH
    fromSys chroma P010 SYS_CH -> VID_CH
    fromSys chroma P210 SYS_CH -> VID_CH
    fromVid luma   NV12 VID_NV12 -> SYS_LU
    fromVid luma   NV16 VID_NV12 -> SYS_LU
    fromVid luma   P010 VID_LU   -> SYS_LU, VID_NV12
    fromVid luma   P210 VID_LU   -> SYS_LU, VID_NV12
    fromVid chroma NV12 VID_NV12 -> SYS_CH
    fromVid chroma NV16 VID_CH   -> SYS_CH
    fromVid chroma P010 VID_CH   -> SYS_CH
    fromVid chroma P210 VID_CH   -> SYS_CH
*/

template<bool fromSys, uint chromaFormat, uint bitDepth, uint BLOCKH, uint BLOCKW>
inline _GENX_ void CopyLumaAndChroma(SurfaceIndex SYS_LU, uint paddingLu, SurfaceIndex SYS_CH, uint paddingCh,
                                     SurfaceIndex VID_LU, SurfaceIndex VID_CH, SurfaceIndex VID_NV12,
                                     uint ox, uint oy, matrix_ref<uint1,BLOCKH,BLOCKW> lumaNv12)
{
    static_assert(( fromSys && (chromaFormat == YUV420 || chromaFormat == YUV422) && (bitDepth == 8 || bitDepth == 10)) ||
                  (!fromSys && chromaFormat == YUV420 && bitDepth == 8), "Bad combination of fromSys, chromaFormat and bitDepth");

    enum { H=4, W=32 };

    if (fromSys) {
        if (bitDepth == 8) {
            ox *= BLOCKW;
            oy *= BLOCKH;
            //#pragma unroll // faster without unroll
            for (uint y = 0; y < BLOCKH; y += H) {
                #pragma unroll
                for (uint x = 0; x < BLOCKW; x += W) {
                    matrix_ref<uint1,H,W> block = lumaNv12.template select<H,1,W,1>(y,x);
                    read(SYS_LU, paddingLu+ox+x, oy+y, block);
                    write_plane(VID_NV12, GENX_SURFACE_Y_PLANE, ox+x, oy+y, block);
                }
            }
        } else if (bitDepth == 10) {
            ox *= BLOCKW*2;
            oy *= BLOCKH;
            //#pragma unroll // faster without unroll
            for (uint y = 0; y < BLOCKH; y += H) {
                #pragma unroll
                for (uint x = 0; x < BLOCKW*2; x += W) {
                    matrix<uint2,H,W/2> luma10b;
                    read(SYS_LU, 2*paddingLu+ox+x, oy+y, luma10b);
#ifndef NO_GPU_POSTPROC_FOR_REXT
                    write(VID_LU, ox+x, oy+y, luma10b);
#endif
                    matrix_ref<uint1,H,W/2> output = lumaNv12.template select<H,1,W/2,1>(y,x/2);
                    output = cm_asr<uint1>(cm_add<uint2>(luma10b, 2), 2, SAT);
                    write_plane(VID_NV12, GENX_SURFACE_Y_PLANE, (ox+x)/2, oy+y, output);
                }
            }
        }
    } else {
        if (bitDepth == 8) {
            ox *= BLOCKW;
            oy *= BLOCKH;
            //#pragma unroll // faster without unroll
            for (uint y = 0; y < BLOCKH; y += H) {
                #pragma unroll
                for (uint x = 0; x < BLOCKW; x += W) {
                    matrix_ref<uint1,H,W> block = lumaNv12.template select<H,1,W,1>(y,x);
                    read_plane(VID_NV12, GENX_SURFACE_Y_PLANE, ox+x, oy+y, block);
                    write(SYS_LU, paddingLu+ox+x, oy+y, block);
                }
            }
        } else if (bitDepth == 10) {
        }
    }

    if (fromSys) {
        if (chromaFormat == YUV420 && bitDepth == 8) {
            oy >>= 1;
            #pragma unroll // faster with unroll
            for (uint y = 0; y < BLOCKH/2; y += H) {
                #pragma unroll
                for (uint x = 0; x < BLOCKW; x += W) {
                    matrix<uint1,H,W> block;
                    read(SYS_CH, paddingCh+ox+x, oy+y, block);
                    write_plane(VID_NV12, GENX_SURFACE_UV_PLANE, ox+x, oy+y, block);
                }
            }
        } else if (chromaFormat == YUV422 && bitDepth == 8) {
#ifndef NO_GPU_POSTPROC_FOR_REXT
            #pragma unroll
            for (uint y = 0; y < BLOCKH; y += H) {
                #pragma unroll
                for (uint x = 0; x < BLOCKW; x += W) {
                    matrix<uint1,H,W> block;
                    read(SYS_CH, paddingCh+ox+x, oy+y, block);
                    write(VID_CH, ox+x, oy+y, block);
                }
            }
#endif
        } else if (chromaFormat == YUV420 && bitDepth == 10) {
#ifndef NO_GPU_POSTPROC_FOR_REXT
            oy >>= 1;
            #pragma unroll
            for (uint y = 0; y < BLOCKH/2; y += H) {
                #pragma unroll
                for (uint x = 0; x < BLOCKW*2; x += W) {
                    matrix<uint1,H,W> block;
                    read(SYS_CH, 2*paddingCh+ox+x, oy+y, block);
                    write(VID_CH, ox+x, oy+y, block);
                }
            }
#endif
        } else if (chromaFormat == YUV422 && bitDepth == 10) {
#ifndef NO_GPU_POSTPROC_FOR_REXT
            #pragma unroll
            for (uint y = 0; y < BLOCKH; y += H) {
                #pragma unroll
                for (uint x = 0; x < BLOCKW*2; x += W) {
                    matrix<uint1,H,W> block;
                    read(SYS_CH, 2*paddingCh+ox+x, oy+y, block);
                    write(VID_CH, ox+x, oy+y, block);
                }
            }
#endif
        }
    } else {
        if (chromaFormat == YUV420 && bitDepth == 8) {
            oy >>= 1;
            #pragma unroll // faster with unroll
            for (uint y = 0; y < BLOCKH/2; y += H) {
                #pragma unroll
                for (uint x = 0; x < BLOCKW; x += W) {
                    matrix<uint1,H,W> block;
                    read_plane(VID_NV12, GENX_SURFACE_UV_PLANE, ox+x, oy+y, block);
                    write(SYS_CH, paddingCh+ox+x, oy+y, block);
                }
            }
        } else if (chromaFormat == YUV422 && bitDepth == 8) {
        } else if (chromaFormat == YUV420 && bitDepth == 10) {
        } else if (chromaFormat == YUV422 && bitDepth == 10) {
        }
    }
}


template<bool fromSys, uint chromaFormat, uint bitDepth> _GENX_ void PrepareSrc(
    SurfaceIndex SYS_LU, SurfaceIndex SYS_CH, uint sysPaddingLu, uint sysPaddingCh, SurfaceIndex VID_LU, SurfaceIndex VID_CH,
    SurfaceIndex VID_NV12, SurfaceIndex DST_2X, SurfaceIndex DST_4X, SurfaceIndex DST_8X, SurfaceIndex DST_16X)
{
    static_assert((chromaFormat == YUV420 || chromaFormat == YUV422) && (bitDepth == 8 || bitDepth == 10), "Bad combination of chromaFormat and bitDepth");

    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    matrix<uint1,16,64> lumaNv12;
    CopyLumaAndChroma<fromSys, chromaFormat, bitDepth>(SYS_LU, sysPaddingLu, SYS_CH, sysPaddingCh, VID_LU, VID_CH, VID_NV12, x, y, lumaNv12.select_all());

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


extern "C" _GENX_MAIN_ void PrepareSrcNv12(SurfaceIndex SYS_LU, SurfaceIndex SYS_CH, uint sysPaddingLu, uint sysPaddingCh,
                                           SurfaceIndex VID_1X, SurfaceIndex VID_2X, SurfaceIndex VID_4X, SurfaceIndex VID_8X, SurfaceIndex VID_16X)
{
    PrepareSrc<true, YUV420, 8>(SYS_LU, SYS_CH, sysPaddingLu, sysPaddingCh, VID_1X, VID_1X, VID_1X, VID_2X, VID_4X, VID_8X, VID_16X);
}

extern "C" _GENX_MAIN_ void PrepareSrcP010(SurfaceIndex SYS_LU, SurfaceIndex SYS_CH, uint sysPaddingLu, uint sysPaddingCh, SurfaceIndex VID_LU, SurfaceIndex VID_CH,
                                           SurfaceIndex VID_1X, SurfaceIndex VID_2X, SurfaceIndex VID_4X, SurfaceIndex VID_8X, SurfaceIndex VID_16X)
{
    PrepareSrc<true, YUV420, 10>(SYS_LU, SYS_CH, sysPaddingLu, sysPaddingCh, VID_LU, VID_CH, VID_1X, VID_2X, VID_4X, VID_8X, VID_16X);
}

extern "C" _GENX_MAIN_ void PrepareSrcNv16(SurfaceIndex SYS_LU, SurfaceIndex SYS_CH, uint sysPaddingLu, uint sysPaddingCh, SurfaceIndex VID_CH,
                                          SurfaceIndex VID_1X, SurfaceIndex VID_2X, SurfaceIndex VID_4X, SurfaceIndex VID_8X, SurfaceIndex VID_16X)
{
    PrepareSrc<true, YUV422, 8>(SYS_LU, SYS_CH, sysPaddingLu, sysPaddingCh, VID_1X, VID_CH, VID_1X, VID_2X, VID_4X, VID_8X, VID_16X);
}

extern "C" _GENX_MAIN_ void PrepareSrcP210(SurfaceIndex SYS_LU, SurfaceIndex SYS_CH, uint sysPaddingLu, uint sysPaddingCh, SurfaceIndex VID_LU, SurfaceIndex VID_CH,
                                           SurfaceIndex VID_1X, SurfaceIndex VID_2X, SurfaceIndex VID_4X, SurfaceIndex VID_8X, SurfaceIndex VID_16X)
{
    PrepareSrc<true, YUV422, 10>(SYS_LU, SYS_CH, sysPaddingLu, sysPaddingCh, VID_LU, VID_CH, VID_1X, VID_2X, VID_4X, VID_8X, VID_16X);
}

extern "C" _GENX_MAIN_ void PrepareRefNv12(SurfaceIndex SYS_LU, SurfaceIndex SYS_CH, uint sysPaddingLu, uint sysPaddingCh,
                                           SurfaceIndex VID_1X, SurfaceIndex VID_2X, SurfaceIndex VID_4X, SurfaceIndex VID_8X, SurfaceIndex VID_16X)
{
    PrepareSrc<false, YUV420, 8>(SYS_LU, SYS_CH, sysPaddingLu, sysPaddingCh, VID_1X, VID_1X, VID_1X, VID_2X, VID_4X, VID_8X, VID_16X);
}

