// Copyright (c) 2018-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __BS_MEM_H
#define __BS_MEM_H

#include <vector>
#include <map>
#include <set>
#include <memory.h>
#include <bs_def.h>
#include <malloc.h>

#define BS_ALLOC_DEBUG 0

using namespace std;

class BS_Allocator;

typedef struct _mem_descriptor{
#if BS_ALLOC_DEBUG
    const char* name;
    const char* file;
    Bs32u line;
#endif
    Bs32u locked    : 31;
    Bs32u to_delete : 1;
    Bs32u size;
    set<void*> base;
    vector<map<void*, _mem_descriptor>::iterator> dependent;
    void (*destructor)(void*, BS_Allocator*);
}mem_descriptor;

class BS_Allocator{
public:
    BS_Allocator(){};
    ~BS_Allocator(){
        while(!mem.empty()){
            /*mem.begin()->second.locked = 0;
            mem.begin()->second.base.clear();
            mem.begin()->second.dependent.clear();
            free(mem.begin()->first);*/
            if(mem.begin()->first)
                ::free(mem.begin()->first);
            mem.erase(mem.begin());
        }
    };
    
    inline void set_name(void* p, const char* n, const char* f = "", Bs32u l = 0){
#if BS_ALLOC_DEBUG
        static const char nstr[] = "name_string";
        Bs32u sz_n = strlen(n) + 1;
        char* _n = 0; 

        alloc((void*&)_n, sz_n, 0, p, 0);
        memcpy(_n, n, sz_n-1);
        mem[_n].name = nstr;

        mem[p].name = _n;
        mem[p].file = f;
        mem[p].line = l;
        printf("SET_NAME: %s (%d) : %p\n(%s : %d)\n", n, mem[p].size, p, f, l);
#else
        (void)p;
        (void)n;
        (void)f;
        (void)l;
#endif
    }

    inline bool touch(void* p) { return !!mem.count(p); }

    BSErr lock(void* p){
        if(!touch(p)) 
            return BS_ERR_INVALID_PARAMS;
        
#if BS_ALLOC_DEBUG
        printf("LOCK: %s (%d) : %p\n", mem[p].name, mem[p].size, p);
#endif

        mem[p].locked++;

        return BS_ERR_NONE;
    }

    BSErr unlock(void* p){
        if(!touch(p)) 
            return BS_ERR_INVALID_PARAMS;

#if BS_ALLOC_DEBUG
        printf("UNLOCK: %s (%d) : %p\n", mem[p].name, mem[p].size, p);
#endif

        if(mem[p].locked)
            mem[p].locked--;

        if(!mem[p].locked && mem[p].to_delete)
            free(p);

        return BS_ERR_NONE;
    }

    BSErr alloc(void*& p, Bs32u size, Bs32u count, void* base, void (*destructor)(void*, BS_Allocator*)){
        mem_descriptor d = {};

        if(!size) return BS_ERR_INVALID_PARAMS;

        p = NULL;

        if(count){ 
            p = calloc(count, size);
            d.size     = size*count;
        } else {
            p = malloc(size);
            if(p) memset(p, 0, size);
            d.size     = size;
        }

#if BS_ALLOC_DEBUG
        printf("ALLOC: %d : %p\n", d.size, p);
#endif

        if(!p) return BS_ERR_MEM_ALLOC;

        d.destructor = destructor;
        mem.insert(std::pair<void*, mem_descriptor>(p,d)); //mem[p] = d;

        if(base)
            return bound(p, base);

        return BS_ERR_NONE;
    }

    BSErr bound(void* p, void* base){
        if(!touch(base) || !touch(p)) 
            return BS_ERR_INVALID_PARAMS;

        if (mem[p].base.find(base) != mem[p].base.end())
            return BS_ERR_INVALID_PARAMS;

        mem[base].dependent.push_back(mem.find(p));
        mem[p].base.insert(base);
        
#if BS_ALLOC_DEBUG
        printf("BOUND: %s -> %s : %p -> %p\n", mem[p].name, mem[base].name, p, base);
#endif

        return BS_ERR_NONE;
    }

    BSErr realloc(void*& p, Bs32u size, Bs32u count){
        map<void*, mem_descriptor>::iterator it = mem.find(p);
        mem_descriptor d;
        Bs32u new_size = size*BS_MAX(count, 1);

        if(it == mem.end()) return BS_ERR_INVALID_PARAMS;

        d = it->second;

#if BS_ALLOC_DEBUG
        printf("REALLOC: %s (%d) : %p\n(%s : %d)\n", d.name, new_size, p, d.file, d.line);
#endif

        if(d.size == new_size) return BS_ERR_NONE;

        mem.erase(it);

        p = ::realloc(p, new_size);

        if(!p) return BS_ERR_MEM_ALLOC;

        memset((Bs8u*)p+d.size, 0, new_size-d.size);
        d.size = new_size;

        mem.insert(std::pair<void*, mem_descriptor>(p,d)); //mem[p] = d;

        return BS_ERR_NONE;
    }

    void free(void* p){
        const map<void*, mem_descriptor>::iterator it = mem.find(p);

        if(it == mem.end()) return;

        if(it->second.locked){
            it->second.to_delete = 1;
            return;
        }

        if(!it->second.base.empty()) return;

#if BS_ALLOC_DEBUG
        printf("FREE: %s (%d) : %p\n", it->second.name, it->second.size, it->first);
#endif

        while(!it->second.dependent.empty()){
            it->second.dependent.back()->second.base.erase(p);

            if(it->second.dependent.back()->second.base.empty())
                free(it->second.dependent.back()->first);

            it->second.dependent.pop_back();
        }

        if(it->second.destructor)
            it->second.destructor(p, this);

        ::free(p);
        mem.erase(it);

        return;
    }

private:
    map<void*, mem_descriptor> mem;
};



#endif //__BS_MEM_H