#ifndef MFXCORE_H_
#define MFXCORE_H_

#include "mfxvideo.h"
#include "../loggers/log.h"
#include "../dumps/dump.h"
#include "../tracer/functions_table.h"

mfxStatus MFXQueryIMPL(mfxSession session, mfxIMPL *impl);
mfxStatus MFXQueryVersion(mfxSession session, mfxVersion *version);
mfxStatus MFXJoinSession(mfxSession session, mfxSession child_session);
mfxStatus MFXCloneSession(mfxSession session, mfxSession *clone);
mfxStatus MFXDisjoinSession(mfxSession session);
mfxStatus MFXSetPriority(mfxSession session, mfxPriority priority);
mfxStatus MFXGetPriority(mfxSession session, mfxPriority *priority);

#endif //MFXCORE_H_