//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_MPEG2_ENC_COMMON_HW_H__
#define __MFX_MPEG2_ENC_COMMON_HW_H__

#include "mfx_common.h"
#include "mfx_ext_buffers.h"

#if defined(MFX_VA)
#if defined (MFX_ENABLE_MPEG2_VIDEO_PAK) || defined (MFX_ENABLE_MPEG2_VIDEO_ENC) || defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)

#define D3DDDIFORMAT        D3DFORMAT
#define DXVADDI_VIDEODESC   DXVA2_VideoDesc

#if   defined(MFX_VA_WIN)
    #include "encoder_ddi.hpp"
    #include "auxiliary_device.h"
    #define PAVP_SUPPORT
#elif defined(MFX_VA_LINUX) || defined(MFX_VA_OSX)
    #include "mfx_h264_encode_struct_vaapi.h"
#endif

#include "mfxstructures.h"

#include <vector>
#include <list>

#ifdef PAVP_SUPPORT
#include "mfxpcp.h"
#endif



#ifdef MPEG2_ENC_HW_PERF
#include "vm_time.h"
#endif 

#define ENCODE_ENC_CTRL_CAPS ENCODE_ENC_CTRL_CAPS

//#define __SW_ENC


//#define MPEG2_ENCODE_HW_PERF
//#define MPEG2_ENC_HW_PERF
//#define MPEG2_ENCODE_DEBUG_HW


    enum
    {
      MFX_MPEG2_TOP_FIELD     = 1,
      MFX_MPEG2_BOTTOM_FIELD  = 2,
      MFX_MPEG2_FRAME_PICTURE = 3
    };

    struct mfxVideoParamEx_MPEG2
    {
        mfxVideoParam           mfxVideoParams;
        bool                    bFieldCoding;
        mfxU32                  MVRangeP[2];
        mfxU32                  MVRangeB[2][2];
        bool                    bAllowFieldPrediction;
        bool                    bAllowFieldDCT;
        bool                    bAddEOS;
        bool                    bRawFrames;
        mfxFrameAllocResponse*  pRecFramesResponse_hw;
        mfxFrameAllocResponse*  pRecFramesResponse_sw;
#ifdef PAVP_SUPPORT
        mfxExtPAVPOption        sExtPAVPOption;
#endif
#ifdef MFX_UNDOCUMENTED_QUANT_MATRIX
        mfxExtCodingOptionQuantMatrix sQuantMatrix;
#endif

        mfxExtVideoSignalInfo   videoSignalInfo;
        bool                    bAddDisplayExt;
        bool                    bMbqpMode;
        bool                    bDisablePanicMode;
    };

namespace MfxHwMpeg2Encode
{
   #define NUM_FRAMES 800

#if defined(MFX_VA_WIN)
   struct mfxRecFrames
   {
        mfxMemId    mids[NUM_FRAMES];         
        mfxU16      indexes[NUM_FRAMES];    
        mfxU16      NumFrameActual;
   };

   struct mfxRawFrames
   {
        mfxMemId    mids[NUM_FRAMES];         
        mfxU16      NumFrameActual;
   };    
#else

    struct ExtVASurface
    {
        VASurfaceID surface;
        mfxU32 number;
        mfxU32 idxBs;
    };
    
    typedef std::vector<ExtVASurface> mfxRecFrames;
    typedef std::vector<ExtVASurface> mfxRawFrames;

/*    struct mfxRecFrames
    {
        mfxMemId    mids[NUM_FRAMES];         
        mfxU16      indexes[NUM_FRAMES];    
        mfxU16      NumFrameActual;
    };

    struct mfxRawFrames
    {
        mfxMemId    mids[NUM_FRAMES];         
        mfxU16      NumFrameActual;
    };    
*/

#endif

#if defined(MFX_VA_WIN)
#define _NUM_STORED_FEEDBACKS 256
   class mfxFeedback
   {
   private:        
        ENCODE_QUERY_STATUS_PARAMS queryStatus[_NUM_STORED_FEEDBACKS];

   public:
       inline mfxFeedback()
       {
           Reset();
       }

       inline void Reset()
       {
            memset (queryStatus,0,sizeof(queryStatus));
       }

       inline bool isUpdateNeeded()
       {
           for (int i=0; i < _NUM_STORED_FEEDBACKS; i++)
           {
               if (queryStatus[i].StatusReportFeedbackNumber && queryStatus[i].bStatus != 1)
                   return false;           
           }
           return true;      
       }

       inline bool GetFeedback(UINT StatusReportFeedbackNumber, ENCODE_QUERY_STATUS_PARAMS& feedback)
       {
           /*for (int i=0; i < _NUM_STORED_FEEDBACKS; i++)
           {
               if (queryStatus[i].StatusReportFeedbackNumber)
               {
                   printf("_q %d\n", queryStatus[i].StatusReportFeedbackNumber);
               }
           }
           */
           for (int i=0; i < _NUM_STORED_FEEDBACKS; i++)
           {
               if (queryStatus[i].StatusReportFeedbackNumber == StatusReportFeedbackNumber)
               {
                   memcpy_s (&feedback, sizeof(ENCODE_QUERY_STATUS_PARAMS), &queryStatus[i], sizeof(ENCODE_QUERY_STATUS_PARAMS));
                   queryStatus[i].StatusReportFeedbackNumber = 0;
                   return true;
               }
           }
           return false;      
       }

       inline void* GetPointer()
       {
            //printf("Update \n");
            return  (void*)queryStatus;
       }

       inline int GetSize()
       {
            return  sizeof(queryStatus);
       }
   };

#undef _NUM_STORED_FEEDBACKS
#else

    typedef std::vector<ExtVASurface> mfxFeedback;

#endif

    mfxStatus QueryHwCaps(VideoCORE* core,
        ENCODE_CAPS & hwCaps);

}; // namespace MfxHwMpeg2Encode

#endif
#endif
#endif
/* EOF */
