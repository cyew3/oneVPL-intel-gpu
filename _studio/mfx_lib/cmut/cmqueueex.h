/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#pragma once

#include "cmrt_cross_platform.h"
#include "cmdeviceex.h"
#include "cmtaskex.h"
#include "cmkernelex.h"
#include "cmassert.h"
#include "vector"
#include "string"

namespace mdfut {
  class CmQueueEx
  {
  public:
    CmQueueEx(CmDeviceEx & deviceEx)
      : deviceEx(deviceEx)
    {
      INT result = deviceEx->CreateQueue(pQueue);
      result;
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);
    }

    ~CmQueueEx()
    {
      for (unsigned int i = 0; i < events.size(); i++) {
        pQueue->DestroyEvent(events[i]);
      }
    }

    void Enqueue(CmTask * pTask, CmThreadSpace * pThreadSpace = NULL)
    {
      CmEvent * pEvent = NULL;

      INT result = pQueue->Enqueue(pTask, pEvent, pThreadSpace);
      result;
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);

      events.push_back(pEvent);
    }

    void Enqueue(CmTaskEx & taskEx, CmThreadSpace * pThreadSpace = NULL)
    {
      Enqueue((CmTask *)taskEx, pThreadSpace);
    }

    void Enqueue(CmKernelEx & kernelEx, CmThreadSpace * pThreadSpace = NULL)
    {
      CmTaskEx taskEx(deviceEx, kernelEx);
      Enqueue(taskEx, pThreadSpace);
    }

#if __INTEL_MDF > CM_3_0
    void Enqueue(std::vector<CmKernelEx *> kernelExs, DWORD syncs, CmThreadSpace * pThreadSpace = NULL)
    {
      CmTaskEx taskEx(deviceEx, kernelExs, syncs);
      Enqueue(taskEx, pThreadSpace);
    }
#endif

    void WaitForFinished(UINT64 * pExecutionTime = NULL)
    {
      INT result = LastEvent()->WaitForTaskFinished(CM_MAX_TIMEOUT_MS);
      result;
      MDFUT_FAIL_IF(result != CM_SUCCESS, result);

      UINT64 time, totalTime = 0;
      for (unsigned int i = 0; i < events.size(); i++) {

        // if requested, accumulate kernel times before destroying events
        if (pExecutionTime) {
          events[i]->GetExecutionTime(time);
          totalTime += (time & 0xffffffff);
        }
        pQueue->DestroyEvent(events[i]);
      }
      events.clear();

      if (pExecutionTime) *pExecutionTime = totalTime;
    }

    CmEvent * LastEvent()
    {
      return *events.rbegin();
    }

    CmQueue * operator ->() { return pQueue; }

  protected:
    CmQueue * pQueue;
    CmDeviceEx & deviceEx;
    std::vector<CmEvent *> events;
          CmQueueEx(const CmQueueEx& that);
  //prohibit assignment operator
  CmQueueEx& operator=(const CmQueueEx&);
  };
};
