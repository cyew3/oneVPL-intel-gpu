/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "h265_frame_coding_data.h"

namespace UMC_HEVC_DECODER
{

void H265FrameCodingData::create(Ipp32s iPicWidth, Ipp32s iPicHeight, Ipp32u uiMaxWidth, Ipp32u uiMaxHeight, Ipp32u uiMaxDepth)
{
    m_MaxCUDepth = (Ipp8u) uiMaxDepth;
    m_NumPartitions  = 1 << (m_MaxCUDepth<<1);

    m_MaxCUWidth = uiMaxWidth;

    m_MinCUWidth = uiMaxWidth >> m_MaxCUDepth;

    m_NumPartInWidth = uiMaxWidth / m_MinCUWidth;

    m_WidthInCU = (iPicWidth % uiMaxWidth) ? iPicWidth / uiMaxWidth + 1 : iPicWidth / uiMaxWidth;
    m_HeightInCU = (iPicHeight % uiMaxHeight) ? iPicHeight / uiMaxHeight + 1 : iPicHeight / uiMaxHeight;

    m_NumCUsInFrame = m_WidthInCU * m_HeightInCU;
    m_CUData = new H265CodingUnit*[m_NumCUsInFrame];

    m_colocatedInfo[0] = new ColocatedTUInfo[m_NumCUsInFrame * m_NumPartitions];
    m_colocatedInfo[1] = new ColocatedTUInfo[m_NumCUsInFrame * m_NumPartitions];

    for (Ipp32u i = 0; i < m_NumCUsInFrame; i++)
    {
        m_CUData[i] = new H265CodingUnit;
        m_CUData[i]->create (m_NumPartitions, m_MaxCUWidth, m_MaxCUWidth);
    }
}

void H265FrameCodingData::destroy()
{
    for (Ipp32u i = 0; i < m_NumCUsInFrame; i++)
    {
        m_CUData[i]->destroy();
        delete m_CUData[i];
        m_CUData[i] = NULL;
    }

    delete [] m_CUData;
    m_CUData = NULL;

    for (int i = 0; i < 2; i++)
    {
        delete[] m_colocatedInfo[i];
        m_colocatedInfo[i] = 0;
    }
}

} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
