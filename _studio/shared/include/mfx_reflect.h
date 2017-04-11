//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//
#pragma once

#include <memory>
#include <string>
#include <string.h> //for memcmp on Linux
#include <list>
#include <vector>
#include <map>
#include <stdexcept>
#include "mfx_config.h"

#if defined(MFX_HAS_CPP11)
#include <typeindex>
#else
#include "mfx_cpp11_replacement.h"
#endif

namespace mfx_reflect
{

#if defined(MFX_HAS_CPP11)
    typedef ::std::type_index TypeIndex;
    namespace mfx_cpp11
    {
        template <class T>
        using shared_ptr = ::std::shared_ptr<T>;
    }
#else
    typedef mfx_cpp11::type_index TypeIndex;
#endif

#if defined(MFX_HAS_CPP11)
    namespace mfx_cpp11
    {
        template <class T>
        inline bool is_pointer()
        {
            return ::std::is_pointer<T>::value;
        }
    }
#endif

    class ReflectedField;
    class ReflectedType;
    class ReflectedTypesCollection;

    class ReflectedField
    {
    public:
        typedef mfx_cpp11::shared_ptr<ReflectedField> P;
        typedef ::std::vector<ReflectedField::P> FieldsCollection;
        typedef FieldsCollection::const_iterator FieldsCollectionCI;


        mfx_cpp11::shared_ptr<ReflectedType>  FieldType;
        mfx_cpp11::shared_ptr<ReflectedType>  AggregatingType;
        const ::std::string &             FieldTypeName; //single C++ type may have several typedef aliases. This is original name that was used for this field declaration.
        size_t                          Offset;
        const ::std::string               FieldName;
        size_t                          Count;
        ReflectedTypesCollection *      m_pCollection;

        void * GetAddress(void *pBase)
        {
            return ((unsigned char*)pBase) + Offset;
        }


    protected:
        ReflectedField(ReflectedTypesCollection *pCollection, mfx_cpp11::shared_ptr<ReflectedType> aggregatingType, mfx_cpp11::shared_ptr<ReflectedType> fieldType, const ::std::string &fieldTypeName, size_t offset, const ::std::string fieldName, size_t count)
            : FieldType(fieldType)
            , AggregatingType(aggregatingType)
            , FieldTypeName(fieldTypeName)
            , Offset(offset)
            , FieldName(fieldName)
            , Count(count)
            , m_pCollection(pCollection)
        {
        }

        friend class ReflectedType;
    };

    class ReflectedType
    {
    public:
        TypeIndex         m_TypeIndex;
        ::std::list< ::std::string >  TypeNames;
        size_t          Size;
        ReflectedTypesCollection *m_pCollection;
        bool m_bIsPointer;
        unsigned int ExtBufferId;

        ReflectedType(ReflectedTypesCollection *pCollection, TypeIndex typeIndex, const ::std::string typeName, size_t size, bool isPointer, unsigned int extBufferId);

        void AddField(ReflectedField::P pField)
        {
            m_Fields.push_back(pField);
        }

        ReflectedField::P AddField(TypeIndex typeIndex, ::std::string typeName, size_t typeSize, bool isPointer, size_t offset, const ::std::string fieldName, size_t count, unsigned int extBufferId);

        ReflectedField::FieldsCollectionCI FindField(const ::std::string fieldName) const;

        ReflectedField::FieldsCollection m_Fields;
        typedef mfx_cpp11::shared_ptr<ReflectedType> P;
        typedef ::std::list< ::std::string> StringList;

    };

    class ReflectedTypesCollection
    {
    public:
        typedef ::std::map<TypeIndex, ReflectedType::P> Container;

        Container m_KnownTypes;

        template <class T> ReflectedType::P FindExistingType()
        {
            return FindExistingType(TypeIndex(typeid(T)));
        }

        ReflectedType::P FindExistingByTypeInfoName(const char* name);

        ReflectedType::P FindExistingType(TypeIndex typeIndex);
        ReflectedType::P FindExtBufferTypeById(unsigned int ExtBufferId);

        ReflectedType::P DeclareType(TypeIndex typeIndex, const ::std::string typeName, size_t typeSize, bool isPointer, unsigned int extBufferId);

        ReflectedType::P FindOrDeclareType(TypeIndex typeIndex, const ::std::string typeName, size_t typeSize, bool isPointer, unsigned int extBufferId);

        void DeclareMsdkStructs();
    };

    template <class T>
    class AccessorBase
    {
    public:
        void *      m_P;
        const T*    m_pReflection;
        AccessorBase(void *p, const T &reflection)
            : m_P(p)
            , m_pReflection(&reflection)
        {
            if (NULL == m_pReflection)
            {
                m_pReflection = NULL;
            }
        }
        typedef mfx_cpp11::shared_ptr<T> P;

        template <class ORIGINAL_TYPE>
        const ORIGINAL_TYPE& Get() const
        {
            return *((const ORIGINAL_TYPE*)m_P);
        }
    };

    template <class T, class I>
    class IterableAccessor : public AccessorBase < T >
    {
    public:
        I m_Iterator;

        IterableAccessor(void *p, I iterator)
            : AccessorBase<T>(p, *(*iterator))
            , m_Iterator(iterator)
        {}
    };

    class AccessorType;

    class AccessorField : public IterableAccessor < ReflectedField, ReflectedField::FieldsCollectionCI >
    {
    public:
        const AccessorType &m_BaseStruct;
        AccessorField(const AccessorType &baseStruct, ReflectedField::FieldsCollectionCI iterator)
            : IterableAccessor(NULL, iterator)
            , m_BaseStruct(baseStruct)
        {
            SetFieldAddress();
        }

        void SetFieldAddress();

        void SetIterator(ReflectedField::FieldsCollectionCI iterator)
        {
            m_Iterator = iterator;
            m_pReflection = (*m_Iterator).get();
            SetFieldAddress();
        }

        AccessorField& operator++();

        bool Compare(const AccessorField& field) const
        {
            if (field.m_pReflection != m_pReflection)
            {
                throw ::std::invalid_argument(::std::string("Types mismatch"));
            }
            size_t size = m_pReflection->FieldType->Size;
            return (0 == memcmp(m_P, field.m_P, size));
        }

        AccessorType AccessSubtype() const;

        bool IsValid() const;
    };

    class AccessorType : public AccessorBase < ReflectedType >
    {
    public:
        AccessorType(void *p, const ReflectedType &reflection) : AccessorBase(p, reflection) {}

        AccessorField AccessField(ReflectedField::FieldsCollectionCI iter) const;
        AccessorField AccessField(const ::std::string fieldName) const;
        AccessorField AccessFirstField() const;
    };

    class AccessibleTypesCollection : public ReflectedTypesCollection
    {
    public:
        AccessibleTypesCollection() 
            : m_bIsInitialized(false)
        {}
        bool m_bIsInitialized;
        template <class T>
        AccessorType Access(T *p)
        {
            ReflectedType::P pType = FindExistingType(TypeIndex(typeid(T)));
            if (pType == NULL)
            {
                throw std::invalid_argument(std::string("Unknown type"));
            }
            else
            {
                return AccessorType(p, *pType);
            }
        }
    };

    class TypeComparisonResult;
    typedef mfx_cpp11::shared_ptr<TypeComparisonResult> TypeComparisonResultP;

    struct FieldComparisonResult
    {
        AccessorField accessorField1;
        AccessorField accessorField2;
        TypeComparisonResultP subtypeComparisonResultP;
    };

    class TypeComparisonResult : public ::std::list < FieldComparisonResult >
    {
    public:
        list<unsigned int> extBufferIdList;
    };

    typedef mfx_cpp11::shared_ptr<AccessorType> AccessorTypeP;

    TypeComparisonResultP CompareTwoStructs(AccessorType data1, AccessorType data2);

    std::string CompareStructsToString(AccessorType data1, AccessorType data2);
    void PrintStuctsComparisonResult(::std::stringstream& comparisonResult, ::std::string prefix, TypeComparisonResultP result);

    template <class T> bool PrintFieldIfTypeMatches(::std::ostream& stream, AccessorField field);
    void PrintFieldValue(::std::stringstream &stream, AccessorField field);
    ::std::ostream& operator<< (::std::ostream& stream, AccessorField field);
    ::std::ostream& operator<< (::std::ostream& stream, AccessorType data);
}