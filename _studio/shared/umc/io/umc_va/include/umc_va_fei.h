//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_VA_FEI_H__
#define __UMC_VA_FEI_H__

#ifdef UMC_VA_LINUX

#include "umc_va_linux.h"

#define VA_DECODE_STREAM_OUT_ENABLE       2
#define VADecodeStreamOutDataBufferType  -8

#include <list>

namespace UMC
{
    class VAStreamOutBuffer
        : public VACompBuffer
    {

    public:

        VAStreamOutBuffer()
            : m_remap_refs(false)
            , m_field(0)
            , m_allowed_max_mbs_in_slice(0)
        {}

        void RemapRefs(bool remap)
        { m_remap_refs = remap; }
        bool RemapRefs() const
        { return m_remap_refs; }

        void BindToField(Ipp32s field)
        { m_field = field; }
        Ipp32s GetField() const
        { return m_field; }

        void FillPicReferences(VAPictureParameterBufferH264 const*);
        void FillSliceReferences(VASliceParameterBufferH264 const*);

        void RemapReferences(void*, Ipp32s);

    private:

        bool           m_remap_refs;
        Ipp32s         m_field;

        Ipp16u         m_allowed_max_mbs_in_slice;
        VAPictureH264  m_references[16];

        //map [Slice::first_mb_in_slice] onto its Ref. lists
        typedef std::list<std::pair<Ipp16u, std::vector<Ipp32u> > > slice_map;
        slice_map      m_slice_map;
    };

    class FEIVideoAccelerator
        : public LinuxVideoAccelerator
    {
        DYNAMIC_CAST_DECL(FEIVideoAccelerator, LinuxVideoAccelerator);

    public:

        FEIVideoAccelerator();
        ~FEIVideoAccelerator();

        Status Init(VideoAcceleratorParams*);
        Status Close();

        Status Execute();
        void ReleaseBuffer(VAStreamOutBuffer*);
        void* GetCompBuffer(Ipp32s buffer_type, UMCVACompBuffer **buf, Ipp32s size, Ipp32s index);
        Status SyncTask(Ipp32s index, void * error = NULL);

        VAStreamOutBuffer* QueryStreamOutBuffer(Ipp32s index, Ipp32s field);

    private:

        Status MapStreamOutBuffer(VAStreamOutBuffer*);

        VAStreamOutBuffer*               m_streamOutBuffer;
        std::vector<VAStreamOutBuffer*>  m_streamOutCache;
    };
} // namespace UMC

#endif // #ifdef UMC_VA_LINUX

#endif // #ifndef __UMC_VA_FEI_H__
