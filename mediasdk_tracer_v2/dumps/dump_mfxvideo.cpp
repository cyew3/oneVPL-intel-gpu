#include "dump.h"
#include "../loggers/log.h"

std::string DumpContext::dump(const std::string structName, const mfxFrameAllocator &allocator)
{
    std::string str;
    str += dump_mfxHDL(structName + ".pthis=", &allocator.pthis) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(allocator.reserved) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxBufferAllocator &allocator)
{
    std::string str;
    str += dump_mfxHDL(structName + ".pthis", &allocator.pthis) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(allocator.reserved) + "\n";
    return str;
}
