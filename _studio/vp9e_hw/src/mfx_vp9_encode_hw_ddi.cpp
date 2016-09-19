/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_vp9_encode_hw_utils.h"
#include "mfx_vp9_encode_hw_ddi.h"

#if defined (AS_VP9E_PLUGIN)

namespace MfxHwVP9Encode
{
    mfxStatus QueryHwCaps(mfxCoreInterface * pCore, ENCODE_CAPS_VP9 & caps, GUID guid)
    {
        std::auto_ptr<DriverEncoder> ddi;

        ddi.reset(CreatePlatformVp9Encoder());
        MFX_CHECK(ddi.get() != NULL, MFX_WRN_PARTIAL_ACCELERATION);

        mfxStatus sts = ddi.get()->CreateAuxilliaryDevice(pCore, guid, 1920, 1088);
        MFX_CHECK_STS(sts);

        sts = ddi.get()->QueryEncodeCaps(caps);
        MFX_CHECK_STS(sts);

        return MFX_ERR_NONE;
    }
} // MfxHwVP9Encode

#endif // AS_VP9E_PLUGIN
