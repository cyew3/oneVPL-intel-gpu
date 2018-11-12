// Copyright (c) 2003-2018 Intel Corporation
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

#ifndef __UMC_THREAD_H__
#define __UMC_THREAD_H__

#include "vm_debug.h"
#include "vm_thread.h"
#include "umc_structures.h"

namespace UMC
{
class Thread
{
public:
    // Default constructor
    Thread(void);
    virtual ~Thread(void);

    // Check thread status
    bool IsValid(void);
    // Create new thread
    Status Create(vm_thread_callback func, void *arg);
    // Wait until thread does exit
    void Wait(void);
    // Set thread priority
    Status SetPriority(vm_thread_priority priority);
    // Close thread object
    void Close(void);

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
    // Set reaction on exception, if exception is caught(VM_THREADCATCHCRASH define)
    Status SetExceptionReaction(vm_thread_callback func);
#endif

protected:
    vm_thread m_Thread;                                         // (vm_thread) handle to system thread
};

inline
bool Thread::IsValid(void)
{
    return vm_thread_is_valid(&m_Thread) ? true : false;

} // bool Thread::IsValid(void)

inline
void Thread::Wait(void)
{
    vm_thread_wait(&m_Thread);

} // void Thread::Wait(void)

inline
void Thread::Close(void)
{
    vm_thread_close(&m_Thread);

} // void Thread::Close(void)

} // namespace UMC

#endif // __UMC_THREAD_H__
