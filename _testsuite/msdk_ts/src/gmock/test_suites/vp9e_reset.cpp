/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"

namespace vp9e_reset
{
#define VP9E_PREDEFINED_BITRATE_VALUE (1111)
    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_VP9) {}
        ~TestSuite() {}
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:
        struct ResetPar
        {
            mfxSession      session;
            tsExtBufType<mfxVideoParam>*  pPar;
        } m_resetPar;

        struct tc_par;
        typedef void (TestSuite::*callback)(tc_par&);

        struct tc_par
        {
            callback set_par;
            mfxU32   p0;
            mfxU32   p1;
            mfxU32   p2;
            mfxU32   p3;
            mfxU32   p4;
        };

        struct tc_struct
        {
            mfxStatus sts;
            tc_par pre_init;
            mfxU32 encode_before;
            tc_par pre_reset;
            mfxU32 encode_after;
        };

        static const tc_struct test_case[];

        enum
        {
            MFX_INIT,
            MFX_RESET,
            RESET_PAR,
        };

        mfxU8* Ptr(mfxU32 id)
        {
            switch (id)
            {
            case MFX_INIT:  return (mfxU8*)&m_par;
            case MFX_RESET: return (mfxU8*)m_resetPar.pPar;
            case RESET_PAR: return (mfxU8*)&m_resetPar;
            default: return 0;
            }
        }

        void set_par(tc_par& arg) { memcpy(Ptr(arg.p0) + arg.p1, &arg.p3, arg.p2); }
        void close_encoder(tc_par& arg) { Close(); }
        void COVP9(tc_par& arg)
        {
            tsExtBufType<mfxVideoParam>* p = (arg.p0 == MFX_INIT) ? &m_par : m_resetPar.pPar;
            mfxExtCodingOptionVP9& co = *p;
            co.EnableMultipleSegments = arg.p3;
        }
        void Size(tc_par& arg)
        {
            mfxVideoParam* p = (mfxVideoParam*)Ptr(arg.p0);
            p->mfx.FrameInfo.Width = (mfxU16)arg.p1;
            p->mfx.FrameInfo.Height = (mfxU16)arg.p2;
            p->mfx.FrameInfo.CropW = (mfxU16)arg.p3;
            p->mfx.FrameInfo.CropH = (mfxU16)arg.p4;
        }
        void ExtBuf(tc_par& arg)
        {
            tsExtBufType<mfxVideoParam>* p = (tsExtBufType<mfxVideoParam>*)Ptr(arg.p0);
            p->AddExtBuffer(arg.p1, arg.p2);
        }
    };

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/* 0*/ MFX_ERR_NONE,{}, 0,{}, 0 },
        {/* 1*/ MFX_ERR_NONE,{}, 1,{}, 1 },
        {/* 2*/ MFX_ERR_NONE,{}, 2,{}, 2 },
        {/* 3*/ MFX_ERR_INVALID_HANDLE,{}, 0,{ &TestSuite::set_par, RESET_PAR, offsetof(ResetPar, session), sizeof(mfxSession), 0 }, 0 },
        {/* 4*/ MFX_ERR_NULL_PTR,{}, 0,{ &TestSuite::set_par, RESET_PAR, offsetof(ResetPar, pPar), sizeof(void*), 0 }, 0 },
        {/* 5*/ MFX_ERR_NOT_INITIALIZED,{}, 0,{ &TestSuite::close_encoder, }, 0 },
        {/* 6*/ MFX_ERR_NONE,{ &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_VIDEO_MEMORY }, 1,{}, 1 },
        {/* 7*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        { &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_VIDEO_MEMORY },  0,
        { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_SYSTEM_MEMORY }, 0 },
        {/* 8*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        { &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_SYSTEM_MEMORY }, 0,
        { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_VIDEO_MEMORY },  0 },
        {/* 9*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        { &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_OPAQUE_MEMORY }, 0,
        { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_SYSTEM_MEMORY }, 0 },
        {/*10*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        { &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, AsyncDepth), sizeof(mfxU16), 3 }, 0,
        { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, AsyncDepth), sizeof(mfxU16), 4 }, 0 },
        {/*11*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        { &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, AsyncDepth), sizeof(mfxU16), 4 }, 0,
        { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, AsyncDepth), sizeof(mfxU16), 3 }, 0 },
        {/*12*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        { &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_SYSTEM_MEMORY }, 0,
        { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_OPAQUE_MEMORY }, 0 },
        {/*13*/ MFX_ERR_NONE,{ &TestSuite::Size, MFX_INIT, 720, 480, 720, 480 }, 1,{ &TestSuite::Size, MFX_RESET, 720, 480, 704, 480 }, 1 },
        {/*14*/ MFX_ERR_NONE,{ &TestSuite::Size, MFX_INIT, 720, 480, 720, 480 }, 1,{ &TestSuite::Size, MFX_RESET, 704, 480, 704, 480 }, 1 },
        {/*15*/ MFX_ERR_NONE,{ &TestSuite::Size, MFX_INIT, 720, 480, 720, 480 }, 1,{ &TestSuite::Size, MFX_RESET, 720, 480, 704, 480 }, 1 },
        {/*16*/ MFX_ERR_NONE,{ &TestSuite::Size, MFX_INIT, 720, 480, 720, 480 }, 1,{ &TestSuite::Size, MFX_RESET, 720, 480, 720, 470 }, 1 },
        {/*17*/ MFX_ERR_NONE,{ &TestSuite::Size, MFX_INIT, 720, 480, 720, 480 }, 1,{ &TestSuite::Size, MFX_RESET, 720, 464, 720, 464 }, 1 },
        {/*18*/ MFX_ERR_NONE,{ &TestSuite::Size, MFX_INIT, 720, 480, 640, 480 }, 1,{ &TestSuite::Size, MFX_RESET, 720, 480, 720, 480 }, 1 },
        {/*19*/ MFX_ERR_NONE,{ &TestSuite::Size, MFX_INIT, 720, 480, 720, 352 }, 1,{ &TestSuite::Size, MFX_RESET, 720, 480, 720, 480 }, 1 },
        {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM,{ &TestSuite::Size, MFX_INIT, 720, 480, 720, 480 }, 0,{ &TestSuite::Size, MFX_RESET, 710, 480, 710, 480 }, 0 },
        {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM,{ &TestSuite::Size, MFX_INIT, 720, 480, 720, 480 }, 0,{ &TestSuite::Size, MFX_RESET, 720, 470, 720, 470 }, 0 },
        {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM,{ &TestSuite::Size, MFX_INIT, 720, 480, 720, 480 }, 0,{ &TestSuite::Size, MFX_RESET, 720, 470, 720, 352 }, 0 },
        {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM,{ &TestSuite::Size, MFX_INIT, 720, 480, 720, 480 }, 0,{ &TestSuite::Size, MFX_RESET, 710, 480, 704, 480 }, 0 },
        {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM,{ &TestSuite::Size, MFX_INIT, 720, 480, 720, 480 }, 0,{ &TestSuite::Size, MFX_RESET, 840, 480, 840, 480 }, 0 },
        {/*25*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,{ &TestSuite::Size, MFX_INIT, 720, 480, 720, 480 }, 0,{ &TestSuite::Size, MFX_RESET, 720, 496, 720, 496 }, 0 },
        {/*26*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,{}, 0,{ &TestSuite::set_par, MFX_RESET,  offsetof(mfxVideoParam, mfx)
        + offsetof(mfxInfoMFX, FrameInfo)
        + offsetof(mfxFrameInfo, PicStruct), sizeof(mfxU16), MFX_PICSTRUCT_FIELD_TFF }, 0 },
        {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM,{}, 0,{ &TestSuite::set_par, MFX_RESET,  offsetof(mfxVideoParam, mfx)
        + offsetof(mfxInfoMFX, FrameInfo)
        + offsetof(mfxFrameInfo, FourCC), sizeof(mfxU16), MFX_FOURCC_YV12 }, 0 },
        {/*28*/ MFX_ERR_INVALID_VIDEO_PARAM,{}, 0,{ &TestSuite::set_par, MFX_RESET,  offsetof(mfxVideoParam, mfx)
        + offsetof(mfxInfoMFX, FrameInfo)
        + offsetof(mfxFrameInfo, ChromaFormat), sizeof(mfxU16), MFX_CHROMAFORMAT_YUV444 }, 0 },
        {/*29*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        { &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, CodecProfile), sizeof(mfxU16), MFX_PROFILE_VP9_0 }, 0,
        { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, CodecProfile), sizeof(mfxU16), MFX_PROFILE_VP9_3 }, 0 },

        {/*30*/ MFX_ERR_NONE,
        { &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, GopPicSize), sizeof(mfxU16), 5 }, 0,
        { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, GopPicSize), sizeof(mfxU16), 10 }, 0 },

        {/*31*/ MFX_ERR_NONE, { }, 0,
        { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, QPP), sizeof(mfxU16), 200 }, 0 },

        {/*32*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,{}, 0, { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, QPB), sizeof(mfxU16), 200 }, 0 },

        {/*33*/ MFX_ERR_NONE,{}, 0,{ &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, TargetUsage), sizeof(mfxU16), MFX_TARGETUSAGE_BEST_SPEED }, 0 },

        {/*34*/ MFX_ERR_NONE,{}, 0,{ &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + +offsetof(mfxInfoMFX, FrameInfo) + offsetof(mfxFrameInfo, FrameRateExtN), sizeof(mfxU32), 50 }, 0 },

        {/*35*/ MFX_ERR_NONE,
        { &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, TargetKbps), sizeof(mfxU16), VP9E_PREDEFINED_BITRATE_VALUE }, 0,
        { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, TargetKbps), sizeof(mfxU16), VP9E_PREDEFINED_BITRATE_VALUE*2 }, 0 },

        {/*36*/ MFX_ERR_NONE,
        { &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, NumRefFrame), sizeof(mfxU16), 1 }, 0,
        { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, NumRefFrame), sizeof(mfxU16), 2 }, 0 },

        {/*37*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtCodingOption) }, 0 },
        {/*38*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtCodingOptionSPSPPS) }, 0 },
        {/*39*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDoNotUse) }, 0 },
        {/*40*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVppAuxData) }, 0 },
        {/*41*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDenoise) }, 0 },
        {/*42*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPProcAmp) }, 0 },
        {/*43*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDetail) }, 0 },
        {/*44*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVideoSignalInfo) }, 0 },
        {/*45*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDoUse) }, 0 },
        {/*46*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtAVCRefListCtrl) }, 0 },
        {/*47*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPFrameRateConversion) }, 0 },
        {/*48*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtPictureTimingSEI) }, 0 },
        {/*49*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtAvcTemporalLayers) }, 0 },
        {/*50*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtCodingOption2) }, 0 },
        {/*51*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPImageStab) }, 0 },
        {/*52*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtEncoderCapability) }, 0 },
        {/*53*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtEncoderResetOption) }, 0 },
        {/*54*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtAVCEncodedFrameInfo) }, 0 },
        {/*55*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPComposite) }, 0 },
        {/*56*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPVideoSignalInfo) }, 0 },
        {/*57*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtEncoderROI) }, 0 },
        {/*58*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDeinterlacing) }, 0 },
        {/*59*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtMVCSeqDesc) }, 0 },
        {/*60*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtMVCTargetViews) }, 0 },
        {/*61*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtJPEGQuantTables) }, 0 },
        {/*62*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtJPEGHuffmanTables) }, 0 },

        {/*63*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,{}, 0,{ &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, AsyncDepth), sizeof(mfxU16), 20 }, 0 },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);


    class BitstreamChecker : public tsBitstreamProcessor, public tsParserVP9
    {
        tsExtBufType<mfxVideoParam>& m_par;
    public:
        BitstreamChecker(tsExtBufType<mfxVideoParam>& par) : m_par(par) {}
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
        void UpdatePar(tsExtBufType<mfxVideoParam>& new_par)
        {
            m_par = new_par;
        }
    };

    mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        SetBuffer(bs);

        while (checked++ < nFrames)
        {
            tsParserVP9::UnitType& hdr = ParseOrDie();

            EXPECT_EQ(m_par.mfx.CodecProfile - 1, (int)hdr.uh.profile);
            EXPECT_EQ(m_par.mfx.FrameInfo.Width, hdr.uh.width);
            EXPECT_EQ(m_par.mfx.FrameInfo.Height, hdr.uh.height);
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        tsExtBufType<mfxVideoParam> reset_par;
        tc_struct tc = test_case[id];
        BitstreamChecker bs_checker(m_par);
        m_bs_processor = &bs_checker;

        if (tc.pre_init.set_par)
        {
            (this->*tc.pre_init.set_par)(tc.pre_init);
        }

        MFXInit();
        Load();

        m_par.AsyncDepth = 2;
        if (m_par.mfx.TargetKbps == VP9E_PREDEFINED_BITRATE_VALUE) {
            m_par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        }

        InitAndSetAllocator();

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            if (g_tsHWtype < MFX_HW_CNL) // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
        }
        else
        {
            g_tsLog << "WARNING: loading encoder from plugin failed!\n";
            return 0;
        }

        Init();
        GetVideoParam(m_session, &reset_par);
        EncodeFrames(tc.encode_before);

        m_resetPar.session = m_session;
        m_resetPar.pPar = &reset_par;

        if (tc.pre_reset.set_par)
        {
            (this->*tc.pre_reset.set_par)(tc.pre_reset);
        }

        if (m_resetPar.pPar && tc.encode_before && (m_par.mfx.FrameInfo.Width != m_resetPar.pPar->mfx.FrameInfo.Width || m_par.mfx.FrameInfo.Height != m_resetPar.pPar->mfx.FrameInfo.Height))
        {
            // If resolution is changed - surfaces info need to be updated to keep processing
            g_tsStatus.check(DrainEncodedBitstream());
            TS_CHECK_MFX;
            g_tsStatus.check(UpdateSurfaceResolutionInfo(m_resetPar.pPar->mfx.FrameInfo.Width, m_resetPar.pPar->mfx.FrameInfo.Height));
            TS_CHECK_MFX;
        }

        g_tsStatus.expect(tc.sts);
        Reset(m_resetPar.session, m_resetPar.pPar);
        bs_checker.UpdatePar(reset_par);

        if (g_tsStatus.get() >= 0)
        {
            EncodeFrames(tc.encode_after);
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(vp9e_reset);
};
