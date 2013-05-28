/* ****************************************************************************** *\

 INTEL CORPORATION PROPRIETARY INFORMATION
 This software is supplied under the terms of a license agreement or nondisclosure
 agreement with Intel Corporation and may not be copied or disclosed except in
 accordance with the terms of that agreement
 Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __VAAPI_EXT_INTERFACE_H__
#define __VAAPI_EXT_INTERFACE_H__

#define VAAPI_DRIVER_VPG

// Misc parameter for encoder
#define VAEncMiscParameterTypePrivate -2

typedef struct _VAEncMiscParameterPrivate
{
    unsigned int target_usage; // Valid values 1-7 for AVC & MPEG2.
    unsigned int reserved[7];  // Reserved for future use.
} VAEncMiscParameterPrivate;

#endif
