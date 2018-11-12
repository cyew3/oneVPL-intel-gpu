// Copyright (c) 2007-2018 Intel Corporation
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

#include "umc_base_codec.h"
#ifndef OPEN_SOURCE
#include "umc_default_memory_allocator.h"
#endif // OPEN_SOURCE

using namespace UMC;

// Default constructor
BaseCodecParams::BaseCodecParams(void)
{
    m_SuggestedOutputSize = 0;
    m_SuggestedInputSize = 0;
    lpMemoryAllocator = 0;
    numThreads = 0;

    m_pData=NULL;

    profile = 0;
    level = 0;
}

// Constructor
BaseCodec::BaseCodec(void)
{
    m_pMemoryAllocator = 0;
    m_bOwnAllocator = false;
}

// Destructor
BaseCodec::~BaseCodec(void)
{
  BaseCodec::Close();
}

// Initialize codec with specified parameter(s)
// Has to be called if MemoryAllocator interface is used
Status BaseCodec::Init(BaseCodecParams *init)
{
  if (init == 0)
    return UMC_ERR_NULL_PTR;
#ifndef OPEN_SOURCE
  // care about reentering as well
  if (init->lpMemoryAllocator) {
    if (m_bOwnAllocator && m_pMemoryAllocator != init->lpMemoryAllocator) {
      vm_debug_trace(VM_DEBUG_ERROR, VM_STRING("can't replace external allocator\n"));
      return UMC_ERR_INIT;
    }
    m_pMemoryAllocator = init->lpMemoryAllocator;
    m_bOwnAllocator = false;
  } else {
    if (m_pMemoryAllocator != 0 && !m_bOwnAllocator) {
      vm_debug_trace(VM_DEBUG_ERROR, VM_STRING("can't replace external allocator\n"));
      return UMC_ERR_INIT;
    }
    if (m_pMemoryAllocator == 0)
      m_pMemoryAllocator = new DefaultMemoryAllocator;
    m_bOwnAllocator = true;
  }
#else
  m_pMemoryAllocator = init->lpMemoryAllocator;
#endif // OPEN_SOURCE
  return UMC_OK;
}

// Close all codec resources
Status BaseCodec::Close(void)
{
#ifndef OPEN_SOURCE
    if ( m_bOwnAllocator && m_pMemoryAllocator != 0 )
    {
      delete m_pMemoryAllocator;
      m_bOwnAllocator = false;
      m_pMemoryAllocator = 0;
    }
#endif // OPEN_SOURCE
    return UMC_OK;
}

