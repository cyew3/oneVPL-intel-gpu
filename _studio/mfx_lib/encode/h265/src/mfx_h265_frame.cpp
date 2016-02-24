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
#include "mfx_h265_lookahead.h"

#ifndef MFX_VA
#define H265FEI_AllocateSurfaceUp(...) (MFX_ERR_NONE)
#define H265FEI_AllocateInputSurface(...) (MFX_ERR_NONE)
#define H265FEI_AllocateReconSurface(...) (MFX_ERR_NONE)
#define H265FEI_AllocateOutputSurface(...) (MFX_ERR_NONE)
#define H265FEI_AllocateOutputBuffer(...) (MFX_ERR_NONE)
#define H265FEI_FreeSurface(...) (MFX_ERR_NONE)
#define CM_ALIGNED_MALLOC(...) ((void *)NULL)
#define CM_ALIGNED_FREE(...)
#endif

namespace H265Enc {

    using namespace MfxEnumShortAliases;

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
        Ipp32s bdShiftLu = allocInfo.bitDepthLu > 8;
        Ipp32s bdShiftCh = allocInfo.bitDepthCh > 8;

        width = allocInfo.width;
        height = allocInfo.height;
        padding = allocInfo.paddingLu;
        pitch_luma_bytes = allocInfo.pitchInBytesLu;
        pitch_chroma_bytes = allocInfo.pitchInBytesCh;
        pitch_luma_pix = allocInfo.pitchInBytesLu >> bdShiftLu;
        pitch_chroma_pix = allocInfo.pitchInBytesCh >> bdShiftCh;

        Ipp32s size = allocInfo.sizeInBytesLu + allocInfo.sizeInBytesCh + allocInfo.alignment * 2;

        Ipp32s vertPaddingSizeInBytesLu = allocInfo.paddingLu * allocInfo.pitchInBytesLu;
        Ipp32s vertPaddingSizeInBytesCh = allocInfo.paddingChH * allocInfo.pitchInBytesCh;

        mem = new Ipp8u[size];

        y = align_pointer<Ipp8u *>(mem + vertPaddingSizeInBytesLu, allocInfo.alignment);
        uv = y + allocInfo.sizeInBytesLu - vertPaddingSizeInBytesLu + vertPaddingSizeInBytesCh;
        uv = align_pointer<Ipp8u *>(uv, allocInfo.alignment);
        y += (allocInfo.paddingLu << bdShiftLu);
        uv += (allocInfo.paddingChW << bdShiftCh);

        if (allocInfo.feiHdl) {
            m_fei = allocInfo.feiHdl;
            mfxStatus sts = H265FEI_AllocateSurfaceUp(allocInfo.feiHdl, y, uv, &m_handle);
            if (sts != MFX_ERR_NONE)
                Throw(std::runtime_error("H265FEI_Allocate[Input/Recon]Surface failed"));
        }
    }


    void FrameData::Destroy()
    {
        if (m_fei && m_handle) {
            H265FEI_FreeSurface(m_fei, m_handle);
            m_handle = NULL;
            m_fei = NULL;
        }

        delete [] mem;
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

        Ipp32s alignedWidth = ((width+63)&~63);
        Ipp32s alignedFrameSize = alignedWidth * ((height+63)&~63);
        m_pitchRsCs4 = alignedWidth>>2;
        for (Ipp32s log2BlkSize = 2; log2BlkSize <= 6; log2BlkSize ++) {
            m_rs[log2BlkSize-2].resize(alignedFrameSize>>(log2BlkSize<<1), 0);
            m_cs[log2BlkSize-2].resize(alignedFrameSize>>(log2BlkSize<<1), 0);
        }

        rscs_ctb.resize(numBlk, 0);
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
        for (Ipp32s log2BlkSize = 2; log2BlkSize <= 6; log2BlkSize ++) {
            m_rs[log2BlkSize-2].resize(0);
            m_cs[log2BlkSize-2].resize(0);
        }
        rscs_ctb.resize(numBlk);
        sc_mask.resize(numBlk);
        qp_mask.resize(numBlk);
        coloc_futr.resize(numBlk);
        coloc_past.resize(numBlk);
    }


    void FeiInputData::Create(const FeiInputData::AllocInfo &allocInfo)
    {
        if (H265FEI_AllocateInputSurface(allocInfo.feiHdl, &m_handle) != MFX_ERR_NONE)
            Throw(std::runtime_error("H265FEI_AllocateInputSurface failed"));
        m_fei = allocInfo.feiHdl;
    }


    void FeiInputData::Destroy()
    {
        if (m_fei && m_handle) {
            H265FEI_FreeSurface(m_fei, m_handle);
            m_fei = NULL;
            m_handle == NULL;
        }
    }


    void FeiReconData::Create(const FeiReconData::AllocInfo &allocInfo)
    {
        if (H265FEI_AllocateReconSurface(allocInfo.feiHdl, &m_handle) != MFX_ERR_NONE)
            Throw(std::runtime_error("H265FEI_AllocateReconSurface failed"));
        m_fei = allocInfo.feiHdl;
    }


    void FeiReconData::Destroy()
    {
        if (m_fei && m_handle) {
            H265FEI_FreeSurface(m_fei, m_handle);
            m_fei = NULL;
            m_handle == NULL;
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

    void FeiBufferUp::Create(const AllocInfo &allocInfo)
    {
        m_allocated = new mfxU8[allocInfo.size + allocInfo.alignment];
        m_sysmem = align_pointer<Ipp8u*>(m_allocated, allocInfo.alignment);
        if (allocInfo.feiHdl) {
            if (H265FEI_AllocateOutputBuffer(allocInfo.feiHdl, m_sysmem, allocInfo.size, &m_handle) != MFX_ERR_NONE) {
                delete [] m_allocated;
                m_allocated = NULL;
                m_sysmem = NULL;
                Throw(std::runtime_error("H265FEI_AllocateOutputBuffer failed"));
            }
            m_fei = allocInfo.feiHdl;
        }
    }

    void FeiBufferUp::Destroy()
    {
        if (m_fei && m_handle) {
            H265FEI_FreeSurface(m_fei, m_handle);
            m_handle = NULL;
            m_fei = NULL;
        }
        delete [] m_allocated;
        m_allocated = NULL;
        m_sysmem = NULL;
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
        Ipp32s len = sizeof(ThreadingTask) * 2 * numCtbs + ALIGN_VALUE;

        mem = H265_Malloc(len);
        if (!mem)
            throw std::exception();

        Ipp8u *ptr = (Ipp8u*)mem;
        m_threadingTasks = align_pointer<ThreadingTask *> (ptr, ALIGN_VALUE);

        // if lookahead
        Ipp32s numTasks, regionCount, lowresRowsInRegion, originRowsInRegion;
        GetLookaheadGranularity(*par, regionCount, lowresRowsInRegion, originRowsInRegion, numTasks);

        m_ttLookahead.resize( numTasks );
        //Build_ttGraph(0);
    }

    void Frame::CopyFrameData(const mfxFrameSurface1 *in)
    {
        FrameData* out = m_origin;
        IppiSize roi = { out->width, out->height };
        mfxU16 InputBitDepthLuma = 8;
        mfxU16 InputBitDepthChroma = 8;

        if (in->Info.FourCC == MFX_FOURCC_P010 || in->Info.FourCC == MFX_FOURCC_P210) {
            InputBitDepthLuma = 10;
            InputBitDepthChroma = 10;
        }

        Ipp32s inPitch = in->Data.Pitch;
        Ipp8u *inDataY = in->Data.Y;
        Ipp8u *inDataUV = in->Data.UV;
        if (m_picStruct != PROGR) {
            inPitch *= 2;
            if (m_bottomFieldFlag) {
                inDataY += in->Data.Pitch;
                inDataUV += in->Data.Pitch;
            }
        }

        switch ((m_bdLumaFlag << 1) | (InputBitDepthLuma > 8)) {
        case 0:
            ippiCopy_8u_C1R(inDataY, inPitch, out->y, out->pitch_luma_bytes, roi);
            break;
        case 1:
            ippiConvert_16u8u_C1R((Ipp16u*)inDataY, inPitch, out->y, out->pitch_luma_bytes, roi);
            break;
        case 2:
            ippiConvert_8u16u_C1R(inDataY, inPitch, (Ipp16u*)out->y, out->pitch_luma_bytes, roi);
            ippiLShiftC_16u_C1IR(m_bitDepthLuma - 8, (Ipp16u*)out->y, out->pitch_luma_bytes, roi);
            break;
        case 3:
            ippiCopy_16u_C1R((Ipp16u*)inDataY, inPitch, (Ipp16u*)out->y, out->pitch_luma_bytes, roi);
            break;
        default:
            VM_ASSERT(0);
        }

        roi.width <<= (m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV444);
        roi.height >>= (m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420);

        switch ((m_bdChromaFlag << 1) | (InputBitDepthChroma > 8)) {
        case 0:
            ippiCopy_8u_C1R(inDataUV, inPitch, out->uv, out->pitch_chroma_bytes, roi);
            break;
        case 1:
            ippiConvert_16u8u_C1R((Ipp16u*)inDataUV, inPitch, out->uv, out->pitch_chroma_bytes, roi);
            break;
        case 2:
            ippiConvert_8u16u_C1R(inDataUV, inPitch, (Ipp16u*)out->uv, out->pitch_chroma_bytes, roi);
            ippiLShiftC_16u_C1IR(m_bitDepthChroma - 8, (Ipp16u*)out->uv, out->pitch_chroma_bytes, roi);
            break;
        case 3:
            ippiCopy_16u_C1R((Ipp16u*)inDataUV, inPitch, (Ipp16u*)out->uv, out->pitch_chroma_bytes, roi);
            break;
        default:
            VM_ASSERT(0);
        }
    }


    void ippsCopy(const Ipp8u *src, Ipp8u *dst, Ipp32s len)   { (void)ippsCopy_8u(src, dst, len); }
    void ippsCopy(const Ipp16u *src, Ipp16u *dst, Ipp32s len) { (void)ippsCopy_16s((const Ipp16s *)src, (Ipp16s *)dst, len); }
    void ippsCopy(const Ipp32u *src, Ipp32u *dst, Ipp32s len) { (void)ippsCopy_32s((const Ipp32s *)src, (Ipp32s *)dst, len); }
    void ippsSet(Ipp8u  value, Ipp8u  *dst, Ipp32s len) { (void)ippsSet_8u(value, dst, len); };
    void ippsSet(Ipp16u value, Ipp16u *dst, Ipp32s len) { (void)ippsSet_16s((Ipp16s)value, (Ipp16s*)dst, len); };
    void ippsSet(Ipp32u value, Ipp32u *dst, Ipp32s len) { (void)ippsSet_32s((Ipp32s)value, (Ipp32s*)dst, len); };

    template <class T> void PadRect(T *plane, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth, Ipp32s padw, Ipp32s padh)
    {
        rectx = Saturate(0, width - 1, rectx);
        recty = Saturate(0, height - 1, recty);
        rectw = Saturate(1, width - rectx, rectw);
        recth = Saturate(1, height - recty, recth);

        if (rectx == 0) {
            rectx -= padw;
            rectw += padw;
            for (Ipp32s y = recty; y < recty + recth; y++)
                ippsSet(plane[y * pitch], plane + y * pitch - padw, padw);
        }
        if (rectx + rectw == width) {
            rectw += padw;
            for (Ipp32s y = recty; y < recty + recth; y++)
                ippsSet(plane[y * pitch + width - 1], plane + y * pitch + width, padw);
        }
        if (recty == 0)
            for (Ipp32s j = 1; j <= padh; j++)
                ippsCopy(plane + rectx, plane + rectx - j * pitch, rectw);
        if (recty + recth == height)
            for (Ipp32s j = 1; j <= padh; j++)
                ippsCopy(plane + (height - 1) * pitch + rectx, plane + (height - 1 + j) * pitch + rectx, rectw);
    }
    template void PadRect<Ipp8u>(Ipp8u *plane, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth, Ipp32s padw, Ipp32s padh);
    template void PadRect<Ipp16u>(Ipp16u *plane, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth, Ipp32s padw, Ipp32s padh);
    template void PadRect<Ipp32u>(Ipp32u *plane, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth, Ipp32s padw, Ipp32s padh);

    void PadRectLuma(const FrameData &fdata, Ipp32s fourcc, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth)
    {
        (fourcc == NV12 || fourcc == NV16)
            ? PadRect((Ipp8u *)fdata.y, fdata.pitch_luma_pix, fdata.width, fdata.height, rectx, recty, rectw, recth, fdata.padding, fdata.padding)
            : PadRect((Ipp16u*)fdata.y, fdata.pitch_luma_pix, fdata.width, fdata.height, rectx, recty, rectw, recth, fdata.padding, fdata.padding);
    }

    void PadRectChroma(const FrameData &fdata, Ipp32s fourcc, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth)
    {
        Ipp32s shiftH = (fourcc == NV12 || fourcc == P010 ) ? 1 : 0;
        (fourcc == NV12 || fourcc == NV16)
            ? PadRect((Ipp16u *)fdata.uv, fdata.pitch_chroma_pix/2, fdata.width/2, fdata.height>>shiftH, rectx/2, recty>>shiftH, rectw/2, recth>>shiftH, fdata.padding/2, fdata.padding>>shiftH)
            : PadRect((Ipp32u *)fdata.uv, fdata.pitch_chroma_pix/2, fdata.width/2, fdata.height>>shiftH, rectx/2, recty>>shiftH, rectw/2, recth>>shiftH, fdata.padding/2, fdata.padding>>shiftH);
    }

    void PadRectLumaAndChroma(const FrameData &fdata, Ipp32s fourcc, Ipp32s rectx, Ipp32s recty, Ipp32s rectw, Ipp32s recth)
    {
        PadRectLuma(fdata, fourcc, rectx, recty, rectw, recth);
        PadRectChroma(fdata, fourcc, rectx, recty, rectw, recth);
    }

    void Dump(H265VideoParam *par, Frame* frame, FrameList & dpb )
    {
        if (!par->doDumpRecon)
            return;

        mfxU8* fbuf = NULL;
        Ipp32s W = par->Width - par->CropLeft - par->CropRight;
        Ipp32s H = par->Height - par->CropTop - par->CropBottom;
        Ipp32s bd_shift_luma = par->bitDepthLuma > 8;
        Ipp32s bd_shift_chroma = par->bitDepthChroma > 8;
        Ipp32s shift_w = frame->m_chromaFormatIdc != MFX_CHROMAFORMAT_YUV444 ? 1 : 0;
        Ipp32s shift_h = frame->m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420 ? 1 : 0;
        Ipp32s shift = 2 - shift_w - shift_h;
        Ipp32s plane_size = (W*H << bd_shift_luma) + (((W*H/2) << shift) << bd_shift_chroma);

        vm_file *f = vm_file_fopen(par->reconDumpFileName, frame->m_frameOrder ? VM_STRING("ab") : VM_STRING("wb"));
        if (f == NULL)
            return;

        Ipp64s seekPos = (par->picStruct == PROGR) ? (Ipp64s)frame->m_frameOrder * plane_size : (frame->m_frameOrder/2) * (plane_size*2);
        vm_file_fseek(f, seekPos, VM_FILE_SEEK_SET);

        FrameData* recon = frame->m_recon;
        mfxU8 *p = recon->y + ((par->CropLeft + par->CropTop * recon->pitch_luma_pix) << bd_shift_luma);
        for (Ipp32s i = 0; i < H; i++) {
            if (par->picStruct != PROGR && frame->m_bottomFieldFlag)
                vm_file_seek(f, W<<bd_shift_luma, VM_FILE_SEEK_CUR);

            vm_file_fwrite(p, 1<<bd_shift_luma, W, f);
            p += recon->pitch_luma_bytes;

            if (par->picStruct != PROGR && !frame->m_bottomFieldFlag)
                vm_file_seek(f, W<<bd_shift_luma, VM_FILE_SEEK_CUR);
        }

        // writing nv12 to yuv420
        // maxlinesize = 8192
        if (W <= 4096*2) { // else invalid dump
            if (bd_shift_chroma) {
                mfxU16 uvbuf[4096];
                mfxU16 *p;
                for (int part = 0; part <= 1; part++) {
                    p = (Ipp16u*)recon->uv + part + (par->CropLeft >> shift_w << 1) + (par->CropTop>>shift_h) * recon->pitch_chroma_pix;
                    for (Ipp32s i = 0; i < H >> shift_h; i++) {
                        for (int j = 0; j < W>>shift_w; j++)
                            uvbuf[j] = p[2*j];
                        if (par->picStruct != PROGR && frame->m_bottomFieldFlag)
                            vm_file_seek(f, W<<bd_shift_chroma>>shift_w, VM_FILE_SEEK_CUR);
                        vm_file_fwrite(uvbuf, 2, W>>shift_w, f);
                        p += recon->pitch_chroma_pix;
                        if (par->picStruct != PROGR && !frame->m_bottomFieldFlag)
                            vm_file_seek(f, W<<bd_shift_chroma>>shift_w, VM_FILE_SEEK_CUR);
                    }
                }
            } else {
                mfxU8 uvbuf[4096];
                for (int part = 0; part <= 1; part++) {
                    p = recon->uv + part + (par->CropLeft >> shift_w << 1) + (par->CropTop>>shift_h) * recon->pitch_chroma_pix;
                    for (Ipp32s i = 0; i < H >> shift_h; i++) {
                        for (int j = 0; j < W>>shift_w; j++)
                            uvbuf[j] = p[2*j];
                        if (par->picStruct != PROGR && frame->m_bottomFieldFlag)
                            vm_file_seek(f, W<<bd_shift_chroma>>shift_w, VM_FILE_SEEK_CUR);
                        vm_file_fwrite(uvbuf, 1, W>>shift_w, f);
                        p += recon->pitch_chroma_pix;
                        if (par->picStruct != PROGR && !frame->m_bottomFieldFlag)
                            vm_file_seek(f, W<<bd_shift_chroma>>shift_w, VM_FILE_SEEK_CUR);
                    }
                }
            }
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
        m_lowres = NULL;
        cu_data = NULL;
        m_stats[0] = m_stats[1] = NULL;
        m_sceneStats = NULL;

        mem = NULL;
    }

    void Frame::ResetEncInfo()
    {
        m_timeStamp = 0;
        m_picCodeType = 0;
        m_RPSIndex = 0;

        m_wasLookAheadProcessed = 0;
        m_lookaheadRefCounter = 0;
 
        m_picStruct = PROGR;
        m_secondFieldFlag = 0;
        m_bottomFieldFlag = 0;
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
        m_sceneOrder  = 0;
        m_forceTryIntra = 0;

        if (m_stats[0]) {
            m_stats[0]->ResetAvgMetrics();
        }
        if (m_stats[1]) {
            m_stats[1]->ResetAvgMetrics();
        }

        m_futureFrames.resize(0);

        m_feiSyncPoint = NULL;
        Zero(m_feiIntraAngModes);
        Zero(m_feiInterData);
        m_feiCuData = NULL;
        m_feiSaoModes = NULL;
        m_feiOrigin = NULL;
        m_feiRecon = NULL;

        m_userSeiMessages.resize(0);
        m_userSeiMessagesData.resize(0);

        m_ttEncComplete.InitEncComplete(this, 0);
        m_ttInitNewFrame.InitNewFrame(this, (mfxFrameSurface1 *)NULL, 0);
        m_ttPadRecon.InitPadRecon(this, 0);

        m_ttSubmitGpuCopySrc.InitGpuSubmit(this, MFX_FEI_H265_OP_GPU_COPY_SRC, 0);

        m_ttSubmitGpuCopyRec.InitGpuSubmit(this, MFX_FEI_H265_OP_GPU_COPY_REF, 0);
        m_ttWaitGpuCopyRec.InitGpuWait(MFX_FEI_H265_OP_GPU_COPY_REF, 0);

        m_ttSubmitGpuIntra.InitGpuSubmit(this, MFX_FEI_H265_OP_INTRA_MODE, 0);
        m_ttWaitGpuIntra.InitGpuWait(MFX_FEI_H265_OP_INTRA_MODE, 0);

        for (Ipp32s i = 0; i < 4; i++) {
            m_ttSubmitGpuMe[i].InitGpuSubmit(this, MFX_FEI_H265_OP_INTER_ME, 0);
            m_ttWaitGpuMe[i].InitGpuWait(MFX_FEI_H265_OP_INTER_ME, 0);
        }

        m_ttSubmitGpuPostProc.InitGpuSubmit(this, MFX_FEI_H265_OP_POSTPROC, 0);
        m_ttWaitGpuPostProc.InitGpuWait(MFX_FEI_H265_OP_POSTPROC, 0);
    }

    void Frame::ResetCounters()
    {
        m_codedRow = 0;
    }
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
