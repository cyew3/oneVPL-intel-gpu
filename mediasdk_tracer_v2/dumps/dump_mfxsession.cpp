#include "dump.h"

std::string DumpContext::dump(const std::string structName, const mfxSession &session)
{
    return std::string("mfxSession " + structName + "=" + ToString(session));
}
