/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#include "ts_encoder.h"
#include "ts_decoder.h"

#define MFX_EXTBUFF_GPU_HANG MFX_MAKEFOURCC('H','A','N','G')
typedef struct {
    mfxExtBuffer Header;
} mfxExtIntGPUHang;

namespace gpu_hang_transcoding
{

mfxFrameSurface1* GetSurface(tsVideoDecoder *dec, bool syncSurfaceFromDecoder)
{
    if(!dec->m_surf_out.size())
        g_tsStatus.check(MFX_ERR_UNKNOWN);

    mfxSyncPoint syncp = dec->m_surf_out.begin()->first;
    mfxFrameSurface1* pS = dec->m_surf_out[syncp];
    if (syncSurfaceFromDecoder)
    {
        mfxStatus res = dec->SyncOperation(dec->m_session, syncp, MFX_INFINITE);
        if (res < 0)
            g_tsStatus.check();
    }
    else
        dec->m_surf_out.erase(syncp);

    if(pS && pS->Data.Locked)
    {
        pS->Data.Locked--;
    }

    return pS;
}

class tsTranscoder: public tsVideoDecoder, public tsVideoEncoder
{
public:
    tsTranscoder(mfxU32 decoderCodecId, mfxU32 encoderCodecId, mfxU32 frame_to_hang):
        tsSession(), tsVideoDecoder(decoderCodecId), tsVideoEncoder(encoderCodecId),
        m_frames_submitted(0), m_frame_to_hang(frame_to_hang)
    {
        m_trigger.AddExtBuffer(MFX_EXTBUFF_GPU_HANG, sizeof(mfxExtIntGPUHang));
    }
    using tsSurfacePool::AllocSurfaces;

    virtual mfxStatus DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
    {
        if (m_frames_submitted == m_frame_to_hang)
        {
            surface_work->Data.NumExtParam = m_trigger.NumExtParam;
            surface_work->Data.ExtParam    = m_trigger.ExtParam;

            g_tsLog << "Frame #" << m_frames_submitted << " - Fire GPU hang trigger\n";
        }

        mfxStatus sts = tsVideoDecoder::DecodeFrameAsync(session, bs, surface_work, surface_out, syncp);

        if (m_frames_submitted == m_frame_to_hang)
        {
            surface_work->Data.NumExtParam = 0;
            surface_work->Data.ExtParam    = NULL;
        }
        return sts;
    }

    tsExtBufType<mfxFrameData> m_trigger;
    mfxU32 m_frames_submitted;
    mfxU32 m_frame_to_hang;
};

int gpu_hang_transcoding_test(mfxU32 decoderCodecId, mfxU32 encoderCodecId, const char* streamName)
{
    TS_START;

    unsigned async = 1;

    const char* streamNameFull = g_tsStreamPool.Get(streamName);
    g_tsStreamPool.Reg();

    mfxU32 frame_to_hung = 0;
    tsTranscoder transcoder(decoderCodecId, encoderCodecId, frame_to_hung);

    tsBitstreamReader reader(streamNameFull, 100000);
    tsVideoDecoder *dec = static_cast<tsVideoDecoder*>(&transcoder);
    tsVideoEncoder *enc = static_cast<tsVideoEncoder*>(&transcoder);
    //tsBitstreamWriter writer("gpu_hang.mpg");
    dec->m_bs_processor = &reader;
    //enc->m_bs_processor = &writer;

    dec->DecodeHeader();
    enc->m_par.mfx.FrameInfo.CropW  = dec->m_par.mfx.FrameInfo.CropW;
    enc->m_par.mfx.FrameInfo.CropH  = dec->m_par.mfx.FrameInfo.CropH;
    enc->m_par.mfx.FrameInfo.Width  = dec->m_par.mfx.FrameInfo.Width  = (dec->m_par.mfx.FrameInfo.Width  + 31) & ~0x1f;
    enc->m_par.mfx.FrameInfo.Height = dec->m_par.mfx.FrameInfo.Height = (dec->m_par.mfx.FrameInfo.Height + 31) & ~0x1f;

    dec->QueryIOSurf();
    enc->QueryIOSurf();

    mfxFrameAllocRequest allocRequest = dec->m_request;
    allocRequest.NumFrameSuggested += enc->m_request.NumFrameMin + async;
    allocRequest.NumFrameMin = allocRequest.NumFrameSuggested;

    transcoder.AllocSurfaces(allocRequest, true);
    if(!enc->m_bitstream.MaxLength)
    {
        enc->AllocBitstream(); // Encoder initialization also here
        TS_CHECK_MFX;
    }

    mfxU32 decoded = 0, encoded = 0;
    mfxU32 submitted = 0;
    mfxStatus res = MFX_ERR_NONE;
    dec->m_par.AsyncDepth = enc->m_par.AsyncDepth = 1;

    while(true)
    {
        g_tsLog << decoded << " FRAMES DECODED\n";
        g_tsLog << encoded << " FRAMES ENCODED\n";

        res = dec->DecodeFrameAsync();
        if (res == MFX_ERR_GPU_HANG)
        {
            g_tsLog << "GPU hang reported";
            return 0;
        }

        if(MFX_ERR_MORE_DATA == res)
        {
            if(dec->m_pBitstream)
                continue;

            // bitstream finished here, get cached frames
            while(true)
            {
                mfxFrameSurface1* decodedSurface = NULL;
                if (dec->m_surf_out.size())
                {
                    decodedSurface = GetSurface(dec, false);
                    decoded++;
                }
                enc->EncodeFrameAsync(enc->m_session, decodedSurface ? enc->m_pCtrl : 0, decodedSurface, enc->m_pBitstream, enc->m_pSyncPoint);
                if (MFX_ERR_MORE_DATA == g_tsStatus.get())
                {
                    if(dec->m_surf_out.size())
                        continue;
                    else
                        break;
                }

                g_tsStatus.check();
                mfxSyncPoint sp = enc->m_syncpoint;
                enc->SyncOperation();
                if (g_tsStatus.get() == MFX_ERR_GPU_HANG)
                {
                    g_tsLog << "GPU hang reported";
                    return 0;
                }
                g_tsStatus.check();
                encoded++;
            }
            break;
        }

        if(res < 0 && res != MFX_ERR_MORE_SURFACE) g_tsStatus.check();

        transcoder.m_frames_submitted++;

        if(MFX_ERR_MORE_SURFACE == res || (res > 0 && *dec->m_pSyncPoint == NULL))
            continue;

//      if(++submitted >= async)
        {
            while(dec->m_surf_out.size())
            {
                mfxFrameSurface1* decodedSurface = GetSurface(dec, false);
                decoded++;
                enc->EncodeFrameAsync(enc->m_session, decodedSurface ? enc->m_pCtrl : 0, decodedSurface, enc->m_pBitstream, enc->m_pSyncPoint);
                if(MFX_ERR_MORE_DATA == g_tsStatus.get())
                    continue;

                g_tsStatus.check();
                mfxSyncPoint sp = enc->m_syncpoint;
                enc->SyncOperation();
                if (g_tsStatus.get() == MFX_ERR_GPU_HANG)
                {
                    g_tsLog << "GPU hang reported";
                    return 0;
                }
                g_tsStatus.check();
                encoded++;
            }
            submitted = 0;
        }
    }

    g_tsLog << decoded << " FRAMES DECODED\n";
    g_tsLog << encoded << " FRAMES ENCODED\n";

    g_tsLog << "ERROR: GPU hang not reported\n";
    return 1;

    TS_END;
}

int mpeg2_to_mpeg2 (unsigned int) { return gpu_hang_transcoding_test(MFX_CODEC_MPEG2, MFX_CODEC_MPEG2, "forBehaviorTest/customer_issues/mpeg2_43-169_trim1.mpg"); }
int avc_to_avc     (unsigned int) { return gpu_hang_transcoding_test(MFX_CODEC_AVC  , MFX_CODEC_AVC  , "conformance/h264/baseline_ext/aud_mw_e.264"); }

TS_REG_TEST_SUITE(gpu_hang_transcoding_mpeg2_to_mpeg2, mpeg2_to_mpeg2, 1);
TS_REG_TEST_SUITE(gpu_hang_transcoding_avc_to_avc    , avc_to_avc,     1);

}
