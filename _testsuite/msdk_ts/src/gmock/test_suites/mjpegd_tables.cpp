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
#include "ts_common.h"

#define BEH_TEST_WIDTH              352
#define BEH_TEST_HEIGHT             288

mfxExtJPEGQuantTables JPEG_DEFAULT_QT = {
    MFX_EXTBUFF_JPEG_QT, sizeof(mfxExtJPEGQuantTables), //Header
    {0,0,0,0,0,0,0},//mfxU16  reserved[7];
    2,//mfxU16  NumTable;
    //mfxU16    Qm[4][64];
    {
        {
            16,  11,  12,  14,  12,  10,  16,  14,
                13,  14,  18,  17,  16,  19,  24,  40,
                26,  24,  22,  22,  24,  49,  35,  37,
                29,  40,  58,  51,  61,  60,  57,  51,
                56,  55,  64,  72,  92,  78,  64,  68,
                87,  69,  55,  56,  80, 109,  81,  87,
                95,  98, 103, 104, 103,  62,  77, 113,
                121, 112, 100, 120,  92, 101, 103,  99
        },
        {
            17,  18,  18,  24,  21,  24,  47,  26,
                26,  47,  99,  66,  56,  66,  99,  99,
                99,  99,  99,  99,  99,  99,  99,  99,
                99,  99,  99,  99,  99,  99,  99,  99,
                99,  99,  99,  99,  99,  99,  99,  99,
                99,  99,  99,  99,  99,  99,  99,  99,
                99,  99,  99,  99,  99,  99,  99,  99,
                99,  99,  99,  99,  99,  99,  99,  99
            },
            {0,},
            {0,}
    }
};

mfxExtJPEGHuffmanTables JPEG_DEFAULT_HT = {
    MFX_EXTBUFF_JPEG_HUFFMAN, sizeof(mfxExtJPEGHuffmanTables), //Header

    {0,0}, //mfxU16  reserved[2];
    2, //mfxU16  NumDCTable;
    2, //mfxU16  NumACTable;

    {
        {
            {
                0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01,
                    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
            },//mfxU8   Bits[16];
            {
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                    0x08, 0x09, 0x0a, 0x0b
                }//mfxU8   Values[12];
        },
        {
            {
                0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
                    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
            },//mfxU8   Bits[16];
            {
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                    0x08, 0x09, 0x0a, 0x0b
                }//mfxU8   Values[12];
        },
        {
            {0,},//mfxU8   Bits[16];
            {0,}//mfxU8   Values[12];
        },
        {
            {0,},//mfxU8   Bits[16];
            {0,}//mfxU8   Values[12];
        }
    }, //DCTables[4];

    {
        {
            {
                0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
                    0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7d
            },//mfxU8   Bits[16];
            {
                0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
                    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
                    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
                    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
                    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
                    0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
                    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
                    0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
                    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
                    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
                    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
                    0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
                    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
                    0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
                    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
                    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
                    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
                    0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
                    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
                    0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
                    0xf9, 0xfa
                }//mfxU8   Values[162];
        },
        {
            {
                0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04,
                    0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77
            },//mfxU8   Bits[16];
            {
                0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
                    0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
                    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
                    0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
                    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
                    0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
                    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
                    0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
                    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
                    0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
                    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
                    0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
                    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
                    0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
                    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
                    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
                    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
                    0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
                    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
                    0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
                    0xf9, 0xfa
                }//mfxU8   Values[162];
        },
        {
            {0,},//mfxU8   Bits[16];
            {0,}//mfxU8   Values[162];
        },
        {
            {0,},//mfxU8   Bits[16];
            {0,}//mfxU8   Values[162];
        }
    }// ACTables[4];
};

//#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace mjpegd_tables
{

    mfxExtJPEGQuantTables ChangeQt(mfxExtJPEGQuantTables qt)
    {
        mfxExtJPEGQuantTables tmp = qt;
        if (tmp.Qm[0][0] != 11)
            tmp.Qm[0][0] = 11;
        else
            tmp.Qm[0][0] = 12;
        return tmp;
    }

    mfxExtJPEGHuffmanTables ChangeHt(mfxExtJPEGHuffmanTables ht)
    {
        mfxExtJPEGHuffmanTables tmp = ht;
        if (tmp.ACTables[0].Values[0] != 1)
            tmp.ACTables[0].Values[0] = 1;
        else
            tmp.ACTables[0].Values[0] = 0;
        return tmp;
    }

    mfxStatus getJPEGTables(mfxBitstream* pBs, BS_JPEG_parser* parser, mfxExtJPEGHuffmanTables* pHT, mfxExtJPEGQuantTables* pQT, mfxU32 offset)
    {
        mfxStatus mfxRes = MFX_ERR_NONE;
        BSErr bs_sts = BS_ERR_NONE;
        BS_JPEG_header* hdr = NULL;
        Bs8u num_qt[4] = {0};
        Bs8u num_ht[2][4] = {0};

        if(!parser) return MFX_ERR_NULL_PTR;

        if(pBs)
        {
            bs_sts = parser->set_buffer(pBs->Data + (pBs->DataOffset + offset), pBs->DataLength - (pBs->DataOffset + offset));
            if(bs_sts != BS_ERR_NONE) {return MFX_ERR_ABORTED;}
        }

        while(1)
        {
            bs_sts = parser->parse_next_unit();
            if(bs_sts == BS_ERR_NOT_IMPLEMENTED) continue;
            if(bs_sts == BS_ERR_MORE_DATA) break;
            if(bs_sts != BS_ERR_NONE) {return MFX_ERR_ABORTED;}
            hdr = (BS_JPEG_header*)parser->get_header();
            if(hdr->marker == 0xFFC4/*DHT*/ && pHT){
                Bs16u n = hdr->ht->n;
                while(n--){
                    BS_JPEG_ht::huffman_table& ht = hdr->ht->h[n];
                    num_ht[ht.Tc][ht.Th] = 1;
                    if(ht.Tc){
                        memcpy(pHT->ACTables[ht.Th].Bits, ht.L, 16);
                        memcpy(pHT->ACTables[ht.Th].Values, ht.V, 162);
                    } else {
                        memcpy(pHT->DCTables[ht.Th].Bits, ht.L, 16);
                        memcpy(pHT->DCTables[ht.Th].Values, ht.V, 12);
                    }
                }
            } else if(hdr->marker == 0xFFDB/*DQT*/ && pQT){
                Bs16u n = hdr->qt->n;
                while(n--){
                    BS_JPEG_qt::quant_table& qt = hdr->qt->q[n];
                    qt.Tq = qt.Tq%4;
                    num_qt[qt.Tq] = 1;
                    if(qt.Pq)
                        memcpy(pQT->Qm+qt.Tq, qt.Q16, 2*64);
                    else
                        for(mfxU16 i = 0; i < 64; i++ )
                            pQT->Qm[qt.Tq][i] = qt.Q8[i];
                }
            } else if(hdr->marker == 0xFFD9/*EOI*/)
                break;
        }

        if(pQT) pQT->NumTable = BS_MAX(pQT->NumTable, num_qt[0]+num_qt[1]+num_qt[2]+num_qt[3]);
        if(pHT)
        {
            pHT->NumACTable = BS_MAX(pHT->NumACTable, num_ht[1][0]+num_ht[1][1]+num_ht[1][2]+num_ht[1][3]);
            pHT->NumDCTable = BS_MAX(pHT->NumDCTable, num_ht[0][0]+num_ht[0][1]+num_ht[0][2]+num_ht[0][3]);
        }

exit:
        return MFX_ERR_NONE;
    }

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
            struct tables {
                const mfxU32 num_frames;
                std::string in_file;
                mfxU32 qt_init;
                mfxU32 ht_init;
                mfxU32 qt;
                mfxU32 ht;

            } table_ctr;
        };

        static const tc_struct test_case[];

        enum
        {
            //modes

            NONE,
            _1_FRAME_PAR_2_FRAME_CTRL,
            _1_FRAME_CTRL_2_FRAME_CTRL,
            _3_FRAME_CTRL

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
        //table ctrl
        {1, "forBehaviorTest/foreman_cif.yuv", 0, 0, 1, 0}
        },
        {/* 1*/ MFX_ERR_NONE, NONE,
        {},
        //table ctrl
        {1, "forBehaviorTest/foreman_cif.yuv", 0, 0, 0, 0},
        },
        {/* 2*/ MFX_ERR_NONE, NONE,
        {},
        //table ctrl
        {1, "forBehaviorTest/foreman_cif.yuv", 0, 1, 0, 0},
        },
        {/* 3*/ MFX_ERR_NONE, NONE,
        {},
        //table ctrl
        {1, "forBehaviorTest/foreman_cif.yuv", 0, 0, 0, 1},
        },
        {/* 4*/ MFX_ERR_NONE, NONE,
        {},
        //table ctrl
        {1, "forBehaviorTest/foreman_cif.yuv", 0, 2, 0, 1},
        },
        {/* 5*/ MFX_ERR_NONE, _1_FRAME_PAR_2_FRAME_CTRL,
        {},
        //table ctrl
        {2, "forBehaviorTest/foreman_cif.yuv", 0, 1, 0, 1},
        },
        {/* 6*/ MFX_ERR_NONE, _1_FRAME_CTRL_2_FRAME_CTRL,
        {},
        //table ctrl
        {2, "forBehaviorTest/foreman_cif.yuv", 0, 0, 0, 1},
        },
        {/* 7*/ MFX_ERR_NONE, _3_FRAME_CTRL,
        {},
        //table ctrl
        {11, "forBehaviorTest/foreman_cif.yuv", 0, 0, 0, 1},
        },
        {/* 8*/ MFX_ERR_NONE, NONE,
        {},
        //table ctrl
        {1, "forBehaviorTest/foreman_cif.yuv", 1, 0, 0, 0},
        },
        {/* 9*/ MFX_ERR_NONE, NONE,
        {},
        //table ctrl
        {1, "forBehaviorTest/foreman_cif.yuv", 0, 0, 1, 0},
        },
        {/* 10*/ MFX_ERR_NONE, NONE,
        {},
        //table ctrl
        {1, "forBehaviorTest/foreman_cif.yuv", 2, 0, 1, 0},
        },
        {/* 11*/ MFX_ERR_NONE, _1_FRAME_PAR_2_FRAME_CTRL,
        {},
        //table ctrl
        {2, "forBehaviorTest/foreman_cif.yuv", 1, 0, 1, 0},
        },
        {/* 12*/ MFX_ERR_NONE, _1_FRAME_CTRL_2_FRAME_CTRL,
        {},
        //table ctrl
        {2, "forBehaviorTest/foreman_cif.yuv", 0, 0, 1, 0},
        },
        {/* 13*/ MFX_ERR_NONE, _3_FRAME_CTRL,
        {},
        //table ctrl
        {11, "forBehaviorTest/foreman_cif.yuv", 0, 0, 1, 0},
        },
        {/* 14*/ MFX_ERR_NONE, NONE,
        {},
        //table ctrl
        {1, "forBehaviorTest/foreman_cif.yuv", 1, 1, 0, 0},
        },
        {/* 15*/ MFX_ERR_NONE, NONE,
        {},
        //table ctrl
        {1, "forBehaviorTest/foreman_cif.yuv", 0, 0, 1, 1},
        },
        {/* 16*/ MFX_ERR_NONE, NONE,
        {},
        //table ctrl
        {1, "forBehaviorTest/foreman_cif.yuv", 2, 2, 1, 1},
        },
        {/* 17*/ MFX_ERR_NONE, _1_FRAME_PAR_2_FRAME_CTRL,
        {},
        //table ctrl
        {2, "forBehaviorTest/foreman_cif.yuv", 1, 1, 1, 1},
        },
        {/* 18*/ MFX_ERR_NONE, _1_FRAME_CTRL_2_FRAME_CTRL,
        {},
        //table ctrl
        {2, "forBehaviorTest/foreman_cif.yuv", 0, 0, 1, 1},
        },
        {/* 19*/ MFX_ERR_NONE, _3_FRAME_CTRL,
        {},
        //table ctrl
        {11, "forBehaviorTest/foreman_cif.yuv", 0, 0, 1, 1},
        }

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

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

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        mfxStatus mfxRes;
        std::map<const char*, mfxExtBuffer*> ebm;
        const tc_struct& tc = test_case[id];
        mfxStatus expected = tc.sts;
        apply_par(tc, T_CTR);
        
        MFXInit();
        TRACE_FUNC3(MFXVideoENCODE_Query, m_session, m_pPar, m_pParOut);
        expected = MFXVideoENCODE_Query(m_session, m_pPar, m_pParOut);
        TS_TRACE(m_pParOut);
        if ((expected != MFX_ERR_NONE) && (expected != MFX_WRN_PARTIAL_ACCELERATION))
        {
            g_tsLog<<"Failed in Query"<<"\n";
            throw tsFAIL;
        }

        mfxExtJPEGQuantTables qt_expect;
        mfxExtJPEGHuffmanTables ht_expect;
        mfxExtJPEGQuantTables qt0;
        qt0.Header.BufferId = MFX_EXTBUFF_JPEG_QT;
        qt0.Header.BufferSz = sizeof(mfxExtJPEGQuantTables);
        mfxExtJPEGHuffmanTables ht0;
        ht0.Header.BufferId = MFX_EXTBUFF_JPEG_HUFFMAN;
        ht0.Header.BufferSz = sizeof(mfxExtJPEGHuffmanTables);
        BS_parser* parser = new BS_JPEG_parser;
        mfxExtJPEGQuantTables   qt = {0};
        mfxExtJPEGHuffmanTables ht = {0};
        qt.Header = qt0.Header;
        ht.Header = ht0.Header;
        mfxExtBuffer** ppExtBuff = (mfxExtBuffer**)calloc(2, sizeof(mfxExtBuffer*));
        mfxExtBuffer** ppExtBuffInit = (mfxExtBuffer**)calloc(2, sizeof(mfxExtBuffer*));


        SetFrameAllocator();
        std::string file = g_tsStreamPool.Get(tc.table_ctr.in_file.c_str());
        SurfProc proc(file.c_str(), m_pPar->mfx.FrameInfo, tc.table_ctr.num_frames);
        proc.Init(m_ctrl);
        m_filler = &proc;
        g_tsStreamPool.Reg();
        ///////////////////////////////////////////////////////////////////////////
        mfxU16 qt_set_init = tc.table_ctr.qt_init;
        mfxU16 ht_set_init = tc.table_ctr.ht_init;
        mfxU16 qt_set = tc.table_ctr.qt;
        mfxU16 ht_set = tc.table_ctr.ht;
        mfxU32 offset = 0;
        mfxU16 tmp;
        mfxU16 set_ctrl; // don't set, 1 set qt, 2 set ht, 2 set all
        mfxU16 enc_frames = 0;
        for(;;)
        {

            //set tables
            memset((mfxU8*)&qt+sizeof(mfxExtBuffer), 0, sizeof(mfxExtJPEGQuantTables)-sizeof(mfxExtBuffer)); 
            memset((mfxU8*)&ht+sizeof(mfxExtBuffer), 0, sizeof(mfxExtJPEGHuffmanTables)-sizeof(mfxExtBuffer));
            memset((mfxU8*)&qt0+sizeof(mfxExtBuffer), 0, sizeof(mfxExtJPEGQuantTables)-sizeof(mfxExtBuffer)); 
            memset((mfxU8*)&ht0+sizeof(mfxExtBuffer), 0, sizeof(mfxExtJPEGHuffmanTables)-sizeof(mfxExtBuffer));
            qt0 = JPEG_DEFAULT_QT;
            ht0 = JPEG_DEFAULT_HT;
            ht = ChangeHt(ht0);
            qt = ChangeQt(qt0);
            qt_expect = qt0;
            ht_expect = ht0;


            if (tc.table_ctr.qt || tc.table_ctr.ht)
            {

                m_pCtrl->NumExtParam = 0;
                m_pCtrl->ExtParam = ppExtBuff;
                m_pCtrl->ExtParam[0] = NULL;
                m_pCtrl->ExtParam[1] = NULL;
            }
            if (tc.table_ctr.qt_init || tc.table_ctr.ht_init)
            {
                m_pPar->NumExtParam = 0;
                m_pPar->ExtParam = ppExtBuffInit;
                m_pPar->ExtParam[0] = NULL;
                m_pPar->ExtParam[1] = NULL;
            }

            if (tc.mod == _1_FRAME_PAR_2_FRAME_CTRL) // 1 frame -par, 2 frame - init
            {
                if (enc_frames%2 == 0)
                {
                    if (tc.table_ctr.qt_init) 
                    {
                        m_pPar->ExtParam[m_pPar->NumExtParam++] = &(qt0.Header); 
                        qt_expect = qt0;
                    }
                    if (tc.table_ctr.ht_init) 
                    {
                        m_pPar->ExtParam[m_pPar->NumExtParam++] = &(ht0.Header);
                        ht_expect = ht0;
                    }
                    tmp = 0;
                    if (tc.table_ctr.qt_init == 2) 
                    {
                        m_pPar->ExtParam[tmp++] = &(qt.Header); 
                        qt_expect = qt;
                    }
                    if (tc.table_ctr.ht_init == 2) 
                    {
                        m_pPar->ExtParam[tmp++] = &(ht.Header);
                        ht_expect = ht;
                    }
                }
                else
                {
                    if (tc.table_ctr.qt) {m_pCtrl->ExtParam[m_pCtrl->NumExtParam++] = &(qt0.Header); qt_expect = qt0;}
                    if (tc.table_ctr.ht) {m_pCtrl->ExtParam[m_pCtrl->NumExtParam++] = &(ht0.Header); ht_expect = ht0;}
                    tmp = 0;
                    if (tc.table_ctr.qt == 2) {m_pCtrl->ExtParam[tmp++] = &(qt.Header); qt_expect = qt;}
                    if (tc.table_ctr.ht == 2) {m_pCtrl->ExtParam[tmp++] = &(ht.Header); ht_expect = ht;}
                }

            }
            else if (tc.mod == _1_FRAME_CTRL_2_FRAME_CTRL)
            {
                if (enc_frames%2 == 0)
                {
                    if (tc.table_ctr.qt) {m_pCtrl->ExtParam[m_pCtrl->NumExtParam++] = &(qt0.Header); qt_expect = qt0;}
                    if (tc.table_ctr.ht) {m_pCtrl->ExtParam[m_pCtrl->NumExtParam++] = &(ht0.Header); ht_expect = ht0;}
                }
                else
                {
                    if (tc.table_ctr.qt) {m_pCtrl->ExtParam[m_pCtrl->NumExtParam++] = &(qt0.Header); qt_expect = qt0;}
                    if (tc.table_ctr.ht) {m_pCtrl->ExtParam[m_pCtrl->NumExtParam++] = &(ht0.Header); ht_expect = ht0;}
                }

                if (tc.table_ctr.qt == 2) {m_pCtrl->ExtParam[m_pCtrl->NumExtParam++] = &(qt.Header); qt_expect = qt;}
                if (tc.table_ctr.ht == 2) {m_pCtrl->ExtParam[m_pCtrl->NumExtParam++] = &(ht.Header); ht_expect = ht;}
            }
            else if (tc.mod == _3_FRAME_CTRL) // init every 3-th frame
            {
                if (enc_frames%3 == 0)
                {
                    if (tc.table_ctr.qt_init) {m_pPar->ExtParam[m_pPar->NumExtParam++] = &(qt0.Header); qt_expect = qt0;}
                    if (tc.table_ctr.ht_init) {m_pPar->ExtParam[m_pPar->NumExtParam++] = &(ht0.Header); ht_expect = ht0;}
                    if (tc.table_ctr.qt) {m_pCtrl->ExtParam[m_pCtrl->NumExtParam++] = &(qt0.Header); qt_expect = qt0;}
                    if (tc.table_ctr.ht) {m_pCtrl->ExtParam[m_pCtrl->NumExtParam++] = &(ht0.Header); ht_expect = ht0;}
                    tmp = 0;
                    if (tc.table_ctr.qt_init == 2) 
                    {
                        m_pPar->ExtParam[tmp++] = &(qt.Header); 
                        if (!tc.table_ctr.qt)
                            qt_expect = qt;
                    }
                    if (tc.table_ctr.ht_init == 2) 
                    {
                        m_pPar->ExtParam[tmp++] = &(ht.Header);
                        if (!tc.table_ctr.ht)
                            ht_expect = ht;
                    }
                    tmp = 0;
                    if (tc.table_ctr.qt == 2) {m_pCtrl->ExtParam[tmp++] = &(qt.Header); qt_expect = qt;}
                    if (tc.table_ctr.ht == 2) {m_pCtrl->ExtParam[tmp++] = &(ht.Header); ht_expect = ht;}

                    qt_set_init = tc.table_ctr.qt_init;
                    qt_set = tc.table_ctr.qt;
                    ht_set_init = tc.table_ctr.ht_init;
                    ht_set = tc.table_ctr.ht;
                }
                else
                {
                    qt_set_init = 0;
                    qt_set = 0;
                    ht_set_init = 0;
                    ht_set = 0;
                }
            }
            else // defoult init
            {
                if (tc.table_ctr.qt_init) {m_pPar->ExtParam[m_pPar->NumExtParam++] = &(qt0.Header); qt_expect = qt0;}
                if (tc.table_ctr.ht_init) {m_pPar->ExtParam[m_pPar->NumExtParam++] = &(ht0.Header); ht_expect = ht0;}
                if (tc.table_ctr.qt) {m_pCtrl->ExtParam[m_pCtrl->NumExtParam++] = &(qt0.Header); qt_expect = qt0;}
                if (tc.table_ctr.ht) {m_pCtrl->ExtParam[m_pCtrl->NumExtParam++] = &(ht0.Header); ht_expect = ht0;}
                tmp = 0;
                if (tc.table_ctr.qt_init == 2) 
                {
                    m_pPar->ExtParam[tmp++] = &(qt.Header); 
                    if (!tc.table_ctr.qt)
                        qt_expect = qt;
                }
                if (tc.table_ctr.ht_init == 2) 
                {
                    m_pPar->ExtParam[tmp++] = &(ht.Header);
                    if (!tc.table_ctr.ht)
                        ht_expect = ht;
                }
                tmp = 0;
                if (tc.table_ctr.qt == 2) {m_pCtrl->ExtParam[tmp++] = &(qt.Header); qt_expect = qt;}
                if (tc.table_ctr.ht == 2) {m_pCtrl->ExtParam[tmp++] = &(ht.Header); ht_expect = ht;}
            }
            m_pPar->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
            m_pPar->mfx.BufferSizeInKB = (mfxU16)(BEH_TEST_WIDTH * BEH_TEST_HEIGHT * 4 / 1024);
            mfxU32 pic_struct = 52428;
            m_pPar->mfx.FrameInfo.PicStruct = ((pic_struct&(MFX_PICSTRUCT_FIELD_TFF|MFX_PICSTRUCT_FIELD_BFF)) ? MFX_PICSTRUCT_UNKNOWN : MFX_PICSTRUCT_PROGRESSIVE);
            m_pPar->mfx.Quality = 1;
            g_tsStatus.expect(expected);
            Init(m_session, m_pPar);

            if (enc_frames == 0)
                g_tsStatus.expect(expected);
            EncodeFrameAsync();
            if(g_tsStatus.get() == MFX_ERR_MORE_DATA)
                continue;
            if(g_tsStatus.get() == MFX_ERR_NONE)
            {
                

                //check tables
                mfxExtBuffer* tmp_eb[2] = {&qt.Header, &ht.Header};
                mfxVideoParam vp = {0};

                vp.NumExtParam = 2;
                vp.ExtParam = tmp_eb;
                GetVideoParam(m_session, &vp);
                
                SyncOperation();TS_CHECK_MFX;

                memset((mfxU8*)&qt+sizeof(mfxExtBuffer), 0, sizeof(mfxExtJPEGQuantTables)-sizeof(mfxExtBuffer)); 
                memset((mfxU8*)&ht+sizeof(mfxExtBuffer), 0, sizeof(mfxExtJPEGHuffmanTables)-sizeof(mfxExtBuffer));

                mfxRes = getJPEGTables(m_pBitstream, (BS_JPEG_parser*)parser, &ht, &qt, offset);
                offset = m_pBitstream->DataLength;
                g_tsStatus.check(mfxRes);
                if (qt_set || qt_set_init)
                {
                    if(memcmp(&qt_expect, &qt, sizeof(mfxExtJPEGQuantTables)))
                    {
                        g_tsLog<<"FILED QT from OutBitstream is wrong!"<<"\n";
                        throw tsFAIL;
                    }
                }
                else if ((qt.NumTable == 0))
                {
                    g_tsLog<<"FILED QT from OutBitstream is wrong!"<<"\n";
                    throw tsFAIL;
                }
                if (ht_set || ht_set_init)
                {
                    if(memcmp(&ht_expect, &ht, sizeof(mfxExtJPEGHuffmanTables)))
                    {
                        g_tsLog<<"FILED HT from OutBitstream is wrong!"<<"\n";
                        throw tsFAIL;
                    }
                }
                else if ((ht.NumACTable == 0) && (tc.table_ctr.in_file.find('none') != tc.table_ctr.in_file.npos))
                {
                    g_tsLog<<"FILED HT from OutBitstream is wrong!"<<"\n";
                    throw tsFAIL;
                }

                enc_frames++;
                Close();
                if(enc_frames >= tc.table_ctr.num_frames)
                    break;
            }
            else if (tc.sts == g_tsStatus.get())
            {
                g_tsLog<<"OK"<<"\n";
            }
            else
            {
                g_tsLog<<"FILED EncodeFrameAsync!"<<"\n";
            }
        }

        g_tsStatus.check();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(mjpegd_tables);

}