/* ////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2013 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __UMC_MJPEG_VIDEO_DECODER_MFX_DECODE_H
#define __UMC_MJPEG_VIDEO_DECODER_MFX_DECODE_H

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)

#include <memory>

#include "ippdefs.h"
#include "ippcore.h"
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"
#include "ippj.h"
#include "umc_structures.h"
#include "umc_video_decoder.h"

#include "umc_frame_data.h"
#include "umc_frame_allocator.h"
#include "jpegdec.h"

#include "umc_jpeg_frame_constructor.h"

#include "mfxvideo++int.h"

// internal JPEG decoder object forward declaration
class CBaseStreamInput;
class CJPEGDecoder;
class CJpegTask;
class CJpegTaskBuffer;

namespace UMC
{

enum
{
    JPEG_MAX_THREADS_HW = 1,
    JPEG_MAX_THREADS = 4
};

typedef struct
{
    ChromaType colorFormat;
    size_t UOffset;
    size_t VOffset;
} ConvertInfo;

class MJPEGVideoDecoderMFX
{
public:
    // Default constructor
    MJPEGVideoDecoderMFX(void);

    // Destructor
    virtual ~MJPEGVideoDecoderMFX(void);

    // Initialize for subsequent frame decoding.
    virtual Status Init(BaseCodecParams* init);

    // Reset decoder to initial state
    virtual Status Reset(void);

    // Close decoding & free all allocated resources
    virtual Status Close(void);

    virtual FrameData *GetDst(void);

    // Get next frame
    virtual Status DecodePicture(const CJpegTask &task, const mfxU32 threadNumber, const mfxU32 callNumber);

    virtual void SetFrameAllocator(FrameAllocator * frameAllocator);

    Status DecodeHeader(MediaData* in);

    Status FillVideoParam(mfxVideoParam *par, bool full);

    Status FillQuantTableExtBuf(mfxExtJPEGQuantTables* quantTables);

    Status FillHuffmanTableExtBuf(mfxExtJPEGHuffmanTables* huffmanTables);

    virtual ConvertInfo * GetConvertInfo();

    ChromaType GetChromaType();

    JCOLOR GetColorType();

    // All memory sizes should come in size_t type
    Status _GetFrameInfo(const Ipp8u* pBitStream, size_t nSize);

    // Allocate the destination frame
    virtual Status AllocateFrame();

    // Close the frame being decoded
    Status CloseFrame(void);

    // Do post processing
    virtual Status PostProcessing(Ipp64f ptr);

    // Get the number of decoders allocated
    inline
    mfxU32 NumDecodersAllocated(void) const;

    // Skip extra data at the begiging of stream
    Status FindStartOfImage(MediaData * in);

    Status SetRotation(Ipp16u rotation);

    Status SetColorSpace(Ipp16u chromaFormat, Ipp16u colorFormat);

protected:

    virtual void AdjustFrameSize(IppiSize & size);

    Status DecodePiece(const mfxU32 fieldNum,
                       const mfxU32 restartNum,
                       const mfxU32 restartsToDecode,
                       const mfxU32 threadNum);

    Status _DecodeHeader(CBaseStreamInput* in, Ipp32s* nUsedBytes, const Ipp32u threadNum);

    bool                    m_IsInit;
    //bool                    m_firstFrame;
    //bool                    m_firstField;
    bool                    m_interleaved;
    bool                    m_interleavedScan;
    //bool                    m_needCloseFrame;
    VideoDecoderParams      m_DecoderParams;
    Ipp32s                  m_frameNo;

    MemID                   m_frameMID;
    FrameData               m_frameData;
    JCOLOR                  m_color;
    VideoData               m_internalFrame;
    bool                    m_needPostProcessing;
    Ipp16u                  m_rotation;


    Ipp8u*                  m_frame;
    IppiSize                m_frameDims;
    Ipp32s                  m_frameChannels;
    Ipp32s                  m_framePrecision;
    int                     m_frameSampling;

    // JPEG decoders allocated
    std::auto_ptr<CJPEGDecoder> m_dec[JPEG_MAX_THREADS];
    // Number of the decoders allocated
    mfxU32                  m_numDec;

    // Pointer to the last buffer decoded. It is required to check if header was already decoded.
    const CJpegTaskBuffer *(m_pLastPicBuffer[JPEG_MAX_THREADS]);

    Ipp64f                  m_local_frame_time;
    Ipp64f                  m_local_delta_frame_time;

    MemoryAllocator*        m_pMemoryAllocator; // (MemoryAllocator*) pointer to memory allocator
    std::auto_ptr<BaseCodec>  m_PostProcessing; // (BaseCodec*) pointer to post processing

    FrameAllocator *        m_frameAllocator;
    VideoAccelerator *      m_va;
};

inline
mfxU32 MJPEGVideoDecoderMFX::NumDecodersAllocated(void) const
{
    return m_numDec;

} // mfxU32 MJPEGVideoDecoderMFX::NumDecodersAllocated(void) const

inline mfxU16 GetMFXChromaFormat(ChromaType type)
{
    mfxU16 chromaFormat = MFX_CHROMAFORMAT_MONOCHROME;
    switch(type)
    {
    case CHROMA_TYPE_YUV400:
        chromaFormat = MFX_CHROMAFORMAT_MONOCHROME;
        break;
    case CHROMA_TYPE_YUV420:
        chromaFormat = MFX_CHROMAFORMAT_YUV420;
        break;
    case CHROMA_TYPE_YUV422H_2Y:
        chromaFormat = MFX_CHROMAFORMAT_YUV422H;
        break;
    case CHROMA_TYPE_YUV444:
        chromaFormat = MFX_CHROMAFORMAT_YUV444;
        break;
    case CHROMA_TYPE_YUV411:
        chromaFormat = MFX_CHROMAFORMAT_YUV411;
        break;
    case CHROMA_TYPE_YUV422V_2Y:
        chromaFormat = MFX_CHROMAFORMAT_YUV422V;
        break;
    case CHROMA_TYPE_YUV422H_4Y:
        chromaFormat = MFX_CHROMAFORMAT_YUV422H;
        break;
    case CHROMA_TYPE_YUV422V_4Y:
        chromaFormat = MFX_CHROMAFORMAT_YUV422V;
        break;
    default:
        VM_ASSERT(false);
        break;
    };

    return chromaFormat;
}

inline mfxU16 GetMFXColorFormat(JCOLOR color)
{
    mfxU16 colorFormat = MFX_JPEG_COLORFORMAT_UNKNOWN;
    switch(color)
    {
    case JC_UNKNOWN:
        colorFormat = MFX_JPEG_COLORFORMAT_UNKNOWN;
        break;
    case JC_GRAY:
        colorFormat = MFX_JPEG_COLORFORMAT_YCbCr;
        break;
    case JC_YCBCR:
        colorFormat = MFX_JPEG_COLORFORMAT_YCbCr;
        break;
    case JC_RGB:
        colorFormat = MFX_JPEG_COLORFORMAT_RGB;
        break;
    default:
        VM_ASSERT(false);
        break;
    };

    return colorFormat;
}

inline JCOLOR GetUMCColorType(Ipp16u chromaFormat, Ipp16u colorFormat)
{
    JCOLOR color = JC_UNKNOWN;

    switch(colorFormat)
    {
    case MFX_JPEG_COLORFORMAT_UNKNOWN:
        color = JC_UNKNOWN;
        break;
    case MFX_JPEG_COLORFORMAT_YCbCr:
        if(chromaFormat == MFX_CHROMAFORMAT_MONOCHROME)
        {
            color = JC_GRAY;
        }
        else
        {
            color = JC_YCBCR;
        }
        break;
    case MFX_JPEG_COLORFORMAT_RGB:
        color = JC_RGB;
        break;
    default:
        VM_ASSERT(false);
        break;
    };

    return color;
}

} // end namespace UMC

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif //__UMC_MJPEG_VIDEO_DECODER_MFX_DECODE_H
