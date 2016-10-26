//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __OUTLINE_MFX_WRAPPER_H__
#define __OUTLINE_MFX_WRAPPER_H__

#include <memory>
#include "mfxvideo++.h"
#include "video_data_checker_mfx.h"

class MFXVideoDECODE_Checker : public MFXVideoDECODE
{
public:

    MFXVideoDECODE_Checker(mfxSession session)
        : MFXVideoDECODE(session)
    {
        // m_checker.reset(new VideoDataChecker_MFX());
    }

    mfxStatus Init(mfxVideoParam *par)
    {
        m_checker->ProcessSequence(par);
        return MFXVideoDECODE_Init(m_session, par);
    }

    mfxStatus DecodeFrameAsync(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
    {
        return MFXVideoDECODE_DecodeFrameAsync(m_session, bs, surface_work, surface_out, syncp);
    }

private:

    std::auto_ptr<VideoDataChecker_MFX> m_checker;

};

#endif // __OUTLINE_MFX_WRAPPER_H__
