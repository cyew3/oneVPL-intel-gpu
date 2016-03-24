/*
 *                  INTEL CORPORATION PROPRIETARY INFORMATION
 *     This software is supplied under the terms of a license agreement or
 *     nondisclosure agreement with Intel Corporation and may not be copied
 *     or disclosed except in accordance with the terms of that agreement.
 *          Copyright(c) 2006-2016 Intel Corporation. All Rights Reserved.
 *
 */

#include <umc_va_base.h>

#ifdef UMC_VA_LINUX

#include "umc_va_fei.h"
#include "umc_automatic_mutex.h"
#include "umc_frame_allocator.h"

#include "mfxfei.h"
#include "mfx_trace.h"

#include <algorithm>

#define UMC_VA_DECODE_STREAM_OUT_ENABLE  2

namespace UMC
{
    inline
    bool check_supported(VADisplay display, VAContextID ctx)
    {
        VABufferID id;
        VAStatus res =
            vaCreateBuffer(display, ctx, static_cast<VABufferType>(VADecodeStreamOutDataBufferType), sizeof(mfxFeiDecStreamOutMBCtrl) * 4096, 1, NULL, &id);
        if (res != VA_STATUS_SUCCESS)
            return false;

        vaDestroyBuffer(display, id);
        return true;
    }

    inline
    bool find_slice_mb(std::pair<Ipp16u, std::vector<Ipp32u> > const& l, std::pair<Ipp16u, std::vector<Ipp32u> > const& r)
    {
        return
            l.first < r.first;
    }

    inline
    Ipp32u map_slice_ref(Ipp32u const* refs, Ipp32u count, Ipp32u frame)
    {
        Ipp32u const* r = std::find(refs, refs + count, frame);

        //use count * 2 (top + bottom) to signal 'not found'
        return
            r != refs + count ? Ipp32s(r - refs) : count * 2;
    }

    void VAStreamOutBuffer::FillPicReferences(VAPictureParameterBufferH264 const* pp)
    {
        VM_ASSERT(m_remap_refs);

        //we do x2 for interlaced content during SPS parsing, scale it down here
        m_allowed_max_mbs_in_slice =
            (pp->picture_width_in_mbs_minus1 + 1) * ((pp->picture_height_in_mbs_minus1 + 1) >> pp->pic_fields.bits.field_pic_flag);

        Ipp32s const count = sizeof(m_references) / sizeof(m_references[0]);
        for (Ipp32u i = 0; i < count; ++i)
            m_references[i] = pp->ReferenceFrames[i].frame_idx;
    }

    void VAStreamOutBuffer::FillSliceReferences(VASliceParameterBufferH264 const* sp)
    {
        VM_ASSERT(m_remap_refs);

        //skip INTRASLICE & S_INTRASLICE
        if ((sp->slice_type % 5) == 2 ||
            (sp->slice_type % 5) == 4)
            return;

        //NOTE: we keep [slice_map] sorted implicitly
        slice_map::iterator s =
            std::lower_bound(m_slice_map.begin(), m_slice_map.end(),
                             std::make_pair(sp->first_mb_in_slice, std::vector<Ipp32u>()),
                             find_slice_mb);

        if (s != m_slice_map.end())
            //there is this slice in map, due chopping for e.g.
            return;

        m_slice_map.push_back(std::make_pair(sp->first_mb_in_slice, std::vector<Ipp32u>()));
        std::vector<Ipp32u>& slice_refs = m_slice_map.back().second;

        Ipp32u const count = sizeof(m_references) / sizeof(m_references[0]);
        //we use single array for both top/bottom & L0/L1
        //use one additional slot in table to place all invalid indices
        //layout: [ |L0 top|L0 bot|I||L1 top|L1 bot|I| ]
        slice_refs.resize((count * 2 + 1) * 2, 0);

        //L0
        Ipp32u* map = &slice_refs[0];
        for (Ipp32u i = 0; i <= sp->num_ref_idx_l0_active_minus1; ++i)
        {
            VAPictureH264 const& pic = sp->RefPicList0[i];
            Ipp32u const idx =
                map_slice_ref(m_references, count, pic.frame_idx);

            Ipp32u const bottom = !!(pic.flags & VA_PICTURE_H264_BOTTOM_FIELD);
            map[idx + count * bottom] = i;
        }

        if ((sp->slice_type % 5) != 1)
            return;

        //L1
        map = &slice_refs[count * 2 + 1];
        for (Ipp32u i = 0; i <= sp->num_ref_idx_l1_active_minus1; ++i)
        {
            VAPictureH264 const& pic = sp->RefPicList1[i];
            Ipp32u const idx =
                map_slice_ref(m_references, count, pic.frame_idx);

            Ipp32u const bottom = !!(pic.flags & VA_PICTURE_H264_BOTTOM_FIELD);
            map[idx + count * bottom] = i;
        }
    }

    void VAStreamOutBuffer::RemapReferences(void* data, Ipp32s size)
    {
        //InterMB.RefIdx explained:
        //Bit 7: Must Be One (1 - indicate it is in Frame store ID format, 0 - indicate it is in Reference Index format)
        //Bit 6:5: reserved MBZ
        //Bit 4:0: Frame store index or Frame Store ID (Bit 4:1 is used to form the binding table index in intel implementation)

        mfxFeiDecStreamOutMBCtrl* mb
            = reinterpret_cast<mfxFeiDecStreamOutMBCtrl*>(data);
        Ipp32s const mb_total = size / sizeof(mfxFeiDecStreamOutMBCtrl);
        Ipp32u const count = sizeof(m_references) / sizeof(m_references[0]);

        Ipp32s mb_processed = 0;
        slice_map::iterator
            f = m_slice_map.begin(),
            l = m_slice_map.end();
        for (; f != l; ++f)
        {
            slice_map::iterator n = f; ++n;
            
            Ipp32s const mb_per_slice_count =
                ((n != l ? (*n).first : m_allowed_max_mbs_in_slice)) - (*f).first;

            mb_processed += mb_per_slice_count;
            if (mb_processed > mb_total)
            {
                VM_ASSERT(FALSE);
                break;
            }

            mfxFeiDecStreamOutMBCtrl const* mb_end = mb + mb_per_slice_count;
            for (; mb != mb_end; ++mb)
            {
                if (mb->IntraMbFlag)
                    continue;

                int const l_count =
                    2;
                    //(sp->slice_type % 5) == 1;
                for (int j = 0; j < l_count; ++j)
                {
                    //select L0/L1
                    Ipp32u const offset =
                        (count * 2 + 1) * j;

                    Ipp32u const* map = &((*f).second[offset]);
                    for (int k = 0; k < 4; ++k)
                    {
                        Ipp32u const field = (mb->InterMB.RefIdx[j][k] & 0x1);
                        Ipp32u const idx   = (mb->InterMB.RefIdx[j][k] & 0x1e) >> 1;
                        mb->InterMB.RefIdx[j][k] =  map[count * field + idx];
                    }
                }
            }
        }
    }

    FEIVideoAccelerator::FEIVideoAccelerator()
        : m_streamOutBuffer(0)
    {}

    FEIVideoAccelerator::~FEIVideoAccelerator()
    {
        Close();
    }

    Status FEIVideoAccelerator::Init(VideoAcceleratorParams* params)
    {
        VM_ASSERT(GetExtBuffer(param->ExtParam, param->NumExtParam, MFX_EXTBUFF_FEI_PARAM));
        VM_ASSERT(params->m_CreateFlags & VA_DECODE_STREAM_OUT_ENABLE);

        Status sts = LinuxVideoAccelerator::Init(params);
        if (sts != UMC_OK)
            return sts;

        return
            check_supported(m_dpy, *m_pContext) ? UMC_OK : UMC_ERR_UNSUPPORTED;
    }

    Status FEIVideoAccelerator::Close()
    {
        if (m_streamOutBuffer)
        {
            VABufferID const id = m_streamOutBuffer->GetID();
            vaUnmapBuffer(m_dpy, id);
            vaDestroyBuffer(m_dpy, id);

            UMC_DELETE(m_streamOutBuffer);
        }

        while (!m_streamOutCache.empty())
        {
            VAStreamOutBuffer* buffer = m_streamOutCache.back();
            VM_ASSERT(buffer != m_streamOutBuffer && "Executed SO buffer should not exist in cache");

            ReleaseBuffer(buffer);
        }

        return
            LinuxVideoAccelerator::Close();
    }

    Status FEIVideoAccelerator::Execute()
    {
        VM_ASSERT(m_streamOutBuffer);
        if (!m_streamOutBuffer)
            return UMC_ERR_FAILED;

        // Save some info used for StreamOut [RefIdx] remapping
        if (m_streamOutBuffer->RemapRefs())
        {
            Ipp32s const index = m_streamOutBuffer->GetIndex();
            //use [PicParams] buffer to fetch reference frames
            VACompBuffer* buffer;
            GetCompBuffer(VAPictureParameterBufferType, reinterpret_cast<UMCVACompBuffer**>(&buffer), 0, -1);
            VM_ASSERT(buffer && "PicParam buffer should exist here");
            if (!buffer || !buffer->GetPtr())
                return UMC_ERR_FAILED;

            VAPictureParameterBufferH264 const* pp
                = reinterpret_cast<VAPictureParameterBufferH264*>(buffer->GetPtr());

            m_streamOutBuffer->FillPicReferences(pp);

            GetCompBuffer(VASliceParameterBufferType, reinterpret_cast<UMCVACompBuffer**>(&buffer), 0, -1);
            VM_ASSERT(buffer && "PicParam buffer should exist here");
            if (!buffer || !buffer->GetPtr())
                return UMC_ERR_FAILED;

            Ipp32s const slice_count = buffer->GetNumOfItem();
            VASliceParameterBufferH264 const* sp
                = reinterpret_cast<VASliceParameterBufferH264*>(buffer->GetPtr());

            for (VASliceParameterBufferH264 const* sp_end = sp + slice_count; sp != sp_end; ++sp)
                m_streamOutBuffer->FillSliceReferences(sp);
        }

        Status sts = LinuxVideoAccelerator::Execute();
        if (sts != UMC_OK)
            return sts;

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "fei: Execute");

        AutomaticMutex l(m_SyncMutex);

        // StreamOut buffer is not handled by the base class, we should execute it here
        {
            VM_ASSERT(m_streamOutBuffer);

            VABufferID id = m_streamOutBuffer->GetID();
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaRenderPicture");
            VAStatus va_res = vaRenderPicture(m_dpy, *m_pContext, &id, 1);
            if (va_res != VA_STATUS_SUCCESS)
                return va_to_umc_res(va_res);
        }

        m_streamOutCache.push_back(m_streamOutBuffer);
        m_streamOutBuffer = NULL;

        return UMC_OK;
    }

    void FEIVideoAccelerator::ReleaseBuffer(VAStreamOutBuffer* buffer)
    {
        AutomaticMutex l(m_SyncMutex);

        VABufferID const id = buffer->GetID();
        vaUnmapBuffer(m_dpy, id);
        vaDestroyBuffer(m_dpy, id);

        std::vector<VAStreamOutBuffer*>::iterator
            i = std::find(m_streamOutCache.begin(), m_streamOutCache.end(), buffer);
        VM_ASSERT(i != m_streamOutCache.end());
        m_streamOutCache.erase(i);

        UMC_DELETE(buffer);
    }

    void* FEIVideoAccelerator::GetCompBuffer(Ipp32s type, UMCVACompBuffer **buf, Ipp32s size, Ipp32s index)
    {
        if (type != VADecodeStreamOutDataBufferType)
            return LinuxVideoAccelerator::GetCompBuffer(type, buf, size, index);
        else
        {
            VABufferType va_type         = (VABufferType)type;
            unsigned int va_size         = size;
            unsigned int va_num_elements = 1;

            AutomaticMutex l(m_SyncMutex);

            VABufferID id;
            VAStatus va_res =
                vaCreateBuffer(m_dpy, *m_pContext, va_type, va_size, va_num_elements, NULL, &id);

            if (va_res != VA_STATUS_SUCCESS)
                return NULL;

            VM_ASSERT(!m_streamOutBuffer);
            m_streamOutBuffer = new VAStreamOutBuffer();
            m_streamOutBuffer->SetBufferPointer(0, va_size * va_num_elements);
            m_streamOutBuffer->SetDataSize(0);
            m_streamOutBuffer->SetBufferInfo(type, id, index);

            if (buf)
                *buf = m_streamOutBuffer;
        }

        return NULL;
    }

    Status FEIVideoAccelerator::SyncTask(Ipp32s index, void* error)
    {
        Status umcRes = LinuxVideoAccelerator::SyncTask(index, error);
        if (umcRes != UMC_OK)
            return umcRes;

        AutomaticMutex l(m_SyncMutex);

        VAStatus va_res;
        VAImage image;

        //lates driver drop doesn't require frame mapping before to map SO
        //these code parts should be removed later
        if (0)
        {
            //we should map frame data befor SO buffer will be mapped.
            VASurfaceID* surface;
            umcRes = m_allocator->GetFrameHandle(index, &surface);
            if (umcRes != UMC_OK)
                return umcRes;

            va_res = vaDeriveImage(m_dpy, *surface, &image);
            if (va_res != VA_STATUS_SUCCESS)
                return va_to_umc_res(va_res);

            Ipp8u* ptr = NULL;
            va_res = vaMapBuffer(m_dpy, image.buf, (void**)&ptr);
            if (va_res != VA_STATUS_SUCCESS)
                return va_to_umc_res(va_res);

            if (!ptr)
                return UMC_ERR_FAILED;
        }

        for (int i = 0; i < 2; ++i)
        {
            VAStreamOutBuffer* streamOut = QueryStreamOutBuffer(index, i);
            if (streamOut)
            {
                umcRes = MapStreamOutBuffer(streamOut);
                if (umcRes != UMC_OK)
                    break;

                void* ptr = streamOut->GetPtr();
                VM_ASSERT(ptr);

                Ipp32s const size = streamOut->GetDataSize();
                VM_ASSERT(size = streamOut->GetBufferSize());

                if (streamOut->RemapRefs())
                    streamOut->RemapReferences(ptr, size);

//#define  UMC_VA_STREAMOUT_DUMP
#ifdef UMC_VA_STREAMOUT_DUMP
                static FILE* dump_ = fopen("dump.so", "wb");
                fwrite(ptr, streamOut->GetDataSize(), 1, dump_);
                fflush(dump_);
#endif
            }
        }

        if (0)
        {
            va_res = vaUnmapBuffer(m_dpy, image.buf);
            if (va_res != VA_STATUS_SUCCESS)
                umcRes = va_to_umc_res(va_res);


            va_res = vaDestroyImage(m_dpy, image.image_id);
            if (umcRes != UMC_OK)
                return umcRes;

            if (va_res != VA_STATUS_SUCCESS)
                umcRes = va_to_umc_res(va_res);
        }

        return umcRes;
    }

    VAStreamOutBuffer* FEIVideoAccelerator::QueryStreamOutBuffer(Ipp32s index, Ipp32s field)
    {
        std::vector<VAStreamOutBuffer*>::const_iterator
            f = m_streamOutCache.begin(),
            l = m_streamOutCache.end();
        for (; f != l; ++f)
        {
            if ((*f)->GetIndex() == index &&
                (*f)->GetField() == field)
                break;
        }

        return
            (f != l) ? *f : NULL;
    }

    Status FEIVideoAccelerator::MapStreamOutBuffer(VAStreamOutBuffer* buffer)
    {
        Ipp8u* ptr = NULL;
        VAStatus va_res = vaMapBuffer(m_dpy, buffer->GetID(), (void**)&ptr);
        Status umcRes = va_to_umc_res(va_res);
        if (umcRes == UMC_OK)
        {
            Ipp32s const size = buffer->GetBufferSize();
            buffer->SetBufferPointer(ptr, size);
            buffer->SetDataSize(size);
        }

        return umcRes;
    }
} // namespace UMC

#endif // UMC_VA_LINUX
