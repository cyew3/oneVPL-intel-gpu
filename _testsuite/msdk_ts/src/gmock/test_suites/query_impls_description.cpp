/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2021 Intel Corporation. All Rights Reserved.

File Name: query_impls_description.cpp

\* ****************************************************************************** */
#include "ts_session.h"
#include "ts_struct.h"

#if defined(MFX_ONEVPL)
#include "gmock/test_suites/reference_query_impls_common.h"
#include "gmock/test_suites/reference_query_impls_decode.h"
#include "gmock/test_suites/reference_query_impls_encode.h"
#include "gmock/test_suites/reference_query_impls_vpp.h"

namespace query_impls_description
{
class TestSuite : tsSession
{
public:
    TestSuite() : tsSession() {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 5;

    enum TestCaseType
    {
        CASE_COMMON,
        CASE_DECODE,
        CASE_ENCODE,
        CASE_VPP,
    };

    struct tc_struct
    {
        TestCaseType sts;
    };

    RefDesCodec& GetCodecReference(bool isEncode)
    {
        switch (g_tsHWtype)
        {
        case MFX_HW_XE_HP_SDV:
            return isEncode ? ref_ATS_enc.GetReference() : ref_ATS_dec.GetReference();
        case MFX_HW_DG2:
            return isEncode ? ref_DG2_enc.GetReference() : ref_DG2_dec.GetReference();
        default:
            break;
        }
        EXPECT_TRUE(false) << "ERROR: Not platform detected, pls set env var TS_PLATFORM\n";
        return isEncode ? ref_default_enc.GetReference() : ref_default_dec.GetReference();
    }

    mfxStatus CheckCodec(mfxU16 MaxcodecLevel, mfxU16 profileNum, const CodecDesc& ref)
    {
        EXPECT_EQ(ref.MaxcodecLevel, MaxcodecLevel) << "ERROR: Incorrect MaxcodecLevel \n";
        EXPECT_EQ(ref.Profiles.size(), profileNum) << "ERROR: Incorrect Profile number \n";
        return MFX_ERR_NONE;
    }
    mfxStatus CheckCodecEncode(mfxU16 BiDirectionalPrediction, const CodecDesc& ref)
    {
        EXPECT_EQ(ref.BiDirectionalPrediction, BiDirectionalPrediction) << "ERROR: Incorrect BiDirectionalPrediction \n";
        return MFX_ERR_NONE;
    }

    mfxStatus CheckProfile(mfxU16 memDescNum, const ProfileDesc& ref)
    {
        EXPECT_EQ(ref.memDescs.size(), memDescNum) << "ERROR: Incorrect memDesc number \n";
        return MFX_ERR_NONE;
    }

    mfxStatus CheckMemDesc(mfxRange32U& width, mfxRange32U& height, mfxU16 numColorFormat, mfxU32* formats, const MemDesc& ref)
    {
        EXPECT_EQ(ref.Width.Min, width.Min) << "ERROR: width.Min != ref.Width.Min \n";
        EXPECT_EQ(ref.Width.Max, width.Max) << "ERROR: width.Max != ref.Width.Max \n";
        EXPECT_EQ(ref.Width.Step, width.Step) << "ERROR: width.Step != ref.Width.Step \n";
        EXPECT_EQ(ref.Height.Min, height.Min) << "ERROR: height.Max != ref.Height.Max \n";
        EXPECT_EQ(ref.Height.Max, height.Max) << "ERROR: height.Max != ref.Height.Max \n";
        EXPECT_EQ(ref.Height.Step, height.Step) << "ERROR: height.Step != ref.Height.Step \n";

        EXPECT_EQ(ref.ColorFormats.size(), numColorFormat) << "ERROR: ColorFormats numbers incorrect \n";

        for (mfxU32 i = 0; i < numColorFormat; ++i)
        {
            EXPECT_EQ((ref.ColorFormats.find(formats[i]) != ref.ColorFormats.end()), true)
                << "ERROR: Reported format cannot be found in reference \n";
        }
        return MFX_ERR_NONE;
    }

    RefDesVpp& GetVppReference()
    {
        switch (g_tsHWtype)
        {
        case MFX_HW_XE_HP_SDV:
            return ref_ATS_vpp.GetReference();
        case MFX_HW_DG2:
            return ref_DG2_vpp.GetReference();
        default:
            break;
        }
        EXPECT_TRUE(false) << "ERROR: Not platform detected, pls set env var TS_PLATFORM\n";
        return ref_default_vpp.GetReference();
    }
    mfxStatus CheckFilter(mfxU16 MaxDelayInFrames, mfxU16 NumMemTypes, const FilterDesc& ref)
    {
        EXPECT_EQ(ref.MaxDelayInFrames, MaxDelayInFrames) << "ERROR: Incorrect MaxDelayInFrames \n";
        EXPECT_EQ(ref.MemDescVps.size(), NumMemTypes) << "ERROR: Incorrect MemDesc number \n";
        return MFX_ERR_NONE;
    }
    mfxStatus CheckMemDescVp(mfxRange32U& width, mfxRange32U& height, mfxU16 numInFormat, const MemDescVp& ref)
    {
        EXPECT_EQ(ref.Width.Min, width.Min) << "ERROR: width.Min != ref.Width.Min \n";
        EXPECT_EQ(ref.Width.Max, width.Max) << "ERROR: width.Max != ref.Width.Max \n";
        EXPECT_EQ(ref.Width.Step, width.Step) << "ERROR: width.Step != ref.Width.Step \n";
        EXPECT_EQ(ref.Height.Min, height.Min) << "ERROR: height.Max != ref.Height.Max \n";
        EXPECT_EQ(ref.Height.Max, height.Max) << "ERROR: height.Max != ref.Height.Max \n";
        EXPECT_EQ(ref.Height.Step, height.Step) << "ERROR: height.Step != ref.Height.Step \n";

        EXPECT_EQ(numInFormat, ref.InFormats.size()) << "ERROR: InFormats numbers incorrect \n";
        return MFX_ERR_NONE;
    }
    mfxStatus CheckFormatDesc(mfxU16 numOutFormat, mfxU32* formats, const MemDescVp& ref)
    {
        EXPECT_EQ(ref.OutFormats.size(), numOutFormat) << "ERROR: OutFormats numbers incorrect \n";

        for (mfxU32 i = 0; i < numOutFormat; ++i)
        {
            EXPECT_EQ((ref.OutFormats.find(formats[i]) != ref.OutFormats.end()), true)
                << "ERROR: Reported out format cannot be found in reference \n";
        }
        return MFX_ERR_NONE;
    }

    ReferenceCommonImplsDescription& GetCommonReference()
    {
        switch (g_tsHWtype)
        {
        case MFX_HW_XE_HP_SDV:
            return ref_ATS_common.GetReference();
        case MFX_HW_DG2:
            return ref_DG2_common.GetReference();
        default:
            break;
        }

        EXPECT_TRUE(false) << "ERROR: Not platform detected, pls set env var TS_PLATFORM\n";
        return ref_default_common.GetReference();
    }

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ CASE_DECODE},
    {/* 1*/ CASE_ENCODE},
    {/* 2*/ CASE_VPP},
    {/* 3*/ CASE_COMMON},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START

    auto loader = MFXLoad();
    
    // TODO: checking device id instead of always using 1st impl
    mfxU32 NumImpls = 1;
    mfxImplDescription* implDes = nullptr;
    mfxHDL *hdls = (mfxHDL *)&implDes;
    g_tsStatus.check(MFXEnumImplementations(loader, 0, MFX_IMPLCAPS_IMPLDESCSTRUCTURE, hdls));
    TS_TRACE(NumImpls);

    for (mfxU32 i = 0; i < NumImpls; ++i)
    {
        auto& impl = *(mfxImplDescription*)hdls[i];

        switch (id)
        {
        case 0: // Decode description check
            {
                INC_PADDING();
                TS_TRACE(impl.Dec.NumCodecs);
                auto ref = GetCodecReference(false);
                EXPECT_EQ(ref.Codecs.size(), impl.Dec.NumCodecs) << "ERROR: Incorrect Codecs number \n";

                INC_PADDING();
                for (auto pCodec = impl.Dec.Codecs; pCodec < impl.Dec.Codecs + impl.Dec.NumCodecs; ++pCodec)
                {
                    std::string CodecID = std::string((char*)&pCodec->CodecID, 4);
                    TS_TRACE(CodecID);
                    TS_TRACE(pCodec->MaxcodecLevel);
                    TS_TRACE(pCodec->NumProfiles);
                    if (ref.Codecs.find(pCodec->CodecID) == ref.Codecs.end())
                    {
                        EXPECT_TRUE(false) << "ERROR: CodecsID cannot be found in ref\n";
                        continue;
                    }
                    auto codecRef = ref.Codecs[pCodec->CodecID];
            
                    CheckCodec(pCodec->MaxcodecLevel, pCodec->NumProfiles, codecRef);
                    ProfileDesc currentRefProfile = {};
            
                    INC_PADDING();
                    for (auto pProfile = pCodec->Profiles; pProfile < pCodec->Profiles + pCodec->NumProfiles; ++pProfile)
                    {
                        TS_TRACE(pProfile->Profile);
                        TS_TRACE(pProfile->NumMemTypes);

                        if (codecRef.Profiles.find(pProfile->Profile) == codecRef.Profiles.end())
                        {
                            EXPECT_TRUE(false) << "ERROR: Profile cannot be found in ref\n";
                            continue;
                        }
                        // Profile with empty memDescs means re-using previous profile's memDesc as reference.
                        if (codecRef.Profiles[pProfile->Profile].memDescs.size()) currentRefProfile = codecRef.Profiles[pProfile->Profile];
                        CheckProfile(pProfile->NumMemTypes, currentRefProfile);
            
                        INC_PADDING();
                        for (auto pMem = pProfile->MemDesc; pMem < pProfile->MemDesc + pProfile->NumMemTypes; ++pMem)
                        {
                            TS_TRACE(pMem->MemHandleType);
                            TS_TRACE(pMem->Width.Min); TS_TRACE(pMem->Width.Max); TS_TRACE(pMem->Width.Step);
                            TS_TRACE(pMem->Height.Min); TS_TRACE(pMem->Height.Max); TS_TRACE(pMem->Height.Step);
                            TS_TRACE(pMem->NumColorFormats);
                            if (g_tsTrace) { INC_PADDING(); g_tsLog << g_tsLog.m_off << "pMem->ColorFormats = "
                                << std::string((char*)pMem->ColorFormats, pMem->NumColorFormats * 4) << "\n"; DEC_PADDING(); }
                            if (currentRefProfile.memDescs.find(pMem->MemHandleType) == currentRefProfile.memDescs.end())
                            {
                                EXPECT_TRUE(false) << "ERROR: MemHandleType cannot be found in ref\n";
                                continue;
                            }
                            auto memDescRef = currentRefProfile.memDescs[pMem->MemHandleType];
                            CheckMemDesc(pMem->Width, pMem->Height, pMem->NumColorFormats, pMem->ColorFormats, memDescRef);
                        }
                        DEC_PADDING();
                    }
                    DEC_PADDING();
                }
                DEC_PADDING();
                DEC_PADDING();
            }
            break;
        case 1: // Encode description check
            {
                INC_PADDING();
                TS_TRACE(impl.Dec.NumCodecs);
                auto ref = GetCodecReference(true);
                EXPECT_EQ(ref.Codecs.size(), impl.Enc.NumCodecs) << "ERROR: Incorrect Codecs number \n";

                INC_PADDING();
                for (auto pCodec = impl.Enc.Codecs; pCodec < impl.Enc.Codecs + impl.Enc.NumCodecs; ++pCodec)
                {
                    std::string CodecID = std::string((char*)&pCodec->CodecID, 4);
                    TS_TRACE(CodecID);
                    TS_TRACE(pCodec->BiDirectionalPrediction);
                    TS_TRACE(pCodec->MaxcodecLevel);
                    TS_TRACE(pCodec->NumProfiles);
                    if (ref.Codecs.find(pCodec->CodecID) == ref.Codecs.end())
                    {
                        EXPECT_TRUE(false) << "ERROR: CodecsID cannot be found in ref\n";
                        continue;
                    }
                    auto codecRef = ref.Codecs[pCodec->CodecID];
                
                    CheckCodec(pCodec->MaxcodecLevel, pCodec->NumProfiles, codecRef);
                    CheckCodecEncode(pCodec->BiDirectionalPrediction, codecRef);
                    ProfileDesc currentRefProfile = {};

                    INC_PADDING();
                    for (auto pProfile = pCodec->Profiles; pProfile < pCodec->Profiles + pCodec->NumProfiles; ++pProfile)
                    {
                        TS_TRACE(pProfile->Profile);
                        TS_TRACE(pProfile->NumMemTypes);

                        if (codecRef.Profiles.find(pProfile->Profile) == codecRef.Profiles.end())
                        {
                            EXPECT_TRUE(false) << "ERROR: Profile cannot be found in ref\n";
                            continue;
                        }
                        // Profile with empty memDesc means re-using previous profile's memDesc as reference.
                        if (codecRef.Profiles[pProfile->Profile].memDescs.size()) currentRefProfile = codecRef.Profiles[pProfile->Profile];
                        CheckProfile(pProfile->NumMemTypes, currentRefProfile);

                        INC_PADDING();
                        for (auto pMem = pProfile->MemDesc; pMem < pProfile->MemDesc + pProfile->NumMemTypes; ++pMem)
                        {
                            TS_TRACE(pMem->MemHandleType);
                            TS_TRACE(pMem->Width.Min); TS_TRACE(pMem->Width.Max); TS_TRACE(pMem->Width.Step);
                            TS_TRACE(pMem->Height.Min); TS_TRACE(pMem->Height.Max); TS_TRACE(pMem->Height.Step);
                            TS_TRACE(pMem->NumColorFormats);
                            if (g_tsTrace) { INC_PADDING(); g_tsLog << g_tsLog.m_off << "pMem->ColorFormats = "
                                << std::string((char*)pMem->ColorFormats, pMem->NumColorFormats * 4) << "\n"; DEC_PADDING(); }
                            if (currentRefProfile.memDescs.find(pMem->MemHandleType) == currentRefProfile.memDescs.end())
                            {
                                EXPECT_TRUE(false) << "ERROR: MemHandleType cannot be found in ref\n";
                                continue;
                            }
                            auto memDescRef = currentRefProfile.memDescs[pMem->MemHandleType];
                            CheckMemDesc(pMem->Width, pMem->Height, pMem->NumColorFormats, pMem->ColorFormats, memDescRef);
                        }
                        DEC_PADDING();
                    }
                    DEC_PADDING();
                }
                DEC_PADDING();
                DEC_PADDING();
            }
            break;
        case 2: // Vpp description check
            {
                INC_PADDING();
                TS_TRACE(impl.VPP.NumFilters);
                auto ref = GetVppReference();
                EXPECT_EQ(ref.Filters.size(), impl.VPP.NumFilters) << "ERROR: Incorrect Filters number \n";

                INC_PADDING();
                for (auto pFilter = impl.VPP.Filters; pFilter < impl.VPP.Filters + impl.VPP.NumFilters; ++pFilter)
                {
                    std::string FilterID = std::string((char*)&pFilter->FilterFourCC, 4);
                    TS_TRACE(FilterID);
                    TS_TRACE(pFilter->MaxDelayInFrames);
                    TS_TRACE(pFilter->NumMemTypes);

                    if (ref.Filters.find(pFilter->FilterFourCC) == ref.Filters.end())
                    {
                        EXPECT_TRUE(false) << "ERROR: FilterID cannot be found in ref\n";
                        continue;
                    }
                    auto filterRef = ref.Filters[pFilter->FilterFourCC];
                    CheckFilter(pFilter->MaxDelayInFrames, pFilter->NumMemTypes, filterRef);

                    INC_PADDING();
                    for (auto pMem = pFilter->MemDesc; pMem < pFilter->MemDesc + pFilter->NumMemTypes; ++pMem)
                    {
                        TS_TRACE(pMem->MemHandleType);
                        TS_TRACE(pMem->Width.Min); TS_TRACE(pMem->Width.Max); TS_TRACE(pMem->Width.Step);
                        TS_TRACE(pMem->Height.Min); TS_TRACE(pMem->Height.Max); TS_TRACE(pMem->Height.Step);
                        TS_TRACE(pMem->NumInFormats);
                
                        if (filterRef.MemDescVps.find(pMem->MemHandleType) == filterRef.MemDescVps.end())
                        {
                            EXPECT_TRUE(false) << "ERROR: MemHandleType cannot be found in ref\n";
                            continue;
                        }
                        auto memDescRef = filterRef.MemDescVps[pMem->MemHandleType];
                        CheckMemDescVp(pMem->Width, pMem->Height, pMem->NumInFormats, memDescRef);

                        INC_PADDING();
                        for (auto pInFormat = pMem->Formats; pInFormat < pMem->Formats + pMem->NumInFormats; ++pInFormat)
                        {
                            char* InFormat = (char*)&pInFormat->InFormat;
                            TS_TRACE(InFormat);
                            TS_TRACE(pInFormat->NumOutFormat);
                            if (g_tsTrace) { INC_PADDING(); g_tsLog << g_tsLog.m_off << "pInFormat->OutFormats = "
                                << std::string((char*)pInFormat->OutFormats, pInFormat->NumOutFormat * 4) << "\n"; DEC_PADDING(); }

                            if (memDescRef.InFormats.find(pInFormat->InFormat) == memDescRef.InFormats.end())
                            {
                                EXPECT_TRUE(false) << "ERROR: InFormat cannot be found in ref\n";
                                continue;
                            }
                            CheckFormatDesc(pInFormat->NumOutFormat, pInFormat->OutFormats, memDescRef);
                        }
                        DEC_PADDING();
                    }
                    DEC_PADDING();
                }
                DEC_PADDING();
                DEC_PADDING();
            }
            break;
        case 3: // Common description check
            {
                auto TransToHex = [&](mfxU32 value) -> std::string
                {
                    std::stringstream stream;
                    stream << "0x" << std::hex << value;
                    std::string out;
                    stream >> out;
                    return out;
                };
                if (g_tsTrace) { INC_PADDING(); g_tsLog << g_tsLog.m_off << "impl.Version = "
                    << (mfxU16)impl.Version.Major << "." << (mfxU16)impl.Version.Minor << "\n"; DEC_PADDING(); }
                TS_TRACE(impl.Impl);
                auto AccelerationMode = TransToHex(impl.AccelerationMode);
                TS_TRACE(AccelerationMode);
                if (g_tsTrace) { INC_PADDING(); g_tsLog << g_tsLog.m_off << "impl.ApiVersion = "
                    << impl.ApiVersion.Major << "." << impl.ApiVersion.Minor << "\n"; DEC_PADDING(); }
                TS_TRACE(impl.ImplName);
                TS_TRACE(impl.License);
                TS_TRACE(impl.Keywords);
                auto VendorID = TransToHex(impl.VendorID);
                TS_TRACE(VendorID);
                TS_TRACE(impl.VendorImplID);
                TS_TRACE(impl.Dev.DeviceID);

                auto ref = GetCommonReference();
                EXPECT_EQ(ref.Version.Major, impl.Version.Major);
                EXPECT_EQ(ref.Version.Minor, impl.Version.Minor);
                EXPECT_EQ(ref.Impl, impl.Impl);
                EXPECT_EQ(ref.AccelerationMode, impl.AccelerationMode);
                EXPECT_EQ(true, ref.ApiVersion.Major <= impl.ApiVersion.Major);
                EXPECT_EQ(true, ref.ApiVersion.Minor <= impl.ApiVersion.Minor);
                EXPECT_EQ(std::string(ref.ImplName), std::string(impl.ImplName));
                EXPECT_EQ(std::string(ref.License), std::string(impl.License));
                EXPECT_EQ(std::string(ref.Keywords), std::string(impl.Keywords));
                EXPECT_EQ(ref.VendorID, impl.VendorID);
                EXPECT_EQ(ref.VendorImplID, impl.VendorImplID);

                mfxU32 DeviceID = 0;
                EXPECT_EQ(1, sscanf(impl.Dev.DeviceID, "%x", &DeviceID));
                EXPECT_TRUE(std::find(ref.DeviceList.begin(), ref.DeviceList.end(), DeviceID) != ref.DeviceList.end());
            }
        break;
        default:
            break;
        }
        break; // Only check 1st impl
    }

    TS_END
    return 0;
}

TS_REG_TEST_SUITE_CLASS(query_impls_description);

}
#endif
