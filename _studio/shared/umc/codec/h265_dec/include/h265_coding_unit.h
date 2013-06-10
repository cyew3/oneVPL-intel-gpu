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

#ifndef __H265_CODING_UNIT_H
#define __H265_CODING_UNIT_H

#include <vector>


#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_heap.h"
#include "umc_h265_headers.h"
#include "umc_h265_dec.h"
#include "umc_h265_frame.h"
#include "h265_motion_info.h"
#include "h265_global_rom.h"
#include "h265_pattern.h"



namespace UMC_HEVC_DECODER
{

class H265Pattern;
class H265SegmentDecoderMultiThreaded;

class H265CodingUnit
{
    //class H265DecoderFrameInfo;
public:
    //pointers --------------------------------------------------------------------------------------
    H265DecoderFrame*        m_Frame;          // frame pointer
    H265SliceHeader *        m_SliceHeader;    // slice header pointer
    H265Pattern*             m_Pattern;        // neighbour access class pointer

    Ipp32s                   m_SliceIdx;
    bool                     m_AvailBorder[8];

    //CU description --------------------------------------------------------------------------------
    Ipp32u                    CUAddr;         // CU address in a slice
    Ipp32u                    m_AbsIdxInLCU;    // absolute address in a CU. It's Z scan order
    Ipp32u                    m_CUPelX;         // CU position in a pixel (X)
    Ipp32u                    m_CUPelY;         // CU position in a pixel (Y)
    Ipp32u                    m_NumPartition;   // total number of minimum partitions in a CU
    Ipp8u *                    m_WidthArray;     // array of widths
    Ipp8u *                    m_HeightArray;    // array of heights
    Ipp8u *                    m_DepthArray;     // array of depths
    Ipp32s                    m_UnitSize;       // size of a "minimum partition"

    //CU data ----------------------------------------------------------------------------------------
    bool*                     m_skipFlag;           // array of skip flags
    Ipp8s*                    m_PartSizeArray;  // array of partition sizes
    Ipp8s*                    m_PredModeArray;  // array of prediction modes
    bool*                     m_CUTransquantBypass; // array of cu_transquant_bypass flags
    Ipp8u*                    m_QPArray;        // array of QP values
    Ipp8u*                    m_TrIdxArray;     // array of transform indices
    Ipp8u*                    m_TrStartArray;
    Ipp8u*                    m_TransformSkip[3];  // array of transform skipping flags
    Ipp8u*                    m_Cbf[3];         // array of coded block flags (CBF)
    CUMVBuffer                m_CUMVbuffer[2];    // array of motion vectors
    H265CoeffsPtrCommon        m_TrCoeffY;       // transformed coefficient buffer (Y)
    H265CoeffsPtrCommon        m_TrCoeffCb;      // transformed coefficient buffer (Cb)
    H265CoeffsPtrCommon        m_TrCoeffCr;      // transformed coefficient buffer (Cr)


    H265PlanePtrYCommon        m_IPCMSampleY;     // PCM sample buffer (Y)
    H265PlanePtrUVCommon       m_IPCMSampleCb;    // PCM sample buffer (Cb)
    H265PlanePtrUVCommon       m_IPCMSampleCr;    // PCM sample buffer (Cr)

    Ipp32s*                    m_SliceSUMap;         // pointer of slice ID map

    //neighbour access variables -------------------------------------------------------------------------
    H265CodingUnit*            m_CUAboveLeft;     // pointer of above-left CU
    H265CodingUnit*            m_CUAboveRight;    // pointer of above-right CU
    H265CodingUnit*            m_CUAbove;         // pointer of above CU
    H265CodingUnit*            m_CULeft;          // pointer of left CU
    H265CodingUnit*            m_CUColocated[2];  // pointer of temporally colocated CU's for both directions
    MVBuffer                m_MVBufferA;       // motion vector of position A
    MVBuffer                m_MVBufferB;       // motion vector of position B
    MVBuffer                m_MVBufferC;       // motion vector of position C
    MVBuffer                m_MVPred;          // motion vector predictor

    //coding tool information ---------------------------------------------------------------------------
    bool*                    m_MergeFlag;       // array of merge flags
    Ipp8u*                    m_MergeIndex;      // array of merge candidate indices

    Ipp8u*                    m_LumaIntraDir;    // array of intra directions (luma)
    Ipp8u*                    m_ChromaIntraDir;  // array of intra directions (chroma)
    Ipp8u*                    m_InterDir;        // array of inter directions
    Ipp8s*                    m_MVPIdx[2];       // array of motion vector predictor candidates
    Ipp8s*                    m_MVPNum[2];       // array of number of possible motion vectors predictors

    bool*                    m_IPCMFlag;        // array of intra_pcm flags
    // misc. variables -------------------------------------------------------------------------------------
    bool                    m_DecSubCU;         // indicates decoder-mode

    Ipp32u*                    m_SliceStartCU;    // Start CU address of current slice
    Ipp32u*                    m_DependentSliceStartCU; // Start CU address of current slice
    Ipp8s                   m_CodedQP;

    bool isLosslessCoded(Ipp32u absPartIdx);

protected:

    Ipp8u* m_cumulativeMemoryPtr;

    // compute scaling factor from POC difference
    Ipp32s GetDistScaleFactor(Ipp32s DiffPocB, Ipp32s DiffPocD);

    Ipp32s getLastValidPartIdx (Ipp32s AbsPartIdx);

    template <typename T> void setAll(T *p, const T &val, Ipp32u PartWidth, Ipp32u PartHeight);

public:

  H265CodingUnit();
  virtual ~H265CodingUnit();

    // create / destroy / init  / copy -----------------------------------------------------------------------------

    void create (Ipp32u numPartition, Ipp32u Width, Ipp32u Height, bool DecSubCu, Ipp32s unitSize);
    void destroy ();

    void initCU (H265SegmentDecoderMultiThreaded* sd, Ipp32u CUAddr);
    void setOutsideCUPart (Ipp32u AbsPartIdx, Ipp32u Depth);

    void copySubCU (H265CodingUnit* CU, Ipp32u PartUnitIdx, Ipp32u Depth);

    // member functions for CU description ------- (only functions with declaration here. simple get/set are removed)
    Ipp32u getSCUAddr();
    void setDepthSubParts (Ipp32u Depth, Ipp32u AbsPartIdx);

    // member functions for CU data ------------- (only functions with declaration here. simple get/set are removed)
    EnumPartSize getPartitionSize (Ipp32u Idx)
    {
        return static_cast<EnumPartSize>(m_PartSizeArray[Idx]);
    }
    EnumPredMode getPredictionMode (Ipp32u Idx)
    {
        return static_cast<EnumPredMode>(m_PredModeArray[Idx]);
    }

    void setAllColFlags(const Ipp8u flags, Ipp32u RefListIdx, Ipp32u PartX, Ipp32u PartY, Ipp32u PartWidth, Ipp32u PartHeight);
    void setAllColPOCDelta(const Ipp32s POCDelta, Ipp32u RefListIdx, Ipp32u PartX, Ipp32u PartY, Ipp32u PartWidth, Ipp32u PartHeight);
    void setAllColMV(const H265MotionVector &mv, Ipp32u RefListIdx, Ipp32u PartX, Ipp32u PartY, Ipp32u PartWidth, Ipp32u PartHeight);

    void setPartSizeSubParts (EnumPartSize Mode, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setCUTransquantBypassSubParts(bool flag, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setSkipFlagSubParts(bool skip, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setPredModeSubParts (EnumPredMode Mode, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setSizeSubParts (Ipp32u Width, Ipp32u Height, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setQPSubParts (Ipp32u QP, Ipp32u AbsPartIdx, Ipp32u Depth);
    Ipp8u getLastCodedQP (Ipp32u AbsPartIdx);
    void setTrIdxSubParts (Ipp32u TrIdx, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setTrStartSubParts (Ipp32u AbsPartIdx, Ipp32u Depth);

    Ipp32u getQuadtreeTULog2MinSizeInCU (Ipp32u Idx);

#if (HEVC_OPT_CHANGES & 2)
    H265_FORCEINLINE
#endif
    Ipp8u* getCbf (EnumTextType Type)
    {
        return m_Cbf[g_ConvertTxtTypeToIdx[Type]];
    }
#if (HEVC_OPT_CHANGES & 2)
    H265_FORCEINLINE
#endif
    Ipp8u getCbf(Ipp32u Idx, EnumTextType Type)
    {
        return m_Cbf[g_ConvertTxtTypeToIdx[Type]][Idx];
    }
#if (HEVC_OPT_CHANGES & 2)
    H265_FORCEINLINE
#endif
    Ipp8u getCbf(Ipp32u Idx, EnumTextType Type, Ipp32u TrDepth)
    {
        return (Ipp8u)((getCbf(Idx, Type) >> TrDepth ) & 0x1);
    }

    void setCbfSubParts (Ipp32u CbfY, Ipp32u CbfU, Ipp32u CbfV, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setCbfSubParts (Ipp32u m_Cbf, EnumTextType TType, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setCbfSubParts (Ipp32u m_Cbf, EnumTextType TType, Ipp32u AbsPartIdx, Ipp32u PartIdx, Ipp32u Depth);

    // member functions for coding tool information (only functions with declaration here. simple get/set are removed)
    void setMergeFlagSubParts (bool m_MergeFlag, Ipp32u AbsPartIdx, Ipp32u PartIdx, Ipp32u Depth);
    void setMergeIndexSubParts (Ipp32u MergeIndex, Ipp32u AbsPartIdx, Ipp32u PartIdx, Ipp32u Depth);
    template <typename T>
    void setSubPart (T Parameter, T* pBaseLCU, Ipp32u CUAddr, Ipp32u CUDepth, Ipp32u PUIdx);
    void setLumaIntraDirSubParts (Ipp32u Dir, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setChromIntraDirSubParts (Ipp32u Dir, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setInterDirSubParts (Ipp32u Dir, Ipp32u AbsPartIdx, Ipp32u PartIdx, Ipp32u Depth);

    void setTransformSkipSubParts(Ipp32u useTransformSkip, EnumTextType Type, Ipp32u AbsPartIdx, Ipp32u Depth);

    void setIPCMFlagSubParts (bool IpcmFlag, Ipp32u AbsPartIdx, Ipp32u Depth);

    // member functions for accessing partition information -----------------------------------------------------------
    void getPartIndexAndSize (Ipp32u AbsPartIdx, Ipp32u PartIdx, Ipp32u &PartAddr, Ipp32u &Width, Ipp32u &Height);
    void getPartSize(Ipp32u AbsPartIdx, Ipp32u partIdx, Ipp32s &nPSW, Ipp32s &nPSH);
    Ipp8u getNumPartInter(Ipp32u AbsPartIdx);

    // member functions for motion vector ---------- (only functions with declaration here. simple get/set are removed)
    void getMVBuffer (H265CodingUnit* pCU, Ipp32u AbsPartIdx, EnumRefPicList RefPicList, MVBuffer& MVbuffer);

    void fillMVPCand (Ipp32u PartIdx, Ipp32u PartAddr, EnumRefPicList RefPicList, Ipp32s RefIdx, AMVPInfo* pInfo);
    bool isDiffMER(Ipp32s xN, Ipp32s yN, Ipp32s xP, Ipp32s yP);
    Ipp32s searchMVPIdx (H265MotionVector MV,  AMVPInfo* pInfo);

    void setMVPIdxSubParts (Ipp32s m_MVPIdx, EnumRefPicList RefPicList, Ipp32u AbsPartIdx, Ipp32u PartIdx, Ipp32u Depth);
    void setMVPNumSubParts (Ipp32s m_MVPNum, EnumRefPicList RefPicList, Ipp32u AbsPartIdx, Ipp32u PartIdx, Ipp32u Depth);


    // utility functions for neighbouring information - (only functions with declaration here. simple get/set are removed)
    H265CodingUnit* getPULeft        (Ipp32u& LPartUnitIdx , Ipp32u CurrPartUnitIdx, bool EnforceSliceRestriction = true, bool bEnforceTileRestriction = true);
    H265CodingUnit* getPUAbove       (Ipp32u& APartUnitIdx , Ipp32u CurrPartUnitIdx, bool EnforceSliceRestriction = true, bool planarAtLCUBoundary = false, bool EnforceTileRestriction = true);
    H265CodingUnit* getPUAboveLeft   (Ipp32u& ALPartUnitIdx, Ipp32u CurrPartUnitIdx, bool EnforceSliceRestriction = true);
    H265CodingUnit* getPUAboveRight  (Ipp32u& ARPartUnitIdx, Ipp32u CurrPartUnitIdx, bool EnforceSliceRestriction = true);
    H265CodingUnit* getPUBelowLeft   (Ipp32u& BLPartUnitIdx, Ipp32u CurrPartUnitIdx, bool EnforceSliceRestriction = true);

    H265CodingUnit* getQPMinCULeft (Ipp32u& LPartUnitIdx, Ipp32u CurrAbsIdxInLCU);
    H265CodingUnit* getQPMinCUAbove(Ipp32u& aPartUnitIdx, Ipp32u currAbsIdxInLCU);
    Ipp8u getRefQP (Ipp32u CurrAbsIdxInLCU);

    H265CodingUnit* getPUAboveRightAdi (Ipp32u& ARPartUnitIdx, Ipp32u CurrPartUnitIdx, Ipp32u PartUnitOffset = 1, bool EnforceSliceRestriction = true);
    H265CodingUnit* getPUBelowLeftAdi (Ipp32u& BLPartUnitIdx, Ipp32u CurrPartUnitIdx, Ipp32u PartUnitOffset = 1, bool EnforceSliceRestriction = true);

    void deriveLeftRightTopIdxAdi (Ipp32u& PartIdxLT, Ipp32u& PartIdxRT, Ipp32u PartOffset, Ipp32u PartDepth);
    void deriveLeftBottomIdxAdi (Ipp32u& PartIdxLB, Ipp32u  PartOffset, Ipp32u PartDepth);

    bool hasEqualMotion (Ipp32u AbsPartIdx, H265CodingUnit* CandCU, Ipp32u CandAbsPartIdx);

    // member functions for modes ---------------------- (only functions with declaration here. simple get/set are removed)
    bool isSkipped (Ipp32u PartIdx);  // SKIP - no residual
    bool isBipredRestriction(Ipp32u AbsPartIdx, Ipp32u PartIdx);

    void getAllowedChromaDir (Ipp32u AbsPartIdx, Ipp32u* ModeList);

    // member functions for SBAC context ----------------------------------------------------------------------------------
    Ipp32u getCtxQtCbf (EnumTextType Type, Ipp32u TrDepth);
    Ipp32u getCtxInterDir (Ipp32u AbsPartIdx);

    Ipp32u getSliceStartCU (Ipp32u pos) { return m_SliceStartCU[pos - m_AbsIdxInLCU]; }
    Ipp32u getSliceSegmentStartCU (Ipp32u pos) { return m_DependentSliceStartCU[pos - m_AbsIdxInLCU]; }

    // member functions for RD cost storage  ------------(only functions with declaration here. simple get/set are removed)
    Ipp32u getCoefScanIdx(Ipp32u AbsPartIdx, Ipp32u Width, bool IsLuma, bool IsIntra);

    void setNDBFilterBlockBorderAvailability(bool independentTileBoundaryEnabled);
};

struct H265FrameHLDNeighborsInfo
{
    union {
        struct {
            Ipp16u IntraDir    : 6;
            Ipp16u Depth       : 3;
            Ipp16u SkipFlag    : 1;
            Ipp16u IsIntra     : 1;
            Ipp16u IsAvailable : 1;
        } members;
        Ipp16u data;
    };
};

struct H265TUData
{
    MVBuffer mvinfo[2];
};

namespace RasterAddress
{
  /** Check whether 2 addresses point to the same column
   * \param addrA          First address in raster scan order
   * \param addrB          Second address in raters scan order
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline bool isEqualCol (Ipp32s addrA, Ipp32s addrB, Ipp32s numUnitsPerRow)
  {
    // addrA % numUnitsPerRow == addrB % numUnitsPerRow
    return ((addrA ^ addrB) & (numUnitsPerRow - 1)) == 0;
  }

  /** Check whether 2 addresses point to the same row
   * \param addrA          First address in raster scan order
   * \param addrB          Second address in raters scan order
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline bool isEqualRow (Ipp32s addrA, Ipp32s addrB, Ipp32s numUnitsPerRow)
  {
    // addrA / numUnitsPerRow == addrB / numUnitsPerRow
    return ((addrA ^ addrB) &~ (numUnitsPerRow - 1)) == 0;
  }

  /** Check whether 2 addresses point to the same row or column
   * \param addrA          First address in raster scan order
   * \param addrB          Second address in raters scan order
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline bool isEqualRowOrCol (Ipp32s addrA, Ipp32s addrB, Ipp32s numUnitsPerRow)
  {
    return isEqualCol(addrA, addrB, numUnitsPerRow) | isEqualRow(addrA, addrB, numUnitsPerRow);
  }

  /** Check whether one address points to the first column
   * \param addr           Address in raster scan order
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline bool isZeroCol (Ipp32s addr, Ipp32s numUnitsPerRow)
  {
    // addr % numUnitsPerRow == 0
    return (addr & (numUnitsPerRow - 1)) == 0;
  }

  /** Check whether one address points to the first row
   * \param addr           Address in raster scan order
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline bool isZeroRow (Ipp32s addr, Ipp32s numUnitsPerRow)
  {
    // addr / numUnitsPerRow == 0
    return (addr &~ (numUnitsPerRow - 1)) == 0;
  }

  /** Check whether one address points to a column whose index is smaller than a given value
   * \param addr           Address in raster scan order
   * \param val            Given column index value
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline bool lessThanCol (Ipp32s addr, Ipp32s val, Ipp32s numUnitsPerRow)
  {
    // addr % numUnitsPerRow < val
    return (addr & (numUnitsPerRow - 1)) < val;
  }

  /** Check whether one address points to a row whose index is smaller than a given value
   * \param addr           Address in raster scan order
   * \param val            Given row index value
   * \param numUnitsPerRow Number of units in a row
   * \return Result of test
   */
  static inline bool lessThanRow (Ipp32s addr, Ipp32s val, Ipp32s numUnitsPerRow)
  {
    // addr / numUnitsPerRow < val
    return addr < val * numUnitsPerRow;
  }
}

} // end namespace UMC_HEVC_DECODER

#endif //__H265_CODING_UNIT_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
