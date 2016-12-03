/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */


#pragma once

#include "mfx_iyuv_source.h"

//encapsulate mvc specific buffers behavior 
//can be attached to any decoders, or encoders, but intended for MVC decoder
template<class T>
class MVCHandlerCommon 
    : public InterfaceProxy<T>
{
    typedef InterfaceProxy<T> base;
public:
    //external buffer since pipeline uses it's information to setup followed filters
    MVCHandlerCommon  ( MFXExtBufferVector &extBufferVector, bool bForceMVCDetection, std::auto_ptr<T>& pTarget)
          : base(pTarget)
          , m_extParams(extBufferVector)
          , m_bForceMVCDetection(bForceMVCDetection)
    {
    }

    virtual mfxStatus GetVideoParam(mfxVideoParam *par) 
    {
        std::mem_fun1_t<mfxStatus, MVCHandlerCommon, mfxVideoParam*> fnc(&MVCHandlerCommon::GetVideoParamBase);
        
        m_extParams_current.clear();
        
        mfxStatus sts;
        //detaching ext seq buffer 
        //TODO: for some cases better to copy them back, it is not implemented
        {
            auto_ext_buffer auto_buffer(*par);
            auto_buffer.remove(BufferIdOf<mfxExtMVCSeqDesc>::id);

            MFX_CHECK_STS (sts = ProcessVideoParam(par, m_extParams_current, std::bind1st(fnc, this)));
        }

        //out own internal buffer hope no one will try to delete it
        m_extParams_returned.clear();
        for (mfxU32 i = 0; i<par->NumExtParam; i++)
        {
            m_extParams_returned.push_back(par->ExtParam[i]);
        }

        //for get video param buffers are passed externally since we cannot know whether they are intended to be included or not
        //TODO: think of it
        m_extParams_current.push_back(new mfxExtMVCTargetViews());
        par->NumExtParam = (mfxU16)m_extParams_current.size();
        par->ExtParam= &m_extParams_current;
        //encoder cannot understand this buffer: we cannot guarantee certain error code for this
        
        /*MFX_CHECK_STS_SKIP*/base::GetVideoParam(par);

        MFXExtBufferPtr<mfxExtMVCTargetViews> tgViews(m_extParams_current);
        if (0 == tgViews->NumView)
        {
            //detaching this ext buffer
            m_extParams_current.pop_back();
        }

        //TODO: possible case when both provided and existed buffers required, example MVC encoding + excCodingOptions
        //we do not provide special buffers externally, lets restore original buffers
        m_extParams_returned.insert(m_extParams_returned.end(), &m_extParams_current, &m_extParams_current + m_extParams_current.size());

        par->NumExtParam = (mfxU16)m_extParams_returned.size();
        par->ExtParam = m_extParams_returned.empty() ? 0 : &m_extParams_returned.front();

        return sts;
    }
    //number of surfaces should match MVC
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
    {
        auto_ext_buffer auto_buf(*par);

        //copying MVC buffers if any
        auto_buf.insert(m_extParams.begin(), m_extParams.end());

        mfxStatus sts ;
        MFX_CHECK_STS(sts = base::QueryIOSurf(par, request)); 

        return sts;
    }

protected:

    mfxStatus GetVideoParamBase(mfxVideoParam *pParam)
    {
        return base::GetVideoParam(pParam);
    }

    template <class TFnc>
    mfxStatus ProcessVideoParam(mfxVideoParam *par, MFXExtBufferVector  &extParams, TFnc functor)
    {
        mfxStatus sts = MFX_ERR_NONE;
        auto_ext_buffer auto_buf(*par);

        for (;;)
        {
            //buffer may contain data
            MFX_CHECK_STS_SKIP( sts = functor(par)
                              , MFX_ERR_MORE_DATA
                              , MFX_ERR_NOT_ENOUGH_BUFFER);

            if (MFX_ERR_NONE <= sts)
            {
                //in case of MVC codec profile specified we will continue decode header to get info
                if (MFX_PROFILE_AVC_MULTIVIEW_HIGH == par->mfx.CodecProfile ||
                    MFX_PROFILE_AVC_STEREO_HIGH == par->mfx.CodecProfile || 
                    m_bForceMVCDetection)
                {
                    MFXExtBufferPtr<mfxExtMVCSeqDesc> seqDescription(extParams);

                    //buffer was attached and decode header completed successfully
                    if (seqDescription.get())
                    {
                        break;
                    }

                    extParams.push_back(new mfxExtMVCSeqDesc());
                    auto_buf.insert(extParams.begin(), extParams.end());
                    continue;
                }

                break;
            }

            //mvc specific buffers allocation
            if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
            {
                MFXExtBufferPtr<mfxExtMVCSeqDesc> seqDescription(extParams);

                //no buffer at all, don't know how to handle MFX_ERR_NOT_ENOUGH_BUFFER in this situation
                MFX_CHECK_POINTER(seqDescription.get());

                MFXExtBufferPtrRef<mfxExtMVCSeqDesc> seqDescriptionRef(*seqDescription);

                //we cannot create two ref objects because after committing it is possible to overwrite parameters
                seqDescriptionRef->View.reserve(seqDescriptionRef->NumView);
                seqDescriptionRef->ViewId.reserve(seqDescriptionRef->NumViewId);
                seqDescriptionRef->OP.reserve(seqDescriptionRef->NumOP);

                //we have enough bitstream data for now
                continue;
            }

            break;
        }
        
        return sts;
    }

    MVCHandlerCommon ( MVCHandlerCommon & that)
        : base (that)
        , m_extParams(that.m_extParams)
    {
    }
    MVCHandlerCommon & operator = ( MVCHandlerCommon & /*that*/)
    {
        return *this;
    }

    MFXExtBufferVector  &m_extParams;
    MFXExtBufferVector   m_extParams_current;
    std::vector<mfxExtBuffer*>   m_extParams_returned;
    bool                 m_bForceMVCDetection;
};


template <class T>
class MVCHandler
    : public MVCHandlerCommon<T>
{
public:
    MVCHandler( MFXExtBufferVector &extBufferVector, bool bForceMVCDetection, std::auto_ptr<T>& pTarget)
        : MVCHandlerCommon<T>(extBufferVector, bForceMVCDetection, pTarget)
    {
    }
};

template <>
class MVCHandler<IYUVSource>
    : public MVCHandlerCommon<IYUVSource>
{
    typedef MVCHandlerCommon<IYUVSource> base;
public:
    MVCHandler( MFXExtBufferVector &extBufferVector, bool bForceMVCDetection, std::auto_ptr<IYUVSource>& pTarget)
        : base(extBufferVector, bForceMVCDetection, pTarget)
    {
    }
    virtual mfxStatus Init(mfxVideoParam *par) 
    { 
        auto_ext_buffer auto_buf(*par);

        //copying MVC buffers if any
        auto_buf.insert(m_extParams.begin(), m_extParams.end());

        mfxStatus sts ;
        MFX_CHECK_STS(sts = base::Init(par)); 

        return sts;
    }
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) 
    {
        mem_fun2_defA<mfxStatus, MVCHandler, mfxBitstream*, mfxVideoParam*> fnc(&MVCHandler::DecodeHeaderBase, bs);
        mfxStatus sts;

        MFX_CHECK_STS_SKIP (sts = ProcessVideoParam(par, m_extParams, std::bind1st(fnc, this)), MFX_ERR_MORE_DATA);

        MFXExtBufferPtr<mfxExtMVCSeqDesc> seqDescription(m_extParams);

        //buffer was attached and decode header completed successfully
        if (seqDescription.get())
        {
            PrintInfo(VM_STRING("Video"), VM_STRING("MVC"));
            PrintInfo(VM_STRING("Number of Views"), VM_STRING("%d"), seqDescription->NumView);
        }

        return sts;
    }

protected:
    mfxStatus DecodeHeaderBase(mfxBitstream *pBs, mfxVideoParam *pParam)
    {
        return base::DecodeHeader(pBs, pParam);
    }
};