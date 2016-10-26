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

#ifndef __H265_CODING_UNIT_H
#define __H265_CODING_UNIT_H

#include <vector>

#include "mfx_h265_optimization.h"
#include "umc_h265_dec_defs.h"
#include "umc_h265_motion_info.h"

namespace UMC_HEVC_DECODER
{

class H265FrameCodingData;
class H265Pattern;
class H265SegmentDecoderMultiThreaded;
class H265DecoderFrame;

#pragma pack(1)
// Data structure describing CTB partition state
struct H265CodingUnitData
{
    struct
    {
        Ipp8u cu_transform_bypass : 1;
        Ipp8u pcm_flag : 1;
        Ipp8u transform_skip : 5;
        Ipp8u depth : 3;
        Ipp8u predMode : 2;
        Ipp8u partSize : 3;
        Ipp8u trIndex : 3;
    };

    Ipp8u width;
    Ipp8s qp;
};
#pragma pack()

// Data structure of frame CTB
class H265CodingUnit
{
public:
    //pointers --------------------------------------------------------------------------------------
    H265DecoderFrame*        m_Frame;          // frame pointer
    H265SliceHeader *        m_SliceHeader;    // slice header pointer

    // conversion of partition index to picture pel position
    Ipp32u *m_rasterToPelX;
    Ipp32u *m_rasterToPelY;

    Ipp32u *m_zscanToRaster;
    Ipp32u *m_rasterToZscan;

    Ipp32s                   m_SliceIdx;
    MFX_HEVC_PP::CTBBorders  m_AvailBorder;

    //CU description --------------------------------------------------------------------------------
    Ipp32u                    CUAddr;         // CU address in a slice
    Ipp32u                    m_CUPelX;         // CU position in a pixel (X)
    Ipp32u                    m_CUPelY;         // CU position in a pixel (Y)
    Ipp32u                    m_NumPartition;   // total number of minimum partitions in a CU

    //CU data ----------------------------------------------------------------------------------------
    H265CodingUnitData         *m_cuData;

    Ipp8u*                    m_cbf[5];         // array of coded block flags (CBF)

    Ipp8u*                    m_lumaIntraDir;    // array of intra directions (luma)
    Ipp8u*                    m_chromaIntraDir;  // array of intra directions (chroma)

    inline H265CodingUnitData * GetCUData(Ipp32s addr) const
    {
        return &m_cuData[addr];
    }

    inline Ipp8u GetTrIndex(Ipp32s partAddr) const
    {
        return m_cuData[partAddr].trIndex;
    }

    inline Ipp8u GetLumaIntra(Ipp32s partAddr) const
    {
        return m_lumaIntraDir[partAddr];
    }

    inline Ipp8u GetChromaIntra(Ipp32s partAddr) const
    {
        return m_chromaIntraDir[partAddr];
    }

    inline bool GetIPCMFlag(Ipp32s partAddr) const
    {
        return m_cuData[partAddr].pcm_flag;
    }

    inline Ipp8u GetTransformSkip(Ipp32s plane, Ipp32s partAddr) const
    {
        return (m_cuData[partAddr].transform_skip) & (1 << plane);
    }

    H265_FORCEINLINE Ipp8u GetCbf(ComponentPlane plane, Ipp32s partAddr) const
    {
        if (m_cbf[plane] == 0)
            return 0;
        return m_cbf[plane][partAddr];
    }

    inline Ipp8u GetWidth(Ipp32s partAddr) const
    {
        return m_cuData[partAddr].width;
    }

    inline Ipp8u GetDepth(Ipp32s partAddr) const
    {
        return m_cuData[partAddr].depth;
    }

    inline bool GetCUTransquantBypass(Ipp32s partAddr) const
    {
        return m_cuData[partAddr].cu_transform_bypass;
    }

    Ipp8s                   m_CodedQP;

    // Return whether TU is coded without quantization. For such blocks SAO should be skipped.
    bool isLosslessCoded(Ipp32u absPartIdx);

    H265CodingUnit();
    ~H265CodingUnit();

    // Initialize coding data dependent fields
    void create(H265FrameCodingData * frameCD, Ipp32s cuAddr);
    // Clean up CTB references
    void destroy();

    // Initialize coding unit coordinates and references to frame and slice
    void initCU(H265SegmentDecoderMultiThreaded* sd, Ipp32u CUAddr);
    // Initialize CU subparts' values that happen to be outside of the frame.
    // This data may be needed later to find last valid part index if DQP is enabled.
    void setOutsideCUPart(Ipp32u AbsPartIdx, Ipp32u Depth);

    // Returns CTB address in tile scan in TU units
    Ipp32u getSCUAddr();
    // Set CU partition depth value in left-top corner
    void setDepth(Ipp32u Depth, Ipp32u AbsPartIdx);

    EnumPartSize GetPartitionSize (Ipp32u Idx) const
    {
        return static_cast<EnumPartSize>(m_cuData[Idx].partSize);
    }

    EnumPredMode GetPredictionMode (Ipp32u Idx) const
    {
        return static_cast<EnumPredMode>(m_cuData[Idx].predMode);
    }

    // Set CU partition siize value in left-top corner
    void setPartSizeSubParts(EnumPartSize Mode, Ipp32u AbsPartIdx);
    // Set CU partition transquant bypass flag in left-top corner
    void setCUTransquantBypass(bool flag, Ipp32u AbsPartIdx);
    // Set CU partition prediction mode in left-top corner
    void setPredMode(EnumPredMode Mode, Ipp32u AbsPartIdx);
    // Set CU partition size top-left corner
    void setSize(Ipp32u Width, Ipp32u AbsPartIdx);
    // Set transform depth level for all subparts in CU partition
    void setTrIdx(Ipp32u TrIdx, Ipp32u AbsPartIdx, Ipp32s Depth);
    // Change QP specified in CU subparts after a new QP value is decoded from bitstream
    void UpdateTUQpInfo (Ipp32u AbsPartIdx, Ipp32s qp, Ipp32s Depth);

    Ipp32u getQuadtreeTULog2MinSizeInCU (Ipp32u Idx);

    H265_FORCEINLINE Ipp8u* GetCbf (ComponentPlane plane) const
    {
        return m_cbf[plane];
    }

    H265_FORCEINLINE Ipp8u GetCbf(ComponentPlane plane, Ipp32u Idx, Ipp32u TrDepth) const
    {
        if (m_cbf[plane] == 0)
            return 0;

        return (Ipp8u)((GetCbf(plane, Idx) >> TrDepth ) & 0x1);
    }

    // Propagate CU partition data from left-top corner to all other subparts
    void SetCUDataSubParts(Ipp32u AbsPartIdx, Ipp32u Depth);

    // Set CBF flags for all planes in CU partition
    void setCbfSubParts (Ipp32u CbfY, Ipp32u CbfU, Ipp32u CbfV, Ipp32u AbsPartIdx, Ipp32u Depth);
    // Set CBF flags for one plane in CU partition
    void setCbfSubParts (Ipp32u m_Cbf, ComponentPlane plane, Ipp32u AbsPartIdx, Ipp32u Depth);

    // Set intra luma prediction direction for all partition subparts
    void setLumaIntraDirSubParts(Ipp32u Dir, Ipp32u AbsPartIdx, Ipp32u Depth);
    // Set chroma intra prediction direction for all partition subparts
    void setChromIntraDirSubParts(Ipp32u Dir, Ipp32u AbsPartIdx, Ipp32u Depth);

    // Set CU transform skip flag in left-top corner for specified plane
    void setTransformSkip(Ipp32u useTransformSkip, ComponentPlane plane, Ipp32u AbsPartIdx);

    // Set IPCM flag in top-left corner of CU partition
    void setIPCMFlag (bool IpcmFlag, Ipp32u AbsPartIdx);

    // Returns number of prediction units in CU partition and prediction unit size in pixels
    void getPartIndexAndSize (Ipp32u AbsPartIdx, Ipp32u PartIdx, Ipp32u &Width, Ipp32u &Height);
    // Returns prediction unit size in pixels
    void getPartSize(Ipp32u AbsPartIdx, Ipp32u partIdx, Ipp32s &nPSW, Ipp32s &nPSH);
    // Returns number of prediction units in CU partition
    Ipp8u getNumPartInter(Ipp32u AbsPartIdx);

    bool isDiffMER(Ipp32s xN, Ipp32s yN, Ipp32s xP, Ipp32s yP);

    // Returns whether partition is too small to be predicted not only from L0 reflist
    // (see HEVC specification 8.5.3.2.2).
    bool isBipredRestriction(Ipp32u AbsPartIdx, Ipp32u PartIdx);

    // Derive chroma modes for decoded luma mode. See HEVC specification 8.4.3.
    void getAllowedChromaDir (Ipp32u AbsPartIdx, Ipp32u* ModeList);

    // Calculate possible scan order index for specified CU partition. Inter prediction always uses diagonal scan.
    Ipp32u getCoefScanIdx(Ipp32u AbsPartIdx, Ipp32u L2Width, bool IsLuma, bool IsIntra);

    // Initialize border flags in all directions needed for SAO filters. This is necessary when
    // frame contains more than one slice or more than one tile, and loop filter across slices
    // and tiles may be disabled.
    void setNDBFilterBlockBorderAvailability(bool independentTileBoundaryEnabled);

    // Find last part index which is not outside of the frame (has any mode other than MODE_NONE).
    Ipp32s getLastValidPartIdx(Ipp32s AbsPartIdx);
};

// Data structure used for current CTB context and up row state
struct H265FrameHLDNeighborsInfo
{
    union {
        struct {
            Ipp16u IsAvailable        : 1;  //  1
            Ipp16u IsIntra            : 1;  //  2
            Ipp16u SkipFlag           : 1;  //  3
            Ipp16u Depth              : 3;  //  6
            Ipp16u IntraDir           : 6;  // 12
            Ipp8s qp;
        } members;
        Ipp32u data;
    };
};

} // end namespace UMC_HEVC_DECODER

#endif //__H265_CODING_UNIT_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
