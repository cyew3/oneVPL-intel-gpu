/* ****************************************************************************** *\
INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */


#include "ts_encoder.h"
#include "ts_struct.h"

/*
 * This test is for VCSD100025510 - create tests on oversized surfaces
 * Issue reproduced in calling: AllocFrames(oversized res)->Init(oversized res)
 *                              ->Reset(correct res)->EncodeFrames(with oversized surfaces)
 *
 * Tests configuration:
 *  1. Oversized Surface Resolution:
 *     -1920x1080
 *  2. Source Stream Resolution:
 *     -1280x720 (16 aligned only)
 *     -720x480 (16&32 aligned)
 *     -720x240 (16 aligned only)
 *     -868x588 (neither 16 nor 32 aligned, but 4 aligned for interlaced test)
 */

namespace avce_oversized_surfaces
{
#if !defined(MSDK_ALIGN16)
#define MSDK_ALIGN16(value)                      (((value + 15) >> 4) << 4) // round up to a multiple of 16
#endif
#if !defined(MSDK_ALIGN32)
#define MSDK_ALIGN32(value)                      (((value + 31) >> 5) << 5) // round up to a multiple of 16
#endif

class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_AVC, true)
        , m_reader()
    {
        srand(0);
        m_filler = this;
    }
    ~TestSuite()
    {
        delete m_reader;
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        //tsRawReader.ProcessSurface() doesn't rely on input s.Info to copy the raw data.
        mfxStatus sts = m_reader->ProcessSurface(s);

        if (m_reader->m_eos)  // re-read stream from the beginning to reach target NumFrames
        {
            m_reader->ResetFile();
            sts = m_reader->ProcessSurface(s);
        }

        return sts;
    }

private:
    enum
    {
        MFX_PAR = 1,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 s_width; //oversized surface's width to create
        mfxU32 s_height;
        const char* stream; //raw stream name
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];

    tsRawReader *m_reader;
};


#define mfx_PicStruct     tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct
#define mfx_CropW         tsStruct::mfxVideoParam.mfx.FrameInfo.CropW
#define mfx_CropH         tsStruct::mfxVideoParam.mfx.FrameInfo.CropH
#define mfx_GopPicSize    tsStruct::mfxVideoParam.mfx.GopPicSize
#define mfx_GopRefDist    tsStruct::mfxVideoParam.mfx.GopRefDist
#define mfx_ChromaFormat  tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat
#define mfx_FourCC        tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC
#define mfx_IOPattern     tsStruct::mfxVideoParam.IOPattern
const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 1920, 1080, "YUV/videoconf1_720p_60.yuv",
            {{MFX_PAR, &mfx_CropW, 1280},
             {MFX_PAR, &mfx_CropH, 720},
             {MFX_PAR, &mfx_GopPicSize, 32},
             {MFX_PAR, &mfx_GopRefDist, 2},
             {MFX_PAR, &mfx_ChromaFormat, MFX_CHROMAFORMAT_YUV420},
             {MFX_PAR, &mfx_FourCC, MFX_FOURCC_NV12},
             {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*01*/ MFX_ERR_NONE, 1920, 1080, "YUV/videoconf1_720p_60.yuv",
            {{MFX_PAR, &mfx_CropW, 1280},
             {MFX_PAR, &mfx_CropH, 720},
             {MFX_PAR, &mfx_GopPicSize, 32},
             {MFX_PAR, &mfx_GopRefDist, 2},
             {MFX_PAR, &mfx_ChromaFormat, MFX_CHROMAFORMAT_YUV420},
             {MFX_PAR, &mfx_FourCC, MFX_FOURCC_NV12},
             {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
             {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*02*/ MFX_ERR_NONE, 1920, 1080, "YUV/videoconf1_720p_60.yuv",
            {{MFX_PAR, &mfx_CropW, 1280},
             {MFX_PAR, &mfx_CropH, 720},
             {MFX_PAR, &mfx_GopPicSize, 32},
             {MFX_PAR, &mfx_GopRefDist, 2},
             {MFX_PAR, &mfx_ChromaFormat, MFX_CHROMAFORMAT_YUV420},
             {MFX_PAR, &mfx_FourCC, MFX_FOURCC_NV12},
             {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
             {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*03*/ MFX_ERR_NONE, 1920, 1080, "YUV/iceage_720x480_491.yuv",
            {{MFX_PAR, &mfx_CropW, 720},
             {MFX_PAR, &mfx_CropH, 480},
             {MFX_PAR, &mfx_ChromaFormat, MFX_CHROMAFORMAT_YUV420},
             {MFX_PAR, &mfx_FourCC, MFX_FOURCC_NV12},
             {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*04*/ MFX_ERR_NONE, 1920, 1080, "YUV/iceage_720x480_491.yuv",
            {{MFX_PAR, &mfx_CropW, 720},
             {MFX_PAR, &mfx_CropH, 480},
             {MFX_PAR, &mfx_ChromaFormat, MFX_CHROMAFORMAT_YUV420},
             {MFX_PAR, &mfx_FourCC, MFX_FOURCC_NV12},
             {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
             {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*05*/ MFX_ERR_NONE, 1920, 1080, "YUV/iceage_720x480_491.yuv",
            {{MFX_PAR, &mfx_CropW, 720},
             {MFX_PAR, &mfx_CropH, 480},
             {MFX_PAR, &mfx_ChromaFormat, MFX_CHROMAFORMAT_YUV420},
             {MFX_PAR, &mfx_FourCC, MFX_FOURCC_NV12},
             {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
             {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*06*/ MFX_ERR_NONE, 1920, 1080, "YUV/iceage_720x240_491.yuv",
            {{MFX_PAR, &mfx_CropW, 720},
             {MFX_PAR, &mfx_CropH, 240},
             {MFX_PAR, &mfx_ChromaFormat, MFX_CHROMAFORMAT_YUV420},
             {MFX_PAR, &mfx_FourCC, MFX_FOURCC_NV12},
             {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*07*/ MFX_ERR_NONE, 1920, 1080, "YUV/iceage_720x240_491.yuv",
            {{MFX_PAR, &mfx_CropW, 720},
             {MFX_PAR, &mfx_CropH, 240},
             {MFX_PAR, &mfx_ChromaFormat, MFX_CHROMAFORMAT_YUV420},
             {MFX_PAR, &mfx_FourCC, MFX_FOURCC_NV12},
             {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
             {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*08*/ MFX_ERR_NONE, 1920, 1080, "YUV/iceage_720x240_491.yuv",
            {{MFX_PAR, &mfx_CropW, 720},
             {MFX_PAR, &mfx_CropH, 240},
             {MFX_PAR, &mfx_ChromaFormat, MFX_CHROMAFORMAT_YUV420},
             {MFX_PAR, &mfx_FourCC, MFX_FOURCC_NV12},
             {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
             {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*09*/ MFX_ERR_NONE, 1920, 1080, "YUV/barcelona2_868x588_1.yuv",
            {{MFX_PAR, &mfx_CropW, 868},
             {MFX_PAR, &mfx_CropH, 588},
             {MFX_PAR, &mfx_ChromaFormat, MFX_CHROMAFORMAT_YUV420},
             {MFX_PAR, &mfx_FourCC, MFX_FOURCC_NV12},
             {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*10*/ MFX_ERR_NONE, 1920, 1080, "YUV/barcelona2_868x588_1.yuv",
            {{MFX_PAR, &mfx_CropW, 868},
             {MFX_PAR, &mfx_CropH, 588},
             {MFX_PAR, &mfx_ChromaFormat, MFX_CHROMAFORMAT_YUV420},
             {MFX_PAR, &mfx_FourCC, MFX_FOURCC_NV12},
             {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TFF},
             {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
    {/*11*/ MFX_ERR_NONE, 1920, 1080, "YUV/barcelona2_868x588_1.yuv",
            {{MFX_PAR, &mfx_CropW, 868},
             {MFX_PAR, &mfx_CropH, 588},
             {MFX_PAR, &mfx_ChromaFormat, MFX_CHROMAFORMAT_YUV420},
             {MFX_PAR, &mfx_FourCC, MFX_FOURCC_NV12},
             {MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BFF},
             {MFX_PAR, &mfx_IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

const int frameNumber = 30;

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    //set input parameters for mfxVideoParam
    SETPARS(m_pPar, MFX_PAR);
    //workaround for VCSD100026230, explicitly set level in the tests. So this test only verify the stream generated by driver.
    m_pPar->mfx.CodecLevel = MFX_LEVEL_AVC_4;
    if (m_pPar->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_UNKNOWN) {
        //in case PicStruct is not set explicitly.
        m_pPar->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }
    m_pPar->mfx.FrameInfo.Width = MSDK_ALIGN16(m_pPar->mfx.FrameInfo.CropW);
    m_pPar->mfx.FrameInfo.Height = (m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)?
                                   MSDK_ALIGN16(m_pPar->mfx.FrameInfo.CropH) : MSDK_ALIGN32(m_pPar->mfx.FrameInfo.CropH);

    {
        auto fi = m_pPar->mfx.FrameInfo;
        fi.FourCC = MFX_FOURCC_YV12;
        m_reader = new tsRawReader(g_tsStreamPool.Get(tc.stream), fi);
    }
    g_tsStreamPool.Reg();

    //save the correct resolution frame information
    mfxFrameInfo SavedInfo;
    SavedInfo = m_pPar->mfx.FrameInfo;
    m_pPar->mfx.FrameInfo.CropW = tc.s_width;
    m_pPar->mfx.FrameInfo.CropH = tc.s_height;
    m_pPar->mfx.FrameInfo.Width = MSDK_ALIGN16(m_pPar->mfx.FrameInfo.CropW);
    m_pPar->mfx.FrameInfo.Height = (m_pPar->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)?
                                   MSDK_ALIGN16(m_pPar->mfx.FrameInfo.CropH) : MSDK_ALIGN32(m_pPar->mfx.FrameInfo.CropH);

    mfxU32 nf = g_tsConfig.sim ? 5 : frameNumber;
    m_max = nf; //some stupid comment about m_max being max number of frames in base tsSurfaceProcessor before returning zero-ptrs
    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

// 1. encode with oversized surfaces
    tsBitstreamCRC32 bs_crc;
    m_bs_processor = &bs_crc;
    //bs_crc.m_dump = "avce_oversized_surfaces0.264";

    MFXInit();
    if (m_pPar->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) {
        SetFrameAllocator(m_session, m_pVAHandle);
        m_pFrameAllocator = m_pVAHandle;
    }
    if(m_pFrameAllocator && !GetAllocator()) {
        SetAllocator(m_pFrameAllocator, true);
    }
    AllocSurfaces();
    Init(m_session, m_pPar);
    //re-load the correct surface resolution and reset encoder
    m_pPar->mfx.FrameInfo = SavedInfo;
    Reset();
    AllocBitstream();
    EncodeFrames(nf);
    DrainEncodedBitstream();
    Ipp32u crc = bs_crc.GetCRC();

// reset
    Close();
    m_reader->ResetFile();
    m_cur = 0; //some stupid comment about reseting current frame counter in base tsSurfaceProcessor to make it provide the same number of frames as in previous cycle
    memset(&m_bitstream, 0, sizeof(mfxBitstream));

// 2. encode with "correct"-size surfaces
    tsBitstreamCRC32 bs_cmp_crc;
    m_bs_processor = &bs_cmp_crc;
    //bs_cmp_crc.m_dump = "avce_oversized_surfaces1.264";

    QueryIOSurf(); //update m_request with exact surf. size
    AllocSurfaces();
    EncodeFrames(nf);
    DrainEncodedBitstream();
    Ipp32u cmp_crc = bs_cmp_crc.GetCRC();
    g_tsLog << "crc = " << crc << "\n";
    g_tsLog << "cmp_crc = " << cmp_crc << "\n";
    if (crc != cmp_crc)
    {
        g_tsLog << "ERROR: the 2 crc values should be the same\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_oversized_surfaces);
};
