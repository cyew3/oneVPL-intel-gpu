/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_istring_printer.h"

class MockPrinter
    : public IStringPrinter
{
public:

    virtual void Print(const tstring & value)
    {
        Strings.push_back(value);
    }
    virtual void PrintLine(const tstring & value)
    {
        Lines.push_back(value);
    }

    std::vector<tstring> Lines;
    std::vector<tstring> Strings;

};

//contains a target printer and dispatch messages to it
//at destruction doesnt destroy target printer
class PrinterDispatcher
    : public IStringPrinter
{
    MockPrinter *m_pReceiver;
public:
    PrinterDispatcher(MockPrinter *pReceiver)
        : m_pReceiver(pReceiver)
    {
    }
    virtual void Print(const tstring & value)
    {
        m_pReceiver->Print(value);
    }
    virtual void PrintLine(const tstring & value)
    {
        m_pReceiver->PrintLine(value);
    }
};