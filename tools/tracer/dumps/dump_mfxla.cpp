/* ****************************************************************************** *\

Copyright (C) 2020 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

\* ****************************************************************************** */

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
