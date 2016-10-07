/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef MFX_MULTIRENDER_H
#define MFX_MULTIRENDER_H

#include "mfx_rnd_wrapper.h"

template <class T>
class MFXViewRender 
    : public InterfaceProxy<IMFXVideoRender>
{
    T m_id;
public :
    MFXViewRender(IMFXVideoRender * pRender, const T & view_id)
        : InterfaceProxy<IMFXVideoRender>(pRender)
        , m_id(view_id){
    }
    const T& ID() const {
        return m_id;
    }
};

template <class T>
class MFXViewRenderCompare
    : public std::binary_function<T, MFXViewRender<T>*, bool>
{
public:
    bool operator () (const T & refValue, const MFXViewRender<T> * pExaminee) const{
        return pExaminee ? (pExaminee->ID() == refValue) : false ;
    }
};

//prototypes based render
class MFXMultiRender
    : public InterfaceProxy<IMFXVideoRender>
{
    //underlined type for viewID
    typedef mfxU32 _TView;

    typedef std::list<MFXViewRender<_TView> * > _CollectionType;
    _CollectionType  m_Renders;

public:
    //in auto view mode multiple renderes will be created
    //else this render will only order surfaces prior rendering
    MFXMultiRender (IMFXVideoRender * pSingleRender);
    ~MFXMultiRender ();

    virtual mfxStatus RenderFrame( mfxFrameSurface1 *surface
                                 , mfxEncodeCtrl * pCtrl = NULL);
protected:
};


#endif // MFX_MULTIRENDER_H
