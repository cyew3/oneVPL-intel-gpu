//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_MJPEG_VIDEO_DECODER_H
#define __UMC_MJPEG_VIDEO_DECODER_H

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_ENCODER)

#include <vector>
#include <memory>

#include "ippdefs.h"
#include "ippcore.h"
#include "ipps.h"
#include "ippi.h"
#include "ippcc.h"
#include "ippj.h"
#include "umc_structures.h"
#include "umc_video_encoder.h"
#include "umc_video_data.h"
#include "umc_mutex.h"

#include "mfxvideo++int.h"

// internal JPEG decoder object forward declaration
class CBaseStreamOutput;
class CJPEGEncoder;

namespace UMC
{

enum
{
    JPEG_ENC_MAX_THREADS_HW = 1,
    JPEG_ENC_MAX_THREADS = 4
};

enum
{
    JPEG_ENC_DEFAULT_NUM_PIECES = JPEG_ENC_MAX_THREADS
};


class MJPEGEncoderScan
{
public:
    MJPEGEncoderScan(): m_numPieces(0) {}
    ~MJPEGEncoderScan() {Close();}

    Status Init(Ipp32u numPieces);
    Status Reset(Ipp32u numPieces);
    void   Close();

    Ipp32u GetNumPieces();

    // Number of restart intervals in scan
    Ipp32u m_numPieces;
    // Array of pieces locations (number of BitstreamBuffer, contained the piece)
    std::vector<size_t> m_pieceLocation;
    // Array of pieces offsets
    std::vector<size_t> m_pieceOffset;
    // Array of pieces sizes
    std::vector<size_t> m_pieceSize;
};


class MJPEGEncoderPicture
{
public:
    MJPEGEncoderPicture();
   ~MJPEGEncoderPicture();

    Ipp32u GetNumPieces();
   
    std::auto_ptr<VideoData>       m_sourceData;
    std::vector<MJPEGEncoderScan*> m_scans;

    Ipp32u                         m_release_source_data;
};


class MJPEGEncoderFrame
{
public:
    MJPEGEncoderFrame() {Reset();}
   ~MJPEGEncoderFrame() {Reset();}

    void Reset();

    Ipp32u GetNumPics();
    Ipp32u GetNumPieces();
    Status GetPiecePosition(Ipp32u pieceNum, Ipp32u* fieldNum, Ipp32u* scanNum, Ipp32u* piecePosInField, Ipp32u* piecePosInScan);

    std::vector<MJPEGEncoderPicture*> m_pics;
};


class MJPEGEncoderParams : public VideoEncoderParams
{
    DYNAMIC_CAST_DECL(MJPEGEncoderParams, VideoEncoderParams)

public:
    MJPEGEncoderParams()
    {
        quality          = 75;  // jpeg frame quality [1,100]
        restart_interval = 0;
        huffman_opt      = 0;
        point_transform  = 0;
        predictor        = 1;
        app0_units       = 0;
        app0_xdensity    = 1;
        app0_ydensity    = 1;
        threading_mode   = 0;
        buf_size         = 0;
        interleaved      = 0;
        chroma_format    = 0;
    }

//    Status ReadParamFile(const vm_char *ParFileName) { return UMC_ERR_NOT_IMPLEMENTED; }

    Ipp32s quality;
    Ipp32s huffman_opt;
    Ipp32s restart_interval;
    Ipp32s point_transform;
    Ipp32s predictor;
    Ipp32s app0_units;
    Ipp32s app0_xdensity;
    Ipp32s app0_ydensity;
    Ipp32s threading_mode;
    Ipp32s buf_size;
    Ipp32s interleaved;
    Ipp32s chroma_format;
};


class MJPEGVideoEncoder : public VideoEncoder
{
    DYNAMIC_CAST_DECL(MJPEGVideoEncoder, VideoEncoder)

public:
    MJPEGVideoEncoder(void);
    virtual ~MJPEGVideoEncoder(void);

    // Initialize for subsequent frame decoding.
    virtual Status Init(BaseCodecParams* init);

    // Reset decoder to initial state
    virtual Status Reset(void);

    // Close decoding & free all allocated resources
    virtual Status Close(void);

    // Get next frame
    virtual Status GetFrame(MediaData* in, MediaData* out);

    // 
    virtual Status EncodePiece(const mfxU32 threadNumber, const Ipp32u numPiece);

    // Get codec working (initialization) parameter(s)
    virtual Status GetInfo(BaseCodecParams *info);

    // Do post processing
    virtual Status PostProcessing(MediaData* out);

    // Get the number of encoders allocated
    Ipp32u NumEncodersAllocated(void);

    //
    Ipp32u NumPicsCollected(void);

    //
    Ipp32u NumPiecesCollected(void);

    //
    Status AddPicture(MJPEGEncoderPicture* pic);

    Status FillQuantTableExtBuf(mfxExtJPEGQuantTables* quantTables);
    Status FillHuffmanTableExtBuf(mfxExtJPEGHuffmanTables* huffmanTables);

    Status SetQuantTableExtBuf(mfxExtJPEGQuantTables* quantTables);
    Status SetHuffmanTableExtBuf(mfxExtJPEGHuffmanTables* huffmanTables);

    Status SetDefaultQuantTable(const mfxU16 quality);
    Status SetDefaultHuffmanTable();

    bool   IsQuantTableInited();
    bool   IsHuffmanTableInited();

protected:

    // JPEG encoders allocated
    std::auto_ptr<CJPEGEncoder>       m_enc[JPEG_ENC_MAX_THREADS];
    // Bitstream buffer for each thread
    std::auto_ptr<MediaData>          m_pBitstreamBuffer[JPEG_ENC_MAX_THREADS];
    //
    std::auto_ptr<MJPEGEncoderFrame>  m_frame;
    // Number of the encoders allocated
    Ipp32u                            m_numEnc;

    UMC::Mutex                        m_guard;

    bool                              m_IsInit;

    MJPEGEncoderParams m_EncoderParams;
};

} // end namespace UMC

#endif // UMC_ENABLE_MJPEG_VIDEO_ENCODER
#endif //__UMC_MJPEG_VIDEO_ENCODER_H
