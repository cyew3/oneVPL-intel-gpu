//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_SPLITTERS_H__
#define __UMC_SPLITTERS_H__

#ifdef UMC_ENABLE_AVI_SPLITTER
    #include "umc_avi_splitter.h"
#endif

#ifdef UMC_ENABLE_MPEG2_SPLITTER
    #include "umc_threaded_demuxer.h"
#endif

#ifdef UMC_ENABLE_MP4_SPLITTER
    #include "umc_mp4_spl.h"
#endif

#ifdef UMC_ENABLE_VC1_SPLITTER
    #include "umc_vc1_spl.h"
#endif

#ifdef UMC_ENABLE_AVS_SPLITTER
    #include "umc_avs_splitter.h"
#endif

#endif // __UMC_SPLITTERS_H__

