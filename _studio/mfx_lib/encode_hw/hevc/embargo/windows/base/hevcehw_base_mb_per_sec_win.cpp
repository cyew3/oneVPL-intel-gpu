// Copyright (c) 2019-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && !defined (MFX_VA_LINUX)

#include "hevcehw_base_mb_per_sec_win.h"
#include "libmfx_core_interface.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Base;
using namespace HEVCEHW::Windows;
using namespace HEVCEHW::Windows::Base;

void MbPerSec::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_ENCODER_CAPABILITY].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        auto& src = *(const mfxExtEncoderCapability*)pSrc;
        auto& dst = *(mfxExtEncoderCapability*)pDst;

        dst.MBPerSec = src.MBPerSec;
    });
}

void MbPerSec::Query1WithCaps(const FeatureBlocks& blocks, TPushQ1 Push)
{
    Push(BLK_QueryCORE
        , [&](const mfxVideoParam& in, mfxVideoParam& par, StorageW& global) -> mfxStatus
    {
        mfxExtEncoderCapability* pCaps =  ExtBuffer::Get(par);
        MFX_CHECK(pCaps, MFX_ERR_NONE);
        MFX_CHECK(!global.Contains(Glob::RealState::Key), MFX_ERR_NONE); // ignore Reset

        auto& core = Glob::VideoCore::Get(global);
        MFX_CHECK(core.GetVAType() == MFX_HW_D3D11, MFX_ERR_UNSUPPORTED);

        EncodeHWCaps* pEncodeCaps = QueryCoreInterface<EncodeHWCaps>(&core, MFXIHWMBPROCRATE_GUID);
        MFX_CHECK(pEncodeCaps, MFX_ERR_UNDEFINED_BEHAVIOR);

        mfxU32 mbPerSec[16] = {};
        auto&  guid         = Glob::GUID::Get(global);
        auto   tu           = GetTuIdx(in);

        //check if MbPerSec for particular TU was already queried or need to save
        bool bValid =
            pEncodeCaps->GetHWCaps<mfxU32>(guid, mbPerSec, mfxU32(Size(mbPerSec))) == MFX_ERR_NONE
            && mbPerSec[tu] != 0;

        pCaps->MBPerSec = mbPerSec[tu];

        bool bQueryMode = !(blocks.m_initialized.at(GetID()) & (eFeatureMode::INIT | eFeatureMode::QUERY_IO_SURF));
        ThrowIf(bQueryMode && bValid, MFX_ERR_NONE); //don't run more blocks

        return MFX_ERR_NONE;
    });

    Push(BLK_QueryDDI
        , [&](const mfxVideoParam& in, mfxVideoParam& par, StorageW& global) -> mfxStatus
    {
        mfxExtEncoderCapability* pCaps = ExtBuffer::Get(par);
        MFX_CHECK(pCaps && !pCaps->MBPerSec, MFX_ERR_NONE);
        MFX_CHECK(!global.Contains(Glob::RealState::Key), MFX_ERR_NONE); // ignore Reset

        ENCODE_QUERY_PROCESSING_RATE_INPUT inPar = {};

        // Some encoding parameters affect MB processing rate. Pass them to driver
        inPar.GopPicSize  = par.mfx.GopPicSize;
        inPar.GopRefDist  = mfxU8(par.mfx.GopRefDist);
        inPar.Level       = mfxU8(par.mfx.CodecLevel);
        inPar.Profile     = mfxU8(par.mfx.CodecProfile);
        inPar.TargetUsage = mfxU8(par.mfx.TargetUsage);

        // DDI driver programming notes require to send 0x(ff) for undefined parameters
        SetDefault(inPar.GopPicSize,  0xffff);
        SetDefault(inPar.GopRefDist,  0xff);
        SetDefault(inPar.Level,       0xff);
        SetDefault(inPar.Profile,     0xff);
        SetDefault(inPar.TargetUsage, 0xff);

        mfxU32 mbPerSec[16] = {};
        DDIExecParam ddiPar;

        ddiPar.Function  = ENCODE_QUERY_MAX_MB_PER_SEC_ID;
        ddiPar.In.pData  = &inPar;
        ddiPar.In.Size   = sizeof(inPar);
        ddiPar.Out.pData = mbPerSec;
        ddiPar.Out.Size  = sizeof(mbPerSec);

        auto sts = Glob::DDI_Execute::Get(global)(ddiPar);
        MFX_CHECK_STS(sts);

        pCaps->MBPerSec = mbPerSec[0];

        auto& core        = Glob::VideoCore::Get(global);
        auto& guid        = Glob::GUID::Get(global);
        auto  tu          = GetTuIdx(in);
        auto  pEncodeCaps = QueryCoreInterface<EncodeHWCaps>(&core, MFXIHWMBPROCRATE_GUID);
        MFX_CHECK(pEncodeCaps, MFX_ERR_UNDEFINED_BEHAVIOR);

        std::fill(std::begin(mbPerSec), std::end(mbPerSec), 0);
        pEncodeCaps->GetHWCaps<mfxU32>(guid, mbPerSec, mfxU32(Size(mbPerSec)));

        mbPerSec[tu] = pCaps->MBPerSec;

        sts = pEncodeCaps->SetHWCaps<mfxU32>(guid, mbPerSec, mfxU32(Size(mbPerSec)));
        MFX_CHECK_STS(sts);

        bool bQueryMode = !(blocks.m_initialized.at(GetID()) & (eFeatureMode::INIT | eFeatureMode::QUERY_IO_SURF));
        ThrowIf(bQueryMode, MFX_ERR_NONE); //don't run more blocks

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)