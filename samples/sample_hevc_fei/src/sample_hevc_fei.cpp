/******************************************************************************\
Copyright (c) 2017, Intel Corporation
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

#include "version.h"
#include "pipeline_hevc_fei.h"

mfxStatus CheckOptions(const sInputParams params, const msdk_char* appName);
void AdjustOptions(sInputParams& params);

// if last option requires value, check that it passed to app
#define CHECK_NEXT_VAL(val, argName, app) \
{ \
    if (val) \
    { \
        msdk_printf(MSDK_STRING("ERROR: Input argument \"%s\" require more parameters\n"), argName); \
        PrintHelp(app, NULL); \
        return MFX_ERR_UNSUPPORTED;\
    } \
}

#define PARSE_CHECK(sts, optName, isParseInvalid) \
{ \
    if (MFX_ERR_NONE != sts) \
    { \
        msdk_printf(MSDK_STRING("ERROR: %s is invalid\n"), optName); \
        isParseInvalid = true; \
    } \
}

void PrintHelp(const msdk_char *strAppName, const msdk_char *strErrorMessage)
{
    msdk_printf(MSDK_STRING("\nIntel(R) Media SDK HEVC FEI Encoding Sample Version %s\n\n"), GetMSDKSampleVersion().c_str());

    if (strErrorMessage)
    {
        msdk_printf(MSDK_STRING("ERROR: %s\n"), strErrorMessage);
    }
    msdk_printf(MSDK_STRING("Usage: %s [<options>] -i InputYUVFile -o OutputEncodedFile -w width -h height\n"), strAppName);
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Options: \n"));
    msdk_printf(MSDK_STRING("   [-nv12] - input is in NV12 color format, if not specified YUV420 is expected\n"));
    msdk_printf(MSDK_STRING("   [-tff|bff|mixed] - input stream is interlaced, top|bottom field first, if not specified progressive is expected.\n"));
    msdk_printf(MSDK_STRING("                    - mixed means that picture structre should be obtained from the input stream\n"));
    msdk_printf(MSDK_STRING("   [-encode] - use extended FEI interface ENC+PAK (FEI ENCODE) (RC is forced to constant QP)\n"));
    msdk_printf(MSDK_STRING("   [-EncodedOrder] - use app-level reordering to encoded order (default is display; ENCODE only)\n"));
    msdk_printf(MSDK_STRING("   [-n number] - number of frames to process\n"));
    msdk_printf(MSDK_STRING("   [-qp qp_value] - QP value for frames (default is 26)\n"));
    msdk_printf(MSDK_STRING("   [-f frameRate] - video frame rate (frames per second)\n"));
    msdk_printf(MSDK_STRING("   [-idr_interval size] - idr interval, default 0 means every I is an IDR, 1 means every other I frame is an IDR etc (default is infinite)\n"));
    msdk_printf(MSDK_STRING("   [-g size] - GOP size (1(default) means I-frames only)\n"));
    msdk_printf(MSDK_STRING("   [-gop_opt closed|strict] - GOP optimization flags (can be used together)\n"));
    msdk_printf(MSDK_STRING("   [-r (-GopRefDist) distance] - Distance between I- or P- key frames (1 means no B-frames) (0 - by default(I frames))\n"));
    msdk_printf(MSDK_STRING("   [-num_ref (-NumRefFrame) numRefs] - number of reference frames \n"));
    msdk_printf(MSDK_STRING("   [-gpb:<on,off>] - make HEVC encoder use regular P-frames (off) or GPB (on) (on - by default)\n"));
    msdk_printf(MSDK_STRING("   [-bref] - arrange B frames in B pyramid reference structure\n"));
    msdk_printf(MSDK_STRING("   [-nobref] - do not use B-pyramid (by default the decision is made by library)\n"));
    msdk_printf(MSDK_STRING("   [-l numSlices] - number of slices \n"));
    msdk_printf(MSDK_STRING("   [-preenc] - use extended FEI interface PREENC (RC is forced to constant QP)\n"));
    msdk_printf(MSDK_STRING("\n"));
}

mfxStatus ParseInputString(msdk_char* strInput[], mfxU32 nArgNum, sInputParams& params)
{
    if (1 == nArgNum)
    {
        PrintHelp(strInput[0], MSDK_STRING("Not enough input parameters"));
        return MFX_ERR_UNSUPPORTED;
    }

    bool isParseInvalid = false; // show help once after all parsing errors will be processed

    // parse command line parameters
    for (mfxU32 i = 1; i < nArgNum; i++)
    {
        MSDK_CHECK_POINTER(strInput[i], MFX_ERR_UNDEFINED_BEHAVIOR);

        // process multi-character options
        if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-encode")))
        {
            params.bENCODE = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-preenc")))
        {
            params.bPREENC = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-EncodedOrder")))
        {
            params.bEncodedOrder = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-nv12")))
        {
            params.ColorFormat = MFX_FOURCC_NV12;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-qp")))
        {
            CHECK_NEXT_VAL(i + 1 >= nArgNum, strInput[i], strInput[0]);
            PARSE_CHECK(msdk_opt_read(strInput[++i], params.QP), "QP", isParseInvalid);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-i")))
        {
            CHECK_NEXT_VAL(i + 1 >= nArgNum, strInput[i], strInput[0]);
            PARSE_CHECK(msdk_opt_read(strInput[++i], params.strSrcFile), "Input file", isParseInvalid);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-o")))
        {
            CHECK_NEXT_VAL(i + 1 >= nArgNum, strInput[i], strInput[0]);
            PARSE_CHECK(msdk_opt_read(strInput[++i], params.strDstFile), "Output file", isParseInvalid);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-w")))
        {
            CHECK_NEXT_VAL(i + 1 >= nArgNum, strInput[i], strInput[0]);
            PARSE_CHECK(msdk_opt_read(strInput[++i], params.nWidth), "Width", isParseInvalid);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-h")))
        {
            CHECK_NEXT_VAL(i + 1 >= nArgNum, strInput[i], strInput[0]);
            PARSE_CHECK(msdk_opt_read(strInput[++i], params.nHeight), "Height", isParseInvalid);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-f")))
        {
            CHECK_NEXT_VAL(i + 1 >= nArgNum, strInput[i], strInput[0]);
            PARSE_CHECK(msdk_opt_read(strInput[++i], params.dFrameRate), "FrameRate", isParseInvalid);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-n")))
        {
            CHECK_NEXT_VAL(i + 1 >= nArgNum, strInput[i], strInput[0]);
            PARSE_CHECK(msdk_opt_read(strInput[++i], params.nNumFrames), "NumFrames", isParseInvalid);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-g")))
        {
            CHECK_NEXT_VAL(i + 1 >= nArgNum, strInput[i], strInput[0]);
            PARSE_CHECK(msdk_opt_read(strInput[++i], params.nGopSize), "GopSize", isParseInvalid);
        }
        else if (   0 == msdk_strcmp(strInput[i], MSDK_STRING("-r"))
                 || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-GopRefDist")))
        {
            CHECK_NEXT_VAL(i + 1 >= nArgNum, strInput[i], strInput[0]);
            PARSE_CHECK(msdk_opt_read(strInput[++i], params.nRefDist), "GopRefDist", isParseInvalid);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-l")))
        {
            CHECK_NEXT_VAL(i + 1 >= nArgNum, strInput[i], strInput[0]);
            PARSE_CHECK(msdk_opt_read(strInput[++i], params.nNumSlices), "NumSlices", isParseInvalid);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bref")))
        {
            params.BRefType = MFX_B_REF_PYRAMID;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-nobref")))
        {
            params.BRefType = MFX_B_REF_OFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-idr_interval")))
        {
            CHECK_NEXT_VAL(i + 1 >= nArgNum, strInput[i], strInput[0]);
            PARSE_CHECK(msdk_opt_read(strInput[++i], params.nIdrInterval), "IdrInterval", isParseInvalid);
        }
        else if (   0 == msdk_strcmp(strInput[i], MSDK_STRING("-num_ref"))
                 || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-NumRefFrame")))
        {
            CHECK_NEXT_VAL(i + 1 >= nArgNum, strInput[i], strInput[0]);
            PARSE_CHECK(msdk_opt_read(strInput[++i], params.nNumRef), "NumRefFrame", isParseInvalid);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-tff")))
        {
            params.nPicStruct = MFX_PICSTRUCT_FIELD_TFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bff")))
        {
            params.nPicStruct = MFX_PICSTRUCT_FIELD_BFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mixed")))
        {
            params.nPicStruct = MFX_PICSTRUCT_UNKNOWN;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-gop_opt")))
        {
            CHECK_NEXT_VAL(i + 1 >= nArgNum, strInput[i], strInput[0]);
            ++i;
            if (0 == msdk_strcmp(strInput[i], MSDK_STRING("closed")))
            {
                params.nGopOptFlag |= MFX_GOP_CLOSED;
            }
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("strict")))
            {
                params.nGopOptFlag |= MFX_GOP_STRICT;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-gpb:on")))
        {
            params.GPB = MFX_CODINGOPTION_ON;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-gpb:off")))
        {
            params.GPB = MFX_CODINGOPTION_OFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("?")))
        {
            PrintHelp(strInput[0], NULL);
            return MFX_ERR_UNSUPPORTED;
        }
        else
        {
            msdk_printf(MSDK_STRING("\nWARNING: Unknown option `%s` will be ignored\n\n"), strInput[i]);
        }
    }

    if (isParseInvalid)
    {
        PrintHelp(strInput[0], NULL);
        return MFX_ERR_UNSUPPORTED;
    }

    if (MFX_ERR_NONE != CheckOptions(params, strInput[0]))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    AdjustOptions(params);

    return MFX_ERR_NONE;
}

mfxStatus CheckOptions(const sInputParams params, const msdk_char* appName)
{
    if (0 == msdk_strlen(params.strSrcFile))
    {
        PrintHelp(appName, "Source file name not found");
        return MFX_ERR_UNSUPPORTED;
    }
    if (0 == params.nWidth || 0 == params.nHeight || params.nWidth < 0 || params.nHeight < 0)
    {
        PrintHelp(appName, "-w -h is not specified");
        return MFX_ERR_UNSUPPORTED;
    }
    if (params.QP > 51)
    {
        PrintHelp(appName, "Wrong QP value (must be in range [0, 51])");
        return MFX_ERR_UNSUPPORTED;
    }
    if (!params.bENCODE && !params.bPREENC)
    {
        PrintHelp(appName, "Pipeline is unsupported. Supported pipeline: ENCODE, PreENC, PreENC + ENCODE");
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

void AdjustOptions(sInputParams& params)
{
    params.dFrameRate   = tune(params.dFrameRate, 0.0, 30.0);
    params.nNumSlices   = tune(params.nNumSlices, 0, 1);
    params.nIdrInterval = tune(params.nIdrInterval, 0, 0xffff);
    params.nGopSize     = tune(params.nGopSize, 0, 1);
    params.nRefDist     = tune(params.nRefDist, 0, 1);

    if (params.nGopSize > 1)
    {
        params.nNumRef = tune(params.nNumRef, 1, params.nPicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2);
    }
    // PreENC works only in encoder order mode
    if (params.bPREENC)
    {
        params.bEncodedOrder = true;
    }
}

#if defined(_WIN32) || defined(_WIN64)
int _tmain(int argc, msdk_char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    sInputParams userParams;    // parameters from command line

    mfxStatus sts = MFX_ERR_NONE;

    try
    {
        sts = ParseInputString(argv, (mfxU8)argc, userParams);
        MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

        CEncodingPipeline pipeline(userParams);

        sts = pipeline.Init();
        MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

        sts = pipeline.Execute();
        MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);
    }
    catch(...)
    {
        msdk_printf(MSDK_STRING("\nUnexpected exception!!\n"));
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return 0;
}
