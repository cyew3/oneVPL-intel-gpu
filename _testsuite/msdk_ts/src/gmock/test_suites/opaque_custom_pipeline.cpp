/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2017 Intel Corporation. All Rights Reserved.
//
*/
#include "ts_struct.h"
#include "ts_encoder.h"
#include "ts_vpp.h"
#include "ts_decoder.h"
#include "time_defs.h"

/*
Description:
    Verify that session with opaque will be work correctly after:
        1) reset one of the component with the same parameters
        2) change surface pool
        3) joined session is executed correctly if there were changes in another session
*/

#define TEST_NAME opaque_custom_pipeline
const mfxU32 NSTEPS = 5;
const mfxU32 NFRAMES = 5;

namespace TEST_NAME
{

enum Component
{
    DEC = 1,
    ENC,
    VPP,
};
enum ActionType { RESET, REALLOC };
enum SessionType { SINGLE, JOIN };

std::string getStream(mfxU32 codec)
{
    switch (codec)
    {
        case MFX_CODEC_AVC:   return "forBehaviorTest/foreman_cif.h264";
        case MFX_CODEC_MPEG2: return "forBehaviorTest/foreman_cif.m2v";
        case MFX_CODEC_HEVC:  return "conformance/hevc/itu/DBLK_A_SONY_3.bit";
        default: break;
    }
    return "";
}

class TestSession: public tsSession
{
public:
    typedef struct {
        mfxBitstream bs;
        mfxSyncPoint syncp;
    } Task;

    tsVideoDecoder m_dec;
    tsVideoEncoder m_enc;
    tsVideoVPP m_vpp;

    std::list<TestSession::Task> m_task_pool;

    tsSurfacePool m_spool_in; // surface pool for decoder and vpp/encode input
    tsSurfacePool m_spool_out; // surface pool for vpp output and encoder

    TestSession(tsExtBufType<mfxVideoParam> dec_par, tsExtBufType<mfxVideoParam> enc_par, tsExtBufType<mfxVideoParam> vpp_par);
    ~TestSession();

    // all status checks are performed inside function calls and throw exceptions,
    // so functions are not necessary to return any value
    void InitComponents();
    void CloseComponents();
    void AllocSurfaces();
    void ProcessFrames(mfxU32 n);
    void CloseInitVPP();
    void CloseInitDecode();
    void CloseInitEncode();
    void ReallocPool(mfxU32 type, tsExtBufType<mfxVideoParam>& upd_par);
    void DecodeHeader();

    Task* GetFreeTask();
    mfxU32 SyncTasks();
    mfxFrameSurface1* GetVPPOutSurface();
    mfxFrameSurface1* GetDecodeSurface();
};

TestSession::TestSession(tsExtBufType<mfxVideoParam> dec_par, tsExtBufType<mfxVideoParam> enc_par,
    tsExtBufType<mfxVideoParam> vpp_par)
    : tsSession()
    , m_dec(false)
    , m_enc(false)
    , m_vpp(false)
    , m_task_pool()
    , m_spool_in()
    , m_spool_out()
{
    MFXInit();

    m_dec.m_session = m_session;
    m_enc.m_session = m_session;
    m_vpp.m_session = m_session;

    m_dec.m_par = dec_par;
    m_enc.m_par = enc_par;
    m_vpp.m_par = vpp_par;

    DecodeHeader();

    m_vpp.m_par.vpp.In = m_dec.m_par.mfx.FrameInfo;
    m_enc.m_par.mfx.FrameInfo = m_vpp.m_par.vpp.Out;

    // create task pool for asynchronous processing
    m_task_pool.resize(m_enc.m_par.AsyncDepth);
}

TestSession::~TestSession()
{
    if (m_dec.m_bs_processor)
        delete m_dec.m_bs_processor;
    if (m_enc.m_bs_processor)
        delete m_enc.m_bs_processor;
    CloseComponents();
    MFXClose();
}

void TestSession::AllocSurfaces() // no implementation for cases like video->opaque, opaque->video
{
    m_dec.QueryIOSurf();
    m_enc.QueryIOSurf();
    m_vpp.QueryIOSurf();

    m_dec.m_request.NumFrameSuggested += m_vpp.m_request[0].NumFrameSuggested - m_vpp.m_par.AsyncDepth;
    m_dec.m_request.NumFrameMin += m_vpp.m_request[0].NumFrameMin - m_vpp.m_par.AsyncDepth;
    m_enc.m_request.NumFrameSuggested += m_vpp.m_request[1].NumFrameSuggested - m_vpp.m_par.AsyncDepth;
    m_enc.m_request.NumFrameMin += m_vpp.m_request[1].NumFrameMin - m_vpp.m_par.AsyncDepth;

    mfxExtOpaqueSurfaceAlloc& opaqueDec = m_dec.m_par;
    mfxExtOpaqueSurfaceAlloc& opaqueVPP = m_vpp.m_par;
    mfxExtOpaqueSurfaceAlloc& opaqueEnc = m_enc.m_par;

    m_spool_in.AllocOpaque(m_dec.m_request, opaqueDec);
    opaqueVPP.In = opaqueDec.Out;

    m_spool_out.AllocOpaque(m_enc.m_request, opaqueEnc);
    opaqueVPP.Out = opaqueEnc.In;
}

void TestSession::ReallocPool(mfxU32 type, tsExtBufType<mfxVideoParam>& upd_par)
{
    if (type == DEC)
    {
        m_spool_in.FreeSurfaces();
        if (m_vpp.m_initialized)
            m_vpp.Close();
        if (m_dec.m_initialized)
            m_dec.Close();

        m_dec.m_par = upd_par;
        DecodeHeader();
        m_vpp.m_par.vpp.In = m_dec.m_par.mfx.FrameInfo;

        m_dec.QueryIOSurf();
        m_vpp.QueryIOSurf();

        m_dec.m_request.NumFrameSuggested += m_vpp.m_request[0].NumFrameSuggested - m_vpp.m_par.AsyncDepth;
        m_dec.m_request.NumFrameMin += m_vpp.m_request[0].NumFrameMin - m_vpp.m_par.AsyncDepth;

        m_dec.m_par.RemoveExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        m_vpp.m_par.RemoveExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        mfxExtOpaqueSurfaceAlloc& opaqueDec = m_dec.m_par;
        mfxExtOpaqueSurfaceAlloc& opaqueVPP = m_vpp.m_par;

        m_spool_in.AllocOpaque(m_dec.m_request, opaqueDec);
        opaqueVPP.In = opaqueDec.Out;

        mfxExtOpaqueSurfaceAlloc* opaqueEnc = (mfxExtOpaqueSurfaceAlloc*)m_enc.m_par.GetExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        memcpy(&opaqueVPP.Out, &opaqueEnc->In, sizeof(opaqueEnc->Out));

        CloseInitDecode();
        CloseInitVPP();
    }
    else if (type == ENC)
    {
        m_spool_out.FreeSurfaces();
        if (m_vpp.m_initialized)
            m_vpp.Close();
        if (m_enc.m_initialized)
            m_enc.Close();

        m_enc.m_par = upd_par;
        m_vpp.m_par.vpp.Out = upd_par.mfx.FrameInfo;

        m_vpp.QueryIOSurf();
        m_enc.QueryIOSurf();

        m_enc.m_request.NumFrameSuggested += m_vpp.m_request[1].NumFrameSuggested - m_vpp.m_par.AsyncDepth;
        m_enc.m_request.NumFrameMin += m_vpp.m_request[1].NumFrameMin - m_vpp.m_par.AsyncDepth;

        m_vpp.m_par.RemoveExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        m_enc.m_par.RemoveExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        mfxExtOpaqueSurfaceAlloc& opaqueVPP = m_vpp.m_par;
        mfxExtOpaqueSurfaceAlloc& opaqueEnc = m_enc.m_par;

        m_spool_out.AllocOpaque(m_enc.m_request, opaqueEnc);
        opaqueVPP.Out = opaqueEnc.In;

        mfxExtOpaqueSurfaceAlloc* opaqueDec = (mfxExtOpaqueSurfaceAlloc*)m_dec.m_par.GetExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        memcpy(&opaqueVPP.In, &opaqueDec->Out, sizeof(opaqueDec->Out));

        CloseInitEncode();
        CloseInitVPP();
    }
    else
    {
        m_spool_in.FreeSurfaces();
        m_spool_out.FreeSurfaces();

        CloseComponents();
        AllocSurfaces();

        CloseInitDecode();
        CloseInitEncode();
        CloseInitVPP();
    }
}

void TestSession::InitComponents()
{
    if (!m_dec.m_par_set)
    {
        m_dec.m_pBitstream = m_dec.m_bs_processor->ProcessBitstream(m_dec.m_bitstream);
        m_dec.DecodeHeader(m_session, m_dec.m_pBitstream, m_dec.m_pPar);
    }

    m_dec.Init(m_session, m_dec.m_pPar);
    m_dec.GetVideoParam();

    m_vpp.Init(m_session, m_vpp.m_pPar);
    m_vpp.GetVideoParam();

    m_enc.Init(m_session, m_enc.m_pPar);
    m_enc.GetVideoParam();

    // alloc memory for bitstream buffer
    mfxU16 tmp_async = m_enc.m_par.AsyncDepth;
    m_enc.m_par.AsyncDepth = 1;
    for (auto& task : m_task_pool)
    {
        m_enc.AllocBitstream();
        task.bs = m_enc.m_bitstream;
        task.syncp = NULL;
    }
    m_enc.m_par.AsyncDepth = tmp_async;
}

void TestSession::CloseComponents()
{
    if (m_dec.m_initialized)
        m_dec.Close();
    if (m_enc.m_initialized)
        m_enc.Close();
    if (m_vpp.m_initialized)
        m_vpp.Close();
}

void TestSession::DecodeHeader()
{
    const char* stream = g_tsStreamPool.Get(getStream(m_dec.m_par.mfx.CodecId));
    if (m_dec.m_bs_processor)
        delete m_dec.m_bs_processor;
    // bit stream buffer for decoder
    m_dec.m_bs_processor = new tsBitstreamReader(stream, 1024*1024);
    if (!m_dec.m_bitstream.DataLength)
    {
        m_dec.m_bitstream.DataLength = m_dec.m_bitstream.MaxLength;
        m_dec.m_bitstream.DataOffset = 0;
    }
    m_dec.m_pBitstream = m_dec.m_bs_processor->ProcessBitstream(m_dec.m_bitstream);
    m_dec.DecodeHeader(m_session, m_dec.m_pBitstream, m_dec.m_pPar);
}

void TestSession::CloseInitVPP()
{
    if (m_vpp.m_initialized)
        m_vpp.Close();

    m_vpp.Init(m_session, m_vpp.m_pPar);
    m_vpp.GetVideoParam();
}

void TestSession::CloseInitEncode()
{
    if (m_enc.m_initialized)
        m_enc.Close();

    m_enc.Init(m_session, m_enc.m_pPar);
    m_enc.GetVideoParam();

    // alloc memory for bitstream buffer with new params
    mfxU16 tmp_async = m_enc.m_par.AsyncDepth;
    m_enc.m_par.AsyncDepth = 1;
    for (auto& task : m_task_pool)
    {
        if (*task.bs.Data)
        {
            delete [] task.bs.Data;
            task.bs.Data = 0;
        }
        m_enc.AllocBitstream();
        task.bs = m_enc.m_bitstream;
        task.syncp = NULL;
    }
    m_enc.m_par.AsyncDepth = tmp_async;
}

void TestSession::CloseInitDecode()
{
    if (m_dec.m_initialized)
        m_dec.Close();

    DecodeHeader();

    m_dec.Init(m_session, m_dec.m_pPar);
    m_dec.GetVideoParam();
}

TestSession::Task* TestSession::GetFreeTask()
{
    for (std::list<Task>::iterator it = m_task_pool.begin(); it != m_task_pool.end(); ++it)
    {
        if ((*it).syncp == NULL)
            return &(*it);
    }
    return NULL;
}

mfxU32 TestSession::SyncTasks()
{
    mfxU32 sync = 0;

    for (auto& task : m_task_pool)
    {
        if (task.syncp == NULL)
            continue;

        m_enc.m_pBitstream = &task.bs; // to dump data in SyncOperation
        m_enc.SyncOperation(task.syncp);
        ++sync;

        task.syncp = NULL;
        task.bs.DataLength = 0;
        task.bs.DataOffset = 0;
    }

    return sync;
}

mfxFrameSurface1* TestSession::GetDecodeSurface()
{
    mfxStatus sts = MFX_ERR_UNKNOWN;
    mfxU32 wait_counter = 0;
    while (MFX_ERR_NONE != sts || (MFX_ERR_NONE < sts && *m_dec.m_pSyncPoint == NULL))
    {
        m_dec.m_pSurf = m_spool_in.GetSurface();

        sts = m_dec.DecodeFrameAsync(m_session,
                                     m_dec.m_pBitstream,
                                     m_dec.m_pSurf,
                                     &m_dec.m_pSurfOut,
                                     m_dec.m_pSyncPoint);

        if (MFX_ERR_MORE_DATA == sts)
        {
            if (!m_dec.m_pBitstream) // end of file
            {
                return 0;
            }
            // Read more data to input bit stream
            if (m_dec.m_bs_processor)
            {
                m_dec.m_pBitstream = m_dec.m_bs_processor->ProcessBitstream(m_dec.m_bitstream);
                continue;
            }
        }

        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            MSDK_SLEEP(100);  // wait 100 ms if device is busy
            if (wait_counter >= 10) // to avoid unexpected test hang, prohibit wait more than 1s
            {
                ADD_FAILURE() << "ERROR: Unexpected MFX_WRN_DEVICE_BUSY in RunFrameVPPAsync";
                throw tsFAIL;
            }
            wait_counter++;
            continue;
        }

        if (MFX_ERR_NONE < sts && *m_dec.m_pSyncPoint != NULL)
            sts = MFX_ERR_NONE;

        if (MFX_ERR_MORE_SURFACE != sts && MFX_ERR_NONE > sts)
            g_tsStatus.check();
    }

    return m_dec.m_pSurfOut;
}

mfxFrameSurface1* TestSession::GetVPPOutSurface()
{
    mfxStatus sts = MFX_ERR_UNKNOWN;
    mfxU32 wait_counter = 0;
    while (MFX_ERR_NONE != sts || (MFX_ERR_NONE < sts && *m_vpp.m_pSyncPoint == NULL))
    {
        m_vpp.m_pSurfOut = m_spool_out.GetSurface();
        sts = m_vpp.RunFrameVPPAsync(m_session,
                                     m_vpp.m_pSurfIn,
                                     m_vpp.m_pSurfOut,
                                     0,
                                     m_vpp.m_pSyncPoint);

        if (MFX_ERR_MORE_DATA == sts)
            return 0;
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            MSDK_SLEEP(100);  // wait 100 ms if device is busy
            if (wait_counter >= 10) // to avoid unexpected test hang, prohibit wait more than 1s
            {
                ADD_FAILURE() << "ERROR: Unexpected MFX_WRN_DEVICE_BUSY in RunFrameVPPAsync";
                throw tsFAIL;
            }
            wait_counter++;
            continue;
        }
        g_tsStatus.check();
    }

    return m_vpp.m_pSurfOut;
}

void TestSession::ProcessFrames(mfxU32 n)
{
    mfxU32 sync = 0;
    mfxU32 submitted = 0;
    bool eos = false;

    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameSurface1* decOut;
    mfxFrameSurface1* vppOut;
    while (submitted < n)
    {
        Task* currTask = GetFreeTask();
        if (currTask == NULL)
        {
            sync += SyncTasks();
            continue;
        }

        decOut = GetDecodeSurface();
        if (!decOut)
            eos = true;

        m_vpp.m_pSurfIn = m_dec.m_pSurfOut;
        vppOut = GetVPPOutSurface();

        m_enc.m_pSurf = vppOut;
        sts = m_enc.EncodeFrameAsync(m_session,
                                     m_enc.m_pSurf ? m_enc.m_pCtrl : 0,
                                     m_enc.m_pSurf,
                                     &currTask->bs,
                                     &currTask->syncp);

        if (MFX_ERR_MORE_DATA == sts && eos)
            break;
        if (MFX_ERR_MORE_DATA == sts) // encoder need more input, request more surfaces
            continue;
        if (MFX_ERR_NONE < sts && currTask->syncp)
            sts = MFX_ERR_NONE;
        if (MFX_ERR_NONE > sts)
            g_tsStatus.check();
        submitted++;
    }

    sync += SyncTasks(); // sync remaining tasks

    g_tsLog << sync << " processed frames\n";
    EXPECT_EQ(sync, n);
}

struct f_pair
{
    mfxU32 ext_type;
    const  tsStruct::Field* f;
    mfxU32 v;
};

const struct TestCase
{
    mfxU32 mode;
    f_pair set_par[MAX_NPARS];
    struct
    {
        mfxU32 comp;
        mfxU32 action;
        f_pair new_pars[MAX_NPARS];
    } pipeline[NSTEPS];
    mfxU32 repeat;
} tcs[] =
{
/*00*/  { SINGLE,
            {{DEC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {DEC, &tsStruct::mfxVideoParam.AsyncDepth, 5},
            {ENC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {ENC, &tsStruct::mfxVideoParam.AsyncDepth, 5},
            {VPP, &tsStruct::mfxVideoParam.AsyncDepth, 5}},
            {{VPP, RESET}},
          1
        },

/*01*/  { SINGLE,
            {{DEC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {DEC, &tsStruct::mfxVideoParam.AsyncDepth, 1},
            {ENC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {ENC, &tsStruct::mfxVideoParam.AsyncDepth, 1},
            {VPP, &tsStruct::mfxVideoParam.AsyncDepth, 1}},
            {{DEC, RESET}},
          1
        },

/*02*/  { SINGLE,
            {{DEC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {DEC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {ENC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {ENC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {VPP, &tsStruct::mfxVideoParam.AsyncDepth, 4}},
            {{ENC, RESET}},
          1
        },

/*03*/  { SINGLE,
            {{DEC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {DEC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {ENC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {ENC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {VPP, &tsStruct::mfxVideoParam.AsyncDepth, 4}},
            {{VPP, RESET}},
          10
        },

/*04*/  { SINGLE,
            {{DEC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {DEC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {ENC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {ENC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {VPP, &tsStruct::mfxVideoParam.AsyncDepth, 4}},

            {{ENC, REALLOC, {{ENC, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1280},
                            {ENC, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 720}
                           }},
             {ENC, REALLOC, {{ENC, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720},
                            {ENC, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480}
                           }},
            },
          10
        },

/*05*/  { SINGLE,
            {{DEC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {DEC, &tsStruct::mfxVideoParam.AsyncDepth, 7},
            {ENC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {ENC, &tsStruct::mfxVideoParam.AsyncDepth, 7},
            {VPP, &tsStruct::mfxVideoParam.AsyncDepth, 7}},

            {{DEC, REALLOC, {{DEC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_MPEG2}}}},
          10
        },

/*06*/  { JOIN,
            {{DEC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {DEC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {ENC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {ENC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {VPP, &tsStruct::mfxVideoParam.AsyncDepth, 4}},

            {{VPP, RESET}},
          10
        },

/*07*/  { JOIN,
            {{DEC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {DEC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {ENC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {ENC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {VPP, &tsStruct::mfxVideoParam.AsyncDepth, 4}},

            {{ENC, REALLOC, {{ENC, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720},
                             {ENC, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 576}
                            }},
            },
          10
        },

/*08*/  { JOIN,
            {{DEC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {DEC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {ENC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {ENC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {VPP, &tsStruct::mfxVideoParam.AsyncDepth, 4}},

            {{VPP, REALLOC, {{VPP, &tsStruct::mfxVideoParam.vpp.Out.Width, 1920},
                             {VPP, &tsStruct::mfxVideoParam.vpp.Out.Height, 1080}
                            }},
            },
          5
        },

/*09*/  { JOIN,
            {{DEC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {DEC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {ENC, &tsStruct::mfxVideoParam.mfx.CodecId, MFX_CODEC_AVC},
            {ENC, &tsStruct::mfxVideoParam.AsyncDepth, 4},
            {VPP, &tsStruct::mfxVideoParam.AsyncDepth, 4}},

            {{VPP, RESET},
             {ENC, REALLOC, {{ENC, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1280},
                             {ENC, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 720}
                            }},
            },
          5
        },
};

void SetDefaultParams(tsExtBufType<mfxVideoParam>* par, mfxU32 type)
{
    par->mfx.FrameInfo.Width  = par->mfx.FrameInfo.CropW = 720;
    par->mfx.FrameInfo.Height = par->mfx.FrameInfo.CropH = 480;
    par->mfx.FrameInfo.FourCC          = MFX_FOURCC_NV12;
    par->mfx.FrameInfo.ChromaFormat    = MFX_CHROMAFORMAT_YUV420;

    if (type == DEC)
    {
        par->IOPattern = MFX_IOPATTERN_OUT_OPAQUE_MEMORY;
    }
    else if (type == ENC)
    {
        par->IOPattern = MFX_IOPATTERN_IN_OPAQUE_MEMORY;
        par->mfx.RateControlMethod  = MFX_RATECONTROL_CQP;
        par->mfx.QPI = par->mfx.QPP = par->mfx.QPB = 26;
        par->mfx.FrameInfo.FrameRateExtN = 30;
        par->mfx.FrameInfo.FrameRateExtD = 1;
        par->mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
    }
    else
    {
        par->IOPattern = MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY;
        par->vpp.In.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        par->vpp.In.FrameRateExtN = 30;
        par->vpp.In.FrameRateExtD = 1;
        par->vpp.Out = par->vpp.In;
    }
}

void SetParams(tsExtBufType<mfxVideoParam>& par, const f_pair* tc, mfxU32 type)
{
    for(mfxU32 i = 0; i < MAX_NPARS; i++)
    {
        if(tc[i].f && tc[i].ext_type == type)
        {
            SetParam(par, tc[i].f->name, tc[i].f->offset, tc[i].f->size, tc[i].v);
        }
    }
}

int RunTest(unsigned int id)
{
    TS_START;
    auto& tc = tcs[id];

    // register all used streams
    g_tsStreamPool.Get(getStream(MFX_CODEC_AVC));
    g_tsStreamPool.Get(getStream(MFX_CODEC_MPEG2));
    g_tsStreamPool.Get(getStream(MFX_CODEC_HEVC));
    g_tsStreamPool.Reg();

    tsExtBufType<mfxVideoParam> dec_par;
    SetDefaultParams(&dec_par, DEC);
    SetParams(dec_par, tc.set_par, DEC);
    tsExtBufType<mfxVideoParam> enc_par;
    SetDefaultParams(&enc_par, ENC);
    SetParams(enc_par, tc.set_par, ENC);
    tsExtBufType<mfxVideoParam> vpp_par;
    SetDefaultParams(&vpp_par, VPP);
    SetParams(vpp_par, tc.set_par, VPP);

    mfxU32 return_verbosity = g_tsTrace; //to avoid huge logs during transcoding

    if (tc.mode == SINGLE)
    {
        TestSession ts(dec_par, enc_par, vpp_par);
        ts.AllocSurfaces();
        ts.InitComponents();
        g_tsTrace = 0;
        ts.ProcessFrames(NFRAMES);
        g_tsTrace = return_verbosity;

        for (mfxU32 i = 0; i < tc.repeat; i++)
        {
            for (mfxU32 i = 0; i < NSTEPS; i++) // apply pipeline actions
            {
                if (!tc.pipeline[i].comp)
                    break;
                switch(tc.pipeline[i].comp)
                {
                    case DEC:
                    {
                        if (tc.pipeline[i].action == REALLOC)
                        {
                            tsExtBufType<mfxVideoParam> new_par = dec_par;
                            SetParams(new_par, tc.pipeline[i].new_pars, DEC);
                            ts.ReallocPool(DEC, new_par);
                        }
                        else
                            ts.CloseInitDecode();
                        break;
                    }
                    case VPP:
                    {
                        if (tc.pipeline[i].action == REALLOC)
                        {
                            tsExtBufType<mfxVideoParam> new_par = vpp_par;
                            SetParams(new_par, tc.pipeline[i].new_pars, VPP);
                            ts.ReallocPool(VPP, new_par);
                        }
                        else
                            ts.CloseInitVPP();
                        break;
                    }
                    case ENC:
                    {
                        if (tc.pipeline[i].action == REALLOC)
                        {
                            tsExtBufType<mfxVideoParam> new_par = enc_par;
                            SetParams(new_par, tc.pipeline[i].new_pars, ENC);
                            ts.ReallocPool(ENC, new_par);
                        }
                        else
                            ts.CloseInitEncode();
                        break;
                    }
                    default:
                        g_tsLog << "ERROR: Undefined component";
                        return 1;
                }
                g_tsTrace = 0;
                ts.ProcessFrames(NFRAMES);
                g_tsTrace = return_verbosity;
            }
        }
    }
    else
    {
        TestSession sessionA(dec_par, enc_par, vpp_par);
        TestSession sessionB(dec_par, enc_par, vpp_par);
        sessionA.MFXJoinSession(sessionB.m_session);

        sessionA.AllocSurfaces();
        sessionA.InitComponents();
        sessionB.AllocSurfaces();
        sessionB.InitComponents();

        g_tsTrace = 0;
        sessionA.ProcessFrames(NFRAMES);
        sessionB.ProcessFrames(NFRAMES);
        g_tsTrace = return_verbosity;

        for (mfxU32 i = 0; i < tc.repeat; i++)
        {
            TestSession* changed_ts = (i % 2) ? &sessionA : &sessionB;
            TestSession* check_ts = (i % 2) ? &sessionB : &sessionA;
            for (mfxU32 i = 0; i < NSTEPS; i++) // apply pipeline actions
            {
                if (!tc.pipeline[i].comp)
                    break;
                switch(tc.pipeline[i].comp)
                {
                    case DEC:
                    {
                        if (tc.pipeline[i].action == REALLOC)
                        {
                            tsExtBufType<mfxVideoParam> new_par = dec_par;
                            SetParams(new_par, tc.pipeline[i].new_pars, DEC);
                            changed_ts->ReallocPool(DEC, new_par);
                        }
                        else
                            changed_ts->CloseInitDecode();
                        break;
                    }
                    case VPP:
                    {
                        if (tc.pipeline[i].action == REALLOC)
                        {
                            tsExtBufType<mfxVideoParam> new_par = vpp_par;
                            SetParams(new_par, tc.pipeline[i].new_pars, VPP);
                            changed_ts->ReallocPool(VPP, new_par);
                        }
                        else
                            changed_ts->CloseInitVPP();
                        break;
                    }
                    case ENC:
                    {
                        if (tc.pipeline[i].action == REALLOC)
                        {
                            tsExtBufType<mfxVideoParam> new_par = enc_par;
                            SetParams(new_par, tc.pipeline[i].new_pars, ENC);
                            changed_ts->ReallocPool(ENC, new_par);
                        }
                        else
                            changed_ts->CloseInitEncode();
                        break;
                    }
                    default:
                        g_tsLog << "ERROR: Undefined component";
                        return 1;
                }
                g_tsTrace = 0;
                check_ts->ProcessFrames(NFRAMES);
                changed_ts->ProcessFrames(NFRAMES);
                g_tsTrace = return_verbosity;
            }
        }

        sessionB.CloseComponents();
        sessionB.MFXDisjoinSession();
        sessionA.CloseComponents();
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(TEST_NAME, RunTest, sizeof(tcs)/sizeof(TestCase));
};
