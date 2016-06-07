/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"

namespace hevcd_low_latency
{
    
int RunTest(unsigned int id)
{
    TS_START;
    const char* stream = g_tsStreamPool.Get("conformance/hevc/itu/HRD_A_Fujitsu_3.bin"); // SPS.max_num_reorder_pics = 0
    g_tsStreamPool.Reg();

    tsVideoDecoder dec(MFX_CODEC_HEVC);
    tsSplitterHEVCES spl(stream, 15 * 1024); // exactly 1 AU in bitstream for each DecodeFrameAsync()

    dec.m_bs_processor = &spl;
    dec.m_par.AsyncDepth = 1; // enable LowLatency mode for decoder (by doc.)

    dec.DecodeHeader();

    for (mfxU32 i = 0; i < 96; i++) // 96 frames in stream; no buffering, so no zero bitstream required at the end
    {
        dec.DecodeFrameAsync();
        g_tsStatus.check(); // expect MFX_ERR_NONE for each call
        dec.SyncOperation();
        dec.m_update_bs = true;
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(hevcd_low_latency, RunTest, 1);
}