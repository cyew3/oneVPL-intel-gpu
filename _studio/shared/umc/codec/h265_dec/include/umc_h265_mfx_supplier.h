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

#ifndef __UMC_H265_MFX_SUPPLIER_H
#define __UMC_H265_MFX_SUPPLIER_H

#include "vm_thread.h"
#include "umc_h265_task_supplier.h"
#include "umc_media_data_ex.h"

#include "umc_h265_task_broker.h"
#include "mfxvideo++int.h"

class VideoDECODEH265;

namespace UMC_HEVC_DECODER
{

// Container for raw header binary data
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

// Container for raw SPS and PPS stream headers
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
// Task supplier which implements MediaSDK decoding API
/****************************************************************************************************/
class MFXTaskSupplier_H265 : public TaskSupplier_H265, public RawHeaders_H265
{
    friend class ::VideoDECODEH265;

public:

    MFXTaskSupplier_H265();

    virtual ~MFXTaskSupplier_H265();

    // Initialize task supplier
    virtual UMC::Status Init(UMC::VideoDecoderParams *pInit);

    using TaskSupplier_H265::Reset;

    // Check whether all slices for the frame were found
    virtual void CompleteFrame(H265DecoderFrame * pFrame);

    // Check whether specified frame has been decoded, and if it was,
    // whether there is some decoding work left to be done
    bool CheckDecoding(bool should_additional_check, H265DecoderFrame * decoded);

    // Set initial video params from application
    void SetVideoParams(mfxVideoParam * par);

    // Initialize mfxVideoParam structure based on decoded bitstream header values
    UMC::Status FillVideoParam(mfxVideoParam *par, bool full);

protected:

    // Decode SEI nal unit
    virtual UMC::Status DecodeSEI(UMC::MediaDataEx *nalUnit);

    // Do something in case reference frame is missing
    virtual void AddFakeReferenceFrame(H265Slice * pSlice);

    // Decode headers nal unit
    virtual UMC::Status DecodeHeaders(UMC::MediaDataEx *nalUnit);

    // Perform decoding task for thread number threadNumber
    mfxStatus RunThread(mfxU32 threadNumber);

    mfxVideoParam  m_firstVideoParams;

private:
    MFXTaskSupplier_H265 & operator = (MFXTaskSupplier_H265 &)
    {
        return *this;

    } // MFXTaskSupplier_H265 & operator = (MFXTaskSupplier_H265 &)
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_MFX_SUPPLIER_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
