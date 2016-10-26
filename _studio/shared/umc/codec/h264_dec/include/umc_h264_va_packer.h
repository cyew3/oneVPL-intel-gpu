//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_VA_PACKER_H
#define __UMC_H264_VA_PACKER_H

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA
#include "umc_mvc_ddi.h"
#include "umc_svc_ddi.h"
#endif

#ifndef UMC_RESTRICTED_CODE_VA

#if defined(_WIN32) || defined(_WIN64)
#pragma warning( disable : 4100 ) // ignore unreferenced parameters in virtual method declaration
#endif

namespace UMC_H264_DECODER
{
    struct H264PicParamSet;
    struct H264ScalingPicParams;
}

namespace UMC
{

class H264DecoderFrame;
class H264Slice;
class H264DecoderFrameInfo;
struct ReferenceFlags;
class TaskSupplier;

class Packer
{

public:

    Packer(VideoAccelerator * va, TaskSupplier * supplier);
    virtual ~Packer();

    virtual Status GetStatusReport(void * pStatusReport, size_t size) = 0;
    virtual Status SyncTask(H264DecoderFrame*, void * error);
    virtual Status QueryTaskStatus(Ipp32s index, void * status, void * error);
    virtual Status QueryStreamOut(H264DecoderFrame*);

    VideoAccelerator * GetVideoAccelerator() { return m_va; }

    virtual void PackAU(const H264DecoderFrame*, Ipp32s isTop) = 0;

    virtual void BeginFrame(H264DecoderFrame*, Ipp32s field) = 0;
    virtual void EndFrame() = 0;

    virtual void Reset() {}

    static Packer * CreatePacker(VideoAccelerator * va, TaskSupplier* supplier);

private:

    virtual void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice) = 0;

    virtual Ipp32s PackSliceParams(H264Slice *pSlice, Ipp32s sliceNum, Ipp32s chopping, Ipp32s numSlicesOfPrevField) = 0;

    virtual void PackQmatrix(const UMC_H264_DECODER::H264ScalingPicParams * scaling) = 0;

protected:

    VideoAccelerator *m_va;
    TaskSupplier     *m_supplier;
};

#ifdef UMC_VA_DXVA

class PackerDXVA2
    : public Packer
{

public:

    PackerDXVA2(VideoAccelerator * va, TaskSupplier * supplier);

    Status GetStatusReport(void * pStatusReport, size_t size);

    void PackAU(const H264DecoderFrame*, Ipp32s isTop);

    void BeginFrame(H264DecoderFrame*, Ipp32s field);
    void EndFrame();

private:

    void PackQmatrix(const UMC_H264_DECODER::H264ScalingPicParams * scaling);

    void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice);

    Ipp32s PackSliceParams(H264Slice *pSlice, Ipp32s sliceNum, Ipp32s chopping, Ipp32s numSlicesOfPrevField);

protected:

    void AddReferenceFrame(DXVA_PicParams_H264 * pPicParams_H264, Ipp32s &pos, H264DecoderFrame * pFrame, Ipp32s reference);
    DXVA_PicEntry_H264 GetFrameIndex(const H264DecoderFrame * frame);

    void PackSliceGroups(DXVA_PicParams_H264 * pPicParams_H264, H264DecoderFrame * frame);
    void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice, DXVA_PicParams_H264* pPicParams_H264);
    Ipp32s PackSliceParams(H264Slice *pSlice, Ipp32s sliceNum, Ipp32s chopping, Ipp32s numSlicesOfPrevField, DXVA_Slice_H264_Long* pDXVA_Slice_H264);

    void SendPAVPStructure(Ipp32s numSlicesOfPrevField, H264Slice *pSlice);

    void CheckData(); //check correctness of encrypted data

    virtual void PackAU(H264DecoderFrameInfo * sliceInfo, Ipp32s firstSlice, Ipp32s count);

    void PackPicParamsMVC(const H264DecoderFrameInfo * pSliceInfo, DXVA_PicParams_H264_MVC* pMVCPicParams_H264);
    void PackPicParamsMVC(const H264DecoderFrameInfo * pSliceInfo, DXVA_Intel_PicParams_MVC* pMVCPicParams_H264);

    //pointer to the beginning of offset for next slice in HW buffer
    Ipp8u * m_pBuf;
    Ipp32u  m_statusReportFeedbackCounter;

    DXVA_PicParams_H264* m_picParams;
};

class PackerDXVA2_Widevine
    : public PackerDXVA2
{

public:

    PackerDXVA2_Widevine(VideoAccelerator * va, TaskSupplier * supplier);
    void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice);

private:

    void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice, DXVA_PicParams_H264* pPicParams_H264);
    void PackAU(H264DecoderFrameInfo * sliceInfo, Ipp32s firstSlice, Ipp32s count);
};

#endif // UMC_VA_DXVA


#ifdef UMC_VA_LINUX

class PackerVA
    : public Packer
{
public:
    PackerVA(VideoAccelerator * va, TaskSupplier * supplier);

    Status GetStatusReport(void * pStatusReport, size_t size);
    Status QueryStreamOut(H264DecoderFrame*);

    void PackAU(const H264DecoderFrame*, Ipp32s isTop);

    void BeginFrame(H264DecoderFrame*, Ipp32s field);
    void EndFrame();

protected:

    void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice);

    void FillFrame(VAPictureH264 * pic, const H264DecoderFrame *pFrame,
        Ipp32s field, Ipp32s reference, Ipp32s defaultIndex);

    Ipp32s FillRefFrame(VAPictureH264 * pic, const H264DecoderFrame *pFrame,
        ReferenceFlags flags, bool isField, Ipp32s defaultIndex);

    void FillFrameAsInvalid(VAPictureH264 * pic);

    void PackProcessingInfo(H264DecoderFrameInfo * sliceInfo);

    enum
    {
        VA_FRAME_INDEX_INVALID = 0x7f
    };

protected:

    void CreateSliceParamBuffer(H264DecoderFrameInfo * pSliceInfo);
    void CreateSliceDataBuffer(H264DecoderFrameInfo * pSliceInfo);

    Ipp32s PackSliceParams(H264Slice *pSlice, Ipp32s sliceNum, Ipp32s chopping, Ipp32s numSlicesOfPrevField);

    void PackQmatrix(const UMC_H264_DECODER::H264ScalingPicParams * scaling);

private:

    bool                       m_enableStreamOut;
};

class PackerVA_PAVP
    : public PackerVA
{

public:

    PackerVA_PAVP(VideoAccelerator * va, TaskSupplier * supplier);

private:

    virtual void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice);

    virtual void CreateSliceDataBuffer(H264DecoderFrameInfo * pSliceInfo);

    virtual Ipp32s PackSliceParams(H264Slice *pSlice, Ipp32s sliceNum, Ipp32s chopping, Ipp32s numSlicesOfPrevField);

protected:

    void PackPavpParams();
};

class PackerVA_Widevine
    : public PackerVA
{

public:

    PackerVA_Widevine(VideoAccelerator * va, TaskSupplier * supplier);

private:

    virtual void PackPicParams(H264DecoderFrameInfo * pSliceInfo, H264Slice * pSlice);
    virtual void PackAU(const H264DecoderFrame *pCurrentFrame, Ipp32s isTop);
};

#endif // UMC_VA_LINUX

inline bool IsVLDProfile (Ipp32s profile)
{
    return (profile & VA_VLD) != 0;
}

} // namespace UMC

#endif // UMC_RESTRICTED_CODE_VA
#endif /* __UMC_H264_VA_PACKER_H */
#endif // UMC_ENABLE_H264_VIDEO_DECODER
