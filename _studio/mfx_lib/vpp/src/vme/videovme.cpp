// Copyright (c) 2007-2018 Intel Corporation
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

#include "meforgen7_5.h"

/*****************************************************************************************************/
VideoVME::VideoVME( )
/*****************************************************************************************************/
{
    p = (mfxHDL)(new MEforGen75);
    ((MEforGen75 *)p)->LoadFlag = 0;
}

/*****************************************************************************************************/
VideoVME::~VideoVME( )
/*****************************************************************************************************/
{
    delete (MEforGen75 *)p;
}

/*****************************************************************************************************/
Status VideoVME::RunSIC(void *vme_uni, void *vme_sic, void *vout)
/*****************************************************************************************************/
{
    return ((MEforGen75 *)p)->RunSIC((mfxVME7_5UNIIn *)vme_uni, (mfxVME7_5SICIn *)vme_sic, (mfxVME7_5UNIOutput *)vout);
}

/*****************************************************************************************************/
Status VideoVME::RunIME(void *vme_uni, void *vme_ime, void *vout_uni, void *vout_ime)
/*****************************************************************************************************/
{
    return ((MEforGen75 *)p)->RunIME((mfxVME7_5UNIIn *)vme_uni, (mfxVME7_5IMEIn *)vme_ime, 
        (mfxVME7_5UNIOutput *)vout_uni, (mfxVME7_5IMEOutput *)vout_ime);
}

/*****************************************************************************************************/
Status VideoVME::RunFBR(void *vme_uni, void *vme_fbr, void *vout)
/*****************************************************************************************************/
{
    return ((MEforGen75 *)p)->RunFBR((mfxVME7_5UNIIn *)vme_uni, (mfxVME7_5FBRIn *)vme_fbr, (mfxVME7_5UNIOutput *)vout);
}

#ifdef VMEPERF
/*****************************************************************************************************/
Status  VideoVME::GetPerf(mfxVMEPerf * perf)
/*****************************************************************************************************/
{
    return ((MEforGen75 *)p)->GetPerf(perf);
}

/*****************************************************************************************************/
Status  VideoVME::ResetPerf( )
/*****************************************************************************************************/
{
    return ((MEforGen75 *)p)->ResetPerf();
}
#endif
