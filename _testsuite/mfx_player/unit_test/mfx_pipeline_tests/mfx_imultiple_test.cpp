/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012 Intel Corporation. All Rights Reserved.
//
//
*/

#include "utest_common.h"
#include "mfx_imultiple.h"

SUITE(IMultiple)
{
    struct ITwoFunctionsFace
    {
        virtual int F1() = 0;
        virtual int F2() = 0;
    };

    template <int f1, int f2>
    struct TwoFunctionsFace : ITwoFunctionsFace
    {
        virtual int F1()
        {
            return f1;
        }
        virtual int F2()
        {
            return f2;
        }
    };


    class TwoFunctionsMultiple : public IMultiple <ITwoFunctionsFace> 
    {
    public:
        virtual int F1()
        {
            ITwoFunctionsFace * pface = ItemFromIdx(CurrentItemIdx(&TwoFunctionsMultiple::F1));
            IncrementItemIdx(&TwoFunctionsMultiple::F1);
            return pface->F1();
        }

        virtual int F2()
        {
            ITwoFunctionsFace * pface = ItemFromIdx(CurrentItemIdx(&TwoFunctionsMultiple::F2));
            IncrementItemIdx(&TwoFunctionsMultiple::F2);
            return pface->F2();
        }
    };

    TEST(generic_feature)
    {
        TwoFunctionsMultiple multiple;
        
        multiple.Register(new TwoFunctionsFace<1,2>(), 2);
        multiple.Register(new TwoFunctionsFace<3,4>(), 1);

        //whole cycle for f1 function
        CHECK_EQUAL(1, multiple.F1());
        CHECK_EQUAL(1, multiple.F1());
        CHECK_EQUAL(3, multiple.F1());
        CHECK_EQUAL(1, multiple.F1());

        //whole cycle for f2 function
        CHECK_EQUAL(2, multiple.F2());
        CHECK_EQUAL(2, multiple.F2());
        CHECK_EQUAL(4, multiple.F2());
        CHECK_EQUAL(2, multiple.F2());
    }

    TEST(pull_out_executor_by_order_1_2_4)
    {
        TwoFunctionsMultiple multiple;

        //configure ratio depended invocation
        multiple.SetRatioOrder(true);
        
        multiple.Register(new TwoFunctionsFace<1,2>(), 2);
        multiple.Register(new TwoFunctionsFace<55,77>(), 4);
        multiple.Register(new TwoFunctionsFace<4,9>(), 1);
 
        //whole cycle for f1 function
        //first frame 
        CHECK_EQUAL(4, multiple.F1());
        CHECK_EQUAL(1, multiple.F1());
        CHECK_EQUAL(55, multiple.F1());
        //secon frame 
        CHECK_EQUAL(55, multiple.F1());
        //3rd frame 
        CHECK_EQUAL(1, multiple.F1());
        CHECK_EQUAL(55, multiple.F1());
        //4th frame 
        CHECK_EQUAL(55, multiple.F1());

        //checking next cycle as well

        CHECK_EQUAL(4, multiple.F1());
        CHECK_EQUAL(1, multiple.F1());
        CHECK_EQUAL(55, multiple.F1());
        CHECK_EQUAL(55, multiple.F1());
        CHECK_EQUAL(1, multiple.F1());
        CHECK_EQUAL(55, multiple.F1());
        CHECK_EQUAL(55, multiple.F1());

        //lest break cycle and execute second function

        CHECK_EQUAL(4, multiple.F1());
        CHECK_EQUAL(1, multiple.F1());
        CHECK_EQUAL(55, multiple.F1());
        CHECK_EQUAL(55, multiple.F1());
        //////////////////////////////////////////////////////////////////////////

        CHECK_EQUAL(9, multiple.F2());
        CHECK_EQUAL(2, multiple.F2());
        CHECK_EQUAL(77, multiple.F2());
        CHECK_EQUAL(77, multiple.F2());
        CHECK_EQUAL(2, multiple.F2());
        CHECK_EQUAL(77, multiple.F2());
        CHECK_EQUAL(77, multiple.F2());

    }
    TEST(pull_out_executor_by_order_1_3_6_24)
    {
        TwoFunctionsMultiple multiple;

        //configure ratio depended invocation
        multiple.SetRatioOrder(true);

        multiple.Register(new TwoFunctionsFace<44,9>(), 24);
        multiple.Register(new TwoFunctionsFace<55,77>(), 3);
        multiple.Register(new TwoFunctionsFace<101,2>(), 1);
        multiple.Register(new TwoFunctionsFace<12,2>(),  6);


        //whole cycle for f1 function
        //first frame 
        CHECK_EQUAL(101, multiple.F1());
        CHECK_EQUAL(55, multiple.F1());
        CHECK_EQUAL(12, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
        
        CHECK_EQUAL(44, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
        
        CHECK_EQUAL(12, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
        
        CHECK_EQUAL(44, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
        
        CHECK_EQUAL(55, multiple.F1());
        CHECK_EQUAL(12, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
        
        CHECK_EQUAL(44, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
        
        CHECK_EQUAL(12, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
        
        CHECK_EQUAL(44, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());

        CHECK_EQUAL(55, multiple.F1());
        CHECK_EQUAL(12, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());

        CHECK_EQUAL(44, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());

        CHECK_EQUAL(12, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());

        CHECK_EQUAL(44, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());

        //check start of next frame with lower frequency
        CHECK_EQUAL(101, multiple.F1());
        CHECK_EQUAL(55, multiple.F1());
        CHECK_EQUAL(12, multiple.F1());
        CHECK_EQUAL(44, multiple.F1());
    }
}
