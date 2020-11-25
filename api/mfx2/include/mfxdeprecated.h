/*############################################################################
  # Copyright (C) 2020 Intel Corporation
  #
  # SPDX-License-Identifier: MIT
  ############################################################################*/

#ifndef __MFXDEPRECATED_H__
#define __MFXDEPRECATED_H__

#include "mfxcommon.h"

#if defined(MFX_ONEVPL)

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

mfxStatus MFX_CDECL MFXVideoVPP_RunFrameVPPAsyncEx(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {
    mfxU32      reserved[4];
    mfxHDL      pthis;
    mfxStatus  (MFX_CDECL *Alloc)    (mfxHDL pthis, mfxU32 nbytes, mfxU16 type, mfxMemId *mid);
    mfxStatus  (MFX_CDECL *Lock)     (mfxHDL pthis, mfxMemId mid, mfxU8 **ptr);
    mfxStatus  (MFX_CDECL *Unlock)   (mfxHDL pthis, mfxMemId mid);
    mfxStatus  (MFX_CDECL *Free)     (mfxHDL pthis, mfxMemId mid);
} mfxBufferAllocator;
MFX_PACK_END()

mfxStatus MFX_CDECL MFXVideoCORE_SetBufferAllocator(mfxSession session, mfxBufferAllocator *allocator);

mfxStatus MFX_CDECL MFXDoWork(mfxSession session);

MFX_PACK_BEGIN_USUAL_STRUCT()
typedef struct {} mfxPlugin;
MFX_PACK_END()

mfxStatus MFX_CDECL MFXVideoUSER_Register(mfxSession session, mfxU32 type, const mfxPlugin *par);
mfxStatus MFX_CDECL MFXVideoUSER_Unregister(mfxSession session, mfxU32 type);
mfxStatus MFX_CDECL MFXVideoUSER_GetPlugin(mfxSession session, mfxU32 type, mfxPlugin *par);
mfxStatus MFX_CDECL MFXVideoUSER_ProcessFrameAsync(mfxSession session, const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxSyncPoint *syncp); 

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {} mfxENCInput;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {} mfxENCOutput;
MFX_PACK_END() 

mfxStatus MFX_CDECL MFXVideoENC_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);
mfxStatus MFX_CDECL MFXVideoENC_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request);
mfxStatus MFX_CDECL MFXVideoENC_Init(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoENC_Reset(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoENC_Close(mfxSession session);

mfxStatus MFX_CDECL MFXVideoENC_ProcessFrameAsync(mfxSession session, mfxENCInput *in, mfxENCOutput *out, mfxSyncPoint *syncp);

mfxStatus MFX_CDECL MFXVideoENC_GetVideoParam(mfxSession session, mfxVideoParam *par);

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {} mfxPAKInput;
MFX_PACK_END()

MFX_PACK_BEGIN_STRUCT_W_PTR()
typedef struct {} mfxPAKOutput;
MFX_PACK_END()

mfxStatus MFX_CDECL MFXVideoPAK_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);
mfxStatus MFX_CDECL MFXVideoPAK_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest request[2]);
mfxStatus MFX_CDECL MFXVideoPAK_Init(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoPAK_Reset(mfxSession session, mfxVideoParam *par);
mfxStatus MFX_CDECL MFXVideoPAK_Close(mfxSession session);

mfxStatus MFX_CDECL MFXVideoPAK_ProcessFrameAsync(mfxSession session, mfxPAKInput *in, mfxPAKOutput *out,  mfxSyncPoint *syncp);

mfxStatus MFX_CDECL MFXVideoPAK_GetVideoParam(mfxSession session, mfxVideoParam *par);
 

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //MFX_ONEVPL
#endif //__MFXDEPRECATED_H__

