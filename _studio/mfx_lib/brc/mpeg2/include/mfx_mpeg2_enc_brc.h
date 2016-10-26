//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_BRC)

#ifndef _MFX_MPEG2_ENC_BRC_H_
#define _MFX_MPEG2_ENC_BRC_H_

#include "mfxvideo++int.h"
#include "mfxvideo.h"
#include "mfx_umc_alloc_wrapper.h"

enum
{
  MPEG2_BRC_OK                      = 0x0,
  MPEG2_BRC_ERR_BIG_FRAME           = 0x1,
  MPEG2_BRC_BIG_FRAME               = 0x2,
  MPEG2_BRC_ERR_SMALL_FRAME         = 0x4,
  MPEG2_BRC_SMALL_FRAME             = 0x8,
  MPEG2_BRC_NOT_ENOUGH_BUFFER       = 0x10
};


typedef struct sHRDState_MPEG2
{
  mfxI32 bufSize;
  mfxI32 bufFullness;
  mfxI32 prevBufFullness;
  mfxU32 maxBitrate;
  mfxI32 inputBitsPerFrame;
  mfxI32 maxInputBitsPerFrame;
  mfxU16 frameCnt;
  mfxU16 prevFrameCnt;
  mfxI32 minFrameSize;
  mfxI32 maxFrameSize;
} HRDState_MPEG2;


class MFXVideoRcMpeg2 : public VideoBRC
{
//    UMC_MPEG2_ENCODER::MPEG2_AVBR* m_pBitRateControl;
//    UMC_MPEG2_ENCODER::EnumPicCodType m_CurPicType;
    Ipp8u*                      m_pBRCBuffer;
    mfxHDL                      m_BRCID;
    VideoCORE*                  m_pMFXCore;

public:
    static mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);

    MFXVideoRcMpeg2(VideoCORE *core, mfxStatus *status) : VideoBRC(), mHRD()
    {
        m_pMFXCore = core;
//        m_pBitRateControl = NULL;
        m_pBRCBuffer = NULL;
        m_BRCID      = 0;
        *status = MFX_ERR_NONE;
        m_totalBitsEncoded = 0;
        rc_ip_delay = 0;
        mPrevQScaleValue = 0;
        mBitrate = 0;
        IPDistance = 0;
        mRCMode = 0;
        q_scale_type = 0;
        VBV_BufferSize = 0;
        mIsField = 0;
        rc_vbv_fullness = 0;
        mPrevDataLength = 0;
        mPrevQScaleType = 0;
        rc_dev = 0;
        mQuantUpdated = 0;
        gopSize = 0;
        rc_delay = 0;
        quantiser_scale_code = 0;
        mPrevQScaleCode = 0;
        rc_ave_frame_bits = 0;
        intra_dc_precision = 0;
        m_FirstFrame = 0;
        block_count = 0;
        mFramerate = 0;
        FieldPicture = 0;
        mBitsEncoded = 0;
        quantiser_scale_value = 0;
        mIndx = 0;
    }
    virtual ~MFXVideoRcMpeg2(void);
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus FrameENCUpdate(mfxFrameCUC *cuc);
    virtual mfxStatus FramePAKRefine(mfxFrameCUC *cuc);
    virtual mfxStatus FramePAKRecord(mfxFrameCUC *cuc);

/*
    virtual mfxStatus SliceEncUpdate(mfxFrameCUC *cuc);
    virtual mfxStatus SlicePakRefine(mfxFrameCUC *cuc);
    virtual mfxStatus SlicePakRecord(mfxFrameCUC *cuc);
*/

protected:
    mfxStatus SetUniformQPs(mfxFrameCUC *cuc, mfxI32 q_scale_type, mfxI32 quantiser_scale_code);
    mfxI32 changeQuant(mfxI32 quant_value);
    mfxStatus InitHRD(mfxVideoParam *par);
    mfxI32 UpdateAndCheckHRD(mfxI32 frameBits);
    void UpdateQuantHRD(mfxI32 bEncoded, mfxI32 sts);
    void UpdateQuant(mfxI32 bEncoded);

    mfxU32 m_totalBitsEncoded;

    mfxU32      mBitrate;
    mfxF64      mFramerate;
    mfxI32      mRCMode;                 // rate control mode, default RC_CBR
    mfxI32      quant_vbr[3];            // quantizers for VBR modes
    mfxI32      FieldPicture;            // field or frame picture (if prog_frame=> frame)

    mfxU32      VBV_BufferSize;
    mfxF64      rc_weight[3];          // frame weight (length proportion)
    mfxF64      rc_tagsize[3];         // bitsize target of the type
    mfxF64      rc_dev;                // bitrate deviation (sum of GOP's frame diffs)
    mfxI32      IPDistance;              // distance between reference frames
    mfxI32      gopSize;                 // size of GOP
    mfxI32      block_count;

//    mfxI32      vbv_delay;             // vbv_delay code for picture header
//    mfxI32      rc_vbv_max;            // max allowed size of the frame to be encoded in bits
//    mfxI32      rc_vbv_min;            // min allowed size of the frame to avoid overflow with next frame
    mfxF64      rc_vbv_fullness;       // buffer fullness before frame removing in bits
    mfxF64      rc_delay;              // vbv examination delay for current picture
    mfxF64      rc_ip_delay;           // extra delay for I or P pictures in bits
    mfxF64      rc_ave_frame_bits;     // bits per frame
    mfxI32      qscale[3];             // qscale codes for 3 frame types (mfxI32!)
    mfxI32      prsize[3];             // bitsize of previous frame of the type
    mfxI32      prqscale[3];           // quant scale value, used with previous frame of the type
    mfxI32      quantiser_scale_value; // for the current frame
    mfxI32      q_scale_type;          // 0 for linear 1 for nonlinear
    mfxI32      quantiser_scale_code;  // bitcode for current scale value

    mfxI32      intra_dc_precision;
    mfxI32      m_FirstFrame;

    mfxI32 mBitsEncoded;
    HRDState_MPEG2 mHRD;
    mfxU8 mQuantUpdated;
    mfxI32 mPrevQScaleCode;
    mfxI32 mPrevQScaleValue;
    mfxI32 mPrevQScaleType;
    mfxU8  mIsField;
    mfxU8  mIndx;

    mfxU32      mPrevDataLength;


};


#endif //_MFX_MPEG2_ENC_BRC_H_
#endif //MFX_ENABLE_MPEG2_VIDEO_ENCODER
