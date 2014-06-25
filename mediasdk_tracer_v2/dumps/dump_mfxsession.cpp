#include "dump.h"

std::string dump(const std::string structName, const mfxSession &session)
{
    return std::string("mfxSession " + structName + "=" + ToString(session));
}
