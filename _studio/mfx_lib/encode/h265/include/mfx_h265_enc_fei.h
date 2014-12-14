/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#if defined (MFX_VA)

#ifndef __MFX_H265_ENC_FEI_H__
#define __MFX_H265_ENC_FEI_H__

#include "mfx_h265_fei.h"
#include "mfx_h265_enc.h"

namespace H265Enc {

///class H265VideoParam;
class H265Frame;
struct H265Slice;

typedef struct _FEIFrame {
    int EncOrder;   // input frame order

    int IntraDone;
    int RefEncOrder[2][MFX_FEI_H265_MAX_NUM_REF_FRAMES];
    int RefDone[2][MFX_FEI_H265_MAX_NUM_REF_FRAMES];
} FEIFrame;

class FeiContext
{
public:

    FeiContext(const H265VideoParam *param, VideoCORE *core);
    ~FeiContext();

    void ProcessFrameFEI(mfxI32 feiInIdx, H265Frame *frameIn, H265Slice *sliceIn, H265Frame **dpb, Ipp32s dpbSize, Ipp8u prevFrameDone);
    mfxFEIH265Output *feiH265Out;
    mfxI32            feiInIdx;  // flipping between current and next

private:

    void ResetFEIFrame(FEIFrame *feiFrame);
    void UpdateFrameStateFEI(mfxFEIH265Input *feiIn, H265Frame *frameIn, H265Frame *frameRef, Ipp32s refIdx, Ipp32s sliceType);

    mfxFEIH265       m_feiH265;
    mfxFEIH265Param  m_feiParam;
    mfxFEIH265Input  m_feiH265In[2];
    mfxFEIH265Output m_feiH265Out;
    mfxFEISyncPoint  m_syncpIntra[2];
    mfxFEISyncPoint  m_syncpInter[2][2][MAX_NUM_REF_IDX];
    
    FEIFrame         m_feiFrame[2];
};

} // namespace


#endif // __MFX_H265_ENC_FEI_H__

#endif // MFX_VA

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
