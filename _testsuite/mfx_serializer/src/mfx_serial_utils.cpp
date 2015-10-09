/******************************************************************************* *\

Copyright (C) 2010-2015 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_serial_utils.cpp

*******************************************************************************/

#pragma warning(disable: 4996)

#include "mfx_serial_utils.h"

typedef struct {
    int code;
    char *string;
} CodeStringTable;

static CodeStringTable StringsOfFourcc[] =
{
    { MFX_FOURCC_NV12,               "NV12"     },
    { MFX_FOURCC_YV12,               "YV12"     },
    { MFX_FOURCC_NV16,               "NV16"     },
    { MFX_FOURCC_YUY2,               "YUY2"     },
    { MFX_FOURCC_RGB3,               "RGB3"     },
    { MFX_FOURCC_RGB4,               "RGB4"     },
    { MFX_FOURCC_P8,                 "P8"       },
    { MFX_FOURCC_P8_TEXTURE,         "P8MB"     },
    { MFX_FOURCC_P010,               "P010"     },
    { MFX_FOURCC_P210,               "P210"     },
    { MFX_FOURCC_BGR4,               "BGR4"     },
    { MFX_FOURCC_A2RGB10,            "A2RGB10"  },
    { MFX_FOURCC_ARGB16,             "ARGB16"   },
    { MFX_FOURCC_R16,                "R16"      },
};

static CodeStringTable StringsOfCodec[] =
{
    { MFX_CODEC_AVC,                 "AVC"      },
    { MFX_CODEC_HEVC,                "HEVC"     },
    { MFX_CODEC_MPEG2,               "MPEG2"    },
    { MFX_CODEC_VC1,                 "VC1"      },
    { MFX_CODEC_CAPTURE,             "CAPT"     },
    { MFX_CODEC_JPEG,                "JPEG"     },
    { MFX_CODEC_VP8,                 "VP8"      },
};

#define DEFINE_CODE(code)\
    code, #code

static CodeStringTable StringsOfStatus[] =
{
    DEFINE_CODE(MFX_ERR_NONE                    ),
    DEFINE_CODE(MFX_ERR_UNKNOWN                 ),
    DEFINE_CODE(MFX_ERR_NULL_PTR                ),
    DEFINE_CODE(MFX_ERR_UNSUPPORTED             ),
    DEFINE_CODE(MFX_ERR_MEMORY_ALLOC            ),
    DEFINE_CODE(MFX_ERR_NOT_ENOUGH_BUFFER       ),
    DEFINE_CODE(MFX_ERR_INVALID_HANDLE          ),
    DEFINE_CODE(MFX_ERR_LOCK_MEMORY             ),
    DEFINE_CODE(MFX_ERR_NOT_INITIALIZED         ),
    DEFINE_CODE(MFX_ERR_NOT_FOUND               ),
    DEFINE_CODE(MFX_ERR_MORE_DATA               ),
    DEFINE_CODE(MFX_ERR_MORE_SURFACE            ),
    DEFINE_CODE(MFX_ERR_ABORTED                 ),
    DEFINE_CODE(MFX_ERR_DEVICE_LOST             ),
    DEFINE_CODE(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM),
    DEFINE_CODE(MFX_ERR_INVALID_VIDEO_PARAM     ),
    DEFINE_CODE(MFX_ERR_UNDEFINED_BEHAVIOR      ),
    DEFINE_CODE(MFX_ERR_DEVICE_FAILED           ),
    DEFINE_CODE(MFX_ERR_MORE_BITSTREAM          ),
    DEFINE_CODE(MFX_ERR_INCOMPATIBLE_AUDIO_PARAM),
    DEFINE_CODE(MFX_ERR_INVALID_AUDIO_PARAM     ),

    DEFINE_CODE(MFX_WRN_IN_EXECUTION            ),
    DEFINE_CODE(MFX_WRN_DEVICE_BUSY             ),
    DEFINE_CODE(MFX_WRN_VIDEO_PARAM_CHANGED     ),
    DEFINE_CODE(MFX_WRN_PARTIAL_ACCELERATION    ),
    DEFINE_CODE(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM),
    DEFINE_CODE(MFX_WRN_VALUE_NOT_CHANGED       ),
    DEFINE_CODE(MFX_WRN_OUT_OF_RANGE            ),
    DEFINE_CODE(MFX_WRN_FILTER_SKIPPED          ),
    DEFINE_CODE(MFX_WRN_INCOMPATIBLE_AUDIO_PARAM),

    DEFINE_CODE(MFX_TASK_DONE                   ),
    DEFINE_CODE(MFX_TASK_WORKING                ),
    DEFINE_CODE(MFX_TASK_BUSY                   ),
};

static CodeStringTable StringsOfImpl[] = {
    DEFINE_CODE(MFX_IMPL_AUTO),
    DEFINE_CODE(MFX_IMPL_SOFTWARE),
    DEFINE_CODE(MFX_IMPL_HARDWARE),
    DEFINE_CODE(MFX_IMPL_AUTO_ANY),
    DEFINE_CODE(MFX_IMPL_HARDWARE_ANY),
    DEFINE_CODE(MFX_IMPL_HARDWARE2),
    DEFINE_CODE(MFX_IMPL_HARDWARE3),
    DEFINE_CODE(MFX_IMPL_HARDWARE4),
    DEFINE_CODE(MFX_IMPL_UNSUPPORTED)
};

static CodeStringTable StringsOfImplVIA[] = {
    DEFINE_CODE(MFX_IMPL_VIA_ANY),
    DEFINE_CODE(MFX_IMPL_VIA_D3D9),
    DEFINE_CODE(MFX_IMPL_VIA_D3D11),
    DEFINE_CODE(MFX_IMPL_VIA_VAAPI),
};

static CodeStringTable StringsOfChromaFormat[] = {
    DEFINE_CODE(MFX_CHROMAFORMAT_MONOCHROME),
    DEFINE_CODE(MFX_CHROMAFORMAT_YUV420),
    DEFINE_CODE(MFX_CHROMAFORMAT_YUV422),
    DEFINE_CODE(MFX_CHROMAFORMAT_YUV444),
    DEFINE_CODE(MFX_CHROMAFORMAT_YUV400),
    DEFINE_CODE(MFX_CHROMAFORMAT_YUV411),
    DEFINE_CODE(MFX_CHROMAFORMAT_YUV422H),
    DEFINE_CODE(MFX_CHROMAFORMAT_YUV422V)
};

static CodeStringTable StringsOfExtendedBuffer[] = {
    DEFINE_CODE(MFX_EXTBUFF_CODING_OPTION            ),
    DEFINE_CODE(MFX_EXTBUFF_CODING_OPTION_SPSPPS     ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_DONOTUSE             ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_AUXDATA              ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_DENOISE              ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_SCENE_ANALYSIS       ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_SCENE_CHANGE         ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_PROCAMP              ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_DETAIL               ),
    DEFINE_CODE(MFX_EXTBUFF_VIDEO_SIGNAL_INFO        ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_DOUSE                ),
    DEFINE_CODE(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION),
    DEFINE_CODE(MFX_EXTBUFF_AVC_REFLIST_CTRL         ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION),
    DEFINE_CODE(MFX_EXTBUFF_PICTURE_TIMING_SEI       ),
    DEFINE_CODE(MFX_EXTBUFF_AVC_TEMPORAL_LAYERS      ),
    DEFINE_CODE(MFX_EXTBUFF_CODING_OPTION2           ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_IMAGE_STABILIZATION  ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION  ),
    DEFINE_CODE(MFX_EXTBUFF_ENCODER_CAPABILITY       ),
    DEFINE_CODE(MFX_EXTBUFF_ENCODER_RESET_OPTION     ),
    DEFINE_CODE(MFX_EXTBUFF_ENCODED_FRAME_INFO       ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_COMPOSITE            ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO    ),
    DEFINE_CODE(MFX_EXTBUFF_ENCODER_ROI              ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_DEINTERLACING        ),
    DEFINE_CODE(MFX_EXTBUFF_AVC_REFLISTS             ),
    DEFINE_CODE(MFX_EXTBUFF_DEC_VIDEO_PROCESSING     ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_FIELD_PROCESSING     ),
    DEFINE_CODE(MFX_EXTBUFF_CODING_OPTION3           ),
    DEFINE_CODE(MFX_EXTBUFF_CHROMA_LOC_INFO          ),
    DEFINE_CODE(MFX_EXTBUFF_MBQP                     ),
    DEFINE_CODE(MFX_EXTBUFF_HEVC_TILES               ),
    DEFINE_CODE(MFX_EXTBUFF_MB_DISABLE_SKIP_MAP      ),
    DEFINE_CODE(MFX_EXTBUFF_DPB                      ),
    DEFINE_CODE(MFX_EXTBUFF_HEVC_PARAM               ),
    DEFINE_CODE(MFX_EXTBUFF_DECODED_FRAME_INFO       ),
    DEFINE_CODE(MFX_EXTBUFF_TIME_CODE                ),
    DEFINE_CODE(MFX_EXTBUFF_HEVC_REGION              ),
    DEFINE_CODE(MFX_EXTBUFF_PRED_WEIGHT_TABLE        ),
    DEFINE_CODE(MFX_EXTBUFF_TEMPORAL_LAYERS          ),
    DEFINE_CODE(MFX_EXTBUFF_DIRTY_RECTANGLES         ),
    DEFINE_CODE(MFX_EXTBUFF_MOVING_RECTANGLES        ),
    DEFINE_CODE(MFX_EXTBUFF_AVC_SCALING_MATRIX       ),
    DEFINE_CODE(MFX_EXTBUFF_MPEG2_QUANT_MATRIX       ),
    DEFINE_CODE(MFX_EXTBUFF_CODING_OPTION_VPS        ),
    DEFINE_CODE(MFX_EXTBUFF_VPP_ROTATION             ),
    DEFINE_CODE(MFX_EXTBUFF_ENCODED_SLICES_INFO      ),

    DEFINE_CODE(MFX_EXTBUFF_VP8_CODING_OPTION        ),
    DEFINE_CODE(MFX_EXTBUFF_MVC_SEQ_DESC             ),
    DEFINE_CODE(MFX_EXTBUFF_MVC_TARGET_VIEWS         )
};

static CodeStringTable StringsOfIOPattern[] = {
    DEFINE_CODE(MFX_IOPATTERN_IN_VIDEO_MEMORY  ),
    DEFINE_CODE(MFX_IOPATTERN_IN_SYSTEM_MEMORY ),
    DEFINE_CODE(MFX_IOPATTERN_IN_OPAQUE_MEMORY ),
    DEFINE_CODE(MFX_IOPATTERN_OUT_VIDEO_MEMORY ),
    DEFINE_CODE(MFX_IOPATTERN_OUT_SYSTEM_MEMORY),
    DEFINE_CODE(MFX_IOPATTERN_OUT_OPAQUE_MEMORY),
};

#define DEFINE_CODE2(code, subcode)\
    code##subcode, #subcode

static CodeStringTable StringsOfPS[] = {
    DEFINE_CODE2(MFX_PICSTRUCT_,UNKNOWN),
    DEFINE_CODE2(MFX_PICSTRUCT_,PROGRESSIVE),
    DEFINE_CODE2(MFX_PICSTRUCT_FIELD_,TFF),
    DEFINE_CODE2(MFX_PICSTRUCT_FIELD_,BFF),
    DEFINE_CODE2(MFX_PICSTRUCT_FIELD_,REPEATED),
    DEFINE_CODE2(MFX_PICSTRUCT_FRAME_,DOUBLING),
    DEFINE_CODE2(MFX_PICSTRUCT_FRAME_,TRIPLING),
};

static CodeStringTable StringsOfFrameType[] = {
    DEFINE_CODE2(MFX_FRAMETYPE_,UNKNOWN),

    DEFINE_CODE2(MFX_FRAMETYPE_,I),
    DEFINE_CODE2(MFX_FRAMETYPE_,P),
    DEFINE_CODE2(MFX_FRAMETYPE_,B),
    DEFINE_CODE2(MFX_FRAMETYPE_,S),

    DEFINE_CODE2(MFX_FRAMETYPE_,REF),
    DEFINE_CODE2(MFX_FRAMETYPE_,IDR),

    DEFINE_CODE2(MFX_FRAMETYPE_,xI),
    DEFINE_CODE2(MFX_FRAMETYPE_,xP),
    DEFINE_CODE2(MFX_FRAMETYPE_,xB),
    DEFINE_CODE2(MFX_FRAMETYPE_,xS),

    DEFINE_CODE2(MFX_FRAMETYPE_,xREF),
    DEFINE_CODE2(MFX_FRAMETYPE_,xIDR),
};

#define MFX_CODE_TO_STRING(table, code) \
    mfxCodeToStringEqual(table, sizeof(table)/sizeof(CodeStringTable), code)

#define STRING_TO_MFX_CODE(table, string) \
    StringTomfxCodeEqual(table, sizeof(table)/sizeof(CodeStringTable), string)

//print values that cann hvae multiple flags
#define MFX_CODE_TO_STRING_ALL(table, code) \
    mfxCodeToStringAll(table, sizeof(table)/sizeof(CodeStringTable), code)

#define STRING_ALL_TO_MFX_CODE(table, string) \
    StringAllTomfxCode(table, sizeof(table)/sizeof(CodeStringTable), string)

std::string mfxCodeToStringEqual(CodeStringTable *table, int table_size, int code)
{
    int i;
    for (i = 0; i < table_size; i++) {
        if (table[i].code == code) {
            return table[i].string;
        }
    }
    std::stringstream stream;
    stream << "UNDEF(" << code << ")";
    return stream.str();
}

int StringTomfxCodeEqual(CodeStringTable *table, int table_size, std::string str)
{
    int i;
    for (i = 0; i < table_size; i++) {
        if (table[i].string == str) {
            return table[i].code;
        }
    }
    return MFX_ERR_UNSUPPORTED;
}

std::string mfxCodeToStringAll(CodeStringTable *table, int table_size, int code)
{
    std::stringstream stream;
    bool bEmpty = true;

    int i;
    for (i = 0; i < table_size; i++) {
        if (0 != table[i].code && 0 != (table[i].code & code)  ) {
            if (!bEmpty)
                stream << "|";

            stream << table[i].string;
            bEmpty = false;
        }
    }
    if (bEmpty)
        stream << "UNDEF(" << code << ")";

    return stream.str();
}

int StringAllTomfxCode(CodeStringTable *table, int table_size, std::string str)
{
    std::vector<std::string> values;
    mfxU32 start = 0, end = 0;
    while ((end = str.find("|", start)) != std::string::npos)
    {
        values.push_back(str.substr(start, end - start));
        start = end + 1;
    }
    values.push_back(str.substr(start));

    int code = 0;
    bool bEmpty = true;

    int i;
    for (i = 0; i < table_size; i++) {
        if (std::find(values.begin(), values.end(), table[i].string) != values.end() ) {
            code |= table[i].code;
        }
    }

    return code;
}

std::string GetMFXFourCCString(mfxU32 mfxFourcc)
{
    return MFX_CODE_TO_STRING(StringsOfFourcc, mfxFourcc);
}

mfxU32 GetMFXFourCCCode(std::string mfxFourcc)
{
    return STRING_TO_MFX_CODE(StringsOfFourcc, mfxFourcc);
}

std::string GetMFXCodecIdString(mfxU32 mfxCodec)
{
    return MFX_CODE_TO_STRING(StringsOfCodec, mfxCodec);
}

mfxU32 GetMFXCodecIdCode(std::string mfxCodec)
{
    return STRING_TO_MFX_CODE(StringsOfCodec, mfxCodec);
}

std::string GetMFXChromaFormatString(mfxU32 chromaFormat)
{
    return MFX_CODE_TO_STRING(StringsOfChromaFormat, chromaFormat);
}

mfxU32 GetMFXChromaFormatCode(std::string chromaFormat)
{
    return STRING_TO_MFX_CODE(StringsOfChromaFormat, chromaFormat);
}

std::string GetMFXStatusString(mfxStatus mfxSts)
{
    return MFX_CODE_TO_STRING(StringsOfStatus, mfxSts);
}

mfxU32 GetMFXStatusCode(std::string mfxSts)
{
    return STRING_TO_MFX_CODE(StringsOfStatus, mfxSts);
}

std::string GetMFXPicStructString(int PicStruct)
{
    return MFX_CODE_TO_STRING_ALL(StringsOfPS, PicStruct);
}

int GetMFXPicStructCode(std::string PicStruct)
{
    return STRING_ALL_TO_MFX_CODE(StringsOfPS, PicStruct);
}

std::string GetMFXFrameTypeString (int FrameType)
{
    return MFX_CODE_TO_STRING_ALL(StringsOfFrameType, FrameType);
}

int GetMFXFrameTypeCode(std::string FrameType)
{
    return STRING_ALL_TO_MFX_CODE(StringsOfFrameType, FrameType);
}

std::string GetMFXBufferIdString(mfxU32 mfxExtBufId)
{
    return MFX_CODE_TO_STRING(StringsOfExtendedBuffer, mfxExtBufId);
}

mfxU32 GetMFXBufferIdCode(std::string mfxExtBufId)
{
    return STRING_TO_MFX_CODE(StringsOfExtendedBuffer, mfxExtBufId);
}

std::string GetMFXIOPatternString (int IOPattern)
{
    return MFX_CODE_TO_STRING_ALL(StringsOfIOPattern, IOPattern);
}

int GetMFXIOPatternCode(std::string IOPattern)
{
    return STRING_ALL_TO_MFX_CODE(StringsOfIOPattern, IOPattern);
}

#define MAKE_HEX( x ) \
    std::setw(2) << std::setfill('0') << std::hex << (int)( x )

std::string GetMFXRawDataString(mfxU8* pData, mfxU32 nData)
{
    std::stringstream stream;
    stream << "[" << nData << "] = {";
    for (size_t j = 0; j < nData; j++)
    {
        stream << MAKE_HEX(pData[j]);

        if (j + 1 < nData)
            stream<< " ";
    }
    stream << "}";
    return stream.str();
}

void GetMFXRawDataValues(mfxU8* pData, std::string Data)
{
    std::stringstream ValNum(Data.substr(Data.find('[') + 1, Data.find(']') - Data.find('[') - 1)),
                      Values(Data.substr(Data.find('{') + 1, Data.find('}') - Data.find('{') - 1));

    mfxU32 num = 0, val;

    ValNum >> num;

    for (mfxU32 i = 0; i < num; i++)
    {
        Values >> std::hex >> val;
        pData[i] = (mfxU8)val;
    }
}

std::string GetMFXImplString(mfxIMPL impl)
{
    std::string str1 = MFX_CODE_TO_STRING(StringsOfImpl, impl & ~(-MFX_IMPL_VIA_ANY));
    std::string str2 = MFX_CODE_TO_STRING(StringsOfImplVIA, impl & (-MFX_IMPL_VIA_ANY));

    return str1 + (str2 == "UNDEF(0)" ? "" : "|" + str2);
}

mfxIMPL GetMFXImplCode(std::string impl)
{
    mfxU32 start = 0, end = 0;
    std::vector<std::string> values;
    while ((end = impl.find("|", start)) != std::string::npos)
    {
        values.push_back(impl.substr(start, end - start));
        start = end + 1;
    }
    values.push_back(impl.substr(start));

    mfxIMPL impl1 = STRING_TO_MFX_CODE(StringsOfImpl, values[0]);
    mfxIMPL impl2 = (values.size() == 2) ? STRING_TO_MFX_CODE(StringsOfImplVIA, values[1]) : MFX_ERR_UNSUPPORTED;

    impl1 = (impl1 != MFX_ERR_UNSUPPORTED) ? impl1 : 0;
    impl2 = (impl2 != MFX_ERR_UNSUPPORTED) ? impl2 : 0;

    return (impl1 | impl2);
}

mfxU32 MFX_TIME_STAMP_FREQUENCY = 90000;
mfxU64 MFX_TIME_STAMP_INVALID   = (mfxU64)-1;

mfxU64 ConvertmfxF642MFXTime(mfxF64 fTime)
{
    mfxU64 nTime = 0;

    if (-1 == fTime)
    {
        nTime = MFX_TIME_STAMP_INVALID;
    }
    else
    {
        nTime = (mfxU64)(fTime * MFX_TIME_STAMP_FREQUENCY);
    }

    return nTime;
}

mfxF64 ConvertMFXTime2mfxF64(mfxU64 nTime)
{
    if (MFX_TIME_STAMP_INVALID == nTime)
    {
        return -1.0;
    }

    return (mfxF64)nTime / (mfxF64)MFX_TIME_STAMP_FREQUENCY;
}

bool SaveSerializedStructure(std::string file_name, std::string input)
{
    FILE* pFILE = { 0 };

    pFILE = fopen(file_name.c_str(), "a");
    if (!pFILE) return false;

    fprintf(pFILE, input.c_str(), input.size());
    fclose(pFILE);

    return true;
}

std::string UploadSerialiedStructures(std::string file_name)
{
    FILE* pFILE = { 0 };
    mfxU32 fsize = 0;

    pFILE = fopen(file_name.c_str(), "r");
    if (!pFILE) return std::string();

    std::string output;

    fseek(pFILE, 0, SEEK_END);
    fsize = ftell(pFILE);
    fseek(pFILE, 0, SEEK_SET);
    output.reserve(fsize);
    fread((void*)output.c_str(), sizeof(char), fsize, pFILE);
    fclose(pFILE);

    return output;
}