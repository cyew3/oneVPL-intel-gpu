/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

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

#endif __TEMPORAL_SCALABLITY_H__
