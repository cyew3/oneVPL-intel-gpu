/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_alloc.h"
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

void WriteCOM(char*  comment, char* ptr)
{
    int   i;
    int   len;
    char  buf[256];

    buf[0] = 0;
    ptr = &buf[0];
    len = (int)strlen(ptr) + 1;
    if(comment != 0)
    {
        int sz;
        sz = (int)BS_MIN(strlen(comment),127);

        for(int i = 0; i < sz; i++)
            ptr[len + i] = comment[i];

        len += sz + 1;
        ptr[len-1] = 0;
    }

    len += 2;


}


namespace mjpegd_payload
{

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_JPEG) {}
        ~TestSuite() {}
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

    private:
        static const mfxU32 max_num_ctrl     = 3;
        static const mfxU32 max_num_reads   = 5;



        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 mod;
            struct tctrl{
                mfxU32 type;
                const tsStruct::Field* field;
                mfxU32 par;
            } ctrl[max_num_ctrl];
            struct payloads {
                mfxU32 payload;

            } payload_set;
        };

        static const tc_struct test_case[];

        enum
        {
            //modes

            NONE

        };

        enum CTRL_TYPE
        {
            STAGE = 0xFF000000
            ,MFXVPAR
            ,T_CTR
        };

        void apply_par(const tc_struct& p, mfxU32 stage)
        {


            for(mfxU32 i = 0; i < max_num_ctrl; i ++)
            {
                auto c = p.ctrl[i];

                void** base = 0;

                if(stage != (c.type))
                    continue;

                switch(c.type)
                {
                case MFXVPAR   : base = (void**)&m_pPar;         break;


                default: break;
                }

                if(base)
                {
                    if(c.field)
                        tsStruct::set(*base, *c.field, c.par);
                    else
                        *base = (void*)c.par;
                }
            }

        }
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/* 0*/ MFX_ERR_NONE, NONE,
        {},
        //payload set
        {0}
        },
        {/* 1*/ MFX_ERR_NONE, NONE,
        {},
        //payload set
        {1}
        },
        {/* 2*/ MFX_ERR_NONE, NONE,
        {},
        //payload set
        {2}
        }

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);


    //////////////////////////////////////////////////////////////
    class SurfProc : public tsSurfaceProcessor
    {
        std::string m_file_name;
        mfxU32 m_nframes;
        tsRawReader m_raw_reader;
        mfxEncodeCtrl* pCtrl;
        mfxFrameInfo* pFrmInfo;
        mfxU32 m_curr_frame;
        mfxU32 m_target_frames;
    public:
        SurfProc(const char* fname, mfxFrameInfo& fi, mfxU32 n_frames)
            : tsSurfaceProcessor()
            , m_file_name(fname)
            , m_nframes(n_frames)
            , m_raw_reader(fname, fi, n_frames)
            , pCtrl(0)
            , pFrmInfo(&fi)
            , m_curr_frame(0)
            , m_target_frames(n_frames)
        {}

        ~SurfProc() {} ;

        mfxStatus Init(mfxEncodeCtrl& ctrl)
        {
            pCtrl = &ctrl;
            return MFX_ERR_NONE;
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            if (m_curr_frame >= m_nframes)
            {
                s.Data.Locked++;
                m_eos = true;
                return MFX_ERR_NONE;
            }

            mfxStatus sts = m_raw_reader.ProcessSurface(s);

            if (m_raw_reader.m_eos)  // re-read stream from the beginning to reach target NumFrames
            {
                m_raw_reader.ResetFile();
                sts = m_raw_reader.ProcessSurface(s);
            }

            m_curr_frame++;
            return sts;
        }
    };
    ////////////////////////////////////////////////////////////////

    class BsDump : public tsBitstreamProcessor, tsParserH264AU
    {
        mfxU32 m_curr_frame;
        mfxU32 numMb;

    public:
        BsDump()
            :tsParserH264AU(BS_H264_INIT_MODE_CABAC|BS_H264_INIT_MODE_CAVLC)
            , m_curr_frame(0)
            , numMb(0)

        {
            set_trace_level(0);
        }
        ~BsDump() {}
        mfxStatus Init(mfxFrameInfo& fi)
        {
            numMb = (fi.Width / 16) * (fi.Height / 16);
            return MFX_ERR_NONE;
        }

        bool check_bs(mfxBitstream* bs, mfxU32 num)
        {
            BS_JPEG_parser parser;
            BSErr bs_sts = BS_ERR_NONE;
            BS_JPEG_header* hdr = NULL;
            mfxU8 tmp = 0;
            UnitType* au;
            mfxU32 num_payloads = num;
            std::string comment;
            bs_sts =  parser.set_buffer(bs->Data + (bs->DataOffset), bs->DataLength - (bs->DataOffset));
            if(bs_sts != BS_ERR_NONE) {return false;}
            for(;;)
            {
                bs_sts = parser.parse_next_unit();
                if(bs_sts == BS_ERR_NOT_IMPLEMENTED) continue;
                if(bs_sts == BS_ERR_MORE_DATA) break;
                if(bs_sts != BS_ERR_NONE) {return false;}
                if(num_payloads == 0) {return true;}
                hdr = (BS_JPEG_header*)parser.get_header();
                au = (UnitType*)hdr->unit;
                if (hdr->marker < 65488 || hdr->marker > 65495)
                {
                    tmp = 0;
                    g_tsLog<<hdr->marker<<"\n";
                }
                if (hdr->marker == 65534)
                {
                    g_tsLog<<hdr->marker<<"\n";
                    num_payloads--;

                }


            }       

            return false;
        }


    };

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        mfxStatus mfxRes;
        std::map<const char*, mfxExtBuffer*> ebm;
        const tc_struct& tc = test_case[id];
        mfxStatus expected = MFX_ERR_NONE;
        apply_par(tc, T_CTR);
        std::string file = g_tsStreamPool.Get("forBehaviorTest/foreman_cif.yuv");


        MFXInit();

        //m_pPar->mfx.FrameInfo.PicStruct = 1; //((pic_struct&(MFX_PICSTRUCT_FIELD_TFF|MFX_PICSTRUCT_FIELD_BFF)) ? MFX_PICSTRUCT_UNKNOWN : MFX_PICSTRUCT_PROGRESSIVE);
        m_pPar->mfx.FrameInfo.Width = 352;
        m_pPar->mfx.FrameInfo.Height = 288;
        m_pPar->mfx.FrameInfo.CropW = 352;;
        m_pPar->mfx.FrameInfo.CropH = 288;
        m_pPar->IOPattern = 2;
        m_pPar->mfx.CodecProfile =1;
        m_pPar->mfx.NumThread = 4;
        m_pPar->mfx.GopPicSize = 50;
        m_pPar->mfx.ExtendedPicStruct = 50;
        m_pPar->mfx.Rotation = 50;
        m_pPar->mfx.RateControlMethod = 0;
        m_pPar->mfx.InitialDelayInKB = 0;
        m_pPar->mfx.Accuracy = 0;
        m_pPar->mfx.Accuracy = 0;
        m_pPar->mfx.TargetKbps = 0;
        m_pPar->mfx.QPP = 0;
        m_pPar->mfx.ICQQuality = 0;
        m_pPar->mfx.MaxKbps = 0;
        m_pPar->mfx.QPB = 0;
        m_pPar->mfx.Convergence = 0;


        expected = MFXVideoENCODE_Query(m_session, m_pPar, m_pParOut);
        if ((expected != MFX_ERR_NONE) && (expected != MFX_WRN_PARTIAL_ACCELERATION))
        {
            g_tsLog<<"Failed in Query"<<"\n";
            throw tsFAIL;
        }




        //init payload
        mfxPayload payload;
        payload.Type = 65534;
        mfxU8 data[256];
        char* comment = "coment";
        data[0] = 255;
        data[1] = 254;
        for (mfxU16 i = 2; i <= strlen(comment) + 2; i++)
        {
            data[i] = comment[i - 2];
        }
        payload.Data = data;
        payload.BufSize = strlen(comment) + 3;
        payload.NumBit = payload.BufSize*8;
        SetFrameAllocator();

        // setup input stream
        g_tsLog<<file.c_str()<<"\n";
        SurfProc proc(file.c_str(), m_pPar->mfx.FrameInfo, 1);
        proc.Init(m_ctrl);
        m_filler = &proc;
        g_tsStreamPool.Reg();


        ///////////////////////////////////////////////////////////////////////////
        mfxU16 enc_frames = 0;
        BsDump bsReader = BsDump();
        for(;;)
        {
            //set payload
            m_pCtrl->Payload = new mfxPayload*[tc.payload_set.payload];
            for (mfxU8 i = 0; i < tc.payload_set.payload; i++)
            {
                m_pCtrl->Payload[i] = &payload;
            }
            m_pCtrl->NumPayload = tc.payload_set.payload;


            g_tsStatus.expect(expected);
            Init(m_session, m_pPar);

            if (enc_frames == 0)
                g_tsStatus.expect(expected);
            EncodeFrameAsync();
            if(g_tsStatus.get() == MFX_ERR_MORE_DATA)
                continue;
            if(g_tsStatus.get() == MFX_ERR_NONE)
            {		
                //check payload in bitstream
                if ((!bsReader.check_bs(m_pBitstream, tc.payload_set.payload)) && (expected == MFX_ERR_NONE))
                {
                    g_tsLog<<"FILED Wrong bitstream"<<"\n";
                    throw tsFAIL;
                }



                enc_frames++;
                Close();
                if(enc_frames >= 1)
                    break;
            }
            else if (tc.sts == g_tsStatus.get())
            {
                g_tsLog<<"OK"<<"\n";
            }
            else
            {
                g_tsLog<<"FILED EncodeFrameAsync!"<<"\n";
                throw tsFAIL;
            }
        }

        g_tsStatus.check();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(mjpegd_payload);

}