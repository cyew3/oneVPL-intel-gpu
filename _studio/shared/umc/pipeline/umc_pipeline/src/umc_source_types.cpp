//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#include "umc_source_types.h"

namespace UMC
{

SourceInfo::SourceInfo()
{
    m_cName[0] = 0;
} //SourceInfo::SourceInfo()

bool SourceInfo::Load(Ipp8u *lpbBuffer, size_t nSize)
{
    // loading formatted string
    // "SourceName"0

    // check error(s)
    if ((NULL == lpbBuffer) || (1 >= nSize) || (MAXIMUM_PATH < nSize))
        return false;

    // load setting(s)
    memcpy(m_cName, lpbBuffer, nSize);
    // error protection
    m_cName[nSize - 1] = 0;

    return true;
} //bool SourceInfo::Load(Ipp8u *lpbBuffer, size_t nSize)

bool SourceInfo::Save(Ipp8u *lpbBuffer, size_t &nSize)
{
    size_t nNeed;

    nNeed = vm_string_strlen(m_cName) + 1;
    // check error(s)
    if ((NULL == lpbBuffer) || (nNeed > nSize))
    {
        nSize = nNeed;
        return false;
    }

    // save setting(s)
    memcpy(lpbBuffer, m_cName, nNeed);
    nSize = nNeed;

    return true;
} //bool SourceInfo::Save(Ipp8u *lpbBuffer, size_t &nSize)

SourceInfo *SourceInfoFile::Clone(void)
{
    SourceInfoFile *lpReturn = new SourceInfoFile(*this);

    return lpReturn;
} //SourceInfo *SourceInfoFile::Clone(void)

SourceInfo *SourceInfoNet::Clone(void)
{
    SourceInfoNet *lpReturn = new SourceInfoNet(*this);

    return lpReturn;
} //SourceInfo *SourceInfoNet::Clone(void)

bool SourceInfoNet::Load(Ipp8u *lpbBuffer, size_t nSize)
{
    // loading formatted string
    // "RemoteServerName:port_number"

    // check error(s)
    if ((NULL == lpbBuffer) || (1 + sizeof(m_nPortNumber)+ 1 >= nSize) || (MAXIMUM_PATH + sizeof(m_nPortNumber) + 1 < nSize))
        return false;

    // load setting(s)
    memcpy(m_cName, lpbBuffer, nSize - 1 - sizeof(m_nPortNumber) - 1);
    m_nPortNumber = vm_string_atol((const vm_char *) (lpbBuffer + nSize - sizeof(m_nPortNumber) - 1));
    // error protection
    m_cName[nSize - 1 - 1 - sizeof(m_nPortNumber) - 1] = 0;

    return true;
} //bool SourceInfoNet::Load(Ipp8u *lpbBuffer, size_t nSize)

bool SourceInfoNet::Save(Ipp8u *lpbBuffer, size_t &nSize)
{
    // loading formatted string
    // "RemoteServerName:port_number"

    size_t nNeed, nLen;

    nLen = vm_string_strlen(m_cName);
    nNeed = nLen + 1 + sizeof(m_nPortNumber) + 1;
    // check error(s)
    if ((NULL == lpbBuffer) || (nNeed > nSize))
    {
        nSize = nNeed;
        return false;
    }

    vm_string_sprintf((vm_char *) lpbBuffer, VM_STRING("%s:%08d"), m_cName, m_nPortNumber);
    nSize = nNeed;

    return true;
} //bool SourceInfoNet::Save(Ipp8u *lpbBuffer, size_t &nSize)

SourceInfo *SourceInfoCam::Clone(void)
{
    SourceInfoCam *lpReturn = new SourceInfoCam(*this);

    return lpReturn;
}//SourceInfo *SourceInfoCam::Clone(void)

SourceInfo *SourceInfoWebCam::Clone(void)
{
    SourceInfoWebCam *lpReturn = new SourceInfoWebCam(*this);

    return lpReturn;
} //SourceInfo *SourceInfoCam::Clone(void)

bool SourceInfoWebCam::Load(Ipp8u *lpbBuffer, size_t nSize)
{
    // loading formatted string
    // "RemoteServerName:port_number"

    // check error(s)
    size_t nNeed;

    nNeed = sizeof(m_uiResolutionX) + 1 +
            sizeof(m_uiResolutionY) + 1 +
            sizeof(m_uiBitrate)     + 1 +
            sizeof(m_fFramerate)    + 1;

    if ((NULL == lpbBuffer) || nNeed >= nSize)
        return false;

    return true;
} //bool SourceInfoNet::Load(Ipp8u *lpbBuffer, size_t nSize)

bool SourceInfoWebCam::Save(Ipp8u *lpbBuffer, size_t &nSize)
{
    // loading formatted string
    // "RemoteServerName:port_number"

    size_t nNeed;

    nNeed = sizeof(m_uiResolutionX) + 1 +
            sizeof(m_uiResolutionY) + 1 +
            sizeof(m_uiBitrate)     + 1 +
            sizeof(m_fFramerate)    + 1;
    // check error(s)
    if ((NULL == lpbBuffer) || (nNeed > nSize))
    {
        nSize = nNeed;
        return false;
    }

    vm_string_sprintf((vm_char *) lpbBuffer, VM_STRING("%d:%d:%d:%f:"), m_uiResolutionX,
                      m_uiResolutionY,
                      m_uiBitrate,
                      m_fFramerate);
    nSize = nNeed;

    return true;
} //bool SourceInfoNet::Save(Ipp8u *lpbBuffer, size_t &nSize)

} //end namespace UMC
