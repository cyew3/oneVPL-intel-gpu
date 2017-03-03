/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

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
