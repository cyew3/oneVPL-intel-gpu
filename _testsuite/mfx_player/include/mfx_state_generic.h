/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "mfx_istate.h"

/*Generic implementation of base classes for state pattern*/

//wraps over structure that need to be passed as state data
template<class T>
class GenericStateData
    : public IStateData
    , private mfx_no_assign
{
public:
    GenericStateData(T _val)
        : m_data (_val)
    {}
    T m_data;
};

//assumes that context data is of GenericSatteData<TData> type
template<class TCtx, class TData>
class GenericState
    : public IState
{
public:
    mfxStatus Handle(IStateContext *pCtx, IStateData * pIData) 
    {
        GenericStateData<TData>* pData = dynamic_cast<GenericStateData<TData>*>(pIData);
        MFX_CHECK_POINTER(pData);

        TCtx *pCtxConcrete = dynamic_cast<TCtx*>(pCtx);
        MFX_CHECK_POINTER(pCtxConcrete);

        return HandleParticular(pCtxConcrete, pData->m_data);
    }

protected:
    //handle particular message
    virtual mfxStatus HandleParticular(TCtx *ctx, TData Data) = 0;
};


///Context That has states
template <class T>
class GenericStateContext
    : public IStateContext
{
public:
    GenericStateContext(IState * pInitialState)
        : m_pCurrentState(pInitialState)
    {
    }

    //iStateContext
    virtual mfxStatus SetState(IState * pState)
    {
        m_pCurrentState.reset(pState);
        return MFX_ERR_NONE;
    }
    

    /* Function That Called to handle data at every state
       function doent make any assumptions regarding handling of status, if it is not switch state status
    */
    virtual mfxStatus Process(T data)
    {
        MFX_CHECK_POINTER(m_pCurrentState.get());

        GenericStateData<T> stateData(data);
        mfxStatus sts = PIPELINE_WRN_SWITCH_STATE;

        while (sts == PIPELINE_WRN_SWITCH_STATE)
        {
            sts = m_pCurrentState->Handle(this, &stateData);
        }

        return sts;
    }

protected:
    std::auto_ptr<IState> m_pCurrentState;
};
