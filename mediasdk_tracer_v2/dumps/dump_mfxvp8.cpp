#include "dump.h"

std::string DumpContext::dump(const std::string structName, const mfxExtVP8CodingOption &_struct)
{
    std::string str;

    str += dump(structName + ".Header=", _struct.Header) + "\n";

    DUMP_FIELD(Version);
    DUMP_FIELD(EnableMultipleSegments);
    DUMP_FIELD(LoopFilterType);
    DUMP_FIELD_RESERVED(LoopFilterLevel);
    DUMP_FIELD(SharpnessLevel);
    DUMP_FIELD(NumTokenPartitions);
    DUMP_FIELD_RESERVED(LoopFilterRefTypeDelta);
    DUMP_FIELD_RESERVED(LoopFilterMbModeDelta);
    DUMP_FIELD_RESERVED(SegmentQPDelta);
    DUMP_FIELD_RESERVED(CoeffTypeQPDelta);
    DUMP_FIELD(WriteIVFHeaders);
    DUMP_FIELD(NumFramesForIVFHeader);
    DUMP_FIELD(reserved);

    return str;
}