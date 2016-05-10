/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

File Name: mpeg2e_user_data.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include <ctime>

#define USER_DATA_HDR 0x01B2

namespace mpeg2e_user_data
{
    class PayloadRaw
    {
    public:
        PayloadRaw(mfxU8 *payloads, size_t length) : m_payloads(payloads), m_length(length)
        {}
        ~PayloadRaw()
        {
            delete [] m_payloads;
        }
        inline mfxU8 *getPayloadRaw() {return m_payloads;}
        inline size_t getLength() {return m_length;}
    private:
        mfxU8 *m_payloads;
        size_t m_length;
    };

    class PayloadWrapper
    {
    public:
        PayloadWrapper(mfxU16 pn) : m_payloadsNumber(pn), m_counter(0)
        {
            if (pn <= 0)
                m_payloads = NULL;
            else
                m_payloads = new PayloadRaw * [pn];
        }
        ~PayloadWrapper()
        {
            if (m_payloads != NULL)
                delete [] m_payloads;
        }
        void addPayload(PayloadRaw *pr)
        {
            if (m_counter >= m_payloadsNumber)
            {
                EXPECT_EQ(0, 1) << "ERROR: cannot add payload to PayloadWrapper) \n";
                return;
            }
            m_payloads[m_counter++] = pr;
        }
        PayloadRaw *getPayload(unsigned int index)
        {
            if (index < 0 || index >= m_counter)
            {
                EXPECT_EQ(0, 1) << "ERROR: index of payload is out of bounds) \n";
                return NULL;
            }
            return m_payloads[index];
        }
        inline mfxU16 getPayloadsNumber()
        {
            return m_payloadsNumber;
        }
    private:
        mfxU16 m_payloadsNumber;
        mfxU16 m_counter;
        PayloadRaw **m_payloads;
    };

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_MPEG2) { }
        ~TestSuite() { }
        int RunTest(unsigned int id);
        static const unsigned int n_cases;
    private:
        PayloadRaw *rand_payload();
        mfxEncodeCtrl *make_ctrl(PayloadWrapper *);
        mfxEncodeCtrl *attach_payloads(PayloadWrapper *);
        mfxPayload *new_payload(PayloadRaw *);
    };

    const unsigned int TestSuite::n_cases = 1;

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        m_par.mfx.CodecId = MFX_CODEC_MPEG2;
        m_par.mfx.TargetKbps = 5000;
        m_par.mfx.RateControlMethod = 1;
        m_par.mfx.GopRefDist = 1;
        m_par.mfx.GopPicSize = 30;
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.FrameInfo.PicStruct = 1;
        m_par.mfx.FrameInfo.ChromaFormat = 1;
        m_par.mfx.FrameInfo.Width = 352;
        m_par.mfx.FrameInfo.Height = 288;
        m_par.mfx.FrameInfo.CropW = 352;
        m_par.mfx.FrameInfo.CropH = 288;
        m_par.IOPattern = 2;

        MFXInit();
        Query();
        Init();
        AllocSurfaces();
        GetVideoParam();
        AllocBitstream();

        srand(rand() % 1000);

        const int payloadSize = 4;

        PayloadWrapper **payloads = new PayloadWrapper * [payloadSize];
        // 0
        payloads[0] = new PayloadWrapper(2);
        payloads[0]->addPayload(rand_payload());
        payloads[0]->addPayload(rand_payload());
        // 1
        payloads[1] = new PayloadWrapper(0);
        // 2
        payloads[2] = new PayloadWrapper(1);
        payloads[2]->addPayload(rand_payload());
        // 3
        payloads[3] = new PayloadWrapper(4);
        payloads[3]->addPayload(rand_payload());
        payloads[3]->addPayload(rand_payload());
        payloads[3]->addPayload(rand_payload());
        payloads[3]->addPayload(rand_payload());

        for (int i = 0, c = 0; i < payloadSize; c++)
        {
            tsRawReader reader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"), m_par.mfx.FrameInfo, payloads[i]->getPayloadsNumber()); // ?????
            g_tsStreamPool.Reg();
            if (c < payloadSize)
            {
                m_filler = &reader;
                m_pSurf = GetSurface();
                m_pSurf->Data.TimeStamp = c;
                m_pCtrl = make_ctrl(payloads[c]);
            }
            else
            {
                m_pCtrl = 0;
            }

            mfxStatus sts = EncodeFrameAsync();

            if (sts == MFX_ERR_NONE)
            {
                SyncOperation();
                BSErr bs_sts = BS_ERR_NONE;
                int n_pl = 0;
                mfxU64 frame_order = m_bitstream.TimeStamp;
                BS_MPEG2_parser parser;
                while (bs_sts != BS_ERR_MORE_DATA && frame_order < payloadSize)
                {
                    parser.set_buffer(m_bitstream.Data + m_bitstream.DataOffset, m_bitstream.DataLength);
                    bs_sts = parser.parse_next_unit();
                    if (bs_sts == BS_ERR_NONE || bs_sts == BS_ERR_NOT_IMPLEMENTED)
                    {
                        m_bitstream.DataOffset += (Bs32u)parser.get_offset();
                        m_bitstream.DataLength -= (Bs32u)parser.get_offset();
                    }
                    BS_MPEG2_header* pHdr = (BS_MPEG2_header*)parser.get_header();
                    if (pHdr->start_code == USER_DATA_HDR)
                    {
                        EXPECT_LE(n_pl+1, payloads[frame_order]->getPayloadsNumber()) << "ERROR: frame contain more payloads than expected.\nFrame: " << frame_order << "\nExpected number of payloads: " << payloads[frame_order]->getPayloadsNumber() << "\n";
                        PayloadRaw *expected = payloads[frame_order]->getPayload(n_pl);
                        size_t size = expected->getLength();
                        mfxU8 *payload = m_bitstream.Data + m_bitstream.DataOffset;
                        int memcmpRes = memcmp(payload, expected->getPayloadRaw(), expected->getLength());
                        EXPECT_EQ(memcmpRes, 0) << "ERROR: payload is different from expected.\nNumber of payload: " << n_pl << "\nFrame: " << frame_order << "\n";
                        n_pl++;
                    }
                }

                if (payloads[frame_order])
                {
                    EXPECT_EQ(payloads[frame_order]->getPayloadsNumber(), n_pl) << "ERROR: Number of payloads doesn't equal to expected.\n" << n_pl << " payloads found for frame " << frame_order << ", expected " << payloads[frame_order]->getPayloadsNumber() << "\n";
                    break;
                }

                if(m_bitstream.DataLength && m_bitstream.DataOffset)
                {
                    memmove(m_bitstream.Data, m_bitstream.Data + m_bitstream.DataOffset, m_bitstream.DataLength);
                }
                m_bitstream.DataOffset = 0;
                i++;
            }
            else if (sts == MFX_ERR_MORE_DATA)
            {
                if(reader.m_eos)
                    break;
            }
            else
            {
                EXPECT_EQ(MFX_ERR_NONE, sts) << "ERROR: Unexpected status from EncodeFrameAsync:" << sts << "\n";
                break;
            }
        }

        TS_END;
        return 0;
    }

    PayloadRaw *TestSuite::rand_payload()
    {
        const int sz = 1 + int(rand() % 127);
        mfxU8 * pl = new mfxU8[sz];
        for (int i(0); i < sz; ++i)
        {
            pl[i] = mfxU8(1+int(rand() % 0xFE));
        }
        PayloadRaw *pr = new PayloadRaw(pl, sz);
        return pr;
    }

    mfxEncodeCtrl* TestSuite::make_ctrl(PayloadWrapper *payload)
    {
        return attach_payloads(payload);
    }

    mfxEncodeCtrl *TestSuite::attach_payloads(PayloadWrapper *payload)
    {
        mfxEncodeCtrl *ctrl = new mfxEncodeCtrl();
        ctrl->NumPayload = payload->getPayloadsNumber();
        if (payload->getPayloadsNumber())
        {
            mfxPayload** pPayload = new mfxPayload * [payload->getPayloadsNumber()];
            for(unsigned int i = 0; i < static_cast<unsigned int>(payload->getPayloadsNumber()); ++i)
            {
                pPayload[i] = new_payload(payload->getPayload(i));
            }
            ctrl->Payload = pPayload;
        }
        return ctrl;
    }

    mfxPayload *TestSuite::new_payload(PayloadRaw *payload)
    {
        mfxPayload *pl = new mfxPayload;
        pl->Data = payload->getPayloadRaw();
        pl->NumBit = static_cast<mfxU32>(8*sizeof(mfxU8)*payload->getLength());
        pl->Type = USER_DATA_HDR;
        pl->BufSize = static_cast<mfxU16>(payload->getLength());
        return pl;
    }

    TS_REG_TEST_SUITE_CLASS(mpeg2e_user_data);
}
