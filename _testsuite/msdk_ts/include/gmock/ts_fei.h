#pragma once

#include "ts_session.h"
#include "ts_ext_buffers.h"
#include "ts_surface.h"

class tsPreENC : public tsSession, public tsSurfacePool
{
public:
    bool                        m_default;
    bool                        m_initialized;
    tsExtBufType<mfxVideoParam> m_par;
    mfxVideoParam*              m_pPar;

    tsPreENC(bool useDefaults = true);
    ~tsPreENC();

    mfxStatus Init();
    mfxStatus Init(mfxSession session, mfxVideoParam *par);

    mfxStatus Close();
    mfxStatus Close(mfxSession session);
};
