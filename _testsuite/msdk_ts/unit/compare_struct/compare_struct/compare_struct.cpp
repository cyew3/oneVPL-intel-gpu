//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2015 - 2017 Intel Corporation. All Rights Reserved.
//

#include "gtest/gtest.h"

#include "mfx_reflect.h"
#include "mfxstructures.h"
#include "mfx_trace.h"
#include "mfx_common_int.h"

#include "Windows.h"
#include "WinBase.h"

typedef std::pair<mfx_reflect::AccessorField, mfx_reflect::AccessorField> PairResult_t;

class PairResult : public PairResult_t
{
public:
    PairResult(mfx_reflect::AccessorField field1, mfx_reflect::AccessorField field2) : PairResult_t(field1, field2)
    {}

    PairResult(mfx_reflect::AccessorField field, std::ptrdiff_t delta) : PairResult_t(field, field)
    {
        second.Move(delta); // The second value is calculate using the offset equal to the difference between the addresses of the structures 'in' and 'out'
    }
};

class TestCompareTwoStructs : public ::testing::Test
{
protected:
    void SetUp()
    {
        m_collection.DeclareMsdkStructs();
    }

    void TearDown()
    {}

    mfx_reflect::AccessibleTypesCollection m_collection;
    std::list<PairResult> resultList;

    bool CompareResultWithNestedFields(const mfx_reflect::TypeComparisonResultP result, std::list<PairResult> &resultList)
    {
        for (std::list<mfx_reflect::FieldComparisonResult>::iterator i = result->begin(); i != result->end(); ++i)
        {
            mfx_reflect::TypeComparisonResultP subtypeResult = i->subtypeComparisonResultP;

            if (NULL != subtypeResult)
            {
                if (!CompareResultWithNestedFields(subtypeResult, resultList)) return false;
            }
            else
            {
                try
                {
                    if (!(i->accessorField1.Equal(resultList.front().first)) || !(i->accessorField2.Equal(resultList.front().second)))
                        return false;
                    else
                        resultList.pop_front();
                }
                catch (std::invalid_argument errorString)
                {
                    printf("ERROR: %s \n", errorString.what());
                    return false;
                }
            }
        }
        return true;
    }
};

/*-----------------------------------------------------------------------------------*/
TEST_F(TestCompareTwoStructs, Test_with_changed_field_array_STR)
{
    mfxExtCodingOption3 in = {}, out = {};

    in.NumRefActiveP[0] = 1; out.NumRefActiveP[0] = 2;

    std::string result = mfx_reflect::CompareStructsToString(m_collection.Access(&in), m_collection.Access(&out));

    ASSERT_EQ(result, "Incompatible VideoParams were updated:\nmfxExtCodingOption3.NumRefActiveP[0] = 1 -> 2\n");
}

TEST_F(TestCompareTwoStructs, Test_with_two_changed_fields_STR)
{
    mfxExtCodingOption3 in = {}, out = {};

    in.BitstreamRestriction = 1; out.BitstreamRestriction = 5;
    in.NumRefActiveP[0] = 1; out.NumRefActiveP[0] = 2;

    std::string result = mfx_reflect::CompareStructsToString(m_collection.Access(&in), m_collection.Access(&out));

    ASSERT_EQ(result, "Incompatible VideoParams were updated:\nmfxExtCodingOption3.BitstreamRestriction = 1 -> 5\nmfxExtCodingOption3.NumRefActiveP[0] = 1 -> 2\n");
}

TEST_F(TestCompareTwoStructs, Test_with_changed_nested_fields_STR)
{
    mfxVideoParam in = {}, out = {};

    in.mfx.NumSlice = 1; out.mfx.NumSlice = 2; // Along with mfx.NumSlice changes vpp.Out.FrameId.QualityId

    in.mfx.InitialDelayInKB = 3; out.mfx.InitialDelayInKB = 5; // Along with mfx.InitialDelayInKB changes mfx.QPI, mfx.Accuracy and vpp.Out.Shift

    std::string result = mfx_reflect::CompareStructsToString(m_collection.Access(&in), m_collection.Access(&out));

    ASSERT_EQ(result, "Incompatible VideoParams were updated:\nmfxVideoParam.mfx.InitialDelayInKB = 3 -> 5\nmfxVideoParam.mfx.QPI = 3 -> 5\nmfxVideoParam.mfx.Accuracy = 3 -> 5\nmfxVideoParam.mfx.NumSlice = 1 -> 2\nmfxVideoParam.vpp.Out.FrameId.QualityId = 1 -> 2\nmfxVideoParam.vpp.Out.Shift = 3 -> 5\n");
}

TEST_F(TestCompareTwoStructs, Test_with_changed_nested_fields_array_STR)
{
    mfxExtVP9Segmentation in = {}, out = {};

    in.Segment[0].FeatureEnabled = 1; out.Segment[0].FeatureEnabled = 2;
    in.Segment[6].LoopFilterLevelDelta = 4; out.Segment[6].LoopFilterLevelDelta = 5;

    std::string result = mfx_reflect::CompareStructsToString(m_collection.Access(&in), m_collection.Access(&out));

    ASSERT_EQ(result, "Incompatible VideoParams were updated:\nmfxExtVP9Segmentation.Segment[0].FeatureEnabled = 1 -> 2\nmfxExtVP9Segmentation.Segment[6].LoopFilterLevelDelta = 4 -> 5\n");
}

TEST_F(TestCompareTwoStructs, Test_with_changed_extended_parameters_STR)
{
    mfxVideoParam in = {}, out = {};

    in.mfx.NumSlice = 1; out.mfx.NumSlice = 2; // Along with mfx.NumSlice changes vpp.Out.FrameId.QualityId

    ExtendedBuffer extBuff_in, extBuff_out;
    extBuff_in.AddTypedBuffer<mfxExtCodingOption2>(MFX_EXTBUFF_CODING_OPTION2);
    extBuff_out.AddTypedBuffer<mfxExtCodingOption2>(MFX_EXTBUFF_CODING_OPTION2);
    extBuff_in.AddTypedBuffer<mfxExtCodingOption3>(MFX_EXTBUFF_CODING_OPTION3);
    extBuff_out.AddTypedBuffer<mfxExtCodingOption3>(MFX_EXTBUFF_CODING_OPTION3);

    extBuff_in.GetBufferByPosition<mfxExtCodingOption2>(0)->MaxFrameSize = 1; extBuff_out.GetBufferByPosition<mfxExtCodingOption2>(0)->MaxFrameSize = 2;
    extBuff_in.GetBufferByPosition<mfxExtCodingOption3>(1)->NumSliceB = 3; extBuff_out.GetBufferByPosition<mfxExtCodingOption3>(1)->NumSliceB = 5;

    in.ExtParam = extBuff_in.GetBuffers(); out.ExtParam = extBuff_out.GetBuffers();
    in.NumExtParam = extBuff_in.GetCount(); out.NumExtParam = extBuff_out.GetCount();

    std::string result = mfx_reflect::CompareStructsToString(m_collection.Access(&in), m_collection.Access(&out));

    ASSERT_EQ(result, "Incompatible VideoParams were updated:\nmfxVideoParam.mfx.NumSlice = 1 -> 2\nmfxVideoParam.vpp.Out.FrameId.QualityId = 1 -> 2\nmfxExtCodingOption2.MaxFrameSize = 1 -> 2\nmfxExtCodingOption3.NumSliceB = 3 -> 5\n");
}

/*-----------------------------------------------------------------------------------*/
TEST_F(TestCompareTwoStructs, Test_with_changed_field_array)
{
    mfxExtCodingOption3 in = {}, out = {};

    in.NumRefActiveP[1] = 1; out.NumRefActiveP[1] = 2;

    std::ptrdiff_t delta = reinterpret_cast<char*>(&out) - reinterpret_cast<char*>(&in);

    mfx_reflect::AccessorField fieldArray = m_collection.Access(&in).AccessField("NumRefActiveP");
    fieldArray.SetIndexElement(1);

    resultList.push_back(PairResult(fieldArray, delta));

    mfx_reflect::TypeComparisonResultP result = mfx_reflect::CompareTwoStructs(m_collection.Access(&in), m_collection.Access(&out));

    ASSERT_EQ(CompareResultWithNestedFields(result, resultList), true);
    ASSERT_EQ(resultList.size(), 0);
}

TEST_F(TestCompareTwoStructs, Test_with_two_changed_fields)
{
    mfxExtCodingOption3 in = {}, out = {};

    in.BitstreamRestriction = 3; out.BitstreamRestriction = 5;
    in.NumRefActiveP[2] = 1; out.NumRefActiveP[2] = 2;

    std::ptrdiff_t delta = reinterpret_cast<char*>(&out) - reinterpret_cast<char*>(&in);

    mfx_reflect::AccessorField fieldArray = m_collection.Access(&in).AccessField("NumRefActiveP");
    fieldArray.SetIndexElement(2);

    resultList.push_back(PairResult(m_collection.Access(&in).AccessField("BitstreamRestriction"), delta));
    resultList.push_back(PairResult(fieldArray, delta));

    mfx_reflect::TypeComparisonResultP result = mfx_reflect::CompareTwoStructs(m_collection.Access(&in), m_collection.Access(&out));

    ASSERT_EQ(CompareResultWithNestedFields(result, resultList), true);
    ASSERT_EQ(resultList.size(), 0);
}

TEST_F(TestCompareTwoStructs, Test_with_changed_nested_fields)
{
    mfxVideoParam in = {}, out = {};

    in.mfx.NumSlice = 1; out.mfx.NumSlice = 2; // Along with mfx.NumSlice changes vpp.Out.FrameId.QualityId

    in.mfx.InitialDelayInKB = 3; out.mfx.InitialDelayInKB = 5; // Along with mfx.InitialDelayInKB changes mfx.QPI, mfx.Accuracy and vpp.Out.Shift

    mfx_reflect::AccessorType data_in = m_collection.Access(&in);

    std::ptrdiff_t delta = reinterpret_cast<char*>(&out) - reinterpret_cast<char*>(&in);

    resultList.push_back(PairResult(data_in.AccessSubtype("mfx").AccessField("InitialDelayInKB"), delta));
    resultList.push_back(PairResult(data_in.AccessSubtype("mfx").AccessField("QPI"), delta));
    resultList.push_back(PairResult(data_in.AccessSubtype("mfx").AccessField("Accuracy"), delta));
    resultList.push_back(PairResult(data_in.AccessSubtype("mfx").AccessField("NumSlice"), delta));
    resultList.push_back(PairResult(data_in.AccessSubtype("vpp").AccessSubtype("Out").AccessSubtype("FrameId").AccessField("QualityId"), delta));
    resultList.push_back(PairResult(data_in.AccessSubtype("vpp").AccessSubtype("Out").AccessField("Shift"), delta));

    mfx_reflect::TypeComparisonResultP result = mfx_reflect::CompareTwoStructs(m_collection.Access(&in), m_collection.Access(&out));

    ASSERT_EQ(CompareResultWithNestedFields(result, resultList), true);
    ASSERT_EQ(resultList.size(), 0);
}

TEST_F(TestCompareTwoStructs, Test_with_changed_nested_fields_array)
{
    mfxExtAVCRefLists in = {}, out = {};

    in.RefPicList0[1].FrameOrder = 1; out.RefPicList0[1].FrameOrder = 2;
    in.RefPicList1[2].PicStruct = 3;  out.RefPicList1[2].PicStruct = 5;

    mfx_reflect::AccessorField fieldArray1 = m_collection.Access(&in).AccessField("RefPicList0");
    fieldArray1.SetIndexElement(1);
    mfx_reflect::AccessorField fieldArray2 = m_collection.Access(&in).AccessField("RefPicList1");
    fieldArray2.SetIndexElement(2);

    std::ptrdiff_t delta = reinterpret_cast<char*>(&out) - reinterpret_cast<char*>(&in);

    resultList.push_back(PairResult(fieldArray1.AccessSubtype().AccessField("FrameOrder"), delta));
    resultList.push_back(PairResult(fieldArray2.AccessSubtype().AccessField("PicStruct"), delta));

    mfx_reflect::TypeComparisonResultP result = mfx_reflect::CompareTwoStructs(m_collection.Access(&in), m_collection.Access(&out));

    ASSERT_EQ(CompareResultWithNestedFields(result, resultList), true);
    ASSERT_EQ(resultList.size(), 0);
}

TEST_F(TestCompareTwoStructs, Test_with_changed_extended_parameters)
{
    mfxVideoParam in = {}, out = {};

    ExtendedBuffer extBuff_in, extBuff_out;
    extBuff_in.AddTypedBuffer<mfxExtCodingOption2>(MFX_EXTBUFF_CODING_OPTION2);
    extBuff_out.AddTypedBuffer<mfxExtCodingOption2>(MFX_EXTBUFF_CODING_OPTION2);
    extBuff_in.AddTypedBuffer<mfxExtCodingOption3>(MFX_EXTBUFF_CODING_OPTION3);
    extBuff_out.AddTypedBuffer<mfxExtCodingOption3>(MFX_EXTBUFF_CODING_OPTION3);

    extBuff_in.GetBufferByPosition<mfxExtCodingOption2>(0)->MaxFrameSize = 1; extBuff_out.GetBufferByPosition<mfxExtCodingOption2>(0)->MaxFrameSize = 2;
    extBuff_in.GetBufferByPosition<mfxExtCodingOption3>(1)->NumSliceB = 3; extBuff_out.GetBufferByPosition<mfxExtCodingOption3>(1)->NumSliceB = 5;

    in.ExtParam = extBuff_in.GetBuffers(); out.ExtParam = extBuff_out.GetBuffers();
    in.NumExtParam = extBuff_in.GetCount(); out.NumExtParam = extBuff_out.GetCount();

    //--------------------------------------------------------------------------------------------------------------------------------------------------
    mfx_reflect::AccessorField field1_in = m_collection.Access(extBuff_in.GetBufferByPosition<mfxExtCodingOption2>(0)).AccessField("MaxFrameSize");
    mfx_reflect::AccessorField field1_out = m_collection.Access(extBuff_out.GetBufferByPosition<mfxExtCodingOption2>(0)).AccessField("MaxFrameSize");
    mfx_reflect::AccessorField field2_in = m_collection.Access(extBuff_in.GetBufferByPosition<mfxExtCodingOption3>(1)).AccessField("NumSliceB");
    mfx_reflect::AccessorField field2_out = m_collection.Access(extBuff_out.GetBufferByPosition<mfxExtCodingOption3>(1)).AccessField("NumSliceB");

    std::ptrdiff_t delta = reinterpret_cast<char*>(&out) - reinterpret_cast<char*>(&in);

    resultList.push_back(PairResult(field1_in, field1_out));
    resultList.push_back(PairResult(field2_in, field2_out));

    mfx_reflect::TypeComparisonResultP result = mfx_reflect::CompareTwoStructs(m_collection.Access(&in), m_collection.Access(&out));

    ASSERT_EQ(CompareResultWithNestedFields(result, resultList), true);
    ASSERT_EQ(resultList.size(), 0);
}

/*-----------------------------------------------------------------------------------*/
TEST_F(TestCompareTwoStructs, Test_with_extreme_parametrs)
{
    mfxExtCodingOption3 in = {}, out = {};

    in.QPOffset[0] = -32767; out.QPOffset[0] = 32767;
    in.NumRefActiveP[0] = 0; out.NumRefActiveP[0] = 65535;
    in.MaxFrameSizeP = 0; out.MaxFrameSizeP = 4294267265;

    ASSERT_NO_THROW(mfx_reflect::CompareTwoStructs(m_collection.Access(&in), m_collection.Access(&out)));
}

TEST_F(TestCompareTwoStructs, Test_with_different_types_of_initial_data)
{
    mfxExtCodingOption3 in  = {};
    mfxExtCodingOption2 out = {};

    ASSERT_ANY_THROW(mfx_reflect::CompareTwoStructs(m_collection.Access(&in), m_collection.Access(&out)));
}

int main(int argc, char *argv[]) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}