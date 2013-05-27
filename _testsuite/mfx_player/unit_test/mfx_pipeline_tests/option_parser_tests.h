/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

class SpecialItemSkipper : public IOptionParserVisitor
{
    vm_char * m_pBuf;
public:
    SpecialItemSkipper(vm_char * pBuf):m_pBuf (pBuf) 
    {
        m_pBuf[0]=0;
    }
    virtual bool visit_string(vm_char *ptr, vm_char * ptr_end)
    {
        vm_char value_at_ptr_end = *ptr_end;
        *ptr_end = 0;
        vm_string_strcat(m_pBuf, ptr);
        *ptr_end = value_at_ptr_end;
        return true;
    }
    virtual bool visit_special(SpecialItemType/* type*/)
    {
        return true;
    }
};

//not actually required 
class OptionCallbacVerifier : public IOptionParserVisitor
{
    int m_nDepth;
    bool m_bEOS;
public :
    OptionCallbacVerifier():m_nDepth(),m_bEOS(){}
    virtual bool visit_string(vm_char * /*ptr*/, vm_char * ptr_end)
    {
        if (0 == *ptr_end)
        {
            return (0 == m_nDepth);
        }
        return true;
    }

    virtual bool visit_special(SpecialItemType type)
    {
        switch(type)
        {
        case ITEM_SELECT:
            {
                break;
            }
        case ITEM_CALLBACK_START:
            {
                m_nDepth++;
                break;
            }
        case ITEM_CALLBACK_END:
            {
                if (0 == m_nDepth)
                    return false;
                m_nDepth--;
                break;
            }
        default:
            return false;
        }
        return true;
    }
};
