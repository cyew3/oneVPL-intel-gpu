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
#define H265FEI_AllocateReconSurface(...) (MFX_ERR_NONE)
#define H265FEI_AllocateOutputSurface(...) (MFX_ERR_NONE)
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
    }
    
    void FrameData::Destroy()
    {
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

        size_t len = width * height >> 4;// because RsCs configured for 4x4 blk only
        m_rs.resize(len, 0);
        m_cs.resize(len, 0);
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
        m_rs.resize(len);
        m_cs.resize(len);
        rscs_ctb.resize(numBlk);
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

    void FeiRecData::Create(const FeiRecData::AllocInfo &allocInfo)
    {
        if (H265FEI_AllocateReconSurface(allocInfo.feiHdl, &m_handle) != MFX_ERR_NONE)
            Throw(std::runtime_error("H265FEI_AllocateReconSurface failed"));

        m_fei = allocInfo.feiHdl;
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
        m_ttWaitGpuCopyRec.InitGpuWait(MFX_FEI_H265_OP_GPU_COPY_REF, 0);
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
