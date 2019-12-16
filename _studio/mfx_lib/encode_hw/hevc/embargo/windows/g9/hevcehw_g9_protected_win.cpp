// Copyright (c) 2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && !defined(MFX_VA_LINUX)

#include "hevcehw_g9_protected_win.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen9;
using namespace HEVCEHW::Windows;
using namespace HEVCEHW::Windows::Gen9;

void Protected::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_PAVP_OPTION].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        auto& src = *(const mfxExtPAVPOption*)pSrc;
        auto& dst = *(mfxExtPAVPOption*)pDst;

        dst.CipherCounter.Count = src.CipherCounter.Count;
        dst.CipherCounter.IV    = src.CipherCounter.IV;
        dst.CounterIncrement    = src.CounterIncrement;
        dst.CounterType         = src.CounterType;
    });

    blocks.m_ebCopySupported[MFX_EXTBUFF_ENCODER_WIDI_USAGE].emplace_back(
        [](const mfxExtBuffer* /*pSrc*/, mfxExtBuffer* pDst) -> void
    {
        auto& dst = *(mfxExtAVCEncoderWiDiUsage*)pDst;
        dst.reserved[0] = 1;
    });
}

bool IsWiDiMode(StorageR& glob)
{
    const mfxExtAVCEncoderWiDiUsage& eb = ExtBuffer::Get(Glob::VideoParam::Get(glob));
    return !!eb.reserved[0];
}

mfxU64 SwapEndian(mfxU64 val)
{
    return
        ((val << 56) & 0xff00000000000000) | ((val << 40) & 0x00ff000000000000) |
        ((val << 24) & 0x0000ff0000000000) | ((val <<  8) & 0x000000ff00000000) |
        ((val >>  8) & 0x00000000ff000000) | ((val >> 24) & 0x0000000000ff0000) |
        ((val >> 40) & 0x000000000000ff00) | ((val >> 56) & 0x00000000000000ff);
}

bool Increment(
    mfxAES128CipherCounter & aesCounter,
    mfxExtPAVPOption const & extPavp)
{
    bool wrapped = false;

    if (extPavp.EncryptionType == MFX_PAVP_AES128_CTR)
    {
        mfxU64 tmp = SwapEndian(aesCounter.Count) + extPavp.CounterIncrement;

        if (extPavp.CounterType == MFX_PAVP_CTR_TYPE_A)
            tmp &= 0xffffffff;

        wrapped = (tmp < extPavp.CounterIncrement);
        aesCounter.Count = SwapEndian(tmp);
    }

    return wrapped;
}

void Decrement(
    mfxAES128CipherCounter & aesCounter,
    mfxExtPAVPOption const & extPavp)
{
    if (extPavp.EncryptionType == MFX_PAVP_AES128_CTR)
    {
        mfxU64 tmp = SwapEndian(aesCounter.Count) - extPavp.CounterIncrement;

        if (extPavp.CounterType == MFX_PAVP_CTR_TYPE_A)
            tmp &= 0xffffffff;

        aesCounter.Count = SwapEndian(tmp);
    }
}

void Protected::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [this](const mfxVideoParam& /*in*/, mfxVideoParam& par, StorageW& /*global*/) -> mfxStatus
    {
        MFX_CHECK(!CheckOrZero<mfxU16>(par.Protected
            , MFX_PROTECTION_PAVP, MFX_PROTECTION_GPUCP_PAVP, 0), MFX_ERR_UNSUPPORTED);
        MFX_CHECK(par.Protected, MFX_ERR_NONE);

        mfxExtPAVPOption* pPAVP = ExtBuffer::Get(par);
        MFX_CHECK(pPAVP, MFX_ERR_NONE);
        auto& PAVP = *pPAVP;

        MFX_CHECK(!CheckOrZero<mfxU16>(PAVP.EncryptionType
            , MFX_PAVP_AES128_CTR
            , MFX_PAVP_AES128_CBC
            , 0), MFX_ERR_UNSUPPORTED);

        MFX_CHECK(!CheckOrZero<mfxU16>(PAVP.CounterType
            , MFX_PAVP_CTR_TYPE_A
            , MFX_PAVP_CTR_TYPE_B
            , MFX_PAVP_CTR_TYPE_C
            , 0), MFX_ERR_UNSUPPORTED);

        mfxU32 changed = 0;
        bool bCheck = (PAVP.CounterType == MFX_PAVP_CTR_TYPE_A && PAVP.CipherCounter.Count);
        changed += bCheck && CheckMinOrClip(PAVP.CipherCounter.Count, 0x0000000100000000u);
        changed += bCheck && CheckMaxOrClip(PAVP.CipherCounter.Count, 0xffffffff00000000u);

        bCheck = !!PAVP.CounterIncrement;
        changed += bCheck && CheckMinOrClip(PAVP.CounterIncrement, 0x0C000u);
        changed += bCheck && CheckMaxOrClip(PAVP.CounterIncrement, 0xC0000u);

        MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        return MFX_ERR_NONE;
    });
}

void Protected::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW& /*strg*/, StorageRW&)
    {
        MFX_CHECK(par.Protected, MFX_ERR_NONE);
        mfxExtPAVPOption* pPAVP = ExtBuffer::Get(par);
        MFX_CHECK(pPAVP, MFX_ERR_NONE);
        auto& PAVP = *pPAVP;

        SetDefault<mfxU16>(PAVP.EncryptionType, MFX_PAVP_AES128_CTR);
        SetDefault<mfxU16>(PAVP.CounterType, MFX_PAVP_CTR_TYPE_A);
        SetDefault<mfxU32>(PAVP.CounterIncrement, 0xC000u);

        return MFX_ERR_NONE;
    });
}

void Protected::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_Init
        , [this](StorageRW& global, StorageRW& local) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(par.Protected, MFX_ERR_NONE);

        Tmp::RecInfo::Get(local).Type |= m_recFlag;

        const mfxExtPAVPOption& PAVP = ExtBuffer::Get(par);

        m_aesCounter = PAVP.CipherCounter;

        m_prevCounter = m_aesCounter;
        Decrement(m_prevCounter, PAVP);

        MFX_CHECK(!global.Contains(Glob::RealState::Key), MFX_ERR_NONE); //!reset

        m_dx9.Desc.SampleWidth          = par.mfx.FrameInfo.Width;
        m_dx9.Desc.SampleHeight         = par.mfx.FrameInfo.Height;
        m_dx9.Desc.Format               = (D3DDDIFORMAT)(MFX_FOURCC_NV12);
        m_dx9.ECD.EncryptionMode        = DXVA2_INTEL_PAVP;
        m_dx9.ECD.CodingFunction        = (ENCODE_WIDI * IsWiDiMode(global)) | ENCODE_ENC_PAK;
        m_dx9.AES                       = { PAVP.CipherCounter.IV, PAVP.CipherCounter.Count };
        m_dx9.Mode                      = { PAVP.EncryptionType,   PAVP.CounterType };
        m_dx9.ECD.CounterAutoIncrement  = PAVP.CounterIncrement;
        m_dx9.ECD.pVideoDesc            = &m_dx9.Desc;
        m_dx9.ECD.pInitialCipherCounter = &m_dx9.AES;
        m_dx9.ECD.pPavpEncryptionMode   = &m_dx9.Mode;

        auto pDdiInit = make_storable<Tmp::DDI_InitParam::TRef>();
        DDIExecParam execPar = {};

        if (Glob::VideoCore::Get(global).GetVAType() == MFX_HW_D3D9)
        {
            execPar.Function  = AUXDEV_CREATE_ACCEL_SERVICE;
            execPar.In.pData  = &Glob::GUID::Get(global);
            execPar.In.Size   = sizeof(GUID);
            execPar.Out.pData = &m_dx9.ECD;
            execPar.Out.Size  = sizeof(m_dx9.ECD);

            pDdiInit->push_back(execPar);
        }
        else
        {
            m_dx11.ResetDeviceIn.bTemporal = false;
            m_dx11.ResetDeviceIn.desc = { Glob::GUID::Get(global), m_dx9.Desc.SampleWidth, m_dx9.Desc.SampleHeight, DXGI_FORMAT_NV12 };
            m_dx11.ResetDeviceIn.config.ConfigDecoderSpecific         = m_dx9.ECD.CodingFunction;
            m_dx11.ResetDeviceIn.config.guidConfigBitstreamEncryption = m_dx9.ECD.EncryptionMode;

            execPar = {};
            execPar.Function  = AUXDEV_CREATE_ACCEL_SERVICE;
            execPar.In.pData  = &m_dx11.ResetDeviceIn;
            execPar.In.Size   = sizeof(m_dx11.ResetDeviceIn);
            execPar.Out.pData = &m_dx11.ResetDeviceOut;
            execPar.Out.Size  = sizeof(m_dx11.ResetDeviceOut);

            pDdiInit->push_front(execPar);


            m_dx11.AES  = { PAVP.CipherCounter.IV, PAVP.CipherCounter.Count };
            m_dx11.Mode = { PAVP.EncryptionType,   PAVP.CounterType };
            m_dx11.Set  = { PAVP.CounterIncrement, &m_dx11.AES, &m_dx11.Mode };

            execPar = {};
            execPar.Function = ENCODE_ENCRYPTION_SET_ID;
            execPar.In.pData = &m_dx11.Set;
            execPar.In.Size  = sizeof(m_dx11.Set);

            pDdiInit->push_back(execPar);
        }

        local.Insert(Tmp::DDI_InitParam::Key, std::move(pDdiInit));

        return MFX_ERR_NONE;
    });
}

void Protected::FrameSubmit(const FeatureBlocks& blocks, TPushFS Push)
{
    Push(BLK_SetBsInfo
        , [this, &blocks](
            const mfxEncodeCtrl* /*pCtrl*/
            , const mfxFrameSurface1* /*pSurf*/
            , mfxBitstream& bs
            , StorageW& global
            , StorageRW& local) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(par.Protected, MFX_ERR_NONE);

        MFX_CHECK_NULL_PTR1(bs.EncryptedData);
        auto& ed   = *bs.EncryptedData;
        auto pInfo = make_storable<BsDataInfo>(BsDataInfo{});

        pInfo->Data       = ed.Data;
        pInfo->DataLength = ed.DataLength;
        pInfo->DataOffset = ed.DataOffset;
        pInfo->MaxLength  = ed.MaxLength;

        local.Insert(Tmp::BsDataInfo::Key, std::move(pInfo));

        return MFX_ERR_NONE;
    });
}

void Protected::InitTask(const FeatureBlocks& /*blocks*/, TPushIT Push)
{
    Push(BLK_IncCounter
        , [this](
            mfxEncodeCtrl* /*pCtrl*/
            , mfxFrameSurface1* /*pSurf*/
            , mfxBitstream* /*pBs*/
            , StorageW& global
            , StorageW& /*task*/) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(par.Protected, MFX_ERR_NONE);

        MFX_CHECK(!Increment(m_aesCounter, ExtBuffer::Get(par)), MFX_WRN_OUT_OF_RANGE);

        return MFX_ERR_NONE;
    });
}

void Protected::PostReorderTask(const FeatureBlocks& /*blocks*/, TPushPostRT Push)
{
    Push(BLK_SetTaskCounter
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(par.Protected, MFX_ERR_NONE);

        Increment(m_prevCounter, ExtBuffer::Get(par));
        m_task[&s_task] = m_prevCounter;

        return MFX_ERR_NONE;
    });
}

void Protected::QueryTask(const FeatureBlocks& /*blocks*/, TPushQT Push)
{
    Push(BLK_GetFeedback
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(par.Protected, MFX_ERR_NONE);
        MFX_CHECK(Glob::EncodeCaps::Get(global).HWCounterAutoIncrementSupport, MFX_ERR_NONE);
        MFX_CHECK(IsWiDiMode(global), MFX_ERR_NONE);

        auto  id    = Task::Common::Get(s_task).StatusReportId;
        auto& ddiFB = Glob::DDI_Feedback::Get(global);
        auto  pFB   = (const ENCODE_QUERY_STATUS_PARAMS*)ddiFB.Get(id);

        MFX_CHECK(ddiFB.bNotReady || (pFB && pFB->bStatus == ENCODE_NOTREADY), MFX_ERR_NONE);

        auto& cntr = m_task[&s_task];
        cntr.Count = pFB->aes_counter.Counter;
        cntr.IV    = pFB->aes_counter.IV;

        return MFX_ERR_NONE;
    });

    Push(BLK_SetBsInfo
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& par = Glob::VideoParam::Get(global);
        MFX_CHECK(par.Protected, MFX_ERR_NONE);

        auto& task          = Task::Common::Get(s_task);
        auto  taskCounterIt = m_task.find(&s_task);

        MFX_CHECK(task.pBsOut && task.pBsOut->EncryptedData, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(taskCounterIt != m_task.end(), MFX_ERR_UNDEFINED_BEHAVIOR);

        auto& ed = *task.pBsOut->EncryptedData;

        task.pBsData            = ed.Data + ed.DataOffset + ed.DataLength;
        task.pBsDataLength      = &ed.DataLength;
        task.BsBytesAvailable   = ed.MaxLength - ed.DataOffset - ed.DataLength;
        task.bDontPatchBS       = true;

        // Driver encyptes aligned number of bytes after encoding
        // So aligned number of bytes should be copied.
        //
        // To let app know which bytes are padding added by encryption
        // DataLength in mfxEncryptedData is set to what returned from Query (may be not aligned)
        auto alignedLen = mfx::align2_value(task.BsDataLength, 16);

        ed.DataLength     -= (alignedLen - task.BsDataLength);
        task.BsDataLength =  alignedLen;
        ed.CipherCounter  =  taskCounterIt->second;

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)