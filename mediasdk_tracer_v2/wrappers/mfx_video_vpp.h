#ifndef MFX_VIDEO_VPP_H_
#define MFX_VIDEO_VPP_H_

#include "mfxvideo.h"
#include "../loggers/log.h"
#include "../dumps/dump.h"
#include "../tracer/functions_table.h"

mfxStatus MFXVideoVPP_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out);
mfxStatus MFXVideoVPP_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request);
mfxStatus MFXVideoVPP_Init(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoVPP_Reset(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoVPP_Close(mfxSession session);
mfxStatus MFXVideoVPP_GetVideoParam(mfxSession session, mfxVideoParam *par);
mfxStatus MFXVideoVPP_GetVPPStat(mfxSession session, mfxVPPStat *stat);
mfxStatus MFXVideoVPP_RunFrameVPPAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp);

#endif //MFX_VIDEO_VPP_H_
