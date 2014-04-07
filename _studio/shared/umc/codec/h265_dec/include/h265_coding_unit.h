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

#ifndef __H265_CODING_UNIT_H
#define __H265_CODING_UNIT_H

#include <vector>

#include "mfx_h265_optimization.h"
#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_heap.h"
#include "umc_h265_headers.h"
#include "umc_h265_dec.h"
#include "umc_h265_frame.h"
#include "h265_motion_info.h"
#include "h265_global_rom.h"

namespace UMC_HEVC_DECODER
{

class H265Pattern;
class H265SegmentDecoderMultiThreaded;

#pragma pack(1)
struct H265CodingUnitData
{
    struct
    {
        Ipp8u cu_transform_bypass : 1;
        Ipp8u pcm_flag : 1;
        Ipp8u transform_skip : 3;
        Ipp8u depth : 3;
        Ipp8u predMode : 2;
        Ipp8u partSize : 3;
        Ipp8u trIndex : 3;
    };

    Ipp8u width;
    Ipp8s qp;
};
#pragma pack()

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

public:
    Ipp8u*                    m_cbf[3];         // array of coded block flags (CBF)

    Ipp8u*                    m_lumaIntraDir;    // array of intra directions (luma)
    Ipp8u*                    m_chromaIntraDir;  // array of intra directions (chroma)
public:

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

    // misc. variables -------------------------------------------------------------------------------------
    Ipp8s                   m_CodedQP;

    bool isLosslessCoded(Ipp32u absPartIdx);

protected:

    // compute scaling factor from POC difference
    Ipp32s GetDistScaleFactor(Ipp32s DiffPocB, Ipp32s DiffPocD);

public:

    H265CodingUnit();
    virtual ~H265CodingUnit();

    // create / destroy / init  / copy -----------------------------------------------------------------------------

    void create (H265FrameCodingData * frameCD, Ipp32s cuAddr);
    void destroy ();

    void initCU (H265SegmentDecoderMultiThreaded* sd, Ipp32u CUAddr);
    void setOutsideCUPart (Ipp32u AbsPartIdx, Ipp32u Depth);

    // member functions for CU description ------- (only functions with declaration here. simple get/set are removed)
    Ipp32u getSCUAddr();
    void setDepth (Ipp32u Depth, Ipp32u AbsPartIdx);

    // member functions for CU data ------------- (only functions with declaration here. simple get/set are removed)
    EnumPartSize GetPartitionSize (Ipp32u Idx) const
    {
        return static_cast<EnumPartSize>(m_cuData[Idx].partSize);
    }

    EnumPredMode GetPredictionMode (Ipp32u Idx) const
    {
        return static_cast<EnumPredMode>(m_cuData[Idx].predMode);
    }

    void setPartSizeSubParts (EnumPartSize Mode, Ipp32u AbsPartIdx);
    void setCUTransquantBypass(bool flag, Ipp32u AbsPartIdx);
    void setPredMode (EnumPredMode Mode, Ipp32u AbsPartIdx);
    void setSize (Ipp32u Width, Ipp32u AbsPartIdx);
    void setTrIdx (Ipp32u TrIdx, Ipp32u AbsPartIdx, Ipp32s Depth);
    void UpdateTUQpInfo (Ipp32u AbsPartIdx, Ipp32s qp, Ipp32s Depth);

    Ipp32u getQuadtreeTULog2MinSizeInCU (Ipp32u Idx);

    H265_FORCEINLINE Ipp8u* GetCbf (ComponentPlane plane) const
    {
        return m_cbf[plane];
    }

    H265_FORCEINLINE Ipp8u GetCbf(ComponentPlane plane, Ipp32u Idx, Ipp32u TrDepth) const
    {
        return (Ipp8u)((GetCbf(plane, Idx) >> TrDepth ) & 0x1);
    }

    void SetCUDataSubParts(Ipp32u AbsPartIdx, Ipp32u Depth);

    void setCbfSubParts (Ipp32u CbfY, Ipp32u CbfU, Ipp32u CbfV, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setCbfSubParts (Ipp32u m_Cbf, ComponentPlane plane, Ipp32u AbsPartIdx, Ipp32u Depth);

    // member functions for coding tool information (only functions with declaration here. simple get/set are removed)
    void setLumaIntraDirSubParts (Ipp32u Dir, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setChromIntraDirSubParts (Ipp32u Dir, Ipp32u AbsPartIdx, Ipp32u Depth);

    void setTransformSkip(Ipp32u useTransformSkip, ComponentPlane plane, Ipp32u AbsPartIdx);

    void setIPCMFlag (bool IpcmFlag, Ipp32u AbsPartIdx);

    // member functions for accessing partition information -----------------------------------------------------------
    void getPartIndexAndSize (Ipp32u AbsPartIdx, Ipp32u PartIdx, Ipp32u &Width, Ipp32u &Height);
    void getPartSize(Ipp32u AbsPartIdx, Ipp32u partIdx, Ipp32s &nPSW, Ipp32s &nPSH);
    Ipp8u getNumPartInter(Ipp32u AbsPartIdx);

    bool isDiffMER(Ipp32s xN, Ipp32s yN, Ipp32s xP, Ipp32s yP);

    // member functions for modes ---------------------- (only functions with declaration here. simple get/set are removed)
    bool isBipredRestriction(Ipp32u AbsPartIdx, Ipp32u PartIdx);

    void getAllowedChromaDir (Ipp32u AbsPartIdx, Ipp32u* ModeList);

    // member functions for RD cost storage  ------------(only functions with declaration here. simple get/set are removed)
    Ipp32u getCoefScanIdx(Ipp32u AbsPartIdx, Ipp32u L2Width, bool IsLuma, bool IsIntra);

    void setNDBFilterBlockBorderAvailability(bool independentTileBoundaryEnabled);
    Ipp32s getLastValidPartIdx(Ipp32s AbsPartIdx);
};

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

struct H265PUInfo
{
    H265MVInfo *interinfo;
    H265DecoderFrame *refFrame[2];
    Ipp32u PartAddr;
    Ipp32u Width, Height;
};

} // end namespace UMC_HEVC_DECODER

#endif //__H265_CODING_UNIT_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
