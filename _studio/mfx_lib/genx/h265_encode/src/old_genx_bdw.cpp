//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#if 0

#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505) 
#include <cm.h>
#include <genx_vme.h>
#include "../include/genx_h265_cmcode_bdw.h"

#define MBINTRADIST_SIZE    4 // mfxU16 intraDist;mfxU16 reserved;
#define MBDIST_SIZE     64  // 16*mfxU32
#define MVDATA_SIZE     4 // mfxI16Pair
#define MBDIST_SIZE2    64 * 2  // 48*mfxU32
#define MVDATA_SIZE2    4 * 3   // 3 MVs
#define DIST_SIZE       4
#define CURBEDATA_SIZE  160             //sizeof(H265EncCURBEData_t)

#define VME_CLEAR_UNIInput_IntraCornerSwap(p) (VME_Input_S1(p, uint1, 1, 28) &= 0x7f)
#define VME_SET_CornerNeighborPixel0(p, v)    (VME_Input_S1(p, uint1, 1,  7)  = v)

#define SELECT_N_ROWS(m, from, nrows) m.select<nrows, 1, m.COLS, 1>(from)
#define SELECT_N_COLS(m, from, ncols) m.select<m.ROWS, 1, ncols, 1>(0, from)
#define SELECT_N_ROWS_TEMPL(m, from, nrows) m.template select<nrows, 1, m.COLS, 1>(from)
#define SELECT_N_COLS_TEMPL(m, from, ncols) m.template select<m.ROWS, 1, ncols, 1>(0, from)
#define NROWS(m, from, nrows) SELECT_N_ROWS_TEMPL(m, from, nrows)
#define NCOLS(m, from, ncols) SELECT_N_COLS_TEMPL(m, from, ncols)
#define SLICE(VEC, FROM, HOWMANY, STEP) ((VEC).select<HOWMANY, STEP>(FROM))
#define SLICE1(VEC, FROM, HOWMANY) SLICE(VEC, FROM, HOWMANY, 1)

#define VME_COPY_DWORD(dst, r, c, src, srcIdx)  \
    (dst.row(r).format<uint4>().select<1, 1>(c) = src.format<uint4>().select<1, 1>(srcIdx))

#define VME_COPY_N_DWORD(dst, r, c, src, srcIdx, dwordCount)  \
    (dst.row(r).format<uint4>().select<dwordCount, 1>(c) = src.format<uint4>().select<dwordCount, 1>(srcIdx))

#define VME_SET_DWORD(dst, r, c, src)           \
    dst.row(r).format<uint4>().select<1, 1>(c) = src

#define VME_INIT_H265_PACK(mbData) \
    {mbData = 0;}

#define VME_GET_UNIInput_SkipModeEn(p) (VME_Output_S1(p, uint1, 1, 0) & 0x1)

#define VME_GET_UNIInput_FTEnable(p) (VME_Output_S1(p, uint1, 0, 14) & 0x2)

#define VME_SET_UNIInput_BlkSkipEnable(p) (VME_Input_S1(p, uint1, 0, 14) |= 0x08)

#define VME_CLEAR_UNIInput_NonSkipModeAdded(p) (VME_Input_S1(p, uint1, 1, 28) &= 0x0bf)

#define VME_CLEAR_UNIInput_NonSkipMvAdded(p) (VME_Input_S1(p, uint1, 1, 28) &= 0x0df)

#define GET_CURBE_T8x8FlagForInterEn(obj) \
    ((obj[0] >> 7) & 1)

#define GET_CURBE_SubPelMode(obj) \
    ((obj[13] >> 4) & 3)

#define GET_CURBE_FTQSkipEnable(obj) \
    ((obj[14] >> 1) & 1)

#define GET_CURBE_LenSP(obj) \
    (obj[8])

#define GET_CURBE_SliceMacroblockHeight(obj) \
    (obj[16] + 1)

#define GET_CURBE_PictureHeight(obj) \
    (obj[17] + 1)

#define GET_CURBE_PictureWidth(obj) \
    (obj[18])

#define GET_CURBE_Log2MvScaleFactor(obj) \
    (obj[19] & 0xf)

#define GET_CURBE_Log2MbScaleFactor(obj) \
    ((obj[19] >> 4) & 0xf)

#define SET_CURBE_PictureHeight(obj, h) \
    (obj[17] = h - 1)

#define SET_CURBE_PictureWidth(obj, w) \
    (obj[18] = w)

#define GET_CURBE_SubMbPartMask(obj) \
    (obj[15])

#define SET_CURBE_SubMbPartMask(obj, v) \
    (obj[15] = v)

#define GET_CURBE_QpPrimeY(obj) \
    (obj[52])

#define GET_CURBE_FTXCoeffThresh(obj) \
    ((obj).format<uint4> ().select<2, 1> (14))

#define GET_CURBE_SkipVal(obj) \
    (obj.format<uint2> ()[64])

#define GET_CURBE_HMECombineThreshold(obj) \
    (obj[144])

#define GET_CURBE_HMECombineOverlap(obj) \
    (obj[145] & 3)

#define GET_CURBE_AllFractional(obj) \
    ((obj[145] >> 2) & 1)

#define GET_CURBE_UseHMEPredictor(obj) \
    (obj[150] & 0x10)

#define GET_CURBE_isFwdFrameShortTermRef(obj) \
    ((obj[150] >> 6) & 1)

#define VME_SET_UNIOutput_InterDistortion(uniOut, v)    \
    (VME_Input_S1(uniOut, uint2, 0, 4) = v)

#define VME_SET_IMEOutput_Rec0_16x16_Distortion(imeOut, v)  \
    (VME_Input_S1(imeOut, uint2, 0, 8) = v)

#define VME_SET_IMEOutput_Rec0_Distortions(imeOut, v)   \
    (VME_Input_S1(imeOut, uint4, 0, 0) = v);            \
    (VME_Input_S1(imeOut, uint4, 0, 1) = v);            \
    (VME_Input_S1(imeOut, uint4, 0, 2) = v);            \
    (VME_Input_S1(imeOut, uint4, 0, 3) = v)

#define VME_SET_IMEOutput_Rec1_16x16_Distortion(imeOut, v)  \
    (VME_Input_S1(imeOut, uint2, 2, 8) = v)

#define VME_SET_IMEOutput_Rec1_Distortions(imeOut, v)   \
    (VME_Input_S1(imeOut, uint4, 2, 0) = v);            \
    (VME_Input_S1(imeOut, uint4, 2, 1) = v);            \
    (VME_Input_S1(imeOut, uint4, 2, 2) = v);            \
    (VME_Input_S1(imeOut, uint4, 2, 3) = v)

#define VME_SET_UNIOutput_BestIntraDistortion(uniOut, v)    \
    (VME_Input_S1(uniOut, uint2, 0, 6) = v)

#define VME_SET_IMEInput_Distortions(p, v)          \
    (p.row(2).format<uint4>().select<4, 1>(0) = v); \
    (p.row(4).format<uint4>().select<4, 1>(0) = v)

#define VME_SET_UNIOutput_SkipRawDistortion(p, v) (VME_Input_S1(p, uint2, 0, 5) = v)

#define VME_SET_UNIInput_SkipModeEnable(obj, value) \
    (obj.row(1)[0] = ((obj.row(1)[0] & 0x0fe) | (value & 1)))

#define VME_SET_UNIInput_ShapeMask(p, v) (VME_Input_S1(p, uint1, 0, 15) = v)


_GENX_ inline
void SetRef(vector_ref<int2, 2> source,         // IN:  SourceX, SourceY
            vector<int2, 2>     predictor,      // IN:  mv predictor
            vector_ref<int1, 2> searchWindow,   // IN:  reference window w/h
            vector<uint1, 2>    picSize,        // IN:  pic size w/h
            vector_ref<int2, 2> reference)      // OUT: Ref0X, Ref0Y
{
    vector<int2, 2> Width = (searchWindow - 16) >> 1;
    vector<int2, 2> MaxMvLen;
    vector<int2, 2> RefSize;
    vector<short, 2> mask;
    vector<int2, 2> res, otherRes;

    // set up parameters
///    MaxMvLen[0] = 512;
    MaxMvLen[0] = 0x7fff / 4;
    MaxMvLen[1] = 0x7fff / 4;
    RefSize[0] = picSize[1] * 16;
    RefSize[1] = (picSize[0] + 1) * 16;

    /* fields and MBAFF are not supported */

    // remove quater pixel fraction
    predictor >>= 2;

    //
    // set the reference position
    //

    reference = predictor;
    reference[1] &= -2;
    reference -= Width;

    res = MaxMvLen - Width;
    mask = (predictor > res);
    otherRes = MaxMvLen - (searchWindow - 16);
    reference.merge(otherRes, mask);

    res = -res;
    mask = (predictor < res);
    otherRes = -MaxMvLen;
    reference.merge(otherRes, mask);

    //
    // saturate to reference dimension
    //

/*
    res = reference + source;
    mask = (RefSize <= res);
    otherRes = ((RefSize - 1) & ~3) - source;
    reference.merge(otherRes, mask);

    res = reference + source + searchWindow;
    mask = (0 >= res);
    otherRes = ((-searchWindow + 5) & ~3) - source;
    reference.merge(otherRes, mask);

    reference[1] &= -2;
*/

} // void SetRef(matrix_ref<uchar, 3, 32> uniIn,

_GENX_ inline void LoadCostsRawMe(
    matrix_ref<uchar, 3, 32>          uniIn,
    vector_ref<uchar, CURBEDATA_SIZE> curbeData)
{
    uniIn.row(2).format<uint4>().select<5,1>(0) = curbeData.format<uint4>().select<5,1>(8);
}

_GENX_ inline void LoadSearchPath(
    matrix_ref<uchar, 2, 32>          imeIn,
    vector_ref<uchar, CURBEDATA_SIZE> curbeData)
{
    imeIn = curbeData.select<64, 1>(64);
}

extern "C" _GENX_MAIN_  void
DownSampleMB(SurfaceIndex SurfIndex,
             SurfaceIndex Surf2XIndex)
{   // Luma only
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();
    uint ix = mbX << 4;
    uint iy = mbY << 4;
    uint ox2x = mbX << 3;
    uint oy2x = mbY << 3;

    matrix<uint1,16,16> inMb;
    matrix<uint1,8,8> outMb2x;

    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix, iy, inMb);

    matrix<uint2,8,16> sum8x16_16;
    matrix<uint2,8,8> sum8x8_16;

    // for 2X
    sum8x16_16.select<8,1,16,1>(0,0) = inMb.select<8,2,16,1>(0,0) + inMb.select<8,2,16,1>(1,0);
    sum8x8_16.select<8,1,8,1>(0,0) = sum8x16_16.select<8,1,8,2>(0,0) + sum8x16_16.select<8,1,8,2>(0,1);

    sum8x8_16.select<8,1,8,1>(0,0) += 2;
    outMb2x = sum8x8_16.select<8,1,8,1>(0,0) >> 2;

    write_plane(Surf2XIndex, GENX_SURFACE_Y_PLANE, ox2x, oy2x, outMb2x);
}

extern "C" _GENX_MAIN_  void
DownSampleMB2t(SurfaceIndex SurfIndex,  // 2 tiers
               SurfaceIndex Surf2XIndex,
               SurfaceIndex Surf4XIndex)
{   // Luma only
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();
    uint ix = mbX << 5; // src 32x32
    uint iy = mbY << 5;
    uint ox2x = mbX << 4; // dst 16x16
    uint oy2x = mbY << 4;
    uint ox4x = mbX << 3; // dst 8x8
    uint oy4x = mbY << 3;

    matrix<uint1,32,32> inMb;
    matrix<uint1,16,16> outMb2x;
    matrix<uint1,8,8> outMb4x;
    matrix<uint2,16,32> sum16x32_16;
    matrix<uint2,8,16> sum8x16_16;

    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix +  0, iy +  0, inMb.select<16,1,16,1>(0,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy +  0, inMb.select<16,1,16,1>(0,16));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix +  0, iy + 16, inMb.select<16,1,16,1>(16,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy + 16, inMb.select<16,1,16,1>(16,16));

    sum16x32_16.select<16,1,32,1>(0,0) = inMb.select<16,2,32,1>(0,0) + inMb.select<16,2,32,1>(1,0);
    sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

    sum16x32_16.select<16,1,16,1>(0,0) += 2;
    outMb2x = sum16x32_16.select<16,1,16,1>(0,0) >> 2;

    // for 4X
    sum8x16_16.select<8,1,16,1>(0,0) = outMb2x.select<8,2,16,1>(0,0) + outMb2x.select<8,2,16,1>(1,0);
    sum8x16_16.select<8,1,8,1>(0,0) = sum8x16_16.select<8,1,8,2>(0,0) + sum8x16_16.select<8,1,8,2>(0,1);

    sum8x16_16.select<8,1,8,1>(0,0) += 2;
    outMb4x = sum8x16_16.select<8,1,8,1>(0,0) >> 2;

    write_plane(Surf4XIndex, GENX_SURFACE_Y_PLANE, ox4x, oy4x, outMb4x);
    write_plane(Surf2XIndex, GENX_SURFACE_Y_PLANE, ox2x, oy2x, outMb2x);
}


extern "C" _GENX_MAIN_  void
DownSampleMB4t(SurfaceIndex SurfIndex,  // 4 tiers
               SurfaceIndex Surf2XIndex,
               SurfaceIndex Surf4XIndex,
               SurfaceIndex Surf8XIndex,
               SurfaceIndex Surf16XIndex)
{   // Luma only
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();
    uint ix = mbX << 8; // src 256x256
    uint iy = mbY << 8;
    uint ox2x = mbX << 7; // dst 128x128
    uint oy2x = mbY << 7;
    uint ox4x = mbX << 6; // dst 64x64
    uint oy4x = mbY << 6;
    uint ox8x = mbX << 5; // dst 32x32
    uint oy8x = mbY << 5;
    uint ox16x = mbX << 4; // dst 16x16
    uint oy16x = mbY << 4;

    matrix<uint1,32,32> inMb;
    matrix<uint1,16,16> outMb2x;
    matrix<uint1,16,16> outMb4x;
    matrix<uint1,16,16> outMb8x;
    matrix<uint1,16,16> outMb16x;
    matrix<uint2,16,32> sum16x32_16;
    matrix<uint2,8,16> sum8x16_16;

    // process 256x256
    for (uint y128_pos = 0; y128_pos < 2; y128_pos ++) {
        for (uint x128_pos = 0; x128_pos < 2; x128_pos ++) {
            uint y128 = iy + (y128_pos << 7);
            uint x128 = ix + (x128_pos << 7);
            // process 128x128
            for (uint y64_pos = 0; y64_pos < 2; y64_pos ++) {
                for (uint x64_pos = 0; x64_pos < 2; x64_pos ++) {
                    uint y64 = y128 + (y64_pos << 6);
                    uint x64 = x128 + (x64_pos << 6);
                    // process 64x64
                    for (uint y32_pos = 0; y32_pos < 2; y32_pos ++) {
                        for (uint x32_pos = 0; x32_pos < 2; x32_pos ++) {
                            uint y32 = y64 + (y32_pos << 5);
                            uint x32 = x64 + (x32_pos << 5);
                            read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, x32 +  0, y32 +  0, inMb.select<16,1,16,1>(0,0));
                            read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, x32 + 16, y32 +  0, inMb.select<16,1,16,1>(0,16));
                            read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, x32 +  0, y32 + 16, inMb.select<16,1,16,1>(16,0));
                            read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, x32 + 16, y32 + 16, inMb.select<16,1,16,1>(16,16));

                            sum16x32_16.select<16,1,32,1>(0,0) = inMb.select<16,2,32,1>(0,0) + inMb.select<16,2,32,1>(1,0);
                            sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

                            sum16x32_16.select<16,1,16,1>(0,0) += 2;
                            outMb2x = sum16x32_16.select<16,1,16,1>(0,0) >> 2;
                            write_plane(Surf2XIndex, GENX_SURFACE_Y_PLANE, ox2x + (x128_pos<<6) + (x64_pos<<5) + (x32_pos<<4), oy2x + (y128_pos<<6) + (y64_pos<<5) + (y32_pos<<4), outMb2x);

                            sum8x16_16.select<8,1,16,1>(0,0) = outMb2x.select<8,2,16,1>(0,0) + outMb2x.select<8,2,16,1>(1,0);
                            sum8x16_16.select<8,1,8,1>(0,0) = sum8x16_16.select<8,1,8,2>(0,0) + sum8x16_16.select<8,1,8,2>(0,1);

                            sum8x16_16.select<8,1,8,1>(0,0) += 2;
                            outMb4x.select<8,1,8,1>((y32_pos<<3),(x32_pos<<3)) = sum8x16_16.select<8,1,8,1>(0,0) >> 2;
                        }
                    }

                    write_plane(Surf4XIndex, GENX_SURFACE_Y_PLANE, ox4x + (x128_pos<<5) + (x64_pos<<4), oy4x + (y128_pos<<5) + (y64_pos<<4), outMb4x);

                    sum8x16_16.select<8,1,16,1>(0,0) = outMb4x.select<8,2,16,1>(0,0) + outMb4x.select<8,2,16,1>(1,0);
                    sum8x16_16.select<8,1,8,1>(0,0) = sum8x16_16.select<8,1,8,2>(0,0) + sum8x16_16.select<8,1,8,2>(0,1);

                    sum8x16_16.select<8,1,8,1>(0,0) += 2;
                    outMb8x.select<8,1,8,1>((y64_pos<<3),(x64_pos<<3)) = sum8x16_16.select<8,1,8,1>(0,0) >> 2;
                }
            }
            
            write_plane(Surf8XIndex, GENX_SURFACE_Y_PLANE, ox8x + (x128_pos<<4), oy8x + (y128_pos<<4), outMb8x);

            sum8x16_16.select<8,1,16,1>(0,0) = outMb8x.select<8,2,16,1>(0,0) + outMb8x.select<8,2,16,1>(1,0);
            sum8x16_16.select<8,1,8,1>(0,0) = sum8x16_16.select<8,1,8,2>(0,0) + sum8x16_16.select<8,1,8,2>(0,1);

            sum8x16_16.select<8,1,8,1>(0,0) += 2;
            outMb16x.select<8,1,8,1>((y128_pos<<3),(x128_pos<<3)) = sum8x16_16.select<8,1,8,1>(0,0) >> 2;
        }
    }

    write_plane(Surf16XIndex, GENX_SURFACE_Y_PLANE, ox16x, oy16x, outMb16x);

}

_GENX_ inline
void PrepareFractionalCall(matrix_ref<uchar, 3, 32> uniIn,
                           matrix_ref<uchar, 4, 32> fbrIn,
                           matrix_ref<uchar, 7, 32> uniOut,
                           uint1 CheckBiDir)
{
    U8 FBRMbMode, FBRSubMbShape, FBRSubPredMode;

    // copy mb mode
    VME_GET_UNIOutput_InterMbMode(uniOut, FBRMbMode);
    VME_SET_UNIInput_FBRMbModeInput(uniIn, FBRMbMode);

    // copy sub mb shape
    VME_GET_UNIOutput_SubMbShape(uniOut, FBRSubMbShape);
    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, FBRSubMbShape);

    // copy sub mb prediction modes
    VME_GET_UNIOutput_SubMbPredMode(uniOut, FBRSubPredMode);
    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, FBRSubPredMode);

    if (!CheckBiDir || FBRSubMbShape)
        VME_SET_UNIInput_BMEDisableFBR(uniIn);
    else
        VME_CLEAR_UNIInput_BMEDisableFBR(uniIn);

    // copy MVs
    fbrIn = uniOut.select<4, 1, 32, 1> (1, 0);

} // void PrepareFractionalCall(matrix_ref<uchar, 3, 32> uniIn,

#define BIT_FIELD(UINT4, OFF, NBITS) ((UINT4 >> OFF) & ((1 << NBITS) - 1))

#define VME_GET_UNIOutput_Transform8x8Flag(p, v)          (v = BIT_FIELD(VME_Output_S1(p, uint4, 0, 0), 15,  1))
#define VME_GET_UNIOutput_ExtendedForm(p, v)              (v = BIT_FIELD(VME_Output_S1(p, uint4, 0, 0), 23,  1))
#define VME_GET_UNIOutput_Ref0BorderReached(p, v)         (v = BIT_FIELD(VME_Output_S1(p, uint4, 0, 1),  0,  4))
#define VME_GET_UNIOutput_Ref1BorderReached(p, v)         (v = BIT_FIELD(VME_Output_S1(p, uint4, 0, 1),  4,  4))
#define VME_GET_UNIOutput_SearchPathLength(p, v)          (v = BIT_FIELD(VME_Output_S1(p, uint4, 0, 1),  8,  8))
#define VME_GET_UNIOutput_SumInterDistL1Upper(p, v)       (v = BIT_FIELD(VME_Output_S1(p, uint4, 0, 1), 16, 11))
#define VME_GET_UNIOutput_EarlyImeStop(p, v)              (v = BIT_FIELD(VME_Output_S1(p, uint4, 0, 6), 24,  1))
#define VME_GET_UNIOutput_MaxMVOccurred(p, v)             (v = BIT_FIELD(VME_Output_S1(p, uint4, 0, 6), 25,  1))
#define VME_GET_UNIOutput_AlternateSearchPathLength(p, v) (v = BIT_FIELD(VME_Output_S1(p, uint4, 0, 6), 26,  6))
#define VME_GET_UNIOutput_TotalVMEComputeClocks(p, v)     (v = BIT_FIELD(VME_Output_S1(p, uint4, 0, 7),  0, 16))
#define VME_GET_UNIOutput_TotalVMEStalledClocks(p, v)     (v = BIT_FIELD(VME_Output_S1(p, uint4, 0, 7), 16, 16))
#define VME_GET_UNIOutput_Blk0LumaCoeffSum(p, v)          (v = BIT_FIELD(VME_Output_S1(p, uint4, 6, 2),  0, 16))
#define VME_GET_UNIOutput_Blk1LumaCoeffSum(p, v)          (v = BIT_FIELD(VME_Output_S1(p, uint4, 6, 2), 16, 16))
#define VME_GET_UNIOutput_Blk2LumaCoeffSum(p, v)          (v = BIT_FIELD(VME_Output_S1(p, uint4, 6, 3),  0, 16))
#define VME_GET_UNIOutput_Blk3LumaCoeffSum(p, v)          (v = BIT_FIELD(VME_Output_S1(p, uint4, 6, 3), 16, 16))
#define VME_GET_UNIOutput_SumInterDistL1Lower(p, v)       (v = BIT_FIELD(VME_Output_S1(p, uint4, 6, 4), 16, 16))
#define VME_GET_UNIOutput_Blbk0ChromaCbCoeffSum(p, v)     (v = BIT_FIELD(VME_Output_S1(p, uint4, 6, 5),  0, 16))
#define VME_GET_UNIOutput_Blbk0ChromaCrCoeffSum(p, v)     (v = BIT_FIELD(VME_Output_S1(p, uint4, 6, 5), 16, 16))
#define VME_GET_UNIOutput_SumRef0InterDist(p, v)          (v = BIT_FIELD(VME_Output_S1(p, uint4, 6, 6),  0, 27))

#define VME_GET_UNIOutput(UNIOUT, FIELDNAME) VME_GET_UNIOutput_##FIELDNAME(UNIOUT, FIELDNAME)
#define VME_GET_IMEOutput(UNIOUT, FIELDNAME) VME_GET_IMEOutput_##FIELDNAME(UNIOUT, FIELDNAME)

template<size_t N>
_GENX_ inline
void DebugUniOutput(matrix_ref<uchar, N, 32> uniOut)
{
    assert(N==7||N==9);
    matrix_ref<uchar, 7, 32> uniOut7 = SELECT_N_ROWS_TEMPL(uniOut, 0, 7);

    // W0.0
    ushort InterMbMode, IntraMbMode, FieldMbPolarityFlag, InterMbType, FieldMbFlag, Transform8x8Flag, IntraMbType, ExtendedForm, MvQuantity;
    // W0.1
    ushort Ref0BorderReached, Ref1BorderReached, SearchPathLength, SumInterDistL1Upper;
    // W0.2-3
    ushort BestInterDistortion, SkipRawDistortion, BestIntraDistortion , BestChromaIntraDistortion;
    // W0.4-5
    ushort LumaIntraPredMode0, LumaIntraPredMode1, LumaIntraPredMode2, LumaIntraPredMode3;
    // W0.6
    ushort MbIntraStruct, SubMbShape, SubMbPredMode, EarlyImeStop, MaxMVOccurred, AlternateSearchPathLength;
    // W0.7
    ushort TotalVMEComputeClocks, TotalVMEStalledClocks;
    // W1.0-4.7
    matrix<short,4,16> Mvs;
    // W5.0-5.7
    matrix<ushort,1,16> InterDistortion;
    // W6.0
    uchar FwdBlk0RefID, BwdBlk0RefID, FwdBlk1RefID, BwdBlk1RefID, FwdBlk2RefID, BwdBlk2RefID, FwdBlk3RefID, BwdBlk3RefID;
    // W6.1
    ushort Blk0LumaNZC, Blk1LumaNZC, Blk2LumaNZC, Blk3LumaNZC;
    // W6.2-3
    ushort Blk0LumaCoeffSum, Blk1LumaCoeffSum, Blk2LumaCoeffSum, Blk3LumaCoeffSum;
    // W6.4
    ushort Blk0ChromaCbNZC, Blk0ChromaCrNZC, SumInterDistL1Lower;
    // W6.5
    ushort Blbk0ChromaCbCoeffSum, Blbk0ChromaCrCoeffSum;
    // W6.6
    uint SumRef0InterDist;
    // W6.7
    ushort MaxRef0InterDist, MaxRef1InterDist;
    // W7.0-7.7
    ushort Rec0_16x16_Distortion, Rec0_16x8_0Distortion, Rec0_16x8_1Distortion, Rec0_8x16_0Distortion, Rec0_8x16_1Distortion;
    ushort Rec0_8x8_0Distortion, Rec0_8x8_1Distortion, Rec0_8x8_2Distortion, Rec0_8x8_3Distortion;
    ushort Rec0_16x16_RefID, Rec0_16x8_0RefID, Rec0_16x8_1RefID, Rec0_8x16_0RefID, Rec0_8x16_1RefID;
    ushort Rec0_8x8_0RefID, Rec0_8x8_1RefID, Rec0_8x8_2RefID, Rec0_8x8_3RefID;
    short Rec0_16x16_X, Rec0_16x16_Y;
    // W8.0-8.7
    short Rec0_16x8_0X, Rec0_16x8_0Y, Rec0_16x8_1X, Rec0_16x8_1Y, Rec0_8x16_0X, Rec0_8x16_0Y, Rec0_8x16_1X, Rec0_8x16_1Y;
    short Rec0_8x8_0X, Rec0_8x8_0Y, Rec0_8x8_1X, Rec0_8x8_1Y, Rec0_8x8_2X, Rec0_8x8_2Y, Rec0_8x8_3X, Rec0_8x8_3Y;

    // W0.0
    VME_GET_UNIOutput(uniOut7, InterMbMode);
    VME_GET_UNIOutput(uniOut7, IntraMbMode);
    VME_GET_UNIOutput(uniOut7, FieldMbPolarityFlag);
    VME_GET_UNIOutput(uniOut7, InterMbType);
    VME_GET_UNIOutput(uniOut7, FieldMbFlag);
    VME_GET_UNIOutput(uniOut7, Transform8x8Flag);
    VME_GET_UNIOutput(uniOut7, IntraMbType);
    VME_GET_UNIOutput(uniOut7, ExtendedForm);
    VME_GET_UNIOutput(uniOut7, MvQuantity);
    // W0.1
    VME_GET_UNIOutput(uniOut7, Ref0BorderReached);
    VME_GET_UNIOutput(uniOut7, Ref1BorderReached);
    VME_GET_UNIOutput(uniOut7, SearchPathLength);
    VME_GET_UNIOutput(uniOut7, SumInterDistL1Upper);
    // W0.2-3
    VME_GET_UNIOutput(uniOut7, BestInterDistortion);
    VME_GET_UNIOutput(uniOut7, SkipRawDistortion);
    VME_GET_UNIOutput(uniOut7, BestIntraDistortion);
    VME_GET_UNIOutput(uniOut7, BestChromaIntraDistortion);
    // W0.4-5
    VME_GET_UNIOutput(uniOut7, LumaIntraPredMode0);
    VME_GET_UNIOutput(uniOut7, LumaIntraPredMode1);
    VME_GET_UNIOutput(uniOut7, LumaIntraPredMode2);
    VME_GET_UNIOutput(uniOut7, LumaIntraPredMode3);
    // W0.6
    VME_GET_UNIOutput(uniOut7, MbIntraStruct);
    VME_GET_UNIOutput(uniOut7, SubMbShape);
    VME_GET_UNIOutput(uniOut7, SubMbPredMode);
    VME_GET_UNIOutput(uniOut7, EarlyImeStop);
    VME_GET_UNIOutput(uniOut7, MaxMVOccurred);
    VME_GET_UNIOutput(uniOut7, AlternateSearchPathLength);
    // W0.7
    VME_GET_UNIOutput(uniOut7, TotalVMEComputeClocks);
    VME_GET_UNIOutput(uniOut7, TotalVMEStalledClocks);
    // W1.0-4.7
    VME_GET_UNIOutput(uniOut7, Mvs);
    // W5.0-5.7
    VME_GET_UNIOutput(uniOut7, InterDistortion);
    // W6.0
    VME_GET_UNIOutput(uniOut7, FwdBlk0RefID);
    VME_GET_UNIOutput(uniOut7, BwdBlk0RefID);
    VME_GET_UNIOutput(uniOut7, FwdBlk1RefID);
    VME_GET_UNIOutput(uniOut7, BwdBlk1RefID);
    VME_GET_UNIOutput(uniOut7, FwdBlk2RefID);
    VME_GET_UNIOutput(uniOut7, BwdBlk2RefID);
    VME_GET_UNIOutput(uniOut7, FwdBlk3RefID);
    VME_GET_UNIOutput(uniOut7, BwdBlk3RefID);
    // W6.1
    VME_GET_UNIOutput(uniOut7, Blk0LumaNZC);
    VME_GET_UNIOutput(uniOut7, Blk1LumaNZC);
    VME_GET_UNIOutput(uniOut7, Blk2LumaNZC);
    VME_GET_UNIOutput(uniOut7, Blk3LumaNZC);
    // W6.2-3
    VME_GET_UNIOutput(uniOut7, Blk0LumaCoeffSum);
    VME_GET_UNIOutput(uniOut7, Blk1LumaCoeffSum);
    VME_GET_UNIOutput(uniOut7, Blk2LumaCoeffSum);
    VME_GET_UNIOutput(uniOut7, Blk3LumaCoeffSum);
    // W6.4
    VME_GET_UNIOutput(uniOut7, Blk0ChromaCbNZC);
    VME_GET_UNIOutput(uniOut7, Blk0ChromaCrNZC);
    VME_GET_UNIOutput(uniOut7, SumInterDistL1Lower);
    // W6.5
    VME_GET_UNIOutput(uniOut7, Blbk0ChromaCbCoeffSum);
    VME_GET_UNIOutput(uniOut7, Blbk0ChromaCrCoeffSum);
    // W6.6
    VME_GET_UNIOutput(uniOut7, SumRef0InterDist);
    // W6.7
    VME_GET_UNIOutput(uniOut7, MaxRef0InterDist);
    VME_GET_UNIOutput(uniOut7, MaxRef1InterDist);
    if (N == 9)
    {
        // W7.0-7.7
        VME_GET_IMEOutput(uniOut, Rec0_16x8_0Distortion);
        VME_GET_IMEOutput(uniOut, Rec0_16x8_1Distortion);
        VME_GET_IMEOutput(uniOut, Rec0_8x16_0Distortion);
        VME_GET_IMEOutput(uniOut, Rec0_8x16_1Distortion);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_0Distortion);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_1Distortion);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_2Distortion);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_3Distortion);
        VME_GET_IMEOutput(uniOut, Rec0_16x16_Distortion);
        VME_GET_IMEOutput(uniOut, Rec0_16x16_RefID);
        VME_GET_IMEOutput(uniOut, Rec0_16x16_X);
        VME_GET_IMEOutput(uniOut, Rec0_16x16_Y);
        VME_GET_IMEOutput(uniOut, Rec0_16x8_0RefID);
        VME_GET_IMEOutput(uniOut, Rec0_16x8_1RefID);
        VME_GET_IMEOutput(uniOut, Rec0_8x16_0RefID);
        VME_GET_IMEOutput(uniOut, Rec0_8x16_1RefID);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_0RefID);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_1RefID);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_2RefID);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_3RefID);
        // W8.0-8.7
        VME_GET_IMEOutput(uniOut, Rec0_16x8_0X);
        VME_GET_IMEOutput(uniOut, Rec0_16x8_0Y);
        VME_GET_IMEOutput(uniOut, Rec0_16x8_1X);
        VME_GET_IMEOutput(uniOut, Rec0_16x8_1Y);
        VME_GET_IMEOutput(uniOut, Rec0_8x16_0X);
        VME_GET_IMEOutput(uniOut, Rec0_8x16_0Y);
        VME_GET_IMEOutput(uniOut, Rec0_8x16_1X);
        VME_GET_IMEOutput(uniOut, Rec0_8x16_1Y);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_0X);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_0Y);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_1X);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_1Y);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_2X);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_2Y);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_3X);
        VME_GET_IMEOutput(uniOut, Rec0_8x8_3Y);
    }

    // W0.0
    InterMbMode, IntraMbMode, FieldMbPolarityFlag, InterMbType, FieldMbFlag, Transform8x8Flag, IntraMbType, ExtendedForm, MvQuantity;
    // W0.1
    Ref0BorderReached, Ref1BorderReached, SearchPathLength, SumInterDistL1Upper;
    // W0.2-3
    BestInterDistortion, SkipRawDistortion, BestIntraDistortion , BestChromaIntraDistortion;
    // W0.4-5
    LumaIntraPredMode0, LumaIntraPredMode1, LumaIntraPredMode2, LumaIntraPredMode3;
    // W0.6
    MbIntraStruct, SubMbShape, SubMbPredMode, EarlyImeStop, MaxMVOccurred, AlternateSearchPathLength;
    // W0.7
    TotalVMEComputeClocks, TotalVMEStalledClocks;
    // W1.0-4.7
    Mvs;
    // W5.0-5.7
    InterDistortion;
    // W6.0
    FwdBlk0RefID, BwdBlk0RefID, FwdBlk1RefID, BwdBlk1RefID, FwdBlk2RefID, BwdBlk2RefID, FwdBlk3RefID, BwdBlk3RefID;
    // W6.1
    Blk0LumaNZC, Blk1LumaNZC, Blk2LumaNZC, Blk3LumaNZC;
    // W6.2-3
    Blk0LumaCoeffSum, Blk1LumaCoeffSum, Blk2LumaCoeffSum, Blk3LumaCoeffSum;
    // W6.4
    Blk0ChromaCbNZC, Blk0ChromaCrNZC, SumInterDistL1Lower;
    // W6.5
    Blbk0ChromaCbCoeffSum, Blbk0ChromaCrCoeffSum;
    // W6.6
    SumRef0InterDist;
    // W6.7
    MaxRef0InterDist, MaxRef1InterDist;
    // W7.0-7.7
    Rec0_16x16_Distortion, Rec0_16x8_0Distortion, Rec0_16x8_1Distortion, Rec0_8x16_0Distortion, Rec0_8x16_1Distortion;
    Rec0_8x8_0Distortion, Rec0_8x8_1Distortion, Rec0_8x8_2Distortion, Rec0_8x8_3Distortion;
    Rec0_16x16_RefID, Rec0_16x8_0RefID, Rec0_16x8_1RefID, Rec0_8x16_0RefID, Rec0_8x16_1RefID;
    Rec0_8x8_0RefID, Rec0_8x8_1RefID, Rec0_8x8_2RefID, Rec0_8x8_3RefID;
    Rec0_16x16_X, Rec0_16x16_Y;
    // W8.0-8.7
    Rec0_16x8_0X, Rec0_16x8_0Y, Rec0_16x8_1X, Rec0_16x8_1Y, Rec0_8x16_0X, Rec0_8x16_0Y, Rec0_8x16_1X, Rec0_8x16_1Y;
    Rec0_8x8_0X, Rec0_8x8_0Y, Rec0_8x8_1X, Rec0_8x8_1Y, Rec0_8x8_2X, Rec0_8x8_2Y, Rec0_8x8_3X, Rec0_8x8_3Y;
    //printf("16x16=(%+2d %+2d) 8x8_0=(%+2d %+2d) (%+2d %+2d) (%+2d %+2d) (%+2d %+2d)\n",
    //    Rec0_16x16_X, Rec0_16x16_Y, Rec0_8x8_0X, Rec0_8x8_0Y, Rec0_8x8_1X, Rec0_8x8_1Y, Rec0_8x8_2X, Rec0_8x8_2Y, Rec0_8x8_3X, Rec0_8x8_3Y);
}

enum { DIR_L, DIR_UL, DIR_U, DIR_UR, DIR_R, DIR_DR, DIR_D, DIR_DL };

template<class T, uint R, uint C>
_GENX_ inline
void read_plane_wide(SurfaceIndex SURF, CmSurfacePlaneIndex idx, int x, int y, matrix_ref<T, R, C> m)
{
    read_plane(SURF, idx, x, y, m);
}

template<> _GENX_ inline
void read_plane_wide<uchar, 4, 36>(SurfaceIndex SURF, CmSurfacePlaneIndex idx, int x, int y, matrix_ref<uchar, 4, 36> m)
{
    read_plane(SURF, idx, x,      y, SELECT_N_COLS(m, 0, 32));
    read_plane(SURF, idx, x + 32, y, SELECT_N_COLS(m, 32, 4));
}

template<> _GENX_ inline
void read_plane_wide<uchar, 1, 36>(SurfaceIndex SURF, CmSurfacePlaneIndex idx, int x, int y, matrix_ref<uchar, 1, 36> m)
{
    read_plane(SURF, idx, x,      y, SELECT_N_COLS(m, 0, 32));
    read_plane(SURF, idx, x + 32, y, SELECT_N_COLS(m, 32, 4));
}

template<int W, int H>
_GENX_ inline
void Interpolate(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, int xsrc, int ysrc, int xref, int yref, vector_ref<uint, 8> sad)
{
    matrix<uchar, 2, W>   src;
    matrix<uchar, 4, (W+4+31)&~31> fp;
    vector<short, W+4> tmpW;
    vector_ref<short, W+1> tmpN = SLICE1(tmpW, 0, W+1);
    vector<short, W+1> tmpD;
    vector<uchar, W+1> hpH;
    vector<uchar, W+4> hpV;
    vector<uchar, W+1> hpD;
    vector<short, W> diff;
    vector<char, 2> srcRowIdx;
    vector<char, 4> fpRowIdx;
    matrix<ushort, 8, W> partialSad = 0;
    srcRowIdx[0] = 0;
    srcRowIdx[1] = 1;
    fpRowIdx[0] = 1;
    fpRowIdx[1] = 2;
    fpRowIdx[2] = 3;
    fpRowIdx[3] = 0;
    sad = 0;

    read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, xsrc+0, ysrc+0, src.row(0));
    read_plane_wide(SURF_REF, GENX_SURFACE_Y_PLANE, xref-2, yref-2, SELECT_N_COLS_TEMPL(fp, 0, W+4));

    tmpN  = SLICE1(fp.row(2), 1, W+1) + SLICE1(fp.row(2), 2, W+1);
    tmpN *= 5;
    tmpN -= SLICE1(fp.row(2), 0, W+1);
    tmpN -= SLICE1(fp.row(2), 3, W+1);
    tmpN += 4;
    hpH = cm_shr<uchar>(tmpN, 3, SAT);

    tmpW  = SLICE1(fp.row(1), 0, W+4) + SLICE1(fp.row(2), 0, W+4);
    tmpW *= 5;
    tmpW -= SLICE1(fp.row(0), 0, W+4);
    tmpW -= SLICE1(fp.row(3), 0, W+4);

    tmpD  = SLICE1(tmpW, 1, W+1) + SLICE1(tmpW, 2, W+1);
    tmpD *= 5;
    tmpD -= SLICE1(tmpW, 0, W+1);
    tmpD -= SLICE1(tmpW, 3, W+1);
    tmpD += 32;
    hpD = cm_shr<uchar>(tmpD, 6, SAT);

    tmpW += 4;
    hpV = cm_shr<uchar>(tmpW, 3, SAT);

    partialSad.row(DIR_L ) += cm_abs<ushort>(SLICE1(hpH.row(0), 0, W) - src.row(0));
    partialSad.row(DIR_UL) += cm_abs<ushort>(SLICE1(hpD.row(0), 0, W) - src.row(0));
    partialSad.row(DIR_U ) += cm_abs<ushort>(SLICE1(hpV.row(0), 2, W) - src.row(0));
    partialSad.row(DIR_UR) += cm_abs<ushort>(SLICE1(hpD.row(0), 1, W) - src.row(0));
    partialSad.row(DIR_R ) += cm_abs<ushort>(SLICE1(hpH.row(0), 1, W) - src.row(0));

    for (int i = 1; i < H; i++)
    {
        vector_ref<uchar, W>   srcRow0 = src.row(srcRowIdx(0));
        vector_ref<uchar, W>   srcRow1 = src.row(srcRowIdx(1));
        vector_ref<uchar, W+4> fpRow0  = SLICE1(fp.row(fpRowIdx(0)), 0, W + 4);
        vector_ref<uchar, W+4> fpRow1  = SLICE1(fp.row(fpRowIdx(1)), 0, W + 4);
        vector_ref<uchar, W+4> fpRow2  = SLICE1(fp.row(fpRowIdx(2)), 0, W + 4);
        vector_ref<uchar, W+4> fpRow3  = SLICE1(fp.row(fpRowIdx(3)), 0, W + 4);

        read_plane(
            SURF_SRC, GENX_SURFACE_Y_PLANE, xsrc+0, ysrc+i, srcRow1);
        read_plane_wide(
            SURF_REF, GENX_SURFACE_Y_PLANE, xref-2, yref+i+1, SLICE1(fpRow3, 0, W+4));

        tmpN  = SLICE1(fpRow2, 1, W+1) + SLICE1(fpRow2, 2, W+1);
        tmpN *= 5;
        tmpN -= SLICE1(fpRow2, 0, W+1);
        tmpN -= SLICE1(fpRow2, 3, W+1);
        tmpN += 4;
        hpH = cm_shr<uchar>(tmpN, 3, SAT);

        tmpW  = fpRow1 + fpRow2;
        tmpW *= 5;
        tmpW -= fpRow0;
        tmpW -= fpRow3;

        tmpD  = SLICE1(tmpW, 1, W+1) + SLICE1(tmpW, 2, W+1);
        tmpD *= 5;
        tmpD -= SLICE1(tmpW, 0, W+1);
        tmpD -= SLICE1(tmpW, 3, W+1);
        tmpD += 32;
        hpD = cm_shr<uchar>(tmpD, 6, SAT);

        tmpW += 4;
        hpV = cm_shr<uchar>(tmpW, 3, SAT);

        partialSad.row(DIR_L)  += cm_abs<ushort>(SLICE1(hpH.row(0), 0, W) - srcRow1);
        partialSad.row(DIR_UL) += cm_abs<ushort>(SLICE1(hpD.row(0), 0, W) - srcRow1);
        partialSad.row(DIR_U)  += cm_abs<ushort>(SLICE1(hpV.row(0), 2, W) - srcRow1);
        partialSad.row(DIR_UR) += cm_abs<ushort>(SLICE1(hpD.row(0), 1, W) - srcRow1);
        partialSad.row(DIR_R)  += cm_abs<ushort>(SLICE1(hpH.row(0), 1, W) - srcRow1);
        partialSad.row(DIR_DR) += cm_abs<ushort>(SLICE1(hpD.row(0), 1, W) - srcRow0);
        partialSad.row(DIR_D)  += cm_abs<ushort>(SLICE1(hpV.row(0), 2, W) - srcRow0);
        partialSad.row(DIR_DL) += cm_abs<ushort>(SLICE1(hpD.row(0), 0, W) - srcRow0);

        srcRowIdx = 1 - srcRowIdx;
        fpRowIdx += 1;
        fpRowIdx &= 3;
    }

    vector_ref<uchar, W>   srcRow0 = src.row(srcRowIdx(0));
    vector_ref<uchar, W+4> fpRow0  = SLICE1(fp.row(fpRowIdx(0)), 0, W + 4);
    vector_ref<uchar, W+4> fpRow1  = SLICE1(fp.row(fpRowIdx(1)), 0, W + 4);
    vector_ref<uchar, W+4> fpRow2  = SLICE1(fp.row(fpRowIdx(2)), 0, W + 4);
    vector_ref<uchar, W+4> fpRow3  = SLICE1(fp.row(fpRowIdx(3)), 0, W + 4);

    read_plane_wide(
        SURF_REF, GENX_SURFACE_Y_PLANE, xref - 2, yref + H + 1, SLICE1(fpRow3, 0, W + 4));

    tmpW  = fpRow1 + fpRow2;
    tmpW *= 5;
    tmpW -= fpRow0;
    tmpW -= fpRow3;

    tmpD  = SLICE1(tmpW, 1, W+1) + SLICE1(tmpW, 2, W+1);
    tmpD *= 5;
    tmpD -= SLICE1(tmpW, 0, W+1);
    tmpD -= SLICE1(tmpW, 3, W+1);
    tmpD += 32;
    hpD = cm_shr<uchar>(tmpD, 6, SAT);

    tmpW += 4;
    hpV = cm_shr<uchar>(tmpW, 3, SAT);

    partialSad.row(DIR_DR) += cm_abs<ushort>(SLICE1(hpD.row(0), 1, W) - srcRow0);
    partialSad.row(DIR_D)  += cm_abs<ushort>(SLICE1(hpV.row(0), 2, W) - srcRow0);
    partialSad.row(DIR_DL) += cm_abs<ushort>(SLICE1(hpD.row(0), 0, W) - srcRow0);

    sad[0] += cm_sum<uint>(partialSad.row(0));
    sad[1] += cm_sum<uint>(partialSad.row(1));
    sad[2] += cm_sum<uint>(partialSad.row(2));
    sad[3] += cm_sum<uint>(partialSad.row(3));
    sad[4] += cm_sum<uint>(partialSad.row(4));
    sad[5] += cm_sum<uint>(partialSad.row(5));
    sad[6] += cm_sum<uint>(partialSad.row(6));
    sad[7] += cm_sum<uint>(partialSad.row(7));
}


template<int W, int H>
_GENX_ inline
void InterpolateSmallBlocks(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, int xsrc, int ysrc, int xref, int yref, vector_ref<uint, 8> sad)
{
    matrix<uchar, H,   W> src;
    matrix<uchar, H+4, W> fp0, fp1, fp2, fp3, fp4;
    matrix<short, H+4, W> tmpH;
    matrix<short, H,   W> tmpH2;
    matrix<uchar, H,   W> hpH;
    matrix<short, H+1, W> tmpD;
    matrix<uchar, H+1, W> hpD;
    matrix<short, H+1, W> tmpV;
    matrix<uchar, H+1, W> hpV;

    read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, xsrc+0, ysrc+0, src);
    //matrix<uchar, H+4, W+4> readBuf;
    //read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref-2, yref-2,     SELECT_N_ROWS_TEMPL(readBuf, 0,   H/2));
    //read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref-2, yref-2+H/2, SELECT_N_ROWS_TEMPL(readBuf, H/2, H/2));
    //fp0 = SELECT_N_COLS_TEMPL(readBuf, 0, W);
    //fp1 = SELECT_N_COLS_TEMPL(readBuf, 1, W);
    //fp2 = SELECT_N_COLS_TEMPL(readBuf, 2, W);
    //fp3 = SELECT_N_COLS_TEMPL(readBuf, 3, W);
    //fp4 = SELECT_N_COLS_TEMPL(readBuf, 4, W);
    read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref-2, yref-2, fp0);
    read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref-1, yref-2, fp1);
    read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref+0, yref-2, fp2);
    read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref+1, yref-2, fp3);
    read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref+2, yref-2, fp4);

    tmpH  = fp1 + fp2;
    tmpH *= 5;
    tmpH -= fp0;
    tmpH -= fp3;
    tmpH2 = SELECT_N_ROWS_TEMPL(tmpH, 2, H) + 4;
    hpH = cm_shr<uchar>(tmpH2, 3, SAT);
    sad[DIR_L] += cm_sum<ushort>(cm_sad2<short>(hpH, src).select<W*H/2,2>(0));

    tmpD  = SELECT_N_ROWS_TEMPL(tmpH, 1, H+1) + SELECT_N_ROWS_TEMPL(tmpH, 2, H+1);
    tmpD *= 5;
    tmpD -= SELECT_N_ROWS_TEMPL(tmpH, 0, H+1);
    tmpD -= SELECT_N_ROWS_TEMPL(tmpH, 3, H+1);
    tmpD += 32;
    hpD = cm_shr<uchar>(tmpD, 6, SAT);
    sad(DIR_UL) += cm_sum<ushort>(cm_sad2<ushort>(hpD.select<H,1,W,1>(0,0), src).select<W*H/2,2>(0));
    sad(DIR_DL) += cm_sum<ushort>(cm_sad2<ushort>(hpD.select<H,1,W,1>(1,0), src).select<W*H/2,2>(0));

    tmpV  = SELECT_N_ROWS_TEMPL(fp2, 1, H+1) + SELECT_N_ROWS_TEMPL(fp2, 2, H+1);
    tmpV *= 5;
    tmpV -= SELECT_N_ROWS_TEMPL(fp2, 0, H+1);
    tmpV -= SELECT_N_ROWS_TEMPL(fp2, 3, H+1);
    tmpV += 4;
    hpV = cm_shr<uchar>(tmpV, 3, SAT);
    sad(DIR_U) += cm_sum<ushort>(cm_sad2<ushort>(hpV.select<H,1,W,1>(0,0), src).select<W*H/2,2>(0));
    sad(DIR_D) += cm_sum<ushort>(cm_sad2<ushort>(hpV.select<H,1,W,1>(1,0), src).select<W*H/2,2>(0));

    tmpH  = fp2 + fp3;
    tmpH *= 5;
    tmpH -= fp1;
    tmpH -= fp4;
    tmpH2 = SELECT_N_ROWS_TEMPL(tmpH, 2, H) + 4;
    hpH = cm_shr<uchar>(tmpH2, 3, SAT);
    sad(DIR_R) += cm_sum<ushort>(cm_sad2<ushort>(hpH, src).select<W*H/2,2>(0));

    tmpD  = SELECT_N_ROWS_TEMPL(tmpH, 1, H+1) + SELECT_N_ROWS_TEMPL(tmpH, 2, H+1);
    tmpD *= 5;
    tmpD -= SELECT_N_ROWS_TEMPL(tmpH, 0, H+1);
    tmpD -= SELECT_N_ROWS_TEMPL(tmpH, 3, H+1);
    tmpD += 32;
    hpD = cm_shr<uchar>(tmpD, 6, SAT);
    sad(DIR_UR) += cm_sum<ushort>(cm_sad2<ushort>(hpD.select<H,1,W,1>(0,0), src).select<W*H/2,2>(0));
    sad(DIR_DR) += cm_sum<ushort>(cm_sad2<ushort>(hpD.select<H,1,W,1>(1,0), src).select<W*H/2,2>(0));
}

#define CALC_SAD(sad, ref, src) sad = cm_sad2<ushort>(ref, src).template select<W/2,2>(0)
#define CALC_SAD_ACC(sad, ref, src) sad += cm_sad2<ushort>(ref, src).template select<W/2,2>(0)
#define CALC_SAD_2(sad1, sad2, ref, src1, src2) \
    sad1 = cm_sad2<ushort>(ref, src1).template select<W/2,2>(0); \
    sad2 = cm_sad2<ushort>(ref, src2).template select<W/2,2>(0)
#define CALC_SAD_ACC_2(sad1, sad2, ref, src1, src2) \
    sad1 += cm_sad2<ushort>(ref, src1).template select<W/2,2>(0); \
    sad2 += cm_sad2<ushort>(ref, src2).template select<W/2,2>(0)
#define CALC_SAD_QPEL(sad, hpel1, hpel2, src) \
    tmp = cm_avg<short>(hpel1, hpel2); \
    sad = cm_sad2<ushort>(tmp.template format<uchar>().template select<W,2>(0), src).template select<W/2,2>(0)
#define CALC_SAD_QPEL_ACC(sad, hpel1, hpel2, src) \
    tmp = cm_avg<short>(hpel1, hpel2); \
    sad += cm_sad2<ushort>(tmp.template format<uchar>().template select<W, 2>(0), src).template select<W/2,2>(0)
#define CALC_SAD_QPEL_2(sad1, sad2, hpel1, hpel2, src1, src2) \
    tmp = cm_avg<short>(hpel1, hpel2); \
    sad1 = cm_sad2<ushort>(tmp.template format<uchar>().template select<W, 2>(0), src1).template select<W/2,2>(0); \
    sad2 = cm_sad2<ushort>(tmp.template format<uchar>().template select<W, 2>(0), src2).template select<W/2,2>(0)
#define CALC_SAD_QPEL_ACC_2(sad1, sad2, hpel1, hpel2, src1, src2) \
    tmp = cm_avg<short>(hpel1, hpel2); \
    sad1 += cm_sad2<ushort>(tmp.template format<uchar>().template select<W, 2>(0), src1).template select<W/2,2>(0); \
    sad2 += cm_sad2<ushort>(tmp.template format<uchar>().template select<W, 2>(0), src2).template select<W/2,2>(0)
#define INTERPOLATE(dst, src0, src1, src2, src3) dst = src1 + src2; dst *= 5; dst -= src0; dst -= src3
#define SHIFT_SATUR(dst, tmp, src, shift) tmp = src + (1 << (shift - 1)); dst = cm_shr<uchar>(tmp, shift, SAT);

template<int W, int H>
_GENX_ inline
void InterpolateHugeBlocks(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, int xsrc, int ysrc, int xref, int yref, vector_ref<uint, 8> sad)
{
    matrix<uchar, 1, W> src;
    matrix<uchar, 4, W> fpel0, fpel1, fpel2, fpel3, fpel4;
    matrix<short, 4, W> tmpL;
    matrix<short, 4, W> tmpR;
    matrix<short, 1, W> tmp;
    matrix<uchar, 1, W> hpelHorzL;
    matrix<uchar, 1, W> hpelHorzR;
    matrix<uchar, 1, W> hpelVert;
    matrix<uchar, 1, W> hpelDiagL;
    matrix<uchar, 1, W> hpelDiagR;

    read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref - 2, yref - 2, fpel0);
    read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref - 2, fpel1);
    read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref - 2, fpel2);
    read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref - 2, fpel3);
    read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref - 2, fpel4);

    tmpL  = fpel1 + fpel2;
    tmpL *= 5;
    tmpL -= fpel0;
    tmpL -= fpel3;
    tmp   = tmpL.row(2) + 4;
    hpelHorzL = cm_shr<uchar>(tmp, 3, SAT);

    tmpR  = fpel2 + fpel3;
    tmpR *= 5;
    tmpR -= fpel1;
    tmpR -= fpel4;
    tmp   = tmpR.row(2) + 4;
    hpelHorzR = cm_shr<uchar>(tmp, 3, SAT);

    tmp  = fpel2.row(1) + fpel2.row(2);
    tmp *= 5;
    tmp -= fpel2.row(0);
    tmp -= fpel2.row(3);
    tmp += 4;
    hpelVert = cm_shr<uchar>(tmp, 3, SAT);

    tmp  = tmpL.row(1) + tmpL.row(2);
    tmp *= 5;
    tmp -= tmpL.row(0);
    tmp -= tmpL.row(3);
    tmp += 32;
    hpelDiagL = cm_shr<uchar>(tmp, 6, SAT);

    tmp  = tmpR.row(1) + tmpR.row(2);
    tmp *= 5;
    tmp -= tmpR.row(0);
    tmp -= tmpR.row(3);
    tmp += 32;
    hpelDiagR = cm_shr<uchar>(tmp, 6, SAT);

    matrix<ushort, 8, W/2> partSad = 0;

    for (int i = 0; i < H; i++)
    {
        vector_ref<uchar, W> fpel0_row3 = fpel0.row(0);
        vector_ref<uchar, W> fpel1_row3 = fpel1.row(0);
        vector_ref<uchar, W> fpel3_row3 = fpel3.row(0);
        vector_ref<uchar, W> fpel4_row3 = fpel4.row(0);

        vector_ref<uchar, W> fpel2_row0 = fpel2.row((i+1)&3);
        vector_ref<uchar, W> fpel2_row1 = fpel2.row((i+2)&3);
        vector_ref<uchar, W> fpel2_row2 = fpel2.row((i+3)&3);
        vector_ref<uchar, W> fpel2_row3 = fpel2.row((i+4)&3);
        vector_ref<short, W> tmpL_row0  = tmpL.row((i+1)&3);
        vector_ref<short, W> tmpL_row1  = tmpL.row((i+2)&3);
        vector_ref<short, W> tmpL_row2  = tmpL.row((i+3)&3);
        vector_ref<short, W> tmpL_row3  = tmpL.row((i+4)&3);
        vector_ref<short, W> tmpR_row0  = tmpR.row((i+1)&3);
        vector_ref<short, W> tmpR_row1  = tmpR.row((i+2)&3);
        vector_ref<short, W> tmpR_row2  = tmpR.row((i+3)&3);
        vector_ref<short, W> tmpR_row3  = tmpR.row((i+4)&3);

        read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, xsrc, ysrc + i, src);

        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref - 2, yref - 2 + 4 + i, fpel0_row3);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref - 2 + 4 + i, fpel1_row3);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref - 2 + 4 + i, fpel2_row3);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref - 2 + 4 + i, fpel3_row3);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref - 2 + 4 + i, fpel4_row3);

        CALC_SAD(partSad.row(DIR_L ), hpelHorzL, src);
        CALC_SAD(partSad.row(DIR_R ), hpelHorzR, src);
        CALC_SAD(partSad.row(DIR_U ), hpelVert,  src);
        CALC_SAD(partSad.row(DIR_UL), hpelDiagL, src);
        CALC_SAD(partSad.row(DIR_UR), hpelDiagR, src);

        tmpL_row3  = fpel1_row3 + fpel2_row3;
        tmpL_row3 *= 5;
        tmpL_row3 -= fpel0_row3;
        tmpL_row3 -= fpel3_row3;
        tmp = tmpL_row2 + 4;
        hpelHorzL = cm_shr<uchar>(tmp, 3, SAT);

        tmpR_row3  = fpel2_row3 + fpel3_row3;
        tmpR_row3 *= 5;
        tmpR_row3 -= fpel1_row3;
        tmpR_row3 -= fpel4_row3;
        tmp = tmpR_row2 + 4;
        hpelHorzR = cm_shr<uchar>(tmp, 3, SAT);

        tmp  = fpel2_row1 + fpel2_row2;
        tmp *= 5;
        tmp -= fpel2_row0;
        tmp -= fpel2_row3;
        tmp += 4;
        hpelVert = cm_shr<uchar>(tmp,  3, SAT);

        tmp  = tmpL_row1 + tmpL_row2;
        tmp *= 5;
        tmp -= tmpL_row0;
        tmp -= tmpL_row3;
        tmp += 32;
        hpelDiagL = cm_shr<uchar>(tmp, 6, SAT);

        tmp  = tmpR_row1 + tmpR_row2;
        tmp *= 5;
        tmp -= tmpR_row0;
        tmp -= tmpR_row3;
        tmp += 32;
        hpelDiagR = cm_shr<uchar>(tmp, 6, SAT);

        CALC_SAD(partSad.row(DIR_D ), hpelVert,  src);
        CALC_SAD(partSad.row(DIR_DL), hpelDiagL, src);
        CALC_SAD(partSad.row(DIR_DR), hpelDiagR, src);
    }

    //for (int i = 0; i < 8; i++)
    //    sad(i) += cm_sum<ushort>(partSad.row(i));
    sad(0) += cm_sum<ushort>(partSad.row(0));
    sad(1) += cm_sum<ushort>(partSad.row(1));
    sad(2) += cm_sum<ushort>(partSad.row(2));
    sad(3) += cm_sum<ushort>(partSad.row(3));
    sad(4) += cm_sum<ushort>(partSad.row(4));
    sad(5) += cm_sum<ushort>(partSad.row(5));
    sad(6) += cm_sum<ushort>(partSad.row(6));
    sad(7) += cm_sum<ushort>(partSad.row(7));
}


template<int W>
_GENX_ inline
void SumSad(matrix_ref<ushort, 9, W/2> partSad, vector_ref<uint, 9> sad);

template<>
_GENX_ inline
void SumSad<32>(matrix_ref<ushort, 9, 16> partSad, vector_ref<uint, 9> sad)
{
    matrix<ushort, 9, 8> t1 = SELECT_N_COLS(partSad, 0, 8) + SELECT_N_COLS(partSad, 8, 8);
    matrix<ushort, 9, 4> t2 = SELECT_N_COLS(t1, 0, 4) + SELECT_N_COLS(t1, 4, 4);
    matrix<ushort, 9, 2> t3 = SELECT_N_COLS(t2, 0, 2) + SELECT_N_COLS(t2, 2, 2);
    sad = SELECT_N_COLS(t3, 0, 1) + SELECT_N_COLS(t3, 1, 1);
}

template<>
_GENX_ inline
void SumSad<16>(matrix_ref<ushort, 9, 8> partSad, vector_ref<uint, 9> sad)
{
    matrix<ushort, 9, 4> t1 = SELECT_N_COLS(partSad, 0, 4) + SELECT_N_COLS(partSad, 4, 4);
    matrix<ushort, 9, 2> t2 = SELECT_N_COLS(t1, 0, 2) + SELECT_N_COLS(t1, 2, 2);
    sad = SELECT_N_COLS(t2, 0, 1) + SELECT_N_COLS(t2, 1, 1);
}


template<int W, int H>
_GENX_ 
void InterpolateQpelFromIntPel(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, int xsrc, int ysrc, int xref, int yref, vector_ref<uint, 9> sad)
{
    matrix<uchar, 8, W> src;
    matrix<uchar, 8, W> fpel0, fpel1, fpel2, fpel3, fpel4;
    vector<short,    W> tmp;
    vector<uchar,    W> horzL, horzR;
    vector<uchar,    W> vert;

    matrix<ushort, 9, W/2> partSad = 0;

    read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref - 2, fpel2);

    INTERPOLATE(tmp, fpel2.row(0), fpel2.row(1), fpel2.row(2), fpel2.row(3));
    SHIFT_SATUR(vert, tmp, tmp, 3);

#define ONE_STEP(j)                                                                             \
    INTERPOLATE(tmp, fpel0.row(j), fpel1.row(j), fpel2.row(j+2&7), fpel3.row(j));               \
    SHIFT_SATUR(horzL, tmp, tmp, 3);                                                            \
    INTERPOLATE(tmp, fpel1.row(j), fpel2.row(j+2&7), fpel3.row(j), fpel4.row(j));               \
    SHIFT_SATUR(horzR, tmp, tmp, 3);                                                            \
    CALC_SAD_ACC(partSad.row(4), fpel2.row(j+2&7), src.row(j));                                 \
    CALC_SAD_QPEL_ACC(partSad.row(3), horzL, fpel2.row(j+2&7), src.row(j));                     \
    CALC_SAD_QPEL_ACC(partSad.row(0), vert,  horzL,            src.row(j));                     \
    CALC_SAD_QPEL_ACC(partSad.row(1), vert,  fpel2.row(j+2&7), src.row(j));                     \
    CALC_SAD_QPEL_ACC(partSad.row(2), vert,  horzR,            src.row(j));                     \
    CALC_SAD_QPEL_ACC(partSad.row(5), horzR, fpel2.row(j+2&7), src.row(j));                     \
    INTERPOLATE(tmp, fpel2.row(j+1&7), fpel2.row(j+2&7), fpel2.row(j+3&7), fpel2.row(j+4&7));   \
    SHIFT_SATUR(vert, tmp, tmp, 3);                                                             \
    CALC_SAD_QPEL_ACC(partSad.row(6), vert,  horzL,            src.row(j));                     \
    CALC_SAD_QPEL_ACC(partSad.row(7), vert,  fpel2.row(j+2&7), src.row(j));                     \
    CALC_SAD_QPEL_ACC(partSad.row(8), vert,  horzR,            src.row(j))

    for (int i = 0; i < H; i += 8)
    {
        read_plane(SURF_SRC, GENX_SURFACE_Y_PLANE, xsrc,     ysrc + i, src);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref - 2, yref + i, fpel0);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i, fpel1);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i, fpel3);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i, fpel4);
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 2, SELECT_N_ROWS_TEMPL(fpel2, 4, 4));

        ONE_STEP(0);
        ONE_STEP(1);
        ONE_STEP(2);
        ONE_STEP(3);
        
        read_plane(SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 6, SELECT_N_ROWS_TEMPL(fpel2, 0, 4));

        ONE_STEP(4);
        ONE_STEP(5);
        ONE_STEP(6);
        ONE_STEP(7);
    }

#undef ONE_STEP

    SumSad<W>(partSad, sad);
}


template<int W, int H>
_GENX_ 
void InterpolateQpelFromHalfPelVert(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, int xsrc, int ysrc, int xref, int yref, vector_ref<uint, 9> sad)
{
    matrix<uchar, 8, W> src;
    matrix<uchar, 4, W> fpel0, fpel1, fpel3, fpel4;
    matrix<uchar, 8, W> fpel2;
    matrix<short, 4, W> tmpL, tmpR;
    vector<short,    W> tmp;
    vector<uchar,    W> horzL, horzR;
    vector<uchar,    W> diagL, diagR;
    vector<uchar,    W> vert;
    matrix<ushort, 9, W/2> partSad = 0;

    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 2, yref - 1, NROWS(fpel0,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref - 1, NROWS(fpel1,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref - 1, NROWS(fpel2,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref - 1, NROWS(fpel3,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref - 1, NROWS(fpel4,1,3) );

    INTERPOLATE( NROWS(tmpL,0,3), NROWS(fpel0,1,3), NROWS(fpel1,1,3), NROWS(fpel2,1,3), NROWS(fpel3,1,3) );
    INTERPOLATE( NROWS(tmpR,0,3), NROWS(fpel1,1,3), NROWS(fpel2,1,3), NROWS(fpel3,1,3), NROWS(fpel4,1,3) );
    SHIFT_SATUR( horzL, tmp, tmpL.row(1), 3 );
    SHIFT_SATUR( horzR, tmp, tmpR.row(1), 3 );

#define ONE_STEP(j)                                                                                         \
    INTERPOLATE( tmpL.row(j+3&3), fpel0.row(j+4&3), fpel1.row(j+4&3), fpel2.row(j+4&7), fpel3.row(j+4&3) ); \
    INTERPOLATE( tmpR.row(j+3&3), fpel1.row(j+4&3), fpel2.row(j+4&7), fpel3.row(j+4&3), fpel4.row(j+4&3) ); \
    INTERPOLATE( tmp, tmpL.row(j+0&3), tmpL.row(j+1&3), tmpL.row(j+2&3), tmpL.row(j+3&3) );                 \
    SHIFT_SATUR( diagL, tmp, tmp, 6 );                                                                      \
    INTERPOLATE( tmp, tmpR.row(j+0&3), tmpR.row(j+1&3), tmpR.row(j+2&3), tmpR.row(j+3&3) );                 \
    SHIFT_SATUR( diagR, tmp, tmp, 6 );                                                                      \
    INTERPOLATE( tmp, fpel2.row(j+1&7), fpel2.row(j+2&7), fpel2.row(j+3&7), fpel2.row(j+4&7) );             \
    SHIFT_SATUR( vert, tmp, tmp, 3 );                                                                       \
    CALC_SAD_ACC( partSad.row(4), vert, src.row(j) );                                                       \
    CALC_SAD_QPEL_ACC( partSad.row(0), vert, horzL,            src.row(j) );                                \
    CALC_SAD_QPEL_ACC( partSad.row(1), vert, fpel2.row(j+2&7), src.row(j) );                                \
    CALC_SAD_QPEL_ACC( partSad.row(2), vert, horzR,            src.row(j) );                                \
    CALC_SAD_QPEL_ACC( partSad.row(3), vert, diagL,            src.row(j) );                                \
    CALC_SAD_QPEL_ACC( partSad.row(5), vert, diagR,            src.row(j) );                                \
    SHIFT_SATUR( horzL, tmp, tmpL.row(j+2&3), 3 );                                                          \
    SHIFT_SATUR( horzR, tmp, tmpR.row(j+2&3), 3 );                                                          \
    CALC_SAD_QPEL_ACC( partSad.row(6), vert,  horzL,            src.row(j) );                               \
    CALC_SAD_QPEL_ACC( partSad.row(7), vert,  fpel2.row(j+3&7), src.row(j) );                               \
    CALC_SAD_QPEL_ACC( partSad.row(8), vert,  horzR,            src.row(j) )

    for (int i = 0; i < H; i += 8)
    {
        read_plane( SURF_SRC, GENX_SURFACE_Y_PLANE, xsrc, ysrc + i, src);

        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 2, yref + i + 2, NROWS(fpel0,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i + 2, NROWS(fpel1,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 2, NROWS(fpel2,4,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i + 2, NROWS(fpel3,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i + 2, NROWS(fpel4,0,4) );

        ONE_STEP(0);
        ONE_STEP(1);
        ONE_STEP(2);
        ONE_STEP(3);

        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 2, yref + i + 6, NROWS(fpel0,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i + 6, NROWS(fpel1,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 6, NROWS(fpel2,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i + 6, NROWS(fpel3,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i + 6, NROWS(fpel4,0,4) );

        ONE_STEP(4);
        ONE_STEP(5);
        ONE_STEP(6);
        ONE_STEP(7);
    }

#undef ONE_STEP

    SumSad<W>(partSad, sad);
}


template<int W, int H>
_GENX_ 
void InterpolateQpelFromHalfPelHorz(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, int xsrc, int ysrc, int xref, int yref, vector_ref<uint, 9> sad)
{
    matrix<uchar, 8, W> src;
    matrix<uchar, 4, W> fpel1, fpel4;
    matrix<uchar, 8, W> fpel2, fpel3;
    matrix<short, 4, W> tmpR;
    vector<short,    W> tmp;
    vector<uchar,    W> horzR;
    vector<uchar,    W> diagR;
    vector<uchar,    W> vert;
    vector<uchar,    W> vertR;
    matrix<ushort, 9, W/2> partSad = 0;

    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref - 2, NROWS(fpel1,0,4) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref - 2, NROWS(fpel2,0,4) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref - 2, NROWS(fpel3,0,4) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref - 2, NROWS(fpel4,0,4) );

    INTERPOLATE( tmpR, fpel1, NROWS(fpel2,0,4), NROWS(fpel3,0,4), fpel4 );
    INTERPOLATE( tmp, tmpR.row(0), tmpR.row(1), tmpR.row(2), tmpR.row(3) );
    SHIFT_SATUR( diagR, tmp, tmp, 6 );
    INTERPOLATE( tmp, fpel2.row(0), fpel2.row(1), fpel2.row(2), fpel2.row(3) );
    SHIFT_SATUR( vert, tmp, tmp, 3 );
    INTERPOLATE( tmp, fpel3.row(0), fpel3.row(1), fpel3.row(2), fpel3.row(3) );
    SHIFT_SATUR( vertR, tmp, tmp, 3 );

#define ONE_STEP(j)                                                                                         \
    SHIFT_SATUR( horzR, tmp, tmpR.row(j+2&3), 3 );                                                          \
    CALC_SAD_ACC( partSad.row(4), horzR, src.row(j) );                                                      \
    CALC_SAD_QPEL_ACC( partSad.row(0), horzR, vert,             src.row(j) );                               \
    CALC_SAD_QPEL_ACC( partSad.row(1), horzR, diagR,            src.row(j) );                               \
    CALC_SAD_QPEL_ACC( partSad.row(2), horzR, vertR,            src.row(j) );                               \
    CALC_SAD_QPEL_ACC( partSad.row(3), horzR, fpel2.row(j+2&7), src.row(j) );                               \
    CALC_SAD_QPEL_ACC( partSad.row(5), horzR, fpel3.row(j+2&7), src.row(j) );                               \
    INTERPOLATE( tmpR.row(j+4&3), fpel1.row(j+4&3), fpel2.row(j+4&7), fpel3.row(j+4&7), fpel4.row(j+4&3) ); \
    INTERPOLATE( tmp, tmpR.row(j+1&3), tmpR.row(j+2&3), tmpR.row(j+3&3), tmpR.row(j+4&3) );                 \
    SHIFT_SATUR( diagR, tmp, tmp, 6 );                                                                      \
    INTERPOLATE( tmp, fpel2.row(j+1&7), fpel2.row(j+2&7), fpel2.row(j+3&7), fpel2.row(j+4&7) );             \
    SHIFT_SATUR( vert, tmp, tmp, 3 );                                                                       \
    INTERPOLATE( tmp, fpel3.row(j+1&7), fpel3.row(j+2&7), fpel3.row(j+3&7), fpel3.row(j+4&7) );             \
    SHIFT_SATUR( vertR, tmp, tmp, 3 );                                                                      \
    CALC_SAD_QPEL_ACC( partSad.row(6), horzR, vert,  src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(7), horzR, diagR, src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(8), horzR, vertR, src.row(j) )

    for (int i = 0; i < H; i += 8)
    {
        read_plane( SURF_SRC, GENX_SURFACE_Y_PLANE, xsrc, ysrc + i, src);

        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i + 2, NROWS(fpel1,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 2, NROWS(fpel2,4,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i + 2, NROWS(fpel3,4,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i + 2, NROWS(fpel4,0,4) );

        ONE_STEP(0);
        ONE_STEP(1);
        ONE_STEP(2);
        ONE_STEP(3);

        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i + 6, NROWS(fpel1,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 6, NROWS(fpel2,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i + 6, NROWS(fpel3,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i + 6, NROWS(fpel4,0,4) );

        ONE_STEP(4);
        ONE_STEP(5);
        ONE_STEP(6);
        ONE_STEP(7);
    }

#undef ONE_STEP

    SumSad<W>(partSad, sad);
}


template<int W, int H>
_GENX_ 
void InterpolateQpelFromHalfPelDiag(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, int xsrc, int ysrc, int xref, int yref, vector_ref<uint, 9> sad)
{
    matrix<uchar, 8, W> src;
    matrix<uchar, 4, W> fpel1, fpel4;
    matrix<uchar, 8, W> fpel2, fpel3;
    matrix<short, 4, W> tmpR;
    vector<short,    W> tmp;
    vector<uchar,    W> horzR;
    vector<uchar,    W> diagR;
    vector<uchar,    W> vert;
    vector<uchar,    W> vertR;
    matrix<ushort, 9, W/2> partSad = 0;

    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref - 1, NROWS(fpel1,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref - 1, NROWS(fpel2,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref - 1, NROWS(fpel3,1,3) );
    read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref - 1, NROWS(fpel4,1,3) );

    INTERPOLATE( NROWS(tmpR,0,3), NROWS(fpel1,1,3), NROWS(fpel2,1,3), NROWS(fpel3,1,3), NROWS(fpel4,1,3) );
    SHIFT_SATUR( horzR, tmp, tmpR.row(1), 3 );

#define ONE_STEP(j)                                                                                         \
    INTERPOLATE( tmp, fpel2.row(j+1&7), fpel2.row(j+2&7), fpel2.row(j+3&7), fpel2.row(j+4&7) );             \
    SHIFT_SATUR( vert, tmp, tmp, 3 );                                                                       \
    INTERPOLATE( tmp, fpel3.row(j+1&7), fpel3.row(j+2&7), fpel3.row(j+3&7), fpel3.row(j+4&7) );             \
    SHIFT_SATUR( vertR, tmp, tmp, 3 );                                                                      \
    INTERPOLATE( tmpR.row(j+3&3), fpel1.row(j+4&3), fpel2.row(j+4&7), fpel3.row(j+4&7), fpel4.row(j+4&3) ); \
    INTERPOLATE( tmp, tmpR.row(j+0&3), tmpR.row(j+1&3), tmpR.row(j+2&3), tmpR.row(j+3&3) );                 \
    SHIFT_SATUR( diagR, tmp, tmp, 6 );                                                                      \
    CALC_SAD_ACC( partSad.row(4), diagR, src.row(j) );                                                      \
    CALC_SAD_QPEL_ACC( partSad.row(0), horzR, vert,  src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(1), horzR, diagR, src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(2), horzR, vertR, src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(3), diagR, vert,  src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(5), diagR, vertR, src.row(j) );                                          \
    SHIFT_SATUR( horzR, tmp, tmpR.row(j+2&3), 3 );                                                          \
    CALC_SAD_QPEL_ACC( partSad.row(6), horzR, vert,  src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(7), horzR, diagR, src.row(j) );                                          \
    CALC_SAD_QPEL_ACC( partSad.row(8), horzR, vertR, src.row(j) )

    for (int i = 0; i < H; i += 8)
    {
        read_plane( SURF_SRC, GENX_SURFACE_Y_PLANE, xsrc, ysrc + i, src);

        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i + 2, NROWS(fpel1,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 2, NROWS(fpel2,4,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i + 2, NROWS(fpel3,4,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i + 2, NROWS(fpel4,0,4) );

        ONE_STEP(0);
        ONE_STEP(1);
        ONE_STEP(2);
        ONE_STEP(3);

        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref - 1, yref + i + 6, NROWS(fpel1,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 0, yref + i + 6, NROWS(fpel2,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 1, yref + i + 6, NROWS(fpel3,0,4) );
        read_plane( SURF_REF, GENX_SURFACE_Y_PLANE, xref + 2, yref + i + 6, NROWS(fpel4,0,4) );

        ONE_STEP(4);
        ONE_STEP(5);
        ONE_STEP(6);
        ONE_STEP(7);
    }

#undef ONE_STEP

    SumSad<W>(partSad, sad);
}



//template< int W, int H > _GENX_ inline
//void QpelRefine(SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, int2 x, int2 y, int2 hpelMvx, int2 hpelMvy, vector_ref<uint, 9> sad)
//{
//    int2 xref = x + (hpelMvx >> 2);
//    int2 yref = y + (hpelMvy >> 2);
//
//    hpelMvx &= 3;
//    hpelMvy &= 3;
//
//    if (hpelMvx == 0 && hpelMvy == 0)
//        InterpolateQpelFromIntPel< W, H >( SURF_SRC, SURF_REF, x, y, xref, yref, sad );
//    else if (hpelMvx == 2 && hpelMvy == 0)
//        InterpolateQpelFromHalfPelHorz< W, H >( SURF_SRC, SURF_REF, x, y, xref, yref, sad );
//    else if (hpelMvx == 0 && hpelMvy == 2)
//        InterpolateQpelFromHalfPelVert< W, H >( SURF_SRC, SURF_REF, x, y, xref, yref, sad );
//    else if (hpelMvx == 2 && hpelMvy == 2)
//        InterpolateQpelFromHalfPelDiag< W, H >( SURF_SRC, SURF_REF, x, y, xref, yref, sad );
//}
#define QpelRefine(W, H, SURF_SRC, SURF_REF, x, y, hpelMvx, hpelMvy, sad) { \
    int2 xref = x + (hpelMvx >> 2); \
    int2 yref = y + (hpelMvy >> 2); \
    hpelMvx &= 3; \
    hpelMvy &= 3; \
    if (hpelMvx == 0 && hpelMvy == 0) \
        InterpolateQpelFromIntPel< W, H >( SURF_SRC, SURF_REF, x, y, xref, yref, sad ); \
    else if (hpelMvx == 2 && hpelMvy == 0) \
        InterpolateQpelFromHalfPelHorz< W, H >( SURF_SRC, SURF_REF, x, y, xref, yref, sad ); \
    else if (hpelMvx == 0 && hpelMvy == 2) \
        InterpolateQpelFromHalfPelVert< W, H >( SURF_SRC, SURF_REF, x, y, xref, yref, sad ); \
    else if (hpelMvx == 2 && hpelMvy == 2) \
        InterpolateQpelFromHalfPelDiag< W, H >( SURF_SRC, SURF_REF, x, y, xref, yref, sad ); \
}

extern "C" _GENX_MAIN_ 
void Ime(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_MV_OUT)
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
    matrix<uchar, 4, 32> uniIn = 0;
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
//    VME_SET_DWORD(uniIn, 0, 3, 0x78040000); // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x78: 8x16,16x8,16x16
    VME_SET_DWORD(uniIn, 0, 3, 0x7e040000); // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x7e: 16x16 only

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
//    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
    // FWD and BWD cost centers for BDW+
    // cost centers are zero (no mv pedictors are used now)
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);

    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0, NULL, costCenter, imeOut);

    //DebugUniOutput<9>(imeOut);

    // 32x32
    vector<int2, 2> imv = imeOut.row(7).format<int2>().select<2,1>(10) << 1;  // MVs are ready for 32x32
    write(SURF_MV_OUT, mbX * MVDATA_SIZE, mbY, SLICE(imv.format<uint>(), 0, 1, 1));
}

extern "C" _GENX_MAIN_ 
void Ime_4MV(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_MV_OUT)
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
    matrix<uchar, 4, 32> uniIn = 0;
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
    VME_SET_DWORD(uniIn, 0, 3, 0x77040000); // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x7e: 8x8 only

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
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);
    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0, NULL, costCenter, imeOut);

    //DebugUniOutput<9>(imeOut);

    // 32x32
    matrix<int2, 2, 4> imv;
    imv.format<int2>() = imeOut.row(8).format<int2>().select<8,1>(8) << 1;  // 4 MVs 8x8
    write(SURF_MV_OUT, mbX * 2 * MVDATA_SIZE, mbY * 2, imv);
}

extern "C" _GENX_MAIN_ 
void ImeWithPred(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_MV_PRED, SurfaceIndex SURF_MV_OUT)
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
    matrix<uchar, 4, 32> uniIn = 0;
    matrix<uchar, 9, 32> imeOut;
    matrix<uchar, 2, 32> imeIn = 0;
    matrix<uchar, 4, 32> fbrIn;

    // declare parameters for VME
    matrix<uint4, 16, 2> costs = 0;
    vector<int2, 2> mvPred = 0;
    read(SURF_MV_PRED, (mbX >> 1) * MVDATA_SIZE, mbY >> 1, mvPred);

    // load search path
    SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control, 0, 64);

    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);

    // M0.3 various prediction parameters
    VME_SET_DWORD(uniIn, 0, 3, 0x78040000); // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x78
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
//    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);

    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0, NULL, costCenter, imeOut);

    //DebugUniOutput<9>(imeOut);

    // 32x32
//    SLICE(fbrOut16x16.format<short>(), 16, 2, 1) = SLICE(fbrOut16x16.format<short>(), 16, 2, 1) << 1;
    vector<short, 2> imv = imeOut.row(7).format<short>().select<2,1>(10) << 1;  // MVs are ready for 32x32
//    SLICE(fbrOut16x16.format<short>(), 16, 2, 1) = SLICE(fbrOut16x16.format<short>(), 16, 2, 1) << 1;
//    write(SURF_MV32x32, mbX * MVDATA_SIZE, mbY, SLICE(fbrOut16x16.format<uint>(), 8, 1, 1));
    write(SURF_MV_OUT, mbX * MVDATA_SIZE, mbY, SLICE(imv.format<uint>(), 0, 1, 1));
}


_GENX_ inline
void ImeOneTier4MV(vector_ref<int2, 2> mvPred, SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, matrix_ref<int2, 2, 4> mvOut,
                   uint2 width, uint2 height, matrix<uchar, 4, 32> uniIn, matrix<uchar, 2, 32> imeIn)
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
//    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
    // FWD and BWD cost centers for BDW+
    // cost centers are zero (no mv pedictors are used now)
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);
    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0, NULL, costCenter, imeOut);

    mvOut.format<int2>() = imeOut.row(8).format<int2>().select<8,1>(8) << 1;  // 4 MVs
}


extern "C" _GENX_MAIN_ 
void Ime3tiers4MV(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_REF_256x256,
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
    matrix<uchar, 4, 32> uniIn = 0;
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
    VME_SET_DWORD(uniIn, 0, 3, 0x77040000); // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x77: 8x8 only
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
//    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
    // FWD and BWD cost centers for BDW+
    // cost centers are zero (no mv pedictors are used now)
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);
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

            ImeOneTier4MV(mvPred0.format<int2>().select<2,1>(mvInd0), SURF_CONTROL, SURF_REF_128x128, mvPred1, width * 2, height * 2, uniIn, imeIn);

            #pragma unroll
            for (int yBlk1 = 0; yBlk1 < 32; yBlk1 += 16) {
                #pragma unroll
                for (int xBlk1 = 0; xBlk1 < 32; xBlk1 += 16) {
                    matrix<int2, 2, 4> mvPred2;
                    // update X and Y
                    VME_SET_UNIInput_SrcX(uniIn, xBase1 + xBlk0 * 2 + xBlk1);
                    VME_SET_UNIInput_SrcY(uniIn, yBase1 + yBlk0 * 2 + yBlk1);

                    int mvInd1 = ((yBlk1 >> 2) | (xBlk1 >> 3)); // choose 8x8_0, 8x8_1, 8x8_2 or 8x8_3

                    ImeOneTier4MV(mvPred1.format<int2>().select<2,1>(mvInd1), SURF_CONTROL, SURF_REF_64x64, mvPred2, width * 4, height * 4, uniIn, imeIn);
                    write(SURF_MV_64x64, (xBase1 + xBlk0 * 2 + xBlk1) / 8 * MVDATA_SIZE, (yBase1 + yBlk0 * 2 + yBlk1) / 8, mvPred2);
                }
            }

        }
    }
}

extern "C" _GENX_MAIN_ 
void MeP32Frac(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF,
               SurfaceIndex SURF_MV32x32, SurfaceIndex SURF_MV32x16, SurfaceIndex SURF_MV16x32)
{   // SURF_MV32x32 should keep MVs from IME for 32x32
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
    matrix<uchar, 4, 32> uniIn = 0;
    matrix<uchar, 9, 32> imeOut;
    matrix<uchar, 2, 32> imeIn = 0;
    matrix<uchar, 4, 32> fbrIn;

    // declare parameters for VME
    matrix<uint4, 16, 2> costs = 0;
    vector<int2, 2> mvPred = 0;
//>    read(SURF_MV32x32, mbX, mbY, mvPred);

    // load search path
    SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control, 0, 64);

    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);

    // M0.3 various prediction parameters
//>    VME_SET_DWORD(uniIn, 0, 3, 0x78040000); // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x78
    VME_SET_DWORD(uniIn, 0, 3, 0x78040000); // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x78
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
//    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
    // FWD and BWD cost centers for BDW+
    // cost centers are zero (no mv pedictors are used now)
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);

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
//>    fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 0) = mvPred.format<uint>()[0]; // motion vectors 16x16 (real 32x32 from Ime2x)
    run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 0, 0, 0, fbrOut16x16);

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

    // 32x32
    SLICE(fbrOut16x16.format<short>(), 16, 2, 1) = SLICE(fbrOut16x16.format<short>(), 16, 2, 1) << 1;
    write(SURF_MV32x32, mbX * MVDATA_SIZE, mbY, SLICE(fbrOut16x16.format<uint>(), 8, 1, 1));
    // 32x16
    fbrOut16x8.format<short, 7, 16>().select<2, 2, 2, 1>(1, 0) = fbrOut16x8.format<short, 7, 16>().select<2, 2, 2, 1>(1, 0) << 1;
    matrix<uint, 2, 1> mv32x16 = SLICE(fbrOut16x8.format<uint>(), 8, 2, 16); 
    write(SURF_MV32x16,  mbX * MVDATA_SIZE, mbY * 2, mv32x16);
    // 16x32
    fbrOut8x16.format<short, 7, 16>().select<2, 1, 2, 1>(1, 0) = fbrOut8x16.format<short, 7, 16>().select<2, 1, 2, 1>(1, 0) << 1;
    matrix<uint, 1, 2> mv16x32 = SLICE(fbrOut8x16.format<uint>(), 8, 2, 8);
    write(SURF_MV16x32,  mbX * MVDATA_SIZE * 2, mbY, mv16x32);
}

extern "C" _GENX_MAIN_ 
void MeP32(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_MV_PRED, // SURF_MV_PRED is DS 4x surf
           SurfaceIndex SURF_MV32x32, SurfaceIndex SURF_MV32x16, SurfaceIndex SURF_MV16x32, int rectParts) // SURF_MV32x32 is DS 2x surf
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
    matrix<uchar, 4, 32> uniIn = 0;
    matrix<uchar, 9, 32> imeOut;
    matrix<uchar, 2, 32> imeIn = 0;
    matrix<uchar, 4, 32> fbrIn;

    // declare parameters for VME
    matrix<uint4, 16, 2> costs = 0;
    vector<int2, 2> mvPred = 0;
    read(SURF_MV_PRED, (mbX >> 1) * MVDATA_SIZE, mbY >> 1, mvPred);

    // load search path
    SELECT_N_ROWS(imeIn, 0, 2) = SLICE1(control, 0, 64);

    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);

    // M0.3 various prediction parameters
    VME_SET_DWORD(uniIn, 0, 3, 0x78040000); // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x78: 8x16,16x8,16x16 (rect subparts are for FEI are on)
//    VME_SET_DWORD(uniIn, 0, 3, 0x7e040000); // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x7e: 16x16 only
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

    vector<int2,2>  ref0 = uniIn.row(0).format<int2>().select<2, 1> (0);
//    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
    // FWD and BWD cost centers for BDW+
    // cost centers are zero (no mv pedictors are used now)
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);

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

    // 32x32
    SLICE(fbrOut16x16.format<short>(), 16, 2, 1) = SLICE(fbrOut16x16.format<short>(), 16, 2, 1) << 1;
    write(SURF_MV32x32, mbX * MVDATA_SIZE, mbY, SLICE(fbrOut16x16.format<uint>(), 8, 1, 1));

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
void MeP32_4MV(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_MV32x32/*SURF_MV_PRED*/,
           /*SurfaceIndex SURF_MV32x32_4MV, */SurfaceIndex SURF_MV16x16, SurfaceIndex SURF_MV32x16, SurfaceIndex SURF_MV16x32, int rectParts) // SURF_MV32x32 is DS 2x surf
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
    matrix<uchar, 4, 32> uniIn = 0;
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
        VME_SET_DWORD(uniIn, 0, 3, 0x70040000); // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x78: 8x8,8x16,16x8,16x16 to have 4MVs in the output (for MeP16)
    } else {
        VME_SET_DWORD(uniIn, 0, 3, 0x76040000); // BMEDisableFBR=1 InterSAD=0 SubMbPartMask=0x78: 8x8,16x16 to have 4MVs in the output (for MeP16)
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
//    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
    // FWD and BWD cost centers for BDW+
    // cost centers are zero (no mv pedictors are used now)
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);

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
    SLICE(fbrOut16x16.format<short>(), 16, 2, 1) = SLICE(fbrOut16x16.format<short>(), 16, 2, 1) << 1;
    write(SURF_MV32x32, mbX * MVDATA_SIZE, mbY, SLICE(fbrOut16x16.format<uint>(), 8, 1, 1));    // 32x32 MVs are updated

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

_GENX_ inline
void SetUpVmeIntra(matrix_ref<uchar, 4, 32> uniIn, matrix_ref<uchar, 4, 32> sicIn,
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
    // M1.4 Cost center 0 for HSW; MBZ for BDW
    // M1.5 Cost center 1 for HSW; MBZ for BDW
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

    // BDW+
    // M3.0 FWD Cost center 0 Delta X and Y
    // M3.1 BWD Cost center 0 Delta X and Y
    // M3.2 FWD Cost center 1 Delta X and Y
    // M3.3 BWD Cost center 1 Delta X and Y
    // M3.4 FWD Cost center 2 Delta X and Y
    // M3.5 BWD Cost center 2 Delta X and Y
    // M3.6 FWD Cost center 3 Delta X and Y
    // M3.7 BWD Cost center 3 Delta X and Y

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
void MeP16_Intra(SurfaceIndex SURF_CURBE_DATA, SurfaceIndex SURF_SRC_AND_REF,
                 SurfaceIndex SURF_SRC, SurfaceIndex SURF_MBDISTINTRA)
{
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();

    vector<uchar, CURBEDATA_SIZE> curbeData;
    read(SURF_CURBE_DATA, 0,   curbeData.select<128,1>(0));
    read(SURF_CURBE_DATA, 128, curbeData.select<32, 1>(128));

    // read MB record data
    matrix<uchar, 3, 32> uniIn = 0;
    matrix<uchar, 9, 32> imeOut;
    matrix<uchar, 2, 32> imeIn = 0;
    matrix<uchar, 4, 32> fbrIn;

    matrix<uchar, 4, 32> uniInIntra;
    matrix<uchar, 4, 32> sicIn;
    matrix<uchar, 7, 32> sicOut;
    SetUpVmeIntra(uniInIntra, sicIn, curbeData, SURF_SRC, mbX, mbY);
    run_vme_sic(uniInIntra, sicIn, SURF_SRC_AND_REF, sicOut);

    write(SURF_MBDISTINTRA, mbX * MBINTRADIST_SIZE, mbY, SLICE1(sicOut.row(0), 12, 4));  // !!!sergo: use sicOut
}

extern "C" _GENX_MAIN_ 
void MeP16(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_MV_PRED, SurfaceIndex SURF_DIST16x16,
           SurfaceIndex SURF_DIST16x8, SurfaceIndex SURF_DIST8x16, SurfaceIndex SURF_DIST8x8,
           /*SurfaceIndex SURF_DIST8x4, SurfaceIndex SURF_DIST4x8, */SurfaceIndex SURF_MV16x16,
           SurfaceIndex SURF_MV16x8, SurfaceIndex SURF_MV8x16, SurfaceIndex SURF_MV8x8/*,
           SurfaceIndex SURF_MV8x4, SurfaceIndex SURF_MV4x8*/, int rectParts)
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
    matrix<uchar, 4, 32> uniIn = 0;
    matrix<uchar, 9, 32> imeOut;
    matrix<uchar, 2, 32> imeIn = 0;
    matrix<uchar, 4, 32> fbrIn;

    // declare parameters for VME
    matrix<uint4, 16, 2> costs = 0;
    vector<int2, 2> mvPred = 0;
    read(SURF_MV_PRED, (mbX >> 1) * MVDATA_SIZE, mbY >> 1, mvPred);

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

    vector<int2,2>  ref0 = uniIn.row(0).format<int2>().select<2, 1> (0);
//    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
    // FWD and BWD cost centers for BDW+
    // cost centers are zero (no mv pedictors are used now)
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);

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
}

extern "C" _GENX_MAIN_ 
void MeP16_4MV(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_DIST16x16,
           SurfaceIndex SURF_DIST16x8, SurfaceIndex SURF_DIST8x16, SurfaceIndex SURF_DIST8x8,
           /*SurfaceIndex SURF_DIST8x4, SurfaceIndex SURF_DIST4x8, */SurfaceIndex SURF_MV16x16,
           SurfaceIndex SURF_MV16x8, SurfaceIndex SURF_MV8x16, SurfaceIndex SURF_MV8x8/*,
           SurfaceIndex SURF_MV8x4, SurfaceIndex SURF_MV4x8*/, int rectParts)
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
    matrix<uchar, 4, 32> uniIn = 0;
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
//    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
    // FWD and BWD cost centers for BDW+
    // cost centers are zero (no mv pedictors are used now)
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);

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
}


#endif // 0