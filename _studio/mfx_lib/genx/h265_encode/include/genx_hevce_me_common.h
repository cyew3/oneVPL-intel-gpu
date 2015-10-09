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

#define MVDATA_SIZE     4 // mfxI16Pair
#define MBDIST_SIZE     64  // 16*mfxU32
#define DIST_SIZE       4

#define SLICE(VEC, FROM, HOWMANY, STEP) ((VEC).select<HOWMANY, STEP>(FROM))
#define SLICE1(VEC, FROM, HOWMANY) SLICE(VEC, FROM, HOWMANY, 1)
#define SELECT_N_ROWS(m, from, nrows) m.select<nrows, 1, m.COLS, 1>(from)
#define SELECT_N_COLS(m, from, ncols) m.select<m.ROWS, 1, ncols, 1>(0, from)

#define VME_SET_DWORD(dst, r, c, src)           \
    dst.row(r).format<uint4>().select<1, 1>(c) = src


_GENX_ inline
void SetRef(vector_ref<int2, 2> /*source*/,     // IN:  SourceX, SourceY
            vector<int2, 2>     predictor,      // IN:  mv predictor
            vector_ref<int1, 2> searchWindow,   // IN:  reference window w/h
            vector<uint1, 2>    /*picSize*/,    // IN:  pic size w/h
            vector_ref<int2, 2> reference)      // OUT: Ref0X, Ref0Y
{
    vector<int2, 2> Width = (searchWindow - 16) >> 1;
    vector<int2, 2> MaxMvLen;
    //vector<int2, 2> RefSize;
    vector<short, 2> mask;
    vector<int2, 2> res, otherRes;

    // set up parameters
    // MaxMvLen[0] = 512;
    MaxMvLen[0] = 0x7fff / 4;
    MaxMvLen[1] = 0x7fff / 4;
    //RefSize[0] = picSize[1] * 16;
    //RefSize[1] = (picSize[0] + 1) * 16;

    // fields and MBAFF are not supported 

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
    //mask = (RefSize <= res);
    //otherRes = ((RefSize - 1) & ~3) - source;
    //reference.merge(otherRes, mask);

    //res = reference + source + searchWindow;
    //mask = (0 >= res);
    //otherRes = ((-searchWindow + 5) & ~3) - source;
    //reference.merge(otherRes, mask);

    //reference[1] &= -2;
}
