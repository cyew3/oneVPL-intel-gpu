#include "ts_decoder.h"
#include "ts_struct.h"

#if defined(_WIN32) || defined(_WIN64)
#include "windows.h"
#include "psapi.h"
#endif

namespace screen_capture_decode_frame_async
{

#if defined(_WIN32) || defined(_WIN64)
class MemoryTracker : public tsSurfaceProcessor
{
public:
    MemoryTracker()
    {
        m_process = 0;
        threshold = 100*1024; //allow 100 kb fluctuations
        skip = 10;
        count = 0;
        memset(&m_counters_init,0,sizeof(m_counters_init));
        memset(&m_counters_runtime,0,sizeof(m_counters_runtime));

        m_counters_init.cb = sizeof(m_counters_init);
        m_counters_runtime.cb = sizeof(m_counters_runtime);
    }
    virtual ~MemoryTracker() { }

    mfxStatus Start()
    {
        m_process = GetCurrentProcess();
        if(!m_process)
        {
            g_tsLog << "TEST ERROR: failed to get current process size\n";
            return MFX_ERR_ABORTED;
        }

        if( !GetProcessMemoryInfo ( m_process, (PROCESS_MEMORY_COUNTERS*) &m_counters_init, sizeof ( m_counters_init ) ) )
            return MFX_ERR_ABORTED;

        return MFX_ERR_NONE;
    }

    mfxStatus Stop()
    {
        if(!m_process)
            return MFX_ERR_NOT_INITIALIZED;

        CoFreeUnusedLibrariesEx(0,0);

        PROCESS_MEMORY_COUNTERS_EX counters;
        memset(&counters,0,sizeof(counters));
        counters.cb = sizeof(counters);
        if( !GetProcessMemoryInfo ( m_process, (PROCESS_MEMORY_COUNTERS*) &counters, sizeof ( counters ) ) )
            return MFX_ERR_ABORTED;

        g_tsLog << "  Process size before MFXInit = " << m_counters_init.PrivateUsage <<" bytes \n";
        g_tsLog << "  Process size after MFXClose = " << counters.PrivateUsage <<" bytes \n";

        //Let's put 20% threshold
        if((m_counters_init.PrivateUsage + m_counters_init.PrivateUsage / 5) < counters.PrivateUsage)
        {
            g_tsLog << "ERROR: Possible memory leaks detected\n";
            return MFX_ERR_ABORTED;
        }

        CloseHandle(m_process);
        m_process = 0;

        return MFX_ERR_NONE;
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        s;
        ++count;

        PROCESS_MEMORY_COUNTERS_EX counters;
        memset(&counters,0,sizeof(counters));
        counters.cb = sizeof(counters);

        if(skip == count)
        {
            GetProcessMemoryInfo ( m_process, (PROCESS_MEMORY_COUNTERS*) &counters, sizeof ( counters ) );
            m_counters_runtime = counters;

            return MFX_ERR_NONE;
        }
        else if(count > skip)
        {
            if(!m_process || (0 == m_counters_runtime.PrivateUsage))
                return MFX_ERR_NOT_INITIALIZED;

            if( !GetProcessMemoryInfo ( m_process, (PROCESS_MEMORY_COUNTERS*) &counters, sizeof ( counters ) ) )
            {
                g_tsLog << "TEST ERROR: failed to get current process size\n";
                return MFX_ERR_ABORTED;
            }

            if((m_counters_runtime.PrivateUsage + threshold) < counters.PrivateUsage)
            {
                g_tsLog << "ERROR: Unexpected runtime memory growth detected\n";
                return MFX_ERR_ABORTED;
            }
        }

        return MFX_ERR_NONE;
    }

private:
    HANDLE m_process;
    mfxU32 threshold; //bytes
    mfxU32 skip;
    mfxU32 count;
    PROCESS_MEMORY_COUNTERS_EX m_counters_init;
    PROCESS_MEMORY_COUNTERS_EX m_counters_runtime;
};
#else
class MemoryTracker : public tsSurfaceProcessor
{
public:
    MemoryTracker() { }
    virtual ~MemoryTracker() { }
    mfxStatus Start() {return MFX_ERR_UNSUPPORTED;}
    mfxStatus Stop() {return MFX_ERR_UNSUPPORTED;}
    mfxStatus ProcessSurface(mfxFrameSurface1& s) {s; return MFX_ERR_UNSUPPORTED;}
};
#endif

class TestSuite : tsVideoDecoder, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_CAPTURE, true, MFX_MAKEFOURCC('C','A','P','T'))
    {
        m_surf_processor = this;
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (s.Data.TimeStamp != -1)
        {
            g_tsLog << "ERROR: s.Data.TimeStamp == " << s.Data.TimeStamp << ". Should be -1\n";
            return MFX_ERR_ABORTED;
        }
        return MFX_ERR_NONE;
    }

private:
    enum
    {
        MFXPAR = 1,
    };

    enum
    {
        CHECK_MEMORY = 1,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*01*/ MFX_ERR_NONE, 0, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY }},
    {/*02*/ MFX_ERR_NONE, 0, { {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                               {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_RGB4},
                               {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444},  }
    },
    {/*03*/ MFX_ERR_NONE, 0, { {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                               {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_RGB4},
                               {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444},  }
    },
    {/*04*/ MFX_ERR_NONE, CHECK_MEMORY, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*05*/ MFX_ERR_NONE, CHECK_MEMORY, {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY }},
    {/*06*/ MFX_ERR_NONE, CHECK_MEMORY, { {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                                          {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_RGB4},
                                          {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444},  }
    },
    {/*07*/ MFX_ERR_NONE, CHECK_MEMORY, { {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY},
                                          {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_RGB4},
                                          {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444},  }
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    mfxU32 n_frames = 3;
    MemoryTracker memTracker;
    if(CHECK_MEMORY == tc.mode)
    {
        g_tsStatus.check( memTracker.Start() );
        m_surf_processor = &memTracker;
        n_frames = 1000;
        g_tsTrace = 0;
    }

    MFXInit();
    Load();

    SETPARS(m_pPar, MFXPAR);
    m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.CropW = 4096;
    m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 4096;
    if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(g_tsImpl))
    {
        mfxHandleType type;
        mfxHDL hdl;
        UseDefaultAllocator( !!(m_pPar->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) );
        if(GetAllocator() && GetAllocator()->get_hdl(type, hdl))
            SetHandle(m_session, type, hdl);
    }
    Init(m_session, m_pPar);

    g_tsStatus.expect(tc.sts);
    DecodeFrames(n_frames);

    if(CHECK_MEMORY == tc.mode)
    {
        //Free test-controlled resources as much as possible
        Close();
        UnLoad();
        MFXClose();
        SetAllocator(0,false);

        g_tsStatus.check( memTracker.Stop() );
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(screen_capture_decode_frame_async);

}