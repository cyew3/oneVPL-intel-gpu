/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2013 Intel Corporation. All Rights Reserved.

File Name: mfxstructures.h

*******************************************************************************/

#pragma once
#include "hash_array.h"
#include <iomanip>
//formaters used to complete actual serialisation to particula output in particular form

namespace Formater
{
    //serialization flags
    enum SerialisationFlags
    {
        //encode, mvc, mfx, bitrate(!CQP) - are default flags
        ENCODE  = 0,
        MFX     = 0,
        DEFAULT = 0,
        SVC     = 1,
        VPP     = 2,
        DECODE  = 4, // MFXInfoMfx
        JPEG_DECODE = 8
    };
    //default formatter
    class SimpleString 
    {
    protected:
        int m_align;
        tstring m_delimiter;
    public:
        SimpleString(int key_alignment = 30, const tstring & delimiter = VM_STRING(": "))
            : m_align(key_alignment)
            , m_delimiter(delimiter)
        {
        }
        tstring operator () (const hash_array<tstring,tstring> & elements) const 
        {
            tstringstream stream;
            hash_array<tstring,tstring>::const_iterator it;
            for(it = elements.begin(); it != elements.end(); it++)
            {
                stream<<std::left<<std::setw(m_align)<<it->first<<m_delimiter<<it->second<<std::endl;
            }
            return stream.str();
        }
    };

    class SingleKey
        : public SimpleString
    {
    protected:
        tstring m_key;
    public:
        SingleKey(const tstring & key, int key_alignment = 30)
            : SimpleString(key_alignment)
            , m_key(key)
        {
        }

        tstring operator () (const hash_array<tstring,tstring> & elements) const
        {
            tstringstream stream;
            hash_array<tstring,tstring>::const_iterator it;
            for(it = elements.begin(); it != elements.end(); it++)
            {
                stream<<std::left<<std::setw(m_align)<<m_key<<VM_STRING(": ")<<it->second<<std::endl;
            }
            return stream.str();
        }
    };

    class MapCopier
    {
    protected:
        hash_array<tstring,tstring> * m_refIn;
    public:
        MapCopier(hash_array<tstring,tstring> & inmap)
            : m_refIn(&inmap)
        {}
        tstring operator () (const hash_array<tstring,tstring> & elements) 
        {
            //map coping
            m_refIn->insert(m_refIn->end(), elements.begin(), elements.end());
            return VM_STRING("");
        }
    };

    class KeyPrefix
        : public MapCopier
    {
        tstring m_prefix;
    public:
        KeyPrefix(const tstring & prefix, hash_array<tstring,tstring> & inmap)
            : MapCopier(inmap)
            , m_prefix(prefix)
        {}

        tstring operator () (const hash_array<tstring,tstring> & elements)
        {
            hash_array<tstring, tstring>::const_iterator it;
            for(it = elements.begin(); it != elements.end(); it++)
            {
                (*m_refIn)[m_prefix+it->first] = it->second;
            }
            return VM_STRING("");
        }
    };


    template<class T>
    class PodFormater
    {
    public :
        const T & operator ()(const T & obj) const
        {
            return obj;
        }
        T & operator () (T & obj) const
        {
            return obj;
        }
        tstring head() const {
            return tstring();
        } 
    };
    //todo: improve
    class RefListFormater
    {
        //to make 1:1 relation to mfx_structures
        void _unused()
        {
            mfxExtAVCRefListCtrl ctrl;
            ctrl;
            STATIC_ASSERT(sizeof(ctrl.PreferredRefList[0]) == sizeof(RefListElement), c1);
            STATIC_ASSERT(sizeof(ctrl.RejectedRefList[0]) == sizeof(RefListElement), c1);
        }
        
    public:

        struct RefListElement{
            mfxU32      FrameOrder;
            mfxU16      PicStruct;
            mfxU16      ViewId; //for MVC interview prediction
            mfxU32      reserved[2]; //QualityID, DependencyID + 2 more just in case
        };
        
        template<class T>
        tstring operator () (const T & element) const
        {
           const RefListElement *rlElement = reinterpret_cast<const RefListElement*>(&element);

            if (rlElement->FrameOrder == (mfxU32)MFX_FRAMEORDER_UNKNOWN)
            {
                return VM_STRING("UNK");
            }
            tstringstream stream;
            stream << rlElement->FrameOrder;
            return stream.str();
        }
        tstring head() const {
            return tstring();
        } 
    };

    class LTRRefListFormater
    {
        //to make 1:1 relation to mfx_structures
        void _unused()
        {
            mfxExtAVCRefListCtrl ctrl;
            ctrl;
            STATIC_ASSERT(sizeof(ctrl.LongTermRefList[0]) == sizeof(LTRElement), c1);
        }

    public:

        struct LTRElement{
            mfxU32      FrameOrder;
            mfxU16      PicStruct;
            mfxU16      ViewId;
            mfxU16      LongTermIdx;
            mfxU16      reserved[3];
        } ;

        template<class T>
        tstring operator () (const T & element) const
        {
            const LTRElement *rlElement = reinterpret_cast<const LTRElement*>(&element);

            if (rlElement->FrameOrder == (mfxU32)MFX_FRAMEORDER_UNKNOWN)
            {
                return VM_STRING("UNK");
            }
            tstringstream stream;
            stream << VM_STRING("{")<<rlElement->FrameOrder << VM_STRING(",") << rlElement->LongTermIdx << VM_STRING("}");
            return stream.str();
        }
        tstring head() const {
            return tstring(VM_STRING("{FrameOrder,LongTermIdx}:"));
        } 
    };

    class AvcTemporalLayersFormater
    {
        //to make 1:1 relation to mfx_structures
        void _unused()
        {
            mfxExtAvcTemporalLayers layers;
            layers;
            STATIC_ASSERT(sizeof(layers.Layer[0]) == sizeof(AvcTemporalLayerElement), c1);
        }
    public:
        struct AvcTemporalLayerElement{
            mfxU16 Scale;
            mfxU16 reserved[3];
        };
    
        template<class T>
        tstring operator () (const T & element) const
        {
            const AvcTemporalLayerElement *tlElement = reinterpret_cast<const AvcTemporalLayerElement*>(&element);

            tstringstream stream;
            stream << tlElement->Scale;
            return stream.str();
        }
        tstring head() const {
            return tstring();
        } 
    };
}

namespace Formaters2
{
    //new type of serialisation formater
    //TODO: merge it with old one

    class ISerializer
    {
    public:
        virtual ~ISerializer(){}

        template<class T>
        void Serialize(const tstring &name, const T & value)
        {
            tstringstream stream;
            stream << value;
            SerializeInternal(name, stream.str());
        }
    protected:
        virtual void SerializeInternal(const tstring &name, const tstring &value) = 0;
    };

    //default formatter to standard string stream
    template <class Stream>
    class StreamSerializer
        : public ISerializer
        , private mfx_no_assign
    {
    protected:
        int m_align;
        Stream &m_ref_target;
        //tstring m_prefix;
    public:
        StreamSerializer(Stream &ref_target, int key_alignment = 30)
            : m_align(key_alignment)
            , m_ref_target(ref_target)
            //, m_prefix(prefix)
        {
        }
        StreamSerializer(const StreamSerializer & that)
            : m_align(that.m_align)
            , m_ref_target(that.m_ref_target)
        {
        }

    protected:
        virtual void SerializeInternal(const tstring &name, const tstring &value)
        {
            m_ref_target<<std::left<<std::setw(m_align)<<name<<VM_STRING(": ")<<value<<std::endl;
        }
    };

    template<class T>
    StreamSerializer<T> SerializedStream(T &ref_target, int key_alignment = 30)
    {
        StreamSerializer<T> stream(ref_target, key_alignment);
        return stream;
    }

    //adapter to old style formater
    class MapPusherSerializer
        : public ISerializer
        , private mfx_no_assign
    {
    protected:
        hash_array<tstring,tstring> & m_hash;
    public:
        MapPusherSerializer( hash_array<tstring,tstring> & hash)
            : m_hash(hash)

        {
        }
    protected:
        virtual void SerializeInternal(const tstring &name, const tstring &value)
        {
            m_hash.push_back(std::make_pair(name, value));
        }
    };

}