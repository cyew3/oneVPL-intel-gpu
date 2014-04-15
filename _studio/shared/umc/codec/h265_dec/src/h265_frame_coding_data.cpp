/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "h265_frame_coding_data.h"

namespace UMC_HEVC_DECODER
{

PartitionInfo::PartitionInfo()
    : m_MaxCUDepth(0)
    , m_MaxCUSize(0)
{
}

// Initialize conversion table of Z-order partition indexes to raster order partition indexes inside of CTB
void PartitionInfo::InitZscanToRaster(Ipp32s MaxDepth, Ipp32s Depth, Ipp32u StartVal, Ipp32u*& CurrIdx)
{
    Ipp32s Stride = 1 << (MaxDepth - 1);

    if (Depth == MaxDepth)
    {
        CurrIdx[0] = StartVal;
        CurrIdx++;
    }
    else
    {
        Ipp32s Step = Stride >> Depth;
        InitZscanToRaster(MaxDepth, Depth + 1, StartVal, CurrIdx);
        InitZscanToRaster(MaxDepth, Depth + 1, StartVal + Step, CurrIdx);
        InitZscanToRaster(MaxDepth, Depth + 1, StartVal + Step * Stride, CurrIdx);
        InitZscanToRaster(MaxDepth, Depth + 1, StartVal + Step * Stride + Step, CurrIdx);
    }
}

// Initialize conversion table of raster order partition indexes to Z-order partition indexes inside of CTB
void PartitionInfo::InitRasterToZscan(const H265SeqParamSet* sps)
{
    Ipp32u  NumPartInSize  = sps->MaxCUSize  / sps->MinCUSize;

    for (Ipp32u i = 0; i < NumPartInSize * NumPartInSize; i++)
    {
        m_rasterToZscan[m_zscanToRaster[i]] = i;
    }
}

// Initialize lookup table for raster partition coordinates from raster index inside of CTB
void PartitionInfo::InitRasterToPelXY(const H265SeqParamSet* sps)
{
    Ipp32u* TempX = &m_rasterToPelX[0];
    Ipp32u* TempY = &m_rasterToPelY[0];

    Ipp32u NumPartInSize = sps->MaxCUSize / sps->MinCUSize;

    for (Ipp32u i = 0; i < NumPartInSize*NumPartInSize; i++)
    {
        TempX[i] = (m_zscanToRaster[i] % NumPartInSize) * sps->MinCUSize;
    }

    for (Ipp32u i = 1; i < NumPartInSize * NumPartInSize; i++)
    {
        TempY[i] = (m_zscanToRaster[i] / NumPartInSize) * sps->MinCUSize;
    }
};

// Initialize all lookup tables
void PartitionInfo::Init(const H265SeqParamSet* sps)
{
    if (m_MaxCUDepth == sps->MaxCUDepth && m_MaxCUSize == sps->MaxCUSize)
        return;

    m_MaxCUDepth = sps->MaxCUDepth;
    m_MaxCUSize = sps->MaxCUSize;

    memset(m_rasterToPelX, 0, sizeof(m_rasterToPelX));
    memset(m_rasterToPelY, 0, sizeof(m_rasterToPelY));
    memset(m_zscanToRaster, 0, sizeof(m_zscanToRaster));
    memset(m_rasterToZscan, 0, sizeof(m_rasterToZscan));

    // initialize partition order.
    Ipp32u* Tmp = &m_zscanToRaster[0];
    InitZscanToRaster(sps->MaxCUDepth + 1, 1, 0, Tmp);
    InitRasterToZscan(sps);

    // initialize conversion matrix from partition index to pel
    InitRasterToPelXY(sps);
}

// Frame CTB data structure constructor
H265FrameCodingData::H265FrameCodingData()
    : m_WidthInCU(0)
    , m_HeightInCU(0)
    , m_MaxCUWidth(0)
    , m_NumCUsInFrame(0)
    , m_CU(0)
    , m_CUOrderMap(0)
    , m_TileIdxMap(0)
    , m_InverseCUOrderMap(0)
    , m_edge(0)
    , m_colocatedInfo(0)
    , m_cumulativeMemoryPtr(0)
{
}

// Frame CTB data structure destructor
H265FrameCodingData::~H265FrameCodingData()
{
    destroy();
}

// Allocate frame CTB array table
void H265FrameCodingData::create(Ipp32s iPicWidth, Ipp32s iPicHeight, Ipp32u uiMaxWidth, Ipp32u uiMaxHeight, Ipp32u uiMaxDepth)
{
    m_MaxCUDepth = (Ipp8u) uiMaxDepth;
    m_NumPartitions  = 1 << (m_MaxCUDepth<<1);

    m_MaxCUWidth = uiMaxWidth;

    Ipp32s MinCUWidth = uiMaxWidth >> m_MaxCUDepth;

    m_NumPartInWidth = uiMaxWidth / MinCUWidth;

    m_WidthInCU = (iPicWidth % uiMaxWidth) ? iPicWidth / uiMaxWidth + 1 : iPicWidth / uiMaxWidth;
    m_HeightInCU = (iPicHeight % uiMaxHeight) ? iPicHeight / uiMaxHeight + 1 : iPicHeight / uiMaxHeight;

    // Allocate main CTB table
    m_NumCUsInFrame = m_WidthInCU * m_HeightInCU;
    m_CU = h265_new_array_throw<H265CodingUnit*>(m_NumCUsInFrame + 1);

    m_colocatedInfo = h265_new_array_throw<H265MVInfo>(m_NumCUsInFrame * m_NumPartitions);

    m_cuData.resize(m_NumCUsInFrame*m_NumPartitions);

    Ipp8u*                    cbf[3];         // array of coded block flags (CBF)

    Ipp8u*                    lumaIntraDir;    // array of intra directions (luma)
    Ipp8u*                    chromaIntraDir;  // array of intra directions (chroma)

    // Allocate additional CTB buffers
    m_cumulativeMemoryPtr = CumulativeArraysAllocation(5, // number of parameters
                                32, // align
                                &lumaIntraDir, sizeof(Ipp8u) * m_NumPartitions * m_NumCUsInFrame,
                                &chromaIntraDir, sizeof(Ipp8u) * m_NumPartitions * m_NumCUsInFrame,

                                &cbf[0], sizeof(Ipp8u) * m_NumPartitions * m_NumCUsInFrame,
                                &cbf[1], sizeof(Ipp8u) * m_NumPartitions * m_NumCUsInFrame,
                                &cbf[2], sizeof(Ipp8u) * m_NumPartitions * m_NumCUsInFrame);

    for (Ipp32s i = 0; i < m_NumCUsInFrame; i++)
    {
        m_CU[i] = new H265CodingUnit;
        m_CU[i]->create (this, i);

        m_CU[i]->m_lumaIntraDir = lumaIntraDir;
        m_CU[i]->m_chromaIntraDir = chromaIntraDir;

        lumaIntraDir += m_NumPartitions;
        chromaIntraDir += m_NumPartitions;

        m_CU[i]->m_cbf[0] = cbf[0];
        m_CU[i]->m_cbf[1] = cbf[1];
        m_CU[i]->m_cbf[2] = cbf[2];

        cbf[0] += m_NumPartitions;
        cbf[1] += m_NumPartitions;
        cbf[2] += m_NumPartitions;
    }

    m_CU[m_NumCUsInFrame] = 0;

    // Allocate edges array for deblocking
    Ipp32s m_edgesInCTBSize = m_MaxCUWidth >> 3;
    m_edgesInFrameWidth = (m_edgesInCTBSize * m_WidthInCU) * 2;
    Ipp32s edgesInFrameHeight = m_edgesInCTBSize * m_HeightInCU;
    m_edge = h265_new_array_throw<H265PartialEdgeData>(m_edgesInFrameWidth * edgesInFrameHeight);
}

// Deallocate frame CTB array table
void H265FrameCodingData::destroy()
{
    if (m_cumulativeMemoryPtr)
    {
        CumulativeFree(m_cumulativeMemoryPtr);
        m_cumulativeMemoryPtr = 0;
    }

    for (Ipp32s i = 0; i < m_NumCUsInFrame; i++)
    {
        m_CU[i]->destroy();
        delete m_CU[i];
        m_CU[i] = 0;
    }

    delete [] m_CU;
    m_CU = 0;

    delete[] m_colocatedInfo;
    m_colocatedInfo = 0;

    delete[] m_edge;
    m_edge = 0;

    m_SAO.destroy();
}

} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
