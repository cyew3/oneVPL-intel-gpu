/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2018 Intel Corporation. All Rights Reserved.

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

        struct tc_struct
        {
            mfxStatus sts_sw;
            mfxStatus sts_hw;
            mfxU32 type;
            mfxU32 sub_type;
            mfxU32 repeat;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxI32 v;
            } set_par[MAX_NPARS];
        };

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);
        static const unsigned int n_cases;

    private:
        enum
        {
            MFX_PAR,
            MFX_PAR_RESET,
            HANDLE_NOT_SET,
            NULL_SESSION,
            NO_INIT,
            INIT_FAIL,
            CLOSED,
            NULL_PAR,
            NO_SYNC,
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
            W_GT_MAX,
            H_GT_MAX,
            WRONG,
            INVALID,
            NO_ENCODE,
            DELTA,
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
        {/* 0*/ MFX_ERR_NONE, MFX_ERR_NONE, NONE, NO_ENCODE, 1,
        },
        {/* 1*/ MFX_ERR_NONE, MFX_ERR_NONE, NONE, NONE, 1,
        },
        {/* 2*/ MFX_ERR_NONE, MFX_ERR_NONE, NONE, NONE, 50,
        },
        {/* 3*/ MFX_ERR_INVALID_HANDLE, MFX_ERR_INVALID_HANDLE, NULL_SESSION, NO_ENCODE, 1,
        },
        {/* 4*/ MFX_ERR_NOT_INITIALIZED, MFX_ERR_NOT_INITIALIZED, NO_INIT, NO_ENCODE, 1,
        },
        {/* 5*/ MFX_ERR_NOT_INITIALIZED, MFX_ERR_NOT_INITIALIZED, INIT_FAIL, NO_ENCODE, 1,
        },
        {/* 6*/ MFX_ERR_NOT_INITIALIZED, MFX_ERR_NOT_INITIALIZED, CLOSED, NO_ENCODE, 1,
        },
        {/* 7*/ MFX_ERR_NULL_PTR, MFX_ERR_NULL_PTR, NULL_PAR, NO_ENCODE, 1,
        },
        //IOPattern
        {/* 8*/ MFX_ERR_NONE, MFX_ERR_NONE, NONE, NONE, 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY }
            }
        },
        {/* 9*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, IOPATTERN, CHANGE, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY }
            }
        },
        {/* 10*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, IOPATTERN, CHANGE, 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY }
            }
        },
        {/* 11*/ MFX_ERR_NONE, MFX_ERR_NONE, IOPATTERN, NO_ENCODE, 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY },
            }
        },
        {/* 12*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, IOPATTERN, CHANGE, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY }
            }
        },
        {/* 13*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, IOPATTERN, CHANGE, 1,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY }
            }
        },
        {/* 14*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, IOPATTERN, WRONG, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.IOPattern, 0x800 }
            }
        },
        //Chroma Format
        {/* 15*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, NONE, NONE, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_RESERVED1 }
            }
        },
        //resolution
        {/* 16*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, DELTA, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 64 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 64 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 64 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 64 },
            }
        },
        {/* 17*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, W_GT_MAX, 1,
         },
        {/* 18*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, RESOLUTION, H_GT_MAX, 1,
        },
        {/* 19*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, DELTA, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1 },
            }
        },
        {/* 20*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, RESOLUTION, DELTA, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1 },
            }
        },
        //Crops
        {/* 21*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, CROP, XY, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 50 },
            }
        },
        {/* 22*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, CROP, XY, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 50 },
            }
        },
        {/* 23*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, CROP, DELTA, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 50 },
            }
        },
        {/* 24*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, CROP, DELTA, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 50 },
            }
        },
        {/* 25*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, CROP, DELTA, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 50 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 50 },
            }
        },
        {/* 26*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, CROP, DELTA, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 0 },
            }
        },
        {/* 27*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, CROP, DELTA, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },

            }
        },
        {/* 28*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, CROP, DELTA, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, -1 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 1 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
            }

        },
        //Async Depth
        {/* 29*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, NONE, CHANGE, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.AsyncDepth, 10 }
            }
        },
        {/* 30*/ MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, NONE, CHANGE, 1,
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
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.Protected, MFX_PROTECTION_GPUCP_PAVP }
            }
        },
        {/* 33*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, PROTECTED, INVALID, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.Protected, 0xfff }
            }
        },
        //Frame Rate
        {/* 34*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, FRAMERATE, CHANGE, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 }
            }
        },
        {/* 35*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, FRAMERATE, WRONG, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN,350 },
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 }
            }
        },
        //FourCC
        {/* 36*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, FOURCC, WRONG, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12 },
            }
        },
        // PicStruct
        {/* 37*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, CHANGE, 1,
            {
                { MFX_PAR_RESET, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, 255 },
            }
        },
        // Ext Buffer
        {/* 38*/ MFX_ERR_NONE, MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION_SPSPPS, 1,
        },
        {/* 39*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_VIDEO_SIGNAL_INFO, 1,
        },
        {/* 40*/ MFX_ERR_NONE, MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION, 1,
        },
        {/* 41*/ MFX_ERR_NONE, MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, 1,
        },
        {/* 42*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VPP_DOUSE, 1,
        },
        {/* 43*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VPP_AUXDATA, 1,
        },
        {/* 44*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_AVC_REFLIST_CTRL, 1,
        },
        {/* 45*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION, 1,
        },
        {/* 46*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_PICTURE_TIMING_SEI, 1,
        },
        {/* 47*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_ENCODER_CAPABILITY, 1,
        },
        {/* 48*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_ENCODER_RESET_OPTION, 1,
        },
        {/* 49*/ MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NULL_PTR, EXT_BUFF, NONE, 1,
        },
        //not wait finish encoding
        {/* 50*/ MFX_ERR_NONE, MFX_ERR_NONE, NO_SYNC, NONE, 1,
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START

        mfxStatus sts = tc.sts_sw;
        tsRawReader *reader;
        mfxHDL hdl;
        mfxHandleType type;
        mfxEncryptedData ed;
        mfxStatus init_fail = MFX_ERR_INVALID_VIDEO_PARAM;
        const char* stream = NULL;
        ENCODE_CAPS_HEVC caps = {};
        mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);

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

            if ((g_tsConfig.lowpower == MFX_CODINGOPTION_ON)
                && (fourcc_id == MFX_FOURCC_YUY2 || fourcc_id == MFX_FOURCC_Y210 || fourcc_id == MFX_FOURCC_Y216))
            {
                g_tsLog << "\n\nWARNING: 4:2:2 formats are supported in HEVCe DualPipe only!\n\n\n";
                throw tsSKIP;
            }

            if ((g_tsConfig.lowpower == MFX_CODINGOPTION_OFF)
                && (fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410 || fourcc_id == MFX_FOURCC_Y416))
            {
                g_tsLog << "\n\nWARNING: 4:4:4 formats are supported in HEVCe VDENC only!\n\n\n";
                throw tsSKIP;
            }

            sts = tc.sts_hw;
        }

        if (tc.sub_type != NO_ENCODE)
        {
            if (fourcc_id == MFX_FOURCC_NV12)
            {
                m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
                m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
                m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

                if (g_tsConfig.sim) {
                    stream = g_tsStreamPool.Get("YUV/salesman_176x144_449.yuv");
                    m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                    m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
                }
                else {
                    stream = g_tsStreamPool.Get("YUV/720x480p_30.00_4mb_h264_cabac_180s.yuv");
                    m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 720;
                    m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;
                }
            }
            else if (fourcc_id == MFX_FOURCC_P010)
            {
                m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
                m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
                m_par.mfx.FrameInfo.Shift = 1;
                m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

                if (g_tsConfig.sim) {
                    stream = g_tsStreamPool.Get("YUV10bit420_ms/Kimono1_176x144_24_p010_shifted.yuv");
                    m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                    m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
                }
                else {
                    stream = g_tsStreamPool.Get("YUV10bit420_ms/Kimono1_352x288_24_p010_shifted.yuv");
                    m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 352;
                    m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 288;
                }
            }
            else if (fourcc_id == MFX_FOURCC_YUY2)
            {
                m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
                m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
                m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

                stream = g_tsStreamPool.Get("YUV8bit422/Kimono1_176x144_24_yuy2.yuv");

                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else if (fourcc_id == MFX_FOURCC_Y210)
            {
                m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
                m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
                m_par.mfx.FrameInfo.Shift = 1;
                m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

                stream = g_tsStreamPool.Get("YUV10bit422/Kimono1_176x144_24_y210.yuv");

                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else if (fourcc_id == MFX_FOURCC_AYUV)
            {
                m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
                m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
                m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

                stream = g_tsStreamPool.Get("YUV8bit444/Kimono1_176x144_24_ayuv.yuv");

                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else if (fourcc_id == MFX_FOURCC_Y410)
            {
                m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
                m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
                m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

                stream = g_tsStreamPool.Get("YUV10bit444/Kimono1_176x144_24_y410.yuv");

                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else if (fourcc_id == GMOCK_FOURCC_P012)
            {
                m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
                m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
                m_par.mfx.FrameInfo.Shift = 1;
                m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;

                stream = g_tsStreamPool.Get("YUV16bit420/FruitStall_176x144_240_p016.yuv");

                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else if (fourcc_id == GMOCK_FOURCC_Y212)
            {
                m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216;
                m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
                m_par.mfx.FrameInfo.Shift = 1;
                m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;

                stream = g_tsStreamPool.Get("YUV16bit422/FruitStall_176x144_240_y216.yuv");

                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else if (fourcc_id == GMOCK_FOURCC_Y412)
            {
                m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
                m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
                m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;

                stream = g_tsStreamPool.Get("YUV16bit444/FruitStall_176x144_240_y416.yuv");

                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else
            {
                g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
                return 0;
            }

            g_tsStreamPool.Reg();
        }

        MFXInit();
        Load();

        SETPARS(m_pPar, MFX_PAR);

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

        if (stream)
        {
            reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
            reader->m_disable_shift_hack = true; // don't shift
            m_filler = reader;
        }

        g_tsStatus.check(GetCaps(&caps, &capSize));

        if (tc.type != NO_INIT)
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

                if (tc.sub_type != NO_ENCODE)
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

                        if (tc.type != NO_SYNC)
                            SyncOperation(); TS_CHECK_MFX;
                        encoded++;
                    }
                }
                if (tc.type == CLOSED)
                    Close();
            }
        }

        if (tc.sub_type != DELTA) {  // not dependent from resolution params
            SETPARS(m_pPar, MFX_PAR_RESET);
        } else {
            for (mfxU32 i = 0; i < MAX_NPARS; i++) {
                if (tc.set_par[i].f && tc.set_par[i].ext_type == MFX_PAR_RESET) {
                    if (tc.type == RESOLUTION)
                    {
                        if (tc.set_par[i].f->name.find("Width") != std::string::npos)
                            m_pPar->mfx.FrameInfo.Width += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("Height") != std::string::npos)
                            m_pPar->mfx.FrameInfo.Height += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropW") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropW += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropH") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropH += tc.set_par[i].v;
                    }
                    if (tc.type == CROP)
                    {
                        if (tc.set_par[i].f->name.find("CropX") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropX += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropY") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropY += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropW") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropW += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropH") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropH += tc.set_par[i].v;
                    }
                }
            }
        }

        if (tc.type == RESOLUTION)
        {
            if (tc.sub_type == W_GT_MAX)
                m_pPar->mfx.FrameInfo.Width = caps.MaxPicWidth + 32;
            if (tc.sub_type == H_GT_MAX)
                m_pPar->mfx.FrameInfo.Height = caps.MaxPicHeight + 32;
        }

        if ((tc.type == FRAMERATE) && (tc.sub_type == CHANGE) && (g_tsConfig.sim))
        {   // for lowres in sim mode Level is not changed in case of fps increase
            sts = MFX_ERR_NONE;
        }

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

            if (tc.sub_type != NO_ENCODE)
            {
                if (tc.type == PROTECTED)
                {
                    ed.DataLength = m_bitstream.DataLength;
                    ed.DataOffset = m_bitstream.DataOffset;
                    ed.MaxLength = m_bitstream.MaxLength;
                    ed.Data = m_bitstream.Data;
                    m_bitstream.EncryptedData = &ed;
                }

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

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_reset, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_reset, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_420_p016_reset, RunTest_Subtype<GMOCK_FOURCC_P012>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_reset, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_reset, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_422_y216_reset, RunTest_Subtype<GMOCK_FOURCC_Y212>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_reset, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_reset, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_444_y416_reset, RunTest_Subtype<GMOCK_FOURCC_Y412>, n_cases);
}
