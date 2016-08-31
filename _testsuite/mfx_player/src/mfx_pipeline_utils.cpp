/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_pipeline_utils.h"
#include "mfx_dispatcher.h"
#include "mfxjpeg.h"
#include "vm_interlocked.h"
#include "vm_file.h"
#include "shared_utils.h"
#include <iomanip>
#include <limits>

#if defined(_WIN32) || defined(_WIN64)
    #include <DXGI.h>
    #include <psapi.h>
#endif

#include "mfxvp8.h"

typedef struct {
    int code;
    const vm_char *string;
} CodeStringTable;


static CodeStringTable StringsOfFourcc[] =
{
    { MFX_CODEC_AVC,                 VM_STRING("AVC")  },
    { MFX_CODEC_MPEG2,               VM_STRING("MPEG2")},
    { MFX_CODEC_VC1,                 VM_STRING("VC1")  },
    { MFX_CODEC_JPEG,                VM_STRING("JPEG") },
    { MFX_FOURCC_NV12,               VM_STRING("NV12") },
    { MFX_FOURCC_YUY2,               VM_STRING("YUY2") },
    { MFX_FOURCC_YV12,               VM_STRING("YV12") },
    { MFX_FOURCC_RGB3,               VM_STRING("RGB3") },
    { MFX_FOURCC_RGB4,               VM_STRING("RGB4") },
    { MFX_FOURCC_P8,                 VM_STRING("P8")   },
    { MFX_CODEC_HEVC,                VM_STRING("HEVC") },
    { MFX_FOURCC_P010,               VM_STRING("P010") },
    { MFX_FOURCC_A2RGB10,            VM_STRING("A2RGB10") },
    { MFX_CODEC_VP8,                 VM_STRING("VP8")  },
    { MFX_FOURCC_R16,                VM_STRING("R16")  },
    { MFX_FOURCC_ARGB16,             VM_STRING("ARGB16")  },
    { MFX_FOURCC_NV16,               VM_STRING("NV16")  },
    { MFX_FOURCC_P210,               VM_STRING("P210")  },
    { MFX_FOURCC_Y210,               VM_STRING("Y210")  },
    { MFX_FOURCC_Y410,               VM_STRING("Y410")  },
    { MFX_FOURCC_AYUV,               VM_STRING("AYUV")  },
};

#define DEFINE_ERR_CODE(code)\
    code, VM_STRING(#code)

static CodeStringTable StringsOfStatus[] =
{
    DEFINE_ERR_CODE(MFX_ERR_NONE                    ),
    DEFINE_ERR_CODE(MFX_ERR_UNKNOWN                 ),
    DEFINE_ERR_CODE(MFX_ERR_NULL_PTR                ),
    DEFINE_ERR_CODE(MFX_ERR_UNSUPPORTED             ),
    DEFINE_ERR_CODE(MFX_ERR_MEMORY_ALLOC            ),
    DEFINE_ERR_CODE(MFX_ERR_NOT_ENOUGH_BUFFER       ),
    DEFINE_ERR_CODE(MFX_ERR_INVALID_HANDLE          ),
    DEFINE_ERR_CODE(MFX_ERR_LOCK_MEMORY             ),
    DEFINE_ERR_CODE(MFX_ERR_NOT_INITIALIZED         ),
    DEFINE_ERR_CODE(MFX_ERR_NOT_FOUND               ),
    DEFINE_ERR_CODE(MFX_ERR_MORE_DATA               ),
    DEFINE_ERR_CODE(MFX_ERR_MORE_SURFACE            ),
    DEFINE_ERR_CODE(MFX_ERR_ABORTED                 ),
    DEFINE_ERR_CODE(MFX_ERR_DEVICE_LOST             ),
    DEFINE_ERR_CODE(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM),
    DEFINE_ERR_CODE(MFX_ERR_INVALID_VIDEO_PARAM     ),
    DEFINE_ERR_CODE(MFX_ERR_UNDEFINED_BEHAVIOR      ),
    DEFINE_ERR_CODE(MFX_ERR_DEVICE_FAILED           ),
    DEFINE_ERR_CODE(MFX_ERR_MORE_BITSTREAM          ),
    DEFINE_ERR_CODE(MFX_ERR_GPU_HANG                ),
    DEFINE_ERR_CODE(MFX_ERR_REALLOC_SURFACE         ),

    DEFINE_ERR_CODE(MFX_WRN_IN_EXECUTION            ),
    DEFINE_ERR_CODE(MFX_WRN_DEVICE_BUSY             ),
    DEFINE_ERR_CODE(MFX_WRN_VIDEO_PARAM_CHANGED     ),
    DEFINE_ERR_CODE(MFX_WRN_PARTIAL_ACCELERATION    ),
    DEFINE_ERR_CODE(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM),
    DEFINE_ERR_CODE(MFX_WRN_VALUE_NOT_CHANGED       ),
    DEFINE_ERR_CODE(MFX_WRN_OUT_OF_RANGE            ),
    DEFINE_ERR_CODE(MFX_WRN_FILTER_SKIPPED          ),


    DEFINE_ERR_CODE(PIPELINE_ERR_STOPPED            ),
    DEFINE_ERR_CODE(PIPELINE_ERR_FILES_SIZES_MISMATCH),
    DEFINE_ERR_CODE(PIPELINE_ERR_THRESHOLD          ),
    DEFINE_ERR_CODE(PIPELINE_ERROR_FILE_READ        ),
    DEFINE_ERR_CODE(PIPELINE_ERROR_RESET_FAILED     ),
};

static CodeStringTable StringsOfImpl[] = {
    DEFINE_ERR_CODE(MFX_IMPL_AUTO),
    DEFINE_ERR_CODE(MFX_IMPL_SOFTWARE),
    DEFINE_ERR_CODE(MFX_IMPL_HARDWARE),
    DEFINE_ERR_CODE(MFX_IMPL_AUTO_ANY),
    DEFINE_ERR_CODE(MFX_IMPL_HARDWARE_ANY),
    DEFINE_ERR_CODE(MFX_IMPL_HARDWARE2),
    DEFINE_ERR_CODE(MFX_IMPL_HARDWARE3),
    DEFINE_ERR_CODE(MFX_IMPL_HARDWARE4),
    DEFINE_ERR_CODE(MFX_IMPL_UNSUPPORTED)
};

static CodeStringTable StringsOfImplVIA[] = {
    DEFINE_ERR_CODE(MFX_IMPL_VIA_ANY),
    DEFINE_ERR_CODE(MFX_IMPL_VIA_D3D9),
    DEFINE_ERR_CODE(MFX_IMPL_VIA_D3D11),
    DEFINE_ERR_CODE(MFX_IMPL_VIA_VAAPI),
};

static CodeStringTable StringsOfChromaFormat[] = {
    DEFINE_ERR_CODE(MFX_CHROMAFORMAT_MONOCHROME),
    DEFINE_ERR_CODE(MFX_CHROMAFORMAT_YUV420),
    DEFINE_ERR_CODE(MFX_CHROMAFORMAT_YUV422),
    DEFINE_ERR_CODE(MFX_CHROMAFORMAT_YUV444),
    DEFINE_ERR_CODE(MFX_CHROMAFORMAT_YUV400),
    DEFINE_ERR_CODE(MFX_CHROMAFORMAT_YUV411),
    DEFINE_ERR_CODE(MFX_CHROMAFORMAT_YUV422H),
    DEFINE_ERR_CODE(MFX_CHROMAFORMAT_YUV422V)
};

#define DEFINE_ERR_CODE2(code, subcode)\
    code##subcode, VM_STRING(#subcode)

static CodeStringTable StringsOfPS[] = {
    DEFINE_ERR_CODE2(MFX_PICSTRUCT_,UNKNOWN),
    DEFINE_ERR_CODE2(MFX_PICSTRUCT_,PROGRESSIVE),
    DEFINE_ERR_CODE2(MFX_PICSTRUCT_FIELD_,TFF),
    DEFINE_ERR_CODE2(MFX_PICSTRUCT_FIELD_,BFF),
    DEFINE_ERR_CODE2(MFX_PICSTRUCT_FIELD_,REPEATED),
    DEFINE_ERR_CODE2(MFX_PICSTRUCT_FRAME_,DOUBLING),
    DEFINE_ERR_CODE2(MFX_PICSTRUCT_FRAME_,TRIPLING),
};

static CodeStringTable StringsOfFrameType[] = {
    DEFINE_ERR_CODE2(MFX_FRAMETYPE_,UNKNOWN),

    DEFINE_ERR_CODE2(MFX_FRAMETYPE_,I),
    DEFINE_ERR_CODE2(MFX_FRAMETYPE_,P),
    DEFINE_ERR_CODE2(MFX_FRAMETYPE_,B),
    DEFINE_ERR_CODE2(MFX_FRAMETYPE_,S),

    DEFINE_ERR_CODE2(MFX_FRAMETYPE_,REF),
    DEFINE_ERR_CODE2(MFX_FRAMETYPE_,IDR),

    DEFINE_ERR_CODE2(MFX_FRAMETYPE_,xI),
    DEFINE_ERR_CODE2(MFX_FRAMETYPE_,xP),
    DEFINE_ERR_CODE2(MFX_FRAMETYPE_,xB),
    DEFINE_ERR_CODE2(MFX_FRAMETYPE_,xS),

    DEFINE_ERR_CODE2(MFX_FRAMETYPE_,xREF),
    DEFINE_ERR_CODE2(MFX_FRAMETYPE_,xIDR),
};



const struct
{
    // instance''s implementation type
    eMfxImplType implType;
    // real implementation
    mfxIMPL impl;
    // adapter's numbers
    mfxU32 adapterID;

} myimplTypes[] =
{
    // MFX_IMPL_AUTO case
    {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE, 0},
    {MFX_LIB_SOFTWARE, MFX_IMPL_SOFTWARE, 0},

    // MFX_IMPL_ANY case
    {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE, 0},
    {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE2, 1},
    {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE3, 2},
    {MFX_LIB_HARDWARE, MFX_IMPL_HARDWARE4, 3},
    {MFX_LIB_SOFTWARE, MFX_IMPL_SOFTWARE, 0}
};

const struct
{
    // start index in implTypes table for specified implementation
    mfxU32 minIndex;
    // last index in implTypes table for specified implementation
    mfxU32 maxIndex;

} myimplTypesRange[] =
{
    // MFX_IMPL_AUTO
    {0, 1},
    // MFX_IMPL_SOFTWARE
    {1, 1},
    // MFX_IMPL_HARDWARE
    {0, 0},
    // MFX_IMPL_AUTO_ANY
    {2, 6},
    // MFX_IMPL_HARDWARE_ANY
    {2, 5},
    // MFX_IMPL_HARDWARE2
    {3, 3},
    // MFX_IMPL_HARDWARE3
    {4, 4},
    // MFX_IMPL_HARDWARE4
    {5, 5}
};

std::map<std::string, std::basic_string<vm_char> > __stored_exprs ;

static mfxU32 g_PipelineErr = 0;
static mfxU32 g_ErrSink     = PIPELINE_ERR_TRACE;
static mfxU32 g_InfoSink    = PIPELINE_INFO_TRACE;
static mfxU32 g_WrnSink     = PIPELINE_WRN_TRACE;
static mfxU32 g_bTraceFrames= true;
static mfxU32 g_processedFrames = 0;
static vm_char  g_AlignStr[] = VM_STRING("%-30s");

static const vm_char * months[] =
{
    VM_STRING("Jan"), VM_STRING("Feb"), VM_STRING("Mar"),
    VM_STRING("Apr"), VM_STRING("May"), VM_STRING("Jun"),
    VM_STRING("Jul"), VM_STRING("Aug"), VM_STRING("Sep"),
    VM_STRING("Oct"), VM_STRING("Nov"), VM_STRING("Dec")
};

void PipelineBuildTime( const vm_char *&platformName
                      , int &year
                      , int &month
                      , int &day
                      , int &hour
                      , int &minute
                      , int &sec)
{
#if defined (WIN64)
    static const vm_char platform []= VM_STRING("win64");
#elif defined (WIN32)
    static const vm_char platform []= VM_STRING("win32");
#elif defined (LINUX64)
    static const vm_char platform []= VM_STRING("linux64");
#elif defined (LINUX32)
    static const vm_char platform []= VM_STRING("linux32");
#else
    static const vm_char platform []= VM_STRING("undef");
#endif

    platformName = platform;

    vm_char temp [] = VM_STRING(__DATE__);
    vm_char temp_time [] = VM_STRING(__TIME__);

    unsigned char i;

    year = vm_string_atoi(temp + 7);
    *(temp + 6) = 0;
    day = vm_string_atoi(temp + 4);
    *(temp + 3) = 0;
    for (i = 0; i < 12; i++)
    {
        if (!vm_string_strcmp(temp, months[i]))
        {
            month = i + 1;
            break;
        }
    }
    temp_time[2] = temp_time[5] = VM_STRING('\0');

    hour   = vm_string_atoi(temp_time);
    minute = vm_string_atoi(temp_time + 3);
    sec    = vm_string_atoi(temp_time + 6);
}

#ifdef LOG_TIME
mfxF64 MFXGetTime(mfxU64 start_time, mfxU64 end_time)
{
    static mfxF64 fFreq = 0.0;
    LARGE_INTEGER lFreq;
    if (fFreq == 0.0)
    {
        QueryPerformanceFrequency(&lFreq);
        fFreq = (mfxF64)lFreq.QuadPart;
    }
    return ((mfxF64)(end_time - start_time))/fFreq;
}
#endif

void PipelineSetLastErr( mfxU32  err
                       , const vm_char * check
                       , const vm_char * pFile
                       , int     line
                       , bool    bwrn)
{
    vm_char new_line[]=VM_STRING("\n\n------------------------------------ ERROR SECTION -----------------------------------\n\n");
    vm_char empty_str[]=VM_STRING("");
    vm_char *pNewLine = bwrn || PipelineGetLastErr()|| !err ? empty_str : new_line;

    g_PipelineErr = err;
    vm_char err_name[][10] = {VM_STRING("WARNING:"), VM_STRING("ERROR:")};
    vm_char *pErr = bwrn ? err_name[0] : err_name[1];
    mfxU32 &targetSink = bwrn ? g_WrnSink : g_ErrSink;

    if (pFile != NULL && line !=0 && check != NULL)
    {
        if (TRACE_CONSOLE == (targetSink & TRACE_CONSOLE))
        {
                vm_string_printf(VM_STRING("%s%s %s at %s :%d\n"), pNewLine, pErr, check, pFile, line);
        }
#if defined(_WIN32) || defined(_WIN64)
        if (TRACE_DEBUG == (targetSink & TRACE_DEBUG))
        {
            vm_char buf[2048];
            vm_string_sprintf_s(buf, sizeof(buf)/sizeof(buf[0]), VM_STRING("%s (%d) : %s :  %s\n"), pFile, line, pErr, check);
            OutputDebugString(buf);
        }
#endif // #if defined(_WIN32) || defined(_WIN64)
    }
    else
    {
        if (TRACE_CONSOLE == (targetSink & TRACE_CONSOLE))
        {
            vm_string_printf(VM_STRING("%s"), pNewLine);
        }
    }
}

void           PipelineSetLastMFXErr( mfxU32 err
                                    , mfxStatus sts
                                    , const vm_char * pAction
                                    , const vm_char * pFile
                                    , int     line)
{
    vm_char str_err[1024];
    vm_string_sprintf_s(str_err, VM_STRING("MFX_ERR_NONE != %s = %s"), GetMFXStatusString(sts).c_str(), pAction);
    PipelineSetLastErr(err, str_err, pFile, line);
}

void  PipelineTraceMFXWrn( mfxStatus sts
                         , const vm_char * pAction
                         , const vm_char * pFile
                         , int     line)
{
    vm_char str_err[1024];
    vm_string_sprintf_s(str_err, VM_STRING("%s = %s"), GetMFXStatusString(sts).c_str(), pAction);
    PipelineSetLastErr(PipelineGetLastErr(), str_err, pFile, line, true);
}

mfxU32 PipelineGetLastErr()
{
    return g_PipelineErr;
}

mfxU32 PipelineSetTrace(mfxU32 nProvider, mfxU32 sink)
{
    mfxU32  nPrevSink = TRACE_NONE;

    struct
    {
        mfxU32  type;
        mfxU32 *sink;
    } all_providers []= { {PROVIDER_ERROR, &g_ErrSink},
                          {PROVIDER_WRN,   &g_WrnSink},
                          {PROVIDER_INFO,  &g_InfoSink} };

    for (unsigned int i =0; i < MFX_ARRAY_SIZE(all_providers); i++)
    {
        if ((nProvider & all_providers[i].type) == all_providers[i].type)
        {
            nPrevSink = *all_providers[i].sink;
            *all_providers[i].sink = sink;
        }
    }

    return nPrevSink;
}

void  PipelineTraceEx2(int mode, const vm_char *pStr, ...)
{
    va_list argptr;
    va_start(argptr, pStr);

    vPipelineTraceEx2(mode, pStr, argptr);
}

void  vPipelineTraceEx2(int mode, const vm_char *pStr, va_list argptr)
{
    if (TRACE_CONSOLE == (mode & TRACE_CONSOLE))
    {
        vm_string_vprintf(pStr, argptr);
    }

    if (TRACE_ERR_CONSOLE == (mode & TRACE_ERR_CONSOLE))
    {
        vm_string_vfprintf(vm_stderr, (vm_char*)pStr, argptr);
    }
#if defined(_WIN32) || defined(_WIN64)
    if (TRACE_DEBUG == (mode & TRACE_DEBUG))
    {
        vm_char str[1024];
        _vstprintf_p(str, sizeof(str)/sizeof(str[0]), pStr, argptr);
        OutputDebugString(str);
    }
#endif
}

void PipelineTraceEx(const vm_char * pStr, ...)
{
    va_list argptr;
    va_start(argptr, pStr);

    vPipelineTraceEx2(g_InfoSink, pStr, argptr);
}

mfxU32 PipelineGetForceSink(mfxU32 original_sink)
{
    if (TRACE_NONE == original_sink)
        return TRACE_NONE;

#if defined(_WIN32) || defined(_WIN64)
    DWORD nMode;

    if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &nMode))
    {
        //output console attached additional output not required
        return original_sink;
    }
    else
#endif
    {
        //also traces to error console
        return  original_sink | TRACE_ERR_CONSOLE;
    }
}

void PipelineTraceForceEx(const vm_char *pStr, ...)
{
    va_list argptr;
    va_start(argptr, pStr);

    vPipelineTraceEx2(PipelineGetForceSink(g_InfoSink), pStr, argptr);
}

void PipelineSetFrameTrace(bool bOn)
{
    g_bTraceFrames = bOn;
}

static void  ProcessFrame(int nFrame)
{
    if (g_InfoSink == TRACE_NONE && !(nFrame % 100))
    {
        vm_string_printf(VM_STRING("."));
    }
}


void PipelineTraceEncFrame(mfxU16 FrameType)
{
    if (g_bTraceFrames)
    {
        vm_char ch;
        switch (FrameType & 0xf)
        {
            case MFX_FRAMETYPE_I: ch = VM_STRING('I'); break;
            case MFX_FRAMETYPE_P: ch = VM_STRING('P'); break;
            case MFX_FRAMETYPE_B: ch = VM_STRING('B'); break;
            default:
                ch = VM_STRING('-');
                break;
        }

        PipelineTrace((VM_STRING("%c"), ch));
       // ProcessFrame(g_processedFrames++);
    }
}

void PipelineTraceDecFrame( int nDecode)
{
    if (g_bTraceFrames)
    {
        if (!nDecode)
        {
            PipelineTrace((VM_STRING("d")));
        }
        else
        {
            PipelineTrace((VM_STRING("(d)")));
        }

        ProcessFrame(g_processedFrames++);
    }
}

void PipelineTraceSplFrame( void )
{
    if (g_bTraceFrames)
    {
        PipelineTrace((VM_STRING("s")));
        //ProcessFrame(g_processedFrames++);
    }
}

#define MFX_CODE_TO_STRING(table, code) \
    mfxCodeToStringEqual(table, sizeof(table)/sizeof(CodeStringTable), code)

//print values that cann hvae multiple flags
#define MFX_CODE_TO_STRING_ALL(table, code) \
    mfxCodeToStringAll(table, sizeof(table)/sizeof(CodeStringTable), code)

tstring mfxCodeToStringEqual(CodeStringTable *table,
                       int table_size,
                       int code)
{
    int i;
    for (i = 0; i < table_size; i++) {
        if (table[i].code == code) {
            return table[i].string;
        }
    }
    tstringstream stream;
    stream <<VM_STRING("UNDEF(") << code <<VM_STRING(")");;
    return stream.str();
}

tstring mfxCodeToStringAll(CodeStringTable *table,
                           int table_size,
                           int code)
{
    tstringstream stream;
    bool bEmpty = true;

    int i;
    for (i = 0; i < table_size; i++) {
        if (0 != table[i].code && 0 != (table[i].code & code)  ) {
            if (!bEmpty)
                stream << VM_STRING("|");

            stream << table[i].string;
            bEmpty = false;
        }
    }
    if (bEmpty)
        stream <<VM_STRING("UNDEF(") << code <<VM_STRING(")");;

    return stream.str();
}

tstring GetMFXFourccString(mfxU32 mfxFourcc)
{
    return MFX_CODE_TO_STRING(StringsOfFourcc, mfxFourcc);
}

tstring   GetMFXChromaString(mfxU32 chromaFormat)
{
    return MFX_CODE_TO_STRING(StringsOfChromaFormat, chromaFormat);
}

tstring   GetMFXStatusString(mfxStatus mfxSts)
{
    return MFX_CODE_TO_STRING(StringsOfStatus, mfxSts);
}

tstring   GetMFXPicStructString(int ps)
{
    return MFX_CODE_TO_STRING_ALL(StringsOfPS, ps);
}

tstring GetMFXFrameTypeString (int FrameType)
{
    return MFX_CODE_TO_STRING_ALL(StringsOfFrameType, FrameType);
}

#define MAKE_HEX( x ) \
    std::setw(2) << std::setfill(VM_STRING('0')) << std::hex << (int)( x )

tstring   GetMFXRawDataString(mfxU8* pData, mfxU32 nData)
{
    tstringstream stream;
    stream << VM_STRING("[") << nData << VM_STRING("] = {");
    for (size_t j = 0; j < nData; j++)
    {
        stream << MAKE_HEX(pData[j]);

        if (j + 1 < nData)
            stream<< VM_STRING(" ");
    }
    stream << VM_STRING("}");
    return stream.str();
}



//const vm_char*     GetMFXImplString(mfxIMPL impl)
//{
//    return MFX_CODE_TO_STRING(StringsOfImpl, impl);
//}

tstring GetMFXImplString(mfxIMPL impl)
{
    tstring str1 = MFX_CODE_TO_STRING(StringsOfImpl, impl & ~(-MFX_IMPL_VIA_ANY));
    tstring str2 = MFX_CODE_TO_STRING(StringsOfImplVIA, impl & (-MFX_IMPL_VIA_ANY));

    return str1 + (str2 == VM_STRING("UNDEF(0)")? VM_STRING("") : VM_STRING(" | ") + str2);
}

void SetPrintInfoAlignLen(int val)
{
    vm_string_sprintf(g_AlignStr, VM_STRING("%%-%ds"), val);
}

//this type of print uses to print on the same line, it wont work if redirection case
void PrintInfoNoCR(const vm_char *name, const vm_char *value, ...)
{
#if defined(_WIN32) || defined(_WIN64)
    //print only if display console attached
    DWORD nMode;
    if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &nMode))
    {
        va_list argptr;
        va_start(argptr, value);

        vm_char str [1024];
        _vstprintf_p(str, sizeof(str)/sizeof(str[0]), value, argptr);

        vm_char outStr[20];
        _stprintf_s(outStr, VM_STRING("%s : %%s"), g_AlignStr);

        PipelineTrace((outStr, name, str));
    }
#else
    va_list argptr;
    vm_char str[1024] = {0}, outStr[20] = {0};

    va_start(argptr, value);
    vm_string_vsprintf(str, value, argptr);
    vm_string_sprintf(outStr, VM_STRING("%s : %%s"), g_AlignStr);
    PipelineTrace((outStr, name, str));
    va_end(argptr);
#endif
}

void PrintInfoForce(const vm_char *name, const vm_char *value, ...)
{
    va_list argptr;
    va_start(argptr, value);

    vPrintInfoEx(PipelineGetForceSink(g_InfoSink), name, value, argptr);
}

void PrintInfo(const vm_char *name, const vm_char *value, ...)
{
    va_list argptr;
    va_start(argptr, value);

    vPrintInfoEx(g_InfoSink, name, value, argptr);
}

void vPrintInfoEx(int nMode, const vm_char *name, const vm_char *value, va_list argptr)
{
#if defined(_WIN32) || defined(_WIN64)
    vm_char str [1024];
    _vstprintf_p(str, sizeof(str)/sizeof(str[0]), value, argptr);

    vm_char outStr[20];

    if (str[_tcslen(str) - 1] == 0xA)
    {
        _stprintf_s(outStr, VM_STRING("%s: %%s"), g_AlignStr);

    } else
    {
        _stprintf_s(outStr, VM_STRING("%s: %%s\n"), g_AlignStr);
    }

    PipelineTraceEx2(nMode, outStr, name, str);
#else
    vm_char str[1024] = {0}, outStr[20] = {0};

    vm_string_vsprintf(str, value, argptr);
    if (str[vm_string_strlen(str) - 1] == 0xA)
    {
        vm_string_sprintf(outStr, VM_STRING("%s : %%s"), g_AlignStr);
    } else
    {
        vm_string_sprintf(outStr, VM_STRING("%s : %%s\n"), g_AlignStr);
    }

    PipelineTraceEx2(nMode, outStr, name, str);
#endif
}

void IncreaseReference(mfxFrameData *ptr)
{
    vm_interlocked_inc16((volatile Ipp16u*)(&ptr->Locked));
}

void DecreaseReference(mfxFrameData *ptr)
{
    vm_interlocked_dec16((volatile Ipp16u*)&ptr->Locked);
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

mfxStatus GetMFXFrameInfoFromFOURCCPatternIdx(int idx_in_pattern, mfxFrameInfo &info)
{
    static const char valid_pattern [] = "nv12( |:mono)|yv12( |:mono)|rgb24|rgb32|yuy2(:h|:v|:mono)|ayuv|p010|a2rgb10|r16|argb16|nv16|p210|y410|i444";

    //if external pattern changed parsing need to be updated
    MFX_CHECK(!std::string(MFX_FOURCC_PATTERN()).compare(valid_pattern));

    info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    switch(idx_in_pattern)
    {
        case 1:
        {
            info.FourCC = MFX_FOURCC_NV12;
            break;
        }
        case 2:
        {
            info.FourCC = MFX_FOURCC_NV12;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV400;
            break;
        }
        case 3:
        {
            info.FourCC = MFX_FOURCC_YV12;
            break;
        }
        case 4:
        {
            info.FourCC = MFX_FOURCC_YV12;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV400;
            break;
        }
        case 5:
        {
            info.FourCC = MFX_FOURCC_RGB3;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            break;
        }
        case 6:
        {
            info.FourCC = MFX_FOURCC_RGB4;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            break;
        }
        case 7:
        {
            info.FourCC = MFX_FOURCC_YUY2;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV422H;
            break;
        }
        case 8:
        {
            info.FourCC = MFX_FOURCC_YUY2;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV422V;
            break;
        }
        case 9:
        {
            info.FourCC = MFX_FOURCC_YUY2;
            info.ChromaFormat = MFX_CHROMAFORMAT_MONOCHROME;
            break;
        }
#if defined(_WIN32) || defined(_WIN64)
        case 10:
        {
            info.FourCC = DXGI_FORMAT_AYUV;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            break;
        }
#endif
        case 11:
        {
            info.FourCC = MFX_FOURCC_P010;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            break;
        }
        case 12:
        {
            info.FourCC = MFX_FOURCC_A2RGB10;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            break;
        }
        case 13:
        {
            info.FourCC = MFX_FOURCC_R16;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            break;
        }
        case 14:
        {
            info.FourCC = MFX_FOURCC_ARGB16;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            break;
        }
        case 15:
        {
            info.FourCC = MFX_FOURCC_NV16;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            break;
        }
        case 16:
        {
            info.FourCC = MFX_FOURCC_P210;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            break;
        }
        case 17:
        {
            info.FourCC = MFX_FOURCC_Y410;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            break;
        }
        case 18:
        {
            info.FourCC = MFX_FOURCC_YUV444_8;
            info.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            break;
        }
        default:
            return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

mfxU32 GetMinPlaneSize(mfxFrameInfo & info)
{
    //number of bits per pixel*2
    mfxU32 bpp_m2 = 1;
    bool isGray = MFX_CHROMAFORMAT_MONOCHROME == info.ChromaFormat;
    if (isGray)
    {
        bpp_m2 = 2;
    }
    else
    {
        switch (info.FourCC)
        {
        case MFX_FOURCC_YV12:
        case MFX_FOURCC_NV12:
            bpp_m2 = 3;
            break;
        case MFX_FOURCC_P010:
            bpp_m2 = 6;
            break;
        case MFX_FOURCC_RGB3:
            bpp_m2 = 6;
            break;
        case MFX_FOURCC_RGB4:
            bpp_m2 = 8;
            break;
        case MFX_FOURCC_R16:
            bpp_m2 = 2;
            break;
        case MFX_FOURCC_YUY2:
        case MFX_FOURCC_NV16:
            bpp_m2 = 4;
            break;
        default:
            bpp_m2 = 8;
            break;
        }
    }

    return (bpp_m2 * info.CropW * info.CropH) >> 1;
}

void GetLoadedModulesList(std::list<tstring>&refModulesList)
{
#if ( defined(_WIN32) || defined(_WIN64) ) && !defined(WIN_TRESHOLD_MOBILE)
    HANDLE   hCurrent = GetCurrentProcess();
    HMODULE *pModules;
    DWORD    cbNeeded;
    int      nModules;
    if (NULL == EnumProcessModules(hCurrent, NULL, 0, &cbNeeded))
        return;

    nModules = cbNeeded / sizeof(HMODULE);

    pModules = new HMODULE[nModules];
    if (NULL == pModules)
    {
        return;
    }
    if (NULL == EnumProcessModules(hCurrent, pModules, cbNeeded, &cbNeeded))
    {
        delete []pModules;
        return;
    }

    for (int i = 0; i < nModules; i++)
    {
        tstring str;
        vm_char buf[2048];
        //str.resize(2048);
        GetModuleFileName(pModules[i], buf, MFX_ARRAY_SIZE(buf));
        refModulesList.push_back(buf);
    }

    delete []pModules;
#endif // #if ( defined(_WIN32) || defined(_WIN64) ) && !defined(WIN_TRESHOLD_MOBILE)
}

mfxU64           GetProcessMemoryLimit()
{
    static const mfxU64 _100M = (mfxU64)100*1024*1024;
    static const mfxU64 _3G = (mfxU64)3*1024*1024*1024;
    static const mfxU64 _4G = (mfxU64)4*1024*1024*1024;
    static const mfxU64 _8T = (mfxU64)8*1024*1024*1024*1024;

    mfxU64 processMemlimitRough = (std::numeric_limits<uintptr_t>::max)();
    mfxU64 processMemlimitPrec = processMemlimitRough;

#ifdef WIN32
    //http://msdn.microsoft.com/en-us/library/windows/desktop/aa366778(v=vs.85).aspx#memory_limits
#ifndef _WIN64

    BOOL isWow64 = FALSE;
    #ifndef WIN_TRESHOLD_MOBILE
    // win threshold mobile - temporary use only 3Gb
    IsWow64Process(GetCurrentProcess(), &isWow64);
    #endif 

    if (TRUE == isWow64)
    {
        processMemlimitPrec = _4G;
    }
    else
    {
        processMemlimitPrec = _3G;
    }
#else
    processMemlimitPrec = _8T;
#endif

    MEMORYSTATUSEX memEx;
    memEx.dwLength = sizeof (memEx);
    GlobalMemoryStatusEx(&memEx);

    processMemlimitPrec = (std::min)((mfxU64)memEx.ullAvailPhys, processMemlimitPrec);
#endif

    //100 mb threshold
    return processMemlimitPrec - (std::min)(_100M, processMemlimitPrec);
}


void PrintDllInfo(const vm_char *msg, const vm_char *filename)
{
    Ipp64u filelen;
    vm_char msg2[1024];
    if (1 == vm_file_getinfo(filename, &filelen, NULL))
    {
        PrintInfo(msg, VM_STRING("%s "), filename);

        vm_string_sprintf(msg2, VM_STRING("%s size"), msg);
        PrintInfo(msg2, VM_STRING("%I64d"), filelen);
    }
#ifdef WIN32
    struct _stat64 fileStat;
    int err = _wstat64(filename, &fileStat);

    if (!err)
    {
        vm_string_sprintf(msg2, VM_STRING("%s mtime"), msg);
        PrintInfo(msg2, VM_STRING("%s"), _tctime64(&fileStat.st_mtime));
    }
#endif
}


mfxStatus myMFXInit(const vm_char *pMFXLibraryPath, mfxIMPL impl, mfxVersion *pVer, mfxSession *session)
{
//#if 1
#if (MFX_VERSION_MAJOR >= 1) && (MFX_VERSION_MINOR >= 1)
    mfxStatus mfxRes = MFX_ERR_NONE;
    mfxVersion ver = {MFX_VERSION_MINOR, MFX_VERSION_MAJOR};
    MFX_DISP_HANDLE *pHandle = new MFX_DISP_HANDLE(NULL == pVer ? ver: *pVer);
    if (!pHandle) return MFX_ERR_MEMORY_ALLOC;

    msdk_disp_char path[MAX_PATH];
#if defined(_WIN32) || defined(_WIN64)
    #ifdef UNICODE
        swprintf(path, L"%s", pMFXLibraryPath);
    #else
        swprintf(path, L"%S", pMFXLibraryPath);
    #endif
#else
    vm_string_sprintf(path, "%s", pMFXLibraryPath);
#endif

    // implementation method masked from the input parameter
    const mfxIMPL implMethod = impl & (MFX_IMPL_VIA_ANY - 1);
    // implementation interface masked from the input parameter
    const mfxIMPL implInterface = impl & -MFX_IMPL_VIA_ANY;
    mfxU32 curImplIdx, maxImplIdx;

    curImplIdx = myimplTypesRange[implMethod].minIndex;
    maxImplIdx = myimplTypesRange[implMethod].maxIndex;

    do
    {
        if (MFX_ERR_NONE == mfxRes)
        {
            mfxInitParam par;

            memset(&par, 0, sizeof(mfxInitParam));
            // try to load the selected DLL using default DLL search mechanism
            mfxRes = pHandle->LoadSelectedDLL(path,
                                     myimplTypes[curImplIdx].implType,
                                     myimplTypes[curImplIdx].impl,
                                     implInterface,
                                     par);
            // unload the failed DLL
            if ((MFX_ERR_NONE != mfxRes) &&
                (MFX_WRN_PARTIAL_ACCELERATION != mfxRes))
            {
                pHandle->UnLoadSelectedDLL();
            }
        }
    }
    while ((MFX_ERR_NONE > mfxRes) && (++curImplIdx <= maxImplIdx));

    if (MFX_ERR_NONE == mfxRes)
    {
        *((MFX_DISP_HANDLE **) session) = pHandle;
    }
    return mfxRes;
#else
    return MFXInit(impl, pVer, session);
#endif
}

mfxStatus myMFXInitEx(const vm_char *pMFXLibraryPath, mfxInitParam par, mfxSession *session)
{
//#if 1
#if (MFX_VERSION_MAJOR >= 1) && (MFX_VERSION_MINOR >= 1)
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_DISP_HANDLE *pHandle = new MFX_DISP_HANDLE(par.Version);
    if (!pHandle) return MFX_ERR_MEMORY_ALLOC;

    msdk_disp_char path[MAX_PATH];
#if defined(_WIN32) || defined(_WIN64)
    #ifdef UNICODE
        swprintf(path, L"%s", pMFXLibraryPath);
    #else
        swprintf(path, L"%S", pMFXLibraryPath);
    #endif
#else
    vm_string_sprintf(path, "%s", pMFXLibraryPath);
#endif

    // implementation method masked from the input parameter
    const mfxIMPL implMethod = par.Implementation & (MFX_IMPL_VIA_ANY - 1);
    // implementation interface masked from the input parameter
    const mfxIMPL implInterface = par.Implementation & -MFX_IMPL_VIA_ANY;
    mfxU32 curImplIdx, maxImplIdx;

    curImplIdx = myimplTypesRange[implMethod].minIndex;
    maxImplIdx = myimplTypesRange[implMethod].maxIndex;

    do
    {
        if (MFX_ERR_NONE == mfxRes)
        {
            // try to load the selected DLL using default DLL search mechanism
            mfxRes = pHandle->LoadSelectedDLL(path,
                                     myimplTypes[curImplIdx].implType,
                                     myimplTypes[curImplIdx].impl,
                                     implInterface,
                                     par);
            // unload the failed DLL
            if ((MFX_ERR_NONE != mfxRes) &&
                (MFX_WRN_PARTIAL_ACCELERATION != mfxRes))
            {
                pHandle->UnLoadSelectedDLL();
            }
        }
    }
    while ((MFX_ERR_NONE > mfxRes) && (++curImplIdx <= maxImplIdx));

    if (MFX_ERR_NONE == mfxRes)
    {
        *((MFX_DISP_HANDLE **) session) = pHandle;
    }
    return mfxRes;
#else
    return MFXInit(impl, pVer, session);
#endif
}

void MessageFromCode(mfxU32 nCode, vm_char *pString, mfxU32 pStringLen)
{
#if defined(_WIN32) || defined(_WIN64)
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, nCode, 0, pString, pStringLen, 0);
#endif
}

mfxU16 Convert_CmdPS_to_MFXPS( mfxI32 cmdPicStruct )
{
    mfxU16 mfxPS;
    //converting picture structure from mfx_pipeline to mediasdk
    switch (cmdPicStruct)
    {
        case PIPELINE_PICSTRUCT_PROGRESSIVE:
        {
            mfxPS = MFX_PICSTRUCT_PROGRESSIVE;
            break;
        }

        case PIPELINE_PICSTRUCT_FIELD_TFF:
        case PIPELINE_PICSTRUCT_FIELD_TFF2:
        {
            mfxPS = MFX_PICSTRUCT_FIELD_TFF;
            break;
        }

        case PIPELINE_PICSTRUCT_FIELD_BFF:
        case PIPELINE_PICSTRUCT_FIELD_BFF2:
        {
            mfxPS = MFX_PICSTRUCT_FIELD_BFF;
            break;
        }

        default:
        //case PIPELINE_PICSTRUCT_UNKNOWN:
        //case PIPELINE_PICSTRUCT_NOT_SPECIFIED:
        {
            mfxPS = MFX_PICSTRUCT_UNKNOWN;
            break;
        }
    }

    return mfxPS;
}

mfxI32 Convert_MFXPS_to_CmdPS( mfxU16 mfxPicStruct, mfxU16 extco )
{
    mfxI16 pipelinePS;

    switch (mfxPicStruct)
    {
        case MFX_PICSTRUCT_FIELD_TFF:
            if (extco == MFX_CODINGOPTION_ON)
            {
                pipelinePS = PIPELINE_PICSTRUCT_FIELD_TFF;
            }else
            {
                pipelinePS = PIPELINE_PICSTRUCT_FIELD_TFF2;
            }
        break;

        case MFX_PICSTRUCT_FIELD_BFF:
            if (extco == MFX_CODINGOPTION_ON)
            {
                pipelinePS = PIPELINE_PICSTRUCT_FIELD_BFF;
            }else
            {
                pipelinePS = PIPELINE_PICSTRUCT_FIELD_BFF2;
            }
        break;

        case MFX_PICSTRUCT_PROGRESSIVE:
            pipelinePS = PIPELINE_PICSTRUCT_PROGRESSIVE;
        break;

        case MFX_PICSTRUCT_UNKNOWN :
            pipelinePS = PIPELINE_PICSTRUCT_UNKNOWN;
        break;

        default:
            pipelinePS = PIPELINE_PICSTRUCT_NOT_SPECIFIED;
        break;
    }

    return pipelinePS;
}

mfxU16  Convert_CmdPS_to_ExtCO(mfxI32 cmdPicStruct)
{
    mfxU16 extCO;
    //converting picture structure from mfx_pipeline to mediasdk
    switch (cmdPicStruct)
    {
        case PIPELINE_PICSTRUCT_FIELD_TFF:
        case PIPELINE_PICSTRUCT_FIELD_BFF:
        {
            extCO = MFX_CODINGOPTION_ON;
            break;
        }

        case PIPELINE_PICSTRUCT_FIELD_TFF2:
        case PIPELINE_PICSTRUCT_FIELD_BFF2:
        {
            extCO = MFX_CODINGOPTION_OFF;
            break;
        }

    default:
        //case PIPELINE_PICSTRUCT_UNKNOWN:
        //case PIPELINE_PICSTRUCT_NOT_SPECIFIED:
        {
            extCO = MFX_CODINGOPTION_UNKNOWN;
            break;
        }
    }

    return extCO;
}

tstring FileNameWithId(const vm_char * pInputFileName, int nPosition)
{
    if (NULL == pInputFileName)
    {
        return VM_STRING("");
    }
    //constructing name
    tstring file_name_ref = pInputFileName;
    tstringstream targetname;
    tstring::size_type pointpos;

    pointpos    = file_name_ref.rfind(VM_STRING('.'));
    targetname << file_name_ref.substr(0, pointpos) << VM_STRING("_") << nPosition;

    if (tstring::npos != pointpos)
        targetname << file_name_ref.substr(pointpos);

    return targetname.str();
}

mfxStatus CheckStatusMultiSkip(mfxStatus status_to_check, ... )
{
    va_list arg_ptr;
    va_start(arg_ptr, status_to_check);

    int current;
    while ((current = va_arg(arg_ptr, int), current != MFX_USER_ERR_STATUS))
    {
        if (status_to_check == current)
            return MFX_ERR_NONE;
    }
    return status_to_check;
}

void BSUtil::MoveNBytesUnsafe(mfxU8 *pTo, mfxBitstream *pFrom, mfxU32 nCopied)
{
    //copy to the end of bs
    memcpy(pTo, pFrom->Data + pFrom->DataOffset, nCopied);

    //moving offsets
    pFrom->DataLength -= nCopied;
    pFrom->DataOffset += nCopied;
}

void BSUtil::MoveNBytesUnsafe(mfxBitstream *pTo, mfxBitstream *pFrom, mfxU32 nCopied)
{
    MoveNBytesUnsafe(pTo->Data + pTo->DataOffset + pTo->DataLength, pFrom, nCopied);

    //moving dest offset
    pTo->DataLength   += nCopied;
}

mfxU32 BSUtil::MoveNBytes(mfxU8 *pTo, mfxBitstream *pFrom, mfxU32 nBytes)
{
    if (NULL == pTo || NULL == pFrom || NULL == pFrom->Data)
        return 0;

    nBytes = (std::min)(pFrom->DataLength, nBytes);
    MoveNBytesUnsafe(pTo, pFrom, nBytes);

    return nBytes;
}

mfxStatus BSUtil::MoveNBytesTail(mfxBitstream *pTo, mfxBitstream *pFrom, mfxU32 &nBytes)
{
    if (0 == nBytes)
        return MFX_ERR_NONE;

    MFX_CHECK_POINTER(pFrom);
    MFX_CHECK_POINTER(pFrom->Data);

    mfxU32 nAvail   = WriteAvailTail(pTo);
    mfxU32 nCanCopy = (std::min)(pFrom->DataLength, nBytes);
    mfxU32 nCopied  = (std::min)(nAvail, nCanCopy);

    //buffer not enough
    if (0 == nAvail)
    {
        //soure is empty as well
        if (0 == nCanCopy)
            return MFX_ERR_NONE;
        //source not empty
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    }
    //source not enough
    if (0 == nCanCopy)
    {
        if (nBytes > pFrom->DataLength)
            return MFX_ERR_MORE_DATA;

        return MFX_ERR_NONE;
    }

    MoveNBytesUnsafe(pTo, pFrom, nCopied);

    nBytes = nCopied;

    return MFX_ERR_NONE;
}

mfxStatus BSUtil::MoveNBytesStrictTail(mfxBitstream *pTo, mfxBitstream *pFrom, mfxU32 nBytes)
{
    MFX_CHECK_POINTER(pFrom);
    MFX_CHECK_POINTER(pFrom->Data);

    mfxU32 nCanCopy = (std::min)(pFrom->DataLength, nBytes);

    //buffer not enough
    if (WriteAvailTail(pTo) < nBytes)
    {
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    }
    //source not enough
    if (nCanCopy < nBytes)
    {
        return MFX_ERR_MORE_DATA;
    }

    MoveNBytesUnsafe(pTo, pFrom, nBytes);

    return MFX_ERR_NONE;
}

mfxU32 BSUtil::WriteAvailTail (mfxBitstream *pTo)
{
    if (!pTo)
        return 0;
    if (!pTo->Data)
        return 0;

    return pTo->MaxLength - pTo->DataLength - pTo->DataOffset;
}

mfxU32 BSUtil::WriteAvail (mfxBitstream *pTo)
{
    if (!pTo)
        return 0;
    if (!pTo->Data)
        return 0;

    return pTo->MaxLength - pTo->DataLength;
}

mfxStatus BSUtil::MoveNBytesStrict(mfxBitstream *pTo, mfxBitstream *pFrom, mfxU32 /*[IN OUT]*/ &nBytes)
{
    if (nBytes > WriteAvail(pTo))
        return MFX_ERR_NOT_ENOUGH_BUFFER;

    mfxStatus sts = MoveNBytesStrictTail(pTo, pFrom, nBytes);
    if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
    {
        //moving existed data to zero offset
        memmove(pTo->Data, pTo->Data + pTo->DataOffset, pTo->DataLength);
        pTo->DataOffset = 0;

        sts = MoveNBytesStrictTail(pTo, pFrom, nBytes);
    }

    return sts;
}

mfxStatus BSUtil::Extend(mfxBitstream *pTo, size_t nRequiredSize)
{
    MFX_CHECK_POINTER(pTo);
    if (pTo->MaxLength >= nRequiredSize)
        return MFX_ERR_NONE;

    delete [] pTo->Data;
    pTo->Data = new mfxU8[nRequiredSize];
    pTo->MaxLength = (mfxU32)nRequiredSize;

    return MFX_ERR_NONE;
}

size_t BSUtil::Reserve(mfxBitstream *pTo, size_t nRequiredSize)
{
    size_t avail_tail = pTo->MaxLength - (pTo->DataLength + pTo->DataOffset);
    if (avail_tail >= nRequiredSize)
        return avail_tail;

    memmove(pTo->Data, pTo->Data + pTo->DataOffset, pTo->DataLength);
    pTo->DataOffset = 0;

    return pTo->MaxLength - pTo->DataLength;
}

#ifdef UNICODE
#include <clocale>
#include <locale>
#endif

tstring cvt_s2t(const std::string & lhs)
{
#ifndef UNICODE
    return lhs;
#else
    std::locale const loc("");
    std::vector<wchar_t> buffer(lhs.size() + 1);
    std::use_facet<std::ctype<wchar_t> >(loc).widen(&lhs.at(0), &lhs.at(0) + lhs.size(), &buffer[0]);
    return tstring(&buffer[0], &buffer[lhs.size()]);

#endif

}