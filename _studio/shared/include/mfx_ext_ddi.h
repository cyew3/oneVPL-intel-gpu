//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2018 Intel Corporation. All Rights Reserved.
//

#ifndef MFX_EXT_DDI_H
#define MFX_EXT_DDI_H

#define DXVA2_GET_STATUS_REPORT 0x07

#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC) || defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE)
// KMD Event prototype
// D3DDDIFMT_INTELENCODE_SYNCOBJECT
//typedef struct tagGPU_SYNC_EVENT_HANDLE
//{
//    HANDLE   gpuSyncEvent;
//} GPU_SYNC_EVENT_HANDLE, *PGPU_SYNC_EVENT_HANDLE;

enum GPU_COMPONENT_ID
{
    GPU_COMPONENT_VP,
    GPU_COMPONENT_CM,
    GPU_COMPONENT_DECODE,
    GPU_COMPONENT_ENCODE,
    MAX_GPU_COMPONENT_IDS
};

typedef struct _GPU_SYNC_EVENT_HANDLE
{
    uint8_t         m_gpuComponentId;   //GPU_COMPONENT_ID
    HANDLE          gpuSyncEvent;
} GPU_SYNC_EVENT_HANDLE, *PGPU_SYNC_EVENT_HANDLE;

#define DXVA2_PRIVATE_SET_GPU_TASK_EVENT_HANDLE   0x11

#endif

#endif //MFX_EXT_DDI_H
