/******************************************************************************* *\

Copyright (C) 2017-2018 Intel Corporation.  All rights reserved.

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
First 26 positive test cases are intended to test those 13 Levels.
For test cases #0-12, Level ID = test_case ID and Width is computed as
maximum available value for the given Level.
For test cases #13-25, Level ID = (test_case ID - NumberOfLevels) and
Height is computed as maximum available value for the given Level.

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

#include <iostream>
#include <fstream>

namespace hevce_fei_level_profile {

// In case you need to verify the output manually, define the MANUAL_DEBUG_MODE macro. The output files are:
// hevce_fei_level_profile.h265 stores encoded bitstream,
// hevce_fei_level_profile1.log stores video parameters which are set in TestSuite::SetParsPositiveTC() for the first 13 test cases,
// hevce_fei_level_profile2.log stores video parameters which are set in TestSuite::SetParsPositiveTC() for the next 13 test cases.
//#define MANUAL_DEBUG_MODE

// A structure to store constraints values for one Level
struct LevelConstraints
{
    mfxU16 Level;         // numerical representation of Level values: 10, 20, 21, 30, 31, 40, 41, 50, 51, 52, 60, 61, 62
    mfxU32 MaxLumaPs;     // max luma picture size (samples)
    mfxU16 MaxResolution; // max Height or Width for the given Level which is computed as sqrt(MaxLumaPS*8) according to the HEVC Standard
    mfxU32 MaxSSPP;       // max number of slice segments per picture which means:
                          // max number of slices allowed per picture at both the max resolution and max Frame Rate
    mfxU32 MaxLumaSr;     // max luma sample rate (samples per second)
};

// The following values are given according to the Table A.6 from the HEVC International Standard
constexpr LevelConstraints TableA6[13] =
{
    // LevelID |  Level   | MaxLumaPs | MaxWidth/ | Max Slice  |  MaxLumaSr
    //                                | MaxHeight |  Segments  |
    //                                |           | PerPicture |
    /*  0  */     { 10,      36864,       543,         16,         552960      }, // only MAIN TIER is available
    /*  1  */     { 20,      122880,      991,         16,         3686400     }, // for all Levels < 4
    /*  2  */     { 21,      245760,      1402,        20,         7372800     },
    /*  3  */     { 30,      552960,      2103,        30,         16588800    },
    /*  4  */     { 31,      983040,      2804,        40,         33177600    },
    /*  5  */     { 40,      2228224,     4222,        75,         66846720    }, // MAIN and HIGH TIERs are available
    /*  6  */     { 41,      2228224,     4222,        75,         133693440   }, // for all Levels >= 4
    /*  7  */     { 50,      8912896,     8444,        200,        267386880   },
    /*  8  */     { 51,      8912896,     8444,        200,        534773760   },
    /*  9  */     { 52,      8912896,     8444,        200,        1069547520  },
    /*  10 */     { 60,      35651584,    16888,       600,        1069547520  },
    /*  11 */     { 61,      35651584,    16888,       600,        2139095040  },
    /*  12 */     { 62,      35651584,    16888,       600,        4278190080  },
};

constexpr mfxU16 NumberOfLevels = sizeof(TableA6)/sizeof(LevelConstraints);

class TestSuite : tsVideoEncoder, tsParserHEVC, tsBitstreamProcessor
{
public:

    // There're 26 test cases (see more details in the test description in the beginning)
    static constexpr mfxU16 n_cases = NumberOfLevels*2;

#ifdef MANUAL_DEBUG_MODE
    tsBitstreamWriter m_tsBsWriter;
#endif

    TestSuite()
    : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
    , tsParserHEVC()
    , tsBitstreamProcessor()
#ifdef MANUAL_DEBUG_MODE
    , m_tsBsWriter("hevce_fei_level_profile.h265")
#endif
    {
        m_bs_processor = this;
        m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        m_par.AsyncDepth = 1;

        // Set all parameters necessary for working with HEVC FEI encoder:
        mfxExtFeiHevcEncFrameCtrl& feiCtrl =  m_ctrl;
        feiCtrl.SubPelMode         = 3; // quarter-pixel motion estimation
        feiCtrl.SearchWindow       = 5; // 48 SUs 48x40 window full search
        feiCtrl.NumFramePartitions = 4; // number of partitions in frame that encoder processes concurrently
    }

    mfxI32 RunTest(mfxU16 id);

private:

    void SetParsPositiveTC(const LevelConstraints& LevelParam, bool maxWidthFlag)
    {
        constexpr mfxU16 alignment    = 16;
        constexpr mfxU16 MaxFrameRate = 300;  // the maximum frame rate supported by HEVC is 300 frames per second
        constexpr mfxU16 MaxWidth     = 4096; // width limitation in HEVC hardware encoder
        constexpr mfxU16 MaxHeight    = 2176; // height limitation in HEVC hardware encoder
        constexpr mfxU16 MaxNumSlice  = 200;  // due to driver issue, NumSlice is limited to 200

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
        if (maxWidthFlag)
        {
            m_par.mfx.FrameInfo.Width  = std::min((mfxU16)(LevelParam.MaxResolution & ~(alignment-1)), MaxWidth);
            m_par.mfx.FrameInfo.Height = std::min((mfxU16)((LevelParam.MaxLumaPs / m_par.mfx.FrameInfo.Width) & ~(alignment-1)), MaxHeight );
        }
        else
        {
            m_par.mfx.FrameInfo.Height = std::min((mfxU16)(LevelParam.MaxResolution & ~(alignment-1)), MaxHeight);
            m_par.mfx.FrameInfo.Width  = std::min((mfxU16)((LevelParam.MaxLumaPs / m_par.mfx.FrameInfo.Height) & ~(alignment-1)), MaxWidth );
        }
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width; // as the Frame Width and Height are aligned, we can use them as the Cropped Width and Height
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;

        m_par.mfx.FrameInfo.FrameRateExtN = std::min((mfxU16)(LevelParam.MaxLumaSr / (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height)), MaxFrameRate);
        m_par.mfx.FrameInfo.FrameRateExtD = 1;

        m_par.mfx.CodecLevel   = LevelParam.Level;      // for Main tier, CodecLevel values are: 10, 20, 21, 30, 31, 40, 41, 50, 51, 52, 60, 61, 62
        m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN; // only the case of Main tier is tested in this version of test suite

        // Due to driver issue, NumSlice is limited to MaxNumSlice (see definition above).
        // Otherwise MFXVideoENCODE_Init will fail with MFX_WRN_INCOMPATIBLE_VIDEO_PARAM status.
        m_par.mfx.NumSlice = std::min((mfxU16)LevelParam.MaxSSPP, MaxNumSlice);

    }

    void PrintDebugInfo(const LevelConstraints& LevelParam, bool maxWidthFlag)
    {
        #ifdef MANUAL_DEBUG_MODE
            std::ofstream stream;
            if (maxWidthFlag)
                stream.open("hevce_fei_level_profile1.log", std::ios::app);
            else
                stream.open("hevce_fei_level_profile2.log", std::ios::app);
        #else
            std::ostream &stream = g_tsLog;
        #endif

        stream << "================================== " << std::fixed << std::setprecision(1) << (mfxF32)(m_par.mfx.CodecLevel/10.0f) << "\n";
        stream << "======== MaxLumaPs:     " << LevelParam.MaxLumaPs << "\n";
        stream << "======== MaxLumaSr:     " << LevelParam.MaxLumaSr << "\n";
        stream << "======== Width:         " << m_par.mfx.FrameInfo.Width << "\n";
        stream << "======== Height:        " << m_par.mfx.FrameInfo.Height << "\n";
        stream << "======== CropW:         " << m_par.mfx.FrameInfo.CropW << "\n";
        stream << "======== CropH:         " << m_par.mfx.FrameInfo.CropH << "\n";
        stream << "======== FrameRateExtN: " << m_par.mfx.FrameInfo.FrameRateExtN << "\n";
        stream << "======== FrameRateExtD: " << m_par.mfx.FrameInfo.FrameRateExtD << "\n";
        stream << "======== NumSlice:      " << m_par.mfx.NumSlice << "\n";
        stream << "======== CodecLevel:    " << m_par.mfx.CodecLevel << "\n";
        stream << "======== CodecProfile:  " << m_par.mfx.CodecProfile << "\n";

        #ifdef MANUAL_DEBUG_MODE
            stream.close();
        #endif
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
                    // Values of the NAL unit Level stored in header are defined as ( (m_par.mfx.CodecLevel & 0xFF) * 3 ):
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

mfxI32 TestSuite::RunTest(mfxU16 id)
{
    TS_START;
    CHECK_HEVC_FEI_SUPPORT();

    MFXInit();

    mfxU16 table_id = id % NumberOfLevels;
    bool maxWidthFlag = id < NumberOfLevels;
    // Set FrameInfo values according to specified Level
    SetParsPositiveTC(TableA6[table_id], maxWidthFlag);
    PrintDebugInfo(TableA6[table_id], maxWidthFlag);
    // Load HEVC FEI plugin
    Load();
    Init();
    EncodeFrames(5);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_fei_level_profile);
} // end of namespace hevce_fei_level_profile
