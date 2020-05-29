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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE) && !defined(MFX_VA_LINUX)

#include "av1ehw_base_dirty_rect_win.h"
#include "av1ehw_base_ddi_packer_win.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;
using namespace AV1EHW::Windows;
using namespace AV1EHW::Windows::Base;

namespace AV1EHW
{
void Windows::Base::DirtyRect::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_PatchDDITask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& par = Glob::DDI_SubmitParam::Get(global);
        auto vaType = Glob::VideoCore::Get(global).GetVAType();

        auto& pps = Deref(GetDDICB<ENCODE_SET_PICTURE_PARAMETERS_AV1>(
            ENCODE_ENC_PAK_ID, DDIPar_In, vaType, par));
        const auto& taskRects = m_taskToRects.at(&s_task);

        pps.NumDirtyRects = 0;
        m_ddiRects.clear();

        for (auto& rect : taskRects)
        {
            if (SkipRectangle(rect))
                continue;

            /* Driver expects a rect with the 'close' right bottom edge but
            MSDK uses the 'open' edge rect, thus the right bottom edge which
            is decreased by 1 below converts 'open' -> 'close' notation
            We expect here boundaries are already aligned with the BlockSize
            and Right > Left and Bottom > Top */
            m_ddiRects.push_back({
                USHORT(rect.Top / AV1_DIRTY_BLOCK_SIZE)
                , USHORT((rect.Bottom / AV1_DIRTY_BLOCK_SIZE) - 1)
                , USHORT(rect.Left / AV1_DIRTY_BLOCK_SIZE)
                , USHORT((rect.Right / AV1_DIRTY_BLOCK_SIZE) - 1)
                });

            ++pps.NumDirtyRects;

            pps.pDirtyRect = pps.NumDirtyRects ? m_ddiRects.data() : nullptr;
        }

        return MFX_ERR_NONE;
    });
}

} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
