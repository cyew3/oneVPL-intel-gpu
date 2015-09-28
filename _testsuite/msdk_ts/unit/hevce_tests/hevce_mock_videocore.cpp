#include "stdexcept"

#include "gtest/gtest.h"

#include "hevce_common_api.h"
#include "hevce_mock_videocore.h"

using namespace ApiTestCommon;

MockMFXCoreInterface::MockMFXCoreInterface()
    : paramset()
{
//    Zero(memIds);
}

void MockMFXCoreInterface::SetParamSet(const ParamSet &param)
{ 
    paramset = &param;
}
