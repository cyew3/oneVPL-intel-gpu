//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2020 Intel Corporation. All Rights Reserved.
//

#pragma once

#include <vector>
#include "test_usage_models_utils.h"

// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&);               \
    void operator=(const TypeName&)   
#endif


struct mfxBitstreamEx
{
    mfxBitstreamEx(): syncp(0), bFree(true){};

    //mfxBitstream   *pBS;
    mfxBitstreamWrapper     bitstream;
    mfxSyncPoint            syncp;
    bool                    bFree;
};

class BitstreamExManager
    {
    public:
        explicit BitstreamExManager(mfxU32 size)
        {
            m_bsPool.resize(size);
        };
        virtual ~BitstreamExManager()
        {
            for (mfxU32 i=0; i < m_bsPool.size(); i++)
            {
                MSDK_SAFE_DELETE(m_bsPool[i].bitstream.Data);
            }
            m_bsPool.clear();

        };

        mfxBitstreamEx* GetNext( void )
        {
            for (mfxU32 i=0; i < m_bsPool.size(); i++)
            {
                if (m_bsPool[i].bFree)
                {
                    m_bsPool[i].bFree = false;
                    return &m_bsPool[i];
                }
            }
            return NULL;
        };

        void Release(mfxBitstreamEx* pBSEx)
        {
            for (mfxU32 i=0; i < m_bsPool.size(); i++)
            {
                if (&m_bsPool[i] == pBSEx)
                {
                    m_bsPool[i].bFree = true;
                    return;
                }
            }
            return;
        };
    protected:
        std::vector<mfxBitstreamEx> m_bsPool;

    private:
        DISALLOW_COPY_AND_ASSIGN(BitstreamExManager);
    };
/* EOF */