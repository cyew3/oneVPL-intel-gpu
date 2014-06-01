#include "dump.h"

void dump_mfxU32(const std::string structName, mfxU32 u32)
{
    Log::WriteLog("mfxU32 " + structName + "=" + ToString(u32) + "\n");
}

void dump_mfxU64(const std::string structName, mfxU64 u64)
{
    Log::WriteLog("mfxU64 " + structName + "=" + ToString(u64) + "\n");
}
