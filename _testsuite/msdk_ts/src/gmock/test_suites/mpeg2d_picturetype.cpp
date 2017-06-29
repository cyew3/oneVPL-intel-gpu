/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"

namespace mpeg2d_picturetype
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_MPEG2) {}
    ~TestSuite()
    {
        for (mfxU32 i = 0; i < PoolSize(); i++)
        {
            mfxFrameSurface1* p = GetSurface(i);
            if (0 != p->Data.NumExtParam)
            {
                delete p->Data.ExtParam[0];
                delete [] p->Data.ExtParam;
            }
        }
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 max_frames = 30;

    enum
    {
        MFX_PAR = 1,
        ALLOC_OPAQUE = 8,
    };

    enum
    {
        DISPORDER = 1,
        RAND,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        std::string stream;
        mfxU16 struct_id;
        mfxU32 mem_type;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxF32 v;
        } set_par[MAX_NPARS];

    };
    struct stream_struct
    {
        mfxU16 id;
        mfxU16 n_frames;
        mfxU16 pictype[max_frames];
    };

    static const tc_struct test_case[];
    static const stream_struct pictype_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*01*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_prg_I.mpg"  , 0},
    {/*02*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_prg_IP.mpg" , 1},
    {/*03*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_prg_IPB.mpg", 2},
    {/*04*/ MFX_ERR_NONE, RAND     , "conformance/mpeg2/bsd_pak_test/bugs_prg_IPB.mpg", 2},
    {/*05*/ MFX_ERR_NONE, RAND     , "conformance/mpeg2/bsd_pak_test/bugs_int_IPB.mpg", 3},
    {/*06*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_fld_IPB.mpg", 4},
    {/*07*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_int_IPB.mpg", 3},
    {/*08*/ MFX_ERR_NONE, RAND     , "conformance/mpeg2/bsd_pak_test/bugs_fld_IPB.mpg", 4},
    {/*09*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_prg_I.mpg"  , 0, ALLOC_OPAQUE, 
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*10*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_prg_IP.mpg" , 1, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*11*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_prg_IPB.mpg", 2, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*12*/ MFX_ERR_NONE, RAND     , "conformance/mpeg2/bsd_pak_test/bugs_prg_IPB.mpg", 2, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*13*/ MFX_ERR_NONE, RAND     , "conformance/mpeg2/bsd_pak_test/bugs_int_IPB.mpg", 3, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*14*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_fld_IPB.mpg", 4, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*15*/ MFX_ERR_NONE, DISPORDER, "conformance/mpeg2/bsd_pak_test/bugs_int_IPB.mpg", 3, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*16*/ MFX_ERR_NONE, RAND     , "conformance/mpeg2/bsd_pak_test/bugs_fld_IPB.mpg", 4, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

//Table of stream's structure aligned with 3 frames
// for interlace cases aligned with 2 frames (except first frame)
const TestSuite::stream_struct TestSuite::pictype_case[] =
{
    {0, 30, { MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR }},

    {1, 30, { MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF}},

    {2, 30, { MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_B, MFX_FRAMETYPE_B,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_B , MFX_FRAMETYPE_B,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_B , MFX_FRAMETYPE_B,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_B , MFX_FRAMETYPE_B,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_B, MFX_FRAMETYPE_B,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_B , MFX_FRAMETYPE_B,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_B , MFX_FRAMETYPE_B,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_B , MFX_FRAMETYPE_B,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR, MFX_FRAMETYPE_B, MFX_FRAMETYPE_B,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF, MFX_FRAMETYPE_B , MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF}},

    {3, 30, { MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR|MFX_FRAMETYPE_xI|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR|MFX_FRAMETYPE_xI|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR|MFX_FRAMETYPE_xI|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF}},

    {4, 30, { MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, 
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_I|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_IDR|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF,
              MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF, MFX_FRAMETYPE_B|MFX_FRAMETYPE_xB,
              MFX_FRAMETYPE_P|MFX_FRAMETYPE_REF|MFX_FRAMETYPE_xP|MFX_FRAMETYPE_xREF}}

};

class Verifier : public tsSurfaceProcessor
{
public:
    std::list <mfxU16> expected_pictype;
    mfxU16 frame;
    Verifier(const mfxU16* type, const mfxU16 N)
        : expected_pictype(type, type + N), frame(0) {}
    ~Verifier() {}

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (s.Data.NumExtParam != 1)
        {
            expected_pictype.pop_front();
            frame++;
            return MFX_ERR_NONE;
        }
        if (s.Data.ExtParam[0] == NULL)
        {
            g_tsLog << "ERROR: null pointer ExtParam\n";
            return MFX_ERR_NULL_PTR;
        }
        if (s.Data.ExtParam[0]->BufferId != MFX_EXTBUFF_DECODED_FRAME_INFO ||
            s.Data.ExtParam[0]->BufferSz == 0)
        {
            g_tsLog << "ERROR: Wrong external buffer or zero size buffer\n";
            return MFX_ERR_NOT_INITIALIZED;
        }
        mfxU16 pictype = ((mfxExtDecodedFrameInfo*)s.Data.ExtParam[0])->FrameType;
        if (expected_pictype.empty())
            return MFX_ERR_NONE;
        if (pictype != expected_pictype.front())
        {
            g_tsLog << "ERROR: Frame#" << frame << " real picture type = " << pictype  << " is not equal expected = " << expected_pictype.front() << "\n";
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
        expected_pictype.pop_front();
        frame++;

        return MFX_ERR_NONE;
    }

};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    const stream_struct& pt = pictype_case[tc.struct_id];
    MFXInit();

    const char* sname = g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 100000);
    m_bs_processor = &reader;

    SETPARS(m_pPar, MFX_PAR);

    if (tc.mem_type == ALLOC_OPAQUE)
    {

        AllocSurfaces();

        m_par.AddExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION, sizeof(mfxExtOpaqueSurfaceAlloc));
        mfxExtOpaqueSurfaceAlloc *osa = (mfxExtOpaqueSurfaceAlloc*)m_par.GetExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        m_request.Type = MFX_MEMTYPE_OPAQUE_FRAME;
        m_request.NumFrameSuggested = m_request.NumFrameMin;

        MFXVideoDECODE_QueryIOSurf(m_session, m_pPar, &m_request);

        AllocOpaque(m_request, *osa);
        Init(m_session, m_pPar);
    }
    else
    {
        AllocSurfaces();
        Init();
    }

    mfxU32 step =  (tc.mode & RAND) ? 2 : 1;
    for (mfxU32 i = 0; i < PoolSize(); i+=step)
    {
        mfxFrameSurface1* p = GetSurface(i);
        p->Data.ExtParam = new mfxExtBuffer*[1];
        p->Data.ExtParam[0] = (mfxExtBuffer*)new mfxExtDecodedFrameInfo;
        memset(p->Data.ExtParam[0], 0, sizeof(mfxExtDecodedFrameInfo));
        p->Data.ExtParam[0]->BufferId = MFX_EXTBUFF_DECODED_FRAME_INFO;
        p->Data.ExtParam[0]->BufferSz = sizeof(mfxExtDecodedFrameInfo);
        p->Data.NumExtParam = 1;
    }
    Verifier v(pt.pictype, pt.n_frames);
    m_surf_processor = &v;

    DecodeFrames(pt.n_frames);

    g_tsStatus.expect(tc.sts);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(mpeg2d_picturetype);
}
