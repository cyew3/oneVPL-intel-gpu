// Copyright (c) 2018-2019 Intel Corporation
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

#ifndef MFX_EXT_DDI_H
#define MFX_EXT_DDI_H

#define DXVA2_GET_STATUS_REPORT 0x07

#if defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC) || defined(MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE)

enum GPU_COMPONENT_ID
{
    GPU_COMPONENT_VP,
    GPU_COMPONENT_CM,
    GPU_COMPONENT_DECODE,
    GPU_COMPONENT_ENCODE,
    GPU_COMPONENT_MFE,
    GPU_COMPONENT_DEFAULT,
    MAX_GPU_COMPONENT_IDS
};

#define DXVA2_SET_GPU_TASK_EVENT_HANDLE   0x11

#endif

#define DXVA_DECODE_CONFIG_DOWNSAMPLING_MASK 0x2000

#endif //MFX_EXT_DDI_H
