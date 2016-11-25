//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
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
#include "ipps.h"

namespace MfxHwH265Encode
{

const GUID GuidTable[2][2][3] =
{
    // LowPower = OFF
    {
        // BitDepthLuma = 8
        {
            /*420*/ DXVA2_Intel_Encode_HEVC_Main,
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            /*422*/ DXVA2_Intel_Encode_HEVC_Main422,
            /*444*/ DXVA2_Intel_Encode_HEVC_Main444
#endif
        },
        // BitDepthLuma = 10
        {
            /*420*/ DXVA2_Intel_Encode_HEVC_Main10,
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            /*422*/ DXVA2_Intel_Encode_HEVC_Main422_10,
            /*444*/ DXVA2_Intel_Encode_HEVC_Main444_10
#endif
        }
    },
    // LowPower = ON
    
#if defined(PRE_SI_TARGET_PLATFORM_GEN10)
    {
        // BitDepthLuma = 8
        {
            /*420*/ DXVA2_Intel_LowpowerEncode_HEVC_Main,
    #if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            /*422*/ DXVA2_Intel_LowpowerEncode_HEVC_Main422,
            /*444*/ DXVA2_Intel_LowpowerEncode_HEVC_Main444
    #endif
        },
        // BitDepthLuma = 10
        {
            /*420*/ DXVA2_Intel_LowpowerEncode_HEVC_Main10,
    #if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            /*422*/ DXVA2_Intel_LowpowerEncode_HEVC_Main422_10,
            /*444*/ DXVA2_Intel_LowpowerEncode_HEVC_Main444_10
    #endif
        }
    }
#endif
};

GUID GetGUID(MfxVideoParam const & par)
{
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    bool is10bit =
        (   par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10
         || par.m_ext.CO3.TargetBitDepthLuma == 10);
    mfxU16 cfId = Clip3<mfxU16>(MFX_CHROMAFORMAT_YUV420, MFX_CHROMAFORMAT_YUV444, par.m_ext.CO3.TargetChromaFormatPlus1 - 1) - MFX_CHROMAFORMAT_YUV420;

    if (par.m_platform.CodeName < MFX_PLATFORM_ICELAKE)
        cfId = 0; // platforms below ICL do not support Main422/Main444 profile, using Main instead.
#else
    bool is10bit =
        (   par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10
         || par.mfx.FrameInfo.BitDepthLuma == 10
         || par.mfx.FrameInfo.FourCC == MFX_FOURCC_P010);
    mfxU16 cfId = 0;
#endif
    if (par.m_platform.CodeName < MFX_PLATFORM_KABYLAKE)
        is10bit = false;

    return GuidTable[IsOn(par.mfx.LowPower)][is10bit] [cfId];
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

    return sts;
}

mfxStatus CheckHeaders(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & caps)
{
    MFX_CHECK_COND(
           par.m_sps.log2_min_luma_coding_block_size_minus3 == 0
        && par.m_sps.separate_colour_plane_flag == 0
        && par.m_sps.sample_adaptive_offset_enabled_flag == 0
        && par.m_sps.pcm_enabled_flag == 0);

#if defined(PRE_SI_TARGET_PLATFORM_GEN10)
    MFX_CHECK_COND(par.m_sps.amp_enabled_flag == 1);
#else
    MFX_CHECK_COND(par.m_sps.amp_enabled_flag == 0);
#endif

#if !defined(PRE_SI_TARGET_PLATFORM_GEN11)
    MFX_CHECK_COND(par.m_pps.tiles_enabled_flag == 0);
#endif

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    MFX_CHECK_COND(
      !(   (!caps.YUV444ReconSupport && (par.m_sps.chroma_format_idc == 3))
        || (!caps.YUV422ReconSupport && (par.m_sps.chroma_format_idc == 2))
        || (caps.Color420Only && (par.m_sps.chroma_format_idc != 1))));
#else
    MFX_CHECK_COND(par.m_sps.chroma_format_idc == 1);
#endif

    MFX_CHECK_COND(
      !(   par.m_sps.pic_width_in_luma_samples > caps.MaxPicWidth
        || par.m_sps.pic_height_in_luma_samples > caps.MaxPicHeight
        || (UINT)(((par.m_pps.num_tile_columns_minus1 + 1) * (par.m_pps.num_tile_rows_minus1 + 1)) > 1) > caps.TileSupport));

    MFX_CHECK_COND(
      !(   (caps.MaxEncodedBitDepth == 0 || caps.BitDepth8Only)
        && (par.m_sps.bit_depth_luma_minus8 != 0 || par.m_sps.bit_depth_chroma_minus8 != 0)));

    MFX_CHECK_COND(
      !(   (caps.MaxEncodedBitDepth == 1 || !caps.BitDepth8Only)
        && ( !(par.m_sps.bit_depth_luma_minus8 == 0
            || par.m_sps.bit_depth_luma_minus8 == 2)
          || !(par.m_sps.bit_depth_chroma_minus8 == 0
            || par.m_sps.bit_depth_chroma_minus8 == 2))));

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    MFX_CHECK_COND(
      !(   caps.MaxEncodedBitDepth == 2
        && ( !(par.m_sps.bit_depth_luma_minus8 == 0
            || par.m_sps.bit_depth_luma_minus8 == 2
            || par.m_sps.bit_depth_luma_minus8 == 4)
          || !(par.m_sps.bit_depth_chroma_minus8 == 0
            || par.m_sps.bit_depth_chroma_minus8 == 2
            || par.m_sps.bit_depth_chroma_minus8 == 4))));

    MFX_CHECK_COND(
      !(   caps.MaxEncodedBitDepth == 3
        && ( !(par.m_sps.bit_depth_luma_minus8 == 0
            || par.m_sps.bit_depth_luma_minus8 == 2
            || par.m_sps.bit_depth_luma_minus8 == 4
            || par.m_sps.bit_depth_luma_minus8 == 8)
          || !(par.m_sps.bit_depth_chroma_minus8 == 0
            || par.m_sps.bit_depth_chroma_minus8 == 2
            || par.m_sps.bit_depth_chroma_minus8 == 4
            || par.m_sps.bit_depth_chroma_minus8 == 8))));
#endif

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
    m_buf.resize(6 + par.mfx.NumSlice);
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
        m_packer.GetPrefixSEI(task, m_cur->pData, m_cur->DataLength);
        break;
    case SUFFIX_SEI_NUT:
        m_packer.GetSuffixSEI(task, m_cur->pData, m_cur->DataLength);
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
#if MFX_EXTBUFF_CU_QP_ENABLE
mfxStatus FillCUQPDataDDI(Task& task, MfxVideoParam &par, MFXCoreInterface& core, mfxFrameInfo &CUQPFrameInfo )
{
    
     mfxCoreParam coreParams = {};

    if (core.GetCoreParam(&coreParams))
       return  MFX_ERR_UNSUPPORTED;

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP || !IsOn(par.m_ext.CO3.EnableMBQP) || ((coreParams.Impl & 0xF00) == MFX_HW_VAAPI))
        return MFX_ERR_NONE;

    mfxU32 minWidthQPData = (par.m_ext.HEVCParam.PicWidthInLumaSamples   + par.LCUSize  - 1) / par.LCUSize;
    mfxU32 minHeightQPData = (par.m_ext.HEVCParam.PicHeightInLumaSamples  + par.LCUSize  - 1) / par.LCUSize;
    mfxU32 minQPSize = minWidthQPData*minHeightQPData;
    mfxU32 driverQPsize = CUQPFrameInfo.Width * CUQPFrameInfo.Height;

    MFX_CHECK(driverQPsize >= minQPSize && minQPSize > 0, MFX_ERR_UNDEFINED_BEHAVIOR);
    mfxU32 k_dr_w   =  1;
    mfxU32 k_dr_h   =  1;
    mfxU32 k_input = 1;
    
    if (driverQPsize > minQPSize)
    {
        k_dr_w = CUQPFrameInfo.Width/minWidthQPData;
        k_dr_h = minHeightQPData/minHeightQPData;
        MFX_CHECK(minWidthQPData*k_dr_w == CUQPFrameInfo.Width && minHeightQPData*k_dr_h == CUQPFrameInfo.Height, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(k_dr_w == 1 ||k_dr_w == 2 || k_dr_w == 4 || k_dr_w == 8 , MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(k_dr_h == 1 ||k_dr_h == 2 || k_dr_h == 4 || k_dr_h == 8 , MFX_ERR_UNDEFINED_BEHAVIOR);
    }


    mfxExtMBQP *mbqp = ExtBuffer::Get(task.m_ctrl);
    if (mbqp)
    {
        mfxU32 inputQPsize = mbqp->NumQPAlloc;
        MFX_CHECK (mbqp->NumQPAlloc >= minQPSize, MFX_ERR_UNDEFINED_BEHAVIOR); 
        
        if (inputQPsize > minQPSize)
        {
            k_input = inputQPsize /minQPSize/2;
            MFX_CHECK(minQPSize*k_input*2 == mbqp->NumQPAlloc, MFX_ERR_UNDEFINED_BEHAVIOR);
            MFX_CHECK(k_input == 2 || k_input == 4 || k_input == 8 , MFX_ERR_UNDEFINED_BEHAVIOR);
        }            
    }
    
    {
        FrameLocker lock(&core, task.m_midCUQp);
        MFX_CHECK(lock.Y, MFX_ERR_LOCK_MEMORY);

        if (mbqp)
             for (mfxU32 i = 0; i < CUQPFrameInfo.Height; i++)
                 for (mfxU32 j = 0; j < CUQPFrameInfo.Width; j++)
                    lock.Y[i * lock.Pitch +j] = mbqp->QP[i*k_input/k_dr_h* CUQPFrameInfo.Width + j*k_input/k_dr_w];
        else
            for (mfxU32 i = 0; i < CUQPFrameInfo.Height; i++)
                ippsSet_8u((mfxU8)task.m_qpY, (Ipp8u *)&lock.Y[i * lock.Pitch], (int)CUQPFrameInfo.Width);
    }
    return MFX_ERR_NONE;
    

}
#endif

}; // namespace MfxHwH265Encode
