/* ***************7*************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010 - 2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef MFX_EXTENDED_BUFFER_H
#define MFX_EXTENDED_BUFFER_H

#include<memory>
#include "mfx_singleton.h"
#include "mfx_ext_buffers.h"
#include "mfxmvc.h"
#include "mfxpcp.h"
#include "mfxsvc.h"
#include "mfxvp8.h"
#include "mfxvp9.h"
#include "mfx_iclonebale.h"

 //warning C4345: behavior change: an object of POD type constructed with an initializer of the form () will be default-initialized
#pragma warning (disable:4345)

//map of the structures type to it's id value
template <class T> 
class BufferIdOf
{
public :
    enum {id = 0};
private:
    BufferIdOf(){}
};

//allows templates buffers to be treat as syngle type
class MFXExtBufferPtrBase
    : public std::auto_ptr<mfxExtBuffer>
    , public ICloneable
    //shared ptr would be great
{
    IMPLEMENT_CLONE(MFXExtBufferPtrBase);
protected:
    bool m_bOwnObject;
public :
     MFXExtBufferPtrBase(mfxExtBuffer *pBuffer = NULL)
         : std::auto_ptr<mfxExtBuffer>(pBuffer)
         , m_bOwnObject(true)
     {
     }
     MFXExtBufferPtrBase(MFXExtBufferPtrBase *pObj)
         : std::auto_ptr<mfxExtBuffer>(NULL == pObj ? NULL : pObj->get())
         , m_bOwnObject(false)
     {
         //donot know specifics
     }
     
     virtual void reset(mfxExtBuffer* p)
     {
         //donot destoyed by auto_ptr
         if (!m_bOwnObject)
             release();

         std::auto_ptr<mfxExtBuffer>::reset(p);
     }
     //aplication shouldnt call this, restore_consistency should be invoked automatically before every using
     virtual void restore_consistency(){}
     
     virtual ~MFXExtBufferPtrBase()
     {
         if (!m_bOwnObject)
         {
             release();
         }
     }
};

//forward declaration only
template <class T> class MFXExtBufferPtr;

template <class T>
class DynCastToBuffer
{
public:
    MFXExtBufferPtr<T>* operator ()(MFXExtBufferPtrBase*) ;
};

class MFXExtBufferVector;


//base for temporary provider objects - used in member access operator (->)
template <class T>
class MFXExtBufferPtrRef
{
    T * ptr;
public:
    MFXExtBufferPtrRef(MFXExtBufferPtrBase* pObj){ptr = reinterpret_cast<T*>(pObj->get());}
    T* operator -> (){return ptr;}
};


template <class T>
class _MFXExtBufferPtr
    : public MFXExtBufferPtrBase
{
    IMPLEMENT_CLONE(_MFXExtBufferPtr);
protected:
    T * m_pParticular;

public:
    typedef T element_type_ext;
public:
    
    //TODO: not owned an object auto_ptr limitation
    _MFXExtBufferPtr(MFXExtBufferPtrBase * pObj)
        : MFXExtBufferPtrBase(pObj)
    {
            reset((T*)((NULL == pObj) ? NULL : pObj->get()));
    }
    //constructs from mfxextbuffer
    _MFXExtBufferPtr(element_type * pObj = NULL)
    {
        reset(pObj);
    }
    _MFXExtBufferPtr(T * pObj )
    {
        reset(pObj);
    }
    _MFXExtBufferPtr(const MFXExtBufferVector &container);

    _MFXExtBufferPtr(const _MFXExtBufferPtr & other)
    {
        T * pAllocated = NULL;
        if (other.get())
        {
            pAllocated = new T;
            memcpy(pAllocated, other.get(), sizeof(T));
            reset(pAllocated);
        }
    }


    void reset (T * pObj)
    {
        MFXExtBufferPtrBase::reset((mfxExtBuffer*)pObj);
        m_pParticular = (T*) MFXExtBufferPtrBase::get();
        if (NULL != m_pParticular)
        {
            MFXExtBufferPtrBase::get()->BufferSz = sizeof(T);
            MFXExtBufferPtrBase::get()->BufferId = id;
        }
    }
    void reset (mfxExtBuffer * pObj)
    {
        reset((T*)pObj);
    }    
    
    //compare whether buffer wasn't modified
    bool IsZero()
    {
        if (NULL == get())
            return true;
        
        return !memcmp(m_pParticular, &zero(), sizeof(T));
        
    }
    T& operator * () const
    {
        return *m_pParticular;
    }
    T* operator -> () const
    {
        return m_pParticular;
    }



    T* get () const
    {
        return m_pParticular;
    }
    static const int id = BufferIdOf<T>::id;
    
    static const T & zero()
    {
        static T p = {};
        static bool bInitialized = false;
        if (!bInitialized)
        {
            ((mfxExtBuffer*)&p)->BufferSz = sizeof(T);
            ((mfxExtBuffer*)&p)->BufferId = id;
            bInitialized = true;
        }
        return p;
    }

protected:
    //cast dynamically to extendbuffer<T> from the base
    static DynCastToBuffer<T> Tcast;
};

//child class to minimize number of function in specifications
template <class T>
class MFXExtBufferPtr : public _MFXExtBufferPtr<T>
{
    IMPLEMENT_CLONE(MFXExtBufferPtr);
    typedef _MFXExtBufferPtr<T> base;
    typedef typename _MFXExtBufferPtr<T>::element_type element_type;

public:
    MFXExtBufferPtr(element_type * pObj = NULL)
        :base(pObj){
    }
    MFXExtBufferPtr(T * pObj )
        : base(pObj){
    }
    MFXExtBufferPtr(MFXExtBufferPtrBase & ref_obj)
        :base(ref_obj){
    }
    MFXExtBufferPtr(const MFXExtBufferVector & ref_obj)
        :base(ref_obj){
    }
};


template<class T>
class MFXExtBufferCompareByID
    : public std::binary_function<T *, mfxU32, bool>
{
public:
    bool operator ()(T * buf, mfxU32 nId) const 
    {
       return buf->BufferId == nId;
    }
};

template<>
class MFXExtBufferCompareByID <MFXExtBufferPtrBase>
    : public std::binary_function<MFXExtBufferPtrBase *, mfxU32, bool>
{
public:
    bool operator ()(MFXExtBufferPtrBase * buf, mfxU32 nId) const 
    {
        return (*buf)->BufferId == nId;
    }
};


class MFXExtBufferVector
    : public std::vector<mfxExtBuffer*>
{
protected:
    //we need to know how to delete object
    std::list<MFXExtBufferPtrBase*> m_buffers;

    //todo : move this flags to Imfxextendedbufferptr
    bool m_bOwnBuffers;
public:
        
    MFXExtBufferVector(mfxVideoParam & vParam);
    //no ref counters of course
    MFXExtBufferVector(mfxExtBuffer**pBufers = NULL, mfxU16 nBuffers = 0);

    virtual ~MFXExtBufferVector();

    void push_back(mfxExtBuffer* pBuffer);
    //copies whole buffer
    void push_back(const mfxExtBuffer& pBuffer);
    void push_back(MFXExtBufferPtrBase & pBuffer);

    void push_back(MFXExtBufferPtrBase * pBuffer);

    template <class T>
    void push_back(T * pObj)
    {
        MFXExtBufferPtr<T> obj(pObj);
        push_back(obj);
        //since clone not copied owned pointer release is required
        obj.release();
    }

    template <class _Iter>
    void merge(_Iter first, _Iter last)
    {
        //TODO:fix this ASAP
        //m_bOwnBuffers = false;
        for(;first != last; first++ )
        {
            //mfxextbuffers
            //need to copy memory
            push_back(**first);
        }
    }

    //TODO: do we need this?
    MFXExtBufferPtrBase * get_by_id(mfxU32 nId)const;
    mfxExtBuffer ** operator &();

    //destroys mfxExtbuffers, and calls to underlined destructors
    void clear();

};

//template members implementation
template <class T>
_MFXExtBufferPtr<T>::_MFXExtBufferPtr(const MFXExtBufferVector &container)
: MFXExtBufferPtrBase(container.get_by_id(id))
{
    m_pParticular = (T*)MFXExtBufferPtrBase::get();
}

template <class T>
DynCastToBuffer<T> _MFXExtBufferPtr<T>::Tcast;

template<class T>
MFXExtBufferPtr<T>* DynCastToBuffer<T>::operator ()(MFXExtBufferPtrBase * ref_obj)
{
    return dynamic_cast<MFXExtBufferPtr<T>*>(ref_obj);
}

//gets internal vector buffer even if it is empty but elements are reserved
template <class T>
T* vector_front(std::vector<T> & refVector)
{
    if (refVector.capacity() == 0)
        return NULL;
    if (refVector.empty())
    {
        refVector.resize(1);
        T * pStart= &refVector.front();
        refVector.pop_back();
        return pStart;
    }
    return & refVector.front();
}

//temporary created vector that commit all changes with temp object to target object
template <class T>
class ElementsVector 
    : public std::vector<T>
{
    std::vector<T>* m_ref_vector;
    mfxU32        * m_num;
    mfxU32        * m_num_allocated;
    T            ** m_pArray;
    
    typedef std::vector<T> base;
public:

    ElementsVector(std::vector<T>* ref_vector = NULL, T ** pArray= NULL, mfxU32 *num= NULL, mfxU32 *num_allocated= NULL)
        : m_ref_vector(ref_vector)
        , m_num(num)
        , m_num_allocated(num_allocated)
        , m_pArray(pArray)
    {

        //we have to transfer elements from target vector in order we may need to edit them
        if (NULL != m_ref_vector)
        {
            base::insert(base::end(), m_ref_vector->begin(), m_ref_vector->end());
            m_ref_vector->clear();
            
        }

        if (m_num_allocated)
        {
            base::reserve(*m_num_allocated);
        }
        
        //if (num_allocated < num)
        // in that case num was updated by Mediasdk
        // to retain vector consistency we need to 1 : either insert elements 2: erase num
        //non of solutions are appropriate, that is why we need to remained num field in structure
    }


    ~ElementsVector()
    {
        //nothing allocated
        if (0 == base::capacity() || NULL == m_ref_vector)
            return;

        //we need to reserve all space to prevent buffer reallocation
        m_ref_vector->reserve(base::capacity());

        if (m_pArray)
        {
            *m_pArray = vector_front(*m_ref_vector);
        }

        //transfer elements back to target vector
        m_ref_vector->insert(m_ref_vector->end(), base::begin(), base::end());

        //reseting structure pointers
        if (m_num)
            *m_num = (mfxU32)m_ref_vector->size();

        if (m_num_allocated)
            *m_num_allocated = (mfxU32)m_ref_vector->capacity();
    }
};



#define GET_OWNER_MEMBER_PTR(member) (pOwner->get()? &pOwner->get()->member : NULL)
#define GET_OWNER_MEMBER(member) (pOwner->get()? pOwner->get()->member : 0)
#define GET_OWNER_MEMBER_OR_LOCAL(member) (pOwner->get()? pOwner->get()->member : m_##member)


#define DECL_ELEMENTS_ALLOCATED(name, num, num_alloc)\
    name(*pOwner->m_##name, GET_OWNER_MEMBER_PTR(name), GET_OWNER_MEMBER_PTR(num), GET_OWNER_MEMBER_PTR(num_alloc))

#define DECL_ELEMENTS(name, num)\
    name(*pOwner->m_##name, GET_OWNER_MEMBER_PTR(name), GET_OWNER_MEMBER_PTR(num))

#define DECL_MFXU32(name)\
    name(GET_OWNER_MEMBER(name))

#define DECL_MFXU32_REF(name)\
    name(GET_OWNER_MEMBER_OR_LOCAL(name))


//arrays presenter could be attached to something or use own array
template <class T>
class vector_with_ptr 
    : public std::vector <T>
{
    std::vector<T> *m_pVector ;

    typedef std::vector<T> base;

public:
    //points to it self
    vector_with_ptr()
    {
        m_pVector = this;
    }
    vector_with_ptr(std::vector<T>*pOther)
    {
        m_pVector = pOther;
    }
    vector_with_ptr(vector_with_ptr<T> &other)
    {
        m_pVector = other.m_pVector;
    }
    base * operator -> ()
    {
        return m_pVector;
    }
    base * operator * ()
    {
        return m_pVector;
    }
    void attach(vector_with_ptr<T> &other)
    {
        m_pVector = other.m_pVector;
    }
};

//contains all registered buffers
#define INCLUDED_FROM_MFX_EXTENDED_BUFFER_H
#include "mfx_extended_buffer_id.h"
#undef INCLUDED_FROM_MFX_EXTENDED_BUFFER_H

//////////////////////////////////////////////////////////////////////////
//VppDoNotUse specification

template <>
class MFXExtBufferPtr<mfxExtVPPDoNotUse> 
    : public _MFXExtBufferPtr<mfxExtVPPDoNotUse>
{
    template<class T>
    friend class MFXExtBufferPtrRef;

    IMPLEMENT_CLONE(MFXExtBufferPtr);
protected:
    vector_with_ptr<mfxU32> m_AlgList ;
public:
    MFXExtBufferPtr(element_type * pObj = NULL)
        :_MFXExtBufferPtr<mfxExtVPPDoNotUse>(pObj){
    }
    MFXExtBufferPtr(element_type_ext * pObj )
        : _MFXExtBufferPtr<mfxExtVPPDoNotUse>(pObj){
    }
    MFXExtBufferPtr(MFXExtBufferPtrBase * pObj)
        : _MFXExtBufferPtr<mfxExtVPPDoNotUse>(pObj)
        , m_AlgList(Tcast(pObj)->m_AlgList){
    }
    MFXExtBufferPtr(const MFXExtBufferVector & ref_obj)
        : _MFXExtBufferPtr<mfxExtVPPDoNotUse>(ref_obj)
        , m_AlgList(Tcast(ref_obj.get_by_id(id))->m_AlgList){
    }
    MFXExtBufferPtr(const MFXExtBufferPtr & other)
        : _MFXExtBufferPtr<mfxExtVPPDoNotUse> (other.get()){
        
    }
    //TODO: consider to remove reference in that case we can reflect changes not very quickly but the performance
    //increased dramatically
    MFXExtBufferPtrRef<mfxExtVPPDoNotUse> operator -> ();
    MFXExtBufferPtrRef<mfxExtVPPDoNotUse> operator *  ();
};

template<>
class MFXExtBufferPtrRef<mfxExtVPPDoNotUse>
{
public :
    MFXExtBufferPtrRef(MFXExtBufferPtr<mfxExtVPPDoNotUse> *pOwner)
    : DECL_ELEMENTS(AlgList, NumAlg)
    {
    }
    MFXExtBufferPtrRef * operator ->()
    {
        return this;
    }
    ElementsVector<mfxU32> AlgList;
};

template <>
class MFXExtBufferPtr<mfxExtVPPDoUse> 
    : public _MFXExtBufferPtr<mfxExtVPPDoUse>
{
    template<class T>
    friend class MFXExtBufferPtrRef;

    IMPLEMENT_CLONE(MFXExtBufferPtr);
protected:
    vector_with_ptr<mfxU32> m_AlgList ;
public:
    MFXExtBufferPtr(element_type * pObj = NULL)
        :_MFXExtBufferPtr<mfxExtVPPDoUse>(pObj){
    }
    MFXExtBufferPtr(element_type_ext * pObj )
        : _MFXExtBufferPtr<mfxExtVPPDoUse>(pObj){
    }
    MFXExtBufferPtr(MFXExtBufferPtrBase * pObj)
        : _MFXExtBufferPtr<mfxExtVPPDoUse>(pObj)
        , m_AlgList(Tcast(pObj)->m_AlgList){
    }
    MFXExtBufferPtr(const MFXExtBufferVector & ref_obj)
        : _MFXExtBufferPtr<mfxExtVPPDoUse>(ref_obj)
        , m_AlgList(Tcast(ref_obj.get_by_id(id))->m_AlgList){
    }
    MFXExtBufferPtr(const MFXExtBufferPtr & other)
        : _MFXExtBufferPtr<mfxExtVPPDoUse> (other.get()){

    }
    //TODO: consider to remove reference in that case we can reflect changes not very quickly but the performance
    //increased dramatically
    MFXExtBufferPtrRef<mfxExtVPPDoUse> operator -> ();
    MFXExtBufferPtrRef<mfxExtVPPDoUse> operator *  ();
};

template<>
class MFXExtBufferPtrRef<mfxExtVPPDoUse>
{
public :
    MFXExtBufferPtrRef(MFXExtBufferPtr<mfxExtVPPDoUse> *pOwner)
        : DECL_ELEMENTS(AlgList, NumAlg)
    {
    }
    MFXExtBufferPtrRef * operator ->()
    {
        return this;
    }
    ElementsVector<mfxU32> AlgList;
};


//////////////////////////////////////////////////////////////////////////
//mfxExtMVCSeqDesc specification


template <>
class MFXExtBufferPtr<mfxExtMVCSeqDesc> 
    : public _MFXExtBufferPtr<mfxExtMVCSeqDesc>
{
    template<class T>
    friend class MFXExtBufferPtrRef;

    IMPLEMENT_CLONE(MFXExtBufferPtr);
protected:
    vector_with_ptr<mfxMVCViewDependency> m_View;
    vector_with_ptr<mfxU16> m_ViewId;
    vector_with_ptr<mfxMVCOperationPoint> m_OP;
public:
    MFXExtBufferPtr(element_type * pObj = NULL)
        : _MFXExtBufferPtr<mfxExtMVCSeqDesc>(pObj){
    }
    MFXExtBufferPtr(element_type_ext * pObj )
        : _MFXExtBufferPtr<mfxExtMVCSeqDesc>(pObj){
    }
    MFXExtBufferPtr(MFXExtBufferPtrBase * pObj)
        :_MFXExtBufferPtr<mfxExtMVCSeqDesc>(pObj){
        AssignRefs(pObj);
    }
    MFXExtBufferPtr(const MFXExtBufferVector & ref_obj)
        : _MFXExtBufferPtr<mfxExtMVCSeqDesc>(ref_obj){
            AssignRefs(ref_obj.get_by_id(id));
    }
    MFXExtBufferPtr(const MFXExtBufferPtr & other)
        : _MFXExtBufferPtr<mfxExtMVCSeqDesc>(other.get()){
            copy_local(reinterpret_cast<element_type*>(other.get()));
    }

    //TODO: consider to remove reference in that case we can reflect changes not very quickly but the performance
    //increased dramatically
    MFXExtBufferPtrRef<mfxExtMVCSeqDesc> operator -> ();
    MFXExtBufferPtrRef<mfxExtMVCSeqDesc> operator *  ();

    virtual void reset(element_type*);
    virtual void reset(element_type_ext *);

    ///updates vectors if elements were inserted using c array of vector storage
    virtual void restore_consistency();

protected:
    //copies elements given by poiter, to own structures
    void copy_local(element_type*);
    void AssignRefs(MFXExtBufferPtrBase * ref_obj)
    {
        MFXExtBufferPtr *pOther = Tcast(ref_obj);
        if (NULL == pOther)
            return;
        m_View.attach(pOther->m_View);
        m_ViewId.attach(pOther->m_ViewId);
        m_OP.attach(pOther->m_OP);
    }
};


//using of this class instead of mfxExtMVCSeqDesc structure
//eliminates necessity of specifying len and alloclen of every buffer
template<>
class MFXExtBufferPtrRef<mfxExtMVCSeqDesc>
{
public:
    MFXExtBufferPtrRef(MFXExtBufferPtr<mfxExtMVCSeqDesc> *pOwner)
        : DECL_MFXU32_REF(NumOP)
        , DECL_ELEMENTS_ALLOCATED(OP, NumOP, NumOPAlloc)
        , DECL_MFXU32_REF(NumViewId)
        , DECL_ELEMENTS_ALLOCATED(View, NumView, NumViewAlloc)
        , DECL_MFXU32_REF(NumView)
        , DECL_ELEMENTS_ALLOCATED(ViewId, NumViewId, NumViewIdAlloc)
    {
    }

    MFXExtBufferPtrRef * operator -> ()
    {
        return this;
    }

    //output parameter
    //integers are not used internally only filled by Mediasdk
    mfxU32 &NumOP;
    ElementsVector<mfxMVCOperationPoint> OP;
    mfxU32 &NumViewId;
    ElementsVector<mfxMVCViewDependency> View;
    mfxU32 &NumView;
    ElementsVector<mfxU16> ViewId;
    //mfxU16 CompatibilityMode;  /* 0 - hardware, 1 - software */
    //mfxU32 Reserved[16];
protected:
    //need to have a reserved value if target has zero buffer
    mfxU32 m_NumOP;
    mfxU32 m_NumViewId;
    mfxU32 m_NumView;
    
    //warning prevention only due to references in members
    MFXExtBufferPtrRef & operator = (const MFXExtBufferPtrRef &)
    {
        return *this;
    }
};





#endif//MFX_EXTENDED_BUFFER_H   
