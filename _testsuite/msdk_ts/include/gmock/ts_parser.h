#pragma once

#include "ts_common.h"
#include "bs_parser++.h"


template<class T> class tsParser : public T
{
public:
    BSErr m_sts;

    tsParser(mfxU32 mode = 0)
        : m_sts(BS_ERR_NONE)
        , T(mode)
    {
        if(!g_tsTrace)
            set_trace_level(0);
    }
    
    tsParser(mfxBitstream b, mfxU32 mode = 0)
        : m_sts(BS_ERR_NONE)
        , T(mode)
    {
        if(!g_tsTrace)
            set_trace_level(0);
        SetBuffer(b);
    }

    ~tsParser(){}

    typename T::UnitType* Parse(bool orDie = false)
    {
        TRACE_FUNC0(BS_parser::parse_next_unit);
        m_sts = parse_next_unit();
        if(m_sts && m_sts != BS_ERR_MORE_DATA || m_sts && orDie)
        {
            g_tsLog << "FAILED in tsParser\n";
            g_tsStatus.check(MFX_ERR_UNKNOWN);
        }
        return (T::UnitType*) get_header();
    }

    typename T::UnitType& ParseOrDie()
    {
        return *Parse(true);
    }

    void SetBuffer(mfxBitstream b)
    {
        TRACE_FUNC2(BS_parser::set_buffer, b.Data + b.DataOffset, b.DataLength + 3);
        set_buffer(b.Data + b.DataOffset, b.DataLength + 3);
    }

};

typedef tsParser<BS_VP8_parser> tsParserVP8;