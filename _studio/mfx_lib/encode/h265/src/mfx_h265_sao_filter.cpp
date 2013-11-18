//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "ipp.h"

#include "mfx_h265_defs.h"
#include "mfx_h265_ctb.h"

#include "mfx_h265_sao_filter.h"

// ========================================================
// UTILS
// ========================================================

const int g_bitDepthY = 8;
const int g_bitDepthC = 8;

const int   g_skipLinesR[NUM_SAO_COMPONENTS] = {5, 3, 3};
const int   g_skipLinesB[NUM_SAO_COMPONENTS] = {4, 2, 2};
const Ipp8u   g_offsetClipTable[] = {
    0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,
    21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,
    43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,
    65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,
    87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,
    107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,
    124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,
    142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,
    161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,
    181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,
    200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,
    218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,
    235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,
    251,252,253,254,255,255,255,255,255,255,255,255};

const Ipp8u* g_offsetClip = &(g_offsetClipTable[SAO_MAX_OFFSET_QVAL]);

//const Ipp16s g_signTable[] = {
//    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
//    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
//    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
//    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
//    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
//    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
//    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
//    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
//    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
//    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
//    -1, -1, -1, -1, -1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
//    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
//
//const Ipp16s* g_sign = &(g_signTable[255]);

inline Ipp16s getSign(Ipp32s x)
{
    return ((x >> 31) | ((Ipp32s)( (((Ipp32u) -x)) >> 31)));
}

// see IEEE "Sample Adaptive Offset in the HEVC standard", p1760 Fast Distortion Estimation
inline Ipp64s EstimateDeltaDist(Ipp64s count, Ipp64s offset, Ipp64s diffSum, int shift)
{
    return (( count*offset*offset - diffSum*offset*2 ) >> shift);
}

inline Ipp64f xRoundIbdi2(int bitDepth, Ipp64f x)
{
    return (x>0) ? (int)(((int)(x)+(1<<(bitDepth-8-1)))/(1<<(bitDepth-8))) : ((int)(((int)(x)-(1<<(bitDepth-8-1)))/(1<<(bitDepth-8))));
}

inline Ipp64f xRoundIbdi(int bitDepth, Ipp64f x)
{
    return (bitDepth > 8 ? xRoundIbdi2(bitDepth, (x)) : ((x)>=0 ? ((int)((x)+0.5)) : ((int)((x)-0.5)))) ;
}

template <typename T> inline T Clip3( T minVal, T maxVal, T a) { return std::min<T> (std::max<T> (minVal, a) , maxVal); }  ///< general min/max clip

Ipp64s GetDistortion(
    int typeIdx,
    int typeAuxInfo,
    int* quantOffset,
    SaoCtuStatistics& statData)
{
    Ipp64s dist=0;
    int shift = 0;

    switch(typeIdx)
    {
        case SAO_TYPE_EO_0:
        case SAO_TYPE_EO_90:
        case SAO_TYPE_EO_135:
        case SAO_TYPE_EO_45:
        {
            for (int offsetIdx=0; offsetIdx<NUM_SAO_EO_CLASSES; offsetIdx++)
            {
                dist += EstimateDeltaDist( statData.count[offsetIdx], quantOffset[offsetIdx], statData.diff[offsetIdx], shift);
            }
        }
        break;

        case SAO_TYPE_BO:
        {
            int startBand = typeAuxInfo;

            for (int bandIdx=startBand; bandIdx<startBand+4; bandIdx++)
            {
                dist += EstimateDeltaDist( statData.count[bandIdx], quantOffset[bandIdx], statData.diff[bandIdx], shift);
            }
        }
        break;

        default:
        {
            VM_ASSERT(!"Not a supported type");
        }
    }

    return dist;

} // Ipp64s GetDistortion(...)


//aya: should be redesigned ASAP
inline int EstimateIterOffset(
    int typeIdx,
    int classIdx,
    Ipp64f lambda,

    int inputOffset,

    Ipp64s count,
    Ipp64s diffSum,

    int shift,
    
    Ipp64s& bestDist,
    Ipp64f& bestCost,
    int offsetTh )
{
    int iterOffset = inputOffset;
    int outputOffset = 0;
    int testOffset;

    Ipp64s tempDist, tempRate;
    Ipp64f cost, minCost;

    // Assuming sending quantized value 0 results in zero offset and sending the value zero needs 1 bit.
    // entropy coder can be used to measure the exact rate here.
    minCost = lambda;

    const int deltaRate = (typeIdx == SAO_TYPE_BO) ? 2 : 1;
    while (iterOffset != 0)
    {
        // Calculate the bits required for signaling the offset
        tempRate = abs(iterOffset) + deltaRate;
        if (abs(iterOffset) == offsetTh) //inclusive
        {
            tempRate --;
        }

        // Do the dequantization before distortion calculation
        testOffset  = iterOffset;

        tempDist    = EstimateDeltaDist( count, testOffset, diffSum, shift);

        cost    = (Ipp64f)tempDist + lambda * (Ipp64f) tempRate;

        if(cost < minCost)
        {
            minCost = cost;
            outputOffset = iterOffset;
            bestDist = tempDist;
            bestCost = cost;
        }

        // offset update
        iterOffset = (iterOffset > 0) ? (iterOffset-1) : (iterOffset+1);
    }

    return outputOffset;

} // int EstimateIterOffset( ... )


void GetQuantOffsets(
    int type_idx,
    SaoCtuStatistics& statData,
    int* quantOffsets,
    int& typeAuxInfo,
    Ipp64f lambda)
{
    int bitDepth = 8;
    int shift = 2 * DISTORTION_PRECISION_ADJUSTMENT(bitDepth-8);
    int offsetTh = SAO_MAX_OFFSET_QVAL;

    memset(quantOffsets, 0, sizeof(int)*MAX_NUM_SAO_CLASSES);

    //derive initial offsets
    int numClasses = (type_idx == SAO_TYPE_BO) ? ((int)NUM_SAO_BO_CLASSES) : ((int)NUM_SAO_EO_CLASSES);

    for(int classIdx=0; classIdx < numClasses; classIdx++)
    {
        if( (type_idx != SAO_TYPE_BO) && (classIdx==SAO_CLASS_EO_PLAIN)  )
        {
            continue; //offset will be zero
        }

        if(statData.count[classIdx] == 0)
        {
            continue; //offset will be zero
        }

        Ipp64f mean_diff = (Ipp64f)statData.diff[classIdx] / (Ipp64f)statData.count[classIdx];

        quantOffsets[classIdx] = (mean_diff) >=0 ? (int)(mean_diff+0.5) : (int)(mean_diff-0.5);

        quantOffsets[classIdx] = Clip3(-offsetTh, offsetTh, quantOffsets[classIdx]);
    }

    // adjust offsets
    switch(type_idx)
    {
        case SAO_TYPE_EO_0:
        case SAO_TYPE_EO_90:
        case SAO_TYPE_EO_135:
        case SAO_TYPE_EO_45:
        {
            Ipp64s classDist;
            Ipp64f classCost;
            for(int classIdx=0; classIdx<NUM_SAO_EO_CLASSES; classIdx++)
            {
                if(classIdx==SAO_CLASS_EO_FULL_VALLEY && quantOffsets[classIdx] < 0) quantOffsets[classIdx] =0;
                if(classIdx==SAO_CLASS_EO_HALF_VALLEY && quantOffsets[classIdx] < 0) quantOffsets[classIdx] =0;
                if(classIdx==SAO_CLASS_EO_HALF_PEAK   && quantOffsets[classIdx] > 0) quantOffsets[classIdx] =0;
                if(classIdx==SAO_CLASS_EO_FULL_PEAK   && quantOffsets[classIdx] > 0) quantOffsets[classIdx] =0;

                if( quantOffsets[classIdx] != 0 )
                {
                    quantOffsets[classIdx] = EstimateIterOffset(
                        type_idx,
                        classIdx,
                        lambda,
                        quantOffsets[classIdx],
                        statData.count[classIdx],
                        statData.diff[classIdx],

                        shift,

                        classDist,
                        classCost,
                        offsetTh );
                }
            }

            typeAuxInfo =0;
        }
        break;

        case SAO_TYPE_BO:
        {
            Ipp64s  distBOClasses[NUM_SAO_BO_CLASSES];
            Ipp64f costBOClasses[NUM_SAO_BO_CLASSES];

            memset(distBOClasses, 0, sizeof(Ipp64s)*NUM_SAO_BO_CLASSES);

            for(int classIdx=0; classIdx< NUM_SAO_BO_CLASSES; classIdx++)
            {
                costBOClasses[classIdx]= lambda;
                if( quantOffsets[classIdx] != 0 )
                {
                    quantOffsets[classIdx] = EstimateIterOffset( 
                        type_idx, 
                        classIdx, 
                        lambda, 
                        quantOffsets[classIdx], 
                        statData.count[classIdx], 
                        statData.diff[classIdx], 

                        shift,

                        distBOClasses[classIdx], 
                        costBOClasses[classIdx], 
                        offsetTh );
                }
            }

            //decide the starting band index
            Ipp64f minCost = MAX_DOUBLE, cost;
            int band, startBand = 0;
            for(band=0; band < (NUM_SAO_BO_CLASSES - 4 + 1); band++)
            {
                cost  = costBOClasses[band  ];
                cost += costBOClasses[band+1];
                cost += costBOClasses[band+2];
                cost += costBOClasses[band+3];

                if(cost < minCost)
                {
                    minCost = cost;
                    startBand = band;
                }
            }

            // clear unused bands
            for(band = 0; band < startBand; band++)
            {
                quantOffsets[band] = 0;
            }
            for(band = startBand + 4; band < NUM_SAO_BO_CLASSES; band++)
            {
                quantOffsets[band] = 0;
            }

            typeAuxInfo = startBand;
        }
        break;

        default:
        {
            VM_ASSERT(!"Not a supported type");
        }
    }

} // void GetQuantOffsets(...)


void GetCtuStatistics_Internal(
    int compIdx,
    PixType* recBlk,
    int recStride,
    PixType* orgBlk,
    int orgStride,

    SaoCtuStatistics* statsDataTypes,

    int width,
    int height,
    bool isLeftAvail,
    bool isAboveAvail,
    bool isAboveLeftAvail,
    bool isAboveRightAvail)
{
    Ipp16s signLineBuf1[64+1];
    Ipp16s signLineBuf2[64+1];

    int x, y, startX, startY, endX, endY;
    int firstLineStartX, firstLineEndX;
    int edgeType;
    Ipp16s signLeft, signRight, signDown;
    Ipp64s *diff, *count;
    
    int skipLinesR = g_skipLinesR[compIdx];
    int skipLinesB = g_skipLinesB[compIdx];

    PixType *recLine, *orgLine;
    PixType* recLineAbove;
    PixType* recLineBelow;


    for(int typeIdx=0; typeIdx< NUM_SAO_BASE_TYPES; typeIdx++)
    {
        SaoCtuStatistics& statsData= statsDataTypes[typeIdx];
        statsData.Reset();

        recLine = recBlk;
        orgLine = orgBlk;
        diff    = statsData.diff;
        count   = statsData.count;
        switch(typeIdx)
        {
            case SAO_TYPE_EO_0:
            {
                diff +=2;
                count+=2;
                endY   = height - skipLinesB;

                startX = isLeftAvail ? 0 : 1;
                endX   = width - skipLinesR;

                for (y=0; y<endY; y++)
                {
                    signLeft = getSign(recLine[startX] - recLine[startX-1]);
                    for (x=startX; x<endX; x++)
                    {
                        signRight =  getSign(recLine[x] - recLine[x+1]);
                        edgeType  =  signRight + signLeft;
                        signLeft  = -signRight;

                        diff [edgeType] += (orgLine[x] - recLine[x]);
                        count[edgeType] ++;
                    }
                    recLine  += recStride;
                    orgLine  += orgStride;
                }
            }
            break;

            case SAO_TYPE_EO_90:
            {
                diff +=2;
                count+=2;
                endX   = width - skipLinesR;

                startY = isAboveAvail ? 0 : 1;
                endY   = height - skipLinesB;
                if (!isAboveAvail)
                {
                    recLine += recStride;
                    orgLine += orgStride;
                }

                recLineAbove = recLine - recStride;
                Ipp16s *signUpLine = signLineBuf1;
                for (x=0; x< endX; x++) 
                {
                    signUpLine[x] = getSign(recLine[x] - recLineAbove[x]);
                }
                
                for (y=startY; y<endY; y++)
                {
                    recLineBelow = recLine + recStride;

                    for (x=0; x<endX; x++)
                    {
                        signDown  = getSign(recLine[x] - recLineBelow[x]); 
                        edgeType  = signDown + signUpLine[x];
                        signUpLine[x]= -signDown;

                        diff [edgeType] += (orgLine[x] - recLine[x]);
                        count[edgeType] ++;
                    }
                    recLine += recStride;
                    orgLine += orgStride;
                }
            }
            break;

            case SAO_TYPE_EO_135:
            {
                diff +=2;
                count+=2;
                startX = isLeftAvail ? 0 : 1 ;
                endX   = width - skipLinesR;
                endY   = height - skipLinesB;

                //prepare 2nd line's upper sign
                Ipp16s *signUpLine, *signDownLine, *signTmpLine;
                signUpLine  = signLineBuf1;
                signDownLine= signLineBuf2;
                recLineBelow = recLine + recStride;
                for (x=startX; x<endX+1; x++)
                {
                    signUpLine[x] = getSign(recLineBelow[x] - recLine[x-1]);
                }

                //1st line
                recLineAbove = recLine - recStride;
                firstLineStartX = isAboveLeftAvail ? 0    : 1;
                firstLineEndX   = isAboveAvail     ? endX : 1;

                for(x=firstLineStartX; x<firstLineEndX; x++)
                {
                    edgeType = getSign(recLine[x] - recLineAbove[x-1]) - signUpLine[x+1];
                    diff [edgeType] += (orgLine[x] - recLine[x]);
                    count[edgeType] ++;
                }
                recLine  += recStride;
                orgLine  += orgStride;


                //middle lines
                for (y=1; y<endY; y++)
                {
                    recLineBelow = recLine + recStride;

                    for (x=startX; x<endX; x++)
                    {
                        signDown = getSign(recLine[x] - recLineBelow[x+1]);
                        edgeType = signDown + signUpLine[x];
                        diff [edgeType] += (orgLine[x] - recLine[x]);
                        count[edgeType] ++;

                        signDownLine[x+1] = -signDown; 
                    }
                    signDownLine[startX] = getSign(recLineBelow[startX] - recLine[startX-1]);

                    signTmpLine  = signUpLine;
                    signUpLine   = signDownLine;
                    signDownLine = signTmpLine;

                    recLine += recStride;
                    orgLine += orgStride;
                }
            }
            break;

            case SAO_TYPE_EO_45:
            {
                diff +=2;
                count+=2;
                endY   = height - skipLinesB;

                startX = isLeftAvail ? 0 : 1;
                endX   = width - skipLinesR;

                //prepare 2nd line upper sign
                recLineBelow = recLine + recStride;
                Ipp16s *signUpLine = signLineBuf1+1;
                for (x=startX-1; x<endX; x++)
                {
                    signUpLine[x] = getSign(recLineBelow[x] - recLine[x+1]);
                }

                //first line
                recLineAbove = recLine - recStride;

                firstLineStartX = isAboveAvail ? startX : endX;
                firstLineEndX   = endX;

                for(x=firstLineStartX; x<firstLineEndX; x++)
                {
                    edgeType = getSign(recLine[x] - recLineAbove[x+1]) - signUpLine[x-1];
                    diff [edgeType] += (orgLine[x] - recLine[x]);
                    count[edgeType] ++;
                }

                recLine += recStride;
                orgLine += orgStride;

                //middle lines
                for (y=1; y<endY; y++)
                {
                    recLineBelow = recLine + recStride;

                    for(x=startX; x<endX; x++)
                    {
                        signDown = getSign(recLine[x] - recLineBelow[x-1]) ;
                        edgeType = signDown + signUpLine[x];

                        diff [edgeType] += (orgLine[x] - recLine[x]);
                        count[edgeType] ++;

                        signUpLine[x-1] = -signDown; 
                    }
                    signUpLine[endX-1] = getSign(recLineBelow[endX-1] - recLine[endX]);
                    recLine  += recStride;
                    orgLine  += orgStride;
                }
            }
            break;

            case SAO_TYPE_BO:
            {
                endX = width- skipLinesR;
                endY = height- skipLinesB;
                const int shiftBits = 3;
                for (y=0; y<endY; y++)
                {
                    for (x=0; x<endX; x++)
                    {
                        int bandIdx= recLine[x] >> shiftBits; 
                        diff [bandIdx] += (orgLine[x] - recLine[x]);
                        count[bandIdx]++;
                    }
                    recLine += recStride;
                    orgLine += orgStride;
                }
            }
            break;

            default:
            {
                VM_ASSERT(!"Not a supported SAO types\n");
            }
        }
    }

} // void GetCtuStatistics_Internal(...)


void InvertQuantOffsets(int type_idx, int typeAuxInfo, int* dstOffsets, int* srcOffsets)
{
    int codedOffset[MAX_NUM_SAO_CLASSES];

    memcpy(codedOffset, srcOffsets, sizeof(int)*MAX_NUM_SAO_CLASSES);
    memset(dstOffsets, 0, sizeof(int)*MAX_NUM_SAO_CLASSES);

    if(type_idx == SAO_TYPE_BO)
    {
        for(int i=0; i< 4; i++)
        {
            dstOffsets[(typeAuxInfo+ i)%NUM_SAO_BO_CLASSES] = codedOffset[(typeAuxInfo+ i)%NUM_SAO_BO_CLASSES];
        }
    }
    else //EO
    {
        for(int i=0; i< NUM_SAO_EO_CLASSES; i++)
        {
            dstOffsets[i] = codedOffset[i];
        }
        VM_ASSERT(dstOffsets[SAO_CLASS_EO_PLAIN] == 0); //keep EO plain offset as zero
    }

} // void InvertQuantOffsets( ... )


void ReconstructCtuSaoParam(SaoCtuParam& recParam, std::vector<SaoCtuParam*>& mergeList)
{
    //for(int compIdx=0; compIdx< NUM_SAO_COMPONENTS; compIdx++)
    for(int compIdx=0; compIdx<1; compIdx++)
    {
        SaoOffsetParam& offsetParam = recParam[compIdx];

        switch(offsetParam.mode_idx)
        {
            case SAO_MODE_OFF:
            {
                ;//aya: TO DO late
            }
            break;

            case SAO_MODE_ON:
            {
                InvertQuantOffsets(offsetParam.type_idx, offsetParam.typeAuxInfo, offsetParam.offset, offsetParam.offset);
            }
            break;

            case SAO_MODE_MERGE:
            {
                SaoCtuParam* mergeTarget = mergeList[offsetParam.type_idx];
                VM_ASSERT(mergeTarget != NULL);

                offsetParam = (*mergeTarget)[compIdx];
            }
            break;

            default:
            {
                VM_ASSERT(!"Not a supported mode");
            }
        }
    }

} // void ReconstructCtuSaoParam(...)


void SliceDecisionAlgorithm(bool* sliceEnabled, int picTempLayer)
{
    // aya: here should be placed real algorithm to SliceLevelControl On/Off
    for (int compIdx=0; compIdx<1; compIdx++)
    {
        sliceEnabled[compIdx] = true;
    }

} // void SliceDecisionAlgorithm(bool* sliceEnabled, int picTempLayer)


void ApplyCtuSao_Internal(
    int typeIdx,
    int* offset,
    PixType* srcBlk,
    PixType* resBlk,
    int srcStride,
    int resStride,
    int width,
    int height,
    bool isLeftAvail,
    bool isRightAvail,
    bool isAboveAvail,
    bool isBelowAvail,
    bool isAboveLeftAvail,
    bool isAboveRightAvail,
    bool isBelowLeftAvail,
    bool isBelowRightAvail)
{
    Ipp16s signLineBuf1[64+1];
    Ipp16s signLineBuf2[64+1];

    int x,y, startX, startY, endX, endY, edgeType;
    int firstLineStartX, firstLineEndX, lastLineStartX, lastLineEndX;
    Ipp16s signLeft, signRight, signDown;

    PixType* srcLine = srcBlk;
    PixType* resLine = resBlk;

    switch(typeIdx)
    {
    case SAO_TYPE_EO_0:
        {
            offset += 2;
            startX = isLeftAvail ? 0 : 1;
            endX   = isRightAvail ? width : (width -1);
            for (y=0; y< height; y++)
            {
                signLeft = (Ipp16s)getSign(srcLine[startX] - srcLine[startX-1]);
                for (x=startX; x< endX; x++)
                {
                    signRight = (Ipp16s)getSign(srcLine[x] - srcLine[x+1]);
                    edgeType =  signRight + signLeft;
                    signLeft  = -signRight;

                    resLine[x] = g_offsetClip[srcLine[x] + offset[edgeType]];
                }
                srcLine  += srcStride;
                resLine += resStride;
            }

        }
        break;
    case SAO_TYPE_EO_90:
        {
            offset += 2;
            Ipp16s *signUpLine = signLineBuf1;

            startY = isAboveAvail ? 0 : 1;
            endY   = isBelowAvail ? height : height-1;
            if (!isAboveAvail)
            {
                srcLine += srcStride;
                resLine += resStride;
            }

            PixType* srcLineAbove= srcLine- srcStride;
            for (x=0; x< width; x++)
            {
                signUpLine[x] = (Ipp16s)getSign(srcLine[x] - srcLineAbove[x]);
            }

            PixType* srcLineBelow;
            for (y=startY; y<endY; y++)
            {
                srcLineBelow= srcLine+ srcStride;

                for (x=0; x< width; x++)
                {
                    signDown  = (Ipp16s)getSign(srcLine[x] - srcLineBelow[x]);
                    edgeType = signDown + signUpLine[x];
                    signUpLine[x]= -signDown;

                    resLine[x] = g_offsetClip[srcLine[x] + offset[edgeType]];
                }
                srcLine += srcStride;
                resLine += resStride;
            }

        }
        break;
    case SAO_TYPE_EO_135:
        {
            offset += 2;
            Ipp16s *signUpLine, *signDownLine, *signTmpLine;

            signUpLine  = signLineBuf1;
            signDownLine= signLineBuf2;

            startX = isLeftAvail ? 0 : 1 ;
            endX   = isRightAvail ? width : (width-1);

            //prepare 2nd line's upper sign
            PixType* srcLineBelow= srcLine+ srcStride;
            for (x=startX; x< endX+1; x++)
            {
                signUpLine[x] = (Ipp16s)getSign(srcLineBelow[x] - srcLine[x- 1]);
            }

            //1st line
            PixType* srcLineAbove= srcLine- srcStride;
            firstLineStartX = isAboveLeftAvail ? 0 : 1;
            firstLineEndX   = isAboveAvail? endX: 1;
            for(x= firstLineStartX; x< firstLineEndX; x++)
            {
                edgeType  =  getSign(srcLine[x] - srcLineAbove[x- 1]) - signUpLine[x+1];
                resLine[x] = g_offsetClip[srcLine[x] + offset[edgeType]];
            }
            srcLine  += srcStride;
            resLine  += resStride;


            //middle lines
            for (y= 1; y< height-1; y++)
            {
                srcLineBelow= srcLine+ srcStride;

                for (x=startX; x<endX; x++)
                {
                    signDown =  (Ipp16s)getSign(srcLine[x] - srcLineBelow[x+ 1]) ;
                    edgeType =  signDown + signUpLine[x];
                    resLine[x] = g_offsetClip[srcLine[x] + offset[edgeType]];

                    signDownLine[x+1] = -signDown;
                }
                signDownLine[startX] = (Ipp16s)getSign(srcLineBelow[startX] - srcLine[startX-1]);

                signTmpLine  = signUpLine;
                signUpLine   = signDownLine;
                signDownLine = signTmpLine;

                srcLine += srcStride;
                resLine += resStride;
            }

            //last line
            srcLineBelow= srcLine+ srcStride;
            lastLineStartX = isBelowAvail ? startX : (width -1);
            lastLineEndX   = isBelowRightAvail ? width : (width -1);
            for(x= lastLineStartX; x< lastLineEndX; x++)
            {
                edgeType =  getSign(srcLine[x] - srcLineBelow[x+ 1]) + signUpLine[x];
                resLine[x] = g_offsetClip[srcLine[x] + offset[edgeType]];

            }
        }
        break;
    case SAO_TYPE_EO_45:
        {
            offset += 2;
            Ipp16s *signUpLine = signLineBuf1+1;

            startX = isLeftAvail ? 0 : 1;
            endX   = isRightAvail ? width : (width -1);

            //prepare 2nd line upper sign
            PixType* srcLineBelow= srcLine+ srcStride;
            for (x=startX-1; x< endX; x++)
            {
                int pos = (int)srcLineBelow[x] - (int)srcLine[x+1];
                VM_ASSERT( pos < 256 && pos > -256 );
                signUpLine[x] = getSign(pos);
                //signUpLine[x] = (Char)g_sign[ (int)srcLineBelow[x] - (int)srcLine[x+1]];
            }


            //first line
            PixType* srcLineAbove= srcLine- srcStride;
            firstLineStartX = isAboveAvail ? startX : (width -1 );
            firstLineEndX   = isAboveRightAvail ? width : (width-1);
            for(x= firstLineStartX; x< firstLineEndX; x++)
            {
                edgeType = getSign(srcLine[x] - srcLineAbove[x+1]) -signUpLine[x-1];
                resLine[x] = g_offsetClip[srcLine[x] + offset[edgeType]];
            }
            srcLine += srcStride;
            resLine += resStride;

            //middle lines
            for (y= 1; y< height-1; y++)
            {
                srcLineBelow= srcLine+ srcStride;

                for(x= startX; x< endX; x++)
                {
                    signDown =  (int)getSign(srcLine[x] - srcLineBelow[x-1]) ;
                    edgeType =  signDown + signUpLine[x];
                    resLine[x] = g_offsetClip[srcLine[x] + offset[edgeType]];
                    signUpLine[x-1] = -signDown;
                }
                signUpLine[endX-1] = (Ipp16s)getSign(srcLineBelow[endX-1] - srcLine[endX]);
                srcLine  += srcStride;
                resLine += resStride;
            }

            //last line
            srcLineBelow= srcLine+ srcStride;
            lastLineStartX = isBelowLeftAvail ? 0 : 1;
            lastLineEndX   = isBelowAvail ? endX : 1;
            for(x= lastLineStartX; x< lastLineEndX; x++)
            {
                edgeType = getSign(srcLine[x] - srcLineBelow[x-1]) + signUpLine[x];
                resLine[x] = g_offsetClip[srcLine[x] + offset[edgeType]];

            }
        }
        break;
    case SAO_TYPE_BO:
        {
            //int shiftBits = ((compIdx == SAO_Y)?g_bitDepthY:g_bitDepthC)- NUM_SAO_BO_CLASSES_LOG2;
            int shiftBits = g_bitDepthY- NUM_SAO_BO_CLASSES_LOG2;
            for (y=0; y< height; y++)
            {
                for (x=0; x< width; x++)
                {
                    resLine[x] = g_offsetClip[ srcLine[x] + offset[srcLine[x] >> shiftBits] ];
                }
                srcLine += srcStride;
                resLine += resStride;
            }
        }
        break;
    default:
        {
            VM_ASSERT(!"Not a supported SAO types\n");
        }
    }

} // void ApplyCtuSao_Internal( ... )

// ========================================================
// SAO Offset
// ========================================================

SaoOffsetParam::SaoOffsetParam()
{
    Reset();
}

SaoOffsetParam::~SaoOffsetParam()
{
}

void SaoOffsetParam::Reset()
{
    mode_idx = SAO_MODE_OFF;
    type_idx = -1;
    typeAuxInfo = -1;
    memset(offset, 0, sizeof(int)* MAX_NUM_SAO_CLASSES);
}

const SaoOffsetParam& SaoOffsetParam::operator= (const SaoOffsetParam& src)
{
    mode_idx = src.mode_idx;
    type_idx = src.type_idx;
    typeAuxInfo = src.typeAuxInfo;
    memcpy(offset, src.offset, sizeof(int)* MAX_NUM_SAO_CLASSES);

    return *this;
}

// ========================================================
// SaoCtuParam
// ========================================================

SaoCtuParam::SaoCtuParam()
{
    Reset();
}

SaoCtuParam::~SaoCtuParam()
{

}

void SaoCtuParam::Reset()
{
    for(int compIdx=0; compIdx< 3; compIdx++)
    {
        m_offsetParam[compIdx].Reset();
    }
}

const SaoCtuParam& SaoCtuParam::operator= (const SaoCtuParam& src)
{
    for(int compIdx=0; compIdx< 3; compIdx++)
    {
        m_offsetParam[compIdx] = src.m_offsetParam[compIdx];
    }
    return *this;

}

// ========================================================
// SAO FILTER
// ========================================================

SAOFilter::SAOFilter()
{
    // TODO

} // SAOFilter::SAOFilter()


void SAOFilter::Close()
{

} // void SAOFilter::Close()


SAOFilter::~SAOFilter()
{
    Close();

} // SAOFilter::~SAOFilter()


void SAOFilter::Init(int width, int height, int maxCUWidth, int maxDepth)
{
    m_frameSize.width = width;
    m_frameSize.height= height;

    m_maxCUSize = maxCUWidth;

    m_numCTU_inWidth = (m_frameSize.width/m_maxCUSize)  + ((m_frameSize.width % m_maxCUSize)?1:0);
    m_numCTU_inHeight= (m_frameSize.height/m_maxCUSize) + ((m_frameSize.height % m_maxCUSize)?1:0);

} // void SAOFilter::Init(...)


void SAOFilter::EstimateCtuSao(mfxFrameData* orgYuv, mfxFrameData* recYuv, bool* sliceEnabled, SaoCtuParam* saoParam)
{
    GetCtuSaoStatistics(orgYuv, recYuv);

    //slice on/off decision algorithm.
    // depth - it depends on GOPSize. Need for early alg termination
    sliceEnabled[SAO_Y] = sliceEnabled[SAO_Cb] = sliceEnabled[SAO_Cr] = false;
    int tmpDepth = 0;
    SliceDecisionAlgorithm(sliceEnabled, tmpDepth);//pPic->getSlice(0)->getDepth());

    GetBestCtuSaoParam(sliceEnabled, recYuv, saoParam);

} // void SAOFilter::EstimateCtuSao()


void SAOFilter::GetCtuSaoStatistics(mfxFrameData* orgYuv, mfxFrameData* recYuv)
{
    bool isLeftAvail, isAboveAvail, isAboveLeftAvail,isAboveRightAvail;

    {
        int xPos   = m_ctb_pelx;//(ctu / m_numCTU_inWidth)*m_maxCUWidth;
        int yPos   = m_ctb_pely;//(ctu % m_numCTU_inWidth)*m_maxCUWidth;
        int height = (yPos + m_maxCUSize > m_frameSize.height)?(m_frameSize.height- yPos):m_maxCUSize;
        int width  = (xPos + m_maxCUSize  > m_frameSize.width )?(m_frameSize.width - xPos):m_maxCUSize;

        // ------------------------------------------------
        isLeftAvail  = (m_ctb_addr % m_numCTU_inWidth != 0);
        isAboveAvail = (m_ctb_addr >= m_numCTU_inWidth );
        isAboveLeftAvail = false;//(isAboveAvail && isLeftAvail);
        isAboveRightAvail= false;//(isAboveAvail && isRightAvail);// make sense for WPP = 1 only

        // ------------------------------------------------

        // aya!!! FIXME!!!
        //pPic->getPicSym()->deriveLoopFilterBoundaryAvailibility(ctu, isLeftAvail,isRightAvail,isAboveAvail,isBelowAvail,isAboveLeftAvail,isAboveRightAvail,isBelowLeftAvail,isBelowRightAvail);

        //NOTE: The number of skipped lines during gathering CTU statistics depends on the slice boundary availabilities.
        //For simplicity, here only picture boundaries are considered.

        /*isRightAvail      = (xPos + m_maxCUWidth  < m_frameSize.width );
        isBelowAvail      = (yPos + m_maxCUWidth < m_frameSize.height);
        isBelowRightAvail = (isRightAvail && isBelowAvail);
        isBelowLeftAvail  = ((xPos > 0) && (isBelowAvail));
        isAboveRightAvail = ((yPos > 0) && (isRightAvail));*/
        // ---------------------------------------------------

        for(int compIdx=0; compIdx< 1; compIdx++)
        {
            bool isLuma     = (compIdx == SAO_Y);
            int  formatShift= isLuma?0:1;

            int  recStride = isLuma?recYuv->Pitch:recYuv->Pitch;
            PixType* recBlk=  recYuv->Y;

            int  orgStride  = isLuma?orgYuv->Pitch:orgYuv->Pitch;
            PixType* orgBlk = orgYuv->Y;

            GetCtuStatistics_Internal(
                compIdx,
                recBlk,
                recStride,
                orgBlk,
                orgStride,
                m_statData[compIdx],
                (width  >> formatShift),
                (height >> formatShift),
                isLeftAvail,
                isAboveAvail,
                isAboveLeftAvail,
                isAboveRightAvail);
        }
    }

} // void SAOFilter::GetCtuSaoStatistics(...)


void SAOFilter::GetBestCtuSaoParam(
    bool* sliceEnabled,
    mfxFrameData* srcYuv,
    SaoCtuParam* codedParam)
{
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_CUR], h265_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

#if defined(SAO_MODE_MERGE_ENABLED)
    //get merge list
    std::vector<SaoCtuParam*> mergeList;
    getMergeList(/*pic,*/
        //ctu,
        m_ctb_addr,// aya!!!!!!!!
        reconParams, mergeList);
#else
    std::vector<SaoCtuParam*> mergeList(2);
    mergeList[0] = NULL;
    mergeList[1] = NULL;
#endif

    SaoCtuParam modeParam;
    Ipp64f minCost = MAX_DOUBLE, modeCost = MAX_DOUBLE;

    for(int mode=0; mode < NUM_SAO_MODES; mode++)
    {
        switch(mode)
        {
            case SAO_MODE_OFF:
            {
                continue; //not necessary, since all-off case will be tested in SAO_MODE_ON case.
            }
            break;

            case SAO_MODE_ON:
            {
                ModeDecision_Base(
                    mergeList,
                    sliceEnabled,
                    modeParam,
                    modeCost,
                    SAO_CABACSTATE_BLK_CUR);
            }
            break;

            case SAO_MODE_MERGE:
            {
#if defined(SAO_MODE_MERGE_ENABLED)
                // AYA!!! FIXME
                ModeDecision_Merge(
                    ctu,
                    mergeList, sliceEnabled, blkStats , modeParam, modeCost, /*m_pppcRDSbacCoder,*/ SAO_CABACSTATE_BLK_CUR);
#endif
            }
            break;

            default:
            {
                VM_ASSERT(!"Not a supported SAO mode\n");
            }
        }

        if(modeCost < minCost)
        {
            minCost = modeCost;
            *codedParam = modeParam;
            m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_NEXT], h265_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
        }
    } //mode

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_NEXT], h265_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

    ReconstructCtuSaoParam(*codedParam, mergeList);

} // void SAOFilter::GetBestCtuSaoParam(...)


int SAOFilter::getMergeList(int ctu, SaoCtuParam* blkParams, std::vector<SaoCtuParam*>& mergeList)
{
    int ctuX = ctu % m_numCTU_inWidth;
    int ctuY = ctu / m_numCTU_inWidth;
    int mergedCTUPos;
    int numValidMergeCandidates = 0;

    for(int mergeType=0; mergeType< NUM_SAO_MERGE_TYPES; mergeType++)
    {
        SaoCtuParam* mergeCandidate = NULL;

        switch(mergeType)
        {
        case SAO_MERGE_ABOVE:
            {
                if(ctuY > 0)
                {
                    mergedCTUPos = ctu- m_numCTU_inWidth;
                    //if( pic->getSAOMergeAvailability(ctu, mergedCTUPos) )
                    {
                        //mergeCandidate = &(blkParams[mergedCTUPos]);
                        mergeCandidate = &(m_codedParams_TotalFrame[mergedCTUPos]);
                    }
                }
            }
            break;
        case SAO_MERGE_LEFT:
            {
                if(ctuX > 0)
                {
                    mergedCTUPos = ctu- 1;
                    //if( pic->getSAOMergeAvailability(ctu, mergedCTUPos) )
                    {
                        //mergeCandidate = &(blkParams[mergedCTUPos]);
                        mergeCandidate = &(m_codedParams_TotalFrame[mergedCTUPos]);
                    }
                }
            }
            break;
        default:
            {
                printf("not a supported merge type");
                //VM_ASSERT(0);
                exit(-1);
            }
        }

        mergeList.push_back(mergeCandidate);
        if (mergeCandidate != NULL)
        {
            numValidMergeCandidates++;
        }
    }

    return numValidMergeCandidates;

} // int SAOFilter::getMergeList(int ctu, SaoCtuParam* blkParams, std::vector<SaoCtuParam*>& mergeList)


void SAOFilter::ModeDecision_Merge(std::vector<SaoCtuParam*>& mergeList, bool* sliceEnabled, SaoCtuParam& modeParam, Ipp64f& modeNormCost, int inCabacLabel)
{
    int mergeListSize = (int)mergeList.size();
    modeNormCost = MAX_DOUBLE;

    Ipp64f cost;
    SaoCtuParam testBlkParam;

    for(int mergeType=0; mergeType< mergeListSize; mergeType++)
    {
        if(mergeList[mergeType] == NULL)
        {
            continue;
        }

        testBlkParam = *(mergeList[mergeType]);
        //normalized distortion
        Ipp64f normDist=0;
        //for(int compIdx=0; compIdx< NUM_SAO_COMPONENTS; compIdx++)
        for(int compIdx=0; compIdx< 1; compIdx++)
        {
            testBlkParam[compIdx].mode_idx = SAO_MODE_MERGE;
            testBlkParam[compIdx].type_idx = mergeType;

            SaoOffsetParam& mergedOffsetParam = (*(mergeList[mergeType]))[compIdx];

            if( mergedOffsetParam.mode_idx != SAO_MODE_OFF)
            {
                //offsets have been reconstructed. Don't call inversed quantization function.
                normDist += (((Ipp64f)GetDistortion(
                    mergedOffsetParam.type_idx,
                    mergedOffsetParam.typeAuxInfo,
                    mergedOffsetParam.offset,
                    m_statData[compIdx][mergedOffsetParam.type_idx])) / m_labmda[compIdx]);
            }

        }

        m_bsf->CtxRestore(m_ctxSAO[inCabacLabel], 0, NUM_CABAC_CONTEXT);
        m_bsf->Reset();

        h265_code_sao_ctb_param(m_bsf, testBlkParam, sliceEnabled, (mergeList[SAO_MERGE_LEFT]!= NULL), (mergeList[SAO_MERGE_ABOVE]!= NULL), false);

        int rate = GetNumWrittenBits();

        cost = normDist + (Ipp64f)rate;

        if(cost < modeNormCost)
        {
            modeNormCost = cost;
            modeParam    = testBlkParam;
            m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], 0, NUM_CABAC_CONTEXT);
        }
    }

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], 0, NUM_CABAC_CONTEXT);

} // void SAOFilter::ModeDecision_Merge(...)


void SAOFilter::ModeDecision_Base(
    std::vector<SaoCtuParam*>& mergeList,
    bool* sliceEnabled,
    SaoCtuParam& modeParam,
    Ipp64f& modeNormCost,
    int inCabacLabel)
{
    Ipp64f minCost = 0.0, cost = 0.0;
    int rate = 0, minRate = 0;
    Ipp64s dist[NUM_SAO_COMPONENTS], modeDist[NUM_SAO_COMPONENTS];
    SaoOffsetParam testOffset[NUM_SAO_COMPONENTS];
    int compIdx;

    modeDist[SAO_Y]= modeDist[SAO_Cb] = modeDist[SAO_Cr] = 0;

    //pre-encode merge flags
    modeParam[SAO_Y ].mode_idx = SAO_MODE_OFF;

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_CUR], h265_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    h265_code_sao_ctb_param(m_bsf, modeParam, sliceEnabled, (mergeList[SAO_MERGE_LEFT]!= NULL), (mergeList[SAO_MERGE_ABOVE]!= NULL), true);
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_MID], h265_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

    //------ luma ---
    compIdx = SAO_Y;
    //"off" case as initial cost
    modeParam[compIdx].mode_idx = SAO_MODE_OFF;

    m_bsf->Reset();
    h265_code_sao_ctb_offset_param(m_bsf, compIdx, modeParam[compIdx], sliceEnabled[compIdx]);
    minRate= GetNumWrittenBits();

    modeDist[compIdx] = 0;
    minCost= m_labmda[compIdx]*((Ipp64f)minRate);

    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], h265_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

    if(sliceEnabled[compIdx])
    {
        for(int type_idx=0; type_idx< NUM_SAO_BASE_TYPES; type_idx++)
        {
            testOffset[compIdx].mode_idx = SAO_MODE_ON;
            testOffset[compIdx].type_idx = type_idx;

            GetQuantOffsets(
                type_idx, 
                m_statData[compIdx][type_idx], 
                testOffset[compIdx].offset, 
                testOffset[compIdx].typeAuxInfo, 
                m_labmda[compIdx]);

            dist[compIdx] = GetDistortion(
                testOffset[compIdx].type_idx, 
                testOffset[compIdx].typeAuxInfo, 
                testOffset[compIdx].offset, 
                m_statData[compIdx][type_idx]);

            m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_MID], h265_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
            m_bsf->Reset();
            h265_code_sao_ctb_offset_param(m_bsf, compIdx, testOffset[compIdx], sliceEnabled[compIdx]);
            rate = GetNumWrittenBits();

            cost = (Ipp64f)dist[compIdx] + m_labmda[compIdx]*((Ipp64f)rate);
            if(cost < minCost)
            {
                minCost = cost;
                minRate = rate;
                modeDist[compIdx] = dist[compIdx];
                modeParam[compIdx]= testOffset[compIdx];

                m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], h265_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
            }
        }
    }

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], h265_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_MID], h265_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

    // aya - temporal solution
    bool isChromaEnabled = false;
    Ipp64f chromaLambda = m_labmda[SAO_Cb];

    // ======================================================
    // aya potential issue???
    // ======================================================
    if( isChromaEnabled )
    {
        //------ chroma --------//
        VM_ASSERT(m_labmda[SAO_Cb] == m_labmda[SAO_Cr]);
        //Ipp64f chromaLambda = m_labmda[SAO_Cb];
        //"off" case as initial cost
        //m_pcRDGoOnSbacCoder->resetBits();
        modeParam[SAO_Cb].mode_idx = SAO_MODE_OFF;
        //m_pcRDGoOnSbacCoder->h265_code_sao_ctb_offset_param(SAO_Cb, modeParam[SAO_Cb], sliceEnabled[SAO_Cb]);
        modeParam[SAO_Cr].mode_idx = SAO_MODE_OFF;
        //m_pcRDGoOnSbacCoder->h265_code_sao_ctb_offset_param(SAO_Cr, modeParam[SAO_Cr], sliceEnabled[SAO_Cr]);
        //minRate= m_pcRDGoOnSbacCoder->getNumberOfWrittenBits();
        modeDist[SAO_Cb] = modeDist[SAO_Cr]= 0;
        minCost= chromaLambda*((Ipp64f)minRate);

        //doesn't need to store cabac status here since the whole CTU parameters will be re-encoded at the end of this function

        for(int type_idx=0; type_idx< NUM_SAO_BASE_TYPES; type_idx++)
        {
            for(compIdx= SAO_Cb; compIdx< NUM_SAO_COMPONENTS; compIdx++)
            {
                if(!sliceEnabled[compIdx])
                {
                    testOffset[compIdx].mode_idx = SAO_MODE_OFF;
                    dist[compIdx]= 0;
                    continue;
                }
                testOffset[compIdx].mode_idx = SAO_MODE_ON;
                testOffset[compIdx].type_idx = type_idx;


                GetQuantOffsets(type_idx, m_statData[compIdx][type_idx], testOffset[compIdx].offset, testOffset[compIdx].typeAuxInfo, m_labmda[compIdx]);

                //InvertQuantOffsets(type_idx, testOffset[compIdx].typeAuxInfo, invQuantOffset, testOffset[compIdx].offset);

                dist[compIdx]= GetDistortion(type_idx, testOffset[compIdx].typeAuxInfo, testOffset[compIdx].offset, m_statData[compIdx][type_idx]);
            }

            //get rate
            /*m_pcRDGoOnSbacCoder->load(cabacCoderRDO[SAO_CABACSTATE_BLK_MID]);
            m_pcRDGoOnSbacCoder->resetBits();
            m_pcRDGoOnSbacCoder->h265_code_sao_ctb_offset_param(SAO_Cb, testOffset[SAO_Cb], sliceEnabled[SAO_Cb]);
            m_pcRDGoOnSbacCoder->h265_code_sao_ctb_offset_param(SAO_Cr, testOffset[SAO_Cr], sliceEnabled[SAO_Cr]);
            rate = m_pcRDGoOnSbacCoder->getNumberOfWrittenBits();*/

            cost = (Ipp64f)(dist[SAO_Cb]+ dist[SAO_Cr]) + chromaLambda*((Ipp64f)rate);
            if(cost < minCost)
            {
                minCost = cost;
                minRate = rate;
                modeDist[SAO_Cb] = dist[SAO_Cb];
                modeDist[SAO_Cr] = dist[SAO_Cr];
                modeParam[SAO_Cb]= testOffset[SAO_Cb];
                modeParam[SAO_Cr]= testOffset[SAO_Cr];
            }
        }
    } // if( isChromaEnabled )

    //----- re-gen rate & normalized cost----//
    modeNormCost  = (Ipp64f)modeDist[SAO_Y]/m_labmda[SAO_Y];

    if( isChromaEnabled )
    {
        modeNormCost += (Ipp64f)(modeDist[SAO_Cb]+ modeDist[SAO_Cr])/chromaLambda;
    }

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_CUR], h265_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    m_bsf->Reset();
    h265_code_sao_ctb_param(m_bsf, modeParam, sliceEnabled, (mergeList[SAO_MERGE_LEFT]!= NULL), (mergeList[SAO_MERGE_ABOVE]!= NULL), false);
    modeNormCost += (Ipp64f)GetNumWrittenBits();

} // void SAOFilter::ModeDecision_Base(...)


void SAOFilter::ApplyCtuSao(
    mfxFrameData* srcYuv,
    mfxFrameData* resYuv,
    SaoCtuParam& saoblkParam,
    int ctu)
{
    bool isLeftAvail,isRightAvail,isAboveAvail,isBelowAvail,isAboveLeftAvail,isAboveRightAvail,isBelowLeftAvail,isBelowRightAvail;

    if(
        (saoblkParam[SAO_Y ].mode_idx == SAO_MODE_OFF) &&
        (saoblkParam[SAO_Cb].mode_idx == SAO_MODE_OFF) &&
        (saoblkParam[SAO_Cr].mode_idx == SAO_MODE_OFF)
        )
    {
        return;
    }

    //block boundary availability
    //-------------------------------------------------------
    // reference code
    //pPic->getPicSym()->deriveLoopFilterBoundaryAvailibility(ctu, isLeftAvail,isRightAvail,isAboveAvail,isBelowAvail,isAboveLeftAvail,isAboveRightAvail,isBelowLeftAvail,isBelowRightAvail);
    // ------------------------------------------------
    m_ctb_addr = ctu;
    int numCTU_inFrame = m_numCTU_inWidth * m_numCTU_inHeight;

    isLeftAvail  = (m_ctb_addr % m_numCTU_inWidth != 0);
    isRightAvail = (m_ctb_addr % m_numCTU_inWidth != m_numCTU_inWidth-1);
    isAboveAvail = (m_ctb_addr >= m_numCTU_inWidth );
    isBelowAvail = (m_ctb_addr <  numCTU_inFrame - m_numCTU_inWidth);
    isAboveLeftAvail = (isAboveAvail && isLeftAvail);
    isAboveRightAvail= (isAboveAvail && isRightAvail);
    isBelowLeftAvail = (isBelowAvail && isLeftAvail);
    isBelowRightAvail= (isBelowAvail && isRightAvail);
    //-------------------------------------------------------

    int yPos   = (m_ctb_addr / m_numCTU_inWidth)*m_maxCUSize;
    int xPos   = (m_ctb_addr % m_numCTU_inWidth)*m_maxCUSize;

    int height = (yPos + m_maxCUSize > m_frameSize.height)?(m_frameSize.height- yPos):m_maxCUSize;
    int width  = (xPos + m_maxCUSize  > m_frameSize.width)?(m_frameSize.width - xPos):m_maxCUSize;

    for(int compIdx= 0; compIdx < 1; compIdx++)
    {
        SaoOffsetParam& ctbOffset = saoblkParam[compIdx];

        if(ctbOffset.mode_idx != SAO_MODE_OFF)
        {
            bool isLuma     = (compIdx == SAO_Y);
            int  formatShift= isLuma?0:1;

            int  blkWidth   = (width  >> formatShift);
            int  blkHeight  = (height >> formatShift);
            /*int  blkYPos    = (yPos   >> formatShift);
            int  blkXPos    = (xPos   >> formatShift);*/

            int  srcStride = isLuma? srcYuv->Pitch:srcYuv->Pitch;
            PixType* srcBlk    = srcYuv->Y;

            int  resStride   = isLuma?resYuv->Pitch:resYuv->Pitch;
            PixType* resBlk  = resYuv->Y;

            ApplyCtuSao_Internal(
                ctbOffset.type_idx,
                ctbOffset.offset,
                srcBlk,
                resBlk,
                srcStride,
                resStride,
                blkWidth,
                blkHeight,
                isLeftAvail, isRightAvail, isAboveAvail, isBelowAvail, isAboveLeftAvail, isAboveRightAvail, isBelowLeftAvail, isBelowRightAvail);
        }
    } //compIdx

} // void SAOFilter::ApplyCtuSao(...)


// ========================================================
//  CTU Caller
// ========================================================

void H265CU::EstimateCtuSao( H265BsFake *bs, SaoCtuParam* saoParam, SaoCtuParam* saoParam_TotalFrame )
{
    m_saoFilter.m_ctb_addr = this->ctb_addr;
    m_saoFilter.m_ctb_pelx = this->ctb_pelx;
    m_saoFilter.m_ctb_pely = this->ctb_pely;

    m_saoFilter.m_codedParams_TotalFrame = saoParam_TotalFrame;
    m_saoFilter.m_bsf = bs;
    m_saoFilter.m_labmda[0] = this->rd_lambda*256;
    // ----------------------------------------------------

    // run
    mfxFrameData orgYuv;
    mfxFrameData recYuv;

    orgYuv.Y = this->y_src;
    orgYuv.UV = this->uv_src;
    orgYuv.Pitch = (Ipp16s)this->pitch_src;

    recYuv.Y = this->y_rec;
    recYuv.U = this->u_rec;
    recYuv.V = this->v_rec;
    recYuv.Pitch = (Ipp16s)this->pitch_rec_luma;

    bool    sliceEnabled[NUM_SAO_COMPONENTS] = {false, false, false};
    m_saoFilter.EstimateCtuSao( &orgYuv, &recYuv, sliceEnabled, saoParam);

    // set slice param
    if( !this->cslice->slice_sao_luma_flag )
    {
        this->cslice->slice_sao_luma_flag = (Ipp8u)sliceEnabled[SAO_Y];
    }
    if( !this->cslice->slice_sao_chroma_flag )
    {
        this->cslice->slice_sao_chroma_flag = 0;
    }

    return;

} // void H265CU::EstimateCtuSao( void )

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
/* EOF */
