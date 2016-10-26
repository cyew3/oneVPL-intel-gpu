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

#include "ippvc.h"
#include "umc_vc1_enc_def.h"
#include "umc_vc1_enc_common.h"
#include "umc_vc1_enc_debug.h"
#include "umc_vc1_enc_block.h"
#include "umc_vc1_enc_block_template.h"


namespace UMC_VC1_ENCODER
{
     //extern fWriteBlockInterSM  pWriteBlockInterSM[4] =
     //{
     //    WriteBlockInter8x8type,
     //    WriteBlockInter8x4type,
     //    WriteBlockInter4x8type,
     //    WriteBlockInter4x4type
     //};
     //extern fWriteBlockInterAdv pWriteBlockInterAdv[4]=
     //{
     //    WriteBlockInter8x8type,
     //    WriteBlockInter8x4type,
     //    WriteBlockInter4x8type,
     //    WriteBlockInter4x4type
     //};
     //extern fWriteDCSM          pWriteDCSM[3]=
     //{
     //    WriteDCQuant1,
     //    WriteDCQuant2,
     //    WriteDCQuantOther
     //
     //};
     //extern fWriteDCAdv         pWriteDCAdv[3]=
     //{
     //    WriteDCQuant1,
     //    WriteDCQuant2,
     //    WriteDCQuantOther
     //};

}

#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
