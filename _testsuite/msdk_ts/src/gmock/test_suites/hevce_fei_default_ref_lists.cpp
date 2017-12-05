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
#include "ts_fei_warning.h"

// #define DUMP_BS

namespace hevce_fei_default_ref_lists
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
        bool   bBottomField;
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
        bool   bBottomField;
        std::vector<Frame> ListX[2];
    };

    char FrameTypeToChar(mfxU32 type)
    {
        char c = (type & MFX_FRAMETYPE_I) ? 'i' : (type & MFX_FRAMETYPE_P) ? 'p' : 'b';
        return (type & MFX_FRAMETYPE_REF) ? char(toupper(c)) : c;
    }

    bool isBFF(mfxVideoParam const & video)
    {
        return ((video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BOTTOM) == MFX_PICSTRUCT_FIELD_BOTTOM);
    }

    mfxU8 GetFrameType(
        mfxVideoParam const & video,
        mfxU32                pictureOrder,
        bool                  isPictureOfLastFrame)
    {
        mfxU32 gopOptFlag = video.mfx.GopOptFlag;
        mfxU32 gopPicSize = video.mfx.GopPicSize;
        mfxU32 gopRefDist = video.mfx.GopRefDist;
        mfxU32 idrPicDist = gopPicSize * (video.mfx.IdrInterval);

        if (gopPicSize == 0xffff)
            idrPicDist = gopPicSize = 0xffffffff;

        bool bFields = !!(video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_SINGLE);

        mfxU32 frameOrder = bFields ? pictureOrder / 2 : pictureOrder;

        bool   bSecondField = bFields && (pictureOrder & 1);
        bool   bIdr = (idrPicDist ? frameOrder % idrPicDist : frameOrder) == 0;

        if (bIdr)
            return bSecondField ? (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF) : (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

        if (frameOrder % gopPicSize == 0)
            return (mfxU8)(bSecondField ? MFX_FRAMETYPE_P : MFX_FRAMETYPE_I) | MFX_FRAMETYPE_REF;

        if (frameOrder % gopPicSize % gopRefDist == 0)
            return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);

        if ((gopOptFlag & MFX_GOP_STRICT) == 0)
        {
            if (((frameOrder + 1) % gopPicSize == 0 && (gopOptFlag & MFX_GOP_CLOSED)) ||
                (idrPicDist && (frameOrder + 1) % idrPicDist == 0) ||
                isPictureOfLastFrame)
            {
                return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);
            }
        }

        return MFX_FRAMETYPE_B;
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
        mfxU32                      m_nMaxFrames;

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

        void Init(tsExtBufType<mfxVideoParam> & par, mfxU32 nMaxFrames)
        {
            m_emuPar = par;
            m_nMaxFrames = nMaxFrames;
        }

        ExternalFrame ProcessFrame(const mfxFrameSurface1 & s, bool flash)
        {
            ExternalFrame out = { 0, 0, -1, 0 };
            FrameIterator itOut = m_queue.end();

            bool bIsFieldCoding       = !!(m_emuPar.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_SINGLE);
            mfxExtCodingOption3 & CO3 = m_emuPar;
            bool isPPyramid           = !!(MFX_P_REF_PYRAMID == CO3.PRefType);

            //=================1. Get type, poc, put frame in the queue=====================
            if (!flash)
            {
                bool isPictureOfLastFrameInGOP = false;

                if ((s.Data.FrameOrder >> bIsFieldCoding) == ((m_nMaxFrames - 1) >> bIsFieldCoding)) // EOS
                    isPictureOfLastFrameInGOP = true;

                Frame frame;

                frame.type = GetFrameType(m_emuPar, (bIsFieldCoding ? (m_lastIdr %2) : 0) + s.Data.FrameOrder - m_lastIdr, isPictureOfLastFrameInGOP);

                if (frame.type & MFX_FRAMETYPE_IDR)
                    m_lastIdr = s.Data.FrameOrder;

                frame.poc          = s.Data.FrameOrder - m_lastIdr;
                frame.bSecondField = bIsFieldCoding && (s.Data.FrameOrder & 1);
                frame.bBottomField = false;
                if (bIsFieldCoding)
                {
                    frame.bBottomField = (s.Info.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF)) == 0 ?
                        (isBFF(m_emuPar) != frame.bSecondField) :
                        (s.Info.PicStruct & MFX_PICSTRUCT_FIELD_BFF) != 0;
                }

                if (frame.type & MFX_FRAMETYPE_I)
                {
                    m_anchorPOC = frame.poc;
                }

                if (isPPyramid)
                {
                    mfxU16 & nMaxRefL0 = CO3.NumRefActiveP[0];

                    mfxI32 idx = std::abs(( frame.poc >> bIsFieldCoding) - (m_anchorPOC >> bIsFieldCoding) ) % nMaxRefL0;
                    static const mfxU16 pattern[4] = {0,2,1,2};
                    frame.PLayer = pattern[idx];
                }

                frame.nalType  = (mfxU8)-1;
                frame.bpo      = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
                frame.lastRAP  = -1;
                frame.IPoc     = -1;
                frame.prevIPoc = -1;
                frame.nextIPoc = -1;

                m_queue.push_back(frame);
            }

            //=================2. Reorder frames, fill output frame poc, type, etc=====================
            itOut = Reorder(flash, bIsFieldCoding);
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
            out.bBottomField = itOut->bBottomField;

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
            std::sort(m_dpb.begin(), m_dpb.end(),   [](const Frame & lhs_frame, const Frame & rhs_frame)
                                                    {
                                                        return lhs_frame.poc < rhs_frame.poc;
                                                    });

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

                auto preferSamePolarity = [&](const Frame & lhs_frame, const Frame & rhs_frame)
                {
                   mfxU32 lhs_distance = std::abs(lhs_frame.poc/2 - out.poc/2) + ((lhs_frame.bBottomField == out.bBottomField) ? 0 : 2);
                   mfxU32 rhs_distance = std::abs(rhs_frame.poc/2 - out.poc/2) + ((rhs_frame.bBottomField == out.bBottomField) ? 0 : 2);

                   return lhs_distance <= rhs_distance;
                };

                auto distance = [&](const Frame & lhs_frame, const Frame & rhs_frame)
                {
                   mfxU32 lhs_distance = std::abs(lhs_frame.poc - out.poc);
                   mfxU32 rhs_distance = std::abs(rhs_frame.poc - out.poc);

                   return lhs_distance < rhs_distance;
                };

                // If lists are bigger than max supported, sort them and remove extra elements
                if (L0.size() > (isB ? CO3.NumRefActiveBL0[0] : CO3.NumRefActiveP[0]))
                {
                    if (isPPyramid)
                    {
                        // For P-pyramid we remove oldest references
                        // with the highest layer except the closest reference.

                        if (bIsFieldCoding)
                        {

                            std::sort(L0.begin(), L0.end(), [&](const Frame & lhs_frame, const Frame & rhs_frame)
                                                            {
                                                                return !preferSamePolarity(lhs_frame, rhs_frame);
                                                            });
                        }
                        else
                        {
                            std::sort(L0.begin(), L0.end(), [&](const Frame & lhs_frame, const Frame & rhs_frame)
                                                            {
                                                                return !distance(lhs_frame, rhs_frame);
                                                            });
                        }

                        while (L0.size() > CO3.NumRefActiveP[out.PLayer])
                        {
                            auto weak = L0.begin();
                            for (auto it = L0.begin(); it != L0.end(); it ++)
                            {
                                if (weak->PLayer < it->PLayer && it->poc != L0.back().poc)
                                {
                                    weak = it;
                                }
                            }

                            L0.erase(weak);
                        }
                    }
                }

                if (bIsFieldCoding)
                {
                    std::sort(L0.begin(), L0.end(), preferSamePolarity);
                    std::sort(L1.begin(), L1.end(), preferSamePolarity);
                }
                else
                {
                    std::sort(L0.begin(), L0.end(), distance);
                    std::sort(L1.begin(), L1.end(), distance);
                }

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
                    std::sort(L1.begin(), L1.end(), distance);
                }

                // Remove extra entries
                if (isB && L0.size() > CO3.NumRefActiveBL0[0])
                    L0.resize(CO3.NumRefActiveBL0[0]);
                if (!isB && L0.size() > CO3.NumRefActiveP[0])
                    L0.resize(CO3.NumRefActiveP[0]);

                if (L1.size() > CO3.NumRefActiveBL1[0])
                    L1.resize(CO3.NumRefActiveBL1[0]);

                std::sort(L0.begin(), L0.end(), distance);
                std::sort(L1.begin(), L1.end(), distance);
            }

            //=================5. Save current frame in DPB=====================
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

        enum
        {
            MFX_PAR = 1,
            EXT_COD2,
            EXT_COD3
        };

        typedef struct
        {
            struct
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
            mfxU32 nFrames;
        } tc_struct;

        static const tc_struct test_case[];

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
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
                ps->Data.FrameOrder = m_cur;
                ps->Info.PicStruct = 0;
                ExternalFrame f = ProcessFrame(*ps, m_cur < m_max ? false : true);

                ps->Data.FrameOrder = 0;

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
            EncodeEmulator::Init(m_par, m_max);
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

#define mfx_PicStruct   tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct
#define mfx_GopPicSize  tsStruct::mfxVideoParam.mfx.GopPicSize
#define mfx_GopRefDist  tsStruct::mfxVideoParam.mfx.GopRefDist
#define mfx_GopOptFlag  tsStruct::mfxVideoParam.mfx.GopOptFlag
#define mfx_IdrInterval tsStruct::mfxVideoParam.mfx.IdrInterval
#define mfx_PRefType    tsStruct::mfxExtCodingOption3.PRefType
#define mfx_BRefType    tsStruct::mfxExtCodingOption2.BRefType
#define mfx_GPB         tsStruct::mfxExtCodingOption3.GPB

    const TestSuite::tc_struct TestSuite::test_case[] =
    {

        {/*00*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  0 },
                 { MFX_PAR,  &mfx_GopRefDist,  0 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                300},
        {/*01*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  0 },
                 { MFX_PAR,  &mfx_GopRefDist,  0 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                600},
        {/*02*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  0 },
                 { MFX_PAR,  &mfx_GopRefDist,  0 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                600},
        {/*03*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  3 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                80},
        {/*04*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  3 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                163},
        {/*05*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  3 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                163},
        {/*06*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  4 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                80},
        {/*07*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  4 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*08*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  4 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*09*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                80},
        {/*10*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*11*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*12*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                80},
        {/*13*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*14*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*15*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_GopOptFlag,  MFX_GOP_CLOSED | MFX_GOP_STRICT },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                80},
        {/*16*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_GopOptFlag,  MFX_GOP_CLOSED | MFX_GOP_STRICT },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*17*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_GopOptFlag,  MFX_GOP_CLOSED | MFX_GOP_STRICT },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*18*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  1 },
                 { EXT_COD3, &mfx_PRefType,    MFX_P_REF_PYRAMID }},
                80},
        {/*19*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  1 },
                 { EXT_COD3, &mfx_PRefType,    MFX_P_REF_PYRAMID }},
                163},
        {/*20*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  1 },
                 { EXT_COD3, &mfx_PRefType,    MFX_P_REF_PYRAMID }},
                163},
        {/*21*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID },
                 { EXT_COD3, &mfx_GPB,         MFX_CODINGOPTION_OFF }},
                80},
        {/*22*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID },
                 { EXT_COD3, &mfx_GPB,         MFX_CODINGOPTION_OFF }},
                163},
        {/*23*/ {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  1 },
                 { EXT_COD3, &mfx_PRefType,    MFX_P_REF_PYRAMID }},
                163},
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        const tc_struct& tc = test_case[id];

        TS_START;
        CHECK_HEVC_FEI_SUPPORT();

        SETPARS(m_pPar, MFX_PAR);

        mfxExtCodingOption2& co2 = m_par;
        SETPARS(&co2, EXT_COD2);

        mfxExtCodingOption3& co3 = m_par;
        SETPARS(&co3, EXT_COD3);

        m_max = tc.nFrames;
        m_par.AsyncDepth = 1;

        Init();
        GetVideoParam();

        mfxExtFeiHevcEncFrameCtrl & ctrl = m_ctrl;
        ctrl.SubPelMode         = 3; // quarter-pixel motion estimation
        ctrl.SearchWindow       = 5; // 48 SUs 48x40 window full search
        ctrl.NumFramePartitions = 4; // number of partitions in frame that encoder processes concurrently
        ctrl.MultiPred[0] = ctrl.MultiPred[1] = 1; // enable internal L0/L1 predictors: 1 - spatial predictors

        EncodeFrames(tc.nFrames);
        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_fei_default_ref_lists);
}
