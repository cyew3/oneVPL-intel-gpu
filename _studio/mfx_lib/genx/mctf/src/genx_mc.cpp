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
//#include <cm/cm.h>
//#include <cm/cmtl.h>
//#include <cm/genx_vme.h>
#include "../include/genx_me_common.h"
#include "../include/genx_blend_mc.h"
#include "../include/genx_sd_common.h"

#if !defined(target_gen7_5) && !defined(target_gen8) && !defined(target_gen9) && !defined(target_gen9_5) && !defined(target_gen10) && !defined(CMRT_EMU)
#error One of macro should be defined: target_gen7_5, target_gen8, target_gen9, target_gen9_5, target_gen10
#endif

extern "C" _GENX_MAIN_
void McP16_4MV_1SURF_WITH_CHR(SurfaceIndex SURF_CONTROL,
    SurfaceIndex SURF_REF1, SurfaceIndex SURF_MV16x16_1,
    SurfaceIndex SURF_SRC, SurfaceIndex SURF_OUT,
    uint start_xy, uint scene_nums) {
    vector<ushort, 2>
        start_mbXY = start_xy;
    vector<uint, 1>
        scene_numbers = scene_nums;
    uchar
        scnFref = scene_numbers.format<uchar>()[0],
        scnSrc = scene_numbers.format<uchar>()[1];
    uint
        mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0],
        mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1],
        x = mbX << 3,//* 8,
        y = mbY << 3;//* 8;
    vector<uchar, 96>
        control;
    read(SURF_CONTROL, 0, control);
    SurfaceIndex
        SURF_TEST = control.format<SurfaceIndex>()[0];
    ushort
        width = control.format<ushort>()[30],
        height = control.format<ushort>()[31],
        th = control.format<short>()[32],
        sTh = control.format<short>()[35];
    matrix<short, 2, 4>
        mv = 0;
    matrix<uchar, 12, 12>
        srcCh = 0,
        preFil = 0;
    matrix<uchar, 8, 8>
        src = 0,
        out = 0,
        out2 = 0,
        fil = 0;
    matrix<uchar, 4, 8>
        och = 0,
        scm = 0;
    vector<float, 2>
        RsCsT = 0;
    short
        nsc = scnFref == scnSrc;
    read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x - 2, y - 2, srcCh);
    if (th > 0) {
        RsCsT = Genx_RsCs_aprox_8x8Block(srcCh.select<8, 1, 8, 1>(2, 2));
        //Reference generation
        out = OMC_Ref_Generation(SURF_REF1, SURF_MV16x16_1, width, height, mbX, mbY, nsc, mv);
        int4 size = cm_sum<int4>(mv);
        int4 simFactor1 = 0;
        if (nsc) {
            simFactor1 = mergeStrengthCalculator(out, srcCh, RsCsT, nsc, th, size);
            srcCh.select<8, 1, 8, 1>(2, 2) = mergeBlocksRef(srcCh, out, simFactor1);
        }
        /*if (size < DISTANCETH && nsc) {
            int4 simFactor1 = mergeStrengthCalculator(out, srcCh, RsCsT, nsc, th, size);
            srcCh.select<8, 1, 8, 1>(2, 2) = mergeBlocksRef(srcCh, out, simFactor1);
        }
        else {
            if (nsc) {
                out2 = SpatialDenoiser_8x8_Y(srcCh, th);
                out = MedianIdx_8x8_3ref(out, srcCh.select<8, 1, 8, 1>(2, 2), out2);
            }
            else
                out = SpatialDenoiser_8x8_Y(srcCh, th);
            int4 simFactor1 = mergeStrengthCalculator(out, srcCh, RsCsT, true, th, size);
            srcCh.select<8, 1, 8, 1>(2, 2) = mergeBlocksRef(srcCh, out, simFactor1);
        }*/
        fil = srcCh.select<8, 1, 8, 1>(2, 2);
        och = SpatialDenoiser_8x8_NV12_Chroma(SURF_SRC, srcCh, mbX, mbY, sTh);
        write_plane(SURF_OUT, GENX_SURFACE_Y_PLANE, x, y, fil);
        write_plane(SURF_OUT, GENX_SURFACE_UV_PLANE, x, y >> 1, och);
    }
    else {
        read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x, y, fil);
        read_plane(SURF_SRC, GENX_SURFACE_UV_PLANE, x, y >> 1, och);
    }
    write_plane(SURF_OUT, GENX_SURFACE_Y_PLANE, x, y, fil);
    write_plane(SURF_OUT, GENX_SURFACE_UV_PLANE, x, y >> 1, och);
}

#define TEST 1

extern "C" _GENX_MAIN_
void McP16_4MV_2SURF_WITH_CHR(SurfaceIndex SURF_CONTROL,
    SurfaceIndex SURF_REF1, SurfaceIndex SURF_MV16x16_1,
    SurfaceIndex SURF_REF2, SurfaceIndex SURF_MV16x16_2,
    SurfaceIndex SURF_SRC, SurfaceIndex SURF_OUT,
    uint start_xy, uint scene_nums) {
    vector<ushort, 2>
        start_mbXY = start_xy;
    vector<uint, 1>
        scene_numbers = scene_nums;
    uchar
        scnFref = scene_numbers.format<uchar>()[0],
        scnBref = scene_numbers.format<uchar>()[2],
        scnSrc = scene_numbers.format<uchar>()[1];
    uchar
        run = 0;
    uint
        mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0],
        mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1],
        x = mbX << 3,//* 8,
        y = mbY << 3;//* 8;
    vector<uchar, 96>
        control;
    read(SURF_CONTROL, 0, control);
    SurfaceIndex
        SURF_TEST = control.format<SurfaceIndex>()[0];
    ushort
        width = control.format<ushort>()[30],
        height = control.format<ushort>()[31],
        th = control.format<short>()[32],
        sTh = control.format<short>()[35];
#if CMRT_EMU
    if (x >= width) {
        printf("X: %d\tY: %d\n", mbX, mbY);
        return;
    }
    cm_assert(x < width);
#endif
#if PRINTDEBUG
    uint
        threadId = mbY * width / 8 + mbX;
#endif
    matrix<short, 2, 4>
        mv = 0;
#if TEST
    matrix<uchar, 12, 16>
        srcCh = 0,
        preFil = 0;
#else
    matrix<uchar, 12, 12>
        srcCh = 0,
        preFil = 0;
#endif
    matrix<uchar, 8, 8>
        src = 0,
        out = 0,
        out2 = 0,
        out3 = 0,
        out4 = 0,
        fil = 0;
    matrix<uchar, 4, 8>
        och = 0;
    matrix<uchar, 4, 8>
        scm = 0;
    vector<float, 2>
        RsCsT = 0,
        RsCsT1 = 0,
        RsCsT2 = 0;
    short
        dif1 = scnFref == scnSrc,
        dif2 = scnSrc == scnBref,
        dift = !dif1 + !dif2;
#if PRINTDEBUG
    printf("%d\tX: %d\tY: %d\t", threadId, mbX, mbY);
    if (th > 0) {
        read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x - 2, y - 2, srcCh);
        RsCsT = Genx_RsCs_aprox_8x8Block(srcCh.select<8, 1, 8, 1>(2, 2));
        printf("%d\tRs: %.4lf\tCs: %.4lf\tsc1: %d\tsc2: %d\t", threadId, RsCsT(0), RsCsT(1), dif1, dif2);
#endif
        if (th > 0) {
            read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x - 2, y - 2, srcCh);
            RsCsT = Genx_RsCs_aprox_8x8Block(srcCh.select<8, 1, 8, 1>(2, 2));
            //First reference
            out = OMC_Ref_Generation(SURF_REF1, SURF_MV16x16_1, width, height, mbX, mbY, dif1, mv);
            int4 size1 = cm_sum<int4>(mv);
            //Second reference
            out2 = OMC_Ref_Generation(SURF_REF2, SURF_MV16x16_2, width, height, mbX, mbY, dif2, mv);
            int4 size2 = cm_sum<int4>(mv);

            int4 size = ((size1 * dif1) + (size2 * dif2));
            if (dif1 + dif2)
                size /= (dif1 + dif2);


            if (size >= DISTANCETH || dift) {
#if PRINTDEBUG
                printf("%d\trun: %d\t", threadId, run);
                int4 simFactor1 = mergeStrengthCalculator(out, srcCh, RsCsT, dif1, th, size, threadId);
                int4 simFactor2 = mergeStrengthCalculator(out2, srcCh, RsCsT, dif2, th, size, threadId);
                srcCh.select<8, 1, 8, 1>(2, 2) = mergeBlocks2Ref(srcCh, out, simFactor1, out2, simFactor2, threadId);
#else
                int4 simFactor1 = mergeStrengthCalculator(out, srcCh, RsCsT, dif1, th, size1);
                int4 simFactor2 = mergeStrengthCalculator(out2, srcCh, RsCsT, dif2, th, size2);
                srcCh.select<8, 1, 8, 1>(2, 2) = mergeBlocks2Ref(srcCh, out, simFactor1, out2, simFactor2);
#endif
            }
            else {
#if PRINTDEBUG
                run = 1;
                printf("%d\trun: %d\t", threadId, run);
#endif
                out = MedianIdx_8x8_3ref(out, srcCh.select<8, 1, 8, 1>(2, 2), out2);
#if PRINTDEBUG
                int4 simFactor1 = mergeStrengthCalculator(out, srcCh, RsCsT, true, th, size, threadId);
                srcCh.select<8, 1, 8, 1>(2, 2) = mergeBlocksRef(srcCh, out, simFactor1, threadId);
#else
                int4 simFactor1 = mergeStrengthCalculator(out, srcCh, RsCsT, true, th, size);
                srcCh.select<8, 1, 8, 1>(2, 2) = mergeBlocksRef(srcCh, out, simFactor1);
#endif
            }
            fil = srcCh.select<8, 1, 8, 1>(2, 2);
#if TEST
            och = SpatialDenoiser_8x8_NV12_Chroma(SURF_SRC, srcCh.select<12, 1, 12, 1>(0, 0), mbX, mbY, sTh);
#else
            och = SpatialDenoiser_8x8_NV12_Chroma(SURF_SRC, srcCh, mbX, mbY, th);
#endif
            write_plane(SURF_OUT, GENX_SURFACE_Y_PLANE, x, y, fil);
            write_plane(SURF_OUT, GENX_SURFACE_UV_PLANE, x, y >> 1, och);
        }
        else {
            read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x, y, fil);
            read_plane(SURF_SRC, GENX_SURFACE_UV_PLANE, x, y >> 1, och);
        }
#if PRINTDEBUG
        printf("\n");
#endif
        write_plane(SURF_OUT, GENX_SURFACE_Y_PLANE, x, y, fil);
        write_plane(SURF_OUT, GENX_SURFACE_UV_PLANE, x, y >> 1, och);
}

    extern "C" _GENX_MAIN_
        void MC_MERGE4(SurfaceIndex SURF_REF1, SurfaceIndex SURF_REF2, uint start_xy) {
        vector<ushort, 2>
            start_mbXY = start_xy;
        uint
            mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0],
            mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1],
            x = mbX << 4,
            y = mbY << 4;
        matrix<uchar, 16, 16>
            ref1 = 0,
            ref2 = 0;

        read_plane(SURF_REF1, GENX_SURFACE_Y_PLANE, x, y, ref1);
        read_plane(SURF_REF2, GENX_SURFACE_Y_PLANE, x, y, ref2);

        ref1 = (ref1 + ref2 + 1) >> 1;
        write_plane(SURF_REF1, GENX_SURFACE_Y_PLANE, x, y, ref1);
    }

#define VAR_SC_DATA_SIZE 8 //2 * size of float in bytes
    extern "C" _GENX_MAIN_
        void MC_VAR_SC_CALC(SurfaceIndex SURF_SRC, SurfaceIndex SURF_NOISE, uint start_xy) {
        vector<ushort, 2>
            start_mbXY = start_xy;
        int
            mbX = get_thread_origin_x() + start_mbXY.format<ushort>()[0],
            mbY = get_thread_origin_y() + start_mbXY.format<ushort>()[1],
            x = mbX * 16,
            y = mbY * 16;
        matrix<uchar, 17, 32>
            src = 0;
        matrix<short, 16, 16>
            tmp = 0;
        matrix<float, 1, 2>
            var_sc = 0;

        read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x - 1, y - 1, src.select<8, 1, 32, 1>(0, 0));
        read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x - 1, y + 7, src.select<8, 1, 32, 1>(8, 0));
        read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x - 1, y + 15, src.select<1, 1, 32, 1>(16, 0));
        matrix<ushort, 4, 4>
            rs4x4,
            cs4x4;
        matrix<short, 16, 16>
            tmpRs = src.select<16, 1, 16, 1>(0, 1) - src.select<16, 1, 16, 1>(1, 1),
            tmpCs = src.select<16, 1, 16, 1>(1, 0) - src.select<16, 1, 16, 1>(1, 1);
#pragma unroll
        for (uchar i = 0; i < 4; i++) {
#pragma unroll
            for (uchar j = 0; j < 4; j++) {
                rs4x4[i][j] = cm_shr<ushort>(cm_sum<uint>(cm_mul<ushort>(tmpRs.select<4, 1, 4, 1>(i << 2, j << 2), tmpRs.select<4, 1, 4, 1>(i << 2, j << 2))), 4, SAT);
                cs4x4[i][j] = cm_shr<ushort>(cm_sum<uint>(cm_mul<ushort>(tmpCs.select<4, 1, 4, 1>(i << 2, j << 2), tmpCs.select<4, 1, 4, 1>(i << 2, j << 2))), 4, SAT);
            }
        }
        float
            average = cm_sum<float>(src.select<16, 1, 16, 1>(1, 1)) / 256,
            square = cm_sum<float>(cm_mul<ushort>(src.select<16, 1, 16, 1>(1, 1), src.select<16, 1, 16, 1>(1, 1))) / 256;
        var_sc(0, 0) = (square - average * average);//Variance value
        float
            RsFull = cm_sum<float>(rs4x4),
            CsFull = cm_sum<float>(cs4x4);
        var_sc(0, 1) = (RsFull + CsFull) / 16;     //RsCs value

        write(SURF_NOISE, mbX * VAR_SC_DATA_SIZE, mbY, var_sc);
    }