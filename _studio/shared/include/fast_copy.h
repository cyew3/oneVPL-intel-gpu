/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2016 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __FAST_COPY_H__
#define __FAST_COPY_H__

#include "ippcore.h"
#include "umc_event.h"
#include "umc_thread.h"
#include "libmfx_allocator.h"

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
    UMC::Event EventStart;
    UMC::Event EventEnd;
};
enum eFAST_COPY_MODE
{
    FAST_COPY_SSE41         =   0x02,
    FAST_COPY_UNSUPPORTED   =   0x03
};

class FastCopy
{
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
    mfxStatus Copy(mfxU8 *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, IppiSize roi);
#ifndef LINUX64
    // copy memory by streaming with shifting
    mfxStatus Copy(mfxU16 *pDst, mfxU32 dstPitch, mfxU16 *pSrc, mfxU32 srcPitch, IppiSize roi, Ipp8u lshift, Ipp8u rshift);
#endif
    // return supported mode
    virtual eFAST_COPY_MODE GetSupportedMode(void) const;

protected:

   // synchronize threads
    mfxStatus Synchronize(void);

    static mfxU32 __STDCALL CopyByThread(void *object);
    static IppBool m_bCopyQuit;

    // mode
    eFAST_COPY_MODE m_mode;

    // handles
    UMC::Thread *m_pThreads;
    Ipp32u m_numThreads;

    FC_TASK *m_tasks;
};

#endif // __FAST_COPY_H__
