/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/


#include "mfx_samples_config.h"

//////////////////////////////////////////////////////////////////////////
// Button.cpp: Button control class.
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// File: Button.cpp
// Desc: Button control classes
//
//  Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "wincontrol.h"
#include "button.h"

/****************************** Button class ****************************/

//-----------------------------------------------------------------------------
// Name: CreateText
// Description: Creates a simple text button
//
// hParent:   Parent window
// szCaption: Text on the button
// nID:       Command ID.
// rcBound:   Bounding rectangle.
//-----------------------------------------------------------------------------

HRESULT Button::CreateText(HWND hParent, const TCHAR *szCaption, int nID,
                               const Rect& rcBound)
{
    CREATESTRUCT create;
    ZeroMemory(&create, sizeof(CREATESTRUCT));

    create.x = rcBound.left;
    create.y = rcBound.top;
    create.cx = rcBound.right - create.x;
    create.cy = rcBound.bottom - create.y;

    create.hwndParent = hParent;
    create.lpszName = szCaption;
    create.hMenu = (HMENU)(INT_PTR)nID;
    create.lpszClass = TEXT("BUTTON");
    create.style = BS_PUSHBUTTON | BS_FLAT;
    return Control::Create(create);
}

//-----------------------------------------------------------------------------
// Name: CreateBitmap
// Description: Creates a simple bitmap button
//
// hParent: Parent window
// nImgID:  Resource ID of the bitmap
// nID:     Command ID.
// rcBound: Bounding rectangle.
//-----------------------------------------------------------------------------

HRESULT Button::CreateBitmap(HWND hParent, int nImgID, int nID, const Rect& rcSize)
{
    HRESULT hr = CreateText(hParent, NULL, nID, rcSize);
    if (SUCCEEDED(hr))
    {
        SetImage(nImgID);
    }
    return hr;
}

//-----------------------------------------------------------------------------
// Name: SetImage
// Description: Set a bitmap for the button
//
// nImgID:  Resource ID of the bitmap
//-----------------------------------------------------------------------------

BOOL Button::SetImage(WORD nImgId)
{
    AddStyle(BS_BITMAP);
    HBITMAP hBitmap = SetBitmapImg(GetInstance(), nImgId, m_hwnd);
    return (hBitmap ? TRUE : FALSE);
}
