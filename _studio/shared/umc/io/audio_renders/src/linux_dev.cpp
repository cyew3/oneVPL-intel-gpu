// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/*
    This file contains class covering Linux /dev situated devices interface
*/

#ifdef LINUX32

#include "linux_dev.h"
#include "vm_debug.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <vm_debug.h>

UMC::Status UMC::LinuxDev::Init(vm_char* szDevName, Ipp32s iMode)
{
    Status umcRes = UMC_OK;

    VM_ASSERT(NULL != szDevName);
    Close();

    m_iHandle = open(szDevName, iMode);
    if (-1 == m_iHandle) {
        vm_debug_trace1(VM_DEBUG_ALL, __VM_STRING("LinuxDev::Init: %s device open error\n"), szDevName);
        umcRes = UMC_ERR_OPEN_FAILED;
    }
    else
    {   m_iMode = iMode;    }
    return umcRes;
}

UMC::Status UMC::LinuxDev::Read(Ipp8u* pbData, Ipp32u uiDataSize, Ipp32u* puiReadSize)
{
    Status umcRes = UMC_OK;
    VM_ASSERT(-1 != m_iHandle);
    VM_ASSERT(NULL != pbData);
    VM_ASSERT(0 != uiDataSize);

    if (NULL != puiReadSize)
    {   *puiReadSize = 0;   }

    ssize_t readRes = read(m_iHandle, pbData, uiDataSize);

    switch (readRes) {
    case 0:
        umcRes = UMC_ERR_END_OF_STREAM;
        break;
    case -1:
        umcRes = UMC_ERR_FAILED;
        vm_debug_trace(VM_DEBUG_ALL, __VM_STRING("LinuxDev::Read: read error\n"));
        break;
    default:
        if (NULL != puiReadSize)
        {   *puiReadSize = readRes; }
    }
    return umcRes;
}

UMC::Status UMC::LinuxDev::Write(Ipp8u* pbBuffer, Ipp32u uiBufSize, Ipp32u* puiWroteSize)
{
    Status umcRes = UMC_OK;
    VM_ASSERT(-1 != m_iHandle);
    VM_ASSERT(NULL != pbBuffer);
    VM_ASSERT(0 != uiBufSize);

    if (NULL != puiWroteSize)
    {   *puiWroteSize = 0;   }

    ssize_t readRes = write(m_iHandle, pbBuffer, uiBufSize);

    if (-1 == readRes) {
        umcRes = UMC_ERR_FAILED;
        vm_debug_trace(VM_DEBUG_ALL, __VM_STRING("LinuxDev::Read: read error\n"));
    } else
        if (NULL != puiWroteSize)
        {   *puiWroteSize = readRes; }
    return umcRes;
}

UMC::Status UMC::LinuxDev::Ioctl(Ipp32s iReqCode)
{
    Status umcRes = UMC_OK;
    VM_ASSERT(-1 != m_iHandle);

    Ipp32s iRes = ioctl(m_iHandle, iReqCode);
    if (-1 == iRes) {   umcRes = UMC_ERR_FAILED;    }
    return umcRes;
}

UMC::Status UMC::LinuxDev::Ioctl(Ipp32s iReqCode, void* uiParam)
{
    Status umcRes = UMC_OK;
    VM_ASSERT(-1 != m_iHandle);

    Ipp32s iRes = ioctl(m_iHandle, iReqCode, uiParam);
    if (-1 == iRes) {   umcRes = UMC_ERR_FAILED;    }
    return umcRes;
}

void UMC::LinuxDev::Close()
{
    if (-1 != m_iHandle) {
        close(m_iHandle);
        m_iHandle = -1;
        m_iMode = 0;
    }
}

#endif // LINUX32
