#include "dump.h"

std::string dump_mfxSession(const std::string structName, mfxSession session)
{
    return std::string("mfxSession " + structName + "=" + ToString(session));
}
