#include "dump.h"

std::string dump_mfxENCInput(const std::string structName, mfxENCInput* encIn)
{
    std::string str = "mfxENCOutput* " + structName + "=" + ToString(encIn) + "\n";
    if (encIn) {
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(encIn->reserved) + "\n";
      str += structName + ".InSurface=" + ToString(encIn->InSurface) + "\n";
      str += structName + ".NumFrameL0=" + ToString(encIn->NumFrameL0) + "\n";
      str += structName + ".L0Surface=" + ToString(encIn->L0Surface) + "\n";
      str += structName + ".NumFrameL1=" + ToString(encIn->NumFrameL1) + "\n";
      str += structName + ".L1Surface=" + ToString(encIn->L1Surface) + "\n";
      str += structName + ".NumExtParam=" + ToString(encIn->NumExtParam) + "\n";
      str += structName + ".ExtParam=" + ToString(encIn->ExtParam) + "\n";
    }
    return str;
}

std::string dump_mfxENCOutput(const std::string structName, mfxENCOutput* encOut)
{
    std::string str = "mfxENCOutput* " + structName + "=" + ToString(encOut) + "\n";
    if (encOut) {
      str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(encOut->reserved) + "\n";
      str += structName + ".NumExtParam=" + ToString(encOut->NumExtParam) + "\n";
      str += structName + ".ExtParam=" + ToString(encOut->ExtParam) + "\n";
    }
    return str;
}
