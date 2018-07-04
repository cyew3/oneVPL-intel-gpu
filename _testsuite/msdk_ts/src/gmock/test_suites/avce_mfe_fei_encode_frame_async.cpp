/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

/*!

\file avce_mfe_fei_encode_frame_async.cpp
\brief Gmock test avce_mfe_fei_encode_frame_async

Description:

This test checks behavior of EncodeFrameAsync() when FEI ENCODE and MFE
are enabled

Algorithm:
- Initialize SDK session sessions (one parent and two child)
- Set video parameters including FEI and MFE for each SDK sessions
- Initialize encoders for each SDK sessions
- Allocate surfaces and bitstreams for each encoder
- Call EncodeFrameAsync() for each encoder
- Sync each encode operations
- Check encode status
- Repeat encoding process for frames_to_encode times
*/

#include <type_traits>

#include "avce_mfe_async_encode.h"

namespace avce_mfe_async_encode
{

class TestSuite : public AsyncEncodeTest {
public:
    //! \brief A constructor
    TestSuite() : AsyncEncodeTest(/*mfe_enabled*/true) {};
    //! \brief A destructor
    ~TestSuite() {}
};

TS_REG_TEST_SUITE_CLASS(avce_mfe_fei_encode_frame_async);

};
