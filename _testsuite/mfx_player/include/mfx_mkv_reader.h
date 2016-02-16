/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once
#include <stdlib.h>
#include "app_defs.h"
#include "mfx_ibitstream_reader.h"

#define MFX_CODEC_VP8  MFX_MAKEFOURCC('V','P','8',' ')
/* Supported MKV tags */
enum TagId {
    TAG_UNKNOWN = 0x00000000,
    TAG_EBML,
    TAG_EBMLVersion,
    TAG_EBMLReadVersion,
    TAG_EBMLMaxIDLength,
    TAG_EBMLMaxSizeLength,
    TAG_DocType,
    TAG_DocTypeVersion,
    TAG_DocTypeReadVersion,
    TAG_Segment,
    TAG_SeekHead,
    TAG_Seek,
    TAG_SeekID,
    TAG_SeekPosition,
    TAG_EBMLVoid,
    TAG_Info,
    TAG_TimecodeScale,
    TAG_MuxingApp,
    TAG_WritingApp,
    TAG_Duration,
    TAG_DateUTC,
    TAG_SegmentUID,
    TAG_Tracks,
    TAG_TrackEntry,
    TAG_TrackNumber,
    TAG_TrackUID,
    TAG_TrackType,
    TAG_FlagLacing,
    TAG_MinCache,
    TAG_CodecID,
    TAG_CodecName,
    TAG_DefaultDuration,
    TAG_Language,
    TAG_Video,
    TAG_PixelWidth,
    TAG_PixelHeight,
    TAG_DisplayWidth,
    TAG_DisplayHeight,
    TAG_CodecPrivate,
    TAG_Cluster,
    TAG_Timecode,
    TAG_SimpleBlock,
    TAG_Attachments,
    TAG_BlockGroup,
    TAG_Block,
    TAG_ReferenceBlock,
    TAG_FlagEnabled,
    TAG_FlagDefault,
    TAG_FlagForced,
    TAG_TrackTimecodeScale,
    TAG_MaxBlockAdditionID,
    TAG_CodecDecodeAll,
    TAG_Audio,
    TAG_BlockDuration,
    TAG_ContentEncodings,
    TAG_Name,
    TAG_Chapters,
    TAG_Cues,
    TAG_Position,
    TAG_EOF
};

/* MKV data has variable lenght data. Supporting enum */
enum DataType {
    DATATYPE_BYTE,   // single byte
    DATATYPE_BYTE3,  // 3 bytes data
    DATATYPE_BYTE8,  // 8 bytes data
    DATATYPE_SHORT,  // 2 bytes data
    DATATYPE_INT,    // 4 bytes data
    DATATYPE_STR,    // UTF-8 String
    DATATYPE_BIN     // raw binary data
};

/* Size masks. Position of the leading 1 in bytes describes how long the embeded data is */
#define BYTE1MASK 0x80  // 1 byte size
#define BYTE2MASK 0x40  // 2 bytes size
#define BYTE3MASK 0x20  // 3 bytes size
#define BYTE4MASK 0x10  // 4 bytes size
#define BYTE8MASK 0x01  // 8 bytes size

struct MKV_Element_t{
    /* Size of the tag maker */
    mfxU32    size;   
    /* ID */
    TagId     id;
    /* Tag's binary mark */
    mfxU8      tag[16];
    /* Printable name of the tag */
    const char      *name;
    /* Tag has size */
    bool      has_size;
    /* Tag has embeded data */
    bool      embeded_size;
    /* Type of the data embeded if any */
    DataType  data_type;
};

#define CHECK_TAG(tag, ideal)\
    if ( (tag) != (ideal)) { return MFX_ERR_NOT_FOUND; }


class MKVReader
    : public IBitstreamReader
{
public:
    MKVReader(sStreamInfo * pParams = NULL)
    {
        m_bInited  = false;
        m_bInfoSet = false;
        m_fSource  = 0;
        m_codec    = 0;        
        m_codec_private_size = 0;
        m_track_number       = 1;

        if (NULL != pParams)
        {
            m_bInfoSet = true;
            m_sInfo = *pParams;
        }
    }

    virtual ~MKVReader(){ Close(); }

    virtual void      Close();
    virtual mfxStatus Init(const vm_char *strFileName);
    virtual mfxStatus ReadNextFrame(mfxBitstream2 &bs);
    virtual mfxStatus GetStreamInfo(sStreamInfo * pParams)
    {
        if (m_bInfoSet)
        {
            if (NULL == pParams)
                return MFX_ERR_UNKNOWN;
            
            *pParams = m_sInfo;
            return MFX_ERR_NONE;
        } else {
            pParams->videoType = m_codec;
            pParams->nHeight = 0;
            pParams->nWidth  = 0;
        }

        return MFX_ERR_NONE;
    }
    virtual mfxStatus SeekTime(mfxF64 /*fSeekTo*/)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus SeekPercent(mfxF64 /*fSeekTo*/)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual mfxStatus SeekFrameOffset(mfxU32 /*nFrameOffset*/, mfxFrameInfo & /*in_info*/)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    virtual bool isFrameModeEnabled() {
        return false;
    }

protected:
    // MKV major parts readers 
    mfxStatus ReadEBMLHeader(void);
    mfxStatus ReadMetaSeek(void);
    mfxStatus ReadSegment(void);
    mfxStatus ReadTrack(void);
    mfxStatus ReadSimpleBlock(mfxBitstream2 &bs);
    mfxStatus ReadCluster(mfxBitstream2 &bs);

    // MKV helper functions
    TagId     GetTag(void);
    mfxU32    GetSize();
    void      ReadValue(mfxU32 size, DataType type, void *value);
    mfxStatus SaveCodecPrivate(mfxU8 *content, int block_size);

    bool             m_bInited;
    sStreamInfo      m_sInfo;
    bool             m_bInfoSet;
    CBitstreamReader m_CbsReader;
    mfxU32           m_codec;
    mfxU8           *m_codec_private;
    mfxU32           m_codec_private_size;
    mfxU8            m_track_number;
    vm_file*         m_fSource;
};
