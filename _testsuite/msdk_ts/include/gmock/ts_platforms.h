/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

enum HWType
{
    MFX_HW_UNKNOWN   = 0,
    MFX_HW_LAKE      = 0x100000,
    MFX_HW_LRB       = 0x200000,
    MFX_HW_SNB       = 0x300000,

    MFX_HW_IVB       = 0x400000,

    MFX_HW_HSW       = 0x500000,
    MFX_HW_HSW_ULT   = 0x500001,

    MFX_HW_VLV       = 0x600000,

    MFX_HW_BDW       = 0x700000,

    MFX_HW_CHV       = 0x800000,

    MFX_HW_SKL       = 0x900000
};

enum OSFamily
{
    MFX_OS_FAMILY_UNKNOWN = 0,
    MFX_OS_FAMILY_WINDOWS,
    MFX_OS_FAMILY_LINUX
};

enum OSWinVersion
{
    MFX_WIN_VER_UNKNOWN = 0,
    MFX_WIN_VER_W7,
    MFX_WIN_VER_W81,
    MFX_WIN_VER_WS2012R2,
    MFX_WIN_VER_W10,
    MFX_WIN_VER_WS2016
};
