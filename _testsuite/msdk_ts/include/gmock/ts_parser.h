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
            this->set_trace_level(0);
    }
    
    tsParser(mfxBitstream b, mfxU32 mode = 0)
        : m_sts(BS_ERR_NONE)
        , T(mode)
    {
        if(!g_tsTrace)
            this->set_trace_level(0);
        SetBuffer(b);
    }

    ~tsParser(){}

    typename T::UnitType* Parse(bool orDie = false)
    {
        TRACE_FUNC0(BS_parser::parse_next_unit);
        m_sts = this->parse_next_unit();
        if(m_sts && m_sts != BS_ERR_MORE_DATA || m_sts && orDie)
        {
            g_tsLog << "FAILED in tsParser\n";
            g_tsStatus.check(MFX_ERR_UNKNOWN);
        }
        return (typename T::UnitType* ) this->get_header();
    }

    typename T::UnitType& ParseOrDie()
    {
        return *Parse(true);
    }

    void SetBuffer(mfxBitstream b)
    {
        TRACE_FUNC2(BS_parser::set_buffer, b.Data + b.DataOffset, b.DataLength + 3);
        this->set_buffer(b.Data + b.DataOffset, b.DataLength + 3);
    }

};

typedef tsParser<BS_VP8_parser> tsParserVP8;
typedef tsParser<BS_HEVC_parser> tsParserHEVC;
typedef tsParser<BS_H264_parser> tsParserH264NALU;

class tsParserH264AU : public BS_H264_parser
{
public:
    typedef BS_H264_au UnitType;
    BSErr m_sts;

    tsParserH264AU(mfxU32 mode = 0)
        : m_sts(BS_ERR_NONE)
        , BS_H264_parser(mode)
    {
        if(!g_tsTrace)
            set_trace_level(0);
    }
    
    tsParserH264AU(mfxBitstream b, mfxU32 mode = 0)
        : m_sts(BS_ERR_NONE)
        , BS_H264_parser(mode)
    {
        if(!g_tsTrace)
            set_trace_level(0);
        SetBuffer(b);
    }

    ~tsParserH264AU(){}

    UnitType* Parse(bool orDie = false)
    {
        UnitType* pAU = 0;

        TRACE_FUNC0(BS_parser::parse_next_unit);
        m_sts = parse_next_au(pAU);
        if(m_sts && m_sts != BS_ERR_MORE_DATA || m_sts && orDie)
        {
            g_tsLog << "FAILED in tsParser\n";
            g_tsStatus.check(MFX_ERR_UNKNOWN);
        }
        return pAU;
    }

    UnitType& ParseOrDie()
    {
        return *Parse(true);
    }

    void SetBuffer(mfxBitstream b)
    {
        TRACE_FUNC2(BS_parser::set_buffer, b.Data + b.DataOffset, b.DataLength + 3);
        set_buffer(b.Data + b.DataOffset, b.DataLength + 3);
    }
};