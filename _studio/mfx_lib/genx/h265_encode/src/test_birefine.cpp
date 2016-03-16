/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2015 Intel Corporation. All Rights Reserved.
//
*/

#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#pragma warning(disable : 4505)
#include "cm_rt.h"
#include "../include/test_common.h"

int TestBiRefine32x32();
int TestBiRefine64x64();

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
    RunTest(TestBiRefine32x32, "TestBiRefine32x32");
//    RunTest(TestBiRefine64x64, "TestBiRefine64x64");
}