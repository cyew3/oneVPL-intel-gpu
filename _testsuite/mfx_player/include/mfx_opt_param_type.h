/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once 

//typevalue semantics
enum OptParamType
{
    OPT_UNDEFINED = 0,
    OPT_INT_32,
    OPT_INT_16,
    OPT_UINT_8,
    OPT_UINT_16,
    OPT_UINT_32,
    OPT_BOOL,
    OPT_64F,
    OPT_STR,
    OPT_COD,
    OPT_TRI_STATE,
    OPT_FILENAME,
    OPT_SPECIAL,
    OPT_AUTO_DESERIAL,
};

template<class T>
struct OptParamTypeTrait 
{
    static const OptParamType type = OPT_UNDEFINED;
};

template<>
struct OptParamTypeTrait <mfxU8>
{
    static const OptParamType type = OPT_UINT_8;
};

template<>
struct OptParamTypeTrait <mfxU16>
{
    static const OptParamType type = OPT_UINT_16;
};

template<>
struct OptParamTypeTrait <mfxU32>
{
    static const OptParamType type = OPT_UINT_32;
};

template<>
struct OptParamTypeTrait <mfxI16>
{
    static const OptParamType type = OPT_INT_16;
};

template<>
struct OptParamTypeTrait <mfxI32>
{
    static const OptParamType type = OPT_INT_32;
};

template<>
struct OptParamTypeTrait <bool>
{
    static const OptParamType type = OPT_BOOL;
};

template<>
struct OptParamTypeTrait <mfxF64>
{
    static const OptParamType type = OPT_64F;
};


