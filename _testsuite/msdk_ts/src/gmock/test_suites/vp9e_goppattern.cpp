/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_transcoder.h"
//#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include "ts_decoder.h"
#include <memory>

namespace vp9e_goppattern
{
    enum
    {
        NONE,
        MFX_PAR,
        INTRA_REQUEST
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
        const char *FrameTypeByMFXEnum(mfxU16 enum_frame_type)
        {
            switch (enum_frame_type)
            {
                case MFX_FRAMETYPE_I : return "I";
                case MFX_FRAMETYPE_P : return "P";
                default : return "UNKNOWN";
            }
        }

    private:
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {
        // Default case
        {/*00*/ MFX_ERR_NONE, NONE, 18, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 6 }
                                 }
        },

        // Corner-case: not enoupgh frames for a GOP
        {/*01*/ MFX_ERR_NONE, NONE, 5, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 6 }
                                 }
        },

        // Corner-case: 1 frame in the end for a new GOP
        {/*02*/ MFX_ERR_NONE, NONE, 7, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 6 }
                                  }
        },

        // 1-frame GOP
        {/*03*/ MFX_ERR_NONE, NONE, 3, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 1 }
                                  }
        },

        // Deault GOP
        {/*04*/ MFX_ERR_NONE, NONE, 3, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 0 }
                                  }
        },

        // Request Intra-frame
        {/*05*/ MFX_ERR_NONE, INTRA_REQUEST, 10, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 5 }
                                  }
        },

        // Very long GOP
        {/*06*/ MFX_ERR_NONE, NONE, 260, {
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 0 },
                                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 257 }
                                    }
        }
    };

    const unsigned int TestSuite::n_cases = sizeof(test_case) / sizeof(tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];
        const char* stream = g_tsStreamPool.Get("YUV/salesman_176x144_449.yuv");
        g_tsStreamPool.Reg();
        tsSurfaceProcessor *reader;

        MFXInit();
        Load();

        SETPARS(m_pPar, MFX_PAR);

        //set default params
        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 176;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
        m_par.mfx.QPI = m_par.mfx.QPP = 100;
        m_par.mfx.TargetUsage = 1;

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

        mfxU32 async = TS_MAX(1, m_par.AsyncDepth);

        //set reader
        reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
        m_filler = reader;

        const mfxU32 default_frames_number = 10;
        const mfxU32 default_intra_frame_request_position = 3;

        const mfxU32 frames_count = (tc.num_frames == 0 ? default_frames_number : tc.num_frames);

        std::unique_ptr<mfxU32[]> frame_types_expected (new mfxU32[frames_count]);
        std::unique_ptr<mfxU32[]> frame_types_encoded (new mfxU32[frames_count]);
        
        mfxU32 gop_frame_count = 0;
        for(mfxU32 i = 0; i < frames_count; i++)
        {
            if (tc.type == INTRA_REQUEST && i == default_intra_frame_request_position)
            {
                m_pCtrl->FrameType = MFX_FRAMETYPE_I;
                // GOP is restarted on Intra-request
                gop_frame_count = 0;
            }
            else if (m_pCtrl)
            {
                m_pCtrl->FrameType = 0;
            }

            //specifying expected frame type
            if (gop_frame_count++ == 0)
            {
                frame_types_expected[i] = MFX_FRAMETYPE_I;
            } 
            else
            {
                frame_types_expected[i] = MFX_FRAMETYPE_P;
            }

            //start new GOP
            if (gop_frame_count == m_pPar->mfx.GopPicSize)
            {
                gop_frame_count = 0;
            }

            frame_types_encoded[i] = MFX_FRAMETYPE_UNKNOWN;
        }

        g_tsStatus.expect(tc.sts);

        Init(m_session, m_pPar);

        if (tc.sts >= MFX_ERR_NONE)
        {
            mfxU32 encoded = 0;
            mfxU32 submitted = 0;
            mfxU32 async = TS_MAX(1, m_par.AsyncDepth);
            mfxSyncPoint sp;

            async = TS_MIN(frames_count, async - 1);

            while(encoded < frames_count)
            {
                //call test function
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

                g_tsStatus.check();TS_CHECK_MFX;
                sp = m_syncpoint;

                if(++submitted >= async)
                {
                    SyncOperation();TS_CHECK_MFX;

                    encoded += submitted;
                    submitted = 0;
                    async = TS_MIN(async, (frames_count - encoded));
                        
                    BSErr bserror = set_buffer(m_pBitstream->Data, m_pBitstream->DataLength);
                    EXPECT_EQ(bserror, BS_ERR_NONE) << "FAILED: Set buffer to stream parser error!";

                    bserror = parse_next_unit();
                    EXPECT_EQ(bserror, BS_ERR_NONE) << "FAILED: Parsing encoded frame's header error!";

                    void *ptr = get_header();
                    if(ptr == nullptr) {
                        ADD_FAILURE() << "FAILED: Obtaining headers from the encoded stream error!";
                    }

                    BS_VP9::Frame* hdr = static_cast<BS_VP9::Frame*>(ptr);

                    if(hdr->FrameOrder > frames_count)
                    {
                        g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
                        ADD_FAILURE() << "FAILED: BS_VP9::Frame->FrameOrder=" << hdr->FrameOrder << " exceeds frames_count=" << frames_count;
                    }

                    if( (hdr->uh.frame_is_intra && m_pBitstream->FrameType != MFX_FRAMETYPE_I ) || (!hdr->uh.frame_is_intra && m_pBitstream->FrameType != MFX_FRAMETYPE_P ) )
                    {
                        g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
                        ADD_FAILURE() << "FAILED: 'm_pBitstream->FrameType' does not match to 'BS_VP9::Frame->uh.frame_is_intra'";
                    }

                    frame_types_encoded[hdr->FrameOrder] = hdr->uh.frame_is_intra ? MFX_FRAMETYPE_I : MFX_FRAMETYPE_P;

                    g_tsLog << "INFO: got encoded frame #" << hdr->FrameOrder << " of size=" << m_pBitstream->DataLength << " type=" << hdr->uh.frame_is_intra << "\n";

                    m_pBitstream->DataLength = m_pBitstream->DataOffset = 0;

                    if(frame_types_expected[hdr->FrameOrder] != frame_types_encoded[hdr->FrameOrder])
                    {
                        g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
                        ADD_FAILURE() << "FAILED: GOP-pattern in the encoded frame does not match to the expected: frame[" << hdr->FrameOrder << "].type=" << FrameTypeByMFXEnum(frame_types_encoded[hdr->FrameOrder]) << ", expected " << FrameTypeByMFXEnum(frame_types_expected[hdr->FrameOrder]);
                    }
                }
            }
        }

        for(mfxU32 i = 0; i < frames_count; i++)
        {
            g_tsLog << "frame[" << i << "] type=" << FrameTypeByMFXEnum(frame_types_encoded[i]) << " expected_type=" << FrameTypeByMFXEnum(frame_types_expected[i]) << "\n";
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(vp9e_goppattern);
};