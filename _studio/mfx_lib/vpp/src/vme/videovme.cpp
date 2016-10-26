//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2013 Intel Corporation. All Rights Reserved.
//

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
