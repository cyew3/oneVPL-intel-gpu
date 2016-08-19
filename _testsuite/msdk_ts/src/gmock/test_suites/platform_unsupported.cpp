/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_parser.h"

#define TEST_NAME platform_unsupported

namespace TEST_NAME
{

typedef struct {
    mfxU32 BufferId;
    mfxU32 BufferSz;
    std::string string;
} BufferIdToString;

#define EXTBUF(TYPE, ID) {ID, sizeof(TYPE), #TYPE},
static BufferIdToString g_StringsOfBuffers[] =
{
#include "ts_ext_buffers_decl.h"
};
#undef EXTBUF

void GetBufferIdSz(const std::string& name, mfxU32& bufId, mfxU32& bufSz)
{
    //constexpr size_t maxBuffers = g_StringsOfBuffers / g_StringsOfBuffers[0];
    const size_t maxBuffers = sizeof( g_StringsOfBuffers ) / sizeof( g_StringsOfBuffers[0] );

    const std::string& buffer_name = name.substr(0, name.find(":"));

    for(size_t i(0); i < maxBuffers; ++i)
    {
        //if( name.find(g_StringsOfBuffers[i].string) != std::string::npos )
        if( buffer_name == g_StringsOfBuffers[i].string )
        {
            bufId = g_StringsOfBuffers[i].BufferId;
            bufSz = g_StringsOfBuffers[i].BufferSz;

            return;
        }
    }
}

typedef void (*callback)(tsExtBufType<mfxVideoParam>*);

void set_chromaformat(mfxFrameInfo& frameinfo)
{
    const mfxU32& FourCC = frameinfo.FourCC;
    mfxU16& ChromaFormat = frameinfo.ChromaFormat;

    switch( FourCC ) {
        case MFX_FOURCC_NV12      :
        case MFX_FOURCC_YV12      :
            ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            break;
        case MFX_FOURCC_NV16      :
        case MFX_FOURCC_YUY2      :
            ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            break;
        case MFX_FOURCC_RGB3      :
        case MFX_FOURCC_RGB4      :
            ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            break;
        case MFX_FOURCC_P8        :
        case MFX_FOURCC_P8_TEXTURE:
            ChromaFormat = 0;
            break;
        case MFX_FOURCC_P010      :
            ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            break;
        case MFX_FOURCC_P210      :
            ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            break;
        case MFX_FOURCC_BGR4      :
            ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            break;
        case MFX_FOURCC_A2RGB10   :
        case MFX_FOURCC_ARGB16    :
        case MFX_FOURCC_ABGR16    :
        case MFX_FOURCC_R16       :
        case MFX_FOURCC_AYUV      :
        case MFX_FOURCC_AYUV_RGB4 :
            ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            break;
        case MFX_FOURCC_UYVY      :
            ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            break;
        default:
            ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            break;
    }
}

void set_chromaformat_mfx(tsExtBufType<mfxVideoParam>* p)
{
    return set_chromaformat(p->mfx.FrameInfo);
}

void set_chromaformat_vpp(tsExtBufType<mfxVideoParam>* p)
{
    set_chromaformat(p->vpp.In);
    return set_chromaformat(p->vpp.Out);
}

class TestSuite : public tsSession
{
public:
    TestSuite() {};
    ~TestSuite() {};
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    enum
    {
        INIT      = 1,
        RT_CTRL   = 1 << 2,
        RT_BS     = 1 << 3
    };

    struct f_pair
    {
        const  tsStruct::Field* f;
        mfxU64 v;
        bool   isInvalid; //if true then check that this field was zero-ed by Query function
        mfxU8  stage;
    };

private:
    enum
    {
        CORE          = 1,
        DECODE        = 1 << 2,
        VPP           = 1 << 3,
        ENCODE        = 1 << 4,
        DECODE_QUERY1 = 1 << 5,
        VPP_QUERY1    = 1 << 6,
        ENCODE_QUERY1 = 1 << 7,
        DECODE_RT     = 1 << 8,
        VPP_RT        = 1 << 9,
        ENCODE_RT     = 1 << 10
    };

    typedef enum
    {
        NONE     = MFX_ERR_NONE,
        E_UNSPRT = MFX_ERR_UNSUPPORTED,
        E_INVLID = MFX_ERR_INVALID_VIDEO_PARAM,
        E_INCOMP = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        W_PARACL = MFX_WRN_PARTIAL_ACCELERATION,
        W_INCOMP = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
    } shortSts;

    struct tc_struct
    {
        //string Descr;
        //string Tag;
        mfxU32 component;
        mfxU32 codec;
        union {
            shortSts  QuerySts;
            mfxStatus query_sts;
            shortSts  RuntimeSts;
            mfxStatus rt_Sts;
        };
        union {
            shortSts  InitSts;
            mfxStatus init_sts;
        };
        f_pair set_par[MAX_NPARS]; //main test parameters to be set
        callback func;             //additional parameters that needed to be set as a dependency to main, e.g. bitrate parameters for RateControl tests
    };
    template <typename T>
    int RunTestQueryInitComponent(const tc_struct& tc);

    template <typename T>
    int RunTestQueryMode1(const tc_struct& tc);

    int RunTestEncodeRuntime(const tc_struct& tc);

    static const tc_struct test_case[];
};

static const tsStruct::Field* AsyncDepth   (&tsStruct::mfxVideoParam.AsyncDepth);
static const tsStruct::Field* Protected    (&tsStruct::mfxVideoParam.Protected);
static const tsStruct::Field* CodecId      (&tsStruct::mfxVideoParam.mfx.CodecId);
static const tsStruct::Field* CodecProfile (&tsStruct::mfxVideoParam.mfx.CodecProfile);
static const tsStruct::Field* CodecLevel   (&tsStruct::mfxVideoParam.mfx.CodecLevel);
static const tsStruct::Field* RateCtrlMthd (&tsStruct::mfxVideoParam.mfx.RateControlMethod);
static const tsStruct::Field* Width        (&tsStruct::mfxVideoParam.mfx.FrameInfo.Width);
static const tsStruct::Field* Height       (&tsStruct::mfxVideoParam.mfx.FrameInfo.Height);
static const tsStruct::Field* mfxFourCC    (&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC);
static const tsStruct::Field* inFourCC     (&tsStruct::mfxVideoParam.vpp.In.FourCC);
static const tsStruct::Field* outFourCC    (&tsStruct::mfxVideoParam.vpp.Out.FourCC);
static const tsStruct::Field* ChromaFormat (&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat);

#if defined(_WIN32) || defined(_WIN64)
const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/**/ 0},
};

#elif (defined(LINUX32) || defined(LINUX64))
const TestSuite::tc_struct TestSuite::test_case[] =
{//  id   component     codec          Query()   Init()    Parameters to set                            Additional parameters to set
//Codecs
    {/*00*/ DECODE, MFX_CODEC_HEVC,    E_UNSPRT, E_INVLID },
    {/*01*/ DECODE, MFX_CODEC_CAPTURE, E_UNSPRT, E_INVLID },
    {/*02*/ DECODE, MFX_CODEC_VP8,     E_UNSPRT, E_INVLID },
    {/*03*/ ENCODE, MFX_CODEC_HEVC,    E_UNSPRT, E_INVLID },
    {/*04*/ ENCODE, MFX_CODEC_CAPTURE, E_UNSPRT, E_INVLID },
    {/*05*/ ENCODE, MFX_CODEC_VP8,     E_UNSPRT, E_INVLID },
    {/*06*/ ENCODE, MFX_CODEC_VC1,     E_UNSPRT, E_INVLID },
//Protected content
    {/*07*/ DECODE, MFX_CODEC_AVC,    E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*08*/ DECODE, MFX_CODEC_MPEG2,  E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*09*/ DECODE, MFX_CODEC_VC1,    E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*10*/ DECODE, MFX_CODEC_JPEG,   E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*11*/ VPP,    0,                E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*12*/ ENCODE, MFX_CODEC_AVC,    E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*13*/ ENCODE, MFX_CODEC_MPEG2,  E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*14*/ ENCODE, MFX_CODEC_JPEG,   E_UNSPRT, E_INVLID, {Protected, 1, true} },
//RateControlMethod
    //AVC
    //suppported
    {/*15*/ ENCODE, MFX_CODEC_AVC, NONE, NONE, {RateCtrlMthd, MFX_RATECONTROL_CBR      }, set_brc_params },
    {/*16*/ ENCODE, MFX_CODEC_AVC, NONE, NONE, {RateCtrlMthd, MFX_RATECONTROL_VBR      }, set_brc_params },
    {/*17*/ ENCODE, MFX_CODEC_AVC, NONE, NONE, {RateCtrlMthd, MFX_RATECONTROL_CQP      }, set_brc_params },
    {/*18*/ ENCODE, MFX_CODEC_AVC, NONE, NONE, {RateCtrlMthd, MFX_RATECONTROL_LA       }, set_brc_params },
    {/*19*/ ENCODE, MFX_CODEC_AVC, NONE, NONE, {{RateCtrlMthd, MFX_RATECONTROL_LA_HRD},
                                                {&tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY},
                                                {&tsStruct::mfxExtCodingOption2.BRefType, MFX_B_REF_OFF} },set_brc_params },
    //support forced by ashapore
    {/*20*/ ENCODE, MFX_CODEC_AVC, NONE, NONE, {RateCtrlMthd, MFX_RATECONTROL_LA_ICQ   }, set_brc_params },
    {/*21*/ ENCODE, MFX_CODEC_AVC, NONE, NONE, {{RateCtrlMthd, MFX_RATECONTROL_LA_EXT   },
                                                {&tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY},
                                                {&tsStruct::mfxExtCodingOption2.BRefType, MFX_B_REF_OFF} },set_brc_params },
    //Any other is unsupported
    {/*22*/ ENCODE, MFX_CODEC_AVC, W_INCOMP, W_INCOMP, {RateCtrlMthd, 0xFFFFFFFF               , true}, },
    {/*23*/ ENCODE, MFX_CODEC_AVC, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_AVBR     , true}, set_brc_params },
    {/*24*/ ENCODE, MFX_CODEC_AVC, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_ICQ      , true}, set_brc_params },
    {/*25*/ ENCODE, MFX_CODEC_AVC, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_VCM      , true}, set_brc_params },
    {/*26*/ ENCODE, MFX_CODEC_AVC, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_QVBR     , true}, set_brc_params },
    {/*27*/ ENCODE, MFX_CODEC_AVC, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_VME      , true}, set_brc_params },
    //MPEG2
    //suppported
    {/*28*/ ENCODE, MFX_CODEC_MPEG2, NONE, NONE, {RateCtrlMthd, MFX_RATECONTROL_CBR      }, set_brc_params },
    {/*29*/ ENCODE, MFX_CODEC_MPEG2, NONE, NONE, {RateCtrlMthd, MFX_RATECONTROL_VBR      }, set_brc_params },
    {/*30*/ ENCODE, MFX_CODEC_MPEG2, NONE, NONE, {RateCtrlMthd, MFX_RATECONTROL_CQP      }, set_brc_params },
    //supported (officially not but hard to turn off in component) /*AVBR via CBR*/
    {/*31*/ ENCODE, MFX_CODEC_MPEG2, NONE, W_INCOMP, {RateCtrlMthd, MFX_RATECONTROL_AVBR}, set_brc_params },
    //unsupported
    {/*32*/ ENCODE, MFX_CODEC_MPEG2, W_INCOMP, E_INCOMP, {RateCtrlMthd, 0xFFFFFFFF               , true} },
    {/*33*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_LA       , true}, set_brc_params },
    {/*34*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_LA_HRD   , true}, set_brc_params },
    {/*35*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_ICQ      , true}, set_brc_params },
    {/*36*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_VCM      , true}, set_brc_params },
    {/*37*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_LA_ICQ   , true}, set_brc_params },
    {/*38*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_LA_EXT   , true}, set_brc_params },
    {/*39*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_QVBR     , true}, set_brc_params },
    {/*40*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_VME      , true}, set_brc_params },

//VPP MVC, replace greyed-out behavior_vpp_multi_view test on linux
    {/*41*/ VPP, 0, E_UNSPRT, E_INVLID, {&tsStruct::mfxExtMVCSeqDesc.NumView, 2 }, },

//API 1.17
    //API 1.17 unsupported
        //MFX_FOURCC_ABGR16
    {/*42*/ DECODE|ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_ABGR16, true}, set_chromaformat_mfx },
    {/*43*/ DECODE|ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_ABGR16, true}, set_chromaformat_mfx },
    {/*44*/ VPP,           0,               E_UNSPRT, E_INVLID, { {inFourCC, MFX_FOURCC_ABGR16, true}, {outFourCC, MFX_FOURCC_ABGR16, true} }, set_chromaformat_vpp },
        //MFX_FOURCC_AYUV
    {/*45*/ DECODE|ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_AYUV, true}, set_chromaformat_mfx },
    {/*46*/ DECODE|ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_AYUV, true}, set_chromaformat_mfx },
    {/*47*/ VPP,           0,               E_UNSPRT, E_INVLID, { {inFourCC, MFX_FOURCC_AYUV, true}, {outFourCC, MFX_FOURCC_AYUV, true} }, set_chromaformat_vpp },
        //MFX_FOURCC_AYUV_RGB4
    {/*48*/ DECODE|ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_AYUV_RGB4, true}, set_chromaformat_mfx },
    {/*49*/ DECODE|ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_AYUV_RGB4, true}, set_chromaformat_mfx },
    {/*50*/ VPP,           0,               E_UNSPRT, E_INVLID, { {inFourCC, MFX_FOURCC_AYUV_RGB4, true}, {outFourCC, MFX_FOURCC_AYUV_RGB4, true} }, set_chromaformat_vpp },
        //MFX_FOURCC_UYVY
    {/*51*/ DECODE|ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_UYVY, true}, set_chromaformat_mfx },
    {/*52*/ DECODE|ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_UYVY, true}, set_chromaformat_mfx },
    {/*53*/ VPP,           0,               E_UNSPRT, E_INVLID, { {inFourCC, MFX_FOURCC_UYVY, true}, {outFourCC, MFX_FOURCC_UYVY, true} }, set_chromaformat_vpp },
    {/*54*/ ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtCodingOption3.FadeDetection, MFX_CODINGOPTION_ON, true }, },
        //TODO:
        //mfxVideoParam::AllocId
        //MFX_MEMTYPE_EXPORT_FRAME   = 0x0008
        //MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET = 0x1000
    //VPS structure is missing in 1.17 headers so that thess tests are commented out
    //{/*00*/ ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, { {&tsStruct::mfxExtCodingOptionVPS.VPSId,      0xFF },
    //                                                       {&tsStruct::mfxExtCodingOptionVPS.VPSBuffer,  (mfxU64) &g_StringsOfBuffers },
    //                                                       {&tsStruct::mfxExtCodingOptionVPS.VPSBufSize, (mfxU64) sizeof(g_StringsOfBuffers) } }},
    //{/*00*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, { {&tsStruct::mfxExtCodingOptionVPS.VPSId,      0xFF },
    //                                                       {&tsStruct::mfxExtCodingOptionVPS.VPSBuffer,  (mfxU64) &g_StringsOfBuffers },
    //                                                       {&tsStruct::mfxExtCodingOptionVPS.VPSBufSize, (mfxU64) sizeof(g_StringsOfBuffers) } }},

    {/*55*/ VPP, 0, E_UNSPRT, E_INVLID, {&tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_90  }, },
    {/*56*/ VPP, 0, E_UNSPRT, E_INVLID, {&tsStruct::mfxExtVPPRotation.Angle, MFX_ANGLE_180 }, },
        //WeightedPred, WeightedBiPred, IntRefCycleDist
    {/*57*/ ENCODE, MFX_CODEC_AVC,   NONE,     NONE,     {&tsStruct::mfxExtCodingOption3.WeightedPred,   MFX_WEIGHTED_PRED_DEFAULT }, },
    {/*58*/ ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtCodingOption3.WeightedPred,   MFX_WEIGHTED_PRED_EXPLICIT, true }, },
    {/*59*/ ENCODE, MFX_CODEC_AVC,   W_INCOMP, W_INCOMP, {&tsStruct::mfxExtCodingOption3.WeightedPred,   MFX_WEIGHTED_PRED_IMPLICIT, true }, },
    {/*60*/ ENCODE, MFX_CODEC_AVC,   NONE,     NONE,     {&tsStruct::mfxExtCodingOption3.WeightedBiPred, MFX_WEIGHTED_PRED_DEFAULT }, },
    {/*61*/ ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtCodingOption3.WeightedBiPred, MFX_WEIGHTED_PRED_EXPLICIT, true }, },
    {/*62*/ ENCODE, MFX_CODEC_AVC,   NONE,     NONE,     {&tsStruct::mfxExtCodingOption3.WeightedBiPred, MFX_WEIGHTED_PRED_IMPLICIT }, },
    {/*71*/ ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {  {&tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
                                                            {&tsStruct::mfxExtCodingOption3.PRefType, MFX_P_REF_SIMPLE, true } }, },
    {/*72*/ ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {  {&tsStruct::mfxVideoParam.mfx.GopRefDist, 1 },
                                                            {&tsStruct::mfxExtCodingOption3.PRefType, MFX_P_REF_PYRAMID, true } }, },
    {/*73*/ ENCODE_QUERY1, MFX_CODEC_AVC,   E_UNSPRT, NONE, {&tsStruct::mfxExtEncoderCapability.MBPerSec, 0 }, },
    //API 1.17 supported
        //TODO:
        //mfxExtFeiCodingOption (DisableHME in FEI header)
        //MFX_MEMTYPE_EXPORT_FRAME   = 0x0008
        //MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET = 0x1000
    {/*74*/ VPP, 0, NONE, NONE, {&tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB }, },
    {/*75*/ VPP, 0, NONE, NONE, {&tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_ADVANCED_NOREF }, },
    {/*76*/ VPP, 0, NONE, NONE, {&tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_ADVANCED }, },
    {/*77*/ VPP, 0, E_UNSPRT, E_INVLID, {&tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_ADVANCED_SCD }, },
    {/*78*/ VPP, 0, E_UNSPRT, E_INVLID, {&tsStruct::mfxExtVPPDeinterlacing.Mode, 0xFFFF, true }, },
    //PTIR modes in VPP are covered by corresponding case
//AVC ROI, DirtyRect, MoveRect
    {/*79*/ ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtEncoderROI.NumROI, 1} },
    {/*80*/ ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtDirtyRect.NumRect, 1} },
    {/*81*/ ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtMoveRect.NumRect,  1} },

    //runtime tests
    {/*82*/ ENCODE_RT, MFX_CODEC_AVC, W_INCOMP, NONE, {&tsStruct::mfxExtEncoderROI.NumROI, 5, false, RT_CTRL} },
    {/*83*/ ENCODE_RT, MFX_CODEC_AVC, W_INCOMP, NONE, {&tsStruct::mfxExtDirtyRect.NumRect, 5, false, RT_CTRL} },
    {/*84*/ ENCODE_RT, MFX_CODEC_AVC, W_INCOMP, NONE, {&tsStruct::mfxExtMoveRect.NumRect,  5, false, RT_CTRL} },
    {/*85*/ ENCODE_RT, MFX_CODEC_AVC, NONE, NONE, {&tsStruct::mfxExtAVCEncodedFrameInfo.QP,  1, false, RT_CTRL} },
    {/*86*/ ENCODE_RT, MFX_CODEC_AVC, NONE, NONE, {&tsStruct::mfxExtAVCEncodedFrameInfo.MAD, 1, false, RT_CTRL} },
};
#endif //

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

template <typename TB>
void SetParams(tsExtBufType<TB>& par, const TestSuite::f_pair pairs[], const size_t length = MAX_NPARS)
{
    for(size_t i(0); i < length; ++i)
    {
        if(pairs[i].f)
        {
            void* ptr = 0;


            if((typeid(TB) == typeid(mfxVideoParam) && ( pairs[i].f->name.find("mfxVideoParam") != std::string::npos) ) || 
               (typeid(TB) == typeid(mfxBitstream)  && ( pairs[i].f->name.find("mfxBitstream")  != std::string::npos) ) || 
               (typeid(TB) == typeid(mfxEncodeCtrl) && ( pairs[i].f->name.find("mfxEncodeCtrl") != std::string::npos) ))
            {
                ptr = (TB*) &par;
            }
            else
            {
                mfxU32 bufId = 0, bufSz = 0;
                GetBufferIdSz(pairs[i].f->name, bufId, bufSz);
                if(0 == bufId + bufSz)
                {
                    EXPECT_NE((mfxU32)0, bufId + bufSz) << "ERROR: (in test) failed to get Ext buffer Id or Size\n";
                    throw tsFAIL;
                }
                ptr = par.GetExtBuffer(bufId);
                if(!ptr)
                {
                    par.AddExtBuffer(bufId, bufSz);
                    ptr = par.GetExtBuffer(bufId);
                }
            }

            tsStruct::set(ptr, *pairs[i].f, pairs[i].v);
        }
    }
}

template <typename TB>
void CheckParams(tsExtBufType<TB>& par, const TestSuite::f_pair pairs[], const size_t length = MAX_NPARS)
{
    for(size_t i(0); i < length; ++i)
    {
        if(pairs[i].f && pairs[i].isInvalid)
        {
            void* ptr = 0;

            if((typeid(TB) == typeid(mfxVideoParam) && ( pairs[i].f->name.find("mfxVideoParam") != std::string::npos) ) || 
               (typeid(TB) == typeid(mfxBitstream)  && ( pairs[i].f->name.find("mfxBitstream")  != std::string::npos) ) || 
               (typeid(TB) == typeid(mfxEncodeCtrl) && ( pairs[i].f->name.find("mfxEncodeCtrl") != std::string::npos) ))
            {
                ptr = (TB*) &par;
            }
            else
            {
                mfxU32 bufId = 0, bufSz = 0;
                GetBufferIdSz(pairs[i].f->name, bufId, bufSz);
                if(0 == bufId + bufSz)
                {
                    EXPECT_NE((mfxU32)0, bufId + bufSz) << "ERROR: (in test) failed to get Ext buffer Id or Size\n";
                    throw tsFAIL;
                }
                ptr = par.GetExtBuffer(bufId);
                if(!ptr)
                {
                    EXPECT_NE(nullptr, ptr) << "ERROR: extended buffer is missing!\n";
                    throw tsFAIL;
                }
            }

            tsStruct::check_ne(ptr, *pairs[i].f, pairs[i].v);
        }
    }
}

template <typename T>
int TestSuite::RunTestQueryInitComponent(const tc_struct& tc)
{
    T component;
    component.m_default = false;
    tsExtBufType<mfxVideoParam> queryParOut;
    tsExtBufType<mfxVideoParam>& queryParIn = component.m_par;
    if(typeid(T) != typeid(tsVideoVPP))
    {
        queryParIn.mfx.CodecId = tc.codec;
        if(MFX_CODEC_AVC == tc.codec) {
            queryParIn.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
            queryParIn.mfx.CodecLevel = MFX_LEVEL_AVC_41;
        } else if (MFX_CODEC_MPEG2 == tc.codec) {
            queryParIn.mfx.CodecProfile = MFX_PROFILE_MPEG2_SIMPLE;
            queryParIn.mfx.CodecLevel = MFX_LEVEL_MPEG2_LOW;
        } else if (MFX_CODEC_HEVC == tc.codec) {
            queryParIn.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
            queryParIn.mfx.CodecLevel = MFX_LEVEL_HEVC_41;
        } else if (MFX_CODEC_VC1 == tc.codec) {
            queryParIn.mfx.CodecProfile = MFX_LEVEL_VC1_MEDIAN;
            queryParIn.mfx.CodecLevel = MFX_LEVEL_VC1_0;
        }
    }
    SetParams(queryParIn, tc.set_par);
    if(tc.func)
    {
        tc.func(&queryParIn);
    }
    const tsExtBufType<mfxVideoParam> tmpPar = queryParIn;
    queryParOut = tmpPar;

    component.MFXInit();

    //Query(session, in, out) test:
    g_tsStatus.expect(tc.query_sts);
    component.Query(component.m_session, &queryParIn, &queryParOut);
    EXPECT_EQ(tmpPar, queryParIn) << "ERROR: Component should not change input structure in Query()!\n";

    //Check that bad parameters were zero-ed or corrected
    CheckParams(queryParOut, tc.set_par);

    //restore parameters, just in case if anything was changed by component
    queryParIn = tmpPar;

    //Query(session, in, in) test:
    g_tsStatus.expect(tc.query_sts);
    component.Query(component.m_session, &queryParIn, &queryParIn);

    //Check that bad parameters were zero-ed or corrected
    CheckParams(queryParIn, tc.set_par);

    //restore parameters, just in case if anything was changed by component
    queryParIn = tmpPar;

    //Init(session, in) test
    g_tsStatus.expect(tc.init_sts);
    component.Init(component.m_session, &queryParIn);
    EXPECT_EQ(tmpPar, queryParIn) << "ERROR: Component should not change input structure in Init()!\n";

    if( tc.init_sts >= MFX_ERR_NONE )
    {
        g_tsStatus.expect( MFX_ERR_NONE );
        component.GetVideoParam(component.m_session, &queryParOut);

        //Check that bad parameters were zero-ed or corrected by component through GetVideoParam
        CheckParams(queryParOut, tc.set_par);

        component.Close();
    }

    component.MFXClose();

    return 0;
}

template <typename T>
int TestSuite::RunTestQueryMode1(const tc_struct& tc)
{
    T component;
    component.m_default = false;
    tsExtBufType<mfxVideoParam>& queryParOut = component.m_par;
    if(typeid(T) != typeid(tsVideoVPP))
    {
        queryParOut.mfx.CodecId = tc.codec;
    }
    SetParams(queryParOut, tc.set_par);
    if(tc.func)
    {
        tc.func(&queryParOut);
    }
    const tsExtBufType<mfxVideoParam> tmpPar = queryParOut;

    component.MFXInit();

    //Query(session, 0, out) test:
    g_tsStatus.expect(tc.query_sts);
    component.Query(component.m_session, 0, &queryParOut);

    //Check that bad parameters were zero-ed or corrected
    CheckParams(queryParOut, tc.set_par);

    component.MFXClose();

    return 0;
}

int TestSuite::RunTestEncodeRuntime(const tc_struct& tc)
{
    tsVideoEncoder encoder(tc.codec, true);

    for(size_t i(0); i < MAX_NPARS; ++i)
    {
        if(tc.set_par[i].f)
        {
            if(tc.set_par[i].stage & INIT)
                SetParams(encoder.m_par, &(tc.set_par[i]), 1);
            if(tc.set_par[i].stage & RT_CTRL)
                SetParams(encoder.m_ctrl, &(tc.set_par[i]), 1);
            if(tc.set_par[i].stage & RT_BS)
                SetParams(encoder.m_bitstream, &(tc.set_par[i]), 1);
        }
    }

    if(tc.rt_Sts == MFX_ERR_MORE_DATA)
    {
        g_tsStatus.expect(tc.rt_Sts);
        encoder.EncodeFrameAsync();
    }
    else
    {
        for(mfxU32 i(0); i < 100; ++i)
        {
            for(size_t i(0); i < MAX_NPARS; ++i)
            {
                if(tc.set_par[i].f)
                {
                    if(tc.set_par[i].stage & RT_CTRL)
                        SetParams(encoder.m_ctrl, &(tc.set_par[i]), 1);
                    if(tc.set_par[i].stage & RT_BS)
                        SetParams(encoder.m_bitstream, &(tc.set_par[i]), 1);
                }
            }
            if(MFX_ERR_MORE_DATA != encoder.EncodeFrameAsync())
                break;
        }
        g_tsStatus.expect(tc.rt_Sts);
        g_tsStatus.check();
    }

    return 0;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    if(0 == tc.component)
    {
        EXPECT_NE((mfxU32)0, tc.component) << "ERROR: test is not implemented\n";
        throw tsFAIL;
    }

    if(DECODE & tc.component)
        RunTestQueryInitComponent<tsVideoDecoder>(tc);

    if(ENCODE & tc.component)
        RunTestQueryInitComponent<tsVideoEncoder>(tc);

    if(VPP & tc.component)
        RunTestQueryInitComponent<tsVideoVPP>(tc);

    if(DECODE_QUERY1 & tc.component)
        RunTestQueryMode1<tsVideoDecoder>(tc);

    if(ENCODE_QUERY1 & tc.component)
        RunTestQueryMode1<tsVideoEncoder>(tc);

    if(VPP_QUERY1 & tc.component)
        RunTestQueryMode1<tsVideoVPP>(tc);

    if(ENCODE_RT & tc.component)
        RunTestEncodeRuntime(tc);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(TEST_NAME);
#undef TEST_NAME

}
