//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <math.h>
#include "mfx_h265_sao_filter.h"
#include "mfx_h265_dispatcher.h"
#include "mfx_h265_ctb.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_encode.h"

namespace H265Enc {

// see IEEE "Sample Adaptive Offset in the HEVC standard", p1760 Fast Distortion Estimation
inline Ipp64s EstimateDeltaDist(Ipp64s count, Ipp64s offset, Ipp64s diffSum, int shift)
{
    return (( count*offset*offset - diffSum*offset*2 ) >> shift);
}

Ipp64s GetDistortion( int typeIdx, int typeAuxInfo, int* quantOffset,  MFX_HEVC_PP::SaoCtuStatistics& statData, int shift)
{
    Ipp64s dist=0;
    int startFrom = 0;
    int end = NUM_SAO_EO_CLASSES;

    if (SAO_TYPE_BO == typeIdx) {
        startFrom = typeAuxInfo;
        end = startFrom + 4;
    }
    for (int offsetIdx=startFrom; offsetIdx < end; offsetIdx++) {
        dist += EstimateDeltaDist( statData.count[offsetIdx], quantOffset[offsetIdx], statData.diff[offsetIdx], shift);
    }
    return dist;
}


// try on each offset from [0, inputOffset]
inline int GetBestOffset( int typeIdx, int classIdx, Ipp32f lambda, int inputOffset, 
                          Ipp64s count, Ipp64s diffSum, int shift, int offsetTh,
                          Ipp32f* bestCost = NULL, Ipp64s* bestDist = NULL)
{
    int iterOffset = inputOffset > 0 ? 7 : -7;
    int outputOffset = 0;
    int testOffset;
    Ipp64s tempDist, tempRate;
    Ipp32f cost, minCost;
    const int deltaRate = (typeIdx == SAO_TYPE_BO) ? 2 : 1;
    
    minCost = lambda * 256;
    while (iterOffset != 0) {
        tempRate = abs(iterOffset) + deltaRate;
        if (abs(iterOffset) == offsetTh) {
            tempRate --;
        }
     
        testOffset  = iterOffset;
        tempDist    = EstimateDeltaDist( count, testOffset, diffSum, shift);
        cost    = (Ipp32f)tempDist + lambda * (Ipp32f) (tempRate << 8);

        if (cost < minCost) {
            minCost = cost;
            outputOffset = iterOffset;
            if (bestDist) *bestDist = tempDist;
            if (bestCost) *bestCost = cost;
        }
        
        iterOffset = (iterOffset > 0) ? (iterOffset-1) : (iterOffset+1);
    }
    return outputOffset;
}

static int tab_numSaoClass[] = {NUM_SAO_EO_CLASSES, NUM_SAO_EO_CLASSES, NUM_SAO_EO_CLASSES, NUM_SAO_EO_CLASSES, NUM_SAO_BO_CLASSES};

void GetQuantOffsets( int typeIdx,  MFX_HEVC_PP::SaoCtuStatistics& statData, int* quantOffsets, int& typeAuxInfo,
                      Ipp32f lambda, int bitDepth, int saoMaxOffsetQval, int shift)
{
    memset(quantOffsets, 0, sizeof(int)*MAX_NUM_SAO_CLASSES);
    
    int numClasses = tab_numSaoClass[typeIdx];
    int classIdx;

    // calculate 'native' offset
    for (classIdx = 0; classIdx < numClasses; classIdx++) {
        if ( 0 == statData.count[classIdx] || (SAO_CLASS_EO_PLAIN == classIdx && SAO_TYPE_BO != typeIdx) ) {
            continue; //offset will be zero
        }

        Ipp32f meanDiff = (Ipp32f)(statData.diff[classIdx] << (bitDepth - 8)) / (Ipp32f)(statData.count[classIdx]);

        int offset;
        if (bitDepth > 8) {
            offset = (int)meanDiff;
            offset += offset >= 0 ? (1<<(bitDepth-8-1)) : -(1<<(bitDepth-8-1));
            offset >>= (bitDepth-8);
        } else {
            offset = (int)floor(meanDiff + 0.5);
        }

        quantOffsets[classIdx] = Saturate(-saoMaxOffsetQval, saoMaxOffsetQval, offset);
    }
    
    // try on to improve a 'native' offset
    if (SAO_TYPE_BO == typeIdx) {
        Ipp32f cost[NUM_SAO_BO_CLASSES];
        for (int classIdx = 0; classIdx < NUM_SAO_BO_CLASSES; classIdx++) {
            cost[classIdx] = lambda;
            if (quantOffsets[classIdx] != 0 ) {
                quantOffsets[classIdx] = GetBestOffset( typeIdx, classIdx, lambda, quantOffsets[classIdx], 
                                                        statData.count[classIdx], statData.diff[classIdx],
                                                        shift, saoMaxOffsetQval, cost + classIdx);
            }
        }
        
        Ipp32f minCost = IPP_MAXABS_32F, bandCost;
        int band, startBand = 0;
        for (band = 0; band < (NUM_SAO_BO_CLASSES - 4 + 1); band++) {
            bandCost  = cost[band  ] + cost[band+1] + cost[band+2] + cost[band+3];

            if (bandCost < minCost) {
                minCost = bandCost;
                startBand = band;
            }
        }

        // clear unused bands
        for (band = 0; band < startBand; band++)
            quantOffsets[band] = 0;

        for (band = startBand + 4; band < NUM_SAO_BO_CLASSES; band++)
            quantOffsets[band] = 0;

        typeAuxInfo = startBand;

    } else { // SAO_TYPE_E[0/45/90/135]
        for (classIdx = 0; classIdx < NUM_SAO_EO_CLASSES; classIdx++) {
            if (SAO_CLASS_EO_FULL_PEAK == classIdx && quantOffsets[classIdx] > 0   ||
                SAO_CLASS_EO_HALF_PEAK == classIdx && quantOffsets[classIdx] > 0   ||
                SAO_CLASS_EO_FULL_VALLEY == classIdx && quantOffsets[classIdx] < 0 ||
                SAO_CLASS_EO_HALF_VALLEY == classIdx && quantOffsets[classIdx] < 0) {
                quantOffsets[classIdx] = 0;
            }

            if ( quantOffsets[classIdx] != 0 ) {
                quantOffsets[classIdx] = GetBestOffset( typeIdx, classIdx, lambda, quantOffsets[classIdx],
                                                        statData.count[classIdx], statData.diff[classIdx], shift, saoMaxOffsetQval);
            }
        }
        typeAuxInfo = 0;
    }
}


void InvertQuantOffsets(int type_idx, int typeAuxInfo, int* dstOffsets, int* srcOffsets)
{
    int codedOffset[MAX_NUM_SAO_CLASSES];

    small_memcpy(codedOffset, srcOffsets, sizeof(int)*MAX_NUM_SAO_CLASSES);
    memset(dstOffsets, 0, sizeof(int)*MAX_NUM_SAO_CLASSES);

    if (type_idx == SAO_TYPE_BO) {
        for (int i = 0; i < 4; i++) {
            int pos = (typeAuxInfo+ i)%NUM_SAO_BO_CLASSES;
            dstOffsets[pos] = codedOffset[pos];// not full range is copied
        }
    } else {//EO    
        for (int i = 0; i < NUM_SAO_EO_CLASSES; i++) {
            dstOffsets[i] = codedOffset[i];
        }
        VM_ASSERT(dstOffsets[SAO_CLASS_EO_PLAIN] == 0); //keep EO plain offset as zero
    }
}


template <typename PixType>
void SplitPlanes( PixType* in, Ipp32s inPitch, PixType* out1, Ipp32s outPitch1, PixType* out2, Ipp32s outPitch2, Ipp32s width, Ipp32s height)
{
    PixType* src = in;
    PixType* dst1 = out1;
    PixType* dst2 = out2;

    for (Ipp32s h = 0; h < height; h++) {
        for (Ipp32s w = 0; w < width; w++) {
            dst1[w] = src[2*w];
            dst2[w] = src[2*w+1];
        }
        src += inPitch;
        dst1 += outPitch1;
        dst2 += outPitch2;
    }
}


void SaoEstimator::GetBestSao_RdoCost(bool* sliceEnabled, SaoCtuParam* mergeList[2], SaoCtuParam* codedParam)
{
    SaoCtuParam &bestParam = *codedParam;
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_CUR], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

    const Ipp32s shift = 2 * (m_bitDepth - 8);
    Ipp32f cost = 0.0;
    Ipp64s dist[NUM_SAO_COMPONENTS] = {0};
    Ipp64s modeDist[NUM_SAO_COMPONENTS] = {0};
    SaoOffsetParam testOffset[NUM_SAO_COMPONENTS];

    bestParam[SAO_Y].mode_idx = SAO_MODE_OFF;
    bestParam[SAO_Y].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_CUR], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    CodeSaoCtbParam(m_bsf, bestParam, sliceEnabled, mergeList[SAO_MERGE_LEFT] != NULL, mergeList[SAO_MERGE_ABOVE] != NULL, true);
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_MID], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

    m_bsf->Reset();
    CodeSaoCtbOffsetParam(m_bsf, SAO_Y, bestParam[SAO_Y], sliceEnabled[SAO_Y]);
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    Ipp32f minCost = m_labmda[SAO_Y] * m_bsf->GetNumBits();

    if (sliceEnabled[SAO_Y]) {
        for (Ipp32s type_idx = 0; type_idx < m_numSaoModes; type_idx++) {
            testOffset[SAO_Y].mode_idx = SAO_MODE_ON;
            testOffset[SAO_Y].type_idx = type_idx;
            testOffset[SAO_Y].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

            GetQuantOffsets(type_idx, m_statData[SAO_Y][type_idx], testOffset[SAO_Y].offset,
                testOffset[SAO_Y].typeAuxInfo, m_labmda[SAO_Y], m_bitDepth, m_saoMaxOffsetQVal, shift);

            dist[SAO_Y] = GetDistortion(testOffset[SAO_Y].type_idx, testOffset[SAO_Y].typeAuxInfo,
                testOffset[SAO_Y].offset, m_statData[SAO_Y][type_idx], shift);

            m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_MID], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
            m_bsf->Reset();
            CodeSaoCtbOffsetParam(m_bsf, SAO_Y, testOffset[SAO_Y], sliceEnabled[SAO_Y]);

            cost = dist[SAO_Y] + m_labmda[SAO_Y]*m_bsf->GetNumBits();
            if (cost < minCost) {
                minCost = cost;
                modeDist[SAO_Y] = dist[SAO_Y];
                small_memcpy(&bestParam[SAO_Y], testOffset + SAO_Y, sizeof(SaoOffsetParam));

                m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
            }
        }
    }
    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_MID], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);


    Ipp32f chromaLambda = m_labmda[SAO_Cb];
    if ( sliceEnabled[SAO_Cb] ) {
        VM_ASSERT(m_labmda[SAO_Cb] == m_labmda[SAO_Cr]);
        VM_ASSERT(sliceEnabled[SAO_Cb] == sliceEnabled[SAO_Cr]);

        m_bsf->Reset();

        bestParam[SAO_Cb].mode_idx = SAO_MODE_OFF;
        bestParam[SAO_Cb].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
        CodeSaoCtbOffsetParam(m_bsf, SAO_Cb, bestParam[SAO_Cb], sliceEnabled[SAO_Cb]);

        bestParam[SAO_Cr].mode_idx = SAO_MODE_OFF;
        bestParam[SAO_Cr].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
        CodeSaoCtbOffsetParam(m_bsf, SAO_Cr, bestParam[SAO_Cr], sliceEnabled[SAO_Cr]);

        minCost= chromaLambda * m_bsf->GetNumBits();
        for (Ipp32s type_idx = 0; type_idx < m_numSaoModes; type_idx++) {
            for (Ipp32s compIdx = SAO_Cb; compIdx < NUM_SAO_COMPONENTS; compIdx++) {
                testOffset[compIdx].mode_idx = SAO_MODE_ON;
                testOffset[compIdx].type_idx = type_idx;
                testOffset[compIdx].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

                GetQuantOffsets(type_idx, m_statData[compIdx][type_idx], testOffset[compIdx].offset,
                    testOffset[compIdx].typeAuxInfo, m_labmda[compIdx], m_bitDepth, m_saoMaxOffsetQVal, shift);

                dist[compIdx]= GetDistortion(type_idx, testOffset[compIdx].typeAuxInfo,
                    testOffset[compIdx].offset, m_statData[compIdx][type_idx], shift);
            }

            m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_MID], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
            m_bsf->Reset();
            CodeSaoCtbOffsetParam(m_bsf, SAO_Cb, testOffset[SAO_Cb], sliceEnabled[SAO_Cb]);
            CodeSaoCtbOffsetParam(m_bsf, SAO_Cr, testOffset[SAO_Cr], sliceEnabled[SAO_Cr]);

            cost = (dist[SAO_Cb]+ dist[SAO_Cr]) + chromaLambda*m_bsf->GetNumBits();
            if (cost < minCost) {
                minCost = cost;
                modeDist[SAO_Cb] = dist[SAO_Cb];
                modeDist[SAO_Cr] = dist[SAO_Cr];
                small_memcpy(&bestParam[SAO_Cb], testOffset + SAO_Cb, 2*sizeof(SaoOffsetParam));
            }
        }
    }
    
    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_CUR], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    m_bsf->Reset();
    CodeSaoCtbParam(m_bsf, bestParam, sliceEnabled, (mergeList[SAO_MERGE_LEFT]!= NULL), (mergeList[SAO_MERGE_ABOVE]!= NULL), false);
    
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

    Ipp32f bestCost = modeDist[SAO_Y] / m_labmda[SAO_Y] + (modeDist[SAO_Cb]+ modeDist[SAO_Cr])/chromaLambda + m_bsf->GetNumBits();

#if defined(SAO_MODE_MERGE_ENABLED)
    int compCount = sliceEnabled[SAO_Cb] ? 3 : 1;
    SaoCtuParam testParam;
    for (Ipp32s mode = 0; mode < 2; mode++) {
        if (NULL == mergeList[mode])
            continue;
        
        small_memcpy(&testParam, mergeList[mode], sizeof(SaoCtuParam));

        Ipp64s distortion=0;
        for (int compIdx = 0; compIdx < compCount; compIdx++) {
            testParam[compIdx].mode_idx = SAO_MODE_MERGE;
            testParam[compIdx].type_idx = mode;

            SaoOffsetParam& mergedOffset = (*(mergeList[mode]))[compIdx];
            if ( SAO_MODE_OFF != mergedOffset.mode_idx ) {
                distortion += GetDistortion( mergedOffset.type_idx, mergedOffset.typeAuxInfo,
                    mergedOffset.offset, m_statData[compIdx][mergedOffset.type_idx], shift);
            }
        }

        m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_CUR], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
        m_bsf->Reset();
        CodeSaoCtbParam(m_bsf, testParam, sliceEnabled, (NULL != mergeList[SAO_MERGE_LEFT]), (NULL != mergeList[SAO_MERGE_ABOVE]), false);

        cost = distortion / m_labmda[0] + m_bsf->GetNumBits();
        if (cost < bestCost) {
            bestCost = cost;
            small_memcpy(&bestParam, &testParam, sizeof(SaoCtuParam));
            m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
        }
    }

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
#endif
}


Ipp32s CodeSaoCtbOffsetParam_BitCost(int compIdx, SaoOffsetParam& ctbParam, bool sliceEnabled)
{
    Ipp32u code;
    Ipp32s bitsCount = 0;
    if (!sliceEnabled) {
        return 0;
    }
    
    if (compIdx == SAO_Y || compIdx == SAO_Cb) {
        if (ctbParam.mode_idx == SAO_MODE_OFF) {
            code =0;
            bitsCount++;
        } else if(ctbParam.type_idx == SAO_TYPE_BO) {
            code = 1;
            bitsCount++;
            bitsCount++;
        } else {
            VM_ASSERT(ctbParam.type_idx < SAO_TYPE_BO); //EO
            code = 2;
            bitsCount += 2;
        }
    }

    if (ctbParam.mode_idx == SAO_MODE_ON) {
        int numClasses = (ctbParam.type_idx == SAO_TYPE_BO)?4:NUM_SAO_EO_CLASSES;
        int offset[5]; // only 4 are used, 5th is added for KW
        int k=0;
        for (int i=0; i< numClasses; i++) {
            if (ctbParam.type_idx != SAO_TYPE_BO && i == SAO_CLASS_EO_PLAIN) {
                continue;
            }
            int classIdx = (ctbParam.type_idx == SAO_TYPE_BO)?(  (ctbParam.typeAuxInfo+i)% NUM_SAO_BO_CLASSES   ):i;
            offset[k] = ctbParam.offset[classIdx];
            k++;
        }

        for (int i=0; i< 4; i++) {

            code = (Ipp32u)( offset[i] < 0) ? (-offset[i]) : (offset[i]);
            if (ctbParam.saoMaxOffsetQVal != 0) {
                Ipp32u i;
                Ipp8u code_last = ((Ipp32u)ctbParam.saoMaxOffsetQVal > code);
                if (code == 0) {
                    bitsCount++;
                } else {
                    bitsCount++;
                    for (i=0; i < code-1; i++) {
                        bitsCount++;
                    }
                    if (code_last) {
                        bitsCount++;
                    }
                }
            }
        }


        if (ctbParam.type_idx == SAO_TYPE_BO) {
            for (int i=0; i< 4; i++) {
                if (offset[i] != 0) {
                    bitsCount++;
                }
            }
            bitsCount += NUM_SAO_BO_CLASSES_LOG2;
        } else {
            if(compIdx == SAO_Y || compIdx == SAO_Cb) {
                VM_ASSERT(ctbParam.type_idx - SAO_TYPE_EO_0 >=0);
                bitsCount +=  NUM_SAO_EO_TYPES_LOG2;
            }
        }
    }

    return bitsCount << 8;
} 


Ipp32s CodeSaoCtbParam_BitCost(SaoCtuParam& saoBlkParam, bool* sliceEnabled, bool leftMergeAvail, bool aboveMergeAvail, bool onlyEstMergeInfo)
{
    bool isLeftMerge = false;
    bool isAboveMerge= false;
    Ipp32u code = 0;
    Ipp32s bitsCount = 0;

    if(leftMergeAvail) {
        isLeftMerge = ((saoBlkParam[SAO_Y].mode_idx == SAO_MODE_MERGE) && (saoBlkParam[SAO_Y].type_idx == SAO_MERGE_LEFT));
        code = isLeftMerge ? 1 : 0;
        bitsCount++;
    }

    if( aboveMergeAvail && !isLeftMerge) {
        isAboveMerge = ((saoBlkParam[SAO_Y].mode_idx == SAO_MODE_MERGE) && (saoBlkParam[SAO_Y].type_idx == SAO_MERGE_ABOVE));
        code = isAboveMerge ? 1 : 0;
        bitsCount++;
    }

    bitsCount <<= 8;
    if(onlyEstMergeInfo) {
        return bitsCount;
    }
    
    if(!isLeftMerge && !isAboveMerge) {
        
        for (int compIdx=0; compIdx < NUM_SAO_COMPONENTS; compIdx++) {
            bitsCount += CodeSaoCtbOffsetParam_BitCost(compIdx, saoBlkParam[compIdx], sliceEnabled[compIdx]);
        }
    }

    return bitsCount;
}


void SaoEstimator::GetBestSao_BitCost(bool* sliceEnabled, SaoCtuParam* mergeList[2], SaoCtuParam* codedParam)
{
    SaoCtuParam &bestParam = *codedParam;

    const Ipp32s shift = 2 * (m_bitDepth - 8);
    Ipp32f cost = 0.0;
    Ipp64s dist[NUM_SAO_COMPONENTS] = {0};
    Ipp64s modeDist[NUM_SAO_COMPONENTS] = {0};
    SaoOffsetParam testOffset[NUM_SAO_COMPONENTS];

    bestParam[SAO_Y].mode_idx = SAO_MODE_OFF;
    bestParam[SAO_Y].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
    
    Ipp32s bitCost = CodeSaoCtbParam_BitCost(bestParam, sliceEnabled, mergeList[SAO_MERGE_LEFT] != NULL, mergeList[SAO_MERGE_ABOVE] != NULL, true);
    
    bitCost = CodeSaoCtbOffsetParam_BitCost(SAO_Y, bestParam[SAO_Y], sliceEnabled[SAO_Y]);
    
    Ipp32f minCost = m_labmda[SAO_Y] * bitCost;

    if (sliceEnabled[SAO_Y]) {
        for (Ipp32s type_idx = 0; type_idx < m_numSaoModes; type_idx++) {
            testOffset[SAO_Y].mode_idx = SAO_MODE_ON;
            testOffset[SAO_Y].type_idx = type_idx;
            testOffset[SAO_Y].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

            GetQuantOffsets(type_idx, m_statData[SAO_Y][type_idx], testOffset[SAO_Y].offset,
                testOffset[SAO_Y].typeAuxInfo, m_labmda[SAO_Y], m_bitDepth, m_saoMaxOffsetQVal, shift);

            dist[SAO_Y] = GetDistortion(testOffset[SAO_Y].type_idx, testOffset[SAO_Y].typeAuxInfo,
                testOffset[SAO_Y].offset, m_statData[SAO_Y][type_idx], shift);
            
            bitCost = CodeSaoCtbOffsetParam_BitCost(SAO_Y, testOffset[SAO_Y], sliceEnabled[SAO_Y]);

            cost = dist[SAO_Y] + m_labmda[SAO_Y]*bitCost;
            if (cost < minCost) {
                minCost = cost;
                modeDist[SAO_Y] = dist[SAO_Y];
                small_memcpy(&bestParam[SAO_Y], testOffset + SAO_Y, sizeof(SaoOffsetParam));
            }
        }
    }

    Ipp32f chromaLambda = m_labmda[SAO_Cb];
    if ( sliceEnabled[SAO_Cb] ) {
        VM_ASSERT(m_labmda[SAO_Cb] == m_labmda[SAO_Cr]);
        VM_ASSERT(sliceEnabled[SAO_Cb] == sliceEnabled[SAO_Cr]);

        bestParam[SAO_Cb].mode_idx = SAO_MODE_OFF;
        bestParam[SAO_Cb].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
        bitCost = CodeSaoCtbOffsetParam_BitCost(SAO_Cb, bestParam[SAO_Cb], sliceEnabled[SAO_Cb]);

        bestParam[SAO_Cr].mode_idx = SAO_MODE_OFF;
        bestParam[SAO_Cr].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
        bitCost += CodeSaoCtbOffsetParam_BitCost(SAO_Cr, bestParam[SAO_Cr], sliceEnabled[SAO_Cr]);

        minCost= chromaLambda * bitCost;
        for (Ipp32s type_idx = 0; type_idx < m_numSaoModes; type_idx++) {
            for (Ipp32s compIdx = SAO_Cb; compIdx < NUM_SAO_COMPONENTS; compIdx++) {
                testOffset[compIdx].mode_idx = SAO_MODE_ON;
                testOffset[compIdx].type_idx = type_idx;
                testOffset[compIdx].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

                GetQuantOffsets(type_idx, m_statData[compIdx][type_idx], testOffset[compIdx].offset,
                    testOffset[compIdx].typeAuxInfo, m_labmda[compIdx], m_bitDepth, m_saoMaxOffsetQVal, shift);

                dist[compIdx]= GetDistortion(type_idx, testOffset[compIdx].typeAuxInfo,
                    testOffset[compIdx].offset, m_statData[compIdx][type_idx], shift);
            }

            bitCost = CodeSaoCtbOffsetParam_BitCost(SAO_Cb, testOffset[SAO_Cb], sliceEnabled[SAO_Cb]);
            bitCost += CodeSaoCtbOffsetParam_BitCost(SAO_Cr, testOffset[SAO_Cr], sliceEnabled[SAO_Cr]);

            cost = (dist[SAO_Cb]+ dist[SAO_Cr]) + chromaLambda*bitCost;
            if (cost < minCost) {
                minCost = cost;
                modeDist[SAO_Cb] = dist[SAO_Cb];
                modeDist[SAO_Cr] = dist[SAO_Cr];
                small_memcpy(&bestParam[SAO_Cb], testOffset + SAO_Cb, 2*sizeof(SaoOffsetParam));
            }
        }
    }
    
    bitCost = CodeSaoCtbParam_BitCost(bestParam, sliceEnabled, (mergeList[SAO_MERGE_LEFT]!= NULL), (mergeList[SAO_MERGE_ABOVE]!= NULL), false);

    Ipp32f bestCost = modeDist[SAO_Y] / m_labmda[SAO_Y] + (modeDist[SAO_Cb]+ modeDist[SAO_Cr])/chromaLambda + bitCost;

#if defined(SAO_MODE_MERGE_ENABLED)
    SaoCtuParam testParam;
    for (Ipp32s mode = 0; mode < 2; mode++) {
        if (NULL == mergeList[mode])
            continue;
        
        small_memcpy(&testParam, mergeList[mode], sizeof(SaoCtuParam));
        testParam[0].mode_idx = SAO_MODE_MERGE;
        testParam[0].type_idx = mode;

        SaoOffsetParam& mergedOffset = (*(mergeList[mode]))[0];

        Ipp64s distortion=0;
        if ( SAO_MODE_OFF != mergedOffset.mode_idx ) {
            distortion = GetDistortion( mergedOffset.type_idx, mergedOffset.typeAuxInfo,
                mergedOffset.offset, m_statData[0][mergedOffset.type_idx], shift);
        }

        bitCost = CodeSaoCtbParam_BitCost(testParam, sliceEnabled, (NULL != mergeList[SAO_MERGE_LEFT]), (NULL != mergeList[SAO_MERGE_ABOVE]), false);

        cost = distortion / m_labmda[0] + bitCost;
        if (cost < bestCost) {
            bestCost = cost;
            small_memcpy(&bestParam, &testParam, sizeof(SaoCtuParam));
        }
    }
#endif

}

template <typename PixType>
SaoApplier<PixType>::SaoApplier()
{
    m_ClipTable     = NULL;
    m_ClipTableBase = NULL;
    m_lumaTableBo   = NULL;
    m_chromaTableBo = NULL;
    m_TmpU          = NULL;
    m_TmpL          = NULL;
    m_TmpU_Chroma   = NULL;
    m_TmpL_Chroma   = NULL;
    m_inited = 0;
}

template <typename PixType>
void SaoApplier<PixType>::Close()
{
    if (m_lumaTableBo) {
        delete [] m_lumaTableBo;
        m_lumaTableBo = NULL;
    }
    if (m_chromaTableBo) {
        delete [] m_chromaTableBo;
        m_chromaTableBo = NULL;
    }
    if (m_ClipTableBase) {
        delete [] m_ClipTableBase;
        m_ClipTableBase = NULL;
    }
    if (m_TmpU) {
        delete[] m_TmpU;
        m_TmpU = NULL;
    }
    if (m_TmpL) {
        delete[] m_TmpL;
        m_TmpL = NULL;
    }
    if (m_TmpU_Chroma) {
        delete[] m_TmpU_Chroma;
        m_TmpU_Chroma = NULL;
    }
    if (m_TmpL_Chroma) {
        delete[] m_TmpL_Chroma;
        m_TmpL_Chroma = NULL;
    }
    m_ClipTable = NULL;
}

template <typename PixType>
SaoApplier<PixType>::~SaoApplier()
{
    Close();
}

template <typename PixType>
void SaoApplier<PixType>::Init(int maxCUWidth, int format, int maxDepth, int bitDepth, int num)
{
    m_bitDepth = bitDepth;
    m_maxCuSize  = maxCUWidth;
    m_format = format;

    // luma
    Ipp32u uiPixelRangeY = 1 << m_bitDepth;
    Ipp32u uiBoRangeShiftY = m_bitDepth - SAO_BO_BITS;
    m_lumaTableBo = new PixType[uiPixelRangeY];
    for (Ipp32u k2 = 0; k2 < uiPixelRangeY; k2++) {
        m_lumaTableBo[k2] = (PixType)(1 + (k2>>uiBoRangeShiftY));
    }

    // chroma
    m_chromaTableBo = new PixType[uiPixelRangeY];
    // we use the same bitDepth for Luma & Chroma
    for (Ipp32u k2 = 0; k2 < uiPixelRangeY; k2++) {
        m_chromaTableBo[k2] = (PixType)(1 + (k2>>uiBoRangeShiftY));
    }

    Ipp32u uiMaxY  = (1 << m_bitDepth) - 1;
    Ipp32u uiMinY  = 0;
    Ipp32u iCRangeExt = uiMaxY>>1;

    m_ClipTableBase = new PixType[uiMaxY+2*iCRangeExt];

    for (Ipp32u i = 0; i < (uiMinY + iCRangeExt);i++) {
        m_ClipTableBase[i] = (PixType)uiMinY;
    }

    for (Ipp32u i = uiMinY + iCRangeExt; i < (uiMaxY + iCRangeExt); i++) {
        m_ClipTableBase[i] = (PixType)(i - iCRangeExt);
    }

    for (Ipp32u i = uiMaxY + iCRangeExt; i < (uiMaxY + 2 * iCRangeExt); i++) {
        m_ClipTableBase[i] = (PixType)uiMaxY;
    }

    m_ClipTable = &(m_ClipTableBase[iCRangeExt]);
    m_ClipTableChroma = &(m_ClipTableBase[iCRangeExt]);

    Ipp32u uiMaxUV  = (1 << m_bitDepth) - 1;
    Ipp32u iCRangeExtUV = uiMaxUV>>1;

    m_TmpU = new PixType [num * maxCUWidth];// index 0
    m_TmpL = new PixType [num * maxCUWidth];

    // for chroma we need to use complex formula for size of CTB
    m_ctbCromaWidthInPix  = (m_format == MFX_CHROMAFORMAT_YUV444) ? 2*maxCUWidth : maxCUWidth;
    m_ctbCromaHeightInPix = (m_format == MFX_CHROMAFORMAT_YUV420) ? maxCUWidth : 2*maxCUWidth;
    m_TmpU_Chroma = new PixType [num * m_ctbCromaWidthInPix];
    m_TmpL_Chroma = new PixType [num * m_ctbCromaHeightInPix];

    m_ctbCromaHeightInRow = m_ctbCromaHeightInPix >> 1;

    m_inited = 1;
}


template <typename PixType>
void SaoApplier<PixType>::SetOffsetsLuma(SaoOffsetParam  &saoLCUParam, Ipp32s typeIdx, Ipp32s *offsetEo, PixType *offsetBo)
{
    Ipp32s offset[LUMA_GROUP_NUM + 1] = {0};
    static const Ipp8u EoTable[9] = { 1,  2,   0,  3,  4,  0,  0,  0,  0 };

    if (typeIdx == SAO_TYPE_BO) {
        for (Ipp32s i = 0; i < NUM_SAO_BO_CLASSES + 1; i++) {
            offset[i] = 0;
        }
        for (Ipp32s i = 0; i < 4; i++) {
            offset[(saoLCUParam.typeAuxInfo + i) % NUM_SAO_BO_CLASSES + 1] = saoLCUParam.offset[(saoLCUParam.typeAuxInfo + i) % NUM_SAO_BO_CLASSES];//[i];
        }

        PixType *ppLumaTable = m_lumaTableBo;
        for (Ipp32s i = 0; i < (1 << m_bitDepth); i++) {
            offsetBo[i] = m_ClipTable[i + offset[ppLumaTable[i]]];
        }
    } else if (typeIdx == SAO_TYPE_EO_0 || typeIdx == SAO_TYPE_EO_90 || typeIdx == SAO_TYPE_EO_135 || typeIdx == SAO_TYPE_EO_45) {
        for (Ipp32s edgeType = 0; edgeType < 6; edgeType++) {
            offsetEo[edgeType] = saoLCUParam.offset[edgeType];
        }
    }
}

void ReconstructSao(SaoCtuParam* mergeList[2], SaoCtuParam& recParam, Ipp8u isChroma)
{
    int compCount = isChroma ? 3 : 1;
    for (int compIdx = 0; compIdx < compCount; compIdx++) {
        SaoOffsetParam& offsetParam = recParam[compIdx];

        if ( SAO_MODE_ON == offsetParam.mode_idx ) {
            InvertQuantOffsets(offsetParam.type_idx, offsetParam.typeAuxInfo, offsetParam.offset, offsetParam.offset);
        } else if ( SAO_MODE_MERGE == offsetParam.mode_idx ) { // were reconstructed early
            SaoCtuParam* mergeTarget = mergeList[offsetParam.type_idx];
            VM_ASSERT(mergeTarget != NULL);
            small_memcpy((void*)&offsetParam, (void*)&(*mergeTarget)[compIdx], sizeof(SaoOffsetParam));
        }
    }
}


inline Ipp16s getSign(Ipp32s x)
{
    return ((x >> 31) | ((Ipp32s)( (((Ipp32u) -x)) >> 31)));
}

void h265_GetCtuStatistics_8u_fast(
    int compIdx, 
    const Ipp8u* recBlk, int recStride, 
    const Ipp8u* orgBlk, int orgStride, 
    int width, int height, int shift,  
    CTBBorders& borders, 
    int numSaoModes, 
    SaoCtuStatistics* statsDataTypes)       
    {
        enum SAOType
{
    SAO_EO_0 = 0,
    SAO_EO_1,
    SAO_EO_2,
    SAO_EO_3,
    SAO_BO,
    MAX_NUM_SAO_TYPE
};

        Ipp16s signLineBuf1[64+1];
        Ipp16s signLineBuf2[64+1];

        int x, y, startX, startY, endX, endY;
        int firstLineStartX, firstLineEndX;
        int edgeType;
        Ipp16s signLeft, signRight, signDown;
        Ipp64s *diff, *count;        

        const Ipp8u *recLine, *orgLine;
        const Ipp8u* recLineAbove;
        const Ipp8u* recLineBelow;


        for(int typeIdx=0; typeIdx< numSaoModes; typeIdx++)
        {
            SaoCtuStatistics& statsData= statsDataTypes[typeIdx];
            statsData.Reset();

            recLine = recBlk;
            orgLine = orgBlk;
            diff    = statsData.diff;
            count   = statsData.count;
            switch(typeIdx)
            {
#if 1
            case SAO_EO_0://SAO_TYPE_EO_0:
                {
                    diff +=2;
                    count+=2;
                    endY   = height;

                    startX = 0;//borders.m_left ? 0 : 1;
                    endX   = width;// - (borders.m_right ? 0 : 1);//skipLinesR;

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
                // DEBUG!!!
                //return;
                break;

            case SAO_EO_1: //SAO_TYPE_EO_90:
                {
                    int signUp;
                    diff +=2;
                    count+=2;                    
#if 0
                    for (y=0; y<height; y++)
                    {
                        recLineAbove = recLine - recStride;
                        recLineBelow = recLine + recStride;

                        for (x=0; x<width; x++)
                        {
                            signDown  = getSign(recLine[x] - recLineBelow[x]); 
                            signUp    = getSign(recLine[x] - recLineAbove[x]); 
                            edgeType  = signDown + signUp;                           

                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;
                        }
                        recLine += recStride;
                        orgLine += orgStride;
                    }
#else
                    const int stepY = 8;
                    const int stepX = 16;
                    int blk = 0;
                    int diff_8x16[5] = {0};
                    int mat_origin[8][16];
                    for (int gy=0; gy<height; gy += stepY)
                    {
                        for (int gx=0; gx<width; gx += stepX) {
                            diff_8x16[0] = diff_8x16[1] = diff_8x16[3] = diff_8x16[4] = 0;

                            for(y = 0; y < stepY; y++) {
                                recLine = recBlk + (gy+y) * recStride;
                                orgLine = orgBlk + (gy+y) * orgStride;
                                recLineAbove = recLine - recStride;
                                recLineBelow = recLine + recStride;

                                for(x = 0; x < stepX; x++) {                                                                                                       

                                    signDown  = getSign(recLine[gx + x] - recLineBelow[gx + x]); 
                                    signUp    = getSign(recLine[gx + x] - recLineAbove[gx + x]); 
                                    edgeType  = signDown + signUp;                           

                                    diff [edgeType] += (orgLine[gx+x] - recLine[gx+x]);
                                    count[edgeType] ++;

                                    diff_8x16[2 + edgeType] += (orgLine[gx+x] - recLine[gx+x]);
                                    mat_origin[y][x] = orgLine[gx+x];
                                }
                                //recLine += recStride;
                                //orgLine += orgStride;
                            }

                            /*if (ctbAddr == 2 && blk < 5) {
                                printf("\n CPU blk %i E90_diff %i, %i, %i, %i \n", blk, diff_8x16[0], diff_8x16[1], diff_8x16[3], diff_8x16[4]);           

                                if (blk == 2) {
                                    for (int y1 = 0; y1 < 8; y1++) {
                                        for(int x1 = 0; x1 < 16; x1++) {
                                            printf(" %i ", mat_origin[y1][x1]);
                                        }
                                        printf("\n");
                                    }
                                }
                            }
                            blk++;*/
                        }
                    }
#endif
                }
                break;
#endif

#if 1
            case SAO_EO_2: //SAO_TYPE_EO_135:
                {
                    diff +=2;
                    count+=2;
                    startX = 0;
                    endX   = width;
                    endY   = height;                                        
                    
                    for (y=0; y<endY; y++)
                    {
                        recLineAbove = recLine - recStride;
                        recLineBelow = recLine + recStride;                        

                        for (x=startX; x<endX; x++)
                        {
                            int signUp = getSign(recLine[x] - recLineAbove[x-1]);
                            signDown = getSign(recLine[x] - recLineBelow[x+1]);
                            edgeType = signDown + signUp;
                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;                         
                        }                    
                                               
                        recLine += recStride;
                        orgLine += orgStride;
                    }                    
                }
                break;

            case SAO_EO_3: //SAO_TYPE_EO_45:
                {
                    diff +=2;
                    count+=2;
                    endY   = height;

                    startX = 0;
                    endX   = width;
                    
                    for (y=0; y<endY; y++)
                    {
                        recLineBelow = recLine + recStride;                        
                        recLineAbove = recLine - recStride;                        

                        for (x = startX; x<endX; x++)
                        {
                            signDown = getSign(recLine[x] - recLineBelow[x-1]) ;
                            int signUp = getSign(recLine[x] - recLineAbove[x+1]) ;
                            edgeType = signDown + signUp;

                            diff [edgeType] += (orgLine[x] - recLine[x]);
                            count[edgeType] ++;                            
                        }                        
                        recLine  += recStride;
                        orgLine  += orgStride;
                    }
                }
                break;
#endif

#if 1
            case SAO_BO: //SAO_TYPE_BO:
                {
                    endX = width;//- skipLinesR;
                    endY = height;//- skipLinesB;

                    // TO CMP WITH DEF
                    /*{
                        endX--;
                        endY--;
                    }*/

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
#endif

            default:
                {
                    //VM_ASSERT(!"Not a supported SAO types\n");
                }
            }
        }

    } 


template <typename PixType>
void H265CU<PixType>::EstimateSao(H265BsReal* bs, SaoCtuParam* saoParam)
{
    m_saoEst.m_bitDepth = m_par->bitDepthLuma;// need to fix in case of chroma != luma bitDepth
    m_saoEst.m_saoMaxOffsetQVal = (1<<(MIN(m_saoEst.m_bitDepth,10)-5))-1;

    m_saoEst.m_numSaoModes = (m_par->saoOpt == SAO_OPT_ALL_MODES) ? NUM_SAO_BASE_TYPES : ((m_par->saoOpt == SAO_OPT_FAST_MODES_ONLY)? NUM_SAO_BASE_TYPES - 1: 1);

    MFX_HEVC_PP::CTBBorders borders = {0};
    borders.m_left = (-1 == m_leftAddr)  ? 0 : (m_par->m_slice_ids[m_ctbAddr] == m_par->m_slice_ids[m_leftAddr]  && m_leftSameTile) ? 1 : 0;
    borders.m_top  = (-1 == m_aboveAddr) ? 0 : (m_par->m_slice_ids[m_ctbAddr] == m_par->m_slice_ids[m_aboveAddr] && m_aboveSameTile) ? 1 : 0;

    SaoCtuParam* mergeList[2] = {NULL, NULL};
    if (borders.m_top) {
        mergeList[SAO_MERGE_ABOVE] = &(saoParam[m_aboveAddr]);
    }
    if (borders.m_left) {
        mergeList[SAO_MERGE_LEFT] = &(saoParam[m_leftAddr]);
    }

    //   workaround (better to rewrite optimized SAO estimate functions to use tmpU and tmpL)
    borders.m_left = 0;
    borders.m_top  = 0;

    Ipp32s height = (m_ctbPelY + m_par->MaxCUSize > m_par->Height) ? (m_par->Height- m_ctbPelY) : m_par->MaxCUSize;
    Ipp32s width  = (m_ctbPelX + m_par->MaxCUSize  > m_par->Width) ? (m_par->Width - m_ctbPelX) : m_par->MaxCUSize;

    Ipp32s offset = 0;// YV12/NV12 Chroma dispatcher
    //h265_GetCtuStatistics_8u_fast(SAO_Y, (Ipp8u*)m_yRec, m_pitchSrcLuma, (Ipp8u*)m_ySrc, m_pitchSrcLuma, width, height, offset, borders, m_saoEst.m_numSaoModes, m_saoEst.m_statData[SAO_Y]);
    h265_GetCtuStatistics(SAO_Y, (PixType*)m_yRec, m_pitchSrcLuma, (PixType*)m_ySrc, m_pitchSrcLuma, width, height, offset, borders, m_saoEst.m_numSaoModes, m_saoEst.m_statData[SAO_Y]);

    if (m_par->SAOChromaFlag) {
        const Ipp32s  planePitch = 64;
        __ALIGN32 PixType reconU[64*64];
        __ALIGN32 PixType reconV[64*64];
        __ALIGN32 PixType originU[64*64];
        __ALIGN32 PixType originV[64*64];

        // Chroma - interleaved format
        Ipp32s shiftW = 1;
        Ipp32s shiftH = 1;
        if (m_par->chromaFormatIdc == MFX_CHROMAFORMAT_YUV422) {
            shiftH = 0;
        } else if (m_par->chromaFormatIdc == MFX_CHROMAFORMAT_YUV444) {
            shiftH = 0;
            shiftW = 0;
        }    
        width  >>= shiftW;
        height >>= shiftH;

        h265_SplitChromaCtb( (PixType*)m_uvRec, m_pitchRecChroma, reconU,  planePitch, reconV,  planePitch, m_par->MaxCUSize >> shiftW, height);
        h265_SplitChromaCtb( (PixType*)m_uvSrc, m_pitchSrcChroma, originU, planePitch, originV, planePitch, m_par->MaxCUSize >> shiftW, height);

        Ipp32s compIdx = 1;
        h265_GetCtuStatistics(compIdx, reconU, planePitch, originU, planePitch, width, height, offset, borders, m_saoEst.m_numSaoModes, m_saoEst.m_statData[compIdx]);
        compIdx = 2;
        h265_GetCtuStatistics(compIdx, reconV, planePitch, originV, planePitch, width, height, offset, borders, m_saoEst.m_numSaoModes, m_saoEst.m_statData[compIdx]);
    }


    m_saoEst.m_bsf = m_bsf;
    m_saoEst.m_labmda[0] = m_rdLambda;
    m_saoEst.m_labmda[1] = m_saoEst.m_labmda[2] = m_saoEst.m_labmda[0];

    bool sliceEnabled[NUM_SAO_COMPONENTS] = {true, false, false};
    if (m_par->SAOChromaFlag)
        sliceEnabled[1] = sliceEnabled[2] = true;

    //m_saoEst.GetBestSao_BitCost(sliceEnabled, mergeList, &saoParam[m_ctbAddr]);
    m_saoEst.GetBestSao_RdoCost(sliceEnabled, mergeList, &saoParam[m_ctbAddr]);
}


template <typename PixType>
void H265CU<PixType>::PackSao(H265BsReal* bs, SaoCtuParam* saoParam)
{
    MFX_HEVC_PP::CTBBorders borders = {0};
    borders.m_left = (-1 == m_leftAddr)  ? 0 : (m_par->m_slice_ids[m_ctbAddr] == m_par->m_slice_ids[m_leftAddr]  && m_leftSameTile) ? 1 : 0;
    borders.m_top  = (-1 == m_aboveAddr) ? 0 : (m_par->m_slice_ids[m_ctbAddr] == m_par->m_slice_ids[m_aboveAddr] && m_aboveSameTile) ? 1 : 0;

    SaoCtuParam* mergeList[2] = {NULL, NULL};
    if (borders.m_top) {
        mergeList[SAO_MERGE_ABOVE] = &(saoParam[m_aboveAddr]);
    }
    if (borders.m_left) {
        mergeList[SAO_MERGE_LEFT] = &(saoParam[m_leftAddr]);
    }

    bool sliceEnabled[NUM_SAO_COMPONENTS] = {true, false, false};
    if (m_par->SAOChromaFlag)
        sliceEnabled[1] = sliceEnabled[2] = true;

    if (!m_cslice->slice_sao_luma_flag)
        m_cslice->slice_sao_luma_flag = sliceEnabled[SAO_Y] ? 1 : 0;
    if (!m_cslice->slice_sao_chroma_flag)
        m_cslice->slice_sao_chroma_flag = (sliceEnabled[SAO_Cb] || sliceEnabled[SAO_Cr] ) ? 1 : 0;

    EncodeSao(bs, 0, 0, 0, saoParam[m_ctbAddr], mergeList[SAO_MERGE_LEFT] != NULL, mergeList[SAO_MERGE_ABOVE] != NULL);
    ReconstructSao(mergeList, saoParam[m_ctbAddr], m_par->SAOChromaFlag);
}


enum SAOType
{
    SAO_EO_0 = 0,
    SAO_EO_1,
    SAO_EO_2,
    SAO_EO_3,
    SAO_BO,
    MAX_NUM_SAO_TYPE
};

//inline Ipp16s getSign(Ipp32s x)
//{
//    return ((x >> 31) | ((Ipp32s)( (((Ipp32u) -x)) >> 31)));
//}


template <typename PixType>
void SaoApplier<PixType>::h265_ProcessSaoCuChroma(PixType* pRec, Ipp32s stride, Ipp32s saoType, PixType* tmpL, PixType* tmpU, Ipp32u maxCUWidth, 
    Ipp32u maxCUHeight, Ipp32s picWidth, Ipp32s picHeight, Ipp32s* pOffsetEoCb, PixType* pOffsetBoCb, Ipp32s* pOffsetEoCr,
    PixType* pOffsetBoCr, PixType* pClipTable, Ipp32u CUPelX, Ipp32u CUPelY) 
{
    Ipp32s tmpUpBuff1[2*65];
    Ipp32s tmpUpBuff2[2*65];

    Ipp32s signLeftCb, signLeftCr;
    Ipp32s signRightCb, signRightCr;
    Ipp32s signDownCb, signDownCr;
    Ipp32u edgeTypeCb, edgeTypeCr;

    Ipp32s LCUWidth  = maxCUWidth;
    Ipp32s LCUHeight = maxCUHeight;
    Ipp32s LPelX     = CUPelX;
    Ipp32s TPelY     = CUPelY;

    Ipp32s RPelX;
    Ipp32s BPelY;
    Ipp32s signLeft;
    Ipp32s signRight;
    Ipp32s signDown;
    Ipp32s signDown1;
    Ipp32u edgeType;
    Ipp32s picWidthTmp;
    Ipp32s picHeightTmp;
    Ipp32s startX;
    Ipp32s startY;
    Ipp32s endX;
    Ipp32s endY;
    Ipp32s x, y;

    picWidthTmp  = picWidth;
    picHeightTmp = picHeight;
    LCUWidth     = LCUWidth;
    LCUHeight    = LCUHeight;
    LPelX        = LPelX;
    TPelY        = TPelY;
    RPelX        = LPelX + LCUWidth;
    BPelY        = TPelY + LCUHeight;
    RPelX        = RPelX > picWidthTmp  ? picWidthTmp  : RPelX;
    BPelY        = BPelY > picHeightTmp ? picHeightTmp : BPelY;
    LCUWidth     = RPelX - LPelX;
    LCUHeight    = BPelY - TPelY;


    switch (saoType) {
        // passed if LCUHeight == CtbHeightInPix/2 && LCUWidth == CtbWidthInPix
    case SAO_EO_0: // dir: -
        {
            startX = (LPelX == 0) ? 2 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth-2 : LCUWidth;

            for (y = 0; y < LCUHeight; y++) {
                signLeftCb = getSign(pRec[startX] - tmpL[y * 2]);
                signLeftCr = getSign(pRec[startX + 1] - tmpL[y * 2 + 1]);

                for (x = startX; x < endX; x += 2) {
                    signRightCb = getSign(pRec[x] - pRec[x + 2]);
                    signRightCr = getSign(pRec[x + 1] - pRec[x + 3]);
                    edgeTypeCb = signRightCb + signLeftCb + 2;
                    edgeTypeCr = signRightCr + signLeftCr + 2;
                    signLeftCb = -signRightCb;
                    signLeftCr = -signRightCr;

                    pRec[x] = pClipTable[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTable[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];

                }
                pRec += stride;
            }
            break;
        }

        // passed if LCUHeight == CtbHeightInPix/2 && LCUWidth == CtbWidthInPix
    case SAO_EO_1: // dir: |
        {
            startY = (TPelY == 0) ? 1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (TPelY == 0) {
                pRec += stride;
            }

            for (x = 0; x < LCUWidth; x++) {
                tmpUpBuff1[x] = getSign(pRec[x] - tmpU[x]);
            }

            for (y = startY; y < endY; y++) {
                for (x = 0; x < LCUWidth; x += 2) {
                    signDownCb = getSign(pRec[x] - pRec[x + stride]);
                    signDownCr = getSign(pRec[x + 1] - pRec[x + 1 + stride]);
                    edgeTypeCb = signDownCb + tmpUpBuff1[x] + 2;
                    edgeTypeCr = signDownCr + tmpUpBuff1[x + 1] + 2;
                    tmpUpBuff1[x] = -signDownCb;
                    tmpUpBuff1[x + 1] = -signDownCr;

                    pRec[x] = pClipTable[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTable[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
                pRec += stride;
            }
            break;
        }

        // passed if LCUHeight == CtbHeightInPix/2 && LCUWidth == CtbWidthInPix
    case SAO_EO_2: // dir: 135
        {
            Ipp32s *pUpBuff = tmpUpBuff1;
            Ipp32s *pUpBufft = tmpUpBuff2;
            Ipp32s *swapPtr;

            startX = (LPelX == 0)           ? 2 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 2 : LCUWidth;

            startY = (TPelY == 0) ?            1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (TPelY == 0) {
                pRec += stride;
            }

            for (x = startX; x < endX; x++) {
                pUpBuff[x] = getSign(pRec[x] - tmpU[x-2]);
            }

            for (y = startY; y < endY; y++) {
                pUpBufft[startX] = getSign(pRec[stride + startX] - tmpL[y * 2]);
                pUpBufft[startX + 1] = getSign(pRec[stride + startX + 1] - tmpL[y * 2 + 1]);

                for (x = startX; x < endX; x += 2) {
                    signDownCb = getSign(pRec[x] - pRec[x + stride + 2]);
                    signDownCr = getSign(pRec[x + 1] - pRec[x + stride + 3]);
                    edgeTypeCb = signDownCb + pUpBuff[x] + 2;
                    edgeTypeCr = signDownCr + pUpBuff[x + 1] + 2;
                    pUpBufft[x + 2] = -signDownCb;
                    pUpBufft[x + 3] = -signDownCr;
                    pRec[x] = pClipTable[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTable[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }

                swapPtr  = pUpBuff;
                pUpBuff  = pUpBufft;
                pUpBufft = swapPtr;

                pRec += stride;
            }
            break;
        }

    case SAO_EO_3: // dir: 45
        {
            startX = (LPelX == 0) ? 2 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 2 : LCUWidth;

            startY = (TPelY == 0) ? 1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (startY == 1) {
                pRec += stride;
            }

            for (x = startX - 2; x < endX; x++) {
                tmpUpBuff1[x+2] = getSign(pRec[x] - tmpU[x+2]);
            }

            for (y = startY; y < endY; y++) {
                x = startX;
                signDownCb = getSign(pRec[x] - tmpL[y * 2 + 2]);
                signDownCr = getSign(pRec[x + 1] - tmpL[y * 2 + 3]);
                edgeTypeCb =  signDownCb + tmpUpBuff1[x + 2] + 2;
                edgeTypeCr =  signDownCr + tmpUpBuff1[x + 3] + 2;
                tmpUpBuff1[x] = -signDownCb;
                tmpUpBuff1[x + 1] = -signDownCr;
                pRec[x] = pClipTable[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                pRec[x + 1] = pClipTable[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];

                for (x = startX + 2; x < endX; x += 2) {
                    signDownCb = getSign(pRec[x] - pRec[x + stride - 2]);
                    signDownCr = getSign(pRec[x + 1] - pRec[x + stride - 1]);
                    edgeTypeCb =  signDownCb + tmpUpBuff1[x + 2] + 2;
                    edgeTypeCr =  signDownCr + tmpUpBuff1[x + 3] + 2;
                    tmpUpBuff1[x] = -signDownCb;
                    tmpUpBuff1[x + 1] = -signDownCr;
                    pRec[x] = pClipTable[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTable[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }

                tmpUpBuff1[endX] = getSign(pRec[endX - 2 + stride] - pRec[endX]);
                tmpUpBuff1[endX + 1] = getSign(pRec[endX - 1 + stride] - pRec[endX + 1]);

                pRec += stride;
            }
            break;
        }
    case SAO_BO:
        {
            for (y = 0; y < LCUHeight; y++) {
                for (x = 0; x < LCUWidth; x += 2) {
                    pRec[x] = pOffsetBoCb[pRec[x]];
                    pRec[x + 1] = pOffsetBoCr[pRec[x + 1]];
                }
                pRec += stride;
            }
            break;
        }
    default: break;
    }
} 


template class H265CU<Ipp8u>;
template class H265CU<Ipp16u>;
template class SaoApplier<Ipp8u>;
template class SaoApplier<Ipp16u>;

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
