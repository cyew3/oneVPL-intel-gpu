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
#error One of macro should be defined: target_gen7_5, target_gen8, target_gen9
#endif

#ifdef target_gen7_5
typedef matrix<uchar,3,32> UniIn;
#else
typedef matrix<uchar,4,32> UniIn;
#endif

_GENX_ inline matrix<uchar,9,32> CallIme(SurfaceIndex REF, UniIn uniIn, matrix<uchar,2,32> imeIn)
{
    vector<int2,2> refRegion0 = uniIn.format<int2>().select<2,1>();
#ifdef target_gen7_5
    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
#else
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);
#endif
    matrix<uchar,9,32> imeOut;
    run_vme_ime(uniIn, imeIn, VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        REF, refRegion0, NULL, costCenter, imeOut);
    return imeOut;
}


_GENX_ inline
void ImeOneTier4MV(vector_ref<int2, 2> mvPred, SurfaceIndex SRC_AND_REF, matrix_ref<int2, 2, 4> mvOut,
                   uint mbX, uint mbY, UniIn uniIn, matrix<uchar, 2, 32> imeIn)
{
    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, mbX * 16);
    VME_SET_UNIInput_SrcY(uniIn, mbY * 16);

    // update ref
    // M0.0 Ref0X, Ref0Y
    vector_ref<int2, 2> sourceXY = uniIn.row(0).format<int2>().select<2,1>(4);
    vector<uint1, 2> widthHeight;
    //widthHeight[0] = (height >> 4) - 1;
    //widthHeight[1] = (width >> 4);
    vector_ref<int1, 2> searchWindow = uniIn.row(0).format<int1>().select<2,1>(22);
    vector_ref<int2, 2> ref0XY = uniIn.row(0).format<int2>().select<2,1>(0);
    SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);

    matrix<uchar, 9, 32> imeOut;
    imeOut = CallIme(SRC_AND_REF, uniIn, imeIn);

    matrix<int2,2,4> mv8 = imeOut.row(8).format<int2>().select<8,1>(8);

    vector<int2,2> diff = cm_abs<int2>(mvPred);
    diff = diff > 16;
    if (diff.any()) {
        vector<uint2,4> dist = imeOut.row(5).format<ushort>().select<4,4>();
        vector<int2,2> mvPred0 = 0;
        SetRef(sourceXY, mvPred0, searchWindow, widthHeight, ref0XY);

        imeOut = CallIme(SRC_AND_REF, uniIn, imeIn);

        vector<uint2,4> dist0 = imeOut.row(5).format<ushort>().select<4,4>();
        matrix<int2,2,4> mv8_0 = imeOut.row(8).format<int2>().select<8,1>(8);
        mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist0 < dist);  // 4 MVs
    }

    mvOut = mv8 << 1;  // 4 MVs
}

_GENX_ inline
void ImeOneTier4MV_1(SurfaceIndex DATA64x64, vector_ref<int2, 2> mvPred, SurfaceIndex SRC_AND_REF,
                     matrix_ref<int2, 2, 4> mvOut, uint mbX, uint mbY, UniIn uniIn, matrix<uchar, 2, 32> imeIn, matrix<uchar, 4, 32> fbrIn) // mvPred is updated
{
    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, mbX * 16);
    VME_SET_UNIInput_SrcY(uniIn, mbY * 16);

    // update ref
    // M0.0 Ref0X, Ref0Y
    vector_ref<int2, 2> sourceXY = uniIn.row(0).format<int2>().select<2,1>(4);
    vector<uint1, 2> widthHeight;
    //widthHeight[0] = (height >> 4) - 1;
    //widthHeight[1] = (width >> 4);
    vector_ref<int1, 2> searchWindow = uniIn.row(0).format<int1>().select<2,1>(22);
    vector_ref<int2, 2> ref0XY = uniIn.row(0).format<int2>().select<2,1>(0);
    SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);

    matrix<uchar, 9, 32> imeOut;
    imeOut = CallIme(SRC_AND_REF, uniIn, imeIn);

    vector<int2,2> mv16 = imeOut.row(7).format<int2>().select<2,1>(10);
    matrix<int2,2,4> mv8 = imeOut.row(8).format<int2>().select<8,1>(8); // 4 MVs

    vector<int2,2> diff = cm_abs<int2>(mvPred);
    diff = diff > 16;
    if (diff.any()) {
        uint2 dist16 = imeOut.row(7).format<ushort>()[8];
        vector<uint2,4> dist8 = imeOut.row(7).format<ushort>().select<4,1>(4);

        vector<int2,2> mvPred0 = 0;
        SetRef(sourceXY, mvPred0, searchWindow, widthHeight, ref0XY);

        imeOut = CallIme(SRC_AND_REF, uniIn, imeIn);

        uint2 dist16_0 = imeOut.row(7).format<ushort>()[8];
        vector<uint2,4> dist8_0 = imeOut.row(7).format<ushort>().select<4,1>(4);
        vector<int2,2> mv16_0 = imeOut.row(7).format<int2>().select<2,1>(10);
        matrix<int2,2,4> mv8_0 = imeOut.row(8).format<int2>().select<8,1>(8);

        mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist16_0 < dist16);
        mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
    }

    mvOut = mv8 << 1;

    matrix<uchar, 7, 32> fbrOut16x16;
    fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 0) = mv16.format<uint4>()[0];
    run_vme_fbr(uniIn, fbrIn, SRC_AND_REF, 0, 0, 0, fbrOut16x16);

    // 64x64
    mvPred = SLICE(fbrOut16x16.format<short>(), 16, 2, 1) << 2;
    write(DATA64x64, mbX * INTERDATA_SIZE_BIG, mbY, mvPred);
}

_GENX_ inline
void MeTier2x(SurfaceIndex DATA32x32, SurfaceIndex DATA16x16, SurfaceIndex SRC_AND_REF, vector_ref<int2, 2> mvPred, 
              uint mbX, uint mbY, UniIn uniIn, matrix<uchar,4,32> fbrIn, matrix<uchar, 2, 32> imeIn/*, int rectParts*/)
{
    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, mbX * 16);
    VME_SET_UNIInput_SrcY(uniIn, mbY * 16);

    // M0.0 Ref0X, Ref0Y
    vector_ref<int2, 2> sourceXY = uniIn.row(0).format<int2>().select<2,1>(4);
    vector<uint1, 2> widthHeight;
    //widthHeight[0] = (height >> 4) - 1;
    //widthHeight[1] = (width >> 4);
    vector_ref<int1, 2> searchWindow = uniIn.row(0).format<int1>().select<2,1>(22);
    vector_ref<int2, 2> ref0XY = uniIn.row(0).format<int2>().select<2,1>(0);
    SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);

    matrix<uchar, 9, 32> imeOut;
    imeOut = CallIme(SRC_AND_REF, uniIn, imeIn);

    vector<int2,2> mv16 = imeOut.row(7).format<int2>().select<2,1>(10);
    matrix<int2,2,4> mv8 = imeOut.row(8).format<int2>().select<8,1>(8); // 4 MVs

    vector<int2,2> diff = cm_abs<int2>(mvPred);
    diff = diff > 16;
    if (diff.any()) {
        uint2 dist16 = imeOut.row(7).format<ushort>()[8];
        vector<uint2,4> dist8 = imeOut.row(7).format<ushort>().select<4,1>(4);
        vector<int2,2> mvPred0 = 0;
        SetRef(sourceXY, mvPred0, searchWindow, widthHeight, ref0XY);

        imeOut = CallIme(SRC_AND_REF, uniIn, imeIn);

        uint2 dist16_0 = imeOut.row(7).format<ushort>()[8];
        vector<uint2,4> dist8_0 = imeOut.row(7).format<ushort>().select<4,1>(4);
        vector<int2,2> mv16_0 = imeOut.row(7).format<int2>().select<2,1>(10);
        matrix<int2,2,4> mv8_0 = imeOut.row(8).format<int2>().select<8,1>(8);

        mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist16_0 < dist16);
        mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
    }

    //DebugUniOutput<9>(imeOut);

   
    matrix<uchar, 7, 32> fbrOut16x16;
    VME_SET_UNIInput_FBRMbModeInput(uniIn, 0);
    fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 0) = mv16.format<uint4>()[0]; // motion vectors 16x16
    run_vme_fbr(uniIn, fbrIn, SRC_AND_REF, 0, 0, 0, fbrOut16x16);

    // 32x32
    mvPred = SLICE(fbrOut16x16.format<short>(), 16, 2, 1) << 1;
    write(DATA32x32, mbX * INTERDATA_SIZE_BIG, mbY, mvPred);

    matrix<uchar, 7, 32> fbrOut8x8;
    VME_SET_UNIInput_FBRMbModeInput(uniIn, 3);
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(0, 0) = mv8.format<uint>()[0]; // 8x8_0
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(1, 0) = mv8.format<uint>()[1]; // 8x8_1
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(2, 0) = mv8.format<uint>()[2]; // 8x8_2
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(3, 0) = mv8.format<uint>()[3]; // 8x8_3
    run_vme_fbr(uniIn, fbrIn, SRC_AND_REF, 3, 0, 0, fbrOut8x8);

    // 16x16
    matrix<uint,2,2> mv8x8;
    mv8x8.format<int2,4,2>() = fbrOut8x8.format<int2,7,16>().select<4,1,2,1>(1,0) << 1;
    matrix<uint,2,4> data8x8;
    data8x8.select<2,1,2,2>() = mv8x8;
    data8x8.select<2,1,2,2>(0,1) = 0; // distortions filled in by ME16 kernel
    write(DATA16x16,  mbX * 2 * INTERDATA_SIZE_SMALL, mbY * 2, data8x8);

    //if (rectParts)
    //{
    //    matrix<uchar, 7, 32> fbrOut16x8;
    //    VME_SET_UNIInput_FBRMbModeInput(uniIn, 1);
    //    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
    //    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
    //    fbrIn.format<uint, 4, 8>().select<2, 1, 4, 2>(0, 0) = imeOut.row(8).format<uint>()[0]; // motion vectors 16x8_0
    //    fbrIn.format<uint, 4, 8>().select<2, 1, 4, 2>(2, 0) = imeOut.row(8).format<uint>()[1]; // motion vectors 16x8_1
    //    run_vme_fbr(uniIn, fbrIn, SRC_AND_REF, 1, 0, 0, fbrOut16x8);

    //    matrix<uchar, 7, 32> fbrOut8x16;
    //    VME_SET_UNIInput_FBRMbModeInput(uniIn, 2);
    //    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
    //    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
    //    fbrIn.format<uint, 4, 8>().select<2, 2, 4, 2>(0, 0) = imeOut.row(8).format<uint>()[2]; // motion vectors 8x16_0
    //    fbrIn.format<uint, 4, 8>().select<2, 2, 4, 2>(1, 0) = imeOut.row(8).format<uint>()[3]; // motion vectors 8x16_1
    //    run_vme_fbr(uniIn, fbrIn, SRC_AND_REF, 2, 0, 0, fbrOut8x16);

    //    // 32x16
    //    fbrOut16x8.format<short, 7, 16>().select<2, 2, 2, 1>(1, 0) = fbrOut16x8.format<short, 7, 16>().select<2, 2, 2, 1>(1, 0) << 1;
    //    matrix<uint, 2, 1> mv32x16 = SLICE(fbrOut16x8.format<uint>(), 8, 2, 16);
    //    write(DATA32x16,  mbX * MVDATA_SIZE, mbY * 2, mv32x16);

    //    // 16x32
    //    fbrOut8x16.format<short, 7, 16>().select<2, 1, 2, 1>(1, 0) = fbrOut8x16.format<short, 7, 16>().select<2, 1, 2, 1>(1, 0) << 1;
    //    matrix<uint, 1, 2> mv16x32 = SLICE(fbrOut8x16.format<uint>(), 8, 2, 8);
    //    write(DATA16x32,  mbX * MVDATA_SIZE * 2, mbY, mv16x32);
    //}
}

template <uint H, uint W> inline _GENX_ vector<uint2,W> Sad(matrix_ref<uint1,H,W> m1, matrix_ref<uint1,H,W> m2)
{
    vector<uint2,W> sad = cm_sad2<uint2>(m1.row(0), m2.row(0));
    #pragma unroll
    for (int i = 1; i < H; i++)
        sad = cm_sada2<uint2>(m1.row(i), m2.row(i), sad);
    return sad;
}

extern "C" _GENX_MAIN_
void HmeMe32(
    SurfaceIndex CONTROL, SurfaceIndex REF_16x, SurfaceIndex REF_8x, SurfaceIndex REF_4x, SurfaceIndex REF_2x,
    SurfaceIndex DATA64x64, SurfaceIndex DATA32x32, SurfaceIndex DATA16x16/*, SurfaceIndex DATA32x16, SurfaceIndex DATA16x32, int rectParts*/)
{
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();

    vector<uchar, 120> control;
    read(CONTROL, 0,  control.select<64,1>());
    read(CONTROL, 64, control.select<56,1>(64));

    vector_ref<uchar,32> longSp  = control.select<32,1>();
    vector_ref<uchar,32> shortSp = control.select<32,1>(32);
    vector_ref<uchar,8>  mvCost2x = control.select<8,1>(80);
    vector_ref<uchar,8>  mvCost4x = control.select<8,1>(88);
    vector_ref<uchar,8>  mvCost8x = control.select<8,1>(96);
    vector_ref<uchar,8>  mvCost16x = control.select<8,1>(104);

    uint1 longSpLenSp     = control[64];
    uint1 longSpMaxNumSu  = control[65];
    uint1 shortSpLenSp    = control[66];
    uint1 shortSpMaxNumSu = control[67];
    uint1 mvCostScaleFactor2x  = control[113];
    uint1 mvCostScaleFactor4x  = control[114];
    uint1 mvCostScaleFactor8x  = control[115];
    uint1 mvCostScaleFactor16x = control[116];

    //uint2 width = control.format<uint2>()[34];
    //uint2 height = control.format<uint2>()[35];

    // read MB record data
    UniIn uniIn = 0;

    vector<int2, 2> mvPred = 0;

    // load search path
    matrix<uchar, 2, 32> imeInLongSp;
    matrix<uchar, 2, 32> imeInShortSp;
    imeInLongSp.format<uint1>().select<32,1>() = longSp;
    imeInLongSp.format<uint1>().select<32,1>(32) = 0;
    imeInShortSp.format<uint1>().select<32,1>() = shortSp;
    imeInShortSp.format<uint1>().select<32,1>(32) = 0;

    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, mbX * 16);
    VME_SET_UNIInput_SrcY(uniIn, mbY * 16);

    uint subpartmask = 0x77240000; // BMEDisableFBR=1 InterSAD=2 SubMbPartMask=0x77: 8x8 to have 4MVs in the output
    VME_SET_DWORD(uniIn, 0, 3, subpartmask);

    // M1.1 MaxNumMVs
    VME_SET_UNIInput_MaxNumMVs(uniIn, 32);
    // M0.5 Reference Window Width & Height
    VME_SET_UNIInput_RefW(uniIn, 48);
    VME_SET_UNIInput_RefH(uniIn, 40);

    // M0.0 Ref0X, Ref0Y
    vector_ref<int2, 2> sourceXY = uniIn.row(0).format<int2>().select<2,1>(4);
    vector<uint1, 2> widthHeight;
    //widthHeight[0] = (height >> 4) - 1;
    //widthHeight[1] = (width >> 4);
    vector_ref<int1, 2> searchWindow = uniIn.row(0).format<int1>().select<2,1>(22);
    vector_ref<int2, 2> ref0XY = uniIn.row(0).format<int2>().select<2,1>(0);
    SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY);

    // M1.0-3 Search path parameters & start centers & MaxNumMVs again!!!
    VME_SET_UNIInput_AdaptiveEn(uniIn);
    VME_SET_UNIInput_T8x8FlagForInterEn(uniIn);
    VME_SET_UNIInput_MaxNumMVs(uniIn, 0x3f);
    VME_SET_UNIInput_MaxNumSU(uniIn, longSpMaxNumSu);
    VME_SET_UNIInput_LenSP(uniIn, longSpLenSp);

    // M1.2 Start0X, Start0Y
    vector<int1, 2> start0 = searchWindow;
    start0 = ((start0 - 16) >> 3) & 0x0f;
    uniIn.row(1)[10] = start0[0] | (start0[1] << 4);

    uniIn.row(1)[31] = 0x1;

    // setup mv costs
    uniIn(1,30) = (uniIn(1,30) & ~3) | mvCostScaleFactor16x;
    uniIn.select<1,1,8,1>(2,12) = mvCost16x;

    matrix<uchar, 9, 32> imeOut = CallIme(REF_16x, uniIn, imeInLongSp);

    matrix<int2, 2, 4> mvPred0;  //8x MVs
    mvPred0.format<int2>() = imeOut.row(8).format<int2>().select<8,1>(8);
    vector<uint2,4> dist = imeOut.row(7).format<ushort>().select<4,1>(4);

    mvPred0.format<int2>() <<= 1;

    // 8x tier //

    VME_SET_UNIInput_MaxNumSU(uniIn, longSpMaxNumSu); // long search path
    VME_SET_UNIInput_LenSP(uniIn, longSpLenSp);
    VME_SET_UNIInput_SubMbPartMask(uniIn, 0x77);
    uniIn(1,30) = (uniIn(1,30) & ~3) | mvCostScaleFactor8x;
    uniIn.select<1,1,8,1>(2,12) = mvCost8x;
    matrix<int2, 4, 8> mvPred64; //16x MVs
    for (int yBlk0 = 0, mb128y = mbY * 2, mvInd = 0; yBlk0 < 2; yBlk0++, mb128y++)
        for (int xBlk0 = 0, mb128x = mbX * 2; xBlk0 < 2; xBlk0++, mb128x++, mvInd += 2)
            ImeOneTier4MV(mvPred0.format<int2>().select<2,1>(mvInd), REF_8x,
                mvPred64.select<2,1,4,1>(2*yBlk0, 4*xBlk0), mb128x, mb128y, uniIn, imeInLongSp);


    // 4x tier //

    matrix<uchar,4,32> fbrIn;
    SLICE(fbrIn.format<uint>(), 1, 16, 2) = 0; // zero L1 motion vectors

    VME_SET_UNIInput_SubMbPartMask(uniIn, 0x76);
    uniIn(1,30) = (uniIn(1,30) & ~3) | mvCostScaleFactor4x;
    uniIn.select<1,1,8,1>(2,12) = mvCost4x;
    VME_SET_UNIInput_SubPelMode(uniIn, 3);
    VME_SET_UNIInput_FBRMbModeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);

    matrix<int2, 8, 16> mvPred32; //64x MVs
    for (int yBlk1 = 0, mb64y = mbY * 4, mvInd = 0; yBlk1 < 4; yBlk1++, mb64y++)
        for (int xBlk1 = 0, mb64x = mbX * 4; xBlk1 < 4; xBlk1++, mb64x++, mvInd += 2)
            ImeOneTier4MV_1(DATA64x64, mvPred64.format<int2>().select<2,1>(mvInd), REF_4x,
                mvPred32.select<2,1,4,1>(2*yBlk1, 4*xBlk1), mb64x, mb64y, uniIn, imeInLongSp, fbrIn);

    // 2x tier //

    VME_SET_UNIInput_MaxNumSU(uniIn, shortSpMaxNumSu); // short search path
    VME_SET_UNIInput_LenSP(uniIn, shortSpLenSp);
    uniIn(1,30) = (uniIn(1,30) & ~3) | mvCostScaleFactor2x;
    uniIn.select<1,1,8,1>(2,12) = mvCost2x;

    for (int yBlk2 = 0, mb32y = mbY * 8, mvInd = 0; yBlk2 < 8; yBlk2++, mb32y++)
        for (int xBlk2 = 0, mb32x = mbX * 8; xBlk2 < 8; xBlk2++, mb32x++, mvInd += 2)
            MeTier2x(DATA32x32, DATA16x16, REF_2x, mvPred32.format<int2>().select<2,1>(mvInd), mb32x, mb32y, uniIn, fbrIn, imeInShortSp);
}
