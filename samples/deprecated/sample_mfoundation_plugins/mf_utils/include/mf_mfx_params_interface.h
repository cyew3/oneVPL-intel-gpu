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

#include "sample_utils.h"
#include <list>
#include <algorithm>

#include "mf_guids.h"
#include "mf_utils.h"
#include "sample_utils.h"

class MFExtBufCollection: public std::vector<mfxExtBuffer*>
{
public:
    virtual ~MFExtBufCollection()
    {
        Free();
    }

    mfxExtBuffer* FindById(mfxU32 bufferId)
    {
        return ::GetExtBuffer(_Myfirst, (mfxU32)size(), bufferId);
    }

    template <class T>
    T* Find()
    {
        return (T*)FindById(mfx_ext_buffer_id<T>::id);
    }

    template <class T>
    T* Create()
    {
        T *pResult = NULL;
        SAFE_NEW(pResult, T);
        if (NULL != pResult)
        {
            init_ext_buffer(*pResult);
            try
            {
                push_back((mfxExtBuffer*)pResult);
            }
            catch(...)
            {
                SAFE_DELETE(pResult);
                ATLASSERT(!"Cannot extend ExtBuf collection");
            }
        }
        return pResult;
    }


    template <class T>
    T* FindOrCreate()
    {
        T *pResult = Find<T>();
        if (NULL == pResult)
        {
            pResult = Create<T>();
        }
        return pResult;
    }

    // checks whether mfxVideoParam points to same array of extParam
    inline bool IsExtParamUsed(const mfxVideoParam &videoParams) const
    {
        bool bResult = size() == videoParams.NumExtParam;
        ATLASSERT(size() == videoParams.NumExtParam);
        if (0 != videoParams.NumExtParam || NULL != videoParams.ExtParam)
        {
            bResult = bResult && _Myfirst == videoParams.ExtParam;
            ATLASSERT(_Myfirst == videoParams.ExtParam);
        }
        return bResult;
    }

    //ignores existing mfxVideoParam extParam and replaces it with new one
    inline void UpdateExtParam(mfxVideoParam &videoParams)
    {
        videoParams.NumExtParam = (mfxU16)(size());
        videoParams.ExtParam = _Myfirst;
    }

    void Free()
    {
        while(!empty())
        {
            SAFE_DELETE(back());
            pop_back();
        }

    }
};

enum MFParamsManagerMessageType
{
    mtUnknown,
    mtNotifyTemporalLayerNextIsBase,
    mtNotifyTemporalLayerIsBase,
    mtNotifyTemporalLayerIsMax,
    mtNotifyTemporalLayerIsNeitherBaseNorMax,
    mtNotifyBeforeIDR,
    mtRequireUpdateMaxMBperSec,
    /////mtRequireNotifyBeforeBaseLayer,
    mtRequireDCC
};

class MFParamsManagerMessage
{
public:
    virtual ~MFParamsManagerMessage() {};
    static MFParamsManagerMessage *Create(MFParamsManagerMessageType type)
    {
        MFParamsManagerMessage *pResult = NULL;
        SAFE_NEW(pResult, MFParamsManagerMessage(type));
        return pResult;
    }
    MFParamsManagerMessageType GetType() const { return m_Type; }
protected:
    MFParamsManagerMessageType m_Type;
    MFParamsManagerMessage(MFParamsManagerMessageType type)
        : m_Type(type) {};
};

class MFParamsMessagePeer
{
public:
    MFParamsMessagePeer() {};
    virtual ~MFParamsMessagePeer() {};
    virtual HRESULT HandleMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender) PURE;
    virtual HRESULT HandleNewMessage(MFParamsManagerMessageType type, MFParamsMessagePeer *pSender)
    {
        std::auto_ptr<MFParamsManagerMessage> pMessage( MFParamsManagerMessage::Create(type) );
        CHECK_POINTER(pMessage.get(), E_OUTOFMEMORY);
        return HandleMessage(pMessage.get(), pSender);
    }
};

class MFParamsWorker;

class MFParamsManager : public MFParamsMessagePeer
{
public:
                    MFParamsManager() {};
    virtual         ~MFParamsManager() { Free(); }
            HRESULT AddWorker(MFParamsWorker *pWorker);
            void    RemoveWorker(MFParamsWorker *pWorker);
    virtual HRESULT HandleMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender);
    virtual bool    IsInitialized() const { return false; }

protected:
    std::list<MFParamsWorker*> m_arrWorkers;
    virtual HRESULT BroadcastMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender);
    STDMETHOD(UpdateVideoParam)(mfxVideoParam &videoParams, MFExtBufCollection &arrExtBuf) const;
            //HRESULT NotifyBeforeBaseLayer() { return E_NOTIMPL; }
            //HRESULT NotifyBeforeIDR() { return E_NOTIMPL; }
            //HRESULT CheckParams() { return E_NOTIMPL; }
            //HRESULT RequestDCC() { return E_NOTIMPL; }
    virtual void    Free();
};

//need ability to SendMessage: DCC, IDR
class MFParamsWorker : public MFParamsMessagePeer
{
public:
    MFParamsWorker(MFParamsManager *pManager) : m_pManager(NULL)
    {
        SetManager(pManager);
    }

    virtual ~MFParamsWorker()
    {
        SetManager(NULL);
    }

    HRESULT SetManager(MFParamsManager *pManager)
    {
        HRESULT hr = S_OK;
        if (m_pManager != pManager)
        {
            if (SUCCEEDED(hr) && NULL != pManager)
            {
                hr = pManager->AddWorker(this);
                ATLASSERT(SUCCEEDED(hr));
            }
            if (SUCCEEDED(hr))
            {
                if (NULL != m_pManager)
                    m_pManager->RemoveWorker(this);
                m_pManager = pManager;
            }
        }
        return hr;
    }

    virtual bool IsEnabled() const { return false; }

    //TODO:
    // add DECLSPEC_NOTHROW?

    // TryUpdate(save state) - checks params, notifies other workers
    // message Commit(forget saved state)
    // message Rollback(return to saved state) - notifies other workers

    // problem: conflict in case of params collision
    // worker1->TryUpdate, worker2->TryUpdate, worker1->Rollback, worker2->Commit



    // caller is reponsible for releasing all collected mfxExtBuffers
    // MFParamsWorker is allowed to modify any mfxExtBuffer only during this call but not after
    STDMETHOD(UpdateVideoParam)(mfxVideoParam &videoParams, MFExtBufCollection &arrExtBuf) const
    {
        HRESULT hr = E_NOTIMPL;
        if (!arrExtBuf.IsExtParamUsed(videoParams))
        {
            hr = E_INVALIDARG;
        }
        return hr;
    }
    // caller is reponsible for releasing all collected mfxExtBuffers
    // MFParamsWorker is allowed to modify any mfxExtBuffer only during this call but not after
    STDMETHOD(UpdateEncodeCtrl)(mfxEncodeCtrl* &pEncodeCtrl, MFExtBufCollection &m_arrEncExtBuf) const
    {
        UNREFERENCED_PARAMETER(pEncodeCtrl);
        UNREFERENCED_PARAMETER(m_arrEncExtBuf);
        return E_NOTIMPL;
    }

    STDMETHOD(SetParams)(mfxVideoParam *pattern, mfxVideoParam *params)
    {
        UNREFERENCED_PARAMETER(pattern);
        UNREFERENCED_PARAMETER(params);
        return E_NOTIMPL;
    }

    virtual HRESULT HandleMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender)
    {
        UNREFERENCED_PARAMETER(pMessage);
        UNREFERENCED_PARAMETER(pSender);
        return E_NOTIMPL;
    }

protected:
    MFParamsManager *m_pManager;

    // guarantees to return SUCCEEDED only if IsEnabled() and a buffer is successfully prepared
    // returns E_NOTIMPL if not enabled
    template <class T>
    HRESULT PrepareExtBuf(mfxVideoParam &videoParams, MFExtBufCollection &arrExtBuf, T* &pResult) const
    {
        HRESULT hr = MFParamsWorker::UpdateVideoParam(videoParams, arrExtBuf);
        if (E_NOTIMPL == hr)
            hr = S_OK;
        if (SUCCEEDED(hr) && !IsEnabled())
            hr = E_NOTIMPL;
        if (SUCCEEDED(hr))
        {
            T *pExtBuf = arrExtBuf.FindOrCreate<T>();
            if (NULL == pExtBuf)
            {
                hr = E_OUTOFMEMORY;
            }
            else
            {
                arrExtBuf.UpdateExtParam(videoParams); // size() may be incresed, data() may be reallocated
                pResult = pExtBuf;
            }
        }

        //debug paranoid checks, can be removed:
        if (SUCCEEDED(hr))
        {
            ATLASSERT(NULL != pResult);
            if (NULL == pResult)
                hr = E_FAIL;
        }

        return hr;
    }
};

/*------------------------------------------------------------------------------*/
