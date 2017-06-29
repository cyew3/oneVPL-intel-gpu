/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_bitstream.h"
#include "ts_parser.h"
#include <vector>

namespace mpeg2d_timecode
{

class TestSuite : tsVideoDecoder, public tsParserMPEG2
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_MPEG2) {}
    ~TestSuite()
    {
        for (mfxU32 i = 0; i < PoolSize(); i++)
        {
            mfxFrameSurface1* p = GetSurface(i);
            p->Data.NumExtParam = 0;
            delete p->Data.ExtParam[0];
            delete [] p->Data.ExtParam;
        }
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        MFX_PAR = 1,
        ALLOC_OPAQUE = 8,
    };

    struct tc_struct
    {
        mfxStatus sts;
        std::string stream;
        mfxU16 nframes;
        mfxU32 mem_type;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxF32 v;
        } set_par[MAX_NPARS];
        
    };

    static const tc_struct test_case[];
};

class Verifier : public tsSurfaceProcessor
{
    std::vector<mfxExtTimeCode> Ref;
    mfxU32 frame;
public:
    Verifier(std::vector<mfxExtTimeCode> const& refCodes)  : frame(0)
    {
        Ref = refCodes;
    }
    ~Verifier() {}

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (s.Data.NumExtParam != 1)
        {
            g_tsLog << "ERROR: expected number of external buffers is not equal 1\n";
            return MFX_ERR_ABORTED;
        }
        if (s.Data.ExtParam[0] == NULL)
        {
            g_tsLog << "ERROR: null pointer ExtParam\n";
            return MFX_ERR_NULL_PTR;
        }
        if (s.Data.ExtParam[0]->BufferId != MFX_EXTBUFF_TIME_CODE ||
            s.Data.ExtParam[0]->BufferSz == 0)
        {
            g_tsLog << "ERROR: Wrong external buffer or zero size buffer\n";
            return MFX_ERR_NOT_INITIALIZED;
        }

        mfxU16 buf_drop = ((mfxExtTimeCode*)s.Data.ExtParam[0])->DropFrameFlag;
        mfxU16 buf_hours = ((mfxExtTimeCode*)s.Data.ExtParam[0])->TimeCodeHours;
        mfxU16 buf_minutes = ((mfxExtTimeCode*)s.Data.ExtParam[0])->TimeCodeMinutes;
        mfxU16 buf_seconds = ((mfxExtTimeCode*)s.Data.ExtParam[0])->TimeCodeSeconds;
        mfxU16 buf_pictures = ((mfxExtTimeCode*)s.Data.ExtParam[0])->TimeCodePictures;

        if (buf_drop == Ref[frame].DropFrameFlag)
        {
            g_tsLog << "Drop Frame Flag is the same (" << buf_drop << ") frame #" << frame <<" \n";
        }
        else
        {
            g_tsLog << "ERROR: Drop Frame Flag is " << Ref[frame].DropFrameFlag << " in stream and " << buf_drop
                    << " in buffer (frame #" << frame << ")\n";
            return MFX_ERR_ABORTED;
        }

        if (buf_hours == Ref[frame].TimeCodeHours)
        {
            g_tsLog << "Time Code Hour is the same (" << buf_hours << ") frame #" << frame <<" \n";
        }
        else
        {
            g_tsLog << "ERROR: Time Code Hour is " << Ref[frame].TimeCodeHours << " in stream and " << buf_hours
                    << " in buffer (frame #" << frame << ")\n";
            return MFX_ERR_ABORTED;
        }

        if (buf_minutes == Ref[frame].TimeCodeMinutes)
        {
            g_tsLog << "Time Code Minutes is the same (" << buf_minutes << ") frame #" << frame <<" \n";
        }
        else
        {
            g_tsLog << "ERROR: Time Code Minutes is " << Ref[frame].TimeCodeMinutes << " in stream and " << buf_minutes
                    << " in buffer (frame #" << frame << ")\n";
            return MFX_ERR_ABORTED;
        }

        if (buf_seconds == Ref[frame].TimeCodeSeconds)
        {
            g_tsLog << "Time Code Seconds is the same (" << buf_seconds << ") frame #" << frame <<" \n";
        }
        else
        {
            g_tsLog << "ERROR: Time Code Seconds is " << Ref[frame].TimeCodeSeconds << " in stream and " << buf_seconds
                    << " in buffer (frame #" << frame << ")\n";
            return MFX_ERR_ABORTED;
        }

        if (buf_pictures == Ref[frame].TimeCodePictures)
        {
            g_tsLog << "Time Code Pictures is the same (" << buf_pictures << ") frame #" << frame <<" \n";
        }
        else
        {
            g_tsLog << "ERROR: Time Code Pictures is " << Ref[frame].TimeCodePictures << " in stream and " << buf_pictures
                    << " in buffer (frame #" << frame << ")\n";
            return MFX_ERR_ABORTED;
        }

        frame++;
        return MFX_ERR_NONE;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, "conformance/mpeg2/bsd_pak_test/bugs_int_IPB.mpg", 30},
    {/*01*/ MFX_ERR_NONE, "conformance/mpeg2/bsd_pak_test/bugs_prg_I.mpg", 30},
    {/*02*/ MFX_ERR_NONE, "conformance/mpeg2/bsd_pak_test/bugs_prg_IP.mpg", 30},
    {/*03*/ MFX_ERR_NONE, "conformance/mpeg2/bsd_pak_test/bugs_prg_IPB.mpg", 30},
    {/*04*/ MFX_ERR_NONE, "conformance/mpeg2/streams/normal.mpg", 1000},
    {/*05*/ MFX_ERR_NONE, "conformance/mpeg2/streams/tcela-15.bits", 60},
    {/*06*/ MFX_ERR_NONE, "conformance/mpeg2/streams/still.mpg", 1000},
    {/*07*/ MFX_ERR_NONE, "conformance/mpeg2/bsd_pak_test/bugs_int_IPB.mpg", 30, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*08*/ MFX_ERR_NONE, "conformance/mpeg2/bsd_pak_test/bugs_prg_I.mpg", 30, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*09*/ MFX_ERR_NONE, "conformance/mpeg2/bsd_pak_test/bugs_prg_IP.mpg", 30, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*10*/ MFX_ERR_NONE, "conformance/mpeg2/bsd_pak_test/bugs_prg_IPB.mpg", 30, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*11*/ MFX_ERR_NONE, "conformance/mpeg2/streams/normal.mpg", 1000, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*12*/ MFX_ERR_NONE, "conformance/mpeg2/streams/tcela-15.bits", 60, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
    {/*13*/ MFX_ERR_NONE, "conformance/mpeg2/streams/still.mpg", 1000, ALLOC_OPAQUE,
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY},
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    MFXInit();

    const tc_struct& tc = test_case[id];

    const char* sname = g_tsStreamPool.Get(tc.stream);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 100000);
    m_bs_processor = &reader;

    std::vector<mfxExtTimeCode> refCodes;
    refCodes.clear();

    //pasrsing
    BSErr err = BS_ERR_NONE;
    err = tsParserMPEG2::open(sname);
    tsParserMPEG2::set_trace_level(0/*BS_MPEG2_TRACE_LEVEL_START_CODE*/);
    UnitType* hdr = 0;
    byte pictype = 0;
    mfxU16 DropFrameFlag = 0;
    mfxU16 TimeCodeHours = 0;
    mfxU16 TimeCodeMinutes = 0;
    mfxU16 TimeCodeSeconds = 0;
    mfxU16 TimeCodePictures = 0;

    do {
        mfxU64 curr = get_offset();
        err = parse_next_unit();
        hdr = (UnitType*)get_header();

        mfxExtTimeCode ref_code = {}; //may not compile on linux
        memset(&ref_code, 0, sizeof(ref_code));

        if ((BS_ERR_MORE_DATA == err) || !hdr)
        {
            break;
        }

        if (hdr && hdr->start_code == 0x00000100) //pic header
        {
            if (hdr->pic_hdr->picture_coding_type == 2 || hdr->pic_hdr->picture_coding_type == 3)
            {
                ref_code.DropFrameFlag = DropFrameFlag;
                ref_code.TimeCodeHours = TimeCodeHours;
                ref_code.TimeCodeMinutes = TimeCodeMinutes;
                ref_code.TimeCodeSeconds = TimeCodeSeconds;
                ref_code.TimeCodePictures = TimeCodePictures;
                // push into vector
                refCodes.push_back(ref_code);
            }
        }

        if ((BS_ERR_NONE == err) && (hdr && hdr->start_code == 0x000001B8 && hdr->gop_hdr)) // gop header
        {
            //hdr->gop_hdr->time_code = 0;
            //parse time_code into hours,mins, secs.....
            TimeCodePictures = (hdr->gop_hdr->time_code & 0x3F);
            TimeCodeSeconds = (hdr->gop_hdr->time_code & 0xFC0) >> 6;
            TimeCodeMinutes = (hdr->gop_hdr->time_code & 0x7E000) >> 13;
            TimeCodeHours = (hdr->gop_hdr->time_code & 0xF80000) >> 19;
            DropFrameFlag = (hdr->gop_hdr->time_code & 0x1000000) >> 20;

            ref_code.DropFrameFlag = DropFrameFlag;
            ref_code.TimeCodeHours = TimeCodeHours;
            ref_code.TimeCodeMinutes = TimeCodeMinutes;
            ref_code.TimeCodeSeconds = TimeCodeSeconds;
            ref_code.TimeCodePictures = TimeCodePictures;
            // push into vector
            refCodes.push_back(ref_code);
        }

    } while(hdr);


    if (tc.mem_type == ALLOC_OPAQUE)
    {
        SETPARS(m_pPar, MFX_PAR);
        AllocSurfaces();

        m_par.AddExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION, sizeof(mfxExtOpaqueSurfaceAlloc));
        mfxExtOpaqueSurfaceAlloc *osa = (mfxExtOpaqueSurfaceAlloc*)m_par.GetExtBuffer(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        m_request.Type = MFX_MEMTYPE_OPAQUE_FRAME;
        m_request.NumFrameSuggested = m_request.NumFrameMin;

        MFXVideoDECODE_QueryIOSurf(m_session, m_pPar, &m_request);

        AllocOpaque(m_request, *osa);
        Init(m_session, m_pPar);
    }
    else
    {
        AllocSurfaces();
        Init();
    }

    for (mfxU32 i = 0; i < PoolSize(); i++)
    {
        mfxFrameSurface1* p = GetSurface(i);
        p->Data.ExtParam = new mfxExtBuffer*[1];
        p->Data.ExtParam[0] = (mfxExtBuffer*)new mfxExtTimeCode;
        memset(p->Data.ExtParam[0], 0, sizeof(mfxExtTimeCode));
        p->Data.ExtParam[0]->BufferId = MFX_EXTBUFF_TIME_CODE;
        p->Data.ExtParam[0]->BufferSz = sizeof(mfxExtTimeCode);
        p->Data.NumExtParam = 1;
    }

    Verifier v(refCodes);
    m_surf_processor = &v;
    DecodeFrames(tc.nframes);
    g_tsStatus.expect(tc.sts);


    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(mpeg2d_timecode);

}
