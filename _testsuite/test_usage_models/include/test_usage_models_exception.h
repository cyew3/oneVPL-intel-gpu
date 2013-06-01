//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010 Intel Corporation. All Rights Reserved.
//
//
//*/

#pragma once

#include "mfxdefs.h"
#include <sstream>

#if defined(_WIN32) || defined(_WIN64)
  #define MY_FUNCSIG __FUNCSIG__
#else
  #define MY_FUNCSIG __FUNCTION__
#endif

class TUMException: public std::exception
{
public:

    explicit
    TUMException(
        const std::string&  message,
        const char* const   fileName,
        const mfxU32        line,
        const char* const   funcName)
    {
        std::stringstream tmpStream;

        tmpStream << "Error: \"" << message << "\"";
        tmpStream << " (file \"" << fileName << ",";
        tmpStream << " line " << line << ")" << std::endl;
        tmpStream << "- in function \"" << funcName << "\";";

        m_Msg = tmpStream.str();
    }

    virtual ~TUMException() throw()
    {    // destroy the object
    }


    virtual const char* what() const throw()
    {
        return m_Msg.c_str();
    }

private:
    std::string     m_Msg;  // the stored message string
};
/* EOF */
