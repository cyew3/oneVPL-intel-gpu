/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_pipeline_types.h"
#include "mfx_lexical_cast.h"
#include "mfx_serial_formater.h"
#include "mfx_serial_collection.h"
#include <set>

// internal, mostly necessary to avoid code duplication for templated definition and base ctor call
template <class T>
struct SerialTypeTrait
{
    typedef T value_type;
};


//data used for creating instance of structurebuilder in array elements serialisation
class NullData
{
};

//serializable primitive
class SerialNode
{
protected:
    tstring m_name;
public:
    typedef SerialSingleElement<NullData> init_type;

    virtual ~SerialNode(){}
    SerialNode(const tstring & name)
        : m_name (name)
    {
    }
    template <class Serializer>
    void Serialize(const Serializer & nonConstantSerializer) 
    {
        _Serialize(&const_cast<Serializer&>(nonConstantSerializer), VM_STRING(""));
    }
    
    //return NodeThat accepts arguments
    bool IsDeserialPossible(const tstring & key, int &nParamsRequired, SerialNode *&pNode)
    {
        return _IsDeserialPossible(key, VM_STRING(""), nParamsRequired, pNode);
    }

    //deserialisation
    virtual void Parse(const tstring & parameters) = 0;


    //virtual version
    virtual void _Serialize(Formaters2::ISerializer * pSerial, const tstring & prefix) = 0;
    
    //true if value could be partially deserialized using provided string
    //i.e. in serialisation of this node there could be string like input one
    //if possible returns also number of additional parameters required for deserialization, default is 1
    virtual bool _IsDeserialPossible(const tstring & key, const tstring & prefix, int &nParamsRequired, SerialNode *&pNode) = 0;


};

//format type for serialization of decimal values
template <class T>
class DecimalFormater
{
public:
    typedef T ret_type;
};

template <>
class DecimalFormater<mfxU8>
{
public:
    typedef mfxU16 ret_type;
};

template <>
class DecimalFormater<mfxI8>
{
public:
    typedef mfxI16 ret_type;
};

template <class T, class Formater>
typename Formater::ret_type FormatValue(T value)
{
    return static_cast<typename Formater::ret_type>(value);
}


template <class T>
class PODNode
    : public SerialNode
{
protected:
    T  &m_value;
public:

    typedef T element_type;
    typedef SerialNode::init_type init_type;

    PODNode(const tstring & name, T & value, const init_type &aux = init_type())
        : SerialNode(name)
        , m_value(value)
    {
        aux;
    }
    PODNode(const PODNode & that)
        : SerialNode(that.m_name)
        , m_value(that.m_value)
    {
    }
    using SerialNode::_Serialize;
    using SerialNode::_IsDeserialPossible;


    virtual void _Serialize(Formaters2::ISerializer * pSerial, const tstring & prefix)
    {
        pSerial->Serialize(prefix + m_name, FormatValue<T, DecimalFormater<T> >(m_value));
    }
    virtual bool _IsDeserialPossible(const tstring & key, const tstring & prefix, int &nParamsRequired, SerialNode *&pNode)
    {
        tstringstream sstream;
        sstream << prefix << m_name;
        tstring streamstr = sstream.str();
        tstring skey      = key;

        transform(streamstr.begin(), streamstr.end(), streamstr.begin(), ::_totlower);
        transform(skey.begin(), skey.end(), skey.begin(), ::_totlower);

        if (skey == streamstr)
        {
            //TODO: 1 for now, bool is possible
            nParamsRequired = 1;
            pNode = this;
            return true;
        }
        return false;
    }
    virtual void Parse(const tstring & parameters)
    {
        lexical_cast(parameters, m_value);
    }
    PODNode& operator = (const PODNode & /*that*/)
    {
        return *this;
    }
};

template <class T>
PODNode<T> make_pod_node(const tstring & name, T & obj)
{
    PODNode<T> node(name, obj);
    return node;
}

//node builder must provide type trait in element_type that it builds
template<class NodeBuilder, int nElem, class ElementCountType = int>
class ArrayNode : public SerialNode, private mfx_no_assign
{
public:
    typedef typename NodeBuilder::element_type element_type;
    typedef typename NodeBuilder::init_type init_type;

private:
    element_type               *m_pElements;
    //if element counter identificator present we will use it in serialization
    ElementCountType           *m_nElements;
    SerialCollection<init_type>& m_dataForBuilder;
    SerialCollection<init_type> m_LocalDataForBuilder;
    bool                        m_bOwnData;

    //class used to store indexes for returned nodes as result for isdeserialpossible function
    //to be used in followed call to SerialNode->Parse()
    //TODO: currently it required to passes of finding appropriate node.
    friend class SerialNodeImpl;
    class SerialNodeImpl
        : public SerialNode
    {
        ArrayNode *m_pOwner;
        //string used to locate necessary node
        tstring m_key;
        tstring m_prefix;

    public:
        SerialNodeImpl(const tstring & key = tstring(), const tstring & prefix = tstring(), ArrayNode *pOwner = NULL)
            : SerialNode(VM_STRING(""))
            , m_pOwner(pOwner)
            , m_key(key)
            , m_prefix(prefix)
        {
        }
        bool isEqual(const tstring & key, const tstring & prefix)
        {
            return m_key == key && m_prefix == prefix;
        }
        virtual void _Serialize(Formaters2::ISerializer * /*pSerial*/, const tstring & /*prefix*/)
        {
            throw tstring(VM_STRING("Not Implemented"));
        }
        virtual bool _IsDeserialPossible(const tstring & /*key*/, const tstring & /*prefix*/, int &/*nParamsRequired*/, SerialNode *& /*pNode*/)
        {
            throw tstring(VM_STRING("Not Implemented"));
        }
        virtual void Parse(const tstring & parameters)
        {
            return m_pOwner->ParseInternal(m_key, m_prefix, parameters);
        }
    };
    
    std::list<SerialNodeImpl> m_parsedNodes;

    //functor's family that invokes with result of located node
    friend class PossibleDeserialization;
    class PossibleDeserialization
    {
        ArrayNode *m_pOwner;
    public:
        PossibleDeserialization(ArrayNode *pOwner)
            : m_pOwner(pOwner)
        {
        }
        void operator ()(const tstring & key, const tstring & prefix, SerialNode *&pNode, ElementCountType /*position*/)
        {
            //create permanent pointer to particular node
            m_pOwner->MakeExternalNode(key, prefix, pNode);
        }
    };

    class ParseFunctor
    {
        tstring m_params;
        ElementCountType *m_pPosition;
    public:
        ParseFunctor(const tstring &params, ElementCountType &ref_position)
            :  m_params(params)
            ,  m_pPosition(&ref_position)
        {
        }
        void operator ()(const tstring & /*key*/, const tstring & /*prefix*/, SerialNode *&pNode, ElementCountType position)
        {
            pNode->Parse(m_params);
            //updated if parsing ok
            *m_pPosition = position;
        }
    };

public:

    ArrayNode( const tstring &name
             , element_type(&elements)[nElem]
             , SerialCollection<init_type> & data
             , bool )
        : SerialNode(name)
        , m_nElements(NULL)
        , m_dataForBuilder(data)
        , m_pElements(elements)
        , m_bOwnData(false)
    {
    }
    ArrayNode( const tstring &name
             , element_type(&elements)[nElem]
             , ElementCountType &size
             , SerialCollection<init_type> & data
             , bool)
        : SerialNode(name)
        , m_nElements(& size)
        , m_dataForBuilder(data)
        , m_pElements(elements)
        , m_bOwnData(false)
    {
    }
    ArrayNode( const tstring &name
             , element_type(&elements)[nElem]
             , const SerialCollection<init_type> & data = SerialCollection<init_type>())
        : SerialNode(name)
        , m_nElements(NULL)
        , m_LocalDataForBuilder(data)
        , m_dataForBuilder(m_LocalDataForBuilder)
        , m_pElements(elements)
        , m_bOwnData(true)
    {
    }
    ArrayNode( const tstring &name
             , element_type(&elements)[nElem]
             , ElementCountType &size
             , const SerialCollection<init_type> & data = SerialCollection<init_type>())
        : SerialNode(name)
        , m_nElements(& size)
        , m_LocalDataForBuilder(data)
        , m_dataForBuilder(m_LocalDataForBuilder)
        , m_pElements(elements)
        , m_bOwnData(true)
    {
    }
    
    //copy ctor
    ArrayNode(const ArrayNode &that)
        : SerialNode(that.m_name)
        , m_nElements(that.m_nElements)
        , m_LocalDataForBuilder(that.m_LocalDataForBuilder)
        , m_dataForBuilder(that.m_bOwnData ? m_LocalDataForBuilder : that.m_dataForBuilder)
        , m_pElements(that.m_pElements)
        , m_bOwnData(that.m_bOwnData)
    {
    }

    using SerialNode::_Serialize;
    using SerialNode::_IsDeserialPossible;

    virtual void _Serialize(Formaters2::ISerializer * pSerial, const tstring & pref)
    {
        //restrict output number
        int nElements = nElem;
        if (m_nElements)
            nElements = *m_nElements;

        m_dataForBuilder.resize(nElements);
        for (int i = 0; i < nElements; i++)
        {
            tstringstream strstreamPref;
            strstreamPref << pref << m_name<<VM_STRING("[") << i << VM_STRING("]");

            NodeBuilder node(strstreamPref.str(), m_pElements[i], m_dataForBuilder[i]);
            node._Serialize(pSerial, VM_STRING(""));
        }
    }

    virtual void Parse(const tstring & /*parameters*/)
    {
        throw tstring(VM_STRING("Parsing of Arrays not implemented"));
    }

    virtual bool _IsDeserialPossible(const tstring & key, const tstring & prefix, int & nParamsRequired, SerialNode *&pNode)
    {
        PossibleDeserialization possibleSerial(this);
        return FindNode(key, prefix, nParamsRequired, pNode, possibleSerial);
    }
private:

    void MakeExternalNode(const tstring & key, const tstring & prefix, SerialNode *& pNode)
    {
        typename std::list<SerialNodeImpl>::iterator i;
        for (i = m_parsedNodes.begin(); i != m_parsedNodes.end(); i++)
        {
            if (i->isEqual(key, prefix))
            {
                pNode = &*i;
                return;
            }
        }

        m_parsedNodes.push_back(SerialNodeImpl(key, prefix, this));
        pNode = &m_parsedNodes.back();
    }

    void ParseInternal(const tstring & key, const tstring & prefix, const tstring & parameters)
    {
        ElementCountType position;
        ParseFunctor fn(parameters, position);
        SerialNode *pTmpNode;
        int nParams;

        FindNode(key, prefix, nParams, pTmpNode, fn);

        if (NULL != m_nElements)
        {
            *m_nElements = (std::max)(*m_nElements, ++position );
        }
        //if parsing succeed lets set current array len
    }

    template <class functor>
    bool FindNode(const tstring & key, const tstring & prefix, int & nParamsRequired, SerialNode *&pNode, functor fn )
    {
        //detecting index in array
        tstring s = prefix + m_name;
        tstring nckey = key;

        transform(s.begin(), s.end(), s.begin(), ::_totlower);
        transform(key.begin(), key.end(), nckey.begin(), ::_totlower);

        size_t pos = nckey.find(s);
        if (tstring::npos == pos || 0 != pos)
            return false;
        
        tstringstream stream (key.substr(s.size()));
        
        vm_char a[3] = {0};
        ElementCountType position;
        stream>>a[0];
        stream>>position;
        stream>>a[1];

        if (!stream.good() && !(stream.eof()))
        {
            return false;
        }
        if (a[0] != VM_STRING('[') || a[1] != VM_STRING(']'))
        {
            return false;
        }
        //check boundaries
        if (position >= nElem)
        {
            return false;
        }

        tstringstream stream2;
        stream>>stream2.rdbuf();

        //resize data if necessary
        m_dataForBuilder.resize(position + 1);
        //continue check with trimmered data
        NodeBuilder node(VM_STRING(""), m_pElements[position], m_dataForBuilder[position]);

        if (!node._IsDeserialPossible(stream2.str(), VM_STRING(""), nParamsRequired, pNode))
            return false;

        fn(key, prefix, pNode, position);
            
        return true;
    }

};

//////////////////////////////////////////////////////////////////////////
class LastNodeInList
{
};

//compiletime version of list
template <class L, class R = LastNodeInList>
class ListNode
    : public SerialNode
{
protected:
    L m_obj1;
    R m_obj2;
public:

    typedef SerialNode::init_type init_type ;

    ListNode(const tstring &name, const L & obj1, const R & obj2)
        : SerialNode(name)
        , m_obj1(obj1)
        , m_obj2(obj2)
    {
    }

    using SerialNode::_Serialize;
    using SerialNode::_IsDeserialPossible;

    virtual void _Serialize(Formaters2::ISerializer * pSerial, const tstring & pref)
    {
        //tstringstream strstream;

        //strstream<<
        m_obj1._Serialize(pSerial, pref + m_name);
        //if object serialized it self somehow we need to add new line
        //if (!strstream.str().empty())
          //  strstream<<std::endl;
        //strstream<<
        m_obj2._Serialize(pSerial, pref + m_name);

        //return strstream.str();
    }
    virtual bool _IsDeserialPossible(const tstring & key, const tstring & prefix, int &nParamsRequired, SerialNode *&pNode )
    {
        if (m_obj1._IsDeserialPossible(key, prefix + m_name, nParamsRequired, pNode))
            return true;

        return m_obj2._IsDeserialPossible(key, prefix + m_name, nParamsRequired, pNode);
    }
    virtual void Parse(const tstring & /*parameters*/)
    {
        throw tstring(VM_STRING("Parsing of Lists not implemented"));
    }

};

template <class L>
class ListNode<L, LastNodeInList>
    : public SerialNode
{
protected:
    L m_obj1;
public:
    ListNode(const tstring &name, const L & obj1, const LastNodeInList & /*obj2*/)
        : SerialNode(name)
        , m_obj1(obj1)
    {
    }

    using SerialNode::_Serialize;
    using SerialNode::_IsDeserialPossible;

    virtual void _Serialize(Formaters2::ISerializer * pSerial, const tstring & pref)
    {
        m_obj1._Serialize(pSerial, pref + m_name);
    }
    virtual bool _IsDeserialPossible(const tstring & key, const tstring & prefix, int &nParamsRequired , SerialNode *&pNode)
    {
        return m_obj1._IsDeserialPossible(key, prefix + m_name, nParamsRequired, pNode);
    }
    virtual void Parse(const tstring & /*parameters*/)
    {
        throw tstring(VM_STRING("Parsing of Lists not implemented"));
    }

};

template <class L, class R>
ListNode<L, R> make_list_node(const L& l, const R&r)
{
    ListNode<L, R> node (VM_STRING(""), l, r);
    return node;
}

template <class L>
ListNode<L, LastNodeInList> make_list_node(const L& l)
{
    ListNode<L, LastNodeInList> node (VM_STRING(""), l, LastNodeInList());
    return node;
}

//condition require to use certain value, mostly for serializing 
//condition may be a part of structure or may be not a part 
template <class Fn, class TParent>
class ConditionalNode : public TParent
{
    //functor specified externally
    Fn m_isSerial;
    tstring m_name;
public:
    //typename typedef TParent element_type;
    //typename typedef TParent::init_type init_type;
    typedef TParent element_type;
    typedef typename TParent::init_type init_type;

    ConditionalNode (const tstring &name, const Fn& isSerial, const TParent & obj)
        : TParent(obj)//copy ctor
        , m_isSerial(isSerial)
        , m_name(name)
    {
    }

    using TParent::_Serialize;
    using TParent::_IsDeserialPossible;

    virtual void _Serialize(Formaters2::ISerializer * pSerial, const tstring & pref)
    {
        if (m_isSerial())
        {
            TParent::_Serialize(pSerial, pref + m_name);
        }

        //return VM_STRING("");
    }
    virtual bool _IsDeserialPossible(const tstring & key, const tstring & prefix, int &nParamsRequired , SerialNode *&pNode)
    {
        return TParent::_IsDeserialPossible(key, m_name+prefix, nParamsRequired, pNode);
    }
};

template <class Fn, class TParent>
ConditionalNode<Fn, TParent> make_conditional_node( const tstring & name
                                                  , const Fn& isSerial
                                                  , const TParent& obj)
{
    ConditionalNode<Fn, TParent> cond(name, isSerial, obj);
    return cond;
}

//version of equal to that may use overloaded == operator
template<class _Tx, class _Ty>
struct equal_to_ext
    : public std::binary_function<_Tx, _Ty, bool>
{   // functor for operator==
    bool operator()(const _Tx& _Left, const _Ty& _Right) const
    {
        return (_Left == _Right);
    }
};

//several useful switches for conditional Node
template <class T, class T1 = T, class Functor = equal_to_ext<T,T1> >
class Switcher
    : private mfx_no_assign
{
    T  &m_Actual;
    T1  m_OnValue;
    Functor m_fn;
public:

    Switcher(T &actual, T1 onValue, Functor fn = Functor())
        : m_Actual(actual)
        , m_OnValue(onValue)
        , m_fn(fn)
    {
    }
    Switcher(const Switcher &that)
        : m_Actual(that.m_Actual)
        , m_OnValue(that.m_OnValue)
    {
    }
    bool operator ()()
    {
        return m_fn(m_Actual, m_OnValue);
    }
};

template <class T, class T1>
Switcher<T, T1, equal_to_ext<T, T1> > make_equal_switch(T & obj, const T1 & reference_val)
{
    Switcher<T, T1, equal_to_ext<T, T1> > eq(obj, reference_val, equal_to_ext<T, T1>());
    return eq;
}


template<class _Ty>
struct set_ext
    : public std::binary_function<_Ty, _Ty, bool>
{
private:
    std::set<_Ty>m_set;
public:
    set_ext(){ 
    }
    set_ext(const _Ty &e1){ 
        m_set.insert(e1); 
    }
    set_ext(const _Ty &e1, const _Ty &e2)    
    { 
        m_set.insert(e1); 
        m_set.insert(e2); 
    }
    set_ext(const _Ty &e1, const _Ty &e2, const _Ty &e3)    
    { 
        m_set.insert(e1); 
        m_set.insert(e2); 
        m_set.insert(e3); 
    }
    
    //element in set
    bool operator == (const _Ty& _Left) const
    {
        return m_set.find(_Left) != m_set.end();
    }
};

template <class _Ty>
bool operator == (const _Ty& _Left, const set_ext<_Ty> & _Right) 
{
    return _Right.operator ==(_Left);
}

template <class _Ty>
set_ext<_Ty> make_set_ex(const _Ty &e1)
{
    return set_ext<_Ty>(e1);
}
template <class _Ty>
set_ext<_Ty> make_set_ex(const _Ty &e1, const _Ty &e2)
{
    return set_ext<_Ty>(e1, e2);
}
template <class _Ty>
set_ext<_Ty> make_set_ex(const _Ty &e1, const _Ty &e2, const _Ty &e3)
{
    return set_ext<_Ty>(e1, e2, e3);
}
