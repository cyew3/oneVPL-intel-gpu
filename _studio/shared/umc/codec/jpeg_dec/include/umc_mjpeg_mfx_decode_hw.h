/* ////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright(c) 2003-2014 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __UMC_MJPEG_VIDEO_DECODER_MFX_DECODE_HW_H
#define __UMC_MJPEG_VIDEO_DECODER_MFX_DECODE_HW_H

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)
#include "ippdefs.h"
#include "ippcore.h"
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"
#include "ippj.h"
#include "umc_structures.h"
#include "umc_mjpeg_mfx_decode.h"
#include "umc_va_base.h"

#ifdef UMC_VA_DXVA
#include "umc_va_dxva2.h"
#endif // #ifdef UMC_VA_DXVA

#if defined(UMC_VA_LINUX)
 #include <va/va_dec_jpeg.h>
#endif
#include <set>
#include <algorithm>

// internal JPEG decoder object forward declaration
class CBaseStreamInput;
class CJPEGDecoder;

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

class MJPEGVideoDecoderMFX_HW : public MJPEGVideoDecoderMFX
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

    Status _DecodeHeader(CBaseStreamInput* in, Ipp32s* nUsedBytes);

    virtual Status _DecodeField(MediaDataEx* in);

#if defined(UMC_VA)
    Status PackHeaders(MediaData* src, JPEG_DECODE_SCAN_PARAMETER* obtainedScanParams, Ipp8u* buffersForUpdate);
#endif

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
};

} // end namespace UMC


#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif //__UMC_MJPEG_VIDEO_DECODER_MFX_DECODE_HW_H
