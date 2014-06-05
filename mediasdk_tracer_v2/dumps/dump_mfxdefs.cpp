#include "dump.h"

std::string dump_mfxU32(const std::string structName, mfxU32 u32)
{
    return std::string("mfxU32 " + structName + "=" + ToString(u32));
}

std::string dump_mfxU64(const std::string structName, mfxU64 u64)
{
    return std::string("mfxU64 " + structName + "=" + ToString(u64));
}
