//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2014 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_VP8_DEC_DECODE_COMMON_H_
#define _MFX_VP8_DEC_DECODE_COMMON_H_

#include "mfx_common.h"

namespace VP8DecodeCommon 
{

    mfxStatus DecodeHeader(VideoCORE *p_core, mfxBitstream *p_bs, mfxVideoParam *p_params);
    void MoveBitstreamData(mfxBitstream& bs, mfxU32 offset);

    typedef struct _IVF_FRAME
    {
        Ipp32u frame_size;
        Ipp64u time_stamp;
        Ipp8u *p_frame_data;

    } IVF_FRAME;

}

class MFX_VP8_Utility
{
public:

    static eMFXPlatform GetPlatform(VideoCORE *pCore, mfxVideoParam *pPar);
    static mfxStatus Query(VideoCORE *pCore, mfxVideoParam *pIn, mfxVideoParam *pOut, eMFXHWType type);
    static bool CheckVideoParam(mfxVideoParam *pIn, eMFXHWType type);

private:

    static bool IsNeedPartialAcceleration(mfxVideoParam * pPar);
};


#endif
