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
#include <vector>
#include <algorithm>

//#define DUMP_BS

namespace hevce_default_ref_lists
{
    using namespace BS_HEVC2;

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
        std::vector<Frame> ListX[2];
    };

    char FrameTypeToChar(mfxU32 type)
    {
        char c = (type & MFX_FRAMETYPE_I) ? 'i' : (type & MFX_FRAMETYPE_P) ? 'p' : 'b';
        return (type & MFX_FRAMETYPE_REF) ? char(toupper(c)) : c;
    }

    mfxU8 GetFrameType(
        mfxVideoParam const & video,
        mfxU32                frameOrder)
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
        {
            return bSecondField ? (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF) : (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);
        }

        if (fo % gopPicSize == 0)
            return (mfxU8)(bSecondField ? MFX_FRAMETYPE_P : MFX_FRAMETYPE_I) | MFX_FRAMETYPE_REF;

        if (fo % gopPicSize % gopRefDist == 0)
            return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);

        if ((gopOptFlag & MFX_GOP_STRICT) == 0)
            if (((frameOrder + 1) % gopPicSize == 0 && (gopOptFlag & MFX_GOP_CLOSED)) ||
                (idrPicDist && (frameOrder + 1) % idrPicDist == 0))
            {
                return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);
            }

        return (MFX_FRAMETYPE_B);
    }

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

            //=================4. Construct RPL=====================
            std::sort(m_dpb.begin(), m_dpb.end(),
                    [](Frame const & lhs, Frame const & rhs) { return lhs.poc < rhs.poc; }); // Sort DPB in POC ascending order

            std::vector<Frame> & L0 = out.ListX[0];
            std::vector<Frame> & L1 = out.ListX[1];

            L0.clear();
            L1.clear();

            if (!isI)
            {
                // Fill L0/L1
                for (auto it = m_dpb.begin(); it != m_dpb.end(); it++)
                {
                    bool list = it->poc > out.poc;
                    out.ListX[list].push_back(*it);
                }

                if (isPPyramid)
                {
                    // For P-pyramid remove entries with the highest layer
                    while (L0.size() > CO3.NumRefActiveP[out.PLayer])
                    {
                        auto weak = L0.begin();

                        for (auto it = L0.begin(); it != L0.end(); it ++)
                        {
                            if (weak->PLayer < it->PLayer && it->poc != L0.back().poc)
                                weak = it;
                        }

                        L0.erase(weak);
                    }
                }

                auto GreaterOrder = [](Frame const & lhs, Frame const & rhs) { return lhs.poc > rhs.poc; };
                auto LessOrder = [](Frame const & lhs, Frame const & rhs) { return lhs.poc < rhs.poc; };

                // L0 in descending order
                std::sort(L0.begin(), L0.end(), GreaterOrder);
                // L1 in ascending order
                std::sort(L1.begin(), L1.end(), LessOrder);

                if ((m_emuPar.mfx.GopOptFlag & MFX_GOP_CLOSED))
                {
                    const mfxI32 & IPoc = itOut->IPoc;
                    {
                        // Remove L0 refs beyond GOP
                        L0.erase(std::remove_if(L0.begin(), L0.end(),
                                                        [&IPoc] (const Frame & frame) { return frame.poc < IPoc; } ),
                                                L0.end());
                    }

                    const mfxI32 & nextIPoc = itOut->nextIPoc;
                    if (nextIPoc != -1)
                    {
                        // Remove L1 refs beyond GOP
                        L1.erase(std::remove_if(L1.begin(), L1.end(),
                                                        [&nextIPoc] (const Frame & frame) { return frame.poc >= nextIPoc; } ),
                                                L1.end());
                    }
                }

                // if B's L1 is zero (e.g. in case of closed gop)
                if (isB && !L1.size() && L0.size())
                    L1.push_back(L0[0]);

                if (!isB)
                {
                    L1 = L0;
                }
            }

            //=================5. Put curr frame in DPB=====================
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
        , public tsParserHEVC2
        , public EncodeEmulator
        , public tsVideoEncoder
    {
    private:
        std::vector<ExternalFrame> m_emu;
    #ifdef DUMP_BS
        tsBitstreamWriter m_writer;
    #endif

    public:
        static const unsigned int n_cases;

        typedef struct
        {
            mfxU16 PicStruct;
            mfxU16 GopPicSize;
            mfxU16 GopRefDist;
            mfxU16 GopOptFlag;
            mfxU16 IdrInterval;
            mfxU16 BRefType;
            mfxU16 PRefType;
            mfxU16 GPB;
            mfxU32 nFrames;
        } tc_struct;

        static const tc_struct test_case[];

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
    #ifdef DUMP_BS
            , m_writer("/tmp/debug.265")
    #endif
        {
            m_bs_processor = this;
            m_filler = this;
        }

        ~TestSuite(){}

        int RunTest(unsigned int id);

        mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
        {
            if (ps)
            {
                ExternalFrame f = ProcessFrame(m_cur < m_max ? m_cur : -1);

                if (f.poc >= 0)
                    m_emu.push_back(f);
            }

            if (m_cur >= m_max)
                return 0;

            m_cur++;

            return ps;
        }

        void Init()
        {
            GetVideoParam();
            EncodeEmulator::Init(m_par);
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

    #ifdef DUMP_BS
        m_writer.ProcessBitstream(bs, nFrames);
    #endif
            while (nFrames--)
            {
                auto& hdr = ParseOrDie();

                for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
                {
                    if (!IsHEVCSlice(pNALU->nal_unit_type))
                        continue;

                    const auto & sh = *pNALU->slice;

                    g_tsLog << "POC " << sh.POC << "\n";

                    auto frame = std::find_if(m_emu.begin(), m_emu.end(), [&sh] (const ExternalFrame & frame) { return frame.poc == sh.POC; });

                    if (frame == m_emu.end())
                    {
                        auto reorderedFrame = std::find_if(m_queue.begin(), m_queue.end(), [&sh] (const Frame & frame) { return frame.poc == sh.POC; });
                        if (reorderedFrame == m_queue.end())
                        {
                            g_tsLog << "ERROR: Test Problem: encoded frame emulation not found, check test logic\n";
                            return MFX_ERR_ABORTED;
                        }

                        static std::string slices[] = {"B", "P", "I" };

                        g_tsLog << "ERROR: Frame is in the reordering list and not encoded yet\n";
                        g_tsLog << " Slice type:\n";
                        g_tsLog << " - Actual: " << slices[sh.type]
                            << " - Expected: " << FrameTypeToChar(reorderedFrame->type) << "\n";
                    return MFX_ERR_ABORTED;
                    }

                    ExternalFrame & emulated = *frame;

                    mfxU32 lx[2] = { sh.num_ref_idx_l0_active, sh.num_ref_idx_l1_active };

                    for (mfxU32 list = 0; list < 2; list++)
                    {
                        RefPic * actualList = list == 0 ? sh.L0 : sh.L1;
                        g_tsLog << " List " << list << ":\n";

                        for (mfxU32 idx = 0; idx < lx[list]; idx++)
                        {
                            mfxI32 expectedPOC = (idx < emulated.ListX[list].size()) ? emulated.ListX[list][idx].poc : -1;
                            g_tsLog << " - Actual: " << actualList[idx].POC
                                << " - Expected: " << expectedPOC << "\n";
                            EXPECT_EQ(expectedPOC, actualList[idx].POC)
                                << " list = " << list
                                << " idx = " << idx << "\n";
                        }
                    }

                    m_emu.erase(frame);
                }
            }

            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
    //       PicStruct                 GopPicSize GopRefDist        GopOptFlag IdrInterval BRefType     PRefType   GPB   nFrames or
    //                                                                                                                   nFields (for interlaced cases)
    /* 00 */{ MFX_PICSTRUCT_PROGRESSIVE,        0,         0,                0,          0,       1,           0,    0,  300 }, // p + default
    /* 01 */{MFX_PICSTRUCT_FIELD_SINGLE,        0,         0,                0,          0,       1,           0,    0,  600 }, // i + default
    /* 02 */{ MFX_PICSTRUCT_PROGRESSIVE,        0,         0,                0,          1,       1,           0,    0,  300 }, // p + 1 IdrInterval
    /* 03 */{MFX_PICSTRUCT_FIELD_SINGLE,        0,         0,                0,          1,       1,           0,    0,  600 }, // i + 1 IdrInterval
    /* 04 */{ MFX_PICSTRUCT_PROGRESSIVE,       30,         3,                0,          0,       1,           0,    0,   70 }, // p + 2B
    /* 05 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         3,                0,          0,       1,           0,    0,  140 }, // i + 2B
    /* 06 */{ MFX_PICSTRUCT_PROGRESSIVE,       30,         3,                0,          1,       1,           0,    0,   70 }, // p + 2B + 1 IdrInterval
    /* 07 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         3,                0,          1,       1,           0,    0,  140 }, // i + 2B + 1 IdrInterval
    /* 08 */{ MFX_PICSTRUCT_PROGRESSIVE,       30,         3,              0x2,          1,       1,           0,    0,   70 }, // p + 2B + 1 IdrInterval + STRICT
    /* 09 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         3,              0x2,          1,       1,           0,    0,  140 }, // i + 2B + 1 IdrInterval + STRICT
    /* 10 */{ MFX_PICSTRUCT_PROGRESSIVE,       30,         4,                0,          0,       2,           0,    0,   70 }, // p + 3B PYR
    /* 11 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         4,                0,          0,       2,           0,    0,  140 }, // i + 3B PYR
    /* 12 */{ MFX_PICSTRUCT_PROGRESSIVE,      256,         8,                0,          0,       2,           0,    0,  300 }, // p + 7B PYR
    /* 13 */{MFX_PICSTRUCT_FIELD_SINGLE,      256,         8,                0,          0,       2,           0,    0,  600 }, // i + 7B PYR
    /* 14 */{ MFX_PICSTRUCT_PROGRESSIVE,       30,         4,                0,          2,       2,           0,    0,   70 }, // p + 3B PYR + 2 IdrInterval
    /* 15 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         4,                0,          2,       2,           0,    0,  140 }, // i + 3B PYR + 2 IdrInterval
    /* 16 */{ MFX_PICSTRUCT_PROGRESSIVE,       30,         8,                0,          2,       2,           0,    0,   70 }, // p + 7B PYR + 2 IdrInterval
    /* 17 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         8,                0,          2,       2,           0,    0,  140 }, // i + 7B PYR + 2 IdrInterval
    /* 18 */{ MFX_PICSTRUCT_PROGRESSIVE,       30,         8,              0x1,          0,       2,           0,    0,   70 }, // p + 7B PYR + CLOSED
    /* 19 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         8,              0x1,          0,       2,           0,    0,  140 }, // i + 7B PYR + CLOSED
    /* 20 */{ MFX_PICSTRUCT_PROGRESSIVE,       30,         8,              0x3,          0,       2,           0,    0,   70 }, // p + 7B PYR + CLOSED + STRICT
    /* 21 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         8,              0x3,          0,       2,           0,    0,  140 }, // i + 7B PYR + CLOSED + STRICT
    /* 22 */{ MFX_PICSTRUCT_PROGRESSIVE,       30,         2,                0,          0,       1,           0,    0,   32 }, // p + B
    /* 23 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         2,                0,          0,       1,           0,    0,   64 }, // i + B
    /* 24 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         2,              0x1,          0,       1,           0,    0,   66 }, // i + B + CLOSED
    /* 25 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         2,                0,          0,       1,           0,    0,   65 }, // i + B + even calls
    /* 26 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         2,                0,          0,       1,           0,    0,    3 }, // i + B + even calls
    /* 27 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         4,                0,          0,       2,           0,    0,   54 }, // i + 3B PYR
    /* 28 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         4,                0,          0,       2,           0,    0,   53 }, // i + 3B PYR + even calls
    /* 29 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         4,                0,          0,       1,           0,    0,   53 }, // i + 3B + even calls
    /* 30 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         3,              0x3,          0,       1,           0, 0x20,  140 }, // i + 2B + CLOSED + + STRICT
    /* 31 */{ MFX_PICSTRUCT_PROGRESSIVE,       30,         2,                0,          0,       1,           0, 0x20,   70 }, // p + B + GPB off
    /* 32 */{MFX_PICSTRUCT_FIELD_SINGLE,       30,         2,                0,          0,       1,           0, 0x20,   70 }, // i + B + GPB off
    /* 33 */{ MFX_PICSTRUCT_PROGRESSIVE,       15,         1,                0,          0,       1,           2,    0,   70 }, // p + P-PYR
    /* 34 */{MFX_PICSTRUCT_FIELD_SINGLE,       15,         1,                0,          0,       1,           2,    0,   70 }, // i + P-PYR
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        const tc_struct& tc = test_case[id];

        TS_START;

        mfxU32 nFrames = tc.nFrames;

        mfxExtCodingOption2& co2 = m_par;
        co2.BRefType = tc.BRefType;

        mfxExtCodingOption3& co3 = m_par;
        co3.PRefType = tc.PRefType;
        co3.GPB = tc.GPB;

        m_par.mfx.FrameInfo.PicStruct = tc.PicStruct;
        m_par.mfx.GopPicSize  = tc.GopPicSize;
        m_par.mfx.GopRefDist  = tc.GopRefDist;
        m_par.mfx.GopOptFlag  = tc.GopOptFlag;
        m_par.mfx.IdrInterval = tc.IdrInterval;

        m_max = nFrames;
        m_par.AsyncDepth = 1;

        Init();
        GetVideoParam();

        EncodeFrames(nFrames);
        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_default_ref_lists);
}