/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"

namespace hevcd_bs_data_flag
{
    
int RunTest(unsigned int id)
{
    TS_START;
    const char* stream = g_tsStreamPool.Get("conformance/hevc/itu/HRD_A_Fujitsu_3.bin"); // low-delay
    const char* stream2 = g_tsStreamPool.Get("conformance/hevc/itu/AMP_D_Hisilicon.bit"); // w/ reordering
    g_tsStreamPool.Reg();

    tsVideoDecoder dec(MFX_CODEC_HEVC);
    tsSplitterHEVCES spl(stream, 15 * 1024);
    tsSplitterHEVCES spl2(stream2, 36 * 1024);
    auto& bs = dec.m_bitstream;

    dec.m_bs_processor = &spl;
    dec.m_par.AsyncDepth = 4;

    dec.MFXInit();
    dec.Load();

    spl.ProcessBitstream(bs, 1);
    bs.DataFlag = 0;

    mfxU8* begin = bs.Data + bs.DataOffset;
    mfxU8* end = begin + bs.DataLength - 4;
    bool found = false;
    mfxBitstream tmp = bs;

    for (mfxU8* p = begin; p < end; p++)
    {
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
        {
            if (found)
            {
                bs.DataLength = mfxU32(p - begin);
                break;
            }
            found = ((p[3] >> 1) & 0x03ff) == 34; //PPS
        }
    }

    g_tsStatus.expect(MFX_ERR_MORE_DATA);
    dec.DecodeHeader(dec.m_session, &bs, &dec.m_par);

    bs.DataFlag = MFX_BITSTREAM_EOS;
    dec.DecodeHeader(dec.m_session, &bs, &dec.m_par);

    dec.Init();

    (mfxBitstream&)bs = tmp;
    bs.DataLength -= 20;
    bs.DataFlag = MFX_BITSTREAM_COMPLETE_FRAME;
    dec.DecodeFrameAsync();
    g_tsStatus.check();

    dec.SyncOperation();
    dec.Close();

    dec.m_bs_processor = &spl2;
    dec.DecodeHeader();
    dec.Init();

    mfxU32 buffered = 0;

    do
    {
        dec.DecodeFrameAsync();
        buffered += g_tsStatus.get() == MFX_ERR_MORE_DATA;
    } while(g_tsStatus.get() == MFX_ERR_MORE_DATA || g_tsStatus.get() == MFX_ERR_MORE_SURFACE || g_tsStatus.get() > 0);

    dec.SyncOperation();

    dec.m_bs_processor = 0;
    bs.DataFlag = MFX_BITSTREAM_EOS; //expected same effect as dec.m_pBitstream = 0;

    while (buffered--)
    {
        dec.DecodeFrameAsync();
        g_tsStatus.check();
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(hevcd_bs_data_flag, RunTest, 1);
}