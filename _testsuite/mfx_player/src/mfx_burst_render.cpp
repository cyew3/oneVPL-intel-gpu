/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_burst_render.h"
#include "umc_automatic_mutex.h"

BurstRender::BurstRender(bool bVerbose, mfxU16 nBurstLen, ITime * pTime, IMFXVideoRender * pDecorated)
    : InterfaceProxy<IMFXVideoRender>(pDecorated)
    , m_pTime(pTime)
    , m_AsyncStatus(MFX_ERR_NONE)
    , m_state()
    , m_bStop()
    , m_nBurstLen(nBurstLen)
    , m_bVerbose(bVerbose)
    , m_nDecodeResume(5)//todo: configure
{
    vm_thread_set_invalid(&m_Thread);
    vm_thread_create(&m_Thread, &BurstRender::ThreadRoutine, this);

    vm_mutex_set_invalid(&m_FramesAcess);
    vm_mutex_init(&m_FramesAcess);

    vm_event_set_invalid(&m_shouldStartDecode);
    vm_event_init(&m_shouldStartDecode, 1, 0);
}

mfxU32  BurstRender::ThreadRoutine(void * lpVoid)
{
    reinterpret_cast<BurstRender*>(lpVoid)->RenderThread();
    return 0;
}

BurstRender::~BurstRender()
{
    CloseLocal();
}

mfxStatus BurstRender::CloseLocal()
{
    m_bStop = true;
    vm_thread_wait(&m_Thread);

    vm_mutex_destroy(&m_FramesAcess);
    vm_event_destroy(&m_shouldStartDecode);

    return MFX_ERR_NONE;
}

mfxStatus BurstRender::RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl)
{
    m_AsyncStatus = MFX_ERR_NONE;

    if (NULL != surface)
    {
        IncreaseReference(&surface->Data);
        {
            UMC::AutomaticMutex guard(m_FramesAcess);
            m_bufferedFrames.push_back(std::pair<mfxFrameSurface1*, mfxEncodeCtrl *>(surface, pCtrl));
        }

        if (m_bufferedFrames.size() >= (mfxU16)(m_nDecodeResume + m_nBurstLen))
        {
            MPA_TRACE("BurstRender::DecodeSuspend");

            vm_event_reset(&m_shouldStartDecode);
            {
                UMC::AutomaticMutex guard(m_FramesAcess);
                m_state = STATE_RENDERING_ONLY;
            }
            vm_event_wait(&m_shouldStartDecode);
        }
    }
    else
    {
        {
            UMC::AutomaticMutex guard(m_FramesAcess);
            m_bufferedFrames.push_back(std::make_pair<mfxFrameSurface1*, mfxEncodeCtrl *>(NULL, NULL));
        }

        for (;!m_bufferedFrames.empty();)
        {
            m_pTime->Wait(10);
            continue;
        }
    }
    
    return m_AsyncStatus;
}

mfxStatus BurstRender::GetHandle(mfxHandleType type, mfxHDL *pHdl)
{
    m_forGetHandleDispatch.type = type;
    m_forGetHandleDispatch.pHdl = pHdl;
    m_state = STATE_DISPATCH_GETHANDLE;

    while (m_state != STATE_IDLE)
    {
        m_pTime->Wait(10);
    }

    return m_AsyncStatus;
}

void BurstRender::RenderThread()
{
    while (!m_bStop || !m_bufferedFrames.empty())
    {
        switch(m_state)
        {
            case STATE_DISPATCH_GETHANDLE:
            {
                IHWDevice *pDevice;
                m_AsyncStatus = InterfaceProxy<IMFXVideoRender>::GetDevice(&pDevice);
                m_AsyncStatus = pDevice->GetHandle(m_forGetHandleDispatch.type, m_forGetHandleDispatch.pHdl);
                m_state = STATE_IDLE;
                break;
            }
            case STATE_IDLE:
            {
                /*if (m_bufferedFrames.size() >= (mfxU16)(m_nDecodeResume + m_nBurstLen) || m_bStop)
                {
                    vm_event_reset(&m_shouldStartDecode);
                    m_state = STATE_RENDERING_ONLY;
                }
                else*/
                {
                    MPA_TRACE("BurstRender::Idle");
                    m_pTime->Wait(10);
                }
                break;
            }
            // states differs only that in render only state there is an event sent out when 
            // rendering que became small enough
            case STATE_RENDERING_AND_DECODING :
            case STATE_RENDERING_ONLY : 
            {
                if (m_bufferedFrames.empty())
                {
                    MPA_TRACE("BurstRender::NoFramesToDisplay");
                    PipelineTrace((VM_STRING("WARNING: Rendere queue:empty, sleep(10) invoked\n")));
                    m_pTime->Wait(10);
                    continue;
                }

                if(m_bVerbose)
                {
                    PipelineTrace((VM_STRING("BurstRender::Rendering: buffer=%d\n"), m_bufferedFrames.size()));
                }

                MPA_TRACE("BurstRender::Rendering");

                m_AsyncStatus = InterfaceProxy<IMFXVideoRender>::RenderFrame( m_bufferedFrames.front().first
                                                               , m_bufferedFrames.front().second);

                if (NULL != m_bufferedFrames.front().first)
                {
                    DecreaseReference(&m_bufferedFrames.front().first->Data);
                }
                
                {
                    //other thread cannot dirty front pointer
                    UMC::AutomaticMutex guard(m_FramesAcess);
                    m_bufferedFrames.pop_front();
                }
                
            }
        }

        switch(m_state)
        {
            case STATE_RENDERING_ONLY :
            {
                if (m_bufferedFrames.size() < m_nDecodeResume)
                {
                    m_state = STATE_RENDERING_AND_DECODING;
                    vm_event_signal(&m_shouldStartDecode);
                }
                break;
            }
            
            default:
            {
                break;
            }
        }

    }
    //unlock all surfaces if any
    for (;!m_bufferedFrames.empty();)
    {
        if(m_bufferedFrames.front().first != NULL)
        {
            DecreaseReference(&m_bufferedFrames.front().first->Data);
        }
    }
}

mfxStatus BurstRender::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    mfxStatus sts = InterfaceProxy<IMFXVideoRender>::QueryIOSurf(par, request);

    if (sts >= MFX_ERR_NONE && NULL != request)
    {
        request->NumFrameMin += m_nBurstLen + m_nDecodeResume;
        request->NumFrameSuggested += m_nBurstLen + m_nDecodeResume;
    }

    return sts;
}

