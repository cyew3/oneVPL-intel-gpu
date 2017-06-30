/******************************************************************************\
Copyright (c) 2017, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include <cstdlib>

#include "sample_defs.h"
#include "ref_list_manager.h"

// TODO: consider replacing it with  std::sort
#define MFX_SORT_COMMON(_AR, _SZ, _COND)\
    for (mfxU32 _i = 0; _i < (_SZ); _i ++)\
        for (mfxU32 _j = _i; _j < (_SZ); _j ++)\
            if (_COND) std::swap(_AR[_i], _AR[_j]);
#define MFX_SORT(_AR, _SZ, _OP) MFX_SORT_COMMON(_AR, _SZ, _AR[_i] _OP _AR[_j])
#define MFX_SORT_STRUCT(_AR, _SZ, _M, _OP) MFX_SORT_COMMON(_AR, _SZ, _AR[_i]._M _OP _AR[_j]._M)

namespace HevcRplUtils
{
    template<class T, class A> mfxStatus Insert(A& _to, mfxU32 _where, T const & _what)
    {
        MSDK_CHECK_NOT_EQUAL(_where + 1 < (sizeof(_to) / sizeof(_to[0])), true, MFX_ERR_UNDEFINED_BEHAVIOR);
        memmove(&_to[_where + 1], &_to[_where], sizeof(_to) - (_where + 1) * sizeof(_to[0]));
        _to[_where] = _what;
        return MFX_ERR_NONE;
    }

    template<class A> mfxStatus Remove(A& _from, mfxU32 _where, mfxU32 _num = 1)
    {
        const mfxU32 S0 = sizeof(_from[0]);
        const mfxU32 S = sizeof(_from);
        const mfxU32 N = S / S0;

        MSDK_CHECK_NOT_EQUAL(_where < N && _num <= (N - _where), true, MFX_ERR_UNDEFINED_BEHAVIOR);

        if (_where + _num < N)
            memmove(&_from[_where], &_from[_where + _num], S - ((_where + _num) * S0));

        memset(&_from[N - _num], IDX_INVALID, S0 * _num);

        return MFX_ERR_NONE;
    }

    inline bool isValid(HevcDpbFrame const & frame) { return IDX_INVALID != frame.m_idxRec; }
    inline bool isDpbEnd(HevcDpbArray const & dpb, mfxU32 idx) { return idx >= MAX_DPB_SIZE || !isValid(dpb[idx]); }

    mfxU32 GetEncodingOrder(mfxU32 displayOrder, mfxU32 begin, mfxU32 end, mfxU32 &level, mfxU32 before, bool & ref)
    {
        assert(displayOrder >= begin);
        assert(displayOrder <  end);

        ref = (end - begin > 1);

        mfxU32 pivot = (begin + end) / 2;
        if (displayOrder == pivot)
            return level + before;

        level ++;
        if (displayOrder < pivot)
            return GetEncodingOrder(displayOrder, begin, pivot,  level , before, ref);
        else
            return GetEncodingOrder(displayOrder, pivot + 1, end, level, before + pivot - begin, ref);
    }

    mfxU32 GetBiFrameLocation(mfxU32 i, mfxU32 num, bool &ref, mfxU32 &level)
    {
        ref  = false;
        level = 1;
        return GetEncodingOrder(i, 0, num, level, 0 ,ref);
    }

    template <class T> mfxU32 BPyrReorder(std::vector<T> brefs)
    {
        mfxU32 num = (mfxU32)brefs.size();
        if (brefs[0]->m_bpo == (mfxU32)MFX_FRAMEORDER_UNKNOWN)
        {
            bool bRef = false;

            for(mfxU32 i = 0; i < (mfxU32)brefs.size(); i++)
            {
                brefs[i]->m_bpo = GetBiFrameLocation(i,num, bRef, brefs[i]->m_level);
                if (bRef)
                    brefs[i]->m_frameType |= MFX_FRAMETYPE_REF;
            }
        }
        mfxU32 minBPO =(mfxU32)MFX_FRAMEORDER_UNKNOWN;
        mfxU32 ind = 0;
        for(mfxU32 i = 0; i < (mfxU32)brefs.size(); i++)
        {
            if (brefs[i]->m_bpo < minBPO)
            {
                ind = i;
                minBPO = brefs[i]->m_bpo;
            }
        }
        return ind;
    }

    mfxU32 CountL1(HevcDpbArray const & dpb, mfxI32 poc)
    {
        mfxU32 c = 0;
        for (mfxU32 i = 0; !isDpbEnd(dpb, i); i ++)
            c += dpb[i].m_poc > poc;
        return c;
    }

    mfxU8 GetDPBIdxByFO(HevcDpbArray const & DPB, mfxU32 fo)
    {
        for (mfxU8 i = 0; !isDpbEnd(DPB, i); i++)
        if (DPB[i].m_fo == fo)
            return i;
        return mfxU8(MAX_DPB_SIZE);
    }

    template<class T> T FindFrameToEncode(
        mfxVideoParam const & par,
        HevcDpbArray const & dpb,
        T begin,
        T end,
        bool flush)
    {
        T top  = begin;
        T b0 = end; // 1st non-ref B with L1 > 0
        std::vector<T> brefs;

        bool isBPyramid = false; // FIXME

        while ( top != end && (top->m_frameType & MFX_FRAMETYPE_B))
        {
            if (CountL1(dpb, top->m_poc))
            {
                if (isBPyramid)
                    brefs.push_back(top);
                else if (top->m_frameType & MFX_FRAMETYPE_REF)
                {
                    if (b0 == end || (top->m_poc - b0->m_poc < 2))
                        return top;
                }
                else if (b0 == end)
                    b0 = top;
            }
            top ++;
        }

        if (!brefs.empty())
        {
            return brefs[BPyrReorder(brefs)];
        }

        if (b0 != end)
            return b0;

        if (flush && top == end && begin != end)
        {
            top --;
            top->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        }

        return top;
    }

    void InitDPB(
        HevcTask &        task,
        HevcTask const &  prevTask,
        mfxExtAVCRefListCtrl * pLCtrl)
    {
        if (task.m_poc > task.m_lastIPoc
            && prevTask.m_poc <= prevTask.m_lastIPoc) // 1st TRAIL
        {
            Fill(task.m_dpb[TASK_DPB_ACTIVE], IDX_INVALID);

            for (mfxU8 i = 0, j = 0; !isDpbEnd(prevTask.m_dpb[TASK_DPB_AFTER], i); i++)
            {
                const HevcDpbFrame& ref = prevTask.m_dpb[TASK_DPB_AFTER][i];

                if (ref.m_poc == task.m_lastIPoc || ref.m_ltr)
                    task.m_dpb[TASK_DPB_ACTIVE][j++] = ref;
            }
        }
        else
        {
            Copy(task.m_dpb[TASK_DPB_ACTIVE], prevTask.m_dpb[TASK_DPB_AFTER]);
        }

        {
            HevcDpbArray& dpb = task.m_dpb[TASK_DPB_ACTIVE];

            for (mfxI16 i = 0; !isDpbEnd(dpb, i); i++)
                if (dpb[i].m_tid > 0 && dpb[i].m_tid >= task.m_tid)
                    Remove(dpb, i--);

            if (pLCtrl)
            {
                for (mfxU16 i = 0; i < 16 && pLCtrl->RejectedRefList[i].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN); i++)
                {
                    mfxU16 idx = GetDPBIdxByFO(dpb, pLCtrl->RejectedRefList[i].FrameOrder);

                    if (idx < MAX_DPB_SIZE && dpb[idx].m_ltr)
                        Remove(dpb, idx);
                }
            }
        }
    }

    void UpdateDPB(
        mfxVideoParam const & par,
        HevcDpbFrame const & task,
        HevcDpbArray & dpb,
        mfxExtAVCRefListCtrl * pLCtrl)
    {
        mfxU16 end = 0; // DPB end
        mfxU16 st0 = 0; // first ST ref in DPB

        while (!isDpbEnd(dpb, end)) end ++;
        for (st0 = 0; st0 < end && dpb[st0].m_ltr; st0++);

        // frames stored in DPB in POC ascending order,
        // LTRs before STRs (use LTR-candidate as STR as long as it possible)

        MFX_SORT_STRUCT(dpb, st0, m_poc, > );
        MFX_SORT_STRUCT((dpb + st0), mfxU16(end - st0), m_poc, >);

        // sliding window over STRs
        if (end && end == par.mfx.NumRefFrame)
        {
            {
                for (st0 = 0; dpb[st0].m_ltr && st0 < end; st0 ++);
            }

            Remove(dpb, st0 == end ? 0 : st0);
            end --;
        }

        if (end < MAX_DPB_SIZE)
            dpb[end++] = task;
        else
            assert(!"DPB overflow, no space for new frame");

        if (pLCtrl)
        {
            bool sort = false;

            for (st0 = 0; dpb[st0].m_ltr && st0 < end; st0++);

            for (mfxU16 i = 0; i < 16 && pLCtrl->LongTermRefList[i].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN); i++)
            {
                mfxU16 idx = GetDPBIdxByFO(dpb, pLCtrl->LongTermRefList[i].FrameOrder);

                if (idx < MAX_DPB_SIZE && !dpb[idx].m_ltr)
                {
                    HevcDpbFrame ltr = dpb[idx];
                    ltr.m_ltr = true;
                    Remove(dpb, idx);
                    Insert(dpb, st0, ltr);
                    st0++;
                    sort = true;
                }
            }

            if (sort)
            {
                MFX_SORT_STRUCT(dpb, st0, m_poc, >);
            }
        }
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

        if (idrPicDist && frameOrder % idrPicDist == 0)
            return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

        if (!idrPicDist && frameOrder == 0)
            return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

        if (frameOrder % gopPicSize == 0)
            return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF);

        if (frameOrder % gopPicSize % gopRefDist == 0)
            return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);

        if (((frameOrder + 1) % gopPicSize == 0 && (gopOptFlag & MFX_GOP_CLOSED)) ||
            (idrPicDist && (frameOrder + 1) % idrPicDist == 0))
            return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF); // switch last B frame to P frame


        return (MFX_FRAMETYPE_B);
    }

    bool isLTR(
        HevcDpbArray const & dpb,
        mfxU32 LTRInterval,
        mfxI32 poc)
    {
        mfxI32 LTRCandidate = dpb[0].m_poc;

        for (mfxU16 i = 1; !isDpbEnd(dpb, i); i++)
        {
            if (dpb[i].m_poc > LTRCandidate
                && dpb[i].m_poc - LTRCandidate >= (mfxI32)LTRInterval)
            {
                LTRCandidate = dpb[i].m_poc;
                break;
            }
        }

        return (poc == LTRCandidate) || (LTRCandidate == 0 && poc >= (mfxI32)LTRInterval);
    }

    void ConstructRPL(
        HevcDpbArray const & DPB,
        bool isB,
        mfxI32 poc,
        mfxU8  tid,
        mfxU8(&RPL)[2][MAX_DPB_SIZE],
        mfxU8(&numRefActive)[2],
        mfxExtAVCRefLists * pExtLists,
        mfxExtAVCRefListCtrl * pLCtrl)
    {
        mfxU8 NumRefLX[2] = { numRefActive[0], numRefActive[1] };
        mfxU8& l0 = numRefActive[0];
        mfxU8& l1 = numRefActive[1];
        mfxU8 LTR[MAX_DPB_SIZE] = {};
        mfxU8 nLTR = 0;
        mfxU8 NumStRefL0 = (mfxU8)(NumRefLX[0]);

        // was with par
        mfxU32 par_LTRInterval = 0;
        mfxU16 par_NumRefLX[2] = { 1, 1 };
        bool par_isLowDelay/*()*/ = 0;

        l0 = l1 = 0;

        if (pExtLists)
        {
            for (mfxU32 i = 0; i < pExtLists->NumRefIdxL0Active; i++)
            {
                mfxU8 idx = GetDPBIdxByFO(DPB, (mfxI32)pExtLists->RefPicList0[i].FrameOrder);

                if (idx < MAX_DPB_SIZE)
                {
                    RPL[0][l0++] = idx;

                    if (l0 == NumRefLX[0])
                        break;
                }
            }

            for (mfxU32 i = 0; i < pExtLists->NumRefIdxL1Active; i++)
            {
                mfxU8 idx = GetDPBIdxByFO(DPB, (mfxI32)pExtLists->RefPicList1[i].FrameOrder);

                if (idx < MAX_DPB_SIZE)
                    RPL[1][l1++] = idx;

                if (l1 == NumRefLX[1])
                    break;
            }
        }

        if (l0 == 0)
        {
            l1 = 0;

            for (mfxU8 i = 0; !isDpbEnd(DPB, i); i++)
            {
                if (DPB[i].m_tid > tid)
                    continue;

                if (poc > DPB[i].m_poc)
                {
                    if (DPB[i].m_ltr || (par_LTRInterval && isLTR(DPB, par_LTRInterval, DPB[i].m_poc)))
                        LTR[nLTR++] = i;
                    else
                        RPL[0][l0++] = i;
                }
                else if (isB)
                    RPL[1][l1++] = i;
            }

            if (pLCtrl)
            {
                // reorder STRs to POC descending order
                for (mfxU8 lx = 0; lx < 2; lx++)
                    MFX_SORT_COMMON(RPL[lx], numRefActive[lx],
                        std::abs(DPB[RPL[lx][_i]].m_poc - poc) > std::abs(DPB[RPL[lx][_j]].m_poc - poc));

                while (nLTR)
                    RPL[0][l0++] = LTR[--nLTR];
            }
            else
            {
                NumStRefL0 -= !!nLTR;

                if (l0 > NumStRefL0)
                {
                    MFX_SORT_COMMON(RPL[0], numRefActive[0], std::abs(DPB[RPL[0][_i]].m_poc - poc) < std::abs(DPB[RPL[0][_j]].m_poc - poc));
                    if (par_isLowDelay/*()*/)
                    {
                        while (l0 > NumStRefL0)
                        {
                            mfxI32 i;

                            // !!! par_NumRefLX[0] used here as distance between "strong" STR, not NumRefActive for current frame
                            for (i = 0; (i < l0) && (((DPB[RPL[0][0]].m_poc - DPB[RPL[0][i]].m_poc) % par_NumRefLX[0]) == 0); i++);

                            Remove(RPL[0], (i >= l0 - 1) ? 0 : i);
                            l0--;
                        }
                    }
                    else
                    {
                        Remove(RPL[0], (par_LTRInterval && !nLTR && l0 > 1), l0 - NumStRefL0);
                        l0 = NumStRefL0;
                    }
                }
                if (l1 > NumRefLX[1])
                {
                    MFX_SORT_COMMON(RPL[1], numRefActive[1], std::abs(DPB[RPL[1][_i]].m_poc - poc) > std::abs(DPB[RPL[1][_j]].m_poc - poc));
                    Remove(RPL[1], NumRefLX[1], l1 - NumRefLX[1]);
                    l1 = (mfxU8)NumRefLX[1];
                }

                // reorder STRs to POC descending order
                for (mfxU8 lx = 0; lx < 2; lx++)
                    MFX_SORT_COMMON(RPL[lx], numRefActive[lx],
                        DPB[RPL[lx][_i]].m_poc < DPB[RPL[lx][_j]].m_poc);

                if (nLTR)
                {
                    MFX_SORT(LTR, nLTR, <);
                    // use LTR as 2nd reference
                    Insert(RPL[0], !!l0, LTR[0]);
                    l0++;

                    for (mfxU16 i = 1; i < nLTR && l0 < NumRefLX[0]; i++, l0++)
                        Insert(RPL[0], l0, LTR[i]);
                }
            }
        }

        assert(l0 > 0);

        if (pLCtrl)
        {
            mfxU16 MaxRef[2] = { NumRefLX[0], NumRefLX[1] };
            mfxU16 pref[2] = {};

            if (pLCtrl->NumRefIdxL0Active)
                MaxRef[0] = std::min(pLCtrl->NumRefIdxL0Active, MaxRef[0]);

            if (pLCtrl->NumRefIdxL1Active)
                MaxRef[1] = std::min(pLCtrl->NumRefIdxL1Active, MaxRef[1]);

            for (mfxU16 i = 0; i < 16 && pLCtrl->RejectedRefList[i].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN); i++)
            {
                mfxU8 idx = GetDPBIdxByFO(DPB, pLCtrl->RejectedRefList[i].FrameOrder);

                if (idx < MAX_DPB_SIZE)
                {
                    for (mfxU16 lx = 0; lx < 2; lx++)
                    {
                        for (mfxU16 j = 0; j < numRefActive[lx]; j++)
                        {
                            if (RPL[lx][j] == idx)
                            {
                                Remove(RPL[lx], j);
                                numRefActive[lx]--;
                                break;
                            }
                        }
                    }
                }
            }

            for (mfxU16 i = 0; i < 16 && pLCtrl->PreferredRefList[i].FrameOrder != static_cast<mfxU32>(MFX_FRAMEORDER_UNKNOWN); i++)
            {
                mfxU8 idx = GetDPBIdxByFO(DPB, pLCtrl->PreferredRefList[i].FrameOrder);

                if (idx < MAX_DPB_SIZE)
                {
                    for (mfxU16 lx = 0; lx < 2; lx++)
                    {
                        for (mfxU16 j = 0; j < numRefActive[lx]; j++)
                        {
                            if (RPL[lx][j] == idx)
                            {
                                Remove(RPL[lx], j);
                                Insert(RPL[lx], pref[lx]++, idx);
                                break;
                            }
                        }
                    }
                }
            }

            for (mfxU16 lx = 0; lx < 2; lx++)
            {
                if (numRefActive[lx] > MaxRef[lx])
                {
                    Remove(RPL[lx], MaxRef[lx], (numRefActive[lx] - MaxRef[lx]));
                    numRefActive[lx] = (mfxU8)MaxRef[lx];
                }
            }

            if (l0 == 0)
                RPL[0][l0++] = 0;
        }

        assert(l0 > 0);

        if (isB && !l1 && l0)
            RPL[1][l1++] = RPL[0][l0 - 1];

        if (!isB)
        {
            l1 = 0; //ignore l1 != l0 in pExtLists for LDB (unsupported by HW)

            for (mfxU16 i = 0; i < std::min<mfxU16>(l0, NumRefLX[1]); i++)
                RPL[1][l1++] = RPL[0][i];
        }
    }
}