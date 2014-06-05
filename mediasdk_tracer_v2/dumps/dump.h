#ifndef DUMP_H_
#define DUMP_H_

#include <string>
#include <sstream>
#include "mfxvideo.h"

#define ToString( x ) dynamic_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()


//mfxdefs
std::string dump_mfxU32(const std::string structName, mfxU32 u32);
std::string dump_mfxU64(const std::string structName, mfxU64 u64);


//mfxcommon
std::string dump_mfxStatus(const std::string structName, mfxStatus status);
std::string dump_mfxSession(const std::string structName, mfxSession session);
std::string dump_mfxIMPL(const std::string structName, mfxIMPL *impl);
std::string dump_mfxVersion(const std::string structName, mfxVersion version);
std::string dump_mfxHDL(const std::string structName, mfxHDL *hdl);
std::string dump_mfxFrameId(const std::string structName, mfxFrameId frame);
std::string dump_mfxFrameInfo(const std::string structName, mfxFrameInfo info);
std::string dump_mfxBufferAllocator(const std::string structName, mfxBufferAllocator *allocator);
std::string dump_mfxFrameAllocator(const std::string structName, mfxFrameAllocator *allocator);
std::string dump_mfxFrameAllocRequest(const std::string structName, mfxFrameAllocRequest *frameAllocRequest);
std::string dump_mfxHandleType(const std::string structName, mfxHandleType handleType);
std::string dump_mfxSyncPoint(const std::string structName, mfxSyncPoint *syncPoint);
std::string dump_mfxInfoVPP(const std::string structName, mfxInfoVPP *vpp);
std::string dump_mfxInfoMFX(const std::string structName, mfxInfoMFX *mfx);
std::string dump_mfxVideoParam(const std::string structName, mfxVideoParam *videoParam);
std::string dump_mfxEncodeStat(const std::string structName, mfxEncodeStat *encodeStat);
std::string dump_mfxExtBuffer(const std::string structName, mfxExtBuffer *extBuffer);
std::string dump_mfxEncodeCtrl(const std::string structName, mfxEncodeCtrl *EncodeCtrl);
std::string dump_mfxFrameData(const std::string structName, mfxFrameData *frameData);
std::string dump_mfxFrameSurface1(const std::string structName, mfxFrameSurface1 *frameSurface1);
std::string dump_mfxBitstream(const std::string structName, mfxBitstream *bitstream);
std::string dump_mfxDecodeStat(const std::string structName, mfxDecodeStat *decodeStat);
std::string dump_mfxSkipMode(const std::string structName, mfxSkipMode skipMode);
std::string dump_mfxPayload(const std::string structName, mfxPayload *payload);
std::string dump_mfxVPPStat(const std::string structName, mfxVPPStat *vppStat);
std::string dump_mfxExtVppAuxData(const std::string structName, mfxExtVppAuxData *extVppAuxData);
std::string dump_mfxPriority(const std::string structName, mfxPriority priority);


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
