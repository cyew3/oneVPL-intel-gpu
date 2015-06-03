/******************************************************************************* *\

Copyright (C) 2010-2015 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_serializer.h

*******************************************************************************/

#pragma once

#include <string>
#include <sstream>

#include "mfxcommon.h"
#include "mfxstructures.h"
#include "mfxvp8.h"
#include "mfxmvc.h"

#include "mfx_serial_formater.h"
#include "mfx_serial_utils.h"

//base serializeble routines
class MFXStructureSerializer
{
protected:
    //union selection flags
    int m_flags;
    //mutable to use in serialization function that actually const but doesn't
    // change target structure, so it works as postconstrruction by flag
    mutable hash_array<std::string, std::string> m_values_map;
public:
    MFXStructureSerializer (int flags)
        : m_flags(flags)
        , m_values_map()
    {
    }
    virtual ~MFXStructureSerializer() {}

    template <class _Formater>
    std::string Serialize(_Formater & fmt) const 
    {
        ConstructValues();
        return fmt(m_values_map);
    }
    std::string Serialize() const
    {
        Formater::SimpleString frmt;
        return Serialize(frmt);
    }
    template <class _Formater>
    bool DeSerialize(hash_array<std::string, std::string> values_map)
    {
        return false;
    }

protected:
    template <class _IntType>
    void SerializeSingleElement(const std::string & str, const _IntType & cRefValue) const
    {
        std::stringstream stream;
        stream << cRefValue;

        m_values_map[str] = stream.str();
    }

    template <class _ElementType>
    bool DeSerializeSingleElement(std::string & refInput, _ElementType &refnum1) const
    {
        if (refInput.empty())
            return true;

        std::stringstream refStream(refInput);

        if (!refStream.good())
            return false;

        refStream>>refnum1;

        return !refStream.fail();
    }

    template <class ElementType>
    void SerializeArrayOfPODs(const std::string & str, ElementType * pArray, int nElements) const
    {
        std::stringstream stream;

        for (int i = 0; i < nElements; i++)
            stream << pArray[i] << ((i+1==nElements) ? "" : ",");
        if (nElements)
            m_values_map[str] = stream.str();
        else
            m_values_map[str] = "empty";
    }

    template <class ElementType>
    void DeSerializeArrayOfPODs(const std::string & str, ElementType * pArray, int nElements) const
    {
        if (nElements == 0)
            return;

        std::vector<std::string> values;
        size_t start = 0, end = 0;
        while ((end = str.find(",", start)) != std::string::npos)
        {
            values.push_back(str.substr(start, end - start));
            start = end + 1;
        }
        values.push_back(str.substr(start));

        for (int i = 0; i < nElements; i++)
            std::stringstream(values[i]) >> pArray[i];
    }

    //special case for unsigned char
    void SerializeSingleElement(const std::string & str, const mfxU8 & cRefValue) const
    {
        std::stringstream stream;
        stream << (mfxU16)cRefValue;

        m_values_map[str] = stream.str();
    }

    bool DeSerializeSingleElement(std::string & refInput, mfxU8 &refnum1) const
    {
        if (refInput.empty())
            return true;

        std::stringstream refStream(refInput);

        if (!refStream.good())
            return false;

        mfxU16 refnum = 0;

        refStream>>refnum;

        refnum1 = (mfxU8)refnum;

        return !refStream.fail();
    }

    void SerializeArrayOfPODs(const std::string & str, mfxU8 * pArray, int nElements) const
    {
        std::stringstream stream;

        for (int i = 0; i < nElements; i++)
            stream << (mfxU16)pArray[i] << ((i+1==nElements) ? "" : ",");
        if (nElements)
            m_values_map[str] = stream.str();
        else
            m_values_map[str] = "empty";
    }

    void DeSerializeArrayOfPODs(const std::string & str, mfxU8 * pArray, int nElements) const
    {
        if (nElements == 0)
            return;

        std::vector<std::string> values;
        size_t start = 0, end = 0;
        while ((end = str.find(",", start)) != std::string::npos)
        {
            values.push_back(str.substr(start, end - start));
            start = end + 1;
        }
        values.push_back(str.substr(start));

        mfxU16 val = 0;

        for (int i = 0; i < nElements; i++)
        {
            std::stringstream(values[i]) >> val;
            pArray[i] = (mfxU8)val;
        }
    }

    template <class _StructType>
    void SerializeStruct(const std::string & prefix, _StructType &obj) const;
    template <class _StructType>
    bool DeSerializeStruct(hash_array<std::string, std::string> values_map, const std::string & prefix, _StructType &obj) const;

    template <class _StructType>
    void SerializeArrayOfStructs(const std::string & prefix , _StructType *obj, int nItems) const
    {
        for (int i = 0; i < nItems; i++)
        {
            std::stringstream struct_prefix;
            struct_prefix << prefix << "[" << i << "].";
            SerializeStruct(struct_prefix.str(), obj[i]);
        }
        if (nItems == 0)
        {
            m_values_map[prefix] = "empty";
        }
    }

    template <class _StructType>
    void DeSerializeArrayOfStructs(hash_array<std::string, std::string> values_map, const std::string & prefix , _StructType *obj, int nItems) const
    {
        for (int i = 0; i < nItems; i++)
        {
            std::stringstream struct_prefix;
            struct_prefix << prefix << "[" << i << "].";
            DeSerializeStruct(values_map, struct_prefix.str(), obj[i]);
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

//reference structure is used with wrappered objects only
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
void MFXStructureSerializer::SerializeStruct(const std::string & prefix, _StructType& obj) const
{
    MFXStructureRef<_StructType> struct_to_be_serialized(obj, m_flags);
    Formater::KeyPrefix frmt(prefix , m_values_map);
    struct_to_be_serialized.Serialize(frmt);
}

template <class _StructType>
bool MFXStructureSerializer::DeSerializeStruct(hash_array<std::string, std::string> values_map, const std::string & prefix, _StructType& obj) const
{
    MFXStructureRef<_StructType> struct_to_be_deserialized(obj, m_flags);
    return struct_to_be_deserialized.DeSerialize(DeFormater::KeyPrefix::parse_hash(prefix , values_map));
}

//non referenced structure contains mfxStructure itself
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
    mutable hash_array<std::string,std::string> m_values_old, m_values_new;
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
        hash_array<std::string,std::string>::iterator it;
        for(it = m_values_old.begin(); it != m_values_old.end(); it++ )
        {
            const std::string refkey = it->first;

            std::stringstream stream;
            //zero indicates that user not specified this value
            if (m_values_new[refkey] != m_values_old[refkey])
            {
                stream<<std::left<<std::setw(5)<<
                    (m_values_new[refkey].empty() ? "-" : m_values_new[refkey])<< "(" <<
                    (m_values_old[refkey].empty() ? "-" : m_values_old[refkey])<< ")";
            }
            else 
            {
                stream<<std::left<<std::setw(5)<<m_values_new[refkey];
            }
            m_values_map[refkey] = stream.str();
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//serialized structures

template<class T>
std::string CompareStructs(T & input1, T & input2, int flags = 0)
{
    return MFXStructuresPair<T>(MFXStructureRef<T>(input1, flags), MFXStructureRef<T>(input2, flags)).Serialize();;
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
    virtual bool DeSerialize(hash_array<std::string, std::string> values_map);
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
    virtual bool    DeSerialize(const std::string & refStr, int *nPosition = NULL)
    {
        refStr;
        nPosition = nPosition; 
        return false;
    };
    virtual bool DeSerialize (hash_array<std::string, std::string> values_map);
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
    virtual bool DeSerialize (hash_array<std::string, std::string> values_map);
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
    virtual bool DeSerialize (hash_array<std::string, std::string> values_map);
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
    virtual bool DeSerialize (hash_array<std::string, std::string> values_map);
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
    bool DeSerialize (hash_array<std::string, std::string> values_map);
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
    bool DeSerialize (hash_array<std::string, std::string> values_map);
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
    bool DeSerialize (hash_array<std::string, std::string> values_map);
protected:
    virtual void ConstructValues() const;
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
    bool DeSerialize (hash_array<std::string, std::string> values_map);
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
    bool DeSerialize (hash_array<std::string, std::string> values_map);
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
    bool DeSerialize (hash_array<std::string, std::string> values_map);
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
    virtual bool DeSerialize(hash_array<std::string, std::string> values_map);
protected:
    virtual void ConstructValues () const;
};

template<>
class MFXStructureRef <mfxBitstream>
    : public MFXStructureBase<mfxBitstream>
{
public:
    MFXStructureRef(mfxBitstream & refStruct, int num_of_data_numbers = 0)
        : MFXStructureBase<mfxBitstream>(refStruct, num_of_data_numbers)
    {
    }
    bool DeSerialize (hash_array<std::string, std::string> values_map);
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
    bool DeSerialize (hash_array<std::string, std::string> values_map);
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
    bool DeSerialize(hash_array<std::string, std::string> values_map);
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
    bool DeSerialize(hash_array<std::string, std::string> values_map);
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
    bool DeSerialize(hash_array<std::string, std::string> values_map);
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
    bool DeSerialize(hash_array<std::string, std::string> values_map);
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
    bool DeSerialize(hash_array<std::string, std::string> values_map);
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
    bool DeSerialize(hash_array<std::string, std::string> values_map);
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
    bool DeSerialize(hash_array<std::string, std::string> values_map);
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
    virtual bool DeSerialize(hash_array<std::string, std::string> values_map);
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
    virtual bool DeSerialize(hash_array<std::string, std::string> values_map);
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
    virtual bool DeSerialize(hash_array<std::string, std::string> values_map);
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
    virtual bool DeSerialize(hash_array<std::string, std::string> values_map);
protected:
    virtual void ConstructValues () const;
};

template <class T>
class DeSerializeHelper : public MFXStructureRef<T>
{
public:
    DeSerializeHelper(T & Struct, int flags = 0)
        : MFXStructureRef<T>(Struct, flags)
    {
    }

    template <class _Formater>
    bool DeSerialize(std::string & input, _Formater & fmt)
    {
        return MFXStructureRef<T>::DeSerialize(fmt(input));
    }
};

//////////////////////////////////////////////////////////////////////////
//API

template<class T>
bool SerializeToList(T & input_to_be_serialized, const std::string struct_name, const std::string file_name, int flags = 0)
{
    MFXStructureRef<T> serealizable(input_to_be_serialized, flags);
    Formater::SimpleString frmt(50, struct_name + ".");
    std::string input = serealizable.Serialize(frmt);
    return SaveSerializedStructure(file_name, input);
}

template<class T>
bool DeSerializeFromList(T & output_to_be_deserialized, const std::string struct_name, const std::string file_name)
{
    std::string input_string(UploadSerialiedStructures(file_name));
    DeSerializeHelper<T> sStruct(output_to_be_deserialized);
    DeFormater::SimpleString dfrmt(struct_name + ".");
    return sStruct.DeSerialize(input_string, dfrmt);
}

template<class T>
bool SerializeToYaml(T & input_to_be_serialized, const std::string struct_name, const std::string file_name, int flags = 0)
{
    MFXStructureRef<T> serealizable(input_to_be_serialized, flags);
    Formater::Yaml frmt(struct_name);
    std::string input = serealizable.Serialize(frmt) + "\n\n";
    return SaveSerializedStructure(file_name, input);
}

template<class T>
bool DeSerializeFromYaml(T & output_to_be_deserialized, const std::string struct_name, const std::string file_name)
{
    std::string input_string(UploadSerialiedStructures(file_name));
    DeSerializeHelper<T> sStruct(output_to_be_deserialized);
    DeFormater::Yaml dfrmt(struct_name);
    return sStruct.DeSerialize(input_string, dfrmt);
}
