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

namespace trans_opaque
{

enum TCVal
{
    AVC     = MFX_CODEC_AVC,
    MPEG2   = MFX_CODEC_MPEG2,
    VC1     = MFX_CODEC_VC1,
    JPEG    = MFX_CODEC_JPEG,
    HEVC    = MFX_CODEC_HEVC,
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
    ENC     = 3
};

const struct TestCase
{
    mfxU32 DEC;
    mfxU32 ENC;
    mfxU16 IOP;
    mfxU16 INV;
} tcs[] =
{
/*0000*/ {AVC  , AVC  , I_VID|O_OPQ, 0},
/*0001*/ {AVC  , AVC  , I_SYS|O_OPQ, 0},
/*0002*/ {AVC  , AVC  , I_OPQ|O_VID, 0},
/*0003*/ {AVC  , AVC  , I_OPQ|O_SYS, 0},
/*0004*/ {AVC  , AVC  , I_OPQ|O_OPQ, 0},
/*0005*/ {AVC  , MPEG2, I_VID|O_OPQ, 0},
/*0006*/ {AVC  , MPEG2, I_SYS|O_OPQ, 0},
/*0007*/ {AVC  , MPEG2, I_OPQ|O_VID, 0},
/*0008*/ {AVC  , MPEG2, I_OPQ|O_SYS, 0},
/*0009*/ {AVC  , MPEG2, I_OPQ|O_OPQ, 0},
/*0010*/ {AVC  , JPEG , I_VID|O_OPQ, 0},
/*0011*/ {AVC  , JPEG , I_SYS|O_OPQ, 0},
/*0012*/ {AVC  , JPEG , I_OPQ|O_VID, 0},
/*0013*/ {AVC  , JPEG , I_OPQ|O_SYS, 0},
/*0014*/ {AVC  , JPEG , I_OPQ|O_OPQ, 0},
/*0015*/ {AVC  , HEVC , I_VID|O_OPQ, 0},
/*0016*/ {AVC  , HEVC , I_SYS|O_OPQ, 0},
/*0017*/ {AVC  , HEVC , I_OPQ|O_VID, 0},
/*0018*/ {AVC  , HEVC , I_OPQ|O_SYS, 0},
/*0019*/ {AVC  , HEVC , I_OPQ|O_OPQ, 0},
/*0020*/ {MPEG2, AVC  , I_VID|O_OPQ, 0},
/*0021*/ {MPEG2, AVC  , I_SYS|O_OPQ, 0},
/*0022*/ {MPEG2, AVC  , I_OPQ|O_VID, 0},
/*0023*/ {MPEG2, AVC  , I_OPQ|O_SYS, 0},
/*0024*/ {MPEG2, AVC  , I_OPQ|O_OPQ, 0},
/*0025*/ {MPEG2, MPEG2, I_VID|O_OPQ, 0},
/*0026*/ {MPEG2, MPEG2, I_SYS|O_OPQ, 0},
/*0027*/ {MPEG2, MPEG2, I_OPQ|O_VID, 0},
/*0028*/ {MPEG2, MPEG2, I_OPQ|O_SYS, 0},
/*0029*/ {MPEG2, MPEG2, I_OPQ|O_OPQ, 0},
/*0030*/ {MPEG2, JPEG , I_VID|O_OPQ, 0},
/*0031*/ {MPEG2, JPEG , I_SYS|O_OPQ, 0},
/*0032*/ {MPEG2, JPEG , I_OPQ|O_VID, 0},
/*0033*/ {MPEG2, JPEG , I_OPQ|O_SYS, 0},
/*0034*/ {MPEG2, JPEG , I_OPQ|O_OPQ, 0},
/*0035*/ {MPEG2, HEVC , I_VID|O_OPQ, 0},
/*0036*/ {MPEG2, HEVC , I_SYS|O_OPQ, 0},
/*0037*/ {MPEG2, HEVC , I_OPQ|O_VID, 0},
/*0038*/ {MPEG2, HEVC , I_OPQ|O_SYS, 0},
/*0039*/ {MPEG2, HEVC , I_OPQ|O_OPQ, 0},
/*0040*/ {VC1  , AVC  , I_VID|O_OPQ, 0},
/*0041*/ {VC1  , AVC  , I_SYS|O_OPQ, 0},
/*0042*/ {VC1  , AVC  , I_OPQ|O_VID, 0},
/*0043*/ {VC1  , AVC  , I_OPQ|O_SYS, 0},
/*0044*/ {VC1  , AVC  , I_OPQ|O_OPQ, 0},
/*0045*/ {VC1  , MPEG2, I_VID|O_OPQ, 0},
/*0046*/ {VC1  , MPEG2, I_SYS|O_OPQ, 0},
/*0047*/ {VC1  , MPEG2, I_OPQ|O_VID, 0},
/*0048*/ {VC1  , MPEG2, I_OPQ|O_SYS, 0},
/*0049*/ {VC1  , MPEG2, I_OPQ|O_OPQ, 0},
/*0050*/ {VC1  , JPEG , I_VID|O_OPQ, 0},
/*0051*/ {VC1  , JPEG , I_SYS|O_OPQ, 0},
/*0052*/ {VC1  , JPEG , I_OPQ|O_VID, 0},
/*0053*/ {VC1  , JPEG , I_OPQ|O_SYS, 0},
/*0054*/ {VC1  , JPEG , I_OPQ|O_OPQ, 0},
/*0055*/ {VC1  , HEVC , I_VID|O_OPQ, 0},
/*0056*/ {VC1  , HEVC , I_SYS|O_OPQ, 0},
/*0057*/ {VC1  , HEVC , I_OPQ|O_VID, 0},
/*0058*/ {VC1  , HEVC , I_OPQ|O_SYS, 0},
/*0059*/ {VC1  , HEVC , I_OPQ|O_OPQ, 0},
/*0060*/ {JPEG , AVC  , I_VID|O_OPQ, 0},
/*0061*/ {JPEG , AVC  , I_SYS|O_OPQ, 0},
/*0062*/ {JPEG , AVC  , I_OPQ|O_VID, 0},
/*0063*/ {JPEG , AVC  , I_OPQ|O_SYS, 0},
/*0064*/ {JPEG , AVC  , I_OPQ|O_OPQ, 0},
/*0065*/ {JPEG , MPEG2, I_VID|O_OPQ, 0},
/*0066*/ {JPEG , MPEG2, I_SYS|O_OPQ, 0},
/*0067*/ {JPEG , MPEG2, I_OPQ|O_VID, 0},
/*0068*/ {JPEG , MPEG2, I_OPQ|O_SYS, 0},
/*0069*/ {JPEG , MPEG2, I_OPQ|O_OPQ, 0},
/*0070*/ {JPEG , JPEG , I_VID|O_OPQ, 0},
/*0071*/ {JPEG , JPEG , I_SYS|O_OPQ, 0},
/*0072*/ {JPEG , JPEG , I_OPQ|O_VID, 0},
/*0073*/ {JPEG , JPEG , I_OPQ|O_SYS, 0},
/*0074*/ {JPEG , JPEG , I_OPQ|O_OPQ, 0},
/*0075*/ {JPEG , HEVC , I_VID|O_OPQ, 0},
/*0076*/ {JPEG , HEVC , I_SYS|O_OPQ, 0},
/*0077*/ {JPEG , HEVC , I_OPQ|O_VID, 0},
/*0078*/ {JPEG , HEVC , I_OPQ|O_SYS, 0},
/*0079*/ {JPEG , HEVC , I_OPQ|O_OPQ, 0},
/*0080*/ {HEVC , AVC  , I_VID|O_OPQ, 0},
/*0081*/ {HEVC , AVC  , I_SYS|O_OPQ, 0},
/*0082*/ {HEVC , AVC  , I_OPQ|O_VID, 0},
/*0083*/ {HEVC , AVC  , I_OPQ|O_SYS, 0},
/*0084*/ {HEVC , AVC  , I_OPQ|O_OPQ, 0},
/*0085*/ {HEVC , MPEG2, I_VID|O_OPQ, 0},
/*0086*/ {HEVC , MPEG2, I_SYS|O_OPQ, 0},
/*0087*/ {HEVC , MPEG2, I_OPQ|O_VID, 0},
/*0088*/ {HEVC , MPEG2, I_OPQ|O_SYS, 0},
/*0089*/ {HEVC , MPEG2, I_OPQ|O_OPQ, 0},
/*0090*/ {HEVC , JPEG , I_VID|O_OPQ, 0},
/*0091*/ {HEVC , JPEG , I_SYS|O_OPQ, 0},
/*0092*/ {HEVC , JPEG , I_OPQ|O_VID, 0},
/*0093*/ {HEVC , JPEG , I_OPQ|O_SYS, 0},
/*0094*/ {HEVC , JPEG , I_OPQ|O_OPQ, 0},
/*0095*/ {HEVC , HEVC , I_VID|O_OPQ, 0},
/*0096*/ {HEVC , HEVC , I_SYS|O_OPQ, 0},
/*0097*/ {HEVC , HEVC , I_OPQ|O_VID, 0},
/*0098*/ {HEVC , HEVC , I_OPQ|O_SYS, 0},
/*0099*/ {HEVC , HEVC , I_OPQ|O_OPQ, 0},
/*0100*/ {AVC  , AVC  , I_OPQ|O_OPQ, 1},
/*0101*/ {MPEG2, AVC  , I_OPQ|O_OPQ, 1},
/*0102*/ {VC1  , AVC  , I_OPQ|O_OPQ, 1},
/*0103*/ {JPEG , AVC  , I_OPQ|O_OPQ, 1},
/*0104*/ {HEVC , AVC  , I_OPQ|O_OPQ, 1},
/*0105*/ {AVC  , AVC  , I_OPQ|O_OPQ, 2},
/*0106*/ {AVC  , AVC  , I_OPQ|O_OPQ, 3},
/*0107*/ {AVC  , MPEG2, I_OPQ|O_OPQ, 3},
/*0108*/ {AVC  , JPEG , I_OPQ|O_OPQ, 3},
/*0109*/ {AVC  , HEVC , I_OPQ|O_OPQ, 3},
};

std::string getStream(mfxU32 codec)
{
    switch (codec)
    {
    case AVC:   return "forBehaviorTest/foreman_cif.h264";
    case MPEG2: return "forBehaviorTest/foreman_cif.m2v";
    case VC1:   return "forBehaviorTest/foreman_cif.vc1";
    case JPEG:  return "forBehaviorTest/mjpeg/wildlife_1280x720_89frames.mjpg";
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
        if ((g_tsHWtype < MFX_HW_SKL && tc.ENC == HEVC) || (tc.ENC == JPEG))
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

TS_REG_TEST_SUITE(trans_opaque, RunTest, sizeof(tcs)/sizeof(TestCase));
};