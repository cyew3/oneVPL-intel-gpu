//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#ifndef __UMC_VC1_DEC_FRAME_DESCR_VA_H_
#define __UMC_VC1_DEC_FRAME_DESCR_VA_H_

#include "umc_va_base.h"

#if defined(UMC_VA)
#include "umc_vc1_dec_frame_descr.h"
#include "umc_vc1_dec_va_defs.h"
#include "umc_vc1_dec_exception.h"
#include "umc_vc1_dec_task_store.h"

#ifdef UMC_VA_DXVA
#include "umc_va_dxva2_protected.h"
#endif

//#define PRINT_VA_DEBUG
namespace UMC
{

//#define bit_get(value, offset, count)      ( (value & (((1<<count)-1) << offset)) >> offset);

#if defined (UMC_VA_LINUX)
    class VC1PackerLVA
    {
    public:
        VC1PackerLVA():m_va(NULL),
                       m_pSliceInfo(NULL),
                       m_pPicPtr(NULL),
                       m_bIsPreviousSkip(false)
         {
        };
        ~VC1PackerLVA(){}
        static void VC1PackPicParams (VC1Context* pContext,
                                      VAPictureParameterBufferVC1* ptr,
                                      VideoAccelerator*              va);
        void VC1PackPicParamsForOneSlice(VC1Context* pContext)
        {
            VC1PackPicParams(pContext,m_pPicPtr,m_va);
            if (VC1_IS_REFERENCE(pContext->m_picLayerHeader->PTYPE))
                m_bIsPreviousSkip = false;
        }

        void VC1PackOneSlice                      (VC1Context* pContext,
                                                   SliceParams* slparams,
                                                   Ipp32u SliceBufIndex,
                                                   Ipp32u MBOffset,
                                                   Ipp32u SliceDataSize,
                                                   Ipp32u StartCodeOffset,
                                                   Ipp32u ChoppingType);//compatibility with Windows code

        void  VC1PackWholeSliceSM                   (VC1Context* pContext,
                                                     Ipp32u MBOffset,
                                                     Ipp32u SliceDataSize);
        void   VC1PackBitStreamForOneSlice          (VC1Context* pContext, Ipp32u Size);

        Ipp32u   VC1PackBitStream             (VC1Context* pContext,
                                                Ipp32u& Size,
                                                Ipp8u* pOriginalData,
                                                Ipp32u OriginalSize,
                                                Ipp32u ByteOffset,
                                                Ipp8u& Flag_03)
        {
            //if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
                return VC1PackBitStreamAdv(pContext, Size, pOriginalData, OriginalSize, ByteOffset, Flag_03);
            //else
            //    return 0;
        }

         Ipp32u VC1PackBitStreamAdv (VC1Context* pContext, Ipp32u& Size,Ipp8u* pOriginalData,
                                   Ipp32u OriginalDataSize, Ipp32u ByteOffset, Ipp8u& Flag_03);

        void   VC1PackBitplaneBuffers               (VC1Context* pContext);

        void   VC1SetSliceBuffer(){};
        void   VC1SetSliceParamBuffer(Ipp32u* pOffsets, Ipp32u* pValues);
        void   VC1SetSliceDataBuffer(Ipp32s size);
        void   VC1SetBitplaneBuffer(Ipp32s size);
        void   VC1SetPictureBuffer();

        void   VC1SetBuffersSize                    (Ipp32u SliceBufIndex, Ipp32u PicBuffIndex);
        void   VC1SetBuffersSize() {};

        void   SetVideoAccelerator                  (VideoAccelerator*  va)
        {
            if (va)
                m_va = va;
        };
        void MarkFrameAsSkip() {m_bIsPreviousSkip = true;}
        bool IsPrevFrameIsSkipped() {return m_bIsPreviousSkip;}

        Ipp32u VC1GetPicHeaderSize(Ipp8u* pOriginalData, size_t Offset, Ipp8u& Flag_03)
        {
            Ipp32u i = 0;
            Ipp32u numZero = 0;
            Ipp8u* ptr = pOriginalData;
            for(i = 0; i < Offset; i++)
            {
                if(*ptr == 0)
                    numZero++;
                else
                    numZero = 0;

                ptr++;

                if((numZero) == 2 && (*ptr == 0x03))
                {
                    if(*(ptr+1) < 4)
                    {
                       ptr++;
                    }
                    numZero = 0;
                }
            }

            if((numZero == 1) && (*ptr == 0) && (*(ptr+1) == 0x03) && (*(ptr+2) < 4))
            {
                Flag_03 = 1;

                if((*(ptr+2) == 0) && (*(ptr+3) == 0) && (*(ptr+4) == 0x03) && (*(ptr+5) < 4))
                    Flag_03 = 5;

                return ((Ipp32u)(ptr - pOriginalData) + 1);
            }
            else if((*ptr == 0) && (*(ptr+1) == 0) && (*(ptr+2) == 0x03)&& (*(ptr+3) < 4))
            {
                Flag_03 = 2;
                return (Ipp32u)(ptr - pOriginalData);
            }
            else if((*(ptr + 1) == 0) && (*(ptr+2) == 0) && (*(ptr+3) == 0x03)&& (*(ptr+4) < 4)&& (*(ptr+5) > 3))
            {
                Flag_03 = 3;
                return (Ipp32u)(ptr - pOriginalData);
            }
            else if((*(ptr + 2) == 0) && (*(ptr+3) == 0) && (*(ptr+4) == 0x03)&& (*(ptr+5) < 4))
            {
                Flag_03 = 4;
                return (Ipp32u)(ptr - pOriginalData);
            }
            else
            {
                Flag_03 = 0;
                return (Ipp32u)(ptr - pOriginalData);
            }

        }

    private:

        // we sure about our types
        template <class T, class T1>
        static T bit_set(T value, Ipp32u offset, Ipp32u count, T1 in)
        {
            return (T)(value | (((1<<count) - 1)&in) << offset);
        };

        VideoAccelerator*              m_va;
        VASliceParameterBufferVC1*     m_pSliceInfo;
        VAPictureParameterBufferVC1*   m_pPicPtr;
        bool m_bIsPreviousSkip;
    };
    class VC1UnPackerLVA
    {
    public:
        VC1UnPackerLVA():m_va(NULL),
                         m_pSliceInfo(NULL),
                         m_pPicPtr(NULL)
        {
        }
        ~VC1UnPackerLVA(){};
        void         VC1UnPackPicParams                   (VC1Context* pContext, Ipp8u picIndex) {};
        void         VC1UnPackBitplaneBuffers             (VC1Context* pContext) {};
        Ipp32u       VC1UnPackOneSlice                    (VC1Context* pContext, SliceParams* slparams) {return 0;} ;
        Ipp8u*       VC1GetBitsream                       (){return 0;};
        Ipp32u       VC1GetNumOfSlices                    (){return 0;};
        void         VC1SetSliceBuffer                    (){};
        void         SetVideoAccelerator                  (VideoAccelerator*  va)
        {
            if (va)
                m_va = va;
        };
     private:
        // we sure about our types
        template <class T>
        T bit_get(T value, Ipp32u offset, Ipp32u count)
        {
            return (T)( (value & (((1<<count)-1) << offset)) >> offset);
        };
        VideoAccelerator*              m_va;
        VASliceParameterBufferVC1*     m_pSliceInfo;
        VAPictureParameterBufferVC1*   m_pPicPtr;
    };
#endif


#if defined (UMC_VA_DXVA)
    class VC1PackerDXVA
    {
    public:
        VC1PackerDXVA():m_va(NULL),
                        m_pSliceInfo(NULL),
                        m_pPicPtr(NULL),
                        m_bIsPreviousSkip(false)
        {
        };
        ~VC1PackerDXVA(){}
        void   VC1Init(){}; //no need to implement yet


        // main function for packing picture parameters based on DXVA2 interface
        void VC1PackPicParams (VC1Context* pContext,
                                      DXVA_PictureParameters* ptr,
                                      VideoAccelerator*              va);
        void VC1PackPicParamsForOneSlice(VC1Context* pContext)
        {
            //UMCVACompBuffer* CompBuf;
            //m_pPicPtr = (DXVA_PictureParameters*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &CompBuf,sizeof(DXVA_PictureParameters));
            // _MAY_BE need to check sizes
            VC1PackPicParams(pContext,m_pPicPtr,m_va);
            if (VC1_IS_REFERENCE(pContext->m_picLayerHeader->PTYPE))
                m_bIsPreviousSkip = false;
            //if (m_bIsPreviousSkip)
            //    m_pPicPtr->wBackwardRefPictureIndex = m_pPicPtr->wForwardRefPictureIndex;
        }

        void VC1PackOneSlice                      (VC1Context* pContext,
                                                   SliceParams* slparams,
                                                   Ipp32u SliceBufIndex,
                                                   Ipp32u Offset,
                                                   Ipp32u SliceDataSize,
                                                   Ipp32u StartCodeOffset,
                                                   Ipp32u ChoppingType);

        void  VC1PackWholeSliceSM                   (VC1Context* pContext,
                                                     Ipp32u MBOffset,
                                                     Ipp32u SliceDataSize);

        void   VC1PackBitStreamForOneSlice          (VC1Context* pContext, Ipp32u Size);
        //return remain unpacked bitstream data
        Ipp32u   VC1PackBitStream                     (VC1Context* pContext,
                                                       Ipp32u Size,
                                                       Ipp8u* pOriginalData,
                                                       Ipp32u ByteOffset,
                                                       Ipp32u isNeedToAddSC,
                                                       Ipp8u& Flag_03)
        {
            if (VC1_PROFILE_ADVANCED != pContext->m_seqLayerHeader.PROFILE)
                return VC1PackBitStreamSM(pContext, Size, pOriginalData, ByteOffset, false);
            else
                return VC1PackBitStreamAdv(pContext, Size, pOriginalData, ByteOffset);
        }

        void   VC1PackBitplaneBuffers               (VC1Context* pContext);

        void   VC1SetSliceBuffer();
        void   VC1SetPictureBuffer();

        void   VC1SetExtPictureBuffer() {};
        void   VC1SetBuffersSize                    (Ipp32u SliceBufIndex,Ipp32u PicBuffIndex);
        //void   VC1SetBuffersSize();

        void   SetVideoAccelerator                  (VideoAccelerator*  va)
        {
            if (va)
                m_va = va;
        };
        // we sure about our types
        template <class T, class T1>
        static T bit_set(T value, Ipp32u offset, Ipp32u count, T1 in)
        {
            return (T)(value | (((1<<count) - 1)&in) << offset);
        };
        void MarkFrameAsSkip() {m_bIsPreviousSkip = true;}
        bool IsPrevFrameIsSkipped() {return m_bIsPreviousSkip;}

        Ipp32u VC1GetPicHeaderSize(Ipp8u* pOriginalData, Ipp32u Offset, Ipp8u& Flag_03)
        {
            // compatibility only
            return 0;
        }

    protected:

       Ipp32u VC1PackBitStreamSM (VC1Context* pContext,
                                              Ipp32u Size,
                                              Ipp8u* pOriginalData,
                                              Ipp32u ByteOffset,
                                              bool   isNeedToAddSC = false);

       Ipp32u VC1PackBitStreamAdv (VC1Context* pContext,
                                   Ipp32u Size,
                                   Ipp8u* pOriginalData,
                                   Ipp32u ByteOffset);
        void VC1PackPicParams (VC1Context* pContext)
        {
            VC1PackPicParams(pContext,m_pPicPtr,m_va);
            ++m_pPicPtr;
        }

        VideoAccelerator*     m_va;
        DXVA_SliceInfo*    m_pSliceInfo;
        DXVA_PictureParameters*   m_pPicPtr;
        bool m_bIsPreviousSkip;
    };

    class VC1PackerDXVA_EagleLake: public VC1PackerDXVA
    {
        public:
        VC1PackerDXVA_EagleLake(): VC1PackerDXVA(){m_pExtPicInfo = NULL;}
        ~VC1PackerDXVA_EagleLake(){m_pExtPicInfo = NULL;};

        virtual void VC1PackExtPicParams (VC1Context* pContext,
                                      DXVA_ExtPicInfo* ptr,
                                      VideoAccelerator* va);

        virtual void VC1PackPicParams (VC1Context* pContext,
                                      DXVA_PictureParameters* ptr,
                                      VideoAccelerator*       va);


        virtual void VC1PackPicParamsForOneSlice(VC1Context* pContext)
        {
            VC1PackPicParams(pContext,m_pPicPtr,m_va);
            VC1PackExtPicParams(pContext,m_pExtPicInfo,m_va);

            if (VC1_IS_REFERENCE(pContext->m_picLayerHeader->PTYPE))
                m_bIsPreviousSkip = false;
            //if (m_bIsPreviousSkip)
            //    m_pPicPtr->wBackwardRefPictureIndex = m_pPicPtr->wForwardRefPictureIndex;
        }

        virtual void VC1PackerDXVA_EagleLake::VC1PackOneSlice  (VC1Context* pContext,
                                          SliceParams* slparams,
                                          Ipp32u BufIndex, // only in future realisations
                                          Ipp32u MBOffset,
                                          Ipp32u SliceDataSize,
                                          Ipp32u StartCodeOffset,
                                          Ipp32u ChoppingType);

        virtual void   VC1SetExtPictureBuffer();
        virtual void   VC1SetBuffersSize                    (Ipp32u SliceBufIndex,Ipp32u PicBuffIndex);
        Ipp32u VC1GetPicHeaderSize(Ipp8u* pOriginalData, size_t Offset, Ipp8u& Flag_03)
        {
            Ipp32u i = 0;
            Ipp32u numZero = 0;
            Ipp8u* ptr = pOriginalData;
            for(i = 0; i < Offset; i++)
            {
                if(*ptr == 0)
                    numZero++;
                else
                    numZero = 0;

                ptr++;

                if((numZero) == 2 && (*ptr == 0x03))
                {
                    if(*(ptr+1) < 4)
                    {
                       ptr++;
                    }
                    numZero = 0;
                }
            }

            if((numZero == 1) && (*ptr == 0) && (*(ptr+1) == 0x03) && (*(ptr+2) < 4))
            {
                Flag_03 = 1;

                if((*(ptr+2) == 0) && (*(ptr+3) == 0) && (*(ptr+4) == 0x03) && (*(ptr+5) < 4))
                    Flag_03 = 5;

                return ((Ipp32u)(ptr - pOriginalData) + 1);
            }
            else if((*ptr == 0) && (*(ptr+1) == 0) && (*(ptr+2) == 0x03)&& (*(ptr+3) < 4))
            {
                Flag_03 = 2;
                return (Ipp32u)(ptr - pOriginalData);
            }
            else if((*(ptr + 1) == 0) && (*(ptr+2) == 0) && (*(ptr+3) == 0x03)&& (*(ptr+4) < 4)&& (*(ptr+5) > 3))
            {
                Flag_03 = 3;
                return (Ipp32u)(ptr - pOriginalData);
            }
            else if((*(ptr + 2) == 0) && (*(ptr+3) == 0) && (*(ptr+4) == 0x03)&& (*(ptr+5) < 4))
            {
                Flag_03 = 4;
                return (Ipp32u)(ptr - pOriginalData);
            }
            else
            {
                Flag_03 = 0;
                return (Ipp32u)(ptr - pOriginalData);
            }

        }

        virtual Ipp32u   VC1PackBitStream             (VC1Context* pContext,
                                                       Ipp32u& Size,
                                                       Ipp8u* pOriginalData,
                                                       Ipp32u OriginalSize,
                                                       Ipp32u ByteOffset,
                                                       Ipp8u& Flag_03)
        {
            //if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
                return VC1PackBitStreamAdv(pContext, Size, pOriginalData, OriginalSize, ByteOffset, Flag_03);
            //else
            //    return 0;
        }

       virtual Ipp32u VC1PackBitStreamAdv (VC1Context* pContext, Ipp32u& Size,Ipp8u* pOriginalData,
                                   Ipp32u OriginalDataSize, Ipp32u ByteOffset, Ipp8u& Flag_03);

        virtual void VC1PackBitplaneBuffers(VC1Context* pContext);

    protected:
        void VC1PackExtPicParams (VC1Context* pContext)
        {
            VC1PackExtPicParams(pContext,m_pExtPicInfo,m_va);
            ++m_pExtPicInfo;
        }

        DXVA_ExtPicInfo*   m_pExtPicInfo;
    };


    class VC1PackerDXVA_Protected: public VC1PackerDXVA
    {
    public:
        VC1PackerDXVA_Protected(): VC1PackerDXVA(){m_pExtPicInfo = NULL;}
        ~VC1PackerDXVA_Protected(){m_pExtPicInfo = NULL;};

        virtual void VC1PackExtPicParams (VC1Context* pContext,
            DXVA_ExtPicInfo* ptr,
            VideoAccelerator* va);

        virtual void VC1PackPicParams (VC1Context* pContext,
            DXVA_PictureParameters* ptr,
            VideoAccelerator*       va);


        virtual void VC1PackPicParamsForOneSlice(VC1Context* pContext)
        {
            VC1PackPicParams(pContext,m_pPicPtr,m_va);
            VC1PackExtPicParams(pContext,m_pExtPicInfo,m_va);

            if (VC1_IS_REFERENCE(pContext->m_picLayerHeader->PTYPE))
                m_bIsPreviousSkip = false;
            /*if (m_bIsPreviousSkip)
                m_pPicPtr->wBackwardRefPictureIndex = m_pPicPtr->wForwardRefPictureIndex;*/
        }

        virtual void VC1PackerDXVA_Protected::VC1PackOneSlice  (VC1Context* pContext,
            SliceParams* slparams,
            Ipp32u BufIndex, // only in future realisations
            Ipp32u MBOffset,
            Ipp32u SliceDataSize,
            Ipp32u StartCodeOffset,
            Ipp32u ChoppingType);

        virtual void   VC1SetExtPictureBuffer();
        virtual void   VC1SetBuffersSize                    (Ipp32u SliceBufIndex,Ipp32u PicBuffIndex);

         virtual Ipp32u   VC1PackBitStream(VC1Context* pContext,
                                                       Ipp32u& Size,
                                                       Ipp8u* pOriginalData,
                                                       Ipp32u OriginalSize,
                                                       Ipp32u ByteOffset,
                                                       Ipp8u& Flag_03)
        {
            if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader.PROFILE)
                return VC1PackBitStreamAdv(pContext, Size, pOriginalData, OriginalSize, ByteOffset, Flag_03);
            else
                return 0;
                }
        Ipp32u VC1PackerDXVA_Protected::VC1PackBitStreamAdv (VC1Context* pContext,
                                                       Ipp32u& Size,
                                                       Ipp8u* pOriginalData,
                                                       Ipp32u OriginalSize,
                                                       Ipp32u ByteOffset,
                                                       Ipp8u& Flag_03);

        virtual void VC1PackBitplaneBuffers(VC1Context* pContext);

        Ipp32u VC1GetPicHeaderSize(Ipp8u* pOriginalData, size_t Offset, Ipp8u& Flag_03)
        {
            Ipp32u i = 0;
            Ipp32u numZero = 0;
            Ipp8u* ptr = pOriginalData;
            for(i = 0; i < Offset; i++)
            {
                if(*ptr == 0)
                    numZero++;
                else
                    numZero = 0;

                ptr++;

                if((numZero) == 2 && (*ptr == 0x03))
                {
                    if(*(ptr+1) < 4)
                    {
                       ptr++;
                    }
                    numZero = 0;
                }
            }

            if((numZero == 1) && (*ptr == 0) && (*(ptr+1) == 0x03) && (*(ptr+2) < 4))
            {
                Flag_03 = 1;

                if((*(ptr+2) == 0) && (*(ptr+3) == 0) && (*(ptr+4) == 0x03) && (*(ptr+5) < 4))
                    Flag_03 = 5;

                return ((Ipp32u)(ptr - pOriginalData) + 1);
            }
            else if((*ptr == 0) && (*(ptr+1) == 0) && (*(ptr+2) == 0x03)&& (*(ptr+3) < 4))
            {
                Flag_03 = 2;
                return (Ipp32u)(ptr - pOriginalData);
            }
            else if((*(ptr + 1) == 0) && (*(ptr+2) == 0) && (*(ptr+3) == 0x03)&& (*(ptr+4) < 4)&& (*(ptr+5) > 3))
            {
                Flag_03 = 3;
                return (Ipp32u)(ptr - pOriginalData);
            }
            else if((*(ptr + 2) == 0) && (*(ptr+3) == 0) && (*(ptr+4) == 0x03)&& (*(ptr+5) < 4))
            {
                Flag_03 = 4;
                return (Ipp32u)(ptr - pOriginalData);
            }
            else
            {
                Flag_03 = 0;
                return (Ipp32u)(ptr - pOriginalData);
            }
        }

    protected:
        void VC1PackExtPicParams (VC1Context* pContext)
        {
            VC1PackExtPicParams(pContext,m_pExtPicInfo,m_va);
            ++m_pExtPicInfo;
        }

        DXVA_ExtPicInfo*   m_pExtPicInfo;
    };
#endif

    template <class T>
    class VC1FrameDescriptorVA: public VC1FrameDescriptor
    {
        friend class VC1TaskStore;
        friend class VC1VideoDecoder;
    public:

        // Default constructor
        VC1FrameDescriptorVA(MemoryAllocator*      pMemoryAllocator,
                             VideoAccelerator*     va):VC1FrameDescriptor(pMemoryAllocator),
                                                       m_va(va),
                                                       m_pBitstream(NULL),
                                                       m_iSliceBufIndex(0),
                                                       m_iPicBufIndex(0),
                                                       m_bIsItarativeMode(false),
                                                       m_pBufferStart(NULL)
        {
//#ifdef UMC_VA_LINUX
            //m_bIsItarativeMode = true;
//#endif

            m_bBframeDelay = true;
        }

        virtual bool Init                        (Ipp32u         DescriporID,
                                                  VC1Context*    pContext,
                                                  VC1TaskStore*  pStore,
                                                  bool IsReoreder = true,
                                                  Ipp16s*        pResidBuf = 0)
        {
            VC1SequenceLayerHeader* seqLayerHeader = &pContext->m_seqLayerHeader;
            m_pStore = pStore;
            m_pPacker.SetVideoAccelerator(m_va);

            //need to think about DBL profile
            if (m_va->m_Profile == VC1_VLD)
            {
                Ipp32u buffSize =  (seqLayerHeader->heightMB*VC1_PIXEL_IN_LUMA + 128)*
                    (seqLayerHeader->widthMB*VC1_PIXEL_IN_LUMA + 128)+
                    ((seqLayerHeader->heightMB*VC1_PIXEL_IN_CHROMA + 64)*
                    (seqLayerHeader->widthMB*VC1_PIXEL_IN_CHROMA + 64))*2;


                if(m_pContext == NULL)
                {
                    if (m_pMemoryAllocator->Alloc(&m_ipContextID,
                                                    sizeof(VC1Context),
                                                    UMC_ALLOC_PERSISTENT,
                                                    16) != UMC_OK)
                                                    return false;
                    m_pContext = (VC1Context*)(m_pMemoryAllocator->Lock(m_ipContextID));
                    if(m_pContext==NULL)
                    {
                        Release();
                        return false;
                    }
                }
                //m_pContext = (VC1Context*)ippsMalloc_8u(sizeof(VC1Context));
                //if(!m_pContext)
                //    return false;
                memset(m_pContext, 0, sizeof(VC1Context));


                //buf size should be divisible by 4
                if(buffSize & 0x00000003)
                    buffSize = (buffSize&0xFFFFFFFC) + 4;


                if(m_pContext->m_pBufferStart == NULL)
                {
                    if (m_pMemoryAllocator->Alloc(&m_ipBufferStartID,
                                                    buffSize*sizeof(Ipp8u),
                                                    UMC_ALLOC_PERSISTENT,
                                                    16) != UMC_OK)
                                                    return false;
                    m_pContext->m_pBufferStart = (Ipp8u*)(m_pMemoryAllocator->Lock(m_ipBufferStartID));
                    if(m_pContext->m_pBufferStart==NULL)
                    {
                        Release();
                        return false;
                    }
                }
                //m_pContext->m_pBufferStart = (Ipp8u*)ippsMalloc_8u(buffSize*sizeof(Ipp8u));
                //if(m_pContext->m_pBufferStart==NULL)
                //{
                //    return false;
                //}
                memset(m_pContext->m_pBufferStart, 0, buffSize);

                m_pContext->m_vlcTbl = pContext->m_vlcTbl;
                m_pContext->pRefDist = &pContext->RefDist;

                m_pContext->m_frmBuff.m_pFrames = pContext->m_frmBuff.m_pFrames;


                m_pContext->m_frmBuff.m_iDisplayIndex =  -1;
                m_pContext->m_frmBuff.m_iCurrIndex    =  -1;
                m_pContext->m_frmBuff.m_iPrevIndex    =  -1;
                m_pContext->m_frmBuff.m_iNextIndex    =  -1;
                m_pContext->m_frmBuff.m_iToFreeIndex = -1;
                m_pContext->m_frmBuff.m_iRangeMapIndex   =  pContext->m_frmBuff.m_iRangeMapIndex;

                m_pContext->m_seqLayerHeader = pContext->m_seqLayerHeader;

                if(m_pContext->m_picLayerHeader == NULL)
                {
                    if (m_pMemoryAllocator->Alloc(&m_iPicHeaderID,
                                                    sizeof(VC1PictureLayerHeader)*VC1_MAX_SLICE_NUM,
                                                    UMC_ALLOC_PERSISTENT,
                                                    16) != UMC_OK)
                                                    return false;
                    m_pContext->m_picLayerHeader = (VC1PictureLayerHeader*)(m_pMemoryAllocator->Lock(m_iPicHeaderID));
                    if(m_pContext->m_picLayerHeader==NULL)
                    {
                        Release();
                        return false;
                    }
                }

                m_pContext->m_InitPicLayer = m_pContext->m_picLayerHeader;

                memset(m_pContext->m_picLayerHeader, 0, sizeof(VC1PictureLayerHeader)*VC1_MAX_SLICE_NUM);

                //Bitplane pool
                if(m_pContext->m_pBitplane.m_databits == NULL)
                {
                    if (m_pMemoryAllocator->Alloc(&m_iBitplaneID,
                                                    sizeof(Ipp8u)*seqLayerHeader->heightMB*
                                                    seqLayerHeader->widthMB*
                                                    VC1_MAX_BITPANE_CHUNCKS,
                                                    UMC_ALLOC_PERSISTENT,
                                                    16) != UMC_OK)
                                                    return false;
                    m_pContext->m_pBitplane.m_databits = (Ipp8u*)(m_pMemoryAllocator->Lock(m_iBitplaneID));
                    if(m_pContext->m_pBitplane.m_databits==NULL)
                    {
                        Release();
                        return false;
                    }
                }

                memset(m_pContext->m_pBitplane.m_databits, 0,sizeof(Ipp8u)*seqLayerHeader->heightMB*
                    seqLayerHeader->widthMB*
                    VC1_MAX_BITPANE_CHUNCKS);

                m_iSelfID = DescriporID;
            }
            return true;
        }

        virtual void Release()
        {
            if (m_va->m_Profile == VC1_VLD)
            {
                if(m_pMemoryAllocator)
                {
                    if (m_iPicHeaderID != (MemID)-1)
                    {
                        m_pMemoryAllocator->Unlock(m_iPicHeaderID);
                        m_pMemoryAllocator->Free(m_iPicHeaderID);
                        m_iPicHeaderID = (MemID)-1;
                    }
                    m_pContext->m_InitPicLayer = NULL;

                    if (m_iBitplaneID != (MemID)-1)
                    {
                        m_pMemoryAllocator->Unlock(m_iBitplaneID);
                        m_pMemoryAllocator->Free(m_iBitplaneID);
                        m_iBitplaneID = (MemID)-1;
                    }
                    m_pContext->m_pBitplane.m_databits = NULL;

                    if (m_ipBufferStartID != (MemID)-1)
                    {
                        m_pMemoryAllocator->Unlock(m_ipBufferStartID);
                        m_pMemoryAllocator->Free(m_ipBufferStartID);
                        m_ipBufferStartID = (MemID)-1;
                    }
                    m_pContext->m_pBufferStart = NULL;

                    if (m_ipContextID != (MemID)-1)
                    {
                        m_pMemoryAllocator->Unlock(m_ipContextID);
                        m_pMemoryAllocator->Free(m_ipContextID);
                        m_ipContextID = (MemID)-1;
                    }
                    m_pContext = NULL;

                }
            }
        }
        void VC1PackSlices    (Ipp32u*  pOffsets,
                               Ipp32u*  pValues,
                               Ipp8u*   pOriginalData,
                               MediaDataEx::_MediaDataEx* pOrigStCodes)

        {
            Ipp32u PicHeaderFlag;
            Ipp32u* pPictHeader = m_pContext->m_bitstream.pBitstream;
            Ipp32u temp_value = 0;
            Ipp32u* bitstream;
            Ipp32s bitoffset = 31;
            Ipp32u SliceSize = 0;

            Ipp32u* pOriginalOffsets = pOrigStCodes->offsets;

            Ipp32u StartCodeOffset = 0;

            // need in case of fields
            Ipp32u* pFirstFieldStartCode = m_pContext->m_bitstream.pBitstream;
            Ipp32u Offset;

            Ipp32u RemBytesInSlice = 0;

            bool isSecondField = false;
            m_iSliceBufIndex = 0;
            m_iPicBufIndex = 0;

            Ipp32u MBEndRowOfAlreadyExec = 0;

            SliceParams slparams;
            Ipp8u  Flag_03 = 0;

            memset(&slparams,0,sizeof(SliceParams));
            slparams.MBStartRow = 0;
            slparams.MBEndRow = m_pContext->m_seqLayerHeader.heightMB;
            m_pContext->m_picLayerHeader->CurrField = 0;
            if (m_pContext->m_picLayerHeader->PTYPE == VC1_SKIPPED_FRAME)
            {
                m_bIsSkippedFrame = true;
                m_bIsReadyToProcess = false;
                return;
            }
            else
            {
                m_bIsSkippedFrame = false;
            }

            if (VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE)
            {

                //skip start codes till Frame Header
                while ((*pValues)&&(*pValues != 0x0D010000))
                {
                    ++pOffsets;
                    ++pValues;
                }
                DecodePicHeader(m_pContext);
            }
            else
            {
                Decode_PictureLayer(m_pContext);
            }

            m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);


            if (m_pContext->m_seqLayerHeader.PROFILE != VC1_PROFILE_ADVANCED)
            {
                Offset = (Ipp32u) (sizeof(Ipp32u)*(m_pContext->m_bitstream.pBitstream - m_pBufferStart)); // offset in words
                SliceSize = m_pContext->m_FrameSize - VC1FHSIZE;

                {
                    Ipp32u chopType = 1;
                    Ipp32u BytesAlreadyExecuted = 0;
                    RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                        SliceSize,
                        pOriginalData,
                        0,
                        false, Flag_03);
                    slparams.m_bitOffset = m_pContext->m_bitstream.bitOffset;
                    if (RemBytesInSlice)
                    {
                        Ipp32u ByteFilledInSlice = SliceSize - RemBytesInSlice;
                        MBEndRowOfAlreadyExec = slparams.MBEndRow;

                        // fill slices
                        while (RemBytesInSlice) // we don't have enough buffer - execute it
                        {
                            m_pPacker.VC1PackOneSlice(m_pContext,
                                                     &slparams,
                                                     m_iSliceBufIndex,
                                                     Offset,
                                                     ByteFilledInSlice,
                                                     StartCodeOffset,
                                                     chopType);

                            m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                            m_va->Execute();
                            BytesAlreadyExecuted += ByteFilledInSlice;

                            m_pPacker.VC1SetSliceBuffer();
                            m_pPacker.VC1SetPictureBuffer();
                            StartCodeOffset = 0;
                            m_iSliceBufIndex = 0;

                            m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                            RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                RemBytesInSlice,
                                pOriginalData + BytesAlreadyExecuted,
                                StartCodeOffset,
                                false, Flag_03);
                            ByteFilledInSlice = SliceSize - BytesAlreadyExecuted - RemBytesInSlice;
                            chopType = 3;
                            slparams.m_bitOffset = 0;
                            Offset = 0;
                        }

                        m_pPacker.VC1PackOneSlice(m_pContext,
                            &slparams,
                            m_iSliceBufIndex,
                            Offset,
                            SliceSize - BytesAlreadyExecuted,
                            StartCodeOffset,
                            2);

                        m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                        m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                        if (UMC_OK != m_va->Execute())
                            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                        StartCodeOffset = 0;
                        m_iSliceBufIndex = 0;


                    }
                    else
                    {
                        m_pPacker.VC1PackOneSlice(m_pContext,
                            &slparams,
                            m_iSliceBufIndex,
                            Offset,
                            SliceSize,
                            StartCodeOffset,
                            Flag_03);
                        m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);
                        if (UMC_OK != m_va->Execute())
                            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);

                    }
                }
                return;
            }
            if (*(pValues+1) == 0x0B010000)
            {
                bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets+1));
                VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
                bitoffset = 31;
                VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);     //SLICE_ADDR
                m_pContext->m_picLayerHeader->is_slice = 1;
            }
            else if(*(pValues+1) == 0x0C010000)
            {
                slparams.MBEndRow = (m_pContext->m_seqLayerHeader.heightMB+1)/2;
            }


            slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
            slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
            slparams.m_bitOffset = m_pContext->m_bitstream.bitOffset;
            Offset = (Ipp32u) (sizeof(Ipp32u)*(m_pContext->m_bitstream.pBitstream - pPictHeader)); // offset in bytes

            if (*(pOffsets+1))
                SliceSize = *(pOriginalOffsets+1) - *pOriginalOffsets;
            else
                SliceSize = m_pContext->m_FrameSize;

            {
                Ipp32u chopType = 1;
                Ipp32u BytesAlreadyExecuted = 0;
                RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                                              SliceSize,
                                                              pOriginalData,
                                                              StartCodeOffset,
                                                              true, Flag_03);
                if (RemBytesInSlice)
                {
                    Ipp32u ByteFilledInSlice = SliceSize - RemBytesInSlice;
                    MBEndRowOfAlreadyExec = slparams.MBEndRow;

                    // fill slices
                    while (RemBytesInSlice) // we don't have enough buffer - execute it
                    {
                        m_pPacker.VC1PackOneSlice(m_pContext,
                                                  &slparams,
                                                  m_iSliceBufIndex,
                                                  Offset,
                                                  ByteFilledInSlice,
                                                  StartCodeOffset,
                                                  chopType);

                        m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                        if (UMC_OK != m_va->Execute())
                            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                        BytesAlreadyExecuted += ByteFilledInSlice;

                        m_pPacker.VC1SetSliceBuffer();
                        m_pPacker.VC1SetPictureBuffer();
                        StartCodeOffset = 0;
                        m_iSliceBufIndex = 0;

                        m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                        RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                                                      RemBytesInSlice,
                                                                      pOriginalData + BytesAlreadyExecuted,
                                                                      StartCodeOffset,
                                                                      false, Flag_03);
                        ByteFilledInSlice = SliceSize - BytesAlreadyExecuted - RemBytesInSlice;
                        chopType = 3;
                        slparams.m_bitOffset = 0;
                        Offset = 0;
                    }

                    m_pPacker.VC1PackOneSlice(m_pContext,
                                             &slparams,
                                              m_iSliceBufIndex,
                                              Offset,
                                              SliceSize - BytesAlreadyExecuted,
                                              StartCodeOffset,
                                              2);

                    m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                    m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                    if (UMC_OK != m_va->Execute())
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                    StartCodeOffset = 0;
                    m_iSliceBufIndex = 0;

                    // we need receive buffers from drv for future processing
                    if (MBEndRowOfAlreadyExec != m_pContext->m_seqLayerHeader.heightMB)
                    {
                        m_pPacker.VC1SetSliceBuffer();
                        m_pPacker.VC1SetPictureBuffer();
                    }

                }
                else
                {
                    m_pPacker.VC1PackOneSlice(m_pContext,
                                              &slparams,
                                              m_iSliceBufIndex,
                                              Offset,
                                              SliceSize,
                                              StartCodeOffset,
                                              0);
                    // let execute in case of fields
                    if (*(pValues+1)== 0x0C010000)
                    {
                        // Execute first slice
                        if (m_va->m_Platform == VA_LINUX)
                            m_pPacker.VC1PackBitplaneBuffers(m_pContext);
                        m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex + 1,m_iPicBufIndex);

                        if (UMC_OK != m_va->Execute())
                            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                        StartCodeOffset = 0;
                        m_iSliceBufIndex = 0;
                    }
                    else
                    {
                        StartCodeOffset += SliceSize - RemBytesInSlice;
                        ++m_iSliceBufIndex;
                    }
                }
            }
            ++pOffsets;
            ++pValues;
            ++pOriginalOffsets;


            while (*pOffsets)
            {
                //Fields
                if (*(pValues) == 0x0C010000)
                {
                    // Execute first slice
                    if (m_va->m_Platform == VA_LINUX)
                        m_pPacker.VC1PackBitplaneBuffers(m_pContext);

                    m_pContext->m_bitstream.pBitstream = pFirstFieldStartCode;



                    if (UMC_OK != m_va->EndFrame())
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                    if (UMC_OK != m_va->BeginFrame(m_pContext->m_frmBuff.m_iCurrIndex))
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);


                    if (*(pOffsets+1))
                        SliceSize = *(pOriginalOffsets+1) - *pOriginalOffsets;
                    else
                        SliceSize = m_pContext->m_FrameSize - *pOriginalOffsets;

                    StartCodeOffset = 0;
                    m_pPacker.VC1SetSliceBuffer();
                    m_pPacker.VC1SetPictureBuffer();
                    //m_iSliceBufIndex = 0;
                    //m_iPicBufIndex = 0;
                    isSecondField = true;
                    // set swap bitstream for parsing
                    m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *pOffsets);
                    m_pContext->m_bitstream.pBitstream += 1; // skip start code
                    m_pContext->m_bitstream.bitOffset = 31;
                    pPictHeader = m_pContext->m_bitstream.pBitstream;

                    ++m_pContext->m_picLayerHeader;
                    *m_pContext->m_picLayerHeader = *m_pContext->m_InitPicLayer;
                    m_pContext->m_picLayerHeader->BottomField = (Ipp8u)m_pContext->m_InitPicLayer->TFF;
                    m_pContext->m_picLayerHeader->PTYPE = m_pContext->m_InitPicLayer->PTypeField2;
                    m_pContext->m_picLayerHeader->CurrField = 1;

                    m_pContext->m_picLayerHeader->is_slice = 0;
                    DecodePicHeader(m_pContext);
                    slparams.MBStartRow = (m_pContext->m_seqLayerHeader.heightMB+1)/2;

                    if (*(pOffsets+1) && *(pValues+1) == 0x0B010000)
                    {
                        bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets+1));
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
                        bitoffset = 31;
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);
                    } else
                        slparams.MBEndRow = m_pContext->m_seqLayerHeader.heightMB;

                    m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                    ++m_iPicBufIndex;
                    slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
                    slparams.m_bitOffset = m_pContext->m_bitstream.bitOffset;
                    Offset = (Ipp32u) (sizeof(Ipp32u)*(m_pContext->m_bitstream.pBitstream - pPictHeader)); // offset in bytes
                    slparams.MBStartRow = 0; //we should decode fields steb by steb

                    {
                        Ipp32u chopType = 1;
                        Ipp32u BytesAlreadyExecuted = 0;
                        RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                            SliceSize,
                            pOriginalData + *pOriginalOffsets,
                            StartCodeOffset,
                            false, Flag_03);
                        if (RemBytesInSlice)
                        {
                            Ipp32u ByteFilledInSlice = SliceSize - RemBytesInSlice;
                            MBEndRowOfAlreadyExec = slparams.MBEndRow;

                            // fill slices
                            while (RemBytesInSlice) // we don't have enough buffer - execute it
                            {
                                m_pPacker.VC1PackOneSlice(m_pContext,
                                    &slparams,
                                    m_iSliceBufIndex,
                                    Offset,
                                    ByteFilledInSlice,
                                    StartCodeOffset,
                                    chopType);

                                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                                m_va->Execute();
                                BytesAlreadyExecuted += ByteFilledInSlice;

                                m_pPacker.VC1SetSliceBuffer();
                                m_pPacker.VC1SetPictureBuffer();
                                StartCodeOffset = 0;
                                m_iSliceBufIndex = 0;

                                m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                                RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                    RemBytesInSlice,
                                    pOriginalData + BytesAlreadyExecuted,
                                    StartCodeOffset,
                                    false, Flag_03);
                                ByteFilledInSlice = SliceSize - BytesAlreadyExecuted - RemBytesInSlice;
                                chopType = 3;
                                slparams.m_bitOffset = 0;
                                Offset = 0;
                            }

                            m_pPacker.VC1PackOneSlice(m_pContext,
                                &slparams,
                                m_iSliceBufIndex,
                                Offset,
                                SliceSize - BytesAlreadyExecuted,
                                StartCodeOffset,
                                2);

                            m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                            m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                            if (UMC_OK != m_va->Execute())
                                throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                            StartCodeOffset = 0;
                            m_iSliceBufIndex = 0;

                            // we need receive buffers from drv for future processing
                            if (MBEndRowOfAlreadyExec != m_pContext->m_seqLayerHeader.heightMB)
                            {
                                m_pPacker.VC1SetSliceBuffer();
                                m_pPacker.VC1SetPictureBuffer();
                            }

                        }
                        else
                        {
                            m_pPacker.VC1PackOneSlice(m_pContext,
                                &slparams,
                                m_iSliceBufIndex,
                                Offset,
                                SliceSize,
                                StartCodeOffset,
                                0);
                            StartCodeOffset += SliceSize - RemBytesInSlice;
                            ++m_iSliceBufIndex;
                        }
                    }

                    ++pOffsets;
                    ++pValues;
                    ++pOriginalOffsets;
                }
                //Slices
                else if (*(pValues) == 0x0B010000)
                {
                    m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *pOffsets);
                    VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,32, temp_value);
                    m_pContext->m_bitstream.bitOffset = 31;
                    pPictHeader = m_pContext->m_bitstream.pBitstream;

                    VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,9, slparams.MBStartRow);     //SLICE_ADDR
                    VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,1, PicHeaderFlag);            //PIC_HEADER_FLAG

                    if (PicHeaderFlag == 1)                //PIC_HEADER_FLAG
                    {
                        ++m_pContext->m_picLayerHeader;
                        if (isSecondField)
                            m_pContext->m_picLayerHeader->CurrField = 1;
                        else
                            m_pContext->m_picLayerHeader->CurrField = 0;

                        DecodePictureHeader_Adv(m_pContext);
                        DecodePicHeader(m_pContext);
                        ++m_iPicBufIndex;
                    }
                    m_pContext->m_picLayerHeader->is_slice = 1;

                    if (*(pOffsets+1) && *(pValues+1) == 0x0B010000)
                    {
                        bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets+1));
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
                        bitoffset = 31;
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);
                    }
                    else if(*(pValues+1) == 0x0C010000)
                        slparams.MBEndRow = (m_pContext->m_seqLayerHeader.heightMB+1)/2;
                    else
                        slparams.MBEndRow = m_pContext->m_seqLayerHeader.heightMB;

                    slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
                    slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
                    slparams.m_bitOffset = m_pContext->m_bitstream.bitOffset;
                    Offset = (Ipp32u) (sizeof(Ipp32u)*(m_pContext->m_bitstream.pBitstream - pPictHeader)); // offset in words

                    if (isSecondField)
                        slparams.MBStartRow -= (m_pContext->m_seqLayerHeader.heightMB+1)/2;

                    if (*(pOffsets+1))
                        SliceSize = *(pOriginalOffsets+1) - *pOriginalOffsets;
                    else
                        //SliceSize = m_pContext->m_FrameSize - StartCodeOffset;
                        SliceSize = m_pContext->m_FrameSize - *pOriginalOffsets;

                    {
                        Ipp32u chopType = 1;
                        RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                                                      SliceSize,
                                                                      pOriginalData + *pOriginalOffsets,
                                                                      StartCodeOffset,
                                                                      false, Flag_03);
                        if (RemBytesInSlice)
                        {
                            Ipp32u ByteFilledInSlice = SliceSize - RemBytesInSlice;
                            MBEndRowOfAlreadyExec = slparams.MBEndRow;
                            Ipp32u BytesAlreadyExecuted = 0;

                            // fill slices
                            while (RemBytesInSlice) // we don't have enough buffer - execute it
                            {
                                m_pPacker.VC1PackOneSlice(m_pContext,
                                                          &slparams,
                                                          m_iSliceBufIndex,
                                                          Offset,
                                                          ByteFilledInSlice,
                                                          StartCodeOffset,
                                                          chopType);

                                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                                if (UMC_OK != m_va->Execute())
                                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                                BytesAlreadyExecuted += ByteFilledInSlice;

                                m_pPacker.VC1SetSliceBuffer();
                                m_pPacker.VC1SetPictureBuffer();

                                StartCodeOffset = 0;
                                m_iSliceBufIndex = 0;

                                m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                                RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                                                              RemBytesInSlice,
                                                                              pOriginalData + *pOriginalOffsets + BytesAlreadyExecuted,
                                                                              StartCodeOffset,
                                                                              false, Flag_03);

                                ByteFilledInSlice = SliceSize - BytesAlreadyExecuted - RemBytesInSlice;
                                chopType = 3;
                                slparams.m_bitOffset = 0;
                                Offset = 0;
                            }

                            m_pPacker.VC1PackOneSlice(m_pContext,
                                                     &slparams,
                                                      m_iSliceBufIndex,
                                                      Offset,
                                                      SliceSize - BytesAlreadyExecuted,
                                                      StartCodeOffset,
                                                      2);
                            m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);
                            m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                            if (UMC_OK != m_va->Execute())
                                throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);

                            StartCodeOffset = 0;
                            m_iSliceBufIndex = 0;

                            // we need receive buffers from drv for future processing
                            if (MBEndRowOfAlreadyExec != m_pContext->m_seqLayerHeader.heightMB)
                            {
                                m_pPacker.VC1SetSliceBuffer();
                                m_pPacker.VC1SetPictureBuffer();
                            }
                        }
                        else
                        {
                            m_pPacker.VC1PackOneSlice(m_pContext,
                                &slparams,
                                m_iSliceBufIndex,
                                Offset,
                                SliceSize,
                                StartCodeOffset,
                                0);
                            // let execute in case of fields
                            if (*(pValues+1)== 0x0C010000)
                            {
                                // Execute first slice
                                if (m_va->m_Platform == VA_LINUX)
                                    m_pPacker.VC1PackBitplaneBuffers(m_pContext);

                                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex + 1,m_iPicBufIndex);

                                if (UMC_OK != m_va->Execute())
                                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                                StartCodeOffset = 0;
                                m_iSliceBufIndex = 0;
                            }
                            else
                            {
                                StartCodeOffset += SliceSize - RemBytesInSlice;
                                ++m_iSliceBufIndex;
                            }
                        }
                    }

                    ++pOffsets;
                    ++pValues;
                    ++pOriginalOffsets;

                }
                else
                {
                    ++pOffsets;
                    ++pValues;
                    ++pOriginalOffsets;
                }

            }
            //need to execute last slice
            if (MBEndRowOfAlreadyExec != m_pContext->m_seqLayerHeader.heightMB)
            {
                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex,m_iPicBufIndex); // +1 !!!!!!!1
                if (m_va->m_Platform == VA_LINUX)
                    m_pPacker.VC1PackBitplaneBuffers(m_pContext);
                if (UMC_OK != m_va->Execute())
                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
            }
         }



        virtual void PrepareVLDVABuffers(Ipp32u*  pOffsets,
                                         Ipp32u*  pValues,
                                         Ipp8u*   pOriginalData,
                                         MediaDataEx::_MediaDataEx* pOrigStCodes)
        {
            if (m_pContext->m_picLayerHeader->PTYPE == VC1_SKIPPED_FRAME)
            {
                m_bIsSkippedFrame = true;
                m_bIsReadyToProcess = false;
                m_pPacker.MarkFrameAsSkip();
            }
            else
            {
                m_pPacker.VC1SetSliceBuffer();
                m_pPacker.VC1SetPictureBuffer();
                VC1PackSlices(pOffsets,pValues,pOriginalData,pOrigStCodes);
            }

            if ((m_iFrameCounter > 1) ||
                (!VC1_IS_REFERENCE(m_pContext->m_picLayerHeader->PTYPE)))
            {
                m_pStore->SetFirstBusyDescriptorAsReady();
            }
        }

        bool IsNeedToExecute()
        {
            return !m_bIsItarativeMode;
        }

        virtual ~VC1FrameDescriptorVA()
        {
        }
        virtual void SetVideoHardwareAccelerator(VideoAccelerator* va)
        {
            if (va)
            {
                m_va = va;
                m_pPacker.SetVideoAccelerator(m_va);
            }

        }
        virtual Status preProcData(VC1Context*            pContext,
                                   Ipp32u                 bufferSize,
                                   Ipp64u                 frameCount,
                                   bool                   isWMV,
                                   bool& skip)
        {
            Status vc1Sts = UMC_OK;

            Ipp32u Ptype;
            Ipp8u* pbufferStart = pContext->m_pBufferStart;

            m_iFrameCounter = frameCount;

            // there is self header before each frame in simple/main profile
            if (m_pContext->m_seqLayerHeader.PROFILE != VC1_PROFILE_ADVANCED)
            {
                bufferSize += VC1FHSIZE;
                m_pContext->m_FrameSize = bufferSize;
            }



            if (m_va->m_Platform == VA_LINUX)
            {
                if (m_iFrameCounter == 1)
                    m_pContext->m_frmBuff.m_iCurrIndex = -1;
            }


            ippsCopy_8u(pbufferStart,m_pContext->m_pBufferStart,(bufferSize & 0xFFFFFFF8) + 8); // (bufferSize & 0xFFFFFFF8) + 8 - skip frames

            m_pContext->m_bitstream.pBitstream = (Ipp32u*)m_pContext->m_pBufferStart;

            if ((m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED) || (!isWMV))
                m_pContext->m_bitstream.pBitstream += 1;

            m_pContext->m_bitstream.bitOffset = 31;
            m_pContext->m_picLayerHeader = m_pContext->m_InitPicLayer;
            m_pContext->m_seqLayerHeader = pContext->m_seqLayerHeader;
            m_bIsSpecialBSkipFrame = false;


            if (m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
            {
                m_pContext->m_bNeedToUseCompBuffer = 0;
                m_pBufferStart = m_pContext->m_bitstream.pBitstream;
                vc1Sts = GetNextPicHeader_Adv(m_pContext);
                UMC_CHECK_STATUS(vc1Sts);
                Ptype = m_pContext->m_picLayerHeader->PTYPE|m_pContext->m_picLayerHeader->PTypeField1;
                {
                    vc1Sts = SetPictureIndices(Ptype,skip);
                }

            }
            else
            {
                if (!isWMV)
                    m_pContext->m_bitstream.pBitstream = (Ipp32u*)m_pContext->m_pBufferStart + 2;

                m_pBufferStart = m_pContext->m_bitstream.pBitstream;
                vc1Sts = GetNextPicHeader(m_pContext, isWMV);
                UMC_CHECK_STATUS(vc1Sts);
                vc1Sts = SetPictureIndices(m_pContext->m_picLayerHeader->PTYPE,skip);
                UMC_CHECK_STATUS(vc1Sts);

            }
            if (m_pStore->IsNeedPostProcFrame(m_pContext->m_picLayerHeader->PTYPE))
                m_pContext->DeblockInfo.isNeedDbl = 1;
            else
                m_pContext->DeblockInfo.isNeedDbl = 0;

            m_pContext->m_bIntensityCompensation = 0;
            return vc1Sts;
        }
        virtual bool isVADescriptor() {return true;}
    protected:
        VideoAccelerator*     m_va;
        Ipp8u*                m_pBitstream;
        Ipp32u m_iSliceBufIndex;
        Ipp32u m_iPicBufIndex;
        bool  m_bIsItarativeMode;
        Ipp32u* m_pBufferStart;
        T m_pPacker;
    };

#ifdef UMC_VA_LINUX
    template <class T>
    class VC1FrameDescriptorVA_Linux: public VC1FrameDescriptorVA<T>
    {
    public:
        // Default constructor
        VC1FrameDescriptorVA_Linux(MemoryAllocator*      pMemoryAllocator,
                                       VideoAccelerator*     va):VC1FrameDescriptorVA<T>(pMemoryAllocator, va)
        {
        }

        virtual ~VC1FrameDescriptorVA_Linux(){};

        void PrepareVLDVABuffers(Ipp32u*  pOffsets,
                                 Ipp32u*  pValues,
                                 Ipp8u*   pOriginalData,
                                 MediaDataEx::_MediaDataEx* pOrigStCodes)
        {
            if (this->m_pContext->m_picLayerHeader->PTYPE == VC1_SKIPPED_FRAME)
            {
                this->m_bIsSkippedFrame = true;
                this->m_bIsReadyToProcess = false;
                this->m_pPacker.MarkFrameAsSkip();
            }
            else
            {
                this->m_pPacker.VC1SetPictureBuffer();
                this->m_pPacker.VC1SetSliceParamBuffer(pOffsets, pValues);
                this->m_pPacker.VC1SetSliceDataBuffer(this->m_pContext->m_FrameSize); // 4 - header ?
                VC1PackSlices(pOffsets,pValues,pOriginalData,pOrigStCodes);
            }

            if ((this->m_iFrameCounter > 1) ||
                (!VC1_IS_REFERENCE(this->m_pContext->m_picLayerHeader->PTYPE)))
            {
                this->m_pStore->SetFirstBusyDescriptorAsReady();
            }
        }
        virtual void VC1PackSlices    (Ipp32u*  pOffsets,
                                       Ipp32u*  pValues,
                                       Ipp8u*   pOriginalData,
                                       MediaDataEx::_MediaDataEx* pOrigStCodes)
       {
            Ipp32u PicHeaderFlag;
            Ipp32u* pPictHeader = this->m_pContext->m_bitstream.pBitstream;
            Ipp32u temp_value = 0;
            Ipp32u* bitstream;
            Ipp32s bitoffset = 31;
            Ipp32u SliceSize = 0;
            Ipp32u DataSize = 0;
            Ipp32u BitstreamDataSize = 0;
            Ipp32u IntCompField = 0;

            // need in case of fields
            Ipp32u* pFirstFieldStartCode = this->m_pContext->m_bitstream.pBitstream;
            Ipp32u Offset = 0;

            Ipp32u RemBytesInSlice = 4;
            Ipp32u PicHeaderSize = 0;
            Ipp8u  Flag_03 = 0; //for search 00 00 03 on the end of pic header

            Ipp32u* p_CurOriginalOffsets = pOrigStCodes->offsets;
            Ipp32u* p_NextOriginalOffsets = pOrigStCodes->offsets;

            Ipp32u*  p_CurOffsets = pOffsets;
            Ipp32u*  p_CurValues = pValues;
            Ipp8u*   p_CurOriginalData = pOriginalData;
            
            Ipp32u*  p_NextOffsets = pOffsets;
            Ipp32u*  p_NextValues = pValues;
            

            bool isSecondField = false;
            this->m_iSliceBufIndex = 0;
            this->m_iPicBufIndex = 0;

            Ipp32u MBEndRowOfAlreadyExec = 0;

            SliceParams slparams;
            memset(&slparams,0,sizeof(SliceParams));
            slparams.MBStartRow = 0;
            slparams.MBEndRow = this->m_pContext->m_seqLayerHeader.heightMB;
            this->m_pContext->m_picLayerHeader->CurrField = 0;

            if (this->m_pContext->m_picLayerHeader->PTYPE == VC1_SKIPPED_FRAME)
            {
                this->m_bIsSkippedFrame = true;
                this->m_bIsReadyToProcess = false;
                return;
            }
            else
            {
                this->m_bIsSkippedFrame = false;
            }

            //skip start codes till Frame Header
            while ((*p_CurValues)&&(*p_CurValues != 0x0D010000))
            {
                ++p_CurOffsets;
                ++p_CurValues;
                ++p_CurOriginalOffsets;
            }

            //move next st code data pointers
            p_NextOffsets = p_CurOffsets + 1;
            p_NextValues = p_CurValues + 1;
            p_NextOriginalOffsets = p_CurOriginalOffsets + 1;

            if (this->m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
                DecodePicHeader(this->m_pContext);
            else
                Decode_PictureLayer(this->m_pContext);
            IntCompField = this->m_pContext->m_picLayerHeader->INTCOMFIELD;
            if (this->m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
            {
                PicHeaderSize = (this->m_pPacker.VC1GetPicHeaderSize(p_CurOriginalData + 4,
                    (Ipp32u)((size_t)sizeof(Ipp32u)*(this->m_pContext->m_bitstream.pBitstream - pPictHeader)), Flag_03));
            }
            else
            {
                PicHeaderSize = (Ipp32u)((size_t)sizeof(Ipp32u)*(this->m_pContext->m_bitstream.pBitstream - pPictHeader));
            }

            this->m_pPacker.VC1PackPicParamsForOneSlice(this->m_pContext);

            slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
            slparams.m_bitOffset    = this->m_pContext->m_bitstream.bitOffset;
            if (VC1_PROFILE_ADVANCED == this->m_pContext->m_seqLayerHeader.PROFILE)
                Offset = PicHeaderSize + 4; // offset in bytes
            else
                Offset = PicHeaderSize;

            if (*(p_CurOffsets+1) && p_CurOriginalOffsets && (p_CurOriginalOffsets + 1))
                SliceSize = *(p_CurOriginalOffsets+1) - *p_CurOriginalOffsets - Offset;
            else
                SliceSize = this->m_pContext->m_FrameSize - Offset;

            //skip user data
            while(*(p_NextValues) == 0x1B010000 || *(p_NextValues) == 0x1D010000 || *(p_NextValues) == 0x1C010000)
            {
                p_NextOffsets++;
                p_NextValues++;
                p_NextOriginalOffsets++;
            }

            if (*p_NextValues == 0x0B010000)
            {
                //slice
                bitstream = reinterpret_cast<Ipp32u*>(this->m_pContext->m_pBufferStart + *p_NextOffsets);
                VC1BitstreamParser::GetNBits(bitstream, bitoffset,32, temp_value);
                bitoffset = 31;
                VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);     //SLICE_ADDR
                this->m_pContext->m_picLayerHeader->is_slice = 1;
            }
            else if(*p_NextValues == 0x0C010000)
            {
                //field
                slparams.MBEndRow = (this->m_pContext->m_seqLayerHeader.heightMB+1)/2;
            }

            VM_ASSERT(SliceSize<0x0FFFFFFF);

            {
                Ipp32u BytesAlreadyExecuted = 0;
                this->m_pPacker.VC1PackBitStream(this->m_pContext,
                                            DataSize,
                                            p_CurOriginalData + Offset,
                                            SliceSize,
                                            0, Flag_03);

                this->m_pPacker.VC1PackOneSlice(this->m_pContext, &slparams, this->m_iSliceBufIndex,
                                            0, DataSize, 0, 0);
                // let execute in case of fields
                if (*p_NextValues== 0x0C010000)
                {
                    // Execute first slice
                    this->m_pPacker.VC1PackBitplaneBuffers(this->m_pContext);
                    this->m_pPacker.VC1SetBuffersSize(this->m_iSliceBufIndex + 1,this->m_iPicBufIndex);

                    if (UMC_OK != this->m_va->Execute())
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                    this->m_iSliceBufIndex = 0;
                }
                else
                {
                    BitstreamDataSize += DataSize;
                    ++this->m_iSliceBufIndex;
                }
            }

            //move current st code data pointers
            p_CurOffsets = p_NextOffsets;
            p_CurValues  = p_NextValues;
            p_CurOriginalOffsets = p_NextOriginalOffsets;

            //move next st code data pointers
            p_NextOffsets++;
            p_NextValues++;
            p_NextOriginalOffsets++;

            while (*p_CurOffsets)
            {
                //Fields
                if (*(p_CurValues) == 0x0C010000)
                {
                    this->m_pContext->m_bitstream.pBitstream = pFirstFieldStartCode;

                    if (UMC_OK != this->m_va->EndFrame())
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);

                    this->m_pPacker.VC1SetPictureBuffer();
                    this->m_pPacker.VC1SetSliceParamBuffer(pOffsets, pValues);
                    this->m_pPacker.VC1SetSliceDataBuffer(this->m_pContext->m_FrameSize); //TODO: what is a valid size of a buffer here?
                    isSecondField = true;
                    // set swap bitstream for parsing
                    this->m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(this->m_pContext->m_pBufferStart + *p_CurOffsets);
                    this->m_pContext->m_bitstream.pBitstream += 1; // skip start code
                    this->m_pContext->m_bitstream.bitOffset = 31;
                    pPictHeader = this->m_pContext->m_bitstream.pBitstream;
                    PicHeaderSize = 0;
                    BitstreamDataSize = 0;

                    ++this->m_pContext->m_picLayerHeader;
                    *this->m_pContext->m_picLayerHeader = *this->m_pContext->m_InitPicLayer;
                    this->m_pContext->m_picLayerHeader->BottomField = (Ipp8u)this->m_pContext->m_InitPicLayer->TFF;
                    this->m_pContext->m_picLayerHeader->PTYPE = this->m_pContext->m_InitPicLayer->PTypeField2;
                    this->m_pContext->m_picLayerHeader->CurrField = 1;

                    this->m_pContext->m_picLayerHeader->is_slice = 0;
                    DecodePicHeader(this->m_pContext);

                    //this->m_pContext->this->m_picLayerHeader->INTCOMFIELD = IntCompField;
                    PicHeaderSize = (Ipp32u)this->m_pPacker.VC1GetPicHeaderSize(p_CurOriginalData + 4 + *p_CurOriginalOffsets,
                        (Ipp32u)((size_t)sizeof(Ipp32u)*(this->m_pContext->m_bitstream.pBitstream - pPictHeader)), Flag_03);

                    slparams.MBStartRow = (this->m_pContext->m_seqLayerHeader.heightMB+1)/2;

                    if (*(p_CurOffsets+1))
                        SliceSize = *(p_CurOriginalOffsets + 1) - *p_CurOriginalOffsets - 4 - PicHeaderSize;
                    else
                        SliceSize = this->m_pContext->m_FrameSize - *p_CurOriginalOffsets - 4 - PicHeaderSize;

                    //skip user data
                    while(*p_NextValues == 0x1B010000 || *p_NextValues == 0x1D010000 || *p_NextValues == 0x1C010000)
                    {
                        p_NextOffsets++;
                        p_NextValues++;
                        p_NextOriginalOffsets++;
                    }   

                    if (*p_NextOffsets && *p_NextValues == 0x0B010000)
                    {
                        bitstream = reinterpret_cast<Ipp32u*>(this->m_pContext->m_pBufferStart + *p_NextOffsets);
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
                        bitoffset = 31;
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);
                    } else
                        slparams.MBEndRow = this->m_pContext->m_seqLayerHeader.heightMB;

                    this->m_pPacker.VC1PackPicParamsForOneSlice(this->m_pContext);
                    ++this->m_iPicBufIndex;
                    slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
                    slparams.m_bitOffset = this->m_pContext->m_bitstream.bitOffset;
                    Offset = PicHeaderSize + 4; // offset in bytes
                    slparams.MBStartRow = 0; //we should decode fields steb by steb

                    {
                        Ipp32u BytesAlreadyExecuted = 0;
                        this->m_pPacker.VC1PackBitStream(this->m_pContext, DataSize,
                                                    pOriginalData + *p_CurOriginalOffsets + Offset,SliceSize,
                                                    0, Flag_03);

                        this->m_pPacker.VC1PackOneSlice(this->m_pContext,
                                                    &slparams,
                                                    this->m_iSliceBufIndex,
                                                    0,
                                                    DataSize,
                                                    0, 0);
                        BitstreamDataSize += DataSize;
                        ++this->m_iSliceBufIndex;
                    }

                    //move current st code data pointers
                    p_CurOffsets = p_NextOffsets;
                    p_CurValues  = p_NextValues;
                    p_CurOriginalOffsets = p_NextOriginalOffsets;

                    //move next st code data pointers
                    p_NextOffsets++;
                    p_NextValues++;
                    p_NextOriginalOffsets++;
                }
                //Slices
                else if (*p_CurValues == 0x0B010000)
                {
                    this->m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(this->m_pContext->m_pBufferStart + *p_CurOffsets);
                    VC1BitstreamParser::GetNBits(this->m_pContext->m_bitstream.pBitstream,this->m_pContext->m_bitstream.bitOffset,32, temp_value);
                    this->m_pContext->m_bitstream.bitOffset = 31;
                    pPictHeader = this->m_pContext->m_bitstream.pBitstream;
                    PicHeaderSize = 0;

                    VC1BitstreamParser::GetNBits(this->m_pContext->m_bitstream.pBitstream,this->m_pContext->m_bitstream.bitOffset,9, slparams.MBStartRow);     //SLICE_ADDR
                    VC1BitstreamParser::GetNBits(this->m_pContext->m_bitstream.pBitstream,this->m_pContext->m_bitstream.bitOffset,1, PicHeaderFlag);            //PIC_HEADER_FLAG

                    if (PicHeaderFlag == 1)                //PIC_HEADER_FLAG
                    {
                        ++this->m_pContext->m_picLayerHeader;
                        if (isSecondField)
                            this->m_pContext->m_picLayerHeader->CurrField = 1;
                        else
                            this->m_pContext->m_picLayerHeader->CurrField = 0;

                        DecodePictureHeader_Adv(this->m_pContext);
                        DecodePicHeader(this->m_pContext);
                        PicHeaderSize = this->m_pPacker.VC1GetPicHeaderSize(p_CurOriginalData + 4 + *p_CurOriginalOffsets,
                            (Ipp32u)((size_t)sizeof(Ipp32u)*(this->m_pContext->m_bitstream.pBitstream - pPictHeader)), Flag_03);

                        ++this->m_iPicBufIndex;
                    }
                    this->m_pContext->m_picLayerHeader->is_slice = 1;
                    //skip user data
                    while(*p_NextValues == 0x1B010000 || *p_NextValues == 0x1D010000 || *p_NextValues == 0x1C010000)
                    {
                        p_NextOffsets++;
                        p_NextValues++;
                        p_NextOriginalOffsets++;
                    }  

                    if (*p_NextOffsets && *p_NextValues == 0x0B010000)
                    {
                        bitstream = reinterpret_cast<Ipp32u*>(this->m_pContext->m_pBufferStart + *(pOffsets+1));
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
                        bitoffset = 31;
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);
                    }
                    else if(*p_NextValues == 0x0C010000)
                        slparams.MBEndRow = (this->m_pContext->m_seqLayerHeader.heightMB+1)/2;
                    else
                        slparams.MBEndRow = this->m_pContext->m_seqLayerHeader.heightMB;

                    slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
                    slparams.m_bitOffset = this->m_pContext->m_bitstream.bitOffset;
                    Offset = PicHeaderSize + 4;

                    if (isSecondField)
                        slparams.MBStartRow -= (this->m_pContext->m_seqLayerHeader.heightMB+1)/2;

                    if (*(p_CurOffsets+1))
                        SliceSize = *(p_CurOriginalOffsets+1) - *p_CurOriginalOffsets - Offset;
                    else
                        SliceSize = this->m_pContext->m_FrameSize - *p_CurOriginalOffsets - Offset;

                    {
                        this->m_pPacker.VC1PackBitStream(this->m_pContext,
                                                                      DataSize,
                                                                      p_CurOriginalData + *p_CurOriginalOffsets + Offset,
                                                                      SliceSize,
                                                                      BitstreamDataSize, Flag_03);

                        this->m_pPacker.VC1PackOneSlice(this->m_pContext,
                                                        &slparams,
                                                        this->m_iSliceBufIndex,   0,
                                                        DataSize, BitstreamDataSize, 0);

                        //skip user data
                        while(*p_NextValues == 0x1B010000 || *p_NextValues == 0x1D010000 || *p_NextValues == 0x1C010000)
                        {
                            p_NextOffsets++;
                            p_NextValues++;
                            p_NextOriginalOffsets++;
                        }  
                        // let execute in case of fields
                            if (*p_NextValues== 0x0C010000)
                        {
                            // Execute first slice
                            this->m_pPacker.VC1PackBitplaneBuffers(this->m_pContext);

                            this->m_pPacker.VC1SetBuffersSize(this->m_iSliceBufIndex + 1,this->m_iPicBufIndex);

                            if (UMC_OK != this->m_va->Execute())
                                throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                            this->m_iSliceBufIndex = 0;
                        }
                        else
                        {
                            BitstreamDataSize += DataSize/*SliceSize - RemBytesInSlice*/;
                            ++this->m_iSliceBufIndex;
                        }
                    }
                    //move current st code data pointers
                    p_CurOffsets = p_NextOffsets;
                    p_CurValues  = p_NextValues;
                    p_CurOriginalOffsets = p_NextOriginalOffsets;

                    //move next st code data pointers
                    p_NextOffsets++;
                    p_NextValues++;
                    p_NextOriginalOffsets++;

                }
                else
                {
                    //move current st code data pointers
                    p_CurOffsets = p_NextOffsets;
                    p_CurValues  = p_NextValues;
                    p_CurOriginalOffsets = p_NextOriginalOffsets;

                    //move next st code data pointers
                    p_NextOffsets++;
                    p_NextValues++;
                    p_NextOriginalOffsets++;
                }

            }
            //need to execute last slice
            if (MBEndRowOfAlreadyExec != this->m_pContext->m_seqLayerHeader.heightMB)
            {
                this->m_pPacker.VC1PackBitplaneBuffers(this->m_pContext);
                this->m_pPacker.VC1SetBuffersSize(this->m_iSliceBufIndex,this->m_iPicBufIndex);
                if (UMC_OK != this->m_va->Execute())
                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
            }
         }
    };

#endif

#ifdef UMC_VA_DXVA
    template <class T>
    class VC1FrameDescriptorVA_EagleLake: public VC1FrameDescriptorVA<T>
    {
    public:
        // Default constructor
        VC1FrameDescriptorVA_EagleLake(MemoryAllocator*      pMemoryAllocator,
                                       VideoAccelerator*     va):VC1FrameDescriptorVA<T>(pMemoryAllocator, va)
        {
        }

        virtual ~VC1FrameDescriptorVA_EagleLake(){};

        void PrepareVLDVABuffers(Ipp32u*  pOffsets,
                                 Ipp32u*  pValues,
                                 Ipp8u*   pOriginalData,
                                 MediaDataEx::_MediaDataEx* pOrigStCodes)
        {
            if (m_pContext->m_picLayerHeader->PTYPE == VC1_SKIPPED_FRAME)
            {
                m_bIsSkippedFrame = true;
                m_bIsReadyToProcess = false;
                m_pPacker.MarkFrameAsSkip();
            }
            else
            {
                m_pPacker.VC1SetSliceBuffer();
                m_pPacker.VC1SetPictureBuffer();
                m_pPacker.VC1SetExtPictureBuffer();
                VC1PackSlices(pOffsets,pValues,pOriginalData,pOrigStCodes);
            }

            if ((m_iFrameCounter > 1) ||
                (!VC1_IS_REFERENCE(m_pContext->m_picLayerHeader->PTYPE)))
            {
                m_pStore->SetFirstBusyDescriptorAsReady();
            }
        }
        virtual void VC1PackSlices    (Ipp32u*  pOffsets,
                                       Ipp32u*  pValues,
                                       Ipp8u*   pOriginalData,
                                       MediaDataEx::_MediaDataEx* pOrigStCodes)
        {
            Ipp32u PicHeaderFlag;
            Ipp32u* pPictHeader = m_pContext->m_bitstream.pBitstream;
            Ipp32u temp_value = 0;
            Ipp32u* bitstream;
            Ipp32s bitoffset = 31;
            Ipp32u SliceSize = 0;
            Ipp32u DataSize = 0;
            Ipp32u BitstreamDataSize = 0;
            Ipp32u IntCompField = 0;

                        // need in case of fields
            Ipp32u* pFirstFieldStartCode = m_pContext->m_bitstream.pBitstream;
            Ipp32u Offset = 0;

            Ipp32u RemBytesInSlice = 4;
            Ipp32u PicHeaderSize = 0;
            Ipp8u  Flag_03 = 0; //for search 00 00 03 on the end of pic header

            Ipp32u* p_CurOriginalOffsets = pOrigStCodes->offsets;
            Ipp32u* p_NextOriginalOffsets = pOrigStCodes->offsets;

            Ipp32u*  p_CurOffsets = pOffsets;
            Ipp32u*  p_CurValues = pValues;
            Ipp8u*   p_CurOriginalData = pOriginalData;
            
            Ipp32u*  p_NextOffsets = pOffsets;
            Ipp32u*  p_NextValues = pValues;
            

            bool isSecondField = false;
            m_iSliceBufIndex = 0;
            m_iPicBufIndex = 0;

            Ipp32u MBEndRowOfAlreadyExec = 0;

            SliceParams slparams;
            memset(&slparams,0,sizeof(SliceParams));
            slparams.MBStartRow = 0;
            slparams.MBEndRow = m_pContext->m_seqLayerHeader.heightMB;
            m_pContext->m_picLayerHeader->CurrField = 0;

           // VM_ASSERT(VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE);

            //if (VC1_PROFILE_ADVANCED != m_pContext->m_seqLayerHeader.PROFILE)
            //    return;
#ifdef VC1_DEBUG_ON_LOG
            static Ipp32u Num = 0;
            Num++;

            printf("\n\n\nPicture type %d  %d\n\n\n", m_pContext->m_picLayerHeader->PTYPE, Num);
#endif
            
            if (m_pContext->m_picLayerHeader->PTYPE == VC1_SKIPPED_FRAME)
            {
                m_bIsSkippedFrame = true;
                m_bIsReadyToProcess = false;
                return;
            }
            else
            {
                m_bIsSkippedFrame = false;
            }

           //skip start codes till Frame Header
            while ((*p_CurValues)&&(*p_CurValues != 0x0D010000))
            {
                ++p_CurOffsets;
                ++p_CurValues;
                ++p_CurOriginalOffsets;
            }

            //move next st code data pointers
            p_NextOffsets = p_CurOffsets + 1;
            p_NextValues = p_CurValues + 1;
            p_NextOriginalOffsets = p_CurOriginalOffsets + 1;

            if (m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
                DecodePicHeader(m_pContext);
            else
                Decode_PictureLayer(m_pContext);
            IntCompField = m_pContext->m_picLayerHeader->INTCOMFIELD;
            if (m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
            {
                PicHeaderSize = (m_pPacker.VC1GetPicHeaderSize(p_CurOriginalData + 4,
                    (Ipp32u)((size_t)sizeof(Ipp32u)*(m_pContext->m_bitstream.pBitstream - pPictHeader)), Flag_03));
            }
            else
            {
                PicHeaderSize = (Ipp32u)((size_t)sizeof(Ipp32u)*(m_pContext->m_bitstream.pBitstream - pPictHeader));
            }

            m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);


            slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
            slparams.m_bitOffset    = m_pContext->m_bitstream.bitOffset;
            if (VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE)
                Offset = PicHeaderSize + 4; // offset in bytes
            else
                Offset = PicHeaderSize;

            if (*(p_CurOffsets+1))
                SliceSize = *(p_CurOriginalOffsets+1) - *p_CurOriginalOffsets - Offset;
            else
                SliceSize = m_pContext->m_FrameSize - Offset;

            //skip user data
            while(*(p_NextValues) == 0x1B010000 || *(p_NextValues) == 0x1D010000 || *(p_NextValues) == 0x1C010000)
            {
                p_NextOffsets++;
                p_NextValues++;
                p_NextOriginalOffsets++;
            }

            if (*p_NextValues == 0x0B010000)
            {
                //slice
                bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *p_NextOffsets);
                VC1BitstreamParser::GetNBits(bitstream, bitoffset,32, temp_value);
                bitoffset = 31;
                VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);     //SLICE_ADDR
                m_pContext->m_picLayerHeader->is_slice = 1;
            }
            else if(*p_NextValues == 0x0C010000)
            {
                //field
                slparams.MBEndRow = (m_pContext->m_seqLayerHeader.heightMB+1)/2;
            }

            VM_ASSERT(SliceSize<0x0FFFFFFF);

            {
                Ipp32u chopType = 1;
                Ipp32u BytesAlreadyExecuted = 0;
                RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                                              DataSize,
                                                              p_CurOriginalData + Offset,
                                                              SliceSize,
                                                              0, Flag_03);
                if (RemBytesInSlice)
                {
                    MBEndRowOfAlreadyExec = slparams.MBEndRow;

                    // fill slices
                    while (RemBytesInSlice) // we don't have enough buffer - execute it
                    {
                        m_pPacker.VC1PackOneSlice(m_pContext, &slparams, m_iSliceBufIndex,
                                                  0, DataSize,  0,  chopType);

                        m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                        if (UMC_OK != m_va->Execute())
                            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);

                        BytesAlreadyExecuted += DataSize;

                        m_pPacker.VC1SetSliceBuffer();
                        m_pPacker.VC1SetPictureBuffer();
                        m_pPacker.VC1SetExtPictureBuffer();

                        m_iSliceBufIndex = 0;

                        m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                        RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                                                      DataSize,
                                                                      p_CurOriginalData + BytesAlreadyExecuted + Offset,
                                                                      RemBytesInSlice,
                                                                      0, Flag_03);
                        slparams.m_bitOffset = 0;
                        chopType = 3;
                        Offset = 4;
                    }

                    chopType = 2;

                    m_pPacker.VC1PackOneSlice(m_pContext, &slparams,
                                              m_iSliceBufIndex,  0,
                                              DataSize, 0, chopType);

                    m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                    m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                    if (UMC_OK != m_va->Execute())
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                    m_iSliceBufIndex = 0;

                    // we need receive buffers from drv for future processing
                    if (MBEndRowOfAlreadyExec != m_pContext->m_seqLayerHeader.heightMB)
                    {
                        m_pPacker.VC1SetSliceBuffer();
                        m_pPacker.VC1SetPictureBuffer();
                        m_pPacker.VC1SetExtPictureBuffer();
                    }

                }
                else
                {
                    m_pPacker.VC1PackOneSlice(m_pContext, &slparams, m_iSliceBufIndex,
                                              0, DataSize, 0, 0);
                    // let execute in case of fields
                    if (*p_NextValues== 0x0C010000)
                    {
                        // Execute first slice
                        m_pPacker.VC1PackBitplaneBuffers(m_pContext);
                        m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex + 1,m_iPicBufIndex);

                        if (UMC_OK != m_va->Execute())
                            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                        m_iSliceBufIndex = 0;
                    }
                    else
                    {
                        BitstreamDataSize += DataSize;
                        ++m_iSliceBufIndex;
                    }
                }
            }

            //move current st code data pointers
            p_CurOffsets = p_NextOffsets;
            p_CurValues  = p_NextValues;
            p_CurOriginalOffsets = p_NextOriginalOffsets;

            //move next st code data pointers
            p_NextOffsets++;
            p_NextValues++;
            p_NextOriginalOffsets++;

            while (*p_CurOffsets)
            {
                //Fields
                if (*(p_CurValues) == 0x0C010000)
                {
                    m_pContext->m_bitstream.pBitstream = pFirstFieldStartCode;

                    if (UMC_OK != m_va->EndFrame())
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);

                    if (m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ||
                    m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG|| 
                    m_pContext->m_seqLayerHeader.RANGERED)
                     {
                        if (UMC_OK != m_va->EndFrame())
                            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                    }

                    if (UMC_OK != m_va->BeginFrame(m_pContext->m_frmBuff.m_iCurrIndex))
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);

                    
                    m_pPacker.VC1SetSliceBuffer();
                    m_pPacker.VC1SetPictureBuffer();
                    m_pPacker.VC1SetExtPictureBuffer();
                    isSecondField = true;
                    // set swap bitstream for parsing
                    m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *p_CurOffsets);
                    m_pContext->m_bitstream.pBitstream += 1; // skip start code
                    m_pContext->m_bitstream.bitOffset = 31;
                    pPictHeader = m_pContext->m_bitstream.pBitstream;
                    PicHeaderSize = 0;
                    BitstreamDataSize = 0;

                    ++m_pContext->m_picLayerHeader;
                    *m_pContext->m_picLayerHeader = *m_pContext->m_InitPicLayer;
                    m_pContext->m_picLayerHeader->BottomField = (Ipp8u)m_pContext->m_InitPicLayer->TFF;
                    m_pContext->m_picLayerHeader->PTYPE = m_pContext->m_InitPicLayer->PTypeField2;
                    m_pContext->m_picLayerHeader->CurrField = 1;

                    m_pContext->m_picLayerHeader->is_slice = 0;
                    DecodePicHeader(m_pContext);

                    //m_pContext->m_picLayerHeader->INTCOMFIELD = IntCompField;
                    PicHeaderSize = (Ipp32u)m_pPacker.VC1GetPicHeaderSize(p_CurOriginalData + 4 + *p_CurOriginalOffsets,
                        (Ipp32u)((size_t)sizeof(Ipp32u)*(m_pContext->m_bitstream.pBitstream - pPictHeader)), Flag_03);

                    slparams.MBStartRow = (m_pContext->m_seqLayerHeader.heightMB+1)/2;

                    if (*(p_CurOffsets+1))
                        SliceSize = *(p_CurOriginalOffsets + 1) - *p_CurOriginalOffsets - 4 - PicHeaderSize;
                    else
                        SliceSize = m_pContext->m_FrameSize - *p_CurOriginalOffsets - 4 - PicHeaderSize;

                    //skip user data
                    while(*p_NextValues == 0x1B010000 || *p_NextValues == 0x1D010000 || *p_NextValues == 0x1C010000)
                    {
                        p_NextOffsets++;
                        p_NextValues++;
                        p_NextOriginalOffsets++;
                    }   

                    if (*p_NextOffsets && *p_NextValues == 0x0B010000)
                    {
                        bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *p_NextOffsets);
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
                        bitoffset = 31;
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);
                    } else
                        slparams.MBEndRow = m_pContext->m_seqLayerHeader.heightMB;

                    m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                    ++m_iPicBufIndex;
                    slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
                    slparams.m_bitOffset = m_pContext->m_bitstream.bitOffset;
                    Offset = PicHeaderSize + 4; // offset in bytes
                    slparams.MBStartRow = 0; //we should decode fields steb by steb

                    {
                        Ipp32u chopType = 1;
                        Ipp32u BytesAlreadyExecuted = 0;
                        RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext, DataSize,
                                                                      p_CurOriginalData + *p_CurOriginalOffsets + Offset,SliceSize,
                                                                      0, Flag_03);
                        if (RemBytesInSlice)
                        {
                            MBEndRowOfAlreadyExec = slparams.MBEndRow;

                            // fill slices
                            while (RemBytesInSlice) // we don't have enough buffer - execute it
                            {
                                m_pPacker.VC1PackOneSlice(m_pContext,   &slparams,   m_iSliceBufIndex,
                                                          0, DataSize, 0,   chopType);

                                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                                if (UMC_OK != m_va->Execute())
                                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                                BytesAlreadyExecuted += DataSize;

                                m_pPacker.VC1SetSliceBuffer();
                                m_pPacker.VC1SetPictureBuffer();
                                m_iSliceBufIndex = 0;

                                m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                                RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext, DataSize,
                                                                              p_CurOriginalData + BytesAlreadyExecuted,
                                                                              RemBytesInSlice,
                                                                              0, Flag_03);
                                chopType = 3;
                                slparams.m_bitOffset = 0;
                                Offset = 0;
                            }

                            m_pPacker.VC1PackOneSlice(m_pContext, &slparams,  m_iSliceBufIndex,
                                                      0, DataSize, 0, 2);

                            m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                            m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                            if (UMC_OK != m_va->Execute())
                                throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                            m_iSliceBufIndex = 0;

                            // we need receive buffers from drv for future processing
                            if (MBEndRowOfAlreadyExec != m_pContext->m_seqLayerHeader.heightMB)
                            {
                                m_pPacker.VC1SetSliceBuffer();
                                m_pPacker.VC1SetPictureBuffer();
                                m_pPacker.VC1SetExtPictureBuffer();
                            }
                        }
                        else
                        {
                            m_pPacker.VC1PackOneSlice(m_pContext,
                                                      &slparams,
                                                      m_iSliceBufIndex,
                                                      0,
                                                      DataSize,
                                                      0, 0);
                            BitstreamDataSize += DataSize;
                            ++m_iSliceBufIndex;
                        }
                    }

                    //move current st code data pointers
                    p_CurOffsets = p_NextOffsets;
                    p_CurValues  = p_NextValues;
                    p_CurOriginalOffsets = p_NextOriginalOffsets;

                    //move next st code data pointers
                    p_NextOffsets++;
                    p_NextValues++;
                    p_NextOriginalOffsets++;
                }
                //Slices
                else if (*p_CurValues == 0x0B010000)
                {
                    m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *p_CurOffsets);
                    VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,32, temp_value);
                    m_pContext->m_bitstream.bitOffset = 31;
                    pPictHeader = m_pContext->m_bitstream.pBitstream;
                    PicHeaderSize = 0;

                    VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,9, slparams.MBStartRow);     //SLICE_ADDR
                    VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,1, PicHeaderFlag);            //PIC_HEADER_FLAG

                    if (PicHeaderFlag == 1)                //PIC_HEADER_FLAG
                    {
                        ++m_pContext->m_picLayerHeader;
                        if (isSecondField)
                            m_pContext->m_picLayerHeader->CurrField = 1;
                        else
                            m_pContext->m_picLayerHeader->CurrField = 0;

                        DecodePictureHeader_Adv(m_pContext);
                        DecodePicHeader(m_pContext);
                        PicHeaderSize = m_pPacker.VC1GetPicHeaderSize(p_CurOriginalData + 4 + *p_CurOriginalOffsets,
                            (Ipp32u)((size_t)sizeof(Ipp32u)*(m_pContext->m_bitstream.pBitstream - pPictHeader)), Flag_03);

                        ++m_iPicBufIndex;
                    }
                    m_pContext->m_picLayerHeader->is_slice = 1;
                    //skip user data
                    while(*p_NextValues == 0x1B010000 || *p_NextValues == 0x1D010000 || *p_NextValues == 0x1C010000)
                    {
                        p_NextOffsets++;
                        p_NextValues++;
                        p_NextOriginalOffsets++;
                    }  

                    if (*p_NextOffsets && *p_NextValues == 0x0B010000)
                    {
                        bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *p_NextOffsets);
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
                        bitoffset = 31;
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);
                    }
                    else if(*p_NextValues == 0x0C010000)
                        slparams.MBEndRow = (m_pContext->m_seqLayerHeader.heightMB+1)/2;
                    else
                        slparams.MBEndRow = m_pContext->m_seqLayerHeader.heightMB;

                    slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
                    slparams.m_bitOffset = m_pContext->m_bitstream.bitOffset;
                    Offset = PicHeaderSize + 4;

                    if (isSecondField)
                        slparams.MBStartRow -= (m_pContext->m_seqLayerHeader.heightMB+1)/2;

                    if (*(p_CurOffsets+1))
                        SliceSize = *(p_CurOriginalOffsets+1) - *p_CurOriginalOffsets - Offset;
                    else
                        SliceSize = m_pContext->m_FrameSize - *p_CurOriginalOffsets - Offset;

                    {
                        Ipp32u chopType = 1;
                        RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                                                      DataSize,
                                                                      p_CurOriginalData + *p_CurOriginalOffsets + Offset,
                                                                      SliceSize,
                                                                      BitstreamDataSize, Flag_03);
                        if (RemBytesInSlice)
                        {
                            MBEndRowOfAlreadyExec = slparams.MBEndRow;
                            Ipp32u BytesAlreadyExecuted = 0;

                            // fill slices
                            while (RemBytesInSlice) // we don't have enough buffer - execute it
                            {
                                m_pPacker.VC1PackOneSlice(m_pContext,    &slparams,
                                                          m_iSliceBufIndex,  0,
                                                          DataSize,          0,   chopType);

                                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                                m_va->Execute();
                                BytesAlreadyExecuted += DataSize;

                                m_pPacker.VC1SetSliceBuffer();
                                m_pPacker.VC1SetPictureBuffer();
                                m_pPacker.VC1SetExtPictureBuffer();

                                m_iSliceBufIndex = 0;

                                m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                                RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                                                              DataSize,
                                                                              p_CurOriginalData + *p_CurOriginalOffsets + BytesAlreadyExecuted + Offset + 4,
                                                                              RemBytesInSlice, 0, Flag_03);

                                chopType = 3;
                                slparams.m_bitOffset = 0;
                                Offset = 0;
                            }

                            m_pPacker.VC1PackOneSlice(m_pContext,       &slparams,
                                                      m_iSliceBufIndex, 0,
                                                      DataSize,         0, 2);
                            m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);
                            m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                            m_va->Execute();

                            m_iSliceBufIndex = 0;

                            // we need receive buffers from drv for future processing
                            if (MBEndRowOfAlreadyExec != m_pContext->m_seqLayerHeader.heightMB)
                            {
                                m_pPacker.VC1SetSliceBuffer();
                                m_pPacker.VC1SetPictureBuffer();
                                m_pPacker.VC1SetExtPictureBuffer();
                            }
                        }
                        else
                        {
                            m_pPacker.VC1PackOneSlice(m_pContext,         &slparams,
                                                      m_iSliceBufIndex,   0,
                                                      DataSize,           BitstreamDataSize, 0);

                        //skip user data
                        while(*p_NextValues == 0x1B010000 || *p_NextValues == 0x1D010000 || *p_NextValues == 0x1C010000)
                        {
                            p_NextOffsets++;
                            p_NextValues++;
                            p_NextOriginalOffsets++;
                        }  
                            // let execute in case of fields
                            if (*p_NextValues== 0x0C010000)
                            {
                                // Execute first slice
                                m_pPacker.VC1PackBitplaneBuffers(m_pContext);

                                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex + 1,m_iPicBufIndex);

                                if (UMC_OK != m_va->Execute())
                                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                                m_iSliceBufIndex = 0;
                            }
                            else
                            {
                                BitstreamDataSize += DataSize/*SliceSize - RemBytesInSlice*/;
                                ++m_iSliceBufIndex;
                            }
                        }
                    }
                    //move current st code data pointers
                    p_CurOffsets = p_NextOffsets;
                    p_CurValues  = p_NextValues;
                    p_CurOriginalOffsets = p_NextOriginalOffsets;

                    //move next st code data pointers
                    p_NextOffsets++;
                    p_NextValues++;
                    p_NextOriginalOffsets++;

                }
                else
                {
                    //move current st code data pointers
                    p_CurOffsets = p_NextOffsets;
                    p_CurValues  = p_NextValues;
                    p_CurOriginalOffsets = p_NextOriginalOffsets;

                    //move next st code data pointers
                    p_NextOffsets++;
                    p_NextValues++;
                    p_NextOriginalOffsets++;
                }

            }
            //need to execute last slice
            if (MBEndRowOfAlreadyExec != m_pContext->m_seqLayerHeader.heightMB)
            {
                m_pPacker.VC1PackBitplaneBuffers(m_pContext);
                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex,m_iPicBufIndex); // +1 !!!!!!!1
                if (UMC_OK != m_va->Execute())
                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);

                if (m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ||
                    m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG|| 
                    m_pContext->m_seqLayerHeader.RANGERED)
                {
                    if (UMC_OK != m_va->EndFrame())
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                }
            }
         }
    };



    template <class T>
    class VC1FrameDescriptorVA_Protected: public VC1FrameDescriptorVA<T>
    {
    public:
        // Default constructor
        VC1FrameDescriptorVA_Protected(MemoryAllocator*      pMemoryAllocator,
            VideoAccelerator*     va):VC1FrameDescriptorVA(pMemoryAllocator, va)
        {
        }

        virtual ~VC1FrameDescriptorVA_Protected(){};

        void PrepareVLDVABuffers(Ipp32u*  pOffsets,
            Ipp32u*  pValues,
            Ipp8u*   pOriginalData,
            MediaDataEx::_MediaDataEx* pOrigStCodes)
        {
            if (m_pContext->m_picLayerHeader->PTYPE == VC1_SKIPPED_FRAME)
            {
                m_bIsSkippedFrame = true;
                m_bIsReadyToProcess = false;
                m_pPacker.MarkFrameAsSkip();
            }
            else
            {
                m_pPacker.VC1SetSliceBuffer();
                m_pPacker.VC1SetPictureBuffer();
                m_pPacker.VC1SetExtPictureBuffer();
                VC1PackSlices(pOffsets,pValues,pOriginalData,pOrigStCodes);
            }

            if ((m_iFrameCounter > 1) ||
                (!VC1_IS_REFERENCE(m_pContext->m_picLayerHeader->PTYPE)))
            {
                m_pStore->SetFirstBusyDescriptorAsReady();
            }
        }

        virtual void VC1PackSlices    (Ipp32u*  pOffsets,Ipp32u*  pValues, Ipp8u* pOriginalData,MediaDataEx::_MediaDataEx* pOrigStCodes)
        {
            if(m_va->GetProtectedVA()->GetBitstream()->EncryptedData)
            {
                VC1PackSlices_Encrypt(pOffsets,pValues, pOriginalData,pOrigStCodes);
            }
            else
            {
                VC1PackSlices_No_Encrypt(pOffsets,pValues, pOriginalData,pOrigStCodes);
            }

        }

        virtual void VC1PackSlices_No_Encrypt    (Ipp32u*  pOffsets,Ipp32u*  pValues, Ipp8u* pOriginalData,MediaDataEx::_MediaDataEx* pOrigStCodes)
        {
            Ipp32u PicHeaderFlag;
            Ipp32u* pPictHeader = m_pContext->m_bitstream.pBitstream;
            Ipp32u temp_value = 0;
            Ipp32u* bitstream;
            Ipp32s bitoffset = 31;
            Ipp32u SliceSize = 0;
            Ipp32u DataSize = 0;
            Ipp32u BitstreamDataSize = 0;
            Ipp32u IntCompField = 0;

            Ipp32u* pOriginalOffsets = pOrigStCodes->offsets;
            // need in case of fields
            Ipp32u* pFirstFieldStartCode = m_pContext->m_bitstream.pBitstream;
            Ipp32u Offset = 0;

            Ipp32u RemBytesInSlice = 4;
            Ipp32u PicHeaderSize = 0;
            Ipp8u  Flag_03 = 0; //for search 00 00 03 on the end of pic header

            if(NULL != m_va->GetProtectedVA() && MFX_PROTECTION_PAVP == m_va->GetProtectedVA()->GetProtected())
            {
                DXVA2_DecodeExtensionData  extensionData = {0};
                DXVA_EncryptProtocolHeader extensionOutput = {0};
                HRESULT hr = S_OK;
                DXVA_Intel_Pavp_Protocol2  extensionInput = {0};
                extensionInput.EncryptProtocolHeader.dwFunction = 0xffff0001;
                extensionInput.EncryptProtocolHeader.guidEncryptProtocol = DXVA_NoEncrypt;

                extensionInput.PavpEncryptionMode.eEncryptionType = (PAVP_ENCRYPTION_TYPE) m_va->GetProtectedVA()->GetEncryptionMode();
                extensionInput.PavpEncryptionMode.eCounterMode = (PAVP_COUNTER_TYPE) m_va->GetProtectedVA()->GetCounterMode();

                extensionData.pPrivateInputData = &extensionInput;
                extensionData.pPrivateOutputData = &extensionOutput;
                extensionData.PrivateInputDataSize = sizeof(extensionInput);
                extensionData.PrivateOutputDataSize = sizeof(extensionOutput);
                hr = m_va->ExecuteExtensionBuffer(&extensionData);

                if (FAILED(hr) ||
                    // asomsiko: This is workaround for 15.22 driver that starts returning GUID_NULL instead DXVA_NoEncrypt
                    /*extensionOutput.guidEncryptProtocol != DXVA_NoEncrypt ||*/
                    extensionOutput.dwFunction != 0xffff0801)
                {
                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                }
            }

            bool isSecondField = false;
            m_iSliceBufIndex = 0;
            m_iPicBufIndex = 0;

            Ipp32u MBEndRowOfAlreadyExec = 0;

            SliceParams slparams;
            memset(&slparams,0,sizeof(SliceParams));
            slparams.MBStartRow = 0;
            slparams.MBEndRow = m_pContext->m_seqLayerHeader.heightMB;
            m_pContext->m_picLayerHeader->CurrField = 0;

            VM_ASSERT(VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE);

            if (VC1_PROFILE_ADVANCED != m_pContext->m_seqLayerHeader.PROFILE)
                return;

#ifdef VC1_DEBUG_ON_LOG
            static Ipp32u Num = 0;

            printf("\n\n\nPicture type %d  %d\n\n\n", m_pContext->m_picLayerHeader->PTYPE, Num);
            Num++;
#endif

            if (m_pContext->m_picLayerHeader->PTYPE == VC1_SKIPPED_FRAME)
            {
                m_bIsSkippedFrame = true;
                m_bIsReadyToProcess = false;
                return;
            }
            else
            {
                m_bIsSkippedFrame = false;
            }

           //skip start codes till Frame Header
            while ((*pValues)&&(*pValues != 0x0D010000))
            {
                ++pOffsets;
                ++pValues;
            }

            DecodePicHeader(m_pContext);
            IntCompField = m_pContext->m_picLayerHeader->INTCOMFIELD;
            PicHeaderSize = (m_pPacker.VC1GetPicHeaderSize(pOriginalData + 4,
                (Ipp32u)((size_t)sizeof(Ipp32u)*(m_pContext->m_bitstream.pBitstream - pPictHeader)), Flag_03));

            m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);

            if (*(pValues+1) == 0x0B010000)
            {
                //slice
                bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets+1));
                VC1BitstreamParser::GetNBits(bitstream, bitoffset,32, temp_value);
                bitoffset = 31;
                VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);     //SLICE_ADDR
                m_pContext->m_picLayerHeader->is_slice = 1;
            }
            else if(*(pValues+1) == 0x0C010000)
            {
                //field
                slparams.MBEndRow = (m_pContext->m_seqLayerHeader.heightMB+1)/2;
            }

            slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
            slparams.m_bitOffset    = m_pContext->m_bitstream.bitOffset;
            Offset = PicHeaderSize + 4; // offset in bytes

            if (*(pOffsets+1))
                SliceSize = *(pOriginalOffsets+1) - *pOriginalOffsets - Offset;
            else
                SliceSize = m_pContext->m_FrameSize - Offset;

            VM_ASSERT(SliceSize<0x0FFFFFFF);

            {
                Ipp32u chopType = 1;
                Ipp32u BytesAlreadyExecuted = 0;
                RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                                              DataSize,
                                                              pOriginalData + Offset,
                                                              SliceSize,
                                                              0, Flag_03);
                if (RemBytesInSlice)
                {
                    MBEndRowOfAlreadyExec = slparams.MBEndRow;

                    // fill slices
                    while (RemBytesInSlice) // we don't have enough buffer - execute it
                    {
                        m_pPacker.VC1PackOneSlice(m_pContext, &slparams, m_iSliceBufIndex,
                                                  0, DataSize,  0,  chopType);

                        m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                        if (UMC_OK != m_va->Execute())
                            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);

                        BytesAlreadyExecuted += DataSize;

                        m_pPacker.VC1SetSliceBuffer();
                        m_pPacker.VC1SetPictureBuffer();
                        m_pPacker.VC1SetExtPictureBuffer();

                        m_iSliceBufIndex = 0;

                        m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                        RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                                                      DataSize,
                                                                      pOriginalData + BytesAlreadyExecuted + Offset,
                                                                      RemBytesInSlice,
                                                                      0, Flag_03);
                        slparams.m_bitOffset = 0;
                        chopType = 3;
                        Offset = 4;
                    }

                    chopType = 2;

                    m_pPacker.VC1PackOneSlice(m_pContext, &slparams,
                                              m_iSliceBufIndex,  0,
                                              DataSize, 0, chopType);

                    m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                    m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                    if (UMC_OK != m_va->Execute())
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                    m_iSliceBufIndex = 0;

                    // we need receive buffers from drv for future processing
                    if (MBEndRowOfAlreadyExec != m_pContext->m_seqLayerHeader.heightMB)
                    {
                        m_pPacker.VC1SetSliceBuffer();
                        m_pPacker.VC1SetPictureBuffer();
                        m_pPacker.VC1SetExtPictureBuffer();
                    }

                }
                else
                {
                    m_pPacker.VC1PackOneSlice(m_pContext, &slparams, m_iSliceBufIndex,
                                              0, DataSize, 0, 0);
                    // let execute in case of fields
                    if (*(pValues+1)== 0x0C010000)
                    {
                        // Execute first slice
                        m_pPacker.VC1PackBitplaneBuffers(m_pContext);
                        m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex + 1,m_iPicBufIndex);

                        if (UMC_OK != m_va->Execute())
                            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                        m_iSliceBufIndex = 0;
                    }
                    else
                    {
                        BitstreamDataSize += DataSize;
                        ++m_iSliceBufIndex;
                    }
                }
            }
            ++pOffsets;
            ++pValues;
            ++pOriginalOffsets;


            while (*pOffsets)
            {
                if(NULL != m_va->GetProtectedVA() && MFX_PROTECTION_PAVP == m_va->GetProtectedVA()->GetProtected())
                {
                    DXVA2_DecodeExtensionData  extensionData = {0};
                    DXVA_EncryptProtocolHeader extensionOutput = {0};
                    HRESULT hr = S_OK;

                    DXVA_Intel_Pavp_Protocol2  extensionInput = {0};
                    extensionInput.EncryptProtocolHeader.dwFunction = 0xffff0001;
                    extensionInput.EncryptProtocolHeader.guidEncryptProtocol = DXVA_NoEncrypt;
                    extensionInput.dwBufferSize = 0;

                    extensionInput.PavpEncryptionMode.eEncryptionType = (PAVP_ENCRYPTION_TYPE) m_va->GetProtectedVA()->GetEncryptionMode();
                    extensionInput.PavpEncryptionMode.eCounterMode = (PAVP_COUNTER_TYPE) m_va->GetProtectedVA()->GetCounterMode();

                    extensionData.pPrivateInputData = &extensionInput;
                    extensionData.pPrivateOutputData = &extensionOutput;
                    extensionData.PrivateInputDataSize = sizeof(extensionInput);
                    extensionData.PrivateOutputDataSize = sizeof(extensionOutput);
                    hr = m_va->ExecuteExtensionBuffer(&extensionData);

                    if (FAILED(hr) ||
                        // asomsiko: This is workaround for 15.22 driver that starts returning GUID_NULL instead DXVA_NoEncrypt
                        /*extensionOutput.guidEncryptProtocol != DXVA_NoEncrypt ||*/
                        extensionOutput.dwFunction != 0xffff0801)
                    {
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                    }
                }

                //Fields
                if (*(pValues) == 0x0C010000)
                {
                    m_pContext->m_bitstream.pBitstream = pFirstFieldStartCode;

                    if (UMC_OK != m_va->EndFrame())
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                    if (UMC_OK != m_va->BeginFrame(m_pContext->m_frmBuff.m_iCurrIndex))
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);


                    m_pPacker.VC1SetSliceBuffer();
                    m_pPacker.VC1SetPictureBuffer();
                    m_pPacker.VC1SetExtPictureBuffer();
                    isSecondField = true;
                    // set swap bitstream for parsing
                    m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *pOffsets);
                    m_pContext->m_bitstream.pBitstream += 1; // skip start code
                    m_pContext->m_bitstream.bitOffset = 31;
                    pPictHeader = m_pContext->m_bitstream.pBitstream;
                    PicHeaderSize = 0;
                    BitstreamDataSize = 0;

                    ++m_pContext->m_picLayerHeader;
                    *m_pContext->m_picLayerHeader = *m_pContext->m_InitPicLayer;
                    m_pContext->m_picLayerHeader->BottomField = (Ipp8u)m_pContext->m_InitPicLayer->TFF;
                    m_pContext->m_picLayerHeader->PTYPE = m_pContext->m_InitPicLayer->PTypeField2;
                    m_pContext->m_picLayerHeader->CurrField = 1;

                    m_pContext->m_picLayerHeader->is_slice = 0;
                    DecodePicHeader(m_pContext);

                    m_pContext->m_picLayerHeader->INTCOMFIELD = IntCompField;
                    PicHeaderSize = (Ipp32u)m_pPacker.VC1GetPicHeaderSize(pOriginalData + 4 + *pOriginalOffsets,
                        (Ipp32u)((size_t)sizeof(Ipp32u)*(m_pContext->m_bitstream.pBitstream - pPictHeader)), Flag_03);

                    slparams.MBStartRow = (m_pContext->m_seqLayerHeader.heightMB+1)/2;

                    if (*(pOffsets+1))
                        SliceSize = *(pOriginalOffsets+1) - *pOriginalOffsets - 4 - PicHeaderSize;
                    else
                        SliceSize = m_pContext->m_FrameSize - *pOriginalOffsets - 4 - PicHeaderSize;

                    if (*(pOffsets+1) && *(pValues+1) == 0x0B010000)
                    {
                        bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets+1));
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
                        bitoffset = 31;
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);
                    } else
                        slparams.MBEndRow = m_pContext->m_seqLayerHeader.heightMB;

                    m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                    ++m_iPicBufIndex;
                    slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
                    slparams.m_bitOffset = m_pContext->m_bitstream.bitOffset;
                    Offset = PicHeaderSize + 4; // offset in bytes
                    slparams.MBStartRow = 0; //we should decode fields steb by steb

                    {
                        Ipp32u chopType = 1;
                        Ipp32u BytesAlreadyExecuted = 0;
                        RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext, DataSize,
                                                                      pOriginalData + *pOriginalOffsets + Offset,SliceSize,
                                                                      0, Flag_03);
                        if (RemBytesInSlice)
                        {
                            MBEndRowOfAlreadyExec = slparams.MBEndRow;

                            // fill slices
                            while (RemBytesInSlice) // we don't have enough buffer - execute it
                            {
                                m_pPacker.VC1PackOneSlice(m_pContext,   &slparams,   m_iSliceBufIndex,
                                                          0, DataSize, 0,   chopType);

                                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                                if (UMC_OK != m_va->Execute())
                                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                                BytesAlreadyExecuted += DataSize;

                                m_pPacker.VC1SetSliceBuffer();
                                m_pPacker.VC1SetPictureBuffer();
                                m_iSliceBufIndex = 0;

                                m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                                RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext, DataSize,
                                                                              pOriginalData + BytesAlreadyExecuted,
                                                                              RemBytesInSlice,
                                                                              0, Flag_03);
                                chopType = 3;
                                slparams.m_bitOffset = 0;
                                Offset = 0;
                            }

                            m_pPacker.VC1PackOneSlice(m_pContext, &slparams,  m_iSliceBufIndex,
                                                      0, DataSize, 0, 2);

                            m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                            m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                            if (UMC_OK != m_va->Execute())
                                throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                            m_iSliceBufIndex = 0;

                            // we need receive buffers from drv for future processing
                            if (MBEndRowOfAlreadyExec != m_pContext->m_seqLayerHeader.heightMB)
                            {
                                m_pPacker.VC1SetSliceBuffer();
                                m_pPacker.VC1SetPictureBuffer();
                                m_pPacker.VC1SetExtPictureBuffer();
                            }
                        }
                        else
                        {
                            m_pPacker.VC1PackOneSlice(m_pContext,
                                                      &slparams,
                                                      m_iSliceBufIndex,
                                                      0,
                                                      DataSize,
                                                      0, 0);
                            BitstreamDataSize += DataSize;
                            ++m_iSliceBufIndex;
                        }
                    }

                    ++pOffsets;
                    ++pValues;
                    ++pOriginalOffsets;
                }
                //Slices
                else if (*(pValues) == 0x0B010000)
                {
                    m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *pOffsets);
                    VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,32, temp_value);
                    m_pContext->m_bitstream.bitOffset = 31;
                    pPictHeader = m_pContext->m_bitstream.pBitstream;
                    PicHeaderSize = 0;

                    VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,9, slparams.MBStartRow);     //SLICE_ADDR
                    VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,1, PicHeaderFlag);            //PIC_HEADER_FLAG

                    if (PicHeaderFlag == 1)                //PIC_HEADER_FLAG
                    {
                        ++m_pContext->m_picLayerHeader;
                        if (isSecondField)
                            m_pContext->m_picLayerHeader->CurrField = 1;
                        else
                            m_pContext->m_picLayerHeader->CurrField = 0;

                        DecodePictureHeader_Adv(m_pContext);
                        DecodePicHeader(m_pContext);
                        PicHeaderSize = m_pPacker.VC1GetPicHeaderSize(pOriginalData + 4 + *pOriginalOffsets,
                            (Ipp32u)((size_t)sizeof(Ipp32u)*(m_pContext->m_bitstream.pBitstream - pPictHeader)), Flag_03);

                        ++m_iPicBufIndex;
                    }
                    m_pContext->m_picLayerHeader->is_slice = 1;

                    if (*(pOffsets+1) && *(pValues+1) == 0x0B010000)
                    {
                        bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets+1));
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
                        bitoffset = 31;
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);
                    }
                    else if(*(pValues+1) == 0x0C010000)
                        slparams.MBEndRow = (m_pContext->m_seqLayerHeader.heightMB+1)/2;
                    else
                        slparams.MBEndRow = m_pContext->m_seqLayerHeader.heightMB;

                    slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
                    slparams.m_bitOffset = m_pContext->m_bitstream.bitOffset;
                    Offset = PicHeaderSize + 4;

                    if (isSecondField)
                        slparams.MBStartRow -= (m_pContext->m_seqLayerHeader.heightMB+1)/2;

                    if (*(pOffsets+1))
                        SliceSize = *(pOriginalOffsets+1) - *pOriginalOffsets - Offset;
                    else
                        SliceSize = m_pContext->m_FrameSize - *pOriginalOffsets - Offset;

                    {
                        Ipp32u chopType = 1;
                        RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                                                      DataSize,
                                                                      pOriginalData + *pOriginalOffsets + Offset,
                                                                      SliceSize,
                                                                      BitstreamDataSize, Flag_03);
                        if (RemBytesInSlice)
                        {
                            MBEndRowOfAlreadyExec = slparams.MBEndRow;
                            Ipp32u BytesAlreadyExecuted = 0;

                            // fill slices
                            while (RemBytesInSlice) // we don't have enough buffer - execute it
                            {
                                m_pPacker.VC1PackOneSlice(m_pContext,    &slparams,
                                                          m_iSliceBufIndex,  0,
                                                          DataSize,          0,   chopType);

                                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);

                                m_va->Execute();
                                BytesAlreadyExecuted += DataSize;

                                m_pPacker.VC1SetSliceBuffer();
                                m_pPacker.VC1SetPictureBuffer();
                                m_pPacker.VC1SetExtPictureBuffer();

                                m_iSliceBufIndex = 0;

                                m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                                RemBytesInSlice  = m_pPacker.VC1PackBitStream(m_pContext,
                                                                              DataSize,
                                                                              pOriginalData + *pOriginalOffsets + BytesAlreadyExecuted + Offset + 4,
                                                                              RemBytesInSlice, 0, Flag_03);

                                chopType = 3;
                                slparams.m_bitOffset = 0;
                                Offset = 0;
                            }

                            m_pPacker.VC1PackOneSlice(m_pContext,       &slparams,
                                                      m_iSliceBufIndex, 0,
                                                      DataSize,         0, 2);
                            m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex+1,m_iPicBufIndex);
                            m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                            m_va->Execute();

                            m_iSliceBufIndex = 0;

                            // we need receive buffers from drv for future processing
                            if (MBEndRowOfAlreadyExec != m_pContext->m_seqLayerHeader.heightMB)
                            {
                                m_pPacker.VC1SetSliceBuffer();
                                m_pPacker.VC1SetPictureBuffer();
                                m_pPacker.VC1SetExtPictureBuffer();
                            }
                        }
                        else
                        {
                            m_pPacker.VC1PackOneSlice(m_pContext,         &slparams,
                                                      m_iSliceBufIndex,   0,
                                                      DataSize,           BitstreamDataSize, 0);
                            // let execute in case of fields
                            if (*(pValues+1)== 0x0C010000)
                            {
                                // Execute first slice
                                m_pPacker.VC1PackBitplaneBuffers(m_pContext);

                                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex + 1,m_iPicBufIndex);

                                if (UMC_OK != m_va->Execute())
                                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                                m_iSliceBufIndex = 0;
                            }
                            else
                            {
                                BitstreamDataSize += DataSize/*SliceSize - RemBytesInSlice*/;
                                ++m_iSliceBufIndex;
                            }
                        }
                    }

                    ++pOffsets;
                    ++pValues;
                    ++pOriginalOffsets;

                }
                else
                {
                    ++pOffsets;
                    ++pValues;
                    ++pOriginalOffsets;
                }

            }
            //need to execute last slice
            if (MBEndRowOfAlreadyExec != m_pContext->m_seqLayerHeader.heightMB)
            {
                m_pPacker.VC1PackBitplaneBuffers(m_pContext);
                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex,m_iPicBufIndex); // +1 !!!!!!!1
                if (UMC_OK != m_va->Execute())
                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);

                if (m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ||
                    m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG/* ||
                    pContext->m_picLayerHeader->RANGEREDFRM*/)
                {
                    if (UMC_OK != m_va->EndFrame())
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                }
            }
         }

         //check correctness of encrypted data
        void CheckData(void)
        {
             mfxBitstream * bs = m_va->GetProtectedVA()->GetBitstream();

            if (!bs->EncryptedData) //  no encryption mode
                return;

            mfxEncryptedData * temp = bs->EncryptedData;
            for (; temp; )
            {
                if(temp->Next)
                {
                    Ipp64u counter1 = temp->CipherCounter.Count;
                    Ipp64u counter2 = temp->Next->CipherCounter.Count;
                    Ipp64u zero = 0xffffffffffffffff;
                    if((m_va->GetProtectedVA())->GetCounterMode() == PAVP_COUNTER_TYPE_A)
                    {
                        counter1 = LITTLE_ENDIAN_SWAP32((DWORD)(counter1 >> 32));
                        counter2 = LITTLE_ENDIAN_SWAP32((DWORD)(counter2 >> 32));
                        zero = 0x00000000ffffffff;
                    }
                    else
                    {
                        counter1 = LITTLE_ENDIAN_SWAP64(counter1);
                        counter2 = LITTLE_ENDIAN_SWAP64(counter2);
                    }
                    //second slice offset starts after the first, there is no counter overall
                    if(counter1 <= counter2)
                    {
                        Ipp32u size = temp->DataLength + temp->DataOffset;
                        if(size & 0xf)
                            size= size+(0x10 - (size & 0xf));
                        Ipp32u sizeCount = (Ipp32u)(counter2 - counter1) *16;
                        //if we have hole between slices
                        if(sizeCount > size)
                            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                        else //or overlap between slices is different in different buffers
                            if(memcmp(temp->Data + sizeCount, temp->Next->Data, size - sizeCount))
                                throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                    }
                    else //there is counter overall or second slice offset starts earlier then the first one
                    {
                        //length of the second slice offset in 16-bytes blocks
                        Ipp64u offsetCounter = (temp->Next->DataOffset + 15)/16;
                        // no counter2 overall
                        if(zero - counter2 > offsetCounter)
                        {
                            Ipp32u size = temp->DataLength + temp->DataOffset;
                            if(size & 0xf)
                                size= size+(0x10 - (size & 0xf));
                            Ipp64u counter3 = size / 16;
                            // no zero during the first slice
                            if(zero - counter1 >= counter3)
                            {
                                //second slice offset contains the first slice with offset
                                if(counter2 + offsetCounter >= counter3)
                                {
                                    temp->Next->Data += (Ipp32u)(counter1 - counter2) * 16;
                                    temp->Next->DataOffset -= (Ipp32u)(counter1 - counter2) * 16;
                                    temp->Next->CipherCounter = temp->CipherCounter;
                                    //overlap between slices is different in different buffers
                                    if (memcmp(temp->Data, temp->Next->Data, temp->DataOffset + temp->DataLength))
                                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                                }
                                else //overlap between slices data or hole between slices
                                {
                                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                                }
                            }
                            else
                            {
                                //size of data between counters
                                Ipp32u sizeCount = (Ipp32u)(counter2 + (zero - counter1)) *16;
                                //hole between slices
                                if(sizeCount > size)
                                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                                else //or overlap between slices is different in different buffers
                                    if(memcmp(temp->Data + sizeCount, temp->Next->Data, size - sizeCount))
                                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                            }
                        }
                        else
                        {
                            temp->Next->Data += (Ipp32u)(counter1 - counter2) * 16;
                            temp->Next->DataOffset -= (Ipp32u)(counter1 - counter2) * 16;
                            temp->Next->CipherCounter = temp->CipherCounter;
                            //overlap between slices is different in different buffers
                            if (memcmp(temp->Data, temp->Next->Data, temp->DataOffset + temp->DataLength))
                                throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                        }
                    }
                }
                temp = temp->Next;
            }
        }


       virtual void VC1PackSlices_Encrypt   (Ipp32u*  pOffsets,Ipp32u*  pValues, Ipp8u* pOriginalData,MediaDataEx::_MediaDataEx* pOrigStCodes)
        {
            VM_ASSERT(VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE);

            if (VC1_PROFILE_ADVANCED != m_pContext->m_seqLayerHeader.PROFILE)
                return;

#ifdef VC1_DEBUG_ON_LOG
            static Ipp32u Num = 0;

            printf("\n\n\nPicture type %d  %d\n\n\n", m_pContext->m_picLayerHeader->PTYPE, Num);
            Num++;

        static FILE* f = 0;
        static int num = 0;
#endif

            if (m_pContext->m_picLayerHeader->PTYPE == VC1_SKIPPED_FRAME)
            {
                m_bIsSkippedFrame = true;
                m_bIsReadyToProcess = false;
                return;
            }
            else
            {
                m_bIsSkippedFrame = false;
            }


            mfxBitstream * bs = 0;
            mfxEncryptedData * encryptedData = 0;

            bs = m_va->GetProtectedVA()->GetBitstream();
            encryptedData = bs->EncryptedData;


            bool isSecondField = false;
            m_iSliceBufIndex = 0;
            m_iPicBufIndex = 0;
            Ipp32u IntCompField = 0;
            Ipp32u temp_value = 0;
            Ipp32u* bitstream = 0;
            Ipp32u PicHeaderFlag = 0;
            Ipp32u MBEndRowOfAlreadyExec = 0;
            Ipp32u BytesAlreadyExecuted = 0;
            Ipp8u *pBuf = 0;

            Ipp32s bitoffset = 31;

            SliceParams slparams;
            memset(&slparams,0,sizeof(SliceParams));
            slparams.MBStartRow = 0;
            slparams.MBEndRow = m_pContext->m_seqLayerHeader.heightMB;
            m_pContext->m_picLayerHeader->CurrField = 0;
            Ipp32u encryptedBufferSize = 0;
            Ipp32u* pOriginalOffsets = pOrigStCodes->offsets;

             mfxEncryptedData * temp = bs->EncryptedData;
             for (; temp; )
             {
                 if(temp->Next)
                {
                    Ipp64u counter1 = temp->CipherCounter.Count;
                    Ipp64u counter2 = temp->Next->CipherCounter.Count;
                    Ipp64u zero = 0xffffffffffffffff;
                    if((m_va->GetProtectedVA())->GetCounterMode() == PAVP_COUNTER_TYPE_A)
                    {
                        counter1 = LITTLE_ENDIAN_SWAP32((DWORD)(counter1 >> 32));
                        counter2 = LITTLE_ENDIAN_SWAP32((DWORD)(counter2 >> 32));
                        zero = 0x00000000ffffffff;
                    }
                    else
                    {
                        counter1 = LITTLE_ENDIAN_SWAP64(counter1);
                        counter2 = LITTLE_ENDIAN_SWAP64(counter2);
                    }
                    if(counter1 <= counter2)
                        encryptedBufferSize += (Ipp32u)(counter2 - counter1) *16;
                    else
                        encryptedBufferSize += (Ipp32u)(counter2 + (zero - counter1)) *16;
                }
                else
                {
                    int size = temp->DataLength + temp->DataOffset;
                    if(size & 0xf)
                        size= size + (0x10 - (size & 0xf));

                    encryptedBufferSize += size;
                }

                temp = temp->Next;
             }

            DXVA2_DecodeExtensionData  extensionData = {0};
            DXVA_EncryptProtocolHeader extensionOutput = {0};
            HRESULT hr = S_OK;

            if(NULL != m_va->GetProtectedVA())
                CheckData();
            if(NULL != m_va->GetProtectedVA() && MFX_PROTECTION_PAVP == m_va->GetProtectedVA()->GetProtected())
            {
                DXVA_Intel_Pavp_Protocol2  extensionInput = {0};
                extensionInput.EncryptProtocolHeader.dwFunction = 0xffff0001;
                extensionInput.EncryptProtocolHeader.guidEncryptProtocol = m_va->GetProtectedVA()->GetEncryptionGUID();
                extensionInput.dwBufferSize = encryptedBufferSize;
                memcpy_s(extensionInput.dwAesCounter, sizeof(encryptedData->CipherCounter), &encryptedData->CipherCounter, sizeof(encryptedData->CipherCounter));

                extensionInput.PavpEncryptionMode.eEncryptionType = (PAVP_ENCRYPTION_TYPE) m_va->GetProtectedVA()->GetEncryptionMode();
                extensionInput.PavpEncryptionMode.eCounterMode = (PAVP_COUNTER_TYPE) m_va->GetProtectedVA()->GetCounterMode();

                extensionData.pPrivateInputData = &extensionInput;
                extensionData.pPrivateOutputData = &extensionOutput;
                extensionData.PrivateInputDataSize = sizeof(extensionInput);
                extensionData.PrivateOutputDataSize = sizeof(extensionOutput);
                hr = m_va->ExecuteExtensionBuffer(&extensionData);

                if (FAILED(hr) ||
                    extensionOutput.guidEncryptProtocol != m_va->GetProtectedVA()->GetEncryptionGUID() ||
                    extensionOutput.dwFunction != 0xffff0801)
                {
                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                }
            }

            //skip start codes till Frame Header
            while ((*pValues)&&(*pValues != 0x0D010000))
            {
                ++pOffsets;
                ++pValues;
                ++pOriginalOffsets;
            }

            DecodePicHeader(m_pContext);

            IntCompField = m_pContext->m_picLayerHeader->INTCOMFIELD;


            m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);

            if (*(pValues+1) == 0x0B010000)
            {
                //slice
                bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets+1));
                VC1BitstreamParser::GetNBits(bitstream, bitoffset,32, temp_value);
                bitoffset = 31;
                VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);     //SLICE_ADDR
                m_pContext->m_picLayerHeader->is_slice = 1;
            }
            else if(*(pValues+1) == 0x0C010000)
            {
                //field
                slparams.MBEndRow = (m_pContext->m_seqLayerHeader.heightMB+1)/2;
            }

            slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
            slparams.m_bitOffset    = m_pContext->m_bitstream.bitOffset;

            {
                //bitstream packing
                {
                    UMCVACompBuffer* CompBuf;

                    Ipp8u* pBitstream = (Ipp8u*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);
                    pBuf=pBitstream;

                    if(NULL != m_va->GetProtectedVA() && IS_PROTECTION_GPUCP_ANY(m_va->GetProtectedVA()->GetProtected()))
                        CompBuf->SetPVPState(&encryptedData->CipherCounter, sizeof (encryptedData->CipherCounter));
                    else
                        CompBuf->SetPVPState(NULL, 0);

                    Ipp32u alignedSize = encryptedData->DataLength + encryptedData->DataOffset;
                    if(alignedSize & 0xf)
                        alignedSize = alignedSize+(0x10 - (alignedSize & 0xf));

                    ippsCopy_8u(encryptedData->Data, pBuf, alignedSize);
#ifdef VC1_DEBUG_ON_LOG
                    {
                        if(!f)
                            f = fopen("BS_sample_enc.txt", "wb");
                        if(f)
                        {
                        fprintf(f, "Size = %d\n", encryptedData->DataLength);
                        fprintf(f, "ByteOffset = %d\n",  encryptedData->DataOffset);

                        for(mfxU32 k = 0; k < encryptedData->DataLength; k++)
                        {
                            fprintf(f, "%x ", *(encryptedData->Data + encryptedData->DataOffset + k));

                           if(k%10 == 0)
                               fprintf(f, "\n");
                        }

                        fprintf(f, "\n\n\n");
                        }
                    }
#endif
                    Ipp32u diff = (Ipp32u)(pBitstream - pBuf) + BytesAlreadyExecuted;
                    m_pPacker.VC1PackOneSlice(m_pContext, &slparams, m_iSliceBufIndex, 0, 
                        encryptedData->DataLength, BytesAlreadyExecuted -diff +encryptedData->DataOffset, 0);
                    BytesAlreadyExecuted += (UINT)(alignedSize - diff);
                    CompBuf->SetDataSize(BytesAlreadyExecuted);
                    if(encryptedData->Next)
                    {
                        Ipp64u counter1 = encryptedData->CipherCounter.Count;
                        Ipp64u counter2 = encryptedData->Next->CipherCounter.Count;
                        Ipp64u zero = 0xffffffffffffffff;
                        if((m_va->GetProtectedVA())->GetCounterMode() == PAVP_COUNTER_TYPE_A)
                        {
                            counter1 = LITTLE_ENDIAN_SWAP32((DWORD)(counter1 >> 32));
                            counter2 = LITTLE_ENDIAN_SWAP32((DWORD)(counter2 >> 32));
                            zero = 0x00000000ffffffff;
                        }
                        else
                        {
                            counter1 = LITTLE_ENDIAN_SWAP64(counter1);
                            counter2 = LITTLE_ENDIAN_SWAP64(counter2);
                        }
                        if(counter2 < counter1)
                        {
                            (unsigned char*)pBuf += (Ipp32u)(counter2 +(zero - counter1))*16;
                        }
                        else
                        {
                            (unsigned char*)pBuf += (Ipp32u)(counter2 - counter1)*16;
                        }
                    }
                }
                {

                    // let execute in case of fields
                    if (*(pValues+1)== 0x0C010000)
                    {
                        // Execute first slice
                        m_pPacker.VC1PackBitplaneBuffers(m_pContext);
                        m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex + 1,m_iPicBufIndex);

                        if (UMC_OK != m_va->Execute())
                            throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);

                        BytesAlreadyExecuted = 0;
                        m_iSliceBufIndex = 0;
                    }
                    else
                    {
                        //BitstreamDataSize += encryptedData->DataLength;
                        ++m_iSliceBufIndex;
                    }
                    encryptedData = encryptedData->Next;
                }
            }
            ++pOffsets;
            ++pValues;
            ++pOriginalOffsets;


            while (*pOffsets)
            {
                //Fields
                if (*(pValues) == 0x0C010000)
                {
                    //m_pContext->m_bitstream.pBitstream = pFirstFieldStartCode;

                    if (UMC_OK != m_va->EndFrame())
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                    if (UMC_OK != m_va->BeginFrame(m_pContext->m_frmBuff.m_iCurrIndex))
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);

                    if(NULL != m_va->GetProtectedVA())
                        CheckData();

                    if(NULL != m_va->GetProtectedVA() && MFX_PROTECTION_PAVP == m_va->GetProtectedVA()->GetProtected())
                    {
                        DXVA_Intel_Pavp_Protocol2  extensionInput = {0};
                        extensionInput.EncryptProtocolHeader.dwFunction = 0xffff0001;
                        extensionInput.EncryptProtocolHeader.guidEncryptProtocol = m_va->GetProtectedVA()->GetEncryptionGUID();
                        extensionInput.dwBufferSize = encryptedBufferSize;
                        memcpy_s(extensionInput.dwAesCounter, sizeof(encryptedData->CipherCounter), &encryptedData->CipherCounter, sizeof(encryptedData->CipherCounter));

                        extensionInput.PavpEncryptionMode.eEncryptionType = (PAVP_ENCRYPTION_TYPE) m_va->GetProtectedVA()->GetEncryptionMode();
                        extensionInput.PavpEncryptionMode.eCounterMode = (PAVP_COUNTER_TYPE) m_va->GetProtectedVA()->GetCounterMode();

                        extensionData.pPrivateInputData = &extensionInput;
                        extensionData.pPrivateOutputData = &extensionOutput;
                        extensionData.PrivateInputDataSize = sizeof(extensionInput);
                        extensionData.PrivateOutputDataSize = sizeof(extensionOutput);
                        hr = m_va->ExecuteExtensionBuffer(&extensionData);
                    }

                    m_pPacker.VC1SetSliceBuffer();
                    m_pPacker.VC1SetPictureBuffer();
                    m_pPacker.VC1SetExtPictureBuffer();
                    isSecondField = true;
                    // set swap bitstream for parsing
                    m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *pOffsets);
                    m_pContext->m_bitstream.pBitstream += 1; // skip start code
                    m_pContext->m_bitstream.bitOffset = 31;

                    ++m_pContext->m_picLayerHeader;
                    *m_pContext->m_picLayerHeader = *m_pContext->m_InitPicLayer;
                    m_pContext->m_picLayerHeader->BottomField = (Ipp8u)m_pContext->m_InitPicLayer->TFF;
                    m_pContext->m_picLayerHeader->PTYPE = m_pContext->m_InitPicLayer->PTypeField2;
                    m_pContext->m_picLayerHeader->CurrField = 1;

                    m_pContext->m_picLayerHeader->is_slice = 0;
                    DecodePicHeader(m_pContext);

                    m_pContext->m_picLayerHeader->INTCOMFIELD = IntCompField;

                    slparams.MBStartRow = (m_pContext->m_seqLayerHeader.heightMB+1)/2;


                    if (*(pOffsets+1) && *(pValues+1) == 0x0B010000)
                    {
                        bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets+1));
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
                        bitoffset = 31;
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);
                    } else
                        slparams.MBEndRow = m_pContext->m_seqLayerHeader.heightMB;

                    m_pPacker.VC1PackPicParamsForOneSlice(m_pContext);
                    ++m_iPicBufIndex;
                    slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
                    slparams.m_bitOffset = m_pContext->m_bitstream.bitOffset;
                    //Offset = PicHeaderSize + 4; // offset in bytes
                    slparams.MBStartRow = 0; //we should decode fields steb by steb

                    {
                        //bitstream packing
                        {
                            UMCVACompBuffer* CompBuf;

                            Ipp8u* pBitstream = (Ipp8u*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);
                            pBuf=pBitstream;

                            if(NULL != m_va->GetProtectedVA() && IS_PROTECTION_GPUCP_ANY(m_va->GetProtectedVA()->GetProtected()))
                                CompBuf->SetPVPState(&encryptedData->CipherCounter, sizeof (encryptedData->CipherCounter));
                            else
                                CompBuf->SetPVPState(NULL, 0);

                            Ipp32u alignedSize = encryptedData->DataLength + encryptedData->DataOffset;
                            if(alignedSize & 0xf)
                                alignedSize = alignedSize+(0x10 - (alignedSize & 0xf));

                            ippsCopy_8u(encryptedData->Data, pBuf, alignedSize);
#ifdef VC1_DEBUG_ON_LOG
                            {
                                if(!f)
                                    f = fopen("BS_sample_enc.txt", "wb");
                                if(f)
                                {
                                fprintf(f, "Size = %d\n", encryptedData->DataLength);
                                fprintf(f, "ByteOffset = %d\n",  encryptedData->DataOffset);

                                for(mfxU32 k = 0; k < encryptedData->DataLength; k++)
                                {
                                    fprintf(f, "%x ", *(encryptedData->Data + encryptedData->DataOffset + k));

                                   if(k%10 == 0)
                                       fprintf(f, "\n");
                                }

                                fprintf(f, "\n\n\n");
                                }
                            }
#endif
                            Ipp32u diff = (Ipp32u)(pBitstream - pBuf) + BytesAlreadyExecuted;
                            CompBuf->SetDataSize(BytesAlreadyExecuted + (UINT)(alignedSize - diff));
                            m_pPacker.VC1PackOneSlice(m_pContext, &slparams, m_iSliceBufIndex, 0,
                                encryptedData->DataLength, BytesAlreadyExecuted -diff +encryptedData->DataOffset, 0);
                            BytesAlreadyExecuted += (UINT)(alignedSize - diff);
                            if(encryptedData->Next)
                            {
                                Ipp64u counter1 = encryptedData->CipherCounter.Count;
                                Ipp64u counter2 = encryptedData->Next->CipherCounter.Count;
                                Ipp64u zero = 0xffffffffffffffff;
                                if((m_va->GetProtectedVA())->GetCounterMode() == PAVP_COUNTER_TYPE_A)
                                {
                                    counter1 = LITTLE_ENDIAN_SWAP32((DWORD)(counter1 >> 32));
                                    counter2 = LITTLE_ENDIAN_SWAP32((DWORD)(counter2 >> 32));
                                    zero = 0x00000000ffffffff;
                                }
                                else
                                {
                                    counter1 = LITTLE_ENDIAN_SWAP64(counter1);
                                    counter2 = LITTLE_ENDIAN_SWAP64(counter2);
                                }
                                if(counter2 < counter1)
                                {
                                    (unsigned char*)pBuf += (Ipp32u)(counter2 +(zero - counter1))*16;
                                }
                                else
                                {
                                    (unsigned char*)pBuf += (Ipp32u)(counter2 - counter1)*16;
                                }
                            }
                        }

                           //BitstreamDataSize += encryptedData->DataLength;
                            ++m_iSliceBufIndex;
                            encryptedData = encryptedData->Next;
                    }

                    ++pOffsets;
                    ++pValues;
                    ++pOriginalOffsets;
                }
                //Slices
                else if (*(pValues) == 0x0B010000)
                {
                    m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *pOffsets);
                    VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,32, temp_value);
                    m_pContext->m_bitstream.bitOffset = 31;

                    VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,9, slparams.MBStartRow);     //SLICE_ADDR
                    VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream,m_pContext->m_bitstream.bitOffset,1, PicHeaderFlag);            //PIC_HEADER_FLAG

                    if (PicHeaderFlag == 1)                //PIC_HEADER_FLAG
                    {
                        ++m_pContext->m_picLayerHeader;
                        if (isSecondField)
                            m_pContext->m_picLayerHeader->CurrField = 1;
                        else
                            m_pContext->m_picLayerHeader->CurrField = 0;

                        DecodePictureHeader_Adv(m_pContext);
                        DecodePicHeader(m_pContext);

                        ++m_iPicBufIndex;
                    }
                    m_pContext->m_picLayerHeader->is_slice = 1;

                    if (*(pOffsets+1) && *(pValues+1) == 0x0B010000)
                    {
                        bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets+1));
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,32, temp_value);
                        bitoffset = 31;
                        VC1BitstreamParser::GetNBits(bitstream,bitoffset,9, slparams.MBEndRow);
                    }
                    else if(*(pValues+1) == 0x0C010000)
                        slparams.MBEndRow = (m_pContext->m_seqLayerHeader.heightMB+1)/2;
                    else
                        slparams.MBEndRow = m_pContext->m_seqLayerHeader.heightMB;

                    slparams.MBRowsToDecode = slparams.MBEndRow-slparams.MBStartRow;
                    slparams.m_bitOffset = m_pContext->m_bitstream.bitOffset;
                    //Offset = PicHeaderSize + 4;

                    if (isSecondField)
                        slparams.MBStartRow -= (m_pContext->m_seqLayerHeader.heightMB+1)/2;


                    {
                        //bitstream packing
                        {
                            UMCVACompBuffer* CompBuf;

                            Ipp8u* pBitstream = (Ipp8u*)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &CompBuf);

                            if(NULL != m_va->GetProtectedVA() && IS_PROTECTION_GPUCP_ANY(m_va->GetProtectedVA()->GetProtected()))
                                CompBuf->SetPVPState(&encryptedData->CipherCounter, sizeof (encryptedData->CipherCounter));
                            else
                                CompBuf->SetPVPState(NULL, 0);

                            Ipp32u alignedSize = encryptedData->DataLength + encryptedData->DataOffset;
                            if(alignedSize & 0xf)
                                alignedSize = alignedSize+(0x10 - (alignedSize & 0xf));

                            ippsCopy_8u(encryptedData->Data, pBuf, alignedSize);
#ifdef VC1_DEBUG_ON_LOG
                            {
                                if(!f)
                                    f = fopen("BS_sample_enc.txt", "wb");
                                if(f)
                                {
                                fprintf(f, "Size = %d\n", encryptedData->DataLength);
                                fprintf(f, "ByteOffset = %d\n",  encryptedData->DataOffset);

                                for(mfxU32 k = 0; k < encryptedData->DataLength; k++)
                                {
                                    fprintf(f, "%x ", *(encryptedData->Data + encryptedData->DataOffset + k));

                                   if(k%10 == 0)
                                       fprintf(f, "\n");
                                }

                                fprintf(f, "\n\n\n");
                                }
                            }
#endif
                            Ipp32u diff = (Ipp32u)(pBitstream - pBuf) + BytesAlreadyExecuted;
                            CompBuf->SetDataSize(BytesAlreadyExecuted + (UINT)(alignedSize - diff));
                            m_pPacker.VC1PackOneSlice(m_pContext, &slparams, m_iSliceBufIndex, 0, encryptedData->DataLength, BytesAlreadyExecuted -diff +encryptedData->DataOffset, 0);
                            BytesAlreadyExecuted += alignedSize - diff;
                            if(encryptedData->Next)
                            {
                                Ipp64u counter1 = encryptedData->CipherCounter.Count;
                                Ipp64u counter2 = encryptedData->Next->CipherCounter.Count;
                                Ipp64u zero = 0xffffffffffffffff;
                                if((m_va->GetProtectedVA())->GetCounterMode() == PAVP_COUNTER_TYPE_A)
                                {
                                    counter1 = LITTLE_ENDIAN_SWAP32((DWORD)(counter1 >> 32));
                                    counter2 = LITTLE_ENDIAN_SWAP32((DWORD)(counter2 >> 32));
                                    zero = 0x00000000ffffffff;
                                }
                                else
                                {
                                    counter1 = LITTLE_ENDIAN_SWAP64(counter1);
                                    counter2 = LITTLE_ENDIAN_SWAP64(counter2);
                                }
                                if(counter2 < counter1)
                                {
                                    (unsigned char*)pBuf += (Ipp32u)(counter2 + (zero - counter1))*16;
                                }
                                else
                                {
                                    (unsigned char*)pBuf += (Ipp32u)(counter2 - counter1)*16;
                                }
                           }
                        }

                        {


                            // let execute in case of fields
                            if (*(pValues+1)== 0x0C010000)
                            {
                                // Execute first slice
                                m_pPacker.VC1PackBitplaneBuffers(m_pContext);

                                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex + 1,m_iPicBufIndex);

                                if (UMC_OK != m_va->Execute())
                                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                                m_iSliceBufIndex = 0;
                            }
                            else
                            {
                              //  BitstreamDataSize +=  encryptedData->Next/*SliceSize - RemBytesInSlice*/;
                                ++m_iSliceBufIndex;
                            }
                            encryptedData = encryptedData->Next;
                        }
                    }

                    ++pOffsets;
                    ++pValues;
                    ++pOriginalOffsets;

                }
                else
                {
                    ++pOffsets;
                    ++pValues;
                    ++pOriginalOffsets;
                }

            }
            //need to execute last slice
            if (MBEndRowOfAlreadyExec != m_pContext->m_seqLayerHeader.heightMB)
            {
                m_pPacker.VC1PackBitplaneBuffers(m_pContext);
                m_pPacker.VC1SetBuffersSize(m_iSliceBufIndex,m_iPicBufIndex); // +1 !!!!!!!1
                if (UMC_OK != m_va->Execute())
                    throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);

                if (m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ||
                    m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG/* ||
                                                                 pContext->m_picLayerHeader->RANGEREDFRM*/)
                {
                    if (UMC_OK != m_va->EndFrame())
                        throw VC1Exceptions::vc1_exception(VC1Exceptions::internal_pipeline_error);
                }
            }
        }
    };
#endif //UMC_VA_DXVA
} // namespace UMC

#endif // #if defined(UMC_VA)

#endif //__UMC_VC1_DEC_FRAME_DESCR_VA_H_
#endif //UMC_ENABLE_VC1_VIDEO_DECODER

