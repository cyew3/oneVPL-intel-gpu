/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
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

    std::vector<mfxExtBuffer*>      m_mfxExtParamsBuf;
    std::vector<mfxExtBuffer*>      m_mfxExtFEIInputBuf;
    std::vector<mfxExtBuffer*>      m_mfxExtFEIOutputBuf;
};

#endif /* _PLUGIN_WRAPPER_H_ */
