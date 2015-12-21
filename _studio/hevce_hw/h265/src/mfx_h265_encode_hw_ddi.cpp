//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#include "mfx_h265_encode_hw_ddi.h"
#if defined (MFX_VA_WIN)
#include "mfx_h265_encode_hw_d3d9.h"
#include "mfx_h265_encode_hw_d3d11.h"
#elif defined (MFX_VA_LINUX)
#include "mfx_h265_encode_vaapi.h"
#endif
#include "mfx_h265_encode_hw_ddi_trace.h"

namespace MfxHwH265Encode
{

GUID GetGUID(MfxVideoParam const & par)
{
    if (par.mfx.LowPower == MFX_CODINGOPTION_ON)
    {
        if (par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10)
            return DXVA2_Intel_LowpowerEncode_HEVC_Main10;
        return DXVA2_Intel_LowpowerEncode_HEVC_Main;
    }
    //
    if (par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10)
        return DXVA2_Intel_Encode_HEVC_Main10;

    return DXVA2_Intel_Encode_HEVC_Main;
}

DriverEncoder* CreatePlatformH265Encoder(MFXCoreInterface* core)
{
    if (core)
    {
        mfxCoreParam par = {};

        if (core->GetCoreParam(&par))
            return 0;

        switch(par.Impl & 0xF00)
        {
#if defined (MFX_VA_WIN)
        case MFX_IMPL_VIA_D3D9:
            return new D3D9Encoder;
        case MFX_IMPL_VIA_D3D11:
            return new D3D11Encoder;
#elif defined (MFX_VA_LINUX)
        case MFX_IMPL_VIA_VAAPI:
            return new VAAPIEncoder;
#endif
        default:
            return 0;
        }
    }

    return 0;
}

mfxStatus QueryHwCaps(MFXCoreInterface* core, GUID guid, ENCODE_CAPS_HEVC & caps)
{
    std::auto_ptr<DriverEncoder> ddi;

    ddi.reset(CreatePlatformH265Encoder(core));
    MFX_CHECK(ddi.get(), MFX_ERR_UNSUPPORTED);

    mfxStatus sts = ddi.get()->CreateAuxilliaryDevice(core, guid, 1920, 1088);
    MFX_CHECK_STS(sts);

    sts = ddi.get()->QueryEncodeCaps(caps);
    MFX_CHECK_STS(sts);

    DDITracer tracer;
    tracer.Trace(caps, 0);

    return sts;
}

mfxStatus CheckHeaders(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & caps)
{
    if (!( par.m_sps.sample_adaptive_offset_enabled_flag == 0
        && par.m_sps.pcm_enabled_flag == 0
        //&& par.m_sps.pcm_loop_filter_disabled_flag == 1
        && par.m_sps.log2_min_luma_coding_block_size_minus3 == 0
        && par.m_sps.log2_diff_max_min_luma_coding_block_size == 2
        && par.m_sps.bit_depth_luma_minus8 == 0
        && par.m_sps.bit_depth_chroma_minus8 == 0
        && par.m_sps.chroma_format_idc == 1
        && par.m_sps.separate_colour_plane_flag == 0
        /* && par.m_pps.cu_qp_delta_enabled_flag == 1*/))
        return MFX_ERR_UNSUPPORTED;

    if (   par.m_sps.pic_width_in_luma_samples > caps.MaxPicWidth
        || par.m_sps.pic_height_in_luma_samples > caps.MaxPicHeight
        || (UINT)(((par.m_pps.num_tile_columns_minus1 + 1) * (par.m_pps.num_tile_rows_minus1 + 1)) > 1) > caps.TileSupport)
        return MFX_ERR_UNSUPPORTED;

    return MFX_ERR_NONE;
}

mfxU16 CodingTypeToSliceType(mfxU16 ct)
{
    switch (ct)
    {
     case CODING_TYPE_I : return 2;
     case CODING_TYPE_P : return 1;
     case CODING_TYPE_B :
     case CODING_TYPE_B1:
     case CODING_TYPE_B2: return 0;
    default: assert(!"invalid coding type"); return 0xFFFF;
    }
}


DDIHeaderPacker::DDIHeaderPacker()
{
}

DDIHeaderPacker::~DDIHeaderPacker()
{
}

void DDIHeaderPacker::Reset(MfxVideoParam const & par)
{
    m_buf.resize(5 + par.mfx.NumSlice);
    m_cur = m_buf.begin();
    m_packer.Reset(par);
}

void DDIHeaderPacker::NewHeader()
{
    assert(m_buf.size());

    if (++m_cur == m_buf.end())
        m_cur = m_buf.begin();

    Zero(*m_cur);
}

ENCODE_PACKEDHEADER_DATA* DDIHeaderPacker::PackHeader(Task const & task, mfxU32 nut)
{
    NewHeader();

    switch(nut)
    {
    case VPS_NUT:
        m_packer.GetVPS(m_cur->pData, m_cur->DataLength);
        break;
    case SPS_NUT:
        m_packer.GetSPS(m_cur->pData, m_cur->DataLength);
        break;
    case PPS_NUT:
        m_packer.GetPPS(m_cur->pData, m_cur->DataLength);
        break;
    case AUD_NUT:
        {
            mfxU32 frameType = task.m_frameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B);

            if (frameType == MFX_FRAMETYPE_I)
                m_packer.GetAUD(m_cur->pData, m_cur->DataLength, 0);
            else if (frameType == MFX_FRAMETYPE_P)
                m_packer.GetAUD(m_cur->pData, m_cur->DataLength, 1);
            else
                m_packer.GetAUD(m_cur->pData, m_cur->DataLength, 2);
        }
        break;
    case PREFIX_SEI_NUT:
        m_packer.GetSEI(task, m_cur->pData, m_cur->DataLength);
        break;
    default:
        return 0;
    }
    m_cur->BufferSize = m_cur->DataLength;
    m_cur->SkipEmulationByteCount = 4;

    return &*m_cur;
}

ENCODE_PACKEDHEADER_DATA* DDIHeaderPacker::PackSliceHeader(Task const & task, mfxU32 id, mfxU32* qpd_offset)
{
    bool is1stNALU = (id == 0 && task.m_insertHeaders == 0);
    NewHeader();

    m_packer.GetSSH(task, id, m_cur->pData, m_cur->DataLength, qpd_offset);
    m_cur->BufferSize = m_cur->DataLength;
    m_cur->SkipEmulationByteCount = 3 + is1stNALU;
    m_cur->DataLength *= 8;

    return &*m_cur;
}

}; // namespace MfxHwH265Encode
