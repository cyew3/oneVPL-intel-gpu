#include "dump.h"

std::string DumpContext::dump(const std::string structName, const mfxENCInput &encIn)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(encIn.reserved) + "\n";
    str += structName + ".InSurface=" + ToString(encIn.InSurface) + "\n";
    str += structName + ".NumFrameL0=" + ToString(encIn.NumFrameL0) + "\n";
    str += structName + ".L0Surface=" + ToString(encIn.L0Surface) + "\n";
    str += structName + ".NumFrameL1=" + ToString(encIn.NumFrameL1) + "\n";
    str += structName + ".L1Surface=" + ToString(encIn.L1Surface) + "\n";
    str += dump_mfxExtParams(structName, encIn);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxENCOutput &encOut)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(encOut.reserved) + "\n";
    str += dump_mfxExtParams(structName, encOut);
    return str;
}
