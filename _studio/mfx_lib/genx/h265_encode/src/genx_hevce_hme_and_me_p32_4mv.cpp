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

#if !defined(target_gen7_5) && !defined(target_gen8) && !defined(CMRT_EMU)
#error One of macro should be defined: target_gen7_5, target_gen8
#endif

#ifdef target_gen8
typedef matrix<uchar, 4, 32> UniIn;
#else
typedef matrix<uchar, 3, 32> UniIn;
#endif

_GENX_ inline
void ImeOneTier4MV(vector_ref<int2, 2> mvPred, SurfaceIndex SURF_SRC_AND_REF, matrix_ref<int2, 2, 4> mvOut,
                   UniIn uniIn, matrix<uchar, 2, 32> imeIn)
{
    matrix<uchar, 9, 32> imeOut;

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
#ifdef target_gen8
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);
#else
    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
#endif
    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0, NULL, costCenter, imeOut);

    mvOut.format<int2>() = imeOut.row(8).format<int2>().select<8,1>(8) << 1;  // 4 MVs
}

_GENX_ inline
void ImeOneTier4MV_1(vector_ref<int2, 2> mvPred, SurfaceIndex SURF_SRC_AND_REF, matrix_ref<int2, 2, 4> mvOut,
                   UniIn uniIn, matrix<uchar, 2, 32> imeIn) // mvPred is updated
{
    matrix<uchar, 9, 32> imeOut;

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
#ifdef target_gen8
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);
#else
    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
#endif
    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0, NULL, costCenter, imeOut);

    mvOut.format<int2>() = imeOut.row(8).format<int2>().select<8,1>(8) << 1;  // 4 MVs
 
    mvPred.format<int2>() = imeOut.row(7).format<int2>().select<2,1>(10) << 2;  // 1 output MV for 16x16
//    vector<int2, 1> dist16x16 = imeOut.row(7).format<int2>().select<1,1>(8);  // dist for 16x16
}

_GENX_ inline
void MeTier2x(SurfaceIndex SURF_SRC_AND_REF, vector_ref<int2, 2> mvPred, SurfaceIndex SURF_MV16x16, SurfaceIndex SURF_MV32x16,
              SurfaceIndex SURF_MV16x32, uint mbX, uint mbY, UniIn uniIn, matrix<uchar, 2, 32> imeIn, int rectParts)
{
    uint x = mbX * 16;
    uint y = mbY * 16;

    // read MB record data
    matrix<uchar, 9, 32> imeOut;
    matrix<uchar, 4, 32> fbrIn;

    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);

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
    VME_GET_UNIOutput_InterMbMode(imeOut, interMbMode); // = 3 : 8x8 only
    VME_GET_UNIOutput_SubMbShape(imeOut, subMbShape);   // = 0 : no subparts
    VME_GET_UNIOutput_SubMbPredMode(imeOut, subMbPredMode); //  = 0 : fwd only
    VME_SET_UNIInput_SubPelMode(uniIn, 3);
    VME_SET_UNIInput_BMEDisableFBR(uniIn);
    SLICE(fbrIn.format<uint>(), 1, 16, 2) = 0; // zero L1 motion vectors

    matrix<uchar, 7, 32> fbrOut16x16;
    VME_SET_UNIInput_FBRMbModeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
    fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 0) = imeOut.row(7).format<uint>()[5]; // motion vectors 16x16
    run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 0, 0, 0, fbrOut16x16);

    // 32x32
    mvPred = SLICE(fbrOut16x16.format<short>(), 16, 2, 1) << 1;

    matrix<uchar, 7, 32> fbrOut8x8;
    subMbShape = 0;
    VME_SET_UNIInput_FBRMbModeInput(uniIn, interMbMode);
    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, subMbShape);
    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, subMbPredMode);

    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(0, 0) = imeOut.row(8).format<uint>()[4]; // 8x8_0
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(1, 0) = imeOut.row(8).format<uint>()[5]; // 8x8_1
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(2, 0) = imeOut.row(8).format<uint>()[6]; // 8x8_2
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(3, 0) = imeOut.row(8).format<uint>()[7]; // 8x8_3

    run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 3, subMbShape, subMbPredMode, fbrOut8x8);

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

extern "C" _GENX_MAIN_
void HmeMe32(
    SurfaceIndex SURF_16XCONTROL, SurfaceIndex SURF_2XCONTROL, SurfaceIndex SURF_REF_16x, SurfaceIndex SURF_REF_8x, SurfaceIndex SURF_REF_4x, SurfaceIndex SURF_REF_2x,
    SurfaceIndex SURF_MV64x64, SurfaceIndex SURF_MV32x32, SurfaceIndex SURF_MV16x16, SurfaceIndex SURF_MV32x16, SurfaceIndex SURF_MV16x32, int rectParts) // SURF_MV32x32 is DS 2x surf
{
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();
    uint x = mbX * 16;
    uint y = mbY * 16;

    vector<uchar, 64> control16x;
    vector<uchar, 64> control2x;
    read(SURF_16XCONTROL, 0, control16x);
    read(SURF_2XCONTROL, 0, control2x);

    uint1 maxNumSu16x = control16x.format<uint1>()[56];
    uint1 lenSp16x = control16x.format<uint1>()[57];
    //uint2 width = control16x.format<uint2>()[30]; // width16x
    //uint2 height = control16x.format<uint2>()[31]; // height16x
    uint1 maxNumSu2x = control2x.format<uint1>()[56];
    uint1 lenSp2x = control2x.format<uint1>()[57];

    // read MB record data
    UniIn uniIn = 0;
    matrix<uchar, 9, 32> imeOut;
    matrix<uchar, 2, 32> imeIn = 0;
    matrix<uchar, 4, 32> fbrIn;

    // declare parameters for VME
    matrix<uint4, 16, 2> costs = 0;
    vector<int2, 2> mvPred = 0;

    // load search path
    SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control16x, 0, 64);

    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);

    uint subpartmask = 0x76040000; // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x78: 8x8,16x16 to have 4MVs in the output
    if (rectParts) {
        subpartmask = 0x70040000; // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x78: 8x8,8x16,16x8,16x16 to have 4MVs in the output (for MeP16)
    }
    VME_SET_DWORD(uniIn, 0, 3, subpartmask);

    // M0.3 various prediction parameters
    //VME_SET_DWORD(uniIn, 0, 3, 0x77040000); // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x77: 8x8 only

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
    VME_SET_UNIInput_MaxNumSU(uniIn, maxNumSu16x);
    VME_SET_UNIInput_LenSP(uniIn, lenSp16x);

    // M1.2 Start0X, Start0Y
    vector<int1, 2> start0 = searchWindow;
    start0 = ((start0 - 16) >> 3) & 0x0f;
    uniIn.row(1)[10] = start0[0] | (start0[1] << 4);

    uniIn.row(1)[31] = 0x1;

    vector<int2,2>  ref0       = uniIn.row(0).format<int2>().select<2, 1> (0);
#ifdef target_gen8
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);
#else
    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
#endif
    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_REF_16x, ref0, NULL, costCenter, imeOut);

    matrix<int2, 2, 4> mvPred0;  //8x MVs
    mvPred0.format<int2>() = imeOut.row(8).format<int2>().select<8,1>(8) << 1;

    const uint xBase0 = x * 2;
    const uint yBase0 = y * 2;
    const uint xBase1 = x * 4;
    const uint yBase1 = y * 4;
    const uint xBase2 = x * 8;
    const uint yBase2 = y * 8;
//    #pragma unroll
    for (int yBlk0 = 0; yBlk0 < 32; yBlk0 += 16) {  // 8x tier
//        #pragma unroll
        for (int xBlk0 = 0; xBlk0 < 32; xBlk0 += 16) {
            matrix<int2, 2, 4> mvPred64; //4x MVs
            // update X and Y
            VME_SET_UNIInput_SrcX(uniIn, xBase0 + xBlk0);
            VME_SET_UNIInput_SrcY(uniIn, yBase0 + yBlk0);

            int mvInd0 = ((yBlk0 >> 2) | (xBlk0 >> 3)); // choose 8x8_0, 8x8_1, 8x8_2 or 8x8_3
            // load search path
            SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control16x, 0, 64);
            VME_SET_UNIInput_MaxNumSU(uniIn, maxNumSu16x);
            VME_SET_UNIInput_LenSP(uniIn, lenSp16x);
            ImeOneTier4MV(mvPred0.format<int2>().select<2,1>(mvInd0), SURF_REF_8x, mvPred64, uniIn, imeIn);

//            #pragma unroll
            for (int yBlk1 = 0; yBlk1 < 32; yBlk1 += 16) {  // 4x tier
//                #pragma unroll
                for (int xBlk1 = 0; xBlk1 < 32; xBlk1 += 16) {
                    matrix<int2, 2, 4> mvPred32; //2x MVs
                    // update X and Y
                    VME_SET_UNIInput_SrcX(uniIn, xBase1 + xBlk0 * 2 + xBlk1);
                    VME_SET_UNIInput_SrcY(uniIn, yBase1 + yBlk0 * 2 + yBlk1);

                    int mvInd1 = ((yBlk1 >> 2) | (xBlk1 >> 3)); // choose 8x8_0, 8x8_1, 8x8_2 or 8x8_3

                    // load search path
                    SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control16x, 0, 64);
                    VME_SET_UNIInput_MaxNumSU(uniIn, maxNumSu16x);
                    VME_SET_UNIInput_LenSP(uniIn, lenSp16x);
                    ImeOneTier4MV_1(mvPred64.format<int2>().select<2,1>(mvInd1), SURF_REF_4x, mvPred32, uniIn, imeIn);
//                    write(SURF_MV64x64, (xBase1 + xBlk0 * 2 + xBlk1) / 16 * MVDATA_SIZE, (yBase1 + yBlk0 * 2 + yBlk1) / 16, mvPred64.format<int2>().select<2,1>(mvInd1));

//                    #pragma unroll
                    for (int yBlk2 = 0; yBlk2 < 32; yBlk2 += 16) {  // 2x tier
//                        #pragma unroll
                        for (int xBlk2 = 0; xBlk2 < 32; xBlk2 += 16) {
                            uint mbXme = (xBase2 + xBlk0 * 4  + xBlk1 * 2 + xBlk2) / 16;
                            uint mbYme = (yBase2 + yBlk0 * 4  + yBlk1 * 2 + yBlk2) / 16;

                            int mvInd2 = ((yBlk2 >> 2) | (xBlk2 >> 3)); // choose 8x8_0, 8x8_1, 8x8_2 or 8x8_3
                            //// load search path
                            SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control2x, 0, 64);
                            VME_SET_UNIInput_MaxNumSU(uniIn, maxNumSu2x);
                            VME_SET_UNIInput_LenSP(uniIn, lenSp2x);
                            MeTier2x(SURF_REF_2x, mvPred32.format<int2>().select<2,1>(mvInd2), SURF_MV16x16, SURF_MV32x16, SURF_MV16x32, mbXme, mbYme, uniIn, imeIn, rectParts);
                        }
                    }

                    write(SURF_MV32x32, (xBase1 + xBlk0 * 2 + xBlk1) / 8 * MVDATA_SIZE, (yBase1 + yBlk0 * 2 + yBlk1) / 8, mvPred32);
                }
            }
            write(SURF_MV64x64, (xBase0 + xBlk0) / 8 * MVDATA_SIZE, (yBase0 + yBlk0) / 8, mvPred64);
        }
    }

}
