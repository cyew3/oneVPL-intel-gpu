//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_H265_INTRAPRED_OPT_H__
#define __MFX_H265_INTRAPRED_OPT_H__

void h265_filter_pred_pels(PixType* PredPel,
                           Ipp32s width);

void h265_intrapred_ver(PixType* PredPel,
                         PixType* pels,
                         Ipp32s pitch,
                         Ipp32s width,
                         Ipp32s bit_depth,
                         Ipp32s isLuma);

void h265_intrapred_hor(PixType* PredPel,
                        PixType* pels,
                        Ipp32s pitch,
                        Ipp32s width,
                        Ipp32s bit_depth,
                        Ipp32s isLuma);

void h265_intrapred_dc(PixType* PredPel,
                       PixType* pels,
                       Ipp32s pitch,
                       Ipp32s width,
                       Ipp32s isLuma);

void h265_intrapred_ang(Ipp32s mode,
                        PixType* PredPel,
                        PixType* pels,
                        Ipp32s pitch,
                        Ipp32s width);

void h265_intrapred_planar(PixType* PredPel,
                           PixType* pels,
                           Ipp32s pitch,
                           Ipp32s width);

#endif // __MFX_H265_INTRAPRED_OPT_H__
