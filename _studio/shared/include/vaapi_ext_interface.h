/* ****************************************************************************** *\

 INTEL CORPORATION PROPRIETARY INFORMATION
 This software is supplied under the terms of a license agreement or nondisclosure
 agreement with Intel Corporation and may not be copied or disclosed except in
 accordance with the terms of that agreement
 Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __VAAPI_EXT_INTERFACE_H__
#define __VAAPI_EXT_INTERFACE_H__

#define VAAPI_DRIVER_VPG

// Misc parameter for encoder
#define  VAEncMiscParameterTypePrivate     -2
// encryption parameters for PAVP
#define  VAEncryptionParameterBufferType   -3

#define EXTERNAL_ENCRYPTED_SURFACE_FLAG   (1<<16)
#define VA_CODED_BUF_STATUS_PRIVATE_DATA_HDCP (1<<8)
#define VA_HDCP_ENABLED (1<<12)

typedef struct _VAEncMiscParameterPrivate
{
    unsigned int target_usage; // Valid values 1-7 for AVC & MPEG2.
    unsigned int reserved[7];  // Reserved for future use.
} VAEncMiscParameterPrivate;

/*VAEncrytpionParameterBuffer*/
typedef struct _VAEncryptionParameterBuffer
{
    //Not used currently
    unsigned int encryptionSupport;
    //Not used currently
    unsigned int hostEncryptMode;
    // For IV, Counter input
    unsigned int pavpAesCounter[2][4];
    // not used currently
    unsigned int pavpIndex;
    // PAVP mode, CTR, CBC, DEDE etc
    unsigned int pavpCounterMode;
    unsigned int pavpEncryptionType;
    // not used currently
    unsigned int pavpInputSize[2];
    // not used currently
    unsigned int pavpBufferSize[2];
    // not used currently
    VABufferID   pvap_buf;
    // set to TRUE if protected media
    unsigned int pavpHasBeenEnabled;
    // not used currently
    unsigned int IntermmediatedBufReq;
    // not used currently
    unsigned int uiCounterIncrement;
    // AppId: PAVP sessin Index from application
    unsigned int app_id;

} VAEncryptionParameterBuffer;

typedef struct _VAHDCPEncryptionParameterBuffer {
    unsigned int       status;
    int                bEncrypted;
    unsigned int       counter[4];
} VAHDCPEncryptionParameterBuffer;

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
