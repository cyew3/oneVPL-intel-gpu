#include "ts_fei.h"

tsPreENC::tsPreENC(bool useDefaults)
    : m_default(useDefaults)
    , m_initialized(false)
    , m_par()
    , m_pPar(&m_par)
{
    if (m_default)
    {
        mfxExtFeiParam& extbuffer = m_par;
        extbuffer.Func = MFX_FEI_FUNCTION_PREENC;

        m_par.mfx.CodecId = MFX_CODEC_AVC;
        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 720;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 576;
        m_par.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }
}

tsPreENC::~tsPreENC()
{
    if (m_initialized)
    {
        Close();
    }
}

mfxStatus tsPreENC::Close()
{
    return Close(m_session);
}

mfxStatus tsPreENC::Close(mfxSession session)
{
    TRACE_FUNC1(MFXVideoENC_Close, session);
    g_tsStatus.check(MFXVideoENC_Close(session));

    m_initialized = false;

    return g_tsStatus.get();
}

mfxStatus tsPreENC::Init()
{
    return Init(m_session, m_pPar);
}

mfxStatus tsPreENC::Init(mfxSession session, mfxVideoParam *par)
{
    TRACE_FUNC2(MFXVideoENC_Init, session, par);
    g_tsStatus.check(MFXVideoENC_Init(session, par));

    m_initialized = (g_tsStatus.get() >= 0);

    return g_tsStatus.get();
}

