#include "dump.h"

std::string DumpContext::dump_mfxU32(const std::string structName, mfxU32 u32)
{
    return std::string("mfxU32 " + structName + "=" + ToString(u32));
}

std::string DumpContext::dump_mfxU64(const std::string structName, mfxU64 u64)
{
    return std::string("mfxU64 " + structName + "=" + ToString(u64));
}

std::string DumpContext::dump_mfxHDL(const std::string structName, const mfxHDL *hdl)
{
    return std::string("mfxHDL* " + structName + "=" + ToString(hdl));
}

std::string DumpContext::dump_mfxStatus(const std::string structName, mfxStatus status)
{
    return std::string(structName + "=" + GetStatusString(status));
}
