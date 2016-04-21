/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#ifndef __MFX_CONFIG_H__
#define __MFX_CONFIG_H__

/* Versions of Android inside Intel */
#define MFX_OTC     0x100
#define MFX_MCG     0x200

/* Google versions of Android */
#define MFX_ANDROID_NAME 0xff
#define MFX_JB           0x01
#define MFX_KK           0x02
#define MFX_LD           0x04
#define MFX_MM           0x08
#define MFX_N            0x10

#define MFX_OTC_JB  MFX_OTC + MFX_JB
#define MFX_MCG_JB  MFX_MCG + MFX_JB

#define MFX_OTC_KK  MFX_OTC + MFX_KK
#define MFX_MCG_KK  MFX_MCG + MFX_KK
#define MFX_OTC_LD  MFX_OTC + MFX_LD
#define MFX_MCG_LD  MFX_MCG + MFX_LD
#endif // #ifndef __MFX_CONFIG_H__
