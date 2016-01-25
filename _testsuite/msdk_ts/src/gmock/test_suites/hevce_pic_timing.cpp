#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"


namespace hevce_pic_timing
{
    struct tc_struct
    {
        mfxU32 pic_struct;
        mfxU32 source_scan_type;
        mfxU32 dublicate_flag;
        mfxU32 pic_timing;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
        {

        }
        ~TestSuite() {}
        int RunTest(unsigned int id);

    private:
        enum
        {
            ON,
            OFF,
            MFX_PAR,
            NONE
        };
        static const tc_struct test_case[];
    };

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserHEVC
    {
    private:
        mfxU32 pic_timing;
        mfxU32 pic_struct;
        mfxU32 source_scan_type;
        mfxU32 dublicate_flag;
        bool skip;
        enum
        {
            NAL_UT_PREFIX_SEI = 39,
            SEI_PIC_TIMING = 1
        };
    public:
        BitstreamChecker(tc_struct tc)
        {
            pic_timing = tc.pic_timing;
            pic_struct = tc.pic_struct;
            source_scan_type = tc.source_scan_type;
            dublicate_flag = tc.dublicate_flag;
            skip = false;
        }
        void SetSkip()
        {
            skip = true;
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
    };

    mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 res = 0;
        SetBuffer(bs);

        UnitType& au = ParseOrDie();

        for (mfxU32 i = 0; i < au.NumUnits; i++)
        {
            if (au.nalu[i]->nal_unit_type == NAL_UT_PREFIX_SEI && au.nalu[i]->sei->NumMessage)
                for (mfxU32 j = 0; j < 64; j++)
                    if (au.nalu[i]->sei->message[j].payloadType == SEI_PIC_TIMING && au.nalu[i]->sei->message[j].payloadSize && au.nalu[i]->sei->message[j].pt)
                        {
                            if (au.nalu[i]->sei->message[j].pt->pic_struct == pic_struct &&
                                au.nalu[i]->sei->message[j].pt->source_scan_type == source_scan_type &&
                                au.nalu[i]->sei->message[j].pt->duplicate_flag == dublicate_flag)
                                    res = 1;
                        }
        }

        if (!skip)
        {
            if (pic_timing == MFX_CODINGOPTION_ON)
            {
                if (!res)
                {
                    g_tsLog << "ERROR: INVALID BITSTREAM\n";
                    g_tsStatus.m_failed = true;
                    if (g_tsStatus.m_throw_exceptions)
                        throw tsFAIL;
                }
            }
            else
            {
                if (res)
                {
                    g_tsLog << "ERROR: INVALID BITSTREAM\n";
                    g_tsStatus.m_failed = true;
                    if (g_tsStatus.m_throw_exceptions)
                        throw tsFAIL;
                }
            }
        }
        else
        {
            g_tsLog << "WARNING: CASE WAS SKIPPED\n";
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    const tc_struct TestSuite::test_case[] =
    {

        {/*00*/ 0, 0, 0, MFX_CODINGOPTION_UNKNOWN },
        {/*01*/ 0, 0, 0, MFX_CODINGOPTION_OFF },
        //PicStruct
        {/*02*/ 0, 1, 0, MFX_CODINGOPTION_ON,
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        },
        {/*03*/ 0, 1, 0, MFX_CODINGOPTION_ON,
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 0 },
        },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        mfxExtBuffer *extCodingOptions = NULL;
        try
        {
            const tc_struct& tc = test_case[id];
            mfxHDL hdl;
            mfxHandleType type;
            const char* stream = g_tsStreamPool.Get("forBehaviorTest/foster_720x576.yuv");
            g_tsStreamPool.Reg();

            MFXInit();
            Load();

            //set defoult param
            m_par.mfx.FrameInfo.Width = 736;
            m_par.mfx.FrameInfo.Height = 576;
            m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;

            if (!GetAllocator())
            {
                UseDefaultAllocator(
                    (m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                    || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
                    );
            }

            //init ext buffers
            extCodingOptions = (mfxExtBuffer*)(new mfxExtCodingOption());
            memset(extCodingOptions, 0, sizeof(mfxExtCodingOption));
            extCodingOptions->BufferId = MFX_EXTBUFF_CODING_OPTION;
            extCodingOptions->BufferSz = sizeof(mfxExtCodingOption);
            ((mfxExtCodingOption*)extCodingOptions)->PicTimingSEI = tc.pic_timing;

            m_par.NumExtParam = 1;
            m_par.ExtParam = new mfxExtBuffer*[m_par.NumExtParam];
            m_par.ExtParam[0] = extCodingOptions;

            SETPARS(m_pPar, MFX_PAR);

            tsSurfaceProcessor *reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
            m_filler = reader;

            //set bs processor
            BitstreamChecker bs_proc(tc);
            m_bs_processor = &bs_proc;

            if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
            {
                if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
                {
                    g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                    g_tsLog << "WARNING: Unsupported HW Platform!\n";
                    Query();
                    return 0;
                }

                //HEVCE_HW need aligned width and height for 32
                m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
                m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));


                //set handle
                if (!((m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                    || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)))
                {
                    m_pFrameAllocator = GetAllocator();
                    SetFrameAllocator();
                    m_pVAHandle = m_pFrameAllocator;
                    m_pVAHandle->get_hdl(type, hdl);
                    SetHandle(m_session, type, hdl);
                    m_is_handle_set = (g_tsStatus.get() >= 0);
                }

            }
            else
            {
                if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
                    bs_proc.SetSkip();
            }

            //init
            Init();

            //test body
            EncodeFrames(10);

        }
        catch (tsRes r)
        {
            if (extCodingOptions)
                delete extCodingOptions;
            extCodingOptions = NULL;
            return r;
        }

        if (extCodingOptions)
            delete extCodingOptions;
        extCodingOptions = NULL;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_pic_timing);
};