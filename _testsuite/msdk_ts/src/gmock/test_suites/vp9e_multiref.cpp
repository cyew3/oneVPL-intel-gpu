/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace vp9e_multiref
{
    #define VP9E_MULTIREF_MAX_FRAME_NUM (1024)
    #define VP9E_MULTIREF_MAX_NUMREFFRAME (3)
    #define VP9E_MULTIREF_DEFAULT_NUMREFFRAME (1)

    #define IVF_SEQ_HEADER_SIZE_BYTES (32)
    #define IVF_PIC_HEADER_SIZE_BYTES (12)
    #define MAX_IVF_HEADER_SIZE IVF_SEQ_HEADER_SIZE_BYTES + IVF_PIC_HEADER_SIZE_BYTES

    #define PSNR_THRESHOLD (35)

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

    struct frame_info
    {
        mfxU32 frame_num;
        mfxU32 frame_encoded_size;
        mfxU32 frame_type;
        mfxU32 frame_pattern_num;
    };

    class TestSuite : tsVideoEncoder, BS_VP9_parser
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_VP9) {}
        ~TestSuite() {}

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(const tc_struct& tc, unsigned int fourcc_id);

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

    // This class is aimed to make an artificial stream by repeating several frames from the initial stream
    class tsRawReaderDublicator : public tsRawReader
    {
        mfxU32 m_RepeatCount;
        std::map<mfxU32, mfxFrameSurface1*>* m_pInputSurfaces;
        frame_info *m_StreamStat;
        tsSurfacePool surfaceStorage;
        std::map<mfxU32, mfxFrameSurface1*> m_StoredSurfaces;
    public:
        // repeat_count=0 : default behavior of the base class (ABCDEF => ABCDEF)
        // repeat_count=1 : repeat first frame infinitely (ABCDEF => AAAAAA..)
        // repeat_count=2 : repeat 1 and 2 frames (ABCDEF => ABABAB..)
        // repeat_count=3 : repeat 1, 2 and 3 frames (ABCDEF => ABCABC...)
        // and so on
        tsRawReaderDublicator(const char* fname, mfxFrameInfo fi, frame_info *stream_stat, std::map<mfxU32, mfxFrameSurface1*>* pInSurfaces, const mfxU32 repeat_count = 0)
            : tsRawReader(fname, fi)
            , m_RepeatCount(repeat_count)
            , m_pInputSurfaces(pInSurfaces)
            , m_StreamStat(stream_stat)
        {
            if (repeat_count == 0)
            {
                //default behavior of base class
                return;
            }

            const mfxU32 numInternalSurfaces = repeat_count;
            mfxFrameAllocRequest req = {};
            req.Info = fi;
            req.NumFrameMin = req.NumFrameSuggested = numInternalSurfaces;
            req.Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
            surfaceStorage.AllocSurfaces(req);
        }

        ~tsRawReaderDublicator()
        {
            if (m_pInputSurfaces)
            {
                m_pInputSurfaces->clear();
            }
            if (m_RepeatCount)
            {
                surfaceStorage.FreeSurfaces();
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
                mfxFrameSurface1* pStorageSurf = surfaceStorage.GetSurface();
                msdk_atomic_inc16(&pStorageSurf->Data.Locked);

                mfxStatus result = tsRawReader::ProcessSurface(*pStorageSurf);
                m_cur--;
                m_StoredSurfaces[m_cur - 1] = pStorageSurf;
            }

            if (!m_eos)
            {
                const mfxU32 storage_pos = m_cur <= m_RepeatCount ? (m_cur - 1) : (m_cur - 1)%m_RepeatCount;
                tsFrame src(*m_StoredSurfaces[storage_pos]);
                tsFrame dst(s);

                dst = src;
                s.Data.FrameOrder = (m_cur - 1);

                if (m_StreamStat)
                {
                    m_StreamStat[s.Data.FrameOrder].frame_num = s.Data.FrameOrder;
                    m_StreamStat[s.Data.FrameOrder].frame_pattern_num = storage_pos;
                }

                if (m_pInputSurfaces)
                {
                    (*m_pInputSurfaces)[s.Data.FrameOrder] = m_StoredSurfaces[storage_pos];
                }
            }

            return MFX_ERR_NONE;
        }
    };

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserVP9, public tsVideoDecoder
    {
        frame_info *m_StreamStat;
        std::map<mfxU32, mfxFrameSurface1*>* m_pInputSurfaces;
        bool m_DecoderInited;
        mfxU32 m_DecodedFramesCount;
    public:
        BitstreamChecker(frame_info *stream_stat, std::map<mfxU32, mfxFrameSurface1*>* pSurfaces)
            : tsVideoDecoder(MFX_CODEC_VP9)
            , m_StreamStat(stream_stat)
            , m_pInputSurfaces(pSurfaces)
            , m_DecoderInited(false)
            , m_DecodedFramesCount(0)
        {}
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
    };

    mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        SetBuffer(bs);

        // Dump encoded stream to file
        /*
        const int encoded_size = bs.DataLength;
        static FILE *fp_vp9 = fopen("encoded_multiref.vp9", "wb");
        fwrite(bs.Data, encoded_size, 1, fp_vp9);
        fflush(fp_vp9);
        */
        while (checked++ < nFrames)
        {
            tsParserVP9::UnitType& hdr = ParseOrDie();

            // do the decoder initialisation on the first encoded frame
            if (m_DecodedFramesCount == 0 && !m_DecoderInited)
            {
                const mfxU32 headers_shift = IVF_PIC_HEADER_SIZE_BYTES + (m_DecodedFramesCount == 0 ? IVF_SEQ_HEADER_SIZE_BYTES : 0);
                m_pBitstream->Data = bs.Data + headers_shift;
                m_pBitstream->DataOffset = 0;
                m_pBitstream->DataLength = bs.DataLength - headers_shift;
                m_pBitstream->MaxLength = bs.MaxLength;

                m_pPar->AsyncDepth = 1;

                mfxStatus decode_header_status = DecodeHeader();

                mfxStatus init_status = Init();
                m_par_set = true;
                if (init_status >= 0)
                {
                    if (m_default && !m_request.NumFrameMin)
                    {
                        QueryIOSurf();
                    }
                    // This is a workaround for the decoder (there is an issue with NumFrameSuggested and decoding superframes)
                    m_request.NumFrameMin = 5;
                    m_request.NumFrameSuggested = 10;

                    mfxStatus alloc_status = tsSurfacePool::AllocSurfaces(m_request, !m_use_memid);
                    if (alloc_status >= 0)
                    {
                        m_DecoderInited = true;
                    }
                    else
                    {
                        ADD_FAILURE() << "WARNING: Could not allocate surfaces for the decoder, status " << alloc_status;
                        throw tsFAIL;
                    }
                }
                else
                {
                    ADD_FAILURE() << "WARNING: Could not inilialize the decoder, Init() returned " << init_status;
                    throw tsFAIL;
                }
            }

            if (m_DecoderInited)
            {
                const mfxU32 ivfSize = hdr.FrameOrder == 0 ? MAX_IVF_HEADER_SIZE : IVF_PIC_HEADER_SIZE_BYTES;

                m_pBitstream->Data = bs.Data + ivfSize;
                m_pBitstream->DataOffset = 0;
                m_pBitstream->DataLength = bs.DataLength - ivfSize;
                m_pBitstream->MaxLength = bs.MaxLength;

                mfxStatus sts = MFX_ERR_NONE;
                do
                {
                    sts = DecodeFrameAsync();
                } while (sts == MFX_WRN_DEVICE_BUSY);

                m_DecodedFramesCount++;

                if (sts < 0)
                {
                    ADD_FAILURE() << "ERROR: DecodeFrameAsync for frame " << hdr.FrameOrder << " failed with status: " << sts;
                    throw tsFAIL;
                }

                sts = SyncOperation();
                if (sts < 0)
                {
                    ADD_FAILURE() << "ERROR: SyncOperation for frame " << hdr.FrameOrder << " failed with status: " << sts;
                    throw tsFAIL;
                }

                // check that respective source frame is stored
                if (m_pInputSurfaces->find(hdr.FrameOrder) == m_pInputSurfaces->end())
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }

                mfxU32 w = m_pPar->mfx.FrameInfo.Width;
                mfxU32 h = m_pPar->mfx.FrameInfo.Height;

                mfxFrameSurface1* pInputSurface = (*m_pInputSurfaces)[hdr.FrameOrder];
                tsFrame src = tsFrame(*pInputSurface);
                tsFrame res = tsFrame(*m_pSurf);
                src.m_info.CropX = src.m_info.CropY = res.m_info.CropX = res.m_info.CropY = 0;
                src.m_info.CropW = res.m_info.CropW = w;
                src.m_info.CropH = res.m_info.CropH = h;

                const mfxF64 psnrY = PSNR(src, res, 0);
                const mfxF64 psnrU = PSNR(src, res, 1);
                const mfxF64 psnrV = PSNR(src, res, 2);

                m_pInputSurfaces->erase(hdr.FrameOrder);
                const mfxF64 minPsnr = PSNR_THRESHOLD;

                g_tsLog << "INFO: frame[" << hdr.FrameOrder << "]: PSNR-Y=" << psnrY << " PSNR-U="
                    << psnrU << " PSNR-V=" << psnrV << " size=" << bs.DataLength << "\n";

                if (psnrY < minPsnr)
                {
                    ADD_FAILURE() << "ERROR: PSNR-Y of frame " << hdr.FrameOrder << " is equal to " << psnrY << " and lower than threshold: " << minPsnr;
                    throw tsFAIL;
                }
                if (psnrU < minPsnr)
                {
                    ADD_FAILURE() << "ERROR: PSNR-U of frame " << hdr.FrameOrder << " is equal to " << psnrU << " and lower than threshold: " << minPsnr;
                    throw tsFAIL;
                }
                if (psnrV < minPsnr)
                {
                    ADD_FAILURE() << "ERROR: PSNR-V of frame " << hdr.FrameOrder << " is equal to " << psnrV << " and lower than threshold: " << minPsnr;
                    throw tsFAIL;
                }
            }

            m_StreamStat[hdr.FrameOrder].frame_encoded_size = bs.DataLength;

            m_StreamStat[hdr.FrameOrder].frame_type = hdr.uh.frame_is_intra ? MFX_FRAMETYPE_I : MFX_FRAMETYPE_P;

            bs.DataLength = bs.DataOffset = 0;
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    // This method determines a quantity of small frames in the sequence
    //  by checking their sizes.
    mfxU32 CalculateInterOnlyFramesQnt(std::vector<mfxU32> sizes_vector)
    {
        mfxU32 result = 0;

        if (sizes_vector.size() < 2)
        {
            return result;
        }

        std::sort(sizes_vector.begin(), sizes_vector.end());

        mfxU32 max_element = sizes_vector[sizes_vector.size() - 1];

        // Iterative approach to find small elements in the array from very obvious case to less obvious
        for (mfxU32 threshold = 10; threshold >= 2; threshold--)
        {
            mfxI32 small_elements_count = -1;

            // On 1st phase we calculate small elements in the array (in compare with the biggest one)
            for (mfxU32 i = 0; i < sizes_vector.size(); i++)
            {
                if (sizes_vector[i] < (max_element / (3*threshold)))
                {
                    small_elements_count++;
                }
            }

            // On 2nd phase we check that there is a significant difference between small elements
            //  and the rest elements
            if (small_elements_count >= 0)
            {
                if (sizes_vector[small_elements_count] < sizes_vector[small_elements_count + 1] / threshold)
                {
                    result = small_elements_count + 1;
                }
            }

            if (result > 0)
            {
                break;
            }
        }

        return result;
    }

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(const tc_struct& tc, unsigned int fourcc_id)
    {
        TS_START;

        // For this test it is important for input stream to have big visual difference
        //  between consequal frames (P-frames should be big enough to differ from inter-only frames)
        char* stream = nullptr;

        std::map<mfxU32, mfxFrameSurface1*> inputSurfaces;
        frame_info m_StreamStat[VP9E_MULTIREF_MAX_FRAME_NUM];
        memset(m_StreamStat, 0, sizeof(m_StreamStat));
        for (mfxU32 i = 0; i < VP9E_MULTIREF_MAX_FRAME_NUM; i++)
        {
            m_StreamStat[i].frame_num = VP9E_MULTIREF_MAX_FRAME_NUM + 1;
        }

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            stream = const_cast<char *>(g_tsStreamPool.Get("YUV/720x480p_30.00_4mb_h264_cabac_180s.yuv"));

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 720;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

            stream = const_cast<char *>(g_tsStreamPool.Get("YUV10bit420_ms/Kimono1_352x288_24_p010_shifted.yuv"));

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 352;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 288;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            stream = const_cast<char *>(g_tsStreamPool.Get("YUV8bit444/Kimono1_352x288_24_ayuv.yuv"));

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 352;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 288;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

            stream = const_cast<char *>(g_tsStreamPool.Get("YUV10bit444/Kimono1_352x288_24_y410.yuv"));

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 352;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 288;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        g_tsStreamPool.Reg();
        tsRawReaderDublicator *reader;
        BitstreamChecker bs_checker(m_StreamStat, &inputSurfaces);
        m_bs_processor = &bs_checker;

        MFXInit();
        Load();

        SETPARS(m_pPar, MFX_PAR);

        InitAndSetAllocator();

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_VP9E_HW.Data, sizeof(MFX_PLUGINID_VP9E_HW.Data)))
        {
            // MFX_PLUGIN_VP9E_HW unsupported on platform less CNL(NV12) and ICL(P010, AYUV, Y410)
            if ((fourcc_id == MFX_FOURCC_NV12 && g_tsHWtype < MFX_HW_CNL)
                || ((fourcc_id == MFX_FOURCC_P010 || fourcc_id == MFX_FOURCC_AYUV
                    || fourcc_id == MFX_FOURCC_Y410) && g_tsHWtype < MFX_HW_ICL))
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
        }
        else {
            g_tsLog << "WARNING: loading encoder from plugin failed!\n";
            return 0;
        }

        //set reader
        reader = new tsRawReaderDublicator(stream, m_pPar->mfx.FrameInfo, m_StreamStat, &inputSurfaces, tc.repeat_pattern_len);
        reader->m_disable_shift_hack = true;
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
                    if (m_StreamStat[i].frame_num != (VP9E_MULTIREF_MAX_FRAME_NUM + 1))
                    {
                        printf("==> FRAME[%2.0d] [%s] [%d]  %d\n", i, m_StreamStat[i].frame_type == MFX_FRAMETYPE_I ?
                            "I" : "P", m_StreamStat[i].frame_pattern_num, m_StreamStat[i].frame_encoded_size);
                        all_frames_size += m_StreamStat[i].frame_encoded_size;
                        sizes_vector.push_back(m_StreamStat[i].frame_encoded_size);
                    }
                    else
                    {
                        break;
                    }
                }

                mfxU32 inter_only_frames_qnt = CalculateInterOnlyFramesQnt(sizes_vector);

                printf("===> TOTAL STREAM SIZE IS %d [%d inter-only frames]\n", all_frames_size, inter_only_frames_qnt);

                EXPECT_EQ(tc.expected_inter_only_frames_qnt, inter_only_frames_qnt)
                    << "ERROR: DPB-buffer not utilized in the expected way: should be " <<
                    tc.expected_inter_only_frames_qnt << " inter-predicted frames, but detected " << inter_only_frames_qnt;
            }
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_multiref,              RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_420_p010_multiref, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_8b_444_ayuv_multiref,  RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(vp9e_10b_444_y410_multiref, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
};
