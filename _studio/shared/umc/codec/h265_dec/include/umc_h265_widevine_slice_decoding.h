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
#if defined (MFX_ENABLE_H265_VIDEO_DECODE)

#ifndef __UMC_H265_WIDEVINE_SLICE_DECODING_H
#define __UMC_H265_WIDEVINE_SLICE_DECODING_H

#include "umc_va_base.h"
#if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#ifndef MFX_ENABLE_CPLIB

#include "umc_h265_slice_decoding.h"
#include "umc_h265_widevine_decrypter.h"

namespace UMC_HEVC_DECODER
{

class H265WidevineSlice : public H265Slice
{
public:
    // Default constructor
    H265WidevineSlice();
    // Destructor
    virtual ~H265WidevineSlice(void);

    virtual void SetDecryptParameters(DecryptParametersWrapper* pDecryptParameters);

    // Decode slice header and initializ slice structure with parsed values
    virtual bool Reset(PocDecoding * pocDecoding);

    // Parse beginning of slice header to get PPS ID
    virtual int32_t RetrievePicParamSetNumber();

    using H265Slice::UpdateReferenceList;
    // Build reference lists from slice reference pic set. HEVC spec 8.3.2
    UMC::Status UpdateReferenceList(H265DBPList *dpb);

    void SetWidevineStatusReportNumber(uint16_t number) {m_WidevineStatusReportNumber = number;}
    uint16_t GetWidevineStatusReportNumber() {return m_WidevineStatusReportNumber;}

public:  // DEBUG !!!! should remove dependence

    // Initialize slice structure to default values
    virtual void Reset();

    // Release resources
    virtual void Release();

    // Decoder slice header and calculate POC
    virtual bool DecodeSliceHeader(PocDecoding * pocDecoding);

private:
    DecryptParametersWrapper m_DecryptParams;

    uint16_t m_WidevineStatusReportNumber;

};

} // namespace UMC_HEVC_DECODER

#endif // #ifndef MFX_ENABLE_CPLIB
#endif // #if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#endif // __UMC_H265_WIDEVINE_SLICE_DECODING_H
#endif // MFX_ENABLE_H265_VIDEO_DECODE
