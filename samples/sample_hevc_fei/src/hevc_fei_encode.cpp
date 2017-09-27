/******************************************************************************\
Copyright (c) 2017, Intel Corporation
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

#include "fei_utils.h"
#include "hevc_fei_encode.h"

FEI_Encode::FEI_Encode(MFXVideoSession* session, mfxHDL hdl, MfxVideoParamsWrapper& encode_pars,
        const msdk_char* dst_output, const msdk_char* mvpInFile, PredictorsRepaking* repacker)
    : m_pmfxSession(session)
    , m_mfxENCODE(*m_pmfxSession)
    , m_buf_allocator(hdl)
    , m_videoParams(encode_pars)
    , m_syncPoint(0)
    , m_dstFileName(dst_output)
{
    if (0 != msdk_strlen(mvpInFile))
    {
        m_pFile_MVP_in.reset(new FileHandler(mvpInFile, MSDK_STRING("rb")));
    }

    m_encodeCtrl.FrameType = MFX_FRAMETYPE_UNKNOWN;
    m_encodeCtrl.QP = m_videoParams.mfx.QPI;
    MSDK_ZERO_MEMORY(m_bitstream);

    m_repacker.reset(repacker);
}

FEI_Encode::~FEI_Encode()
{
    m_mfxENCODE.Close();
    m_pmfxSession = NULL;
    WipeMfxBitstream(&m_bitstream);

    try
    {
        mfxExtFeiHevcEncMVPredictors* pMVP = m_encodeCtrl.GetExtBuffer<mfxExtFeiHevcEncMVPredictors>();
        if (pMVP)
        {
            m_buf_allocator.Free(pMVP);
        }
    }
    catch(mfxError& ex)
    {
        msdk_printf("Exception raised in FEI Encode destructor sts = %d, msg = %s\n", ex.GetStatus(), ex.GetMessage().c_str());
    }
    catch(...)
    {
        msdk_printf("Exception raised in FEI Encode destructor\n");
    }

}

mfxStatus FEI_Encode::PreInit()
{
    // call Query to check that Encode's parameters are valid
    mfxStatus sts = Query();
    MSDK_CHECK_STATUS(sts, "FEI Encode Query failed");

    mfxU32 nEncodedDataBufferSize = m_videoParams.mfx.FrameInfo.Width * m_videoParams.mfx.FrameInfo.Height * 4;
    sts = InitMfxBitstream(&m_bitstream, nEncodedDataBufferSize);
    MSDK_CHECK_STATUS_SAFE(sts, "InitMfxBitstream failed", WipeMfxBitstream(&m_bitstream));

    // add FEI frame ctrl with default values
    mfxExtFeiHevcEncFrameCtrl* ctrl = m_encodeCtrl.AddExtBuffer<mfxExtFeiHevcEncFrameCtrl>();
    MSDK_CHECK_POINTER(ctrl, MFX_ERR_NOT_INITIALIZED);
    ctrl->SubPelMode         = 3;
    ctrl->SearchWindow       = 5;
    ctrl->NumFramePartitions = 4;

    if (!ctrl->SearchWindow)
    {
        ctrl->SearchPath     = 0;
        ctrl->LenSP          = 57;
        ctrl->RefWidth       = 32;
        ctrl->RefHeight      = 32;
        ctrl->AdaptiveSearch = 1;
    }

    // allocate ext buffer for input MV predictors required for Encode.
    if (m_repacker.get() || m_pFile_MVP_in.get())
    {
        mfxExtFeiHevcEncMVPredictors* pMVP = m_encodeCtrl.AddExtBuffer<mfxExtFeiHevcEncMVPredictors>();
        MSDK_CHECK_POINTER(pMVP, MFX_ERR_NOT_INITIALIZED);
        pMVP->VaBufferID = VA_INVALID_ID;
    }

    sts = ResetExtBuffers(m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI Encode ResetExtBuffers failed");

    return MFX_ERR_NONE;
}

mfxStatus FEI_Encode::ResetExtBuffers(const MfxVideoParamsWrapper & videoParams)
{
    /* Driver uses blocks of 16x16 pixels for CTUs representation.
       The layout of data inside those blocks is related to CTU size,
       but the buffer size itself - doesn't.
    */
    static const mfxU32 element_size = 16; // Buffers granularity is always 16x16 blocks
    mfxU32 numElements = (MSDK_ALIGN32(videoParams.mfx.FrameInfo.Width) / element_size) * (MSDK_ALIGN32(videoParams.mfx.FrameInfo.Height) / element_size);

    try
    {
        mfxExtFeiHevcEncMVPredictors* pMVP = m_encodeCtrl.GetExtBuffer<mfxExtFeiHevcEncMVPredictors>();
        if (pMVP)
        {
            m_buf_allocator.Free(pMVP);

            m_buf_allocator.Alloc(pMVP, numElements);
        }

        // TODO: add condition when buffer is required
        // mfxExtFeiHevcEncQP* pQP = m_encodeCtrl.GetExtBuffer<mfxExtFeiHevcEncQP>();
        // if (pQP)
        // {
        //     m_buf_allocator.Free(pQP);
        //
        //     m_buf_allocator.Alloc(pQP, numElements);
        // }
    }
    catch (mfxError& ex)
    {
        MSDK_CHECK_STATUS(ex.GetStatus(), ex.GetMessage());
    }
    catch(...)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

mfxStatus FEI_Encode::Query()
{
    mfxStatus sts = m_mfxENCODE.Query(&m_videoParams, &m_videoParams);
    MSDK_CHECK_WRN(sts, "FEI Encode Query");

    return sts;
}

mfxStatus FEI_Encode::Init()
{
    mfxStatus sts = m_FileWriter.Init(m_dstFileName.c_str());
    MSDK_CHECK_STATUS(sts, "FileWriter Init failed");

    sts = m_mfxENCODE.Init(&m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI Encode Init failed");
    MSDK_CHECK_WRN(sts, "FEI Encode Init");

    // just in case, call GetVideoParam to get current Encoder state
    sts = m_mfxENCODE.GetVideoParam(&m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI Encode GetVideoParam failed");

    return sts;
}

mfxStatus FEI_Encode::QueryIOSurf(mfxFrameAllocRequest* request)
{
    return m_mfxENCODE.QueryIOSurf(&m_videoParams, request);
}

mfxFrameInfo* FEI_Encode::GetFrameInfo()
{
    return &m_videoParams.mfx.FrameInfo;
}

mfxStatus FEI_Encode::Reset(mfxVideoParam& par)
{
    m_videoParams = par;
    mfxStatus sts = m_mfxENCODE.Reset(&m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI Encode Reset failed");
    MSDK_CHECK_WRN(sts, "FEI Encode Reset");

    // just in case, call GetVideoParam to get current Encoder state
    sts = m_mfxENCODE.GetVideoParam(&m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI Encode GetVideoParam failed");

    return sts;
}

const MfxVideoParamsWrapper& FEI_Encode::GetVideoParam()
{
    return m_videoParams;
}

// in encoded order
mfxStatus FEI_Encode::EncodeFrame(HevcTask* task)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (task)
    {
        sts = SetCtrlParams(*task);
        MSDK_CHECK_STATUS(sts, "FEI Encode::SetCtrlParams failed");
    }

    mfxFrameSurface1* pSurf = task->m_surf;

    sts = EncodeFrame(pSurf);
    MSDK_CHECK_STATUS(sts, "FEI EncodeFrame failed");

    return sts;
}

mfxStatus FEI_Encode::EncodeFrame(mfxFrameSurface1* pSurf)
{
    mfxStatus sts = MFX_ERR_NONE;

    for (;;)
    {

        sts = m_mfxENCODE.EncodeFrameAsync(&m_encodeCtrl, pSurf, &m_bitstream, &m_syncPoint);
        MSDK_CHECK_WRN(sts, "FEI EncodeFrameAsync");

        if (MFX_ERR_NONE < sts && !m_syncPoint) // repeat the call if warning and no output
        {
            if (MFX_WRN_DEVICE_BUSY == sts)
            {
                WaitForDeviceToBecomeFree(*m_pmfxSession, m_syncPoint, sts);
            }
            continue;
        }

        if (MFX_ERR_NONE <= sts && m_syncPoint) // ignore warnings if output is available
        {
            sts = SyncOperation();
            break;
        }

        if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
        {
            sts = AllocateSufficientBuffer();
            MSDK_CHECK_STATUS(sts, "FEI Encode: AllocateSufficientBuffer failed");
        }

        MSDK_BREAK_ON_ERROR(sts);

    } // for (;;)

    // when pSurf==NULL, MFX_ERR_MORE_DATA indicates all cached frames within encoder were drained, return as is
    // otherwise encoder need more input surfaces, ignore status
    if (MFX_ERR_MORE_DATA == sts && pSurf)
    {
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    }

    return sts;
}

mfxStatus FEI_Encode::SetCtrlParams(const HevcTask& task)
{
    m_encodeCtrl.FrameType = task.m_frameType;

    if (m_repacker.get() || m_pFile_MVP_in.get())
    {
        mfxExtFeiHevcEncFrameCtrl* ctrl = m_encodeCtrl.GetExtBuffer<mfxExtFeiHevcEncFrameCtrl>();
        MSDK_CHECK_POINTER(ctrl, MFX_ERR_NOT_INITIALIZED);

        // switch predictors off for I-frames
        ctrl->MVPredictor = (m_encodeCtrl.FrameType & MFX_FRAMETYPE_I) == 0;
        ctrl->NumMvPredictors[0] = ctrl->NumMvPredictors[1] = 0;
        if (m_encodeCtrl.FrameType & MFX_FRAMETYPE_P)
        {
            ctrl->NumMvPredictors[0] = m_videoParams.GetExtBuffer<mfxExtCodingOption3>()->NumRefActiveP[0];
        }
        else if (m_encodeCtrl.FrameType & MFX_FRAMETYPE_B)
        {
            ctrl->NumMvPredictors[0] = m_videoParams.GetExtBuffer<mfxExtCodingOption3>()->NumRefActiveBL0[0];
            ctrl->NumMvPredictors[1] = m_videoParams.GetExtBuffer<mfxExtCodingOption3>()->NumRefActiveBL1[0];
        }

        mfxExtFeiHevcEncMVPredictors* pMVP = m_encodeCtrl.GetExtBuffer<mfxExtFeiHevcEncMVPredictors>();
        MSDK_CHECK_POINTER(pMVP, MFX_ERR_NOT_INITIALIZED);
        AutoBufferLocker<mfxExtFeiHevcEncMVPredictors> lock(m_buf_allocator, *pMVP);

        if (m_repacker.get())
        {
            mfxStatus sts = m_repacker->RepackPredictors(task, *pMVP);
            MSDK_CHECK_STATUS(sts, "FEI Encode::RepackPredictors failed");
        }
        else
        {
            mfxStatus sts = m_pFile_MVP_in->Read(pMVP->Data, pMVP->DataSize, 1);
            MSDK_CHECK_STATUS(sts, "FEI Encode. Read MV predictors failed");
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus FEI_Encode::SyncOperation()
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_pmfxSession->SyncOperation(m_syncPoint, MSDK_WAIT_INTERVAL);
    MSDK_CHECK_STATUS(sts, "FEI Encode: SyncOperation failed");

    sts = m_FileWriter.WriteNextFrame(&m_bitstream);
    MSDK_CHECK_STATUS(sts, "FEI Encode: WriteNextFrame failed");

    return sts;
}

mfxStatus FEI_Encode::AllocateSufficientBuffer()
{
    // find out the required buffer size
    // call GetVideoParam to get current Encoder state
    mfxStatus sts = m_mfxENCODE.GetVideoParam(&m_videoParams);
    MSDK_CHECK_STATUS(sts, "FEI Encode GetVideoParam failed");

    // reallocate bigger buffer for output
    sts = ExtendMfxBitstream(&m_bitstream, m_videoParams.mfx.BufferSizeInKB * 1000);
    MSDK_CHECK_STATUS_SAFE(sts, "ExtendMfxBitstream failed", WipeMfxBitstream(&m_bitstream));

    return sts;
}
