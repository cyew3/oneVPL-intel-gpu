#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace fei_encode_disable_deblocking
{

class TestSuite : public tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_FEI_FUNCTION_ENCPAK, MFX_CODEC_AVC, true) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        MFXPAR = 1,
    };

    enum
    {
        RAND = 1,
        SLICE_CONST = 1 << 2,
        FRAME_CONST = 1 << 3
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
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];

};

const TestSuite::tc_struct TestSuite::test_case[] =
{/*00*/ MFX_ERR_NONE, FRAME_CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF}},
 /*00*/ MFX_ERR_NONE, FRAME_CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF}},
 /*00*/ MFX_ERR_NONE, FRAME_CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}},
 /*00*/ MFX_ERR_NONE, SLICE_CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 3},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF}},
 /*00*/ MFX_ERR_NONE, SLICE_CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 3},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF}},
 /*00*/ MFX_ERR_NONE, SLICE_CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 3},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}},
 /*00*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 1},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF}},
 /*00*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF}},
 /*00*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 1},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF}},
 /*00*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF}},
 /*00*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 1},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}},
 /*00*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}},
 // cases with system memory
 /*00*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                             {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}},
 /*00*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                             {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}},
 /*00*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                             {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}},

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class ProcessDeblocking : public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserH264AU
{
    mfxU32 m_nFields;
    mfxU32 m_nSurf;
    mfxU32 m_nFrame;
    std::vector <mfxExtBuffer*> m_buffs;

public:
    ProcessDeblocking(mfxU32 fields, std::vector <mfxExtFeiSliceHeader*> buffs) 
        : m_nFields(fields), m_nSurf(0), m_nFrame(0)
    {
        for (std::vector<mfxExtFeiSliceHeader*>::iterator it = buffs.begin(); it != buffs.end(); it++)
        {
            m_buffs.push_back((mfxExtBuffer*)(*it));
            if (m_nFields == 2)
                m_buffs.push_back((mfxExtBuffer*)(*it));
        }
    }
     
    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {

        s.Data.NumExtParam = m_nFields;
        s.Data.ExtParam = new mfxExtBuffer*[m_nFields];
        s.Data.ExtParam = &m_buffs[m_nSurf];

        m_nSurf++;

        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

        mfxU16 err = 0;

        while (nFrames--) {
            for (mfxU16 field = 0; field < m_nFields; field++) {
                UnitType& au = ParseOrDie();
                mfxU16 nSlice = 0;
                for (Bs32u i = 0; i < au.NumUnits; i++)
                {
                    if (!(au.NALU[i].nal_unit_type == 0x01 || au.NALU[i].nal_unit_type == 0x05))
                        continue;

                    mfxExtFeiSliceHeader* tmp = (mfxExtFeiSliceHeader*) m_buffs[m_nFrame*m_nFields];
                    
                    Bs32s expected_idc = tmp->Slice[nSlice].DisableDeblockingFilterIdc;
                    Bs32s expected_alpha = tmp->Slice[nSlice].SliceAlphaC0OffsetDiv2;
                    Bs32s expected_beta = tmp->Slice[nSlice].SliceBetaOffsetDiv2;
                    Bs32s idc = au.NALU[i].slice_hdr->disable_deblocking_filter_idc;
                    Bs32s alpha = au.NALU[i].slice_hdr->slice_alpha_c0_offset_div2;
                    Bs32s beta = au.NALU[i].slice_hdr->slice_beta_offset_div2;

                    g_tsLog << "DisableDeblockingFilterIdc = " << idc << " (expected = " << expected_idc << ")\n";
                    g_tsLog << "SliceAlphaC0OffsetDiv2 = " << alpha << " (expected = " << expected_alpha << ")\n";
                    g_tsLog << "SliceBetaOffsetDiv2 = " << beta << " (expected = " << expected_beta << ")\n";
                    
                    if ( idc != expected_idc)
                    {
                        g_tsLog << "ERROR: DisableDeblockingFilterIdc in stream != expected\n";
                        err++;
                    }
                    if (alpha != expected_alpha)
                    {
                        g_tsLog << "ERROR: SliceAlphaC0OffsetDiv2 in stream != expected\n";
                        err++;
                    }
                    if (beta != expected_beta)
                    {
                        g_tsLog << "ERROR: SliceBetaOffsetDiv2 in stream != expected\n";
                        err++;
                    }
                    g_tsLog << "\n";
                    nSlice++;
                }
            }
            m_nFrame++;
        }

        bs.DataOffset = 0;
        bs.DataLength = 0;

        if (err)
            return MFX_ERR_ABORTED;

        return MFX_ERR_NONE;
    }
};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    srand(id);

    MFXInit();

    m_pPar->mfx.GopPicSize = 30;
    m_pPar->mfx.GopRefDist = 4;
    m_pPar->mfx.NumRefFrame = 4;
    m_pPar->mfx.TargetUsage = 4;
    m_pPar->AsyncDepth = 1;
    m_pPar->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    SETPARS(m_pPar, MFXPAR);

    std::vector<mfxExtFeiSliceHeader*> fsh_set;
    mfxU16 n_frames = 10;
    mfxU16 num_fields = m_pPar->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;

    for (mfxU16 i = 0; i < n_frames; i++)
    {
        mfxI16 idc = 0;
        mfxI16 alpha = 0;
        mfxI16 beta = 0;
        if (tc.mode & SLICE_CONST)
        {
            idc = rand() % 3;
            if (idc != 1)
            {
                alpha = rand() % 7 - 6;
                beta = rand() % 7 - 6;
            }
        }
        mfxExtFeiSliceHeader* sh = new mfxExtFeiSliceHeader;
        memset(sh, 0, sizeof(sh));
        sh->Header.BufferId = MFX_EXTBUFF_FEI_SLICE;
        sh->Header.BufferSz = sizeof(mfxExtFeiSliceHeader);

        sh->NumSlice = sh->NumSliceAlloc = m_pPar->mfx.NumSlice;
        sh->Slice = new mfxExtFeiSliceHeader::mfxSlice[m_pPar->mfx.NumSlice];
        memset(sh->Slice, 0, m_pPar->mfx.NumSlice);

        for (mfxU16 slice = 0; slice < m_pPar->mfx.NumSlice; slice++)
        {
            if (tc.mode & RAND)
            {
                idc = rand() % 3;
                if (idc != 1)
                {
                    alpha = rand() % 7 - 6;
                    beta = rand() % 7 - 6;
                }
            }
            if (tc.mode & FRAME_CONST)
            {
                idc = slice % 3;
                if (idc != 1)
                {
                    alpha = slice % 7 - 6;
                    beta = slice % 7;
                }
            }
            sh->Slice[slice].DisableDeblockingFilterIdc = idc;
            sh->Slice[slice].SliceAlphaC0OffsetDiv2     = alpha;
            sh->Slice[slice].SliceBetaOffsetDiv2        = beta;
        }

        fsh_set.push_back(sh);
    }

    ProcessDeblocking pd(num_fields, fsh_set);
    m_filler = &pd;
    m_bs_processor = &pd;

    Init(m_session, m_pPar);
    AllocSurfaces();
    AllocBitstream((m_par.mfx.FrameInfo.Width*m_par.mfx.FrameInfo.Height) * 1024 * 1024 * n_frames);
    SetFrameAllocator(m_session, m_pVAHandle);

    g_tsStatus.expect(tc.sts);

    EncodeFrames(n_frames, true);

    for (std::vector<mfxExtFeiSliceHeader*>::iterator it = fsh_set.begin(); it != fsh_set.end(); it++)
    {
        delete [] (*it)->Slice;
        delete (*it);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_disable_deblocking);
};
