/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include <Unknwn.h>
//rationale:
//sometimes in testing it is suitable to have IUnknown implementation without having implicit
template <class T>
class IUnknownAdapter
    : public IUnknown
{
    T* m_obj;
    int m_c;
public:
    IUnknownAdapter(T * obj) 
      : m_obj(  obj)
      , m_c()
      {}

    virtual HRESULT STDMETHODCALLTYPE QueryInterface( 
    /* [in] */ REFIID riid,
    /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject) {
        *ppvObject = this;
        return S_OK;
    }

    virtual ULONG STDMETHODCALLTYPE AddRef( void){
        return m_c++;
    }
    
    virtual ULONG STDMETHODCALLTYPE Release( void){
        return m_c--;
    }
};