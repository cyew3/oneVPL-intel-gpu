/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_serializer.h"

//simple structure for testing
struct Items
    : std::vector<int>
{
};

template <>
class MFXStructureRef<Items>
    : public MFXStructureBase<Items>
{
public:
    MFXStructureRef(Items &refItem)
        : MFXStructureBase(refItem, 0)
    {

    }
    virtual void ConstructValues () const
    {
        for (size_t i = 0; i < m_pStruct->size(); i++)
        {
            vm_char name[2];
            vm_string_sprintf(name, VM_STRING("%d"), i);
            SerializeWithInserter(name ,(*m_pStruct)[i]);
        }
    }
};

class MockFormater
{
public:
    tstring operator ()(hash_array<tstring,tstring> & elements) const
    {
        tstringstream stream;
        hash_array<tstring, tstring>::iterator it;
        for(it = elements.begin(); it != elements.end(); it++)
        {
            stream<<it->first<<VM_STRING(":")<<it->second<<std::endl;
        }
        return stream.str();
    }
};

//contains last serialized name,value pair
class PairSerializer
    : public Formaters2::ISerializer
    , private mfx_no_copy
{
protected:
    std::pair<tstring, tstring> m_lastOperation;
public:
    PairSerializer()
    {
    }
    const tstring &type()
    {
        return m_lastOperation.first;
    }
    const tstring &value()
    {
        return m_lastOperation.second;
    }
protected:
    virtual void SerializeInternal(const tstring &name, const tstring &value)
    {
        m_lastOperation = std::make_pair(name, value);
    }
};



