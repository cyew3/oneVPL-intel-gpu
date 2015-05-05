//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <math.h>
#include "mfx_h265_sao_filter.h"
#include "mfx_h265_dispatcher.h"
#include "mfx_h265_ctb.h"
#include "mfx_h265_enc.h"

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
inline int GetBestOffset( int typeIdx, int classIdx, Ipp64f lambda, int inputOffset, 
                          Ipp64s count, Ipp64s diffSum, int shift, int offsetTh,
                          Ipp64f* bestCost = NULL, Ipp64s* bestDist = NULL)
{
    int iterOffset = inputOffset;
    int outputOffset = 0;
    int testOffset;
    Ipp64s tempDist, tempRate;
    Ipp64f cost, minCost;
    const int deltaRate = (typeIdx == SAO_TYPE_BO) ? 2 : 1;
    
    minCost = lambda;
    while (iterOffset != 0) {
        tempRate = abs(iterOffset) + deltaRate;
        if (abs(iterOffset) == offsetTh) {
            tempRate --;
        }
     
        testOffset  = iterOffset;
        tempDist    = EstimateDeltaDist( count, testOffset, diffSum, shift);
        cost    = (Ipp64f)tempDist + lambda * (Ipp64f) tempRate;

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
                      Ipp64f lambda, int bitDepth, int saoMaxOffsetQval, int shift)
{
    memset(quantOffsets, 0, sizeof(int)*MAX_NUM_SAO_CLASSES);
    
    int numClasses = tab_numSaoClass[typeIdx];
    int classIdx;

    // calculate 'native' offset
    for (classIdx = 0; classIdx < numClasses; classIdx++) {
        if ( 0 == statData.count[classIdx] || (SAO_CLASS_EO_PLAIN == classIdx && SAO_TYPE_BO != typeIdx) ) {
            continue; //offset will be zero
        }

        Ipp64f meanDiff = (Ipp64f)(statData.diff[classIdx] << (bitDepth - 8)) / (Ipp64f)(statData.count[classIdx]);

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
        Ipp64f cost[NUM_SAO_BO_CLASSES];
        for (int classIdx = 0; classIdx < NUM_SAO_BO_CLASSES; classIdx++) {
            cost[classIdx] = lambda;
            if (quantOffsets[classIdx] != 0 ) {
                quantOffsets[classIdx] = GetBestOffset( typeIdx, classIdx, lambda, quantOffsets[classIdx], 
                                                        statData.count[classIdx], statData.diff[classIdx],
                                                        shift, saoMaxOffsetQval, cost + classIdx);
            }
        }
        
        Ipp64f minCost = MAX_DOUBLE, bandCost;
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


void SaoEstimator::ReconstructCtuSaoParam(SaoCtuParam& recParam)
{
    SaoCtuParam* mergeList[2] = {NULL, NULL};
    GetMergeList(m_ctb_addr, mergeList);

    int compCount = m_saoChroma ? 3 : 1;
    for (int compIdx = 0; compIdx < compCount; compIdx++) {
        SaoOffsetParam& offsetParam = recParam[compIdx];

        if ( SAO_MODE_ON == offsetParam.mode_idx ) {
            InvertQuantOffsets(offsetParam.type_idx, offsetParam.typeAuxInfo, offsetParam.offset, offsetParam.offset);
        } else if ( SAO_MODE_MERGE == offsetParam.mode_idx ) { // were reconstructed early
            SaoCtuParam* mergeTarget = mergeList[offsetParam.type_idx];
            VM_ASSERT(mergeTarget != NULL);
            offsetParam = (*mergeTarget)[compIdx];
        }
    }

}


// SAO Offset
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


SaoOffsetParam& SaoOffsetParam::operator= (const SaoOffsetParam& src)
{
    mode_idx = src.mode_idx;
    type_idx = src.type_idx;
    typeAuxInfo = src.typeAuxInfo;
    saoMaxOffsetQVal = src.saoMaxOffsetQVal;
    small_memcpy(offset, src.offset, sizeof(int)* MAX_NUM_SAO_CLASSES);

    return *this;
}

// SaoCtuParam
SaoCtuParam::SaoCtuParam()
{
    Reset();
}

SaoCtuParam::~SaoCtuParam()
{
}

void SaoCtuParam::Reset()
{
    for (int compIdx = 0; compIdx < 3; compIdx++){
        m_offsetParam[compIdx].Reset();
    }
}


SaoCtuParam& SaoCtuParam::operator= (const SaoCtuParam& src)
{
    for (int compIdx = 0; compIdx < 3; compIdx++){
        m_offsetParam[compIdx] = src.m_offsetParam[compIdx];
    }
    return *this;

}


SaoEstimator::SaoEstimator()
{
}

void SaoEstimator::Close()
{
}

SaoEstimator::~SaoEstimator()
{
    Close();
}

void SaoEstimator::Init(int width, int height, int maxCUWidth, int maxDepth, int bitDepth, int saoOpt, int saoChroma, int chromaFormat)
{
    m_frameSize.width = width;
    m_frameSize.height= height;

    m_maxCuSize = maxCUWidth;

    m_bitDepth = bitDepth;
    m_saoMaxOffsetQVal = (1<<(MIN(m_bitDepth,10)-5))-1;
    m_chromaFormat = chromaFormat;
    m_saoChroma = saoChroma;

    m_numCTU_inWidth = (m_frameSize.width  + m_maxCuSize - 1) / m_maxCuSize;
    m_numCTU_inHeight= (m_frameSize.height + m_maxCuSize - 1) / m_maxCuSize;
#ifndef AMT_SAO_MIN
    m_numSaoModes = (saoOpt == SAO_OPT_ALL_MODES) ? NUM_SAO_BASE_TYPES : NUM_SAO_BASE_TYPES - 1;
#else
    m_numSaoModes = (saoOpt == SAO_OPT_ALL_MODES) ? NUM_SAO_BASE_TYPES : ((saoOpt == SAO_OPT_FAST_MODES_ONLY)? NUM_SAO_BASE_TYPES - 1: 1);
#endif
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

template <typename PixType>
void SaoEstimator::GetCtuSaoStatistics(FrameData* origin, FrameData* recon)
{
    int xPos   = m_ctb_pelx;
    int yPos   = m_ctb_pely;
    int height = (yPos + m_maxCuSize > m_frameSize.height)?(m_frameSize.height- yPos):m_maxCuSize;
    int width  = (xPos + m_maxCuSize  > m_frameSize.width )?(m_frameSize.width - xPos):m_maxCuSize;

    int compIdx = SAO_Y;
    int offset   = 0;// YV12/NV12 Chroma dispatcher
    int  recStride = recon->pitch_luma_pix;
    PixType* recBlk=  (PixType*)recon->y;
    int  orgStride  = origin->pitch_luma_pix;
    PixType* orgBlk = (PixType*)origin->y;

    h265_GetCtuStatistics(compIdx, recBlk, recStride, orgBlk, orgStride, width, height, offset, m_borders, m_numSaoModes, m_statData[compIdx]);

    if (m_saoChroma == 0)
        return;

    const Ipp32s  planePitch = 64;
    const Ipp32s size = 64*64;
    PixType  workBuf[64*64*4];
    PixType* reconU  = workBuf;
    PixType* reconV  = workBuf + 1*size;
    PixType* originU = workBuf + 2*size;
    PixType* originV = workBuf + 3*size;

    // Chroma - interleaved format
    Ipp32s shiftW = 1;
    Ipp32s shiftH = 1;
    if (m_chromaFormat == MFX_CHROMAFORMAT_YUV422) {
        shiftH = 0;
    } else if (m_chromaFormat == MFX_CHROMAFORMAT_YUV444) {
        shiftH = 0;
        shiftW = 0;
    }    
    width  >>= shiftW;
    height >>= shiftH;

    SplitPlanes( (PixType*)recon->uv, recon->pitch_chroma_pix, reconU, planePitch, reconV, planePitch, width, height);
    SplitPlanes( (PixType*)origin->uv, origin->pitch_chroma_pix, originU, planePitch, originV, planePitch, width, height);

    compIdx = 1;
    h265_GetCtuStatistics(compIdx, reconU, planePitch, originU, planePitch, width, height, offset, m_borders, m_numSaoModes, m_statData[compIdx]);
    compIdx = 2;
    h265_GetCtuStatistics(compIdx, reconV, planePitch, originV, planePitch, width, height, offset, m_borders, m_numSaoModes, m_statData[compIdx]);
}


void SaoEstimator::GetBestCtuSaoParam( bool* sliceEnabled, SaoCtuParam* codedParam)
{
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_CUR], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

    SaoCtuParam* mergeList[2] = {NULL, NULL};
    GetMergeList(m_ctb_addr, mergeList);

    SaoCtuParam modeParam;
    Ipp64f minCost = MAX_DOUBLE, modeCost = MAX_DOUBLE;

    ModeDecision_Base( mergeList, sliceEnabled, modeParam, modeCost, SAO_CABACSTATE_BLK_CUR);

    if (modeCost < minCost) {
        minCost = modeCost;
        *codedParam = modeParam;
        m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_NEXT], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    }

#if defined(SAO_MODE_MERGE_ENABLED)
    ModeDecision_Merge( mergeList, sliceEnabled, modeParam, modeCost, SAO_CABACSTATE_BLK_CUR);

    if(modeCost < minCost) {
        minCost = modeCost;
        *codedParam = modeParam;
        m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_NEXT], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    }
#endif
}


void SaoEstimator::GetMergeList(int ctu, SaoCtuParam* mergeList[2])
{
    int ctuX = ctu % m_numCTU_inWidth;
    int ctuY = ctu / m_numCTU_inWidth;
    int mergedCTUPos;

    if (ctuY > 0) {
        mergedCTUPos = ctu- m_numCTU_inWidth;        
        if ( m_slice_ids[ctu] == m_slice_ids[mergedCTUPos] ) {
            mergeList[SAO_MERGE_ABOVE] = &(m_codedParams_TotalFrame[mergedCTUPos]);
        }
    }
    if (ctuX > 0) {
        mergedCTUPos = ctu- 1;
        if ( m_slice_ids[ctu] == m_slice_ids[mergedCTUPos] ) {
            mergeList[SAO_MERGE_LEFT] = &(m_codedParams_TotalFrame[mergedCTUPos]);
        }
    }
}

void SaoEstimator::ModeDecision_Merge( SaoCtuParam* mergeList[2], bool* sliceEnabled,
                                         SaoCtuParam& bestParam, Ipp64f& bestCost, int cabac)
{
    int shift = 2 * (m_bitDepth - 8);
    bestCost = MAX_DOUBLE;
    Ipp64f cost;
    SaoCtuParam testParam;

    for (int mode=0; mode< NUM_SAO_MERGE_TYPES; mode++) {
        if (NULL == mergeList[mode]) {
            continue;
        }

        testParam = *(mergeList[mode]);
        Ipp64f distortion=0;
        for (int compIdx=0; compIdx< 1; compIdx++) {
            testParam[compIdx].mode_idx = SAO_MODE_MERGE;
            testParam[compIdx].type_idx = mode;

            SaoOffsetParam& mergedOffset = (*(mergeList[mode]))[compIdx];

            if ( SAO_MODE_OFF != mergedOffset.mode_idx ) {
                distortion += (Ipp64f)GetDistortion( mergedOffset.type_idx, mergedOffset.typeAuxInfo,
                    mergedOffset.offset, m_statData[compIdx][mergedOffset.type_idx], shift);
            }
        }

        m_bsf->CtxRestore(m_ctxSAO[cabac], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
        m_bsf->Reset();

        CodeSaoCtbParam(m_bsf, testParam, sliceEnabled, (NULL != mergeList[SAO_MERGE_LEFT]), (NULL != mergeList[SAO_MERGE_ABOVE]), false);

        int rate = GetNumWrittenBits();

        cost = distortion / m_labmda[0] + (Ipp64f)rate;

        if (cost < bestCost) {
            bestCost = cost;
            bestParam= testParam;
            m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
        }
    }

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
}


// etimate compression gain SAO_ON vs SAO_OFF via RDO
void SaoEstimator::ModeDecision_Base( SaoCtuParam* mergeList[2],  bool *sliceEnabled, SaoCtuParam &bestParam, 
                                        Ipp64f &bestCost, int cabac)
{
    int shift = 2 * (m_bitDepth - 8);
    Ipp64f minCost = 0.0, cost = 0.0;
    int rate = 0, minRate = 0;
    Ipp64s dist[NUM_SAO_COMPONENTS] = {0};
    Ipp64s modeDist[NUM_SAO_COMPONENTS] = {0};
    SaoOffsetParam testOffset[NUM_SAO_COMPONENTS];

    int compIdx = SAO_Y;
    bestParam[compIdx].mode_idx = SAO_MODE_OFF;
    bestParam[compIdx].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_CUR], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    CodeSaoCtbParam(m_bsf, bestParam, sliceEnabled, mergeList[SAO_MERGE_LEFT] != NULL, mergeList[SAO_MERGE_ABOVE] != NULL, true);
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_MID], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    bestParam[compIdx].mode_idx = SAO_MODE_OFF;

    m_bsf->Reset();
    CodeSaoCtbOffsetParam(m_bsf, compIdx, bestParam[compIdx], sliceEnabled[compIdx]);
    minRate = GetNumWrittenBits();

    modeDist[compIdx] = 0;
    minCost = m_labmda[compIdx] * (Ipp64f)minRate;

    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

    if (sliceEnabled[compIdx]) {
        for (Ipp32s type_idx = 0; type_idx < m_numSaoModes; type_idx++) {
            testOffset[compIdx].mode_idx = SAO_MODE_ON;
            testOffset[compIdx].type_idx = type_idx;
            testOffset[compIdx].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

            GetQuantOffsets(type_idx, m_statData[compIdx][type_idx], testOffset[compIdx].offset,
                testOffset[compIdx].typeAuxInfo, m_labmda[compIdx], m_bitDepth, m_saoMaxOffsetQVal, shift);

            dist[compIdx] = GetDistortion(testOffset[compIdx].type_idx, testOffset[compIdx].typeAuxInfo,
                testOffset[compIdx].offset, m_statData[compIdx][type_idx], shift);

            m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_MID], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
            m_bsf->Reset();
            CodeSaoCtbOffsetParam(m_bsf, compIdx, testOffset[compIdx], sliceEnabled[compIdx]);
            rate = GetNumWrittenBits();

            cost = (Ipp64f)dist[compIdx] + m_labmda[compIdx]*((Ipp64f)rate);
            if (cost < minCost) {
                minCost = cost;
                minRate = rate;
                modeDist[compIdx] = dist[compIdx];
                bestParam[compIdx]= testOffset[compIdx];

                m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
            }
        }
    }
    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_MID], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

    bool isChromaEnabled = sliceEnabled[1];
    Ipp64f chromaLambda = m_labmda[SAO_Cb];
    if ( isChromaEnabled ) {
        VM_ASSERT(m_labmda[SAO_Cb] == m_labmda[SAO_Cr]);

        m_bsf->Reset();

        bestParam[SAO_Cb].mode_idx = SAO_MODE_OFF;
        bestParam[SAO_Cb].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
        CodeSaoCtbOffsetParam(m_bsf, SAO_Cb, bestParam[SAO_Cb], sliceEnabled[SAO_Cb]);

        bestParam[SAO_Cr].mode_idx = SAO_MODE_OFF;
        bestParam[SAO_Cr].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
        CodeSaoCtbOffsetParam(m_bsf, SAO_Cr, bestParam[SAO_Cr], sliceEnabled[SAO_Cr]);

        minRate = GetNumWrittenBits();

        modeDist[SAO_Cb] = modeDist[SAO_Cr]= 0;
        minCost= chromaLambda*((Ipp64f)minRate);

        for (Ipp32s type_idx = 0; type_idx < m_numSaoModes; type_idx++) {
            for (compIdx = SAO_Cb; compIdx < NUM_SAO_COMPONENTS; compIdx++) {
                if (!sliceEnabled[compIdx]) {
                    testOffset[compIdx].mode_idx = SAO_MODE_OFF;
                    dist[compIdx]= 0;
                    continue;
                }
                testOffset[compIdx].mode_idx = SAO_MODE_ON;
                testOffset[compIdx].type_idx = type_idx;
                testOffset[compIdx].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

                GetQuantOffsets(type_idx, m_statData[compIdx][type_idx], testOffset[compIdx].offset,
                    testOffset[compIdx].typeAuxInfo, m_labmda[compIdx],
                    m_bitDepth, m_saoMaxOffsetQVal, shift);                

                dist[compIdx]= GetDistortion(type_idx, testOffset[compIdx].typeAuxInfo,
                    testOffset[compIdx].offset,
                    m_statData[compIdx][type_idx], shift);
            }

            m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_MID], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
            m_bsf->Reset();
            CodeSaoCtbOffsetParam(m_bsf, SAO_Cb, testOffset[SAO_Cb], sliceEnabled[SAO_Cb]);
            CodeSaoCtbOffsetParam(m_bsf, SAO_Cr, testOffset[SAO_Cr], sliceEnabled[SAO_Cr]);
            rate = GetNumWrittenBits();

            cost = (Ipp64f)(dist[SAO_Cb]+ dist[SAO_Cr]) + chromaLambda*((Ipp64f)rate);
            if (cost < minCost) {
                minCost = cost;
                minRate = rate;
                modeDist[SAO_Cb] = dist[SAO_Cb];
                modeDist[SAO_Cr] = dist[SAO_Cr];
                bestParam[SAO_Cb]= testOffset[SAO_Cb];
                bestParam[SAO_Cr]= testOffset[SAO_Cr];
            }
        }
    }

    bestCost = (Ipp64f)modeDist[SAO_Y]/m_labmda[SAO_Y];

    if (isChromaEnabled) {
        bestCost += (Ipp64f)(modeDist[SAO_Cb]+ modeDist[SAO_Cr])/chromaLambda;
    }

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_CUR], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    m_bsf->Reset();
    CodeSaoCtbParam(m_bsf, bestParam, sliceEnabled, (mergeList[SAO_MERGE_LEFT]!= NULL), (mergeList[SAO_MERGE_ABOVE]!= NULL), false);
    bestCost += (Ipp64f)GetNumWrittenBits();
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


template <typename PixType>
void H265CU<PixType>::EstimateCtuSao( H265BsFake *bs, SaoCtuParam* saoParam, SaoCtuParam* saoParam_TotalFrame,
                                     const MFX_HEVC_PP::CTBBorders & borders, const Ipp16u* slice_ids)
{
    m_saoEst.m_ctb_addr = this->m_ctbAddr;
    m_saoEst.m_ctb_pelx = this->m_ctbPelX;
    m_saoEst.m_ctb_pely = this->m_ctbPelY;

    m_saoEst.m_codedParams_TotalFrame = saoParam_TotalFrame;
    m_saoEst.m_bsf = bs;
    m_saoEst.m_labmda[0] = this->m_rdLambda*256;
    m_saoEst.m_borders = borders;

    m_saoEst.m_slice_ids = slice_ids;
    //chroma issues with lambda???
    m_saoEst.m_labmda[1] = m_saoEst.m_labmda[2] = m_saoEst.m_labmda[0];
    //m_saoEst.m_labmda[1] = m_saoEst.m_labmda[2] = this->m_rdLambda*256 / this->m_ChromaDistWeight;

    FrameData origin;
    FrameData recon;

    origin.y = (mfxU8*)this->m_ySrc;
    origin.uv = (mfxU8*)this->m_uvSrc;
    origin.pitch_luma_pix = this->m_pitchSrcLuma;
    origin.pitch_chroma_pix = this->m_pitchSrcChroma;

    recon.y = (mfxU8*)this->m_yRec;
    recon.uv = (mfxU8*)this->m_uvRec;
    recon.pitch_luma_pix = this->m_pitchRecLuma;
    recon.pitch_chroma_pix = this->m_pitchRecChroma;

    bool    sliceEnabled[NUM_SAO_COMPONENTS] = {true, true, true};
    if (m_par->SAOChromaFlag == 0) {
        sliceEnabled[1] = sliceEnabled[2] = false;
    }

    m_saoEst.GetCtuSaoStatistics<PixType>(&origin, &recon);
    m_saoEst.GetBestCtuSaoParam(sliceEnabled, saoParam);

    // set slice param
    if ( !this->m_cslice->slice_sao_luma_flag ) {
        this->m_cslice->slice_sao_luma_flag = (Ipp8u)sliceEnabled[SAO_Y];
    }
    if ( !this->m_cslice->slice_sao_chroma_flag ) {
        this->m_cslice->slice_sao_chroma_flag = (sliceEnabled[SAO_Cb] || sliceEnabled[SAO_Cr] ) ? 1 : 0;
    }
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

inline Ipp16s getSign(Ipp32s x)
{
    return ((x >> 31) | ((Ipp32s)( (((Ipp32u) -x)) >> 31)));
}


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
