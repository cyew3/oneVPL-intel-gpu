/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2009 Intel Corporation. All Rights Reserved.

File Name: libmfxsw_bsd.cpp

\* ****************************************************************************** */
#include "libmfxsw_core.h"
#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_BSD)
#include "mfx_mpeg2_bsd.h"
#endif

#if defined (MFX_ENABLE_VC1_VIDEO_BSD)
#include "mfx_vc1_bsd.h"
#endif

#if defined (MFX_ENABLE_H264_VIDEO_BSD)
#include "mfx_h264_bsd.h"
#endif

struct SWBSD:private DispatchSession {
    SWCORE *core;
    VideoBSD *bsd;
    mfxU32  CodecId;
};

mfxStatus MFXVideoBSD_Create(mfxHDL core, mfxHDL *bsd) {
    if (!core) return MFX_ERR_INVALID_HANDLE;
    if (!bsd) return MFX_ERR_NULL_PTR;

    try {
        SWBSD *pBSD=new SWBSD;
        if (!pBSD) return MFX_ERR_MEMORY_ALLOC;
        pBSD->core=(SWCORE *)core;
        pBSD->bsd=0;
        pBSD->CodecId=0;
        *bsd=(mfxHDL)pBSD;
        return MFX_ERR_NONE;
    } catch (...) {
        return MFX_ERR_UNKNOWN;
    }
}

mfxStatus MFXVideoBSD_Destroy(mfxHDL bsd) {
    try {
        if (!bsd) return MFX_ERR_INVALID_HANDLE;
        MFXVideoBSD_Close(bsd);
        delete (SWBSD *)bsd;
        return MFX_ERR_NONE;
    } catch (...) {
        return MFX_ERR_UNKNOWN;
    }
}

static mfxStatus CreateCodecSpecificClass(mfxU32 CodecId, VideoCORE *core, VideoBSD **pBSD) {
    mfxStatus sts=MFX_ERR_UNSUPPORTED;
    switch (CodecId) {
#if defined (MFX_ENABLE_VC1_VIDEO_BSD)
    case MFX_CODEC_VC1:
        *pBSD=(VideoBSD *)new MFXVideoBSDVC1(core, &sts);
        break;
#endif
#if defined (MFX_ENABLE_MPEG2_VIDEO_BSD)
    case MFX_CODEC_MPEG2:
        *pBSD=(VideoBSD *)new VideoBSDMPEG2(core, &sts);
        break;
#endif
#if defined (MFX_ENABLE_H264_VIDEO_BSD)
    case MFX_CODEC_AVC:
        *pBSD=(VideoBSD *)new VideoBSDH264(core, &sts);
        break;
#endif
    default:
        return sts;
    }
    if (!(*pBSD)) return MFX_ERR_MEMORY_ALLOC;
    if (sts>MFX_ERR_NONE) {
        delete *pBSD; *pBSD=0;
    }
    return sts;
}

mfxStatus MFXVideoBSD_Query(mfxHDL bsd, mfxVideoParam *in, mfxVideoParam *out) {
    try {
        SWBSD *pBSD=(SWBSD *)bsd;
        if (!pBSD) return MFX_ERR_INVALID_HANDLE;
        if (!out) return MFX_ERR_NULL_PTR;

        VideoBSD *tbsd=pBSD->bsd;
        if (tbsd) return tbsd->Query(in,out);
        mfxStatus sts=CreateCodecSpecificClass(out->mfx.CodecId,pBSD->core->core,&tbsd);
        if (sts>MFX_ERR_NONE) return sts;
        sts=tbsd->Query(in,out);
        delete tbsd;
        return sts;
    } catch (...) {
        return MFX_ERR_UNKNOWN;
    }
}

mfxStatus MFXVideoBSD_Init(mfxHDL bsd, mfxVideoParam *par) {
    try {
        if (!par) return MFX_ERR_NULL_PTR;
        if (par->Version.Major) return MFX_ERR_UNSUPPORTED;

        SWBSD *pBSD=(SWBSD *)bsd;
        if (!pBSD) return MFX_ERR_INVALID_HANDLE;

        if (pBSD->bsd && pBSD->CodecId!=par->mfx.CodecId) {
            delete pBSD->bsd;
            pBSD->bsd=0;
            pBSD->CodecId=0;
        }

        if (!pBSD->bsd) {
            mfxStatus sts=CreateCodecSpecificClass(par->mfx.CodecId,pBSD->core->core,&pBSD->bsd);
            if (sts>MFX_ERR_NONE) return sts;
            pBSD->CodecId=par->mfx.CodecId;
        }
        return pBSD->bsd->Init(par);
    } catch (...) {
        return MFX_ERR_UNKNOWN;
    }
}

mfxStatus MFXVideoBSD_Close(mfxHDL bsd) {
    try {
        SWBSD *pBSD=(SWBSD *)bsd;
        if (!pBSD) return MFX_ERR_INVALID_HANDLE;
        if (!pBSD->bsd) return MFX_ERR_NONE;
        mfxStatus sts=pBSD->bsd->Close();
        delete pBSD->bsd;
        pBSD->bsd=0;
        return sts;
    } catch (...) {
        return MFX_ERR_UNKNOWN;
    }
}

#undef FUNCTION
#define FUNCTION(RETVAL,FUNC1,PROTO,FUNC2,ARGS) \
    RETVAL FUNC1 PROTO {    \
        try {   \
            SWBSD *pBSD=(SWBSD *)bsd;   \
            if (!pBSD) return MFX_ERR_INVALID_HANDLE;     \
            if (!pBSD->bsd) return MFX_ERR_NOT_INITIALIZED;    \
            return pBSD->bsd-> FUNC2 ARGS;  \
        } catch (...) {     \
            return MFX_ERR_UNKNOWN;     \
        }   \
    }

FUNCTION(mfxStatus,MFXVideoBSD_Reset,(mfxHDL bsd, mfxVideoParam *par),Reset,(par))

FUNCTION(mfxStatus,MFXVideoBSD_GetVideoParam,(mfxHDL bsd, mfxVideoParam *par),GetVideoParam,(par))
FUNCTION(mfxStatus,MFXVideoBSD_GetFrameParam,(mfxHDL bsd, mfxFrameParam *par),GetFrameParam,(par))
FUNCTION(mfxStatus,MFXVideoBSD_GetSliceParam,(mfxHDL bsd, mfxSliceParam *par),GetSliceParam,(par))

FUNCTION(mfxStatus,MFXVideoBSD_RunVideoParam,(mfxHDL bsd, mfxBitstream *bs, mfxVideoParam *par),RunVideoParam,(bs,par))
FUNCTION(mfxStatus,MFXVideoBSD_RunFrameParam,(mfxHDL bsd, mfxBitstream *bs, mfxFrameParam *par),RunFrameParam,(bs,par))
FUNCTION(mfxStatus,MFXVideoBSD_RunSliceParam,(mfxHDL bsd, mfxBitstream *bs, mfxSliceParam *par),RunSliceParam,(bs,par))

FUNCTION(mfxStatus,MFXVideoBSD_RunSliceBSD,(mfxHDL bsd, mfxFrameCUC *cuc),RunSliceBSD,(cuc))
FUNCTION(mfxStatus,MFXVideoBSD_RunSliceMFX,(mfxHDL bsd, mfxFrameCUC *cuc),RunSliceMFX,(cuc))
FUNCTION(mfxStatus,MFXVideoBSD_RunFrameBSD,(mfxHDL bsd, mfxFrameCUC *cuc),RunFrameBSD,(cuc))
FUNCTION(mfxStatus,MFXVideoBSD_RunFrameMFX,(mfxHDL bsd, mfxFrameCUC *cuc),RunFrameMFX,(cuc))

FUNCTION(mfxStatus,MFXVideoBSD_ExtractUserData,(mfxHDL bsd,mfxBitstream *bs,mfxU8 *ud,mfxU32 *sz,mfxU64 *ts),ExtractUserData,(bs,ud,sz,ts))
