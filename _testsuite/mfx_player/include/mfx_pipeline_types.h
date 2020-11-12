/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2020 Intel Corporation. All Rights Reserved.

File Name: mfx_pipeline_types.h

*******************************************************************************/

#pragma  once
#include <functional>
//todo: fix includes
typedef std::basic_string<vm_char>tstring;
typedef std::basic_stringstream<vm_char> tstringstream;

#if !defined(_WIN32) && !defined(_WIN64)
    #if defined(UNICODE) || defined(_UNICODE)
        #define _totlower towlower
    #else
        #define _totlower tolower
    #endif
#endif

#include "mfx_pipeline_utils.h"
#include "mfx_opt_param_type.h"
#include "mfx_timeout.h"

//local version of codecid since mediasdk has unnamed union
struct FourCC
{
    mfxU32 m_id;
    FourCC(mfxU32 nId)
        : m_id(nId){}
};

enum MFXBufType
{
    MFX_BUF_UNSPECIFIED = 0,
    MFX_BUF_HW,
    MFX_BUF_SW,
    MFX_BUF_OPAQ,
    MFX_BUF_HW_DX11,
};

//whether to use fifo approach in selecting free surfaces, valid for dec and vpp
enum eSurfacesSearchAlgo
{
    USE_FIRST = 0,
    USE_OLDEST_DIRECT,
    USE_OLDEST_REVERSE,
    USE_RANDOM
};

//BasePipeLineError codes
enum 
{
    PE_NO_ERROR         = 0,
    PE_UNKNOWN_ERROR,
    PE_INV_INPUT_FILE,
    PE_INV_OUTPUT_FILE,
    PE_INV_RENDER_TYPE,
    PE_INV_LIB_TYPE,
    PE_INV_BUF_TYPE,
    PE_C_FMT_THIS_MODE,
    PE_C_FMT_MUST_THIS_MODE,
    PE_CHECK_PARAMS,
    PE_CREATE_SPL,
    PE_CREATE_ALLOCATOR,
    PE_CREATE_VPP,
    PE_CREATE_DEC,
    PE_CREATE_RND,
    PE_INIT_CORE,
    PE_DECODE_HEADER,
    PE_PAR_FILE,
    PE_OPTION
};

//! Base class for types that should not be assigned.
class mfx_no_assign  {
    // Deny assignment
    void operator=( const mfx_no_assign& );
public:
#if __GNUC__
    //! Explicitly define default construction, because otherwise gcc issues gratuitous warning.
    mfx_no_assign() {}
#endif /* __GNUC__ */
};

//! Base class for types that should not be copied or assigned.
class mfx_no_copy: mfx_no_assign {
    //! Deny copy construction
    mfx_no_copy( const mfx_no_copy& );
public:
    //! Allow default construction
    mfx_no_copy() {}
};

struct SrfEncCtl
{
    mfxFrameSurface1 *pSurface;
    mfxEncodeCtrl    *pCtrl;

    SrfEncCtl(mfxFrameSurface1 * pSrf = NULL, mfxEncodeCtrl *pCtl = NULL)
        : pSurface(pSrf)
        , pCtrl(pCtl) 
    {}
};

struct SrfEncCtlDelete : public std::unary_function<SrfEncCtl,void> 
{
    void operator ()(SrfEncCtl surface)
    {
        delete surface.pSurface;
        if (NULL != surface.pCtrl)
        {
            MFX_DELETE(surface.pCtrl->ExtParam[0]);
            MFX_DELETE_ARRAY(surface.pCtrl->ExtParam);
            MFX_DELETE(surface.pCtrl);
        }
    }
};

struct BufferIdCompare
    : public std::binary_function<mfxExtBuffer*, mfxU32 , bool>
{
    bool operator ()(mfxExtBuffer* a, mfxU32 bufferid) const
    {
        if (NULL == a )
            return false;
        return a->BufferId == bufferid;
    }
};

struct mfxExtBufferIdEqual
    : public std::binary_function<mfxExtBuffer*, mfxExtBuffer* , bool>
{
    bool operator ()(mfxExtBuffer* a, mfxExtBuffer *b) const
    {
        if (NULL == a || NULL == b)
            return false;

        return a->BufferId == b->BufferId;
    }
};

struct mfxExtBufferIdCompare
    : public std::binary_function<mfxExtBuffer*, mfxExtBuffer* , bool>
{
    bool operator ()(mfxExtBuffer* a, mfxExtBuffer *b) const
    {
        if (NULL == a || NULL == b)
            return false;

        return a->BufferId < b->BufferId;
    }
};

typedef struct
{
    const vm_char *pRefFile;
    //mfxU32      nNumFrames;
    bool        bVerbose;
    bool        bZeroBottomStripe;
    mfxU32      nDelayOnMSDKCalls; //pipeline will call Sleep after MSDK async calls
    mfxU16      nBufferSizeInKB;
} EncodeExtraParams;


//necessary if u want to disableTrace for particular function
class auto_trace_disabler
{   
    mfxU32 m_nLastWTrace;
    mfxU32 m_nLastITrace;
    mfxU32 m_nLastETrace;
    bool   m_bTurnOff;
    bool  m_bTurnOffErrors;

public:
    auto_trace_disabler(bool bTurnTraceOff, bool bTurnOffErrors = false) :
        m_nLastWTrace(0),
        m_nLastITrace(0),
        m_nLastETrace(0)
    {
        if (true == (m_bTurnOff = bTurnTraceOff))
        {
            m_nLastWTrace = PipelineSetTrace(PROVIDER_WRN, TRACE_NONE);
            m_nLastITrace = PipelineSetTrace(PROVIDER_INFO, TRACE_NONE);
        }
        if (true == (m_bTurnOffErrors = bTurnOffErrors))
        {
            m_nLastETrace = PipelineSetTrace(PROVIDER_ERROR, TRACE_NONE);
        }
    }
    ~auto_trace_disabler()
    {
        if (m_bTurnOff)
        {
            PipelineSetTrace(PROVIDER_WRN, m_nLastWTrace);
            PipelineSetTrace(PROVIDER_INFO,  m_nLastITrace);
        }
        if (true == (m_bTurnOffErrors = m_bTurnOffErrors))
        {
            PipelineSetTrace(PROVIDER_ERROR, m_nLastETrace);
        }
    }
};

class auto_ext_buffer_base
{
public:
    auto_ext_buffer_base()
        : m_pOldExtBuf()
        , m_nOldExtBuf()
        , m_bPushed()
        , m_numExParamsRef()
        , m_ppExtParamsRef()

    {
    }
    virtual ~auto_ext_buffer_base()
    {
        pop();
    }

    void insert(mfxExtBuffer* bufToInsert) {
        if (!m_bPushed ) return;

        IntergityMaintainer maintain(*this);
        //copying inserted buffers
        m_final_buffer.push_back(bufToInsert);
        //keep unique buffers
        std::sort(m_final_buffer.begin(), m_final_buffer.end(), mfxExtBufferIdCompare());
        m_final_buffer.erase(std::unique(m_final_buffer.begin(), m_final_buffer.end(), mfxExtBufferIdEqual()), m_final_buffer.end());
    }

    //temporally inserts more buffers, will remove this on exiting
    void insert(std::vector<mfxExtBuffer*>::iterator start,
                std::vector<mfxExtBuffer*>::iterator end)
    {
        if (m_bPushed )
        {
            IntergityMaintainer maintain(*this);
            //copying inserted buffers
            m_final_buffer.insert(m_final_buffer.end(), start, end);
            //keep unique buffers
            std::sort(m_final_buffer.begin(), m_final_buffer.end(), mfxExtBufferIdCompare());
            m_final_buffer.erase(std::unique(m_final_buffer.begin(), m_final_buffer.end(), mfxExtBufferIdEqual()), m_final_buffer.end());
        }
    }

    void remove(mfxU32 buffedID)
    {
        if (m_bPushed )
        {
            IntergityMaintainer maintain(*this);
            m_final_buffer.erase(std::remove_if(m_final_buffer.begin(), m_final_buffer.end(), std::bind2nd(BufferIdCompare(), buffedID)), m_final_buffer.end());
        }
    }

    template <class T>
    void push(T &par)
    {
        if (m_bPushed)
            return;

        m_pOldExtBuf = par.ExtParam;
        m_nOldExtBuf = par.NumExtParam;
        
        m_ppExtParamsRef = &par.ExtParam;
        m_numExParamsRef = &par.NumExtParam;

        m_bPushed = true;
    }


    void pop()
    {
        if (!m_bPushed)
            return;

        *m_ppExtParamsRef = m_pOldExtBuf;
        *m_numExParamsRef = m_nOldExtBuf;
        m_bPushed              = false;
    }
protected:
    friend class IntergityMaintainer;

    class IntergityMaintainer  : mfx_no_copy
    {
        auto_ext_buffer_base & m_Owner;
    public:
        IntergityMaintainer(auto_ext_buffer_base &auto_ext)
            : m_Owner(auto_ext)
        {
            if (!m_Owner.m_bPushed)
                return;

            //recreating final buffer
            m_Owner.m_final_buffer.clear();
            //copying original buffers
            m_Owner.m_final_buffer.insert(m_Owner.m_final_buffer.end(), m_Owner.m_pOldExtBuf, m_Owner.m_pOldExtBuf + m_Owner.m_nOldExtBuf);
        }

        ~IntergityMaintainer()
        {
            if (!m_Owner.m_bPushed)
                return;

            //assign
            *m_Owner.m_numExParamsRef = (mfxU16)m_Owner.m_final_buffer.size();
            *m_Owner.m_ppExtParamsRef = m_Owner.m_final_buffer.empty() ? NULL : &m_Owner.m_final_buffer.front();
        }
    };

    mfxExtBuffer **m_pOldExtBuf;
    mfxU16 m_nOldExtBuf; 
    
    bool m_bPushed;
    
    mfxU16          *m_numExParamsRef;
    mfxExtBuffer  ***m_ppExtParamsRef;

    std::vector<mfxExtBuffer*> m_final_buffer;
};

//push always if buffers array not empty
class auto_ext_buffer : public auto_ext_buffer_base {
public:
    template <class T>
    auto_ext_buffer(T &par) {
        push(par);
    }
};

//push only if buffers array not empty
class auto_ext_buffer_if_exist : public auto_ext_buffer_base {
public:
    template <class T>
    auto_ext_buffer_if_exist(T &par) {
        if (0 != par.NumExtParam && 0 != par.ExtParam) {
            push(par);
        }
    }
};

class auto_ext_buffer_remove : public auto_ext_buffer_base {
public:
    template <class T>
    auto_ext_buffer_remove(T &par, mfxU32 IdToRemove) {
        push(par);
        remove(IdToRemove);
    }
};


template <class T>
struct deleter 
    : public std::unary_function<T, void> 
{
    void operator ()(T pObj)
    {
        delete pObj;
    }
};


template<class TCaller>
class second_of_t
{
    TCaller m_caller;
public:
    second_of_t(TCaller external = TCaller())
        : m_caller(external){}
    template <class T> 
    typename TCaller::result_type  operator ()(T & obj)
    {
        return m_caller(obj.second);
    }
    template <class T> 
    typename TCaller::result_type operator ()(const T & obj)
    {
        return m_caller(obj.second);
    }
};

template<class TCaller>
second_of_t<TCaller> second_of(TCaller ext)
{
    return second_of_t<TCaller>(ext);
}

template<class TCaller>
class first_of_t
{
    TCaller m_caller;
    
public:
    typedef typename TCaller::result_type result_type;

    first_of_t(TCaller external = TCaller())
        : m_caller(external){}
    
    template <class T> result_type operator ()(T & obj)
    {
        return m_caller(obj.first);
    }
};

template<class TCaller>
first_of_t<TCaller> first_of(TCaller ext)
{
    return first_of_t<TCaller>(ext);
}

template <class T>
class Maximum
    : public std::unary_function<T, void>
{
    T * max_value;
public:
    Maximum(T & external_max)
        : max_value(&external_max){}

    void operator ()(const T & arg)
    {
        *max_value = (std::max)(*max_value, arg);
    }
};

//TODO: need a generalized solution
template <class T>
class CMPPairs1st
    : public std::binary_function<std::pair<T,T>, std::pair<T,T>, bool>
{
    
public:
    bool operator ()(const std::pair<T, T> & arg, const std::pair<T, T> & arg2)
    {
        return arg.first == arg2.first;
    }
};

template <class T>
class CMPPairs2nd
    : public std::binary_function<std::pair<T,T>, std::pair<T,T>, bool>
{

public:
    bool operator ()(const std::pair<T, T> & arg, const std::pair<T, T> & arg2)
    {
        return arg.second == arg2.second;
    }
};

template <class iterator, class pred>
bool HasDuplicates(iterator start, iterator end, pred pr)
{
    iterator it = start;
    for (; it != end; it++)
    {
        iterator jt = it;
        for (jt ++; jt != end; jt++)
        {
            //matched elements found
            if (pr(*jt, *it))
            {
                return true;
            }
        }
    }
    return false;
}

//operator () assign value to particular member
template<class T, class TMember>
class mem_var_setter
    : public std::binary_function<T , TMember, void>
{
public:
    explicit mem_var_setter(TMember T::*_Pm)
        : m_memvar(_Pm)
        {
        }

    void operator()(T & obj, TMember newValue) const
    {
        obj.*m_memvar = newValue;
    }
private:
    TMember T::*m_memvar;   // the member variable pointer
};

//preserve VIA flags
template <class T>
struct mem_var_setter<T, mfxIMPL>
    : public std::binary_function<T, mfxIMPL, void>
{
    explicit mem_var_setter(mfxIMPL T::*_Pm)
    : m_memvar(_Pm)
    {   // construct from pointer
    }

    void operator ()(T& obj, mfxIMPL impl)const
    {
        mfxU32 impl_new = impl & ~(-MFX_IMPL_VIA_ANY);
        mfxU32 via_flags_new = impl & (-MFX_IMPL_VIA_ANY);

        //impl reused if non zero , or zero and no via flags
        bool bUseImpl = (impl_new) || (!via_flags_new);
        
        //via reused if non zero , or zero and no impl iz zero
        bool bUseVia  = (via_flags_new) || !impl_new;

        mfxU32 impl_old = obj.*m_memvar & ~(-MFX_IMPL_VIA_ANY);
        mfxU32 via_flags_old = obj.*m_memvar & (-MFX_IMPL_VIA_ANY);
        
        //new via assigned if it is not zero
        obj.*m_memvar  = (bUseImpl ? impl_new : impl_old) | (bUseVia ? via_flags_new : via_flags_old);
    }

private:
    mfxIMPL T::*m_memvar;   // the member variable pointer
};


template <class T, class TMember, class TAny/*is convertable by static cast to TMember*/>
std::binder2nd<mem_var_setter<T, TMember> >
mem_var_set(TMember T::*mem,  TAny value)
{
    mem_var_setter<T, TMember> setter(mem);
    return std::bind2nd(setter, static_cast<TMember>(value));
};


template<class T, class TMember>
class mem_var_isequal
    : public std::binary_function<T , TMember, bool>
{
public:
    explicit mem_var_isequal(TMember T::*_Pm)
        : m_memvar(_Pm)
        {   // construct from pointer
        }

    bool operator()(T & obj, TMember value) const
    {
        return obj.*m_memvar == value;
    }
private:
    TMember T::*m_memvar;   // the member function pointer
};

template<class chType>
class StringConcat
{
    std::basic_stringstream<chType> m_input;

public:
    StringConcat(const std::basic_string<chType> & input)
        : m_input(input)
    {
    }
    StringConcat & operator +(const std::basic_string<chType> & input)
    {
        m_input<<input;
        return *this;
    }
    std::basic_string<chType> str()
    {
        return m_input.str();
    }
};

#include "mfx_mem_fun_any.h"



template <class Functor, class FunctorBool>
class Untill_t
{
public:
    //typedef std::unary_function<typename Functor::result_type, bool> FunctorBool; 
    Untill_t (Functor fnc, FunctorBool fncBool)
        : m_fnc (fnc)
        , m_fncBool(fncBool)
    {
    }

    bool operator ()(typename Functor::argument_type op)
    {
        return m_fncBool(m_fnc(op));
    }
protected:
    Functor m_fnc;
    FunctorBool m_fncBool;
};

template <class Functor , class FunctorBool>
Untill_t<Functor, FunctorBool>
Untill (Functor fnc, FunctorBool fncBool )
{
    return  Untill_t<Functor , FunctorBool>(fnc, fncBool);
}


//true if error
struct NoMfxErr
    : public std::unary_function<mfxStatus, bool>
{
    bool operator () (mfxStatus sts)
    {
        return sts != MFX_ERR_NONE;
    }
};


//for generating number sequence 
template <typename T>
struct sequence {
    T x;
    //default initializer 0 for integer types
    sequence(T seed = T()) : x(seed) { }
    T operator ()() 
    { 
        return x++;
    }
};

//class Version of mfxFrameInfo
class mfxFrameInfoCpp
{
    mfxFrameInfo m_Info;
public:
    mfxFrameInfoCpp(mfxU16  TemporalId = 0,
                    mfxU16  PriorityId = 0,
                    mfxU16  DependencyId = 0,
                    mfxU16  QualityId = 0,
                    mfxU32  FourCC = 0,
                    mfxU16  Width = 0,
                    mfxU16  Height = 0,
                    mfxU16  CropX = 0,
                    mfxU16  CropY = 0,
                    mfxU16  CropW = 0,
                    mfxU16  CropH = 0,
                    mfxU32  FrameRateExtN = 0,
                    mfxU32  FrameRateExtD = 0,
                    mfxU16  AspectRatioW = 0,
                    mfxU16  AspectRatioH = 0,
                    mfxU16  PicStruct = 0,
                    mfxU16  ChromaFormat = 0)
    {
         m_Info = {};
         m_Info.FrameId.DependencyId = DependencyId;
         m_Info.FrameId.PriorityId = PriorityId;
         m_Info.FrameId.QualityId = QualityId;
         m_Info.FrameId.TemporalId = TemporalId;
         m_Info.FourCC = FourCC;
         m_Info.Width = Width;
         m_Info.Height = Height;
         m_Info.CropX = CropX;
         m_Info.CropY = CropY;
         m_Info.CropW = CropW;
         m_Info.CropH = CropH;
         m_Info.FrameRateExtN = FrameRateExtN;
         m_Info.FrameRateExtD = FrameRateExtD;
         m_Info.AspectRatioW = AspectRatioW;
         m_Info.AspectRatioH = AspectRatioH;
         m_Info.PicStruct = PicStruct;
         m_Info.ChromaFormat = ChromaFormat;
    }
    operator const mfxFrameInfo &()const
    {
        return m_Info;
    }
    operator mfxFrameInfo &()
    {
        return m_Info;
    }

};
