/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2011 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once

#include "mfx_ibitstream_reader.h"

class BitstreamReader
    : public IBitstreamReader
{
public:
    BitstreamReader(sStreamInfo * pParams = NULL)
    {
       // m_fSource  = NULL;
        m_bInited  = false;
        m_bInfoSet = false;

        if (NULL != pParams)
        {
            m_bInfoSet = true;
            m_sInfo = *pParams;
        }
    }

    virtual ~BitstreamReader(){}

    virtual void      Close()
    {
        m_CbsReader.Close();
    }
    virtual mfxStatus Init(const vm_char *strFileName)
    {
        return m_CbsReader.Init(strFileName);
    }
    virtual mfxStatus ReadNextFrame(mfxBitstream2 &bs)
    {
        return m_CbsReader.ReadNextFrame(bs.isNull ? NULL : &bs);
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
            return m_CbsReader.SetFilePos(0, VM_FILE_SEEK_SET);
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

        return m_CbsReader.SetFilePos(nFileOffset, VM_FILE_SEEK_SET);
    }
    virtual bool isFrameModeEnabled() {
        return false;
    }


protected:
    //vm_file*         m_fSource;
    bool             m_bInited;
    sStreamInfo      m_sInfo;
    bool             m_bInfoSet;
    CBitstreamReader m_CbsReader;
};
