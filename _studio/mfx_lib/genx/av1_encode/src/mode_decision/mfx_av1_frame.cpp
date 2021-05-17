// Copyright (c) 2014-2019 Intel Corporation
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

#include "mfx_av1_defs.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_frame.h"
#include "mfx_av1_enc.h"

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

    // template to align a pointer
    template<class T> inline
        T align_pointer(void *pv, size_t lAlignValue = 16)
    {
        // some compilers complain to conversion to/from
        // pointer types from/to integral types.
        return (T) ((((uint8_t *) pv - (uint8_t *) 0) + (lAlignValue - 1)) &
            ~(lAlignValue - 1));
    }


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

        y = align_pointer<uint8_t *>(mem + vertPaddingSizeInBytesLu, allocInfo.alignment);
        uv = y + allocInfo.sizeInBytesLu - vertPaddingSizeInBytesLu + vertPaddingSizeInBytesCh;
        uv = align_pointer<uint8_t *>(uv, allocInfo.alignment);
        y  = align_pointer<uint8_t *>(y + (allocInfo.paddingLu << bdShiftLu), 64);
        uv = align_pointer<uint8_t *>(uv + (allocInfo.paddingChW << bdShiftCh), 64);
    }


    void FrameData::Destroy()
    {
        delete [] mem;
        mem = NULL;
    }

    void Frame::Create(const H265VideoParam *par)
    {
        int32_t numCtbs = par->PicWidthInCtbs * par->PicHeightInCtbs;
        m_lcuQps.resize(numCtbs);
        m_bitDepthLuma =  par->bitDepthLuma;
        m_bitDepthChroma = par->bitDepthChroma;
        m_bdLumaFlag = par->bitDepthLuma > 8;
        m_bdChromaFlag = par->bitDepthChroma > 8;
        m_chromaFormatIdc = par->chromaFormatIdc;

        const int32_t numTiles = par->tileParam.cols * par->tileParam.rows;

        // VP9 specific
        m_tileContexts.resize(numTiles);
        for (int32_t tr = 0, ti = 0; tr < par->tileParam.rows; tr++)
            for (int32_t tc = 0; tc < par->tileParam.cols; tc++, ti++)
                m_tileContexts[ti].Alloc(par->tileParam.colWidth[tc], par->tileParam.rowHeight[tr]);

        m_tplMvs = (TplMvRef *)H265_Malloc((par->miRows + 8) * par->miPitch * sizeof(*m_tplMvs));
        if (!m_tplMvs)
            throw std::exception();
        m_txkTypes4x4 = (int32_t*)H265_Malloc(par->sb64Rows * par->sb64Cols * sizeof(int32_t) * 256);
        if (!m_txkTypes4x4)
            throw std::exception();

        const int32_t width  = AlignValue(par->Width,  64);
        const int32_t height = AlignValue(par->Height, 64);
        const int32_t w8  = width / 8,  h8  = height / 8;
        const int32_t w16 = width / 16, h16 = height / 16;
        const int32_t w32 = width / 32, h32 = height / 32;
        const int32_t w64 = width / 64, h64 = height / 64;
        for (int32_t i = 0; i < 3; i++) {
            for (int32_t sz = 0; sz < 4; sz++) {
                const int32_t dataSize = sz < 2 ? 8 : 64;
                const int32_t blockSize = 3 << sz;
                const int32_t pitch = AlignValue((width / blockSize) * dataSize, 64);
                m_feiInterData[i][sz]->Alloc((height / blockSize) * pitch, pitch);
            }
        }

        for (int32_t i = 0; i < 4; i++) {
            const int32_t pitch = AlignValue(par->sb64Cols * 64, 64);
            m_feiInterp[i]->Alloc(par->sb64Rows * par->sb64Cols * 4096, pitch);
        }
    }

    template <typename T> void ippsSet(T val, T *p, int32_t len) { std::fill(p, p + len, val); }
    template <typename T> void ippsCopy(const T *s, T *d, int32_t len) { memcpy(d, s, len * sizeof(T)); }

    template <class T> void PadRect(T *plane, int32_t pitch, int32_t width, int32_t height, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth, int32_t padL, int32_t padR, int32_t padh)
    {
        rectx = Saturate(0, width - 1, rectx);
        recty = Saturate(0, height - 1, recty);
        rectw = Saturate(1, width - rectx, rectw);
        recth = Saturate(1, height - recty, recth);

        if (rectx == 0) {
            rectx -= padL;
            rectw += padL;
            for (int32_t y = recty; y < recty + recth; y++)
                ippsSet(plane[y * pitch], plane + y * pitch - padL, padL);
        }
        if (rectx + rectw == width) {
            rectw += padR;
            for (int32_t y = recty; y < recty + recth; y++)
                ippsSet(plane[y * pitch + width - 1], plane + y * pitch + width, padR);
        }
        if (recty == 0)
            for (int32_t j = 1; j <= padh; j++)
                ippsCopy(plane + rectx, plane + rectx - j * pitch, rectw);
        if (recty + recth == height)
            for (int32_t j = 1; j <= padh; j++)
                ippsCopy(plane + (height - 1) * pitch + rectx, plane + (height - 1 + j) * pitch + rectx, rectw);
    }
    template void PadRect<uint8_t>(uint8_t *plane, int32_t pitch, int32_t width, int32_t height, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth,  int32_t padL, int32_t padR, int32_t padh);
    template void PadRect<uint16_t>(uint16_t *plane, int32_t pitch, int32_t width, int32_t height, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth, int32_t padL, int32_t padR, int32_t padh);
    template void PadRect<uint32_t>(uint32_t *plane, int32_t pitch, int32_t width, int32_t height, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth, int32_t padL, int32_t padR, int32_t padh);

    void PadRectLuma(const FrameData &fdata, int32_t fourcc, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth)
    {
        // work-around for 8x and 16x downsampling on gpu
        // currently DS kernel expect right border is padded up to pitch
        int32_t paddingL = fdata.padding;
        int32_t paddingR = fdata.padding;
        int32_t bppShift = 0;
        if (fdata.m_handle) {
            paddingR = fdata.pitch_luma_pix - fdata.width - (AlignValue(fdata.padding << bppShift, 64) >> bppShift);
            paddingL = AlignValue(fdata.padding << bppShift, 64) >> bppShift;
        }

        PadRect((uint8_t *)fdata.y, fdata.pitch_luma_pix, fdata.width, fdata.height, rectx, recty, rectw, recth, paddingL,paddingR, fdata.padding);
    }

    void PadRectChroma(const FrameData &fdata, int32_t fourcc, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth)
    {
        int32_t shiftH = 1;
        PadRect((uint16_t *)fdata.uv, fdata.pitch_chroma_pix/2, fdata.width/2, fdata.height>>shiftH, rectx/2, recty>>shiftH, rectw/2, recth>>shiftH, fdata.padding/2, fdata.padding/2, fdata.padding>>shiftH);
    }

    void PadRectLumaAndChroma(const FrameData &fdata, int32_t fourcc, int32_t rectx, int32_t recty, int32_t rectw, int32_t recth)
    {
        PadRectLuma(fdata, fourcc, rectx, recty, rectw, recth);
        PadRectChroma(fdata, fourcc, rectx, recty, rectw, recth);
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
    }
    void TileContexts::Alloc(int32_t tileSb64Cols_, int32_t tileSb64Rows_) {
        tileSb64Cols = tileSb64Cols_;
        tileSb64Rows = tileSb64Rows_;
        aboveNonzero[0] = (uint8_t *)H265_Malloc(16 * tileSb64Cols);
        ThrowIf(!aboveNonzero[0], std::bad_alloc());
        aboveNonzero[1] = (uint8_t *)H265_Malloc(16 * tileSb64Cols);
        ThrowIf(!aboveNonzero[1], std::bad_alloc());
        aboveNonzero[2] = (uint8_t *)H265_Malloc(16 * tileSb64Cols);
        ThrowIf(!aboveNonzero[2], std::bad_alloc());
        leftNonzero[0] = (uint8_t *)H265_Malloc(16 * tileSb64Rows);
        ThrowIf(!leftNonzero[0], std::bad_alloc());
        leftNonzero[1] = (uint8_t *)H265_Malloc(16 * tileSb64Rows);
        ThrowIf(!leftNonzero[1], std::bad_alloc());
        leftNonzero[2] = (uint8_t *)H265_Malloc(16 * tileSb64Rows);
        ThrowIf(!leftNonzero[2], std::bad_alloc());
        abovePartition = (uint8_t *)H265_Malloc(8 * tileSb64Cols);
        ThrowIf(!abovePartition, std::bad_alloc());
        leftPartition = (uint8_t *)H265_Malloc(8 * tileSb64Rows);
        ThrowIf(!leftPartition, std::bad_alloc());
        aboveTxfm = (uint8_t *)H265_Malloc(16 * tileSb64Cols);
        ThrowIf(!aboveTxfm, std::bad_alloc());
        leftTxfm = (uint8_t *)H265_Malloc(16 * tileSb64Rows);
        ThrowIf(!leftTxfm, std::bad_alloc());
    }
    void TileContexts::Free() {
        H265_SafeFree(aboveNonzero[0]);
        H265_SafeFree(aboveNonzero[1]);
        H265_SafeFree(aboveNonzero[2]);
        H265_SafeFree(leftNonzero[0]);
        H265_SafeFree(leftNonzero[1]);
        H265_SafeFree(leftNonzero[2]);
        H265_SafeFree(abovePartition);
        H265_SafeFree(leftPartition);
        H265_SafeFree(aboveTxfm);
        H265_SafeFree(leftTxfm);
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
    }

    void Frame::Destroy()
    {
        H265_SafeFree(m_tplMvs);
        H265_SafeFree(m_txkTypes4x4);
    }

    void Frame::ResetMemInfo()
    {
        m_origin = NULL;
        m_recon = NULL;
        m_lowres = NULL;
        cu_data = NULL;
        m_tplMvs = NULL;
        m_txkTypes4x4 = NULL;
    }

    void Frame::ResetEncInfo()
    {
        m_fenc = NULL;
        m_timeStamp = 0;
        m_picCodeType = 0;
        m_RPSIndex = 0;

        m_wasLookAheadProcessed = 0;
        m_lookaheadRefCounter = 0;

        m_picStruct = 1;  //PROGR;
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
        m_isIdrPic = 0;
        m_isRef = 0;
        memset(m_refPicList, 0, sizeof(m_refPicList));

        m_origin = NULL;
        m_recon  = NULL;
        m_lowres = NULL;
        m_encOrder    = uint32_t(-1);
        m_frameOrder  = uint32_t(-1);
        m_timeStamp   = 0;
        m_dpbSize     = 0;
        m_sceneOrder  = 0;
        m_sliceQpY    = 0;

        m_avCmplx      = 0.0;
        m_CmplxQstep   = 0.0;
        m_qpBase       = 0;
        m_totAvComplx  = 0.0;
        m_totComplxCnt = 0;
        m_complxSum    = 0.0;
        m_predBits     = 0;
        m_cmplx        = 0.0;
        m_refQp        = -1;

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

        for (size_t i = 0; i < m_tileContexts.size(); i++)
            m_tileContexts[i].Clear();
    }
}
