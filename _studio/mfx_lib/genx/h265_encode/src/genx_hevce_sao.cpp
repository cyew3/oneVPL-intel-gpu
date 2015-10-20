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
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#include <cm/cm.h>
#include <cm/cmtl.h>
#include <cm/genx_vme.h>


enum { W_STAT = 16, H_STAT = 16 };
enum { W_STAT_CHROMA = 8, H_STAT_CHROMA = 16 };                // <=  use separate Chroma kernel with alternative threadspace
//enum { W_STAT_CHROMA = W_STAT/2, H_STAT_CHROMA = H_STAT/2 }; // <=  switch on in case of use the same threadspace as for luma

struct MiniVideoParam
{
	int4 Width;
	int4 Height;	
	int4 PicWidthInCtbs;	
	int4 PicHeightInCtbs;		

	int4 chromaFormatIdc;
	int4 MaxCUSize;		
	int4 bitDepthLuma;
	int4 saoOpt;

	float m_rdLambda;
	int4 SAOChromaFlag;
    int4 enableBandOffset;
    int4 offsetChroma;
}; // sizeof == 48 bytes


_GENX_ inline
void ReadParam(SurfaceIndex PARAM, MiniVideoParam& param)
{
#if 0
    vector<uint1, 48> data_pack;
    read(PARAM, 0, data_pack);

    param.Width  = data_pack.format<int4>()[0];
    param.Height = data_pack.format<int4>()[1];
    param.PicWidthInCtbs  = data_pack.format<int4>()[2];
    param.PicHeightInCtbs = data_pack.format<int4>()[3];

    //param.chromaFormatIdc = data_pack.format<int4>()[4];
    param.MaxCUSize = data_pack.format<int4>()[5];
    //param.bitDepthLuma = data_pack.format<int4>()[6];
    //param.saoOpt = data_pack.format<int4>()[7];

    param.m_rdLambda  = data_pack.format<float>()[8];
    param.SAOChromaFlag = data_pack.format<int4>()[9];
    param.enableBandOffset = data_pack.format<int4>()[10];
    //param.reserved1  = data_pack.format<int4>()[10];
    //param.reserved2 = data_pack.format<int4>()[11];
#else
    vector<uint1,64> part0, part1, part2, part3, part4;
    vector<uint1,32> part5;
    read(PARAM, 0, part0);
    //read(PARAM, 64, part1);
    read(PARAM, 128, part2);
    //read(PARAM, 192, part3);
    //read(PARAM, 256, part4);
    read(PARAM, 320, part5);    

    param.Width                  = part0.format<int2>()[26];
    param.Height                 = part0.format<int2>()[27];
    param.PicWidthInCtbs         = part0.format<int2>()[28];
    param.PicHeightInCtbs        = part0.format<int2>()[29];
    /*param.tcOffset               = part0.format<int2>()[30];
    param.betaOffset             = part0.format<int2>()[31];
    param.crossSliceBoundaryFlag = part1.format<uint1>()[122-64];
    param.crossTileBoundaryFlag  = part1.format<uint1>()[123-64];
    param.TULog2MinSize          = part1.format<uint1>()[124-64];
    param.MaxCUDepth             = part1.format<uint1>()[125-64];
    param.Log2MaxCUSize          = part1.format<uint1>()[126-64];
    param.Log2NumPartInCU        = part1.format<uint1>()[127-64];*/
    param.MaxCUSize              = part2.format<uint1>()[182-128];
    //param.chromaFormatIdc        = part2.format<uint1>()[183-128];

    // sao extension
    param.m_rdLambda  = part5.format<float>()[4];
    param.SAOChromaFlag = part5.format<int4>()[5];
    param.enableBandOffset = part5.format<int4>()[6];
    param.offsetChroma     = part5.format<int4>()[7];

    //printf("\n lambda %15.3f enableBO \n", param.m_rdLambda, param.enableBandOffset);
#endif
}


enum SaoModes
{
  SAO_MODE_OFF = 0,
  SAO_MODE_ON,
  SAO_MODE_MERGE_LEFT,
  SAO_MODE_MERGE_ABOVE,
  NUM_SAO_MODES
};

enum SaoBaseTypes
{
  SAO_TYPE_EO_0 = 0,
  SAO_TYPE_EO_90,
  SAO_TYPE_EO_135,
  SAO_TYPE_EO_45,
  SAO_TYPE_BO,
  NUM_SAO_BASE_TYPES
};


template<class TV, class TI, uint W> _GENX_ inline void MinHalf(vector_ref<TV,W> values, vector_ref<TI,W> indexes)
{
    vector<int2,W/2> mask = values.template select<W/2,1>(0) > values.template select<W/2,1>(W/2);
    values.template select<W/2,1>(0).merge(values.template select<W/2,1>(W/2), mask);
    indexes.template select<W/2,1>(0).merge(indexes.template select<W/2,1>(W/2), mask);
}


// Truncated Rise
//   0..7  - Edge Offsets
//   8..15 - Band Offsets (+1 sign bit for non-zero offsets)
const uint2 OFFSET_BITS[16] = {1,2,3,4,5,6,7,7,1,3,4,5,6,7,8,8};
const uint2 EO_CLASSES[4] = {SAO_TYPE_EO_0, SAO_TYPE_EO_90, SAO_TYPE_EO_135, SAO_TYPE_EO_45};

#define ENABLE_RDO

template<uint W> _GENX_ inline void Classify(vector_ref<int2,W> diff, vector_ref<int2,W> cls, matrix_ref<int2,4,W> totalDiffEO, matrix_ref<int2,4,W> totalCountEO, int row)
{
    if (row == 0) {
        totalDiffEO.row(0).merge(diff, 0, cls == -2);
        totalCountEO.row(0).merge(1, 0, cls == -2);
        totalDiffEO.row(1).merge(diff, 0, cls == -1);
        totalCountEO.row(1).merge(1, 0, cls == -1);
        totalDiffEO.row(2).merge(diff, 0, cls == 1);
        totalCountEO.row(2).merge(1, 0, cls == 1);
        totalDiffEO.row(3).merge(diff, 0, cls == 2);
        totalCountEO.row(3).merge(1, 0, cls == 2);
    } else {
        vector<int2,W> maskedDiff;
        vector<uint2,W> maskedCount;
        maskedDiff.merge(diff, 0, cls == -2);
        maskedCount.merge(1, 0, cls == -2);
        totalDiffEO.row(0) += maskedDiff;
        totalCountEO.row(0) += maskedCount;
        maskedDiff.merge(diff, 0, cls == -1);
        maskedCount.merge(1, 0, cls == -1);
        totalDiffEO.row(1) += maskedDiff;
        totalCountEO.row(1) += maskedCount;
        maskedDiff.merge(diff, 0, cls == 1);
        maskedCount.merge(1, 0, cls == 1);
        totalDiffEO.row(2) += maskedDiff;
        totalCountEO.row(2) += maskedCount;
        maskedDiff.merge(diff, 0, cls == 2);
        maskedCount.merge(1, 0, cls == 2);
        totalDiffEO.row(3) += maskedDiff;
        totalCountEO.row(3) += maskedCount;
    }
}

template<class T, uint W> _GENX_ inline vector_ref<T,W> MakeRef(vector<T,W> v) { return v; }

template<class RT, class T, uint R> _GENX_ inline vector<RT,R> ParallelSum(matrix_ref<T,R,16> m)
{
    vector_ref<T,R*16> v = m.template format<T>();
    v.template select<R*16/2,1>(0) = v.template select<R*16/2,2>(0) + v.template select<R*16/2,2>(1);
    v.template select<R*16/4,1>(0) = v.template select<R*16/4,2>(0) + v.template select<R*16/4,2>(1);
    v.template select<R*16/8,1>(0) = v.template select<R*16/8,2>(0) + v.template select<R*16/8,2>(1);
    return v.template select<R*16/16,2>(0) + v.template select<R*16/16,2>(1);
}

template<class RT, class T, uint R> _GENX_ inline vector<RT,R> ParallelSum_Chroma(matrix_ref<T,R,16> m, int offset)
{
    vector_ref<T,R*16/2> v = m.template format<T>().template select<R*16/2,2>(offset);
    //v.template select<R*16/2,1>(0) = v.template select<R*16/2,2>(0) + v.template select<R*16/2,2>(1);
    vector<T,R*16/4> tmp;
    tmp = v.template select<R*16/4,2>(0) + v.template select<R*16/4,2>(1);
    tmp.template select<R*16/8,1>(0) = tmp.template select<R*16/8,2>(0) + tmp.template select<R*16/8,2>(1);
    return tmp.template select<R*16/16,2>(0) + tmp.template select<R*16/16,2>(1);
}

template<class RT, class T, uint R> _GENX_ inline vector<RT,R> ParallelSum(matrix_ref<T,R,32> m)
{
    vector_ref<T,R*C> v = m.template format<T>();
    v.template select<R*32/2,1>(0) = v.template select<R*32/2,2>(0) + v.template select<R*32/2,2>(1);
    v.template select<R*32/4,1>(0) = v.template select<R*32/4,2>(0) + v.template select<R*32/4,2>(1);
    v.template select<R*32/8,1>(0) = v.template select<R*32/8,2>(0) + v.template select<R*32/8,2>(1);
    v.template select<R*32/16,1>(0) = v.template select<R*32/16,2>(0) + v.template select<R*32/16,2>(1);
    return v.template select<R*32/32,2>(0) + v.template select<R*32/32,2>(1);
}

template<uint W, uint H> _GENX_ inline void GetBOStat(matrix_ref<uint1,H,W> rec, matrix_ref<int2,H,W> diff, vector_ref<int2,32> diffBO, vector_ref<int2,32> countBO, int2 ox, int2 oy, int4 width, int4 height)
{
    diffBO = 0;
    countBO = 0;
    matrix<uint1,H,W> bands = rec >> 3;

    #pragma unroll
    for (uint1 y8=0; y8<H; y8 += 8) {
        if (oy + y8 < height) {
            #pragma unroll
            for (uint1 x8=0; x8<W; x8 += 8) {
                if (ox + x8 < width) {
                    #pragma unroll
                    for (uint1 y=0; y<8; y++) {
                        #pragma unroll
                        for (uint1 x=0; x<8; x++) {
                            uint1 bandIdx = bands(y8+y,x8+x);
                            diffBO[bandIdx] += diff(y8+y,x8+x);
                            countBO[bandIdx]++;
                        }                
                    }
                }
            }
        }
    }
}

template<uint W, uint H> 
_GENX_ inline void GetBOStat_Chroma(
    matrix_ref<uint1,H,W> rec, matrix_ref<int2,H,W> diff, 
    vector_ref<int2,32> diffBO_U, vector_ref<int2,32> countBO_U,
    vector_ref<int2,32> diffBO_V, vector_ref<int2,32> countBO_V, 
    int2 ox, int2 oy, int4 width, int4 height)
{
    //diffBO_U = 0;
    //countBO_U = 0;
    //diffBO_V = 0;
    //countBO_V = 0;
    matrix<uint1,H,W> bands = rec >> 3;

    #pragma unroll
    for (uint1 y4=0; y4<H; y4 += 4) {
        if (oy + y4 < height) {
            #pragma unroll
            for (uint1 x4=0; x4<W/2; x4 += 4) {
                if (ox + 2*x4 < width) {
                    #pragma unroll
                    for (uint1 y=0; y<4; y++) {
                        #pragma unroll
                        for (uint1 x=0; x<4; x++) {
                            // U
                            uint1 bandIdx = bands(y4+y,2*(x4+x));
                            diffBO_U[bandIdx] += diff(y4+y,2*(x4+x));
                            countBO_U[bandIdx]++;
                            // V
                            bandIdx = bands(y4+y,2*(x4+x)+1);
                            diffBO_V[bandIdx] += diff(y4+y,2*(x4+x)+1);
                            countBO_V[bandIdx]++;
                        }
                    }
                }
            }
        }
    }
}
template<uint W> _GENX_ inline void GetSign(vector_ref<uint1,W> blk1, vector_ref<uint1,W> blk2, vector_ref<int2,W> sign)
{
    static_assert(W % 16 == 0, "W must be divisible by 16");

    vector<int2,W> q;
    #pragma unroll
    for (int i = 0; i < W; i += 16) // do this by 16 elements to get add.g.f instruction instead of 2 add + cmp
        q.template select<16,1>(i) = blk1.template select<16,1>(i) - blk2.template select<16,1>(i);
    sign = q >> 15;
    sign.merge(1, q > 0);
}

template<uint W, uint H> _GENX_ inline void GetEOStat(matrix_ref<uint1,H+2,W> recon, matrix_ref<uint1,H+2,W> reconL, matrix_ref<uint1,H+2,W> reconR, matrix_ref<int2,H,W> diff,
                             vector_ref<int2,16> diffEO, vector_ref<int2,16> countEO,
                             vector_ref<uint2,W> maskHor0, vector_ref<uint2,W> maskHor1, vector_ref<uint2,H> maskVer0, vector_ref<uint2,H> maskVer1,
                             vector_ref<int2,16> diffEO_2, vector_ref<int2,16> countEO_2,
                             uint1 chromaSplit = 0)
{
    matrix<int2,16,W> totalDiffEO;
    matrix<int2,16,W> totalCountEO;
    vector<int2,W> q, cls;
    matrix<int2,2,W> sign;
    vector_ref<int2,W> sign1 = sign.row(0);
    vector_ref<int2,W> sign2 = sign.row(1);

    // EO 0
    #pragma unroll
    for (int y = 0; y < H; y++) {
        GetSign(recon.row(y+1), reconL.row(y+1), sign1);
        GetSign(recon.row(y+1), reconR.row(y+1), sign2);
        cls = sign1 + sign2;
        cls.merge(0, maskHor1 | vector<uint2,W>(maskVer0[y]));
        Classify(diff.row(y), MakeRef(cls), totalDiffEO.template select<4,1,W,1>(0,0), totalCountEO.template select<4,1,W,1>(0,0), y);
    }            
    // E90
    GetSign(recon.row(0), recon.row(1), sign.row(1));
    #pragma unroll
    for (int y = 0; y < H; y++) {
        GetSign(recon.row(y+1), recon.row(y+2), sign.row(y & 1));
        cls = sign.row(y & 1) - sign.row(1 - (y & 1));
        cls.merge(0, maskHor0 | vector<uint2,W>(maskVer1[y]));
        Classify(diff.row(y), MakeRef(cls), totalDiffEO.template select<4,1,W,1>(4,0), totalCountEO.template select<4,1,W,1>(4,0), y);
    }
    // EO 135
    #pragma unroll
    for (int y = 0; y < H; y++) {
        GetSign(recon.row(y+1), reconL.row(y), sign1);
        GetSign(recon.row(y+1), reconR.row(y+2), sign2);
        cls = sign1 + sign2;
        cls.merge(0, maskHor1 | vector<uint2,W>(maskVer1[y]));
        Classify(diff.row(y), MakeRef(cls), totalDiffEO.template select<4,1,W,1>(8,0), totalCountEO.template select<4,1,W,1>(8,0), y);
    }            
    // EO 45
    #pragma unroll
    for (int y = 0; y < H; y++) {
        GetSign(recon.row(y+1), reconL.row(y+2), sign1);
        GetSign(recon.row(y+1), reconR.row(y), sign2);
        cls = sign1 + sign2;
        cls.merge(0, maskHor1 | vector<uint2,W>(maskVer1[y]));
        Classify(diff.row(y), MakeRef(cls), totalDiffEO.template select<4,1,W,1>(12,0), totalCountEO.template select<4,1,W,1>(12,0), y);
    }            

    if (chromaSplit == 0) {
        diffEO = ParallelSum<int2>(totalDiffEO.select_all());
        countEO = ParallelSum<int2>(totalCountEO.select_all());
    } else {
        diffEO = ParallelSum_Chroma<int2>(totalDiffEO.select_all(), 0);
        countEO = ParallelSum_Chroma<int2>(totalCountEO.select_all(), 0);
        diffEO_2 = ParallelSum_Chroma<int2>(totalDiffEO.select_all(), 1);
        countEO_2 = ParallelSum_Chroma<int2>(totalCountEO.select_all(), 1);
    }
}
const int2 IDX_INIT_SEQ[] = { 0,1,2,3,0,1,2,3 };
const int1 MERGE_TYPES[] = { SAO_MODE_MERGE_LEFT, SAO_MODE_MERGE_ABOVE };

_GENX_ inline 
void GetBestMode(
    int4 enableBandOffset,
    float lambda,
    vector_ref<int4, 16> diffEO, 
    vector_ref<int2, 16> countEO, 
    vector_ref<int4, 32> diffBO,
    vector_ref<int2, 32> countBO,
    vector_ref<int1, 16> bestParam,
    matrix_ref<int1,2,16> mergeParam,
    int2 mergeLeftAvail,
    int2 mergeAboveAvail)
{
    lambda *= 256;
    int2 mergeFlagsBits = mergeLeftAvail + mergeAboveAvail;

    // SAO_OFF
    bestParam = 0;
    bestParam.format<float>()[3] = lambda * (1 + mergeFlagsBits); // cost of SAO OFF
    vector_ref<float,1> minCost = bestParam.format<float>().select<1,1>(3);
    

    vector<uint2,16> offsetBits(OFFSET_BITS);

    vector<float,16> lambdaOffsetBits = lambda * offsetBits;
    vector_ref<float,8> lambdaEoOffsetBits = lambdaOffsetBits.select<8,1>(0);
    vector_ref<float,8> lambdaBoOffsetBits = lambdaOffsetBits.select<8,1>(8);

    const float lambdaEoBits = lambda * (2 + 2 + mergeFlagsBits);
    const float lambdaBoBits = lambda * (2 + 5 + mergeFlagsBits);
    
    // prepare signs: offset[0..1] >= 0 offset[2..3] <= 0
    matrix<int2,4,4> offsetSign = 1;
    offsetSign.format<int4>().select<4,2>(1) = -1;
    // EO
    {
        // prepare signs: offset[0..1] >= 0 offset[2..3] <= 0
        //matrix<int2,4,4> offsetSign = 1;
        //offsetSign.format<int4>().select<4,2>(1) = -1;

        // calculating dist = count * offset * offset - 2 * diff * offset for all possible offsets [-7..0] or [0..7]
        vector<int4,16> dist;
        vector<float,16> costs;
        vector<float,16> bestCost;
        vector<int2,16> offsets;
        diffEO *= offsetSign;

        dist = -(2*7) * diffEO;
        dist += (7*7) * countEO;
        bestCost = dist + lambdaEoOffsetBits[7];
        offsets = 7;

        #pragma unroll
        for (int i = 6; i >= 0; i--) {
            dist = -(2*i) * diffEO;
            dist += (i*i) * countEO;
            costs = dist + lambdaEoOffsetBits[i];
            vector<int2,16> mask = bestCost > costs;
            bestCost.merge(costs, mask);
            offsets.merge(i, mask);
        }

        // sum up 4 classes costs
        bestCost.select<8,1>(0) = bestCost.select<8,2>(0) + bestCost.select<8,2>(1);
        bestCost.select<4,1>(0) = bestCost.select<4,2>(0) + bestCost.select<4,2>(1);
        bestCost.select<4,1>(0) += lambdaEoBits;

        // choose best type
        vector<uint1,4> eoClasses(EO_CLASSES);
        MinHalf(bestCost.select<4,1>(), eoClasses.select<4,1>());
        MinHalf(bestCost.select<2,1>(), eoClasses.select<2,1>());

        // update best sao mode
        vector<int1,16> eoParams;
        eoParams[0] = SAO_MODE_ON;
        eoParams[1] = eoClasses[0];
        eoParams[2] = 0;
        eoParams.format<int2>().select<4,1>(2) = offsets.select<4,1>(4*eoClasses[0]) * offsetSign.row(0);
        eoParams.format<float>().select<1,1>(3) = bestCost[0];

        vector<int2,16> mask = bestCost[0] < minCost[0];
        bestParam.merge(eoParams, mask);

        // restore sign for merge mode
        diffEO *= offsetSign;
    }

    // BO
    if (enableBandOffset) {
        // remember signs of average offsets
        vector<int2,32> offsetSign = (diffBO >> 31).format<int2>().select<32,2>(0);
        offsetSign *= 2;
        offsetSign += 1;

        // calculating dist = count * offset * offset - 2 * diff * offset for all possible offsets [-7..0] or [0..7]
        vector<int4,32> dist;
        vector<float,32> cost;
        vector<int2,32> offsets = 7;
        vector<float,36> bestCost36;
        vector_ref<float,32> bestCost = bestCost36.select<32,1>();

        #pragma unroll
        for (int i = 7; i >= 0; i--) {
            dist = -(2*i)  * diffBO;
            dist *= offsetSign;
            dist += countBO * (i*i);
            if (i == 7) {
                bestCost = dist + lambdaBoOffsetBits[i];
            } else {
                cost = dist + lambdaBoOffsetBits[i];
                vector<int2,32> mask = bestCost > cost;
                bestCost.merge(cost, mask);
                offsets.merge(i, mask);
            }
        }

        // restore signs
        offsets *= offsetSign;

        // sum up all possible 4 consecutive bands (0..3, 1..4, 2..5, etc)
        vector<float,36> costSum;
        costSum.select<32,1>(0) = bestCost36.select<32,1>(0) + bestCost36.select<32,1>(1);
        costSum.select<32,1>(0) += bestCost36.select<32,1>(2);
        costSum.select<32,1>(0) += bestCost36.select<32,1>(3);
        costSum.select<3,1>(29) = FLT_MAX;

        vector<int2,32> band;
        cmtl::cm_vector_assign<int2,32>(band,0,1);

        // choose best start band
        MinHalf(costSum.select<32,1>(), band.select<32,1>());
        MinHalf(costSum.select<16,1>(), band.select<16,1>());
        MinHalf(costSum.select<8,1>(), band.select<8,1>());
        MinHalf(costSum.select<4,1>(), band.select<4,1>());
        MinHalf(costSum.select<2,1>(), band.select<2,1>());
        costSum[0] += lambdaBoBits;

        // update best sao mode
        vector<int1,16> boParams;
        boParams[0] = SAO_MODE_ON;
        boParams[1] = SAO_TYPE_BO;
        boParams[2] = band[0];
        boParams.format<int2>().select<4,1>(2) = offsets.select<4,1>(band[0]);
        boParams.format<float>().select<1,1>(3) = costSum[0];

        vector<int2,16> mask = costSum[0] < minCost[0];
        bestParam.merge(boParams, mask);
    }
}


_GENX_ inline 
void GetBestMode_Chroma_v2(
    int4 enableBandOffset,
    float lambda,

    vector_ref<int4, 16> diffEO_U,
    vector_ref<int2, 16> countEO_U,
    vector_ref<int4, 16> diffEO_V,
    vector_ref<int2, 16> countEO_V,

    vector_ref<int4, 32> diffBO_U,
    vector_ref<int2, 32> countBO_U,
    vector_ref<int4, 32> diffBO_V,
    vector_ref<int2, 32> countBO_V,

    vector_ref<int1, 16> bestParamU,
    vector_ref<int1, 16> bestParamV,
    
    int2 mergeLeftAvail,
    int2 mergeAboveAvail)
{
    lambda *= 256;
    int2 mergeFlagsBits = 0;//mergeLeftAvail + mergeAboveAvail;

    // SAO_OFF
    bestParamU = 0;
    bestParamV = 0;
    bestParamU.format<float>()[3] = lambda * (1 + mergeFlagsBits); // cost of SAO OFF
    bestParamV.format<float>()[3] = 0;

    //vector_ref<float,1> minCost = bestParamU.format<float>().select<1,1>(3);
    vector<float,1> minCost = bestParamU.format<float>().select<1,1>(3);

    vector<uint2,16> offsetBits(OFFSET_BITS);

    vector<float,16> lambdaOffsetBits = lambda * offsetBits;
    vector_ref<float,8> lambdaEoOffsetBits = lambdaOffsetBits.select<8,1>(0);
    vector_ref<float,8> lambdaBoOffsetBits = lambdaOffsetBits.select<8,1>(8);

    const float lambdaEoBits = lambda * (2 + 2 + mergeFlagsBits);    //mode + type <- once for UV
    const float lambdaBoBits = lambda * (2 + /*5*/ + mergeFlagsBits);//mode        <- once for UV
    const float lambdaBoBits_startBandIdx = lambda * (5);            // for each U/V


#if 1
    // EO
    {
        // plane U
        vector<float,16> bestCostU;
        vector<int2,16> offsetsU;
        // prepare signs: offset[0..1] >= 0 offset[2..3] <= 0
        matrix<int2,4,4> offsetSign = 1;
        offsetSign.format<int4>().select<4,2>(1) = -1;
        {
            // calculating dist = count * offset * offset - 2 * diff * offset for all possible offsets [-7..0] or [0..7]
            vector<int4,16> dist;
            vector<float,16> costs;
            vector<float,16> bestCost;
            vector<int2,16> offsets;
            diffEO_U *= offsetSign; //modify diffEO_U !!!

            dist = -(2*7) * diffEO_U;
            dist += (7*7) * countEO_U;
            bestCost = dist + lambdaEoOffsetBits[7];
            offsets = 7;

            #pragma unroll
            for (int i = 6; i >= 0; i--) {
                dist = -(2*i) * diffEO_U;
                dist += (i*i) * countEO_U;
                costs = dist + lambdaEoOffsetBits[i];
                vector<int2,16> mask = bestCost > costs;
                bestCost.merge(costs, mask);
                offsets.merge(i, mask);
            }

            // sum up 4 classes costs
            bestCost.select<8,1>(0) = bestCost.select<8,2>(0) + bestCost.select<8,2>(1);
            bestCost.select<4,1>(0) = bestCost.select<4,2>(0) + bestCost.select<4,2>(1);
            //bestCost.select<4,1>(0) += lambdaEoBits;

            // set output U
            bestCostU = bestCost;
            offsetsU = offsets;
            
            diffEO_U *= offsetSign; //restore diffEO_U !!!
        }

        // plane V
        vector<float,16> bestCostV;
        vector<int2,16> offsetsV;
        {
            // calculating dist = count * offset * offset - 2 * diff * offset for all possible offsets [-7..0] or [0..7]
            vector<int4,16> dist;
            vector<float,16> costs;
            vector<float,16> bestCost;
            vector<int2,16> offsets;
            diffEO_V *= offsetSign; // modify diffEO_V !!!

            dist = -(2*7) * diffEO_V;
            dist += (7*7) * countEO_V;
            bestCost = dist + lambdaEoOffsetBits[7];
            offsets = 7;

            #pragma unroll
            for (int i = 6; i >= 0; i--) {
                dist = -(2*i) * diffEO_V;
                dist += (i*i) * countEO_V;
                costs = dist + lambdaEoOffsetBits[i];
                vector<int2,16> mask = bestCost > costs;
                bestCost.merge(costs, mask);
                offsets.merge(i, mask);
            }

            // sum up 4 classes costs
            bestCost.select<8,1>(0) = bestCost.select<8,2>(0) + bestCost.select<8,2>(1);
            bestCost.select<4,1>(0) = bestCost.select<4,2>(0) + bestCost.select<4,2>(1);
            //bestCost.select<4,1>(0) += lambdaEoBits;

            // set output U
            bestCostV = bestCost;
            offsetsV = offsets;
            diffEO_V *= offsetSign; // restore diffEO_V !!!
        }


        // choose best type
        vector<float,16> bestCost = bestCostU + bestCostV + lambdaEoBits;
        vector<uint1,4> eoClasses(EO_CLASSES);
        MinHalf(bestCost.select<4,1>(), eoClasses.select<4,1>());
        MinHalf(bestCost.select<2,1>(), eoClasses.select<2,1>());

        // update best sao mode
        vector<int1,16> eoParams;
        eoParams[0] = SAO_MODE_ON;
        eoParams[1] = eoClasses[0];
        eoParams[2] = 0;

        vector<int2,16> mask = bestCost[0] < minCost[0];
        minCost.merge(bestCost[0], mask[0]);
        eoParams.format<float>().select<1,1>(3) = minCost;

        // U
        eoParams.format<int2>().select<4,1>(2) = offsetsU.select<4,1>(4*eoClasses[0]) * offsetSign.row(0);
        bestParamU.merge(eoParams, mask);
        // V
        eoParams.format<int2>().select<4,1>(2) = offsetsV.select<4,1>(4*eoClasses[0]) * offsetSign.row(0);
        bestParamV.merge(eoParams, mask);
    }
#endif
    

#if 1
    // BO
    if (enableBandOffset) {

        // U
        vector<float,36> costSumU;
        vector<int2,32> offsetsU;
        vector<int2,32> startBandU;
        {
            // remember signs of average offsets
            vector<int2,32> offsetSign = (diffBO_U >> 31).format<int2>().select<32,2>(0);
            offsetSign *= 2;
            offsetSign += 1;

            // calculating dist = count * offset * offset - 2 * diff * offset for all possible offsets [-7..0] or [0..7]
            vector<int4,32> dist;
            vector<float,32> cost;
            vector<int2,32> offsets = 7;
            vector<float,36> bestCost36;
            vector_ref<float,32> bestCost = bestCost36.select<32,1>();

#pragma unroll
            for (int i = 7; i >= 0; i--) {
                dist = -(2*i)  * diffBO_U;
                dist *= offsetSign;
                dist += countBO_U * (i*i);
                if (i == 7) {
                    bestCost = dist + lambdaBoOffsetBits[i];
                } else {
                    cost = dist + lambdaBoOffsetBits[i];
                    vector<int2,32> mask = bestCost > cost;
                    bestCost.merge(cost, mask);
                    offsets.merge(i, mask);
                }
            }

            // restore signs
            offsets *= offsetSign;

            // sum up all possible 4 consecutive bands (0..3, 1..4, 2..5, etc)
            vector<float,36> costSum;
            costSum.select<32,1>(0) = bestCost36.select<32,1>(0) + bestCost36.select<32,1>(1);
            costSum.select<32,1>(0) += bestCost36.select<32,1>(2);
            costSum.select<32,1>(0) += bestCost36.select<32,1>(3);
            costSum.select<3,1>(29) = FLT_MAX;

            
            offsetsU = offsets;

            vector<int2,32> band;
            cmtl::cm_vector_assign<int2,32>(band,0,1);

            // choose best start band
            MinHalf(costSum.select<32,1>(), band.select<32,1>());
            MinHalf(costSum.select<16,1>(), band.select<16,1>());
            MinHalf(costSum.select<8,1>(), band.select<8,1>());
            MinHalf(costSum.select<4,1>(), band.select<4,1>());
            MinHalf(costSum.select<2,1>(), band.select<2,1>());

#if 0
            // patch
            if (costSum[0] == costSum[1] && band[0] > band[1] ) {
                band[0] = band[1];
            }
#endif


            costSumU = costSum;
            startBandU = band;
        }

        // V
        vector<float,36> costSumV;
        vector<int2,32> offsetsV;
        vector<int2,32> startBandV;
        {
            // remember signs of average offsets
            vector<int2,32> offsetSign = (diffBO_V >> 31).format<int2>().select<32,2>(0);
            offsetSign *= 2;
            offsetSign += 1;

            // calculating dist = count * offset * offset - 2 * diff * offset for all possible offsets [-7..0] or [0..7]
            vector<int4,32> dist;
            vector<float,32> cost;
            vector<int2,32> offsets = 7;
            vector<float,36> bestCost36;
            vector_ref<float,32> bestCost = bestCost36.select<32,1>();

#pragma unroll
            for (int i = 7; i >= 0; i--) {
                dist = -(2*i)  * diffBO_V;
                dist *= offsetSign;
                dist += countBO_V * (i*i);
                if (i == 7) {
                    bestCost = dist + lambdaBoOffsetBits[i];
                } else {
                    cost = dist + lambdaBoOffsetBits[i];
                    vector<int2,32> mask = bestCost > cost;
                    bestCost.merge(cost, mask);
                    offsets.merge(i, mask);
                }
            }

            // restore signs
            offsets *= offsetSign;

            // sum up all possible 4 consecutive bands (0..3, 1..4, 2..5, etc)
            vector<float,36> costSum;
            costSum.select<32,1>(0) = bestCost36.select<32,1>(0) + bestCost36.select<32,1>(1);
            costSum.select<32,1>(0) += bestCost36.select<32,1>(2);
            costSum.select<32,1>(0) += bestCost36.select<32,1>(3);
            costSum.select<3,1>(29) = FLT_MAX;

            
            offsetsV = offsets;

            vector<int2,32> band;
            cmtl::cm_vector_assign<int2,32>(band,0,1);

            // choose best start band
            MinHalf(costSum.select<32,1>(), band.select<32,1>());
            MinHalf(costSum.select<16,1>(), band.select<16,1>());
            MinHalf(costSum.select<8,1>(), band.select<8,1>());
            MinHalf(costSum.select<4,1>(), band.select<4,1>());
            MinHalf(costSum.select<2,1>(), band.select<2,1>());

#if 0
            // patch
            if (costSum[0] == costSum[1] && band[0] > band[1] ) {
                band[0] = band[1];
            }
#endif

            startBandV = band;
            costSumV = costSum;
        }

        vector<float,36> costSum = costSumU + costSumV;
        //costSum[0] += (lambdaBoBits + lambdaBoBits);
        costSum[0] += (lambdaBoBits + lambdaBoBits_startBandIdx + lambdaBoBits_startBandIdx);

        // update best sao mode
        vector<int1,16> boParams;
        boParams[0] = SAO_MODE_ON;
        boParams[1] = SAO_TYPE_BO;

        vector<int2,16> mask = costSum[0] < minCost[0]; 
        minCost.merge(costSum[0], mask[0]);

        boParams.format<float>().select<1,1>(3) = minCost;

        // U
        boParams[2] = startBandU[0];
        boParams.format<int2>().select<4,1>(2) = offsetsU.select<4,1>(startBandU[0]);
        bestParamU.merge(boParams, mask);

        // V
        boParams[2] = startBandV[0];
        boParams.format<int2>().select<4,1>(2) = offsetsV.select<4,1>(startBandV[0]);
        bestParamV.merge(boParams, mask);
    }
#endif
}

template<uint W> _GENX_ inline void AddOffset(vector_ref<uint1,W> recon, vector_ref<int2,4> offsets, vector_ref<int2,W> cls)
{
    vector<int2,W> offset;
    offset.merge(offsets[0], 0, cls == -2);
    offset.merge(offsets[1], cls == -1);
    offset.merge(offsets[2], cls == 1);
    offset.merge(offsets[3], cls == 2);
    recon = cm_add<uint1>(recon, offset, SAT);
}

inline _GENX_ void SaoApplyImpl(SurfaceIndex SRC, SurfaceIndex DST, MiniVideoParam &par, vector_ref<int1,16> saoMode, int2 ox, int2 oy)
{
    enum { W=32, H=8 };

    vector_ref<int2,4> offsets = saoMode.format<int2>().select<4,1>(2);

    vector<uint2,64> pely;
    cmtl::cm_vector_assign<uint2,64>(pely,0,1);
    pely += oy*64;
    vector<uint2,64> maskVer = (pely == 0) | (pely >= par.Height - 1);

    matrix<int2,2,W> sign;
    vector<int2,W> cls;
    vector_ref<int2,W> sign1 = sign.row(0);
    vector_ref<int2,W> sign2 = sign.row(1);

#if 0
    // no sao for chroma yet, just copy
    matrix<uint1,8,32> chroma;
    #pragma unroll
    for (int row = 0; row < 32; row += 8) {
        #pragma unroll
        for (int col = 0; col < 64; col += 32) {
            read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, chroma);
            write_plane(DST, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, chroma);
        }
    }
#endif

    if (saoMode[0] == SAO_MODE_OFF) {

        matrix<uint1,8,32> luma;
        #pragma unroll
        for (int row = 0; row < 64; row += 8) {
            #pragma unroll
            for (int col = 0; col < 64; col += 32) {
                read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, luma);
                write_plane(DST, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, luma);
            }
        }

    } else if (saoMode[1] == SAO_TYPE_EO_90) {

        for (int row = 0; row < 64; row += H) {
            #pragma unroll
            for (int col = 0; col < 64; col += W) {
                matrix<uint1,H+2,W> recon;
                read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row-1,   recon.select<H,1,W,1>(0,0));
                read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row-1+H, recon.select<2,1,W,1>(H,0));

                GetSign(recon.row(0), recon.row(1), sign.row(1));
                #pragma unroll
                for (int y = 0; y < H; y++) {
                    GetSign(recon.row(y+1), recon.row(y+2), sign.row(y&1));
                    cls = sign.row(y&1) - sign.row(1-(y&1));
                    cls.merge(0, vector<uint2,W>(maskVer[row + y]));
                    AddOffset<W>(recon.row(y+1), offsets, cls);
                }

                write_plane(DST, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, recon.select<H,1,W,1>(1,0));
            }
        }

    } else if (saoMode[1] == SAO_TYPE_BO) {

        vector<uint1,4> band;
        band.format<uint4>()[0] = 0x03020100;
        band += saoMode[2];
        band &= 31;

        for (int row = 0; row < 64; row += H) {
            #pragma unroll
            for (int col = 0; col < 64; col += W) {
                matrix<uint1,H,W> recon;
                read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, recon);

                matrix<uint1,H,W> reconShifted = recon >> 3;

                #pragma unroll
                for (int y = 0; y < H; y++) {
                    vector<int2,W> off;
                    off.merge(offsets[0], 0, reconShifted.row(y) == band[0]);
                    off.merge(offsets[1], reconShifted.row(y) == band[1]);
                    off.merge(offsets[2], reconShifted.row(y) == band[2]);
                    off.merge(offsets[3], reconShifted.row(y) == band[3]);
                    recon.row(y) = cm_add<uint1>(recon.row(y), off, SAT);
                }

                write_plane(DST, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, recon);
            }
        }

    } else {
        
        vector<uint2,64> pelx;
        cmtl::cm_vector_assign<uint2,64>(pelx,0,1);
        pelx += ox*64;
        vector<uint2,64> maskHor = (pelx == 0) | (pelx >= par.Width - 1);

        if (saoMode[1] == SAO_TYPE_EO_0) {

            for (int row = 0; row < 64; row += H) {
                #pragma unroll
                for (int col = 0; col < 64; col += W) {
                    matrix<uint1,H,W> center;
                    matrix<uint1,H,W> left;
                    matrix<uint1,H,W> right;
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col,   oy*64+row, center);
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col-1, oy*64+row, left);
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col+1, oy*64+row, right);

                    #pragma unroll
                    for (int y = 0; y < H; y++) {
                        GetSign(center.row(y), left.row(y), sign1);
                        GetSign(center.row(y), right.row(y), sign2);
                        cls = sign1 + sign2;
                        cls.merge(0, maskHor.select<W,1>(col));
                        AddOffset<W>(center.row(y), offsets, cls);
                    }

                    write_plane(DST, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, center);
                }
            }

        } else if (saoMode[1] == SAO_TYPE_EO_135) {

            for (int row = 0; row < 64; row += H) {
                #pragma unroll
                for (int col = 0; col < 64; col += W) {
                    matrix<uint1,H,W> center;
                    matrix<uint1,H,W> leftUp;
                    matrix<uint1,H,W> rightDown;
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col,   oy*64+row,   center);
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col-1, oy*64+row-1, leftUp);
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col+1, oy*64+row+1, rightDown);

                    #pragma unroll
                    for (int y = 0; y < H; y++) {
                        GetSign(center.row(y), leftUp.row(y), sign1);
                        GetSign(center.row(y), rightDown.row(y), sign2);
                        cls = sign1 + sign2;
                        cls.merge(0, vector<uint2,W>(maskVer[row + y]));
                        cls.merge(0, maskHor.select<W,1>(col));
                        AddOffset<W>(center.row(y), offsets, cls);
                    }

                    write_plane(DST, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, center);
                }
            }

        } else if (saoMode[1] == SAO_TYPE_EO_45) {

            for (int row = 0; row < 64; row += H) {
                #pragma unroll
                for (int col = 0; col < 64; col += W) {
                    matrix<uint1,H,W> center;
                    matrix<uint1,H,W> leftDown;
                    matrix<uint1,H,W> rightUp;
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col,   oy*64+row,   center);
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col-1, oy*64+row+1, leftDown);
                    read_plane(SRC, GENX_SURFACE_Y_PLANE, ox*64+col+1, oy*64+row-1, rightUp);

                    #pragma unroll
                    for (int y = 0; y < H; y++) {
                        GetSign(center.row(y), leftDown.row(y), sign1);
                        GetSign(center.row(y), rightUp.row(y), sign2);
                        cls = sign1 + sign2;
                        cls.merge(0, vector<uint2,W>(maskVer[row + y]));
                        cls.merge(0, maskHor.select<W,1>(col));
                        AddOffset<W>(center.row(y), offsets, cls);
                    }

                    write_plane(DST, GENX_SURFACE_Y_PLANE, ox*64+col, oy*64+row, center);
                }
            }
        }
    }
}

// ctb based processing
inline _GENX_ void SaoApplyImpl_Chroma(SurfaceIndex SRC, SurfaceIndex DST, MiniVideoParam &par, vector_ref<int1,16> saoMode, vector_ref<int1,16> saoModeV, int2 ox, int2 oy)
{
    enum { W=32, H=8 };

    vector_ref<int2,4> offsetsU = saoMode.format<int2>().select<4,1>(2);
    vector_ref<int2,4> offsetsV = saoModeV.format<int2>().select<4,1>(2);

    vector<uint2,32> pely;
    cmtl::cm_vector_assign<uint2,32>(pely,0,1);
    pely += oy*32;
    vector<uint2,32> maskVer = (pely == 0) | (pely >= par.Height/2 - 1);

    matrix<int2,2,W> sign;
    vector<int2,W> cls;
    vector_ref<int2,W> sign1 = sign.row(0);
    vector_ref<int2,W> sign2 = sign.row(1);

    if (saoMode[0] == SAO_MODE_OFF) {

        matrix<uint1,H,W> chroma;
        #pragma unroll
        for (int row = 0; row < 32; row += H) {
            #pragma unroll
            for (int col = 0; col < 64; col += W) {
                read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, chroma);
                write_plane(DST, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, chroma);
            }
        }

    } else if (saoMode[1] == SAO_TYPE_EO_90) {

        for (int row = 0; row < 32; row += H) {
            #pragma unroll
            for (int col = 0; col < 64; col += W) {
                matrix<uint1,H+2,W> recon;
                read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row-1,   recon.select<H,1,W,1>(0,0));
                read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row-1+H, recon.select<2,1,W,1>(H,0));

                GetSign(recon.row(0), recon.row(1), sign.row(1));
                #pragma unroll
                for (int y = 0; y < H; y++) {
                    GetSign(recon.row(y+1), recon.row(y+2), sign.row(y&1));
                    cls = sign.row(y&1) - sign.row(1-(y&1));

                    cls.merge(0, vector<uint2,W>(maskVer[row + y])); // ???

                    // U
                    AddOffset<W/2>(recon.row(y+1).select<W/2,2>(0), offsetsU, cls.select<W/2,2>(0));
                    // V
                    AddOffset<W/2>(recon.row(y+1).select<W/2,2>(1), offsetsV, cls.select<W/2,2>(1));
                }

                write_plane(DST, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, recon.select<H,1,W,1>(1,0));
            }
        }
    }
#if 1
    else if (saoMode[1] == SAO_TYPE_BO) {

        vector<uint1,4> bandU;
        bandU.format<uint4>()[0] = 0x03020100;
        bandU += saoMode[2];
        bandU &= 31;

        vector<uint1,4> bandV;
        bandV.format<uint4>()[0] = 0x03020100;
        bandV += saoModeV[2];
        bandV &= 31;

        for (int row = 0; row < 32; row += H) {
            #pragma unroll
            for (int col = 0; col < 64; col += W) {
                matrix<uint1,H,W> recon;
                read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, recon);

                matrix<uint1,H,W> reconShifted = recon >> 3;

                #pragma unroll
                for (int y = 0; y < H; y++) {
                    vector<int2,W> off;

                    // U
                    off.select<W/2, 2>(0).merge(offsetsU[0], 0, reconShifted.row(y).select<W/2, 2>(0) == bandU[0]);
                    off.select<W/2, 2>(0).merge(offsetsU[1],    reconShifted.row(y).select<W/2, 2>(0) == bandU[1]);
                    off.select<W/2, 2>(0).merge(offsetsU[2],    reconShifted.row(y).select<W/2, 2>(0) == bandU[2]);
                    off.select<W/2, 2>(0).merge(offsetsU[3],    reconShifted.row(y).select<W/2, 2>(0) == bandU[3]);

                    // V
                    off.select<W/2, 2>(1).merge(offsetsV[0], 0, reconShifted.row(y).select<W/2, 2>(1) == bandV[0]);
                    off.select<W/2, 2>(1).merge(offsetsV[1],    reconShifted.row(y).select<W/2, 2>(1) == bandV[1]);
                    off.select<W/2, 2>(1).merge(offsetsV[2],    reconShifted.row(y).select<W/2, 2>(1) == bandV[2]);
                    off.select<W/2, 2>(1).merge(offsetsV[3],    reconShifted.row(y).select<W/2, 2>(1) == bandV[3]);

                    recon.row(y) = cm_add<uint1>(recon.row(y), off, SAT);
                }

                write_plane(DST, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, recon);
            }
        }
    } 
#endif
    else {
        
        vector<uint2,64> pelx;
        cmtl::cm_vector_assign<uint2,64>(pelx,0,1);
        pelx += ox*64;
        pelx >>= 1;
        vector<uint2,64> maskHor = (pelx == 0) | (pelx >= par.Width/2 - 1);

        if (saoMode[1] == SAO_TYPE_EO_0) {

            for (int row = 0; row < 32; row += H) {
                #pragma unroll
                for (int col = 0; col < 64; col += W) {
                    matrix<uint1,H,W> center;
                    matrix<uint1,H,W> left;
                    matrix<uint1,H,W> right;
                    read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col,   oy*32+row, center);
                    read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col-2, oy*32+row, left);
                    read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col+2, oy*32+row, right);

                    #pragma unroll
                    for (int y = 0; y < H; y++) {
                        GetSign(center.row(y), left.row(y), sign1);
                        GetSign(center.row(y), right.row(y), sign2);
                        cls = sign1 + sign2;
                        cls.merge(0, maskHor.select<W,1>(col));

                        AddOffset<W/2>(center.row(y).select<W/2,2>(0), offsetsU, cls.select<W/2,2>(0));
                        AddOffset<W/2>(center.row(y).select<W/2,2>(1), offsetsV, cls.select<W/2,2>(1));
                    }

                    write_plane(DST, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, center);
                }
            }

        } else if (saoMode[1] == SAO_TYPE_EO_135) {

            for (int row = 0; row < 32; row += H) {
                #pragma unroll
                for (int col = 0; col < 64; col += W) {
                    matrix<uint1,H,W> center;
                    matrix<uint1,H,W> leftUp;
                    matrix<uint1,H,W> rightDown;
                    read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col,   oy*32+row,   center);
                    read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col-2, oy*32+row-1, leftUp);
                    read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col+2, oy*32+row+1, rightDown);

                    #pragma unroll
                    for (int y = 0; y < H; y++) {
                        GetSign(center.row(y), leftUp.row(y), sign1);
                        GetSign(center.row(y), rightDown.row(y), sign2);
                        cls = sign1 + sign2;
                        cls.merge(0, vector<uint2,W>(maskVer[row + y]));
                        cls.merge(0, maskHor.select<W,1>(col));

                        AddOffset<W/2>(center.row(y).select<W/2,2>(0), offsetsU, cls.select<W/2,2>(0));
                        AddOffset<W/2>(center.row(y).select<W/2,2>(1), offsetsV, cls.select<W/2,2>(1));
                    }

                    write_plane(DST, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, center);
                }
            }

        } else if (saoMode[1] == SAO_TYPE_EO_45) {

            for (int row = 0; row < 32; row += H) {
                #pragma unroll
                for (int col = 0; col < 64; col += W) {
                    matrix<uint1,H,W> center;
                    matrix<uint1,H,W> leftDown;
                    matrix<uint1,H,W> rightUp;
                    read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col,   oy*32+row,   center);
                    read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col-2, oy*32+row+1, leftDown);
                    read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col+2, oy*32+row-1, rightUp);

                    #pragma unroll
                    for (int y = 0; y < H; y++) {
                        GetSign(center.row(y), leftDown.row(y), sign1);
                        GetSign(center.row(y), rightUp.row(y), sign2);
                        cls = sign1 + sign2;
                        cls.merge(0, vector<uint2,W>(maskVer[row + y]));
                        cls.merge(0, maskHor.select<W,1>(col));

                        AddOffset<W/2>(center.row(y).select<W/2,2>(0), offsetsU, cls.select<W/2,2>(0));
                        AddOffset<W/2>(center.row(y).select<W/2,2>(1), offsetsV, cls.select<W/2,2>(1));
                    }

                    write_plane(DST, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, center);
                }
            }
        }

    }
}
extern "C" _GENX_MAIN_ void SaoStat(SurfaceIndex SRC, SurfaceIndex RECON, SurfaceIndex PARAM, SurfaceIndex STATS)
{
    MiniVideoParam par;
    ReadParam(PARAM, par);  
       
    int2 ox = get_thread_origin_x();
    int2 oy = get_thread_origin_y();
    int4 offset = 0;
    int4 enableBandOffset = par.enableBandOffset & 0x01;

    // Luma
    {
        enum { W=W_STAT, H=H_STAT };
        int4 block = oy * par.PicWidthInCtbs * (64 / W) + ox;
        ox *= W;
        oy *= H;

        matrix<uint1,H,W> origin;
        matrix<uint1,H+2,W> recon;
        matrix<uint1,H+2,W> reconL;
        matrix<uint1,H+2,W> reconR;

        read_plane(SRC, GENX_SURFACE_Y_PLANE, ox, oy, origin);

        if (W*(H+2) <= 256) {
            read_plane(RECON, GENX_SURFACE_Y_PLANE, ox,   oy-1, recon);
            read_plane(RECON, GENX_SURFACE_Y_PLANE, ox-1, oy-1, reconL);
            read_plane(RECON, GENX_SURFACE_Y_PLANE, ox+1, oy-1, reconR);
        } else {
            read_plane(RECON, GENX_SURFACE_Y_PLANE, ox,   oy-1, recon.select<H,1,W,1>(0,0));
            read_plane(RECON, GENX_SURFACE_Y_PLANE, ox,   oy-1+H, recon.select<2,1,W,1>(H,0));
            read_plane(RECON, GENX_SURFACE_Y_PLANE, ox-1, oy-1, reconL.select<H,1,W,1>(0,0));
            read_plane(RECON, GENX_SURFACE_Y_PLANE, ox-1, oy-1+H, reconL.select<2,1,W,1>(H,0));
            read_plane(RECON, GENX_SURFACE_Y_PLANE, ox+1, oy-1, reconR.select<H,1,W,1>(0,0));
            read_plane(RECON, GENX_SURFACE_Y_PLANE, ox+1, oy-1+H, reconR.select<2,1,W,1>(H,0));
        }

        matrix<int2,H,W> diff = origin - recon.select<H,1,W,1>(1,0);

        vector<int2,H> pely;
        cmtl::cm_vector_assign<int2,H>(pely,0,1);
        pely += oy;
        vector<uint2,H> maskVer0 = (pely >= par.Height);
        vector<uint2,H> maskVer1 = (pely == 0) | (pely >= par.Height - 1);

        vector<int2,W> pelx;
        cmtl::cm_vector_assign<int2,W>(pelx,0,1);
        pelx += ox;
        vector<uint2,W> maskHor0 = (pelx >= par.Width);
        vector<uint2,W> maskHor1 = (pelx == 0) | (pelx >= par.Width - 1);

        matrix<int2,2,16> resultEO;
        GetEOStat<W,H>(recon, reconL, reconR, diff, resultEO.row(0), resultEO.row(1), maskHor0, maskHor1, maskVer0, maskVer1, resultEO.row(0), resultEO.row(1)); // last ones are fake for luma
        write(STATS, (16+16+32+32)*2*block, resultEO.format<int2>());

        if (enableBandOffset) {
            matrix<int2,2,32> resultBO;
            GetBOStat<W,H>(recon.select<H,1,W,1>(1,0), diff, resultBO.row(0), resultBO.row(1), ox, oy, par.Width, par.Height);
            write(STATS, (16+16+32+32)*2*block+(16+16)*2, resultBO.format<int2>());
        }
        // track offset
        int4 tsWidth   = par.PicWidthInCtbs *  (par.MaxCUSize / W);
        int4 tsHeight  = par.PicHeightInCtbs * (par.MaxCUSize / H);
        int4 numBlocks = tsWidth * tsHeight;
        offset = (16+16+32+32)*2*numBlocks;
    }

#if 0
    // Chroma
    if (par.SAOChromaFlag) 
    {
        enum { W=W_STAT_CHROMA*2, H=H_STAT_CHROMA }; // W=W_STAT_CHROMA*2 - due to interleave UV processing
        ox = get_thread_origin_x();
        oy = get_thread_origin_y();
        int4 block = oy * par.PicWidthInCtbs * (64 / W) + ox;

        ox *= W;
        oy *= H;

        matrix<uint1,H,W> origin;
        matrix<uint1,H+2,W> recon;
        matrix<uint1,H+2,W> reconL;
        matrix<uint1,H+2,W> reconR;

        read_plane(SRC, GENX_SURFACE_UV_PLANE, ox, oy, origin);

        if (W*(H+2) <= 256) {
            read_plane(RECON, GENX_SURFACE_UV_PLANE, ox,   oy-1, recon);
            read_plane(RECON, GENX_SURFACE_UV_PLANE, ox-2, oy-1, reconL);
            read_plane(RECON, GENX_SURFACE_UV_PLANE, ox+2, oy-1, reconR);
        } else {
            read_plane(RECON, GENX_SURFACE_UV_PLANE, ox,   oy-1,   recon.select<H,1,W,1>(0,0));
            read_plane(RECON, GENX_SURFACE_UV_PLANE, ox,   oy-1+H, recon.select<2,1,W,1>(H,0));

            read_plane(RECON, GENX_SURFACE_UV_PLANE, ox-2, oy-1,   reconL.select<H,1,W,1>(0,0));
            read_plane(RECON, GENX_SURFACE_UV_PLANE, ox-2, oy-1+H, reconL.select<2,1,W,1>(H,0));

            read_plane(RECON, GENX_SURFACE_UV_PLANE, ox+2, oy-1,   reconR.select<H,1,W,1>(0,0));
            read_plane(RECON, GENX_SURFACE_UV_PLANE, ox+2, oy-1+H, reconR.select<2,1,W,1>(H,0));
        }

        matrix<int2,H,W> diff = origin - recon.select<H,1,W,1>(1,0);

        vector<int2,H> pely;
#if 1
        cmtl::cm_vector_assign<int2,H>(pely,0,1);
#else
#pragma unroll
        for (int1 pos = 0; pos < H; pos++) {
            pely[pos] = pos;
        }
#endif
        pely += oy;
        vector<uint2,H> maskVer0 = (pely >= par.Height/2);
        vector<uint2,H> maskVer1 = (pely == 0) | (pely >= par.Height/2 - 1);

        vector<int2,W> pelx;
#if 1
        cmtl::cm_vector_assign<int2,W>(pelx,0,1);
#else
#pragma unroll
        for (int1 pos = 0; pos < W; pos++) {
            pelx[pos] = pos;
        }
#endif
        pelx += ox;
        pelx >>= 1;
        vector<uint2,W> maskHor0 = (pelx >= par.Width/2);
        vector<uint2,W> maskHor1 = (pelx == 0) | (pelx >= par.Width/2 - 1);

        matrix<int2,2,16> resultEO_Chr0;
        matrix<int2,2,16> resultEO_Chr1;
        uint1 enableChroma = 1;
        GetEOStat<W,H>(recon, reconL, reconR, diff, resultEO_Chr0.row(0), resultEO_Chr0.row(1), maskHor0, maskHor1, maskVer0, maskVer1, resultEO_Chr1.row(0), resultEO_Chr1.row(1), enableChroma);

        {
#if 0
            printf("\n on gfx write blk(%i): ", block);
            for (int pos = 0; pos < 16; pos++) {
                printf("%i ", resultEO_Chr0[0][pos]);
            }
#endif
            write(STATS, offset + (16+16+32+32)*2*block, resultEO_Chr0.format<int2>());
            write(STATS, (offset<<1) + (16+16+32+32)*2*block, resultEO_Chr1.format<int2>());
        }

        if (enableBandOffset) {
            matrix<int2,2*2,32> resultBO;
            GetBOStat_Chroma<W,H>(recon.select<H,1,W,1>(1,0), diff, resultBO.row(0), resultBO.row(1), resultBO.row(2), resultBO.row(3), ox, oy, par.Width, par.Height >> 1);
            write(STATS, offset + (16+16+32+32)*2*block + (16+16)*2, resultBO.format<int2>().select<2*32, 1>(0));
            write(STATS, (offset<<1) + (16+16+32+32)*2*block + (16+16)*2, resultBO.format<int2>().select<2*32, 1>(2*32));
        }
    }
#endif
}


extern "C" _GENX_MAIN_ void SaoStatChroma(SurfaceIndex SRC, SurfaceIndex RECON, SurfaceIndex PARAM, SurfaceIndex STATS)
{
    MiniVideoParam par;
    ReadParam(PARAM, par);

    int4 startOffset = par.offsetChroma;// in bytes

    enum { W=2*W_STAT_CHROMA, H=H_STAT_CHROMA };
       
    int2 ox = get_thread_origin_x();
    int2 oy = get_thread_origin_y();
    int4 block = oy * par.PicWidthInCtbs * (64 / W) + ox;
    ox *= W;
    oy *= H;

    matrix<uint1,H,W> origin;
    matrix<uint1,H+2,W> recon;
    matrix<uint1,H+2,W> reconL;
    matrix<uint1,H+2,W> reconR;

    read_plane(SRC, GENX_SURFACE_UV_PLANE, ox, oy, origin);

    if (W*(H+2) <= 256) {
        read_plane(RECON, GENX_SURFACE_UV_PLANE, ox,   oy-1, recon);
        read_plane(RECON, GENX_SURFACE_UV_PLANE, ox-2, oy-1, reconL);
        read_plane(RECON, GENX_SURFACE_UV_PLANE, ox+2, oy-1, reconR);
    } else {
        read_plane(RECON, GENX_SURFACE_UV_PLANE, ox,   oy-1,   recon.select<H,1,W,1>(0,0));
        read_plane(RECON, GENX_SURFACE_UV_PLANE, ox,   oy-1+H, recon.select<2,1,W,1>(H,0));

        read_plane(RECON, GENX_SURFACE_UV_PLANE, ox-2, oy-1,   reconL.select<H,1,W,1>(0,0));
        read_plane(RECON, GENX_SURFACE_UV_PLANE, ox-2, oy-1+H, reconL.select<2,1,W,1>(H,0));

        read_plane(RECON, GENX_SURFACE_UV_PLANE, ox+2, oy-1,   reconR.select<H,1,W,1>(0,0));
        read_plane(RECON, GENX_SURFACE_UV_PLANE, ox+2, oy-1+H, reconR.select<2,1,W,1>(H,0));
    }

    matrix<int2,H,W> diff = origin - recon.select<H,1,W,1>(1,0);

    vector<int2,H> pely;
    cmtl::cm_vector_assign<int2,H>(pely,0,1);
    pely += oy;
    vector<uint2,H> maskVer0 = (pely >= par.Height/2);
    vector<uint2,H> maskVer1 = (pely == 0) | (pely >= par.Height/2 - 1);

    vector<int2,W> pelx;
    cmtl::cm_vector_assign<int2,W>(pelx,0,1);
    pelx += ox;
    pelx >>= 1;
    vector<uint2,W> maskHor0 = (pelx >= par.Width/2);
    vector<uint2,W> maskHor1 = (pelx == 0) | (pelx >= par.Width/2 - 1);
    
    matrix<int2,2,16> resultEO_Chr0;
    matrix<int2,2,16> resultEO_Chr1;
    uint1 chromaSplit = 1;
    GetEOStat<W,H>(recon, reconL, reconR, diff, resultEO_Chr0.row(0), resultEO_Chr0.row(1), maskHor0, maskHor1, maskVer0, maskVer1, resultEO_Chr1.row(0), resultEO_Chr1.row(1), chromaSplit);

    int4 offset;
    {
        write(STATS, startOffset + (16+16+32+32)*2*block, resultEO_Chr0.format<int2>());
        int4 tsWidth   = par.PicWidthInCtbs *  (par.MaxCUSize/ W);//interleaved U/V
        int4 tsHeight  = par.PicHeightInCtbs * ((par.MaxCUSize >> 1) / H);
        /*mfxU32 numThreads = */int4 numBlocks = tsWidth * tsHeight;
        offset = (16+16+32+32)*2*numBlocks;
        write(STATS, startOffset + offset + (16+16+32+32)*2*block, resultEO_Chr1.format<int2>());
    }

    if (par.enableBandOffset & 0x01) {
        matrix<int2,2*2,32> resultBO(0);
        GetBOStat_Chroma<W,H>(recon.select<H,1,W,1>(1,0), diff, resultBO.row(0), resultBO.row(1), resultBO.row(2), resultBO.row(3), ox, oy, par.Width, par.Height >> 1);
        write(STATS, startOffset + (16+16+32+32)*2*block + (16+16)*2, resultBO.format<int2>().select<2*32, 1>(0));
        write(STATS, startOffset + offset + (16+16+32+32)*2*block + (16+16)*2, resultBO.format<int2>().select<2*32, 1>(2*32));
    }
}


extern "C" _GENX_MAIN_ void SaoEstimate(SurfaceIndex PARAM, SurfaceIndex STATS, SurfaceIndex SAO_MODES)
{
    MiniVideoParam par;
    ReadParam(PARAM, par);

    int2 ox = get_thread_origin_x();
    int2 oy = get_thread_origin_y();
    int4 ctb = oy * par.PicWidthInCtbs + ox;

    // Luma
    vector<int4,16> diffEO = 0;
    vector<int2,16> countEO = 0;
    vector<int4,32> diffBO = 0;
    vector<int2,32> countBO = 0;
    {
        enum { W=W_STAT, H=H_STAT };
#pragma unroll
        for (int y = 0; y < 64/H; y++) {
#pragma unroll
            for (int x = 0; x < 64/W; x++) {
                int4 blk = (oy*(64/H) + y) * par.PicWidthInCtbs * (64/W) + ox*(64/W) + x;
                matrix<int2,2,16> tmpEO;
                matrix<int2,2,32> tmpBO;
                read(STATS, (16+16+32+32)*2*blk, tmpEO.format<int2>());
                read(STATS, (16+16+32+32)*2*blk+16*2*2, tmpBO.format<int2>());
                diffEO += tmpEO.row(0);
                diffBO += tmpBO.row(0);
                countEO += tmpEO.row(1);
                countBO += tmpBO.row(1);
            }
        }
    }

    // Chroma
    vector<int4,16> diffEO_U = 0;
    vector<int2,16> countEO_U = 0;
    vector<int4,16> diffEO_V = 0;
    vector<int2,16> countEO_V = 0;

    vector<int4,32> diffBO_U = 0;
    vector<int2,32> countBO_U = 0;
    vector<int4,32> diffBO_V = 0;
    vector<int2,32> countBO_V = 0;
    if (par.SAOChromaFlag)
    {
        enum { W=W_STAT_CHROMA, H=H_STAT_CHROMA }; // you should use 2*W in case of interleave UV processing but U & V stats placed separately now

        int4 offsetLuma = par.offsetChroma;//(16+16+32+32)*2*numBlocksLuma;

        const int4 numBlocksChroma = par.PicWidthInCtbs *  (32 / W) * par.PicHeightInCtbs * (32 / H);
        const int4 offsetChroma = (16+16+32+32)*2*numBlocksChroma;
#pragma unroll
        for (int y = 0; y < (32 / H); y++) {
#pragma unroll
            for (int x = 0; x < (32 / W); x++) {
                int4 blk = (oy*(32 / H) + y) * par.PicWidthInCtbs * (32 / W) + ox*(32 / W) + x;

                matrix<int2,2,16> tmpEO;
                matrix<int2,2,32> tmpBO;
                read(STATS, offsetLuma + (16+16+32+32)*2*blk, tmpEO.format<int2>());
                read(STATS, offsetLuma + (16+16+32+32)*2*blk+16*2*2, tmpBO.format<int2>());
                diffEO_U += tmpEO.row(0);
                diffBO_U += tmpBO.row(0);
                countEO_U += tmpEO.row(1);
                countBO_U += tmpBO.row(1);

                read(STATS, offsetLuma + offsetChroma + (16+16+32+32)*2*blk, tmpEO.format<int2>());
                read(STATS, offsetLuma + offsetChroma + (16+16+32+32)*2*blk+16*2*2, tmpBO.format<int2>());
                diffEO_V += tmpEO.row(0);
                diffBO_V += tmpBO.row(0);
                countEO_V += tmpEO.row(1);
                countBO_V += tmpBO.row(1);
            }
        }
    }


    cm_wait();

    
    matrix<int1,2,16> saoMergeModes;
    const int2 enableMergeMode = 1;//par.enableBandOffset >> 1;
    read(SAO_MODES, GENX_MODIFIED, (ctb - 1) * 16, saoMergeModes.row(0));
    read(SAO_MODES, GENX_MODIFIED, (ctb - par.PicWidthInCtbs) * 16, saoMergeModes.row(1));

    int2 leftAvail;// = ox > 0;
    int2 aboveAvail;// = oy > 0;
    if (1)
    {
        vector<uint1,64> partX, partY;
        int2 addr16 = ((88*4 + ox) >> 4) << 4;
        int2 offset = ox & 15;
        read(PARAM, addr16, partX);
        leftAvail = partX[offset];
        const int2 maxNumCtbX = 3840 / 64;
        addr16 = ((88*4 + maxNumCtbX + oy) >> 4) << 4;
        offset = (88*4 + maxNumCtbX + oy) & 15;
        read(PARAM, addr16, partY);
        aboveAvail = partY[offset];
    }

    int4 numCtbs = par.PicWidthInCtbs * par.PicHeightInCtbs;

    vector<int1, 16> saoMode;
    int4 enableBandOffset = par.enableBandOffset & 0x1;
    GetBestMode(enableBandOffset, par.m_rdLambda, diffEO, countEO, diffBO, countBO, saoMode, saoMergeModes, leftAvail, aboveAvail);

    vector<int1, 16> saoModeU;
    vector<int1, 16> saoModeV;
    matrix<int1,2,16> saoMergeModesU;
    matrix<int1,2,16> saoMergeModesV;
    if (par.SAOChromaFlag) {
        GetBestMode_Chroma_v2(enableBandOffset, par.m_rdLambda, diffEO_U, countEO_U, diffEO_V, countEO_V, diffBO_U, countBO_U, diffBO_V, countBO_V, saoModeU, saoModeV, leftAvail, aboveAvail);

        if (enableMergeMode) {
            //if (leftAvail)
            read(SAO_MODES, GENX_MODIFIED,  1 * numCtbs * 16 + (ctb - 1) * 16, saoMergeModesU.row(0));
            //if (aboveAvail)
            read(SAO_MODES, GENX_MODIFIED,  1 * numCtbs * 16 + (ctb - par.PicWidthInCtbs) * 16, saoMergeModesU.row(1));
            //if (leftAvail)
            read(SAO_MODES, GENX_MODIFIED,  2 * numCtbs * 16 + (ctb - 1) * 16, saoMergeModesV.row(0));
            //if (aboveAvail)
            read(SAO_MODES, GENX_MODIFIED,  2 * numCtbs * 16 + (ctb - par.PicWidthInCtbs) * 16, saoMergeModesV.row(1));
        }
    }

    // SAO merge
    if (enableMergeMode)
    {
        int2 mergeLeftAvail = leftAvail;
        int2 mergeAboveAvail= aboveAvail;
        float lambda = par.m_rdLambda * 256;
        vector<uint2,8> idxInitSeq(IDX_INIT_SEQ);

        vector<float,1> minCost = saoMode.format<float>().select<1,1>(3);
        if (par.SAOChromaFlag) {
            minCost += saoModeU.format<float>().select<1,1>(3);
        }

        // total RD cost (to compare with minCost)
        vector<float,1> mergeLeftCost  = 0;
        vector<float,1> mergeAboveCost = 0;


        // FOR EACH PLANE:: Y
        matrix_ref<int1,2,16> mergeParam = saoMergeModes;
        vector_ref<int1,16> mergeLeftParam = mergeParam.row(0);
        vector_ref<int1,16> mergeAboveParam = mergeParam.row(1);
        {
            // costs for SAO merge
            // calculations in parallel: both left and above, both Edge Offset and Band Offset
            vector<int2,16> offsets;
            offsets.select<4,1>(0) = mergeLeftParam.format<int2>().select<4,1>(2);
            offsets.select<4,1>(4) = mergeAboveParam.format<int2>().select<4,1>(2);
            offsets.select<8,1>(8) = offsets.select<8,1>(0);

            //vector<uint2,8> idxInitSeq(IDX_INIT_SEQ);
            vector<uint2,2> eoType = mergeParam.column(1) * 4;
            vector<uint2,2> startBand = mergeParam.column(2);
            vector<uint2,8> eoIdx = idxInitSeq + eoType.replicate<2,1,4,0>();
            // patch for debug
            //vector<uint1, 8> debug_mask = eoIdx > 15;
            //eoIdx.merge(15, debug_mask);
            //
            vector<uint2,8> boIdx = idxInitSeq + startBand.replicate<2,1,4,0>();

            vector<int2,16> count;
            count.select<8,1>(0) = countEO.iselect(eoIdx);
            count.select<8,1>(8) = countBO.iselect(boIdx);

            vector<int4,16> diff;
            diff.select<8,1>(0) = diffEO.iselect(eoIdx);
            diff.select<8,1>(8) = diffBO.iselect(boIdx);

            // 16 distortions: 4 offsets for each of leftEO, aboveEO, leftBO, aboveBO
            vector<int4,16> dist;
            dist = count * offsets;
            dist -= 2 * diff;
            dist *= offsets;
            // sum up distortions for each 4 offsets
            dist.select<8,1>(0) = dist.select<8,2>(0) + dist.select<8,2>(1);
            dist.select<4,1>(0) = dist.select<4,2>(0) + dist.select<4,2>(1);

            // select between Edge and Band Offset
            vector<int2,2> maskBo = (mergeParam.column(1) == vector<int2,2>(SAO_TYPE_BO));
            dist.select<2,1>(0).merge(dist.select<2,1>(2), maskBo);

            // select between SAO Off and On
            vector<int2,2> maskOff = (!mergeParam.column(0));
            dist.select<2,1>(0).merge(0, maskOff);

            mergeLeftParam[0]  = SAO_MODE_MERGE_LEFT;
            mergeAboveParam[0] = SAO_MODE_MERGE_ABOVE;

            // total RD cost
            mergeLeftCost  += dist[0]/* + lambda*/;
            mergeAboveCost += dist[1]/* + lambda * (1 + mergeLeftAvail)*/;
        }

        // FOR EACH PLANE:: U
        matrix_ref<int1,2,16> mergeParamU = saoMergeModesU;
        vector_ref<int1,16> mergeLeftParamU = mergeParamU.row(0);
        vector_ref<int1,16> mergeAboveParamU = mergeParamU.row(1);
        if (par.SAOChromaFlag) 
        {
            // costs for SAO merge
            // calculations in parallel: both left and above, both Edge Offset and Band Offset
            vector<int2,16> offsets;
            offsets.select<4,1>(0) = mergeLeftParamU.format<int2>().select<4,1>(2);
            offsets.select<4,1>(4) = mergeAboveParamU.format<int2>().select<4,1>(2);
            offsets.select<8,1>(8) = offsets.select<8,1>(0);
            
            vector<uint2,2> eoType = mergeParamU.column(1) * 4;
            vector<uint2,2> startBand = mergeParamU.column(2);
            vector<uint2,8> eoIdx = idxInitSeq + eoType.replicate<2,1,4,0>();
            // patch for debug
            //vector<uint1, 8> debug_mask = eoIdx > 15;
            //eoIdx.merge(15, debug_mask);
            //
            vector<uint2,8> boIdx = idxInitSeq + startBand.replicate<2,1,4,0>();

            vector<int2,16> count;
            count.select<8,1>(0) = countEO_U.iselect(eoIdx);
            count.select<8,1>(8) = countBO_U.iselect(boIdx);

            vector<int4,16> diff;
            diff.select<8,1>(0) = diffEO_U.iselect(eoIdx);
            diff.select<8,1>(8) = diffBO_U.iselect(boIdx);

            // 16 distortions: 4 offsets for each of leftEO, aboveEO, leftBO, aboveBO
            vector<int4,16> dist;
            dist = count * offsets;
            dist -= 2 * diff;
            dist *= offsets;
            // sum up distortions for each 4 offsets
            dist.select<8,1>(0) = dist.select<8,2>(0) + dist.select<8,2>(1);
            dist.select<4,1>(0) = dist.select<4,2>(0) + dist.select<4,2>(1);

            // select between Edge and Band Offset
            vector<int2,2> maskBo = (mergeParamU.column(1) == vector<int2,2>(SAO_TYPE_BO));
            dist.select<2,1>(0).merge(dist.select<2,1>(2), maskBo);

            // select between SAO Off and On
            vector<int2,2> maskOff = (!mergeParamU.column(0));
            dist.select<2,1>(0).merge(0, maskOff);

            mergeLeftParamU[0]  = SAO_MODE_MERGE_LEFT;
            mergeAboveParamU[0] = SAO_MODE_MERGE_ABOVE;

            // total RD cost
            mergeLeftCost  += dist[0]/* + lambda*/;
            mergeAboveCost += dist[1]/* + lambda * (1 + mergeLeftAvail)*/;
        }

        // FOR EACH PLANE:: U
        matrix_ref<int1,2,16> mergeParamV = saoMergeModesV;
        vector_ref<int1,16> mergeLeftParamV = mergeParamV.row(0);
        vector_ref<int1,16> mergeAboveParamV = mergeParamV.row(1);
        if (par.SAOChromaFlag)
        {
            // costs for SAO merge
            // calculations in parallel: both left and above, both Edge Offset and Band Offset
            vector<int2,16> offsets;
            offsets.select<4,1>(0) = mergeLeftParamV.format<int2>().select<4,1>(2);
            offsets.select<4,1>(4) = mergeAboveParamV.format<int2>().select<4,1>(2);
            offsets.select<8,1>(8) = offsets.select<8,1>(0);
            
            vector<uint2,2> eoType = mergeParamV.column(1) * 4;
            vector<uint2,2> startBand = mergeParamV.column(2);
            vector<uint2,8> eoIdx = idxInitSeq + eoType.replicate<2,1,4,0>();
            // patch for debug
            //vector<uint1, 8> debug_mask = eoIdx > 15;
            //eoIdx.merge(15, debug_mask);
            //
            vector<uint2,8> boIdx = idxInitSeq + startBand.replicate<2,1,4,0>();

            vector<int2,16> count;
            count.select<8,1>(0) = countEO_V.iselect(eoIdx);
            count.select<8,1>(8) = countBO_V.iselect(boIdx);

            vector<int4,16> diff;
            diff.select<8,1>(0) = diffEO_V.iselect(eoIdx);
            diff.select<8,1>(8) = diffBO_V.iselect(boIdx);

            // 16 distortions: 4 offsets for each of leftEO, aboveEO, leftBO, aboveBO
            vector<int4,16> dist;
            dist = count * offsets;
            dist -= 2 * diff;
            dist *= offsets;
            // sum up distortions for each 4 offsets
            dist.select<8,1>(0) = dist.select<8,2>(0) + dist.select<8,2>(1);
            dist.select<4,1>(0) = dist.select<4,2>(0) + dist.select<4,2>(1);

            // select between Edge and Band Offset
            vector<int2,2> maskBo = (mergeParamV.column(1) == vector<int2,2>(SAO_TYPE_BO));
            dist.select<2,1>(0).merge(dist.select<2,1>(2), maskBo);

            // select between SAO Off and On
            vector<int2,2> maskOff = (!mergeParamV.column(0));
            dist.select<2,1>(0).merge(0, maskOff);

            mergeLeftParamV[0]  = SAO_MODE_MERGE_LEFT;
            mergeAboveParamV[0] = SAO_MODE_MERGE_ABOVE;

            // total RD cost
            mergeLeftCost  += dist[0]/* + lambda*/;
            mergeAboveCost += dist[1]/* + lambda * (1 + mergeLeftAvail)*/;
        }

        mergeLeftCost  += /*dist[0] + */lambda;
        mergeAboveCost += /*dist[1] + */lambda * (1 + mergeLeftAvail);

        // ------------------------------------------------
        // MODE DECISION
        // ------------------------------------------------

        // SAO MERGE LEFT
        vector<int2,1> better;
        better = (mergeLeftCost[0] < minCost[0]);
        better = better[0] & mergeLeftAvail;
        saoMode.merge(mergeLeftParam, better.replicate<16>());
        if (par.SAOChromaFlag) {
            saoModeU.merge(mergeLeftParamU, better.replicate<16>());
            saoModeV.merge(mergeLeftParamV, better.replicate<16>());
        }
        minCost.merge(mergeLeftCost, better);

        // SAO MERGE ABOVE
        better = (mergeAboveCost[0] < minCost[0]);
        better[0] = better[0] & mergeAboveAvail;
        saoMode.merge(mergeAboveParam, better.replicate<16>());
        if (par.SAOChromaFlag) {
            saoModeU.merge(mergeAboveParamU, better.replicate<16>());
            saoModeV.merge(mergeAboveParamV, better.replicate<16>());
        }
        minCost.merge(mergeAboveCost, better);
    }

    //Luma
    write(SAO_MODES, ctb * 16, saoMode);

    if (par.SAOChromaFlag) {
        //if (ctb == 1) {
        //    printf("\n mode_idx %i type_idx %i \n", saoModeU[0], saoModeU[1]);
        //}
        //saoModeU[0] = 7;// debug
        write(SAO_MODES, 1 * numCtbs * 16 + ctb * 16, saoModeU);
        write(SAO_MODES, 2 * numCtbs * 16 + ctb * 16, saoModeV);
    }

    cm_fence();
}


extern "C" _GENX_MAIN_ void SaoApply(SurfaceIndex SRC, SurfaceIndex DST, SurfaceIndex PARAM, SurfaceIndex SAO_MODES)
{
    MiniVideoParam par;
    ReadParam(PARAM, par);  
       
    int2 ox = get_thread_origin_x();
    int2 oy = get_thread_origin_y();
    int4 ctb = oy * par.PicWidthInCtbs + ox;

    vector<int1,16> saoMode;
    read(SAO_MODES, ctb * 16, saoMode);

    SaoApplyImpl(SRC, DST, par, saoMode, ox, oy);
    if (par.SAOChromaFlag) { // Active Chroma Processing
        int4 numCtbs = par.PicWidthInCtbs * par.PicHeightInCtbs;
        vector<int1,16> saoModeU;
        read(SAO_MODES, (1 * numCtbs * 16) + ctb * 16, saoModeU);
        vector<int1,16> saoModeV;
        read(SAO_MODES, (2 * numCtbs * 16) + ctb * 16, saoModeV);

        SaoApplyImpl_Chroma(SRC, DST, par, saoModeU, saoModeV, ox, oy);

    } else { // Passive Chroma Processing (Copy)
        matrix<uint1,8,32> chroma;
#pragma unroll
        for (int row = 0; row < 32; row += 8) {
#pragma unroll
            for (int col = 0; col < 64; col += 32) {
                read_plane(SRC, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, chroma);
                write_plane(DST, GENX_SURFACE_UV_PLANE, ox*64+col, oy*32+row, chroma);
            }
        }
    }
}


//extern "C" _GENX_MAIN_ void SaoEstimateAndApply(SurfaceIndex SRC, SurfaceIndex DST, SurfaceIndex PARAM, SurfaceIndex STATS, SurfaceIndex SAO_MODES)
//{
//    MiniVideoParam par;
//    ReadParam(PARAM, par);  
//
//    int2 ox = get_thread_origin_x();
//    int2 oy = get_thread_origin_y();
//
//    vector<int4,16> diffEO = 0;
//    vector<int2,16> countEO = 0;
//    vector<int4,32> diffBO = 0;
//    vector<int2,32> countBO = 0;
//
//    #pragma unroll
//    for (int y = 0; y < 64/STATS_H; y++) {
//        #pragma unroll
//        for (int x = 0; x < 64/STATS_W; x++) {
//            int4 blk = (oy*(64/STATS_H) + y) * par.PicWidthInCtbs * (64/STATS_W) + ox*(64/STATS_W) + x;
//            matrix<int2,2,16> tmpEO;
//            matrix<int2,2,32> tmpBO;
//            read(STATS, (16+16+32+32)*2*blk, tmpEO.format<int2>());
//            read(STATS, (16+16+32+32)*2*blk+16*2*2, tmpBO.format<int2>());
//            diffEO += tmpEO.row(0);
//            diffBO += tmpBO.row(0);
//            countEO += tmpEO.row(1);
//            countBO += tmpBO.row(1);
//        }
//    }
//
//    int4 ctb = oy * par.PicWidthInCtbs + ox;
//
//    vector<int1, 16> saoMode;
//    matrix<int1,2,16> saoMergeModes;
//
//    cm_wait();
//    read(SAO_MODES, GENX_MODIFIED, (ctb - 1) * 16, saoMergeModes.row(0));
//    read(SAO_MODES, GENX_MODIFIED, (ctb - par.PicWidthInCtbs) * 16, saoMergeModes.row(1));
//    GetBestMode(par.enableBandOffset, par.m_rdLambda, diffEO, countEO, diffBO, countBO, saoMode, saoMergeModes, ox > 0, oy > 0);
//    write(SAO_MODES, ctb * 16, saoMode);
//    cm_fence();
//
//    SaoApplyImpl(SRC, DST, par, saoMode, ox, oy);
//}


//extern "C" _GENX_MAIN_ void SaoStatAndEstimate(SurfaceIndex SRC, SurfaceIndex RECON, SurfaceIndex PARAM, SurfaceIndex SAO_MODES)
//{
//    MiniVideoParam par;
//    ReadParam(PARAM, par);  
//       
//    int2 ox = get_thread_origin_x();
//    int2 oy = get_thread_origin_y();
//    int4 ctb = oy * par.PicWidthInCtbs + ox;
//    ox *= 64;
//    oy *= 64;
//
//    vector<int4,16> diffEO = 0;
//    vector<int2,16> countEO = 0;
//    vector<int4,32> diffBO = 0;
//    vector<int2,32> countBO = 0;
//   
//    int2 maskX = 64/W - 1;
//    int2 shiftY = 6 - LOG2_W;
//    int2 num_blocks = 4096/W/H;
//
//    vector<int2,W> pelx;
//    vector<int2,H> pely;
//    cmtl::cm_vector_assign<int2,W>(pelx,0,1);
//    cmtl::cm_vector_assign<int2,H>(pely,0,1);
//
//    for (uint2 blk = 0; blk < num_blocks; blk++) 
//    {
//        int2 blkx = blk & maskX;
//        int2 blky = blk >> shiftY;
//        blkx *= W;
//        blky *= H;
//
//        blkx += ox;
//        blky += oy;
//
//        matrix<uint1,H,W> origin;
//        matrix<uint1,H+2,W> recon;
//        matrix<uint1,H+2,W> reconL;
//        matrix<uint1,H+2,W> reconR;
//
//        read_plane(SRC, GENX_SURFACE_Y_PLANE, blkx, blky, origin);        
//
//        if (W*(H+2) <= 256) {
//            read_plane(RECON, GENX_SURFACE_Y_PLANE, blkx,   blky-1, recon);
//            read_plane(RECON, GENX_SURFACE_Y_PLANE, blkx-1, blky-1, reconL);
//            read_plane(RECON, GENX_SURFACE_Y_PLANE, blkx+1, blky-1, reconR);
//        } else {
//            read_plane(RECON, GENX_SURFACE_Y_PLANE, blkx,   blky-1, recon.select<H,1,W,1>(0,0));
//            read_plane(RECON, GENX_SURFACE_Y_PLANE, blkx,   blky-1+H, recon.select<2,1,W,1>(H,0));
//            read_plane(RECON, GENX_SURFACE_Y_PLANE, blkx-1, blky-1, reconL.select<H,1,W,1>(0,0));
//            read_plane(RECON, GENX_SURFACE_Y_PLANE, blkx-1, blky-1+H, reconL.select<2,1,W,1>(H,0));
//            read_plane(RECON, GENX_SURFACE_Y_PLANE, blkx+1, blky-1, reconR.select<H,1,W,1>(0,0));
//            read_plane(RECON, GENX_SURFACE_Y_PLANE, blkx+1, blky-1+H, reconR.select<2,1,W,1>(H,0));
//        }
//
//        matrix<int2,H,W> diff = origin - recon.select<H,1,W,1>(1,0);
//
//        vector<uint2,W> tmpx = pelx + blkx;
//        vector<uint2,H> tmpy = pely + blky;
//
//        vector<uint2,W> maskHor0 = (tmpx >= par.Width);
//        vector<uint2,H> maskVer0 = (tmpy >= par.Height);
//        vector<uint2,W> maskHor1 = (tmpx == 0) | (tmpx >= par.Width - 1);
//        vector<uint2,H> maskVer1 = (tmpy == 0) | (tmpy >= par.Height - 1);
//
//        matrix<int2,2,16> tmpEO = 0;
//        GetEOStat(recon, reconL, reconR, diff, tmpEO.row(0), tmpEO.row(1), maskHor0, maskHor1, maskVer0, maskVer1);
//        diffEO += tmpEO.row(0);
//        countEO += tmpEO.row(1);
//
//        if (par.enableBandOffset) {
//            matrix<int2,2,32> tmpBO;
//            GetBOStat(recon.select<H,1,W,1>(1,0), diff, tmpBO.row(0), tmpBO.row(1), blkx, blky, par.Width, par.Height);
//            diffBO += tmpBO.row(0);
//            countBO += tmpBO.row(1);
//        }
//    }
//
//    vector<int1, 16> saoMode;
//    matrix<int1,2,16> saoMergeModes;
//
//    cm_wait();
//    read(SAO_MODES, GENX_MODIFIED, (ctb - 1) * 16, saoMergeModes.row(0));
//    read(SAO_MODES, GENX_MODIFIED, (ctb - par.PicWidthInCtbs) * 16, saoMergeModes.row(1));
//    GetBestMode(par.enableBandOffset, par.m_rdLambda, diffEO, countEO, diffBO, countBO, saoMode, saoMergeModes, ox > 0, oy > 0);
//    write(SAO_MODES, ctb * 16, saoMode);
//    cm_fence();
//}
