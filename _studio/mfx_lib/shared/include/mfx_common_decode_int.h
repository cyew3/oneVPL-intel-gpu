// Copyright (c) 2008-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __MFX_COMMON_DECODE_INT_H__
#define __MFX_COMMON_DECODE_INT_H__

#include <vector>
#include "mfx_common.h"
#include "mfx_common_int.h"
#include "umc_video_decoder.h"
#include "vm_sys_info.h"

class MFXMediaDataAdapter : public UMC::MediaData
{
public:
    MFXMediaDataAdapter(mfxBitstream *pBitstream = 0);

    void Load(mfxBitstream *pBitstream);
    void Save(mfxBitstream *pBitstream);

    void SetExtBuffer(mfxExtBuffer*);
};

mfxStatus ConvertUMCStatusToMfx(UMC::Status status);

void ConvertMFXParamsToUMC(mfxVideoParam const* par, UMC::VideoStreamInfo* umcVideoParams);
void ConvertMFXParamsToUMC(mfxVideoParam const* par, UMC::VideoDecoderParams* umcVideoParams);

mfxU32 ConvertUMCColorFormatToFOURCC(UMC::ColorFormat);
void ConvertUMCParamsToMFX(UMC::VideoStreamInfo const*, mfxVideoParam*);
void ConvertUMCParamsToMFX(UMC::VideoDecoderParams const*, mfxVideoParam*);

bool IsNeedChangeVideoParam(mfxVideoParam *par);

mfxU16 FourCcBitDepth(mfxU32 fourCC);
bool InitBitDepthFields(mfxFrameInfo *info);

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

        for (uint32_t i = 0; i < sizeof(svcDesc->DependencyLayer)/sizeof(svcDesc->DependencyLayer[0]); i++)
        {
            if (svcDesc->DependencyLayer[i].Active)
            {
                if ((svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[0] & 0xf) || (svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[1] & 0xf))
                {
                    isBaselineConstraints = false;
                    break;
                }

                uint32_t depD = svcDesc->DependencyLayer[i].RefLayerDid;
                if (depD >= 8)
                    continue;
                uint16_t scaledW = (uint16_t)(svcDesc->DependencyLayer[i].Width - svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[0]-svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[2]);
                uint16_t scaledH = (uint16_t)(svcDesc->DependencyLayer[i].Height - svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[1]-svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[3]);
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

inline
void MoveBitstreamData(mfxBitstream& bs, mfxU32 offset)
{
    VM_ASSERT(offset <= bs.DataLength);
    bs.DataOffset += offset;
    bs.DataLength -= offset;
}

// Memory reference counting base class
class RefCounter
{
public:

    RefCounter() : m_refCounter(0)
    {
    }

    void IncrementReference() const;

    void DecrementReference();

    void ResetRefCounter() { m_refCounter = 0; }

    uint32_t GetRefCounter() const { return m_refCounter; }

protected:
    mutable int32_t m_refCounter;

    virtual ~RefCounter()
    {
    }

    virtual void Free()
    {
    }
};

inline
mfxU16 CalculateNumThread(mfxVideoParam *par, eMFXPlatform platform)
{
    mfxU16 numThread = (MFX_PLATFORM_SOFTWARE == platform) ? static_cast<mfxU16>(vm_sys_info_get_cpu_num()) : 1;
    if (!par || !par->AsyncDepth)
        return numThread;

    return std::min(par->AsyncDepth, numThread);
}

inline
mfxU32 CalculateAsyncDepth(eMFXPlatform platform, mfxVideoParam *par)
{
    mfxU32 asyncDepth = par ? par->AsyncDepth : 0;
    if (!asyncDepth)
    {
        asyncDepth = (platform == MFX_PLATFORM_SOFTWARE) ? vm_sys_info_get_cpu_num() : MFX_AUTO_ASYNC_DEPTH_VALUE;
    }

    return asyncDepth;
}

#endif
