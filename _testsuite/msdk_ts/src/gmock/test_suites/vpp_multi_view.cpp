/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

File Name: vpp_multi_view.cpp
\* ****************************************************************************** */

/*!
\file vpp_multi_view.cpp
\brief Gmock test vpp_multi_view

Description:
This suite tests AVC VPP\n

Algorithm:
- Initialize MSDK
- Set test suite params
- Set test case params
- Attach buffers
- Call QueryIOSurf and Init functions
- AllocSurfaces
- Run vpp frame and check ViewId of it

*/
#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include "ts_fei_warning.h"

/*! \brief Main test name space */
namespace vpp_multi_view
{

    enum
    {
        MFX_PAR = 1,
    };

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        /*! \brief Number of frames to PP*/
        mfxU32 n_frames;
        /*! \brief Structure contains params for some fields of VPP */
        struct f_pair
        {
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;

        }set_par[MAX_NPARS];
    };

    //!\brief Main test class
    class TestSuite:tsVideoVPP
    {
    public:
        //! \brief A constructor
        TestSuite(): tsVideoVPP(true) {

            m_par.vpp.In.Width = 352;
            m_par.vpp.In.Height = 288;
            m_par.vpp.In.CropW = 352;
            m_par.vpp.In.CropH = 288;

            m_par.AddExtBuffer(MFX_EXTBUFF_MVC_SEQ_DESC, sizeof(mfxExtMVCSeqDesc));
            mfxExtMVCSeqDesc* ext_buf = (mfxExtMVCSeqDesc*)m_par.GetExtBuffer(MFX_EXTBUFF_MVC_SEQ_DESC);
            ext_buf->NumView = 2;
        }
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ 50},
        {/*01*/ 50,
            {
                {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 176},
                {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 144},
                {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 176},
                {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 144}
            }
        },

        {/*02*/ 50,
            {
                {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60 }
            }
        },

        {/*03*/ 50,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60 }
            }
        },

        {/*04*/ 50,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60 }
            }
        },

        {/*05*/ 50,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60 },
                { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30 }
            }
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        SKIP_IF_LINUX;
        MFXInit();
        auto& tc = test_case[id];

        SETPARS(&m_par, MFX_PAR);

        QueryIOSurf();
        Init();

        tsRawReader reader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"), m_par.mfx.FrameInfo);
        g_tsStreamPool.Reg();
        m_surf_in_processor = &reader;

        AllocSurfaces();

        for(mfxU32 i = 0; i < tc.n_frames; i++) {
            m_pSurfOut = m_pSurfPoolOut->GetSurface();
            m_pSurfIn = m_pSurfPoolIn->GetSurface();
            m_surf_in_processor->ProcessSurface(m_pSurfIn, m_pFrameAllocator);

            mfxU16 viewId = i % 2;
            m_pSurfIn->Info.FrameId.ViewId = viewId;
            mfxStatus sts = RunFrameVPPAsync(m_session, m_pSurfIn, m_pSurfOut, 0, m_pSyncPoint);

            if(g_tsStatus.get() == 0) {
                m_surf_out.insert(std::make_pair(*m_pSyncPoint, m_pSurfOut));
                if(m_pSurfOut) {
                    m_pSurfOut->Data.Locked++;
                }
            }

            if(sts == MFX_ERR_NONE) {
                EXPECT_EQ(m_pSurfIn->Info.FrameId.ViewId, m_pSurfOut->Info.FrameId.ViewId) << "ERROR: ViewIds aren't the same. Frame - " << i << "\n";
                SyncOperation();
            } else if(sts == MFX_ERR_MORE_DATA || MFX_ERR_MORE_SURFACE){
                continue;
            } else {
                g_tsLog << "ERROR: Unexpected return sts of RunFrameVPPAsync";
                return 1;
            }

        }

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_multi_view);
}