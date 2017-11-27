/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2017 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_pipeline_defs.h"
#include "mfx_buffered_bs_reader.h"
#include "ippdefs.h"

BufferedBitstreamReader::BufferedBitstreamReader( IBitstreamReader *pReader
                                                , int nAllocationChunk
                                                , int nSpecificChunkSize
                                                , int nDataSizeForward
                                                , int nDataSizeForwardSpecificSize
                                                , const mfx_shared_ptr<IFile> & fileAcc)
: base(pReader)
, m_CurrFrame()
, m_CurrFrameOffset()
, m_CurChunk ()
, m_CurChunkOffset ()
, m_AllocationChunkAlgo(nAllocationChunk)
, m_AllocationChunkize(nAllocationChunk & SpecificChunkSize ? nSpecificChunkSize : DefaultChunkLenght)
, m_ForwardChunkAlgo(nDataSizeForward)
, m_ForwardChunkize(nDataSizeForward & SpecificChunkSize ? nDataSizeForwardSpecificSize : DefaultChunkLenght)
, m_pFileAccess(fileAcc)
{
}

BufferedBitstreamReader::~BufferedBitstreamReader()
{
    Close();
}

void BufferedBitstreamReader::Close()
{
    m_BufferedFile.clear();
    base::Close();
}

mfxStatus BufferedBitstreamReader::Init(const vm_char *strFileName)
{
    Ipp64u file_size = 0;
    MFX_CHECK_STS(m_pFileAccess->GetInfo(strFileName, file_size));

    if (file_size > GetProcessMemoryLimit())
    {
        PipelineTrace((VM_STRING("WARNING: Cannot Buffer Whole File, Performance Results could be INCORRECT\n")));
        m_bBuffering = false;
    }
    else
    {
        m_bBuffering = true;
    }

    MFX_CHECK_STS(base::Init(strFileName));

    if (!m_bBuffering)
        return PIPELINE_WRN_BUFFERING_UNAVAILABLE;

    PrintInfoNoCR(VM_STRING("Buffering"), VM_STRING("%.3f\r"), 0.0);

    //set size for forward ond store chunks only once
    if (m_AllocationChunkAlgo & MaxFileSize) {
        m_AllocationChunkize = (std::min)((mfxU32)file_size,  m_AllocationChunkize);
    }

    //Frame to be populated by splitter
    std::vector<mfxU8> advanceFrame;

    if (m_ForwardChunkAlgo & MaxFileSize) {
        advanceFrame.resize((std::min)((mfxU32)file_size,  m_ForwardChunkize));
    }
    else {
        advanceFrame.resize(m_ForwardChunkize);
    }
    //and coresponding bs
    mfxBitstream2 bs;
    bs.MaxLength = advanceFrame.size();
    bs.Data = &advanceFrame.front();

    mfxU64 nBuffered = 0;
    mfxU64 nBufferedLast = 0;
    mfxU64 nOriginal = file_size == 0 ? 1 : file_size;
    mfxStatus sts = MFX_ERR_NONE;
    
    //offset in currently written chunk 
    mfxU32 nStorageDataOffset = 0;
    //current pointer
    FileStorage::iterator i = m_BufferedFile.end();
    
    while ( MFX_ERR_NONE == sts)
    {
        //probing for 1450 - insufficient resources error, decreasing chunk
        Timeout<10> error1405;
        for ( ;!error1405; )
        {
            sts = base::ReadNextFrame(bs);
            if (sts == MFX_ERR_NONE)
                break;

            if (m_pFileAccess->GetLastError() != MFX_WRN_DEVICE_BUSY)
                break;
                
            if (bs.MaxLength > 1024*1024)
            {
                bs.MaxLength /= 2;
            }
            else
            {
                error1405.wait("Handling of ERROR_NO_SYSTEM_RESOURCES");
            }
        }

        MFX_CHECK_STS_SKIP(sts, MFX_ERR_UNKNOWN);
        if (MFX_ERR_UNKNOWN == sts) {
            break;
        }

        //file_size  -= bs.DataLength;//even in nonpacked data size decrements
        nBufferedLast += bs.DataLength;

        //1 percent increment
        if (((double)nBufferedLast / (double)nOriginal) > 0.01)
        {
            nBuffered += nBufferedLast;
            nBufferedLast = 0;
            PrintInfoNoCR(VM_STRING("Buffering"), VM_STRING("%.1f\r"), 100.0 * (double)nBuffered / (double)nOriginal);
        }

        moveToInternalBuf(bs, i, nStorageDataOffset);
    }

    m_CurChunk = m_BufferedFile.begin();

    PrintInfo(VM_STRING("Buffering"), VM_STRING("%.1f"), 100.0 * (double)(nBuffered +nBufferedLast)/ (double)nOriginal);

    return MFX_ERR_NONE;
}

void BufferedBitstreamReader::moveToInternalBuf(mfxBitstream2 & bs, FileStorage::iterator & currentChunk, mfxU32 &offset)
{
    if (!bs.DataLength)
        return ;

    //store frame description
    m_BufferedFrames.push_back(bs);
    
    //store frame data in linked list memory pool
    while(bs.DataLength) {
        //currently data chunk that is being written to
        if (currentChunk == m_BufferedFile.end() || offset == m_AllocationChunkize) 
        {
            m_BufferedFile.push_back(std::vector<mfxU8>());
            m_BufferedFile.back().resize(m_AllocationChunkize);
            currentChunk = m_BufferedFile.end();
            currentChunk --;
            offset = 0;
        }
        
        mfxU32 nCanCopy = (std::min)(m_AllocationChunkize - offset, bs.DataLength);
        memcpy(&currentChunk->front() + offset, bs.Data + bs.DataOffset, nCanCopy);
        bs.DataOffset += nCanCopy;
        bs.DataLength -= nCanCopy;
        offset += nCanCopy;
    }

    bs.DataOffset = 0;
}

int BufferedBitstreamReader::copyFromInternalBuf(mfxU8 *pTo, mfxU32 howMany)
{
    int nCopied = 0;
    
    while (0 != howMany && m_CurChunk != m_BufferedFile.end())
    {
        mfxU32 nLocalCopied = IPP_MIN(howMany, m_CurChunk->size() - m_CurChunkOffset);
        memcpy(pTo + nCopied, &m_CurChunk->front() + m_CurChunkOffset, nLocalCopied);
        
        m_CurChunkOffset += nLocalCopied;

        if (m_CurChunkOffset == m_CurChunk->size())
        {
            m_CurChunk++;
            m_CurChunkOffset = 0;
        }
        nCopied += nLocalCopied;
        howMany -= nLocalCopied;
    }

    return nCopied;
}

mfxStatus BufferedBitstreamReader::ReadNextFrame(mfxBitstream2 &bs)
{
    if (!m_bBuffering)
        return base::ReadNextFrame(bs);

    mfxU32 nBytesRead = 0;

    if (bs.DataOffset == bs.MaxLength)
    {
        bs.DataOffset = 0;
    }

    mfxU32 nCanWrite = bs.MaxLength - bs.DataLength - bs.DataOffset;

    if ( 0 == nCanWrite && bs.DataOffset )
    {
       nCanWrite = bs.DataOffset;
       memmove(bs.Data, bs.Data + bs.DataOffset, bs.DataLength);
       bs.DataOffset = 0;
    }

    MFX_CHECK_WITH_ERR(0 != nCanWrite || 0 != bs.DataLength,  MFX_ERR_UNKNOWN);

    if (nCanWrite != 0 )
    {
        //TODO: num bytes can be copied from last frame - should be less than buffer in most case lets write warning, until REFACTORED
        mfxU32 nShouldCopy = IPP_MIN(nCanWrite, m_CurrFrame < m_BufferedFrames.size() ? 
            m_BufferedFrames[m_CurrFrame].DataLength - m_CurrFrameOffset : 0);

        //if there already frame offset means we didnt read whole frame on previous call
        //dont trace warning if not in frame mode
        if (m_pTarget->isFrameModeEnabled() && 0 == m_CurrFrameOffset && nShouldCopy && nShouldCopy < m_BufferedFrames[m_CurrFrame].DataLength)
        {
            PipelineTraceForce((VM_STRING("WARNING: [BufferedBitstreamReader]: cannot copy whole frame, frame_size=%u, buffer_size=%u\n")
                , m_BufferedFrames[m_CurrFrame].DataLength
                , nCanWrite));
        }
        
        nBytesRead = (mfxU32)copyFromInternalBuf(bs.Data + bs.DataLength + bs.DataOffset, nShouldCopy);

        //end of stream - no trace required
        if (0 == nBytesRead)
            return MFX_ERR_UNKNOWN;
        
        bs.DataLength += nBytesRead;

        m_CurrFrameOffset += nBytesRead;

        //assigning bitstream properties
        bs.TimeStamp = m_BufferedFrames[m_CurrFrame].TimeStamp;
        bs.PicStruct = m_BufferedFrames[m_CurrFrame].PicStruct;
        bs.FrameType = m_BufferedFrames[m_CurrFrame].FrameType;
        bs.DecodeTimeStamp = m_BufferedFrames[m_CurrFrame].DecodeTimeStamp;

        if (m_CurrFrameOffset == m_BufferedFrames[m_CurrFrame].DataLength)
        {
            m_CurrFrameOffset = 0;
            m_CurrFrame++;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus BufferedBitstreamReader::SeekFrameOffset(mfxU32 nFrameOffset, mfxFrameInfo &in_info)
{
    if (!m_bBuffering)
        return base::SeekFrameOffset(nFrameOffset, in_info);

    size_t nPos = 0;
    size_t nTargetPos = (size_t)GetMinPlaneSize(in_info) * (size_t)nFrameOffset;
    
    m_CurChunkOffset = 0;

    //seek among file chunks
    for (m_CurChunk = m_BufferedFile.begin(); m_CurChunk !=  m_BufferedFile.end(); m_CurChunk++)
    {
        size_t nPosSum = nPos + m_CurChunk->size();
        if (nPosSum > nTargetPos)
            break;

        nPos = nPosSum;
    }
    
    MFX_CHECK_WITH_ERR(m_CurChunk !=  m_BufferedFile.end(), MFX_ERR_UNDEFINED_BEHAVIOR);
    m_CurChunkOffset = nTargetPos - nPos;

    //seek among frames chunks
    m_CurrFrame = 0;
    nPos = 0;
    for (; m_CurrFrame < m_BufferedFrames.size(); m_CurrFrame++)
    {
        size_t nPosSum = nPos + m_BufferedFrames[m_CurrFrame].DataLength;
        if (nPosSum > nTargetPos)
            break;

        nPos = nPosSum;
    }
    
    MFX_CHECK_WITH_ERR(m_CurrFrame < m_BufferedFrames.size(), MFX_ERR_UNDEFINED_BEHAVIOR);
    m_CurrFrameOffset = nTargetPos - nPos;

    return MFX_ERR_NONE;;
}

mfxStatus BufferedBitstreamReader::SeekTime(mfxF64 fSeekTo) {
    if (fSeekTo != 0.)
        return MFX_ERR_UNSUPPORTED;

    m_CurChunk = m_BufferedFile.begin();
    m_CurChunkOffset = 0;
    m_CurrFrame = 0;
    m_CurrFrameOffset = 0;
    return MFX_ERR_NONE;
}