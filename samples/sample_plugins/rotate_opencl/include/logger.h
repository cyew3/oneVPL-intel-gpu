/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel’s Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2005-2015 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#pragma once

#include <iostream>
#include "sample_utils.h"

///////////////////////////////////////////////////////////////////////////////
//! delegates operator << calls to underlying ostream with
//! on == true, and swallows the input with on == false
template <bool on = true>
class OnOffFilter
{
private:
    std::ostream *m_os;
public:
    OnOffFilter(std::ostream &os) : m_os(&os)
    {
    }

    //! delegates << to m_os
    template <typename Message>
    OnOffFilter &operator <<(const Message& message)
    {
        (*m_os) << message;
        return *this;
    }

    //! handles manipulators like endl, etc.
    typedef std::ostream& (*OstreamManipulator)(std::ostream&);
    OnOffFilter& operator<<(OstreamManipulator manip)
    {
        manip(*m_os);
        return *this;
    }
};

//! swallow message if on == false
template <>
template <typename Message>
OnOffFilter<false> &OnOffFilter<false>::operator <<(const Message& message)
{
    return *this;
}

//! swallow manip if on == false
template <>
inline OnOffFilter<false>& OnOffFilter<false>::operator<<(OnOffFilter<false>::OstreamManipulator manip)
{
    return *this;
}
                                                                           ////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//! available LogLevels
enum LogLevel
{
    None       = 0,
    Fatal      = 1,
    Error      = 2,
    Warning    = 3,
    Info       = 4,
    Debug      = 5,
};

////////////////////////////////////////////////////////////////////////////////
//! swallows messages with level higher than specified on object creation
template <LogLevel level = Info>
class Logger : private no_copy
{
private:
    std::ostream &m_os;

public:
    Logger(std::ostream &os) : m_os(os)
    {
    }

    OnOffFilter<level >= Fatal>   fatal()   { return OnOffFilter<level >= Fatal>  (m_os); }
    OnOffFilter<level >= Error>   error()   { return OnOffFilter<level >= Error>  (m_os); }
    OnOffFilter<level >= Warning> warning() { return OnOffFilter<level >= Warning>(m_os); }
    OnOffFilter<level >= Info>    info()    { return OnOffFilter<level >= Info>   (m_os); }
    OnOffFilter<level >= Debug>   debug()   { return OnOffFilter<level >= Debug>  (m_os); }
};

template <typename T>
std::string toString(const T& t)
{
    std::ostringstream oss;
    oss << t;
    return oss.str();
}