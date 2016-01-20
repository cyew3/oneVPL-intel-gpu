//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2011 Intel Corporation. All Rights Reserved.
//
//
//*/


#include "vc1_spl_header_parser.h"
#include <assert.h>

namespace ProtectedLibrary
{

    mfxStatus VC1_SPL_Headers::MVRangeDecode()
    {
        if (SH.EXTENDED_MV == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.MVRANGE);

            if(PicH.MVRANGE)
            {
                VC1_SPL_GET_BITS(1, PicH.MVRANGE);
                if(PicH.MVRANGE)
                {
                    VC1_SPL_GET_BITS(1, PicH.MVRANGE);
                    PicH.MVRANGE += 1;
                }
                PicH.MVRANGE += 1;
            }
        }
        else
        {
            PicH.MVRANGE = 0;
        }
        return MFX_ERR_NONE;
    }

    mfxStatus VC1_SPL_Headers::DMVRangeDecode()
    {
        if(SH.EXTENDED_DMV == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.DMVRANGE);
            if(PicH.DMVRANGE==0)
            {
                PicH.DMVRANGE = VC1_DMVRANGE_NONE;
            }
            else
            {
                VC1_SPL_GET_BITS(1, PicH.DMVRANGE);
                if(PicH.DMVRANGE==0)
                {
                    PicH.DMVRANGE = VC1_DMVRANGE_HORIZONTAL_RANGE;
                }
                else
                {
                    VC1_SPL_GET_BITS(1, PicH.DMVRANGE);
                    if(PicH.DMVRANGE==0)
                    {
                        PicH.DMVRANGE = VC1_DMVRANGE_VERTICAL_RANGE;
                    }
                    else
                    {
                        PicH.DMVRANGE = VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE;
                    }
                }
            }
        }

        return MFX_ERR_NONE;
    }


}