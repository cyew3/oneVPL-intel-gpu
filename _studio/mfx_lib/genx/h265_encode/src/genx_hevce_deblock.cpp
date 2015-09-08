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

struct DeblockParam
{
    uint2 Width;
    uint2 Height;
    uint2 PicWidthInCtbs;
    uint2 PicHeightInCtbs;
    int2  tcOffset;
    int2  betaOffset;
    uint1 crossSliceBoundaryFlag;
    uint1 crossTileBoundaryFlag;
    uint1 TULog2MinSize;
    uint1 MaxCUDepth;
    uint1 Log2MaxCUSize;
    uint1 Log2NumPartInCU;
    uint1 MaxCUSize;
    uint1 chromaFormatIdc;
};


struct H265CUData16
{
    vector<int2, 32>  mv0;      // 16 mvs for reflist0 stored as x,y,x,y...
    vector<int2, 32>  mv1;      // 16 mvs for reflist1 stored as x,y,x,y...
    vector<int1, 32>  refIdx;   // 16 pairs of refidx stored as l0,l1,l0,l1...

    vector<uint2, 16> depth;
    vector<uint2, 16> predMode;
    vector<uint2, 16> totalDepth;
    vector<uint2, 16> qp;
    vector<uint2, 16> cbf0;
    vector<uint2, 16> absPartIdx;
    vector<int2, 16>  ctbAddr;
    vector<int2, 16>  outOfPic;
};

_GENX_ inline
void GetEdgeStrength(
    H265CUData16        &cudataQ,
    H265CUData16        &cudataP,
    vector_ref<int4,16>  list0,
    vector_ref<int4,16>  list1,
    uint1                MaxCUDepth,
    vector_ref<uint2,16> strength)
{
    vector<uint2,16> is_intra_any = (cudataP.predMode | cudataQ.predMode); // 3 instrs less than this: (cudataP.predMode == (uint1)MODE_INTRA) | (cudataQ.predMode == (uint1)MODE_INTRA)
    vector<uint2,16> xor_absPartIdx = (cudataP.absPartIdx ^ cudataQ.absPartIdx);

    // vector<uint2,16> mask_cbf0_any = ((cudataQ.cbf0 >> cudataQ.trIdx) > 0) | ((cudataP.cbf0 >> cudataP.trIdx) > 0);
    vector<uint2,16> mask_cbf0_any = (cudataQ.cbf0 > 0) | (cudataP.cbf0 > 0);

    vector<uint2,16> notSameCuDepth = (cudataP.depth != cudataQ.depth);
    vector<uint2,16> notSameTotDepth = (cudataP.totalDepth != cudataQ.totalDepth);
    vector<uint2,16> notSameCtb = (cudataQ.ctbAddr != cudataP.ctbAddr);

    // (xor_absPartIdx >> (((int2)MaxCUDepth - totDepthP) << 1)) > 0;
    vector<uint2,16> mask_xor_absPartIdx = MaxCUDepth - cudataP.totalDepth;
    mask_xor_absPartIdx <<= 1;
    mask_xor_absPartIdx = xor_absPartIdx >> mask_xor_absPartIdx;
    mask_xor_absPartIdx = mask_xor_absPartIdx > 0;

    vector<uint2,16> mask_checkIntraOnly = notSameCtb | notSameCuDepth | notSameTotDepth | mask_xor_absPartIdx;
    vector<uint2,16> mask_applyMV = (is_intra_any | (mask_checkIntraOnly & mask_cbf0_any)) == 0;

    // check MV
    vector<uint2,16> numRefsP = (cudataP.refIdx.select<16,2>(0) >= 0) + (cudataP.refIdx.select<16,2>(1) >= 0);
    vector<uint2,16> numRefsQ = (cudataQ.refIdx.select<16,2>(0) >= 0) + (cudataQ.refIdx.select<16,2>(1) >= 0);

    // maskTwoRef = ((numRefsQ + numRefsP) == 4) & mask_applyMV;
    vector<uint2,16> maskTwoRef = numRefsQ + numRefsP;
    maskTwoRef = maskTwoRef == 4;
    maskTwoRef &= mask_applyMV;

    vector<uint2,16> goodRefIdx0Q, goodRefIdx1Q, goodRefIdx0P, goodRefIdx1P;
    vector<uint2,16> maskRef0AvailQ = (cudataQ.refIdx.select<16,2>(0) >= 0);
    vector<uint2,16> maskRef0AvailP = (cudataP.refIdx.select<16,2>(0) >= 0);

    goodRefIdx0Q.merge(cudataQ.refIdx.select<16,2>(0), 0, maskRef0AvailQ);
    goodRefIdx0P.merge(cudataP.refIdx.select<16,2>(0), 0, maskRef0AvailP);
    goodRefIdx1Q.merge(cudataQ.refIdx.select<16,2>(1), 0, (cudataQ.refIdx.select<16,2>(1) >= 0));
    goodRefIdx1P.merge(cudataP.refIdx.select<16,2>(1), 0, (cudataP.refIdx.select<16,2>(1) >= 0));

    vector<int4,16> refPoc0Q = list0.iselect(goodRefIdx0Q);
    vector<int4,16> refPoc1Q = list1.iselect(goodRefIdx1Q);
    vector<int4,16> refPoc0P = list0.iselect(goodRefIdx0P);
    vector<int4,16> refPoc1P = list1.iselect(goodRefIdx1P);

    { // both P and Q have 2 refs
        vector<uint2,32> t0 = cm_abs<uint2>(cm_add<int2>(cudataP.mv0, -cudataQ.mv0, SAT));
        vector<uint2,32> t1 = cm_abs<uint2>(cm_add<int2>(cudataP.mv1, -cudataQ.mv1, SAT));
        vector<uint2,16> res1 =  (t0.select<16,2>(0) >= 4) | (t0.select<16,2>(1) >= 4) | (t1.select<16,2>(0) >= 4) | (t1.select<16,2>(1) >= 4);

        t0 = cm_abs<uint2>(cm_add<int2>(cudataP.mv0, -cudataQ.mv1, SAT));
        t1 = cm_abs<uint2>(cm_add<int2>(cudataP.mv1, -cudataQ.mv0, SAT));
        vector<uint2,16> res2 = (t0.select<16,2>(0) >= 4) | (t0.select<16,2>(1) >= 4) | (t1.select<16,2>(0) >= 4) | (t1.select<16,2>(1) >= 4);

        vector<uint2,16> res3 = res1 & res2;

        vector<uint2,16> mask_general = (((refPoc0Q == refPoc0P) & (refPoc1Q == refPoc1P)) | ((refPoc0Q == refPoc1P) & (refPoc1Q == refPoc0P)));
        vector<uint2,16> mask_NotEqualPOC_P0P1 = (refPoc0P != refPoc1P);
        vector<uint2,16> mask_EqualPOC_Q0P0 = (refPoc0Q == refPoc0P);
        vector<uint2,16> mask_common1 = mask_general & maskTwoRef;
        vector<uint2,16> mask_common2 = mask_common1 & mask_NotEqualPOC_P0P1;

        strength.merge(res1, 1, mask_common2 & mask_EqualPOC_Q0P0); // mask_general & mask_NotEqualPOC_P0P1 & mask_EqualPOC_Q0P0 & maskTwoRef
        strength.merge(res2, mask_common2 & !mask_EqualPOC_Q0P0); // mask_NotEqualPOC_P0P1 & !mask_EqualPOC_Q0P0 & maskTwoRef
        strength.merge(res3, mask_common1 & !mask_NotEqualPOC_P0P1); // mask_general & !mask_NotEqualPOC_P0P1 & maskTwoRef
    }

    { // both P and Q have 1 ref
        vector<int4,16> refPocQ, refPocP;
        refPocQ.merge(refPoc0Q, refPoc1Q, maskRef0AvailQ);
        refPocP.merge(refPoc0P, refPoc1P, maskRef0AvailP);

        vector<int2,32> mvQ, mvP;
        mvQ.format<uint4>().merge(cudataQ.mv0.format<uint4>(), cudataQ.mv1.format<uint4>(), maskRef0AvailQ);
        mvP.format<uint4>().merge(cudataP.mv0.format<uint4>(), cudataP.mv1.format<uint4>(), maskRef0AvailP);

        vector<uint2,32> t = cm_abs<uint2>(cm_add<int2>(mvP, -mvQ, SAT));
        vector<uint2,16> strengthOneRef = (t.select<16,2>(0) >= 4) | (t.select<16,2>(1) >= 4);
        vector<uint2,16> maskOneRef = (refPocQ == refPocP) & numRefsP & numRefsQ & mask_applyMV;

        strength.merge(strengthOneRef, maskOneRef);
    }

    strength.merge(0, is_intra_any);                       // strength=0
    strength.merge(2, is_intra_any & mask_checkIntraOnly); // strength=2
}


_GENX_ inline
void ReadCuData16(SurfaceIndex CU_DATA, vector_ref<uint4,16> addrs, H265CUData16 &cudata)
{
    // cuData structure layout
    // ------------------------------
    //                 b/dw offset
    // ------------------------------
    // mv0         0..4 / 0
    // mv1         4..7 / 1
    // refIdx    16..17 / 4
    // depth         18 / 4
    // predMode      19 / 4
    // trIdx         20 / 5
    // qp            21 / 5
    // cbf           22 / 5

    vector<uint1,64> readbuf;
    read(CU_DATA, 0u, addrs, readbuf.format<uint4>());
    cudata.mv0.select<32,1>(0) = readbuf.format<int2>();

    read(CU_DATA, 1u, addrs, readbuf.format<uint4>());
    cudata.mv1.select<32,1>(0) = readbuf.format<int2>();

    read(CU_DATA, 4u, addrs, readbuf.format<uint4>());
    cudata.refIdx.format<uint2>() = readbuf.format<uint2>().select<16,2>(0);
    cudata.depth.select<16,1>(0) = readbuf.select<16,4>(2);
    cudata.predMode.select<16,1>(0) = readbuf.select<16,4>(3);

    read(CU_DATA, 5u, addrs, readbuf.format<uint4>());
    vector_ref<uint1,16> trIdx = readbuf.select<16,4>(0);
    cudata.totalDepth.select<16,1>(0) = trIdx + cudata.depth.select<16,1>(0);
    cudata.qp.select<16,1>(0) = readbuf.select<16,4>(1);
    cudata.cbf0.select<16,1>(0) = readbuf.select<16,4>(2) >> trIdx;

}


_GENX_ inline void Filter4Edge(
    matrix_ref<uint1,8,16> src,
    vector_ref<uint2,4> tc,
    vector_ref<uint2,4> tc5,           // 5 * tc + 1 >> 1
    vector_ref<uint2,4> beta,
    vector_ref<uint2,4> beta2,         // beta >> 2
    vector_ref<uint2,4> beta3,         // beta >> 3
    vector_ref<uint2,4> sideThresh)    // beta + (beta >> 1) >> 3
{
    vector_ref<uint1,16> p3(src.row(0));
    vector_ref<uint1,16> p2(src.row(1));
    vector_ref<uint1,16> p1(src.row(2));
    vector_ref<uint1,16> p0(src.row(3));
    vector_ref<uint1,16> q0(src.row(4));
    vector_ref<uint1,16> q1(src.row(5));
    vector_ref<uint1,16> q2(src.row(6));
    vector_ref<uint1,16> q3(src.row(7));

    vector<int2,16> acc16w;

    vector<uint2,32> delta_pq;
    vector_ref<uint2,16> delta_q = delta_pq.select<16,1>(0);
    vector_ref<uint2,16> delta_p = delta_pq.select<16,1>(16);
    // delta_q = abs(q2 - 2*q1 + q0);
    acc16w = q2 + q0;
    acc16w -= 2 * q1;
    delta_q = cm_abs<uint2>(acc16w);
    // delta_p = abs(p2 - 2*p1 + p0);
    acc16w = p2 + p0;
    acc16w -= 2 * p1;
    delta_p = cm_abs<uint2>(acc16w);
    vector<uint2,16> sum_delta_pq = delta_p + delta_q;

    vector<uint2,4> d0 = sum_delta_pq.select<4,4>(0); //d0 = dp0 + dq0
    vector<uint2,4> d3 = sum_delta_pq.select<4,4>(3); //d3 = dp3 + dq3
    vector<uint2,4> d = d0 + d3; // d0(=dp0+dq0) + d3(=dp3+dq3)

    vector<uint2,16> ds; // ds = abs(p3 - p0) + abs(q0 - q3)
    ds  = cm_abs<uint2>(cm_add<int2>(p3, -p0));
    ds += cm_abs<uint2>(cm_add<int2>(q0, -q3));

    vector<int2,16> q0_minus_p0 = q0 - p0;
    vector<uint2,16> dm = cm_abs<uint2>(q0_minus_p0);

    vector<uint2,16> mask25 = cm_mul<uint2>(sum_delta_pq, 2) < beta2.replicate<4,1,4,0>();
    vector<uint2,16> mask14 = ds < beta3.replicate<4,1,4,0>();
    vector<uint2,16> mask36 = dm < tc5.replicate<4,1,4,0>();
    vector<uint2,4> strongFiltering =
        mask14.select<4,4>(0) & mask25.select<4,4>(0) & mask36.select<4,4>(0) &
        mask14.select<4,4>(3) & mask25.select<4,4>(3) & mask36.select<4,4>(3);

    vector<int2,4> tmp = d < beta;
    vector<int2,16> mask_apply_filter = tmp.replicate<4,1,4,0>();
    vector<uint2,16> mask_strong = strongFiltering.replicate<4,1,4,0>();
    vector<uint2,16> tc_spread = tc.replicate<4,1,4,0>();

    vector<uint1,16> p2_new_strong;
    vector<uint1,16> p1_new_strong;
    vector<uint1,16> p0_new_strong;
    vector<uint1,16> q0_new_strong;
    vector<uint1,16> q1_new_strong;
    vector<uint1,16> q2_new_strong;

    // p-processing
    vector<int2,16> limit;
    vector<int2,16> double_tc_spread = 2 * tc_spread;
    
    acc16w = q0 + p0;
    vector<int2,16> commonPartP = p1 + acc16w;
    vector<int2,16> commonPartQ = q1 + acc16w;
    vector<int2,16> commonPartPplus2 = commonPartP + 2;
    vector<int2,16> commonPartQplus2 = commonPartQ + 2;

    // p0 = SAT(p2 + 2 * commonPartP + q1 + 4) >> 3)
    acc16w = p2 + q1;
    acc16w += 2 * commonPartPplus2;
    acc16w = acc16w >> 3;
    limit = p0 - double_tc_spread;
    acc16w.merge(limit, acc16w < limit);
    limit = p0 + double_tc_spread;
    acc16w.merge(limit, acc16w > limit);
    p0_new_strong = cm_add<uint1, int2, int2>(acc16w, 0, SAT);

    // p1 = SAT((p2 + commonPartP + 2) >> 2)
    acc16w = p2 + commonPartPplus2;
    acc16w = acc16w >> 2;
    limit = p1 - double_tc_spread;
    acc16w.merge(limit, acc16w < limit);
    limit = p1 + double_tc_spread;
    acc16w.merge(limit, acc16w > limit);
    p1_new_strong = cm_add<uint1, int2, int2>(acc16w, 0, SAT);

    // p2 = SAT((2 * p3 + 3 * p2 + commonPartP + 4) >> 3)
    acc16w = commonPartP + 4;
    acc16w += 2 * p3;
    acc16w += 3 * p2;
    acc16w = acc16w >> 3;
    limit = p2 - double_tc_spread;
    acc16w.merge(limit, acc16w < limit);
    limit = p2 + double_tc_spread;
    acc16w.merge(limit, acc16w > limit);
    p2_new_strong = cm_add<uint1, int2, int2>(acc16w, 0, SAT);

    // q-processing

    // q0 = SAT((q2 + 2 * commonPartQ + p1 + 4) >> 3)
    acc16w = p1 + q2;
    acc16w += 2 * commonPartQplus2;
    acc16w = acc16w >> 3;
    limit = q0 - double_tc_spread;
    acc16w.merge(limit, acc16w < limit);
    limit = q0 + double_tc_spread;
    acc16w.merge(limit, acc16w > limit);
    q0_new_strong = cm_add<uint1, int2, int2>(acc16w, 0, SAT);

    // q1 = SAT((q2 + commonPartQ + 2) >> 2)
    acc16w = q2 + commonPartQplus2;
    acc16w = acc16w >> 2;
    limit = q1 - double_tc_spread;
    acc16w.merge(limit, acc16w < limit);
    limit = q1 + double_tc_spread;
    acc16w.merge(limit, acc16w > limit);
    q1_new_strong = cm_add<uint1, int2, int2>(acc16w, 0, SAT);

    // q2 = SAT((2 * q3 + 3 * q2 + commonPartQ + 4) >> 3)
    acc16w = commonPartQ + 4;
    acc16w += 2 * q3;
    acc16w += 3 * q2;
    acc16w = acc16w >> 3;
    limit = q2 - double_tc_spread;
    acc16w.merge(limit, acc16w < limit);
    limit = q2 + double_tc_spread;
    acc16w.merge(limit, acc16w > limit);
    q2_new_strong = cm_add<uint1, int2, int2>(acc16w, 0, SAT);

    // update pixels with strong filtering
    vector<uint2,16> mask_strong2 = mask_strong & mask_apply_filter;
    p2.merge(p2_new_strong, mask_strong2);
    p1.merge(p1_new_strong, mask_strong2);
    p0.merge(p0_new_strong, mask_strong2);
    q0.merge(q0_new_strong, mask_strong2);
    q1.merge(q1_new_strong, mask_strong2);
    q2.merge(q2_new_strong, mask_strong2);


    vector<uint1,16> p1_new_weak;
    vector<uint1,16> p0_new_weak;
    vector<uint1,16> q0_new_weak;
    vector<uint1,16> q1_new_weak;

    // delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4
    acc16w = q1 - p1;
    acc16w *= -3;
    acc16w += cm_mul<int2>(q0_minus_p0, 9);
    acc16w += 8;
    vector<int2,16> delta = acc16w >> 4;

    // keep here!!! because delta will be modified during weak filtering
    acc16w = tc_spread * 10;
    vector<int2,16> mask_weak_active = (cm_abs<int2>(delta) < acc16w) & !mask_strong;
    mask_weak_active &= mask_apply_filter;

    vector<uint2, 2*4> dqp = delta_pq.select<2*4,4>(0) + delta_pq.select<2*4,4>(3); //dq0 + dq3 //dp0 + dp3
    vector<int2, 2*4> threshCond = dqp < sideThresh.replicate<2>();

    vector<uint2,16> mask = (delta < -tc_spread);
    vector<uint2,16> tc_half = tc_spread >> 1;
    delta.merge(-tc_spread, mask);
    mask = (delta > tc_spread);
    delta.merge(tc_spread, mask);

    // p1 = SAT(p1 + SAT((((p2 + p0 + 1) >> 1) - p1 + delta) >> 1))
    acc16w = cm_avg<int2>(p2, p0);
    acc16w -= p1;
    acc16w += delta;
    acc16w >>= 1;
    acc16w.merge(-tc_half, acc16w < -tc_half);
    acc16w.merge(tc_half, acc16w > tc_half);
    p1_new_weak = cm_add<uint1, uint1, int2>(p1, acc16w, SAT);

    // q1 = SAT(q1 + SAT((((q2 + q0 + 1) >> 1) - q1 - delta) >> 1))
    acc16w = cm_avg<int2>(q2, q0);
    acc16w -= q1;
    acc16w -= delta;
    acc16w >>= 1;
    acc16w.merge(-tc_half, acc16w < -tc_half);
    acc16w.merge(tc_half, acc16w > tc_half);
    q1_new_weak = cm_add<uint1, uint1, int2>(q1, acc16w, SAT);

    p0_new_weak = cm_add<uint1, uint1, int2>(p0, delta, SAT);
    q0_new_weak = cm_add<uint1, uint1, int4>(q0, -delta, SAT);

    // update pixels with weak filtering
    p1.merge(p1_new_weak, mask_weak_active & threshCond.replicate<4,1,4,0>(4));
    q1.merge(q1_new_weak, mask_weak_active & threshCond.replicate<4,1,4,0>(0));
    p0.merge(p0_new_weak, mask_weak_active);
    q0.merge(q0_new_weak, mask_weak_active);
}


_GENX_ inline
void Transpose16R8C(matrix_ref<uint1,16,8> src, matrix_ref<uint1,8,16> dst)
{
    #pragma unroll
    for (int r = 0; r < 8; r++) {
        dst.row(r).select<8,1>(0) = src.select<8,1,1,1>(0, r);
        dst.row(r).select<8,1>(8) = src.select<8,1,1,1>(8, r);
    }
}


_GENX_ inline
void Transpose8R16C(matrix_ref<uint1,8,16> src, matrix_ref<uint1,16,8> dst)
{
    matrix<uint1,16,8> tmp;
    #pragma unroll
    for (int r = 0; r < 8; r++)
        tmp.select<2,1,8,1>(2*r, 0) = src.select<4,1,4,4>(4*(r&1), r/2);
    #pragma unroll
    for (int r = 0; r < 8; r++)
        dst.select<2,1,8,1>(2*r, 0) = tmp.select<8,1,2,4>(8*(r&1), r/2);
}


_GENX_ inline
void Transpose16R16C(matrix_ref<uint1,16,16> src, matrix_ref<uint1,16,16> dst)
{
    matrix<uint1,16,16> tmp;
    #pragma unroll
    for (int y = 0; y < 16; y++)
        tmp.row(y) = src.select<4,1,4,4>((y&3)<<2,y>>2);
    #pragma unroll
    for (int y = 0; y < 16; y++)
        dst.row(y) = tmp.select<4,1,4,4>((y&3)<<2,y>>2);
}

const int1 CENTERX[] = { 4, 4, 12, 12, 4, 4, 12, 12, 0, 0, 8, 8, 0, 0, 8, 8 };
const int1 CENTERY[] = { 0, 4, 0, 4, 8, 12, 8, 12, 0, 4, 0, 4, 8, 12, 8, 12 };

// --------------------------------------------------------
_GENX_ inline
void ReadPQData(
    SurfaceIndex          CU_DATA,
    DeblockParam         &param,
    vector_ref<uint1,16>  scan2z,
    vector_ref<int2,2>    globXY,
    H265CUData16         &cudataP,
    H265CUData16         &cudataQ)
{
    vector<int2,16> centerX(CENTERX); // xQ: 4, 4, 12, 12, 4, 4, 12, 12; xP: 0, 0, 8, 8, 0, 0, 8, 8
    centerX += globXY[0];
    vector<int2,16> centerY(CENTERY); // yQ: 0, 4, 0, 4, 8, 12, 8, 12; yP: 0, 4, 0, 4, 8, 12, 8, 12
    centerY += globXY[1];
    
    vector<int2,16> outOfPic;
    outOfPic.select<8,1>(8)  = (centerX.select<8,1>(8) <= 0);           // Left Ps
    outOfPic.select<8,1>(0)  = (centerX.select<8,1>(0) >= param.Width); // Right Qs
    outOfPic.select<8,2>(0) |= (centerY.select<8,2>(0) <= 0);           // Top Ps and Qs
    outOfPic.select<8,2>(1) |= (centerY.select<8,2>(1) >= param.Height);// Bottom Ps and Qs

    vector<int2,16> ctbAddr = centerY >> param.Log2MaxCUSize;
    ctbAddr = ctbAddr * param.PicWidthInCtbs;
    vector<int2,16> ctbAddrX = centerX >> param.Log2MaxCUSize;
    cudataQ.outOfPic.select<8,1>(0) = outOfPic.select<8,1>(0);
    cudataP.outOfPic.select<8,1>(0) = outOfPic.select<8,1>(8);
    ctbAddr += ctbAddrX;
    cudataQ.ctbAddr.select<8,1>(0) = ctbAddr.select<8,1>(0);
    cudataP.ctbAddr.select<8,1>(0) = ctbAddr.select<8,1>(8);

    vector<uint2,16> pix_col = centerX & (param.MaxCUSize - 1);
    vector<uint2,16> pix_row = centerY & (param.MaxCUSize - 1);
    pix_col >>= param.TULog2MinSize;
    pix_row >>= param.TULog2MinSize;

    vector<uint2,8> y = scan2z.iselect(pix_row.select<8,1>(0)) * 2; // assuming pix_row.select<8,1>(0) == pix_row.select<8,1>(8)
    vector<uint2,16> absPartIdx = scan2z.iselect(pix_col) + y.replicate<2>();
    cudataQ.absPartIdx.select<8,1>(0) = absPartIdx.select<8,1>(0);
    cudataP.absPartIdx.select<8,1>(0) = absPartIdx.select<8,1>(8);

    cudataP.outOfPic.select<8,1>(8).merge(cudataP.outOfPic.replicate<4,2,2,0>(0), cudataQ.outOfPic.replicate<4,2,2,0>(0), 0x55);
    cudataQ.outOfPic.select<8,1>(8).merge(cudataP.outOfPic.replicate<4,2,2,0>(1), cudataQ.outOfPic.replicate<4,2,2,0>(1), 0x55);
    cudataP.absPartIdx.select<8,1>(8).merge(cudataP.absPartIdx.replicate<4,2,2,0>(0), cudataQ.absPartIdx.replicate<4,2,2,0>(0), 0x55);
    cudataQ.absPartIdx.select<8,1>(8).merge(cudataP.absPartIdx.replicate<4,2,2,0>(1), cudataQ.absPartIdx.replicate<4,2,2,0>(1), 0x55);
    cudataP.ctbAddr.select<8,1>(8).merge(cudataP.ctbAddr.replicate<4,2,2,0>(0), cudataQ.ctbAddr.replicate<4,2,2,0>(0), 0x55);
    cudataQ.ctbAddr.select<8,1>(8).merge(cudataP.ctbAddr.replicate<4,2,2,0>(1), cudataQ.ctbAddr.replicate<4,2,2,0>(1), 0x55);

    int2 shift = (param.MaxCUDepth << 1);
    vector<uint4, 16> addr = cudataQ.ctbAddr << shift;
    addr += cudataQ.absPartIdx;
    addr *= 10; // addr and offsets are in dwords
#ifdef CMRT_EMU
    int4 maxAddr = ((param.PicWidthInCtbs * param.PicHeightInCtbs) << param.Log2NumPartInCU) * 10 - 10;
    vector<int4, 16> tmpAddr = addr;
    tmpAddr.merge(0, tmpAddr < 0);
    tmpAddr.merge(maxAddr, tmpAddr > maxAddr);
    addr = tmpAddr;
#endif
    ReadCuData16(CU_DATA, addr, cudataQ);

    addr = cudataP.ctbAddr << shift;
    addr += cudataP.absPartIdx;
    addr *= 10; // addr and offsets are in dwords
#ifdef CMRT_EMU
    tmpAddr = addr;
    tmpAddr.merge(0, tmpAddr < 0);
    tmpAddr.merge(maxAddr, tmpAddr > maxAddr);
    addr = tmpAddr;
#endif
    ReadCuData16(CU_DATA, addr, cudataP);
}


#define SWAP(TYPE, X,Y) { TYPE temp = X ; X = Y ; Y = temp; }

extern "C" _GENX_MAIN_
void Deblock(SurfaceIndex SRC_LU, SurfaceIndex SRC_CH, uint paddingLu, uint paddingCh,
             SurfaceIndex DST, SurfaceIndex CU_DATA, SurfaceIndex PARAM)
{
    DeblockParam param;
    vector<uint1,64> part0, part1, part2, part3, part4;
    vector<uint1,16> part5;
    read(PARAM, 0, part0);
    read(PARAM, 64, part1);
    read(PARAM, 128, part2);
    read(PARAM, 192, part3);
    read(PARAM, 256, part4);
    read(PARAM, 320, part5);

    param.Width                  = part0.format<int2>()[26];
    param.Height                 = part0.format<int2>()[27];
    param.PicWidthInCtbs         = part0.format<int2>()[28];
    param.PicHeightInCtbs        = part0.format<int2>()[29];
    param.tcOffset               = part0.format<int2>()[30];
    param.betaOffset             = part0.format<int2>()[31];
    param.crossSliceBoundaryFlag = part1.format<uint1>()[122-64];
    param.crossTileBoundaryFlag  = part1.format<uint1>()[123-64];
    param.TULog2MinSize          = part1.format<uint1>()[124-64];
    param.MaxCUDepth             = part1.format<uint1>()[125-64];
    param.Log2MaxCUSize          = part1.format<uint1>()[126-64];
    param.Log2NumPartInCU        = part1.format<uint1>()[127-64];
    param.MaxCUSize              = part2.format<uint1>()[182-128];
    param.chromaFormatIdc        = part2.format<uint1>()[183-128];
    

    vector_ref<uint1,52> betaTable = part0.select<52,1>();
    vector_ref<uint1,58> chromaQpTable = part1.select<58,1>();
    vector_ref<uint1,54> tcTable = part2.select<54,1>();
    vector_ref<int4,16> list0 = part3.format<int4>();
    vector_ref<int4,16> list1 = part4.format<int4>();
    vector_ref<uint1,16> scan2z = part5;

    //central point of first cross
    vector<int2,2> globXY;
    globXY[0] = get_thread_origin_x() * 16;
    globXY[1] = get_thread_origin_y() * 16;
    globXY -= 12;

    // Calc Deblock Strengths for block 16x16 via 4x 8x8
    H265CUData16 cudataQ;
    H265CUData16 cudataP;

    /*
    *  For vertical edges Ps and Qs are stored like this:
    *  P0 | Q0   P2 | Q2
    *  ---|---   ---|---
    *  P1 | Q1   P3 | Q3
    *  
    *  P4 | Q4   P6 | Q6
    *  ---|---   ---|---
    *  P5 | Q5   P7 | Q7
    *  
    *  For horizontal edges Ps and Qs are stored like this:
    *   P8 | P9    P10 | P11
    *  ----|----   ----|----
    *   Q8 | Q9    Q10 | Q11
    *  
    *  P12 | P13   P14 | P15
    *  ----|----   ----|----
    *  Q12 | Q13   Q14 | Q15
    */

    ReadPQData(CU_DATA, param, scan2z, globXY, cudataP, cudataQ);

    // qPL = (QpQ + QpP + 1) >> 1
    vector<uint2,16> edge_qp = cm_avg<uint2>(cudataQ.qp, cudataP.qp);

    vector<uint2,16> strength;
    GetEdgeStrength(cudataQ, cudataP, list0, list1, param.MaxCUDepth, strength);

    strength.merge(0, cudataP.outOfPic | cudataQ.outOfPic);

    // we read cross in scan order but it is optimal to have slightly different order
    ////4,5<->2,3
    SWAP(uint4, strength.format<uint4>()[1], strength.format<uint4>()[2])
    SWAP(uint4, edge_qp.format<uint4>()[1], edge_qp.format<uint4>()[2])

    // calcualte ts
    // tcIdx = Clip3(0, 53, qPL + 2 * (bS - 1) + (slice_tc_offset_div2 << 1))
    vector<int2, 16> tmp1 = cm_add<int2>(param.tcOffset, -2) + cm_mul<int2>(strength, 2);
    vector<uint2,16> tcIdx = cm_add<uint2, uint2, int2>(edge_qp, tmp1, SAT);
    tcIdx.merge(53, tcIdx > 53);
    vector<uint2,16> tc = tcTable.iselect(tcIdx);
    tc.merge(0, strength < 1);
    vector<uint2,16> tc5 = cm_avg<uint2>(cm_mul<uint2>(tc, 5), 0); // tc5 = ((tc * 5 + 1) >> 1);

    // calcualte beta
    // betaIdx = Clip3(0, 51, qPL + (slice_beta_offset_div2 << 1))
    vector<uint2,16> betaIdx = cm_add<uint2, uint2, int2>(edge_qp, param.betaOffset, SAT);
    betaIdx.merge(51, betaIdx > 51);
    vector<uint2,16> beta = betaTable.iselect(betaIdx);
    vector<uint2,16> beta2 = (beta >> 2);
    vector<uint2,16> beta3 = (beta >> 3);
    vector<uint2,16> sideThresh = cm_add<int2>(beta, cm_asr<int2, uint2>(beta, 1)) >> 3; // beta + (beta >> 1) >> 3

    matrix<uint1,16,16> src;
    read(SRC_LU, paddingLu+globXY[0], globXY[1], src);

    Transpose16R16C(src, src);
    Filter4Edge(src.select<8,1,16,1>(0,0), tc.select<4,1>(0), tc5.select<4,1>(0), beta.select<4,1>(0), beta2.select<4,1>(0), beta3.select<4,1>(0), sideThresh.select<4,1>(0));
    Filter4Edge(src.select<8,1,16,1>(8,0), tc.select<4,1>(4), tc5.select<4,1>(4), beta.select<4,1>(4), beta2.select<4,1>(4), beta3.select<4,1>(4), sideThresh.select<4,1>(4));
    Transpose16R16C(src, src);

    Filter4Edge(src.select<8,1,16,1>(0), tc.select<4,1>(8), tc5.select<4,1>(8), beta.select<4,1>(8), beta2.select<4,1>(8), beta3.select<4,1>(8), sideThresh.select<4,1>(8));
    Filter4Edge(src.select<8,1,16,1>(8), tc.select<4,1>(12), tc5.select<4,1>(12), beta.select<4,1>(12), beta2.select<4,1>(12), beta3.select<4,1>(12), sideThresh.select<4,1>(12));
    write_plane(DST, GENX_SURFACE_Y_PLANE, globXY[0], globXY[1], src);


    // --- CHROMA --- //
    // chroma edge params are taken from luma edge params with indices 5, 7, 13 and 15
    vector<uint2,4> strengthChroma = strength.replicate<2,8,2,2>(5);
    vector<uint1,4> qpChroma = chromaQpTable.iselect(edge_qp.replicate<2,8,2,2>(5));

    // calculate tcChroma
    vector<int2,4> tmp = cm_add<int2>(param.tcOffset, -2) + cm_mul<int2>(strengthChroma, 2);
    vector<uint2,4> tcChromaIdx = cm_add<uint2>(qpChroma, tmp, SAT);
    tcChromaIdx.merge(53, tcChromaIdx > 53);
    vector<uint2,4> tcChroma = tcTable.iselect(tcChromaIdx);
    tcChroma.merge(0, strengthChroma <= 1);

    int4 chromaX = globXY[0] + 4;
    int4 chromaY = globXY[1] >> 1;
    chromaY += 2;

    vector<int2,16> tmp16w;
    matrix<uint1,8,16> src8x16;
    { // filter both vertical chroma edges
        matrix<uint2,8,2> tcChromaVert = tcChroma.replicate<2,1,8,0>(0); // replicate tc, 2 edges -> 2*4*2 chroma pixels
        read(SRC_CH, paddingCh+chromaX, chromaY, src8x16);
        matrix<uint1,8,8> src8x8 = src8x16.select<8,1,8,1>(0,4);

        matrix_ref<uint1,8,2> p1(src8x8.select<8,1,2,1>(0,0));
        matrix_ref<uint1,8,2> p0(src8x8.select<8,1,2,1>(0,2));
        matrix_ref<uint1,8,2> q0(src8x8.select<8,1,2,1>(0,4));
        matrix_ref<uint1,8,2> q1(src8x8.select<8,1,2,1>(0,6));

        // delta = SAT((((q0 - p0) << 2) + (p1 - q1) + 4) >> 3)
        tmp16w = p1 - q1;
        tmp16w += 4;
        tmp16w += 4 * cm_add<int2>(q0, -p0); // expect mac instruction here
        matrix<int2,8,2> delta = tmp16w >> 3;
        delta.merge(tcChromaVert, delta > tcChromaVert);
        delta.merge(-tcChromaVert, delta < -tcChromaVert);

        // interpet pair of chroma pixels as single uword
        src8x8.format<uint2,8,4>().column(1) = cm_add<uint1>(p0, delta, SAT).format<uint2>();
        src8x8.format<uint2,8,4>().column(2) = cm_add<uint1>(q0, -delta, SAT).format<uint2>();

        src8x16.select<8,1,8,1>(0,4) = src8x8;
    }

    { // filter both horizonal chroma edges
        vector_ref<uint1,16> p1(src8x16.row(2));
        vector_ref<uint1,16> p0(src8x16.row(3));
        vector_ref<uint1,16> q0(src8x16.row(4));
        vector_ref<uint1,16> q1(src8x16.row(5));

        vector<uint2,16> tcChromaHorz = tcChroma.replicate<2,1,8,0>(2); // replicate tc, 2 edges -> 2*4*2 chroma pixels

        // delta = SAT((((q0 - p0) << 2) + (p1 - q1) + 4) >> 3)
        tmp16w = p1 - q1;
        tmp16w += 4;
        tmp16w += 4 * cm_add<int2>(q0, -p0); // expect mac instruction here
        vector<int2,16> delta = tmp16w >> 3;
        delta.merge(tcChromaHorz, delta > tcChromaHorz);
        delta.merge(-tcChromaHorz, delta < -tcChromaHorz);
        p0 = cm_add<uint1>(p0, delta, SAT); // p0 = SAT(p0 + delta)
        q0 = cm_add<uint1>(q0, -delta, SAT); // p0 = SAT(p0 + delta)
        write_plane(DST, GENX_SURFACE_UV_PLANE, chromaX, chromaY, src8x16);
    }
}
