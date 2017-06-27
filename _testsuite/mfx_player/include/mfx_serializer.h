/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2017 Intel Corporation. All Rights Reserved.

File Name: mfxstructures.h

*******************************************************************************/

#pragma once

#include "mfx_ext_buffers.h"
#include "mfxvp8.h"
#include "mfxvp9.h"
#include "mfxmvc.h"
#include "mfxsvc.h"
#include <sstream>
#include "mfx_pipeline_types.h"
#include "mfx_serial_formater.h"


//to create not sorted map, this will allow us to save registration order

//base serializeble routines
//TODO : create an archive from this
class MFXStructureSerializer
{
protected:
    //union selection flags
    int m_flags;
    //mutable to use in serialization function that actually const but doesn't
    // change target structure, so it works as postconstrruction by flag
    mutable hash_array<tstring,tstring> m_values_map;
public:
    MFXStructureSerializer (int flags)
        : m_flags(flags)
    {
    }
    virtual ~MFXStructureSerializer() {}

    template <class _Formater>
    tstring         Serialize(_Formater fmt ) const 
    {
        ConstructValues();
        return fmt(m_values_map);
    }
    tstring         Serialize() const
    {
        return Serialize(Formater::SimpleString());
    }
    virtual bool    DeSerialize(const tstring & /*refStr*/, int * nPosition = NULL)
    {
        nPosition = nPosition; 
        return false;
    }

protected:
    
    //serialize helper, this function doesnt do any alignment
    template <class _IntType>
    void SerializeWithInserter(const tstring & str, const _IntType & cRefValue) const
    {
        tstringstream stream;
        stream << cRefValue;

        m_values_map[str] = stream.str();
    }
    template <class ElementType, class PodFmt>
    void SerializeArrayOfPODs(const tstring & str, ElementType * pArray, int nElements, PodFmt elementFormater ) const
    {
        tstringstream stream;

        stream << elementFormater.head();
        for (int i = 0; i < nElements; i++)
            stream<<elementFormater(pArray[i])<<((i+1==nElements) ? VM_STRING("") : VM_STRING(","));
        if (nElements)
            m_values_map[str] = stream.str();
        else
            m_values_map[str] = VM_STRING("empty");
    }

    //default elements formater
    template <class ElementType>
    void SerializeArrayOfPODs(const tstring & str, ElementType * pArray, int nElements) const
    {
        SerializeArrayOfPODs<ElementType, Formater::PodFormater<ElementType> >(str, pArray, nElements, Formater::PodFormater<ElementType>());
    }

    template <class _ElementType>
    bool DeserializeSingleElement(tstringstream & refInput, _ElementType &refnum1)
    {
        if (!refInput.good())
            return false;

        refInput>>refnum1;

        return !refInput.fail();
    }
    ///returns number of deserialised elements in array
    template <class _ElementType>
    bool DeserializeArray(tstringstream & refInput, int refnum1, _ElementType * pBuffer)
    {
        for (int i = 0;i < refnum1; i++)
        {
            refInput>>pBuffer[i];

            if (!refInput.good() && !(refInput.eof() && i + 1 == refnum1))
            {
                return false;
            }
        }
        return true;
    }

    template <class _StructType>
    void SerializeStruct(const tstring & prefix, _StructType &obj) const;
    //{
    //    MFXStructureRef<_StructType> struct_to_be_serialized(obj, m_flags);
    //    struct_to_be_serialized.Serialize(KeyPrefix(prefix , m_values_map));
    //}
    template <class _StructType>
    void SerializeArrayOfStructs(const tstring & prefix , _StructType *obj, int nItems) const
    {
        for (int i = 0; i < nItems; i++)
        {
            tstringstream struct_prefix;
            struct_prefix<<prefix<<VM_STRING("[")<<i<<VM_STRING("].");
            SerializeStruct(struct_prefix.str() ,obj[i]);
        }
        if (nItems == 0)
        {
            m_values_map[prefix] = VM_STRING("empty");
        }
    }

    virtual void ConstructValues () const = 0;
};



//NULL implementation
template <class T>
class MFXStructureBase 
    : public MFXStructureSerializer
{
protected:
    T * m_pStruct;
public:
    MFXStructureBase (T & refStruct, int flags = 0)  
        : MFXStructureSerializer(flags)
        , m_pStruct(&refStruct)
    {
    }
    operator T &()
    {
        return *m_pStruct;
    }
    operator const T &() const
    {
        return *m_pStruct;
    }
    T* operator &()
    {
        return m_pStruct;
    }
    const T* operator &() const
    {
        return m_pStruct;
    }

};


///reference structure is used with wrappered objects only
template <class T>
class MFXStructureRef 
    : public MFXStructureBase<T>
{
public:
    MFXStructureRef (T & refStruct, int flags = 0)
        : MFXStructureBase<T>(refStruct, flags)
    {
    }
};

//GCC specific template member should be not out of order -> move implementation of structref

template <class _StructType>
void MFXStructureSerializer::SerializeStruct(const tstring & prefix, _StructType& obj) const
{
    MFXStructureRef<_StructType> struct_to_be_serialized(obj, m_flags);
    struct_to_be_serialized.Serialize(Formater::KeyPrefix(prefix , m_values_map));
}

///non referenced structure contains mfxStructure itself
template <class T>
class MFXStructure 
    : public MFXStructureRef<T>
{
protected:
    T m_ownStruct;
public:
    MFXStructure (int flags = 0)
        : MFXStructureRef<T>(m_ownStruct, flags)
        , m_ownStruct()
    {
    }
};

//old params and new params
template <class T>
class MFXStructuresPair
    : public MFXStructureSerializer
{
    mutable hash_array<tstring,tstring> m_values_old, m_values_new;
public:

    MFXStructuresPair( const MFXStructureRef<T> & old
                     , const MFXStructureRef<T> & newStructure)
        : MFXStructureSerializer(0)
    {
        //obtaining maps from two version
        MFXStructureRef<T> oldSructureCopy = old;
        oldSructureCopy.Serialize(Formater::MapCopier(m_values_old));
     
        MFXStructureRef<T> newStructureCopy = newStructure;
        newStructureCopy.Serialize(Formater::MapCopier(m_values_new));
    }

    MFXStructuresPair( const MFXStructureRef<T> & old)
        : MFXStructureSerializer(0)
    {
        //obtaining maps from old params only
        MFXStructureRef<T> oldSructureCopy = old;
        oldSructureCopy.Serialize(Formater::MapCopier(m_values_old));
    }
    //if new and old params shares some buffers it is useful to use constructor with old values, 
    //and assign new prior serializing
    MFXStructuresPair<T>& SetNewParams( const MFXStructureRef<T> & newStructure)
    {
        MFXStructureRef<T> newStructureCopy = newStructure;
        newStructureCopy.Serialize(Formater::MapCopier(m_values_new));
        return *this;
    }

protected:
    virtual void ConstructValues () const 
    {
        //constructing own map
        hash_array<tstring,tstring>::iterator it;
        for(it = m_values_old.begin(); it != m_values_old.end(); it++ )
        {
            const tstring refkey = it->first;

            tstringstream stream;
            //zero indicates that user not specified this value
            if (m_values_new[refkey] != m_values_old[refkey])
            {
                stream<<std::left<<std::setw(5)<<
                    (m_values_new[refkey].empty() ? VM_STRING("-") : m_values_new[refkey])<< VM_STRING("(") <<
                    (m_values_old[refkey].empty() ? VM_STRING("-") : m_values_old[refkey])<< VM_STRING(")");
            }
            else 
            {
                stream<<std::left<<std::setw(5)<<m_values_new[refkey];
            }
            m_values_map[refkey] = stream.str();
        }
    }
};




//uses old new and get param structures
template <class T>
class MFXStructureThree
    : public MFXStructureSerializer
{
    MFXStructureRef<T> m_old;
    MFXStructureRef<T> m_new;
    //some parameters should reflect whether user whats to override output after getparam
    //known cases are buffersizeinkb
    MFXStructureRef<T> m_user;
    
    mutable hash_array<tstring,tstring> m_values_old, m_values_new, m_values_user;
public:
    
    MFXStructureThree( const MFXStructureRef<T> & old
                     , const MFXStructureRef<T> & newStructure
                     , const MFXStructureRef<T> & user_param )
        : MFXStructureSerializer(0)
        , m_old(old)
        , m_new(newStructure)
        , m_user(user_param)
    {
    }

protected:
    virtual void ConstructValues () const
    {
        //obtaining maps from three version
        m_old.Serialize(Formater::MapCopier(m_values_old));
        m_new.Serialize(Formater::MapCopier(m_values_new));
        m_user.Serialize(Formater::MapCopier(m_values_user));

        //constructing own map
        hash_array<tstring,tstring>::iterator it;
        for(it = m_values_old.begin(); it != m_values_old.end(); it++ )
        {
            const tstring refkey = it->first;
            
            //check for default falues of all possible types
            bool bNonUser = 
                m_values_user[refkey] == VM_STRING("0") || 
                m_values_user[refkey] == GetMFXChromaString(0) || 
                m_values_user[refkey].empty();

            tstringstream stream;
            //zero indicates that user not specified this value
            if (bNonUser && m_values_new[refkey] != m_values_old[refkey])
            {
                stream<<std::left<<std::setw(5)<<m_values_new[refkey]<<VM_STRING("(")<<m_values_old[refkey]<<VM_STRING(")");
            }
            else 
            {
                if (bNonUser)
                {
                    stream<<std::left<<std::setw(5)<<m_values_new[refkey];
                }
                else
                {
                    stream<<std::left<<std::setw(5)<<m_values_old[refkey]<<VM_STRING("(")<<m_values_old[refkey]<<VM_STRING(") - user");
                }

            }
            m_values_map[refkey] = stream.str();
        }

    }
};


//////////////////////////////////////////////////////////////////////////
//explicits
//serialized structures

template<class T, class Formater_Ty >
tstring Serialize(T & input_to_be_serialized, Formater_Ty _fmt)
{
    MFXStructureRef<T> serealizable(input_to_be_serialized);
    return serealizable.Serialize(_fmt);
}

template<class T>
tstring Serialize(T & input_to_be_serialized)
{
    MFXStructureRef<T> serealizable(input_to_be_serialized);
    return serealizable.Serialize(Formater::SimpleString());
}

//serialize usefull for single values serialization
template<class T>
tstring SerializeWithKey(const tstring &key, T & input_to_be_serialized)
{
    return Serialize(input_to_be_serialized, Formater::SingleKey(key));
}

template<>
class MFXStructureRef <mfxExtBuffer>
    : public MFXStructureBase<mfxExtBuffer>
{
public:
    MFXStructureRef(mfxExtBuffer & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtBuffer>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};



template<>
class MFXStructureRef <mfxMVCViewDependency>
    : public MFXStructureBase<mfxMVCViewDependency>
{
public:
    MFXStructureRef(mfxMVCViewDependency & refStruct, int flags = 0)
        :MFXStructureBase<mfxMVCViewDependency>(refStruct, flags)
    {
    }
    virtual bool    DeSerialize(const tstring & refStr, int *nPosition = NULL);
protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxMVCOperationPoint>
    : public MFXStructureBase<mfxMVCOperationPoint>
{
public:
    MFXStructureRef(mfxMVCOperationPoint & refStruct, int flags = 0)
        :MFXStructureBase<mfxMVCOperationPoint>(refStruct, flags)
    {
    }
protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtMVCSeqDesc>
    : public MFXStructureBase<mfxExtMVCSeqDesc>
{
public:
    MFXStructureRef(mfxExtMVCSeqDesc & refStruct, int flags = 0)
        :MFXStructureBase<mfxExtMVCSeqDesc>(refStruct, flags)
    {
    }
protected:
    virtual void ConstructValues () const;
};


template<>
class MFXStructureRef <mfxVideoParam>
    : public MFXStructureBase<mfxVideoParam>
{
public:
    MFXStructureRef(mfxVideoParam & refStruct, int flags = 0)
        :MFXStructureBase<mfxVideoParam>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};


template<>
class MFXStructureRef <mfxExtCodingOption>
    : public MFXStructureBase<mfxExtCodingOption>
{
public:
    MFXStructureRef(mfxExtBuffer & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtCodingOption>((mfxExtCodingOption &)refStruct, flags)
    {
    }
    MFXStructureRef(mfxExtCodingOption & refStruct, int flags = 0)
        :MFXStructureBase<mfxExtCodingOption>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtCodingOption2>
    : public MFXStructureBase<mfxExtCodingOption2>
{
public:
    MFXStructureRef(mfxExtBuffer & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtCodingOption2>((mfxExtCodingOption2 &)refStruct, flags)
    {
    }
    MFXStructureRef(mfxExtCodingOption2 & refStruct, int flags = 0)
        :MFXStructureBase<mfxExtCodingOption2>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtCodingOption3>
    : public MFXStructureBase<mfxExtCodingOption3>
{
public:
    MFXStructureRef(mfxExtBuffer & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtCodingOption3>((mfxExtCodingOption3 &)refStruct, flags)
    {
        }
    MFXStructureRef(mfxExtCodingOption3 & refStruct, int flags = 0)
        :MFXStructureBase<mfxExtCodingOption3>(refStruct, flags)
    {
        }
protected:
    virtual void ConstructValues() const;
};
template<>
class MFXStructureRef <mfxExtCodingOptionDDI>
    : public MFXStructureBase<mfxExtCodingOptionDDI>
{
public:
    MFXStructureRef(mfxExtBuffer & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtCodingOptionDDI>((mfxExtCodingOptionDDI &)refStruct, flags)
    {
    }

    MFXStructureRef(mfxExtCodingOptionDDI & refStruct, int flags = 0)
        :MFXStructureBase<mfxExtCodingOptionDDI>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtCodingOptionQuantMatrix>
    : public MFXStructureBase<mfxExtCodingOptionQuantMatrix>
{
public:
    MFXStructureRef(mfxExtBuffer & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtCodingOptionQuantMatrix>((mfxExtCodingOptionQuantMatrix &)refStruct, flags)
    {
    }

    MFXStructureRef(mfxExtCodingOptionQuantMatrix & refStruct, int flags = 0)
        :MFXStructureBase<mfxExtCodingOptionQuantMatrix>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtCodingOptionHEVC>
    : public MFXStructureBase<mfxExtCodingOptionHEVC>
{
public:
    MFXStructureRef(mfxExtBuffer & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtCodingOptionHEVC>((mfxExtCodingOptionHEVC &)refStruct, flags)
    {
    }

    MFXStructureRef(mfxExtCodingOptionHEVC & refStruct, int flags = 0)
        :MFXStructureBase<mfxExtCodingOptionHEVC>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};


template<>
class MFXStructureRef <mfxExtHEVCTiles>
    : public MFXStructureBase<mfxExtHEVCTiles>
{
public:
    MFXStructureRef(mfxExtBuffer & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtHEVCTiles>((mfxExtHEVCTiles &)refStruct, flags)
    {
    }

    MFXStructureRef(mfxExtHEVCTiles & refStruct, int flags = 0)
        :MFXStructureBase<mfxExtHEVCTiles>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtHEVCParam>
    : public MFXStructureBase<mfxExtHEVCParam>
{
public:
    MFXStructureRef(mfxExtBuffer & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtHEVCParam>((mfxExtHEVCParam &)refStruct, flags)
    {
    }

    MFXStructureRef(mfxExtHEVCParam & refStruct, int flags = 0)
        :MFXStructureBase<mfxExtHEVCParam>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtVP8CodingOption>
    : public MFXStructureBase<mfxExtVP8CodingOption>
{
public:
    MFXStructureRef(mfxExtBuffer & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtVP8CodingOption>((mfxExtVP8CodingOption &)refStruct, flags)
    {
    }

    MFXStructureRef(mfxExtVP8CodingOption & refStruct, int flags = 0)
        :MFXStructureBase<mfxExtVP8CodingOption>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtVP9Param>
    : public MFXStructureBase<mfxExtVP9Param>
{
public:
    MFXStructureRef(mfxExtBuffer & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtVP9Param>((mfxExtVP9Param &)refStruct, flags)
    {
    }

    MFXStructureRef(mfxExtVP9Param & refStruct, int flags = 0)
        :MFXStructureBase<mfxExtVP9Param>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxFrameInfo>
    : public MFXStructureBase<mfxFrameInfo>
{
public:
    MFXStructureRef(mfxFrameInfo & refStruct, int flags = 0)
        : MFXStructureBase<mfxFrameInfo>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxBitstream>
    : public MFXStructureBase<mfxBitstream>
{
public:
    MFXStructureRef(mfxBitstream & refStruct, int num_of_data_numbers)
        : MFXStructureBase<mfxBitstream>(refStruct, num_of_data_numbers)
    {
    }
protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxInfoVPP>
    : public MFXStructureBase<mfxInfoVPP>
{
public:
    MFXStructureRef(mfxInfoVPP & refStruct, int flags = 0)
        : MFXStructureBase<mfxInfoVPP>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxInfoMFX>
    : public MFXStructureBase<mfxInfoMFX>
{
public:
    MFXStructureRef(mfxInfoMFX & refStruct, int flags = 0)
        : MFXStructureBase<mfxInfoMFX>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <FourCC>
    : public MFXStructureBase<FourCC>
{
public:
    MFXStructureRef(FourCC & refStruct, int flags = 0)
        : MFXStructureBase<FourCC>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxVersion>
    : public MFXStructureBase<mfxVersion>
{
public:
    MFXStructureRef(mfxVersion & refStruct, int flags = 0)
        : MFXStructureBase<mfxVersion>(refStruct, flags)
    {
    }

    virtual bool DeSerialize(const tstring & refStr, int *nPosition);

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <MFXBufType>
    : public MFXStructureBase<MFXBufType>
{
public:
    MFXStructureRef(MFXBufType & refStruct, int flags = 0)
        : MFXStructureBase<MFXBufType>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <bool>
    : public MFXStructureBase<bool>
{
public:
    MFXStructureRef(bool & refStruct, int flags = 0)
        : MFXStructureBase<bool>(refStruct, flags)
    {
    }

protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxIMPL>
    : public MFXStructureBase<mfxIMPL>
{
public:
    MFXStructureRef(mfxIMPL & refStruct, int flags = 0)
        : MFXStructureBase<mfxIMPL>(refStruct, flags)
    {
    }
protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtVideoSignalInfo>
    : public MFXStructureBase<mfxExtVideoSignalInfo>
{
public:
    MFXStructureRef(mfxExtVideoSignalInfo & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtVideoSignalInfo>(refStruct, flags)
    {
    }
protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtAVCRefListCtrl>
    : public MFXStructureBase<mfxExtAVCRefListCtrl>
{
public:
    MFXStructureRef(mfxExtAVCRefListCtrl & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtAVCRefListCtrl>(refStruct, flags)
    {
    }
    virtual bool DeSerialize(const tstring & refStr, int *nPosition);
protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtVPPFrameRateConversion>
    : public MFXStructureBase<mfxExtVPPFrameRateConversion>
{
public:
    MFXStructureRef(mfxExtVPPFrameRateConversion & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtVPPFrameRateConversion>(refStruct, flags)
    {
    }
protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtCodingOptionSPSPPS>
    : public MFXStructureBase<mfxExtCodingOptionSPSPPS>
{
public:
    MFXStructureRef(mfxExtCodingOptionSPSPPS & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtCodingOptionSPSPPS>(refStruct, flags)
    {
    }
protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtAvcTemporalLayers>
    : public MFXStructureBase<mfxExtAvcTemporalLayers>
{
public:
    MFXStructureRef(mfxExtAvcTemporalLayers & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtAvcTemporalLayers>(refStruct, flags)
    {
    }
    virtual bool DeSerialize(const tstring & refStr, int *nPosition);
protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtVP9TemporalLayers>
    : public MFXStructureBase<mfxExtVP9TemporalLayers>
{
public:
    MFXStructureRef(mfxExtVP9TemporalLayers & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtVP9TemporalLayers>(refStruct, flags)
    {
    }
    virtual bool DeSerialize(const tstring & refStr, int *nPosition);
protected:
    virtual void ConstructValues() const;
};

template<>
class MFXStructureRef <mfxExtTemporalLayers>
    : public MFXStructureBase<mfxExtTemporalLayers>
{
public:
    MFXStructureRef(mfxExtTemporalLayers & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtTemporalLayers>(refStruct, flags)
    {
    }
    virtual bool DeSerialize(const tstring & refStr, int *nPosition);
protected:
    virtual void ConstructValues() const;
};

template<>
class MFXStructureRef <IppiRect>
    : public MFXStructureBase<IppiRect>
{
public:
    MFXStructureRef(IppiRect & refStruct, int flags = 0)
        : MFXStructureBase<IppiRect>(refStruct, flags)
    {
    }
    virtual bool DeSerialize(const tstring & refStr, int *nPosition);
protected:
    virtual void ConstructValues () const;
};


template<>
class MFXStructureRef <mfxExtSvcTargetLayer>
    : public MFXStructureBase<mfxExtSvcTargetLayer>
{
public:
    MFXStructureRef(mfxExtSvcTargetLayer & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtSvcTargetLayer>(refStruct, flags)
    {
    }
    virtual bool DeSerialize(const tstring & refStr, int *nPosition);
protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtEncoderCapability>
    : public MFXStructureBase<mfxExtEncoderCapability>
{
public:
    MFXStructureRef(mfxExtEncoderCapability & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtEncoderCapability>(refStruct, flags)
    {
    }
protected:
    virtual void ConstructValues () const;
};


template<>
class MFXStructureRef <mfxExtAVCEncodedFrameInfo>
    : public MFXStructureBase<mfxExtAVCEncodedFrameInfo>
{
public:
    MFXStructureRef(mfxExtAVCEncodedFrameInfo & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtAVCEncodedFrameInfo>(refStruct, flags)
    {
    }
protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxExtAVCRefLists::mfxRefPic>
    : public MFXStructureBase<mfxExtAVCRefLists::mfxRefPic>
{
public:
    MFXStructureRef(mfxExtAVCRefLists::mfxRefPic & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtAVCRefLists::mfxRefPic>(refStruct, flags)
    {
    }
protected:
    virtual void ConstructValues() const;
};

template<>
class MFXStructureRef <mfxExtAVCRefLists>
    : public MFXStructureBase<mfxExtAVCRefLists>
{
public:
    MFXStructureRef(mfxExtAVCRefLists & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtAVCRefLists>(refStruct, flags)
    {
    }
protected:
    virtual void ConstructValues() const;
};

template<>
class MFXStructureRef <mfxExtPredWeightTable>
    : public MFXStructureBase<mfxExtPredWeightTable>
{
public:
    MFXStructureRef(mfxExtPredWeightTable & refStruct, int flags = 0)
        : MFXStructureBase<mfxExtPredWeightTable>(refStruct, flags)
    {
    }
protected:
    virtual void ConstructValues() const;
};

template <class T> 
MFXStructureRef<T> make_structure_ref( T & structure) {
    MFXStructureRef<T> struct_ref(structure);
    return struct_ref;
}


template <class CharT, class Traits, class T>
std::basic_ostream<CharT, Traits> & operator << (std::basic_ostream<CharT, Traits> &os, const MFXStructureRef<T> &obj)
{
    os << obj.Serialize();
    return os;
}

/*
* Rationale : provides extensibility for any de-serializeable object that works with string input only
*/
template <class T>
class DeSerializeHelperRef : public MFXStructureRef<T>
{
public:
    DeSerializeHelperRef(T & refStruct, int flags = 0)
        : MFXStructureRef<T>(refStruct, flags)
    {
    }
    /// roolStart - usualy meaningfull option name during which parsing error happened
    /// TODO: refactor behavior to directly highlight commandline position with error
    bool DeSerialize(vm_char **& start, vm_char ** end, vm_char **rollStart = NULL)
    {
        if (NULL == rollStart)
            rollStart = start - 1;

        vm_char **start_orig = start;
        start = rollStart;
        MFX_CHECK_WITH_ERR(start_orig < end, false);

        int nDeserializedTo = 0;
        MFX_CHECK_WITH_ERR(MFXStructureRef<T>::DeSerialize(StringFromPPChar(start_orig, end), &nDeserializedTo), false);
        
        //-1 is due to argv will be incremented in for statement
        start  = start_orig;
        start += PPCharOffsetFromStringOffset(start, nDeserializedTo) - 1;

        return true;
    }

protected:
    //constructs string from argv array, 
    static tstring StringFromPPChar(vm_char ** start, vm_char **end)
    {
        tstringstream stream;
        for (;start != end; start++)
        {
            stream<<*start;
            if (start + 1 != end)
                stream<<VM_STRING(" ");
        }
        return stream.str();
    }
    ///returns number of vm_char* items that were processed according to string offset
    static int PPCharOffsetFromStringOffset(vm_char ** start, int nOffset)
    {
        int nItems = 0;
        for (; nOffset > 0; nItems++, nOffset--, start++)
        {
            nOffset -= (int)vm_string_strlen(*start);
            if (nOffset < 0)
                return nItems;
        }
        return nItems;
    }
    virtual void ConstructValues () const
    {
    }
};

template <class T> bool DeSerialize(T &obj, vm_char **& start, vm_char ** end, vm_char **rollStart = NULL)
{
    DeSerializeHelperRef<T> helper(obj);
    return helper.DeSerialize(start, end, rollStart);
}

template <class T>
class DeSerializeHelper : public DeSerializeHelperRef<T>
{
protected:
    T m_ownStruct;
public:
    DeSerializeHelper(int flags = 0)
        : DeSerializeHelperRef<T>(m_ownStruct, flags)
    {
    }
};
