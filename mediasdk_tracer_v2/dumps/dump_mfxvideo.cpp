#include "dump.h"
#include "../loggers/log.h"

std::string dump_mfxFrameAllocator(const std::string structName, mfxFrameAllocator *allocator)
{
    std::string str = "mfxFrameAllocator* " + structName + "=" + ToString(allocator) + "\n";
    if(allocator){
      str += dump_mfxHDL(structName + ".pthis=", &allocator->pthis) + "\n";
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(allocator->reserved) + "\n";
    }
    return str;
}

std::string dump_mfxBufferAllocator(const std::string structName, mfxBufferAllocator *allocator)
{
    std::string str = "mfxBufferAllocator* " + structName + "=" + ToString(allocator) + "\n";
    if(allocator){
      str += dump_mfxHDL(structName + ".pthis", &allocator->pthis) + "\n";
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(allocator->reserved) + "\n";
    }
    return str;
}
