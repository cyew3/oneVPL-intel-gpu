//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "mfx_config.h"
#if defined(MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE)

#include "mfx_h265_encode_vaapi.h"

namespace MfxHwH265FeiEncode
{
    class VAAPIh265FeiEncoder : public MfxHwH265Encode::VAAPIEncoder
    {
    protected:
        virtual mfxStatus PreSubmitExtraStage()
        {
            return MFX_ERR_NONE;
        }

        virtual mfxStatus PostQueryExtraStage()
        {
            return MFX_ERR_NONE;
        }
    };
}

#endif