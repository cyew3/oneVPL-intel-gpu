//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
//*/

#pragma once

#include "mfxvideo++.h"
#include "hw_device.h"
#include "base_allocator.h"
#include "av_alloc_request.h"
#include "isample.h"
#include <memory>


class PipelineFactory;

//Rationale:
//mediaSDK c++ wrapper wraps only c library functionality
//however OOP style code requires to have allocators connected to session as a property

class MFXVideoSessionExt : public MFXVideoSession {
public:
    MFXVideoSessionExt(PipelineFactory& factory);

    virtual mfxStatus GetFrameAllocator(mfxFrameAllocator *&refAllocator) {
        refAllocator = m_pAllocator.get();
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Init(mfxIMPL impl, mfxVersion *ver) {
        mfxStatus sts = MFXInit(impl, ver, &m_session);
        if (!sts)
            SetDeviceAndAllocator();
        return sts;
    }
    virtual std::auto_ptr<CHWDevice>& GetDevice() {
        return m_pDevice;
    }
    virtual std::auto_ptr<BaseFrameAllocator>& GetAllocator() {
        return m_pAllocator;
    }
    virtual ~MFXVideoSessionExt() {
        // we need to explicitly close the session to make sure that it releases
        // all objects, otherwise we may release device or allocator while
        // it is still used by the session
        Close();
    }
protected:
    PipelineFactory& m_factory;

    std::auto_ptr<CHWDevice> m_pDevice;
    std::auto_ptr<BaseFrameAllocator> m_pAllocator;

    void CreateDevice(AllocatorImpl impl);
    void SetDeviceAndAllocator();
};
