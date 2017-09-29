/******************************************************************************* *\

Copyright (C) 2016-2017 Intel Corporation.  All rights reserved.

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

namespace hevce_interlace_sei_sps_flags
{

    using namespace BS_HEVC;

    class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserHEVCAU
    {
    public:
        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
            , tsParserHEVCAU(BS_HEVC::INIT_MODE_CABAC)
            , m_reader(NULL)
        {
            m_filler = this;
            m_bs_processor = this;

            m_pPar->mfx.FrameInfo.Width  = m_pPar->mfx.FrameInfo.CropW = 720;
            m_pPar->mfx.FrameInfo.Height = m_pPar->mfx.FrameInfo.CropH = 480;
            m_pPar->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_pPar->mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;

            m_reader = new tsRawReader(g_tsStreamPool.Get("YUV/iceage_720x480_491.yuv"),
                m_pPar->mfx.FrameInfo);
            g_tsStreamPool.Reg();

        }
        ~TestSuite()
        {
            delete m_reader;
        }

        int RunTest(unsigned int id);
        static const unsigned int n_cases;

        mfxU16 ConvertMFXfieldTypeToHEVC(mfxU16 ft)
        {
            switch (ft)
            {
            case MFX_PICSTRUCT_FIELD_TOP:
                return 1;

            case MFX_PICSTRUCT_FIELD_BOTTOM:
                return 2;

            default:                            // TODO: Extend on repeated fields as well
                return 0xffff;
            }
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            mfxStatus sts = m_reader->ProcessSurface(s);

            static const mfxU16 FieldStructs[] = { MFX_PICSTRUCT_FIELD_TOP, MFX_PICSTRUCT_FIELD_BOTTOM };

            mfxU32 isBff = (m_pPar->mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF) ? 1 : 0;
            mfxU32 cur_field_index = (s.Data.FrameOrder & 1) ^ isBff;

            m_assigned_picstructs.push_back(FieldStructs[cur_field_index]);

            s.Info.PicStruct = FieldStructs[cur_field_index];

            return sts;
        }


        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength);
            //m_ts = bs.TimeStamp;

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

                        EXPECT_EQ(ConvertMFXfieldTypeToHEVC(m_assigned_picstructs.front()), pt->pic_struct);
                        m_assigned_picstructs.pop_front();

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
            MFX_PAR = 1
        };

        struct tc_struct
        {
            mfxStatus sts;

            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];

        tsRawReader *m_reader;

        //mfxU32 m_ts;

        bool m_sps_checked = false;

        std::list<mfxU16> m_assigned_picstructs;
    };

#define mfx_PicStruct  tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct
#define mfx_GopPicSize tsStruct::mfxVideoParam.mfx.GopPicSize
#define mfx_GopRefDist tsStruct::mfxVideoParam.mfx.GopRefDist

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_TOP },
                               { MFX_PAR, &mfx_GopPicSize, 30 },
                               { MFX_PAR, &mfx_GopRefDist, 1 }}},

        {/*01*/ MFX_ERR_NONE, {{ MFX_PAR, &mfx_PicStruct, MFX_PICSTRUCT_FIELD_BOTTOM },
                               { MFX_PAR, &mfx_GopPicSize, 30 },
                               { MFX_PAR, &mfx_GopRefDist, 1 }}}
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    const int frameNumber = 30;

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;


        const tc_struct& tc = test_case[id];

        //set parameters for mfxVideoParam
        SETPARS(m_pPar, MFX_PAR);
        m_pPar->AsyncDepth = 1;
        m_pPar->mfx.RateControlMethod = MFX_RATECONTROL_CQP;

        mfxExtCodingOption& CO = m_par;
        CO.PicTimingSEI = MFX_CODINGOPTION_ON;

        //set parameters for mfxEncodeCtrl
        //SETPARS(m_pCtrl, MFX_FRM_CTRL);

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
        //EXPECT_EQ(true, m_sei_checked) << "SEI wasn't found in stream";

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_interlace_sei_sps_flags);
};