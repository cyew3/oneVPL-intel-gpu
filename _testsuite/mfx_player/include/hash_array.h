/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2013 Intel Corporation. All Rights Reserved.

File Name: hash_array.h

*******************************************************************************/

#ifndef HASH_ARRAY
#define HASH_ARRAY

#include <map>
#include <vector>
#include <utility>

//to create not sorted map, this will allow us to save registration order
template <class TKey, class TElement>
class hash_array 
    : public std::vector<std::pair<TKey, TElement> >
{
  
public:
    typedef std::vector<std::pair<TKey, TElement> > base;
    typedef typename base::iterator iterator;
    typedef typename base::const_iterator const_iterator;

    const TElement & operator [](const TKey & key)const
    {
        for(iterator it = base::begin(); it != base::end(); it++)
        {
            if (it->first == key)
                return it->second;
        }
        this->push_back(std::make_pair(key, TElement()));
        return base::back().second;
    }

    TElement & operator [](const TKey & key)
    {
        for(iterator it = base::begin(); it != base::end(); it++)
        {
            if (it->first == key)
                return it->second;
        }
        
        this->push_back(std::make_pair(key, TElement()));
        return base::back().second;
    }

    iterator find(const TKey & key)
    {
        for(iterator it = base::begin(); it != base::end(); it++)
        {
            if (it->first == key)
                return it;
        }
        return base::end();
    }
};


#endif //HASH_ARRAY
