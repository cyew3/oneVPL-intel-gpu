//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_frame.h"
#include "mfx_h265_enc.h"
#include "vm_file.h"
#include "ippi.h"
#include "ippvc.h"
#include <memory>

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


void H265Frame::Create(H265VideoParam *par)
{
    Ipp32s numCtbs = par->PicWidthInCtbs * par->PicHeightInCtbs;
    Ipp32s numCtbs_parts = numCtbs << par->Log2NumPartInCU;
    //Ipp32s numCtbs_padd = (par->PicWidthInCtbs + 2) * (par->PicHeightInCtbs + 2);

    padding = par->MaxCUSize + 16;
    //padding = par->MaxCUSize + 64;

    width = par->Width;
    height = par->Height;
    m_bitDepthLuma =  par->bitDepthLuma;
    m_bitDepthChroma = par->bitDepthChroma;
    m_bdLumaFlag = par->bitDepthLuma > 8;
    m_bdChromaFlag = par->bitDepthChroma > 8;
    m_chromaFormatIdc = par->chromaFormatIdc;

    pitch_luma_bytes = pitch_luma_pix = width + padding * 2;

    Ipp32s plane_size_luma = (width + padding * 2) * (height + padding * 2);
    Ipp32s plane_size_chroma;
    
    switch (m_chromaFormatIdc) {
    case MFX_CHROMAFORMAT_YUV422:
       plane_size_chroma = plane_size_luma;
       pitch_chroma_pix = pitch_luma_pix;
       break;
    case MFX_CHROMAFORMAT_YUV444:
       plane_size_chroma = plane_size_luma << 1;
       pitch_chroma_pix = pitch_luma_pix << 1;
       break;
    default:
    case MFX_CHROMAFORMAT_YUV420:
       plane_size_chroma = plane_size_luma >> 1;
       pitch_chroma_pix = pitch_luma_pix;
       break;
    }

    pitch_chroma_bytes = pitch_chroma_pix;

    if (m_bdLumaFlag) {
        plane_size_luma <<= 1;
        pitch_luma_bytes <<= 1;
    }
    if (m_bdChromaFlag) {
        plane_size_chroma <<= 1;
        pitch_chroma_bytes <<= 1;
    }

    Ipp32s len = numCtbs_parts * sizeof(H265CUData) + ALIGN_VALUE;
    len += (plane_size_luma + plane_size_chroma) + ALIGN_VALUE*3;

    mem = H265_Malloc(len);
    if (!mem)
        throw std::exception();

    cu_data = align_pointer<H265CUData *> (mem, ALIGN_VALUE);
    y = align_pointer<Ipp8u *> (cu_data + numCtbs_parts, ALIGN_VALUE);
    uv = align_pointer<Ipp8u *> (y + plane_size_luma, ALIGN_VALUE);;

    y += ((pitch_luma_pix + 1) * padding) << m_bdLumaFlag;
    uv += ((pitch_chroma_pix * padding >> par->chromaShiftH) + (padding << par->chromaShiftWInv)) << m_bdChromaFlag;

    //brc lookahead
    {
        const Ipp32s sizeBlk = 8;
        const Ipp32s picWidthInBlks  = (width  + sizeBlk - 1) / sizeBlk;
        const Ipp32s picHeightInBlks = (height + sizeBlk - 1) / sizeBlk;

        m_intraSatd.resize(picWidthInBlks*picHeightInBlks);
    }

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

void H265Frame::CopyFrame(const mfxFrameSurface1 *surface)
{
    IppiSize roi = { width, height };
    mfxU16 InputBitDepthLuma = 8;
    mfxU16 InputBitDepthChroma = 8;

    if (surface->Info.FourCC == MFX_FOURCC_P010 || surface->Info.FourCC == MFX_FOURCC_P210) {
        InputBitDepthLuma = 10;
        InputBitDepthChroma = 10;
    }

    switch ((m_bdLumaFlag << 1) | (InputBitDepthLuma > 8)) {
    case 0:
        ippiCopy_8u_C1R(surface->Data.Y, surface->Data.Pitch, y, pitch_luma_bytes, roi);
        break;
    case 1:
        ippiConvert_16u8u_C1R((Ipp16u*)surface->Data.Y, surface->Data.Pitch, y, pitch_luma_bytes, roi);
        break;
    case 2:
        ippiConvert_8u16u_C1R(surface->Data.Y, surface->Data.Pitch, (Ipp16u*)y, pitch_luma_bytes, roi);
        ippiLShiftC_16u_C1IR(m_bitDepthLuma - 8, (Ipp16u*)y, pitch_luma_bytes, roi);
        break;
    case 3:
        ippiCopy_16u_C1R((Ipp16u*)surface->Data.Y, surface->Data.Pitch, (Ipp16u*)y, pitch_luma_bytes, roi);
        break;
    default:
        VM_ASSERT(0);
    }

    roi.width <<= (m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV444);
    roi.height >>= (m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420);

    switch ((m_bdChromaFlag << 1) | (InputBitDepthChroma > 8)) {
    case 0:
        ippiCopy_8u_C1R(surface->Data.UV, surface->Data.Pitch, uv, pitch_chroma_bytes, roi);
        break;
    case 1:
        ippiConvert_16u8u_C1R((Ipp16u*)surface->Data.UV, surface->Data.Pitch, uv, pitch_chroma_bytes, roi);
        break;
    case 2:
        ippiConvert_8u16u_C1R(surface->Data.UV, surface->Data.Pitch, (Ipp16u*)uv, pitch_chroma_bytes, roi);
        ippiLShiftC_16u_C1IR(m_bitDepthChroma - 8, (Ipp16u*)uv, pitch_chroma_bytes, roi);
        break;
    case 3:
        ippiCopy_16u_C1R((Ipp16u*)surface->Data.UV, surface->Data.Pitch, (Ipp16u*)uv, pitch_chroma_bytes, roi);
        break;
    default:
        VM_ASSERT(0);
    }

    // so far needed for gradient analysis only
    (m_bdLumaFlag == 0)
        ? PadOnePix((Ipp8u *)y, pitch_luma_pix, width, height)
        : PadOnePix((Ipp16u *)y, pitch_luma_pix, width, height);
    (m_bdChromaFlag == 0)
        ? PadOnePix((Ipp16u *)uv, pitch_chroma_pix / 2, width / 2, height / 2)
        : PadOnePix((Ipp32u *)uv, pitch_chroma_pix / 2, width / 2, height / 2);

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


template <class PixType> void PadOneLine_Bottom_v2(PixType *startLine, Ipp32s pitch, Ipp32s width, Ipp32s padding_w, Ipp32s padding_h)
{
    PixType* ptr = startLine - padding_w;

    for (Ipp32s i = 1; i <= padding_h; i++) {
        ippsCopy(ptr, ptr + i*pitch, width + 2*padding_w);
    }
}

void PadOneReconRow(H265Frame* frame, Ipp32u ctb_row, Ipp32u maxCuSize, Ipp32u PicHeightInCtbs)
{
    H265Frame *reconstructFrame = frame;

    Ipp8u bitDepth8 = (reconstructFrame->m_bitDepthLuma == 8) ? 1 : 0;
    Ipp32s shift_w = reconstructFrame->m_chromaFormatIdc != MFX_CHROMAFORMAT_YUV444 ? 1 : 0;
    Ipp32s shift_h = reconstructFrame->m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420 ? 1 : 0;

    // L/R Padding
    {
        //luma
        Ipp8u *startLine = reconstructFrame->y + (ctb_row)*reconstructFrame->pitch_luma_bytes*maxCuSize;
        (bitDepth8)
            ? PadOneLine_v2((Ipp8u*)startLine, reconstructFrame->pitch_luma_pix, reconstructFrame->width, maxCuSize, reconstructFrame->padding)
            : PadOneLine_v2((Ipp16u*)startLine, reconstructFrame->pitch_luma_pix, reconstructFrame->width, maxCuSize, reconstructFrame->padding);

        //Chroma
        //startLine = reconstructFrame->uv + (ctb_row)*reconstructFrame->pitch_chroma_bytes* m_videoParam.MaxCUSize/2;
        startLine = reconstructFrame->uv + (ctb_row)*reconstructFrame->pitch_chroma_bytes* (maxCuSize >> shift_h);
        (bitDepth8) 
            ? PadOneLine_v2((Ipp16u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, maxCuSize >> shift_h, reconstructFrame->padding >> shift_w)
            : PadOneLine_v2((Ipp32u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, maxCuSize >> shift_h, reconstructFrame->padding >> shift_w);
    }

    //first line: top padding
    if (ctb_row == 0) {
        Ipp8u *startLine = reconstructFrame->y;
        (bitDepth8)
            ? PadOneLine_Top_v2((Ipp8u*)startLine, reconstructFrame->pitch_luma_pix, reconstructFrame->width, reconstructFrame->padding, reconstructFrame->padding)
            : PadOneLine_Top_v2((Ipp16u*)startLine, reconstructFrame->pitch_luma_pix, reconstructFrame->width, reconstructFrame->padding, reconstructFrame->padding);

        startLine = reconstructFrame->uv;
        (bitDepth8)
            ? PadOneLine_Top_v2((Ipp16u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, reconstructFrame->padding >> shift_w, reconstructFrame->padding >> shift_h)
            : PadOneLine_Top_v2((Ipp32u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, reconstructFrame->padding >> shift_w, reconstructFrame->padding >> shift_h);
    }

    // last line: bottom padding
    if (ctb_row == PicHeightInCtbs - 1) {
        Ipp8u *startLine = reconstructFrame->y + (reconstructFrame->height - 1)*reconstructFrame->pitch_luma_bytes;
        (bitDepth8)
            ? PadOneLine_Bottom_v2((Ipp8u*)startLine, reconstructFrame->pitch_luma_pix, reconstructFrame->width, reconstructFrame->padding, reconstructFrame->padding)
            : PadOneLine_Bottom_v2((Ipp16u*)startLine, reconstructFrame->pitch_luma_pix, reconstructFrame->width, reconstructFrame->padding, reconstructFrame->padding);

        startLine = reconstructFrame->uv + ((reconstructFrame->height >> shift_h) - 1)*reconstructFrame->pitch_chroma_bytes;
        (bitDepth8)
            ? PadOneLine_Bottom_v2((Ipp16u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, reconstructFrame->padding >> shift_w, reconstructFrame->padding >> shift_h)
            : PadOneLine_Bottom_v2((Ipp32u *)startLine, reconstructFrame->pitch_chroma_pix/2, reconstructFrame->width >> shift_w, reconstructFrame->padding >> shift_w, reconstructFrame->padding >> shift_h);
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

    void H265Frame::doPadding()
    {
        if (!m_bdLumaFlag) {
            ippiExpandPlane_H264_8u_C1R(y, width, height, pitch_luma_bytes, padding, IPPVC_FRAME);
        } else {
            ExpandPlane(1, y, pitch_luma_bytes, width, height, padding, padding);
        }

        Ipp32s shift_w = m_chromaFormatIdc != MFX_CHROMAFORMAT_YUV444 ? 1 : 0;
        Ipp32s shift_h = m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420 ? 1 : 0;
        
        if (!m_bdChromaFlag) {
            ExpandPlane(1, uv, pitch_chroma_bytes, width >> shift_w, height >> shift_h, padding >> shift_w, padding >> shift_h);
        } else {
            ExpandPlane(2, uv, pitch_chroma_bytes, width >> shift_w, height >> shift_h, padding >> shift_w, padding >> shift_h);
    }
}

    void Dump(const vm_char* fname, H265VideoParam *par, H265Frame* frame, TaskList & dpb )
    {
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

    if (!fname || !fname[0])
        return;

        Ipp32s numlater = 0; // number of dumped frames with later POC
        if (frame->m_picCodeType == MFX_FRAMETYPE_B) {
            for (TaskIter it = dpb.begin(); it != dpb.end(); it++ ) {
                H265Frame *pFrm = (*it)->m_frameRecon;
                if ( NULL == (*it)->m_frameOrigin && frame->m_poc < pFrm->m_poc)
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

    int i;
    mfxU8 *p = frame->y + ((par->CropLeft + par->CropTop * frame->pitch_luma_pix) << bd_shift_luma);
    for (i = 0; i < H; i++) {
        vm_file_fwrite(p, 1<<bd_shift_luma, W, f);
        p += frame->pitch_luma_bytes;
    }
    // writing nv12 to yuv420
    // maxlinesize = 4096
    if (W <= 2048*2) { // else invalid dump
        if (bd_shift_chroma) {
            mfxU16 uvbuf[2048];
            mfxU16 *p;
            for (int part = 0; part <= 1; part++) {
                p = (Ipp16u*)frame->uv + part + (par->CropLeft >> shift_w << 1) + (par->CropTop>>shift_h) * frame->pitch_chroma_pix;
                for (i = 0; i < H >> shift_h; i++) {
                    for (int j = 0; j < W>>shift_w; j++)
                        uvbuf[j] = p[2*j];
                    vm_file_fwrite(uvbuf, 2, W>>shift_w, f);
                    p += frame->pitch_chroma_pix;
                }
            }
        } else {
            mfxU8 uvbuf[2048];
            for (int part = 0; part <= 1; part++) {
                p = frame->uv + part + (par->CropLeft >> shift_w << 1) + (par->CropTop>>shift_h) * frame->pitch_chroma_pix;
                for (i = 0; i < H >> shift_h; i++) {
                    for (int j = 0; j < W>>shift_w; j++)
                        uvbuf[j] = p[2*j];
                    vm_file_fwrite(uvbuf, 1, W>>shift_w, f);
                    p += frame->pitch_chroma_pix;
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


void H265Frame::Destroy()
{
    if (mem)
        H265_Free(mem);
    mem = NULL;
}

void H265Frame::ResetMemInfo()
{
    mem = NULL;
    cu_data = NULL;
    y = NULL;
    uv = NULL;

    width = 0;
    height = 0;
    padding = 0;
    pitch_luma_pix = 0;
    pitch_luma_bytes = 0;
    pitch_chroma_pix = 0;
    pitch_chroma_bytes = 0;
    m_bitDepthLuma = 0;
    m_bdLumaFlag = 0;
    m_bitDepthChroma = 0;
    m_bdChromaFlag = 0;
    m_chromaFormatIdc = 0;
}

void H265Frame::ResetEncInfo()
{
    m_timeStamp = 0;
    m_picCodeType = 0;
    m_RPSIndex = 0;
    m_wasLookAheadProcessed = 0;
    m_pyramidLayer = 0;
    m_miniGopCount = 0;
    m_biFramesInMiniGop = 0;
    m_frameOrder = 0;
    m_frameOrderOfLastIdr = 0;
    m_frameOrderOfLastIntra = 0;
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
}

void H265Frame::ResetCounters()
{
    m_codedRow = 0;
    m_refCounter = 0;
}

    FramePtrIter FindFreeFrame(FramePtrList & queue)
    {
        FramePtrIter end = queue.end();
        for (FramePtrIter i = queue.begin(); i != end; ++i)
            if (vm_interlocked_cas32(&(*i)->m_refCounter, 1, 0) == 0)
                return i;
        return end;
    }

    FramePtrIter GetFreeFrame(FramePtrList & queue, H265VideoParam *par)
    {
        FramePtrIter i = FindFreeFrame(queue);
        if (i != queue.end())
            return i;

        std::auto_ptr<H265Frame> newFrame(new H265Frame());
        newFrame->Create(par);
        newFrame->AddRef();
        queue.push_back(newFrame.release());
        return --queue.end();
    }

    void H265Frame::CopyEncInfo(const H265Frame *src)
    {
        m_timeStamp = src->m_timeStamp;
        m_picCodeType = src->m_picCodeType;
        m_RPSIndex = src->m_RPSIndex;
        m_wasLookAheadProcessed = src->m_wasLookAheadProcessed;
        m_pyramidLayer = src->m_pyramidLayer;
        m_miniGopCount = src->m_miniGopCount;
        m_biFramesInMiniGop = src->m_biFramesInMiniGop;
        m_frameOrder = src->m_frameOrder;
        m_frameOrderOfLastIdr = src->m_frameOrderOfLastIdr;
        m_frameOrderOfLastIntra = src->m_frameOrderOfLastIntra;
        m_frameOrderOfLastAnchor = src->m_frameOrderOfLastAnchor;
        m_poc = src->m_poc;
        m_encOrder = src->m_encOrder;
        m_isShortTermRef = src->m_isShortTermRef;
        m_isLongTermRef = src->m_isLongTermRef;
        m_isIdrPic = src->m_isIdrPic;
        m_isRef = src->m_isRef;
        small_memcpy(m_refPicList, src->m_refPicList, sizeof(m_refPicList));
        small_memcpy(m_mapRefIdxL1ToL0, src->m_mapRefIdxL1ToL0, sizeof(m_mapRefIdxL1ToL0));
        m_allRefFramesAreFromThePast = src->m_allRefFramesAreFromThePast;
    }
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
