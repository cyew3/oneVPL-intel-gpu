//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2013 Intel Corporation. All Rights Reserved.
//

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

void* VideoAcceleratorExt::GetCompBuffer(Ipp32s buffer_type, UMCVACompBuffer **buf, Ipp32s size, Ipp32s index)
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
