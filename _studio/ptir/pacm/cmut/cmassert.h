/**             
***
*** Copyright  (C) 1985-2016 Intel Corporation. All rights reserved.
***
*** The information and source code contained herein is the exclusive
*** property of Intel Corporation. and may not be disclosed, examined
*** or reproduced in whole or in part without explicit written authorization
*** from the company.
***
*** ----------------------------------------------------------------------------
**/ 
#pragma once

#include "infrastructure.h"
#include "cmutility.h"
#include "math.h"

template<bool> struct assert_static;
template<> struct assert_static<true> {};

#define INT2STRING2(i) #i
#define INT2STRING(i)  INT2STRING2(i)

//#ifndef __LINUX__
//  #define CMUT_EXCEPTION2(message, exception) exception(__FILE__ ": " __FUNCTION__ "(" INT2STRING(__LINE__) "): error: " message);
//  #define CMUT_EXCEPTION(message) std::exception((std::string(__FILE__ ": " __FUNCTION__ "(" INT2STRING(__LINE__) "): error: ") + std::string(message)).c_str());
//#else
  #define CMUT_EXCEPTION2(message, exception) exception();
  #define CMUT_EXCEPTION(message) std::exception();
//#endif

//FIXME need better name
#define CM_FAIL_IF(condition, result) \
  do {  \
    if (condition) {  \
    throw CMUT_EXCEPTION(cmut::cm_result_to_str(result)); \
    } \
  } while (false)

#define CMUT_ASSERT_EQUAL(expect, actual, message)  \
  if (expect != (actual)) {                           \
    throw CMUT_EXCEPTION(message);                 \
  }
#define CMUT_ASSERT(condition)                        \
  if (!(condition)) {                                 \
    throw CMUT_EXCEPTION("Expect : " #condition);    \
  }
#define CMUT_ASSERT_MESSAGE(condition, message)       \
  if (!(condition)) {                                 \
    throw CMUT_EXCEPTION(message);                   \
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

        throw CMUT_EXCEPTION("Difference is detected");
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
