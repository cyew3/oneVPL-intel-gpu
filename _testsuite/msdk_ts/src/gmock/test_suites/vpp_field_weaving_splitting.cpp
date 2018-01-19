/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(C) 2018 Intel Corporation. All Rights Reserved.

File Name: vpp_field_weaving_splitting.cpp

\* ****************************************************************************** */

/*!
\file vpp_field_weaving_splitting.cpp
\brief Gmock test vpp_field_weaving_splitting

Description:
Field splitting is used for transcoding interlace frames into 2 HEVC field pictures (with half vertical dimension each, see HEVC spec annex D.2.19) 
sequence_Type_flag = 1 //Field picture
progressive_source_flag = 0 // input is interlace
MFX_EXTBUFF_VPP_FIELD_SPLITTING filter is enabled when input picture is a frame (!MFX_PICSTRUCT_FIELD_SINGLE) and output picture is a Field (MFX_PICSTRUCT_FIELD_SINGLE). 
When input is a single field (MFX_PICSTRUCT_FIELD_SINGLE) and output is a frame (interlaced or progressive), the SDK enables field weaving optionally with deinterlacing. 
Field weaving is stiching 2 fields into an interlace frame.


VPP processing for Field Splitting:
- Input frame  is interlace, provided output surface is set with MFX_PICSTRUCT_FIELD_SINGLE to request for field splitting 
  Input can have an unknown PicStruct at Init, but then correct picStruct can be set in frame's SEI during decode.
- Output is 2 half size progressive picture fields:
  * if input is picture structure is TFF, output PicStruct is MFX_PICSTRUCT_FIELD_TOP followed by MFX_PICSTRUCT_FIELD_BOTTOM
  * if input is picture structure is BFF, output PicStruct is MFX_PICSTRUCT_FIELD_BOTTOM followed by MFX_PICSTRUCT_FIELD_TOP
  * TBD: define behavior for progressive picture structure


Algorithm:
This suite tests that VPP FieldSplitting sets MFX_PICSTRUCT_FIELD_TOP/MFX_PICSTRUCT_FIELD_BOTTOM per out frame\n
- Set test suite params
- Set test case params
- Attach external buffer for field splitting
- Call Init, VPP functions
- Check that output picstruct should be MFX_PICSTRUCT_FIELD_TOP/MFX_PICSTRUCT_FIELD_BOTTOM per frame output
 VPP input:
   interlace stream, input PicStruct contains MFX_PICSTRUCT_FIELD_TFF, MFX_PICSTRUCT_FIELD_BFF or MFX_PICSTRUCT_UNKNOWN
 VPP output:
  output picstruct should be MFX_PICSTRUCT_FIELD_TOP/MFX_PICSTRUCT_FIELD_BOTTOM per frame output

  Expected error condition:
  - when:
    mfxVideoParam.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    mfxVideoParam.vpp.Out.PicStruct = MFX_PICSTRUCT_FIELD_SINGLE;
  - MFXVideoVPP_Query returns MFX_ERR_UNSUPPORTED
  - MFXVideoVPP_Init() returns MFX_ERR_INVALID_VIDEO_PARAM
  - MFXVideoVPP_Reset returns MFX_ERR_INVALID_VIDEO_PARAM

*/

#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"


//!Namespace of VPP FieldSplitting test
namespace vpp_field_weaving_splitting
{

//!\brief Main test class
class TestSuite: protected tsVideoVPP, public tsSurfaceProcessor
{
public:
    //! \brief A constructor (VPP)
    TestSuite(): tsVideoVPP(true)
    {
        m_surf_out_processor  = this;
        m_par.vpp.Out.PicStruct = MFX_PICSTRUCT_FIELD_SINGLE;
        m_par.vpp.Out.Height = 240; // default input frame is 720x480, field is half the height
        m_par.vpp.Out.CropH = 240;
        currentFrameNumber = 0;
    }
    //! \brief A destructor
    ~TestSuite() { }
    //! \brief Main method. Runs test case
    //! \param id - test case number
    virtual int RunTest(unsigned int id);
    //! The number of test cases
    static const unsigned int n_cases;
     //! \brief current output frame number starting from 0
    mfxU16 currentFrameNumber;
    mfxStatus SetCurrentFrameNumber(mfxU16 n)
    {
        currentFrameNumber = n;
        return MFX_ERR_NONE;
    }
protected:

    enum
    {
        MFX_PAR,
        MFX_PAR_1,
        RESET,
    };


    struct tc_struct
    {
        mfxStatus sts_query;
        mfxStatus sts_init;
        mfxStatus sts_reset;

        mfxU16 inputFrames; // number of input frames to process
        bool bUseReset;

        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU64 v;
        } set_par[MAX_NPARS];

    };

    //! \brief Set of test cases
    static const tc_struct test_case[];

    //! \brief Stream generator
    tsNoiseFiller m_noise;

    mfxStatus UpdateInputPicStruct(mfxU64 NewPicStruct)
    {
        m_pSurfIn->Info.PicStruct = NewPicStruct;
        return MFX_ERR_NONE;
    }

    /*! \fn mfxStatus ProcessSurface(mfxFrameSurface1& s)
        \brief Process output surface after sync
        \param s The output surface to validate
      Details: Check that output picture structure is correct
      Dependency:  currentFrameNumber shall be updated accordingly outside of this function
    */
    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        int FirstField = m_pSurfIn->Info.PicStruct; // first field shall have input frame parity
        int SecondField = (FirstField == MFX_PICSTRUCT_FIELD_TFF) ? MFX_PICSTRUCT_FIELD_BFF : MFX_PICSTRUCT_FIELD_TFF; // second field shall have opposite parity

        mfxFrameAllocator* pfa = (mfxFrameAllocator*)m_spool_in.GetAllocator();
        m_noise.tsSurfaceProcessor::ProcessSurface(&s, pfa);

         // check for expected output picture structure
        if (m_pSurfIn->Info.PicStruct & MFX_PICSTRUCT_FIELD_SINGLE)// weaving
        {
            EXPECT_EQ(s.Info.PicStruct, m_pSurfOut->Info.PicStruct);
        }
        else // splitting
        {
            if (currentFrameNumber & 1) // Second Field
            {
                EXPECT_EQ(s.Info.PicStruct, (SecondField | MFX_PICSTRUCT_FIELD_SINGLE));
            }
            else // first Field
            {
                EXPECT_EQ(s.Info.PicStruct, (FirstField | MFX_PICSTRUCT_FIELD_SINGLE));
            }
        }

        return MFX_ERR_NONE;
    }

    int RunTest(const tc_struct& tc);
};



const TestSuite::tc_struct TestSuite::test_case[] =
{
    // splitting
    {/*00 - TFF*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, 1, false,
        {   {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE},
        },
    },
    {/*01 - BFF*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, 2, false,
        {   {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE},
        },
    },
    {/*02 - Alternate between TFF and BFF */ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, 7, false,
        {   {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE},
            {MFX_PAR_1, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
        },
    },
    {/*03 - Alternate between BFF and TFF */ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, 7, false,
        {   {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE},
            {MFX_PAR_1, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        },
    },
    {/*04 - Unknown input picture structure to be setup later */ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM, 1, false,
        {   {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE},
        },
    },
    {/*05 - Progressive input is expected to fail */  MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, 3,false,
        {   {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE},
        },
    },
    {/*06 - Progressive input is expected to fail for reset */  MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM, 3,true,
        {   {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Height,     480},
            {RESET, &tsStruct::mfxVideoParam.vpp.In.Width,      720},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Height,    480},
            {RESET, &tsStruct::mfxVideoParam.vpp.Out.Width,     720},
        },
    },
    // weaving
    {/*07 - TFF*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, 1, false,
        {   { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
        },
    },
    {/*08 - BFF*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, 2, false,
        {   { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_BFF },
        },
    },
    {/*09 - UNKNOWN*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, 2, false,
        {   { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_UNKNOWN },
        },
    },
    {/*10 - Alternate between TFF and BFF */ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, 7, false,
        {   { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { MFX_PAR_1, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_BFF },
        },
    },
    {/*11 - Alternate between BFF and TFF */ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, 7, false,
        {   { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_BFF },
            { MFX_PAR_1, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
        },
    },
    // to Check
    {/*12 - Progressive output is expected to fail */  MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM, 3,false,
        {   { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
        },
    },
    {/*13 - Progressive output is expected to fail for reset */  MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM, 3,true,
        {   { MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
            { MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF },
            { RESET, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_SINGLE },
            { RESET, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
            { RESET, &tsStruct::mfxVideoParam.vpp.In.Height,     480 },
            { RESET, &tsStruct::mfxVideoParam.vpp.In.Width,      720 },
            { RESET, &tsStruct::mfxVideoParam.vpp.Out.Height,    480 },
            { RESET, &tsStruct::mfxVideoParam.vpp.Out.Width,     720 },
        },
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

int TestSuite::RunTest(unsigned int id)
{
    return  RunTest(test_case[id]);
}

int TestSuite::RunTest(const TestSuite::tc_struct& tc)
{
    TS_START;
    mfxU16 processed = 0;
    tsExtBufType<mfxVideoParam> mfx_par_1;
    mfxStatus sts, sts_query = tc.sts_query,
              sts_init  = tc.sts_init;

    SETPARS(&m_par, MFX_PAR);
    SETPARS(&mfx_par_1, MFX_PAR_1);

    if (MFX_FOURCC_YV12 == m_par.vpp.In.FourCC && MFX_ERR_NONE == tc.sts_init
     && MFX_OS_FAMILY_WINDOWS == g_tsOSFamily && g_tsImpl & MFX_IMPL_VIA_D3D11)
    {
        sts_query = (sts_query == MFX_ERR_NONE) ? MFX_WRN_PARTIAL_ACCELERATION : sts_query;
        sts_init = (sts_init == MFX_ERR_NONE) ? MFX_WRN_PARTIAL_ACCELERATION : sts_init;
    }

    MFXInit();
    CreateAllocators();
    SetFrameAllocator();
    SetHandle();

    tsExtBufType<mfxVideoParam> par_out;
    par_out=m_par;

    g_tsStatus.expect(sts_query);
    Query(m_session, m_pPar, &par_out);

    g_tsStatus.expect(sts_init);
    Init(m_session, m_pPar);


    if (sts_init >= MFX_ERR_NONE && (m_par.vpp.In.PicStruct != MFX_PICSTRUCT_UNKNOWN) && (m_par.vpp.Out.PicStruct != MFX_PICSTRUCT_UNKNOWN))
    {
        AllocSurfaces();
        // One input frame produces 2 output field pictures
        while (processed < 2 * tc.inputFrames)
        {
            // change input picstruct after 5 frames
            if (processed > 5)
            {
                UpdateInputPicStruct(mfx_par_1.vpp.In.PicStruct);
            }
            SetCurrentFrameNumber(processed);
            sts = RunFrameVPPAsync();

            while(m_surf_out.size()) SyncOperation();

            // process first output field
            if (MFX_ERR_MORE_SURFACE == sts)
            {
                ProcessSurface(*m_pSurfOut);
            }

            processed++;
        }
    }

    if (tc.bUseReset)
    {
        tsExtBufType<mfxVideoParam> par_reset;
        SETPARS(&par_reset, RESET);
        g_tsStatus.expect(tc.sts_reset);
        Reset(m_session, &par_reset);
    }

    g_tsLog <<  processed << "FRAMES PROCESSED \n";

    TS_END;
    return 0;
}

//!\brief Reg the test suite into test system
TS_REG_TEST_SUITE_CLASS(vpp_field_weaving_splitting);
}
