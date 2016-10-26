//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2013 Intel Corporation. All Rights Reserved.
//

#include <mfxvideo.h>
#include <mfx_session.h>

mfxStatus MFXQueryIMPL(mfxSession session, mfxIMPL *impl)
{
    mfxIMPL currentImpl;

    // check error(s)
    if (0 == session)
    {
        return MFX_ERR_INVALID_HANDLE;
    }
    if (0 == impl)
    {
        return MFX_ERR_NULL_PTR;
    }

    // set the library's type
#ifdef MFX_VA
    if (0 == session->m_adapterNum)
    {
        currentImpl = MFX_IMPL_HARDWARE;
    }
    else
    {
        currentImpl = (mfxIMPL) (MFX_IMPL_HARDWARE2 + (session->m_adapterNum - 1));
    }
    currentImpl |= session->m_implInterface;
#else
    currentImpl = MFX_IMPL_SOFTWARE;
#endif

    // save the current implementation type
    *impl = currentImpl;

    return MFX_ERR_NONE;

} // mfxStatus MFXQueryIMPL(mfxSession session, mfxIMPL *impl)

mfxStatus MFXQueryVersion(mfxSession session, mfxVersion *pVersion)
{
    if (0 == session)
    {
        return MFX_ERR_INVALID_HANDLE;
    }
    if (0 == pVersion)
    {
        return MFX_ERR_NULL_PTR;
    }

    // set the library's version
    pVersion->Major = MFX_VERSION_MAJOR;
    pVersion->Minor = MFX_VERSION_MINOR;

    return MFX_ERR_NONE;

} // mfxStatus MFXQueryVersion(mfxSession session, mfxVersion *pVersion)
