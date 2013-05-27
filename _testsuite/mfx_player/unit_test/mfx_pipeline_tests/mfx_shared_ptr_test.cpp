/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010 - 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_shared_ptr.h"

SUITE(mfx_shared_ptr)
{
    TEST(destructor_counter)
    {
        int f = 0;
        destructor_counter *p = new destructor_counter(f);
        delete p;
        delete p;

        CHECK_EQUAL(2, f);
    }

    TEST(SHARED_DELETED)
    {
        int f = 0;
        destructor_counter *deleteme = new destructor_counter(f);

        {
            mfx_shared_ptr<destructor_counter> ptr(deleteme);
        }
        CHECK_EQUAL(1, f);
    }
    
    TEST(SHARED_REOWNING)
    {
        int f = 0;
        destructor_counter *deleteme = new destructor_counter(f);

        {
            mfx_shared_ptr<destructor_counter> ptr_new;
            {
                mfx_shared_ptr<destructor_counter> ptr(deleteme);
                ptr_new = ptr;
            }
            CHECK_EQUAL(0, f);
        }
        CHECK_EQUAL(1, f);

        f = 0;
        {
            mfx_shared_ptr<destructor_counter> ptr_new(deleteme);
            mfx_shared_ptr<destructor_counter> ptr(ptr_new);
            mfx_shared_ptr<destructor_counter> ptr2(ptr_new);
        }
        CHECK_EQUAL(1, f);
    }
    
    TEST(SHARED_RELEASE_ASSIGN)
    {
        int f = 0;
        destructor_counter *deleteme = new destructor_counter(f);

        {
            mfx_shared_ptr<destructor_counter> ptr_new;
            {
                mfx_shared_ptr<destructor_counter> ptr(deleteme);
                ptr.release();
            }
            CHECK_EQUAL(0, f);

            {
                mfx_shared_ptr<destructor_counter> ptr(deleteme);
                ptr_new = ptr.release();
            }
            CHECK_EQUAL(0, f);
        }
        CHECK_EQUAL(1, f);
    }

    TEST(ASSIGN_EXISTED)
    {
        int f = 0;
        int f2 = 0;
        destructor_counter *deleteme = new destructor_counter(f);
        destructor_counter *deleteme2 = new destructor_counter(f2);

        {
            mfx_shared_ptr<destructor_counter> ptr(deleteme);
            mfx_shared_ptr<destructor_counter> ptr2(deleteme2);
            
            ptr2 = ptr;
            
            CHECK_EQUAL(1, f2);
            CHECK_EQUAL(0, f);
        }
        CHECK_EQUAL(1, f);
    }
    
    TEST(SHARED_RETURN_OBJ)
    {
        int f = 0;
        destructor_counter *deleteme = new destructor_counter(f);

        {
            mfx_shared_ptr<destructor_counter> ptr(deleteme);
            CHECK_EQUAL((void*)deleteme, (void*)&*ptr);
            CHECK_EQUAL((void*)deleteme, (void*)ptr.get());
            
            //double reset
            ptr.reset(NULL);
            ptr.reset(NULL);
            
            CHECK_EQUAL((void*)NULL, (void*)ptr.get());
        }
        
        {
            mfx_shared_ptr<destructor_counter> ptr(deleteme);
            CHECK_EQUAL((void*)deleteme, (void*)&*ptr);
            CHECK_EQUAL((void*)deleteme, (void*)ptr.get());

            //double release
            ptr.release();
            ptr.release();

            CHECK_EQUAL((void*)NULL, (void*)ptr.get());
        }
    }
    
    TEST(STL_CONTAINERS)
    {
        int f[4] = {0};
        {
            std::vector<mfx_shared_ptr<destructor_counter> > vec;

            vec.push_back(new destructor_counter(f[0]));
            vec.push_back(new destructor_counter(f[1]));
            vec.push_back(new destructor_counter(f[2]));
            vec.push_back(new destructor_counter(f[3]));

            CHECK_EQUAL(0, f[0]);
            CHECK_EQUAL(0, f[1]);
            CHECK_EQUAL(0, f[2]);
            CHECK_EQUAL(0, f[3]);

            vec[0] = vec[2];
            CHECK_EQUAL(1, f[0]);
            CHECK_EQUAL(0, f[1]);
            CHECK_EQUAL(0, f[2]);
            CHECK_EQUAL(0, f[3]);

            vec[1] = vec[0];
            CHECK_EQUAL(1, f[0]);
            CHECK_EQUAL(1, f[1]);
            CHECK_EQUAL(0, f[2]);
            CHECK_EQUAL(0, f[3]);
        }

        CHECK_EQUAL(1, f[0]);
        CHECK_EQUAL(1, f[1]);
        CHECK_EQUAL(1, f[2]);
        CHECK_EQUAL(1, f[3]);
    }
}