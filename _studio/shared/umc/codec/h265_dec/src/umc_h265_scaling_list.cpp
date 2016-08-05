/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_dec_defs.h"
#include "umc_h265_bitstream_headers.h"
#include "umc_h265_tables.h"

namespace UMC_HEVC_DECODER
{

// Allocate and initialize scaling list tables
void H265ScalingList::init()
{
    VM_ASSERT(!m_initialized);

    for (Ipp32u sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        Ipp32u scalingListNum = g_scalingListNum[sizeId];
        Ipp32u scalingListSize = g_scalingListSize[sizeId];

        Ipp16s* pScalingList = h265_new_array_throw<Ipp16s>(scalingListNum * scalingListSize * SCALING_LIST_REM_NUM);

        for (Ipp32u listId = 0; listId < scalingListNum; listId++)
        {
            for (Ipp32u qp = 0; qp < SCALING_LIST_REM_NUM; qp++)
            {
                m_dequantCoef[sizeId][listId][qp] = pScalingList + (qp * scalingListSize);
            }
            pScalingList += (SCALING_LIST_REM_NUM * scalingListSize);
        }
    }

    //alias list [1] as [3].
    for (Ipp32u qp = 0; qp < SCALING_LIST_REM_NUM; qp++)
    {
        m_dequantCoef[SCALING_LIST_32x32][3][qp] = m_dequantCoef[SCALING_LIST_32x32][1][qp];
    }

    m_initialized = true;
}

// Deallocate scaling list tables
void H265ScalingList::destroy()
{
    if (!m_initialized)
        return;

    for (Ipp32u sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        delete [] m_dequantCoef[sizeId][0][0];
        m_dequantCoef[sizeId][0][0] = 0;
    }

    m_initialized = false;
}

// Calculated coefficients used for dequantization
void H265ScalingList::calculateDequantCoef(void)
{
    VM_ASSERT(m_initialized);

    static const Ipp32u g_scalingListSizeX[4] = { 4, 8, 16, 32 };

    for (Ipp32u sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        for (Ipp32u listId = 0; listId < g_scalingListNum[sizeId]; listId++)
        {
            for (Ipp32u qp = 0; qp < SCALING_LIST_REM_NUM; qp++)
            {
                Ipp32u width = g_scalingListSizeX[sizeId];
                Ipp32u height = g_scalingListSizeX[sizeId];
                Ipp32u ratio = g_scalingListSizeX[sizeId] / IPP_MIN(MAX_MATRIX_SIZE_NUM, (Ipp32s)g_scalingListSizeX[sizeId]);
                Ipp16s *dequantcoeff;
                Ipp32s *coeff = getScalingListAddress(sizeId, listId);

                dequantcoeff = getDequantCoeff(listId, qp, sizeId);
                processScalingListDec(coeff, dequantcoeff, g_invQuantScales[qp], height, width, ratio,
                    IPP_MIN(MAX_MATRIX_SIZE_NUM, (Ipp32s)g_scalingListSizeX[sizeId]), getScalingListDC(sizeId, listId));
            }
        }
    }
}

// Initialize scaling list with default data
void H265ScalingList::initFromDefaultScalingList()
{
    VM_ASSERT (!m_initialized);

    init();

    for (Ipp32u sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        for (Ipp32u listId = 0; listId < g_scalingListNum[sizeId]; listId++)
        {
            MFX_INTERNAL_CPY(getScalingListAddress(sizeId, listId),
                getScalingListDefaultAddress(sizeId, listId),
                sizeof(Ipp32s) * IPP_MIN(MAX_MATRIX_COEF_NUM, (Ipp32s)g_scalingListSize[sizeId]));
            setScalingListDC(sizeId, listId, SCALING_LIST_DC);
        }
    }

    calculateDequantCoef();
}

// Calculated coefficients used for dequantization in one scaling list matrix
void H265ScalingList::processScalingListDec(Ipp32s *coeff, Ipp16s *dequantcoeff, Ipp32s invQuantScales, Ipp32u height, Ipp32u width, Ipp32u ratio, Ipp32u sizuNum, Ipp32u dc)
{
    for(Ipp32u j = 0; j < height; j++)
    {
#ifdef __INTEL_COMPILER
        #pragma vector always
#endif
        for(Ipp32u i = 0; i < width; i++)
        {
            dequantcoeff[j * width + i] = (Ipp16s)(invQuantScales * coeff[sizuNum * (j / ratio) + i / ratio]);
        }
    }
    if(ratio > 1)
    {
        dequantcoeff[0] = (Ipp16s)(invQuantScales * dc);
    }
}

// Returns default scaling matrix for specified parameters
const int* H265ScalingList::getScalingListDefaultAddress(unsigned sizeId, unsigned listId)
{
    const int *src = 0;
    switch(sizeId)
    {
    case SCALING_LIST_4x4:
        src = g_quantTSDefault4x4;
        break;
    case SCALING_LIST_8x8:
        src = (listId<3) ? g_quantIntraDefault8x8 : g_quantInterDefault8x8;
        break;
    case SCALING_LIST_16x16:
        src = (listId<3) ? g_quantIntraDefault8x8 : g_quantInterDefault8x8;
        break;
    case SCALING_LIST_32x32:
        src = (listId<1) ? g_quantIntraDefault8x8 : g_quantInterDefault8x8;
        break;
    default:
        VM_ASSERT(0);
        src = NULL;
        break;
    }
    return src;
}

// Copy data from predefined scaling matrixes
void H265ScalingList::processRefMatrix(unsigned sizeId, unsigned listId , unsigned refListId)
{
  MFX_INTERNAL_CPY(getScalingListAddress(sizeId, listId),
      ((listId == refListId) ? getScalingListDefaultAddress(sizeId, refListId) : getScalingListAddress(sizeId, refListId)),
      sizeof(int)*IPP_MIN(MAX_MATRIX_COEF_NUM, (int)g_scalingListSize[sizeId]));
}

} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H265_VIDEO_DECODER
