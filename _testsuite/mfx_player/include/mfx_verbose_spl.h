/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */


#pragma once
#include "mfx_ibitstream_reader.h"

#define LOG_LEVEL_SILENT   0
#define LOG_LEVEL_CRITICAL 1
#define LOG_LEVEL_ERROR    2
#define LOG_LEVEL_PERF     4
#define LOG_LEVEL_WARNING  8
#define LOG_LEVEL_INFO     16
#define LOG_LEVEL_DEBUG    32

class VerboseSpl : public InterfaceProxy<IBitstreamReader>
{
    int m_level;
    int m_nFrame;
    typedef InterfaceProxy<IBitstreamReader> base;
public:
    VerboseSpl (int level, IBitstreamReader * pTarget)
        : base(pTarget)
        , m_level(level)
        , m_nFrame(0)
    {
    }

    virtual mfxStatus ReadNextFrame(mfxBitstream2 &bs)
    {
        mfxStatus sts = base::ReadNextFrame(bs);

        if (MFX_ERR_NONE == sts)
        {
            tstringstream stream;
            stream << VM_STRING("<FRAME type=compressed from=spliter order=")  << m_nFrame++ << VM_STRING(">\n")
                   << MFXStructureRef <mfxBitstream>(bs, 30).Serialize(Formater::SimpleString(0, VM_STRING("=")))
                   << VM_STRING("</FRAME>\n");
            
            PipelineTrace(( stream.str().c_str()));
        }

        return sts;
    }

};
