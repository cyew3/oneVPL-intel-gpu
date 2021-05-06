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
#ifndef MFX_VA

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

    virtual int32_t GetChromaFormat() const = 0;

    // Do luma intra prediction
    virtual void PredictIntra(int32_t predMode, PlanePtrY PredPel, PlanePtrY pRec, int32_t pitch, int32_t width, uint32_t bit_depth) = 0;

    // Create a buffer of neighbour luma samples for intra prediction
    virtual void GetPredPelsLuma(PlanePtrY pSrc, PlanePtrY PredPel, int32_t blkSize, int32_t srcPitch, uint32_t tpIf, uint32_t lfIf, uint32_t tlIf, uint32_t bit_depth) = 0;

    // Do chroma intra prediction
    virtual void PredictIntraChroma(int32_t predMode, PlanePtrY PredPel, PlanePtrY pels, int32_t pitch, int32_t width) = 0;

    // Create a buffer of neighbour NV12 chroma samples for intra prediction
    virtual void GetPredPelsChromaNV12(PlanePtrY pSrc, PlanePtrY PredPel, int32_t blkSize, int32_t srcPitch, uint32_t tpIf, uint32_t lfIf, uint32_t tlIf, uint32_t bit_depth) = 0;

    // Strong intra smoothing luma filter
    virtual void FilterPredictPels(DecodingContext* sd, H265CodingUnit* pCU, PlanePtrY PredPel, int32_t width, uint32_t TrDepth, uint32_t AbsPartIdx) = 0;

    // Luma deblocking edge filter
    virtual void FilterEdgeLuma(H265EdgeData *edge, PlanePtrY srcDst, size_t srcDstStride, int32_t x, int32_t y, int32_t dir, uint32_t bit_depth) = 0;

    // Chroma deblocking edge filter
    virtual void FilterEdgeChroma(H265EdgeData *edge, PlanePtrY srcDst, size_t srcDstStride, int32_t x, int32_t y, int32_t dir, int32_t chromaCbQpOffset, int32_t chromaCrQpOffset, uint32_t bit_depth) = 0;

    virtual void CopyPartOfFrameFromRef(PlanePtrY pRefPlane, PlanePtrY pCurrentPlane, int32_t pitch,
        int32_t offsetX, int32_t offsetY, int32_t offsetXL, int32_t offsetYL,
        mfxSize cuSize, mfxSize frameSize) = 0;

    virtual void DecodePCMBlock(H265Bitstream *bitStream, CoeffsPtr *pcm, uint32_t size, uint32_t sampleBits) = 0;

    virtual void ReconstructPCMBlock(PlanePtrY luma, int32_t pitchLuma, uint32_t PcmLeftShiftBitLuma, PlanePtrY chroma, int32_t pitchChroma, uint32_t PcmLeftShiftBitChroma, CoeffsPtr *pcm, uint32_t size) = 0;

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
    uint32_t              m_ChromaArrayType;

    H265CodingUnit*     m_cu;

    struct
    {
        bool predictionExist;
        uint32_t saveColumn;
        uint32_t saveRow;
        uint32_t saveHeight;
    } m_deblockPredData[2];

    int32_t isMin4x4Block;

    uint8_t * m_pY_CU_Plane;
    uint8_t * m_pUV_CU_Plane;
    size_t  m_pitch;

    // intra chroma prediction. For temporal save
    uint32_t save_lfIf;
    uint32_t save_tlIf;
    uint32_t save_tpIf;

    std::unique_ptr<ReconstructorBase>  m_reconstructor;
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
    int32_t m_CurrCTBStride;

    uint8_t    m_weighted_prediction;

    struct
    {
        int32_t m_QPRem, m_QPPer;
        int32_t m_QPScale;
    } m_ScaledQP[3];

    // Local context for reconstructing Intra
    uint32_t m_RecIntraFlagsHolder[128]; // Placeholder for Intra availability flags needed during reconstruction
    uint32_t *m_RecTpIntraFlags;         // 17 x32 flags for top intra blocks for current CTB
    uint32_t *m_RecLfIntraFlags;         // 17 x32 flags for left intra blocks for current CTB
    uint32_t *m_RecTLIntraFlags;         // 1 x32 flags for top-left intra blocks for current CTB
    uint16_t *m_RecTpIntraRowFlags;      // 128 + 2 x16 flags for top line of intra blocks for the frame of 8192 pixels wide

    // mt params
    bool m_needToSplitDecAndRec;
    int32_t m_mvsDistortionTemp; // max y component of all mvs in slice
    int32_t m_mvsDistortion; // max y component of all mvs in slice

    // Allocate context buffers
    void Init(H265Task *task);
    // Fill up decoder context for new CTB using previous CTB values and values stored for top row
    void UpdateCurrCUContext(uint32_t lastCUAddr, uint32_t newCUAddr);
    // Clean up all availability information for decoder
    void ResetRowBuffer();

    // Update reconstruct information with neighbour information for intra prediction
    void UpdateRecCurrCTBContext(int32_t lastCUAddr, int32_t newCUAddr);
    // Clean up all availability information for reconstruct
    void ResetRecRowBuffer();

    // Set new QP value and calculate scaled values for luma and chroma
    void SetNewQP(int32_t newQP, int32_t chroma_offset_idx = -1);
    int32_t GetQP(void)
    {
        return m_LastValidQP;
    }

    H265_FORCEINLINE int16_t *getDequantCoeff(uint32_t list, uint32_t qp, uint32_t size)
    {
        return (*m_dequantCoef)[size][list][qp];
    }
    int16_t *(*m_dequantCoef)[SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][SCALING_LIST_REM_NUM];

protected:

    int32_t          m_LastValidQP;
    int32_t          m_LastValidOffsetIndex;
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
                                    int32_t allowMergeLeft,
                                    int32_t allowMergeUp);

    // Decode SAO truncated rice offset value. HEVC spec 9.3.3.2
    int32_t parseSaoMaxUvlc(int32_t maxSymbol);
    // Decode SAO type idx
    int32_t parseSaoTypeIdx();
    // Decode SAO plane type and offsets
    void parseSaoOffset(SAOLCUParam* psSaoLcuParam, uint32_t compIdx);

    // Parse truncated rice symbol. HEVC spec 9.3.3.2
    void ReadUnaryMaxSymbolCABAC(uint32_t& uVal, uint32_t CtxIdx, int32_t Offset, uint32_t MaxSymbol);
    // Decode CU split flag
    bool DecodeSplitFlagCABAC(int32_t PartX, int32_t PartY, uint32_t Depth);
    // Decode inter PU merge index
    uint32_t DecodeMergeIndexCABAC(void);
    // Decode CU skip flag
    bool DecodeSkipFlagCABAC(int32_t PartX, int32_t PartY);
    bool DecodeCUTransquantBypassFlag(uint32_t AbsPartIdx);
    // Decode inter PU MV predictor index
    void DecodeMVPIdxPUCABAC(uint32_t AbsPartAddr, uint32_t PartIdx, EnumRefPicList RefList, H265MVInfo &MVi, uint8_t InterDir);
    // Decode CU prediction mode
    int32_t DecodePredModeCABAC(uint32_t AbsPartIdx);
    // Decode partition size. HEVC spec 9.3.3.5
    void DecodePartSizeCABAC(uint32_t AbsPartIdx, uint32_t Depth);
    // Decode IPCM CU flag and samples
    void DecodeIPCMInfoCABAC(uint32_t AbsPartIdx, uint32_t Depth);
    // Decode PCM alignment zero bits.
    void DecodePCMAlignBits();
    // Decode luma intra direction
    void DecodeIntraDirLumaAngCABAC(uint32_t AbsPartIdx, uint32_t Depth);
    // Decode intra chroma direction. HEVC spec 9.3.3.6
    void DecodeIntraDirChromaCABAC(uint32_t AbsPartIdx, uint32_t Depth);
    // Decode inter PU information
    bool DecodePUWiseCABAC(uint32_t AbsPartIdx, uint32_t Depth);
    // Decode merge flag
    bool DecodeMergeFlagCABAC(void);
    uint8_t DecodeInterDirPUCABAC(uint32_t AbsPartIdx);
    // Decode truncated rice reference frame index for AMVP PU
    RefIndexType DecodeRefFrmIdxPUCABAC(EnumRefPicList RefList, uint8_t InterDir);
    // Decode MV delta. HEVC spec 7.3.8.9
    void DecodeMVdPUCABAC(EnumRefPicList RefList, H265MotionVector &MVd, uint8_t InterDir);
    // Decode explicit TU split
    void ParseTransformSubdivFlagCABAC(uint32_t& SubdivFlag, uint32_t Log2TransformBlockSize);
    // Decode EG1 coded abs_mvd_minus2 values
    void ReadEpExGolombCABAC(uint32_t& Value, uint32_t Count);
    // Decode all CU coefficients
    void DecodeCoeff(uint32_t AbsPartIdx, uint32_t Depth, bool& CodeDQP, bool isFirstPartMerge);
    // Recursively decode TU data
    void DecodeTransform(uint32_t AbsPartIdx, uint32_t Depth, uint32_t  l2width, uint32_t TrIdx, bool& CodeDQP);
    // Decode quad tree CBF value
    void ParseQtCbfCABAC(uint32_t AbsPartIdx, ComponentPlane plane, uint32_t TrDepth, uint32_t Depth);
    // Decode root CU CBF value
    void ParseQtRootCbfCABAC(uint32_t& QtRootCbf);
    // Decode and set new QP value
    void DecodeQP(uint32_t AbsPartIdx);
    
    void DecodeQPChromaAdujst(); //ChromaQpOffsetCoded
    // Calculate CU QP value based on previously used QP values. HEVC spec 8.6.1
    int32_t getRefQP(int32_t AbsPartIdx);
    // Decode and set new QP value. HEVC spec 9.3.3.8
    void ParseDeltaQPCABAC(uint32_t AbsPartIdx);
    void FinishDecodeCU(uint32_t AbsPartIdx, uint32_t Depth, uint32_t& IsLast);
    // Copy CU data from position 0 to all other subparts
    // This information is needed for reconstruct which may be done in another thread
    void BeforeCoeffs(uint32_t AbsPartIdx, uint32_t Depth);
    // Decode CU recursively
    void DecodeCUCABAC(uint32_t AbsPartIdx, uint32_t Depth, uint32_t& IsLast);
    // Decode slice end flag if necessary
    bool DecodeSliceEnd(uint32_t AbsPartIdx, uint32_t Depth);

    // Decode X and Y coordinates of last significant coefficient in a TU block
    uint32_t ParseLastSignificantXYCABAC(uint32_t &PosLastX, uint32_t &PosLastY, uint32_t L2Width, bool IsLuma, uint32_t ScanIdx);

    // Parse TU coefficients
    template <bool scaling_list_enabled_flag>
    void ParseCoeffNxNCABACOptimized(CoeffsPtr pCoef, uint32_t AbsPartIdx, uint32_t Log2BlockSize, ComponentPlane plane);

    // Parse TU coefficients
    void ParseCoeffNxNCABAC(CoeffsPtr pCoef, uint32_t AbsPartIdx, uint32_t Log2BlockSize, ComponentPlane plane);

    // Parse TU transform skip flag
    void ParseTransformSkipFlags(uint32_t AbsPartIdx, ComponentPlane plane);

    void ReadCoefRemainExGolombCABAC(uint32_t &Symbol, uint32_t &GoRiceParam);

    // Recursively produce CU reconstruct from decoded values
    void ReconstructCU(uint32_t AbsPartIdx, uint32_t Depth);
    // Reconstruct intra quad tree including handling IPCM
    void ReconIntraQT(uint32_t AbsPartIdx, uint32_t Depth);
    // Perform inter CU reconstruction
    void ReconInter(uint32_t AbsPartIdx, uint32_t Depth);
    // Place IPCM decoded samples to reconstruct frame
    void ReconPCM(uint32_t AbsPartIdx, uint32_t Depth);

    // Reconstruct intra (no IPCM) quad tree recursively
    void IntraRecQT(uint32_t TrDepth, uint32_t AbsPartIdx, uint32_t ChromaPredMode);

    // Reconstruct intra luma block
    void IntraRecLumaBlk(uint32_t TrDepth,
                         uint32_t AbsPartIdx,
                         uint32_t tpIf, uint32_t lfIf, uint32_t tlIf);
    // Reconstruct intra NV12 chroma block
    void IntraRecChromaBlk(uint32_t TrDepth,
                           uint32_t AbsPartIdx,
                           uint32_t ChromaPredMode,
                           uint32_t tpIf, uint32_t lfIf, uint32_t tlIf);

    void FillReferenceSamplesChroma(
        int32_t bitDepth,
        PlanePtrUV pRoiOrigin,
        PlanePtrUV pAdiTemp,
        bool* NeighborFlags,
        int32_t NumIntraNeighbor,
        int32_t UnitSize,
        int32_t NumUnitsInCU,
        int32_t TotalUnits,
        uint32_t CUSize,
        uint32_t Size,
        int32_t PicStride);

    // Deblock edges inside of one CTB, left and top of it
    void DeblockOneLCU(int32_t curLCUAddr);
    // Deblock horizontal edge
    void DeblockOneCross(int32_t curPixelColumn, int32_t curPixelRow, bool isNeddAddHorDeblock);

    // Calculate edge strength
    template <int32_t direction, typename EdgeType>
    void CalculateEdge(EdgeType * edge, int32_t x, int32_t y, bool diffTr);

    // Recursively deblock edges inside of CU
    void DeblockCURecur(uint32_t absPartIdx, uint32_t depth);
    // Recursively deblock edges inside of TU
    void DeblockTU(uint32_t absPartIdx, uint32_t depth);

    // Edge strength calculation for two inter blocks
    inline void GetEdgeStrengthInter(H265MVInfo *mvinfoQ, H265MVInfo *mvinfoP, H265PartialEdgeData *edge);

    bool m_DecodeDQPFlag;
    bool m_IsCuChromaQpOffsetCoded;
    uint32_t m_minCUDQPSize;
    uint32_t m_MaxDepth; //max number of depth
    H265DecYUVBufferPadded* m_ppcYUVResi; //array of residual buffer

    std::unique_ptr<H265Prediction> m_Prediction;

    H265TrQuant* m_TrQuant;
    uint32_t m_BakAbsPartIdxChroma;
    uint32_t m_bakAbsPartIdxQp;

    DecodingContext * m_context;
    std::unique_ptr<DecodingContext> m_context_single_thread;

    // Fill up basic CU information in local decoder context
    void UpdateNeighborBuffers(uint32_t AbsPartIdx, uint32_t Depth, bool isSkipped);
    // Change QP recorded in CU block of local context to a new value
    void UpdateNeighborDecodedQP(uint32_t AbsPartIdx, uint32_t Depth);
    // Fill up inter information in local decoding context and colocated lookup table
    void UpdatePUInfo(uint32_t PartX, uint32_t PartY, uint32_t PartWidth, uint32_t PartHeight, H265MVInfo &mvInfo);
    // Update intra availability flags for reconstruct
    void UpdateRecNeighboursBuffersN(int32_t PartX, int32_t PartY, int32_t PartSize, bool IsIntra);

    H265Slice *m_pSlice;                                        // (H265Slice *) current slice pointer
    H265SliceHeader *m_pSliceHeader;                      // (H265SliceHeader *) current slice header

    // Default constructor
    H265SegmentDecoder(TaskBroker_H265 * pTaskBroker);
    // Destructor
    virtual ~H265SegmentDecoder(void);

    // Initialize decoder's number
    virtual UMC::Status Init(int32_t iNumber);

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
    uint32_t getCtxSplitFlag(int32_t PartX, int32_t PartY, uint32_t Depth);
    // Calculate CABAC context for skip flag. HEVC spec 9.3.4.2.2
    uint32_t getCtxSkipFlag(int32_t PartX, int32_t PartY);
    // Get predictor array of intra directions for intra luma direction decoding. HEVC spec 8.4.2
    void getIntraDirLumaPredictor(uint32_t AbsPartIdx, int32_t IntraDirPred[]);
    // Prepare an array of motion vector candidates for merge mode. HEVC spec 8.5.3.2.2
    void getInterMergeCandidates(uint32_t AbsPartIdx, uint32_t PUIdx, H265MVInfo *MVBufferNeighbours,
        uint8_t* InterDirNeighbours, int32_t mrgCandIdx);
    // Prepare an array of motion vector candidates for AMVP mode. HEVC spec 8.5.3.2.6
    void fillMVPCand(uint32_t AbsPartIdx, uint32_t PartIdx, EnumRefPicList RefPicList, int32_t RefIdx, AMVPInfo* pInfo);
    // Check availability of spatial MV candidate which points to the same reference frame. HEVC spec 8.5.3.2.7 #6
    bool AddMVPCand(AMVPInfo* pInfo, EnumRefPicList RefPicList, int32_t RefIdx, int32_t NeibAddr, const H265MVInfo &motionInfo);
    bool AddMVPCandOrder(AMVPInfo* pInfo, EnumRefPicList RefPicList, int32_t RefIdx, int32_t NeibAddr, const H265MVInfo &motionInfo);
    // Check availability of collocated motion vector with given coordinates. HEVC spec 8.5.3.2.9
    bool GetColMVP(EnumRefPicList refPicListIdx, uint32_t PartX, uint32_t PartY, H265MotionVector& rcMV, int32_t RefIdx);
    // Returns whether motion information in two cells of collocated motion table is the same
    bool hasEqualMotion(int32_t dir1, int32_t dir2);

    // constrained intra prediction in reconstruct
    int32_t isRecIntraAboveAvailable(int32_t TUPartNumberInCTB, int32_t NumUnitsInCU, bool *ValidFlags);
    int32_t isRecIntraLeftAvailable(int32_t TUPartNumberInCTB, int32_t NumUnitsInCU, bool *ValidFlags);
    int32_t isRecIntraAboveRightAvailable(int32_t TUPartNumberInCTB, int32_t PartX, int32_t XInc, int32_t NumUnitsInCU, bool *ValidFlags);
    int32_t isRecIntraAboveRightAvailableOtherCTB(int32_t PartX, int32_t NumUnitsInCU, bool *ValidFlags);
    int32_t isRecIntraBelowLeftAvailable(int32_t TUPartNumberInCTB, int32_t PartY, int32_t YInc, int32_t NumUnitsInCU, bool *ValidFlags);

    // we lock the assignment operator to avoid any
    // accasional assignments
    H265SegmentDecoder & operator = (H265SegmentDecoder &)
    {
        return *this;

    } // H265SegmentDecoder & operator = (H265SegmentDecoder &)
};

} // namespace UMC_HEVC_DECODER

#endif /* __UMC_H265_SEGMENT_DECODER_H */
#endif // #ifndef MFX_VA
#endif // MFX_ENABLE_H265_VIDEO_DECODE
