/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
This sample was distributed or derived from the Intel’s Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#ifndef _PLUGIN_WRAPPER_H_
#define _PLUGIN_WRAPPER_H_

/* internal (sample) files */
#include "mfxvideo++.h"
#include "mfxplugin++.h"
#include "sample_defs.h"
#include "sample_params.h"
#include "plugin_loader.h"

#include "d3d_allocator.h"
#include "d3d11_allocator.h"
#include "general_allocator.h"
#include "hw_device.h"
#include "d3d_device.h"
#include "d3d11_device.h"

#ifdef LIBVA_SUPPORT
#include "vaapi_device.h"
#include "vaapi_utils.h"
#include "vaapi_allocator.h"
#endif

class CH265FEI
{

public:
    CH265FEI();
    ~CH265FEI();
    mfxStatus Init(SampleParams *sp);
    mfxStatus ProcessFrameAsync(ProcessInfo *pi, mfxFEIH265Output *out, mfxSyncPoint *syncp);
    mfxStatus SyncOperation(mfxSyncPoint syncp);
    mfxStatus Close(void);

private:
    mfxVideoParam                   m_mfxParams;
    mfxExtFEIH265Param              m_mfxExtParams;
    mfxExtFEIH265Input              m_mfxExtFEIInput;
    mfxExtFEIH265Output             m_mfxExtFEIOutput;

    std::auto_ptr<MFXVideoSession>  m_pmfxSession;
    std::auto_ptr<MFXVideoENC>      m_pmfxFEI;
    std::auto_ptr<MFXVideoUSER>     m_pUserEncModule;
    std::auto_ptr<MFXPlugin>        m_pUserEncPlugin;
    std::auto_ptr<CHWDevice>        m_hwdev;

    std::vector<mfxExtBuffer*>      m_mfxExtParamsBuf;
    std::vector<mfxExtBuffer*>      m_mfxExtFEIInputBuf;
    std::vector<mfxExtBuffer*>      m_mfxExtFEIOutputBuf;
};

#endif /* _PLUGIN_WRAPPER_H_ */
