/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2019 Intel Corporation. All Rights Reserved.

File Name: 

*******************************************************************************/

#pragma once

#include "mfxsvc.h"
#include "mfx_iyuv_source.h"

class TargetLayerSvcDecoder
    : public InterfaceProxy<IYUVSource>
{
protected:
    typedef InterfaceProxy<IYUVSource> base;
    mfxExtSvcTargetLayer m_refLayerDesc;
    std::vector<mfxExtBuffer*> m_pAttachedBuffers;
public:
    TargetLayerSvcDecoder( mfxExtSvcTargetLayer &refLayerDesc, std::unique_ptr<IYUVSource> &&pTarget)
        : base(std::move(pTarget))
        , m_refLayerDesc(refLayerDesc)
    {
        mfx_set_ext_buffer_header(m_refLayerDesc);
    }
    virtual mfxStatus Init(mfxVideoParam *par)
    {
        if (NULL != par)
        {
            for (size_t i =0; i < par->NumExtParam; i++)
            {
                m_pAttachedBuffers.push_back(par->ExtParam[i]);
            }
            m_pAttachedBuffers.push_back((mfxExtBuffer*)&m_refLayerDesc);

            mfxExtBuffer **origin = par->ExtParam;

            par->ExtParam = &m_pAttachedBuffers.front();
            par->NumExtParam++;

            mfxStatus sts = base::Init(par);

            par->NumExtParam--;
            par->ExtParam = origin;

            return sts;
        }

        return base::Init(par);
    }

};
