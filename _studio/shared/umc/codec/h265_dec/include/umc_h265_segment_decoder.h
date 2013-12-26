/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_SEGMENT_DECODER_H
#define __UMC_H265_SEGMENT_DECODER_H

#include "umc_h265_dec.h"
#include "umc_h265_dec_tables.h"

#include "umc_h265_thread.h"
#include "umc_h265_frame.h"

#include "h265_prediction.h"
#include "mfx_h265_optimization.h"

using namespace MFX_HEVC_PP;

namespace UMC_HEVC_DECODER
{

struct H265SliceHeader;
struct H265FrameHLDNeighborsInfo;
struct H265MotionVector;
struct H265MVInfo;
struct SAOParams;
struct AMVPInfo;
class H265Prediction;
class H265TrQuant;
class TaskBroker_H265;
class H265Task;

//
// Class to incapsulate functions, implementing common decoding functional.
//

class ReconstructorBase
{
public:

    virtual void PredictIntra(Ipp32s predMode, H265PlaneYCommon* PredPel, H265PlaneYCommon* pRec, Ipp32s pitch, Ipp32s width) = 0;

    virtual void GetPredPelsLuma(H265PlaneYCommon* pSrc, H265PlaneYCommon* PredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf) = 0;

    virtual void PredictIntraChroma(Ipp32s predMode, H265PlaneYCommon* PredPel, H265PlaneYCommon* pels, Ipp32s pitch, Ipp32s width) = 0;

    virtual void GetPredPelsChromaNV12(H265PlaneYCommon* pSrc, H265PlaneYCommon* PredPel, Ipp32s blkSize, Ipp32s srcPitch, Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf) = 0;

    virtual void FilterPredictPels(DecodingContext* sd, H265CodingUnit* pCU, H265PlaneYCommon* PredPel, Ipp32s width, Ipp32u TrDepth, Ipp32u AbsPartIdx) = 0;

    virtual void FilterEdgeLuma(H265EdgeData *edge, H265PlaneYCommon *srcDst, Ipp32s srcDstStride, Ipp32s x, Ipp32s y, Ipp32s dir) = 0;

protected:
};

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


    std::auto_ptr<ReconstructorBase>  m_reconstructor;
};

class DecodingContext : public HeapObject
{
public:

    DecodingContext();

    virtual void Reset();

    const H265SeqParamSet *m_sps;
    const H265PicParamSet *m_pps;
    const H265DecoderFrame *m_frame;
    const H265SliceHeader *m_sh;

    H265DecoderRefPicList::ReferenceInformation *m_refPicList[2];

    std::vector<H265FrameHLDNeighborsInfo> m_TopNgbrsHolder;
    std::vector<H265MVInfo> m_TopMVInfoHolder;
    std::vector<H265MVInfo> m_CurrCTBHolder;
    std::vector<H265FrameHLDNeighborsInfo> m_CurrCTBFlagsHolder;

    // Updated after whole CTB is done for initializing external neighbour data in m_CurCTB
    H265FrameHLDNeighborsInfo *m_TopNgbrs;
    H265MVInfo *m_TopMVInfo;
    // Updated after every PU is processed and MV info is available
    H265FrameHLDNeighborsInfo *m_CurrCTBFlags;
    H265MVInfo *m_CurrCTB;
    Ipp32s m_CurrCTBStride;

    struct
    {
        Ipp32s m_QPRem, m_QPPer;
        Ipp16s m_QPScale;
    } m_ScaledQP[3];

    // Local context for reconstructing Intra
    Ipp32u m_RecIntraFlagsHolder[128]; // Placeholder for Intra availability flags needed during reconstruction
    Ipp32u *m_RecTpIntraFlags;         // 17 x32 flags for top intra blocks for current CTB
    Ipp32u *m_RecLfIntraFlags;         // 17 x32 flags for left intra blocks for current CTB
    Ipp32u *m_RecTLIntraFlags;         // 1 x32 flags for top-left intra blocks for current CTB
    Ipp16u *m_RecTpIntraRowFlags;      // 128 + 2 x16 flags for top line of intra blocks for the frame of 8192 pixels wide

    // mt params
    bool m_needToSplitDecAndRec;
    Ipp32s m_mvsDistortion; // max y component of all mvs in slice

    void Init(H265Slice *slice);
    void UpdateCurrCUContext(Ipp32u lastCUAddr, Ipp32u newCUAddr);
    void ResetRowBuffer();

    void UpdateRecCurrCTBContext(Ipp32s lastCUAddr, Ipp32s newCUAddr);
    void ResetRecRowBuffer();
    
    void SetNewQP(Ipp32s newQP);
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

    Ipp32s m_LastValidQP;
};

class H265SegmentDecoder : public Context
{
public:
    //create internal buffers
    void create(H265SeqParamSet* pSPS);
    //destroy internal buffers
    void destroy();
    void DecodeSAOOneLCU(H265CodingUnit* pCU);
    void parseSaoOneLcuInterleaving(bool saoLuma,
                                    bool saoChroma,
                                    H265CodingUnit* pcCU,
                                    Ipp32s allowMergeLeft,
                                    Ipp32s allowMergeUp);

    void parseSaoMaxUvlc(Ipp32u& val, Ipp32u maxSymbol);
    void parseSaoUflc(Ipp32s length, Ipp32u&  riVal);
    void parseSaoMerge(Ipp32u&  ruiVal);
    void parseSaoTypeIdx(Ipp32u&  ruiVal);
    void parseSaoOffset(SAOLCUParam* psSaoLcuParam, Ipp32u compIdx);

    void ReadUnaryMaxSymbolCABAC(Ipp32u& uVal, Ipp32u CtxIdx, Ipp32s Offset, Ipp32u MaxSymbol);
    bool DecodeSplitFlagCABAC(H265CodingUnit* pCU, Ipp32s PartX, Ipp32s PartY, Ipp32u Depth);
    Ipp32u DecodeMergeIndexCABAC(void);

    bool DecodeSkipFlagCABAC(Ipp32s PartX, Ipp32s PartY);
    bool DecodeCUTransquantBypassFlag(H265CodingUnit* pCU, Ipp32u AbsPartIdx);
    void DecodeMVPIdxPUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartAddr, Ipp32u PartIdx, EnumRefPicList RefList, H265MVInfo &MVi, Ipp8u InterDir);
    Ipp32s DecodePredModeCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx);
    void DecodePartSizeCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodeIPCMInfoCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodePCMAlignBits();
    void DecodeIntraDirLumaAngCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodeIntraDirChromaCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    bool DecodePUWiseCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    bool DecodeMergeFlagCABAC(void);
    Ipp8u DecodeInterDirPUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx);
    RefIndexType DecodeRefFrmIdxPUCABAC(EnumRefPicList RefList, Ipp8u InterDir);
    void DecodeMVdPUCABAC(EnumRefPicList RefList, H265MotionVector &MVd, Ipp8u InterDir);
    void ParseTransformSubdivFlagCABAC(Ipp32u& SubdivFlag, Ipp32u Log2TransformBlockSize);
    void ReadEpExGolombCABAC(Ipp32u& Value, Ipp32u Count);
    void DecodeCoeff(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, bool& CodeDQP, bool isFirstPartMerge);
    void DecodeTransform(H265CodingUnit* pCU, Ipp32u offsetLuma, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u  l2width, Ipp32u TrIdx, bool& CodeDQP);
    void ParseQtCbfCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, ComponentPlane plane, Ipp32u TrDepth, Ipp32u Depth);
    void ParseQtRootCbfCABAC(Ipp32u& QtRootCbf);
    void DecodeQP(H265CodingUnit* pCU, Ipp32u AbsPartIdx);
    Ipp32s getRefQP (H265CodingUnit *pCU, Ipp32s AbsPartIdx);
    void ParseDeltaQPCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx);
    void ReadUnarySymbolCABAC(Ipp32u& Value, Ipp32s ctxIdx, Ipp32s Offset);
    void FinishDecodeCU(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& IsLast);
    void BeforeCoeffs(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodeCUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& IsLast);
    void SaveCTBContext(H265CodingUnit* pCU);
    bool DecodeSliceEnd(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);

    Ipp32u ParseLastSignificantXYCABAC(Ipp32u &PosLastX, Ipp32u &PosLastY, Ipp32u L2Width, bool IsLuma, Ipp32u ScanIdx);

    template <bool scaling_list_enabled_flag>
    void ParseCoeffNxNCABACOptimized(H265CodingUnit* pCU, H265CoeffsPtrCommon pCoef, Ipp32u AbsPartIdx, Ipp32u Log2BlockSize, Ipp32u Depth, ComponentPlane plane);

    void ParseCoeffNxNCABAC(H265CodingUnit* pCU, H265CoeffsPtrCommon pCoef, Ipp32u AbsPartIdx, Ipp32u Log2BlockSize, Ipp32u Depth, ComponentPlane plane);

    void ParseTransformSkipFlags(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, ComponentPlane plane);

    void ReadCoefRemainExGolombCABAC(Ipp32u &Symbol, Ipp32u &GoRiceParam);

    void ReconstructCU(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void ReconIntraQT(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void ReconInter(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void ReconPCM(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void FillPCMBuffer(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodeInterTexture(H265CodingUnit* pCU, Ipp32u AbsPartIdx);

    void IntraRecQT(H265CodingUnit* pCU, Ipp32u TrDepth, Ipp32u AbsPartIdx, Ipp32u ChromaPredMode);

    void IntraRecLumaBlk(H265CodingUnit* pCU,
                         Ipp32u TrDepth,
                         Ipp32u AbsPartIdx,
                         Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf);
    void IntraRecChromaBlk(H265CodingUnit* pCU,
                           Ipp32u TrDepth,
                           Ipp32u AbsPartIdx,
                           Ipp32u ChromaPredMode,
                           Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf);

    void InitNeighbourPatternChroma(
        H265CodingUnit* pCU,
        Ipp32u ZorderIdxInPart,
        Ipp32u PartDepth,
        H265PlanePtrUVCommon pAdiBuf,
        Ipp32u OrgBufStride,
        Ipp32u OrgBufHeight,
        bool *neighborAvailable,
        Ipp32s numIntraNeighbors);

    void FillReferenceSamplesChroma(
        Ipp32s bitDepth,
        H265PlanePtrUVCommon pRoiOrigin,
        H265PlanePtrUVCommon pAdiTemp,
        bool* NeighborFlags,
        Ipp32s NumIntraNeighbor,
        Ipp32s UnitSize,
        Ipp32s NumUnitsInCU,
        Ipp32s TotalUnits,
        Ipp32u CUSize,
        Ipp32u Size,
        Ipp32s PicStride);

    void CleanLeftEdges(bool leftAvailable, H265EdgeData *ctb_start_edge, Ipp32s height);
    void CleanTopEdges(bool leftAvailable, H265EdgeData *ctb_start_edge, Ipp32s width);
    void CleanRightHorEdges(void);
    void DeblockOneLCU(Ipp32s curLCUAddr);
    void DeblockOneCrossLuma(H265CodingUnit* curLCU, Ipp32s curPixelColumn, Ipp32s curPixelRow);
    void DeblockOneCrossChroma(H265CodingUnit* curLCU, Ipp32s curPixelColumn, Ipp32s curPixelRow);

    void GetCTBEdgeStrengths(void);
    template<Ipp32s tusize> inline void GetCTBEdgeStrengths(void);
    template<Ipp32s tusize, Ipp32s dir> inline void GetEdgeStrength(Ipp32s tuQ, Ipp32s tuP, H265EdgeData *edge, Ipp32s xQ, Ipp32s yQ);
    template<Ipp32s dir> inline void GetEdgeStrengthDelayed(Ipp32s tusize, Ipp32s x, Ipp32s y, H265EdgeData *edge);
    inline void GetEdgeStrengthInter(H265MVInfo *mvinfoQ, H265MVInfo *mvinfoP, H265EdgeData *edge);
    template<Ipp32s tusize> void GetCTBEdgeStrengthsSimple(void);
    template<Ipp32s tusize> void GetEdgeStrengthSimple(Ipp32s tuQ, Ipp32s tuP, H265EdgeData *edge, bool anotherCU);

    H265CodingUnit* m_curCU;
    bool m_DecodeDQPFlag;
    Ipp32s m_minCUDQPSize;
    Ipp32u m_MaxDepth; //max number of depth
    H265DecYUVBufferPadded* m_ppcYUVResi; //array of residual buffer

    std::auto_ptr<H265Prediction> m_Prediction;

    H265TrQuant* m_TrQuant;
    Ipp32u m_BakAbsPartIdx;
    Ipp32u m_BakChromaOffset;
    Ipp32u m_bakAbsPartIdxCU;

    DecodingContext * m_context;
    std::auto_ptr<DecodingContext> m_context_single_thread;

    void UpdateNeighborBuffers(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u TrStart, bool isSkipped, bool isTranquantBypass, bool isIPCM, bool isTrCbfY);
    void UpdateNeighborDecodedQP(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void UpdatePUInfo(Ipp32u PartX, Ipp32u PartY, Ipp32u PartWidth, Ipp32u PartHeight, H265MVInfo &mvInfo);
    void UpdateRecNeighboursBuffersN(Ipp32s PartX, Ipp32s PartY, Ipp32s PartSize, bool IsIntra);

    Ipp32s m_iNumber;                                           // (Ipp32s) ordinal number of decoder
    H265Slice *m_pSlice;                                        // (H265Slice *) current slice pointer
    H265SliceHeader *m_pSliceHeader;                      // (H265SliceHeader *) current slice header
    TaskBroker_H265 * m_pTaskBroker;

    // Default constructor
    H265SegmentDecoder(TaskBroker_H265 * pTaskBroker);
    // Destructor
    virtual ~H265SegmentDecoder(void);

    // Initialize object
    virtual UMC::Status Init(Ipp32s iNumber);

    // Decode slice's segment
    virtual UMC::Status ProcessSegment(void) = 0;

    virtual UMC::Status ProcessSlice(H265Task & task) = 0;

//protected:
    // Release object
    void Release(void);

    // Function to de-block partition of macro block row
    virtual void DeblockSegment(H265Task & task);

private:
    Ipp32u getCtxSplitFlag(H265CodingUnit *pCU, Ipp32s PartX, Ipp32s PartY, Ipp32u Depth);
    Ipp32u getCtxSkipFlag(Ipp32s PartX, Ipp32s PartY);
    void getIntraDirLumaPredictor(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32s IntraDirPred[]);
    void getInterMergeCandidates(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32u PUIdx, H265MVInfo *MVBufferNeighbours,
        Ipp8u* InterDirNeighbours, Ipp32s &numValidMergeCand, Ipp32s mrgCandIdx);
    void fillMVPCand(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32u PartIdx, EnumRefPicList RefPicList, Ipp32s RefIdx, AMVPInfo* pInfo);
    // add possible motion vector predictor candidates
    bool AddMVPCand(AMVPInfo* pInfo, EnumRefPicList RefPicList, Ipp32s RefIdx, Ipp32s NeibAddr);
    bool AddMVPCandOrder(AMVPInfo* pInfo, EnumRefPicList RefPicList, Ipp32s RefIdx, Ipp32s NeibAddr);
    bool GetColMVP(EnumRefPicList refPicListIdx, Ipp32u PartX, Ipp32u PartY, H265MotionVector& rcMV, Ipp32s RefIdx);
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

#endif /* __UMC_H264_SEGMENT_DECODER_H */
#endif // UMC_ENABLE_H264_VIDEO_DECODER
