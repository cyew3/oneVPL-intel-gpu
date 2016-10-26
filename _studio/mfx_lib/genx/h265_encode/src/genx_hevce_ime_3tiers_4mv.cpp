//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#include <cm/cm.h>
#include <cm/cmtl.h>
#include <cm/genx_vme.h>
#include "../include/genx_hevce_me_common.h"

#if !defined(target_gen7_5) && !defined(target_gen8) && !defined(target_gen9) && !defined(CMRT_EMU)
#error One of macro should be defined: target_gen7_5, target_gen8, target_gen8_5
#endif

#ifdef target_gen7_5
typedef matrix<uchar, 3, 32> UniIn;
#else
typedef matrix<uchar, 4, 32> UniIn;
#endif


_GENX_ inline
void ImeOneTier4MV(vector_ref<int2, 2> mvPred, SurfaceIndex SURF_SRC_AND_REF, matrix_ref<int2, 2, 4> mvOut,
                   uint2 width, uint2 height, UniIn uniIn, matrix<uchar, 2, 32> imeIn)
{
    matrix<uchar, 9, 32> imeOut;

    // update ref
    // M0.0 Ref0X, Ref0Y
    vector_ref<int2, 2> sourceXY = uniIn.row(0).format<int2>().select<2,1>(4);
    vector<uint1, 2> widthHeight;
    widthHeight[0] = (height >> 4) - 1;
    widthHeight[1] = (width >> 4);
    vector_ref<int1, 2> searchWindow = uniIn.row(0).format<int1>().select<2,1>(22);
    vector_ref<int2, 2> ref0XY = uniIn.row(0).format<int2>().select<2,1>(0);
    SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);

    // M1.2 Start0X, Start0Y
    vector<int1, 2> start0 = searchWindow;
    start0 = ((start0 - 16) >> 3) & 0x0f;
    uniIn.row(1)[10] = start0[0] | (start0[1] << 4);

    uniIn.row(1)[31] = 0x1;

    vector<int2,2>  ref0       = uniIn.row(0).format<int2>().select<2, 1> (0);
#ifdef target_gen7_5
    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
#else
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);
#endif
    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0, NULL, costCenter, imeOut);

    mvOut.format<int2>() = imeOut.row(8).format<int2>().select<8,1>(8) << 1;  // 4 MVs

    vector<int2,2> diff = cm_abs<int2>(mvPred);
    diff = diff > 16;
    if (diff.any()) {
        vector<uint2,4> dist = imeOut.row(5).format<ushort>().select<4,4>();
        vector<int2,2> mvPred0 = 0;
        SetRef(sourceXY, mvPred0, searchWindow, widthHeight, ref0XY);
        ref0 = uniIn.row(0).format<int2>().select<2, 1> (0);
        run_vme_ime(uniIn, imeIn,
            VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
            SURF_SRC_AND_REF, ref0, NULL, costCenter, imeOut);

        vector<uint2,4> dist0 = imeOut.row(5).format<ushort>().select<4,4>();
        matrix<int2,2,4> mvOut0 = imeOut.row(8).format<int2>().select<8,1>(8) << 1;
        mvOut.format<uint4>().merge(mvOut0.format<uint4>(), dist0 < dist);  // 4 MVs
    }
}

_GENX_ inline
void ImeOneTier4MV64(vector_ref<int2, 2> mvPred, SurfaceIndex SURF_SRC_AND_REF, matrix_ref<int2, 2, 4> mvOut,
                   UniIn uniIn, matrix<uchar, 2, 32> imeIn) // mvPred is updated
{
    matrix<uchar, 9, 32> imeOut;
    matrix<uchar, 4, 32> fbrIn;

    // update ref
    // M0.0 Ref0X, Ref0Y
    vector_ref<int2, 2> sourceXY = uniIn.row(0).format<int2>().select<2,1>(4);
    vector<uint1, 2> widthHeight;
    //widthHeight[0] = (height >> 4) - 1;
    //widthHeight[1] = (width >> 4);
    vector_ref<int1, 2> searchWindow = uniIn.row(0).format<int1>().select<2,1>(22);
    vector_ref<int2, 2> ref0XY = uniIn.row(0).format<int2>().select<2,1>(0);
    SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);

    // M1.2 Start0X, Start0Y
    vector<int1, 2> start0 = searchWindow;
    start0 = ((start0 - 16) >> 3) & 0x0f;
    uniIn.row(1)[10] = start0[0] | (start0[1] << 4);

    uniIn.row(1)[31] = 0x1;

    vector<int2,2>  ref0       = uniIn.row(0).format<int2>().select<2, 1> (0);
#ifdef target_gen7_5
    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
#else
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);
#endif
    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0, NULL, costCenter, imeOut);

    vector<int2,2> mv16 = imeOut.row(7).format<int2>().select<2,1>(10);
    matrix<int2,2,4> mv8 = imeOut.row(8).format<int2>().select<8,1>(8) << 1; // 4 MVs

    vector<int2,2> diff = cm_abs<int2>(mvPred);
    diff = diff > 16;
    if (diff.any()) {
        uint2 dist16 = imeOut.row(7).format<ushort>()[8];
        vector<uint2,4> dist8 = imeOut.row(7).format<ushort>().select<4,1>(4);

        vector<int2,2> mvPred0 = 0;
        SetRef(sourceXY, mvPred0, searchWindow, widthHeight, ref0XY);
        ref0 = uniIn.row(0).format<int2>().select<2, 1> (0);
        run_vme_ime(uniIn, imeIn,
            VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
            SURF_SRC_AND_REF, ref0, NULL, costCenter, imeOut);

        uint2 dist16_0 = imeOut.row(7).format<ushort>()[8];
        vector<uint2,4> dist8_0 = imeOut.row(7).format<ushort>().select<4,1>(4);
        vector<int2,2> mv16_0 = imeOut.row(7).format<int2>().select<2,1>(10);
        matrix<int2,2,4> mv8_0 = imeOut.row(8).format<int2>().select<8,1>(8) << 1;

        mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist16_0 < dist16);
        mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
    }

    mvOut.format<int2>() = mv8;

    VME_SET_UNIInput_SubPelMode(uniIn, 3);
    VME_SET_UNIInput_BMEDisableFBR(uniIn);
    SLICE(fbrIn.format<uint>(), 1, 16, 2) = 0; // zero L1 motion vectors

    matrix<uchar, 7, 32> fbrOut16x16;
    VME_SET_UNIInput_FBRMbModeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
    //fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 0) = imeOut.row(7).format<uint>()[5];
    fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 0) = mv16.format<uint4>()[0];
    run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 0, 0, 0, fbrOut16x16);

    // 64x64
    mvPred = SLICE(fbrOut16x16.format<short>(), 16, 2, 1) << 2;

}

extern "C" _GENX_MAIN_
void Ime3Tiers4Mv(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_REF_256x256,
                  SurfaceIndex SURF_REF_128x128, SurfaceIndex SURF_REF_64x64, SurfaceIndex SURF_MV_64x64)
{
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();
    uint x = mbX * 16;
    uint y = mbY * 16;

    vector<uchar, 64> control;
    read(SURF_CONTROL, 0, control);

    uint1 maxNumSu = control.format<uint1>()[56];
    uint1 lenSp = control.format<uint1>()[57];
    uint2 width = control.format<uint2>()[30];
    uint2 height = control.format<uint2>()[31];

    // read MB record data
    UniIn uniIn = 0;
    matrix<uchar, 9, 32> imeOut;
    matrix<uchar, 2, 32> imeIn = 0;
    matrix<uchar, 4, 32> fbrIn;

    // declare parameters for VME
    matrix<uint4, 16, 2> costs = 0;
    vector<int2, 2> mvPred = 0;

    // load search path
    SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control, 0, 64);

    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);

    // M0.3 various prediction parameters
    VME_SET_DWORD(uniIn, 0, 3, 0x77240000); // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x77: 8x8 only
    // M1.1 MaxNumMVs
    VME_SET_UNIInput_MaxNumMVs(uniIn, 32);
    // M0.5 Reference Window Width & Height
    VME_SET_UNIInput_RefW(uniIn, 48);
    VME_SET_UNIInput_RefH(uniIn, 40);

    // M0.0 Ref0X, Ref0Y
    vector_ref<int2, 2> sourceXY = uniIn.row(0).format<int2>().select<2,1>(4);
    vector<uint1, 2> widthHeight;
    widthHeight[0] = (height >> 4) - 1;
    widthHeight[1] = (width >> 4);
    vector_ref<int1, 2> searchWindow = uniIn.row(0).format<int1>().select<2,1>(22);
    vector_ref<int2, 2> ref0XY = uniIn.row(0).format<int2>().select<2,1>(0);
    SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);

    // M1.0-3 Search path parameters & start centers & MaxNumMVs again!!!
    VME_SET_UNIInput_AdaptiveEn(uniIn);
    VME_SET_UNIInput_T8x8FlagForInterEn(uniIn);
    VME_SET_UNIInput_MaxNumMVs(uniIn, 0x3f);
    VME_SET_UNIInput_MaxNumSU(uniIn, maxNumSu);
    VME_SET_UNIInput_LenSP(uniIn, lenSp);

    // M1.2 Start0X, Start0Y
    vector<int1, 2> start0 = searchWindow;
    start0 = ((start0 - 16) >> 3) & 0x0f;
    uniIn.row(1)[10] = start0[0] | (start0[1] << 4);

    uniIn.row(1)[31] = 0x1;

    vector<int2,2>  ref0       = uniIn.row(0).format<int2>().select<2, 1> (0);
#ifdef target_gen7_5
    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
#else
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);
#endif
    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_REF_256x256, ref0, NULL, costCenter, imeOut);

    matrix<int2, 2, 4> mvPred0;
    mvPred0.format<int2>() = imeOut.row(8).format<int2>().select<8,1>(8) << 1;

    const uint xBase0 = x * 2;
    const uint yBase0 = y * 2;
    const uint xBase1 = x * 4;
    const uint yBase1 = y * 4;
    #pragma unroll
    for (int yBlk0 = 0; yBlk0 < 32; yBlk0 += 16) {
        #pragma unroll
        for (int xBlk0 = 0; xBlk0 < 32; xBlk0 += 16) {
            matrix<int2, 2, 4> mvPred1;
            // update X and Y
            VME_SET_UNIInput_SrcX(uniIn, xBase0 + xBlk0);
            VME_SET_UNIInput_SrcY(uniIn, yBase0 + yBlk0);

            int mvInd0 = ((yBlk0 >> 2) | (xBlk0 >> 3)); // choose 8x8_0, 8x8_1, 8x8_2 or 8x8_3

            VME_SET_UNIInput_SubMbPartMask(uniIn, 0x77);
            ImeOneTier4MV(mvPred0.format<int2>().select<2,1>(mvInd0), SURF_REF_128x128, mvPred1, width * 2, height * 2, uniIn, imeIn);

            #pragma unroll
            for (int yBlk1 = 0; yBlk1 < 32; yBlk1 += 16) {
                #pragma unroll
                for (int xBlk1 = 0; xBlk1 < 32; xBlk1 += 16) {
                    matrix<int2, 2, 4> mvPred2;
                    // update X and Y
                    VME_SET_UNIInput_SrcX(uniIn, xBase1 + xBlk0 * 2 + xBlk1);
                    VME_SET_UNIInput_SrcY(uniIn, yBase1 + yBlk0 * 2 + yBlk1);

                    int mvInd1 = ((yBlk1 >> 2) | (xBlk1 >> 3)); // choose 8x8_0, 8x8_1, 8x8_2 or 8x8_3

                    VME_SET_UNIInput_SubMbPartMask(uniIn, 0x76);
                    ImeOneTier4MV64(mvPred1.format<int2>().select<2,1>(mvInd1), SURF_REF_64x64, mvPred2, uniIn, imeIn);
                    write(SURF_MV_64x64, (xBase1 + xBlk0 * 2 + xBlk1) / 8 * MVDATA_SIZE, (yBase1 + yBlk0 * 2 + yBlk1) / 8, mvPred2);
                }
            }

        }
    }
}
