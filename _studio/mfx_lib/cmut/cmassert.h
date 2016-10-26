//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "cminfrastructure.h"

#define INT2STRING2(i) #i
#define INT2STRING(i)  INT2STRING2(i)

#define MDFUT_EXCEPTION2(message, exception) exception();
#define MDFUT_EXCEPTION(message) exception();

#define MDFUT_FAIL_IF(condition, result)

#if 0
//FIXME need better name
#define MDFUT_FAIL_IF(condition, result) \
  do {  \
    if (condition) {  \
    throw MDFUT_EXCEPTION(mdfut::cm_result_to_str(result)); \
    } \
  } while (false)
#endif
#define MDFUT_ASSERT_EQUAL(expect, actual, message)  \
  if (expect != actual) {                           \
    throw MDFUT_EXCEPTION(message);                 \
  }
#define MDFUT_ASSERT(condition)                        \
  if (!(condition)) {                                 \
    throw MDFUT_EXCEPTION("Expect : " #condition);    \
  }
#define MDFUT_ASSERT_MESSAGE(condition, message)       \
  if (!(condition)) {                                 \
    throw MDFUT_EXCEPTION(message);                   \
  }


template<typename ty, unsigned int size>
class CmAssertEqual
{
public:
  typedef ty elementTy;
  enum { elementSize = size };
public:
  template<typename ty2>
  CmAssertEqual(const ty2 * pExpects) : dataView(pExpects)
  {
  }

  void Assert(const void * pData) const
  {
    const ty * pActuals = (const ty *)pData;

    for (unsigned int i = 0; i < size; ++i) {
      if (!IsEqual(dataView[i], pActuals[i])) {
        write<size>((const ty *)dataView, pActuals, i);

        throw MDFUT_EXCEPTION("Difference is detected");
      }
    }
  }

  template<typename ty2>
  bool IsEqual(ty2 x, ty2 y) const
  {
    return x == y;
  }

  bool IsEqual(float x, float y) const
  {
    return fabs(x - y) <= 1e-5;
  }

  bool IsEqual(double x, double y) const
  {
    return fabs(x - y) <= 1e-5;
  }

protected:
  data_view<ty, size> dataView;
};
