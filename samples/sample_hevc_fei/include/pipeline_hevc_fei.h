/******************************************************************************\
Copyright (c) 2017, Intel Corporation
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
#ifndef __PIPELINE_FEI_H__
#define __PIPELINE_FEI_H__

#include "sample_hevc_fei_defs.h"
#include "mfxplugin.h"
#include "mfxplugin++.h"
#include "plugin_utils.h"
#include "plugin_loader.h"
#include "vaapi_device.h"
#include "fei_utils.h"
#include "hevc_fei_encode.h"
#include "hevc_fei_preenc.h"
#include "ref_list_manager.h"

/* This class implements a FEI pipeline */
class CEncodingPipeline
{
public:
    CEncodingPipeline(sInputParams& userInput);
    ~CEncodingPipeline();

    mfxStatus Init();
    mfxStatus Execute();
    void Close();

    void PrintInfo();

private:
    const sInputParams m_inParams; /* collection of user parameters, adjusted in parsing and
                                      shouldn't be modified during processing to not lose
                                      initial settings */

    mfxU32                   m_processedFrames;

    mfxIMPL                  m_impl;
    MFXVideoSession          m_mfxSession;
    std::auto_ptr<MFXPlugin> m_pPlugin;
    mfxPluginUID             m_pluginGuid;

    std::auto_ptr<MFXFrameAllocator>  m_pMFXAllocator;
    std::auto_ptr<mfxAllocatorParams> m_pMFXAllocatorParams;
    std::auto_ptr<CHWDevice>          m_pHWdev;

    std::auto_ptr<IVideoReader> m_pYUVSource; // source of raw data for encoder (can be either reading from file,
                                              // or decoding bitstream)
    std::auto_ptr<FEI_Encode>   m_pFEI_Encode;
    std::auto_ptr<IPreENC>      m_pFEI_PreENC;

    std::auto_ptr<EncodeOrderControl> m_pOrderCtrl;

    SurfacesPool m_EncSurfPool;

    mfxStatus LoadFEIPlugin();
    mfxStatus CreateAllocator();
    mfxStatus CreateHWDevice();
    mfxStatus FillInputFrameInfo(mfxFrameInfo& fi);
    mfxStatus AllocFrames();
    mfxStatus InitComponents();
    mfxStatus DrainBufferedFrames();

    FEI_Preenc* CreatePreENC(mfxFrameInfo& in_fi);
    FEI_Encode* CreateEncode(mfxFrameInfo& in_fi);

    // forbid copy constructor and operator
    CEncodingPipeline(const CEncodingPipeline& pipeline);
    CEncodingPipeline& operator=(const CEncodingPipeline& pipeline);
};

// function-utils for creating mfxVideoParam for every component in pipeline
MfxVideoParamsWrapper GetCommonEncodeParams(const sInputParams& user_pars, const mfxFrameInfo& in_fi);
MfxVideoParamsWrapper GetPreEncParams(const sInputParams& user_pars, const mfxFrameInfo& in_fi);
MfxVideoParamsWrapper GetEncodeParams(const sInputParams& user_pars, const mfxFrameInfo& in_fi);

#endif // __PIPELINE_FEI_H__
