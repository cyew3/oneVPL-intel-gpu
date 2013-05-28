/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_CMD_OPTION_PROCESSOR_H
#define __MFX_CMD_OPTION_PROCESSOR_H

#include "mfx_serial_core.h"

enum SpecialItemType
{
    ITEM_UNKNOWN,
    ITEM_CALLBACK_START,
    ITEM_CALLBACK_END,
    ITEM_QUANTIFICATOR_START,
    ITEM_QUANTIFICATOR_END,
    ITEM_SELECT
};

class IOptionParserVisitor
{

public:
    virtual ~IOptionParserVisitor() {};
    virtual bool visit_string(vm_char *ptr, vm_char * ptr_end) = 0;
    virtual bool visit_special(SpecialItemType type)   = 0;
};

class OptionPatternParser
{
public :
    static bool Parse(const vm_char *pattern, IOptionParserVisitor*);
};

class CmdOptionProcessor
{
    typedef std::map<tstring, std::list<tstring> > _CacheType;

    bool m_bPrint;
    bool m_bAdaptivePrinter;

    _CacheType m_cachedPatterns;

    //formating related constants
    static const int nOptionNameAlign  = 25;
    static const int nOptionNameOffset = 4;
    static const int nOptionParamAlign = 8;


public:

    CmdOptionProcessor(bool bPrintMode, bool bAdaptivePrint = true)
        : m_bPrint(bPrintMode)
        , m_bAdaptivePrinter(bAdaptivePrint){}
    virtual ~CmdOptionProcessor() {};

    virtual void    SetPrint(bool bPrintOption, bool bAdaptivePtinter = true);
    virtual bool    GetPrint();
    //TODO: return not value but order of matched pattern, zero otherwise
    virtual mfxU32  Check( vm_char *opt
                         , vm_char *pattern
                         , vm_char *description
                         , OptParamType param_type = OPT_UNDEFINED
                         , vm_char *opt_param = NULL
                         , SerialNode /*[in,out]*/ **pNode = NULL);

    virtual void    ClearCache();

protected:
    virtual void    PrintPatternHelp( vm_char *print_at
                                    , vm_char *pattern
                                    , bool bUseComas);
    virtual std::list<tstring> &  GetOptionsFromPattern(const tstring &ref_pattern);
};


//containes all possible options
class OptionsContainer : public IOptionParserVisitor
{
    typedef std::pair<vm_char*, vm_char*> string_ptr;

    struct _node;
    struct _node_list
    {
        typedef std::list<_node>::iterator iterator;
        std::list<_node>  nodes;
        _node_list * pParent;
        iterator     parentStart;
    };
    
    //only one group is available at the moment
    struct _node_any_list
    {
        typedef std::list<_node_list>::iterator iterator;
        std::list<_node_list> lst;
        _node_any_list   *pParent;
        _node_any_list()
            :  pParent()
        {}
    }m_rexpression, * m_pcurrent;
    
    struct _node
    {
        string_ptr      str;
        _node_any_list  child;
        _node(){}
        _node(string_ptr instr)
            :str(instr)
        {}
    };

    std::list<tstring> m_all_options;

public :

    OptionsContainer();
    virtual ~OptionsContainer() {};
    virtual bool visit_string(vm_char *ptr, vm_char * ptr_end);
    virtual bool visit_special(SpecialItemType type);
    virtual std::list<tstring> & get_all_options(){return m_all_options;}

protected:
    //tree nodes traverse support
    std::list<string_ptr>                       m_option_tokens;

    void build_all_options();
    void stich_tree(_node_any_list * pCurrent, 
                    _node_list     * pParent,
                    _node_list::iterator itNext);
    void traverse_expression(_node_any_list * pCurrent);
    void traverse_node_list( _node_list * pCurrent, _node_list::iterator it);
    void traverse_finish();
};

#endif//__MFX_CMD_OPTION_PROCESSOR_H
