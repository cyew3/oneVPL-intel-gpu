/*//////////////////////////////////////////////////////////////////////////////
////                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#ifndef _MFX_H265_ENC_CM_PLUGIN_H_
#define _MFX_H265_ENC_CM_PLUGIN_H_

#ifdef AS_H265FEI_PLUGIN

#include "mfx_enc_ext.h"
#include "mfx_h265_fei.h"
#include "cmrt_cross_platform.h"

#include <memory>
#include <list>
#include <vector>


typedef struct
{
    mfxFEIH265Input      feiH265In;
    mfxExtFEIH265Output  feiH265Out;
    mfxFEISyncPoint      feiH265SyncPoint;

} sAsyncParams;

class VideoENC_H265FEI:  public VideoENC_Ext
{
public:

     VideoENC_H265FEI(VideoCORE *core, mfxStatus *sts);

    // Destructor
    virtual
    ~VideoENC_H265FEI(void);

    virtual
    mfxStatus Init(mfxVideoParam *par) ;
    virtual
    mfxStatus Reset(mfxVideoParam *par);
    virtual
    mfxStatus Close(void);
    virtual
    mfxStatus AllocateInputSurface(mfxHDL *surfIn);
    virtual
    mfxStatus AllocateOutputSurface(mfxU8 *sysMem, mfxSurfInfoENC *surfInfo, mfxHDL *surfOut);
    virtual
    mfxStatus FreeSurface(mfxHDL surf);

    static 
    mfxStatus Query(VideoCORE*, mfxVideoParam *in, mfxVideoParam *out);
    static 
    mfxStatus QueryIOSurf(VideoCORE*, mfxVideoParam *par, mfxFrameAllocRequest *request);

    virtual
    mfxStatus GetVideoParam(mfxVideoParam *par);
    
    virtual
    mfxStatus GetFrameParam(mfxFrameParam *par);

    using VideoENC::RunFrameVmeENCCheck;
    virtual
    mfxStatus RunFrameVmeENCCheck(mfxENCInput *input, mfxENCOutput *output, MFX_ENTRY_POINT pEntryPoints[], mfxU32 &numEntryPoints);

    mfxStatus ResetTaskCounters();

    /* defined in internal header mfx_h265_fei.h */
    mfxFEIH265              m_feiH265;
    mfxFEIH265Param         m_feiH265Param;
    mfxFEIH265Input         m_feiH265In;
    mfxFEIH265Output        m_feiH265Out;

private:
    bool                    m_bInit;
    VideoCORE*              m_core;

};

#endif  // AS_H265FEI_PLUGIN

#endif  // _MFX_H265_ENC_CM_PLUGIN_H_

