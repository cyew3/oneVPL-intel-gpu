/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */
#include "mfx_pipeline_defs.h"
#include "mfx_cmd_option_processor.h"
#include "mfx_serial_core.h"


using namespace std;

void    CmdOptionProcessor::SetPrint(bool bPrintOption, bool bAdaptivePrint)
{
    m_bPrint = bPrintOption;
    m_bAdaptivePrinter = bAdaptivePrint;
}

bool    CmdOptionProcessor::GetPrint()
{
    return m_bPrint;
}

mfxU32 CmdOptionProcessor::Check( const vm_char *opt
                                , const vm_char *pattern
                                , const vm_char *description
                                , OptParamType param_type
                                , const vm_char *opt_param
                                , SerialNode **ppInOutNode) 
{
    if (m_bPrint)
    {
        vm_char opt_name[512] = VM_STRING("");
        
        PrintPatternHelp(opt_name, pattern, true);

        if (m_bAdaptivePrinter && (vm_string_strlen(opt_name) > nOptionNameAlign + nOptionNameOffset && GetOptionsFromPattern(pattern).size() > 1))
        {
            opt_name[0]=0;
            PrintPatternHelp(opt_name, pattern, false);
        }
    
        if (!opt_param)
        {
            switch (param_type)
            {
                case OPT_TRI_STATE: {
                    opt_param = VM_STRING("on|off"); 
                    break;
                }
                case OPT_INT_32:
                case OPT_UINT_8:
                case OPT_UINT_16:
                case OPT_UINT_32:
                case OPT_UINT_64:{
                    opt_param = VM_STRING("integer");
                    break;
                }
                case OPT_64F:{
                    opt_param = VM_STRING("float");
                    break;
                }
                case OPT_FILENAME:{
                    opt_param = VM_STRING("filename");
                    break;
                }
                case OPT_BOOL:
                default:{
                    opt_param = VM_STRING("no arg");
                    break;
                }
            }
        }
        
        vm_string_printf(VM_STRING("%s (%-*s) %s\n"), opt_name, nOptionParamAlign, opt_param, description);

        return 0;
    }

    //auto deserialization completed by fully using appropriate class, we return OK here
    switch (param_type)
    {
        case OPT_AUTO_DESERIAL: 
        {
            int nParams; 
            return (*ppInOutNode)->_IsDeserialPossible(opt, pattern, nParams, *ppInOutNode);
        }
            
        default:
            break;
    }

    std::list<tstring>  options = GetOptionsFromPattern(pattern);
    std::list<tstring>::iterator it;

    mfxU32 i = 1;

    //eliminate spaces
    tstring in_str (opt);

    tstring::size_type stpos = in_str.find_first_not_of(VM_STRING(" "));
    tstring::size_type enpos = in_str.find_last_not_of(VM_STRING(" "));

    if (stpos == tstring::npos)
    {
        return 0;
    }
    
    tstring in_no_spaces = in_str.substr(stpos, enpos - stpos + 1);
    std::transform(in_no_spaces.begin(), in_no_spaces.end(), in_no_spaces.begin(), ::_totlower);
    
    for (it = options.begin(); it != options.end(); it++, i++)
    {
        
        stpos = it->find_first_not_of(VM_STRING(" "));
        enpos = it->find_last_not_of(VM_STRING(" "));

        tstring str = it->substr(stpos, enpos - stpos + 1);
        std::transform(str.begin(), str.end(), str.begin(), ::_totlower);

        if (str == in_no_spaces)
        {
            return i;
        }
    }

    return 0;
}

void CmdOptionProcessor::ClearCache()
{
    m_cachedPatterns.clear();
}

void  CmdOptionProcessor::PrintPatternHelp( vm_char *print_at
                                          , const vm_char *pattern 
                                          , bool bUseComas)
{
    std::list<tstring>  options = GetOptionsFromPattern(pattern);
    std::list<tstring>::iterator it;
    vm_char tmp_print_at[512] = {0};
    mfxU32 i = 0;

    for (it = options.begin(); !options.empty() && (i+1)!=options.size(); it++, i++)
    {
        if (bUseComas)
        {
            if (1 == i)
            {
                vm_string_strcat(tmp_print_at, VM_STRING("  ( "));
            }

            vm_string_strcat_s(tmp_print_at, MFX_ARRAY_SIZE(tmp_print_at), it->c_str());

            if (i > 0)
            {
                vm_string_strcat(tmp_print_at, VM_STRING(", "));
            }
        }
        else
        {
            vm_string_sprintf(print_at + vm_string_strlen(print_at), VM_STRING("%*s"),nOptionNameOffset," ");
            vm_string_sprintf(print_at + vm_string_strlen(print_at), VM_STRING("%s\n"), it->c_str());
        }
    }

    if (!options.empty())
    {
        if (bUseComas)
        {
            if (1 == i)
                vm_string_strcat(tmp_print_at, VM_STRING("  ( "));

            vm_string_strcat_s(tmp_print_at, MFX_ARRAY_SIZE(tmp_print_at), it->c_str());

            if (options.size() > 1)
                vm_string_strcat(tmp_print_at, VM_STRING(")"));
            
            vm_string_sprintf(print_at, VM_STRING("%*s"), nOptionNameOffset," ");
            vm_string_sprintf(print_at + vm_string_strlen(print_at),VM_STRING("%-*s"), nOptionNameAlign, tmp_print_at);
        }
        else
        {
            vm_string_sprintf(print_at + vm_string_strlen(print_at), VM_STRING("%*s"),nOptionNameOffset, " ");
            vm_string_sprintf(print_at + vm_string_strlen(print_at), VM_STRING("%-*s"),nOptionNameAlign, it->c_str());
        }
    }else
    {
        vm_string_sprintf(print_at, VM_STRING("%*s"),nOptionNameAlign, "");
    }
}

std::list<tstring> &  CmdOptionProcessor::GetOptionsFromPattern(const tstring &ref_pattern)
{
    _CacheType::iterator it;
    if (m_cachedPatterns.end() == (it = m_cachedPatterns.find(ref_pattern)))
    {
        OptionsContainer storage;
        if (OptionPatternParser::Parse(ref_pattern.c_str(), &storage))
        {
            
            std::pair<_CacheType::iterator, bool>  it_bool = m_cachedPatterns.insert(_CacheType::value_type(ref_pattern, storage.get_all_options()));
            it = it_bool.first;
        }
    }
    return it->second;
}



bool  OptionPatternParser::Parse( const vm_char *cpattern
                                , IOptionParserVisitor* pVisitor)
{
    if (!cpattern || !pVisitor)
        return false;

    //////////////////////////////////////////////////////////////////////////
    //simple regexp parser  - only supports multiple cases with operator |
    //and subexpessions with()
    int i;
    vm_char *pattern = const_cast<vm_char *>(cpattern);
    vm_char *pattern_start = pattern;
    
    for (i =0; pattern[i] != 0; i++)
    {
        SpecialItemType item = ITEM_UNKNOWN;
        
        switch(pattern[i])
        {
            case VM_STRING('(') : 
            {
                item = ITEM_CALLBACK_START;
                break;
            }
            case VM_STRING(')') : 
            {
                item = ITEM_CALLBACK_END;
                break;
            }
            case VM_STRING('|') : 
            {
                item = ITEM_SELECT;
                break;
            }
            case VM_STRING('{') : 
            {
                item = ITEM_QUANTIFICATOR_START;
                break;
            }
            case VM_STRING('}') : 
            {
                item = ITEM_QUANTIFICATOR_END;
                break;
            }        
        }
        if (item != ITEM_UNKNOWN)
        {
            if (i + pattern != pattern_start)
            {
                if(!pVisitor->visit_string(pattern_start, pattern + i ))
                    return false;
            }

            pattern_start = pattern + i + 1;

            if (!pVisitor->visit_special(item))
                return false;
        }
    }
    return pVisitor->visit_string(pattern_start, pattern + i);
}

OptionsContainer::OptionsContainer()
    : m_pcurrent()
{
}


bool OptionsContainer::visit_string(vm_char *ptr, vm_char * ptr_end)
{
    if (NULL == m_pcurrent)
        m_pcurrent = &m_rexpression;
    
    if (ptr != ptr_end)
    {
        if (m_pcurrent->lst.empty())
            m_pcurrent->lst.push_back(_node_list());

        m_pcurrent->lst.back().nodes.push_back(_node(string_ptr(ptr, ptr_end)));
    }

    if (0 == *ptr_end)
    {
        //removing empty list
        if (m_pcurrent !=0)
        {
            if (!m_pcurrent->lst.empty())
            {
                if (m_pcurrent->lst.back().nodes.empty())
                {
                    m_pcurrent->lst.pop_back();
                }
            }
        }
        //building a set of possible strings
        build_all_options();
    }

    return true;
}

bool OptionsContainer::visit_special(SpecialItemType type)
{
    switch(type)
    {
        case ITEM_SELECT:
        {
            if (NULL == m_pcurrent)
                m_pcurrent = &m_rexpression;

            if (!m_pcurrent->lst.empty())
            {
                m_pcurrent->lst.push_back(_node_list());
            }
            break;
        }
        case ITEM_CALLBACK_START:
        {
            if (NULL == m_pcurrent)
                m_pcurrent = &m_rexpression;

            if (m_pcurrent->lst.empty())
                m_pcurrent->lst.push_back(_node_list());

            m_pcurrent->lst.back().nodes.push_back(_node());
            m_pcurrent->lst.back().nodes.back().child.pParent = m_pcurrent;
            m_pcurrent = &m_pcurrent->lst.back().nodes.back().child;

            break;
        }
        case ITEM_CALLBACK_END:
        {
            //removing empty list
            if (!m_pcurrent->lst.empty())
            {
                if (m_pcurrent->lst.back().nodes.empty())
                {
                    m_pcurrent->lst.pop_back();
                }
            }

            if (m_pcurrent->lst.empty())
            {
                m_pcurrent = m_pcurrent->pParent;
                m_pcurrent->lst.pop_back();
            }else
            {
                m_pcurrent = m_pcurrent->pParent;
            }
            break;
        }
        case ITEM_QUANTIFICATOR_START:
        {
            break;
        }
        case ITEM_QUANTIFICATOR_END:
        {
            break;
        }
        default:
            return false;
    }
    return true; 
}

void OptionsContainer::build_all_options()
{
    if (m_rexpression.lst.empty() || m_rexpression.lst.front().nodes.empty())
        return;

    stich_tree(&m_rexpression, NULL, _node_list::iterator());
    traverse_expression(&m_rexpression);
}

void OptionsContainer::stich_tree(_node_any_list * pCurrent, 
                                  _node_list     * pParent,
                                  _node_list::iterator itNext)
{
    std::list<_node_list>::iterator it;
    _node_list::iterator node_it;

    for (it = pCurrent->lst.begin(); it != pCurrent->lst.end(); it++)
    {
        it->parentStart = itNext;
        it->pParent     = pParent;
        for (node_it = it->nodes.begin(); node_it != it->nodes.end(); node_it ++)
        {
            if (!node_it->child.lst.empty())
            {
                _node_list::iterator node_it_next = node_it;
                stich_tree(&node_it->child, &*it, ++node_it_next);
            }
        }
    }
}

void OptionsContainer::traverse_expression(_node_any_list * pCurrent)
{
    std::list<_node_list>::iterator it;

    for (it = pCurrent->lst.begin(); it != pCurrent->lst.end(); it++)
    {
        traverse_node_list(&*it, it->nodes.begin());
    }
}

void OptionsContainer::traverse_node_list( _node_list * pCurrent, _node_list::iterator it_start)
{
    if (NULL == pCurrent)
    {
        return traverse_finish();
    }

    int nShouldPop   = 0;
    bool bIsLeave    = true;

    for (_node_list::iterator it = it_start; it != pCurrent->nodes.end(); it++)
    {
        if (it->str.first != it->str.second)
        {
            //there is a string token, not down level list token
            m_option_tokens.push_back(it->str);
            nShouldPop++;
        }else
        {
            //visiting expression in brackets
            traverse_expression(&it->child);
            bIsLeave = false;
            break;
        }
    }
    //need to continue with parent node_list, if no downstream
    if (bIsLeave)
        traverse_node_list(pCurrent->pParent, pCurrent->parentStart);

    for(;nShouldPop;nShouldPop--)
        m_option_tokens.pop_back();
}

void OptionsContainer::traverse_finish()
{
    std::list<string_ptr>::iterator it;
    tstring str;
    for (it = m_option_tokens.begin(); it != m_option_tokens.end(); it++)
    {
        vm_char end_value = *(it->second);
        *it->second = 0;
        str.append(it->first);
        *it->second = end_value;
    }
    if (!str.empty())
        m_all_options.push_back(str);
}
