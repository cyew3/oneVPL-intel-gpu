/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_pipeline_utils.h"
#include "mfx_cmd_option_processor.h"
#include "mfx_irandom.h"

SUITE(mfx_random)
{
    template <class T>
    struct random_tester
    {
        random_tester()
        {
            T rnd;
            rnd.srand(1000);
            mfxU32 val0 = rnd.rand();
            //go a bit further on this list
            mfxU32 val1 = rnd.rand();
            mfxU32 val2 = rnd.rand();
            
            rnd.srand(1000);
            CHECK_EQUAL(val0, rnd.rand());
            CHECK_EQUAL(val1, rnd.rand());
            CHECK_EQUAL(val2, rnd.rand());

            T rnd2;
            rnd2.srand(1000);
            CHECK_EQUAL(val0, rnd2.rand());
            CHECK_EQUAL(val1, rnd2.rand());
            CHECK_EQUAL(val2, rnd2.rand());
        }
    };
    TEST(IPPBasedRandomizer)
    {
        random_tester<IPPBasedRandom> rnd;
    }

    TEST(STDLibRandomizer)
    {
        random_tester<DefaultRandom> rnd;
    }

}