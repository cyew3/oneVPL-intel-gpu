/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

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