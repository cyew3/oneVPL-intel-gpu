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

#ifndef __TEMPORAL_SCALABLITY_H__
#define __TEMPORAL_SCALABLITY_H__

#include "mfxdefs.h"
#include "mfxstructures.h"
#include <windows.h>
#include <assert.h>
#include <vfwmsgs.h>
#include "mf_guids.h"

#include "mf_mfx_params_interface.h"

const ULONG BASE_LAYER_NUMBER = 0;
const ULONG MAX_TEMPORAL_LAYERS_COUNT = 3;
const mfxU16 TEMPORAL_LAYERS_SCALE_RATIO = 2;

class TemporalScalablity : public MFParamsWorker
{
public:
    TemporalScalablity( MFParamsManager *pManager );
    virtual ~TemporalScalablity( void );

    //MFParamsWorker::
    virtual HRESULT HandleMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender);
    virtual bool IsEnabled() const { return m_nTemporalLayersCount > 0; }
    STDMETHOD(UpdateVideoParam)(mfxVideoParam &videoParams, MFExtBufCollection &arrExtBuf) const;
    //TODO: wrap GenerateTemporalLayersBuffer with:
    //STDMETHOD(UpdateEncodeCtrl)(mfxEncodeCtrl* &pEncodeCtrl, MFExtBufCollection &m_arrEncExtBuf) const;


    //Temporal Layers Count
    bool        IsTemporalLayersCountValid(ULONG nTemporalLayersCount) const;

    //doesn't change current TLC immediately, just remebers new value and generates new ext buffer for it
    HRESULT     SetNewTemporalLayersCount(ULONG nTemporalLayersCount);
    void        TemporalLayersCountChanged();

    //TODO: check if it is possible to hide ULONG GetTemporalLayersCount()
    inline ULONG       GetTemporalLayersCount() const { return m_nTemporalLayersCount; }

    HRESULT     GetTemporalLayersCount(ULONG &nTemporalLayersCount) const;

    //Mfx structures generators

    // allocates mfxEncodeCtrl, making the caller responsible for freeing
    // Increments frame number (m_nFrameNum)
    mfxEncodeCtrl*              GenerateFrameEncodeCtrl();


    mfxU16      GetMinNumRefFrame() const;

    //QP per layer
    HRESULT     SelectLayer(ULONG nLayerNumber);
    HRESULT     GetSelectedLayerNumber(ULONG &nLayerNumber) const;
    HRESULT     SetLayerEncodeQP(ULONGLONG nQP);
    HRESULT     GetLayerEncodeQP(ULONGLONG &nQP) const;

    //Close-Init, Key frames
    void        ResetLayersSequence();
    HRESULT     PostponeDCCBeforeBaseLayer()
    {
        m_bPostponeDCCBeforeBaseLayer = true;
        return TryRequireDCC();
    }

    HRESULT TryRequireDCC()
    {
        HRESULT hr = S_OK;
        if (m_bPostponeDCCBeforeBaseLayer)
        {
            bool bStartNewPattern = false;
            if (m_bInfiniteGop)
            {
                //Check if next frame is base layer
                LONG nNextFrameLayerNum = GetTemporalLayerNumber();
                if (0 == nNextFrameLayerNum)
                    bStartNewPattern = true;
            }
            else
            {
                mfxU16 nDivisor = 1 << (MAX_TEMPORAL_LAYERS_COUNT - 1);
                if (0 == m_nFrameNum % nDivisor)
                {
                    MFX_LTRACE_I(MF_TL_GENERAL, m_nFrameNum % nDivisor);
                    bStartNewPattern = true;
                }
            }
            if (bStartNewPattern)
            {
                if (NULL != m_pManager)
                {
                    // notify everyone about next frame is base layer on
                    // in case if somebody wants to perform specific actions before that
                    m_bPostponeDCCBeforeBaseLayer = false;
                    TemporalLayersCountChanged();
                    hr = m_pManager->HandleNewMessage(mtRequireDCC, this);
                    ATLASSERT(S_OK == hr);
                }
                else
                {
                    ATLASSERT(NULL != m_pManager);
                    hr = E_POINTER;
                }
            }
        }
        return hr;
    }

    HRESULT     ForceKeyFrameForBaseLayer() { m_bForceKeyFrameForBaseLayer = true; return S_OK; }
    bool        IsForceKeyFrameRequested() const { return m_bForceKeyFrameForBaseLayer; }

    void        SetGopSizeMode(bool bInfiniteGop)
    {
        m_bInfiniteGop = bInfiniteGop;
    }

    //Trace function
    static void TraceEncodeCtrlStatic(const mfxEncodeCtrl *pEncodeCtrl);

private:
    ULONGLONG   m_nFrameNum;
    ULONG       m_nTemporalLayersCount;
    mfxU16      m_arrLayersMfxQP[MAX_TEMPORAL_LAYERS_COUNT]; // MSFT meaning: 0...100 (Min...Max)
    ULONG       m_nSelectedLayer;
    bool        m_bForceKeyFrameForBaseLayer;
    bool        m_bPostponeDCCBeforeBaseLayer;
    ULONG       m_nNewTemporalLayersCount;
    bool        m_bInfiniteGop;

    mutable mfxExtAvcTemporalLayers m_extAvcTemporalLayers;

    static ULONG GetTemporalLayerNumber(ULONGLONG nFrameNum, ULONG nLayersCount);
    inline LONG GetTemporalLayerNumber() const { return GetTemporalLayerNumber( m_nFrameNum, m_nTemporalLayersCount ); }
    bool        IsTemporalLayerNumberValid(ULONG nTemporalLayerNumber) const;

    HRESULT SetEncodeQP(ULONG nTemporalLayerNumber, mfxU16 nPerFrameMfxQP);
    HRESULT GetEncodeQP(ULONG nTemporalLayerNumber, mfxU16 &nPerFrameMfxQP) const;

    // does not allocate mfxExtAvcTemporalLayers, but updates existing member
    void GenerateTemporalLayersBuffer(ULONG nTemporalLayersCount) const;
};

#endif //__TEMPORAL_SCALABLITY_H__
