/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2013-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_VA_PACKER_H
#define __UMC_H265_VA_PACKER_H

#include "umc_va_base.h"

#ifndef UMC_RESTRICTED_CODE_VA

#ifdef UMC_VA_DXVA
#include "umc_hevc_ddi.h"
#endif

#if defined(UMC_VA_LINUX)
#include <va/va_dec_hevc.h>
#endif

namespace UMC_HEVC_DECODER
{

class H265DecoderFrame;
class H265DecoderFrameInfo;
class H265Slice;
class TaskSupplier_H265;

class Packer
{
public:
    Packer(UMC::VideoAccelerator * va);
    virtual ~Packer();

    virtual UMC::Status GetStatusReport(void * pStatusReport, size_t size) = 0;

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;

    virtual void PackAU(const H265DecoderFrame *pCurrentFrame, TaskSupplier_H265 * supplier) = 0;

    virtual void PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 * supplier) = 0;

    virtual bool PackSliceParams(H265Slice *pSlice, Ipp32u &sliceNum, bool isLastSlice) = 0;

    virtual void PackQmatrix(const H265Slice *pSlice) = 0;

    static Packer * CreatePacker(UMC::VideoAccelerator * va);

protected:
    UMC::VideoAccelerator *m_va;
};

#ifdef UMC_VA_DXVA

class PackerDXVA2 : public Packer
{
public:
    PackerDXVA2(UMC::VideoAccelerator * va);

    virtual UMC::Status GetStatusReport(void * pStatusReport, size_t size);

    virtual void BeginFrame();
    virtual void EndFrame();

    virtual void PackQmatrix(const H265Slice *pSlice);

    virtual void PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 * supplier);

    virtual bool PackSliceParams(H265Slice *pSlice, Ipp32u &sliceNum, bool isLastSlice);

    virtual void PackAU(const H265DecoderFrame *pCurrentFrame, TaskSupplier_H265 * supplier);

    void GetSliceVABuffers(
        DXVA_Intel_Slice_HEVC_Long **ppSliceHeader, Ipp32s headerSize,
        void **ppSliceData, Ipp32s dataSize,
        Ipp32s dataAlignment);

protected:
    Ipp32u              m_statusReportFeedbackCounter;
    DXVA_Intel_PicEntry_HEVC  m_refFrameListCache[16];
    int                 m_refFrameListCacheSize;
};

class MSPackerDXVA2 : public PackerDXVA2
{
public:
    MSPackerDXVA2(UMC::VideoAccelerator * va);

    virtual void PackQmatrix(const H265Slice *pSlice);

    virtual void PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 * supplier);

    virtual bool PackSliceParams(H265Slice *pSlice, Ipp32u &sliceNum, bool isLastSlice);
};

class PackerDXVA2_Widevine: public PackerDXVA2
{
public:
    PackerDXVA2_Widevine(UMC::VideoAccelerator * va);

    virtual void PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 * supplier);

    virtual void PackAU(const H265DecoderFrame *pCurrentFrame, TaskSupplier_H265 * supplier);
};

#endif // UMC_VA_DXVA


#if defined(UMC_VA_LINUX)

class PackerVA : public Packer
{
public:
    PackerVA(UMC::VideoAccelerator * va);

    virtual UMC::Status GetStatusReport(void * pStatusReport, size_t size);

    virtual void PackQmatrix(const H265Slice *pSlice);

    virtual void PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 * supplier);

    virtual bool PackSliceParams(H265Slice *pSlice, Ipp32u &sliceNum, bool isLastSlice);

    virtual void BeginFrame();
    virtual void EndFrame();

    virtual void PackAU(const H265DecoderFrame *pCurrentFrame, TaskSupplier_H265 * supplier);

private:

    void CreateSliceDataBuffer(H265DecoderFrameInfo * sliceInfo);
    void CreateSliceParamBuffer(H265DecoderFrameInfo * sliceInfo);

};

class PackerVA_Widevine: public PackerVA
{
public:
    PackerVA_Widevine(UMC::VideoAccelerator * va);

    virtual void PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 * supplier);

    virtual void PackAU(const H265DecoderFrame *pCurrentFrame, TaskSupplier_H265 * supplier);
};

#endif // UMC_VA_LINUX

inline bool IsVLDProfile (Ipp32s profile)
{
    return (profile & UMC::VA_VLD) != 0;
}

} // namespace UMC_HEVC_DECODER

#endif // UMC_RESTRICTED_CODE_VA
#endif /* __UMC_H265_VA_PACKER_H */
#endif // UMC_ENABLE_H265_VIDEO_DECODER
