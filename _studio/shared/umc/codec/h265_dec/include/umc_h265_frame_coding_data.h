//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2014 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __H265_FRAME_CODING_DATA_H
#define __H265_FRAME_CODING_DATA_H

#include "umc_h265_dec_defs.h"
#include "umc_h265_motion_info.h"
#include "umc_h265_sao.h"
#include "umc_h265_coding_unit.h"

namespace UMC_HEVC_DECODER
{

// Partition indexes conversion tables
class PartitionInfo
{
public:

    PartitionInfo();

    void Init(const H265SeqParamSet* sps);

    // conversion of partition index to picture pel position
    Ipp32u m_rasterToPelX[MAX_NUM_PU_IN_ROW * MAX_NUM_PU_IN_ROW];
    Ipp32u m_rasterToPelY[MAX_NUM_PU_IN_ROW * MAX_NUM_PU_IN_ROW];

    // flexible conversion from relative to absolute index
    Ipp32u m_zscanToRaster[MAX_NUM_PU_IN_ROW * MAX_NUM_PU_IN_ROW];
    Ipp32u m_rasterToZscan[MAX_NUM_PU_IN_ROW * MAX_NUM_PU_IN_ROW];

private:
    void InitZscanToRaster(Ipp32s MaxDepth, Ipp32s Depth, Ipp32u StartVal, Ipp32u*& CurrIdx);
    void InitRasterToZscan(const H265SeqParamSet* sps);
    void InitRasterToPelXY(const H265SeqParamSet* sps);

    Ipp32u m_MaxCUDepth;
    Ipp32u m_MaxCUSize;
};

// Values for m_ColTUFlags
enum
{
    COL_TU_INTRA         = 0,
    COL_TU_INVALID_INTER = 1,
    COL_TU_ST_INTER      = 2,
    COL_TU_LT_INTER      = 3
};

// Deblocking edge description
struct H265PartialEdgeData
{
    Ipp8u deblockP  : 1;
    Ipp8u deblockQ  : 1;
    Ipp8s strength  : 3;

    Ipp8s qp;
}; // sizeof - 2 bytes


// Picture coding data class
class H265FrameCodingData
{
    DISALLOW_COPY_AND_ASSIGN(H265FrameCodingData);

public:
    Ipp32u m_WidthInCU;
    Ipp32u m_HeightInCU;

    Ipp32u m_MaxCUWidth;

    Ipp8u m_MaxCUDepth;                        // max. depth
    Ipp32s m_NumPartitions;
    Ipp32s m_NumPartInWidth;
    Ipp32s m_NumCUsInFrame;

    H265CodingUnit *m_CU;
    std::vector<H265CodingUnitData> m_cuData;
    Ipp8u * m_cumulativeMemoryPtr;

    Ipp32u* m_CUOrderMap;                   //the map of LCU raster scan address relative to LCU encoding order
    Ipp32u* m_TileIdxMap;                   //the map of the tile index relative to LCU raster scan address
    Ipp32u* m_InverseCUOrderMap;

    SAOLCUParam* m_saoLcuParam;
    Ipp32s m_sizeOfSAOData;

    PartitionInfo m_partitionInfo;
#ifndef MFX_VA
    H265SampleAdaptiveOffset m_SAO;
#endif
    // Deblocking data
    H265PartialEdgeData *m_edge;
    Ipp32s m_edgesInCTBSize, m_edgesInFrameWidth;

    struct RefFrameInfo
    {
        Ipp32s pocDelta;
        Ipp32s flags;
    };

    std::vector<RefFrameInfo> m_refFrameInfo;

public:

    H265MVInfo *m_colocatedInfo;

    H265MotionVector & GetMV(EnumRefPicList direction, Ipp32u partNumber)
    {
        return m_colocatedInfo[partNumber].m_mv[direction];
    }

    Ipp8u GetTUFlags(EnumRefPicList direction, Ipp32u partNumber)
    {
        if (m_colocatedInfo[partNumber].m_refIdx[direction] == -1)
            return COL_TU_INVALID_INTER;

        return (Ipp8u)m_refFrameInfo[m_colocatedInfo[partNumber].m_index[direction]].flags;
    }

    Ipp32s GetTUPOCDelta(EnumRefPicList direction, Ipp32u partNumber)
    {
        return m_refFrameInfo[m_colocatedInfo[partNumber].m_index[direction]].pocDelta;
    }

    RefIndexType & GetRefIdx(EnumRefPicList direction, Ipp32u partNumber)
    {
        return m_colocatedInfo[partNumber].m_refIdx[direction];
    }

    H265MVInfo & GetTUInfo(Ipp32u partNumber)
    {
        return m_colocatedInfo[partNumber];
    }

    void create(Ipp32s iPicWidth, Ipp32s iPicHeight, const H265SeqParamSet* sps);
    void destroy();
    void initSAO(const H265SeqParamSet* sps);

    H265FrameCodingData();
    ~H265FrameCodingData();

    H265CodingUnit* getCU(Ipp32u CUAddr) const
    {
        return &m_CU[CUAddr];
    }

    Ipp32u getNumPartInCU()
    {
        return m_NumPartitions;
    }

    Ipp32s getCUOrderMap(Ipp32s encCUOrder)
    {
        return *(m_CUOrderMap + (encCUOrder >= m_NumCUsInFrame ? m_NumCUsInFrame : encCUOrder));
    }
    Ipp32u getTileIdxMap(Ipp32s i)                              { return *(m_TileIdxMap + i); }
    Ipp32s GetInverseCUOrderMap(Ipp32s cuAddr)                  {return *(m_InverseCUOrderMap + (cuAddr >= m_NumCUsInFrame ? m_NumCUsInFrame : cuAddr));}
};

} // end namespace UMC_HEVC_DECODER

#endif //__H265_FRAME_CODING_DATA_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
