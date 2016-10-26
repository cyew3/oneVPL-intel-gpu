//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2012 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_MJPEG_TASK_H_
#define _MFX_MJPEG_TASK_H_

#include "mfx_common.h"
#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)

#include <vector>
#include <memory>

class CJpegTaskBuffer
{
public:
    // Default construct
    CJpegTaskBuffer(void);
    // Destructor
    ~CJpegTaskBuffer(void);

    // Allocate the buffer
    mfxStatus Allocate(const size_t size);

    // Pointer to the buffer
    mfxU8* pBuf;
    // Buffer size
    size_t bufSize;
    // Data size
    size_t dataSize;

    // Size of header data
    size_t imageHeaderSize;
    // Array of pieces offsets
    std::vector<size_t> pieceOffset;
    // Array of pieces sizes
    std::vector<size_t> pieceSize;
    // Count of RST markers prior current piece
    std::vector<size_t> pieceRSTOffset;

    // Array of SOS segments (offsets)
    std::vector<size_t> scanOffset;
    // Array of SOS segments (sizes)
    std::vector<size_t> scanSize;
    // Array of tables before SOS (offsets)
    std::vector<size_t> scanTablesOffset;
    // Array of tables before SOS (sizes)
    std::vector<size_t> scanTablesSize;

    // Picture's time stamp
    Ipp64f timeStamp;

    // Number of scans in the image
    mfxU32 numScans;

    // Number of independent pieces in the image
    mfxU32 numPieces;

    // The field position in interlaced case
    mfxU32 fieldPos;

protected:
    // Close the object
    void Close(void);
};

// Forward declaration of used classes
namespace UMC
{
class FrameData;
class MJPEGVideoDecoderMFX;
class MediaDataEx;
class VideoDecoderParams;
class FrameAllocator;

} // namespace UMC

class CJpegTask
{
public:
    // Default constructor
    CJpegTask(void);
    // Destructor
    ~CJpegTask(void);

    // Initialize the task object
    mfxStatus Initialize(UMC::VideoDecoderParams &params,
                         UMC::FrameAllocator *pFrameAllocator,
                         mfxU16 rotation,
                         mfxU16 chromaFormat,
                         mfxU16 colorFormat);

    // Reset the task, drop all counters
    void Reset(void);

    // Add a picture to the task
    mfxStatus AddPicture(UMC::MediaDataEx *pSrcData, const mfxU32  fieldPos);

    // Get the number of pictures collected
    inline
    mfxU32 NumPicCollected(void) const;

    // Get the number of pieces collected
    inline
    mfxU32 NumPiecesCollected(void) const;

    // Get the picture's buffer
    inline
    const CJpegTaskBuffer &GetPictureBuffer(mfxU32 picNum) const
    {
        return *(m_pics[picNum]);
    }

    // tasks parameters
    UMC::FrameData *dst;
    mfxFrameSurface1 *surface_work;
    mfxFrameSurface1 *surface_out;

    // Decoder's array
    std::auto_ptr<UMC::MJPEGVideoDecoderMFX> m_pMJPEGVideoDecoder;

protected:
    // Close the object, release all resources
    void Close(void);

    // Make sure that the buffer is big enough to collect the source
    mfxStatus CheckBufferSize(const size_t srcSize);

    // Number of pictures collected
    mfxU32 m_numPic;
    // Array with collected pictures
    std::vector<CJpegTaskBuffer *> m_pics;

    // Number of scan data pieces. This number is used to request the number of
    // threads.
    mfxU32 m_numPieces;

};

//
// Inline members
//

inline
mfxU32 CJpegTask::NumPicCollected(void) const
{
    return m_numPic;

} // mfxU32 CJpegTask::NumPicCollected(void) const

inline
mfxU32 CJpegTask::NumPiecesCollected(void) const
{
    return m_numPieces;

} // mfxU32 CJpegTask::NumPiecesCollected(void) const

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER
#endif // _MFX_MJPEG_TASK_H_
