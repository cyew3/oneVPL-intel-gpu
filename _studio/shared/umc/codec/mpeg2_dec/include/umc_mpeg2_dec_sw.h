//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2017 Intel Corporation. All Rights Reserved.
//

//  MPEG-2 is a international standard promoted by ISO/IEC and
//  other organizations. Implementations of this standard, or the standard
//  enabled platforms may require licenses from various entities, including
//  Intel Corporation.

#pragma once

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include "umc_mpeg2_dec_defs_sw.h"
#include "umc_mpeg2_dec_base.h"

#ifdef UMC_VA_LINUX
//LibVA headers
#   include <va/va.h>
#endif

//IPP headers
#include "ipps.h"
#include "ippi.h"
#include "ippvc.h"

//VM headers
#include "vm_debug.h"
#include "vm_thread.h"
#include "vm_event.h"

//UMC headers
#include "umc_structures.h"
#include "umc_video_decoder.h"
#include "umc_media_data_ex.h"
#include "umc_cyclic_buffer.h"
//MPEG-2
#include "umc_mpeg2_dec_bstream.h"
#include "umc_mpeg2_dec_defs.h"
#include "umc_mpeg2_dec.h"

#include "mfxstructures.h"
//#define OTLAD


struct DecodeSpec
{
//Block
DECLALIGN(16)
    DecodeIntraSpec_MPEG2 decodeIntraSpec;
DECLALIGN(8)
    DecodeInterSpec_MPEG2 decodeInterSpec;
DECLALIGN(8)
    DecodeIntraSpec_MPEG2 decodeIntraSpecChroma;
DECLALIGN(8)
    DecodeInterSpec_MPEG2 decodeInterSpecChroma;

Ipp32s      flag;
};

#if defined (__ICL)
//non-pointer conversion from "unsigned __int64" to "Ipp32s={signed int}" may lose significant bits
//#pragma warning(disable:2259)
void ownvc_Average8x16_8u_C1R(const Ipp8u *pSrc, Ipp32s srcStep, Ipp8u *pDst, Ipp32s dstStep);
void ownvc_Average8x16HP_HF0_8u_C1R(const Ipp8u *pSrc, Ipp32s srcStep, Ipp8u *pDst, Ipp32s dstStep);
void ownvc_Average8x16HP_FH0_8u_C1R(const Ipp8u *pSrc, Ipp32s srcStep, Ipp8u *pDst, Ipp32s dstStep);
void ownvc_Average8x16HP_HH0_8u_C1R(const Ipp8u *pSrc, Ipp32s srcStep, Ipp8u *pDst, Ipp32s dstStep);
template<class TSrc, class TDst>
void GeneralCopy(TSrc* src, Ipp32u srcStep, TDst* dst, Ipp32u dstStep, IppiSize roi);
#endif

namespace UMC
{

class MPEG2VideoDecoderSW : public MPEG2VideoDecoderBase
{
public:
    ///////////////////////////////////////////////////////
    /////////////High Level public interface///////////////
    ///////////////////////////////////////////////////////
    // Default constructor
    MPEG2VideoDecoderSW(void);

    // Default destructor
    virtual ~MPEG2VideoDecoderSW(void);

    void SetOutputPointers(MediaData *output, int task_num);


protected:
    DecodeSpec*               m_Spec;
    void*                     m_Spec_raw_memory_ptr;

    mp2_VLCTable              vlcMBAdressing;
    mp2_VLCTable              vlcMBType[3];
    mp2_VLCTable              vlcMBPattern;
    mp2_VLCTable              vlcMotionVector;

    //Slice decode, includes MB decode
    virtual Status DecodeSlice(VideoContext *video, int task_num);

    //Slice Header decode
    virtual Status DecodeSliceHeader(VideoContext *video, int task_num);

    virtual bool InitTables();

    virtual void DeleteHuffmanTables();

    Status DecodeSlice_MPEG1(VideoContext *video, int task_num);


 protected:
     virtual Status          ThreadingSetup(Ipp32s maxThreads);
     virtual Status UpdateFrameBuffer(int task_num, Ipp8u* iqm, Ipp8u*niqm);
     virtual void OnDecodePicHeaderEx(int task_num);
     virtual Status ProcessRestFrame(int task_num);   
     virtual void   quant_matrix_extension(int task_num);

    Status                  DecodeSlice_FrameI_420(VideoContext *video, int task_num);
    Status                  DecodeSlice_FrameI_422(VideoContext *video, int task_num);
    Status                  DecodeSlice_FramePB_420(VideoContext *video, int task_num);
    Status                  DecodeSlice_FramePB_422(VideoContext *video, int task_num);
    Status                  DecodeSlice_FieldPB_420(VideoContext *video, int task_num);
    Status                  DecodeSlice_FieldPB_422(VideoContext *video, int task_num);

    Status                  mv_decode(Ipp32s r, Ipp32s s, VideoContext *video, int task_num);
    Status                  mv_decode_dp(VideoContext *video, int task_num);
    Status                  update_mv(Ipp16s *pval, Ipp32s s, VideoContext *video, int task_num);

    Status                  mc_frame_forward_420(VideoContext *video, int task_num);
    Status                  mc_frame_forward_422(VideoContext *video, int task_num);
    Status                  mc_field_forward_420(VideoContext *video, int task_num);
    Status                  mc_field_forward_422(VideoContext *video, int task_num);

    Status                  mc_frame_backward_420(VideoContext *video, int task_num);
    Status                  mc_frame_backward_422(VideoContext *video, int task_num);
    Status                  mc_field_backward_420(VideoContext *video, int task_num);
    Status                  mc_field_backward_422(VideoContext *video, int task_num);

    Status                  mc_frame_backward_add_420(VideoContext *video, int task_num);
    Status                  mc_frame_backward_add_422(VideoContext *video, int task_num);
    Status                  mc_field_backward_add_420(VideoContext *video, int task_num);
    Status                  mc_field_backward_add_422(VideoContext *video, int task_num);

    Status                  mc_fullpel_forward(VideoContext *video, int task_num);
    Status                  mc_fullpel_backward(VideoContext *video, int task_num);
    Status                  mc_fullpel_backward_add(VideoContext *video, int task_num);

    void                    mc_frame_forward0_420(VideoContext *video, int task_num);
    void                    mc_frame_forward0_422(VideoContext *video, int task_num);
    void                    mc_field_forward0_420(VideoContext *video, int task_num);
    void                    mc_field_forward0_422(VideoContext *video, int task_num);

    Status                  mc_dualprime_frame_420(VideoContext *video, int task_num);
    Status                  mc_dualprime_frame_422(VideoContext *video, int task_num);
    Status                  mc_dualprime_field_420(VideoContext *video, int task_num);
    Status                  mc_dualprime_field_422(VideoContext *video, int task_num);

    Status                  mc_mp2_420b_skip(VideoContext *video, int task_num);
    Status                  mc_mp2_422b_skip(VideoContext *video, int task_num);
    Status                  mc_mp2_420_skip(VideoContext *video, int task_num);
    Status                  mc_mp2_422_skip(VideoContext *video, int task_num);

private:
    MPEG2VideoDecoderSW(MPEG2VideoDecoderSW const&);
    MPEG2VideoDecoderSW& operator=(MPEG2VideoDecoderSW const&);

};
}
#pragma warning(default: 4324)

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
