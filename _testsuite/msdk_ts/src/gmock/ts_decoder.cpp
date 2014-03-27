#include "ts_decoder.h"

tsVideoDecoder::tsVideoDecoder(mfxU32 CodecId, bool useDefaults)
    : m_default(useDefaults)
    , m_initialized(false)
    , m_par()
    , m_bitstream()
    , m_request()
    , m_pPar(&m_par)
    , m_pParOut(&m_par)
    , m_pBitstream(&m_bitstream)
    , m_pRequest(&m_request)
    , m_pSyncPoint(&m_syncpoint)
    , m_pSurf(0)
    , m_pSurfOut(0)
    , m_surf_processor(0)
    , m_bs_processor(0)
    , m_uid(0)
    , m_loaded(false)
    , m_par_set(false)
{
    m_par.mfx.CodecId = CodecId;
    if(m_default)
    {
        m_par.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }
    m_uid = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_DECODE, CodecId);
    m_loaded = !m_uid;
}

tsVideoDecoder::~tsVideoDecoder() 
{
    if(m_initialized)
    {
        Close();
    }
}

mfxStatus tsVideoDecoder::Init() 
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
        if(     !m_pFrameAllocator 
            && (   (m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
                || (m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)))
        {
            if(!GetAllocator())
            {
                UseDefaultAllocator(false);
            }
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
        }
        if(!m_par_set)
        {
            DecodeHeader();
        }
        if(m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            QueryIOSurf();
            AllocOpaque(m_request, m_par);
        }
    }
    return Init(m_session, m_pPar);
}


mfxStatus tsVideoDecoder::Init(mfxSession session, mfxVideoParam *par)
{
    TRACE_FUNC2(MFXVideoDECODE_Init, session, par);
    g_tsStatus.check( MFXVideoDECODE_Init(session, par) );

    m_initialized = (g_tsStatus.get() >= 0);

    return g_tsStatus.get();
}

mfxStatus tsVideoDecoder::Close()
{
    return Close(m_session);
}

mfxStatus tsVideoDecoder::Close(mfxSession session)
{
    TRACE_FUNC1(MFXVideoDECODE_Close, session);
    g_tsStatus.check( MFXVideoDECODE_Close(session) );

    m_initialized = false;

    return g_tsStatus.get();
}

mfxStatus tsVideoDecoder::Query()
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
        if(!m_par_set)
        {
            DecodeHeader();
        }
    }
    return Query(m_session, m_pPar, m_pParOut);
}

mfxStatus tsVideoDecoder::Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    TRACE_FUNC3(MFXVideoDECODE_Query, session, in, out);
    g_tsStatus.check( MFXVideoDECODE_Query(session, in, out) );
    TS_TRACE(out);

    return g_tsStatus.get();
}

mfxStatus tsVideoDecoder::QueryIOSurf()
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
        if(!m_par_set)
        {
            DecodeHeader();
        }
    }
    return QueryIOSurf(m_session, m_pPar, m_pRequest);
}

mfxStatus tsVideoDecoder::QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    TRACE_FUNC3(MFXVideoDECODE_QueryIOSurf, session, par, request);
    g_tsStatus.check( MFXVideoDECODE_QueryIOSurf(session, par, request) );
    TS_TRACE(request);

    return g_tsStatus.get();
}

mfxStatus tsVideoDecoder::Reset()
{
    return Reset(m_session, m_pPar);
}

mfxStatus tsVideoDecoder::Reset(mfxSession session, mfxVideoParam *par)
{
    TRACE_FUNC2(MFXVideoDECODE_Reset, session, par);
    g_tsStatus.check( MFXVideoDECODE_Reset(session, par) );

    //m_frames_buffered = 0;

    return g_tsStatus.get();
}


mfxStatus tsVideoDecoder::GetVideoParam() 
{
    if(m_default && !m_initialized)
    {
        Init();
    }
    return GetVideoParam(m_session, m_pPar); 
}

mfxStatus tsVideoDecoder::GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    TRACE_FUNC2(MFXVideoDECODE_GetVideoParam, session, par);
    g_tsStatus.check( MFXVideoDECODE_GetVideoParam(session, par) );
    TS_TRACE(par);

    return g_tsStatus.get();
}

mfxStatus tsVideoDecoder::AllocSurfaces()
{
    if(m_default && !m_request.NumFrameMin)
    {
        QueryIOSurf();
    }
    return tsSurfacePool::AllocSurfaces(m_request);
}

mfxStatus tsVideoDecoder::DecodeHeader()
{
    if(m_bs_processor)
    {
        m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);
    }
    return DecodeHeader(m_session, m_pBitstream, m_pPar);
}

mfxStatus tsVideoDecoder::DecodeHeader(mfxSession session, mfxBitstream *bs, mfxVideoParam *par)
{
    TRACE_FUNC3(MFXVideoDECODE_DecodeHeader, session, bs, par);
    g_tsStatus.check( MFXVideoDECODE_DecodeHeader(session, bs, par) );
    TS_TRACE(par);

    m_par_set = (g_tsStatus.get() >= 0);

    return g_tsStatus.get();
}

mfxStatus tsVideoDecoder::DecodeFrameAsync()
{
    if(m_default)
    {
        if(!PoolSize())
        {
            if(m_pFrameAllocator && !GetAllocator())
            {
                SetAllocator(m_pFrameAllocator, true);
            }
            AllocSurfaces();
            if(!m_pFrameAllocator && (m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)))
            {
                m_pFrameAllocator = GetAllocator();
                SetFrameAllocator();
            }
        }
        if(!m_initialized)
        {
            Init();
        }
        if(g_tsStatus.get() != MFX_ERR_MORE_DATA)
        {
            m_pSurf = GetSurface();
        }
        else
        {
            if(m_bs_processor)
            {
                m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);
            }
        }
    }

    DecodeFrameAsync(m_session, m_pBitstream, m_pSurf, &m_pSurfOut, m_pSyncPoint);

    if(g_tsStatus.get() == 0)
    {
        m_surf_out.insert( std::make_pair(*m_pSyncPoint, m_pSurfOut) );
        if(m_pSurfOut)
        {
            m_pSurfOut->Data.Locked ++;
        }
    }

    return g_tsStatus.get();
}

mfxStatus tsVideoDecoder::DecodeFrameAsync(
    mfxSession session, 
    mfxBitstream *bs, 
    mfxFrameSurface1 *surface_work, 
    mfxFrameSurface1 **surface_out, 
    mfxSyncPoint *syncp)
{
    TRACE_FUNC5(MFXVideoDECODE_DecodeFrameAsync, session, bs, surface_work, surface_out, syncp);
    mfxStatus mfxRes = MFXVideoDECODE_DecodeFrameAsync(session, bs, surface_work, surface_out, syncp);
    TS_TRACE(mfxRes);
    TS_TRACE(surface_out);
    TS_TRACE(syncp);
    
    return g_tsStatus.m_status = mfxRes;
}

mfxStatus tsVideoDecoder::SyncOperation()
{
    if(!m_surf_out.size())
    {
        g_tsStatus.check(MFX_ERR_UNKNOWN);
    }
    return SyncOperation(m_surf_out.begin()->first);
}

mfxStatus tsVideoDecoder::SyncOperation(mfxSyncPoint syncp)
{
    mfxFrameSurface1* pS = m_surf_out[syncp];
    mfxStatus res = SyncOperation(m_session, syncp, MFX_INFINITE);

    if(m_default && pS && pS->Data.Locked)
    {
        pS->Data.Locked --;
    }

    if (m_default && m_surf_processor && g_tsStatus.get() == MFX_ERR_NONE)
    {
        m_surf_processor->ProcessSurface(pS, m_pFrameAllocator);
    }

    return g_tsStatus.m_status = res;
}

mfxStatus tsVideoDecoder::SyncOperation(mfxSession session,  mfxSyncPoint syncp, mfxU32 wait)
{
    m_surf_out.erase(syncp);
    return tsSession::SyncOperation(session, syncp, wait);
}

mfxStatus tsVideoDecoder::DecodeFrames(mfxU32 n)
{
    
    mfxU32 decoded = 0;
    mfxU32 submitted = 0;
    mfxU32 async = TS_MAX(1, m_par.AsyncDepth);
    mfxStatus res = MFX_ERR_NONE;

    async = TS_MIN(n, async - 1);

    while(decoded < n)
    {
        res = DecodeFrameAsync();

        if(MFX_ERR_MORE_DATA == res)
        {
            if(!m_pBitstream)
            {
                if(submitted)
                {
                    decoded += submitted;

                    while(m_surf_out.size()) SyncOperation();
                }
                break;
            } 
            continue;
        }

        if(MFX_ERR_MORE_SURFACE == res || res > 0)
        {
            continue;
        }

        if(res < 0) g_tsStatus.check();

        if(++submitted >= async)
        {
            while(m_surf_out.size()) SyncOperation();
            decoded += submitted;
            submitted = 0;
            async = TS_MIN(async, (n - decoded));
        }
    }

    g_tsLog << decoded << " FRAMES DECODED\n";
    
    return g_tsStatus.get();
}