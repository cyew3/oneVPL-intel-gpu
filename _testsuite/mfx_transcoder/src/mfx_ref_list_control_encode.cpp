/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_ref_list_control_encode.h"

RefListControlEncode::RefListControlEncode (std::auto_ptr<IVideoEncode>& pTarget)
    : InterfaceProxy<IVideoEncode>(pTarget)
    , m_bAttach()
    , m_ctrl()
    , m_nFramesEncoded()
{
    mfxExtAVCRefListCtrl tmp_elem = mfxExtAVCRefListCtrl();
    m_extParams.push_back(&tmp_elem);
    m_pRefList = (mfxExtAVCRefListCtrl*)m_extParams.back();
    m_ctrl.NumExtParam = (mfxU16)m_extParams.size();
    m_ctrl.ExtParam = &m_extParams;
}

mfxStatus RefListControlEncode::EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp) 
{
    if (!m_bAttach)
    {
        if (NULL != surface)
        {
            m_nFramesEncoded ++;
        }
        return InterfaceProxy<IVideoEncode>::EncodeFrameAsync(ctrl, surface, bs, syncp);
    }
        
    //if using not our own buffer we need to recreate whole pointers array
    mfxExtBuffer ** ppExtParams = NULL;
    std::vector<mfxExtBuffer *> ppExtParamsNew ;
    mfxExtAVCRefListCtrl * pCurrentRefList = m_pRefList; 

    if (NULL == ctrl)
    {
        ctrl = &m_ctrl;
    } 
    else
    {
        MFXExtBufferPtr<mfxExtAVCRefListCtrl> pRefList(MFXExtBufferVector(ctrl->ExtParam, ctrl->NumExtParam));

        if (!pRefList.get())
        {
            //recreation of extbuffer array with increased len
            ppExtParams = ctrl->ExtParam;
            for (mfxU32 i = 0; i < ctrl->NumExtParam; i++)
            {
                ppExtParamsNew.push_back(ppExtParams[i]);
            }

            //point to our buffer
            MFXExtBufferPtr<mfxExtAVCRefListCtrl> pRefListOwn(m_extParams);
            ppExtParamsNew.push_back((mfxExtBuffer*)pRefListOwn.get());

            ctrl->ExtParam = &ppExtParamsNew.front();
            ctrl->NumExtParam++;
        }
        else
        {
            //using provided pointer - very unusual situation
            pCurrentRefList = pRefList.get();
        }
    }
    
    UpdateRefList(pCurrentRefList);

    if (NULL != surface)
    {
        m_nFramesEncoded ++;
    }

    mfxStatus sts = InterfaceProxy<IVideoEncode>::EncodeFrameAsync(ctrl, surface, bs, syncp);

    //detaching extbuffer
    if (ctrl != &m_ctrl)
    {
        ctrl->ExtParam = ppExtParams;
    }

    return sts;
}

void RefListControlEncode::UpdateRefList(mfxExtAVCRefListCtrl * pCurrent)
{
    //todo: implement
    * pCurrent = m_currentPattern;
}

void RefListControlEncode::AddExtBuffer( mfxExtBuffer &buffer )
{
    m_bAttach = true;
    m_currentPattern = (mfxExtAVCRefListCtrl&)buffer;
}

void RefListControlEncode::RemoveExtBuffer( mfxU32 /*mfxExtBufferId*/ )
{
    m_bAttach = false;
}