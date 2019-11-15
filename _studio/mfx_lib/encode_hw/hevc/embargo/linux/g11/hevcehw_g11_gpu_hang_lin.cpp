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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_EXTBUFF_GPU_HANG_ENABLE) && defined(MFX_VA_LINUX)

#include "hevcehw_g11_gpu_hang_lin.h"
#include "va/va.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen11;

void HEVCEHW::Linux::Gen11::GPUHang::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_PatchDDITask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto pTriggerHang = ExtBuffer::Get(Task::Common::Get(s_task).ctrl, MFX_EXTBUFF_GPU_HANG);
        MFX_CHECK(pTriggerHang, MFX_ERR_NONE);

        const VABufferType VATriggerCodecHangBufferType = ((VABufferType)-16);
        DDIExecParam xPar;
        xPar.Function = VATriggerCodecHangBufferType;
        xPar.In.pData = &m_triggerHang;
        xPar.In.Size = sizeof(m_triggerHang);
        xPar.In.Num = 1;

        Glob::DDI_SubmitParam::Get(global).push_back(xPar);

        return MFX_ERR_NONE;
    });
}


#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
