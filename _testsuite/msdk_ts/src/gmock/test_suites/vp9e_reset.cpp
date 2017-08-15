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
#define MFX_PROFILE_VP9_CHANGE (11)
#define MFX_PROFILE_VP9_FULL_CHANGE (12)

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_VP9) {}
        ~TestSuite() {}

        struct tc_par;
        typedef void (TestSuite::*callback)(tc_par&);

        struct ResetPar
        {
            mfxSession      session;
            tsExtBufType<mfxVideoParam>*  pPar;
        } m_resetPar;

        struct tc_par
        {
            callback set_par;
            mfxU32   p0;
            mfxU32   p1;
            mfxU32   p2;
            mfxU32   p3;
            mfxU32   p4;
        };

        void set_par(tc_par& arg) { memcpy(Ptr(arg.p0) + arg.p1, &arg.p3, arg.p2); }

        struct tc_struct
        {
            mfxStatus sts;
            tc_par pre_init;
            mfxU32 encode_before;
            tc_par pre_reset;
            mfxU32 encode_after;
        };

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);
        static const unsigned int n_cases;

    private:

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

        void close_encoder(tc_par& arg) { Close(); }

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
        {/*0*/ MFX_ERR_NONE,{}, 0,{}, 0 },
        {/*1*/ MFX_ERR_NONE,{}, 1,{}, 1 },
        {/*2*/ MFX_ERR_NONE,{}, 2,{}, 2 },
        {/*3*/ MFX_ERR_INVALID_HANDLE,{}, 0,{ &TestSuite::set_par, RESET_PAR, offsetof(ResetPar, session), sizeof(mfxSession), 0 }, 0 },
        {/*4*/ MFX_ERR_NULL_PTR,{}, 0,{ &TestSuite::set_par, RESET_PAR, offsetof(ResetPar, pPar), sizeof(void*), 0 }, 0 },
        {/*5*/ MFX_ERR_NOT_INITIALIZED,{}, 0,{ &TestSuite::close_encoder, }, 0 },
        {/*6*/ MFX_ERR_NONE,{ &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_VIDEO_MEMORY }, 1,{}, 1 },
        {/*7*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
            { &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_VIDEO_MEMORY },  0,
            { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_SYSTEM_MEMORY }, 0 },
        {/*8*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
            { &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_SYSTEM_MEMORY }, 0,
            { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_VIDEO_MEMORY },  0 },
        {/*9*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
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
        {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM,{}, 0,{ &TestSuite::set_par, MFX_RESET,  offsetof(mfxVideoParam, mfx)
            + offsetof(mfxInfoMFX, FrameInfo)
            + offsetof(mfxFrameInfo, PicStruct), sizeof(mfxU16), MFX_PICSTRUCT_FIELD_TFF }, 0 },
        {/*27 check error status on wrogn FourCC*/ MFX_ERR_INVALID_VIDEO_PARAM,{}, 0,{ &TestSuite::set_par, MFX_RESET,  offsetof(mfxVideoParam, mfx)
            + offsetof(mfxInfoMFX, FrameInfo)
            + offsetof(mfxFrameInfo, FourCC), sizeof(mfxU16), MFX_FOURCC_YV12 }, 0 },
        {/*28 check error status on wrong ChromaFormat*/ MFX_ERR_INVALID_VIDEO_PARAM,{}, 0,{ &TestSuite::set_par, MFX_RESET,  offsetof(mfxVideoParam, mfx)
            + offsetof(mfxInfoMFX, FrameInfo)
            + offsetof(mfxFrameInfo, ChromaFormat), sizeof(mfxU16), MFX_CHROMAFORMAT_YUV411 }, 0 },
        {/*29 check error status on wrong profile*/ MFX_ERR_INVALID_VIDEO_PARAM,
            {}, 0, { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, CodecProfile), sizeof(mfxU16), MFX_PROFILE_VP9_CHANGE }, 0 },

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

        {/*36 increasing NumRefFrame caused an error (reallocation impossible in Reset())*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
            { &TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, NumRefFrame), sizeof(mfxU16), 1 }, 0,
            { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, NumRefFrame), sizeof(mfxU16), 2 }, 0 },

        {/*37*/ MFX_ERR_NONE,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtCodingOption2) }, 0 },
        {/*38*/ MFX_ERR_NONE,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtEncoderResetOption) }, 0 },
        {/*39*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtCodingOption) }, 0 },
        {/*40*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtCodingOptionSPSPPS) }, 0 },
        {/*41*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDoNotUse) }, 0 },
        {/*42*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVppAuxData) }, 0 },
        {/*43*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDenoise) }, 0 },
        {/*44*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPProcAmp) }, 0 },
        {/*45*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDetail) }, 0 },
        {/*46*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVideoSignalInfo) }, 0 },
        {/*47*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDoUse) }, 0 },
        {/*48*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtAVCRefListCtrl) }, 0 },
        {/*49*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPFrameRateConversion) }, 0 },
        {/*50*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtPictureTimingSEI) }, 0 },
        {/*51*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtAvcTemporalLayers) }, 0 },
        {/*52*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPImageStab) }, 0 },
        {/*53*/ MFX_ERR_UNSUPPORTED,{}, 0,{ &TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtEncoderCapability) }, 0 },
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

        {/*64 check error status on attempt to change format*/ MFX_ERR_INVALID_VIDEO_PARAM,
            {}, 0, { &TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, CodecProfile), sizeof(mfxU16), MFX_PROFILE_VP9_FULL_CHANGE }, 0 },
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

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;
        tsExtBufType<mfxVideoParam> reset_par;
        BitstreamChecker bs_checker(m_par);
        m_bs_processor = &bs_checker;

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
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

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
            // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL(NV12) and ICL(P010, AYUV, Y410)
            if ((fourcc_id == MFX_FOURCC_NV12 && g_tsHWtype < MFX_HW_CNL)
                || ((fourcc_id == MFX_FOURCC_P010 || fourcc_id == MFX_FOURCC_AYUV
                    || fourcc_id == MFX_FOURCC_Y410) && g_tsHWtype < MFX_HW_ICL))
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
        }
        else {
            ADD_FAILURE() << "ERROR: loading encoder from plugin failed!";
            throw tsFAIL;
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
            //if resolution is changed - surfaces info need to be updated to keep processing
            g_tsStatus.check(DrainEncodedBitstream());
            TS_CHECK_MFX;
            g_tsStatus.check(UpdateSurfaceResolutionInfo(m_resetPar.pPar->mfx.FrameInfo.Width, m_resetPar.pPar->mfx.FrameInfo.Height));
            TS_CHECK_MFX;
        }

        if (m_resetPar.pPar && m_resetPar.pPar->mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
            m_resetPar.pPar->mfx.MaxKbps = m_resetPar.pPar->mfx.TargetKbps*2;
        }

        if (m_resetPar.pPar && m_resetPar.pPar->mfx.CodecProfile == MFX_PROFILE_VP9_CHANGE)
        {
            //set wrong profile for Reset() to check error status is returned
            if (m_par.mfx.CodecProfile == MFX_PROFILE_VP9_3)
            {
                m_resetPar.pPar->mfx.CodecProfile = MFX_PROFILE_VP9_0;
            }
            else
            {
                m_resetPar.pPar->mfx.CodecProfile++;
            }
        }
        else if (m_resetPar.pPar && m_resetPar.pPar->mfx.CodecProfile == MFX_PROFILE_VP9_FULL_CHANGE)
        {
            //change source format for Reset() to check error status is returned
            m_resetPar.pPar->mfx.CodecProfile = 0;
            if (fourcc_id == MFX_FOURCC_NV12)
            {
                m_resetPar.pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
                m_resetPar.pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
                m_resetPar.pPar->mfx.FrameInfo.Shift = 1;
                m_resetPar.pPar->mfx.FrameInfo.BitDepthLuma = m_resetPar.pPar->mfx.FrameInfo.BitDepthChroma = 10;
            }
            else if (fourcc_id == MFX_FOURCC_P010)
            {
                m_resetPar.pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
                m_resetPar.pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
                m_resetPar.pPar->mfx.FrameInfo.BitDepthLuma = m_resetPar.pPar->mfx.FrameInfo.BitDepthChroma = 8;
            }
            else if (fourcc_id == MFX_FOURCC_AYUV)
            {
                m_resetPar.pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
                m_resetPar.pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
                m_resetPar.pPar->mfx.FrameInfo.BitDepthLuma = m_resetPar.pPar->mfx.FrameInfo.BitDepthChroma = 10;
            }
            else if (fourcc_id == MFX_FOURCC_Y410)
            {
                m_resetPar.pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
                m_resetPar.pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
                m_resetPar.pPar->mfx.FrameInfo.BitDepthLuma = m_resetPar.pPar->mfx.FrameInfo.BitDepthChroma = 8;
            }
        }

        TRACE_FUNC2(MFXVideoENCODE_Reset, m_resetPar.session, m_resetPar.pPar);
        mfxStatus reset_status = MFXVideoENCODE_Reset(m_resetPar.session, m_resetPar.pPar);
        bs_checker.UpdatePar(reset_par);

        if (reset_status >= 0)
        {
            g_tsLog << "INFO: Call GetVideoParam() after Reset() to check changes in params...\n";
            mfxVideoParam param_after_reset = {};
            GetVideoParam(m_session, &param_after_reset);
        }

        g_tsLog << "INFO: Checking Reset return code...\n";
        g_tsStatus.expect(tc.sts);
        g_tsStatus.check(reset_status);

        if (g_tsStatus.get() >= 0)
        {
            EncodeFrames(tc.encode_after);
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_reset,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_reset, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_8b_444_ayuv_reset,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_444_y410_reset, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
};
