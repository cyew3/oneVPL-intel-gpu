/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_WIDEVINE_DECRYPTER_H
#define __UMC_H264_WIDEVINE_DECRYPTER_H

#include "umc_va_base.h"
#include "umc_h264_headers.h"
#include "huc_based_drm_common.h"

namespace UMC
{
class DecryptParametersWrapper : public DECRYPT_QUERY_STATUS_PARAMS_AVC
{
public:
    DecryptParametersWrapper(void);
    DecryptParametersWrapper(const DECRYPT_QUERY_STATUS_PARAMS_AVC & pDecryptParameters);
    virtual ~DecryptParametersWrapper(void);

    DecryptParametersWrapper & operator = (const DECRYPT_QUERY_STATUS_PARAMS_AVC & pDecryptParameters);
    DecryptParametersWrapper & operator = (const DecryptParametersWrapper & pDecryptParametersWrapper);

    Status GetSequenceParamSet(H264SeqParamSet *sps);
    Status GetPictureParamSetPart1(H264PicParamSet *pps);
    Status GetPictureParamSetPart2(H264PicParamSet *pps);

    //Status GetNALUnitType(NAL_Unit_Type &nal_unit_type, Ipp32u &nal_ref_idc);
    Status GetSliceHeaderPart1(H264SliceHeader *pSliceHeader);
    Status GetSliceHeaderPart2(H264SliceHeader *pSliceHeader,
                               const H264PicParamSet *pps,
                               const H264SeqParamSet *sps);
    Status GetSliceHeaderPart3(H264SliceHeader *pSliceHeader,
                               PredWeightTable *pPredWeight_L0,
                               PredWeightTable *pPredWeight_L1,
                               RefPicListReorderInfo *pReorderInfo_L0,
                               RefPicListReorderInfo *pReorderInfo_L1,
                               AdaptiveMarkingInfo *pAdaptiveMarkingInfo,
                               AdaptiveMarkingInfo *pBaseAdaptiveMarkingInfo,
                               const H264PicParamSet *pps,
                               const H264SeqParamSet *sps,
                               const H264SeqParamSetSVCExtension *spsSvcExt);
    Status GetSliceHeaderPart4(H264SliceHeader *hdr,
                                const H264SeqParamSetSVCExtension *spsSvcExt);

    void ParseSEIBufferingPeriod(const Headers & headers, H264SEIPayLoad *spl);
    void ParseSEIPicTiming(const Headers & headers, H264SEIPayLoad *spl);
    void ParseSEIRecoveryPoint(H264SEIPayLoad *spl);

    Ipp64f GetTime() const {return m_pts;}
    void SetTime(Ipp64f pts) {m_pts = pts;}

private:
    Status GetVUIParam(H264SeqParamSet *sps, H264VUI *vui);
    Status GetHRDParam(H264SeqParamSet *sps, h264_hrd_param_set_t *hrd, H264VUI *vui);

    void CopyDecryptParams(const DECRYPT_QUERY_STATUS_PARAMS_AVC & pDecryptParameters);

private:
    Ipp64f m_pts;
};


class WidevineDecrypter
{
public:

    WidevineDecrypter()
        : m_va(0)
    {
#ifdef UMC_VA_DXVA
        m_pDummySurface = NULL;
#endif
    }

    ~WidevineDecrypter()
    {
    }

    void Init()
    {
        m_bitstreamSubmitted = false;
        m_PESPacketCounter = 0;
        if (!m_va)
            return;
    }

    void Reset()
    {
        m_bitstreamSubmitted = false;
        m_PESPacketCounter = 0;
    }

    void SetVideoHardwareAccelerator(VideoAccelerator * va)
    {
        if (va)
            m_va = (VideoAccelerator*)va;
    }

    Status DecryptFrame(MediaData *pSource, DecryptParametersWrapper* pDecryptParams);

    void ReleaseForNewBitstream()
    {
        m_bitstreamSubmitted = false;
    }

protected:
    VideoAccelerator *m_va;
    bool m_bitstreamSubmitted;
    Ipp16u m_PESPacketCounter;

#ifdef UMC_VA_DXVA
    IDirect3DSurface9* m_pDummySurface;
#endif
};

} // namespace UMC

#endif /* __UMC_H264_WIDEVINE_DECRYPTER_H */
#endif // UMC_ENABLE_H264_VIDEO_DECODER
