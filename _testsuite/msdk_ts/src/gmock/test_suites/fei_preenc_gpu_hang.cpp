/******************************************************************************* *\

Copyright (C) 2017 Intel Corporation.  All rights reserved.

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

#include "ts_preenc.h"
#include "ts_fei_warning.h"
#include "mfx_ext_buffers.h"

namespace fei_preenc_gpu_hang
{

int test(mfxU32 codecId)
{
    TS_START;

    CHECK_FEI_SUPPORT();

    const mfxU16 n_frames = 10;
    std::vector<mfxExtBuffer*> in_buffs;
    std::vector<mfxExtBuffer*> out_buffs;
    mfxExtBuffer bufHang;
    mfxExtBuffer *pBufHang = &bufHang;
    mfxExtFeiPreEncCtrl ctrl;
    mfxExtFeiPreEncCtrl *pCtrl = &ctrl;
    mfxExtFeiPreEncMV mv;
    mfxExtFeiPreEncMV *pMv = &mv;
    mfxExtFeiPreEncMBStat mb;
    mfxExtFeiPreEncMBStat *pMb = &mb;
    mfxU32 count;
    mfxStatus sts = MFX_ERR_NONE;

    tsVideoPreENC preenc(MFX_FEI_FUNCTION_PREENC, codecId, true);

    preenc.m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    preenc.m_par.mfx.FrameInfo.Width  = preenc.m_par.mfx.FrameInfo.CropW = 720;
    preenc.m_par.mfx.FrameInfo.Height = preenc.m_par.mfx.FrameInfo.CropH = 576;
    preenc.m_par.mfx.GopPicSize = 1;
    preenc.m_par.mfx.GopRefDist = 1;
    preenc.m_par.mfx.IdrInterval = 0;
    preenc.m_par.mfx.NumSlice = 1;
    preenc.m_par.mfx.NumRefFrame = 1;
    preenc.m_par.AsyncDepth = 1;
    preenc.m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    preenc.m_par.mfx.EncodedOrder = 1;

    mfxExtFeiParam& extbuffer = preenc.m_par;
    extbuffer.SingleFieldProcessing = MFX_CODINGOPTION_OFF;

    preenc.Init();
    g_tsStatus.check();

    preenc.m_request.NumFrameMin = 1;
    preenc.m_request.NumFrameSuggested= 1;
    preenc.m_request.Type = MFX_MEMTYPE_FROM_ENC |
                            MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET |
                            MFX_MEMTYPE_EXTERNAL_FRAME;
    preenc.m_request.Info = preenc.m_par.mfx.FrameInfo;
    preenc.m_request.AllocId = preenc.m_par.AllocId;

    preenc.AllocSurfaces();
    g_tsStatus.check();

    in_buffs.clear();
    out_buffs.clear();

    memset(pCtrl, 0x0, sizeof(mfxExtFeiPreEncCtrl));
    pCtrl->Header.BufferId = MFX_EXTBUFF_FEI_PREENC_CTRL;
    pCtrl->Header.BufferSz = sizeof(mfxExtFeiPreEncCtrl);
    pCtrl->PictureType = MFX_PICTYPE_FRAME;
    pCtrl->RefPictureType[0] = MFX_PICTYPE_FRAME;
    pCtrl->RefPictureType[1] = MFX_PICTYPE_FRAME;
    pCtrl->DownsampleReference[0] = MFX_CODINGOPTION_OFF;
    pCtrl->DownsampleReference[1] = MFX_CODINGOPTION_OFF;
    pCtrl->DisableMVOutput = false;
    pCtrl->DisableStatisticsOutput = false;
    pCtrl->FTEnable = false;
    pCtrl->AdaptiveSearch = 0;
    pCtrl->LenSP = 48;
    pCtrl->MBQp = 0;
    pCtrl->MVPredictor = 0;
    pCtrl->RefHeight = 32;
    pCtrl->RefWidth = 32;
    pCtrl->SubPelMode = 0x03;
    pCtrl->SearchWindow = 5;
    pCtrl->SearchPath = 0;
    pCtrl->Qp = 26;
    pCtrl->IntraSAD = 0;
    pCtrl->InterSAD = 0;
    pCtrl->SubMBPartMask = 0;
    pCtrl->IntraPartMask = 0;
    pCtrl->Enable8x8Stat = 0;
    pCtrl->DownsampleInput = MFX_CODINGOPTION_ON;
    pCtrl->RefFrame[0] = NULL;
    pCtrl->RefFrame[1] = NULL;
    in_buffs.push_back(reinterpret_cast<mfxExtBuffer *>(pCtrl));

    // Sending a MFX_EXTBUFF_GPU_HANG ext buffer to a debug-version UMD
    // manually triggers a GPU HANG event.
    memset(pBufHang, 0x0, sizeof(mfxExtBuffer));
    pBufHang->BufferId = MFX_EXTBUFF_GPU_HANG;
    pBufHang->BufferSz = sizeof(mfxExtBuffer);
    in_buffs.push_back(reinterpret_cast<mfxExtBuffer *>(pBufHang));

    preenc.m_pPreENCInput->NumExtParam = (mfxU16)in_buffs.size();
    preenc.m_pPreENCInput->ExtParam = in_buffs.data();
    preenc.m_pPreENCInput->NumFrameL0 = 0;
    preenc.m_pPreENCInput->NumFrameL1 = 0;
    preenc.m_pPreENCInput->InSurface = preenc.GetSurface(0);

    memset(pMv, 0x0, sizeof(mfxExtFeiPreEncMV));
    pMv->Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV;
    pMv->Header.BufferSz = sizeof(mfxExtFeiPreEncMV);
    pMv->NumMBAlloc = (preenc.m_par.mfx.FrameInfo.Width/16) *
                      (preenc.m_par.mfx.FrameInfo.Height/16);
    pMv->MB = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB[pMv->NumMBAlloc];
    out_buffs.push_back(reinterpret_cast<mfxExtBuffer *>(pMv));

    memset(pMb, 0x0, sizeof(mfxExtFeiPreEncMBStat));
    pMb->Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MB;
    pMb->Header.BufferSz = sizeof(mfxExtFeiPreEncMBStat);
    pMb->NumMBAlloc = pMv->NumMBAlloc;
    pMb->MB = new mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB[pMb->NumMBAlloc];
    out_buffs.push_back(reinterpret_cast<mfxExtBuffer *>(pMb));

    memset(preenc.m_pPreENCOutput,  0x0, sizeof(mfxENCOutput));
    preenc.m_pPreENCOutput->NumExtParam = (mfxU16)out_buffs.size();
    preenc.m_pPreENCOutput->ExtParam = out_buffs.data();

    g_tsStatus.disable();

    for(count=0; (count<n_frames)&&(sts==MFX_ERR_NONE); count++)
    {
        sts = preenc.ProcessFrameAsync();
        if(MFX_ERR_GPU_HANG == sts)
            break;

        sts = preenc.SyncOperation();
        if(MFX_ERR_GPU_HANG == sts)
            break;
    }

    delete [] (pMv->MB);
    delete [] (pMb->MB);

    g_tsStatus.expect(MFX_ERR_GPU_HANG);
    g_tsStatus.enable();
    g_tsStatus.check(sts);

    g_tsLog << count << " FRAMES Encoded Before GPU hang.\n";

    TS_END;
    return 0;
}

int FEIPreENCTest    (unsigned int) { return test(MFX_CODEC_AVC); }

TS_REG_TEST_SUITE(fei_preenc_gpu_hang, FEIPreENCTest, 1);
}
