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
#include "time_defs.h"
#include <list>

namespace avce_gpu_hang
{

int test(mfxU32 codecId, mfxU32 version)
{
    TS_START;

    if (version == 0)
    {
        MSDK_SLEEP(2000);
    }

    tsVideoEncoder enc(codecId);
    tsNoiseFiller f;
    std::list<mfxSyncPoint> sp;
    bool hang_detected = false;

    enc.GetVideoParam();

    if (!enc.m_par.AsyncDepth)
    {
        enc.Close();
        enc.m_par.AsyncDepth = 5;
        enc.GetVideoParam();
    }

    enc.QueryIOSurf();

    // for EncodeFrameAsync() at the pipeline end
    // component may keep locked surface for failed task till Close()
    enc.m_request.NumFrameMin ++;
    enc.m_request.NumFrameSuggested ++;
    enc.AllocSurfaces();


    if (version == 0)
    {
        //fill all surfaces before encoding to reduce time between EncodeFrameAsync()
        //for (mfxU32 i = 0; ((tsSurfaceProcessor*)&f)->ProcessSurface(enc.GetSurface(i), enc.m_pFrameAllocator); i++);
        enc.m_filler = &f;
    }

    // try to cause hang several times to compensate "echo 1 > /sys/kernel/debug/dri/0/i915_wedged" method instability
    for (mfxU16 j = 0; j < 50 && !hang_detected; j++)
    {
        while (sp.size() < enc.m_par.AsyncDepth)
        {
            if (enc.EncodeFrameAsync() == MFX_ERR_NONE)
            {
                if (version == 0)
                {
                    int unused_ = system("echo 1 > /sys/kernel/debug/dri/0/i915_wedged");
                    unused_;
                }
                else
                {
                    if (sp.size() == 2)
                    {
                        enc.m_ctrl.AddExtBuffer(MFX_MAKEFOURCC('H','A','N','G'), sizeof(mfxExtBuffer));
                        enc.m_pCtrl = &enc.m_ctrl;
                    }
                    else enc.m_pCtrl = 0;
                }

                sp.push_back(*enc.m_pSyncPoint);
                g_tsStatus.check();
            }
            else if (g_tsStatus.get() == MFX_ERR_GPU_HANG)
            {
                g_tsStatus.expect(MFX_ERR_GPU_HANG);
                g_tsStatus.check();
                break;
            }
            else
            {
                g_tsStatus.expect(MFX_ERR_MORE_DATA);
                g_tsStatus.check();
            }
        }

        do
        {
            g_tsStatus.disable_next_check();
            enc.SyncOperation(sp.front());
            enc.m_bitstream.DataLength = 0;
            sp.pop_front();

            if (g_tsStatus.get() == MFX_ERR_GPU_HANG)
            {
                hang_detected = true;
                continue;
            }

            g_tsStatus.expect(hang_detected ? MFX_ERR_ABORTED : MFX_ERR_NONE);
            g_tsStatus.check();

        } while (hang_detected && !sp.empty());
    }

    EXPECT_TRUE(hang_detected);

    if (hang_detected)
    {
        enc.m_filler = 0;

        g_tsStatus.expect(MFX_ERR_GPU_HANG);
        enc.EncodeFrameAsync();
        g_tsStatus.check();
        g_tsStatus.expect(MFX_ERR_GPU_HANG);
        enc.EncodeFrameAsync();
        g_tsStatus.check();
    }

    TS_END;
    return 0;
}

int AVCTest    (unsigned int) { return test(MFX_CODEC_AVC, 1); }
int MPEG2Test  (unsigned int) { return test(MFX_CODEC_MPEG2, 1); }
int HEVCTest   (unsigned int) { return test(MFX_CODEC_HEVC, 1); }

TS_REG_TEST_SUITE(hevce_gpu_hang, HEVCTest, 1);
TS_REG_TEST_SUITE(avce_gpu_hang, AVCTest, 1);
TS_REG_TEST_SUITE(mpeg2e_gpu_hang, MPEG2Test, 1);
}
