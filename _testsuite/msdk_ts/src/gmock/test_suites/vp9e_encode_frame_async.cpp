/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

namespace vp9e_encode_frame_async
{
    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_VP9)
        {

        }
        ~TestSuite() {}
        int RunTest(unsigned int id);

    private:
        enum
        {
            MFX_PAR,
            MFX_BS,
            MFX_SURF,
            NULL_SESSION,
            NULL_BS,
            NULL_SURF,
            NOT_INIT,
            FAILED_INIT,
            CLOSED,
            CROP_XY,
            SYSTEM_MEMORY,
            NULL_MEMID,
            NONE
        };

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 type;
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
        //correct default params
        {/*00*/ MFX_ERR_NONE, NONE },

        //check NULL params/ptrs
        {/*01*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION },
        {/*02*/ MFX_ERR_NULL_PTR, NULL_BS },
        {/*03*/ MFX_ERR_MORE_DATA, NULL_SURF },
        {/*04*/ MFX_ERR_NOT_INITIALIZED, NOT_INIT },
        {/*05*/ MFX_ERR_NOT_INITIALIZED, FAILED_INIT },
        {/*06*/ MFX_ERR_NOT_ENOUGH_BUFFER, NONE,
            { MFX_BS, &tsStruct::mfxBitstream.MaxLength, 10 }
        },
        {/*07*/ MFX_ERR_UNDEFINED_BEHAVIOR, NONE,
            { MFX_BS, &tsStruct::mfxBitstream.DataOffset, 0xFFFFFFFF }
        },
        {/*08*/ MFX_ERR_NULL_PTR, NONE,

            { MFX_BS, &tsStruct::mfxBitstream.Data, 0 }
        },
        {/*09*/ MFX_ERR_NOT_ENOUGH_BUFFER, NONE,
            { MFX_BS, &tsStruct::mfxBitstream.MaxLength, 0 },
        },
        {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, NULL_MEMID, {}
        },

        //check additional frame rate
        {/*11*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 }
            }
        },

        //check crop
        {/*12*/ MFX_ERR_NONE, CROP_XY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
            }
        },

        //cases for system memory on input surface
        {/*13*/ MFX_ERR_NONE, SYSTEM_MEMORY},

        //NB: this case is incorrect for Y410 (use Y410-ptr instead)
        {/*14*/ MFX_ERR_NULL_PTR, SYSTEM_MEMORY,
            { MFX_SURF, &tsStruct::mfxFrameSurface1.Data.Y, 0 }
        },
        {/*15*/ MFX_ERR_UNDEFINED_BEHAVIOR, SYSTEM_MEMORY,
            { MFX_SURF, &tsStruct::mfxFrameSurface1.Data.PitchHigh, 0x8000 }
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];
        const char* stream = g_tsStreamPool.Get("YUV/720x480p_30.00_4mb_h264_cabac_180s.yuv");
        g_tsStreamPool.Reg();
        tsSurfaceProcessor *reader;
        mfxStatus sts;

        MFXInit();
        Load();

        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 720;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;
        // Currently only VIDEO_MEMORY is supported
        if(tc.type == SYSTEM_MEMORY)
        {
            m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        }
        else
        {
            m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        }

        SETPARS(m_pPar, MFX_PAR);

        InitAndSetAllocator();

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            if (g_tsHWtype < MFX_HW_CNL) // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
        } else {
            g_tsLog << "WARNING: loading encoder from plugin failed!\n";
            return 0;
        }

        reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
        m_filler = reader;

        //init
        if (tc.type == FAILED_INIT)
        {
            g_tsStatus.expect(MFX_ERR_NULL_PTR);
            Init(m_session, NULL);
        }
        else if (tc.type != NOT_INIT)
        {
            Init();
            
            // set test param
            if(tc.sts < MFX_ERR_NONE)
            {
                AllocBitstream(); TS_CHECK_MFX;
                SETPARS(m_pBitstream, MFX_BS);
                AllocSurfaces(); TS_CHECK_MFX;
                m_pSurf = GetSurface(); TS_CHECK_MFX;
                if (m_filler)
                {
                    m_pSurf = m_filler->ProcessSurface(m_pSurf, m_pFrameAllocator);
                }
            }
            
            SETPARS(m_pSurf, MFX_SURF);
        }

        if (tc.type == CLOSED)
        {
            Close();
        }

        if(tc.type == NULL_MEMID)
        {
            m_pSurf->Data.MemId = 0;
        }

        if (tc.sts >= MFX_ERR_NONE)
        {
            int encoded = 0;
            while (encoded < 1)
            {
                if (MFX_ERR_MORE_DATA == EncodeFrameAsync())
                {
                    continue;
                }

                g_tsStatus.check(); TS_CHECK_MFX;
                SyncOperation(); TS_CHECK_MFX;
                encoded++;
            }
            sts = tc.sts;
        }
        else if (tc.type == NULL_MEMID)
        {
            sts = EncodeFrameAsync(m_session, m_pSurf ? m_pCtrl : 0, m_pSurf, m_pBitstream, m_pSyncPoint);
        }
        else if (tc.type == NULL_SESSION)
        {
            sts = EncodeFrameAsync(NULL, m_pSurf ? m_pCtrl : 0, m_pSurf, m_pBitstream, m_pSyncPoint);
        }
        else if (tc.type == NULL_BS)
        {
            sts = EncodeFrameAsync(m_session, m_pSurf ? m_pCtrl : 0, m_pSurf, NULL, m_pSyncPoint);
        }
        else if (tc.type == NULL_SURF)
        {
            sts = EncodeFrameAsync(m_session, m_pSurf ? m_pCtrl : 0, NULL, m_pBitstream, m_pSyncPoint);
        }
        else
        {
            sts = EncodeFrameAsync(m_session, m_pSurf ? m_pCtrl : 0, m_pSurf, m_pBitstream, m_pSyncPoint);
        }
        g_tsStatus.expect(tc.sts);
        g_tsStatus.check(sts);
        if (tc.sts != MFX_ERR_NOT_INITIALIZED)
            g_tsStatus.expect(MFX_ERR_NONE);
        else
            g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(vp9e_encode_frame_async);
};
