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

//This file is included from [mocks/include/mfx/detail/encoder.hxx]
//DO NOT include it into the project manually

#ifndef MOCKS_MFX_ENCODER_HXX
    #error "[mocks/include/mfx/detail/encoder.hxx] need to be included first"
#endif

enum
{
    HEVC_LCU_SIZE_16x16 = 1,
    HEVC_LCU_SIZE_32x32 = 2,
    HEVC_LCU_SIZE_64x64 = 4,
    HEVC_LCU_SIZE_64x64_DP = 6
};

int constexpr ENCODE_HEVC_MAX_8K_PIC_WIDTH   = 8192;
int constexpr ENCODE_HEVC_MAX_8K_PIC_HEIGHT  = 8192;

int constexpr CODECHAL_ENCODE_HEVC_NUM_MAX_VME_L0_REF_G10   = 4;
int constexpr CODECHAL_ENCODE_HEVC_NUM_MAX_VME_L1_REF_G10   = 4;

int constexpr CODECHAL_ENCODE_HEVC_NUM_MAX_VDENC_L0_REF_G10 = 3;
int constexpr CODECHAL_ENCODE_HEVC_NUM_MAX_VDENC_L1_REF_G10 = 3;

int constexpr ENCODE_HEVC_CNL_TU_SUPPORT = ((1 << (1 - 1)) | (1 << (4 - 1))); // CNL HEVC Dual pipe support TU1, TU4 only

int constexpr ENCODE_VDENC_HEVC_MAX_ROI_NUMBER_G10 = 8;
int constexpr ENCODE_VDENC_HEVC_ROI_BLOCKSIZE_G10  = 2; // 0:8x8, 1:16x16, 2:32x32, 3:64x64

namespace g10
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
                TRUE,                   // Color420Only
                SLICE_STRUCT_ARBITRARYMBSLICE,  // SliceStructure
                FALSE,                  // SliceIPOnly
                TRUE,                   // SliceIPBOnly
                TRUE,                   // NoWeightedPred
                TRUE,                   // NoMinorMVs
                TRUE,                   // RawReconRefToggle
                TRUE,                   // NoInterlacedField
                FALSE,                  // BRCReset
                FALSE,                  // RollingIntraRefresh
                TRUE,                   // UserMaxFrameSizeSupport
                FALSE,                  // FrameLevelRateCtrl
                FALSE,                  // SliceByteSizeCtrl
                FALSE,                  // VCMBitRateControl
                FALSE,                  // Parallel BRC
                FALSE,                  // TileSupport
                FALSE,                  // SkipFrame
                FALSE,                  // MbQpDataSupport
                FALSE,                  // SliceLevelWeightedPred
                FALSE,                  // LumaWeightedPred
                FALSE,                  // ChromaWeightedPred
                FALSE,                  // QVBRBRCSupport
                FALSE,                  // HMEOffsetSupport
                FALSE,                  // YUV422ReconSupport
                FALSE,                  // YUV444ReconSupport
                FALSE,                  // RGBReconSupport
                1                       // MaxEncodedBitDepth
            },

            ENCODE_HEVC_MAX_4K_PIC_WIDTH,      // MaxPicWidth
            ENCODE_HEVC_MAX_4K_PIC_HEIGHT,     // MaxPicHeight
            CODECHAL_ENCODE_HEVC_NUM_MAX_VME_L0_REF_G10, // MaxNum_Reference0
            CODECHAL_ENCODE_HEVC_NUM_MAX_VME_L1_REF_G10, // MaxNum_Reference1
            TRUE,                           // MBBRCSupport
            ENCODE_HEVC_CNL_TU_SUPPORT,     // TUSupport

            // ROICaps
            {
                0,                          // MaxNumOfROI
                FALSE                       // ROIBRCPriorityLevelSupport
            },

            // CodingLimits2
            {
                TRUE,                      // SliceLevelReportSupport
                0, 0, 0, 0,
                0,                          // IntraRefreshBlockUnitSize
                HEVC_LCU_SIZE_64x64_DP         // LCUSizeSupported
            },

            0,                          // MaxNum_WeightedPredL0
            0                           // MaxNum_WeightedPredL1
        };

        return c;
    }

    inline constexpr
    ENCODE_CAPS_HEVC caps(std::true_type /*VDENC*/)
    {
        static constexpr ENCODE_CAPS_HEVC c =
        {
            // Coding Limits
            {
                TRUE,                   // CodingLimitSet
                FALSE,                  // BitDepth8Only
                TRUE,                   // Color420Only
                SLICE_STRUCT_ROWSLICE,  // SliceStructure
                TRUE,                   // SliceIPOnly
                FALSE,                  // SliceIPBOnly
                FALSE,                  // NoWeightedPred
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
                FALSE,                  // TileSupport
                FALSE,                  // SkipFrame
                FALSE,                  // MbQpDataSupport
                TRUE,                   // SliceLevelWeightedPred
                TRUE,                   // LumaWeightedPred
                TRUE,                   // ChromaWeightedPred
                TRUE,                   // QVBRBRCSupport
                FALSE,                  // HMEOffsetSupport
                FALSE,                  // YUV422ReconSupport
                FALSE,                  // YUV444ReconSupport
                FALSE,                  // RGBReconSupport
                1                       // MaxEncodedBitDepth
            },

            ENCODE_HEVC_MAX_8K_PIC_WIDTH,      // MaxPicWidth
            ENCODE_HEVC_MAX_8K_PIC_HEIGHT,     // MaxPicHeight
            CODECHAL_ENCODE_HEVC_NUM_MAX_VDENC_L0_REF_G10,   // MaxNum_Reference0
            CODECHAL_ENCODE_HEVC_NUM_MAX_VDENC_L1_REF_G10,   // MaxNum_Reference1
            TRUE,                               // MBBRCSupport
            ENCODE_HEVC_SKL_TU_SUPPORT,         // TUSupport

            // ROICaps
            {
                ENCODE_VDENC_HEVC_MAX_ROI_NUMBER_G10,        // MaxNumOfROI
                FALSE,                                       // ROIBRCPriorityLevelSupport
                ENCODE_VDENC_HEVC_ROI_BLOCKSIZE_G10,         // BlockSize
            },

            // CodingLimits2
            {
                TRUE,                       // SliceLevelReportSupport
                0, 0, 0, 0,
                ENCODE_VDENC_HEVC_ROI_BLOCKSIZE_G10,     // IntraRefreshBlockUnitSize 32x32
                HEVC_LCU_SIZE_64x64,        // LCUSizeSupported
                8,                          // MaxNumDeltaQP
                TRUE,                       // DirtyRectSupport
                FALSE,                      // MoveRectSupport
                TRUE,                       // FrameSizeToleranceSupport (Low Delay, Sliding Window)
                FALSE,                      // HWCounterAutoIncrementSupport
                TRUE,                       // ROIDeltaQPSupport
            },

            3,                              // MaxNum_WeightedPredL0, same as max num references
            3                               // MaxNum_WeightedPredL1, same as max num references
        };

        return c;
    }
}
