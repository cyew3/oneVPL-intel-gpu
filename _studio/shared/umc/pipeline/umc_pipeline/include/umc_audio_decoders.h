//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_AUDIO_DECODERS_H__
#define __UMC_AUDIO_DECODERS_H__

#ifdef UMC_ENABLE_MP3_INT_AUDIO_DECODER
    #include "umc_mp3_dec.h"
#endif

#ifdef UMC_ENABLE_AAC_INT_AUDIO_DECODER
    #include "umc_aac_decoder_int.h"
#endif

#ifdef UMC_ENABLE_MP3_AUDIO_DECODER
    #include "umc_mp3dec_fp.h"
#endif

#ifdef UMC_ENABLE_AAC_AUDIO_DECODER
    #include "umc_aac_decoder.h"
#endif

#ifdef UMC_ENABLE_AC3_AUDIO_DECODER
    #include "umc_ac3_decoder.h"
#endif

#ifdef UMC_ENABLE_PCM_AUDIO_DECODER
    #include "umc_pcm_decoder.h"
#endif

#endif // __UMC_AUDIO_DECODERS_H__
