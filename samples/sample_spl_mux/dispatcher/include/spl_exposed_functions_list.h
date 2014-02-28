//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//

FUNCTION(retMfxSpl, mfxStatus, MFXSplitter_GetInfo, (mfxSplitter _0, mfxStreamParams *par), (((retMfxSpl*)_0)->spl, par))
FUNCTION(retMfxSpl, mfxStatus, MFXSplitter_GetBitstream, (mfxSplitter _0, mfxU32 *track_num, mfxBitstream *bs), (((retMfxSpl*)_0)->spl, track_num, bs))
FUNCTION(retMfxSpl, mfxStatus, MFXSplitter_ReleaseBitstream, (mfxSplitter _0, mfxBitstream *bs), (((retMfxSpl*)_0)->spl, bs))
FUNCTION(retMfxSpl, mfxStatus, MFXSplitter_Seek, (mfxSplitter _0, mfxU64 timestamp), (((retMfxSpl*)_0)->spl, timestamp))
