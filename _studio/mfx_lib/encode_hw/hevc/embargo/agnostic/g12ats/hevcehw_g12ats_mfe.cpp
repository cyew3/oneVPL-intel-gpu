// Copyright (c) 2019 Intel Corporation
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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_MFE)

#include "hevcehw_g12ats_mfe.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen12ATS;

void MFE::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_MULTI_FRAME_PARAM].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        auto& buf_src = *(const mfxExtMultiFrameParam*)pSrc;
        auto& buf_dst = *(mfxExtMultiFrameParam*)pDst;

        MFX_COPY_FIELD(MFMode);
        MFX_COPY_FIELD(MaxNumFrames);
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_MULTI_FRAME_CONTROL].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        auto& buf_src = *(const mfxExtMultiFrameControl*)pSrc;
        auto& buf_dst = *(mfxExtMultiFrameControl*)pDst;

        MFX_COPY_FIELD(Timeout);
        MFX_COPY_FIELD(Flush);
    });
}

void MFE::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [this](const mfxVideoParam&, mfxVideoParam& out, StorageW&) -> mfxStatus
    {
        mfxExtMultiFrameParam* pMFPar = ExtBuffer::Get(out);
        mfxExtMultiFrameControl* pMFCtrl = ExtBuffer::Get(out);
        bool bCheckPar  = pMFPar && pMFPar->MaxNumFrames > 1;
        bool bCheckCtrl = bCheckPar && pMFCtrl;

        if (bCheckPar)
        {
            SetDefault(pMFPar->MFMode, MFX_MF_MANUAL); //only Manual mode for pre-si now
            CheckMaxOrClip(pMFPar->MaxNumFrames, 8);// 8 by default now
        }

        if (bCheckCtrl)
        {
            SetDefault(pMFCtrl->Timeout, mfxU32(10 * 60 * 1000000)); //10 minutes now for pre-si
        }

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)