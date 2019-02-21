// Copyright (c) 2012-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#ifdef MFX_ENABLE_H265_VIDEO_DECODE

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
        uint8_t cu_transform_bypass : 1;
        uint8_t pcm_flag : 1;
        uint8_t transform_skip : 5;
        uint8_t depth : 3;
        uint8_t predMode : 2;
        uint8_t partSize : 3;
        uint8_t trIndex : 3;
    };

    uint8_t width;
    int8_t qp;
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
    uint32_t *m_rasterToPelX;
    uint32_t *m_rasterToPelY;

    uint32_t *m_zscanToRaster;
    uint32_t *m_rasterToZscan;

    int32_t                   m_SliceIdx;
    MFX_HEVC_PP::CTBBorders  m_AvailBorder;

    //CU description --------------------------------------------------------------------------------
    uint32_t                    CUAddr;         // CU address in a slice
    uint32_t                    m_CUPelX;         // CU position in a pixel (X)
    uint32_t                    m_CUPelY;         // CU position in a pixel (Y)
    uint32_t                    m_NumPartition;   // total number of minimum partitions in a CU

    //CU data ----------------------------------------------------------------------------------------
    H265CodingUnitData         *m_cuData;

    uint8_t*                    m_cbf[5];         // array of coded block flags (CBF)

    uint8_t*                    m_lumaIntraDir;    // array of intra directions (luma)
    uint8_t*                    m_chromaIntraDir;  // array of intra directions (chroma)

    inline H265CodingUnitData * GetCUData(int32_t addr) const
    {
        return &m_cuData[addr];
    }

    inline uint8_t GetTrIndex(int32_t partAddr) const
    {
        return m_cuData[partAddr].trIndex;
    }

    inline uint8_t GetLumaIntra(int32_t partAddr) const
    {
        return m_lumaIntraDir[partAddr];
    }

    inline uint8_t GetChromaIntra(int32_t partAddr) const
    {
        return m_chromaIntraDir[partAddr];
    }

    inline bool GetIPCMFlag(int32_t partAddr) const
    {
        return m_cuData[partAddr].pcm_flag;
    }

    inline uint8_t GetTransformSkip(int32_t plane, int32_t partAddr) const
    {
        return (m_cuData[partAddr].transform_skip) & (1 << plane);
    }

    H265_FORCEINLINE uint8_t GetCbf(ComponentPlane plane, int32_t partAddr) const
    {
        if (m_cbf[plane] == 0)
            return 0;
        return m_cbf[plane][partAddr];
    }

    inline uint8_t GetWidth(int32_t partAddr) const
    {
        return m_cuData[partAddr].width;
    }

    inline uint8_t GetDepth(int32_t partAddr) const
    {
        return m_cuData[partAddr].depth;
    }

    inline bool GetCUTransquantBypass(int32_t partAddr) const
    {
        return m_cuData[partAddr].cu_transform_bypass;
    }

    int8_t                   m_CodedQP;

    // Return whether TU is coded without quantization. For such blocks SAO should be skipped.
    bool isLosslessCoded(uint32_t absPartIdx);

    H265CodingUnit();
    ~H265CodingUnit();

    // Initialize coding data dependent fields
    void create(H265FrameCodingData * frameCD, int32_t cuAddr);
    // Clean up CTB references
    void destroy();

    // Initialize coding unit coordinates and references to frame and slice
    void initCU(H265SegmentDecoderMultiThreaded* sd, uint32_t CUAddr);
    // Initialize CU subparts' values that happen to be outside of the frame.
    // This data may be needed later to find last valid part index if DQP is enabled.
    void setOutsideCUPart(uint32_t AbsPartIdx, uint32_t Depth);

    // Returns CTB address in tile scan in TU units
    uint32_t getSCUAddr();
    // Set CU partition depth value in left-top corner
    void setDepth(uint32_t Depth, uint32_t AbsPartIdx);

    EnumPartSize GetPartitionSize (uint32_t Idx) const
    {
        return static_cast<EnumPartSize>(m_cuData[Idx].partSize);
    }

    EnumPredMode GetPredictionMode (uint32_t Idx) const
    {
        return static_cast<EnumPredMode>(m_cuData[Idx].predMode);
    }

    // Set CU partition siize value in left-top corner
    void setPartSizeSubParts(EnumPartSize Mode, uint32_t AbsPartIdx);
    // Set CU partition transquant bypass flag in left-top corner
    void setCUTransquantBypass(bool flag, uint32_t AbsPartIdx);
    // Set CU partition prediction mode in left-top corner
    void setPredMode(EnumPredMode Mode, uint32_t AbsPartIdx);
    // Set CU partition size top-left corner
    void setSize(uint32_t Width, uint32_t AbsPartIdx);
    // Set transform depth level for all subparts in CU partition
    void setTrIdx(uint32_t TrIdx, uint32_t AbsPartIdx, int32_t Depth);
    // Change QP specified in CU subparts after a new QP value is decoded from bitstream
    void UpdateTUQpInfo (uint32_t AbsPartIdx, int32_t qp, int32_t Depth);

    uint32_t getQuadtreeTULog2MinSizeInCU (uint32_t Idx);

    H265_FORCEINLINE uint8_t* GetCbf (ComponentPlane plane) const
    {
        return m_cbf[plane];
    }

    H265_FORCEINLINE uint8_t GetCbf(ComponentPlane plane, uint32_t Idx, uint32_t TrDepth) const
    {
        if (m_cbf[plane] == 0)
            return 0;

        return (uint8_t)((GetCbf(plane, Idx) >> TrDepth ) & 0x1);
    }

    // Propagate CU partition data from left-top corner to all other subparts
    void SetCUDataSubParts(uint32_t AbsPartIdx, uint32_t Depth);

    // Set CBF flags for all planes in CU partition
    void setCbfSubParts (uint32_t CbfY, uint32_t CbfU, uint32_t CbfV, uint32_t AbsPartIdx, uint32_t Depth);
    // Set CBF flags for one plane in CU partition
    void setCbfSubParts (uint32_t m_Cbf, ComponentPlane plane, uint32_t AbsPartIdx, uint32_t Depth);

    // Set intra luma prediction direction for all partition subparts
    void setLumaIntraDirSubParts(uint32_t Dir, uint32_t AbsPartIdx, uint32_t Depth);
    // Set chroma intra prediction direction for all partition subparts
    void setChromIntraDirSubParts(uint32_t Dir, uint32_t AbsPartIdx, uint32_t Depth);

    // Set CU transform skip flag in left-top corner for specified plane
    void setTransformSkip(uint32_t useTransformSkip, ComponentPlane plane, uint32_t AbsPartIdx);

    // Set IPCM flag in top-left corner of CU partition
    void setIPCMFlag (bool IpcmFlag, uint32_t AbsPartIdx);

    // Returns number of prediction units in CU partition and prediction unit size in pixels
    void getPartIndexAndSize (uint32_t AbsPartIdx, uint32_t PartIdx, uint32_t &Width, uint32_t &Height);
    // Returns prediction unit size in pixels
    void getPartSize(uint32_t AbsPartIdx, uint32_t partIdx, int32_t &nPSW, int32_t &nPSH);
    // Returns number of prediction units in CU partition
    uint8_t getNumPartInter(uint32_t AbsPartIdx);

    bool isDiffMER(int32_t xN, int32_t yN, int32_t xP, int32_t yP);

    // Returns whether partition is too small to be predicted not only from L0 reflist
    // (see HEVC specification 8.5.3.2.2).
    bool isBipredRestriction(uint32_t AbsPartIdx, uint32_t PartIdx);

    // Derive chroma modes for decoded luma mode. See HEVC specification 8.4.3.
    void getAllowedChromaDir (uint32_t AbsPartIdx, uint32_t* ModeList);

    // Calculate possible scan order index for specified CU partition. Inter prediction always uses diagonal scan.
    uint32_t getCoefScanIdx(uint32_t AbsPartIdx, uint32_t L2Width, bool IsLuma, bool IsIntra);

    // Initialize border flags in all directions needed for SAO filters. This is necessary when
    // frame contains more than one slice or more than one tile, and loop filter across slices
    // and tiles may be disabled.
    void setNDBFilterBlockBorderAvailability(bool independentTileBoundaryEnabled);

    // Find last part index which is not outside of the frame (has any mode other than MODE_NONE).
    int32_t getLastValidPartIdx(int32_t AbsPartIdx);
};

// Data structure used for current CTB context and up row state
struct H265FrameHLDNeighborsInfo
{
    union {
        struct {
            uint16_t IsAvailable        : 1;  //  1
            uint16_t IsIntra            : 1;  //  2
            uint16_t SkipFlag           : 1;  //  3
            uint16_t Depth              : 3;  //  6
            uint16_t IntraDir           : 6;  // 12
            int8_t qp;
        } members;
        uint32_t data;
    };
};

} // end namespace UMC_HEVC_DECODER

#endif //__H265_CODING_UNIT_H
#endif // MFX_ENABLE_H265_VIDEO_DECODE
