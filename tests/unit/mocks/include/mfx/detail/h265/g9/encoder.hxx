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

enum
{
    SLICE_STRUCT_ONESLICE           = 0,    // Once slice for the whole frame
    SLICE_STRUCT_POW2ROWS           = 1,    // Slices are power of 2 number of rows, all slices the same
    SLICE_STRUCT_ROWSLICE           = 2,    // Slices are any number of rows, all slices the same
    SLICE_STRUCT_ARBITRARYROWSLICE  = 3,    // Slices are any number of rows, slices can be different
    SLICE_STRUCT_ARBITRARYMBSLICE   = 4     // Slices are any number of MBs, slices can be different
    // 5 - 7 are Reserved
};

int constexpr ENCODE_HEVC_MAX_4K_PIC_WIDTH   = 4096;
int constexpr ENCODE_HEVC_MAX_4K_PIC_HEIGHT  = 2176;

int constexpr CODECHAL_ENCODE_HEVC_NUM_MAX_VME_L0_REF_G9    = 3;
int constexpr CODECHAL_ENCODE_HEVC_NUM_MAX_VME_L1_REF_G9    = 1;

int constexpr ENCODE_HEVC_SKL_TU_SUPPORT = (1 << (1 - 1)) | (1 << (4 - 1)) | (1 << (7 - 1)); // SKL support TU1, TU4, and TU7

int constexpr CODECHAL_ENCODE_HEVC_MAX_NUM_ROI    = 16;

namespace g9
{
    template <bool LowPower>
    inline constexpr
    ENCODE_CAPS_HEVC caps(std::integral_constant<bool, LowPower>)
    {
        return
        {
            // Coding Limits
            {
                TRUE,                   // CodingLimitSet
                TRUE,                   // BitDepth8Only  Note: FtrEncodeHEVC10bit is enabled on KBL SKU
                TRUE,                   // Color420Only
                SLICE_STRUCT_ARBITRARYMBSLICE,  // SliceStructure
                FALSE,                  // SliceIPOnly
                TRUE,                   // SliceIPBOnly
                TRUE,                   // NoWeightedPred
                TRUE,                   // NoMinorMVs
                TRUE,                   // RawReconRefToggle
                TRUE,                   // NoInterlacedField
                TRUE,                   // BRCReset
                TRUE,                   // RollingIntraRefresh
                TRUE,                   // UserMaxFrameSizeSupport
                TRUE,                   // FrameLevelRateCtrl
                FALSE,                  // SliceByteSizeCtrl
                TRUE,                   // VCMBitRateControl
                TRUE,                   // Parallel BRC
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
                0                       // MaxEncodedBitDepth
            },

            ENCODE_HEVC_MAX_4K_PIC_WIDTH,      // MaxPicWidth
            ENCODE_HEVC_MAX_4K_PIC_HEIGHT,     // MaxPicHeight
            CODECHAL_ENCODE_HEVC_NUM_MAX_VME_L0_REF_G9, // MaxNum_Reference0
            CODECHAL_ENCODE_HEVC_NUM_MAX_VME_L1_REF_G9, // MaxNum_Reference1
            TRUE,                           // MBBRCSupport
            ENCODE_HEVC_SKL_TU_SUPPORT,     // TUSupport

            // ROICaps
            {
                CODECHAL_ENCODE_HEVC_MAX_NUM_ROI,    // MaxNumOfROI
                TRUE                                 // ROIBRCPriorityLevelSupport
            },

            // CodingLimits2
            {
                FALSE,                      // SliceLevelReportSupport
                0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                TRUE                        // RGBEncodingSupport
            },

            0,                          // MaxNum_WeightedPredL0
            0                           // MaxNum_WeightedPredL1
        };
    }
}
