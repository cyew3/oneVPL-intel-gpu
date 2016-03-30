//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2016 Intel Corporation. All Rights Reserved.
//

#include "pipeline_fei.h"
#include "refListsManagement_fei.h"
#include "sysmem_allocator.h"
#include "math.h"

#if D3D_SURFACES_SUPPORT
#include "d3d_allocator.h"
#include "d3d11_allocator.h"

#include "d3d_device.h"
#include "d3d11_device.h"
#endif

#ifdef LIBVA_SUPPORT
#include "vaapi_allocator.h"
#include "vaapi_device.h"
#endif

#if _DEBUG
#define mdprintf fprintf
#else
#define mdprintf(...)
#endif

mfxU8 GetDefaultPicOrderCount(mfxVideoParam const & par)
{
    if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        return 0;
    if (par.mfx.GopRefDist > 1)
        return 0;

    return 2;
}

static FILE* mbstatout   = NULL;
static FILE* mvout       = NULL;
static FILE* mvENCPAKout = NULL;
static FILE* mbcodeout   = NULL;
static FILE* pMvPred     = NULL;
static FILE* pEncMBs     = NULL;
static FILE* pPerMbQP    = NULL;
static mfxU32 numMBs     = 0;
static bool bDRCReset    = false;

CEncTaskPool::CEncTaskPool()
{
    m_pTasks            = NULL;
    m_pmfxSession       = NULL;
    m_nTaskBufferStart  = 0;
    m_nPoolSize         = 0;
    m_nFieldId          = NOT_IN_SINGLE_FIELD_MODE;
}

CEncTaskPool::~CEncTaskPool()
{
    Close();
}

mfxStatus CEncTaskPool::Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize, CSmplBitstreamWriter *pOtherWriter)
{
    MSDK_CHECK_POINTER(pmfxSession, MFX_ERR_NULL_PTR);
    //MSDK_CHECK_POINTER(pWriter, MFX_ERR_NULL_PTR);

    MSDK_CHECK_ERROR(nPoolSize,   0, MFX_ERR_UNDEFINED_BEHAVIOR);
    MSDK_CHECK_ERROR(nBufferSize, 0, MFX_ERR_UNDEFINED_BEHAVIOR);

    // nPoolSize must be even in case of 2 output bitstreams
    if (pOtherWriter && (0 != nPoolSize % 2))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    m_pmfxSession = pmfxSession;
    m_nPoolSize   = nPoolSize;

    m_pTasks = new sTask [m_nPoolSize];
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_MEMORY_ALLOC);

    mfxStatus sts = MFX_ERR_NONE;

    if (pOtherWriter) // 2 bitstreams on output
    {
        for (mfxU32 i = 0; i < m_nPoolSize; i+=2)
        {
            sts = m_pTasks[i+0].Init(nBufferSize, pWriter);
            sts = m_pTasks[i+1].Init(nBufferSize, pOtherWriter);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }
    else
    {
        for (mfxU32 i = 0; i < m_nPoolSize; i++)
        {
            sts = m_pTasks[i].Init(nBufferSize, pWriter);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncTaskPool::SynchronizeFirstTask()
{
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(m_pmfxSession, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts  = MFX_ERR_NONE;

    // non-null sync point indicates that task is in execution
    if (NULL != m_pTasks[m_nTaskBufferStart].EncSyncP)
    {
        sts = m_pmfxSession->SyncOperation(m_pTasks[m_nTaskBufferStart].EncSyncP, MSDK_WAIT_INTERVAL);

        if (MFX_ERR_NONE == sts)
        {
            //Write output for encode
            mfxBitstream& bs = m_pTasks[m_nTaskBufferStart].mfxBS;
            if (m_nFieldId == NOT_IN_SINGLE_FIELD_MODE){
                for (int fieldId = 0; fieldId < ((bs.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2); fieldId++){
                    sts = DropENCODEoutput(bs, fieldId);
                    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                }
            }
            else{
                sts = DropENCODEoutput(bs, m_nFieldId);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }

            sts = m_pTasks[m_nTaskBufferStart].WriteBitstream();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            sts = m_pTasks[m_nTaskBufferStart].Reset();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            // move task buffer start to the next executing task
            // the first transform frame to the right with non zero sync point
            for (mfxU32 i = 0; i < m_nPoolSize; i++)
            {
                m_nTaskBufferStart = (m_nTaskBufferStart + 1) % m_nPoolSize;
                if (NULL != m_pTasks[m_nTaskBufferStart].EncSyncP)
                {
                    break;
                }
            }
        }

        return sts;
    }
    else
    {
        return MFX_ERR_NOT_FOUND; // no tasks left in task buffer
    }
} // mfxStatus CEncTaskPool::SynchronizeFirstTask()

mfxStatus CEncTaskPool::SetFieldToStore(mfxU32 fieldId)
{
    m_nFieldId = fieldId;
    return MFX_ERR_NONE;

} // mfxStatus CEncTaskPool::SetFieldToStore(mfxU32 fieldId)

mfxU32 CEncTaskPool::GetFreeTaskIndex()
{
    mfxU32 off = 0;

    if (m_pTasks)
    {
        for (off = 0; off < m_nPoolSize; off++)
        {
            if (NULL == m_pTasks[(m_nTaskBufferStart + off) % m_nPoolSize].EncSyncP)
            {
                break;
            }
        }
    }

    if (off >= m_nPoolSize)
        return m_nPoolSize;

    return (m_nTaskBufferStart + off) % m_nPoolSize;
}

mfxStatus CEncTaskPool::GetFreeTask(sTask **ppTask)
{
    MSDK_CHECK_POINTER(ppTask, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_NOT_INITIALIZED);

    mfxU32 index = GetFreeTaskIndex();

    if (index >= m_nPoolSize)
    {
        return MFX_ERR_NOT_FOUND;
    }

    // return the address of the task
    *ppTask = &m_pTasks[index];

    return MFX_ERR_NONE;
}

mfxStatus CEncTaskPool::DropENCODEoutput(mfxBitstream& bs, mfxU32 m_nFieldId)
{
    mfxExtFeiEncMV*     mvBuf     = NULL;
    mfxExtFeiEncMBStat* mbstatBuf = NULL;
    mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;
    mfxU32 mvBufCounter     = 0;
    mfxU32 mbStatBufCounter = 0;
    mfxU32 mbCodeBufCounter = 0;

    for (int i = 0; i < bs.NumExtParam; i++)
    {
        switch (bs.ExtParam[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_ENC_MV:
            if (mvENCPAKout)
            {
                if (mvBufCounter++ != m_nFieldId)
                    continue;

                mvBuf = (mfxExtFeiEncMV*)(bs.ExtParam[i]);
                if (!(extractType(bs.FrameType, m_nFieldId) & MFX_FRAMETYPE_I)){
                    SAFE_FWRITE(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout, MFX_ERR_MORE_DATA);
                }
                else{
                    mfxExtFeiEncMV::mfxExtFeiEncMVMB tmpMB;
                    memset(&tmpMB, 0x8000, sizeof(tmpMB));
                    for (mfxU32 k = 0; k < mvBuf->NumMBAlloc; k++){
                        SAFE_FWRITE(&tmpMB, sizeof(tmpMB), 1, mvENCPAKout, MFX_ERR_MORE_DATA);
                    }
                }
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_MB_STAT:
            if (mbstatout){
                if (mbStatBufCounter++ != m_nFieldId)
                    continue;

                mbstatBuf = (mfxExtFeiEncMBStat*)(bs.ExtParam[i]);
                SAFE_FWRITE(mbstatBuf->MB, sizeof(mbstatBuf->MB[0])*mbstatBuf->NumMBAlloc, 1, mbstatout, MFX_ERR_MORE_DATA);
            }
            break;

        case MFX_EXTBUFF_FEI_PAK_CTRL:
            if (mbcodeout){
                if (mbCodeBufCounter++ != m_nFieldId)
                    continue;

                mbcodeBuf = (mfxExtFeiPakMBCtrl*)(bs.ExtParam[i]);
                SAFE_FWRITE(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout, MFX_ERR_MORE_DATA);
            }
            break;
        } // switch (bs.ExtParam[i]->BufferId)
    } // for (int i = 0; i < bs.NumExtParam; i++)

    return MFX_ERR_NONE;
}


void CEncTaskPool::Close()
{
    if (m_pTasks)
    {
        for (mfxU32 i = 0; i < m_nPoolSize; i++)
        {
            m_pTasks[i].Close();
        }
    }

    MSDK_SAFE_DELETE_ARRAY(m_pTasks);

    m_pmfxSession = NULL;
    m_nTaskBufferStart = 0;
    m_nPoolSize = 0;
}

sTask::sTask()
    : EncSyncP(0)
    , pWriter(NULL)
{
    MSDK_ZERO_MEMORY(mfxBS);
}

mfxStatus sTask::Init(mfxU32 nBufferSize, CSmplBitstreamWriter *pwriter)
{
    Close();

    pWriter = pwriter;

    mfxStatus sts = Reset();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = InitMfxBitstream(&mfxBS, nBufferSize);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(&mfxBS));

    return sts;
}

mfxStatus sTask::Close()
{
    WipeMfxBitstream(&mfxBS);
    EncSyncP = 0;

    return MFX_ERR_NONE;
}

mfxStatus sTask::WriteBitstream()
{
    if (!pWriter)
        return MFX_ERR_NOT_INITIALIZED;

    return pWriter->WriteNextFrame(&mfxBS);
}

mfxStatus sTask::Reset()
{
    // mark sync point as free
    EncSyncP = NULL;

    // prepare bit stream
    mfxBS.DataOffset = 0;
    mfxBS.DataLength = 0;

    for (std::list<std::pair<bufSet*, mfxFrameSurface1*> >::iterator it = bufs.begin(); it != bufs.end(); )
    {
        if (!(*it).second || (*it).second->Data.Locked == 0)
        {
            (*it).first->vacant = true;
            it = bufs.erase(it);
            //break;
        }
        else{
            ++it;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::InitMfxEncParams(sInputParams *pInParams)
{
    MSDK_CHECK_POINTER(pInParams, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;

    m_encpakParams = *pInParams;

    m_mfxEncParams.mfx.CodecId     = pInParams->CodecId;
    m_mfxEncParams.mfx.TargetUsage = 0; // FEI doesn't have support of
    m_mfxEncParams.mfx.TargetKbps  = 0; // these features
    /*For now FEI work with RATECONTROL_CQP only*/
    m_mfxEncParams.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    sts = ConvertFrameRate(pInParams->dFrameRate, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtN, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtD);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    m_mfxEncParams.mfx.EncodedOrder = pInParams->EncodedOrder; // binary flag, 0 signals encoder to take frames in display order

    if (0 != pInParams->QP)
        m_mfxEncParams.mfx.QPI = m_mfxEncParams.mfx.QPP = m_mfxEncParams.mfx.QPB = pInParams->QP;

    // specify memory type
    if (D3D9_MEMORY == pInParams->memType || D3D11_MEMORY == pInParams->memType)
    {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }
    else
    {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }

    // frame info parameters
    m_mfxEncParams.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
    m_mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_mfxEncParams.mfx.FrameInfo.PicStruct    = pInParams->nPicStruct;

    if (pInParams->bDynamicRC)
    {
       m_bNeedDRC = true;
       m_drcDftW = pInParams->nDRCdefautW;
       m_drcDftH = pInParams->nDRCdefautH;
       size_t whsize = pInParams->nDrcWidth.size();
       m_drcWidth.reserve(whsize);
       m_drcHeight.reserve(whsize);
       m_drcStart = pInParams->nDrcStart;
       m_drcWidth = pInParams->nDrcWidth;
       m_drcHeight = pInParams->nDrcHeight;
    }

    // set frame size and crops
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_mfxEncParams.mfx.FrameInfo.Width  = MSDK_ALIGN16(pInParams->nDstWidth);
    m_mfxEncParams.mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxEncParams.mfx.FrameInfo.PicStruct)?
        MSDK_ALIGN16(pInParams->nDstHeight) : MSDK_ALIGN32(pInParams->nDstHeight);

    m_mfxEncParams.mfx.FrameInfo.CropX = 0;
    m_mfxEncParams.mfx.FrameInfo.CropY = 0;
    m_mfxEncParams.mfx.FrameInfo.CropW = pInParams->nDstWidth;
    m_mfxEncParams.mfx.FrameInfo.CropH = pInParams->nDstHeight;
    m_mfxEncParams.AsyncDepth      = 1; //current limitation
    m_mfxEncParams.mfx.GopRefDist  = pInParams->refDist > 0 ? pInParams->refDist : 1;
    m_mfxEncParams.mfx.GopPicSize  = pInParams->gopSize > 0 ? pInParams->gopSize : 1;
    m_mfxEncParams.mfx.IdrInterval = pInParams->nIdrInterval;
    m_mfxEncParams.mfx.GopOptFlag  = pInParams->GopOptFlag;
    m_mfxEncParams.mfx.CodecProfile = pInParams->CodecProfile;
    m_mfxEncParams.mfx.CodecLevel   = pInParams->CodecLevel;

    /* Multi references and multi slices*/
    if (pInParams->numRef == 0)
    {
        m_mfxEncParams.mfx.NumRefFrame = 1;
    }
    else
    {
        m_mfxEncParams.mfx.NumRefFrame = pInParams->numRef;
    }

    if (pInParams->numSlices == 0)
    {
        m_mfxEncParams.mfx.NumSlice = 1;
    }
    else
    {
        m_mfxEncParams.mfx.NumSlice = pInParams->numSlices;
    }

    // configure trellis, B-pyramid, RAW-reference settings
    //if (pInParams->bRefType || pInParams->Trellis || pInParams->bRawRef)
    {
        m_CodingOption2.UseRawRef = pInParams->bRawRef ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
        m_CodingOption2.BRefType  = pInParams->bRefType;
        m_CodingOption2.Trellis   = pInParams->Trellis;
        m_EncExtParams.push_back((mfxExtBuffer *)&m_CodingOption2);
    }

    // configure P/B reference number
    {
        m_CodingOption3.NumRefActiveP      = pInParams->bNRefPSpecified   ? pInParams->NumRefActiveP   : MaxNumActiveRefP;
        m_CodingOption3.NumRefActivePRef   = pInParams->bNRefPSpecified   ? pInParams->NumRefActiveP   : MaxNumActiveRefP;
        m_CodingOption3.NumRefActiveBL0    = pInParams->bNRefBL0Specified ? pInParams->NumRefActiveBL0 : MaxNumActiveRefBL0;
        m_CodingOption3.NumRefActiveBRefL0 = pInParams->bNRefBL0Specified ? pInParams->NumRefActiveBL0 : MaxNumActiveRefBL0;
        m_CodingOption3.NumRefActiveBL1    = pInParams->bNRefBL1Specified ? pInParams->NumRefActiveBL1 :
            (pInParams->nPicStruct == MFX_PICSTRUCT_PROGRESSIVE ? MaxNumActiveRefBL1 : MaxNumActiveRefBL1_i);
        m_CodingOption3.NumRefActiveBRefL1 = pInParams->bNRefBL1Specified ? pInParams->NumRefActiveBL1 :
            (pInParams->nPicStruct == MFX_PICSTRUCT_PROGRESSIVE ? MaxNumActiveRefBL1 : MaxNumActiveRefBL1_i);

        /* values stored in m_CodingOption3 required to fill encoding task for PREENC/ENC/PAK*/
        if (pInParams->bNRefPSpecified || pInParams->bNRefBL0Specified || pInParams->bNRefBL1Specified)
            m_EncExtParams.push_back((mfxExtBuffer *)&m_CodingOption3);
    }

    if(pInParams->bENCODE)
    {
        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_ENCODE;
        if (pInParams->bFieldProcessingMode)
            m_encpakInit.SingleFieldProcessing = MFX_CODINGOPTION_ON;
        m_EncExtParams.push_back((mfxExtBuffer *)&m_encpakInit);

        //Create extended buffer to init deblocking parameters
        if (m_encpakParams.DisableDeblockingIdc || m_encpakParams.SliceAlphaC0OffsetDiv2 ||
            m_encpakParams.SliceBetaOffsetDiv2)
        {
            size_t numFields = m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
            MSDK_ZERO_ARRAY(m_encodeSliceHeader, numFields);
            for (size_t fieldId = 0; fieldId < numFields; ++fieldId)
            {
                m_encodeSliceHeader[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_SLICE;
                m_encodeSliceHeader[fieldId].Header.BufferSz = sizeof(mfxExtFeiSliceHeader);

                m_encodeSliceHeader[fieldId].NumSlice =
                    m_encodeSliceHeader[fieldId].NumSliceAlloc = m_mfxEncParams.mfx.NumSlice;
                m_encodeSliceHeader[fieldId].Slice = new mfxExtFeiSliceHeader::mfxSlice[m_mfxEncParams.mfx.NumSlice];
                MSDK_CHECK_POINTER(m_encodeSliceHeader[fieldId].Slice, MFX_ERR_MEMORY_ALLOC);
                MSDK_ZERO_ARRAY(m_encodeSliceHeader[fieldId].Slice, m_mfxEncParams.mfx.NumSlice);

                for (size_t sliceNum = 0; sliceNum < m_mfxEncParams.mfx.NumSlice; ++sliceNum)
                {
                    m_encodeSliceHeader[fieldId].Slice[sliceNum].DisableDeblockingFilterIdc = pInParams->DisableDeblockingIdc;
                    m_encodeSliceHeader[fieldId].Slice[sliceNum].SliceAlphaC0OffsetDiv2     = pInParams->SliceAlphaC0OffsetDiv2;
                    m_encodeSliceHeader[fieldId].Slice[sliceNum].SliceBetaOffsetDiv2        = pInParams->SliceBetaOffsetDiv2;
                }
                m_EncExtParams.push_back((mfxExtBuffer *)&m_encodeSliceHeader[fieldId]);
            }
        }
    }

    if (!m_EncExtParams.empty())
    {
        m_mfxEncParams.ExtParam = &m_EncExtParams[0]; // vector is stored linearly in memory
        m_mfxEncParams.NumExtParam = (mfxU16)m_EncExtParams.size();
    }

    return sts;
}

mfxStatus CEncodingPipeline::InitMfxVppParams(sInputParams *pInParams)
{
    MSDK_CHECK_POINTER(pInParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    // specify memory type
    if (D3D9_MEMORY == pInParams->memType || D3D11_MEMORY == pInParams->memType)
    {
        m_mfxVppParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }
    else
    {
        m_mfxVppParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }

    // input frame info
    m_mfxVppParams.vpp.In.FourCC = MFX_FOURCC_NV12;
    m_mfxVppParams.vpp.In.PicStruct = pInParams->nPicStruct;;
    sts = ConvertFrameRate(pInParams->dFrameRate, &m_mfxVppParams.vpp.In.FrameRateExtN, &m_mfxVppParams.vpp.In.FrameRateExtD);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_mfxVppParams.vpp.In.Width = MSDK_ALIGN16(pInParams->nWidth);
    m_mfxVppParams.vpp.In.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxVppParams.vpp.In.PicStruct) ?
        MSDK_ALIGN16(pInParams->nHeight) : MSDK_ALIGN32(pInParams->nHeight);

    // set crops in input mfxFrameInfo for correct work of file reader
    // VPP itself ignores crops at initialization
    m_mfxVppParams.vpp.In.CropW = pInParams->nWidth;
    m_mfxVppParams.vpp.In.CropH = pInParams->nHeight;

    // fill output frame info
    MSDK_MEMCPY_VAR(m_mfxVppParams.vpp.Out, &m_mfxVppParams.vpp.In, sizeof(mfxFrameInfo));

    // only resizing is supported
    m_mfxVppParams.vpp.Out.Width = MSDK_ALIGN16(pInParams->nDstWidth);
    m_mfxVppParams.vpp.Out.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxVppParams.vpp.Out.PicStruct) ?
        MSDK_ALIGN16(pInParams->nDstHeight) : MSDK_ALIGN32(pInParams->nDstHeight);

    // configure and attach external parameters
    m_VppDoNotUse.NumAlg = 4;
    m_VppDoNotUse.AlgList = new mfxU32[m_VppDoNotUse.NumAlg];
    MSDK_CHECK_POINTER(m_VppDoNotUse.AlgList, MFX_ERR_MEMORY_ALLOC);
    m_VppDoNotUse.AlgList[0] = MFX_EXTBUFF_VPP_DENOISE;        // turn off denoising (on by default)
    m_VppDoNotUse.AlgList[1] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS; // turn off scene analysis (on by default)
    m_VppDoNotUse.AlgList[2] = MFX_EXTBUFF_VPP_DETAIL;         // turn off detail enhancement (on by default)
    m_VppDoNotUse.AlgList[3] = MFX_EXTBUFF_VPP_PROCAMP;        // turn off processing amplified (on by default)
    m_VppExtParams.push_back((mfxExtBuffer *)&m_VppDoNotUse);

    m_mfxVppParams.ExtParam = &m_VppExtParams[0]; // vector is stored linearly in memory
    m_mfxVppParams.NumExtParam = (mfxU16)m_VppExtParams.size();

    m_mfxVppParams.AsyncDepth = 1; //current limitation

    if (pInParams->preencDSstrength)
    {
        m_mfxDSParams = m_mfxVppParams;

        m_mfxDSParams.vpp.Out.Width = MSDK_ALIGN16(pInParams->nDstWidth / pInParams->preencDSstrength);
        m_mfxDSParams.vpp.Out.Height = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxDSParams.vpp.Out.PicStruct) ?
            MSDK_ALIGN16(pInParams->nDstHeight / pInParams->preencDSstrength) : MSDK_ALIGN32(pInParams->nDstHeight / pInParams->preencDSstrength);
        m_mfxDSParams.vpp.Out.CropW = m_mfxDSParams.vpp.Out.Width;
        m_mfxDSParams.vpp.Out.CropH = m_mfxDSParams.vpp.Out.Height;
    }

    return sts;
}

mfxStatus CEncodingPipeline::InitMfxDecodeParams(sInputParams *pInParams)
{
    mfxStatus sts = MFX_ERR_NONE;
    MSDK_CHECK_POINTER(pInParams, MFX_ERR_NULL_PTR);

    m_mfxDecParams.AsyncDepth = 1; //current limitation

    // set video type in parameters
    m_mfxDecParams.mfx.CodecId = pInParams->DecodeId;

    m_BSReader.Init(pInParams->strSrcFile);
    sts = InitMfxBitstream(&m_mfxBS, 1024 * 1024);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // read a portion of data for DecodeHeader function
    sts = m_BSReader.ReadNextFrame(&m_mfxBS);
    if (MFX_ERR_MORE_DATA == sts)
        return sts;
    else
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // try to find a sequence header in the stream
    // if header is not found this function exits with error (e.g. if device was lost and there's no header in the remaining stream)
    for (;;)
    {
        // parse bit stream and fill mfx params
        sts = m_pmfxDECODE->DecodeHeader(&m_mfxBS, &m_mfxDecParams);

        if (MFX_ERR_MORE_DATA == sts)
        {
            if (m_mfxBS.MaxLength == m_mfxBS.DataLength)
            {
                sts = ExtendMfxBitstream(&m_mfxBS, m_mfxBS.MaxLength * 2);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }

            // read a portion of data for DecodeHeader function
            sts = m_BSReader.ReadNextFrame(&m_mfxBS);
            if (MFX_ERR_MORE_DATA == sts)
                return sts;
            else
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            continue;
        }
        else
            break;
    }

    // if input stream header doesn't contain valid values use default (30.0)
    if (!(m_mfxDecParams.mfx.FrameInfo.FrameRateExtN * m_mfxDecParams.mfx.FrameInfo.FrameRateExtD))
    {
        m_mfxDecParams.mfx.FrameInfo.FrameRateExtN = 30;
        m_mfxDecParams.mfx.FrameInfo.FrameRateExtD = 1;
    }

     // specify memory type
     m_mfxDecParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    return sts;
}

mfxStatus CEncodingPipeline::CreateHWDevice()
{
    mfxStatus sts = MFX_ERR_NONE;
#if D3D_SURFACES_SUPPORT
    POINT point = {0, 0};
    HWND window = WindowFromPoint(point);

#if MFX_D3D11_SUPPORT
    if (D3D11_MEMORY == m_memType)
        m_hwdev = new CD3D11Device();
    else
#endif // #if MFX_D3D11_SUPPORT
        m_hwdev = new CD3D9Device();

    MSDK_CHECK_POINTER(m_hwdev, MFX_ERR_MEMORY_ALLOC);

    sts = m_hwdev->Init(
        window,
        0,
        MSDKAdapter::GetNumber(m_mfxSession));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

#elif LIBVA_SUPPORT
    m_hwdev = CreateVAAPIDevice();
    if (NULL == m_hwdev)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    sts = m_hwdev->Init(NULL, 0, MSDKAdapter::GetNumber(m_mfxSession));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
#endif
    return sts;
}

mfxStatus CEncodingPipeline::ResetDevice()
{
    if (D3D9_MEMORY == m_memType || D3D11_MEMORY == m_memType)
    {
        return m_hwdev->Reset();
    }
    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocFrames()
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest DecRequest;
    mfxFrameAllocRequest VppRequest[2];
    mfxFrameAllocRequest EncRequest;
    mfxFrameAllocRequest ReconRequest;

    mfxU16 nEncSurfNum = 0; // number of surfaces for encoder

    MSDK_ZERO_MEMORY(DecRequest);
    MSDK_ZERO_MEMORY(VppRequest[0]); //VPP in
    MSDK_ZERO_MEMORY(VppRequest[1]); //VPP out
    MSDK_ZERO_MEMORY(EncRequest);
    MSDK_ZERO_MEMORY(ReconRequest);

    // Calculate the number of surfaces for components.
    // QueryIOSurf functions tell how many surfaces are required to produce at least 1 output.

    if (m_pmfxENCODE)
    {
        sts = m_pmfxENCODE->QueryIOSurf(&m_mfxEncParams, &EncRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    m_maxQueueLength = m_refDist * 2 + m_mfxEncParams.AsyncDepth;
    /* temporary solution for a while for PREENC + ENC cases
    * (It required to calculate accurate formula for all usage cases)
    * this is not optimal from memory usage consumption */
    m_maxQueueLength += m_mfxEncParams.mfx.NumRefFrame + 1;

    // The number of surfaces shared by vpp output and encode input.
    // When surfaces are shared 1 surface at first component output contains output frame that goes to next component input
    nEncSurfNum = EncRequest.NumFrameSuggested + (m_nAsyncDepth - 1)+2;
    if ((m_encpakParams.bPREENC) || (m_encpakParams.bENCPAK) || (m_encpakParams.bENCODE) ||
        (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK))
    {
        nEncSurfNum = m_maxQueueLength;
        nEncSurfNum += m_pmfxVPP ? m_refDist + 1 : 0;
    }

    // prepare allocation requests
    EncRequest.NumFrameMin = nEncSurfNum;
    EncRequest.NumFrameSuggested = nEncSurfNum;
    MSDK_MEMCPY_VAR(EncRequest.Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

    EncRequest.AllocId = m_BaseAllocID;
    EncRequest.Type |= MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
    if (m_pmfxPREENC && !m_pmfxDS)
        EncRequest.Type |= MFX_MEMTYPE_FROM_ENC;
    if ((m_pmfxPAK) || (m_pmfxENC))
        EncRequest.Type |= (MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_FROM_PAK);

    if (m_pmfxDS)
    {
        mfxFrameAllocRequest DSRequest[2];
        MSDK_ZERO_MEMORY(DSRequest[0]);
        MSDK_ZERO_MEMORY(DSRequest[1]);

        sts = m_pmfxDS->QueryIOSurf(&m_mfxDSParams, DSRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // these surfaces are input surfaces for PREENC
        DSRequest[1].NumFrameMin = DSRequest[1].NumFrameSuggested = EncRequest.NumFrameSuggested;
        DSRequest[1].Type = EncRequest.Type | MFX_MEMTYPE_FROM_ENC;
        DSRequest[1].AllocId = m_BaseAllocID;

        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &(DSRequest[1]), &m_dsResponse);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        sts = FillSurfacePool(m_pDSSurfaces, &m_dsResponse, &(m_mfxDSParams.vpp.Out));
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if (m_pmfxDECODE)
    {
        sts = m_pmfxDECODE->QueryIOSurf(&m_mfxDecParams, &DecRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        m_decodePoolSize = DecRequest.NumFrameSuggested;
        DecRequest.NumFrameMin = DecRequest.NumFrameSuggested;

        if (!m_pmfxVPP)
        {
            // in case of Decode and absence of VPP we use the same surface pool for the entire pipeline
            nEncSurfNum += m_mfxEncParams.mfx.NumRefFrame + 1;
            if (DecRequest.NumFrameSuggested <= nEncSurfNum)
                DecRequest.NumFrameMin = DecRequest.NumFrameSuggested = nEncSurfNum;
            else
                DecRequest.NumFrameMin = DecRequest.NumFrameSuggested = m_decodePoolSize + nEncSurfNum;
            if ((m_pmfxENC) || (m_pmfxPAK)) // plus reconstructed frames for PAK
                DecRequest.NumFrameMin = DecRequest.NumFrameSuggested = DecRequest.NumFrameSuggested + m_mfxEncParams.mfx.GopRefDist * 2 + m_mfxEncParams.AsyncDepth;
        }
        MSDK_MEMCPY_VAR(DecRequest.Info, &(m_mfxDecParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

        if (m_pmfxVPP || m_pmfxDS)
        {
            DecRequest.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_DECODE;
        }
        else
            DecRequest.Type = EncRequest.Type | MFX_MEMTYPE_FROM_DECODE;
        DecRequest.AllocId = m_BaseAllocID;

        // alloc frames for decoder
        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &DecRequest, &m_DecResponse);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // prepare mfxFrameSurface1 array for decoder
        sts = FillSurfacePool(m_pDecSurfaces, &m_DecResponse, &(m_mfxDecParams.mfx.FrameInfo));
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        if (!m_pmfxVPP)
            return MFX_ERR_NONE;
    }

    if (m_pmfxVPP && !m_pmfxDECODE)
    {
        // in case of absence of DECODE we need to allocate input surfaces for VPP

        sts = m_pmfxVPP->QueryIOSurf(&m_mfxVppParams, VppRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        VppRequest[0].NumFrameMin = VppRequest[0].NumFrameSuggested;
        VppRequest[0].AllocId = m_BaseAllocID;

        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &(VppRequest[0]), &m_VppResponse);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // prepare mfxFrameSurface1 array for VPP
        sts = FillSurfacePool(m_pVppSurfaces, &m_VppResponse, &(m_mfxVppParams.mfx.FrameInfo));
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // alloc frames for encoder
    sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &EncRequest, &m_EncResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    /* ENC use input source surfaces only & does not need real reconstructed surfaces.
     * But source surfaces index is always equal to reconstructed surface index.
     * PAK generate real reconstructed surfaces (instead of source surfaces)Alloc reconstructed surfaces for PAK
     * So, after PAK call source surface changed on reconstructed surface
     * This is important limitation for PAK
     * */
    if ((m_pmfxENC) || (m_pmfxPAK))
    {
        ReconRequest.AllocId = EncRequest.AllocId;
        MSDK_MEMCPY_VAR(ReconRequest.Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
        ReconRequest.NumFrameMin       = EncRequest.NumFrameMin;
        ReconRequest.NumFrameSuggested = EncRequest.NumFrameSuggested;
        ReconRequest.Type = EncRequest.Type;
    }

    // prepare mfxFrameSurface1 array for encoder
    sts = FillSurfacePool(m_pEncSurfaces, &m_EncResponse, &(m_mfxEncParams.mfx.FrameInfo));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if ((m_pmfxENC) || (m_pmfxPAK))
    {
        m_pReconSurfaces = m_pEncSurfaces;
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::FillSurfacePool(mfxFrameSurface1* & surfacesPool, mfxFrameAllocResponse* allocResponse, mfxFrameInfo* FrameInfo)
{
    mfxStatus sts = MFX_ERR_NONE;

    surfacesPool = new mfxFrameSurface1[allocResponse->NumFrameActual];
    MSDK_CHECK_POINTER(surfacesPool, MFX_ERR_MEMORY_ALLOC);
    MSDK_ZERO_ARRAY(surfacesPool, allocResponse->NumFrameActual);

    for (int i = 0; i < allocResponse->NumFrameActual; i++)
    {
        MSDK_MEMCPY_VAR(surfacesPool[i].Info, FrameInfo, sizeof(mfxFrameInfo));

        if (m_bExternalAlloc)
        {
            surfacesPool[i].Data.MemId = allocResponse->mids[i];
        }
        else
        {
            // get YUV pointers
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, allocResponse->mids[i], &(surfacesPool[i].Data));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    return sts;
}

mfxStatus CEncodingPipeline::CreateAllocator()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (D3D9_MEMORY == m_memType || D3D11_MEMORY == m_memType)
    {
#if D3D_SURFACES_SUPPORT
        sts = CreateHWDevice();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        mfxHDL hdl = NULL;
        mfxHandleType hdl_t =
        #if MFX_D3D11_SUPPORT
            D3D11_MEMORY == m_memType ? MFX_HANDLE_D3D11_DEVICE :
        #endif // #if MFX_D3D11_SUPPORT
            MFX_HANDLE_D3D9_DEVICE_MANAGER;

        sts = m_hwdev->GetHandle(hdl_t, &hdl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // handle is needed for HW library only
        mfxIMPL impl = 0;
        m_mfxSession.QueryIMPL(&impl);

        if (impl != MFX_IMPL_SOFTWARE)
        {
            sts = m_mfxSession.SetHandle(hdl_t, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            if (m_encpakParams.bDECODE){
                sts = m_decode_mfxSession.SetHandle(hdl_t, hdl);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }

            if (m_encpakParams.bPREENC){
                sts = m_preenc_mfxSession.SetHandle(hdl_t, hdl);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
        }

        // create D3D allocator
#if MFX_D3D11_SUPPORT
        if (D3D11_MEMORY == m_memType)
        {
            m_pMFXAllocator = new D3D11FrameAllocator;
            MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

            D3D11AllocatorParams *pd3dAllocParams = new D3D11AllocatorParams;
            MSDK_CHECK_POINTER(pd3dAllocParams, MFX_ERR_MEMORY_ALLOC);
            pd3dAllocParams->pDevice = reinterpret_cast<ID3D11Device *>(hdl);

            m_pmfxAllocatorParams = pd3dAllocParams;
        }
        else
#endif // #if MFX_D3D11_SUPPORT
        {
            m_pMFXAllocator = new D3DFrameAllocator;
            MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

            D3DAllocatorParams *pd3dAllocParams = new D3DAllocatorParams;
            MSDK_CHECK_POINTER(pd3dAllocParams, MFX_ERR_MEMORY_ALLOC);
            pd3dAllocParams->pManager = reinterpret_cast<IDirect3DDeviceManager9 *>(hdl);

            m_pmfxAllocatorParams = pd3dAllocParams;
        }

        /* In case of video memory we must provide MediaSDK with external allocator
        thus we demonstrate "external allocator" usage model.
        Call SetAllocator to pass allocator to Media SDK */
        sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        if (m_encpakParams.bDECODE){
            sts = m_decode_mfxSession.SetFrameAllocator(m_pMFXAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        if (m_encpakParams.bPREENC){
            sts = m_preenc_mfxSession.SetFrameAllocator(m_pMFXAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        m_bExternalAlloc = true;
#endif
#ifdef LIBVA_SUPPORT
        sts = CreateHWDevice();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        /* It's possible to skip failed result here and switch to SW implementation,
        but we don't process this way */

        mfxHDL hdl = NULL;
        sts = m_hwdev->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
        // provide device manager to MediaSDK
        sts = m_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        if (m_encpakParams.bDECODE){
            sts = m_decode_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        if (m_encpakParams.bPREENC){
            sts = m_preenc_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        // create VAAPI allocator
        m_pMFXAllocator = new vaapiFrameAllocator;
        MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

        vaapiAllocatorParams *p_vaapiAllocParams = new vaapiAllocatorParams;
        MSDK_CHECK_POINTER(p_vaapiAllocParams, MFX_ERR_MEMORY_ALLOC);

        p_vaapiAllocParams->m_dpy = (VADisplay)hdl;
        m_pmfxAllocatorParams = p_vaapiAllocParams;

        /* In case of video memory we must provide MediaSDK with external allocator
        thus we demonstrate "external allocator" usage model.
        Call SetAllocator to pass allocator to mediasdk */
        sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        if (m_encpakParams.bDECODE){
            sts = m_decode_mfxSession.SetFrameAllocator(m_pMFXAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        if (m_encpakParams.bPREENC){
            sts = m_preenc_mfxSession.SetFrameAllocator(m_pMFXAllocator);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        m_bExternalAlloc = true;
#endif
    }
    else
    {
#ifdef LIBVA_SUPPORT
        //in case of system memory allocator we also have to pass MFX_HANDLE_VA_DISPLAY to HW library
        mfxIMPL impl;
        m_mfxSession.QueryIMPL(&impl);

        if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(impl))
        {
            sts = CreateHWDevice();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            mfxHDL hdl = NULL;
            sts = m_hwdev->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
            // provide device manager to MediaSDK
            sts = m_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            if (m_encpakParams.bDECODE){
                sts = m_decode_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            if (m_encpakParams.bPREENC){
                sts = m_preenc_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
        }
#endif

        // create system memory allocator
        m_pMFXAllocator = new SysMemFrameAllocator;
        MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

        /* In case of system memory we demonstrate "no external allocator" usage model.
        We don't call SetAllocator, Media SDK uses internal allocator.
        We use system memory allocator simply as a memory manager for application*/
    }

    // initialize memory allocator
    sts = m_pMFXAllocator->Init(m_pmfxAllocatorParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

void CEncodingPipeline::DeleteFrames()
{
    // delete surfaces array
    MSDK_SAFE_DELETE_ARRAY(m_pDecSurfaces);
    MSDK_SAFE_DELETE_ARRAY(m_pVppSurfaces);
    MSDK_SAFE_DELETE_ARRAY(m_pDSSurfaces);
    if (m_pEncSurfaces != m_pReconSurfaces)
    {
        MSDK_SAFE_DELETE_ARRAY(m_pEncSurfaces);
        MSDK_SAFE_DELETE_ARRAY(m_pReconSurfaces);
    }
    else
    {
        MSDK_SAFE_DELETE_ARRAY(m_pEncSurfaces);
        m_pReconSurfaces = NULL;
    }

    // delete frames
    if (m_pMFXAllocator)
    {
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_DecResponse);
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_VppResponse);
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_dsResponse);
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_EncResponse);
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_ReconResponse);
    }
}

mfxStatus CEncodingPipeline::ReleaseResources()
{
    mfxStatus sts = MFX_ERR_NONE;

    SAFE_FCLOSE(mvout,       MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(mbstatout,   MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(mbcodeout,   MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(mvENCPAKout, MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(pMvPred,     MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(pEncMBs,     MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(pPerMbQP,    MFX_ERR_INVALID_HANDLE);

    //unlock last frames
    sts = ClearTasks();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = FreeBuffers(m_preencBufs);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    sts = FreeBuffers(m_encodeBufs);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    MSDK_SAFE_DELETE(m_ctr);
    MSDK_SAFE_DELETE(m_tmpMBpreenc);
    MSDK_SAFE_DELETE(m_tmpForMedian);
    MSDK_SAFE_DELETE(m_tmpForReading);
    MSDK_SAFE_DELETE(m_tmpMBenc);
    MSDK_SAFE_DELETE(m_last_task);

    sts = ResetRefInfo();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

void CEncodingPipeline::DeleteHWDevice()
{
    MSDK_SAFE_DELETE(m_hwdev);
}

void CEncodingPipeline::DeleteAllocator()
{
    // delete allocator
    MSDK_SAFE_DELETE(m_pMFXAllocator);
    MSDK_SAFE_DELETE(m_pmfxAllocatorParams);

    DeleteHWDevice();
}

CEncodingPipeline::CEncodingPipeline()
{
    m_pmfxDECODE          = NULL;
    m_pmfxVPP             = NULL;
    m_pmfxDS              = NULL;
    m_pmfxPREENC          = NULL;
    m_pmfxENC             = NULL;
    m_pmfxPAK             = NULL;
    m_pmfxENCODE          = NULL;
    m_pMFXAllocator       = NULL;
    m_pmfxAllocatorParams = NULL;
    m_memType             = D3D9_MEMORY; //only hw memory is supported
    m_bExternalAlloc      = false;
    m_pDecSurfaces        = NULL;
    m_pEncSurfaces        = NULL;
    m_pVPP_mfxSession     = NULL;
    m_pVppSurfaces        = NULL;
    m_pDSSurfaces         = NULL;
    m_pReconSurfaces      = NULL;
    m_nAsyncDepth         = 0;
    m_LastDecSyncPoint    = 0;
    m_tmpForMedian        = NULL;
    m_tmpForReading       = NULL;
    m_tmpMBpreenc         = NULL;
    m_tmpMBenc            = NULL;
    m_ctr                 = NULL;
    m_last_task           = NULL;
    m_bNeedDRC            = false;
    m_drcDftW             = 0;
    m_drcDftH             = 0;

    MSDK_ZERO_ARRAY(m_numOfRefs[0], 2);
    MSDK_ZERO_ARRAY(m_numOfRefs[1], 2);

    m_ref_info = {};

    MSDK_ZERO_MEMORY(m_mfxBS);

    m_FileWriters.first = m_FileWriters.second = NULL;

    MSDK_ZERO_MEMORY(m_CodingOption2);
    m_CodingOption2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    m_CodingOption2.Header.BufferSz = sizeof(m_CodingOption2);

    MSDK_ZERO_MEMORY(m_CodingOption3);
    m_CodingOption3.Header.BufferId = MFX_EXTBUFF_CODING_OPTION3;
    m_CodingOption3.Header.BufferSz = sizeof(m_CodingOption3);

    MSDK_ZERO_MEMORY(m_VppDoNotUse);
    m_VppDoNotUse.Header.BufferId = MFX_EXTBUFF_VPP_DONOTUSE;
    m_VppDoNotUse.Header.BufferSz = sizeof(m_VppDoNotUse);

#if D3D_SURFACES_SUPPORT
    m_hwdev = NULL;
#endif

    MSDK_ZERO_MEMORY(m_mfxEncParams);
    MSDK_ZERO_MEMORY(m_DecResponse);
    MSDK_ZERO_MEMORY(m_EncResponse);

    m_bEndOfFile = false;
    m_insertIDR  = false;

    m_log2frameNumMax = 8;
    m_numOfFields = 1; // default is progressive case
    m_isField     = false;

    m_frameCount = 0;
    m_frameOrderIdrInDisplayOrder = 0;
    m_frameType = PairU8((mfxU8)MFX_FRAMETYPE_UNKNOWN, (mfxU8)MFX_FRAMETYPE_UNKNOWN);
    m_frameIdrCounter = (mfxU16)(-1);
}

CEncodingPipeline::~CEncodingPipeline()
{
    Close();
}

mfxStatus CEncodingPipeline::InitFileWriter(CSmplBitstreamWriter **ppWriter, const msdk_char *filename)
{
    MSDK_CHECK_ERROR(ppWriter, NULL, MFX_ERR_NULL_PTR);

    MSDK_SAFE_DELETE(*ppWriter);
    *ppWriter = new CSmplBitstreamWriter;
    MSDK_CHECK_POINTER(*ppWriter, MFX_ERR_MEMORY_ALLOC);
    mfxStatus sts = (*ppWriter)->Init(filename);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

mfxStatus CEncodingPipeline::InitFileWriters(sInputParams *pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    // prepare output file writers
    if((pParams->bENCODE) || (pParams->bENCPAK) || (pParams->bOnlyPAK)){ //need only for ENC+PAK, only ENC + only PAK, only PAK
        sts = InitFileWriter(&m_FileWriters.first, pParams->dstFileBuff[0]);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    return sts;
}

mfxStatus CEncodingPipeline::ResetIOFiles(sInputParams & pParams)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = ResetBuffers();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (m_FileWriters.first) // no file writers for preenc
    {
        sts = m_FileWriters.first->Init(pParams.dstFileBuff[0]);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if (pParams.bDECODE)
    {
        sts = m_BSReader.Init(pParams.strSrcFile);
    }
    else
    {
        sts = m_FileReader.Init(pParams.strSrcFile,
            pParams.ColorFormat,
            0,
            pParams.srcFileBuff);
    }
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    SAFE_FSEEK(mbstatout,   0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(mvout,       0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(mvENCPAKout, 0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(mbcodeout,   0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(pMvPred,     0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(pEncMBs,     0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(pPerMbQP,    0, SEEK_SET, MFX_ERR_MORE_DATA);

    return sts;
}

mfxStatus CEncodingPipeline::Init(sInputParams *pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    m_refDist = pParams->refDist > 0 ? pParams->refDist : 1;
    m_gopSize = pParams->gopSize > 0 ? pParams->gopSize : 1;

    // prepare input file reader
    if (!pParams->bDECODE)
    {
        sts = m_FileReader.Init(pParams->strSrcFile,
            pParams->ColorFormat,
            0,
            pParams->srcFileBuff);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    sts = InitFileWriters(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Init session

    // try searching on all display adapters
    mfxIMPL impl = MFX_IMPL_HARDWARE_ANY;

    // if d3d11 surfaces are used ask the library to run acceleration through D3D11
    // feature may be unsupported due to OS or MSDK API version
    if (D3D11_MEMORY == pParams->memType)
        impl |= MFX_IMPL_VIA_D3D11;

    sts = m_mfxSession.Init(impl, NULL);

    mfxSession akaSession = m_mfxSession.operator mfxSession() ;
    m_BaseAllocID = *((mfxU32*) &(akaSession) );

    // MSDK API version may not support multiple adapters - then try initialize on the default
    if (MFX_ERR_NONE != sts)
        sts = m_mfxSession.Init((impl & (!MFX_IMPL_HARDWARE_ANY)) | MFX_IMPL_HARDWARE, NULL);

    if (pParams->bDECODE){
        sts = m_decode_mfxSession.Init(impl, NULL);

        // MSDK API version may not support multiple adapters - then try initialize on the default
        if (MFX_ERR_NONE != sts)
            sts = m_decode_mfxSession.Init((impl & (!MFX_IMPL_HARDWARE_ANY)) | MFX_IMPL_HARDWARE, NULL);
    }

    if(pParams->bPREENC){
        sts = m_preenc_mfxSession.Init(impl, NULL);

        // MSDK API version may not support multiple adapters - then try initialize on the default
        if (MFX_ERR_NONE != sts)
           sts = m_preenc_mfxSession.Init((impl & (!MFX_IMPL_HARDWARE_ANY)) | MFX_IMPL_HARDWARE, NULL);
    }

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // set memory type
    m_memType = pParams->memType;

    sts = InitMfxEncParams(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // create and init frame allocator
    sts = CreateAllocator();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (pParams->bDECODE)
    {
        // create decoder
        m_pmfxDECODE = new MFXVideoDECODE(m_decode_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxDECODE, MFX_ERR_MEMORY_ALLOC);

        sts = InitMfxDecodeParams(pParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // create preprocessor if resizing was requested from command line
    if (pParams->nWidth  != pParams->nDstWidth ||
        pParams->nHeight != pParams->nDstHeight||
        m_bNeedDRC)
    {
        if (pParams->bDECODE)
            m_pVPP_mfxSession = &m_decode_mfxSession;
        else if (pParams->bPREENC && !pParams->preencDSstrength) // in case of downscaled input surfaces for PreEnc, its session already contains VPP
        {
            m_pVPP_mfxSession = &m_preenc_mfxSession;
        }
        else
            m_pVPP_mfxSession = &m_mfxSession;
        m_pmfxVPP = new MFXVideoVPP(*m_pVPP_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxVPP, MFX_ERR_MEMORY_ALLOC);

        sts = InitMfxVppParams(pParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    else
        m_pVPP_mfxSession = NULL;

    //if(pParams->bENCODE){
        // create encoder
        m_pmfxENCODE = new MFXVideoENCODE(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxENCODE, MFX_ERR_MEMORY_ALLOC);
    //}

    if(pParams->bPREENC){
        m_pmfxPREENC = new MFXVideoENC(m_preenc_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxPREENC, MFX_ERR_MEMORY_ALLOC);

        if (pParams->preencDSstrength)
        {
            if (!m_pmfxVPP)
            {
                sts = InitMfxVppParams(pParams);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            m_pmfxDS = new MFXVideoVPP(m_preenc_mfxSession);
            MSDK_CHECK_POINTER(m_pmfxDS, MFX_ERR_MEMORY_ALLOC);
        }
    }

    if(pParams->bENCPAK){
        m_pmfxENC = new MFXVideoENC(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_MEMORY_ALLOC);

        m_pmfxPAK = new MFXVideoPAK(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxPAK, MFX_ERR_MEMORY_ALLOC);
    }

    if(pParams->bOnlyENC){
        // create ENC
        m_pmfxENC = new MFXVideoENC(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_MEMORY_ALLOC);
    }

    if(pParams->bOnlyPAK){
        // create PAK
        m_pmfxPAK = new MFXVideoPAK(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxPAK, MFX_ERR_MEMORY_ALLOC);
    }

    m_nAsyncDepth = 1; // this number can be tuned for better performance

    sts = ResetMFXComponents(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

void CEncodingPipeline::Close()
{
    if (m_FileWriters.first)
        msdk_printf(MSDK_STRING("Frame number: %u\r"), m_frameCount);

    MSDK_SAFE_DELETE(m_pmfxDECODE);
    MSDK_SAFE_DELETE(m_pmfxVPP);
    MSDK_SAFE_DELETE(m_pmfxDS);
    MSDK_SAFE_DELETE(m_pmfxENCODE);
    MSDK_SAFE_DELETE(m_pmfxENC);
    MSDK_SAFE_DELETE(m_pmfxPAK);
    MSDK_SAFE_DELETE(m_pmfxPREENC);

    MSDK_SAFE_DELETE_ARRAY(m_VppDoNotUse.AlgList);

    if (m_encpakParams.DisableDeblockingIdc || m_encpakParams.SliceAlphaC0OffsetDiv2 ||
        m_encpakParams.SliceBetaOffsetDiv2)
    {
        for (size_t field = 0; field < m_numOfFields; field++)
        {
            MSDK_SAFE_DELETE_ARRAY(m_encodeSliceHeader[field].Slice);
        }
    }

    DeleteFrames();

    m_TaskPool.Close();
    m_mfxSession.Close();
    if (m_encpakParams.bDECODE)
    {
        m_decode_mfxSession.Close();
    }
    if (m_encpakParams.bPREENC){
        m_preenc_mfxSession.Close();
    }

    // allocator if used as external for MediaSDK must be deleted after SDK components
    DeleteAllocator();

    m_FileReader.Close();
    FreeFileWriters();

    WipeMfxBitstream(&m_mfxBS);
}

void CEncodingPipeline::FreeFileWriters()
{
    if (m_FileWriters.second == m_FileWriters.first)
    {
        m_FileWriters.second = NULL; // second do not own the writer - just forget pointer
    }

    if (m_FileWriters.first)
        m_FileWriters.first->Close();
    MSDK_SAFE_DELETE(m_FileWriters.first);

    if (m_FileWriters.second)
        m_FileWriters.second->Close();
    MSDK_SAFE_DELETE(m_FileWriters.second);
}

mfxStatus CEncodingPipeline::ResetMFXComponents(sInputParams* pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    if (m_pmfxDECODE)
    {
        sts = m_pmfxDECODE->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if (m_pmfxVPP)
    {
        sts = m_pmfxVPP->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if (m_pmfxDS)
    {
        sts = m_pmfxDS->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if(m_pmfxENCODE)
    {
        sts = m_pmfxENCODE->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // free allocated frames
    DeleteFrames();

    m_TaskPool.Close();

    sts = AllocFrames();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //use encoded order if both is used
    if (m_encpakParams.bPREENC && (m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC)){
        m_mfxEncParams.mfx.EncodedOrder = 1;
    }

    // Init decode
    if (m_pmfxDECODE)
    {
        m_mfxDecParams.AllocId     = m_BaseAllocID;
        sts = m_pmfxDECODE->Init(&m_mfxDecParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if (m_pmfxVPP)
    {
        m_mfxVppParams.AllocId     = m_BaseAllocID;
        sts = m_pmfxVPP->Init(&m_mfxVppParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if ((m_encpakParams.bENCODE) && (m_pmfxENCODE))
    {
        sts = m_pmfxENCODE->Init(&m_mfxEncParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if(m_pmfxPREENC)
    {
        mfxVideoParam preEncParams = m_mfxEncParams;

        if (m_pmfxDS)
        {
            sts = m_pmfxDS->Init(&m_mfxDSParams);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            // PREENC will be performed on downscaled picture
            preEncParams.mfx.FrameInfo.Width = m_mfxDSParams.vpp.Out.Width;
            preEncParams.mfx.FrameInfo.Height = m_mfxDSParams.vpp.Out.Height;
            preEncParams.mfx.FrameInfo.CropW = m_mfxDSParams.vpp.Out.Width;
            preEncParams.mfx.FrameInfo.CropH = m_mfxDSParams.vpp.Out.Height;
        }

        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_PREENC;
        if (pParams->bFieldProcessingMode)
            m_encpakInit.SingleFieldProcessing = MFX_CODINGOPTION_ON;

        mfxExtBuffer* buf[1];
        buf[0] = (mfxExtBuffer*)&m_encpakInit;

        preEncParams.NumExtParam = 1;
        preEncParams.ExtParam    = buf;
        preEncParams.AllocId     = m_BaseAllocID;

        sts = m_pmfxPREENC->Init(&preEncParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts) {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if(m_pmfxENC)
    {
        mfxVideoParam encParams = m_mfxEncParams;

        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_ENC;
        if (pParams->bFieldProcessingMode)
            m_encpakInit.SingleFieldProcessing = MFX_CODINGOPTION_ON;

        mfxExtBuffer* buf[1];
        buf[0] = (mfxExtBuffer*)&m_encpakInit;

        encParams.NumExtParam = 1;
        encParams.ExtParam    = buf;
        encParams.AllocId     = m_BaseAllocID;

        sts = m_pmfxENC->Init(&encParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if(m_pmfxPAK)
    {
        mfxVideoParam pakParams = m_mfxEncParams;

        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_PAK;
        if (pParams->bFieldProcessingMode)
            m_encpakInit.SingleFieldProcessing = MFX_CODINGOPTION_ON;

        mfxExtBuffer* buf[1];
        buf[0] = (mfxExtBuffer*)&m_encpakInit;

        pakParams.NumExtParam = 1;
        pakParams.ExtParam    = buf;
        pakParams.AllocId     = m_BaseAllocID;

        sts = m_pmfxPAK->Init(&pakParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    mfxU32 nEncodedDataBufferSize = m_mfxEncParams.mfx.FrameInfo.Width * m_mfxEncParams.mfx.FrameInfo.Height * 4;
    sts = m_TaskPool.Init(&m_mfxSession, m_FileWriters.first, m_nAsyncDepth * 2, nEncodedDataBufferSize, m_FileWriters.second);
    //sts = m_TaskPool.Init(&m_mfxSession, m_FileWriters.first, 1, nEncodedDataBufferSize, m_FileWriters.second);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocateSufficientBuffer(mfxBitstream* pBS)
{
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pmfxENCODE, MFX_ERR_NOT_INITIALIZED);

    mfxVideoParam par;
    MSDK_ZERO_MEMORY(par);

    // find out the required buffer size
    mfxStatus sts = m_pmfxENCODE->GetVideoParam(&par);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // reallocate bigger buffer for output
    sts = ExtendMfxBitstream(pBS, par.mfx.BufferSizeInKB * 1000);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(pBS));

    return MFX_ERR_NONE;
}


mfxStatus CEncodingPipeline::UpdateVideoParams()
{
    MSDK_CHECK_POINTER(m_pmfxENCODE, MFX_ERR_NOT_INITIALIZED); // temporary

    mfxStatus sts = MFX_ERR_NOT_FOUND;
    if (m_pmfxENCODE)
    {
        sts = m_pmfxENCODE->GetVideoParam(&m_mfxEncParams);
        return sts;
    }
    else if (m_pmfxPREENC)
    {
        //sts = m_pmfxPREENC->GetVideoParam(&m_mfxEncParams);
        return sts;
    }
    else if (m_pmfxENC)
    {
        //sts = m_pmfxENC->GetVideoParam(&m_mfxEncParams);
        return sts;
    }
    else if (m_pmfxPAK)
    {
        //sts = m_pmfxPAK->GetVideoParam(&m_mfxEncParams);
        return sts;
    }

    return sts;
}

mfxStatus CEncodingPipeline::GetFreeTask(sTask **ppTask)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_TaskPool.GetFreeTask(ppTask);
    if (MFX_ERR_NOT_FOUND == sts)
    {
        sts = SynchronizeFirstTask();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // try again
        sts = m_TaskPool.GetFreeTask(ppTask);
    }

    return sts;
}

PairU8 CEncodingPipeline::GetFrameType(mfxU32 frameOrder)
{
    mfxU32 gopOptFlag = m_mfxEncParams.mfx.GopOptFlag;
    mfxU32 gopPicSize = m_mfxEncParams.mfx.GopPicSize;
    mfxU32 gopRefDist = m_mfxEncParams.mfx.GopRefDist;
    mfxU32 idrPicDist = gopPicSize * (m_mfxEncParams.mfx.IdrInterval + 1);

    if (gopPicSize == 0xffff) //infinite GOP
        idrPicDist = gopPicSize = 0xffffffff;

    if (frameOrder % idrPicDist == 0)
        return ExtendFrameType(MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

    if (frameOrder % gopPicSize == 0)
        return ExtendFrameType(MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF);

    if (frameOrder % gopPicSize % gopRefDist == 0)
        return ExtendFrameType(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);

    if ((gopOptFlag & MFX_GOP_STRICT) == 0)
        if (((frameOrder + 1) % gopPicSize == 0 && (gopOptFlag & MFX_GOP_CLOSED)) ||
            (frameOrder + 1) % idrPicDist == 0)
            return ExtendFrameType(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF); // switch last B frame to P frame

    bool adjust_last_B = !(gopOptFlag & MFX_GOP_STRICT) && m_encpakParams.nNumFrames && (frameOrder + 1) >= m_encpakParams.nNumFrames;
    return adjust_last_B ? ExtendFrameType(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF) : ExtendFrameType(MFX_FRAMETYPE_B);
}

PairU8 ExtendFrameType(mfxU32 type)
{
    mfxU32 type1 = type & 0xff;
    mfxU32 type2 = type >> 8;

    if (type2 == 0)
    {
        type2 = type1 & ~MFX_FRAMETYPE_IDR; // second field can't be IDR

        if (type1 & MFX_FRAMETYPE_I)
        {
            type2 &= ~MFX_FRAMETYPE_I;
            type2 |= MFX_FRAMETYPE_P;
        }
    }

    return PairU8(type1, type2);
}

BiFrameLocation CEncodingPipeline::GetBiFrameLocation(mfxU32 frameOrder)
{

    mfxU32 gopPicSize = m_mfxEncParams.mfx.GopPicSize;
    mfxU32 gopRefDist = m_mfxEncParams.mfx.GopRefDist;
    mfxU32 biPyramid  = m_CodingOption2.BRefType;

    BiFrameLocation loc;

    if (gopPicSize == 0xffff) //infinite GOP
        gopPicSize = 0xffffffff;

    if (biPyramid != MFX_B_REF_OFF)
    {
        bool ref = false;
        mfxU32 orderInMiniGop = frameOrder % gopPicSize % gopRefDist - 1;

        loc.encodingOrder = GetEncodingOrder(orderInMiniGop, 0, gopRefDist - 1, 0, ref);
        loc.miniGopCount = frameOrder % gopPicSize / gopRefDist;
        loc.refFrameFlag = mfxU16(ref ? MFX_FRAMETYPE_REF : 0);
    }

    return loc;
}

mfxU32 GetEncodingOrder(mfxU32 displayOrder, mfxU32 begin, mfxU32 end, mfxU32 counter, bool & ref)
{
    //assert(displayOrder >= begin);
    //assert(displayOrder <  end);

    ref = (end - begin > 1);

    mfxU32 pivot = (begin + end) / 2;
    if (displayOrder == pivot)
        return counter;
    else if (displayOrder < pivot)
        return GetEncodingOrder(displayOrder, begin, pivot, counter + 1, ref);
    else
        return GetEncodingOrder(displayOrder, pivot + 1, end, counter + 1 + pivot - begin, ref);
}

mfxStatus CEncodingPipeline::SynchronizeFirstTask()
{
    mfxStatus sts = m_TaskPool.SynchronizeFirstTask();

    return sts;
}

mfxStatus CEncodingPipeline::InitInterfaces()
{
    mfxU32 fieldId = 0;
    int numExtInParams = 0, numExtInParamsI = 0, numExtOutParams = 0, numExtOutParamsI = 0;

    if (m_encpakParams.bPREENC)
    {
        //setup control structures
        m_disableMVoutPreENC     = (m_encpakParams.mvoutFile     == NULL) && !(m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC);
        m_disableMBStatPreENC    = (m_encpakParams.mbstatoutFile == NULL) && !(m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC);
        bool enableMVpredictor   = m_encpakParams.mvinFile != NULL;
        bool enableMBQP          = m_encpakParams.mbQpFile != NULL        && !(m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC);

        bufSet*                      tmpForInit = NULL;
        mfxExtFeiPreEncCtrl*         preENCCtr  = NULL;
        mfxExtFeiPreEncMVPredictors* mvPreds    = NULL;
        mfxExtFeiEncQP*              qps        = NULL;
        mfxExtFeiPreEncMV*           mvs        = NULL;
        mfxExtFeiPreEncMBStat*       mbdata     = NULL;

        int num_buffers = m_maxQueueLength + (m_encpakParams.bDECODE ? this->m_decodePoolSize : 0) + (m_pmfxVPP ? 2 : 0) + 4;
        num_buffers = IPP_MAX(num_buffers, m_maxQueueLength*m_encpakParams.numRef);

        for (int k = 0; k < num_buffers; k++)
        {
            numExtInParams   = 0;
            numExtInParamsI  = 0;
            numExtOutParams  = 0;
            numExtOutParamsI = 0;

            tmpForInit = new bufSet;
            MSDK_CHECK_POINTER(tmpForInit, MFX_ERR_MEMORY_ALLOC);
            MSDK_ZERO_MEMORY(*tmpForInit);
            tmpForInit->vacant = true;

            for (fieldId = 0; fieldId < m_numOfFields; fieldId++)
            {
                if (fieldId == 0){
                    preENCCtr = new mfxExtFeiPreEncCtrl[m_numOfFields];
                    MSDK_CHECK_POINTER(preENCCtr, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(preENCCtr, m_numOfFields);
                }
                numExtInParams++;
                numExtInParamsI++;

                preENCCtr[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_CTRL;
                preENCCtr[fieldId].Header.BufferSz = sizeof(mfxExtFeiPreEncCtrl);
                preENCCtr[fieldId].PictureType             = GetCurPicType(fieldId);
                preENCCtr[fieldId].RefPictureType[0]       = preENCCtr[fieldId].PictureType;
                preENCCtr[fieldId].RefPictureType[1]       = preENCCtr[fieldId].PictureType;
                preENCCtr[fieldId].DownsampleReference[0]  = MFX_CODINGOPTION_OFF; // in sample_fei preenc works only in encoded order
                preENCCtr[fieldId].DownsampleReference[1]  = MFX_CODINGOPTION_OFF; // so all references would be already downsampled
                preENCCtr[fieldId].DisableMVOutput         = m_disableMVoutPreENC;
                preENCCtr[fieldId].DisableStatisticsOutput = m_disableMBStatPreENC;
                preENCCtr[fieldId].FTEnable                = m_encpakParams.FTEnable;
                preENCCtr[fieldId].AdaptiveSearch          = m_encpakParams.AdaptiveSearch;
                preENCCtr[fieldId].LenSP                   = m_encpakParams.LenSP;
                preENCCtr[fieldId].MBQp                    = enableMBQP;
                preENCCtr[fieldId].MVPredictor             = enableMVpredictor;
                preENCCtr[fieldId].RefHeight               = m_encpakParams.RefHeight;
                preENCCtr[fieldId].RefWidth                = m_encpakParams.RefWidth;
                preENCCtr[fieldId].SubPelMode              = m_encpakParams.SubPelMode;
                preENCCtr[fieldId].SearchWindow            = m_encpakParams.SearchWindow;
                preENCCtr[fieldId].SearchPath              = m_encpakParams.SearchPath;
                preENCCtr[fieldId].Qp                      = m_encpakParams.QP;
                preENCCtr[fieldId].IntraSAD                = m_encpakParams.IntraSAD;
                preENCCtr[fieldId].InterSAD                = m_encpakParams.InterSAD;
                preENCCtr[fieldId].SubMBPartMask           = m_encpakParams.SubMBPartMask;
                preENCCtr[fieldId].IntraPartMask           = m_encpakParams.IntraPartMask;
                preENCCtr[fieldId].Enable8x8Stat           = m_encpakParams.Enable8x8Stat;

                if (preENCCtr[fieldId].MVPredictor)
                {
                    if (fieldId == 0){
                        mvPreds = new mfxExtFeiPreEncMVPredictors[m_numOfFields];
                        MSDK_CHECK_POINTER(mvPreds, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(mvPreds, m_numOfFields);
                    }
                    numExtInParams++;
                    numExtInParamsI++;

                    mvPreds[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV_PRED;
                    mvPreds[fieldId].Header.BufferSz = sizeof(mfxExtFeiPreEncMVPredictors);
                    mvPreds[fieldId].NumMBAlloc = m_numMBpreenc;
                    mvPreds[fieldId].MB = new mfxExtFeiPreEncMVPredictors::mfxExtFeiPreEncMVPredictorsMB[m_numMBpreenc];
                    MSDK_CHECK_POINTER(mvPreds[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(mvPreds[fieldId].MB, m_numMBpreenc);

                    if ((pMvPred == NULL) && (m_encpakParams.mvinFile != NULL))
                    {
                        //read mvs from file
                        printf("Using MV input file: %s\n", m_encpakParams.mvinFile);
                        MSDK_FOPEN(pMvPred, m_encpakParams.mvinFile, MSDK_CHAR("rb"));
                        if (pMvPred == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_encpakParams.mvinFile);
                            exit(-1);
                        }
                    }
                }

                if (preENCCtr[fieldId].MBQp)
                {
                    if (fieldId == 0){
                        qps = new mfxExtFeiEncQP[m_numOfFields];
                        MSDK_CHECK_POINTER(qps, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(qps, m_numOfFields);
                    }
                    numExtInParams++;
                    numExtInParamsI++;

                    qps[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_QP;
                    qps[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncQP);
                    qps[fieldId].NumQPAlloc = m_numMBpreenc;
                    qps[fieldId].QP = new mfxU8[m_numMBpreenc];
                    MSDK_CHECK_POINTER(qps[fieldId].QP, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(qps[fieldId].QP, m_numMBpreenc);

                    if ((pPerMbQP == NULL) && (m_encpakParams.mbQpFile != NULL))
                    {
                        //read mvs from file
                        printf("Using QP input file: %s\n", m_encpakParams.mbQpFile);
                        MSDK_FOPEN(pPerMbQP, m_encpakParams.mbQpFile, MSDK_CHAR("rb"));
                        if (pPerMbQP == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_encpakParams.mbQpFile);
                            exit(-1);
                        }
                    }
                }

                if (!preENCCtr[fieldId].DisableMVOutput)
                {
                    if (fieldId == 0){
                        mvs = new mfxExtFeiPreEncMV[m_numOfFields];
                        MSDK_CHECK_POINTER(mvs, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(mvs, m_numOfFields);
                    }
                    numExtOutParams++;

                    mvs[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV;
                    mvs[fieldId].Header.BufferSz = sizeof(mfxExtFeiPreEncMV);
                    mvs[fieldId].NumMBAlloc = m_numMBpreenc;
                    mvs[fieldId].MB = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB[m_numMBpreenc];
                    MSDK_CHECK_POINTER(mvs[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(mvs[fieldId].MB, m_numMBpreenc);

                    if ((mvout == NULL) && (m_encpakParams.mvoutFile != NULL) &&
                        !(m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC))
                    {
                        printf("Using MV output file: %s\n", m_encpakParams.mvoutFile);
                        m_tmpMBpreenc = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB;
                        memset(m_tmpMBpreenc, 0x8000, sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB));
                        MSDK_FOPEN(mvout, m_encpakParams.mvoutFile, MSDK_CHAR("wb"));
                        if (mvout == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_encpakParams.mvoutFile);
                            exit(-1);
                        }
                    }
                }

                if (!preENCCtr[fieldId].DisableStatisticsOutput)
                {
                    if (fieldId == 0){
                        mbdata = new mfxExtFeiPreEncMBStat[m_numOfFields];
                        MSDK_CHECK_POINTER(mbdata, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(mbdata, m_numOfFields);
                    }
                    numExtOutParams++;
                    numExtOutParamsI++;

                    mbdata[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MB;
                    mbdata[fieldId].Header.BufferSz = sizeof(mfxExtFeiPreEncMBStat);
                    mbdata[fieldId].NumMBAlloc = m_numMBpreenc;
                    mbdata[fieldId].MB = new mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB[m_numMBpreenc];
                    MSDK_CHECK_POINTER(mbdata[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(mbdata[fieldId].MB, m_numMBpreenc);

                    if ((mbstatout == NULL) && (m_encpakParams.mbstatoutFile != NULL))
                    {
                        printf("Using MB distortion output file: %s\n", m_encpakParams.mbstatoutFile);
                        MSDK_FOPEN(mbstatout, m_encpakParams.mbstatoutFile, MSDK_CHAR("wb"));
                        if (mbstatout == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_encpakParams.mbstatoutFile);
                            exit(-1);
                        }
                    }
                }

            } // for (fieldId = 0; fieldId < m_numOfFields; fieldId++)

            tmpForInit-> I_bufs.in.ExtParam  = new mfxExtBuffer*[numExtInParamsI];
            MSDK_CHECK_POINTER(tmpForInit-> I_bufs.in.ExtParam,  MFX_ERR_MEMORY_ALLOC);
            tmpForInit->PB_bufs.in.ExtParam  = new mfxExtBuffer*[numExtInParams];
            MSDK_CHECK_POINTER(tmpForInit->PB_bufs.in.ExtParam,  MFX_ERR_MEMORY_ALLOC);
            tmpForInit-> I_bufs.out.ExtParam = new mfxExtBuffer*[numExtOutParamsI];
            MSDK_CHECK_POINTER(tmpForInit-> I_bufs.out.ExtParam, MFX_ERR_MEMORY_ALLOC);
            tmpForInit->PB_bufs.out.ExtParam = new mfxExtBuffer*[numExtOutParams];
            MSDK_CHECK_POINTER(tmpForInit->PB_bufs.out.ExtParam, MFX_ERR_MEMORY_ALLOC);

            MSDK_ZERO_ARRAY(tmpForInit-> I_bufs.in.ExtParam,  numExtInParamsI);
            MSDK_ZERO_ARRAY(tmpForInit->PB_bufs.in.ExtParam,  numExtInParams);
            MSDK_ZERO_ARRAY(tmpForInit-> I_bufs.out.ExtParam, numExtOutParamsI);
            MSDK_ZERO_ARRAY(tmpForInit->PB_bufs.out.ExtParam, numExtOutParams);

            for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(preENCCtr + fieldId);
                tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(preENCCtr + fieldId);
            }
            if (enableMVpredictor){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(mvPreds + fieldId);
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(mvPreds + fieldId);
                }
            }
            if (enableMBQP){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(qps + fieldId);
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(qps + fieldId);
                }
            }
            if (!m_disableMVoutPreENC){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit->PB_bufs.out.ExtParam[tmpForInit->PB_bufs.out.NumExtParam++] = (mfxExtBuffer*)(mvs + fieldId);
                }
            }
            if (!m_disableMBStatPreENC){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.out.ExtParam[tmpForInit-> I_bufs.out.NumExtParam++] = (mfxExtBuffer*)(mbdata + fieldId);
                    tmpForInit->PB_bufs.out.ExtParam[tmpForInit->PB_bufs.out.NumExtParam++] = (mfxExtBuffer*)(mbdata + fieldId);
                }
            }

            m_preencBufs.push_back(tmpForInit);
        } // for (int k = 0; k < num_buffers; k++)
    } // if (m_encpakParams.bPREENC)

    if ((m_encpakParams.bENCODE) || (m_encpakParams.bENCPAK) ||
        (m_encpakParams.bOnlyPAK) || (m_encpakParams.bOnlyENC))
    {
        bufSet*                   tmpForInit         = NULL;
        //FEI buffers
        mfxExtFeiSPS*             feiSPS             = NULL;
        mfxExtFeiPPS*             feiPPS             = NULL;
        mfxExtFeiSliceHeader*     feiSliceHeader     = NULL;
        mfxExtFeiEncFrameCtrl*    feiEncCtrl         = NULL;
        mfxExtFeiEncMVPredictors* feiEncMVPredictors = NULL;
        mfxExtFeiEncMBCtrl*       feiEncMBCtrl       = NULL;
        mfxExtFeiEncMBStat*       feiEncMbStat       = NULL;
        mfxExtFeiEncMV*           feiEncMV           = NULL;
        mfxExtFeiEncQP*           feiEncMbQp         = NULL;
        mfxExtFeiPakMBCtrl*       feiEncMBCode       = NULL;

        m_ctr = new mfxEncodeCtrl;
        MSDK_CHECK_POINTER(m_ctr, MFX_ERR_MEMORY_ALLOC);
        MSDK_ZERO_MEMORY(*m_ctr);
        m_ctr->FrameType = MFX_FRAMETYPE_UNKNOWN;
        m_ctr->QP = m_encpakParams.QP;

        bool MVPredictors = (m_encpakParams.mvinFile   != NULL) || m_encpakParams.bPREENC; //couple with PREENC
        bool MBCtrl       = m_encpakParams.mbctrinFile != NULL;
        bool MBQP         = m_encpakParams.mbQpFile    != NULL;

        bool MBStatOut    = m_encpakParams.mbstatoutFile != NULL;
        bool MVOut        = m_encpakParams.mvoutFile     != NULL;
        bool MBCodeOut    = m_encpakParams.mbcodeoutFile != NULL;

        bool nonDefDblk = (m_encpakParams.DisableDeblockingIdc || m_encpakParams.SliceAlphaC0OffsetDiv2 ||
            m_encpakParams.SliceBetaOffsetDiv2) && !m_encpakParams.bENCODE;

        /*SPS Header */
        if (m_encpakParams.bPassHeaders)
        {
            feiSPS = new mfxExtFeiSPS;
            MSDK_CHECK_POINTER(feiSPS, MFX_ERR_MEMORY_ALLOC);
            MSDK_ZERO_MEMORY(*feiSPS);
            feiSPS->Header.BufferId = MFX_EXTBUFF_FEI_SPS;
            feiSPS->Header.BufferSz = sizeof(mfxExtFeiSPS);
            feiSPS->SPSId   = 0;
            feiSPS->Profile = m_encpakParams.CodecProfile;
            feiSPS->Level   = m_encpakParams.CodecLevel;

            feiSPS->NumRefFrame = m_mfxEncParams.mfx.NumRefFrame;
            feiSPS->WidthInMBs  = m_widthMB;
            feiSPS->HeightInMBs = m_heightMB;

            feiSPS->ChromaFormatIdc  = m_mfxEncParams.mfx.FrameInfo.ChromaFormat;
            feiSPS->FrameMBsOnlyFlag = (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 0;
            feiSPS->MBAdaptiveFrameFieldFlag = 0;
            feiSPS->Direct8x8InferenceFlag   = 1;
            feiSPS->Log2MaxFrameNum = 8;
            feiSPS->PicOrderCntType = GetDefaultPicOrderCount(m_mfxEncParams);
            feiSPS->Log2MaxPicOrderCntLsb       = 4;
            feiSPS->DeltaPicOrderAlwaysZeroFlag = 1;
        }

        int num_buffers = m_maxQueueLength + (m_encpakParams.bDECODE ? this->m_decodePoolSize : 0) + (m_pmfxVPP ? 2 : 0);

        bool is_8x8tranform_forced = false;
        for (int k = 0; k < num_buffers; k++)
        {
            tmpForInit = new bufSet;
            MSDK_CHECK_POINTER(tmpForInit, MFX_ERR_MEMORY_ALLOC);
            MSDK_ZERO_MEMORY(*tmpForInit);
            tmpForInit->vacant = true;

            numExtInParams   = m_encpakParams.bPassHeaders ? 1 : 0; // count SPS header
            numExtInParamsI  = m_encpakParams.bPassHeaders ? 1 : 0; // count SPS header
            numExtOutParams  = 0;
            numExtOutParamsI = 0;

            for (fieldId = 0; fieldId < m_numOfFields; fieldId++)
            {
                if (fieldId == 0){
                    feiEncCtrl = new mfxExtFeiEncFrameCtrl[m_numOfFields];
                    MSDK_CHECK_POINTER(feiEncCtrl, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncCtrl, m_numOfFields);
                }
                numExtInParams++;
                numExtInParamsI++;

                feiEncCtrl[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
                feiEncCtrl[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);

                feiEncCtrl[fieldId].SearchPath             = m_encpakParams.SearchPath;
                feiEncCtrl[fieldId].LenSP                  = m_encpakParams.LenSP;
                feiEncCtrl[fieldId].SubMBPartMask          = m_encpakParams.SubMBPartMask;
                feiEncCtrl[fieldId].MultiPredL0            = m_encpakParams.MultiPredL0;
                feiEncCtrl[fieldId].MultiPredL1            = m_encpakParams.MultiPredL1;
                feiEncCtrl[fieldId].SubPelMode             = m_encpakParams.SubPelMode;
                feiEncCtrl[fieldId].InterSAD               = m_encpakParams.InterSAD;
                feiEncCtrl[fieldId].IntraSAD               = m_encpakParams.IntraSAD;
                feiEncCtrl[fieldId].IntraPartMask          = m_encpakParams.IntraPartMask;
                feiEncCtrl[fieldId].DistortionType         = m_encpakParams.DistortionType;
                feiEncCtrl[fieldId].RepartitionCheckEnable = m_encpakParams.RepartitionCheckEnable;
                feiEncCtrl[fieldId].AdaptiveSearch         = m_encpakParams.AdaptiveSearch;
                feiEncCtrl[fieldId].MVPredictor            = MVPredictors;

                mfxU16 nmvp_l0 = m_encpakParams.bNPredSpecified_l0 ?
                    m_encpakParams.NumMVPredictors[0] : IPP_MIN(m_mfxEncParams.mfx.NumRefFrame*(mfxU16)m_numOfFields, MaxFeiEncMVPNum);
                mfxU16 nmvp_l1 = m_encpakParams.bNPredSpecified_l1 ?
                    m_encpakParams.NumMVPredictors[1] : IPP_MIN(m_mfxEncParams.mfx.NumRefFrame*(mfxU16)m_numOfFields, MaxFeiEncMVPNum);

                feiEncCtrl[fieldId].NumMVPredictors[0] = m_numOfRefs[fieldId][0] = nmvp_l0;
                feiEncCtrl[fieldId].NumMVPredictors[1] = m_numOfRefs[fieldId][1] = nmvp_l1;
                feiEncCtrl[fieldId].PerMBQp                = MBQP;
                feiEncCtrl[fieldId].PerMBInput             = MBCtrl;
                feiEncCtrl[fieldId].MBSizeCtrl             = m_encpakParams.bMBSize;
                feiEncCtrl[fieldId].ColocatedMbDistortion  = m_encpakParams.ColocatedMbDistortion;
                //Note:
                //(RefHeight x RefWidth) should not exceed 2048 for P frames and 1024 for B frames
                feiEncCtrl[fieldId].RefHeight    = m_encpakParams.RefHeight;
                feiEncCtrl[fieldId].RefWidth     = m_encpakParams.RefWidth;
                feiEncCtrl[fieldId].SearchWindow = m_encpakParams.SearchWindow;

                /* PPS Header */
                if (m_encpakParams.bPassHeaders)
                {
                    if (fieldId == 0){
                        feiPPS = new mfxExtFeiPPS[m_numOfFields];
                        MSDK_CHECK_POINTER(feiPPS, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiPPS, m_numOfFields);
                    }
                    numExtInParams++;
                    numExtInParamsI++;

                    feiPPS[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PPS;
                    feiPPS[fieldId].Header.BufferSz = sizeof(mfxExtFeiPPS);

                    feiPPS[fieldId].SPSId = feiSPS ? feiSPS->SPSId : 0;
                    feiPPS[fieldId].PPSId = 0;

                    feiPPS[fieldId].FrameNum = (mfxU16)(2*m_frameCount + fieldId);

                    //feiPPS[fieldId].PicInitQP = (m_encpakParams.QP != 0) ? m_encpakParams.QP : 26;
                    /* PicInitQP should be always 26 !!!
                     * Adjusting of QP parameter should be done via Slice header */
                    feiPPS[fieldId].PicInitQP = 26;

                    feiPPS[fieldId].NumRefIdxL0Active = 1;
                    feiPPS[fieldId].NumRefIdxL1Active = 1;

                    feiPPS[fieldId].ChromaQPIndexOffset       = m_encpakParams.ChromaQPIndexOffset;
                    feiPPS[fieldId].SecondChromaQPIndexOffset = m_encpakParams.SecondChromaQPIndexOffset;

                    feiPPS[fieldId].IDRPicFlag                = 0;
                    feiPPS[fieldId].ReferencePicFlag          = 0;
                    feiPPS[fieldId].EntropyCodingModeFlag     = 1;
                    feiPPS[fieldId].ConstrainedIntraPredFlag  = m_encpakParams.ConstrainedIntraPredFlag;
                    feiPPS[fieldId].Transform8x8ModeFlag      = m_encpakParams.Transform8x8ModeFlag;
                    /*
                    IntraPartMask description from manual
                    This value specifies what block and sub-block partitions are enabled for intra MBs.
                    0x01 - 16x16 is disabled
                    0x02 - 8x8 is disabled
                    0x04 - 4x4 is disabled

                    So on, there is additional condition for Transform8x8ModeFlag:
                    If partitions below 16x16 present Transform8x8ModeFlag flag should be ON
                     * */
                    if (!(feiEncCtrl[fieldId].IntraPartMask &0x02) || !(feiEncCtrl[fieldId].IntraPartMask &0x04) )
                    {
                        feiPPS[fieldId].Transform8x8ModeFlag = 1;
                        is_8x8tranform_forced = true;
                    }
                }

                /* Slice Header */
                if (m_encpakParams.bPassHeaders || nonDefDblk)
                {
                    if (fieldId == 0){
                        feiSliceHeader = new mfxExtFeiSliceHeader[m_numOfFields];
                        MSDK_CHECK_POINTER(feiSliceHeader, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiSliceHeader, m_numOfFields);
                    }
                    numExtInParams++;
                    numExtInParamsI++;

                    feiSliceHeader[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_SLICE;
                    feiSliceHeader[fieldId].Header.BufferSz = sizeof(mfxExtFeiSliceHeader);

                    feiSliceHeader[fieldId].NumSlice =
                        feiSliceHeader[fieldId].NumSliceAlloc = m_encpakParams.numSlices;
                    feiSliceHeader[fieldId].Slice = new mfxExtFeiSliceHeader::mfxSlice[m_encpakParams.numSlices];
                    MSDK_CHECK_POINTER(feiSliceHeader[fieldId].Slice, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiSliceHeader[fieldId].Slice, m_encpakParams.numSlices);

                    // TODO: Implement real slice divider
                    // For now only one slice is supported
                    mfxU16 nMBrows = (m_heightMB + feiSliceHeader[fieldId].NumSlice - 1) / feiSliceHeader[fieldId].NumSlice,
                        nMBremain = m_heightMB;
                    for (mfxU16 numSlice = 0; numSlice < feiSliceHeader[fieldId].NumSlice; numSlice++)
                    {
                        feiSliceHeader[fieldId].Slice[numSlice].MBAaddress = numSlice*(nMBrows*m_widthMB);
                        feiSliceHeader[fieldId].Slice[numSlice].NumMBs     = MSDK_MIN(nMBrows, nMBremain)*m_widthMB;
                        feiSliceHeader[fieldId].Slice[numSlice].SliceType  = 0;
                        feiSliceHeader[fieldId].Slice[numSlice].PPSId      = feiPPS ? feiPPS[fieldId].PPSId : 0;
                        feiSliceHeader[fieldId].Slice[numSlice].IdrPicId   = 0;

                        feiSliceHeader[fieldId].Slice[numSlice].CabacInitIdc = 0;
                        mfxU32 initQP = (m_encpakParams.QP != 0) ? m_encpakParams.QP : 26;
                        feiSliceHeader[fieldId].Slice[numSlice].SliceQPDelta               = (mfxI16)(initQP - feiPPS[fieldId].PicInitQP);
                        feiSliceHeader[fieldId].Slice[numSlice].DisableDeblockingFilterIdc = m_encpakParams.DisableDeblockingIdc;
                        feiSliceHeader[fieldId].Slice[numSlice].SliceAlphaC0OffsetDiv2     = m_encpakParams.SliceAlphaC0OffsetDiv2;
                        feiSliceHeader[fieldId].Slice[numSlice].SliceBetaOffsetDiv2        = m_encpakParams.SliceBetaOffsetDiv2;

                        nMBremain -= nMBrows;
                    }
                }

                if (MVPredictors)
                {
                    if (fieldId == 0){
                        feiEncMVPredictors = new mfxExtFeiEncMVPredictors[m_numOfFields];
                        MSDK_CHECK_POINTER(feiEncMVPredictors, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiEncMVPredictors, m_numOfFields);
                    }
                    numExtInParams++;

                    feiEncMVPredictors[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV_PRED;
                    feiEncMVPredictors[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncMVPredictors);
                    feiEncMVPredictors[fieldId].NumMBAlloc = m_numMB;
                    feiEncMVPredictors[fieldId].MB = new mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB[m_numMB];
                    MSDK_CHECK_POINTER(feiEncMVPredictors[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMVPredictors[fieldId].MB, m_numMB);

                    if (!m_encpakParams.bPREENC && (pMvPred == NULL) && (m_encpakParams.mvinFile != NULL)) //not load if we couple with PREENC
                    {
                        printf("Using MV input file: %s\n", m_encpakParams.mvinFile);
                        MSDK_FOPEN(pMvPred, m_encpakParams.mvinFile, MSDK_CHAR("rb"));
                        if (pMvPred == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_encpakParams.mvinFile);
                            exit(-1);
                        }
                        if (m_encpakParams.bRepackPreencMV){
                            m_tmpForMedian  = new mfxI16[16];
                            MSDK_CHECK_POINTER(m_tmpForMedian, MFX_ERR_MEMORY_ALLOC);
                            MSDK_ZERO_ARRAY(m_tmpForMedian, 16);
                            m_tmpForReading = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB[m_numMB];
                            MSDK_CHECK_POINTER(m_tmpForReading, MFX_ERR_MEMORY_ALLOC);
                            MSDK_ZERO_ARRAY(m_tmpForReading, m_numMB);
                        }
                    }
                    else {
                        m_tmpForMedian = new mfxI16[16];
                        MSDK_CHECK_POINTER(m_tmpForMedian, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(m_tmpForMedian, 16);
                    }
                }

                if (MBCtrl)
                {
                    if (fieldId == 0){
                        feiEncMBCtrl = new mfxExtFeiEncMBCtrl[m_numOfFields];
                        MSDK_CHECK_POINTER(feiEncMBCtrl, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiEncMBCtrl, m_numOfFields);
                    }
                    numExtInParams++;
                    numExtInParamsI++;

                    feiEncMBCtrl[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB;
                    feiEncMBCtrl[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncMBCtrl);
                    feiEncMBCtrl[fieldId].NumMBAlloc = m_numMB;
                    feiEncMBCtrl[fieldId].MB = new mfxExtFeiEncMBCtrl::mfxExtFeiEncMBCtrlMB[m_numMB];
                    MSDK_CHECK_POINTER(feiEncMBCtrl[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMBCtrl[fieldId].MB, m_numMB);

                    if ((pEncMBs == NULL) && (m_encpakParams.mbctrinFile != NULL))
                    {
                        printf("Using MB control input file: %s\n", m_encpakParams.mbctrinFile);
                        MSDK_FOPEN(pEncMBs, m_encpakParams.mbctrinFile, MSDK_CHAR("rb"));
                        if (pEncMBs == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_encpakParams.mbctrinFile);
                            exit(-1);
                        }
                    }
                }

                if (MBQP)
                {
                    if (fieldId == 0){
                        feiEncMbQp = new mfxExtFeiEncQP[m_numOfFields];
                        MSDK_CHECK_POINTER(feiEncMbQp, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiEncMbQp, m_numOfFields);
                    }
                    numExtInParams++;
                    numExtInParamsI++;

                    feiEncMbQp[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_QP;
                    feiEncMbQp[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncQP);
                    feiEncMbQp[fieldId].NumQPAlloc = m_numMB;
                    feiEncMbQp[fieldId].QP = new mfxU8[m_numMB];
                    MSDK_CHECK_POINTER(feiEncMbQp[fieldId].QP, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMbQp[fieldId].QP, m_numMB);

                    if ((pPerMbQP == NULL) && (m_encpakParams.mbQpFile != NULL))
                    {
                        printf("Use MB QP input file: %s\n", m_encpakParams.mbQpFile);
                        MSDK_FOPEN(pPerMbQP, m_encpakParams.mbQpFile, MSDK_CHAR("rb"));
                        if (pPerMbQP == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_encpakParams.mbQpFile);
                            exit(-1);
                        }
                    }
                }

                //Open output files if any
                //distortion buffer have to be always provided - current limitation
                if (MBStatOut)
                {
                    if (fieldId == 0){
                        feiEncMbStat = new mfxExtFeiEncMBStat[m_numOfFields];
                        MSDK_CHECK_POINTER(feiEncMbStat, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiEncMbStat, m_numOfFields);
                    }
                    numExtOutParams++;
                    numExtOutParamsI++;

                    feiEncMbStat[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MB_STAT;
                    feiEncMbStat[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncMBStat);
                    feiEncMbStat[fieldId].NumMBAlloc = m_numMB;
                    feiEncMbStat[fieldId].MB = new mfxExtFeiEncMBStat::mfxExtFeiEncMBStatMB[m_numMB];
                    MSDK_CHECK_POINTER(feiEncMbStat[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMbStat[fieldId].MB, m_numMB);

                    if ((mbstatout == NULL) && (m_encpakParams.mbstatoutFile != NULL))
                    {
                        printf("Use MB distortion output file: %s\n", m_encpakParams.mbstatoutFile);
                        MSDK_FOPEN(mbstatout, m_encpakParams.mbstatoutFile, MSDK_CHAR("wb"));
                        if (mbstatout == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_encpakParams.mbstatoutFile);
                            exit(-1);
                        }
                    }
                }

                if ((MVOut) || (m_encpakParams.bENCPAK))
                {
                    MVOut = true; /* ENC_PAK need MVOut and MBCodeOut buffer */
                    if (fieldId == 0){
                        feiEncMV = new mfxExtFeiEncMV[m_numOfFields];
                        MSDK_CHECK_POINTER(feiEncMV, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiEncMV, m_numOfFields);
                    }
                    numExtOutParams++;
                    numExtOutParamsI++;

                    feiEncMV[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_MV;
                    feiEncMV[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncMV);
                    feiEncMV[fieldId].NumMBAlloc = m_numMB;
                    feiEncMV[fieldId].MB = new mfxExtFeiEncMV::mfxExtFeiEncMVMB[m_numMB];
                    MSDK_CHECK_POINTER(feiEncMV[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMV[fieldId].MB, m_numMB);

                    if ((mvENCPAKout == NULL) && (NULL != m_encpakParams.mvoutFile))
                    {
                        printf("Use MV output file: %s\n", m_encpakParams.mvoutFile);
                        if (m_encpakParams.bOnlyPAK)
                            MSDK_FOPEN(mvENCPAKout, m_encpakParams.mvoutFile, MSDK_CHAR("rb"));
                        else { /*for all other cases need to write into this file*/
                            MSDK_FOPEN(mvENCPAKout, m_encpakParams.mvoutFile, MSDK_CHAR("wb"));
                            m_tmpMBenc = new mfxExtFeiEncMV::mfxExtFeiEncMVMB;
                            memset(m_tmpMBenc, 0x8000, sizeof(mfxExtFeiEncMV::mfxExtFeiEncMVMB));
                        }
                        if (mvENCPAKout == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_encpakParams.mvoutFile);
                            exit(-1);
                        }
                    }
                }

                //distortion buffer have to be always provided - current limitation
                if ((MBCodeOut) || (m_encpakParams.bENCPAK))
                {
                    MBCodeOut = true; /* ENC_PAK need MVOut and MBCodeOut buffer */
                    if (fieldId == 0){
                        feiEncMBCode = new mfxExtFeiPakMBCtrl[m_numOfFields];
                        MSDK_CHECK_POINTER(feiEncMBCode, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiEncMBCode, m_numOfFields);
                    }
                    numExtOutParams++;
                    numExtOutParamsI++;

                    feiEncMBCode[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PAK_CTRL;
                    feiEncMBCode[fieldId].Header.BufferSz = sizeof(mfxExtFeiPakMBCtrl);
                    feiEncMBCode[fieldId].NumMBAlloc = m_numMB;
                    feiEncMBCode[fieldId].MB = new mfxFeiPakMBCtrl[m_numMB];
                    MSDK_CHECK_POINTER(feiEncMBCode[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMBCode[fieldId].MB, m_numMB);

                    if ((mbcodeout == NULL) && (NULL != m_encpakParams.mbcodeoutFile))
                    {
                        printf("Use MB code output file: %s\n", m_encpakParams.mbcodeoutFile);
                        if (m_encpakParams.bOnlyPAK)
                            MSDK_FOPEN(mbcodeout, m_encpakParams.mbcodeoutFile, MSDK_CHAR("rb"));
                        else
                            MSDK_FOPEN(mbcodeout, m_encpakParams.mbcodeoutFile, MSDK_CHAR("wb"));

                        if (mbcodeout == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_encpakParams.mbcodeoutFile);
                            exit(-1);
                        }
                    }
                }

            } // for (fieldId = 0; fieldId < m_numOfFields; fieldId++)

            tmpForInit-> I_bufs.in.ExtParam  = new mfxExtBuffer*[numExtInParamsI];
            MSDK_CHECK_POINTER(tmpForInit->I_bufs.in.ExtParam, MFX_ERR_MEMORY_ALLOC);
            tmpForInit->PB_bufs.in.ExtParam  = new mfxExtBuffer*[numExtInParams];
            MSDK_CHECK_POINTER(tmpForInit->PB_bufs.in.ExtParam, MFX_ERR_MEMORY_ALLOC);
            tmpForInit-> I_bufs.out.ExtParam = new mfxExtBuffer*[numExtOutParamsI];
            MSDK_CHECK_POINTER(tmpForInit->I_bufs.out.ExtParam, MFX_ERR_MEMORY_ALLOC);
            tmpForInit->PB_bufs.out.ExtParam = new mfxExtBuffer*[numExtOutParams];
            MSDK_CHECK_POINTER(tmpForInit->PB_bufs.out.ExtParam, MFX_ERR_MEMORY_ALLOC);

            MSDK_ZERO_ARRAY(tmpForInit-> I_bufs.in.ExtParam, numExtInParamsI);
            MSDK_ZERO_ARRAY(tmpForInit->PB_bufs.in.ExtParam, numExtInParams);
            MSDK_ZERO_ARRAY(tmpForInit-> I_bufs.out.ExtParam, numExtOutParamsI);
            MSDK_ZERO_ARRAY(tmpForInit->PB_bufs.out.ExtParam, numExtOutParams);

            for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncCtrl[fieldId]);
                tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncCtrl[fieldId]);
            }

            if (feiSPS){
                tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)feiSPS;
                tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)feiSPS;
            }
            if (feiPPS){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiPPS[fieldId]);
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiPPS[fieldId]);
                }
            }
            if (feiSliceHeader){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiSliceHeader[fieldId]);
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiSliceHeader[fieldId]);
                }
            }
            if (MVPredictors){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    //tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)feiEncMVPredictors;
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncMVPredictors[fieldId]);
                }
            }
            if (MBCtrl){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncMBCtrl[fieldId]);
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncMBCtrl[fieldId]);
                }
            }
            if (MBQP){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncMbQp[fieldId]);
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiEncMbQp[fieldId]);
                }
            }
            if (MBStatOut){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.out.ExtParam[tmpForInit-> I_bufs.out.NumExtParam++] = (mfxExtBuffer*)(&feiEncMbStat[fieldId]);
                    tmpForInit->PB_bufs.out.ExtParam[tmpForInit->PB_bufs.out.NumExtParam++] = (mfxExtBuffer*)(&feiEncMbStat[fieldId]);
                }
            }
            if (MVOut){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.out.ExtParam[tmpForInit-> I_bufs.out.NumExtParam++] = (mfxExtBuffer*)(&feiEncMV[fieldId]);
                    tmpForInit->PB_bufs.out.ExtParam[tmpForInit->PB_bufs.out.NumExtParam++] = (mfxExtBuffer*)(&feiEncMV[fieldId]);
                }
            }
            if (MBCodeOut){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.out.ExtParam[tmpForInit-> I_bufs.out.NumExtParam++] = (mfxExtBuffer*)(&feiEncMBCode[fieldId]);
                    tmpForInit->PB_bufs.out.ExtParam[tmpForInit->PB_bufs.out.NumExtParam++] = (mfxExtBuffer*)(&feiEncMBCode[fieldId]);
                }
            }

            m_encodeBufs.push_back(tmpForInit);

        } // for (int k = 0; k < num_buffers; k++)

        if (is_8x8tranform_forced){
            msdk_printf(MSDK_STRING("\nWARNING: Transform8x8ModeFlag enforced!\n"));
            msdk_printf(MSDK_STRING("           Reason: IntraPartMask = %u, does not disable partitions below 16x16\n"), m_encpakParams.IntraPartMask);
        }

    } // if (m_encpakParams.bENCODE)

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::Run()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_encpakParams.bENCODE)
    {
        sts = UpdateVideoParams(); // update settings according to those that exposed by MSDK library
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    mfxFrameSurface1* pSurf = NULL; // dispatching pointer
    sTask *pCurrentTask     = NULL; // a pointer to the current task

    /* if TFF or BFF*/
    if ((m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF) ||
        (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF))
    {
        m_numOfFields = 2;
        m_isField = true;
    }

    //set Max Number of MB for ext Buffer
    bool chng_res = m_bNeedDRC || m_encpakParams.nDstWidth != m_encpakParams.nWidth || m_encpakParams.nDstHeight != m_encpakParams.nHeight;
    mfxU16 wdt = chng_res ? m_encpakParams.nDstWidth  : m_encpakParams.nWidth,  // if no VPP or DRC used
           hgt = chng_res ? m_encpakParams.nDstHeight : m_encpakParams.nHeight; // dstW/dstH is set the same as source W/H

    // For interlaced mode, may need an extra MB vertically
    // For example if the progressive mode has 45 MB vertically
    // The interlace should have 23 MB for each field

    m_widthMB  = MSDK_ALIGN16(wdt);
    m_heightMB = m_isField ? MSDK_ALIGN32(hgt) : MSDK_ALIGN16(hgt);
    m_numMB = (m_widthMB * m_heightMB) >> 8;
    m_numMB /= (mfxU16)m_numOfFields;

    m_widthMB  >>= 4;
    m_heightMB >>= m_isField ? 5 : 4;

    if (m_encpakParams.bPREENC && m_encpakParams.preencDSstrength)
    {
        // PreEnc is performed on lower resolution
        m_widthMBpreenc = MSDK_ALIGN16(m_mfxDSParams.vpp.Out.Width);
        mfxU16 heightMB = m_isField ? MSDK_ALIGN32(m_mfxDSParams.vpp.Out.Height) : MSDK_ALIGN16(m_mfxDSParams.vpp.Out.Height);
        m_numMBpreenc = (m_widthMBpreenc * heightMB) >> 8;
        m_numMBpreenc /= (mfxU16)m_numOfFields;

        m_widthMBpreenc >>= 4;
    }
    else
    {
        m_widthMBpreenc = m_widthMB;
        m_numMBpreenc = m_numMB;
    }

    m_ffid = m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF;
    m_sfid = m_isField - m_ffid;

    // init parameters
    sts = InitInterfaces();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    m_twoEncoders = m_encpakParams.bPREENC && (m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC || m_encpakParams.bENCODE);
    bool create_task = (m_encpakParams.bPREENC)  || (m_encpakParams.bENCPAK)  ||
                       (m_encpakParams.bOnlyENC) || (m_encpakParams.bOnlyPAK) ||
                       (m_encpakParams.bENCODE && m_encpakParams.EncodedOrder);

    iTask* eTask = NULL; // encoding task
    time_t start = time(0);
    size_t rctime = 0;

    sts = MFX_ERR_NONE;

    // main loop, preprocessing and encoding
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)
    {
        if ((m_encpakParams.nNumFrames && m_frameCount >= m_encpakParams.nNumFrames)
            || (m_encpakParams.nTimeout && (time(0) - start >= m_encpakParams.nTimeout)))
        {
            break;
        }

        sts = GetOneFrame(pSurf);
        if (sts == MFX_ERR_MORE_DATA && m_encpakParams.nTimeout) // new cycle in loop mode
        {
            sts = MFX_ERR_NONE;
            continue;
        }
        MSDK_BREAK_ON_ERROR(sts);

        if (m_bNeedDRC)
        {
           m_insertIDR  = false;
           sts = ResizeFrame(m_frameCount, m_insertIDR, rctime, eTask, pCurrentTask);
        }

        if (m_insertIDR)
        {
            m_insertIDR = false;
            m_frameType = PairU8((mfxU8)(MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF),
                                 (mfxU8)(MFX_FRAMETYPE_P                     | MFX_FRAMETYPE_REF));
        }
        else
        {
            m_frameType = GetFrameType(m_frameCount);
        }

        if (m_encpakParams.nPicStruct == MFX_PICSTRUCT_FIELD_BFF)
            std::swap(m_frameType[0], m_frameType[1]);

        if (m_frameType[m_ffid] & MFX_FRAMETYPE_IDR)
            m_frameOrderIdrInDisplayOrder = m_frameCount;

        // get a pointer to a free task (bit stream and sync point for encoder)
        sts = GetFreeTask(&pCurrentTask);
        MSDK_BREAK_ON_ERROR(sts);

        pSurf->Data.FrameOrder = m_frameCount; // in display order

        eTask = NULL;
        if (create_task)
        {
            eTask = CreateAndInitTask();
            MSDK_CHECK_POINTER(eTask, MFX_ERR_MEMORY_ALLOC);
        }

        m_frameCount++;

        if (m_pmfxVPP)
        {
            // pre-process a frame
            sts = PreProcessOneFrame(pSurf);
            MSDK_BREAK_ON_ERROR(sts);
        }

        if (m_encpakParams.bPREENC)
        {
            bool need_to_continue = false;
            sts = PreencOneFrame(eTask, pSurf, false, need_to_continue);
            MSDK_BREAK_ON_ERROR(sts);

            if (need_to_continue)
                continue;
        }

        if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) || (m_encpakParams.bOnlyENC))
        {
            bool need_to_continue = false;
            sts = EncPakOneFrame(eTask, pSurf, pCurrentTask, false, need_to_continue);
            MSDK_BREAK_ON_ERROR(sts);

            if (need_to_continue)
                continue;
        }

        if (m_encpakParams.bENCODE)
        {
            bool need_to_continue = false;
            sts = EncodeOneFrame(eTask, pSurf, pCurrentTask, false, need_to_continue);
            MSDK_BREAK_ON_ERROR(sts);

            if (need_to_continue)
                continue;
        }

    } // while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)

    // means that the input file has ended, need to go to buffering loops
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


    // loop to get buffered frames from encoder

    if (create_task)
    {
        mfxU32 numUnencoded = CountUnencodedFrames();
        while (numUnencoded != 0)
        {
            if (!(m_mfxEncParams.mfx.GopOptFlag & MFX_GOP_STRICT)){
                sts = ProcessLastB();
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }

            // get a pointer to a free task (bit stream and sync point for encoder)
            sts = GetFreeTask(&pCurrentTask);
            MSDK_BREAK_ON_ERROR(sts);

            if (m_encpakParams.bPREENC)
            {
                bool need_to_continue = false;
                sts = PreencOneFrame(eTask, pSurf, true, need_to_continue);
                MSDK_BREAK_ON_ERROR(sts);

                if (need_to_continue)
                    continue;
            }

            if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK) || (m_encpakParams.bOnlyENC))
            {
                bool need_to_continue = false;
                sts = EncPakOneFrame(eTask, pSurf, pCurrentTask, true, need_to_continue);
                MSDK_BREAK_ON_ERROR(sts);

                if (need_to_continue)
                    continue;
            }

            if (m_encpakParams.bENCODE)
            {
                bool need_to_continue = false;
                sts = EncodeOneFrame(eTask, pSurf, pCurrentTask, true, need_to_continue);
                MSDK_BREAK_ON_ERROR(sts);

                if (need_to_continue)
                    continue;
            }

            numUnencoded--;
        }
    }
    else // ENCODE in display order
    {
        if (m_encpakParams.bENCODE)
        {
            while (MFX_ERR_NONE <= sts)
            {
                // get a pointer to a free task (bit stream and sync point for encoder)
                sts = GetFreeTask(&pCurrentTask);
                MSDK_BREAK_ON_ERROR(sts);

                bool need_to_continue = false;
                sts = EncodeOneFrame(eTask, NULL, pCurrentTask, true, need_to_continue);

                if (need_to_continue)
                    continue;
            }

            // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
            // indicates that there are no more buffered frames
            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
            // exit in case of other errors
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            // synchronize all tasks that are left in task pool
            while (MFX_ERR_NONE == sts) {
                sts = m_TaskPool.SynchronizeFirstTask();
            }
        }
    }
    // MFX_ERR_NOT_FOUND is the correct status to exit the loop with
    // EncodeFrameAsync and SyncOperation don't return this status
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_FOUND);
    // report any errors that occurred in asynchronous part
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // deallocation
    sts = ReleaseResources();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

/* read YUV frame */

mfxStatus CEncodingPipeline::GetOneFrame(mfxFrameSurface1* & pSurf)
{
    mfxStatus sts = MFX_ERR_NONE;
    ExtendedSurface DecExtSurface = { 0 };

    if (!m_pmfxDECODE) // load frame from YUV file
    {
        // find free surface for encoder input
        mfxFrameSurface1 *pSurfPool = m_pEncSurfaces;
        mfxU16 poolSize = m_EncResponse.NumFrameActual;
        if (m_pmfxVPP)
        {
            pSurfPool = m_pVppSurfaces;
            poolSize = m_VppResponse.NumFrameActual;
        }
        mfxU16 nEncSurfIdx = GetFreeSurface(pSurfPool, poolSize);
        MSDK_CHECK_ERROR(nEncSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
        // point pSurf to encoder surface
        pSurf = &pSurfPool[nEncSurfIdx];

        // load frame from file to surface data
        // if we share allocator with Media SDK we need to call Lock to access surface data and...
        if (m_bExternalAlloc)
        {
            // get YUV pointers
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        sts = m_FileReader.LoadNextFrame(pSurf);
        if (sts == MFX_ERR_MORE_DATA && m_encpakParams.nTimeout)
        {
            // infinite loop mode, need to proceed from the beginning
            sts = ResetIOFiles(m_encpakParams);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            m_insertIDR = true;
            return MFX_ERR_MORE_DATA;
        }
        MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, sts);

        // ... after we're done call Unlock
        if (m_bExternalAlloc)
        {
            sts = m_pMFXAllocator->Unlock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }
    else // decode frame
    {
        if (!m_bEndOfFile)
        {
            sts = DecodeOneFrame(&DecExtSurface);
            if (MFX_ERR_MORE_DATA == sts)
            {
                sts = DecodeLastFrame(&DecExtSurface);
                m_bEndOfFile = true;
            }
        }
        else
        {
            sts = DecodeLastFrame(&DecExtSurface);
        }

        if (sts == MFX_ERR_MORE_DATA)
        {
            if (m_encpakParams.nTimeout)
            {
                // infinite loop mode, need to proceed from the beginning
                sts = ResetIOFiles(m_encpakParams);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
                m_insertIDR  = true;
                m_bEndOfFile = false;
                return MFX_ERR_MORE_DATA;
            }
            DecExtSurface.pSurface = NULL;
            return MFX_ERR_NONE;
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        for (;;)
        {
            sts = m_decode_mfxSession.SyncOperation(DecExtSurface.Syncp, MSDK_WAIT_INTERVAL);

            if (MFX_ERR_NONE < sts && !DecExtSurface.Syncp) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts){
                    //MSDK_SLEEP(1); // wait if device is busy
                    WaitForDeviceToBecomeFree(m_decode_mfxSession, DecExtSurface.Syncp, sts);
                }
            }
            else if (MFX_ERR_NONE <= sts && DecExtSurface.Syncp) {
                sts = MFX_ERR_NONE; // ignore warnings if output is available
                break;
            }
            else
            {
                MSDK_BREAK_ON_ERROR(sts);
            }
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        pSurf = DecExtSurface.pSurface;
    }

    return sts;
}

/* reordering functions */

iTask* CEncodingPipeline::FindFrameToEncode(bool buffered_frames)
{
    std::list<iTask*> unencoded_queue;
    for (std::list<iTask*>::iterator it = m_inputTasks.begin(); it != m_inputTasks.end(); ++it){
        if ((*it)->encoded == 0){
            unencoded_queue.push_back(*it);
        }
    }

    std::list<iTask*>::iterator top = ReorderFrame(unencoded_queue), begin = unencoded_queue.begin(), end = unencoded_queue.end();

    bool flush = unencoded_queue.size() < m_mfxEncParams.mfx.GopRefDist && buffered_frames;

    if (flush && top == end && begin != end)
    {
        if (!!(m_mfxEncParams.mfx.GopOptFlag & MFX_GOP_STRICT))
        {
            top = begin; // TODO: reorder remaining B frames for B-pyramid when enabled
        }
        else
        {
            top = end;
            --top;
            //assert((*top)->frameType & MFX_FRAMETYPE_B);
            (*top)->m_type[0] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            (*top)->m_type[1] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            top = ReorderFrame(unencoded_queue);
            //assert(top != end || begin == end);
        }
    }

    if (top == end)
        return NULL; //skip B without refs

    return *top;
}

std::list<iTask*>::iterator CEncodingPipeline::ReorderFrame(std::list<iTask*>& unencoded_queue){
    std::list<iTask*>::iterator top = unencoded_queue.begin(), end = unencoded_queue.end();
    while (top != end &&                                     // go through buffered frames in display order and
        (((ExtractFrameType(**top) & MFX_FRAMETYPE_B) &&     // get earliest non-B frame
        CountFutureRefs((*top)->m_frameOrder) == 0) ||  // or B frame with L1 reference
        (*top)->encoded == 1))                               // which is not encoded yet
    {
        ++top;
    }

    if (top != end && (ExtractFrameType(**top) & MFX_FRAMETYPE_B))
    {
        // special case for B frames (when B pyramid is enabled)
        std::list<iTask*>::iterator i = top;
        while (++i != end &&                                          // check remaining
            (ExtractFrameType(**i) & MFX_FRAMETYPE_B) &&              // B frames
            ((*i)->m_loc.miniGopCount == (*top)->m_loc.miniGopCount)) // from the same mini-gop
        {
            if ((*i)->encoded == 0 && (*top)->m_loc.encodingOrder > (*i)->m_loc.encodingOrder)
                top = i;
        }
    }

    return top;
}

mfxU32 CEncodingPipeline::CountFutureRefs(mfxU32 frameOrder){
    mfxU32 count = 0;
    for (std::list<iTask*>::iterator it = m_inputTasks.begin(); it != m_inputTasks.end(); ++it)
        if (frameOrder < (*it)->m_frameOrder && (*it)->encoded == 1)
            count++;
    return count;
}

iTask* CEncodingPipeline::GetTaskByFrameOrder(mfxU32 frame_order)
{
    for (std::list<iTask*>::iterator it = m_inputTasks.begin(); it != m_inputTasks.end(); ++it)
    {
        if ((*it)->m_frameOrder == frame_order)
            return *it;
    }

    return NULL;
}

/* task configuration */

iTask* CEncodingPipeline::CreateAndInitTask()
{
    iTask* eTask = new iTask;
    MSDK_CHECK_POINTER(eTask, NULL);
    *eTask = {};

    eTask->m_fieldPicFlag = m_isField;
    eTask->PicStruct = m_mfxEncParams.mfx.FrameInfo.PicStruct;
    eTask->BRefType  = m_CodingOption2.BRefType; // m_encpakParams.bRefType;
    eTask->m_fid[0]  = m_ffid;
    eTask->m_fid[1]  = m_sfid;

    MSDK_ZERO_MEMORY(eTask->in);
    MSDK_ZERO_MEMORY(eTask->out);
    MSDK_ZERO_MEMORY(eTask->inPAK);
    MSDK_ZERO_MEMORY(eTask->outPAK);

    eTask->m_type         = m_frameType;
    eTask->m_frameOrder   = m_frameCount;
    eTask->encoded        = 0;
    eTask->bufs           = NULL;
    eTask->preenc_bufs    = NULL;
    eTask->fullResSurface = NULL;

    if (ExtractFrameType(*eTask) & MFX_FRAMETYPE_B)
    {
        eTask->m_loc = GetBiFrameLocation(m_frameCount - m_frameOrderIdrInDisplayOrder);
        eTask->m_type[0] |= eTask->m_loc.refFrameFlag;
        eTask->m_type[1] |= eTask->m_loc.refFrameFlag;
    }

    eTask->m_reference[m_ffid] = !!(eTask->m_type[m_ffid] & MFX_FRAMETYPE_REF);
    eTask->m_reference[m_sfid] = !!(eTask->m_type[m_sfid] & MFX_FRAMETYPE_REF);

    eTask->m_poc[0] = GetPoc(*eTask, TFIELD);
    eTask->m_poc[1] = GetPoc(*eTask, BFIELD);

    eTask->m_nalRefIdc[m_ffid] = eTask->m_reference[m_ffid];
    eTask->m_nalRefIdc[m_sfid] = eTask->m_reference[m_sfid];

    eTask->NumRefActiveP   = m_CodingOption3.NumRefActiveP;
    eTask->NumRefActiveBL0 = m_CodingOption3.NumRefActiveBL0;
    eTask->NumRefActiveBL1 = m_CodingOption3.NumRefActiveBL1;

    return eTask;
}

iTask* CEncodingPipeline::ConfigureTask(iTask* task, bool is_buffered)
{
    if (!is_buffered)
        m_inputTasks.push_back(task); //inputTasks in display order

    iTask* eTask = FindFrameToEncode(is_buffered);
    MSDK_CHECK_POINTER(eTask, NULL); //not found frame to encode

    //...........................reflist control........................................
    eTask->prevTask = m_last_task;

    eTask->m_frameOrderIdr = (ExtractFrameType(*eTask) & MFX_FRAMETYPE_IDR) ? eTask->m_frameOrder : (eTask->prevTask ? eTask->prevTask->m_frameOrderIdr : 0);
    eTask->m_frameOrderI   = (ExtractFrameType(*eTask) & MFX_FRAMETYPE_I)   ? eTask->m_frameOrder : (eTask->prevTask ? eTask->prevTask->m_frameOrderI   : 0);
    mfxU8  frameNumIncrement = (eTask->prevTask && (ExtractFrameType(*(eTask->prevTask)) & MFX_FRAMETYPE_REF || eTask->prevTask->m_nalRefIdc[0])) ? 1 : 0;
    eTask->m_frameNum = (eTask->prevTask && !(ExtractFrameType(*eTask) & MFX_FRAMETYPE_IDR)) ? mfxU16((eTask->prevTask->m_frameNum + frameNumIncrement) % (1 << m_log2frameNumMax)) : 0;

    eTask->m_picNum[0] = eTask->m_frameNum * (eTask->m_fieldPicFlag + 1) + eTask->m_fieldPicFlag;
    eTask->m_picNum[1] = eTask->m_picNum[0];

    eTask->m_dpb[eTask->m_fid[0]] = eTask->prevTask ? eTask->prevTask->m_dpbPostEncoding : ArrayDpbFrame();
    UpdateDpbFrames(*eTask, eTask->m_fid[0], 1 << m_log2frameNumMax);
    InitRefPicList(*eTask, eTask->m_fid[0]);
    ModifyRefPicLists(m_mfxEncParams, *eTask, m_ffid);
    MarkDecodedRefPictures(m_mfxEncParams, *eTask, eTask->m_fid[0]);
    if (eTask->m_fieldPicFlag)
    {
        UpdateDpbFrames(*eTask, eTask->m_fid[1], 1 << m_log2frameNumMax);
        InitRefPicList(*eTask, eTask->m_fid[1]);
        ModifyRefPicLists(m_mfxEncParams, *eTask, m_sfid);

        // mark second field of last added frame short-term ref
        eTask->m_dpbPostEncoding = eTask->m_dpb[eTask->m_fid[1]];
        if (eTask->m_reference[eTask->m_fid[1]])
            eTask->m_dpbPostEncoding.Back().m_refPicFlag[eTask->m_fid[1]] = 1;
    }

    ShowDpbInfo(eTask, eTask->m_frameOrder);
    return eTask;
}

mfxStatus CEncodingPipeline::UpdateTaskQueue(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;

    eTask->encoded = 1;
    if (!m_last_task)
        m_last_task = new iTask;

    sts = CopyState(eTask);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (/*m_maxQueueLength*//*this->m_refDist * 2*/ m_mfxEncParams.mfx.NumRefFrame + 1 <= (mfxU16)m_inputTasks.size()) {
        sts = RemoveOneTask();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    return sts;
}

mfxStatus CEncodingPipeline::CopyState(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask,       MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_last_task, MFX_ERR_NULL_PTR);

    m_last_task->m_frameOrderIdr   = eTask->m_frameOrderIdr;
    m_last_task->m_frameOrderI     = eTask->m_frameOrderI;
    m_last_task->m_nalRefIdc       = eTask->m_nalRefIdc;
    m_last_task->m_frameNum        = eTask->m_frameNum;
    m_last_task->m_type            = eTask->m_type;
    m_last_task->m_dpbPostEncoding = eTask->m_dpbPostEncoding;
    m_last_task->m_poc             = eTask->m_poc;
    m_last_task->PicStruct         = eTask->PicStruct;

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::ReleasePreencMVPinfo(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;

    for (std::list<PreEncMVPInfo>::iterator it = eTask->preenc_mvp_info.begin(); it != eTask->preenc_mvp_info.end(); ++it)
    {
        SAFE_RELEASE_EXT_BUFSET((*it).preenc_output_bufs);
    }

    eTask->preenc_mvp_info.clear();

    return sts;
}

mfxStatus CEncodingPipeline::RemoveOneTask()
{
    mfxStatus sts = MFX_ERR_NONE;
    ArrayDpbFrame & dpb = m_last_task->m_dpbPostEncoding;
    std::list<mfxU32> FramesInDPB;


    for (mfxU32 i = 0; i < dpb.Size(); i++)
        FramesInDPB.push_back(dpb[i].m_frameOrder);

    for (std::list<iTask*>::iterator it = m_inputTasks.begin(); it != m_inputTasks.end(); ++it)
    {
        if (std::find(FramesInDPB.begin(), FramesInDPB.end(), (*it)->m_frameOrder) == FramesInDPB.end() && (*it)->encoded == 1) // delete task
        {
            MSDK_CHECK_POINTER(*it, MFX_ERR_NULL_PTR);

            SAFE_RELEASE_EXT_BUFSET((*it)->bufs);
            SAFE_RELEASE_EXT_BUFSET((*it)->preenc_bufs);

            sts = ReleasePreencMVPinfo(*it);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            if (!(m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC || m_encpakParams.bOnlyPAK)) //actually we need to do it only for preenc
            {
                MSDK_SAFE_DELETE_ARRAY((*it)->in.L0Surface);
                MSDK_SAFE_DELETE_ARRAY((*it)->in.L1Surface);
                MSDK_SAFE_DELETE_ARRAY((*it)->inPAK.L0Surface);
                MSDK_SAFE_DELETE_ARRAY((*it)->inPAK.L1Surface);
            }

            SAFE_DEC_LOCKER((*it)->in.InSurface);

            /* In case of preenc + enc (+ pak) pipeline we need to do decrement manually for both interfaces*/
            if (m_twoEncoders && (m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC || m_encpakParams.bOnlyPAK))
                SAFE_DEC_LOCKER((*it)->in.InSurface);

            if (!!m_encpakParams.preencDSstrength)
                SAFE_DEC_LOCKER((*it)->fullResSurface);

            SAFE_DEC_LOCKER((*it)->outPAK.OutSurface);

            delete (*it);
            m_inputTasks.erase(it);
            return sts;
        }
    }

    return sts;
}

mfxStatus CEncodingPipeline::ClearTasks()
{
    mfxStatus sts = MFX_ERR_NONE;

    for (std::list<iTask*>::iterator it = m_inputTasks.begin(); it != m_inputTasks.end(); ++it)
    {
        MSDK_CHECK_POINTER(*it, MFX_ERR_NULL_PTR);

        SAFE_RELEASE_EXT_BUFSET((*it)->bufs);
        SAFE_RELEASE_EXT_BUFSET((*it)->preenc_bufs);

        sts = ReleasePreencMVPinfo(*it);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        if (!(m_pmfxPAK || m_pmfxENC))
        {
            MSDK_SAFE_DELETE_ARRAY((*it)->in.L0Surface);
            MSDK_SAFE_DELETE_ARRAY((*it)->in.L1Surface);
            MSDK_SAFE_DELETE_ARRAY((*it)->inPAK.L0Surface);
            MSDK_SAFE_DELETE_ARRAY((*it)->inPAK.L1Surface);
        }

        SAFE_UNLOCK((*it)->in.InSurface);
        SAFE_UNLOCK((*it)->outPAK.OutSurface);
        if (!!m_encpakParams.preencDSstrength)
            SAFE_UNLOCK((*it)->fullResSurface);

        delete (*it);
    }

    m_inputTasks.clear();

    return sts;
}

mfxStatus CEncodingPipeline::ResetBuffers()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_encpakParams.bENCODE)
    {
        // synchronize all tasks that are left in task pool
        while (MFX_ERR_NONE == sts) {
            sts = m_TaskPool.SynchronizeFirstTask();
        }
        sts = MFX_ERR_NONE;
    }

    sts = ClearTasks();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

mfxStatus CEncodingPipeline::FillRefInfo(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;

    sts = ResetRefInfo();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    iTask* ref_task = NULL;
    mfxFrameSurface1* ref_surface = NULL;
    std::vector<mfxFrameSurface1*>::iterator rslt;
    mfxU32 k = 0, fid, n_l0, n_l1;

    for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
    {
        fid = eTask->m_fid[fieldId];
        for (DpbFrame* instance = eTask->m_dpb[fid].Begin(); instance != eTask->m_dpb[fid].End(); instance++)
        {
            ref_task = GetTaskByFrameOrder(instance->m_frameOrder);
            MSDK_CHECK_POINTER(ref_task, MFX_ERR_NULL_PTR);

            ref_surface = ref_task->out.OutSurface; // this is shared output surface for ENC/PAK
            MSDK_CHECK_POINTER(ref_surface, MFX_ERR_NULL_PTR);

            rslt = std::find(m_ref_info.reference_frames.begin(), m_ref_info.reference_frames.end(), ref_surface);

            if (rslt == m_ref_info.reference_frames.end()){
                m_ref_info.state[fieldId].dpb_idx.push_back((mfxU16)m_ref_info.reference_frames.size());
                m_ref_info.reference_frames.push_back(ref_surface);
            }
            else{
                m_ref_info.state[fieldId].dpb_idx.push_back(static_cast<mfxU16>(std::distance(m_ref_info.reference_frames.begin(), rslt)));
            }
        }

        /* in some cases l0 and l1 lists are equal, if so same ref lists for l0 and l1 should be used*/
        n_l0 = GetNBackward(eTask, fieldId);
        n_l1 = GetNForward(eTask, fieldId);

        if (!n_l0 && eTask->m_list0[fid].Size() && !(eTask->m_type[fid] & MFX_FRAMETYPE_I))
        {
            n_l0 = eTask->m_list0[fid].Size();
        }

        if (!n_l1 && eTask->m_list1[fid].Size() && (eTask->m_type[fid] & MFX_FRAMETYPE_B))
        {
            n_l1 = eTask->m_list1[fid].Size();
        }

        k = 0;
        for (mfxU8 const * instance = eTask->m_list0[fid].Begin(); k < n_l0 && instance != eTask->m_list0[fid].End(); instance++)
        {
            ref_task = GetTaskByFrameOrder(eTask->m_dpb[fid][*instance & 127].m_frameOrder);
            MSDK_CHECK_POINTER(ref_task, MFX_ERR_NULL_PTR);

            ref_surface = ref_task->out.OutSurface; // this is shared output surface for ENC/PAK
            MSDK_CHECK_POINTER(ref_surface, MFX_ERR_NULL_PTR);

            rslt = std::find(m_ref_info.reference_frames.begin(), m_ref_info.reference_frames.end(), ref_surface);

            if (rslt == m_ref_info.reference_frames.end()){
                return MFX_ERR_MORE_SURFACE; // surface from L0 list not in DPB (should never happen)
            }
            else{
                m_ref_info.state[fieldId].l0_idx.push_back(static_cast<mfxU16>(std::distance(m_ref_info.reference_frames.begin(), rslt)));
            }

            //m_ref_info.state[fieldId].l0_idx.push_back(*instance & 127);
            m_ref_info.state[fieldId].l0_parity.push_back((*instance)>>7);
            k++;
        }

        k = 0;
        for (mfxU8 const * instance = eTask->m_list1[fid].Begin(); k < n_l1 && instance != eTask->m_list1[fid].End(); instance++)
        {
            ref_task = GetTaskByFrameOrder(eTask->m_dpb[fid][*instance & 127].m_frameOrder);
            MSDK_CHECK_POINTER(ref_task, MFX_ERR_NULL_PTR);

            ref_surface = ref_task->out.OutSurface; // this is shared output surface for ENC/PAK
            MSDK_CHECK_POINTER(ref_surface, MFX_ERR_NULL_PTR);

            rslt = std::find(m_ref_info.reference_frames.begin(), m_ref_info.reference_frames.end(), ref_surface);

            if (rslt == m_ref_info.reference_frames.end()){
                return MFX_ERR_MORE_SURFACE; // surface from L0 list not in DPB (should never happen)
            }
            else{
                m_ref_info.state[fieldId].l1_idx.push_back(static_cast<mfxU16>(std::distance(m_ref_info.reference_frames.begin(), rslt)));
            }

            //m_ref_info.state[fieldId].l1_idx.push_back(*instance & 127);
            m_ref_info.state[fieldId].l1_parity.push_back((*instance) >> 7);
            k++;
        }
    }

    return sts;
}

mfxStatus CEncodingPipeline::ResetRefInfo()
{
    mfxStatus sts = MFX_ERR_NONE;

    m_ref_info.reference_frames.clear();

    for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
    {
        m_ref_info.state[fieldId].dpb_idx.clear();
        m_ref_info.state[fieldId].l0_idx.clear();
        m_ref_info.state[fieldId].l1_idx.clear();
        m_ref_info.state[fieldId].l0_parity.clear();
        m_ref_info.state[fieldId].l1_parity.clear();
    }

    return sts;
}

mfxU32 CEncodingPipeline::GetNBackward(iTask* eTask, mfxU32 fieldId)
{
    MSDK_CHECK_POINTER(eTask, 0);

    mfxU32 fid = eTask->m_fid[fieldId];

    if (eTask->m_list0[fid].Size() == 0)
        return 0;

    if (eTask->m_list1[fid].Size() == 0 ||
        std::find(eTask->m_list0[fid].Begin(), eTask->m_list0[fid].End(), *eTask->m_list1[fid].Begin())
        == eTask->m_list0[fid].End())
    {
        // No forward ref in L0
        return eTask->m_list0[fid].Size();
    }
    else
    {
        return static_cast<mfxU32>(std::distance(eTask->m_list0[fid].Begin(),
            std::find(eTask->m_list0[fid].Begin(), eTask->m_list0[fid].End(), *eTask->m_list1[fid].Begin())));
    }
}

mfxU32 CEncodingPipeline::GetNForward(iTask* eTask, mfxU32 fieldId)
{
    MSDK_CHECK_POINTER(eTask, 0);

    mfxU32 fid = eTask->m_fid[fieldId];

    if (eTask->m_list1[fid].Size() == 0)
        return 0;

    if (std::find(eTask->m_list1[fid].Begin(), eTask->m_list1[fid].End(), *eTask->m_list0[fid].Begin())
        == eTask->m_list1[fid].End())
    {
        // No backward ref in L1
        return eTask->m_list1[fid].Size();
    }
    else
    {
        return static_cast<mfxU32>(std::distance(eTask->m_list1[fid].Begin(),
            std::find(eTask->m_list1[fid].Begin(), eTask->m_list1[fid].End(), *eTask->m_list0[fid].Begin())));
    }
}

mfxU32 CEncodingPipeline::CountUnencodedFrames()
{
    mfxU32 numUnencoded = 0;
    for (std::list<iTask*>::iterator it = m_inputTasks.begin(); it != m_inputTasks.end(); ++it){
        if ((*it)->encoded == 0){
            numUnencoded++;
        }
    }

    return numUnencoded;
}

mfxStatus CEncodingPipeline::ProcessLastB()
{
    if (m_inputTasks.back()->m_type[m_ffid] & MFX_FRAMETYPE_B) {
        m_inputTasks.back()->m_type[m_ffid] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
    }
    if (m_inputTasks.back()->m_type[m_sfid] & MFX_FRAMETYPE_B) {
        m_inputTasks.back()->m_type[m_sfid] = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
    }

    return MFX_ERR_NONE;
}

inline mfxU16 CEncodingPipeline::GetCurPicType(mfxU32 fieldId)
{
    if (!m_isField)
        return MFX_PICTYPE_FRAME;
    else
    {
        if (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF)
            return (mfxU16)(fieldId ? MFX_PICTYPE_BOTTOMFIELD : MFX_PICTYPE_TOPFIELD);
        else
            return (mfxU16)(fieldId ? MFX_PICTYPE_TOPFIELD : MFX_PICTYPE_BOTTOMFIELD);
    }
}

/* initialization functions */

mfxStatus CEncodingPipeline::InitPreEncFrameParamsEx(iTask* eTask, iTask* refTask[2][2], int ref_fid[2][2], bool isDownsamplingNeeded)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxExtFeiPreEncMVPredictors* pMvPredBuf = NULL;
    mfxExtFeiEncQP*              pMbQP      = NULL;
    mfxExtFeiPreEncCtrl*         preENCCtr  = NULL;

    mfxFrameSurface1* refSurf0[2] = { NULL, NULL }; // L0 ref surfaces
    mfxFrameSurface1* refSurf1[2] = { NULL, NULL }; // L1 ref surfaces

    eTask->preenc_bufs = getFreeBufSet(m_preencBufs);
    MSDK_CHECK_POINTER(eTask->preenc_bufs, MFX_ERR_NULL_PTR);

    for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
    {
        switch (ExtractFrameType(*eTask, fieldId) & MFX_FRAMETYPE_IPB) {
        case MFX_FRAMETYPE_I:
            eTask->in.NumFrameL0 = 0;
            eTask->in.NumFrameL1 = 0;

            //in data
            eTask->in.NumExtParam = eTask->preenc_bufs->I_bufs.in.NumExtParam;
            eTask->in.ExtParam    = eTask->preenc_bufs->I_bufs.in.ExtParam;
            //out data
            //exclude MV output from output buffers
            eTask->out.NumExtParam = eTask->preenc_bufs->I_bufs.out.NumExtParam;
            eTask->out.ExtParam    = eTask->preenc_bufs->I_bufs.out.ExtParam;
            break;

        case MFX_FRAMETYPE_P:
            refSurf0[fieldId] = refTask[fieldId][0] ? refTask[fieldId][0]->in.InSurface : NULL;

            eTask->in.NumFrameL0 = !!refSurf0[fieldId];
            eTask->in.NumFrameL1 = 0;

            //in data
            eTask->in.NumExtParam = eTask->preenc_bufs->PB_bufs.in.NumExtParam;
            eTask->in.ExtParam    = eTask->preenc_bufs->PB_bufs.in.ExtParam;
            //out data
            eTask->out.NumExtParam = eTask->preenc_bufs->PB_bufs.out.NumExtParam;
            eTask->out.ExtParam    = eTask->preenc_bufs->PB_bufs.out.ExtParam;
            break;

        case MFX_FRAMETYPE_B:
            refSurf0[fieldId] = refTask[fieldId][0] ? refTask[fieldId][0]->in.InSurface : NULL;
            refSurf1[fieldId] = refTask[fieldId][1] ? refTask[fieldId][1]->in.InSurface : NULL;

            eTask->in.NumFrameL0 = !!refTask[fieldId][0];
            eTask->in.NumFrameL1 = !!refTask[fieldId][1];

            //in data
            eTask->in.NumExtParam = eTask->preenc_bufs->PB_bufs.in.NumExtParam;
            eTask->in.ExtParam    = eTask->preenc_bufs->PB_bufs.in.ExtParam;
            //out data
            eTask->out.NumExtParam = eTask->preenc_bufs->PB_bufs.out.NumExtParam;
            eTask->out.ExtParam    = eTask->preenc_bufs->PB_bufs.out.ExtParam;
            break;

        default:
            break;
        }
    }

    mfxU32 preENCCtrId = 0, pMvPredId = 0;
    mfxU8 type = MFX_FRAMETYPE_UNKNOWN;
    for (int i = 0; i < eTask->in.NumExtParam; i++)
    {
        switch (eTask->in.ExtParam[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_PREENC_CTRL:
            type = ExtractFrameType(*eTask, preENCCtrId); // IP pair is supported
            preENCCtr = (mfxExtFeiPreEncCtrl*)(eTask->in.ExtParam[i]);
            preENCCtr->DisableMVOutput = (type & MFX_FRAMETYPE_I) ? 1 : m_disableMVoutPreENC;

            preENCCtr->RefPictureType[0] = (mfxU16)(!m_isField ? MFX_PICTYPE_FRAME :
                ref_fid[preENCCtrId][0] == 0 ? MFX_PICTYPE_TOPFIELD : MFX_PICTYPE_BOTTOMFIELD);

            preENCCtr->RefPictureType[1] = (mfxU16)(!m_isField ? MFX_PICTYPE_FRAME :
                ref_fid[preENCCtrId][1] == 0 ? MFX_PICTYPE_TOPFIELD : MFX_PICTYPE_BOTTOMFIELD);

            preENCCtr->RefFrame[0] = refSurf0[preENCCtrId];
            preENCCtr->RefFrame[1] = refSurf1[preENCCtrId];

            if (!isDownsamplingNeeded && m_encpakParams.bPerfMode)
                preENCCtr->DownsampleInput = MFX_CODINGOPTION_OFF;
            else
                preENCCtr->DownsampleInput = MFX_CODINGOPTION_ON; // the default is ON too

            preENCCtrId++;
            break;

        case MFX_EXTBUFF_FEI_PREENC_MV_PRED:
            if (pMvPred)
            {
                if (!(ExtractFrameType(*eTask, pMvPredId) & MFX_FRAMETYPE_I)) // IP pair is supported
                {
                    pMvPredBuf = (mfxExtFeiPreEncMVPredictors*)(eTask->in.ExtParam[i]);
                    SAFE_FREAD(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0])*pMvPredBuf->NumMBAlloc, 1, pMvPred, MFX_ERR_MORE_DATA);
                }
                else{
                    SAFE_FSEEK(pMvPred, sizeof(mfxExtFeiPreEncMVPredictors::mfxExtFeiPreEncMVPredictorsMB)*m_numMBpreenc, SEEK_CUR, MFX_ERR_MORE_DATA);
                }
                pMvPredId++;
            }
            break;

        case MFX_EXTBUFF_FEI_PREENC_QP:
            if (pPerMbQP)
            {
                pMbQP = (mfxExtFeiEncQP*)(eTask->in.ExtParam[i]);
                SAFE_FREAD(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP, MFX_ERR_MORE_DATA);
            }
            break;
        }
    } // for (int i = 0; i<eTask->in.NumExtParam; i++)

    mdprintf(stderr, "enc: %d t: %d\n", eTask->m_frameOrder, (eTask->m_type[0]& MFX_FRAMETYPE_IPB));
    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::InitEncPakFrameParams(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    mfxExtFeiEncMVPredictors* pMvPredBuf       = NULL;
    mfxExtFeiEncMBCtrl*       pMbEncCtrl       = NULL;
    mfxExtFeiEncQP*           pMbQP            = NULL;
    mfxExtFeiSPS*             feiSPS           = NULL;
    mfxExtFeiPPS*             feiPPS           = NULL;
    mfxExtFeiSliceHeader*     feiSliceHeader   = NULL;
    mfxExtFeiEncFrameCtrl*    feiEncCtrl       = NULL;

    if (m_encpakParams.bPREENC)
    {
        MSDK_SAFE_DELETE_ARRAY(eTask->in.L0Surface);
        MSDK_SAFE_DELETE_ARRAY(eTask->in.L1Surface);
        MSDK_SAFE_DELETE_ARRAY(eTask->inPAK.L0Surface);
        MSDK_SAFE_DELETE_ARRAY(eTask->inPAK.L1Surface);
    }

    eTask->bufs = getFreeBufSet(m_encodeBufs);
    MSDK_CHECK_POINTER(eTask->bufs, MFX_ERR_NULL_PTR);

    int pMvPredId = 0;
    for (int i = 0; i < eTask->bufs->PB_bufs.in.NumExtParam; i++)
    {
        switch (eTask->bufs->PB_bufs.in.ExtParam[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_PPS:
            if (!feiPPS){
                feiPPS = (mfxExtFeiPPS*)(eTask->bufs->PB_bufs.in.ExtParam[i]);
            }
            break;

        case MFX_EXTBUFF_FEI_SPS:
            feiSPS = (mfxExtFeiSPS*)(eTask->bufs->PB_bufs.in.ExtParam[i]);
            break;

        case MFX_EXTBUFF_FEI_SLICE:
            if (!feiSliceHeader){
                feiSliceHeader = (mfxExtFeiSliceHeader*)(eTask->bufs->PB_bufs.in.ExtParam[i]);
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_MV_PRED:
            if (!m_encpakParams.bPREENC && pMvPred)
            {
                if (!(ExtractFrameType(*eTask, pMvPredId) & MFX_FRAMETYPE_I))
                {
                    pMvPredBuf = (mfxExtFeiEncMVPredictors*)(eTask->bufs->PB_bufs.in.ExtParam[i]);
                    if (m_encpakParams.bRepackPreencMV)
                    {
                        SAFE_FREAD(m_tmpForReading, sizeof(*m_tmpForReading)*m_numMB, 1, pMvPred, MFX_ERR_MORE_DATA);
                        repackPreenc2Enc(m_tmpForReading, pMvPredBuf->MB, m_numMB, m_tmpForMedian);
                    }
                    else {
                        SAFE_FREAD(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0]) *pMvPredBuf->NumMBAlloc, 1, pMvPred, MFX_ERR_MORE_DATA);
                    }
                }
                else{
                    int shft = m_encpakParams.bRepackPreencMV ? sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB) : sizeof(mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB);
                    SAFE_FSEEK(pMvPred, shft*m_numMB, SEEK_CUR, MFX_ERR_MORE_DATA);
                }
                pMvPredId++;
            }
            break;
        } // switch (eTask->bufs->PB_bufs.in.ExtParam[i]->BufferId)
    } // for (int i = 0; i < eTask->bufs->PB_bufs.in.NumExtParam; i++)

    sts = FillRefInfo(eTask); // get info to fill reference structures
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    switch (ExtractFrameType(*eTask, m_isField /*second field*/) & MFX_FRAMETYPE_IPB) {
    case MFX_FRAMETYPE_I:
        /* ENC data */
        if (m_pmfxENC)
        {
            eTask->in.NumFrameL0 = eTask->in.NumFrameL1 = 0;
            eTask->in.L0Surface = eTask->in.L1Surface = NULL;
            eTask->in.NumExtParam = eTask->bufs->I_bufs.in.NumExtParam;
            eTask->in.ExtParam    = eTask->bufs->I_bufs.in.ExtParam;
            eTask->out.NumExtParam = eTask->bufs->I_bufs.out.NumExtParam;
            eTask->out.ExtParam    = eTask->bufs->I_bufs.out.ExtParam;
        }

        /* PAK data */
        if (m_pmfxPAK)
        {
            eTask->inPAK.NumFrameL0 = eTask->inPAK.NumFrameL1 = 0;
            eTask->inPAK.L0Surface = eTask->inPAK.L1Surface = NULL;
            eTask->inPAK.NumExtParam = eTask->bufs->I_bufs.out.NumExtParam;
            eTask->inPAK.ExtParam    = eTask->bufs->I_bufs.out.ExtParam;
            eTask->outPAK.NumExtParam = eTask->bufs->I_bufs.in.NumExtParam;
            eTask->outPAK.ExtParam    = eTask->bufs->I_bufs.in.ExtParam;
        }
        break;

    case MFX_FRAMETYPE_P:
    {
        if (m_pmfxENC)
        {
            eTask->in.NumFrameL0 = (mfxU16)m_ref_info.reference_frames.size();
            eTask->in.NumFrameL1 = 0;
            eTask->in.L0Surface = &m_ref_info.reference_frames[0];
            eTask->in.L1Surface = NULL;
            eTask->in.NumExtParam = eTask->bufs->PB_bufs.in.NumExtParam;
            eTask->in.ExtParam    = eTask->bufs->PB_bufs.in.ExtParam;
            eTask->out.NumExtParam = eTask->bufs->PB_bufs.out.NumExtParam;
            eTask->out.ExtParam    = eTask->bufs->PB_bufs.out.ExtParam;
        }

        if (m_pmfxPAK)
        {
            eTask->inPAK.NumFrameL0 = (mfxU16)m_ref_info.reference_frames.size();
            eTask->inPAK.NumFrameL1 = 0;
            eTask->inPAK.L0Surface = &m_ref_info.reference_frames[0];
            eTask->inPAK.L1Surface = NULL;
            eTask->inPAK.NumExtParam = eTask->bufs->PB_bufs.out.NumExtParam;
            eTask->inPAK.ExtParam    = eTask->bufs->PB_bufs.out.ExtParam;
            eTask->outPAK.NumExtParam = eTask->bufs->PB_bufs.in.NumExtParam;
            eTask->outPAK.ExtParam    = eTask->bufs->PB_bufs.in.ExtParam;
        }
    }
    break;

    case MFX_FRAMETYPE_B:
    {
        if (m_pmfxENC)
        {
            eTask->in.NumFrameL0 = (mfxU16)m_ref_info.reference_frames.size();
            eTask->in.NumFrameL1 = 0;
            eTask->in.L0Surface = &m_ref_info.reference_frames[0];
            eTask->in.L1Surface = NULL;
            eTask->in.NumExtParam = eTask->bufs->PB_bufs.in.NumExtParam;
            eTask->in.ExtParam    = eTask->bufs->PB_bufs.in.ExtParam;
            eTask->out.NumExtParam = eTask->bufs->PB_bufs.out.NumExtParam;
            eTask->out.ExtParam    = eTask->bufs->PB_bufs.out.ExtParam;
        }

        if (m_pmfxPAK)
        {
            eTask->inPAK.NumFrameL0 = (mfxU16)m_ref_info.reference_frames.size();
            eTask->inPAK.NumFrameL1 = 0;
            eTask->inPAK.L0Surface = &m_ref_info.reference_frames[0];
            eTask->inPAK.L1Surface = NULL;
            eTask->inPAK.NumExtParam = eTask->bufs->PB_bufs.out.NumExtParam;
            eTask->inPAK.ExtParam    = eTask->bufs->PB_bufs.out.ExtParam;
            eTask->outPAK.NumExtParam = eTask->bufs->PB_bufs.in.NumExtParam;
            eTask->outPAK.ExtParam    = eTask->bufs->PB_bufs.in.ExtParam;
        }
    }
    break;
    }

    /* SPS, PPS, SliceHeader processing */
    for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
    {
        switch (ExtractFrameType(*eTask, fieldId) & MFX_FRAMETYPE_IPB)
        {
        case MFX_FRAMETYPE_I:
            /* Process SPS, and PPS only*/
            if (feiPPS || feiSPS || feiSliceHeader)
            {
                if (feiPPS)
                {
                    feiPPS[fieldId].NumRefIdxL0Active = 0;
                    feiPPS[fieldId].NumRefIdxL1Active = 0;
                    /*I is always reference */
                    feiPPS[fieldId].ReferencePicFlag = 1;

                    feiPPS[fieldId].IDRPicFlag = (ExtractFrameType(*eTask, fieldId) & MFX_FRAMETYPE_IDR) ? 1 : 0;
                    if (feiPPS[fieldId].IDRPicFlag)
                        m_frameIdrCounter++;
                    feiPPS[fieldId].FrameNum   = eTask->m_frameNum;

                    memset(feiPPS[fieldId].ReferenceFrames, -1, 16*sizeof(mfxU16));
                    memcpy(feiPPS[fieldId].ReferenceFrames, &m_ref_info.state[fieldId].dpb_idx[0], sizeof(mfxU16)*m_ref_info.state[fieldId].dpb_idx.size());
                }

                if (feiSliceHeader)
                {
                    MSDK_CHECK_POINTER(feiSliceHeader[fieldId].Slice, MFX_ERR_NULL_PTR);

                    for (mfxU32 i = 0; i < feiSliceHeader[fieldId].NumSlice; i++)
                    {
                        MSDK_ZERO_ARRAY(feiSliceHeader[fieldId].Slice[i].RefL0, 32);
                        MSDK_ZERO_ARRAY(feiSliceHeader[fieldId].Slice[i].RefL1, 32);
                        feiSliceHeader[fieldId].Slice[i].SliceType = FEI_SLICETYPE_I;
                        feiSliceHeader[fieldId].Slice[i].NumRefIdxL0Active = 0;
                        feiSliceHeader[fieldId].Slice[i].NumRefIdxL1Active = 0;
                        feiSliceHeader[fieldId].Slice[i].IdrPicId          = m_frameIdrCounter;
                    }
                }
            }
            break;

        case MFX_FRAMETYPE_P:
            /* PPS only */
            if (feiPPS || feiSliceHeader)
            {
                if (feiPPS)
                {
                    feiPPS[fieldId].IDRPicFlag        = 0;
                    feiPPS[fieldId].NumRefIdxL0Active = (mfxU16)m_ref_info.state[fieldId].l0_idx.size();
                    feiPPS[fieldId].NumRefIdxL1Active = 0;
                    feiPPS[fieldId].ReferencePicFlag  = (ExtractFrameType(*eTask, fieldId) & MFX_FRAMETYPE_REF) ? 1 : 0;
                    feiPPS[fieldId].FrameNum          = eTask->m_frameNum;

                    memset(feiPPS[fieldId].ReferenceFrames, -1, 16 * sizeof(mfxU16));
                    memcpy(feiPPS[fieldId].ReferenceFrames, &m_ref_info.state[fieldId].dpb_idx[0], sizeof(mfxU16)*m_ref_info.state[fieldId].dpb_idx.size());
                }

                if (feiSliceHeader)
                {
                    MSDK_CHECK_POINTER(feiSliceHeader[fieldId].Slice, MFX_ERR_NULL_PTR);

                    for (mfxU32 i = 0; i < feiSliceHeader[fieldId].NumSlice; i++)
                    {
                        MSDK_ZERO_ARRAY(feiSliceHeader[fieldId].Slice[i].RefL0, 32);
                        MSDK_ZERO_ARRAY(feiSliceHeader[fieldId].Slice[i].RefL1, 32);
                        feiSliceHeader[fieldId].Slice[i].SliceType = FEI_SLICETYPE_P;

                        for (mfxU32 k = 0; k < m_ref_info.state[fieldId].l0_idx.size(); k++)
                        {
                            feiSliceHeader[fieldId].Slice[i].RefL0[k].Index       = m_ref_info.state[fieldId].l0_idx[k];
                            feiSliceHeader[fieldId].Slice[i].RefL0[k].PictureType = (mfxU16)(!m_isField ? MFX_PICTYPE_FRAME :
                                (m_ref_info.state[fieldId].l0_parity[k] ? MFX_PICTYPE_BOTTOMFIELD : MFX_PICTYPE_TOPFIELD));
                        }
                        feiSliceHeader[fieldId].Slice[i].NumRefIdxL0Active = (mfxU16)m_ref_info.state[fieldId].l0_idx.size();
                        feiSliceHeader[fieldId].Slice[i].NumRefIdxL1Active = 0;
                        feiSliceHeader[fieldId].Slice[i].IdrPicId          = m_frameIdrCounter;
                    }
                }
            }
            break;

        case MFX_FRAMETYPE_B:
            /* PPS only */
            if (feiPPS || feiSliceHeader)
            {
                if (feiPPS)
                {
                    feiPPS[fieldId].IDRPicFlag        = 0;
                    feiPPS[fieldId].NumRefIdxL0Active = (mfxU16)m_ref_info.state[fieldId].l0_idx.size();
                    feiPPS[fieldId].NumRefIdxL1Active = (mfxU16)m_ref_info.state[fieldId].l1_idx.size();
                    feiPPS[fieldId].ReferencePicFlag  = 0;
                    feiPPS[fieldId].IDRPicFlag        = 0;
                    feiPPS[fieldId].FrameNum          = eTask->m_frameNum;

                    memset(feiPPS[fieldId].ReferenceFrames, -1, 16 * sizeof(mfxU16));
                    memcpy(feiPPS[fieldId].ReferenceFrames, &m_ref_info.state[fieldId].dpb_idx[0], sizeof(mfxU16)*m_ref_info.state[fieldId].dpb_idx.size());
                }

                if (feiSliceHeader)
                {
                    MSDK_CHECK_POINTER(feiSliceHeader[fieldId].Slice, MFX_ERR_NULL_PTR);

                    for (mfxU32 i = 0; i < feiSliceHeader[fieldId].NumSlice; i++)
                    {
                        MSDK_ZERO_ARRAY(feiSliceHeader[fieldId].Slice[i].RefL0, 32);
                        MSDK_ZERO_ARRAY(feiSliceHeader[fieldId].Slice[i].RefL1, 32);
                        feiSliceHeader[fieldId].Slice[i].SliceType = FEI_SLICETYPE_B;

                        for (mfxU32 k = 0; k < m_ref_info.state[fieldId].l0_idx.size(); k++)
                        {
                            feiSliceHeader[fieldId].Slice[i].RefL0[k].Index       = m_ref_info.state[fieldId].l0_idx[k];
                            feiSliceHeader[fieldId].Slice[i].RefL0[k].PictureType = (mfxU16)(!m_isField ? MFX_PICTYPE_FRAME :
                                (m_ref_info.state[fieldId].l0_parity[k] ? MFX_PICTYPE_BOTTOMFIELD : MFX_PICTYPE_TOPFIELD));
                        }
                        for (mfxU32 k = 0; k < m_ref_info.state[fieldId].l1_idx.size(); k++)
                        {
                            feiSliceHeader[fieldId].Slice[i].RefL1[k].Index       = m_ref_info.state[fieldId].l1_idx[k];
                            feiSliceHeader[fieldId].Slice[i].RefL1[k].PictureType = (mfxU16)(!m_isField ? MFX_PICTYPE_FRAME :
                                (m_ref_info.state[fieldId].l1_parity[k] ? MFX_PICTYPE_BOTTOMFIELD : MFX_PICTYPE_TOPFIELD));
                        }
                        feiSliceHeader[fieldId].Slice[i].NumRefIdxL0Active = (mfxU16)m_ref_info.state[fieldId].l0_idx.size();
                        feiSliceHeader[fieldId].Slice[i].NumRefIdxL1Active = (mfxU16)m_ref_info.state[fieldId].l1_idx.size();
                        feiSliceHeader[fieldId].Slice[i].IdrPicId          = m_frameIdrCounter;
                    }
                } // if (feiSliceHeader)
            }
            break;
        }
    }

    /* the rest ext buffers */
    int encCtrlId = 0;
    for (int i = 0; i<eTask->in.NumExtParam; i++)
    {
        switch (eTask->in.ExtParam[i]->BufferId){
        case MFX_EXTBUFF_FEI_ENC_CTRL:

            feiEncCtrl = (mfxExtFeiEncFrameCtrl*)(eTask->in.ExtParam[i]);
            feiEncCtrl->MVPredictor = (ExtractFrameType(*eTask, encCtrlId) & MFX_FRAMETYPE_I) ? 0 : (m_encpakParams.mvinFile != NULL) || m_encpakParams.bPREENC;

            // adjust ref window size if search window is 0
            if (m_encpakParams.SearchWindow == 0 && m_pmfxENC)
            {
                // window size is limited to 1024 for bi-prediction
                if ((ExtractFrameType(*eTask, encCtrlId) & MFX_FRAMETYPE_B) && m_encpakParams.RefHeight * m_encpakParams.RefWidth > 1024)
                {
                    feiEncCtrl->RefHeight = 32;
                    feiEncCtrl->RefWidth  = 32;
                }
                else{
                    feiEncCtrl->RefHeight = m_encpakParams.RefHeight;
                    feiEncCtrl->RefWidth  = m_encpakParams.RefWidth;
                }
            }
            encCtrlId++;
            break;

        case MFX_EXTBUFF_FEI_ENC_MB:
            if (pEncMBs)
            {
                pMbEncCtrl = (mfxExtFeiEncMBCtrl*)(eTask->in.ExtParam[i]);
                SAFE_FREAD(pMbEncCtrl->MB, sizeof(pMbEncCtrl->MB[0])*pMbEncCtrl->NumMBAlloc, 1, pEncMBs, MFX_ERR_MORE_DATA);
            }
            break;

        case MFX_EXTBUFF_FEI_PREENC_QP:
            if (pPerMbQP)
            {
                pMbQP = (mfxExtFeiEncQP*)(eTask->in.ExtParam[i]);
                SAFE_FREAD(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP, MFX_ERR_MORE_DATA);
            }
            break;
        } // switch (eTask->in.ExtParam[i]->BufferId)
    } // for (int i = 0; i<eTask->in.NumExtParam; i++)

    mdprintf(stderr, "enc: %d t: %d %d\n", eTask->m_frameOrder, (eTask->m_type[0] & MFX_FRAMETYPE_IPB), (eTask->m_type[1] & MFX_FRAMETYPE_IPB));
    return sts;
}

mfxStatus CEncodingPipeline::InitEncodeFrameParams(mfxFrameSurface1* encodeSurface, sTask* pCurrentTask, PairU8 frameType, bool is_buffered)
{
    mfxStatus sts = MFX_ERR_NONE;
    if (!is_buffered){
        MSDK_CHECK_POINTER(encodeSurface, MFX_ERR_NULL_PTR);
    }
    MSDK_CHECK_POINTER(pCurrentTask,  MFX_ERR_NULL_PTR);

    bufSet * freeSet = getFreeBufSet(m_encodeBufs);
    MSDK_CHECK_POINTER(freeSet, MFX_ERR_NULL_PTR);
    if (m_bNeedDRC){
        sts = ResetExtBufMBnum(freeSet);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    pCurrentTask->bufs.push_back(std::pair<bufSet*, mfxFrameSurface1*>(freeSet, encodeSurface));

    /* Load input Buffer for FEI ENCODE */
    mfxExtFeiEncMVPredictors* pMvPredBuf = NULL;
    mfxExtFeiEncMBCtrl*       pMbEncCtrl = NULL;
    mfxExtFeiEncQP*           pMbQP      = NULL;
    mfxExtFeiEncFrameCtrl*    feiEncCtrl = NULL;

    mfxU32 feiEncCtrlId = m_ffid, pMvPredId = m_ffid, fieldId = 0;
    for (mfxU32 i = 0; i < freeSet->PB_bufs.in.NumExtParam; i++)
    {
        switch (freeSet->PB_bufs.in.ExtParam[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_ENC_MV_PRED:
            if (!m_encpakParams.bPREENC && pMvPred)
            {
                if (!(frameType[pMvPredId] & MFX_FRAMETYPE_I))
                {
                    pMvPredBuf = (mfxExtFeiEncMVPredictors*)(freeSet->PB_bufs.in.ExtParam[i]);
                    if (m_encpakParams.bRepackPreencMV)
                    {
                        SAFE_FREAD(m_tmpForReading, sizeof(*m_tmpForReading)*m_numMB, 1, pMvPred, MFX_ERR_MORE_DATA);
                        repackPreenc2Enc(m_tmpForReading, pMvPredBuf->MB, m_numMB, m_tmpForMedian);
                    }
                    else {
                        SAFE_FREAD(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0])*pMvPredBuf->NumMBAlloc, 1, pMvPred, MFX_ERR_MORE_DATA);
                    }
                }
                else{
                    int shft = m_encpakParams.bRepackPreencMV ? sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB) : sizeof(mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB);
                    SAFE_FSEEK(pMvPred, shft*m_numMB, SEEK_CUR, MFX_ERR_MORE_DATA);
                }
            }
            pMvPredId = 1 - pMvPredId; // set to sfid
            break;

        case MFX_EXTBUFF_FEI_ENC_MB:
            if (pEncMBs){
                pMbEncCtrl = (mfxExtFeiEncMBCtrl*)(freeSet->PB_bufs.in.ExtParam[i]);
                SAFE_FREAD(pMbEncCtrl->MB, sizeof(pMbEncCtrl->MB[0])*pMbEncCtrl->NumMBAlloc, 1, pEncMBs, MFX_ERR_MORE_DATA);
            }
            break;

        case MFX_EXTBUFF_FEI_PREENC_QP:
            if (pPerMbQP){
                pMbQP = (mfxExtFeiEncQP*)(freeSet->PB_bufs.in.ExtParam[i]);
                SAFE_FREAD(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP, MFX_ERR_MORE_DATA);
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_CTRL:
            feiEncCtrl = (mfxExtFeiEncFrameCtrl*)(freeSet->PB_bufs.in.ExtParam[i]);

            // adjust ref window size if search window is 0
            if (m_encpakParams.SearchWindow == 0)
            {
                bool adjust_window = (frameType[feiEncCtrlId] & MFX_FRAMETYPE_B) && m_encpakParams.RefHeight * m_encpakParams.RefWidth > 1024;

                feiEncCtrl->RefHeight = adjust_window ? 32 : m_encpakParams.RefHeight;
                feiEncCtrl->RefWidth  = adjust_window ? 32 : m_encpakParams.RefWidth;
            }

            feiEncCtrl->MVPredictor = (!(frameType[feiEncCtrlId] & MFX_FRAMETYPE_I) && (m_encpakParams.mvinFile != NULL || m_encpakParams.bPREENC)) ? 1 : 0;

            // Need to set the number to actual ref number for each field
            feiEncCtrl->NumMVPredictors[0] = m_numOfRefs[fieldId][0];
            feiEncCtrl->NumMVPredictors[1] = m_numOfRefs[fieldId][1];
            fieldId++;

            feiEncCtrlId = 1 - feiEncCtrlId; // set to sfid
            break;
        } // switch (freeSet->PB_bufs.in.ExtParam[i]->BufferId)
    } // for (int i = 0; i<freeSet->PB_bufs.in.NumExtParam; i++)

    // Add input buffers
    bool is_I_frame = !m_isField && (frameType[m_ffid] & MFX_FRAMETYPE_I);

    MSDK_CHECK_POINTER(m_ctr, MFX_ERR_NULL_PTR);
    m_ctr->NumExtParam = is_I_frame ? freeSet->I_bufs.in.NumExtParam : freeSet->PB_bufs.in.NumExtParam;
    m_ctr->ExtParam    = is_I_frame ? freeSet->I_bufs.in.ExtParam    : freeSet->PB_bufs.in.ExtParam;

    // Add output buffers
    pCurrentTask->mfxBS.NumExtParam = freeSet->PB_bufs.out.NumExtParam;
    pCurrentTask->mfxBS.ExtParam    = freeSet->PB_bufs.out.ExtParam;

    return sts;
}

/* read input / write output */

mfxStatus CEncodingPipeline::ReadPAKdata(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxExtFeiEncMV*     mvBuf     = NULL;
    mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;

    for (int i = 0; i < eTask->inPAK.NumExtParam; i++){
        switch (eTask->inPAK.ExtParam[i]->BufferId){
        case MFX_EXTBUFF_FEI_ENC_MV:
            if (mvENCPAKout){
                mvBuf = (mfxExtFeiEncMV*)(eTask->inPAK.ExtParam[i]);
                SAFE_FREAD(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout, MFX_ERR_MORE_DATA);
            }
            break;

        case MFX_EXTBUFF_FEI_PAK_CTRL:
            if (mbcodeout){
                mbcodeBuf = (mfxExtFeiPakMBCtrl*)(eTask->inPAK.ExtParam[i]);
                SAFE_FREAD(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout, MFX_ERR_MORE_DATA);
            }
            break;
        } // switch (eTask->inPAK.ExtParam[i]->BufferId)
    } // for (int i = 0; i < eTask->inPAK.NumExtParam; i++)

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::DropENCPAKoutput(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxExtFeiEncMV*     mvBuf     = NULL;
    mfxExtFeiEncMBStat* mbstatBuf = NULL;
    mfxExtFeiPakMBCtrl* mbcodeBuf = NULL;

    int mvBufId = 0;
    for (int i = 0; i < eTask->out.NumExtParam; i++)
    {
        switch (eTask->out.ExtParam[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_ENC_MV:
            if (mvENCPAKout)
            {
                if (!(ExtractFrameType(*eTask, mvBufId) & MFX_FRAMETYPE_I)){
                    mvBuf = (mfxExtFeiEncMV*)(eTask->out.ExtParam[i]);
                    SAFE_FWRITE(mvBuf->MB, sizeof(mvBuf->MB[0])*mvBuf->NumMBAlloc, 1, mvENCPAKout, MFX_ERR_MORE_DATA);
                }
                else
                {
                    for (mfxU32 k = 0; k < m_numMB; k++)
                    {
                        SAFE_FWRITE(m_tmpMBenc, sizeof(*m_tmpMBenc), 1, mvENCPAKout, MFX_ERR_MORE_DATA);
                    }
                }
                mvBufId++;
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_MB_STAT:
            if (mbstatout){
                mbstatBuf = (mfxExtFeiEncMBStat*)(eTask->out.ExtParam[i]);
                SAFE_FWRITE(mbstatBuf->MB, sizeof(mbstatBuf->MB[0])*mbstatBuf->NumMBAlloc, 1, mbstatout, MFX_ERR_MORE_DATA);
            }
            break;

        case MFX_EXTBUFF_FEI_PAK_CTRL:
            if (mbcodeout){
                mbcodeBuf = (mfxExtFeiPakMBCtrl*)(eTask->out.ExtParam[i]);
                SAFE_FWRITE(mbcodeBuf->MB, sizeof(mbcodeBuf->MB[0])*mbcodeBuf->NumMBAlloc, 1, mbcodeout, MFX_ERR_MORE_DATA);
            }
            break;
        } // switch (eTask->out.ExtParam[i]->BufferId)
    } // for(int i=0; i<eTask->out.NumExtParam; i++)

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::DropPREENCoutput(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxExtFeiPreEncMBStat* mbdata = NULL;
    mfxExtFeiPreEncMV*     mvs    = NULL;

    int mvsId = 0;
    for (int i = 0; i < eTask->out.NumExtParam; i++)
    {
        switch (eTask->out.ExtParam[i]->BufferId){
        case MFX_EXTBUFF_FEI_PREENC_MB:
            if (mbstatout){
                mbdata = (mfxExtFeiPreEncMBStat*)(eTask->out.ExtParam[i]);
                SAFE_FWRITE(mbdata->MB, sizeof(mbdata->MB[0]) * mbdata->NumMBAlloc, 1, mbstatout, MFX_ERR_MORE_DATA);
            }
            break;

        case MFX_EXTBUFF_FEI_PREENC_MV:
            if (mvout){
                if (ExtractFrameType(*eTask, mvsId) & MFX_FRAMETYPE_I)
                {                                                   // IP pair
                    for (int k = 0; k < m_numMBpreenc; k++){        // in progressive case Ext buffer for I frame is detached
                        SAFE_FWRITE(m_tmpMBpreenc, sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB), 1, mvout, MFX_ERR_MORE_DATA);
                    }
                }
                else
                {
                    mvs = (mfxExtFeiPreEncMV*)(eTask->out.ExtParam[i]);
                    SAFE_FWRITE(mvs->MB, sizeof(mvs->MB[0]) * mvs->NumMBAlloc, 1, mvout, MFX_ERR_MORE_DATA);
                }
            }
            mvsId++;
            break;
        } // switch (eTask->out.ExtParam[i]->BufferId)
    } //for (int i = 0; i < eTask->out.NumExtParam; i++)

    if (!eTask->m_fieldPicFlag && mvout && (ExtractFrameType(*eTask) & MFX_FRAMETYPE_I)){ // drop 0x8000 for progressive I-frames
        for (mfxU32 k = 0; k < m_numMBpreenc; k++){
            SAFE_FWRITE(m_tmpMBpreenc, sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB), 1, mvout, MFX_ERR_MORE_DATA);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::PassPreEncMVPred2EncEx(iTask* eTask, mfxU16 numMVP[2][2])
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxExtFeiEncMVPredictors* mvp    = NULL;
    bufSet*                   set    = NULL;

    mfxStatus sts = MFX_ERR_NONE;

    mfxU32 nPred_actual = 0;
    mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB ** bestDistortionPredMB[MaxFeiEncMVPNum];
    mfxU32 *refIdx[MaxFeiEncMVPNum];

    for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
    {
        MSDK_ZERO_ARRAY(bestDistortionPredMB, MaxFeiEncMVPNum);
        MSDK_ZERO_ARRAY(refIdx, MaxFeiEncMVPNum);

        mfxU32 nPred_actual[2] = { IPP_MIN(numMVP[fieldId][0], MaxFeiEncMVPNum), IPP_MIN(numMVP[fieldId][1], MaxFeiEncMVPNum) }; //adjust n of passed pred
        mfxU32 nOfPredPairs = IPP_MAX(nPred_actual[0], nPred_actual[1]);
        if (nOfPredPairs == 0 || (ExtractFrameType(*eTask, fieldId) & MFX_FRAMETYPE_I))
            continue; // I-field

        if (set == NULL) {
            set = getFreeBufSet(m_encodeBufs);
            if (set == NULL){
                sts = MFX_ERR_NULL_PTR;
                break;
            }
        }

        // MVP is predictors Ext Buffer
        mvp = (mfxExtFeiEncMVPredictors*)getBufById(&set->PB_bufs.in, MFX_EXTBUFF_FEI_ENC_MV_PRED, fieldId);
        if (mvp == NULL){
            sts = MFX_ERR_NULL_PTR;
            break;
        }

        for (mfxU32 j = 0; j < nOfPredPairs; j++)
        {
            bestDistortionPredMB[j] = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *[2]; // L0/L1 best 16 MV by distortion
            if (bestDistortionPredMB[j] == NULL){
                sts = MFX_ERR_NULL_PTR;
                break;
            }
            MSDK_ZERO_ARRAY(bestDistortionPredMB[j], 2);

            refIdx[j] = new mfxU32[2];
            if (refIdx[j] == NULL){
                sts = MFX_ERR_NULL_PTR;
                break;
            }
            MSDK_ZERO_ARRAY(refIdx[j], 2);
        }

        for (mfxU32 i = 0; i < m_numMBpreenc; i++) // get best nPred_actual L0/L1 predictors for each MB
        {
            sts = GetBestSetByDistortion(eTask->preenc_mvp_info, bestDistortionPredMB, refIdx, nPred_actual, fieldId, i);
            MSDK_BREAK_ON_ERROR(sts);

            for (mfxU32 j = 0; j < nOfPredPairs; j++)
            {
                if (!!m_encpakParams.preencDSstrength)
                {
                    sts = repackDSPreenc2EncExMB(bestDistortionPredMB[j], mvp, i, refIdx[j], j);
                }
                else
                {
                    sts = repackPreenc2EncExOneMB(bestDistortionPredMB[j], &mvp->MB[i], refIdx[j], j, m_tmpForMedian);
                }
                MSDK_BREAK_ON_ERROR(sts);
            }
        }

        for (mfxU32 j = 0; j < nOfPredPairs; j++)
        {
            MSDK_SAFE_DELETE_ARRAY(bestDistortionPredMB[j]);
            MSDK_SAFE_DELETE_ARRAY(refIdx[j]);
        }

    } // for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)

    /* do this if we breaked on error */
    for (mfxU32 j = 0; j < nPred_actual; j++)
    {
        MSDK_SAFE_DELETE_ARRAY(bestDistortionPredMB[j]);
        MSDK_SAFE_DELETE_ARRAY(refIdx[j]);
    }

    SAFE_RELEASE_EXT_BUFSET(set);

    sts = ReleasePreencMVPinfo(eTask);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

/* Simplified conversion - no sorting by distortion, no median on MV */
mfxStatus CEncodingPipeline::PassPreEncMVPred2EncExPerf(iTask* eTask, mfxU16 numMVP[2][2])
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    mfxExtFeiEncMVPredictors* mvp = NULL;
    mfxExtFeiPreEncMV**       mvs = NULL;
    bufSet*                   set = NULL;

    mfxU32 *refIdx[MaxFeiEncMVPNum];
    mfxU32 nPred_actual = 0;

    for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
    {
        MSDK_ZERO_ARRAY(refIdx, MaxFeiEncMVPNum);

        mfxU32 nPred_actual[2] = { IPP_MIN(numMVP[fieldId][0], MaxFeiEncMVPNum), IPP_MIN(numMVP[fieldId][1], MaxFeiEncMVPNum) }; //adjust n of passed pred
        mfxU32 nOfPredPairs = IPP_MAX(nPred_actual[0], nPred_actual[1]);
        if (nOfPredPairs == 0 || (ExtractFrameType(*eTask, fieldId) & MFX_FRAMETYPE_I))
            continue; // I-field

        mvs = new mfxExtFeiPreEncMV*[nOfPredPairs];
        if (mvs == NULL){
            sts = MFX_ERR_NULL_PTR;
            break;
        }
        MSDK_ZERO_ARRAY(mvs, nOfPredPairs);

        if (set == NULL) {
            set = getFreeBufSet(m_encodeBufs);
            if (set == NULL){
                sts = MFX_ERR_NULL_PTR;
                break;
            }
        }

        mvp = (mfxExtFeiEncMVPredictors*)getBufById(&set->PB_bufs.in, MFX_EXTBUFF_FEI_ENC_MV_PRED, fieldId);
        if (mvp == NULL){
            sts = MFX_ERR_NULL_PTR;
            break;
        }

        mfxU32 j = 0;
        for (std::list<PreEncMVPInfo>::iterator it = eTask->preenc_mvp_info.begin(); it != eTask->preenc_mvp_info.end() && j < nOfPredPairs; ++it, ++j)
        {
            mvs[j] = (mfxExtFeiPreEncMV*)getBufById(&(*it).preenc_output_bufs->PB_bufs.out, MFX_EXTBUFF_FEI_PREENC_MV, fieldId);
            if (mvs[j] == NULL){
                sts = MFX_ERR_NULL_PTR;
                break;
            }

            refIdx[j] = new mfxU32[2];
            if (refIdx[j] == NULL){
                sts = MFX_ERR_NULL_PTR;
                break;
            }
            memcpy(refIdx[j], (*it).refIdx[fieldId], 2*sizeof(mfxU32));
        }

        for (mfxU32 i = 0; i < m_numMB; i++) // get nPred_actual L0/L1 predictors for each MB
        {
            for (mfxU32 j = 0; j < nOfPredPairs; j++)
            {
                mvp->MB[i].RefIdx[j].RefL0 = refIdx[j][0];
                mvp->MB[i].RefIdx[j].RefL1 = refIdx[j][1];

                if (!m_encpakParams.preencDSstrength)
                {
                    memcpy(mvp->MB[i].MV[j], mvs[j]->MB[i].MV[0], 2 * sizeof(mfxI16Pair));
                }
                else
                {
                    static mfxI16 MVZigzagOrder[16] = { 0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15 };

                    mfxU32 preencMBidx = i / m_widthMB / m_encpakParams.preencDSstrength * m_widthMBpreenc + i % m_widthMB / m_encpakParams.preencDSstrength;
                    if (preencMBidx >= m_numMBpreenc)
                        continue;

                    mfxU8 MVrow = i / m_widthMB % m_encpakParams.preencDSstrength, MVcolumn = i % m_widthMB % m_encpakParams.preencDSstrength;
                    mfxU8 preencMBMVidx = 0;
                    switch (m_encpakParams.preencDSstrength)
                    {
                    case 2:
                        preencMBMVidx = MVrow * 8 + MVcolumn * 2;
                        break;
                    case 4:
                        preencMBMVidx = MVrow * 4 + MVcolumn;
                        break;
                    case 8:
                        preencMBMVidx = MVrow / 2 * 4 + MVcolumn / 2;
                        break;
                    default:
                        break;
                    }

                    memcpy(mvp->MB[i].MV[j], mvs[j]->MB[preencMBidx].MV[MVZigzagOrder[preencMBMVidx]], 2 * sizeof(mfxI16Pair));

                    mvp->MB[i].MV[j][0].x *= m_encpakParams.preencDSstrength;
                    mvp->MB[i].MV[j][0].y *= m_encpakParams.preencDSstrength;
                    mvp->MB[i].MV[j][1].x *= m_encpakParams.preencDSstrength;
                    mvp->MB[i].MV[j][1].y *= m_encpakParams.preencDSstrength;
                }
            }
        }

        MSDK_SAFE_DELETE_ARRAY(mvs);
        for (mfxU32 j = 0; j < nOfPredPairs; j++)
        {
            MSDK_SAFE_DELETE_ARRAY(refIdx[j]);
        }
    } // for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)

    /* do this if we breaked on error */
    MSDK_SAFE_DELETE_ARRAY(mvs);
    for (mfxU32 j = 0; j < nPred_actual; j++)
    {
        MSDK_SAFE_DELETE_ARRAY(refIdx[j]);
    }

    SAFE_RELEASE_EXT_BUFSET(set);

    sts = ReleasePreencMVPinfo(eTask);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

mfxStatus ResetExtBufMBnum(bufSet* freeSet)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxExtFeiEncMV*           mvBuf       = NULL;
    mfxExtFeiEncMBStat*       mbstatBuf   = NULL;
    mfxExtFeiPakMBCtrl*       mbcodeBuf   = NULL;
    mfxExtFeiEncQP*           pMbQP       = NULL;
    mfxExtFeiEncMBCtrl*       pMbEncCtrl  = NULL;
    mfxExtFeiEncMVPredictors* pMvPredBuf  = NULL;

    if (bDRCReset == false)
    {
        return MFX_ERR_NONE;
    }
    setElem &bufsIn = freeSet->PB_bufs.in;
    for (mfxU16 i = 0; i < bufsIn.NumExtParam; i++)
    {
        switch (bufsIn.ExtParam[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_ENC_MV_PRED:
            if (pMvPred)
            {
                pMvPredBuf = (mfxExtFeiEncMVPredictors*)(bufsIn.ExtParam[i]);
                pMvPredBuf->NumMBAlloc = numMBs ? numMBs : pMvPredBuf->NumMBAlloc;
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_MB:
            if (pEncMBs)
            {
                pMbEncCtrl = (mfxExtFeiEncMBCtrl*)(freeSet->PB_bufs.in.ExtParam[i]);
                pMbEncCtrl->NumMBAlloc  = numMBs ? numMBs :pMbEncCtrl->NumMBAlloc ;
            }
            break;

        case MFX_EXTBUFF_FEI_PREENC_QP:
            if (pPerMbQP)
            {
                pMbQP = (mfxExtFeiEncQP*)(bufsIn.ExtParam[i]);
                pMbQP->NumQPAlloc = numMBs ? numMBs : pMbQP->NumQPAlloc;
            }
            break;
        }
    }

    setElem &bufsOut = freeSet->PB_bufs.out;
    for (mfxU16 i = 0; i < bufsOut.NumExtParam; i++)
    {
        switch (bufsOut.ExtParam[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_ENC_MV:
            if (mvENCPAKout)
            {
               mvBuf = (mfxExtFeiEncMV*)(bufsOut.ExtParam[i]);
               mvBuf->NumMBAlloc = numMBs ? numMBs : mvBuf->NumMBAlloc;
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_MB_STAT:
            if (mbstatout)
            {
                mbstatBuf = (mfxExtFeiEncMBStat*)(bufsOut.ExtParam[i]);
                mbstatBuf->NumMBAlloc = numMBs ? numMBs : mbstatBuf->NumMBAlloc;
            }
            break;

        case MFX_EXTBUFF_FEI_PAK_CTRL:
            if (mbcodeout)
            {
                mbcodeBuf = (mfxExtFeiPakMBCtrl*)(bufsOut.ExtParam[i]);
                mbcodeBuf->NumMBAlloc = numMBs ? numMBs : mbcodeBuf->NumMBAlloc;
            }
            break;
        }
    }
    return sts;
}
/* repackPreenc2Enc passes only one predictor (median of provided preenc MVs) because we dont have distortions to choose 4 best possible */

mfxStatus repackPreenc2Enc(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *preencMVoutMB, mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB *EncMVPredMB, mfxU32 NumMB, mfxI16 *tmpBuf)
{
    MSDK_ZERO_ARRAY(EncMVPredMB, NumMB);
    for (mfxU32 i = 0; i<NumMB; i++)
    {
        //only one ref is used for now
        for (int j = 0; j < 4; j++){
            EncMVPredMB[i].RefIdx[j].RefL0 = 0;
            EncMVPredMB[i].RefIdx[j].RefL1 = 0;
        }

        //use only first subblock component of MV
        for (int j = 0; j < 2; j++){
            EncMVPredMB[i].MV[0][j].x = get16Median(preencMVoutMB + i, tmpBuf, 0, j);
            EncMVPredMB[i].MV[0][j].y = get16Median(preencMVoutMB + i, tmpBuf, 1, j);
        }
    } // for(mfxU32 i=0; i<NumMBAlloc; i++)

    return MFX_ERR_NONE;
}

mfxStatus repackPreenc2EncExOneMB(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *preencMVoutMB[2], mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB *EncMVPredMB, mfxU32 refIdx[2], mfxU32 predIdx, mfxI16 *tmpBuf)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (0 == predIdx) {
        MSDK_ZERO_MEMORY(*EncMVPredMB);
    }

    EncMVPredMB->RefIdx[predIdx].RefL0 = refIdx[0];
    EncMVPredMB->RefIdx[predIdx].RefL1 = refIdx[1];

    EncMVPredMB->MV[predIdx][0].x = get16Median(preencMVoutMB[0], tmpBuf, 0, 0);
    EncMVPredMB->MV[predIdx][0].y = get16Median(preencMVoutMB[0], tmpBuf, 1, 0);

    if (preencMVoutMB[1])
    {
        EncMVPredMB->MV[predIdx][1].x = get16Median(preencMVoutMB[1], tmpBuf, 0, 1);
        EncMVPredMB->MV[predIdx][1].y = get16Median(preencMVoutMB[1], tmpBuf, 1, 1);
    }

    return sts;
}

mfxStatus CEncodingPipeline::repackDSPreenc2EncExMB(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *preencMVoutMB[2], mfxExtFeiEncMVPredictors *EncMVPred, mfxU32 MBnum, mfxU32 refIdx[2], mfxU32 predIdx)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 mv_idx = 0;
    static mfxI16 MVZigzagOrder[16] = { 0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15 };

    for (mfxU16 i = 0; i < pow(m_encpakParams.preencDSstrength,2); ++i)
    {
        mfxU32 encMBidx = (MBnum % m_widthMBpreenc + MBnum / m_widthMBpreenc *  m_widthMB) * m_encpakParams.preencDSstrength + i % m_encpakParams.preencDSstrength + m_widthMB*(i / m_encpakParams.preencDSstrength);

        if (encMBidx >= m_numMB)
            continue;

        mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB *EncMVPredMB = &EncMVPred->MB[encMBidx];

        if (0 == predIdx) {
            MSDK_ZERO_MEMORY(*EncMVPredMB);
        }

        EncMVPredMB->RefIdx[predIdx].RefL0 = refIdx[0];
        EncMVPredMB->RefIdx[predIdx].RefL1 = refIdx[1];

        switch (m_encpakParams.preencDSstrength)
        {
        case 2:
            EncMVPredMB->MV[predIdx][0].x = get4Median(preencMVoutMB[0], m_tmpForMedian, 0, 0, i);
            EncMVPredMB->MV[predIdx][0].y = get4Median(preencMVoutMB[0], m_tmpForMedian, 1, 0, i);
            if (preencMVoutMB[1])
            {
                EncMVPredMB->MV[predIdx][1].x = get4Median(preencMVoutMB[1], m_tmpForMedian, 0, 1, i);
                EncMVPredMB->MV[predIdx][1].y = get4Median(preencMVoutMB[1], m_tmpForMedian, 1, 1, i);
            }
            break;
        case 4:
            EncMVPredMB->MV[predIdx][0].x = preencMVoutMB[0]->MV[MVZigzagOrder[i]][0].x;
            EncMVPredMB->MV[predIdx][0].y = preencMVoutMB[0]->MV[MVZigzagOrder[i]][0].y;
            if (preencMVoutMB[1])
            {
                EncMVPredMB->MV[predIdx][1].x = preencMVoutMB[1]->MV[MVZigzagOrder[i]][1].x;
                EncMVPredMB->MV[predIdx][1].y = preencMVoutMB[1]->MV[MVZigzagOrder[i]][1].y;
            }
            break;
        case 8:
            mv_idx = MVZigzagOrder[i % 16 % 8 / 2 + i / 16 * 4];
            EncMVPredMB->MV[predIdx][0].x = preencMVoutMB[0]->MV[mv_idx][0].x;
            EncMVPredMB->MV[predIdx][0].y = preencMVoutMB[0]->MV[mv_idx][0].y;
            if (preencMVoutMB[1])
            {
                EncMVPredMB->MV[predIdx][1].x = preencMVoutMB[1]->MV[mv_idx][1].x;
                EncMVPredMB->MV[predIdx][1].y = preencMVoutMB[1]->MV[mv_idx][1].y;
            }
            break;
        default:
            break;
        }

        EncMVPredMB->MV[predIdx][0].x *= m_encpakParams.preencDSstrength;
        EncMVPredMB->MV[predIdx][0].y *= m_encpakParams.preencDSstrength;
        if (preencMVoutMB[1])
        {
            EncMVPredMB->MV[predIdx][1].x *= m_encpakParams.preencDSstrength;
            EncMVPredMB->MV[predIdx][1].y *= m_encpakParams.preencDSstrength;
        }
    }

    return sts;
}

inline mfxI16 get16Median(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB* preencMB, mfxI16* tmpBuf, int xy, int L0L1)
{
    switch (xy){
    case 0:
        for (int k = 0; k < 16; k++){
            tmpBuf[k] = preencMB->MV[k][L0L1].x;
        }
        break;
    case 1:
        for (int k = 0; k < 16; k++){
            tmpBuf[k] = preencMB->MV[k][L0L1].y;
        }
        break;
    default:
        return 0;
    }

    std::sort(tmpBuf, tmpBuf + 16);
    return (tmpBuf[7] + tmpBuf[8]) / 2;
}

inline mfxI16 get4Median(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB* preencMB, mfxI16* tmpBuf, int xy, int L0L1, int offset)
{
    switch (xy){
    case 0:
        for (int k = 0; k < 4; k++){
            tmpBuf[k] = preencMB->MV[k + offset][L0L1].x;
        }
        break;
    case 1:
        for (int k = 0; k < 4; k++){
            tmpBuf[k] = preencMB->MV[k + offset][L0L1].y;
        }
        break;
    default:
        return 0;
    }

    std::sort(tmpBuf, tmpBuf + 4);
    return (tmpBuf[1] + tmpBuf[2]) / 2;
}

/* ext buffers management */

bufSet* getFreeBufSet(std::list<bufSet*> bufs)
{
    for (std::list<bufSet*>::iterator it = bufs.begin(); it != bufs.end(); ++it){
        if ((*it)->vacant)
        {
            (*it)->vacant = false;
            return (*it);
        }
    }

    return NULL;
}

inline mfxExtBuffer * getBufById(setElem* bufSet, mfxU32 id, mfxU32 fieldId)
{
    MSDK_CHECK_POINTER(bufSet, NULL);
    for (mfxU16 i = 0; i < bufSet->NumExtParam - fieldId; i++)
    {
        if (bufSet->ExtParam[i]->BufferId == id)
        {
            return (bufSet->ExtParam[i + fieldId]->BufferId == id) ? bufSet->ExtParam[i + fieldId] : NULL;
        }
    }

    return NULL;
}

mfxStatus CEncodingPipeline::FreeBuffers(std::list<bufSet*> bufs)
{
    for (std::list<bufSet*>::iterator it = bufs.begin(); it != bufs.end(); ++it)
    {
        MSDK_CHECK_POINTER(*it, MFX_ERR_NULL_PTR);
        (*it)->vacant = false;

        for (int i = 0; i < (*it)->PB_bufs.in.NumExtParam; /*i++*/)
        {
            switch ((*it)->PB_bufs.in.ExtParam[i]->BufferId)
            {
            case MFX_EXTBUFF_FEI_PREENC_CTRL:
            {
                mfxExtFeiPreEncCtrl* preENCCtr = (mfxExtFeiPreEncCtrl*)((*it)->PB_bufs.in.ExtParam[i]);
                MSDK_SAFE_DELETE_ARRAY(preENCCtr);
                i += m_numOfFields;
            }
            break;

            case MFX_EXTBUFF_FEI_PREENC_MV_PRED:
            {
                mfxExtFeiPreEncMVPredictors* mvPreds = (mfxExtFeiPreEncMVPredictors*)((*it)->PB_bufs.in.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(mvPreds[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(mvPreds);
                i += m_numOfFields;
            }
            break;

            case MFX_EXTBUFF_FEI_PREENC_QP:
            {
                mfxExtFeiEncQP* qps = (mfxExtFeiEncQP*)((*it)->PB_bufs.in.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(qps[fieldId].QP);
                }
                MSDK_SAFE_DELETE_ARRAY(qps);
                i += m_numOfFields;
            }
            break;

            case MFX_EXTBUFF_FEI_ENC_CTRL:
            {
                mfxExtFeiEncFrameCtrl* feiEncCtrl = (mfxExtFeiEncFrameCtrl*)((*it)->PB_bufs.in.ExtParam[i]);
                MSDK_SAFE_DELETE_ARRAY(feiEncCtrl);
                i += m_numOfFields;
            }
            break;

            case MFX_EXTBUFF_FEI_SPS:
            {
                mfxExtFeiSPS* feiSPS = (mfxExtFeiSPS*)((*it)->PB_bufs.in.ExtParam[i]);
                MSDK_SAFE_DELETE(feiSPS);
                i++;
            }
            break;

            case MFX_EXTBUFF_FEI_PPS:
            {
                mfxExtFeiPPS* feiPPS = (mfxExtFeiPPS*)((*it)->PB_bufs.in.ExtParam[i]);
                MSDK_SAFE_DELETE_ARRAY(feiPPS);
                i += m_numOfFields;
            }
            break;

            case MFX_EXTBUFF_FEI_SLICE:
            {
                mfxExtFeiSliceHeader* feiSliceHeader = (mfxExtFeiSliceHeader*)((*it)->PB_bufs.in.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(feiSliceHeader[fieldId].Slice);
                }
                MSDK_SAFE_DELETE_ARRAY(feiSliceHeader);
                i += m_numOfFields;
            }
            break;

            case MFX_EXTBUFF_FEI_ENC_MV_PRED:
            {
                mfxExtFeiEncMVPredictors* feiEncMVPredictors = (mfxExtFeiEncMVPredictors*)((*it)->PB_bufs.in.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(feiEncMVPredictors[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(feiEncMVPredictors);
                i += m_numOfFields;
            }
            break;

            case MFX_EXTBUFF_FEI_ENC_MB:
            {
                mfxExtFeiEncMBCtrl* feiEncMBCtrl = (mfxExtFeiEncMBCtrl*)((*it)->PB_bufs.in.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(feiEncMBCtrl[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(feiEncMBCtrl);
                i += m_numOfFields;
            }
            break;

            default:
                i++;
                break;
            } // switch ((*it)->PB_bufs.in.ExtParam[i]->BufferId)
        } // for (int i = 0; i < (*it)->PB_bufs.in.NumExtParam; )

        MSDK_SAFE_DELETE_ARRAY((*it)->PB_bufs.in.ExtParam);
        MSDK_SAFE_DELETE_ARRAY((*it)-> I_bufs.in.ExtParam);

        for (int i = 0; i < (*it)->PB_bufs.out.NumExtParam; /*i++*/)
        {
            switch ((*it)->PB_bufs.out.ExtParam[i]->BufferId)
            {
            case MFX_EXTBUFF_FEI_PREENC_MV:
            {
                mfxExtFeiPreEncMV* mvs = (mfxExtFeiPreEncMV*)((*it)->PB_bufs.out.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(mvs[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(mvs);
                i += m_numOfFields;
            }
            break;

            case MFX_EXTBUFF_FEI_PREENC_MB:
            {
                mfxExtFeiPreEncMBStat* mbdata = (mfxExtFeiPreEncMBStat*)((*it)->PB_bufs.out.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(mbdata[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(mbdata);
                i += m_numOfFields;
            }
            break;

            case MFX_EXTBUFF_FEI_ENC_MB_STAT:
            {
                mfxExtFeiEncMBStat* feiEncMbStat = (mfxExtFeiEncMBStat*)((*it)->PB_bufs.out.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(feiEncMbStat[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(feiEncMbStat);
                i += m_numOfFields;
            }
            break;

            case MFX_EXTBUFF_FEI_ENC_MV:
            {
                mfxExtFeiEncMV* feiEncMV = (mfxExtFeiEncMV*)((*it)->PB_bufs.out.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(feiEncMV[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(feiEncMV);
                i += m_numOfFields;
            }
            break;

            case MFX_EXTBUFF_FEI_PAK_CTRL:
            {
                mfxExtFeiPakMBCtrl* feiEncMBCode = (mfxExtFeiPakMBCtrl*)((*it)->PB_bufs.out.ExtParam[i]);
                for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    MSDK_SAFE_DELETE_ARRAY(feiEncMBCode[fieldId].MB);
                }
                MSDK_SAFE_DELETE_ARRAY(feiEncMBCode);
                i += m_numOfFields;
            }
            break;

            default:
                i++;
                break;
            } // switch ((*it)->PB_bufs.out.ExtParam[i]->BufferId)
        } // for (int i = 0; i < (*it)->PB_bufs.out.NumExtParam; )

        MSDK_SAFE_DELETE_ARRAY((*it)->PB_bufs.out.ExtParam);
        MSDK_SAFE_DELETE_ARRAY((*it)-> I_bufs.out.ExtParam);
        MSDK_SAFE_DELETE(*it);
    } // for (std::list<bufSet*>::iterator it = bufs.begin(); it != bufs.end(); ++it)

    bufs.clear();

    return MFX_ERR_NONE;
}

/* per-frame processing by interfaces*/

mfxStatus CEncodingPipeline::PreencOneFrame(iTask* &eTask, mfxFrameSurface1* pSurf, bool is_buffered, bool &cont)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pSurf, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;
    cont = false;

    pSurf->Data.Locked++;
    eTask->in.InSurface = pSurf;

    if (m_encpakParams.preencDSstrength)
    {
        // PREENC needs to be performed on downscaled surface
        // For simplicity, let's just replace the original surface
        eTask->fullResSurface = eTask->in.InSurface;

        // find/wait for a free output surface
        ExtendedSurface VppExtSurface = { 0 };
        mfxU16 nVPPSurfIdx = GetFreeSurface(m_pDSSurfaces, m_dsResponse.NumFrameActual);
        MSDK_CHECK_ERROR(nVPPSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
        VppExtSurface.pSurface = &m_pDSSurfaces[nVPPSurfIdx];
        MSDK_CHECK_POINTER(VppExtSurface.pSurface, MFX_ERR_MEMORY_ALLOC);

        // make sure picture structure has the initial value
        // surfaces are reused and VPP may change this parameter in certain configurations
        VppExtSurface.pSurface->Info.PicStruct = m_mfxEncParams.mfx.FrameInfo.PicStruct;

        sts = VPPOneFrame(m_pmfxDS, &m_preenc_mfxSession, eTask->fullResSurface, &VppExtSurface);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        eTask->in.InSurface = VppExtSurface.pSurface;
        eTask->in.InSurface->Data.Locked++;
    }

    eTask = ConfigureTask(eTask, is_buffered);
    if (eTask == NULL){ //not found frame to encode
        cont = true;
        return sts;
    }

    sts = ProcessMultiPreenc(eTask, m_numOfRefs);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Pass MVP to encode,encpak
    if (m_encpakParams.bENCODE || m_encpakParams.bENCPAK || m_encpakParams.bOnlyENC)
    {
        m_ctr->FrameType = eTask->m_fieldPicFlag ? createType(*eTask) : ExtractFrameType(*eTask);

        if (m_isField || !(ExtractFrameType(*eTask) & MFX_FRAMETYPE_I)){
            //repack MV predictors
            if (!m_encpakParams.bPerfMode)
                sts = PassPreEncMVPred2EncEx(eTask, m_numOfRefs);
            else
                sts = PassPreEncMVPred2EncExPerf(eTask, m_numOfRefs);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    if (!m_twoEncoders)
    {
        sts = UpdateTaskQueue(eTask);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    return sts;
}

mfxStatus CEncodingPipeline::ProcessMultiPreenc(iTask* eTask, mfxU16 num_of_refs[2][2])
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;
    MSDK_ZERO_ARRAY(num_of_refs[0], 2);
    MSDK_ZERO_ARRAY(num_of_refs[1], 2);

    // max possible candidates to L0 / L1
    mfxU32 total_l0 = (ExtractFrameType(*eTask, m_isField) & MFX_FRAMETYPE_P) ? ((ExtractFrameType(*eTask) & MFX_FRAMETYPE_IDR) ? 1 : eTask->NumRefActiveP) : ((ExtractFrameType(*eTask) & MFX_FRAMETYPE_I) ? 1 : eTask->NumRefActiveBL0);
    mfxU32 total_l1 = (ExtractFrameType(*eTask) & MFX_FRAMETYPE_B) ? eTask->NumRefActiveBL1 : 1; // just one iteration here for non-B

    mfxU32 adj_l0 = IPP_MAX(1, m_encpakParams.bNPredSpecified_l0 ? m_encpakParams.NumMVPredictors[0] : m_numOfFields*m_mfxEncParams.mfx.NumRefFrame);
    mfxU32 adj_l1 = IPP_MAX(1, m_encpakParams.bNPredSpecified_l1 ? m_encpakParams.NumMVPredictors[1] : m_numOfFields*m_mfxEncParams.mfx.NumRefFrame);

    total_l0 = IPP_MIN(total_l0, adj_l0); // adjust to
    total_l1 = IPP_MIN(total_l1, adj_l1); // user input

    int preenc_ref_idx[2][2]; // indexes means [fieldId][L0L1]
    int ref_fid[2][2];
    iTask* refTask[2][2];
    bool isDownsamplingNeeded = true;

    for (mfxU32 l0_idx = 0, l1_idx = 0; l0_idx < total_l0 || l1_idx < total_l1; l0_idx++, l1_idx++)
    {
        sts = GetRefTaskEx(eTask, l0_idx, l1_idx, preenc_ref_idx, ref_fid, refTask);

        if (ExtractFrameType(*eTask) & MFX_FRAMETYPE_I)
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_OUT_OF_RANGE);

        if (sts == MFX_WRN_OUT_OF_RANGE){ // some of the indexes exceeds DPB
            sts = MFX_ERR_NONE;
            continue;
        }
        MSDK_BREAK_ON_ERROR(sts);

        for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
        {
            if (refTask[fieldId][0]) { num_of_refs[fieldId][0]++; }
            if (refTask[fieldId][1]) { num_of_refs[fieldId][1]++; }
        }

        sts = InitPreEncFrameParamsEx(eTask, refTask, ref_fid, isDownsamplingNeeded);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // If input surface is not being changed between PreENC calls
        // (including calls for different fields of the same field pair in Single Field processing mode),
        // an application can avoid redundant downsampling on driver side.
        isDownsamplingNeeded = false;

        // Doing PreEnc
        for (;;)
        {
            sts = m_pmfxPREENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
            MSDK_BREAK_ON_ERROR(sts);
            /*PRE-ENC is running in separate session */
            sts = m_preenc_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
            MSDK_BREAK_ON_ERROR(sts);
            mdprintf(stderr, "preenc synced : %d\n", sts);
            if (m_encpakParams.bFieldProcessingMode)
            {
                sts = m_pmfxPREENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                MSDK_BREAK_ON_ERROR(sts);
                /*PRE-ENC is running in separate session */
                sts = m_preenc_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                MSDK_BREAK_ON_ERROR(sts);
                mdprintf(stderr, "preenc synced : %d\n", sts);
            }

            if (MFX_ERR_NONE < sts && !eTask->EncSyncP) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts)
                {
                    WaitForDeviceToBecomeFree(m_preenc_mfxSession, eTask->EncSyncP, sts);
                }
            }
            else if (MFX_ERR_NONE < sts && eTask->EncSyncP) {
                sts = MFX_ERR_NONE; // ignore warnings if output is available
                break;
            }
            else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
                //                sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                //                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            else {
                // get next surface and new task for 2nd bitstream in ViewOutput mode
                MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                break;
            }
        }

        // Store PreEnc output
        PreEncMVPInfo result = {};
        result.preenc_output_bufs = eTask->preenc_bufs;

        for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
        {
            memcpy(result.refIdx[fieldId], preenc_ref_idx[fieldId], 2 * sizeof(mfxU32));
        }
        eTask->preenc_mvp_info.push_back(result);

        //drop output data to output file
        sts = DropPREENCoutput(eTask);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    } // for (mfxU32 l0_idx = 0, l1_idx = 0; l0_idx < total_l0 || l1_idx < total_l1; l0_idx++, l1_idx++)

    return sts;
}

/* PREENC functions */

mfxStatus CEncodingPipeline::GetRefTaskEx(iTask *eTask, mfxU32 l0_idx, mfxU32 l1_idx, int refIdx[2][2], int ref_fid[2][2], iTask *outRefTask[2][2])
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxStatus stsL0 = MFX_WRN_OUT_OF_RANGE, stsL1 = MFX_WRN_OUT_OF_RANGE;

    for (int i = 0; i < 2; i++){
        MSDK_ZERO_ARRAY(refIdx[i],     2);
        MSDK_ZERO_ARRAY(outRefTask[i], 2);
        MSDK_ZERO_ARRAY(ref_fid[i],    2);
    }

    for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
    {
        mfxU8 type = ExtractFrameType(*eTask, fieldId);
        mfxU32 l0_ref_count = GetNBackward(eTask, fieldId),
               l1_ref_count = GetNForward(eTask, fieldId),
               fid = eTask->m_fid[fieldId];

        /* adjustment for case of equal l0 and l1 lists*/
        if (!l0_ref_count && eTask->m_list0[fid].Size() && !(eTask->m_type[fid] & MFX_FRAMETYPE_I))
        {
            l0_ref_count = eTask->m_list0[fid].Size();
        }

        if (l0_idx < l0_ref_count && eTask->m_list0[fid].Size())
        {
            mfxU8 const * l0_instance = eTask->m_list0[fid].Begin() + l0_idx;
            iTask *L0_ref_task = GetTaskByFrameOrder(eTask->m_dpb[fid][*l0_instance & 127].m_frameOrder);
            MSDK_CHECK_POINTER(L0_ref_task, MFX_ERR_NULL_PTR);

            refIdx[fieldId][0]     = l0_idx;
            outRefTask[fieldId][0] = L0_ref_task;
            ref_fid[fieldId][0]    = (*l0_instance > 127) ? 1 : 0; // for bff ? 0 : 1; (but now preenc supports only tff)

            stsL0 = MFX_ERR_NONE;
        } else {
            if (eTask->m_list0[fid].Size())
                stsL0 = MFX_WRN_OUT_OF_RANGE;
        }

        if (l1_idx < l1_ref_count && eTask->m_list1[fid].Size())
        {
            if (type & MFX_FRAMETYPE_IP)
            {
                //refIdx[fieldId][1] = 0;        // already zero
                //outRefTask[fieldId][1] = NULL; // No forward ref for P
            }
            else
            {
                mfxU8 const *l1_instance = eTask->m_list1[fid].Begin() + l1_idx;
                iTask *L1_ref_task = GetTaskByFrameOrder(eTask->m_dpb[fid][*l1_instance & 127].m_frameOrder);
                MSDK_CHECK_POINTER(L1_ref_task, MFX_ERR_NULL_PTR);

                refIdx[fieldId][1]     = l1_idx;
                outRefTask[fieldId][1] = L1_ref_task;
                ref_fid[fieldId][1]    = (*l1_instance > 127) ? 1 : 0; // for bff ? 0 : 1; (but now preenc supports only tff)

                stsL1 = MFX_ERR_NONE;
            }
        } else {
            if (eTask->m_list1[fid].Size())
                stsL1 = MFX_WRN_OUT_OF_RANGE;
        }
    }

    return stsL1 != stsL0 ? MFX_ERR_NONE : stsL0;
}

mfxStatus CEncodingPipeline::GetBestSetByDistortion(std::list<PreEncMVPInfo>& preenc_mvp_info,
    mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB ** bestDistortionPredMBext[MaxFeiEncMVPNum], mfxU32 * refIdx[MaxFeiEncMVPNum],
    mfxU32 nPred[2], mfxU32 fieldId, mfxU32 MB_idx)
{
    mfxStatus sts = MFX_ERR_NONE;
    for (mfxU32 i = 0; i < IPP_MAX(nPred[0],nPred[1]); i++)
    {
        MSDK_CHECK_POINTER(bestDistortionPredMBext[i], MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_POINTER(refIdx[i],                  MFX_ERR_NOT_INITIALIZED);
    }

    std::list<std::pair<std::pair<mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *, mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB *>, mfxU32*> > curMBSet;

    mfxExtFeiPreEncMV*     mvs    = NULL;
    mfxExtFeiPreEncMBStat* mbdata = NULL;

    for (std::list<PreEncMVPInfo>::iterator it = preenc_mvp_info.begin(); it != preenc_mvp_info.end(); ++it)
    {
        mvs = (mfxExtFeiPreEncMV*)getBufById(&(*it).preenc_output_bufs->PB_bufs.out, MFX_EXTBUFF_FEI_PREENC_MV, fieldId);
        MSDK_CHECK_POINTER(mvs, MFX_ERR_NULL_PTR);

        mbdata = (mfxExtFeiPreEncMBStat*)getBufById(&(*it).preenc_output_bufs->PB_bufs.out, MFX_EXTBUFF_FEI_PREENC_MB, fieldId);
        MSDK_CHECK_POINTER(mbdata, MFX_ERR_NULL_PTR);

        curMBSet.push_back(std::pair<std::pair<mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *, mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB *>, mfxU32*>
            (std::pair<mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *, mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB *>(&mvs->MB[MB_idx], &mbdata->MB[MB_idx]), (*it).refIdx[fieldId]));
    }

    curMBSet.sort(compareL0Distortion); // find and copy nPred[0] best L0
    mfxU32 i = 0;
    for (std::list<std::pair<std::pair<mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *, mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB *>, mfxU32*> >::iterator it = curMBSet.begin();
        it != curMBSet.end() && i<nPred[0]; ++it)
    {
        bestDistortionPredMBext[i][0]  = (*it).first.first;
        refIdx[i++][0] = (*it).second[0];
    }

    curMBSet.sort(compareL1Distortion); // find and copy nPred[1] best L1
    i = 0;
    for (std::list<std::pair<std::pair<mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *, mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB *>, mfxU32*> >::iterator it = curMBSet.begin();
        it != curMBSet.end() && i<nPred[1]; ++it)
    {
        bestDistortionPredMBext[i][1]  = (*it).first.first;
        refIdx[i++][1] = (*it).second[1];
    }

    return sts;
}

bool compareL0Distortion(std::pair<std::pair<mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *, mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB *>, mfxU32*> frst,
    std::pair<std::pair<mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *, mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB *>, mfxU32*> scnd)
{
    return frst.first.second->Inter[0].BestDistortion < scnd.first.second->Inter[0].BestDistortion;
}

bool compareL1Distortion(std::pair<std::pair<mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *, mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB *>, mfxU32*> frst,
    std::pair<std::pair<mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB *, mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB *>, mfxU32*> scnd)
{
    return frst.first.second->Inter[1].BestDistortion < scnd.first.second->Inter[1].BestDistortion;
}

/* end of PREENC functions */

mfxStatus CEncodingPipeline::EncPakOneFrame(iTask* &eTask, mfxFrameSurface1* pSurf, sTask* pCurrentTask, bool is_buffered, bool &cont)
{
    MSDK_CHECK_POINTER(eTask,        MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pSurf,        MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pCurrentTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    pSurf->Data.Locked++;
    eTask->in.InSurface = eTask->inPAK.InSurface = pSurf;

//    /* this is Recon surface which will be generated by FEI PAK*/
//    mfxFrameSurface1 *pSurfPool = m_pReconSurfaces;
//    mfxU16 poolSize = m_ReconResponse.NumFrameActual;
//    if (m_pmfxDECODE && !m_pmfxVPP)
//    {
//        pSurfPool = m_pDecSurfaces;
//        poolSize  = m_DecResponse.NumFrameActual;
//    }
//    mfxU16 nReconSurfIdx = GetFreeSurface(pSurfPool, poolSize);
//    MSDK_CHECK_ERROR(nReconSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
//    mfxFrameSurface1* pReconSurf = &pSurfPool[nReconSurfIdx];
    mfxFrameSurface1* pReconSurf = pSurf;
    pReconSurf->Data.Locked++;
    eTask->out.OutSurface = eTask->outPAK.OutSurface = pReconSurf;
    eTask->outPAK.Bs = &pCurrentTask->mfxBS;

    if (!m_encpakParams.bPREENC) // in case of preenc + enc (+ pak) pipeline eTask already cofigurated by preenc
    {
        eTask = ConfigureTask(eTask, is_buffered);
        if (eTask == NULL){ //not found frame to encode
            cont = true;
            return sts;
        }
    }

    sts = InitEncPakFrameParams(eTask);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    for (;;)
    {
        mdprintf(stderr, "frame: %d  t:%d %d : submit ", eTask->m_frameOrder, eTask->m_type[m_ffid], eTask->m_type[m_sfid]);

        if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC))
        {
            sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
            MSDK_BREAK_ON_ERROR(sts);
            sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
            MSDK_BREAK_ON_ERROR(sts);
            mdprintf(stderr, "synced : %d\n", sts);
        }

        /* In case of PAK only we need to read data from file */
        if (m_encpakParams.bOnlyPAK)
        {
            sts = ReadPAKdata(eTask);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK))
        {
            sts = m_pmfxPAK->ProcessFrameAsync(&eTask->inPAK, &eTask->outPAK, &eTask->EncSyncP);
            MSDK_BREAK_ON_ERROR(sts);
            sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
            MSDK_BREAK_ON_ERROR(sts);
            mdprintf(stderr, "synced : %d\n", sts);
        }

        if (m_encpakParams.bFieldProcessingMode)
        {
            if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC))
            {
                sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                MSDK_BREAK_ON_ERROR(sts);
                sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                MSDK_BREAK_ON_ERROR(sts);
                mdprintf(stderr, "synced : %d\n", sts);
            }
            if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK))
            {
                sts = m_pmfxPAK->ProcessFrameAsync(&eTask->inPAK, &eTask->outPAK, &eTask->EncSyncP);
                MSDK_BREAK_ON_ERROR(sts);
                sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                MSDK_BREAK_ON_ERROR(sts);
                mdprintf(stderr, "synced : %d\n", sts);
            }
        }

        if (MFX_ERR_NONE < sts && !eTask->EncSyncP) // repeat the call if warning and no output
        {
            if (MFX_WRN_DEVICE_BUSY == sts){
                WaitForDeviceToBecomeFree(m_mfxSession, eTask->EncSyncP, sts);
            }
        }
        else if (MFX_ERR_NONE < sts && eTask->EncSyncP) {
            sts = MFX_ERR_NONE; // ignore warnings if output is available
            break;
        }
        else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
            sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        else {
            // get next surface and new task for 2nd bitstream in ViewOutput mode
            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
            break;
        }
    }

    //drop output data to output file
    if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyPAK)){
        sts = pCurrentTask->WriteBitstream();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if ((m_encpakParams.bENCPAK) || (m_encpakParams.bOnlyENC))
    {
        sts = DropENCPAKoutput(eTask);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    sts = UpdateTaskQueue(eTask);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

mfxStatus CEncodingPipeline::EncodeOneFrame(iTask* &eTask, mfxFrameSurface1* pSurf, sTask* pCurrentTask, bool is_buffered, bool &cont)
{
    if (!is_buffered){
        MSDK_CHECK_POINTER(pSurf,    MFX_ERR_NULL_PTR);
    }
    MSDK_CHECK_POINTER(pCurrentTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;
    bool create_task = eTask != NULL;
    mfxU32 fieldProcessingCounter = 0;
    cont = false;

    if (create_task && !m_encpakParams.bPREENC)
    {
        eTask->in.InSurface = pSurf;
        eTask->in.InSurface->Data.Locked++;
        eTask = ConfigureTask(eTask, is_buffered); // reorder frames in case of encoded frame order
        if (eTask == NULL){ //not found frame to encode
            cont = true;
            return sts;
        }

        m_ctr->FrameType = eTask->m_fieldPicFlag ? createType(*eTask) : ExtractFrameType(*eTask);
    }

    mfxFrameSurface1* encodeSurface = create_task ? (m_encpakParams.preencDSstrength ? eTask->fullResSurface : eTask->in.InSurface) : pSurf;
    PairU8 frameType = create_task ? eTask->m_type : m_frameType;
    if (m_mfxEncParams.mfx.EncodedOrder || !m_mfxEncParams.mfx.EncodedOrder && !is_buffered) // no need to do this for buffered frames in display order
    {
        sts = InitEncodeFrameParams(encodeSurface, pCurrentTask, frameType, is_buffered);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    for (;;) {
        // at this point surface for encoder contains either a frame from file or a frame processed by vpp

        MSDK_ZERO_MEMORY(pCurrentTask->EncSyncP);
        sts = m_pmfxENCODE->EncodeFrameAsync(m_ctr, encodeSurface, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

        fieldProcessingCounter++;

        if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
        {
            if (MFX_WRN_DEVICE_BUSY == sts){
                WaitForDeviceToBecomeFree(m_mfxSession, pCurrentTask->EncSyncP, sts);
            }
        }
        else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP)
        {
            sts = MFX_ERR_NONE; // ignore warnings if output is available
            sts = SyncOneEncodeFrame(pCurrentTask, eTask, fieldProcessingCounter);
            MSDK_BREAK_ON_ERROR(sts);

            if ((MFX_CODINGOPTION_ON == m_encpakInit.SingleFieldProcessing) &&
                (1 == fieldProcessingCounter)){
                continue;
            }
            break;
        }
        else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
            sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        else {
            // get next surface and new task for 2nd bitstream in ViewOutput mode
            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
            if (!is_buffered){
                MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA); // correct status to finish encoding of buffered frames
            }
            if (pCurrentTask->EncSyncP)
            {
                sts = SyncOneEncodeFrame(pCurrentTask, eTask, fieldProcessingCounter);
                MSDK_BREAK_ON_ERROR(sts);

                if ((MFX_CODINGOPTION_ON == m_encpakInit.SingleFieldProcessing) &&
                    (1 == fieldProcessingCounter)){
                    continue;
                }
            }
            break;
        }
    }

    return sts;
}

mfxStatus CEncodingPipeline::SyncOneEncodeFrame(sTask* pCurrentTask, iTask* eTask, mfxU32 fieldProcessingCounter)
{
    MSDK_CHECK_POINTER(pCurrentTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = m_mfxSession.SyncOperation(pCurrentTask->EncSyncP, MSDK_WAIT_INTERVAL);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (MFX_CODINGOPTION_ON == m_encpakInit.SingleFieldProcessing)
    {
        sts = m_TaskPool.SetFieldToStore(fieldProcessingCounter);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        if (fieldProcessingCounter == 1)
        {
            sts = SynchronizeFirstTask();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            /* second field coding */
            return sts;
        }
    }

    if (eTask != NULL)
    {
        sts = UpdateTaskQueue(eTask);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    sts = SynchronizeFirstTask();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

mfxStatus CEncodingPipeline::DecodeOneFrame(ExtendedSurface *pExtSurface)
{
    MSDK_CHECK_POINTER(pExtSurface, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_MORE_SURFACE;
    mfxFrameSurface1 *pDecSurf = NULL;
    pExtSurface->pSurface      = NULL;

    while (MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE < sts)
    {
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            WaitForDeviceToBecomeFree(m_decode_mfxSession,m_LastDecSyncPoint,sts);
        }
        else if (MFX_ERR_MORE_DATA == sts)
        {
            sts = m_BSReader.ReadNextFrame(&m_mfxBS); // read more data to input bit stream
            MSDK_BREAK_ON_ERROR(sts);
        }
        else if (MFX_ERR_MORE_SURFACE == sts)
        {
            // find free surface for decoder input
            mfxU16 nDecSurfIdx = GetFreeSurface(m_pDecSurfaces, m_DecResponse.NumFrameActual);
            MSDK_CHECK_ERROR(nDecSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
            pDecSurf = &m_pDecSurfaces[nDecSurfIdx];

            MSDK_CHECK_POINTER(pDecSurf, MFX_ERR_MEMORY_ALLOC); // return an error if a free surface wasn't found
        }

        sts = m_pmfxDECODE->DecodeFrameAsync(&m_mfxBS, pDecSurf, &pExtSurface->pSurface, &pExtSurface->Syncp);

        if (!sts)
        {
            m_LastDecSyncPoint = pExtSurface->Syncp;
        }
        // ignore warnings if output is available,
        if (MFX_ERR_NONE < sts && pExtSurface->Syncp)
        {
            sts = MFX_ERR_NONE;
        }

    } //while processing

    return sts;
}

mfxStatus CEncodingPipeline::DecodeLastFrame(ExtendedSurface *pExtSurface)
{
    MSDK_CHECK_POINTER(pExtSurface, MFX_ERR_NULL_PTR);

    mfxFrameSurface1 *pDecSurf = NULL;
    mfxStatus sts = MFX_ERR_MORE_SURFACE;
    pExtSurface->pSurface = NULL;

    // retrieve the buffered decoded frames
    while (MFX_ERR_MORE_SURFACE == sts || MFX_WRN_DEVICE_BUSY == sts)
    {
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            WaitForDeviceToBecomeFree(m_decode_mfxSession,m_LastDecSyncPoint,sts);
        }
        // find free surface for decoder input
        mfxU16 nDecSurfIdx = GetFreeSurface(m_pDecSurfaces, m_DecResponse.NumFrameActual);
        MSDK_CHECK_ERROR(nDecSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
        pDecSurf = &m_pDecSurfaces[nDecSurfIdx];

        MSDK_CHECK_POINTER(pDecSurf, MFX_ERR_MEMORY_ALLOC); // return an error if a free surface wasn't found

        sts = m_pmfxDECODE->DecodeFrameAsync(NULL, pDecSurf, &pExtSurface->pSurface, &pExtSurface->Syncp);
    }
    return sts;
}

mfxStatus CEncodingPipeline::PreProcessOneFrame(mfxFrameSurface1* & pSurf)
{
    MSDK_CHECK_POINTER(pSurf, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;
    ExtendedSurface VppExtSurface = { 0 };

    // find/wait for a free working surface
    mfxU16 nVPPSurfIdx = GetFreeSurface(m_pEncSurfaces, m_EncResponse.NumFrameActual);
    MSDK_CHECK_ERROR(nVPPSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);
    VppExtSurface.pSurface = &m_pEncSurfaces[nVPPSurfIdx];
    MSDK_CHECK_POINTER(VppExtSurface.pSurface, MFX_ERR_MEMORY_ALLOC);

    // make sure picture structure has the initial value
    // surfaces are reused and VPP may change this parameter in certain configurations
    VppExtSurface.pSurface->Info.PicStruct = m_mfxEncParams.mfx.FrameInfo.PicStruct;

    if (m_bNeedDRC)
    {
        VppExtSurface.pSurface->Info.Width = m_mfxEncParams.mfx.FrameInfo.Width;
        VppExtSurface.pSurface->Info.Height = m_mfxEncParams.mfx.FrameInfo.Height;
        VppExtSurface.pSurface->Info.CropW = m_mfxEncParams.mfx.FrameInfo.CropW;
        VppExtSurface.pSurface->Info.CropH = m_mfxEncParams.mfx.FrameInfo.CropH;
        VppExtSurface.pSurface->Info.CropX = 0;
        VppExtSurface.pSurface->Info.CropY = 0;
    }

    sts = VPPOneFrame(m_pmfxVPP, m_pVPP_mfxSession, pSurf, &VppExtSurface);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    pSurf = VppExtSurface.pSurface;

    return sts;
}

mfxStatus CEncodingPipeline::VPPOneFrame(MFXVideoVPP* VPPobj, MFXVideoSession* session, mfxFrameSurface1 *pSurfaceIn, ExtendedSurface *pExtSurface)
{
    MSDK_CHECK_POINTER(pExtSurface, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;

    for (;;)
    {
        sts = VPPobj->RunFrameVPPAsync(pSurfaceIn, pExtSurface->pSurface, NULL, &pExtSurface->Syncp);

        if (!pExtSurface->Syncp)
        {
            if (MFX_WRN_DEVICE_BUSY == sts){
                WaitForDeviceToBecomeFree(*session, pExtSurface->Syncp, sts);
            }
            else
                return sts;
        }
        break;
    }

    for (;;)
    {
        sts = session->SyncOperation(pExtSurface->Syncp, MSDK_WAIT_INTERVAL);

        if (!pExtSurface->Syncp)
        {
            if (MFX_WRN_DEVICE_BUSY == sts){
                WaitForDeviceToBecomeFree(*session, pExtSurface->Syncp, sts);
            }
            else
                return sts;
        }
        else if (MFX_ERR_NONE <= sts) {
            sts = MFX_ERR_NONE; // ignore warnings if output is available
            break;
        }
    }

    return sts;
}

mfxStatus CEncodingPipeline::ResizeFrame(mfxU32 m_frameCount, bool &m_insertIDR, size_t &rctime, iTask* &eTask, sTask *pCurrentTask)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 RCStart     = 0;
    mfxU16 tmpRCWidth  = 0;
    mfxU16 tmpRCHeight = 0;
    bDRCReset     = false;

    if (m_drcStart.size() - rctime > 0)
    {
        RCStart = m_drcStart[rctime];
        if (RCStart == m_frameCount )
        {
            tmpRCWidth = m_drcWidth[rctime];
            tmpRCHeight = m_drcHeight[rctime];
            bDRCReset = true;
            rctime++;
        }
    }
    if (bDRCReset)
    {
        if (m_refDist > 1 && m_frameCount > 0 )
        {
            sts = GetFreeTask(&pCurrentTask);
            while (MFX_ERR_NONE <= sts)
            {
                bool need_to_continue = false;
                sts = EncodeOneFrame(eTask, NULL, pCurrentTask, true, need_to_continue);
                if (need_to_continue)
                    continue;
            }

            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);

            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            while (MFX_ERR_NONE == sts) {
                sts = m_TaskPool.SynchronizeFirstTask();
            }
            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_FOUND);

            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        }
       //bDRCReset = false;
        m_mfxEncParams.mfx.FrameInfo.Width = MSDK_ALIGN16(tmpRCWidth);
        m_mfxEncParams.mfx.FrameInfo.Height = m_isField ? MSDK_ALIGN32(tmpRCHeight) : MSDK_ALIGN16(tmpRCHeight);
        m_mfxEncParams.mfx.FrameInfo.CropW = tmpRCWidth;
        m_mfxEncParams.mfx.FrameInfo.CropH = tmpRCHeight;
        m_mfxEncParams.mfx.FrameInfo.CropX = 0;
        m_mfxEncParams.mfx.FrameInfo.CropY = 0;

        sts = m_pmfxENCODE->Reset(&m_mfxEncParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        //VPP itself ignores crops at initialization,so just re-initilize Width,Height.
        m_mfxVppParams.vpp.Out.Width = m_mfxEncParams.mfx.FrameInfo.Width;
        m_mfxVppParams.vpp.Out.Height = m_mfxEncParams.mfx.FrameInfo.Height;

        sts = m_pmfxVPP->Reset(&m_mfxVppParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        m_insertIDR = true;

        numMBs = (m_mfxVppParams.vpp.Out.Height * m_mfxVppParams.vpp.Out.Width) >> 8;
        numMBs /= (mfxU16)m_numOfFields;

        sts = UpdateVideoParams();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    return sts;
}

/* Info printing */

const char* getFrameType(mfxU8 type)
{
    switch (type & MFX_FRAMETYPE_IPB) {
        case MFX_FRAMETYPE_I:
            if (type & MFX_FRAMETYPE_IDR) {
                return "IDR";
            } else {
                return "I";
            }
        case MFX_FRAMETYPE_P:
            return "P";
        case MFX_FRAMETYPE_B:
            if (type & MFX_FRAMETYPE_REF) {
                return "B-ref";
            } else {
                return "B";
            }
        default:
            return "?";
    }
}

void CEncodingPipeline::ShowDpbInfo(iTask *task, int frame_order)
{
    mdprintf(stderr, "\n\n--------------Show DPB Info of frame %d-------\n", frame_order);
    for (mfxU32 j = 0; j < m_numOfFields; j++) {
        mdprintf(stderr, "\t[%d]: List dpb frame of frame %d in (frame_order, frame_num, POC):\n\t\tDPB:", j, task->m_frameOrder);
        for (mfxU32 i = 0; i < task->m_dpb[task->m_fid[j]].Size(); i++) {
            DpbFrame & ref = task->m_dpb[task->m_fid[j]][i];
            mdprintf(stderr, "(%d, %d, %d) ", ref.m_frameOrder, ref.m_frameNum, ref.m_poc[j]);
        }
        mdprintf(stderr, "\n\t\tL0: ");
        for (mfxU32 i = 0; i < task->m_list0[task->m_fid[j]].Size(); i++) {
            mdprintf(stderr, "%d ", task->m_list0[task->m_fid[j]][i]);
        }
        mdprintf(stderr, "\n\t\tL1: ");
        for (mfxU32 i = 0; i < task->m_list1[task->m_fid[j]].Size(); i++) {
            mdprintf(stderr, "%d ", task->m_list1[task->m_fid[j]][i]);
        }
        //mdprintf(stderr, "\n\t\tm_dpbPostEncoding: ");
        //for (mfxU32 i = 0; i < task->m_dpbPostEncoding.Size(); i++) {
        //    DpbFrame & ref = task->m_dpbPostEncoding[i];
        //    mdprintf(stderr, "(%d, %d, %d) ", ref.m_frameOrder, ref.m_frameNum, ref.m_poc[j]);
        //}
        mdprintf(stderr, "\n-------------------------------------------\n");
    }
}

void CEncodingPipeline::PrintInfo()
{
    msdk_printf(MSDK_STRING("Intel(R) Media SDK Encoding Sample Version %s\n"), MSDK_SAMPLE_VERSION);
    if (!m_pmfxDECODE)
        msdk_printf(MSDK_STRING("\nInput file format\t%s\n"), ColorFormatToStr(m_FileReader.m_ColorFormat));
    else
        msdk_printf(MSDK_STRING("\nInput  video: %s\n"), CodecIdToStr(m_mfxDecParams.mfx.CodecId).c_str());
    msdk_printf(MSDK_STRING("Output video\t\t%s\n"), CodecIdToStr(m_mfxEncParams.mfx.CodecId).c_str());

    mfxFrameInfo SrcPicInfo = (m_pmfxVPP || m_pmfxDS) ? m_mfxVppParams.vpp.In : m_mfxEncParams.vpp.In;
    mfxFrameInfo DstPicInfo = m_mfxEncParams.mfx.FrameInfo;

    msdk_printf(MSDK_STRING("\nSource picture:\n"));
    msdk_printf(MSDK_STRING("\tResolution\t%dx%d\n"), SrcPicInfo.Width, SrcPicInfo.Height);
    msdk_printf(MSDK_STRING("\tCrop X,Y,W,H\t%d,%d,%d,%d\n"), SrcPicInfo.CropX, SrcPicInfo.CropY, SrcPicInfo.CropW, SrcPicInfo.CropH);

    msdk_printf(MSDK_STRING("\nDestination picture:\n"));
    msdk_printf(MSDK_STRING("\tResolution\t%dx%d\n"), DstPicInfo.Width, DstPicInfo.Height);
    msdk_printf(MSDK_STRING("\tCrop X,Y,W,H\t%d,%d,%d,%d\n"), DstPicInfo.CropX, DstPicInfo.CropY, DstPicInfo.CropW, DstPicInfo.CropH);

    msdk_printf(MSDK_STRING("\nFrame rate\t\t%.2f\n"), DstPicInfo.FrameRateExtN * 1.0 / DstPicInfo.FrameRateExtD);

    const msdk_char* sMemType = m_memType == D3D9_MEMORY ? MSDK_STRING("d3d9")
        : (m_memType == D3D11_MEMORY ? MSDK_STRING("d3d11")
        : MSDK_STRING("system"));
    msdk_printf(MSDK_STRING("Memory type\t\t%s\n"), sMemType);

    mfxIMPL impl;
    m_mfxSession.QueryIMPL(&impl);

    const msdk_char* sImpl = (MFX_IMPL_VIA_D3D11 == MFX_IMPL_VIA_MASK(impl)) ? MSDK_STRING("hw_d3d11")
        : (MFX_IMPL_HARDWARE & impl) ? MSDK_STRING("hw")
        : MSDK_STRING("sw");
    msdk_printf(MSDK_STRING("Media SDK impl\t\t%s\n"), sImpl);

    mfxVersion ver;
    m_mfxSession.QueryVersion(&ver);
    msdk_printf(MSDK_STRING("Media SDK version\t%d.%d\n"), ver.Major, ver.Minor);

    msdk_printf(MSDK_STRING("\n"));
}
