/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012 Intel Corporation. All Rights Reserved.
//
*/

#ifndef _MFX_VP8_DECODE_HW_H_
#define _MFX_VP8_DECODE_HW_H_

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP8_VIDEO_DECODE) && defined(MFX_VA)
//#define __FAKE_VP8_HW

#include "mfx_common_int.h"
#include "mfx_umc_alloc_wrapper.h"

#include "vp8_dec_defs.h"

#include "umc_mutex.h"
#include "umc_vp8_decoder.h"
#include "umc_vp8_mfx_decode_hw.h"

#include <map>


class MFX_VP8_BoolDecoder
{
private:
    Ipp32u m_range;
    Ipp32u m_value;
    Ipp32s m_bitcount;
    Ipp8u *m_input;
    Ipp32s m_input_size;

    static const int range_normalization_shift[64];

    int decode_bit(int prob)
    {
        Ipp32u split, split256;
        int ret;

        split    = 1 + (((m_range - 1) * (prob)) >> 8);
        split256 = (split << 8);

        if (m_value >= split256)
        { 
            m_range -= split; 
            m_value -= split256; 
            ret    = 1; 
        } 
        else 
        { 
            m_range = split; 
            ret   = 0; 
        }

        if (m_range < 128) 
        { 
            Ipp32u shift = range_normalization_shift[m_range >> 1]; 
            m_range <<= shift; 
            m_value <<= shift; 
            m_bitcount += shift; 
            if (m_bitcount >= 8) 
            { 
                m_bitcount -= 8; 
                m_value    |= *(m_input) << m_bitcount; 
                m_input++; 
            } 
        } 

        return ret;
    }

public:
    MFX_VP8_BoolDecoder() : 
        m_range(0),
        m_value(0),
        m_bitcount(0),
        m_input(0),
        m_input_size(0) { }

    MFX_VP8_BoolDecoder(Ipp8u *pBitStream, Ipp32s dataSize)
    {
        init(pBitStream, dataSize);
    }

    void init(Ipp8u *pBitStream, Ipp32s dataSize)
    {
        dataSize = IPP_MIN(dataSize, 2);

        m_input     = pBitStream;
        m_input_size = dataSize;
        m_range     = 255;
        m_bitcount  = 0;
        m_value     = 0;

        for(Ipp8u i = 0; i < dataSize; i++)
            m_value = (m_value << (8*i)) | (m_input[i]);

        m_input    += dataSize;
    }

    Ipp32u decode(int bits = 1, int prob = 128)
    {
        Ipp32u val = 0;
        for(int n=0;n < bits;n++)
        {
            int bit = decode_bit(prob);
            val = val << 1 | bit;
        }

        return val;
    }

    Ipp8u *data() const
    {
        return m_input;
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

struct sFrameInfo
{
    UMC::FrameType frameType;
    mfxU16 currIndex;
    mfxU16 goldIndex;
    mfxU16 altrefIndex;
    FrameData *frmData;
};

class VideoDECODEVP8_HW : public VideoDECODE
{
public:

    VideoDECODEVP8_HW(VideoCORE *pCore, mfxStatus *sts);

    static mfxStatus Query(VideoCORE *pCore, mfxVideoParam *pIn, mfxVideoParam *pOut);
    static mfxStatus QueryIOSurf(VideoCORE *pCore, mfxVideoParam *pPar, mfxFrameAllocRequest *pRequest);
    static mfxStatus DecodeHeader(VideoCORE *pCore, mfxBitstream *pBs, mfxVideoParam *pPar);

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
    virtual mfxStatus GetPayload(mfxSession session, mfxU64 *pTimeStamp, mfxPayload *pPayload);
    virtual mfxStatus SetSkipMode(mfxSkipMode mode);

protected:
    void CalculateTimeSteps(mfxFrameSurface1 *);
    mfxStatus ConstructFrame(mfxBitstream *, mfxBitstream *, IVF_FRAME&);
    mfxStatus PreDecodeFrame(mfxBitstream *, mfxFrameSurface1 *);
    mfxStatus QueryIOSurfInternal(eMFXPlatform, mfxVideoParam *, mfxFrameAllocRequest *);

    mfxStatus AllocateFrame();
    mfxStatus DecodeFrameHeader(mfxBitstream *p_bistream);
    void UpdateSegmentation(MFX_VP8_BoolDecoder &);
    void UpdateLoopFilterDeltas(MFX_VP8_BoolDecoder &);
    void DecodeInitDequantization(MFX_VP8_BoolDecoder &);
    mfxStatus PackHeaders(mfxBitstream *p_bistream);

    bool CheckHardwareSupport(VideoCORE *p_core, mfxVideoParam *p_par);

    UMC::FrameMemID GetMemIdToUnlock();

    mfxStatus GetFrame(MediaData* in, FrameData** out);

private:
    bool                    m_is_initialized;
    VideoCORE*              m_p_core;
    eMFXPlatform            m_platform;

    mfxVideoParamWrapper    m_on_init_video_params,
                            m_video_params;
    mfxU32                  m_init_w,
                            m_init_h;
    mfxU32                  m_num_output_frames;
    mfxF64                  m_in_framerate;
    mfxU16                  m_frameOrder;

    vm_mutex                m_mutex;

    mfxBitstream            m_bs;
    vp8_FrameInfo           m_frame_info;
    vp8_RefreshInfo         m_refresh_info;
    vp8_FrameProbabilities  m_frameProbs;
    vp8_FrameProbabilities  m_frameProbs_saved;
    vp8_QuantInfo           m_quantInfo;
    vp8_MbInfo*             m_pMbInfo;
    Ipp8u                   m_RefFrameIndx[VP8_NUM_OF_REF_FRAMES];
    MFX_VP8_BoolDecoder     m_boolDecoder[VP8_MAX_NUMBER_OF_PARTITIONS];

    FrameData m_frame_data[VP8_NUM_OF_REF_FRAMES];
    FrameData m_last_frame_data;
    FrameData *m_p_curr_frame;
    std::map<FrameData *, bool> m_surface_list;

    mfxU16 curr_indx;
    mfxU16 gold_indx;
    mfxU16 altref_indx;
    std::vector<sFrameInfo> m_frames;

    bool m_is_software_buffer;

    std::auto_ptr<mfx_UMC_FrameAllocator> m_p_frame_allocator;
    std::auto_ptr<mfx_UMC_FrameAllocator_D3D> m_p_frame_allocator_d3d;

    mfxFrameAllocResponse   m_response;
    mfxDecodeStat           m_stat;

    //////////////////////////////////////////

    UMC::VideoAccelerator *m_p_video_accelerator;
};

#endif // _MFX_VP8_DECODE_HW_H_
#endif // MFX_ENABLE_VP8_VIDEO_DECODE && MFX_VA
/* EOF */
