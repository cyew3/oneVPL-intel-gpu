/******************************************************************************* *\

Copyright (C) 2018 Intel Corporation.  All rights reserved.

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
#include "ts_fei_warning.h"
#include "ts_parser.h"
#include "fei_buffer_allocator.h"

namespace hevce_fei_ctu_force_flags
{

/* Description:
 *
 * This test checks whether setting the per-CTU force flags (passed
 * to the driver via an extension buffer) leads to the CTUs in question
 * being correctly encoded (in accordance with the set force flags).
 */

typedef enum
{
    FF_NONE = 0,
    FF_INTRA = 1,
    FF_INTER = 2,
    FF_INTRAINTER = 3,
    NUM_FLAG_TYPES = 4,
    INVALID_FLAG_TYPE = -1,
} ForceFlagType;

typedef enum
{
    CU_SKIP = 0,
    CU_INTRA = 1,
    CU_INTER = 2,
    INVALID_CU_TYPE = -1,
} CUType;


typedef enum
{
    PATTERN, RANDOM
} TestMode;

typedef enum
{
    HORIZONTAL, VERTICAL, RASTER
} PatternType;

typedef enum
{
    INTRA_FIRST, INTER_FIRST, INTRAINTER_FIRST //Sequences with SKIP to be added
} FlagSequenceType;

class FlagSequencer
{
public:
    FlagSequencer(mfxU32 width_in_ctu, mfxU32 height_in_ctu, FlagSequenceType seq_type,
                  PatternType pattern_type, mfxU32 step):
                  m_width_in_ctu(width_in_ctu),
                  m_height_in_ctu(height_in_ctu),
                  m_seq_type(seq_type),
                  m_pattern_type(pattern_type),
                  m_step(step)
    {
        m_sequence = m_sequence_map[seq_type];
        m_seq_iter = m_sequence.begin();
        if (m_seq_iter == m_sequence.end())
        {
            throw std::string("ERROR: empty sequence during sequencer construction");
        }
    }
    void Reset()
    {
        m_ctu_coord_x = 0;
        m_ctu_coord_y = 0;
        m_ctu_raster_order = 0;
        m_seq_iter = m_sequence.begin();
    }
    ForceFlagType GetNextFlagType()
    {
        m_ctu_coord_x = m_ctu_raster_order % m_width_in_ctu;
        m_ctu_coord_y = m_ctu_raster_order / m_width_in_ctu;
        switch (m_pattern_type)
        {
        case HORIZONTAL:
            if ((m_ctu_coord_x == 0) && !(m_ctu_coord_y % m_step) && m_ctu_coord_y != 0)
            {
                m_seq_iter = AdvanceSequenceIterator();
            }
            break;
        case VERTICAL:
            if (m_ctu_coord_x == 0)
            {
                m_seq_iter = m_sequence.begin();
            }
            else if (!(m_ctu_coord_x % m_step))
            {
                m_seq_iter = AdvanceSequenceIterator();
            }
            break;
        case RASTER:
            if (!(m_ctu_raster_order % m_step) && m_ctu_raster_order != 0)
            {
                m_seq_iter = AdvanceSequenceIterator();
            }
            break;
        }

        m_ctu_raster_order++;
        return *m_seq_iter;
    }
    static std::map<FlagSequenceType, std::vector<ForceFlagType>> m_sequence_map;

    private:
        std::vector<ForceFlagType>::iterator AdvanceSequenceIterator()
        {
            auto it = m_seq_iter + 1;
            if (it == m_sequence.end())
            {
                it = m_sequence.begin();
            }
            return it;
        }

        mfxU32 m_width_in_ctu = 0;
        mfxU32 m_height_in_ctu = 0;
        mfxU32 m_ctu_raster_order = 0;
        FlagSequenceType m_seq_type = INTRA_FIRST;
        PatternType m_pattern_type = HORIZONTAL;

        mfxU32 m_ctu_coord_x = 0;
        mfxU32 m_ctu_coord_y = 0;

        mfxU32 m_step = 0;

        std::vector<ForceFlagType> m_sequence;
        std::vector<ForceFlagType>::iterator m_seq_iter = m_sequence.begin();
    };

std::map<FlagSequenceType, std::vector<ForceFlagType>> FlagSequencer::m_sequence_map = {
                   {INTRA_FIRST, std::vector<ForceFlagType>({FF_INTRA, FF_INTER})},
                   {INTER_FIRST, std::vector<ForceFlagType>({FF_INTER, FF_INTRA})},
                   {INTRAINTER_FIRST, std::vector<ForceFlagType>({FF_INTRAINTER, FF_INTER})},
       };



class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserHEVCAU
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
        , tsParserHEVCAU(BS_HEVC::INIT_MODE_CABAC)
        , m_ctu_size(32)
    {
        m_filler       = this;
        m_bs_processor = this;

        m_pPar->mfx.FrameInfo.Width        = m_pPar->mfx.FrameInfo.CropW = 352;
        m_pPar->mfx.FrameInfo.Height       = m_pPar->mfx.FrameInfo.CropH = 288;
        m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_pPar->mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        m_pPar->AsyncDepth                 = 1;                   //limitation for FEI
        m_pPar->mfx.RateControlMethod      = MFX_RATECONTROL_CQP; //For now FEI work with CQP only

        m_request.Width   = m_pPar->mfx.FrameInfo.Width;
        m_request.Height  = m_pPar->mfx.FrameInfo.Height;
        m_request.CTUSize = m_ctu_size;

        m_width_in_ctu  = (m_pPar->mfx.FrameInfo.Width  + m_ctu_size - 1) / m_ctu_size;
        m_height_in_ctu = (m_pPar->mfx.FrameInfo.Height + m_ctu_size - 1) / m_ctu_size;

        m_reader.reset(new tsRawReader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"),
                                   m_pPar->mfx.FrameInfo));
        g_tsStreamPool.Reg();

        m_Gen.seed(0);

    }

    ~TestSuite()
    {
        for (auto& map_entry : m_map_ctrl)
        {
            mfxExtFeiHevcEncCtuCtrl& hevcFeiEncCtuCtrl = map_entry.second.ctrl;
            m_hevcFeiAllocator->Free(&hevcFeiEncCtuCtrl);
        }
    }

    std::string GetForceFlagTypeString(ForceFlagType type)
    {
        switch (type)
        {
        case FF_NONE:
            return "FF_NONE";
        case FF_INTRA:
            return "FF_INTRA";
        case FF_INTER:
            return "FF_INTER";
        case FF_INTRAINTER:
            return "FF_INTRAINTER";
        default:
            return "INVALID_FLAG_TYPE";
        }
    }

    std::string GetExpectedCUTypeStringByFlag(ForceFlagType type)
    {
        switch (type)
        {
        case FF_NONE:
            return "ANY";
        case FF_INTRA:
            return "INTRA";
        case FF_INTER:
            return "INTER";
        case FF_INTRAINTER:
            return "INTER";
        default:
            return "INVALID_FLAG_TYPE";
        }
    }

    std::string GetCUTypeString(CUType cu)
    {
        switch (cu)
        {
        case CU_SKIP:
            return "SKIP";
        case CU_INTRA:
            return "INTRA";
        case CU_INTER:
            return "INTER";
        default:
            return "INVALID_CU_TYPE";
        }
    }


    void SetDefaultsToCtrl(mfxExtFeiHevcEncFrameCtrl& ctrl)
    {
        memset(&ctrl, 0, sizeof(ctrl));

        ctrl.Header.BufferId = MFX_EXTBUFF_HEVCFEI_ENC_CTRL;
        ctrl.Header.BufferSz = sizeof(mfxExtFeiHevcEncFrameCtrl);
        ctrl.SubPelMode         = 3; // quarter-pixel motion estimation
        ctrl.SearchWindow       = 5; // 48 SUs 48x40 window full search
        ctrl.NumFramePartitions = 4; // number of partitions in frame that encoder processes concurrently
        // enable internal L0/L1 predictors: 1 - spatial predictors
        ctrl.MultiPred[0] = ctrl.MultiPred[1] = 1;
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;


    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxStatus sts = m_reader->ProcessSurface(s);

        s.Data.TimeStamp = s.Data.FrameOrder;

        Ctrl& ctrl = m_map_ctrl[s.Data.FrameOrder];
        ctrl.buf.resize(m_width_in_ctu * m_height_in_ctu);
        std::fill(ctrl.buf.begin(), ctrl.buf.end(), FF_NONE);

        mfxExtFeiHevcEncFrameCtrl& feiCtrl = ctrl.ctrl;
        SetDefaultsToCtrl(feiCtrl);
        feiCtrl.PerCtuInput = 1;

        mfxExtFeiHevcEncCtuCtrl& hevcFeiEncCtuCtrl = ctrl.ctrl;
        m_hevcFeiAllocator->Free(&hevcFeiEncCtuCtrl);
        m_hevcFeiAllocator->Alloc(&hevcFeiEncCtuCtrl, m_request);

        AutoBufferLocker<mfxExtFeiHevcEncCtuCtrl> auto_locker(*m_hevcFeiAllocator, hevcFeiEncCtuCtrl);

        ForceFlagType type = INVALID_FLAG_TYPE;

        FlagSequencer sequencer(m_width_in_ctu, m_height_in_ctu, m_flag_sequence_type,
                                m_pattern_type, m_step);

        std::uniform_int_distribution<mfxU32> distr(0, NUM_FLAG_TYPES - 1);
        for (mfxU32 i = 0; i < m_height_in_ctu; i++)
        {
            for (mfxU32 j = 0; j < m_width_in_ctu; j++)
            {
                mfxU32 curr_ctu_num = i * m_width_in_ctu + j;
                switch (m_mode)
                {
                case PATTERN:
                    type = sequencer.GetNextFlagType();
                    break;
                case RANDOM:
                    type = (ForceFlagType) distr(m_Gen);
                    break;
                default:
                    break;
                }
                hevcFeiEncCtuCtrl.Data[curr_ctu_num] = GetCtuCtrlByFlagType(type);
                ctrl.buf[curr_ctu_num] = type;
            }
        }

        ShallowCopy(static_cast<mfxEncodeCtrl&>(m_ctrl), static_cast<mfxEncodeCtrl&>(ctrl.ctrl));

        return sts;
    }

    void ShallowCopy(mfxEncodeCtrl& dst, const mfxEncodeCtrl& src)
    {
        dst = src;
    }

    mfxFeiHevcEncCtuCtrl GetCtuCtrlByFlagType(ForceFlagType type)
    {
        mfxFeiHevcEncCtuCtrl ret_ctrl = {0};
        switch (type)
        {
        case FF_INTRA:
            ret_ctrl.ForceToIntra = 1;
            ret_ctrl.ForceToInter = 0;
            break;
        case FF_INTER:
            ret_ctrl.ForceToIntra = 0;
            ret_ctrl.ForceToInter = 1;
            break;
        case FF_INTRAINTER:
            ret_ctrl.ForceToIntra = 1;
            ret_ctrl.ForceToInter = 1;
            break;
        default:
            break;
        }
        return ret_ctrl;
    }

    void GetCUTypes(std::vector<CUType>& types, const BS_HEVC::CQT & cqt)
    {
        if (cqt.split_cu_flag)
        {
            GetCUTypes(types, cqt.split[0]);
            GetCUTypes(types, cqt.split[1]);
            GetCUTypes(types, cqt.split[2]);
            GetCUTypes(types, cqt.split[3]);
            return;
        }
        else
        {
            types.push_back(GetCUTypeFromCQT(cqt));
        }
    }

    CUType GetCUTypeFromCQT(const BS_HEVC::CQT & cqt)
    {
        if (cqt.cu_skip_flag)
        {
            return CU_SKIP;
        }
        else if (cqt.pred_mode_flag == 0)
        {
            return CU_INTER;
        }
        else if (cqt.pred_mode_flag == 1)
        {
            return CU_INTRA;
        }
        else
        {
            return INVALID_CU_TYPE;
        }
    }


    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

        while (nFrames--)
        {
            UnitType& au = ParseOrDie();
            auto& pic = *au.pic;
            for (mfxU32 i = 0; i < au.NumUnits; i++)
            {
                mfxU16 type = au.nalu[i]->nal_unit_type;
                if (!isNALUNonIRAPSlice(type))
                {
                    continue; // Only check B and P slices
                }

                mfxU32 counter = 0;
                for (BS_HEVC::CTU* cu = pic.CuInRs; cu < pic.CuInRs + pic.NumCU; cu++)
                {
                    ForceFlagType expected_type = m_map_ctrl[bs.TimeStamp].buf[counter];
                    if (expected_type == FF_NONE)
                    {
                        continue;
                    }
                    std::vector<CUType> cu_types;
                    GetCUTypes(cu_types, cu->cqt);
                    for (size_t i = 0; i < cu_types.size(); i++)
                    {
                        std::string force_flags_type = GetForceFlagTypeString(m_map_ctrl[bs.TimeStamp].buf[counter]);
                        std::string expected_cu_type = GetExpectedCUTypeStringByFlag(m_map_ctrl[bs.TimeStamp].buf[counter]);
                        std::string actual_cu_type = GetCUTypeString(cu_types[i]);
                        bool isPassed = (expected_cu_type == actual_cu_type) ||
                                        (expected_cu_type == "INTER" && actual_cu_type == "SKIP"); // As of now, ForceToInter
                                                                                                   // allows SKIP as well
                        EXPECT_TRUE(isPassed) << "expected " << expected_cu_type
                                    << ", actual " << actual_cu_type
                                    << " for frame #" << bs.TimeStamp
                                    << " , CTU #" << counter
                                    << ", CU #" << i
                                    << " , force flags: " << force_flags_type << "\n";
                    }
                    counter++;
                }
            }
        }
        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    struct Ctrl
    {
        tsExtBufType<mfxEncodeCtrl> ctrl;
        std::vector<ForceFlagType>  buf;
    };

    bool isNALUNonIRAPSlice(mfxU16 nalu_type)
    {
        return (nalu_type < 16);
    }

    struct tc_struct
    {
        tc_struct(TestMode _mode, mfxU32 _ctu_size, FlagSequenceType fst = INTRA_FIRST,
                  PatternType _pattern_type = HORIZONTAL, mfxU32 _step = 0):
                        m_mode(_mode),
                        m_ctu_size(_ctu_size),
                        m_flag_sequence_type(fst),
                        m_pattern_type(_pattern_type),
                        m_step(_step) {}

        TestMode         m_mode;
        mfxU32           m_ctu_size;
        FlagSequenceType m_flag_sequence_type;
        PatternType      m_pattern_type;
        mfxU32           m_step;
    };

    static const tc_struct test_case[];

    std::unique_ptr <tsRawReader>        m_reader;
    std::unique_ptr <FeiBufferAllocator> m_hevcFeiAllocator;

    TestMode         m_mode               = PATTERN;
    mfxU32           m_ctu_size           = 0;
    mfxU32           m_width_in_ctu       = 0;
    mfxU32           m_height_in_ctu      = 0;
    FlagSequenceType m_flag_sequence_type = INTRA_FIRST;
    PatternType      m_pattern_type       = HORIZONTAL;
    mfxU32           m_step               = 0;

    std::mt19937           m_Gen;
    std::map<mfxU32, Ctrl> m_map_ctrl;
    BufferAllocRequest     m_request;

};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ PATTERN, 32, INTRA_FIRST, HORIZONTAL, 1},
    {/*01*/ PATTERN, 32, INTER_FIRST, HORIZONTAL, 1},

    {/*02*/ PATTERN, 32, INTRA_FIRST, VERTICAL, 1},
    {/*03*/ PATTERN, 32, INTER_FIRST, VERTICAL, 1},

    {/*04*/ PATTERN, 32, INTRA_FIRST, RASTER, 1},
    {/*05*/ PATTERN, 32, INTER_FIRST, RASTER, 1},

    {/*06*/ PATTERN, 32, INTRAINTER_FIRST, RASTER, 1},

    {/*07*/ PATTERN, 32, INTRA_FIRST, HORIZONTAL, 2},
    {/*08*/ PATTERN, 32, INTER_FIRST, HORIZONTAL, 2},

    {/*09*/ PATTERN, 32, INTRA_FIRST, VERTICAL, 2},
    {/*10*/ PATTERN, 32, INTER_FIRST, VERTICAL, 2},

    {/*11*/ PATTERN, 32, INTRA_FIRST, RASTER, 2},
    {/*12*/ PATTERN, 32, INTER_FIRST, RASTER, 2},

    {/*13*/ PATTERN, 32, INTRAINTER_FIRST, RASTER, 2},

    {/*14*/ RANDOM, 32},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

const int frameNumber = 30;

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_HEVC_FEI_SUPPORT();

    const tc_struct& tc = test_case[id];
    m_mode               = tc.m_mode;
    m_flag_sequence_type = tc.m_flag_sequence_type;
    m_pattern_type       = tc.m_pattern_type;
    m_step               = tc.m_step;
    m_ctu_size           = tc.m_ctu_size;

    ///////////////////////////////////////////////////////////////////////////

    MFXInit();

    if (m_pPar->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) {
        SetFrameAllocator(m_session, m_pVAHandle);
        m_pFrameAllocator = m_pVAHandle;
    }

    // Create Buffer Allocator for libva buffers
    mfxHDL hdl;
    mfxHandleType type;
    m_pVAHandle->get_hdl(type, hdl);

    m_hevcFeiAllocator.reset(new FeiBufferAllocator((VADisplay) hdl, m_par.mfx.FrameInfo.Width, m_par.mfx.FrameInfo.Height));

    EncodeFrames(frameNumber);

    Close();

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_fei_ctu_force_flags);
};
