/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2009-2019 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef MFX_BUFFERED_BITSTREAM_READER
#define MFX_BUFFERED_BITSTREAM_READER

//buffers all input file during init call
//proxy pattern
#include "mfx_ibitstream_reader.h"
#include "mfx_file_generic.h"


class BufferedBitstreamReader
    : public InterfaceProxy<IBitstreamReader>
{
    enum {
        //ReadFile cannot read blocks large than this
        DefaultChunkLenght = 4 * 1024 * 1024
    };
    typedef InterfaceProxy<IBitstreamReader> base;

public :

    enum {
        DefaultChunkSize = 0,
        SpecificChunkSize = 1,
        MaxFileSize = 2,
    };

    BufferedBitstreamReader(IBitstreamReader *pReader
        //chunks of that size will store file data
        , int nAllocationChunk = DefaultChunkSize | MaxFileSize
        , int nSpecificChunkSize = 0
        // how much to reserve buffer for single frame to call downstream::ReadNextFrame()
        // NOTE: for frame splitters it is vital to guarantee that whole frame can be read at once
        // since there is no mechanism that splitter tell that output buffer is too small
        // TODO: refactor whole splitter design
        , int nDataSizeForward = DefaultChunkSize | MaxFileSize
        , int nDataSizeForwardSpecificSize = 0

        //TODO: consider move file access to base class
        , const mfx_shared_ptr<IFile> &pFileAccess = mfx_make_shared<GenericFile>());

    virtual ~BufferedBitstreamReader();

    virtual void      Close();
    virtual mfxStatus Init(const vm_char *strFileName);
    virtual mfxStatus ReadNextFrame(mfxBitstream2 &pBS);
    //reposition to specific frames number offset in  input stream
    virtual mfxStatus SeekFrameOffset(mfxU32 nFrameOffset, mfxFrameInfo &in_info);
    virtual mfxStatus SeekTime(mfxF64 fSeekTo);

protected:

    //index of current frame
    mfxU32                     m_CurrFrame;
    //read offset inside frame
    mfxU32                     m_CurrFrameOffset;
    std::vector<mfxBitstream2> m_BufferedFrames;


    //store file in big chunks
    typedef std::list<std::vector<mfxU8> > FileStorage;
    FileStorage               m_BufferedFile;
    // chunk idx
    FileStorage::iterator     m_CurChunk;
    // data offset in current chunk
    mfxU32                    m_CurChunkOffset;

    bool                      m_bBuffering;
    mfxU32                    m_AllocationChunkAlgo;
    mfxU32                    m_AllocationChunkize;
    mfxU32                    m_ForwardChunkAlgo;
    mfxU32                    m_ForwardChunkize;

    //for quering file information
    mfx_shared_ptr<IFile>     m_pFileAccess;

    virtual int copyFromInternalBuf(mfxU8 *pTo, mfxU32 howMany);
    virtual void moveToInternalBuf(mfxBitstream2 & bs, FileStorage::iterator & currentChunk, mfxU32 &offset);
};

#endif // MFX_BUFFERED_BITSTREAM_READER
