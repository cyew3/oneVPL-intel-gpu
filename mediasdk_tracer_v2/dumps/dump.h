#ifndef DUMP_H_
#define DUMP_H_

#include <string>
#include <sstream>
#include <iomanip>
#include <iterator>
#include <typeinfo>
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

template<typename T>
inline const char* get_type(){ return typeid(T).name(); }

#define DEFINE_GET_TYPE(type) \
template<> \
inline const char* get_type<type>(){ return #type; }

template<typename T>
std::string dump(const std::string structName, const T *_struct)
{
    std::string str = get_type<T>();
    str += "* " + structName + "=" + ToString(_struct) + "\n";
    if (_struct) str += dump("  " + structName, *_struct);
    return str;
}

#define DEFINE_DUMP_FUNCTION(type) \
  std::string dump(const std::string structName, const type &_var);

#define DEFINE_DUMP_FOR_TYPE(type) \
  DEFINE_GET_TYPE(type) \
  DEFINE_DUMP_FUNCTION(type)

//mfxdefs
std::string dump_mfxU32(const std::string structName, mfxU32 u32);
std::string dump_mfxU64(const std::string structName, mfxU64 u64);
std::string dump_mfxHDL(const std::string structName, const mfxHDL *hdl);
std::string dump_mfxStatus(const std::string structName, mfxStatus status);

//mfxcommon
DEFINE_DUMP_FOR_TYPE(mfxBitstream);
DEFINE_DUMP_FOR_TYPE(mfxExtBuffer);
DEFINE_DUMP_FOR_TYPE(mfxIMPL);
DEFINE_DUMP_FOR_TYPE(mfxPriority);
DEFINE_DUMP_FOR_TYPE(mfxVersion);
DEFINE_DUMP_FOR_TYPE(mfxSyncPoint);

//mfxenc
DEFINE_DUMP_FOR_TYPE(mfxENCInput);
DEFINE_DUMP_FOR_TYPE(mfxENCOutput);

//mfxplugin
DEFINE_DUMP_FOR_TYPE(mfxPlugin);

//mfxstructures
DEFINE_DUMP_FOR_TYPE(mfxDecodeStat);
DEFINE_DUMP_FOR_TYPE(mfxEncodeCtrl);
DEFINE_DUMP_FOR_TYPE(mfxFrameAllocRequest);
DEFINE_DUMP_FOR_TYPE(mfxFrameData);
DEFINE_DUMP_FOR_TYPE(mfxFrameId);
DEFINE_DUMP_FOR_TYPE(mfxFrameInfo);
DEFINE_DUMP_FOR_TYPE(mfxFrameSurface1);
DEFINE_DUMP_FOR_TYPE(mfxHandleType);
DEFINE_DUMP_FOR_TYPE(mfxInfoMFX);
DEFINE_DUMP_FOR_TYPE(mfxInfoVPP);
DEFINE_DUMP_FOR_TYPE(mfxPayload);
DEFINE_DUMP_FOR_TYPE(mfxSkipMode);
DEFINE_DUMP_FOR_TYPE(mfxVideoParam);
DEFINE_DUMP_FOR_TYPE(mfxVPPStat);
DEFINE_DUMP_FOR_TYPE(mfxExtVppAuxData);

//mfxsession
DEFINE_DUMP_FOR_TYPE(mfxSession);

//mfxvideo
DEFINE_DUMP_FOR_TYPE(mfxBufferAllocator);
DEFINE_DUMP_FOR_TYPE(mfxFrameAllocator);

#endif //DUMP_H_
