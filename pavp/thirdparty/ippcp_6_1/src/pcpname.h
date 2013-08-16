/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2002-2006 Intel Corporation. All Rights Reserved.
//
//              Intel(R) Integrated Performance Primitives
//                  Cryptographic Primitives (ippcp)
//
//  Purpose:
//
//  Author(s): Alexey Korchuganov
//
//  Created: 2-Aug-1999 21:27
//
*/
#ifndef __PCPNAME_H__
#define __PCPNAME_H__

/*
   The prefix of library without the quotes. The prefix is directly used to generate the
   GetLibVersion function. It is used to generate the names in the dispatcher code, in
   the version description and in the resource file.
*/
#define LIB_PREFIX          ippcp

/*
   Names of library. It is used in the resource file and is used to generate the names
   in the dispatcher code.
*/
#define IPP_LIB_LONGNAME()     "Cryptography"
#define IPP_LIB_SHORTNAME()    "ippCP"



#define GET_STR2(x)      #x
#define GET_STR(x)       GET_STR2(x)
#define IPP_LIB_PREFIX() GET_STR(LIB_PREFIX)
#define IPP_INC_NAME()   GET_STR(LIB_PREFIX##.h)

#endif /* __PCPNAME_H__ */
/* ///////////////////////// End of file "pcpname.h" ///////////////////////// */
