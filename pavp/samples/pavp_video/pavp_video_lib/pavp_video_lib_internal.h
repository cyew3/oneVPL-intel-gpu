/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2012 Intel Corporation. All Rights Reserved.
//
*/
#ifndef __PAVP_VIDEO_LIB_INTERNAL__
#define __PAVP_VIDEO_LIB_INTERNAL__

#include "pavpsdk_pavpsession.h"
#include "intel_pavp_epid_api.h" //PAVPPathId

/** Invalidate decoding stream key inside PAVP session.
For security reasons application must invalidate key when finished using it. 
@param session PAVP session key was created for.
@param pathId Specify which media path (audio or video) to invalidate key for.
@param streamId Stream ID for given path ID to invalidate key for.
*/
PavpEpidStatus CPAVPSession_invalidateDecodingStreamKey(
    CPAVPSession *session, 
    PAVPPathId pathId, 
    uint32 streamId);


/** Request decoding stream key for PAVP session.
PAVP device will generate key for given pathId and streamId. Key generated can 
be used then to encrypt data for PAVP sesson.
@param[in] session PAVP session to create key for.
@param[in] pathId Media path (audio or video) to create key for.
@param[in] streamId Stream ID for given path ID to create key for.
@param[out] streamKey Pointer to the buffer recieving stream key. Buffer size
    is dependent from PAVP session implementation.
*/
PavpEpidStatus CPAVPSession_getDecodingStreamKey(
    CPAVPSession *session, 
    PAVPPathId pathId, 
    uint32 streamId, 
    uint8 *streamKey);


/** Request video transcoding stream keys for PAVP session.
PAVP device will generate keys. Key generated can be used then to 
encrypt and decrypt data for PAVP sesson.
@param[in] session PAVP session to create key for.
@param[out] decodeStreamKey Pointer to the buffer recieving decoder stream key. 
    Buffer size is dependent from PAVP session implementation.
@param[out] encodeStreamKey Pointer to the buffer recieving encoder stream key. 
    Buffer size is dependent from PAVP session implementation.
*/
PavpEpidStatus CPAVPSession_getVideoTranscodingStreamKeys(
    CPAVPSession *session, 
    uint8 *decodeStreamKey, 
    uint8 *encodeStreamKey);


#endif//__PAVP_VIDEO_LIB_INTERNAL__