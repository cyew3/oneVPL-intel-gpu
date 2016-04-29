/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once

#include <vector>
#include "itransform.h"
#include <map>
#include <algorithm>

class TransformConfigDesc {
    int m_trackID;
    MFXAVParams m_avParam;
public:
    TransformConfigDesc(int trackID, const MFXAVParams &avParam)
        : m_trackID(trackID)
        , m_avParam(avParam) {
    }
    int TrackID() const {
        return m_trackID;
    }
    const MFXAVParams& Params() const {
        return m_avParam;
    }
    MFXAVParams& Params() {
        return m_avParam;
    }
};

class TransformStorage {
    typedef std::map<int, std::vector<ITransform*> > Container_t;
    typedef std::map<int, std::vector<TransformConfigDesc> > InitContainer_t;
    Container_t m_transforms;
    InitContainer_t m_transformsInit;

public:
    typedef Container_t::value_type::second_type::iterator iterator;

    virtual ~TransformStorage() {
        for (Container_t::iterator it = m_transforms.begin(); it != m_transforms.end() ; ++it) {
           std::transform(it->second.rbegin(), it->second.rend(), it->second.rbegin(), DeletePtr());
        }
    }
    //takes ownership over this object
    virtual void RegisterTransform(const TransformConfigDesc &desc, std::auto_ptr<ITransform> & transform) {
        // TO DO: check for duplication
        m_transforms[desc.TrackID()].push_back(transform.get());
        m_transformsInit[desc.TrackID()].push_back(desc);
        transform.release();
    }
    //initializes transforms according to their order
    virtual void Resolve() {
        for (Container_t::iterator i = m_transforms.begin(); i != m_transforms.end(); i++ ) {
            //initialize transforms chains for each ID with links to next transform in same chain
            ITransform *pNext = NULL;
            for (size_t j = i->second.size(); j > 0; j--) {
                ITransform & trans = *(i->second[j - 1]);
                TransformConfigDesc &desc = m_transformsInit[i->first][j - 1];
                trans.Configure(desc.Params(), pNext);
                pNext = &trans;
            }
        }
    }
    //creates iterator for transform components that particular sample has to pass through
    virtual iterator begin(int id) {
        return m_transforms[id].begin();
    }
    virtual iterator end(int id) {
        return m_transforms[id].end();
    }
    virtual bool empty(int id) {
        return  m_transforms.find(id) == m_transforms.end();
    }
};
