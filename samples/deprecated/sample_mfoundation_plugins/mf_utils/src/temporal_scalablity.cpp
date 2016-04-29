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

#include "temporal_scalablity.h"

/*------------------------------------------------------------------------------*/

TemporalScalablity::TemporalScalablity( MFParamsManager *pManager ) :
    MFParamsWorker(pManager),
    m_nFrameNum(0),
    m_nTemporalLayersCount(0),
    m_nSelectedLayer(0),
    m_bForceKeyFrameForBaseLayer(false),
    m_bPostponeDCCBeforeBaseLayer(false),
    m_nNewTemporalLayersCount(0),
    m_bInfiniteGop(false)
{
    memset( &m_arrLayersMfxQP, 0, sizeof(m_arrLayersMfxQP) );
}

/*------------------------------------------------------------------------------*/

TemporalScalablity::~TemporalScalablity(void)
{
}

/*------------------------------------------------------------------------------*/

bool TemporalScalablity::IsTemporalLayersCountValid(ULONG nTemporalLayersCount) const
{
    return nTemporalLayersCount > 0 && nTemporalLayersCount <= MAX_TEMPORAL_LAYERS_COUNT;
}

/*------------------------------------------------------------------------------*/

HRESULT TemporalScalablity::UpdateVideoParam(mfxVideoParam &videoParams, MFExtBufCollection &arrExtBuf) const
{
    mfxExtAvcTemporalLayers *pAvcTemporalLayers = NULL;
    HRESULT hr = PrepareExtBuf(videoParams, arrExtBuf, pAvcTemporalLayers);
    if (SUCCEEDED(hr))
    {
        ATLASSERT(NULL != pAvcTemporalLayers); // is guaranteed by return value
        memcpy_s(pAvcTemporalLayers, sizeof(*pAvcTemporalLayers),
                    &m_extAvcTemporalLayers, sizeof(m_extAvcTemporalLayers));
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT TemporalScalablity::SetNewTemporalLayersCount(ULONG nTemporalLayersCount)
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    MFX_LTRACE_I(MF_TL_GENERAL, nTemporalLayersCount);
    HRESULT hr = E_INVALIDARG;
    if ( IsTemporalLayersCountValid( nTemporalLayersCount ) || 0 == nTemporalLayersCount)
    {
        m_nNewTemporalLayersCount = nTemporalLayersCount;
        GenerateTemporalLayersBuffer(m_nNewTemporalLayersCount); //ignore result
        hr = S_OK;
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

void TemporalScalablity::TemporalLayersCountChanged()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    ATLASSERT(IsTemporalLayersCountValid(m_nNewTemporalLayersCount) || 0 == m_nNewTemporalLayersCount);
    m_nTemporalLayersCount = m_nNewTemporalLayersCount;
    ResetLayersSequence();
    memset( &m_arrLayersMfxQP, 0, sizeof(m_arrLayersMfxQP) );   // reset QP values for temporal layers
    m_nSelectedLayer = 0;                                       // reset current layer selection
}

/*------------------------------------------------------------------------------*/

HRESULT TemporalScalablity::GetTemporalLayersCount(ULONG &nTemporalLayersCount) const
{
    HRESULT hr = VFW_E_CODECAPI_NO_CURRENT_VALUE;
    if (IsTemporalLayersCountValid(m_nTemporalLayersCount)) //return fail for TLC=0
    {
        nTemporalLayersCount = m_nTemporalLayersCount;
        hr = S_OK;
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

mfxEncodeCtrl* TemporalScalablity::GenerateFrameEncodeCtrl()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxEncodeCtrl *pResult = NULL;
    LONG nFrameLayerNum = GetTemporalLayerNumber();
    MFX_LTRACE_2(MF_TL_PERF, "","Frame #%04llu, Layer: #%d", m_nFrameNum, nFrameLayerNum);
    ATLASSERT( IsTemporalLayerNumberValid( nFrameLayerNum ) );
    if ( IsTemporalLayerNumberValid( nFrameLayerNum ) ) // should be true
    {
        //notify others (especially LTR about layer number):
        HRESULT hr = E_FAIL;
        if (0 == nFrameLayerNum)
            hr = m_pManager->HandleNewMessage(mtNotifyTemporalLayerIsBase, this);
        else if (nFrameLayerNum+1 == m_nTemporalLayersCount)
            hr = m_pManager->HandleNewMessage(mtNotifyTemporalLayerIsMax, this);
        else
            hr = m_pManager->HandleNewMessage(mtNotifyTemporalLayerIsNeitherBaseNorMax, this);
        ATLASSERT(SUCCEEDED(hr));

        mfxU16 nFrameType = MFX_FRAMETYPE_UNKNOWN;

        //TODO: this is not aligned with current Hardware Encoder Recommendations and Guidelines
        if ( BASE_LAYER_NUMBER == nFrameLayerNum && m_bForceKeyFrameForBaseLayer )
        {
            nFrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
            //no need in resetting m_nFrameNum, because IDRs are inserted for base layer only.
            //in common case calling ResetLayersSequence() is preffered
            m_bForceKeyFrameForBaseLayer = false;
        }

        mfxU16 nFrameQP = 0;
        nFrameQP = m_arrLayersMfxQP[nFrameLayerNum];

        if ( MFX_FRAMETYPE_UNKNOWN != nFrameType || 0 != nFrameQP )
        {
            SAFE_NEW(pResult, mfxEncodeCtrl);
            if ( NULL != pResult )
            {
                memset( pResult, 0, sizeof(mfxEncodeCtrl) );
                pResult->FrameType = nFrameType;
                pResult->QP = nFrameQP;
            }
        }
        m_nFrameNum++;
        hr = TryRequireDCC();
        ATLASSERT(SUCCEEDED(hr));

        //Check if next frame is base layer
        LONG nNextFrameLayerNum = GetTemporalLayerNumber();
        if (0 == nNextFrameLayerNum)
        {
            MFX_LTRACE_I(MF_TL_GENERAL, 0 == nNextFrameLayerNum);
            ATLASSERT(NULL != m_pManager);
            if (NULL != m_pManager)
            {
                // notify everyone about next frame is base layer on
                // in case if somebody wants to perform specific actions before that
                hr = m_pManager->HandleNewMessage(mtNotifyTemporalLayerNextIsBase, this);
            }
        }
    }

    TraceEncodeCtrlStatic(pResult);
    return pResult;
}

/*------------------------------------------------------------------------------*/

mfxU16 TemporalScalablity::GetMinNumRefFrame() const
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
    mfxU16 nResult = 0;
    switch (m_nNewTemporalLayersCount)
    {
    case 1: nResult = 1; break;
    case 2: nResult = 1; break;
    case 3: nResult = 2; break;
    case 4: nResult = 4; break;
    };
    MFX_LTRACE_I(MF_TL_GENERAL, nResult);
    return nResult;
}

/*------------------------------------------------------------------------------*/

HRESULT TemporalScalablity::SelectLayer(ULONG nLayerNumber)
{
    HRESULT hr = E_INVALIDARG;
    if (nLayerNumber < m_nTemporalLayersCount)
    {
        m_nSelectedLayer = nLayerNumber;
        hr = S_OK;
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT TemporalScalablity::GetSelectedLayerNumber(ULONG &nLayerNumber) const
{
    nLayerNumber = m_nSelectedLayer;
    return S_OK;
}


/*------------------------------------------------------------------------------*/

HRESULT TemporalScalablity::SetLayerEncodeQP(ULONGLONG nQP)
{
    return SetEncodeQP( m_nSelectedLayer, (mfxU16)nQP );
}

/*------------------------------------------------------------------------------*/

HRESULT TemporalScalablity::GetLayerEncodeQP(ULONGLONG &nQP) const
{
    mfxU16 nPerFrameMfxQP = 0;
    HRESULT hr = GetEncodeQP( m_nSelectedLayer, nPerFrameMfxQP );
    nQP = nPerFrameMfxQP;
    return hr;
}

/*------------------------------------------------------------------------------*/

void TemporalScalablity::ResetLayersSequence()
{
    m_nFrameNum = 0;
    m_bForceKeyFrameForBaseLayer = false; //TODO: = true?
}

/*------------------------------------------------------------------------------*/

HRESULT TemporalScalablity::HandleMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender)
{
    UNREFERENCED_PARAMETER(pSender);
    HRESULT hr = E_NOTIMPL;
    if (mtRequireDCC == pMessage->GetType())
    {
        m_bPostponeDCCBeforeBaseLayer = false;
        hr = S_OK;
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

void TemporalScalablity::TraceEncodeCtrlStatic(const mfxEncodeCtrl *pEncodeCtrl)
{
    if ( NULL == pEncodeCtrl)
    {
        MFX_LTRACE_S(MF_TL_PERF, "EncodeCtrl: NULL");
    }
    else
    {
        MFX_LTRACE_2(MF_TL_PERF, "EncodeCtrl: ", "QP=%02d, IDR=%d",
            pEncodeCtrl->QP, (MFX_FRAMETYPE_IDR & pEncodeCtrl->FrameType ? 1 : 0));
    }
}

/*------------------------------------------------------------------------------*/

ULONG TemporalScalablity::GetTemporalLayerNumber(ULONGLONG nFrameNum, ULONG nLayersCount)
{
    if (nLayersCount < 2)
        return 0;
    ULONG nLayerId = nLayersCount-1;
    // if the frame number is divisable by 2^n, it is in layer nLayersCount-1-n
    for ( ; nLayerId > 0; nLayerId-- )
    {
        if ( nFrameNum & 1 )
            break;
        nFrameNum = nFrameNum >> 1;
    }
    return nLayerId;
}

/*------------------------------------------------------------------------------*/

bool TemporalScalablity::IsTemporalLayerNumberValid(ULONG nTemporalLayerNumber) const
{
    if (0 == m_nTemporalLayersCount)
        return 0 == nTemporalLayerNumber;
    else
        return m_nTemporalLayersCount <= MAX_TEMPORAL_LAYERS_COUNT &&
                    nTemporalLayerNumber < m_nTemporalLayersCount;
}


/*------------------------------------------------------------------------------*/

HRESULT TemporalScalablity::SetEncodeQP(ULONG nTemporalLayerNumber, mfxU16 nPerFrameMfxQP)
{
    HRESULT hr = E_INVALIDARG;
    if ( IsTemporalLayerNumberValid ( nTemporalLayerNumber ) &&
            nPerFrameMfxQP >= 1 && nPerFrameMfxQP <= 51 )
    {
        m_arrLayersMfxQP[nTemporalLayerNumber] = nPerFrameMfxQP;
        hr = S_OK;
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT TemporalScalablity::GetEncodeQP(ULONG nTemporalLayerNumber, mfxU16 &nPerFrameMfxQP) const
{
    HRESULT hr = E_INVALIDARG;
    if ( IsTemporalLayerNumberValid ( nTemporalLayerNumber ))
    {
        hr = VFW_E_CODECAPI_NO_CURRENT_VALUE;
        nPerFrameMfxQP = m_arrLayersMfxQP[nTemporalLayerNumber];
        if ( nPerFrameMfxQP >= 1 && nPerFrameMfxQP < 51 )
        {
            hr = S_OK;
        }
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

void TemporalScalablity::GenerateTemporalLayersBuffer(ULONG nTemporalLayersCount) const
{
    if ( nTemporalLayersCount > 0 && nTemporalLayersCount <= MAX_TEMPORAL_LAYERS_COUNT )
    {
        memset( &m_extAvcTemporalLayers, 0, sizeof(m_extAvcTemporalLayers) );
        m_extAvcTemporalLayers.Header.BufferId = MFX_EXTBUFF_AVC_TEMPORAL_LAYERS;
        m_extAvcTemporalLayers.Header.BufferSz = sizeof(mfxExtAvcTemporalLayers);
        // Value for m_extAvcTemporalLayers.BaseLayerPID is not provided

        // first layer is base one it must have scale 1 by definition
        m_extAvcTemporalLayers.Layer[0].Scale = 1;

        // next layers will have increasing scales: 2, 4, 8,...
        for ( ULONG i = 1; i < nTemporalLayersCount; i++ )
        {
            m_extAvcTemporalLayers.Layer[i].Scale =
                TEMPORAL_LAYERS_SCALE_RATIO * m_extAvcTemporalLayers.Layer[i-1].Scale;
        }
    }
}

/*------------------------------------------------------------------------------*/