/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.
//
//
//          MPEG2 encoder
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)

#include "mfx_enc_common.h"
#include "mfx_mpeg2_encode_hybrid_hw.h"
#include "umc_mpeg2_video_encoder.h"
#include "umc_mpeg2_enc.h"
#include "umc_mpeg2_enc_defs.h"
#include "mfx_brc_common.h"
#include "mfx_mpeg2_enc_common.h"
#include "umc_video_brc.h"
#include "umc_mpeg2_brc.h"
#include "mfx_task.h"
#include "libmfx_core.h"
#include "mfx_session.h"

namespace MPEG2EncoderHW
{

    HybridEncode::HybridEncode(VideoCORE *core, mfxStatus *sts)
    {
        m_pCore = core;
        m_pController = 0;

        m_pENC  = 0;
        m_pPAK  = 0;
        m_pBRC  = 0;

        m_pFrameStore = 0;
        m_pFrameTasks = 0;
        m_nFrameTasks = 0;

        m_nCurrTask         = 0;
        m_pExtTasks    = 0;

        *sts = (core ? MFX_ERR_NONE : MFX_ERR_NULL_PTR);
    }

    mfxStatus HybridEncode::Query(VideoCORE * core, mfxVideoParam *in, mfxVideoParam *out)
    {
        return ControllerBase::Query(core,in, out);
    }

    mfxStatus HybridEncode::QueryIOSurf(VideoCORE * core, mfxVideoParam *par, mfxFrameAllocRequest *request)
    {
        return ControllerBase::QueryIOSurf(core,par,request);
    }

    mfxStatus HybridEncode::Init(mfxVideoParam *par)
    {
        mfxStatus sts  = MFX_ERR_NONE;
        mfxStatus sts1 = MFX_ERR_NONE;

        if (is_initialized())
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        m_pController = new ControllerBase(m_pCore);

        sts = m_pController->Reset(par, true);
        if (sts == MFX_WRN_PARTIAL_ACCELERATION || sts < MFX_ERR_NONE)
        {
            Close();
            return sts;
        }
        sts1 = ResetImpl();
        if (MFX_ERR_NONE != sts1)
        {
            Close();
            return sts1;  
        }

#ifdef MPEG2_ENCODE_DEBUG_HW
        m_pENC->mpeg2_debug_ENC_HW = m_pPAK->mpeg2_debug_PAK = mpeg2_debug_ENCODE_HW = new MPEG2EncodeDebug_HW();
        mpeg2_debug_ENCODE_HW->Init(this);
#endif //MPEG2_ENCODE_DEBUG_HW

        return sts;
    }

    mfxStatus HybridEncode::Reset(mfxVideoParam *par)
    {
        mfxStatus sts  = MFX_ERR_NONE;
        mfxStatus sts1 = MFX_ERR_NONE;

        MFX_CHECK(is_initialized(), MFX_ERR_NOT_INITIALIZED);

        sts = m_pController->Reset(par, true);
        if (sts == MFX_WRN_PARTIAL_ACCELERATION || sts < MFX_ERR_NONE)
        {
            return sts;
        }
        sts1 = ResetImpl();
        MFX_CHECK_STS(sts1);

        return sts;
    }

    mfxStatus HybridEncode::ResetImpl()
    {
        mfxStatus sts = MFX_ERR_NONE;

        UMC::VideoBrcParams brcParams;
        m_nCurrTask     = 0;

        MFX_CHECK(is_initialized(), MFX_ERR_NOT_INITIALIZED);

        bool bHWEnc     = MFXVideoENCMPEG2_HW::GetIOPattern() & MFX_IOPATTERN_IN_VIDEO_MEMORY;
        bool bHWPak     = MFXVideoPAKMPEG2::GetIOPattern() & MFX_IOPATTERN_IN_VIDEO_MEMORY;
        bool bHWInput   = m_pController->isHWInput();
        bool bUseRawPic = m_pController->isRawFrames();
        mfxVideoParamEx_MPEG2* paramsEx = m_pController->getVideoParamsEx();


        if (!m_pFrameTasks)
        {
            m_nFrameTasks = 2;
            m_pFrameTasks = new EncodeFrameTaskHybrid[m_nFrameTasks]; 
        }

        mfxU16 refEncType =
            (bUseRawPic && bHWEnc == bHWInput)? m_pController->GetInputFrameType():
            mfxU16((bHWEnc? MFX_MEMTYPE_DXVA2_DECODER_TARGET : MFX_MEMTYPE_SYSTEM_MEMORY)|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_FROM_ENCODE);

        mfxU16 refPakType = (mfxU16)(((bHWPak)? MFX_MEMTYPE_DXVA2_DECODER_TARGET : MFX_MEMTYPE_SYSTEM_MEMORY)|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_FROM_ENCODE);

        for (mfxU32 i=0; i<m_nFrameTasks; i++)
        {
            sts = m_pFrameTasks[i].Reset(&paramsEx->mfxVideoParams,m_pCore,refEncType,refPakType);
            MFX_CHECK_STS(sts);
        }
        if (!m_pFrameStore)
        {
            m_pFrameStore = new FrameStoreHybrid(m_pCore);
        }

        sts = m_pFrameStore->Reset(bHWEnc,bHWPak,bUseRawPic,m_pController->GetInputFrameType(),m_nFrameTasks,&paramsEx->mfxVideoParams.mfx.FrameInfo);
        MFX_CHECK_STS(sts);  

        m_pController->SetAllocResponse(m_pFrameStore->GetFrameAllocResponse(true),true);
        m_pController->SetAllocResponse(m_pFrameStore->GetFrameAllocResponse(false),false);

        if (!m_pBRC)
        {
            m_pBRC = new MPEG2BRC_HW (m_pCore);
            sts = m_pBRC->Init(&paramsEx->mfxVideoParams);
            MFX_CHECK_STS(sts);  
        }
        else
        {
            sts = m_pBRC->Reset(&paramsEx->mfxVideoParams);
            MFX_CHECK_STS(sts);  
        }
        if (!m_pENC)
        {
            m_pENC = new MFXVideoENCMPEG2_HW(m_pCore,&sts);
            sts = m_pENC->Init(paramsEx);
            MFX_CHECK_STS(sts);
        }
        else
        {
            sts = m_pENC->Reset(paramsEx);
            MFX_CHECK_STS(sts);
        }
        if (!m_pPAK)
        {
            m_pPAK = new MFXVideoPAKMPEG2(m_pCore,&sts);
            sts = m_pPAK->Init(&paramsEx->mfxVideoParams);
            MFX_CHECK_STS(sts);
            m_pPAK->pThreadedMPEG2PAK->SetEncoderPointer(m_pCore->GetSession()->m_pENCODE.get());

        }
        else
        {
            sts = m_pPAK->Reset(&paramsEx->mfxVideoParams);
            MFX_CHECK_STS(sts); 
        }
        if (!m_pExtTasks)
        {
            m_pExtTasks = new clExtTasks1;
        }
#ifdef MPEG2_ENCODE_HW_PERF
        vm_time_init(&common_time);
        vm_time_init(&enc_time);
        vm_time_init(&pak_time);
        vm_time_init(&prepare_input_frames_time);
        vm_time_init(&prepare_ref_frames_time);
        num_input_copy = 0;
        num_ref_copy = 0;
#endif
        return sts;
    }

    //virtual mfxStatus Close(void); // same name
    mfxStatus HybridEncode::Close(void)
    {

        if(!is_initialized())
            return MFX_ERR_NOT_INITIALIZED;

#ifdef MPEG2_ENCODE_HW_PERF
        FILE* f = fopen ("mpeg2_encode_hw_perf.txt","a+");
        fprintf(f,"%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", (int)common_time.diff,
            (int)enc_time.diff,
            (int)pak_time.diff,
            (int)prepare_input_frames_time.diff,
            (int)prepare_ref_frames_time.diff,
            (int)common_time.freq,
            num_input_copy,
            num_ref_copy);
        fclose(f);
#endif

        if (m_pController)
        {
            m_pController->Close();
            delete m_pController;
            m_pController = 0; 
        }
        if (m_pENC)
        {
            m_pENC->Close();
            delete m_pENC;
            m_pENC = 0;
        }
        if (m_pPAK)
        {
            m_pPAK->Close();
            delete m_pPAK;
            m_pPAK = 0;
        }
        if (m_pFrameTasks)
        {
            delete [] m_pFrameTasks;  
            m_nFrameTasks = 0;
        }
        if (m_pFrameStore)
        {
            delete m_pFrameStore;
            m_pFrameStore = 0; 
        }
        if (m_pBRC)
        {
            delete m_pBRC;
            m_pBRC = 0;  
        }  
        if (m_pExtTasks)
        {
            delete m_pExtTasks;
            m_pExtTasks = 0;
        }
        return MFX_ERR_NONE;
    }

    mfxStatus HybridEncode::GetVideoParam(mfxVideoParam *par)
    {
        mfxStatus sts = MFX_ERR_NONE;

        if(!is_initialized())
            return MFX_ERR_NOT_INITIALIZED;

        sts = m_pController->GetVideoParam(par);
        MFX_CHECK_STS(sts);

        if (mfxExtCodingOptionSPSPPS* ext = SHParametersEx::GetExtCodingOptionsSPSPPS(par->ExtParam, par->NumExtParam))
        {
            MFX_CHECK_NULL_PTR1(ext->SPSBuffer);

            mfxBitstream bs = { 0 };
            mfxFrameCUC cuc = { 0 };

            bs.Data       = ext->SPSBuffer;
            bs.MaxLength  = ext->SPSBufSize;
            cuc.Bitstream = &bs;

            mfxStatus sts = m_pPAK->RunSeqHeader(&cuc);
            MFX_CHECK_STS(sts);

            ext->SPSBufSize = mfxU16(cuc.Bitstream->DataLength);
            ext->PPSBufSize = 0; // pps is n/a for mpeg2
            ext->SPSId      = 0;
            ext->PPSId      = 0;
        }

        return MFX_ERR_NONE;
    }

    mfxStatus HybridEncode::GetFrameParam(mfxFrameParam *par)
    {
        MFX_CHECK_NULL_PTR1(par)

        return MFX_ERR_UNSUPPORTED ;
    }

    mfxStatus HybridEncode::GetEncodeStat(mfxEncodeStat *stat)
    {
        if(!is_initialized())
            return MFX_ERR_NOT_INITIALIZED;

        return  m_pController->GetEncodeStat(stat);
    }


    mfxStatus HybridEncode::EncodeFrameCheck(
        mfxEncodeCtrl *ctrl,
        mfxFrameSurface1 *surface,
        mfxBitstream *bs,
        mfxFrameSurface1 **reordered_surface,
        mfxEncodeInternalParams *pInternalParams)
    {
        if(!is_initialized())
            return MFX_ERR_NOT_INITIALIZED;

        return  m_pController->EncodeFrameCheck(ctrl,surface,bs,reordered_surface,pInternalParams);
    }


    mfxStatus HybridEncode::EncodeFrame(mfxEncodeCtrl* /*ctrl*/, mfxEncodeInternalParams *pInputInternalParams, mfxFrameSurface1 *input_surface, mfxBitstream *bs)
    {
        mfxStatus         sts       = MFX_ERR_NONE;
        mfxU32            dataLen   = 0;
        mfxI32            recode    = 0;

        mfxU32            currTask  = m_nCurrTask;
        mfxU32            nextTask  = (m_nCurrTask + 1)%m_nFrameTasks;
        bool              bInputFrame= true;

        mfxVideoParamEx_MPEG2* paramsEx = m_pController->getVideoParamsEx();

        if(!is_initialized())
            return MFX_ERR_NOT_INITIALIZED;

        MFX_CHECK_NULL_PTR1(bs);


#ifdef MPEG2_ENCODE_HW_PERF
        vm_time_start(0,&common_time);
#endif


        if (input_surface)
        {
            sts = m_pController->CheckFrameType(pInputInternalParams);
            MFX_CHECK_STS(sts);
        }
        if (m_pFrameTasks[currTask].isNotStarted())
        {
            mfxEncodeInternalParams   sInternalParams;
            mfxEncodeInternalParams*  pInternalParams = &sInternalParams;      
            mfxFrameSurface1 *        surface = 0;
            FramesSet                 EncFrames;  
            FramesSet                 PakFrames;


            if ((sts = m_pController->ReorderFrame(pInputInternalParams,
                m_pController->GetOriginalSurface(input_surface),
                pInternalParams,
                &surface))== MFX_ERR_MORE_DATA)
            {
#ifdef MPEG2_ENCODE_HW_PERF
                vm_time_stop(0,&common_time);
#endif 
                return MFX_ERR_NONE;
            }
            MFX_CHECK_STS(sts);
            bInputFrame = false;

#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_start(0,&prepare_input_frames_time);
#endif
            sts = m_pFrameStore->NextFrame( surface,
                pInternalParams->FrameOrder,
                pInternalParams->FrameType,
                pInternalParams->InternalFlags,
                &EncFrames, &PakFrames);
#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_stop(0,&prepare_input_frames_time);
            num_input_copy++;
#endif
            MFX_CHECK_STS(sts);

            m_pFrameTasks[currTask].SetFrames(&EncFrames, &PakFrames, pInternalParams);

            sts = m_pFrameTasks[currTask].FillFrameParams((mfxU8)pInternalParams->FrameType,paramsEx,surface->Info.PicStruct,(pInternalParams->InternalFlags&MFX_IFLAG_BWD_ONLY)?1:0); 
            MFX_CHECK_STS(sts);

            sts = m_pFrameTasks[currTask].FillSliceMBParams();
            MFX_CHECK_STS(sts);


            m_pFrameTasks[currTask].DataReady();

            sts = m_pCore->DecreaseReference(&surface->Data);
            MFX_CHECK_STS(sts);

        }
        if (m_pFrameTasks[nextTask].isNotStarted())
        {            
            mfxEncodeInternalParams   sInternalParams;
            mfxEncodeInternalParams*  pInternalParams = &sInternalParams;      
            mfxFrameSurface1 *        surface = 0;
            FramesSet                 EncFrames;  
            FramesSet                 PakFrames;

            sts = (bInputFrame)? m_pController->ReorderFrame(pInputInternalParams,
                m_pController->GetOriginalSurface(input_surface),
                pInternalParams,&surface): 
            m_pController->CheckNextFrame(pInternalParams,&surface);

            if  (sts != MFX_ERR_MORE_DATA)
            {

                MFX_CHECK_STS(sts);

#ifdef MPEG2_ENCODE_HW_PERF
                vm_time_start(0,&prepare_input_frames_time);
#endif
                sts = m_pFrameStore->NextFrame( surface,
                    pInternalParams->FrameOrder,
                    pInternalParams->FrameType,
                    pInternalParams->InternalFlags,
                    &EncFrames, &PakFrames);
#ifdef MPEG2_ENCODE_HW_PERF
                vm_time_stop(0,&prepare_input_frames_time);
                num_input_copy++;
#endif

                MFX_CHECK_STS(sts);


                m_pFrameTasks[nextTask].SetFrames(&EncFrames, &PakFrames, pInternalParams);

                sts = m_pFrameTasks[nextTask].FillFrameParams((mfxU8)pInternalParams->FrameType,
                    paramsEx,
                    surface->Info.PicStruct,
                    (pInternalParams->InternalFlags&MFX_IFLAG_BWD_ONLY)?1:0); 
                MFX_CHECK_STS(sts);

                sts = m_pFrameTasks[nextTask].FillSliceMBParams();
                MFX_CHECK_STS(sts);

                m_pFrameTasks[nextTask].DataReady();

                sts = m_pCore->DecreaseReference(&surface->Data);
                MFX_CHECK_STS(sts);

            }
            else
            {
                // sts = MFX_ERR_NONE;
            }
        }

        if (m_pFrameTasks[currTask].isEncStarted())
        {
            mfxEncodeInternalParams* pInternalParams = m_pFrameTasks[currTask].GetInternalParams();
            m_pBRC->SetQuant(pInternalParams->QP, pInternalParams->FrameType);

            sts = m_pBRC->StartNewFrame(m_pFrameTasks[currTask].GetFrameParams(), 0);
            MFX_CHECK_STS(sts);

            sts = m_pBRC->SetQuantDCPredAndDelay(m_pFrameTasks[currTask].GetFrameCUC(),0);
            MFX_CHECK_STS(sts); 

#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_start(0,&enc_time);
#endif
            sts = m_pENC->RunFrameVmeENC_Stage2(m_pFrameTasks[currTask].GetFrameCUC());

#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_stop(0,&enc_time);
#endif

            MFX_CHECK_STS(sts);

            m_pFrameTasks[currTask].EncReady();    
        }
        if (m_pFrameTasks[currTask].isDataReady())
        {
            mfxEncodeInternalParams* pInternalParams = m_pFrameTasks[currTask].GetInternalParams();
            m_pBRC->SetQuant(pInternalParams->QP, pInternalParams->FrameType);


            m_pFrameTasks[currTask].FillFrameDataParameters (true);

            sts = m_pBRC->StartNewFrame(m_pFrameTasks[currTask].GetFrameParams(), 0);
            MFX_CHECK_STS(sts);

            sts = m_pBRC->SetQuantDCPredAndDelay(m_pFrameTasks[currTask].GetFrameCUC(),0);
            MFX_CHECK_STS(sts); 

#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_start(0,&enc_time);
#endif
            sts = m_pENC->RunFrameVmeENC(m_pFrameTasks[currTask].GetFrameCUC());

#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_stop(0,&enc_time);
#endif
            MFX_CHECK_STS(sts);

            m_pFrameTasks[currTask].EncReady();    
        }


        if (m_pFrameTasks[nextTask].isDataReady()  &&
            !m_pFrameTasks[currTask].isNextField() && 
            (m_pController->isRawFrames() || !m_pFrameTasks[currTask].isReferenceFrame()))
        {
            m_pFrameTasks[nextTask].FillFrameDataParameters (true);

            sts = m_pBRC->SetQuantDCPredAndDelay(m_pFrameTasks[nextTask].GetFrameCUC(),0);
            MFX_CHECK_STS(sts);

#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_start(0,&enc_time);
#endif
            sts = m_pENC->RunFrameVmeENC_Stage1(m_pFrameTasks[nextTask].GetFrameCUC());

#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_stop(0,&enc_time);
#endif 

            MFX_CHECK_STS(sts);

            m_pFrameTasks[nextTask].EncStarted();    
        }

        if (!m_pFrameTasks[currTask].isEncReady())
        {
            return MFX_ERR_UNKNOWN;    
        }

        mfxEncodeInternalParams* pInternalParams = m_pFrameTasks[currTask].GetInternalParams();

        bs->FrameType = pInternalParams->FrameType ;
        bs->TimeStamp = m_pFrameTasks[currTask].GetTimeStamp();

        bs->DecodeTimeStamp = (pInternalParams->FrameType & MFX_FRAMETYPE_B)? 
            CalcDTSForNonRefFrameMpeg2(bs->TimeStamp):
            CalcDTSForRefFrameMpeg2(bs->TimeStamp,
                m_pFrameTasks[currTask].GetLastRefDist(),
                paramsEx->mfxVideoParams.mfx.GopRefDist,
                CalculateUMCFramerate(paramsEx->mfxVideoParams.mfx.FrameInfo.FrameRateExtN, paramsEx->mfxVideoParams.mfx.FrameInfo.FrameRateExtD));

        m_pFrameTasks[currTask].FillBSParameters(bs);


        if ((pInternalParams->InternalFlags & MFX_IFLAG_ADD_HEADER))
        {
            sts = m_pPAK->RunSeqHeader(m_pFrameTasks[currTask].GetFrameCUC());
            MFX_CHECK_STS(sts);
        }    
        m_pFrameTasks[currTask].FillFrameDataParameters (false);
        recode = 0;
        dataLen = bs->DataLength;
        for (;;)
        {
            mfxU32  bitsize             = 0;
            bool    bNotEnoughBuffer    = false;

            if(pInternalParams->NumPayload > 0 && pInternalParams->Payload!=0 )
            {
                for (mfxU32 i = 0; i < pInternalParams->NumPayload; i++)
                {
                    if (pInternalParams->Payload[i]!=0 && pInternalParams->Payload[i]->Type == USER_START_CODE)
                    {
                        sts = m_pPAK->InsertUserData(pInternalParams->Payload[i]->Data, (pInternalParams->Payload[i]->NumBit+7)>>3,0,bs);
                        MFX_CHECK_STS(sts);
                    }
                }
            }
            if (m_pBRC->IsSkipped(recode))
            {
                m_pFrameTasks[currTask].SetSkipMBParams();
            }
#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_start(0,&pak_time);
#endif
            sts = m_pPAK->RunFramePAK(m_pFrameTasks[currTask].GetFrameCUC());

#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_stop(0,&pak_time);
#endif 
            if (sts == MFX_ERR_NOT_ENOUGH_BUFFER)
            {
                sts = MFX_ERR_NONE;
                bNotEnoughBuffer = true;        
            }
            MFX_CHECK_STS(sts);

            bitsize = (bs->DataLength - dataLen)*8;        
            sts = m_pBRC->UpdateBRC (m_pFrameTasks[currTask].GetFrameParams(),
                bs,bitsize,m_pController->GetFrameNumber()+1,
                bNotEnoughBuffer,recode);
            MFX_CHECK_STS(sts);

            if (recode == 0)
            {
                break;        
            }

            sts = m_pBRC->StartNewFrame(m_pFrameTasks[currTask].GetFrameParams(), recode);
            MFX_CHECK_STS(sts);

            sts = m_pBRC->SetQuantDCPredAndDelay(m_pFrameTasks[currTask].GetFrameCUC(),recode);
            MFX_CHECK_STS(sts); 

            bs->DataLength  = dataLen;


        }
#ifdef MPEG2_ENCODE_HW_PERF
        vm_time_start(0,&prepare_ref_frames_time);
#endif
        sts = m_pFrameTasks[currTask].CopyReference();
#ifdef MPEG2_ENCODE_HW_PERF
        vm_time_stop(0,&prepare_ref_frames_time);
        num_ref_copy += (m_pFrameTasks[currTask].isReferenceFrame())? 1:0;
#endif 

        MFX_CHECK_STS(sts);

        if (m_pFrameTasks[currTask].isNextField())
        {
            m_pFrameTasks[currTask].FillFrameDataParameters (true);

            sts = m_pBRC->StartNewFrame(m_pFrameTasks[currTask].GetFrameParams(), 0);
            MFX_CHECK_STS(sts);

            sts = m_pBRC->SetQuantDCPredAndDelay(m_pFrameTasks[currTask].GetFrameCUC(),0);
            MFX_CHECK_STS(sts);

#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_start(0,&enc_time);
#endif
            sts = m_pENC->RunFrameVmeENC(m_pFrameTasks[currTask].GetFrameCUC());

#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_stop(0,&enc_time);
#endif 

            MFX_CHECK_STS(sts); 

            m_pFrameTasks[0].FillFrameDataParameters (false);

            recode = 0;
            dataLen = bs->DataLength;

            for (;;)
            {
                bool bNotEnoughBuffer = false;
#ifdef MPEG2_ENCODE_HW_PERF
                vm_time_start(0,&pak_time);
#endif
                sts = m_pPAK->RunFramePAK(m_pFrameTasks[currTask].GetFrameCUC());

#ifdef MPEG2_ENCODE_HW_PERF
                vm_time_stop(0,&pak_time);
#endif 

                if (sts == MFX_ERR_NOT_ENOUGH_BUFFER)
                {
                    sts = MFX_ERR_NONE;
                    bNotEnoughBuffer = true;        
                }
                MFX_CHECK_STS(sts);

                mfxU32 bitsize = (bs->DataLength - dataLen)*8;        
                sts = m_pBRC->UpdateBRC (m_pFrameTasks[currTask].GetFrameParams(),bs,bitsize,
                    m_pController->GetFrameNumber(),bNotEnoughBuffer,recode);
                MFX_CHECK_STS(sts);

                if (recode == 0)
                {
                    break;        
                }
                else if (recode == UMC::BRC_RECODE_EXT_PANIC)
                {
                    //limited mode (temp solution)
                    return MFX_ERR_NOT_ENOUGH_BUFFER;            
                }

                sts = m_pBRC->StartNewFrame(m_pFrameTasks[currTask].GetFrameParams(), recode);
                MFX_CHECK_STS(sts);

                sts = m_pBRC->SetQuantDCPredAndDelay(m_pFrameTasks[currTask].GetFrameCUC(),recode);
                MFX_CHECK_STS(sts); 

                bs->DataLength  = dataLen;

            }
#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_start(0,&prepare_ref_frames_time);
#endif 
            sts = m_pFrameTasks[currTask].CopyReference();

#ifdef MPEG2_ENCODE_HW_PERF
            vm_time_stop(0,&prepare_ref_frames_time);
            num_ref_copy += (m_pFrameTasks[currTask].isReferenceFrame())? 1:0;
#endif 


            MFX_CHECK_STS(sts);
        }

        m_pFrameTasks[currTask].finishTask();

        sts = m_pFrameTasks[currTask].ReleaseFrames();
        MFX_CHECK_STS(sts);  

        m_pController->FinishFrame(bs->DataLength - dataLen);

        m_nCurrTask = nextTask;

#ifdef MPEG2_ENCODE_HW_PERF
        vm_time_stop(0,&common_time);
#endif 

        return MFX_ERR_NONE;
    }

    mfxStatus HybridEncode::CancelFrame(mfxEncodeCtrl * /*ctrl*/, mfxEncodeInternalParams * /*pInternalParams*/, mfxFrameSurface1 *surface, mfxBitstream * /*bs*/)
    {
        MFX_CHECK_NULL_PTR1(surface)
            m_pController->FinishFrame(0);
        return m_pCore->DecreaseReference(&surface->Data);
    }
#undef RANGE_TO_F_CODE



    static
        mfxStatus MPEG2ENCODERoutine(void *pState, void *param, mfxU32 /*n*/, mfxU32 /*callNumber*/)
    {
        //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "MPEG2EncodeFrame");

        mfxStatus sts = MFX_ERR_NONE;
        HybridEncode* th = (HybridEncode*)pState;
        sExtTask1 *pTask = (sExtTask1 *)param;

        sts = th->m_pExtTasks->CheckTask(pTask);
        MFX_CHECK_STS(sts);

        if(VM_TIMEOUT != vm_event_timed_wait(&pTask->m_new_frame_event, 0))
        {
            sts = th->EncodeFrame(0, &pTask->m_inputInternalParams, pTask->m_pInput_surface, pTask->m_pBs);
            MFX_CHECK_STS(sts);
            return MFX_TASK_DONE;
        }
        else if ((sts = th->m_pPAK->pThreadedMPEG2PAK->PAKSlice()) == MFX_ERR_NONE)
        {
            return  MFX_TASK_NEED_CONTINUE;
        }
        else if (sts == MFX_ERR_NOT_FOUND)
        {
            return MFX_TASK_BUSY;
        }
        return MFX_TASK_BUSY;
    }


    mfxStatus HybridEncode::EncodeFrameCheck(mfxEncodeCtrl *ctrl,
        mfxFrameSurface1 *surface,
        mfxBitstream *bs,
        mfxFrameSurface1 **reordered_surface,
        mfxEncodeInternalParams *pInternalParams,
        MFX_ENTRY_POINT *pEntryPoint)
    {
        MFX_CHECK(is_initialized(),MFX_ERR_NOT_INITIALIZED);

        mfxStatus sts_ret = MFX_ERR_NONE;
        mfxStatus sts = MFX_ERR_NONE;
        sExtTask1 *pTask = 0;


        //MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "MPEG2Enc_check");

        // pointer to the task processing object
        pEntryPoint->pState = this;

        // pointer to the task processing routine
        pEntryPoint->pRoutine = MPEG2ENCODERoutine;
        // task name
        pEntryPoint->pRoutineName = "EncodeMPEG2";

        // number of simultaneously allowed threads for the task

        pEntryPoint->requiredNumThreads = (m_pPAK) ? m_pPAK->pThreadedMPEG2PAK->GetNumOfThreads() + 1 : 1;

        mfxFrameSurface1 *pOriginalSurface = m_pController->GetOriginalSurface(surface);
        if (pOriginalSurface != surface)
        {
            if (pOriginalSurface == 0)
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            pOriginalSurface->Info = surface->Info;
            pOriginalSurface->Data.Corrupted = surface->Data.Corrupted;
            pOriginalSurface->Data.DataFlag = surface->Data.DataFlag;
            pOriginalSurface->Data.TimeStamp = surface->Data.TimeStamp;
            pOriginalSurface->Data.FrameOrder = surface->Data.FrameOrder;
        }

        sts_ret = EncodeFrameCheck(ctrl, pOriginalSurface, bs, reordered_surface, pInternalParams);

        if (sts_ret != MFX_ERR_NONE && sts_ret !=(mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK && sts_ret<0)
            return sts_ret;

        sts = m_pExtTasks->AddTask( pInternalParams,
                                    *reordered_surface, 
                                    bs,
                                    &pTask);
        MFX_CHECK_STS(sts);


        // pointer to the task's parameter
        pEntryPoint->pParam = pTask;


        return sts_ret;
    }
}
#endif // MFX_ENABLE_MPEG2_VIDEO_ENCODE
