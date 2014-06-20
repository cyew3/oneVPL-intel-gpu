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