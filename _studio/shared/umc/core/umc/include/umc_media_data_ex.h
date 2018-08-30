//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_MEDIA_DATA_EX_H__
#define __UMC_MEDIA_DATA_EX_H__

#include "umc_media_data.h"

namespace UMC
{

class MediaDataEx : public MediaData
{
    DYNAMIC_CAST_DECL(MediaDataEx, MediaData)

public:
    class _MediaDataEx{
        DYNAMIC_CAST_DECL_BASE(_MediaDataEx)
        public:
        uint32_t count;
        uint32_t index;
        unsigned long long bstrm_pos;
        uint32_t *offsets;
        uint32_t *values;
        uint32_t limit;

        _MediaDataEx()
        {
            count = 0;
            index = 0;
            bstrm_pos = 0;
            limit   = 2000;
            offsets = (uint32_t*)malloc(sizeof(uint32_t)*limit);
            values  = (uint32_t*)malloc(sizeof(uint32_t)*limit);
        }

        virtual ~_MediaDataEx()
        {
            if(offsets)
            {
                free(offsets);
                offsets = 0;
            }
            if(values)
            {
                free(values);
                values = 0;
            }
            limit   = 0;
        }
    };

    // Default constructor
    MediaDataEx()
    {
        m_exData = NULL;
    };

    // Destructor
    virtual ~MediaDataEx(){};

    _MediaDataEx* GetExData()
    {
        return m_exData;
    };

    void SetExData(_MediaDataEx* pDataEx)
    {
        m_exData = pDataEx;
    };

protected:
    _MediaDataEx *m_exData;
};

}

#endif //__UMC_MEDIA_DATA_EX_H__

