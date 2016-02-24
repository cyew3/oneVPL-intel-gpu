/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2005-2016 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __VIDEO_APP_DEFS_H
#define __VIDEO_APP_DEFS_H

#define ERROR_NOERR       MFX_ERR_NONE
#define ERROR_NOTINIT     MFX_ERR_NOT_INITIALIZED
#define ERROR_PARAMS      MFX_ERR_UNSUPPORTED
#define ERROR_FILE_OPEN   MFX_ERR_NOT_FOUND
#define ERROR_FILE_READ   MFX_ERR_NOT_FOUND

#define INVALID_SURF_IDX 0xFFFF

#define MAX_FILELEN 1024

#define DEC_WAIT_INTERVAL 60000
#define ENC_WAIT_INTERVAL 10000
#define VPP_WAIT_INTERVAL 60000

#define PRINT_RET_MSG(ERR) vm_string_printf(VM_STRING("Return on error: error code %d,\t%s\t%d\n\n"), ERR, VM_STRING(__FILE__), __LINE__)

#define CHECK_ERROR(P, X, ERR)              {if ((X) == (P)) {PRINT_RET_MSG(ERR); return ERR;}}
#define CHECK_NOT_EQUAL(P, X, ERR)          {if (static_cast<unsigned int>(X) != static_cast<unsigned int>(P)) {PRINT_RET_MSG(ERR); return ERR;}}
#define CHECK_RESULT(P, X, ERR)             {if ((X) > (P)) {PRINT_RET_MSG(ERR); return ERR;}}
#define CHECK_RESULT_SAFE(P, X, ERR, ACTION) {if ((X) > (P)) {ACTION; PRINT_RET_MSG(ERR); return ERR;}}
#define IGNORE_MFX_STS(P, X)                    {if ((X) == (P)) {P = MFX_ERR_NONE;}}
#define CHECK_POINTER(P, ERR)               {if ( 0  == (P)) {return ERR;}}
#define CHECK_POINTER_NO_RET(P)             {if ( 0  == (P)) {return;}}
#define CHECK_POINTER_SAFE(P, ERR, ACTION)  {if ( 0  == (P)) {ACTION; return ERR;}}
#define BREAK_ON_ERROR(P)                   {if(MFX_ERR_NONE != (P)) break;}
#define SAFE_DELETE_ARRAY(P)                {if (P) {delete[] P; P = NULL;}}
#ifndef SAFE_DELETE
#define SAFE_DELETE(P)                      {if (P) {delete P; P = NULL;}}
#endif
#define ZERO_MEMORY(VAR)                    {memset(&VAR, 0, sizeof(VAR));}
#define MFX_CHECK_STATUS(value) {mfxStatus sts = value; CHECK_NOT_EQUAL(sts,MFX_ERR_NONE,sts);}

#define ALIGN16(SZ)                         (((SZ + 15) >> 4) << 4) // round up to a multiple of 16
#define ALIGN32(SZ)                         (((SZ + 31) >> 5) << 5) // round up to a multiple of 32

#endif
