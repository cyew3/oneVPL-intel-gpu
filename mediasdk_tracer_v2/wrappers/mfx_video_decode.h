#ifndef MFX_VIDEO_DECODE_H_
#define MFX_VIDEO_DECODE_H_

#include "mfxvideo.h"
#include "../loggers/log.h"
#include "../dumps/dump.h"
#include "../tracer/functions_table.h"

mfxStatus MFXVideoDECODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);
mfxStatus MFXVideoDECODE_DecodeHeader(mfxSession session, mfxBitstream *bs, mfxVideoParam *par);
mfxStatus MFXVideoDECODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request);
mfxStatus MFXVideoDECODE_Init(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoDECODE_Reset(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoDECODE_Close(mfxSession session);
mfxStatus MFXVideoDECODE_GetVideoParam(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoDECODE_GetDecodeStat(mfxSession session, mfxDecodeStat *stat);
mfxStatus MFXVideoDECODE_SetSkipMode(mfxSession session, mfxSkipMode mode);
mfxStatus MFXVideoDECODE_GetPayload(mfxSession session, mfxU64 *ts, mfxPayload *payload);
mfxStatus MFXVideoDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);

#endif //MFX_VIDEO_DECODE_H_
