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
#include "../include/genx_hevce_me_common.h"
#include "..\include\genx_hevce_refine_me_p_common_old.h"

#if !defined(target_gen7_5) && !defined(target_gen8) && !defined(CMRT_EMU)
#error One of macro should be defined: target_gen7_5, target_gen8
#endif

#ifdef target_gen8
typedef matrix<uchar, 4, 32> UniIn;
#else
typedef matrix<uchar, 3, 32> UniIn;
#endif

extern "C" _GENX_MAIN_
void MeP16_4MV(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_DIST16x16,
           SurfaceIndex SURF_DIST16x8, SurfaceIndex SURF_DIST8x16, SurfaceIndex SURF_DIST8x8,
           /*SurfaceIndex SURF_DIST8x4, SurfaceIndex SURF_DIST4x8, */SurfaceIndex SURF_MV16x16,
           SurfaceIndex SURF_MV16x8, SurfaceIndex SURF_MV8x16, SurfaceIndex SURF_MV8x8/*,
           SurfaceIndex SURF_MV8x4, SurfaceIndex SURF_MV4x8*/, int rectParts,
           SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_MV32X32, SurfaceIndex SURF_MBDIST_32x32)
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

    uint2 picWidthInMB = width >> 4;
    uint mbIndex = picWidthInMB * mbY + mbX;

    // read MB record data
    UniIn uniIn = 0;
    matrix<uchar, 9, 32> imeOut;
    matrix<uchar, 2, 32> imeIn = 0;
    matrix<uchar, 4, 32> fbrIn;

    // declare parameters for VME
    matrix<uint4, 16, 2> costs = 0;
    vector<int2, 2> mvPred = 0;
    read(SURF_MV16x16, mbX * MVDATA_SIZE, mbY, mvPred); // these pred MVs will be updated later here

    // load search path
    SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control, 0, 64);

    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);

    // M0.3 various prediction parameters
///    VME_SET_DWORD(uniIn, 0, 3, 0x76a40000); // BMEDisableFBR=1 InterSAD=2 SubMbPartMask=0x76: 8x8,16x16
    VME_SET_DWORD(uniIn, 0, 3, 0x70a40000); // BMEDisableFBR=1 InterSAD=2 SubMbPartMask=0x70: 8x8,8x16,16x8,16x16 (rect subparts are for FEI are on)
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

    uniIn.row(1)[6] = 0x20;
    uniIn.row(1)[31] = 0x1;

    vector<int2,2>  ref0       = uniIn.row(0).format<int2>().select<2, 1> (0);
#ifdef target_gen8
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);
#else
    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
#endif
    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0, NULL, costCenter, imeOut);

    //DebugUniOutput<9>(imeOut);

    uchar interMbMode, subMbShape, subMbPredMode;
    VME_GET_UNIOutput_InterMbMode(imeOut, interMbMode);
    VME_GET_UNIOutput_SubMbShape(imeOut, subMbShape);
    VME_GET_UNIOutput_SubMbPredMode(imeOut, subMbPredMode);
    VME_SET_UNIInput_SubPelMode(uniIn, 3);
    VME_SET_UNIInput_BMEDisableFBR(uniIn);
    SLICE(fbrIn.format<uint>(), 1, 16, 2) = 0; // zero L1 motion vectors

    matrix<uchar, 7, 32> fbrOut16x16;
    VME_SET_UNIInput_FBRMbModeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
    fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 0) = imeOut.row(7).format<uint>()[5]; // motion vectors 16x16
    run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 0, 0, 0, fbrOut16x16);

    matrix<uchar, 7, 32> fbrOut8x8;
    subMbShape = 0;
    VME_SET_UNIInput_FBRMbModeInput(uniIn, 3);
    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, subMbShape);
    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, subMbPredMode);
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(0, 0) = imeOut.row(8).format<uint>()[4]; // motion vectors 8x8_0
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(1, 0) = imeOut.row(8).format<uint>()[5]; // motion vectors 8x8_1
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(2, 0) = imeOut.row(8).format<uint>()[6]; // motion vectors 8x8_2
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(3, 0) = imeOut.row(8).format<uint>()[7]; // motion vectors 8x8_3
    run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 3, subMbShape, subMbPredMode, fbrOut8x8);

    // distortions
    // 16x16
    matrix<uint, 1, 1> dist16x16 = SLICE(fbrOut16x16.row(5).format<ushort>(), 0, 1, 1);
    write(SURF_DIST16x16, mbX * DIST_SIZE, mbY, dist16x16);
    // 8x8
    matrix<uint, 2, 2> dist8x8 = SLICE(fbrOut8x8.row(5).format<ushort>(), 0, 4, 4);
    write(SURF_DIST8x8, mbX * DIST_SIZE * 2, mbY * 2, dist8x8);

    // motion vectors
    // 16x16
    write(SURF_MV16x16, mbX * MVDATA_SIZE, mbY, SLICE(fbrOut16x16.format<uint>(), 8, 1, 1));
    // 8x8
    matrix<uint, 2, 2> mv8x8 = SLICE(fbrOut8x8.format<uint>(), 8, 4, 8);
    write(SURF_MV8x8,   mbX * MVDATA_SIZE * 2, mbY * 2, mv8x8);


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

        // distortions
        // 16x8
        matrix<uint, 2, 1> dist16x8 = SLICE(fbrOut16x8.row(5).format<ushort>(), 0, 2, 8);
        write(SURF_DIST16x8, mbX * DIST_SIZE, mbY * 2, dist16x8);
        // 8x16
        matrix<uint, 1, 2> dist8x16 = SLICE(fbrOut8x16.row(5).format<ushort>(), 0, 2, 4);
        write(SURF_DIST8x16, mbX * DIST_SIZE * 2, mbY, dist8x16);

        // motion vectors
        // 16x8
        matrix<uint, 2, 1> mv16x8 = SLICE(fbrOut16x8.format<uint>(), 8, 2, 16);
        write(SURF_MV16x8,  mbX * MVDATA_SIZE, mbY * 2, mv16x8);
        // 8x16
        matrix<uint, 1, 2> mv8x16 = SLICE(fbrOut8x16.format<uint>(), 8, 2, 8);
        write(SURF_MV8x16,  mbX * MVDATA_SIZE * 2, mbY, mv8x16);

        //matrix<uchar, 7, 32> fbrOut8x4;
        //subMbShape = 1 + 4 + 16 + 64;
        //VME_SET_UNIInput_FBRMbModeInput(uniIn, 3);
        //VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, subMbShape);
        //VME_SET_UNIInput_FBRSubPredModeInput(uniIn, subMbPredMode);
        //run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 3, subMbShape, subMbPredMode, fbrOut8x4);

        //matrix<uchar, 7, 32> fbrOut4x8;
        //subMbShape = (1 + 4 + 16 + 64) << 1;
        //VME_SET_UNIInput_FBRMbModeInput(uniIn, 3);
        //VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, subMbShape);
        //VME_SET_UNIInput_FBRSubPredModeInput(uniIn, subMbPredMode);
        //run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 3, subMbShape, subMbPredMode, fbrOut4x8);

        //// 8x4
        //matrix<uint, 4, 2> dist8x4 = SLICE(fbrOut8x4.row(5).format<ushort>(), 0, 8, 2);
        //write(SURF_DIST8x4, mbX * DIST_SIZE * 2, mbY * 4, dist8x4);
        //// 4x8
        //matrix<uint, 2, 4> dist4x8;
        //dist4x8.row(0) = SLICE(fbrOut4x8.row(5).format<ushort>(), 0, 4, 4);
        //dist4x8.row(1) = SLICE(fbrOut4x8.row(5).format<ushort>(), 1, 4, 4);
        //write(SURF_DIST4x8, mbX * DIST_SIZE * 4, mbY * 2, dist4x8);
        //// 8x4
        //matrix<uint, 4, 2> mv8x4 = SLICE(fbrOut8x4.format<uint>(), 8, 8, 4);
        //write(SURF_MV8x4,   mbX * MVDATA_SIZE * 2, mbY * 4, mv8x4);
        //// 4x8
        //matrix<uint, 2, 4> mv4x8;
        //SLICE(mv4x8.format<uint>(), 0, 4, 2) = SLICE(fbrOut4x8.format<uint>(),  8, 4, 8);
        //SLICE(mv4x8.format<uint>(), 1, 4, 2) = SLICE(fbrOut4x8.format<uint>(), 10, 4, 8);
        //write(SURF_MV4x8, mbX * MVDATA_SIZE * 4, mbY * 2, mv4x8);
    }

    if ((mbX & 1) == 0 && (mbY & 1) == 0) {
        vector< uint,9 > sad32x32;
        vector< int2,2 > mv32x32;
        mbX >>= 1;
        mbY >>= 1;
        read(SURF_MV32X32, mbX * MVDATA_SIZE, mbY, mv32x32);

        uint x = mbX * 32;
        uint y = mbY * 32;

        QpelRefine(32, 32, SURF_SRC, SURF_REF, x, y, mv32x32[0], mv32x32[1], sad32x32);

        write(SURF_MBDIST_32x32, mbX * MBDIST_SIZE,      mbY, SLICE1(sad32x32, 0,  8));   // 32bytes is max until BDW
        write(SURF_MBDIST_32x32, mbX * MBDIST_SIZE + 32, mbY, SLICE1(sad32x32, 8, 1));
    }
}
