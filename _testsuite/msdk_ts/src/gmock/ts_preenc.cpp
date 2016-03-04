#include "ts_preenc.h"

tsVideoPreENC::tsVideoPreENC(bool useDefaults)
    : m_default(useDefaults)
    , m_initialized(false)
    , m_loaded(false)
    , m_par()
    , m_request()
    , m_pPar(&m_par)
    , m_pParOut(&m_par)
    , m_pRequest(&m_request)
    , m_pSurf(0)
    , m_filler(0)
    , m_frames_buffered(0)
    , m_uid(0)
    , m_field_processed(0)
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
        m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 26;
        m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }
    m_loaded = true;
}

tsVideoPreENC::tsVideoPreENC(mfxFeiFunction func, mfxU32 CodecId, bool useDefaults)
    : m_default(useDefaults)
    , m_initialized(false)
    , m_loaded(false)
    , m_par()
    , m_request()
    , m_pPar(&m_par)
    , m_pParOut(&m_par)
    , m_pRequest(&m_request)
    , m_pSurf(0)
    , m_filler(0)
    , m_frames_buffered(0)
    , m_uid(0)
    , m_field_processed(0)
{
    if(m_default)
    {
        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 720;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 576;
        m_par.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 26;
        m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }

    mfxExtFeiParam& extbuffer = m_par;
    extbuffer.Func = func;
    m_par.mfx.CodecId = CodecId;

    m_loaded = true;
}

tsVideoPreENC::~tsVideoPreENC()
{
    if (m_initialized)
    {
        Close();
    }
}

mfxStatus tsVideoPreENC::Init()
{
    if(m_default)
    {
        if(!m_session)
        {
            MFXInit();TS_CHECK_MFX;
        }
        if(!m_loaded)
        {
            Load();
        }
        if(     !m_pFrameAllocator
            && (   (m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
                || (m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        {
            if(!GetAllocator())
            {
                UseDefaultAllocator(false);
            }
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();TS_CHECK_MFX;
        }
        if(m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            QueryIOSurf();
            AllocOpaque(m_request, m_par);
        }
    }
    return Init(m_session, m_pPar);
}

mfxStatus tsVideoPreENC::Init(mfxSession session, mfxVideoParam *par)
{
    mfxVideoParam orig_par;
    if (par) memcpy(&orig_par, m_pPar, sizeof(mfxVideoParam));

    TRACE_FUNC2(MFXVideoENC_Init, session, par);
    g_tsStatus.check( MFXVideoENC_Init(session, par) );

    m_initialized = (g_tsStatus.get() >= 0);

    if (par)
    {
        if (g_tsConfig.lowpower != MFX_CODINGOPTION_UNKNOWN)
        {
            EXPECT_EQ(g_tsConfig.lowpower, par->mfx.LowPower)
                << "ERROR: external configuration of LowPower doesn't equal to real value\n";
        }
        EXPECT_EQ(0, memcmp(&orig_par, m_pPar, sizeof(mfxVideoParam)))
            << "ERROR: Input parameters must not be changed in Init()";
    }

    return g_tsStatus.get();
}

mfxStatus tsVideoPreENC::Close()
{
    return Close(m_session);
}

mfxStatus tsVideoPreENC::Close(mfxSession session)
{
    TRACE_FUNC1(MFXVideoENC_Close, session);
    g_tsStatus.check(MFXVideoENC_Close(session));

    m_initialized = false;

    return g_tsStatus.get();
}

mfxStatus tsVideoPreENC::Query()
{
    if(m_default)
    {
        if(!m_session)
        {
            MFXInit();
        }
        if(!m_loaded)
        {
            Load();
        }
    }
    return Query(m_session, m_pPar, m_pParOut);
}

mfxStatus tsVideoPreENC::Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    TRACE_FUNC3(MFXVideoENC_Query, session, in, out);
    g_tsStatus.check( MFXVideoENC_Query(session, in, out) );
    TS_TRACE(out);

    return g_tsStatus.get();
}

mfxStatus tsVideoPreENC::QueryIOSurf()
{
    if(m_default)
    {
        if(!m_session)
        {
            MFXInit();
        }
        if(!m_loaded)
        {
            Load();
        }
    }
    return QueryIOSurf(m_session, m_pPar, m_pRequest);
}

mfxStatus tsVideoPreENC::QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{

    TRACE_FUNC3(MFXVideoENC_QueryIOSurf, session, par, request);
    g_tsStatus.check( MFXVideoENC_QueryIOSurf(session, par, request) );
    TS_TRACE(request);

    return g_tsStatus.get();
}
