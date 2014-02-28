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

#pragma once

#include "sample_utils.h"
#include "option_handler.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include <map>

struct LastType {
};

template<class T = LastType, class TNext = LastType>
struct OptionsContainer {
    T first;
    TNext second;
    OptionsContainer() {}
    OptionsContainer (const T& element) 
        : first(element) {
    }
    OptionsContainer (const T& element, const TNext &next) 
        : first(element)
        , second(next) {
    }
    OptionsContainer(const OptionsContainer &that) 
        : first(that.first)
        , second(that.second) {

    }
    template <class TOption>
    OptionsContainer<TOption, OptionsContainer<T, TNext> > operator ()  (const TOption & hdl) const{
        OptionsContainer<TOption, OptionsContainer<T, TNext> > container(hdl, *this);
        return container;
    }
    
    template <class TOption>
    OptionsContainer<TOption> AddOption (const TOption& opt) const {
        OptionsContainer<TOption> cnt(opt);
        return cnt;
    }
};

typedef OptionsContainer<> Options;

//parser exceptions
class NonRegisteredOptionError {
};

class NonParsedOptionError {
};

class CmdLineParser {
protected:
    std::vector <OptionHandler*> m_options;
    std::map<msdk_string, OptionHandler*> m_handled;
    msdk_string m_sHelpHead;
    msdk_string m_sUsageExamples;
public:
    CmdLineParser(msdk_string HelpHead, msdk_string UsageExamples) : 
        m_sHelpHead(HelpHead)
        ,m_sUsageExamples(UsageExamples) {

    }
    virtual ~CmdLineParser() {
        m_handled.clear();
        std::transform(m_options.begin(), m_options.end(), m_options.begin(), DeletePtr());
    }

    template <class T>
    void RegisterOption(std::auto_ptr<T> & hdl) {
        m_options.push_back(hdl.get());
        hdl.release();
    }

    //space delimited parser
    virtual bool Parse(const msdk_string & params);

    virtual void PrintHelp(msdk_ostream & os) const {
        size_t maxSize = 0;
        
        for (size_t j = 0; j < m_options.size(); j++) {
            maxSize = (std::max)(m_options[j]->GetName().size(), maxSize);
        }
        os<<m_sHelpHead<<std::endl;
        for (size_t j = 0; j < m_options.size(); j++) {
            m_options[j]->PrintHelp(os, maxSize);
            os<<std::endl;
        }
        os<<m_sUsageExamples<<std::endl;
    }
    //option was parsed 
    virtual bool IsPresent(const msdk_string & option) const {
        return m_handled.find(option) != m_handled.end();
    }

    const OptionHandler & operator[](const msdk_string & option_name) {
        return at(option_name);
    }

    //returns handler for particular option
    virtual const OptionHandler & at(const msdk_string & option_name) const;

protected:

    bool IsRegistered(const msdk_string & option) const {
        for (size_t j =0; j != m_options.size(); j++) {
            if (m_options[j]->GetName() == option) {
                return true;
            }
        }
        return false;
    }

};

template <class Options_t>
class CmdLineParserTmpl : public CmdLineParser {
    Options_t m_options2;

public:
    CmdLineParserTmpl(const Options_t &opts, msdk_string HelpHead,  msdk_string UsageExamples)
        : CmdLineParser(HelpHead, UsageExamples), m_options2 (opts) {
        RegisterAll(m_options2);
    }
    virtual ~CmdLineParserTmpl() {
        m_handled.clear();
        m_options.clear();
    }
    template <class T, class TNext> void RegisterAll(OptionsContainer<T, TNext>& cnt) {
        m_options.push_back(&cnt.first);
        RegisterAll(cnt.second);
    }
    
    template <class T> void RegisterAll(OptionsContainer<T, LastType>& cnt) {
        m_options.push_back(&cnt.first);
    }

    void RegisterAll(OptionsContainer<LastType, LastType>& /*cnt*/) {
    }
};

template <class T> 
CmdLineParserTmpl<T>* CreateCmdLineParser(const T&options_description, msdk_string HelpHead,  msdk_string UsageExamples) {
    CmdLineParserTmpl<T> *parser = new CmdLineParserTmpl<T>(options_description, HelpHead, UsageExamples);
    return parser;
}
