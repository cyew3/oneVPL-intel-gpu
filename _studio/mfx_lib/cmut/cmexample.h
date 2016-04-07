/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#pragma once

#include "iostream"
#include "cmassert.h"

class CmExampleTest
{
public:
  virtual ~CmExampleTest () = 0;

public:
  bool Test ()
  {
    bool result = true;

    try
    {
      Setup ();

      RunAndTimeFor (*this, &CmExampleTest::RunExpect, "Expect ");

      RunAndTimeFor (*this, &CmExampleTest::RunActual, "Actual ");

      Validate ();
    } catch (std::exception & exp) {
      std::cout << "Error => " << exp.what () << std::endl;
      result = false;
    }

    try
    {
      Teardown ();
    } catch (...) {
      result = false;
    }

    return result;
  }

protected:
  virtual void Setup () = 0;
  virtual void Teardown () = 0;
  virtual void RunActual () = 0;
  virtual void RunExpect () = 0;
  virtual void Validate () = 0;

protected:
  template<typename objectTy, typename funcTy>
  static void RunAndTimeFor (objectTy & object, funcTy func, const char * description)
  {
    //Clock clock;
    //clock.Begin ();

    (object.*func) ();

    //clock.End ();

    //std::cout << description << " @ " << clock.Duration () << " ms" << std::endl;
  }
};

inline CmExampleTest::~CmExampleTest() {}

inline void AssertEqual (float x, float y, float threshold = 1e-3)
{
  float diff = x - y;
  if (diff > threshold || diff < -threshold) {
    throw MDFUT_EXCEPTION("AssertEqual Fail");
  }
}
