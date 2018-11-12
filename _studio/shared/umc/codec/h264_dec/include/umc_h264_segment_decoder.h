// Copyright (c) 2003-2018 Intel Corporation
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
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_SEGMENT_DECODER_H
#define __UMC_H264_SEGMENT_DECODER_H

#include "umc_h264_dec.h"
#include "umc_h264_segment_decoder_base.h"
#include "umc_h264_slice_ex.h"
#include "umc_h264_dec_tables.h"

#include "umc_h264_frame_ex.h"

#include "umc_h264_dec_deblocking.h"

using namespace UMC_H264_DECODER;

namespace UMC_H264_DECODER
{
    struct H264SliceHeader;
}

namespace UMC
{

struct DeblockingParameters;
struct DeblockingParametersMBAFF;
class TaskBroker;

//
// Class to incapsulate functions, implementing common decoding functional.
//
struct Context
{
    ReferenceFlags *(m_pFields[2]);
    H264DecoderFrame **(m_pRefPicList[2]);

    bool m_IsUseConstrainedIntra;
    bool m_IsUseDirect8x8Inference;
    bool m_IsBSlice;

    bool m_isMBAFF;
    bool m_isSliceGroups;
    uint32_t m_uPitchLuma;
    uint32_t m_uPitchChroma;

    uint8_t *m_pYPlane;
    uint8_t *m_pUPlane;
    uint8_t *m_pVPlane;
    uint8_t *m_pUVPlane;

    int32_t m_CurMBAddr;                                         // (int32_t) current macroblock address

    bool m_IsUseSpatialDirectMode;
    int32_t m_MVDistortion[2];

    int32_t m_CurMB_X;                                           // (int32_t) horizontal position of MB
    int32_t m_CurMB_Y;                                           // (int32_t) vertical position of MB
    H264DecoderCurrentMacroblockDescriptor m_cur_mb;            // (H264DecoderCurrentMacroblockDescriptor) current MB info

    DeblockingParametersMBAFF  m_deblockingParams;

    H264Bitstream *m_pBitStream;                                // (H264Bitstream *) pointer to bit stream object

    int32_t mb_width;                                            // (int32_t) width in MB
    int32_t mb_height;                                           // (int32_t) height in MB

    int32_t m_MBSkipCount;
    int32_t m_QuantPrev;

    int32_t m_iFirstSliceMb;
    int32_t m_iSliceNumber;

    H264DecoderGlobalMacroblocksDescriptor *m_gmbinfo;
    H264DecoderLocalMacroblockDescriptor m_mbinfo;              // (H264DecoderLocalMacroblockDescriptor) local MB data
    int32_t m_PairMBAddr;                                        // (int32_t) pair macroblock address (MBAFF only)
    CoeffsPtrCommon m_pCoeffBlocksWrite;                        // (int16_t *) pointer to write buffer
    CoeffsPtrCommon m_pCoeffBlocksRead;                         // (int16_t *) pointer to read buffer

    int32_t bit_depth_luma;
    int32_t bit_depth_chroma;
    int32_t m_prev_dquant;
    bool m_bNeedToCheckMBSliceEdges;                            // (bool) flag to ...
    int32_t m_field_index;                                       // (int32_t) ordinal number of current field
    int32_t m_iSkipNextMacroblock;                               // (bool) the next macroblock shall be skipped
    bool m_bError;

    // external data
    const H264PicParamSet *m_pPicParamSet;                      // (const H264PicParamSet *) pointer to current picture parameters sets
    const H264SeqParamSet *m_pSeqParamSet;                      // (const H264SeqParamSet *) pointer to current sequence parameters sets
    const H264ScalingPicParams *m_pScalingPicParams;
    H264DecoderFrameEx *m_pCurrentFrame;                          // (H264DecoderFrame *) pointer to frame being decoded
    IntraType *m_pMBIntraTypes;                                 // (uint32_t *) pointer to array of intra types
    const PredWeightTable *m_pPredWeight[2];                    // (PredWeightTable *) pointer to forward/backward (0/1) prediction table

};

class H264SegmentDecoder : public H264SegmentDecoderBase, public Context
{
public:

    // forward declaration of internal types
    typedef void (H264SegmentDecoder::*DeblockingFunction)(int32_t nMBAddr);
    typedef void (H264SegmentDecoder::*PrepareMBParams)();

    volatile bool m_bFrameDeblocking;                                    // (bool) frame deblocking flag

    H264SliceEx *m_pSlice;                                        // (H264Slice *) current slice pointer
    const H264SliceHeader *m_pSliceHeader;                      // (H264SliceHeader *) current slice header

    uint16_t m_BufferForBackwardPrediction[16 * 16 * 3 + DEFAULT_ALIGN_VALUE]; // allocated buffer for backward prediction
    uint8_t  *m_pPredictionBuffer;                                // pointer to aligned buffer for backward prediction

    // Default constructor
    H264SegmentDecoder(TaskBroker * pTaskBroker);
    // Destructor
    virtual ~H264SegmentDecoder(void);

    // Initialize object
    virtual Status Init(int32_t iNumber);

    // Decode slice's segment
    virtual Status ProcessSegment(void) = 0;

    virtual Status ProcessSlice(int32_t nCurMBNumber, int32_t &nMBToProcess) = 0;

//protected:
    // Release object
    void Release(void);

    // Update info about current MB
    void UpdateCurrentMBInfo();
    void AdjustIndex(int32_t ref_mb_is_bottom, int32_t ref_mb_is_field, int8_t &RefIdx);
    // Update neighbour's addresses
    inline void UpdateNeighbouringAddresses();

    inline void UpdateNeighbouringAddressesField(void);

    inline void UpdateNeighbouringBlocksBMEH(int32_t DeblockCalls = 0);
    inline void UpdateNeighbouringBlocksH2(int32_t DeblockCalls = 0);
    inline void UpdateNeighbouringBlocksH4(int32_t DeblockCalls = 0);

    void DecodeEdgeType();
    void ReconstructEdgeType(uint8_t &edge_type_2t, uint8_t &edge_type_2b, int32_t &special_MBAFF_case);

    // Get context functions
    inline uint32_t GetDCBlocksLumaContext();

    // an universal function for an every case of the live
    inline uint32_t GetBlocksLumaContext(int32_t x,int32_t y);

    // a function for the first luma block in a macroblock
    inline int32_t GetBlocksLumaContextExternal(void);

    // a function for a block on the upper edge of a macroblock,
    // but not for the first block
    inline int32_t GetBlocksLumaContextTop(int32_t x, int32_t left_coeffs);

    // a function for a block on the left edge of a macroblock,
    // but not for the first block
    inline uint32_t GetBlocksLumaContextLeft(int32_t y, int32_t above_coeffs);

    // a function for any internal block of a macroblock
    inline uint32_t GetBlocksLumaContextInternal(int32_t raster_block_num, uint8_t *pNumCoeffsArray);

    // an universal function for an every case of the live
    inline uint32_t GetBlocksChromaContextBMEH(int32_t x,int32_t y,int32_t component);

    // a function for the first block in a macroblock
    inline int32_t GetBlocksChromaContextBMEHExternal(int32_t iComponent);

    // a function for a block on the upper edge of a macroblock,
    // but not for the first block
    inline int32_t GetBlocksChromaContextBMEHTop(int32_t x, int32_t left_coeffs, int32_t iComponent);

    // a function for a block on the left edge of a macroblock,
    // but not for the first block
    inline int32_t GetBlocksChromaContextBMEHLeft(int32_t y, int32_t above_coeffs, int32_t iComponent);

    // a function for any internal block of a macroblock
    inline int32_t GetBlocksChromaContextBMEHInternal(int32_t raster_block_num, uint8_t *pNumCoeffsArray);

    inline uint32_t GetBlocksChromaContextH2(int32_t x,int32_t y,int32_t component);

    inline uint32_t GetBlocksChromaContextH4(int32_t x,int32_t y,int32_t component);

    // Decode macroblock type
    void DecodeMacroBlockType(IntraType *pMBIntraTypes, int32_t *MBSkipCount, int32_t *PassFDFDecode);
    void DecodeMBTypeISlice_CABAC(void);
    void DecodeMBTypeISlice_CAVLC(void);
    void DecodeMBTypePSlice_CABAC(void);
    void DecodeMBTypePSlice_CAVLC(void);
    void DecodeMBTypeBSlice_CABAC(void);
    void DecodeMBTypeBSlice_CAVLC(void);
    void DecodeMBFieldDecodingFlag_CABAC(void);
    void DecodeMBFieldDecodingFlag_CAVLC(void);
    void DecodeMBFieldDecodingFlag(void);

    // Decode intra block types
    void DecodeIntraTypes4x4_CAVLC(IntraType *pMBIntraTypes, bool bUseConstrainedIntra);
    void DecodeIntraTypes8x8_CAVLC(IntraType *pMBIntraTypes, bool bUseConstrainedIntra);
    void DecodeIntraTypes4x4_CABAC(IntraType *pMBIntraTypes, bool bUseConstrainedIntra);
    void DecodeIntraTypes8x8_CABAC(IntraType *pMBIntraTypes, bool bUseConstrainedIntra);
    void DecodeIntraPredChromaMode_CABAC(void);

    void DecodeMBQPDelta_CABAC(void);
    void DecodeMBQPDelta_CAVLC(void);

    // Get colocated location
    int32_t GetColocatedLocation(H264DecoderFrameEx *pRefFrame, int32_t Field, int32_t &block, int32_t *scale = NULL);
    // Decode motion vectors
    void DecodeDirectMotionVectorsTemporal(bool is_direct_mb);

    // Decode motion vectors
    void DecodeDirectMotionVectorsTemporal_8x8Inference();
    // Compute  direct spatial reference indexes
    void ComputeDirectSpatialRefIdx(int32_t *pRefIndexL0, int32_t *pRefIndexL1);
    void GetRefIdx4x4_CABAC(const uint32_t nActive,
                              const uint8_t* pBlkIdx,
                              const uint8_t*  pCodRIx,
                              uint32_t ListNum);
    void GetRefIdx4x4_CABAC(const uint32_t nActive,
                              const uint8_t  pCodRIx,
                              uint32_t ListNum);
    void GetRefIdx4x4_16x8_CABAC(const uint32_t nActive,
                                const uint8_t*  pCodRIx,
                                uint32_t ListNum);
    void GetRefIdx4x4_8x16_CABAC(const uint32_t nActive,
                                const uint8_t*  pCodRIx,
                                uint32_t ListNum);

    int32_t GetSE_RefIdx_CABAC(uint32_t ListNum, uint32_t BlockNum);

    H264DecoderMotionVector GetSE_MVD_CABAC(uint32_t ListNum, uint32_t BlockNum);

    // Get direct motion vectors
    void GetDirectTemporalMV(int32_t MBCol,
                             uint32_t ipos,
                             H264DecoderMotionVector *& MVL0, // return colocated MV here
                             int8_t &RefIndexL0); // return ref index here
    void GetDirectTemporalMVFLD(int32_t MBCol,
                                uint32_t ipos,
                                H264DecoderMotionVector *& MVL0, // return colocated MV here
                                int8_t &RefIndexL0); // return ref index here
    void GetDirectTemporalMVMBAFF(int32_t MBCol,
                                  uint32_t ipos,
                                  H264DecoderMotionVector *& MVL0, // return colocated MV here
                                  int8_t &RefIndexL0); // return ref index here
    // Decode and return the coded block pattern.
    // Return 255 is there is an error in the CBP.
    uint32_t DecodeCBP_CAVLC(uint32_t color_format);
    uint32_t DecodeCBP_CABAC(uint32_t color_format);

    // Decode motion vector predictors
    void ComputeMotionVectorPredictors(uint8_t ListNum,
                                       int8_t RefIndex, // reference index for this part
                                       int32_t block, // block or subblock number, depending on mbtype
                                       H264DecoderMotionVector * mv);

    void ComputeMotionVectorPredictorsMBAFF(const uint8_t ListNum,
                                                       int8_t RefIndex, // reference index for this part
                                                       const int32_t block, // block or subblock number, depending on mbtype
                                                       H264DecoderMotionVector * mv);

    // Decode slipped MB
    uint32_t DecodeMBSkipFlag_CABAC(int32_t ctxIdx);
    uint32_t DecodeMBSkipRun_CAVLC(void);

    // Get location functions
    void GetLeftLocationForCurrentMBLumaMBAFF(H264DecoderBlockLocation *Block,int32_t AdditionalDecrement=0);
    void GetTopLocationForCurrentMBLumaMBAFF(H264DecoderBlockLocation *Block,int32_t DeblockCalls);
    void GetTopLeftLocationForCurrentMBLumaMBAFF(H264DecoderBlockLocation *Block);
    void GetTopRightLocationForCurrentMBLumaMBAFF(H264DecoderBlockLocation *Block);
    void GetLeftLocationForCurrentMBLumaNonMBAFF(H264DecoderBlockLocation *Block);
    void GetTopLocationForCurrentMBLumaNonMBAFF(H264DecoderBlockLocation *Block);
    void GetTopLeftLocationForCurrentMBLumaNonMBAFF(H264DecoderBlockLocation *Block);
    void GetTopRightLocationForCurrentMBLumaNonMBAFF(H264DecoderBlockLocation *Block);

    void GetTopLocationForCurrentMBChromaMBAFFBMEH(H264DecoderBlockLocation *Block);
    void GetLeftLocationForCurrentMBChromaMBAFFBMEH(H264DecoderBlockLocation *Block);
    void GetTopLocationForCurrentMBChromaNonMBAFFBMEH(H264DecoderBlockLocation *Block);
    void GetLeftLocationForCurrentMBChromaNonMBAFFBMEH(H264DecoderBlockLocation *Block);

    void GetTopLocationForCurrentMBChromaMBAFFH2(H264DecoderBlockLocation *Block);
    void GetLeftLocationForCurrentMBChromaMBAFFH2(H264DecoderBlockLocation *Block);
    void GetTopLocationForCurrentMBChromaNonMBAFFH2(H264DecoderBlockLocation *Block);
    void GetLeftLocationForCurrentMBChromaNonMBAFFH2(H264DecoderBlockLocation *Block);

    void GetTopLocationForCurrentMBChromaMBAFFH4(H264DecoderBlockLocation *Block);
    void GetLeftLocationForCurrentMBChromaMBAFFH4(H264DecoderBlockLocation *Block);
    void GetTopLocationForCurrentMBChromaNonMBAFFH4(H264DecoderBlockLocation *Block);
    void GetLeftLocationForCurrentMBChromaNonMBAFFH4(H264DecoderBlockLocation *Block);

    CoeffsPtrCommon m_pCoefficientsBuffer;

    // Perform deblocking on whole frame.
    // It is possible only for Baseline profile.
    void DeblockFrame(int32_t nCurMBNumber, int32_t nMacroBlocksToDeblock);
    // Function to de-block partition of macro block row
    virtual void DeblockSegment(int32_t iCurMBNumber, int32_t iMBToProcess);

    //
    // Optimized deblocking functions
    //

    // Reset deblocking variables
    void InitDeblockingOnce();
    void InitDeblockingSliceBoundaryInfo();
    void ResetDeblockingVariables();
    void ResetDeblockingVariablesMBAFF();
    // Function to do luma deblocking
    void DeblockLuma(uint32_t dir);
    void DeblockLumaVerticalMBAFF();
    void DeblockLumaHorizontalMBAFF();
    // Function to do chroma deblocking
    void DeblockChroma(uint32_t dir);
    void DeblockChromaVerticalMBAFF();
    void DeblockChromaHorizontalMBAFF();
    // Function to prepare deblocking parameters for mixed MB types
    void DeblockMacroblockMSlice();

    void AdujstMvsAndType();

    inline void EvaluateStrengthExternal(int32_t cbp4x4_luma, int32_t nNeighbour, int32_t dir,
        int32_t idx,
        H264DecoderFrame **pNRefPicList0,
        ReferenceFlags *pNFields0,
        H264DecoderFrame **pNRefPicList1,
        ReferenceFlags *pNFields1);

    inline void foo_internal(int32_t dir,
        int32_t idx, uint32_t cbp4x4_luma);

    inline uint8_t EvaluateStrengthInternal(int32_t dir, int32_t idx);

    inline uint8_t EvaluateStrengthInternal8x8(int32_t dir, int32_t idx);
    void PrepareStrengthsInternal();

    inline void SetStrength(int32_t dir, int32_t block_num, uint32_t cbp4x4_luma, uint8_t strength);
    void GetReferencesB8x8();

    inline void foo_external_p(int32_t cbp4x4_luma, int32_t nNeighbour, int32_t dir,
        int32_t idx,
        H264DecoderFrame **pNRefPicList0,
        ReferenceFlags *pNFields0);

    inline void foo_internal_p(int32_t dir,
        int32_t idx, uint32_t cbp4x4_luma);

    //
    // Function to do deblocking on I slices
    //

    void DeblockMacroblock(PrepareMBParams prepareMBParams);
    void DeblockMacroblockMBAFF(PrepareMBParams prepareMBParams);

    void PrepareDeblockingParametersISlice();
    void PrepareDeblockingParametersISliceMBAFF();

    // obtain reference number for block
    size_t GetReferencePSlice(int32_t iMBNum, int32_t iBlockNum);
    inline void GetReferencesBSlice(int32_t iMBNum, int32_t iBlockNum, size_t *pReferences);
    inline void GetReferencesBCurMB(int32_t iBlockNum, size_t *pReferences);
    void PrepareDeblockingParametersIntern16x16Vert();
    void PrepareDeblockingParametersIntern16x16Horz();

    //
    // Function to do deblocking on P slices
    //

    void PrepareDeblockingParametersPSlice();
    void PrepareDeblockingParametersPSliceMBAFF();
    // Prepare deblocking parameters for macroblocks from P slice
    // MbPart is 16, MbPart of opposite direction is 16
    void PrepareDeblockingParametersPSlice16x16Vert();
    void PrepareDeblockingParametersPSlice16x16Horz();
    // Prepare deblocking parameters for macroblocks from P slice
    // MbPart is 8, MbPart of opposite direction is 16
    void PrepareDeblockingParametersPSlice8x16(uint32_t dir);
    // Prepare deblocking parameters for macroblocks from P slice
    // MbPart is 16, MbPart of opposite direction is 8
    void PrepareDeblockingParametersPSlice16x8(uint32_t dir);
    // Prepare deblocking parameters for macroblocks from P slice
    // MbParts of both directions are 4
    void PrepareDeblockingParametersPSlice4(uint32_t dir);
    void PrepareDeblockingParametersPSlice4MBAFFField(uint32_t dir);
    // Prepare deblocking parameters for macroblock from P slice,
    // which coded in frame mode, but above macroblock is coded in field mode
    void PrepareDeblockingParametersPSlice4MBAFFMixedExternalEdge();
    // Prepare deblocking parameters for macroblock from P slice,
    // which coded in frame mode, but left macroblock is coded in field mode
    void PrepareDeblockingParametersPSlice4MBAFFComplexFrameExternalEdge();
    // Prepare deblocking parameters for macroblock from P slice,
    // which coded in field mode, but left macroblock is coded in frame mode
    void PrepareDeblockingParametersPSlice4MBAFFComplexFieldExternalEdge();

    //
    // Function to do deblocking on B slices
    //

    void PrepareDeblockingParametersBSlice();
    void PrepareDeblockingParametersBSliceMBAFF();
    // Prepare deblocking parameters for macroblocks from B slice
    // MbPart is 16, MbPart of opposite direction is 16
    void PrepareDeblockingParametersBSlice16x16Vert();
    void PrepareDeblockingParametersBSlice16x16Horz();
    // Prepare deblocking parameters for macroblocks from B slice
    // MbPart is 8, MbPart of opposite direction is 16
    void PrepareDeblockingParametersBSlice8x16(uint32_t dir);
    // Prepare deblocking parameters for macroblocks from B slice
    // MbPart is 16, MbPart of opposite direction is 8
    void PrepareDeblockingParametersBSlice16x8(uint32_t dir);
    // Prepare deblocking parameters for macroblocks from B slice
    // MbParts of both directions are 4
    void PrepareDeblockingParametersBSlice4(uint32_t dir);
    void PrepareDeblockingParametersBSlice4MBAFFField(uint32_t dir);

    CoeffsPtrCommon GetCoefficientsBuffer(uint32_t nNum = 0);

private:
    H264SegmentDecoder( const H264SegmentDecoder &s );  // No copy CTR
    // we lock the assignment operator to avoid any
    // accasional assignments
    H264SegmentDecoder & operator = (H264SegmentDecoder &)
    {
        return *this;

    } // H264SegmentDecoder & operator = (H264SegmentDecoder &)
};

extern
void InitializeSDCreator_ManyBits();
extern
void InitializeSDCreator_ManyBits_MFX();
extern
bool CutPlanes(H264DecoderFrame * , H264SeqParamSet * );


// codes used to indicate the coding state of each block
const uint8_t CodNone = 0;    // no code
const uint8_t CodInBS = 1;    // read code from bitstream
const uint8_t CodLeft = 2;    // code same as left 4x4 subblock
const uint8_t CodAbov = 3;    // code same as above 4x4 subblock
const uint8_t CodSkip = 4;    // skip for direct mode
const uint8_t CodPred = 5;    // prediction

// declaration of const table(s)
extern const
uint8_t CodeToMBTypeB[];
extern const
uint8_t CodeToBDir[][2];
extern const
int32_t NIT2LIN[16];
extern const
int32_t SBTYPESIZE[5][4];
extern const
uint8_t pCodFBD[5][4];
extern const
uint8_t pCodTemplate[16];
extern const
uint32_t sb_x[4][16];
extern const
uint32_t sb_y[4][16];
extern const
int32_t sign_mask[2];

} // namespace UMC

#include "umc_h264_sd_inlines.h"

#endif /* __UMC_H264_SEGMENT_DECODER_H */
#endif // UMC_ENABLE_H264_VIDEO_DECODER
