//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//
//
//*/

#include "cmdline_parser.h"
#include "sample_defs.h"
#include <functional>
#include <iostream>

bool CmdLineParser::Parse(const msdk_string & params) { 
    msdk_string str_remained(params);
    msdk_string str_head;
    bool bHandlerFound = true;
    
    for (; bHandlerFound && !str_remained.empty(); )
    {
        size_t delimiter_pos = str_remained.find_first_of(MSDK_CHAR(' '));
        str_head = str_remained.substr(0, delimiter_pos);

        bHandlerFound = false;
        for (std::vector <OptionHandler*>::iterator i = m_options.begin(); i != m_options.end(); i++) {
            if ((*i)->GetName() == str_head) {
                m_handled[str_head] = *i;

                size_t nAccepted = (*i)->Handle(str_remained);
            
                if (!nAccepted) {
                    return false;
                }
                str_remained = str_remained.substr(nAccepted);
                bHandlerFound = true;
                break;
            }
        }
        if (!bHandlerFound) {
            MSDK_TRACE_ERROR(MSDK_STRING("Unknown option: ") << str_head);
        }
    }

    return bHandlerFound;
} 

//returns handler for particular option
const OptionHandler & CmdLineParser::at(const msdk_string & option_name) const {

    if (!IsPresent(option_name)) {
        if (!IsRegistered(option_name)) {
            MSDK_TRACE_ERROR( MSDK_STRING("CommandLineParser - option: ")<<option_name<<MSDK_STRING(" not registered"));
            throw NonRegisteredOptionError();
        }
        MSDK_TRACE_ERROR( MSDK_STRING("CommandLineParser - option: ")<<option_name<<MSDK_STRING(" not parsed"));
        throw NonParsedOptionError();
    }
    return *(m_handled.find(option_name)->second);
}
