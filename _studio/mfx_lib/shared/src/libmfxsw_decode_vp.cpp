// Copyright (c) 2020-2021 Intel Corporation
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

#include <mfxvideo.h>

#if defined(MFX_ONEVPL)

mfxStatus MFXVideoDECODE_VPP_Init(mfxSession session, mfxVideoParam* decode_par, mfxVideoChannelParam** vpp_par_array, mfxU32 num_channel_par)
{
    return MFX_ERR_NOT_IMPLEMENTED;
}

mfxStatus MFXVideoDECODE_VPP_DecodeFrameAsync(mfxSession session, mfxBitstream* bs, mfxU32* skip_channels, mfxU32 num_skip_channels, mfxSurfaceArray** surf_array_out)
{
    return MFX_ERR_NOT_IMPLEMENTED;
}

mfxStatus MFXVideoDECODE_VPP_Reset(mfxSession /*session*/, mfxVideoParam* /*decode_par*/, mfxVideoChannelParam** /*vpp_par_array*/, mfxU32 /*num_channel_par*/)
{
    return MFX_ERR_NOT_IMPLEMENTED;
}

mfxStatus MFXVideoDECODE_VPP_Close(mfxSession session)
{
    return MFX_ERR_NOT_IMPLEMENTED;
}

mfxStatus MFXVideoDECODE_VPP_GetChannelParam(mfxSession /*session*/, mfxVideoChannelParam* /*par*/, mfxU32 /*channel_id*/)
{
    return MFX_ERR_NOT_IMPLEMENTED;
}

#endif //#if MFX_ONEVPL
