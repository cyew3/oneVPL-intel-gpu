/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2012 Intel Corporation. All Rights Reserved.

File Name: msdk_defs.h

\* ****************************************************************************** */

#pragma once

#define MSDKA_LEVEL_GLOBAL 1
#define MSDKA_LEVEL_FRAME  2

#define GET_COMPONENT(XX)\
    (Component)(XX & 0xFF)

#define GET_DIRECTION(XX)\
    (XX & 0xF00)

//component + direction
#define GET_STREAM(XX)\
    (XX & 0xFFF)

#define GET_FLAGS(XX)\
    (XX & ~0xFFF)


#define RECORD_CONFIGURATION2(XX,LL,YY) \
    if (XX) {    \
    FILE *fd=NULL;  \
    int level=LL; \
    if (!gc.async_dump)_tfopen_s(&fd,gc.log_file,TEXT("a+"));  \
    if (fd || gc.async_dump) {   \
    YY; \
    } \
    if(fd) { \
    fclose(fd); \
    }   \
    }

#define RECORD_RAW_BITS_UNCONSTRAINED(XX, YY) \
    if (GET_STREAM(gc.record_raw_bits_module)==GET_STREAM(XX)) { \
    FILE *fd2=NULL;  \
    _tfopen_s(&fd2,gc.raw_file,TEXT("ab+"));  \
    if (fd2) {   \
    YY; \
    fclose(fd2); \
    }}

#define RECORD_RAW_BITS(XX,KK,YY) \
    if (GET_STREAM(gc.record_raw_bits_module)==GET_STREAM(XX) && (KK)>=gc.record_raw_bits_start && (KK)<gc.record_raw_bits_end) { \
    FILE *fd2=NULL;  \
    _tfopen_s(&fd2,gc.raw_file,TEXT("ab+"));  \
    if (fd2) {   \
    YY; \
    fclose(fd2); \
    }}

#define RECORD_RAW_BITS_IF_FLAG(XX,KK,YY) \
    if (gc.record_raw_bits_module==(XX) && (KK)>=gc.record_raw_bits_start && (KK)<gc.record_raw_bits_end) { \
        FILE *fd2=NULL;  \
        _tfopen_s(&fd2,gc.raw_file,TEXT("ab+"));  \
        if (fd2) {   \
        YY; \
        fclose(fd2); \
        }}

#define RECORD_RAW_BITS_IF_NOFLAG(XX,KK,YY) \
    if (GET_STREAM(gc.record_raw_bits_module)==GET_STREAM(XX) && (!(GET_FLAGS(XX) & GET_FLAGS(gc.record_raw_bits_module))) && \
        (KK)>=gc.record_raw_bits_start && (KK)<gc.record_raw_bits_end) { \
        FILE *fd2=NULL;  \
        _tfopen_s(&fd2,gc.raw_file,TEXT("ab+"));  \
        if (fd2) {   \
        YY; \
        fclose(fd2); \
        }}


#define RECORD_CONFIGURATION(XX) RECORD_CONFIGURATION2(gc.record_configuration, MSDKA_LEVEL_GLOBAL, XX)
#define RECORD_CONFIGURATION_PER_FRAME(XX) RECORD_CONFIGURATION2(gc.record_configuration_per_frame, MSDKA_LEVEL_FRAME, XX)
//pointers are different from run to run so printing its value may slowdown problem troubleshooting
#define RECORD_POINTERS(PP) if (gc.record_pointers){PP;}

#define EDIT_CONFIGURATION2(XX,YY,ZZ) \
    if (XX) {   \
    int len=(int)(sizeof(YY)/sizeof(TCHAR)-1);  \
    for (LinkedString *ls=gc.edit_configuration_lines;ls;ls=ls->next) { \
    if (_tcsncmp(ls->string,YY,len)) continue;    \
    TCHAR *line2=ls->string+len;  \
    ZZ; \
    }   \
    }

#define EDIT_CONFIGURATION(XX,YY) EDIT_CONFIGURATION2(gc.edit_configuration,XX,YY)
#define EDIT_CONFIGURATION_PER_FRAME(XX,YY) EDIT_CONFIGURATION2(gc.edit_configuration_per_frame,XX,YY)

#define MFX_CALL(fnc_name, params)\
    (gc.use_dispatcher? DISPATCHER_EXPOSED_PREFIX(fnc_name)params :(*p##fnc_name)params)
