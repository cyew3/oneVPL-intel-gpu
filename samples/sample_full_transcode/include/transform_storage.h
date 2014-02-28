//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

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
