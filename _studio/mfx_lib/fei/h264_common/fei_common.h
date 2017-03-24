//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_H264_FEI_COMMON_H_
#define _MFX_H264_FEI_COMMON_H_

#include "mfx_common.h"

#if defined(MFX_VA) && defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && (defined(MFX_ENABLE_H264_VIDEO_FEI_ENC) || defined(MFX_ENABLE_H264_VIDEO_FEI_PAK))

#include "mfxfei.h"

#include "mfxpak.h"
#include "mfxenc.h"

#include "mfx_session.h"
#include "mfx_task.h"
#include "libmfx_core.h"
#include "libmfx_core_hw.h"
#include "libmfx_core_interface.h"
#include "mfx_ext_buffers.h"

#include "mfx_h264_encode_hw.h"
#include "mfx_enc_common.h"
#include "mfx_h264_enc_common_hw.h"
#include "mfx_h264_encode_hw_utils.h"
#include "umc_va_dxva2_protected.h"

#include "mfx_ext_buffers.h"

#include "mfx_h264_encode_vaapi.h"

#include <memory>
#include <list>
#include <vector>
#include <algorithm>
#include <numeric>

namespace MfxH264FEIcommon
{
#if MFX_VERSION >= 1023

    template <typename T>
    void ConfigureTaskFEI(
        MfxHwH264Encode::DdiTask             & task,
        MfxHwH264Encode::DdiTask       const & prevTask,
        MfxHwH264Encode::MfxVideoParam const & video,
        T * inParams);
#else
    template <typename T, typename U>
    void ConfigureTaskFEI(
        MfxHwH264Encode::DdiTask             & task,
        MfxHwH264Encode::DdiTask       const & prevTask,
        MfxHwH264Encode::MfxVideoParam const & video,
        T * inParams,
        U * outParams,
        std::map<mfxU32, mfxU32> &      frameOrder_frameNum);
#endif //MFX_VERSION >= 1023

    static void TEMPORAL_HACK_WITH_DPB(
        MfxHwH264Encode::ArrayDpbFrame & dpb,
        mfxMemId                 const * mids,
        std::vector<mfxU32>      const & fo)
    {
        for (mfxU32 i = 0; i < dpb.Size(); ++i)
        {
            // Index of reconstruct surface
            dpb[i].m_frameIdx = mfxU32(std::find(fo.begin(), fo.end(), dpb[i].m_frameOrder) - fo.begin());
            dpb[i].m_midRec   = 0; // mids[dpb[i].m_frameIdx];
        }
    }

    mfxStatus CheckInitExtBuffers(const MfxHwH264Encode::MfxVideoParam & owned_video, const mfxVideoParam & passed_video);

    template <typename T, typename U>
    mfxStatus CheckRuntimeExtBuffers(T* input, U* output, const MfxHwH264Encode::MfxVideoParam & owned_video);

    bool IsRunTimeInputExtBufferIdSupported(const MfxHwH264Encode::MfxVideoParam & owned_video, mfxU32 id);
    bool IsRunTimeOutputExtBufferIdSupported(const MfxHwH264Encode::MfxVideoParam & owned_video, mfxU32 id);

#if MFX_VERSION >= 1023
    template <typename T, typename U>
    mfxStatus CheckDPBpairCorrectness(T* input, U* output, mfxExtFeiPPS* extFeiPPSinRuntime, const MfxHwH264Encode::MfxVideoParam & owned_video);

    mfxStatus CheckOneDPBCorrectness(mfxExtFeiPPS::mfxExtFeiPpsDPB* DPB, const MfxHwH264Encode::MfxVideoParam & owned_video, bool is_IDR_field = false);
#endif // MFX_VERSION >= 1023
};

#endif
#endif // _MFX_H264_FEI_COMMON_H_
