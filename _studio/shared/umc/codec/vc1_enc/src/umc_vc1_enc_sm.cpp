//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#include "umc_vc1_enc_sm.h"
#include "umc_structures.h"
#include "umc_video_data.h"
#include "ippi.h"
#include "umc_vc1_enc_debug.h"
#include "umc_video_data_scene_info.h"
#include "umc_vc1_enc_tables.h"

namespace UMC_VC1_ENCODER
{

    UMC::Status  VC1EncoderSM::Init(UMC::VC1EncoderParams* pParams)
    {
        UMC::Status     err = UMC::UMC_OK;
        Ipp32u          w=0, h=0;
        Ipp32s          memSize = 0;

        bool            bNV12 = false;

        if(!m_pMemoryAllocator)
            return UMC::UMC_ERR_INIT;

        Close();

        m_bMixedMV              =  pParams->m_bMixed;
        m_uiGOPLength           = (pParams->m_uiGOPLength!=0)? pParams->m_uiGOPLength:200;
        m_uiBFrmLength          = (pParams->m_uiBFrmLength < m_uiGOPLength)? pParams->m_uiBFrmLength:m_uiGOPLength-1;

        m_uiBFrmLength          = (m_uiBFrmLength > 7) ? 7 : m_uiBFrmLength;

        m_bSequenceHeader       = false;
        m_iFrameCount           = 0;

        m_uiPictuteQuantIndex   = 7;
        m_bHalfQuant            = false;
        m_bFrameRecoding        = pParams->m_bFrameRecoding;

        pParams->m_bFastUVMC    = false;

        m_bFastUVMC             = pParams->m_bFastUVMC;
        m_bIntensityCompensation= pParams->m_bIntensityCompensation;
        bNV12                   = pParams->m_bNV12;
        

        m_uiOrigFramesUsingFlag    = (pParams->m_bOrigFramePred) ? VC1_ENC_ORIG_FRAMES_FOR_ME|VC1_ENC_ORIG_FRAMES_FOR_SKIP : 0; //  0 - Original (non-reconstructed frames aren't  used)
        //  (m_uiOrigFramesUsingFlag & VC1_ENC_ORIG_FRAMES_FOR_ME): originalare used for ME
        //  (m_uiOrigFramesUsingFlag & VC1_ENC_ORIG_FRAMES_FOR_SKIP): originalare used for skip frames detection

        if(pParams->m_bUseUpdateMeFeedback)
            m_uiOrigFramesUsingFlag |= VC1_ENC_ORIG_FRAMES_FOR_STAT;

        m_SH = new VC1EncoderSequenceSM;
        if (!m_SH)
            return UMC::UMC_ERR_ALLOC;

        err = m_SH->Init(pParams);
        if (err != UMC::UMC_OK)
            return err;

        err = m_SH->CheckParameters(m_cLastError);
        if (err != UMC::UMC_OK)
            return err;

        w =  m_SH->GetNumMBInRow();
        h =  m_SH->GetNumMBInCol();
        //------------Planes for decoding -------------------------------

        m_pStoredFrames  =  new StoredFrames;
        if (!m_pStoredFrames)
            return UMC::UMC_ERR_ALLOC;

        m_pGOP = new GOP;
        if (!m_pGOP)
            return UMC::UMC_ERR_ALLOC;

        m_pWaitingList = new WaitingList;
        if (!m_pWaitingList)
            return UMC::UMC_ERR_ALLOC;

        memSize = StoredFrames::CalcAllocatedMemSize(m_uiBFrmLength +((m_uiOrigFramesUsingFlag)?4:2),
                                                     pParams->info.clip_info.width,pParams->info.clip_info.height, 32, bNV12);

        if(m_pMemoryAllocator->Alloc(&m_StoredFramesID, memSize,
                                     UMC::UMC_ALLOC_PERSISTENT, 16) != UMC::UMC_OK )
                                     return UMC::UMC_ERR_ALLOC;

        m_pStoredFramesBuffer = (Ipp8u*)m_pMemoryAllocator->Lock(m_StoredFramesID);

        err = m_pStoredFrames->Init(m_pStoredFramesBuffer, memSize,
                                    m_uiBFrmLength +((m_uiOrigFramesUsingFlag)?4:2),
                                    pParams->info.clip_info.width,pParams->info.clip_info.height, 32, bNV12);
        if (err !=UMC::UMC_OK)
            return err;

        err =m_pGOP->Init(m_uiBFrmLength);
        if (err !=UMC::UMC_OK)
            return err;

        err =m_pWaitingList->Init(m_uiBFrmLength);
        if (err !=UMC::UMC_OK)
            return err;

        if (m_uiBFrmLength)
        {
            /* we should save MV for direct prediction in B frames*/
            if(m_pMemoryAllocator->Alloc(&m_SavedMVID, w*h*2*sizeof(Ipp16s),
                UMC::UMC_ALLOC_PERSISTENT, 16) != UMC::UMC_OK )
                return UMC::UMC_ERR_ALLOC;
            m_pSavedMV = (Ipp16s*)m_pMemoryAllocator->Lock(m_SavedMVID);

        }

        //---------------MBs-----------------------------------------------

        m_pMBs = new VC1EncoderMBs;
        if (!m_pMBs)
            return UMC::UMC_ERR_ALLOC;

        memSize = VC1EncoderMBs::CalcAllocMemorySize(w,2,bNV12);
        if(m_pMemoryAllocator->Alloc(&m_MBsID, memSize,UMC::UMC_ALLOC_PERSISTENT, 16) != UMC::UMC_OK )
            return UMC::UMC_ERR_ALLOC;
        m_MBsBuffer = (Ipp8u*)m_pMemoryAllocator->Lock(m_MBsID);

        err = m_pMBs->Init(m_MBsBuffer, memSize, w, 2, bNV12);
        if (err != UMC::UMC_OK)
            return err;

        m_pCodedMB =  new VC1EncoderCodedMB [w*h];
        if (!m_pCodedMB)
            return UMC::UMC_ERR_ALLOC;

        m_pCurrPicture = new VC1EncoderPictureSM (bNV12);
        if (!m_pCurrPicture)
            return UMC::UMC_ERR_ALLOC;

        m_pCurrPicture->Init(m_SH, m_pMBs, m_pCodedMB);

        //---------------Motion estimation
        m_bUseMeFeedback = pParams->m_bUseMeFeedback;
        m_bUseUpdateMeFeedback = pParams->m_bUseUpdateMeFeedback;
        m_bUseFastMeFeedback   = pParams->m_bUseFastMeFeedback;

        m_bSelectVLCTables         = pParams->m_bSelectVLCTables;
        m_bChangeInterpPixelType   = pParams->m_bChangeInterpPixelType;
        m_bUseTreillisQuantization = pParams->m_bUseTreillisQuantization;

        {
            UMC::MeInitParams MEParamsInit;
            memset(&MEParamsInit,0,sizeof(MEParamsInit));

            MEParamsInit.WidthMB= (pParams->info.clip_info.width+15)/16;
            MEParamsInit.HeightMB= (pParams->info.clip_info.height+15)/16;
            MEParamsInit.refPadding     = 32;
            MEParamsInit.MbPart         = UMC::ME_Mb16x16;
            MEParamsInit.bNV12 = bNV12; 
            //MEParamsInit.CostMetrics    = UMC::ME_Sad;
#ifndef VC1_ME_MB_STATICTICS
            MEParamsInit.UseStatistics   = true;
#endif
            MEParamsInit.UseDownSampling = true;

            if(m_uiBFrmLength)
                MEParamsInit.SearchDirection = UMC::ME_BidirSearch;
            else
                MEParamsInit.SearchDirection = UMC::ME_ForwardSearch;

            MEParamsInit.MaxNumOfFrame = 3;

            m_pME = new UMC::MeVC1;
#ifdef ME_VME
            m_pME = (UMC::MeBase*)new UMC::MeVme;
#endif


            if(!m_pME)
                return err;

            if(!m_pME->Init(&MEParamsInit, NULL, memSize))
                return UMC::UMC_ERR_INIT;
            if(m_pMemoryAllocator->Alloc(&m_MEID, memSize,UMC::UMC_ALLOC_PERSISTENT, 16) != UMC::UMC_OK )
                return UMC::UMC_ERR_ALLOC;
            m_pMEBuffer = (Ipp8u*)m_pMemoryAllocator->Lock(m_MEID);
            if(!m_pME->Init(&MEParamsInit, m_pMEBuffer, memSize))
                return UMC::UMC_ERR_INIT;


            m_MESearchSpeed = pParams->m_uiMESearchSpeed;

            Ipp32s numRefFrame = MEParamsInit.MaxNumOfFrame;
            for(Ipp32s i = 0; i < numRefFrame; i++)
            {
                m_MeFrame[i] = &MEParamsInit.pFrames[i];
            }
        }

        //--------------Bitrate-------------------------------------------

        m_pBitRateControl = new VC1BitRateControl();
        if (!m_pBitRateControl)
            return UMC::UMC_ERR_ALLOC;

        memSize = VC1BitRateControl::CalcAllocMemorySize(m_uiGOPLength, m_uiBFrmLength);

        if(m_pMemoryAllocator->Alloc(&m_BRCID, memSize,
            UMC::UMC_ALLOC_PERSISTENT, 16) != UMC::UMC_OK )
            return UMC::UMC_ERR_ALLOC;

        m_pBRCBuffer = (Ipp8u*)m_pMemoryAllocator->Lock(m_BRCID);

        Ipp8u QuantMode = 3;

        switch (m_SH->GetQuantType())
        {
        case VC1_ENC_QTYPE_IMPL:
            QuantMode = 3;
            break;
        case VC1_ENC_QTYPE_UF:
            QuantMode = 1;
            break;
        case VC1_ENC_QTYPE_NUF:
            QuantMode = 2;
            break;
        }

        err = m_pBitRateControl->Init(m_pBRCBuffer, memSize, (pParams->info.clip_info.height*pParams->info.clip_info.width*3)/2,
            pParams->info.bitrate, pParams->info.framerate, VC1_BRC_HIGHT_QUALITY_MODE,
            m_uiGOPLength, m_uiBFrmLength, pParams->m_iConstQuant, QuantMode);
        if (err != UMC::UMC_OK)
            return err;

        /*if (buffer number < 0) InitBuffer function will set corresponding number and
        initialize it as new buffer
        if it is number of existing buffer, it will change buffer params
        if buffer size <=0 or > maxsize, buffer size will be equal to buffer size*/

        Ipp8u BRCLevel = m_pBitRateControl->GetLevel(m_SH->GetProfile(),pParams->info.bitrate, w, h);
        if(BRCLevel < pParams->level)
            BRCLevel = (Ipp8u)pParams->level;

        m_SH->SetLevel(BRCLevel);

        err = m_pBitRateControl->InitBuffer(m_SH->GetProfile(),m_SH->GetLevel(),
            pParams->m_uiHRDBufferSize, pParams->m_uiHRDBufferInitFullness);
        if (err != UMC::UMC_OK)
            return err;

        //-----------Bitstream ----------------------------------------------------

        m_pCodedFrame = new VC1EncoderBitStreamSM;
        if (!m_pCodedFrame)
            return UMC::UMC_ERR_ALLOC;

#ifdef UMC_ENABLE_UMC_SCENE_ANALYZER
        if(pParams->m_bSceneAnalyzer)
        {
            UMC::SceneAnalyzerParams saParams;
            saParams.m_maxGOPLength = m_uiGOPLength;
            saParams.m_maxBLength   = m_uiBFrmLength;

            if(pParams->m_bInterlace)
                saParams.m_interlaceType = UMC::INTERLEAVED_TOP_FIELD_FIRST;
            else
                saParams.m_interlaceType = UMC::PROGRESSIVE;

            m_pSceneAnalyzer = new UMC::SceneAnalyzer();

            if (!m_pSceneAnalyzer)
                return UMC::UMC_ERR_ALLOC;

            err =  m_pSceneAnalyzer->Init(&saParams);
            if (err != UMC::UMC_OK)
                return err;
        }
#endif

#ifdef VC1_ENC_DEBUG_ON
        pDebug = new VC1EncDebug;
        assert(pDebug != NULL);
        pDebug->Init(w, h, pParams->m_bNV12);
#endif

        IntensityCompChroma = (bNV12)? IntensityCompChromaNV12:IntensityCompChromaYV12;
        return UMC::UMC_OK;
    }
    UMC::Status VC1EncoderSM::Close()
    {
        for(Ipp32u i = 0; i < 32; i++)
            m_MeFrame[i] = NULL;

        m_MeIndex.MeCurrIndex = 0;
        m_MeIndex.MeRefFIndex = 0;
        m_MeIndex.MeRefBIndex = 0;

        ResetPlanes();

        if  (m_SH)
        {
            delete m_SH;
            m_SH = 0;
        }
        if (m_pCurrPicture)
        {
            delete m_pCurrPicture;
            m_pCurrPicture = 0;
        }

        if(m_pCodedFrame)
        {
            delete m_pCodedFrame;
            m_pCodedFrame = NULL;
        }

        if(m_pBitRateControl)
        {
            delete m_pBitRateControl;
            m_pBitRateControl = NULL;
        }

#ifdef UMC_ENABLE_UMC_SCENE_ANALYZER
        if(m_pSceneAnalyzer)
        {
            delete m_pSceneAnalyzer;
            m_pSceneAnalyzer = NULL;
        }
#endif

        if (m_pWaitingList)
        {
            delete m_pWaitingList;
            m_pWaitingList = 0;
        }
        if (m_pGOP)
        {
            delete m_pGOP;
            m_pGOP = 0;
        }

        if (m_pStoredFrames)
        {
            delete m_pStoredFrames;
            m_pStoredFrames = 0;
        }

        if(m_pMemoryAllocator)
        {
            m_pMemoryAllocator->Unlock(m_StoredFramesID);
            m_pMemoryAllocator->Free(m_StoredFramesID);
        }
        m_StoredFramesID = (UMC::MemID)-1;
        m_pStoredFramesBuffer = NULL;

        if(m_pMemoryAllocator)
        {
            m_pMemoryAllocator->Unlock(m_SavedMVID);
            m_pMemoryAllocator->Free(m_SavedMVID);
        }
        m_SavedMVID = (UMC::MemID)-1;
        m_pSavedMV = 0;


        if(m_pMemoryAllocator)
        {
            m_pMemoryAllocator->Unlock(m_MBsID);
            m_pMemoryAllocator->Free(m_MBsID);
        }
        m_MBsID = (UMC::MemID)-1;
        m_MBsBuffer = 0;
        if(m_pMBs)
        {
            delete m_pMBs;
            m_pMBs = NULL;
        }

        if(m_pMemoryAllocator)
        {
            m_pMemoryAllocator->Unlock(m_BRCID);
            m_pMemoryAllocator->Free(m_BRCID);
        }
        m_BRCID = (UMC::MemID)-1;
        m_pBRCBuffer = 0;

        if(m_pCodedMB)
        {
            delete [] m_pCodedMB;
            m_pCodedMB = 0;
        }

        if(m_pMemoryAllocator)
        {
            m_pMemoryAllocator->Unlock(m_MEID);
            m_pMemoryAllocator->Free(m_MEID);
        }
        m_MEID = (UMC::MemID)-1;
        m_pMEBuffer = NULL;

        if(m_pME)
        {
            delete m_pME;
            m_pME = NULL;
        }

        if(m_pBRCBuffer)
        {
            delete m_pBRCBuffer;
            m_pBRCBuffer = NULL;
        }

#ifdef VC1_ENC_DEBUG_ON
        if(pDebug)
        {
            delete pDebug;
            pDebug = NULL;
        }
#endif

        return UMC::UMC_OK;
    }

    Ipp8u     VC1EncoderSM::GetRoundControl(ePType pictureType, Ipp8u roundControl)
    {
        switch (pictureType)
        {
        case VC1_ENC_I_FRAME:
        case VC1_ENC_BI_FRAME:
            roundControl = 1;
            break;
        case VC1_ENC_P_FRAME:
            roundControl = !roundControl;
            break;
        default:
            roundControl =  roundControl;
            break;
        }
        return roundControl;
    }
    UMC::Status VC1EncoderSM::GetFrame(UMC::MediaData *in, UMC::MediaData *out)
    {
        UMC::Status         err             =  UMC::UMC_ERR_NOT_ENOUGH_DATA;
        UMC::VideoData*     pVideoData      = (in)? DynamicCast<UMC::VideoData> (in):0;
        double              time            = 0;
        size_t              seqHeaderLen    = 0;
        Frame*              inFrame         = 0;
        Frame*              outFrame        = 0;
        bool                stored          = false;
        UMC::VideoData      dst; //for scene analyzer

        ePType              inputPictureType = VC1_ENC_I_FRAME;

        //only for debug
        static int z_z = 0;

        ResetPlanes();

#ifdef UMC_ENABLE_UMC_SCENE_ANALYZER
        if(m_pSceneAnalyzer && (pVideoData == NULL || pVideoData->GetFrameType() == UMC::NONE_PICTURE))
        {
            //scene analyzer
            err = m_pSceneAnalyzer->GetFrame(pVideoData, &dst);
            // usual case. we have something to encode
            if (UMC::UMC_OK == err)
            {
                pVideoData      = &dst;
            }
            // it can be the beginning either ending of the stream.
            else if (err == UMC::UMC_ERR_NOT_ENOUGH_DATA)
            {
                if (pVideoData)
                {
                    return err;
                }
            }
            else
            {
                return err;
            }
        }
#endif

        //-------------- try to save input data-------------------------------------------------------------------
        if (pVideoData)
        {
            switch (pVideoData->GetFrameType())
            {
            case UMC::I_PICTURE:
                //printf("I frame\n");
                inputPictureType = VC1_ENC_I_FRAME;
                break;
            case UMC::P_PICTURE:
                //printf("P frame\n");
                inputPictureType = VC1_ENC_P_FRAME;
                break;
            case UMC::B_PICTURE:
                //printf("B frame\n");
                inputPictureType = VC1_ENC_B_FRAME;
                break;
            default:
                inputPictureType = m_pGOP->GetPictureType(m_iFrameCount,m_uiGOPLength,m_uiBFrmLength);
                break;
            };

            m_iFrameCount++;

            inFrame = m_pStoredFrames->GetFreeFramePointer();
            if (inFrame)
            {
                inFrame->CopyPlane( (Ipp8u*)pVideoData->GetPlanePointer(0), (Ipp32u)pVideoData->GetPlanePitch(0),
                    (Ipp8u*)pVideoData->GetPlanePointer(1), (Ipp32u)pVideoData->GetPlanePitch(1),
                    (Ipp8u*)pVideoData->GetPlanePointer(2), (Ipp32u)pVideoData->GetPlanePitch(2),
                    inputPictureType);

                if (!m_pWaitingList->AddFrame(inFrame))
                    return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
                pVideoData->SetDataSize(0);
                stored = true;
            }
        }
        inFrame = 0;
        while ((inFrame = m_pWaitingList->GetCurrFrame())!=0)
        {
            outFrame = (m_uiOrigFramesUsingFlag)?m_pStoredFrames->GetFreeFramePointer():0;
            if (m_pGOP->isSkipFrame(inFrame,(m_uiOrigFramesUsingFlag & VC1_ENC_ORIG_FRAMES_FOR_SKIP)!=0,m_LastQuant))
            {
                inFrame->SetType(VC1_ENC_SKIP_FRAME);
                if (m_uiOrigFramesUsingFlag)
                {
                    outFrame->CopyPlane(inFrame);
                }
            }
            if (!m_pGOP->AddFrame(inFrame,outFrame))
                break;
            m_pWaitingList->MoveOnNextFrame();
        }

        //---------------------------------------------------------------------------------------------------------

        if (!pVideoData)
        {
            m_pGOP->CloseGop();
        }
        inFrame  = m_pGOP->GetInFrameForDecoding();
        outFrame = m_pGOP->GetOutFrameForDecoding();

        if (inFrame && outFrame)
        {
            Frame* pFrameRef = 0;
            UMC::MediaDataUD * pOutUD    = DynamicCast<UMC::MediaDataUD> (out);
            if (pOutUD)
            {
                pOutUD->pictureCode = (Ipp8u)inFrame->GetPictureType();
            }       
            m_pCodedFrame->Init(out);
            if (!m_bSequenceHeader)
            {
                m_SH->SetMaxBFrames(m_uiBFrmLength);

                err = m_SH->WriteSeqHeader(m_pCodedFrame);
                if (err != UMC::UMC_OK)
                    return err;

                m_bSequenceHeader = true;
                seqHeaderLen = m_pCodedFrame->GetDataLen();
            }
            err = m_pCurrPicture->Init(m_SH,m_pMBs,m_pCodedMB);
            if (err != UMC::UMC_OK)
                return err;

#ifdef VC1_BRC_DEBUG
            printf("PicType = %d\n", inFrame->GetPictureType());
#endif

            if (inFrame->GetPictureType() != VC1_ENC_SKIP_FRAME)
            {
                m_pPlane = inFrame;

                if (inFrame->GetPictureType()!= VC1_ENC_B_FRAME && m_uiGOPLength>1)
                {
                    m_pRaisedPlane = outFrame;
                }
                if (inFrame->GetPictureType() != VC1_ENC_I_FRAME)
                {
                    pFrameRef = m_pGOP->GetOutReferenceFrame();
                    if (!pFrameRef)
                        return UMC::UMC_ERR_FAILED;

                    m_pForwardPlane = pFrameRef;
                    if (m_uiOrigFramesUsingFlag & VC1_ENC_ORIG_FRAMES_FOR_ME)
                    {
                        pFrameRef = m_pGOP->GetInReferenceFrame();
                    }

                    m_pForwardMEPlane = pFrameRef;
                }
                if (inFrame->GetPictureType() == VC1_ENC_B_FRAME)
                {
                    pFrameRef = m_pGOP->GetOutReferenceFrame(true);
                    if (!pFrameRef)
                        return UMC::UMC_ERR_FAILED;

                    m_pBackwardPlane = pFrameRef;

                    if (m_uiOrigFramesUsingFlag & VC1_ENC_ORIG_FRAMES_FOR_ME)
                    {
                        pFrameRef = m_pGOP->GetInReferenceFrame(true);
                    }

                    m_pBackwardMEPlane = pFrameRef;
                    m_MeIndex.MeCurrIndex = 2;
                }
                else
                {
                    m_MeIndex.MeRefBIndex = m_MeIndex.MeRefFIndex;
                    m_MeIndex.MeCurrIndex = m_MeIndex.MeRefFIndex;
                    m_MeIndex.MeRefFIndex = 1 - m_MeIndex.MeRefFIndex;
                }

                m_uiRoundControl= GetRoundControl (inFrame->GetPictureType(),m_uiRoundControl);
                m_pCurrPicture->SetRoundControl(m_uiRoundControl);

            }

            Ipp32u CodedSize = m_pCodedFrame->GetDataLen();

            err = WriteFrame(inFrame->GetPictureType(), CodedSize);
            if (err != UMC::UMC_OK)
                return err;
            err = m_pCurrPicture->CompletePicture(m_pCodedFrame,time,seqHeaderLen);
            if (err != UMC::UMC_OK)
                return err;

#ifdef VC1_ENC_DEBUG_ON
            //debug
            pDebug->SetPicType(inFrame->GetPictureType());
            pDebug->SetFrameSize(m_pCodedFrame->GetDataLen() - CodedSize);
            pDebug->WriteFrameInfo();
#endif

            err = m_pCodedFrame->DataComplete(out);
            if (err != UMC::UMC_OK)
                return err;

            if (inFrame->GetPictureType() != VC1_ENC_B_FRAME && m_uiGOPLength>1)
            {
                err = outFrame->PadFrameProgressive();
                if (err != UMC::UMC_OK)
                    return err;
                if (m_uiOrigFramesUsingFlag)
                {
                    err = inFrame->PadFrameProgressive();
                    if (err != UMC::UMC_OK)
                        return err;
                }
            }

            m_pGOP->ReleaseCurrentFrame();
        }
        else if (!pVideoData)
        {
            return UMC::UMC_ERR_END_OF_STREAM;
        }

        //-------------- try to save input data-------------------------------------------------------------------
        if (pVideoData && !stored)
        {
            inFrame = m_pStoredFrames->GetFreeFramePointer();
            if (!inFrame)
                return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

            inFrame->CopyPlane( (Ipp8u*)pVideoData->GetPlanePointer(0), (Ipp32u)pVideoData->GetPlanePitch(0),
                (Ipp8u*)pVideoData->GetPlanePointer(1), (Ipp32u)pVideoData->GetPlanePitch(1),
                (Ipp8u*)pVideoData->GetPlanePointer(2), (Ipp32u)pVideoData->GetPlanePitch(2),
                inputPictureType);

            if (!m_pWaitingList->AddFrame(inFrame))
                return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
            pVideoData->SetDataSize(0);
        }
        while ((inFrame = m_pWaitingList->GetCurrFrame())!=0)
        {
            if (m_pGOP->isSkipFrame(inFrame,(m_uiOrigFramesUsingFlag & VC1_ENC_ORIG_FRAMES_FOR_SKIP)!=0,m_LastQuant))
            {
                inFrame->SetType(VC1_ENC_SKIP_FRAME);
                if (m_uiOrigFramesUsingFlag)
                {
                    outFrame->CopyPlane(inFrame);
                }
            }
            outFrame = (m_uiOrigFramesUsingFlag)? m_pStoredFrames->GetFreeFramePointer():0;
            if (!m_pGOP->AddFrame(inFrame,outFrame ))
                break;
            m_pWaitingList->MoveOnNextFrame();
        }
        //---------------------------------------------------------------------------------------------------------

        return err;
    }

    UMC::Status VC1EncoderSM::WriteFrame(ePType InputPictureType, Ipp32u CodedSize)
    {
        UMC::Status   err = UMC::UMC_OK;
        UMC::MeParams MEParams;
        Ipp8u uiQuantIndex = 31;
        bool  bHalfQuant = 0;
        bool  bUniform   = true;
        Ipp32s RecodedFrameNum = 0;
        bool   frame_recoding  = true;
        InitPictureParams  param;

        err = m_pCurrPicture->SetPictureParams(InputPictureType, m_pSavedMV);
        VC1_ENC_CHECK(err);

        m_pCurrPicture->SetIntesityCompensationParameters(false,0,0); 

        //inialize picture params
        SetInitPictureParams(&param,InputPictureType);

        if (m_bIntensityCompensation && InputPictureType == VC1_ENC_P_FRAME )
        {
            IppiSize            YSize      =  {m_SH->GetPictureWidth(), m_SH->GetPictureHeight()};
            IppiSize            UVSize      = {YSize.width>>1, YSize.height>>1};

            Ipp8u*              pSrcY = 0;        
            Ipp8u*              pSrcU = 0;
            Ipp8u*              pSrcV = {0};

            Ipp8u*              pRefY = 0;
            Ipp8u*              pRefU = 0;
            Ipp8u*              pRefV = 0;

            Ipp32u              srcYStep   = 0;
            Ipp32u              srcUVStep  = 0;

            Ipp32u              refYStep  = {0};
            Ipp32u              refUVStep = {0};


            IppiSize            blockSize       = {3*16,3*16};
            CorrelationParams   sCorrParams     = {0};
            Ipp32u              LUMSCALE        = 0;
            Ipp32u              LUMSHIFT        = 0;
            Ipp32s              iScale = 0;
            Ipp32s              iShift = 0;
            Ipp32s              LUTY[257] ;
            Ipp32s              LUTUV[257];

            m_pPlane->GetPlane_Prog(pSrcY, srcYStep, pSrcU, pSrcV, srcUVStep);
            m_pForwardPlane->GetPlane_Prog(pRefY, refYStep, pRefU, pRefV, refUVStep);

            CalculateCorrelationBlock_8u_P1R (pRefY ,refYStep,pSrcY, srcYStep,                                          
                YSize, blockSize,&sCorrParams);

            if (CalculateIntesityCompensationParams (sCorrParams,LUMSCALE,LUMSHIFT))
            {

                m_pCurrPicture->SetIntesityCompensationParameters(true,LUMSCALE,LUMSHIFT); 

                get_iScale_iShift (LUMSCALE, LUMSHIFT, iScale, iShift); 
                CreateICTable (iScale, iShift, LUTY, LUTUV);  

                ippiLUT_8u_C1IR(pRefY,refYStep,YSize,LUTY,intesityCompensationTbl,257);
                IntensityCompChroma(pRefU,pRefV,refUVStep,UVSize,LUTUV,intesityCompensationTbl);
                m_pForwardPlane->PadFrameProgressive();

                if (m_pForwardMEPlane && m_pForwardMEPlane->GetYPlane() != 0 &&
                    m_pForwardMEPlane->GetYPlane() != m_pForwardPlane->GetYPlane())
                {
                    m_pForwardPlane->GetPlane_Prog(pRefY, refYStep, pRefU, pRefV, refUVStep);
                    ippiLUT_8u_C1IR(pRefY,refYStep,YSize,LUTY,intesityCompensationTbl,257);
                    IntensityCompChroma(pRefU,pRefV,refUVStep,UVSize,LUTUV,intesityCompensationTbl);
                    m_pForwardMEPlane->PadFrameProgressive();
                } 
            }         
        }
        while(frame_recoding && (RecodedFrameNum < VC1_ENC_RECODING_MAX_NUM))
        {

            //-----BRC---------------
            m_pBitRateControl->GetQuant(InputPictureType, &uiQuantIndex, &bHalfQuant, &bUniform);
            err = m_pCurrPicture->SetPictureQuantParams(uiQuantIndex, bHalfQuant);
            if (err != UMC::UMC_OK)
                return err;

            STATISTICS_START_TIME(m_TStat->me_StartTime);
            err = SetMEParams(&MEParams, &param ,uiQuantIndex*2 + bHalfQuant, bUniform);
            VC1_ENC_CHECK(err);

            //----ME---------------
            switch (InputPictureType)
            {
            case VC1_ENC_I_FRAME:

                err = SetMEParams_I(&MEParams);
                VC1_ENC_CHECK(err);

                err = Enc_Frame(m_pME, &MEParams);
                VC1_ENC_CHECK(err);
                break;
            case VC1_ENC_P_FRAME:
                if (!param.bMVMixed)
                {
                    err = SetMEParams_P(&MEParams);
                    VC1_ENC_CHECK(err);

                    err = Enc_Frame(m_pME, &MEParams);
                    VC1_ENC_CHECK(err);
                    break;
                }
                else
                {        
                    err = SetMEParams_PMixed(&MEParams);
                    VC1_ENC_CHECK(err);

                    err = Enc_Frame(m_pME, &MEParams);
                    VC1_ENC_CHECK(err);
                }
                break;
            case VC1_ENC_B_FRAME:
                err = SetMEParams_B(&MEParams,&param.uiBFraction);
                VC1_ENC_CHECK(err);

                err = Enc_Frame(m_pME, &MEParams);
                VC1_ENC_CHECK(err);

                break;
            case VC1_ENC_SKIP_FRAME:
                break;
            default:
                return UMC::UMC_ERR_FAILED;
            }
            STATISTICS_END_TIME(m_TStat->me_StartTime, m_TStat->me_EndTime, m_TStat->me_TotalTime);

            err = MEPictureParamsRefine(&MEParams,&param);
            VC1_ENC_CHECK(err);

            //set picture params
            m_pCurrPicture->SetInitPictureParams(&param);

            err = m_pCurrPicture->CheckParameters(m_cLastError);
            VC1_ENC_CHECK(err);

            //----PAC---------------
#ifdef VC1_ME_MB_STATICTICS
            m_pCurrPicture->SetMEStat(MEParams.pSrc->stat);
#endif
            switch (InputPictureType)
            {
            case VC1_ENC_I_FRAME:
                {
                    err = m_pCurrPicture->SetPlaneParams (m_pPlane, VC1_ENC_CURR_PLANE);
                    VC1_ENC_CHECK(err);

                    if(m_uiGOPLength > 1)
                    {
                        err = m_pCurrPicture->SetPlaneParams  (m_pRaisedPlane, VC1_ENC_RAISED_PLANE);
                        VC1_ENC_CHECK(err);
                    }

                    //----coding---------------
                    err = m_pCurrPicture->PAC_IFrame(&MEParams);
                    VC1_ENC_CHECK(err);

                    //----VLC---------------
                    err = m_pCurrPicture->VLC_IFrame(m_pCodedFrame);
                    VC1_ENC_CHECK(err);
                }
                break;
            case VC1_ENC_P_FRAME:
                {
                    err = m_pCurrPicture->SetPlaneParams (m_pPlane, VC1_ENC_CURR_PLANE);
                    VC1_ENC_CHECK(err);

                    err = m_pCurrPicture->SetPlaneParams  (m_pRaisedPlane, VC1_ENC_RAISED_PLANE);
                    VC1_ENC_CHECK(err);

                    err = m_pCurrPicture->SetPlaneParams  (m_pForwardPlane, VC1_ENC_FORWARD_PLANE);
                    VC1_ENC_CHECK(err);

                    if (!param.bMVMixed)
                    {
                        //----coding---------------
                        err = m_pCurrPicture->PAC_PFrame(&MEParams);
                        VC1_ENC_CHECK(err);

                        //----VLC---------------
                        err = m_pCurrPicture->VLC_PFrame(m_pCodedFrame);
                        VC1_ENC_CHECK(err);
                    }
                    else
                    {
                        //----coding---------------
                        err = m_pCurrPicture->PAC_PFrameMixed(&MEParams);
                        VC1_ENC_CHECK(err);
                        //----VLC---------------
                        err = m_pCurrPicture->VLC_PFrameMixed(m_pCodedFrame);
                        VC1_ENC_CHECK(err);
                    }
                }
                break;
            case VC1_ENC_B_FRAME:
                {
                    err = m_pCurrPicture->SetPlaneParams (m_pPlane, VC1_ENC_CURR_PLANE);
                    VC1_ENC_CHECK(err);

                    err = m_pCurrPicture->SetPlaneParams  (m_pForwardPlane, VC1_ENC_FORWARD_PLANE);
                    VC1_ENC_CHECK(err);

                    err = m_pCurrPicture->SetPlaneParams  (m_pBackwardPlane, VC1_ENC_BACKWARD_PLANE);
                    VC1_ENC_CHECK(err);

                    //----coding---------------
                    err = m_pCurrPicture->PAC_BFrame(&MEParams);
                    VC1_ENC_CHECK(err);

                    //----VLC---------------
                    err = m_pCurrPicture->VLC_BFrame(m_pCodedFrame);
                    VC1_ENC_CHECK(err);
                }
                break;
            case VC1_ENC_SKIP_FRAME:
                err = m_pCurrPicture->VLC_SkipFrame(m_pCodedFrame);
                VC1_ENC_CHECK(err);
                break;
            default:
                return UMC::UMC_ERR_FAILED;

            }

            //--------ME-UPDATE---------------
#ifdef VC1_ME_MB_STATICTICS
            if(m_pPlane)
            {
                MEParams.pSrc->SrcPlane.ptr[0]  = m_pPlane->GetYPlane();
                MEParams.pSrc->SrcPlane.step[0] = m_pPlane->GetYStep();
                MEParams.pSrc->SrcPlane.ptr[1]  = m_pPlane->GetUPlane();
                MEParams.pSrc->SrcPlane.step[1] = m_pPlane->GetUStep();
                MEParams.pSrc->SrcPlane.ptr[2]  = m_pPlane->GetVPlane();
                MEParams.pSrc->SrcPlane.step[2] = m_pPlane->GetVStep();

            }
            else
            {
                MEParams.pSrc->SrcPlane.ptr[0]  = NULL;
                MEParams.pSrc->SrcPlane.step[0] = NULL;
                MEParams.pSrc->SrcPlane.ptr[1]  = NULL;
                MEParams.pSrc->SrcPlane.step[1] = NULL;
                MEParams.pSrc->SrcPlane.ptr[2]  = NULL;
                MEParams.pSrc->SrcPlane.step[2] = NULL;
            }

            if(m_pRaisedPlane)
            {
                MEParams.pSrc->RecPlane.ptr[0]  = m_pRaisedPlane->GetYPlane();
                MEParams.pSrc->RecPlane.step[0] = m_pRaisedPlane->GetYStep();
                MEParams.pSrc->RecPlane.ptr[1]  = m_pRaisedPlane->GetUPlane();
                MEParams.pSrc->RecPlane.step[1] = m_pRaisedPlane->GetUStep();
                MEParams.pSrc->RecPlane.ptr[2]  = m_pRaisedPlane->GetVPlane();
                MEParams.pSrc->RecPlane.step[2] = m_pRaisedPlane->GetVStep();
            }
            else
            {
                MEParams.pSrc->RecPlane.ptr[0]  = NULL;
                MEParams.pSrc->RecPlane.step[0] = NULL;
                MEParams.pSrc->RecPlane.ptr[1]  = NULL;
                MEParams.pSrc->RecPlane.step[1] = NULL;
                MEParams.pSrc->RecPlane.ptr[2]  = NULL;
                MEParams.pSrc->RecPlane.step[2] = NULL;
            }
#endif
            switch (InputPictureType)
            {
            case VC1_ENC_I_FRAME:
                err = Enc_IFrameUpdate(m_pME, &MEParams);
                VC1_ENC_CHECK(err);
                break;
            case VC1_ENC_P_FRAME:
                if (!param.bMVMixed)
                {
                    err = Enc_PFrameUpdate(m_pME, &MEParams);
                    VC1_ENC_CHECK(err);
                }
                else
                {        
                    err = Enc_PFrameUpdateMixed(m_pME, &MEParams);
                    VC1_ENC_CHECK(err);
                }
                break;
            case VC1_ENC_B_FRAME:
                //err = Enc_BFrameUpdate(m_pME, &MEParams);
                //if (err != UMC::UMC_OK)
                //    return err;
                break;
            case VC1_ENC_SKIP_FRAME:
                break;
            default:
                return UMC::UMC_ERR_FAILED;
            }

            //-----BRC---------------
            err = m_pBitRateControl->CheckFrameCompression(InputPictureType, m_pCodedFrame->GetDataLen() - CodedSize);
            if(err == UMC::UMC_OK)
                frame_recoding = false;

            else if((err == UMC::UMC_ERR_NOT_ENOUGH_BUFFER) && (uiQuantIndex == 31))
                frame_recoding = false;
            else  if((err == UMC::UMC_ERR_NOT_ENOUGH_DATA) && (uiQuantIndex == 2))
                frame_recoding = false;

            frame_recoding = frame_recoding & m_bFrameRecoding;

            RecodedFrameNum++;
        }

        m_LastQuant = uiQuantIndex*2 + bHalfQuant;

        //-----BRC-UPDATE--------------
        err = m_pBitRateControl->CompleteFrame(InputPictureType);
        //TODO: BRC error handling
        err = UMC::UMC_OK;
        return UMC::UMC_OK;
    }

    void VC1EncoderSM::SetMemoryAllocator(UMC::MemoryAllocator *pMemoryAllocator, bool bOwnAllocator)
    {
        m_pMemoryAllocator = pMemoryAllocator;
        m_bOwnAllocator    = bOwnAllocator;
    }

    UMC::Status VC1EncoderSM::Reset()
    {
        UMC::Status umcSts = UMC::UMC_OK;
        m_pWaitingList->Reset();
        m_pGOP->Reset();
        m_pStoredFrames->Reset();
        m_pBitRateControl->Reset();

        m_iFrameCount = 0;
        ResetPlanes();

        return umcSts;
    }

    UMC::Status VC1EncoderSM::GetInfo(UMC::VC1EncoderParams* pInfo)
    {
        UMC::Status umcSts = UMC::UMC_OK;
        VC1BRInfo BRInfo;
        VC1_hrd_OutData HRDParam;

        if(!m_pBitRateControl)
            return UMC::UMC_ERR_NOT_INITIALIZED;

        umcSts = m_pBitRateControl->GetCurrentHRDParams(0,&HRDParam);
        if(umcSts != UMC::UMC_OK)
            return umcSts;

        umcSts = m_pBitRateControl->GetBRInfo(&BRInfo);
        if(umcSts != UMC::UMC_OK)
            return umcSts;

        pInfo->qualityMeasure = -1;

        pInfo->info.bitrate      = BRInfo.bitrate;
        pInfo->info.framerate    = BRInfo.framerate;
        pInfo->m_iConstQuant     = BRInfo.constQuant;
        pInfo->m_uiHRDBufferSize = HRDParam.hrd_buffer;

        pInfo->m_uiGOPLength     = m_uiGOPLength;
        pInfo->m_uiBFrmLength    = m_uiBFrmLength;
        pInfo->m_uiMESearchSpeed = m_MESearchSpeed;
        pInfo->m_bFrameRecoding  = m_bFrameRecoding;

        pInfo->m_uiNumFrames     = m_iFrameCount;
        pInfo->numEncodedFrames  = m_iFrameCount;

        pInfo->level = m_SH->GetLevel();
        pInfo->info.stream_type    = UMC::VC1_VIDEO;
        pInfo->info.stream_subtype = UMC::VC1_VIDEO_VC1;
        pInfo->info.interlace_type = UMC::PROGRESSIVE;
        pInfo->info.color_format = UMC::YUV420;
        pInfo->info.streamPID = 0;
        pInfo->info.duration  = 0;
        pInfo->numThreads     = 0;

        pInfo->info.clip_info.height = m_SH->GetPictureHeight();
        pInfo->info.clip_info.width  = m_SH->GetPictureWidth();
        pInfo->m_bDeblocking = m_SH->IsLoopFilter();
        pInfo->m_bVSTransform = m_SH->IsVSTransform();
        pInfo->profile = VC1_ENC_PROFILE_M;

        pInfo->info.aspect_ratio_width = 1;
        pInfo->info.aspect_ratio_height = 1;

        pInfo->m_SuggestedOutputSize = 0;
        pInfo->m_SuggestedInputSize  =  0;

        pInfo->m_pData = NULL;

        return umcSts;
    }



    UMC::Status     VC1EncoderSM::SetMEParams(UMC::MeParams* MEParams, InitPictureParams *pParams,Ipp8u doubleQuant, bool Uniform)
    {
        UMC::Status umcSts          =  UMC::UMC_OK;

        MEParams->SetSearchSpeed(m_MESearchSpeed);

        MEParams->SkippedMetrics  = UMC::ME_Sad;
        MEParams->CostMetric      = UMC::ME_Sad;

        MEParams->ProcessSkipped = true;
        MEParams->ProcessDirect  = true;

        MEParams->UseFeedback    = m_bUseMeFeedback;
        MEParams->UpdateFeedback = m_bUseUpdateMeFeedback;
        MEParams->UseFastFeedback= m_bUseFastMeFeedback;

        MEParams->SelectVlcTables        = m_bSelectVLCTables;
        MEParams->ChangeInterpPixelType  = m_bChangeInterpPixelType;
        MEParams->UseTrellisQuantization = m_bUseTreillisQuantization;

        MEParams->Quant        = doubleQuant;
        MEParams->UniformQuant = Uniform;
        MEParams->IsSMProfile  = true;

        MEParams->MVRangeIndex = pParams->uiMVRangeIndex;
        MEParams->MVSizeOffset = MVSizeOffset;
        MEParams->MVLengthLong = longMVLength;
        MEParams->FastChroma   = m_bFastUVMC;
        MEParams->UseVarSizeTransform = m_SH->IsVSTransform();


        MEParams->DcTableLuma   = DCTables[pParams->sVLCTablesIndex.uiDecTypeDCIntra][0];
        MEParams->DcTableChroma = DCTables[pParams->sVLCTablesIndex.uiDecTypeDCIntra][1];

        MEParams->ScanTable[0] = VC1_Inter_8x8_Scan;
        MEParams->ScanTable[1] = VC1_Inter_8x4_Scan;
        MEParams->ScanTable[2] = VC1_Inter_4x8_Scan;
        MEParams->ScanTable[3] = VC1_Inter_4x4_Scan;
        MEParams->Interpolation      = (pParams->uiMVMode == VC1_ENC_1MV_HALF_BILINEAR)        ? 
            UMC::ME_VC1_Bilinear :UMC::ME_VC1_Bicubic;

        MEParams->ChromaInterpolation = UMC::ME_VC1_Bilinear;
        MEParams->PixelType           = (VC1_ENC_1MV_HALF_BILINEAR == pParams->uiMVMode || 
            VC1_ENC_1MV_HALF_BICUBIC == pParams->uiMVMode) ?
            UMC::ME_HalfPixel : UMC::ME_QuarterPixel;

        MEParams->SearchRange.x  = MVRange[2*pParams->uiMVRangeIndex];
        MEParams->SearchRange.y  = MVRange[2*pParams->uiMVRangeIndex + 1];


        return umcSts;
    }

    UMC::Status VC1EncoderSM::SetMEParams_I(UMC::MeParams* MEParams)
    {
        UMC::Status umcSts          =  UMC::UMC_OK;

        MEParams->pSrc = m_MeFrame[m_MeIndex.MeCurrIndex];
        assert(MEParams->pSrc!=NULL);
        VC1_NULL_PTR(MEParams->pSrc);

        MEParams->pSrc->type    = UMC::ME_FrmIntra;
        MEParams->SearchDirection = UMC::ME_IntraSearch;

        MEParams->pSrc->plane[0].ptr[0]  = m_pPlane->GetYPlane();
        MEParams->pSrc->plane[0].step[0] = m_pPlane->GetYStep();
        MEParams->pSrc->plane[0].ptr[1]  = m_pPlane->GetUPlane();
        MEParams->pSrc->plane[0].step[1] = m_pPlane->GetUStep();
        MEParams->pSrc->plane[0].ptr[2]  = m_pPlane->GetVPlane();
        MEParams->pSrc->plane[0].step[2] = m_pPlane->GetVStep();

#ifdef VC1_ME_MB_STATICTICS
        MEParams->pSrc->StatValid = true;
        MEParams->pSrc->RecPlane.clear();
        MEParams->pSrc->SrcPlane.clear();
#endif

        MEParams->BRefFramesNum = 0;
        MEParams->FRefFramesNum = 0;

        MEParams->pSrc->WidthMB = (m_SH->GetPictureWidth() + 15)/16;
        MEParams->pSrc->HeightMB = (m_SH->GetPictureHeight() + 15)/16;
        MEParams->FirstMB = 0 ;
        MEParams->LastMB  = MEParams->pSrc->HeightMB*MEParams->pSrc->WidthMB-1;

        return umcSts;
    }


    UMC::Status VC1EncoderSM::SetMEParams_P(UMC::MeParams* MEParams)
    {
        UMC::Status umcSts          =  UMC::UMC_OK;

        assert(m_pME!=NULL);

        if(m_pME == NULL)
            return UMC::UMC_ERR_NULL_PTR;
        MEParams->pSrc = m_MeFrame[m_MeIndex.MeCurrIndex];
        assert(MEParams->pSrc!=NULL);
        VC1_NULL_PTR(MEParams->pSrc);

        MEParams->pSrc->plane[0].ptr[0] = m_pPlane->GetYPlane();
        MEParams->pSrc->plane[0].step[0] = m_pPlane->GetYStep();
        MEParams->pSrc->plane[0].ptr[1] = m_pPlane->GetUPlane();
        MEParams->pSrc->plane[0].step[1] = m_pPlane->GetUStep();
        MEParams->pSrc->plane[0].ptr[2] = m_pPlane->GetVPlane();
        MEParams->pSrc->plane[0].step[2] = m_pPlane->GetVStep();

        MEParams->pSrc->WidthMB = (m_SH->GetPictureWidth() + 15)/16;
        MEParams->pSrc->HeightMB = (m_SH->GetPictureHeight() + 15)/16;
        MEParams->FirstMB = 0 ;
        MEParams->LastMB  = MEParams->pSrc->HeightMB*MEParams->pSrc->WidthMB-1;


#ifdef VC1_ME_MB_STATICTICS
        MEParams->pSrc->StatValid = true;
        MEParams->pSrc->RecPlane.clear();
        MEParams->pSrc->SrcPlane.clear();
#endif

        MEParams->pRefF[0] = m_MeFrame[m_MeIndex.MeRefFIndex];
        assert(MEParams->pRefF[0]!=NULL);
        VC1_NULL_PTR(MEParams->pRefF[0]);

        MEParams->pRefF[0]->plane[0].ptr[0] =  m_pForwardMEPlane->GetYPlane();
        MEParams->pRefF[0]->plane[0].step[0] = m_pForwardMEPlane->GetYStep();
        MEParams->pRefF[0]->plane[0].ptr[1] =  m_pForwardMEPlane->GetUPlane();
        MEParams->pRefF[0]->plane[0].step[1] = m_pForwardMEPlane->GetUStep();
        MEParams->pRefF[0]->plane[0].ptr[2] =  m_pForwardMEPlane->GetVPlane();
        MEParams->pRefF[0]->plane[0].step[2] = m_pForwardMEPlane->GetVStep();
        MEParams->pRefF[0]->WidthMB = (m_SH->GetPictureWidth() + 15)/16;
        MEParams->pRefF[0]->HeightMB = (m_SH->GetPictureHeight() + 15)/16;
        MEParams->pRefF[0]->padding = 32;
        MEParams->pSrc->type    = UMC::ME_FrmFrw;


        MEParams->SearchDirection    =  UMC::ME_ForwardSearch;

        MEParams->MbPart             =  UMC::ME_Mb16x16;

        MEParams->PredictionType     = UMC::ME_VC1Hybrid;

        MEParams->PicRange.top_left.x       = -14;
        MEParams->PicRange.top_left.y       = -14;
        MEParams->PicRange.bottom_right.x   = m_SH->GetPictureWidth() + 13;
        MEParams->PicRange.bottom_right.y   = m_SH->GetPictureHeight()+ 13;

        MEParams->BRefFramesNum             = 0;
        MEParams->FRefFramesNum             = 1;

        return umcSts;
    }
    UMC::Status  VC1EncoderSM::SetMEParams_PMixed(UMC::MeParams* MEParams)
    {
        UMC::Status umcSts          =  UMC::UMC_OK;

        assert(m_pME!=NULL);

        if(m_pME == NULL)
            return UMC::UMC_ERR_NULL_PTR;

        MEParams->pSrc = m_MeFrame[m_MeIndex.MeCurrIndex];
        assert(MEParams->pSrc!=NULL);
        VC1_NULL_PTR(MEParams->pSrc);
        MEParams->pSrc->type    = UMC::ME_FrmFrw;

        MEParams->pSrc->plane[0].ptr[0] = m_pPlane->GetYPlane();
        MEParams->pSrc->plane[0].step[0] = m_pPlane->GetYStep();
        MEParams->pSrc->plane[0].ptr[1] = m_pPlane->GetUPlane();
        MEParams->pSrc->plane[0].step[1] = m_pPlane->GetUStep();
        MEParams->pSrc->plane[0].ptr[2] = m_pPlane->GetVPlane();
        MEParams->pSrc->plane[0].step[2] = m_pPlane->GetVStep();

        MEParams->pSrc->WidthMB = (m_SH->GetPictureWidth() + 15)/16;
        MEParams->pSrc->HeightMB = (m_SH->GetPictureHeight() + 15)/16;
        MEParams->FirstMB = 0 ;
        MEParams->LastMB  = MEParams->pSrc->HeightMB*MEParams->pSrc->WidthMB-1;


#ifdef VC1_ME_MB_STATICTICS
        MEParams->pSrc->StatValid = true;
        MEParams->pSrc->RecPlane.clear();
        MEParams->pSrc->SrcPlane.clear();
#endif

        MEParams->pRefF[0] = m_MeFrame[m_MeIndex.MeRefFIndex];
        assert(MEParams->pRefF[0]!=NULL);
        VC1_NULL_PTR(MEParams->pRefF[0]);

        MEParams->pRefF[0]->plane[0].ptr[0] =  m_pForwardMEPlane->GetYPlane();
        MEParams->pRefF[0]->plane[0].step[0] = m_pForwardMEPlane->GetYStep();
        MEParams->pRefF[0]->plane[0].ptr[1] =  m_pForwardMEPlane->GetUPlane();
        MEParams->pRefF[0]->plane[0].step[1] = m_pForwardMEPlane->GetUStep();
        MEParams->pRefF[0]->plane[0].ptr[2] =  m_pForwardMEPlane->GetVPlane();
        MEParams->pRefF[0]->plane[0].step[2] = m_pForwardMEPlane->GetVStep();

        MEParams->pRefF[0]->WidthMB = (m_SH->GetPictureWidth() + 15)/16;
        MEParams->pRefF[0]->HeightMB = (m_SH->GetPictureHeight() + 15)/16;
        MEParams->pRefF[0]->padding = 32;

        MEParams->Interpolation      =  UMC::ME_VC1_Bicubic;
        MEParams->ChromaInterpolation = UMC::ME_VC1_Bilinear;

        MEParams->SearchDirection    =  UMC::ME_ForwardSearch;
        MEParams->PixelType          =  UMC::ME_QuarterPixel;
        MEParams->MbPart             =  UMC::ME_Mb8x8;

        MEParams->PredictionType     = UMC::ME_VC1Hybrid;

        MEParams->PicRange.top_left.x       = -7;
        MEParams->PicRange.top_left.y       = -7;
        MEParams->PicRange.bottom_right.x   = m_SH->GetPictureWidth() +7 ;
        MEParams->PicRange.bottom_right.y   = m_SH->GetPictureHeight()+7;


        MEParams->BRefFramesNum             = 0;
        MEParams->FRefFramesNum             = 1;

        return umcSts;
    }
    UMC::Status  VC1EncoderSM::SetMEParams_B(UMC::MeParams* MEParams, sFraction* pBFraction)
    {
        UMC::Status umcSts       = UMC::UMC_OK;

        assert(m_pME!=NULL);

        if(m_pME == NULL)
            return UMC::UMC_ERR_NULL_PTR;

        Ipp32u h = m_SH->GetNumMBInCol();
        Ipp32u w = m_SH->GetNumMBInRow();

        Ipp32s      scaleFactor  = BFractionScaleFactor[pBFraction->denom][pBFraction->num];
        void        (* GetMVDirect) (Ipp16s x, Ipp16s y, Ipp32s scaleFactor,
            sCoordinate * mvF, sCoordinate *mvB);


        MEParams->pSrc = m_MeFrame[m_MeIndex.MeCurrIndex];
        assert(MEParams->pSrc!=NULL);
        VC1_NULL_PTR(MEParams->pSrc);
        MEParams->pSrc->type    = UMC::ME_FrmBidir;

#ifdef VC1_ME_MB_STATICTICS
        MEParams->pSrc->StatValid = true;
        MEParams->pSrc->RecPlane.clear();
        MEParams->pSrc->SrcPlane.clear();
#endif

        MEParams->pRefF[0] = m_MeFrame[m_MeIndex.MeRefFIndex];
        assert(MEParams->pRefF[0]!=NULL);
        VC1_NULL_PTR(MEParams->pRefF[0]);

        MEParams->pRefB[0] = m_MeFrame[m_MeIndex.MeRefBIndex];
        assert(MEParams->pRefB[0]!=NULL);
        VC1_NULL_PTR(MEParams->pRefB[0]);

        sCoordinate MVFDirect    = {0,0};
        sCoordinate MVBDirect    = {0,0};

        sCoordinate                 MVPredMin1 = {-60,-60};
        sCoordinate                 MVPredMax1= {((Ipp16s)w*16-1)*4, ((Ipp16s)h*16-1)*4};

        Ipp16s*                     pSavedMV = m_pSavedMV;

        Ipp32u i = 0;
        Ipp32u j = 0;

        GetMVDirect = (MEParams->Interpolation == UMC::ME_VC1_Bilinear)? 
GetMVDirectHalf : GetMVDirectQuarter;

        MEParams->pSrc->plane[0].ptr[0] = m_pPlane->GetYPlane();
        MEParams->pSrc->plane[0].step[0] = m_pPlane->GetYStep();
        MEParams->pSrc->plane[0].ptr[1] = m_pPlane->GetUPlane();
        MEParams->pSrc->plane[0].step[1] = m_pPlane->GetUStep();
        MEParams->pSrc->plane[0].ptr[2] = m_pPlane->GetVPlane();
        MEParams->pSrc->plane[0].step[2] = m_pPlane->GetVStep();

        MEParams->pSrc->WidthMB = (m_SH->GetPictureWidth() + 15)/16;
        MEParams->pSrc->HeightMB = (m_SH->GetPictureHeight() + 15)/16;
        MEParams->FirstMB = 0 ;
        MEParams->LastMB  = MEParams->LastMB  = MEParams->pSrc->HeightMB*MEParams->pSrc->WidthMB-1;


        MEParams->pRefF[0]->plane[0].ptr[0] =  m_pForwardMEPlane->GetYPlane();
        MEParams->pRefF[0]->plane[0].step[0] = m_pForwardMEPlane->GetYStep();
        MEParams->pRefF[0]->plane[0].ptr[1] =  m_pForwardMEPlane->GetUPlane();
        MEParams->pRefF[0]->plane[0].step[1] = m_pForwardMEPlane->GetUStep();
        MEParams->pRefF[0]->plane[0].ptr[2] =  m_pForwardMEPlane->GetVPlane();
        MEParams->pRefF[0]->plane[0].step[2] = m_pForwardMEPlane->GetVStep();

        MEParams->pRefF[0]->WidthMB = (m_SH->GetPictureWidth() + 15)/16;
        MEParams->pRefF[0]->HeightMB = (m_SH->GetPictureHeight() + 15)/16;
        MEParams->pRefF[0]->padding = 32;

        MEParams->pRefB[0]->plane[0].ptr[0] =  m_pBackwardMEPlane->GetYPlane();
        MEParams->pRefB[0]->plane[0].step[0] = m_pBackwardMEPlane->GetYStep();
        MEParams->pRefB[0]->plane[0].ptr[1] =  m_pBackwardMEPlane->GetUPlane();
        MEParams->pRefB[0]->plane[0].step[1] = m_pBackwardMEPlane->GetUStep();
        MEParams->pRefB[0]->plane[0].ptr[2] =  m_pBackwardMEPlane->GetVPlane();
        MEParams->pRefB[0]->plane[0].step[2] = m_pBackwardMEPlane->GetVStep();

        MEParams->pRefB[0]->WidthMB = (m_SH->GetPictureWidth() + 15)/16;
        MEParams->pRefB[0]->HeightMB = (m_SH->GetPictureHeight() + 15)/16;
        MEParams->pRefB[0]->padding = 32;


        MEParams->SearchDirection    = UMC::ME_BidirSearch;

        MEParams->MbPart             = UMC::ME_Mb16x16;
        MEParams->SetSearchSpeed(m_MESearchSpeed);
        MEParams->PredictionType     = UMC::ME_VC1;
        MEParams->PicRange.top_left.x = -7;
        MEParams->PicRange.top_left.y = -7;
        MEParams->PicRange.bottom_right.x = m_SH->GetPictureWidth() ;
        MEParams->PicRange.bottom_right.y = m_SH->GetPictureHeight();


        MEParams->BRefFramesNum             = 1;
        MEParams->FRefFramesNum             = 1;

        for (i=0; i < h; i++)
        {
            for (j=0; j < w; j++)
            {
                GetMVDirect (pSavedMV[0],pSavedMV[1],scaleFactor, &MVFDirect, &MVBDirect);
                ScalePredict(&MVFDirect, j*16*4,i*16*4,MVPredMin1,MVPredMax1);
                ScalePredict(&MVBDirect, j*16*4,i*16*4,MVPredMin1,MVPredMax1);

                MEParams->pRefF[0]->MVDirect[(j + i*w)].x = MVFDirect.x;
                MEParams->pRefF[0]->MVDirect[(j + i*w)].y = MVFDirect.y;

                MEParams->pRefB[0]->MVDirect[(j + i*w)].x = MVBDirect.x;
                MEParams->pRefB[0]->MVDirect[(j + i*w)].y = MVBDirect.y;

                pSavedMV +=2;
            }
        }

        return umcSts;
    }

    UMC::Status VC1EncoderSM::SetInitPictureParams( InitPictureParams*  pParams, ePType PicType)
    {
        pParams->uiBFraction.num = 1;
        pParams->uiBFraction.denom = 2;

        if (m_SH->IsExtendedMV())
            pParams->uiMVRangeIndex = 1; // [0,3]
        else
            pParams->uiMVRangeIndex = 0;

        if (  (PicType == VC1_ENC_P_FRAME)&&m_bMixedMV)
        {
            pParams->bMVMixed = true;
            pParams->uiMVMode = VC1_ENC_MIXED_QUARTER_BICUBIC; 
        }
        else
        {
            pParams->bMVMixed = false;
            pParams->uiMVMode = VC1_ENC_1MV_HALF_BILINEAR;
        }


        pParams->sVLCTablesIndex.uiDecTypeAC      = 0;
        pParams->sVLCTablesIndex.uiCBPTab         = 0;
        pParams->sVLCTablesIndex.uiMVTab          = 0;
        pParams->sVLCTablesIndex.uiDecTypeDCIntra = 0;

        return UMC::UMC_OK;

    }
    UMC::Status VC1EncoderSM::MEPictureParamsRefine(UMC::MeParams* MEParams,InitPictureParams* pInitPicParam)
    {
        if(MEParams->ChangeInterpPixelType)
        {
            if(MEParams->MbPart == UMC::ME_Mb16x16 && MEParams->PixelType == UMC::ME_HalfPixel
                && MEParams->Interpolation == UMC::ME_VC1_Bilinear)
                pInitPicParam->uiMVMode = VC1_ENC_1MV_HALF_BILINEAR;
            else
                if(MEParams->MbPart == UMC::ME_Mb16x16 && MEParams->PixelType == UMC::ME_QuarterPixel
                    && MEParams->Interpolation == UMC::ME_VC1_Bicubic)
                    pInitPicParam->uiMVMode = VC1_ENC_1MV_QUARTER_BICUBIC;
                else
                    if(MEParams->MbPart == UMC::ME_Mb16x16 && MEParams->PixelType == UMC::ME_HalfPixel
                        && MEParams->Interpolation == UMC::ME_VC1_Bicubic)
                        pInitPicParam->uiMVMode = VC1_ENC_1MV_HALF_BICUBIC;
                    else
                        if(MEParams->MbPart == UMC::ME_Mb8x8 && MEParams->PixelType == UMC::ME_QuarterPixel
                            && MEParams->Interpolation == UMC::ME_VC1_Bicubic)
                            pInitPicParam->uiMVMode = VC1_ENC_MIXED_QUARTER_BICUBIC;
        }

        if(MEParams->SelectVlcTables)
        {
            pInitPicParam->sVLCTablesIndex.uiMVTab      = MEParams->OutMvTableIndex;
            pInitPicParam->sVLCTablesIndex.uiDecTypeAC  = MEParams->OutAcTableIndex;
            pInitPicParam->sVLCTablesIndex.uiCBPTab     = MEParams->OutCbpcyTableIndex;
        }

        return UMC::UMC_OK;
    }

}

#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
