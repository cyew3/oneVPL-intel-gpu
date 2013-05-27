/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2012 Intel Corporation. All Rights Reserved.

File Name: btest_allocator.cpp

/* ****************************************************************************** */

#if defined(_WIN32) || defined(_WIN64)
#include <d3d9.h>
#include <d3d11.h>
#include <dxva2api.h>
#include <objbase.h>
#include <atlbase.h>
#include <initguid.h>
#endif

#include <memory>
#include <vector>

#include <mfxvideo.h>
#include <mfxstructurespro.h>
#include <mfxvp8.h>
#include <mfxsvc.h>
#include <mfxmvc.h>
#include <mfxjpeg.h>
#include <vm_file.h>

#include "bs_parser++.h"
#include "msdk_ts.h"
#include "test_trace.h"