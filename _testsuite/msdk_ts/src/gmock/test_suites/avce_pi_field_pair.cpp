
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
/* This test vierfies the P/I field pair feature.
 * The main thing after adjusting B or P to P/I is the reference list of the P
 * and B that POC is before or after the P/I.
 * Only add most representative pictures to the testcase elements if NumRef is to large.
 */

#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace avce_pi_field_pair
{

class TestSuite : tsVideoEncoder
{
public:
    const mfxI8* sn;

    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC)
    {
        sn = g_tsStreamPool.Get("YUV/stockholm_720x576i_252.yuv");
        g_tsStreamPool.Reg();

        m_par.mfx.FrameInfo.Width     = 720;
        m_par.mfx.FrameInfo.Height    = 576;
        m_par.mfx.FrameInfo.CropW     = 720;
        m_par.mfx.FrameInfo.CropH     = 576;
    }
    ~TestSuite() { }
    mfxI32 RunTest(mfxU32 id);

    static const mfxU32 n_cases;
    static const mfxU16 max_refs      = 16;
    static const mfxU16 max_elements  = 16; // max num elements that need to be checked;
    static const mfxU16 max_mfx_pars  = 5;

    struct ctrl{
        const tsStruct::Field* field;
        mfxU32 value;
    };

    /* As frame has been set to P/I ,the l0 reflist of P and the l0/l1 reflist of
     * B may be affected by this. Some representative pics (element) have been added
     * to the testcase for validation.
     * */
    struct tc_struct
    {
        mfxU16  nFrames;
        mfxU16  bRef;
        mfxI32  posSetPI[2];               // frame positions to be set to P/I filed pair
        mfxI32  elementNum;                // num of pics that needed to check reflists
        mfxU16  encodedOrder;              // EncodedOrder flag
        ctrl    mfx[max_mfx_pars];

        /* the information of pic needs to be checked */
        struct elementToCheck {
            mfxU32 pocToCheck;             // pic's poc
            mfxI32 l0RefPoc[max_refs];     // pic's l0 reflist (POC num)
            mfxI32 l1RefPoc[max_refs];     // pic's l1 reflist (POC num)
        } element[max_elements];

    };

    static const tc_struct test_case[];
};

class SFiller : public tsSurfaceProcessor
{
private:
    mfxI32                       m_count;
    mfxVideoParam&               m_par;
    mfxEncodeCtrl&               m_ctrl;
    const TestSuite::tc_struct & m_tc;
    tsRawReader*                 m_reader;

public:
    SFiller(const TestSuite::tc_struct & tc, mfxEncodeCtrl& ctrl, mfxVideoParam& par, const mfxI8 *sn)
        : m_count(0)
        , m_par(par)
        , m_ctrl(ctrl)
        , m_tc(tc)
        , m_reader(0)
    {
        m_reader = new tsRawReader(sn, m_par.mfx.FrameInfo, tc.nFrames);
    };

    ~SFiller()
    {
        if (m_reader)
        {
            delete m_reader;
            m_reader = NULL;
        }
    };

    mfxStatus ProcessSurface(mfxFrameSurface1& s);
    void GetFrameTypeAndOrder(mfxU16 &frameType, mfxI32 &frameOrder);
};


#define PR (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF)
#define BU (MFX_FRAMETYPE_B)
#define BR (MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF)
#define PI (MFX_FRAMETYPE_P | MFX_FRAMETYPE_xI | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF)

/* These tables are used to easily get frameOrder or frametype.
 * And these only for GopRefDist = 4, if extending encodedorder
 * testcase it needs to add tables according to GopRefDist.
 */
mfxU16 FrameTypeTab[2][3][4] =
{
  {  // for B ref off
    { PI, PR, BU, BU },    // change the first  B to PI
    { PI, BU, PR, BU },    // change the middle B to PI
    { PI, BU, BU, PR }     // change the last   B to PI
   },
  {  // for B ref on
    { PI, PR, BR, BU },    // change the first  B to PI
    { PI, BU, PR, BU },    // change the middle B to PI
    { PI, BR, BU, PR }     // change the last   B to PI
   },
};

mfxU16 FrameOrderOffset[2][3][4] =
{
  {  // for B ref off
    { 1, 4, 2, 3 },       // change the first  B to PI
    { 2, 1, 4, 3 },       // change the middle B to PI
    { 3, 1, 2, 4 }        // change the last   B to PI
   },
  {  // for B ref on
    { 1, 4, 2, 3 },       // change the first  B to PI
    { 2, 1, 4, 3 },       // change the middle B to PI
    { 3, 2, 1, 4 }        // change the last   B to PI
   },
};

// map encoder order to display order table(Bref on/off)
mfxI32 FrameOrderTab[2][49] =
{
  { 0,
    4, 1, 2, 3, 8, 5, 6, 7,
    12,9,10,11,16,13,14,15,
    20,17,18,19,24,21,22,23,
    28,25,26,27,32,29,30,31,
    36,33,34,35,40,37,38,39,
    44,41,42,43,48,45,46,47,
  },
  { 0,
    4, 2, 1, 3, 8, 6, 5, 7,
    12,10,9,11,16,14,13,15,
    20,18,17,19,24,22,21,23,
    28,26,25,27,32,30,29,31,
    36,34,33,35,40,38,37,39,
    44,42,41,43,48,46,45,47,
  },
};

/*  Get frameOrder and type for surface by m_count var.
 *
 *  We treat input yuv as things that have been reorder well according to
 *  encoderd order. What we need to do is to get the right frametype and display
 *  order for each surface.
 *
 *  We have to modify frametype's calculation when setting B to P/I in encodedOrder mode.
 *  eg: B1 B2 B3 P1 B4 B5 B6 P2 , encodedOrder input should be: P1 B1 B2 B3 P2 B4 B5 B6
 *  when changing B5 to P/I,      encodedOrder input should be: P1 B1 B2 B3 PI B4 P2 B6
 *  even the P has been affected and when B ref on or B's position that setted to B changes
 *  all these need to be considered
 *
 */
void SFiller::GetFrameTypeAndOrder(mfxU16 &frameType, mfxI32 &frameOrder)
{
    mfxI32 refDist    = m_par.mfx.GopRefDist;
    mfxU16 isBPyramid = (m_tc.bRef == MFX_B_REF_PYRAMID);

    if (m_count == 0)
    {
        frameType  = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
        frameOrder = 0;
        return;
    }

    if (refDist == 1)
    {
        if (m_count == m_tc.posSetPI[0])
            frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xI | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
        else
            frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;

        frameOrder = m_count;
        return;
    }

    frameOrder = FrameOrderTab[isBPyramid][m_count];

    // change P to P/I case
    if (m_tc.posSetPI[0] % refDist == 0)
    {
        if ((m_count - 1)%refDist == 0)     // current is P frame
        {
            if (frameOrder == m_tc.posSetPI[0])
                frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xI | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
            else
                frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        }
        else                                // current is B frame
        {
            if (isBPyramid && (m_count % refDist - 1) % 2 == 1)
                frameType = MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF;
            else
                frameType = MFX_FRAMETYPE_B;
        }
        return;
    }

    // for B change to PI case
    mfxI32 prevP = ((m_tc.posSetPI[0] - 1) / refDist) * refDist;  // previous P frame display order
    mfxU16 idxB  = m_tc.posSetPI[0] - prevP - 1;                  // the index of B changed to PI

    if ((m_count > prevP) && (m_count <= prevP + refDist))        // P/I happens in this cycle
    {
        frameType  = FrameTypeTab[isBPyramid][idxB][m_count - prevP - 1];
        frameOrder = FrameOrderOffset[isBPyramid][idxB][m_count - prevP - 1] + prevP;
    }
    else                                                          // normal case
    {
        if ((m_count - 1)%refDist == 0)
             frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        else if (isBPyramid && (m_count % refDist - 1) % 2 == 1)
            frameType = MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF;
        else
            frameType = MFX_FRAMETYPE_B;
    }
}

mfxStatus SFiller::ProcessSurface(mfxFrameSurface1& s)
{
    mfxU16 frameType;
    mfxI32 frameOrder;

    memset(&m_ctrl, 0, sizeof(m_ctrl));
    m_reader->ProcessSurface(s);

    if (m_tc.encodedOrder)
    {
        // for encodedOrder mode all P/I setting and adjustment have
        // been done in the GetFrameType.
        GetFrameTypeAndOrder(frameType, frameOrder);
        s.Data.FrameOrder = frameOrder;
        m_ctrl.FrameType  = frameType;
    }
    else
    {
        // change specified frame to P/I field pair
        if (((m_count == m_tc.posSetPI[0]) || (m_count == m_tc.posSetPI[1])) && (m_count > 0))
            m_ctrl.FrameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_xI | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xREF;
    }

    m_count++;

    return MFX_ERR_NONE;
}

class BitstreamChecker : public tsBitstreamProcessor, public tsParserAVC2
{
private:
    mfxVideoParam&               m_par;
    const TestSuite::tc_struct & m_tc;
public:

    BitstreamChecker(const TestSuite::tc_struct & tc, mfxVideoParam& par)
        : tsParserAVC2()
        , m_par(par)
        , m_tc(tc)
    {
    }

    ~BitstreamChecker() { }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
    mfxI32 FindPocToCheck(mfxU32 poc);

};

mfxI32 BitstreamChecker::FindPocToCheck(mfxU32 poc)
{
  mfxI32 i   =  0;
  mfxI32 ret = -1;

  for (i = 0; i < m_tc.elementNum; i++)
  {
       if (m_tc.element[i].pocToCheck == poc)
           return i;
  }
  return ret;
}

mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    mfxU32 checked = 0;
    mfxI32 index   = 0;

    SetBuffer(bs);
    nFrames *= 1 + !(bs.PicStruct & MFX_PICSTRUCT_PROGRESSIVE);

    while (checked++ < nFrames)
    {
        UnitType& au = ParseOrDie();

        for (Bs32u au_ind = 0; au_ind < au.NumUnits; au_ind++)
        {
            if ((au.nalu[au_ind]->nal_unit_type != 1 && au.nalu[au_ind]->nal_unit_type != 5))
                continue;

            BS_AVC2::Slice& s = *au.nalu[au_ind]->slice;

            // whether this pic needed to check reflist
            index = FindPocToCheck(s.POC);
            if (index < 0) continue;

            // check the l0 reflist
            for (mfxU16 i = 0; i <= s.num_ref_idx_l0_active_minus1; i ++)
            {
                if (m_tc.element[index].l0RefPoc[i] != -1)
                    EXPECT_EQ(m_tc.element[index].l0RefPoc[i], (mfxU32)s.RefPicListX[0][i].POC);
            }

            if ((s.slice_type != 1) && (s.slice_type != 6))
                 continue;

            // check the l1 reflist
            for (mfxU16 i = 0; i <= s.num_ref_idx_l1_active_minus1; i ++)
            {
                if (m_tc.element[index].l1RefPoc[i] != -1)
                    EXPECT_EQ(m_tc.element[index].l1RefPoc[i], (mfxU32)s.RefPicListX[1][i].POC);
            }

        }
    }

    bs.DataLength = 0;

    return MFX_ERR_NONE;
}

#define PROGR MFX_PICSTRUCT_PROGRESSIVE
#define TFF   MFX_PICSTRUCT_FIELD_TFF
#define BFF   MFX_PICSTRUCT_FIELD_BFF

#define MFX_PARS(ps,gps,grd,nrf,tu) {\
            {&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, ps}, \
            {&tsStruct::mfxVideoParam.mfx.GopPicSize,          gps}, \
            {&tsStruct::mfxVideoParam.mfx.GopRefDist,          grd}, \
            {&tsStruct::mfxVideoParam.mfx.NumRefFrame,         nrf}, \
            {&tsStruct::mfxVideoParam.mfx.TargetUsage,         tu}, \
}

const TestSuite::tc_struct TestSuite::test_case[] =
{
    /*00
     *    P P P P/I P (change P to P/I)
     *    check P (behind P/I)'s l0 ref
     *    the I field should be as ref while P shouldn't of the P/I
     */
    {
        40, MFX_B_REF_OFF, { 15, -1 }, 4, 0,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 1, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 32, { 31, -1 },                { -1 } },       //check P pics behind P/I
            { 33, { 31, 32, -1 },            { -1 } },
            { 34, { 32, 33, 31, -1 },        { -1 } },
            { 35, { 33, 34, 31, 32, -1 },    { -1 } },
        }
    },

    /*01
     *    B B B P B B B P/I B B B P  (change P to P/I , B_REF_OFF)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        40, MFX_B_REF_OFF, { 20, -1 }, 6, 0,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 48, { 41, -1 },                { -1 } },      //check P pics behind P/I
            { 49, { 41, 48, -1 },            { -1 } },
            { 42, { 41, -1 },        { 48, 49, -1 } },      //check B pics behind P/I
            { 43, { 41, -1 },        { 49, 48, -1 } },
            { 38, { 32, 33, 24, 25, -1 }, { 40,-1 } },      //check B pics before P/I
            { 39, { 33, 32, 25, 24, -1 }, { 40,-1 } },
        }
    },

    /*02:
     *    B B B P B B B P/I B B B P  (change P to P/I , B_REF_ON)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        40, MFX_B_REF_PYRAMID, { 20, -1 }, 10, 0,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 48, { 41, -1 },                { -1 } },      //check P pics behind P/I
            { 49, { 48, 41, -1 },            { -1 } },
            { 42, { 41, -1 },        { 44, 45, -1 } },      //check B(unref) pics behind P/I
            { 43, { 41, -1 },        { 45, 44, -1 } },
            { 44, { 41, -1 },        { 48, 49, -1 } },      //check B(ref) pics behind P/I
            { 45, { 41, 44, -1 },    { 49, 48, -1 } },
            { 36, { 32, 33, 28, 29,-1 }, { 40, -1 } },      //check B(ref) pics before P/I
            { 37, { 33, 36, 32, -1 },    { 40, -1 } },
            { 38, { 36, 37, 32, 33,-1 }, { 40, -1 } },      //check B(unref) pics before P/I
            { 39, { 37, 36, 33, 32, -1 },{ 40, -1 } },
        }
    },

    /*03
     *    B B B P B P/I B P B B B P  (change B to P/I , B_REF_OFF)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        40, MFX_B_REF_OFF, { 18, -1 }, 6, 0,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 38, { 37, -1 },        { 40, 41, -1 } },      //check B pics behind P/I
            { 39, { 37, -1 },         { 41, 40,-1 } },
            { 40, { 37, -1 },                { -1 } },      //check P pics behind P/I
            { 41, { 37, 40, -1 },            { -1 } },
            { 34, { 32, 33, 24, 25, -1 },{ 36, -1 } },      //check B pics before P/I
            { 35, { 33, 32, 25, 24, -1 },{ 36, -1 } },
        }
    },

    /*04
     *    B B B P B P/I B P B B B P  (change B(ref) to P/I , B_REF_ON)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        40, MFX_B_REF_PYRAMID, { 18, -1 }, 6, 0,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 38, { 37, -1 },         { 40, 41, -1 } },      //check B pics behind P/I
            { 39, { 37, -1 },         { 41, 40, -1 } },
            { 40, { 37, -1 },                 { -1 } },      //check P pics behind P/I
            { 41, { 37, 40, -1 },             { -1 } },
            { 34, { 32, 33, 28, 29, -1 }, { 36, -1 } },      //check B pics before P/I
            { 35, { 33, 32, 29, 28, -1 }, { 36, -1 } },
        }
    },

    /*05
     *    B B B P B B P/I P B B B P  (change B(last) to P/I , B_REF_ON)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        40, MFX_B_REF_PYRAMID, { 19, -1 }, 6, 0,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 40, { 39, -1 },                 { -1 } },       //check P pics behind P/I
            { 41, { 40, 39, -1 },             { -1 } },
            { 44, { 40, 41, 39, -1 },  { 48, 49,-1 } },       //check B pics behind P/I
            { 45, { 41, 44, 40, -1 },  { 49, 48,-1 } },
            { 36, { 32, 33, 28, 29, -1 }, { 38, -1 } },       //check B pics before P/I
            { 37, { 33, 36, 32, -1 },     { 38, -1 } },
        }
    },

    /*06
     *    B B B P B B P/I P B B B P  (change B(last) to P/I , B_REF_ON, BFF)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        40, MFX_B_REF_PYRAMID, { 19, -1 }, 6, 0,
        MFX_PARS(/*PicStruct*/ BFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 40, { 39, -1 },                { -1 } },       //check P pics behind P/I
            { 41, { 40, 39, -1 },            { -1 } },
            { 44, { 40, 41, 39,-1 }, { 48, 49, -1 } },       //check B pics behind P/I
            { 45, { 41, 44, 40, -1 },{ 49, 48, -1 } },
            { 36, { 32, 33, 28, 29,-1 }, { 38, -1 } },       //check B pics before P/I
            { 37, { 33, 36, 32, -1 },    { 38, -1 } },
        }
    },

    /*07
     *    B B B B B P/I B B B B B P/I  (change two P to P/I , B_REF_ON, TFF)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        60, MFX_B_REF_PYRAMID, { 36, 42 }, 11, 0,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 6, /*NumRefFrame*/ 5, /*TU*/ 1),
        {
            { 84, { 73, -1 },                 { -1 } },       //check P(P/I) pic behind P/I
            { 96, { 85, -1 },                 { -1 } },       //check P pic behind the second P/I
            { 97, { 96, 85, -1 },             { -1 } },
            { 78, { 73, -1 },             { 84, -1 } },       //check B (ref) pics between two P/I
            { 79, { 73, 78, -1 },         { 84, -1 } },
            { 76, { 73, -1 },         { 78, 79, -1 } },
            { 77, { 73, 76, -1 },     { 79, 78, -1 } },
            { 82, { 78, 79, 76, 77, -1 } ,{ 84, -1 } },
            { 83, { 79, 82, 77, 78, -1 }, { 84, -1 } },
            { 74, { 73, -1 },         { 76, 77, -1 } },       //check B (unref) pics between two P/I
            { 75, { 73, -1 },         { 77, 76, -1 } },
        }
    },

    /*08
     *    B B B B B P B B P/I B B P/I  (change P and B(middle) to P/I , B_REF_ON, TFF)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        60, MFX_B_REF_PYRAMID, { 39, 42 }, 7, 0,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 6, /*NumRefFrame*/ 5, /*TU*/ 1),
        {
            { 84, { 79, -1 },                 { -1 } },       //check P(P/I) pic behind P/I
            { 82, { 79, -1 },             { 84, -1 } },       //check B pics between two P/Is
            { 83, { 79, 82, -1 },         { 84, -1 } },
            { 80, { 79, -1 },         { 82, 83, -1 } },
            { 81, { 79, -1 },         { 83, 82, -1 } },
            { 76, { 72, 73, 70, 71, -1 }, { 78, -1 } },       // check B before first P/I
            { 77, { 73, 76, 71, 72, -1 }, { 78, -1 } },
        }
    },

    /*09
     *    B B B B B P P/I B B B B P/I  (change P and B(unref) to P/I , B_REF_ON, TFF)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        60, MFX_B_REF_PYRAMID, { 37, 42 }, 9, 0,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 6, /*NumRefFrame*/ 5, /*TU*/ 1),
        {
            { 84, { 75, -1 },                     { -1 } },    //check P(P/I) pic behind P/I
            { 82, { 78, 79, 76, 77, -1 },     { 84, -1 } },    //check B pics between two P/I
            { 83, { 79 , 82, 77, 78, -1 },    { 84, -1 } },    //including both ref and unref B pics
            { 80, { 78, 79, 76, 77, -1 }, { 82, 83, -1 } },
            { 81, { 79, 78, 77, 76, -1 }, { 83, 82, -1 } },
            { 78, { 75, -1 },                 { 84, -1 } },
            { 79, { 75, 78, -1 },             { 84, -1 } },
            { 76, { 75, -1 },             { 78, 79, -1 } },
            { 77, { 75, 76, -1 },         { 79, 78, -1 } },
        }
    },

    /*10
     *    P P P P/I P (change P to P/I, EncodedOrder = 1 )
     *    check P (behind P/I)'s l0 ref
     *    the I field should be as ref while P shouldn't of the P/I
     */
    {
        40, MFX_B_REF_OFF, { 15, -1 }, 4, 1,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 1, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 32, { 31, -1 },                { -1 } },       //check P pics behind P/I
            { 33, { 31, 32, -1 },            { -1 } },
            { 34, { 32, 33, 31, -1 },        { -1 } },
            { 35, { 33, 34, 31, 32, -1 },    { -1 } },
        }
    },

    /*11
     *    B B B P B B B P/I B B B P  (change P to P/I , B_REF_OFF, EncodedOrder = 1)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        40, MFX_B_REF_OFF, { 20, -1 }, 6, 1,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 48, { 41, -1 },                { -1 } },      //check P pics behind P/I
            { 49, { 41, 48, -1 },            { -1 } },
            { 42, { 41, -1 },        { 48, 49, -1 } },      //check B pics behind P/I
            { 43, { 41, -1 },        { 49, 48, -1 } },
            { 38, { 32, 33, 24, 25, -1 }, { 40,-1 } },      //check B pics before P/I
            { 39, { 33, 32, 25, 24, -1 }, { 40,-1 } },
        }
    },

    /*12:
     *    B B B P B B B P/I B B B P  (change P to P/I , B_REF_ON, EncodedOrder = 1)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        40, MFX_B_REF_PYRAMID, { 20, -1 }, 10, 1,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 48, { 41, -1 },                { -1 } },      //check P pics behind P/I
            { 49, { 48, 41, -1 },            { -1 } },
            { 42, { 41, -1 },        { 44, 45, -1 } },      //check B(unref) pics behind P/I
            { 43, { 41, -1 },        { 45, 44, -1 } },
            { 44, { 41, -1 },        { 48, 49, -1 } },      //check B(ref) pics behind P/I
            { 45, { 41, 44, -1 },    { 49, 48, -1 } },
            { 36, { 32, 33, 28, 29,-1 }, { 40, -1 } },      //check B(ref) pics before P/I
            { 37, { 33, 36, 32, -1 },    { 40, -1 } },
            { 38, { 36, 37, 32, 33,-1 }, { 40, -1 } },      //check B(unref) pics before P/I
            { 39, { 37, 36, 33, 32, -1 },{ 40, -1 } },
        }
    },

    /*13
     *    B B B P B P/I B P B B B P  (change B to P/I , B_REF_OFF, EncodedOrder = 1)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        40, MFX_B_REF_OFF, { 18, -1 }, 6, 1,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 38, { 37, -1 },        { 40, 41, -1 } },      //check B pics behind P/I
            { 39, { 37, -1 },         { 41, 40,-1 } },
            { 40, { 37, -1 },                { -1 } },      //check P pics behind P/I
            { 41, { 37, 40, -1 },            { -1 } },
            { 34, { 32, 33, 24, 25, -1 },{ 36, -1 } },      //check B pics before P/I
            { 35, { 33, 32, 25, 24, -1 },{ 36, -1 } },
        }
    },

    /*14
     *    B B B P B P/I B P B B B P  (change B(ref) to P/I , B_REF_ON, EncodedOrder = 1)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        40, MFX_B_REF_PYRAMID, { 18, -1 }, 6, 1,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 38, { 37, -1 },         { 40, 41, -1 } },      //check B pics behind P/I
            { 39, { 37, -1 },         { 41, 40, -1 } },
            { 40, { 37, -1 },                 { -1 } },      //check P pics behind P/I
            { 41, { 37, 40, -1 },             { -1 } },
            { 34, { 32, 33, 28, 29, -1 }, { 36, -1 } },      //check B pics before P/I
            { 35, { 33, 32, 29, 28, -1 }, { 36, -1 } },
        }
    },

    /*15
     *    B B B P B B P/I P B B B P  (change B(last) to P/I , B_REF_ON, EncodedOrder = 1)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        40, MFX_B_REF_PYRAMID, { 19, -1 }, 6, 1,
        MFX_PARS(/*PicStruct*/ TFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 40, { 39, -1 },                 { -1 } },       //check P pics behind P/I
            { 41, { 40, 39, -1 },             { -1 } },
            { 44, { 40, 41, 39, -1 },  { 48, 49,-1 } },       //check B pics behind P/I
            { 45, { 41, 44, 40, -1 },  { 49, 48,-1 } },
            { 36, { 32, 33, 28, 29, -1 }, { 38, -1 } },       //check B pics before P/I
            { 37, { 33, 36, 32, -1 },     { 38, -1 } },
        }
    },

    /*16
     *    B B B P B B P/I P B B B P  (change B(last) to P/I , B_REF_ON, BFF, EncodedOrder = 1)
     *    check P and B (behind P/I)'s l0 ref
     *    check B(befor P/I)'s l1 ref
     *    l0 should use the I field as ref while l1 use the P of the P/I
     */
    {
        40, MFX_B_REF_PYRAMID, { 19, -1 }, 6, 1,
        MFX_PARS(/*PicStruct*/ BFF, /*GopPicSize*/ 0, /*GopRefDist*/ 4, /*NumRefFrame*/ 3, /*TU*/ 1),
        {
            { 40, { 39, -1 },                { -1 } },       //check P pics behind P/I
            { 41, { 40, 39, -1 },            { -1 } },
            { 44, { 40, 41, 39,-1 }, { 48, 49, -1 } },       //check B pics behind P/I
            { 45, { 41, 44, 40, -1 },{ 49, 48, -1 } },
            { 36, { 32, 33, 28, 29,-1 }, { 38, -1 } },       //check B pics before P/I
            { 37, { 33, 36, 32, -1 },    { 38, -1 } },
        }
    },

};

const mfxU32 TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

mfxI32 TestSuite::RunTest(mfxU32 id)
{
    TS_START;

    const tc_struct& tc = test_case[id];

    m_par.AsyncDepth   = 1;
    m_par.mfx.NumSlice = 1;
    m_pPar->IOPattern  = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    for (auto& ctrl : tc.mfx)
        if (ctrl.field)
            tsStruct::set(m_pPar, *ctrl.field, ctrl.value);

    ((mfxExtCodingOption2&)m_par).BRefType = tc.bRef;
    m_par.mfx.EncodedOrder = tc.encodedOrder;

    SFiller sf(tc, *m_pCtrl, *m_pPar, sn);
    BitstreamChecker c(tc, *m_pPar);
    m_filler = &sf;
    m_bs_processor = &c;

#if 0
    // dump stream
    tsBitstreamWriter bw("dump_pi_out.264");
    m_bs_processor = &bw;
#endif

    EncodeFrames(tc.nFrames);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_pi_field_pair);
}
