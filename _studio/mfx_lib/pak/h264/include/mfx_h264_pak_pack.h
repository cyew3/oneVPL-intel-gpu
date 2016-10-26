//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2010 Intel Corporation. All Rights Reserved.
//

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
