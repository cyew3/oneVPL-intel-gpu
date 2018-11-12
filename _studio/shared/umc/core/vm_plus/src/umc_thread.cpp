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

#include "umc_thread.h"

namespace UMC
{

Thread::Thread(void) :m_Thread()
{
    vm_thread_set_invalid(&m_Thread);

} // Thread::Thread(void)

Thread::~Thread()
{   Close();    }

Status Thread::Create(vm_thread_callback func, void *arg)
{
    Status umcRes = UMC_OK;
    
    if (0 == vm_thread_create(&m_Thread, func, arg))
        umcRes = UMC_ERR_FAILED;

    return umcRes;

} // Status Thread::Create(vm_thread_callback func, void *arg)

Status Thread::SetPriority(vm_thread_priority priority)
{
    if (vm_thread_set_priority(&m_Thread, priority))
        return UMC_OK;
    else
        return UMC_ERR_FAILED;

} // Status Thread::SetPriority(vm_thread_priority priority)

#if defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
Status Thread::SetExceptionReaction(vm_thread_callback /*func*/)
{
    return UMC_OK;
}
#endif

} // namespace UMC
