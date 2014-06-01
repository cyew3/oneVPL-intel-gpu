#ifndef MFXVIDEOCORE_H_
#define MFXVIDEOCORE_H_

#include "mfxvideo.h"
#include "../loggers/log.h"
#include "../dumps/dump.h"
#include "../tracer/functions_table.h"

mfxStatus MFXVideoCORE_SetBufferAllocator(mfxSession session, mfxBufferAllocator *allocator);
mfxStatus MFXVideoCORE_SetFrameAllocator(mfxSession session, mfxFrameAllocator *allocator);
mfxStatus MFXVideoCORE_SetHandle(mfxSession session, mfxHandleType type, mfxHDL hdl);
mfxStatus MFXVideoCORE_GetHandle(mfxSession session, mfxHandleType type, mfxHDL *hdl);
mfxStatus MFXVideoCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait);

#endif //MFXVIDEOCORE_H_
