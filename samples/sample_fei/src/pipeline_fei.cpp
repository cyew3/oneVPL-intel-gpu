/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "pipeline_fei.h"

mfxU8 GetDefaultPicOrderCount(mfxVideoParam const & par)
{
    if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE || par.mfx.GopRefDist > 1)
        return 0;

    return 2;
}

static FILE* mbstatout       = NULL;
static FILE* mvout           = NULL;
static FILE* mvENCPAKout     = NULL;
static FILE* mbcodeout       = NULL;
static FILE* pMvPred         = NULL;
static FILE* pEncMBs         = NULL;
static FILE* pPerMbQP        = NULL;
static FILE* decodeStreamout = NULL;
static FILE* pRepackCtrl     = NULL;

CEncTaskPool::CEncTaskPool():
    m_pTasks(NULL),
    m_nPoolSize(0),
    m_nTaskBufferStart(0),
    m_nFieldId(NOT_IN_SINGLE_FIELD_MODE),
    m_pmfxSession(NULL)
{
}

CEncTaskPool::~CEncTaskPool()
{
    Close();
}

mfxStatus CEncTaskPool::Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize)
{
    MSDK_CHECK_POINTER(pmfxSession, MFX_ERR_NULL_PTR);

    MSDK_CHECK_ERROR(nPoolSize,   0, MFX_ERR_UNDEFINED_BEHAVIOR);
    MSDK_CHECK_ERROR(nBufferSize, 0, MFX_ERR_UNDEFINED_BEHAVIOR);

    m_pmfxSession = pmfxSession;
    m_nPoolSize   = nPoolSize;

    m_pTasks = new sTask [m_nPoolSize];
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_MEMORY_ALLOC);

    mfxStatus sts = MFX_ERR_NONE;

        for (mfxU32 i = 0; i < m_nPoolSize; i++)
        {
            sts = m_pTasks[i].Init(nBufferSize, pWriter);
            MSDK_CHECK_STATUS(sts, "m_pTasks[i].Init failed");
        }

    return MFX_ERR_NONE;
}

mfxStatus CEncTaskPool::SynchronizeFirstTask()
{
    MSDK_CHECK_POINTER(m_pTasks,      MFX_ERR_NOT_INITIALIZED);
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
                    MSDK_CHECK_STATUS(sts, "DropENCODEoutput failed");
                }
            }
            else{
                sts = DropENCODEoutput(bs, m_nFieldId);
                MSDK_CHECK_STATUS(sts, "DropENCODEoutput failed");
            }

            sts = m_pTasks[m_nTaskBufferStart].WriteBitstream();
            MSDK_CHECK_STATUS(sts, "m_pTasks[m_nTaskBufferStart].WriteBitstream failed");

            sts = m_pTasks[m_nTaskBufferStart].Reset();
            MSDK_CHECK_STATUS(sts, "m_pTasks[m_nTaskBufferStart].Reset failed");

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
    MSDK_CHECK_STATUS(sts, "Reset failed");

    sts = InitMfxBitstream(&mfxBS, nBufferSize);
    MSDK_CHECK_STATUS_SAFE(sts, "InitMfxBitstream failed", WipeMfxBitstream(&mfxBS));

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

mfxStatus sTask::WriteBitstream(CSmplBitstreamWriter *pWriter, mfxBitstream* mfxBS)
{
    MSDK_CHECK_POINTER(pWriter, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(mfxBS,   MFX_ERR_NOT_INITIALIZED);

    return pWriter->WriteNextFrame(mfxBS);
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

mfxStatus CEncodingPipeline::InitMfxEncParams(AppConfig *pConfig)
{
    MSDK_CHECK_POINTER(pConfig, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;

    m_appCfg = *pConfig;

    m_mfxEncParams.mfx.CodecId     = pConfig->CodecId;
    m_mfxEncParams.mfx.TargetUsage = 0; // FEI doesn't have support of
    m_mfxEncParams.mfx.TargetKbps  = 0; // these features
    /*For now FEI work with RATECONTROL_CQP only*/
    m_mfxEncParams.mfx.RateControlMethod = MFX_RATECONTROL_CQP;

    sts = ConvertFrameRate(pConfig->dFrameRate, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtN, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtD);
    MSDK_CHECK_STATUS(sts, "ConvertFrameRate failed");

    // binary flag, 0 signals encoder to take frames in display order. PREENC, ENCPAK, ENC, PAK interfaces works only in encoded order
    m_mfxEncParams.mfx.EncodedOrder = (mfxU16)(pConfig->EncodedOrder || (m_appCfg.bPREENC || m_appCfg.bENCPAK || m_appCfg.bOnlyENC || m_appCfg.bOnlyPAK));

    if (0 != pConfig->QP)
        m_mfxEncParams.mfx.QPI = m_mfxEncParams.mfx.QPP = m_mfxEncParams.mfx.QPB = pConfig->QP;

    // specify memory type
    if (m_bUseHWmemory)
    {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }
    else
    {
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    }

    if (pConfig->bDECODE && !m_bVPPneeded)
    {
        // in case of decoder without VPP copy FrameInfo from decoder
        MSDK_MEMCPY_VAR(m_mfxEncParams.mfx.FrameInfo, &m_mfxDecParams.mfx.FrameInfo, sizeof(mfxFrameInfo));
        m_mfxEncParams.mfx.FrameInfo.PicStruct = pConfig->nPicStruct;

        pConfig->nWidth  = pConfig->nDstWidth  = m_mfxEncParams.mfx.FrameInfo.CropW;
        pConfig->nHeight = pConfig->nDstHeight = m_mfxEncParams.mfx.FrameInfo.CropH;
    }
    else
    {
        // frame info parameters
        m_mfxEncParams.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        m_mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_mfxEncParams.mfx.FrameInfo.PicStruct    = pConfig->nPicStruct;

        // set frame size and crops
        // width must be a multiple of 16
        // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
        m_mfxEncParams.mfx.FrameInfo.Width  = MSDK_ALIGN16(pConfig->nDstWidth);
        m_mfxEncParams.mfx.FrameInfo.Height = (MFX_PICSTRUCT_PROGRESSIVE & m_mfxEncParams.mfx.FrameInfo.PicStruct) ?
            MSDK_ALIGN16(pConfig->nDstHeight) : MSDK_ALIGN32(pConfig->nDstHeight);

        m_mfxEncParams.mfx.FrameInfo.CropX = 0;
        m_mfxEncParams.mfx.FrameInfo.CropY = 0;
        m_mfxEncParams.mfx.FrameInfo.CropW = pConfig->nDstWidth;
        m_mfxEncParams.mfx.FrameInfo.CropH = pConfig->nDstHeight;
    }

    if (m_bNeedDRC)
    {
        m_drcStart = pConfig->nDrcStart;
        m_drcDftW = pConfig->nDRCdefautW;
        m_drcDftH = pConfig->nDRCdefautH;

        size_t whsize = pConfig->nDrcWidth.size();
        m_drcWidth.reserve(whsize);
        m_drcHeight.reserve(whsize);

        m_drcWidth  = pConfig->nDrcWidth;
        m_drcHeight = pConfig->nDrcHeight;
    }

    m_mfxEncParams.AsyncDepth       = 1; //current limitation
    m_mfxEncParams.mfx.GopRefDist   = (std::max)(pConfig->refDist, mfxU16(1));
    m_mfxEncParams.mfx.GopPicSize   = (std::max)(pConfig->gopSize, mfxU16(1));
    m_mfxEncParams.mfx.IdrInterval  = pConfig->nIdrInterval;
    m_mfxEncParams.mfx.GopOptFlag   = pConfig->GopOptFlag;
    m_mfxEncParams.mfx.CodecProfile = pConfig->CodecProfile;
    m_mfxEncParams.mfx.CodecLevel   = pConfig->CodecLevel;

    /* Multi references and multi slices*/
    m_mfxEncParams.mfx.NumRefFrame  = (std::max)(pConfig->numRef,    mfxU16(1));
    m_mfxEncParams.mfx.NumSlice     = (std::max)(pConfig->numSlices, mfxU16(1));

    // configure trellis, B-pyramid, RAW-reference settings
    //if (pInParams->bRefType || pInParams->Trellis || pInParams->bRawRef)
    {
        m_CodingOption2.UseRawRef = (mfxU16)(pConfig->bRawRef ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
        m_CodingOption2.BRefType  = pConfig->bRefType;
        m_CodingOption2.Trellis   = pConfig->Trellis;
        m_EncExtParams.push_back((mfxExtBuffer *)&m_CodingOption2);
    }

    // configure P/B reference number
    {
        m_CodingOption3.NumRefActiveP[0]   = pConfig->bNRefPSpecified   ? pConfig->NumRefActiveP   : MaxNumActiveRefP;
        m_CodingOption3.NumRefActiveBL0[0] = pConfig->bNRefBL0Specified ? pConfig->NumRefActiveBL0 : MaxNumActiveRefBL0;
        m_CodingOption3.NumRefActiveBL1[0] = pConfig->bNRefBL1Specified ? pConfig->NumRefActiveBL1 :
            (pConfig->nPicStruct == MFX_PICSTRUCT_PROGRESSIVE ? MaxNumActiveRefBL1 : MaxNumActiveRefBL1_i);

        /* values stored in m_CodingOption3 required to fill encoding task for PREENC/ENC/PAK*/
        if (pConfig->bNRefPSpecified || pConfig->bNRefBL0Specified || pConfig->bNRefBL1Specified)
            m_EncExtParams.push_back((mfxExtBuffer *)&m_CodingOption3);
    }

    if (pConfig->bENCODE)
    {
        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_ENCODE;
        if (pConfig->bFieldProcessingMode)
            m_encpakInit.SingleFieldProcessing = MFX_CODINGOPTION_ON;
        m_EncExtParams.push_back((mfxExtBuffer *)&m_encpakInit);

        //Create extended buffer to init deblocking parameters
        if (m_appCfg.DisableDeblockingIdc || m_appCfg.SliceAlphaC0OffsetDiv2 ||
            m_appCfg.SliceBetaOffsetDiv2)
        {
            size_t numFields = m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
            MSDK_ZERO_ARRAY(m_encodeSliceHeader, numFields);
            for (size_t fieldId = 0; fieldId < numFields; ++fieldId)
            {
                m_encodeSliceHeader[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_SLICE;
                m_encodeSliceHeader[fieldId].Header.BufferSz = sizeof(mfxExtFeiSliceHeader);

                m_encodeSliceHeader[fieldId].NumSlice =
                    m_encodeSliceHeader[fieldId].NumSliceAlloc = m_mfxEncParams.mfx.NumSlice;
                m_encodeSliceHeader[fieldId].Slice = new mfxExtFeiSliceHeader::mfxSlice[m_encodeSliceHeader[fieldId].NumSliceAlloc];
                MSDK_CHECK_POINTER(m_encodeSliceHeader[fieldId].Slice, MFX_ERR_MEMORY_ALLOC);
                MSDK_ZERO_ARRAY(m_encodeSliceHeader[fieldId].Slice, m_encodeSliceHeader[fieldId].NumSliceAlloc);

                for (size_t sliceNum = 0; sliceNum < m_encodeSliceHeader[fieldId].NumSliceAlloc; ++sliceNum)
                {
                    m_encodeSliceHeader[fieldId].Slice[sliceNum].DisableDeblockingFilterIdc = pConfig->DisableDeblockingIdc;
                    m_encodeSliceHeader[fieldId].Slice[sliceNum].SliceAlphaC0OffsetDiv2     = pConfig->SliceAlphaC0OffsetDiv2;
                    m_encodeSliceHeader[fieldId].Slice[sliceNum].SliceBetaOffsetDiv2        = pConfig->SliceBetaOffsetDiv2;
                }
                m_EncExtParams.push_back((mfxExtBuffer *)&m_encodeSliceHeader[fieldId]);
            }
        }
    }

    if (!m_EncExtParams.empty())
    {
        m_mfxEncParams.ExtParam    = &m_EncExtParams[0]; // vector is stored linearly in memory
        m_mfxEncParams.NumExtParam = (mfxU16)m_EncExtParams.size();
    }

    return sts;
}

mfxStatus CEncodingPipeline::InitMfxVppParams(AppConfig *pConfig)
{
    MSDK_CHECK_POINTER(pConfig, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    // specify memory type
    if (m_bUseHWmemory)
    {
        m_mfxVppParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY  | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }
    else
    {
        m_mfxVppParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    }

    if (pConfig->bDECODE)
    {
        MSDK_MEMCPY_VAR(m_mfxVppParams.vpp.In, &m_mfxDecParams.mfx.FrameInfo, sizeof(mfxFrameInfo));
        m_mfxVppParams.vpp.In.PicStruct = pConfig->nPicStruct;

        pConfig->nWidth  = m_mfxVppParams.vpp.Out.CropW;
        pConfig->nHeight = m_mfxVppParams.vpp.Out.CropH;
    }
    else
    {
        // input frame info
        m_mfxVppParams.vpp.In.FourCC    = MFX_FOURCC_NV12;
        m_mfxVppParams.vpp.In.PicStruct = pConfig->nPicStruct;

        sts = ConvertFrameRate(pConfig->dFrameRate, &m_mfxVppParams.vpp.In.FrameRateExtN, &m_mfxVppParams.vpp.In.FrameRateExtD);
        MSDK_CHECK_STATUS(sts, "ConvertFrameRate failed");

        // width must be a multiple of 16
        // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
        m_mfxVppParams.vpp.In.Width  = MSDK_ALIGN16(pConfig->nWidth);
        m_mfxVppParams.vpp.In.Height = (MFX_PICSTRUCT_PROGRESSIVE & m_mfxVppParams.vpp.In.PicStruct) ?
            MSDK_ALIGN16(pConfig->nHeight) : MSDK_ALIGN32(pConfig->nHeight);

        // set crops in input mfxFrameInfo for correct work of file reader
        // VPP itself ignores crops at initialization
        m_mfxVppParams.vpp.In.CropX = m_mfxVppParams.vpp.In.CropY = 0;
        m_mfxVppParams.vpp.In.CropW = pConfig->nWidth;
        m_mfxVppParams.vpp.In.CropH = pConfig->nHeight;
    }

    // fill output frame info
    MSDK_MEMCPY_VAR(m_mfxVppParams.vpp.Out, &m_mfxVppParams.vpp.In, sizeof(mfxFrameInfo));

    // only resizing is supported
    m_mfxVppParams.vpp.Out.CropX = m_mfxVppParams.vpp.Out.CropY = 0;
    m_mfxVppParams.vpp.Out.CropW = pConfig->nDstWidth;
    m_mfxVppParams.vpp.Out.CropH = pConfig->nDstHeight;

    m_mfxVppParams.vpp.Out.Width  = MSDK_ALIGN16(pConfig->nDstWidth);
    m_mfxVppParams.vpp.Out.Height = (MFX_PICSTRUCT_PROGRESSIVE & m_mfxVppParams.vpp.Out.PicStruct) ?
        MSDK_ALIGN16(pConfig->nDstHeight) : MSDK_ALIGN32(pConfig->nDstHeight);

    // configure and attach external parameters
    m_VppDoNotUse.NumAlg = 4;
    m_VppDoNotUse.AlgList = new mfxU32[m_VppDoNotUse.NumAlg];
    MSDK_CHECK_POINTER(m_VppDoNotUse.AlgList, MFX_ERR_MEMORY_ALLOC);
    m_VppDoNotUse.AlgList[0] = MFX_EXTBUFF_VPP_DENOISE;        // turn off denoising (on by default)
    m_VppDoNotUse.AlgList[1] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS; // turn off scene analysis (on by default)
    m_VppDoNotUse.AlgList[2] = MFX_EXTBUFF_VPP_DETAIL;         // turn off detail enhancement (on by default)
    m_VppDoNotUse.AlgList[3] = MFX_EXTBUFF_VPP_PROCAMP;        // turn off processing amplified (on by default)
    m_VppExtParams.push_back((mfxExtBuffer *)&m_VppDoNotUse);

    m_mfxVppParams.ExtParam    = &m_VppExtParams[0]; // vector is stored linearly in memory
    m_mfxVppParams.NumExtParam = (mfxU16)m_VppExtParams.size();

    m_mfxVppParams.AsyncDepth = 1;

    if (pConfig->preencDSstrength)
    {
        m_mfxDSParams = m_mfxVppParams;

        if (m_pmfxVPP)
        {
            MSDK_MEMCPY_VAR(m_mfxDSParams.vpp.In, &m_mfxVppParams.vpp.Out, sizeof(mfxFrameInfo));
        }

        m_mfxDSParams.vpp.Out.Width  = MSDK_ALIGN16(pConfig->nDstWidth / pConfig->preencDSstrength);
        m_mfxDSParams.vpp.Out.Height = (MFX_PICSTRUCT_PROGRESSIVE & m_mfxDSParams.vpp.Out.PicStruct) ?
            MSDK_ALIGN16(pConfig->nDstHeight / pConfig->preencDSstrength) : MSDK_ALIGN32(pConfig->nDstHeight / pConfig->preencDSstrength);
        m_mfxDSParams.vpp.Out.CropW = m_mfxDSParams.vpp.Out.Width;
        m_mfxDSParams.vpp.Out.CropH = m_mfxDSParams.vpp.Out.Height;
    }

    return sts;
}

mfxStatus CEncodingPipeline::InitMfxDecodeParams(AppConfig *pConfig)
{
    mfxStatus sts = MFX_ERR_NONE;
    MSDK_CHECK_POINTER(pConfig, MFX_ERR_NULL_PTR);

    m_mfxDecParams.AsyncDepth = 1;

    // set video type in parameters
    m_mfxDecParams.mfx.CodecId = pConfig->DecodeId;

    m_BSReader.Init(pConfig->strSrcFile);
    sts = InitMfxBitstream(&m_mfxBS, 1024 * 1024);
    MSDK_CHECK_STATUS(sts, "InitMfxBitstream failed");

    // read a portion of data for DecodeHeader function
    sts = m_BSReader.ReadNextFrame(&m_mfxBS);
    if (MFX_ERR_MORE_DATA == sts)
        return sts;
    else
        MSDK_CHECK_STATUS(sts, "m_BSReader.ReadNextFrame failed");

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
                MSDK_CHECK_STATUS(sts, "ExtendMfxBitstream failed");
            }

            // read a portion of data for DecodeHeader function
            sts = m_BSReader.ReadNextFrame(&m_mfxBS);
            if (MFX_ERR_MORE_DATA == sts)
                return sts;
            else
                MSDK_CHECK_STATUS(sts, "m_BSReader.ReadNextFrame failed");

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

    if (pConfig->nPicStruct == MFX_PICSTRUCT_UNKNOWN)
    {
        // In this case we acquire picture structure from the input stream
        m_mfxDecParams.mfx.ExtendedPicStruct = 1;
    }

     // specify memory type
     m_mfxDecParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    return sts;
}

mfxStatus CEncodingPipeline::CreateHWDevice()
{
    mfxStatus sts = MFX_ERR_NONE;

    m_hwdev = CreateVAAPIDevice();
    if (NULL == m_hwdev)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    sts = m_hwdev->Init(NULL, 0, MSDKAdapter::GetNumber(m_mfxSession));
    MSDK_CHECK_STATUS(sts, "m_hwdev->Init failed");

    return sts;
}

mfxStatus CEncodingPipeline::ResetDevice()
{
    if (m_bUseHWmemory)
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
        MSDK_CHECK_STATUS(sts, "m_pmfxENCODE->QueryIOSurf failed");
    }

    m_maxQueueLength = m_refDist * 2 + m_mfxEncParams.AsyncDepth;
    /* temporary solution for a while for PREENC + ENC cases
    * (It required to calculate accurate formula for all usage cases)
    * this is not optimal from memory usage consumption */
    m_maxQueueLength += m_mfxEncParams.mfx.NumRefFrame + 1;

    // The number of surfaces shared by vpp output and encode input.
    // When surfaces are shared 1 surface at first component output contains output frame that goes to next component input
    if (m_appCfg.nInputSurf == 0)
    {
        nEncSurfNum = EncRequest.NumFrameSuggested + (m_nAsyncDepth - 1) + 2;
        if ((m_appCfg.bPREENC) || (m_appCfg.bENCPAK) || (m_appCfg.bENCODE) ||
            (m_appCfg.bOnlyENC) || (m_appCfg.bOnlyPAK))
        {
            nEncSurfNum  = m_maxQueueLength;
            nEncSurfNum += m_pmfxVPP ? m_refDist + 1 : 0;
        }
    }
    else
    {
        nEncSurfNum = m_appCfg.nInputSurf;
    }

    // prepare allocation requests
    EncRequest.NumFrameMin       = nEncSurfNum;
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
        MSDK_CHECK_STATUS(sts, "m_pmfxDS->QueryIOSurf failed");

        // these surfaces are input surfaces for PREENC
        DSRequest[1].NumFrameMin = DSRequest[1].NumFrameSuggested = EncRequest.NumFrameSuggested;
        DSRequest[1].Type        = EncRequest.Type | MFX_MEMTYPE_FROM_ENC;
        DSRequest[1].AllocId     = m_BaseAllocID;

        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &(DSRequest[1]), &m_dsResponse);
        MSDK_CHECK_STATUS(sts, "m_pMFXAllocator->Alloc failed");

        m_pDSSurfaces.PoolSize = m_dsResponse.NumFrameActual;
        m_pDSSurfaces.Strategy = m_surfPoolStrategy;
        sts = FillSurfacePool(m_pDSSurfaces.SurfacesPool, &m_dsResponse, &(m_mfxDSParams.vpp.Out));
        MSDK_CHECK_STATUS(sts, "FillSurfacePool failed");
    }

    if (m_pmfxDECODE)
    {
        sts = m_pmfxDECODE->QueryIOSurf(&m_mfxDecParams, &DecRequest);
        MSDK_CHECK_STATUS(sts, "m_pmfxDECODE->QueryIOSurf failed");
        DecRequest.NumFrameMin = m_decodePoolSize = DecRequest.NumFrameSuggested;

        if (!m_pmfxVPP)
        {
            // in case of Decode and absence of VPP we use the same surface pool for the entire pipeline
            DecRequest.NumFrameMin = DecRequest.NumFrameSuggested = m_decodePoolSize + nEncSurfNum;
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
        MSDK_CHECK_STATUS(sts, "m_pMFXAllocator->Alloc failed");

        // prepare mfxFrameSurface1 array for decoder
        m_pDecSurfaces.PoolSize = m_DecResponse.NumFrameActual;
        m_pDecSurfaces.Strategy = m_surfPoolStrategy;
        sts = FillSurfacePool(m_pDecSurfaces.SurfacesPool, &m_DecResponse, &(m_mfxDecParams.mfx.FrameInfo));
        MSDK_CHECK_STATUS(sts, "FillSurfacePool failed");

        if (!(m_pmfxVPP || m_pmfxENC || m_pmfxPAK))
            return MFX_ERR_NONE;
    }

    if (m_pmfxVPP && !m_pmfxDECODE)
    {
        // in case of absence of DECODE we need to allocate input surfaces for VPP

        sts = m_pmfxVPP->QueryIOSurf(&m_mfxVppParams, VppRequest);
        MSDK_CHECK_STATUS(sts, "m_pmfxVPP->QueryIOSurf failed");

        VppRequest[0].NumFrameMin = VppRequest[0].NumFrameSuggested;
        VppRequest[0].AllocId     = m_BaseAllocID;

        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &(VppRequest[0]), &m_VppResponse);
        MSDK_CHECK_STATUS(sts, "m_pMFXAllocator->Alloc failed");

        // prepare mfxFrameSurface1 array for VPP
        m_pVppSurfaces.PoolSize = m_VppResponse.NumFrameActual;
        m_pVppSurfaces.Strategy = m_surfPoolStrategy;
        sts = FillSurfacePool(m_pVppSurfaces.SurfacesPool, &m_VppResponse, &(m_mfxVppParams.mfx.FrameInfo));
        MSDK_CHECK_STATUS(sts, "FillSurfacePool failed");
    }

    if (!m_pmfxDECODE || m_pmfxVPP)
    {
        // alloc frames for encoder
        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &EncRequest, &m_EncResponse);
        MSDK_CHECK_STATUS(sts, "m_pMFXAllocator->Alloc failed");

        // prepare mfxFrameSurface1 array for encoder
        m_pEncSurfaces.PoolSize = m_EncResponse.NumFrameActual;
        m_pEncSurfaces.Strategy = m_surfPoolStrategy;
        sts = FillSurfacePool(m_pEncSurfaces.SurfacesPool, &m_EncResponse, &(m_mfxEncParams.mfx.FrameInfo));
        MSDK_CHECK_STATUS(sts, "FillSurfacePool failed");
    }

    /* ENC use input source surfaces only & does not need real reconstructed surfaces.
     * But source surfaces index is always equal to reconstructed surface index.
     * PAK generate real reconstructed surfaces (instead of source surfaces)Alloc reconstructed surfaces for PAK
     * So, after PAK call source surface changed on reconstructed surface
     * This is important limitation for PAK
     * */
    if ((m_pmfxENC) || (m_pmfxPAK))
    {
        ReconRequest.AllocId = m_EncPakReconAllocID;
        MSDK_MEMCPY_VAR(ReconRequest.Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
        ReconRequest.NumFrameMin       = m_appCfg.nReconSurf ? m_appCfg.nReconSurf : EncRequest.NumFrameMin;
        ReconRequest.NumFrameSuggested = m_appCfg.nReconSurf ? m_appCfg.nReconSurf : EncRequest.NumFrameSuggested;
        ReconRequest.Type = EncRequest.Type;

        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &ReconRequest, &m_ReconResponse);
        MSDK_CHECK_STATUS(sts, "m_pMFXAllocator->Alloc failed");

        m_pReconSurfaces.PoolSize = m_ReconResponse.NumFrameActual;
        m_pReconSurfaces.Strategy = m_surfPoolStrategy;
        sts = FillSurfacePool(m_pReconSurfaces.SurfacesPool, &m_ReconResponse, &(m_mfxEncParams.mfx.FrameInfo));
        MSDK_CHECK_STATUS(sts, "FillSurfacePool failed");
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
            MSDK_CHECK_STATUS(sts, "m_pMFXAllocator->Lock failed");
        }
    }

    return sts;
}

mfxStatus CEncodingPipeline::CreateAllocator()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_bUseHWmemory)
    {
        sts = CreateHWDevice();
        MSDK_CHECK_STATUS(sts, "CreateHWDevice failed");
        /* It's possible to skip failed result here and switch to SW implementation,
        but we don't process this way */

        mfxHDL hdl = NULL;
        sts = m_hwdev->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
        // provide device manager to MediaSDK
        sts = m_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
        MSDK_CHECK_STATUS(sts, "m_mfxSession.SetHandle failed");
        if (m_bSeparatePreENCSession){
            sts = m_preenc_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
            MSDK_CHECK_STATUS(sts, "m_preenc_mfxSession.SetHandle failed");
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
        MSDK_CHECK_STATUS(sts, "m_mfxSession.SetFrameAllocator failed");
        if (m_bSeparatePreENCSession){
            sts = m_preenc_mfxSession.SetFrameAllocator(m_pMFXAllocator);
            MSDK_CHECK_STATUS(sts, "m_preenc_mfxSession.SetFrameAllocator failed");
        }

        m_bExternalAlloc = true;
    }
    else
    {
        //in case of system memory allocator we also have to pass MFX_HANDLE_VA_DISPLAY to HW library
        mfxIMPL impl;
        m_mfxSession.QueryIMPL(&impl);

        if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(impl))
        {
            sts = CreateHWDevice();
            MSDK_CHECK_STATUS(sts, "CreateHWDevice failed");

            mfxHDL hdl = NULL;
            sts = m_hwdev->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
            // provide device manager to MediaSDK
            sts = m_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
            MSDK_CHECK_STATUS(sts, "m_mfxSession.SetHandle failed");

            if (m_bSeparatePreENCSession){
                sts = m_preenc_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
                MSDK_CHECK_STATUS(sts, "m_preenc_mfxSession.SetHandle failed");
            }
        }

        // create system memory allocator
        m_pMFXAllocator = new SysMemFrameAllocator;
        MSDK_CHECK_POINTER(m_pMFXAllocator, MFX_ERR_MEMORY_ALLOC);

        /* In case of system memory we demonstrate "no external allocator" usage model.
        We don't call SetAllocator, Media SDK uses internal allocator.
        We use system memory allocator simply as a memory manager for application*/
    }

    // initialize memory allocator
    sts = m_pMFXAllocator->Init(m_pmfxAllocatorParams);
    MSDK_CHECK_STATUS(sts, "m_pMFXAllocator->Init failed");

    return sts;
}

void CEncodingPipeline::DeleteFrames()
{
    // delete all allocated surfaces
    m_pDecSurfaces.DeleteFrames();
    m_pVppSurfaces.DeleteFrames();
    m_pDSSurfaces.DeleteFrames();

    if (m_pEncSurfaces.SurfacesPool != m_pReconSurfaces.SurfacesPool)
    {
        m_pEncSurfaces.DeleteFrames();
        m_pReconSurfaces.DeleteFrames();
    }
    else
    {
        m_pEncSurfaces.DeleteFrames();

        /* Prevent double-free in m_pReconSurfaces.DeleteFrames */
        m_pReconSurfaces.SurfacesPool = NULL;
        m_pReconSurfaces.DeleteFrames();
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

    SAFE_FCLOSE(mvout,           MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(mbstatout,       MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(mbcodeout,       MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(mvENCPAKout,     MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(pMvPred,         MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(pEncMBs,         MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(pPerMbQP,        MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(decodeStreamout, MFX_ERR_INVALID_HANDLE);
    SAFE_FCLOSE(pRepackCtrl,     MFX_ERR_INVALID_HANDLE);

    //unlock last frames
    m_inputTasks.Clear();

    sts = FreeBuffers(m_preencBufs);
    MSDK_CHECK_STATUS(sts, "FreeBuffers failed");
    sts = FreeBuffers(m_encodeBufs);
    MSDK_CHECK_STATUS(sts, "FreeBuffers failed");

    MSDK_SAFE_DELETE(m_ctr);
    MSDK_SAFE_DELETE(m_tmpMBpreenc);
    MSDK_SAFE_DELETE_ARRAY(m_tmpForMedian);
    MSDK_SAFE_DELETE_ARRAY(m_tmpForReading);
    MSDK_SAFE_DELETE(m_tmpMBenc);

    sts = ClearDecoderBuffers();
    MSDK_CHECK_STATUS(sts, "ClearDecoderBuffers failed");

    m_ref_info.Clear();

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

CEncodingPipeline::CEncodingPipeline():
    m_FileWriter(NULL),
    m_nAsyncDepth(1),
    m_numOfFields(1), // default is progressive case
    m_pmfxDECODE(NULL),
    m_pmfxVPP(NULL),
    m_pmfxDS(NULL),
    m_pmfxENCODE(NULL),
    m_pmfxPREENC(NULL),
    m_pmfxENC(NULL),
    m_pmfxPAK(NULL),
    m_ctr(NULL),
    m_pMFXAllocator(NULL),
    m_pmfxAllocatorParams(NULL),
    m_bUseHWmemory(true), //only HW memory is supported (ENCODE supports SW memory)
    m_bExternalAlloc(false),
    m_hwdev(NULL),
    m_LastDecSyncPoint(0),
    m_pExtBufDecodeStreamout(NULL),

    m_bEndOfFile(false),
    m_insertIDR(false),
    m_disableMVoutPreENC(false),
    m_disableMBStatPreENC(false),
    m_bSeparatePreENCSession(false),
    m_enableMVpredPreENC(false),
    m_log2frameNumMax(8),
    m_frameCount(0),
    m_frameOrderIdrInDisplayOrder(0),
    m_frameType(PairU8((mfxU8)MFX_FRAMETYPE_UNKNOWN, (mfxU8)MFX_FRAMETYPE_UNKNOWN)),
    m_isField(false),

    m_numMB_drc(0),
    m_bDRCReset(false),
    m_bNeedDRC(false),
    m_bVPPneeded(false),
    m_drcDftW(0),
    m_drcDftH(0),

    m_tmpForReading(NULL),
    m_tmpMBpreenc(NULL),
    m_tmpMBenc(NULL),
    m_tmpForMedian(NULL),
    m_surfPoolStrategy(PREFER_FIRST_FREE)
{
    MSDK_ZERO_MEMORY(m_pDecSurfaces);
    MSDK_ZERO_MEMORY(m_pEncSurfaces);
    MSDK_ZERO_MEMORY(m_pVppSurfaces);
    MSDK_ZERO_MEMORY(m_pDSSurfaces);
    MSDK_ZERO_MEMORY(m_pReconSurfaces);

    MSDK_ZERO_ARRAY(m_numOfRefs[0], 2);
    MSDK_ZERO_ARRAY(m_numOfRefs[1], 2);

    MSDK_ZERO_MEMORY(m_mfxBS);

    MSDK_ZERO_MEMORY(m_CodingOption2);
    m_CodingOption2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    m_CodingOption2.Header.BufferSz = sizeof(m_CodingOption2);

    MSDK_ZERO_MEMORY(m_CodingOption3);
    m_CodingOption3.Header.BufferId = MFX_EXTBUFF_CODING_OPTION3;
    m_CodingOption3.Header.BufferSz = sizeof(m_CodingOption3);

    MSDK_ZERO_MEMORY(m_VppDoNotUse);
    m_VppDoNotUse.Header.BufferId = MFX_EXTBUFF_VPP_DONOTUSE;
    m_VppDoNotUse.Header.BufferSz = sizeof(m_VppDoNotUse);

    MSDK_ZERO_MEMORY(m_mfxEncParams);
    MSDK_ZERO_MEMORY(m_DecResponse);
    MSDK_ZERO_MEMORY(m_EncResponse);
}

CEncodingPipeline::~CEncodingPipeline()
{
    Close();
}

mfxStatus CEncodingPipeline::InitFileWriter(CSmplBitstreamWriter * & pWriter, const msdk_char *filename)
{
    MSDK_SAFE_DELETE(pWriter);
    pWriter = new CSmplBitstreamWriter;
    MSDK_CHECK_POINTER(pWriter, MFX_ERR_MEMORY_ALLOC);
    mfxStatus sts = pWriter->Init(filename);
    MSDK_CHECK_STATUS(sts, "pWriter->Init(filename) failed");

    return sts;
}

mfxStatus CEncodingPipeline::ResetIOFiles(const AppConfig & Config)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = ResetBuffers();
    MSDK_CHECK_STATUS(sts, "ResetBuffers failed");

    if (m_FileWriter) // no file writers for preenc
    {
        sts = m_FileWriter->Init(Config.dstFileBuff[0]);
        MSDK_CHECK_STATUS(sts, "m_FileWriter->Init failed");
    }

    if (Config.bDECODE)
    {
        sts = m_BSReader.Init(Config.strSrcFile);
    }
    else
    {
        sts = m_FileReader.Init(Config.strSrcFile,
            Config.ColorFormat,
            0,
            Config.srcFileBuff);
    }
    MSDK_CHECK_STATUS(sts, "<File reader>.Init failed");

    SAFE_FSEEK(mbstatout,       0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(mvout,           0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(mvENCPAKout,     0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(mbcodeout,       0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(pMvPred,         0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(pEncMBs,         0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(pPerMbQP,        0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(decodeStreamout, 0, SEEK_SET, MFX_ERR_MORE_DATA);
    SAFE_FSEEK(pRepackCtrl,     0, SEEK_SET, MFX_ERR_MORE_DATA);

    return sts;
}

mfxStatus CEncodingPipeline::Init(AppConfig *pConfig)
{
    MSDK_CHECK_POINTER(pConfig, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;
    m_bNeedDRC = pConfig->bDynamicRC;

    m_bVPPneeded = pConfig->nWidth  != pConfig->nDstWidth  ||
                   pConfig->nHeight != pConfig->nDstHeight ||
                   m_bNeedDRC;

    m_refDist = pConfig->refDist > 0 ? pConfig->refDist : 1;
    m_gopSize = pConfig->gopSize > 0 ? pConfig->gopSize : 1;

    // define strategy of surface selection from pool
    m_surfPoolStrategy = (pConfig->nReconSurf || pConfig->nInputSurf) ? PREFER_NEW : PREFER_FIRST_FREE;

    // prepare input file reader
    if (!pConfig->bDECODE)
    {
        sts = m_FileReader.Init(pConfig->strSrcFile,
            pConfig->ColorFormat,
            0,
            pConfig->srcFileBuff);
        MSDK_CHECK_STATUS(sts, "m_FileReader.Init failed");
    }

    // Init FileWriter for bitstream
    if (pConfig->bENCODE || pConfig->bENCPAK || pConfig->bOnlyPAK)
    {
        sts = InitFileWriter(m_FileWriter, pConfig->dstFileBuff[0]);
        MSDK_CHECK_STATUS(sts, "InitFileWriter failed");
    }

    // Section below initialize sessions and sets proper allocId for components
    m_bSeparatePreENCSession = pConfig->bPREENC && (pConfig->bENCPAK || pConfig->bOnlyENC || (pConfig->preencDSstrength && m_bVPPneeded));
    m_pPreencSession = m_bSeparatePreENCSession ? &m_preenc_mfxSession : &m_mfxSession;

    sts = InitSessions();
    MSDK_CHECK_STATUS(sts, "InitSessions failed");

    mfxSession akaSession = m_mfxSession.operator mfxSession();
    m_BaseAllocID = (mfxU64)&akaSession & 0xffffffff;
    m_EncPakReconAllocID = m_BaseAllocID + 1;

    // set memory type
    m_bUseHWmemory = pConfig->bUseHWmemory;

    if (pConfig->bDECODE)
    {
        // create decoder
        m_pmfxDECODE = new MFXVideoDECODE(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxDECODE, MFX_ERR_MEMORY_ALLOC);

        sts = InitMfxDecodeParams(pConfig);
        MSDK_CHECK_STATUS(sts, "InitMfxDecodeParams failed");
    }

    sts = InitMfxEncParams(pConfig);
    MSDK_CHECK_STATUS(sts, "InitMfxEncParams failed");

    // create and init frame allocator
    sts = CreateAllocator();
    MSDK_CHECK_STATUS(sts, "CreateAllocator failed");

    // create preprocessor if resizing was requested from command line
    if (m_bVPPneeded)
    {
        m_pmfxVPP = new MFXVideoVPP(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxVPP, MFX_ERR_MEMORY_ALLOC);

        sts = InitMfxVppParams(pConfig);
        MSDK_CHECK_STATUS(sts, "InitMfxVppParams failed");
    }

    //if(pParams->bENCODE){
        // create encoder
        m_pmfxENCODE = new MFXVideoENCODE(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxENCODE, MFX_ERR_MEMORY_ALLOC);
    //}

    if (pConfig->bPREENC){
        m_pmfxPREENC = new MFXVideoENC(*m_pPreencSession);
        MSDK_CHECK_POINTER(m_pmfxPREENC, MFX_ERR_MEMORY_ALLOC);

        if (pConfig->preencDSstrength)
        {
            if (!m_pmfxVPP)
            {
                sts = InitMfxVppParams(pConfig);
                MSDK_CHECK_STATUS(sts, "InitMfxVppParams failed");
            }
            m_pmfxDS = new MFXVideoVPP(*m_pPreencSession);
            MSDK_CHECK_POINTER(m_pmfxDS, MFX_ERR_MEMORY_ALLOC);
        }
    }

    if (pConfig->bENCPAK || pConfig->bOnlyENC){
        // create ENC
        m_pmfxENC = new MFXVideoENC(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_MEMORY_ALLOC);
    }

    if (pConfig->bENCPAK || pConfig->bOnlyPAK){
        // create PAK
        m_pmfxPAK = new MFXVideoPAK(m_mfxSession);
        MSDK_CHECK_POINTER(m_pmfxPAK, MFX_ERR_MEMORY_ALLOC);
    }

    m_nAsyncDepth = 1; // FEI works only in syncronyous mode

    sts = ResetMFXComponents(pConfig, true);
    MSDK_CHECK_STATUS(sts, "ResetMFXComponents failed");

    sts = FillPipelineParameters();
    MSDK_CHECK_STATUS(sts, "FillPipelineParameters failed");

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::FillPipelineParameters()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_appCfg.bENCODE)
    {
        sts = UpdateVideoParams(); // update settings according to those that exposed by MSDK library
        MSDK_CHECK_STATUS(sts, "UpdateVideoParams failed");
    }

    /* if TFF or BFF*/
    if ((m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF) ||
        (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF) ||
        (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_UNKNOWN))
    {
        m_numOfFields = 2;
        m_isField = true;
    }

    // Section below calculates max possible number of MBs for extended Buffer allocation
    bool chng_res = m_bNeedDRC || m_appCfg.nDstWidth != m_appCfg.nWidth || m_appCfg.nDstHeight != m_appCfg.nHeight;
    mfxU16 wdt = chng_res ? m_appCfg.nDstWidth  : m_appCfg.nWidth,  // if no VPP or DRC used
           hgt = chng_res ? m_appCfg.nDstHeight : m_appCfg.nHeight; // dstW/dstH is set the same as source W/H

    // For interlaced mode, may need an extra MB vertically
    // For example if the progressive mode has 45 MB vertically
    // The interlace should have 23 MB for each field

    m_widthMB  = MSDK_ALIGN16(wdt);
    m_heightMB = m_isField ? MSDK_ALIGN32(hgt) : MSDK_ALIGN16(hgt);

    m_numMB_frame = m_numMB = (m_widthMB * m_heightMB) >> 8;

    m_widthMB  >>= 4;
    m_heightMB >>= m_isField ? 5 : 4;

    m_numMB /= (mfxU16)m_numOfFields;
    m_numMB_drc = m_numMB;

    if (m_appCfg.bPREENC && m_appCfg.preencDSstrength)
    {
        // PreEnc is performed on lower resolution
        m_widthMBpreenc = MSDK_ALIGN16(m_mfxDSParams.vpp.Out.Width);
        mfxU16 heightMB = m_isField ? MSDK_ALIGN32(m_mfxDSParams.vpp.Out.Height) : MSDK_ALIGN16(m_mfxDSParams.vpp.Out.Height);
        m_numMBpreenc_frame = m_numMBpreenc = (m_widthMBpreenc * heightMB) >> 8;

        m_numMBpreenc /= (mfxU16)m_numOfFields;

        m_widthMBpreenc >>= 4;
    }
    else
    {
        m_widthMBpreenc = m_widthMB;
        m_numMBpreenc = m_numMB;
        m_numMBpreenc_frame = m_numMB_frame;
    }

    /* m_ffid / m_sfid - holds parity of first / second field: 0 - top_field, 1 - bottom_field */
    m_ffid = m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF;
    m_sfid = m_isField - m_ffid;

    /* Initialize task pool */
    m_inputTasks.Init(m_mfxEncParams.mfx.GopRefDist, m_mfxEncParams.mfx.GopOptFlag, m_mfxEncParams.mfx.NumRefFrame, m_mfxEncParams.mfx.NumRefFrame + 1, m_log2frameNumMax);

    m_taskInitializationParams.BRefType        = m_CodingOption2.BRefType;
    m_taskInitializationParams.GopPicSize      = m_mfxEncParams.mfx.GopPicSize;
    m_taskInitializationParams.GopRefDist      = m_mfxEncParams.mfx.GopRefDist;
    m_taskInitializationParams.NumRefActiveP   = m_CodingOption3.NumRefActiveP[0];
    m_taskInitializationParams.NumRefActiveBL0 = m_CodingOption3.NumRefActiveBL0[0];
    m_taskInitializationParams.NumRefActiveBL1 = m_CodingOption3.NumRefActiveBL1[0];

    return sts;
}

void CEncodingPipeline::Close()
{
    if (m_FileWriter)
        msdk_printf(MSDK_STRING("Frame number: %u\r"), m_frameCount);

    MSDK_SAFE_DELETE(m_pmfxDECODE);
    MSDK_SAFE_DELETE(m_pmfxVPP);
    MSDK_SAFE_DELETE(m_pmfxDS);
    MSDK_SAFE_DELETE(m_pmfxENCODE);
    MSDK_SAFE_DELETE(m_pmfxENC);
    MSDK_SAFE_DELETE(m_pmfxPAK);
    MSDK_SAFE_DELETE(m_pmfxPREENC);

    MSDK_SAFE_DELETE_ARRAY(m_VppDoNotUse.AlgList);

    if (m_appCfg.DisableDeblockingIdc || m_appCfg.SliceAlphaC0OffsetDiv2 ||
        m_appCfg.SliceBetaOffsetDiv2)
    {
        for (size_t field = 0; field < m_numOfFields; field++)
        {
            MSDK_SAFE_DELETE_ARRAY(m_encodeSliceHeader[field].Slice);
        }
    }

    DeleteFrames();

    m_TaskPool.Close();
    m_mfxSession.Close();
    if (m_bSeparatePreENCSession){
        m_preenc_mfxSession.Close();
    }

    // allocator if used as external for MediaSDK must be deleted after SDK components
    DeleteAllocator();

    m_FileReader.Close();
    FreeFileWriter();

    WipeMfxBitstream(&m_mfxBS);
}

void CEncodingPipeline::FreeFileWriter()
{
    if (m_FileWriter)
        m_FileWriter->Close();
    MSDK_SAFE_DELETE(m_FileWriter);
}

mfxStatus CEncodingPipeline::ResetMFXComponents(AppConfig* pConfig, bool realloc_frames)
{
    MSDK_CHECK_POINTER(pConfig, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    if (m_pmfxDECODE)
    {
        sts = m_pmfxDECODE->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_STATUS(sts, "m_pmfxDECODE->Close failed");
    }

    if (m_pmfxVPP)
    {
        sts = m_pmfxVPP->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_STATUS(sts, "m_pmfxVPP->Close failed");
    }

    if (m_pmfxDS)
    {
        sts = m_pmfxDS->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_STATUS(sts, "m_pmfxDS->Close failed");
    }

    if (m_pmfxPREENC)
    {
        sts = m_pmfxPREENC->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_STATUS(sts, "m_pmfxPREENC->Close failed");
    }

    if (m_pmfxENC)
    {
        sts = m_pmfxENC->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_STATUS(sts, "m_pmfxENC->Close failed");
    }

    if (m_pmfxPAK)
    {
        sts = m_pmfxPAK->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_STATUS(sts, "m_pmfxPAK->Close failed");
    }

    if (m_pmfxENCODE)
    {
        sts = m_pmfxENCODE->Close();
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_STATUS(sts, "m_pmfxENCODE->Close failed");
    }

    // free allocated frames
    if (realloc_frames)
    {
        DeleteFrames();

        sts = AllocFrames();
        MSDK_CHECK_STATUS(sts, "AllocFrames failed");
    }

    m_TaskPool.Close();

    // Init decode
    if (m_pmfxDECODE)
    {
        // if streamout enabled
        mfxExtBuffer* buf[1];
        if (m_appCfg.bDECODESTREAMOUT)
        {
            MSDK_ZERO_MEMORY(m_encpakInit);
            m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
            m_encpakInit.Header.BufferSz = sizeof(mfxExtFeiParam);
            m_encpakInit.Func = MFX_FEI_FUNCTION_DEC;
            m_encpakInit.SingleFieldProcessing = MFX_CODINGOPTION_OFF;
            buf[0] = (mfxExtBuffer*)&m_encpakInit;
            m_mfxDecParams.NumExtParam = 1;
            m_mfxDecParams.ExtParam    = buf;
        }
        m_mfxDecParams.mfx.DecodedOrder = (mfxU16)m_appCfg.DecodedOrder;

        m_mfxDecParams.AllocId = m_BaseAllocID;
        sts = m_pmfxDECODE->Init(&m_mfxDecParams);
        MSDK_CHECK_STATUS(sts, "m_pmfxDECODE->Init failed");
    }

    if (m_pmfxVPP)
    {
        m_mfxVppParams.AllocId = m_BaseAllocID;
        sts = m_pmfxVPP->Init(&m_mfxVppParams);
        MSDK_CHECK_STATUS(sts, "m_pmfxVPP->Init failed");
    }

    if ((m_appCfg.bENCODE) && (m_pmfxENCODE))
    {
        sts = m_pmfxENCODE->Init(&m_mfxEncParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_STATUS(sts, "m_pmfxENCODE->Init failed");
    }

    if(m_pmfxPREENC)
    {
        mfxVideoParam preEncParams = m_mfxEncParams;

        if (m_pmfxDS)
        {
            sts = m_pmfxDS->Init(&m_mfxDSParams);
            MSDK_CHECK_STATUS(sts, "m_pmfxDS->Init failed");

            // PREENC will be performed on downscaled picture
            preEncParams.mfx.FrameInfo.Width  = m_mfxDSParams.vpp.Out.Width;
            preEncParams.mfx.FrameInfo.Height = m_mfxDSParams.vpp.Out.Height;
            preEncParams.mfx.FrameInfo.CropW  = m_mfxDSParams.vpp.Out.Width;
            preEncParams.mfx.FrameInfo.CropH  = m_mfxDSParams.vpp.Out.Height;
        }

        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_PREENC;
        if (pConfig->bFieldProcessingMode)
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

        MSDK_CHECK_STATUS(sts, "m_pmfxPREENC->Init failed");
    }

    if(m_pmfxENC)
    {
        mfxVideoParam encParams = m_mfxEncParams;

        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_ENC;
        if (pConfig->bFieldProcessingMode)
            m_encpakInit.SingleFieldProcessing = MFX_CODINGOPTION_ON;

        mfxExtBuffer* buf[1];
        buf[0] = (mfxExtBuffer*)&m_encpakInit;

        encParams.NumExtParam = 1;
        encParams.ExtParam    = buf;
        encParams.AllocId     = m_EncPakReconAllocID;

        sts = m_pmfxENC->Init(&encParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_STATUS(sts, "m_pmfxENC->Init failed");
    }

    if(m_pmfxPAK)
    {
        mfxVideoParam pakParams = m_mfxEncParams;

        //Create extended buffer to Init FEI
        MSDK_ZERO_MEMORY(m_encpakInit);
        m_encpakInit.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
        m_encpakInit.Header.BufferSz = sizeof (mfxExtFeiParam);
        m_encpakInit.Func = MFX_FEI_FUNCTION_PAK;
        if (pConfig->bFieldProcessingMode)
            m_encpakInit.SingleFieldProcessing = MFX_CODINGOPTION_ON;

        mfxExtBuffer* buf[1];
        buf[0] = (mfxExtBuffer*)&m_encpakInit;

        pakParams.NumExtParam = 1;
        pakParams.ExtParam    = buf;
        pakParams.AllocId     = m_EncPakReconAllocID;

        sts = m_pmfxPAK->Init(&pakParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        }

        MSDK_CHECK_STATUS(sts, "m_pmfxPAK->Init failed");
    }

    mfxU32 nEncodedDataBufferSize = m_mfxEncParams.mfx.FrameInfo.Width * m_mfxEncParams.mfx.FrameInfo.Height * 4;
    sts = m_TaskPool.Init(&m_mfxSession, m_FileWriter, m_nAsyncDepth * 2, nEncodedDataBufferSize);
    MSDK_CHECK_STATUS(sts, "m_TaskPool.Init failed");

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::InitSessions()
{
    mfxIMPL impl = MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_VAAPI;

    mfxStatus sts = m_mfxSession.Init(impl, NULL);
    MSDK_CHECK_STATUS(sts, "m_mfxSession.Init failed");

    if (m_bSeparatePreENCSession)
    {
        sts = m_preenc_mfxSession.Init(impl, NULL);
        MSDK_CHECK_STATUS(sts, "m_preenc_mfxSession.Init");
    }

    return sts;
}

mfxStatus CEncodingPipeline::ResetSessions()
{
    mfxStatus sts = m_mfxSession.Close();
    MSDK_CHECK_STATUS(sts, "m_mfxSession.Close failed");

    if (m_bSeparatePreENCSession)
    {
        sts = m_preenc_mfxSession.Close();
        MSDK_CHECK_STATUS(sts, "m_preenc_mfxSession.Close failed");
    }

    sts = InitSessions();
    MSDK_CHECK_STATUS(sts, "InitSessions failed");

    return sts;
}

mfxStatus CEncodingPipeline::AllocateSufficientBuffer(mfxBitstream* pBS)
{
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pmfxENCODE, MFX_ERR_NOT_INITIALIZED);

    mfxVideoParam par;
    MSDK_ZERO_MEMORY(par);

    // find out the required buffer size
    mfxStatus sts = m_pmfxENCODE->GetVideoParam(&par);
    MSDK_CHECK_STATUS(sts, "m_pmfxENCODE->GetVideoParam failed");

    // reallocate bigger buffer for output
    sts = ExtendMfxBitstream(pBS, par.mfx.BufferSizeInKB * 1000);
    MSDK_CHECK_STATUS_SAFE(sts, "ExtendMfxBitstream failed", WipeMfxBitstream(pBS));

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
        MSDK_CHECK_STATUS(sts, "SynchronizeFirstTask failed");

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

    bool adjust_last_B = !(gopOptFlag & MFX_GOP_STRICT) && m_appCfg.nNumFrames && (frameOrder + 1) >= m_appCfg.nNumFrames;
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

mfxStatus CEncodingPipeline::SynchronizeFirstTask()
{
    mfxStatus sts = m_TaskPool.SynchronizeFirstTask();

    return sts;
}

mfxStatus CEncodingPipeline::InitInterfaces()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_appCfg.bDECODESTREAMOUT)
    {
        m_StreamoutBufs.reserve(m_DecResponse.NumFrameActual);
        for (int i = 0; i < m_DecResponse.NumFrameActual; i++) // alloc a streamout buffer per decoder surface
        {
            m_pExtBufDecodeStreamout = new mfxExtFeiDecStreamOut;
            MSDK_CHECK_POINTER(m_pExtBufDecodeStreamout, MFX_ERR_MEMORY_ALLOC);
            MSDK_ZERO_MEMORY(*m_pExtBufDecodeStreamout);

            m_pExtBufDecodeStreamout->Header.BufferId = MFX_EXTBUFF_FEI_DEC_STREAM_OUT;
            m_pExtBufDecodeStreamout->Header.BufferSz = sizeof(mfxExtFeiDecStreamOut);
            m_pExtBufDecodeStreamout->PicStruct   = m_mfxEncParams.mfx.FrameInfo.PicStruct;
            m_pExtBufDecodeStreamout->RemapRefIdx = MFX_CODINGOPTION_ON; /* turn on refIdx remapping in library */
            m_pExtBufDecodeStreamout->NumMBAlloc  = (m_mfxDecParams.mfx.FrameInfo.Width * m_mfxDecParams.mfx.FrameInfo.Height) >> 8; /* streamout uses one buffer to store info about both fields */

            m_pExtBufDecodeStreamout->MB = new mfxFeiDecStreamOutMBCtrl[m_pExtBufDecodeStreamout->NumMBAlloc];
            MSDK_CHECK_POINTER(m_pExtBufDecodeStreamout->MB, MFX_ERR_MEMORY_ALLOC);
            MSDK_ZERO_ARRAY(m_pExtBufDecodeStreamout->MB, m_pExtBufDecodeStreamout->NumMBAlloc);

            m_StreamoutBufs.push_back(m_pExtBufDecodeStreamout);

            /* attach created buffer to decoder surface */
            m_pDecSurfaces.SurfacesPool[i].Data.ExtParam    = (mfxExtBuffer**)(&(m_StreamoutBufs[i]));
            m_pDecSurfaces.SurfacesPool[i].Data.NumExtParam = 1;
        }

        if (m_appCfg.decodestreamoutFile)
        {
            MSDK_FOPEN(decodeStreamout, m_appCfg.decodestreamoutFile, MSDK_CHAR("wb"));
            if (decodeStreamout == NULL) {
                mdprintf(stderr, "Can't open file %s\n", m_appCfg.decodestreamoutFile);
                exit(-1);
            }
        }
    }

    mfxU32 fieldId = 0;
    int numExtInParams = 0, numExtInParamsI = 0, numExtOutParams = 0, numExtOutParamsI = 0;

    if (m_appCfg.bPREENC)
    {
        //setup control structures
        m_disableMVoutPreENC  = (m_appCfg.mvoutFile     == NULL) && !(m_appCfg.bENCODE || m_appCfg.bENCPAK || m_appCfg.bOnlyENC);
        m_disableMBStatPreENC = (m_appCfg.mbstatoutFile == NULL) && !(m_appCfg.bENCODE || m_appCfg.bENCPAK || m_appCfg.bOnlyENC);
        m_enableMVpredPreENC  = m_appCfg.mvinFile != NULL;
        bool enableMBQP       = m_appCfg.mbQpFile != NULL        && !(m_appCfg.bENCODE || m_appCfg.bENCPAK || m_appCfg.bOnlyENC);

        bufSet*                      tmpForInit = NULL;
        mfxExtFeiPreEncCtrl*         preENCCtr  = NULL;
        mfxExtFeiPreEncMVPredictors* mvPreds    = NULL;
        mfxExtFeiEncQP*              qps        = NULL;
        mfxExtFeiPreEncMV*           mvs        = NULL;
        mfxExtFeiPreEncMBStat*       mbdata     = NULL;

        int num_buffers = m_maxQueueLength + (m_appCfg.bDECODE ? this->m_decodePoolSize : 0) + (m_pmfxVPP ? 2 : 0) + 4;
        num_buffers = (std::max)(num_buffers, m_maxQueueLength*m_appCfg.numRef);

        for (int k = 0; k < num_buffers; k++)
        {
            numExtInParams   = 0;
            numExtInParamsI  = 0;
            numExtOutParams  = 0;
            numExtOutParamsI = 0;

            tmpForInit = new bufSet;
            MSDK_CHECK_POINTER(tmpForInit, MFX_ERR_MEMORY_ALLOC);

            for (fieldId = 0; fieldId < m_numOfFields; fieldId++)
            {
                mfxU32 numMB = m_numMBpreenc;
                if (fieldId == 0){
                    preENCCtr = new mfxExtFeiPreEncCtrl[m_numOfFields];
                    MSDK_CHECK_POINTER(preENCCtr, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(preENCCtr, m_numOfFields);

                    if (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN)
                    {
                        // allocate "frame-size" buffers for the first field to re-use them for frames
                        numMB = m_numMBpreenc_frame;
                    }
                }
                numExtInParams++;
                numExtInParamsI++;

                preENCCtr[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_CTRL;
                preENCCtr[fieldId].Header.BufferSz = sizeof(mfxExtFeiPreEncCtrl);
                preENCCtr[fieldId].PictureType             = GetCurPicType(fieldId);
                preENCCtr[fieldId].RefPictureType[0]       = preENCCtr[fieldId].PictureType;
                preENCCtr[fieldId].RefPictureType[1]       = preENCCtr[fieldId].PictureType;
                preENCCtr[fieldId].DownsampleInput         = MFX_CODINGOPTION_ON;  // Default is ON
                preENCCtr[fieldId].DownsampleReference[0]  = MFX_CODINGOPTION_OFF; // In sample_fei preenc works only in encoded order
                preENCCtr[fieldId].DownsampleReference[1]  = MFX_CODINGOPTION_OFF; // so all references would be already downsampled
                preENCCtr[fieldId].DisableMVOutput         = m_disableMVoutPreENC;
                preENCCtr[fieldId].DisableStatisticsOutput = m_disableMBStatPreENC;
                preENCCtr[fieldId].FTEnable                = m_appCfg.FTEnable;
                preENCCtr[fieldId].AdaptiveSearch          = m_appCfg.AdaptiveSearch;
                preENCCtr[fieldId].LenSP                   = m_appCfg.LenSP;
                preENCCtr[fieldId].MBQp                    = enableMBQP;
                preENCCtr[fieldId].MVPredictor             = m_enableMVpredPreENC;
                preENCCtr[fieldId].RefHeight               = m_appCfg.RefHeight;
                preENCCtr[fieldId].RefWidth                = m_appCfg.RefWidth;
                preENCCtr[fieldId].SubPelMode              = m_appCfg.SubPelMode;
                preENCCtr[fieldId].SearchWindow            = m_appCfg.SearchWindow;
                preENCCtr[fieldId].SearchPath              = m_appCfg.SearchPath;
                preENCCtr[fieldId].Qp                      = m_appCfg.QP;
                preENCCtr[fieldId].IntraSAD                = m_appCfg.IntraSAD;
                preENCCtr[fieldId].InterSAD                = m_appCfg.InterSAD;
                preENCCtr[fieldId].SubMBPartMask           = m_appCfg.SubMBPartMask;
                preENCCtr[fieldId].IntraPartMask           = m_appCfg.IntraPartMask;
                preENCCtr[fieldId].Enable8x8Stat           = m_appCfg.Enable8x8Stat;

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
                    mvPreds[fieldId].NumMBAlloc = numMB;
                    mvPreds[fieldId].MB = new mfxExtFeiPreEncMVPredictors::mfxExtFeiPreEncMVPredictorsMB[numMB];
                    MSDK_CHECK_POINTER(mvPreds[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(mvPreds[fieldId].MB, numMB);

                    if ((pMvPred == NULL) && (m_appCfg.mvinFile != NULL))
                    {
                        //read mvs from file
                        printf("Using MV input file: %s\n", m_appCfg.mvinFile);
                        MSDK_FOPEN(pMvPred, m_appCfg.mvinFile, MSDK_CHAR("rb"));
                        if (pMvPred == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_appCfg.mvinFile);
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

                    qps[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_QP;
                    qps[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncQP);
                    qps[fieldId].NumQPAlloc = numMB;
                    qps[fieldId].QP = new mfxU8[numMB];
                    MSDK_CHECK_POINTER(qps[fieldId].QP, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(qps[fieldId].QP, numMB);

                    if ((pPerMbQP == NULL) && (m_appCfg.mbQpFile != NULL))
                    {
                        //read mvs from file
                        printf("Using QP input file: %s\n", m_appCfg.mbQpFile);
                        MSDK_FOPEN(pPerMbQP, m_appCfg.mbQpFile, MSDK_CHAR("rb"));
                        if (pPerMbQP == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_appCfg.mbQpFile);
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
                    mvs[fieldId].NumMBAlloc = numMB;
                    mvs[fieldId].MB = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB[numMB];
                    MSDK_CHECK_POINTER(mvs[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(mvs[fieldId].MB, numMB);

                    if ((mvout == NULL) && (m_appCfg.mvoutFile != NULL) &&
                        !(m_appCfg.bENCODE || m_appCfg.bENCPAK || m_appCfg.bOnlyENC))
                    {
                        printf("Using MV output file: %s\n", m_appCfg.mvoutFile);
                        m_tmpMBpreenc = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB;
                        memset(m_tmpMBpreenc, 0x8000, sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB));
                        MSDK_FOPEN(mvout, m_appCfg.mvoutFile, MSDK_CHAR("wb"));
                        if (mvout == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_appCfg.mvoutFile);
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
                    mbdata[fieldId].NumMBAlloc = numMB;
                    mbdata[fieldId].MB = new mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB[numMB];
                    MSDK_CHECK_POINTER(mbdata[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(mbdata[fieldId].MB, numMB);

                    if ((mbstatout == NULL) && (m_appCfg.mbstatoutFile != NULL))
                    {
                        printf("Using MB distortion output file: %s\n", m_appCfg.mbstatoutFile);
                        MSDK_FOPEN(mbstatout, m_appCfg.mbstatoutFile, MSDK_CHAR("wb"));
                        if (mbstatout == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_appCfg.mbstatoutFile);
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
            if (m_enableMVpredPreENC){
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
    } // if (m_appCfg.bPREENC)

    if ((m_appCfg.bENCODE) || (m_appCfg.bENCPAK) ||
        (m_appCfg.bOnlyPAK) || (m_appCfg.bOnlyENC))
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
        mfxExtFeiRepackCtrl*      feiRepack          = NULL;

        if (m_appCfg.bENCODE)
        {
            m_ctr = new mfxEncodeCtrl;
            MSDK_CHECK_POINTER(m_ctr, MFX_ERR_MEMORY_ALLOC);
            MSDK_ZERO_MEMORY(*m_ctr);
            m_ctr->FrameType = MFX_FRAMETYPE_UNKNOWN;
            m_ctr->QP = m_appCfg.QP;
        }

        bool MVPredictors = (m_appCfg.mvinFile   != NULL) || m_appCfg.bPREENC; //couple with PREENC
        bool MBCtrl       = m_appCfg.mbctrinFile != NULL;
        bool MBQP         = m_appCfg.mbQpFile    != NULL;

        bool MBStatOut    = m_appCfg.mbstatoutFile  != NULL;
        bool MVOut        = m_appCfg.mvoutFile      != NULL;
        bool MBCodeOut    = m_appCfg.mbcodeoutFile  != NULL;
        bool RepackCtrl   = m_appCfg.repackctrlFile != NULL;

        bool nonDefDblk = (m_appCfg.DisableDeblockingIdc || m_appCfg.SliceAlphaC0OffsetDiv2 ||
            m_appCfg.SliceBetaOffsetDiv2) && !m_appCfg.bENCODE;

        /*SPS Header */
        if (m_appCfg.bENCPAK || m_appCfg.bOnlyENC || m_appCfg.bOnlyPAK)
        {
            feiSPS = new mfxExtFeiSPS;
            MSDK_CHECK_POINTER(feiSPS, MFX_ERR_MEMORY_ALLOC);
            MSDK_ZERO_MEMORY(*feiSPS);
            feiSPS->Header.BufferId = MFX_EXTBUFF_FEI_SPS;
            feiSPS->Header.BufferSz = sizeof(mfxExtFeiSPS);
            feiSPS->SPSId   = 0;
            feiSPS->Profile = m_appCfg.CodecProfile;
            feiSPS->Level   = m_appCfg.CodecLevel;

            feiSPS->NumRefFrame = m_mfxEncParams.mfx.NumRefFrame;

            feiSPS->ChromaFormatIdc  = m_mfxEncParams.mfx.FrameInfo.ChromaFormat;
            feiSPS->FrameMBsOnlyFlag = (m_mfxEncParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 0;
            feiSPS->MBAdaptiveFrameFieldFlag = 0;
            feiSPS->Direct8x8InferenceFlag   = 1;
            feiSPS->Log2MaxFrameNum = 8;
            feiSPS->PicOrderCntType = GetDefaultPicOrderCount(m_mfxEncParams);
            feiSPS->Log2MaxPicOrderCntLsb       = 4;
            feiSPS->DeltaPicOrderAlwaysZeroFlag = 1;
        }

        int num_buffers = m_maxQueueLength + (m_appCfg.bDECODE ? this->m_decodePoolSize : 0) + (m_pmfxVPP ? 2 : 0);

        bool is_8x8tranform_forced = false;
        for (int k = 0; k < num_buffers; k++)
        {
            tmpForInit = new bufSet;
            MSDK_CHECK_POINTER(tmpForInit, MFX_ERR_MEMORY_ALLOC);

            numExtInParams   = (m_appCfg.bENCPAK || m_appCfg.bOnlyENC || m_appCfg.bOnlyPAK) ? 1 : 0; // count SPS header
            numExtInParamsI  = (m_appCfg.bENCPAK || m_appCfg.bOnlyENC || m_appCfg.bOnlyPAK) ? 1 : 0; // count SPS header
            numExtOutParams  = 0;
            numExtOutParamsI = 0;

            for (fieldId = 0; fieldId < m_numOfFields; fieldId++)
            {
                mfxU16 numMB = m_numMB;
                if (fieldId == 0){
                    feiEncCtrl = new mfxExtFeiEncFrameCtrl[m_numOfFields];
                    MSDK_CHECK_POINTER(feiEncCtrl, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncCtrl, m_numOfFields);
                    if (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN)
                    {
                        // allocate "frame-size" buffers for the first field to re-use them for frames
                        numMB = m_numMB_frame;
                    }
                }
                numExtInParams++;
                numExtInParamsI++;

                feiEncCtrl[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
                feiEncCtrl[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);

                feiEncCtrl[fieldId].SearchPath             = m_appCfg.SearchPath;
                feiEncCtrl[fieldId].LenSP                  = m_appCfg.LenSP;
                feiEncCtrl[fieldId].SubMBPartMask          = m_appCfg.SubMBPartMask;
                feiEncCtrl[fieldId].MultiPredL0            = m_appCfg.MultiPredL0;
                feiEncCtrl[fieldId].MultiPredL1            = m_appCfg.MultiPredL1;
                feiEncCtrl[fieldId].SubPelMode             = m_appCfg.SubPelMode;
                feiEncCtrl[fieldId].InterSAD               = m_appCfg.InterSAD;
                feiEncCtrl[fieldId].IntraSAD               = m_appCfg.IntraSAD;
                feiEncCtrl[fieldId].IntraPartMask          = m_appCfg.IntraPartMask;
                feiEncCtrl[fieldId].DistortionType         = m_appCfg.DistortionType;
                feiEncCtrl[fieldId].RepartitionCheckEnable = m_appCfg.RepartitionCheckEnable;
                feiEncCtrl[fieldId].AdaptiveSearch         = m_appCfg.AdaptiveSearch;
                feiEncCtrl[fieldId].MVPredictor            = MVPredictors;

                mfxU16 nmvp_l0 = m_appCfg.bNPredSpecified_l0 ?
                    m_appCfg.NumMVPredictors[0] : (std::min)(static_cast<mfxU16>(m_mfxEncParams.mfx.NumRefFrame*m_numOfFields), MaxFeiEncMVPNum);
                mfxU16 nmvp_l1 = m_appCfg.bNPredSpecified_l1 ?
                    m_appCfg.NumMVPredictors[1] : (std::min)(static_cast<mfxU16>(m_mfxEncParams.mfx.NumRefFrame*m_numOfFields), MaxFeiEncMVPNum);

                feiEncCtrl[fieldId].NumMVPredictors[0] = m_numOfRefs[fieldId][0] = nmvp_l0;
                feiEncCtrl[fieldId].NumMVPredictors[1] = m_numOfRefs[fieldId][1] = nmvp_l1;
                feiEncCtrl[fieldId].PerMBQp                = MBQP;
                feiEncCtrl[fieldId].PerMBInput             = MBCtrl;
                feiEncCtrl[fieldId].MBSizeCtrl             = m_appCfg.bMBSize;
                feiEncCtrl[fieldId].ColocatedMbDistortion  = m_appCfg.ColocatedMbDistortion;
                //Note:
                //(RefHeight x RefWidth) should not exceed 2048 for P frames and 1024 for B frames
                feiEncCtrl[fieldId].RefHeight    = m_appCfg.RefHeight;
                feiEncCtrl[fieldId].RefWidth     = m_appCfg.RefWidth;
                feiEncCtrl[fieldId].SearchWindow = m_appCfg.SearchWindow;

                /* PPS Header */
                if (m_appCfg.bENCPAK || m_appCfg.bOnlyENC || m_appCfg.bOnlyPAK)
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

                    //feiPPS[fieldId].PicInitQP = (m_appCfg.QP != 0) ? m_appCfg.QP : 26;
                    /* PicInitQP should be always 26 !!!
                     * Adjusting of QP parameter should be done via Slice header */
                    feiPPS[fieldId].PicInitQP = 26;

                    feiPPS[fieldId].NumRefIdxL0Active = 1;
                    feiPPS[fieldId].NumRefIdxL1Active = 1;

                    feiPPS[fieldId].ChromaQPIndexOffset       = m_appCfg.ChromaQPIndexOffset;
                    feiPPS[fieldId].SecondChromaQPIndexOffset = m_appCfg.SecondChromaQPIndexOffset;

                    feiPPS[fieldId].IDRPicFlag                = 0;
                    feiPPS[fieldId].ReferencePicFlag          = 0;
                    feiPPS[fieldId].EntropyCodingModeFlag     = 1;
                    feiPPS[fieldId].ConstrainedIntraPredFlag  = m_appCfg.ConstrainedIntraPredFlag;
                    feiPPS[fieldId].Transform8x8ModeFlag      = m_appCfg.Transform8x8ModeFlag;
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
                if (m_appCfg.bENCPAK || m_appCfg.bOnlyENC || m_appCfg.bOnlyPAK || nonDefDblk)
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
                        feiSliceHeader[fieldId].NumSliceAlloc = m_appCfg.numSlices;
                    feiSliceHeader[fieldId].Slice = new mfxExtFeiSliceHeader::mfxSlice[feiSliceHeader[fieldId].NumSliceAlloc];
                    MSDK_CHECK_POINTER(feiSliceHeader[fieldId].Slice, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiSliceHeader[fieldId].Slice, feiSliceHeader[fieldId].NumSliceAlloc);

                    // TODO: Implement real slice divider
                    // For now only one slice is supported
                    mfxU16 nMBrows = (m_heightMB + feiSliceHeader[fieldId].NumSlice - 1) / feiSliceHeader[fieldId].NumSlice,
                        nMBremain = m_heightMB;
                    for (mfxU16 numSlice = 0; numSlice < feiSliceHeader[fieldId].NumSliceAlloc; numSlice++)
                    {
                        feiSliceHeader[fieldId].Slice[numSlice].MBAaddress = numSlice*(nMBrows*m_widthMB);
                        feiSliceHeader[fieldId].Slice[numSlice].NumMBs     = MSDK_MIN(nMBrows, nMBremain)*m_widthMB;
                        feiSliceHeader[fieldId].Slice[numSlice].SliceType  = 0;
                        feiSliceHeader[fieldId].Slice[numSlice].PPSId      = feiPPS ? feiPPS[fieldId].PPSId : 0;
                        feiSliceHeader[fieldId].Slice[numSlice].IdrPicId   = 0;

                        feiSliceHeader[fieldId].Slice[numSlice].CabacInitIdc = 0;
                        mfxU32 initQP = (m_appCfg.QP != 0) ? m_appCfg.QP : 26;
                        feiSliceHeader[fieldId].Slice[numSlice].SliceQPDelta               = (mfxI16)(initQP - feiPPS[fieldId].PicInitQP);
                        feiSliceHeader[fieldId].Slice[numSlice].DisableDeblockingFilterIdc = m_appCfg.DisableDeblockingIdc;
                        feiSliceHeader[fieldId].Slice[numSlice].SliceAlphaC0OffsetDiv2     = m_appCfg.SliceAlphaC0OffsetDiv2;
                        feiSliceHeader[fieldId].Slice[numSlice].SliceBetaOffsetDiv2        = m_appCfg.SliceBetaOffsetDiv2;

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
                    feiEncMVPredictors[fieldId].NumMBAlloc = numMB;
                    feiEncMVPredictors[fieldId].MB = new mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB[numMB];
                    MSDK_CHECK_POINTER(feiEncMVPredictors[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMVPredictors[fieldId].MB, numMB);

                    if (!m_appCfg.bPREENC && (pMvPred == NULL) && (m_appCfg.mvinFile != NULL)) //not load if we couple with PREENC
                    {
                        printf("Using MV input file: %s\n", m_appCfg.mvinFile);
                        MSDK_FOPEN(pMvPred, m_appCfg.mvinFile, MSDK_CHAR("rb"));
                        if (pMvPred == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_appCfg.mvinFile);
                            exit(-1);
                        }
                        if (m_appCfg.bRepackPreencMV){
                            m_tmpForMedian  = new mfxI16[16];
                            MSDK_CHECK_POINTER(m_tmpForMedian, MFX_ERR_MEMORY_ALLOC);
                            MSDK_ZERO_ARRAY(m_tmpForMedian, 16);
                            m_tmpForReading = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB[numMB];
                            MSDK_CHECK_POINTER(m_tmpForReading, MFX_ERR_MEMORY_ALLOC);
                            MSDK_ZERO_ARRAY(m_tmpForReading, numMB);
                        }
                    }
                    else {
                        m_tmpForMedian = new mfxI16[16];
                        MSDK_CHECK_POINTER(m_tmpForMedian, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(m_tmpForMedian, 16);
                    }
                }

                if (RepackCtrl)
                {
                    if (fieldId == 0){
                        feiRepack = new mfxExtFeiRepackCtrl[m_numOfFields];
                        MSDK_CHECK_POINTER(feiRepack, MFX_ERR_MEMORY_ALLOC);
                        MSDK_ZERO_ARRAY(feiRepack, m_numOfFields);
                    }
                    numExtInParams++;
                    numExtInParamsI++;

                    feiRepack[fieldId].Header.BufferId =  MFX_EXTBUFF_FEI_REPACK_CTRL;
                    feiRepack[fieldId].Header.BufferSz = sizeof(mfxExtFeiRepackCtrl);
                    feiRepack[fieldId].MaxFrameSize = 0;
                    feiRepack[fieldId].NumPasses = 1;
                    MSDK_ZERO_ARRAY(feiRepack[fieldId].DeltaQP, 8);
                    if ((pRepackCtrl == NULL) && (m_appCfg.repackctrlFile != NULL))
                    {
                        printf("Using Frame Size control input file: %s\n", m_appCfg.repackctrlFile);
                        MSDK_FOPEN(pRepackCtrl, m_appCfg.repackctrlFile, MSDK_CHAR("rb"));
                        if (pRepackCtrl == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_appCfg.repackctrlFile);
                            exit(-1);
                        }
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
                    feiEncMBCtrl[fieldId].NumMBAlloc = numMB;
                    feiEncMBCtrl[fieldId].MB = new mfxExtFeiEncMBCtrl::mfxExtFeiEncMBCtrlMB[numMB];
                    MSDK_CHECK_POINTER(feiEncMBCtrl[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMBCtrl[fieldId].MB, numMB);

                    if ((pEncMBs == NULL) && (m_appCfg.mbctrinFile != NULL))
                    {
                        printf("Using MB control input file: %s\n", m_appCfg.mbctrinFile);
                        MSDK_FOPEN(pEncMBs, m_appCfg.mbctrinFile, MSDK_CHAR("rb"));
                        if (pEncMBs == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_appCfg.mbctrinFile);
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

                    feiEncMbQp[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_QP;
                    feiEncMbQp[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncQP);
                    feiEncMbQp[fieldId].NumQPAlloc = numMB;
                    feiEncMbQp[fieldId].QP = new mfxU8[numMB];
                    MSDK_CHECK_POINTER(feiEncMbQp[fieldId].QP, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMbQp[fieldId].QP, numMB);

                    if ((pPerMbQP == NULL) && (m_appCfg.mbQpFile != NULL))
                    {
                        printf("Use MB QP input file: %s\n", m_appCfg.mbQpFile);
                        MSDK_FOPEN(pPerMbQP, m_appCfg.mbQpFile, MSDK_CHAR("rb"));
                        if (pPerMbQP == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_appCfg.mbQpFile);
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
                    feiEncMbStat[fieldId].NumMBAlloc = numMB;
                    feiEncMbStat[fieldId].MB = new mfxExtFeiEncMBStat::mfxExtFeiEncMBStatMB[numMB];
                    MSDK_CHECK_POINTER(feiEncMbStat[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMbStat[fieldId].MB, numMB);

                    if ((mbstatout == NULL) && (m_appCfg.mbstatoutFile != NULL))
                    {
                        printf("Use MB distortion output file: %s\n", m_appCfg.mbstatoutFile);
                        MSDK_FOPEN(mbstatout, m_appCfg.mbstatoutFile, MSDK_CHAR("wb"));
                        if (mbstatout == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_appCfg.mbstatoutFile);
                            exit(-1);
                        }
                    }
                }

                if ((MVOut) || (m_appCfg.bENCPAK) || (m_appCfg.bOnlyPAK))
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
                    feiEncMV[fieldId].NumMBAlloc = numMB;
                    feiEncMV[fieldId].MB = new mfxExtFeiEncMV::mfxExtFeiEncMVMB[numMB];
                    MSDK_CHECK_POINTER(feiEncMV[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMV[fieldId].MB, numMB);

                    if ((mvENCPAKout == NULL) && (NULL != m_appCfg.mvoutFile))
                    {
                        printf("Use MV output file: %s\n", m_appCfg.mvoutFile);
                        if (m_appCfg.bOnlyPAK)
                            MSDK_FOPEN(mvENCPAKout, m_appCfg.mvoutFile, MSDK_CHAR("rb"));
                        else { /*for all other cases need to write into this file*/
                            MSDK_FOPEN(mvENCPAKout, m_appCfg.mvoutFile, MSDK_CHAR("wb"));
                            m_tmpMBenc = new mfxExtFeiEncMV::mfxExtFeiEncMVMB;
                            memset(m_tmpMBenc, 0x8000, sizeof(mfxExtFeiEncMV::mfxExtFeiEncMVMB));
                        }
                        if (mvENCPAKout == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_appCfg.mvoutFile);
                            exit(-1);
                        }
                    }
                }

                //distortion buffer have to be always provided - current limitation
                if ((MBCodeOut) || (m_appCfg.bENCPAK) || (m_appCfg.bOnlyPAK))
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
                    feiEncMBCode[fieldId].NumMBAlloc = numMB;
                    feiEncMBCode[fieldId].MB = new mfxFeiPakMBCtrl[numMB];
                    MSDK_CHECK_POINTER(feiEncMBCode[fieldId].MB, MFX_ERR_MEMORY_ALLOC);
                    MSDK_ZERO_ARRAY(feiEncMBCode[fieldId].MB, numMB);

                    if ((mbcodeout == NULL) && (NULL != m_appCfg.mbcodeoutFile))
                    {
                        printf("Use MB code output file: %s\n", m_appCfg.mbcodeoutFile);
                        if (m_appCfg.bOnlyPAK)
                            MSDK_FOPEN(mbcodeout, m_appCfg.mbcodeoutFile, MSDK_CHAR("rb"));
                        else
                            MSDK_FOPEN(mbcodeout, m_appCfg.mbcodeoutFile, MSDK_CHAR("wb"));

                        if (mbcodeout == NULL) {
                            mdprintf(stderr, "Can't open file %s\n", m_appCfg.mbcodeoutFile);
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
            if (RepackCtrl){
                for (fieldId = 0; fieldId < m_numOfFields; fieldId++){
                    tmpForInit-> I_bufs.in.ExtParam[tmpForInit-> I_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiRepack[fieldId]);
                    tmpForInit->PB_bufs.in.ExtParam[tmpForInit->PB_bufs.in.NumExtParam++] = (mfxExtBuffer*)(&feiRepack[fieldId]);
                }
            }

            m_encodeBufs.push_back(tmpForInit);

        } // for (int k = 0; k < num_buffers; k++)

        if (is_8x8tranform_forced){
            msdk_printf(MSDK_STRING("\nWARNING: Transform8x8ModeFlag enforced!\n"));
            msdk_printf(MSDK_STRING("           Reason: IntraPartMask = %u, does not disable partitions below 16x16\n"), m_appCfg.IntraPartMask);
        }

    } // if (m_appCfg.bENCODE)

    return sts;
}

mfxStatus CEncodingPipeline::Run()
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxFrameSurface1* pSurf = NULL; // dispatching pointer

    // init parameters
    sts = InitInterfaces();
    MSDK_CHECK_STATUS(sts, "InitInterfaces failed");

    bool create_task = (m_appCfg.bPREENC)  || (m_appCfg.bENCPAK)  ||
                       (m_appCfg.bOnlyENC) || (m_appCfg.bOnlyPAK) ||
                       (m_appCfg.bENCODE && m_appCfg.EncodedOrder);

    bool swap_fields = false;

    iTask* eTask = NULL; // encoding task
    time_t start = time(0);
    size_t rctime = 0;

    sts = MFX_ERR_NONE;

    // main loop, preprocessing and encoding
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)
    {
        if ((m_appCfg.nNumFrames && m_frameCount >= m_appCfg.nNumFrames)
            || (m_appCfg.nTimeout && (time(0) - start >= m_appCfg.nTimeout)))
        {
            break;
        }

        sts = GetOneFrame(pSurf);
        if ((sts == MFX_ERR_MORE_DATA && m_appCfg.nTimeout) || // New cycle in loop mode
            (sts == MFX_ERR_GPU_HANG))                         // New cycle if GPU Recovered
        {
            sts = MFX_ERR_NONE;
            continue;
        }
        MSDK_BREAK_ON_ERROR(sts);

        if (m_bNeedDRC)
        {
           sts = ResizeFrame(m_frameCount, rctime);
        }

        if (m_insertIDR)
        {
            m_insertIDR = false;

            if (NULL != m_ctr)
            {
                bool isField = (m_appCfg.nPicStruct != MFX_PICSTRUCT_UNKNOWN) ? !!m_isField : !(pSurf->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE);

                m_ctr->FrameType = isField ? (MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_xREF)
                                           : (MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF);
            }
            m_frameType = PairU8((mfxU8)(MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF),
                                 (mfxU8)(MFX_FRAMETYPE_P                     | MFX_FRAMETYPE_REF));
        }
        else
        {
            if (NULL != m_ctr)
                m_ctr->FrameType = MFX_FRAMETYPE_UNKNOWN;

            m_frameType = GetFrameType(m_frameCount - m_frameOrderIdrInDisplayOrder);
        }

        swap_fields = m_appCfg.nPicStruct == MFX_PICSTRUCT_FIELD_BFF ||
                     (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN      &&
                      (pSurf->Info.PicStruct & MFX_PICSTRUCT_FIELD_BFF) &&
                     !(pSurf->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE));

        if (swap_fields)
            std::swap(m_frameType[0], m_frameType[1]);

        if (m_frameType[m_ffid] & MFX_FRAMETYPE_IDR)
            m_frameOrderIdrInDisplayOrder = m_frameCount;

        pSurf->Data.FrameOrder = m_frameCount; // in display order

        m_frameCount++;

        if (m_pmfxVPP)
        {
            // pre-process a frame
            sts = PreProcessOneFrame(pSurf);
            if (sts == MFX_ERR_GPU_HANG) { continue; }
            MSDK_BREAK_ON_ERROR(sts);
        }

        /* Reorder income frame */
        eTask = NULL;
        if (create_task)
        {
            /* ENC and/or PAK */
            if (m_appCfg.bENCPAK || m_appCfg.bOnlyENC || m_appCfg.bOnlyPAK)
            {
                /* FEI PAK requires separate pool for reconstruct surfaces */
                mfxFrameSurface1* pReconSurf = m_pReconSurfaces.GetFreeSurface_FEI();
                MSDK_CHECK_POINTER(pReconSurf, MFX_ERR_MEMORY_ALLOC);
                m_taskInitializationParams.ReconSurf = pReconSurf;

                // get a pointer to a free task (bit stream and sync point for encoder)
                sTask *pSyncTask;
                sts = GetFreeTask(&pSyncTask);
                MSDK_CHECK_STATUS(sts, "Failed to get free sTask");
                m_taskInitializationParams.BitStream = &pSyncTask->mfxBS;
            }

            mfxU16 ps = (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN) ? (pSurf->Info.PicStruct & 0xf) : m_mfxEncParams.mfx.FrameInfo.PicStruct;
            m_taskInitializationParams.InputSurf  = pSurf;
            m_taskInitializationParams.PicStruct  = ps;
            m_taskInitializationParams.FrameType  = m_frameType;
            m_taskInitializationParams.FrameCount = m_frameCount - 1;
            m_taskInitializationParams.FrameOrderIdrInDisplayOrder = m_frameOrderIdrInDisplayOrder;

            eTask = new iTask(m_taskInitializationParams);

            m_inputTasks.AddTask(eTask);
            eTask = m_inputTasks.GetTaskToEncode(false);

            if (!eTask) continue;
        }

        if (m_appCfg.bPREENC)
        {
            sts = PreencOneFrame(eTask);
            if (sts == MFX_ERR_GPU_HANG) { continue; }
            MSDK_BREAK_ON_ERROR(sts);
        }

        if ((m_appCfg.bENCPAK) || (m_appCfg.bOnlyPAK) || (m_appCfg.bOnlyENC))
        {
            sts = EncPakOneFrame(eTask);
            if (sts == MFX_ERR_GPU_HANG) { continue; }
            MSDK_BREAK_ON_ERROR(sts);
        }

        if (m_appCfg.bENCODE)
        {
            sts = EncodeOneFrame(eTask, pSurf);
            if (sts == MFX_ERR_GPU_HANG) { continue; }
            MSDK_BREAK_ON_ERROR(sts);
        }

        m_inputTasks.UpdatePool();

    } // while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)

    // means that the input file has ended, need to go to buffering loops
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_STATUS(sts, "Unexpected error!!");


    // loop to get buffered frames from encoder

    if (create_task)
    {
        mfxU32 numUnencoded = m_inputTasks.CountUnencodedFrames();

        if (numUnencoded)
        {
            m_inputTasks.ProcessLastB();
        }

        while (numUnencoded != 0)
        {
            eTask = m_inputTasks.GetTaskToEncode(true);

            if (m_appCfg.bPREENC)
            {
                sts = PreencOneFrame(eTask);
                if (sts == MFX_ERR_GPU_HANG) { continue; }
                MSDK_BREAK_ON_ERROR(sts);
            }

            if ((m_appCfg.bENCPAK) || (m_appCfg.bOnlyPAK) || (m_appCfg.bOnlyENC))
            {
                sts = EncPakOneFrame(eTask);
                if (sts == MFX_ERR_GPU_HANG) { continue; }
                MSDK_BREAK_ON_ERROR(sts);
            }

            if (m_appCfg.bENCODE)
            {
                sts = EncodeOneFrame(eTask, NULL);
                if (sts == MFX_ERR_GPU_HANG) { continue; }
                MSDK_BREAK_ON_ERROR(sts);
            }

            m_inputTasks.UpdatePool();

            numUnencoded--;
        }
    }
    else // ENCODE in display order
    {
        if (m_appCfg.bENCODE)
        {
            while (MFX_ERR_NONE <= sts)
            {
                sts = EncodeOneFrame(NULL, NULL);
            }

            // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
            // indicates that there are no more buffered frames
            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
            // exit in case of other errors
            MSDK_CHECK_STATUS(sts, "EncodeOneFrame failed");

            // synchronize all tasks that are left in task pool
            while (MFX_ERR_NONE == sts) {
                sts = m_TaskPool.SynchronizeFirstTask();
            }
        }
    }
    // MFX_ERR_NOT_FOUND is the correct status to exit the loop with
    // EncodeFrameAsync and SyncOperation, so don't return this status
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_FOUND);
    // report any errors that occurred in asynchronous part
    MSDK_CHECK_STATUS(sts, "m_TaskPool.SynchronizeFirstTask failed");

    // release runtime resources
    sts = ReleaseResources();
    MSDK_CHECK_STATUS(sts, "ReleaseResources failed");

    return sts;
}

/* read YUV frame */

mfxStatus CEncodingPipeline::GetOneFrame(mfxFrameSurface1* & pSurf)
{
    MFX_ITT_TASK("GetOneFrame");
    mfxStatus sts = MFX_ERR_NONE;

    if (!m_pmfxDECODE) // load frame from YUV file
    {
        // find free surface for encoder input
        ExtSurfPool & SurfPool = m_pmfxVPP ? m_pVppSurfaces : m_pEncSurfaces;

        // point pSurf to encoder surface
        pSurf = SurfPool.GetFreeSurface_FEI();
        MSDK_CHECK_POINTER(pSurf, MFX_ERR_MEMORY_ALLOC);

        // load frame from file to surface data
        // if we share allocator with Media SDK we need to call Lock to access surface data and...
        if (m_bExternalAlloc)
        {
            // get YUV pointers
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
            MSDK_CHECK_STATUS(sts, "m_pMFXAllocator->Lock failed");
        }

        sts = m_FileReader.LoadNextFrame(pSurf);
        if (sts == MFX_ERR_MORE_DATA && m_appCfg.nTimeout)
        {
            // infinite loop mode, need to proceed from the beginning

            sts = ResetIOFiles(m_appCfg);
            MSDK_CHECK_STATUS(sts, "ResetIOFiles failed");

            m_insertIDR = true;
            return MFX_ERR_MORE_DATA;
        }
        MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, sts);

        // ... after we're done call Unlock
        if (m_bExternalAlloc)
        {
            sts = m_pMFXAllocator->Unlock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
            MSDK_CHECK_STATUS(sts, "m_pMFXAllocator->Unlock failed");
        }
    }
    else // decode frame
    {
        ExtendedSurface DecExtSurface = { 0 };

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

        if (sts == MFX_ERR_GPU_HANG)
        {
            return sts;
        }

        if (sts == MFX_ERR_MORE_DATA)
        {
            if (m_appCfg.nTimeout)
            {
                // infinite loop mode, need to proceed from the beginning
                sts = ResetIOFiles(m_appCfg);
                MSDK_CHECK_STATUS(sts, "ResetIOFiles failed");

                m_insertIDR  = true;
                m_bEndOfFile = false;
                return MFX_ERR_MORE_DATA;
            }
            DecExtSurface.pSurface = NULL;
            return sts;
        }
        MSDK_CHECK_STATUS(sts, "Decode<One|Last>Frame failed");

        pSurf = DecExtSurface.pSurface;

        /* We have to sync before any FEI interface */
        bool sync = m_appCfg.bDECODESTREAMOUT || m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN || !!m_pmfxDS || !!m_pmfxVPP;
        if (sync)
        {
            for (;;)
            {
                sts = m_mfxSession.SyncOperation(DecExtSurface.Syncp, MSDK_WAIT_INTERVAL);

                if (sts == MFX_ERR_GPU_HANG)
                {
                    sts = doGPUHangRecovery();
                    MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
                    return MFX_ERR_GPU_HANG;
                }

                if (MFX_ERR_NONE < sts && !DecExtSurface.Syncp) // repeat the call if warning and no output
                {
                    if (MFX_WRN_DEVICE_BUSY == sts){
                        WaitForDeviceToBecomeFree(m_mfxSession, DecExtSurface.Syncp, sts); // wait if device is busy
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
            MSDK_CHECK_STATUS(sts, "m_mfxSession.SyncOperation failed");

            if (m_appCfg.bDECODESTREAMOUT)
            {
                sts = DropDecodeStreamoutOutput(pSurf);
                MSDK_CHECK_STATUS(sts, "DropDecodeStreamoutOutput failed");
            }
        }

    }

    return sts;
}

mfxStatus CEncodingPipeline::ResetBuffers()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_appCfg.bENCODE)
    {
        // synchronize all tasks that are left in task pool
        while (MFX_ERR_NONE == sts) {
            sts = m_TaskPool.SynchronizeFirstTask();
        }
        sts = MFX_ERR_NONE;
    }

    return sts;
}

mfxStatus CEncodingPipeline::FillRefInfo(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;

    m_ref_info.Clear();

    iTask* ref_task = NULL;
    mfxFrameSurface1* ref_surface = NULL;
    std::vector<mfxFrameSurface1*>::iterator rslt;
    mfxU32 k = 0, fid, n_l0, n_l1;

    for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
    {
        fid = eTask->m_fid[fieldId];
        for (DpbFrame* instance = eTask->m_dpb[fid].Begin(); instance != eTask->m_dpb[fid].End(); instance++)
        {
            ref_task = m_inputTasks.GetTaskByFrameOrder(instance->m_frameOrder);
            MSDK_CHECK_POINTER(ref_task, MFX_ERR_NULL_PTR);

            ref_surface = ref_task->out.OutSurface; // this is shared output surface for ENC reference / PAK reconstruct
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
        n_l0 = eTask->GetNBackward(fieldId);
        n_l1 = eTask->GetNForward(fieldId);

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
            ref_task = m_inputTasks.GetTaskByFrameOrder(eTask->m_dpb[fid][*instance & 127].m_frameOrder);
            MSDK_CHECK_POINTER(ref_task, MFX_ERR_NULL_PTR);

            ref_surface = ref_task->out.OutSurface; // this is shared output surface for ENC reference / PAK reconstruct
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
            ref_task = m_inputTasks.GetTaskByFrameOrder(eTask->m_dpb[fid][*instance & 127].m_frameOrder);
            MSDK_CHECK_POINTER(ref_task, MFX_ERR_NULL_PTR);

            ref_surface = ref_task->out.OutSurface; // this is shared output surface for ENC reference / PAK reconstruct
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

inline mfxU16 CEncodingPipeline::GetCurPicType(mfxU32 fieldId)
{
    if (!m_isField)
        return MFX_PICTYPE_FRAME;
    else
    {
        if (m_mfxEncParams.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_TFF)
            return (mfxU16)(fieldId ? MFX_PICTYPE_BOTTOMFIELD : MFX_PICTYPE_TOPFIELD);
        else
            return (mfxU16)(fieldId ? MFX_PICTYPE_TOPFIELD : MFX_PICTYPE_BOTTOMFIELD);
    }
}

/* initialization functions */

mfxStatus CEncodingPipeline::InitPreEncFrameParamsEx(iTask* eTask, iTask* refTask[2][2], mfxU8 ref_fid[2][2], bool isDownsamplingNeeded)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxExtFeiPreEncMVPredictors* pMvPredBuf = NULL;
    mfxExtFeiEncQP*              pMbQP      = NULL;
    mfxExtFeiPreEncCtrl*         preENCCtr  = NULL;

    mfxFrameSurface1* refSurf0[2] = { NULL, NULL }; // L0 ref surfaces
    mfxFrameSurface1* refSurf1[2] = { NULL, NULL }; // L1 ref surfaces

    eTask->preenc_bufs = getFreeBufSet(m_preencBufs);
    MSDK_CHECK_POINTER(eTask->preenc_bufs, MFX_ERR_NULL_PTR);
    mfxU32 numOfFields = eTask->m_fieldPicFlag ? 2 : 1;

    if (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN)
    {
        mfxStatus sts = ResetExtBufMBnum(eTask->preenc_bufs, eTask->m_fieldPicFlag ? m_numMB : m_numMB_frame, false);
        MSDK_CHECK_STATUS(sts, "ResetExtBufMBnum failed");
    }

    for (mfxU32 fieldId = 0; fieldId < numOfFields; fieldId++)
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
    bool ref0_isFrame = !m_isField;
    bool ref1_isFrame = !m_isField;
    for (int i = 0; i < eTask->in.NumExtParam; i++)
    {
        switch (eTask->in.ExtParam[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_PREENC_CTRL:
            type = ExtractFrameType(*eTask, preENCCtrId); // IP pair is supported
            preENCCtr = (mfxExtFeiPreEncCtrl*)(eTask->in.ExtParam[i]);
            preENCCtr->DisableMVOutput = ((type & MFX_FRAMETYPE_I) || (!refSurf0[preENCCtrId] && !refSurf1[preENCCtrId])) ? 1 : m_disableMVoutPreENC;

            if (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN)
            {
                ref0_isFrame = ref1_isFrame = true;
                if (refSurf0[preENCCtrId])
                    ref0_isFrame = refSurf0[preENCCtrId]->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE;
                if (refSurf1[preENCCtrId])
                    ref1_isFrame = refSurf1[preENCCtrId]->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE;
            }

            preENCCtr->RefPictureType[0] = (mfxU16)(ref0_isFrame ? MFX_PICTYPE_FRAME :
                ref_fid[preENCCtrId][0] == 0 ? MFX_PICTYPE_TOPFIELD : MFX_PICTYPE_BOTTOMFIELD);

            preENCCtr->RefPictureType[1] = (mfxU16)(ref1_isFrame ? MFX_PICTYPE_FRAME :
                ref_fid[preENCCtrId][1] == 0 ? MFX_PICTYPE_TOPFIELD : MFX_PICTYPE_BOTTOMFIELD);

            preENCCtr->RefFrame[0] = refSurf0[preENCCtrId];
            preENCCtr->RefFrame[1] = refSurf1[preENCCtrId];

            if (!isDownsamplingNeeded)
                preENCCtr->DownsampleInput = MFX_CODINGOPTION_OFF;
            else
                preENCCtr->DownsampleInput = MFX_CODINGOPTION_ON; // the default is ON too

            if (m_enableMVpredPreENC)
            {
                preENCCtr->MVPredictor = 0;

                if (m_appCfg.bPreencPredSpecified_l0 && !(type & MFX_FRAMETYPE_I))
                    preENCCtr->MVPredictor |= (0x01 * m_appCfg.PreencMVPredictors[0]);
                else
                    preENCCtr->MVPredictor |= (0x01 * (!!preENCCtr->RefFrame[0]));

                if (m_appCfg.bPreencPredSpecified_l1 && !(type & MFX_FRAMETYPE_I))
                    preENCCtr->MVPredictor |= (0x02 * m_appCfg.PreencMVPredictors[1]);
                else
                    preENCCtr->MVPredictor |= (0x02 * (!!preENCCtr->RefFrame[1]));
            }

            if (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN)
            {
                if (eTask->in.InSurface->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE)
                {
                    preENCCtr->PictureType = MFX_PICTYPE_FRAME;
                }
                else if (eTask->in.InSurface->Info.PicStruct & MFX_PICSTRUCT_FIELD_TFF)
                {
                    preENCCtr->PictureType = mfxU16(!preENCCtrId ? MFX_PICTYPE_TOPFIELD : MFX_PICTYPE_BOTTOMFIELD);
                }
                else if (eTask->in.InSurface->Info.PicStruct & MFX_PICSTRUCT_FIELD_BFF)
                {
                    preENCCtr->PictureType = mfxU16(!preENCCtrId ? MFX_PICTYPE_BOTTOMFIELD : MFX_PICTYPE_TOPFIELD);
                }
            }

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

        case MFX_EXTBUFF_FEI_ENC_QP:
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
            if (!m_appCfg.bPREENC && pMvPred)
            {
                if (!(ExtractFrameType(*eTask, pMvPredId) & MFX_FRAMETYPE_I))
                {
                    pMvPredBuf = (mfxExtFeiEncMVPredictors*)(eTask->bufs->PB_bufs.in.ExtParam[i]);
                    if (m_appCfg.bRepackPreencMV)
                    {
                        SAFE_FREAD(m_tmpForReading, sizeof(*m_tmpForReading)*m_numMB, 1, pMvPred, MFX_ERR_MORE_DATA);
                        repackPreenc2Enc(m_tmpForReading, pMvPredBuf->MB, m_numMB, m_tmpForMedian);
                    }
                    else {
                        SAFE_FREAD(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0]) *pMvPredBuf->NumMBAlloc, 1, pMvPred, MFX_ERR_MORE_DATA);
                    }
                }
                else{
                    int shft = m_appCfg.bRepackPreencMV ? sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB) : sizeof(mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB);
                    SAFE_FSEEK(pMvPred, shft*m_numMB, SEEK_CUR, MFX_ERR_MORE_DATA);
                }
                pMvPredId++;
            }
            break;
        } // switch (eTask->bufs->PB_bufs.in.ExtParam[i]->BufferId)
    } // for (int i = 0; i < eTask->bufs->PB_bufs.in.NumExtParam; i++)

    sts = FillRefInfo(eTask); // get info to fill reference structures
    MSDK_CHECK_STATUS(sts, "FillRefInfo failed");

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
                    feiPPS[fieldId].FrameNum   = eTask->m_frameNum;

                    memset(feiPPS[fieldId].ReferenceFrames, -1, 16*sizeof(mfxU16));
                    memcpy(feiPPS[fieldId].ReferenceFrames, &m_ref_info.state[fieldId].dpb_idx[0], sizeof(mfxU16)*m_ref_info.state[fieldId].dpb_idx.size());
                }

                if (feiSliceHeader)
                {
                    MSDK_CHECK_POINTER(feiSliceHeader[fieldId].Slice, MFX_ERR_NULL_PTR);

                    for (mfxU32 i = 0; i < feiSliceHeader[fieldId].NumSliceAlloc; i++)
                    {
                        MSDK_ZERO_ARRAY(feiSliceHeader[fieldId].Slice[i].RefL0, 32);
                        MSDK_ZERO_ARRAY(feiSliceHeader[fieldId].Slice[i].RefL1, 32);
                        feiSliceHeader[fieldId].Slice[i].SliceType = FEI_SLICETYPE_I;
                        feiSliceHeader[fieldId].Slice[i].NumRefIdxL0Active = 0;
                        feiSliceHeader[fieldId].Slice[i].NumRefIdxL1Active = 0;
                        feiSliceHeader[fieldId].Slice[i].IdrPicId          = eTask->m_frameIdrCounter;
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

                    for (mfxU32 i = 0; i < feiSliceHeader[fieldId].NumSliceAlloc; i++)
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
                        feiSliceHeader[fieldId].Slice[i].IdrPicId          = eTask->m_frameIdrCounter;
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

                    for (mfxU32 i = 0; i < feiSliceHeader[fieldId].NumSliceAlloc; i++)
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
                        feiSliceHeader[fieldId].Slice[i].IdrPicId          = eTask->m_frameIdrCounter;
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
            feiEncCtrl->MVPredictor = (ExtractFrameType(*eTask, encCtrlId) & MFX_FRAMETYPE_I) ? 0 : ((m_appCfg.mvinFile != NULL) || m_appCfg.bPREENC);

            // adjust ref window size if search window is 0
            if (m_appCfg.SearchWindow == 0 && m_pmfxENC)
            {
                // window size is limited to 1024 for bi-prediction
                if ((ExtractFrameType(*eTask, encCtrlId) & MFX_FRAMETYPE_B) && m_appCfg.RefHeight * m_appCfg.RefWidth > 1024)
                {
                    feiEncCtrl->RefHeight = 32;
                    feiEncCtrl->RefWidth  = 32;
                }
                else{
                    feiEncCtrl->RefHeight = m_appCfg.RefHeight;
                    feiEncCtrl->RefWidth  = m_appCfg.RefWidth;
                }
            }

            feiEncCtrl->NumMVPredictors[0] = feiEncCtrl->MVPredictor * m_numOfRefs[encCtrlId][0]; // driver requires these fields to be zero in case of feiEncCtrl->MVPredictor == false
            feiEncCtrl->NumMVPredictors[1] = feiEncCtrl->MVPredictor * m_numOfRefs[encCtrlId][1]; // but MSDK lib will adjust them to zero if application doesn't

            encCtrlId++;
            break;

        case MFX_EXTBUFF_FEI_ENC_MB:
            if (pEncMBs)
            {
                pMbEncCtrl = (mfxExtFeiEncMBCtrl*)(eTask->in.ExtParam[i]);
                SAFE_FREAD(pMbEncCtrl->MB, sizeof(pMbEncCtrl->MB[0])*pMbEncCtrl->NumMBAlloc, 1, pEncMBs, MFX_ERR_MORE_DATA);
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_QP:
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

mfxStatus CEncodingPipeline::InitEncodeFrameParams(mfxFrameSurface1* encodeSurface, sTask* pCurrentTask, PairU8 frameType, iTask* eTask)
{
    mfxStatus sts = MFX_ERR_NONE;

    MSDK_CHECK_POINTER(pCurrentTask,  MFX_ERR_NULL_PTR);

    bufSet * freeSet = getFreeBufSet(m_encodeBufs);
    MSDK_CHECK_POINTER(freeSet, MFX_ERR_NULL_PTR);

    if (m_bNeedDRC && m_bDRCReset)
        sts = ResetExtBufMBnum(freeSet, m_numMB_drc, true);
    else if (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN)
        sts = ResetExtBufMBnum(freeSet, (encodeSurface->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) ? m_numMB_frame: m_numMB, false);
    MSDK_CHECK_STATUS(sts, "ResetExtBufMBnum failed");

    /* In case of ENCODE in DisplayOrder mode, sTask perform ext buffers management, otherwise iTask does it */
    if (eTask)
        eTask->bufs = freeSet;
    else
        pCurrentTask->bufs.push_back(std::pair<bufSet*, mfxFrameSurface1*>(freeSet, encodeSurface));

    /* We have to force frame type through control in case of ENCODE in EncodedOrder mode */
    if (eTask)
    {
        m_ctr->FrameType = eTask->m_fieldPicFlag ? createType(*eTask) : ExtractFrameType(*eTask);
    }

    /* Load input Buffer for FEI ENCODE */
    mfxExtFeiEncMVPredictors* pMvPredBuf    = NULL;
    mfxExtFeiEncMBCtrl*       pMbEncCtrl    = NULL;
    mfxExtFeiEncQP*           pMbQP         = NULL;
    mfxExtFeiEncFrameCtrl*    feiEncCtrl    = NULL;
    mfxExtFeiRepackCtrl*      feiRepackCtrl = NULL;
    mfxU8 ffid = m_ffid;
    if (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN)
        ffid = !(encodeSurface->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) && (encodeSurface->Info.PicStruct & MFX_PICSTRUCT_FIELD_BFF);

    mfxU32 feiEncCtrlId = ffid, pMvPredId = ffid, encMBID = 0, mbQPID = 0, fieldId = 0;
    for (mfxU32 i = 0; i < freeSet->PB_bufs.in.NumExtParam; i++)
    {
        switch (freeSet->PB_bufs.in.ExtParam[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_ENC_MV_PRED:
            if (!m_appCfg.bPREENC && pMvPred)
            {
                mfxU16 numMB = m_numMB;
                if (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN && (encodeSurface->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE))
                {
                    if (feiEncCtrlId != ffid)
                        continue;
                    numMB = m_numMB_frame;
                }
                if (!(frameType[pMvPredId] & MFX_FRAMETYPE_I))
                {
                    pMvPredBuf = (mfxExtFeiEncMVPredictors*)(freeSet->PB_bufs.in.ExtParam[i]);
                    if (m_appCfg.bRepackPreencMV)
                    {
                        SAFE_FREAD(m_tmpForReading, sizeof(*m_tmpForReading)*numMB, 1, pMvPred, MFX_ERR_MORE_DATA);
                        repackPreenc2Enc(m_tmpForReading, pMvPredBuf->MB, numMB, m_tmpForMedian);
                    }
                    else {
                        SAFE_FREAD(pMvPredBuf->MB, sizeof(pMvPredBuf->MB[0])*pMvPredBuf->NumMBAlloc, 1, pMvPred, MFX_ERR_MORE_DATA);
                    }
                }
                else{
                    int shft = m_appCfg.bRepackPreencMV ? sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB) : sizeof(mfxExtFeiEncMVPredictors::mfxExtFeiEncMVPredictorsMB);
                    SAFE_FSEEK(pMvPred, shft*numMB, SEEK_CUR, MFX_ERR_MORE_DATA);
                }
            }
            pMvPredId = 1 - pMvPredId; // set to sfid
            break;

        case MFX_EXTBUFF_FEI_ENC_MB:
            if (pEncMBs){
                if (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN && (encodeSurface->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) && !!encMBID)
                    continue;
                pMbEncCtrl = (mfxExtFeiEncMBCtrl*)(freeSet->PB_bufs.in.ExtParam[i]);
                SAFE_FREAD(pMbEncCtrl->MB, sizeof(pMbEncCtrl->MB[0])*pMbEncCtrl->NumMBAlloc, 1, pEncMBs, MFX_ERR_MORE_DATA);
                encMBID++;
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_QP:
            if (pPerMbQP){
                if (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN && (encodeSurface->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) && !!mbQPID)
                    continue;
                pMbQP = (mfxExtFeiEncQP*)(freeSet->PB_bufs.in.ExtParam[i]);
                SAFE_FREAD(pMbQP->QP, sizeof(pMbQP->QP[0])*pMbQP->NumQPAlloc, 1, pPerMbQP, MFX_ERR_MORE_DATA);
                mbQPID++;
            }
            break;

        case MFX_EXTBUFF_FEI_ENC_CTRL:
            feiEncCtrl = (mfxExtFeiEncFrameCtrl*)(freeSet->PB_bufs.in.ExtParam[i]);

            if (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN && (encodeSurface->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) && feiEncCtrlId != ffid)
                continue;

            // adjust ref window size if search window is 0
            if (m_appCfg.SearchWindow == 0)
            {
                bool adjust_window = (frameType[feiEncCtrlId] & MFX_FRAMETYPE_B) && m_appCfg.RefHeight * m_appCfg.RefWidth > 1024;

                feiEncCtrl->RefHeight = adjust_window ? 32 : m_appCfg.RefHeight;
                feiEncCtrl->RefWidth  = adjust_window ? 32 : m_appCfg.RefWidth;
            }

            feiEncCtrl->MVPredictor = (!(frameType[feiEncCtrlId] & MFX_FRAMETYPE_I) && (m_appCfg.mvinFile != NULL || m_appCfg.bPREENC)) ? 1 : 0;

            // Need to set the number to actual ref number for each field
            feiEncCtrl->NumMVPredictors[0] = feiEncCtrl->MVPredictor * m_numOfRefs[fieldId][0]; // driver requires these fields to be zero in case of feiEncCtrl->MVPredictor == false
            feiEncCtrl->NumMVPredictors[1] = feiEncCtrl->MVPredictor * m_numOfRefs[fieldId][1]; // but MSDK lib will adjust them to zero if application doesn't
            fieldId++;

            feiEncCtrlId = 1 - feiEncCtrlId; // set to sfid
            break;

        case MFX_EXTBUFF_FEI_REPACK_CTRL:
            if (pRepackCtrl)
            {
                feiRepackCtrl = (mfxExtFeiRepackCtrl*)(freeSet->PB_bufs.in.ExtParam[i]);
                SAFE_FREAD(&(feiRepackCtrl->MaxFrameSize), sizeof(mfxU32),  1, pRepackCtrl, MFX_ERR_MORE_DATA);
                SAFE_FREAD(&(feiRepackCtrl->NumPasses),    sizeof(mfxU32),  1, pRepackCtrl, MFX_ERR_MORE_DATA);
                SAFE_FREAD(feiRepackCtrl->DeltaQP,         sizeof(mfxU8)*8, 1, pRepackCtrl, MFX_ERR_MORE_DATA);
                if (feiRepackCtrl->NumPasses > 4) {
                    msdk_printf(MSDK_STRING("WARNING: NumPasses should be less than or equal to 4\n"));
                }
            }
            break;
        } // switch (freeSet->PB_bufs.in.ExtParam[i]->BufferId)
    } // for (int i = 0; i<freeSet->PB_bufs.in.NumExtParam; i++)

    // Add input buffers
    bool is_I_frame = !m_isField && (frameType[ffid] & MFX_FRAMETYPE_I);
    if (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN)
    {
        is_I_frame = (encodeSurface->Info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) && (frameType[ffid] & MFX_FRAMETYPE_I);
    }

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

    mfxStatus sts = MFX_ERR_NONE;

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

    return sts;
}

mfxStatus CEncodingPipeline::DropDecodeStreamoutOutput(mfxFrameSurface1* pSurf)
{
    mfxStatus sts = MFX_ERR_NONE;
    /* set m_pExtBufDecodeStreamout to current streamout data */
    m_pExtBufDecodeStreamout = NULL;
    for (int i = 0; i < pSurf->Data.NumExtParam; i++)
        if (pSurf->Data.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_DEC_STREAM_OUT)
        {
            m_pExtBufDecodeStreamout = (mfxExtFeiDecStreamOut*)pSurf->Data.ExtParam[i];
            break;
        }

    MSDK_CHECK_POINTER(m_pExtBufDecodeStreamout,     MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pExtBufDecodeStreamout->MB, MFX_ERR_NULL_PTR);

    if (decodeStreamout)
    {
        /* NOTE: streamout holds data for both fields in MB array (first NumMBAlloc for first field data, second NumMBAlloc for second field) */
        SAFE_FWRITE(m_pExtBufDecodeStreamout->MB, sizeof(mfxFeiDecStreamOutMBCtrl)*m_pExtBufDecodeStreamout->NumMBAlloc, 1, decodeStreamout, MFX_ERR_MORE_DATA);
    }

    return sts;
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
    mfxU32 numMB = eTask->m_fieldPicFlag ? m_numMBpreenc : m_numMBpreenc_frame;

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
                    for (mfxU32 k = 0; k < numMB; k++){             // in progressive case Ext buffer for I frame is detached
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
        for (mfxU32 k = 0; k < numMB; k++){
            SAFE_FWRITE(m_tmpMBpreenc, sizeof(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB), 1, mvout, MFX_ERR_MORE_DATA);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::PassPreEncMVPred2EncEx(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxExtFeiEncMVPredictors* mvp = NULL;
    bufSet*                   set = getFreeBufSet(m_encodeBufs);
    MSDK_CHECK_POINTER(set, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    BestMVset BestSet(m_numOfRefs);

    mfxU32 i, j, numOfFields = eTask->m_fieldPicFlag ? 2 : 1;

    mfxU16 numMBpreenc = eTask->m_fieldPicFlag ? m_numMBpreenc : m_numMBpreenc_frame;

    for (mfxU32 fieldId = 0; fieldId < numOfFields; fieldId++)
    {
        MSDK_BREAK_ON_ERROR(sts);

        mfxU32 nPred_actual[2] = { (std::min)(m_numOfRefs[fieldId][0], MaxFeiEncMVPNum), (std::min)(m_numOfRefs[fieldId][1], MaxFeiEncMVPNum) }; //adjust n of passed pred
        if (nPred_actual[0] + nPred_actual[1] == 0 || (ExtractFrameType(*eTask, fieldId) & MFX_FRAMETYPE_I))
            continue; // I-field

        // MVP is predictors Ext Buffer
        mvp = reinterpret_cast<mfxExtFeiEncMVPredictors*>(set->PB_bufs.in.getBufById(MFX_EXTBUFF_FEI_ENC_MV_PRED, fieldId));
        if (mvp == NULL){
            sts = MFX_ERR_NULL_PTR;
            break;
        }

        /* not necessary for pipelines PreENC + FEI_ENCODE / (ENCPAK), if number of predictors for next interface properly set */
        MSDK_ZERO_ARRAY(mvp->MB, mvp->NumMBAlloc);

        for (i = 0; i < numMBpreenc; ++i)
        {
            /* get best L0/L1 PreENC predictors for current MB in terms of distortions */
            sts = GetBestSetsByDistortion(eTask->preenc_output, BestSet, nPred_actual, fieldId, i);
            MSDK_BREAK_ON_ERROR(sts);

            if (!m_appCfg.preencDSstrength)
            {
                /* in case of PreENC on original surfaces just get median of MVs for each predictor */
                for (j = 0; j < BestSet.bestL0.size(); ++j)
                {
                    mvp->MB[i].RefIdx[j].RefL0 = BestSet.bestL0[j].refIdx;
                    mvp->MB[i].MV[j][0].x = get16Median(BestSet.bestL0[j].preenc_MVMB, m_tmpForMedian, 0, 0);
                    mvp->MB[i].MV[j][0].y = get16Median(BestSet.bestL0[j].preenc_MVMB, m_tmpForMedian, 1, 0);
                }
                for (j = 0; j < BestSet.bestL1.size(); ++j)
                {
                    mvp->MB[i].RefIdx[j].RefL1 = BestSet.bestL1[j].refIdx;
                    mvp->MB[i].MV[j][1].x = get16Median(BestSet.bestL1[j].preenc_MVMB, m_tmpForMedian, 0, 1);
                    mvp->MB[i].MV[j][1].y = get16Median(BestSet.bestL1[j].preenc_MVMB, m_tmpForMedian, 1, 1);
                }
           }
           else
           {
               /* in case of PreENC on downsampled surfaces we need to calculate umpsampled pridictors */
               for (j = 0; j < BestSet.bestL0.size(); ++j)
               {
                   UpsampleMVP(BestSet.bestL0[j].preenc_MVMB, i, mvp, j, BestSet.bestL0[j].refIdx, 0);
               }
               for (j = 0; j < BestSet.bestL1.size(); ++j)
               {
                   UpsampleMVP(BestSet.bestL1[j].preenc_MVMB, i, mvp, j, BestSet.bestL1[j].refIdx, 1);
               }
           }
        } // for (i = 0; i < numMBpreenc; ++i)

    } // for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)

    SAFE_RELEASE_EXT_BUFSET(set);

    eTask->ReleasePreEncOutput();

    return sts;
}

/*  In case of PreENC with downsampling output motion vectors should be upscaled to correspond full-resolution frame

    preenc_MVMB - current MB on downsampled frame
    MBindex_DS  - index of current MB
    mvp         - motion vectors predictors array for full-resolution frame
    predIdx     - index of current predictor [0-3] (up to 4 is supported)
    refIdx      - indef of current reference (for which predictor is applicable) in reference list
    L0L1        - indicates list of referrences being processed [0-1] (0 - L0 list, 1- L1 list)
*/
void CEncodingPipeline::UpsampleMVP(mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB * preenc_MVMB, mfxU32 MBindex_DS, mfxExtFeiEncMVPredictors* mvp, mfxU32 predIdx, mfxU8 refIdx, mfxU32 L0L1)
{
    static mfxI16 MVZigzagOrder[16] = { 0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15 };

    mfxU16 mv_idx;
    mfxU32 encMBidx;
    mfxI16Pair* mvp_mb;

    for (mfxU16 k = 0; k < m_appCfg.preencDSstrength*m_appCfg.preencDSstrength; ++k)
    {
        encMBidx = (MBindex_DS % m_widthMBpreenc + MBindex_DS / m_widthMBpreenc *  m_widthMB) * m_appCfg.preencDSstrength + k % m_appCfg.preencDSstrength + m_widthMB*(k / m_appCfg.preencDSstrength);

        if (encMBidx >= m_numMB)
            continue;

        mvp_mb = &(mvp->MB[encMBidx].MV[predIdx][L0L1]);

        if (!L0L1)
            mvp->MB[encMBidx].RefIdx[predIdx].RefL0 = refIdx;
        else
            mvp->MB[encMBidx].RefIdx[predIdx].RefL1 = refIdx;

        switch (m_appCfg.preencDSstrength)
        {
        case 2:
            mvp_mb->x = get4Median(preenc_MVMB, m_tmpForMedian, 0, L0L1, k);
            mvp_mb->y = get4Median(preenc_MVMB, m_tmpForMedian, 1, L0L1, k);
            break;
        case 4:
            mvp_mb->x = preenc_MVMB->MV[MVZigzagOrder[k]][L0L1].x;
            mvp_mb->y = preenc_MVMB->MV[MVZigzagOrder[k]][L0L1].y;
            break;
        case 8:
            mv_idx = MVZigzagOrder[k % 16 % 8 / 2 + k / 16 * 4];
            mvp_mb->x = preenc_MVMB->MV[mv_idx][L0L1].x;
            mvp_mb->y = preenc_MVMB->MV[mv_idx][L0L1].y;
            break;
        default:
            break;
        }

        mvp_mb->x *= m_appCfg.preencDSstrength;
        mvp_mb->y *= m_appCfg.preencDSstrength;

    } // for (k = 0; k < m_encpakParams.preencDSstrength*m_encpakParams.preencDSstrength; ++k)
}

/* Simplified conversion - no sorting by distortion, no median on MV */
mfxStatus CEncodingPipeline::PassPreEncMVPred2EncExPerf(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    mfxExtFeiEncMVPredictors* mvp = NULL;
    std::vector<mfxExtFeiPreEncMV*> mvs_v;
    std::vector<mfxU8*>             refIdx_v;
    bufSet* set = getFreeBufSet(m_encodeBufs);
    MSDK_CHECK_POINTER(set, MFX_ERR_NULL_PTR);

    mvs_v.reserve(MaxFeiEncMVPNum);
    refIdx_v.reserve(MaxFeiEncMVPNum);

    mfxU32 i, j, preencMBidx, MVrow, MVcolumn, preencMBMVidx;

    mfxU32 nOfPredPairs = 0, numOfFields = eTask->m_fieldPicFlag ? 2 : 1;

    mfxU16 numMB = eTask->m_fieldPicFlag ? m_numMB : m_numMB_frame;

    for (mfxU32 fieldId = 0; fieldId < numOfFields; fieldId++)
    {
        nOfPredPairs = (std::min)((std::max)(m_numOfRefs[fieldId][0], m_numOfRefs[fieldId][1]), MaxFeiEncMVPNum);
        if (nOfPredPairs == 0 || (ExtractFrameType(*eTask, fieldId) & MFX_FRAMETYPE_I))
            continue; // I-field

        mvs_v.clear();
        refIdx_v.clear();

        mvp = reinterpret_cast<mfxExtFeiEncMVPredictors*>(set->PB_bufs.in.getBufById(MFX_EXTBUFF_FEI_ENC_MV_PRED, fieldId));
        if (mvp == NULL){
            sts = MFX_ERR_NULL_PTR;
            break;
        }

        j = 0;
        for (std::list<PreEncOutput>::iterator it = eTask->preenc_output.begin(); j < nOfPredPairs; ++it, ++j)
        {
            mvs_v.push_back(reinterpret_cast<mfxExtFeiPreEncMV*>((*it).output_bufs->PB_bufs.out.getBufById(MFX_EXTBUFF_FEI_PREENC_MV, fieldId)));
            refIdx_v.push_back((*it).refIdx[fieldId]);
        }

        for (i = 0; i < numMB; ++i) // get nPred_actual L0/L1 predictors for each MB
        {
            for (j = 0; j < nOfPredPairs; ++j)
            {
                mvp->MB[i].RefIdx[j].RefL0 = refIdx_v[j][0];
                mvp->MB[i].RefIdx[j].RefL1 = refIdx_v[j][1];

                if (!m_appCfg.preencDSstrength)
                {
                    memcpy(mvp->MB[i].MV[j], mvs_v[j]->MB[i].MV[0], 2 * sizeof(mfxI16Pair));
                }
                else
                {
                    static mfxI16 MVZigzagOrder[16] = { 0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15 };

                    preencMBidx = i / m_widthMB / m_appCfg.preencDSstrength * m_widthMBpreenc + i % m_widthMB / m_appCfg.preencDSstrength;
                    if (preencMBidx >= m_numMBpreenc)
                        continue;

                    MVrow    = i / m_widthMB % m_appCfg.preencDSstrength;
                    MVcolumn = i % m_widthMB % m_appCfg.preencDSstrength;

                    switch (m_appCfg.preencDSstrength)
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
                        preencMBMVidx = 0;
                        break;
                    }

                    memcpy(mvp->MB[i].MV[j], mvs_v[j]->MB[preencMBidx].MV[MVZigzagOrder[preencMBMVidx]], 2 * sizeof(mfxI16Pair));

                    mvp->MB[i].MV[j][0].x *= m_appCfg.preencDSstrength;
                    mvp->MB[i].MV[j][0].y *= m_appCfg.preencDSstrength;
                    mvp->MB[i].MV[j][1].x *= m_appCfg.preencDSstrength;
                    mvp->MB[i].MV[j][1].y *= m_appCfg.preencDSstrength;
                }
            }
        }

    } // for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)

    SAFE_RELEASE_EXT_BUFSET(set);

    eTask->ReleasePreEncOutput();

    return sts;
}

mfxStatus CEncodingPipeline::ResetExtBufMBnum(bufSet* freeSet, mfxU16 new_numMB, bool both_fields)
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxExtFeiPreEncMVPredictors* mvPreds    = NULL;
    mfxExtFeiEncMV*              mvBuf      = NULL;
    mfxExtFeiEncMBStat*          mbstatBuf  = NULL;
    mfxExtFeiPakMBCtrl*          mbcodeBuf  = NULL;
    mfxExtFeiEncQP*              pMbQP      = NULL;
    mfxExtFeiEncMBCtrl*          pMbEncCtrl = NULL;
    mfxExtFeiEncMVPredictors*    pMvPredBuf = NULL;
    mfxExtFeiPreEncMV*           mvs        = NULL;
    mfxExtFeiPreEncMBStat*       mbdata     = NULL;

    mfxU16 increment = both_fields ? 1 : m_numOfFields;

    setElem &bufsIn = freeSet->PB_bufs.in;
    for (mfxU16 i = 0; i < bufsIn.NumExtParam; i += increment)
    {
        switch (bufsIn.ExtParam[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_PREENC_MV_PRED:
            mvPreds = (mfxExtFeiPreEncMVPredictors*)(bufsIn.ExtParam[i]);
            mvPreds->NumMBAlloc = new_numMB;
            break;

        case MFX_EXTBUFF_FEI_ENC_MV_PRED:
            pMvPredBuf = (mfxExtFeiEncMVPredictors*)(bufsIn.ExtParam[i]);
            pMvPredBuf->NumMBAlloc = new_numMB;
            break;

        case MFX_EXTBUFF_FEI_ENC_MB:
            pMbEncCtrl = (mfxExtFeiEncMBCtrl*)(freeSet->PB_bufs.in.ExtParam[i]);
            pMbEncCtrl->NumMBAlloc = new_numMB;
            break;

        case MFX_EXTBUFF_FEI_ENC_QP:
            pMbQP = (mfxExtFeiEncQP*)(bufsIn.ExtParam[i]);
            pMbQP->NumQPAlloc = new_numMB;
            break;
        }
    }

    setElem &bufsOut = freeSet->PB_bufs.out;
    for (mfxU16 i = 0; i < bufsOut.NumExtParam; i += increment)
    {
        switch (bufsOut.ExtParam[i]->BufferId)
        {
        case MFX_EXTBUFF_FEI_PREENC_MV:
            mvs = (mfxExtFeiPreEncMV*)(bufsOut.ExtParam[i]);
            mvs->NumMBAlloc = new_numMB;
            break;

        case MFX_EXTBUFF_FEI_PREENC_MB:
            mbdata = (mfxExtFeiPreEncMBStat*)(bufsOut.ExtParam[i]);
            mbdata->NumMBAlloc = new_numMB;
            break;

        case MFX_EXTBUFF_FEI_ENC_MV:
            mvBuf = (mfxExtFeiEncMV*)(bufsOut.ExtParam[i]);
            mvBuf->NumMBAlloc = new_numMB;
            break;

        case MFX_EXTBUFF_FEI_ENC_MB_STAT:
            mbstatBuf = (mfxExtFeiEncMBStat*)(bufsOut.ExtParam[i]);
            mbstatBuf->NumMBAlloc = new_numMB;
            break;

        case MFX_EXTBUFF_FEI_PAK_CTRL:
            mbcodeBuf = (mfxExtFeiPakMBCtrl*)(bufsOut.ExtParam[i]);
            mbcodeBuf->NumMBAlloc = new_numMB;
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

/*  PreEnc outputs 16 MVs per-MB, one of the way to construct predictor from this array
    is to extract median for x and y component

    preencMB - MB of motion vectors buffer
    tmpBuf   - temporary array, for inplace sorting
    xy       - indicates coordinate being processed [0-1] (0 - x coordinate, 1 - y coordinate)
    L0L1     - indicates reference list being processed [0-1] (0 - L0-list, 1 - L1-list)

    returned value - median of 16 element array
*/
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

/* Repacking of PreENC MVs with 4x downscale requires calculating 4-element median

    Input/output parameters are similair with get16Median

    offset - indicates position of 4 MVs which maps to current MB on full-resolution frame
             (other 12 components are correspondes to another MBs on full-resolution frame)
*/

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

            case MFX_EXTBUFF_FEI_REPACK_CTRL:
            {
                mfxExtFeiRepackCtrl* feiRepack = (mfxExtFeiRepackCtrl*)((*it)->PB_bufs.in.ExtParam[i]);
                MSDK_SAFE_DELETE_ARRAY(feiRepack);
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

            case MFX_EXTBUFF_FEI_ENC_QP:
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

/* free decode streamout buffers*/

mfxStatus CEncodingPipeline::ClearDecoderBuffers()
{
    if (m_appCfg.bDECODESTREAMOUT)
    {
        m_pExtBufDecodeStreamout = NULL;

        for (mfxU32 i = 0; i < m_StreamoutBufs.size(); i++)
        {
            MSDK_SAFE_DELETE_ARRAY(m_StreamoutBufs[i]->MB);
            MSDK_SAFE_DELETE(m_StreamoutBufs[i]);
        }

        m_StreamoutBufs.clear();
    }
    return MFX_ERR_NONE;
}

/* per-frame processing by interfaces*/

mfxStatus CEncodingPipeline::PreencOneFrame(iTask* eTask)
{
    MFX_ITT_TASK("PreencOneFrame");
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    /* PreENC DownSampling */
    if (m_appCfg.preencDSstrength)
    {
        MFX_ITT_TASK("DownsampleInput");
        // PREENC needs to be performed on downscaled surface
        // For simplicity, let's just replace the original surface
        eTask->fullResSurface = eTask->in.InSurface;

        // find/wait for a free output surface
        ExtendedSurface VppExtSurface = { 0 };
        VppExtSurface.pSurface = m_pDSSurfaces.GetFreeSurface_FEI();
        MSDK_CHECK_POINTER(VppExtSurface.pSurface, MFX_ERR_MEMORY_ALLOC);

        // make sure picture structure has the initial value
        // surfaces are reused and VPP may change this parameter in certain configurations
        mfxU16 ps = (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN) ? (eTask->in.InSurface->Info.PicStruct & 0xf) : m_mfxEncParams.mfx.FrameInfo.PicStruct;
        VppExtSurface.pSurface->Info.PicStruct = ps;

        sts = VPPOneFrame(m_pmfxDS, m_pPreencSession, eTask->fullResSurface, &VppExtSurface, true);
        if (sts == MFX_ERR_GPU_HANG)
        {
            return MFX_ERR_GPU_HANG;
        }
        MSDK_CHECK_STATUS(sts, "PreENC DownsampleInput failed");

        eTask->in.InSurface = VppExtSurface.pSurface;
        eTask->in.InSurface->Data.Locked++;
    }

    sts = ProcessMultiPreenc(eTask);
    if (sts == MFX_ERR_GPU_HANG)
    {
        return MFX_ERR_GPU_HANG;
    }
    MSDK_CHECK_STATUS(sts, "ProcessMultiPreenc failed");

    // Pass MVP to encode,encpak
    if (m_appCfg.bENCODE || m_appCfg.bENCPAK || m_appCfg.bOnlyENC)
    {
        if (m_ctr)
            m_ctr->FrameType = eTask->m_fieldPicFlag ? createType(*eTask) : ExtractFrameType(*eTask);

        if (eTask->m_fieldPicFlag || !(ExtractFrameType(*eTask) & MFX_FRAMETYPE_I)){
            //repack MV predictors
            MFX_ITT_TASK("RepackMVs");
            if (!m_appCfg.bPerfMode)
                sts = PassPreEncMVPred2EncEx(eTask);
            else
                sts = PassPreEncMVPred2EncExPerf(eTask);
            MSDK_CHECK_STATUS(sts, "PassPreEncMVPred2EncEx failed");
        }
    }

    return sts;
}

mfxStatus CEncodingPipeline::ProcessMultiPreenc(iTask* eTask)
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;
    MSDK_ZERO_ARRAY(m_numOfRefs[0], 2);
    MSDK_ZERO_ARRAY(m_numOfRefs[1], 2);
    mfxU32 numOfFields = eTask->m_fieldPicFlag ? 2 : 1;

    // max possible candidates to L0 / L1
    mfxU32 total_l0 = (ExtractFrameType(*eTask, eTask->m_fieldPicFlag) & MFX_FRAMETYPE_P) ? ((ExtractFrameType(*eTask) & MFX_FRAMETYPE_IDR) ? 1 : eTask->NumRefActiveP) : ((ExtractFrameType(*eTask) & MFX_FRAMETYPE_I) ? 1 : eTask->NumRefActiveBL0);
    mfxU32 total_l1 = (ExtractFrameType(*eTask) & MFX_FRAMETYPE_B) ? eTask->NumRefActiveBL1 : 1; // just one iteration here for non-B

    total_l0 = (std::min)(total_l0, numOfFields*m_mfxEncParams.mfx.NumRefFrame); // adjust to maximal
    total_l1 = (std::min)(total_l1, numOfFields*m_mfxEncParams.mfx.NumRefFrame); // number of references

    mfxU8 preenc_ref_idx[2][2]; // indexes means [fieldId][L0L1]
    mfxU8 ref_fid[2][2];
    iTask* refTask[2][2];
    bool isDownsamplingNeeded = true;

    for (mfxU8 l0_idx = 0, l1_idx = 0; l0_idx < total_l0 || l1_idx < total_l1; l0_idx++, l1_idx++)
    {
        MFX_ITT_TASK("PreENCpass");
        sts = GetRefTaskEx(eTask, l0_idx, l1_idx, preenc_ref_idx, ref_fid, refTask);

        if (ExtractFrameType(*eTask) & MFX_FRAMETYPE_I)
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_OUT_OF_RANGE);

        if (sts == MFX_WRN_OUT_OF_RANGE){ // some of the indexes exceeds DPB
            sts = MFX_ERR_NONE;
            continue;
        }
        MSDK_BREAK_ON_ERROR(sts);

        for (mfxU32 fieldId = 0; fieldId < numOfFields; fieldId++)
        {
            if (refTask[fieldId][0]) { m_numOfRefs[fieldId][0]++; }
            if (refTask[fieldId][1]) { m_numOfRefs[fieldId][1]++; }
        }

        sts = InitPreEncFrameParamsEx(eTask, refTask, ref_fid, isDownsamplingNeeded);
        MSDK_CHECK_STATUS(sts, "InitPreEncFrameParamsEx failed");

        // If input surface is not being changed between PreENC calls
        // (including calls for different fields of the same field pair in Single Field processing mode),
        // an application can avoid redundant downsampling on driver side.
        isDownsamplingNeeded = false;

        // Doing PreEnc
        for (;;)
        {
            sts = m_pmfxPREENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
            if (sts == MFX_ERR_GPU_HANG)
            {
                sts = doGPUHangRecovery();
                MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
                return MFX_ERR_GPU_HANG;
            }
            MSDK_BREAK_ON_ERROR(sts);

            /*PRE-ENC is running in separate session */
            sts = m_pPreencSession->SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
            if (sts == MFX_ERR_GPU_HANG)
            {
                sts = doGPUHangRecovery();
                MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
                return MFX_ERR_GPU_HANG;
            }
            MSDK_BREAK_ON_ERROR(sts);
            mdprintf(stderr, "preenc synced : %d\n", sts);
            if (m_appCfg.bFieldProcessingMode)
            {
                sts = m_pmfxPREENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                if (sts == MFX_ERR_GPU_HANG)
                {
                    sts = doGPUHangRecovery();
                    MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
                    return MFX_ERR_GPU_HANG;
                }
                MSDK_BREAK_ON_ERROR(sts);

                /*PRE-ENC is running in separate session */
                sts = m_pPreencSession->SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                if (sts == MFX_ERR_GPU_HANG)
                {
                    sts = doGPUHangRecovery();
                    MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
                    return MFX_ERR_GPU_HANG;
                }
                MSDK_BREAK_ON_ERROR(sts);
                mdprintf(stderr, "preenc synced : %d\n", sts);
            }

            if (MFX_ERR_NONE < sts && !eTask->EncSyncP) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts)
                {
                    WaitForDeviceToBecomeFree(*m_pPreencSession, eTask->EncSyncP, sts);
                }
            }
            else if (MFX_ERR_NONE < sts && eTask->EncSyncP)
            {
                sts = MFX_ERR_NONE; // ignore warnings if output is available
                break;
            }
            else
            {
                break;
            }
        }
        MSDK_CHECK_STATUS(sts, "FEI PreENC failed to process frame");

        // Store PreEnc output
        eTask->preenc_output.push_back(PreEncOutput(eTask->preenc_bufs, preenc_ref_idx));

        //drop output data to output file
        sts = DropPREENCoutput(eTask);
        MSDK_CHECK_STATUS(sts, "DropPREENCoutput failed");

    } // for (mfxU32 l0_idx = 0, l1_idx = 0; l0_idx < total_l0 || l1_idx < total_l1; l0_idx++, l1_idx++)

    return sts;
}

/* PREENC functions */

mfxStatus CEncodingPipeline::GetRefTaskEx(iTask *eTask, mfxU8 l0_idx, mfxU8 l1_idx, mfxU8 refIdx[2][2], mfxU8 ref_fid[2][2], iTask *outRefTask[2][2])
{
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxStatus stsL0 = MFX_WRN_OUT_OF_RANGE, stsL1 = MFX_WRN_OUT_OF_RANGE;

    for (int i = 0; i < 2; i++){
        MSDK_ZERO_ARRAY(refIdx[i],     2);
        MSDK_ZERO_ARRAY(outRefTask[i], 2);
        MSDK_ZERO_ARRAY(ref_fid[i],    2);
    }

    mfxU32 numOfFields = eTask->m_fieldPicFlag ? 2 : 1;
    for (mfxU32 fieldId = 0; fieldId < numOfFields; fieldId++)
    {
        mfxU8 type = ExtractFrameType(*eTask, fieldId);
        mfxU32 l0_ref_count = eTask->GetNBackward(fieldId),
               l1_ref_count = eTask->GetNForward(fieldId),
               fid = eTask->m_fid[fieldId];

        /* adjustment for case of equal l0 and l1 lists*/
        if (!l0_ref_count && eTask->m_list0[fid].Size() && !(eTask->m_type[fid] & MFX_FRAMETYPE_I))
        {
            l0_ref_count = eTask->m_list0[fid].Size();
        }

        if (l0_idx < l0_ref_count && eTask->m_list0[fid].Size())
        {
            mfxU8 const * l0_instance = eTask->m_list0[fid].Begin() + l0_idx;
            iTask *L0_ref_task = m_inputTasks.GetTaskByFrameOrder(eTask->m_dpb[fid][*l0_instance & 127].m_frameOrder);
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
                iTask *L1_ref_task = m_inputTasks.GetTaskByFrameOrder(eTask->m_dpb[fid][*l1_instance & 127].m_frameOrder);
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

/*  PreENC may be called multiple times for reference frames and multiple MV, MBstat buffers are generated.
    Here PreENC results are sorted indipendently for L0 and L1 references in terms of distortion.
    This function is called per-MB.

    preenc_output - list of output PreENC buffers from multicall stage
    RepackingInfo - structure that holds information about resulting predictors
                    (it is shrinked to fit nPred size before return)
    nPred         - array of numbers predictors expected by next interface (ENC/ENCODE)
                    (nPred[0] - number of L0 predictors, nPred[1] - number of L1 predictors)
    fieldId       - id of field being processed (0 - first field, 1 - second field)
    MB_idx        - offset of MB being processed
*/

mfxStatus CEncodingPipeline::GetBestSetsByDistortion(std::list<PreEncOutput>& preenc_output,
    BestMVset & BestSet,
    mfxU32 nPred[2], mfxU32 fieldId, mfxU32 MB_idx)
{
    mfxStatus sts = MFX_ERR_NONE;

    // clear previous data
    BestSet.Clear();

    mfxExtFeiPreEncMV*     mvs    = NULL;
    mfxExtFeiPreEncMBStat* mbdata = NULL;

    for (std::list<PreEncOutput>::iterator it = preenc_output.begin(); it != preenc_output.end(); ++it)
    {
        mvs = reinterpret_cast<mfxExtFeiPreEncMV*> ((*it).output_bufs->PB_bufs.out.getBufById(MFX_EXTBUFF_FEI_PREENC_MV, fieldId));
        MSDK_CHECK_POINTER(mvs, MFX_ERR_NULL_PTR);

        mbdata = reinterpret_cast<mfxExtFeiPreEncMBStat*> ((*it).output_bufs->PB_bufs.out.getBufById(MFX_EXTBUFF_FEI_PREENC_MB, fieldId));
        MSDK_CHECK_POINTER(mbdata, MFX_ERR_NULL_PTR);

        /* Store all necessary info about current reference MB: pointer to MVs; reference index; distortion*/
        BestSet.bestL0.push_back(MVP_elem(&mvs->MB[MB_idx], (*it).refIdx[fieldId][0], mbdata->MB[MB_idx].Inter[0].BestDistortion));

        if (nPred[1])
        {
            BestSet.bestL1.push_back(MVP_elem(&mvs->MB[MB_idx], (*it).refIdx[fieldId][1], mbdata->MB[MB_idx].Inter[1].BestDistortion));
        }
    }

    /* find nPred best predictors by distortion */
    std::sort(BestSet.bestL0.begin(), BestSet.bestL0.end(), compareDistortion);
    BestSet.bestL0.resize(nPred[0]);

    if (nPred[1])
    {
        std::sort(BestSet.bestL1.begin(), BestSet.bestL1.end(), compareDistortion);
        BestSet.bestL1.resize(nPred[1]);
    }

    return sts;
}

inline bool compareDistortion(MVP_elem frst, MVP_elem scnd)
{
    return frst.distortion < scnd.distortion;
}

/* end of PREENC functions */

mfxStatus CEncodingPipeline::EncPakOneFrame(iTask* eTask)
{
    MFX_ITT_TASK("EncPakOneFrame");
    MSDK_CHECK_POINTER(eTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = InitEncPakFrameParams(eTask);
    MSDK_CHECK_STATUS(sts, "InitEncPakFrameParams failed");

    for (;;)
    {
        mdprintf(stderr, "frame: %d  t:%d %d : submit ", eTask->m_frameOrder, eTask->m_type[m_ffid], eTask->m_type[m_sfid]);

        for (int i = 0; i < 1 + m_appCfg.bFieldProcessingMode; ++i)
        {
            if ((m_appCfg.bENCPAK) || (m_appCfg.bOnlyENC))
            {
                sts = m_pmfxENC->ProcessFrameAsync(&eTask->in, &eTask->out, &eTask->EncSyncP);
                if (sts == MFX_ERR_GPU_HANG)
                {
                    sts = doGPUHangRecovery();
                    MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
                    return MFX_ERR_GPU_HANG;
                }
                MSDK_BREAK_ON_ERROR(sts);

                sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                if (sts == MFX_ERR_GPU_HANG)
                {
                    sts = doGPUHangRecovery();
                    MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
                    return MFX_ERR_GPU_HANG;
                }
                MSDK_BREAK_ON_ERROR(sts);
                mdprintf(stderr, "synced : %d\n", sts);
            }

            if (i == 0)
            {
                MSDK_DEBUG
                /* In case of PAK only we need to read data from file */
                if (m_appCfg.bOnlyPAK)
                {
                    sts = ReadPAKdata(eTask);
                    MSDK_CHECK_STATUS(sts, "ReadPAKdata failed");
                }
            }

            if ((m_appCfg.bENCPAK) || (m_appCfg.bOnlyPAK))
            {
                sts = m_pmfxPAK->ProcessFrameAsync(&eTask->inPAK, &eTask->outPAK, &eTask->EncSyncP);
                if (sts == MFX_ERR_GPU_HANG)
                {
                    sts = doGPUHangRecovery();
                    MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
                    return MFX_ERR_GPU_HANG;
                }
                MSDK_BREAK_ON_ERROR(sts);

                sts = m_mfxSession.SyncOperation(eTask->EncSyncP, MSDK_WAIT_INTERVAL);
                if (sts == MFX_ERR_GPU_HANG)
                {
                    sts = doGPUHangRecovery();
                    MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
                    return MFX_ERR_GPU_HANG;
                }
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
        else if (MFX_ERR_NONE < sts && eTask->EncSyncP)
        {
            sts = MFX_ERR_NONE; // ignore warnings if output is available
            break;
        }
        else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
        {
            sts = AllocateSufficientBuffer(eTask->outPAK.Bs);
            MSDK_CHECK_STATUS(sts, "AllocateSufficientBuffer failed");
        }
        else
        {
            break;
        }
    }
    MSDK_CHECK_STATUS(sts, "FEI ENCPAK failed to encode frame");

    //drop output data to output file
    if ((m_appCfg.bENCPAK) || (m_appCfg.bOnlyPAK))
    {
        sts = sTask::WriteBitstream(m_FileWriter, eTask->outPAK.Bs);
        MSDK_CHECK_STATUS(sts, "pCurrentTask->WriteBitstream failed");
    }

    if ((m_appCfg.bENCPAK) || (m_appCfg.bOnlyENC))
    {
        sts = DropENCPAKoutput(eTask);
        MSDK_CHECK_STATUS(sts, "DropENCPAKoutput failed");
    }

    return sts;
}

mfxStatus CEncodingPipeline::EncodeOneFrame(iTask* eTask, mfxFrameSurface1* pSurf)
{
    MFX_ITT_TASK("EncodeOneFrame");

    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 fieldProcessingCounter = 0;

    // get a pointer to a free task (bit stream and sync point for encoder)
    sTask *pSyncTask;
    sts = GetFreeTask(&pSyncTask);
    MSDK_CHECK_STATUS(sts, "Failed to get free sTask");

    mfxFrameSurface1* encodeSurface = eTask ? (m_appCfg.preencDSstrength ? eTask->fullResSurface : eTask->in.InSurface) : pSurf;
    PairU8 frameType = eTask ? eTask->m_type : m_frameType;
    if (m_mfxEncParams.mfx.EncodedOrder || (!m_mfxEncParams.mfx.EncodedOrder && encodeSurface)) // no need to do this for buffered frames in display order
    {
        sts = InitEncodeFrameParams(encodeSurface, pSyncTask, frameType, eTask);
        MSDK_CHECK_STATUS(sts, "InitEncodeFrameParams failed");
    }

    for (;;) {
        // at this point surface for encoder contains either a frame from file or a frame processed by vpp

        MSDK_ZERO_MEMORY(pSyncTask->EncSyncP);
        sts = m_pmfxENCODE->EncodeFrameAsync(m_ctr, encodeSurface, &pSyncTask->mfxBS, &pSyncTask->EncSyncP);
        if (sts == MFX_ERR_GPU_HANG)
        {
            sts = doGPUHangRecovery();
            MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
            return MFX_ERR_GPU_HANG;
        }

        fieldProcessingCounter++;

        if (MFX_ERR_NONE < sts && !pSyncTask->EncSyncP) // repeat the call if warning and no output
        {
            if (MFX_WRN_DEVICE_BUSY == sts)
            {
                WaitForDeviceToBecomeFree(m_mfxSession, pSyncTask->EncSyncP, sts);
            }
        }
        else if (MFX_ERR_NONE < sts && pSyncTask->EncSyncP)
        {
            sts = MFX_ERR_NONE; // ignore warnings if output is available
            sts = SyncOneEncodeFrame(pSyncTask, fieldProcessingCounter);
            if (sts == MFX_ERR_GPU_HANG)
            {
                return MFX_ERR_GPU_HANG;
            }
            MSDK_BREAK_ON_ERROR(sts);

            if ((MFX_CODINGOPTION_ON == m_encpakInit.SingleFieldProcessing) &&
                (1 == fieldProcessingCounter)){
                continue;
            }
            break;
        }
        else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
        {
            sts = AllocateSufficientBuffer(&pSyncTask->mfxBS);
            MSDK_CHECK_STATUS(sts, "AllocateSufficientBuffer failed");
        }
        else
        {
            if (encodeSurface)
            {
                MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA); // correct status to finish encoding of buffered frames
            }                                                // so don't ignore it
            if (pSyncTask->EncSyncP)
            {
                sts = SyncOneEncodeFrame(pSyncTask, fieldProcessingCounter);
                if (sts == MFX_ERR_GPU_HANG)
                {
                    return MFX_ERR_GPU_HANG;
                }
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

mfxStatus CEncodingPipeline::SyncOneEncodeFrame(sTask* pCurrentTask, mfxU32 fieldProcessingCounter)
{
    MSDK_CHECK_POINTER(pCurrentTask, MFX_ERR_NULL_PTR);

    mfxStatus sts = m_mfxSession.SyncOperation(pCurrentTask->EncSyncP, MSDK_WAIT_INTERVAL);
    if (sts == MFX_ERR_GPU_HANG)
    {
        sts = doGPUHangRecovery();
        MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
        return MFX_ERR_GPU_HANG;
    }
    MSDK_CHECK_STATUS(sts, "m_mfxSession.SyncOperation failed");

    if (MFX_CODINGOPTION_ON == m_encpakInit.SingleFieldProcessing)
    {
        sts = m_TaskPool.SetFieldToStore(fieldProcessingCounter);
        MSDK_CHECK_STATUS(sts, "m_TaskPool.SetFieldToStore failed");

        if (fieldProcessingCounter == 1)
        {
            sts = SynchronizeFirstTask();
            MSDK_CHECK_STATUS(sts, "SynchronizeFirstTask failed");

            /* second field coding */
            return sts;
        }
    }

    sts = SynchronizeFirstTask();
    MSDK_CHECK_STATUS(sts, "SynchronizeFirstTask failed");

    return sts;
}

mfxStatus CEncodingPipeline::DecodeOneFrame(ExtendedSurface *pExtSurface)
{
    MFX_ITT_TASK("DecodeOneFrame");
    MSDK_CHECK_POINTER(pExtSurface, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_MORE_SURFACE;
    mfxFrameSurface1 *pDecSurf = NULL;
    pExtSurface->pSurface      = NULL;

    while (MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE < sts)
    {
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            WaitForDeviceToBecomeFree(m_mfxSession,m_LastDecSyncPoint,sts);
        }
        else if (MFX_ERR_MORE_DATA == sts)
        {
            sts = m_BSReader.ReadNextFrame(&m_mfxBS); // read more data to input bit stream
            MSDK_BREAK_ON_ERROR(sts);
        }
        else if (MFX_ERR_MORE_SURFACE == sts)
        {
            // find free surface for decoder input
            pDecSurf = m_pDecSurfaces.GetFreeSurface_FEI();
            MSDK_CHECK_POINTER(pDecSurf, MFX_ERR_MEMORY_ALLOC); // return an error if a free surface wasn't found
        }

        sts = m_pmfxDECODE->DecodeFrameAsync(&m_mfxBS, pDecSurf, &pExtSurface->pSurface, &pExtSurface->Syncp);
        if (sts == MFX_ERR_GPU_HANG)
        {
            sts = doGPUHangRecovery();
            MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
            return MFX_ERR_GPU_HANG;
        }

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
    MFX_ITT_TASK("DecodeLastFrame");
    MSDK_CHECK_POINTER(pExtSurface, MFX_ERR_NULL_PTR);

    mfxFrameSurface1 *pDecSurf = NULL;
    mfxStatus sts = MFX_ERR_MORE_SURFACE;
    pExtSurface->pSurface = NULL;

    // retrieve the buffered decoded frames
    while (MFX_ERR_MORE_SURFACE == sts || MFX_WRN_DEVICE_BUSY == sts)
    {
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            WaitForDeviceToBecomeFree(m_mfxSession,m_LastDecSyncPoint,sts);
        }
        // find free surface for decoder input
        pDecSurf = m_pDecSurfaces.GetFreeSurface_FEI();
        MSDK_CHECK_POINTER(pDecSurf, MFX_ERR_MEMORY_ALLOC); // return an error if a free surface wasn't found

        sts = m_pmfxDECODE->DecodeFrameAsync(NULL, pDecSurf, &pExtSurface->pSurface, &pExtSurface->Syncp);
        if (sts == MFX_ERR_GPU_HANG)
        {
            sts = doGPUHangRecovery();
            MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
            return MFX_ERR_GPU_HANG;
        }
    }
    return sts;
}

mfxStatus CEncodingPipeline::PreProcessOneFrame(mfxFrameSurface1* & pSurf)
{
    MFX_ITT_TASK("VPPOneFrame");
    MSDK_CHECK_POINTER(pSurf, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;
    ExtendedSurface VppExtSurface = { 0 };

    // find/wait for a free working surface
    VppExtSurface.pSurface = m_pEncSurfaces.GetFreeSurface_FEI();
    MSDK_CHECK_POINTER(VppExtSurface.pSurface, MFX_ERR_MEMORY_ALLOC);

    // make sure picture structure has the initial value
    // surfaces are reused and VPP may change this parameter in certain configurations
    VppExtSurface.pSurface->Info.PicStruct = (m_appCfg.nPicStruct == MFX_PICSTRUCT_UNKNOWN) ? pSurf->Info.PicStruct : m_mfxEncParams.mfx.FrameInfo.PicStruct;

    if (m_bNeedDRC)
    {
        VppExtSurface.pSurface->Info.Width  = m_mfxEncParams.mfx.FrameInfo.Width;
        VppExtSurface.pSurface->Info.Height = m_mfxEncParams.mfx.FrameInfo.Height;
        VppExtSurface.pSurface->Info.CropW  = m_mfxEncParams.mfx.FrameInfo.CropW;
        VppExtSurface.pSurface->Info.CropH  = m_mfxEncParams.mfx.FrameInfo.CropH;
        VppExtSurface.pSurface->Info.CropX  = 0;
        VppExtSurface.pSurface->Info.CropY  = 0;
    }

    sts = VPPOneFrame(m_pmfxVPP, &m_mfxSession, pSurf, &VppExtSurface, !!m_pmfxDS);
    if (sts == MFX_ERR_GPU_HANG)
    {
        return MFX_ERR_GPU_HANG;
    }
    MSDK_CHECK_STATUS(sts, "VPPOneFrame failed");

    pSurf = VppExtSurface.pSurface;

    return sts;
}

mfxStatus CEncodingPipeline::VPPOneFrame(MFXVideoVPP* VPPobj, MFXVideoSession* session, mfxFrameSurface1 *pSurfaceIn, ExtendedSurface *pExtSurface, bool sync)
{
    MSDK_CHECK_POINTER(pExtSurface, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;

    for (;;)
    {
        sts = VPPobj->RunFrameVPPAsync(pSurfaceIn, pExtSurface->pSurface, NULL, &pExtSurface->Syncp);
        if (sts == MFX_ERR_GPU_HANG)
        {
            sts = doGPUHangRecovery();
            MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
            return MFX_ERR_GPU_HANG;
        }

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

    /* We need to sync before any FEI interface */
    if (sync)
    {
        for (;;)
        {
            sts = session->SyncOperation(pExtSurface->Syncp, MSDK_WAIT_INTERVAL);
            if (sts == MFX_ERR_GPU_HANG)
            {
                sts = doGPUHangRecovery();
                MSDK_CHECK_STATUS(sts, "doGPUHangRecovery failed");
                return MFX_ERR_GPU_HANG;
            }

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

            MSDK_BREAK_ON_ERROR(sts);
        }
    }

    return sts;
}

mfxStatus CEncodingPipeline::ResizeFrame(mfxU32 m_frameCount, size_t &rctime)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 RCStart     = 0;
    mfxU16 tmpRCWidth  = 0;
    mfxU16 tmpRCHeight = 0;
    m_bDRCReset        = false;
    m_insertIDR        = false;

    if (m_drcStart.size() - rctime > 0)
    {
        RCStart = m_drcStart[rctime];
        if (RCStart == m_frameCount)
        {
            tmpRCWidth = m_drcWidth[rctime];
            tmpRCHeight = m_drcHeight[rctime];
            m_bDRCReset = true;
            rctime++;
        }
    }

    if (m_bDRCReset)
    {
        if (m_refDist > 1 && m_frameCount > 0)
        {
            while (MFX_ERR_NONE <= sts)
            {
                sts = EncodeOneFrame(NULL, NULL);
            }

            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
            MSDK_CHECK_STATUS(sts, "EncodeOneFrame failed");

            while (MFX_ERR_NONE == sts) {
                sts = m_TaskPool.SynchronizeFirstTask();
            }
            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_FOUND);
            MSDK_CHECK_STATUS(sts, "m_TaskPool.SynchronizeFirstTask failed");

        }
       //bDRCReset = false;
        m_mfxEncParams.mfx.FrameInfo.Width  = MSDK_ALIGN16(tmpRCWidth);
        m_mfxEncParams.mfx.FrameInfo.Height = m_isField ? MSDK_ALIGN32(tmpRCHeight) : MSDK_ALIGN16(tmpRCHeight);
        m_mfxEncParams.mfx.FrameInfo.CropW = tmpRCWidth;
        m_mfxEncParams.mfx.FrameInfo.CropH = tmpRCHeight;
        m_mfxEncParams.mfx.FrameInfo.CropX = 0;
        m_mfxEncParams.mfx.FrameInfo.CropY = 0;

        sts = m_pmfxENCODE->Reset(&m_mfxEncParams);
        MSDK_CHECK_STATUS(sts, "m_pmfxENCODE->Reset failed");

        //VPP itself ignores crops at initialization,so just re-initilize Width,Height.
        m_mfxVppParams.vpp.Out.Width  = m_mfxEncParams.mfx.FrameInfo.Width;
        m_mfxVppParams.vpp.Out.Height = m_mfxEncParams.mfx.FrameInfo.Height;

        sts = m_pmfxVPP->Reset(&m_mfxVppParams);
        MSDK_CHECK_STATUS(sts, "m_pmfxVPP->Reset failed");

        m_insertIDR = true;

        m_numMB_drc = (m_mfxVppParams.vpp.Out.Height * m_mfxVppParams.vpp.Out.Width) >> 8;
        m_numMB_drc /= (mfxU16)m_numOfFields;

        sts = UpdateVideoParams();
        MSDK_CHECK_STATUS(sts, "UpdateVideoParams failed");
    }
    return sts;
}

/* GPU Hang Recovery */

mfxStatus CEncodingPipeline::doGPUHangRecovery()
{
    msdk_printf(MSDK_STRING("GPU Hang detected. Recovering...\n"));
    mfxStatus sts = MFX_ERR_NONE;

    msdk_printf(MSDK_STRING("Recreation of pipeline...\n"));

    sts = ResetMFXComponents(&m_appCfg, false);
    MSDK_CHECK_STATUS(sts, "ResetMFXComponents failed");

    sts = ResetSessions();
    MSDK_CHECK_STATUS(sts, "ResetSessions failed");

    sts = ResetIOFiles(m_appCfg);
    MSDK_CHECK_STATUS(sts, "ResetIOFiles failed");

    m_inputTasks.Clear();
    m_ref_info.Clear();

    if (m_appCfg.bDECODE)
    {
        m_mfxBS.DataLength = 0;
        m_mfxBS.DataOffset = 0;
    }

    m_frameCount = 0;

    msdk_printf(MSDK_STRING("Pipeline successfully recovered\n"));
    return sts;
}

/* Info printing */

void CEncodingPipeline::PrintInfo()
{
    msdk_printf(MSDK_STRING("Intel(R) Media SDK FEI Sample Version %s\n"), MSDK_SAMPLE_VERSION);
    if (!m_pmfxDECODE)
        msdk_printf(MSDK_STRING("\nInput file format\t%s\n"), ColorFormatToStr(m_FileReader.m_ColorFormat));
    else
        msdk_printf(MSDK_STRING("\nInput  video\t\t%s\n"), CodecIdToStr(m_mfxDecParams.mfx.CodecId).c_str());
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

    const msdk_char* sMemType = m_bUseHWmemory ? MSDK_STRING("hw") : MSDK_STRING("system");
    msdk_printf(MSDK_STRING("Memory type\t\t%s\n"), sMemType);

    mfxVersion ver;
    m_mfxSession.QueryVersion(&ver);
    msdk_printf(MSDK_STRING("Media SDK version\t%d.%d\n"), ver.Major, ver.Minor);

    msdk_printf(MSDK_STRING("\n"));
}
