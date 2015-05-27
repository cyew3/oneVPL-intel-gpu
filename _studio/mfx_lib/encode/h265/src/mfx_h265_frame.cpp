//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <memory>
#include "ippi.h"
#include "ippvc.h"
#include "vm_file.h"
#include "mfx_h265_frame.h"
#include "mfx_h265_enc.h"

#ifndef MFX_VA
#define H265FEI_AllocateInputSurface(...) (MFX_ERR_NONE)
#define H265FEI_AllocateOutputSurface(...) (MFX_ERR_NONE)
#define H265FEI_FreeSurface(...) (MFX_ERR_NONE)
#define CM_ALIGNED_MALLOC(...) ((void *)NULL)
#define CM_ALIGNED_FREE(...)
#endif

namespace H265Enc {

#define ALIGN_VALUE 32
    // template to align a pointer
    template<class T> inline
        T align_pointer(void *pv, size_t lAlignValue = UMC::DEFAULT_ALIGN_VALUE)
    {
        // some compilers complain to conversion to/from
        // pointer types from/to integral types.
        return (T) ((((Ipp8u *) pv - (Ipp8u *) 0) + (lAlignValue - 1)) &
            ~(lAlignValue - 1));
    }


    void FrameData::Create(const FrameData::AllocInfo &allocInfo)
    {
        width = allocInfo.width;
        height = allocInfo.height;
        padding = allocInfo.padding;

        Ipp32s bdLumaFlag = allocInfo.bitDepthLu > 8;
        Ipp32s bdChromaFlag = allocInfo.bitDepthCh > 8;
        Ipp32s chromaPaddingW = padding;
        Ipp32s chromaPaddingH = padding;

        pitch_luma_bytes = pitch_luma_pix = width + padding * 2;

        Ipp32s plane_size_luma = (width + padding * 2) * (height + padding * 2);
        Ipp32s plane_size_chroma;

        switch (allocInfo.chromaFormat) {
        case MFX_CHROMAFORMAT_YUV422:
            plane_size_chroma = plane_size_luma;
            pitch_chroma_pix = pitch_luma_pix;
            break;
        case MFX_CHROMAFORMAT_YUV444:
            plane_size_chroma = plane_size_luma << 1;
            pitch_chroma_pix = pitch_luma_pix << 1;
            chromaPaddingW <<= 1;
            break;
        case MFX_CHROMAFORMAT_YUV400:
            plane_size_chroma = 0;
            pitch_chroma_pix = 0;
            break;
        default:
        case MFX_CHROMAFORMAT_YUV420:
            plane_size_chroma = plane_size_luma >> 1;
            pitch_chroma_pix = pitch_luma_pix;
            chromaPaddingH >>= 1;
            break;
        }

        pitch_chroma_bytes = pitch_chroma_pix;

        if (bdLumaFlag) {
            plane_size_luma <<= 1;
            pitch_luma_bytes <<= 1;
        }
        if (bdChromaFlag) {
            plane_size_chroma <<= 1;
            pitch_chroma_bytes <<= 1;
        }

        // workarround from PKoval
        plane_size_luma += 128;
        plane_size_chroma += 128;

        Ipp32s len = (plane_size_luma + plane_size_chroma) + ALIGN_VALUE * 3;

        mem = new mfxU8[len];

        y = align_pointer<Ipp8u *> (mem, ALIGN_VALUE);
        uv = align_pointer<Ipp8u *> (y + plane_size_luma, ALIGN_VALUE);

        y += ((pitch_luma_pix + 1) * padding) << bdLumaFlag;
        uv += (pitch_chroma_pix * chromaPaddingH + chromaPaddingW) << bdChromaFlag;
    }
    
    void FrameData::Destroy()
    {
        delete [] (mfxU8 *)mem;
        mem = NULL;
    }

    void Statistics::Create(const Statistics::AllocInfo &allocInfo)
    {
        // all algorithms designed for 8x8 (CsRs - 4x4)
        Ipp32s width  = allocInfo.width;
        Ipp32s height = allocInfo.height;
        Ipp32s blkSize = SIZE_BLK_LA;
        Ipp32s PicWidthInBlk  = (width  + blkSize - 1) / blkSize;
        Ipp32s PicHeightInBlk = (height + blkSize - 1) / blkSize;
        Ipp32s numBlk = PicWidthInBlk * PicHeightInBlk;

        // for BRC
        m_intraSatd.resize(numBlk);
        m_interSatd.resize(numBlk);

        // for content analysis (paq/calq)
        m_interSad.resize(numBlk);

        m_interSad_pdist_past.resize(numBlk);
        m_interSad_pdist_future.resize(numBlk);

        H265MV initMv = {0,0};
        m_mv.resize(numBlk, initMv);
        m_mv_pdist_past.resize(numBlk, initMv);
        m_mv_pdist_future.resize(numBlk, initMv);

        size_t len = width * height >> 4;// because RsCs configured for 4x4 blk only
        m_rs.resize(len, 0);
        m_cs.resize(len, 0);

        sc_mask.resize(numBlk, 0);
        qp_mask.resize(numBlk, 0);
        coloc_futr.resize(numBlk, 0);
        coloc_past.resize(numBlk, 0);

        ResetAvgMetrics();
    }

    void Statistics::Destroy()
    {
        Ipp32s numBlk = 0;
        Ipp32s len = 0;
        m_intraSatd.resize(numBlk);
        m_interSatd.resize(numBlk);
        m_interSad.resize(numBlk);
        m_interSad_pdist_past.resize(numBlk);
        m_interSad_pdist_future.resize(numBlk);
        m_mv.resize(numBlk);
        m_mv_pdist_past.resize(numBlk);
        m_mv_pdist_future.resize(numBlk);
        m_rs.resize(len);
        m_cs.resize(len);
        sc_mask.resize(numBlk);
        qp_mask.resize(numBlk);
        coloc_futr.resize(numBlk);
        coloc_past.resize(numBlk);
    }


    void FeiInData::Create(const FeiInData::AllocInfo &allocInfo)
    {
        if (H265FEI_AllocateInputSurface(allocInfo.feiHdl, &m_handle) != MFX_ERR_NONE)
            Throw(std::runtime_error("H265FEI_AllocateInputSurface failed"));

        m_fei = allocInfo.feiHdl;
    }

    void FeiInData::Destroy()
    {
        if (m_fei && m_handle) {
            H265FEI_FreeSurface(m_fei, m_handle);
            m_handle = NULL;
            m_fei = NULL;
        }
    }

    void FeiOutData::Create(const FeiOutData::AllocInfo &allocInfo)
    {
        const mfxSurfInfoENC &info = allocInfo.allocInfo;

        m_sysmem = (mfxU8 *)CM_ALIGNED_MALLOC(info.size, info.align);
        if (!m_sysmem) {
            Throw(std::runtime_error("CM_ALIGNED_MALLOC failed"));
        }

        if (H265FEI_AllocateOutputSurface(allocInfo.feiHdl, m_sysmem, const_cast<mfxSurfInfoENC *>(&info), &m_handle) != MFX_ERR_NONE) {
            CM_ALIGNED_FREE(m_sysmem);
            m_sysmem = NULL;
            Throw(std::runtime_error("H265FEI_AllocateOutputSurface failed"));
        }

        m_pitch = (Ipp32s)info.pitch;
        m_fei = allocInfo.feiHdl;
    }

    void FeiOutData::Destroy()
    {
        if (m_fei && m_handle && m_sysmem) {
            H265FEI_FreeSurface(m_fei, m_handle);
            CM_ALIGNED_FREE(m_sysmem);
            m_sysmem = NULL;
            m_handle = NULL;
            m_fei = NULL;
        }
    }

    void Frame::Create(H265VideoParam *par)
    {
        Ipp32s numCtbs = par->PicWidthInCtbs * par->PicHeightInCtbs;
        m_lcuQps.resize(numCtbs);
        m_bitDepthLuma =  par->bitDepthLuma;
        m_bitDepthChroma = par->bitDepthChroma;
        m_bdLumaFlag = par->bitDepthLuma > 8;
        m_bdChromaFlag = par->bitDepthChroma > 8;
        m_chromaFormatIdc = par->chromaFormatIdc;

        m_slices.resize(par->NumSlices);

        Ipp32s numCtbs_parts = numCtbs << par->Log2NumPartInCU;
        Ipp32s len = numCtbs_parts * sizeof(H265CUData) + ALIGN_VALUE;
        len += sizeof(ThreadingTask) * 2 * numCtbs + ALIGN_VALUE;

        mem = H265_Malloc(len);
        if (!mem)
            throw std::exception();

        Ipp8u *ptr = (Ipp8u*)mem;
        cu_data = align_pointer<H265CUData *> (ptr, ALIGN_VALUE);
        ptr += numCtbs_parts * sizeof(H265CUData) + ALIGN_VALUE;
        m_threadingTasks = align_pointer<ThreadingTask *> (ptr, ALIGN_VALUE);
    }

    inline static void memset_bd(Ipp8u *dst, Ipp32s val, Ipp32s size_pix, Ipp8u bdFlag) {
        if (bdFlag) {
            ippsSet_16s(val, (Ipp16s *)dst, size_pix);
        } else {
            ippsSet_8u((Ipp8u)val, dst, size_pix);
        }
    }

    IppStatus ippsCopy(const Ipp8u *src, Ipp8u *dst, Ipp32s len)   { return ippsCopy_8u(src, dst, len); }
    IppStatus ippsCopy(const Ipp16u *src, Ipp16u *dst, Ipp32s len) { return ippsCopy_16s((const Ipp16s *)src, (Ipp16s *)dst, len); }
    IppStatus ippsCopy(const Ipp32u *src, Ipp32u *dst, Ipp32s len) { return ippsCopy_32s((const Ipp32s *)src, (Ipp32s *)dst, len); }

    template <class PixType> void PadOnePix(PixType *frame, Ipp32s pitch, Ipp32s width, Ipp32s height)
    {
        PixType *ptr = frame;
        for (Ipp32s i = 0; i < height; i++, ptr += pitch) {
            ptr[-1] = ptr[0];
            ptr[width] = ptr[width - 1];
        }

        ptr = frame - 1;
        ippsCopy(ptr, ptr - pitch, width + 2);
        ptr = frame + height * pitch - 1;
        ippsCopy(ptr - pitch, ptr, width + 2);
    }
    
    void Frame::CopyFrameData(const mfxFrameSurface1 *in, Ipp8u have8bitCopy)
    {
        FrameData* out = m_origin;
        IppiSize roi = { out->width, out->height };
        mfxU16 InputBitDepthLuma = 8;
        mfxU16 InputBitDepthChroma = 8;

        if (in->Info.FourCC == MFX_FOURCC_P010 || in->Info.FourCC == MFX_FOURCC_P210) {
            InputBitDepthLuma = 10;
            InputBitDepthChroma = 10;
        }

        switch ((m_bdLumaFlag << 1) | (InputBitDepthLuma > 8)) {
        case 0:
            ippiCopy_8u_C1R(in->Data.Y, in->Data.Pitch, out->y, out->pitch_luma_bytes, roi);
            break;
        case 1:
            ippiConvert_16u8u_C1R((Ipp16u*)in->Data.Y, in->Data.Pitch, out->y, out->pitch_luma_bytes, roi);
            break;
        case 2:
            ippiConvert_8u16u_C1R(in->Data.Y, in->Data.Pitch, (Ipp16u*)out->y, out->pitch_luma_bytes, roi);
            ippiLShiftC_16u_C1IR(m_bitDepthLuma - 8, (Ipp16u*)out->y, out->pitch_luma_bytes, roi);
            break;
        case 3:
            ippiCopy_16u_C1R((Ipp16u*)in->Data.Y, in->Data.Pitch, (Ipp16u*)out->y, out->pitch_luma_bytes, roi);
            if (have8bitCopy)
                h265_ConvertShiftR((const Ipp16s *)out->y, out->pitch_luma_pix, m_luma_8bit->y, m_luma_8bit->pitch_luma_pix, roi.width, roi.height, 2);
            break;
        default:
            VM_ASSERT(0);
        }

        roi.width <<= (m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV444);
        roi.height >>= (m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420);

        switch ((m_bdChromaFlag << 1) | (InputBitDepthChroma > 8)) {
        case 0:
            ippiCopy_8u_C1R(in->Data.UV, in->Data.Pitch, out->uv, out->pitch_chroma_bytes, roi);
            break;
        case 1:
            ippiConvert_16u8u_C1R((Ipp16u*)in->Data.UV, in->Data.Pitch, out->uv, out->pitch_chroma_bytes, roi);
            break;
        case 2:
            ippiConvert_8u16u_C1R(in->Data.UV, in->Data.Pitch, (Ipp16u*)out->uv, out->pitch_chroma_bytes, roi);
            ippiLShiftC_16u_C1IR(m_bitDepthChroma - 8, (Ipp16u*)out->uv, out->pitch_chroma_bytes, roi);
            break;
        case 3:
            ippiCopy_16u_C1R((Ipp16u*)in->Data.UV, in->Data.Pitch, (Ipp16u*)out->uv, out->pitch_chroma_bytes, roi);
            break;
        default:
            VM_ASSERT(0);
        }

        // so far needed for gradient analysis only
        (m_bdLumaFlag == 0)
            ? PadOnePix((Ipp8u *)out->y, out->pitch_luma_pix, out->width, out->height)
            : PadOnePix((Ipp16u *)out->y, out->pitch_luma_pix, out->width, out->height);
        (m_bdChromaFlag == 0)
            ? PadOnePix((Ipp16u *)out->uv, out->pitch_chroma_pix / 2, out->width / 2, out->height / 2)
            : PadOnePix((Ipp32u *)out->uv, out->pitch_chroma_pix / 2, out->width / 2, out->height / 2);
    }


    template <class PixType> void PadOneLine_v2(PixType *startLine, Ipp32s pitch, Ipp32s width, Ipp32s lineCount, Ipp32s padding)
    {
        PixType *ptr = startLine;
        Ipp32s leftPos = 0;
        Ipp32s rightPos = width - 1;

        // left/right
        for (Ipp32s i = 0; i < lineCount; i++, ptr += pitch) {

            for(Ipp32s padIdx = 1; padIdx <= padding; padIdx++) {
                ptr[leftPos-padIdx] = ptr[leftPos];
                ptr[rightPos+padIdx] = ptr[rightPos];
            }
        }
    }

    typedef enum {
        PAD_LEFT = 0,
        PAD_RIGHT = 1,
        PAD_BOTH = 2,
        PAD_NONE = 3
    } PaddMode;

    template <class PixType> void PadOneLine_v3(PixType *startLine, Ipp32s pitch, Ipp32s width, Ipp32s lineCount, Ipp32s padding, PaddMode mode)
    {
        PixType *ptr = startLine;
        Ipp32s leftPos = 0;
        Ipp32s rightPos = width - 1;

        if (mode == PAD_LEFT || mode == PAD_BOTH) {
            // left
            for (Ipp32s i = 0; i < lineCount; i++, ptr += pitch) {

                for(Ipp32s padIdx = 1; padIdx <= padding; padIdx++) {
                    ptr[leftPos-padIdx] = ptr[leftPos];
                }
            }
        }
        if (mode == PAD_RIGHT || mode == PAD_BOTH) {
            // right
            for (Ipp32s i = 0; i < lineCount; i++, ptr += pitch) {

                for(Ipp32s padIdx = 1; padIdx <= padding; padIdx++) {
                    ptr[rightPos+padIdx] = ptr[rightPos];
                }
            }
        }
    }

    //IppStatus ippsCopy(const Ipp8u *src, Ipp8u *dst, Ipp32s len)   { return ippsCopy_8u(src, dst, len); }
    //IppStatus ippsCopy(const Ipp16u *src, Ipp16u *dst, Ipp32s len) { return ippsCopy_16s((const Ipp16s *)src, (Ipp16s *)dst, len); }
    //IppStatus ippsCopy(const Ipp32u *src, Ipp32u *dst, Ipp32s len) { return ippsCopy_32s((const Ipp32s *)src, (Ipp32s *)dst, len); }

    template <class PixType> void PadOneLine_Top_v2(PixType *startLine, Ipp32s pitch, Ipp32s width, Ipp32s padding_w, Ipp32s padding_h)
    {
        PixType* ptr = startLine - padding_w;
        for (Ipp32s i = 1; i <= padding_h; i++) {
            ippsCopy(ptr, ptr - i*pitch, width + 2*padding_w);
        }
    }


    template <class PixType> void PadOneLine_Top_v3(PixType *startLine, Ipp32s pitch, Ipp32s width, Ipp32s padding_w, Ipp32s padding_h, PaddMode mode)
    {
        PixType* ptr;

        if (mode == PAD_LEFT || mode == PAD_BOTH)
            ptr = startLine - padding_w;
        else
            ptr = startLine;

        if (mode == PAD_LEFT || mode == PAD_RIGHT)
            width += padding_w;
        else if (mode == PAD_BOTH)
            width += 2*padding_w;

        for (Ipp32s i = 1; i <= padding_h; i++) {
            ippsCopy(ptr, ptr - i*pitch, width);
        }
    }

    template <class PixType> void PadOneLine_Bottom_v2(PixType *startLine, Ipp32s pitch, Ipp32s width, Ipp32s padding_w, Ipp32s padding_h)
    {
        PixType* ptr = startLine - padding_w;

        for (Ipp32s i = 1; i <= padding_h; i++) {
            ippsCopy(ptr, ptr + i*pitch, width + 2*padding_w);
        }
    }

    template <class PixType> void PadOneLine_Bottom_v3(PixType *startLine, Ipp32s pitch, Ipp32s width, Ipp32s padding_w, Ipp32s padding_h, PaddMode mode)
    {
        PixType* ptr;

        if (mode == PAD_LEFT || mode == PAD_BOTH)
            ptr = startLine - padding_w;
        else
            ptr = startLine;

        if (mode == PAD_LEFT || mode == PAD_RIGHT)
            width += padding_w;
        else if (mode == PAD_BOTH)
            width += 2*padding_w;

        for (Ipp32s i = 1; i <= padding_h; i++) {
            ippsCopy(ptr, ptr + i*pitch, width);
        }
    }
    void PadOneReconRow(H265Enc::FrameData* frame, Ipp32u ctb_row, Ipp32u maxCuSize, Ipp32u PicHeightInCtbs, const FrameData::AllocInfo &allocInfo)
    {
        Ipp8u bitDepth8 = (allocInfo.bitDepthLu == 8) ? 1 : 0;
        Ipp32s shift_w = allocInfo.chromaFormat != MFX_CHROMAFORMAT_YUV444 ? 1 : 0;
        Ipp32s shift_h = allocInfo.chromaFormat == MFX_CHROMAFORMAT_YUV420 ? 1 : 0;

        // L/R Padding
        {
            //luma
            Ipp8u *startLine = frame->y + (ctb_row)*frame->pitch_luma_bytes*maxCuSize;
            (bitDepth8)
                ? PadOneLine_v2((Ipp8u*)startLine, frame->pitch_luma_pix, frame->width, maxCuSize, frame->padding)
                : PadOneLine_v2((Ipp16u*)startLine, frame->pitch_luma_pix, frame->width, maxCuSize, frame->padding);

            //Chroma
            //startLine = frame->uv + (ctb_row)*frame->pitch_chroma_bytes* m_videoParam.MaxCUSize/2;
            startLine = frame->uv + (ctb_row)*frame->pitch_chroma_bytes* (maxCuSize >> shift_h);
            (bitDepth8) 
                ? PadOneLine_v2((Ipp16u *)startLine, frame->pitch_chroma_pix/2, frame->width >> shift_w, maxCuSize >> shift_h, frame->padding >> shift_w)
                : PadOneLine_v2((Ipp32u *)startLine, frame->pitch_chroma_pix/2, frame->width >> shift_w, maxCuSize >> shift_h, frame->padding >> shift_w);
        }

        //first line: top padding
        if (ctb_row == 0) {
            Ipp8u *startLine = frame->y;
            (bitDepth8)
                ? PadOneLine_Top_v2((Ipp8u*)startLine, frame->pitch_luma_pix, frame->width, frame->padding, frame->padding)
                : PadOneLine_Top_v2((Ipp16u*)startLine, frame->pitch_luma_pix, frame->width, frame->padding, frame->padding);

            startLine = frame->uv;
            (bitDepth8)
                ? PadOneLine_Top_v2((Ipp16u *)startLine, frame->pitch_chroma_pix/2, frame->width >> shift_w, frame->padding >> shift_w, frame->padding >> shift_h)
                : PadOneLine_Top_v2((Ipp32u *)startLine, frame->pitch_chroma_pix/2, frame->width >> shift_w, frame->padding >> shift_w, frame->padding >> shift_h);
        }

        // last line: bottom padding
        if (ctb_row == PicHeightInCtbs - 1) {
            Ipp8u *startLine = frame->y + (frame->height - 1)*frame->pitch_luma_bytes;
            (bitDepth8)
                ? PadOneLine_Bottom_v2((Ipp8u*)startLine, frame->pitch_luma_pix, frame->width, frame->padding, frame->padding)
                : PadOneLine_Bottom_v2((Ipp16u*)startLine, frame->pitch_luma_pix, frame->width, frame->padding, frame->padding);

            startLine = frame->uv + ((frame->height >> shift_h) - 1)*frame->pitch_chroma_bytes;
            (bitDepth8)
                ? PadOneLine_Bottom_v2((Ipp16u *)startLine, frame->pitch_chroma_pix/2, frame->width >> shift_w, frame->padding >> shift_w, frame->padding >> shift_h)
                : PadOneLine_Bottom_v2((Ipp32u *)startLine, frame->pitch_chroma_pix/2, frame->width >> shift_w, frame->padding >> shift_w, frame->padding >> shift_h);
        }
    }

    void PadOneReconCtu(Frame* frame, Ipp32u ctb_row, Ipp32u ctb_col, Ipp32u maxCuSize, Ipp32u PicHeightInCtbs, Ipp32u PicWidthInCtbs)
    {
        FrameData *reconstructFrame = frame->m_recon;

        Ipp8u bitDepth8 = (frame->m_bitDepthLuma == 8) ? 1 : 0;
        Ipp32s shift_w = frame->m_chromaFormatIdc != MFX_CHROMAFORMAT_YUV444 ? 1 : 0;
        Ipp32s shift_h = frame->m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420 ? 1 : 0;

        PaddMode mode = PicWidthInCtbs == 1 ? PAD_BOTH : ctb_col == 0 ? PAD_LEFT : ctb_col == PicWidthInCtbs - 1 ? PAD_RIGHT : PAD_NONE;

        if (mode != PAD_NONE) {
            // L/R Padding
            //luma
            Ipp8u *startLine = reconstructFrame->y + (ctb_row)*reconstructFrame->pitch_luma_bytes*maxCuSize;
            (bitDepth8)
                ? PadOneLine_v3((Ipp8u*)startLine, reconstructFrame->pitch_luma_pix, reconstructFrame->width, maxCuSize, reconstructFrame->padding, mode)
                : PadOneLine_v3((Ipp16u*)startLine, reconstructFrame->pitch_luma_pix, reconstructFrame->width, maxCuSize, reconstructFrame->padding, mode);

            //Chroma
            //startLine = reconstructFrame->uv + (ctb_row)*reconstructFrame->pitch_chroma_bytes* m_videoParam.MaxCUSize/2;
            startLine = reconstructFrame->uv + (ctb_row)*reconstructFrame->pitch_chroma_bytes* (maxCuSize >> shift_h);
            (bitDepth8) 
                ? PadOneLine_v3((Ipp16u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, maxCuSize >> shift_h, reconstructFrame->padding >> shift_w, mode)
                : PadOneLine_v3((Ipp32u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, maxCuSize >> shift_h, reconstructFrame->padding >> shift_w, mode);
        }

        //first line: top padding
        if (ctb_row == 0) {
            Ipp8u *startLine = reconstructFrame->y;
            Ipp32u width = IPP_MIN(maxCuSize, reconstructFrame->width - ctb_col * maxCuSize);
            (bitDepth8)
                ? PadOneLine_Top_v3((Ipp8u*)startLine + ctb_col * maxCuSize, reconstructFrame->pitch_luma_pix, width, reconstructFrame->padding, reconstructFrame->padding, mode)
                : PadOneLine_Top_v3((Ipp16u*)startLine + ctb_col * maxCuSize, reconstructFrame->pitch_luma_pix, width, reconstructFrame->padding, reconstructFrame->padding, mode);

            startLine = reconstructFrame->uv;
            (bitDepth8)
                ? PadOneLine_Top_v3((Ipp16u *)startLine + ctb_col * (maxCuSize >> shift_w), reconstructFrame->pitch_chroma_pix/2, width >> shift_w, reconstructFrame->padding >> shift_w, reconstructFrame->padding >> shift_h, mode)
                : PadOneLine_Top_v3((Ipp32u *)startLine + ctb_col * (maxCuSize >> shift_w), reconstructFrame->pitch_chroma_pix/2, width >> shift_w, reconstructFrame->padding >> shift_w, reconstructFrame->padding >> shift_h, mode);
        }

        // last line: bottom padding
        if (ctb_row == PicHeightInCtbs - 1) {
            Ipp8u *startLine = reconstructFrame->y + (reconstructFrame->height - 1)*reconstructFrame->pitch_luma_bytes;
            Ipp32u width = IPP_MIN(maxCuSize, reconstructFrame->width - ctb_col * maxCuSize);
            (bitDepth8)
                ? PadOneLine_Bottom_v3((Ipp8u*)startLine + ctb_col * maxCuSize, reconstructFrame->pitch_luma_pix, width, reconstructFrame->padding, reconstructFrame->padding, mode)
                : PadOneLine_Bottom_v3((Ipp16u*)startLine + ctb_col * maxCuSize, reconstructFrame->pitch_luma_pix, width, reconstructFrame->padding, reconstructFrame->padding, mode);

            startLine = reconstructFrame->uv + ((reconstructFrame->height >> shift_h) - 1)*reconstructFrame->pitch_chroma_bytes;
            (bitDepth8)
                ? PadOneLine_Bottom_v3((Ipp16u *)startLine + ctb_col * (maxCuSize >> shift_w), reconstructFrame->pitch_chroma_pix/2, width >> shift_w, reconstructFrame->padding >> shift_w, reconstructFrame->padding >> shift_h, mode)
                : PadOneLine_Bottom_v3((Ipp32u *)startLine + ctb_col * (maxCuSize >> shift_w), reconstructFrame->pitch_chroma_pix/2, width >> shift_w, reconstructFrame->padding >> shift_w, reconstructFrame->padding >> shift_h, mode);
        }
    }

    static void ExpandPlane(Ipp8u elem_shift, Ipp8u *ptr, Ipp32s pitch_bytes, Ipp32s width, Ipp32s height, Ipp32s padding_w, Ipp32s padding_h) {
        Ipp8u *src = ptr;
        Ipp8u *dst = src - pitch_bytes;
        for (Ipp32s y = 0; y < padding_h; y++, dst -= pitch_bytes)
            ippsCopy_8u(src, dst, width<<elem_shift);

        src = ptr + (height - 1) * pitch_bytes;
        dst = src + pitch_bytes;
        for (Ipp32s y = 0; y < padding_h; y++, dst += pitch_bytes)
            ippsCopy_8u(src, dst, width<<elem_shift);

        Ipp8u * p0 = ptr - padding_h * pitch_bytes;
        if (elem_shift == 0) {
            for (Ipp32s y = 0; y < height + padding_h * 2; y++, p0 += pitch_bytes)
                ippsSet_8u(*p0, p0 - padding_w, padding_w);
        } else if (elem_shift == 1) {
            for (Ipp32s y = 0; y < height + padding_h * 2; y++, p0 += pitch_bytes)
                ippsSet_16s(*(Ipp16s*)p0, (Ipp16s*)p0 - padding_w, padding_w);
        } else if (elem_shift == 2) {
            for (Ipp32s y = 0; y < height + padding_h * 2; y++, p0 += pitch_bytes)
                ippsSet_32s(*(Ipp32s*)p0, (Ipp32s*)p0 - padding_w, padding_w);
        }

        p0 = ptr + ((width - 1) << elem_shift) - padding_h * pitch_bytes;
        if (elem_shift == 0) {
            for (Ipp32s y = 0; y < height + padding_h * 2; y++, p0 += pitch_bytes)
                ippsSet_8u(*p0, p0 + 1, padding_w);
        } else if (elem_shift == 1) {
            for (Ipp32s y = 0; y < height + padding_h * 2; y++, p0 += pitch_bytes)
                ippsSet_16s(*(Ipp16s*)p0, (Ipp16s*)p0 + 1, padding_w);
        } else if (elem_shift == 2) {
            for (Ipp32s y = 0; y < height + padding_h * 2; y++, p0 += pitch_bytes)
                ippsSet_32s(*(Ipp32s*)p0, (Ipp32s*)p0 + 1, padding_w);
        }
    }

    void Frame::doPadding()
    {
        /*if (!m_bdLumaFlag) {
        ippiExpandPlane_H264_8u_C1R(y, width, height, pitch_luma_bytes, padding, IPPVC_FRAME);
        } else {
        ExpandPlane(1, y, pitch_luma_bytes, width, height, padding, padding);
        }

        Ipp32s shift_w = m_chromaFormatIdc != MFX_CHROMAFORMAT_YUV444 ? 1 : 0;
        Ipp32s shift_h = m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420 ? 1 : 0;

        if (!m_bdChromaFlag) {
        ExpandPlane(1, uv, pitch_chroma_bytes, width >> shift_w, height >> shift_h, padding >> shift_w, padding >> shift_h);
        } else {
        ExpandPlane(2, uv, pitch_chroma_bytes, width >> shift_w, height >> shift_h, padding >> shift_w, padding >> shift_h);*/
    }

    void Dump(H265VideoParam *par, Frame* frame, FrameList & dpb )
    {
        if (!par->doDumpRecon)
            return;

        vm_char *fname = par->reconDumpFileName;
        Ipp32s frame_num = frame->m_encOrder;
        vm_file *f;
        mfxU8* fbuf = NULL;
        Ipp32s W = par->Width - par->CropLeft - par->CropRight;
        Ipp32s H = par->Height - par->CropTop - par->CropBottom;
        Ipp32s bd_shift_luma = par->bitDepthLuma > 8;
        Ipp32s bd_shift_chroma = par->bitDepthChroma > 8;
        Ipp32s shift_w = frame->m_chromaFormatIdc != MFX_CHROMAFORMAT_YUV444 ? 1 : 0;
        Ipp32s shift_h = frame->m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420 ? 1 : 0;
        Ipp32s shift = 2 - shift_w - shift_h;
        Ipp32s plane_size = (W*H << bd_shift_luma) + (((W*H/2) << shift) << bd_shift_chroma);
        
        Ipp32s numlater = 0; // number of dumped frames with later POC
        if (frame->m_picCodeType == MFX_FRAMETYPE_B) {
            for (FrameIter it = dpb.begin(); it != dpb.end(); it++ ) {
                Frame *pFrm = (*it);
                if ( NULL == (*it)->m_origin && frame->m_poc < pFrm->m_poc)
                    numlater++;
            }
        }
        if ( numlater ) { // simple reorder for B: read last ref, replacing with B, write ref
            f = vm_file_fopen(fname,VM_STRING("r+b"));
            if (!f) return;
            fbuf = new mfxU8[numlater*plane_size];
            vm_file_fseek(f, -numlater*plane_size, VM_FILE_SEEK_END);
            vm_file_fread(fbuf, 1, numlater*plane_size, f);
            vm_file_fseek(f, -numlater*plane_size, VM_FILE_SEEK_END);
        } else {
            f = vm_file_fopen(fname, frame_num ? VM_STRING("a+b") : VM_STRING("wb"));
            if (!f) return;
        }
    
        if (f == NULL)
            return;
    
        FrameData* recon = frame->m_recon;
        int i;
        mfxU8 *p = recon->y + ((par->CropLeft + par->CropTop * recon->pitch_luma_pix) << bd_shift_luma);
        for (i = 0; i < H; i++) {
            vm_file_fwrite(p, 1<<bd_shift_luma, W, f);
            p += recon->pitch_luma_bytes;
        }
        // writing nv12 to yuv420
        // maxlinesize = 4096
        if (W <= 2048*2) { // else invalid dump
            if (bd_shift_chroma) {
                mfxU16 uvbuf[2048];
                mfxU16 *p;
                for (int part = 0; part <= 1; part++) {
                    p = (Ipp16u*)recon->uv + part + (par->CropLeft >> shift_w << 1) + (par->CropTop>>shift_h) * recon->pitch_chroma_pix;
                    for (i = 0; i < H >> shift_h; i++) {
                        for (int j = 0; j < W>>shift_w; j++)
                            uvbuf[j] = p[2*j];
                        vm_file_fwrite(uvbuf, 2, W>>shift_w, f);
                        p += recon->pitch_chroma_pix;
                    }
                }
            } else {
                mfxU8 uvbuf[2048];
                for (int part = 0; part <= 1; part++) {
                    p = recon->uv + part + (par->CropLeft >> shift_w << 1) + (par->CropTop>>shift_h) * recon->pitch_chroma_pix;
                    for (i = 0; i < H >> shift_h; i++) {
                        for (int j = 0; j < W>>shift_w; j++)
                            uvbuf[j] = p[2*j];
                        vm_file_fwrite(uvbuf, 1, W>>shift_w, f);
                        p += recon->pitch_chroma_pix;
                    }
                }
            }
        }
    
        if (fbuf) {
            vm_file_fwrite(fbuf, 1, numlater*plane_size, f);
            delete[] fbuf;
        }
        vm_file_fclose(f);
    }

    void Frame::Destroy()
    {
        if (mem)
            H265_Free(mem);
        mem = NULL;
    }

    void Frame::ResetMemInfo()
    {
        m_origin = NULL;
        m_recon = NULL;
        m_luma_8bit = NULL;
        m_lowres = NULL;
        cu_data = NULL;
        m_stats[0] = m_stats[1] = NULL;

        mem = NULL;
    }

    void Frame::ResetEncInfo()
    {
        m_timeStamp = 0;
        m_picCodeType = 0;
        m_RPSIndex = 0;

        m_wasLookAheadProcessed = 0;
        m_lookaheadRefCounter = 0;

        m_pyramidLayer = 0;
        m_miniGopCount = 0;
        m_biFramesInMiniGop = 0;
        m_frameOrder = 0;
        m_frameOrderOfLastIdr = 0;
        m_frameOrderOfLastIntra = 0;
        m_frameOrderOfLastIntraInEncOrder = 0;
        m_frameOrderOfLastAnchor = 0;
        m_poc = 0;
        m_encOrder = 0;
        m_isShortTermRef = 0;
        m_isLongTermRef = 0;
        m_isIdrPic = 0;
        m_isRef = 0;
        memset(m_refPicList, 0, sizeof(m_refPicList));
        memset(m_mapRefIdxL1ToL0, 0, sizeof(m_mapRefIdxL1ToL0));
        m_allRefFramesAreFromThePast = 0;

        m_origin = NULL;
        m_recon  = NULL;
        m_lowres = NULL;
        m_encOrder    = Ipp32u(-1);
        m_frameOrder  = Ipp32u(-1);
        m_timeStamp   = 0;
        m_encIdx      = -1;
        m_dpbSize     = 0;

        if (m_stats[0]) {
            m_stats[0]->ResetAvgMetrics();
        }
        if (m_stats[1]) {
            m_stats[1]->ResetAvgMetrics();
        }

        m_futureFrames.resize(0);

        m_feiSyncPoint = NULL;
        m_feiOrigin = NULL;
        m_feiRecon = NULL;
        Zero(m_feiIntraAngModes);
        Zero(m_feiInterMv);
        Zero(m_feiInterDist);

        m_userSeiMessages = NULL;
        m_numUserSeiMessages = 0;

        m_ttEncComplete.InitEncComplete(0);
        m_ttInitNewFrame.InitNewFrame(this, (mfxFrameSurface1 *)NULL, 0);
        m_ttSubmitGpuCopySrc.InitGpuSubmit(this, MFX_FEI_H265_OP_GPU_COPY_SRC, 0);
        m_ttSubmitGpuCopyRec.InitGpuSubmit(this, MFX_FEI_H265_OP_GPU_COPY_REF, 0);
        m_ttSubmitGpuIntra.InitGpuSubmit(this, MFX_FEI_H265_OP_INTRA_MODE, 0);
        for (Ipp32s i = 0; i < 4; i++)
            m_ttSubmitGpuMe[i].InitGpuSubmit(this, MFX_FEI_H265_OP_INTER_ME, 0);
        m_ttWaitGpuIntra.InitGpuWait(MFX_FEI_H265_OP_INTRA_MODE, 0);
        for (Ipp32s i = 0; i < 4; i++)
            m_ttWaitGpuMe[i].InitGpuWait(MFX_FEI_H265_OP_INTER_ME, 0);
    }

    void Frame::ResetCounters()
    {
        m_codedRow = 0;
    }
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
