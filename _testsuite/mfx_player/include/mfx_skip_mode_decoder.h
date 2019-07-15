/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_iyuv_source.h"

//after init this class Setskipmode bypassing special features thru standarad Mediasdk API
class SkipModesSeterDecoder
    : public InterfaceProxy<IYUVSource>
{
    typedef InterfaceProxy<IYUVSource> base;
    std::vector<mfxI32> m_skipModes;

public:
    SkipModesSeterDecoder(std::vector<mfxI32>& nSkipModes, std::unique_ptr<IYUVSource> &&pBase)
        : base(std::move(pBase))
        , m_skipModes(nSkipModes)
    {
    }

    virtual mfxStatus Init(mfxVideoParam *par)
    {
        MFX_CHECK_STS(base::Init(par));

        for (size_t i = 0; i < m_skipModes.size(); i++)
        {
            MFX_CHECK_STS(base::SetSkipMode((mfxSkipMode) m_skipModes[i]));
        }

        return MFX_ERR_NONE;
    }
};
