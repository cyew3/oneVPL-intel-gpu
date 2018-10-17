//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//


#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#include <cm/cm.h>
#include <cm/cmtl.h>

#define INTERDATA_SIZE_SMALL    8
#define INTERDATA_SIZE_BIG      64   // 32x32 and 64x64 blocks
#define MVDATA_SIZE             4    // mfxI16Pair
#define SINGLE_MVDATA_SIZE      2    // mfxI16Pair
#define MBDIST_SIZE             64   // 16*mfxU32
#define DIST_SIZE               4

#define SLICE(VEC, FROM, HOWMANY, STEP) ((VEC).select<HOWMANY, STEP>(FROM))
#define SLICE1(VEC, FROM, HOWMANY) SLICE(VEC, FROM, HOWMANY, 1)
#define SELECT_N_ROWS(m, from, nrows) m.select<nrows, 1, m.COLS, 1>(from)
#define SELECT_N_COLS(m, from, ncols) m.select<m.ROWS, 1, ncols, 1>(0, from)

#define VME_SET_DWORD(dst, r, c, src)           \
    dst.row(r).format<uint>().select<1, 1>(c) = src

#define VME_Output_S2(p, t, r, c, n) p.row(r).format<t>().select<n,1>(c)
#define VME_Output_S2a(p, t, c, n) p.format<t>().select<n,1>(c)
#define VME_Output_S4(p, t, r, c, n) p.row(r).format<t>().select<n,4>(c)
#define VME_Output_S4a(p, t, c, n) p.format<t>().select<n,8>(c)
#define VME_GET_IMEOutput_Rec0_8x8_4Distortion(p, v) (v = VME_Output_S2(p, ushort, 7, 4, 4))
#define VME_GET_IMEOutput_Rec1_8x8_4Distortion(p, v) (v = VME_Output_S2(p, ushort, 9, 4, 4))
#define VME_GET_IMEOutput_Rec0_8x8_4Mv(p, v) (v = VME_Output_S2(p, uint, 8, 4, 4))
#define VME_GET_IMEOutput_Rec1_8x8_4Mv(p, v) (v = VME_Output_S2(p, uint,10, 4, 4))
#define VME_GET_IMEOutput_Rec0_16x16_Mv(p, v) (v = VME_Output_S2(p, ushort, 7, 10, 2))
#define VME_GET_IMEOutput_Rec1_16x16_Mv(p, v) (v = VME_Output_S2(p, ushort, 9, 10, 2))

#define VME_GET_FBROutput_Rec0_16x16_Mv(p, v) (v = VME_Output_S2a(p, short, 16, 2))
#define VME_GET_FBROutput_Rec1_16x16_Mv(p, v) (v = VME_Output_S2a(p, short, 18, 2))
#define VME_GET_FBROutput_Rec0_8x8_4Mv(p, v) (v = VME_Output_S4a(p, uint, 8, 4))
#define VME_GET_FBROutput_Rec1_8x8_4Mv(p, v) (v = VME_Output_S4a(p, uint, 9, 4))

#define VME_GET_FBROutput_Dist_16x16_Bi(p, v) (v = VME_Output_S2(p, ushort, 5, 0, 1))
#define VME_GET_FBROutput_Dist_8x8_Bi(p, v) (v = VME_Output_S4(p, ushort, 5, 0, 4))


