// Copyright (c) 2012-2018 Intel Corporation
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

#ifdef _MSVC_LANG
#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#endif
#if defined (target_gen11)
#ifdef VME_Input_S1
#undef VME_Input_S1
#endif
#define VME_Input_S1(p, t, r, c) p.row(r).format<t>().select<1,1>(c)

// Select one element from the output result
#ifdef VME_Output_S1
#undef VME_Output_S1
#endif
#define VME_Output_S1(p, t, r, c) p.row(r).format<t>().select<1,1>(c)(0)


// Format = U16
#define VME_SET_UNIInput_SrcX(p, v) (VME_Input_S1(p, uint2, 0, 4) = v)
// Format = U16
#define VME_SET_UNIInput_SrcY(p, v) (VME_Input_S1(p, uint2, 0, 5) = v)

#define VME_SET_UNIInput_MaxNumMVs(p, v) (VME_Input_S1(p, uint1, 1, 4) = v)
// The value must be a multiple of 4. Range = [20, 64]
#define VME_SET_UNIInput_RefW(p, v) (VME_Input_S1(p, uint1, 0, 22) = v)
// The value must be a multiple of 4. Range = [20, 64]
#define VME_SET_UNIInput_RefH(p, v) (VME_Input_S1(p, uint1, 0, 23) = v)

#define VME_SET_UNIInput_EarlyImeSuccessEn(p) (VME_Input_S1(p, uint1, 1, 0) |= 0x20)

#define VME_SET_UNIInput_AdaptiveEn(p) (VME_Input_S1(p, uint1, 1, 0) |= 0x2)
#define VME_SET_UNIInput_T8x8FlagForInterEn(p) (VME_Input_S1(p, uint1, 1, 0) |= 0x80)
// Format = U8, Valid range [1,63]
#define VME_SET_UNIInput_LenSP(p, v) (VME_Input_S1(p, uint1, 1, 8) = v)
// Format = U8, Valid range [1,63]
#define VME_SET_UNIInput_MaxNumSU(p, v) (VME_Input_S1(p, uint1, 1, 9) = v)


#define VME_GET_IMEOutput_Rec0_16x16_Distortion(p, v) (v = VME_Output_S1(p, uint2, 7, 8))

// #define INT_MODE 0x0
// #define HALF_PEL_MODE 0x1
// #define QUARTER_PEL_MODE 0x3
#define VME_SET_UNIInput_SubPelMode(p, v) (VME_Input_S1(p, uint1, 0, 13) = VME_Input_S1(p, uint1, 0, 13) & 0xCF | (v << 4) )
#define VME_CLEAR_UNIInput_BMEDisableFBR(p) (VME_Input_S1(p, uint1, 0, 14) &= 0xFB)
// Format = U8
#define VME_SET_UNIInput_FBRMbModeInput(p, v) (VME_Input_S1(p, uint1, 2, 20) = v)
// Format = U8
#define VME_SET_UNIInput_FBRSubMBShapeInput(p, v) (VME_Input_S1(p, uint1, 2, 21) = v)
// Format = U8
#define VME_SET_UNIInput_FBRSubPredModeInput(p, v) (VME_Input_S1(p, uint1, 2, 22) = v)

// Format = U6, Valid Values: [16, 21, 32, 43, 48]
// #define VME_SET_UNIInput_BiWeight(p, v) (VME_Input_S1(p, uint1, 1, 6) = v)
#define VME_SET_UNIInput_BiWeight(p, v) (VME_Input_S1(p, uint1, 1, 6) = VME_Input_S1(p, uint1, 1, 6) & 0xC0 | v)
#define VME_CLEAR_UNIInput_BMEDisableFBR(p) (VME_Input_S1(p, uint1, 0, 14) &= 0xFB)

#endif