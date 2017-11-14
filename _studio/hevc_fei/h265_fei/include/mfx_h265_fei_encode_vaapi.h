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

#include "mfxfeihevc.h"
#include "mfx_h265_encode_vaapi.h"
#include "mfx_h265_encode_hw_utils.h"

#include <va/va_fei_hevc.h>

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

namespace MfxHwH265FeiEncode
{

    template <typename T> mfxExtBuffer* GetBufById(T & par, mfxU32 id)
    {
        if (par.NumExtParam && !par.ExtParam)
            return NULL;

        for (mfxU16 i = 0; i < par.NumExtParam; ++i)
        {
            if (par.ExtParam[i] && par.ExtParam[i]->BufferId == id)
            {
                return par.ExtParam[i];
            }
        }

        return NULL;
    }

    template <typename T> mfxExtBuffer* GetBufById(T * par, mfxU32 id)
    {
        return par ? GetBufById(*par, id) : NULL;
    }

    enum VA_BUFFER_STORAGE_ID
    {
        VABID_FEI_FRM_CTRL = MfxHwH265Encode::VABuffersHandler::VABID_END_OF_LIST + 1
    };

    class VAAPIh265FeiEncoder : public MfxHwH265Encode::VAAPIEncoder
    {
    public:
        VAAPIh265FeiEncoder();
        virtual ~VAAPIh265FeiEncoder();

        void SoftReset(const MfxHwH265Encode::MfxVideoParam& par)
        {
            m_videoParam.m_pps.cu_qp_delta_enabled_flag =
                m_pps.pic_fields.bits.cu_qp_delta_enabled_flag = par.m_pps.cu_qp_delta_enabled_flag;

            DDIHeaderPacker::ResetPPS(m_videoParam);
        }

    protected:
        virtual mfxStatus PreSubmitExtraStage(MfxHwH265Encode::Task const & task);

        virtual mfxStatus PostQueryExtraStage();

        virtual VAEntrypoint GetVAEntryPoint()
        {
            return VAEntrypointFEI;
        }

        virtual mfxStatus ConfigureExtraVAattribs(std::vector<VAConfigAttrib> & attrib);

        virtual mfxStatus CheckExtraVAattribs(std::vector<VAConfigAttrib> & attrib);
    };
}

#endif