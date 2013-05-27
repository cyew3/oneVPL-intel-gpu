
/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: mfxstructures.h

*******************************************************************************/

#pragma  once

#include "mfx_itime.h"

//warning C4345: behavior change: an object of POD type constructed with an initializer of the form () will be default-initialized
#pragma warning (disable:4345)

class SurfacesPool
{
public:
    SurfacesPool(ITime *pTime)
        :  m_pTime (pTime)
    {
    }
    ~SurfacesPool()
    {
        for_each(m_Surfaces.begin(), m_Surfaces.end(), SrfEncCtlDelete());
    }

    mfxStatus FindFree(SrfEncCtl & surface_and_ctrl, mfxU64 nTimeout)
    {
        mfxU32 SleepInterval = 10; // milliseconds    

        for (mfxU32 j = 0; j < nTimeout; j += SleepInterval)
        {
            for (mfxU32 i = 0; i < m_Surfaces.size(); i++)
            {
                if (0 == m_Surfaces[i].pSurface->Data.Locked)
                {       
                    surface_and_ctrl = m_Surfaces[i];
                    return MFX_ERR_NONE;
                }
            }
            //wait if there's no free surface
            m_pTime->Wait(SleepInterval);
        }          

        return MFX_WRN_IN_EXECUTION;
    }

    mfxStatus Create(mfxFrameAllocResponse &nresponse, mfxFrameInfo &info)
    {
        for (int i = 0; i < nresponse.NumFrameActual; i++)
        { 
            mfxFrameSurface1 *pSrf;
            MFX_CHECK_POINTER(pSrf = new mfxFrameSurface1);
            
            memcpy(&pSrf->Info, &info, sizeof(info));
            memset(&pSrf->Data, 0, sizeof(mfxFrameData));
            pSrf->Data.MemId = nresponse.mids[i];
            m_Surfaces.push_back(SrfEncCtl(pSrf));
        }
        return MFX_ERR_NONE;
    }

protected:
    std::vector<SrfEncCtl> m_Surfaces;
    ITime * m_pTime;
};
