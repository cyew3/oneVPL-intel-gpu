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

#include "mfx_samples_config.h"

#include "video_conf_pipeline.h"
#include "actions.h"
#include "action_processor.h"
#include "brc.h"
#include <sample_defs.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <typeinfo>


void PrintHelp( const std::basic_string<msdk_char> & strAppName
              , const std::basic_string<msdk_char> & strErrorMessage = MSDK_STRING(""))
{
    if (!strErrorMessage.empty())
    {
        msdk_printf(MSDK_STRING("Error: %s\n\n"), strErrorMessage.c_str());
    }

    msdk_printf(MSDK_STRING("Usage: %s [Options] -i InputYUVFile -o OutputEncodedFile -w width -h height\n"), strAppName.c_str());
    msdk_printf(MSDK_STRING("Options: \n"));
    msdk_printf(MSDK_STRING("   [-i1, -i2 ... frame_num fileName] - after reaching frame_num encoding starts from second input file, encoder will be reset with new corresponding resolution\n"));
    msdk_printf(MSDK_STRING("   [-w1, -w2 ... width] - width to be used for encoding n-th input file\n"));
    msdk_printf(MSDK_STRING("   [-h1, -h2 ... height] - height to be used for encoding n-th file\n"));
    msdk_printf(MSDK_STRING("   [-f, -f1, -f2 ... frameRate] - video frame rate (frames per second) for n-th file\n"));
    msdk_printf(MSDK_STRING("   [-b bitRate] - encoded bit rate (Kbits per second)\n"));
    msdk_printf(MSDK_STRING("   [-hw] - use platform specific SDK implementation (default)\n"));
    msdk_printf(MSDK_STRING("   [-sw] - use software implementation, if not specified platform specific SDK implementation is used\n"));
#if D3D_SURFACES_SUPPORT
    msdk_printf(MSDK_STRING("   [-d3d] - work with d3d surfaces\n"));
#endif
    msdk_printf(MSDK_STRING("   [-par parameters_file] - parameters file may contain same options as a command line\n"));
    msdk_printf(MSDK_STRING("   [-bs frame_num value] - prior encoding of 'frame_num' scales current bitrate by 'value'\n"));
    msdk_printf(MSDK_STRING("   [-bf frame_num broken_frame_num] - prior encoding of 'frame_num' frame, encoder received information that already encoded frame with  number 'broken_frame_num' failed to be decoded\n"));
    msdk_printf(MSDK_STRING("   [-gkf frame_num] - generates key frame at position 'frame_num'\n"));
    msdk_printf(MSDK_STRING("   [-ltr frame_num] - 'frame_num' wil be converted into longterm\n"));
    msdk_printf(MSDK_STRING("   [-ts num_layers] - will create up to 4 temporal layers with scale_base=2 -> 1,2,4,8\n"));
    msdk_printf(MSDK_STRING("   [-brc] - enables external bitrate control based on per-frame QP\n"));
    msdk_printf(MSDK_STRING("   [-l0 frame_num L0_len ] - specifies number of reference frames in L0 array for encoding 'frame_num'\n"));
    msdk_printf(MSDK_STRING("   [-rpmrs] - enables reference picure marking repetion SEI feature\n"));
    msdk_printf(MSDK_STRING("   [-latency] - enables latency calculation and reporting\n"));
    msdk_printf(MSDK_STRING("   [-ir cycle_size qp_delta] - enables intra refresh (supports only refresh by column of MBs). cycle_size is the number of pictures within refresh cycle (should be more than 2 and less than %d), qp_delta specifies QP difference for inserted intra MBs (should be in [-51, 51] range)\n"), VideoConfPipeline::gopLength);
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Example: %s -i InputYUVFile -o OutputEncodedFile -w width -h height -bs 10 0.5\n"), strAppName.c_str());
    msdk_printf(MSDK_STRING("\n"));
}

template<typename T>
    inline void read_argument(const msdk_string& option, const msdk_string& argument, T& value)
    {
        if (MFX_ERR_NONE != msdk_opt_read(argument.c_str(), value)) {
            throw msdk_string(MSDK_STRING("Failed to read argument for the option ") + option);
        }
    }

mfxI32 get_index(const std::basic_string<msdk_char>& from, int prefix_len )
{
    mfxI32 idx = 0;
    if (from.length() != (size_t)prefix_len)
    {
        if (MFX_ERR_NONE != msdk_opt_read(from.substr(prefix_len), idx))
        {
            idx = -1;
        }
    }
    return idx;
}

#define CHECK_OPTION_ARGS(n)\
if (i + n == nArgNum)\
{\
    throw std::basic_string<msdk_char>(MSDK_STRING("Invalid syntax for option:")) + strInput[i];\
}

void ParseParFile(const msdk_char* file_path, VideoConfParams& pParams);
void CheckInitParams(VideoConfParams& pParams);
void ParseInputString(msdk_char** strInput, int nArgNum, VideoConfParams& params)
{
    if (0 == nArgNum)
    {
        throw std::basic_string<msdk_char>(MSDK_STRING(""));
    }

    // default implementation
    params.bUseHWLib = true;

    // parse command line parameters
    for (mfxU8 i = 0; i < nArgNum; i++)
    {
        msdk_char* arg = strInput[i];
        mfxI32 idx = get_index(strInput[i], 2);
        if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-sw")))
        {
            params.bUseHWLib = false;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-hw")))
        {
            params.bUseHWLib = true;
        }
#if D3D_SURFACES_SUPPORT
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d")))
        {
            params.memType = D3D9_MEMORY;
        }
#endif
        else if (0 == msdk_strncmp(strInput[i], MSDK_STRING("-w"), 2) && -1 != idx)
        {
            CHECK_OPTION_ARGS(1);
            read_argument(arg, strInput[++i], params.sources[idx].nWidth);
        }
        else if (0 == msdk_strncmp(strInput[i], MSDK_STRING("-h"), 2) && -1 != idx)
        {
            CHECK_OPTION_ARGS(1);
            read_argument(arg, strInput[++i], params.sources[idx].nHeight);
        }
        else if (0 == msdk_strncmp(strInput[i], MSDK_STRING("-f"), 2) && -1 != idx)
        {
            CHECK_OPTION_ARGS(1);
            read_argument(arg, strInput[++i], params.sources[idx].dFrameRate);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-b")))
        {
            CHECK_OPTION_ARGS(1);
            read_argument(arg, strInput[++i], params.nTargetKbps);
        }
        else if (0 == msdk_strncmp(strInput[i], MSDK_STRING("-i"), 2) && -1 != idx)
        {
            CHECK_OPTION_ARGS(1);
            //we expect frame number attached with -i1, -i2
            if (idx != 0)
            {
                CHECK_OPTION_ARGS(2);
                mfxU32 nFrame = 0;
                read_argument(arg, strInput[++i], nFrame);
                params.pActionProc->RegisterAction(nFrame, new SetSourceAction(idx));
            }
            params.sources[idx].srcFile = strInput[++i];
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-o")))
        {
            CHECK_OPTION_ARGS(1);
            params.dstFile = strInput[++i];
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-par")))
        {
            CHECK_OPTION_ARGS(1);
            ParseParFile(strInput[++i], params);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bs" )))
        {
            CHECK_OPTION_ARGS(2);

            double dBitrateScale = 0;
            mfxU32 nFrameOrder = 0;
            read_argument(arg, strInput[++i], nFrameOrder);
            read_argument(arg, strInput[++i], dBitrateScale);

            params.pActionProc->RegisterAction(nFrameOrder, new ChangeBitrateAction(dBitrateScale));
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-l0")))
        {
            CHECK_OPTION_ARGS(2);

            mfxU16 nLen = 0;
            mfxU32 nFrameOrder = 0;
            read_argument(arg, strInput[++i], nFrameOrder);
            read_argument(arg, strInput[++i], nLen);

            params.pActionProc->RegisterAction(nFrameOrder, new SetL0SizeAction(nLen));
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-brc")))
        {
            params.pBrc.reset(new SampleBRC());
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-rpmrs")))
        {
            params.bRPMRS = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bf")))
        {
            CHECK_OPTION_ARGS(2);

            mfxU32 nFrameOrder = 0;
            mfxU32 nFrameBroken = 0;
            read_argument(arg, strInput[++i], nFrameOrder);
            read_argument(arg, strInput[++i], nFrameBroken);
            if (nFrameBroken >= nFrameOrder)
            {
                std::basic_stringstream<msdk_char> stream;
                stream<<MSDK_STRING("in parsing -bf option. Broken frameorder(current=")<<nFrameBroken<<MSDK_STRING(") ");
                stream<<MSDK_STRING("should be less than reported frameorder(current=")<<nFrameOrder<<MSDK_STRING(")");

                throw stream.str();
            }

            //to instruct encoder to DONOT predict from specific frame it is necessary to add this frame to rejected ref list
            //putting frame into rejected list does make this frame permanently rejected

            //if feedback is delayed frames predicted from broken can be already encoded, need to also remove all of them from references
            for (mfxU32 j = nFrameBroken; j < nFrameOrder; j++)
            {
                params.pActionProc->RegisterAction(nFrameOrder, new PutFrameIntoRefListAction(REFLIST_REJECTED, j, false));
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-gkf")))
        {
            CHECK_OPTION_ARGS(1);

            mfxU32 nFrameOrder = 0;
            read_argument(arg, strInput[++i], nFrameOrder);

            params.pActionProc->RegisterAction(nFrameOrder, new KeyFrameInsertAction());
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-ltr")))
        {
            CHECK_OPTION_ARGS(1);

            mfxU32 nLTFrameOrder = 0;
            read_argument(arg, strInput[++i], nLTFrameOrder);

            //firstly frame should be added to long term list once
            //NOTE: to remove this longterm, you need to put it once into rejected reflist
            params.pActionProc->RegisterAction(nLTFrameOrder, new PutFrameIntoRefListAction(REFLIST_LONGTERM, nLTFrameOrder, false));

            //secondary to say MediaSDK that prediction from this longterm is necessary you need to put frameorder into preferred reflist
            //preferred list is stateless and should be created for every frame, that is why we creating permanent action untill next IDR
            params.pActionProc->RegisterAction(nLTFrameOrder, new PutFrameIntoRefListAction(REFLIST_PREFERRED, nLTFrameOrder, true));
        } else if ( 0== msdk_strcmp(strInput[i], MSDK_STRING("-ts")))
        {
            CHECK_OPTION_ARGS(1);
            params.nTemporalScalabilityBase = 2;
            read_argument(arg, strInput[++i], params.nTemporalScalabilityLayers);
            if (params.nTemporalScalabilityLayers > 4)
            {
                throw std::basic_string<msdk_char>(MSDK_STRING("in -ts option maximum layers value is 4"));
            }
        } else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-latency")))
        {
            params.bCalcLAtency = true;
        } else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-ir")))
        {
            CHECK_OPTION_ARGS(2);
            params.nRefrType = 1;
            read_argument(arg, strInput[++i], params.nCycleSize);
            if (params.nCycleSize < 2 || params.nCycleSize >= VideoConfPipeline::gopLength)
            {
                std::basic_stringstream<msdk_char> stream;
                stream<<MSDK_STRING("in -ir option cycle_size should be more than 2 and less than ")<<VideoConfPipeline::gopLength;
                throw stream.str();
            }
            read_argument(arg, strInput[++i], params.nQPDelta);
            if (params.nQPDelta < -51 || params.nQPDelta > 51)
            {
                throw std::basic_string<msdk_char>(MSDK_STRING("in -ir option QP difference is signed value in [-51, 51] range"));
            }
        }

        else
        {
            throw std::basic_string<msdk_char>(MSDK_STRING("Unknown option : ")) + strInput[i];
        }
    }
}

void CheckInitParams(VideoConfParams& params)
{
    std::map<mfxU32, VideoConfParams::SourceInfo> :: iterator i;
    std::basic_stringstream<msdk_char> error;
    if (params.sources.empty())
    {
        throw std::basic_string<msdk_char>(MSDK_STRING("Source file name not set"));
    }

    for (i = params.sources.begin(); i != params.sources.end(); i++)
    {
        VideoConfParams::SourceInfo &info = i->second;
        // check if all mandatory parameters were set
        if (info.srcFile.empty())
        {
            if (i->first != 0)
            {
                error<< i->first << MSDK_STRING(" ");
            }
            error<< MSDK_STRING("source file name not set");

            throw error.str();
        }

        if (params.dstFile.empty())
        {
            throw std::basic_string<msdk_char>(MSDK_STRING("Destination file name not set"));
        }

        if (0 == info.nWidth)
        {
            if (i->first != 0)
            {
                error<< i->first << MSDK_STRING(" ");
            }
            error<< MSDK_STRING("width must be specidied");

            if (i->first != 0)
            {
                error<< MSDK_STRING("(-w") << i->first << MSDK_STRING(")");
            }
            else
            {
                error<< MSDK_STRING("(-w)");
            }

            throw error.str();
        }

        if (0 == info.nHeight)
        {
            if (i->first!= 0)
            {
                error<< i->first << MSDK_STRING(" ");
            }
            error<< MSDK_STRING("height must be specified");

            if (i->first != 0)
            {
                error<< MSDK_STRING("(-h") << i->first << MSDK_STRING(")");
            }
            else
            {
                error<< MSDK_STRING("(-h)");
            }

            throw error.str();
        }

        if (info.dFrameRate <= 0)
        {
            info.dFrameRate = 30;
        }
    }
    // calculate default bitrate based on the resolution (a parameter for encoder, so Dst resolution is used)
    if (params.nTargetKbps == 0)
    {
        params.nTargetKbps = CalculateDefaultBitrate(MFX_CODEC_AVC, 0, params.sources[0].nWidth, params.sources[0].nHeight, params.sources[0].dFrameRate);
    }
}

void ParseParFile(const msdk_char* file_path, VideoConfParams& pParams)
{
    //reading whole file into string stream
    std::basic_fstream<msdk_char> file_stream(file_path, std::ios_base::in );
    if (file_stream.fail())
        throw std::basic_string<msdk_char>(MSDK_STRING("Failed to open par file : ")) + file_path;

    std::basic_stringstream<msdk_char> str_stream;
    file_stream >> str_stream.rdbuf();

    std::basic_string<msdk_char> str = str_stream.str();

    //removing comments
    for (size_t offset = 0; offset != std::basic_string<msdk_char>::npos;)
    {
        offset = str.find(MSDK_STRING("#"), offset);
        if (offset != std::basic_string<msdk_char>::npos)
        {
            //removing whole line
            size_t offset2 = str.find(MSDK_STRING("\n"), offset);
            size_t count = offset2 == std::basic_string<msdk_char>::npos ? offset2 : offset2 - offset;
            str.erase(offset, count);
            offset++;
        }
    }
    //removing line endings
    std::replace(str.begin(), str.end(), MSDK_CHAR('\n'), MSDK_CHAR(' '));

    //put this 1 line file into stream;
    str_stream.str(str);

    //split it into words by spaces, and forming msdk_char** array
    std::list<std::basic_string<msdk_char> > command_line_strings;
    std::vector<msdk_char*> command_line_args;

    for (;!str_stream.eof();)
    {
        std::basic_string<msdk_char> current_arg;
        if ((str_stream >> current_arg).fail())
            break;
        command_line_strings.push_back(current_arg);
        command_line_args.push_back(&command_line_strings.back().at(0));
    }

    if (!command_line_args.empty())
    {
        ParseInputString((msdk_char**)&command_line_args.front(), (int)command_line_args.size(), pParams);
    }
}

#if defined(_WIN32) || defined(_WIN64)
int _tmain(int argc, TCHAR *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    VideoConfParams init_params;   // input parameters from command line
    std::auto_ptr<IPipeline>  pPipeline;
    mfxStatus sts = MFX_ERR_NONE; // return value check
    std::basic_string<msdk_char> appName (*argv);

    try
    {
        msdk_printf(MSDK_STRING("Video Conference Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);

        init_params.pActionProc.reset(new ActionProcessor);

        ParseInputString(++argv, --argc, init_params);
        CheckInitParams(init_params);

        pPipeline.reset(new VideoConfPipeline());

        sts = pPipeline->Init(&init_params);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

        pPipeline->PrintInfo();

        msdk_printf(MSDK_STRING("Processing started\n"));

        sts = pPipeline->Run();

        if (MFX_ERR_DEVICE_LOST == sts || MFX_ERR_DEVICE_FAILED == sts)
        {
            msdk_printf(MSDK_STRING("\nERROR: Hardware device was lost or returned an unexpected error\n"));
        }

        msdk_printf(MSDK_STRING("\rProcessing finished\n"));

    }
    catch (const std::basic_string<msdk_char> & message)
    {
        //get the application name from path
        size_t file_name_start_pos = appName.find_last_of((MSDK_STRING("/\\")));
        if (std::basic_string<msdk_char>::npos != file_name_start_pos)
        {
            appName.erase(0, file_name_start_pos + 1);
        }
        PrintHelp(appName, message);
    }
    catch (std::exception &ex)
    {
        msdk_cout << "std::exception caught: " << ex.what() << '\n';
    }
    catch (...)
    {
        msdk_printf(MSDK_STRING("\nUnknown exception caught\n"));
    }

    return 0;
}