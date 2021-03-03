// Copyright (c) 2020 Intel Corporation
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

#ifndef MOCKS_MFX_ENCODER_HXX
    #error "[mocks/include/mfx/detail/encoder.hxx] need to be included first"
#endif

int constexpr CODECHAL_ENCODE_HEVC_MAX_NUM_WEIGHTED_PRED_L0 = 1;
int constexpr CODECHAL_ENCODE_HEVC_MAX_NUM_WEIGHTED_PRED_L1 = 1;

int constexpr ENCODE_DP_HEVC_ROI_BLOCK_SIZE       = 1;  //From DDI, 0:8x8, 1:16x16, 2:32x32, 3:64x64

namespace g11
{
    inline constexpr
    ENCODE_CAPS_HEVC caps(std::false_type /*VME*/)
    {
        static constexpr ENCODE_CAPS_HEVC c =
        {
            // Coding Limits
            {
                TRUE,                   // CodingLimitSet
                FALSE,                  // BitDepth8Only
                FALSE,                  // Color420Only
                SLICE_STRUCT_ARBITRARYMBSLICE,  // SliceStructure
                FALSE,                  // SliceIPOnly
                TRUE,                   // SliceIPBOnly
                FALSE,                  // NoWeightedPred
                TRUE,                   // NoMinorMVs
                TRUE,                   // RawReconRefToggle
                TRUE,                   // NoInterlacedField
                FALSE,                  // BRCReset
                FALSE,                  // RollingIntraRefresh
                TRUE,                   // UserMaxFrameSizeSupport
                FALSE,                  // FrameLevelRateCtrl
                FALSE,                  // SliceByteSizeCtrl
                FALSE,                  // VCMBitRateControl
                TRUE,                   // Parallel BRC
                TRUE,                   // TileSupport
                FALSE,                  // SkipFrame
                FALSE,                  // MbQpDataSupport
                FALSE,                  // SliceLevelWeightedPred
                TRUE,                   // LumaWeightedPred
                FALSE,                  // ChromaWeightedPred
                TRUE,                   // QVBRBRCSupport
                FALSE,                  // HMEOffsetSupport
                TRUE,                   // YUV422ReconSupport
                FALSE,                  // YUV444ReconSupport
                FALSE,                  // RGBReconSupport
                TRUE                    // MaxEncodedBitDepth
            },

            ENCODE_HEVC_MAX_8K_PIC_WIDTH,      // MaxPicWidth
            ENCODE_HEVC_MAX_8K_PIC_HEIGHT,     // MaxPicHeight
            CODECHAL_ENCODE_HEVC_NUM_MAX_VME_L0_REF_G10, // MaxNum_Reference0
            CODECHAL_ENCODE_HEVC_NUM_MAX_VME_L1_REF_G10, // MaxNum_Reference1
            TRUE,                           // MBBRCSupport
            ENCODE_HEVC_SKL_TU_SUPPORT,     // TUSupport

            // ROICaps
            {
                CODECHAL_ENCODE_HEVC_MAX_NUM_ROI,    // MaxNumOfROI
                FALSE,                               // ROIBRCPriorityLevelSupport
                ENCODE_DP_HEVC_ROI_BLOCK_SIZE        // BlockSize
            },

            // CodingLimits2
            {
                TRUE,                       // SliceLevelReportSupport
                0, 0, 0, 0,
                0,                          // IntraRefreshBlockUnitSize
		        HEVC_LCU_SIZE_64x64 | HEVC_LCU_SIZE_32x32,     // LCUSizeSupported
                0,                          // MaxNumDeltaQP
                FALSE,                      // DirtyRectSupport
                FALSE,                      // MoveRectSupport
                TRUE,                       // FrameSizeToleranceSupport
                FALSE,                      // HWCounterAutoIncrementSupport
                TRUE,                       // ROIDeltaQPSupport
                1,                          // NumScalablePipesMinus1
                TRUE,                       // NegativeQPSupport
                FALSE,                      // HRDConformanceSupport
                0, 0,
                TRUE                        // RGBEncodingSupport
            },

            CODECHAL_ENCODE_HEVC_MAX_NUM_WEIGHTED_PRED_L0,  // MaxNum_WeightedPredL0
            CODECHAL_ENCODE_HEVC_MAX_NUM_WEIGHTED_PRED_L0   // MaxNum_WeightedPredL1
        };

        return c;
    }

    inline constexpr
    ENCODE_CAPS_HEVC caps(std::true_type /*VDENC*/)
    {
        //TODO: Per SKU 'Color420Only' adjusment
        static const ENCODE_CAPS_HEVC c =
        {
            // Coding Limits
            {
                TRUE,                   // CodingLimitSet
                FALSE,                  // BitDepth8Only
                FALSE,                  // Color420Only
                SLICE_STRUCT_ARBITRARYMBSLICE,  // SliceStructure
                TRUE,                   // SliceIPOnly
                FALSE,                  // SliceIPBOnly
                FALSE,                   // NoWeightedPred
                TRUE,                   // NoMinorMVs
                FALSE,                  // RawReconRefToggle
                TRUE,                   // NoInterlacedField
                TRUE,                   // BRCReset
                TRUE,                   // RollingIntraRefresh
                TRUE,                   // UserMaxFrameSizeSupport
                TRUE,                   // FrameLevelRateCtrl
                TRUE,                   // SliceByteSizeCtrl
                TRUE,                   // VCMBitRateControl
                FALSE,                  // Parallel BRC
                TRUE,                   // TileSupport
                FALSE,                  // SkipFrame
                FALSE,                  // MbQpDataSupport
                FALSE,                  // SliceLevelWeightedPred
                TRUE,                   // LumaWeightedPred
                TRUE,                   // ChromaWeightedPred
                TRUE,                   // QVBRBRCSupport
                FALSE,                  // HMEOffsetSupport
                FALSE,                  // YUV422ReconSupport
                TRUE,                   // YUV444ReconSupport
                FALSE,                  // RGBReconSupport
                1                       // MaxEncodedBitDepth
            },

            ENCODE_HEVC_MAX_8K_PIC_WIDTH,          // MaxPicWidth
            ENCODE_HEVC_MAX_8K_PIC_HEIGHT,         // MaxPicHeight
            CODECHAL_ENCODE_HEVC_NUM_MAX_VDENC_L0_REF_G10,   // MaxNum_Reference0
            CODECHAL_ENCODE_HEVC_NUM_MAX_VDENC_L1_REF_G10,   // MaxNum_Reference1
            TRUE,                               // MBBRCSupport
            ENCODE_HEVC_SKL_TU_SUPPORT,         // TUSupport

            // ROICaps
            {
                CODECHAL_ENCODE_HEVC_MAX_NUM_ROI,            // MaxNumOfROI
                FALSE,                                       // ROIBRCPriorityLevelSupport
                ENCODE_VDENC_HEVC_ROI_BLOCKSIZE_G10,         // BlockSize
            },

            // CodingLimits2
            {
                TRUE,                       // SliceLevelReportSupport
                0,
                0,
                TRUE,                       // CustomRoundingControl
                TRUE,                       // LowDelayBRCSupport
                ENCODE_VDENC_HEVC_ROI_BLOCKSIZE_G10,    // IntraRefreshBlockUnitSize 32x32
                HEVC_LCU_SIZE_64x64,        // LCUSizeSupported
                16,                         // MaxNumDeltaQP
                TRUE,                       // DirtyRectSupport
                FALSE,                      // MoveRectSupport
                TRUE,                       // FrameSizeToleranceSupport (Low Delay, Sliding Window)
                FALSE,                      // HWCounterAutoIncrementSupport
                TRUE,                       // ROIDeltaQPSupport
                1,                          // NumScalablePipesMinus1
                FALSE,                      // NegativeQPSupport
                FALSE,                      // HRDConformanceSupport
                FALSE,                      // TileBasedEncodingSupport
                0,                          // PartialFrameUpdateSupport
                TRUE,                       // RGBEncodingSupport
                FALSE,                      // LLCStreamingBufferSupport
                FALSE,                      // DDRStreamingBufferSupport
            },

            3,                              // MaxNum_WeightedPredL0
            3,                              // MaxNum_WeightedPredL1
            0,                              // MaxNumOfDirtyRect
            0,                              // MaxNumOfMoveRect
            0,                              // MaxNumOfConcurrentFramesMinus1
            0,                              // LLCSizeInMBytes

            // CodingLimits3
            {
                FALSE,                      // PFrameSupport
                TRUE,                       // LookaheadAnalysisSupport
                FALSE,                      // LookaheadBRCSupport
            }
        };

        return c;
    }
}
