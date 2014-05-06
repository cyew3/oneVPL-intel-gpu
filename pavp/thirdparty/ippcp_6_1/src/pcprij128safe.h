/*
//               INTEL CORPORATION PROPRIETARY INFORMATION
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//   Copyright (c) 2007-2008 Intel Corporation. All Rights Reserved.
//
//
// Purpose:
//    Cryptography Primitive.
//    Internal Safe Rijndael Encrypt, Decrypt
//
//    Created: Mon 08-Oct-2007 18:24
//  Author(s): Sergey Kirillov
//
*/
#if !defined(_PCP_SAFE_RIJ_H)
#define _PCP_SAFE_RIJ_H

Ipp8u TransformByte(Ipp8u x, const Ipp8u Transformation[]);
void TransformNative2Composite(Ipp8u out[], const Ipp8u inp[], int len);


#if !((_IPP ==_IPP_V8) || (_IPP==_IPP_P8) || \
      (_IPPLP32 ==_IPPLP32_S8) || (_IPP==_IPP_G9) || \
      (_IPP32E==_IPP32E_U8) || (_IPP32E==_IPP32E_Y8) || \
      (_IPPLP64==_IPPLP64_N8) || (_IPP32E==_IPP32E_E9))

Ipp8u InverseComposite(Ipp8u x);
void TransformComposite2Native(Ipp8u out[], const Ipp8u inp[], int len);
void AddRoundKey(Ipp8u out[], const Ipp8u inp[], const Ipp8u pKey[]);

#endif
#endif /* _PCP_SAFE_RIJ_H */
