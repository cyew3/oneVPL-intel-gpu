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

//
//    This file contains class covering Linux /dev situated devices interface
//

#ifndef __LINUX_DEV_H__
#define __LINUX_DEV_H__

#ifdef LINUX32

#include "umc_structures.h"

namespace UMC
{
    class LinuxDev
    {
    public:
        LinuxDev():m_iHandle(-1), m_iMode(0) {}
        virtual ~LinuxDev() { Close(); }

        Status Init(vm_char* szDevName, Ipp32s iMode);
        Status Read(Ipp8u* pbData, Ipp32u uiDataSize,
                    Ipp32u* puiReadSize);
        Status Write(Ipp8u* pbBuffer, Ipp32u uiBufSize,
                     Ipp32u* puiWroteSize);
        Status Ioctl(Ipp32s iReqCode);
        Status Ioctl(Ipp32s iReqCode, void* uiParam);
        void Close();

    protected:
        Ipp32s m_iHandle;
        Ipp32s m_iMode;
    };
};  //  namespace UMC

#endif // LINUX32

#endif // __LINUX_DEV_H__
