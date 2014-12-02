#include "ts_vpp.h"
#include "ts_struct.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace ptir_init
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

void ext_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 size)
{
    par.AddExtBuffer(id, size);
}

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP(true, MFX_MAKEFOURCC('P','T','I','R')) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 6;

    enum
    {
        MFX_PAR = 1,
        EXT_DI,
    };

    enum
    {
        NULL_SESSION = 1,
        NULL_PARAMS  = 2,

        USE_EXT_BUF = 3,

        ALLOC_OPAQUE = 4,
        ALLOC_OPAQUE_LESS = ALLOC_OPAQUE+1,
        ALLOC_OPAQUE_MORE = ALLOC_OPAQUE+2
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
        struct
        {
            callback func;
            mfxU32 buf;
            mfxU32 size;
        } set_buf;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*0*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/*1*/ MFX_ERR_NULL_PTR, NULL_PARAMS},

    // Check default parameters (see constructor)
    {/*2*/ MFX_ERR_NONE},

    // No resize
    {/*3*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 320}},
    {/*4*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 320}},
    // No crop
    {/*5*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropX, 320}},
    {/*6*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropY, 320}},
    {/*7*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropW, 320}},
    {/*8*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.CropH, 320}},

    // FourCC cases (only NV12 supported)
    {/*9*/ MFX_ERR_NONE, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_NV12}},
    {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YV12}},
    {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P8}},
    {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4}},
    {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YUY2}},
    {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P8_TEXTURE}},

    // ChromaFormat cases (only 420 supported)
    {/*15*/ MFX_ERR_NONE, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV420}},
    {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME}},
    {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV422}},
    {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444}},
    {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV400}},
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV411}},
    {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV422H}},
    {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV422V}},

    // Only PROGRESSIVE as output
    {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF}}},
    {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF}}},

    // IOPattern cases
    {/*25*/ MFX_ERR_NONE, 0, {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*26*/ MFX_ERR_NONE, 0, {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*27*/ MFX_ERR_NONE, 0, {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*28*/ MFX_ERR_NONE, 0, {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*29*/ MFX_ERR_NONE, ALLOC_OPAQUE, {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*30*/ MFX_ERR_NONE, ALLOC_OPAQUE, {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*31*/ MFX_ERR_NONE, ALLOC_OPAQUE, {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*32*/ MFX_ERR_NONE, ALLOC_OPAQUE, {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*33*/ MFX_ERR_NONE, ALLOC_OPAQUE, {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*34*/ MFX_ERR_INVALID_VIDEO_PARAM, ALLOC_OPAQUE_LESS, {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*35*/ MFX_ERR_INVALID_VIDEO_PARAM, ALLOC_OPAQUE_MORE, {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*36*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},

    // ext buffers
    {/*37*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDoNotUse            )}},
    {/*38*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVppAuxData             )}},
    {/*39*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDenoise             )}},
    {/*40*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPProcAmp             )}},
    {/*41*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDetail              )}},
    {/*42*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVideoSignalInfo        )}},
    {/*43*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse               )}},
    {/*44*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtAVCRefListCtrl         )}},
    {/*45*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion )}},
    {/*46*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPImageStab           )}},
    {/*47*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPComposite           )}},
    {/*48*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPVideoSignalInfo     )}},

    // async
    {/*49*/ MFX_ERR_NONE, 0, {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 0}},
    {/*50*/ MFX_ERR_NONE, 0, {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 1}},
    {/*51*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {MFX_PAR, &tsStruct::mfxVideoParam.AsyncDepth, 2}},

    // unsupported DI modes
    {/*52*/ MFX_ERR_INVALID_VIDEO_PARAM, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_BOB}}},
    {/*53*/ MFX_ERR_INVALID_VIDEO_PARAM, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_ADVANCED}}},

    // MFX_DEINTERLACING_AUTO_DOUBLE
    {/*54*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60}}},
    {/*55*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 2},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 120}}},
    {/*56*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60}}},
    {/*57*/ MFX_ERR_NONE, USE_EXT_BUF,
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_AUTO_DOUBLE}},
    {/*58*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 60},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_AUTO_DOUBLE}}},

    // MFX_DEINTERLACING_AUTO_SINGLE
    {/*59*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30}}},
    {/*60*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 2}}},
    {/*61*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 30}}},
    {/*62*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_AUTO_SINGLE}}},
    {/*63*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 60},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_AUTO_SINGLE}}},

    // MFX_DEINTERLACING_FULL_FR_OUT
    {/*64*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1}}},
    {/*65*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1}}},
    {/*66*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 2},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1}}},
    {/*67*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 24},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 60}}},
    {/*68*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FULL_FR_OUT}}},
    {/*69*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 24},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 60},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FULL_FR_OUT}}},

    // MFX_DEINTERLACING_HALF_FR_OUT
    {/*70*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 30},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 30}}},
    {/*71*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 30},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 30}}},
    {/*72*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 2},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 60},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 30}}},
    {/*73*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 24},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 30}}},
    {/*74*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_HALF_FR_OUT}}},
    {/*75*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 24},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 30},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_HALF_FR_OUT}}},

    // MFX_DEINTERLACING_24FPS_OUT
    {/*76*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 30},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 24}}},
    {/*77*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 30},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 24}}},
    {/*78*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 35},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 29}}},
    {/*79*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_24FPS_OUT}}},
    {/*80*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 35},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 1},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 28},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_24FPS_OUT}}},

    // MFX_DEINTERLACING_FIXED_TELECINE_PATTERN
    {/*81*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FIXED_TELECINE_PATTERN},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.TelecinePattern, MFX_TELECINE_PATTERN_32}}},
    {/*82*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FIXED_TELECINE_PATTERN},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.TelecinePattern, MFX_TELECINE_PATTERN_2332}}},
    {/*83*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FIXED_TELECINE_PATTERN},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.TelecinePattern, MFX_TELECINE_PATTERN_FRAME_REPEAT}}},
    {/*84*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FIXED_TELECINE_PATTERN},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.TelecinePattern, MFX_TELECINE_PATTERN_41}}},
    {/*85*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FIXED_TELECINE_PATTERN},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.TelecinePattern, MFX_TELECINE_POSITION_PROVIDED}}},
    {/*86*/ MFX_ERR_INVALID_VIDEO_PARAM, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FIXED_TELECINE_PATTERN},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.TelecinePattern, MFX_TELECINE_POSITION_PROVIDED+1}}},
    {/*87*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FIXED_TELECINE_PATTERN},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.TelecinePattern, MFX_TELECINE_POSITION_PROVIDED},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.TelecineLocation, 1}}},
    {/*88*/ MFX_ERR_NONE, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FIXED_TELECINE_PATTERN},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.TelecinePattern, MFX_TELECINE_POSITION_PROVIDED},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.TelecineLocation, 4}}},
    {/*89*/ MFX_ERR_INVALID_VIDEO_PARAM, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_FIXED_TELECINE_PATTERN},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.TelecinePattern, MFX_TELECINE_POSITION_PROVIDED},
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.TelecineLocation, 5}}},

    // MFX_DEINTERLACING_30FPS_OUT
    {/*90*/ MFX_ERR_INVALID_VIDEO_PARAM, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_30FPS_OUT}}},

    // MFX_DEINTERLACING_DETECT_INTERLACE
    {/*91*/ MFX_ERR_INVALID_VIDEO_PARAM, USE_EXT_BUF, {
        {EXT_DI, &tsStruct::mfxExtVPPDeinterlacing.Mode, MFX_DEINTERLACING_DETECT_INTERLACE}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

#define SETPARS(p, type)\
for(mfxU32 i = 0; i < n_par; i++) \
{ \
    if(tc.set_par[i].f && tc.set_par[i].ext_type == type) \
    { \
        tsStruct::set(p, *tc.set_par[i].f, tc.set_par[i].v); \
    } \
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    std::vector<mfxExtBuffer*> buffs;
    std::vector<mfxExtBuffer*> buffs_cpy;

    if (tc.mode != NULL_SESSION)
    {
        MFXInit();

        // always load plug-in
        mfxPluginUID* ptir = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_VPP, MFX_MAKEFOURCC('P','T','I','R'));
        tsSession::Load(m_session, ptir, 1);
    }

    if (tc.mode == USE_EXT_BUF)
    {
        m_par.vpp.In.PicStruct = 0;
        m_par.vpp.In.FrameRateExtN = 0;
        m_par.vpp.In.FrameRateExtD = 0;
        m_par.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.vpp.Out.FrameRateExtN = 0;
        m_par.vpp.Out.FrameRateExtD = 0;

        mfxExtVPPDeinterlacing di = {0};
        di.Header.BufferId = MFX_EXTBUFF_VPP_DEINTERLACING;
        di.Header.BufferSz = sizeof(mfxExtVPPDeinterlacing);
        SETPARS(&di, EXT_DI);
        buffs.push_back((mfxExtBuffer*)&di);
        mfxExtVPPDeinterlacing di_cpy(di);
        buffs_cpy.push_back((mfxExtBuffer*)&di_cpy);
    }

    // set up parameters for case
    SETPARS(m_pPar, MFX_PAR);

    if (tc.mode != NULL_SESSION)
    {
        bool isSW = !(!!(m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY));
        m_spool_in.UseDefaultAllocator(isSW);
        SetFrameAllocator(m_session, m_spool_in.GetAllocator());
        m_spool_out.SetAllocator(m_spool_in.GetAllocator(), true);

        if (!m_is_handle_set)
        {
            mfxHDL hdl;
            mfxHandleType type;
            if (isSW)
            {
                if (!m_pVAHandle)
                {
                    m_pVAHandle = new frame_allocator(
                            TS_HW_ALLOCATOR_TYPE,
                            frame_allocator::ALLOC_MAX,
                            frame_allocator::ENABLE_ALL,
                            frame_allocator::ALLOC_EMPTY);
                }
                m_pVAHandle->get_hdl(type, hdl);
            }
            else
            {
                m_spool_in.GetAllocator()->get_hdl(type, hdl);
            }
            SetHandle(m_session, type, hdl);
        }
    }

    if (tc.mode & ALLOC_OPAQUE)
    {
        mfxExtOpaqueSurfaceAlloc osa = {0};
        osa.Header.BufferId = MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION;
        osa.Header.BufferSz = sizeof(mfxExtOpaqueSurfaceAlloc);
        QueryIOSurf();
        if (tc.mode == ALLOC_OPAQUE_LESS)
        {
            m_request[0].NumFrameSuggested = m_request[0].NumFrameMin = m_request[0].NumFrameMin - 1;
            m_request[1].NumFrameSuggested = m_request[1].NumFrameMin = m_request[1].NumFrameMin - 1;
        }
        else if (tc.mode == ALLOC_OPAQUE_MORE)
        {
            m_request[0].NumFrameSuggested = m_request[0].NumFrameMin = m_request[0].NumFrameMin + 1;
            m_request[1].NumFrameSuggested = m_request[1].NumFrameMin = m_request[1].NumFrameMin + 1;
        }
        if (m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            m_spool_in.AllocOpaque(m_request[0], osa);
        if (m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
            m_spool_out.AllocOpaque(m_request[1], osa);

        mfxExtOpaqueSurfaceAlloc osa_cpy(osa);
        buffs.push_back((mfxExtBuffer*)&osa);
        buffs_cpy.push_back((mfxExtBuffer*)&osa_cpy);
    }

    // no buffers were added to m_par before
    tsExtBufType<mfxVideoParam> par_cpy(m_par);

    if (tc.set_buf.func) // set ExtBuffer
    {
        (*tc.set_buf.func)(this->m_par, tc.set_buf.buf, tc.set_buf.size);
        (*tc.set_buf.func)(par_cpy, tc.set_buf.buf, tc.set_buf.size);
    }

    // adding buffers
    if (buffs.size())
    {
        m_par.NumExtParam = par_cpy.NumExtParam = buffs.size();
        m_par.ExtParam = &buffs[0];
        par_cpy.ExtParam = &buffs_cpy[0];
    }

    if (tc.mode == NULL_PARAMS)
        m_pPar = 0;
    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

    // check Query() after Init() to avoid changing m_par
    if (tc.sts != MFX_ERR_NULL_PTR)
    {
        if (tc.sts == MFX_ERR_INVALID_VIDEO_PARAM)
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
        Query(m_session, m_pPar, &par_cpy);
    }

    g_tsStatus.expect(tc.sts);
    Init(m_session, m_pPar);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(ptir_init);

}

