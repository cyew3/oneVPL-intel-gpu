/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_MFX_SUPPLIER_H
#define __UMC_H265_MFX_SUPPLIER_H

//#define UMC_RESTRICTED_CODE_MFX

#ifndef UMC_RESTRICTED_CODE_MFX
#include "vm_thread.h"
//#include "mfx_h265_ex_param_buf.h"
#include "umc_h265_task_supplier.h"
#include "umc_media_data_ex.h"

#include "umc_h265_task_broker.h"
#include "mfxvideo++int.h"

class VideoDECODEH265;

namespace UMC_HEVC_DECODER
{

class RawHeader_H265
{
public:

    RawHeader_H265();

    void Reset();

    Ipp32s GetID() const;

    size_t GetSize() const;

    Ipp8u * GetPointer();

    void Resize(Ipp32s id, size_t newSize);

protected:
    typedef std::vector<Ipp8u> BufferType;
    BufferType  m_buffer;
    Ipp32s      m_id;
};

class RawHeaders_H265
{
public:

    void Reset();

    RawHeader_H265 * GetSPS();

    RawHeader_H265 * GetPPS();

protected:

    RawHeader_H265 m_sps;
    RawHeader_H265 m_pps;
};

/****************************************************************************************************/
// TaskSupplier_H265
/****************************************************************************************************/
class MFXTaskSupplier_H265 : public TaskSupplier_H265, public RawHeaders_H265
{
    friend class ::VideoDECODEH265;

public:

    MFXTaskSupplier_H265();

    virtual ~MFXTaskSupplier_H265();

    virtual UMC::Status Init(UMC::BaseCodecParams *pInit);

    virtual void Reset();

    virtual void CompleteFrame(H265DecoderFrame * pFrame);

    bool CheckDecoding(bool should_additional_check, H265DecoderFrame * decoded);

    void SetVideoParams(mfxVideoParam * par);

protected:

    virtual UMC::Status DecodeSEI(UMC::MediaDataEx *nalUnit);

    virtual void AddFakeReferenceFrame(H265Slice * pSlice);

    virtual UMC::Status DecodeHeaders(UMC::MediaDataEx *nalUnit);

    mfxStatus RunThread(mfxU32 threadNumber);

    mfxVideoParam  m_firstVideoParams;

private:
    MFXTaskSupplier_H265 & operator = (MFXTaskSupplier_H265 &)
    {
        return *this;

    } // MFXTaskSupplier_H265 & operator = (MFXTaskSupplier_H265 &)
};

class MFX_Utility
{
public:

    static eMFXPlatform MFX_CDECL GetPlatform_H265(VideoCORE * core, mfxVideoParam * par);
    static UMC::Status MFX_CDECL FillVideoParam(TaskSupplier_H265 * supplier, ::mfxVideoParam *par, bool full, bool isHW);
    static UMC::Status MFX_CDECL DecodeHeader(TaskSupplier_H265 * supplier, UMC::BaseCodecParams* params, mfxBitstream *bs, mfxVideoParam *out, bool isHW);

    static mfxStatus MFX_CDECL Query_H265(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out, eMFXHWType type);
    static bool MFX_CDECL CheckVideoParam_H265(mfxVideoParam *in, eMFXHWType type);

private:

    static bool IsNeedPartialAcceleration_H265(mfxVideoParam * par, eMFXHWType type);

};

} // namespace UMC_HEVC_DECODER

#endif // UMC_RESTRICTED_CODE_MFX

#endif // __UMC_H264_MFX_SUPPLIER_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
