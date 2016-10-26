//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_frame_coding_data.h"
#include "umc_h265_heap.h"

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
    , m_MaxCUDepth(0)
    , m_NumPartitions(0)
    , m_NumPartInWidth(0)
    , m_NumCUsInFrame(0)
    , m_CU(0)
    , m_cumulativeMemoryPtr(0)
    , m_CUOrderMap(0)
    , m_TileIdxMap(0)
    , m_InverseCUOrderMap(0)
    , m_saoLcuParam(0)
    , m_sizeOfSAOData(0)
    , m_edge(0)
    , m_edgesInCTBSize(0)
    , m_edgesInFrameWidth(0)
    , m_colocatedInfo(0)
{
}

// Frame CTB data structure destructor
H265FrameCodingData::~H265FrameCodingData()
{
    destroy();
}

// Allocate frame CTB array table
void H265FrameCodingData::create(Ipp32s iPicWidth, Ipp32s iPicHeight, const H265SeqParamSet* sps)
{
    m_MaxCUDepth = (Ipp8u) sps->MaxCUDepth;
    m_NumPartitions  = 1 << (m_MaxCUDepth<<1);

    m_MaxCUWidth = sps->MaxCUSize;

    Ipp32s MinCUWidth = m_MaxCUWidth >> m_MaxCUDepth;

    m_NumPartInWidth = m_MaxCUWidth / MinCUWidth;

    m_WidthInCU = (iPicWidth % m_MaxCUWidth) ? iPicWidth / m_MaxCUWidth + 1 : iPicWidth / m_MaxCUWidth;
    m_HeightInCU = (iPicHeight % m_MaxCUWidth) ? iPicHeight / m_MaxCUWidth + 1 : iPicHeight / m_MaxCUWidth;

    // Allocate main CTB table
    m_NumCUsInFrame = m_WidthInCU * m_HeightInCU;

#ifndef MFX_VA
    m_CU = h265_new_array_throw<H265CodingUnit>(m_NumCUsInFrame + 1);

    m_colocatedInfo = h265_new_array_throw<H265MVInfo>(m_NumCUsInFrame * m_NumPartitions);
    memset(m_colocatedInfo, 0, sizeof(H265MVInfo)*m_NumCUsInFrame * m_NumPartitions);

    m_cuData.resize(m_NumCUsInFrame*m_NumPartitions);

    Ipp8u*                    cbf[5];         // array of coded block flags (CBF)

    Ipp8u*                    lumaIntraDir;    // array of intra directions (luma)
    Ipp8u*                    chromaIntraDir;  // array of intra directions (chroma)

    // Allocate additional CTB buffers
    m_cumulativeMemoryPtr = CumulativeArraysAllocation(7, // number of parameters
                                32, // align
                                &lumaIntraDir, sizeof(Ipp8u) * m_NumPartitions * m_NumCUsInFrame,
                                &chromaIntraDir, sizeof(Ipp8u) * m_NumPartitions * m_NumCUsInFrame,

                                &cbf[0], sizeof(Ipp8u) * m_NumPartitions * m_NumCUsInFrame,
                                &cbf[1], sizeof(Ipp8u) * m_NumPartitions * m_NumCUsInFrame,
                                &cbf[2], sizeof(Ipp8u) * m_NumPartitions * m_NumCUsInFrame,
                                &cbf[3], sizeof(Ipp8u) * m_NumPartitions * m_NumCUsInFrame,
                                &cbf[4], sizeof(Ipp8u) * m_NumPartitions * m_NumCUsInFrame);

    for (Ipp32s i = 0; i < m_NumCUsInFrame; i++)
    {
        m_CU[i].create (this, i);

        m_CU[i].m_lumaIntraDir = lumaIntraDir;
        m_CU[i].m_chromaIntraDir = chromaIntraDir;

        lumaIntraDir += m_NumPartitions;
        chromaIntraDir += m_NumPartitions;

        m_CU[i].m_cbf[0] = cbf[0];
        m_CU[i].m_cbf[1] = cbf[1];
        m_CU[i].m_cbf[2] = cbf[2];
        if (sps->ChromaArrayType == CHROMA_FORMAT_422)
        {
            m_CU[i].m_cbf[3] = cbf[3];
            m_CU[i].m_cbf[4] = cbf[4];

            cbf[3] += m_NumPartitions;
            cbf[4] += m_NumPartitions;
        }

        cbf[0] += m_NumPartitions;
        cbf[1] += m_NumPartitions;
        cbf[2] += m_NumPartitions;
    }

    // Allocate edges array for deblocking
    Ipp32s m_edgesInCTBSize = m_MaxCUWidth >> 3;
    m_edgesInFrameWidth = (m_edgesInCTBSize * m_WidthInCU) * 2;
    Ipp32s edgesInFrameHeight = m_edgesInCTBSize * m_HeightInCU;
    m_edge = h265_new_array_throw<H265PartialEdgeData>(m_edgesInFrameWidth * edgesInFrameHeight);
#endif
}

// Deallocate frame CTB array table
void H265FrameCodingData::destroy()
{
#ifndef MFX_VA

    delete[] m_CU;
    m_CU = 0;

    if (m_cumulativeMemoryPtr)
    {
        CumulativeFree(m_cumulativeMemoryPtr);
        m_cumulativeMemoryPtr = 0;
    }

    delete[] m_colocatedInfo;
    m_colocatedInfo = 0;

    delete[] m_edge;
    m_edge = 0;

    m_SAO.destroy();

    delete[] m_saoLcuParam;
    m_saoLcuParam = 0;
#endif
}

// Deallocate frame CTB array table
void H265FrameCodingData::initSAO(const H265SeqParamSet* sps)
{
#ifndef MFX_VA
    if (sps->sample_adaptive_offset_enabled_flag)
    {
        m_SAO.init(sps);

        Ipp32s size = m_WidthInCU * m_HeightInCU;
        if (m_sizeOfSAOData < size || !m_saoLcuParam)
        {
            delete[] m_saoLcuParam;
            m_saoLcuParam = 0;

            m_saoLcuParam = h265_new_array_throw<SAOLCUParam>(size);
            m_sizeOfSAOData = size;
        }
    }
#else
    sps;
#endif
}

} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H265_VIDEO_DECODER
