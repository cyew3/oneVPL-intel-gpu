/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

//#define DUMP_BS
#define FRAME_TO_ENCODE 70
#define PROGRESSIVE  MFX_PICSTRUCT_PROGRESSIVE
#define FIELD        MFX_PICSTRUCT_FIELD_SINGLE

namespace hevce_nalutype
{
    using namespace BS_HEVC;

    enum
    {
        DEFAULT  = 0,  /* default nalu type setting */
        EXPLICIT,      /* normal nalu type setting */

        EXT_IDR_N_LP, /* special case for IDR_W_RADL to IDR_N_LP */
        EXT_CRA2TRAIL,/* speical case for CRA to TRAIL */
        EXT_TRAIL2CRA,/* speical case for TRAIL to CRA */
    };

    struct Frame
    {
        mfxU32 type;
        mfxU8 nalType;
        mfxI32 poc;
        mfxU32 bpo;
        mfxU16 PLayer;
        bool   bSecondField;
        mfxI32 lastRAP;
        mfxI32 IPoc;
        mfxI32 prevIPoc;
        mfxI32 nextIPoc;
    };

    struct ExternalFrame
    {
        mfxU32 type;
        mfxU8  nalType;
        mfxI32 poc;
        mfxU16 PLayer;
        bool   bSecondField;

        mfxI32 lastRAP;
        mfxI32 lastRANalType;
    };
    typedef std::vector<ExternalFrame> ExternalFrameVec;

    bool isIRAPPicture(mfxU8 nalu_type)
    {
        return ((nalu_type >= BLA_W_LP) && (nalu_type <= /* RSV_IRAP_VCL23 */ CRA_NUT));
    }

    bool isLeadingPicture(mfxU8 nalu_type)
    {
        return ((nalu_type >= RADL_N) && (nalu_type <= RASL_R));
    }

    bool isTrailingPicture(mfxU8 nalu_type)
    {
        return ((nalu_type >= TRAIL_N) && (nalu_type <= STSA_R));
    }

    mfxU8 GetFrameType(mfxVideoParam const & video, mfxU32 frameOrder)
    {
        mfxU32 gopOptFlag = video.mfx.GopOptFlag;
        mfxU32 gopPicSize = video.mfx.GopPicSize;
        mfxU32 gopRefDist = video.mfx.GopRefDist;
        mfxU32 idrPicDist = gopPicSize * (video.mfx.IdrInterval);

        if (gopPicSize == 0xffff) //infinite GOP
            idrPicDist = gopPicSize = 0xffffffff;

        bool bFields = !!(video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_SINGLE);
        mfxU32 fo = bFields ? frameOrder / 2 : frameOrder;
        bool   bSecondField = bFields ? (frameOrder & 1) != 0 : false;
        bool   bIdr = (idrPicDist ? fo % idrPicDist : fo) == 0;

        if (bIdr)
            return bSecondField ? (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF) : (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

        if (fo % gopPicSize == 0)
            return (mfxU8)(bSecondField ? MFX_FRAMETYPE_P : MFX_FRAMETYPE_I) | MFX_FRAMETYPE_REF;

        if (fo % gopPicSize % gopRefDist == 0)
            return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);

        if (((frameOrder + 1) % gopPicSize == 0 && (gopOptFlag & MFX_GOP_CLOSED)) ||
                (idrPicDist && (frameOrder + 1) % idrPicDist == 0)) {
            return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);
        }

        return (MFX_FRAMETYPE_B);
    }

    void PrintNaluType (const mfxU8 nalType)
    {
        const char *string;
        switch (nalType) {
            /*Trailing pictures*/
            case 0: string = "TRAIL_N";     break;
            case 1: string = "TRAIL_R";     break;
            case 2: string = "TSA_N";       break;
            case 3: string = "TSA_R";       break;
            case 4: string = "STSA_N";      break;
            case 5: string = "STSA_R";      break;
            /*Leading pictures*/
            case 6: string = "RADL_N";      break;
            case 7: string = "RADL_R";      break;
            case 8: string = "RASL_N";      break;
            case 9: string = "RASL_R";      break;
            /*IRAP*/
            case 16: string = "BLA_W_LP";   break;
            case 17: string = "BLA_W_RADL"; break;
            case 18: string = "BLA_N_LP";   break;
            case 19: string = "IDR_W_RADL"; break;
            case 20: string = "IDR_N_LP";   break;
            case 21: string = "CRA_NUT";    break;

            default:
                g_tsLog << "ERROR: Unknown NAL type\n";
                string = "NULL";
                EXPECT_EQ(MFX_ERR_NONE, -1);
                break;
        }
        g_tsLog << "NaluType: " << string << "\n";
    };


    class EncodeEmulator
    {
    protected:

        class LastReorderedFieldInfo
        {
        public:
            mfxI32     m_poc;
            bool       m_bReference;
            bool       m_bFirstField;

            LastReorderedFieldInfo() :
                m_poc(-1),
                m_bReference(false),
                m_bFirstField(false){}

            void Reset()
            {
                m_poc = -1;
                m_bReference = false;
                m_bFirstField = false;
            }
            void SaveInfo(Frame const & frame)
            {
                m_poc = frame.poc;
                m_bReference = ((frame.type & MFX_FRAMETYPE_REF) != 0);
                m_bFirstField = !frame.bSecondField;
            }
            void CorrectFrameInfo(Frame & frame)
            {
                if ( !isCorrespondSecondField(frame) )
                    return;
                // copy params to the 2nd field
                if (m_bReference)
                    frame.type |= MFX_FRAMETYPE_REF;
            }
            bool isCorrespondSecondField(Frame const & frame)
            {
                if (m_poc + 1 != frame.poc || !frame.bSecondField || !m_bFirstField)
                    return false;
                return true;
            }
            bool bFirstField() { return m_bFirstField; }
        };

        typedef std::vector<Frame>           FrameArray;
        typedef std::vector<Frame>::iterator FrameIterator;

        FrameArray                  m_queue;
        LastReorderedFieldInfo      m_lastFieldInfo;

        FrameArray                  m_dpb;

        tsExtBufType<mfxVideoParam> m_emuPar;

        mfxI32                      m_lastIdr;   // for POC calculation
        mfxI32                      m_anchorPOC; // for P-Pyramid
        Frame                       m_lastFrame;

    private:

        mfxU8 GetNALUType(Frame const & frame, bool isRAPIntra)
        {
            const bool isI   = !!(frame.type & MFX_FRAMETYPE_I);
            const bool isRef = !!(frame.type & MFX_FRAMETYPE_REF);
            const bool isIDR = !!(frame.type & MFX_FRAMETYPE_IDR);

            if (isIDR)
                return IDR_W_RADL;

            if (isI && isRAPIntra)
            {
                return CRA_NUT;
            }

            if (frame.poc > frame.lastRAP)
            {
                return isRef ? TRAIL_R : TRAIL_N;
            }

            if (isRef)
                return RASL_R;
            return RASL_N;
        }

        bool HasL1(mfxI32 poc)
        {
            for (auto it = m_dpb.begin(); it < m_dpb.end(); it++)
                if (it->poc > poc)
                    return true;
            return false;
        }

        mfxU32 BRefOrder(mfxU32 displayOrder, mfxU32 begin, mfxU32 end, mfxU32 counter, bool & ref)
        {
            ref = (end - begin > 1);

            mfxU32 pivot = (begin + end) / 2;
            if (displayOrder == pivot)
                return counter;
            else if (displayOrder < pivot)
                return BRefOrder(displayOrder, begin, pivot, counter + 1, ref);
            else
                return BRefOrder(displayOrder, pivot + 1, end, counter + 1 + pivot - begin, ref);
        }

        mfxU32 GetBiFrameLocation(mfxU32 displayOrder, mfxU32 num, bool &ref)
        {
            ref = false;
            return BRefOrder(displayOrder, 0, num, 0, ref);
        }

        mfxU32 BPyrReorder(const std::vector<FrameIterator> & bframes)
        {
            mfxU32 num = (mfxU32)bframes.size();
            if (bframes[0]->bpo == (mfxU32)MFX_FRAMEORDER_UNKNOWN)
            {
                bool bRef = false;

                for(mfxU32 i = 0; i < (mfxU32)bframes.size(); i++)
                {
                    bframes[i]->bpo = GetBiFrameLocation(i,num, bRef);
                    if (bRef)
                        bframes[i]->type |= MFX_FRAMETYPE_REF;
                }
            }
            mfxU32 minBPO = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
            mfxU32 ind = 0;
            for(mfxU32 i = 0; i < (mfxU32)bframes.size(); i++)
            {
                if (bframes[i]->bpo < minBPO)
                {
                    ind = i;
                    minBPO = bframes[i]->bpo;
                }
            }
            return ind;
        }

        FrameIterator Reorder(bool flush, bool bFields)
        {
            FrameIterator begin = m_queue.begin();
            FrameIterator end = m_queue.begin();

            while (end != m_queue.end())
            {
                if ((end != begin) && (end->type & MFX_FRAMETYPE_IDR))
                {
                    flush = true;
                    break;
                }
                end++;
            }

            if (bFields && m_lastFieldInfo.bFirstField())
            {
                while (begin != end && !m_lastFieldInfo.isCorrespondSecondField(*begin))
                    begin++;

                if (begin != end)
                {
                    m_lastFieldInfo.CorrectFrameInfo(*begin);
                    return begin;
                }
                else
                    begin = m_queue.begin();
            }

            FrameIterator top = Reorder(begin, end, flush, bFields);

            if (top == end)
            {
                return top;
            }

            if (bFields)
            {
                m_lastFieldInfo.SaveInfo(*top);
            }

            return top;
        }

        FrameIterator Reorder(FrameIterator begin, FrameIterator end, bool flush, bool bFields)
        {
            FrameIterator top = begin;
            FrameIterator b0 = end; // 1st non-ref B with L1 > 0
            std::vector<FrameIterator> bframes;

            bool isBPyramid = !!(((mfxExtCodingOption2&)m_emuPar).BRefType == MFX_B_REF_PYRAMID);

            while ( top != end && (top->type & MFX_FRAMETYPE_B))
            {
                if (HasL1(top->poc) && (!top->bSecondField))
                {
                    if (isBPyramid)
                        bframes.push_back(top);
                    else if (top->type & MFX_FRAMETYPE_REF)
                    {
                        if (b0 == end || (top->poc - b0->poc < bFields + 2))
                            return top;
                    }
                    else if (b0 == end)
                        b0 = top;
                }
                top ++;
            }

            if (!bframes.empty())
            {
                return bframes[BPyrReorder(bframes)];
            }

            if (b0 != end)
                return b0;

            bool strict = !!(m_emuPar.mfx.GopOptFlag & MFX_GOP_STRICT);
            if (flush && top == end && begin != end)
            {
                top --;
                if (strict)
                    top = begin;
                else
                    top->type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;

                if (top->bSecondField && top != begin)
                {
                    top--;
                    if (strict)
                        top = begin;
                    else
                        top->type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                }
            }

            return top;
        }

    public:
        EncodeEmulator() :
            m_lastIdr(0)
            , m_anchorPOC(-1)
            {
                m_lastFrame.IPoc = -1;
                m_lastFrame.prevIPoc = -1;
                m_lastFrame.nextIPoc = -1;
            };

        ~EncodeEmulator() {};

        void Init(tsExtBufType<mfxVideoParam> & par)
        {
            m_emuPar = par;
        }

        ExternalFrame ProcessFrame(mfxI32 order)
        {
            mfxI32 poc = -1;
            ExternalFrame out = { 0, 0, poc, 0 };
            FrameIterator itOut = m_queue.end();

            bool bIsFieldCoding       = !!(m_emuPar.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_SINGLE);
            mfxExtCodingOption3 & CO3 = m_emuPar;
            bool isPPyramid           = !!(MFX_P_REF_PYRAMID == CO3.PRefType);

            mfxU32 type = 0;

            //=================1. Get type, poc, put frame in the queue=====================
            if (order >= 0)
            {
                type = GetFrameType(m_emuPar, order - m_lastIdr);

                if (type & MFX_FRAMETYPE_IDR)
                    m_lastIdr = order;

                poc = order - m_lastIdr;

                if (type & MFX_FRAMETYPE_I)
                {
                    m_anchorPOC = order;
                }

                mfxU16 PLayer = 0;
                if (isPPyramid)
                {
                    mfxU16 & nMaxRefL0 = CO3.NumRefActiveP[0];

                    mfxI32 idx = std::abs(( poc >> bIsFieldCoding) - (m_anchorPOC >> bIsFieldCoding) ) % nMaxRefL0;
                    static const mfxU16 pattern[4] = {0,2,1,2};
                    PLayer = pattern[idx];
                }
                m_queue.push_back({type, (mfxU8)-1, poc, (mfxU32)MFX_FRAMEORDER_UNKNOWN, PLayer, bIsFieldCoding && (poc & 1), -1, -1, -1, -1});
            }

            //=================2. Reorder frames, fill output frame poc, type, etc=====================
            itOut = Reorder(order < 0, bIsFieldCoding);
            if (itOut == m_queue.end())
                return out;

            bool isIdr   = !!(itOut->type & MFX_FRAMETYPE_IDR);
            bool isRef   = !!(itOut->type & MFX_FRAMETYPE_REF);
            bool isI     = !!(itOut->type & MFX_FRAMETYPE_I);
            bool isB     = !!(itOut->type & MFX_FRAMETYPE_B);

            itOut->lastRAP = m_lastFrame.lastRAP;

            if (isI)
            {
                itOut->IPoc = itOut->poc;
                itOut->prevIPoc = m_lastFrame.IPoc;
                itOut->nextIPoc = -1;
            }
            else
            {
                if (itOut->poc >= m_lastFrame.IPoc)
                {
                    itOut->IPoc = m_lastFrame.IPoc;
                    itOut->prevIPoc = m_lastFrame.prevIPoc;
                    itOut->nextIPoc = m_lastFrame.nextIPoc;
                }
                else
                {
                    itOut->IPoc = m_lastFrame.prevIPoc;
                    itOut->prevIPoc = -1;
                    itOut->nextIPoc = m_lastFrame.IPoc;
                }
            }

            out.poc          = itOut->poc;
            out.type         = itOut->type;
            out.bSecondField = itOut->bSecondField;

            //=================3. Update DPB=====================
            if (isIdr)
                m_dpb.clear();

            if (itOut->poc > itOut->lastRAP &&
                m_lastFrame.poc <= m_lastFrame.lastRAP)
            {
                const mfxI32 & lastRAP = itOut->lastRAP;
                // On the 1st TRAIL remove all except IRAP
                m_dpb.erase(std::remove_if(m_dpb.begin(), m_dpb.end(),
                                                [&lastRAP] (Frame const & entry) { return entry.poc != lastRAP; } ),
                                        m_dpb.end());
            }

            //=================4. Save current frame in DPB=====================
            if (isRef)
            {
                if (m_dpb.size() == m_emuPar.mfx.NumRefFrame)
                {
                    auto toRemove = m_dpb.begin();
                    if (isPPyramid)
                    {
                        auto weak = m_dpb.begin();

                        // remove a picture with the highest layer
                        for (auto it = m_dpb.begin(); it != m_dpb.end(); it ++)
                        {
                            if (bIsFieldCoding && it->poc / 2 == itOut->poc / 2)
                                continue;
                            if (weak->PLayer < it->PLayer)
                                weak = it;
                        }
                        toRemove = weak;
                    }

                    m_dpb.erase(toRemove);
                }
                m_dpb.push_back(*itOut);
            }

            itOut->nalType = GetNALUType(*itOut, !bIsFieldCoding);

            if (itOut->nalType == CRA_NUT || itOut->nalType == IDR_W_RADL)
                itOut->lastRAP = itOut->poc;

            m_lastFrame = *itOut;

            m_queue.erase(itOut);

            return out;
        }
    };

    class TestSuite
        : public tsBitstreamProcessor
        , public tsSurfaceProcessor
        , public tsParserHEVC
        , public tsVideoEncoder
        , public EncodeEmulator
    {
        private:
#ifdef DUMP_BS
            tsBitstreamWriter m_writer;
#endif
        public:
            struct tc_struct
            {
                mfxI32 NaluTypeCtrl;
                mfxU16 PicStruct;
                mfxU16 EncodedOrder;
                mfxU16 GopRefDist;
                mfxU16 GopOptFlag;
                mfxU16 IdrInterval;
                mfxU16 BRefType;
                mfxU16 PRefType;
            };

            static const tc_struct test_case[];
            static const unsigned int n_cases;
            mfxU32 m_frameCnt;
            mfxU32 m_CRACnt;
            mfxI32 m_nalTypeCtrl;
            mfxI32 m_anchorPOC;
            mfxU8  m_anchorNaluType;
            ExternalFrameVec m_encodedFrameVec;
            ExternalFrame    m_extFrameEmulator;
            ExternalFrameVec m_extFrameEmuVec;

            TestSuite()
                : tsParserHEVC()
                , tsVideoEncoder(MFX_CODEC_HEVC)
#ifdef DUMP_BS
                , m_writer("/tmp/debug.265")
#endif
                ,m_frameCnt(0)
                ,m_CRACnt(0)
                ,m_nalTypeCtrl(0)
                ,m_anchorPOC(0)
                ,m_anchorNaluType(0)
                {
                    m_bs_processor = this;
                    m_filler = this;
                }

            ~TestSuite() {}

            int RunTest(unsigned int id);
            mfxStatus ProcessSurface(mfxFrameSurface1& s);
            mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);

            void Init()
            {
                GetVideoParam();
                EncodeEmulator::Init(m_par);
            }
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        /*  NalTypeCtrl   PicStruct EncodedOrder GopRefDist GopOptFlag IdrInterval BRefType  PRefType */
        /* default nalu_type */
        /*01*/{DEFAULT,       PROGRESSIVE,    0,       3,          0,          0,       1,        0}, // p + disp order
        /*02*/{DEFAULT,             FIELD,    0,       3,          0,          0,       1,        0}, // i + disp order
        /*03*/{DEFAULT,       PROGRESSIVE,    0,       1,          0,          1,       1,        2}, // p + disp order + P-PYR
        /*04*/{DEFAULT,             FIELD,    0,       1,          0,          1,       1,        2}, // i + disp order + P-PYR
        /*05*/{DEFAULT,       PROGRESSIVE,    0,       3,          0,          0,       2,        0}, // p + disp order + B-PYR
        /*06*/{DEFAULT,             FIELD,    0,       3,          0,          0,       2,        0}, // i + disp order + B-PYR
        /*07*/{DEFAULT,       PROGRESSIVE,    0,       4,          2,          2,       2,        0}, // p + disp order + B-PYR + strict
        /*08*/{DEFAULT,             FIELD,    0,       4,          2,          2,       2,        0}, // i + disp order + B-PYR + strict
        /*09*/{DEFAULT,       PROGRESSIVE,    1,       3,          0,          0,       1,        0}, // p + encoded order
        /*10*/{DEFAULT,             FIELD,    1,       3,          0,          0,       1,        0}, // i + encoded order
        /*11*/{DEFAULT,       PROGRESSIVE,    1,       3,          0,          0,       2,        0}, // p + encoded order + B-PYR
        /*12*/{DEFAULT,             FIELD,    1,       3,          0,          0,       2,        0}, // i + encoded order + B-PYR

        /* external positive case */
        /*13*/{EXPLICIT,      PROGRESSIVE,    1,       1,          0,          1,       1,        2}, // p + P-PYR
        /*14*/{EXPLICIT,            FIELD,    1,       1,          0,          1,       1,        2}, // i + P-PYR
        /*15*/{EXPLICIT,      PROGRESSIVE,    1,       4,          0,          1,       1,        0}, // p + 1 idrInterval
        /*16*/{EXPLICIT,            FIELD,    1,       4,          0,          1,       1,        0}, // i + 1 idrInterval
        /*17*/{EXPLICIT,      PROGRESSIVE,    1,       4,          0,          2,       2,        0}, // p + B-PYR
        /*18*/{EXPLICIT,            FIELD,    1,       4,          0,          2,       2,        0}, // i + B-PYR
        /*19*/{EXPLICIT,      PROGRESSIVE,    1,       4,          2,          2,       2,        0}, // p + B-PYR + strict
        /*20*/{EXPLICIT,            FIELD,    1,       4,          2,          2,       2,        0}, // i + B-PYR + strict
        /*21*/{EXPLICIT,      PROGRESSIVE,    1,       4,          3,          2,       2,        0}, // P + B-PYR + strict + close
        /*22*/{EXPLICIT,            FIELD,    1,       4,          3,          2,       2,        0}, // i + B-PYR + strcit + close
        /*23*/{EXPLICIT,      PROGRESSIVE,    1,       7,          3,          2,       2,        0}, // P + B-PYR + strict + close + RefDist=7
        /*24*/{EXPLICIT,            FIELD,    1,       7,          3,          2,       2,        0}, // i + B-PYR + strcit + close + RefDist=7
        /*25*/{EXT_CRA2TRAIL, PROGRESSIVE,    1,       4,          0,          2,       2,        0}, // P + B-PYR + EXT_CRA2TRAIL
        /*26*/{EXT_CRA2TRAIL,       FIELD,    1,       4,          0,          2,       2,        0}, // i + B-PYR + EXT_CRA2TRAIL
        /*27*/{EXT_TRAIL2CRA, PROGRESSIVE,    1,       4,          0,          2,       2,        0}, // P + B-PYR + EXT_TRAIL2CRA
        /*28*/{EXT_IDR_N_LP,  PROGRESSIVE,    1,       4,          0,          2,       2,        0}, // P + B-PYR + EXT_IDR_N_LP
        /*29*/{EXT_IDR_N_LP,        FIELD,    1,       4,          0,          2,       2,        0}, // i + B-PYR + EXT_IDR_N_LP
    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);


    mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& s)
    {
        if (!m_par.mfx.EncodedOrder)
            return MFX_ERR_NONE;

        ExternalFrame f = ProcessFrame(m_frameCnt);
        while (f.poc < 0)
        {
            m_frameCnt++;
            f = ProcessFrame(m_frameCnt);
        }

        s.Data.FrameOrder  = m_extFrameEmulator.poc  = f.poc;
        m_pCtrl->FrameType = m_extFrameEmulator.type = f.type;

#if (MFX_VERSION >= MFX_VERSION_NEXT)
        bool bFields = !!(m_par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_SINGLE);
        bool isIDR  = !!(m_extFrameEmulator.type & MFX_FRAMETYPE_IDR);
        bool isI    = !!(m_extFrameEmulator.type & MFX_FRAMETYPE_I);
        bool isP    = !!(m_extFrameEmulator.type & MFX_FRAMETYPE_P);
        bool isRef  = !!(m_extFrameEmulator.type & MFX_FRAMETYPE_REF);

        if (m_nalTypeCtrl > DEFAULT)
        {
            if (isIDR)
            {
                m_extFrameEmuVec.clear();

                if(m_nalTypeCtrl == EXT_IDR_N_LP)
                {
                    m_pCtrl->MfxNalUnitType = MFX_HEVC_NALU_TYPE_IDR_N_LP;
                    m_extFrameEmulator.nalType = IDR_N_LP;
                }
                else
                {
                    m_pCtrl->MfxNalUnitType = MFX_HEVC_NALU_TYPE_IDR_W_RADL;
                    m_extFrameEmulator.nalType = IDR_W_RADL;
                }
            }
            else if (isI)
            {
                m_pCtrl->MfxNalUnitType = MFX_HEVC_NALU_TYPE_CRA_NUT;
                m_extFrameEmulator.nalType = CRA_NUT;

                if (m_nalTypeCtrl == EXT_CRA2TRAIL)
                {
                    m_pCtrl->MfxNalUnitType = MFX_HEVC_NALU_TYPE_TRAIL_R;
                    m_extFrameEmulator.nalType = TRAIL_R;
                }

                if ( m_extFrameEmulator.nalType == CRA_NUT)
                    m_CRACnt++;
            }
            else if (m_extFrameEmulator.poc > m_extFrameEmulator.lastRAP)
            {
                if (isRef)
                {
                    m_pCtrl->MfxNalUnitType = MFX_HEVC_NALU_TYPE_TRAIL_R;
                    m_extFrameEmulator.nalType = TRAIL_R;

                    if (m_nalTypeCtrl == EXT_TRAIL2CRA && isP && !bFields)
                    {
                        m_pCtrl->MfxNalUnitType = MFX_HEVC_NALU_TYPE_CRA_NUT;
                        m_extFrameEmulator.nalType = CRA_NUT;
                        m_extFrameEmulator.type = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
                        m_pCtrl->FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
                    }
                }
                else
                {
                    m_pCtrl->MfxNalUnitType = MFX_HEVC_NALU_TYPE_TRAIL_N;
                    m_extFrameEmulator.nalType = TRAIL_N;
                }
            }
            else if (m_extFrameEmulator.poc < m_extFrameEmulator.lastRAP)
            {
                if (m_extFrameEmulator.lastRANalType == IDR_W_RADL)
                {
                    if (isRef)
                    {
                        m_pCtrl->MfxNalUnitType = MFX_HEVC_NALU_TYPE_RADL_R;
                        m_extFrameEmulator.nalType = RADL_R;
                    }
                    else
                    {
                        m_pCtrl->MfxNalUnitType = MFX_HEVC_NALU_TYPE_RADL_N;
                        m_extFrameEmulator.nalType = RADL_N;
                    }
                }
                else
                {
                    if (m_extFrameEmulator.lastRANalType == CRA_NUT)
                    {
                        if (m_CRACnt % 2 == 0)//set RASL pictures that follow even CRA picture
                        {
                            if (isRef)
                            {
                                m_pCtrl->MfxNalUnitType = MFX_HEVC_NALU_TYPE_RASL_R;
                                m_extFrameEmulator.nalType = RASL_R;
                            }
                            else
                            {
                                m_pCtrl->MfxNalUnitType = MFX_HEVC_NALU_TYPE_RASL_N;
                                m_extFrameEmulator.nalType = RASL_N;
                            }
                        }
                        else//set RADL pictures that follow odd CRA picture
                        {
                            if (isRef)
                            {
                                m_pCtrl->MfxNalUnitType = MFX_HEVC_NALU_TYPE_RADL_R;
                                m_extFrameEmulator.nalType = RADL_R;
                            }
                            else
                            {
                                m_pCtrl->MfxNalUnitType = MFX_HEVC_NALU_TYPE_RADL_N;
                                m_extFrameEmulator.nalType = RADL_N;
                            }
                        }
                    }
                }
            }

            if (isIRAPPicture(m_extFrameEmulator.nalType))
            {
                m_extFrameEmulator.lastRAP     = m_extFrameEmulator.poc;
                m_extFrameEmulator.lastRANalType = m_extFrameEmulator.nalType;
            }

            m_extFrameEmuVec.push_back(m_extFrameEmulator);
        }
#endif

        m_frameCnt++;
        return MFX_ERR_NONE;
    }


    mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        SetBuffer0(bs);

#ifdef DUMP_BS
        m_writer.ProcessBitstream(bs, nFrames);
#endif

        auto& AU = ParseOrDie();
        auto& sh = AU.pic->slice[0]->slice[0];
        for(mfxU32 i = 0; i < AU.NumUnits; i ++)
        {
            if (!IsHEVCSlice(AU.nalu[i]->nal_unit_type))
                continue;

            const auto& nalu = AU.nalu[i];

#if (MFX_VERSION >= MFX_VERSION_NEXT)
            /* CHECK external nal type settings */
            mfxExtCodingOption3& co3 = m_par;
            if (m_nalTypeCtrl != DEFAULT)
            {
                EXPECT_EQ(co3.EnableNalUnitType, MFX_CODINGOPTION_ON);

                ExternalFrameVec::iterator extFrameEmu =
                    std::find_if(m_extFrameEmuVec.begin(),  m_extFrameEmuVec.end(), [&sh] (const ExternalFrame & extFrameEmu) { return extFrameEmu.poc == sh.PicOrderCntVal; });

                if (extFrameEmu != m_extFrameEmuVec.end())
                {
                    g_tsLog << "--Expected nal type:\n";
                    PrintNaluType((*extFrameEmu).nalType);
                    g_tsLog << "--Actual nalu type: \n";
                    PrintNaluType(nalu->nal_unit_type);

                    EXPECT_EQ((*extFrameEmu).nalType, nalu->nal_unit_type);

                    m_extFrameEmuVec.erase(extFrameEmu);
                }
            }
#endif

            if (isIRAPPicture(nalu->nal_unit_type))
            {
                EXPECT_EQ(nalu->nuh_temporal_id_plus1 - 1, 0);
                EXPECT_EQ(sh.type, I);

                m_anchorNaluType = nalu->nal_unit_type;
                m_anchorPOC = sh.PicOrderCntVal;

                m_encodedFrameVec.clear();
            }

            if (isLeadingPicture(nalu->nal_unit_type))
            {
                //no leading pictures are allowed for an IDR picture with nalu type IDR_N_LP
                EXPECT_NE(m_anchorNaluType, IDR_N_LP);
                //leading picture precedes the closest previous IRAP picture in output order.
                EXPECT_LT(sh.PicOrderCntVal , m_anchorPOC);

                //all leading pictures of an IDR_W_RADL picture must be decodable and use the RADL type.
                //RASL pictures are not allowed to be associated with any IDR picture.
                if (m_anchorNaluType == IDR_W_RADL)
                {
                    EXPECT_NE(nalu->nal_unit_type, RASL_N);
                    EXPECT_NE(nalu->nal_unit_type, RASL_R);
                }

                if (sh.type != I)
                {
                    for (mfxU8 lx = 0; lx <= (sh.type == B); lx++)
                    {
                        auto& actualList = (lx == 0) ?  AU.pic->RefPicList0 : AU.pic->RefPicList1;
                        mfxU8 listSize = (lx == 0) ?  sh.num_ref_idx_l0_active_minus1 + 1 : sh.num_ref_idx_l1_active_minus1 + 1;
                        g_tsLog << "-- List" << (int)lx <<"\n";
                        for (mfxU8 refIdx = 0; refIdx < listSize; refIdx++)
                        {
                            g_tsLog << "---- ref" << (int)refIdx <<": ";
                            PrintNaluType(actualList[refIdx]->slice[0]->nal_unit_type);
                            g_tsLog <<"; Ref POC: " << actualList[refIdx]->PicOrderCntVal << "\n";

                            EXPECT_NE(actualList[refIdx]->slice[0]->nal_unit_type, RADL_N);
                            EXPECT_NE(actualList[refIdx]->slice[0]->nal_unit_type, RASL_N);
                            //every picture that depends on a RASL picture must also be a RASL picture.
                            if (actualList[refIdx]->slice[0]->nal_unit_type == RASL_R)
                                EXPECT_GE(nalu->nal_unit_type, RASL_N); //nalu_type should be RASL_N or RASL_R
                        }
                    }

                    for (mfxU32 i = 0; i < m_encodedFrameVec.size(); i++)
                    {
                        //leading picture must precedes trailing pictures associated wiht the same IRAP picture in decoding order.
                        EXPECT_EQ(isTrailingPicture(m_encodedFrameVec[i].nalType), 0);

                        //RASL pictures must precede RADL pictures in output order
                        if (nalu->nal_unit_type == RASL_N || nalu->nal_unit_type == RASL_R)
                        {
                            EXPECT_NE(m_encodedFrameVec[i].nalType, RADL_N);
                            EXPECT_NE(m_encodedFrameVec[i].nalType, RADL_R);
                        }
                    }

                }
            }

            if (isTrailingPicture(nalu->nal_unit_type))
            {
                if (nalu->nal_unit_type == TSA_R || nalu->nal_unit_type == TSA_N
                        || (nalu->nuh_layer_id == 0 && (nalu->nal_unit_type == STSA_R || nalu->nal_unit_type == STSA_N)))
                {
                    EXPECT_GT(nalu->nuh_temporal_id_plus1 - 1, 0);
                }

                //trailing picture follows the associated IRAP picture.
                EXPECT_GT(sh.PicOrderCntVal , m_anchorPOC);

                if (sh.type != I)
                {
                    for (mfxU8 lx = 0; lx <= (sh.type == B); lx++)
                    {
                        auto& actualList = (lx == 0) ?  AU.pic->RefPicList0 : AU.pic->RefPicList1;
                        mfxU8 listSize = (lx == 0) ?  sh.num_ref_idx_l0_active_minus1 + 1 : sh.num_ref_idx_l1_active_minus1 + 1;
                        g_tsLog << "-- List" << (int)lx <<"\n";
                        for (mfxU8 refIdx = 0; refIdx < listSize; refIdx++)
                        {
                            g_tsLog << "---- ref" << (int)refIdx <<": ";
                            PrintNaluType(actualList[refIdx]->slice[0]->nal_unit_type);
                            g_tsLog <<"; Ref POC: " << actualList[refIdx]->PicOrderCntVal << "\n";

                            //not allowed to depend on any leading pictures
                            EXPECT_EQ(isLeadingPicture(actualList[refIdx]->slice[0]->nal_unit_type), 0);

                            //not allowed to depend on any pictures of previous IRAP picture.
                            EXPECT_GE(actualList[refIdx]->PicOrderCntVal, m_anchorPOC);
                        }
                    }
                }
            }

            //Save each picture in decoder order, starting with one IRAP picture
            ExternalFrame frameInfo;
            frameInfo.nalType  = nalu->nal_unit_type;
            frameInfo.type     = sh.type;
            frameInfo.poc      = sh.PicOrderCntVal;

            m_encodedFrameVec.push_back(frameInfo);

        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }


    int TestSuite::RunTest(unsigned int id)
    {
        mfxStatus sts = MFX_ERR_NONE;
        const tc_struct& tc = test_case[id];

        TS_START;

        mfxExtCodingOption2& co2 = m_par;
        co2.BRefType = tc.BRefType;

        mfxExtCodingOption3& co3 = m_par;
        co3.PRefType = tc.PRefType;
        co3.GPB = 0x20;
#if (MFX_VERSION >= MFX_VERSION_NEXT)
        m_nalTypeCtrl = tc.NaluTypeCtrl;
        if (tc.NaluTypeCtrl == DEFAULT)
            co3.EnableNalUnitType = MFX_CODINGOPTION_OFF;
        else
            co3.EnableNalUnitType = MFX_CODINGOPTION_ON;
#endif

        m_par.mfx.FrameInfo.PicStruct = tc.PicStruct;
        m_par.mfx.EncodedOrder  = tc.EncodedOrder;
        m_par.mfx.GopRefDist  = tc.GopRefDist;
        m_par.mfx.GopOptFlag  = tc.GopOptFlag;
        m_par.mfx.IdrInterval = tc.IdrInterval;
        m_par.AsyncDepth = 1;
        m_par.mfx.GopPicSize  = 15;

        Init();
        GetVideoParam();
        AllocBitstream();
        AllocSurfaces();

        EncodeFrames(FRAME_TO_ENCODE);
        Close();
        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_nalutype);
}
