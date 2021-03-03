/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2021 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "reference_query_impls_defs.h"

namespace query_impls_description
{
    struct ReferenceCommonImplsDescription {
        mfxStructVersion       Version;
        mfxImplType            Impl;
        mfxAccelerationMode    AccelerationMode;
        mfxVersion             ApiVersion;
        mfxChar                ImplName[MFX_IMPL_NAME_LEN];
        mfxChar                License[MFX_STRFIELD_LEN];
        mfxChar                Keywords[MFX_STRFIELD_LEN];
        mfxU32                 VendorID;
        mfxU32                 VendorImplID;

        mfxChar                DeviceID[MFX_STRFIELD_LEN];
    };

    class ReferenceCommon
    {
    public:
        ReferenceCommonImplsDescription& GetReference()
        {
            return m_reference;
        }
    protected:
        ReferenceCommonImplsDescription m_reference = { {0, 0}, MFX_IMPL_TYPE_HARDWARE,
#if defined(_WIN32) || defined(_WIN64)
            MFX_ACCEL_MODE_VIA_D3D11,
#else
            MFX_ACCEL_MODE_VIA_VAAPI,
#endif
            {0, 2},
            {}, {}, {},
            0x8086, 0, {}
        };
    };
    ReferenceCommon ref_default_common;

    class ReferenceCommonATS : public ReferenceCommon
    {
    public:
        ReferenceCommonATS()
        {
            snprintf(m_reference.DeviceID, sizeof(m_reference.DeviceID), "20a");
        }
    };
    static ReferenceCommonATS ref_ATS_common;

    class ReferenceCommonDG2 : public ReferenceCommon
    {
    public:
        ReferenceCommonDG2()
        {
            snprintf(m_reference.DeviceID, sizeof(m_reference.DeviceID), "4f84");
        }
    };
    static ReferenceCommonDG2 ref_DG2_common;
}
