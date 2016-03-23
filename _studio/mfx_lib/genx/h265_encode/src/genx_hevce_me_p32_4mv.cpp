/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2016 Intel Corporation. All Rights Reserved.
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
#include "../include/genx_hevce_me_common.h"

#if !defined(target_gen7_5) && !defined(target_gen8) && !defined(target_gen9) && !defined(CMRT_EMU)
#error One of macro should be defined: target_gen7_5, target_gen8, target_gen9
#endif

#ifdef target_gen7_5
typedef matrix<uchar, 3, 32> UniIn;
#else
typedef matrix<uchar, 4, 32> UniIn;
#endif

extern "C" _GENX_MAIN_
void MeP32_4MV(
    SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_MV32x32/*SURF_MV_PRED*/,
    SurfaceIndex SURF_MV16x16, SurfaceIndex SURF_MV32x16, SurfaceIndex SURF_MV16x32, int rectParts) // SURF_MV32x32 is DS 2x surf
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
    read(SURF_MV32x32, mbX * MVDATA_SIZE, mbY, mvPred);

    // load search path
    SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control, 0, 64);

    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);

    // M0.3 various prediction parameters
    if (rectParts) {
        VME_SET_DWORD(uniIn, 0, 3, 0x70240000); // BMEDisableFBR=1 InterSAD=2 SubMbPartMask=0x78: 8x8,8x16,16x8,16x16 to have 4MVs in the output (for MeP16)
    } else {
        VME_SET_DWORD(uniIn, 0, 3, 0x76240000); // BMEDisableFBR=1 InterSAD=2 SubMbPartMask=0x78: 8x8,16x16 to have 4MVs in the output (for MeP16)
    }

    // M1.1 MaxNumMVs
    VME_SET_UNIInput_MaxNumMVs(uniIn, 32);

    // M0.4 reserved
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
        SURF_SRC_AND_REF, ref0, NULL, costCenter, imeOut);

    vector<int2,2> mv16 = imeOut.row(7).format<int2>().select<2,1>(10);
    matrix<int2,2,4> mv8 = imeOut.row(8).format<int2>().select<8,1>(8); // 4 MVs

    vector<int2,2> diff = cm_abs<int2>(mvPred);
    diff = diff > 16;
    if (diff.any()) {
        uint2 dist16 = imeOut.row(7).format<ushort>()[8];
        vector<uint2,4> dist8 = imeOut.row(7).format<ushort>().select<4,1>(4);
        vector<int2,2> mvPred0 = 0;
        SetRef(sourceXY, mvPred0, searchWindow, widthHeight, ref0XY);
        ref0 = uniIn.row(0).format<int2>().select<2,1>(0);
        run_vme_ime(uniIn, imeIn,
            VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
            SURF_SRC_AND_REF, ref0, NULL, costCenter, imeOut);

        uint2 dist16_0 = imeOut.row(7).format<ushort>()[8];
        vector<uint2,4> dist8_0 = imeOut.row(7).format<ushort>().select<4,1>(4);
        vector<int2,2> mv16_0 = imeOut.row(7).format<int2>().select<2,1>(10);
        matrix<int2,2,4> mv8_0 = imeOut.row(8).format<int2>().select<8,1>(8);

        mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist16_0 < dist16);
        mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
    }

    //DebugUniOutput<9>(imeOut);

    VME_SET_UNIInput_SubPelMode(uniIn, 3);
    VME_SET_UNIInput_BMEDisableFBR(uniIn);
    SLICE(fbrIn.format<uint>(), 1, 16, 2) = 0; // zero L1 motion vectors

    matrix<uchar, 7, 32> fbrOut16x16;
    VME_SET_UNIInput_FBRMbModeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
    fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 0) = mv16.format<uint4>()[0]; // motion vectors 16x16
    run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 0, 0, 0, fbrOut16x16);

    // 32x32
    SLICE(fbrOut16x16.format<short>(), 16, 2, 1) = SLICE(fbrOut16x16.format<short>(), 16, 2, 1) << 1;
    write(SURF_MV32x32, mbX * MVDATA_SIZE, mbY, SLICE(fbrOut16x16.format<uint>(), 8, 1, 1));    // 32x32 MVs are updated

    matrix<uchar, 7, 32> fbrOut8x8;
    VME_SET_UNIInput_FBRMbModeInput(uniIn, 3);
    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);

    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(0, 0) = mv8.format<uint>()[0]; // 8x8_0
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(1, 0) = mv8.format<uint>()[1]; // 8x8_1
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(2, 0) = mv8.format<uint>()[2]; // 8x8_2
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(3, 0) = mv8.format<uint>()[3]; // 8x8_3

    run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 3, 0, 0, fbrOut8x8);

    // 16x16
    matrix<uint, 2, 2> mv8x8;
    mv8x8.format<int2, 4, 2>() = fbrOut8x8.format<int2, 7, 16>().select<4, 1, 2, 1>(1, 0) << 1;
    write(SURF_MV16x16, mbX * MVDATA_SIZE * 2, mbY * 2, mv8x8); // 4 MVs of 16x16 are saved

    if (rectParts)
    {
        matrix<uchar, 7, 32> fbrOut16x8;
        VME_SET_UNIInput_FBRMbModeInput(uniIn, 1);
        VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
        VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
        fbrIn.format<uint, 4, 8>().select<2, 1, 4, 2>(0, 0) = imeOut.row(8).format<uint>()[0]; // motion vectors 16x8_0
        fbrIn.format<uint, 4, 8>().select<2, 1, 4, 2>(2, 0) = imeOut.row(8).format<uint>()[1]; // motion vectors 16x8_1
        run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 1, 0, 0, fbrOut16x8);

        matrix<uchar, 7, 32> fbrOut8x16;
        VME_SET_UNIInput_FBRMbModeInput(uniIn, 2);
        VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
        VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
        fbrIn.format<uint, 4, 8>().select<2, 2, 4, 2>(0, 0) = imeOut.row(8).format<uint>()[2]; // motion vectors 8x16_0
        fbrIn.format<uint, 4, 8>().select<2, 2, 4, 2>(1, 0) = imeOut.row(8).format<uint>()[3]; // motion vectors 8x16_1
        run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 2, 0, 0, fbrOut8x16);

        // 32x16
        fbrOut16x8.format<short, 7, 16>().select<2, 2, 2, 1>(1, 0) = fbrOut16x8.format<short, 7, 16>().select<2, 2, 2, 1>(1, 0) << 1;
        matrix<uint, 2, 1> mv32x16 = SLICE(fbrOut16x8.format<uint>(), 8, 2, 16);
        write(SURF_MV32x16,  mbX * MVDATA_SIZE, mbY * 2, mv32x16);

        // 16x32
        fbrOut8x16.format<short, 7, 16>().select<2, 1, 2, 1>(1, 0) = fbrOut8x16.format<short, 7, 16>().select<2, 1, 2, 1>(1, 0) << 1;
        matrix<uint, 1, 2> mv16x32 = SLICE(fbrOut8x16.format<uint>(), 8, 2, 8);
        write(SURF_MV16x32,  mbX * MVDATA_SIZE * 2, mbY, mv16x32);
    }
}
