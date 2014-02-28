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

//Rationale: 
//mediaSDK c++ wrapper wraps only c library functionality
//however OOP style code requires to have allocators connected to session as a property

class MFXVideoSessionExt : public MFXVideoSession {
public:
    MFXVideoSessionExt() 
        : m_FrameAllocator() {}
    virtual mfxStatus SetFrameAllocator(mfxFrameAllocator *allocator) { 
        m_FrameAllocator = allocator;
        return MFXVideoCORE_SetFrameAllocator(m_session, allocator); 
    }
    virtual mfxStatus GetFrameAllocator(mfxFrameAllocator *&refAllocator) { 
        refAllocator = m_FrameAllocator;
        return MFX_ERR_NONE; 
    }
protected:
    mfxFrameAllocator *m_FrameAllocator;
};
