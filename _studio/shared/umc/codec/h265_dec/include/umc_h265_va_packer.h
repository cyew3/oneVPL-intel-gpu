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
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_VA_PACKER_H
#define __UMC_H265_VA_PACKER_H

#include "umc_va_base.h"

#ifndef UMC_RESTRICTED_CODE_VA

#ifdef UMC_VA_DXVA
#include "umc_hevc_ddi.h"
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

    virtual void PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 * supplier) = 0;

    virtual void PackSliceParams(H265Slice *pSlice, Ipp32u &sliceNum, bool isLong, bool isLastSlice) = 0;

    virtual void PackQmatrix(const H265Slice *pSlice) = 0;

    virtual void ExecuteBuffers() = 0;

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

    virtual void PackSliceParams(H265Slice *pSlice, Ipp32u &sliceNum, bool isLong, bool isLastSlice);

    void GetPicParamVABuffer(DXVA_Intel_PicParams_HEVC **ppPicParam, Ipp32s headerSize);

    void GetSliceVABuffers(
        DXVA_Intel_Slice_HEVC_Long **ppSliceHeader, Ipp32s headerSize,
        void **ppSliceData, Ipp32s dataSize,
        Ipp32s dataAlignment);

    void GetIQMVABuffer(DXVA_Intel_Qmatrix_HEVC **, Ipp32s bffrSize);

    void ExecuteBuffers();

protected:
    void AddReferenceFrame(DXVA_Intel_PicParams_HEVC * pPicParams_H265, Ipp32s &pos,
        H265DecoderFrame * pFrame, Ipp32s reference);

    void PackSliceGroups(DXVA_Intel_PicParams_HEVC * pPicParams_H265, H265DecoderFrame * frame);

    void SendPAVPStructure(Ipp32s numSlicesOfPrevField, H265Slice *pSlice);
    //check correctness of encrypted data
    void CheckData();
    //pointer to the beginning of offset for next slice in HW buffer
    Ipp8u * pBuf;

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

    virtual void PackSliceParams(H265Slice *pSlice, Ipp32u &sliceNum, bool isLong, bool isLastSlice);
};

#endif // UMC_VA_DXVA


#if 0 //def UMC_VA_LINUX

class PackerVA : public Packer
{
public:

private:
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
