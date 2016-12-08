//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_COMMON_DECODE_INT_H__
#define __MFX_COMMON_DECODE_INT_H__

#include <vector>
#include "mfx_config.h"
#include "mfx_common_int.h"
#include "umc_video_decoder.h"

class MFXMediaDataAdapter : public UMC::MediaData
{
public:
    MFXMediaDataAdapter(mfxBitstream *pBitstream = 0);

    void Load(mfxBitstream *pBitstream);
    void Save(mfxBitstream *pBitstream);
};

mfxStatus ConvertUMCStatusToMfx(UMC::Status status);

void ConvertMFXParamsToUMC(mfxVideoParam *par, UMC::VideoDecoderParams *umcVideoParams);
void ConvertMFXParamsToUMC(mfxVideoParam *par, UMC::VideoStreamInfo *umcVideoParams);

bool IsNeedChangeVideoParam(mfxVideoParam *par);

inline mfxU32 ExtractProfile(mfxU32 profile)
{
    return profile & 0xFF;
}

inline bool IsMVCProfile(mfxU32 profile)
{
    profile = ExtractProfile(profile);
    return (profile == MFX_PROFILE_AVC_MULTIVIEW_HIGH || profile == MFX_PROFILE_AVC_STEREO_HIGH);
}

#if defined(MFX_ENABLE_SVC_VIDEO_DECODE)
inline bool IsSVCProfile(mfxU32 profile)
{
    profile = ExtractProfile(profile);
    return (profile == MFX_PROFILE_AVC_SCALABLE_BASELINE || profile == MFX_PROFILE_AVC_SCALABLE_HIGH);
}
#endif

#if defined (MFX_VA_WIN)
template <class T>
mfxStatus CheckIntelDataPrivateReport(T *pConfig, mfxVideoParam *par)
{
    if ((par->mfx.FrameInfo.Width > pConfig->ConfigMBcontrolRasterOrder) ||
        (par->mfx.FrameInfo.Height > pConfig->ConfigResidDiffHost))
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

#ifdef MFX_ENABLE_SVC_VIDEO_DECODE
    bool isArbitraryCroppingSupported = pConfig->ConfigDecoderSpecific & 0x01;

    mfxExtSVCSeqDesc * svcDesc = (mfxExtSVCSeqDesc*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC);
    if (svcDesc && IsSVCProfile(par->mfx.CodecProfile) && !isArbitraryCroppingSupported)
    {
        bool isBaselineConstraints = true;

        for (Ipp32u i = 0; i < sizeof(svcDesc->DependencyLayer)/sizeof(svcDesc->DependencyLayer[0]); i++)
        {
            if (svcDesc->DependencyLayer[i].Active)
            {
                if ((svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[0] & 0xf) || (svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[1] & 0xf))
                {
                    isBaselineConstraints = false;
                    break;
                }

                Ipp32u depD = svcDesc->DependencyLayer[i].RefLayerDid;
                if (depD >= 8)
                    continue;
                Ipp16u scaledW = (Ipp16u)(svcDesc->DependencyLayer[i].Width - svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[0]-svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[2]);
                Ipp16u scaledH = (Ipp16u)(svcDesc->DependencyLayer[i].Height - svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[1]-svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[3]);
                if ( ! ((scaledW == svcDesc->DependencyLayer[depD].Width &&
                    scaledH == svcDesc->DependencyLayer[depD].Height) ||

                    (scaledW == 2*svcDesc->DependencyLayer[depD].Width &&
                    scaledH == 2*svcDesc->DependencyLayer[depD].Height) ||

                    (scaledW == (3*svcDesc->DependencyLayer[depD].Width)/2 &&
                    scaledH == (3*svcDesc->DependencyLayer[depD].Height)/2) ))
                {
                    isBaselineConstraints = false;
                    break;
                }
            }
        }

        if (!isBaselineConstraints)
        {
            return MFX_WRN_PARTIAL_ACCELERATION;
        }
    }
#endif

    return MFX_ERR_NONE;
} // mfxStatus CheckIntelDataPrivateReport(...)
#endif // defined (MFX_VA_WIN)

#endif
