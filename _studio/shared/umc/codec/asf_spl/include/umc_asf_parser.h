//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_ASF_PARSER_H__
#define __UMC_ASF_PARSER_H__

#include "umc_asf_guids.h"

/*******************DECLARATIONS TYPES********************/

namespace UMC
{

#define ASF_MAX_NUM_OF_STREAMS  10

struct asf_Object
{
    asf_GUID  objectID;
    Ipp64u startPos;    /* start pos in file */
    Ipp64u endPos;      /* end pos in file */
    Ipp64u size;        /* number of byte in this object */
};

struct asf_CodecEntry
{
    asf_CodecEntry()
    {
        pCodecName = NULL;
        pCodecDescription = NULL;
        pCodecInfo = NULL;
    }
    Ipp16u    type;
    Ipp16u    codecNameLen;
    Ipp16u    *pCodecName;
    Ipp16u    codecDescrLen;
    Ipp16u    *pCodecDescription;
    Ipp16u    codecInfoLen;
    Ipp8u     *pCodecInfo;
};

struct asf_CodecListObject
{
    asf_CodecListObject()
    {
        pCodecEntries = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    asf_GUID  reserved;
    Ipp32u    codecEntriesCount;
    asf_CodecEntry *pCodecEntries;
};

struct asf_CmdType
{
    asf_CmdType()
    {
        pCmdTypeName = NULL;
    }
    Ipp16u    cmdTypeNameLen;
    Ipp16u    *pCmdTypeName;
};

struct asf_CmdEntry
{
    asf_CmdEntry()
    {
        cmdName = NULL;
    }
    Ipp32u    presTime;
    Ipp16u    typeIndex;
    Ipp16u    cmdNameLen;
    Ipp16u    *cmdName;
};

struct asf_ScriptCmdObject
{
    asf_ScriptCmdObject()
    {
        pCmdTypes = NULL;
        pCmdEntries = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    asf_GUID  reserved;
    Ipp16u    cmdCount;
    Ipp16u    cmdTypes;
    asf_CmdType   *pCmdTypes;
    asf_CmdEntry  *pCmdEntries;
};

struct asf_Marker
{
    asf_Marker()
    {
        pMarkerDescr = NULL;
    }
    Ipp64u    offset;
    Ipp64u    presTime;
    Ipp16u    entryLen;
    Ipp32u    sendTime; /*** in milliseconds ***/
    Ipp32u    flags;
    Ipp32u    markerDescrLen;
    Ipp16u    *pMarkerDescr;
};

struct asf_MarkerObject
{
    asf_MarkerObject()
    {
        pName = NULL;
        pMarkers = NULL;
    }
    asf_GUID    objectID;
    Ipp64u      objectSize;
    asf_GUID    reserved1;
    Ipp32u      markersCount;
    Ipp16u      reserved2;
    Ipp16u      nameLen;
    Ipp16u      *pName;
    asf_Marker  *pMarkers;
};

struct asf_BitrateMutualExclObject
{
    asf_BitrateMutualExclObject()
    {
        pStreamNumbers = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    asf_GUID  exclusionType;
    Ipp16u    streamNumbersCount;
    Ipp16u    *pStreamNumbers;
};

struct asf_ErrCorrectObject
{
    asf_ErrCorrectObject()
    {
        pErrCorrectData = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    asf_GUID  errCorrectType;
    Ipp32u    errCorrectDataLen;
    Ipp8u     *pErrCorrectData;
};

struct asf_ContentDescrObject
{
    asf_ContentDescrObject()
    {
        pTitle = NULL;
        pAuthor = NULL;
        pCopyright = NULL;
        pDescription = NULL;
        pRating = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    Ipp16u    titleLen;
    Ipp16u    authorLen;
    Ipp16u    copyrightLen;
    Ipp16u    DescrLen;
    Ipp16u    RatingLen;
    Ipp16u    *pTitle;
    Ipp16u    *pAuthor;
    Ipp16u    *pCopyright;
    Ipp16u    *pDescription;
    Ipp16u    *pRating;
};

struct asf_ContentDescriptor
{
    asf_ContentDescriptor()
    {
        pDescrName = NULL;
        pDescrValue = NULL;
    }
    Ipp16u    descrNameLen;
    Ipp16u    *pDescrName;
    Ipp16u    descrValueDataType;
    Ipp16u    descrValueLen;
    void      *pDescrValue; /*** depends on descrValueDataType ***/
};

struct asf_ExtContentDescrObject
{
    asf_ExtContentDescrObject()
    {
        pContDescr = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    Ipp16u    contentDescrCount;
    asf_ContentDescriptor  *pContDescr;
};

struct asf_BitrateRecord
{
    Ipp16u    flags;
    Ipp32u    averageBitrate;
};

struct asf_StreamBitratePropObject
{
    asf_StreamBitratePropObject()
    {
        pBitrateRecords = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    Ipp16u    bitrateRecordsCount;
    asf_BitrateRecord *pBitrateRecords;
};

struct asf_ContentBrandObject
{
    asf_ContentBrandObject()
    {
        pBannerImgData = NULL;
        pBannerImgUrl = NULL;
        pCopyrightUrl = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    Ipp32u    bannerImgType;
    Ipp32u    bannerImgDataSize;
    Ipp8u     *pBannerImgData;
    Ipp32u    bannerImgUrlLen;
    char      *pBannerImgUrl;   /*** ASCII (not WCHAR) ***/
    Ipp32u    copyrightUtlLen;
    char      *pCopyrightUrl;
};

struct asf_ContentEncryptObject
{
    asf_ContentEncryptObject()
    {
        pSecretData = NULL;
        pProtectType = NULL;
        pKeyID = NULL;
        pLicUrl = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    Ipp32u    secretDataLen;
    Ipp8u     *pSecretData;
    Ipp32u    protectTypeLen;
    char      *pProtectType;    /*** ASCII (not WCHAR) ***/
    Ipp32u    keyIDLen;
    char      *pKeyID;    /*** ASCII (not WCHAR) ***/
    Ipp32u    licUrlLen;
    char      *pLicUrl;    /*** ASCII (not WCHAR) ***/
};

struct asf_ExtContentEncryptObject
{
    asf_ExtContentEncryptObject()
    {
        pData = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    Ipp32u    dataSize;
    Ipp8u     *pData;
};

struct asf_DigSignatureObject
{
    asf_DigSignatureObject()
    {
        pSignatureData = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    Ipp32u    signatureType;
    Ipp32u    signatureDataLen;
    Ipp8u     *pSignatureData;
};

struct asf_PaddingObject
{
    asf_PaddingObject()
    {
        pPaddingData = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    Ipp8u     *pPaddingData;
};

/***** OBJECTS in the ASF HEADER EXTENSION OBJECT *****/

struct asf_StreamName
{
    asf_StreamName()
    {
        pStreamName = NULL;
    }
    Ipp16u    langIDIndex;
    Ipp16u    streamNameLen;
    Ipp16u    *pStreamName;
};

struct asf_PayldExtSys
{
    asf_GUID  extSysID;
    Ipp16u    extDataSize;
    Ipp32u    extSysInfoLen;
    Ipp8u     *pExtSysinfo;
};

struct asf_AdvMutualExclObject
{
    asf_AdvMutualExclObject()
    {
        pStreamNums = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    asf_GUID  exclType;
    Ipp16u    streamNumCount;
    Ipp16u    *pStreamNums;
};

struct asf_GroupMutualExclRecord
{
    asf_GroupMutualExclRecord()
    {
        pStreamNums = NULL;
    }
    Ipp16u    streamCount;
    Ipp16u    *pStreamNums;
};

struct asf_GroupMutualExclObject
{
    asf_GroupMutualExclObject()
    {
        pRecords = NULL;
    }
    asf_GUID                    objectID;
    Ipp64u                      objectSize;
    asf_GUID                    exclType;
    Ipp16u                      recordCount;
    asf_GroupMutualExclRecord *pRecords;
};

struct asf_PriorityRecord
{
    Ipp16u    streamNum;
    Ipp16u    priorityFlags;
};

struct asf_StreamPrioritObject
{
    asf_StreamPrioritObject()
    {
        pPrioritRecords = NULL;
    }
    asf_GUID            objectID;
    Ipp64u              objectSize;
    Ipp16u              priorityRecsCount;
    asf_PriorityRecord  *pPrioritRecords;
};

struct asf_BandwidthSharingObject
{
    asf_BandwidthSharingObject()
    {
        pStreamNums = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    asf_GUID  shareType;
    Ipp32u    dataBitrate;
    Ipp32u    bufferSize;
    Ipp16u    streamNumsCount;
    Ipp16u    *pStreamNums;
};

struct asf_LangIDRec
{
    asf_LangIDRec()
    {
        pLangID = NULL;
    }
    Ipp8u     langIDLen;
    Ipp16u    *pLangID; /*** UTF16 string ***/
};

struct asf_LangListObject
{
    asf_LangListObject()
    {
        pLangIDRecs = NULL;
    }
    asf_GUID        objectID;
    Ipp64u          objectSize;
    Ipp16u          ladIDRecsCount;
    asf_LangIDRec   *pLangIDRecs;
};

struct asf_MetadataDescrRecord
{
    asf_MetadataDescrRecord()
    {
        pName = NULL;
        pData = NULL;
    }
    Ipp16u    reserved;   /*** must be zero ***/
    Ipp16u    streamNum;
    Ipp16u    nameLen;
    Ipp16u    dataType;
    Ipp32u    dataLen;
    Ipp16u    *pName;     /*** UTF-16 string ***/
    Ipp8u     *pData;     /*** depends on dataType ***/
};

struct asf_MetadataObject
{
    asf_MetadataObject()
    {
        pDescrRecords = NULL;
    }
    asf_GUID                objectID;
    Ipp64u                  objectSize;
    Ipp16u                  descrRecordsCount;
    asf_MetadataDescrRecord *pDescrRecords;
};

struct asf_MetadataLibDescrRecord
{
    asf_MetadataLibDescrRecord()
    {
        pName = NULL;
        pData = NULL;
    }
    Ipp16u    langListIndex;
    Ipp16u    streamNum;
    Ipp16u    nameLen;
    Ipp16u    dataType;
    Ipp32u    dataLen;
    Ipp16u    *pName;     /*** UTF-16 string ***/
    Ipp8u     *pData;     /*** depends on dataType ***/
};

struct asf_MetadataLibObject
{
    asf_MetadataLibObject()
    {
        pDescrRecords = NULL;
    }
    asf_GUID                    objectID;
    Ipp64u                      objectSize;
    Ipp16u                      descrRecordsCount;
    asf_MetadataLibDescrRecord  *pDescrRecords;
};

struct asf_IndexSpecifier
{
    Ipp16u    streamNum;
    Ipp16u    indType;
};

struct asf_IndexParamsObject
{
    asf_IndexParamsObject()
    {
        pIndSpecifiers = NULL;
    }
    asf_GUID            objectID;
    Ipp64u              objectSize;
    Ipp32u              indEntryTimeInterval;
    Ipp16u              indSpecifiersCount;
    asf_IndexSpecifier  *pIndSpecifiers;
};

struct asf_MediaObjectIndexParamsObject
{
    asf_MediaObjectIndexParamsObject()
    {
        pIndSpecifiers = NULL;
    }
    asf_GUID            objectID;
    Ipp64u              objectSize;
    Ipp32u              indEntryCountInterval;
    Ipp16u              indSpecifiersCount;
    asf_IndexSpecifier  *pIndSpecifiers;
};

struct asf_TimecodeIndexParamsObject
{
    asf_TimecodeIndexParamsObject()
    {
        pIndSpecifiers = NULL;
    }
    asf_GUID            objectID;
    Ipp64u              objectSize;
    Ipp32u              indEntryCountInterval;
    Ipp16u              indSpecifiersCount;
    asf_IndexSpecifier  *pIndSpecifiers;
};

struct asf_CompatibilityObject
{
    asf_GUID  objectID;
    Ipp64u    objectSize;
    Ipp8u     profile;
    Ipp8u     mode;
};

struct asf_EncryptObjectRecord
{
    asf_EncryptObjectRecord()
    {
        pEencryptObjID = NULL;
    }
    Ipp16u    encryptObjIDType;
    Ipp16u    encryptObjIDLen;
    Ipp8u     *pEencryptObjID;
};

struct asf_ContEncryptRecord
{
    asf_ContEncryptRecord()
    {
        pEncryptObjRecs = NULL;
    }
    asf_GUID  systemID;
    Ipp32u    sysVersion;
    Ipp16u    encryptObjRecCount;
    asf_EncryptObjectRecord *pEncryptObjRecs;
    Ipp32u    dataSize;
    Ipp8u     *pData;
};

struct asf_AdvContentEncryptObject
{
    asf_AdvContentEncryptObject()
    {
        pContEncryptRecs = NULL;
    }
    asf_GUID                objectID;
    Ipp64u                  objectSize;
    Ipp16u                  contEncryptRecCount;
    asf_ContEncryptRecord   *pContEncryptRecs;
};

/***** ASF top-level Data Object *****/

struct asf_DataObject
{
    asf_GUID  objectID;
    Ipp64u    objectSize;
    asf_GUID  fileID;
    Ipp64u    totalDataPackets;
    Ipp16u    reserved;
};

 /*** WAVEFORMATEX structure ***/
struct asf_AudioMediaInfo
{
    asf_AudioMediaInfo()
    {
        pCodecSpecData = NULL;
    }
    Ipp16u    formatTag;
    Ipp16u    numChannels;
    Ipp32u    sampleRate;   /*** in Hertz ***/
    Ipp32u    avgBytesPerSec;
    Ipp16u    blockAlign;
    Ipp16u    bitsPerSample;
    Ipp16u    codecSpecDataSize;
    Ipp8u     *pCodecSpecData;
};

 /*** BITMAPINFOHEADER structure ***/

#define VIDEO_SPEC_DATA_LEN   40

struct asf_FormatData
{
    asf_FormatData()
    {
        pCodecSpecData = NULL;
    }
    Ipp32u    formatDataSize;
    Ipp32u    width;
    Ipp32u    height;
    Ipp16u    reserved;
    Ipp16u    bitsPerPixelCount;
    Ipp32u    compresID;
    Ipp32u    imgSize;
    Ipp32u    hrzPixelsPerMeter;
    Ipp32u    vertPixelsPerMeter;
    Ipp32u    colorsUsedCount;
    Ipp32u    importColorsCount;
    Ipp8u     *pCodecSpecData;    /*** formatDataSize - VIDEO_SPEC_DATA_LEN ***/
};

struct asf_VideoMediaInfo
{
    Ipp32u          width;
    Ipp32u          height;
    Ipp8u           flags;
    Ipp16u          formatDataSize;
    asf_FormatData  FormatData;
};

struct asf_SpreadAudioData
{
    asf_SpreadAudioData()
    {
        pSilenceData = NULL;
    }
    Ipp8u     span;
    Ipp16u    virtPackLen;
    Ipp16u    virtChunkLen;
    Ipp16u    silenceDataLen;
    Ipp8u     *pSilenceData;  /*** 0 for silenceDataLen bytes ***/
};

struct asf_StreamPropObject
{
    asf_StreamPropObject()
    {
        typeSpecData.pAnyData = NULL;
        pErrCorrectData = NULL;
    }
    asf_GUID  objectID;
    Ipp64u    objectSize;
    asf_GUID  streamType;
    asf_GUID  errCorrectType;
    Ipp64u    timeOffset;
    Ipp32u    typeSpecDataLen;
    Ipp32u    errCorrectDataLen;
    Ipp16u    flags;
    Ipp32u    reserved;

    union {
        void               *pAnyData;
        asf_AudioMediaInfo *pAudioSpecData;
        asf_VideoMediaInfo *pVideoSpecData;
    }         typeSpecData;    /* tagged type by streamType */
    asf_SpreadAudioData *pErrCorrectData;
};

struct asf_ExtStreamPropObject
{
    asf_ExtStreamPropObject()
    {
        pStreamNames = NULL;
        pPayldExtSystems = NULL;
        pStreamPropObj = NULL;
    }
    asf_GUID                objectID;
    Ipp64u                  objectSize;
    Ipp64u                  startTime;
    Ipp64u                  endTime;
    Ipp32u                  dataBitrate;
    Ipp32u                  bufSize;
    Ipp32u                  initialBufFullness;
    Ipp32u                  altDataBitrate;
    Ipp32u                  altBufSize;
    Ipp32u                  altInitialBufFullness;
    Ipp32u                  maxObjSize;
    Ipp32u                  flags;
    Ipp16u                  streamNum;
    Ipp16u                  streamLangIDIndex;
    Ipp64u                  avgTimePerFrame;
    Ipp16u                  streamNameCount;
    Ipp16u                  payldExtSysCount;
    asf_StreamName          *pStreamNames;
    asf_PayldExtSys         *pPayldExtSystems;
    asf_StreamPropObject    *pStreamPropObj;
};

/***** HEADER OBJECT (mandatory, the only) *****/

struct asf_FPropObject
{
    asf_GUID  objectID;
    Ipp64u    objectSize;
    asf_GUID  fileID;
    Ipp64u    fileSize;
    Ipp64u    creationDate;     /*** number of 100nanosec since Jan 1, 1601 ***/
    Ipp64u    dataPacketsCount;
    Ipp64u    playDuration;
    Ipp64u    sendDuration;
    Ipp64u    preroll;          /*** time to buffer - in milliseconds ***/
    Ipp32u    flags;
    Ipp32u    minDataPackSize;
    Ipp32u    maxDataPackSize;
    Ipp32u    maxBitrate;
};

struct asf_HeaderExtData
{
    asf_HeaderExtData()
  {
    pAdvMutualExclObject = NULL;
    pGroupMutualExclObject = NULL;
    pStreamPrioritObject = NULL;
    pAdvMutualExclObject = NULL;
    pLangListObject = NULL;
    pMetadataObject = NULL;
    pMetadataLibObject = NULL;
    pIndexParamsObject = NULL;
    pMediaObjectIndexParamsObject = NULL;
    pTimecodeIndexParamsObject = NULL;
    pCompatibilityObject = NULL;
    pAdvContentEncryptObject = NULL;
  }

    asf_ExtStreamPropObject *ppExtStreamPropObject[ASF_MAX_NUM_OF_STREAMS];
    asf_AdvMutualExclObject *pAdvMutualExclObject;
    asf_GroupMutualExclObject *pGroupMutualExclObject;
    asf_StreamPrioritObject *pStreamPrioritObject;
    asf_BandwidthSharingObject *pBandwidthSharingObject;
    asf_LangListObject *pLangListObject;
    asf_MetadataObject *pMetadataObject;
    asf_MetadataLibObject *pMetadataLibObject;
    asf_IndexParamsObject *pIndexParamsObject;
    asf_MediaObjectIndexParamsObject *pMediaObjectIndexParamsObject;
    asf_TimecodeIndexParamsObject *pTimecodeIndexParamsObject;
    asf_CompatibilityObject *pCompatibilityObject;
    asf_AdvContentEncryptObject *pAdvContentEncryptObject;

};

struct asf_HeaderExtObject
{
    asf_HeaderExtObject()
    {
        pHeaderExtData = NULL;
    }
    asf_GUID            objectID;
    Ipp64u              objectSize;
    asf_GUID            reserved1;
    Ipp16u              reserved2;
    Ipp32u              headerExtDataSize;
    asf_HeaderExtData   *pHeaderExtData;
};

struct asf_HeaderObject
{
    asf_HeaderObject()
  {
    objectID = ASF_Header_Object;
    objectSize = 0;
    reserved1 = 0x01;
    reserved2 = 0x02;
    pFPropObject = NULL;
    ppStreamPropObject = NULL;
    pHeaderExtObject = NULL;
    pCodecListObject = NULL;
    pScriptCmdObject = NULL;
    pMarkerObject = NULL;
    pBitrateMutualExclObject = NULL;
    pErrCorrectObject = NULL;
    pContentDescrObject = NULL;
    pExtContentDescrObject = NULL;
    pStreamBitratePropObject = NULL;
    pContentBrandObject = NULL;
    pExtContentEncryptObject = NULL;
    pDigSignatureObject = NULL;
    pPaddingObject = NULL;
  }

    asf_GUID  objectID;
    Ipp64u    objectSize;
    Ipp32u    nObjects;
    Ipp8u     reserved1;
    Ipp8u     reserved2;
    asf_FPropObject *pFPropObject;
    asf_StreamPropObject **ppStreamPropObject;
    asf_HeaderExtObject *pHeaderExtObject;
    asf_CodecListObject *pCodecListObject;
    asf_ScriptCmdObject *pScriptCmdObject;
    asf_MarkerObject *pMarkerObject;
    asf_BitrateMutualExclObject *pBitrateMutualExclObject;
    asf_ErrCorrectObject *pErrCorrectObject;
    asf_ContentDescrObject *pContentDescrObject;
    asf_ExtContentDescrObject *pExtContentDescrObject;
    asf_StreamBitratePropObject *pStreamBitratePropObject;
    asf_ContentBrandObject *pContentBrandObject;
    asf_ExtContentEncryptObject *pExtContentEncryptObject;
    asf_DigSignatureObject *pDigSignatureObject;
    asf_PaddingObject *pPaddingObject;

};

struct asf_ErrCorrectionData
{
    Ipp8u     errCorrFlags;
    Ipp8u     firstBType;;
    Ipp8u     secBCycle;
};

struct asf_PayloadParseInfo
{
    Ipp8u     mulPld;
    Ipp8u     seqType;
    Ipp8u     paddLenType;
    Ipp8u     packLenType;
    Ipp8u     errCorr;
    Ipp8u     replDataLenType;;
    Ipp8u     offIntoMediaObjLenType;;
    Ipp8u     mediaObjNumLenType;;
    Ipp8u     streamNumLenType;;
    Ipp32u    packLen;
    Ipp32u    sequence;
    Ipp32u    paddLen;
    Ipp32u    sendTime;
    Ipp16u    duration;
};

struct asf_Payload
{
    asf_Payload()
    {
        pReplData = NULL;
        pPldData = NULL;
    }
    Ipp32u    streamNumber;
    Ipp8u     keyFrame;
    Ipp32u    mediaObjNum;
    Ipp32u    offIntoMediaObj;
    Ipp32u    replDataLen;
    Ipp8u     *pReplData;
    Ipp32u    pldLen;
    Ipp8u     *pPldData;
};

} // namespace UMC

#endif //__UMC_MP4_PARSER_H__
