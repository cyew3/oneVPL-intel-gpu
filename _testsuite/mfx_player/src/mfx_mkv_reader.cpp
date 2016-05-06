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

#include "mfx_mkv_reader.h"

mfxStatus MKVReader::Init(const vm_char *strFileName){
    mfxStatus sts;
    CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);
    CHECK_ERROR(vm_string_strlen(strFileName), 0, MFX_ERR_UNKNOWN);

    Close();

    //open file to read input stream
    m_fSource = vm_file_fopen(strFileName, VM_STRING("rb"));
    CHECK_POINTER(m_fSource, MFX_ERR_NULL_PTR);

    sts = ReadEBMLHeader();

    sts = ReadMetaSeek();
    
    sts = ReadSegment();

    sts = ReadTrack();

    m_bInited = true;
    return MFX_ERR_NONE;
}

void MKVReader::Close(){
    if (m_fSource)
    {
        vm_file_close(m_fSource);
        m_fSource = NULL;
    }

    m_codec   = 0;
    m_bInited = false;
}

#define MFX_TIME_STAMP_INVALID  (mfxU64)-1; 

mfxStatus MKVReader::ReadNextFrame(mfxBitstream2 &bs)
{
    CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts;

    memcpy(bs.Data, bs.Data + bs.DataOffset, bs.DataLength);
    bs.DataOffset = 0;
    bs.TimeStamp = MFX_TIME_STAMP_INVALID;
    
    if( m_codec_private_size > 0 ){
        memcpy(bs.Data + bs.DataLength, m_codec_private, m_codec_private_size);
        bs.DataLength += m_codec_private_size;
        m_codec_private_size = 0;
        if ( m_codec_private) {
            delete m_codec_private;
        }
    }

    sts = ReadCluster(bs);

    if (sts != MFX_ERR_NONE)
    {
        if (bs.DataFlag & MFX_BITSTREAM_EOS)
            return MFX_ERR_UNKNOWN;
        bs.DataFlag |= MFX_BITSTREAM_EOS;
    }

    return MFX_ERR_NONE;
}

/* MKV Parser helpers */
static struct MKV_Element_t elements[] = {
   {4, TAG_EBML,              {0x1A,0x45,0xDF,0xA3}, "TAG_EBML", true, false, DATATYPE_BYTE},
   {2, TAG_EBMLVersion,       {0x42,0x86}          , "TAG_EBMLVersion", true, false, DATATYPE_BYTE},
   {2, TAG_EBMLReadVersion,   {0x42,0xF7}          , "TAG_EBMLReadVersion", true, false , DATATYPE_BYTE},
   {2, TAG_EBMLMaxIDLength,   {0x42,0xF2}          , "TAG_EBMLMaxIDLength", true, false, DATATYPE_BYTE},
   {2, TAG_EBMLMaxSizeLength, {0x42,0xF3}          , "TAG_EBMLMaxSizeLength", true, false, DATATYPE_BYTE},
   {2, TAG_DocType,           {0x42,0x82}          , "TAG_DocType", true, false, DATATYPE_BYTE},
   {2, TAG_DocTypeVersion,    {0x42,0x87}          , "TAG_DocTypeVersion", true, false, DATATYPE_BYTE},
   {2, TAG_DocTypeReadVersion,{0x42,0x85}          , "TAG_DocTypeReadVersion", true, false, DATATYPE_BYTE},
   {4, TAG_Segment,           {0x18,0x53,0x80,0x67}, "TAG_Segment", true, false, DATATYPE_BYTE},
   {4, TAG_SeekHead,          {0x11,0x4d,0x9b,0x74}, "TAG_SeekHead", true, false, DATATYPE_BYTE},
   {2, TAG_Seek,              {0x4d,0xbb}          , "TAG_Seek", true, false, DATATYPE_BYTE},
   {2, TAG_SeekID,            {0x53,0xab}          , "TAG_SeekID", true, false, DATATYPE_BYTE},
   {2, TAG_SeekPosition,      {0x53,0xac}          , "TAG_SeekPosition", true, false, DATATYPE_BYTE},
   {1, TAG_EBMLVoid,          {0xec}               , "TAG_EBMLVoid", true, false, DATATYPE_BYTE},
   {4, TAG_Info,              {0x15,0x49,0xA9,0x66}, "TAG_Info", true, false, DATATYPE_BYTE},
   {3, TAG_TimecodeScale,     {0x2a,0xd7,0xb1},      "TAG_TimecodeScale", true, false, DATATYPE_BYTE},
   {2, TAG_MuxingApp,         {0x4d,0x80},           "TAG_MuxingApp", true, false, DATATYPE_BYTE},
   {2, TAG_WritingApp,        {0x57,0x41},           "TAG_WritingApp", true, false, DATATYPE_BYTE},
   {2, TAG_Duration,          {0x44,0x89},           "TAG_Duration", true, false, DATATYPE_BYTE},
   {2, TAG_DateUTC,           {0x44,0x61},           "TAG_DateUTC", true, false, DATATYPE_BYTE},
   {2, TAG_SegmentUID,        {0x73,0xa4},           "TAG_SegmentUID", true, false, DATATYPE_BYTE},
   {4, TAG_Tracks,            {0x16,0x54,0xae,0x6b}, "TAG_Tracks", true, false, DATATYPE_BYTE},
   {1, TAG_TrackEntry,        {0xae},                "TAG_TrackEntry", true, false, DATATYPE_BYTE},
   {1, TAG_TrackNumber,       {0xd7},                "TAG_TrackNumber", true, true, DATATYPE_BYTE},
   {2, TAG_TrackUID,          {0x73,0xc5},           "TAG_TrackUID", true, true, DATATYPE_BYTE},
   {1, TAG_TrackType,         {0x83},                "TAG_TrackType", true, true, DATATYPE_BYTE},
   {1, TAG_FlagEnabled,       {0xb9},                "TAG_FlagEnabled", true, true, DATATYPE_BYTE},
   {1, TAG_FlagDefault,       {0x88},                "TAG_FlagDefault", true, true, DATATYPE_BYTE},
   {2, TAG_FlagForced,        {0x55, 0xaa},          "TAG_FlagForced", true, true, DATATYPE_BYTE},
   {1, TAG_FlagLacing,        {0x9c},                "TAG_FlagLAcing", true, true, DATATYPE_BYTE},
   {2, TAG_MinCache,          {0x6d,0xe7},           "TAG_MinCache", true, true, DATATYPE_BYTE},
   {1, TAG_Audio,             {0xe1},                "TAG_Audio", true, false, DATATYPE_BYTE},
   {1, TAG_CodecDecodeAll,    {0xaa},                "TAG_CodecDecodeAll", true, true, DATATYPE_BYTE},
   {2, TAG_MaxBlockAdditionID,{0x55,0xee},           "TAG_MaxBlockAdditionID", true, true, DATATYPE_INT},
   {3, TAG_TrackTimecodeScale,{0x23,0x31,0x4f},      "TAG_TrackTimecodeScale", true, true, DATATYPE_INT},
   {1, TAG_CodecID,           {0x86},                "TAG_CodecID", true, true, DATATYPE_BYTE},
   {3, TAG_CodecName,         {0x25,0x86,0x88},      "TAG_CodecName", true, true, DATATYPE_BYTE},
   {3, TAG_DefaultDuration,   {0x23,0xe3,0x83},      "TAG_DefaultDuration", true, true, DATATYPE_BYTE},
   {3, TAG_Language,          {0x22,0xb5,0x9c},      "TAG_Language", true, true, DATATYPE_BYTE},
   {1, TAG_Video,             {0xe0},                "TAG_Video", true, false, DATATYPE_BYTE},
   {1, TAG_PixelWidth,        {0xb0},                "TAG_PixelWidth", true, false, DATATYPE_BYTE},
   {1, TAG_PixelHeight,       {0xba},                "TAG_PixelHeight", true, false, DATATYPE_BYTE},
   {2, TAG_DisplayWidth,      {0x54,0xb0},           "TAG_DisplayWidth", true, false, DATATYPE_BYTE},
   {2, TAG_DisplayHeight,     {0x54,0xba},           "TAG_DisplayHeight", true, false, DATATYPE_BYTE},
   {2, TAG_CodecPrivate,      {0x63,0xa2},           "TAG_CodecPrivate", true, true, DATATYPE_BYTE},
   {2, TAG_ContentEncodings,  {0x6d,0x80},           "TAG_ContentEncodings", true, false, DATATYPE_BYTE},
   {4, TAG_Cluster,           {0x1f,0x43,0xb6,0x75}, "TAG_Cluster", true, false, DATATYPE_BYTE},
   {1, TAG_Timecode,          {0xe7},                "TAG_Timecode", true, false, DATATYPE_BYTE},
   {1, TAG_SimpleBlock,       {0xa3},                "TAG_SimpleBlock", true, false, DATATYPE_BYTE},
   {4, TAG_Attachments,       {0x19,0x41,0xa4,0x69}, "TAG_Attachments", true, false, DATATYPE_BYTE},
   {4, TAG_Chapters,          {0x10,0x43,0xa7,0x70}, "TAG_Chapters", true, false, DATATYPE_BYTE},
   {1, TAG_BlockGroup,        {0xa0},                "TAG_BlockGroup" , true, false, DATATYPE_BYTE},
   {1, TAG_Block,             {0xa1},                "TAG_Block"      , true, false, DATATYPE_BYTE},
   {1, TAG_ReferenceBlock,    {0xfb},                "TAG_ReferenceBlock"      , true, false, DATATYPE_BYTE},
   {1, TAG_BlockDuration,     {0x9b},                "TAG_BlockDuration"      , true, true, DATATYPE_BYTE},
   {2, TAG_Name,              {0x53,0x6e},           "TAG_Name"         , true, true, DATATYPE_BYTE},
   {4, TAG_Cues,              {0x1C,0x53,0xbb,0x6b}, "TAG_Cues"         , true, true, DATATYPE_BYTE},
   {1, TAG_Position,          {0xA7},                "TAG_Position"     , true, false, DATATYPE_BYTE},
   {0, TAG_UNKNOWN,           {0x00},                "TAG_UNKNOWN"      , false, false, DATATYPE_BYTE},
};

bool TagHasSize(TagId id){
    for(int i = 0;;i++){
        if(elements[i].id == TAG_UNKNOWN){
            return false;
        }

        if ( id == elements[i].id){
            return elements[i].has_size;
        }
    }
}

bool TagHasEmbedData(TagId id){
    for(int i = 0;;i++){
        if(elements[i].id == TAG_UNKNOWN){
            return false;
        }
        if ( id == elements[i].id){
            return elements[i].embeded_size;
        }
    }
}

DataType TagGetDataType(TagId id){
    for(int i = 0;;i++){
        if(elements[i].id == TAG_UNKNOWN){
            return DATATYPE_BYTE;
        }
        if ( id == elements[i].id){
            return elements[i].data_type;
        }
    }
}

int TagsMatched(mfxU8 *buffer, int size){
    if ( ! buffer ) {
        return -1;
    }

    int matched_tags = 0;
    bool match = false;
    for(int i =0;; i++){
        if(elements[i].id == TAG_UNKNOWN){
            return matched_tags;
        }
        match = true;
        if ( elements[i].size < static_cast<mfxU32>(size)){
            continue;
        }
        for(int j = 0; j < size; j++){
            if(elements[i].tag[j] != buffer[j]){
                match = false;
                break;
            }
        }

        if ( match ){
            matched_tags++;
        }
    }
}

const char *TagToString(TagId id) {
    for (int i = 0;; i++){
        if(elements[i].id == TAG_UNKNOWN){
            return "unknown";
        }
        if ( id == elements[i].id )
            return elements[i].name;
    }

    return "unknown";
 }

void PrintValue(DataType type, void *value, int size){
    switch(type){
        case DATATYPE_BYTE:
            fprintf(stderr, " Value(byte): %d\n", *((mfxU8 *)value));
            break;
        case DATATYPE_INT:
            fprintf(stderr, " Value(int):  %d\n", *((int *)value));
            break;
        case DATATYPE_BYTE3:
            fprintf(stderr, " Value(byte3):  %d\n", *((int *)value));
            break;
        case DATATYPE_BYTE8:
            fprintf(stderr, " Value(byte8):  %ld\n", *((long *)value));
            break;
        case DATATYPE_SHORT:
            fprintf(stderr, " Value(short): %d\n", *((short *)value));
            break;
        case DATATYPE_STR:
            fprintf(stderr, " Value(str): %s\n", (char *)value);
            break;
        case DATATYPE_BIN:
            fprintf(stderr, " Value(bin): ");
            size_t i = 0;
            while(size--){
                fprintf(stderr, "0x%x ", ((mfxU8*)value)[i]);
                i++;
            }
            fprintf(stderr, "\n");
            break;

    }
}

TagId FindTagId(mfxU8 *buffer, int size){
    size_t i = 0;
    bool matched = false;
    for (i = 0;; i++){
        if(elements[i].id == TAG_UNKNOWN){
            return TAG_UNKNOWN;
        }
        if (static_cast<mfxU32>(size) == elements[i].size ){
            matched = true;
            for(int j = 0; j < size; j++){
                if (buffer[j] != elements[i].tag[j]){
                    matched = false;
                    break;
                }
            }
            if ( matched )
                return elements[i].id;
        }
    }
}

mfxU32 MKVReader::GetSize(){
  
    mfxU8  buffer[8];
    mfxU32 result = 0;
    int    rewind = 0;
    Ipp32s read;
    
    read = vm_file_fread(buffer, 1, 8, m_fSource);
    if ( 8 != read ) {
        return 0;
    }

    if ( buffer[0] == 0xFF){
        result = 0;
        rewind = -7;
    } else if ( BYTE1MASK & buffer[0] ){
        result = (mfxU32)(buffer[0] ^ BYTE1MASK);
        rewind = -7;
    }else if ( BYTE2MASK & buffer[0] ){
        result = ((buffer[0] ^ BYTE2MASK) << 8 | buffer[1] );
        rewind = -6;
    } else if ( BYTE3MASK & buffer[0] ){
        result = ((buffer[0] ^ BYTE3MASK) << 16 | buffer[1] << 8 |  buffer[2] );
        rewind = -5;
    } else if ( BYTE4MASK & buffer[0] ){
        result = ((buffer[0] ^ BYTE4MASK) << 24 | buffer[1] << 16 |  buffer[2] << 8 | buffer[3] );
        rewind = -4;
    } else if ( BYTE8MASK & buffer[0] ){
        result = (unsigned long)(static_cast<uint64_t>(buffer[0] ^ BYTE8MASK) << 56 |
                static_cast<uint64_t>(buffer[1]) << 48 |
                static_cast<uint64_t>(buffer[2]) << 40|
                static_cast<uint64_t>(buffer[3]) << 32 | buffer[4] << 24 | buffer[5] << 16 | buffer[6] << 8 | buffer[7]);
    }

    vm_file_fseek(m_fSource, rewind, VM_FILE_SEEK_CUR);
    
    return result;
}

void MKVReader::ReadValue(mfxU32 size, DataType type, void *value){
    mfxU8 *buffer;

    buffer = (mfxU8 *)malloc(size);
    switch(type){
        case DATATYPE_BYTE:
            if (!vm_file_fread(buffer, 1, 1, m_fSource))
                break;
            *((mfxU8 *)value) = buffer[0];
            break;
        case DATATYPE_SHORT:
            if (!vm_file_fread(buffer, 1, 2, m_fSource))
                break;
            *((short *)value) = ( buffer[0] << 8 | buffer[1]);
            break;
        case DATATYPE_INT:
            if (!vm_file_fread(buffer, 1, 4, m_fSource))
                break;
            *((int *)value) = ( buffer[0] << 24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3]);
            break;
        case DATATYPE_BYTE3:
            if (!vm_file_fread(buffer, 1, 3, m_fSource))
                break;
            *((int *)value) = ( buffer[0] << 16 | buffer[1] << 8 | buffer[2] );
            break;
        case DATATYPE_BYTE8:
            if (!vm_file_fread(buffer, 1, size, m_fSource))
                break;
            *((unsigned long *)value) = (static_cast<uint64_t>(buffer[0]) << 56 |
                    static_cast<uint64_t>(buffer[1]) << 48 |
                    static_cast<uint64_t>(buffer[2]) << 40 |
                    static_cast<uint64_t>(buffer[3]) << 32 | buffer[4] << 24 | buffer[5] << 16 | buffer[6] << 8 | buffer[7]);
            break;
        case DATATYPE_STR:
            if (!vm_file_fread(buffer, 1, size, m_fSource))
                break;
            memcpy((char*)value, (char *)buffer, size);
            ((char*)value)[size] = '\0';
            break;
        case DATATYPE_BIN:
            if (!vm_file_fread(buffer, 1, size, m_fSource))
                break;
            memcpy((mfxU8*)value, (mfxU8 *)buffer, size);
            break;
    }
    free(buffer);
    return;
}

void PrintSize(int size){
    fprintf(stderr, " Size: %d\n", size);
}

/* Save codec's private info. Only h264 supported so far */
mfxStatus MKVReader::SaveCodecPrivate(mfxU8 *content, int block_size){
    mfxU8 sets;
    short set_len;
    mfxU32 shift = 0;
    mfxU8 prefix[4] = { 0x00, 0x00, 0x00, 0x01 };
    switch(m_codec){
        case MFX_CODEC_AVC:
            m_codec_private = (mfxU8 *)malloc(block_size);
            /*
            Size (bits) Contents Format MPEG2Video
                8            Reserved
                8            Profile
                8            Reserved 
                8            Level
                6            Reserved
                2            Size of NALU length minus 1
            */
            content += 5;
        
            /* Number of Sequence Parameter Sets*/
            sets = content[0]; content++;
            sets &= 0x1F;
            while(sets){
                set_len = ((content[0]) << 8 | content[1] ); content+=2;
                
                memcpy_s(&m_codec_private[shift], 4, prefix, 4); shift+=4;
                memcpy_s(&m_codec_private[shift], set_len, content,set_len); 
                content+=set_len;
                shift  +=set_len;
                sets--;
            }
            
            /* Number of Sequence Parameter Sets*/
            sets = content[0]; content++;
            sets &= 0x1F;
            while(sets){
                set_len = ((content[0]) << 8 | content[1] ); content+=2;
                memcpy_s(&m_codec_private[shift], 4, prefix, 4); shift+=4;
                memcpy_s(&m_codec_private[shift], set_len, content,set_len); 
                content+=set_len;
                shift  +=set_len;
                sets--;
            }
            m_codec_private_size = shift + 1;
        break;

        default:
            return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}

TagId MKVReader::GetTag(){
    mfxU8 buffer[16];
    memset(buffer, 0, 16);
    mfxU32 size = 0;
    Ipp32s read;

    for(;;){
        read = vm_file_fread(&buffer[size], 1, 1, m_fSource);
        if ( 1 != read ){
            return TAG_EOF;
        }

        int matched_tags =  TagsMatched(buffer, size+1);
        if ( matched_tags == 0 && size != 0 ){
            vm_file_fseek(m_fSource, -1, VM_FILE_SEEK_CUR);
            return FindTagId(buffer, size);
        }
        size++;
        if ( matched_tags == 0){
            return TAG_UNKNOWN;
        }
    }

    return TAG_UNKNOWN;
}

/* EBML header is not parsed, just skipped */
mfxStatus MKVReader::ReadEBMLHeader(void){
    TagId tag = GetTag();
    CHECK_TAG(tag, TAG_EBML);
    mfxU32 size = GetSize();
    return 0 == vm_file_fseek(m_fSource, size, VM_FILE_SEEK_CUR) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN;
}

/* Metaseek part is not parsed, just skipped */
mfxStatus MKVReader::ReadMetaSeek(void)
{
    TagId tag;
    mfxU32 size;

    tag = GetTag();
    CHECK_TAG(tag, TAG_Segment);
    size = GetSize();
    
    if ( size == 0 ){
        // Sometimes Metaseek part could be null
        return MFX_ERR_NONE;
    }

    tag = GetTag();
    CHECK_TAG(tag, TAG_SeekHead);
    size = GetSize();

    // Skip all subentries
    if (0 != vm_file_fseek(m_fSource, size, VM_FILE_SEEK_CUR) ){
        return MFX_ERR_UNSUPPORTED;
    }
    
    // TAG_EBMLVoid
    tag = GetTag();
    if ( TAG_Info == tag )
    {
        // This is for WebM where Info section goes right after MetaSeek
        return 0 == vm_file_fseek(m_fSource, -4, VM_FILE_SEEK_CUR) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN ;
    }
    CHECK_TAG(tag, TAG_EBMLVoid);
    size = GetSize();

    return 0 == vm_file_fseek(m_fSource, size, VM_FILE_SEEK_CUR) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN ;
}

/* SEgment part is not parsed, just skipped */
mfxStatus MKVReader::ReadSegment(void)
{
    TagId tag;
    mfxU32 size;

    tag = GetTag();
    CHECK_TAG(tag, TAG_Info);
    size = GetSize();

    return 0 == vm_file_fseek(m_fSource, size, VM_FILE_SEEK_CUR) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN ;
}

/* Parse track part */
mfxStatus MKVReader::ReadTrack(void){
    int    value_int;
    short  value_short;
    char   value_str[1024];
    mfxU8   value_byte = 0;
    Ipp32s size;
    mfxU8  *codec_private = 0;
    TagId  tag;
    Ipp64u track_size;
    mfxU32 codec = 0;
    mfxU8  track_number = 0;

    // TAG_Tracks
    tag = GetTag();
    CHECK_TAG(tag, TAG_Tracks);
    track_size = GetSize();
    track_size += vm_file_ftell(m_fSource);

    while(vm_file_ftell(m_fSource) < track_size){
        tag = GetTag();

        if ( TagHasSize(tag) ){
            size = GetSize();

            if ( tag == TAG_TrackEntry ){
                // New track detected. Drop coded and related data
                codec        = 0;
                track_number = 0;
                if( codec_private ) {
                    delete[] codec_private;
                    codec_private = 0;
                }
            }

            if ( tag == TAG_Video ||  tag == TAG_Audio ||  tag == TAG_ContentEncodings){
                // Skip embeded data
                vm_file_fseek(m_fSource, size, VM_FILE_SEEK_CUR);
                continue;
            } else if (tag == TAG_CodecID) {
                ReadValue(size, DATATYPE_STR, value_str);
                if ( strncmp(value_str, "V_MPEG4/ISO/AVC", size) == 0 ){
                    m_codec = codec = MFX_CODEC_AVC;
                } else  if ( strncmp(value_str, "V_MPEGH/ISO/HEVC", size) == 0 ){
                    m_codec = codec = MFX_CODEC_HEVC;
                } else  if ( strncmp(value_str, "V_VP8", size) == 0 ){
                    m_codec = codec = MFX_CODEC_VP8;
                } else  if ( strncmp(value_str, "V_VP9", size) == 0 ){
                    m_codec = codec = MFX_CODEC_VP9;
                } else {
                    // Unknown codec
                    codec = 1;
                }
            } else if ( tag == TAG_CodecPrivate ){
                codec_private = new mfxU8[size];
                if (!vm_file_fread(codec_private, 1, size, m_fSource))
                {
                    return MFX_ERR_UNKNOWN;
                }
            } else if ( TagHasEmbedData(tag) ){
                switch(size){
                    case 1:
                        ReadValue(size, DATATYPE_BYTE, &value_byte);
                        break;
                    case 2:
                        ReadValue(size, DATATYPE_SHORT, &value_short);
                        break;
                    case 3:
                        ReadValue(size, DATATYPE_BYTE3, &value_int);
                        break;
                    case 4:
                        ReadValue(size, DATATYPE_INT,   &value_int);
                        break;
                    default: 
                        ReadValue(size, DATATYPE_STR,  value_str);
                }

                if ( tag == TAG_TrackNumber ){
                    track_number = value_byte;
                }
            }

            if( track_number && codec > 1){
                m_track_number = track_number;
            }

            if ( codec > 1 && codec_private){
                SaveCodecPrivate(codec_private, size);
                codec = 0;
                delete[] codec_private;
                codec_private = 0;
            }
        } // if ( TagHasSize(tag) )
    } // while

    if ( codec_private ){
        delete[] codec_private;
    }

    // Jump to the end of tracks part
    vm_file_fseek(m_fSource, track_size, VM_FILE_SEEK_SET);
    tag = GetTag();
    if ( TAG_EBMLVoid == tag || TAG_Cues == tag) {
        size = GetSize();
    } else if ( TAG_Cluster == tag ){
        size = -4;
    } else {
        return MFX_ERR_UNKNOWN;
    }

    // Jump to the next section
    return 0 == vm_file_fseek(m_fSource, size, VM_FILE_SEEK_CUR) ? MFX_ERR_NONE : MFX_ERR_ABORTED;
 }

 /* Read simple block of data */
mfxStatus MKVReader::ReadSimpleBlock(mfxBitstream2 &bs){
    mfxU32 block_size;
    TagId  tag;
    mfxU32 size;
    mfxU32 track_num;
    mfxU8 *data;
    Ipp64u begin;
    int rewind_attempt;
    int shift = 3; 

    for(;;) {
        tag = GetTag();    
        if ( tag == TAG_ReferenceBlock || 
             tag == TAG_Position || 
             tag == TAG_BlockDuration){
            // Skip tags w/o actual data
            size = GetSize();
            vm_file_fseek(m_fSource, size, VM_FILE_SEEK_CUR);
            continue;
        } 
        else if ( tag == TAG_Cluster ){
            // New cluster found
            vm_file_fseek(m_fSource, -4, VM_FILE_SEEK_CUR);
            return MFX_ERR_NONE;
        }
        else if ( tag == TAG_UNKNOWN ) {
            return MFX_ERR_ABORTED;
        }
        else if ( TAG_EOF == tag ) {
            return MFX_ERR_MORE_DATA;
        }

        block_size = GetSize();
        if ( tag == TAG_BlockGroup ){
            tag = GetTag();
            block_size = GetSize();
        }
        // Get number of associated track
        track_num = GetSize();

        // Actual block data started here
        begin = vm_file_ftell(m_fSource);    
        rewind_attempt = 0;
        shift = 3; // 3 bytes are not needed - just different flags. Sometimes
                   // one byte is omitted. In this case, there will be rewind 
SEEK:
        if ( rewind_attempt ){
            // It was wrong to skip 3 bytes, need to skip only 2
            shift = 2;
        }
        
        if ( track_num == m_track_number ){
            vm_file_fseek(m_fSource, shift, VM_FILE_SEEK_CUR);

            block_size-=shift+1;
            data = new mfxU8[block_size];
            if (!vm_file_fread(data, 1, block_size, m_fSource))
            {
                return MFX_ERR_UNKNOWN;
            }
 
            mfxU32 internal = 0;
            mfxU32 jump;

            for(;(m_codec != MFX_CODEC_VP8) && (m_codec != MFX_CODEC_VP9);){
                // Decode data
                // First 4 bytes contain size of sub-block. Extract it. 
                if (data[internal] & 0x80){
                    data[internal] ^= 0x80;
                } else if ( data[internal] & 0x01 ){
                    data[internal] ^= 0x01;
                }

                jump = (data[internal] << 24 | data[internal+1] << 16 | data[internal+2] << 8 | data[internal + 3]);

                if ( jump > block_size){
                    // Size of sub-block is bigger than size of block itself. It means that 
                    // initial shift was not correct. 
                    if ( rewind_attempt ){
                        break;
                    }
                    rewind_attempt = 1;
                    block_size+=shift+1;
                    delete data;
                    vm_file_fseek(m_fSource, begin, VM_FILE_SEEK_SET);
                    goto SEEK;
                }
                data[internal] = data[internal+1] = data[internal+2] = 0;
                data[internal+3] = 1;
                internal += jump + 4;
                
                if ( jump == 1 ) { break; }
                if ( internal >= block_size) break;
            }
            // Copy data to bitstream
            memcpy(bs.Data + bs.DataLength, data, block_size);
            bs.DataLength += block_size;
            delete[] data;
            if ( (MFX_CODEC_VP8 == m_codec) || (MFX_CODEC_VP9 == m_codec) )
            {
                return MFX_ERR_NONE;
            }
        } else {
            // It was block not connected to video stream. Skip it. 
            block_size -= 1;
            vm_file_fseek(m_fSource, block_size, VM_FILE_SEEK_CUR);
        }
    }
}

mfxStatus MKVReader::ReadCluster(mfxBitstream2 &bs)
{

    int  value_int;
    short value_short;
    mfxU8 value_byte;
    TagId tag;

    for(;;){
        tag = GetTag();
        if ( tag == TAG_Attachments || tag == TAG_Chapters || tag == TAG_EBMLVoid){
            mfxU32 size = GetSize();
            vm_file_fseek(m_fSource, size, VM_FILE_SEEK_CUR);
            continue;
        }
        if (TAG_EOF == tag)
        {
            return MFX_ERR_MORE_DATA;
        }
        if ( TAG_SimpleBlock == tag )
        {
            vm_file_fseek(m_fSource, -1, VM_FILE_SEEK_CUR);
            return ReadSimpleBlock(bs);
        }

        CHECK_TAG(tag, TAG_Cluster);
        mfxU32 size = GetSize();
        
        if ( size > bs.MaxLength){
            
            mfxU32 nNewLen = (1024 * 1024) * ( (size) / (1024 * 1024) + (((size) % (1024 * 1024)) ? 1 : 0));
            mfxU8 * p;
            p = new mfxU8[nNewLen], MFX_ERR_MEMORY_ALLOC;

            bs.MaxLength = nNewLen;
            memcpy(p, bs.Data + bs.DataOffset, bs.DataLength);
            delete [] bs.Data;
            bs.Data = p;
            bs.DataOffset = 0;
    
        }
        tag = GetTag();
        CHECK_TAG(tag, TAG_Timecode);
        size = GetSize();

        if ( size == 1 ) {
            ReadValue(size, DATATYPE_BYTE, &value_byte);
        } else if ( size == 2 )  {
            ReadValue(size, DATATYPE_SHORT, &value_short);
        } else if ( size == 3 )  {
            ReadValue(size, DATATYPE_BYTE3, &value_int);
        }

        return ReadSimpleBlock(bs);
    }
}
