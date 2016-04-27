/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.
//
*/

#ifndef _MFX_VP8_DECODE_HW_H_
#define _MFX_VP8_DECODE_HW_H_

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP8_VIDEO_DECODE_HW) && defined(MFX_VA)
//#define __FAKE_VP8_HW

#include "mfx_common_int.h"
#include "mfx_umc_alloc_wrapper.h"
#include "mfx_critical_error_handler.h"
#include "umc_frame_data.h"
#include "umc_media_buffer.h"
#include "umc_media_data.h"
#include "umc_va_base.h"

#include "mfx_task.h"

#include "umc_mutex.h"

#include "mfx_vp8_dec_decode_vp8_defs.h"
#include "mfx_vp8_dec_decode_common.h"

class MFX_VP8_BoolDecoder
{
private:
    Ipp32u m_range;
    Ipp32u m_value;
    Ipp32s m_bitcount;
    Ipp32u m_pos;
    Ipp8u *m_input;
    Ipp32s m_input_size;

    static const int range_normalization_shift[64];

    int decode_bit(int probability)
    {

        Ipp32u bit = 0;
        Ipp32u split;
        Ipp32u bigsplit;
        Ipp32u count = this->m_bitcount;
        Ipp32u range = this->m_range;
        Ipp32u value = this->m_value;

        split = 1 +  (((range - 1) * probability) >> 8);
        bigsplit = (split << 24);

        range = split;
        if(value >= bigsplit)
        {
           range = this->m_range - split;
           value = value - bigsplit;
           bit = 1;
        }

        if(range >= 0x80)
        {
            this->m_value = value;
            this->m_range = range;
            return bit;
        }
        else
        {
            do
            {
                range += range;
                value += value;

                if (!--count)
                {
                    count = 8;
                    value |= static_cast<Ipp32u>(this->m_input[this->m_pos]);
                    this->m_pos++;
                }
             }
             while(range < 0x80);
        }
        this->m_bitcount = count;
        this->m_value = value;
        this->m_range = range;
        return bit;

    }

public:
    MFX_VP8_BoolDecoder() :
        m_range(0),
        m_value(0),
        m_bitcount(0),
        m_pos(0),
        m_input(0),
        m_input_size(0)
    {}

    MFX_VP8_BoolDecoder(Ipp8u *pBitStream, Ipp32s dataSize)
    {
        init(pBitStream, dataSize);
    }

    void init(Ipp8u *pBitStream, Ipp32s dataSize)
    {
        dataSize = IPP_MIN(dataSize, 2);
        m_range = 255;
        m_bitcount = 8;
        m_pos = 0;
        m_value = (pBitStream[0] << 24) + (pBitStream[1] << 16) +
                  (pBitStream[2] << 8) + pBitStream[3];
        m_pos += 4;
        m_input     = pBitStream;
        m_input_size = dataSize;
    }

    Ipp32u decode(int bits = 1, int prob = 128)
    {
        uint32_t z = 0;
        int bit;
        for (bit = bits - 1; bit >= 0;bit--)
        {
            z |= (decode_bit(prob) << bit);
        }
        return z;
    }

    Ipp8u * input()
    {
        return &m_input[m_pos];
    }

    Ipp32u pos() const
    {
        return m_pos;
    }

    Ipp32s bitcount() const
    {
        return m_bitcount;
    }

    Ipp32u range() const
    {
        return m_range;
    }

    Ipp32u value() const
    {
        return m_value;
    }
};

static mfxStatus MFX_CDECL VP8DECODERoutine(void *p_state, void *pp_param, mfxU32 thread_number, mfxU32);

class VideoDECODEVP8_HW : public VideoDECODE, public MfxCriticalErrorHandler
{
public:

    VideoDECODEVP8_HW(VideoCORE *pCore, mfxStatus *sts);
    ~VideoDECODEVP8_HW();

    static mfxStatus Query(VideoCORE *pCore, mfxVideoParam *pIn, mfxVideoParam *pOut);
    static mfxStatus QueryIOSurf(VideoCORE *pCore, mfxVideoParam *pPar, mfxFrameAllocRequest *pRequest);

    virtual mfxStatus Init(mfxVideoParam *pPar);
    virtual mfxStatus Reset(mfxVideoParam *pPar);
    virtual mfxStatus Close();

    virtual mfxTaskThreadingPolicy GetThreadingPolicy();
    virtual mfxStatus GetVideoParam(mfxVideoParam *pPar);
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *pStat);

    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out);
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *pBs, mfxFrameSurface1 *pSurfaceWork, mfxFrameSurface1 **ppSurfaceOut, MFX_ENTRY_POINT *pEntryPoint);
    virtual mfxStatus DecodeFrame(mfxBitstream *pBs, mfxFrameSurface1 *pSurfaceWork, mfxFrameSurface1 *pSurfaceOut);

    virtual mfxStatus GetUserData(mfxU8 *pUserData, mfxU32 *pSize, mfxU64 *pTimeStamp);
    virtual mfxStatus GetPayload(mfxU64 *pTimeStamp, mfxPayload *pPayload);
    virtual mfxStatus SetSkipMode(mfxSkipMode mode);

private:

    mfxStatus ConstructFrame(mfxBitstream *, mfxBitstream *, VP8DecodeCommon::IVF_FRAME&);
    mfxStatus PreDecodeFrame(mfxBitstream *, mfxFrameSurface1 *);
    static mfxStatus QueryIOSurfInternal(mfxVideoParam *, mfxFrameAllocRequest *);

    mfxStatus DecodeFrameHeader(mfxBitstream *p_bistream);
    void UpdateSegmentation(MFX_VP8_BoolDecoder &);
    void UpdateLoopFilterDeltas(MFX_VP8_BoolDecoder &);
    void DecodeInitDequantization(MFX_VP8_BoolDecoder &);
    mfxStatus PackHeaders(mfxBitstream *p_bistream);

    static bool CheckHardwareSupport(VideoCORE *p_core, mfxVideoParam *p_par);

    UMC::FrameMemID GetMemIdToUnlock();

    mfxStatus GetFrame(UMC::MediaData* in, UMC::FrameData** out);

    friend mfxStatus MFX_CDECL VP8DECODERoutine(void *p_state, void *pp_param, mfxU32 thread_number, mfxU32);

    struct VP8DECODERoutineData
    {
        VideoDECODEVP8_HW* decoder;
        mfxFrameSurface1* surface_work;
        FrameMemID memId;
        FrameMemID memIdToUnlock;
    };

    struct sFrameInfo
    {
        UMC::FrameType frameType;
        mfxU16 currIndex;
        mfxU16 goldIndex;
        mfxU16 altrefIndex;
        mfxU16 lastrefIndex;
        UMC::FrameMemID memId;
    };

    bool                    m_is_initialized;
    VideoCORE*              m_p_core;
    eMFXPlatform            m_platform;

    mfxVideoParamWrapper    m_on_init_video_params,
                            m_video_params;
    mfxU32                  m_init_w,
                            m_init_h;
    mfxF64                  m_in_framerate;
    mfxU16                  m_frameOrder;

    mfxBitstream            m_bs;
    VP8Defs::vp8_FrameInfo  m_frame_info;
    unsigned                m_CodedCoeffTokenPartition;
    bool                    m_firstFrame;

    VP8Defs::vp8_RefreshInfo         m_refresh_info;
    VP8Defs::vp8_FrameProbabilities  m_frameProbs;
    VP8Defs::vp8_FrameProbabilities  m_frameProbs_saved;
    VP8Defs::vp8_QuantInfo           m_quantInfo;
    MFX_VP8_BoolDecoder     m_boolDecoder[VP8Defs::VP8_MAX_NUMBER_OF_PARTITIONS];

    mfxU16 gold_indx;
    mfxU16 altref_indx;
    mfxU16 lastrefIndex;

    std::vector<sFrameInfo> m_frames;

    mfxFrameAllocResponse   m_response;
    mfxDecodeStat           m_stat;
    mfxFrameAllocRequest    m_request;

    std::auto_ptr<mfx_UMC_FrameAllocator> m_p_frame_allocator;
    UMC::VideoAccelerator *m_p_video_accelerator;

};

#endif // _MFX_VP8_DECODE_HW_H_
#endif // MFX_ENABLE_VP8_VIDEO_DECODE && MFX_VA
/* EOF */
