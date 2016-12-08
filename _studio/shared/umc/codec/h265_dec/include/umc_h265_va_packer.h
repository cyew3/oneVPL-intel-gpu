//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_VA_PACKER_H
#define __UMC_H265_VA_PACKER_H

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA
#include "umc_hevc_ddi.h"
#endif

#if defined(UMC_VA_LINUX)
#include <va/va_dec_hevc.h>
#endif

#include "umc_h265_tables.h"

namespace UMC_HEVC_DECODER
{

enum
{
    NO_REFERENCE = 0,
    SHORT_TERM_REFERENCE = 1,
    LONG_TERM_REFERENCE = 2,
    INTERVIEW_TERM_REFERENCE = 3
};

extern int const s_quantTSDefault4x4[16];
extern int const s_quantIntraDefault8x8[64];
extern int const s_quantInterDefault8x8[64];
extern Ipp16u const* SL_tab_up_right[];

inline
int const* getDefaultScalingList(unsigned sizeId, unsigned listId)
{
    const int *src = 0;
    switch (sizeId)
    {
    case SCALING_LIST_4x4:
        src = (int*)g_quantTSDefault4x4;
        break;
    case SCALING_LIST_8x8:
        src = (listId < 3) ? s_quantIntraDefault8x8 : s_quantInterDefault8x8;
        break;
    case SCALING_LIST_16x16:
        src = (listId < 3) ? s_quantIntraDefault8x8 : s_quantInterDefault8x8;
        break;
    case SCALING_LIST_32x32:
        src = (listId < 1) ? s_quantIntraDefault8x8 : s_quantInterDefault8x8;
        break;
    default:
        VM_ASSERT(0);
        src = NULL;
        break;
    }

    return src;
}

template <int COUNT>
inline
void initQMatrix(const H265ScalingList *scalingList, int sizeId, unsigned char qm[6][COUNT], bool force_upright_scan = false)
{
    /*        n*m    listId
        --------------------
        Intra   Y       0
        Intra   Cb      1
        Intra   Cr      2
        Inter   Y       3
        Inter   Cb      4
        Inter   Cr      5         */

    Ipp16u const* scan = 0;
    if (force_upright_scan)
        scan = SL_tab_up_right[sizeId];

    for (int n = 0; n < 6; n++)
    {
        const int *src = scalingList->getScalingListAddress(sizeId, n);
        for (int i = 0; i < COUNT; i++)  // coef.
        {
            int const idx = scan ? scan[i] : i;
            qm[n][i] = (unsigned char)src[idx];
        }
    }
}

template<int COUNT>
inline
void initQMatrix(const H265ScalingList *scalingList, int sizeId, unsigned char qm[3][2][COUNT])
{
    for(int comp=0 ; comp <= 2 ; comp++)    // Y Cb Cr
    {
        for(int n=0; n <= 1;n++)
        {
            int listId = comp + 3*n;
            const int *src = scalingList->getScalingListAddress(sizeId, listId);
            for(int i=0;i < COUNT;i++)  // coef.
                qm[comp][n][i] = (unsigned char)src[i];
        }
    }
}

inline
void initQMatrix(const H265ScalingList *scalingList, int sizeId, unsigned char qm[2][64], bool force_upright_scan = false)
{
    /*      n      m     listId
        --------------------
        Intra   Y       0
        Inter   Y       1         */

    Ipp16u const* scan = 0;
    if (force_upright_scan)
        scan = SL_tab_up_right[sizeId];

    for(int n=0;n < 2;n++)  // Intra, Inter
    {
        const int *src = scalingList->getScalingListAddress(sizeId, n);

        for (int i = 0; i < 64; i++)  // coef.
        {
            int const idx = scan ? scan[i] : i;
            qm[n][i] = (unsigned char)src[idx];
        }
    }
}

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
    virtual UMC::Status SyncTask(Ipp32s index, void * error) = 0;

    virtual void BeginFrame(H265DecoderFrame*) = 0;
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

#if defined(UMC_VA_LINUX)

class PackerVA : public Packer
{
public:
    PackerVA(UMC::VideoAccelerator * va);

    virtual UMC::Status GetStatusReport(void * pStatusReport, size_t size);
    virtual UMC::Status SyncTask(Ipp32s index, void * error) { return m_va->SyncTask(index, error); }

    virtual void PackQmatrix(const H265Slice *pSlice);

    virtual void PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 * supplier);

    virtual bool PackSliceParams(H265Slice *pSlice, Ipp32u &sliceNum, bool isLastSlice);

    virtual void BeginFrame(H265DecoderFrame*);
    virtual void EndFrame();

    virtual void PackAU(const H265DecoderFrame *pCurrentFrame, TaskSupplier_H265 * supplier);

private:

    void CreateSliceDataBuffer(H265DecoderFrameInfo * sliceInfo);
    void CreateSliceParamBuffer(H265DecoderFrameInfo * sliceInfo);

};

#ifndef MFX_PROTECTED_FEATURE_DISABLE
class PackerVA_Widevine: public PackerVA
{
public:
    PackerVA_Widevine(UMC::VideoAccelerator * va);

    virtual void PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 * supplier);

    virtual void PackAU(const H265DecoderFrame *pCurrentFrame, TaskSupplier_H265 * supplier);
};
#endif

#endif // UMC_VA_LINUX

inline bool IsVLDProfile (Ipp32s profile)
{
    return (profile & UMC::VA_VLD) != 0;
}

} // namespace UMC_HEVC_DECODER

#endif /* __UMC_H265_VA_PACKER_H */
#endif // UMC_ENABLE_H265_VIDEO_DECODER
