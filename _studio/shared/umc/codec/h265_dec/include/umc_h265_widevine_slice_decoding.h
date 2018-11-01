//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#ifndef __UMC_H265_WIDEVINE_SLICE_DECODING_H
#define __UMC_H265_WIDEVINE_SLICE_DECODING_H

#include "umc_va_base.h"
#if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)

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

#endif // #if defined (UMC_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)
#endif // __UMC_H265_WIDEVINE_SLICE_DECODING_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
