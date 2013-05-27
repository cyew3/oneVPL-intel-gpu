/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2011 Intel Corporation. All Rights Reserved.
//          MFX VC-1 DEC
//
*/

#include "mfx_common.h"
#if defined (MFX_ENABLE_VC1_VIDEO_DEC)

#include "mfxvideo++int.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_common_mvdiff_tbl.h"
#include "mfx_memory_allocator.h"
#include "mfx_vc1_dec.h"
#include "mfx_vc1_dec_threading.h"
#include "umc_structures.h"
#include "vm_sys_info.h"
#include "mfx_vc1_enc_ex_param_buf.h"
#include "mfx_enc_common.h"
#include "mfx_vc1_dec_utils.h"



using namespace UMC;
using namespace MFXVC1DecCommon;


MFXVideoDECVC1::MFXVideoDECVC1(VideoCORE *core, mfxStatus* sts):VideoDEC(),
                                                m_pCore(core),
                                                m_pMfxTQueueDec(0), //Concrete type of tasks depend
                                                m_pCUC(0),
                                                m_pContext(0),
                                                m_iHeapID(0),
                                                m_iMemContextID(0),
                                                m_iPadFramesID(0),
                                                m_bIsSliceCall(false)
{
    m_pMfxTQueueDec = &m_MxfDecObj;
    *sts = MFX_ERR_NONE;
}


mfxStatus MFXVideoDECVC1::Init(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);

    Ipp32u mbWidth = (par->mfx.FrameInfo.Width + 15)/VC1_PIXEL_IN_LUMA;
    Ipp32u mbHeight= (par->mfx.FrameInfo.Height  + 15)/VC1_PIXEL_IN_LUMA;

    m_VideoParams = *par;


    //threading support definition
    Ipp32u nAllowedThreadNumber = par->mfx.NumThread;

    if (par->mfx.NumThread < 0)
        nAllowedThreadNumber = 0;
    else if (par->mfx.NumThread  > 8)
        nAllowedThreadNumber = 8;

    //nAllowedThreadNumber = 1;

    m_iThreadDecoderNum = (0 == nAllowedThreadNumber) ? (vm_sys_info_get_cpu_num()) : (nAllowedThreadNumber);

    if (!m_pContext)
    {
        Ipp8u* ptr = NULL;
        mfxI32 size;
        ptr += align_value<Ipp32u>(sizeof(VC1Context));
        ptr += align_value<Ipp32u>(sizeof(VC1SequenceLayerHeader));
        ptr += align_value<Ipp32u>(sizeof(VC1MB)*(mbHeight*mbWidth));
        ptr += align_value<Ipp32u>(sizeof(VC1PictureLayerHeader)*VC1_MAX_SLICE_NUM);
        ptr += align_value<Ipp32u>(sizeof(VC1DCMBParam)*mbHeight*mbWidth);


        size = (mfxI32)(ptr - (mfxU8*)NULL);
        // TBD
        //m_pCore->AllocBuffer(size,MFX_EXTMEM_SYSTEM_MEMORY,&m_iMemContextID);
        MFX_CHECK_NULL_PTR1(m_iMemContextID);


        m_pCore->LockBuffer(m_iMemContextID,(Ipp8u**)(&m_pContext));
        memset(m_pContext,0,size_t(ptr));
        ptr = (Ipp8u*)m_pContext;

        ptr += align_value<Ipp32u>(sizeof(VC1Context));
        m_pContext->m_seqLayerHeader = (VC1SequenceLayerHeader*)ptr;

        ptr +=  align_value<Ipp32u>(sizeof(VC1SequenceLayerHeader));
        m_pContext->m_MBs = (VC1MB*)ptr;

        ptr +=  align_value<Ipp32u>(sizeof(VC1MB)*(mbHeight*mbWidth));
        m_pContext->m_picLayerHeader = (VC1PictureLayerHeader*)ptr;
        m_pContext->m_InitPicLayer = m_pContext->m_picLayerHeader;

        ptr += align_value<Ipp32u>(sizeof(VC1PictureLayerHeader)*VC1_MAX_SLICE_NUM);
        m_pContext->DCACParams = (VC1DCMBParam*)ptr;
    }
    SetInitParams(par,m_pContext->m_seqLayerHeader);
    MFX_CHECK_STS(AllocatePadFrames());
    {
        Ipp32u Size = 0;
        Ipp32u counter = 0;

        Size += align_value<Ipp32u>(sizeof(VC1ThreadDecoder**)*m_iThreadDecoderNum);

        for (counter = 0; counter < m_iThreadDecoderNum; counter += 1)
        {
            Size += align_value<Ipp32u>(sizeof(VC1ThreadDecoder));
            Size += align_value<Ipp32u>(sizeof(MfxVC1TaskProcessor));
        }
        // TBD
        //m_pCore->AllocBuffer(Size,MFX_EXTMEM_SYSTEM_MEMORY,&m_iHeapID);
        m_pCore->AllocBuffer(Size,0,&m_iHeapID);
        // need to change for alloc with ext allocator
        m_pCore->LockBuffer(m_iHeapID,(Ipp8u**)&m_pHeap);
        m_pHeap = new UMC::VC1TSHeap((Ipp8u*)m_pHeap,Size);
    }

    // create thread decoders for multithreading support
    {
        Ipp32u i;
        m_pHeap->s_new(&m_pdecoder,m_iThreadDecoderNum);
        if (NULL == m_pdecoder)
            return MFX_ERR_MEMORY_ALLOC;

        memset(m_pdecoder, 0, sizeof(VC1ThreadDecoder*) * m_iThreadDecoderNum);

        for (i = 0; i < m_iThreadDecoderNum; i += 1)
        {
            m_pHeap->s_new(&m_pdecoder[i]);
            if (NULL == m_pdecoder[i])
                return MFX_ERR_MEMORY_ALLOC;

        }
        m_pHeap->s_new<UMC::VC1TaskProcessor,
                       MfxVC1TaskProcessor,
                       VC1MfxTQueueBase>(&m_pMfxDecTProc,m_iThreadDecoderNum,m_pMfxTQueueDec);



        for (i = 0;i < m_iThreadDecoderNum;i += 1)
        {
            if (UMC_OK != m_pdecoder[i]->Init(m_pContext, i, 0,
                                              0, 0, &m_pMfxDecTProc[i]))
                return MFX_ERR_MEMORY_ALLOC;
        }
    }
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::Reset(mfxVideoParam* par)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    MFXSts = Close();
    MFX_CHECK_STS(MFXSts);
    MFXSts = Init(par);
    return MFXSts;
}
mfxStatus MFXVideoDECVC1::Close(void)
{
    if (m_pContext->m_frmBuff.m_pFrames)
    {
        delete m_pContext->m_frmBuff.m_pFrames;
        m_pContext->m_frmBuff.m_pFrames = 0;
    }
    if (m_iHeapID)
    {
        m_pCore->UnlockBuffer(m_iHeapID);
        m_pCore->FreeBuffer(m_iHeapID);
        m_iHeapID = 0;
    }
    if (m_iMemContextID)
    {
        m_pCore->UnlockBuffer(m_iMemContextID);
        m_pCore->FreeBuffer(m_iMemContextID);
        m_iMemContextID = 0;
    }
    if (m_iPadFramesID)
    {
        m_pCore->UnlockBuffer(m_iPadFramesID);
        m_pCore->FreeBuffer(m_iPadFramesID);
        m_iPadFramesID = 0;
    }
    if (m_pHeap)
    {
        delete m_pHeap;
        m_pHeap = 0;
    }
    m_pContext = 0;
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(out);
    MFXVC1DecCommon::Query(in, out);
    return MFXSts;
}
mfxStatus MFXVideoDECVC1::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    *par = m_VideoParams;
    return  MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::GetFrameParam(mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    // only if we have processed CUC
    if (m_pCUC)
        *par = *m_pCUC->FrameParam;
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::GetSliceParam(mfxSliceParam *par)
{
    MFX_CHECK_NULL_PTR1(par);
    // only if we have processed CUC
    if (m_pCUC)
        *par = m_pCUC->SliceParam[0];
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::RunFrameFullDEC(mfxFrameCUC *cuc)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(cuc);
    try
    {
        mfxSts = UnPackFrameHeaders(cuc);
        MFX_CHECK_STS(mfxSts);
        if (!(cuc->FrameParam->VC1.FrameType & MFX_FRAMETYPE_S))
        {
            PrepareInternalFrames();
            mfxSts = ProcessUnit(cuc,Full);
            MFX_CHECK_STS(mfxSts);
        }
        PrepaeOutputPlanes();
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::RunFramePredDEC(mfxFrameCUC *cuc)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(cuc);
    try
    {
        if ((cuc->FrameParam->VC1.FrameType & MFX_FRAMETYPE_P)||
            (cuc->FrameParam->VC1.FrameType & MFX_FRAMETYPE_B)||
            (cuc->FrameParam->VC1.FrameType & MFX_FRAMETYPE_S))
        {

            mfxSts = UnPackFrameHeaders(cuc);
            MFX_CHECK_STS(mfxSts);
            if (!(cuc->FrameParam->VC1.FrameType & MFX_FRAMETYPE_S))
            {
                PrepareInternalFrames();
                mfxSts = ProcessUnit(cuc,PredDec);
                MFX_CHECK_STS(mfxSts);
            }
            PrepaeOutputPlanes();
        }
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::RunFrameIQT(mfxFrameCUC *cuc, mfxU8 scan)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(cuc);
    try
    {
        mfxSts = UnPackFrameHeaders(cuc);
        MFX_CHECK_STS(mfxSts);
        if (!(cuc->FrameParam->VC1.FrameType & MFX_FRAMETYPE_S))
        {
            mfxSts = ProcessUnit(cuc,IQT);
            MFX_CHECK_STS(mfxSts);
        }
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::RunFrameILDB(mfxFrameCUC *cuc)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(cuc);
    try
    {
        mfxSts = UnPackFrameHeaders(cuc);
        MFX_CHECK_STS(mfxSts);
        if (!(cuc->FrameParam->VC1.FrameType & MFX_FRAMETYPE_S))
        {
            MFX_CHECK_STS(ProcessUnit(cuc,RunILDBL));
        }
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::GetFrameRecon(mfxFrameCUC *cuc)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(cuc);
    try
    {
        mfxSts = UnPackFrameHeaders(cuc);
        MFX_CHECK_STS(mfxSts);
        if (!(cuc->FrameParam->VC1.FrameType & MFX_FRAMETYPE_S))
        {
            PrepareInternalFrames(); // !!!!!!!!!! need to change to pointers
            MFX_CHECK_STS(ProcessUnit(cuc,GetRecon));
            PrepaeOutputPlanes();  // !!!!!!!!!! need to change to pointers
        }
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::RunSliceFullDEC(mfxFrameCUC *cuc)
{
    //MFX_CHECK_NULL_PTR1(cuc);
    //MFX_CHECK_STS(SetpExtBufs(cuc));
    //PrepareInternalFrames();
    //MFX_CHECK_STS(ProcessUnit(cuc,Full));
    //PrepaeOutputPlanes();
    //return MFX_ERR_NONE;

    // identical to frame except slice = 1
    mfxStatus mfxSts = MFX_ERR_NONE;
    try
    {
        m_bIsSliceCall = true;
        mfxSts = RunFrameFullDEC(cuc);
        MFX_CHECK_STS(mfxSts);
        m_bIsSliceCall = false;
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::RunSlicePredDEC(mfxFrameCUC *cuc)
{
    //MFX_CHECK_NULL_PTR1(cuc);
    //MFX_CHECK_STS(SetpExtBufs(cuc));
    //PrepareInternalFrames();
    //MFX_CHECK_STS(ProcessUnit(cuc,PredDec));
    //PrepaeOutputPlanes();
    //return MFX_ERR_NONE;
    mfxStatus mfxSts = MFX_ERR_NONE;
    try
    {
        m_bIsSliceCall = true;
        mfxSts = RunFramePredDEC(cuc);
        MFX_CHECK_STS(mfxSts);
        m_bIsSliceCall = false;
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::RunSliceIQT(mfxFrameCUC *cuc, mfxU8 scan)
{
    //MFX_CHECK_NULL_PTR1(cuc);
    //MFX_CHECK_STS(SetpExtBufs(cuc));
    //MFX_CHECK_STS(ProcessUnit(cuc,IQT));
    //return MFX_ERR_NONE;
    mfxStatus mfxSts = MFX_ERR_NONE;
    try
    {
        m_bIsSliceCall = true;
        mfxSts = RunFrameIQT(cuc,scan);
        MFX_CHECK_STS(mfxSts);
        m_bIsSliceCall = false;
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::RunSliceILDB(mfxFrameCUC *cuc)
{
    //MFX_CHECK_NULL_PTR1(cuc);
    //MFX_CHECK_STS(SetpExtBufs(cuc));
    //PrepareInternalFrames();
    //MFX_CHECK_STS(ProcessUnit(cuc,RunILDBL));
    //PrepaeOutputPlanes();
    //return MFX_ERR_UNSUPPORTED;
    mfxStatus mfxSts = MFX_ERR_NONE;
    try
    {
        m_bIsSliceCall = true;
        mfxSts = RunFrameILDB(cuc);
        MFX_CHECK_STS(mfxSts);
        m_bIsSliceCall = false;
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::GetSliceRecon(mfxFrameCUC *cuc)
{
    //MFX_CHECK_NULL_PTR1(cuc);
    //MFX_CHECK_STS(SetpExtBufs(cuc));
    //PrepareInternalFrames();
    //MFX_CHECK_STS(ProcessUnit(cuc,IQT));
    //PrepaeOutputPlanes();
    //return MFX_ERR_UNSUPPORTED;
    mfxStatus mfxSts = MFX_ERR_NONE;
    try
    {
        m_bIsSliceCall = true;
        mfxSts = RunFrameILDB(cuc);
        MFX_CHECK_STS(mfxSts);
        m_bIsSliceCall = false;
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::SetInitParams(mfxVideoParam* par,
                                        VC1SequenceLayerHeader*   pSeqHeadr)
{
    if (par->mfx.CodecId != MFX_CODEC_VC1)
        return MFX_ERR_UNSUPPORTED;

    // TBD. Where is strongly correspondance brw FuctID and Real API
    //v 0.92 alredaye delete this fields
    //if ((par->FunctionId != MFX_FUNCTION_DEC)||
    //    (par->FunctionId != MFX_FUNCTION_DEC_ILDB))
    //    return MFX_ERR_UNSUPPORTED;

    switch (par->mfx.CodecProfile)
    {
    case MFX_PROFILE_VC1_SIMPLE:
        pSeqHeadr->PROFILE = VC1_PROFILE_SIMPLE;
        break;
    case MFX_PROFILE_VC1_MAIN:
        pSeqHeadr->PROFILE = VC1_PROFILE_MAIN;
        break;
    case MFX_PROFILE_VC1_ADVANCED:
        pSeqHeadr->PROFILE = VC1_PROFILE_ADVANCED;
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }
    pSeqHeadr->MAX_CODED_HEIGHT = par->mfx.FrameInfo.Height/2 -1;
    pSeqHeadr->MAX_CODED_WIDTH  = par->mfx.FrameInfo.Width/2 -1;

    pSeqHeadr->widthMB = (par->mfx.FrameInfo.Width + 15)/VC1_PIXEL_IN_LUMA;
    pSeqHeadr->heightMB = (par->mfx.FrameInfo.Height  + 15)/VC1_PIXEL_IN_LUMA;

    // TBD. Need to use mem. alloc. Remove in 0.92
    //if (par->FunctionId == MFX_FUNCTION_DEC)
    //{
    //    //m_pMfxTQueueDec = new VC1MfxTQueueDecFullDec;
    //    m_pMfxTQueueDec = &m_MxfDecObj;
    //}
    //// Only for testing needs
    //else if (par->FunctionId == MFX_FUNCTION_DEC_ILDB)
    //{
    //    m_pMfxTQueueDec = new VC1MfxTQueueDecParal<VC1DECPredILDB>;
    //}
    //else
    //    return MFX_ERR_UNSUPPORTED;

    m_pMfxTQueueDec = &m_MxfDecObj;


    m_iThreadDecoderNum = par->mfx.NumThread;

    // May be something add ?
    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECVC1::UnPackSeqHeader(mfxFrameCUC*  pCUC,
                                          VC1SequenceLayerHeader*   pSeqHeadr)
{
    pSeqHeadr->BROKEN_LINK = pCUC->FrameParam->VC1.BrokenLinkFlag;
    pSeqHeadr->CLOSED_ENTRY = pCUC->FrameParam->VC1.CloseEntryFlag;
    pSeqHeadr->QUANTIZER = pCUC->FrameParam->VC1.PicQuantizer;


    // as same as DXVA2
    if (VC1_PROFILE_ADVANCED == pSeqHeadr->PROFILE)
    {
        //if (VC1_FieldInterlace == pSeqHeadr->FCM)
        //    pSeqHeadr->heightMB = ((2*(pCUC->FrameParam->FrameHinMbMinus1 + 1) -1) + 15)/VC1_PIXEL_IN_LUMA;
        //else
            pSeqHeadr->heightMB = (pCUC->FrameParam->VC1.FrameHinMbMinus1 + 15)/VC1_PIXEL_IN_LUMA;

        pSeqHeadr->widthMB = (pCUC->FrameParam->VC1.FrameWinMbMinus1 + 15)/VC1_PIXEL_IN_LUMA;
    }
    else
    {
        pSeqHeadr->heightMB  = pCUC->FrameParam->VC1.FrameHinMbMinus1 + 1;
        pSeqHeadr->widthMB = pCUC->FrameParam->VC1.FrameWinMbMinus1 + 1;
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECVC1::UnPackPicHeader(mfxFrameCUC*  pCUC,
                                          VC1Context*   pContext)
{
    MFX_CHECK_NULL_PTR1(pCUC);
    MFX_CHECK_NULL_PTR1(pContext);

    mfxFrameParam* pFrameParam = pCUC->FrameParam;


    if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader->PROFILE)
    {
        // interlace fields
        if (0 == pFrameParam->VC1.PicExtrapolation)
        {
            pContext->m_picLayerHeader->FCM = VC1_Progressive;
            pContext->m_picLayerHeader->TFF = 0;
        }
        else if (1 == pFrameParam->VC1.PicExtrapolation)
        {
            pContext->m_picLayerHeader->FCM = VC1_FrameInterlace;
            pContext->m_picLayerHeader->TFF = 0;
        }
        else if (2 == pFrameParam->VC1.PicExtrapolation) // fields
        {
            pContext->m_picLayerHeader->CurrField = pFrameParam->VC1.SecondFieldFlag;
            if (pContext->m_picLayerHeader->CurrField == 0)
            {
                if (pFrameParam->VC1.BottomFieldFlag)
                {
                    pContext->m_picLayerHeader->TFF = 0;
                    pContext->m_picLayerHeader->BottomField = 1;
                }
                else
                {
                    pContext->m_picLayerHeader->TFF = 1;
                    pContext->m_picLayerHeader->BottomField = 0;
                }
            }
            else
            {
                if (pFrameParam->VC1.BottomFieldFlag)
                {
                    pContext->m_picLayerHeader->TFF = 1;
                    pContext->m_picLayerHeader->BottomField = 0;
                }
                else
                {
                    pContext->m_picLayerHeader->TFF = 0;
                    pContext->m_picLayerHeader->BottomField = 1;
                }
            }
        }
    }
    else
    {
        pContext->m_picLayerHeader->FCM = VC1_Progressive;
        pContext->m_picLayerHeader->TFF = 0;
    }

    switch(pFrameParam->VC1.FrameType & 0xF)
    {
    case MFX_FRAMETYPE_I:
        pContext->m_picLayerHeader->PTYPE = VC1_I_FRAME;
        break;
    case MFX_FRAMETYPE_P:
        pContext->m_picLayerHeader->PTYPE = VC1_P_FRAME;
        break;
    case MFX_FRAMETYPE_B:
        pContext->m_picLayerHeader->PTYPE = VC1_B_FRAME;
        break;
    case MFX_FRAMETYPE_S:
        pContext->m_picLayerHeader->PTYPE = VC1_SKIPPED_FRAME;
        break;
     default:
         break;
    }

    pContext->m_picLayerHeader->MV4SWITCH = pFrameParam->VC1.Pic4MvAllowed;

    //if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader->PROFILE)
        pContext->m_picLayerHeader->RNDCTRL = pFrameParam->VC1.RoundControl;
    //else
    //    pContext->m_seqLayerHeader->RNDCTRL = pFrameParam->VC1.RoundControl;

    // define Deblocking parameters

    pContext->m_picLayerHeader->LUMSCALE = pFrameParam->VC1.LumaScale;
    pContext->m_picLayerHeader->LUMSCALE1 = pFrameParam->VC1.LumaScale2;

    pContext->m_picLayerHeader->LUMSHIFT = pFrameParam->VC1.LumaShift;
    pContext->m_picLayerHeader->LUMSHIFT1 = pFrameParam->VC1.LumaShift2;

    //TBD
    //pContext->m_picLayerHeader->MVTYPEMB = bit_get(pFrameParam->VC1.RawCodingFlag, 0, 1);
    //pContext->m_picLayerHeader->m_DirectMB = bit_get(pFrameParam->VC1.RawCodingFlag, 1, 1);
    //pContext->m_picLayerHeader->SKIPMB = bit_get(pFrameParam->VC1.RawCodingFlag, 2, 1);
    //pContext->m_picLayerHeader->FIELDTX = bit_get(pFrameParam->VC1.RawCodingFlag, 3, 1);
    //pContext->m_picLayerHeader->FORWARDMB = bit_get(pFrameParam->VC1.RawCodingFlag, 4, 1);
    //pContext->m_picLayerHeader->ACPRED = bit_get(pFrameParam->VC1.RawCodingFlag, 5, 1);
    //pContext->m_picLayerHeader->OVERFLAGS = bit_get(pFrameParam->VC1.RawCodingFlag, 6, 1);

    pContext->m_picLayerHeader->TTFRM = 1 << pFrameParam->VC1.TTFRM;
    pContext->m_picLayerHeader->TTMBF = pFrameParam->VC1.TTMBF;
    pContext->m_picLayerHeader->PQUANT = pFrameParam->VC1.PQuant;

    pContext->m_picLayerHeader->BFRACTION = pFrameParam->VC1.CodedBfraction;
    pContext->m_picLayerHeader->QuantizationType = 1 - pFrameParam->VC1.PQuantUniform;

        //TBD- need to translate into AltPQuantConfig
    pContext->m_picLayerHeader->m_PQuant_mode = pFrameParam->VC1.AltPQuantConfig;
    pContext->m_picLayerHeader->HALFQP = pFrameParam->VC1.HalfQP;
    pContext->m_picLayerHeader->m_AltPQuant = pFrameParam->VC1.AltPQuant;

    pContext->m_picLayerHeader->CBPTAB = pFrameParam->VC1.CBPTable;
    pContext->m_picLayerHeader->TRANSDCTAB = pFrameParam->VC1.TransDCTable;
    pContext->m_picLayerHeader->TRANSACFRM =  pFrameParam->VC1.TransACTable;

    if ((VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE)||
        (VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE))
    {
        pContext->m_picLayerHeader->TRANSACFRM2 = pFrameParam->VC1.TransACTable2;
    }
    else
    {
        pContext->m_picLayerHeader->MBMODETAB = pFrameParam->VC1.MbModeTable;
        pContext->m_picLayerHeader->MVTAB = pFrameParam->VC1.MvTable;
        pContext->m_picLayerHeader->MV2BPTAB = pFrameParam->VC1.TwoMVBPTable;
        pContext->m_picLayerHeader->MV4BPTAB = pFrameParam->VC1.FourMVBPTable;
    }

    pContext->m_picLayerHeader->REFDIST = pFrameParam->VC1.RefDistance;
    pContext->m_picLayerHeader->NUMREF = pFrameParam->VC1.NumRefPic;
    pContext->m_picLayerHeader->REFFIELD = pFrameParam->VC1.RefFieldPicFlag;

    pContext->m_seqLayerHeader->FASTUVMC = pFrameParam->VC1.FastUVMCflag;
    //see Pic4MvAllowed
    pContext->m_picLayerHeader->MV4SWITCH = pFrameParam->VC1.FourMvSwitch;
    {
    // enum. Our
    //{
    //    VC1_MVMODE_HPELBI_1MV    = 0,    //0000      1 MV Half-pel bilinear
    //    VC1_MVMODE_1MV           = 1,    //1         1 MV
    //    VC1_MVMODE_MIXED_MV      = 2,    //01        Mixed MV
    //    VC1_MVMODE_HPEL_1MV      = 3,    //001       1 MV Half-pel
    //    VC1_MVMODE_INTENSCOMP    = 4,    //0001      Intensity Compensation
    //};
    // standard MFX
    //    UnifiedMvMode Description
    //    00b Mixed-MV
    //    01b 1-MV
    //    10b 1-MV half-pel
    //    11b 1-MV half-pel bilinear
    //
        static Ipp32u LTMVMode[4] = {2, 1, 3, 0};
        pContext->m_picLayerHeader->MVMODE = LTMVMode[pFrameParam->VC1.UnifiedMvMode];
    }

    if (pFrameParam->VC1.IntCompField)
    {
        //static Ipp32s IntCompTtable[3] = {-1, 3, 2}; TBD
        pContext->m_picLayerHeader->INTCOMFIELD = pFrameParam->VC1.IntCompField;
        pContext->m_bIntensityCompensation = 1;
    }
    else
    {
        pContext->m_bIntensityCompensation = 0;
    }

    pContext->m_seqLayerHeader->LOOPFILTER = MfxVC1DECUnpacking::bit_get(pFrameParam->VC1.PicDeblock, 2, 1);

    pContext->m_picLayerHeader->MVRANGE = pFrameParam->VC1.ExtendedMvRange;
    pContext->m_picLayerHeader->DMVRANGE = pFrameParam->VC1.ExtendedDMvRange;

    if (VC1_B_FRAME != pContext->m_picLayerHeader->PTYPE)
        pContext->m_seqLayerHeader->OVERLAP =  MfxVC1DECUnpacking::bit_get(pFrameParam->VC1.PicDeblocked, 5, 1); // overlap


    // mv range
    if (pContext->m_picLayerHeader->MVRANGE > 3)
        return MFX_ERR_UNKNOWN;

    pContext->m_picLayerHeader->m_pCurrMVRangetbl = &VC1_MVRangeTbl[pContext->m_picLayerHeader->MVRANGE];

    return MFX_ERR_NONE;

}

mfxStatus MFXVideoDECVC1::PrepareTasksForProcessing()
{
    mfxSliceParam* pSliceParam = m_pCUC->SliceParam;
    MfxVC1ThreadUnitParams tUnitParams;
    Ipp32u SliceCounter;

    //need to save some parameters from reference frames
    SZTables(m_pContext);
    if ((VC1_B_FRAME != m_pContext->m_picLayerHeader->PTYPE)&&
        (VC1_BI_FRAME != m_pContext->m_picLayerHeader->PTYPE))

    {
        if ((m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].ICFieldMask)||
            ((m_pContext->m_frmBuff.m_iPrevIndex > -1)&&
            (m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iPrevIndex].ICFieldMask)))
        {
            if (m_pContext->m_picLayerHeader->FCM ==  VC1_FieldInterlace)
                CreateComplexICTablesForFields(m_pContext);
            else
                CreateComplexICTablesForFrame(m_pContext);
        }
    }

    for (SliceCounter = 0; SliceCounter < m_pCUC->NumSlice; SliceCounter++)
    {
        tUnitParams.MBStartRow = pSliceParam[SliceCounter].VC1.FirstMbY;
        tUnitParams.MBRowsToDecode = pSliceParam[SliceCounter].VC1.NumMb / m_pContext->m_seqLayerHeader->widthMB;
        tUnitParams.MBEndRow = tUnitParams.MBStartRow + tUnitParams.MBRowsToDecode;
        tUnitParams.pContext = m_pContext;
        tUnitParams.pCUC = m_pCUC;
        tUnitParams.isFirstInSlice = true;
        m_MxfDecObj.FormalizeSliceTaskGroup(&tUnitParams);
    }
    return MFX_ERR_NONE;
 }
mfxStatus MFXVideoDECVC1::ProcessUnit(mfxFrameCUC *cuc,
                                      VC1DecEntryPoints point)
{
    m_MxfDecObj.SetEntrypoint(point);
    m_pCUC = cuc;
    PrepareTasksForProcessing();

    for (Ipp32u i = 1;i < m_iThreadDecoderNum;i += 1)
        m_pdecoder[i]->StartProcessing();

    m_pdecoder[0]->processMainThread();
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECVC1::SetpExtBufs(mfxFrameCUC*  pCUC)
{
    m_pCUC = pCUC;
    // Need to rewrite
    // info from extended buffers
    //mfxI16  index  =  GetExBufferIndex(m_pCUC, MFX_LABEL_RESCOEFF, MFX_CUC_VC1_RESIDAUALBUF);
    mfxI16  index = 0;
    MFX_CHECK_EXBUF_INDEX(index);
    ExtVC1Residual* pResidual = (ExtVC1Residual*)m_pCUC->ExtBuffer[index];
    MFX_CHECK_NULL_PTR1(pResidual);
    m_pContext->m_pBlock = pResidual->pRes;

    //saved MV. Debug ????
    //index = GetExBufferIndex(m_pCUC, MFX_LABEL_MVDATA, MFX_CUC_VC1_SAVEDMVBUF);
    index = 1;
    MFX_CHECK_EXBUF_INDEX(index);
    ExtVC1SavedMV* pSaved = (ExtVC1SavedMV*)m_pCUC->ExtBuffer[index];
    MFX_CHECK_NULL_PTR1(pSaved);
    //m_pContext->savedMV = pSaved->pMVs;
    m_pContext->savedMV_Curr = pSaved->pMVs;

    //direction
    //index = GetExBufferIndex(m_pCUC, MFX_LABEL_MVDATA, MFX_CUC_VC1_SAVEDMVDIRBUF);
    index = 2;
    MFX_CHECK_EXBUF_INDEX(index);
    ExtVC1SavedMVDirection* pDirSaved = (ExtVC1SavedMVDirection*)m_pCUC->ExtBuffer[index];
    MFX_CHECK_NULL_PTR1(pDirSaved);
    //m_pContext->savedMVSamePolarity = pDirSaved->bRefField;
    m_pContext->savedMVSamePolarity_Curr = pDirSaved->bRefField;

//    index = GetExBufferIndex(m_pCUC, MFX_LABEL_EXTHEADER, MFX_CUC_VC1_EXPANDPARAMSBUF);
    index = 3;
    MFX_CHECK_EXBUF_INDEX(index);
    ExtVC1RefFrameInfo* pFrInfo = (ExtVC1RefFrameInfo*)m_pCUC->ExtBuffer[index];
    m_pContext->m_frmBuff.m_pFrames[1].FCM = pFrInfo->FCMForward;
    m_pContext->m_frmBuff.m_pFrames[2].FCM = pFrInfo->FCMBackward;
    // TBD IC tables
    for (mfxU32 count = 0; count < 4; count++)
    {
        m_pContext->m_frmBuff.m_pFrames[1].LumaTablePrev[count] = 0;
        m_pContext->m_frmBuff.m_pFrames[1].ChromaTablePrev[count] = 0;

        m_pContext->m_frmBuff.m_pFrames[2].LumaTablePrev[count] = 0;
        m_pContext->m_frmBuff.m_pFrames[2].ChromaTablePrev[count] = 0;
    }
    m_pContext->m_frmBuff.m_pFrames[2].LumaTableCurr[0] = 0;
    m_pContext->m_frmBuff.m_pFrames[2].ChromaTableCurr[0] = 0;
    m_pContext->m_frmBuff.m_pFrames[2].LumaTableCurr[1] = 0;
    m_pContext->m_frmBuff.m_pFrames[2].ChromaTableCurr[1] = 0;




    return MFX_ERR_NONE;

}
// temporal needs for padding, before needed interpolation function implementation
mfxStatus MFXVideoDECVC1::AllocatePadFrames()
{
    if (!m_pContext->m_frmBuff.m_pFrames)
        m_pContext->m_frmBuff.m_pFrames = new Frame[3];
    return MFX_ERR_NONE;
}
void MFXVideoDECVC1::PrepareInternalFrames(void)
{
    //IppiSize  roiSize;
    mfxFrameParam* pFrameParam = m_pCUC->FrameParam;
    // Reference Frames
    bool isPredFrame = !VC1_IS_NOT_PRED(m_pContext->m_picLayerHeader->PTYPE);

    m_pContext->m_frmBuff.m_iCurrIndex = 0;
    m_pContext->m_frmBuff.m_iPrevIndex = 1;
    m_pContext->m_frmBuff.m_iNextIndex = 2;

    if (isPredFrame)
    {
        m_pContext->m_frmBuff.m_pFrames[1].m_pY = m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListP[0]]->Y;
        m_pContext->m_frmBuff.m_pFrames[1].m_iYPitch = m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListP[0]]->Pitch;
        // U
        m_pContext->m_frmBuff.m_pFrames[1].m_pU = m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListP[0]]->U;
        m_pContext->m_frmBuff.m_pFrames[1].m_iUPitch = m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListP[0]]->Pitch/2;

        m_pContext->m_frmBuff.m_pFrames[1].m_pV = m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListP[0]]->V;
        m_pContext->m_frmBuff.m_pFrames[1].m_iVPitch = m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListP[0]]->Pitch/2;

        if (VC1_B_FRAME == m_pContext->m_picLayerHeader->PTYPE )
        {
            m_pContext->m_frmBuff.m_pFrames[2].m_pY = m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListB[0][1]]->Y;
            m_pContext->m_frmBuff.m_pFrames[2].m_iYPitch = m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListB[0][1]]->Pitch;
            m_pContext->m_frmBuff.m_pFrames[2].m_pU = m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListB[0][1]]->U;
            m_pContext->m_frmBuff.m_pFrames[2].m_iUPitch = m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->Pitch/2;

            m_pContext->m_frmBuff.m_pFrames[2].m_pV = m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListB[0][1]]->V;
            m_pContext->m_frmBuff.m_pFrames[2].m_iVPitch = m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->Pitch/2;
        }
    }
    //Current Frame
    m_pContext->m_frmBuff.m_pFrames[0].m_pY = m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->Y;
    m_pContext->m_frmBuff.m_pFrames[0].m_iYPitch = m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->Pitch;

    m_pContext->m_frmBuff.m_pFrames[0].m_pU = m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->U;
    m_pContext->m_frmBuff.m_pFrames[0].m_iUPitch = m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->Pitch/2;
    m_pContext->m_frmBuff.m_pFrames[0].m_pV = m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->V;
    m_pContext->m_frmBuff.m_pFrames[0].m_iVPitch = m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->Pitch/2;



}

void MFXVideoDECVC1::PrepaeOutputPlanes(void)
{
    IppiSize  roiSize;
    mfxFrameParam* pFrameParam = m_pCUC->FrameParam;
    roiSize.width = m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->Pitch;
    roiSize.height = m_pCUC->FrameSurface->Info.Height;

    // Think about Skipping frames in UMC. No need coping
    // Copy for Skipping frames
    if (m_pCUC->FrameParam->VC1.FrameType & MFX_FRAMETYPE_S)
    {
        ippiCopy_8u_C1R(m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListP[0]]->Y,
            m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListP[0]]->Pitch,
            m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->Y,
            m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->Pitch,
            roiSize);

        //Chroma copy. We sure that we work with YUV420
        roiSize.width = m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->Pitch/2;
        roiSize.height = m_pCUC->FrameSurface->Info.Height/2;

        //Chroma
        ippiCopy_8u_C1R(m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListP[0]]->U,
                        m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListP[0]]->Pitch/2,
                        m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->U,
                        m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->Pitch/2,
                        roiSize);

        ippiCopy_8u_C1R(m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListP[0]]->V,
                        m_pCUC->FrameSurface->Data[pFrameParam->VC1.RefFrameListP[0]]->Pitch/2,
                        m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->V,
                        m_pCUC->FrameSurface->Data[pFrameParam->VC1.CurrFrameLabel]->Pitch/2,
                        roiSize);
    }
}

mfxStatus MFXVideoDECVC1::UnPackFrameHeaders(mfxFrameCUC*  pCUC)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxSts = SetpExtBufs(pCUC);
    MFX_CHECK_STS(mfxSts);
    mfxSts = UnPackSeqHeader(pCUC, m_pContext->m_seqLayerHeader);
    MFX_CHECK_STS(mfxSts);
    mfxSts = UnPackPicHeader(pCUC, m_pContext);
    MFX_CHECK_STS(mfxSts);
    return mfxSts;
}


#endif
