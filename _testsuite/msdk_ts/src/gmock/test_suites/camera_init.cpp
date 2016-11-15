#include "ts_vpp.h"
#include "ts_struct.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace camera_init
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

void ext_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 size)
{
    par.AddExtBuffer(id, size); 
}
void del_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 /*size*/)
{
    par.RemoveExtBuffer(id); 
}

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP(true, MFX_MAKEFOURCC('C','A','M','R')) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus Init(mfxSession session, mfxVideoParam *par)
    {
        if(session && par)
        {
            if((MFX_FOURCC_A2RGB10 == par->vpp.In.FourCC) ||
               (MFX_FOURCC_ARGB16  == par->vpp.In.FourCC) ||
               (MFX_FOURCC_R16     == par->vpp.In.FourCC))
               par->vpp.In.BitDepthLuma = 10;
            if((MFX_FOURCC_A2RGB10 == par->vpp.Out.FourCC) ||
               (MFX_FOURCC_ARGB16  == par->vpp.Out.FourCC) ||
               (MFX_FOURCC_R16     == par->vpp.Out.FourCC))
               par->vpp.Out.BitDepthLuma = 10;
            if (MFX_FOURCC_NV12 == par->vpp.In.FourCC)
                par->vpp.In.BitDepthLuma = 8;
            if (MFX_FOURCC_NV12 == par->vpp.Out.FourCC)
                par->vpp.Out.BitDepthLuma = 8;
        }
        mfxVideoParam tmp_par;
        if (par)
            tmp_par = *par;
        return tsVideoVPP::Init(session, par);
    }

private:
    static const mfxU32 n_par = 8;

    enum NULLPTRS
    {
        NOTHING = 0,
        MFXVP   = 1,
        SESSION = 2,
        UID     = 3
    };

    enum ALLOC_CTRL
    {
        SET_ALLOCATOR     =  0x0100,
        ALLOC_OPAQUE      =  0x0200,
    };

    enum AllocMode
    {
        ALLOC_MIN_MINUS_1,
        ALLOC_MIN,
        ALLOC_AVG,
        ALLOC_MAX,
        ALLOC_LOTS
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 zero_ptr;
        mfxU32 num_calls;
        mfxU32 alloc_ctrl;
        struct f_pair 
        {
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
    ////Zero ptr cases
    {/* 0*/ MFX_ERR_INVALID_HANDLE, SESSION, 1 },
    {/* 1*/ MFX_ERR_NULL_PTR, MFXVP, 1 },
    // FourCC cases (only MFX_FOURCC_R16 at IN and (MFX_FOURCC_RGB4 || MFX_FOURCC_ARGB16 || MFX_FOURCC_NV12) at OUT are supported, 0 is invalid value)
    {/* 2*/ MFX_ERR_NONE, 0, 3, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_R16 },
    { &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 } } },
    {/* 3*/ MFX_ERR_NONE, 0, 3, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_R16 },
    { &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_ARGB16 } } },
    {/* 4*/ MFX_ERR_NONE, 0, 3, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_R16 },
    { &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12 } } },
    {/* 5*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 3, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FourCC, 0 },
    { &tsStruct::mfxVideoParam.vpp.Out.FourCC, 0 } } },
    {/* 6*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_NV12 },
    { &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12 } } },
    {/* 7*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_NV12 },
    { &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YV12 } } },
    {/* 8*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_NV12 },
    { &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 } } },
    {/* 9*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YV12 },
    { &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YV12 } } },
    {/* 10*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P8 },
    { &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P8 } } },
    {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4 },
    { &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_RGB4 } } },
    {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YUY2 },
    { &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_YUY2 } } },
    {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P8_TEXTURE },
    { &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P8_TEXTURE } } },
    // ChromaFormat cases, only 444 for RGB formats
    {/*14*/ MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME },
    { &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV444 } } },
    {/*15*/ MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME },
    { &tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_NV12 },
    { &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV420 } } },
    {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV420 },
    { &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV420 } } },
    {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME },
    { &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME } } },
    {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV422 },
    { &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV422 } } },
    {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444 },
    { &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV444 } } },
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV400 },
    { &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV400 } } },
    {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV411 },
    { &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV411 } } },
    {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV422H },
    { &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV422H } } },
    {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV422V },
    { &tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV422V } } },
    //Width, Height, Crops, AR, FR
    {/*24*/ MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.Width, 1920 },
    { &tsStruct::mfxVideoParam.vpp.In.Height, 1088 },
    { &tsStruct::mfxVideoParam.vpp.Out.Width, 1920 },
    { &tsStruct::mfxVideoParam.vpp.Out.Height, 1088 } } },
    {/*25*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.Width, 0 },
    { &tsStruct::mfxVideoParam.vpp.In.Height, 0 },
    { &tsStruct::mfxVideoParam.vpp.Out.Width, 0 },
    { &tsStruct::mfxVideoParam.vpp.Out.Height, 0 } } },
    {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.Width, 721 },
    { &tsStruct::mfxVideoParam.vpp.In.Height, 481 },
    { &tsStruct::mfxVideoParam.vpp.Out.Width, 721 },
    { &tsStruct::mfxVideoParam.vpp.Out.Height, 481 } } },
    {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.Width, 680 },
    { &tsStruct::mfxVideoParam.vpp.In.Height, 600 },
    { &tsStruct::mfxVideoParam.vpp.Out.Width, 680 },
    { &tsStruct::mfxVideoParam.vpp.Out.Height, 600 } } },
    {/*28*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.CropX, 720 },
    { &tsStruct::mfxVideoParam.vpp.In.CropY, 720 },
    { &tsStruct::mfxVideoParam.vpp.Out.CropW, 480 },
    { &tsStruct::mfxVideoParam.vpp.Out.CropH, 480 } } },
    {/*29*/ MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.AspectRatioW, 666 },  //ARs should be ignored during Init
    { &tsStruct::mfxVideoParam.vpp.In.AspectRatioH, 555 },
    { &tsStruct::mfxVideoParam.vpp.Out.AspectRatioW, 777 },
    { &tsStruct::mfxVideoParam.vpp.Out.AspectRatioH, 888 } } },
    {/*30*/ MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30 },  //Frame rate should be
    { &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1 },   //any valid value
    { &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30 },
    { &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1 } } },
    {/*31*/ MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60 },
    { &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 2 },
    { &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60 },
    { &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 2 } } },
    {/*32*/ MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 240 },
    { &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 10 },
    { &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 240 },
    { &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 10 } } },
    {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30 },
    { &tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 0 },
    { &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30 },
    { &tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 0 } } },
    {/*34*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.Width, 4096 }, //resize is unsupported
    { &tsStruct::mfxVideoParam.vpp.In.Height, 4096 },
    { &tsStruct::mfxVideoParam.vpp.Out.Width, 2048 },
    { &tsStruct::mfxVideoParam.vpp.Out.Height, 2048 },
    { &tsStruct::mfxVideoParam.vpp.In.CropW, 4096 },
    { &tsStruct::mfxVideoParam.vpp.In.CropH, 4096 },
    { &tsStruct::mfxVideoParam.vpp.Out.CropW, 2048 },
    { &tsStruct::mfxVideoParam.vpp.Out.CropH, 2048 } } },
    {/*35*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { { &tsStruct::mfxVideoParam.vpp.In.Width, 2048 },
    { &tsStruct::mfxVideoParam.vpp.In.Height, 2048 },
    { &tsStruct::mfxVideoParam.vpp.Out.Width, 4096 },
    { &tsStruct::mfxVideoParam.vpp.Out.Height, 4096 },
    { &tsStruct::mfxVideoParam.vpp.In.CropW, 2048 },
    { &tsStruct::mfxVideoParam.vpp.In.CropH, 2048 },
    { &tsStruct::mfxVideoParam.vpp.Out.CropW, 4096 },
    { &tsStruct::mfxVideoParam.vpp.Out.CropH, 4096 } } },

    // IOPattern cases
    {/*36*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY } },
    {/*37*/ MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY } },
    {/*38*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY } },
    {/*39*/ MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY } },
    {/*40*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, ALLOC_OPAQUE | ALLOC_MIN, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY } },
    {/*41*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, ALLOC_OPAQUE | ALLOC_MIN, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY } },
    {/*42*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, ALLOC_OPAQUE | ALLOC_MIN, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY } },
    {/*43*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, ALLOC_OPAQUE | ALLOC_MIN, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY } },
    {/*44*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, ALLOC_OPAQUE | ALLOC_MIN, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY } },
    {/*45*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, ALLOC_OPAQUE | ALLOC_MIN_MINUS_1, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY } },
    {/*46*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, ALLOC_OPAQUE | ALLOC_LOTS, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY } },
    {/*47*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, { &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_OPAQUE_MEMORY } },
    // ext buffers
    {/*48*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtVPPDoNotUse) } },
    {/*49*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtVppAuxData) } },
    {/*50*/  MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtVPPDenoise) } },
    {/*51*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtVPPProcAmp) } },
    {/*52*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtVPPDetail) } },
    {/*53*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtVideoSignalInfo) } },
    {/*54*/  MFX_ERR_UNDEFINED_BEHAVIOR, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse) } },
    {/*55*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtAVCRefListCtrl) } },
    {/*56*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion) } },
    {/*57*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtVPPImageStab) } },
    {/*58*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtVPPComposite) } },
    {/*59*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtVPPVideoSignalInfo) } },
    {/*60*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtVPPDeinterlacing) } },
    // camera's ext buffers
    {/*61*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtCamGammaCorrection) } },
    {/*62*/  MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtCamWhiteBalance) } },
    {/*63*/  MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtCamHotPixelRemoval) } },
    {/*64*/  MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtCamBlackLevelCorrection) } },
    {/*65*/  MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtCamColorCorrection3x3) } },
    {/*66*/  MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtCamCscYuvRgb) } },
    {/*67*/  MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtCamTotalColorControl) } },
    {/*68*/  MFX_ERR_NONE, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { ext_buf, EXT_BUF_PAR(mfxExtCamPipeControl) } },
    {/*69*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, 1, SET_ALLOCATOR | ALLOC_MIN, {}, { del_buf, EXT_BUF_PAR(mfxExtCamPipeControl) } },

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    if(tc.zero_ptr == UID)
    {
        m_uid = 0;
    }

    if(tc.zero_ptr != SESSION)
    {
        MFXInit();

        if(m_uid)
        {
            Load();
        }

        m_spool_in.UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));
        SetFrameAllocator(m_session, m_spool_in.GetAllocator());
        m_spool_out.SetAllocator(m_spool_in.GetAllocator(), true);

        if (tc.alloc_ctrl & ALLOC_OPAQUE)
        {
            m_par.vpp.In.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
            mfxExtOpaqueSurfaceAlloc& osa = m_par;
            QueryIOSurf();
            if ((0x00F & tc.alloc_ctrl) == ALLOC_MIN_MINUS_1)
            {
                m_request[0].NumFrameSuggested = m_request[0].NumFrameMin = m_request[0].NumFrameMin - 1;
                m_request[1].NumFrameSuggested = m_request[1].NumFrameMin = m_request[1].NumFrameMin - 1;
            }
            else if ((0x00F & tc.alloc_ctrl) & ALLOC_LOTS)
            {
                m_request[0].NumFrameSuggested = m_request[0].NumFrameMin = m_request[0].NumFrameMin * 4;
                m_request[1].NumFrameSuggested = m_request[1].NumFrameMin = m_request[1].NumFrameMin * 4;
            }
            if (m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
                m_spool_in.AllocOpaque(m_request[0], osa);
            if (m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
                m_spool_out.AllocOpaque(m_request[1], osa);
        }
    }

    if (tc.zero_ptr == MFXVP)
    {
        m_pPar = 0;
    }
    else
    {
        for(mfxU32 i = 0; i < n_par; i++)
        {
            if(tc.set_par[i].f)
            {
                tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
                std::cout << "  Set field " << tc.set_par[i].f->name << " to " << tc.set_par[i].v << "\n";
            }
        }
        if(tc.set_buf.func) // set ExtBuffer
        {
            (*tc.set_buf.func)(this->m_par, tc.set_buf.buf, tc.set_buf.size);
        }
    }

    for(mfxU32 i = 0; i < (tc.num_calls - 1); i++)
    {
        g_tsStatus.expect(tc.sts);
        Init(m_session, m_pPar);

        if(m_initialized)
        {
            g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR);
            Init(m_session, m_pPar);
            Close();
        }
        if(tc.zero_ptr != SESSION)
        {
            g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
            Close();
        }
    }

    g_tsStatus.expect(tc.sts);
    Init(m_session, m_pPar);

    if(tc.set_buf.func) //Component should not relay on provided buffer
        m_par.RemoveExtBuffer(tc.set_buf.buf);

    if((MFX_ERR_NONE == tc.sts) || 
       (MFX_WRN_PARTIAL_ACCELERATION == tc.sts))
    {
        tsExtBufType<mfxVideoParam> parOut;
        mfxExtVPPDoUse& do_use = parOut;
        do_use.NumAlg = 5;
        mfxU32 filter_from_getvideoparam[5] = {0,};
        do_use.AlgList = filter_from_getvideoparam;

        GetVideoParam(m_session, &parOut);
        EXPECT_EQ(&do_use, (mfxExtVPPDoUse*) *parOut.ExtParam) << "Fail.: GetVideoParam should not change ptr to extBuffers\n";
        if(tc.set_buf.func)
        {
            bool algorithm_was_reported = false;
            for(mfxU32 i = 0; i < do_use.NumAlg; ++i)
            {
                if(filter_from_getvideoparam[i] == tc.set_buf.buf) algorithm_was_reported = true;

                if(MFX_EXTBUFF_VPP_DENOISE == tc.set_buf.buf && MFX_EXTBUF_CAM_BAYER_DENOISE == filter_from_getvideoparam[i] ) algorithm_was_reported = true;
            }
            EXPECT_EQ(true,algorithm_was_reported) << "Fail.: GetVideoParam should update used algoritms list in the ExtVPPDoUse\n";
        }
        EXPECT_EQ(m_par.vpp.In.Width,   parOut.vpp.In.Width);
        EXPECT_EQ(m_par.vpp.In.Height,  parOut.vpp.In.Height);
        EXPECT_EQ(m_par.vpp.Out.Width,  parOut.vpp.Out.Width);
        EXPECT_EQ(m_par.vpp.Out.Height, parOut.vpp.Out.Height);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(camera_init);

}