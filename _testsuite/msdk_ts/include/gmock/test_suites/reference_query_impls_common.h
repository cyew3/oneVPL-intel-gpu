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
        std::vector<mfxU32>    DeviceList;
    };

    class ReferenceCommon
    {
    public:
        ReferenceCommon()
        {
            snprintf(m_reference.ImplName, sizeof(m_reference.ImplName), "mfx-gen");
        }
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

    class ReferenceCommonTGL : public ReferenceCommon
    {
    public:
        ReferenceCommonTGL()
        {
            m_reference.DeviceList = {
                0x9A40, 0x9A49, 0x9A59, 0x9A60, 0x9A68, 0x9A70, 0x9A78,
                0xFF20, 0x9A09, 0x9A39, 0x9A69, 0x9A79,
                0x4C80, 0x4C8A, 0x4C81, 0x4C8B, 0x4C90, 0x4C9A  // RKL is derivative of TGL
            };
        }
    };
    static ReferenceCommonTGL ref_TGL_common;

    class ReferenceCommonDG1 : public ReferenceCommon
    {
    public:
        ReferenceCommonDG1()
        {
            m_reference.DeviceList = {
                0x4905, 0x4906, 0x4907, 0x4908
            };
        }
    };
    static ReferenceCommonDG1 ref_DG1_common;

    class ReferenceCommonADL : public ReferenceCommon
    {
    public:
        ReferenceCommonADL()
        {
            m_reference.DeviceList = {
                0x4600, 0x4680, 0x4681, 0x4683, 0x4690, 0x4691, 0x4693, 0x4698, 0x4699, 0x46A0,
                0x46A1, 0x46A3, 0x46A6, 0x4626, 0x46B1, 0x46B3, 0x46A8, 0x4628, 0x46C0, 0x46C1,
                0x46C3, 0x46AA, 0x462A
            };
        }
    };
    static ReferenceCommonADL ref_ADL_common;

    class ReferenceCommonATS : public ReferenceCommon
    {
    public:
        ReferenceCommonATS()
        {
            m_reference.DeviceList = {
                0x0201, 0x0202, 0x0203, 0x0204, 0x0205, 0x0206, 0x0207, 0x0208, 0x0209, 0x020A,
                0x020B, 0x020C, 0x020D, 0x020E, 0x020F, 0x0210
            };
        }
    };
    static ReferenceCommonATS ref_ATS_common;

    class ReferenceCommonDG2 : public ReferenceCommon
    {
    public:
        ReferenceCommonDG2()
        {
            m_reference.DeviceList = {
                0x4F80, 0x4F81, 0x4F82, 0x4F83, 0x4F84, 0x4F85, 0x4F86, 0x4F87, 0x4F88, 0x5690,
                0x5691, 0x5692, 0x5693, 0x5694, 0x5695, 0x5696, 0x5697, 0x5698, 0x56A0, 0x56A1,
                0x56A2, 0x56A3, 0x56A4, 0x56A5, 0x56A6, 0x56A7, 0x56A8, 0x56A9, 0x56B0, 0x56B1,
                0x56C0
            };
        }
    };
    static ReferenceCommonDG2 ref_DG2_common;
}
