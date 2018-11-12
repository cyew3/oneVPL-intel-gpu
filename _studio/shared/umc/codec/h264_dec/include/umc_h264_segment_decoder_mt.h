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

#ifndef __UMC_H264_SEGMENT_DECODER_MT_H
#define __UMC_H264_SEGMENT_DECODER_MT_H

#include "umc_h264_segment_decoder.h"

namespace UMC
{
class H264Task;
class SegmentDecoderHPBase;

class H264SegmentDecoderMultiThreaded : public H264SegmentDecoder
{
public:
    // Default constructor
    H264SegmentDecoderMultiThreaded(TaskBroker * pTaskBroker);
    // Destructor
    virtual
    ~H264SegmentDecoderMultiThreaded(void);

    // Initialize object
    virtual
    Status Init(int32_t iNumber);

    // Decode slice's segment
    virtual
    Status ProcessSegment(void);

    // asynchronous called functions
    Status DecRecSegment(int32_t iCurMBNumber, int32_t &iMaxMBToDecRec);
    Status DecodeSegment(int32_t iCurMBNumber, int32_t &iMBToDecode);
    Status ReconstructSegment(int32_t iCurMBNumber, int32_t &iMBToReconstruct);
    virtual Status DeblockSegmentTask(int32_t iCurMBNumber, int32_t &iMBToDeblock);

    CoeffsPtrCommon GetCoefficientsBuffer(void)
    {
        return m_psBuffer;
    }

    virtual Status ProcessSlice(int32_t iCurMBNumber, int32_t &iMBToProcess);

    virtual Status ProcessTask(H264Task *task);

    virtual void RestoreErrorRect(int32_t startMb, int32_t endMb, H264Slice * pSlice);

    // Allocated more coefficients buffers
    void ReallocateCoefficientsBuffer(void);

    Status DecodeMacroBlockCAVLC(uint32_t nCurMBNumber, uint32_t &nMaxMBNumber);
    Status DecodeMacroBlockCABAC(uint32_t nCurMBNumber, uint32_t &nMaxMBNumber);

    Status ReconstructMacroBlockCAVLC(uint32_t nCurMBNumber, uint32_t nMaxMBNumber);
    Status ReconstructMacroBlockCABAC(uint32_t nCurMBNumber, uint32_t nMaxMBNumber);

    // Get direct motion vectors for block 4x4
    void GetMVD4x4_CABAC(const uint8_t *pBlkIdx,
                           const uint8_t *pCodMVd,
                           uint32_t ListNum);
    void GetMVD4x4_16x8_CABAC(const uint8_t *pCodMVd,
                                uint32_t ListNum);
    void GetMVD4x4_8x16_CABAC(const uint8_t *pCodMVd,
                                uint32_t ListNum);
    void GetMVD4x4_CABAC(const uint8_t pCodMVd,
                           uint32_t ListNum);

    // Reconstruct skipped motion vectors
    void ReconstructSkipMotionVectors();

    // Decode motion vectors
    void DecodeMotionVectorsPSlice_CAVLC(void);
    void DecodeMotionVectors_CAVLC(bool bIsBSlice);
    void DecodeMotionVectors_CABAC();

    // Reconstruct motion vectors
    void ReconstructMotionVectors(void);

    void ReconstructMVsExternal(int32_t iListNum);
    void ReconstructMVsTop(int32_t iListNum);
    void ReconstructMVsLeft(int32_t iListNum);
    void ReconstructMVsInternal(int32_t iListNum);

    void ReconstructMVs16x16(int32_t iListNum);
    void ReconstructMVs16x8(int32_t iListNum, int32_t iSubBlockNum);
    void ReconstructMVs8x16(int32_t iListNum, int32_t iSubBlockNum);

    void ReconstructMVs8x8External(int32_t iListNum);
    void ReconstructMVs8x8Top(int32_t iListNum);
    void ReconstructMVs8x8Left(int32_t iListNum);
    void ReconstructMVs8x8Internal(int32_t iListNum);

    void ReconstructMVs8x4External(int32_t iListNum);
    void ReconstructMVs8x4Top(int32_t iListNum);
    void ReconstructMVs8x4Left(int32_t iListNum, int32_t iRowNum);
    void ReconstructMVs8x4Internal(int32_t iListNum, int32_t iSubBlockNum);

    void ReconstructMVs4x8External(int32_t iListNum);
    void ReconstructMVs4x8Top(int32_t iListNum, int32_t iColumnNum);
    void ReconstructMVs4x8Left(int32_t iListNum);
    void ReconstructMVs4x8Internal(int32_t iListNum, int32_t iSubBlockNum, int32_t iAddrC);

    void ReconstructMVs4x4External(int32_t iListNum);
    void ReconstructMVs4x4Top(int32_t iListNum, int32_t iColumnNum);
    void ReconstructMVs4x4Left(int32_t iListNum, int32_t iRowNum);
    void ReconstructMVs4x4Internal(int32_t iListNum, int32_t iBlockNum, int32_t iAddrC);
    void ReconstructMVs4x4InternalFewCheckRef(int32_t iListNum, int32_t iBlockNum, int32_t iAddrC);
    void ReconstructMVs4x4InternalNoCheckRef(int32_t iListNum, int32_t iBlockNum);

    void ResetMVs16x16(int32_t iListNum);
    void ResetMVs16x8(int32_t iListNum, int32_t iVectorOffset);
    void ResetMVs8x16(int32_t iListNum, int32_t iVectorOffset);
    void ResetMVs8x8(int32_t iListNum, int32_t iVectorOffset);
    void CopyMVs8x16(int32_t iListNum);
    void ReconstructMVPredictorExternalBlock(int32_t iListNum,
                                             const H264DecoderBlockLocation &mbAddrC,
                                             H264DecoderMotionVector *pPred);
    void ReconstructMVPredictorExternalBlockMBAFF(int32_t iListNum,
                                                  H264DecoderBlockLocation mbAddrC,
                                                  H264DecoderMotionVector *pPred);
    void ReconstructMVPredictorTopBlock(int32_t iListNum,
                                        int32_t iColumnNum,
                                        H264DecoderBlockLocation mbAddrC,
                                        H264DecoderMotionVector *pPred);
    void ReconstructMVPredictorLeftBlock(int32_t iListNum,
                                         int32_t iRowNum,
                                         H264DecoderBlockLocation mbAddrC,
                                         H264DecoderMotionVector *pPred);
    void ReconstructMVPredictorLeftBlockFewCheckRef(int32_t iListNum,
                                                    int32_t iRowNum,
                                                    H264DecoderMotionVector *pPred);
    void ReconstructMVPredictorInternalBlock(int32_t iListNum,
                                             int32_t iBlockNum,
                                             int32_t iAddrC,
                                             H264DecoderMotionVector *pPred);
    void ReconstructMVPredictorInternalBlockFewCheckRef(int32_t iListNum,
                                                        int32_t iBlockNum,
                                                        int32_t iAddrC,
                                                        H264DecoderMotionVector *pPred);
    void ReconstructMVPredictorInternalBlockNoCheckRef(int32_t iListNum,
                                                       int32_t iBlockNum,
                                                       H264DecoderMotionVector *pPred);
    void ReconstructMVPredictor16x16(int32_t iListNum,
                                     H264DecoderMotionVector *pPred);
    void ReconstructMVPredictor16x8(int32_t iListNum,
                                    int32_t iSubBlockNum,
                                    H264DecoderMotionVector *pPred);
    void ReconstructMVPredictor8x16(int32_t iListNum,
                                    int32_t iSubBlockNum,
                                    H264DecoderMotionVector *pPred);

    // Decode motion vectors
    void ReconstructDirectMotionVectorsSpatial(bool isDirectMB);
    void ReconstructMotionVectors4x4(const uint8_t pCodMVd,
                                       uint32_t ListNum);
    void ReconstructMotionVectors4x4(const uint8_t *pBlkIdx,
                                       const uint8_t* pCodMVd,
                                       uint32_t ListNum);
    void ReconstructMotionVectors16x8(const uint8_t *pCodMVd,
                                        uint32_t ListNum);
    void ReconstructMotionVectors8x16(const uint8_t *pCodMVd,
                                        uint32_t ListNum);

    void DecodeDirectMotionVectors(bool isDirectMB);

    CoeffsPtrCommon  m_psBuffer;
    Mutex m_mGuard;

    SegmentDecoderHPBase* m_SD;

protected:

    virtual SegmentDecoderHPBase* CreateSegmentDecoder();

    virtual void StartProcessingSegment(H264Task &Task);
    virtual void EndProcessingSegment(H264Task &Task);

private:

    // we lock the assignment operator to avoid any
    // accasional assignments
    H264SegmentDecoderMultiThreaded & operator = (H264SegmentDecoderMultiThreaded &)
    {
        return *this;

    } // H264SegmentDecoderMultiThreaded & operator = (H264SegmentDecoderMultiThreaded &)
};

} // namespace UMC

#endif /* __UMC_H264_SEGMENT_DECODER_MT_H */
#endif // UMC_ENABLE_H264_VIDEO_DECODER
