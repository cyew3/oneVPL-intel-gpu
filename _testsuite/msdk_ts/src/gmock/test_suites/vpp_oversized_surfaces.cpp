/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2017 Intel Corporation. All Rights Reserved.
//
*/
#include "ts_struct.h"
#include "ts_vpp.h"

/*
Description:
    Verify that processing with pool of oversized input surfaces and with pool of usual input surfaces
    will produce b2b result
    Pipeline:
    - create 2 session
    - alloc oversized input surfaces in first session, usual input surfaces in second session
    - process nframes in first session, calculate CRC of output
    - process nframes in second session, calculate CRC of output
    - compare crcs of outputs from both sessions
*/

#define MSDK_ALIGN16(value)  (((value + 15) >> 4) << 4) // round up to a multiple of 16
#define MSDK_ALIGN32(value)  (((value + 31) >> 5) << 5) // round up to a multiple of 16

namespace vpp_oversized_surfaces
{
const mfxU8 debug = 0;

class Processor : public tsSurfaceProcessor
{
public:
    tsSurfaceCRC32 m_crc;
    tsSurfaceWriter* m_writer;

    Processor(mfxU32 id, const std::string sn)
        : tsSurfaceProcessor()
        , m_crc()
        , m_writer(0)
    {
        if (debug)
        {
        char tmp_out[20];
#pragma warning(disable:4996)
        sprintf(tmp_out, "%02d_%s.yuv", id, sn.c_str());
#pragma warning(default:4996)
            m_writer = new tsSurfaceWriter(tmp_out);
        }
    }

    ~Processor()
    {
        if (m_writer)
        {
            delete m_writer;
            m_writer = NULL;
        }
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (m_writer)
            m_writer->ProcessSurface(s);

        return m_crc.ProcessSurface(s);
    }

    Ipp32u GetCRC() { return m_crc.GetCRC(); }
};

enum
{
    MFX_PAR
};

struct tc_struct
{
    mfxStatus sts;
    const char* stream; //raw stream name
    struct f_pair
    {
        mfxU32 ext_type;
        const  tsStruct::Field* f;
        mfxU32 v;
    } set_par[MAX_NPARS];
};

const tc_struct test_cases[] =
{
    {/*00*/ MFX_ERR_NONE, "YUV/calendar_720x480_600_nv12.yuv",
            {{MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 720},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 480}}},
    {/*01*/ MFX_ERR_NONE, "YUV/720x480i29_jetcar_CC60f.nv12",
            {{MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 720},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 480},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF}}},
    {/*02*/ MFX_ERR_NONE, "YUV/c7TC_1280x720i_bff_nv12.yuv",
            {{MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 1280},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 720},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF}}},
    {/*03*/ MFX_ERR_NONE, "forBehaviorTest/720x480i29_jetcar_30f.rgb4",
            {{MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 720},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 480},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
    {/*04*/ MFX_ERR_NONE, "forBehaviorTest/720x480i29_jetcar_30f.rgb4",
            {{MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 720},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 480},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444}}},
    {/*05*/ MFX_ERR_NONE, "YUV/720x480i29_jetcar_CC60f.nv12",
            {{MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 720},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 480},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
            {MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED}}},
    {/*06*/ MFX_ERR_NONE, "YUV/c7TC_1280x720i_bff_nv12.yuv",
            {{MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 1280},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 720},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
             {MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_ADVANCED}}},
    {/*07*/ MFX_ERR_NONE, "YUV/c7TC_1280x720i_nv12.yuv",
            {{MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 1280},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 720},
             {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
             {MFX_PAR, &tsStruct::mfxExtVPPDeinterlacing.Mode,  MFX_DEINTERLACING_BOB}}},
    {/*08*/ MFX_ERR_NONE, "YUV/720x480i29_jetcar_CC60f.nv12",
            {{MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 720},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 480},
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 75}}},
    {/*09*/ MFX_ERR_NONE, "YUV/720x480i29_jetcar_CC60f.nv12",
            {{MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 720},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 480},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 75}}},
    {/*10*/ MFX_ERR_NONE, "YUV/720x480i29_jetcar_CC60f.nv12",
            {{MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropW, 720},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.CropH, 480},
            {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
            {MFX_PAR, &tsStruct::mfxExtVPPDenoise.DenoiseFactor, 75}}},
};

const mfxU16 IOPatterns[] = {MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY,
                             MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY};

const mfxU8 ncases = sizeof(test_cases)/sizeof(tc_struct) * (sizeof(IOPatterns)/sizeof(mfxU16));

const mfxU32 s_oversized_w = 1920;
const mfxU32 s_oversized_h = 1088;

tc_struct GetTestCase(unsigned int id)
{
    return test_cases[id / (sizeof(IOPatterns)/sizeof(mfxU16))];
}

int RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = GetTestCase(id);

    const mfxU8 nframes = 10;
    bool default_pars = true;
    tsVideoVPP test_vpp(default_pars); // session with pool of oversized surfaces
    tsVideoVPP cmp_vpp(default_pars);
    test_vpp.MFXInit();
    cmp_vpp.MFXInit();

    tsExtBufType<mfxVideoParam> test_par (test_vpp.m_par);
     // set test parameters
    SETPARS(&test_par, MFX_PAR);
    bool interlaced = (test_par.vpp.In.PicStruct != MFX_PICSTRUCT_PROGRESSIVE);
    test_par.vpp.In.Width = MSDK_ALIGN16(test_par.vpp.In.CropW);
    test_par.vpp.In.Height = interlaced ? MSDK_ALIGN32(test_par.vpp.In.CropH) : MSDK_ALIGN16(test_par.vpp.In.CropH);
    test_par.vpp.Out.FourCC = test_par.vpp.In.FourCC;
    test_par.vpp.Out.ChromaFormat = test_par.vpp.In.ChromaFormat;
    test_par.IOPattern = IOPatterns[id % (sizeof(IOPatterns)/sizeof(mfxU16))];

    mfxExtVPPDeinterlacing* di = (mfxExtVPPDeinterlacing*)test_par.GetExtBuffer(MFX_EXTBUFF_VPP_DEINTERLACING);
    mfxExtVPPDenoise* dn = (mfxExtVPPDenoise*)test_par.GetExtBuffer(MFX_EXTBUFF_VPP_DENOISE);
    if (di)
    {
        test_par.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        test_par.vpp.Out.CropW = test_par.vpp.In.CropW;
        test_par.vpp.Out.CropH = test_par.vpp.In.CropH;
        test_par.vpp.Out.Width = MSDK_ALIGN16(test_par.vpp.Out.CropW);
        test_par.vpp.Out.Height = MSDK_ALIGN16(test_par.vpp.Out.CropH);
    }
    else if (dn)
    {
        test_par.vpp.Out.PicStruct = test_par.vpp.In.PicStruct;
        test_par.vpp.Out.CropW = test_par.vpp.In.CropW;
        test_par.vpp.Out.CropH = test_par.vpp.In.CropH;
        test_par.vpp.Out.Width = MSDK_ALIGN16(test_par.vpp.Out.CropW);
        test_par.vpp.Out.Height = MSDK_ALIGN16(test_par.vpp.Out.CropH);
    }
    else
    {
        test_par.vpp.Out.PicStruct = test_par.vpp.In.PicStruct;
        test_par.vpp.Out.Width = test_par.vpp.Out.CropW = MSDK_ALIGN16(test_par.vpp.In.Width / 2);
        test_par.vpp.Out.Height = test_par.vpp.Out.CropH = interlaced ? MSDK_ALIGN32(test_par.vpp.In.Height / 2)
                                                                      : MSDK_ALIGN16(test_par.vpp.In.Height / 2);
    }

    const char* sname = g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();
    tsRawReader input_reader(sname, test_par.vpp.In);
    test_vpp.m_surf_in_processor = &input_reader;

    // create processor for crc calculation and dump stream
    Processor test_proc(id, "test");
    Processor cmp_proc(id, "ref");
    test_vpp.m_surf_out_processor = &test_proc;
    cmp_vpp.m_surf_out_processor = &cmp_proc;

    memcpy(&test_vpp.m_par, &test_par, sizeof(mfxVideoParam));
    if (   test_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY
        || test_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
    {
        test_vpp.CreateAllocators();
        test_vpp.SetHandle();
    }

    // create pool of oversized surfaces
    test_vpp.m_par.vpp.In.Width = test_vpp.m_par.vpp.In.CropW = MSDK_ALIGN16(s_oversized_w);
    test_vpp.m_par.vpp.In.Height = test_vpp.m_par.vpp.In.CropH = interlaced ? MSDK_ALIGN32(s_oversized_h) : MSDK_ALIGN16(s_oversized_h);
    test_vpp.AllocSurfaces();
    test_vpp.Init();
    // restore test parameters
    memcpy(&test_vpp.m_par, &test_par, sizeof(mfxVideoParam));
    test_vpp.Reset();
    for (mfxU32 i = 0; i < test_vpp.m_pSurfPoolIn->PoolSize(); i++)
    {
        test_vpp.m_pSurfPoolIn->GetSurface(i)->Info.CropW = test_vpp.m_par.vpp.In.CropW;
        test_vpp.m_pSurfPoolIn->GetSurface(i)->Info.CropH = test_vpp.m_par.vpp.In.CropH;
    }
    test_vpp.ProcessFrames(nframes);
    bool check_locked_cnt = true;
    test_vpp.Close(check_locked_cnt);

    input_reader.ResetFile();
    cmp_vpp.m_surf_in_processor = &input_reader;

    memcpy(&cmp_vpp.m_par, &test_par, sizeof(mfxVideoParam));
    if (   test_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY
        || test_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
    {
        cmp_vpp.CreateAllocators();
        cmp_vpp.SetHandle();
    }
    // create pool of normal size surfaces
    cmp_vpp.AllocSurfaces();
    cmp_vpp.Init();
    cmp_vpp.ProcessFrames(nframes);
    cmp_vpp.Close(check_locked_cnt);

    Ipp32u test_crc = test_proc.GetCRC();
    Ipp32u cmp_crc = cmp_proc.GetCRC();

    g_tsLog << "CRC = " << test_crc << "\n";
    g_tsLog << "CMP CRC = " << cmp_crc << "\n";
    if (test_crc != cmp_crc)
    {
        g_tsLog << "ERROR: the 2 crc values should be the same\n";
        g_tsStatus.check(MFX_ERR_ABORTED);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(vpp_oversized_surfaces, RunTest, ncases);
};
