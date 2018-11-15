// Copyright (c) 2003-2018 Intel Corporation
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

#ifndef __UMC_MJPEG_VIDEO_DECODER_MFX_DECODE_HW_H
#define __UMC_MJPEG_VIDEO_DECODER_MFX_DECODE_HW_H

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)

#include "umc_va_base.h"

#if defined(UMC_VA)

#include "umc_mjpeg_mfx_decode_base.h"
#include <set>

#ifdef UMC_VA_DXVA
#include "umc_va_dxva2.h"
#endif // #ifdef UMC_VA_DXVA

#if defined(UMC_VA_LINUX)
 #include <va/va_dec_jpeg.h>
#endif

namespace UMC
{
#if defined(UMC_VA_LINUX)
typedef struct tagJPEG_DECODE_SCAN_PARAMETER
{
    Ipp16u        NumComponents;
    Ipp8u         ComponentSelector[4];
    Ipp8u         DcHuffTblSelector[4];
    Ipp8u         AcHuffTblSelector[4];
    Ipp16u        RestartInterval;
    Ipp32u        MCUCount;
    Ipp16u        ScanHoriPosition;
    Ipp16u        ScanVertPosition;
    Ipp32u        DataOffset;
    Ipp32u        DataLength;
} JPEG_DECODE_SCAN_PARAMETER;

typedef struct tagJPEG_DECODE_QUERY_STATUS
{
    Ipp32u        StatusReportFeedbackNumber;
    Ipp8u         bStatus;
    Ipp8u         reserved8bits;
    Ipp16u        reserved16bits;
} JPEG_DECODE_QUERY_STATUS;
#endif

class MJPEGVideoDecoderMFX_HW : public MJPEGVideoDecoderBaseMFX
{
public:
    // Default constructor
    MJPEGVideoDecoderMFX_HW(void);

    // Destructor
    virtual ~MJPEGVideoDecoderMFX_HW(void);

    // Initialize for subsequent frame decoding.
    virtual Status Init(BaseCodecParams* init);

    // Reset decoder to initial state
    virtual Status Reset(void);

    // Close decoding & free all allocated resources
    virtual Status Close(void);

    // Allocate the destination frame
    virtual Status AllocateFrame();

    // Close the frame being decoded
    Status CloseFrame(UMC::FrameData** in, const mfxU32  fieldPos);

    // Get next frame
    virtual Status GetFrame(UMC::MediaDataEx *pSrcData, UMC::FrameData** out, const mfxU32  fieldPos);

    virtual ConvertInfo * GetConvertInfo();

    Ipp32u GetStatusReportNumber() {return m_statusReportFeedbackCounter;}

    mfxStatus CheckStatusReportNumber(Ipp32u statusReportFeedbackNumber, mfxU16* corrupted);

    void   SetFourCC(Ipp32u fourCC) {m_fourCC = fourCC;}

protected:

    Status _DecodeField();

    Status _DecodeHeader(int32_t* nUsedBytes);

    virtual Status _DecodeField(MediaDataEx* in);


    Status PackHeaders(MediaData* src, JPEG_DECODE_SCAN_PARAMETER* obtainedScanParams, Ipp8u* buffersForUpdate);

    Status GetFrameHW(MediaDataEx* in);
    Status DefaultInitializationHuffmantables();

    Ipp16u GetNumScans(MediaDataEx* in);

    Ipp32u  m_statusReportFeedbackCounter;
    ConvertInfo m_convertInfo;

    Ipp32u m_fourCC;

    Mutex m_guard;
    std::set<mfxU32> m_submittedTaskIndex;
    std::set<mfxU32> m_cachedReadyTaskIndex;
    std::set<mfxU32> m_cachedCorruptedTaskIndex;
    VideoAccelerator *      m_va;
};

} // end namespace UMC

#endif //#if defined(UMC_VA)
#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif //__UMC_MJPEG_VIDEO_DECODER_MFX_DECODE_HW_H
