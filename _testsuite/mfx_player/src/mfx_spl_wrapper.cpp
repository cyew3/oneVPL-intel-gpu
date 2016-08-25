/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */
#include "mfx_pipeline_defs.h"

#include "mfx_spl_wrapper.h"
#define UMC_ENABLE_FILE_READER

//preventing from vm_assert redefinition 
#undef assert

#include "umc_splitter.h"
#include "umc_avi_splitter.h"
#include "umc_threaded_demuxer.h"
#include "umc_ivf_splitter.h"
#include "umc_file_reader.h"
#include "umc_corruption_reader.h"
#include "mfx_frame_constructor.h"
#include "mfxjpeg.h"
#include "mfxvp8.h"
#include "mfxvp9.h"

using namespace UMC;

namespace UMC
{
    Splitter *CreateVC1Splitter();
    Splitter *CreateASFSplitter();
    Splitter *CreateMPEG4Splitter();
}

#undef  UMC_CALL
#define UMC_CALL(FUNC)\
{\
    Status _umcRes;\
    MFX_CHECK_SET_ERR(UMC_OK == (_umcRes = FUNC), PipelineGetLastErr(), Status2MfxStatus(_umcRes))\
}

UMCSplWrapper::UMCSplWrapper(mfxU32 nCorruptionLevel)
    : m_pSplitter()
    , m_pReader()
    , m_pInputData()
    , m_pVideoInfo()
    , m_pInitParams()
    , m_pConstructor()
    , m_isVC1()
    , m_bInited()
    , m_bDecSpecInfo()
    , m_nCorruptionLevel(nCorruptionLevel)
{
}

UMCSplWrapper::~UMCSplWrapper()
{
    Close();
}

void UMCSplWrapper::Close()
{
    MFX_DELETE(m_pSplitter);
    MFX_DELETE(m_pInputData);
    MFX_DELETE(m_pReader);
    MFX_DELETE(m_pInitParams);
    MFX_DELETE(m_pConstructor);
    m_isVC1 = false;
    m_bDecSpecInfo = false;
}


mfxStatus Status2MfxStatus(UMC::Status val)
{
    switch (val)
    {
        case UMC_ERR_NOT_ENOUGH_DATA: return MFX_ERR_MORE_DATA;
    }
    
    return MFX_ERR_UNKNOWN;
}

//////////////////////////////////////////////////////////////////////////

mfxStatus UMCSplWrapper::Init(const vm_char *strFileName)
{
    MpegSplitterParams*iMpegSplitterParams;
    bool               isMp4 = false;
    SystemStreamType   streamType;

    // Data Reader
    MFX_CHECK_STS(SelectDataReader(strFileName))
    
    // Create Splitter
    streamType = Splitter::GetStreamType(m_pReader);
    

    switch(streamType)
    {
    case MPEGx_SYSTEM_STREAM:
        
        MFX_CHECK_WITH_ERR(iMpegSplitterParams = new MpegSplitterParams(), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(iMpegSplitterParams->m_mediaData = new MediaDataEx(), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pSplitter         = new UMC::ThreadedDemuxer(), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pConstructor      = new MFXFrameConstructor(), MFX_ERR_MEMORY_ALLOC);

        m_pInputData        = iMpegSplitterParams->m_mediaData;
        m_pInitParams       = iMpegSplitterParams;
        break;

    case AVI_STREAM:
        MFX_CHECK_WITH_ERR(m_pInputData     = new MediaDataEx, MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pInitParams    = new SplitterParams(), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pSplitter      = new UMC::AVISplitter(), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pConstructor   = new MFXFrameConstructor(), MFX_ERR_MEMORY_ALLOC);
        break;

    case MP4_ATOM_STREAM:
        MFX_CHECK_WITH_ERR(iMpegSplitterParams = new MpegSplitterParams(), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(iMpegSplitterParams->m_mediaData = new MediaDataEx(), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pSplitter         = CreateMPEG4Splitter(), MFX_ERR_MEMORY_ALLOC);
        // we need to know stream type (AVC/HEVC) to initialize correct frame constructor
//        MFX_CHECK_WITH_ERR(m_pConstructor      = new MFXAVCFrameConstructor(), MFX_ERR_MEMORY_ALLOC);

        m_pInputData        = iMpegSplitterParams->m_mediaData;
        m_pInitParams       = iMpegSplitterParams;
        isMp4               = true;
        break;

    case VC1_PURE_VIDEO_STREAM:
        MFX_CHECK_WITH_ERR(m_pInputData  = new MediaData, MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pSplitter   = CreateVC1Splitter(), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pInitParams = new SplitterParams(), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pConstructor= new MFXFrameConstructor(), MFX_ERR_MEMORY_ALLOC);
        m_isVC1         = true;
        break;

    case ASF_STREAM:
        MFX_CHECK_WITH_ERR(m_pInputData  = new MediaData, MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pSplitter   = CreateASFSplitter(), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pInitParams = new SplitterParams(), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pConstructor= new MFXVC1FrameConstructor(), MFX_ERR_MEMORY_ALLOC);
        break;
    case IVF_STREAM:
        MFX_CHECK_WITH_ERR(m_pInputData  = new MediaData, MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pSplitter = new IVFSplitter (), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pInitParams = new SplitterParams(), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR(m_pConstructor   = new MFXFrameConstructor(), MFX_ERR_MEMORY_ALLOC);
        break;
        
    default:
        UMC_CALL(UMC_ERR_UNSUPPORTED);
        break;
    }

    MFX_CHECK_POINTER(m_pSplitter);

    // Init splitter
    m_pInitParams->m_lFlags             = FLAG_VSPL_VIDEO_HEADER_REQ | UMC::VIDEO_SPLITTER;
    m_pInitParams->m_pDataReader        = m_pReader;
    m_pInitParams->m_uiSelectedVideoPID = SELECT_ANY_VIDEO_PID;
    UMC_CALL(m_pSplitter->Init(*m_pInitParams));
    UMC_CALL(m_pSplitter->GetInfo(&m_SplitterInfo));

    TrackType videoType;
    m_iVideoPid = -1;

    for (Ipp32u i=0; i<m_SplitterInfo->m_nOfTracks; i++)
    {
        if ((videoType = m_SplitterInfo->m_ppTrackInfo[i]->m_Type) & TRACK_ANY_VIDEO)
        {
            m_iVideoPid = i;
            if (NULL != m_SplitterInfo->m_ppTrackInfo[i]->m_pDecSpecInfo)
            {
                m_pInputData->operator = (*m_SplitterInfo->m_ppTrackInfo[i]->m_pDecSpecInfo);
                m_bDecSpecInfo = true;
            }
            break;
        }
    }
    
    MFX_CHECK(-1 != m_iVideoPid);

    m_pVideoInfo = reinterpret_cast<sVideoStreamInfo *>
        (m_SplitterInfo->m_ppTrackInfo[m_iVideoPid]->m_pStreamInfo);

    const vm_char * cFmt = VM_STRING("");
    if (UNCOMPRESSED_VIDEO == m_pVideoInfo->stream_type)
    {
        if (m_pVideoInfo->color_format == YV12)
        {
            cFmt = VM_STRING("-YV12");
        }else if (m_pVideoInfo->color_format == NV12)
        {
            cFmt = VM_STRING("-NV12");
        }
        else
        {
            cFmt = VM_STRING("");
        }
    }

    PrintInfo(VM_STRING("Container"), VM_STRING("%s"), GetStreamTypeString(streamType));
    PrintInfo(VM_STRING("Video"), VM_STRING("%s%s"), GetVideoTypeString(m_pVideoInfo->stream_type), cFmt);
    PrintInfo(VM_STRING("Resolution"), VM_STRING("%dx%d"), m_pVideoInfo->clip_info.width,m_pVideoInfo->clip_info.height);

    if (!m_isVC1)
    {
        UMC_CALL(m_pSplitter->Run());
     //   m_pSplitter->GetNextData(m_pInputData, m_iVideoPid);
    }

    //initializing of frame constructor
    mfxVideoParam vParam;
    vParam.mfx.FrameInfo.Width  = (mfxU16)m_pVideoInfo->clip_info.width;
    vParam.mfx.FrameInfo.Height = (mfxU16)m_pVideoInfo->clip_info.height;
    vParam.mfx.CodecProfile     = (mfxU16)m_pVideoInfo->profile;

    if (WVC1_VIDEO == m_pVideoInfo->stream_subtype)
        vParam.mfx.CodecProfile = MFX_PROFILE_VC1_ADVANCED;
    else if (WMV3_VIDEO == m_pVideoInfo->stream_subtype)
        vParam.mfx.CodecProfile = MFX_PROFILE_VC1_MAIN;

    if (MP4_ATOM_STREAM == streamType)
    {
        if (HEVC_VIDEO == m_pVideoInfo->stream_type)
        {
            MFX_CHECK_WITH_ERR(m_pConstructor = new MFXHEVCFrameConstructor(), MFX_ERR_MEMORY_ALLOC);
        }
        else
        {
            MFX_CHECK_WITH_ERR(m_pConstructor = new MFXAVCFrameConstructor(), MFX_ERR_MEMORY_ALLOC);
        }
    }

    MFX_CHECK_STS(m_pConstructor->Init(&vParam));

    m_bInited    =  true;
    m_nFrames    =  0;
    m_fLastTime  =  0.0;
    m_fFirstTime = -1.0;

    MFX_CHECK_STS(GetCurrentTimeFromSpl());
    //PRINT1("StreamFormat:  %s\n", GetStreamTypeString(m_SplitterInfo->m_system_info.stream_type));

    return MFX_ERR_NONE;
}

mfxStatus UMCSplWrapper::ReadNextFrame(mfxBitstream2 &bs2)
{
    MFX_CHECK(m_bInited);
    MFX_CHECK_POINTER(m_pSplitter);

    bool bforce = false;;
    for (;;)
    {
        if (m_pInputData->GetDataSize() == 0 || bforce)
        {
            UMC::Status umcRes = m_pSplitter->GetNextData(m_pInputData, m_iVideoPid);
            if(m_bInited && (UMC_ERR_NOT_ENOUGH_DATA == umcRes))
            {
                MPA_TRACE("Sleep(5)");
                vm_time_sleep(5);
                continue;
            }

            //exiting due to close command
            if (!m_bInited)
                return MFX_ERR_UNKNOWN;
            
            //exiting due to endof streams
            if (umcRes == UMC_ERR_END_OF_STREAM)
            {
                if (bs2.DataFlag & MFX_BITSTREAM_EOS)
                    return MFX_ERR_UNKNOWN;
                bs2.DataFlag |= MFX_BITSTREAM_EOS;
                //PipelineTrace((VM_STRING("\n")));
                return MFX_ERR_NONE;
            }

            UMC_CALL(umcRes);
            m_fLastTime = m_pInputData->GetTime();
        }

#if ENABLE_GOPs_CUTTING
        {
            static int first_Iframe = 0;

            if(first_Iframe && m_pInputData->GetFrameType() == I_PICTURE )
            {
                first_Iframe ++;
            }

            if (!first_Iframe)
            {
                first_Iframe = 1;
            }

            if (first_Iframe < 10)
            {
                bforce=true;
                m_pInputData->SeetDataSize(0);
                continue;
            }

            static FILE *pFile = fopen("c:\\1\\1.m2v","wb");
            fwrite (m_pInputData->GetDataPointer(), 1, m_pInputData->GetDataSize(), pFile);
            

        }
#endif
        mfxBitstream bs = {};
        
        bs.Data       = (mfxU8*) m_pInputData->GetDataPointer();
        bs.DataOffset = 0;
        bs.MaxLength  = (mfxU32)m_pInputData->GetDataSize();
        bs.DataLength = bs.MaxLength;
        bs.TimeStamp  = ConvertmfxF642MFXTime(m_fLastTime);
        
        switch(m_pInputData->GetFrameType())
        {
            case I_PICTURE    : bs.FrameType  = MFX_FRAMETYPE_I; break;
            case P_PICTURE    : bs.FrameType  = MFX_FRAMETYPE_P; break;
            case B_PICTURE    : bs.FrameType  = MFX_FRAMETYPE_B; break;
            default: break;
        }
        

        MFX_CHECK_STS_SKIP(m_pConstructor->ConstructFrame(&bs, &bs2), MFX_ERR_MORE_DATA);
        //skip whole frame
        m_pInputData->MoveDataPointer(m_pInputData->GetDataSize());

        //we put only header need to copy rest of the frame
        if (m_bDecSpecInfo) {
            m_bDecSpecInfo= false;
            continue;
        }

        break;
    }

    return MFX_ERR_NONE;
}

mfxStatus UMCSplWrapper::GetStreamInfo(sStreamInfo * pParams)
{
    MFX_CHECK(m_bInited);
    MFX_CHECK_POINTER(pParams);
    
    pParams->nWidth  = (mfxU16)m_pVideoInfo->clip_info.width;
    pParams->nHeight = (mfxU16)m_pVideoInfo->clip_info.height;

    switch (m_pVideoInfo->stream_type)
    {
        case MJPEG_VIDEO:
            pParams->videoType = MFX_CODEC_JPEG;
            break;
        case MPEG1_VIDEO: 
        case MPEG2_VIDEO: pParams->videoType = MFX_CODEC_MPEG2; break;
//        case MPEG4_VIDEO: pParams->videoType = MFX_FOURCC_MPEG4; break;
//        case H261_VIDEO : pParams->videoType = MFX_FOURCC_H261;  break;
        case H263_VIDEO : pParams->videoType = MFX_CODEC_H263;  break;
        case H264_VIDEO : pParams->videoType = MFX_CODEC_AVC;   break;
        case HEVC_VIDEO : pParams->videoType = MFX_CODEC_HEVC;  break;
        case WMV_VIDEO  :
        case VC1_VIDEO  : pParams->videoType = MFX_CODEC_VC1;   break;
        case VP8_VIDEO  : pParams->videoType = MFX_CODEC_VP8;   break;
        case VP9_VIDEO  : pParams->videoType = MFX_CODEC_VP9;   break;
        case UNCOMPRESSED_VIDEO : 
        {
            if (m_pVideoInfo->color_format == YV12)
            {
                pParams->videoType = MFX_FOURCC_YV12;
            }else
            if (m_pVideoInfo->color_format == NV12)
            {
                pParams->videoType = MFX_FOURCC_NV12;
            }else
            {
                return MFX_ERR_UNKNOWN;
            }
            break;
        }
        
        default : 
            return MFX_ERR_UNKNOWN;
    }
    if (WVC1_VIDEO == m_pVideoInfo->stream_subtype)
        pParams->CodecProfile = MFX_PROFILE_VC1_ADVANCED;
    else if (WMV3_VIDEO == m_pVideoInfo->stream_subtype)
        pParams->CodecProfile = MFX_PROFILE_VC1_MAIN;
 
    if (m_SplitterInfo->m_SystemType == MP4_ATOM_STREAM && pParams->videoType == MFX_CODEC_AVC)
    {
        pParams->isDefaultFC = false;
    }else
    if (m_SplitterInfo->m_SystemType == ASF_STREAM && pParams->videoType == MFX_CODEC_VC1)
    {
        pParams->isDefaultFC = false;
    }else
    {
        pParams->isDefaultFC = true;
    }

    return MFX_ERR_NONE;
}

mfxStatus UMCSplWrapper::SeekTime(mfxF64 fSeekTo)
{
    if (fSeekTo <= 0.0)
    {
        fSeekTo = 0.0;
    }
    
    m_pInputData->SetDataSize(0);
    m_fLastTime  =  0.0;

    PipelineTrace((VM_STRING("Seeking to : %.2lf ....."), fSeekTo));
    //m_fLastTime = fSeekTo;
    if (fSeekTo == 0.0)
    {
        UMC_CALL(m_pSplitter->Close());
        UMC_CALL(m_pInitParams->m_pDataReader->SetPosition(0.0));
        UMC_CALL(m_pSplitter->Init(*m_pInitParams));
        UMC_CALL(m_pSplitter->GetInfo(&m_SplitterInfo));
        
        //hope video pid won't change
        m_pVideoInfo = reinterpret_cast<sVideoStreamInfo *>
            (m_SplitterInfo->m_ppTrackInfo[m_iVideoPid]->m_pStreamInfo);
        
        if (!m_isVC1)
        {
            UMC_CALL(m_pSplitter->Run());
            //   m_pSplitter->GetNextData(m_pInputData, m_iVideoPid);
        }

    }else
    {
        UMC_CALL(m_pSplitter->SetTimePosition(fSeekTo));
        UMC_CALL(m_pSplitter->Run());
    }

    if (NULL != m_SplitterInfo->m_ppTrackInfo[m_iVideoPid]->m_pDecSpecInfo)
    {
        m_pInputData->operator = 
            (*m_SplitterInfo->m_ppTrackInfo[m_iVideoPid]->m_pDecSpecInfo);
        m_bDecSpecInfo = true;
    }
    
    MFX_CHECK_STS(GetCurrentTimeFromSpl());
    m_pConstructor->Reset();


    return MFX_ERR_NONE;
}

mfxStatus UMCSplWrapper::SeekPercent(mfxF64 fSeekTo)
{
    return SeekTime(m_SplitterInfo->m_dDuration * fSeekTo);
}

mfxStatus UMCSplWrapper::SeekFrameOffset(mfxU32 nFrameOffset, mfxFrameInfo & /*in_info*/)
{
    double dSeekTo = nFrameOffset * 1. / m_pVideoInfo->framerate;
    PipelineTrace((VM_STRING("INFO: reposition to frame=%d, using target timestamp=%lf\n"), nFrameOffset, dSeekTo));
    return SeekTime(dSeekTo);
}

mfxStatus UMCSplWrapper::GetCurrentTimeFromSpl()
{
    m_fLastTime = 0.0;
    UMC::MediaData tmp;
    int nTimeout = TimeoutVal<>::val();
    int nSleep   = 5;
    for (;nTimeout>0;nTimeout-=nSleep)
    {
        UMC::Status umcRes = m_pSplitter->CheckNextData(&tmp, m_iVideoPid);
        if(UMC_ERR_NOT_ENOUGH_DATA == umcRes)
        {
            MPA_TRACE("Sleep(Splitter)");
            vm_time_sleep(nSleep);
            continue;
        }else if (UMC_ERR_END_OF_STREAM == umcRes)
        {
            m_fLastTime = -1;
            return MFX_ERR_NONE;
        }

        UMC_CALL(umcRes);
        break;
    }
    MFX_CHECK(nTimeout > 0);
    m_fLastTime = tmp.GetTime();
 
    return MFX_ERR_NONE;
}

mfxStatus UMCSplWrapper::SelectDataReader(const vm_char * strFileName)
{
    std::auto_ptr<DataReader>       pReader;
    std::auto_ptr<DataReaderParams> pParams;

    //creating generic reader
    {
        FileReaderParams   *pFileReaderParams;
        MFX_CHECK_WITH_ERR((pReader.reset(new FileReader()), NULL != pReader.get()), MFX_ERR_MEMORY_ALLOC);
        MFX_CHECK_WITH_ERR((pParams.reset(pFileReaderParams = new FileReaderParams()), NULL != pParams.get()), MFX_ERR_MEMORY_ALLOC);

        pFileReaderParams->m_portion_size = 0;
        MFX_CHECK(0 == vm_string_strcpy_s(pFileReaderParams->m_file_name, MFX_ARRAY_SIZE(pFileReaderParams->m_file_name), strFileName));
    }
    
    //creating corrupter
    switch(m_nCorruptionLevel)
    {
        case 0:
        {
            break;
        }

        //non zero case will create corrupter
        default:
        {
            //file reader over wich we will corrupt
            std::auto_ptr<DataReader>        pFileReader = pReader;
            std::auto_ptr<DataReaderParams>  pFileReaderParams = pParams;

            CorruptionReaderParams   *pCorruptionParams;

            MFX_CHECK_WITH_ERR((pParams.reset(pCorruptionParams = new CorruptionReaderParams()), NULL != pParams.get()), MFX_ERR_MEMORY_ALLOC);
            MFX_CHECK_WITH_ERR((pReader.reset(new CorruptionReader()), NULL != pReader.get()), MFX_ERR_MEMORY_ALLOC);

            pCorruptionParams->CorruptMode   = m_nCorruptionLevel;
            pCorruptionParams->pActual       = pFileReader.release();
            pCorruptionParams->pActualParams = pFileReaderParams.release();
            break;
        }
    }
    
    // Data Reader

    MFX_CHECK_LASTERR(UMC_OK == pReader->Init(pParams.get()));
     
    m_pReader = pReader.release();
    pParams.reset();

    return MFX_ERR_NONE;
}