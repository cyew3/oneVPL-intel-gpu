/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#include <fstream>
#include <vector>

namespace avce_nalu_size
{

    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC)
        {
        }
        ~TestSuite() {}
        int RunTest(unsigned int id);

    private:
        struct tc_struct
        {
            mfxStatus sts;
            mfxI8 bufSize;
            bool attachFrameInfo;
            mfxI32 sliceSize;
            mfxU16 lowPower;
            mfxU16 picStruct;
            char* debugOut;
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/* 0*/ MFX_ERR_NONE, /*bufsize*/ 100,  false,   -1, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer enough, but codingoption3 is switched off
        {/* 1*/ MFX_ERR_NONE, /*bufsize*/ 100,  false,   -1, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer enough
        {/* 2*/ MFX_ERR_NONE, /*bufsize*/ 3,    false,   -1, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer not enough
        {/* 3*/ MFX_ERR_NONE, /*bufsize*/ 0,    false,   -1, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer is zero
        {/* 4*/ MFX_ERR_NONE, /*bufsize*/ -1,   false,   -1, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer is dynamic size
        {/* 5*/ MFX_ERR_NONE, /*bufsize*/ -2,   false,   -1, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer is not zero, but no memory allocated
        {/* 6*/ MFX_ERR_NONE, /*bufsize*/ 100,  true,    -1, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer enough and passing frameinfo extbuff
        {/* 7*/ MFX_ERR_NONE, /*bufsize*/ 100,  false, 1460, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer enough and multislice
        {/* 8*/ MFX_ERR_NONE, /*bufsize*/ -1,   false, 1460, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer enough and multislice
        {/* 9*/ MFX_ERR_NONE, /*bufsize*/ 100,  false,   -1,  MFX_CODINGOPTION_ON, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer enough
        {/*10*/ MFX_ERR_NONE, /*bufsize*/ 3,    false,   -1,  MFX_CODINGOPTION_ON, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer not enough
        {/*11*/ MFX_ERR_NONE, /*bufsize*/ 0,    false,   -1,  MFX_CODINGOPTION_ON, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer is zero
        {/*12*/ MFX_ERR_NONE, /*bufsize*/ -1,   false,   -1,  MFX_CODINGOPTION_ON, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer is dynamic size
        {/*13*/ MFX_ERR_NONE, /*bufsize*/ -2,   false,   -1,  MFX_CODINGOPTION_ON, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer is not zero, but no memory allocated
        {/*14*/ MFX_ERR_NONE, /*bufsize*/ 100,  true,    -1,  MFX_CODINGOPTION_ON, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer enough and passing frameinfo extbuff
        {/*15*/ MFX_ERR_NONE, /*bufsize*/ 100,  false, 1460,  MFX_CODINGOPTION_ON, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer enough and multislice
        {/*16*/ MFX_ERR_NONE, /*bufsize*/ -1,   false, 1460,  MFX_CODINGOPTION_ON, MFX_PICSTRUCT_PROGRESSIVE, NULL }, //if buffer enough and multislice
        {/*17*/ MFX_ERR_NONE, /*bufsize*/ 100,  false,   -1, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_FIELD_TFF  , NULL }, //if buffer enough
        {/*18*/ MFX_ERR_NONE, /*bufsize*/ 3,    false,   -1, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_FIELD_TFF  , NULL }, //if buffer not enough
        {/*19*/ MFX_ERR_NONE, /*bufsize*/ 0,    false,   -1, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_FIELD_TFF  , NULL }, //if buffer is zero
        {/*20*/ MFX_ERR_NONE, /*bufsize*/ -1,   false,   -1, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_FIELD_TFF  , NULL }, //if buffer is dynamic size
        {/*21*/ MFX_ERR_NONE, /*bufsize*/ -2,   false,   -1, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_FIELD_TFF  , NULL }, //if buffer is not zero, but no memory allocated
        {/*22*/ MFX_ERR_NONE, /*bufsize*/ 100,  true,    -1, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_FIELD_TFF  , NULL }, //if buffer enough and passing frameinfo extbuff
        {/*23*/ MFX_ERR_NONE, /*bufsize*/ 100,  false, 1460, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_FIELD_TFF  , NULL }, //if buffer enough and multislice
        {/*24*/ MFX_ERR_NONE, /*bufsize*/ -1,   false, 1460, MFX_CODINGOPTION_OFF, MFX_PICSTRUCT_FIELD_TFF  , NULL }, //if buffer enough and multislice
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];

        //file to process
        const char* file_name = NULL;
        if (tc.sliceSize <= 0)
        {
            file_name = g_tsStreamPool.Get("YUV/iceage_720x576_491.yuv");
            m_par.mfx.FrameInfo.Width = 720;
            m_par.mfx.FrameInfo.Height = 576;
            m_par.mfx.TargetKbps = 2000;
            m_par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
            m_par.mfx.GopPicSize = 5;
            m_par.mfx.GopRefDist = 2;
            m_par.mfx.IdrInterval = 1;
            m_par.mfx.MaxKbps = m_par.mfx.TargetKbps;
        }
        else
        {
            file_name = g_tsStreamPool.Get("YUV/matrix_1920x1088_250.yuv");
            m_par.mfx.FrameInfo.Width = 1920;
            m_par.mfx.FrameInfo.Height = 1088;
            m_par.mfx.NumSlice = 4;
        }
        //frame count to process
        const mfxU32 frameCount = 11;
        mfxU32 maxNALU = 0;
        mfxStatus sts = MFX_ERR_NONE;
        mfxU32 expectedSliceCount = 0;
        std::fstream fout;

        if (tc.debugOut)
            fout.open(tc.debugOut, std::fstream::binary | std::ios_base::out);

        m_par.mfx.CodecId                   = MFX_CODEC_AVC;
        m_par.mfx.FrameInfo.ChromaFormat    = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.CropW           = m_par.mfx.FrameInfo.Width;
        m_par.mfx.FrameInfo.CropH           = m_par.mfx.FrameInfo.Height;
        m_par.mfx.LowPower                  = tc.lowPower;

        MFXInit();

        if (tc.lowPower == MFX_CODINGOPTION_ON && g_tsHWtype < MFX_HW_SKL)
        {
            g_tsLog << "NOTICE >> LowPower mode doesn't supports\n";
            throw tsSKIP;
        }

        m_par.mfx.FrameInfo.PicStruct = tc.picStruct;

        if (tc.lowPower == MFX_CODINGOPTION_ON || tc.sliceSize > 0)
        {
            m_par.mfx.GopRefDist = 1;
            m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
            m_par.mfx.QPI = 26;
            m_par.mfx.QPB = 26;
            m_par.mfx.QPP = 26;
        }

        /* Counting expected slice count */
        expectedSliceCount = m_par.mfx.NumSlice ? m_par.mfx.NumSlice:1;
        switch (m_par.mfx.FrameInfo.PicStruct)
        {
        case MFX_PICSTRUCT_PROGRESSIVE:
            break;
        case MFX_PICSTRUCT_FIELD_TFF:
        case MFX_PICSTRUCT_FIELD_BFF:
            expectedSliceCount *= 2;
            break;
        }

        g_tsStreamPool.Reg();
        tsRawReader reader(file_name, m_par.mfx.FrameInfo);
        m_filler = &reader;

        /* Allocate and Initialize ext buffers */
        m_bitstream.AddExtBuffer<mfxExtEncodedUnitsInfo>(0);
        /* Test for FrameInfo correct */
        if (tc.attachFrameInfo) {
            m_bitstream.AddExtBuffer<mfxExtAVCEncodedFrameInfo>(0);
        }

        mfxExtEncodedUnitsInfo      *buf          = m_bitstream; /* GetExtBuffer */
        mfxExtAVCEncodedFrameInfo   *avcFrameInfo = m_bitstream; /* GetExtBuffer */

        /*
            Check for extbuffers were attached.
        */
        if (!buf) {
            g_tsLog << "ERROR >> No NALU Info buffer found!\n";
            throw tsFAIL;
        }

        if (tc.attachFrameInfo) {
            if (!avcFrameInfo) {
                g_tsLog << "ERROR >> No Frame Info buffer found!\n";
                throw tsFAIL;
            }
            else {
                memset((mfxU8*)avcFrameInfo + sizeof(mfxExtBuffer), 0, sizeof(mfxExtAVCEncodedFrameInfo)-sizeof(mfxExtBuffer));
                avcFrameInfo->QP = mfxU16(~0); //set all bits to 1
            }
        }

        /*
            Allocating memory for test case
        */
        if (tc.bufSize > 0) {
            buf->NumUnitsAlloc = tc.bufSize;
            buf->UnitInfo = new mfxEncodedUnitInfo[tc.bufSize];
            memset(buf->UnitInfo, 0, sizeof(mfxEncodedUnitInfo) * tc.bufSize);
        }

        /*
            Check extbuffer availability in Query
        */
        m_par.AddExtBuffer<mfxExtEncodedUnitsInfo>(0);
        Query();
        //It is runtime buffer, it should be removed before calling Init
        m_par.RemoveExtBuffer<mfxExtEncodedUnitsInfo>();

        /*
            Check handling of SliceSizeReport
        */
        m_par.AddExtBuffer<mfxExtCodingOption3>(0);
        if (id == 0)
        {
            ((mfxExtCodingOption3*)m_par)->EncodedUnitsInfo = 0xFF;
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            Query();
            ((mfxExtCodingOption3*)m_par)->EncodedUnitsInfo = MFX_CODINGOPTION_OFF;
            Query();
            ((mfxExtCodingOption3*)m_par)->EncodedUnitsInfo = MFX_CODINGOPTION_UNKNOWN;
            Query();
        }

        if(id > 0)
            ((mfxExtCodingOption3*)m_par)->EncodedUnitsInfo = MFX_CODINGOPTION_ON;

        Init();
        GetVideoParam();

        if (g_tsStatus.get() < 0)
            return 0;

        AllocSurfaces();
        AllocBitstream(m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 4);

        /* Encode process */
        for (mfxU32 i = 0, c = 0; i<frameCount; ++c) {
            m_pSurf = GetSurface();

            if (c < frameCount) {
                /* Support of dynamic NALU size */
                if (tc.bufSize < 0) {
                    buf->NumUnitsAlloc = maxNALU;
                    delete[] buf->UnitInfo;
                    if (tc.bufSize != -2) {
                        buf->UnitInfo = new mfxEncodedUnitInfo[maxNALU];
                        memset(buf->UnitInfo, 0, sizeof(mfxEncodedUnitInfo) * maxNALU);
                    }
                    else {
                        buf->UnitInfo = NULL;
                    }
                }
                if (tc.attachFrameInfo) {
                    avcFrameInfo->FrameOrder = mfxU32(~0); //set all bits to 1
                    avcFrameInfo->SecondFieldOffset = mfxU32(~0); //if we have interlaced content, frame order not used, only secondfieldoffset
                }
            }

            mfxStatus sts = EncodeFrameAsync();

            if (sts == MFX_ERR_NONE) {
                SyncOperation();
                if(tc.debugOut)
                    fout.write((char*)m_bitstream.Data, m_bitstream.DataLength);
                if (tc.attachFrameInfo && avcFrameInfo->FrameOrder == mfxU32(~0) && avcFrameInfo->SecondFieldOffset == mfxU32(~0))
                {
                    g_tsLog << "ERROR >> FrameInfo buffer data haven't changed\n";
                    throw tsFAIL;
                }
                /* Help data */
                g_tsLog << "Frame #" << i << ":got " << buf->NumUnitsEncoded << " overflow " << (buf->NumUnitsEncoded > buf->NumUnitsAlloc ? buf->NumUnitsEncoded - buf->NumUnitsAlloc : 0) << "\n";
                if (tc.attachFrameInfo) {
                    g_tsLog << "Received Frame order: " << avcFrameInfo->FrameOrder << "\n";
                }
                /* Check that NULL-pointer of UnitInfo was sucessfully handled */
                if (!buf->UnitInfo && tc.bufSize == -2)
                {
                    g_tsLog << "No unit info buffer as expected\n";
                    continue;
                }
                if (id == 0)
                {
                    if (buf->NumUnitsEncoded != 0)
                    {
                        g_tsLog << "ERROR >> No results are expected, because codingoption is switched off!\n";
                        throw tsFAIL;
                    }
                    else
                    {
                        g_tsLog << "No results as expected, because codingoption is switched off\n";
                        continue;
                    }
                }
                for (int j = 0; j < std::min(buf->NumUnitsEncoded, buf->NumUnitsAlloc); j++) {
                    g_tsLog << "NALU SIZE: " << (buf->UnitInfo + j)->Size << "\n";
                }
                /* Dynamic NALU size */
                if (maxNALU < ((mfxU32)buf->NumUnitsEncoded)) {
                    maxNALU = buf->NumUnitsEncoded;
                }
                ++i;
                /* Trying to check that NAL size is correctly reported and point to expected NAL units by Type */
                mfxU8 *p = m_bitstream.Data, *sbegin = m_bitstream.Data;
                mfxU32 sliceCount = 0, unitCount = 0;
                for(mfxU32 j = 0; j<std::min(buf->NumUnitsEncoded,buf->NumUnitsAlloc); ++j) {
                    g_tsLog << "NAL "
                            << mfxU64(p - m_bitstream.Data)
                            << " \t[" << mfxU32(p[0])
                            << "]\t[" << mfxU32(p[1])
                            << "]\t[" << mfxU32(p[2])
                            << "]\t[" << mfxU32(p[3])
                            << "] " << mfxU16(p[p[2] == 0x01 ? 3 : 4] & 0x1f)
                            << "==" << (buf->UnitInfo + j)->Type
                            << " ";
                    if (p[0] == 0 && p[1] == 0 && (p[2] == 1 || (p[2] == 0 && p[3] == 1)))
                    {
                        if( (p[p[2] == 1 ? 3 : 4] & 0x1f) == (buf->UnitInfo + j)->Type)
                            g_tsLog << "OK\n";
                        else
                        {
                            g_tsLog << "ERROR >> Unexpected NAL unit type!\n";
                            throw tsFAIL;
                        }
                        if ((buf->UnitInfo + j)->Type == 1 || (buf->UnitInfo + j)->Type == 5) //Count IDR and non-IDR slices
                            ++sliceCount;
                    }
                    else
                    {
                        g_tsLog << "ERROR >> NALU delimiter not found!\n";
                        throw tsFAIL;
                    }
                    p += (buf->UnitInfo+j)->Size;
                }
                /* Looking for nalu delimiters between start and counted p */
                for (sbegin = m_bitstream.Data; sbegin < p - 4; ++sbegin)
                {
                    if (sbegin[0] == 0 && sbegin[1] == 0 && sbegin[2] == 1)
                        ++unitCount;
                }
                /* Check for encoding units */
                if (unitCount != std::min(buf->NumUnitsEncoded, buf->NumUnitsAlloc))
                {
                    g_tsLog << "ERROR >> Unit count is invalid, " << unitCount << " found, " << std::min(buf->NumUnitsEncoded, buf->NumUnitsAlloc) << " expected\n";
                    throw tsFAIL;
                }
                /* Check for multislice */
                if (tc.sliceSize > 0 && buf->NumUnitsEncoded<=buf->NumUnitsAlloc && sliceCount != expectedSliceCount)
                {
                    g_tsLog << "ERROR >> Slice count is invalid, " << sliceCount << " found, " << expectedSliceCount << " expected\n";
                    throw tsFAIL;
                }
                /* if we have all NALU lengths in the report */
                if (buf->NumUnitsEncoded <= buf->NumUnitsAlloc)
                {
                    if ((p - m_bitstream.Data) == m_bitstream.DataLength)
                    {
                        g_tsLog << "Bitstream end reached\n";
                    }
                    else
                    {
                        g_tsLog << "ERROR >> Bitstream end not reached! " << m_bitstream.DataLength << "!=" << ptrdiff_t(p - m_bitstream.Data) << '\n';
                        throw tsFAIL;
                    }
                }
                else
                {
                    g_tsLog << "End of the bitstream cannot be reached, expected\n";
                }
                m_bitstream.DataLength = 0;
            }
            else if (sts == MFX_ERR_MORE_DATA) {
                if (reader.m_eos)
                    break;
            }
            else {
                break;
            }
        }

        if(tc.debugOut)
            fout.close();

        /* Free ext buffers */
        delete[] buf->UnitInfo;

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(avce_nalu_size);
}
