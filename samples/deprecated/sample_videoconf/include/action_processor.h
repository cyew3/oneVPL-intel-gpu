/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
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

#include "interface.h"
#include <map>
#include <algorithm>

class ActionProcessor : public IActionProcessor
{
public:
    ActionProcessor(){}

    ~ActionProcessor()
    {
        std::for_each(m_actions.begin(), m_actions.end(), actionsArrayDeleter);
    }

    virtual void RegisterAction(mfxU32 nFrame, IAction *pAction)
    {
        if (NULL == pAction)
            return;
        m_actions[nFrame].push_back(pAction);
    }

    virtual void GetActionsForFrame(mfxU32 nFrameOrder, std::vector<IAction*> & vec)
    {
        vec.clear();

        std::map<int, std::vector<IAction*> >::iterator  it = m_actions.find(nFrameOrder);
        if (it != m_actions.end())
        {
            m_comitted.insert(m_comitted.end(), it->second.begin(), it->second.end());
        }

        //returning already comitted actions + for curent frame
        vec.insert(vec.end(), m_comitted.begin(), m_comitted.end());
    }

    virtual void OnFrameEncoded(mfxBitstream * pBs)
    {
        //need to remove already comitted actions by specific condition
        m_pLastBs = pBs;
        m_comitted.erase(std::remove_if(m_comitted.begin(), m_comitted.end(), clean_commited(this)), m_comitted.end());
    }

protected:
    friend class clean_commited;
    class clean_commited
    {

        ActionProcessor * m_pProc;
    public:
        clean_commited(ActionProcessor *pProc)
            : m_pProc(pProc)
        {
        }
        bool operator ()(IAction *pAction)
        {
            return m_pProc->ShouldRemove(pAction);
        }
    };

    bool ShouldRemove(IAction *p)
    {
        if (p->IsPermanent())
            return false;

        if (p->IsRepeatUntillIDR() && (!m_pLastBs || !(m_pLastBs->FrameType & MFX_FRAMETYPE_I)))
            return false;

        return true;
    }


    static void actionsArrayDeleter(std::pair<int, std::vector<IAction *> > actionsArray)
    {
        for_each(actionsArray.second.begin(), actionsArray.second.end(), actionsDeleter);
    }
    static void actionsDeleter(IAction *p)
    {
        delete p;
    }

    std::map<int, std::vector<IAction*> > m_actions;
    std::vector<IAction*>                 m_comitted;
    mfxBitstream                         *m_pLastBs;
};