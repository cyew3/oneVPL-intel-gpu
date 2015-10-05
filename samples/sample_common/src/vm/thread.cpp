/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2012-2015 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include <new> // std::bad_alloc

#include "vm/thread_defs.h"

AutomaticMutex::AutomaticMutex(MSDKMutex& mutex):
    m_rMutex(mutex),
    m_bLocked(false)
{
    if (MFX_ERR_NONE != Lock()) throw std::bad_alloc();
};
AutomaticMutex::~AutomaticMutex(void)
{
    Unlock();
}

mfxStatus AutomaticMutex::Lock(void)
{
    mfxStatus sts = MFX_ERR_NONE;
    if (!m_bLocked)
    {
        if (!m_rMutex.Try())
        {
            // add time measurement here to estimate how long you sleep on mutex...
            sts = m_rMutex.Lock();
        }
        m_bLocked = true;
    }
    return sts;
}

mfxStatus AutomaticMutex::Unlock(void)
{
    mfxStatus sts = MFX_ERR_NONE;
    if (m_bLocked)
    {
        sts = m_rMutex.Unlock();
        m_bLocked = false;
    }
    return sts;
}
