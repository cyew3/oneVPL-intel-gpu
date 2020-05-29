// Copyright (c) 2020 Intel Corporation
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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE) && !defined (MFX_VA_LINUX)

#include "av1ehw_base_alloc_win.h"
#include "ehw_resources_pool_dx11.h"

using namespace AV1EHW;
using namespace AV1EHW::Windows::Base;

void Allocator::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_Init
        , [](StorageRW&, StorageRW& local) -> mfxStatus
    {
        auto CreateAllocator = [](VideoCORE& core) -> AV1EHW::Base::IAllocation*
        {
            if (core.GetVAType() == MFX_HW_D3D11)
                return MakeAlloc(std::unique_ptr<MfxEncodeHW::ResPool>(new MfxEncodeHW::ResPoolDX11(core)));
            return MakeAlloc(std::unique_ptr<MfxEncodeHW::ResPool>(new MfxEncodeHW::ResPool(core)));
        };

        using Tmp = AV1EHW::Base::Tmp;
        local.Insert(Tmp::MakeAlloc::Key, new Tmp::MakeAlloc::TRef(CreateAllocator));

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
