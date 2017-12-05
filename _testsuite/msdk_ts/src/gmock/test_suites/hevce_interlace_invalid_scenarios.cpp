/******************************************************************************* *\

Copyright (C) 2017 Intel Corporation.  All rights reserved.

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
#include <random>

namespace hevce_interlace_invalid_scenarios
{

    using namespace BS_HEVC;

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

    class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserHEVCAU
    {
    public:
        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
            , tsParserHEVCAU(INIT_MODE_DEFAULT)
            , m_reader(NULL)
        {
            m_filler       = this;
            m_bs_processor = this;

            m_pPar->mfx.FrameInfo.Width  = m_pPar->mfx.FrameInfo.CropW = 720;
            m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 480;
            m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_pPar->mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;

            m_reader = new tsRawReader(g_tsStreamPool.Get("YUV/iceage_720x480_491.yuv"),
                m_pPar->mfx.FrameInfo);
            g_tsStreamPool.Reg();

#ifdef DEBUG_TEST
            m_writer = new tsBitstreamWriter("debug_stream.265");
#endif

            m_Gen.seed(0);
        }

        ~TestSuite()
        {
            delete m_reader;

#ifdef DEBUG_TEST
            delete m_writer;
#endif
        }

        template<mfxU32 uid_id>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(unsigned int id, mfxPluginUID uid);
        static const unsigned int n_cases;

        // Generates 0 or 1 with probability 0.5
        mfxU32 GetRandomBit()
        {
            std::bernoulli_distribution distr(0.5);
            return distr(m_Gen);
        }

        // Returns random number in range [min, max]
        mfxU32 GetRandomNumber(mfxU32 min, mfxU32 max)
        {
            std::uniform_int_distribution<mfxU32> distr(min, max);
            return distr(m_Gen);
        }

        // Returns +1 or -1 with probability 0.5
        mfxI32 GetRandomSign()
        {
            return GetRandomBit() ? -1 : 1;
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            mfxStatus sts = m_reader->ProcessSurface(s);

            static const mfxU16 FieldStructs[] = { MFX_PICSTRUCT_FIELD_TOP, MFX_PICSTRUCT_FIELD_BOTTOM };
            static const mfxU16 Decorators[] = { MFX_PICSTRUCT_FRAME_DOUBLING, MFX_PICSTRUCT_FRAME_TRIPLING };

            mfxU32 isBff = (m_pPar->mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF) ? 1 : 0;
            mfxU32 isOdd = s.Data.FrameOrder & 1;

            mfxU32 cur_field_index = isOdd ^ isBff;

            s.Info.PicStruct = FieldStructs[cur_field_index];

            switch (m_test_mode)
            {
            case INVALID_PICSTRUCT:

                s.Data.TimeStamp = s.Data.FrameOrder;

                if (GetRandomBit())
                    s.Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

                if (GetRandomBit())
                    s.Info.PicStruct |= Decorators[GetRandomBit()];

                break;

            case INVALID_FRAMEORDER:

                if (s.Data.FrameOrder > 4 && isOdd)
                {
                    s.Data.FrameOrder = MFX_FRAMEORDER_UNKNOWN;

                    m_sync_status = MFX_ERR_UNDEFINED_BEHAVIOR;
                }
                break;

            case INVALID_FRAMEORDER_RANDOM:

                if (s.Data.FrameOrder > 4 && isOdd)
                {
                    s.Data.FrameOrder = GetRandomBit() ?
                        GetRandomNumber(0, s.Data.FrameOrder - 1) :                                  // From Past
                        GetRandomNumber(s.Data.FrameOrder + m_pPar->mfx.GopRefDist + 1, 0xfffffffe); // From Future

                    m_sync_status = MFX_ERR_UNDEFINED_BEHAVIOR;
                }
                break;

            case INVALID_RESOLUTION:

                if (s.Data.FrameOrder > 4 && isOdd)
                {
                    s.Info.Width  = s.Info.CropW = m_pPar->mfx.FrameInfo.Width  - GetRandomNumber(1, 30);

                    s.Info.Height = s.Info.CropH = m_pPar->mfx.FrameInfo.Height - GetRandomNumber(1, 30);

                    m_submit_status = MFX_ERR_INVALID_VIDEO_PARAM;
                }
                break;

            default:
                break;
            }

            return sts;
        }

        // See T-REC-H.265-201612 : Table D.2 - Interpretation of pic_struct
        mfxU16 ConvertMFXfieldTypeToHEVC(mfxU16 ft)
        {
            switch (ft)
            {
            case MFX_PICSTRUCT_FIELD_TOP:
                return 1;

            case MFX_PICSTRUCT_FIELD_BOTTOM:
                return 2;

            case MFX_PICSTRUCT_FIELD_TOP | MFX_PICSTRUCT_FIELD_PAIRED_PREV:
                return 9;

            case MFX_PICSTRUCT_FIELD_BOTTOM | MFX_PICSTRUCT_FIELD_PAIRED_PREV:
                return 10;

            case MFX_PICSTRUCT_FIELD_TOP | MFX_PICSTRUCT_FIELD_PAIRED_NEXT:
                return 11;

            case MFX_PICSTRUCT_FIELD_BOTTOM | MFX_PICSTRUCT_FIELD_PAIRED_NEXT:
                return 12;

            default:
                return 0xffff;
            }
        }


        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
#ifdef DEBUG_TEST
            m_writer->ProcessBitstream(bs, nFrames);
#endif
            if (m_test_mode == INVALID_PICSTRUCT)
            {
                set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

                while (nFrames--)
                {
                    UnitType& au = ParseOrDie();

                    bool sei_found = false;

                    for (mfxU32 au_ind = 0; au_ind < au.NumUnits; ++au_ind)
                    {
                        auto& nalu = au.nalu[au_ind];

                        switch (nalu->nal_unit_type)
                        {
                        case PREFIX_SEI_NUT:
                        {
                            // Search for PicTiming SEI
                            auto it = std::find_if(nalu->sei->message, nalu->sei->message + nalu->sei->NumMessage,
                                [](SEI::MSG& message) { return message.payloadType == 1; });

                            EXPECT_NE(nalu->sei->message + nalu->sei->NumMessage, it) << "PicTiming SEI wasn't found";

                            // Check that this is the only PicTiming SEI
                            EXPECT_EQ(1, std::count_if(nalu->sei->message, nalu->sei->message + nalu->sei->NumMessage,
                                [](SEI::MSG& message) { return message.payloadType == 1; })) << "More than one PicTiming SEI attached";

                            PicTiming* pt = it->pt;

                            // Check that default picstruct assigned to frame
                            bool isOdd = bs.TimeStamp & 1;
                            bool isBff = m_pPar->mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF;

                            mfxU16 expected_picstruct = ConvertMFXfieldTypeToHEVC((isOdd ^ isBff) ? MFX_PICSTRUCT_FIELD_BOTTOM : MFX_PICSTRUCT_FIELD_TOP);

                            EXPECT_EQ(expected_picstruct, pt->pic_struct);

                            EXPECT_EQ(0, pt->source_scan_type);

                            EXPECT_EQ(0, pt->duplicate_flag);

                            sei_found = true;
                        }
                        break;

                        default:
                            break;
                        }
                    }

                    EXPECT_EQ(true, sei_found) << "SEI wasn't found in stream";
                }
            }

            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }

    private:
        enum
        {
            MFX_PAR = 1,
            EXT_COD2,
            EXT_COD3
        };

        enum
        {
            INVALID_PICSTRUCT = 1,
            INVALID_FRAMEORDER,
            INVALID_FRAMEORDER_RANDOM,
            INVALID_RESOLUTION,
            RESET_ON_ODD_PICTURE,
            INIT_PROGR_ASSIGN_INTERLACE
        };

        struct tc_struct
        {
            mfxU32 mode;

            struct
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];

        tsRawReader *m_reader = nullptr;

#ifdef DEBUG_TEST
        tsBitstreamWriter* m_writer = nullptr;
#endif

        mfxU32 m_test_mode = INVALID_PICSTRUCT;

        mfxStatus m_sync_status   = MFX_ERR_NONE;
        mfxStatus m_submit_status = MFX_ERR_NONE;

        std::mt19937 m_Gen; // random number generator
    };

#define mfx_PicStruct  tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct
#define mfx_GopPicSize tsStruct::mfxVideoParam.mfx.GopPicSize
#define mfx_GopRefDist tsStruct::mfxVideoParam.mfx.GopRefDist
#define mfx_PRefType tsStruct::mfxExtCodingOption3.PRefType
#define mfx_BRefType tsStruct::mfxExtCodingOption2.BRefType

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ INVALID_PICSTRUCT,           {{ MFX_PAR,  &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                              { MFX_PAR,  &mfx_GopPicSize, 30 },
                                              { MFX_PAR,  &mfx_GopRefDist, 1 },
                                              { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                              { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*01*/ INVALID_FRAMEORDER,          {{ MFX_PAR,  &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                              { MFX_PAR,  &mfx_GopPicSize, 30 },
                                              { MFX_PAR,  &mfx_GopRefDist, 1 },
                                              { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                              { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*02*/ INVALID_FRAMEORDER_RANDOM,   {{ MFX_PAR,  &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                              { MFX_PAR,  &mfx_GopPicSize, 30 },
                                              { MFX_PAR,  &mfx_GopRefDist, 1 },
                                              { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                              { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*03*/ INVALID_RESOLUTION,          {{ MFX_PAR,  &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                              { MFX_PAR,  &mfx_GopPicSize, 30 },
                                              { MFX_PAR,  &mfx_GopRefDist, 1 },
                                              { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                              { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*04*/ RESET_ON_ODD_PICTURE,        {{ MFX_PAR,  &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                              { MFX_PAR,  &mfx_GopPicSize, 30 },
                                              { MFX_PAR,  &mfx_GopRefDist, 1 },
                                              { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                              { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*05*/ INIT_PROGR_ASSIGN_INTERLACE, {{ MFX_PAR,  &mfx_PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
                                              { MFX_PAR,  &mfx_GopPicSize, 30 },
                                              { MFX_PAR,  &mfx_GopRefDist, 1 },
                                              { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                              { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}}

    };

    const mfxU32 TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const mfxU32 frameNumber = 30;

    template<mfxU32 uid_id>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        switch (uid_id)
        {
        case 1:
            return RunTest(id, MFX_PLUGINID_HEVCE_HW);

        case 2:
            return RunTest(id, MFX_PLUGINID_HEVC_FEI_ENCODE);

        default:
            ADD_FAILURE() << "ERROR: Wrong uid_id " << uid_id;
            throw tsFAIL;
        }
    }

    int TestSuite::RunTest(unsigned int id, mfxPluginUID uid)
    {
        TS_START;

        bool is_HEVCeFEI = memcmp(uid.Data, MFX_PLUGINID_HEVC_FEI_ENCODE.Data, sizeof(mfxU8) * 16) == 0;

        if (is_HEVCeFEI)
        {
            CHECK_HEVC_FEI_SUPPORT();
        }

        const tc_struct& tc = test_case[id];

        m_test_mode = tc.mode;

        // Set parameters for mfxVideoParam
        SETPARS(m_pPar, MFX_PAR);
        m_pPar->AsyncDepth = 1;
        m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        m_pPar->mfx.EncodedOrder      = !!(m_test_mode & INVALID_FRAMEORDER);
        m_bUseDefaultFrameType        = m_pPar->mfx.EncodedOrder;

        // Set CodingOption2 parameters
        mfxExtCodingOption2& CO2 = m_par;
        SETPARS(&CO2, EXT_COD2);

        // Set CodingOption3 parameters
        mfxExtCodingOption3& CO3 = m_par;
        SETPARS(&CO3, EXT_COD3);

        ///////////////////////////////////////////////////////////////////////////
        MFXInit();

        g_tsPlugin.Reg(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC, uid);
        m_uid = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC);
        m_loaded = false;
        Load();

        // In case of FEI ENCODE additional runtime buffer required
        if (is_HEVCeFEI)
        {
            mfxExtFeiHevcEncFrameCtrl& control = m_ctrl;
            SetDefaultsToCtrl(m_ctrl);
        }

        // ENCODE frames
        for (mfxU32 encoded = 0; encoded < frameNumber; ++encoded)
        {
            if ((m_test_mode == RESET_ON_ODD_PICTURE) && (encoded > 4) && (encoded & 1))
            {
                Reset(m_session, m_pPar);
            }

            if (MFX_ERR_MORE_DATA == EncodeFrameAsync())
            {
                continue;
            }
            // Set and check expected status for frame submit
            g_tsStatus.expect(m_submit_status);
            g_tsStatus.check(); TS_CHECK_MFX;

            // Return back MFX_ERR_NONE to expect correct application termination
            g_tsStatus.expect(MFX_ERR_NONE);

            // Break from encoding loop in case of error
            if (m_submit_status != MFX_ERR_NONE)
                break;

            // Set and check expected status for frame sync
            g_tsStatus.expect(m_sync_status);

            SyncOperation();
            TS_CHECK_MFX;

            g_tsStatus.expect(MFX_ERR_NONE);

            if (m_sync_status != MFX_ERR_NONE)
                break;
        }

        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_interlace_invalid_scenarios,     RunTest_Subtype<1>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_fei_interlace_invalid_scenarios, RunTest_Subtype<2>, n_cases);
};
