/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#include "ts_transcoder.h"

namespace vp9e_trans_opaque
{

enum TCVal
{
    AVC     = MFX_CODEC_AVC,
    HEVC    = MFX_CODEC_HEVC,
    VP9     = MFX_CODEC_VP9,
    I_VID   = MFX_IOPATTERN_IN_VIDEO_MEMORY,
    I_SYS   = MFX_IOPATTERN_IN_SYSTEM_MEMORY,
    I_OPQ   = MFX_IOPATTERN_IN_OPAQUE_MEMORY,
    O_VID   = MFX_IOPATTERN_OUT_VIDEO_MEMORY,
    O_SYS   = MFX_IOPATTERN_OUT_SYSTEM_MEMORY,
    O_OPQ   = MFX_IOPATTERN_OUT_OPAQUE_MEMORY,
    IO_IN   = 0x0f,
    IO_OUT  = 0xf0,
    DEC     = 1,
    VPP     = 2,
    ENC     = 3,
};

const struct TestCase
{
    mfxU32 DEC;
    mfxU32 ENC;
    mfxU16 IOP;
    mfxU16 INV;
}
tcs[] =
{
    /*0000*/ {HEVC  , VP9  , I_OPQ|O_VID, 0},
    /*0001*/ {HEVC  , VP9  , I_OPQ|O_SYS, 0},
    /*0002*/ {HEVC  , VP9  , I_OPQ|O_OPQ, 0},
    /*0003*/ {AVC   , VP9  , I_OPQ|O_VID, 0},
    /*0004*/ {AVC   , VP9  , I_OPQ|O_SYS, 0},
    /*0005*/ {AVC   , VP9  , I_OPQ|O_OPQ, 0},
};

std::string getStream(mfxU32 codec)
{
    switch (codec)
    {
        case AVC:   return "forBehaviorTest/foreman_cif.h264";
        case HEVC:  return "conformance/hevc/itu/DBLK_A_SONY_3.bit";
        default: break;
    }
    return "";
}

void InvalidateOpaque(mfxFrameSurface1& s)
{
    if (g_tsImpl == MFX_IMPL_HARDWARE)
        s.Data.MemId = (mfxMemId)1;
    else
        s.Data.Y = (mfxU8*)1;
}

void RestoreOpaque(mfxFrameSurface1& s)
{
    s.Data.MemId = (mfxMemId)0;
    s.Data.Y = (mfxU8*)0;
}

int RunTest(unsigned int id)
{
    TS_START;
    auto& tc = tcs[id];

    if (g_tsImpl == MFX_IMPL_HARDWARE)
    {
        if (g_tsHWtype < MFX_HW_CNL)
        {
            g_tsLog << "SKIPPED for this platform\n";
            return 0;
        }
    }

    const char* stream = g_tsStreamPool.Get(getStream(tc.DEC));
    g_tsStreamPool.Reg();

    tsTranscoder tr(tc.DEC, tc.ENC);

    tsBitstreamReader reader(stream, 100000);
    tr.m_bsProcIn = &reader;

    //tsBitstreamWriter w("debug.bit");
    //tr.m_bsProcOut = &w;

    tr.m_parDEC.IOPattern = tc.IOP & IO_OUT;
    tr.m_parENC.IOPattern = tc.IOP & IO_IN;

    tr.InitPipeline();

    tsVideoDecoder& dec = tr;
    tsVideoVPP& vpp = tr;
    tsVideoEncoder& enc = tr;

    switch (tc.INV)
    {
    case 1:
        tr.m_cId = 0;
        dec.SetPar4_DecodeFrameAsync();
        dec.m_default = false;

        InvalidateOpaque(*dec.m_pSurf);

        dec.DecodeFrameAsync();
        g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR);
        g_tsStatus.check();

        return 0;
    case 2:
        vpp.m_default = false;
        vpp.m_pSurfIn = vpp.m_pSurfPoolIn->GetSurface();
        vpp.m_pSurfOut = vpp.m_pSurfPoolOut->GetSurface();

        InvalidateOpaque(*vpp.m_pSurfIn);

        vpp.RunFrameVPPAsync();
        g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR);
        g_tsStatus.check();

        RestoreOpaque(*vpp.m_pSurfIn);
        InvalidateOpaque(*vpp.m_pSurfOut);

        vpp.RunFrameVPPAsync();
        g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR);
        g_tsStatus.check();

        return 0;
    case 3:
        enc.m_default = false;
        enc.m_pSurf = vpp.m_pSurfPoolOut->GetSurface();

        InvalidateOpaque(*enc.m_pSurf);

        enc.EncodeFrameAsync();
        g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR);
        g_tsStatus.check();

        return 0;
    case 0:
    default:
        break;
    }

    tr.TranscodeFrames(30);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(vp9e_trans_opaque, RunTest, sizeof(tcs)/sizeof(TestCase));
};
