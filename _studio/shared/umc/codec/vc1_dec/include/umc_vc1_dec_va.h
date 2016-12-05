//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#ifndef UMC_RESTRICTED_CODE_VA

#ifndef __UMC_VC1_VIDEO_DECODER_VA_H_
#define __UMC_VC1_VIDEO_DECODER_VA_H_

#include "umc_va_base.h"

#if defined(UMC_VA)
#include "umc_vc1_video_decoder.h"
#include "umc_vc1_dec_frame_descr_va.h"
#include "umc_vc1_dec_exception.h"

namespace UMC
{
    using namespace VC1Exceptions;
    template <class T>
    class VC1VideoDecoderVA : public VC1VideoDecoder
    {
        friend class VC1ThreadDecoder;
      DYNAMIC_CAST_DECL(VC1VideoDecoderVA, VC1VideoDecoder)
    public:
        VC1VideoDecoderVA():m_stCodes_VA(NULL)
        {
        }
        virtual ~VC1VideoDecoderVA()
        {
            if (m_stCodes_VA)
            {
                ippsFree(m_stCodes_VA);
                m_stCodes_VA = NULL;
            }
            m_PostProcessing = NULL;
            m_pStore = NULL;
            m_pContext = NULL;
        }

    protected:
        //virtual Status VC1DecodeFrame                      (VC1VideoDecoder* pDec,MediaData* in, VideoData* out_data);
        virtual bool    InitAlloc                   (VC1Context* pContext,
                                                       Ipp32u MaxFrameNum)
        {
            if(!InitTables(pContext))
                return false;

            pContext->m_frmBuff.m_iDisplayIndex = -1;
            pContext->m_frmBuff.m_iCurrIndex    = -1;
            pContext->m_frmBuff.m_iPrevIndex    = -1;
            pContext->m_frmBuff.m_iNextIndex    = -1;
            pContext->m_frmBuff.m_iBFrameIndex  = -1;
            pContext->m_frmBuff.m_iRangeMapIndex = -1;
            pContext->m_frmBuff.m_iRangeMapIndexPrev = -1;

            m_bLastFrameNeedDisplay = true;



            //for slice, field start code
            if(m_stCodes_VA == NULL)
            {
                m_stCodes_VA = (MediaDataEx::_MediaDataEx *)ippsMalloc_8u(START_CODE_NUMBER*2*sizeof(Ipp32s)+sizeof(MediaDataEx::_MediaDataEx));
                memset(m_stCodes_VA, 0, (START_CODE_NUMBER*2*sizeof(Ipp32s)+sizeof(MediaDataEx::_MediaDataEx)));
                m_stCodes_VA->count      = 0;
                m_stCodes_VA->index      = 0;
                m_stCodes_VA->bstrm_pos  = 0;
                m_stCodes_VA->offsets    = (Ipp32u*)((Ipp8u*)m_stCodes_VA +
                                    sizeof(MediaDataEx::_MediaDataEx));
                m_stCodes_VA->values     = (Ipp32u*)((Ipp8u*)m_stCodes_VA->offsets +
                                    START_CODE_NUMBER*sizeof( Ipp32u));
            }

            return true;
        }
        void GetStartCodes_HW(MediaData* in, bool isWMV, Ipp32u &sShift)
        {
            Ipp8u* readPos = (Ipp8u*)in->GetBufferPointer();
            Ipp32u readBufSize = (Ipp32u)in->GetDataSize();
            Ipp8u* readBuf = (Ipp8u*)in->GetBufferPointer();

            Ipp32u frameSize = 0;
            MediaDataEx::_MediaDataEx *stCodes = m_stCodes_VA;

            if (!m_bIsNeedAddFrameSC)
                stCodes->count = 0;
            else
                stCodes->count = 1;

            sShift = 0;


            Ipp32u size = 0;
            Ipp8u* ptr = NULL;
            Ipp32u readDataSize = 0;
            Ipp32u a = 0x0000FF00 | (*readPos);
            Ipp32u b = 0xFFFFFFFF;
            Ipp32u FrameNum = 0;

            memset(stCodes->offsets, 0,START_CODE_NUMBER*sizeof(Ipp32s));
            memset(stCodes->values, 0,START_CODE_NUMBER*sizeof(Ipp32s));


            while(readPos < (readBuf + readBufSize))
            {
                if (stCodes->count > 512)
                    return;
                //find sequence of 0x000001 or 0x000003
                while(!( b == 0x00000001 || b == 0x00000003 )
                    &&(++readPos < (readBuf + readBufSize)))
                {
                    a = (a<<8)| (Ipp32s)(*readPos);
                    b = a & 0x00FFFFFF;
                }

                //check end of read buffer
                if(readPos < (readBuf + readBufSize - 1))
                {
                    if(*readPos == 0x01)
                    {
                        if((*(readPos + 1) == VC1_Slice) ||
                           (*(readPos + 1) == VC1_Field) ||
                           (*(readPos + 1) == VC1_FrameHeader)||
                           (*(readPos + 1) == VC1_SliceLevelUserData) ||
                           (*(readPos + 1) == VC1_FieldLevelUserData) ||
                           (*(readPos + 1) == VC1_FrameLevelUserData)                           
                           )
                            /*|| (FrameNum)*/
                        {
                            readPos+=2;
                            ptr = readPos - 5;
                            //trim zero bytes
                            //if (stCodes->count)
                            //{
                            //    ptr--;
                            //    ptr--;
                            //    while ( (*ptr==0) && (ptr > readBuf) )
                            //        ptr--;
                            //}
                            //slice or field size
                            size = (Ipp32u)(ptr - readBuf - readDataSize+1);

                            frameSize = frameSize + size;

                            if (!m_bIsNeedAddFrameSC)
                            {
                                stCodes->offsets[stCodes->count] = frameSize;
                                if (FrameNum)
                                    sShift = 1;
                            }
                            //MS wmv parser skip first start code
                            else
                                stCodes->offsets[stCodes->count] = frameSize + 4;

                            stCodes->values[stCodes->count]  = ((*(readPos-1))<<24) + ((*(readPos-2))<<16) + ((*(readPos-3))<<8) + (*(readPos-4));

                            readDataSize = (Ipp32u)(readPos - readBuf - 4);
                            a = 0x00010b00 |(Ipp32s)(*readPos);
                            b = a & 0x00FFFFFF;
                            stCodes->count++;
                        }
                        else
                        {
                            {
                                readPos+=2;
                                ptr = readPos - 5;

                                //trim zero bytes
                                if (stCodes->count)
                                {
                                    while ( (*ptr==0) && (ptr > readBuf) )
                                        ptr--;
                                }

                                //slice or field size
                                size = (Ipp32u)(readPos - readBuf - readDataSize - 4);

                                //currFramePos = currFramePos + size;
                                frameSize = frameSize + size;

                                //if (!m_bIsNeedAddFrameSC)
                                //    stCodes->offsets[stCodes->count] = frameSize;
                                ////MS wmv parser skip first start code
                                //else
                                //    stCodes->offsets[stCodes->count] = frameSize + 4;

                                //stCodes->values[stCodes->count]  = ((*(readPos-1))<<24) + ((*(readPos-2))<<16) + ((*(readPos-3))<<8) + (*(readPos-4));

                                //stCodes->count++;
                                readDataSize = readDataSize + size;
                                FrameNum++;
                                //return;
                            }
                        }
                    }
                    else //if(*readPos == 0x03)
                    {
                        readPos++;
                        a = (a<<8)| (Ipp32s)(*readPos);
                        b = a & 0x00FFFFFF;
                    }
                }
                else
                {
                    //end of stream
                    size = (Ipp32u)(readPos- readBuf - readDataSize);
                    readDataSize = readDataSize + size;
                    return;
                }
            }
        }


        virtual Status VC1DecodeFrame(VC1VideoDecoder* pDec,MediaData* in, VideoData* out_data)
        {
            if (m_va->m_Profile == VC1_VLD)
            {
#if defined (UMC_VA_DXVA)
                if (m_va->IsIntelCustomGUID())
                {
                    if(m_va->GetProtectedVA())
                         return VC1DecodeFrame_VLD<VC1FrameDescriptorVA_Protected<T> >(pDec,in,out_data);
                    else
                         return VC1DecodeFrame_VLD<VC1FrameDescriptorVA_EagleLake<T> >(pDec,in,out_data);
                }
                else
#endif
#if defined (UMC_VA_LINUX)
                return VC1DecodeFrame_VLD<VC1FrameDescriptorVA_Linux<T> >(pDec,in,out_data);
#endif
                    return VC1DecodeFrame_VLD<VC1FrameDescriptorVA<T> >(pDec,in,out_data);
            }
            return UMC_ERR_FAILED;
        }
        virtual Status FillAndExecute(VC1VideoDecoder* pDec, MediaData* in)
        {
           Ipp32u stShift = 0;
           Ipp32s SCoffset = 0;
           if (pDec->m_bIsWMPSplitter)
           {
               if ((VC1_PROFILE_ADVANCED == pDec->m_pContext->m_seqLayerHeader.PROFILE)&&
                   (m_bIsNeedAddFrameSC))
                   SCoffset = VC1SCSIZE;
           }
           else if ((VC1_PROFILE_ADVANCED != pDec->m_pContext->m_seqLayerHeader.PROFILE))
               // special header (with frame size) in case of .rcv format
               SCoffset = -VC1FHSIZE;

           VC1FrameDescriptor* pPackDescriptorChild = m_pStore->GetLastDS();
           pPackDescriptorChild->m_bIsFieldAbsent = false;

           
           if ((pPackDescriptorChild->m_pContext->m_picLayerHeader->PTYPE != VC1_SKIPPED_FRAME)&&
                (VC1_PROFILE_ADVANCED == pDec->m_pContext->m_seqLayerHeader.PROFILE))
            {
                GetStartCodes_HW(in,pDec->m_bIsWMPSplitter,stShift);
                if (stShift) // we begin since start code frame in case of external MS splitter
                {
                    SCoffset -= *m_stCodes_VA->offsets;
                    pPackDescriptorChild->m_pContext->m_FrameSize -= *m_stCodes_VA->offsets;

                }
            }
            try
            {
                pPackDescriptorChild->PrepareVLDVABuffers(pDec->m_pContext->m_Offsets,
                                                          pDec->m_pContext->m_values,
                                                         (Ipp8u*)in->GetDataPointer() - SCoffset,
                                                          m_stCodes_VA + stShift);

            }
            catch(vc1_exception ex)
            {
                exception_type e_type = ex.get_exception_type();
                if ( mem_allocation_er == e_type)
                    return UMC_ERR_NOT_ENOUGH_BUFFER;
            }
            if (pPackDescriptorChild->m_pContext->m_picLayerHeader->PTYPE != VC1_SKIPPED_FRAME)
            {
                m_va->EndFrame();
            }
            in->MoveDataPointer(pPackDescriptorChild->m_pContext->m_FrameSize);

            if (pPackDescriptorChild->m_pContext->m_picLayerHeader->FCM == VC1_FieldInterlace && m_stCodes_VA->count < 2)
                pPackDescriptorChild->m_bIsFieldAbsent = true;


           if ((VC1_PROFILE_ADVANCED != pDec->m_pContext->m_seqLayerHeader.PROFILE))
           {
                pDec->m_pContext->m_seqLayerHeader.RNDCTRL = pPackDescriptorChild->m_pContext->m_seqLayerHeader.RNDCTRL;
           }

            return UMC_OK;

        }
#ifdef UMC_VA_DXVA
        virtual Status GetStatusReport(DXVA_Status_VC1 *pStatusReport)
        {
            UMC::Status sts = UMC_OK;
            {
                sts = m_va->ExecuteStatusReportBuffer((void*)pStatusReport, sizeof(DXVA_Status_VC1)*VC1_MAX_REPORTS);
                if (sts != UMC_OK)
                    return UMC_ERR_FAILED;

                for (Ipp32u i=0; i < VC1_MAX_REPORTS; i++)
                {
                    // status report presents
                    if (pStatusReport[i].StatusReportFeedbackNumber)
                    {
                        return sts;
                    }
                }
                return UMC_WRN_INFO_NOT_READY;
            }
        }
#endif
        template <class Descriptor>
        Status VC1DecodeFrame_VLD(VC1VideoDecoder* pDec, MediaData* in, VideoData* out_data)
        {
            Ipp32s SCoffset = 0;
            m_bIsNeedAddFrameSC = pDec->m_bIsNeedAddFrameSC;
            Descriptor* pPackDescriptorChild = NULL;
            Descriptor* pTempDescriptor = NULL;
            bool skip = false;

            if (pDec->m_bIsWMPSplitter)
            {
                if ((VC1_PROFILE_ADVANCED == pDec->m_pContext->m_seqLayerHeader.PROFILE)&&
                    (m_bIsNeedAddFrameSC))
                    SCoffset = VC1SCSIZE;
            }
            else if ((VC1_PROFILE_ADVANCED != pDec->m_pContext->m_seqLayerHeader.PROFILE))
                // special header (with frame size) in case of .rcv format
                SCoffset = -VC1FHSIZE;

            m_PostProcessing = pDec->m_PostProcessing;
            m_pStore = pDec->m_pStore;
            Status umcRes = UMC_ERR_NOT_ENOUGH_DATA;
            try
            {
                // Get FrameDescriptor for Packing Data
                 m_pStore->GetReadyDS(&pPackDescriptorChild);

                if (NULL == pPackDescriptorChild)
                    throw vc1_exception(internal_pipeline_error);


                pPackDescriptorChild->m_pContext->m_FrameSize = (Ipp32u)in->GetDataSize() + SCoffset;


                umcRes = pPackDescriptorChild->preProcData(pDec->m_pContext,
                                                           (Ipp32u) (pDec->m_frameData->GetDataSize() + SCoffset),
                                                           pDec->m_lFrameCount,
                                                           pDec->m_bIsWMPSplitter,
                                                           skip);
                if (UMC_OK != umcRes)
                {

                    if (UMC_ERR_NOT_ENOUGH_DATA == umcRes)
                        throw vc1_exception(invalid_stream);
                    else
                        throw vc1_exception(internal_pipeline_error);
                }
            }
            catch (vc1_exception ex)
            {
                if (invalid_stream == ex.get_exception_type())
                {
                    if (fast_err_detect == vc1_except_profiler::GetEnvDescript().m_Profile)
                        m_pStore->AddInvalidPerformedDS(pPackDescriptorChild);
                    else if (fast_decoding == vc1_except_profiler::GetEnvDescript().m_Profile)
                        m_pStore->ResetPerformedDS(pPackDescriptorChild);
                    else
                    {
                        // Error - let speak about it 
                        m_pStore->ResetPerformedDS(pPackDescriptorChild);
                        // need to free surfaces
                        if(!skip)
                        {
                            m_pStore->UnLockSurface(pDec->m_pContext->m_frmBuff.m_iCurrIndex);
                            m_pStore->UnLockSurface(pDec->m_pContext->m_frmBuff.m_iToSkipCoping);
                        }
                        // for smart decoding we should decode and reconstruct frame according to standard pipeline
                    }

                    return  UMC_ERR_NOT_ENOUGH_DATA;
                }
                else
                    return UMC_ERR_FAILED;
            }

            // if no RM - lets execute
            if ((!pPackDescriptorChild->m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG) &&
                (!pPackDescriptorChild->m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG) &&
                (!pPackDescriptorChild->m_pContext->m_seqLayerHeader.RANGERED))

            {
                umcRes =FillAndExecute(pDec, in);
            }

            // Update round ctrl field of seq header.
            pDec->m_pContext->m_seqLayerHeader.RNDCTRL = pPackDescriptorChild->m_pContext->m_seqLayerHeader.RNDCTRL;

            m_bLastFrameNeedDisplay = true;
            pDec->m_bLastFrameNeedDisplay = true;


            if (umcRes == UMC_WRN_INVALID_STREAM)
                m_bIsWarningStream = true;


            // only if not external allocator
            if (!pDec->m_pExtFrameAllocator)
            {
                if (!m_pStore->GetPerformedDS(&pTempDescriptor))
                    m_pStore->GetReadySkippedDS(&pTempDescriptor,true);
            }
            if (VC1_SKIPPED_FRAME == pPackDescriptorChild->m_pContext->m_picLayerHeader->PTYPE)
                out_data->SetFrameType(D_PICTURE); // means skipped 
            else
                out_data->SetFrameType(I_PICTURE);

             //if ((m_pStore->GetProcFramesNumber() < pDec->m_iMaxFramesInProcessing)&&
             //    (umcRes != UMC_ERR_INVALID_STREAM))
             //{
             //    umcRes = UMC_ERR_NOT_ENOUGH_DATA;
             //}
            if (UMC_OK == umcRes)
            {
                if (1 == pDec->m_lFrameCount)
                    umcRes = UMC_ERR_NOT_ENOUGH_DATA;
            }
            return umcRes;
        }
        // Start codes in original non-swapped buffer
        private:
        MediaDataEx::_MediaDataEx* m_stCodes_VA;
    };
} // namespace UMC

#endif // #if defined(UMC_VA)

#endif //__UMC_VC1_VIDEO_DECODER_VA_H
#endif // #ifndef UMC_RESTRICTED_CODE_VA
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
