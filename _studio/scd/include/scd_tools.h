// Copyright (c) 2017-2018 Intel Corporation
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
#pragma once

#include <memory>
#include <list>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <functional>
#include <map>
#include "mfxplugin++.h"

namespace SCDTools
{
template <class T, class U0>
bool CheckOption(T & opt, U0 deflt)
{
    if (opt == T(deflt))
        return false;

    opt = T(deflt);
    return true;
}

template <class T, class U0, class... U1>
bool CheckOption(T & opt, U0 deflt, U1... supprtd)
{
    if (opt == T(deflt))
        return false;

    if (CheckOption(opt, supprtd...))
    {
        opt = T(deflt);
        return true;
    }

    return false;
}

template <class T, class U0>
bool CheckMax(T & opt, U0 max)
{
    if (opt > max)
    {
        opt = T(max);
        return true;
    }

    return false;
}

template<class T>
class AutoClose
{
public:
    T* m_plugin;

    AutoClose(T* p)
        : m_plugin(p)
    {
    }

    ~AutoClose()
    {
        if (m_plugin)
            m_plugin->Close();
    }

    void Cancel()
    {
        m_plugin = 0;
    }
};

template<class T>
class AsyncPool
{
protected:
    std::list<T> m_free;
    std::list<T> m_busy;
    std::mutex m_mtx;
    std::condition_variable m_cv;

public:
    typedef std::unique_lock<decltype(m_mtx)> ULGuard;

    T* Pop()
    {
        ULGuard guard(m_mtx);

        if (!m_free.empty())
        {
            m_busy.splice(m_busy.end(), m_free, m_free.begin());
            return &m_busy.back();
        }

        return 0;
    }

    bool Push(T* p)
    {
        ULGuard guard(m_mtx);
        auto it = std::find_if(m_busy.begin(), m_busy.end(), [&](T& x) { return p == &x; });

        if (it == m_busy.end())
            return false;

        m_free.splice(m_free.end(), m_busy, it);

        if (m_busy.empty())
            m_cv.notify_one();

        return true;
    }

    bool Push(T& p)
    {
        ULGuard guard(m_mtx);
        auto it = std::find_if(m_busy.begin(), m_busy.end(), [&](T& x) { return p == x; });

        if (it == m_busy.end())
            return false;

        m_free.splice(m_free.end(), m_busy, it);

        if (m_busy.empty())
            m_cv.notify_one();

        return true;
    }

    bool Sync(ULGuard& guard, bool bWait = false)
    {
        m_cv.wait(guard, [&]() { return !bWait || m_busy.empty(); });
        return m_busy.empty();
    }
};

template<class T>
class TaskManager : protected AsyncPool<T>
{
public:
    typedef AsyncPool<T> Base;

    ~TaskManager()
    {
        Close();
    }

    bool Reset(mfxU32 numTask)
    {
        typename Base::ULGuard guard(Base::m_mtx);
        Base::Sync(guard, true);

        Base::m_free.splice(Base::m_free.end(), m_reserved);

        if (Base::m_free.empty())
        {
            Base::m_free.resize(numTask);
            return true;
        }

        if (Base::m_free.size() < numTask)
            return false;

        if (Base::m_free.size() == numTask)
            return true;

        {
            auto eit = Base::m_free.begin();
            std::advance(eit, Base::m_free.size() - numTask);
            m_reserved.splice(m_reserved.end(), Base::m_free, Base::m_free.begin(), eit);
        }

        return true;
    }

    T* New()
    {
        T* pTask = Base::Pop();

        if (pTask)
            pTask->Reset();

        return pTask;
    }

    void Ready(T* pTask)
    {
        Base::Push(pTask);
    }

    void Close()
    {
        Reset(0);
        m_reserved.resize(0);
    }

protected:
    std::list<T> m_reserved;
};

class InternalSurfaces : protected AsyncPool<mfxMemId>
{
public:
    InternalSurfaces() : m_pAlloc(0) {}
    ~InternalSurfaces() { Free(); }

    mfxStatus Alloc(mfxFrameAllocator& allocator, mfxFrameAllocRequest& req);
    void Free();
    bool Pop(mfxFrameSurface1& surf);
    bool Push(mfxFrameSurface1& surf);

protected:
    mfxFrameAllocResponse m_response;
    mfxFrameInfo m_info;
    mfxFrameAllocator* m_pAlloc;
};

class AutoDestructor
{
private:
    std::multimap<mfxU32, std::function<void()> > m_cbMap;
public:
    inline void Swap(AutoDestructor& other)
    {
        m_cbMap.swap(other.m_cbMap);
    }

    ~AutoDestructor()
    {
        Call();
    }

    template<class ...Ts>
    inline void Add(mfxU32 order, Ts... bindArgs)
    {
        m_cbMap.emplace(order, std::bind(bindArgs...));
    }

    inline void Clear()
    {
        m_cbMap.clear();
    }

    inline void Call()
    {
        for (auto& entry : m_cbMap)
            entry.second();
        Clear();
    }
};

}
