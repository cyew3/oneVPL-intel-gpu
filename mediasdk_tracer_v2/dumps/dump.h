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

#define DUMP_FIELD(_field) \
    str += structName + "." #_field "=" + ToString(_struct._field) + "\n";

#define DUMP_FIELD_RESERVED(_field) \
    str += structName + "." #_field "[]=" + DUMP_RESERVED_ARRAY(_struct._field) + "\n";

#define ToString( x ) dynamic_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

#define TimeToString( x ) dynamic_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::left << std::setw(4) << std::dec << x <<" msec") ).str()

/*
#define ToHexFormatString( x ) dynamic_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::hex << x ) ).str()
*/

#define ToHexFormatString( x ) StringToHexString(dynamic_cast< std::ostringstream & >( ( std::ostringstream() << std::dec << x ) ).str() )

#define DEFINE_GET_TYPE(type) \
    template<> \
    inline const char* get_type<type>(){ return #type; }

#define DEFINE_DUMP_FUNCTION(type) \
    DEFINE_GET_TYPE(type) \
    std::string dump(const std::string structName, const type &_var);

enum eDumpContect {
    DUMPCONTEXT_MFX,
    DUMPCONTEXT_VPP,
    DUMPCONTEXT_ALL,
};

enum eDumpFormat {
    DUMP_DEC,
    DUMP_HEX
};

class DumpContext
{
public:
    eDumpContect context;

    DumpContext(void) {
        context = DUMPCONTEXT_ALL;
    };
    ~DumpContext(void) {};

    template<typename T>
    inline std::string toString( T x, eDumpFormat format = DUMP_DEC){
        return dynamic_cast< std::ostringstream & >(( std::ostringstream() << ((format == DUMP_DEC) ? std::dec : std::hex) << x )).str();
    }

    const char* get_bufferid_str(mfxU32 bufferid);

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

    template<typename T>
    std::string dump(const std::string structName, const T *_struct)
    {
        std::string str = get_type<T>();
        str += "* " + structName + "=" + ToString(_struct) + "\n";
        if (_struct) str += dump("  " + structName, *_struct);
        return str;
    }

    template<typename T>
    inline std::string dump_mfxExtParams(const std::string structName, const T& _struct)
    {
        std::string str;
        std::string name;

        str += structName + ".NumExtParam=" + ToString(_struct.NumExtParam) + "\n";
        str += structName + ".ExtParam=" + ToString(_struct.ExtParam) + "\n";
        for (mfxU16 i = 0; i < _struct.NumExtParam; ++i)
        {
            name = structName + ".ExtParam[" + ToString(i) + "]";
            str += name + "=" + ToString(_struct.ExtParam[i]) + "\n";
            if (_struct.ExtParam[i]) {
                switch (_struct.ExtParam[i]->BufferId)
                {
                  case MFX_EXTBUFF_CODING_OPTION:
                    str += dump(name, *((mfxExtCodingOption*)_struct.ExtParam[i])) + "\n";
                    break;
                  case MFX_EXTBUFF_CODING_OPTION2:
                    str += dump(name, *((mfxExtCodingOption2*)_struct.ExtParam[i])) + "\n";
                    break;
                  case MFX_EXTBUFF_CODING_OPTION3:
                    str += dump(name, *((mfxExtCodingOption3*)_struct.ExtParam[i])) + "\n";
                    break;
                  case MFX_EXTBUFF_ENCODER_RESET_OPTION:
                    str += dump(name, *((mfxExtEncoderResetOption*)_struct.ExtParam[i])) + "\n";
                    break;
                  default:
                    str += dump(name, *(_struct.ExtParam[i])) + "\n";
                    break;
                };
            }
        }
        return str;
    }

    //mfxdefs
    std::string dump_mfxU32(const std::string structName, mfxU32 u32);
    std::string dump_mfxU64(const std::string structName, mfxU64 u64);
    std::string dump_mfxHDL(const std::string structName, const mfxHDL *hdl);
    std::string dump_mfxStatus(const std::string structName, mfxStatus status);

    //mfxcommon
    DEFINE_DUMP_FUNCTION(mfxBitstream);
    DEFINE_DUMP_FUNCTION(mfxExtBuffer);
    DEFINE_DUMP_FUNCTION(mfxIMPL);
    DEFINE_DUMP_FUNCTION(mfxPriority);
    DEFINE_DUMP_FUNCTION(mfxVersion);
    DEFINE_DUMP_FUNCTION(mfxSyncPoint);

    //mfxenc
    DEFINE_DUMP_FUNCTION(mfxENCInput);
    DEFINE_DUMP_FUNCTION(mfxENCOutput);

    //mfxplugin
    DEFINE_DUMP_FUNCTION(mfxPlugin);

    //mfxstructures
    DEFINE_DUMP_FUNCTION(mfxDecodeStat);
    DEFINE_DUMP_FUNCTION(mfxEncodeCtrl);
    DEFINE_DUMP_FUNCTION(mfxEncodeStat);
    DEFINE_DUMP_FUNCTION(mfxExtCodingOption);
    DEFINE_DUMP_FUNCTION(mfxExtCodingOption2);
    DEFINE_DUMP_FUNCTION(mfxExtCodingOption3);
    DEFINE_DUMP_FUNCTION(mfxExtEncoderResetOption);
    DEFINE_DUMP_FUNCTION(mfxExtVppAuxData);
    DEFINE_DUMP_FUNCTION(mfxFrameAllocRequest);
    DEFINE_DUMP_FUNCTION(mfxFrameData);
    DEFINE_DUMP_FUNCTION(mfxFrameId);
    DEFINE_DUMP_FUNCTION(mfxFrameInfo);
    DEFINE_DUMP_FUNCTION(mfxFrameSurface1);
    DEFINE_DUMP_FUNCTION(mfxHandleType);
    DEFINE_DUMP_FUNCTION(mfxInfoMFX);
    DEFINE_DUMP_FUNCTION(mfxInfoVPP);
    DEFINE_DUMP_FUNCTION(mfxPayload);
    DEFINE_DUMP_FUNCTION(mfxSkipMode);
    DEFINE_DUMP_FUNCTION(mfxVideoParam);
    DEFINE_DUMP_FUNCTION(mfxVPPStat);
    DEFINE_DUMP_FUNCTION(mfxExtVPPDoNotUse);

    //mfxsession
    DEFINE_DUMP_FUNCTION(mfxSession);

    //mfxvideo
    DEFINE_DUMP_FUNCTION(mfxBufferAllocator);
    DEFINE_DUMP_FUNCTION(mfxFrameAllocator);
};
#endif //DUMP_H_

