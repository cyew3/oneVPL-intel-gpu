//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _ENCODER_VC1_PLANES_H_
#define _ENCODER_VC1_PLANES_H_

#include "ippvc.h"
#include "umc_vc1_enc_def.h"
#include "umc_structures.h"
#include "umc_memory_allocator.h"
#include "umc_vc1_enc_common.h"

#include "new.h"

namespace UMC_VC1_ENCODER
{
    //|--------------|//
    //|frame         |//
    //|  |-------|   |//
    //|  | plane |   |//
    //|  |       |   |//
    //|  |-------|   |//
    //|--------------|//

    inline void ReplicateBorderChroma_YV12 (Ipp8u* pU,Ipp8u* pV, Ipp32u stepUV,IppiSize srcRoiSizeUV, IppiSize dstRoiSizeUV, 
                                            int topBorderWidth,  int leftBorderWidth)
    {
        _own_ippiReplicateBorder_8u_C1R  (pU, stepUV, srcRoiSizeUV,dstRoiSizeUV,topBorderWidth,leftBorderWidth);
        _own_ippiReplicateBorder_8u_C1R  (pV, stepUV, srcRoiSizeUV,dstRoiSizeUV,topBorderWidth,leftBorderWidth);
    }

    inline void ReplicateBorderChroma_NV12 (Ipp8u* pUV,Ipp8u* /*pV*/, Ipp32u stepUV,IppiSize srcRoiSizeUV, IppiSize dstRoiSizeUV, 
                                     int topBorderWidth,  int leftBorderWidth)
    {
        _own_ippiReplicateBorder_16u_C1R  ((Ipp16u*)pUV, stepUV, srcRoiSizeUV,dstRoiSizeUV,topBorderWidth,leftBorderWidth);
    }

    class Frame
    {
    public:
        
        Frame ():
           m_pYFrame(0),
           m_pUFrame(0),
           m_pVFrame(0),
           m_pYPlane(0),
           m_pUPlane(0),
           m_pVPlane(0),
           m_stepY (0),
           m_stepUV (0),
           m_widthYPlane (0),
           m_widthUVPlane (0),
           m_heightYPlane(0),
           m_heightUVPlane(0),
           m_paddingSize(0),
           m_pictureType(VC1_ENC_I_FRAME),
           m_bTaken(false),
           m_UDataBuffer (0),
           m_UDataBufferSize(0),
           m_UDataSize(0)
           {
               
           };
           virtual ~Frame ()
           {
                Close ();
           }
           static Ipp32u CalcAllocatedMemSize(Ipp32u w, Ipp32u h, Ipp32u paddingSize, bool bUData = false);
           virtual UMC::Status Init(Ipp8u* pBuffer, Ipp32s memSize, Ipp32s w, Ipp32s h, Ipp32u paddingSize, bool bUData = false);

           UMC::Status Init(    Ipp8u* pYPlane, Ipp32u stepY,
                                Ipp8u* pUPlane, Ipp8u* pVPlane, Ipp32u stepUV,
                                Ipp32s Width, Ipp32s WidthUV,
                                Ipp32s Height,Ipp32s HeightUV,
                                Ipp32u paddingSize,
                                ePType pictureType,
                                Ipp8u* pUDataBuffer=0,
                                Ipp32u UDataBufferSize = 0)
           {
                m_pYFrame = 0;
                m_pUFrame = 0;
                m_pVFrame = 0;
                m_pYPlane = pYPlane;
                m_pUPlane = pUPlane;
                m_pVPlane = pVPlane;
                m_stepY   = stepY;
                m_stepUV  = stepUV;
                m_widthYPlane = Width;
                m_widthUVPlane= WidthUV;
                m_heightYPlane= Height;
                m_heightUVPlane=HeightUV;
                m_paddingSize=paddingSize;
                m_pictureType=pictureType;
                m_bTaken= true;
                m_UDataBuffer = pUDataBuffer;
                m_UDataBufferSize = UDataBufferSize;
                m_UDataSize = 0;
                return UMC::UMC_OK;
           }



           void        Close();

           virtual UMC::Status PadFrameProgressive()
           {

               IppiSize srcRoiSizeY   = {m_widthYPlane ,m_heightYPlane};
               IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};
               IppiSize dstRoiSizeY   = {(((srcRoiSizeY.width  + 15)>>4)<<4) + (m_paddingSize<<1),(((srcRoiSizeY.height  + 15)>>4)<<4)+ (m_paddingSize<<1) };
               IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1,dstRoiSizeY.height>>1};

               _own_ippiReplicateBorder_8u_C1R  (m_pYPlane, m_stepY, srcRoiSizeY,dstRoiSizeY,m_paddingSize,m_paddingSize);
               ReplicateBorderChroma_YV12(m_pUPlane,m_pVPlane, m_stepUV, srcRoiSizeUV,dstRoiSizeUV,m_paddingSize>>1,m_paddingSize>>1);

               return UMC::UMC_OK;
           }

           virtual UMC::Status PadFrameField()
           {

               IppiSize srcRoiSizeY   = {m_widthYPlane ,m_heightYPlane>>1};
               IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};
               IppiSize dstRoiSizeY   = {(((srcRoiSizeY.width  + 15)>>4)<<4) + (m_paddingSize<<1),(((srcRoiSizeY.height  + 15)>>4)<<4)+ (m_paddingSize) };
               IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1,dstRoiSizeY.height>>1};

               _own_ippiReplicateBorder_8u_C1R  (m_pYPlane,            m_stepY<<1, srcRoiSizeY,dstRoiSizeY,m_paddingSize>>1,m_paddingSize);
               _own_ippiReplicateBorder_8u_C1R  (m_pYPlane + m_stepY,  m_stepY<<1, srcRoiSizeY,dstRoiSizeY,m_paddingSize>>1,m_paddingSize);


               ReplicateBorderChroma_YV12(m_pUPlane,m_pVPlane, m_stepUV<<1, srcRoiSizeUV,dstRoiSizeUV,m_paddingSize>>2,m_paddingSize>>1);
               ReplicateBorderChroma_YV12(m_pUPlane + m_stepUV,m_pVPlane + m_stepUV, m_stepUV<<1, srcRoiSizeUV,dstRoiSizeUV,m_paddingSize>>2,m_paddingSize>>1);

               return UMC::UMC_OK;

           }

           virtual UMC::Status PadField(bool bBottomField)
           {
               IppiSize srcRoiSizeY   = {m_widthYPlane ,m_heightYPlane>>1};
               IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};
               IppiSize dstRoiSizeY   = {(((srcRoiSizeY.width  + 15)>>4)<<4) + (m_paddingSize<<1),(((srcRoiSizeY.height  + 15)>>4)<<4)+ (m_paddingSize) };
               IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1,dstRoiSizeY.height>>1};

               if (!bBottomField)
               {
                   _own_ippiReplicateBorder_8u_C1R  (m_pYPlane,            m_stepY<<1, srcRoiSizeY,dstRoiSizeY,m_paddingSize>>1,m_paddingSize);
                   ReplicateBorderChroma_YV12(m_pUPlane,m_pVPlane, m_stepUV<<1, srcRoiSizeUV,dstRoiSizeUV,m_paddingSize>>2,m_paddingSize>>1);

               }
               else
               {
                   _own_ippiReplicateBorder_8u_C1R  (m_pYPlane + m_stepY,  m_stepY<<1, srcRoiSizeY,dstRoiSizeY,m_paddingSize>>1,m_paddingSize);
                   ReplicateBorderChroma_YV12(m_pUPlane + m_stepUV,m_pVPlane + m_stepUV, m_stepUV<<1, srcRoiSizeUV,dstRoiSizeUV,m_paddingSize>>2,m_paddingSize>>1);

               }
               return UMC::UMC_OK;


           }

           virtual UMC::Status PadPlaneProgressive()
           {

               IppiSize srcRoiSizeY   = {m_widthYPlane ,m_heightYPlane};
               IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};
               IppiSize dstRoiSizeY   = {(((srcRoiSizeY.width  + 15)>>4)<<4),(((srcRoiSizeY.height  + 15)>>4)<<4)};
               IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1,dstRoiSizeY.height>>1};


               _own_ippiReplicateBorder_8u_C1R  (m_pYPlane, m_stepY, srcRoiSizeY,dstRoiSizeY,0,0);
               ReplicateBorderChroma_YV12(m_pUPlane,m_pVPlane, m_stepUV, srcRoiSizeUV,dstRoiSizeUV,0,0);

               return UMC::UMC_OK;
           }

           virtual UMC::Status PadPlaneField()
           {
               IppiSize srcRoiSizeY   = {m_widthYPlane ,m_heightYPlane>>1};
               IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};
               IppiSize dstRoiSizeY   = {(((srcRoiSizeY.width  + 15)>>4)<<4),(((srcRoiSizeY.height  + 15)>>4)<<4) };
               IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1,dstRoiSizeY.height>>1};


               _own_ippiReplicateBorder_8u_C1R  (m_pYPlane,            m_stepY<<1, srcRoiSizeY,dstRoiSizeY,0,0);
               _own_ippiReplicateBorder_8u_C1R  (m_pYPlane + m_stepY,  m_stepY<<1, srcRoiSizeY,dstRoiSizeY,0,0);

               ReplicateBorderChroma_YV12(m_pUPlane,m_pVPlane, m_stepUV<<1, srcRoiSizeUV,dstRoiSizeUV,0,0);
               ReplicateBorderChroma_YV12(m_pUPlane + m_stepUV,m_pVPlane + m_stepUV, m_stepUV<<1, srcRoiSizeUV,dstRoiSizeUV,0,0);

               return UMC::UMC_OK;
           }

           virtual  UMC::Status CopyPlane ( Ipp8u* pYPlane, Ipp32u stepY,
                                            Ipp8u* pUPlane, Ipp32u stepU,
                                            Ipp8u* pVPlane, Ipp32u stepV,
                                            ePType pictureType=VC1_ENC_I_FRAME);
           virtual UMC::Status CopyPlane (Frame * fr);

           inline void SetType ( ePType pictureType)
           {
                m_pictureType = pictureType;
                m_bTaken      = true;
           };
           inline ePType GetType ( )
           {
                return m_pictureType;
           };

           inline Ipp8u* GetYPlane() {return m_pYPlane;}
           inline Ipp8u* GetUPlane() {return m_pUPlane;}
           inline Ipp8u* GetVPlane() {return m_pVPlane;}
           inline Ipp32u GetYStep()  {return m_stepY;}
           inline Ipp32u GetUStep()  {return m_stepUV;}
           inline Ipp32u GetVStep()  {return m_stepUV;}
           inline void GetPlane_Prog ( Ipp8u *& pY, Ipp32u& YStep,
                                       Ipp8u *& pU, Ipp8u*& pV, 
                                       Ipp32u&  UVStep)
           {
                pY       = m_pYPlane;
                pU       = m_pUPlane;
                pV       = m_pVPlane;
                YStep    = m_stepY;
                UVStep   = m_stepUV;
           }
           inline void GetPlane_Field ( bool     bBottom,
                                        Ipp8u *& pY, Ipp32u& YStep,
                                        Ipp8u *& pU, Ipp8u*& pV, 
                                        Ipp32u&  UVStep)
           {
               pY       = m_pYPlane + ((bBottom)? m_stepY:0);
               pU       = m_pUPlane + ((bBottom)? m_stepUV:0);
               pV       = m_pVPlane + ((bBottom)? m_stepUV:0);
               YStep    = m_stepY << 1;
               UVStep   = m_stepUV << 1;
           }


           inline Ipp32u GetPaddingSize()
           {
                return m_paddingSize;           
           }
           inline void GetPictureSizeLuma(IppiSize *pSize)
           {
                pSize->height = m_heightYPlane;
                pSize->width  = m_widthYPlane;
           }
           inline void GetPictureSizeChroma(IppiSize *pSize)
           {
                pSize->height = m_heightUVPlane;
                pSize->width  = m_widthUVPlane;
           }
           inline ePType GetPictureType()
           {
               return m_pictureType;
           }
           inline bool   isTaken()
           {
              return m_bTaken;
           }

           inline void ReleasePlane ()
           {
                m_bTaken = false;
           }

           inline void SetReferenceFrameType()
           {
               m_pictureType = VC1_ENC_I_FRAME;
           }
           inline bool isReferenceFrame()
           {
               return (m_pictureType == VC1_ENC_I_FRAME ||
                       m_pictureType == VC1_ENC_P_FRAME  ||
                       m_pictureType == VC1_ENC_SKIP_FRAME ||
                       m_pictureType == VC1_ENC_I_I_FIELD ||
                       m_pictureType == VC1_ENC_P_I_FIELD ||
                       m_pictureType == VC1_ENC_I_P_FIELD ||
                       m_pictureType == VC1_ENC_P_P_FIELD );
           }
           inline bool isIntraFrame()
           {
               return (m_pictureType == VC1_ENC_I_FRAME ||
                       m_pictureType == VC1_ENC_I_I_FIELD);
           }

           inline bool isInterlace()
           {
               return (m_pictureType == VC1_ENC_I_I_FIELD ||
                       m_pictureType == VC1_ENC_P_I_FIELD ||
                       m_pictureType == VC1_ENC_I_P_FIELD ||
                       m_pictureType == VC1_ENC_P_P_FIELD ||
                       m_pictureType == VC1_ENC_B_B_FIELD ||
                       m_pictureType == VC1_ENC_BI_B_FIELD ||
                       m_pictureType == VC1_ENC_B_BI_FIELD ||
                       m_pictureType == VC1_ENC_BI_BI_FIELD);
           }
           inline bool SetUserData(Ipp8u* pBuf, Ipp32u size)
           {
               if (m_UDataBuffer!=0 && size < m_UDataBufferSize && size !=0)
               {
                   MFX_INTERNAL_CPY(m_UDataBuffer,pBuf,size);
                   m_UDataSize = size ;
                   return true;
               }
               m_UDataSize = 0;
               return false;
           }
           inline void ResetUserData()
           {
                m_UDataSize = 0;
           }
           inline Ipp32u GetUserDataSize ()
           {
               return m_UDataSize;
           }
           inline Ipp8u* GetUserData ()
           {
               return m_UDataBuffer;
           }


    protected:

        Ipp8u* m_pYFrame;
        Ipp8u* m_pUFrame;
        Ipp8u* m_pVFrame;

        Ipp32u m_stepY;
        Ipp32u m_stepUV;

        Ipp8u* m_pYPlane;
        Ipp8u* m_pUPlane;
        Ipp8u* m_pVPlane;
        Ipp8u* m_UDataBuffer;
        Ipp32u m_UDataBufferSize;
        Ipp32u m_UDataSize;

        Ipp32s m_widthYPlane;
        Ipp32s m_widthUVPlane;

        Ipp32s m_heightYPlane;
        Ipp32s m_heightUVPlane;

        Ipp32u m_paddingSize;

        ePType m_pictureType;
        bool   m_bTaken;

    };
    class FrameNV12:public Frame
    {
    public:

          virtual UMC::Status Init(Ipp8u* pBuffer, Ipp32s memSize, Ipp32s w, Ipp32s h, Ipp32u paddingSize, bool bUData = false);
          virtual  UMC::Status CopyPlane (  Ipp8u* pYPlane, Ipp32u stepY,
                                            Ipp8u* pUPlane, Ipp32u stepU,
                                            Ipp8u* pVPlane, Ipp32u stepV,
                                            ePType pictureType=VC1_ENC_I_FRAME);
          virtual UMC::Status CopyPlane (Frame * fr);

          virtual UMC::Status PadFrameProgressive()
          {

              IppiSize srcRoiSizeY   = {m_widthYPlane ,m_heightYPlane};
              IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};
              IppiSize dstRoiSizeY   = {(((srcRoiSizeY.width  + 15)>>4)<<4) + (m_paddingSize<<1),(((srcRoiSizeY.height  + 15)>>4)<<4)+ (m_paddingSize<<1) };
              IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1,dstRoiSizeY.height>>1};

              _own_ippiReplicateBorder_8u_C1R  (m_pYPlane, m_stepY, srcRoiSizeY,dstRoiSizeY,m_paddingSize,m_paddingSize);
              ReplicateBorderChroma_NV12(m_pUPlane,m_pVPlane, m_stepUV, srcRoiSizeUV,dstRoiSizeUV,m_paddingSize>>1,m_paddingSize>>1);

              return UMC::UMC_OK;
          }

          virtual UMC::Status PadFrameField()
          {

              IppiSize srcRoiSizeY   = {m_widthYPlane ,m_heightYPlane>>1};
              IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};
              IppiSize dstRoiSizeY   = {(((srcRoiSizeY.width  + 15)>>4)<<4) + (m_paddingSize<<1),(((srcRoiSizeY.height  + 15)>>4)<<4)+ (m_paddingSize) };
              IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1,dstRoiSizeY.height>>1};

              _own_ippiReplicateBorder_8u_C1R  (m_pYPlane,            m_stepY<<1, srcRoiSizeY,dstRoiSizeY,m_paddingSize>>1,m_paddingSize);
              _own_ippiReplicateBorder_8u_C1R  (m_pYPlane + m_stepY,  m_stepY<<1, srcRoiSizeY,dstRoiSizeY,m_paddingSize>>1,m_paddingSize);


              ReplicateBorderChroma_NV12(m_pUPlane,m_pVPlane, m_stepUV<<1, srcRoiSizeUV,dstRoiSizeUV,m_paddingSize>>2,m_paddingSize>>1);
              ReplicateBorderChroma_NV12(m_pUPlane + m_stepUV,m_pVPlane + m_stepUV, m_stepUV<<1, srcRoiSizeUV,dstRoiSizeUV,m_paddingSize>>2,m_paddingSize>>1);

              return UMC::UMC_OK;

          }

          virtual UMC::Status PadField(bool bBottomField)
          {
              IppiSize srcRoiSizeY   = {m_widthYPlane ,m_heightYPlane>>1};
              IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};
              IppiSize dstRoiSizeY   = {(((srcRoiSizeY.width  + 15)>>4)<<4) + (m_paddingSize<<1),(((srcRoiSizeY.height  + 15)>>4)<<4)+ (m_paddingSize) };
              IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1,dstRoiSizeY.height>>1};

              if (!bBottomField)
              {
                  _own_ippiReplicateBorder_8u_C1R  (m_pYPlane,            m_stepY<<1, srcRoiSizeY,dstRoiSizeY,m_paddingSize>>1,m_paddingSize);
                  ReplicateBorderChroma_NV12(m_pUPlane,m_pVPlane, m_stepUV<<1, srcRoiSizeUV,dstRoiSizeUV,m_paddingSize>>2,m_paddingSize>>1);

              }
              else
              {
                  _own_ippiReplicateBorder_8u_C1R  (m_pYPlane + m_stepY,  m_stepY<<1, srcRoiSizeY,dstRoiSizeY,m_paddingSize>>1,m_paddingSize);
                  ReplicateBorderChroma_NV12(m_pUPlane + m_stepUV,m_pVPlane + m_stepUV, m_stepUV<<1, srcRoiSizeUV,dstRoiSizeUV,m_paddingSize>>2,m_paddingSize>>1);

              }
              return UMC::UMC_OK;


          }

          virtual UMC::Status PadPlaneProgressive()
          {

              IppiSize srcRoiSizeY   = {m_widthYPlane ,m_heightYPlane};
              IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};
              IppiSize dstRoiSizeY   = {(((srcRoiSizeY.width  + 15)>>4)<<4),(((srcRoiSizeY.height  + 15)>>4)<<4)};
              IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1,dstRoiSizeY.height>>1};


              _own_ippiReplicateBorder_8u_C1R  (m_pYPlane, m_stepY, srcRoiSizeY,dstRoiSizeY,0,0);
              ReplicateBorderChroma_NV12(m_pUPlane,m_pVPlane, m_stepUV, srcRoiSizeUV,dstRoiSizeUV,0,0);

              return UMC::UMC_OK;
          }

          virtual UMC::Status PadPlaneField()
          {
              IppiSize srcRoiSizeY   = {m_widthYPlane ,m_heightYPlane>>1};
              IppiSize srcRoiSizeUV  = {srcRoiSizeY.width>>1,srcRoiSizeY.height>>1};
              IppiSize dstRoiSizeY   = {(((srcRoiSizeY.width  + 15)>>4)<<4),(((srcRoiSizeY.height  + 15)>>4)<<4) };
              IppiSize dstRoiSizeUV  = {dstRoiSizeY.width>>1,dstRoiSizeY.height>>1};


              _own_ippiReplicateBorder_8u_C1R  (m_pYPlane,            m_stepY<<1, srcRoiSizeY,dstRoiSizeY,0,0);
              _own_ippiReplicateBorder_8u_C1R  (m_pYPlane + m_stepY,  m_stepY<<1, srcRoiSizeY,dstRoiSizeY,0,0);

              ReplicateBorderChroma_NV12(m_pUPlane,m_pVPlane, m_stepUV<<1, srcRoiSizeUV,dstRoiSizeUV,0,0);
              ReplicateBorderChroma_NV12(m_pUPlane + m_stepUV,m_pVPlane + m_stepUV, m_stepUV<<1, srcRoiSizeUV,dstRoiSizeUV,0,0);

              return UMC::UMC_OK;
          }
    };
    class StoredFrames
    {
    public:
        StoredFrames():
            m_nFrames(0),
            m_pFrames(0)
            {};

        static Ipp32u CalcAllocatedMemSize(Ipp32u nFrames, Ipp32u w, Ipp32u h, Ipp32u paddingSize,bool bNV12, bool bUserData = false)
        {
            Ipp32u memSize = 0;
            if (bNV12)
            {
                memSize += UMC::align_value<Ipp32u>(nFrames*sizeof(FrameNV12));
                memSize += nFrames*UMC::align_value<Ipp32u>(FrameNV12::CalcAllocatedMemSize(w, h, paddingSize, bUserData));
            }
            else
            {
                memSize += UMC::align_value<Ipp32u>(nFrames*sizeof(Frame));
                memSize += nFrames*UMC::align_value<Ipp32u>(Frame::CalcAllocatedMemSize(w, h, paddingSize, bUserData));
            }

            return memSize;
        }

        UMC::Status Init(Ipp8u* pBuffer, Ipp32u memSize, Ipp32u nFrames, Ipp32u w, Ipp32u h, Ipp32u paddingSize, bool bNV12,bool bUserData = false)
        {
            Ipp32u      i           = 0;
            UMC::Status ret         = UMC::UMC_OK;
            Ipp32u      frameSize   = 0;

            Close();

            if(!pBuffer)
                return UMC::UMC_ERR_NULL_PTR;

            if(memSize == 0)
                return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

            m_nFrames = nFrames; // two reference frames
            if (bNV12)
            {
                m_pFrames = new (pBuffer) FrameNV12[m_nFrames];
                if (!m_pFrames)
                    return UMC::UMC_ERR_ALLOC;

                pBuffer += UMC::align_value<Ipp32u>(m_nFrames*sizeof(FrameNV12));
                memSize -= UMC::align_value<Ipp32u>(m_nFrames*sizeof(FrameNV12));
                if(memSize < 0)
                    return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

                frameSize = UMC::align_value<Ipp32u>(FrameNV12::CalcAllocatedMemSize(w,h,paddingSize,bUserData));
            }
            else
            {
                m_pFrames = new (pBuffer) Frame[m_nFrames];
                if (!m_pFrames)
                    return UMC::UMC_ERR_ALLOC;

                pBuffer += UMC::align_value<Ipp32u>(m_nFrames*sizeof(Frame));
                memSize -= UMC::align_value<Ipp32u>(m_nFrames*sizeof(Frame));
                if(memSize < 0)
                    return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

                frameSize = UMC::align_value<Ipp32u>(Frame::CalcAllocatedMemSize(w,h,paddingSize,bUserData));

            }
            for (i=0;i<m_nFrames;i++)
            {
                ret = m_pFrames[i].Init(pBuffer, memSize, w,h,paddingSize,bUserData);
                if (ret != UMC::UMC_OK)
                    return ret;
                pBuffer += frameSize;
                memSize -= frameSize;
                if(memSize < 0)
                    return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
            }
            return ret;
        }
        void        Close()
        {
            m_nFrames = 0;
        }

        ~StoredFrames()
        {
            Close();
        }

        void Reset()
        {
            Ipp32u i;
            if (m_pFrames)
                for (i = 0; i < m_nFrames; i++)
                {
                        m_pFrames[i].ReleasePlane();
                }
        }

        inline Frame* GetFreeFramePointer ()
        {
            Ipp32u i;
            for (i = 0; i < m_nFrames; i++)
            {
                if (!m_pFrames[i].isTaken())
                {
                    return &m_pFrames[i];
                }
            }
            return 0;
        }
        inline  Ipp32u      GetYStep()
        {
            return m_pFrames[0].GetYStep();

        }

    private:
        Ipp32u  m_nFrames;
        Frame  *m_pFrames;
        bool   *m_pTaken;
    };

    class GOP
    {
    public:
        GOP():
          m_maxB(0),
          m_pInFrames(0),
          m_numBuffB(0),
          m_iCurr(0),
          m_pOutFrames(0)
          {};

        virtual ~GOP()
        {
            Close();
        }
        inline bool isSkipFrame(Frame* inFrame, bool bOriginal, Ipp32u th=0)
        {
            bool bSkipSupported = 0; /* Skip Frames not supported now */

            if (!bSkipSupported)
              return false;            

            Frame* refFrame = (bOriginal)? m_pInFrames[0]: m_pOutFrames[0] ;
            IppiSize Size = {0};
            if ((m_numBuffB !=  0) || (refFrame==0) || (th==0))
               return false;

            inFrame->GetPictureSizeLuma(&Size);
            if (!MaxAbsDiff(inFrame->GetYPlane(),inFrame->GetYStep(),
                       refFrame->GetYPlane(),refFrame->GetYStep(),Size,th))
            {
                return false;
            }

            inFrame->GetPictureSizeChroma(&Size);
            if (!MaxAbsDiff(inFrame->GetUPlane(),inFrame->GetUStep(),
                       refFrame->GetUPlane(),refFrame->GetUStep(),Size,th))
            {
                return false;
            }
            if (!MaxAbsDiff(inFrame->GetVPlane(),inFrame->GetVStep(),
                       refFrame->GetVPlane(),refFrame->GetVStep(),Size,th))
            {
                return false;
            }
            return true;
        }

        inline Ipp32u GetNumberOfB()
        {
            return m_numBuffB;
        }
    protected:
        Ipp32u  m_maxB;
        Frame** m_pInFrames; /* 0     - forward  reference frame,
                                1     - backward reference frame
                                other - B frames*/
        Frame** m_pOutFrames;
        Ipp32u  m_numBuffB;
        Ipp32u  m_iCurr;

    protected:
        void Close()
        {
            if (m_pInFrames)
            {
                delete [] m_pInFrames;
                m_pInFrames = 0;
            }
            if (m_pOutFrames)
            {
               delete [] m_pOutFrames;
               m_pOutFrames = 0;
            }
            m_iCurr = 0;
            m_numBuffB = 0;
            m_maxB = 0;
        }

        inline bool NewGOP ()
        {

            m_pInFrames[0] ->ReleasePlane();
            m_pOutFrames[0]->ReleasePlane();

            m_pInFrames[0]   = m_pInFrames[1];
            m_pOutFrames[0]  = m_pOutFrames[1];

            m_pInFrames[1]   = 0;
            m_pOutFrames[1]  = 0;


            m_iCurr = 1;
            m_numBuffB = 0;
            return true;
        }

        inline virtual bool AddBFrame(Frame* inFrame,Frame* outFrame)
        {
            if (m_numBuffB < m_maxB &&
                m_pInFrames[0] != 0  && m_pInFrames[1] ==0&&
                m_pOutFrames[0] != 0 && m_pOutFrames[1]==0)
            {
                m_pInFrames[m_numBuffB+2]  = inFrame;
                m_pOutFrames[m_numBuffB+2] = outFrame;
                m_numBuffB ++;
                return true;
            }
            return false;
        }
        inline virtual bool AddReferenceFrame(Frame* inFrame,Frame* outFrame)
        {
            if (m_pInFrames[0] == 0 && m_pOutFrames[0] == 0)
            {
                m_pInFrames[0]  = inFrame;
                m_pOutFrames[0] = outFrame;
                return true;
            }
            else if (m_pInFrames[1] == 0 && m_pOutFrames[1] == 0)
            {
                m_pInFrames[1]  = inFrame;
                m_pOutFrames[1] = outFrame;
                return true;
            }
            return false;
        }

    public:

        inline bool AddFrame(Frame* inFrame, Frame* outFrame=0)
        {
            if (outFrame)
            {
                if (inFrame->isReferenceFrame())
                {
                    outFrame->SetType(inFrame->GetType());
                    return AddReferenceFrame(inFrame,outFrame);
                }
                else
                {
                    return AddBFrame(inFrame,inFrame);
                }
            }
            else
            {
                if (inFrame->isReferenceFrame())
                {
                    return AddReferenceFrame(inFrame,inFrame);
                }
                else
                {
                    return AddBFrame(inFrame,inFrame);
                }

            }
        }
        inline virtual UMC::Status Init(Ipp32u maxB)
        {
            Close();
            m_maxB = maxB;
            m_pInFrames = new Frame* [m_maxB+2];
            if (!m_pInFrames)
                return UMC::UMC_ERR_ALLOC;
            m_pOutFrames =  new Frame* [m_maxB+2];
            if (!m_pOutFrames)
                return UMC::UMC_ERR_ALLOC;
            Reset();
            return UMC::UMC_OK;
        }

        inline virtual void Reset()
        {
            if (m_pInFrames)
            {
                memset(m_pInFrames,0,sizeof(Frame*)*(m_maxB+2));
            }
            if (m_pOutFrames)
            {
                memset(m_pOutFrames,0,sizeof(Frame*)*(m_maxB+2));
            }
            m_iCurr = 0;
            m_numBuffB = 0;
        };

        inline Frame* GetInFrameForDecoding()
        {
            return m_pInFrames[m_iCurr];
        }
        inline Frame* GetOutFrameForDecoding()
        {
            return m_pOutFrames[m_iCurr];
        }
        inline  virtual void ReleaseCurrentFrame()
        {
            if (!m_pInFrames[m_iCurr] || !m_pOutFrames[m_iCurr])
                return;

            if (m_iCurr>1)
            {
                m_pInFrames[m_iCurr]->ReleasePlane();
                m_pOutFrames[m_iCurr]->ReleasePlane();
                m_pInFrames[m_iCurr] = 0;
                m_pOutFrames[m_iCurr] = 0;
            }
            m_iCurr++;
            if (m_iCurr>=m_numBuffB+2)
            {
                NewGOP();
            }
        }
        inline  virtual void CloseGop(ePType pPictureType=VC1_ENC_I_FRAME)
        {
            if (m_pInFrames[1]==0 && m_numBuffB>0)
            {
                m_pInFrames[1]            = m_pInFrames[2+m_numBuffB-1];
                m_pOutFrames[1]           = m_pOutFrames[2+m_numBuffB-1];
                m_pInFrames[2 + m_numBuffB-1] = 0;
                m_numBuffB --;
                m_pInFrames[1]->SetType(pPictureType);
            }
        }
        inline Frame* GetInReferenceFrame(bool bBackward = false)
        {
            return m_pInFrames[bBackward];
        }
        inline Frame* GetOutReferenceFrame(bool bBackward = false)
        {
            return m_pOutFrames[bBackward];
        }
        inline virtual ePType GetPictureType(Ipp32u frameCount,Ipp32u GOPLength, Ipp32u BFrmLength)
        {
            Ipp32s      nFrameInGOP        =  (frameCount++) % GOPLength;

            if (nFrameInGOP)
            {
                if ( nFrameInGOP %(BFrmLength+1)==0)
                    return VC1_ENC_P_FRAME;
                else
                    return VC1_ENC_B_FRAME;
            }
            return VC1_ENC_I_FRAME;
        }


    };
    class GOPWithoutReordening : public GOP
    {
    public:

        inline virtual UMC::Status Init(Ipp32u maxB)
        {
            return GOP::Init(maxB>0);
        }

    protected:
        inline virtual bool AddBFrame(Frame* inFrame, Frame* outFrame)
        {
            if (m_numBuffB < m_maxB &&
                m_pInFrames[0] != 0 && m_pInFrames[1]!=0 &&
                m_pOutFrames[0] != 0 && m_pOutFrames[1]!=0)
            {
                m_pInFrames[m_numBuffB+2]  = inFrame;
                m_pOutFrames[m_numBuffB+2] = outFrame;
                return true;
            }
            return false;
        }
        inline virtual bool AddReferenceFrame(Frame* inFrame, Frame* outFrame)
        {
            if (!GOP::AddReferenceFrame(inFrame,outFrame))
            {
                NewGOP();
                return GOP::AddReferenceFrame(inFrame, outFrame);
            }
            return true;
        }
    public:
        inline virtual ePType GetPictureType(Ipp32u frameCount,Ipp32u GOPLength, Ipp32u BFrmLength)
        {
            Ipp32s      nFrameInGOP        =  (frameCount) % GOPLength;

            if (nFrameInGOP)
            {
                if ( (nFrameInGOP-1) %(BFrmLength+2)==0)
                    return VC1_ENC_P_FRAME;
                else
                    return VC1_ENC_B_FRAME;
            }
            return VC1_ENC_I_FRAME;
        }



        inline virtual void ReleaseCurrentFrame()
        {
            if (!m_pInFrames[m_iCurr] || !m_pOutFrames[m_iCurr])
                return;
            if (m_iCurr>1)
            {
                m_pInFrames[m_iCurr]->ReleasePlane();
                m_pOutFrames[m_iCurr]->ReleasePlane();
                m_pInFrames[m_iCurr] = 0;
                m_pOutFrames[m_iCurr] = 0;
            }
            else
            {
                m_iCurr++;
            }

        }
        inline virtual void CloseGop()
        {
            return;
        }
     };

    class WaitingList
    {
    public:
        WaitingList():
            m_maxN(0),
            m_pFrame(0),
            m_curIndex(0),
            m_nFrames(0)
            {};
        ~WaitingList()
        {Close();}
        void Close()
        {
            if (m_pFrame)
            {
                delete m_pFrame;
                m_pFrame = 0;
            }
            m_maxN = 0;
            m_curIndex = 0;
            m_nFrames = 0;
        }

        UMC::Status Init(Ipp32u maxB)
        {
            Close();
            m_maxN = maxB+2;
            m_pFrame = new Frame* [ m_maxN];
            if (!m_pFrame)
                return UMC::UMC_ERR_ALLOC;
            memset (m_pFrame,0, sizeof(Frame*)* m_maxN);
            return UMC::UMC_OK;
        }

        void Reset()
        {
            Ipp32u i = 0;
            if(m_pFrame)
                memset (m_pFrame,0, sizeof(Frame*)* m_maxN);
            m_curIndex = 0;
            m_nFrames = 0;
            if(m_pFrame)
                for(i = 0; i < m_maxN; i++)
                {
                    if(m_pFrame[i])
                        m_pFrame[i]->ReleasePlane();
                }
        }

        bool AddFrame(Frame* frame)
        {
            if (m_nFrames < m_maxN)
            {
                Ipp32u ind = (m_curIndex + m_nFrames)%m_maxN;
                m_pFrame[ind] = frame;
                m_nFrames ++;
                return true;
            }
            return false;
        }
        Frame* GetCurrFrame()
        {
            Frame* frm = 0;
            if (m_nFrames>0)
            {
                frm = m_pFrame[m_curIndex];
            }
            return frm;
        }
        bool MoveOnNextFrame()
        {
            if (m_nFrames>0)
            {
                m_pFrame[m_curIndex] = 0;
                m_curIndex = (++m_curIndex)%m_maxN;
                m_nFrames--;
                return true;
            }
            return false;
        }

    private:
        Ipp32u  m_maxN;
        Frame** m_pFrame;
        Ipp32u  m_curIndex;
        Ipp32u  m_nFrames;

    };

    class BufferedFrames
    {

    public:
        BufferedFrames():
            m_pFrames(0),       //sequence of frames
            m_bufferSize (0),   //number of frames in sequence
            m_nBuffered (0),
            m_currFrameIndex (0),
            m_bClosed (false) // closed sequence (if the backward reference frame is present)
            {};

         virtual ~BufferedFrames()
         {
            Close();
         }
         static Ipp32u CalcAllocatedMemSize(Ipp32u w, Ipp32u h, Ipp32u paddingSize, Ipp32u n);
         UMC::Status    Init  (Ipp8u* pBuffer, Ipp32u memSize,
                               Ipp32u w, Ipp32u h, Ipp32u paddingSize, Ipp32u n);
         void           Close();
         UMC::Status    SaveFrame           (Ipp8u* pYPlane, Ipp32u stepY,
                                            Ipp8u* pUPlane, Ipp32u stepU,
                                            Ipp8u* pVPlane, Ipp32u stepV );

         UMC::Status    GetFrame            (Frame** currFrame);
         UMC::Status    GetReferenceFrame   (Frame** currFrame);

        UMC::Status     ReferenceFrame();

         inline bool isClosedSequence ()
         {
            return m_bClosed;
         }
         inline bool isBuffered()
         {
            return (m_nBuffered!=0);
         }
    private:
        Frame* m_pFrames;
        Ipp32u m_bufferSize;
        Ipp32u m_nBuffered;
        Ipp32u m_currFrameIndex;
        bool   m_bClosed; // closed sequence (if the backward reference frame is present)
    };
}
#endif
#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
