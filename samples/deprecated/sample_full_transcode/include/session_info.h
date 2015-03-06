//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "mfxcommon.h"

// Rationale:
// Audio and video sessions are separate, and always require 2 parameters to be initialized

class MFXSessionInfo {
    mfxIMPL m_impl;
    mfxVersion m_ver;
public:
    MFXSessionInfo(mfxIMPL impl = MFX_IMPL_SOFTWARE, mfxVersion ver = mfxVersion())
        : m_impl(impl)
        , m_ver(ver) {}
    mfxIMPL IMPL() const {
        return m_impl;
    }
    mfxVersion Version() const {
        return m_ver;
    }
    mfxIMPL& IMPL()  {
        return m_impl;
    }
    mfxVersion& Version()  {
        return m_ver;
    }
};
