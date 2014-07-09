/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once
#include <stdlib.h>
#include "app_defs.h"
#include "mfx_pipeline_defs.h"
#include "mfx_ibitstream_reader.h"
#include "mfx_file_generic.h"


//---- Canon RMF specific definition --------------
#define RMF_HOB_SAMPLE_COUNT   4  // left vertical line
#define RMF_VOB_LINE_COUNT     2  // upper horizontal line
#define RMF_LEFT_SAMPLE_COUNT  8
#define RMF_RIGHT_SAMPLE_COUNT 8
#define RMF_UPPER_LINE_COUNT   8
#define RMF_LOWER_LINE_COUNT   8
#define RMF_IMAGE_DATA_OFFSET  65536
#define RMF_BIT_DEPTH          10

#define  RMF_FRAME_SURF_WIDTH    4096
#define  RMF_FRAME_SURF_HEIGHT   2160

#define MFX_TIME_STAMP_INVALID  (mfxU64)-1;
#define MSDK_ALIGN32(X) (((mfxU32)((X)+31)) & (~ (mfxU32)31))

class BayerVideoReader : public IBitstreamReader
{
public :

    BayerVideoReader(sStreamInfo *pParams);
    ~BayerVideoReader() {};

    virtual mfxStatus Init(const vm_char *strFileName);
    virtual mfxStatus ReadNextFrame(mfxBitstream2 &bs);
    virtual mfxStatus GetStreamInfo(sStreamInfo * /*pParams*/)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    virtual mfxStatus SeekTime(mfxF64 /*fSeekTo*/)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus SeekPercent(mfxF64 /*fSeekTo*/) 
    {
        return MFX_ERR_UNSUPPORTED;
    }
    //reposition to specific frames number offset in  input stream
    virtual mfxStatus SeekFrameOffset(mfxU32 nFrameOffset, mfxFrameInfo &/*in_info*/)
    {
        m_FileNum = nFrameOffset;
        return MFX_ERR_NONE;
    }
    //for splitters that returns 1
    virtual bool isFrameModeEnabled() 
    {
        return false;
    }

     void Close();
protected:


    bool     m_bInited;
    bool     m_bInfoSet;
    sStreamInfo      m_sInfo;
    mfxI32   m_FileFramePitchBytes;
    mfxI32   m_FileFrameHeight;
    mfxU32  *m_pRawData;
    mfxI32   m_RawWidthU32;
    vm_file  *m_fSrc;
    vm_char  m_FileNameBase[MAX_PATH];
    mfxU32   m_FileNum;
    mfxCamBayerFormat m_BayerType;
};
