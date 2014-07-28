//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
//*/

#pragma once

#include "gtest/gtest.h"
#include <gmock/gmock.h>
#include "gmock/gmock-matchers.h"
#include "gmock/gmock-more-actions.h"
#include "gmock/gmock-spec-builders.h"
#include "gmock/gmock-actions.h"

#include "test_utils.h"
#include "string_convert.h"

#include "mock_cmdline_parser.h"
#include "mock_mfxvideo++.h"
#include "mock_pipeline_factory.h"
#include "mock_allocator.h"
#include "mock_itransform.h"
#include "mock_splitter.h"
#include "mock_fileio.h"
#include "mock_mfxvideo++.h"
#include "mock_isample.h"
#include "mock_transform_storage.h"
#include "mock_isink.h"
#include "mock_isource.h"
#include "mock_itransform.h"
#include "mock_isample.h"
#include "mock_session_storage.h"
#include "mock_fileio.h"
#include "mock_pipeline_profile.h"
#include "mock_muxer.h"
#include "mock_samples_pool.h"
#include "mock_hwdevice.h"

using namespace ::testing;

#if defined(_WIN32) || defined(_WIN64)
#define remove _tremove
#define fopen _tfopen
#endif // #if defined(_WIN32) || defined(_WIN64)

#ifndef _WIN32 || _WIN64
//#include <inttypes.h>
//#define int64_t long long
typedef long long __int64;
#else
//#define int64_t __int64

#endif // #ifndef _WIN32 || _WIN64