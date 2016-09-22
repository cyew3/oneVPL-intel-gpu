/******************************************************************************* *\

Copyright (C) 2016 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include "ts_fei_warning.h"

namespace fei_encode_disable_deblocking
{

#define SAFE_DELETE_ARRAY(P) {if (P){ delete [] P; P = NULL; }}
enum
{
    RAND = 1,
    CONST = 1 << 1,       // const parameters for all stream
    SLICE_CONST = 1 << 2, // const parameters for every slice in frame
    FRAME_CONST = 1 << 3  // const parameters for every frame, but different for slices
};

enum
{
    DEFAULT_IDC = 2,
    DEFAULT_ALPHA = -6,
    DEFAULT_BETA = 6
};

class TestSuite : public tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_FEI_FUNCTION_ENCODE, MFX_CODEC_AVC, true) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        MFXPAR = 1,
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
{/*00*/ MFX_ERR_NONE, CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF}},
 /*01*/ MFX_ERR_NONE, CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF}},
 /*02*/ MFX_ERR_NONE, CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}},

 /*03*/ MFX_ERR_NONE, FRAME_CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF}},
 /*04*/ MFX_ERR_NONE, FRAME_CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF}},
 /*05*/ MFX_ERR_NONE, FRAME_CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}},

 /*06*/ MFX_ERR_NONE, SLICE_CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 3},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF}},
 /*07*/ MFX_ERR_NONE, SLICE_CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 3},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF}},
 /*08*/ MFX_ERR_NONE, SLICE_CONST, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 3},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}},
 // random values
 /*09*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 1},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF}},
 /*10*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF}},
 /*11*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 1},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF}},
 /*12*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF}},
 /*13*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 1},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}},
 /*14*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}},
 // cases with system memory
 /*15*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                             {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}},
 /*16*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                             {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}},
 /*17*/ MFX_ERR_NONE, RAND, {{MFXPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                             {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                             {MFXPAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY}},

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class ProcessDeblocking : public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserH264AU
{
    mfxU32 m_nFields;
    mfxU32 m_nSurf;
    mfxU32 m_nFrame;
    mfxVideoParam& m_par;
    mfxEncodeCtrl& m_ctrl;
    mfxU32 m_mode;
    std::vector <mfxExtBuffer*> m_buffs;

public:
    ProcessDeblocking(mfxVideoParam& par, mfxEncodeCtrl& ctrl, mfxU32 mode)
        : tsParserH264AU(), m_nSurf(0), m_nFrame(0), m_par(par), m_ctrl(ctrl), m_mode(mode)
    {
        m_nFields = m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
        set_trace_level(BS_H264_TRACE_LEVEL_SLICE);
    }

    ~ProcessDeblocking()
    {
        for (std::vector<mfxExtBuffer*>::iterator it = m_buffs.begin(); it != m_buffs.end(); /*it++*/)
        {
            switch ((*it)->BufferId)
            {
            case MFX_EXTBUFF_FEI_ENC_CTRL:
                if (*it){
                    delete[](mfxExtFeiEncFrameCtrl*)(*it);
                    *it = NULL;
                }
                for (mfxU32 i = 0; i < m_nFields; i++){
                    ++it;
                }
                break;

            case MFX_EXTBUFF_FEI_SLICE:
                for (mfxU32 i = 0; i < m_nFields; i++){
                    SAFE_DELETE_ARRAY(((mfxExtFeiSliceHeader*)(*it) + i)->Slice);
                }

                if (*it){
                    delete[](mfxExtFeiSliceHeader*)(*it);
                    *it = NULL;
                }
                for (mfxU32 i = 0; i < m_nFields; i++){
                    ++it;
                }
                break;

            default:
                ++it;
            }
        }
    }

    mfxStatus PrepareCtrlBuf(mfxExtFeiEncFrameCtrl* & fei_encode_ctrl)
    {
        fei_encode_ctrl = new mfxExtFeiEncFrameCtrl[m_nFields];
        memset(fei_encode_ctrl, 0, sizeof(*fei_encode_ctrl)*m_nFields);

        for (mfxU32 field = 0; field < m_nFields; field++)
        {
            fei_encode_ctrl[field].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
            fei_encode_ctrl[field].Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);

            fei_encode_ctrl[field].SearchPath             = 2;
            fei_encode_ctrl[field].LenSP                  = 57;
            fei_encode_ctrl[field].SubMBPartMask          = 0;
            fei_encode_ctrl[field].MultiPredL0            = 0;
            fei_encode_ctrl[field].MultiPredL1            = 0;
            fei_encode_ctrl[field].SubPelMode             = 0x03;
            fei_encode_ctrl[field].InterSAD               = 0;
            fei_encode_ctrl[field].IntraSAD               = 0;
            fei_encode_ctrl[field].IntraPartMask          = 0;
            fei_encode_ctrl[field].DistortionType         = 0;
            fei_encode_ctrl[field].RepartitionCheckEnable = 0;
            fei_encode_ctrl[field].AdaptiveSearch         = 0;
            fei_encode_ctrl[field].MVPredictor            = 0;
            fei_encode_ctrl[field].NumMVPredictors[0]        = 0;
            fei_encode_ctrl[field].PerMBQp                = 26;
            fei_encode_ctrl[field].PerMBInput             = 0;
            fei_encode_ctrl[field].MBSizeCtrl             = 0;
            fei_encode_ctrl[field].ColocatedMbDistortion  = 0;
            fei_encode_ctrl[field].RefHeight              = 32;
            fei_encode_ctrl[field].RefWidth               = 32;
            fei_encode_ctrl[field].SearchWindow           = 5;
        }

        return MFX_ERR_NONE;
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxI16 idc   = 0;
        mfxI16 alpha = 0;
        mfxI16 beta  = 0;

        if (!(m_mode & CONST))
        {
            if (m_mode & SLICE_CONST)
            {
                idc = rand() % 3;
                alpha = idc == 1 ? 0 : rand() % 7 - 6;
                beta  = idc == 1 ? 0 : rand() % 7 - 6;
            }

            mfxExtFeiEncFrameCtrl* fei_encode_ctrl = NULL;
            PrepareCtrlBuf(fei_encode_ctrl);
            for (mfxU32 f = 0; f < m_nFields; f++)
            {
                m_buffs.push_back((mfxExtBuffer*)(fei_encode_ctrl + f));
            }

            mfxExtFeiSliceHeader* fsh = new mfxExtFeiSliceHeader[m_nFields];
            memset(fsh, 0, m_nFields * sizeof(*fsh));
            for (mfxU32 f = 0; f < m_nFields; f++)
            {
                fsh[f].Header.BufferId = MFX_EXTBUFF_FEI_SLICE;
                fsh[f].Header.BufferSz = sizeof(mfxExtFeiSliceHeader);

                fsh[f].NumSlice = fsh[f].NumSliceAlloc = m_par.mfx.NumSlice;
                fsh[f].Slice = new mfxExtFeiSliceHeader::mfxSlice[m_par.mfx.NumSlice];
                memset(fsh[f].Slice, 0, m_par.mfx.NumSlice*sizeof(mfxExtFeiSliceHeader::mfxSlice));

                if (m_mode & RAND)
                {
                    idc = rand() % 3;
                    alpha = idc == 1 ? 0 : rand() % 7 - 6;
                    beta = idc == 1 ? 0 : rand() % 7 - 6;
                }

                for (mfxU16 s = 0; s < m_par.mfx.NumSlice; s++)
                {
                    if (m_mode & FRAME_CONST)
                    {
                        idc = s % 3;
                        alpha = idc == 1 ? 0 : s % 7 - 6;
                        beta  = idc == 1 ? 0 : s % 7 - 6;
                    }

                    fsh[f].Slice[s].DisableDeblockingFilterIdc = idc;
                    fsh[f].Slice[s].SliceAlphaC0OffsetDiv2     = alpha;
                    fsh[f].Slice[s].SliceBetaOffsetDiv2        = beta;
                }

                m_buffs.push_back((mfxExtBuffer*)(fsh+f));
            }

            /*mfxExtBuffer** tmp = new mfxExtBuffer*[4];
            for (int j = 0; j < 4; j++){
                tmp[j] = m_buffs[2 * m_nSurf*m_nFields + j];
            }*/

            m_ctrl.NumExtParam = 2*m_nFields; // feiCtrl and feiSliceHeader
            m_ctrl.ExtParam = /*tmp;*/ &m_buffs[2 * m_nSurf*m_nFields];
        }

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

                    // default values which set for all stream
                    Bs32s expected_idc = DEFAULT_IDC;
                    Bs32s expected_alpha = DEFAULT_ALPHA;
                    Bs32s expected_beta = DEFAULT_BETA;
                    if (!(m_mode & CONST))
                    {
                        mfxExtFeiSliceHeader* tmp = (mfxExtFeiSliceHeader*)m_buffs[m_nFrame*m_nFields * 2 + m_nFields + field];
                        expected_idc   = tmp->Slice[nSlice].DisableDeblockingFilterIdc;
                        expected_alpha = tmp->Slice[nSlice].SliceAlphaC0OffsetDiv2;
                        expected_beta  = tmp->Slice[nSlice].SliceBetaOffsetDiv2;
                    }

                    Bs32s idc   = au.NALU[i].slice_hdr->disable_deblocking_filter_idc;
                    Bs32s alpha = au.NALU[i].slice_hdr->slice_alpha_c0_offset_div2;
                    Bs32s beta  = au.NALU[i].slice_hdr->slice_beta_offset_div2;

                    g_tsLog << "DisableDeblockingFilterIdc = " << idc << " (expected = " << expected_idc << ")\n";
                    g_tsLog << "SliceAlphaC0OffsetDiv2 = " << alpha << " (expected = " << expected_alpha << ")\n";
                    g_tsLog << "SliceBetaOffsetDiv2 = " << beta << " (expected = " << expected_beta << ")\n";

                    if ( idc != expected_idc)
                    {
                        g_tsLog << "ERROR: DisableDeblockingFilterIdc in stream != expected\n";
                        err++;
                    }
                    if (alpha != expected_alpha && expected_idc != 1) // expected idc means disabled deblocking, no alpha and beta param would be packed to stream
                    {
                        g_tsLog << "ERROR: SliceAlphaC0OffsetDiv2 in stream != expected\n";
                        err++;
                    }
                    if (beta != expected_beta && expected_idc != 1)
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

    CHECK_FEI_SUPPORT();

    const tc_struct& tc = test_case[id];
    srand(id);

    MFXInit();

    memset(&m_ctrl, 0, sizeof(m_ctrl));
    m_ctrl.FrameType = MFX_FRAMETYPE_UNKNOWN;
    m_ctrl.QP = 26;

    m_pPar->mfx.GopPicSize = 30;
    m_pPar->mfx.GopRefDist  = 1; // to exclude
    m_pPar->mfx.NumRefFrame = 2; // frame reordering
    m_pPar->mfx.TargetUsage = 4;
    m_pPar->AsyncDepth = 1;
    m_pPar->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    mfxU16 n_frames = 10;

    SETPARS(m_pPar, MFXPAR);

    std::vector<mfxExtBuffer*> in_buffs;
    mfxU16 num_fields = m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
    if (tc.mode & CONST)
    {
        mfxExtFeiSliceHeader* fsh = new mfxExtFeiSliceHeader[num_fields];

        mfxExtFeiEncFrameCtrl* fei_encode_ctrl = NULL;
        {
            fei_encode_ctrl = new mfxExtFeiEncFrameCtrl[num_fields];
            memset(fei_encode_ctrl, 0, sizeof(*fei_encode_ctrl)*num_fields);

            for (mfxU32 field = 0; field < num_fields; field++)
            {
                fei_encode_ctrl[field].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
                fei_encode_ctrl[field].Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);

                fei_encode_ctrl[field].SearchPath = 2;
                fei_encode_ctrl[field].LenSP = 57;
                fei_encode_ctrl[field].SubMBPartMask = 0;
                fei_encode_ctrl[field].MultiPredL0 = 0;
                fei_encode_ctrl[field].MultiPredL1 = 0;
                fei_encode_ctrl[field].SubPelMode = 0x03;
                fei_encode_ctrl[field].InterSAD = 0;
                fei_encode_ctrl[field].IntraSAD = 0;
                fei_encode_ctrl[field].IntraPartMask = 0;
                fei_encode_ctrl[field].DistortionType = 0;
                fei_encode_ctrl[field].RepartitionCheckEnable = 0;
                fei_encode_ctrl[field].AdaptiveSearch = 0;
                fei_encode_ctrl[field].MVPredictor = 0;
                fei_encode_ctrl[field].NumMVPredictors[0] = 0;
                fei_encode_ctrl[field].PerMBQp = 26;
                fei_encode_ctrl[field].PerMBInput = 0;
                fei_encode_ctrl[field].MBSizeCtrl = 0;
                fei_encode_ctrl[field].ColocatedMbDistortion = 0;
                fei_encode_ctrl[field].RefHeight = 32;
                fei_encode_ctrl[field].RefWidth = 32;
                fei_encode_ctrl[field].SearchWindow = 5;
            }
        }
        for (mfxU32 f = 0; f < num_fields; f++)
        {
            in_buffs.push_back((mfxExtBuffer*)(fei_encode_ctrl + f));
        }

        memset(fsh, 0, num_fields * sizeof(*fsh));
        for (mfxU16 f = 0; f<num_fields; f++)
        {
            fsh[f].Header.BufferId = MFX_EXTBUFF_FEI_SLICE;
            fsh[f].Header.BufferSz = sizeof(mfxExtFeiSliceHeader);

            fsh[f].NumSlice = fsh[f].NumSliceAlloc = m_pPar->mfx.NumSlice;
            fsh[f].Slice = new mfxExtFeiSliceHeader::mfxSlice[m_pPar->mfx.NumSlice];
            memset(fsh[f].Slice, 0, m_pPar->mfx.NumSlice * sizeof(mfxExtFeiSliceHeader::mfxSlice));

            for (mfxU16 s = 0; s < m_pPar->mfx.NumSlice; s++)
            {
                fsh[f].Slice[s].DisableDeblockingFilterIdc = DEFAULT_IDC;
                fsh[f].Slice[s].SliceAlphaC0OffsetDiv2     = DEFAULT_ALPHA;
                fsh[f].Slice[s].SliceBetaOffsetDiv2        = DEFAULT_BETA;
            }

            in_buffs.push_back((mfxExtBuffer *)&fsh[f]);
        }

        m_pCtrl->ExtParam    = &in_buffs[0];
        m_pCtrl->NumExtParam = (mfxU16)in_buffs.size();
    }

    ProcessDeblocking pd(m_par, m_ctrl, tc.mode);
    m_filler = &pd;
    m_bs_processor = &pd;

    if (m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
        SetFrameAllocator(m_session, m_pVAHandle);

    Init(m_session, m_pPar);
    AllocSurfaces();
    AllocBitstream((m_par.mfx.FrameInfo.Width*m_par.mfx.FrameInfo.Height) * 1024 * 1024 * n_frames);

    g_tsStatus.expect(tc.sts);

    EncodeFrames(n_frames, true);

    for (std::vector<mfxExtBuffer*>::iterator it = in_buffs.begin(); it != in_buffs.end(); /*it++*/)
    {
        switch ((*it)->BufferId)
        {
        case MFX_EXTBUFF_FEI_ENC_CTRL:
            if (*it){
                delete[](mfxExtFeiEncFrameCtrl*)(*it);
                *it = NULL;
            }
            for (mfxU32 i = 0; i < num_fields; i++){
                ++it;
            }
            break;

        case MFX_EXTBUFF_FEI_SLICE:
            for (mfxU32 i = 0; i < num_fields; i++){
                SAFE_DELETE_ARRAY(((mfxExtFeiSliceHeader*)(*it) + i)->Slice);
            }

            if (*it){
                delete[](mfxExtFeiSliceHeader*)(*it);
                *it = NULL;
            }
            for (mfxU32 i = 0; i < num_fields; i++){
                ++it;
            }
            break;

        default:
            ++it;
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_disable_deblocking);
};
