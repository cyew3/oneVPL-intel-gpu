/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "mfx_iyuv_source.h"
#include "mfx_plugin_loader.h"

/*
* Rationale: Decoder plugins aims to load user plugin from given file name, then register in within mediasdk, and
* futher fork as generic mediasdk based decoder
*/
class DecoderPlugin 
    : public MFXDecoder
{
    PluginLoader<MFXDecoderPlugin> m_plg;
public:
    DecoderPlugin(const tstring & plugin_name, mfxSession session)
        : MFXDecoder(session)
        , m_plg(session, plugin_name)
    {
        MFX_CHECK_AND_THROW(m_plg.IsOk());
    }
    virtual ~DecoderPlugin() { 
        Close(); 
        m_session = NULL;
    }
};