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

#pragma once

#include <assert.h>
#include <algorithm>
#include "mfxstructures.h"
#include "sample_hevc_fei_defs.h"

inline bool isField(MfxVideoParamsWrapper const & par)
{
    return !!(par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_SINGLE);
}

class SecondFieldInfo
{
public:
    mfxI32     m_poc;
    bool       m_bReference;
    mfxU32     m_level;

    SecondFieldInfo() :
        m_poc(-1),
        m_bReference(false),
        m_level(0) {}

    void Reset()
    {
        m_poc = -1;
        m_bReference = false;
        m_level = 0;
    }
    void SaveInfo(HevcTask const* task)
    {
        m_poc = task->m_poc + 1;
        m_bReference = ((task->m_frameType & MFX_FRAMETYPE_REF) != 0);
        m_level = task->m_level;
    }
    void CorrectTaskInfo(HevcTask* task)
    {
        if (m_poc != task->m_poc || !task->m_secondField)
            return;
        if (m_bReference)
            task->m_frameType |= MFX_FRAMETYPE_REF;
        task->m_level = m_level;

        Reset();
    }
    bool bSecondField() { return m_poc != -1; }
};

class EncodeOrderControl
{
public:
    EncodeOrderControl(const MfxVideoParamsWrapper & par, bool bIsPreENC) :
    m_par(par),
    m_lockRawRef(bIsPreENC),
    m_frameOrder(0),
    m_lastIDR(0)
    {
        Reset();
        // make m_lastTask invalid:
        Fill(static_cast<HevcDpbFrame & >(m_lastTask), IDX_INVALID);
        m_lastTask.m_frameType = IDX_INVALID;
        Fill(m_lastTask.m_refPicList, IDX_INVALID);
        Fill(m_lastTask.m_numRefActive, IDX_INVALID);
        m_lastTask.m_lastIPoc = IDX_INVALID;
        m_lastTask.m_lastRAP = IDX_INVALID;
        Fill(m_lastTask.m_dpb[TASK_DPB_ACTIVE], IDX_INVALID);
        Fill(m_lastTask.m_dpb[TASK_DPB_AFTER],  IDX_INVALID);
        Fill(m_lastTask.m_dpb[TASK_DPB_BEFORE], IDX_INVALID);
    }

    ~EncodeOrderControl()
    {
    }

    void  Reset()
    {
        m_free.resize(MaxTask(m_par));
        m_reordering.resize(0);
        m_encoding.resize(0);
        m_secondFieldInfo.Reset();
    }

    HevcTask* GetTask(mfxFrameSurface1 * surface)
    {
        HevcTask* task = ReorderFrame(surface);
        if (task)
        {
            ConstructRPL(*task, m_lastTask);
        }
        return task;
    }

    inline mfxU16 GetNumReorderFrames()
    {
        return MaxTask(m_par);
    }

    void ReleaseResources(HevcTask & task);

    void FreeTask(HevcTask* pTask)
    {
        if (pTask)
        {
            ReleaseResources(*pTask);
            TaskList::iterator it = std::find(m_encoding.begin(), m_encoding.end(), *pTask);
            if (it != m_encoding.end())
            {
                m_free.splice(m_free.end(), m_encoding, it);
            }
        }
    }

private:
    HevcTask* ReorderFrame(mfxFrameSurface1* in);
    void ConstructRPL(HevcTask & task, const HevcTask & prevTask);

    inline mfxU16 MaxTask(MfxVideoParamsWrapper const & par)
    {
        return std::max(1, par.AsyncDepth + par.mfx.GopRefDist - 1 + ((par.AsyncDepth > 1)? 1: 0));
    }

private:
    const MfxVideoParamsWrapper m_par;
    const bool m_lockRawRef;

    typedef std::list<HevcTask>   TaskList;
    TaskList                      m_free;
    TaskList                      m_reordering;
    TaskList                      m_encoding;

    HevcTask                      m_lastTask;

    mfxU32                        m_frameOrder;
    mfxU32                        m_lastIDR;

    SecondFieldInfo               m_secondFieldInfo;

private:
    DISALLOW_COPY_AND_ASSIGN(EncodeOrderControl);
};
