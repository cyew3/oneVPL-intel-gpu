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
    Ipp32u i;

    m_TotalDepth = (Ipp8u) uiMaxDepth;
    m_NumPartitions  = 1 << (m_TotalDepth<<1);

    m_MaxCUWidth = uiMaxWidth;
    m_MaxCUHeight = uiMaxHeight;

    m_MinCUWidth = uiMaxWidth >> m_TotalDepth;
    m_MinCUHeight = uiMaxHeight >> m_TotalDepth;

    m_NumPartInWidth = m_MaxCUWidth / m_MinCUWidth;
    m_NumPartInHeight = m_MaxCUHeight / m_MinCUHeight;

    m_WidthInCU = (iPicWidth % m_MaxCUWidth) ? iPicWidth / m_MaxCUWidth + 1 : iPicWidth / m_MaxCUWidth;
    m_HeightInCU = (iPicHeight % m_MaxCUHeight) ? iPicHeight / m_MaxCUHeight + 1 : iPicHeight / m_MaxCUHeight;

    m_NumCUsInFrame = m_WidthInCU * m_HeightInCU;
    m_CUData = new H265CodingUnit*[m_NumCUsInFrame];

    m_ColTUFlags[0] = new Ipp8u[m_NumCUsInFrame * m_NumPartitions];
    m_ColTUFlags[1] = new Ipp8u[m_NumCUsInFrame * m_NumPartitions];
    m_ColTUPOCDelta[0] = new Ipp32s[m_NumCUsInFrame * m_NumPartitions];
    m_ColTUPOCDelta[1] = new Ipp32s[m_NumCUsInFrame * m_NumPartitions];
    m_ColTUMV[0] = new H265MotionVector[m_NumCUsInFrame * m_NumPartitions];
    m_ColTUMV[1] = new H265MotionVector[m_NumCUsInFrame * m_NumPartitions];

    for (i=0; i < m_NumCUsInFrame; i++)
    {
        m_CUData[i] = new H265CodingUnit;
        m_CUData[i]->create (m_NumPartitions, m_MaxCUWidth, m_MaxCUHeight, false, m_MaxCUWidth >> m_TotalDepth);
    }
}

void H265FrameCodingData::destroy()
{
    Ipp32u i;

    for (i = 0; i < m_NumCUsInFrame; i++)
    {
        m_CUData[i]->destroy();
        delete m_CUData[i];
        m_CUData[i] = NULL;
    }

    delete [] m_CUData;
    m_CUData = NULL;

    delete [] m_ColTUFlags[0];
    m_ColTUFlags[0] = NULL;

    delete [] m_ColTUFlags[1];
    m_ColTUFlags[1] = NULL;

    delete [] m_ColTUPOCDelta[0];
    m_ColTUPOCDelta[0] = NULL;

    delete [] m_ColTUPOCDelta[1];
    m_ColTUPOCDelta[1] = NULL;

    delete [] m_ColTUMV[0];
    m_ColTUMV[0] = NULL;

    delete [] m_ColTUMV[1];
    m_ColTUMV[1] = NULL;
}

/*
Ipp32u H265FrameCodingData::CalculateNextCUAddr(Ipp32u CurrCUAddr)
{
    Ipp32u NextCUAddr;
    Ipp32u TileIdx;

    //get the tile index for the current LCU
    TileIdx = this->getTileIdxMap(CurrCUAddr);

    //get the raster scan address for the next LCU
    if (CurrCUAddr % m_WidthInCU == this->getTile(TileIdx)->getRightEdgePosInCU() &&
        CurrCUAddr / m_WidthInCU == this->getTile(TileIdx)->getBottomEdgePosInCU())
    //the current LCU is the last LCU of the tile
    {
        if (TileIdx == (Ipp32u) (m_NumColumnsMinus1 + 1) * (m_NumRowsMinus1 + 1) - 1)
        {
            NextCUAddr = m_NumCUsInFrame;
        }
        else
        {
            NextCUAddr = this->getTile(TileIdx + 1)->getFirstCUAddr();
        }
    }
    else//the current LCU is not the last LCU of the tile
    {
        if (CurrCUAddr % m_WidthInCU == this->getTile(TileIdx)->getRightEdgePosInCU())
        //the current LCU is one the rightmost edge of the tile
        {
            NextCUAddr = CurrCUAddr + m_WidthInCU - this->getTile(TileIdx)->getTileWidth() + 1;
        }
        else
        {
            NextCUAddr = CurrCUAddr + 1;
        }
    }
    return NextCUAddr;
}
*/
} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
