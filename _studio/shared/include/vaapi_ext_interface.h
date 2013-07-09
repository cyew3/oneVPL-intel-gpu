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

// structure for encoder capability
typedef struct _VAEncQueryCapabilities
{
    /* these 2 fields should be preserved over all versions of this structure */
    unsigned int version; /* [in] unique version of the sturcture interface  */
    unsigned int size;    /* [in] size of the structure*/
    union
    {
        struct
        {
            unsigned int SliceStructure    :3; /* 1 - SliceDividerSnb; 2 - SliceDividerHsw; 3 - SliceDividerBluRay; the other - SliceDividerOneSlice  */
            unsigned int NoInterlacedField :1;
        } bits;
        unsigned int CodingLimits;
    } EncLimits;
    unsigned int MaxPicWidth;
    unsigned int MaxPicHeight;
    unsigned int MaxNum_ReferenceL0;
    unsigned int MaxNum_ReferenceL1;
    unsigned int MBBRCSupport;
} VAEncQueryCapabilities;

// VAAPI Extension to support querying encoder capability
typedef VAStatus (*vaExtQueryEncCapabilities)(
    VADisplay                   dpy,
    VAProfile                   profile,
    VAEncQueryCapabilities      *pVaEncCaps);

#define VPG_EXT_QUERY_ENC_CAPS  "vpgExtQueryEncCapabilities"

#endif
