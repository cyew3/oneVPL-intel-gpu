/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_lexical_cast.h"

//#define RUN_MULTIINIT

SUITE(MFXInit)
{
#ifdef RUN_MULTIINIT
    struct Spawner
    {
        static mfxU32 VM_CALLCONVENTION spawn(void* pCtx)
        {
            int p = reinterpret_cast<int>(pCtx);
            for (size_t i = 0; true; i++)
            {
                mfxSession ses ;
                MFXInit(MFX_IMPL_HARDWARE, NULL, &ses);
                MFXClose(ses);
                std::cout<<"Spawner="<<p<<" n=" << i <<std::endl;
            }
            return 0;
        }
    };
    TEST(MultiInit)
    {
        std::vector<UMC::Thread> t(1);
        
        for (size_t i = 0; i < t.size(); i++)
        {
            t[i].Create(&Spawner::spawn, (void*)i);
        }
        vm_time_sleep((mfxU32)-1);
    }
#endif
}