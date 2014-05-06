#include "ts_decoder.h"
#include "ts_struct.h"
#include "mfxvp8.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace vp8d_init
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

void ext_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 size)
{
    par.AddExtBuffer(id, size); 
}

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_VP8) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 10;
    
    enum
    {
        NULL_SESSION = 1,
        NULL_PARAMS,
        NEED_ALLOC,
        ALLOC_OPAQUE,
        ALLOC_OPAQUE_LESS,
        ALLOC_OPAQUE_MORE
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
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
    {/* 0*/ MFX_ERR_INVALID_HANDLE, TestSuite::NULL_SESSION},
    {/* 1*/ MFX_ERR_NULL_PTR, TestSuite::NULL_PARAMS},

    // FourCC cases (only NV12 supported)
    {/* 2*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_NV12}}},
    {/* 3*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YV12}}},
    {/* 4*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_P8}}},
    {/* 5*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_RGB4}}},
    {/* 6*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_YUY2}}},
    {/* 7*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_P8_TEXTURE}}},

    // ChromaFormat cases (only 420 supported)
    {/* 8*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV420}}},
    {/* 9*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME}}},
    {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422}}},
    {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
    {/*12*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV400}}},
    {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV411}}},
    {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422H}}},
    {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV422V}}},
     
    // IOPattern cases
    {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}}},
    {/*17*/ MFX_ERR_NONE, TestSuite::NEED_ALLOC, {{&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY}}},
    {/*18*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY}}},
    {/*19*/ MFX_ERR_NONE, TestSuite::ALLOC_OPAQUE, {{&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}},
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}},
    {/*21*/ MFX_ERR_NONE, TestSuite::ALLOC_OPAQUE_LESS, {{&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}},
    {/*22*/ MFX_ERR_NONE, TestSuite::ALLOC_OPAQUE_MORE, {{&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}},

    // Codec profiles (1 to 4 supported)
    {/*23*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_VP8_0}}},
    {/*24*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_VP8_1}}},
    {/*25*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_VP8_2}}},
    {/*26*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_VP8_3}}},
    {/*27*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.CodecProfile, 5}}},

    // Codec levels
    {/*28*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.CodecLevel, 0}}},
    {/*29*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.CodecLevel, 5}}},

    // Width/height
    {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 0}}},
    {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 0}}},
    {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 65}}},
    {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 65}}},
    {/*34*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0}, 
                             {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0}}},
    {/*35*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0}, 
                             {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 0}, 
                             {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 64}, 
                             {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 64}}},
    {/*36*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 42}, 
                                            {&tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 33}}},
    
    {/*37*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_UNKNOWN}}},
    {/*38*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*39*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF}}},
    {/*40*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF}}},
    {/*41*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_REPEATED}}},
    {/*42*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FRAME_DOUBLING}}},
    {/*43*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FRAME_TRIPLING}}},
    {/*44*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE|MFX_PICSTRUCT_FIELD_TFF}}},

    // Async_depth
    {/*45*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.AsyncDepth, 1}}},
    {/*46*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.AsyncDepth, 10}}},

    // Ext Buffers
    {/*47*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtCodingOptionVP8        )}},
    {/*48*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtCodingOption           )}},
    {/*49*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtCodingOptionSPSPPS     )}},
    {/*50*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDoNotUse            )}},
    {/*51*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVppAuxData             )}},
    {/*52*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDenoise             )}},
    {/*53*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPProcAmp             )}},
    {/*54*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDetail              )}},
    {/*55*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVideoSignalInfo        )}},
    {/*56*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse               )}},
    {/*57*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtAVCRefListCtrl         )}},
    {/*58*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion )}},
    {/*59*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtPictureTimingSEI       )}},
    {/*60*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtAvcTemporalLayers      )}},
    {/*61*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtCodingOption2          )}},
    {/*62*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPImageStab           )}},
    {/*63*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtEncoderCapability      )}},
    {/*64*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtEncoderResetOption     )}},
    {/*65*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtAVCEncodedFrameInfo    )}},
    {/*66*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPComposite           )}},
    {/*67*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPVideoSignalInfo     )}},
    {/*68*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtEncoderROI             )}},
    {/*69*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDeinterlacing       )}},
    {/*70*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtMVCSeqDesc             )}},
    {/*71*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtMVCTargetViews         )}},
    {/*72*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtJPEGQuantTables        )}},
    {/*73*/  MFX_ERR_INVALID_VIDEO_PARAM, 0, {}, {ext_buf, EXT_BUF_PAR(mfxExtJPEGHuffmanTables      )}},

    // Max width/height
    {/*74*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 16384}}},
    {/*75*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 16384}}},
    {/*76*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 16416}}},
    {/*77*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 16416}}}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    m_par_set = true;
    const tc_struct& tc = test_case[id];

    if (tc.mode != NULL_SESSION)
    {
        MFXInit();

        if(m_uid) // load plugin
            Load();
    }

    if (tc.mode == NULL_PARAMS)
    {
        m_pPar = 0;
    }

    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
    }

    if(tc.set_buf.func) // set ExtBuffer
    {
        (*tc.set_buf.func)(this->m_par, tc.set_buf.buf, tc.set_buf.size);
    }

    UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));
    
    if (tc.mode == NEED_ALLOC)
    {    
        SetFrameAllocator(m_session, GetAllocator());
    } 

    if (m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
    {
        mfxHDL hdl;
        mfxHandleType type; 
        GetAllocator()->get_hdl(type, hdl); 
        SetHandle(m_session, type, hdl);
    }

    if (tc.mode == ALLOC_OPAQUE || tc.mode == ALLOC_OPAQUE_LESS || tc.mode == ALLOC_OPAQUE_MORE)
    {
        mfxExtOpaqueSurfaceAlloc& osa = m_par;
        QueryIOSurf();
        if (tc.mode == ALLOC_OPAQUE_LESS)
        {
            m_request.NumFrameSuggested = m_request.NumFrameMin = m_request.NumFrameMin - 1;
        }
        else if (tc.mode == ALLOC_OPAQUE_MORE)
        {
            m_request.NumFrameSuggested = m_request.NumFrameMin = m_request.NumFrameMin + 1;
        }
        AllocOpaque(m_request, osa);
    }
    
    g_tsStatus.expect(tc.sts);
    Init(m_session, m_pPar);
    
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vp8d_init);

}