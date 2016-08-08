/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfxstructures.h"
#include <vector>

//////////////////////////////////////////////////////////////////////////
//trace support functions
void             PipelineSetLastErr( mfxU32
                                    , const vm_char * check = NULL
                                    , const vm_char * pFile = NULL
                                    , int     line  = 0
                                    , bool    bwrn  = false);

void             PipelineSetLastMFXErr( mfxU32
                                       , mfxStatus sts
                                       , const vm_char * pAction
                                       , const vm_char * pFile = NULL
                                       , int     line  = 0);

void             PipelineTraceMFXWrn( mfxStatus sts
                                     , const vm_char * pAction
                                     , const vm_char * pFile = NULL
                                     , int     line  = 0);


mfxU32           PipelineGetLastErr( void );

//sets trace messages sink, returned previous sink
//provider mode 1 - info 2 - error 4 - warning 0xff - all
mfxU32           PipelineSetTrace( mfxU32 nMode, mfxU32 new_sink);

//traces variable parameters formated string to infosink
void             PipelineTraceEx(const vm_char *str, ...);

//traces variable parameters formated string to specified sink
void             PipelineTraceEx2(int mode, const vm_char *str, ...);

//traces variable parameters formated string to additionaly to error console if output was redirectered
void             PipelineTraceForceEx(const vm_char *str, ...);

//if output console redirectered, function adds error sink
mfxU32           PipelineGetForceSink(mfxU32 original_sink);


//list version of pipelinetrace ex
void             vPipelineTraceEx2(int mode, const vm_char *str, va_list ptr);

void             PipelineSetFrameTrace( bool bOn);
void             PipelineTraceEncFrame( mfxU16 FrameType);
//used to print frame from several decoders, in future prefix and postfix may require
void             PipelineTraceDecFrame( int nDecode = 0);
void             PipelineTraceSplFrame( void );

void             PipelineBuildTime( const vm_char *&platformName 
                                   , int &year
                                   , int &month
                                   , int &day
                                   , int &hour
                                   , int &minute
                                   , int &sec);

//conversion 
tstring          GetMFXFourccString(mfxU32 mfxFourcc);
tstring          GetMFXChromaString(mfxU32 chromaFormat);
tstring          GetMFXStatusString(mfxStatus mfxSts);
tstring          GetMFXPicStructString(int ps);
tstring          GetMFXFrameTypeString(int ps);
tstring          GetMFXRawDataString(mfxU8* pData, mfxU32 nData);
tstring          GetMFXImplString(mfxIMPL impl);

mfxU64           ConvertmfxF642MFXTime(mfxF64 fTime);
mfxF64           ConvertMFXTime2mfxF64(mfxU64 nTime);
//memory require to store plane's data, i.e. crops no count 
mfxU32           GetMinPlaneSize(mfxFrameInfo & info);

//possible extension to the end of this enum
#define MFX_FOURCC_PATTERN() "nv12( |:mono)|yv12( |:mono)|rgb24|rgb32|yuy2(:h|:v|:mono)|ayuv|p010|a2rgb10|r16|argb16|nv16|p210|y410"
//gets and mediasdk fourcc & chroma format fields set from given fourcc index
mfxStatus        GetMFXFrameInfoFromFOURCCPatternIdx(int idx_in_pattern, mfxFrameInfo &info);

//converts command line version of picture structure (-s picstruct)to MFX
mfxU16           Convert_CmdPS_to_MFXPS(mfxI32 cmdPicStruct);
//extracts ext coding options from pipeline picstruct (-s picstruct)
mfxU16           Convert_CmdPS_to_ExtCO(mfxI32 cmdPicStruct);

//converts MFX picture structure with ext coding option to command line version (-s picstruct)
mfxI32           Convert_MFXPS_to_CmdPS(mfxU16 mfxPicStruct, mfxU16 extco = MFX_CODINGOPTION_UNKNOWN);

//utilityz
void             IncreaseReference(mfxFrameData *ptr);
void             DecreaseReference(mfxFrameData *ptr);
void             SetPrintInfoAlignLen(int len);
void             PrintInfoNoCR(const vm_char *name, const vm_char *value, ...);

void             PrintInfo(const vm_char *name, const vm_char *value, ...);

void             vPrintInfoEx(int mode, const vm_char *name, const vm_char *value, va_list ptr);
//same behavior as Force suffix does for trace commands
void             PrintInfoForce(const vm_char *name, const vm_char *value, ...);

void             MessageFromCode(mfxU32 nCode, vm_char *pString, mfxU32 pStringLen);


//lists all loaded modules
void             GetLoadedModulesList(std::list<tstring>&);

//current processmemory limit, depending on free system ram, and windows architeture
mfxU64           GetProcessMemoryLimit();

// Print file info
void             PrintDllInfo(const vm_char *msg, const vm_char *filename);

//constructs filename with givven integer id before file extension
tstring          FileNameWithId(const vm_char * pInputFileName, int nPosition);

mfxStatus        CheckStatusMultiSkip(mfxStatus status_to_check, ... );

// Load specific MFX library
mfxStatus       myMFXInit(const vm_char *pMFXLibraryPath, mfxIMPL impl, mfxVersion *pVer, mfxSession *session);
mfxStatus       myMFXInitEx(const vm_char *pMFXLibraryPath, mfxInitParam par, mfxSession *session);

//bitstreams utils
class BSUtil
{
public: 
    //copy as much data as it can, but no more than nBytes
    //and move offset in source buffer, returns number of copied bytes 
    static mfxU32 MoveNBytes(mfxU8 *pTo, mfxBitstream *pFrom, mfxU32 nBytes);

    //copy as much data as it can, but no more than nBytes, and donot move present bytes inside destination buffer
    //and move offset in source buffer, nBytes contains number of copied bytes 
    static mfxStatus MoveNBytesTail(mfxBitstream *pTo, mfxBitstream *pFrom, mfxU32 /*[IN OUT]*/ &nBytes);
    
    //same behavior as MoveNBytesToEnd
    //if fail to move strictly nBytesReportsOf an error
    static mfxStatus MoveNBytesStrictTail(mfxBitstream *pTo, mfxBitstream *pFrom, mfxU32 nBytes);

    //return number of bytes available to write to data pointer without moving bitstream
    static mfxU32 WriteAvailTail (mfxBitstream *pTo);

    //return number of bytes available to write , however moving of persisted data may require
    static mfxU32 WriteAvail (mfxBitstream *pTo);

    //copy as much data as it can, but no more than nBytes, 
    //if not enough space at the end it will move exsited data to the zero offset
    //and move offset in source buffer, nBytes contains number of copied bytes 
    static mfxStatus MoveNBytesStrict(mfxBitstream *pTo, mfxBitstream *pFrom, mfxU32 /*[IN OUT]*/ &nBytes);

    //extends bitstream if necessary
    static mfxStatus Extend(mfxBitstream *pTo, size_t nRequiredSize);

    //reserve as much bytes as possible, but not more than required at the end of buffer without increasing
    //return num bytes reserved
    static size_t Reserve(mfxBitstream *pTo, size_t nRequiredSize);

private:
    //no pointers or size checks, just copy and offsets adjustments
    static void MoveNBytesUnsafe(mfxBitstream *pTo, mfxBitstream *pFrom, mfxU32 nBytes);
    
    //no pointers or size checks, just copy and offsets adjustments
    static void MoveNBytesUnsafe(mfxU8 *pTo, mfxBitstream *pFrom, mfxU32 nBytes);
};

tstring cvt_s2t(const std::string & lhs);

//aligns to arbitrary value
template<class T> 
inline T mfx_align(const T& value, unsigned int alignment)
{
    return static_cast<T>((alignment) * ( (value) / (alignment) + (((value) % (alignment)) ? 1 : 0)));
}

template <class T, class T1>
void mfx_silent_assign(T &left, const T1 & right)
{
    left = static_cast<T>(right);
}