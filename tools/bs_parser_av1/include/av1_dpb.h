// Copyright (c) 2018-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <bs_reader.h>
#include <bs_mem.h>
#include <av1_struct.h>
#include <string>

namespace BS_AV1
{

enum MV_REFERENCE_FRAME
{
    NONE = -1,
    INTRA_FRAME = 0,
    LAST_FRAME = 1,
    LAST2_FRAME = 2,
    LAST3_FRAME = 3,
    GOLDEN_FRAME = 4,
    BWDREF_FRAME = 5,
    ALTREF2_FRAME = 6,
    ALTREF_FRAME = 7,
    MAX_REF_FRAMES = 8
};

class DPB : public BS_Allocator
{
private:
    Frame*& m_cur;
    Frame* m_ref[NUM_REF_FRAMES];
    Frame* m_active[MAX_REF_FRAMES];
    Bs32u m_frame_order;

public:
    DPB()
        : m_cur(m_active[INTRA_FRAME])
        , m_frame_order(0)
    {
        memset(m_ref, 0, sizeof(m_ref));
        memset(m_active, 0, sizeof(m_active));
    }

    Frame* NewFrame()
    {
        free(m_cur);
        memset(m_active, 0, sizeof(m_active));

        alloc((void*&)m_cur, sizeof(Frame), 0, 0, 0);
        if (m_cur)
        {
            m_cur->FrameOrder = m_frame_order++;
#if BS_ALLOC_DEBUG
            set_name(m_cur, (std::string("Frame #") + std::to_string(m_cur->FrameOrder)).c_str(), __FILE__, __LINE__);
#endif
        }

        return m_cur;
    }

    void UpdateActiveRefs()
    {
    }

    void UpdateDPB()
    {
    }

    void PrintDPB()
    {
#ifdef __BS_TRACE__
        bool trace_flag = true;
        BS_TRACE_ARR(DPB, m_ref[_idx] ? m_ref[_idx]->FrameOrder : NONE, NUM_REF_FRAMES);
#endif
    }

    inline Frame* Ref(Bs8u idx) { return m_active[idx & 3]; }
};
}
