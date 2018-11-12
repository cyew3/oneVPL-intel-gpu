// Copyright (c) 2006-2018 Intel Corporation
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

#include "umc_va_base.h"

#if 0  //absent in menlow project
#ifdef UMC_VA_LINUX

#include "umc_va.h"

namespace UMC
{

UMCVAFrameBuffer* VideoAcceleratorExt::GetFrameBuffer(int index) // to get pointer to uncompressed buffer.
{
    if(index<m_NumOfFrameBuffers)
        return &m_FrameBuffers[index];
    else
        return NULL;
}

void* VideoAcceleratorExt::GetCompBuffer(int32_t buffer_type, UMCVACompBuffer **buf, int32_t size, int32_t index)
{
    /*try to find cached buffer*/
    list<VACompBuffer>::iterator i= find( m_CachedCompBuffers.begin(), m_CachedCompBuffers.end(), VACompBuffer(buffer_type, -1, index));
    if(i == m_CachedCompBuffers.end())
    {
        /*get buffer from HW*/
        AutomaticMutex guard(m_mGuard);
        i= find( m_CachedCompBuffers.begin(), m_CachedCompBuffers.end(), VACompBuffer(buffer_type, -1, index));
        if(i == m_CachedCompBuffers.end())
        {
            m_CachedCompBuffers.push_back( GetCompBufferHW(buffer_type, size, index) );
            i = m_CachedCompBuffers.end();
            i--;
            i->SetDataSize(0);
        }
    }

    if(buf != NULL)
        *buf = &(*i);
    return (void*)i->GetPtr();
}

Status VideoAcceleratorExt::EndFrame()
{
    for(list<VACompBuffer>::iterator i = m_CachedCompBuffers.begin(); i != m_CachedCompBuffers.end(); ++i)
        ReleaseBuffer(i->GetType());
    m_CachedCompBuffers.clear();
    return UMC_OK;
}

} //  namespace UMC

#endif // #ifdef UMC_VA_LINUX
#endif
