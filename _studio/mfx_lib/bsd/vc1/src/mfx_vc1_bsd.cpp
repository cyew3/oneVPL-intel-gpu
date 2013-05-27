/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2011 Intel Corporation. All Rights Reserved.
//          MFX VC-1 BSD
//
*/

#include "mfx_common.h"


#if defined (MFX_ENABLE_VC1_VIDEO_BSD)

#include "mfx_vc1_dec_common.h"
#include "mfx_vc1_bsd.h"
#include "umc_vc1_dec_task_store.h"
#include "vm_sys_info.h"
#include "umc_vc1_dec_exception.h"
#include "umc_vc1_common.h"
#include "mfx_vc1_bsd_utils.h"
#include "mfx_vc1_enc_ex_param_buf.h"
#include "mfx_enc_common.h"
#include "mfx_common.h"


using namespace MFXVC1DecCommon;
using namespace UMC;
using namespace UMC::VC1Exceptions;
using namespace UMC::VC1Common;

MFXVideoBSDVC1::MFXVideoBSDVC1 (VideoCORE *core,mfxStatus* sts):VideoBSD(),
                                                                m_pCore(core),
                                                                m_pVideoParams(NULL),
                                                                m_pArrayTProcBSD(NULL),
                                                                m_CurrSlNum(0),
                                                                m_BufferSize(0),
                                                                m_bIsSliceCall(false),
                                                                m_pCUC(0),
                                                                m_pHeap(0),
                                                                m_pContext(0),
                                                                m_pdecoder(0),
                                                                m_iThreadDecoderNum(0),
                                                                m_iMemContextID(0),
                                                                m_iHeapID(0),
                                                                m_iFrameBufferID(0)
{
    *sts = MFX_ERR_NONE;
    //m_pVideoParams = new VC1VideoDecoderParams;
}
MFXVideoBSDVC1::~MFXVideoBSDVC1(void)
{
    Close();
}

mfxStatus MFXVideoBSDVC1::Init(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    MFX_CHECK_STS(CheckAndSetInitParams(par));

    m_MemoryAllocator.Init(0,m_pCore);

    mfxStatus       MFXSts = MFX_ERR_NONE;

    Ipp32u mbWidth = (par->mfx.FrameInfo.Width + 15)/VC1_PIXEL_IN_LUMA;
    Ipp32u mbHeight= (par->mfx.FrameInfo.Height  + 15)/VC1_PIXEL_IN_LUMA;

    Ipp32u FSize =  (mbHeight*VC1_PIXEL_IN_LUMA)*(mbWidth*VC1_PIXEL_IN_LUMA )+
                    ((mbHeight*VC1_PIXEL_IN_CHROMA )*(mbWidth*VC1_PIXEL_IN_CHROMA))*2;


    //threading support definition
    Ipp32u nAllowedThreadNumber = 1;//par->mfx.NumThread;

    if (par->mfx.NumThread < 0)
        nAllowedThreadNumber = 0;
    else if (par->mfx.NumThread  > 8)
        nAllowedThreadNumber = 8;

    //nAllowedThreadNumber = 1;

    m_iThreadDecoderNum = (0 == nAllowedThreadNumber) ? (vm_sys_info_get_cpu_num()) : (nAllowedThreadNumber);
    m_VideoParams.mfx.NumThread = (mfxU8)m_iThreadDecoderNum;

    // Memory for VC1Context
    MFX_CHECK_UMC_STS(ContextAllocation(mbWidth, mbHeight));


    if (par->mfx.CodecProfile == MFX_PROFILE_VC1_ADVANCED)
        m_pContext->m_seqLayerHeader->PROFILE = VC1_PROFILE_ADVANCED;
    else
        m_pContext->m_seqLayerHeader->PROFILE = VC1_PROFILE_MAIN;

    //Heap allocation
    {
        Ipp32u heapSize = CalculateHeapSize();
        mfxU8* ptr;
        // Need to replace with MFX allocator
        MFX_CHECK_UMC_STS(m_pCore->AllocBuffer(heapSize, 0, &m_iHeapID));
        m_pCore->LockBuffer(m_iHeapID, &ptr);
        new(m_pHeap) VC1TSHeap(ptr,heapSize);
    }
    //m_pHeap->s_new(&m_pVideoParams);

    // Set picture size in MB
    m_pContext->m_seqLayerHeader->widthMB = (mfxU16)mbWidth;
    m_pContext->m_seqLayerHeader->heightMB = (mfxU16)mbHeight;

    // let think that interlace pictures are acceptable
    m_pContext->m_seqLayerHeader->INTERLACE = 1;
    //tables for decoding
    MFX_CHECK_UMC_ALLOC(InitTables(m_pContext));

    // Internal buffer for swapped data
    MFX_CHECK_UMC_STS(CreateFrameBuffer(FSize));
    m_BufferSize = FSize;


    // create thread decoders for multithreading support
    {
        Ipp32u i;
        m_pHeap->s_new(&m_pdecoder,m_iThreadDecoderNum);
        MFX_CHECK_NULL_PTR1(m_pdecoder);
        memset(m_pdecoder, 0, sizeof(VC1ThreadDecoder**) * m_iThreadDecoderNum);


        for (i = 0; i < m_iThreadDecoderNum; i += 1)
        {
            m_pHeap->s_new(&m_pdecoder[i]);
            MFX_CHECK_NULL_PTR1( m_pdecoder[i]);

        }
        m_pMfxTQueueBsd = &m_MxfObj;
        m_pHeap->s_new<UMC::VC1TaskProcessor,
                       MfxVC1TaskProcessor,
                       VC1MfxTQueueBase>(&m_pArrayTProcBSD,m_iThreadDecoderNum,m_pMfxTQueueBsd);

        for (i = 0;i < m_iThreadDecoderNum;i += 1)
        {
            MFX_CHECK_UMC_STS(m_pdecoder[i]->Init(m_pContext, i, 0, 0,
                                                  &m_MemoryAllocator,
                                                  &m_pArrayTProcBSD[i]))
        }
    }
    return MFXSts;
}
mfxStatus MFXVideoBSDVC1::CheckAndSetInitParams(mfxVideoParam* pVideoParams)
{
    if (pVideoParams->mfx.CodecId != MFX_CODEC_VC1)
        return MFX_ERR_UNSUPPORTED;

    //if (pVideoParams->FunctionId != MFX_FUNCTION_BSD)
    //    return MFX_ERR_UNSUPPORTED;

    //FrameType; Should be filled

    m_VideoParams.mfx.CodecId = MFX_CODEC_VC1;
    m_VideoParams.mfx.CodecProfile = pVideoParams->mfx.CodecProfile;
    m_VideoParams.mfx.FrameInfo.Height = pVideoParams->mfx.FrameInfo.Height;
    m_VideoParams.mfx.FrameInfo.Width = pVideoParams->mfx.FrameInfo.Width;
    //m_VideoParams.FunctionId = MFX_FUNCTION_BSD;
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoBSDVC1::SetpExtBufs()
{
    // info from extended buffers
    // TBD
    //mfxI16  index  =  GetExBufferIndex(m_pCUC, MFX_LABEL_RESCOEFF, MFX_CUC_VC1_RESIDAUALBUF);
    mfxI16  index  =  0;
    MFX_CHECK_EXBUF_INDEX(index);
    ExtVC1Residual* pResidual = (ExtVC1Residual*)m_pCUC->ExtBuffer[index];
    MFX_CHECK_NULL_PTR1(pResidual);
    m_pContext->m_pBlock = pResidual->pRes;

    memset(m_pContext->m_pBlock, 0, sizeof(Ipp16s)*384*pResidual->numMbs);


    //saved MV. Debug ????
    //index = GetExBufferIndex(m_pCUC, MFX_LABEL_MVDATA, MFX_CUC_VC1_SAVEDMVBUF);
    index = 1;
    MFX_CHECK_EXBUF_INDEX(index);
    ExtVC1SavedMV* pSaved = (ExtVC1SavedMV*)m_pCUC->ExtBuffer[index];
    MFX_CHECK_NULL_PTR1(pSaved);
    m_pContext->savedMV = m_pContext->savedMV_Curr = pSaved->pMVs;

    ////direction
    //index = GetExBufferIndex(m_pCUC, MFX_LABEL_MVDATA, MFX_CUC_VC1_SAVEDMVDIRBUF);
    index = 2;
    MFX_CHECK_EXBUF_INDEX(index);
    ExtVC1SavedMVDirection* pDirSaved = (ExtVC1SavedMVDirection*)m_pCUC->ExtBuffer[index];
    MFX_CHECK_NULL_PTR1(pDirSaved);
    m_pContext->savedMVSamePolarity = m_pContext->savedMVSamePolarity_Curr = pDirSaved->bRefField;

    return MFX_ERR_NONE;

}
mfxStatus MFXVideoBSDVC1::Reset(mfxVideoParam* par)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    try
    {
        MFXSts = Close();
        MFX_CHECK_STS(MFXSts);
        MFXSts = Init(par);
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }

    return MFXSts;
}

mfxStatus MFXVideoBSDVC1::Close(void)
{
    if(m_pContext)
    {
        if (m_pContext->m_vlcTbl)
            FreeTables(m_pContext);
    }
    if(m_pdecoder)
    {
        for(Ipp32u i = 0; i < m_iThreadDecoderNum; i += 1)
        {
            if(m_pdecoder[i])
            {
                m_pdecoder[i] = 0;
            }
        }
        m_pdecoder = 0;
    }
    if(m_pCore)
    {
        if (m_iHeapID != 0)
        {
            m_pCore->UnlockBuffer(m_iHeapID);
            m_pCore->FreeBuffer(m_iHeapID);
            m_iHeapID = 0;
            //reset pointers, memory free throw external allocator
            m_pContext = 0;
        }
        if (m_iFrameBufferID != 0)
        {
            m_pCore->UnlockBuffer(m_iFrameBufferID);
            m_pCore->FreeBuffer(m_iFrameBufferID);
            m_iFrameBufferID = 0;
        }

    }
    //BaseCodec::Close(); // delete internal allocator if exists
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoBSDVC1::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(out);
    MFXVC1DecCommon::Query(in, out);
    return MFXSts;
}
mfxStatus MFXVideoBSDVC1::GetVideoParam(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_VideoParams;
    return  MFX_ERR_NONE;

}
mfxStatus MFXVideoBSDVC1::GetFrameParam(mfxFrameParam* par)
{
    mfxStatus  MFXSts = MFX_ERR_NONE;
    try
    {
        MFXSts = FillmfxPictureParams(m_pContext, par);
        MFX_CHECK_STS(MFXSts);
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }

    return MFXSts;
}
mfxStatus MFXVideoBSDVC1::GetSliceParam(mfxSliceParam* par)
{
    // Need to know about current slice processing (threading)
    MFX_CHECK_NULL_PTR1(par);

    if (!m_pCUC)
        return MFX_ERR_UNKNOWN;
    par = &m_pCUC->SliceParam[m_CurrSlNum];
    return MFX_ERR_NONE;

}
mfxStatus MFXVideoBSDVC1::RunVideoParam(mfxBitstream *bs, mfxVideoParam *par)
{
    mfxStatus     MFXSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);
    MFXSts = ParseSeqHeader(bs,par);
    MFX_CHECK_STS(MFXSts);
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoBSDVC1::RunFrameParam(mfxBitstream *bs, mfxFrameParam *par)
{
    mfxStatus     MFXSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);
    //MFXSts = ProcessFrameHeaderParams(bs);
    MFX_CHECK_STS(MFXSts);
    *par = m_FrameParams;
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoBSDVC1::RunSliceParam(mfxBitstream *bs, mfxSliceParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    return MFX_ERR_UNSUPPORTED;
}
mfxStatus MFXVideoBSDVC1::ExtractUserData(mfxBitstream *bs, mfxU8 *ud, mfxU32 *sz, mfxU64 *ts)
{
    MFX_CHECK_NULL_PTR1(bs);
    MFX_CHECK_NULL_PTR1(ud);
    MFX_CHECK_NULL_PTR1(sz);
    MFX_CHECK_NULL_PTR1(ts);

    return MFX_ERR_UNSUPPORTED;
}
mfxStatus MFXVideoBSDVC1::RunSliceBSD(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    m_bIsSliceCall = true;

    if(!m_pContext)
        return MFX_ERR_UNSUPPORTED;

    try
    {
        m_pCUC = cuc;
        SetpExtBufs();
        MFX_CHECK_STS(ParseMFXBitstream(m_pCUC->Bitstream));
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_UNSUPPORTED;
}
mfxStatus MFXVideoBSDVC1::RunSliceMFX(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    return MFX_ERR_UNSUPPORTED;
}
mfxStatus MFXVideoBSDVC1::RunFrameBSD(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    m_bIsSliceCall = false;

    if(!m_pContext)
        return MFX_ERR_UNSUPPORTED;

    try
    {
        m_pCUC = cuc;
        SetpExtBufs();
        MFX_CHECK_STS(ParseMFXBitstream(m_pCUC->Bitstream));
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;
}
mfxStatus MFXVideoBSDVC1::RunFrameMFX(mfxFrameCUC *cuc)
{
    MFX_CHECK_NULL_PTR1(cuc);
    return MFX_ERR_UNSUPPORTED;
}
Ipp32u MFXVideoBSDVC1::CalculateHeapSize()
{
    Ipp32u Size = 0;
    Ipp32u counter = 0;

    Size += align_value<Ipp32u>(sizeof(VC1ThreadDecoder**)*m_iThreadDecoderNum);

    for (counter = 0; counter < m_iThreadDecoderNum; counter += 1)
    {
        Size += align_value<Ipp32u>(sizeof(VC1ThreadDecoder));
        Size += align_value<Ipp32u>(sizeof(MfxVC1TaskProcessor));
    }
    Size += align_value<Ipp32u>(sizeof(MediaDataEx));
    return Size;
}
mfxStatus MFXVideoBSDVC1::ParseMFXBitstream(mfxBitstream *bs)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    Status umcRes = UMC_OK;


    if (VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader->PROFILE)
    {
        Ipp32u readSize;
        m_frameData->GetExData()->count = 0;
        memset(m_frameData->GetExData()->offsets, 0,START_CODE_NUMBER*sizeof(Ipp32s));
        memset(m_frameData->GetExData()->values, 0,START_CODE_NUMBER*sizeof(Ipp32s));
        MFXSts = GetStartCodes((Ipp8u*)(bs->Data + bs->DataOffset),
                                       (Ipp32u)(bs->DataLength),
                                       m_frameData,
                                       &readSize); // parse and copy to self buffer

        MFX_CHECK_STS(MFXSts);
        SwapData((Ipp8u*)m_frameData->GetDataPointer(), (Ipp32u)m_frameData->GetDataSize());
        m_pContext->m_FrameSize = readSize;
        MFXSts = InternalSCProcessing((Ipp8u*)m_frameData->GetDataPointer(),
                                      (Ipp32u)m_frameData->GetDataSize(),
                                       m_frameData->GetExData()->offsets,
                                       m_frameData->GetExData()->values);
        MFX_CHECK_UMC_STS(umcRes);
        //move data pointers
        bs->DataOffset += readSize;
        bs->DataLength -= readSize;



    }
    else //Simple/Main profiles pack without Start Codes
    {
        if (bs->DataLength < 8)
            return MFX_ERR_MORE_DATA; //TBD. May be change return type

        mfxU32 UnitSize = GetUnSizeSM(bs->Data + bs->DataOffset);

        if (UnitSize > bs->DataLength)
            return MFX_ERR_MORE_DATA; //TBD. May be change return type

        if (UnitSize > m_BufferSize)
            return MFX_ERR_NOT_ENOUGH_BUFFER;

        // copy data to self buffer
        ippsCopy_8u((Ipp8u*)(bs->Data + bs->DataOffset),
                             m_pDataBuffer,
                             UnitSize);

        m_pContext->m_FrameSize = UnitSize;
        m_frameData->SetDataSize(UnitSize);
        SwapData((Ipp8u*)m_frameData->GetDataPointer(), UnitSize);

        umcRes = SMProfilesProcessing(m_pDataBuffer);
        MFX_CHECK_UMC_STS(umcRes);

        //move data pointers
        bs->DataOffset += UnitSize;
        bs->DataLength -= UnitSize;

    }
    return MFXSts;
}
mfxStatus MFXVideoBSDVC1::InternalSCProcessing(Ipp8u*   pBStream,
                                               Ipp32u   DataSize,
                                               Ipp32u*  pOffsets,
                                               Ipp32u*  pValues)
{
    mfxStatus  MFXSts = MFX_ERR_NONE;
    Ipp32u readSize = 0;
    Ipp32u UnitSize;
    while ((*pValues != FrameHeader)&&
           (*pValues != Slice) &&
           (*pValues))
    {
        m_pContext->m_bitstream.pBitstream = (Ipp32u*)(pBStream + *pOffsets) + 1;//skip start code
        if(*(pOffsets + 1))
            UnitSize = *(pOffsets + 1) - *pOffsets;
        else
            UnitSize = DataSize - *pOffsets;

        readSize += UnitSize;
        m_pContext->m_bitstream.bitOffset  = 31;
        // May be need to process all prev headers
        switch (*pValues)
        {
        case SequenceHeader:
            SequenceLayer(m_pContext);
            m_pCUC->FrameParam->VC1.FrameType = 0xFF;
            return MFX_ERR_NONE;
        case EndOfSequence:
            m_pCUC->FrameParam->VC1.FrameType = 0xFF;
            return MFX_ERR_NONE;
         case EntryPointHeader:
            m_pCUC->FrameParam->VC1.FrameType = 0xFF;
            EntryPointLayer(m_pContext);
            return MFX_ERR_NONE;
        case SliceLevelUserData:
        case FieldLevelUserData:
        case FrameLevelUserData:
        case EntryPointLevelUserData:
        case SequenceLevelUserData:
            break;
        default:
            printf("incorrect start code suffix \n");
            return MFX_ERR_UNSUPPORTED;
            break;

        }
        pValues++;
        pOffsets++;
    }
    if (((FrameHeader) == *pValues)||
        ((Slice) == *pValues))// we have frame to decode
    {
        UnitSize = DataSize - *pOffsets;
        readSize += UnitSize;
        m_pContext->m_pBufferStart = (pBStream + *pOffsets); //skip start code

        try //work with queue of frame descriptors and process every frame
        {
            if (!m_bIsSliceCall)
            {
                m_pContext->m_Offsets = pOffsets;
                m_pContext->m_values = pValues;
            }
            else
            {
                *m_pContext->m_Offsets = pOffsets[m_pCUC->SliceParam->VC1.SliceId];
                *m_pContext->m_values = pValues[m_pCUC->SliceParam->VC1.SliceId];
            }
            ProcessOneUnit();
        }
        catch (vc1_exception ex)
        {
            exception_type e_type = ex.get_exception_type();
            if (e_type == internal_pipeline_error)
                return MFX_ERR_UNSUPPORTED;
        }
    }
    return  MFXSts;
}
mfxStatus MFXVideoBSDVC1::SMProfilesProcessing(Ipp8u* pBitstream)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    m_pContext->m_bitstream.pBitstream = (Ipp32u*)pBitstream;
    m_pContext->m_bitstream.bitOffset  = 31;
    if ((*m_pContext->m_bitstream.pBitstream&0xFF) == 0xC5)

    {
        //Close();
        //MFXSts = Init(&params);

        Ipp32s seq_size = 0;
        Ipp8u* seqStart = NULL;
        Ipp32u height;
        Ipp32u width;

        SwapData(m_pContext->m_pBufferStart, m_pContext->m_FrameSize);

        seqStart = m_pContext->m_pBufferStart + 4;
        seq_size  = ((*(seqStart+3))<<24) + ((*(seqStart+2))<<16) + ((*(seqStart+1))<<8) + *(seqStart);


        seqStart = m_pContext->m_pBufferStart + 8 + seq_size;
        height  = ((*(seqStart+3))<<24) + ((*(seqStart+2))<<16) + ((*(seqStart+1))<<8) + *(seqStart);
        seqStart+=4;
        width  = ((*(seqStart+3))<<24) + ((*(seqStart+2))<<16) + ((*(seqStart+1))<<8) + *(seqStart);

        m_pContext->m_seqLayerHeader->widthMB  = (Ipp16u)((width+15)/VC1_PIXEL_IN_LUMA);
        m_pContext->m_seqLayerHeader->heightMB = (Ipp16u)((height+15)/VC1_PIXEL_IN_LUMA);
        m_pContext->m_seqLayerHeader->MAX_CODED_HEIGHT = height/2 - 1;
        m_pContext->m_seqLayerHeader->MAX_CODED_WIDTH = width/2 - 1;

        SwapData(m_pContext->m_pBufferStart, m_pContext->m_FrameSize);

        m_pContext->m_bitstream.pBitstream = (Ipp32u*)m_pContext->m_pBufferStart + 2; // skip header
        m_pContext->m_bitstream.bitOffset = 31;

        SequenceLayer(m_pContext);

        m_pCUC->FrameParam->VC1.FrameType = 0xFF;

        MFX_CHECK_STS(MFXSts);
        return MFXSts;
    }

    m_pContext->m_pBufferStart  = (Ipp8u*)pBitstream;

    try //work with queue of frame descriptors and process every frame
    {
        ProcessOneUnit();
    }
    catch (vc1_exception ex)
    {
        exception_type e_type = ex.get_exception_type();
        if (e_type == internal_pipeline_error)
            return MFX_ERR_UNKNOWN;
    }
    return MFXSts;

}
mfxStatus MFXVideoBSDVC1::ProcessFrameHeaderParams(mfxBitstream *bs)
{
    mfxStatus  MFXSts = MFX_ERR_NONE;
//    VC1FrameDescriptor *pCurrDescriptor = NULL;
//    m_pStore->GetReadyDS(&pCurrDescriptor);
//    MFX_CHECK_NULL_PTR1(pCurrDescriptor);
//    Ipp32u readSize;
//
//    if (VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader->PROFILE)
//    {
//        m_frameData->GetExData()->count = 0;
//        memset(m_frameData->GetExData()->offsets, 0,START_CODE_NUMBER*sizeof(Ipp32s));
//        memset(m_frameData->GetExData()->values, 0,START_CODE_NUMBER*sizeof(Ipp32s));
//
//        MFX_CHECK_UMC_STS(GetStartCodes((Ipp8u*)(bs->Data + bs->DataOffset),
//                                        (Ipp32u)(bs->DataLength),
//                                         m_frameData,
//                                         &readSize)); // parse and copy to self buffer
//
//        SwapData((Ipp8u*)m_frameData->GetDataPointer(), (Ipp32u)m_frameData->GetDataSize());
//        m_pContext->m_FrameSize = readSize;
//        m_pContext->m_pBufferStart = (Ipp8u*)m_frameData->GetDataPointer();
//    }
//    else
//    {
//        // copy data to self buffer
//        ippsCopy_8u((Ipp8u*)(bs->Data + bs->DataOffset),
//                     m_dataBuffer,
//                    (Ipp32u)(bs->DataLength));
//
//        m_pContext->m_FrameSize = (Ipp32u)(bs->DataLength);
//        m_frameData->SetDataSize(m_pContext->m_FrameSize);
//        SwapData((Ipp8u*)m_frameData->GetDataPointer(), (Ipp32u)m_frameData->GetDataSize());
//        m_pContext->m_pBufferStart = (Ipp8u*)m_frameData->GetDataPointer();
//
////        umcRes = SMProfilesProcessing(m_dataBuffer);
//    }
//
//    MFX_CHECK_UMC_STS(pCurrDescriptor->preProcData(m_pContext->m_pBufferStart,
//                                                   GetCurrentFrameSize(),
//                                                   m_lFrameCount,
//                                                   false));
//
//    if (!pCurrDescriptor->isEmpty())//check descriptor correctness
//    {
//        pCurrDescriptor->VC1FrameDescriptor::processFrame(m_pContext->m_Offsets,
//                                                          m_pContext->m_values);
//        MFX_CHECK_STS(FillmfxPictureParams(pCurrDescriptor->m_pContext,&m_FrameParams));
//    }
//    else
//        return MFX_ERR_NOT_ENOUGH_BUFFER;

    return MFXSts;
}
mfxStatus MFXVideoBSDVC1::ProcessOneUnit()
{
    mfxStatus  MFXSts = MFX_ERR_NONE;
    //VC1FrameDescriptor *pCurrDescriptor = NULL;
    //m_pStore->GetReadyDS(&pCurrDescriptor);
    //MFX_CHECK_NULL_PTR1(pCurrDescriptor);

    MFX_CHECK_UMC_STS(PictureParamsPreParsing());
    MFX_CHECK_UMC_STS(CreateQueueOfTasks(m_pContext->m_Offsets,
                                         m_pContext->m_values));

    //MFX_CHECK_UMC_STS(pCurrDescriptor->preProcData(m_pContext->m_pBufferStart,
    //                                               GetCurrentFrameSize(),
    //                                               m_lFrameCount,
    //                                               m_bIsWMPSplitter));

    //if (!pCurrDescriptor->isEmpty())//check descriptor correctness
    //{
    //    pCurrDescriptor->VC1FrameDescriptor::processFrame(m_pContext->m_Offsets,
    //                                                      m_pContext->m_values);
    //    m_pStore->DistributeTasks(pCurrDescriptor->GetID());
    //}
    //else
    //    return MFX_ERR_NOT_ENOUGH_BUFFER;

    //m_pStore->SetDstForBSDUnit(pCurrDescriptor);

    // Wake Upppp
    //m_pStore->WakeUP();
    //m_pStore->StartDecoding();
    for (Ipp32u i = 1;i < m_iThreadDecoderNum;i += 1)
        m_pdecoder[i]->StartProcessing();

    m_pdecoder[0]->processMainThread();

    // other threads sleep, we can exit safety


    return  MFXSts;
}
mfxStatus MFXVideoBSDVC1::FillSliceParams(VC1Context* pContext,
                                          Ipp32u StartRow,
                                          Ipp32u RowToDecode)
{
    MFX_CHECK_NULL_PTR1(pContext);
    mfxSliceParam* pSlice = &m_pCUC->SliceParam[m_CurrSlNum];
    pSlice->VC1.FirstMbX = 0;
    pSlice->VC1.FirstMbY = (mfxU8)StartRow;
    pSlice->VC1.NumMb = (mfxU16)(RowToDecode * pContext->m_seqLayerHeader->widthMB);
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoBSDVC1::PictureParamsPreParsing()
{
    mfxStatus  MFXSts = MFX_ERR_NONE;

    m_pContext->m_bitstream.pBitstream = (Ipp32u*)m_pContext->m_pBufferStart;

    if ((m_pContext->m_seqLayerHeader->PROFILE == VC1_PROFILE_ADVANCED)) // skip start code
        m_pContext->m_bitstream.pBitstream += 1;
    else // skip frame header
        m_pContext->m_bitstream.pBitstream += 2;

    m_pContext->m_bitstream.bitOffset = 31;
    m_pContext->m_picLayerHeader = m_pContext->m_InitPicLayer;

    if (m_pContext->m_seqLayerHeader->PROFILE == VC1_PROFILE_ADVANCED)
    {
        m_pContext->m_bNeedToUseCompBuffer = 0;
        GetNextPicHeader_Adv(m_pContext);
    }
    else
    {
        GetNextPicHeader(m_pContext, false);
    }
    return MFXSts;
}

mfxStatus MFXVideoBSDVC1::CreateQueueOfTasks(Ipp32u*  pOffsets,
                                             Ipp32u*  pValues)
{
    mfxStatus  MFXSts = MFX_ERR_NONE;
    MfxVC1ThreadUnitParamsBSD tUnitParams;

    Ipp32u temp_value = 0;
    Ipp32u* bitstream;
    Ipp32s bitoffset = 31;

    bool isSecondField = false;

    Ipp16u heightMB = m_pContext->m_seqLayerHeader->heightMB;

    m_pCUC->NumMb = m_pContext->m_seqLayerHeader->widthMB * m_pContext->m_seqLayerHeader->heightMB;

    if ((m_pContext->m_picLayerHeader->FCM != VC1_FieldInterlace)&&
        (m_pContext->m_seqLayerHeader->IsResize))
        heightMB -= 1;

    tUnitParams.BaseSlice.MBStartRow = 0;
    tUnitParams.BaseSlice.MBEndRow = heightMB;

    if (m_pContext->m_picLayerHeader->PTYPE == VC1_SKIPPED_FRAME)
    {
        MFXSts = FillmfxPictureParams(m_pContext, m_pCUC->FrameParam);
        return MFXSts;
    }


    if (m_pContext->m_seqLayerHeader->PROFILE == VC1_PROFILE_ADVANCED)
        DecodePicHeader(m_pContext);
    else
        Decode_PictureLayer(m_pContext);

    MFXSts = FillmfxPictureParams(m_pContext, m_pCUC->FrameParam);
    MFX_CHECK_STS(MFXSts);


    tUnitParams.pBS = m_pContext->m_bitstream.pBitstream;
    tUnitParams.BitOffset = m_pContext->m_bitstream.bitOffset;
    tUnitParams.pPicLayerHeader = m_pContext->m_picLayerHeader;
    tUnitParams.pVlcTbl = m_pContext->m_vlcTbl;
    tUnitParams.BaseSlice.pContext = m_pContext;
    tUnitParams.BaseSlice.pCUC = m_pCUC;


    if (m_pContext->m_seqLayerHeader->PROFILE != VC1_PROFILE_ADVANCED)
    {
        tUnitParams.BaseSlice.MBRowsToDecode = tUnitParams.BaseSlice.MBEndRow - tUnitParams.BaseSlice.MBStartRow;
        m_MxfObj.FormalizeSliceTaskGroup(&tUnitParams);
        FillSliceParams(m_pContext,tUnitParams.BaseSlice.MBStartRow,tUnitParams.BaseSlice.MBRowsToDecode);
        m_CurrSlNum = 1;
        m_pCUC->NumSlice = (mfxU16)m_CurrSlNum;
        return MFXSts;
    }

    if (*(pValues+1) == 0x0B010000)
    {
        bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets+1));
        VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
        bitoffset = 31;
        VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, tUnitParams.BaseSlice.MBEndRow);     //SLICE_ADDR
        m_pContext->m_picLayerHeader->is_slice = 1;
    }
    else if(*(pValues+1) == 0x0C010000)
    {
         tUnitParams.pPicLayerHeader->CurrField = 0;
         tUnitParams.pPicLayerHeader->PTYPE = m_pContext->m_picLayerHeader->PTypeField1;
         tUnitParams.pPicLayerHeader->BottomField = (Ipp8u)(1 - m_pContext->m_picLayerHeader->TFF);
         tUnitParams.BaseSlice.MBEndRow = heightMB/2;
    }


    tUnitParams.BaseSlice.MBRowsToDecode = tUnitParams.BaseSlice.MBEndRow - tUnitParams.BaseSlice.MBStartRow;
    MFXSts = m_MxfObj.FormalizeSliceTaskGroup(&tUnitParams);
    MFX_CHECK_STS(MFXSts);
    MFXSts = FillSliceParams(m_pContext,
                             tUnitParams.BaseSlice.MBStartRow,
                             tUnitParams.BaseSlice.MBRowsToDecode);
    MFX_CHECK_STS(MFXSts);

    pOffsets++;
    pValues++;
    m_CurrSlNum++;

    while (*pOffsets)
    {
        if (*(pValues) == 0x0C010000)
        {
            isSecondField = true;
            m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *pOffsets);
            m_pContext->m_bitstream.pBitstream += 1; // skip start code
            m_pContext->m_bitstream.bitOffset = 31;
            //m_pContext->m_picLayerHeader = m_pContext->m_InitPicLayer + 1;
            ++m_pContext->m_picLayerHeader;
            *m_pContext->m_picLayerHeader = *m_pContext->m_InitPicLayer;
            m_pContext->m_picLayerHeader->BottomField = (Ipp8u)m_pContext->m_InitPicLayer->TFF;
            m_pContext->m_picLayerHeader->PTYPE = m_pContext->m_InitPicLayer->PTypeField2;
            m_pContext->m_picLayerHeader->CurrField = 1;

            m_pContext->m_picLayerHeader->is_slice = 0;
            DecodePicHeader(m_pContext);

            tUnitParams.BaseSlice.MBStartRow = heightMB/2;

            if (*(pOffsets+1) && *(pValues+1) == 0x0B010000)
            {
                bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets+1));
                VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
                bitoffset = 31;
                VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, tUnitParams.BaseSlice.MBEndRow);
            } else
                tUnitParams.BaseSlice.MBEndRow = heightMB;

            tUnitParams.pBS = m_pContext->m_bitstream.pBitstream;
            tUnitParams.BitOffset = m_pContext->m_bitstream.bitOffset;
            tUnitParams.pPicLayerHeader = m_pContext->m_picLayerHeader;
            tUnitParams.pVlcTbl = m_pContext->m_vlcTbl;
            tUnitParams.BaseSlice.MBRowsToDecode = tUnitParams.BaseSlice.MBEndRow -
                                                   tUnitParams.BaseSlice.MBStartRow;
            MFXSts = m_MxfObj.FormalizeSliceTaskGroup(&tUnitParams);
            MFX_CHECK_STS(MFXSts);
            MFXSts = FillSliceParams(m_pContext,
                                     tUnitParams.BaseSlice.MBStartRow,
                                     tUnitParams.BaseSlice.MBRowsToDecode);
            MFX_CHECK_STS(MFXSts);
            pOffsets++;
            pValues++;
            m_CurrSlNum++;
        }
        else if (*(pValues) == 0x0B010000)
        {
            m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *pOffsets);
            VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,32, temp_value);
            m_pContext->m_bitstream.bitOffset = 31;

            VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,9, tUnitParams.BaseSlice.MBStartRow);     //SLICE_ADDR
            VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,1, temp_value);            //PIC_HEADER_FLAG
            if (temp_value == 1)                //PIC_HEADER_FLAG
            {
                ++m_pContext->m_picLayerHeader;
                if (isSecondField)
                    m_pContext->m_picLayerHeader->CurrField = 1;
                else
                    m_pContext->m_picLayerHeader->CurrField = 0;
                DecodePictureHeader_Adv(m_pContext);
                DecodePicHeader(m_pContext);
            }
            m_pContext->m_picLayerHeader->is_slice = 1;
            if (*(pOffsets+1) && *(pValues+1) == 0x0B010000)
            {
                bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets+1));
                VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
                bitoffset = 31;
                VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, tUnitParams.BaseSlice.MBEndRow);

            }
            else if(*(pValues+1) == 0x0C010000)
                tUnitParams.BaseSlice.MBEndRow = heightMB/2;
            else
                tUnitParams.BaseSlice.MBEndRow = heightMB;

            tUnitParams.pBS = m_pContext->m_bitstream.pBitstream;
            tUnitParams.BitOffset = m_pContext->m_bitstream.bitOffset;
            tUnitParams.pPicLayerHeader = m_pContext->m_picLayerHeader;
            tUnitParams.pVlcTbl = m_pContext->m_vlcTbl;
            tUnitParams.BaseSlice.MBRowsToDecode = tUnitParams.BaseSlice.MBEndRow -
                                                   tUnitParams.BaseSlice.MBStartRow;

            MFXSts = m_MxfObj.FormalizeSliceTaskGroup(&tUnitParams);
            MFX_CHECK_STS(MFXSts);
            MFXSts = FillSliceParams(m_pContext,
                                          tUnitParams.BaseSlice.MBStartRow,
                                          tUnitParams.BaseSlice.MBRowsToDecode);
            MFX_CHECK_STS(MFXSts);
            tUnitParams.BaseSlice.MBStartRow = tUnitParams.BaseSlice.MBEndRow;
            pOffsets++;
            pValues++;
            m_CurrSlNum++;
        }
        else
        {
            pOffsets++;
            pValues++;
        }
    }
    m_pCUC->NumSlice = (mfxU16)m_CurrSlNum;
    m_CurrSlNum = 0;
    return MFXSts;
 }
mfxStatus MFXVideoBSDVC1::ContextAllocation(Ipp32u mbWidth,Ipp32u mbHeight)
{
    // need to extend for threading case
    if (!m_pContext)
    {
        mfxU8* t_ptr;

        Ipp8u* ptr = NULL;
        ptr += align_value<Ipp32u>(sizeof(VC1Context));
        ptr += align_value<Ipp32u>(sizeof(VC1SequenceLayerHeader));
        ptr += align_value<Ipp32u>(sizeof(VC1MB)*mbHeight*mbWidth);
        ptr += align_value<Ipp32u>(sizeof(VC1PictureLayerHeader)*VC1_MAX_SLICE_NUM);
        ptr += align_value<Ipp32u>((mbHeight*mbWidth*VC1_MAX_BITPANE_CHUNCKS));
        ptr += align_value<Ipp32u>(sizeof(VC1DCMBParam)*mbHeight*mbWidth);
        ptr += align_value<Ipp32u>(sizeof(VC1VLCTables));
        ptr += align_value<Ipp32u>(sizeof(VC1TSHeap));
        if(m_stCodes == NULL)
        {
            ptr += align_value<Ipp32u>(START_CODE_NUMBER*2*sizeof(Ipp32u)+sizeof(MediaDataEx::_MediaDataEx));
        }

        // Need to replace with MFX allocator
        MFX_CHECK_STS(m_pCore->AllocBuffer((size_t)ptr, 0, &m_iMemContextID));

        m_pCore->LockBuffer(m_iMemContextID,&t_ptr);
        m_pContext = (VC1Context*)t_ptr;
        memset(m_pContext,0,(size_t)ptr);
        ptr = (Ipp8u*)m_pContext;

        ptr += align_value<Ipp32u>(sizeof(VC1Context));
        m_pContext->m_seqLayerHeader = (VC1SequenceLayerHeader*)ptr;

        ptr += align_value<Ipp32u>(sizeof(VC1SequenceLayerHeader));
        m_pContext->m_MBs = (VC1MB*)ptr;

        ptr += align_value<Ipp32u>(sizeof(VC1MB)*mbHeight*mbWidth);
        m_pContext->m_InitPicLayer = (VC1PictureLayerHeader*)ptr;
        m_pContext->m_picLayerHeader = m_pContext->m_InitPicLayer;

        ptr += align_value<Ipp32u>(sizeof(VC1PictureLayerHeader)*VC1_MAX_SLICE_NUM);
        m_pContext->m_pBitplane.m_databits = ptr;

        ptr += align_value<Ipp32u>(mbHeight*mbWidth*VC1_MAX_BITPANE_CHUNCKS);
        m_pContext->DCACParams = (VC1DCMBParam*)ptr;

        ptr += align_value<Ipp32u>(sizeof(VC1DCMBParam)*mbHeight*mbWidth);
        m_pContext->m_vlcTbl = (VC1VLCTables*)ptr;

        ptr +=  align_value<Ipp32u>(sizeof(VC1VLCTables));
        m_pHeap = (VC1TSHeap*)ptr;

        if(m_stCodes == NULL)
        {
            ptr += align_value<Ipp32u>(sizeof(VC1TSHeap));
            m_stCodes = (MediaDataEx::_MediaDataEx *)(ptr);


            memset(m_stCodes, 0, (START_CODE_NUMBER*2*sizeof(Ipp32s)+sizeof(MediaDataEx::_MediaDataEx)));
            m_stCodes->count      = 0;
            m_stCodes->index      = 0;
            m_stCodes->bstrm_pos  = 0;
            m_stCodes->offsets    = (Ipp32u*)((Ipp8u*)m_stCodes +
            sizeof(MediaDataEx::_MediaDataEx));
            m_stCodes->values     = (Ipp32u*)((Ipp8u*)m_stCodes->offsets +
            START_CODE_NUMBER*sizeof( Ipp32u));
        }

    }
    return MFX_ERR_NONE;
}
mfxU32 MFXVideoBSDVC1::GetUnSizeSM(mfxU8* pData)
{
    mfxU32 FrameSize;
    mfxU8* ptemp;

    if ((*(pData+3) == 0xC5))
    {
        FrameSize  = 4;
        ptemp = pData +  4;
        mfxU32 temp_size = ((*(ptemp+3))<<24) + ((*(ptemp+2))<<16) + ((*(ptemp+1))<<8) + *(ptemp);


        FrameSize += temp_size;
        FrameSize +=12;
        ptemp = pData +  FrameSize;
        temp_size = ((*(ptemp+3))<<24) + ((*(ptemp+2))<<16) + ((*(ptemp+1))<<8) + *(ptemp);
        FrameSize += temp_size;
        FrameSize +=4;
    }
    else
    {
        FrameSize  = ((*(pData+3))<<24) + ((*(pData+2))<<16) + ((*(pData+1))<<8) + *(pData);
        FrameSize &= 0x0fffffff;
        FrameSize += 8;
    }
    return FrameSize;
}
mfxStatus MFXVideoBSDVC1::CreateFrameBuffer(mfxU32 bufferSize)
{
    if(m_pDataBuffer == NULL)
    {
       MFX_CHECK_STS(m_pCore->AllocBuffer(bufferSize, 0, &m_iFrameBufferID));
       (m_pCore->LockBuffer(m_iFrameBufferID, &m_pDataBuffer));
    }

    memset(m_pDataBuffer,0,bufferSize);
    m_pContext->m_pBufferStart  = (Ipp8u*)m_pDataBuffer;
    m_pContext->m_bitstream.pBitstream       = (Ipp32u*)(m_pContext->m_pBufferStart);

    if(m_frameData == NULL)
    {
        m_pHeap->s_new(&m_frameData);
    }
    m_frameData->SetBufferPointer(m_pDataBuffer, bufferSize);
    m_frameData->SetDataSize(bufferSize);
    m_frameData->SetExData(m_stCodes);

    return MFX_ERR_NONE;
}
mfxStatus MFXVideoBSDVC1::GetStartCodes (Ipp8u* pDataPointer,
                                         Ipp32u DataSize,
                                         MediaDataEx* out,
                                         Ipp32u* readSize)
{
    Ipp8u* readPos = pDataPointer;
    Ipp32u readBufSize =  DataSize;
    Ipp8u* readBuf = pDataPointer;

    Ipp8u* currFramePos = (Ipp8u*)out->GetBufferPointer();
    Ipp32u frameSize = 0;
    Ipp32u frameBufSize = (Ipp32u)out->GetBufferSize();
    MediaDataEx::_MediaDataEx *stCodes = out->GetExData();

    Ipp32u size = 0;
    Ipp8u* ptr = NULL;
    Ipp32u readDataSize = 0;
    Ipp32u zeroNum = 0;
    Ipp32u a = 0x0000FF00 | (*readPos);
    Ipp32u b = 0xFFFFFFFF;
    Ipp32u FrameNum = 0;

    Ipp32u shift = 0;

    memset(stCodes->offsets, 0,START_CODE_NUMBER*sizeof(Ipp32s));
    memset(stCodes->values, 0,START_CODE_NUMBER*sizeof(Ipp32s));

    while(readPos < (readBuf + readBufSize))
    {
        //find sequence of 0x000001 or 0x000003
        while(!( b == 0x00000001 || b == 0x00000003 )
            &&(readPos < (readBuf + readBufSize)))
        {
            readPos++;
            a = (a<<8)| (Ipp32s)(*readPos);
            b = a & 0x00FFFFFF;
        }

        //check end of read buffer
        if(readPos < (readBuf + readBufSize - 1))
        {
            if(*readPos == 0x01)
            {
                if((*(readPos + 1)  ==  VC1_Slice) ||
                    (*(readPos + 1) == VC1_Field) ||
                    (*(readPos + 1) == VC1_EntryPointHeader) ||
                    (*(readPos + 1) == VC1_FrameHeader ))
                    /*|| (FrameNum)*/
                {
                    readPos+=2;
                    ptr = readPos - 5;
                    if (stCodes->count) // if not first start code
                    {
                        //trim zero bytes
                        while ( (*ptr==0) && (ptr > readBuf) )
                            ptr--;
                    }

                    //slice or field size
                    size = (Ipp32u)(ptr - readBuf - readDataSize+1);

                    if(frameSize + size > frameBufSize)
                        return MFX_ERR_NOT_ENOUGH_BUFFER;

                    ippsCopy_8u(readBuf + readDataSize, currFramePos, size);

                    currFramePos = currFramePos + size;
                    frameSize = frameSize + size;

                    zeroNum = frameSize - 4*((frameSize)/4);
                    if(zeroNum!=0)
                        zeroNum = 4 - zeroNum;

                    memset(currFramePos, 0, zeroNum);

                    //set write parameters
                    currFramePos = currFramePos + zeroNum;
                    frameSize = frameSize + zeroNum;

                    stCodes->offsets[stCodes->count] = frameSize;
                    stCodes->values[stCodes->count]  = ((*(readPos-1))<<24) + ((*(readPos-2))<<16) + ((*(readPos-3))<<8) + (*(readPos-4));

                    readDataSize = (Ipp32u)(readPos - readBuf - 4);

                    a = 0x00010b00 |(Ipp32s)(*readPos);
                    b = a & 0x00FFFFFF;

                    zeroNum = 0;
                    stCodes->count++;
                }
                else
                {
                    if(FrameNum)
                    {
                        readPos+=2;
                        ptr = readPos - 5;
                        //trim zero bytes
                        if (stCodes->count) // if not first start code
                        {
                            while ( (*ptr==0) && (ptr > readBuf) )
                                ptr--;
                        }

                        //slice or field size
                        size = (Ipp32u)(readPos - readBuf - readDataSize - 4);

                        if(frameSize + size > frameBufSize)
                            return MFX_ERR_NOT_ENOUGH_BUFFER;

                        ippsCopy_8u(readBuf + readDataSize, currFramePos, size);

                        //currFramePos = currFramePos + size;
                        frameSize = frameSize + size;

                        stCodes->offsets[stCodes->count] = frameSize;
                        stCodes->values[stCodes->count]  = ((*(readPos-1))<<24) + ((*(readPos-2))<<16) + ((*(readPos-3))<<8) + (*(readPos-4));

                        stCodes->count++;

                        out->SetDataSize(frameSize + shift);
                        readDataSize = readDataSize + size;
                        //*readSize = readDataSize + shift;
                        *readSize = readDataSize;
                        return  MFX_ERR_NONE;
                    }
                    else
                    {
                        //beginning of frame
                        readPos++;
                        a = 0x00000100 |(Ipp32s)(*readPos);
                        b = a & 0x00FFFFFF;

                        stCodes->offsets[stCodes->count] = (Ipp32u)(0);
                        stCodes->values[stCodes->count]  = ((*(readPos))<<24) + ((*(readPos-1))<<16) + ((*(readPos-2))<<8) + (*(readPos-3));

                        stCodes->count++;
                        zeroNum = 0;
                        FrameNum++;
                    }
                }
            }
            else //if(*readPos == 0x03)
            {
                //000003
                if((*(readPos + 1) <  0x04))
                {
                    size = (Ipp32u)(readPos - readBuf - readDataSize);
                    if(frameSize + size > frameBufSize)
                        return MFX_ERR_NOT_ENOUGH_BUFFER;

                    ippsCopy_8u(readBuf + readDataSize, currFramePos, size);
                    frameSize = frameSize + size;
                    currFramePos = currFramePos + size;
                    zeroNum = 0;

                    readPos++;
                    a = (a<<8)| (Ipp32s)(*readPos);
                    b = a & 0x00FFFFFF;

                    readDataSize = readDataSize + size + 1;
                }
                else
                {
                    readPos++;
                    a = (a<<8)| (Ipp32s)(*readPos);
                    b = a & 0x00FFFFFF;
                }
            }

        }
        else
        {
            //end of stream
            size = (Ipp32u)(readPos- readBuf - readDataSize);

            if(frameSize + size > frameBufSize)
            {
                return MFX_ERR_NOT_ENOUGH_BUFFER;
            }

            ippsCopy_8u(readBuf + readDataSize, currFramePos, size);
            out->SetDataSize(frameSize + size + shift);

            readDataSize = readDataSize + size;
            //*readSize = readDataSize + shift;
            *readSize = readDataSize;
            return  MFX_ERR_NONE;
        }
    }
    return  MFX_ERR_NONE;
}


#endif
