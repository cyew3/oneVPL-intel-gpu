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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_MFE)

#include "hevcehw_g12_mfe_win.h"
#include "hevcehw_g12_data.h"
#include "libmfx_core_interface.h"
#include "hevcehw_g11_d3d11_win.h"
#include "hevcehw_g11_blocking_sync_win.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen12;

void Windows::Gen12::MFE::InitExternal(const FeatureBlocks& /*blocks*/, TPushIE Push)
{
    Push(BLK_SetCallChains,
        [this](const mfxVideoParam&, StorageRW& strg, StorageRW&) -> mfxStatus
    {
        auto& defaults = Glob::Defaults::Get(strg);
        auto& bSet = defaults.SetForFeature[GetID()];
        MFX_CHECK(!bSet, MFX_ERR_NONE);

        defaults.GetGUID.Push([](
            Defaults::TGetGUID::TExt prev
            , const Defaults::Param& par
            , ::GUID& guid)
        {
            const mfxExtMultiFrameParam* pMFPar = ExtBuffer::Get(par.mvp);
            bool bUseMfeGuid =
                pMFPar && ((pMFPar->MaxNumFrames > 1) || (pMFPar->MFMode >= MFX_MF_AUTO));

            if (bUseMfeGuid)
            {
                guid = DXVA2_Intel_MFE;
                return true;
            }

            return prev(par, guid);
        });

        bSet = true;
        return MFX_ERR_NONE;
    });
}

void Windows::Gen12::MFE::Query1NoCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_SetCallChains,
        [this](const mfxVideoParam&, mfxVideoParam&, StorageRW& strg) -> mfxStatus
    {
        using TIn   = Gen11::DDI_D3D11::CreateDeviceIn;
        using TOut  = Gen11::DDI_D3D11::CreateDeviceOut;
        using TCall = Glob::DDI_Execute::TRef::TExt;

        MFX_CHECK(!m_bDDIExecSet, MFX_ERR_NONE);

        auto& ddiExec = Glob::DDI_Execute::Get(strg);

        auto GetCaps = [&](TCall /*prev*/, const Gen11::DDIExecParam& ep)
        {
            auto pCaps = (ENCODE_CAPS_HEVC*)(m_pMfeAdapter->GetCaps(CODEC_HEVC));
            MFX_CHECK(pCaps, MFX_ERR_UNSUPPORTED);

            Deref<ENCODE_CAPS_HEVC>(ep.Out) = *pCaps;

            return MFX_ERR_NONE;
        };
        auto QueryInfo = [&](TCall prev, const Gen11::DDIExecParam& ep)
        {
            auto epCopy = ep;
            epCopy.In.pData = &m_codec;
            epCopy.In.Size  = sizeof(m_codec);

            return prev(ep);
        };
        auto Submit = [&](TCall prev, const Gen11::DDIExecParam& ep)
        {
            auto& cbi = Deref<ENCODE_EXECUTE_PARAMS>(ep.In);
            m_cbd.resize(cbi.NumCompBuffers + 1 + m_bSendEvent);

            auto& siCBD = m_cbd.front();
            siCBD = {};
            siCBD.CompressedBufferType = (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_STREAMINFO;
            siCBD.pCompBuffer          = &m_streamInfo;
            siCBD.DataSize             = sizeof(m_streamInfo);

            auto itCBDst = std::next(m_cbd.begin());

            if (m_bSendEvent)
            {
                auto& evtCBD = *itCBDst;
                evtCBD.CompressedBufferType = (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_EVENT;
                siCBD.pCompBuffer           = &m_mfeGpuEvent;
                siCBD.DataSize              = sizeof(m_mfeGpuEvent);
                ++itCBDst;
            }

            std::copy_n(cbi.pCompressedBuffers, cbi.NumCompBuffers, itCBDst);

            cbi.pCompressedBuffers = m_cbd.data();
            cbi.NumCompBuffers     = UINT(m_cbd.size());

            auto sts = prev(ep);
            MFX_CHECK_STS(sts);

            return m_pMfeAdapter->Submit(m_streamInfo.StreamId, m_timeout, false);
        };
        auto QueryStatus = [&](TCall prev, const Gen11::DDIExecParam& ep)
        {
            Deref<ENCODE_QUERY_STATUS_PARAMS_DESCR>(ep.In).StreamID = m_streamInfo.StreamId;
            return prev(ep);
        };
        auto InitMFE = [&](TCall prev, const Gen11::DDIExecParam& ep)
        {
            auto& core = Glob::VideoCore::Get(strg);
            auto& in   = Deref<TIn>(ep.In);
            auto& out  = Deref<TOut>(ep.Out);
            auto* pVideoEncoder
                = QueryCoreInterface<ComPtrCore<MFEDXVAEncoder>>(&core, MFXMFEHEVCENCODER_SEARCH_GUID);

            ThrowAssert(!pVideoEncoder, "QueryCoreInterface failed for MFXMFEHEVCENCODER_SEARCH_GUID");

            if (!pVideoEncoder->get())
                *pVideoEncoder = new MFEDXVAEncoder;

            m_pMfeAdapter = pVideoEncoder->get();
            MFX_CHECK(m_pMfeAdapter, MFX_ERR_UNSUPPORTED);

            D3D11Interface* pD3d11 = QueryCoreInterface<D3D11Interface>(&core);
            MFX_CHECK(pD3d11, MFX_ERR_DEVICE_FAILED)

            out.vdevice = pD3d11->GetD3D11VideoDevice(in.bTemporal);
            MFX_CHECK(out.vdevice, MFX_ERR_DEVICE_FAILED);

            out.vcontext = pD3d11->GetD3D11VideoContext(in.bTemporal);
            MFX_CHECK(out.vcontext, MFX_ERR_DEVICE_FAILED);

            auto sts = m_pMfeAdapter->Create(out.vdevice, out.vcontext, in.desc.SampleWidth, in.desc.SampleHeight);
            MFX_CHECK_STS(sts);

            out.vdecoder = m_pMfeAdapter->GetVideoDecoder();
            MFX_CHECK(out.vdecoder, MFX_ERR_UNSUPPORTED);

            return MFX_ERR_NONE;
        };

        ddiExec.Push(
            [&](TCall prev, const Gen11::DDIExecParam& ep)
        {
            bool bInitMfe     = ep.Function == AUXDEV_CREATE_ACCEL_SERVICE && Deref<TIn>(ep.In).desc.Guid == DXVA2_Intel_MFE;
            bool bGetCaps     = m_pMfeAdapter && ep.Function == ENCODE_QUERY_ACCEL_CAPS_ID;
            bool bQueryInfo   = m_pMfeAdapter && (ep.Function == ENCODE_FORMAT_COUNT_ID || ep.Function == ENCODE_FORMATS_ID);
            bool bSubmit      = m_pMfeAdapter && ep.Function == ENCODE_ENC_PAK_ID;
            bool bQueryStatus = m_pMfeAdapter && ep.Function == ENCODE_QUERY_STATUS_ID;

            MFX_CHECK(!bInitMfe,     InitMFE(prev, ep));
            MFX_CHECK(!bGetCaps,     GetCaps(prev, ep));
            MFX_CHECK(!bQueryInfo,   QueryInfo(prev, ep));
            MFX_CHECK(!bSubmit,      Submit(prev, ep));
            MFX_CHECK(!bQueryStatus, QueryStatus(prev, ep));

            return prev(ep);
        });

        m_bDDIExecSet = true;

        return MFX_ERR_NONE;
    });
}

void Windows::Gen12::MFE::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_Init
        , [this](StorageRW& strg, StorageRW&) -> mfxStatus
    {
        MFX_CHECK(m_pMfeAdapter, MFX_ERR_NONE);

        auto& par     = Glob::VideoParam::Get(strg);
        bool  bFields = (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE);
        auto& fi      = par.mfx.FrameInfo;

        m_streamInfo.CodecId = CODEC_HEVC;
        uint64_t timeout = (mfxU64(fi.FrameRateExtD) * 1000000 / fi.FrameRateExtN) / (bFields + 1);

        m_pMfeAdapter->Join(ExtBuffer::Get(par), m_streamInfo, timeout);

        return MFX_ERR_NONE;
    });
}

void Windows::Gen12::MFE::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_UpdateDDITask
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        using TaskEvent = Gen11::BlockingSync::TaskEvent;

        m_bSendEvent = false;

        MFX_CHECK(m_pMfeAdapter, MFX_ERR_NONE);

        auto  IsEvent   = [](const Gen11::DDIExecParam& par) { return par.Function == DXVA2_PRIVATE_SET_GPU_TASK_EVENT_HANDLE; };
        auto& submit    = Glob::DDI_SubmitParam::Get(global);
        auto  itEvent   = std::find_if(submit.begin(), submit.end(), IsEvent);

        MFX_CHECK(itEvent != submit.end(), MFX_ERR_NONE);

        auto& taskEvent = TaskEvent::Get(s_task);

        m_mfeGpuEvent.StatusReportFeedbackNumber = taskEvent.ReportID;
        m_mfeGpuEvent.gpuSyncEvent               = taskEvent.Handle.gpuSyncEvent;

        m_bSendEvent = true;

        submit.erase(itEvent);

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)