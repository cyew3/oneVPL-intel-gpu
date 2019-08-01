// Copyright (c) 2014-2019 Intel Corporation
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

#ifndef _MFX_LOWPOWER_LOOKAHEAD_H_
#define _MFX_LOWPOWER_LOOKAHEAD_H_

#if defined (MFX_ENABLE_LP_LOOKAHEAD)

namespace MfxHwH265Encode
{
    class MFXVideoENCODEH265_HW;
}
using namespace MfxHwH265Encode;

class MfxLpLookAhead
{
public:
    MfxLpLookAhead(VideoCORE *core) : m_core(core) {}

    virtual ~MfxLpLookAhead() { Close(); }

    virtual mfxStatus Init(mfxVideoParam* param);

    virtual mfxStatus Reset(mfxVideoParam* param);

    virtual mfxStatus Close();

    virtual mfxStatus Submit(mfxFrameSurface1 * surface);

protected:
    bool                   m_bInitialized = false;
    VideoCORE*             m_core         = nullptr;
    MFXVideoENCODEH265_HW* m_pEnc         = nullptr;
    mfxBitstream           m_bitstream    = {};
};

#endif

#endif // !_MFX_LOWPOWER_LOOKAHEAD_H_