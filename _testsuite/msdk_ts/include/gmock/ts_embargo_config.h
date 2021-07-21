/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2021 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

struct tsEmbargoFeatures
{
    class Flag
    {
    public:
        Flag(bool v) : m_val(v) {}
        operator bool() const;
    private:
        bool m_val;
    };

    static const bool embargo = true;  //enabled for ENV("TS_OPENSOURCE", "0") == "0"
    static const bool open    = false; //always enabled

    //RTCommon features:
    struct Common
    {
        Flag QImplDescMediaAdapterType = embargo; //API 2.5
    } common;
};

const tsEmbargoFeatures g_tsEmbargo;
