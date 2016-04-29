/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

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
