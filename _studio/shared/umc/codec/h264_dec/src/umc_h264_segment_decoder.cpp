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

#include "umc_h264_segment_decoder.h"
#include "umc_h264_segment_decoder_mt.h"
#include "umc_h264_bitstream_inlines.h"
#include "umc_h264_segment_decoder_templates.h"

namespace UMC
{

H264SegmentDecoder::H264SegmentDecoder(TaskBroker * pTaskBroker)
        : H264SegmentDecoderBase(pTaskBroker)
{
    m_pCoefficientsBuffer = NULL;

    m_pSlice = 0;
    m_pSliceHeader = 0;

    // set pointer to backward prediction buffer
    m_pPredictionBuffer = align_pointer<UMC::PlanePtrYCommon>(m_BufferForBackwardPrediction, DEFAULT_ALIGN_VALUE);

    bit_depth_luma = 8;
    bit_depth_chroma = 8;
} // H264SegmentDecoder::H264SegmentDecoder(H264SliceStore &Store)

H264SegmentDecoder::~H264SegmentDecoder(void)
{
    Release();

} // H264SegmentDecoder::~H264SegmentDecoder(void)

void H264SegmentDecoder::Release(void)
{
    if (m_pCoefficientsBuffer)
        delete [] (int32_t*)m_pCoefficientsBuffer;

    m_pCoefficientsBuffer = NULL;
} // void H264SegmentDecoder::Release(void)

Status H264SegmentDecoder::Init(int32_t iNumber)
{
    // release object before initialization
    Release();

    // save ordinal number
    m_iNumber = iNumber;

    m_pCoefficientsBuffer = (UMC::CoeffsCommon*)(new int32_t[COEFFICIENTS_BUFFER_SIZE + DEFAULT_ALIGN_VALUE]);

    return UMC_OK;

} // Status H264SegmentDecoder::Init(int32_t sNumber)

UMC::CoeffsPtrCommon H264SegmentDecoder::GetCoefficientsBuffer(uint32_t nNum)
{
    return align_pointer<UMC::CoeffsPtrCommon> (m_pCoefficientsBuffer +
                                     COEFFICIENTS_BUFFER_SIZE * nNum, DEFAULT_ALIGN_VALUE);

} // int16_t *H264SegmentDecoder::GetCoefficientsBuffer(uint32_t nNum)

SegmentDecoderHPBase *CreateSD(int32_t bit_depth_luma,
                               int32_t bit_depth_chroma,
                               bool is_field,
                               int32_t color_format,
                               bool is_high_profile)
{
    if (bit_depth_chroma > 8 || bit_depth_luma > 8)
    {
        return CreateSD_ManyBits(bit_depth_luma,
                                 bit_depth_chroma,
                                 is_field,
                                 color_format,
                                 is_high_profile);
    }
    else
    {
        if (is_field)
        {
            return CreateSegmentDecoderWrapper<int16_t, uint8_t, uint8_t, true>::CreateSoftSegmentDecoder(color_format, is_high_profile);
        } else {
            return CreateSegmentDecoderWrapper<int16_t, uint8_t, uint8_t, false>::CreateSoftSegmentDecoder(color_format, is_high_profile);
        }
    }

} // SegmentDecoderHPBase *CreateSD(int32_t bit_depth_luma,

static
void InitializeSDCreator()
{
    CreateSegmentDecoderWrapper<int16_t, uint8_t, uint8_t, true>::CreateSoftSegmentDecoder(0, false);
    CreateSegmentDecoderWrapper<int16_t, uint8_t, uint8_t, false>::CreateSoftSegmentDecoder(0, false);
}

class SDInitializer
{
public:
    SDInitializer()
    {
        InitializeSDCreator();
        InitializeSDCreator_ManyBits();
    }
};

static SDInitializer tableInitializer;


} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
