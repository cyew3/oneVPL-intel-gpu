// Copyright (c) 2014-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include <fstream>

#include "mfx_av1_ctb.h"
#include "mfx_av1_frame.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_lookahead.h"
#include "mfx_av1_deblocking.h"
#include "mfx_av1_superres.h"

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

namespace AV1Enc {

    using namespace MfxEnumShortAliases;

    void FrameData::Create(const FrameData::AllocInfo &allocInfo)
    {
        int32_t bdShiftLu = allocInfo.bitDepthLu > 8;
        int32_t bdShiftCh = allocInfo.bitDepthCh > 8;

        width = allocInfo.width;
        height = allocInfo.height;
        padding = allocInfo.paddingLu;
        pitch_luma_bytes = allocInfo.pitchInBytesLu;
        pitch_chroma_bytes = allocInfo.pitchInBytesCh;
        pitch_luma_pix = allocInfo.pitchInBytesLu >> bdShiftLu;
        pitch_chroma_pix = allocInfo.pitchInBytesCh >> bdShiftCh;

        int32_t size = allocInfo.sizeInBytesLu + allocInfo.sizeInBytesCh + allocInfo.alignment * 2;

        int32_t vertPaddingSizeInBytesLu = allocInfo.paddingLu * allocInfo.pitchInBytesLu;
        int32_t vertPaddingSizeInBytesCh = allocInfo.paddingChH * allocInfo.pitchInBytesCh;

        mem = new uint8_t[size];

        y = UMC::align_pointer<uint8_t *>(mem + vertPaddingSizeInBytesLu, allocInfo.alignment);
        uv = y + allocInfo.sizeInBytesLu - vertPaddingSizeInBytesLu + vertPaddingSizeInBytesCh;
        uv = UMC::align_pointer<uint8_t *>(uv, allocInfo.alignment);
        y = UMC::align_pointer<uint8_t *>(y + (allocInfo.paddingLu << bdShiftLu), 64);
        uv = UMC::align_pointer<uint8_t *>(uv + (allocInfo.paddingChW << bdShiftCh), 64);

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

        delete[] mem;
        mem = NULL;
    }

    void Statistics::Create(const Statistics::AllocInfo &allocInfo)
    {
        // all algorithms designed for 8x8 (CsRs - 4x4)
        int32_t width = allocInfo.width;
        int32_t height = allocInfo.height;
        int32_t blkSize = SIZE_BLK_LA;
        int32_t PicWidthInBlk = (width + blkSize - 1) / blkSize;
        int32_t PicHeightInBlk = (height + blkSize - 1) / blkSize;
        int32_t numBlk = PicWidthInBlk * PicHeightInBlk;

        // for BRC
        if (allocInfo.analyzeCmplx) {
            m_intraSatd.resize(numBlk);
            m_interSatd.resize(numBlk);
            m_intraSatdHist.resize(33, -1);
            m_bestSatdHist.resize(33, -1);
        }

        // for content analysis (paq/calq)
        if (allocInfo.analyzeCmplx || (allocInfo.deltaQpMode & (AMT_DQP_PAQ | AMT_DQP_CAL))) {
            m_interSad.resize(numBlk);
            m_interSad_pdist_past.resize(numBlk);
            m_interSad_pdist_future.resize(numBlk);

            AV1MV initMv = { 0,0 };
            m_mv.resize(numBlk, initMv);
            m_mv_pdist_past.resize(numBlk, initMv);
            m_mv_pdist_future.resize(numBlk, initMv);
        }

        const int32_t alignedWidth = ((width + 63) & ~63);
        const int32_t alignedHeight = ((height + 63) & ~63);

        for (int32_t log2BlkSize = 2; log2BlkSize <= 6; log2BlkSize++) {
            const int32_t widthInBlk = alignedWidth >> log2BlkSize;
            const int32_t heightInBlk = alignedHeight >> log2BlkSize;
            m_pitchRsCs[log2BlkSize - 2] = (widthInBlk + 7) & ~7; // 32byte aligned
            m_rcscSize[log2BlkSize - 2] = m_pitchRsCs[log2BlkSize - 2] * heightInBlk;
            m_rs[log2BlkSize - 2] = (int32_t *)AV1_Malloc(sizeof(int32_t) * m_rcscSize[log2BlkSize - 2]); // ippMalloc is 64-bytes aligned
            m_cs[log2BlkSize - 2] = (int32_t *)AV1_Malloc(sizeof(int32_t) * m_rcscSize[log2BlkSize - 2]);
            memset(m_rs[log2BlkSize - 2], 0, sizeof(int32_t) * m_rcscSize[log2BlkSize - 2]);
            memset(m_cs[log2BlkSize - 2], 0, sizeof(int32_t) * m_rcscSize[log2BlkSize - 2]);
        }

        m_pitchColorCount16x16 = (width >> 4);
        m_colorCount16x16 = (uint8_t *)AV1_Malloc(sizeof(uint8_t) * m_pitchColorCount16x16 * (height >> 4));
        memset(m_colorCount16x16, 32, sizeof(uint8_t)* m_pitchColorCount16x16 * (height >> 4));
        //rscs_ctb.resize(numBlk, 0);
        //sc_mask.resize(numBlk, 0);
        int32_t numCtbs = ((width + 63) >> 6) * ((height + 63) >> 6);
        qp_mask.resize(numCtbs, 0);
        coloc_futr.resize(numCtbs, 0);
        coloc_past.resize(numCtbs, 0);
#ifdef AMT_HROI_PSY_AQ
        if (allocInfo.deltaQpMode & AMT_DQP_PSY_HROI) {
            // Inside HROI aligned by 16
            int32_t roiPitch = (width + 15)&~15;
            int32_t roiHeight = (height + 15)&~15;
            m_RoiPitch = roiPitch >> 3;
            int32_t numBlkRoi = m_RoiPitch * (roiHeight >> 3);
            roi_map_8x8.resize(3 * numBlkRoi, 0);
            lum_avg_8x8.resize(numBlkRoi, 0);
            ctbStats = (CtbRoiStats *)AV1_Malloc(sizeof(CtbRoiStats) * numBlkRoi);
            if (ctbStats == NULL) throw std::exception();
            memset(ctbStats, 0, sizeof(CtbRoiStats) * numBlkRoi);
        }
#endif
        ResetAvgMetrics();
    }

    void Statistics::Destroy()
    {
        int32_t numBlk = 0;
        m_intraSatd.resize(numBlk);
        m_interSatd.resize(numBlk);
        m_interSad.resize(numBlk);
        m_intraSatdHist.resize(0);
        m_bestSatdHist.resize(0);
        m_interSad_pdist_past.resize(numBlk);
        m_interSad_pdist_future.resize(numBlk);
        m_mv.resize(numBlk);
        m_mv_pdist_past.resize(numBlk);
        m_mv_pdist_future.resize(numBlk);
        for (int32_t log2BlkSize = 2; log2BlkSize <= 6; log2BlkSize++) {
            if (m_rs[log2BlkSize - 2]) { AV1_Free(m_rs[log2BlkSize - 2]); m_rs[log2BlkSize - 2] = NULL; }
            if (m_cs[log2BlkSize - 2]) { AV1_Free(m_cs[log2BlkSize - 2]); m_cs[log2BlkSize - 2] = NULL; }
        }
        if (m_colorCount16x16) AV1_Free(m_colorCount16x16);
        //rscs_ctb.resize(numBlk);
        //sc_mask.resize(numBlk);
        qp_mask.resize(numBlk);
        coloc_futr.resize(numBlk);
        coloc_past.resize(numBlk);
#ifdef AMT_HROI_PSY_AQ
        roi_map_8x8.resize(0);
        lum_avg_8x8.resize(0);
        if (ctbStats) AV1_Free(ctbStats);   ctbStats = NULL;
#endif
    }


    void FeiInputData::Create(const FeiInputData::AllocInfo &allocInfo)
    {
        if (allocInfo.feiHdl && H265FEI_AllocateInputSurface(allocInfo.feiHdl, &m_handle) != MFX_ERR_NONE)
            Throw(std::runtime_error("H265FEI_AllocateInputSurface failed"));
        m_fei = allocInfo.feiHdl;
    }


    void FeiInputData::Destroy()
    {
        if (m_fei && m_handle) {
            H265FEI_FreeSurface(m_fei, m_handle);
            m_fei = NULL;
            m_handle = NULL;
        }
    }


    void FeiReconData::Create(const FeiReconData::AllocInfo &allocInfo)
    {
        if (allocInfo.feiHdl && H265FEI_AllocateReconSurface(allocInfo.feiHdl, &m_handle) != MFX_ERR_NONE)
            Throw(std::runtime_error("H265FEI_AllocateReconSurface failed"));
        m_fei = allocInfo.feiHdl;
    }


    void FeiReconData::Destroy()
    {
        if (m_fei && m_handle) {
            H265FEI_FreeSurface(m_fei, m_handle);
            m_fei = NULL;
            m_handle = NULL;
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

        m_pitch = (int32_t)info.pitch;
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

    void FeiSurfaceUp::Create(const FeiSurfaceUp::AllocInfo &allocInfo)
    {
        const mfxSurfInfoENC &info = allocInfo.allocInfo;

        m_allocated.reset(new mfxU8[info.size + info.align]);
        m_sysmem = UMC::align_pointer<uint8_t*>(m_allocated.get(), info.align);

        if (allocInfo.feiHdl) {
            if (H265FEI_AllocateOutputSurface(allocInfo.feiHdl, m_sysmem, const_cast<mfxSurfInfoENC *>(&info), &m_handle) != MFX_ERR_NONE) {
                m_sysmem = nullptr;
                m_allocated.reset(nullptr);
                Throw(std::runtime_error("H265FEI_AllocateOutputSurface failed"));
            }
        }

        m_pitch = (int32_t)info.pitch;
        m_fei = allocInfo.feiHdl;
    }

    void FeiSurfaceUp::Destroy()
    {
        if (m_fei && m_handle && m_sysmem) {
            H265FEI_FreeSurface(m_fei, m_handle);
            m_handle = NULL;
            m_fei = NULL;
        }
        m_sysmem = nullptr;
        m_allocated.reset(nullptr);
    }

    void FeiBufferUp::Create(const AllocInfo &allocInfo)
    {
        m_allocated = new mfxU8[allocInfo.size + allocInfo.alignment];
        m_sysmem = UMC::align_pointer<uint8_t*>(m_allocated, allocInfo.alignment);
        if (allocInfo.feiHdl) {
            if (H265FEI_AllocateOutputBuffer(allocInfo.feiHdl, m_sysmem, allocInfo.size, &m_handle) != MFX_ERR_NONE) {
                delete[] m_allocated;
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
        delete[] m_allocated;
        m_allocated = NULL;
        m_sysmem = NULL;
    }

    void Frame::Create(const AV1VideoParam *par)
    {
        m_par = par;

        int32_t numCtbs = par->PicWidthInCtbs * par->PicHeightInCtbs;
        m_lcuQps.resize(numCtbs);
        m_lcuRound.resize(numCtbs);
        m_bitDepthLuma = par->bitDepthLuma;
        m_bitDepthChroma = par->bitDepthChroma;
        m_bdLumaFlag = par->bitDepthLuma > 8;
        m_bdChromaFlag = par->bitDepthChroma > 8;
        m_chromaFormatIdc = par->chromaFormatIdc;


        m_ttEncode = (ThreadingTask *)AV1_Malloc(numCtbs * sizeof(ThreadingTask));
        if (!m_ttEncode)
            throw std::exception();

        m_ttDeblockAndCdef = (ThreadingTask *)AV1_Malloc(numCtbs * sizeof(ThreadingTask));
        if (!m_ttDeblockAndCdef)
            throw std::exception();

        const int32_t numTiles = std::max(par->tileParamKeyFrame.numTiles, par->tileParamInterFrame.numTiles);

        if (PACK_BY_TILES) {
            m_ttPackTile = (ThreadingTask *)AV1_Malloc(numTiles * sizeof(ThreadingTask));
            if (!m_ttPackTile)
                throw std::exception();
        } else {
            const int32_t numTileCols = std::max(par->tileParamKeyFrame.cols, par->tileParamInterFrame.cols);
            m_ttPackRow = (ThreadingTask *)AV1_Malloc(numTileCols * par->sb64Rows * sizeof(ThreadingTask));
            if (!m_ttPackRow)
                throw std::exception();
        }

        m_widthLowRes4x = (par->Width >> 2) & ~0x1f; // 32 bytes aligned
        m_heightLowRes4x = (par->Height >> 2) & ~0xf; // 16 bytes aligned
        m_lowres4x = (uint8_t *)AV1_Malloc(m_widthLowRes4x * m_heightLowRes4x);
        if (!m_lowres4x)
            throw std::exception();
        m_vertMvHist.resize(2 * m_heightLowRes4x);

        m_numDetectScrollRegions = std::min(MAX_NUM_DEPENDENCIES, (int32_t)m_par->num_threads);
        m_detectScrollRegionWidth = DivUp(m_widthLowRes4x >> 5, m_numDetectScrollRegions) << 5;
        m_numDetectScrollRegions = DivUp(m_widthLowRes4x, m_detectScrollRegionWidth);

        m_ttDetectScrollStart.frame = this;
        m_ttDetectScrollEnd.frame = this;
        for (int32_t i = 0; i < m_numDetectScrollRegions; i++) {
            m_ttDetectScrollRoutine[i].frame = this;
            m_ttDetectScrollRoutine[i].startX = std::min(m_widthLowRes4x, i * m_detectScrollRegionWidth);
            m_ttDetectScrollRoutine[i].endX = std::min(m_widthLowRes4x, (i + 1) * m_detectScrollRegionWidth);
        }

        // if lookahead
        int32_t numTasks, regionCount, lowresRowsInRegion, originRowsInRegion;
        GetLookaheadGranularity(*par, regionCount, lowresRowsInRegion, originRowsInRegion, numTasks);

        m_numLaTasks = numTasks;
        m_ttLookahead = (ThreadingTask *)AV1_Malloc(numTasks * sizeof(ThreadingTask));
        if (!m_ttLookahead)
            throw std::exception();

        for (int32_t i = LAST_FRAME; i <= ALTREF_FRAME; i++) {
            m_ttSubmitGpuMe[i] = (ThreadingTask *)AV1_Malloc(par->numGpuSlices * sizeof(ThreadingTask));
            if (!m_ttSubmitGpuMe[i])
                throw std::exception();
        }

        m_ttSubmitGpuMd = (ThreadingTask *)AV1_Malloc(par->numGpuSlices * sizeof(ThreadingTask));
        if (!m_ttSubmitGpuMd)
            throw std::exception();

        m_ttWaitGpuMd = (ThreadingTask *)AV1_Malloc(par->numGpuSlices * sizeof(ThreadingTask));
        if (!m_ttWaitGpuMd)
            throw std::exception();

        // VP9 specific
        m_tileContexts.resize(numTiles);
        for (int32_t i = 0; i < numTiles; i++)
            m_tileContexts[i].Alloc(par->sb64Cols, par->sb64Rows); // allocate for entire frame to be ready to switch to single tile encoding

#if USE_CMODEL_PAK
        av1frameOrigin.InitAlloc(par->Width, par->Height, CmodelAv1::YUV420, 8);
        av1frameRecon.InitAlloc(par->Width, par->Height, CmodelAv1::YUV420, 8);
#endif
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

        int32_t inPitch = in->Data.Pitch;
        uint8_t *inDataY = in->Data.Y;
        uint8_t *inDataUV = in->Data.UV;
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
#if ENABLE_10BIT
        case 1:
            ippiConvert_16u8u_C1R((uint16_t*)inDataY, inPitch, out->y, out->pitch_luma_bytes, roi);
            break;
        case 2:
            ippiConvert_8u16u_C1R(inDataY, inPitch, (uint16_t*)out->y, out->pitch_luma_bytes, roi);
            ippiLShiftC_16u_C1IR(m_bitDepthLuma - 8, (uint16_t*)out->y, out->pitch_luma_bytes, roi);
            break;
        case 3:
            ippiCopy_16u_C1R((uint16_t*)inDataY, inPitch, (uint16_t*)out->y, out->pitch_luma_bytes, roi);
            break;
#endif
        default:
            assert(0);
        }

        roi.width <<= int(m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV444);
        roi.height >>= int(m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420);

        switch ((m_bdChromaFlag << 1) | (InputBitDepthChroma > 8)) {
        case 0:
            ippiCopy_8u_C1R(inDataUV, inPitch, out->uv, out->pitch_chroma_bytes, roi);
            break;
#if ENABLE_10BIT
        case 1:
            ippiConvert_16u8u_C1R((uint16_t*)inDataUV, inPitch, out->uv, out->pitch_chroma_bytes, roi);
            break;
        case 2:
            ippiConvert_8u16u_C1R(inDataUV, inPitch, (uint16_t*)out->uv, out->pitch_chroma_bytes, roi);
            ippiLShiftC_16u_C1IR(m_bitDepthChroma - 8, (uint16_t*)out->uv, out->pitch_chroma_bytes, roi);
            break;
        case 3:
            ippiCopy_16u_C1R((uint16_t*)inDataUV, inPitch, (uint16_t*)out->uv, out->pitch_chroma_bytes, roi);
            break;
#endif
        default:
            assert(0);
        }
    }

    template <class T> void PadRect(T *plane, int32_t pitch, int32_t width, int32_t height, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth, int32_t padL, int32_t padR, int32_t padh)
    {
        rectx = Saturate(0, width - 1, rectx);
        recty = Saturate(0, height - 1, recty);
        rectw = Saturate(1, width - rectx, rectw);
        recth = Saturate(1, height - recty, recth);
        height = recth;
        width = rectw;
        if (rectx == 0) {
            rectx -= padL;
            rectw += padL;
            for (int32_t y = recty; y < recty + recth; y++)
                _ippsSet(plane[y * pitch], plane + y * pitch - padL, padL);
        }
        if (rectx + rectw == width) {
            rectw += padR;
            for (int32_t y = recty; y < recty + recth; y++)
                _ippsSet(plane[y * pitch + width - 1], plane + y * pitch + width, padR);
        }
        if (recty == 0)
            for (int32_t j = 1; j <= padh; j++)
                _ippsCopy(plane + rectx, plane + rectx - j * pitch, rectw);
        if (recty + recth == height)
            for (int32_t j = 1; j <= padh; j++)
                _ippsCopy(plane + (height - 1) * pitch + rectx, plane + (height - 1 + j) * pitch + rectx, rectw);
    }
    template void PadRect<uint8_t>(uint8_t *plane, int32_t pitch, int32_t width, int32_t height, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth, int32_t padL, int32_t padR, int32_t padh);
    template void PadRect<uint16_t>(uint16_t *plane, int32_t pitch, int32_t width, int32_t height, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth, int32_t padL, int32_t padR, int32_t padh);
    template void PadRect<uint32_t>(uint32_t *plane, int32_t pitch, int32_t width, int32_t height, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth, int32_t padL, int32_t padR, int32_t padh);

    void PadRectLuma(const FrameData &fdata, int32_t fourcc, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth)
    {
        // work-around for 8x and 16x downsampling on gpu
        // currently DS kernel expect right border is padded up to pitch
        int32_t paddingL = fdata.padding;
        int32_t paddingR = fdata.padding;
        int32_t bppShift = ((fourcc == P010) || (fourcc == P210)) ? 1 : 0;
        if (fdata.m_handle) {
            paddingR = fdata.pitch_luma_pix - fdata.width - (mfx::align2_value<int32_t>(fdata.padding << bppShift, 64) >> bppShift);
            paddingL = mfx::align2_value<int32_t>(fdata.padding << bppShift, 64) >> bppShift;
        }

        (fourcc == NV12 || fourcc == NV16)
            ? PadRect((uint8_t *)fdata.y, fdata.pitch_luma_pix, fdata.width, fdata.height, rectx, recty, rectw, recth, paddingL, paddingR, fdata.padding)
            : PadRect((uint16_t*)fdata.y, fdata.pitch_luma_pix, fdata.width, fdata.height, rectx, recty, rectw, recth, paddingL, paddingR, fdata.padding);
    }

    void PadRectChroma(const FrameData &fdata, int32_t fourcc, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth)
    {
        int32_t shiftH = (fourcc == NV12 || fourcc == P010) ? 1 : 0;
        (fourcc == NV12 || fourcc == NV16)
            ? PadRect((uint16_t *)fdata.uv, fdata.pitch_chroma_pix / 2, fdata.width / 2, fdata.height >> shiftH, rectx / 2, recty >> shiftH, rectw / 2, recth >> shiftH, fdata.padding / 2, fdata.padding / 2, fdata.padding >> shiftH)
            : PadRect((uint32_t *)fdata.uv, fdata.pitch_chroma_pix / 2, fdata.width / 2, fdata.height >> shiftH, rectx / 2, recty >> shiftH, rectw / 2, recth >> shiftH, fdata.padding / 2, fdata.padding / 2, fdata.padding >> shiftH);
    }

    void PadRectLumaAndChroma(const FrameData &fdata, int32_t fourcc, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth)
    {
        PadRectLuma(fdata, fourcc, rectx, recty, rectw, recth);
        PadRectChroma(fdata, fourcc, rectx, recty, rectw, recth);
    }

    static void convert8bTo16b(Frame* frame)
    {
        uint16_t *data_y16b = (uint16_t *)frame->m_recon10->y;
        for (int32_t y = 0; y < frame->m_par->Height; y++) {
            for (int32_t x = 0; x < frame->m_par->Width; x++) {
                data_y16b[y * frame->m_recon10->pitch_luma_pix + x] = (uint16_t)frame->m_recon->y[y * frame->m_recon->pitch_luma_pix + x] << 2;
            }
        }

        uint16_t *data_uv16b = (uint16_t *)frame->m_recon10->uv;
        for (int32_t y = 0; y < frame->m_par->Height / 2; y++) {
            for (int32_t x = 0; x < frame->m_par->Width / 2; x++) {
                data_uv16b[y * frame->m_recon10->pitch_chroma_pix + 2 * x] = (uint16_t)frame->m_recon->uv[y * frame->m_recon->pitch_chroma_pix + 2 * x] << 2;
                data_uv16b[y * frame->m_recon10->pitch_chroma_pix + 2 * x + 1] = (uint16_t)frame->m_recon->uv[y * frame->m_recon->pitch_chroma_pix + 2 * x + 1] << 2;
                //frame->av1frameOrigin.data_v16b[y * frame->av1frameOrigin.pitch + x] = frame->m_recon->uv[y * frame->m_recon->pitch_chroma_pix + 2 * x + 1];
            }
        }
    }
    void Dump_src10enc8pak10(AV1VideoParam *par, Frame* frame, FrameList & dpb)
    {
#if USE_CMODEL_PAK
        mfxU8* fbuf = NULL;
        int32_t W = par->Width - par->CropLeft - par->CropRight;
        int32_t H = par->Height - par->CropTop - par->CropBottom;
        int32_t bd_shift_luma = /*par->bitDepthLuma*/10 > 8;
        int32_t bd_shift_chroma = /*par->bitDepthChroma*/10 > 8;
        int32_t shift_w = frame->m_chromaFormatIdc != MFX_CHROMAFORMAT_YUV444 ? 1 : 0;
        int32_t shift_h = frame->m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420 ? 1 : 0;
        int32_t shift = 2 - shift_w - shift_h;
        int32_t plane_size = (W*H << bd_shift_luma) + (((W*H / 2) << shift) << bd_shift_chroma);

        std::fstream file;
        std::ios_base::openmode mode(std::ios_base::in | std::ios_base::binary);
        if (frame->m_frameOrder == 0)
            mode = std::ios_base::binary | std::ios_base::out;
        file.open(par->reconDumpFileName, mode);
        if (!file.is_open()) return;

        uint64_t seekPos = (par->picStruct == PROGR)
            ? uint64_t(frame->m_frameOrder)   * (plane_size)
            : uint64_t(frame->m_frameOrder / 2) * (plane_size * 2);
        file.seekp(seekPos, std::ios_base::beg)

        CmodelAv1::Av1EncoderFrame* recon = &frame->av1frameRecon;
        mfxU16 *p = recon->data_y16b + (par->CropLeft + par->CropTop * recon->pitch);

        for (int32_t i = 0; i < H; i++) {
            file.write((const char*)p, (1 << bd_shift_luma)* W);
            p += recon->pitch;
        }

        // writing nv12 to yuv420
        // maxlinesize = 8192
        if (W <= 4096 * 2) { // else invalid dump
                mfxU16 *p = recon->data_u16b;
                for (int32_t i = 0; i < H/2; i++) {
                    file.write((const char*)p, (1 << bd_shift_chroma)*W / 2);
                    p += recon->pitch;
                }
                /*mfxU16 **/p = recon->data_v16b;
                for (int32_t i = 0; i < H / 2; i++) {
                    file.write((const char*)p, (1 << bd_shift_chroma)*W / 2);
                    p += recon->pitch;
                }
        }

        file.close();
#else

        //convert8bTo16b(frame);

       // mfxU8* fbuf = NULL;
        int32_t W = par->Width - par->CropLeft - par->CropRight;
        int32_t H = par->Height - par->CropTop - par->CropBottom;
        int32_t bd_shift_luma = /*par->bitDepthLuma*/10 > 8;
        int32_t bd_shift_chroma = /*par->bitDepthChroma*/10 > 8;
        int32_t shift_w = frame->m_chromaFormatIdc != MFX_CHROMAFORMAT_YUV444 ? 1 : 0;
        int32_t shift_h = frame->m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420 ? 1 : 0;
        int32_t shift = 2 - shift_w - shift_h;
        int32_t plane_size = (W*H << bd_shift_luma) + (((W*H / 2) << shift) << bd_shift_chroma);

        std::fstream file;
        std::ios_base::openmode mode(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
        if (frame->m_frameOrder == 0)
            mode = std::ios_base::binary | std::ios_base::out;
        file.open(par->reconDumpFileName, mode);
        if (!file.is_open()) return;

        uint64_t seekPos = (par->picStruct == PROGR)
            ? uint64_t(frame->m_frameOrder)   * (plane_size)
            : uint64_t(frame->m_frameOrder / 2) * (plane_size * 2);
        file.seekp(seekPos, std::ios_base::beg);

        //CmodelAv1::Av1EncoderFrame* recon = &frame->av1frameRecon;
        uint16_t *data_y16b = (uint16_t *)frame->m_recon10->y;
        mfxU16 *p = data_y16b + (par->CropLeft + par->CropTop * frame->m_recon10->pitch_luma_pix);

        for (int32_t i = 0; i < H; i++) {
            file.write((const char*)p, (1 << bd_shift_luma)* W);
            p += frame->m_recon10->pitch_luma_pix;
        }

        // options: P010 or I010???
#if 0
        // P010
        if (W <= 4096 * 2) { // else invalid dump
            mfxU16 *p = (uint16_t *)frame->m_recon10->uv;
            for (int32_t i = 0; i < H / 2; i++) {
                file.write((const char*)p, (1 << bd_shift_chroma)*W);
                p += frame->m_recon10->pitch_chroma_pix;
            }
        }
#else
        // writing nv12 to yuv420 (i010)
        // maxlinesize = 8192
        std::vector<uint16_t> data_u16b(W/2*H/2);
        std::vector<uint16_t> data_v16b(W/2*H/2);
        uint16_t *src_uv16b = (uint16_t *)frame->m_recon10->uv;
        int pitchU = W / 2;
        for (int32_t y = 0; y < H / 2; y++) {
            for (int32_t x = 0; x < W / 2; x++) {
                data_u16b[y*pitchU + x] = src_uv16b[y * frame->m_recon10->pitch_chroma_pix + 2 * x];
                data_v16b[y*pitchU + x] = src_uv16b[y * frame->m_recon10->pitch_chroma_pix + 2 * x + 1];
            }
        }


        if (W <= 4096 * 2) { // else invalid dump
            p = (mfxU16 *)data_u16b.data();
            for (int32_t i = 0; i < H / 2; i++) {
                file.write((const char*)p, (1 << bd_shift_chroma)*W/2);
                p += pitchU;
            }
            p = data_v16b.data();
            for (int32_t i = 0; i < H / 2; i++) {
                file.write((const char*)p, (1 << bd_shift_chroma)*W / 2);
                p += pitchU;
            }
        }

        file.close();
#endif
#endif
    }

    void Dump(AV1VideoParam *par_, Frame* frame, FrameList & dpb)
    {
        if (!par_->doDumpRecon)
            return;

        if (par_->src10Enable) {
            Dump_src10enc8pak10(par_, frame, dpb);
            return;
        }

        AV1VideoParam param = *par_;
        AV1VideoParam *par = &param;
        if (par_->superResFlag)
            param.Width = param.sourceWidth;

        int32_t W = par->Width - par->CropLeft - par->CropRight;
        int32_t H = par->Height - par->CropTop - par->CropBottom;
        int32_t bd_shift_luma = par->bitDepthLuma > 8;
        int32_t bd_shift_chroma = par->bitDepthChroma > 8;
        int32_t shift_w = frame->m_chromaFormatIdc != MFX_CHROMAFORMAT_YUV444 ? 1 : 0;
        int32_t shift_h = frame->m_chromaFormatIdc == MFX_CHROMAFORMAT_YUV420 ? 1 : 0;
        int32_t shift = 2 - shift_w - shift_h;
        int32_t plane_size = (W*H << bd_shift_luma) + (((W*H / 2) << shift) << bd_shift_chroma);

        std::fstream file;
        std::ios_base::openmode mode(std::ios_base::in | std::ios_base::out | std::ios_base::binary);
        if (frame->m_frameOrder == 0)
            mode = std::ios_base::binary | std::ios_base::out;
        file.open(par->reconDumpFileName, mode);
        if (!file.is_open()) return;

        uint64_t seekPos = (par->picStruct == PROGR)
            ? uint64_t(frame->m_frameOrder)   * (plane_size)
            : uint64_t(frame->m_frameOrder / 2) * (plane_size * 2);
        file.seekp(seekPos, std::ios_base::beg);

        FrameData* recon = par_->superResFlag ? frame->m_reconUpscale : frame->m_recon;
        mfxU8 *p = recon->y + ((par->CropLeft + par->CropTop * recon->pitch_luma_pix) << bd_shift_luma);
        for (int32_t i = 0; i < H; i++) {
            if (par->picStruct != PROGR && frame->m_bottomFieldFlag)
                file.seekp(W << bd_shift_luma, std::ios_base::cur);

            file.write((const char*)p, (1 << bd_shift_luma)*W);
            p += recon->pitch_luma_bytes;

            if (par->picStruct != PROGR && !frame->m_bottomFieldFlag)
                file.seekp(W << bd_shift_luma, std::ios_base::cur);
        }

        // writing nv12 to yuv420
        // maxlinesize = 8192
        if (W <= 4096 * 2) { // else invalid dump
            if (bd_shift_chroma) {
                mfxU16 uvbuf[4096];
                for (int part = 0; part <= 1; part++) {
                    uint16_t *puv = (uint16_t*)recon->uv + part + (par->CropLeft >> shift_w << 1) + (par->CropTop >> shift_h) * recon->pitch_chroma_pix;
                    for (int32_t i = 0; i < H >> shift_h; i++) {
                        for (int j = 0; j < W >> shift_w; j++)
                            uvbuf[j] = puv[2 * j];
                        if (par->picStruct != PROGR && frame->m_bottomFieldFlag)
                            file.seekp(W << bd_shift_chroma >> shift_w, std::ios_base::cur);
                        file.write((const char*)uvbuf, 2 * (W >> shift_w));
                        puv += recon->pitch_chroma_pix;
                        if (par->picStruct != PROGR && !frame->m_bottomFieldFlag)
                            file.seekp(W << bd_shift_chroma >> shift_w, std::ios_base::cur);
                    }
                }
            }
            else {
                mfxU8 uvbuf[4096];
                for (int part = 0; part <= 1; part++) {
                    p = recon->uv + part + (par->CropLeft >> shift_w << 1) + (par->CropTop >> shift_h) * recon->pitch_chroma_pix;
                    for (int32_t i = 0; i < H >> shift_h; i++) {
                        for (int j = 0; j < W >> shift_w; j++)
                            uvbuf[j] = p[2 * j];
                        if (par->picStruct != PROGR && frame->m_bottomFieldFlag)
                            file.seekp(W << bd_shift_chroma >> shift_w, std::ios_base::cur);
                        file.write((const char*)uvbuf, (W >> shift_w));
                        p += recon->pitch_chroma_pix;
                        if (par->picStruct != PROGR && !frame->m_bottomFieldFlag)
                            file.seekp(W << bd_shift_chroma >> shift_w, std::ios_base::cur);
                    }
                }
            }
        }

        file.close();
    }

    TileContexts::TileContexts() {
        tileSb64Cols = 0;
        tileSb64Rows = 0;
        aboveNonzero[0] = NULL;
        aboveNonzero[1] = NULL;
        aboveNonzero[2] = NULL;
        leftNonzero[0] = NULL;
        leftNonzero[1] = NULL;
        leftNonzero[2] = NULL;
        abovePartition = NULL;
        leftPartition = NULL;
        aboveTxfm = NULL;
        leftTxfm = NULL;
#ifdef ADAPTIVE_DEADZONE
        adzctx = NULL;
#ifdef ADAPTIVE_DEADZONE_TEMPORAL_ONLY
        adzctxDelta = NULL;
#endif
#endif
    }
    void TileContexts::Alloc(int32_t tileSb64Cols_, int32_t tileSb64Rows_) {
        tileSb64Cols = tileSb64Cols_;
        tileSb64Rows = tileSb64Rows_;
        aboveNonzero[0] = (uint8_t *)AV1_Malloc(16 * tileSb64Cols);
        ThrowIf(!aboveNonzero[0], std::bad_alloc());
        aboveNonzero[1] = (uint8_t *)AV1_Malloc(16 * tileSb64Cols);
        ThrowIf(!aboveNonzero[1], std::bad_alloc());
        aboveNonzero[2] = (uint8_t *)AV1_Malloc(16 * tileSb64Cols);
        ThrowIf(!aboveNonzero[2], std::bad_alloc());
        leftNonzero[0] = (uint8_t *)AV1_Malloc(16 * tileSb64Rows);
        ThrowIf(!leftNonzero[0], std::bad_alloc());
        leftNonzero[1] = (uint8_t *)AV1_Malloc(16 * tileSb64Rows);
        ThrowIf(!leftNonzero[1], std::bad_alloc());
        leftNonzero[2] = (uint8_t *)AV1_Malloc(16 * tileSb64Rows);
        ThrowIf(!leftNonzero[2], std::bad_alloc());
        abovePartition = (uint8_t *)AV1_Malloc(8 * tileSb64Cols);
        ThrowIf(!abovePartition, std::bad_alloc());
        leftPartition = (uint8_t *)AV1_Malloc(8 * tileSb64Rows);
        ThrowIf(!leftPartition, std::bad_alloc());
        aboveTxfm = (uint8_t *)AV1_Malloc(16 * tileSb64Cols);
        ThrowIf(!aboveTxfm, std::bad_alloc());
        leftTxfm = (uint8_t *)AV1_Malloc(16 * tileSb64Rows);
        ThrowIf(!leftTxfm, std::bad_alloc());
#ifdef ADAPTIVE_DEADZONE
        {
#ifdef ADAPTIVE_DEADZONE_TEMPORAL_ONLY
            const int32_t DZTW = tileSb64Cols;
#else
            const int32_t DZTW = 3;
#endif
            adzctx = (ADzCtx **)AV1_Malloc(sizeof(ADzCtx*) * DZTW);
            ThrowIf(!adzctx, std::bad_alloc());
            for (int i = 0; i < DZTW; i++) {
                adzctx[i] = (ADzCtx *)AV1_Malloc(sizeof(ADzCtx) * tileSb64Rows);
                ThrowIf(!adzctx[i], std::bad_alloc());
            }
        }
#ifdef ADAPTIVE_DEADZONE_TEMPORAL_ONLY
        {
            const int32_t DZTW = tileSb64Cols;
            adzctxDelta = (ADzCtx **)AV1_Malloc(sizeof(ADzCtx*) * DZTW);
            ThrowIf(!adzctxDelta, std::bad_alloc());
            for (int i = 0; i < DZTW; i++) {
                adzctxDelta[i] = (ADzCtx *)AV1_Malloc(sizeof(ADzCtx) * tileSb64Rows);
                ThrowIf(!adzctxDelta[i], std::bad_alloc());
            }
        }
#endif
#endif
    }
    void TileContexts::Free() {
        AV1_SafeFree(aboveNonzero[0]);
        AV1_SafeFree(aboveNonzero[1]);
        AV1_SafeFree(aboveNonzero[2]);
        AV1_SafeFree(leftNonzero[0]);
        AV1_SafeFree(leftNonzero[1]);
        AV1_SafeFree(leftNonzero[2]);
        AV1_SafeFree(abovePartition);
        AV1_SafeFree(leftPartition);
        AV1_SafeFree(aboveTxfm);
        AV1_SafeFree(leftTxfm);
#ifdef ADAPTIVE_DEADZONE
        {
#ifdef ADAPTIVE_DEADZONE_TEMPORAL_ONLY
            const int32_t DZTW = tileSb64Cols;
#else
            const int32_t DZTW = 3;
#endif
            for (int i = 0; i < DZTW; i++) {
                AV1_SafeFree(adzctx[i]);
            }
            AV1_SafeFree(adzctx);
        }
#ifdef ADAPTIVE_DEADZONE_TEMPORAL_ONLY
        {
            const int32_t DZTW = tileSb64Cols;
            for (int i = 0; i < DZTW; i++) {
                AV1_SafeFree(adzctxDelta[i]);
            }
            AV1_SafeFree(adzctxDelta);
        }
#endif
#endif
    }
    void TileContexts::Clear() {
        memset(aboveNonzero[0], 0, 16 * tileSb64Cols);
        memset(aboveNonzero[1], 0, 16 * tileSb64Cols);
        memset(aboveNonzero[2], 0, 16 * tileSb64Cols);
        memset(leftNonzero[0], 0, 16 * tileSb64Rows);
        memset(leftNonzero[1], 0, 16 * tileSb64Rows);
        memset(leftNonzero[2], 0, 16 * tileSb64Rows);
        memset(abovePartition, 0, 8 * tileSb64Cols);
        memset(leftPartition, 0, 8 * tileSb64Rows);
        memset(aboveTxfm, tx_size_wide[TX_SIZES_LARGEST], 16 * tileSb64Cols);
        memset(leftTxfm, tx_size_wide[TX_SIZES_LARGEST], 16 * tileSb64Rows);
#ifdef ADAPTIVE_DEADZONE_NO_FRAME_PROPAGATION
        memset(adzctx[0], 0, sizeof(ADzCtx) * tileSb64Rows);
        memset(adzctx[1], 0, sizeof(ADzCtx) * tileSb64Rows);
        memset(adzctx[2], 0, sizeof(ADzCtx) * tileSb64Rows);
#endif
    }

    void Frame::Destroy()
    {
        m_par = NULL;
        AV1_SafeFree(m_ttEncode);
        AV1_SafeFree(m_ttPackTile);
        AV1_SafeFree(m_ttPackRow);
        AV1_SafeFree(m_ttDeblockAndCdef);
        AV1_SafeFree(m_ttLookahead);
        AV1_SafeFree(m_lowres4x);
        AV1_SafeFree(m_ttSubmitGpuMe[LAST_FRAME]);
        AV1_SafeFree(m_ttSubmitGpuMe[GOLDEN_FRAME]);
        AV1_SafeFree(m_ttSubmitGpuMe[ALTREF_FRAME]);
        AV1_SafeFree(m_ttSubmitGpuMd);
        AV1_SafeFree(m_ttWaitGpuMd);
        m_lcuQps.resize(0);
        m_lcuRound.resize(0);
#if USE_CMODEL_PAK
        av1frameOrigin.clearMemory();
        av1frameRecon.clearMemory();
#endif
    }

    void Frame::ResetMemInfo()
    {
        m_par = nullptr;
        m_origin = nullptr;
        m_origin10 = nullptr;
        m_recon = nullptr;
        m_reconUpscale = nullptr;
        m_recon10 = nullptr;
        m_lowres = nullptr;
        m_modeInfo = nullptr;
        m_stats[0] = m_stats[1] = nullptr;
        m_sceneStats = nullptr;

        m_ttEncode = nullptr;
        m_ttDeblockAndCdef = nullptr;
        m_ttPackTile = nullptr;
        m_ttPackRow = nullptr;
        m_ttLookahead = nullptr;
        m_lowres4x = nullptr;
        m_ttSubmitGpuMe[LAST_FRAME] = nullptr;
        m_ttSubmitGpuMe[GOLDEN_FRAME] = nullptr;
        m_ttSubmitGpuMe[ALTREF_FRAME] = nullptr;
        m_ttSubmitGpuMd = nullptr;
        m_ttWaitGpuMd = nullptr;
    }

    void Frame::ResetEncInfo()
    {
        m_fenc = nullptr;
        m_timeStamp = 0;
        m_picCodeType = 0;
        m_RPSIndex = 0;

        m_wasLookAheadProcessed = 0;
        m_lookaheadRefCounter = 0;

        m_picStruct = PROGR;
        m_secondFieldFlag = 0;
        m_bottomFieldFlag = 0;
        m_pyramidLayer = 0;
        m_temporalId = 0;
        m_miniGopCount = 0;
        m_biFramesInMiniGop = 0;
        m_frameOrder = 0;
        m_frameOrderOfLastIdr = 0;
        m_frameOrderOfLastIntra = 0;
        m_frameOrderOfLastBaseTemporalLayer = 0;
        m_frameOrderOfLastIntraInEncOrder = 0;
        m_frameOrderOfLastAnchor = 0;
        m_frameOrderOfLastExternalLTR = MFX_FRAMEORDER_UNKNOWN;

        m_poc = 0;
        m_encOrder = 0;
        m_isShortTermRef = 0;
        m_isIdrPic = 0;
        m_isRef = 0;
        memset(m_refPicList, 0, sizeof(m_refPicList));
        memset(&refctrl, 0, sizeof(mfxExtAVCRefListCtrl));
        m_hasRefCtrl = 0;
        m_isErrorResilienceFrame = 0;

        m_origin = NULL;
        m_origin10= NULL;
        m_recon  = NULL;
        m_recon10= NULL;
        m_reconUpscale = NULL;
        m_lowres = NULL;
        m_encOrder    = uint32_t(-1);
        m_frameOrder  = uint32_t(-1);
        m_timeStamp   = 0;
        m_dpbSize     = 0;
        m_sceneOrder  = 0;
        m_temporalSync = 0;
        m_temporalActv = 0;
        m_forceTryIntra = 0;
        m_sliceQpY    = 0;
        m_sliceQpBrc = 0;
        m_hasMbqpCtrl = 0;
        memset(&mbqpctrl, 0, sizeof(mfxExtMBQP));

        m_maxFrameSizeInBitsSet = 0;
        m_fzCmplx = 0.0;
        m_fzCmplxK = 0.0;
        m_fzRateE = 0.0;
        m_avCmplx      = 0.0;
        m_CmplxQstep   = 0.0;
        m_qpBase       = 0;
        m_qsMinForMaxFrameSize = 0.0;
        m_qpMinForMaxFrameSize = 0;
        m_totAvComplx  = 0.0;
        m_totComplxCnt = 0;
        m_complxSum    = 0.0;
        m_predBits     = 0;
        m_cmplx        = 0.0;
        m_refQp        = -1;

        if (m_stats[0]) {
            m_stats[0]->ResetAvgMetrics();
        }
        if (m_stats[1]) {
            m_stats[1]->ResetAvgMetrics();
        }

        m_futureFrames.resize(0);

        Zero(m_feiInterData);
        m_feiModeInfo1 = nullptr;
        m_feiModeInfo2 = nullptr;
#if GPU_VARTX
        m_feiAdz = nullptr;
        m_feiAdzDelta = nullptr;
        m_feiVarTxInfo = nullptr;
        m_feiCoefs = nullptr;
#endif // GPU_VARTX
        for (int32_t i = 0; i < 4; i++) {
            m_feiRs[i] = nullptr;
            m_feiCs[i] = nullptr;
        }

        m_feiOrigin = nullptr;
        m_feiRecon = nullptr;

        m_userSeiMessages.resize(0);
        m_userSeiMessagesData.resize(0);

        m_ttEncComplete.InitEncComplete(this, 0);
        m_ttInitNewFrame.InitNewFrame(this, (mfxFrameSurface1 *)NULL, 0);

        m_ttSubmitGpuCopySrc.InitGpuSubmit(this, MFX_FEI_H265_OP_GPU_COPY_SRC, 0);

        m_ttSubmitGpuCopyRec.InitGpuSubmit(this, MFX_FEI_H265_OP_GPU_COPY_REF, 0);
        m_ttWaitGpuCopyRec.InitGpuWait(MFX_FEI_H265_OP_GPU_COPY_REF, 0);

        m_ttFrameRepetition.InitRepeatFrame(this, 0);

        // VP9 specific
        Zero(m_dpbVP9);
        Zero(m_dpbVP9NextFrame);
        Zero(m_contexts);
        Zero(m_refFramesContextsVp9);
        Zero(m_refFramesContextsAv1);
        intraOnly = 0;
        refreshFrameFlags = 0;
        referenceMode = SINGLE_REFERENCE;
        Zero(refFrameIdx);
        Zero(refFrameSignBiasVp9);
        Zero(refFrameSignBiasAv1);
        compoundReferenceAllowed = 0;
        segmentationEnabled = 0;
        segmentationUpdateMap = 0;
        showFrame = 1;
        showExistingFrame = 0;
        frameToShowMapIdx = 0;
        frameSizeInBytes = 0;
        compFixedRef = 0;
        Zero(compVarRef);
        m_prevFrame = NULL;
        m_prevFrameIsOneOfRefs = false;
        Zero(m_loopFilterParam);
        m_isLtrFrame = 0;
        m_isExternalLTR = 0;
        m_shortTermRefFlag = 0;
        m_ltrConfidenceLevel = 0;
        m_ltrDQ = 0;
        showExistingFrame = 0;

        for (size_t i = 0; i < m_tileContexts.size(); i++)
            m_tileContexts[i].Clear();
    }
}

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
