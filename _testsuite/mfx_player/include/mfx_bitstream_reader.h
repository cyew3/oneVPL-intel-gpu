/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014-2017 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include "mfx_ibitstream_reader.h"
#include "umc_corruption_reader.h"
#include "umc_fio_reader.h"

#define MFX_TIME_STAMP_INVALID   (mfxU64)-1;
class BitstreamReader
    : public IBitstreamReader
{
public:
    BitstreamReader(sStreamInfo * pParams = NULL)
    {
       // m_fSource  = NULL;
        m_bInited  = false;
        m_bInfoSet = false;
        m_bCorrupted    = false;
        m_CorruptLevel  = UMC::CORRUPT_NONE;

        if (NULL != pParams)
        {
            m_bInfoSet = true;
            m_sInfo = *pParams;
            m_CorruptLevel = (UMC::CorruptionLevel)pParams->corrupted;
        }
        m_CorruptionReader.reset(new UMC::CorruptionReader());
    }

    virtual ~BitstreamReader()
    {
    }

    virtual void      Close()
    {
    }

    virtual mfxStatus Init(const vm_char *strFileName)
    {
        mfxStatus sts;
        UMC::FIOReader *pFileReader = new  UMC::FIOReader();

        UMC::CorruptionReaderParams  *pCorruptionParams = new UMC::CorruptionReaderParams();
        UMC::FileReaderParams        *pFileReaderParams = new UMC::FileReaderParams();
        vm_string_strcpy_s(pFileReaderParams->m_file_name, ((sizeof(pFileReaderParams->m_file_name)/sizeof(vm_char))-1), strFileName);
        pFileReaderParams->m_portion_size = 0;
        pCorruptionParams->CorruptMode    = m_CorruptLevel;
        pCorruptionParams->pActual        = (UMC::DataReader *)pFileReader;
        pCorruptionParams->pActualParams  = pFileReaderParams;
        
        sts = (mfxStatus)m_CorruptionReader->Init(pCorruptionParams);
        delete pCorruptionParams;
        if ( MFX_ERR_NONE != sts )
        {
             return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
        return sts;
    }

    virtual mfxStatus ReadNextFrame(mfxBitstream2 &bs)
    {
        mfxStatus sts;
        mfxU32 nBytesRead = 0;
        memmove(bs.Data, bs.Data + bs.DataOffset, bs.DataLength);
        bs.DataOffset = 0;
        bs.TimeStamp = MFX_TIME_STAMP_INVALID;
        nBytesRead   = bs.MaxLength - bs.DataLength;
        sts = (mfxStatus)m_CorruptionReader->ReadData(bs.Data + bs.DataLength, &nBytesRead);
        if (0 == nBytesRead)
        {
            if (bs.DataFlag & MFX_BITSTREAM_EOS)
                return MFX_ERR_UNKNOWN;
            bs.DataFlag |= MFX_BITSTREAM_EOS;
        }
        bs.DataLength += nBytesRead;
        return MFX_ERR_NONE;
    }

    virtual mfxStatus GetStreamInfo(sStreamInfo * pParams)
    {
        if (m_bInfoSet)
        {
            if (NULL == pParams)
                return MFX_ERR_UNKNOWN;
            
            *pParams = m_sInfo;
            return MFX_ERR_NONE;
        }
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus SeekTime(mfxF64 fSeekTo)

    {
        if (fSeekTo == 0.0)
        {
            return (mfxStatus)m_CorruptionReader->SetPosition(0);
        }
        fSeekTo = fSeekTo;
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus SeekPercent(mfxF64 fSeekTo)
    {
        fSeekTo = fSeekTo;
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus SeekFrameOffset(mfxU32 nFrameOffset, mfxFrameInfo &in_info)
    {
        mfxI64 nFileOffset;
        mfxFrameInfo info = {};
        if (m_bInfoSet)
        {
            info.Width  = m_sInfo.nWidth;
            info.Height = m_sInfo.nHeight;
            info.FourCC = m_sInfo.videoType;
            //not a monochrome at list
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        }
        else
        {
            info = in_info;
        }

        nFileOffset = (mfxI64)GetMinPlaneSize(info) * nFrameOffset;
        PipelineTrace((VM_STRING("INFO: reposition to frame=%d, using file offset=%I64d\n"), nFrameOffset, nFileOffset));

        return (mfxStatus)m_CorruptionReader->SetPosition((Ipp64f)nFileOffset);
    }

    virtual bool isFrameModeEnabled() 
    {
        return false;
    }


protected:
    bool             m_bInited;
    sStreamInfo      m_sInfo;
    bool             m_bInfoSet;
    bool             m_bCorrupted;
    std::auto_ptr<UMC::CorruptionReader>  m_CorruptionReader;
    UMC::CorruptionLevel                  m_CorruptLevel;

};
