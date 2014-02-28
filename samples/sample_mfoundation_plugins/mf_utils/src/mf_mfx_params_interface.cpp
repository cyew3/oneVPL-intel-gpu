/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mf_mfx_params_interface.h"

HRESULT MFParamsManager::AddWorker(MFParamsWorker *pWorker)
{
    HRESULT hr = S_OK;
    try
    {
        m_arrWorkers.push_back(pWorker);
    }
    catch (...)
    {
        hr = E_OUTOFMEMORY;
        ATLASSERT(SUCCEEDED(hr));
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

void MFParamsManager::RemoveWorker(MFParamsWorker *pWorker)
{
    ATLASSERT(! m_arrWorkers.empty());
    if (! m_arrWorkers.empty())
        m_arrWorkers.remove(pWorker);
}

/*------------------------------------------------------------------------------*/

HRESULT MFParamsManager::HandleMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender)
{
    return BroadcastMessage(pMessage, pSender);
}

/*------------------------------------------------------------------------------*/

HRESULT MFParamsManager::BroadcastMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender)
{
    HRESULT hr = S_OK;
    for (std::list<MFParamsWorker*>::iterator i = m_arrWorkers.begin(); i != m_arrWorkers.end(); i++)
    {
        ATLASSERT(NULL != *i);
        MFParamsWorker *pWorker = dynamic_cast<MFParamsWorker*>(pSender);
        if (*i != pWorker && NULL != *i) // notify all other workers, skip originator
        {
            hr = (*i)->HandleMessage(pMessage, pSender);
            if (E_NOTIMPL == hr)
                hr = S_OK;
        }
    }
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFParamsManager::UpdateVideoParam(mfxVideoParam &videoParams, MFExtBufCollection &arrExtBuf) const
{
    HRESULT hr = S_OK;
    // let all workers update params
    for (std::list<MFParamsWorker*>::const_iterator i = m_arrWorkers.begin(); i != m_arrWorkers.end(); i++)
    {
        ATLASSERT(NULL != *i);
        if (NULL != *i) 
        {
            hr = (*i)->UpdateVideoParam(videoParams, arrExtBuf);
            if (FAILED(hr) && E_NOTIMPL != hr)
                break;
        }
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

void MFParamsManager::Free()
{
    while (!m_arrWorkers.empty())
    {
        MFParamsWorker *pWorker = m_arrWorkers.front();
        if (NULL != pWorker)
            pWorker->SetManager(NULL); // removes from m_arrWorkers
    }
}

/*------------------------------------------------------------------------------*/


