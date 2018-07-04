/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2018 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "ts_encoder.h"


namespace avce_mfe_fei
{

typedef struct
{
    mfxStatus      sts;

    mfxU16         width;
    mfxU16         height;
    mfxU16         cropW;
    mfxU16         cropH;

    mfxU16         numSlice;
    mfxFeiFunction feiFunc;

    mfxU16         inMFMode;
    mfxU16         inMaxNumFrames;
    mfxU16         expMFMode;
    mfxU16         expMaxNumFrames;
} tc_struct;

static tc_struct test_case[] =
{
        // Test cases when inMFMode = MFX_MF_AUTO
        // Negative status MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is expected
        // when input MaxNumFrames exceeds the maximum supported limit
        {/*00*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_AUTO, /*inMaxNumFrames*/0,
                MFX_MF_AUTO, /*expMaxNumFrames*/0},

        {/*01*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_AUTO, /*inMaxNumFrames*/1,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},
        {/*02*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_AUTO, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_AUTO, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        {/*04*/ MFX_ERR_NONE,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_AUTO, /*inMaxNumFrames*/1,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},
        {/*05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_AUTO, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},

        {/*06*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_AUTO, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*07*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_AUTO, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        {/*08*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/2, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_AUTO, /*inMaxNumFrames*/1,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},
        {/*09*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/2, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_AUTO, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},

        // Test cases when inMFMode = MFX_MF_DEFAULT
        // Mode MFX_MF_DISABLED is not supported when MaxNumFrames > 1
        // Negative status MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is expected
        // in these cases
        {/*10*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/0,
                MFX_MF_DEFAULT, /*expMaxNumFrames*/0},
        {/*11*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/1,
                MFX_MF_DEFAULT, /*expMaxNumFrames*/1},
        {/*12*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*13*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        {/*14*/ MFX_ERR_NONE,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/1,
                MFX_MF_DEFAULT, /*expMaxNumFrames*/1},
        {/*15*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/2,
                MFX_MF_DEFAULT, /*expMaxNumFrames*/1},

        {/*16*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*17*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        {/*18*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/2, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/1,
                MFX_MF_DEFAULT, /*expMaxNumFrames*/1},
        {/*19*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/2, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/2,
                MFX_MF_DEFAULT, /*expMaxNumFrames*/1},

        // Test cases when inMFMode = MFX_MF_DISABLED
        // Mode MFX_MF_DISABLED is not supported when MaxNumFrames != 1.
        // Status MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is expected in these cases
        {/*20*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DISABLED, /*inMaxNumFrames*/0,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},
        {/*21*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DISABLED, /*inMaxNumFrames*/1,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},
        {/*22*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DISABLED, /*inMaxNumFrames*/2,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},

        {/*23*/ MFX_ERR_NONE,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DISABLED, /*inMaxNumFrames*/1,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},
        {/*24*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DISABLED, /*inMaxNumFrames*/2,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},

        {/*25*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DISABLED, /*inMaxNumFrames*/1,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},
        {/*26*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DISABLED, /*inMaxNumFrames*/2,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},

        {/*27*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/2, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DISABLED, /*inMaxNumFrames*/1,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},
        {/*28*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/2, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_DISABLED, /*inMaxNumFrames*/2,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},

        // Test cases when inMFMode = MFX_MF_MANUAL
        // Negative status MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is expected
        // when input MaxNumFrames exceeds the maximum supported limit
        {/*29*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL, /*inMaxNumFrames*/0,
                MFX_MF_MANUAL, /*expMaxNumFrames*/0},
        {/*30*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL, /*inMaxNumFrames*/1,
                MFX_MF_MANUAL, /*expMaxNumFrames*/1},
        {/*31*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL, /*inMaxNumFrames*/2,
                MFX_MF_MANUAL, /*expMaxNumFrames*/2},
        {/*32*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL, /*inMaxNumFrames*/3,
                MFX_MF_MANUAL, /*expMaxNumFrames*/2},

        {/*33*/ MFX_ERR_NONE,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL, /*inMaxNumFrames*/1,
                MFX_MF_MANUAL, /*expMaxNumFrames*/1},
        {/*34*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/3840, /*height*/2160, /*cropW*/3840, /*cropH*/2160,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL, /*inMaxNumFrames*/2,
                MFX_MF_MANUAL, /*expMaxNumFrames*/1},

        {/*35*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL, /*inMaxNumFrames*/2,
                MFX_MF_MANUAL, /*expMaxNumFrames*/2},
        {/*36*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/1, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL, /*inMaxNumFrames*/3,
                MFX_MF_MANUAL, /*expMaxNumFrames*/2},

        {/*37*/ MFX_ERR_NONE,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropH*/480,
                /*numSlice*/2, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL, /*inMaxNumFrames*/1,
                MFX_MF_MANUAL, /*expMaxNumFrames*/1},
        {/*38*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/2, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL, /*inMaxNumFrames*/2,
                MFX_MF_MANUAL, /*expMaxNumFrames*/1},

        // Test cases when inMFMode > MFX_MF_MANUAL
        // Mode values that are more than MFX_MF_MANUAL are not supported.
        // Status MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is expected in these cases
        {/*39*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL + 1, /*inMaxNumFrames*/0,
                MFX_MF_DEFAULT, /*expMaxNumFrames*/0},
        {/*40*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL + 1, /*inMaxNumFrames*/1,
                MFX_MF_DEFAULT, /*expMaxNumFrames*/1},
        {/*41*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL + 1, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},
        {/*42*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENCODE,
                MFX_MF_MANUAL + 1, /*inMaxNumFrames*/3,
                MFX_MF_AUTO, /*expMaxNumFrames*/2},

        // Negative test cases when FEI Function != ENCODE
        {/*43*/ MFX_ERR_UNSUPPORTED,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_PREENC,
                MFX_MF_AUTO, /*inMaxNumFrames*/2,
                MFX_MF_AUTO, /*expMaxNumFrames*/1},
        {/*44*/ MFX_ERR_UNSUPPORTED,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENC,
                MFX_MF_DEFAULT, /*inMaxNumFrames*/2,
                MFX_MF_DEFAULT, /*expMaxNumFrames*/1},
        {/*45*/ MFX_ERR_UNSUPPORTED,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_PAK,
                MFX_MF_DISABLED, /*inMaxNumFrames*/2,
                MFX_MF_DISABLED, /*expMaxNumFrames*/1},
        {/*46*/ MFX_ERR_UNSUPPORTED,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_DEC,
                MFX_MF_MANUAL, /*inMaxNumFrames*/2,
                MFX_MF_MANUAL, /*expMaxNumFrames*/1},
        {/*47*/ MFX_ERR_UNSUPPORTED,
                /*width*/720, /*height*/480, /*cropW*/720, /*cropW*/480,
                /*numSlice*/0, MFX_FEI_FUNCTION_ENC,
                MFX_MF_MANUAL + 1, /*inMaxNumFrames*/2,
                MFX_MF_DEFAULT, /*expMaxNumFrames*/1},
};

static void set_params(tsVideoEncoder& enc, tc_struct& tc,
        bool fei_enabled = true, bool mfe_enabled = true)
{
    enc.m_par.mfx.FrameInfo.Width = MSDK_ALIGN16(tc.width);
    enc.m_par.mfx.FrameInfo.Height = MSDK_ALIGN16(tc.height);
    enc.m_par.mfx.FrameInfo.CropW = MSDK_ALIGN16(tc.cropW);
    enc.m_par.mfx.FrameInfo.CropH = MSDK_ALIGN16(tc.cropH);

    if (tc.width > 1920 && tc.height > 1088) {
        // Set progressive mode just to avoid MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
        // warning (current resolution corresponds to level > 4.1 but it is not
        // valid for interlaced mode)
        enc.m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    enc.m_par.mfx.NumSlice = tc.numSlice;

    if (fei_enabled) {
        mfxExtFeiParam& fei = enc.m_par;
        fei.Func = tc.feiFunc;
    }

    if (mfe_enabled) {
        mfxExtMultiFrameParam& mfe = enc.m_par;
        mfe.MFMode = tc.inMFMode;
        mfe.MaxNumFrames = tc.inMaxNumFrames;
    }

    // Update expected values for platforms != MFX_HW_SKL
    if (g_tsHWtype != MFX_HW_SKL) {
        if (tc.inMaxNumFrames) {
            tc.expMaxNumFrames = 1;

            if (tc.inMaxNumFrames != tc.expMaxNumFrames &&
                    tc.feiFunc == MFX_FEI_FUNCTION_ENCODE)
            {
                tc.sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }

            if (tc.inMFMode == MFX_MF_DEFAULT || tc.inMFMode > MFX_MF_MANUAL) {
                tc.expMFMode = MFX_MF_DEFAULT;
            }
        }
    }
}

}

