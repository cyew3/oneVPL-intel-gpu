// Copyright (c) 2008-2019 Intel Corporation
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

#include <mfxvideo.h>

#include <assert.h>
#include <functional>

#include <mfx_session.h>
#include <mfx_tools.h>
#include <mfx_common.h>

#include "mfx_reflect.h"

// sheduling and threading stuff
#include <mfx_task.h>

#include "mfxvideo++int.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE)
#if defined(MFX_VA)
#include "mfx_h264_encode_hw.h"
#endif
#ifndef OPEN_SOURCE
#include "mfx_h264_enc_common.h"
#include "mfx_h264_encode.h"
#endif
#if defined (MFX_ENABLE_MVC_VIDEO_ENCODE)
#include "mfx_mvc_encode.h"
#endif
#if defined(MFX_ENABLE_H264_FEI_ENCPAK)
#include "mfxfei.h"
#endif
#endif

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)
#if defined(MFX_VA)
#include "mfx_mpeg2_encode_hw.h"
#ifndef OPEN_SOURCE
#include "mfx_mpeg2_encode.h"
#endif
#else
#include "mfx_mpeg2_encode.h"
#endif
#endif

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE)
#if defined(MFX_VA)
#include "mfx_mjpeg_encode_hw.h"
#if defined(MFX_ENABLE_SW_FALLBACK)
#include "mfx_mjpeg_encode.h"
#endif
#else
#include "mfx_mjpeg_encode.h"
#endif
#endif

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
#if defined(OPEN_SOURCE) || !defined(AS_HEVCE_PLUGIN)
#if defined(MFX_VA)
#if defined(MFX_ENABLE_HEVCEHW_REFACTORING)
#include "../../encode_hw/hevc/hevcehw_disp.h"
#else
#include "mfx_h265_encode_hw.h"
#endif
#endif
#else
#include "mfx_h265_encode_api.h"
#endif
#endif

#if defined (MFX_ENABLE_VP9_VIDEO_ENCODE_HW) && !defined (AS_VP9E_PLUGIN) && defined(MFX_VA)
#include "mfx_vp9_encode_hw.h"
#endif

#if defined (MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE)
#if defined(MFX_VA)
#include "mfx_h265_fei_encode_hw.h"
#endif
#endif

#include <libmfx_core_interface.h> // for MFXIFEIEnabled_GUID

#if defined (MFX_RT)
#pragma warning(disable:4065)
#endif


struct CodecKey {
    const mfxU32 codecId;
    const bool   fei;

    CodecKey(mfxU32 codecId, bool fei) : codecId(codecId), fei(fei) {}

    enum {
        // special value for codecId to denote plugin, it must be
        // different from other MFX_CODEC_* used in codecId2Handlers
        MFX_CODEC_DUMMY_FOR_PLUGIN = 0
    };

    // Exact ordering rule is unsignificant as far as it provides strict weak ordering.
    // Compare for fei after codecId because fei values are mostly same.
    friend bool operator<(CodecKey l, CodecKey r)
    {
        if (l.codecId == r.codecId)
            return l.fei < r.fei;
        return l.codecId < r.codecId;
    }
};

struct Handlers {
    typedef std::function<VideoENCODE*(VideoCORE* core, mfxU16 codecProfile, mfxStatus *mfxRes)> CtorType;

    struct Funcs {
        CtorType ctor;
        std::function<mfxStatus(mfxSession s, mfxVideoParam *in, mfxVideoParam *out)> query;
        std::function<mfxStatus(mfxSession s, mfxVideoParam *par, mfxFrameAllocRequest *request)> queryIOSurf;
    };

    Funcs primary;
    Funcs fallback;
};

typedef std::map<CodecKey, Handlers> CodecId2Handlers;

static const CodecId2Handlers codecId2Handlers =
{
#ifdef MFX_ENABLE_USER_ENCODE
    {
        {
            CodecKey::MFX_CODEC_DUMMY_FOR_PLUGIN,
            // .fei =
            false
        },
        {
            // .primary =
            {
                // .ctor =
                nullptr,
                // .query =
                [](mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
                {
                    assert(session->m_plgEnc);
                    return session->m_plgEnc->Query(session->m_pCORE.get(), in, out);
                },
                // .queryIOSurf =
                [](mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    assert(session->m_plgEnc);
                    return session->m_plgEnc->QueryIOSurf(session->m_pCORE.get(), par, request, 0);
                }
            },
            // .fallback =
            {
            }
        }
    },
#endif

#ifdef MFX_ENABLE_H264_VIDEO_ENCODE
    {
        {
            MFX_CODEC_AVC,
            // .fei =
            false
        },
        {
#if defined(MFX_VA) && defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW)
            // .primary =
            {
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MFXHWVideoENCODEH264(core, mfxRes);
                },
                // .query =
                [](mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
                {
                    if (!session->m_pENCODE.get())
                        return MFXHWVideoENCODEH264::Query(session->m_pCORE.get(), in, out);
                    else
                        return MFXHWVideoENCODEH264::Query(session->m_pCORE.get(), in, out, session->m_pENCODE.get());
                },
                // .queryIOSurf =
                [](mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    return MFXHWVideoENCODEH264::QueryIOSurf(session->m_pCORE.get(), par, request);
                }
            },
            // .fallback =
            {
  #ifndef OPEN_SOURCE
                // .ctor =
                [](VideoCORE* core, mfxU16 codecProfile, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    (void)codecProfile;
#ifdef MFX_ENABLE_MVC_VIDEO_ENCODE
                    if(codecProfile == MFX_PROFILE_AVC_MULTIVIEW_HIGH || codecProfile == MFX_PROFILE_AVC_STEREO_HIGH)
                        return new MFXVideoENCODEMVC(core, mfxRes);
#endif // MFX_ENABLE_MVC_VIDEO_ENCODE
                    return new MFXVideoENCODEH264(core, mfxRes);
                },
                // .query =
                [](mfxSession /*session*/, mfxVideoParam *in, mfxVideoParam *out)
                {
                    mfxStatus mfxRes;
    #ifdef MFX_ENABLE_MVC_VIDEO_ENCODE
                    if(in && (in->mfx.CodecProfile == MFX_PROFILE_AVC_MULTIVIEW_HIGH || in->mfx.CodecProfile == MFX_PROFILE_AVC_STEREO_HIGH))
                        mfxRes = MFXVideoENCODEMVC::Query(in, out);
                    else
    #endif
                        mfxRes = MFXVideoENCODEH264::Query(in, out);
                    return mfxRes;
                },
                // .queryIOSurf =
                [](mfxSession /*session*/, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    mfxStatus mfxRes;
#ifdef MFX_ENABLE_MVC_VIDEO_ENCODE
                    if (par->mfx.CodecProfile == MFX_PROFILE_AVC_MULTIVIEW_HIGH || par->mfx.CodecProfile == MFX_PROFILE_AVC_STEREO_HIGH)
                        mfxRes = MFXVideoENCODEMVC::QueryIOSurf(par, request);
                    else
#endif // MFX_ENABLE_MVC_VIDEO_ENCODE
                        mfxRes = MFXVideoENCODEH264::QueryIOSurf(par, request);
                    return mfxRes;
                }
  #endif // OPEN_SOURCE
            }
#else // MFX_VA
            // .primary =
            {
                // .ctor =
                [](VideoCORE* core, mfxU16 codecProfile, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    (void)codecProfile;
#ifdef MFX_ENABLE_MVC_VIDEO_ENCODE
                    if(codecProfile == MFX_PROFILE_AVC_MULTIVIEW_HIGH || codecProfile == MFX_PROFILE_AVC_STEREO_HIGH)
                        return new MFXVideoENCODEMVC(core, mfxRes);
#endif // MFX_ENABLE_MVC_VIDEO_ENCODE
                    return new MFXVideoENCODEH264(core, mfxRes);
                },
                // .query =
                [](mfxSession /*session*/, mfxVideoParam *in, mfxVideoParam *out)
                {
                    mfxStatus mfxRes;
  #ifdef MFX_ENABLE_MVC_VIDEO_ENCODE
                    if(in && (in->mfx.CodecProfile == MFX_PROFILE_AVC_MULTIVIEW_HIGH || in->mfx.CodecProfile == MFX_PROFILE_AVC_STEREO_HIGH))
                        mfxRes = MFXVideoENCODEMVC::Query(in, out);
                    else
  #endif
                        mfxRes = MFXVideoENCODEH264::Query(in, out);
                    return mfxRes;
                },
                // .queryIOSurf =
                [](mfxSession /*session*/, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    mfxStatus mfxRes;
#ifdef MFX_ENABLE_MVC_VIDEO_ENCODE
                    if (par->mfx.CodecProfile == MFX_PROFILE_AVC_MULTIVIEW_HIGH || par->mfx.CodecProfile == MFX_PROFILE_AVC_STEREO_HIGH)
                        mfxRes = MFXVideoENCODEMVC::QueryIOSurf(par, request);
                    else
#endif // MFX_ENABLE_MVC_VIDEO_ENCODE
                        mfxRes = MFXVideoENCODEH264::QueryIOSurf(par, request);
                    return mfxRes;
                }
            }
#endif // MFX_VA
        }
    },
#endif // MFX_ENABLE_H264_VIDEO_ENCODE

#ifdef MFX_ENABLE_MPEG2_VIDEO_ENCODE
    {
        {
            MFX_CODEC_MPEG2,
            // .fei =
            false
        },
        {
#if defined(MFX_VA)
            // .primary =
            {
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MFXVideoENCODEMPEG2_HW(core, mfxRes);
                },
                // .query =
                [](mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
                {
                    return MFXVideoENCODEMPEG2_HW::Query(session->m_pCORE.get(), in, out);
                },
                // .queryIOSurf =
                [](mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    return MFXVideoENCODEMPEG2_HW::QueryIOSurf(session->m_pCORE.get(), par, request);
                }
            },
            // .fallback =
            {
  #ifndef OPEN_SOURCE
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MFXVideoENCODEMPEG2(core, mfxRes);
                },
                // .query =
                [](mfxSession /*session*/, mfxVideoParam *in, mfxVideoParam *out)
                {
                    return MFXVideoENCODEMPEG2::Query(in, out);
                },
                // .queryIOSurf =
                [](mfxSession /*session*/, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    return MFXVideoENCODEMPEG2::QueryIOSurf(par, request);
                }
  #endif // OPEN_SOURCE
            }
#else // MFX_VA
            // .primary =
            {
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MFXVideoENCODEMPEG2(core, mfxRes);
                },
                // .query =
                [](mfxSession /*session*/, mfxVideoParam *in, mfxVideoParam *out)
                {
                    return MFXVideoENCODEMPEG2::Query(in, out);
                },
                // .queryIOSurf =
                [](mfxSession /*session*/, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    return MFXVideoENCODEMPEG2::QueryIOSurf(par, request);
                }
            }
#endif // MFX_VA
        }
    },
#endif // MFX_ENABLE_MPEG2_VIDEO_ENCODE

#if defined(MFX_ENABLE_MJPEG_VIDEO_ENCODE)
    {
        {
            MFX_CODEC_JPEG,
            // .fei =
            false
        },
        {
#if defined(MFX_VA)
            // .primary =
            {
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MFXVideoENCODEMJPEG_HW(core, mfxRes);
                },
                // .query =
                [](mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
                {
                    return MFXVideoENCODEMJPEG_HW::Query(session->m_pCORE.get(), in, out);
                },
                // .queryIOSurf =
                [](mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    return MFXVideoENCODEMJPEG_HW::QueryIOSurf(session->m_pCORE.get(), par, request);
                }
            },
            // .fallback =
            {
  #if defined(MFX_ENABLE_SW_FALLBACK)
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MFXVideoENCODEMJPEG(core, mfxRes);
                },
                // .query =
                [](mfxSession /*session*/, mfxVideoParam *in, mfxVideoParam *out)
                {
                    return MFXVideoENCODEMJPEG::Query(in, out);
                },
                // .queryIOSurf =
                [](mfxSession /*session*/, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    return MFXVideoENCODEMJPEG::QueryIOSurf(par, request);
                }
  #endif
            }
#else // MFX_VA
            // .primary =
            {
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MFXVideoENCODEMJPEG(core, mfxRes);
                },
                // .query =
                [](mfxSession /*session*/, mfxVideoParam *in, mfxVideoParam *out)
                {
                    return MFXVideoENCODEMJPEG::Query(in, out);
                },
                // .queryIOSurf =
                [](mfxSession /*session*/, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    return MFXVideoENCODEMJPEG::QueryIOSurf(par, request);
                }
            }
#endif // MFX_VA
        }
    },
#endif // MFX_ENABLE_MJPEG_VIDEO_ENCODE

#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_VA) && (defined(OPEN_SOURCE) || !defined(AS_HEVCE_PLUGIN))
  #if defined (MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE)
    {
        {
            MFX_CODEC_HEVC,
            // .fei =
            true
        },
        {
            // .primary =
            {
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MfxHwH265FeiEncode::H265FeiEncode_HW(core, mfxRes);
                },
                // .query =
                [](mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
                { return MfxHwH265FeiEncode::H265FeiEncode_HW::Query(session->m_pCORE.get(), in, out); },
                // .queryIOSurf =
                [](mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
                { return MfxHwH265FeiEncode::H265FeiEncode_HW::QueryIOSurf(session->m_pCORE.get(), par, request); }
            }
        }
    },
  #endif
    {
        {
            MFX_CODEC_HEVC,
            // .fei =
            false
        },
        {
            // .primary =
            {
#if defined(MFX_ENABLE_HEVCEHW_REFACTORING)
                
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    if (core && mfxRes)
                        return HEVCEHW::Create(*core, *mfxRes);
                    return nullptr;
                },
                // .query =
                [](mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
                { return HEVCEHW::Query(session->m_pCORE.get(), in, out); },
                // .queryIOSurf =
                [](mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
                { return HEVCEHW::QueryIOSurf(session->m_pCORE.get(), par, request); }
#else
                // .ctor =
                [](VideoCORE* core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MfxHwH265Encode::MFXVideoENCODEH265_HW(core, mfxRes);
                },
                // .query =
                [](mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
                { return MfxHwH265Encode::MFXVideoENCODEH265_HW::Query(session->m_pCORE.get(), in, out); },
                // .queryIOSurf =
                [](mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
                { return MfxHwH265Encode::MFXVideoENCODEH265_HW::QueryIOSurf(session->m_pCORE.get(), par, request); }
#endif
            },
            // .fallback =
            {
            }
        }
    },
#endif // MFX_ENABLE_H265_VIDEO_ENCODE

#if defined(MFX_ENABLE_VP9_VIDEO_ENCODE_HW)
    {
        {
            MFX_CODEC_VP9,
            // .fei =
            false
        },
        {
            // .primary =
            {
                // .ctor =
                [](VideoCORE *core, mfxU16 /*codecProfile*/, mfxStatus *mfxRes)
                -> VideoENCODE*
                {
                    return new MfxHwVP9Encode::MFXVideoENCODEVP9_HW(core, mfxRes);
                },
                // .query =
                [](mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
                {
                    return MfxHwVP9Encode::MFXVideoENCODEVP9_HW::Query(session->m_pCORE.get(), in, out);
                },
                // .queryIOSurf =
                [](mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
                {
                    return MfxHwVP9Encode::MFXVideoENCODEVP9_HW::QueryIOSurf(session->m_pCORE.get(), par, request);
                }
            },
            // .fallback =
            {
            }
        }
    }
#endif // MFX_ENABLE_VP9_VIDEO_ENCODE_HW
}; // codecId2Handlers

// first - is QueryCoreInterface() returns non-null ptr, second - fei status
std::pair<bool, bool> check_fei(VideoCORE* core)
{
    bool *feiEnabled = (bool*)core->QueryCoreInterface(MFXIFEIEnabled_GUID);
    if (!feiEnabled)
    {
        return std::pair<bool,bool>(false,false);
    }
    return std::pair<bool,bool>(true, *feiEnabled);
}

#if !defined (MFX_RT)
template<>
VideoENCODE* _mfxSession::Create<VideoENCODE>(mfxVideoParam& par)
{
    VideoCORE* core = m_pCORE.get();
    mfxU32 CodecId = par.mfx.CodecId;

    bool feiStatusAvailable, fei;
    std::tie(feiStatusAvailable, fei) = check_fei(core);
    if (!feiStatusAvailable)
    {
        return nullptr;
    }

    // create a codec instance
    auto handler = codecId2Handlers.find(CodecKey(CodecId, fei));
    if (handler == codecId2Handlers.end())
    {
        return nullptr;
    }

    const Handlers::CtorType &ctor = m_bIsHWENCSupport?
        handler->second.primary.ctor : handler->second.fallback.ctor;
    if (!ctor)
    {
        return nullptr;
    }

    mfxStatus mfxRes = MFX_ERR_MEMORY_ALLOC;
    std::unique_ptr<VideoENCODE> pENCODE(ctor(core, par.mfx.CodecProfile, &mfxRes));
    // check error(s)
    if (MFX_ERR_NONE != mfxRes)
    {
        return nullptr;
    }

    return pENCODE.release();
} // VideoENCODE *CreateENCODESpecificClass(mfxU32 CodecId, VideoCORE *core)
#endif

mfxStatus MFXVideoENCODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(out, MFX_ERR_NULL_PTR);

#if !defined(ANDROID)
    if ((0 != in) && (MFX_HW_VAAPI == session->m_pCORE->GetVAType()))
    {
        // protected content not supported on Linux
        if(0 != in->Protected)
        {
            out->Protected = 0;
            return MFX_ERR_UNSUPPORTED;
        }
    }
#endif

    mfxStatus mfxRes;
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, in);

    bool bIsHWENCSupport = false;

    try
    {
        CodecId2Handlers::const_iterator handler;

        if (session->m_plgEnc.get())
        {
            handler = codecId2Handlers.find(CodecKey(CodecKey::MFX_CODEC_DUMMY_FOR_PLUGIN, /*fei=*/false));
            assert(handler != codecId2Handlers.end());
        }
        // if plugin is not supported, or wrong parameters passed we should not look into library
        else
        {
            // required to check FEI plugin registration
            bool feiStatusAvailable, fei;
            std::tie(feiStatusAvailable, fei) = check_fei(session->m_pCORE.get());
            MFX_CHECK(feiStatusAvailable, MFX_ERR_NULL_PTR);

            handler = codecId2Handlers.find(CodecKey(out->mfx.CodecId, fei));
        }

        mfxRes = handler == codecId2Handlers.end() ? MFX_ERR_UNSUPPORTED
            : (handler->second.primary.query)(session, in, out);

        if (MFX_WRN_PARTIAL_ACCELERATION == mfxRes)
        {
            assert(handler != codecId2Handlers.end());

#ifndef OPEN_SOURCE
            // for plugin, do not touch MFX_WRN_PARTIAL_ACCELERATION
            if (!session->m_plgEnc.get())
#endif
            {
                mfxRes = !handler->second.fallback.query ? MFX_ERR_UNSUPPORTED
                        : (handler->second.fallback.query)(session, in, out);
            }
        }
        else
        {
            bIsHWENCSupport = true;
        }
    }
    // handle error(s)
    catch(...)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }

    if (MFX_PLATFORM_HARDWARE == session->m_currentPlatform &&
        !bIsHWENCSupport &&
        MFX_ERR_NONE <= mfxRes)
    {
#if defined(MFX_ENABLE_SW_FALLBACK)
        mfxRes = MFX_WRN_PARTIAL_ACCELERATION;
#else
        mfxRes = MFX_ERR_UNSUPPORTED;
#endif
    }

#if (MFX_VERSION >= 1025)
#if !defined(AS_HEVCE_PLUGIN)
    if (mfxRes == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM || mfxRes == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
    {
        try
        {
            mfx_reflect::AccessibleTypesCollection g_Reflection = GetReflection();
            if (g_Reflection.m_bIsInitialized)
            {
                std::string result = mfx_reflect::CompareStructsToString(g_Reflection.Access(in), g_Reflection.Access(out));
                MFX_LTRACE_MSG(MFX_TRACE_LEVEL_INTERNAL, result.c_str())
            }
        }
        catch (const std::exception& e)
        {
            MFX_LTRACE_MSG(MFX_TRACE_LEVEL_INTERNAL, e.what());
        }
        catch (...)
        {
            MFX_LTRACE_MSG(MFX_TRACE_LEVEL_INTERNAL, "Unknown exception was caught while comparing In and Out VideoParams.");
        }
    }
#endif
#endif

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, out);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoENCODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    MFX_CHECK(request, MFX_ERR_NULL_PTR);

    mfxStatus mfxRes;
    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, par);

    bool bIsHWENCSupport = false;

    try
    {
        CodecId2Handlers::const_iterator handler;

        if (session->m_plgEnc.get())
        {
            handler = codecId2Handlers.find(CodecKey(CodecKey::MFX_CODEC_DUMMY_FOR_PLUGIN, /*fei=*/false));
            assert(handler != codecId2Handlers.end());
        }
        // if plugin is not supported, or wrong parameters passed we should not look into library
        else
        {
            // required to check FEI plugin registration
            bool feiStatusAvailable, fei;
            std::tie(feiStatusAvailable, fei) = check_fei(session->m_pCORE.get());
            MFX_CHECK(feiStatusAvailable, MFX_ERR_NULL_PTR);

            handler = codecId2Handlers.find(CodecKey(par->mfx.CodecId, fei));
        }

        mfxRes = handler == codecId2Handlers.end() ? MFX_ERR_INVALID_VIDEO_PARAM
            : (handler->second.primary.queryIOSurf)(session, par, request);

        if (MFX_WRN_PARTIAL_ACCELERATION == mfxRes)
        {
            assert(handler != codecId2Handlers.end());

#ifndef OPEN_SOURCE
            // for plugin, do not touch MFX_WRN_PARTIAL_ACCELERATION
            if (!session->m_plgEnc.get())
#endif
            {
                mfxRes = !handler->second.fallback.query ? MFX_ERR_INVALID_VIDEO_PARAM
                    : (handler->second.fallback.queryIOSurf)(session, par, request);
            }
        }
        else
        {
            bIsHWENCSupport = true;
        }
    }
    // handle error(s)
    catch(...)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    if (MFX_PLATFORM_HARDWARE == session->m_currentPlatform &&
        !bIsHWENCSupport &&
        MFX_ERR_NONE <= mfxRes)
    {
#if defined(MFX_ENABLE_SW_FALLBACK)
        mfxRes = MFX_WRN_PARTIAL_ACCELERATION;
#else
        mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
#endif
    }

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, request);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoENCODE_Init(mfxSession session, mfxVideoParam *par)
{
    mfxStatus mfxRes;

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, par);

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);

    try
    {
#if !defined (MFX_RT)
        // check existence of component
        if (!session->m_pENCODE)
        {
            // create a new instance
            session->m_bIsHWENCSupport = true;
            session->m_pENCODE.reset(session->Create<VideoENCODE>(*par));
            MFX_CHECK(session->m_pENCODE.get(), MFX_ERR_INVALID_VIDEO_PARAM);
        }
#endif

        mfxRes = session->m_pENCODE->Init(par);

        if (MFX_WRN_PARTIAL_ACCELERATION == mfxRes)
        {
            session->m_bIsHWENCSupport = false;
#if defined(MFX_ENABLE_SW_FALLBACK)
#if !defined (MFX_RT)
            session->m_pENCODE.reset(session->Create<VideoENCODE>(*par));
            MFX_CHECK(session->m_pENCODE.get(), MFX_ERR_INVALID_VIDEO_PARAM);
            mfxRes = session->m_pENCODE->Init(par);
#endif
#else
            mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
#endif
        }
        else if (mfxRes >= MFX_ERR_NONE)
            session->m_bIsHWENCSupport = true;

        if (MFX_PLATFORM_HARDWARE == session->m_currentPlatform &&
            !session->m_bIsHWENCSupport &&
            MFX_ERR_NONE <= mfxRes)
        {
#if defined(MFX_ENABLE_SW_FALLBACK)
            mfxRes = MFX_WRN_PARTIAL_ACCELERATION;
#else
            mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
#endif
        }
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoENCODE_Close(mfxSession session)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);

    MFX_CHECK(session,               MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(session->m_pENCODE,    MFX_ERR_NOT_INITIALIZED);

    try
    {
        if (!session->m_pENCODE)
        {
            return MFX_ERR_NOT_INITIALIZED;
        }

        // wait until all tasks are processed
        session->m_pScheduler->WaitForTaskCompletion(session->m_pENCODE.get());

        mfxRes = session->m_pENCODE->Close();
        // delete the codec's instance if not plugin
        if (!session->m_plgEnc)
        {
            session->m_pENCODE.reset(nullptr);
        }
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

static
mfxStatus MFXVideoENCODELegacyRoutine(void *pState, void *pParam,
                                      mfxU32 threadNumber, mfxU32 callNumber)
{
    (void)callNumber;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "EncodeFrame");
    VideoENCODE *pENCODE = (VideoENCODE *) pState;
    MFX_THREAD_TASK_PARAMETERS *pTaskParam = (MFX_THREAD_TASK_PARAMETERS *) pParam;
    mfxStatus mfxRes;

    // check error(s)
    if ((NULL == pState) ||
        (NULL == pParam) ||
        (0 != threadNumber))
    {
        return MFX_ERR_NULL_PTR;
    }

    // call the obsolete method
    mfxRes = pENCODE->EncodeFrame(pTaskParam->encode.ctrl,
                                  &pTaskParam->encode.internal_params,
                                  pTaskParam->encode.surface,
                                  pTaskParam->encode.bs);

    return mfxRes;

} // mfxStatus MFXVideoENCODELegacyRoutine(void *pState, void *pParam,

enum
{
    MFX_NUM_ENTRY_POINTS = 2
};

mfxStatus MFXVideoENCODE_EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
{
    mfxStatus mfxRes;

    MFX_AUTO_LTRACE_WITHID(MFX_TRACE_LEVEL_API, "MFX_EncodeFrameAsync");
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, ctrl);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, surface);

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pENCODE.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(syncp, MFX_ERR_NULL_PTR);

    try
    {
        mfxSyncPoint syncPoint = NULL;
        mfxFrameSurface1 *reordered_surface = NULL;
        mfxEncodeInternalParams internal_params;
        MFX_ENTRY_POINT entryPoints[MFX_NUM_ENTRY_POINTS];
        mfxU32 numEntryPoints = MFX_NUM_ENTRY_POINTS;

        memset(&entryPoints, 0, sizeof(entryPoints));
        mfxRes = session->m_pENCODE->EncodeFrameCheck(ctrl,
                                                      surface,
                                                      bs,
                                                      &reordered_surface,
                                                      &internal_params,
                                                      entryPoints,
                                                      numEntryPoints);
        // source data is OK, go forward
        if ((MFX_ERR_NONE == mfxRes) ||
            (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfxRes) ||
            (MFX_WRN_OUT_OF_RANGE == mfxRes) ||
            // WHAT IS IT??? IT SHOULD BE REMOVED
            ((mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes) ||
            (MFX_ERR_MORE_BITSTREAM == mfxRes))
        {
            // prepare the obsolete kind of task.
            // it is obsolete and must be removed.
            if (NULL == entryPoints[0].pRoutine)
            {
                MFX_TASK task;

                memset(&task, 0, sizeof(task));
                // BEGIN OF OBSOLETE PART
                task.bObsoleteTask = true;
                task.obsolete_params.encode.internal_params = internal_params;
                // fill task info
                task.pOwner = session->m_pENCODE.get();
                task.entryPoint.pRoutine = &MFXVideoENCODELegacyRoutine;
                task.entryPoint.pState = session->m_pENCODE.get();
                task.entryPoint.requiredNumThreads = 1;

                // fill legacy parameters
                task.obsolete_params.encode.ctrl = ctrl;
                task.obsolete_params.encode.surface = reordered_surface;
                task.obsolete_params.encode.bs = bs;
                // END OF OBSOLETE PART

                task.priority = session->m_priority;
                task.threadingPolicy = session->m_pENCODE->GetThreadingPolicy();
                // fill dependencies
                task.pSrc[0] = surface;
                task.pDst[0] = ((mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes) ? 0: bs;

// specific plug-in case to run additional task after main task
#if defined(OPEN_SOURCE) || !defined(AS_HEVCE_PLUGIN)
                task.pSrc[1] =  bs;
#endif
                task.pSrc[2] = ctrl ? ctrl->ExtParam : 0;

#ifdef MFX_TRACE_ENABLE
                task.nParentId = MFX_AUTO_TRACE_GETID();
                task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_ENCODE;
#endif // MFX_TRACE_ENABLE

                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));
            }
            else if (1 == numEntryPoints)
            {
                MFX_TASK task;

                memset(&task, 0, sizeof(task));
                task.pOwner = session->m_pENCODE.get();
                task.entryPoint = entryPoints[0];
                task.priority = session->m_priority;
                task.threadingPolicy = session->m_pENCODE->GetThreadingPolicy();
                // fill dependencies
                task.pSrc[0] = surface;
                // specific plug-in case to run additional task after main task
#if defined(OPEN_SOURCE) || !defined(AS_HEVCE_PLUGIN)
                task.pSrc[1] =  bs;
#endif
                task.pSrc[2] = ctrl ? ctrl->ExtParam : 0;
                task.pDst[0] = ((mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes) ? 0 : bs;


#ifdef MFX_TRACE_ENABLE
                task.nParentId = MFX_AUTO_TRACE_GETID();
                task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_ENCODE;
#endif
                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));
            }
            else
            {
                MFX_TASK task;

                memset(&task, 0, sizeof(task));
                task.pOwner = session->m_pENCODE.get();
                task.entryPoint = entryPoints[0];
                task.priority = session->m_priority;
                task.threadingPolicy = session->m_pENCODE->GetThreadingPolicy();
                // fill dependencies
                task.pSrc[0] = surface;
                task.pSrc[1] = ctrl ? ctrl->ExtParam : 0;
                task.pDst[0] = entryPoints[0].pParam;

#ifdef MFX_TRACE_ENABLE
                task.nParentId = MFX_AUTO_TRACE_GETID();
                task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_ENCODE;
#endif
                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));

                memset(&task, 0, sizeof(task));
                task.pOwner = session->m_pENCODE.get();
                task.entryPoint = entryPoints[1];
                task.priority = session->m_priority;
                task.threadingPolicy = session->m_pENCODE->GetThreadingPolicy();
                // fill dependencies
                task.pSrc[0] = entryPoints[0].pParam;
                task.pDst[0] = ((mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes) ? 0: bs;

#ifdef MFX_TRACE_ENABLE
                task.nParentId = MFX_AUTO_TRACE_GETID();
                task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_ENCODE2;
#endif
                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));
            }

            // IT SHOULD BE REMOVED
            if ((mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxRes)
            {
                mfxRes = MFX_ERR_MORE_DATA;
                syncPoint = NULL;
            }
        }

        // return pointer to synchronization point
        *syncp = syncPoint;
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, bs);
    if (mfxRes == MFX_ERR_NONE && syncp)
    {
        MFX_LTRACE_P(MFX_TRACE_LEVEL_API, *syncp);
    }
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;

} // mfxStatus MFXVideoENCODE_EncodeFrameAsync(mfxSession session, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)

//
// THE OTHER ENCODE FUNCTIONS HAVE IMPLICIT IMPLEMENTATION
//

FUNCTION_RESET_IMPL(ENCODE, Reset, (mfxSession session, mfxVideoParam *par), (par))
FUNCTION_IMPL(ENCODE, GetVideoParam, (mfxSession session, mfxVideoParam *par), (par))
FUNCTION_IMPL(ENCODE, GetEncodeStat, (mfxSession session, mfxEncodeStat *stat), (stat))
