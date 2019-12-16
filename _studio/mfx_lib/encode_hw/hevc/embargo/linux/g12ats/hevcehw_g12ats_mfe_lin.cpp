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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_MFE) && defined(MFX_VA_LINUX)

#include "hevcehw_g12ats_mfe_lin.h"
#include "hevcehw_g12_data.h"
#include "libmfx_core_interface.h"
#include "hevcehw_g9_va_lin.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen12ATS;

void Linux::Gen12ATS::MFE::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_SetCallChains,
        [this](const mfxVideoParam& in, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        using HEVCEHW::Gen9::Glob;
        using TCall             = Glob::DDI_Execute::TRef::TExt;
        using TCreateContextPar = decltype(TupleArgs(vaCreateContext));
        using TEndPicturePar    = decltype(TupleArgs(vaEndPicture));

        const mfxExtMultiFrameParam* pMfePar = ExtBuffer::Get(in);

        MFX_CHECK(pMfePar && pMfePar->MaxNumFrames > 1, MFX_ERR_NONE);
        MFX_CHECK(!m_bDDIExecSet, MFX_ERR_NONE);

        auto& ddiExec = Glob::DDI_Execute::Get(strg);

        ddiExec.Push(
            [&](TCall prev, const Gen9::DDIExecParam& ep)
        {
            auto sts = prev(ep);
            MFX_CHECK_STS(sts);

            if (ep.Function == Gen9::DDI_VA::VAFID_CreateContext)
            {
                auto& vaPar     = Deref<TCreateContextPar>(ep.In);
                auto& par       = Glob::VideoParam::Get(strg);
                bool  bFields   = (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE);
                auto& fi        = par.mfx.FrameInfo;
                auto& core      = Glob::VideoCore::Get(strg);
                auto* pVideoEncoder
                    = QueryCoreInterface<ComPtrCore<MFEVAAPIEncoder>>(&core, MFXMFEHEVCENCODER_SEARCH_GUID);

                ThrowAssert(!pVideoEncoder, "QueryCoreInterface failed for MFXMFEHEVCENCODER_SEARCH_GUID");

                if (!pVideoEncoder->get())
                    *pVideoEncoder = new MFEVAAPIEncoder;

                m_pMfeAdapter = pVideoEncoder->get();
                MFX_CHECK(m_pMfeAdapter, MFX_ERR_UNSUPPORTED);

                mfxStatus sts = m_pMfeAdapter->Create(ExtBuffer::Get(par), std::get<0>(vaPar));
                MFX_CHECK_STS(sts);

                uint64_t timeout = (mfxU64(fi.FrameRateExtD) * 1000000 / fi.FrameRateExtN) / (bFields + 1);

                sts = m_pMfeAdapter->Join(*std::get<7>(vaPar), timeout);
                MFX_CHECK_STS(sts);
            }

            if (ep.Function == Gen9::DDI_VA::VAFID_EndPicture)
            {
                auto& vaPar = Deref<TEndPicturePar>(ep.In);

                //TODO: implement correct timeout handling from user.
                //TODO: call somewhere else for skip-frames
                sts = m_pMfeAdapter->Submit(std::get<1>(vaPar), m_timeout, false); 
                MFX_CHECK_STS(sts);
            }

            return MFX_ERR_NONE;
        });

        m_bDDIExecSet = true;

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
