/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE_HW) && defined(MFX_VA)

#include "mfx_session.h"
#include "mfx_task.h"
#include "mfx_ext_buffers.h"

#include "mfx_vp8_encode_hw.h"
#include "mfx_vp8_enc_common_hw.h"
#include "mfx_vp8_encode_hybrid_hw.h"

#define DDI_HYBRID_PAK_IMPL
//#define DDI_ENCODE_IMPL
//#define C_MODEL_IMPL

VideoENCODE * CreateMFXHWVideoENCODEVP8(
    VideoCORE * core,
    mfxStatus * res)
{
    return new MFXHWVideoENCODEVP8(core, res);
}


mfxStatus MFXHWVideoENCODEVP8::Init(mfxVideoParam * par)
{
    mfxStatus                   sts = MFX_ERR_NONE;
    std::auto_ptr<VideoENCODE>  impl;

    MFX_CHECK(m_impl.get() == 0, MFX_ERR_UNDEFINED_BEHAVIOR); 

#ifdef C_MODEL_IMPL
    impl.reset((VideoENCODE *) new  MFX_VP8ENC::CmodelImpl(m_core));
#else
    ENCODE_CAPS_VP8             caps = {0};
    sts = MFX_VP8ENC::QueryHwCaps(m_core, caps);
    MFX_CHECK(sts == MFX_ERR_NONE, MFX_WRN_PARTIAL_ACCELERATION);
#ifdef DDI_ENCODE_IMPL
    if (impl.get() == 0 && caps.EncodeFunc)
    {
        impl.reset((VideoENCODE *) new  MFX_VP8ENC::EncodeDDIImpl(m_core));    
    }
#endif
#ifdef DDI_HYBRID_PAK_IMPL
#if !defined (VP8_HYBRID_DUMP_READ)
    if (impl.get() == 0 && caps.HybridPakFunc)
#endif
    {
        impl.reset((VideoENCODE *) new  MFX_VP8ENC::HybridPakDDIImpl(m_core));
    }
#endif // DDI_HYBRID_PAK_IMPL
#endif // !C_MODEL_IMPL
    MFX_CHECK(impl.get() != 0, MFX_WRN_PARTIAL_ACCELERATION); 

    sts = impl->Init(par);
    MFX_CHECK(
        sts >= MFX_ERR_NONE &&
        sts != MFX_WRN_PARTIAL_ACCELERATION, sts);

    m_impl = impl;
    return sts;
}

mfxStatus MFXHWVideoENCODEVP8::Query(
    VideoCORE *     core,
    mfxVideoParam * in,
    mfxVideoParam * out)
{ 
    MFX_CHECK_NULL_PTR2(core, out);

    ENCODE_CAPS_VP8             caps = {0};
    MFX_CHECK(MFX_ERR_NONE == MFX_VP8ENC::QueryHwCaps(core, caps), MFX_WRN_PARTIAL_ACCELERATION);
    
    return   (in == 0) ? MFX_VP8ENC::SetSupportedParameters(out):
        MFX_VP8ENC::CheckParameters(in,out);
}


mfxStatus MFXHWVideoENCODEVP8::QueryIOSurf(
    VideoCORE *            core,
    mfxVideoParam *        par,
    mfxFrameAllocRequest * request)
{
    MFX_CHECK_NULL_PTR3(core,par,request);

    MFX_CHECK(MFX_VP8ENC::CheckPattern(par->IOPattern), MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(MFX_VP8ENC::CheckFrameSize(par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height),MFX_ERR_INVALID_VIDEO_PARAM);

    request->Type = mfxU16((par->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)? MFX_VP8ENC::MFX_MEMTYPE_SYS_EXT:MFX_VP8ENC::MFX_MEMTYPE_D3D_EXT) ;

    request->NumFrameMin =  (par->AsyncDepth ? par->AsyncDepth: MFX_VP8ENC::GetDefaultAsyncDepth())  + 1;
    request->NumFrameSuggested = request->NumFrameMin;

    request->Info = par->mfx.FrameInfo;
    return MFX_ERR_NONE;
}

#endif 

