//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#include "umc_vc1_enc_smoothing_adv.h"
#include "umc_vc1_enc_smoothing_sm.h"

namespace UMC_VC1_ENCODER
{
    extern fSmooth_I_MB_Adv Smooth_I_MBFunction_Adv_YV12[8] = 
    { 
        NoSmoothMB_I_Adv,            //right/top/left   000
        Smoth_TopRightMB_I_Adv_YV12, //right/top        001
        NoSmoothMB_I_Adv,            //right, left      010
        Smoth_RightMB_I_Adv_YV12,    //right            011
        Smooth_LeftMB_I_Adv_YV12,    //top/left         100
        Smoth_TopMB_I_Adv_YV12,      //top              101
        Smooth_LeftMB_I_Adv_YV12,    //left             110
        Smoth_MB_I_Adv_YV12          //common           111
    };

    extern fSmooth_I_MB_Adv Smooth_I_MBFunction_Adv_NV12[8] = 
    { 
        NoSmoothMB_I_Adv,            //right/top/left   000
        Smoth_TopRightMB_I_Adv_NV12, //right/top        001
        NoSmoothMB_I_Adv,            //right, left      010
        Smoth_RightMB_I_Adv_NV12,    //right            011
        Smooth_LeftMB_I_Adv_NV12,    //top/left         100
        Smoth_TopMB_I_Adv_NV12,      //top              101
        Smooth_LeftMB_I_Adv_NV12,    //left             110
        Smoth_MB_I_Adv_NV12          //common           111
    };

    extern fSmooth_P_MB_Adv Smooth_P_MBFunction_Adv_YV12[8] = 
    { 
        NoSmoothMB_P_Adv,            //right/top/left   000
        Smoth_TopRightMB_P_Adv_YV12, //right/top        001
        NoSmoothMB_P_Adv,            //right, left      010
        Smoth_RightMB_P_Adv_YV12,    //right            011
        Smooth_LeftMB_P_Adv_YV12,    //top/left         100
        Smoth_TopMB_P_Adv_YV12,      //top              101
        Smooth_LeftMB_P_Adv_YV12,    //left             110
        Smoth_MB_P_Adv_YV12          //common           111
    };

    extern fSmooth_P_MB_Adv Smooth_P_MBFunction_Adv_NV12[8] = 
    { 
        NoSmoothMB_P_Adv,            //right/top/left   000
        Smoth_TopRightMB_P_Adv_NV12, //right/top        001
        NoSmoothMB_P_Adv,            //right, left      010
        Smoth_RightMB_P_Adv_NV12,    //right            011
        Smooth_LeftMB_P_Adv_NV12,    //top/left         100
        Smoth_TopMB_P_Adv_NV12,      //top              101
        Smooth_LeftMB_P_Adv_NV12,    //left             110
        Smoth_MB_P_Adv_NV12          //common           111
    };

    extern fSmooth_I_MB_SM Smooth_I_MBFunction_SM_YV12[8] = 
    { 
        NoSmoothMB_I_SM,            //right/top/left   000
        Smoth_TopRightMB_I_SM_YV12, //right/top        001
        NoSmoothMB_I_SM,            //right, left      010
        Smoth_RightMB_I_SM_YV12,    //right            011
        Smooth_LeftMB_I_SM_YV12,    //top/left         100
        Smoth_TopMB_I_SM_YV12,      //top              101
        Smooth_LeftMB_I_SM_YV12,    //left             110
        Smoth_MB_I_SM_YV12          //common           111
    };

    extern fSmooth_I_MB_SM Smooth_I_MBFunction_SM_NV12[8] = 
    { 
        NoSmoothMB_I_SM,            //right/top/left   000
        Smoth_TopRightMB_I_SM_NV12, //right/top        001
        NoSmoothMB_I_SM,            //right, left      010
        Smoth_RightMB_I_SM_NV12,    //right            011
        Smooth_LeftMB_I_SM_NV12,    //top/left         100
        Smoth_TopMB_I_SM_NV12,      //top              101
        Smooth_LeftMB_I_SM_NV12,    //left             110
        Smoth_MB_I_SM_NV12          //common           111
    };

    extern fSmooth_P_MB_SM Smooth_P_MBFunction_SM_YV12[8] = 
    { 
        NoSmoothMB_P_SM,            //right/top/left   000
        Smoth_TopRightMB_P_SM_YV12, //right/top        001
        NoSmoothMB_P_SM,            //right, left      010
        Smoth_RightMB_P_SM_YV12,    //right            011
        Smooth_LeftMB_P_SM_YV12,    //top/left         100
        Smoth_TopMB_P_SM_YV12,      //top              101
        Smooth_LeftMB_P_SM_YV12,    //left             110
        Smoth_MB_P_SM_YV12          //common           111
    };

    extern fSmooth_P_MB_SM Smooth_P_MBFunction_SM_NV12[8] = 
    { 
        NoSmoothMB_P_SM,            //right/top/left   000
        Smoth_TopRightMB_P_SM_NV12, //right/top        001
        NoSmoothMB_P_SM,            //right, left      010
        Smoth_RightMB_P_SM_NV12,    //right            011
        Smooth_LeftMB_P_SM_NV12,    //top/left         100
        Smoth_TopMB_P_SM_NV12,      //top              101
        Smooth_LeftMB_P_SM_NV12,    //left             110
        Smoth_MB_P_SM_NV12          //common           111
    };
}

#endif
