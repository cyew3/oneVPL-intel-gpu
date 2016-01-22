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
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_WIDEVINE_SUPPLIER_H
#define __UMC_H264_WIDEVINE_SUPPLIER_H

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_h264_va_supplier.h"
#include "umc_h264_widevine_decrypter.h"

namespace UMC
{

class POCDecoderWidevine : public POCDecoder
{
public:
    void DecodePictureOrderCount(const H264Slice *slice, Ipp32s frame_num);
};


class WidevineTaskSupplier : public VATaskSupplier
{
public:

    WidevineTaskSupplier();

    ~WidevineTaskSupplier();

    virtual Status Init(VideoDecoderParams *pInit);

    virtual void Reset();

    virtual Status AddSource(DecryptParametersWrapper* pDecryptParams);

protected:

    virtual Status DecryptWidevineHeaders(MediaData *pSource, DecryptParametersWrapper* pDecryptParams);

    Status ParseWidevineSPSPPS(DecryptParametersWrapper* pDecryptParams);

    virtual H264Slice * ParseWidevineSliceHeader(DecryptParametersWrapper* pDecryptParams);

    virtual Status ParseWidevineSEI(DecryptParametersWrapper* pDecryptParams);

    virtual Status AddOneFrame(DecryptParametersWrapper* pDecryptParams);

    virtual Status AddOneFrame(MediaData * pSource);

    virtual Status CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field);

private:
    WidevineTaskSupplier & operator = (WidevineTaskSupplier &)
    {
        return *this;
    }

    WidevineDecrypter * m_pWidevineDecrypter;
};

} // namespace UMC

#endif // UMC_RESTRICTED_CODE_VA

#endif // __UMC_H264_WIDEVINE_SUPPLIER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
