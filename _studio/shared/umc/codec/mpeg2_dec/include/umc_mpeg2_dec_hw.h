//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

//  MPEG-2 is a international standard promoted by ISO/IEC and
//  other organizations. Implementations of this standard, or the standard
//  enabled platforms may require licenses from various entities, including
//  Intel Corporation.

#pragma once

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include "umc_mpeg2_dec_base.h"

#ifdef UMC_VA_LINUX
//LibVA headers
#   include <va/va.h>
#endif

//UMC headers
#include "umc_structures.h"
#include "umc_media_data_ex.h"
//MPEG-2
#include "umc_va_base.h"

#ifdef UMC_VA_DXVA
#include "umc_va_dxva2_protected.h"
#endif

#if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)


#define VA_NO       UNKNOWN
#define VA_VLD_W    MPEG2_VLD
#define VA_VLD_L    MPEG2_VLD

namespace UMC
{

class PackVA
{
public:
    PackVA()
        : va_mode(VA_NO)
        , m_va(NULL)
#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
        , bTriggerGPUHang(false)
#endif
        , totalNumCoef(0)
        , pBitsreamData(NULL)
        , va_index(0)
        , field_buffer_index(0)
        , pSliceStart(NULL)
        , bs_size(0)
        , bs_size_getting(0)
        , slice_size_getting(0)
        , m_SliceNum(0)
        , IsProtectedBS(false)
        , bs(NULL)
        , add_to_slice_start(0)
        , is_bs_aligned_16(false)
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
        , curr_encryptedData(NULL)
        , curr_bs_encryptedData(NULL)
#endif
        , overlap(0)
        , vpPictureParam(NULL)
        , pMBControl0(NULL)
    #ifdef UMC_VA_DXVA
        , pResidual(NULL)
    #endif
        , pSliceInfo(NULL)
        , pSliceInfoBuffer(NULL)
        , pQmatrixData(NULL)
        , QmatrixData()
    {
        memset(init_count, 0, sizeof(init_count));
        memset(add_bytes,0,16);
    };
    virtual ~PackVA();
    bool SetVideoAccelerator(VideoAccelerator * va);
    Status InitBuffers(int size_bf = 0, int size_sl = 0);
    Status SetBufferSize(
        Ipp32s          numMB,
        int             size_bs=0,
        int             size_sl=0);
    Status SaveVLDParameters(
        sSequenceHeader*    sequenceHeader,
        sPictureHeader*     PictureHeader,
        Ipp8u*              bs_start_ptr,
        sFrameBuffer*       frame_buffer,
        Ipp32s              task_num,
        Ipp32s              source_mb_height = 0);

#if defined(UMC_VA_DXVA)
    Status GetStatusReport(DXVA_Status_VC1 *pStatusReport);
#endif

    VideoAccelerationProfile va_mode;
    VideoAccelerator         *m_va;

#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
    bool   bTriggerGPUHang;
#endif

    Ipp32s totalNumCoef;
    Ipp8u  *pBitsreamData;
    Ipp32s va_index;
    Ipp32s field_buffer_index;
    Ipp8u  *pSliceStart;
    Ipp32s bs_size;
    Ipp32s bs_size_getting;
    Ipp32s slice_size_getting;
    Ipp32s m_SliceNum;
    //protected content:
    Ipp8u  add_bytes[16];
    bool   IsProtectedBS;
    Ipp32u init_count[4];
    mfxBitstream * bs;
    Ipp32s add_to_slice_start;
    bool   is_bs_aligned_16;
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    mfxEncryptedData * curr_encryptedData;
    mfxEncryptedData * curr_bs_encryptedData;
#endif
    Ipp32u overlap;

#ifdef UMC_VA_DXVA
    DXVA_PictureParameters  *vpPictureParam;
    void                    *pMBControl0;
    DXVA_TCoefSingle        *pResidual;
    DXVA_SliceInfo          *pSliceInfo;
    DXVA_SliceInfo          *pSliceInfoBuffer;
    DXVA_QmatrixData        *pQmatrixData;
    DXVA_QmatrixData        QmatrixData;
#elif defined UMC_VA_LINUX
    VAPictureParameterBufferMPEG2   *vpPictureParam;
    VAMacroblockParameterBufferMPEG2 *pMBControl0;
    VASliceParameterBufferMPEG2     *pSliceInfo;
    VASliceParameterBufferMPEG2     *pSliceInfoBuffer;
    VAIQMatrixBufferMPEG2           *pQmatrixData;
    VAIQMatrixBufferMPEG2           QmatrixData;
#endif
};

#define pack_l      pack_w

    class MPEG2VideoDecoderHW : public MPEG2VideoDecoderBase
    {
    public:
        PackVA pack_w;

        virtual Status Init(BaseCodecParams *init);

        Status SaveDecoderState();
        Status RestoreDecoderState();
        Status RestoreDecoderStateAndRemoveLastField();
        Status PostProcessFrame(int display_index, int task_num);

    protected:
        //The purpose of protected interface to have controlled
        //code in derived UMC MPEG2 decoder classes

        ///////////////////////////////////////////////////////
        /////////////Level 1 protected interface///////////////
        ///////////////////////////////////////////////////////
        //Level 1 can call level 2 functions or re-implement it

        //Slice decode, includes MB decode
        virtual Status DecodeSlice(VideoContext *video, int task_num);

        ///////////////////////////////////////////////////////
        /////////////Level 3 protected interface///////////////
        ///////////////////////////////////////////////////////
        //Level 3 can call level 4 functions or re-implement it

        //Slice Header decode
        virtual Status DecodeSliceHeader(VideoContext *video, int task_num);

        virtual Status ProcessRestFrame(int task_num);
        virtual void           quant_matrix_extension(int task_num);
        virtual Status UpdateFrameBuffer(int task_num, Ipp8u* iqm, Ipp8u*niqm);

protected:
         Status BeginVAFrame(int task_num);
    };
}
#pragma warning(default: 4324)

#endif // #if defined(UMC_VA_DXVA) || defined(UMC_VA_LINUX)


#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
