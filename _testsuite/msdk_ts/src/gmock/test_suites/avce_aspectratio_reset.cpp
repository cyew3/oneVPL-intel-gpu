/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2020 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace avce_aspectratio_reset
{

class TestSuite : tsVideoEncoder //, public tsBitstreamProcessor, public tsParserH264AU
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC), m_session() {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    mfxSession m_session;

    enum
    {
        MFXAR = 1, //aspect ratio
        MFXRES //resolution
    };

    struct tc_struct
    {
        mfxStatus sts;
        struct f_pair
        {
            mfxU32 ext_type;

            const  tsStruct::Field* arw;
            mfxU32 v_arw;
            const  tsStruct::Field* arh;
            mfxU32 v_arh;
            const  tsStruct::Field* resw;
            mfxU32 v_resw;
            const  tsStruct::Field* resh;
            mfxU32 v_resh;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    //default input resolution, then reset aspect ratio twice
    {/* 0*/ MFX_ERR_NONE, {
                              {MFXAR,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,10,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,11,0,0,0,0},
                              {MFXAR,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,64,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,33,0,0,0,0}}},

    //reset input resolution and aspect ratio at the same time
    {/* 2*/ MFX_ERR_NONE, {
                              {MFXAR | MFXRES,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,16,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,11,
                              &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,720,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,576}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

const mfxU16 ARbyIdc[][2] = {
    { 1, 1}, { 1, 1}, {12,11}, {10,11}, {16, 11}, { 40,33}, {24,11}, {20, 11},
    {32,11}, {80,33}, {18,11}, {15,11}, {64, 33}, {160,99}, { 4, 3}, { 3,  2}, { 2, 1}
};

class Verifier : public tsBitstreamProcessor, public tsParserH264AU
{
    mfxU16 m_aspectratioW;
    mfxU16 m_aspectratioH;
public:
    Verifier() : m_aspectratioW(0),m_aspectratioH(0) {}
    mfxStatus Init(mfxU16 ar_w, mfxU16 ar_h)
    {
        m_aspectratioW = ar_w;
        m_aspectratioH = ar_h;
        return MFX_ERR_NONE;
    }
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        SetBuffer(bs);
        UnitType& au = ParseOrDie();

        for(mfxU32 i = 0; i < au.NumUnits; i ++)
        {
            if (!(au.NALU[i].nal_unit_type == 0x07))
            {
                continue;
            }

            if (au.NALU[i].SPS->vui_parameters_present_flag != 1)
            {
                byte fl = au.NALU[i].SPS->vui_parameters_present_flag;
                g_tsLog << "ERROR: vui_parameters_present_flag is " << fl
                         << " (expected 1)\n";
                return MFX_ERR_ABORTED;
            }
            mfxU16 ARW = au.NALU[i].SPS->vui->aspect_ratio_idc == 255 ? au.NALU[i].SPS->vui->sar_width : ARbyIdc[au.NALU[i].SPS->vui->aspect_ratio_idc][0];
            mfxU16 ARH = au.NALU[i].SPS->vui->aspect_ratio_idc == 255 ? au.NALU[i].SPS->vui->sar_height : ARbyIdc[au.NALU[i].SPS->vui->aspect_ratio_idc][1];
            g_tsLog << "INFO: vui ARW = " << ARW <<" vui ARH = " << ARH << "\n";
            if(m_aspectratioW != 0 && m_aspectratioH != 0)
            {
                if(m_aspectratioW != ARW || m_aspectratioH != ARH)
                {
                    g_tsLog << "ERROR: sar_width is " << ARW << "(expected " << m_aspectratioW << ")\n";
                    g_tsLog << "ERROR: sar_height is " << ARH << "(expected " << m_aspectratioH << ")\n";
                    return MFX_ERR_ABORTED;
                }
            }
        }
        bs.DataLength = 0;
        return MFX_ERR_NONE;
    }
};

class ResChange : public tsSurfaceProcessor
{
private:
    mfxU32 m_dynamic_width, m_dynamic_height;
public:
    ResChange() : m_dynamic_width(0),m_dynamic_height(0) {}
    mfxStatus Init(mfxU16 res_w, mfxU16 res_h)
    {
        m_dynamic_width = res_w;
        m_dynamic_height = res_h;
        return MFX_ERR_NONE;
    }
    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if(m_dynamic_width == 0 && m_dynamic_height == 0)
        {
            return MFX_ERR_NONE;
        }
        if(s.Info.Width != m_dynamic_width)
        {
            s.Info.Width = m_dynamic_width;
            s.Info.CropW = m_dynamic_width;
        }
        if(s.Info.Height != m_dynamic_height)
        {
            s.Info.Height = m_dynamic_height;
            s.Info.CropH = m_dynamic_height;
        }
        return MFX_ERR_NONE;
    }
};

#if !defined(MSDK_ALIGN16)
#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4)
#endif
#if !defined(MSDK_ALIGN32)
#define MSDK_ALIGN32(X) (((mfxU32)((X)+31)) & (~ (mfxU32)31))
#endif

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    if (id == 1 && g_tsConfig.sim && g_tsConfig.lowpower != MFX_CODINGOPTION_ON)
    {
        // workload is too big for sim environment
        throw tsSKIP;
    }
    auto& tc = test_case[id];
    int nframes = 10;
    mfxU16 width = g_tsConfig.sim? 256 : 1920;
    mfxU16 height = g_tsConfig.sim ? 144 : 1080;

    tsStruct::set(m_pPar,tsStruct::mfxVideoParam.mfx.FrameInfo.Width, MSDK_ALIGN16(width));
    // ALIGN32 required for MFX_PICSTRUCT_PROGRESSIVE
    tsStruct::set(m_pPar,tsStruct::mfxVideoParam.mfx.FrameInfo.Height, MSDK_ALIGN32(height));
    tsStruct::set(m_pPar,tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, width);
    tsStruct::set(m_pPar,tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, height);
    Init();

    Verifier verifier;
    m_bs_processor = &verifier;
    ResChange resolution;
    m_filler = &resolution;
    EncodeFrames(nframes);

    for(unsigned int i = 0; i < MAX_NPARS; i++)
    {
        if(!(tc.set_par[i].ext_type & (MFXAR | MFXRES)))
        {
            break;
        }

        verifier.Init(0,0);
        resolution.Init(0,0);
        if(tc.set_par[i].ext_type & MFXAR)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].arw, tc.set_par[i].v_arw);
            tsStruct::set(m_pPar, *tc.set_par[i].arh, tc.set_par[i].v_arh);
            verifier.Init(tc.set_par[i].v_arw, tc.set_par[i].v_arh);
        }
        if(tc.set_par[i].ext_type & MFXRES)
        {
            mfxU32 resw = g_tsConfig.sim? 180 : tc.set_par[i].v_resw;
            mfxU32 resh = g_tsConfig.sim? 144 : tc.set_par[i].v_resh;

            tsStruct::set(m_pPar, *tc.set_par[i].resw, MSDK_ALIGN16(resw));
            tsStruct::set(m_pPar, *tc.set_par[i].resh, MSDK_ALIGN32(resh));
            tsStruct::set(m_pPar,tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, resw);
            tsStruct::set(m_pPar,tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, resh);
            resolution.Init(tc.set_par[i].v_resw, tc.set_par[i].v_resh);
        }
        Reset();
        EncodeFrames(nframes);
    }
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_aspectratio_reset);
}
