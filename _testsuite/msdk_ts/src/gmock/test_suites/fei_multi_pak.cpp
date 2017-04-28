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
#include <vector>
#include "ts_encpak.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include "ts_fei_warning.h"
#include "avc2_struct.h"

namespace fei_multi_pak
{

    class MultiPAK : public tsVideoENCPAK
    {
    public:
        //variables
        int m_numPAK;
        //functions
        MultiPAK(mfxFeiFunction funcEnc, mfxFeiFunction funcPak, mfxU32 CodecId = 0, bool useDefaults = true) : tsVideoENCPAK(funcEnc, funcPak, CodecId, useDefaults) { m_numPAK = 1; };
        mfxStatus ProcessFrameAsync();
    };

    mfxStatus MultiPAK::ProcessFrameAsync()
    {
        m_ENCInput->ExtParam = enc.inbuf.data();
        m_ENCInput->NumExtParam = (mfxU16)enc.inbuf.size();
        m_ENCOutput->ExtParam = enc.outbuf.data();
        m_ENCOutput->NumExtParam = (mfxU16)enc.outbuf.size();

        mfxStatus mfxRes = tsVideoENCPAK::ProcessFrameAsync(m_session, m_ENCInput, m_ENCOutput, m_pSyncPoint);
        if (mfxRes != MFX_ERR_NONE)
            return mfxRes;

        mfxRes = tsVideoENCPAK::SyncOperation(*m_pSyncPoint, 0);
        if (mfxRes != MFX_ERR_NONE)
            return mfxRes;

        for (int countPAK = 0; countPAK < m_numPAK; countPAK++) {

            m_PAKInput->ExtParam = pak.inbuf.data();
            m_PAKInput->NumExtParam = (mfxU16)pak.inbuf.size();
            m_PAKOutput->ExtParam = pak.outbuf.data();
            m_PAKOutput->NumExtParam = (mfxU16)pak.outbuf.size();

            mfxRes = tsVideoENCPAK::ProcessFrameAsync(m_session, m_PAKInput, m_PAKOutput, m_pSyncPoint);
            if (mfxRes != MFX_ERR_NONE)
                return mfxRes;

            mfxRes = tsVideoENCPAK::SyncOperation(m_session, *m_pSyncPoint, MFX_INFINITE);
            if (mfxRes != MFX_ERR_NONE)
                return mfxRes;

            if (countPAK == (m_numPAK - 1)) {
                if (m_default && m_bs_processor && g_tsStatus.get() == MFX_ERR_NONE)
                {
                    g_tsStatus.check(m_bs_processor->ProcessBitstream(m_pBitstream ? *m_pBitstream : m_bitstream, 1));
                    TS_CHECK_MFX;
                }
            }

            m_pBitstream ? m_pBitstream->DataLength = 0 : m_bitstream.DataLength = 0;
        }

        return MFX_ERR_NONE;
    };

    class TestSuite : public tsSurfaceProcessor//, public tsBitstreamWriter
    {
    public:
        //variables
        tsVideoENCPAK encpak;
        MultiPAK multipak;
        static const unsigned int n_cases;

        //functions
        TestSuite() : //tsBitstreamWriter("bs.out")
            encpak(MFX_FEI_FUNCTION_ENC, MFX_FEI_FUNCTION_PAK, MFX_CODEC_AVC, true)
            , multipak(MFX_FEI_FUNCTION_ENC, MFX_FEI_FUNCTION_PAK, MFX_CODEC_AVC, true)
        {
            encpak.enc.m_par.mfx.FrameInfo.Width = encpak.enc.m_par.mfx.FrameInfo.CropW = 1920;
            encpak.enc.m_par.mfx.FrameInfo.Height = encpak.enc.m_par.mfx.FrameInfo.CropH = 1088;
            encpak.pak.m_par.mfx.FrameInfo.Width = encpak.pak.m_par.mfx.FrameInfo.CropW = 1920;
            encpak.pak.m_par.mfx.FrameInfo.Height = encpak.pak.m_par.mfx.FrameInfo.CropH = 1088;

            multipak.enc.m_par.mfx.FrameInfo.Width = multipak.enc.m_par.mfx.FrameInfo.CropW = 1920;
            multipak.enc.m_par.mfx.FrameInfo.Height = multipak.enc.m_par.mfx.FrameInfo.CropH = 1088;
            multipak.pak.m_par.mfx.FrameInfo.Width = multipak.pak.m_par.mfx.FrameInfo.CropW = 1920;
            multipak.pak.m_par.mfx.FrameInfo.Height = multipak.pak.m_par.mfx.FrameInfo.CropH = 1088;

            //initialization of the raw reader
            m_rawReaderFrameInfo = encpak.enc.m_par.mfx.FrameInfo;
            m_rawReaderFrameInfo.FourCC = MFX_FOURCC_YV12;

            encpak.m_filler = this;
            //encpak.m_bs_processor = this;

            multipak.m_filler = this;
            //multipak.m_bs_processor = this;
        }
        ~TestSuite() {
            if (m_reader != NULL)
                delete m_reader;
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s);
        int RunTest(unsigned int id);

        struct tc_struct
        {
            mfxStatus sts;
            mfxU16 picStruct;
            int numPAK;
        };

    private:
        //variables
        mfxFrameInfo m_rawReaderFrameInfo;
        tsRawReader *m_reader;
        static const tc_struct test_case[];

        //functions
        void SetEncodeParams(tsVideoENCPAK *obj, tc_struct tc);
    };

    void TestSuite::SetEncodeParams(tsVideoENCPAK *obj, tc_struct tc)
    {
        obj->enc.m_par.mfx.FrameInfo.PicStruct = tc.picStruct;

        obj->enc.m_pPar->mfx.GopPicSize = 30;
        obj->enc.m_pPar->mfx.GopRefDist = 1;
        obj->enc.m_pPar->mfx.NumRefFrame = 2;
        obj->enc.m_pPar->mfx.TargetUsage = 4;

        obj->enc.m_pPar->AsyncDepth = 1;
        obj->enc.m_pPar->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

        obj->pak.m_par.mfx = obj->enc.m_par.mfx;
        obj->pak.m_par.AsyncDepth = obj->enc.m_par.AsyncDepth;
        obj->pak.m_par.IOPattern = obj->enc.m_par.IOPattern;
    };

    mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& s)
    {
        mfxStatus sts = m_reader->ProcessSurface(s);
        return sts;
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        /*00*/ {MFX_ERR_NONE, MFX_PICSTRUCT_PROGRESSIVE, 2},
        ///*01*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_TFF, 2},
        ///*02*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_BFF, 2}
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

#define VERIFY_FIELD(var, expected, name) { \
    g_tsLog << name << " = " << var << " (expected = " << expected << ")\n"; \
    if ( var != expected) { \
        g_tsLog << "ERROR: " << name <<  " in stream != expected\n"; \
        err++; \
    } \
}

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;

        CHECK_FEI_SUPPORT();

        const tc_struct& tc = test_case[id];

        const mfxU16 n_frames = 10;
        mfxStatus sts;

        SetEncodeParams(&encpak, tc);
        SetEncodeParams(&multipak, tc);

        //set picture structure for raw reader
        m_rawReaderFrameInfo.PicStruct = encpak.enc.m_par.mfx.FrameInfo.PicStruct;

        std::vector <Ipp32u> refCRCVect;
        std::vector <Ipp32u> testCRCVect;

        //Set number of PAK
        multipak.m_numPAK = tc.numPAK;

        mfxU16 num_fields = encpak.enc.m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;

        encpak.PrepareInitBuffers();
        multipak.PrepareInitBuffers();

        g_tsStatus.expect(tc.sts);

        //ref
        sts = encpak.Init();

        if (sts == MFX_ERR_NONE)
            sts = encpak.AllocBitstream((encpak.enc.m_par.mfx.FrameInfo.Width * encpak.enc.m_par.mfx.FrameInfo.Height) * 16 * n_frames + 1024 * 1024);

        //test
        sts = multipak.Init();

        if (sts == MFX_ERR_NONE)
            sts = multipak.AllocBitstream((multipak.enc.m_par.mfx.FrameInfo.Width * multipak.enc.m_par.mfx.FrameInfo.Height) * 16 * n_frames + 1024 * 1024);

        mfxU32 count;
        mfxU32 async = TS_MAX(1, encpak.enc.m_par.AsyncDepth);
        async = TS_MIN(n_frames, async - 1);

        //ref
        m_reader = new tsRawReader(g_tsStreamPool.Get("YUV/matrix_1920x1088_250.yuv"), m_rawReaderFrameInfo);
        g_tsStreamPool.Reg();

        tsBitstreamCRC32 bs_crc;
        encpak.m_bs_processor = &bs_crc;

        for (count = 0; count < n_frames && sts == MFX_ERR_NONE; count++) {
            for (mfxU32 field = 0; field < num_fields && sts == MFX_ERR_NONE; field++) {
                encpak.PrepareFrameBuffers(field);

                sts = encpak.EncodeFrame(field);

                if (sts != MFX_ERR_NONE)
                    break;

                refCRCVect.push_back(bs_crc.GetCRC());
            }

        }

        encpak.Close();
        delete m_reader;
        m_reader = NULL;

        g_tsStatus.check(sts);

        //test
        m_reader = new tsRawReader(g_tsStreamPool.Get("YUV/matrix_1920x1088_250.yuv"), m_rawReaderFrameInfo);
        g_tsStreamPool.Reg();

        tsBitstreamCRC32 bs_crc_cmp;
        multipak.m_bs_processor = &bs_crc_cmp;

        for (count = 0; count < n_frames && sts == MFX_ERR_NONE; count++) {
            for (mfxU32 field = 0; field < num_fields && sts == MFX_ERR_NONE; field++) {
                multipak.PrepareFrameBuffers(field);

                sts = multipak.EncodeFrame(field);

                if (sts != MFX_ERR_NONE)
                    break;

                testCRCVect.push_back(bs_crc_cmp.GetCRC());
            }

        }

        multipak.Close();
        delete m_reader;
        m_reader = NULL;

        g_tsLog << count << " FRAMES Encoded\n";

        g_tsStatus.check(sts);

        if (refCRCVect.size() != testCRCVect.size())
        {
            g_tsLog << "ERROR: number of encoded frames should be the same\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }

        for (unsigned int countCRC = 0; countCRC < refCRCVect.size(); countCRC++) {
            g_tsLog << "crc = " << refCRCVect.at(countCRC) << "\n";
            g_tsLog << "crc_cmp = " << testCRCVect.at(countCRC) << "\n";
            if (refCRCVect.at(countCRC) != testCRCVect.at(countCRC))
            {
                g_tsLog << "ERROR: the 2 encoded bitstreams should be the same\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(fei_multi_pak);
};
