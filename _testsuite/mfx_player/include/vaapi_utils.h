/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#ifndef __VAAPI_UTILS_H__
#define __VAAPI_UTILS_H__

#ifdef LIBVA_SUPPORT

#include <va/va.h>
#include "sample_utils.h"

class CLibVA
{
public:
    CLibVA(void);
    virtual ~CLibVA(void);

    virtual mfxStatus Init(void);
    virtual void Close(void);

    VADisplay GetVADisplay(void) { return m_va_dpy; }
    bool isVAInitialized(void) { return m_bVAInitialized; }

protected:
    void *m_display;
    VADisplay m_va_dpy;
    bool m_bVAInitialized;

private:
    DISALLOW_COPY_AND_ASSIGN(CLibVA);
};

mfxStatus va_to_mfx_status(VAStatus va_res);

#endif // #ifdef LIBVA_SUPPORT

#endif // #ifndef __VAAPI_UTILS_H__
