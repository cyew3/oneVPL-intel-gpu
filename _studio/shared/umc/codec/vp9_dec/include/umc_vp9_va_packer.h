/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_VP9_VIDEO_DECODER

#ifndef __UMC_VP9_VA_PACKER_H
#define __UMC_VP9_VA_PACKER_H

#include "umc_va_base.h"

#ifndef UMC_RESTRICTED_CODE_VA

#ifdef UMC_VA_DXVA
    #include "umc_vp9_ddi.h"
#endif

#if defined(UMC_VA_LINUX)
    #include "va/va_dec_vp9.h"
#endif

namespace UMC
{ class MediaData; }

namespace UMC_VP9_DECODER
{
    class VP9Bitstream;
    class VP9DecoderFrame;

class Packer
{

public:

    Packer(UMC::VideoAccelerator * va);
    virtual ~Packer();

    virtual UMC::Status GetStatusReport(void* pStatusReport, size_t size) = 0;

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;

    virtual void PackAU(VP9Bitstream*, VP9DecoderFrame const*) = 0;

    static Packer* CreatePacker(UMC::VideoAccelerator * va);

protected:

    UMC::VideoAccelerator *m_va;
};

#ifdef UMC_VA_DXVA

class PackerDXVA
    : public Packer
{
public:

    PackerDXVA(UMC::VideoAccelerator * va);

    void BeginFrame();
    void EndFrame();

    UMC::Status GetStatusReport(void* pStatusReport, size_t size);

protected:

    Ipp32u  m_report_counter;
};

class PackerIntel
    : public PackerDXVA
{

public:

    PackerIntel(UMC::VideoAccelerator * va);

    void PackAU(VP9Bitstream*, VP9DecoderFrame const*);

private:

    void PackPicParams(DXVA_Intel_PicParams_VP9*, VP9DecoderFrame const*);
    void PackSegmentParams(DXVA_Intel_Segment_VP9*, VP9DecoderFrame const*);
};

#if defined(NTDDI_WIN10_TH2) && (NTDDI_VERSION >= NTDDI_WIN10_TH2)
class PackerMS
    : public PackerDXVA
{

public:

    PackerMS(UMC::VideoAccelerator * va);

    void PackAU(VP9Bitstream*, VP9DecoderFrame const*);

private:

    void PackPicParams(DXVA_PicParams_VP9*, VP9DecoderFrame const*);
};
#endif

#endif // UMC_VA_DXVA


#if defined(UMC_VA_LINUX)

class PackerVA
    : public Packer
{

public:

    PackerVA(UMC::VideoAccelerator * va);

    UMC::Status GetStatusReport(void * pStatusReport, size_t size);

    void BeginFrame();
    void EndFrame();

    void PackAU(VP9Bitstream*, VP9DecoderFrame const*);

 private:

    void PackPicParams(VADecPictureParameterBufferVP9*, VP9DecoderFrame const*);
    void PackSliceParams(VASliceParameterBufferVP9*, VP9DecoderFrame const*);
};

#endif // UMC_VA_LINUX

} // namespace UMC_HEVC_DECODER

#endif // UMC_RESTRICTED_CODE_VA
#endif /* __UMC_VP9_VA_PACKER_H */
#endif // UMC_ENABLE_VP9_VIDEO_DECODER
