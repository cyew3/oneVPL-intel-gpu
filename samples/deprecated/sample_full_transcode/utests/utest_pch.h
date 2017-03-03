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