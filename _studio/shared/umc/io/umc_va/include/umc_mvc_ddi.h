// Copyright (c) 2006-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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

#endif // __UMC_MVC_DDI_H
