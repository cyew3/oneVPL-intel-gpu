/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2006-2014 Intel Corporation. All Rights Reserved.
*/

#ifdef _DEBUG
#ifndef VM_DEBUG
#define VM_DEBUG
#endif
#endif

#include "umc_va_base.h"
#include "umc_defs.h"

#ifndef UMC_RESTRICTED_CODE_VA

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

namespace UMC
{
VideoAccelerationProfile VideoType2VAProfile(VideoStreamType video_type)
{
    switch (video_type)
    {
        case MPEG2_VIDEO: return VA_MPEG2;
        case H261_VIDEO:  return VA_MPEG4;
        case H263_VIDEO:  return VA_MPEG4;
        case MPEG4_VIDEO: return VA_MPEG4;
        case H264_VIDEO:  return VA_H264;
        case HEVC_VIDEO:  return VA_H265;
        case VC1_VIDEO:   return VA_VC1;
        case WMV_VIDEO:   return VA_VC1;
        default:          return UNKNOWN;
    }
}
}

void UMCVACompBuffer::SetDataSize(Ipp32s size)
{
    DataSize = size;
    if (DataSize > BufferSize)
    //if (DataSize)
    {
        vm_trace_i(type);
        vm_trace_i(DataSize);
        vm_trace_i(BufferSize);
        //printf("SetDataSize!!!!!!!!!! %d %d\n", DataSize, BufferSize);
    }
    VM_ASSERT(DataSize <= BufferSize);
#if 0
    if (DataSize > BufferSize)
    {
        //exit(1);
        throw /*SimError(UMC_ERR_FAILED, */"Buffer is too small in UMCVACompBuffer::SetDataSize";
    }
#endif
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

#endif // UMC_RESTRICTED_CODE_VA
