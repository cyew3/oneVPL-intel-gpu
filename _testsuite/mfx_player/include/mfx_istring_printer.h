/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

class IStringPrinter
{
public:
    virtual ~IStringPrinter(){}
    virtual void Print(const tstring & value) = 0;
    virtual void PrintLine(const tstring & value) = 0;
};

//default traceprinter

class ConsolePrinter : public IStringPrinter
{
public:
    virtual void Print(const tstring & value)
    {
        PipelineTrace((value.c_str()));
    }
    virtual void PrintLine(const tstring & value)
    {
        PipelineTrace((VM_STRING("%s\n"), value.c_str()));
    }
};