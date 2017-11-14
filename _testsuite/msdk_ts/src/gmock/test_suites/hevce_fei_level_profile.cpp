/******************************************************************************* *\

Copyright (C) 2017 Intel Corporation.  All rights reserved.

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

/*
Description:
This test suite checks what Levels are supported by HEVC FEI Encoder.

In HEVC Standard there are 13 Levels:  1, 2, 2.1, 3, 3.1, 4, 4.1, 5, 5.1, 5.2, 6, 6.1, 6.2
First 13 positive test cases are intended to test those 13 Levels (Level ID = test_case ID).
For each test case, input values are: Level itself and resolution, frame rate and NumSlice
specific for the given Level.

Algorithm:
- MFXInit
- set FrameInfo according to specified Level
- load Hevc FEI plugin
- Init
- EncodeFrames

*/

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_fei_warning.h"
#include "ts_decoder.h"

namespace hevce_fei_level_profile {

// In case you need to verify the output manually, define the MANUAL_DEBUG_MODE macro.
// In this case the encoded bitstream will be written to the test_out.h265 file.
//#define MANUAL_DEBUG_MODE

// struct to store constraints values for one Level
struct LevelConstraints
{
    mfxU16 Level;     // to ease math operations, let's work with integer Level values:
                      // 10, 20, 21, 30, 31, 40, 41, 50, 51, 52, 60, 61, 62
    mfxU32 MaxLumaPs; // max luma picture size (samples)
    mfxU32 MaxSSPP;   // max number of slice segments per picture which means:
                      // max number of slices alowed per picture at both the max resolution and max Frame Rate
    mfxU32 MaxLumaSr; // max luma sample rate (samples per second)
};

// The following values are given according to the Table A.6 from the HEVC International Standard
const LevelConstraints TableA6[13] =
{
    // LevelID |  Level   | MaxLumaPs | Max Slice  |  MaxLumaSr
    //                                |  Segments  |
    //                                | PerPicture |
    /*  0  */     { 10,      36864,         16,         552960      }, // only MAIN TIER is available
    /*  1  */     { 20,      122880,        16,         3686400     }, // for all Levels < 4
    /*  2  */     { 21,      245760,        20,         7372800     },
    /*  3  */     { 30,      552960,        30,         16588800    },
    /*  4  */     { 31,      983040,        40,         33177600    },
    /*  5  */     { 40,      2228224,       75,         66846720    }, // MAIN and HIGH TIERs are available
    /*  6  */     { 41,      2228224,       75,         133693440   }, // for all Levels >= 4
    /*  7  */     { 50,      8912896,       200,        267386880   },
    /*  8  */     { 51,      8912896,       200,        534773760   },
    /*  9  */     { 52,      8912896,       200,        1069547520  },
    /*  10 */     { 60,      35651584,      600,        1069547520  },
    /*  11 */     { 61,      35651584,      600,        2139095040  },
    /*  12 */     { 62,      35651584,      600,        4278190080  },
};

inline mfxU16 CeilDiv(mfxU16 x, mfxU16 y) { return (x + y - 1) / y; }

class TestSuite : tsVideoEncoder, tsParserHEVC, tsBitstreamProcessor
{
public:
    static const mfxU32 n_cases;
#ifdef MANUAL_DEBUG_MODE
    tsBitstreamWriter m_tsBsWriter;
#endif

    TestSuite()
    : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
    , tsParserHEVC()
    , tsBitstreamProcessor()
#ifdef MANUAL_DEBUG_MODE
    , m_tsBsWriter("test_out.h265")
#endif
    {
        m_bs_processor = this;
        m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        m_par.AsyncDepth = 1;

        // Set all parameters necessary for working with HEVC FEI encoder:
        mfxExtFeiHevcEncFrameCtrl& feiCtrl =  m_ctrl;
        feiCtrl.SubPelMode         = 3;
        feiCtrl.SearchWindow       = 5;
        feiCtrl.NumFramePartitions = 4;
    }

    ~TestSuite() { delete m_filler; }

    mfxI32 RunTest(mfxU16 id);

private:

    void SetParsPositiveTC(const LevelConstraints& LevelParam) // LevelId = test_case ID
    {
        mfxU16 alignment = 16; // 32
        mfxU16 maxFrameRateHEVC = 300; // The maximum frame rate supported by HEVC is 300 frames per second

        // On the basis of formulas given in the HEVC Standard, Frame Width is defined as
        // the aligned value of maximum allowed width for specified Max Luma pic size:
        // ..., bitstreams conforming to a profile at a specified tier and level shall obey the
        // following constraints ... :
        // ...
        // b) The value of pic_width_in_luma_samples shall be less than or equal to Sqrt( MaxLumaPs * 8 ).
        // c) The value of pic_height_in_luma_samples shall be less than or equal to Sqrt( MaxLumaPs * 8 ).
        // ...
        // where MaxLumaPs is specified in Table A.6.
        // Due to the restriction of HEVC FEI Encoder implementation, the maximum supported resolution is 4096x2176.
        // If a greater resolution is specified for HEVC FEI Encoder, the Query function terminates with MFX_ERR_UNSUPPORTED status.
        m_par.mfx.FrameInfo.Width  = TS_MIN(( (mfxU16)(sqrt((mfxF32)LevelParam.MaxLumaPs * 8)) & ~(alignment-1)), 4096);
        m_par.mfx.FrameInfo.Height = TS_MIN(((LevelParam.MaxLumaPs / m_par.mfx.FrameInfo.Width) & ~(alignment-1)), 2176 );

        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width; // as the Frame Width and Height are aligned, we can use them as the Cropped Width and Height
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;

        m_par.mfx.FrameInfo.FrameRateExtN = TS_MIN( (LevelParam.MaxLumaSr / (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height)), maxFrameRateHEVC);
        m_par.mfx.FrameInfo.FrameRateExtD = 1;

        m_par.mfx.CodecLevel = LevelParam.Level; // for Main tier, CodecLevel values are: 10, 20, 21, 30, 31, 40, 41, 50, 51, 52, 60, 61, 62
        m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;   // Only the case of Main tier is tested in this version of test suite.

        m_par.mfx.NumSlice = TS_MIN( TS_MIN( LevelParam.MaxSSPP, 68 ), CeilDiv(m_par.mfx.FrameInfo.Height, 64) );

        g_tsLog << "======== MaxLumaPs:     " << LevelParam.MaxLumaPs << "\n";
        g_tsLog << "======== MaxLumaSr:     " << LevelParam.MaxLumaSr << "\n";
        g_tsLog << "======== Width:         " << m_par.mfx.FrameInfo.Width << "\n";
        g_tsLog << "======== Height:        " << m_par.mfx.FrameInfo.Height << "\n";
        g_tsLog << "======== CropW:         " << m_par.mfx.FrameInfo.CropW << "\n";
        g_tsLog << "======== CropH:         " << m_par.mfx.FrameInfo.CropH << "\n";
        g_tsLog << "======== FrameRateExtN: " << m_par.mfx.FrameInfo.FrameRateExtN << "\n";
        g_tsLog << "======== FrameRateExtD: " << m_par.mfx.FrameInfo.FrameRateExtD << "\n";
        g_tsLog << "======== NumSlice:      " << m_par.mfx.NumSlice << "\n";
        g_tsLog << "======== CodecLevel:    " << m_par.mfx.CodecLevel << "\n";
        g_tsLog << "======== CodecProfile:  " << m_par.mfx.CodecProfile << "\n";
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        using namespace BS_HEVC;
        if (bs.Data)
        {
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

#ifdef MANUAL_DEBUG_MODE
            m_tsBsWriter.ProcessBitstream(bs, nFrames);
#endif

            UnitType& au = ParseOrDie();
            for (mfxU32 i = 0; i < au.NumUnits; i ++)
            {
                if (au.nalu[i] && au.nalu[i]->nal_unit_type == SPS_NUT)
                {
                    g_tsLog << "======== Input Level: " << (m_par.mfx.CodecLevel & 0xFF) << "\n";
                    // Values of the NAL unit Level stored in header, are defined as ( (m_par.mfx.CodecLevel & 0xFF) * 3 ):
                    // 30, 60, 63, 90, 93, 120, 123, 150, 153, 156, 180, 183, 186
                    g_tsLog << "======== Output Level:  " << (au.nalu[i]->sps->ptl.general.level_idc/3) << "\n";
                    EXPECT_EQ(au.nalu[i]->sps->ptl.general.level_idc/3 , (m_par.mfx.CodecLevel & 0xFF ) ) << "ERROR: Level value is not as expected\n";
                }
            }
        }
        bs.DataLength = 0;
        return MFX_ERR_NONE;
    }

}; // end of class TestSuite

const mfxU32 TestSuite::n_cases = sizeof(TableA6)/sizeof(LevelConstraints);

mfxI32 TestSuite::RunTest(mfxU16 id)
{
    TS_START;
    CHECK_HEVC_FEI_SUPPORT();

    MFXInit();
    // Set FrameInfo values according to specified Level
    SetParsPositiveTC(TableA6[id]);
    // Load HEVC FEI plugin
    Load();
    Init();
    EncodeFrames(5);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_fei_level_profile);
} // end of namespace hevce_fei_level_profile
