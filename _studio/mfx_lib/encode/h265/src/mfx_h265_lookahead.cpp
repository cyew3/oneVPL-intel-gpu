//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

//#include "mfx_h265_defs.h"
#include "mfx_h265_encode.h"
#include "mfx_h265_frame.h"

#include "algorithm" // ???
#include "mfx_h265_defs.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_optimization.h"
#include "ippi.h"

namespace H265Enc {
// --------------------------------------------------------
// INTRA PREDICTION
// --------------------------------------------------------
template <typename PixType>
static
void GetPredPelsLuma(PixType* src, Ipp32s srcPitch, Ipp32s x, Ipp32s y, IppiSize roi, PixType* PredPel,
                      Ipp32s width, Ipp8u bitDepthLuma)
{
    PixType* tmpSrcPtr;
    PixType dcval;
    Ipp32s i;
    
    dcval = (1 << (bitDepthLuma - 1));

    //src += y*srcPitch + x;

    if ((x | y) == 0) 
    {
        // Fill border with DC value
        for (i = 0; i <= 4*width; i++)
            PredPel[i] = dcval;
    }
    else if (x == 0)
    {
        // Fill top-left border with rec. samples
        tmpSrcPtr = src - srcPitch;
        dcval = tmpSrcPtr[0];
        PredPel[0] = dcval;
        Ipp32s hlim = IPP_MIN(roi.width - x, 2*width);
        for (i = 0; i < hlim; i++) {
            PredPel[2*width + 1 + i] = dcval;
            PredPel[1 + i] = tmpSrcPtr[i];
        }
        for (i = hlim; i < 2*width; i++) {
            PredPel[2*width + 1 + i] = dcval;
            PredPel[1 + i] = tmpSrcPtr[hlim - 1];
        }
    }
    else if (y == 0)
    {
        // Fill top and top right border with rec. samples
        tmpSrcPtr = src - 1;
        dcval = tmpSrcPtr[0];
        PredPel[0] = dcval;
        Ipp32s vlim = IPP_MIN(roi.height - y, 2*width);
        for (i = 0; i < vlim; i++) {
            PredPel[1 + i] = dcval;
            PredPel[2*width + 1 + i] = tmpSrcPtr[0];
            tmpSrcPtr += srcPitch;
        }
        for (i = vlim; i < 2*width; i++) {
            PredPel[1 + i] = dcval;
            PredPel[2*width + 1 + i] = tmpSrcPtr[-srcPitch];
        }
    }
    else
    {
        // Fill top-left border with rec. samples
        tmpSrcPtr = src - srcPitch - 1;
        PredPel[0] = tmpSrcPtr[0];

        // Fill top and top right border with rec. samples
        Ipp32s hlim = IPP_MIN(roi.width - x, 2*width);
        tmpSrcPtr = src - srcPitch;
        for (i = 0; i < hlim; i++)
            PredPel[1 + i] = tmpSrcPtr[i];
        for (i = hlim; i < 2*width; i++)
            PredPel[1 + i] = tmpSrcPtr[hlim - 1];

        // Fill left and below left border with rec. samples
        Ipp32s vlim = IPP_MIN(roi.height - y, 2*width);
        tmpSrcPtr = src - 1;
        for (i = 0; i < vlim; i++) {
            PredPel[2*width + 1 + i] = tmpSrcPtr[0];
            tmpSrcPtr += srcPitch;
        }
        for (i = vlim; i < 2*width; i++)
            PredPel[2*width + 1 + i] = tmpSrcPtr[-srcPitch];
    }
}


static inline Ipp32s H265_Calc_SATD(Ipp8u* pSrc, Ipp32s srcStep, Ipp8u* pRef, Ipp32s width)
{
    Ipp32s numblks = width >> 3;
    Ipp32s satd = 0;
    for (Ipp32s i = 0; i < numblks; i++) {
        for (Ipp32s j = 0; j < numblks; j++) {
            Ipp8u *pS = pSrc + i*8*srcStep + j*8;
            Ipp8u *pR = pRef + i*8*width + j*8;
            satd +=  MFX_HEVC_PP::NAME(h265_SATD_8x8_8u)((const Ipp8u *)pS, srcStep, (const Ipp8u *)pR, width);
        }
    }
    return satd;
}

static inline Ipp32s H265_Calc_SATD(Ipp16u* pSrc, Ipp32s srcStep, Ipp16u* pRef, Ipp32s width)
{
    Ipp32s numblks = width >> 3;
    Ipp32s satd = 0;
    for (Ipp32s i = 0; i < numblks; i++) {
        for (Ipp32s j = 0; j < numblks; j++) {
            Ipp16u *pS = pSrc + i*8*srcStep + j*8;
            Ipp16u *pR = pRef + i*8*width + j*8;
            satd +=  MFX_HEVC_PP::NAME(h265_SATD_8x8_16u)((const Ipp16u *)pS, srcStep, (const Ipp16u *)pR, width);
        }
    }
    return satd;
}


template <typename PixType>
Ipp32s IntraPredSATD(PixType *pSrc, Ipp32s srcPitch, Ipp32s x, Ipp32s y, IppiSize roi, Ipp32s width, Ipp8u bitDepthLuma)
{
    typedef typename GetHistogramType<PixType>::type HistType;
    PixType PredPel[4*2*64+1];
    __ALIGN32 PixType recPel[MAX_CU_SIZE * MAX_CU_SIZE];
    //__ALIGN32 HistType hist4[40 * MAX_CU_SIZE / 4 * MAX_CU_SIZE / 4];
    //__ALIGN32 HistType hist8[40 * MAX_CU_SIZE / 8 * MAX_CU_SIZE / 8];
    __ALIGN32 HistType hist8[40 * MAX_CU_SIZE / 4 * MAX_CU_SIZE / 4];

    pSrc += y*srcPitch + x;
    Ipp32s numblks = width >> 3;

    //if (!(x|y))
    //printf("\n %p %d %d %d %d %d \n", pSrc, srcPitch, x, y, roi.width, roi.height);


    GetPredPelsLuma(pSrc, srcPitch, x, y, roi, PredPel, width, bitDepthLuma);

    h265_PredictIntra_DC(PredPel, recPel, width, width, 1);
    Ipp32s bestSATD = H265_Calc_SATD(pSrc, srcPitch, recPel, width);

    h265_PredictIntra_Ver(PredPel, recPel, width, width, bitDepthLuma, 1);
    Ipp32s satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
    if (satd < bestSATD)
        bestSATD = satd;

    h265_PredictIntra_Hor(PredPel, recPel, width, width, bitDepthLuma, 1);
    satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
    if (satd < bestSATD)
        bestSATD = satd;

    //h265_AnalyzeGradient(pSrc, srcPitch, hist4, hist8, width, width);
    h265_AnalyzeGradient(pSrc, srcPitch, hist8, hist8, width, width);
    if (numblks > 1) {
        Ipp32s pitch = 40 * numblks;
        const HistType *histBlock = hist8;
        for (Ipp32s py = 0; py < numblks; py++, histBlock += pitch)
            for (Ipp32s px = 0; px < numblks; px++)
                if (px | py) {
                    for (Ipp32s i = 0; i < 35; i++)
                        hist8[i] += histBlock[40 * px + i];
                }
    }
    Ipp32s angmode = (Ipp32s)(std::max_element(hist8 + 2, hist8 + 35) - hist8);

    Ipp32s diff = MIN(abs(angmode - INTRA_HOR), abs(angmode - INTRA_VER));

    bool isFilterNeeded = (diff > h265_filteredModes[h265_log2m2[width]]);
    if (!isFilterNeeded && diff > 0) {
        h265_PredictIntra_Ang(angmode, PredPel, recPel, width, width);
        satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
        if (satd < bestSATD)
            bestSATD = satd;
    }

    // planar - filtering required
    //if (video->csps->strong_intra_smoothing_enabled_flag && width == 32)
    //{
    //    Ipp32s threshold = 1 << (video->bitDepthLuma - 5);

    //    Ipp32s topLeft = PredPel[0];
    //    Ipp32s topRight = PredPel[2*width];
    //    Ipp32s midHor = PredPel[width];

    //    Ipp32s bottomLeft = PredPel[4*width];
    //    Ipp32s midVer = PredPel[3*width];

    //    bool bilinearLeft = abs(topLeft + topRight - 2*midHor) < threshold;
    //    bool bilinearAbove = abs(topLeft + bottomLeft - 2*midVer) < threshold;

    //    if (bilinearLeft && bilinearAbove)
    //        h265_FilterPredictPels_Bilinear(PredPel, width, topLeft, bottomLeft, topRight);
    //    else
    //        h265_FilterPredictPels(PredPel, width);
    //} else
    h265_FilterPredictPels(PredPel, width);

    if (isFilterNeeded && diff > 0) {
        h265_PredictIntra_Ang(angmode, PredPel, recPel, width, width);
        satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
        if (satd < bestSATD)
            bestSATD = satd;
    }
    
    h265_PredictIntra_Planar(PredPel, recPel, width, width);
    satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
    if (satd < bestSATD)
        bestSATD = satd;
    //if (!(x|y))
        //printf("%d \n", bestSATD);


    return bestSATD;
}

template Ipp32s IntraPredSATD<Ipp8u>(Ipp8u *pSrc, Ipp32s srcPitch, Ipp32s x, Ipp32s y, IppiSize roi, Ipp32s width, Ipp8u bitDepthLuma);

void DoIntraAnalysis(Task* task)
{
    // analysis for blk 8x8.
    //pars->PicWidthInCtbs = (pars->Width + pars->MaxCUSize - 1) / pars->MaxCUSize;
    //pars->PicHeightInCtbs = (pars->Height + pars->MaxCUSize - 1) / pars->MaxCUSize;
    const Ipp32s sizeBlk = 8;
    const Ipp32s picWidthInBlks  = (task->m_frameOrigin->width  + sizeBlk - 1) / sizeBlk;
    const Ipp32s picHeightInBlks = (task->m_frameOrigin->height + sizeBlk - 1) / sizeBlk;
    const IppiSize frmSize = {task->m_frameOrigin->width, task->m_frameOrigin->height};

    
    for (Ipp32s blk_row = 0; blk_row < picHeightInBlks; blk_row++) {
        for (Ipp32s blk_col = 0; blk_col < picWidthInBlks; blk_col++) {
            Ipp32s x = blk_col*sizeBlk;
            Ipp32s y = blk_row*sizeBlk;
            Ipp32s fPos = blk_row * picWidthInBlks + blk_col;
            task->m_frameOrigin->m_intraSatd[fPos] = IntraPredSATD(task->m_frameOrigin->y, task->m_frameOrigin->pitch_luma_pix, x, y, frmSize, sizeBlk, 8);
        }
    }
}

} // namespace

TaskIter  findOldestToLookAheadProcess(std::list<Task*> & queue)
{
    if ( queue.empty() ) 
        return queue.end();

    for (TaskIter it = queue.begin(); it != queue.end(); it++ ) {

        if ( !(*it)->m_frameOrigin->wasLAProcessed() ) {
            return it;
        }
    }

    return queue.end();
}

void MFXVideoENCODEH265::LookAheadAnalysis()
{
    TaskIter it = findOldestToLookAheadProcess(m_inputQueue);
    TaskIter prevIt = it;
    --prevIt;

    if (it == m_inputQueue.end()) {
        if (!m_inputQueue.empty())
            m_lookaheadQueue.splice(m_lookaheadQueue.end(), m_inputQueue, m_inputQueue.begin());
        return;
    }

    Task* curr = (*it);
    Task* prev = (it == m_inputQueue.begin()) ? NULL : (*prevIt);

    DoIntraAnalysis(curr);
    //DoInterAnalysis(curr, prev);

    curr->m_frameOrigin->setWasLAProcessed();

    if(prev)
        m_lookaheadQueue.splice(m_lookaheadQueue.end(), m_inputQueue, prevIt);
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
