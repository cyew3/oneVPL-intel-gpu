/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/


#include "mf_param_cabac.h"
#include "sample_utils.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
using ::testing::AtLeast;


HRESULT Get_CAVLC_CodingOption(MFParamCabac &cabac, mfxU16 &cavlc)
{
    HRESULT hr = S_OK;
    mfxVideoParam          videoParam;
    MFExtBufCollection     arrExtBuf;
    arrExtBuf.UpdateExtParam(videoParam);
    hr = cabac.UpdateVideoParam(videoParam, arrExtBuf);
    if (S_OK == hr)
    {
        mfxExtCodingOption *pExtCodingOption = arrExtBuf.Find<mfxExtCodingOption>();
        if (NULL == pExtCodingOption)
        {
            hr = E_POINTER; //or cavlc = MFX_CODINGOPTION_UNKNOWN; instead
        }
        else
        {
            cavlc = pExtCodingOption->CAVLC;
        }
    }
    arrExtBuf.Free();
    return hr;
}

TEST(Param_Cabac, DefaultSettings)
{
    MFParamCabac cabac(NULL);
    EXPECT_EQ(true, cabac.IsEnabled());

    EXPECT_EQ(S_OK, cabac.IsSupported());
    EXPECT_EQ(S_OK, cabac.IsModifiable());
    HRESULT hr = E_FAIL;
    cabac.Get(&hr);
    EXPECT_EQ(VFW_E_CODECAPI_NO_CURRENT_VALUE, hr);

    mfxU16 cavlc = MFX_CODINGOPTION_UNKNOWN;
    EXPECT_EQ(S_OK, Get_CAVLC_CodingOption(cabac, cavlc));
    EXPECT_EQ(MFX_CODINGOPTION_UNKNOWN, cavlc);
}

TEST(Param_Cabac, DefaultProfileSetCABAC)
{
    MFParamCabac cabac(NULL);

    EXPECT_EQ(S_OK, cabac.IsModifiable());
    EXPECT_EQ(S_OK, cabac.Set(VARIANT_TRUE));
    HRESULT hr = E_FAIL;
    EXPECT_EQ(VARIANT_TRUE, cabac.Get(&hr));
    EXPECT_EQ(S_OK, hr);

    mfxU16 cavlc = MFX_CODINGOPTION_UNKNOWN;
    EXPECT_EQ(S_OK, Get_CAVLC_CodingOption(cabac, cavlc));
    EXPECT_EQ(MFX_CODINGOPTION_OFF, cavlc);
}

TEST(Param_Cabac, BaseProfileSetCABAC)
{
    MFParamCabac cabac(NULL);
    cabac.SetCodecProfile(MFX_PROFILE_AVC_BASELINE);
    EXPECT_EQ(S_FALSE, cabac.IsModifiable());
    //Set itself does not perform any checks for profile, it is guaranteed by user class to check IsModifiable before calling Set
    //EXPECT_EQ(S_FALSE, cabac.Set(VARIANT_TRUE));
    //HRESULT hr = E_FAIL;
    //cabac.Get(&hr);
    //EXPECT_EQ(VFW_E_CODECAPI_NO_CURRENT_VALUE, hr);

    mfxU16 cavlc = MFX_CODINGOPTION_UNKNOWN;
    EXPECT_EQ(S_OK, Get_CAVLC_CodingOption(cabac, cavlc));
    EXPECT_EQ(MFX_CODINGOPTION_UNKNOWN, cavlc);
}

TEST(Param_Cabac, BaseProfileSetCAVLC)
{
    MFParamCabac cabac(NULL);

    cabac.SetCodecProfile(MFX_PROFILE_AVC_BASELINE);
    EXPECT_EQ(S_FALSE, cabac.IsModifiable());
    //Set itself does not perform any checks for profile, it is guaranteed by user class to check IsModifiable before calling Set
    mfxU16 cavlc = MFX_CODINGOPTION_UNKNOWN;
    EXPECT_EQ(S_OK, Get_CAVLC_CodingOption(cabac, cavlc));
    EXPECT_EQ(MFX_CODINGOPTION_UNKNOWN, cavlc);
}

TEST(Param_Cabac, MainProfileSetCABAC)
{
    MFParamCabac cabac(NULL);

    cabac.SetCodecProfile(MFX_PROFILE_AVC_MAIN);
    EXPECT_EQ(S_OK, cabac.IsModifiable());
    EXPECT_EQ(S_OK, cabac.Set(VARIANT_TRUE));
    HRESULT hr = E_FAIL;
    EXPECT_EQ(VARIANT_TRUE, cabac.Get(&hr));
    EXPECT_EQ(S_OK, hr);

    mfxU16 cavlc = MFX_CODINGOPTION_UNKNOWN;
    EXPECT_EQ(S_OK, Get_CAVLC_CodingOption(cabac, cavlc));
    EXPECT_EQ(MFX_CODINGOPTION_OFF, cavlc);
}

TEST(Param_Cabac, MainProfileSetCAVLC)
{
    MFParamCabac cabac(NULL);

    cabac.SetCodecProfile(MFX_PROFILE_AVC_MAIN);
    EXPECT_EQ(S_OK, cabac.IsModifiable());
    EXPECT_EQ(S_OK, cabac.Set(VARIANT_FALSE));
    HRESULT hr = E_FAIL;
    EXPECT_EQ(VARIANT_FALSE, cabac.Get(&hr));
    EXPECT_EQ(S_OK, hr);

    mfxU16 cavlc = MFX_CODINGOPTION_UNKNOWN;
    EXPECT_EQ(S_OK, Get_CAVLC_CodingOption(cabac, cavlc));
    EXPECT_EQ(MFX_CODINGOPTION_ON, cavlc);
}
