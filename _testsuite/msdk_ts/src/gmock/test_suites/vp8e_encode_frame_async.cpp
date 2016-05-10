/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace vp8e_encode_frame_async
{
class TestSuite : public tsVideoEncoder, public tsBitstreamProcessor
{
private:
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_VP8)
    {
        srand(0);
        m_bs_processor = this;
    }

    ~TestSuite() {}

    enum
    {
        PRE_INIT = 1
        , SURF
        , BS
        , MFX
        , NULL_SESSION
        , NULL_CTRL
        , NULL_SURFACE
        , NULL_BITSTREAM
        , NULL_SP
        , CLOSE_ENC
        , ASYNC
    };

    static const unsigned int n_cases;
    int RunTest(unsigned int id);

    struct tc_struct
    {
        mfxStatus sts;

        mfxU32 preInitMode;
        struct pre_init
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];

        mfxU32 preEncMode;
        struct pre_encode
         {
             mfxU32 ext_type;
             const  tsStruct::Field* f;
             mfxU32 v;
         } set_par_pre_encode[MAX_NPARS];
    };

    static const tc_struct test_case[];

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        tsParserVP8 p(m_bitstream);
        //wr.ProcessBitstream(m_bitstream, 1);

        for (mfxU32 i = 0; i < nFrames; i++)
        {
            tsParserVP8::UnitType& hdr = p.ParseOrDie();

            EXPECT_EQ(m_par.mfx.CodecProfile - 1, (int)hdr.udc->version);

            if(!hdr.udc->key_frame)
            {
                EXPECT_EQ(m_par.mfx.FrameInfo.Width, hdr.udc->Width);
                EXPECT_EQ(m_par.mfx.FrameInfo.Height, hdr.udc->Height);
            }

            if((mfxExtVP8CodingOption*)m_par)
            {
                mfxExtVP8CodingOption* co =
                        (mfxExtVP8CodingOption*)m_par.GetExtBuffer(MFX_EXTBUFF_VP8_CODING_OPTION);
                if(co->EnableMultipleSegments)
                {
                    EXPECT_EQ(mfxU32(co->EnableMultipleSegments == MFX_CODINGOPTION_ON),
                              hdr.udc->fh.segmentation_enabled);
                }
            }
        }

        return MFX_ERR_NONE;
    }

    mfxStatus EncodeFrames2(mfxU32 n)
    {
        mfxU32 encoded = 0;
        mfxU32 submitted = 0;
        mfxU32 async = TS_MAX(1, m_par.AsyncDepth);
        mfxSyncPoint sp;

        async = TS_MIN(n, async - 1);

        while(encoded < n)
        {
            if(MFX_ERR_MORE_DATA == EncodeFrameAsync())
            {
                if(!m_pSurf)
                {
                    if(submitted)
                    {
                        encoded += submitted;
                        SyncOperation(sp);
                    }
                    break;
                }
                continue;
            }

            if (g_tsStatus.m_expected != MFX_ERR_NONE) {
                return g_tsStatus.m_expected;
            }

            g_tsStatus.check();TS_CHECK_MFX;
            sp = m_syncpoint;

            if(++submitted >= async)
            {
                SyncOperation();TS_CHECK_MFX;
                encoded += submitted;
                submitted = 0;
                async = TS_MIN(async, (n - encoded));
            }
        }

        g_tsLog << encoded << " FRAMES ENCODED\n";

        return g_tsStatus.get();
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/
     // Before calling any SDK functions, the application must create an SDK session.
     MFX_ERR_INVALID_HANDLE, 0, {}, NULL_SESSION, {} },
    {/* 1*/ MFX_ERR_NONE, 0, {}, NULL_CTRL, {}},
    {/* 2*/
     // Not enough surfaces.
     MFX_ERR_MORE_DATA, 0, {}, NULL_SURFACE, {} },
    {/* 3*/
     // Bitstream pointer is NULL.
     MFX_ERR_NULL_PTR, 0, {}, NULL_BITSTREAM, {} },
    {/* 4*/
     //Sync Point pointer is NULL.
     MFX_ERR_NULL_PTR, 0, {}, NULL_SP, {} },
    {/* 5*/
     // Closed encoder, then trying to call EncodeFrames().
     MFX_ERR_NOT_INITIALIZED, 0, {}, CLOSE_ENC, {} },
    {/* 6*/
     //Allocated bitstream buffer size is insufficient.
     MFX_ERR_NOT_ENOUGH_BUFFER, 0, {}, BS, {BS, &tsStruct::mfxBitstream.MaxLength, 100} },
    {/* 7*/
     // Next reading/writing position may not be equal to -1.
     MFX_ERR_UNDEFINED_BEHAVIOR, 0, {}, BS, {BS,  &tsStruct::mfxBitstream.DataOffset, 0xFFFFFFFF} },
    {/* 8*/
     // The application has to specify Y pointer.
     MFX_ERR_UNDEFINED_BEHAVIOR, 0, {}, SURF, {SURF, &tsStruct::mfxFrameSurface1.Data.Y, 0} },
    {/* 9*/
     // Memory ID of the data buffers may not be equal to 0.
     MFX_ERR_UNDEFINED_BEHAVIOR,
     PRE_INIT, { MFX, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY},
     SURF, { SURF, &tsStruct::mfxFrameSurface1.Data.MemId, 0} },
    {/*10*/
     // Bitstream buffer pointer is 0.
     MFX_ERR_NULL_PTR, 0, {}, BS, {BS, &tsStruct::mfxBitstream.Data, 0} },
    {/*11*/ MFX_ERR_NONE, PRE_INIT, {MFX, &tsStruct::mfxVideoParam.AsyncDepth, 4}, ASYNC, { ASYNC, 0, 4} },
    {/*12*/ MFX_ERR_NONE, PRE_INIT, { MFX, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_VP8_0}, 0, {} },
    {/*13*/ MFX_ERR_NONE, PRE_INIT, { MFX, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_VP8_1}, 0, {} },
    {/*14*/ MFX_ERR_NONE, PRE_INIT, { MFX, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_VP8_2}, 0, {} },
    {/*15*/ MFX_ERR_NONE, PRE_INIT, { MFX, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_VP8_3}, 0, {} }
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];

    MFXInit();
    Load();

    m_par.AsyncDepth = 1;
    m_par.mfx.CodecProfile = MFX_PROFILE_VP8_0;

    //tsBitstreamWriter wr("0000.vp8");
    if(tc.preInitMode == PRE_INIT)
        SETPARS(m_pPar, MFX);

    Init();

    AllocSurfaces();
    AllocBitstream();

    mfxSession tmp_session = m_session;
    if (tc.preEncMode == NULL_SESSION)
        m_session = 0;
    else if (tc.preEncMode == NULL_CTRL)
        m_pCtrl = 0;
    else if (tc.preEncMode == NULL_SURFACE)
        m_pSurf = 0;
    else if (tc.preEncMode == NULL_BITSTREAM)
        m_pBitstream = 0;
    else if (tc.preEncMode == NULL_SP)
        m_pSyncPoint = 0;
    else if (tc.preEncMode == CLOSE_ENC)
    {
        Close();
        m_default = false;
    }

    if (tc.preEncMode == BS)
    {
        for(mfxU32 i = 0; i < MAX_NPARS; i++)
        {
            if(tc.set_par_pre_encode[i].f && tc.set_par_pre_encode[i].ext_type == BS)
            {
                tsStruct::set(m_pBitstream, *tc.set_par_pre_encode[i].f, tc.set_par_pre_encode[i].v);
            }
        }
    }

    if (tc.preEncMode == SURF)
    {
        m_default = false;
        m_pSurf = GetSurface();
        for(mfxU32 i = 0; i < MAX_NPARS; i++)
        {
            if(tc.set_par_pre_encode[i].f && tc.set_par_pre_encode[i].ext_type == SURF)
            {
                tsStruct::set(m_pSurf, *tc.set_par_pre_encode[i].f, tc.set_par_pre_encode[i].v);
            }
        }
    }
    g_tsStatus.expect(tc.sts);

    if (tc.preEncMode == ASYNC)
    {
        for(mfxI32 i = 1; i < (mfxI64)tc.set_par_pre_encode[0].v; i ++)
        {
            EncodeFrameAsync();
            if(g_tsStatus.get() == MFX_ERR_MORE_DATA)
                i --;
            else g_tsStatus.check();
        }
    }
    else
        EncodeFrames2(1);

    if (tc.preEncMode == NULL_SESSION)
        m_session = tmp_session;
    g_tsStatus.expect(MFX_ERR_NONE);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vp8e_encode_frame_async);
};
