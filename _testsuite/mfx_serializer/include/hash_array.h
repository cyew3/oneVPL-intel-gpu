/******************************************************************************* *\

Copyright (C) 2010-2016 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: hash_array.h

*******************************************************************************/

#ifndef HASH_ARRAY
#define HASH_ARRAY

#include <vector>

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

    bool contains_substr(const TKey & key)
    {
        for(iterator it = base::begin(); it != base::end(); it++)
        {
            if (it->first.find(key) == 0)
                return true;
        }
        return false;
    }
};


#endif //HASH_ARRAY
