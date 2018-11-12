// Copyright (c) 2008-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
