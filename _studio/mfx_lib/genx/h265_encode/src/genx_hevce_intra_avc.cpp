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
typedef matrix<uchar, 3, 32> UniIn;
typedef matrix_ref<uchar, 3, 32> UniInRef;
#else
typedef matrix<uchar, 4, 32> UniIn;
typedef matrix_ref<uchar, 4, 32> UniInRef;
#endif


#define VME_COPY_DWORD(dst, r, c, src, srcIdx) (dst.row(r).format<uint4>().select<1, 1>(c) = src.format<uint4>().select<1, 1>(srcIdx))
#define GET_CURBE_PictureWidth(obj) (obj[18])
#define VME_SET_CornerNeighborPixel0(p, v) (VME_Input_S1(p, uint1, 1,  7)  = v)
#define VME_CLEAR_UNIInput_IntraCornerSwap(p) (VME_Input_S1(p, uint1, 1, 28) &= 0x7f)

enum { MBINTRADIST_SIZE = 4 };
enum { CURBEDATA_SIZE = 160 };

_GENX_ inline
void SetUpVmeIntra(UniInRef uniIn, matrix_ref<uchar, 4, 32> sicIn,
                   vector_ref<uchar, CURBEDATA_SIZE> curbeData, SurfaceIndex SURF_SRC,
                   uint mbX, uint mbY)
{
    vector<uchar, 8>     leftMbIntraModes = 0x22;
    vector<uchar, 8>     upperMbIntraModes = 0x22;
    matrix<uint1, 16, 1> leftBlockValues = 0;
    matrix<uint1, 1, 28> topBlocksValues = 0;
    matrix<uint1, 8, 2>  chromaLeftBlockValues = 0;
    matrix<uint1, 1, 20> chromaTopBlocksValues = 0;
    ushort               availFlagA = 0;
    ushort               availFlagB = 0;
    ushort               availFlagC = 0;
    ushort               availFlagD = 0;

    uint x = mbX * 16;
    uint y = mbY * 16;
    int offset = 0;

    uint PicWidthInMB = GET_CURBE_PictureWidth(curbeData);
    uint MbIndex = PicWidthInMB * mbY + mbX;

    INIT_VME_UNIINPUT(uniIn);
    INIT_VME_SICINPUT(sicIn);

    uniIn.row(1).format<uint1>().select<1,1>(29) = 0; // availability flags (MbIntraStruct)
    // copy dwords from CURBE data into universal VME header
    // register M0
    // M0.0 Reference 0 Delta
    // M0.1 Reference 1 Delta
    // M0.2 Source X/Y
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);
    // M0.3
    VME_COPY_DWORD(uniIn, 0, 3, curbeData, 3);
    // M0.4 reserved
    // M0.5
    VME_COPY_DWORD(uniIn, 0, 5, curbeData, 5);
    // M0.6 debug
    // M0.7 debug

    // register M1
    // M1.0/1.1/1.2
    uniIn.row(1).format<uint>().select<3, 1> (0) = curbeData.format<uint>().select<3, 1> (0);
    VME_CLEAR_UNIInput_SkipModeEn(uniIn);
    // M1.3 Weighted SAD
    // M1.4 Cost center 0
    // M1.5 Cost center 1
    // M1.6 Fwd/Bwd Block RefID
    // M1.7 various prediction parameters
    VME_COPY_DWORD(uniIn, 1, 7, curbeData, 7);
    VME_CLEAR_UNIInput_IntraCornerSwap(uniIn);

    if (mbX)
    {
        uniIn.row(1).format<uint1>().select<1,1>(29) |= 0x60; // left mb is available (bit5=bit6)
        matrix<uint1, 16, 4> temp;
        read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x - 4, y, temp);
        leftBlockValues = temp.select<16, 1, 1, 1> (0, 3);
    }

    if (mbY)
    {
        if (mbX == 0)
        {
            uniIn.row(1).format<uint1>().select<1,1>(29) |= 0x14; // upper and upper-right mbs are available
            // read luminance samples
            matrix<uint1, 1, 24> temp;
            read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x, y - 1, temp);
            topBlocksValues.select<1, 1, 24, 1> (0, 4) = temp;
        }
        else if (mbX + 1 == PicWidthInMB)
        {
            uniIn.row(1).format<uint1>().select<1,1>(29) |= 0x18; // upper and upper-left mbs are available
            matrix<uint1, 1, 20> temp;
            read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x - 4, y - 1, temp);
            topBlocksValues.select<1,1,20,1>(0, 0) = temp;
            topBlocksValues.select<1,1, 8,1>(0,20) = temp.row(0)[19];
        }
        else
        {
            uniIn.row(1).format<uint1>().select<1,1>(29) |= 0x1C; // upper, upper-left and upper-right mbs are available
            read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, x - 4, y - 1, topBlocksValues);
        }
    }

    // register M2
    // M2.0/2.1/2.2/2.3/2.4
    uniIn.row(2).format<uint>().select<5, 1> (0) = curbeData.format<uint>().select<5, 1> (8);
    // M2.5 FBR parameters
    // M2.6 SIC Forward Transform Coeff Threshold Matrix
    // M2.7 SIC Forward Transform Coeff Threshold Matrix

    //
    // set up the SIC message
    //

    // register M0
    // M0.0 Ref0 Skip Center 0
    // M0.1 Ref1 Skip Center 0
    // M0.2 Ref0 Skip Center 1
    // M0.3 Ref1 Skip Center 1
    // M0.4 Ref0 Skip Center 2
    // M0.5 Ref1 Skip Center 2
    // M0.6 Ref0 Skip Center 3
    // M0.3 Ref1 Skip Center 3

    // register M1
    // M1.0 ACV Intra 4x4/8x8 mode mask
    // M1.1 AVC Intra Modes
    sicIn.row(1).format<uint>().select<2, 1> (0) = curbeData.format<uint>().select<2, 1> (30);
    VME_SET_CornerNeighborPixel0(sicIn, topBlocksValues(0, 3));

    // M1.2-1.7 Neighbor pixel Luma values
    sicIn.row(1).format<uint>().select<6, 1> (2) = topBlocksValues.format<uint>().select<6, 1> (1);

    // register M2
    // M2.0-2.3 Neighbor pixel luma values
    sicIn.row(2).format<uint>().select<4, 1> (0) = leftBlockValues.format<uint>().select<4, 1> (0);
    // M2.4 Intra Predictor Mode
    sicIn.row(2).format<uint1>().select<1, 1>(16) = 0;
    sicIn.row(2).format<uint1>().select<1, 1>(17) = 0;
}

extern "C" _GENX_MAIN_
void IntraAvc(SurfaceIndex SURF_CURBE_DATA, SurfaceIndex SURF_SRC_AND_REF,
              SurfaceIndex SURF_SRC, SurfaceIndex SURF_MBDISTINTRA)
{
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();

    vector<uchar, CURBEDATA_SIZE> curbeData;
    read(SURF_CURBE_DATA, 0,   curbeData.select<128,1>(0));
    read(SURF_CURBE_DATA, 128, curbeData.select<32, 1>(128));

    UniIn  uniInIntra;
    matrix<uchar, 4, 32> sicIn;
    matrix<uchar, 7, 32> sicOut;
    SetUpVmeIntra(uniInIntra, sicIn, curbeData, SURF_SRC, mbX, mbY);
    run_vme_sic(uniInIntra, sicIn, SURF_SRC_AND_REF, sicOut);

    write(SURF_MBDISTINTRA, mbX * MBINTRADIST_SIZE, mbY, SLICE1(sicOut.row(0), 12, 4));  // !!!sergo: use sicOut
}
