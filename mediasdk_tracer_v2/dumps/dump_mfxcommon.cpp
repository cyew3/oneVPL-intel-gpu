#include "dump.h"
#include "../loggers/log.h"

std::string DumpContext::dump(const std::string structName, const mfxBitstream &bitstream)
{
    std::string str;
    str += structName + ".EncryptedData=" + ToString(bitstream.EncryptedData) + "\n";
    str += dump_mfxExtParams(structName, bitstream) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(bitstream.reserved) + "\n";
    str += structName + ".DecodeTimeStamp=" + ToString(bitstream.DecodeTimeStamp) + "\n";
    str += structName + ".TimeStamp=" + ToString(bitstream.TimeStamp) + "\n";
    if (Log::GetLogLevel() == LOG_LEVEL_FULL) {
        str += structName + ".Data=" + ToHexFormatString(bitstream.Data) + "\n";
    }
    str += structName + ".DataOffset=" + ToString(bitstream.DataOffset) + "\n";
    str += structName + ".DataLength=" + ToString(bitstream.DataLength) + "\n";
    str += structName + ".MaxLength=" + ToString(bitstream.MaxLength) + "\n";
    str += structName + ".PicStruct=" + ToString(bitstream.PicStruct) + "\n";
    str += structName + ".FrameType=" + ToString(bitstream.FrameType) + "\n";
    str += structName + ".DataFlag=" + ToString(bitstream.DataFlag) + "\n";
    str += structName + ".reserved2=" + ToString(bitstream.reserved2);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtBuffer &extBuffer)
{
    std::string str;
    const char* bufid_str = get_bufferid_str(extBuffer.BufferId);
    if (bufid_str)
        str += structName + ".BufferId=" + std::string(bufid_str) + "\n";
    else
        str += structName + ".BufferId=" + ToString(extBuffer.BufferId) + "\n";
    str += structName + ".BufferSz=" + ToString(extBuffer.BufferSz);
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxIMPL &impl)
{
    return std::string("mfxIMPL " + structName + "=" + ToString(impl));
}

std::string DumpContext::dump(const std::string structName, const mfxPriority &priority)
{
    return std::string("mfxPriority " + structName + "=" + ToString(priority));
}

std::string DumpContext::dump(const std::string structName, const mfxSyncPoint &syncPoint)
{
    return std::string("mfxSyncPoint* " + structName + "=" + ToString(syncPoint));
}

std::string DumpContext::dump(const std::string structName, const mfxVersion &version)
{
    std::string str;
    str += structName + ".Major=" + ToString(version.Major) + "\n";
    str += structName + ".Minor=" + ToString(version.Minor) + "\n";
    str += structName + ".Version=" + ToString(version.Version);
    return str;
}
