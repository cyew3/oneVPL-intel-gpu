//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_ENCODER)

#include <string.h>
#include "vm_debug.h"
#include "umc_video_data.h"
#include "umc_mjpeg_video_encoder.h"
#include <umc_automatic_mutex.h>
#include "membuffout.h"
#include "jpegenc.h"

// VD: max num of padding bytes at the end of AVI frame
//     should be 1 by definition (padding up to Ipp16s boundary),
//     but in reality can be bigger
#define AVI_MAX_PADDING_BYTES 4


namespace UMC {

Status MJPEGEncoderScan::Init(Ipp32u numPieces)
{
    if(m_numPieces != numPieces)
    {
        m_numPieces = numPieces;
        m_pieceLocation.resize(m_numPieces);
        m_pieceOffset.resize(m_numPieces);
        m_pieceSize.resize(m_numPieces);
    }
    return UMC_OK;
}

Status MJPEGEncoderScan::Reset(Ipp32u numPieces)
{
    return Init(numPieces);
}

void MJPEGEncoderScan::Close()
{
    m_numPieces = 0;
    m_pieceLocation.clear();
    m_pieceOffset.clear();
    m_pieceSize.clear();
}

Ipp32u MJPEGEncoderScan::GetNumPieces()
{
    return m_numPieces;
}


MJPEGEncoderPicture::MJPEGEncoderPicture()
{
    m_sourceData.reset(new VideoData());
    m_release_source_data = false;
}

MJPEGEncoderPicture::~MJPEGEncoderPicture()
{
    if(m_release_source_data)
    {
        m_sourceData.get()->ReleaseImage();
    }

    for(Ipp32u i=0; i<m_scans.size(); i++)
    {
        delete m_scans[i];
    }

    m_scans.clear();
}

Ipp32u MJPEGEncoderPicture::GetNumPieces()
{
    Ipp32u numPieces = 0;

    for(Ipp32u i=0; i<m_scans.size(); i++)
        numPieces += m_scans[i]->GetNumPieces();

    return numPieces;
}


void MJPEGEncoderFrame::Reset()
{
    for(Ipp32u i=0; i<m_pics.size(); i++)
    {
        delete m_pics[i];
    }

    m_pics.clear();
}

Ipp32u MJPEGEncoderFrame::GetNumPics()
{
    return (Ipp32u)m_pics.size();
}

Ipp32u MJPEGEncoderFrame::GetNumPieces()
{
    Ipp32u numPieces = 0;

    for(Ipp32u i=0; i<GetNumPics(); i++)
        numPieces += m_pics[i]->GetNumPieces();

    return numPieces;
}

Status MJPEGEncoderFrame::GetPiecePosition(Ipp32u pieceNum, Ipp32u* fieldNum, Ipp32u* scanNum, Ipp32u* piecePosInField, Ipp32u* piecePosInScan)
{
    Ipp32u prevNumPieces = 0;
    for(Ipp32u i=0; i<GetNumPics(); i++)
    {
        if(prevNumPieces <= pieceNum && pieceNum < prevNumPieces + m_pics[i]->GetNumPieces())
        {
            *fieldNum = i;
            *piecePosInField = pieceNum - prevNumPieces;
            break;
        }
        else
        {
            prevNumPieces += m_pics[i]->GetNumPieces();
        }
    }

    prevNumPieces = 0;

    for(Ipp32u i=0; i<m_pics[*fieldNum]->m_scans.size(); i++)
    {
        if(prevNumPieces <= *piecePosInField && *piecePosInField < prevNumPieces + m_pics[*fieldNum]->m_scans[i]->GetNumPieces())
        {
            *scanNum = i;
            *piecePosInScan = *piecePosInField - prevNumPieces;
            break;
        }
        else
        {
            prevNumPieces += m_pics[*fieldNum]->m_scans[i]->GetNumPieces();
        }
    }

    return UMC_OK;
}


VideoEncoder *CreateMJPEGEncoder() { return new MJPEGVideoEncoder(); }


MJPEGVideoEncoder::MJPEGVideoEncoder(void)
{
    m_IsInit      = false;
    m_numEnc      = 0;
}

MJPEGVideoEncoder::~MJPEGVideoEncoder(void)
{
    Close();
}

Status MJPEGVideoEncoder::Init(BaseCodecParams* lpInit)
{
    MJPEGEncoderParams* pEncoderParams = (MJPEGEncoderParams*) lpInit;
    Status status = UMC_OK;
    JERRCODE jerr = JPEG_OK;
    Ipp32u i, numThreads;

    if(!pEncoderParams)
        return UMC_ERR_NULL_PTR;

    for (i = 0; i < m_numEnc; i += 1)
    {
        //m_enc[i].reset(0);
        m_pBitstreamBuffer[i].get()->Reset();
    }

    status = VideoEncoder::Close();
    if(UMC_OK != status)
        return UMC_ERR_INIT;

    status = VideoEncoder::Init(lpInit);
    if(UMC_OK != status)
        return UMC_ERR_INIT;

    m_EncoderParams  = *pEncoderParams;
    
    // allocate the JPEG encoders
    numThreads = JPEG_ENC_MAX_THREADS;
    if ((m_EncoderParams.numThreads) &&
        (numThreads > (Ipp32u) m_EncoderParams.numThreads))
    {
        numThreads = m_EncoderParams.numThreads;
    }

    for (i = 0; i < numThreads; i += 1)
    {
        m_enc[i].reset(new CJPEGEncoder());
        if (NULL == m_enc[i].get())
        {
            return UMC_ERR_ALLOC;
        }

        if(m_EncoderParams.quality)
        {
            jerr = m_enc[i]->SetDefaultQuantTable((Ipp16u)m_EncoderParams.quality);
            if(JPEG_OK != jerr)
                return UMC_ERR_FAILED;
        }
        
        jerr = m_enc[i]->SetDefaultACTable(); 
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;

        jerr = m_enc[i]->SetDefaultDCTable();
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;

        if(!m_pBitstreamBuffer[i].get())
            m_pBitstreamBuffer[i].reset(new MediaData(m_EncoderParams.buf_size));
    }
    m_numEnc = numThreads;

    if(m_frame.get())
        m_frame.get()->Reset();

    m_frame.reset(new MJPEGEncoderFrame);

    m_IsInit      = true;

    return UMC_OK;
}

Status MJPEGVideoEncoder::Reset(void)
{
    return Init(&m_EncoderParams);
}

Status MJPEGVideoEncoder::Close(void)
{
    for (Ipp32u i = 0; i < m_numEnc; i += 1)
    {
        m_enc[i].reset();
        m_pBitstreamBuffer[i].reset();
    }

    if(m_frame.get())
        m_frame.get()->Reset();

    m_IsInit       = false;

    return VideoEncoder::Close();
}

Status MJPEGVideoEncoder::FillQuantTableExtBuf(mfxExtJPEGQuantTables* quantTables)
{
    JERRCODE jerr = JPEG_OK;

    if (!m_IsInit)
        return UMC_ERR_NOT_INITIALIZED;

    quantTables->NumTable = m_enc[0]->GetNumQuantTables();
    for(int i=0; i<quantTables->NumTable; i++)
    {
        jerr = m_enc[0]->FillQuantTable(i, quantTables->Qm[i]);
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;
    }

    return UMC_OK;
}

Status MJPEGVideoEncoder::FillHuffmanTableExtBuf(mfxExtJPEGHuffmanTables* huffmanTables)
{
    JERRCODE jerr = JPEG_OK;

    if (!m_IsInit)
        return UMC_ERR_NOT_INITIALIZED;

    huffmanTables->NumACTable = m_enc[0]->GetNumACTables();
    for(int i=0; i<huffmanTables->NumACTable; i++)
    {
        jerr = m_enc[0]->FillACTable(i, huffmanTables->ACTables[i].Bits, huffmanTables->ACTables[i].Values);
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;
    }

    huffmanTables->NumDCTable = m_enc[0]->GetNumDCTables();
    for(int i=0; i<huffmanTables->NumDCTable; i++)
    {
        jerr = m_enc[0]->FillDCTable(i, huffmanTables->DCTables[i].Bits, huffmanTables->DCTables[i].Values);
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;
    }

    return UMC_OK;
}

Status MJPEGVideoEncoder::SetDefaultQuantTable(const mfxU16 quality)
{
    JERRCODE jerr = JPEG_OK;

    if (!m_IsInit)
        return UMC_ERR_NOT_INITIALIZED;

    for(Ipp32u i=0; i<m_numEnc; i++)
    {
        jerr = m_enc[i]->SetDefaultQuantTable(quality);
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;
    }

    return UMC_OK;
}

Status MJPEGVideoEncoder::SetDefaultHuffmanTable()
{
    JERRCODE jerr = JPEG_OK;

    if (!m_IsInit)
        return UMC_ERR_NOT_INITIALIZED;

    for(Ipp32u i=0; i<m_numEnc; i++)
    {
        jerr = m_enc[i]->SetDefaultACTable(); 
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;

        jerr = m_enc[i]->SetDefaultDCTable();
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;
    }

    return UMC_OK;
}

Status MJPEGVideoEncoder::SetQuantTableExtBuf(mfxExtJPEGQuantTables* quantTables)
{
    JERRCODE jerr = JPEG_OK;

    if (!m_IsInit)
        return UMC_ERR_NOT_INITIALIZED;

    for(Ipp32u i=0; i<m_numEnc; i++)
    {
        for(Ipp16u j=0; j<quantTables->NumTable; j++)
        {
            jerr = m_enc[i]->SetQuantTable(j, quantTables->Qm[j]);
            if(JPEG_OK != jerr)
                return UMC_ERR_FAILED;
        }
    }

    return UMC_OK;
}

Status MJPEGVideoEncoder::SetHuffmanTableExtBuf(mfxExtJPEGHuffmanTables* huffmanTables)
{
    JERRCODE jerr = JPEG_OK;

    if (!m_IsInit)
        return UMC_ERR_NOT_INITIALIZED;

    for(Ipp32u i=0; i<m_numEnc; i++)
    {     
        for(int j=0; j<huffmanTables->NumACTable; j++)
        {
            jerr = m_enc[i]->SetACTable(j, huffmanTables->ACTables[j].Bits, huffmanTables->ACTables[j].Values);
            if(JPEG_OK != jerr)
                return UMC_ERR_FAILED;
        }

        for(int j=0; j<huffmanTables->NumDCTable; j++)
        {
            jerr = m_enc[i]->SetDCTable(j, huffmanTables->DCTables[j].Bits, huffmanTables->DCTables[j].Values);
            if(JPEG_OK != jerr)
                return UMC_ERR_FAILED;
        }
    }

    return UMC_OK;
}

bool MJPEGVideoEncoder::IsQuantTableInited()
{
    return m_enc[0]->IsQuantTableInited();
}

bool MJPEGVideoEncoder::IsHuffmanTableInited()
{
    return m_enc[0]->IsACTableInited() && m_enc[0]->IsDCTableInited();
}

Status MJPEGVideoEncoder::GetInfo(BaseCodecParams *info)
{
    if (!m_IsInit)
        return UMC_ERR_NOT_INITIALIZED;

    *(MJPEGEncoderParams*)info = m_EncoderParams;

    return UMC_OK;
}

Ipp32u MJPEGVideoEncoder::NumEncodersAllocated(void)
{
    return m_numEnc;
}

Ipp32u MJPEGVideoEncoder::NumPicsCollected(void)
{
    AutomaticUMCMutex guard(m_guard);

    return m_frame.get()->GetNumPics();
}

Ipp32u MJPEGVideoEncoder::NumPiecesCollected(void)
{
    return m_frame.get()->GetNumPieces();
}

Status MJPEGVideoEncoder::AddPicture(MJPEGEncoderPicture* pic)
{
    if (!m_IsInit)
        return UMC_ERR_NOT_INITIALIZED;

    AutomaticUMCMutex guard(m_guard);

    m_frame.get()->m_pics.push_back(pic);
    
    return UMC_OK;
}

Status MJPEGVideoEncoder::GetFrame(MediaData* in, MediaData* out)
{
    in;
    out;    
    JERRCODE   status   = JPEG_OK;
#if 0
    VideoData *pDataIn = DynamicCast<VideoData, MediaData>(in);
    IppiSize   srcSize;
    JCOLOR     jcolor   = JC_NV12;
    JTMODE     jtmode   = JT_OLD;
    JMODE      jmode    = JPEG_BASELINE;
    JSS        jss      = JS_444;
    Ipp32s     channels = 3;
    Ipp32s     depth    = 8;
    bool       planar   = false;
    CMemBuffOutput streamOut;
    VideoData      cvt;

    if(!in)
        return UMC_OK;

    srcSize.width  = pDataIn->GetWidth();
    srcSize.height = pDataIn->GetHeight();

    if(pDataIn->GetColorFormat() == NV12 || pDataIn->GetColorFormat() == YV12)
    {
        cvt.Init(pDataIn->GetWidth(), pDataIn->GetHeight(), UMC::YUV420);
        cvt.Alloc();

        VideoProcessing proc;
        proc.GetFrame(pDataIn, &cvt);
        pDataIn = &cvt;
    }

    jmode = (JMODE)m_EncoderParams.profile;
    depth = pDataIn->GetPlaneBitDepth(0);

    switch(pDataIn->GetColorFormat())
    {
    case YV12:
        channels = 3;
        jcolor   = JC_YCBCR;
        jss      = JS_411;
        planar   = true;
        break;
    case NV12:
        channels = 2;
        jcolor   = JC_NV12;
        jss      = JS_411;
        planar   = true;
        break;
    case YUV420:
        channels = 3;
        jcolor   = JC_YCBCR;
        jss      = JS_411;
        planar   = true;
        break;
    case YUV422:
        channels = 3;
        jcolor   = JC_YCBCR;
        jss      = JS_422V;
        planar   = true;
        break;
    case YUV444:
        channels = 3;
        jcolor   = JC_YCBCR;
        jss      = JS_444;
        planar   = true;
        break;
    case RGB24:
        channels = 3;
        jcolor   = JC_RGB;
        break;
    case RGB32:
        channels = 4;
        jcolor   = JC_RGBA;
        break;
    case GRAY:
        channels = 1;
        jcolor   = JC_GRAY;
        break;
    default:
        return UMC_ERR_UNSUPPORTED;
    }

    status = streamOut.Open((Ipp8u*)out->GetBufferPointer(), (int)out->GetBufferSize());
    if(JPEG_OK != status)
        return UMC_ERR_FAILED;

    status = m_enc[0]->SetDestination(&streamOut);
    if(JPEG_OK != status)
        return UMC_ERR_FAILED;

    if(depth == 8)
    {
        if(planar)
        {
            Ipp8u  *pSrc[]  = {(Ipp8u*)pDataIn->GetPlanePointer(0), (Ipp8u*)pDataIn->GetPlanePointer(1), (Ipp8u*)pDataIn->GetPlanePointer(2), (Ipp8u*)pDataIn->GetPlanePointer(3)};
            Ipp32s  iStep[] = {(Ipp32s)pDataIn->GetPlanePitch(0), (Ipp32s)pDataIn->GetPlanePitch(1), (Ipp32s)pDataIn->GetPlanePitch(2), (Ipp32s)pDataIn->GetPlanePitch(3)};

            status = m_enc[0]->SetSource(pSrc, iStep, srcSize, channels, jcolor, jss, 8);
            if(JPEG_OK != status)
                return UMC_ERR_FAILED;
        }
        else
        {
            Ipp8u  *pSrc  = (Ipp8u*)pDataIn->GetPlanePointer(0);
            Ipp32s  iStep = (Ipp32s)pDataIn->GetPlanePitch(0);

            status = m_enc[0]->SetSource(pSrc, iStep, srcSize, channels, jcolor, jss, 8);
            if(JPEG_OK != status)
                return UMC_ERR_FAILED;
        }
    }
    else if(depth == 16)
    {
        if(planar)
        {
            Ipp16s *pSrc[]  = {(Ipp16s*)pDataIn->GetPlanePointer(0), (Ipp16s*)pDataIn->GetPlanePointer(1), (Ipp16s*)pDataIn->GetPlanePointer(2), (Ipp16s*)pDataIn->GetPlanePointer(3)};
            Ipp32s  iStep[] = {(Ipp32s)pDataIn->GetPlanePitch(0), (Ipp32s)pDataIn->GetPlanePitch(1), (Ipp32s)pDataIn->GetPlanePitch(2), (Ipp32s)pDataIn->GetPlanePitch(3)};

            status = m_enc[0]->SetSource(pSrc, iStep, srcSize, channels, jcolor, jss, 16);
            if(JPEG_OK != status)
                return UMC_ERR_FAILED;
        }
        else
        {
            Ipp16s *pSrc  = (Ipp16s*)pDataIn->GetPlanePointer(0);
            Ipp32s  iStep = (Ipp32s)pDataIn->GetPlanePitch(0);

            status = m_enc[0]->SetSource(pSrc, iStep, srcSize, channels, jcolor, jss, 16);
            if(JPEG_OK != status)
                return UMC_ERR_FAILED;
        }
    }
    else
        return UMC_ERR_UNSUPPORTED;

    if(jmode == JPEG_LOSSLESS)
    {
        status = m_enc[0]->SetParams(JPEG_LOSSLESS,
                                     jcolor,
                                     jss,
                                     m_EncoderParams.restart_interval,
                                     m_EncoderParams.huffman_opt,
                                     m_EncoderParams.point_transform,
                                     m_EncoderParams.predictor);
        if(JPEG_OK != status)
            return UMC_ERR_FAILED;
    }
    else
    {
        status = m_enc[0]->SetParams(jmode,
                                     jcolor,
                                     jss,
                                     m_EncoderParams.restart_interval,
                                     0,
                                     1,
                                     0,
                                     1,
                                     0,
                                     m_EncoderParams.huffman_opt,
                                     m_EncoderParams.quality,
                                     jtmode);
        if(JPEG_OK != status)
            return UMC_ERR_FAILED;
    }

    status = m_enc[0]->SetJFIFApp0Resolution(JRU_NONE, 1, 1);
    if(JPEG_OK != status)
        return UMC_ERR_FAILED;

    status = m_enc[0]->WriteHeader();
    if(JPEG_OK != status)
        return UMC_ERR_FAILED;

    status = m_enc[0]->WriteData();
    if(JPEG_OK != status)
        return UMC_ERR_FAILED;

    out->SetTime(pDataIn->GetTime());
    out->SetDataSize(streamOut.GetPosition());
#endif
    return status;
}

Status MJPEGVideoEncoder::EncodePiece(const mfxU32 threadNumber, const Ipp32u numPiece)
{
    IppiSize       srcSize;
    JCOLOR         jsrcColor = JC_NV12;
    JCOLOR         jdstColor = JC_NV12;
    JTMODE         jtmode   = JT_OLD;
    JMODE          jmode    = JPEG_BASELINE;
    JSS            jss      = JS_444;
    Ipp32s         srcChannels = 3;
    Ipp32s         depth    = 8;
    bool           planar   = false;
    JERRCODE       status   = JPEG_OK;
    CMemBuffOutput streamOut;
    Ipp32u         numField, numScan, piecePosInField, piecePosInScan;

    if (!m_IsInit)
        return UMC_ERR_NOT_INITIALIZED;

    m_frame.get()->GetPiecePosition(numPiece, &numField, &numScan, &piecePosInField, &piecePosInScan);

    VideoData *in = m_frame.get()->m_pics[numField]->m_sourceData.get();
    VideoData *pDataIn = DynamicCast<VideoData, MediaData>(in);

    if(!in)
        return UMC_OK;

    if(!pDataIn)
        return UMC_OK;

    srcSize.width  = pDataIn->GetWidth();
    srcSize.height = pDataIn->GetHeight();

    jmode = (JMODE)m_EncoderParams.profile;
    depth = pDataIn->GetPlaneBitDepth(0);

    switch(m_EncoderParams.chroma_format)
    {
    case MFX_CHROMAFORMAT_YUV444:
        // now RGB32 only is supported
        srcChannels = 4;
        jsrcColor   = JC_BGRA;        
        jdstColor   = JC_RGB;
        jss         = JS_444;  
        break;
    case MFX_CHROMAFORMAT_YUV422H:
        srcChannels = 3;
        jsrcColor   = JC_YCBCR;
        jdstColor   = JC_YCBCR;
        jss         = JS_422H;
        planar      = true;
        break;
    case MFX_CHROMAFORMAT_YUV422V:
        srcChannels = 3;
        jsrcColor   = JC_YCBCR;
        jdstColor   = JC_YCBCR;
        jss         = JS_422V;
        planar      = true;
        break;
    case MFX_CHROMAFORMAT_YUV420:
        srcChannels = 3;
        jsrcColor   = JC_YCBCR;
        jdstColor   = JC_YCBCR;
        jss         = JS_420;
        planar      = true;
        break;
    case MFX_CHROMAFORMAT_YUV400:
        srcChannels = 1;
        jsrcColor   = JC_GRAY;
        jdstColor   = JC_GRAY;
        break;
    default:
        return UMC_ERR_UNSUPPORTED;
    }

    status = streamOut.Open((Ipp8u*)m_pBitstreamBuffer[threadNumber].get()->GetBufferPointer() + m_pBitstreamBuffer[threadNumber].get()->GetDataSize(), 
                            (int)m_pBitstreamBuffer[threadNumber].get()->GetBufferSize() - (int)m_pBitstreamBuffer[threadNumber].get()->GetDataSize());
    if(JPEG_OK != status)
        return UMC_ERR_FAILED;

    status = m_enc[threadNumber]->SetDestination(&streamOut);
    if(JPEG_OK != status)
        return UMC_ERR_FAILED;

    if(depth == 8)
    {
        if(planar)
        {
            Ipp8u  *pSrc[]  = {(Ipp8u*)pDataIn->GetPlanePointer(0), (Ipp8u*)pDataIn->GetPlanePointer(1), (Ipp8u*)pDataIn->GetPlanePointer(2), (Ipp8u*)pDataIn->GetPlanePointer(3)};
            Ipp32s  iStep[] = {(Ipp32s)pDataIn->GetPlanePitch(0), (Ipp32s)pDataIn->GetPlanePitch(1), (Ipp32s)pDataIn->GetPlanePitch(2), (Ipp32s)pDataIn->GetPlanePitch(3)};

            status = m_enc[threadNumber]->SetSource(pSrc, iStep, srcSize, srcChannels, jsrcColor, jss, 8);
            if(JPEG_OK != status)
                return UMC_ERR_FAILED;
        }
        else
        {
            Ipp8u  *pSrc  = (Ipp8u*)pDataIn->GetPlanePointer(0);
            Ipp32s  iStep = (Ipp32s)pDataIn->GetPlanePitch(0);

            status = m_enc[threadNumber]->SetSource(pSrc, iStep, srcSize, srcChannels, jsrcColor, jss, 8);
            if(JPEG_OK != status)
                return UMC_ERR_FAILED;
        }
    }
    else if(depth == 16)
    {
        if(planar)
        {
            Ipp16s *pSrc[]  = {(Ipp16s*)pDataIn->GetPlanePointer(0), (Ipp16s*)pDataIn->GetPlanePointer(1), (Ipp16s*)pDataIn->GetPlanePointer(2), (Ipp16s*)pDataIn->GetPlanePointer(3)};
            Ipp32s  iStep[] = {(Ipp32s)pDataIn->GetPlanePitch(0), (Ipp32s)pDataIn->GetPlanePitch(1), (Ipp32s)pDataIn->GetPlanePitch(2), (Ipp32s)pDataIn->GetPlanePitch(3)};

            status = m_enc[threadNumber]->SetSource(pSrc, iStep, srcSize, srcChannels, jsrcColor, jss, 16);
            if(JPEG_OK != status)
                return UMC_ERR_FAILED;
        }
        else
        {
            Ipp16s *pSrc  = (Ipp16s*)pDataIn->GetPlanePointer(0);
            Ipp32s  iStep = (Ipp32s)pDataIn->GetPlanePitch(0);

            status = m_enc[threadNumber]->SetSource(pSrc, iStep, srcSize, srcChannels, jsrcColor, jss, 16);
            if(JPEG_OK != status)
                return UMC_ERR_FAILED;
        }
    }
    else
        return UMC_ERR_UNSUPPORTED;

    if(jmode == JPEG_LOSSLESS)
    {
        status = m_enc[threadNumber]->SetParams(JPEG_LOSSLESS,
                                                jdstColor,
                                                jss,
                                                m_EncoderParams.restart_interval,
                                                m_EncoderParams.huffman_opt,
                                                m_EncoderParams.point_transform,
                                                m_EncoderParams.predictor);
        if(JPEG_OK != status)
            return UMC_ERR_FAILED;
    }
    else
    {
        status = m_enc[threadNumber]->SetParams(jmode,
                                                jdstColor,
                                                jss,
                                                m_EncoderParams.restart_interval,
                                                m_EncoderParams.interleaved,
                                                m_frame.get()->m_pics[numField]->GetNumPieces(),
                                                piecePosInField,
                                                numScan,
                                                piecePosInScan,
                                                m_EncoderParams.huffman_opt,
                                                m_EncoderParams.quality,
                                                jtmode);
        if(JPEG_OK != status)
            return UMC_ERR_FAILED;
    }

    status = m_enc[threadNumber]->SetJFIFApp0Resolution(JRU_NONE, 1, 1);
    if(JPEG_OK != status)
        return UMC_ERR_FAILED;

    status = m_enc[threadNumber]->WriteHeader();
    if(JPEG_OK != status)
        return UMC_ERR_FAILED;

    status = m_enc[threadNumber]->WriteData();
    if(JPEG_ERR_DHT_DATA == status)
        return UMC_ERR_INVALID_PARAMS;
    else if(JPEG_OK != status)
        return UMC_ERR_FAILED;

    m_frame.get()->m_pics[numField]->m_scans[numScan]->m_pieceLocation[piecePosInScan] = threadNumber; //m_pOutputBuffer[numPic].get()->m_pieceLocation[numPiece] = threadNumber;
    m_frame.get()->m_pics[numField]->m_scans[numScan]->m_pieceOffset[piecePosInScan] = m_pBitstreamBuffer[threadNumber].get()->GetDataSize(); //m_pOutputBuffer[numPic].get()->m_pieceOffset[numPiece] = m_pBitstreamBuffer[threadNumber].get()->GetDataSize();
    m_frame.get()->m_pics[numField]->m_scans[numScan]->m_pieceSize[piecePosInScan] = streamOut.GetPosition(); //m_pOutputBuffer[numPic].get()->m_pieceSize[numPiece] = streamOut.GetPosition();
    
    //out->SetTime(pDataIn->GetTime());
    
    m_pBitstreamBuffer[threadNumber]->SetDataSize(m_pBitstreamBuffer[threadNumber].get()->GetDataSize() + streamOut.GetPosition());

    return status;
}

Status MJPEGVideoEncoder::PostProcessing(MediaData* out)
{
    if (!m_IsInit)
        return UMC_ERR_NOT_INITIALIZED;

    Status status = UMC_OK;
    Ipp32u i, j, k;
    size_t requiredDataSize = 0;

    // check buffer size
    for(i=0; i<m_frame.get()->GetNumPics(); i++)
        for(j=0; j<m_frame.get()->m_pics[i]->m_scans.size(); j++)
            for(k=0; k<m_frame.get()->m_pics[i]->m_scans[j]->GetNumPieces(); k++)
                requiredDataSize += m_frame.get()->m_pics[i]->m_scans[j]->m_pieceSize[k];

    if(out->GetBufferSize() - out->GetDataSize() < requiredDataSize)
    {
        return UMC_ERR_NOT_ENOUGH_BUFFER;
    }

    for(i=0; i<m_frame.get()->GetNumPics(); i++)
    {
        for(j=0; j<m_frame.get()->m_pics[i]->m_scans.size(); j++)
        {
            for(k=0; k<m_frame.get()->m_pics[i]->m_scans[j]->GetNumPieces(); k++)
            {
                size_t location = m_frame.get()->m_pics[i]->m_scans[j]->m_pieceLocation[k];
                size_t offset = m_frame.get()->m_pics[i]->m_scans[j]->m_pieceOffset[k];
                size_t size = m_frame.get()->m_pics[i]->m_scans[j]->m_pieceSize[k];

                MFX_INTERNAL_CPY((Ipp8u *)out->GetBufferPointer() + out->GetDataSize(),
                            (Ipp8u *)m_pBitstreamBuffer[location]->GetBufferPointer() + offset,
                            (Ipp32u)size);

                if(k != m_frame.get()->m_pics[i]->m_scans[j]->GetNumPieces() - 1)
                {
                    Ipp8u *link = (Ipp8u *)out->GetBufferPointer() + out->GetDataSize() + size;
                    link[0] = 0xFF;
                    link[1] = (Ipp8u)(0xD0 + (k % 8));

                    out->SetDataSize(out->GetDataSize() + size + 2);
                }
                else
                {
                    out->SetDataSize(out->GetDataSize() + size);
                }
            }
        }
    }

    return status;
}

} // end namespace UMC

#endif // UMC_ENABLE_MJPEG_VIDEO_ENCODER
