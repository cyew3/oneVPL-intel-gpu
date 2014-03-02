/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#include "stdio.h"
#include "exception"
#include "../include/test_common.h"

int TestAnalyzeGradient();
int TestRefineMeP32x32();
int TestRefineMeP32x16();
int TestRefineMeP16x32();

typedef int (*TestFuncPtr)();

void RunTest(TestFuncPtr testFunc, char * kernelName)
{
    printf("%s... ", kernelName);
    try {
        int res = testFunc();
        if (res == PASSED)
            printf("passed\n");
        else if (res == FAILED)
            printf("FAILED!\n");
    }
    catch (std::exception & e) {
        printf("EXCEPTION: %s\n", e.what());
    }
    catch (int***) {
        printf("UNKNOWN EXCEPTION\n");
    }
}

int main()
{
    RunTest(TestRefineMeP32x32, "RefineMeP32x32");
    RunTest(TestRefineMeP32x16, "RefineMeP32x16");
    RunTest(TestRefineMeP16x32, "RefineMeP16x32");
    RunTest(TestAnalyzeGradient, "AnalyzeGradient");
}
