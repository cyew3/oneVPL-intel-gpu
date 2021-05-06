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

#ifndef __H265_PREDICTION_H
#define __H265_PREDICTION_H

#include "umc_h265_dec_defs.h"
#include "umc_h265_yuv.h"
#include "umc_h265_motion_info.h"
#include "mfx_h265_optimization.h" // common data types here for interpolation

namespace UMC_HEVC_DECODER
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Class definition
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class DecodingContext;
class H265DecoderFrame;

// Prediction unit information data for motion compensation
struct H265PUInfo
{
    H265MVInfo *interinfo;
    H265DecoderFrame *refFrame[2];
    uint32_t PartAddr;
    uint32_t Width, Height;
};

// prediction class
class H265Prediction
{
public:

protected:
    PlanePtrY m_temp_interpolarion_buffer;
    uint32_t m_MaxCUSize;

    DecodingContext* m_context;

    H265DecYUVBufferPadded m_YUVPred[2];

    // Interpolate one reference frame block
    template <EnumTextType c_plane_type, bool bi, typename H265PlaneYCommon>
    void inline H265_FORCEINLINE PredInterUni(H265CodingUnit* pCU, H265PUInfo &PUi, EnumRefPicList RefPicList, H265DecYUVBufferPadded *YUVPred, MFX_HEVC_PP::EnumAddAverageType eAddAverage = MFX_HEVC_PP::AVERAGE_NO);

    // Returns true if reference indexes in ref lists point to the same frame and motion vectors in these references are equal
    bool CheckIdenticalMotion(H265CodingUnit* pCU, const H265MVInfo &mvInfo);

    // Calculate mean average from two references
    template<typename PixType>
    static void WriteAverageToPic(
                     const PixType * in_pSrc0,
                     uint32_t in_Src0Pitch,      // in samples
                     const PixType * in_pSrc1,
                     uint32_t in_Src1Pitch,      // in samples
                     PixType* H265_RESTRICT in_pDst,
                     uint32_t in_DstPitch,       // in samples
                     int32_t width,
                     int32_t height);

    // Copy prediction unit extending its bits for later addition with PU from another reference
    template <typename PixType>
    static void CopyExtendPU(const PixType * in_pSrc,
                     uint32_t in_SrcPitch, // in samples
                     int16_t* H265_RESTRICT in_pDst,
                     uint32_t in_DstPitch, // in samples
                     int32_t width,
                     int32_t height,
                     int c_shift);

    // Do weighted prediction from one reference frame
    template<typename PixType>
    void CopyWeighted(H265DecoderFrame* frame, H265DecYUVBufferPadded* src, uint32_t CUAddr, uint32_t PartIdx, uint32_t Width, uint32_t Height,
        int32_t *w, int32_t *o, int32_t *logWD, int32_t *round, uint32_t bit_depth, uint32_t bit_depth_chroma);

    // Do weighted prediction from two reference frames
    template<typename PixType>
    void CopyWeightedBidi(H265DecoderFrame* frame, H265DecYUVBufferPadded* src1, H265DecYUVBufferPadded* src2, uint32_t CUAddr, uint32_t PartIdx, uint32_t Width, uint32_t Height,
        int32_t *w0, int32_t *w1, int32_t *logWD, int32_t *round, uint32_t bit_depth, uint32_t bit_depth_chroma);

    // Calculate address offset inside of source frame
    int32_t GetAddrOffset(uint32_t PartUnitIdx, uint32_t width);

    // Motion compensation with bit depth constant
    template<typename PixType>
    void MotionCompensationInternal(H265CodingUnit* pCU, uint32_t AbsPartIdx, uint32_t Depth);

    // Perform weighted addition from one or two reference sources
    template<typename PixType>
    void WeightedPrediction(H265CodingUnit* pCU, const H265PUInfo & PUi);

public:
    H265Prediction();
    virtual ~H265Prediction();

    // Allocate temporal buffers which may be necessary to store interpolated reference frames
    void InitTempBuff(DecodingContext* context);

    void MotionCompensation(H265CodingUnit* pCU, uint32_t AbsPartIdx, uint32_t Depth);
};

} // end namespace UMC_HEVC_DECODER

#endif // __H265_PREDICTION_H
#endif // MFX_ENABLE_H265_VIDEO_DECODE
