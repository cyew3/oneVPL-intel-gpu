/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2005-2016 Intel Corporation. All Rights Reserved.

**********************************************************************************/
#include "modified_sample_fei.h"
#pragma warning( disable : 4702)

#define MSDK_DEBUG mfxFrameSurface1* decodedSurf = NULL;\
                   for (std::list<std::pair<mfxFrameSurface1*, mfxFrameSurface1*> >::iterator it = m_DecodeToVppCorresp.begin(); it != m_DecodeToVppCorresp.end(); ++it)\
                   {\
                       if ((*it).second == eTask->in.InSurface)\
                       {\
                           decodedSurf = (*it).first;\
                       }\
                   }\
                   for (int i = 0; i < decodedSurf->Data.NumExtParam; i++)\
                       if (decodedSurf->Data.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_DEC_STREAM_OUT)\
                       {\
                           m_pExtBufDecodeStreamout = (mfxExtFeiDecStreamOut*)decodedSurf->Data.ExtParam[i];\
                           break;\
                       }\
                   if (m_encpakParams.bDECODESTREAMOUT && m_encpakParams.bOnlyPAK)\
                   {\
                     sts = PakOneStreamoutFrame(m_pExtBufDecodeStreamout, m_numOfFields, eTask);\
                     MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);\
                   }\
                   else
