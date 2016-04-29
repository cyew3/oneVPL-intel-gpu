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

#include "mfx_samples_config.h"

//////////////////////////////////////////////////////////////////////////
//
// Helpers.cpp : Miscellaneous helpers.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//
//////////////////////////////////////////////////////////////////////////

#include "EVRPresenter.h"


//-----------------------------------------------------------------------------
// SamplePool class
//-----------------------------------------------------------------------------

SamplePool::SamplePool() : m_bInitialized(FALSE), m_cPending(0)
{

}

SamplePool::~SamplePool()
{
}


//-----------------------------------------------------------------------------
// GetSample
//
// Gets a sample from the pool. If no samples are available, the method
// returns MF_E_SAMPLEALLOCATOR_EMPTY.
//-----------------------------------------------------------------------------

HRESULT SamplePool::GetSample(IMFSample **ppSample)
{
    AutoLock lock(m_lock);

    if (!m_bInitialized)
    {
        return MF_E_NOT_INITIALIZED;
    }

    if (m_VideoSampleQueue.IsEmpty())
    {
        return MF_E_SAMPLEALLOCATOR_EMPTY;
    }

    HRESULT hr = S_OK;
    IMFSample *pSample = NULL;

    // Get a sample from the allocated queue.

    // It doesn't matter if we pull them from the head or tail of the list,
    // but when we get it back, we want to re-insert it onto the opposite end.
    // (see ReturnSample)

    CHECK_HR(hr = m_VideoSampleQueue.RemoveFront(&pSample));

    m_cPending++;

    // Give the sample to the caller.
    *ppSample = pSample;
    (*ppSample)->AddRef();

done:
    SAFE_RELEASE(pSample);
    return hr;
}

//-----------------------------------------------------------------------------
// ReturnSample
//
// Returns a sample to the pool.
//-----------------------------------------------------------------------------

HRESULT SamplePool::ReturnSample(IMFSample *pSample)
{
    AutoLock lock(m_lock);

    if (!m_bInitialized)
    {
        return MF_E_NOT_INITIALIZED;
    }

    HRESULT hr = S_OK;

    CHECK_HR(hr = m_VideoSampleQueue.InsertBack(pSample));

    m_cPending--;

done:
    return hr;
}

//-----------------------------------------------------------------------------
// AreSamplesPending
//
// Returns TRUE if any samples are in use.
//-----------------------------------------------------------------------------

BOOL SamplePool::AreSamplesPending()
{
    AutoLock lock(m_lock);

    if (!m_bInitialized)
    {
        return FALSE;
    }

    return (m_cPending > 0);
}


//-----------------------------------------------------------------------------
// Initialize
//
// Initializes the pool with a list of samples.
//-----------------------------------------------------------------------------

HRESULT SamplePool::Initialize(VideoSampleList& samples)
{
    AutoLock lock(m_lock);

    if (m_bInitialized)
    {
        return MF_E_INVALIDREQUEST;
    }

    HRESULT hr = S_OK;
    IMFSample *pSample = NULL;

    // Move these samples into our allocated queue.
    VideoSampleList::POSITION pos = samples.FrontPosition();
    while (pos != samples.EndPosition())
    {
        CHECK_HR(hr = samples.GetItemPos(pos, &pSample));
        CHECK_HR(hr = m_VideoSampleQueue.InsertBack(pSample));

        pos = samples.Next(pos);
        SAFE_RELEASE(pSample);
    }

    m_bInitialized = TRUE;

done:
    samples.Clear();

    SAFE_RELEASE(pSample);
    return hr;
}


//-----------------------------------------------------------------------------
// Clear
//
// Releases all samples.
//-----------------------------------------------------------------------------

HRESULT SamplePool::Clear()
{
    HRESULT hr = S_OK;

    AutoLock lock(m_lock);

    m_VideoSampleQueue.Clear();
    m_bInitialized = FALSE;
    m_cPending = 0;
    return S_OK;
}

