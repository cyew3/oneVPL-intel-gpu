// Copyright (c) 2012-2019 Intel Corporation
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

#include "umc_defs.h"
#ifdef MFX_ENABLE_H265_VIDEO_DECODE

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
        uint8_t sao_merge_up_flag : 1;
        uint8_t sao_merge_left_flag : 1;
    };

    int8_t m_typeIdx[2];
    int8_t m_subTypeIdx[3];
    int32_t m_offset[3][4];
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

    virtual void SAOProcess(H265DecoderFrame* pFrame, int32_t startCU, int32_t toProcessCU) = 0;
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
    virtual void SAOProcess(H265DecoderFrame* pFrame, int32_t startCU, int32_t toProcessCU);

protected:
    H265DecoderFrame*   m_Frame;
    const H265SeqParamSet* m_sps;

    int32_t              m_OffsetEo[LUMA_GROUP_NUM];
    int32_t              m_OffsetEo2[LUMA_GROUP_NUM];
    int32_t              m_OffsetEoChroma[LUMA_GROUP_NUM];
    int32_t              m_OffsetEo2Chroma[LUMA_GROUP_NUM];
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

    uint32_t              m_PicWidth;
    uint32_t              m_PicHeight;
    uint32_t              m_chroma_format_idc;
    uint32_t              m_bitdepth_luma;
    uint32_t              m_bitdepth_chroma;
    uint32_t              m_MaxCUSize;
    uint32_t              m_SaoBitIncreaseY, m_SaoBitIncreaseC;
    bool                m_UseNIF;
    bool                m_needPCMRestoration;
    bool                m_slice_sao_luma_flag;
    bool                m_slice_sao_chroma_flag;

    bool                m_isInitialized;

    // Calculate SAO lookup tables for luma offsets from bitstream data
    void SetOffsetsLuma(SAOLCUParam &saoLCUParam, int32_t typeIdx);
    // Calculate SAO lookup tables for chroma offsets from bitstream data
    void SetOffsetsChroma(SAOLCUParam &saoLCUParamCb, int32_t typeIdx);

    // Calculate whether it is necessary to check slice and tile boundaries because of
    // filtering restrictions in some slice of the frame.
    void createNonDBFilterInfo();
    // Restore parts of CTB encoded with PCM or transquant bypass if filtering should be disabled for them
    void PCMCURestoration(H265CodingUnit* pcCU, uint32_t AbsZorderIdx, uint32_t Depth, bool restore);
    // Save/restore losless coded samples from temporary buffer back to frame
    void PCMSampleRestoration(H265CodingUnit* pcCU, uint32_t AbsZorderIdx, uint32_t Depth, bool restore);

    // Apply SAO filtering to NV12 chroma plane. Filter is enabled across slices and
    // tiles so only frame boundaries are taken into account.
    // HEVC spec 8.7.3.
    void processSaoCuOrgChroma(int32_t Addr, int32_t PartIdx, PlaneType *tmpL);
    // Apply SAO filtering to NV12 chroma plane. Filter may be disabled across slices or
    // tiles so neighbour availability has to be checked for every CTB.
    // HEVC spec 8.7.3.
    void processSaoCuChroma(int32_t addr, int32_t saoType, PlaneType *tmpL);
    // Apply SAO filter to a range of CTBs
    void processSaoUnits(int32_t first, int32_t toProcess);
    // Apply SAO filter to a line or part of line of CTBs in a frame
    void processSaoLine(SAOLCUParam* saoLCUParam, int32_t firstCU, int32_t lastCU);
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
    void SAOProcess(H265DecoderFrame* pFrame, int32_t start, int32_t toProcess);

protected:
    H265SampleAdaptiveOffsetBase * m_base;

    bool m_isInitialized;
    bool m_is10bits;
};


} // end namespace UMC_HEVC_DECODER


#endif // __UMC_H265_SAO_H__
#endif // MFX_ENABLE_H265_VIDEO_DECODE
