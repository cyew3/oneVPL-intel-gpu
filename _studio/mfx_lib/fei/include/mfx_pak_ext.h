//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#ifndef _MFX_PAK_EXT_H_
#define _MFX_PAK_EXT_H_



#include "mfxvideo++int.h"
#include "mfxpak.h"

class VideoPAK_Ext:  public VideoPAK
{
public:
    // Destructor
    virtual
    ~VideoPAK_Ext(void){}

    virtual
    mfxTaskThreadingPolicy GetThreadingPolicy(void) {return MFX_TASK_THREADING_DEFAULT;}

    virtual
    mfxStatus GetVideoParam(mfxVideoParam *par) = 0;
    virtual
    mfxStatus GetFrameParam(mfxFrameParam * /* par */)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual 
    mfxStatus  RunFramePAKCheck( mfxPAKInput *           /* in */,
                                    mfxPAKOutput *          /* out */,
                                    MFX_ENTRY_POINT*        /* pEntryPoints */)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual
    mfxStatus RunFramePAKCheck(  mfxPAKInput *  in,
                                    mfxPAKOutput *  out,
                                    MFX_ENTRY_POINT pEntryPoints[],
                                    mfxU32 & numEntryPoints) = 0;

    virtual
    mfxStatus RunFramePAK(mfxPAKInput * /* in */, mfxPAKOutput * /* out */)
    {
        return MFX_ERR_UNSUPPORTED;
    };

    virtual 
    void* GetDstForSync(MFX_ENTRY_POINT& /* pEntryPoints */) 
    {
        return NULL;
    }

    virtual 
    void* GetSrcForSync(MFX_ENTRY_POINT& /* pEntryPoints */) 
    {
         return NULL;   
    }

};

#endif


