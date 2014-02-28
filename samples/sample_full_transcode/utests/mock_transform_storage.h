//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "transform_storage.h"
#include <gmock/gmock.h>

class MockTransformStorage  : public TransformStorage {
public:
    MOCK_METHOD2(RegisterTransform, void (const TransformConfigDesc &desc, std::auto_ptr<ITransform> & transform ));
    MOCK_METHOD1(begin, iterator (int id));
    MOCK_METHOD1(end, iterator (int id));
    MOCK_METHOD1(empty, bool (int id));
};
