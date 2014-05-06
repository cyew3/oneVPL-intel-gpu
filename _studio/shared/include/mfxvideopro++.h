/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2014 Intel Corporation. All Rights Reserved.


File Name: mfxvideopro++.h

\* ****************************************************************************** */

#ifndef __MFXVIDEOPROPLUSPLUS_H
#define __MFXVIDEOPROPLUSPLUS_H

#include "mfxvideopro.h"


// MFXVideoENC was moved in mfxvideo++.h

class MFXVideoPAK
{
public:

    MFXVideoPAK(mfxSession session) { m_session = session; }
    virtual ~MFXVideoPAK(void) { Close(); }

    mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) { return MFXVideoPAK_Query(m_session, in, out); }
    mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request) { return MFXVideoPAK_QueryIOSurf(m_session, par, request); }
    mfxStatus Init(mfxVideoParam *par) { return MFXVideoPAK_Init(m_session, par); }
    mfxStatus Reset(mfxVideoParam *par) { return MFXVideoPAK_Reset(m_session, par); }
    mfxStatus Close(void) { return MFXVideoPAK_Close(m_session); }

    mfxStatus GetVideoParam(mfxVideoParam *par) { return MFXVideoPAK_GetVideoParam(m_session, par); }
    mfxStatus GetFrameParam(mfxFrameParam *par) { return MFXVideoPAK_GetFrameParam(m_session, par); }
    mfxStatus RunSeqHeader(mfxFrameCUC *cuc) { return MFXVideoPAK_RunSeqHeader(m_session, cuc); }
    mfxStatus RunFramePAKAsync(mfxFrameCUC *cuc, mfxSyncPoint *syncp) { return MFXVideoPAK_RunFramePAKAsync(m_session, cuc, syncp); }

protected:

    mfxSession m_session;                                       // (mfxSession) handle to the owning session
};

class MFXVideoBRC
{
public:

    MFXVideoBRC(mfxSession session) { m_session = session; }
    virtual ~MFXVideoBRC(void) { Close(); }

    mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) { return MFXVideoBRC_Query(m_session, in, out); }
    mfxStatus Init(mfxVideoParam *par) { return MFXVideoBRC_Init(m_session, par); }
    mfxStatus Reset(mfxVideoParam *par) { return MFXVideoBRC_Reset(m_session, par); }
    mfxStatus Close(void) { return MFXVideoBRC_Close(m_session); }

    mfxStatus FrameENCUpdate(mfxFrameCUC *cuc) { return MFXVideoBRC_FrameENCUpdate(m_session, cuc); }
    mfxStatus FramePAKRefine(mfxFrameCUC *cuc) { return MFXVideoBRC_FramePAKRefine(m_session, cuc); }
    mfxStatus FramePAKRecord(mfxFrameCUC *cuc) { return MFXVideoBRC_FramePAKRecord(m_session, cuc); }

protected:

    mfxSession m_session;                                       // (mfxSession) handle to the owning session
};

#endif // __MFXVIDEOPLUSPLUS_H
