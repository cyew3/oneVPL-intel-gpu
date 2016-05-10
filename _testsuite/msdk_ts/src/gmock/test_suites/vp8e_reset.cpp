/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"

namespace
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_VP8) {}
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
        switch(id)
        {
        case MFX_INIT:  return (mfxU8*)&m_par;
        case MFX_RESET: return (mfxU8*)m_resetPar.pPar;
        case RESET_PAR: return (mfxU8*)&m_resetPar;
        default: return 0;
        }
    }

    void set_par      (tc_par& arg) { memcpy(Ptr(arg.p0) + arg.p1, &arg.p3, arg.p2); }
    void close_encoder(tc_par& arg) { Close(); }
    void COVP8        (tc_par& arg)
    {
        tsExtBufType<mfxVideoParam>* p = (arg.p0 == MFX_INIT) ? &m_par : m_resetPar.pPar;
        mfxExtVP8CodingOption& co      = *p;
        co.EnableMultipleSegments      = arg.p3;
    }
    void Size(tc_par& arg)
    {
        mfxVideoParam* p = (mfxVideoParam*)Ptr(arg.p0);
        p->mfx.FrameInfo.Width  = (mfxU16)arg.p1;
        p->mfx.FrameInfo.Height = (mfxU16)arg.p2;
        p->mfx.FrameInfo.CropW  = (mfxU16)arg.p3;
        p->mfx.FrameInfo.CropH  = (mfxU16)arg.p4;
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
    {/* 0*/ MFX_ERR_INVALID_HANDLE,  {}, 0, {&TestSuite::set_par, RESET_PAR, offsetof(ResetPar, session), sizeof(mfxSession), 0}, 0},
    {/* 1*/ MFX_ERR_NULL_PTR,        {}, 0, {&TestSuite::set_par, RESET_PAR, offsetof(ResetPar, pPar), sizeof(void*), 0}, 0},
    {/* 2*/ MFX_ERR_NOT_INITIALIZED, {}, 0, {&TestSuite::close_encoder,}, 0},
    {/* 3*/ MFX_ERR_NONE,            {}, 0, {}, 0},
    {/* 4*/ MFX_ERR_NONE,            {}, 1, {}, 1},
    {/* 5*/ MFX_ERR_NONE, {&TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_VIDEO_MEMORY}, 1, {}, 1},
    {/* 6*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {&TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_VIDEO_MEMORY},  0,
        {&TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_SYSTEM_MEMORY}, 0},
    {/* 7*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {&TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_SYSTEM_MEMORY}, 0,
        {&TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_VIDEO_MEMORY},  0},
    {/* 8*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {&TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_OPAQUE_MEMORY}, 0,
        {&TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_SYSTEM_MEMORY}, 0},
    {/* 9*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {&TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, AsyncDepth), sizeof(mfxU16), 3}, 0,
        {&TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, AsyncDepth), sizeof(mfxU16), 4}, 0},
    {/*10*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {&TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, AsyncDepth), sizeof(mfxU16), 4}, 0,
        {&TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, AsyncDepth), sizeof(mfxU16), 3}, 0},
    {/*11*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {&TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_SYSTEM_MEMORY}, 0,
        {&TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_OPAQUE_MEMORY}, 0},
    {/*12*/ MFX_ERR_NONE, {&TestSuite::Size, MFX_INIT, 720, 480, 720, 480}, 1, {&TestSuite::Size, MFX_RESET, 720, 480, 704, 480}, 1},
    {/*13*/ MFX_ERR_NONE, {&TestSuite::Size, MFX_INIT, 720, 480, 720, 480}, 1, {&TestSuite::Size, MFX_RESET, 704, 480, 704, 480}, 1},
    {/*14*/ MFX_ERR_NONE, {&TestSuite::Size, MFX_INIT, 720, 480, 720, 480}, 1, {&TestSuite::Size, MFX_RESET, 720, 480, 704, 480}, 1},
    {/*15*/ MFX_ERR_NONE, {&TestSuite::Size, MFX_INIT, 720, 480, 720, 480}, 1, {&TestSuite::Size, MFX_RESET, 720, 480, 720, 470}, 1},
    {/*16*/ MFX_ERR_NONE, {&TestSuite::Size, MFX_INIT, 720, 480, 720, 480}, 1, {&TestSuite::Size, MFX_RESET, 720, 464, 720, 464}, 1},
    {/*17*/ MFX_ERR_NONE, {&TestSuite::Size, MFX_INIT, 720, 480, 640, 480}, 1, {&TestSuite::Size, MFX_RESET, 720, 480, 720, 480}, 1},
    {/*18*/ MFX_ERR_NONE, {&TestSuite::Size, MFX_INIT, 720, 480, 720, 352}, 1, {&TestSuite::Size, MFX_RESET, 720, 480, 720, 480}, 1},
    {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, {&TestSuite::Size, MFX_INIT, 720, 480, 720, 480}, 0, {&TestSuite::Size, MFX_RESET, 710, 480, 710, 480}, 0},
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, {&TestSuite::Size, MFX_INIT, 720, 480, 720, 480}, 0, {&TestSuite::Size, MFX_RESET, 720, 470, 720, 470}, 0},
    {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, {&TestSuite::Size, MFX_INIT, 720, 480, 720, 480}, 0, {&TestSuite::Size, MFX_RESET, 720, 470, 720, 352}, 0},
    {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, {&TestSuite::Size, MFX_INIT, 720, 480, 720, 480}, 0, {&TestSuite::Size, MFX_RESET, 710, 480, 704, 480}, 0},
    {/*23*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {&TestSuite::Size, MFX_INIT, 720, 480, 720, 480}, 0, {&TestSuite::Size, MFX_RESET, 840, 480, 840, 480}, 0},
    {/*24*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {&TestSuite::Size, MFX_INIT, 720, 480, 720, 480}, 0, {&TestSuite::Size, MFX_RESET, 720, 496, 720, 496}, 0},
    {/*25*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {}, 0, {&TestSuite::set_par, MFX_RESET,  offsetof(mfxVideoParam, mfx)
                                                                          + offsetof(mfxInfoMFX, FrameInfo)
                                                                          + offsetof(mfxFrameInfo, PicStruct), sizeof(mfxU16), MFX_PICSTRUCT_FIELD_TFF}, 0},
    {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::set_par, MFX_RESET,  offsetof(mfxVideoParam, mfx)
                                                                     + offsetof(mfxInfoMFX, FrameInfo)
                                                                     + offsetof(mfxFrameInfo, FourCC), sizeof(mfxU16), MFX_FOURCC_YV12}, 0},
    {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::set_par, MFX_RESET,  offsetof(mfxVideoParam, mfx)
                                                                     + offsetof(mfxInfoMFX, FrameInfo)
                                                                     + offsetof(mfxFrameInfo, ChromaFormat), sizeof(mfxU16), MFX_CHROMAFORMAT_YUV444}, 0},
    {/*28*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        {&TestSuite::set_par, MFX_INIT,  offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, CodecProfile), sizeof(mfxU16), MFX_PROFILE_VP8_0}, 0,
        {&TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, CodecProfile), sizeof(mfxU16), MFX_PROFILE_VP8_3}, 0},
    {/*29*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {}, 0,
        {&TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, CodecLevel), sizeof(mfxU16), 1}, 0},
    {/*30*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {}, 0,
        {&TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, GopRefDist), sizeof(mfxU16), 1}, 0},
    {/*31*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {}, 0,
        {&TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, NumSlice), sizeof(mfxU16), 1}, 0},
    {/*32*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {}, 0,
        {&TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, IdrInterval), sizeof(mfxU16), 1}, 0},
    {/*33*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {}, 0,
        {&TestSuite::set_par, MFX_RESET, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, NumRefFrame), sizeof(mfxU16), 1}, 0},
    {/*34*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtCodingOption           )}, 0},
    {/*35*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtCodingOptionSPSPPS     )}, 0},
    {/*36*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDoNotUse            )}, 0},
    {/*37*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVppAuxData             )}, 0},
    {/*38*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDenoise             )}, 0},
    {/*39*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPProcAmp             )}, 0},
    {/*40*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDetail              )}, 0},
    {/*41*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVideoSignalInfo        )}, 0},
    {/*42*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDoUse               )}, 0},
    {/*43*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtAVCRefListCtrl         )}, 0},
    {/*44*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPFrameRateConversion )}, 0},
    {/*45*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtPictureTimingSEI       )}, 0},
    {/*46*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtAvcTemporalLayers      )}, 0},
    {/*47*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtCodingOption2          )}, 0},
    {/*48*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPImageStab           )}, 0},
    {/*49*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtEncoderCapability      )}, 0},
    {/*50*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtEncoderResetOption     )}, 0},
    {/*51*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtAVCEncodedFrameInfo    )}, 0},
    {/*52*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPComposite           )}, 0},
    {/*53*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPVideoSignalInfo     )}, 0},
    {/*54*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtEncoderROI             )}, 0},
    {/*55*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtVPPDeinterlacing       )}, 0},
    {/*56*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtMVCSeqDesc             )}, 0},
    {/*57*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtMVCTargetViews         )}, 0},
    {/*58*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtJPEGQuantTables        )}, 0},
    {/*59*/ MFX_ERR_INVALID_VIDEO_PARAM, {}, 0, {&TestSuite::ExtBuf, MFX_RESET, EXT_BUF_PAR(mfxExtJPEGHuffmanTables      )}, 0},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);


class BitstreamChecker : public tsBitstreamProcessor, public tsParserVP8
{
    tsExtBufType<mfxVideoParam>& m_par;
public:
    BitstreamChecker(tsExtBufType<mfxVideoParam>& par) : m_par(par) {}
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
};

mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    mfxU32 checked = 0;

    SetBuffer(bs);

    while(checked++ < nFrames)
    {
        tsParserVP8::UnitType& hdr = ParseOrDie();

        EXPECT_EQ(m_par.mfx.CodecProfile - 1, (int)hdr.udc->version);

        if(!hdr.udc->key_frame)
        {
            EXPECT_EQ(m_par.mfx.FrameInfo.CropW, hdr.udc->Width);
            EXPECT_EQ(m_par.mfx.FrameInfo.CropH, hdr.udc->Height);
        }

        if((mfxExtVP8CodingOption*)m_par)
        {
            mfxExtVP8CodingOption& co = m_par;
            if(co.EnableMultipleSegments)
            {
                EXPECT_EQ(mfxU32(co.EnableMultipleSegments == MFX_CODINGOPTION_ON), hdr.udc->fh.segmentation_enabled);
            }
        }
    }
    EXPECT_EQ(bs.DataLength, (mfxU32)get_offset()) << "FAILED: more than " << nFrames << " frame(s) in output stream";
    bs.DataLength = 0;

    return MFX_ERR_NONE;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tsExtBufType<mfxVideoParam> reset_par;
    tc_struct tc = test_case[id];
    BitstreamChecker bs_checker(reset_par);
    m_bs_processor = &bs_checker;

    if(tc.pre_init.set_par)
    {
        (this->*tc.pre_init.set_par)(tc.pre_init);
    }

    MFXInit();
    Load();

    Init();
    GetVideoParam(m_session, &reset_par);
    EncodeFrames(tc.encode_before);

    m_resetPar.session = m_session;
    m_resetPar.pPar    = &reset_par;

    if(tc.pre_reset.set_par)
    {
        (this->*tc.pre_reset.set_par)(tc.pre_reset);
    }
    bs_checker.reset();

    g_tsStatus.expect(tc.sts);
    Reset(m_resetPar.session, m_resetPar.pPar);

    if(g_tsStatus.get() >= 0)
    {
        EncodeFrames(tc.encode_after);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vp8e_reset);
};