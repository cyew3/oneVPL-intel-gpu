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

#define OPTION_I MSDK_CHAR("-i")
#define OPTION_O MSDK_CHAR("-o")
#define OPTION_HW MSDK_CHAR("-hw")
#define OPTION_SW MSDK_CHAR("-sw")
#define OPTION_ASW MSDK_CHAR("-asw")
#define OPTION_VB MSDK_CHAR("-v:b")
#define OPTION_AB MSDK_CHAR("-a:b")
#define OPTION_LOOP MSDK_CHAR("-loop")
#define OPTION_ADEPTH MSDK_CHAR("-async")
#define OPTION_D3D11 MSDK_CHAR("-d3d11")
#define OPTION_H MSDK_CHAR("-h")
#define OPTION_W MSDK_CHAR("-w")
#define OPTION_ACODEC MSDK_CHAR("-acodec")
#define OPTION_VCODEC MSDK_CHAR("-vcodec")
#define OPTION_ACODEC_COPY MSDK_CHAR("-acodec:copy")
//TODO: implement parsing and handling
#define OPTION_FORMAT MSDK_CHAR("-format")
#define OPTION_VDECPLG MSDK_CHAR("-vdecplugin")
#define OPTION_PLG MSDK_CHAR("-plugin")
#define OPTION_VENCPLG MSDK_CHAR("-vencplugin")
//place holder instead of codec type
#define ARG_COPY MSDK_CHAR("copy")

//logging level for all piepline
#define OPTION_TRACELEVEL MSDK_CHAR("-trace_level")