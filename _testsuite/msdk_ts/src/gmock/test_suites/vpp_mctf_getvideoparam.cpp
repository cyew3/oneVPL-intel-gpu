/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2018 Intel Corporation. All Rights Reserved.
//
//
*/
#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_mctf_getvideoparam
{

    class TestSuite : tsVideoVPP
    {
    public:
        TestSuite()
            : tsVideoVPP() { }
        ~TestSuite() {}
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:

        mfxExtVPPDoUse* vpp_du;
        mfxExtVPPDoUse* vpp_du2;

        enum
        {
            MFX_PAR = 1,
            NULL_PAR,
            STREAM,
            NULL_SESSION,
            DOUSE,
            TELL_ACTIVE_FILTERS,
        };

        struct tc_struct
        {
            mfxStatus init_sts;
            mfxStatus sts; // this is a status for GetVideoParam
            mfxU32 mode;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxF32 v;
            } set_par[MAX_NPARS];
            mfxU32 alg_num;
            // if we want to request a list of filters, it might  be long
            mfxU32 alg_list[20];
            // platforms supported by MCTF
            mfxU32 platforms_n;
            // suppose 19 platforms can be mentioned
            HWType platform[19];
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_PAR,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 10 },
#if (MFX_VERSION >= MFX_VERSION_NEXT)
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Overlap, MFX_CODINGOPTION_ON },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking, MFX_CODINGOPTION_ON },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_2REF },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.BitsPerPixelx100k, 10000 },
#endif
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision, MFX_MVPRECISION_QUARTERPEL },
                { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50 },
                { MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 50 },
//                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_PRESERVE_TIMESTAMP },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 50.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue, 100.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB },
            },
            0 ,{}
        },

        {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_PAR,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 22 },
#if (MFX_VERSION >= MFX_VERSION_NEXT)
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Overlap, MFX_CODINGOPTION_ON },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking, MFX_CODINGOPTION_ON },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_2REF },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.BitsPerPixelx100k, 10000 },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision, MFX_MVPRECISION_QUARTERPEL },
#endif
                { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50 },
                { MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 50 },
//                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_PRESERVE_TIMESTAMP },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 50.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue, 100.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB },
            },
            0 ,{}
        },

        {/*02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_PAR,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 21 },
#if (MFX_VERSION >= MFX_VERSION_NEXT)
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Overlap, MFX_CODINGOPTION_ADAPTIVE },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking, MFX_CODINGOPTION_ADAPTIVE },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_4REF },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.BitsPerPixelx100k, (12 * 100000 + 1) },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision, MFX_MVPRECISION_HALFPEL },
#endif
                { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50 },
                { MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 50 },
//                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_PRESERVE_TIMESTAMP },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 50.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue, 100.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB },
            },
            0 ,{}
        },

        {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_PAR,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 20 },
#if (MFX_VERSION >= MFX_VERSION_NEXT)
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Overlap, MFX_CODINGOPTION_ON },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking, MFX_CODINGOPTION_ON },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_4REF },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.BitsPerPixelx100k, (12 * 100000) },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision, MFX_MVPRECISION_QUARTERPEL },
#endif
                { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50 },
                { MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 50 },
//                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_PRESERVE_TIMESTAMP },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 50.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue, 100.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB },
            },
            0 ,{}
        },

        {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_PAR,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 20 },
#if (MFX_VERSION >= MFX_VERSION_NEXT)
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Overlap, MFX_CODINGOPTION_UNKNOWN },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking, MFX_CODINGOPTION_ON },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_4REF },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.BitsPerPixelx100k, (12 * 100000) },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision, MFX_MVPRECISION_QUARTERPEL },
#endif
                { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50 },
                { MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 50 },
//                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_PRESERVE_TIMESTAMP },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 50.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue, 100.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB },
            },
            0 ,{}
        },

        {/*05*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_PAR,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 20 },
#if (MFX_VERSION >= MFX_VERSION_NEXT)
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Overlap, MFX_CODINGOPTION_ON },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking, MFX_CODINGOPTION_UNKNOWN },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_4REF },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.BitsPerPixelx100k, (12 * 100000) },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision, MFX_MVPRECISION_QUARTERPEL },
#endif
                { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50 },
                { MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 50 },
//                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_PRESERVE_TIMESTAMP },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 50.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue, 100.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB },
            },
            0 ,{}
        },

        {/*06*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_PAR,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 20 },
#if (MFX_VERSION >= MFX_VERSION_NEXT)
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Overlap, MFX_CODINGOPTION_ON },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking, MFX_CODINGOPTION_ON },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_UNKNOWN },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.BitsPerPixelx100k, (12 * 100000) },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision, MFX_MVPRECISION_QUARTERPEL },
#endif
                { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50 },
                { MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 50 },
//                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_PRESERVE_TIMESTAMP },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 50.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue, 100.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB },
            },
            0 ,{}
        },

        {/*07*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_PAR,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 20 },
#if (MFX_VERSION >= MFX_VERSION_NEXT)
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Overlap, MFX_CODINGOPTION_ON },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.Deblocking, MFX_CODINGOPTION_ON },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.TemporalMode, MFX_MCTF_TEMPORAL_MODE_4REF },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.BitsPerPixelx100k, (12 * 100000) },
                { MFX_PAR, &tsStruct::mfxExtVppMctf.MVPrecision, MFX_MVPRECISION_UNKNOWN },
#endif
                { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50 },
                { MFX_PAR, &tsStruct::mfxExtVPPDetail.DetailFactor, 50 },
//                { MFX_PAR, &tsStruct::mfxExtVPPFrameRateConversion.Algorithm, MFX_FRCALGM_PRESERVE_TIMESTAMP },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Brightness, 50.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Contrast, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Hue, 100.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPProcAmp.Saturation, 5.0F },
                { MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB },
            },
            0 ,{}
        },

        {/*08*/ MFX_ERR_NONE, MFX_ERR_NONE, DOUSE,
            {},
            3 , {   MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_MCTF, MFX_EXTBUFF_VPP_DETAIL } 
        },

        // try to find MCTF filter in the list
        {/*09*/ MFX_ERR_NONE, MFX_ERR_NONE, TELL_ACTIVE_FILTERS,
            {},

            3,{ MFX_EXTBUFF_VPP_DENOISE, MFX_EXTBUFF_VPP_MCTF, MFX_EXTBUFF_VPP_DETAIL },
            // platforms that support MCTF
            4, { MFX_HW_BDW, MFX_HW_SKL, MFX_HW_KBL, MFX_HW_CNL }
        },

        {/*10*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_PAR,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 20 },
                { MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 50 },
            },
            0 ,{}
        },

        {/*11*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_PAR,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 21 },
            },
            0 ,{}
        },
        {/*12*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_PAR,
            {
                { MFX_PAR, &tsStruct::mfxExtVppMctf.FilterStrength, 0 },
            },
            0 ,{}
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

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

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];

        MFXInit();

        m_par.AsyncDepth = 1;

        SETPARS(&m_par, MFX_PAR);

        if (tc.mode == DOUSE)
        {
            m_par.AddExtBuffer(MFX_EXTBUFF_VPP_DOUSE, sizeof(mfxExtVPPDoUse));
            vpp_du = (mfxExtVPPDoUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
            vpp_du->NumAlg = tc.alg_num;
            vpp_du->AlgList = new mfxU32[tc.alg_num];

            memset(vpp_du->AlgList, 0, sizeof(mfxU32)*vpp_du->NumAlg);

            for (mfxU32 i = 0; i< tc.alg_num; i++)
                vpp_du->AlgList[i] = tc.alg_list[i];
        }
        else
        if (tc.mode == TELL_ACTIVE_FILTERS)
        {
            m_par.AddExtBuffer(MFX_EXTBUFF_VPP_DOUSE, sizeof(mfxExtVPPDoUse));
            vpp_du = (mfxExtVPPDoUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
            vpp_du->NumAlg = tc.alg_num;
            vpp_du->AlgList = new mfxU32[tc.alg_num];

            memset(vpp_du->AlgList, 0, sizeof(mfxU32)*vpp_du->NumAlg);

            for (mfxU32 i = 0; i< tc.alg_num; i++)
                vpp_du->AlgList[i] = tc.alg_list[i];
        }


        CreateAllocators();
        SetFrameAllocator();
        SetHandle();
        AllocSurfaces();

        g_tsStatus.expect(tc.init_sts);
        Init(m_session, m_pPar);


        tsExtBufType<mfxVideoParam> par_start; // initial(start) parameters
        SETPARS(&par_start, MFX_PAR);

        tsExtBufType<mfxVideoParam> par_init;
        mfxVideoParam *out_pPar = &par_init;
        mfxSession m_session_tmp = m_session;

        if (tc.mode == NULL_PAR)
            out_pPar = 0;
        else if (tc.mode == STREAM)
        {
            std::string stream_name = "forBehaviorTest/valid_yuv.yuv";
            tsRawReader stream(g_tsStreamPool.Get(stream_name), m_pPar->mfx.FrameInfo);
            g_tsStreamPool.Reg();
            m_surf_in_processor = &stream;
            ProcessFrames(2);
        }
        else if (tc.mode == NULL_SESSION)
            m_session = 0;
        else if (tc.mode == MFX_PAR)
        {
            for (mfxU32 i = 0; i< m_par.NumExtParam; i++)
                par_init.AddExtBuffer(m_par.ExtParam[i]->BufferId, m_par.ExtParam[i]->BufferSz);
        }
        else if (tc.mode == DOUSE)
        {
            par_init.AddExtBuffer(MFX_EXTBUFF_VPP_DOUSE, sizeof(mfxExtVPPDoUse));
            vpp_du2 = (mfxExtVPPDoUse*)par_init.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
            vpp_du2->NumAlg = tc.alg_num;
            vpp_du2->AlgList = new mfxU32[tc.alg_num];

            memset(vpp_du2->AlgList, 0, sizeof(mfxU32)*vpp_du2->NumAlg);

            for (mfxU32 i = 0; i< tc.alg_num; i++)
                par_init.AddExtBuffer(tc.alg_list[i], GetBufSzById(tc.alg_list[i]));
        }
        else if (tc.mode == TELL_ACTIVE_FILTERS)
        {
            par_init.AddExtBuffer(MFX_EXTBUFF_VPP_DOUSE, sizeof(mfxExtVPPDoUse));
            vpp_du2 = (mfxExtVPPDoUse*)par_init.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
            vpp_du2->NumAlg = tc.alg_num;
            vpp_du2->AlgList = new mfxU32[tc.alg_num];
            memset(vpp_du2->AlgList, 0, sizeof(mfxU32)*vpp_du2->NumAlg);
        }

        g_tsStatus.expect(tc.sts);
        GetVideoParam(m_session, out_pPar);
        g_tsStatus.expect(MFX_ERR_NONE);

        if (tc.mode == TELL_ACTIVE_FILTERS)
        {

            vpp_du2 = (mfxExtVPPDoUse*)par_init.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
            if(!vpp_du2)
                TS_FAIL_TEST("No DoUse list was reported after GetVideoParam", MFX_ERR_NONE);
            bool bPlatformSupportsMCTF(true);
            if (tc.platforms_n && MFX_HW_UNKNOWN != g_tsHWtype)
            {
                printf("g_tsHWtype: %d\n", g_tsHWtype);
                bool ValidHWPlatform(false);
                for (mfxU32 i = 0; i < tc.platforms_n; ++i)
                {
                    if (MFX_HW_UNKNOWN == tc.platform[i])
                        continue;

                    if (g_tsHWtype == tc.platform[i])
                    {
                        ValidHWPlatform = true;
                        break;
                    }
                };
                if (!ValidHWPlatform)
                    bPlatformSupportsMCTF = false;
            }
            
            std::list<mfxU32> desired_filters;
            for (mfxU32 i = 0; i < tc.alg_num; ++i)
                desired_filters.push_back(tc.alg_list[i]);

            // lets find all filters:
            mfxU32 bNumbersFiltersFound(0);
            while (!desired_filters.empty())
            {
                for (mfxU32 i = 0; i < vpp_du2->NumAlg; i++)
                {
                    if (vpp_du2->AlgList[i] == desired_filters.back())
                    {
                        ++bNumbersFiltersFound;
                        desired_filters.pop_back();
                        break;
                    }
                }
            }

            //error if: either filter found but platform does not support, OR filter(s) are not found, but platform supports
            if ((bNumbersFiltersFound == tc.alg_num) ^ bPlatformSupportsMCTF)
            {
                TS_FAIL_TEST("Wrong reporting of MCTF active status! Check combination of platform & reporting.", MFX_ERR_NONE);
            }
            delete[] vpp_du->AlgList;
            vpp_du->AlgList = 0;
            delete[] vpp_du2->AlgList;
            vpp_du2->AlgList = 0;
        }
        else  if (tc.mode == DOUSE)
        {
            for (mfxU32 i = 0; i< vpp_du->NumAlg; i++)
            {
                if (vpp_du->AlgList[i] == MFX_EXTBUFF_VPP_DENOISE)
                {
                    mfxExtVPPDenoise* denoise = (mfxExtVPPDenoise*)par_init.GetExtBuffer(MFX_EXTBUFF_VPP_DENOISE);

                    if (denoise->DenoiseFactor > 100)
                        TS_FAIL_TEST("Denoise factor is not valid", MFX_ERR_NONE);

                }
                else if (vpp_du->AlgList[i] == MFX_EXTBUFF_VPP_DETAIL)
                {
                    mfxExtVPPDetail* detail = (mfxExtVPPDetail*)par_init.GetExtBuffer(MFX_EXTBUFF_VPP_DETAIL);

                    if (detail->DetailFactor > 100)
                        TS_FAIL_TEST("Detail factor is not valid", MFX_ERR_NONE);

                }
                else if (vpp_du->AlgList[i] == MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION)
                {
                    mfxExtVPPFrameRateConversion* frc = (mfxExtVPPFrameRateConversion*)par_init.GetExtBuffer(MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION);

                    if (frc->Algorithm != MFX_FRCALGM_PRESERVE_TIMESTAMP && frc->Algorithm != MFX_FRCALGM_DISTRIBUTED_TIMESTAMP
                        && frc->Algorithm != MFX_FRCALGM_FRAME_INTERPOLATION)
                        TS_FAIL_TEST("FRC Algorithm is not valid", MFX_ERR_NONE);

                }
                else if (vpp_du->AlgList[i] == MFX_EXTBUFF_VPP_PROCAMP)
                {
                    mfxExtVPPProcAmp* procamp = (mfxExtVPPProcAmp*)par_init.GetExtBuffer(MFX_EXTBUFF_VPP_PROCAMP);

                    if (procamp->Brightness < -100.0F || procamp->Brightness > 100.0F)
                        TS_FAIL_TEST("ProcAmp Brightness is not valid", MFX_ERR_NONE);
                    if (procamp->Contrast < 0.0F || procamp->Contrast > 10.0F)
                        TS_FAIL_TEST("ProcAmp Contrast is not valid", MFX_ERR_NONE);
                    if (procamp->Hue < -180.0F || procamp->Hue > 180.0F)
                        TS_FAIL_TEST("ProcAmp Hue is not valid", MFX_ERR_NONE);
                    if (procamp->Saturation < 0.0F || procamp->Saturation > 10.0F)
                        TS_FAIL_TEST("ProcAmp Saturation is not valid", MFX_ERR_NONE);

                }
                else if (vpp_du->AlgList[i] == MFX_EXTBUFF_VPP_DEINTERLACING)
                {
                    mfxExtVPPDeinterlacing* deinter = (mfxExtVPPDeinterlacing*)par_init.GetExtBuffer(MFX_EXTBUFF_VPP_DEINTERLACING);

                    if (deinter->Mode != MFX_DEINTERLACING_BOB && deinter->Mode != MFX_DEINTERLACING_ADVANCED
                        && deinter->Mode != MFX_DEINTERLACING_AUTO_DOUBLE && deinter->Mode != MFX_DEINTERLACING_AUTO_SINGLE
                        && deinter->Mode != MFX_DEINTERLACING_FULL_FR_OUT && deinter->Mode != MFX_DEINTERLACING_HALF_FR_OUT
                        && deinter->Mode != MFX_DEINTERLACING_24FPS_OUT && deinter->Mode != MFX_DEINTERLACING_FIXED_TELECINE_PATTERN
                        && deinter->Mode != MFX_DEINTERLACING_30FPS_OUT && deinter->Mode != MFX_DEINTERLACING_DETECT_INTERLACE
                        && deinter->Mode != MFX_DEINTERLACING_ADVANCED_NOREF && deinter->Mode != MFX_DEINTERLACING_ADVANCED_SCD)
                        TS_FAIL_TEST("Deinterlacing Mode is not valid", MFX_ERR_NONE);
                }
                else if ((vpp_du->AlgList[i] == MFX_EXTBUFF_VPP_MCTF))
                {
                    mfxExtVppMctf* pMctf = (mfxExtVppMctf*)par_init.GetExtBuffer(MFX_EXTBUFF_VPP_MCTF);
                    if (MFX_CODINGOPTION_OFF != pMctf->Overlap)
                        TS_FAIL_TEST("MCTF DoUse settings for Overlaping is not valid", MFX_ERR_NONE);

                    if (MFX_CODINGOPTION_OFF != pMctf->Deblocking)
                        TS_FAIL_TEST("MCTF DoUse settings for Deblocking is not valid", MFX_ERR_NONE);

                    if (MFX_MCTF_TEMPORAL_MODE_2REF != pMctf->TemporalMode)
                        TS_FAIL_TEST("MCTF DoUse settings for TemporalModel is not valid", MFX_ERR_NONE);

                    if (MFX_MVPRECISION_INTEGER != pMctf->MVPrecision)
                        TS_FAIL_TEST("MCTF DoUse settings for MVPrecision is not valid", MFX_ERR_NONE);
                }
            }

            delete[] vpp_du->AlgList;
            vpp_du->AlgList = 0;
            delete[] vpp_du2->AlgList;
            vpp_du2->AlgList = 0;
        }
        else if (tc.mode == MFX_PAR) {
            EXPECT_EQ(m_par, par_init) << "ERROR: Filter's parameters before and after GetVideoPram() are not equal \n";
            for (mfxU32 i = 0; i< par_start.NumExtParam; i++)
                switch (par_start.ExtParam[i]->BufferId)
                {
                case MFX_EXTBUFF_VPP_MCTF:
                {
                    mfxExtVppMctf* pBuf_start = reinterpret_cast<mfxExtVppMctf*>(par_start.ExtParam[i]);
                    mfxExtVppMctf* pBuf = reinterpret_cast<mfxExtVppMctf*>(m_par.ExtParam[i]);
                    mfxExtVppMctf* pBuf_init = reinterpret_cast<mfxExtVppMctf*>(par_init.ExtParam[i]);
                    if (pBuf_start->FilterStrength <= 20)
                        EXPECT_EQ(pBuf_start->FilterStrength, pBuf_init->FilterStrength) << "MCTF ERROR: FilterStrength after Init & GetVideoPram was changed\n";
                    else
                        EXPECT_NE(pBuf_start->FilterStrength, pBuf_init->FilterStrength) << "MCTF ERROR: FilterStrength after Init & GetVideoPram was not changed\n";
                    EXPECT_EQ(pBuf->FilterStrength, pBuf_init->FilterStrength) << "MCTF ERROR: FilterStrength before and after GetVideoPram() is not equal \n";
                    if (MFX_CODINGOPTION_ON != pBuf_start->Deblocking &&
                        MFX_CODINGOPTION_OFF != pBuf_start->Deblocking)
                    {
                        if (MFX_CODINGOPTION_UNKNOWN != pBuf_start->Deblocking)
                            EXPECT_NE(pBuf_start->Deblocking, pBuf_init->Deblocking) << "MCTF ERROR: Deblocking after Init & GetVideoPram was changed\n";
                        else
                            EXPECT_EQ(pBuf_init->Deblocking, MFX_CODINGOPTION_OFF) << "MCTF ERROR: MFX_CODINGOPTION_UNKNOWN: Deblocking after Init must be MFX_CODINGOPTION_OFF\n";
                    }
                    else
                        EXPECT_EQ(pBuf_start->Deblocking, pBuf_init->Deblocking) << "MCTF ERROR: Deblocking after Init & GetVideoPram was not changed\n";
                    EXPECT_EQ(pBuf->Deblocking, pBuf_init->Deblocking) << "MCTF ERROR: Deblocking before and after GetVideoPram() is not equal \n";

                    if (MFX_CODINGOPTION_ON != pBuf_start->Overlap &&
                        MFX_CODINGOPTION_OFF != pBuf_start->Overlap)
                    {
                        if (MFX_CODINGOPTION_UNKNOWN != pBuf_start->Deblocking)
                            EXPECT_NE(pBuf_start->Overlap, pBuf_init->Overlap) << "MCTF ERROR: Overlap after Init & GetVideoPram was changed\n";
                        else
                            EXPECT_EQ(pBuf_init->Deblocking, MFX_CODINGOPTION_OFF) << "MCTF ERROR: MFX_CODINGOPTION_UNKNOWN: Overlap after Init must be MFX_CODINGOPTION_OFF\n";
                    }
                    else
                        EXPECT_EQ(pBuf_start->Overlap, pBuf_init->Overlap) << "MCTF ERROR: Overlap after Init & GetVideoPram was not changed\n";
                    EXPECT_EQ(pBuf->Overlap, pBuf_init->Overlap) << "MCTF ERROR: Overlap before and after GetVideoPram() is not equal \n";

                    if (MFX_MVPRECISION_QUARTERPEL != pBuf_start->MVPrecision &&
                        MFX_MVPRECISION_INTEGER != pBuf_start->MVPrecision)
                    {
                        if (MFX_MVPRECISION_UNKNOWN != pBuf_start->MVPrecision)
                            EXPECT_NE(pBuf_start->MVPrecision, pBuf_init->MVPrecision) << "MCTF ERROR: MVPrecision after Init & GetVideoPram was changed\n";
                        else
                            EXPECT_EQ(pBuf_init->MVPrecision, MFX_MVPRECISION_INTEGER) << "MCTF ERROR: MFX_MVPRECISION_UNKNOWN: MVPrecision after Init must be MFX_MVPRECISION_INTEGER\n";
                    }
                    else
                        EXPECT_EQ(pBuf_start->MVPrecision, pBuf_init->MVPrecision) << "MCTF ERROR: MVPrecision after Init & GetVideoPram was not changed\n";
                    EXPECT_EQ(pBuf->MVPrecision, pBuf_init->MVPrecision) << "MCTF ERROR: MVPrecision before and after GetVideoPram() is not equal \n";

                    if (MFX_MCTF_TEMPORAL_MODE_4REF != pBuf_start->TemporalMode &&
                        MFX_MCTF_TEMPORAL_MODE_2REF != pBuf_start->TemporalMode &&
                        MFX_MCTF_TEMPORAL_MODE_1REF != pBuf_start->TemporalMode &&
                        MFX_MCTF_TEMPORAL_MODE_SPATIAL != pBuf_start->TemporalMode)
                    {
                        if (MFX_MCTF_TEMPORAL_MODE_UNKNOWN != pBuf_start->TemporalMode)
                            EXPECT_NE(pBuf_start->TemporalMode, pBuf_init->TemporalMode) << "MCTF ERROR: TemporalMode after Init & GetVideoPram was changed\n";
                        else
                            EXPECT_EQ(pBuf_init->TemporalMode, MFX_MCTF_TEMPORAL_MODE_2REF) << "MCTF ERROR: MFX_MCTF_TEMPORAL_MODE_UNKNOWN: TemporalMode after Init must be MFX_MCTF_TEMPORAL_MODE_2REF\n";
                    }
                    else
                        EXPECT_EQ(pBuf_start->TemporalMode, pBuf_init->TemporalMode) << "MCTF ERROR: TemporalMode after Init & GetVideoPram was not changed\n";
                    EXPECT_EQ(pBuf->TemporalMode, pBuf_init->TemporalMode) << "MCTF ERROR: TemporalMode before and after GetVideoPram() is not equal \n";
                }
                break;

                default:
                    break;
                }

        }

        if (tc.mode == NULL_SESSION)
            m_session = m_session_tmp;

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(vpp_mctf_getvideoparam);

}