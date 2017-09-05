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

#include "ts_encpak.h"
#include "ts_fei_warning.h"
#include "mfx_ext_buffers.h"

namespace fei_pak_gpu_hang
{

int test(mfxU32 codecId)
{
    TS_START;

    CHECK_FEI_SUPPORT();

    const mfxU16 n_frames = 10;
    mfxU16 num_fields;
    mfxExtCodingOption extOpt;
    mfxExtBuffer bufHang;
    mfxU32 count;
    mfxStatus sts = MFX_ERR_NONE;

    tsVideoENCPAK encpak(MFX_FEI_FUNCTION_ENC, MFX_FEI_FUNCTION_PAK, codecId, true);

    encpak.enc.m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    encpak.enc.m_par.mfx.GopPicSize = 30;
    encpak.enc.m_par.mfx.GopRefDist = 1;
    encpak.enc.m_par.mfx.NumRefFrame = 2;
    encpak.enc.m_par.mfx.TargetUsage = 4;
    encpak.enc.m_par.AsyncDepth = 1;
    encpak.enc.m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    encpak.enc.m_par.mfx.EncodedOrder = 1;

    encpak.pak.m_par.mfx = encpak.enc.m_par.mfx;
    encpak.pak.m_par.AsyncDepth = encpak.enc.m_par.AsyncDepth;
    encpak.pak.m_par.IOPattern = encpak.enc.m_par.IOPattern;

    num_fields = (encpak.enc.m_par.mfx.FrameInfo.PicStruct==MFX_PICSTRUCT_PROGRESSIVE)? 1 : 2;

    encpak.PrepareInitBuffers();

    memset(&extOpt, 0, sizeof(extOpt));
    extOpt.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    extOpt.Header.BufferSz = sizeof(mfxExtCodingOption);
    extOpt.AUDelimiter = 0;
    encpak.pak.initbuf.push_back(&extOpt.Header);

    memset(&bufHang, 0, sizeof(bufHang));
    bufHang.BufferId = MFX_EXTBUFF_GPU_HANG;
    bufHang.BufferSz = sizeof(mfxExtBuffer);

    encpak.Init();
    g_tsStatus.check();

    encpak.AllocBitstream((encpak.enc.m_par.mfx.FrameInfo.Width *
                          encpak.enc.m_par.mfx.FrameInfo.Height) * 16
                          * n_frames + 1024*1024);
    g_tsStatus.check();

    g_tsStatus.disable();
    for(count=0; (count<n_frames)&&(sts==MFX_ERR_NONE); count++)
    {
        encpak.PrepareFrameBuffers(false);

        encpak.pak.inbuf.push_back(&bufHang);

        sts = encpak.EncodeFrame(false);
        if(sts != MFX_ERR_NONE)
            break;
    }

    g_tsStatus.expect(MFX_ERR_GPU_HANG);
    g_tsStatus.enable();
    g_tsStatus.check(sts);

    g_tsLog << count << " FRAMES Encoded Before GPU hang.\n";

    TS_END;
    return 0;
}

int FEIPakTest    (unsigned int) { return test(MFX_CODEC_AVC); }

TS_REG_TEST_SUITE(fei_pak_gpu_hang, FEIPakTest, 1);
}
