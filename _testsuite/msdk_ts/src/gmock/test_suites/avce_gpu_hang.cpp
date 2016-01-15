#include "ts_encoder.h"
#include <vector>

namespace avce_gpu_hang
{

int test(unsigned int)
{
    TS_START;

    tsVideoEncoder enc(MFX_CODEC_AVC);
    tsNoiseFiller f;
    std::vector<mfxSyncPoint> sp;
    bool hang_detected = false;

    enc.m_filler = &f;

    enc.GetVideoParam();

    for (mfxU16 i = 0; i < enc.m_par.AsyncDepth;)
    {
        if (enc.EncodeFrameAsync() == MFX_ERR_NONE)
        {
            system("echo 1 > /sys/kernel/debug/dri/0/i915_wedged");
            sp.push_back(*enc.m_pSyncPoint);
            i++;
        }
    }

    while (!sp.empty())
    {
        g_tsStatus.disable_next_check();
        enc.SyncOperation(sp.back());
        sp.pop_back();

        if (g_tsStatus.get() == MFX_ERR_GPU_HANG)
        {
            hang_detected = true;
            continue;
        }

        g_tsStatus.expect(hang_detected ? MFX_ERR_NONE : MFX_ERR_ABORTED);
        g_tsStatus.check();
    }

    EXPECT_TRUE(hang_detected);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(avce_gpu_hang, test, 1);
}