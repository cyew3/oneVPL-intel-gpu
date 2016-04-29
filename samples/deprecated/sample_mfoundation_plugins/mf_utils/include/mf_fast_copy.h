/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#ifndef __FAST_COPY_H__
#define __FAST_COPY_H__

#include "ippcore.h"
#include <vector>
//#include "umc_event.h"
//#include "umc_thread.h"
//#include "libmfx_allocator.h"


enum eFAST_COPY_MODE
{
    FAST_COPY_SSE41         =   0x02,
    FAST_COPY_UNSUPPORTED   =   0x03
};

class FastCopy
{
    struct FC_TASK
    {
        // pointers to source and destination
        Ipp8u *pS;
        Ipp8u *pD;

        // size of chunk to copy
        Ipp32u chunkSize;

        // pitches and frame size
        IppiSize roi;
        Ipp32u srcPitch, dstPitch;

        // event handles
        MyEvent EventStart;
        MyEvent EventEnd;

        IppBool *pbCopyQuit;
    };
public:

    // constructor
    FastCopy(void);

    // destructor
    virtual ~FastCopy(void);

    // initialize available functionality
    mfxStatus Initialize(void);

    // release object
    mfxStatus Release(void);

    // copy memory by streaming
    mfxStatus Copy(mfxU8 *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, const IppiSize &roi);

    // return supported mode
    virtual eFAST_COPY_MODE GetSupportedMode(void) const;

protected:

   // synchronize threads
    mfxStatus Synchronize(void);

    static mfxU32 MY_THREAD_CALLCONVENTION CopyByThread(void *object);
    IppBool m_bCopyQuit;

    // mode
    eFAST_COPY_MODE m_mode;

    // handles
    Ipp32u m_numThreads;
    std::vector<MyThread> m_pThreads;
    std::vector<FC_TASK>m_tasks;
};

#endif // __FAST_COPY_H__
