/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once
#include "mfx_pipeline_defs.h"

#ifdef PAVP_BUILD
#include "pavp_video_lib.h"
#include "mfx_encoder_wrap.h"
class MFXProtectedEncodeWRAPPER: public MFXEncodeWRAPPER
{
public:
    MFXProtectedEncodeWRAPPER(
        CPAVPVideo  *pavpVideo
        , ComponentParams &refParams
        , mfxStatus *status
        , std::auto_ptr<IVideoEncode> &pTargetEncode)
        :MFXEncodeWRAPPER(refParams, status, pTargetEncode)
        ,m_pavpVideo(pavpVideo)
    {}
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out)
    {
        if (NULL != in && NULL != out)
            memcpy(out, in, sizeof(*out)); // workaround for MPEG2 encoder. Queery always fail for protected due to counter type checking switch-case logic
        else
            return MFXEncodeWRAPPER::Query(in, out);
        return MFX_ERR_NONE;
    }
protected:
    virtual mfxStatus InitBitstream(mfxBitstream *bs);
    virtual mfxStatus PutBsData(mfxBitstream* pData);

    CPAVPVideo  *m_pavpVideo;
};
#endif // PAVP_BUILD

