/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <fstream>

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace avce_skipframes_tektronix
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct tc_par;

    enum
    {
        MFXPAR = 1,
        EXT_COD2,
        EXT_COD,
    };

    static const char * EnumStrings[];
    const char * getTextForEnum( int enumVal ) const
    {
        return EnumStrings[enumVal];
    }

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
        std::string skips;
    };

    void PrintTcStruct(const tc_struct& tc) const
    {
        g_tsLog << "Test case parameters:\n";
        g_tsLog << "  sts  = " << tc.sts << "\n";
        g_tsLog << "  mode = " << getTextForEnum(tc.mode) << "\n";
        for(size_t i(0); i < MAX_NPARS; ++i)
        {
            if(tc.set_par[i].f)
            {
                g_tsLog << "  set_par["<<i <<"].ext_type = " << getTextForEnum(tc.set_par[i].ext_type) << "\n";
                g_tsLog << "          "   <<"  .f        = " << tc.set_par[i].f->name << "\n";
                g_tsLog << "          "   <<"  .v        = " << tc.set_par[i].v << "\n";
            }
        }
        g_tsLog << "  skips = " << tc.skips << "\n\n";
    }

    static const tc_struct test_case[];
};

const char * TestSuite::EnumStrings[] = { "0", "MFXPAR", "EXT_COD2", "EXT_COD" };

const TestSuite::tc_struct TestSuite::test_case[] =
{
    // IPPP
    {/*00*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*01*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*02*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*03*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*04*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*05*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*06*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*07*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*08*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*09*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*10*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*11*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*12*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each2"},
    {/*13*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each6"},
    {/*14*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each2"},
    {/*15*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each2"},
    {/*16*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each6"},
    {/*17*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each2"},
    {/*18*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each2"},
    {/*19*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each6"},
    {/*20*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each2"},
    {/*21*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each2"},
    {/*22*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each6"},
    {/*23*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each2"},
    {/*24*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "last10"},
    {/*25*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "last10"},
    {/*26*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "last10"},
    {/*27*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "last10"},
    {/*28*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "last10"},
    {/*29*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "last10"},
    {/*30*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "last10"},
    {/*31*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "last10"},
    {/*32*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "last10"},
    {/*33*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "last10"},
    {/*34*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "last10"},
    {/*35*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "last10"},

    // B-frames
    {/*36*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 3 5 7 9 11 13"},
    {/*37*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 3 5 7 9 11 13"},
    {/*38*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 3 5 7 9 11 13"},
    {/*39*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 3 5 7 9 11 13"},
    {/*40*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 3 5 7 9 11 13"},
    {/*41*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 3 5 7 9 11 13"},
    {/*42*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 3 5 7 9 11 13"},
    {/*43*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 3 5 7 9 11 13"},
    {/*44*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 3 5 7 9 11 13"},
    {/*45*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 3 5 7 9 11 13"},
    {/*46*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 3 5 7 9 11 13"},
    {/*47*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 3 5 7 9 11 13"},

    // MFX_SKIPFRAME_INSERT_NOTHING
    {/*48*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 2 3 4 5 6 7 8 9"},
    {/*49*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 2 3 4 5 6 7 8 9"},
    {/*50*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 2 3 4 5 6 7 8 9"},
    {/*51*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 2 3 4 5 6 7 8 9"},
    {/*52*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 2 3 4 5 6 7 8 9"},
    {/*53*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 2 3 4 5 6 7 8 9"},
    {/*54*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 2 3 4 5 6 7 8 9"},
    {/*55*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 2 3 4 5 6 7 8 9"},
    {/*56*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 2 3 4 5 6 7 8 9"},
    {/*57*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 2 3 4 5 6 7 8 9"},
    {/*58*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 2 3 4 5 6 7 8 9"},
    {/*59*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 2 3 4 5 6 7 8 9"},
    {/*60*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "each2"},
    {/*61*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "each6"},
    {/*62*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "each2"},
    {/*63*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "each2"},
    {/*64*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "each6"},
    {/*65*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "each2"},
    {/*66*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "each2"},
    {/*67*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "each6"},
    {/*68*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "each2"},
    {/*69*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "each2"},
    {/*70*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "each6"},
    {/*71*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "each2"},
    {/*72*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "last10"},
    {/*73*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "last10"},
    {/*74*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "last10"},
    {/*75*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "last10"},
    {/*76*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "last10"},
    {/*77*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "last10"},
    {/*78*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "last10"},
    {/*79*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "last10"},
    {/*80*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "last10"},
    {/*81*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "last10"},
    {/*82*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "last10"},
    {/*83*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "last10"},

    // B-frames
    {/*84*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 3 5 7 9 11 13"},
    {/*85*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 3 5 7 9 11 13"},
    {/*86*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 3 5 7 9 11 13"},
    {/*87*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 3 5 7 9 11 13"},
    {/*88*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 3 5 7 9 11 13"},
    {/*89*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 3 5 7 9 11 13"},
    {/*90*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 3 5 7 9 11 13"},
    {/*91*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 3 5 7 9 11 13"},
    {/*92*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 3 5 7 9 11 13"},
    {/*93*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 3 5 7 9 11 13"},
    {/*94*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 3 5 7 9 11 13"},
    {/*95*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 3 5 7 9 11 13"},

    // MFX_SKIPFRAME_BRC_ONLY
    {/*96*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*97*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*98*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*99*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*100*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*101*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*102*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*103*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*104*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*105*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*106*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*107*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},

    {/*108*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*109*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*110*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*111*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*112*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*113*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_ON},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*114*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*115*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*116*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*117*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*118*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
    {/*119*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 7},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
        {EXT_COD, &tsStruct::mfxExtCodingOption.CAVLC, MFX_CODINGOPTION_OFF},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_BRC_ONLY}}, "10 5 30 10"},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class SurfProc : public tsSurfaceProcessor
{
    std::string m_file_name;
    mfxU32 m_nframes;
    tsRawReader m_raw_reader;
    mfxEncodeCtrl* pCtrl;
    mfxFrameInfo* pFrmInfo;
    std::vector<mfxU32> m_skip_frames;
    std::map<mfxU32, mfxU32> m_skip_values;
    mfxU32 m_curr_frame;
    mfxU32 m_target_frames;
  public:
    SurfProc(const char* fname, mfxFrameInfo& fi, mfxU32 n_frames)
        : m_file_name(fname)
        , m_nframes(n_frames)
        , m_raw_reader(fname, fi, n_frames)
        , pCtrl(0)
        , pFrmInfo(&fi)
        , m_curr_frame(0)
        , m_target_frames(n_frames)
    {}
    ~SurfProc() {} ;

    mfxStatus Init(mfxEncodeCtrl& ctrl, std::vector<mfxU32>& skip_frames)
    {
        pCtrl = &ctrl;
        m_skip_frames = skip_frames;
        for (size_t i = 0; i < skip_frames.size(); i++) {
            m_skip_values[m_skip_frames[i]] = 1;
        }
        return MFX_ERR_NONE;
    }

    mfxStatus SetSkipValues(std::vector<mfxU32>& values)
    {
        if (values.size() != m_skip_frames.size())
        {
            assert(values.size() == m_skip_frames.size());
            return MFX_ERR_UNSUPPORTED;
        }

        for (size_t i = 0; i < values.size(); i++) {
            m_skip_values[m_skip_frames[i]] = values[i];
        }

        return MFX_ERR_NONE;
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (m_curr_frame >= m_nframes)
        {
            m_eos = true;
            return MFX_ERR_NONE;
        }

        mfxStatus sts = m_raw_reader.ProcessSurface(s);

        if (m_raw_reader.m_eos)  // re-read stream from the beginning to reach target NumFrames
        {
            m_raw_reader.ResetFile();
            sts = m_raw_reader.ProcessSurface(s);
        }

        if (m_skip_frames.size())
        {
            std::vector<mfxU32>::iterator it = std::find(m_skip_frames.begin(), m_skip_frames.end(), m_curr_frame);
            if (it != m_skip_frames.end())
            {
                pCtrl->SkipFrame = m_skip_values[*it];
            }
            else
            {
                pCtrl->SkipFrame = 0;
            }
        }
        else
        {
            pCtrl->SkipFrame = 1;
        }
        s.Data.TimeStamp = m_curr_frame++;
        return sts;
    }
};

class BsDump : public tsBitstreamProcessor, tsParserH264AU
{
    tsBitstreamWriter m_writer;
    std::vector<mfxU32> m_skip_frames;
    mfxU32 m_curr_frame;
    mfxU32 numMb;
    mfxU16 m_skip_mode;
    bool is_progressive;
public:
    BsDump(const char* fname)
        : tsBitstreamProcessor()
        , tsParserH264AU(BS_H264_INIT_MODE_CABAC|BS_H264_INIT_MODE_CAVLC)
        , m_writer(fname)
        , m_curr_frame(0)
        , numMb(0)
        , m_skip_mode(0)
    {
        set_trace_level(0);
    }
    ~BsDump() {}
    mfxStatus Init(std::vector<mfxU32>& skip_frames, mfxFrameInfo& fi, mfxU16 skip_mode)
    {
        numMb = (fi.Width / 16) * (fi.Height / 16);
        m_skip_frames = skip_frames;
        m_skip_mode = skip_mode;
        is_progressive = !!(fi.PicStruct == MFX_PICSTRUCT_PROGRESSIVE);
        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        bool skipped_frame = (bool)!m_skip_frames.size();
        if (std::find(m_skip_frames.begin(), m_skip_frames.end(), m_curr_frame) != m_skip_frames.end())
        {
            skipped_frame = true;
        }

        if (skipped_frame && m_skip_mode == MFX_SKIPFRAME_INSERT_NOTHING)
        {
            if (bs.DataLength != 0)
            {
                g_tsLog << "ERROR: frame should be skipped\n";
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }
        }
        else
        {
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);
            mfxU32 numMb_real = 0;
            for (int i = 0; i < 1 + !is_progressive; i++)
            {
                UnitType& au = ParseOrDie();
                if (skipped_frame && m_skip_mode == MFX_SKIPFRAME_INSERT_DUMMY)
                {
                    for (Bs32u i = 0; i < au.NumUnits; i++)
                    {
                        if (!(au.NALU[i].nal_unit_type == 0x01 || au.NALU[i].nal_unit_type == 0x05))
                        {
                            continue;
                        }
                        byte type = au.NALU[i].slice_hdr->slice_type % 5;
                        if ((type % 5) == 2)   // skip I frames
                        {
                            continue;
                        }
                        char ftype[5] = {'P', 'B', 'I', 'S', 'S'};
                        numMb_real += au.NALU[i].slice_hdr->NumMb;
                        for (Bs32u nmb = 0; nmb < au.NALU[i].slice_hdr->NumMb; nmb++)
                        {
                            if (0 == au.NALU[i].slice_hdr->mb[nmb].mb_skip_flag)
                            {
                                g_tsLog << "ERROR: frame#" << bs.TimeStamp << " (" << ftype[type] << ")" <<
                                           " mb#" << nmb << ": mb_skip_flag is not set\n";
                                return MFX_ERR_INVALID_VIDEO_PARAM;
                            }
                        }
                    }
                }
            }
            
            if (skipped_frame && m_skip_mode == MFX_SKIPFRAME_INSERT_DUMMY)
            {
                if (numMb_real != numMb)
                {
                    g_tsLog << "ERROR: real NumMb=" << numMb_real
                            << " is not equal to targer=" << numMb << "\n";
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }
            }
        }
        m_curr_frame++;

        return m_writer.ProcessBitstream(bs, nFrames);
    }
};

int TestSuite::RunTest(unsigned int id)
{
    int err = 0;
    TS_START;
    const tc_struct& tc = test_case[id];
    PrintTcStruct(tc);

    MFXInit();

    const mfxU32 nframes = 900;
    m_par.mfx.GopPicSize = 300;
    m_par.mfx.GopRefDist = 1;

    SETPARS(m_pPar, MFXPAR);

    mfxExtCodingOption& cod = m_par;
    SETPARS(&cod, EXT_COD);

    mfxExtCodingOption2& cod2 = m_par;
    SETPARS(&cod2, EXT_COD2);

    std::vector<mfxU32> skip_frames;
    std::vector<mfxU32> skip_values;
    if (0 == strncmp(tc.skips.c_str(), "each2", strlen("each2")))
    {
        for (mfxU32 i = 1; i < nframes; i += 2)
            skip_frames.push_back(i);
    }
    else if (0 == strncmp(tc.skips.c_str(), "each6", strlen("each6")))
    {
        for (mfxU32 i = 1; i < nframes; i += 6)
            skip_frames.push_back(i);
    }
    else if (0 == strncmp(tc.skips.c_str(), "last10", strlen("last10")))
    {
        for (mfxU32 i = nframes - 1; i > (nframes - 11); i--)
            skip_frames.push_back(i);

        // repeat skips for each GOP
        size_t sz = skip_frames.size();
        for (size_t i = 0; i < sz; i++)
        {
            skip_frames.push_back(skip_frames[i] - m_par.mfx.GopPicSize);
        }
    }
    else
    {
        std::stringstream stream(tc.skips);
        mfxU32 f;
        while (stream >> f)
        {
            skip_frames.push_back(f);
            if (cod2.SkipFrame == MFX_SKIPFRAME_BRC_ONLY)
            {
                stream >> f;
                skip_values.push_back(f);
            }
        }

        // repeat skips for each GOP
        size_t gops = nframes / m_par.mfx.GopPicSize;
        size_t skips = skip_frames.size();
        for (size_t i = 1; i < gops; ++i)
        {
            if (skip_frames.back() + m_par.mfx.GopPicSize >= nframes)
            {
                break;
            }
            for (size_t j = 0; j < skips; ++j)
            {
                skip_frames.push_back(skip_frames[j] + i * m_par.mfx.GopPicSize);
                if (cod2.SkipFrame == MFX_SKIPFRAME_BRC_ONLY)
                {
                    skip_values.push_back(skip_values[i]/* + m_par.mfx.GopPicSize*/);
                }
            }
        }
    }

    std::string input = ENV("TS_INPUT_YUV", "");
    std::string in_name;
    if (input.length())
    {
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height = 0;
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width = 0;

        std::stringstream s(input);
        s >> in_name;
        s >> m_par.mfx.FrameInfo.Width;
        s >> m_par.mfx.FrameInfo.Height;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
    }
    if (!in_name.length() || !m_par.mfx.FrameInfo.Width || !m_par.mfx.FrameInfo.Height)
    {
        g_tsLog << "ERROR: input stream is not defined\n";
        return 1;
    }

    // set up parameters for case
    if (m_par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        if (m_par.mfx.FrameInfo.Width < 1280)
        {
            m_par.mfx.MaxKbps = m_par.mfx.TargetKbps = 700;
        }
        else
        {
            m_par.mfx.MaxKbps = m_par.mfx.TargetKbps = 1200;
        }
        m_par.mfx.InitialDelayInKB = 0;
    }
    if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
    {
        m_par.mfx.MaxKbps = mfxU16(m_par.mfx.TargetKbps * 1.4);
    }
    if (m_par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        mfxU32 fr = mfxU32(m_par.mfx.FrameInfo.FrameRateExtN / m_par.mfx.FrameInfo.FrameRateExtD);
        // buffer = 0.5 sec
        m_par.mfx.BufferSizeInKB = mfxU16(m_par.mfx.MaxKbps / fr * mfxU16(fr / 2));
        m_par.mfx.InitialDelayInKB = mfxU16(m_par.mfx.BufferSizeInKB / 2);
    }

    SetFrameAllocator();

    // setup input stream
    SurfProc surf_proc(in_name.c_str(), m_par.mfx.FrameInfo, nframes);
    surf_proc.Init(m_ctrl, skip_frames);
    if (cod2.SkipFrame == MFX_SKIPFRAME_BRC_ONLY)
    {
        surf_proc.SetSkipValues(skip_values);
    }
    m_filler = &surf_proc;
    g_tsStreamPool.Reg();

    // setup output stream
    char tmp_out[10];
#pragma warning(disable:4996)
    sprintf(tmp_out, "%04d.h264", id+1);
#pragma warning(default:4996)
    std::string out_name = ENV("TS_OUTPUT_NAME", tmp_out);
    BsDump b(out_name.c_str());
    if (m_par.mfx.GopRefDist != 1)
    {
        for (size_t i = 0; i < skip_frames.size(); i++)
            skip_frames[i]++;
    }
    b.Init(skip_frames, m_par.mfx.FrameInfo, cod2.SkipFrame);
    m_bs_processor = &b;

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);
    EncodeFrames(nframes);

    // check bitrate
    if (m_par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        size_t nframes_br = nframes;
        g_tsLog << "Checking bitrate...\n";
        if (cod2.SkipFrame == MFX_SKIPFRAME_BRC_ONLY)
        {
            g_tsLog << "  frames encoded = " << nframes << "\n";
            for (size_t i = 0; i < skip_values.size(); i++)
            {
                nframes_br += skip_values[i];
            }
            g_tsLog << "  Number of frames to calculate bitrate = " << nframes_br << "\n";
        }
        std::ifstream in(out_name.c_str(), std::ifstream::ate | std::ifstream::binary);
        mfxU32 size = (mfxU32)in.tellg();
        mfxI32 bitrate = (8 * size *
            (m_par.mfx.FrameInfo.FrameRateExtN / m_par.mfx.FrameInfo.FrameRateExtD)) / nframes_br;
        mfxI32 target = m_par.mfx.TargetKbps * 1000;
        g_tsLog << "  File size: " << size << ";  Real bitrate: " << 
                    bitrate << ";  Target bitrate: " << target << ".\n";

        // choose threshold for bitrate deviation
        mfxF32 threshold = 0.1;
        if(0 == strncmp(tc.skips.c_str(), "each2", strlen("each2")))
            threshold = 0.2;
        if(0 == strncmp(tc.skips.c_str(), "each6", strlen("each6")))
            threshold = 0.2;

        if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CBR &&
            abs(target - bitrate) > target * threshold)
        {
            g_tsLog << "Real bitrate=" << bitrate << " is differ from required=" << target << "\n";
            g_tsLog << "ERROR: Real bitrate is differ from required\n";
            err++;
        }
        if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_VBR &&
            bitrate > (m_par.mfx.MaxKbps * 1000))
        {
            g_tsLog << "Real bitrate=" << bitrate << " is bigger than MaxKbps=" << m_par.mfx.MaxKbps << "\n";
            g_tsLog << "ERROR: Real bitrate is bigger than MaxKbps\n";
            err++;
        }
    }

    TS_END;
    return err;
}

TS_REG_TEST_SUITE_CLASS(avce_skipframes_tektronix);
};


