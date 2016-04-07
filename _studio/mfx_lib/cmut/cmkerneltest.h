/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#pragma once

#ifndef __LINUX__
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include "cmkernelruntimeenv.h"

// NOTE: to be implemented by test application
void Test(const string & isaFileName);

int main (int argc, char * argv[])
{
#ifndef __LINUX__
  _CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

#ifdef _DEBUG
  int * pLeak = new int[1];
  pLeak[0] = 0xA31EA31E;
#endif
#endif

  try
  {
    const std::string isaFileName(ISA_FILE_NAME2 (argv[0]));

    Test (isaFileName);

    cout << "Pass" << endl;

    return TEST_TARGET_PASS;
  } catch (std::exception & exp) {
    cout << "Fail" << endl;

    cout << exp.what () << endl;

    return TEST_TARGET_FAIL;
  }
}
