#ifndef DUMP_H_
#define DUMP_H_

#include <string>
#include <sstream>
#include "mfxvideo.h"
#include "../loggers/log.h"

#define ToString( x ) dynamic_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()


//mfxdefs
void dump_mfxU32(const std::string structName, mfxU32 u32);
void dump_mfxU64(const std::string structName, mfxU64 u64);


//mfxcommon
void dump_mfxStatus(const std::string structName, mfxStatus status);
void dump_mfxSession(const std::string structName, mfxSession session);
void dump_mfxIMPL(const std::string structName, mfxIMPL *impl);
void dump_mfxVersion(const std::string structName, mfxVersion version);
void dump_mfxHDL(const std::string structName, mfxHDL *hdl);
void dump_mfxFrameId(const std::string structName, mfxFrameId frame);
void dump_mfxFrameInfo(const std::string structName, mfxFrameInfo info);
void dump_mfxBufferAllocator(const std::string structName, mfxBufferAllocator *allocator);
void dump_mfxFrameAllocator(const std::string structName, mfxFrameAllocator *allocator);
void dump_mfxFrameAllocRequest(const std::string structName, mfxFrameAllocRequest *frameAllocRequest);
void dump_mfxHandleType(const std::string structName, mfxHandleType handleType);
void dump_mfxSyncPoint(const std::string structName, mfxSyncPoint *syncPoint);
void dump_mfxInfoVPP(const std::string structName, mfxInfoVPP *vpp);
void dump_mfxInfoMFX(const std::string structName, mfxInfoMFX *mfx);
void dump_mfxVideoParam(const std::string structName, mfxVideoParam *videoParam);
void dump_mfxEncodeStat(const std::string structName, mfxEncodeStat *encodeStat);
void dump_mfxExtBuffer(const std::string structName, mfxExtBuffer *extBuffer);
void dump_mfxEncodeCtrl(const std::string structName, mfxEncodeCtrl *EncodeCtrl);
void dump_mfxFrameData(const std::string structName, mfxFrameData *frameData);
void dump_mfxFrameSurface1(const std::string structName, mfxFrameSurface1 *frameSurface1);
void dump_mfxBitstream(const std::string structName, mfxBitstream *bitstream);
void dump_mfxDecodeStat(const std::string structName, mfxDecodeStat *decodeStat);
void dump_mfxSkipMode(const std::string structName, mfxSkipMode skipMode);
void dump_mfxPayload(const std::string structName, mfxPayload *payload);
void dump_mfxVPPStat(const std::string structName, mfxVPPStat *vppStat);
void dump_mfxExtVppAuxData(const std::string structName, mfxExtVppAuxData *extVppAuxData);
void dump_mfxPriority(const std::string structName, mfxPriority priority);


//mfxstructures
/*
void dump_mfxFrameInfo(const std::string structName, mfxFrameInfo info);
void dump_mfxBufferAllocator(const std::string structName, mfxBufferAllocator *allocator);
void dump_mfxFrameAllocator(const std::string structName, mfxFrameAllocator *allocator);
void dump_mfxFrameAllocRequest(const std::string structName, mfxFrameAllocRequest *frameAllocRequest);
void dump_mfxHandleType(const std::string structName, mfxHandleType handleType);
void dump_mfxSyncPoint(const std::string structName, mfxSyncPoint *syncPoint);
void dump_mfxInfoVPP(const std::string structName, mfxInfoVPP *vpp);
void dump_mfxInfoMFX(const std::string structName, mfxInfoMFX *mfx);
void dump_mfxVideoParam(const std::string structName, mfxVideoParam *videoParam);
void dump_mfxEncodeStat(const std::string structName, mfxEncodeStat *encodeStat);
void dump_mfxExtBuffer(const std::string structName, mfxExtBuffer *extBuffer);
void dump_mfxEncodeCtrl(const std::string structName, mfxEncodeCtrl *EncodeCtrl);
void dump_mfxFrameData(const std::string structName, mfxFrameData *frameData);
void dump_mfxFrameSurface1(const std::string structName, mfxFrameSurface1 *frameSurface1);
void dump_mfxBitstream(const std::string structName, mfxBitstream *bitstream);
void dump_mfxDecodeStat(const std::string structName, mfxDecodeStat *decodeStat);
void dump_mfxSkipMode(const std::string structName, mfxSkipMode skipMode);
void dump_mfxPayload(const std::string structName, mfxPayload *payload);
void dump_mfxVPPStat(const std::string structName, mfxVPPStat *vppStat);
void dump_mfxExtVppAuxData(const std::string structName, mfxExtVppAuxData *extVppAuxData);
*/
#endif //DUMP_H_
