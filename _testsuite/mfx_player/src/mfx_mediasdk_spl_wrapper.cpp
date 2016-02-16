/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */
#include "mfx_pipeline_defs.h"

#include "mfx_mediasdk_spl_wrapper.h"
#include "mfx_frame_constructor.h"
#include "mfxvp8.h"

mfxU32 TIME_STAMP_FREQUENCY = 90000;

MediaSDKSplWrapper::MediaSDKSplWrapper(vm_char *extractedAudioFile)
    : m_streamParams()
    , m_pConstructor()
    , m_extractedAudioFile(NULL)
{
    if (0 != vm_string_strlen(extractedAudioFile))
    {
        m_extractedAudioFile = vm_file_fopen(extractedAudioFile, VM_STRING("wb"));
    }
}

MediaSDKSplWrapper::~MediaSDKSplWrapper()
{
    Close();
}

void MediaSDKSplWrapper::Close()
{
    //TODO: initialize m_streamParams
    for (mfxU32 i=0; i < m_streamParams.NumTracksAllocated; i++)
    {
        delete m_streamParams.TrackInfo[i];
        m_streamParams.TrackInfo[i] = NULL;
    }
    delete m_streamParams.TrackInfo;
    m_streamParams.TrackInfo = NULL;

    MFXSplitter_Close(m_mfxSplitter);

    MFX_DELETE(m_pConstructor);

    if (m_extractedAudioFile)
        vm_file_fclose(m_extractedAudioFile);
}

mfxStatus MediaSDKSplWrapper::Init(const vm_char *strFileName)
{
    mfxStatus sts = MFX_ERR_NONE;
    vm_file* pSrcFile = NULL;
    pSrcFile = vm_file_fopen(strFileName, VM_STRING("rb"));
    MFX_CHECK_POINTER(pSrcFile);

    MFX_ZERO_MEM(m_splReader);
    m_splReader.m_bInited = true;
    m_splReader.m_fSource = pSrcFile;

    m_dataIO.pthis = &m_splReader;
    m_dataIO.Read = RdRead;
    m_dataIO.Seek = RdSeek;

    sts = MFXSplitter_Init(&m_dataIO, &m_mfxSplitter);
    MFX_CHECK_STS(sts);

    MFX_ZERO_MEM(m_streamParams);
    sts = MFXSplitter_GetInfo(m_mfxSplitter, &m_streamParams);
    MFX_CHECK_STS(sts);

    m_streamParams.TrackInfo = new mfxTrackInfo*[m_streamParams.NumTracks];
    for (mfxU32 i=0; i < m_streamParams.NumTracks; i++)
        m_streamParams.TrackInfo[i] = new mfxTrackInfo;
    m_streamParams.NumTracksAllocated = m_streamParams.NumTracks;

    sts = MFXSplitter_GetInfo(m_mfxSplitter, &m_streamParams);
    MFX_CHECK_STS(sts);

    MFX_CHECK_WITH_ERR(m_pConstructor = new MFXFrameConstructor(), MFX_ERR_MEMORY_ALLOC);

    for (mfxU32 i=0; i < m_streamParams.NumTracks; i++)
    {
        if (m_streamParams.TrackInfo[i]->Type & MFX_TRACK_ANY_VIDEO)
        {
            //initializing of frame constructor
            mfxVideoParam vParam = {};
            vParam.mfx.FrameInfo.Width  = m_streamParams.TrackInfo[i]->VideoParam.FrameInfo.Width;
            vParam.mfx.FrameInfo.Height = m_streamParams.TrackInfo[i]->VideoParam.FrameInfo.Height;
            vParam.mfx.CodecProfile     = m_streamParams.TrackInfo[i]->VideoParam.CodecProfile;
            MFX_CHECK_STS(m_pConstructor->Init(&vParam));
            break;
        }
    }

    return sts;
}

mfxStatus MediaSDKSplWrapper::ReadNextFrame(mfxBitstream2 &bs2)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 iOutputTrack = 0;
    bool bVideoFrame = false;
    mfxBitstream bs = {};
    MFX_CHECK_POINTER(m_mfxSplitter);
    while (!bVideoFrame && sts == MFX_ERR_NONE)
    {
        if (sts == MFX_ERR_NONE)
        {
            sts = MFXSplitter_GetBitstream(m_mfxSplitter, &iOutputTrack, &bs);
        }
        if (sts == MFX_ERR_NONE && m_streamParams.TrackInfo[iOutputTrack]->Type & MFX_TRACK_ANY_VIDEO)
        {
            sts = m_pConstructor->ConstructFrame(&bs, &bs2);
            bVideoFrame = true;
        }
        else if (sts == MFX_ERR_NONE && m_streamParams.TrackInfo[iOutputTrack]->Type & MFX_TRACK_ANY_AUDIO && m_extractedAudioFile)
        {
            if (!vm_file_fwrite(bs.Data, 1, bs.DataLength, m_extractedAudioFile))
                return MFX_ERR_UNKNOWN;
        }
        if (sts == MFX_ERR_NONE)
        {
            sts = MFXSplitter_ReleaseBitstream(m_mfxSplitter, &bs);
        }
    }

    return sts;
}

mfxStatus MediaSDKSplWrapper::GetStreamInfo(sStreamInfo * pParams)
{
    MFX_CHECK_POINTER(pParams);

    for (mfxU32 i=0; i < m_streamParams.NumTracks; i++)
    {
        if (m_streamParams.TrackInfo[i]->Type & MFX_TRACK_ANY_VIDEO)
        {
            pParams->nWidth  = m_streamParams.TrackInfo[i]->VideoParam.FrameInfo.Width;
            pParams->nHeight = m_streamParams.TrackInfo[i]->VideoParam.FrameInfo.Height;

            switch (m_streamParams.TrackInfo[i]->Type)
            {
                case MFX_TRACK_MPEG2V: pParams->videoType = MFX_CODEC_MPEG2; break;
                case MFX_TRACK_H264  : pParams->videoType = MFX_CODEC_AVC;   break;
                case MFX_TRACK_VP8   : pParams->videoType = MFX_CODEC_VP8;   break;
                case MFX_TRACK_VC1   : pParams->videoType = MFX_CODEC_VC1;   break;
                default : 
                    return MFX_ERR_UNKNOWN;
            }

            pParams->CodecProfile = m_streamParams.TrackInfo[i]->VideoParam.CodecProfile;

            if (m_streamParams.SystemType == MFX_ASF_STREAM && pParams->videoType == MFX_CODEC_VC1)
            {
                pParams->isDefaultFC = false;
            }else
            {
                pParams->isDefaultFC = true;
            }

            break;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MediaSDKSplWrapper::SeekTime(mfxF64 fSeekTo)
{
    MFX_CHECK_POINTER(m_mfxSplitter);

    if (fSeekTo < 0)
        return MFX_ERR_UNKNOWN;

    return MFXSplitter_Seek(m_mfxSplitter,  (mfxU64)(fSeekTo * TIME_STAMP_FREQUENCY));
}

mfxStatus MediaSDKSplWrapper::SeekPercent(mfxF64 fSeekTo)
{
    MFX_CHECK_POINTER(m_mfxSplitter);

    if (fSeekTo < 0 || fSeekTo > 100)
        return MFX_ERR_UNKNOWN;

    return MFXSplitter_Seek(m_mfxSplitter, (mfxU64) (0.01f * fSeekTo * m_streamParams.Duration ));
}

mfxStatus MediaSDKSplWrapper::SeekFrameOffset(mfxU32 /*nFrameOffset*/, mfxFrameInfo & /*in_info*/)
{
    return MFX_ERR_NONE;
}

mfxI32 MediaSDKSplWrapper::RdRead(void* in_DataReader, mfxBitstream *BS)
{
    DataReader *pDr;
    size_t ReadSize;

    if (NULL == in_DataReader)
        return MFX_ERR_NULL_PTR;

    pDr = (DataReader*)(in_DataReader);
    if (NULL == pDr)
        return MFX_ERR_UNKNOWN;

    size_t freeSpace = BS->MaxLength - BS->DataOffset - BS->DataLength;
    void *freeSpacePtr = (void*)(BS->Data + BS->DataOffset + BS->DataLength);
    ReadSize = vm_file_fread(freeSpacePtr, 1, freeSpace, pDr->m_fSource);
    BS->DataLength +=  ReadSize;

    return ReadSize;
}

mfxI64 MediaSDKSplWrapper::RdSeek(void* in_DataReader, mfxI64 offset, mfxSeekOrigin origin)
{
    DataReader *pRd;
    Ipp64u oldOffset, fileSize;
    VM_FILE_SEEK_MODE whence;

    if (NULL == in_DataReader)
        return MFX_ERR_NULL_PTR;
    else
        pRd = (DataReader*)in_DataReader;

    switch(origin)
    {
    case MFX_SEEK_ORIGIN_BEGIN:
        whence = VM_FILE_SEEK_SET;
        break;
    case MFX_SEEK_ORIGIN_CURRENT:
        whence = VM_FILE_SEEK_CUR;
        break;
    default:
        whence = VM_FILE_SEEK_END;
        break;
    }

    if (whence == VM_FILE_SEEK_END && offset == 0)
    {
        oldOffset = vm_file_ftell(pRd->m_fSource);
        vm_file_fseek(pRd->m_fSource, 0, VM_FILE_SEEK_END);
        fileSize = vm_file_ftell(pRd->m_fSource);
        vm_file_fseek(pRd->m_fSource, oldOffset, VM_FILE_SEEK_SET);
        return fileSize;
    }
    vm_file_fseek(pRd->m_fSource, (long) offset, whence);

    return MFX_ERR_NONE;
}
