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

#ifndef __UMC_H264_MFX_SUPPLIER_H
#define __UMC_H264_MFX_SUPPLIER_H

//#include "mfx_h264_ex_param_buf.h"
#include "umc_h264_task_supplier.h"
#include "umc_media_data_ex.h"
#include "umc_h264_task_broker.h"
#include "mfxvideo++int.h"

class VideoDECODEH264;

namespace UMC
{

class RawHeader
{
public:

    RawHeader();

    void Reset();

    Ipp32s GetID() const;

    size_t GetSize() const;

    Ipp8u * GetPointer();

    void Resize(Ipp32s id, size_t newSize);

#ifdef __APPLE__
    size_t  GetRBSPSize();
    void  SetRBSPSize(size_t rbspSize);
#endif 

protected:
    typedef std::vector<Ipp8u> BufferType;
    BufferType  m_buffer;
    Ipp32s      m_id;
#ifdef __APPLE__
    size_t      m_rbspSize;
#endif
};

class RawHeaders
{
public:

    void Reset();

    RawHeader * GetSPS();

    RawHeader * GetPPS();

protected:

    RawHeader m_sps;
    RawHeader m_pps;
};

/****************************************************************************************************/
// TaskSupplier
/****************************************************************************************************/
class MFXTaskSupplier : public TaskSupplier, public RawHeaders
{
    friend class ::VideoDECODEH264;

public:

    MFXTaskSupplier();

    virtual ~MFXTaskSupplier();

    virtual Status Init(VideoDecoderParams *pInit);

    virtual void Reset();

    virtual Status CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field);

    virtual bool ProcessNonPairedField(H264DecoderFrame * pFrame);

    bool CheckDecoding(bool should_additional_check, H264DecoderFrame * decoded);

    void SetVideoParams(mfxVideoParam * par);

protected:

    virtual Status DecodeSEI(MediaDataEx *nalUnit);

    virtual void AddFakeReferenceFrame(H264Slice * pSlice);

    virtual Status DecodeHeaders(MediaDataEx *nalUnit);

    mfxStatus RunThread(mfxU32 threadNumber);

    mfxVideoParam  m_firstVideoParams;

private:
    MFXTaskSupplier & operator = (MFXTaskSupplier &)
    {
        return *this;
    } // MFXTaskSupplier & operator = (MFXTaskSupplier &)
};

inline Ipp32u ExtractProfile(mfxU32 profile)
{
    return profile & 0xFF;
}

inline bool isMVCProfile(mfxU32 profile)
{
    return (profile == MFX_PROFILE_AVC_MULTIVIEW_HIGH || profile == MFX_PROFILE_AVC_STEREO_HIGH);
}

inline bool isSVCProfile(mfxU32 profile)
{
    return (profile == MFX_PROFILE_AVC_SCALABLE_BASELINE || profile == MFX_PROFILE_AVC_SCALABLE_HIGH);
}

} // namespace UMC


class MFX_Utility
{
public:

    static eMFXPlatform GetPlatform(VideoCORE * core, mfxVideoParam * par);
    static UMC::Status FillVideoParam(UMC::TaskSupplier * supplier, ::mfxVideoParam *par, bool full);
    static UMC::Status FillVideoParamMVCEx(UMC::TaskSupplier * supplier, ::mfxVideoParam *par);
    static UMC::Status DecodeHeader(UMC::TaskSupplier * supplier, UMC::H264VideoDecoderParams* params, mfxBitstream *bs, mfxVideoParam *out);

    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out, eMFXHWType type);
    static bool CheckVideoParam(mfxVideoParam *in, eMFXHWType type);

private:

    static bool IsNeedPartialAcceleration(mfxVideoParam * par, eMFXHWType type);

};


#endif // __UMC_H264_MFX_SUPPLIER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
