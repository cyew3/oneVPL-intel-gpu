//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_AU_SPLITTER_H
#define __UMC_H264_AU_SPLITTER_H

#include <vector>
#include "umc_h264_dec_defs_dec.h"
#include "umc_media_data_ex.h"
#include "umc_h264_heap.h"
#include "umc_h264_slice_decoding.h"
#include "umc_h264_frame_info.h"

#include "umc_h264_headers.h"

namespace UMC
{
class NALUnitSplitter;

/****************************************************************************************************/
// SeiPayloadArray class routine
/****************************************************************************************************/
class SeiPayloadArray
{
public:
    SeiPayloadArray();
    SeiPayloadArray(const SeiPayloadArray & payloads);

    ~SeiPayloadArray();

    void Release();

    void AddPayload(H264SEIPayLoad*);

    void MovePayloadsFrom(SeiPayloadArray &messages);

    H264SEIPayLoad* GetPayload(size_t pos) const;
    size_t GetPayloadCount() const;

    H264SEIPayLoad* FindPayload(SEI_TYPE type) const;

protected:
    typedef std::vector<H264SEIPayLoad*> PayloadArray;
    PayloadArray m_payloads;

    Ipp32s FindPayloadPos(SEI_TYPE type) const;
};

/****************************************************************************************************/
// SetOfSlices class routine
/****************************************************************************************************/
class SetOfSlices
{
public:
    SetOfSlices();
    ~SetOfSlices();

    SetOfSlices(const SetOfSlices& set);

    H264Slice * GetSlice(size_t pos) const;
    size_t GetSliceCount() const;
    void AddSlice(H264Slice * slice);
    void Release();
    void Reset();
    void SortSlices();
    void CleanUseless();

    void AddSet(const SetOfSlices *set);

    size_t GetViewId() const;

    H264DecoderFrame * m_frame;
    bool m_isCompleted;
    bool m_isFull;

    SeiPayloadArray  m_payloads;

    SetOfSlices& operator=(const SetOfSlices& set);

protected:
    std::vector<H264Slice*> m_pSliceQueue;
};

/****************************************************************************************************/
// AccessUnit class routine
/****************************************************************************************************/
class AccessUnit
{
public:

    AccessUnit();
    virtual ~AccessUnit();

    bool AddSlice(H264Slice * slice);

    void CombineSets();

    void Release();

    void Reset();

    bool IsItSliceOfThisAU(H264Slice * slice);

    size_t GetLayersCount() const;
    SetOfSlices * GetLayer(size_t pos);
    SetOfSlices * GetLastLayer();

    void CompleteLastLayer();

    Ipp32s FindLayerByDependency(Ipp32s dependency);

    Ipp32u GetAUIndentifier() const;

    void SortforASO();

    void CleanUseless();

    bool IsFullAU() const;

    bool m_isInitialized;

    SeiPayloadArray  m_payloads;

protected:
    SetOfSlices * GetLayerBySlice(H264Slice * slice);
    SetOfSlices * AddLayer(H264Slice * slice);

    std::vector<SetOfSlices>  m_layers;

    bool m_isFullAU;

    Ipp32u m_auCounter;
};

class AU_Splitter
{
public:
    AU_Splitter(H264_Heap_Objects *objectHeap);
    virtual ~AU_Splitter();

    void Init();
    void Close();

    void Reset();

    MediaDataEx * GetNalUnit(MediaData * src);

    NALUnitSplitter * GetNalUnitSplitter();

protected:

    Headers     m_Headers;
    H264_Heap_Objects   *m_objHeap;

protected:

    //Status DecodeHeaders(MediaDataEx *nalUnit);

    std::auto_ptr<NALUnitSplitter> m_pNALSplitter;
};

} // namespace UMC

#endif // __UMC_H264_AU_SPLITTER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
