#include "dump.h"

std::string DumpContext::dump(const std::string structName, const mfxExtLAControl &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";
    DUMP_FIELD(LookAheadDepth);
    DUMP_FIELD(DependencyDepth);
    DUMP_FIELD(DownScaleFactor);
    DUMP_FIELD(BPyramid);

    DUMP_FIELD_RESERVED(reserved1);

    DUMP_FIELD(NumOutStream);

    str += structName + ".OutStream[]={\n";
    for (int i = 0; i < _struct.NumOutStream; i++)
    {
        str += dump("", _struct.OutStream[i]) + ",\n";
    }
    str += "}\n";

    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtLAControl::mfxStream &_struct)
{
    std::string str;

    DUMP_FIELD(Width);
    DUMP_FIELD(Height);

    DUMP_FIELD_RESERVED(reserved2);

    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxLAFrameInfo &_struct)
{
    std::string str;
    
    DUMP_FIELD(Width);
    DUMP_FIELD(Height);

    DUMP_FIELD(FrameType);
    DUMP_FIELD(FrameDisplayOrder);
    DUMP_FIELD(FrameEncodeOrder);

    DUMP_FIELD(IntraCost);
    DUMP_FIELD(InterCost);
    DUMP_FIELD(DependencyCost);
    DUMP_FIELD(Layer);
    DUMP_FIELD_RESERVED(reserved);

    DUMP_FIELD_RESERVED(EstimatedRate);

    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxExtLAFrameStatistics &_struct)
{
    std::string str;

    str += dump(structName + ".Header", _struct.Header) + "\n";

    DUMP_FIELD_RESERVED(reserved);

    DUMP_FIELD(NumAlloc);
    DUMP_FIELD(NumStream);
    DUMP_FIELD(NumFrame);
    

    str += structName + ".FrameStat[]={\n";
    for (int i = 0; i < _struct.NumStream; i++)
    {
        str += dump("", _struct.FrameStat[i]) + ",\n";
    }
    str += "}\n";


    if (_struct.OutSurface)
    {
        str += dump(structName + ".OutSurface", *_struct.OutSurface) + "\n";
    }
    else
    {
        str += structName + ".OutSurface=NULL\n";
    }

    return str;
}
