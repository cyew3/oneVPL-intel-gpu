/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mf_mfx_params_interface.h"

#define MF_PARAM_SLICECONTROL_MODE_MBS_PER_SLICE   0
#define MF_PARAM_SLICECONTROL_MODE_BITS_PER_SLICE  1
#define MF_PARAM_SLICECONTROL_MODE_MBROW_PER_SLICE 2
#define MF_PARAM_SLICECONTROL_MODE_MAX             MF_PARAM_SLICECONTROL_MODE_MBROW_PER_SLICE

class MFParamSliceControl: public MFParamsWorker
{
public:
    MFParamSliceControl(MFParamsManager *pManager)
        : MFParamsWorker(pManager),
          m_nMode(0),
          m_nSize(0),
          m_nInitNumSlice(0)
    {
        Zero(m_frameInfo);
    }

    virtual ~MFParamSliceControl()
    {
    };

    //MFParamsWorker::

    //virtual HRESULT HandleMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender)
    //{ return E_FAIL; }


    virtual bool IsEnabled() const { return true; }

    //Endpoint of ICodecAPI:

    virtual HRESULT SetMode(ULONG nMode)
    {
        HRESULT hr = S_OK;
        switch (nMode)
        {
        case MF_PARAM_SLICECONTROL_MODE_MBROW_PER_SLICE:
        case MF_PARAM_SLICECONTROL_MODE_MBS_PER_SLICE:
            m_nMode = nMode;
            break;
        default:
            hr = E_INVALIDARG;
        };
        return hr;
    }

    virtual ULONG GetMode(HRESULT *pHR)
    {
        if (NULL != pHR)
            *pHR = S_OK;
        return m_nMode;
    }
    
    virtual HRESULT GetModeParameterRange(VARIANT *ValueMin, VARIANT *ValueMax, VARIANT *SteppingDelta)
    {
        HRESULT hr = S_OK;
        CHECK_POINTER(ValueMin, E_POINTER);
        CHECK_POINTER(ValueMax, E_POINTER);
        CHECK_POINTER(SteppingDelta, E_POINTER);
        if (SUCCEEDED(hr))
        {
            hr = VariantClear(ValueMin);
        }
        if (SUCCEEDED(hr))
        {
            hr = VariantClear(ValueMax);
        }
        if (SUCCEEDED(hr))
        {
            hr = VariantClear(SteppingDelta);
        }
        
        ValueMin->vt   = VT_UI4;
        ValueMax->vt   = VT_UI4;
        SteppingDelta->vt   = VT_UI4;

        return GetModeParameterRange(ValueMin->ulVal, ValueMax->ulVal, SteppingDelta->ulVal);
    }

    virtual HRESULT GetModeParameterRange(ULONG &ValueMin, ULONG &ValueMax, ULONG &SteppingDelta)
    {
        ValueMin = 0; 
        ValueMax = 2;
        SteppingDelta = 2;
        return S_OK;
    }

    virtual HRESULT SetSize(ULONG nSize)
    {
        HRESULT hr = E_INVALIDARG;
        //no need to check for <= CalcMaxSize(m_nMode, m_frameInfo);
        //because last (only one in this case) slice might be smaller.
        //ATLASSERT(nSize > 0);
        if (nSize > 0)
        {
            m_nSize = nSize;
            hr = S_OK;
        }
        return hr;
    }

    virtual ULONG GetSize(HRESULT *pHR)
    {
        if (NULL != pHR)
            *pHR = S_OK;
        return m_nSize;
    }
    
    virtual HRESULT SetFrameInfo(const mfxFrameInfo &frameInfo)
    {
        HRESULT hr = E_INVALIDARG;
        if (frameInfo.CropW > 0 && frameInfo.CropH > 0)
        {
            memcpy_s(&m_frameInfo, sizeof(m_frameInfo), &frameInfo, sizeof(frameInfo));
            hr = S_OK;
        }
        return hr;
    }

    virtual mfxU16 GetNumSlice() const
    {
        return CalcNumSlice(m_nMode, m_nSize, m_frameInfo);
    }

    virtual HRESULT SetInitNumSlice(mfxU16 nNumSlice)
    {
        m_nInitNumSlice = nNumSlice;
        return S_OK;
    }

    virtual bool IsIdrNeeded() const
    {
        return GetNumSlice() > m_nInitNumSlice;
    }

protected:
    ULONG m_nMode;
    ULONG m_nSize;
    mfxFrameInfo m_frameInfo;
    mfxU16 m_nInitNumSlice;

    static mfxU32 CalcMaxSize(ULONG nMode, const mfxFrameInfo &frameInfo)
    {
        mfxU32 nResult = 0;
        //TODO: interlaced video?
        mfxU16 nMBsRowPerFrame = (frameInfo.CropH + 15) / 16;   //paranoid
        mfxU16 nMBsPerRow = (frameInfo.CropW + 15) / 16;        //paranoid
        switch (nMode)
        {
        case MF_PARAM_SLICECONTROL_MODE_MBS_PER_SLICE:
            nResult = nMBsRowPerFrame * nMBsPerRow;
            break;
        case MF_PARAM_SLICECONTROL_MODE_MBROW_PER_SLICE:
            nResult = nMBsRowPerFrame;
            break;
        default:
            ATLASSERT(false);
        };
        return nResult;
    }

    static mfxU16 CalcNumSlice(ULONG nMode, ULONG nSize, const mfxFrameInfo &frameInfo)
    {
        mfxU16 nResult = 0;
        if (nSize > 0)
        {
            mfxU32 nU32Result = ( CalcMaxSize(nMode, frameInfo) + nSize-1 ) / nSize; // last slice might be smaller
            if (nU32Result <= USHRT_MAX) //8K?
            {
                nResult = (mfxU16)nU32Result;
            }
            else
            {
                ATLASSERT(nU32Result <= USHRT_MAX);
                nResult = USHRT_MAX;
            }
        }
        return nResult;
    }
};