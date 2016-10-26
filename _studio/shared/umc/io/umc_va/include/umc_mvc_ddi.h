//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_MVC_DDI_H
#define __UMC_MVC_DDI_H

#pragma warning(disable: 4201)

enum _DXVA_Intel_ExtensionCompressedBuffers
{
    DXVA_MVCPictureParametersExtBufferType = 23
};

typedef struct _DXVA_Intel_PicParams_MVC
{
    USHORT      CurrViewID;
    UCHAR       anchor_pic_flag;
    UCHAR       inter_view_flag;
    UCHAR       NumInterViewRefsL0;
    UCHAR       NumInterViewRefsL1;

    union
    {
        UCHAR       bPicFlags;
        struct
        {
            UCHAR   SwitchToAVC : 1;
            UCHAR   Reserved7Bits : 7;
        };
    };

    UCHAR       Reserved8Bits;
    USHORT      ViewIDList[16];
    USHORT      InterViewRefList[2][16];
} DXVA_Intel_PicParams_MVC;

#endif __UMC_MVC_DDI_H
