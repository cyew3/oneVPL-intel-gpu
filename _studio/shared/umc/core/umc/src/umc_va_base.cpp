/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2006-2016 Intel Corporation. All Rights Reserved.
*/

#include "umc_va_base.h"
#include "umc_defs.h"

using namespace UMC;

Status VideoAccelerator::Close(void)
{
    m_allocator = 0;
    return UMC_OK;
}

Status VideoAccelerator::Reset(void)
{
    m_allocator = 0;
    return UMC_OK;
}

void UMCVACompBuffer::SetDataSize(Ipp32s size)
{
    DataSize = size;
    VM_ASSERT(DataSize <= BufferSize);
}

void UMCVACompBuffer::SetNumOfItem(Ipp32s )
{
}

Status UMCVACompBuffer::SetPVPState(void *buf, Ipp32u size)
{
    if (16 < size)
        return UMC_ERR_ALLOC;
    if (NULL != buf)
    {
        if (0 == size)
            return UMC_ERR_ALLOC;
        PVPState = PVPStateBuf;
        MFX_INTERNAL_CPY(PVPState, buf, size);
    }
    else
        PVPState = NULL;

    return UMC_OK;
}

