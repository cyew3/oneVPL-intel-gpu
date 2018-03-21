/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2018 Intel Corporation. All Rights Reserved.
//
*/

#include "ts_transcoder.h"

/*
    Test allocated opaque DEC->VPP->ENC and check different sequence at deinitialization steps.
    Test check that MSDK behaviour does not crashed due to different combinations of close.
*/

namespace trans_opaque_deallocation
{

enum TCVal
{
    AVC    = MFX_CODEC_AVC,
    I_OPQ  = MFX_IOPATTERN_IN_OPAQUE_MEMORY,
    O_OPQ  = MFX_IOPATTERN_OUT_OPAQUE_MEMORY,
    IO_IN  = 0x0f,
    IO_OUT = 0xf0,
    DEC    = 1,
    VPP    = 2,
    ENC    = 3
};

enum MODE
{
    DEALLOC_ENC_DEC_VPP = 1,
    DEALLOC_ENC_VPP_DEC = 2,
    DEALLOC_DEC_ENC_VPP = 3,
    DEALLOC_DEC_VPP_ENC = 4,
    DEALLOC_VPP_DEC_ENC = 5,
    DEALLOC_VPP_ENC_DEC = 6
};

const struct TestCase
{
    mfxU32 m_mode;
} tcs[] =
{
/*00*/ {DEALLOC_ENC_DEC_VPP},
/*01*/ {DEALLOC_ENC_VPP_DEC},
/*02*/ {DEALLOC_DEC_ENC_VPP},
/*03*/ {DEALLOC_DEC_VPP_ENC},
/*04*/ {DEALLOC_VPP_DEC_ENC},
/*05*/ {DEALLOC_VPP_ENC_DEC},
};

int RunTest(unsigned int id)
{
    TS_START;
    auto& tc = tcs[id];

    const char* stream = g_tsStreamPool.Get("forBehaviorTest/foreman_cif.h264");
    g_tsStreamPool.Reg();

    tsTranscoder tr(AVC, AVC);

    tsBitstreamReader reader(stream, 100000);
    tr.m_bsProcIn = &reader;

    mfxU16 IOP = I_OPQ|O_OPQ;

    tr.m_parDEC.IOPattern = IOP & IO_OUT;
    tr.m_parENC.IOPattern = IOP & IO_IN;
    tr.m_parVPP.IOPattern = IOP;

    tr.InitPipeline();

    tsVideoDecoder& dec = tr;
    tsVideoVPP& vpp     = tr;
    tsVideoEncoder& enc = tr;

    tr.TranscodeFrames(30);

    switch (tc.m_mode)
    {
    case DEALLOC_ENC_DEC_VPP:
        enc.Close();
        dec.Close();
        vpp.Close();

        break;
    case DEALLOC_ENC_VPP_DEC:
        enc.Close();
        vpp.Close();
        dec.Close();

        break;
    case DEALLOC_DEC_ENC_VPP:
        dec.Close();
        enc.Close();
        vpp.Close();

        break;
    case DEALLOC_DEC_VPP_ENC:
        dec.Close();
        vpp.Close();
        enc.Close();

        break;
    case DEALLOC_VPP_DEC_ENC:
        vpp.Close();
        dec.Close();
        enc.Close();

        break;
    case DEALLOC_VPP_ENC_DEC:
        vpp.Close();
        enc.Close();
        dec.Close();

        break;
    default:
        g_tsLog << "ERROR: Unexpected behaviour, test case not supported\n";
        return 1;
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(trans_opaque_deallocation, RunTest, sizeof(tcs)/sizeof(TestCase));
};