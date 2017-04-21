/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <vector>
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace vp9e_multiref
{
    #define VP9E_MULTIREF_MAX_FRAME_NUM (1024)
    #define VP9E_MULTIREF_MAX_NUMREFFRAME (3)
    #define VP9E_MULTIREF_DEFAULT_NUMREFFRAME (1)

    enum
    {
        NONE,
        MFX_PAR,
        INIT_ONLY_CHECK
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 type;
        mfxU16 num_frames;
        mfxU16 repeat_pattern_len;
        mfxU16 expected_inter_only_frames_qnt;
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
        mfxU32 m_MaxFrameSize;

        struct frame_info
        {
            mfxU32 frame_num;
            mfxU32 frame_encoded_size;
            mfxU32 frame_type;
            mfxU32 frame_pattern_num;
        };
        frame_info m_FrameValues[VP9E_MULTIREF_MAX_FRAME_NUM];

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_VP9)
            , m_MaxFrameSize(0)
        {
            memset(m_FrameValues, 0, sizeof(m_FrameValues));
            for (mfxU32 i = 0; i < VP9E_MULTIREF_MAX_FRAME_NUM; i++)
            {
                m_FrameValues[i].frame_num = VP9E_MULTIREF_MAX_FRAME_NUM + 1;
            }
        }
        ~TestSuite() {}
        int RunTest(unsigned int id);

    private:
        void EncodingCycle(const tc_struct& tc, const mfxU32& frames_count);
        static const tc_struct test_case[];
    };

    const tc_struct TestSuite::test_case[] =
    {
        // Check default NumRefFrame
        {/*00*/ MFX_ERR_NONE, INIT_ONLY_CHECK, 0, 0, 0,
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 0 },
        },

        // Check big NumRefFrame value (TU is default => "4")
        {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT_ONLY_CHECK, 0, 0, 0,
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, VP9E_MULTIREF_MAX_NUMREFFRAME + 1 },
        },

        // Check big NumRefFrame value for TU=1
        {/*02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT_ONLY_CHECK, 0, 0, 0,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, VP9E_MULTIREF_MAX_NUMREFFRAME + 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
        }
        },

        // Check big NumRefFrame value for TU=7 (it only supports 2)
        {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT_ONLY_CHECK, 0, 0, 0,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, VP9E_MULTIREF_MAX_NUMREFFRAME },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 },
        }
        },

        // Below are cases that check ability of the encoder to utilize DPB-buffer.
        // The test stream is artificial (repeated pattern of frames of a real stream)
        // Each case checks how many frames were encoded as inter-only by estimating encoded sizes

        // == EXPECTED ERROR CODE == TEST TYPE == STREAM LEN == REPEAT PATTERN == EXPECTED QNT OF INTER-ONLY FRAMES ==

        // CQP-mode
        {/*04*/ MFX_ERR_NONE, NONE, 20, 2, 0,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 55 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1 },
        }
        },
        {/*05*/ MFX_ERR_NONE, NONE, 20, 2, 8,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 55 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2 },
        }
        },
        {/*06*/ MFX_ERR_NONE, NONE, 20, 2, 16,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 55 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3 },
        }
        },
        {/*07*/ MFX_ERR_NONE, NONE, 20, 2, 0,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 55 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1 },
        }
        },
        {/*08*/ MFX_ERR_NONE, NONE, 20, 2, 8,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 55 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2 },
        }
        },
        {/*09*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, 20, 2, 8,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 55 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3 },
        }
        },
        {/*10*/ MFX_ERR_NONE, NONE, 20, 2, 0,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 55 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1 },
        }
        },
        {/*11*/ MFX_ERR_NONE, NONE, 20, 2, 8,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 55 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2 },
        }
        },
        {/*12*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, 20, 2, 8,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 50 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 55 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3 },
        }
        },

        // CBR-mode
        {/*13*/ MFX_ERR_NONE, NONE, 20, 2, 0,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1 },
        }
        },
        {/*14*/ MFX_ERR_NONE, NONE, 20, 2, 8,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2 },
        }
        },
        {/*15*/ MFX_ERR_NONE, NONE, 20, 2, 16,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3 },
        }
        },
        {/*16*/ MFX_ERR_NONE, NONE, 20, 2, 0,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1 },
        }
        },
        {/*17*/ MFX_ERR_NONE, NONE, 20, 2, 8,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2 },
        }
        },
        {/*18*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, 20, 2, 8,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3 },
        }
        },
        {/*19*/ MFX_ERR_NONE, NONE, 20, 2, 0,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1 },
        }
        },
        {/*20*/ MFX_ERR_NONE, NONE, 20, 2, 8,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2 },
        }
        },
        {/*21*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, 20, 2, 8,
        {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 3000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 7 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3 },
        }
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(test_case) / sizeof(tc_struct);

    // THis class is aimed to make an artificial stream by repeating several frames from the initial stream
    class tsRawReaderDublicator : public tsRawReader
    {
    public:
        // repeat_count=0 : default behavior of the base class (ABCDEF => ABCDEF)
        // repeat_count=1 : repeat first frame infinitely (ABCDEF => AAAAAA..)
        // repeat_count=2 : repeat 1 and 2 frames (ABCDEF => ABABAB..)
        // repeat_count=3 : repeat 1, 2 and 3 frames (ABCDEF => ABCABC...)
        // and so on
        tsRawReaderDublicator(const char* fname, mfxFrameInfo fi, TestSuite *testPtr, const mfxU32 repeat_count = 0)
            : tsRawReader(fname, fi)
            , m_RepeatCount(repeat_count)
            , m_SurfaceStorage(nullptr)
            , m_TestPtr(testPtr)
        {
            if (repeat_count == 0)
            {
                //default behavior of base class
                return;
            }
            if (fi.FourCC != MFX_FOURCC_NV12)
            {
                ADD_FAILURE() << "Only NV12 currently supported by tsRawReaderDublicator!"; throw tsFAIL;
            }

            m_SurfaceStorage = new mfxFrameSurface1[repeat_count];
            for (mfxU32 i = 0; i < repeat_count; i++)
            {
                m_SurfaceStorage[i].Info = fi;
                mfxFrameData& m_data = m_SurfaceStorage[i].Data;

                mfxU32 fsz = fi.Width * fi.Height;
                mfxU32 pitch = 0;

                if (fi.ChromaFormat == MFX_CHROMAFORMAT_YUV420)
                {
                    fsz = fsz * 3 / 2;
                }

                mfxU8 *buf = new mfxU8[fsz];

                m_data.Y = buf;
                m_data.UV = m_data.Y + fi.Width * fi.Height;
                pitch = fi.Width;

                m_data.PitchLow = (pitch & 0xFFFF);
                m_data.PitchHigh = (pitch >> 16);
            }
        }

        ~tsRawReaderDublicator()
        {
            for (mfxU32 i = 0; i < m_RepeatCount; i++)
            {
                delete []m_SurfaceStorage[i].Data.Y;
            }
            if (m_SurfaceStorage)
            {
                delete []m_SurfaceStorage;
            }
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            if (m_RepeatCount == 0)
            {
                return tsRawReader::ProcessSurface(s);
            }

            if (m_cur <= m_RepeatCount)
            {
                mfxStatus result = tsRawReader::ProcessSurface(m_SurfaceStorage[m_cur-1]);
                m_cur--;
            }

            if (!m_eos)
            {
                const mfxU32 storage_pos = m_cur <= m_RepeatCount ? (m_cur - 1) : (m_cur - 1)%m_RepeatCount;
                tsFrame src(m_SurfaceStorage[storage_pos]);
                tsFrame dst(s);

                dst = src;
                s.Data.FrameOrder = (m_cur - 1);

                m_TestPtr->m_FrameValues[s.Data.FrameOrder].frame_num = s.Data.FrameOrder;
                m_TestPtr->m_FrameValues[s.Data.FrameOrder].frame_pattern_num = storage_pos;
            }

            return MFX_ERR_NONE;
        }
    private:
        mfxU32 m_RepeatCount;
        mfxFrameSurface1 *m_SurfaceStorage;
        TestSuite *m_TestPtr;
    };

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserVP9
    {
        TestSuite *m_TestPtr;
    public:
        BitstreamChecker(TestSuite *testPtr) : m_TestPtr(testPtr) {}
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
    };

    mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        SetBuffer(bs);

        while (checked++ < nFrames)
        {
            tsParserVP9::UnitType& hdr = ParseOrDie();

            m_TestPtr->m_FrameValues[hdr.FrameOrder].frame_encoded_size = bs.DataLength;
            if (bs.DataLength > m_TestPtr->m_MaxFrameSize)
            {
                m_TestPtr->m_MaxFrameSize = bs.DataLength;
            }

            m_TestPtr->m_FrameValues[hdr.FrameOrder].frame_type = hdr.uh.frame_is_intra ? MFX_FRAMETYPE_I : MFX_FRAMETYPE_P;

            bs.DataLength = bs.DataOffset = 0;
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    // This method determines a quantity of small frames in the sequence
    //  by checking their sizes.
    mfxU32 CalculateInterOnlyFramesQnt(std::vector<mfxU32> sizes_vector)
    {
        std::sort(sizes_vector.begin(), sizes_vector.end());

        const mfxU32 threshold = 5;

        for (mfxU32 i = 0; i < sizes_vector.size() - 1; i++)
        {
            if ((sizes_vector[i + 1] - sizes_vector[i]) > sizes_vector[i] * threshold)
            {
                return (i + 1);
            }
        }

        return 0;
    }

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        const tc_struct& tc = test_case[id];

        // For this test it is important for input stream to have big visual difference
        //  between consequal frames (P-frames should be big enough to differ from inter-only frames)
        const char* stream = g_tsStreamPool.Get("YUV/720x480p_30.00_4mb_h264_cabac_180s.yuv");

        g_tsStreamPool.Reg();
        tsRawReaderDublicator *reader;
        BitstreamChecker bs_checker(this);
        m_bs_processor = &bs_checker;

        MFXInit();
        Load();

        //set default params
        m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 720;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;

        SETPARS(m_pPar, MFX_PAR);

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
        reader = new tsRawReaderDublicator(stream, m_pPar->mfx.FrameInfo, this, tc.repeat_pattern_len);
        m_filler = reader;

        g_tsStatus.expect(tc.sts);
        mfxStatus init_result_status = MFXVideoENCODE_Init(m_session, m_pPar);
        if (init_result_status >= MFX_ERR_NONE)
        {
            m_initialized = true;
        }
        else
        {
            ADD_FAILURE() << "Init() failed with error";
            return init_result_status;
        }
        g_tsStatus.check(init_result_status);

        g_tsStatus.expect(MFX_ERR_NONE);

        mfxVideoParam new_par = {};
        GetVideoParam(m_session, &new_par);

        mfxU16 tu_actual = (m_par.mfx.TargetUsage == 0 ? 4 : m_par.mfx.TargetUsage);
        mfxU16 NumRefFrame_maximum_expected = (tu_actual > 2 ? 2 : 3);
        if (tu_actual > 2 && m_pPar->mfx.NumRefFrame > 2)
        {
            EXPECT_EQ(new_par.mfx.NumRefFrame, NumRefFrame_maximum_expected) << "ERROR: NumRefFrame for "
                << m_pPar->mfx.NumRefFrame << " is inited to " << new_par.mfx.NumRefFrame << ", expected " << NumRefFrame_maximum_expected;
        }
        else if (m_pPar->mfx.NumRefFrame > VP9E_MULTIREF_MAX_NUMREFFRAME)
        {
            EXPECT_EQ(new_par.mfx.NumRefFrame, NumRefFrame_maximum_expected) << "ERROR: NumRefFrame for "
                << m_pPar->mfx.NumRefFrame << " is inited to " << new_par.mfx.NumRefFrame << ", expected " << NumRefFrame_maximum_expected;
        }
        else if (m_pPar->mfx.NumRefFrame == 0)
        {
            EXPECT_EQ(new_par.mfx.NumRefFrame, VP9E_MULTIREF_DEFAULT_NUMREFFRAME) << "ERROR: NumRefFrame for "
                << m_pPar->mfx.NumRefFrame << " is inited to " << new_par.mfx.NumRefFrame << ", expected " << VP9E_MULTIREF_DEFAULT_NUMREFFRAME;
        }
        else
        {
            EXPECT_EQ(new_par.mfx.NumRefFrame, m_pPar->mfx.NumRefFrame) << "ERROR: NumRefFrame for "
                << m_pPar->mfx.NumRefFrame << " is inited to " << new_par.mfx.NumRefFrame << ", expected " << m_pPar->mfx.NumRefFrame;
        }

        if (tc.type != INIT_ONLY_CHECK)
        {
            const mfxU32 default_frames_number = 10;
            const mfxU32 frames_count = (tc.num_frames == 0 ? default_frames_number : tc.num_frames - (new_par.AsyncDepth - 1));

            if (tc.sts >= MFX_ERR_NONE)
            {
                EncodeFrames(frames_count);

                g_tsStatus.check(DrainEncodedBitstream());
                TS_CHECK_MFX;

                mfxU32 all_frames_size = 0;
                std::vector<mfxU32> sizes_vector;
                for (mfxU32 i = 0; i < VP9E_MULTIREF_MAX_FRAME_NUM; i++)
                {
                    if (m_FrameValues[i].frame_num != (VP9E_MULTIREF_MAX_FRAME_NUM + 1))
                    {
                        printf("==> FRAME[%2.0d] [%s] [%d]  %d\n", i, m_FrameValues[i].frame_type == MFX_FRAMETYPE_I ?
                            "I" : "P", m_FrameValues[i].frame_pattern_num, m_FrameValues[i].frame_encoded_size);
                        all_frames_size += m_FrameValues[i].frame_encoded_size;
                        sizes_vector.push_back(m_FrameValues[i].frame_encoded_size);
                    }
                    else
                    {
                        break;
                    }
                }

                mfxU32 inter_only_frames_qnt = CalculateInterOnlyFramesQnt(sizes_vector);

                printf("===> TOTAL STREAM SIZE IS %d [%d inter-only frames]\n", all_frames_size, inter_only_frames_qnt);

                EXPECT_EQ(tc.expected_inter_only_frames_qnt, inter_only_frames_qnt) << "ERROR: Expected " << 
                    tc.expected_inter_only_frames_qnt << " inter-only frames, but detected " << inter_only_frames_qnt;
            }
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(vp9e_multiref);
};
