//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
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
    uint32_t m_rasterToPelX[MAX_NUM_PU_IN_ROW * MAX_NUM_PU_IN_ROW];
    uint32_t m_rasterToPelY[MAX_NUM_PU_IN_ROW * MAX_NUM_PU_IN_ROW];

    // flexible conversion from relative to absolute index
    uint32_t m_zscanToRaster[MAX_NUM_PU_IN_ROW * MAX_NUM_PU_IN_ROW];
    uint32_t m_rasterToZscan[MAX_NUM_PU_IN_ROW * MAX_NUM_PU_IN_ROW];

private:
    void InitZscanToRaster(int32_t MaxDepth, int32_t Depth, uint32_t StartVal, uint32_t*& CurrIdx);
    void InitRasterToZscan(const H265SeqParamSet* sps);
    void InitRasterToPelXY(const H265SeqParamSet* sps);

    uint32_t m_MaxCUDepth;
    uint32_t m_MaxCUSize;
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
    uint8_t deblockP  : 1;
    uint8_t deblockQ  : 1;
    int8_t strength  : 3;

    int8_t qp;
}; // sizeof - 2 bytes


// Picture coding data class
class H265FrameCodingData
{
    DISALLOW_COPY_AND_ASSIGN(H265FrameCodingData);

public:
    uint32_t m_WidthInCU;
    uint32_t m_HeightInCU;

    uint32_t m_MaxCUWidth;

    uint8_t m_MaxCUDepth;                        // max. depth
    int32_t m_NumPartitions;
    int32_t m_NumPartInWidth;
    int32_t m_NumCUsInFrame;

    H265CodingUnit *m_CU;
    std::vector<H265CodingUnitData> m_cuData;
    uint8_t * m_cumulativeMemoryPtr;

    uint32_t* m_CUOrderMap;                   //the map of LCU raster scan address relative to LCU encoding order
    uint32_t* m_TileIdxMap;                   //the map of the tile index relative to LCU raster scan address
    uint32_t* m_InverseCUOrderMap;

    SAOLCUParam* m_saoLcuParam;
    int32_t m_sizeOfSAOData;

    PartitionInfo m_partitionInfo;
#ifndef MFX_VA
    H265SampleAdaptiveOffset m_SAO;
#endif
    // Deblocking data
    H265PartialEdgeData *m_edge;
    int32_t m_edgesInCTBSize, m_edgesInFrameWidth;

    struct RefFrameInfo
    {
        int32_t pocDelta;
        int32_t flags;
    };

    std::vector<RefFrameInfo> m_refFrameInfo;

public:

    H265MVInfo *m_colocatedInfo;

    H265MotionVector & GetMV(EnumRefPicList direction, uint32_t partNumber)
    {
        return m_colocatedInfo[partNumber].m_mv[direction];
    }

    uint8_t GetTUFlags(EnumRefPicList direction, uint32_t partNumber)
    {
        if (m_colocatedInfo[partNumber].m_refIdx[direction] == -1)
            return COL_TU_INVALID_INTER;

        return (uint8_t)m_refFrameInfo[m_colocatedInfo[partNumber].m_index[direction]].flags;
    }

    int32_t GetTUPOCDelta(EnumRefPicList direction, uint32_t partNumber)
    {
        return m_refFrameInfo[m_colocatedInfo[partNumber].m_index[direction]].pocDelta;
    }

    RefIndexType & GetRefIdx(EnumRefPicList direction, uint32_t partNumber)
    {
        return m_colocatedInfo[partNumber].m_refIdx[direction];
    }

    H265MVInfo & GetTUInfo(uint32_t partNumber)
    {
        return m_colocatedInfo[partNumber];
    }

    void create(int32_t iPicWidth, int32_t iPicHeight, const H265SeqParamSet* sps);
    void destroy();
    void initSAO(const H265SeqParamSet* sps);

    H265FrameCodingData();
    ~H265FrameCodingData();

    H265CodingUnit* getCU(uint32_t CUAddr) const
    {
        return &m_CU[CUAddr];
    }

    uint32_t getNumPartInCU()
    {
        return m_NumPartitions;
    }

    int32_t getCUOrderMap(int32_t encCUOrder)
    {
        return *(m_CUOrderMap + (encCUOrder >= m_NumCUsInFrame ? m_NumCUsInFrame : encCUOrder));
    }
    uint32_t getTileIdxMap(int32_t i)                              { return *(m_TileIdxMap + i); }
    int32_t GetInverseCUOrderMap(int32_t cuAddr)                  {return *(m_InverseCUOrderMap + (cuAddr >= m_NumCUsInFrame ? m_NumCUsInFrame : cuAddr));}
};

} // end namespace UMC_HEVC_DECODER

#endif //__H265_FRAME_CODING_DATA_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
