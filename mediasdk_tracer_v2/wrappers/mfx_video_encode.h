#ifndef MFX_VIDEO_ENCODE_H_
#define MFX_VIDEO_ENCODE_H_

#include "mfxvideo.h"
#include "../loggers/log.h"
#include "../dumps/dump.h"
#include "../tracer/functions_table.h"

mfxStatus MFXVideoENCODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);
mfxStatus MFXVideoENCODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request);
mfxStatus MFXVideoENCODE_Init(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoENCODE_Reset(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoENCODE_Close(mfxSession session);
mfxStatus MFXVideoENCODE_GetVideoParam(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoENCODE_GetEncodeStat(mfxSession session, mfxEncodeStat *stat);
mfxStatus MFXVideoENCODE_EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp);

#endif //MFX_VIDEO_ENCODE_H_