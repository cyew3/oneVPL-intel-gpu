/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#ifndef __UMC_H265_WIDEVINE_SUPPLIER_H
#define __UMC_H265_WIDEVINE_SUPPLIER_H

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_h265_va_supplier.h"
#include "umc_h265_widevine_decrypter.h"

namespace UMC_HEVC_DECODER
{

class WidevineTaskSupplier : public VATaskSupplier
{
public:

    WidevineTaskSupplier();

    ~WidevineTaskSupplier();

    virtual UMC::Status Init(UMC::VideoDecoderParams *pInit);

    virtual void Reset();

    virtual UMC::Status AddSource(DecryptParametersWrapper* pDecryptParams);

protected:

    virtual UMC::Status DecryptWidevineHeaders(UMC::MediaData *pSource, DecryptParametersWrapper* pDecryptParams);

    UMC::Status ParseWidevineVPSSPSPPS(DecryptParametersWrapper* pDecryptParams);

    virtual H265Slice * ParseWidevineSliceHeader(DecryptParametersWrapper* pDecryptParams);

    virtual UMC::Status ParseWidevineSEI(DecryptParametersWrapper* pDecryptParams);

    virtual UMC::Status AddOneFrame(DecryptParametersWrapper* pDecryptParams);

    virtual UMC::Status AddOneFrame(UMC::MediaData * pSource);

    virtual void CompleteFrame(H265DecoderFrame * pFrame);

private:
    WidevineTaskSupplier & operator = (WidevineTaskSupplier &)
    {
        return *this;
    }

    WidevineDecrypter * m_pWidevineDecrypter;
};

} // namespace UMC_HEVC_DECODER

#endif // UMC_RESTRICTED_CODE_VA

#endif // __UMC_H265_WIDEVINE_SUPPLIER_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
