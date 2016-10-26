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

#ifndef __UMC_H265_SEGMENT_DECODER_H
#define __UMC_H265_SEGMENT_DECODER_H

#include "umc_h265_tables.h"
#include "umc_h265_frame.h"
#include "umc_h265_bitstream.h"
#include "umc_h265_prediction.h"
#include "mfx_h265_optimization.h"

#include "umc_h265_segment_decoder_base.h"

using namespace MFX_HEVC_PP;

namespace UMC_HEVC_DECODER
{

class H265Task;
class TaskBroker_H265;

struct H265SliceHeader;
struct H265FrameHLDNeighborsInfo;
struct H265MotionVector;
struct H265MVInfo;
struct AMVPInfo;
class H265Prediction;
class H265TrQuant;
class H265Task;

// Reconstructor template base
class ReconstructorBase
{
public:

    virtual ~ReconstructorBase(void) { };

    virtual bool Is8BitsReconstructor() const = 0;

    virtual Ipp32s GetChromaFormat() const = 0;

    // Do luma intra prediction
    virtual void PredictIntra(Ipp32s predMode, PlanePtrY PredPel, PlanePtrY pRec, Ipp32s pitch, Ipp32s width, Ipp32u bit_depth) = 0;

    // Create a buffer of neighbour luma samples for intra prediction
    virtual void GetPredPelsLuma(PlanePtrY pSrc, PlanePtrY PredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf, Ipp32u bit_depth) = 0;

    // Do chroma intra prediction
    virtual void PredictIntraChroma(Ipp32s predMode, PlanePtrY PredPel, PlanePtrY pels, Ipp32s pitch, Ipp32s width) = 0;

    // Create a buffer of neighbour NV12 chroma samples for intra prediction
    virtual void GetPredPelsChromaNV12(PlanePtrY pSrc, PlanePtrY PredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf, Ipp32u bit_depth) = 0;

    // Strong intra smoothing luma filter
    virtual void FilterPredictPels(DecodingContext* sd, H265CodingUnit* pCU, PlanePtrY PredPel, Ipp32s width, Ipp32u TrDepth, Ipp32u AbsPartIdx) = 0;

    // Luma deblocking edge filter
    virtual void FilterEdgeLuma(H265EdgeData *edge, PlanePtrY srcDst, size_t srcDstStride, Ipp32s x, Ipp32s y, Ipp32s dir, Ipp32u bit_depth) = 0;

    // Chroma deblocking edge filter
    virtual void FilterEdgeChroma(H265EdgeData *edge, PlanePtrY srcDst, size_t srcDstStride, Ipp32s x, Ipp32s y, Ipp32s dir, Ipp32s chromaCbQpOffset, Ipp32s chromaCrQpOffset, Ipp32u bit_depth) = 0;

    virtual void CopyPartOfFrameFromRef(PlanePtrY pRefPlane, PlanePtrY pCurrentPlane, Ipp32s pitch,
        Ipp32s offsetX, Ipp32s offsetY, Ipp32s offsetXL, Ipp32s offsetYL,
        IppiSize cuSize, IppiSize frameSize) = 0;

    virtual void DecodePCMBlock(H265Bitstream *bitStream, CoeffsPtr *pcm, Ipp32u size, Ipp32u sampleBits) = 0;

    virtual void ReconstructPCMBlock(PlanePtrY luma, Ipp32s pitchLuma, Ipp32u PcmLeftShiftBitLuma, PlanePtrY chroma, Ipp32s pitchChroma, Ipp32u PcmLeftShiftBitChroma, CoeffsPtr *pcm, Ipp32u size) = 0;

protected:
};

// Slice decoder state
struct Context
{
    H265DecoderRefPicList::ReferenceInformation *m_pRefPicList[2];
    H265Bitstream *m_pBitStream;                                // (H265Bitstream *) pointer to bit stream object

    // external data
    const H265PicParamSet *m_pPicParamSet;                      // (const H265PicParamSet *) pointer to current picture parameters sets
    const H265SeqParamSet *m_pSeqParamSet;                      // (const H265SeqParamSet *) pointer to current sequence parameters sets
    H265DecoderFrame *m_pCurrentFrame;                          // (H265DecoderFrame *) pointer to frame being decoded
    bool                m_bIsNeedWADeblocking;
    bool                m_hasTiles;
    Ipp32u              m_ChromaArrayType;

    H265CodingUnit*     m_cu;

    struct
    {
        bool predictionExist;
        Ipp32u saveColumn;
        Ipp32u saveRow;
        Ipp32u saveHeight;
    } m_deblockPredData[2];

    Ipp32s isMin4x4Block;

    Ipp8u * m_pY_CU_Plane;
    Ipp8u * m_pUV_CU_Plane;
    size_t  m_pitch;

    // intra chroma prediction. For temporal save
    Ipp32u save_lfIf;
    Ipp32u save_tlIf;
    Ipp32u save_tpIf;

    std::auto_ptr<ReconstructorBase>  m_reconstructor;
};

// Slice decoder CTB local state
class DecodingContext : public HeapObject
{
public:

    DecodingContext();

    // Clear all flags in left and top buffers
    virtual void Reset();

    const H265SeqParamSet *m_sps;
    const H265PicParamSet *m_pps;
    const H265DecoderFrame *m_frame;
    const H265SliceHeader *m_sh;

    H265DecoderRefPicList::ReferenceInformation *m_refPicList[2];

    std::vector<H265FrameHLDNeighborsInfo> m_TopNgbrsHolder;
    std::vector<H265FrameHLDNeighborsInfo> m_CurrCTBFlagsHolder;

    CoeffsBuffer    m_coeffBuffer;

    H265Bitstream    m_BitStream;
    CoeffsPtr        m_coeffsRead;
    CoeffsPtr        m_coeffsWrite;

    // Updated after whole CTB is done for initializing external neighbour data in m_CurCTB
    H265FrameHLDNeighborsInfo *m_TopNgbrs;
    // Updated after every PU is processed and MV info is available
    H265FrameHLDNeighborsInfo *m_CurrCTBFlags;
    Ipp32s m_CurrCTBStride;

    Ipp8u    m_weighted_prediction;

    struct
    {
        Ipp32s m_QPRem, m_QPPer;
        Ipp32s m_QPScale;
    } m_ScaledQP[3];

    // Local context for reconstructing Intra
    Ipp32u m_RecIntraFlagsHolder[128]; // Placeholder for Intra availability flags needed during reconstruction
    Ipp32u *m_RecTpIntraFlags;         // 17 x32 flags for top intra blocks for current CTB
    Ipp32u *m_RecLfIntraFlags;         // 17 x32 flags for left intra blocks for current CTB
    Ipp32u *m_RecTLIntraFlags;         // 1 x32 flags for top-left intra blocks for current CTB
    Ipp16u *m_RecTpIntraRowFlags;      // 128 + 2 x16 flags for top line of intra blocks for the frame of 8192 pixels wide

    // mt params
    bool m_needToSplitDecAndRec;
    Ipp32s m_mvsDistortionTemp; // max y component of all mvs in slice
    Ipp32s m_mvsDistortion; // max y component of all mvs in slice

    // Allocate context buffers
    void Init(H265Task *task);
    // Fill up decoder context for new CTB using previous CTB values and values stored for top row
    void UpdateCurrCUContext(Ipp32u lastCUAddr, Ipp32u newCUAddr);
    // Clean up all availability information for decoder
    void ResetRowBuffer();

    // Update reconstruct information with neighbour information for intra prediction
    void UpdateRecCurrCTBContext(Ipp32s lastCUAddr, Ipp32s newCUAddr);
    // Clean up all availability information for reconstruct
    void ResetRecRowBuffer();

    // Set new QP value and calculate scaled values for luma and chroma
    void SetNewQP(Ipp32s newQP, Ipp32s chroma_offset_idx = -1);
    Ipp32s GetQP(void)
    {
        return m_LastValidQP;
    }

    H265_FORCEINLINE Ipp16s *getDequantCoeff(Ipp32u list, Ipp32u qp, Ipp32u size)
    {
        return (*m_dequantCoef)[size][list][qp];
    }
    Ipp16s *(*m_dequantCoef)[SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][SCALING_LIST_REM_NUM];

protected:

    Ipp32s          m_LastValidQP;
};

// Main single thread decoder class
class H265SegmentDecoder : public H265SegmentDecoderBase, public Context
{
public:
    // Initialize new slice decoder instance
    void create(H265SeqParamSet* pSPS);
    //destroy internal buffers
    void destroy();
    // Decode CTB SAO information
    void DecodeSAOOneLCU();
    // Parse merge flags and offsets if needed
    void parseSaoOneLcuInterleaving(bool saoLuma,
                                    bool saoChroma,
                                    Ipp32s allowMergeLeft,
                                    Ipp32s allowMergeUp);

    // Decode SAO truncated rice offset value. HEVC spec 9.3.3.2
    Ipp32s parseSaoMaxUvlc(Ipp32s maxSymbol);
    // Decode SAO type idx
    Ipp32s parseSaoTypeIdx();
    // Decode SAO plane type and offsets
    void parseSaoOffset(SAOLCUParam* psSaoLcuParam, Ipp32u compIdx);

    // Parse truncated rice symbol. HEVC spec 9.3.3.2
    void ReadUnaryMaxSymbolCABAC(Ipp32u& uVal, Ipp32u CtxIdx, Ipp32s Offset, Ipp32u MaxSymbol);
    // Decode CU split flag
    bool DecodeSplitFlagCABAC(Ipp32s PartX, Ipp32s PartY, Ipp32u Depth);
    // Decode inter PU merge index
    Ipp32u DecodeMergeIndexCABAC(void);
    // Decode CU skip flag
    bool DecodeSkipFlagCABAC(Ipp32s PartX, Ipp32s PartY);
    bool DecodeCUTransquantBypassFlag(Ipp32u AbsPartIdx);
    // Decode inter PU MV predictor index
    void DecodeMVPIdxPUCABAC(Ipp32u AbsPartAddr, Ipp32u PartIdx, EnumRefPicList RefList, H265MVInfo &MVi, Ipp8u InterDir);
    // Decode CU prediction mode
    Ipp32s DecodePredModeCABAC(Ipp32u AbsPartIdx);
    // Decode partition size. HEVC spec 9.3.3.5
    void DecodePartSizeCABAC(Ipp32u AbsPartIdx, Ipp32u Depth);
    // Decode IPCM CU flag and samples
    void DecodeIPCMInfoCABAC(Ipp32u AbsPartIdx, Ipp32u Depth);
    // Decode PCM alignment zero bits.
    void DecodePCMAlignBits();
    // Decode luma intra direction
    void DecodeIntraDirLumaAngCABAC(Ipp32u AbsPartIdx, Ipp32u Depth);
    // Decode intra chroma direction. HEVC spec 9.3.3.6
    void DecodeIntraDirChromaCABAC(Ipp32u AbsPartIdx, Ipp32u Depth);
    // Decode inter PU information
    bool DecodePUWiseCABAC(Ipp32u AbsPartIdx, Ipp32u Depth);
    // Decode merge flag
    bool DecodeMergeFlagCABAC(void);
    Ipp8u DecodeInterDirPUCABAC(Ipp32u AbsPartIdx);
    // Decode truncated rice reference frame index for AMVP PU
    RefIndexType DecodeRefFrmIdxPUCABAC(EnumRefPicList RefList, Ipp8u InterDir);
    // Decode MV delta. HEVC spec 7.3.8.9
    void DecodeMVdPUCABAC(EnumRefPicList RefList, H265MotionVector &MVd, Ipp8u InterDir);
    // Decode explicit TU split
    void ParseTransformSubdivFlagCABAC(Ipp32u& SubdivFlag, Ipp32u Log2TransformBlockSize);
    // Decode EG1 coded abs_mvd_minus2 values
    void ReadEpExGolombCABAC(Ipp32u& Value, Ipp32u Count);
    // Decode all CU coefficients
    void DecodeCoeff(Ipp32u AbsPartIdx, Ipp32u Depth, bool& CodeDQP, bool isFirstPartMerge);
    // Recursively decode TU data
    void DecodeTransform(Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u  l2width, Ipp32u TrIdx, bool& CodeDQP);
    // Decode quad tree CBF value
    void ParseQtCbfCABAC(Ipp32u AbsPartIdx, ComponentPlane plane, Ipp32u TrDepth, Ipp32u Depth);
    // Decode root CU CBF value
    void ParseQtRootCbfCABAC(Ipp32u& QtRootCbf);
    // Decode and set new QP value
    void DecodeQP(Ipp32u AbsPartIdx);
    
    void DecodeQPChromaAdujst(); //ChromaQpOffsetCoded
    // Calculate CU QP value based on previously used QP values. HEVC spec 8.6.1
    Ipp32s getRefQP(Ipp32s AbsPartIdx);
    // Decode and set new QP value. HEVC spec 9.3.3.8
    void ParseDeltaQPCABAC(Ipp32u AbsPartIdx);
    void FinishDecodeCU(Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& IsLast);
    // Copy CU data from position 0 to all other subparts
    // This information is needed for reconstruct which may be done in another thread
    void BeforeCoeffs(Ipp32u AbsPartIdx, Ipp32u Depth);
    // Decode CU recursively
    void DecodeCUCABAC(Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& IsLast);
    // Decode slice end flag if necessary
    bool DecodeSliceEnd(Ipp32u AbsPartIdx, Ipp32u Depth);

    // Decode X and Y coordinates of last significant coefficient in a TU block
    Ipp32u ParseLastSignificantXYCABAC(Ipp32u &PosLastX, Ipp32u &PosLastY, Ipp32u L2Width, bool IsLuma, Ipp32u ScanIdx);

    // Parse TU coefficients
    template <bool scaling_list_enabled_flag>
    void ParseCoeffNxNCABACOptimized(CoeffsPtr pCoef, Ipp32u AbsPartIdx, Ipp32u Log2BlockSize, ComponentPlane plane);

    // Parse TU coefficients
    void ParseCoeffNxNCABAC(CoeffsPtr pCoef, Ipp32u AbsPartIdx, Ipp32u Log2BlockSize, ComponentPlane plane);

    // Parse TU transform skip flag
    void ParseTransformSkipFlags(Ipp32u AbsPartIdx, ComponentPlane plane);

    void ReadCoefRemainExGolombCABAC(Ipp32u &Symbol, Ipp32u &GoRiceParam);

    // Recursively produce CU reconstruct from decoded values
    void ReconstructCU(Ipp32u AbsPartIdx, Ipp32u Depth);
    // Reconstruct intra quad tree including handling IPCM
    void ReconIntraQT(Ipp32u AbsPartIdx, Ipp32u Depth);
    // Perform inter CU reconstruction
    void ReconInter(Ipp32u AbsPartIdx, Ipp32u Depth);
    // Place IPCM decoded samples to reconstruct frame
    void ReconPCM(Ipp32u AbsPartIdx, Ipp32u Depth);

    // Reconstruct intra (no IPCM) quad tree recursively
    void IntraRecQT(Ipp32u TrDepth, Ipp32u AbsPartIdx, Ipp32u ChromaPredMode);

    // Reconstruct intra luma block
    void IntraRecLumaBlk(Ipp32u TrDepth,
                         Ipp32u AbsPartIdx,
                         Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf);
    // Reconstruct intra NV12 chroma block
    void IntraRecChromaBlk(Ipp32u TrDepth,
                           Ipp32u AbsPartIdx,
                           Ipp32u ChromaPredMode,
                           Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf);

    void FillReferenceSamplesChroma(
        Ipp32s bitDepth,
        PlanePtrUV pRoiOrigin,
        PlanePtrUV pAdiTemp,
        bool* NeighborFlags,
        Ipp32s NumIntraNeighbor,
        Ipp32s UnitSize,
        Ipp32s NumUnitsInCU,
        Ipp32s TotalUnits,
        Ipp32u CUSize,
        Ipp32u Size,
        Ipp32s PicStride);

    // Deblock edges inside of one CTB, left and top of it
    void DeblockOneLCU(Ipp32s curLCUAddr);
    // Deblock horizontal edge
    void DeblockOneCross(Ipp32s curPixelColumn, Ipp32s curPixelRow, bool isNeddAddHorDeblock);

    // Calculate edge strength
    template <Ipp32s direction, typename EdgeType>
    void CalculateEdge(EdgeType * edge, Ipp32s x, Ipp32s y, bool diffTr);

    // Recursively deblock edges inside of CU
    void DeblockCURecur(Ipp32u absPartIdx, Ipp32u depth);
    // Recursively deblock edges inside of TU
    void DeblockTU(Ipp32u absPartIdx, Ipp32u depth);

    // Edge strength calculation for two inter blocks
    inline void GetEdgeStrengthInter(H265MVInfo *mvinfoQ, H265MVInfo *mvinfoP, H265PartialEdgeData *edge);

    bool m_DecodeDQPFlag;
    bool m_IsCuChromaQpOffsetCoded;
    Ipp32u m_minCUDQPSize;
    Ipp32u m_MaxDepth; //max number of depth
    H265DecYUVBufferPadded* m_ppcYUVResi; //array of residual buffer

    std::auto_ptr<H265Prediction> m_Prediction;

    H265TrQuant* m_TrQuant;
    Ipp32u m_BakAbsPartIdxChroma;
    Ipp32u m_bakAbsPartIdxQp;

    DecodingContext * m_context;
    std::auto_ptr<DecodingContext> m_context_single_thread;

    // Fill up basic CU information in local decoder context
    void UpdateNeighborBuffers(Ipp32u AbsPartIdx, Ipp32u Depth, bool isSkipped);
    // Change QP recorded in CU block of local context to a new value
    void UpdateNeighborDecodedQP(Ipp32u AbsPartIdx, Ipp32u Depth);
    // Fill up inter information in local decoding context and colocated lookup table
    void UpdatePUInfo(Ipp32u PartX, Ipp32u PartY, Ipp32u PartWidth, Ipp32u PartHeight, H265MVInfo &mvInfo);
    // Update intra availability flags for reconstruct
    void UpdateRecNeighboursBuffersN(Ipp32s PartX, Ipp32s PartY, Ipp32s PartSize, bool IsIntra);

    H265Slice *m_pSlice;                                        // (H265Slice *) current slice pointer
    H265SliceHeader *m_pSliceHeader;                      // (H265SliceHeader *) current slice header

    // Default constructor
    H265SegmentDecoder(TaskBroker_H265 * pTaskBroker);
    // Destructor
    virtual ~H265SegmentDecoder(void);

    // Initialize decoder's number
    virtual UMC::Status Init(Ipp32s iNumber);

    // Decode slice's segment
    virtual UMC::Status ProcessSegment(void) = 0;

    virtual UMC::Status ProcessSlice(H265Task & task) = 0;

//protected:
    // Release memory
    void Release(void);

    // Do deblocking task for a range of CTBs specified in the task
    virtual void DeblockSegment(H265Task & task);

private:
    // Calculate CABAC context for split flag. HEVC spec 9.3.4.2.2
    Ipp32u getCtxSplitFlag(Ipp32s PartX, Ipp32s PartY, Ipp32u Depth);
    // Calculate CABAC context for skip flag. HEVC spec 9.3.4.2.2
    Ipp32u getCtxSkipFlag(Ipp32s PartX, Ipp32s PartY);
    // Get predictor array of intra directions for intra luma direction decoding. HEVC spec 8.4.2
    void getIntraDirLumaPredictor(Ipp32u AbsPartIdx, Ipp32s IntraDirPred[]);
    // Prepare an array of motion vector candidates for merge mode. HEVC spec 8.5.3.2.2
    void getInterMergeCandidates(Ipp32u AbsPartIdx, Ipp32u PUIdx, H265MVInfo *MVBufferNeighbours,
        Ipp8u* InterDirNeighbours, Ipp32s mrgCandIdx);
    // Prepare an array of motion vector candidates for AMVP mode. HEVC spec 8.5.3.2.6
    void fillMVPCand(Ipp32u AbsPartIdx, Ipp32u PartIdx, EnumRefPicList RefPicList, Ipp32s RefIdx, AMVPInfo* pInfo);
    // Check availability of spatial MV candidate which points to the same reference frame. HEVC spec 8.5.3.2.7 #6
    bool AddMVPCand(AMVPInfo* pInfo, EnumRefPicList RefPicList, Ipp32s RefIdx, Ipp32s NeibAddr, const H265MVInfo &motionInfo);
    bool AddMVPCandOrder(AMVPInfo* pInfo, EnumRefPicList RefPicList, Ipp32s RefIdx, Ipp32s NeibAddr, const H265MVInfo &motionInfo);
    // Check availability of collocated motion vector with given coordinates. HEVC spec 8.5.3.2.9
    bool GetColMVP(EnumRefPicList refPicListIdx, Ipp32u PartX, Ipp32u PartY, H265MotionVector& rcMV, Ipp32s RefIdx);
    // Returns whether motion information in two cells of collocated motion table is the same
    bool hasEqualMotion(Ipp32s dir1, Ipp32s dir2);

    // constrained intra prediction in reconstruct
    Ipp32s isRecIntraAboveAvailable(Ipp32s TUPartNumberInCTB, Ipp32s NumUnitsInCU, bool *ValidFlags);
    Ipp32s isRecIntraLeftAvailable(Ipp32s TUPartNumberInCTB, Ipp32s NumUnitsInCU, bool *ValidFlags);
    Ipp32s isRecIntraAboveRightAvailable(Ipp32s TUPartNumberInCTB, Ipp32s PartX, Ipp32s XInc, Ipp32s NumUnitsInCU, bool *ValidFlags);
    Ipp32s isRecIntraAboveRightAvailableOtherCTB(Ipp32s PartX, Ipp32s NumUnitsInCU, bool *ValidFlags);
    Ipp32s isRecIntraBelowLeftAvailable(Ipp32s TUPartNumberInCTB, Ipp32s PartY, Ipp32s YInc, Ipp32s NumUnitsInCU, bool *ValidFlags);

    // we lock the assignment operator to avoid any
    // accasional assignments
    H265SegmentDecoder & operator = (H265SegmentDecoder &)
    {
        return *this;

    } // H265SegmentDecoder & operator = (H265SegmentDecoder &)
};

} // namespace UMC_HEVC_DECODER

#endif /* __UMC_H265_SEGMENT_DECODER_H */
#endif // UMC_ENABLE_H265_VIDEO_DECODER
