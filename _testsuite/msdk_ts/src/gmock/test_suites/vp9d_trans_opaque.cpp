/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2016 Intel Corporation. All Rights Reserved.
//
*/

#include "ts_transcoder.h"

namespace vp9d_trans_opaque
{


class tsIvfReader : public tsBitstreamProcessor, public tsReader
{
    #pragma pack(push, 4)
    typedef struct
    {
        union{
            mfxU32 _signature;
            mfxU8  signature[4]; //DKIF
        };
        mfxU16 version;
        mfxU16 header_length; //bytes
        mfxU32 FourCC;        //CodecId
        mfxU16 witdh;
        mfxU16 height;
        mfxU32 frame_rate;
        mfxU32 time_scale;
        mfxU32 n_frames;
        mfxU32 unused;
    } IVF_file_header;

    typedef struct
    {
        mfxU32 frame_size; //bytes
        mfxU64 time_stamp;
    } IVF_frame_header;
    #pragma pack(pop)

    IVF_file_header file_header;
    IVF_frame_header frame_header;
    size_t count;

public:
    bool    m_eos;
    mfxU8*  m_buf;
    size_t  m_buf_size;
    mfxU8*  inter_buf;
    size_t  inter_buf_lenth;
    size_t  inter_buf_size;

    tsIvfReader(const char* fname, mfxU32 buf_size)
        : tsReader(fname)
        , count(0)
        , m_eos(false)
        , m_buf(new mfxU8[buf_size])
        , m_buf_size(buf_size)
        , inter_buf(new mfxU8[buf_size])
        , inter_buf_lenth(0)
        , inter_buf_size(buf_size)
    {
        inter_buf_lenth += Read(inter_buf, inter_buf_size);
        assert(inter_buf_lenth >= (sizeof(IVF_file_header) + sizeof(IVF_frame_header)));
        for(size_t i(0); i < inter_buf_lenth; ++i)
        {
            if(0 == strcmp((char*)(inter_buf + i), "DKIF"))
            {
                inter_buf_lenth -= i;
                memmove(inter_buf, inter_buf + i, inter_buf_lenth);
                break;
            }
        }
        IVF_file_header const * const fl_hdr = (IVF_file_header*) inter_buf;
        file_header = *fl_hdr;
        IVF_frame_header const * const fr_hdr = (IVF_frame_header*) (inter_buf + file_header.header_length);
        frame_header = *fr_hdr;

        inter_buf_lenth -= file_header.header_length + sizeof(IVF_frame_header);
        memmove(inter_buf, inter_buf + file_header.header_length + sizeof(IVF_frame_header), inter_buf_lenth);

        inter_buf_lenth += Read(inter_buf+inter_buf_lenth, (inter_buf_size - inter_buf_lenth));
    };
    virtual ~tsIvfReader()
    {
        if(m_buf)
        {
            delete[] m_buf;
            m_buf = 0;
        }
        if(inter_buf)
        {
            delete[] inter_buf;
            inter_buf = 0;
        }
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if(m_eos)
            return MFX_ERR_MORE_DATA;
        if(bs.DataLength + bs.DataOffset > m_buf_size)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        if(bs.DataLength)
            return MFX_ERR_NONE;

        bs.Data      = m_buf;
        bs.MaxLength = m_buf_size;

        bs.DataOffset = 0;
        bs.DataLength = frame_header.frame_size;
        bs.TimeStamp  = frame_header.time_stamp;

        memcpy(bs.Data, inter_buf, bs.DataLength);
        ++count;

        if(count == file_header.n_frames)
        {
            m_eos = true;
        }
        else
        {
            const IVF_frame_header fr_hdr_next = *((IVF_frame_header*) (inter_buf + frame_header.frame_size));
            assert(inter_buf_lenth >= (frame_header.frame_size + sizeof(IVF_frame_header)) );
            inter_buf_lenth -= frame_header.frame_size + sizeof(IVF_frame_header);
            memmove(inter_buf, inter_buf + frame_header.frame_size + sizeof(IVF_frame_header), inter_buf_lenth);

            inter_buf_lenth += Read(inter_buf+inter_buf_lenth, (inter_buf_size - inter_buf_lenth));

            frame_header = fr_hdr_next;
        }

        return MFX_ERR_NONE;
    }

};

enum TCVal
{
    AVC     = MFX_CODEC_AVC,
    MPEG2   = MFX_CODEC_MPEG2,
    VC1     = MFX_CODEC_VC1,
    JPEG    = MFX_CODEC_JPEG,
    HEVC    = MFX_CODEC_HEVC,
    VP9     = MFX_CODEC_VP9,
    I_VID   = MFX_IOPATTERN_IN_VIDEO_MEMORY,
    I_SYS   = MFX_IOPATTERN_IN_SYSTEM_MEMORY,
    I_OPQ   = MFX_IOPATTERN_IN_OPAQUE_MEMORY,
    O_VID   = MFX_IOPATTERN_OUT_VIDEO_MEMORY,
    O_SYS   = MFX_IOPATTERN_OUT_SYSTEM_MEMORY,
    O_OPQ   = MFX_IOPATTERN_OUT_OPAQUE_MEMORY,
    IO_IN   = 0x0f,
    IO_OUT  = 0xf0,
    DEC     = 1,
    VPP     = 2,
    ENC     = 3
};

struct TestCase
{
    mfxU32 DEC;
    mfxU32 ENC;
    mfxU16 IOP;
    mfxU16 INV;
};

const TestCase tcNV12s[] =
{
    /*00*/ {VP9, HEVC , I_OPQ|O_VID, 0},
    /*01*/ {VP9, HEVC , I_OPQ|O_SYS, 0},
    /*02*/ {VP9, HEVC , I_OPQ|O_OPQ, 0},
    /*03*/ {VP9, AVC  , I_OPQ|O_VID, 0},
    /*04*/ {VP9, AVC  , I_OPQ|O_SYS, 0},
    /*05*/ {VP9, AVC  , I_OPQ|O_OPQ, 0},
    /*06*/ {VP9, MPEG2, I_OPQ|O_VID, 0},
    /*07*/ {VP9, MPEG2, I_OPQ|O_SYS, 0},
    /*08*/ {VP9, MPEG2, I_OPQ|O_OPQ, 0},
    /*09*/ {VP9, VP9  , I_OPQ|O_VID, 0},
    /*10*/ {VP9, VP9  , I_OPQ|O_SYS, 0},
    /*11*/ {VP9, VP9  , I_OPQ|O_OPQ, 0},
    /*12*/ {VP9, AVC  , I_OPQ|O_OPQ, 1},
};

const TestCase tcREXTs[] =
{
    /*00*/ {VP9, HEVC , I_OPQ|O_VID, 0},
    /*01*/ {VP9, HEVC , I_OPQ|O_SYS, 0},
    /*02*/ {VP9, HEVC , I_OPQ|O_OPQ, 0},
    /*03*/ {VP9, VP9  , I_OPQ|O_VID, 0},
    /*04*/ {VP9, VP9  , I_OPQ|O_SYS, 0},
    /*05*/ {VP9, VP9  , I_OPQ|O_OPQ, 0},
    /*06*/ {VP9, VP9  , I_OPQ|O_OPQ, 1},
};

const char* getStream(const decltype(MFX_FOURCC_YV12)& fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12: return "forBehaviorTest/foreman_cif.ivf";
    case MFX_FOURCC_AYUV: return "conformance/vp9/SBE/8bit_444/Stress_VP9_FC2p1ss444_432x240_250_extra_stress_2.2.0.vp9";
    case MFX_FOURCC_P010: return "conformance/vp9/SBE/10bit/Stress_VP9_FC2p2b10_432x240_050_intra_stress_1.5.vp9";
    case MFX_FOURCC_Y410: return "conformance/vp9/SBE/10bit_444/Syntax_VP9_FC2p3ss444_432x240_101_inter_basic_2.0.0.vp9";
    default: assert(!"Wrong/unsupported format requested!"); break;
    }
    return "";
}

void InvalidateOpaque(mfxFrameSurface1& s)
{
    if (g_tsImpl == MFX_IMPL_HARDWARE)
        s.Data.MemId = (mfxMemId)1;
    else
        s.Data.Y = (mfxU8*)1;
}

void RestoreOpaque(mfxFrameSurface1& s)
{
    s.Data.MemId = (mfxMemId)0;
    s.Data.Y = (mfxU8*)0;
}

int RunTest(const TestCase& tc, char const * const stream)
{
    TS_START;

    if (VP9 == tc.ENC &&
        MFX_IMPL_HARDWARE == g_tsImpl && 
        MFX_HW_KBL > g_tsHWtype)
    {
        g_tsLog << "SKIPPED for this platform\n";
        return 0;
    }

    tsTranscoder tr(tc.DEC, tc.ENC);

    tsIvfReader reader(stream, 100000);
    tr.m_bsProcIn = &reader;

    //tsBitstreamWriter w("debug.bit");
    //tr.m_bsProcOut = &w;

    tr.m_parDEC.IOPattern = tc.IOP & IO_OUT;
    tr.m_parENC.IOPattern = tc.IOP & IO_IN;

    tr.InitPipeline();

    tsVideoDecoder& dec = tr;
    tsVideoVPP& vpp = tr;
    tsVideoEncoder& enc = tr;

    switch (tc.INV)
    {
    case 1:
        tr.m_cId = 0;
        dec.SetPar4_DecodeFrameAsync();
        dec.m_default = false;

        InvalidateOpaque(*dec.m_pSurf);

        dec.DecodeFrameAsync();
        g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR);
        g_tsStatus.check();
        return 0;
    case 2:
        vpp.m_default = false;
        vpp.m_pSurfIn = vpp.m_pSurfPoolIn->GetSurface();
        vpp.m_pSurfOut = vpp.m_pSurfPoolOut->GetSurface();

        InvalidateOpaque(*vpp.m_pSurfIn);

        vpp.RunFrameVPPAsync();
        g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR);
        g_tsStatus.check();
        
        RestoreOpaque(*vpp.m_pSurfIn);
        InvalidateOpaque(*vpp.m_pSurfOut);
        
        vpp.RunFrameVPPAsync();
        g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR);
        g_tsStatus.check();

        return 0;
    case 3:
        enc.m_default = false;
        enc.m_pSurf = vpp.m_pSurfPoolOut->GetSurface();

        InvalidateOpaque(*enc.m_pSurf);

        enc.EncodeFrameAsync();
        g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR);
        g_tsStatus.check();

        return 0;
    case 0:
    default:
        break;
    }

    tr.TranscodeFrames(30);

    TS_END;
    return 0;
}

template<decltype(MFX_FOURCC_YV12) fourcc>
int RunTest_fourcc(const unsigned int id)
{
    char const * const stream = g_tsStreamPool.Get(getStream(fourcc));
    g_tsStreamPool.Reg();

    if(MFX_FOURCC_NV12 == fourcc)
        return RunTest(tcNV12s[id], stream);
    else
        return RunTest(tcREXTs[id], stream);
}

TS_REG_TEST_SUITE(vp9d_8b_420_trans_opaque,  RunTest_fourcc<MFX_FOURCC_NV12>, sizeof(tcNV12s)/sizeof(TestCase));
TS_REG_TEST_SUITE(vp9d_10b_420_trans_opaque, RunTest_fourcc<MFX_FOURCC_P010>, sizeof(tcREXTs)/sizeof(TestCase));
TS_REG_TEST_SUITE(vp9d_8b_444_trans_opaque,  RunTest_fourcc<MFX_FOURCC_AYUV>, sizeof(tcREXTs)/sizeof(TestCase));
TS_REG_TEST_SUITE(vp9d_10b_444_trans_opaque, RunTest_fourcc<MFX_FOURCC_Y410>, sizeof(tcREXTs)/sizeof(TestCase));

};