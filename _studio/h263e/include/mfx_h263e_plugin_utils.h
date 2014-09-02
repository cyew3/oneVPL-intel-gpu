/**********************************************************************************

Copyright (C) 2014 Intel Corporation.  All rights reserved.

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

File Name: h263e_plugin_utils.h

***********************************************************************************/

#pragma once

#include <stdio.h>
#include <memory>
#include <vector>
#include "mfxvideo.h"
#include "mfxvideo++int.h"
#include "mfxplugin++.h"
#include "mfx_utils.h"
#include <umc_mutex.h>

namespace MFX_H263ENC
{

#define H263_DEF_TASKS_NUM 2
  
    enum eTaskStatus
    {
        TASK_FREE = 0,
        TASK_OCCUPIED
    };

    enum
    {
        MFX_MEMTYPE_D3D_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME,
        MFX_MEMTYPE_D3D_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME,
        MFX_MEMTYPE_SYS_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY        | MFX_MEMTYPE_EXTERNAL_FRAME,
        MFX_MEMTYPE_SYS_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_SYSTEM_MEMORY        | MFX_MEMTYPE_INTERNAL_FRAME
    };

    struct Task
    {
        eTaskStatus m_status;

        mfxU32 m_frame_order;
        mfxFrameSurface1* m_surface;
        mfxBitstream* m_bitstream;
        mfxCoreInterface *m_core;

        Task():
            m_status(TASK_FREE),
            m_frame_order(0),
            m_surface(NULL),
            m_bitstream(NULL),
            m_core(NULL)
        {
        }
        ~Task()
        {
            FreeTask();
        }

        inline mfxStatus Init(mfxCoreInterface *core)
        {
            m_core = core;

            return MFX_ERR_NONE;
        }

        inline mfxStatus InitTask(
            mfxFrameSurface1* surface,
            mfxBitstream *bitstream,
            mfxU32 frame_order)
        {
            mfxStatus sts = MFX_ERR_NONE;

            m_surface = surface;
            m_bitstream = bitstream;
            //m_timeStamp = surface->Data.TimeStamp;
            m_frame_order = frame_order;

            if (m_surface)
            {
                sts = m_core->IncreaseReference(m_core->pthis, &m_surface->Data);
            }

            m_status = TASK_OCCUPIED;

            return sts;
        }

        inline mfxStatus FreeTask()
        {
            mfxStatus sts = MFX_ERR_NONE;

            if (m_surface) {
                sts = m_core->DecreaseReference(m_core->pthis, &m_surface->Data);
                m_surface = NULL;
            }
            if (m_bitstream) {
                m_bitstream = NULL;
            }
            m_frame_order = 0;
            m_status = TASK_FREE;

            return sts;
        }

        inline bool isFreeTask()
        {
            return (m_status == TASK_FREE);
        }
    };

    template <class TTask>
    inline TTask* GetFreeTask(std::vector<TTask> & tasks)
    {
        typename std::vector<TTask>::iterator task = tasks.begin();
        for (;task != tasks.end(); task ++)
        {
            if (task->isFreeTask())
            {
                return &task[0];
            }
        }
        return NULL;
    }

    template <class TTask>
    inline mfxStatus FreeTasks(std::vector<TTask> & tasks)
    {
        mfxStatus sts = MFX_ERR_NONE;
        typename std::vector<TTask>::iterator task = tasks.begin();
        for (;task != tasks.end(); task ++)
        {
            if (!task->isFreeTask())
            {
                sts = task->FreeTask();
                MFX_CHECK_STS(sts);
            }
        }
        return sts;
    }

    class TaskManager
    {
    protected:
        mfxCoreInterface* m_pCore;
        mfxVideoParam m_video;

        std::vector<Task> m_Tasks;
        mfxU32 m_frameNum;

    public:
        TaskManager():
            m_pCore(NULL),
            m_frameNum(0)
        {
        }

        ~TaskManager()
        {
            FreeTasks(m_Tasks);
        }

        mfxStatus Init(mfxCoreInterface* pCore, mfxVideoParam *par)
        {
            mfxStatus sts = MFX_ERR_NONE;

            MFX_CHECK(!m_pCore, MFX_ERR_UNDEFINED_BEHAVIOR);

            m_pCore = pCore;
            m_video = *par;

            m_frameNum = 0;

            m_Tasks.resize(H263_DEF_TASKS_NUM);
            for (mfxU32 i = 0; i < H263_DEF_TASKS_NUM; i++)
            {
                sts = m_Tasks[i].Init(m_pCore);
                MFX_CHECK_STS(sts);
            }

            return MFX_ERR_NONE;
        }

        mfxStatus Reset(mfxVideoParam* /*par*/)
        {
            return MFX_ERR_NONE;
        }

        mfxStatus InitTask(mfxFrameSurface1* pSurface, mfxBitstream* pBitstream, Task* & pOutTask)
        {
            mfxStatus sts = MFX_ERR_NONE;
            Task* pTask = GetFreeTask(m_Tasks);

            MFX_CHECK(m_pCore!=NULL, MFX_ERR_NOT_INITIALIZED);
            MFX_CHECK(pTask!=NULL, MFX_WRN_DEVICE_BUSY);

            sts = pTask->InitTask(pSurface, pBitstream, m_frameNum);
            MFX_CHECK_STS(sts);

            pOutTask = pTask;

            ++m_frameNum;

            return sts;
        }
    };

} // namespace MFX_H263ENC
