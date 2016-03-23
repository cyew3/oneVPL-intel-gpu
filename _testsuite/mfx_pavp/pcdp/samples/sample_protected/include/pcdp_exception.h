//* ///////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2016 Intel Corporation. All Rights Reserved.
//
//
//*/

#pragma once

#include <windows.h>
#include <sstream>

class PCDPException
    :
    public std::exception
{
public:

    explicit
    PCDPException(
        const std::string&  message,
        const char* const   fileName,
        const DWORD         line,
        const char* const   funcName)
    {
        std::stringstream tmpStream;

        tmpStream << "Error: \"" << message << "\"";
        tmpStream << " (file \"" << fileName << "\",";
        tmpStream << " line " << line << ")" << std::endl;
        tmpStream << "- in function \"" << funcName << "\";";

        m_Msg = tmpStream.str();
    }

    // Destroy the object
    virtual
    ~PCDPException()
        throw()
    {
    }


    virtual const char*
    what()
        const throw()
    {
        return m_Msg.c_str();
    }

private:

    // The stored message string
    std::string  m_Msg;
};
