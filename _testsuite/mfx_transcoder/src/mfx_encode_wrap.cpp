/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_encoder_wrap.h"
#include "mfx_ext_buffers.h"
#include "mfx_serializer.h"
#include "ipps.h"
#include "ippi.h"


MFXEncodeWRAPPER::MFXEncodeWRAPPER(ComponentParams & refParams, mfxStatus *status, std::auto_ptr<IVideoEncode>& pEncode)
    : MFXFileWriteRender(FileWriterRenderInputParams(MFX_FOURCC_NV12), refParams.m_pSession, status)
    , m_refParams(refParams)
    , m_encoder(pEncode)
    , m_ExtraParams()
{
    m_pSyncPoints       = NULL;
    m_EncodedSize       = 0;
    m_nSyncPoints       = 0;
    m_pCacheBuffer      = NULL;
    m_pRefFile          = NULL;
    
    if (!refParams.m_pSession && status)
    {
        *status = MFX_ERR_NULL_PTR;
    }else
    {
        m_InSession = refParams.m_pSession->GetMFXSession();
    }
}

MFXEncodeWRAPPER::~MFXEncodeWRAPPER()
{
    //avoiding double freeing because core is the same as for decoder
    m_InSession = NULL;

    SetOutBitstreamsNum(0);
    MFX_DELETE_ARRAY(m_pCacheBuffer);

    if (NULL != m_pRefFile)
    {
        vm_file_close(m_pRefFile);
        m_pRefFile = NULL;
    }

    PrintInfo(VM_STRING("Encode : number frames"), VM_STRING("%d"), m_nFrames);
    PrintInfo(VM_STRING("Encode : filesize"), VM_STRING("%I64u bytes"), m_EncodedSize);
    double expected_size = (double)m_VideoParams.mfx.TargetKbps * IPP_MAX(1, m_VideoParams.mfx.BRCParamMultiplier) * 1000 * m_nFrames / (8 * GetFrameRate(&m_VideoParams.mfx.FrameInfo));
    //PrintInfo(VM_STRING("expected_size"),           VM_STRING("%d"), (int)expected_size);
    PrintInfoForce(VM_STRING("Encode : bitrate accuracy"), VM_STRING("%.2lf"), (double)m_EncodedSize/expected_size);
    //_ftprintf(stdout, VM_STRING("Bitrate accuracy = %.2f\n"), (double)m_EncodedSize/expected_size);
    PrintInfoForce(VM_STRING("Encode : fps"), VM_STRING("%.2lf"), 1 == m_refParams.m_nMaxAsync ?(double)m_nFrames/m_encTime.OverallTiming():0.0);
}

mfxStatus MFXEncodeWRAPPER::SetOutBitstreamsNum(mfxU16 nBitstreamsOut)
{
    if (m_pSyncPoints)
    {
        for (mfxU32 i = 0; i < m_nSyncPoints; i++)
        {
            if (m_pSyncPoints[i].m_pBitstream)
            {
                MFX_DELETE_ARRAY(m_pSyncPoints[i].m_pBitstream->Data);
            }

            MFX_DELETE(m_pSyncPoints[i].m_pBitstream);

            if (m_pSyncPoints[i].m_bCtrlAllocated)
            {
                MFX_DELETE(m_pSyncPoints[i].m_pCtrl);
            }
        }
        MFX_DELETE_ARRAY(m_pSyncPoints);
    }
    
    m_nSyncPoints = 0;

    if (nBitstreamsOut == 0)
    {
        return MFX_ERR_NONE;
    }

    *((void**)&m_pSyncPoints) = (void*)(new char [(1 + nBitstreamsOut) * sizeof(m_pSyncPoints[0])]);
    
    MFX_CHECK_POINTER(m_pSyncPoints);
    m_nSyncPoints = nBitstreamsOut + 1;

    memset(m_pSyncPoints, 0, m_nSyncPoints * sizeof(m_pSyncPoints[0]));

    return MFX_ERR_NONE;
}

mfxStatus MFXEncodeWRAPPER::Init(mfxVideoParam *pInit, const vm_char *pFilename)
{
    mfxStatus ret = MFX_ERR_NONE;

    MFX_CHECK_POINTER(pInit);

    mfxVideoParam pBeforeInit;
    memcpy(&pBeforeInit, pInit, sizeof(pBeforeInit));

    mfxVideoParam vParamUser = {};
    MFXStructureThree<mfxVideoParam> structThree(pBeforeInit, *pInit, vParamUser);

    MFX_CHECK_STS_CUSTOM_HANDLER(ret = m_encoder->Init(pInit), {
        PipelineTrace((VM_STRING("---------------------- MFXInit FAILED -------------\n")
                     VM_STRING("%s---------------------------------------------------\n"), structThree.Serialize().c_str()));
    });
    MFX_CHECK_STS(m_encoder->GetVideoParam(pInit));

    //set user hadrcoded values
    if (0 != m_ExtraParams.nBufferSizeInKB)
    {
        pInit->mfx.BufferSizeInKB = m_ExtraParams.nBufferSizeInKB;
        vParamUser.mfx.BufferSizeInKB = 1;
    }
    
    PipelineTrace((VM_STRING("---------------------- MFXInit SUCEED -------------\n")
                 VM_STRING("%s---------------------------------------------------\n"), structThree.Serialize().c_str()));

    MFX_CHECK_STS(MFXFileWriteRender::Init(pInit, pFilename));

    return ret;
}

mfxStatus MFXEncodeWRAPPER::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    mfxStatus sts;
    MFX_CHECK_STS(sts = m_encoder->QueryIOSurf(par, request));
    if (MFX_ERR_NONE != sts && MFX_WRN_PARTIAL_ACCELERATION != sts)
    {
        request->NumFrameMin = request->NumFrameSuggested =  1;
    }
    
    return MFX_ERR_NONE;
}

mfxStatus MFXEncodeWRAPPER::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    //ATTENTION:
    //default behavior of query function is to set 0 to fields 
    //validity of that is cannot be defined by this function
    mfxStatus sts = m_encoder->Query(in, out);
    if (MFX_ERR_NONE <= sts) 
    {
        PipelineTrace((VM_STRING("------------------MFXQuery SUCCEED-----------------\n")
                     VM_STRING("%s---------------------------------------------------\n"), MFXStructuresPair<mfxVideoParam>(*in, *out).Serialize().c_str()));
        Serialize(*out);
    }
    return sts;
}

mfxStatus MFXEncodeWRAPPER::GetEncodeStat(mfxEncodeStat *stat)
{
    //ATTENTION : h264 encoder doesn't implemented this function
    //return MFXVideoENCODE_GetEncodeStat(m_InSession, stat);
    return m_encoder->GetEncodeStat(stat);
}

mfxStatus MFXEncodeWRAPPER::GetVideoParam(mfxVideoParam *par)
{
    return m_encoder->GetVideoParam(par);
}

mfxStatus MFXEncodeWRAPPER::Reset(mfxVideoParam *pParam)
{
    mfxVideoParam pBeforeReset = {};
    mfxVideoParam pAfterReset  = {};
    mfxVideoParam vParamUser   = {};

    //use input pparam since encoder may clean passed parameters in case of error
    MFXStructureThree<mfxVideoParam> structThree(pBeforeReset, *pParam, vParamUser);

    MFX_CHECK_STS(m_encoder->GetVideoParam(&pBeforeReset));

    MFX_CHECK_STS_CUSTOM_HANDLER(m_encoder->Reset(pParam), {
        PipelineTrace((VM_STRING("%s"), structThree.Serialize().c_str()));
    });
    
    MFX_CHECK_STS(m_encoder->GetVideoParam(&pAfterReset));

    PipelineTrace((VM_STRING("\n")));

    if (0 != m_ExtraParams.nBufferSizeInKB)
    {
        pParam->mfx.BufferSizeInKB = m_ExtraParams.nBufferSizeInKB;
        vParamUser.mfx.BufferSizeInKB = 1;
    }

    MFXStructureThree<mfxVideoParam> structThree2(pBeforeReset, pAfterReset, vParamUser);

    PipelineTrace((VM_STRING("%s"), structThree2.Serialize().c_str()));

    //freeing bitstreams
    for (int j = 0; j < m_nSyncPoints; j++)
    {
        m_pSyncPoints[j].m_SyncPoint  = NULL; 
    }

    return MFXFileWriteRender::Reset(pParam);
}

mfxStatus  MFXEncodeWRAPPER::Close()
{
    MFX_CHECK_STS_SKIP(m_encoder->Close(), MFX_ERR_NOT_INITIALIZED);
    return MFXFileWriteRender::Close();
}

mfxStatus MFXEncodeWRAPPER::WaitTasks(mfxU32 nMilisecconds)
{
    //check whether some sync points already done
    int k;
    for ( k = 0; k < m_nSyncPoints; k++)
    {
        if (m_pSyncPoints[k].m_SyncPoint == NULL)
            continue;

        if (MFX_ERR_NONE != m_encoder->SyncOperation( m_pSyncPoints[k].m_SyncPoint, 0))
        {
            //not completed syncpoint, lets wait it 
            MFX_CHECK_STS_SKIP(m_encoder->SyncOperation( m_pSyncPoints[k].m_SyncPoint, nMilisecconds), MFX_WRN_IN_EXECUTION);
            return MFX_ERR_NONE;
        }
    }

    return MFXFileWriteRender::WaitTasks(nMilisecconds);
}

mfxStatus MFXEncodeWRAPPER::InitBitstream(mfxBitstream *bs)
{
    memset(bs, 0, sizeof(mfxBitstream));   // zero memory

    //prepare buffer
    mfxU64 nBufferSize = (mfxU64 )m_VideoParams.mfx.BufferSizeInKB * 1000 * 
        (0 == m_VideoParams.mfx.BRCParamMultiplier ? 1 : m_VideoParams.mfx.BRCParamMultiplier);
    MFX_CHECK_WITH_ERR(bs->Data = new mfxU8[nBufferSize], MFX_ERR_MEMORY_ALLOC);
    bs->MaxLength = nBufferSize;
    return MFX_ERR_NONE;
}

mfxStatus MFXEncodeWRAPPER::RenderFrame(mfxFrameSurface1 *pSurf, mfxEncodeCtrl * pCtrl)
{
    mfxStatus sts  = MFX_ERR_NONE;
    mfxStatus sts2 = MFX_ERR_NONE;
    bool      bForceWait = false; //flag is used in case of EOS to reduce number of cyclings

    if (0 == m_nSyncPoints)
    {
        MFX_CHECK_STS(SetOutBitstreamsNum(IPP_MAX(m_refParams.m_nMaxAsync, 1)));
    }

    //TODO: temporary solution to avoid every frame info retrieving from special file
    //TODO: if FAIL add support for dec enc suites
    /*if (NULL != pSurf && 0 != m_refParams.m_params.mfx.FrameInfo.PicStruct)
    {
        pSurf->Info.PicStruct = m_refParams.m_params.mfx.FrameInfo.PicStruct;
    }*/

    for (;;)
    {
        mfxU32 i, j, k;
        
        //find first used sync point
        for (i = 0; i < m_nSyncPoints; i++)
        {
            if (m_pSyncPoints[i].m_SyncPoint != NULL)
                break;
        }

        //have found something used
        if ( i != m_nSyncPoints)
        {
            //find first free sync point
            for (k = 0; k < m_nSyncPoints; k++, i = (i + 1) % m_nSyncPoints )
            {
                if (m_pSyncPoints[i].m_SyncPoint == NULL)
                    break;
            }
            
            mfxSyncPoint *freeSync = &m_pSyncPoints[(i + 1) % m_nSyncPoints].m_SyncPoint;
            
            //no free sync points, have to wait one
            if (*freeSync != NULL)
            {
                sts = m_encoder->SyncOperation( *freeSync, TimeoutVal<>::val());
                MFX_CHECK_STS(sts);
            }

            sts2 = MFX_ERR_NONE;
            //counting number of used syncpoints 
            mfxU32 nUsedSynchPoints = 0;
            for (int u = 0; u < m_nSyncPoints; u++)
            if (NULL != m_pSyncPoints[u].m_SyncPoint)
            {
                nUsedSynchPoints++;
            }

            //check whether some sync points already done
            for (j = i, k = 0; k < m_nSyncPoints; k++, j = (j + 1) % m_nSyncPoints)
            {
                if (m_pSyncPoints[j].m_SyncPoint == NULL)
                    continue;

                if (MFX_ERR_NONE == (sts2 = m_encoder->SyncOperation( m_pSyncPoints[j].m_SyncPoint
                                                                    , bForceWait ? TimeoutVal<>::val() : 0)))
                {
                    //maxasync queue size logging
                    m_refParams.m_uiMaxAsyncReached = (std::max)(m_refParams.m_uiMaxAsyncReached, nUsedSynchPoints);
                    m_refParams.m_fAverageAsync = 
                        ((m_refParams.m_fAverageAsync * m_nFrames) + nUsedSynchPoints) / (m_nFrames + 1);
                    
                    nUsedSynchPoints--;

                    m_pSyncPoints[j].m_SyncPoint = 0;
                    

                    if (m_ExtraParams.bVerbose)
                    {
                        static char chFrameType[] = "?IP?B???";

                        printf(" FrameType %c, size = %d\n",
                            chFrameType[m_pSyncPoints[j].m_pBitstream->FrameType & 7],
                            m_pSyncPoints[j].m_pBitstream->DataLength);
                    }

                    // PutData of encoded frame
                    MFX_CHECK_STS(PutBsData( m_pSyncPoints[j].m_pBitstream ));
                                        
                    //incrementing num frames counter
                    MFXVideoRender::RenderFrame(pSurf, pCtrl);
                    
                    PipelineTraceEncFrame(m_pSyncPoints[j].m_pBitstream->FrameType);

                    // need to clear
                    m_pSyncPoints[j].m_pBitstream->DataLength = 0;
                    m_pSyncPoints[j].m_pBitstream->DataFlag   = 0;
                    m_pSyncPoints[j].m_pBitstream->DataOffset = 0;

                    //limit of surface already written - it is controled by number frames decoded
                    //if(m_ExtraParams.nNumFrames != 0 && m_nFrames >= m_ExtraParams.nNumFrames)
                        //return MFX_ERR_NONE;

                }else
                {
                    if (bForceWait)
                    {
                        MFX_CHECK_STS(sts2);
                    }
                    //found first in use
                    if (MFX_WRN_IN_EXECUTION == sts2 )
                    {
                        break;
                    }
                    MFX_CHECK_STS(sts2);
                }
            }

            if (MFX_ERR_MORE_DATA == sts2)
            {
                break;
            }

        }else
        {
            i = 0;
            //encoder gives us EOS, and all data encoded
            if (sts != MFX_ERR_NONE)
            {
                //delivering NULL bs to downstream writer
                MFX_CHECK_STS(PutData(NULL, 0));
                break;
            }
        }

        //encoder or sync previous error - means EOS
        if (sts != MFX_ERR_NONE && sts != MFX_ERR_MORE_BITSTREAM)
        {
            continue;
        }
        
        //checking exiting condition before running encode - - it is controled by number frames decoded
#if 0
        if (m_ExtraParams.nNumFrames != 0)
        {
            if(m_nFrames >= m_ExtraParams.nNumFrames)
                break;
            
            mfxU32 nQueuedFrames = 0;
            
            for (j = 0; j < m_nSyncPoints; j++)
                if(NULL != m_pSyncPoints[j].m_SyncPoint) 
                    nQueuedFrames++ ;
            
            if(m_nFrames + nQueuedFrames >= m_ExtraParams.nNumFrames)
            {
                if (nQueuedFrames == 0)
                {
                    break;
                }
                bForceWait = true;
                continue;
            }
        }
#endif

        //Assign bitstream from pool
        if (m_pSyncPoints[i].m_pBitstream == NULL)
        {
            for (j = 0; j < m_nSyncPoints; j++)
            {
                if(NULL == m_pSyncPoints[j].m_SyncPoint && 
                   NULL != m_pSyncPoints[j].m_pBitstream)
                {
                    break;
                }
            }

            if (j == m_nSyncPoints)
            {
                //no free bitstream found creating new
                std::auto_ptr<mfxBitstream> pNewBitstream;
                MFX_CHECK_WITH_ERR((pNewBitstream.reset(new mfxBitstream), NULL != pNewBitstream.get()), MFX_ERR_MEMORY_ALLOC);

                MFX_CHECK_STS(InitBitstream(pNewBitstream.get()));

                m_pSyncPoints[i].m_pBitstream = pNewBitstream.release();
            }else
            {
                //reassign free bit stream
                m_pSyncPoints[i].m_pBitstream = m_pSyncPoints[j].m_pBitstream;
                //mark bit stream as used
                m_pSyncPoints[j].m_pBitstream = NULL;
            }
        }
    
        MFX_CHECK_STS(ZeroBottomStripe(pSurf));

        
        Timeout<5> enc_timeout;

        for (;;)
        {
            if (m_ExtraParams.nDelayOnMSDKCalls != 0)
            {
                MPA_TRACE("Sleep(nDelayOnMSDKCalls)");
                vm_time_sleep(m_ExtraParams.nDelayOnMSDKCalls);
            }

            //timer valid value only for async 1 mode
            m_encTime.Start();
            
            m_pSyncPoints[i].m_pCtrl = pCtrl;
            m_pSyncPoints[i].m_bCtrlAllocated = false;
            sts = m_encoder->EncodeFrameAsync(m_pSyncPoints[i].m_pCtrl, pSurf, m_pSyncPoints[i].m_pBitstream, &m_pSyncPoints[i].m_SyncPoint);

            if (MFX_WRN_DEVICE_BUSY == sts)
            {
                m_encTime.Stop();

                //lets wait for existed task 
                mfxStatus res ;
                {
                    MPA_TRACE("WaitTasks");
                    res = WaitTasks(enc_timeout.interval);
                }

                MFX_CHECK_STS_SKIP( res, MFX_WRN_IN_EXECUTION );

                if (MFX_WRN_IN_EXECUTION == res)
                {
                    enc_timeout.wait("WaitEncodeComplete");
                }else
                {
                    enc_timeout.wait_0();
                }

                MFX_CHECK_WITH_ERR(!enc_timeout, MFX_WRN_DEVICE_BUSY);
                continue;
            }
            
            break;
        }

        //in case of -async 1 we have to do sync immediately before return
        //it affects only transcode/time so all errors handling will be done, in next call to renderframe
        if (1 == m_refParams.m_nMaxAsync && NULL != m_pSyncPoints[i].m_SyncPoint)
        {
            m_encoder->SyncOperation( m_pSyncPoints[i].m_SyncPoint, TimeoutVal<>::val());
        }

        m_encTime.Stop();

        MFX_CHECK_STS_SKIP(sts, MFX_ERR_MORE_DATA, MFX_ERR_MORE_BITSTREAM);

        if (pSurf != NULL)
        {
            break;
        }
        
        if (MFX_ERR_MORE_DATA == sts)
        {
            bForceWait = true;
        }
    }
    
    return MFX_ERR_NONE;
}

mfxStatus MFXEncodeWRAPPER::ZeroBottomStripe(mfxFrameSurface1 * psrf)
{
    //FOURCC == 0 for opaque surface
    if (NULL == psrf || 0 == psrf->Info.FourCC)
        return MFX_ERR_NONE;
    
    //zeroing down border
    if (((psrf->Info.CropY + psrf->Info.CropH != psrf->Info.Height) || 
         (psrf->Info.CropX + psrf->Info.CropW != psrf->Info.Width)) && m_ExtraParams.bZeroBottomStripe )
    {
        //only nv12 zeroing so far
        MFX_CHECK(MFX_FOURCC_NV12 == psrf->Info.FourCC);
        MFX_CHECK_STS(LockFrame(psrf));

        mfxU32 pitch = psrf->Data.PitchLow + ((mfxU32)psrf->Data.PitchHigh << 16);
        
        //zeroing bottom stripe
        if (psrf->Info.CropY + psrf->Info.CropH != psrf->Info.Height)
        {
            mfxU32 offsetY   = (psrf->Info.CropY + psrf->Info.CropH);
            mfxU32 planesize = psrf->Info.Height * pitch;

            void *pdata    = psrf->Data.Y + offsetY * pitch;

            memset(pdata, 0, planesize - offsetY * pitch);

            offsetY >>= 1;
            planesize = (psrf->Info.Height >> 1)* pitch;
            pdata     = psrf->Data.U + offsetY  * pitch;

            memset(pdata, 0, planesize - offsetY * pitch);
        }

        //zeroing top stripe
        if (psrf->Info.CropY != 0)
        {
            memset(psrf->Data.Y, 0, pitch * psrf->Info.CropY);
            memset(psrf->Data.U, 0, pitch * (psrf->Info.CropY >> 1));
        }
        
        //zeroing right stripe
        if (psrf->Info.CropX + psrf->Info.CropW != psrf->Info.Width)
        {
            void *pdata  = psrf->Data.Y + psrf->Info.CropX + psrf->Info.CropW + psrf->Info.CropY * pitch;

            IppiSize roi = {psrf->Info.Width - (psrf->Info.CropX + psrf->Info.CropW), psrf->Info.CropH};

            ippiSet_8u_C1R(0, (Ipp8u*)pdata, pitch, roi);

            pdata        = psrf->Data.U + psrf->Info.CropX + psrf->Info.CropW + (psrf->Info.CropY >> 1) * pitch;
            roi.height >>=1;

            ippiSet_8u_C1R(0, (Ipp8u*)pdata, pitch, roi);
        }

        //zeroing left stripe
        if (psrf->Info.CropX != 0)
        {
            void *pdata  = psrf->Data.Y + psrf->Info.CropY * pitch;

            IppiSize roi = {psrf->Info.CropX, psrf->Info.CropH};

            ippiSet_8u_C1R(0, (Ipp8u*)pdata, pitch, roi);

            pdata       = psrf->Data.U + (psrf->Info.CropY >> 1) * pitch;
            roi.height>>=1;

            ippiSet_8u_C1R(0, (Ipp8u*)pdata, pitch, roi);
        }

        MFX_CHECK_STS(UnlockFrame(psrf));
    }
    
    return MFX_ERR_NONE;
}

mfxStatus MFXEncodeWRAPPER::GetHandle(mfxHandleType type, mfxHDL *pHdl)
{
    UNREFERENCED_PARAMETER(pHdl);
    UNREFERENCED_PARAMETER(type);
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXEncodeWRAPPER::SetExtraParams(EncodeExtraParams *pExtraParams)
{
    if (NULL == pExtraParams)
        return MFX_ERR_NULL_PTR;

    m_ExtraParams = *pExtraParams;

    if (vm_string_strlen(m_ExtraParams.pRefFile) != 0)
    {
        MFX_CHECK_VM_FOPEN(m_pRefFile, m_ExtraParams.pRefFile, VM_STRING("rb"));
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXEncodeWRAPPER::PutBsData(mfxBitstream* pData)
{
    MFX_CHECK_STS(MFXFileWriteRender::PutBsData(pData));
    if (NULL == pData)
        return MFX_ERR_NONE;

    m_EncodedSize += pData->DataLength;

    if (NULL != m_pRefFile)
    {
        if (NULL == m_pCacheBuffer)
        {
            m_nCacheBufferSize = MAX_CACHED_BUF_LEN;
            m_pCacheBuffer = new mfxU8[m_nCacheBufferSize];
            MFX_CHECK_POINTER(m_pCacheBuffer);
        }
        mfxU32 nSize   = pData->DataLength;
        mfxU8* pBuffer = pData->Data + pData->DataOffset;
        for(;0 < (std::min)(nSize, m_nCacheBufferSize);)
        {
            mfxU32 nReaded = (std::min)(nSize, m_nCacheBufferSize);
            MFX_CHECK(nReaded == (mfxU32)vm_file_fread(m_pCacheBuffer, 1, nReaded, m_pRefFile));
            MFX_CHECK(0 == memcmp(m_pCacheBuffer, pBuffer, nReaded));
            pBuffer += nReaded;
            nSize -= nReaded;
        }
    }

    return MFX_ERR_NONE;
};

