/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include <memory>

#define VP9E_CQP_DEFAULT_QPI (169)
#define VP9E_CQP_DEFAULT_QPP (173)
#define VP9E_CQP_MIN_QP_VALUE (1)
#define VP9E_CQP_MAX_QP_VALUE (255)
#define VP9E_CQP_QP_FOR_REQUEST (111)

#define VP9E_CQP_QIndexDeltaLumaDC (7)
#define VP9E_CQP_QIndexDeltaChromaAC (8)
#define VP9E_CQP_QIndexDeltaChromaDC (9)

namespace vp9e_cqp
{
    enum
    {
        NONE,
        MFX_PAR,
        INTRA_REQUEST,
        NULL_QP,
        PREDEFINED_QP,
        BIG_QP,
        REQUEST_QP,
        USE_EXT_BUFFER,
        USE_EXT_BUFFER_FRAME
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 type;
        mfxU16 num_frames;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder, BS_VP9_parser
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_VP9)
        {

        }
        ~TestSuite() {}
        int RunTest(unsigned int id);

    private:
        void EncodingCycle(const tc_struct& tc, const mfxU32& frames_count);
        static const tc_struct test_case[];
        std::unique_ptr<mfxExtBuffer> m_ExtBuff;
    };

    const tc_struct TestSuite::test_case[] =
    {
        // Default case with correct predefined QP
        {/*00*/ MFX_ERR_NONE, PREDEFINED_QP, 3, {}
        },

        // NULL QPI and QPP
        {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NULL_QP, 2, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 0 },
                                 }
        },

        // NULL QPP
        {/*02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NULL_QP, 2, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 0 },
                                 }
        },

        // NULL QPI
        {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NULL_QP, 2, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 0 },
                                 }
        },

        // Too big QPI and QPP
        {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, BIG_QP, 2, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 0xffff },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 0xffff },
                                 }
        },

        // Very different QPI and QPP
        {/*05*/ MFX_ERR_NONE, PREDEFINED_QP, 2, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, VP9E_CQP_MIN_QP_VALUE },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, VP9E_CQP_MAX_QP_VALUE },
                                 }
        },

        // Request QP for a frame
        {/*06*/ MFX_ERR_NONE, REQUEST_QP, 8, {}
        },

        // Request cqp-related codec-specific options via m_ExtBuffer for whole encoding
        {/*07*/ MFX_ERR_NONE, USE_EXT_BUFFER, 3, {}
        },

        // Request cqp-related codec-specific options via m_ExtBuffer for each frame
        {/*08*/ MFX_ERR_NONE, USE_EXT_BUFFER_FRAME, 5, {}
        },

    };

    const unsigned int TestSuite::n_cases = sizeof(test_case) / sizeof(tc_struct);

    void TestSuite::EncodingCycle(const tc_struct& tc, const mfxU32& frames_count)
    {
        mfxU32 encoded = 0;
        mfxU32 submitted = 0;
        mfxU32 async = TS_MAX(1, m_par.AsyncDepth);
        mfxU32 cycle_counter = 1;
        mfxU32 cycle_counter_encoded = 1;
        mfxU16  requested_qp  = 0;
        mfxSyncPoint sp;

        async = TS_MIN(frames_count, async - 1);

        while(encoded < frames_count)
        {
            if(tc.type == REQUEST_QP)
            {
                // Generating pseudo-random QP value for each encoding cycle
                requested_qp = ((VP9E_CQP_QP_FOR_REQUEST*cycle_counter++)%255) + 1;
                m_pCtrl->QP = requested_qp;
            }

            if(tc.type == USE_EXT_BUFFER_FRAME)
            {
                ((mfxExtCodingOptionVP9*)m_ExtBuff.get())->QIndexDeltaLumaDC = VP9E_CQP_QIndexDeltaLumaDC+cycle_counter;
                ((mfxExtCodingOptionVP9*)m_ExtBuff.get())->QIndexDeltaChromaAC = VP9E_CQP_QIndexDeltaChromaAC+cycle_counter;
                ((mfxExtCodingOptionVP9*)m_ExtBuff.get())->QIndexDeltaChromaDC = VP9E_CQP_QIndexDeltaChromaDC+cycle_counter;


                m_pCtrl->NumExtParam = 1;
                m_pCtrl->ExtParam = (mfxExtBuffer **)&m_ExtBuff;

            }

            if(MFX_ERR_MORE_DATA == EncodeFrameAsync())
            {
                if(!m_pSurf)
                {
                    if(submitted)
                    {
                        encoded += submitted;
                        SyncOperation(sp);
                    }
                    break;
                }
                continue;
            }

            g_tsStatus.check(); if(g_tsStatus.m_failed) {ADD_FAILURE() << "FAILED encoding frame"; throw tsFAIL;}
            sp = m_syncpoint;

            if(++submitted >= async)
            {
                SyncOperation(); if(g_tsStatus.m_failed) {ADD_FAILURE() << "FAILED sync operation"; throw tsFAIL;}

                encoded += submitted;
                submitted = 0;
                async = TS_MIN(async, (frames_count - encoded));
                        
                BSErr bserror = set_buffer(m_pBitstream->Data, m_pBitstream->DataLength);
                ASSERT_EQ(bserror, BS_ERR_NONE) << "FAILED: Set buffer to stream parser error!";

                bserror = parse_next_unit();
                ASSERT_EQ(bserror, BS_ERR_NONE) << "FAILED: Parsing encoded frame's header error!";

                void *ptr = get_header();
                if(ptr == nullptr) {
                    ADD_FAILURE() << "FAILED: Obtaining headers from the encoded stream error!";
                    throw tsFAIL;
                }

                BS_VP9::Frame* hdr = static_cast<BS_VP9::Frame*>(ptr);

                cycle_counter_encoded++;
                if(tc.type == REQUEST_QP)
                {
                    mfxU16 expected_qp = ((VP9E_CQP_QP_FOR_REQUEST*cycle_counter_encoded)%255) + 1;
                    ASSERT_EQ(hdr->uh.quant.base_qindex, expected_qp) << "FAILED: base_q_index=" << (mfxU32)hdr->uh.quant.base_qindex
                            << " in the encoded I-frame header does not match to requested m_pCtrl->QP=" << expected_qp;
                }
                else if(tc.type == BIG_QP)
                {
                    ASSERT_EQ(hdr->uh.quant.base_qindex, VP9E_CQP_MAX_QP_VALUE) << "FAILED: base_q_index=" << (mfxU32)hdr->uh.quant.base_qindex
                            << " in the encoded frame header does not match to mfxVideoParam.mfx.QPI(P)=" << VP9E_CQP_MAX_QP_VALUE;
                }
                else if(tc.type == NULL_QP)
                {
                    if(m_pBitstream->FrameType == MFX_FRAMETYPE_I)
                    {
                        ASSERT_EQ(hdr->uh.quant.base_qindex, m_pPar->mfx.QPI) << "FAILED: base_q_index=" << (mfxU32)hdr->uh.quant.base_qindex
                            << " in the encoded I-frame header does not match to mfxVideoParam.mfx.QPI=" << m_pPar->mfx.QPI;
                    }
                    else if(m_pBitstream->FrameType == MFX_FRAMETYPE_P)
                    {
                        ASSERT_EQ(hdr->uh.quant.base_qindex, m_pPar->mfx.QPP) << "FAILED: base_q_index=" << (mfxU32)hdr->uh.quant.base_qindex
                            << " in the encoded P-frame header does not match to mfxVideoParam.mfx.QPP=" << m_pPar->mfx.QPP;
                    }
                    else
                    {
                        ADD_FAILURE() << "FAILED: Unexpected frame type m_pBitstream->FrameType=" << m_pBitstream->FrameType;
                    }
                }
                else if(tc.type == PREDEFINED_QP)
                {
                    if(m_pBitstream->FrameType == MFX_FRAMETYPE_I)
                    {
                        ASSERT_EQ(hdr->uh.quant.base_qindex, m_pPar->mfx.QPI) << "FAILED: base_q_index=" << (mfxU32)hdr->uh.quant.base_qindex
                            << " in the encoded I-frame header does not match to mfxVideoParam.mfx.QPI=" << m_pPar->mfx.QPI;
                    }
                    else if(m_pBitstream->FrameType == MFX_FRAMETYPE_P)
                    {
                        ASSERT_EQ(hdr->uh.quant.base_qindex, m_pPar->mfx.QPP) << "FAILED: base_q_index=" << (mfxU32)hdr->uh.quant.base_qindex
                            << " in the encoded P-frame header does not match to mfxVideoParam.mfx.QPP=" << m_pPar->mfx.QPP;
                    }
                    else
                    {
                        ADD_FAILURE() << "FAILED: Unexpected frame type m_pBitstream->FrameType=" << m_pBitstream->FrameType;
                    }
                }

                if(tc.type == USE_EXT_BUFFER)
                {
                    ASSERT_EQ(hdr->uh.quant.y_dc.delta*hdr->uh.quant.y_dc.sign, VP9E_CQP_QIndexDeltaLumaDC) << "FAILED: Global codec-specific params not match (DeltaLumaDC)";
                    ASSERT_EQ(hdr->uh.quant.uv_ac.delta*hdr->uh.quant.uv_ac.sign, VP9E_CQP_QIndexDeltaChromaAC) << "FAILED: Global codec-specific params not match (DeltaChromaAC)";
                    ASSERT_EQ(hdr->uh.quant.uv_dc.delta*hdr->uh.quant.uv_dc.sign, VP9E_CQP_QIndexDeltaChromaDC) << "FAILED: Global codec-specific params not match (DeltaChromaDC)";
                }
                else if(tc.type == USE_EXT_BUFFER_FRAME)
                {
                    ASSERT_EQ(hdr->uh.quant.y_dc.delta*hdr->uh.quant.y_dc.sign, VP9E_CQP_QIndexDeltaLumaDC+cycle_counter_encoded) << "FAILED: Codec-specific params for a frame not match (DeltaLumaDC)";
                    ASSERT_EQ(hdr->uh.quant.uv_ac.delta*hdr->uh.quant.uv_ac.sign, VP9E_CQP_QIndexDeltaChromaAC+cycle_counter_encoded) << "FAILED: Codec-specific params for a frame not match (DeltaChromaAC)";
                    ASSERT_EQ(hdr->uh.quant.uv_dc.delta*hdr->uh.quant.uv_dc.sign, VP9E_CQP_QIndexDeltaChromaDC+cycle_counter_encoded) << "FAILED: Codec-specific params for a frame not match (DeltaChromaDC)";
                }

                m_pBitstream->DataLength = m_pBitstream->DataOffset = 0;
            }
        }
    }

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];
        const char* stream = g_tsStreamPool.Get("YUV/salesman_176x144_449.yuv");
        g_tsStreamPool.Reg();
        tsSurfaceProcessor *reader;

        MFXInit();
        Load();

        //set default params
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        m_par.mfx.QPI = VP9E_CQP_DEFAULT_QPI;
        m_par.mfx.QPP = VP9E_CQP_DEFAULT_QPP;
        m_par.mfx.TargetUsage = 1;
        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 176;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;

        SETPARS(m_pPar, MFX_PAR);

        if(tc.type == USE_EXT_BUFFER || tc.type == USE_EXT_BUFFER_FRAME)
        {
            m_ExtBuff.reset((mfxExtBuffer*)(new mfxExtCodingOptionVP9()));
            memset(m_ExtBuff.get(), 0, sizeof(mfxExtCodingOptionVP9));
            m_ExtBuff->BufferId = MFX_EXTBUFF_CODING_OPTION_VP9;
            m_ExtBuff->BufferSz = sizeof(mfxExtCodingOptionVP9);

            ((mfxExtCodingOptionVP9*)m_ExtBuff.get())->QIndexDeltaLumaDC = VP9E_CQP_QIndexDeltaLumaDC;
            ((mfxExtCodingOptionVP9*)m_ExtBuff.get())->QIndexDeltaChromaAC = VP9E_CQP_QIndexDeltaChromaAC;
            ((mfxExtCodingOptionVP9*)m_ExtBuff.get())->QIndexDeltaChromaDC = VP9E_CQP_QIndexDeltaChromaDC;

            if(tc.type == USE_EXT_BUFFER)
            {
                m_pPar->NumExtParam = 1;
                m_pPar->ExtParam = (mfxExtBuffer **)&m_ExtBuff;
            }
        }

        InitAndSetAllocator();

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            if (g_tsHWtype < MFX_HW_CNL) // unsupported on platform less CNL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                return 0;
            }
        }  else {
            g_tsLog << "WARNING: loading encoder from plugin failed!\n";
            return 0;
        }

        //set reader
        reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
        m_filler = reader;

        const mfxU32 default_frames_number = 10;
        const mfxU32 frames_count = (tc.num_frames == 0 ? default_frames_number : tc.num_frames + m_pPar->AsyncDepth);

        g_tsStatus.expect(tc.sts);

        Init(m_session, m_pPar);

        if(tc.type == BIG_QP || tc.type == NULL_QP)
        {
            //for these tests Init() will overwrite some params - it's ok
            g_tsStatus.expect(MFX_ERR_NONE);
        } 
        else
        {
            if( g_tsStatus.m_status != tc.sts)
            {
                ADD_FAILURE() << "Init() failed with error";
                return g_tsStatus.get();
            }
        }

        if (tc.sts >= MFX_ERR_NONE)
        {
            EncodingCycle(tc, frames_count);
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(vp9e_cqp);
};
