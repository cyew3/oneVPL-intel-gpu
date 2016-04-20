/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2016 Intel Corporation. All Rights Reserved.

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
        int RunTest(unsigned int id);

    private:
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {
        //invalid GopRefDist = 1, GopPicSize = 1
        {/*00*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {}, {
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

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];
        mfxHDL hdl;
        mfxHandleType type;
        const char* stream = g_tsStreamPool.Get("YUV/720x480p_30.00_4mb_h264_cabac_180s.yuv");
        g_tsStreamPool.Reg();
        tsSurfaceProcessor *reader;
        bool skip = false;

        MFXInit();
        Load();

        //set defoult param
        m_par.mfx.FrameInfo.Width = 736;
        m_par.mfx.FrameInfo.Height = 480;
        m_par.mfx.IdrInterval = 1;
        m_par.mfx.GopRefDist = 1;
        m_par.mfx.GopPicSize = m_par.mfx.GopRefDist + 1;


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
                    }
                }
            }
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_goppattern);
};