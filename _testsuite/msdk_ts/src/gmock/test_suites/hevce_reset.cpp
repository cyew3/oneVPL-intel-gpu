/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2017 Intel Corporation. All Rights Reserved.

File Name: hevce_reset.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

namespace hevce_reset
{

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC){}
        ~TestSuite() { }
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:

        struct tc_struct
        {
            mfxStatus sts_sw;
            mfxStatus sts_hw;
            mfxU32 type;
            mfxU32 sub_type;
            mfxU32 repeat;
            const std::string stream[2];
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        enum
        {
            MFX_PAR,
            MFX_PAR_RESET,
            HANDLE_NOT_SET,
            ALLOCK,
            MAX,
            NULL_SESSION,
            NOT_INIT,
            INIT_FAIL,
            CLOSED,
            NULL_PAR,
            NOT_SYNC,
            EXT_BUFF,
            CROP,
            IOPATTERN,
            RESOLUTION,
            FRAMERATE,
            FOURCC,
            PIC_STRUCT,
            XY,
            PROTECTED,
            CHANGE,
            WRONG,
            INVALID,
            NONE
        };

        static const tc_struct test_case[];

        mfxU32 GetBufSzById(mfxU32 BufId)
        {
            const size_t maxBuffers = sizeof(g_StringsOfBuffers) / sizeof(g_StringsOfBuffers[0]);

            for (size_t i(0); i < maxBuffers; ++i)
            {
                if (BufId == g_StringsOfBuffers[i].BufferId)
                    return g_StringsOfBuffers[i].BufferSz;
            }
            return 0;
        }

    };


    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/* 0*/ MFX_ERR_NONE, MFX_ERR_NONE, NONE, NONE, 1,
            { "", "" },

        },
        {/* 1*/ MFX_ERR_NONE, MFX_ERR_NONE, NONE, NONE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        {/* 2*/ MFX_ERR_NONE, MFX_ERR_NONE, NONE, NONE, 50,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        {/* 3*/ MFX_ERR_INVALID_HANDLE, MFX_ERR_INVALID_HANDLE, NULL_SESSION, NONE, 1,
            { "", "" },

        },
        {/* 4*/ MFX_ERR_NOT_INITIALIZED, MFX_ERR_NOT_INITIALIZED, NOT_INIT, NONE, 1,
            { "", "" },

        },
        {/* 5*/ MFX_ERR_NOT_INITIALIZED, MFX_ERR_NOT_INITIALIZED, INIT_FAIL, NONE, 1,
            { "", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 }
            }
        },
        {/* 6*/ MFX_ERR_NOT_INITIALIZED, MFX_ERR_NOT_INITIALIZED, CLOSED, NONE, 1,
            { "", "" },

        },
        {/* 7*/ MFX_ERR_NULL_PTR, MFX_ERR_NULL_PTR, NULL_PAR, NONE, 1,
            { "", "" },

        },
        //IOPattern
        {/* 8*/ MFX_ERR_NONE, MFX_ERR_NONE, NONE, NONE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY }
            }

        },
        {/* 9*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, IOPATTERN, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY }
            }

        },
        {/* 10*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, IOPATTERN, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY }
            }

        },
        {/* 11*/ MFX_ERR_NONE, MFX_ERR_NONE, IOPATTERN, NONE, 1,
            { "", "" },//can't use opaque allock
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY },
            }

        },
        {/* 12*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, IOPATTERN, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY }
            }

        },
        {/* 13*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, IOPATTERN, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY }
            }

        },
        {/* 14*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, IOPATTERN, WRONG, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.IOPattern, 0x800 }
            }

        },
        //Chroma Format
        {/* 15*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_RESERVED1 }
            }

        },
        //resolution
        {/* 16*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 + 32 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 576 +32 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720 + 32 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576 + 32 }
            }

        },
        {/* 17*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192 + 32 },
            }

        },
        {/* 18*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4320 + 32 },
            }

        },
        {/* 19*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 736 + 1 },
            }

        },
        {/* 20*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 576 + 1 },
            }

        },
        //Crops
        {/* 21*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, CROP, XY, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 10 },
            }

        },
        {/* 22*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, CROP, XY, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 10 },
            }

        },
        {/* 23*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, CROP, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 + 10 },
            }

        },
        {/* 24*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, CROP, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576 + 10 },
            }

        },
        {/* 25*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, CROP, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 736 + 10 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576 + 10 },
            }

        },
        {/* 26*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, CROP, NONE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720 - 1 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0 },

            }

        },
        {/* 27*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, CROP, NONE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576 - 1 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },

            }

        },
        {/* 28*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, CROP, NONE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720 - 1 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576 - 1 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },

            }

        },
        //Async Depth
        {/* 29*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, NONE, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.AsyncDepth, 10 }
            }

        },
        {/* 30*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, NONE, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.AsyncDepth, 1 }
            }

        },
        //Protected
        {/* 31*/ MFX_ERR_INVALID_VIDEO_PARAM,
#if (defined(LINUX32) || defined(LINUX64))
                                      MFX_ERR_INVALID_VIDEO_PARAM
#else
                                      MFX_ERR_NONE
#endif
                                                         , PROTECTED, NONE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_PAVP }
            }

        },
        {/* 32*/ MFX_ERR_INVALID_VIDEO_PARAM,
#if (defined(LINUX32) || defined(LINUX64))
                                      MFX_ERR_INVALID_VIDEO_PARAM
#else
                                      MFX_ERR_NONE
#endif
                                                         , PROTECTED, NONE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_PAVP }
            }

        },
        {/* 33*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, PROTECTED, INVALID, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.Protected, 0xfff }
            }

        },
        //Frame Rate
        {/* 34*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAMERATE, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 }
            }

        },
        {/* 35*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, FRAMERATE, WRONG, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,350 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 }
            }

        },
        //FourCC
        {/* 36*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, FOURCC, WRONG, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12 },
            }

        },
        // PicStruct
        {/* 37*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, CHANGE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 255 },
            }

        },
        // Ext Buffer
        {/* 38*/ MFX_ERR_NONE, MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION_SPSPPS, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        {/* 39*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_VIDEO_SIGNAL_INFO, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        {/* 40*/ MFX_ERR_NONE, MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        {/* 41*/ MFX_ERR_NONE, MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        {/* 42*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VPP_DOUSE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        {/* 43*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VPP_AUXDATA, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        {/* 44*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_AVC_REFLIST_CTRL, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        {/* 45*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        {/* 46*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_PICTURE_TIMING_SEI, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        {/* 47*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_ENCODER_CAPABILITY, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        {/* 48*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_ENCODER_RESET_OPTION, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        {/* 49*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NULL_PTR, EXT_BUFF, NONE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },
        //not wait finish encoding
        {/* 50*/ MFX_ERR_NONE, MFX_ERR_NONE, NOT_SYNC, NONE, 1,
            { "forBehaviorTest/foster_720x576.yuv", "" },

        },


    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START
        auto tc = test_case[id];
        mfxStatus sts = tc.sts_sw;
        tsSurfaceProcessor *reader;
        mfxHDL hdl;
        mfxHandleType type;
        mfxEncryptedData ed;
        mfxStatus init_fail = MFX_ERR_INVALID_VIDEO_PARAM;
        const char* stream0 = g_tsStreamPool.Get(tc.stream[0]);
        const char* stream1 = g_tsStreamPool.Get(tc.stream[1] == "" ? tc.stream[0] : tc.stream[1]);
        g_tsStreamPool.Reg();

        MFXInit();

        //set default param
        m_par.mfx.FrameInfo.Width = 720;
        m_par.mfx.FrameInfo.Height = 576;
        m_par.mfx.FrameInfo.CropH = 0;
        m_par.mfx.FrameInfo.CropW = 0;


        SETPARS(m_pPar, MFX_PAR);

        Load();

        if (!GetAllocator())
        {
            UseDefaultAllocator(
                (m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
                );
        }

        if ((tc.type != HANDLE_NOT_SET) && !(m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY))
        {
            if (m_pVAHandle)
            {
                SetAllocator(m_pVAHandle, true);
                m_pFrameAllocator = GetAllocator();
                SetFrameAllocator();
            }
            else
            {
                m_pFrameAllocator = GetAllocator();
                SetFrameAllocator();
                m_pFrameAllocator->get_hdl(type, hdl);
                SetHandle(m_session, type, hdl);
            }
        }

        if ((0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data))) ||
            (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_GACC.Data, sizeof(MFX_PLUGINID_HEVCE_GACC.Data))))
        {
            if ((g_tsHWtype < MFX_HW_SKL) &&
               (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                sts = MFXVideoENCODE_Query(m_session, m_pPar, m_pParOut);
                g_tsStatus.check(sts);
                return 0;
            }

            sts = tc.sts_hw;
            m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
            m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));
            init_fail = MFX_ERR_INVALID_VIDEO_PARAM;
        }

        if (tc.stream[0] != "")
        {
            reader = new tsRawReader(stream0, m_pPar->mfx.FrameInfo);
            m_filler = reader;
        }

        if (tc.type != NOT_INIT)
        {
            if (tc.type == INIT_FAIL)
            {
                m_pPar->mfx.FrameInfo.Width = m_pPar->mfx.FrameInfo.Width + 1;
                g_tsStatus.expect(init_fail);
                Init();
            }
            else
            {
                Init();
                GetVideoParam();

                if (tc.stream[0] != "")
                {
                    // Encode 1 frame
                    if (m_filler)
                        m_filler->m_max = 1;

                    int encoded = 0;
                    while (encoded < 1)
                    {
                        if (MFX_ERR_MORE_DATA == EncodeFrameAsync())
                        {
                            continue;
                        }

                        g_tsStatus.check(); TS_CHECK_MFX;

                        if (tc.type != NOT_SYNC)
                            SyncOperation(); TS_CHECK_MFX;
                        encoded++;
                    }
                }
                if (tc.type == CLOSED)
                    Close();
            }
        }

        SETPARS(m_pPar, MFX_PAR_RESET);

        if (tc.type == EXT_BUFF)
        {
            m_par.AddExtBuffer(tc.sub_type, GetBufSzById(tc.sub_type));

        }

        for (mfxU32 i = 0; i < tc.repeat; i++)
        {
            g_tsStatus.expect(sts);
            if (tc.type == NULL_SESSION)
                Reset(NULL, m_pPar);
            else if (tc.type == NULL_PAR)
                Reset(m_session, NULL);
            else
                Reset(m_session, m_pPar);

        }

        if (sts >= 0)
        {

            if (tc.stream[0] != "" || tc.stream[1] != "")
            {
                if (tc.type == PROTECTED)
                {
                    ed.DataLength = m_bitstream.DataLength;
                    ed.DataOffset = m_bitstream.DataOffset;
                    ed.MaxLength = m_bitstream.MaxLength;
                    ed.Data = m_bitstream.Data;
                    m_bitstream.EncryptedData = &ed;
                }

                if (tc.stream[1] != "")
                {
                    reader = new tsRawReader(stream1, m_pPar->mfx.FrameInfo);
                    m_filler = reader;
                }
                else if (tc.stream[0] != "")
                {
                    reader = new tsRawReader(stream0, m_pPar->mfx.FrameInfo);
                    m_filler = reader;
                }

                if (m_filler)
                    m_filler->m_max = 1;

                EncodeFrames(1, true);
            }
            Close();
        }
        else if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_SW.Data, sizeof(MFX_PLUGINID_HEVCE_SW.Data)))
        {
            if (sts == MFX_ERR_INVALID_HANDLE)
                sts = MFX_ERR_NONE;
            else
                sts = MFX_ERR_NOT_INITIALIZED;
            g_tsStatus.expect(sts);
            Close();
        }
        TS_END
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_reset);
}
