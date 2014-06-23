#ifndef DUMP_H_
#define DUMP_H_

#include <string>
#include <sstream>
#include <iomanip>
#include <iterator>
#include "mfxvideo.h"
#include "mfxplugin.h"
#include "mfxenc.h"

std::string StringToHexString(std::string data);

#define GET_ARRAY_SIZE(x) sizeof(x)/sizeof(x[0])
#define DUMP_RESERVED_ARRAY(r) dump_reserved_array(&(r[0]), GET_ARRAY_SIZE(r))

#define ToString( x ) dynamic_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

#define TimeToString( x ) dynamic_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::left << std::setw(4) << std::dec << x <<" msec") ).str()

/*
#define ToHexFormatString( x ) dynamic_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::hex << x ) ).str()
*/

#define ToHexFormatString( x ) StringToHexString(dynamic_cast< std::ostringstream & >( ( std::ostringstream() << std::dec << x ) ).str() )

template<typename T>
std::string dump_reserved_array(T* data, size_t size)
{
    std::stringstream result;

    result << "{ ";
    for (size_t i = 0; i < size; ++i) {
        result << data[i];
        if (i < (size-1)) result << ", ";
    }
    result << " }";
    return result.str();
}

//mfxdefs
std::string dump_mfxU32(const std::string structName, mfxU32 u32);
std::string dump_mfxU64(const std::string structName, mfxU64 u64);
std::string dump_mfxHDL(const std::string structName, const mfxHDL *hdl);
std::string dump_mfxStatus(const std::string structName, mfxStatus status);

//mfxcommon
std::string dump_mfxBitstream(const std::string structName, mfxBitstream *bitstream);
std::string dump_mfxExtBuffer(const std::string structName, mfxExtBuffer *extBuffer);
std::string dump_mfxIMPL(const std::string structName, mfxIMPL impl);
std::string dump_mfxPriority(const std::string structName, mfxPriority priority);
std::string dump_mfxVersion(const std::string structName, mfxVersion *version);
std::string dump_mfxSyncPoint(const std::string structName, mfxSyncPoint *syncPoint);

//mfxenc
std::string dump_mfxENCInput(const std::string structName, mfxENCInput* encIn);
std::string dump_mfxENCOutput(const std::string structName, mfxENCOutput* encOut);

//mfxplugin
std::string dump_mfxPlugin(const std::string structName, const mfxPlugin* plugin);

//mfxstructures
std::string dump_mfxDecodeStat(const std::string structName, mfxDecodeStat *decodeStat);
std::string dump_mfxEncodeCtrl(const std::string structName, mfxEncodeCtrl *EncodeCtrl);
std::string dump_mfxEncodeStat(const std::string structName, mfxEncodeStat *encodeStat);
std::string dump_mfxFrameAllocRequest(const std::string structName, mfxFrameAllocRequest *frameAllocRequest);
std::string dump_mfxFrameData(const std::string structName, mfxFrameData *frameData);
std::string dump_mfxFrameId(const std::string structName, mfxFrameId *frame);
std::string dump_mfxFrameInfo(const std::string structName, mfxFrameInfo *info);
std::string dump_mfxFrameSurface1(const std::string structName, mfxFrameSurface1 *frameSurface1);
std::string dump_mfxHandleType(const std::string structName, mfxHandleType handleType);
std::string dump_mfxInfoMFX(const std::string structName, mfxInfoMFX *mfx);
std::string dump_mfxInfoVPP(const std::string structName, mfxInfoVPP *vpp);
std::string dump_mfxPayload(const std::string structName, mfxPayload *payload);
std::string dump_mfxSkipMode(const std::string structName, mfxSkipMode skipMode);
std::string dump_mfxVideoParam(const std::string structName, mfxVideoParam *videoParam);
std::string dump_mfxVPPStat(const std::string structName, mfxVPPStat *vppStat);
std::string dump_mfxExtVppAuxData(const std::string structName, mfxExtVppAuxData *extVppAuxData);

//mfxsession
std::string dump_mfxSession(const std::string structName, mfxSession session);

//mfxvideo
std::string dump_mfxBufferAllocator(const std::string structName, mfxBufferAllocator *allocator);
std::string dump_mfxFrameAllocator(const std::string structName, mfxFrameAllocator *allocator);

#endif //DUMP_H_
