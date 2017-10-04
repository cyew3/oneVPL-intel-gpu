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

        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC)
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
            NSLICE_EQ_MAX,
            NSLICE_GT_MAX,
            NSLICE_LT_MAX,
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
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, 51 }
        }
        },
        {/*11*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 }
            }
        },
        {/*12*/ MFX_ERR_NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW, 16 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH, 9 }
            }
        },
        {/*13*/ MFX_ERR_NONE, CROP_XY,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480 - 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
            }
        },
        {/*14*/ MFX_ERR_NONE, NSLICE_GT_MAX },  // NumSlice > MAX
        {/*15*/ MFX_ERR_NONE, NSLICE_LT_MAX },  // NumSlice < MAX
        {/*16*/ MFX_ERR_NONE, NSLICE_EQ_MAX },  // NumSlice == MAX

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];
        mfxHDL hdl;
        mfxHandleType type;
        const char* stream = g_tsStreamPool.Get("YUV/720x480p_30.00_4mb_h264_cabac_180s.yuv");
        g_tsStreamPool.Reg();
        tsSurfaceProcessor *reader;
        mfxStatus sts;

        MFXInit();
        Load();

        //set defoult param
        m_par.mfx.FrameInfo.Width = 736;
        m_par.mfx.FrameInfo.Height = 480;


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


        mfxU16 LCUSize = 64;    // LCUSize=32 is for internal use only (ICL DP mode) and is not supposeed to be tested
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

            if (tc.type == NSLICE_GT_MAX) {
                m_pPar->mfx.NumSlice = maxSlices + 1;
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            } else if (tc.type == NSLICE_LT_MAX) {
                m_pPar->mfx.NumSlice = maxSlices - 1;
            } else if (tc.type == NSLICE_EQ_MAX) {
                m_pPar->mfx.NumSlice = maxSlices;
            }

            Init();

            // set test param
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

    TS_REG_TEST_SUITE_CLASS(hevce_encode_frame_async);
};