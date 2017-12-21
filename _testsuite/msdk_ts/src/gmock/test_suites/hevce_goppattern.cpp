/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2017 Intel Corporation. All Rights Reserved.

File Name: hevce_goppattern.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#define MAX_IDR 30

namespace hevce_goppattern
{
    enum
    {
        MFX_PAR,
        MFX_BS,
        NONE
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU16 num_idr;
        mfxU16 idr[MAX_IDR];
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

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);
        int RunTest(tc_struct tc, unsigned int fourcc_id);

    private:
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {
        //invalid GopRefDist = 1, GopPicSize = 1
        {/*00*/ MFX_ERR_NONE, 0, {}, {
                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 1 }
                                     }
        },
        //defoutl GopRefDist = 1, GopPicSize = 2
        {/*01*/ MFX_ERR_NONE, 0, {}, {} },
        {/*02*/ MFX_ERR_NONE, 1, {0}, {} },
        {/*03*/ MFX_ERR_NONE, 5, {1, 3, 5, 7, 10}, {} },
        {/*04*/ MFX_ERR_NONE, MAX_IDR, {0, 1, 2, 3, 4, 5 ,6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29}, {} },
        //GopRefDist = 2, GopPicSize = 4
        {/*05*/ MFX_ERR_NONE, 0, {}, {
                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 4 }
                                     }
        },
        {/*06*/ MFX_ERR_NONE, 1, {0}, {
                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 4 }
                                      }
        },
        {/*07*/ MFX_ERR_NONE, 5, {1, 3, 5, 7, 10}, {
                                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
                                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 4 }
                                                   }
        },
        {/*08*/ MFX_ERR_NONE, MAX_IDR, {0, 1, 2, 3, 4, 5 ,6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29}, {
                                                                                                                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
                                                                                                                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 4 }
                                                                                                                                                       }
        },
        //GopRefDist = 2, GopPicSize = 5
        {/*09*/ MFX_ERR_NONE, 0, {}, {
                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 5 }
                                     }
        },
        {/*10*/ MFX_ERR_NONE, 1, {0}, {
                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 5 }
                                      }
        },
        {/*11*/ MFX_ERR_NONE, 5, {1, 3, 5, 7, 10}, {
                                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
                                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 5 }
                                                   }
        },
        {/*12*/ MFX_ERR_NONE, MAX_IDR, {0, 1, 2, 3, 4, 5 ,6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29}, {
                                                                                                                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 },
                                                                                                                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 5 }
                                                                                                                                                       }
        },
        //Idrinterval = 2, GopRefDist = 7, GopPicSize = 16
        {/*13*/ MFX_ERR_NONE, 0, {}, {
                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.IdrInterval, 2 },
                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7 },
                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 16 }
                                     }
        },
        {/*14*/ MFX_ERR_NONE, 1, { 0 }, {
                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.IdrInterval, 2 },
                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7 },
                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 16 }
                                        }
        },
        {/*15*/ MFX_ERR_NONE, 5, { 1, 3, 5, 7, 10 }, {
                                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.IdrInterval, 2 },
                                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7 },
                                                        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 16 }
                                                     }
        },
        {/*16*/ MFX_ERR_NONE, MAX_IDR, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29 }, {
                                                                                                                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.IdrInterval, 2 },
                                                                                                                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7 },
                                                                                                                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 16 }
                                                                                                                                                         }
        },


    };

    const unsigned int TestSuite::n_cases = sizeof(test_case) / sizeof(tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;

        mfxHDL hdl;
        mfxHandleType type;
        const char* stream = NULL;
        tsSurfaceProcessor *reader;
        bool skip = false;

        MFXInit();
        Load();

        //set default param
        m_par.mfx.IdrInterval = 1;
        m_par.mfx.GopRefDist = 1;
        m_par.mfx.GopPicSize = m_par.mfx.GopRefDist + 1;

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            stream = g_tsStreamPool.Get("YUV/720x480p_30.00_4mb_h264_cabac_180s.yuv");

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 720;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;
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

            stream = g_tsStreamPool.Get("YUV8bit422/Kimono1_352x288_24_yuy2.yuv");

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 352;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 288;
        }
        else if (fourcc_id == MFX_FOURCC_Y210)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

            stream = g_tsStreamPool.Get("YUV10bit422/Kimono1_352x288_24_y210.yuv");

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 352;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 288;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            stream = g_tsStreamPool.Get("YUV8bit444/Kimono1_352x288_24_ayuv.yuv");

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 352;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 288;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

            stream = g_tsStreamPool.Get("YUV10bit444/Kimono1_352x288_24_y410.yuv");

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 352;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 288;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        g_tsStreamPool.Reg();

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

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else if (fourcc_id == MFX_FOURCC_YUY2)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_Y210)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else
        {
            g_tsLog << "WARNING: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
        {
            if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
            else if (m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_P010 && g_tsHWtype < MFX_HW_KBL) {
                g_tsLog << "\n\nWARNING: P010 format only supported on KBL+!\n\n\n";
                throw tsSKIP;
            }
            else if ((m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210 || m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_YUY2)
                && (g_tsHWtype < MFX_HW_ICL || g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
            {
                g_tsLog << "\n\nWARNING: 422 formats only supported on ICL+ and ENC+PAK!\n\n\n";
                throw tsSKIP;
            }
            else if ((m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_AYUV || m_pPar->mfx.FrameInfo.FourCC == MFX_FOURCC_Y410)
                && (g_tsHWtype < MFX_HW_ICL || g_tsConfig.lowpower != MFX_CODINGOPTION_ON))
            {
                g_tsLog << "\n\nWARNING: 444 formats only supported on ICL+ and VDENC!\n\n\n";
                throw tsSKIP;
            }
        }
        else
        {
            if (m_par.mfx.GopPicSize < (m_par.mfx.GopRefDist + 1))
            {
                skip = true;
                g_tsLog << "WARNING: Case is skipped!\n";
            }
        }

        //set reader
        reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
        m_filler = reader;

        if (!skip)
        {
            g_tsStatus.expect(tc.sts);

            Init();

            //call test function
            if (tc.sts >= MFX_ERR_NONE)
            {

                mfxU16 index_idr = 0;
                mfxU32 encoded = 0;
                mfxU32 submitted = 0;
                mfxU32 async = TS_MAX(1, m_par.AsyncDepth);
                mfxSyncPoint sp;

                bool bSingleFldProc = false;
                async = TS_MIN(MAX_IDR, async - 1);

                while(encoded < MAX_IDR)
                {
                    if ((index_idr < tc.num_idr) && (tc.idr[index_idr] == encoded))
                    {
                        m_pCtrl->FrameType = MFX_FRAMETYPE_IDR;
                        index_idr++;
                    }
                    else if (m_pCtrl)
                    {
                        m_pCtrl->FrameType = 0;
                    }

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

                    g_tsStatus.check();TS_CHECK_MFX;
                    sp = m_syncpoint;

                    if(++submitted >= async)
                    {
                        SyncOperation();TS_CHECK_MFX;

                        encoded += submitted;
                        submitted = 0;
                        async = TS_MIN(async, (MAX_IDR - encoded));
                        m_bitstream.DataLength = m_bitstream.DataOffset = 0;
                    }
                }
            }
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_goppattern, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_hevce_goppattern, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_hevce_goppattern, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_hevce_goppattern, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_hevce_goppattern, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_hevce_goppattern, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
};