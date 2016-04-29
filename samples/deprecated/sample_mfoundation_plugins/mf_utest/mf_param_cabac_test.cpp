/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/


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
