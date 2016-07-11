/******************************************************************************* *\

Copyright (C) 2016 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*
 * This test is for MQ1234.
 * Please note the ext buffer of HANG can only be supported by specific internal driver
 * release to verify msdk functions to report GPU hang and to recover from GPU hang.
 */

#include "ts_encoder.h"

namespace fei_encode_gpu_hang
{

int test(mfxU32 codecId)
{
    TS_START;

    tsVideoEncoder enc(MFX_FEI_FUNCTION_ENCODE, codecId);
    enc.m_pPar->AsyncDepth = 1; //limitation for FEI (from sample_fei)
    enc.m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    enc.m_pPar->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    std::list<mfxSyncPoint> sp;
    bool hang_detected = false;

    enc.GetVideoParam();

    enc.QueryIOSurf();

    // for EncodeFrameAsync() at the pipeline end
    // component may keep locked surface for failed task till Close()
    enc.m_request.NumFrameMin ++;
    enc.m_request.NumFrameSuggested ++;
    enc.AllocSurfaces();

    //to add mfxExtFeiEncFrameCtrl buffer for FEI ENCODE
    enc.m_ctrl.AddExtBuffer(MFX_EXTBUFF_FEI_ENC_CTRL, sizeof(mfxExtFeiEncFrameCtrl));
    mfxExtFeiEncFrameCtrl *feiEncCtrl = (mfxExtFeiEncFrameCtrl *)enc.m_ctrl.GetExtBuffer(MFX_EXTBUFF_FEI_ENC_CTRL);
    feiEncCtrl->SearchPath = 2;
    feiEncCtrl->LenSP = 57;
    feiEncCtrl->SubMBPartMask = 0;
    feiEncCtrl->MultiPredL0 = 0;
    feiEncCtrl->MultiPredL1 = 0;
    feiEncCtrl->SubPelMode = 3;
    feiEncCtrl->InterSAD = 2;
    feiEncCtrl->IntraSAD = 2;
    feiEncCtrl->DistortionType = 0;
    feiEncCtrl->RepartitionCheckEnable = 0;
    feiEncCtrl->AdaptiveSearch = 0;
    feiEncCtrl->MVPredictor = 0;
    feiEncCtrl->NumMVPredictors[0] = 1;
    feiEncCtrl->PerMBQp = 0; //non-zero value
    feiEncCtrl->PerMBInput = 0;
    feiEncCtrl->MBSizeCtrl = 0;
    feiEncCtrl->RefHeight = 32;
    feiEncCtrl->RefWidth = 32;
    feiEncCtrl->SearchWindow = 5;

    mfxU32 submitted  = 0;
    //hardcode here, put 50+ cycles, but it should not use so many cycles out.
    for (mfxU16 j = 0; j < 50 && !hang_detected; j++)
    {
        while (sp.size() < enc.m_par.AsyncDepth)
        {
            if (enc.EncodeFrameAsync() == MFX_ERR_NONE)
            {
                //hardcode here, to attach HANG ext buffer to trigger GPU hang after
                //encoding several frames normally.
                if (submitted == 5)
                {
                    enc.m_ctrl.AddExtBuffer(MFX_MAKEFOURCC('H','A','N','G'), sizeof(mfxExtBuffer));
                }

                submitted++;
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

    EXPECT_TRUE(hang_detected) << "ERROR: GPU Hang was not triggered by 'HANG' Extended buffer\n";

    if (hang_detected)
    {
        //GPU hang was already detected, remove it from ext buffers list
        enc.m_ctrl.RemoveExtBuffer(MFX_MAKEFOURCC('H','A','N','G'));

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

int FEIEncodeTest    (unsigned int) { return test(MFX_CODEC_AVC); }

TS_REG_TEST_SUITE(fei_encode_gpu_hang, FEIEncodeTest, 1);
}
