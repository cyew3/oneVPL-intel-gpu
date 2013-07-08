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

#ifndef __H265_FRAME_CODING_DATA_H
#define __H265_FRAME_CODING_DATA_H

#include "umc_h265_dec_defs_dec.h"
#include "h265_coding_unit.h"

namespace UMC_HEVC_DECODER
{
class H265CodingUnit;
struct H265MotionVector;

/// picture coding data class
class H265FrameCodingData
{
public:
    Ipp32u m_WidthInCU;
    Ipp32u m_HeightInCU;

    Ipp32u m_MaxCUWidth;
    Ipp32u m_MaxCUHeight;
    Ipp32u m_MinCUWidth;
    Ipp32u m_MinCUHeight;

    Ipp8u m_MaxCUDepth;                        // max. depth
    Ipp32u m_NumPartitions;
    Ipp32u m_NumPartInWidth;
    Ipp32u m_NumPartInHeight;
    Ipp32u m_NumCUsInFrame;

    H265CodingUnit **m_CUData;                // array of CU data

    Ipp32u* m_CUOrderMap;                   //the map of LCU raster scan address relative to LCU encoding order
    Ipp32u* m_TileIdxMap;                   //the map of the tile index relative to LCU raster scan address
    Ipp32u* m_InverseCUOrderMap;

    // 18 bytes of data per TU
    Ipp8u *m_ColTUFlags[2];
    Ipp32s *m_ColTUPOCDelta[2];
    H265MotionVector *m_ColTUMV[2];

    void create (Ipp32s iPicWidth, Ipp32s iPicHeight, Ipp32u uiMaxWidth, Ipp32u uiMaxHeight, Ipp32u uiMaxDepth);
    void destroy();

    H265FrameCodingData()
        : m_WidthInCU(0)
        , m_HeightInCU(0)
        , m_MaxCUWidth(0)
        , m_MaxCUHeight(0)

        , m_NumCUsInFrame(0)

        , m_CUData(0)
        , m_CUOrderMap(0)
        , m_TileIdxMap(0)
        , m_InverseCUOrderMap(0)
    {
        for (int i = 0; i < 2; i++)
        {
            m_ColTUFlags[i] = 0;
            m_ColTUPOCDelta[i] = 0;
            m_ColTUMV[i] = 0;
        }
    }

    ~H265FrameCodingData()
    {
        this->destroy();
    }

    H265CodingUnit*  getCU(Ipp32u CUAddr)
    {
        return m_CUData[CUAddr];
    }

    Ipp32u getNumPartInCU()
    {
        return m_NumPartitions;
    }

    // Values for m_ColTUFlags
#define COL_TU_INTRA         0
#define COL_TU_INVALID_INTER 1
#define COL_TU_ST_INTER      2
#define COL_TU_LT_INTER      3

    void setCUOrderMap(Ipp32s encCUOrder, Ipp32s cuAddr)        { *(m_CUOrderMap + encCUOrder) = cuAddr; }
    Ipp32s getCUOrderMap(Ipp32u encCUOrder)
    {
        return *(m_CUOrderMap + (encCUOrder >= m_NumCUsInFrame ? m_NumCUsInFrame : encCUOrder));
    }
    Ipp32u getTileIdxMap(Ipp32s i)                              { return *(m_TileIdxMap + i); }
    void setInverseCUOrderMap(Ipp32s cuAddr, Ipp32s encCUOrder) {*(m_InverseCUOrderMap + cuAddr) = encCUOrder;}
    Ipp32s GetInverseCUOrderMap(Ipp32u cuAddr)                  {return *(m_InverseCUOrderMap + (cuAddr >= m_NumCUsInFrame ? m_NumCUsInFrame : cuAddr));}
};

} // end namespace UMC_HEVC_DECODER

#endif //__H265_FRAME_CODING_DATA_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
