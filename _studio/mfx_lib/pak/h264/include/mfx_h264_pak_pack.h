// Copyright (c) 2008-2018 Intel Corporation
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

#ifndef _MFX_H264_PAK_PACK_H_
#define _MFX_H264_PAK_PACK_H_

#include "mfx_common.h"
#if defined (MFX_ENABLE_H264_VIDEO_PAK)

#include "umc_h264_bs.h"

struct mfxExtAvcSeiMessage;
struct mfxExtAvcSeiBufferingPeriod;
struct mfxExtAvcSeiPicTiming;

namespace H264Pak
{
    class MbDescriptor;
    union CoeffData;

    class H264PakBitstream
    {
    public:
        H264PakBitstream();
        void StartPicture(const mfxFrameParamAVC& fp, void* buf, mfxU32 size);
        void StartSlice(const mfxFrameParamAVC& fp, const mfxSliceParamAVC& sp);
        void EndSlice();
        void EndPicture();

        void PackAccessUnitDelimiter(
            mfxU32 picType);

        void PackSeqParamSet(
            const mfxVideoParam& vp,
            const mfxFrameParamAVC& fp,
            bool vuiParamPresentFlag);

        void PackPicParamSet(
            const mfxVideoParam& vp,
            const mfxFrameParamAVC& fp);

        void PackSei(
            const mfxExtAvcSeiBufferingPeriod* bufferingPeriod,
            const mfxExtAvcSeiPicTiming* picTiming,
            const mfxExtAvcSeiMessage* formattedSeiMessages);

        void PackSliceHeader(
            const mfxVideoParam& vp,
            const mfxFrameParamAVC& fp,
            const mfxExtAvcRefFrameParam& refFrameParam,
            const mfxSliceParamAVC& sp);

        void PackMb(
            const mfxFrameParamAVC& fp,
            const mfxSliceParamAVC& sp,
            const MbDescriptor& mb,
            const CoeffData& coeffData);

        mfxStatus GetRbsp(mfxBitstream& bs);

    protected:
        void PackMbCabac(
            const mfxFrameParamAVC& fp,
            const mfxSliceParamAVC& sp,
            const MbDescriptor& mb,
            const CoeffData& coeffData);

        void PackMbCavlc(
            const mfxFrameParamAVC& fp,
            const mfxSliceParamAVC& sp,
            const MbDescriptor& mb,
            const CoeffData& coeffData);

        void PackMbHeaderCabac(
            const mfxFrameParamAVC& fp,
            const mfxSliceParamAVC& sp,
            const MbDescriptor& mb);

        void PackMbHeaderCavlc(
            const mfxFrameParamAVC& fp,
            const mfxSliceParamAVC& sp,
            const MbDescriptor& mb);

    private:
        UMC_H264_ENCODER::H264BsReal_8u16s m_bs;
        mfxI32 m_prevMbQpY;
        mfxI32 m_prevMbQpDelta;
        mfxU32 m_skipRun;
    };
};

#endif // MFX_ENABLE_H264_VIDEO_PAK
#endif // _MFX_H264_PAK_PACK_H_
