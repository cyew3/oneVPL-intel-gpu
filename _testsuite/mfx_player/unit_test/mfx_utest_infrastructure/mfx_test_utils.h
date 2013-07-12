/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ipipeline_config.h"
#include "mfx_ibitstream_reader.h"

//warning C4345: behavior change: an object of POD type constructed with an initializer of the form () will be default-initialized
#pragma warning (disable:4345)

#define CMD_PARAM(param, value)\
    params[VM_STRING(param)].push_back(VM_STRING(value));


//only limittion that interface has to be defined as proxyied 
template<class T>
class UnDeletableProxy 
    : public InterfaceProxy<T>
{
public:
    UnDeletableProxy ( std::auto_ptr<T> &pTarget)
        : InterfaceProxy<T>(pTarget)
    {
    }
    ~UnDeletableProxy()
    {
        m_pTarget.release();
    }
};

template<>
class UnDeletableProxy <IMFXVideoVPP>
    : public InterfaceProxy<IMFXVideoVPP>
{
public:
    UnDeletableProxy ( std::auto_ptr<IMFXVideoVPP> &pTarget)
        : InterfaceProxy<IMFXVideoVPP>(pTarget.release())
    {
    }
    ~UnDeletableProxy()
    {
        m_pTarget.release();
    }
};

template<>
class UnDeletableProxy <IBitstreamReader>
    : public InterfaceProxy<IBitstreamReader>
{
public:
    UnDeletableProxy ( std::auto_ptr<IBitstreamReader> &pTarget)
        : InterfaceProxy<IBitstreamReader>(pTarget.release())
    {
    }
    ~UnDeletableProxy()
    {
        m_pTarget.release();
    }
};


template<class T>
typename T::InterfaceType * MakeUndeletable(T& refTarget)
{
    std::auto_ptr<T::InterfaceType> tmp(&refTarget);
    return new UnDeletableProxy<T::InterfaceType>(tmp);
}

//to create short life time object valid for only 1 call 
template <class T>
inline std::auto_ptr<typename T::InterfaceType> & make_instant_auto_ptr(T* obj) {
    static std::auto_ptr<T::InterfaceType> x;
    x.reset(obj);
    return x;
}


class TestUtils
{
public:
    template <class T>
    static void CopyExtParamsEnabledStructures( T *pTo
                                              , T *pFrom
                                              , bool bCreateExtBuffer)
    {
        if (pTo && pFrom) 
        {
            //accurate copy of extended buffers
            if (pFrom->NumExtParam!=0)
            {
                CopyExtBuffers(*pTo, *pFrom, bCreateExtBuffer);
            }

            //copying other fields
            mfxU16 nExtBuffers = pTo->NumExtParam;
            mfxExtBuffer ** ppExtBuffers = pTo->ExtParam;

            *pTo = *pFrom;

            pTo->NumExtParam = nExtBuffers;
            pTo->ExtParam = ppExtBuffers;
        }
    }
    
    template <class T>
    static void CopyExtBuffers (T& pTo, T& pFrom, bool bCreate) {
        if (bCreate)
        {
            //creating buffers by allocating same memory that in source buffer
            pTo.NumExtParam = pFrom.NumExtParam;
            pTo.ExtParam = new mfxExtBuffer*[pFrom.NumExtParam];
            for (int i = 0; i < pFrom.NumExtParam; i++)
            {
                pTo.ExtParam[i] = (mfxExtBuffer*)new char[pFrom.ExtParam[i]->BufferSz];
                pTo.ExtParam[i]->BufferId = pFrom.ExtParam[i]->BufferId;
            }
        }

        for (int i = 0; i < pFrom.NumExtParam; i++)
        {
            //find matched buffers
            for (int j = 0; j < pTo.NumExtParam; j++)
            {
                if (pTo.ExtParam[j]->BufferId == pFrom.ExtParam[i]->BufferId)
                {
                    //corresponding buffer found
                    //lets copy content to this buffer
                    memcpy(pTo.ExtParam[j], pFrom.ExtParam[i], pFrom.ExtParam[i]->BufferSz);
                }
            }
        }
    }
};

template <class TPipelineConfig>
class PipelineRunner
{
public:
    typedef std::map <tstring, std::vector<tstring> > CmdLineParams;
    typedef std::multimap <tstring, std::vector<tstring> > MultiCmdLineParams;
    
    //can use either map or multimap params
    template <class T>
    mfxStatus ProcessCommand(T & params, IMFXPipelineConfig *pExternalCFG = NULL)
    {
        std::vector<const vm_char*> cmd_line_params;

        T::iterator i;
        for (i  = params.begin();
             i != params.end();
             i++)
        {
            if (0 != i->first.size())
                cmd_line_params.push_back(i->first.c_str());
            
            if (!i->second.empty())
            {
                //parsing second parameter
                for (size_t  j = 0; j < i->second.size(); j++)
                {
                    cmd_line_params.push_back(i->second[j].c_str());
                }
            }
        }
        vm_char** argv = const_cast<vm_char**>(&cmd_line_params.front());

        IMFXPipelineConfig * pCfg ;
        if (!pExternalCFG)
        {
            pCfg = new TPipelineConfig((int)cmd_line_params.size(), argv);
        }
        else
        {
            pCfg = pExternalCFG;
        }
        m_pPipeline.reset(pCfg->CreatePipeline());
        return m_pPipeline->ProcessCommand(argv, (int)cmd_line_params.size());
    }

    std::auto_ptr<IMFXPipeline>& operator ->()
    {
        return m_pPipeline;
    }

protected:
    std::auto_ptr<IMFXPipeline> m_pPipeline;
};