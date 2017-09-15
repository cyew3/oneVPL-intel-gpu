//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_SAO_H__
#define __UMC_H265_SAO_H__

#include "umc_structures.h"

namespace UMC_HEVC_DECODER
{
class H265DecoderFrame;

// SAO constants
enum SAOTypeLen
{
    SAO_OFFSETS_LEN = 4,
    SAO_MAX_BO_CLASSES = 32
};

// SAO modes
enum SAOType
{
    SAO_EO_0 = 0,
    SAO_EO_1,
    SAO_EO_2,
    SAO_EO_3,
    SAO_BO,
    MAX_NUM_SAO_TYPE
};

#pragma pack(1)

// SAO CTB parameters
struct SAOLCUParam
{
    struct
    {
        Ipp8u sao_merge_up_flag : 1;
        Ipp8u sao_merge_left_flag : 1;
    };

    Ipp8s m_typeIdx[2];
    Ipp8s m_subTypeIdx[3];
    Ipp32s m_offset[3][4];
};

#pragma pack()

// SAO abstract functionality
class H265SampleAdaptiveOffsetBase
{
public:
    virtual ~H265SampleAdaptiveOffsetBase()
    {
    }

    virtual bool isNeedReInit(const H265SeqParamSet* sps) = 0;
    virtual void init(const H265SeqParamSet* sps) = 0;
    virtual void destroy() = 0;

    virtual void SAOProcess(H265DecoderFrame* pFrame, Ipp32s startCU, Ipp32s toProcessCU) = 0;
};

// SAO functionality and state
template<typename PlaneType>
class H265SampleAdaptiveOffsetTemplate : public H265SampleAdaptiveOffsetBase
{
public:
    H265SampleAdaptiveOffsetTemplate();

    virtual ~H265SampleAdaptiveOffsetTemplate()
    {
        destroy();
    }

    // Returns whethher it is necessary to reallocate SAO context
    bool isNeedReInit(const H265SeqParamSet* sps);
    // SAO context initalization
    virtual void init(const H265SeqParamSet* sps);
    // SAO context data structure deallocator
    virtual void destroy();

    // Apply SAO filter to a frame
    virtual void SAOProcess(H265DecoderFrame* pFrame, Ipp32s startCU, Ipp32s toProcessCU);

protected:
    H265DecoderFrame*   m_Frame;
    const H265SeqParamSet* m_sps;

    Ipp32s              m_OffsetEo[LUMA_GROUP_NUM];
    Ipp32s              m_OffsetEo2[LUMA_GROUP_NUM];
    Ipp32s              m_OffsetEoChroma[LUMA_GROUP_NUM];
    Ipp32s              m_OffsetEo2Chroma[LUMA_GROUP_NUM];
    PlaneType       *m_OffsetBo;
    PlaneType       *m_OffsetBoChroma;
    PlaneType       *m_OffsetBo2Chroma;
    PlaneType       *m_lumaTableBo;
    PlaneType       *m_chromaTableBo;

    PlaneType *m_ClipTable;
    PlaneType *m_ClipTableChroma;
    PlaneType *m_ClipTableBase;

    PlaneType *m_TmpU[2];
    PlaneType *m_TmpL[2];

    PlaneType *m_tempPCMBuffer;

    Ipp32u              m_PicWidth;
    Ipp32u              m_PicHeight;
    Ipp32u              m_chroma_format_idc;
    Ipp32u              m_bitdepth_luma;
    Ipp32u              m_bitdepth_chroma;
    Ipp32u              m_MaxCUSize;
    Ipp32u              m_SaoBitIncreaseY, m_SaoBitIncreaseC;
    bool                m_UseNIF;
    bool                m_needPCMRestoration;
    bool                m_slice_sao_luma_flag;
    bool                m_slice_sao_chroma_flag;

    bool                m_isInitialized;

    // Calculate SAO lookup tables for luma offsets from bitstream data
    void SetOffsetsLuma(SAOLCUParam &saoLCUParam, Ipp32s typeIdx);
    // Calculate SAO lookup tables for chroma offsets from bitstream data
    void SetOffsetsChroma(SAOLCUParam &saoLCUParamCb, Ipp32s typeIdx);

    // Calculate whether it is necessary to check slice and tile boundaries because of
    // filtering restrictions in some slice of the frame.
    void createNonDBFilterInfo();
    // Restore parts of CTB encoded with PCM or transquant bypass if filtering should be disabled for them
    void PCMCURestoration(H265CodingUnit* pcCU, Ipp32u AbsZorderIdx, Ipp32u Depth, bool restore);
    // Save/restore losless coded samples from temporary buffer back to frame
    void PCMSampleRestoration(H265CodingUnit* pcCU, Ipp32u AbsZorderIdx, Ipp32u Depth, bool restore);

    // Apply SAO filtering to NV12 chroma plane. Filter is enabled across slices and
    // tiles so only frame boundaries are taken into account.
    // HEVC spec 8.7.3.
    void processSaoCuOrgChroma(Ipp32s Addr, Ipp32s PartIdx, PlaneType *tmpL);
    // Apply SAO filtering to NV12 chroma plane. Filter may be disabled across slices or
    // tiles so neighbour availability has to be checked for every CTB.
    // HEVC spec 8.7.3.
    void processSaoCuChroma(Ipp32s addr, Ipp32s saoType, PlaneType *tmpL);
    // Apply SAO filter to a range of CTBs
    void processSaoUnits(Ipp32s first, Ipp32s toProcess);
    // Apply SAO filter to a line or part of line of CTBs in a frame
    void processSaoLine(SAOLCUParam* saoLCUParam, Ipp32s firstCU, Ipp32s lastCU);
};

// SAO interface class
class H265SampleAdaptiveOffset
{
public:
    H265SampleAdaptiveOffset();
    virtual ~H265SampleAdaptiveOffset(void) { };

    // Initialize SAO context
    virtual void init(const H265SeqParamSet* sps);
    // SAO context cleanup
    virtual void destroy();

    // Apply SAO filter to a target frame CTB range
    void SAOProcess(H265DecoderFrame* pFrame, Ipp32s start, Ipp32s toProcess);

protected:
    H265SampleAdaptiveOffsetBase * m_base;

    bool m_isInitialized;
    bool m_is10bits;
};


} // end namespace UMC_HEVC_DECODER


#endif // __UMC_H265_SAO_H__
#endif // UMC_ENABLE_H265_VIDEO_DECODER
