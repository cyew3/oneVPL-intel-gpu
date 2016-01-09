/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed e[]cept in accordance with the terms of that agreement.
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
#include "../include/genx_hevce_me_common.h"
#include "..\include\genx_hevce_refine_me_p_common_old.h"

#if !defined(target_gen7_5) && !defined(target_gen8) && !defined(target_gen9) && !defined(CMRT_EMU)
#error One of macro should be defined: target_gen7_5, target_gen8, target_gen9
#endif

#ifdef target_gen7_5
typedef matrix<uchar, 3, 32> UniIn;
#else
typedef matrix<uchar, 4, 32> UniIn;
#endif

template <int rectParts> inline _GENX_ void Impl(
    SurfaceIndex CONTROL, SurfaceIndex SRC_AND_REF, SurfaceIndex DATA32x32,
    SurfaceIndex DATA16x16, SurfaceIndex DATA8x8, SurfaceIndex DATA16x8,
    SurfaceIndex DATA8x16, SurfaceIndex SRC, SurfaceIndex REF, uint start_mbX, uint start_mbY)
{
    uint mbX = get_thread_origin_x() + start_mbX;
    uint mbY = get_thread_origin_y() + start_mbY;
    uint x = mbX * 16;
    uint y = mbY * 16;

    vector<uchar, 120> control;
    read(CONTROL, 0,  control.select<64,1>());
    read(CONTROL, 64, control.select<56,1>(64));

    vector_ref<uchar,32> searchPath = control.select<32,1>(32);
    vector_ref<uchar,8>  mvCost = control.select<8,1>(72);
    uint1 mvCostScaleFactor = control[112];
    uint1 lenSp    = control[66];
    uint1 maxNumSu = control[67];
    uint2 width = control.format<uint2>()[34];
    uint2 height = control.format<uint2>()[35];

    // read MB record data
    UniIn uniIn = 0;
    matrix<uchar, 9, 32> imeOut;
    matrix<uchar, 4, 32> fbrIn;

    // declare parameters for VME
    matrix<uint4, 16, 2> costs = 0;
    vector<int2, 2> mvPred = 0;
    read(DATA16x16, mbX * INTERDATA_SIZE_SMALL, mbY, mvPred); // these pred MVs will be updated later here

    // load search path
    matrix<uchar, 2, 32> imeIn;
    imeIn.format<uint1>().select<32,1>() = searchPath;
    imeIn.format<uint1>().select<32,1>(32) = 0;

    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);

    // M0.3 various prediction parameters
    if (rectParts)
        VME_SET_DWORD(uniIn, 0, 3, 0x70a40000); // BMEDisableFBR=1 InterSAD=2 SubMbPartMask=0x70: 8x8,8x16,16x8,16x16 (rect subparts are for FEI are on)
    else
        VME_SET_DWORD(uniIn, 0, 3, 0x76a40000); // BMEDisableFBR=1 InterSAD=2 SubMbPartMask=0x76: 8x8,16x16

    // M1.1 MaxNumMVs
    VME_SET_UNIInput_MaxNumMVs(uniIn, 32);
    // M0.5 Reference Window Width & Height
    VME_SET_UNIInput_RefW(uniIn, 48);
    VME_SET_UNIInput_RefH(uniIn, 40);

    // M0.0 Ref0X, Ref0Y
    vector_ref<int2,2> sourceXY = uniIn.row(0).format<int2>().select<2,1>(4);
    vector<uint1,2> widthHeight;
    widthHeight[0] = (height >> 4) - 1;
    widthHeight[1] = (width >> 4);
    vector_ref<int1,2> searchWindow = uniIn.row(0).format<int1>().select<2,1>(22);
    vector_ref<int2,2> ref0XY = uniIn.row(0).format<int2>().select<2,1>(0);
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

    uniIn.row(1)[6] = 0x20;
    uniIn.row(1)[31] = 0x1;

#ifdef target_gen7_5
    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4,1>(8);
#else
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16,1>(0);
#endif

    // setup mv costs
    uniIn(1,30) = (uniIn(1,30) & ~3) | mvCostScaleFactor;
    uniIn.select<1,1,8,1>(2,12) = mvCost;

    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);

    vector<int2,2> mv16 = imeOut.row(7).format<int2>().select<2,1>(10);
    matrix<int2,2,4> mv8 = imeOut.row(8).format<int2>().select<8,1>(8); // 4 MVs

    vector<int2,2> diff = cm_abs<int2>(mvPred);
    diff = diff > 16;
    if (diff.any()) {
        uint2 dist16 = imeOut.row(7).format<ushort>()[8];
        vector<uint2,4> dist8 = imeOut.row(7).format<ushort>().select<4,1>(4);

        mvPred = 0;
        SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);
        run_vme_ime(uniIn, imeIn,
            VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
            SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);

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
    run_vme_fbr(uniIn, fbrIn, SRC_AND_REF, 0, 0, 0, fbrOut16x16);

    matrix<uchar, 7, 32> fbrOut8x8;
    VME_SET_UNIInput_FBRMbModeInput(uniIn, 3);
    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(0, 0) = mv8.format<uint>()[0]; // motion vectors 8x8_0
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(1, 0) = mv8.format<uint>()[1]; // motion vectors 8x8_1
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(2, 0) = mv8.format<uint>()[2]; // motion vectors 8x8_2
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(3, 0) = mv8.format<uint>()[3]; // motion vectors 8x8_3
    run_vme_fbr(uniIn, fbrIn, SRC_AND_REF, 3, 0, 0, fbrOut8x8);

    // 16x16
    vector<uint, 2> data16x16;
    SLICE(data16x16, 0, 1, 1) = SLICE(fbrOut16x16.format<uint>(), 8, 1, 1); // MV
    SLICE(data16x16, 1, 1, 1) = SLICE(fbrOut16x16.row(5).format<ushort>(), 0, 1, 1); // dist
    write(DATA16x16, mbX * INTERDATA_SIZE_SMALL, mbY, data16x16);

    // 8x8
    matrix<uint, 2, 4> data8x8;
    SLICE(data8x8.format<uint>(), 0, 4, 2) = SLICE(fbrOut8x8.format<uint>(), 8, 4, 8); // MV
    SLICE(data8x8.format<uint>(), 1, 4, 2) = SLICE(fbrOut8x8.row(5).format<ushort>(), 0, 4, 4); // dist
    write(DATA8x8, mbX * 2 * INTERDATA_SIZE_SMALL, mbY * 2, data8x8);

    if (rectParts)
    {
        matrix<uchar, 7, 32> fbrOut16x8;
        VME_SET_UNIInput_FBRMbModeInput(uniIn, 1);
        VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
        VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
        fbrIn.format<uint, 4, 8>().select<2, 1, 4, 2>(0, 0) = imeOut.row(8).format<uint>()[0]; // motion vectors 16x8_0
        fbrIn.format<uint, 4, 8>().select<2, 1, 4, 2>(2, 0) = imeOut.row(8).format<uint>()[1]; // motion vectors 16x8_1
        run_vme_fbr(uniIn, fbrIn, SRC_AND_REF, 1, 0, 0, fbrOut16x8);

        matrix<uchar, 7, 32> fbrOut8x16;
        VME_SET_UNIInput_FBRMbModeInput(uniIn, 2);
        VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
        VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
        fbrIn.format<uint, 4, 8>().select<2, 2, 4, 2>(0, 0) = imeOut.row(8).format<uint>()[2]; // motion vectors 8x16_0
        fbrIn.format<uint, 4, 8>().select<2, 2, 4, 2>(1, 0) = imeOut.row(8).format<uint>()[3]; // motion vectors 8x16_1
        run_vme_fbr(uniIn, fbrIn, SRC_AND_REF, 2, 0, 0, fbrOut8x16);

        // 16x8
        matrix<uint, 2, 2> data16x8;
        SLICE(data16x8.format<uint>(), 0, 2, 2) = SLICE(fbrOut16x8.format<uint>(), 8, 2, 16); // MV
        SLICE(data16x8.format<uint>(), 1, 2, 2) = SLICE(fbrOut16x8.row(5).format<ushort>(), 0, 2, 8); // dist
        write(DATA16x8, mbX * INTERDATA_SIZE_SMALL, mbY * 2, data16x8);

        // 8x16
        vector<uint, 4> data8x16;
        SLICE(data8x16.format<uint>(), 0, 2, 2) = SLICE(fbrOut8x16.format<uint>(), 8, 2, 8); // MV
        SLICE(data8x16.format<uint>(), 1, 2, 2) = SLICE(fbrOut8x16.row(5).format<ushort>(), 0, 2, 4); // dist
        write(DATA8x16, mbX * 2 * INTERDATA_SIZE_SMALL, mbY, data8x16);

    }

    if ((mbX & 1) == 0 && (mbY & 1) == 0) {
        vector< uint,9 > sad32x32;
        vector< int2,2 > mv32x32;
        mbX >>= 1;
        mbY >>= 1;
        read(DATA32x32, mbX * INTERDATA_SIZE_BIG, mbY, mv32x32);

        uint x = mbX * 32;
        uint y = mbY * 32;

        QpelRefine(32, 32, SRC, REF, x, y, mv32x32[0], mv32x32[1], sad32x32);

        write(DATA32x32, mbX * INTERDATA_SIZE_BIG + 4,  mbY, SLICE1(sad32x32, 0, 8));   // 32bytes is max until BDW
        write(DATA32x32, mbX * INTERDATA_SIZE_BIG + 36, mbY, SLICE1(sad32x32, 8, 1));
    }
}

extern "C" _GENX_MAIN_
void Me16AndRefine32x32(
    SurfaceIndex CONTROL, SurfaceIndex SRC_AND_REF, SurfaceIndex DATA32x32,
    SurfaceIndex DATA16x16, SurfaceIndex DATA8x8, SurfaceIndex SRC, SurfaceIndex REF, uint start_mbX, uint start_mbY)
{
    Impl<0>(CONTROL, SRC_AND_REF, DATA32x32, DATA16x16, DATA8x8, DATA8x8, DATA8x8, SRC, REF, start_mbX, start_mbY);
}

extern "C" _GENX_MAIN_
void Me16RectPartsAndRefine32x32(
    SurfaceIndex CONTROL, SurfaceIndex SRC_AND_REF, SurfaceIndex DATA32x32, SurfaceIndex DATA16x16,
    SurfaceIndex DATA8x8, SurfaceIndex DATA16x8, SurfaceIndex DATA8x16, SurfaceIndex SRC, SurfaceIndex REF, uint start_mbX, uint start_mbY)
{
    Impl<1>(CONTROL, SRC_AND_REF,DATA32x32, DATA16x16, DATA8x8, DATA16x8, DATA8x16, SRC, REF, start_mbX, start_mbY);
}

