/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

//used for shared acess to non threadsafety parts of pipeline
class IPipelineSynhro
{
public:
    virtual ~IPipelineSynhro(){}
    virtual void Enter() = 0;
    virtual void Leave() = 0;
};

class AutoPipelineSynhro
{

public: 
    AutoPipelineSynhro(IPipelineSynhro*p)
        : m_p(p)
    {
        if (p)
            p->Enter();
    }
    ~AutoPipelineSynhro()
    {
        if (m_p)
            m_p->Leave();
    }

    IPipelineSynhro * m_p;
};