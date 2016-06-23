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

class tsTranscoder: public tsVideoDecoder, public tsVideoEncoder
{
public:
    tsTranscoder(mfxU32 decoderCodecId, mfxU32 encoderCodecId, mfxU32 frame_to_hang):
        tsSession(MFX_IMPL_HARDWARE), tsVideoDecoder(decoderCodecId), tsVideoEncoder(encoderCodecId),
        m_frames_submitted(0), m_frame_to_hang(frame_to_hang), m_expect_gpu_hang_from_dec_frame_async(false),
        m_expect_gpu_hang_from_dec_syncop(false), m_hang_triggered(false)
    {
        m_trigger.AddExtBuffer(MFX_EXTBUFF_GPU_HANG, sizeof(mfxExtIntGPUHang));
    }
    using tsSurfacePool::AllocSurfaces;

    virtual mfxStatus DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
    {
        if (m_hang_triggered)
        {
            m_hang_triggered = false;
            m_expect_gpu_hang_from_dec_frame_async = true;
        }

        if (m_frames_submitted == m_frame_to_hang)
        {
            surface_work->Data.NumExtParam = m_trigger.NumExtParam;
            surface_work->Data.ExtParam    = m_trigger.ExtParam;

            g_tsLog << "Frame #" << m_frames_submitted << " - Fire GPU hang trigger\n";
        }

        mfxStatus sts = tsVideoDecoder::DecodeFrameAsync(session, bs, surface_work, surface_out, syncp);

        if (m_frames_submitted == m_frame_to_hang)
        {
            if (sts >= 0 || sts == MFX_ERR_MORE_SURFACE)
                m_hang_triggered = true;

            surface_work->Data.NumExtParam = 0;
            surface_work->Data.ExtParam    = NULL;
        }

        if (m_hang_triggered)
            m_expect_gpu_hang_from_dec_syncop = true;

        return sts;
    }

    tsExtBufType<mfxFrameData> m_trigger;
    mfxU32 m_frames_submitted;
    mfxU32 m_frame_to_hang;
    bool   m_expect_gpu_hang_from_dec_frame_async;
    bool   m_expect_gpu_hang_from_dec_syncop;
    bool   m_hang_triggered;
};

mfxFrameSurface1* GetSurface(tsVideoDecoder *dec, bool syncSurfaceFromDecoder)
{
    tsTranscoder *tr = dynamic_cast<tsTranscoder*> (dec);
    if (tr == NULL)
    {
        ADD_FAILURE() << "Dynamic casting to tsTranscoder* failed";
        throw tsFAIL;
    }

    if(dec->m_surf_out.size() == 0)
    {
        ADD_FAILURE() << "No frame to synchronize";
        throw tsFAIL;
    }

    mfxSyncPoint syncp = dec->m_surf_out.begin()->first;
    mfxFrameSurface1* pS = dec->m_surf_out[syncp];
    if (syncSurfaceFromDecoder)
    {
        if (tr->m_expect_gpu_hang_from_dec_syncop)
            g_tsStatus.expect(MFX_ERR_GPU_HANG);

        dec->SyncOperation(dec->m_session, syncp, MFX_INFINITE);
    }
    else
        dec->m_surf_out.erase(syncp);

    if(pS && pS->Data.Locked)
    {
        pS->Data.Locked--;
    }

    return pS;
}

int gpu_hang_transcoding_test(mfxU32 decoderCodecId, mfxU32 encoderCodecId, const char* streamName)
{
    TS_START;

    unsigned async = 1;

    const char* streamNameFull = g_tsStreamPool.Get(streamName);
    g_tsStreamPool.Reg();

    mfxU32 frame_to_hung = 0;
    tsTranscoder transcoder(decoderCodecId, encoderCodecId, frame_to_hung);
    transcoder.MFXInit();
    transcoder.SetAllocator(transcoder.m_pVAHandle, true);
    transcoder.m_pFrameAllocator = transcoder.m_pVAHandle;
    transcoder.SetFrameAllocator();

    tsBitstreamReader reader(streamNameFull, 100000);
    tsVideoDecoder *dec = static_cast<tsVideoDecoder*>(&transcoder);
    tsVideoEncoder *enc = static_cast<tsVideoEncoder*>(&transcoder);
    dec->m_bs_processor = &reader;
    dec->DecodeHeader();

    dec->m_par.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    enc->m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    enc->m_par.mfx.FrameInfo = dec->m_par.mfx.FrameInfo;
    enc->m_par.mfx.FrameInfo.FrameRateExtN = 30;
    enc->m_par.mfx.FrameInfo.FrameRateExtD = 1;
    enc->Query();
    dec->m_par.mfx.FrameInfo.Width  = enc->m_par.mfx.FrameInfo.Width;
    dec->m_par.mfx.FrameInfo.Height = enc->m_par.mfx.FrameInfo.Height;

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
    mfxStatus decFrameAsyncStatus = MFX_ERR_NONE;

    while(true)
    {
        g_tsLog << decoded << " FRAMES DECODED\n";
        g_tsLog << encoded << " FRAMES ENCODED\n";

        g_tsStatus.m_status = decFrameAsyncStatus; // g_tsStatus treated as previous result from DecFrameAsync in dec->DecodeFrameAsync() call below
        decFrameAsyncStatus = dec->DecodeFrameAsync();
        if (decFrameAsyncStatus == MFX_WRN_DEVICE_BUSY)
        {
            ADD_FAILURE() << "Not expected MFX_WRN_DEVICE_BUSY from decoder";
            throw tsFAIL;
        }

        if (transcoder.m_expect_gpu_hang_from_dec_frame_async)
        {
            g_tsStatus.expect(MFX_ERR_GPU_HANG);
            g_tsStatus.check();
            throw tsOK;
        }

        if(MFX_ERR_MORE_DATA == decFrameAsyncStatus)
        {
            if(dec->m_pBitstream)
                continue;

            // bitstream finished here, get cached frames
            while(true)
            {
                mfxFrameSurface1* decodedSurface = NULL;
                if (dec->m_surf_out.size())
                {
                    decodedSurface = GetSurface(dec, true);
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
                enc->SyncOperation();

                if (transcoder.m_expect_gpu_hang_from_dec_syncop)
                {
                    g_tsStatus.expect(MFX_ERR_GPU_HANG);
                    g_tsStatus.check();
                    throw tsOK;
                }
                else
                    g_tsStatus.check();

                enc->m_bitstream.DataOffset = 0;
                enc->m_bitstream.DataLength = 0;
                encoded++;
            }
            break;
        }

        if(decFrameAsyncStatus < 0 && decFrameAsyncStatus != MFX_ERR_MORE_SURFACE) g_tsStatus.check();

        transcoder.m_frames_submitted++;

        if(MFX_ERR_MORE_SURFACE == decFrameAsyncStatus || (decFrameAsyncStatus > 0 && *dec->m_pSyncPoint == NULL))
            continue;

//      if(++submitted >= async)
        {
            while(dec->m_surf_out.size())
            {
                mfxFrameSurface1* decodedSurface = GetSurface(dec, true);
                decoded++;
                enc->EncodeFrameAsync(enc->m_session, decodedSurface ? enc->m_pCtrl : 0, decodedSurface, enc->m_pBitstream, enc->m_pSyncPoint);
                if(MFX_ERR_MORE_DATA == g_tsStatus.get())
                    continue;

                g_tsStatus.check();
                enc->SyncOperation();
                if (transcoder.m_expect_gpu_hang_from_dec_syncop)
                {
                    g_tsStatus.expect(MFX_ERR_GPU_HANG);
                    g_tsStatus.check();
                    throw tsOK;
                }
                else
                    g_tsStatus.check();

                enc->m_bitstream.DataOffset = 0;
                enc->m_bitstream.DataLength = 0;
                encoded++;
            }
            submitted = 0;
        }
    }

    g_tsLog << decoded << " FRAMES DECODED\n";
    g_tsLog << encoded << " FRAMES ENCODED\n";

    ADD_FAILURE() << "ERROR: GPU hang not reported";
    throw tsFAIL;

    TS_END;
}

int mpeg2_to_mpeg2 (unsigned int) { return gpu_hang_transcoding_test(MFX_CODEC_MPEG2, MFX_CODEC_MPEG2, "forBehaviorTest/customer_issues/mpeg2_43-169_trim1.mpg"); }
int avc_to_avc     (unsigned int) { return gpu_hang_transcoding_test(MFX_CODEC_AVC  , MFX_CODEC_AVC  , "conformance/h264/bluesky.h264"); }

TS_REG_TEST_SUITE(gpu_hang_transcoding_mpeg2_to_mpeg2, mpeg2_to_mpeg2, 1);
TS_REG_TEST_SUITE(gpu_hang_transcoding_avc_to_avc    , avc_to_avc,     1);

}
