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
#include <random>

namespace hevce_interlace_sei_sps_flags
{

    using namespace BS_HEVC;

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

        int RunTest(unsigned int id);
        static const unsigned int n_cases;

        // See T-REC-H.265-201612 : Table D.2 – Interpretation of pic_struct
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

        // Generates 0 or 1 with probability 0.5
        mfxU32 GetRandomBit()
        {
            std::bernoulli_distribution distr(0.5);
            return distr(m_Gen);
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            mfxStatus sts = m_reader->ProcessSurface(s);

            s.Data.TimeStamp = s.Data.FrameOrder;

            static const mfxU16 FieldStructs[]  = { MFX_PICSTRUCT_FIELD_TOP, MFX_PICSTRUCT_FIELD_BOTTOM };
            static const mfxU16 PairedStructs[] = { MFX_PICSTRUCT_FIELD_PAIRED_NEXT, MFX_PICSTRUCT_FIELD_PAIRED_PREV };

            mfxU32 isBff = (m_pPar->mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF) ? 1 : 0;
            mfxU32 isOdd = s.Data.FrameOrder & 1;

            mfxU32 cur_field_index = (m_generation_mode & DEFAULT)
                ? isOdd ^ isBff    // Default mode - just alter picstructs
                : GetRandomBit();  // Random mode - assign random picstruct


            s.Info.PicStruct = FieldStructs[cur_field_index];

            if (m_generation_mode & PAIRED)
            {
                // In case of PAIRED test decorate picstructs with flag which indicates relations between fields

                mfxU32 cur_paired_idx = isBff ? 1 - cur_field_index : cur_field_index;

                s.Info.PicStruct |= PairedStructs[cur_paired_idx];
            }

            m_assigned_picstructs[s.Data.FrameOrder] = s.Info.PicStruct;

            return sts;
        }


        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

#ifdef DEBUG_TEST
            m_writer->ProcessBitstream(bs, nFrames);
#endif

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

                        EXPECT_NE(m_assigned_picstructs.find(bs.TimeStamp), m_assigned_picstructs.end())
                            << "Invalid TimeStamp in bitstream: " << bs.TimeStamp;

                        EXPECT_EQ(ConvertMFXfieldTypeToHEVC(m_assigned_picstructs[bs.TimeStamp]), pt->pic_struct);
                        m_assigned_picstructs.erase(bs.TimeStamp);

                        EXPECT_EQ(0, pt->source_scan_type);

                        EXPECT_EQ(0, pt->duplicate_flag);

                        sei_found = true;
                    }
                        break;

                    case SPS_NUT:
                        EXPECT_EQ(1, nalu->sps->vui.field_seq_flag);
                        EXPECT_EQ(1, nalu->sps->vui.frame_field_info_present_flag);
                        m_sps_checked = true;
                        break;
                    }
                }

                EXPECT_EQ(true, sei_found) << "SEI wasn't found in stream";
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
            DEFAULT = 1,
            RANDOM,
            PAIRED = 0x100
        };

        struct tc_struct
        {
            mfxStatus sts;

            mfxU32 mode;

            struct
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];

        tsRawReader *m_reader;

#ifdef DEBUG_TEST
        tsBitstreamWriter* m_writer;
#endif

        mfxU32 m_generation_mode = DEFAULT;

        bool m_sps_checked = false;

        std::map<mfxU32, mfxU16> m_assigned_picstructs;

        std::mt19937 m_Gen; // random number generator
    };

#define mfx_PicStruct  tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct
#define mfx_GopPicSize tsStruct::mfxVideoParam.mfx.GopPicSize
#define mfx_GopRefDist tsStruct::mfxVideoParam.mfx.GopRefDist
#define mfx_PRefType tsStruct::mfxExtCodingOption3.PRefType
#define mfx_BRefType tsStruct::mfxExtCodingOption2.BRefType

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, DEFAULT,          {{ MFX_PAR,  &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                                 { MFX_PAR,  &mfx_GopPicSize, 30 },
                                                 { MFX_PAR,  &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*01*/ MFX_ERR_NONE, DEFAULT,          {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BOTTOM },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*02*/ MFX_ERR_NONE, RANDOM,           {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*03*/ MFX_ERR_NONE, RANDOM,           {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BOTTOM },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*04*/ MFX_ERR_NONE, DEFAULT | PAIRED, {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*05*/ MFX_ERR_NONE, DEFAULT | PAIRED, {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BOTTOM },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*06*/ MFX_ERR_NONE, RANDOM | PAIRED,  {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*07*/ MFX_ERR_NONE, RANDOM | PAIRED,  {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BOTTOM },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*08*/ MFX_ERR_NONE, DEFAULT,          {{ MFX_PAR,  &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                                 { MFX_PAR,  &mfx_GopPicSize, 30 },
                                                 { MFX_PAR,  &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_PYRAMID },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*09*/ MFX_ERR_NONE, DEFAULT,          {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BOTTOM },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_PYRAMID },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*10*/ MFX_ERR_NONE, RANDOM,           {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_PYRAMID },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*11*/ MFX_ERR_NONE, RANDOM,           {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BOTTOM },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_PYRAMID },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*12*/ MFX_ERR_NONE, DEFAULT | PAIRED, {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_PYRAMID },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*13*/ MFX_ERR_NONE, DEFAULT | PAIRED, {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BOTTOM },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_PYRAMID },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*14*/ MFX_ERR_NONE, RANDOM | PAIRED,  {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_PYRAMID },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*15*/ MFX_ERR_NONE, RANDOM | PAIRED,  {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BOTTOM },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 1 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_PYRAMID },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_OFF }}},

        {/*16*/ MFX_ERR_NONE, DEFAULT,          {{ MFX_PAR,  &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                                 { MFX_PAR,  &mfx_GopPicSize, 30 },
                                                 { MFX_PAR,  &mfx_GopRefDist, 5 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_PYRAMID }}},

        {/*17*/ MFX_ERR_NONE, DEFAULT,          {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BOTTOM },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 5 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_PYRAMID }}},

        {/*18*/ MFX_ERR_NONE, RANDOM,           {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 5 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_PYRAMID }}},

        {/*19*/ MFX_ERR_NONE, RANDOM,           {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BOTTOM },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 5 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_PYRAMID }}},

        {/*20*/ MFX_ERR_NONE, DEFAULT | PAIRED, {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 5 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_PYRAMID }}},

        {/*21*/ MFX_ERR_NONE, DEFAULT | PAIRED, {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BOTTOM },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 5 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_PYRAMID }}},

        {/*22*/ MFX_ERR_NONE, RANDOM | PAIRED,  {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 5 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_PYRAMID }}},

        {/*23*/ MFX_ERR_NONE, RANDOM | PAIRED,  {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BOTTOM },
                                                 { MFX_PAR, &mfx_GopPicSize, 30 },
                                                 { MFX_PAR, &mfx_GopRefDist, 5 },
                                                 { EXT_COD3, &mfx_PRefType, MFX_P_REF_SIMPLE },
                                                 { EXT_COD2, &mfx_BRefType, MFX_B_REF_PYRAMID }}}
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const int frameNumber = 30;

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];

        m_generation_mode = tc.mode;

        // Set parameters for mfxVideoParam
        SETPARS(m_pPar, MFX_PAR);
        m_pPar->AsyncDepth = 1;
        m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP;

        // Set CodingOption2 parameters
        mfxExtCodingOption2& CO2 = m_par;
        SETPARS(&CO2, EXT_COD2);

        // Set CodingOption3 parameters
        mfxExtCodingOption3& CO3 = m_par;
        SETPARS(&CO3, EXT_COD3);

        ///////////////////////////////////////////////////////////////////////////
        g_tsStatus.expect(tc.sts);

        MFXInit();

        g_tsPlugin.Reg(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_HW);
        m_uid = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC);
        m_loaded = false;
        Load();

        // ENCODE frames
        EncodeFrames(frameNumber);
        Close();

        EXPECT_EQ(true, m_sps_checked) << "SPS wasn't found in stream";

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_interlace_sei_sps_flags);
};