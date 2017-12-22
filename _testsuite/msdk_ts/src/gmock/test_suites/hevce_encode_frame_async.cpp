/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2017 Intel Corporation. All Rights Reserved.

File Name: hevce_encode_frame_async.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include "ts_aux_dev.h"

namespace hevce_encode_frame_async
{
    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        static const unsigned int n_cases_nv12;
        static const unsigned int n_cases_yuy2;
        static const unsigned int n_cases_y210;

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

        static const tc_struct test_case_nv12[];
        static const tc_struct test_case_yuy2[];
        static const tc_struct test_case_y210[];

        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
        {

        }
        ~TestSuite() {}

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);

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
            NSLICE_EQ_MAX,
            NSLICE_GT_MAX,
            NSLICE_LT_MAX,
            NONE
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION },
        {/*01*/ MFX_ERR_NULL_PTR, NULL_BS },
        {/*02*/ MFX_ERR_MORE_DATA, NULL_SURF },
        {/*03*/ MFX_ERR_NOT_INITIALIZED, NOT_INIT },
        {/*04*/ MFX_ERR_NOT_INITIALIZED, FAILED_INIT },
        {/*05*/ MFX_ERR_NOT_ENOUGH_BUFFER, NONE,
            { MFX_BS, &tsStruct::mfxBitstream.MaxLength, 10 }
        },
        {/*06*/ MFX_ERR_UNDEFINED_BEHAVIOR, NONE,
            { MFX_BS, &tsStruct::mfxBitstream.DataOffset, 0xFFFFFFFF }
        },
        {/*07*/ MFX_ERR_NOT_ENOUGH_BUFFER, NONE,
            {
                { MFX_BS, &tsStruct::mfxBitstream.MaxLength, 0 },
                { MFX_BS, &tsStruct::mfxBitstream.Data, 0 }
            }
        },
        {/*08*/ MFX_ERR_UNDEFINED_BEHAVIOR, NONE,
            { MFX_SURF, &tsStruct::mfxFrameSurface1.Data.Y, 0 }
        },
        {/*09*/ MFX_ERR_NONE, NONE },
        {/*10*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 }
            }
        },
        {/*11*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW, 16 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH, 9 }
            }
        },

        {/*12*/ MFX_ERR_NONE, NSLICE_GT_MAX },  // NumSlice > MAX
        {/*13*/ MFX_ERR_NONE, NSLICE_LT_MAX },  // NumSlice < MAX
        {/*14*/ MFX_ERR_NONE, NSLICE_EQ_MAX },  // NumSlice == MAX
    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const TestSuite::tc_struct TestSuite::test_case_nv12[] =
    {
        {/*15*/ MFX_ERR_NONE, CROP_XY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
            }
        },
        {/*16*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, 51 }
            }
        },
    };
    const unsigned int TestSuite::n_cases_nv12 = sizeof(TestSuite::test_case_nv12) / sizeof(TestSuite::tc_struct) + n_cases;

    const TestSuite::tc_struct TestSuite::test_case_yuy2[] =
    {
        {/*15*/ MFX_ERR_NONE, CROP_XY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288 - 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
            }
        },
        {/*16*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_REXT },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, 51 }
            }
        },
    };
    const unsigned int TestSuite::n_cases_yuy2 = sizeof(TestSuite::test_case_yuy2) / sizeof(TestSuite::tc_struct) + n_cases;

    const TestSuite::tc_struct TestSuite::test_case_y210[] =
    {
        {/*15*/ MFX_ERR_NONE, CROP_XY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288 - 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
            }
        },
        {/*16*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_REXT },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, 51 }
            }
        },
    };
    const unsigned int TestSuite::n_cases_y210 = sizeof(TestSuite::test_case_y210) / sizeof(TestSuite::tc_struct) + n_cases;

    const TestSuite::tc_struct* getTestTable(const mfxU32& fourcc)
    {
        switch (fourcc)
        {
            case MFX_FOURCC_NV12: return TestSuite::test_case_nv12;
            case MFX_FOURCC_YUY2: return TestSuite::test_case_yuy2;
            case MFX_FOURCC_Y210: return TestSuite::test_case_y210;
            default: assert(0); return 0;
        }
    }

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct* fourcc_table = getTestTable(fourcc);
        const unsigned int real_id = (id < n_cases) ? (id) : (id - n_cases);
        const tc_struct& tc = (real_id == id) ? test_case[real_id] : fourcc_table[real_id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;

        mfxHDL hdl;
        mfxHandleType type;
        const char* stream = nullptr;
        tsSurfaceProcessor *reader;
        mfxStatus sts;

        //set default param
        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            stream = g_tsStreamPool.Get("YUV/720x480p_30.00_4mb_h264_cabac_180s.yuv");

            m_par.mfx.FrameInfo.Width = 736;
            m_par.mfx.FrameInfo.Height = 480;
        }
        else if (fourcc_id == MFX_FOURCC_YUY2)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            stream = g_tsStreamPool.Get("YUV8bit422/Kimono1_352x288_24_yuy2.yuv");

            m_par.mfx.FrameInfo.Width = 352;
            m_par.mfx.FrameInfo.Height = 288;
        }
        else if (fourcc_id == MFX_FOURCC_Y210)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

            stream = g_tsStreamPool.Get("YUV10bit422/Kimono1_352x288_24_y210.yuv");

            m_par.mfx.FrameInfo.Width = 352;
            m_par.mfx.FrameInfo.Height = 288;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        mfxU32 LCUSize = (g_tsHWtype >= MFX_HW_CNL) ? 64 : 32;

        g_tsStreamPool.Reg();

        MFXInit();
        Load();

        if (!GetAllocator())
        {
            UseDefaultAllocator(
                (m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
                );
        }

        //set handle
        if (!((m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
            && (!m_pVAHandle))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
            m_pVAHandle = m_pFrameAllocator;
            m_pVAHandle->get_hdl(type, hdl);
            SetHandle(m_session, type, hdl);
            m_is_handle_set = (g_tsStatus.get() >= 0);
        }

        SETPARS(m_pPar, MFX_PAR);

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
        {
            if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }

            if ((m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210 || m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_YUY2)
                && (g_tsHWtype < MFX_HW_ICL || g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
            {
                g_tsLog << "\n\nWARNING: 422 format only supported on ICL+ and ENC+PAK!\n\n\n";
                throw tsSKIP;
            }

            //HEVCE_HW need aligned width and height for 32
            m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
            m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));
        }
        else
        {
            if (tc.type == CROP_XY)
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        }

        reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
        m_filler = reader;

        ENCODE_CAPS_HEVC caps = {};
        mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);

        g_tsStatus.check(GetCaps(&caps, &capSize));

        //g_tsLog << "CAPS: LCUSizeSupported = " << caps.LCUSizeSupported << "\n";
        //g_tsLog << "CAPS: SliceStructure = " << caps.SliceStructure << "\n";
        //g_tsLog << "CAPS: BlockSize = " << (mfxU32)caps.BlockSize << "\n";
        //g_tsLog << "CAPS: ROICaps = " << (mfxU32)caps.ROICaps << "\n";

        //init
        if (tc.type == FAILED_INIT)
        {
            g_tsStatus.expect(MFX_ERR_NULL_PTR);
            Init(m_session, NULL);
        }
        else if (tc.type != NOT_INIT)
        {
            mfxU32 nLCUrow = CEIL_DIV(m_pPar->mfx.FrameInfo.CropW, LCUSize);
            mfxU32 nLCUcol = CEIL_DIV(m_pPar->mfx.FrameInfo.CropH, LCUSize);
            mfxU32 maxSlices = 0;
            bool RowAlignedSlice = false;
            if ((caps.SliceStructure == 2) || (m_pPar->mfx.LowPower & MFX_CODINGOPTION_ON))
                RowAlignedSlice = true;

            if (RowAlignedSlice)
                maxSlices = nLCUcol;
            else
                maxSlices = nLCUrow * nLCUcol;

            //check current supported MAX number of slices
            m_pPar->mfx.NumSlice = maxSlices;
            g_tsStatus.disable_next_check();
            Query();

            if (tc.type == NSLICE_GT_MAX) {
                m_pPar->mfx.NumSlice = m_pParOut->mfx.NumSlice + 1;
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            } else if (tc.type == NSLICE_LT_MAX) {
                /*
                In case of test type = NSLICE_LT_MAX, value for NumSlices was calculated like (maxSlices - 1),
                but it is incorrect for VDEnc (SliceStructure = 2), because for this encoder mode we can specify for NumSlice
                only those values, which are composed of any number of rows, but all must have the same size, except last one, which can be
                smaller or equal to previous slices. For example,if maxSlices = 8 (resolution is 720x480), we cannot specify NumSlice = 7 
                to check test case NSLICE_LT_MAX. The most closed correct value, that is less than max value, will be 8/2 = 4. 
                For odd values (i.e. maxSlices = 5) correct value for NumSlice will be the result of a similar division, but with rounding(5/2 ~= 3). 
                Thus, it is most convenient to use CEIL_DIV.
                */
                if (caps.SliceStructure == 2 || (m_pPar->mfx.LowPower & MFX_CODINGOPTION_ON)) {
                    m_pPar->mfx.NumSlice = CEIL_DIV(m_pParOut->mfx.NumSlice, 2);
                } else {
                    m_pPar->mfx.NumSlice = m_pParOut->mfx.NumSlice - 1;
                }
            } else if (tc.type == NSLICE_EQ_MAX) {
                m_pPar->mfx.NumSlice = m_pParOut->mfx.NumSlice;
            }

            Init();

            //set test param
            AllocBitstream(); TS_CHECK_MFX;
            SETPARS(m_pBitstream, MFX_BS);
            AllocSurfaces(); TS_CHECK_MFX;
            m_pSurf = GetSurface(); TS_CHECK_MFX;
            if (m_filler)
            {
                m_pSurf = m_filler->ProcessSurface(m_pSurf, m_pFrameAllocator);
            }
            SETPARS(m_pSurf, MFX_SURF);
        }

        if (tc.type == CLOSED)
        {
            Close();
        }

        //call test function
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

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_encode_frame_async, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases_nv12);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_encode_frame_async, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases_yuy2);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_encode_frame_async, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases_y210);
};
