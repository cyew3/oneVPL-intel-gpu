/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2018 Intel Corporation. All Rights Reserved.

File Name: vpp_pts.cpp
\* ****************************************************************************** */


#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_pts_suite
{
    enum
    {
        MFX_PAR,
    };

    class    TestSuite: tsVideoVPP
    {
    public:
        TestSuite(): tsVideoVPP(true)
        {
            m_par.vpp.In.Height = 576;
            m_par.vpp.In.CropH = 576;

            m_par.vpp.Out.Height = 576;
            m_par.vpp.Out.CropH = 576;
            m_par.vpp.Out.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
            m_par.vpp.In.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
        }
        ~TestSuite() {}
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:
        struct tc_struct
        {
            const char* stream_name;
            bool memid;

            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            }set_par[MAX_NPARS];

            //Array with DoNotUse algs (if required)
            mfxU32 dnu[3];

        };

        static const tc_struct test_case[];
    };


    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ "YUV/foster_720x576.yuv", false,
            {
                { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 3 }
            },
            { MFX_EXTBUFF_VPP_SCENE_ANALYSIS,MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL }
        },

        {/*01*/ "YUV/foster_720x576.yuv", true,
            {
                { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 3 }
            },
            { MFX_EXTBUFF_VPP_SCENE_ANALYSIS,MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DETAIL }
        },

        {/*02*/ "YUV/foster_720x576.yuv", false,
            {
                { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 3 }
            },
            { MFX_EXTBUFF_VPP_SCENE_ANALYSIS,MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_DETAIL }
        },


        {/*03*/ "YUV/foster_720x576.yuv", true,
            {
                { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 3 }
            },
            { MFX_EXTBUFF_VPP_SCENE_ANALYSIS,MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_DETAIL }
        },


        {/*04*/ "YUV/foster_720x576.yuv", false,
            {
                { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 3 }
            },
            { MFX_EXTBUFF_VPP_SCENE_ANALYSIS,MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DENOISE }
        },

        {/*05*/ "YUV/foster_720x576.yuv", true,
            {
                { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 3 }
            },
            { MFX_EXTBUFF_VPP_SCENE_ANALYSIS,MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DENOISE }
        },

        {/*06*/ "YUV/foster_720x576.yuv", false,
            {
                { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 3 }
            },
            { MFX_EXTBUFF_VPP_DETAIL,MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DENOISE }
        },

        {/*07*/ "YUV/foster_720x576.yuv", true,
            {
                { MFX_PAR, &tsStruct::mfxExtVPPDoNotUse.NumAlg, 3 }
            },
            { MFX_EXTBUFF_VPP_DETAIL,MFX_EXTBUFF_VPP_PROCAMP, MFX_EXTBUFF_VPP_DENOISE }
        },


        {/*08*/ "YUV/foster_720x576.yuv", false,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 80 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 64 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 80 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 64 }
            },
            {}
        },

        {/*09*/ "YUV/foster_720x576.yuv", true,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 80 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 64 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 80 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 64 }
            },
            {}
        },

        {/*10*/ "YUV/foster_720x576.yuv", false,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
            },
            {}
        },

        {/*11*/ "YUV/foster_720x576.yuv", true,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
            },
            {}
        },

        {/*12*/ "YUV/foster_720x576.yuv", false,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YV12 },
            },
            {}
        },

        {/*13*/ "YUV/foster_720x576.yuv", true,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YV12 },
            },
            {}
        },

        {/*14*/ "YUV/foster_720x576.yuv", false,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60 },
                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_PRESERVE_TIMESTAMP }
            },
            {}
        },

        {/*15*/ "YUV/foster_720x576.yuv", true,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60 },
                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_PRESERVE_TIMESTAMP }
            },
            {}
        },

        {/*16*/ "YUV/foster_720x576.yuv", false,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60 },
                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_DISTRIBUTED_TIMESTAMP }
            },
            {}
        },

        {/*17*/ "YUV/foster_720x576.yuv", true,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60 },
                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_DISTRIBUTED_TIMESTAMP }
            },
            {}
        },

        {/*18*/ "YUV/foster_720x576.yuv", false,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YV12 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 80 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 64 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 80 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 64 }
            },
            {}
        },

        {/*19*/ "YUV/foster_720x576.yuv", true,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YV12 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 80 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 64 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 80 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 64 }
            },
            {}
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        MFXInit();
        auto& tc = test_case[id];
        SETPARS(&m_par, MFX_PAR);

        mfxExtVPPDoNotUse* buf = (mfxExtVPPDoNotUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE);
        if(buf != nullptr) {
            buf->AlgList = new mfxU32[buf->NumAlg];
            memcpy(buf->AlgList, tc.dnu, sizeof(mfxU32)*buf->NumAlg);
        }

        m_use_memid = tc.memid;

        frame_allocator::AllocatorType al_type = frame_allocator::AllocatorType::SOFTWARE;
        if(g_tsImpl & MFX_IMPL_HARDWARE) {
            al_type = frame_allocator::AllocatorType::HARDWARE;
            m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        }
        if(g_tsImpl & MFX_IMPL_VIA_D3D11) {
            al_type = frame_allocator::AllocatorType::HARDWARE_DX11;
        }

        frame_allocator al = frame_allocator(al_type,
                                             frame_allocator::AllocMode::ALLOC_AVG,
                                             frame_allocator::LockMode::ENABLE_ALL,
                                             frame_allocator::OpaqueAllocMode::ALLOC_EMPTY);

        mfxFrameAllocator* p_Allocator;
#if defined (WIN32) || (WIN64)
        mfxHDL hdl;
        mfxHandleType hdl_type;
        m_pFrameAllocator = &al;

        if(al_type == frame_allocator::AllocatorType::HARDWARE || al_type == frame_allocator::AllocatorType::HARDWARE_DX11) {
            al.get_hdl(hdl_type, hdl);
            SetHandle(m_session, hdl_type, hdl);
        }
        m_spool_in.SetAllocator(m_pFrameAllocator, true);
        m_spool_out.SetAllocator(m_pFrameAllocator, true);
        p_Allocator = m_pFrameAllocator;
        SetFrameAllocator(m_session, p_Allocator);
#else
        //!\note In Linux handle and FrameAllocator sets automatically
        m_spool_in.SetAllocator(m_pVAHandle, true);
        m_spool_out.SetAllocator(m_pVAHandle, true);
        p_Allocator = m_pVAHandle;
#endif

        Init();
        QueryIOSurf();

        m_pSurfPoolIn->AllocSurfaces(m_request[0], !m_use_memid);
        m_pSurfPoolOut->AllocSurfaces(m_request[1], !m_use_memid);

        tsRawReader reader(g_tsStreamPool.Get(tc.stream_name), m_par.mfx.FrameInfo);
        g_tsStreamPool.Reg();
        m_surf_in_processor = &reader;

        mfxU32 frames = 0;
        mfxU32 frames_in = 0;
        mfxU64 pts = 0;
        bool more_in = true;
        bool more_out = true;
        while(frames < 20) {

            if(more_in) {
                m_pSurfIn = m_pSurfPoolIn->GetSurface();

                if(m_surf_in_processor) {
                    m_pSurfIn = m_surf_in_processor->ProcessSurface(m_pSurfIn, p_Allocator);
                }

                m_pSurfIn->Data.TimeStamp = (mfxU64)((frames_in) * 90000 / m_par.vpp.In.FrameRateExtN);
                frames_in++;
                more_in = false;
            }

            if(more_out) {
                m_pSurfOut = m_pSurfPoolOut->GetSurface();
                more_out = false;
            }

            mfxStatus sts = RunFrameVPPAsync(m_session, m_pSurfIn, m_pSurfOut, 0, m_pSyncPoint);

            if(g_tsStatus.get() == 0) {
                m_surf_out.insert(std::make_pair(*m_pSyncPoint, m_pSurfOut));
                if(m_pSurfOut) {
                    msdk_atomic_inc16(&m_pSurfOut->Data.Locked);
                }
            }

            switch(sts) {
                case MFX_ERR_MORE_SURFACE:
                    pts = m_pSurfOut->Data.TimeStamp;
                    frames++;
                    more_out = true;
                    break;
                case MFX_ERR_MORE_DATA:
                    more_in = true;
                    break;
                case MFX_ERR_NONE:
                {
                    pts = m_pSurfOut->Data.TimeStamp;
                    SyncOperation();
                    mfxI64 pts_delta = pts - (mfxI64)((frames * 90000 / m_par.vpp.Out.FrameRateExtN));

                    pts_delta = pts_delta >= 0 ? pts_delta : -pts_delta;

                    mfxExtVPPFrameRateConversion* frc_buf = (mfxExtVPPFrameRateConversion*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION);
                    if(frc_buf != nullptr) {
                        if(frc_buf->Algorithm == MFX_FRCALGM_PRESERVE_TIMESTAMP) {
                            EXPECT_EQ(pts, MFX_TIMESTAMP_UNKNOWN) << "FAILED: expected TimeStamp = UNKNOWN for inserted frames when FRC Algorithm = PRESERVE_TIMESTAMP\n";
                        }
                        pts_delta = 0;   // PTS of repeated frame should stay invalid
                    }

                    EXPECT_LE(pts_delta, 1);
                    frames++;
                    more_out = true;
                    more_in = true;
                    break;
                }
                default:
                    throw tsFAIL;
                    break;
            }

        }
        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(vpp_pts_suite);

}
