/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_segment_decoder_mt.h"
#include "h265_coding_unit.h"
#include "umc_h265_frame.h"
#include "umc_h265_frame_info.h"
#include "h265_global_rom.h"

#include <cstdarg>

#if defined(_WIN32) || defined(_WIN64)
  #define H265_ALIGNED_MALLOC _aligned_malloc
  #define H265_ALIGNED_FREE _aligned_free
#else
  // TODO: aligned_alloc is available in C11, do we need to use it?
  #define H265_ALIGNED_MALLOC(M,A) malloc(M)
  #define H265_ALIGNED_FREE free
#endif

namespace UMC_HEVC_DECODER
{


Ipp8u * CumulativeArraysAllocation(int n, ...)
{
    va_list args;
    va_start(args, n);

    int cumulativeSize = 0;
    for (int i = 0; i < n; i++)
    {
        void * ptr = va_arg(args, void *);
        ptr; // just skip it

        int currSize = va_arg(args, int);
        cumulativeSize += currSize;
    }

    va_end(args);

    Ipp8u *cumulativePtr = new Ipp8u[cumulativeSize + 32*n];
    Ipp8u *cumulativePtrSaved = cumulativePtr;

    va_start(args, n);

    for (int i = 0; i < n; i++)
    {
        void ** ptr = va_arg(args, void **);
        *ptr = UMC::align_pointer<void*> (cumulativePtr, 32);

        int currSize = va_arg(args, int);
        cumulativePtr = (Ipp8u*)*ptr + currSize;
    }

    va_end(args);
    return cumulativePtrSaved;
}

// Constructor, destructor, create, destroy -------------------------------------------------------------
H265CodingUnit::H265CodingUnit()
{
    m_cumulativeMemoryPtr = 0;

    m_Frame = NULL;
    m_SliceHeader = NULL;
    m_SliceIdx = -1;
    m_DepthArray = NULL;

    m_PartSizeArray = NULL;
    m_PredModeArray = NULL;
    m_CUTransquantBypass = NULL;
    m_WidthArray = NULL;
    m_HeightArray = NULL;
    m_QPArray = NULL;
    m_MergeFlag = NULL;
    m_MergeIndex = NULL;

    m_LumaIntraDir = NULL;
    m_ChromaIntraDir = NULL;
    m_InterDir = NULL;
    m_TrIdxArray = NULL;
    m_TrStartArray = NULL;
    // ML: FIXED: changed the last 3 into 2
    m_TransformSkip[0] = m_TransformSkip[1] = m_TransformSkip[2] = NULL;
    m_Cbf[0] = NULL;
    m_Cbf[1] = NULL;
    m_Cbf[2] = NULL;
    m_TrCoeffY = NULL;
    m_TrCoeffCb = NULL;
    m_TrCoeffCr = NULL;

    m_IPCMFlag = NULL;
    m_IPCMSampleY = NULL;
    m_IPCMSampleCb = NULL;
    m_IPCMSampleCr = NULL;

    m_Pattern = NULL;

    m_CUAboveLeft = NULL;
    m_CUAboveRight = NULL;
    m_CUAbove = NULL;
    m_CULeft = NULL;

    m_CUColocated[0] = NULL;
    m_CUColocated[1] = NULL;

    m_MVPIdx[0] = NULL;
    m_MVPIdx[1] = NULL;
    m_MVPNum[0] = NULL;
    m_MVPNum[1] = NULL;

    m_DecSubCU = false;
    m_SliceStartCU = 0;
    m_DependentSliceStartCU = 0;
}

H265CodingUnit::~H265CodingUnit()
{
}

void H265CodingUnit::create (Ipp32u numPartition, Ipp32u Width, Ipp32u Height, bool DecSubCu, Ipp32s unitSize)
{
    m_DecSubCU = DecSubCu;

    m_Frame = NULL;
    m_SliceHeader = NULL;
    m_NumPartition = numPartition;
    m_UnitSize = unitSize;

    if (!DecSubCu)
    {
         m_cumulativeMemoryPtr = CumulativeArraysAllocation(32,  &m_QPArray, sizeof(Ipp8u) * numPartition,
                                        &m_DepthArray, sizeof(Ipp8u) * numPartition,
                                        &m_WidthArray, sizeof(Ipp8u) * numPartition,
                                        &m_HeightArray, sizeof(Ipp8u) * numPartition,
                                        &m_PartSizeArray, sizeof(Ipp8u) * numPartition,
                                        &m_PredModeArray, sizeof(Ipp8u) * numPartition,

                                        &m_skipFlag, sizeof(bool) * numPartition,
                                        &m_CUTransquantBypass, sizeof(bool) * numPartition,

                                        &m_MergeFlag, sizeof(bool) * numPartition,
                                        &m_MergeIndex, sizeof(bool) * numPartition,

                                        &m_LumaIntraDir, sizeof(Ipp8u) * numPartition,
                                        &m_ChromaIntraDir, sizeof(Ipp8u) * numPartition,
                                        &m_InterDir, sizeof(Ipp8u) * numPartition,

                                        &m_TrIdxArray, sizeof(Ipp8u) * numPartition,
                                        &m_TrStartArray, sizeof(Ipp8u) * numPartition,
                                        &m_TransformSkip[0], sizeof(Ipp8u) * numPartition,
                                        &m_TransformSkip[1], sizeof(Ipp8u) * numPartition,
                                        &m_TransformSkip[2], sizeof(Ipp8u) * numPartition,

                                        &m_TrCoeffY, sizeof(H265CoeffsCommon) * Width * Height,
                                        &m_TrCoeffCb, sizeof(H265CoeffsCommon) * Width * Height / 4,
                                        &m_TrCoeffCr, sizeof(H265CoeffsCommon) * Width * Height / 4,

                                        &m_Cbf[0], sizeof(Ipp8u) * numPartition,
                                        &m_Cbf[1], sizeof(Ipp8u) * numPartition,
                                        &m_Cbf[2], sizeof(Ipp8u) * numPartition,

                                        &m_MVPIdx[0], sizeof(Ipp8s) * numPartition,
                                        &m_MVPIdx[1], sizeof(Ipp8s) * numPartition,
                                        &m_MVPNum[0], sizeof(Ipp8s) * numPartition,
                                        &m_MVPNum[1], sizeof(Ipp8s) * numPartition,

                                        &m_IPCMFlag, sizeof(bool) * numPartition,
                                        &m_IPCMSampleY, sizeof(H265PlaneYCommon) * Width * Height,
                                        &m_IPCMSampleCb, sizeof(H265PlaneYCommon) * Width * Height / 4,
                                        &m_IPCMSampleCr, sizeof(H265PlaneYCommon) * Width * Height / 4

                                      );

        /*m_QPArray = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);
        m_DepthArray = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);
        m_WidthArray = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);
        m_HeightArray = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);
        m_PartSizeArray = new Ipp8s[numPartition];
        m_PredModeArray = new Ipp8s[numPartition];

        m_skipFlag = (bool*) H265_ALIGNED_MALLOC(sizeof(bool) * numPartition, 32);
        m_CUTransquantBypass = (bool*) H265_ALIGNED_MALLOC(sizeof(bool) * numPartition, 32);

        m_MergeFlag = (bool*) H265_ALIGNED_MALLOC(sizeof(bool) * numPartition, 32);
        m_MergeIndex = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(bool) * numPartition, 32);

        m_LumaIntraDir = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);
        m_ChromaIntraDir = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);
        m_InterDir = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);

        m_TrIdxArray = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);
        m_TrStartArray = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);
        m_TransformSkip[0] = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);
        m_TransformSkip[1] = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);
        m_TransformSkip[2] = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);

        m_TrCoeffY = (H265CoeffsPtrCommon)H265_ALIGNED_MALLOC(sizeof(Ipp32s) * Width * Height, 32);
        m_TrCoeffCb = (H265CoeffsPtrCommon)H265_ALIGNED_MALLOC(sizeof(Ipp32s) * Width * Height / 4, 32);
        m_TrCoeffCr = (H265CoeffsPtrCommon)H265_ALIGNED_MALLOC(sizeof(Ipp32s) * Width * Height / 4, 32);

        m_Cbf[0] = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);
        m_Cbf[1] = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);
        m_Cbf[2] = (Ipp8u*) H265_ALIGNED_MALLOC(sizeof(Ipp8u) * numPartition, 32);

        m_MVPIdx[0] = new Ipp8s[numPartition];
        m_MVPIdx[1] = new Ipp8s[numPartition];
        m_MVPNum[0] = new Ipp8s[numPartition];
        m_MVPNum[1] = new Ipp8s[numPartition];

        m_IPCMFlag = (bool*) H265_ALIGNED_MALLOC(sizeof(bool) * numPartition, 32);
        m_IPCMSampleY = (H265PlanePtrYCommon)H265_ALIGNED_MALLOC(sizeof(Ipp16s) * Width * Height, 32);
        m_IPCMSampleCb = (H265PlanePtrUVCommon)H265_ALIGNED_MALLOC(sizeof(Ipp16s) * Width * Height / 4, 32);
        m_IPCMSampleCr = (H265PlanePtrUVCommon)H265_ALIGNED_MALLOC(sizeof(Ipp16s) * Width * Height / 4, 32);*/

        m_CUMVbuffer[0].create(numPartition);
        m_CUMVbuffer[1].create(numPartition);

    }
    else
    {
        m_CUMVbuffer[0].m_NumPartition = numPartition;
        m_CUMVbuffer[1].m_NumPartition = numPartition;
    }

    m_SliceStartCU = (Ipp32u*) H265_ALIGNED_MALLOC(sizeof(Ipp32u) * numPartition, 32);
    m_DependentSliceStartCU = (Ipp32u*) H265_ALIGNED_MALLOC(sizeof(Ipp32u) * numPartition, 32);

  // create pattern memory
    m_Pattern = (H265Pattern*) H265_ALIGNED_MALLOC(sizeof(H265Pattern), 32);

  // create motion vector fields

    m_CUAboveLeft      = NULL;
    m_CUAboveRight     = NULL;
    m_CUAbove          = NULL;
    m_CULeft           = NULL;

    m_CUColocated[0]  = NULL;
    m_CUColocated[1]  = NULL;
}

void H265CodingUnit::destroy()
{
    m_Frame = NULL;
    m_SliceHeader = NULL;

    if (m_Pattern)
    {
        H265_ALIGNED_FREE(m_Pattern);
        m_Pattern = NULL;
    }

  // encoder-side buffer free
    if (!m_DecSubCU)
    {
        if (m_cumulativeMemoryPtr)
        {
            delete[] m_cumulativeMemoryPtr;
            m_cumulativeMemoryPtr = 0;

            /*H265_ALIGNED_FREE(m_QPArray);
            m_QPArray = NULL;

            H265_ALIGNED_FREE(m_DepthArray);
            m_DepthArray = NULL;

            H265_ALIGNED_FREE(m_WidthArray);
            m_WidthArray = NULL;

            H265_ALIGNED_FREE(m_HeightArray);
            m_HeightArray = NULL;

            delete[] m_PartSizeArray;
            m_PartSizeArray = NULL;

            delete[] m_PredModeArray;
            m_PredModeArray = NULL;

            H265_ALIGNED_FREE(m_skipFlag);
            m_skipFlag = NULL;

            H265_ALIGNED_FREE(m_MergeFlag);
            m_MergeFlag = NULL;

            H265_ALIGNED_FREE(m_MergeIndex);
            m_MergeIndex = NULL;

            H265_ALIGNED_FREE(m_LumaIntraDir);
            m_LumaIntraDir = NULL;

            H265_ALIGNED_FREE(m_InterDir);
            m_InterDir = NULL;

            H265_ALIGNED_FREE(m_ChromaIntraDir);
            m_ChromaIntraDir = NULL;

            H265_ALIGNED_FREE(m_CUTransquantBypass);
            m_CUTransquantBypass = NULL; 

            H265_ALIGNED_FREE(m_TrCoeffY);
            m_TrCoeffY = NULL;

            H265_ALIGNED_FREE(m_TrCoeffCb);
            m_TrCoeffCb = NULL;
 
            H265_ALIGNED_FREE(m_TrCoeffCr);
            m_TrCoeffCr = NULL;


            H265_ALIGNED_FREE(m_TrIdxArray);
            m_TrIdxArray = NULL;

            H265_ALIGNED_FREE(m_TrStartArray);
            m_TrStartArray = NULL;

            for (Ipp32s i = 0; i < 3; i++)
                if (m_TransformSkip[i])
                {
                    H265_ALIGNED_FREE(m_TransformSkip[i]);
                    m_TransformSkip[i] = NULL;
                }

            H265_ALIGNED_FREE(m_Cbf[0]);
            m_Cbf[0] = NULL;
            H265_ALIGNED_FREE(m_Cbf[1]);
            m_Cbf[1] = NULL;
            H265_ALIGNED_FREE(m_Cbf[2]);
            m_Cbf[2] = NULL;

            H265_ALIGNED_FREE(m_IPCMFlag);
            m_IPCMFlag = NULL;

            H265_ALIGNED_FREE(m_IPCMSampleY);
            m_IPCMSampleY = NULL;

            H265_ALIGNED_FREE(m_IPCMSampleCb);
            m_IPCMSampleCb = NULL;

            H265_ALIGNED_FREE(m_IPCMSampleCr);
            m_IPCMSampleCr = NULL;

            delete[] m_MVPIdx[0];
            m_MVPIdx[0] = NULL;

            delete[] m_MVPIdx[1];
            m_MVPIdx[1] = NULL;

            delete[] m_MVPNum[0];
            m_MVPNum[0] = NULL;

            delete[] m_MVPNum[1];
            m_MVPNum[1] = NULL;*/
        }

        m_CUMVbuffer[0].destroy();
        m_CUMVbuffer[1].destroy();
    }

    m_CUAboveLeft = NULL;
    m_CUAboveRight = NULL;
    m_CUAbove = NULL;
    m_CULeft = NULL;

    m_CUColocated[0] = NULL;
    m_CUColocated[1] = NULL;

    if(m_SliceStartCU)
    {
        H265_ALIGNED_FREE(m_SliceStartCU);
        m_SliceStartCU = NULL;
    }
    if(m_DependentSliceStartCU)
    {
        H265_ALIGNED_FREE(m_DependentSliceStartCU);
        m_DependentSliceStartCU = NULL;
    }
}


// Public member functions --------------------------------------------------------------------------------------
// Initialization -----------------------------------------------------------------------------------------------
/*
 - initialize top-level CU
 - internal buffers are already created
 - set values before encoding a CU
 \param  pcPic     picture (TComPic) class pointer
 \param  iCUAddr   CU address
 */
void H265CodingUnit::initCU(H265SegmentDecoderMultiThreaded* sd, Ipp32u iCUAddr)
{
    const H265SeqParamSet* sps = sd->m_pSeqParamSet;
    H265DecoderFrame *Pic = sd->m_pCurrentFrame;

    m_Frame = Pic;
    m_SliceIdx = sd->m_pSlice->GetSliceNum();
    m_SliceHeader = sd->m_pSliceHeader;
    CUAddr = iCUAddr;
    m_CUPelX = (iCUAddr % sps->WidthInCU) * sps->MaxCUWidth; //HM uses g_MaxCUWidth & g_MaxCUHeight
    m_CUPelY = (iCUAddr / sps->WidthInCU) * sps->MaxCUHeight;
    m_AbsIdxInLCU = 0;
    m_NumPartition = sps->NumPartitionsInCU;

    for (Ipp32u i = 0; i < m_NumPartition; i++)
    {
        if (Pic->m_CodingData->GetInverseCUOrderMap(iCUAddr) * Pic->getCD()->getNumPartInCU() + i >= m_SliceHeader->SliceCurStartCUAddr)
        {
            m_SliceStartCU[i] = m_SliceHeader->SliceCurStartCUAddr;
        }
        else
        {
            m_SliceStartCU[i] = Pic->getCU(iCUAddr)->m_SliceStartCU[i];
        }
    }
    for (Ipp32u i = 0; i < m_NumPartition; i++)
    {
        if (Pic->m_CodingData->GetInverseCUOrderMap(iCUAddr) * Pic->getCD()->getNumPartInCU() + i >= m_SliceHeader->SliceCurStartCUAddr)
        {
            m_DependentSliceStartCU[i] = m_SliceHeader->m_sliceSegmentCurStartCUAddr;
        }
        else
        {
            m_DependentSliceStartCU[i] = Pic->getCU(iCUAddr)->m_DependentSliceStartCU[i];
        }
    }

    Ipp32s partStartIdx = m_SliceHeader->m_sliceSegmentCurStartCUAddr - Pic->m_CodingData->GetInverseCUOrderMap(iCUAddr) * Pic->getCD()->getNumPartInCU();

    Ipp32s numElements = IPP_MIN( partStartIdx, (Ipp32s) m_NumPartition );
    for (Ipp32s ind = 0; ind < numElements; ind++)
    {
        H265CodingUnit * From = Pic->getCU(iCUAddr);
        m_skipFlag[ind] = From->m_skipFlag[ind];
        m_PartSizeArray[ind] = From->m_PartSizeArray[ind];
        m_PredModeArray[ind] = From->m_PredModeArray[ind];
        m_CUTransquantBypass[ind] = From->m_CUTransquantBypass[ind];
        m_DepthArray[ind] = From->m_DepthArray[ind];
        m_WidthArray[ind] = From->m_WidthArray[ind];
        m_HeightArray[ind] = From->m_HeightArray[ind];
        m_TrIdxArray[ind] = From->m_TrIdxArray[ind];
        m_TrStartArray[ind] = From->m_TrStartArray[ind];
        for (Ipp32s i = 0; i < 3; i++)
            m_TransformSkip[i][ind] = From->m_TransformSkip[i][ind];
        m_MVPIdx[0][ind] = From->m_MVPIdx[0][ind];;
        m_MVPIdx[1][ind] = From->m_MVPIdx[1][ind];
        m_MVPNum[0][ind] = From->m_MVPNum[0][ind];
        m_MVPNum[1][ind] = From->m_MVPNum[1][ind];
        m_QPArray[ind] = From->m_QPArray[ind];
        m_MergeFlag[ind] = From->m_MergeFlag[ind];
        m_MergeIndex[ind] = From->m_MergeIndex[ind];

        m_LumaIntraDir[ind] = From->m_LumaIntraDir[ind];
        m_ChromaIntraDir[ind] = From->m_ChromaIntraDir[ind];
        m_InterDir[ind] = From->m_InterDir[ind];
        m_Cbf[0][ind] = From->m_Cbf[0][ind];
        m_Cbf[1][ind] = From->m_Cbf[1][ind];
        m_Cbf[2][ind] = From->m_Cbf[2][ind];
        m_IPCMFlag[ind] = From->m_IPCMFlag[ind];
    }

    Ipp32s firstElement = IPP_MAX(partStartIdx, 0);
    numElements = m_NumPartition - firstElement;
    if (numElements > 0)
    {
        memset( m_skipFlag       + firstElement, false,                    numElements * sizeof( *m_skipFlag ) );
        memset( m_PartSizeArray  + firstElement, SIZE_NONE,                numElements * sizeof( *m_PartSizeArray ) );
        memset( m_PredModeArray  + firstElement, MODE_NONE,                numElements * sizeof( *m_PredModeArray ) );
        memset( m_CUTransquantBypass + firstElement, 0,                    numElements * sizeof( *m_CUTransquantBypass ) );
        memset( m_DepthArray     + firstElement, 0,                        numElements * sizeof( *m_DepthArray ) );
        memset( m_TrIdxArray     + firstElement, 0,                        numElements * sizeof( *m_TrIdxArray ) );
        memset( m_TrStartArray   + firstElement, 0,                        numElements * sizeof( *m_TrStartArray ) );
        for (Ipp32s i = 0; i < 3; i++)
            memset (m_TransformSkip[i] + firstElement, 0,                  numElements * sizeof( *m_TransformSkip[i] ) );
        memset( m_WidthArray     + firstElement, g_MaxCUWidth,             numElements * sizeof( *m_WidthArray ) );
        memset( m_HeightArray    + firstElement, g_MaxCUHeight,            numElements * sizeof( *m_HeightArray ) );
        memset( m_MVPIdx[0]      + firstElement, -1,                       numElements * sizeof( *m_MVPIdx[0] ) );
        memset( m_MVPIdx[1]      + firstElement, -1,                       numElements * sizeof( *m_MVPIdx[1] ) );
        memset( m_MVPNum[0]      + firstElement, -1,                       numElements * sizeof( *m_MVPNum[0] ) );
        memset( m_MVPNum[1]      + firstElement, -1,                       numElements * sizeof( *m_MVPNum[1] ) );
        memset( m_QPArray        + firstElement, m_SliceHeader->SliceQP,     numElements * sizeof( *m_QPArray ) );
        memset( m_MergeFlag      + firstElement, false,                    numElements * sizeof( *m_MergeFlag ) );
        memset( m_MergeIndex     + firstElement, 0,                        numElements * sizeof( *m_MergeIndex ) );
        memset( m_LumaIntraDir   + firstElement, DC_IDX,                   numElements * sizeof( *m_LumaIntraDir ) );
        memset( m_ChromaIntraDir + firstElement, 0,                        numElements * sizeof( *m_ChromaIntraDir ) );
        memset( m_InterDir       + firstElement, 0,                        numElements * sizeof( *m_InterDir ) );
        memset( m_Cbf[0]         + firstElement, 0,                        numElements * sizeof( *m_Cbf[0] ) );
        memset( m_Cbf[1]         + firstElement, 0,                        numElements * sizeof( *m_Cbf[1] ) );
        memset( m_Cbf[2]         + firstElement, 0,                        numElements * sizeof( *m_Cbf[2] ) );
        memset( m_IPCMFlag       + firstElement, false,                    numElements * sizeof( *m_IPCMFlag ) );
    }

    Ipp32u Tmp = g_MaxCUWidth * g_MaxCUHeight;
    if (0 >= partStartIdx)
    {
        m_CUMVbuffer[0].clearMVBuffer();
        m_CUMVbuffer[1].clearMVBuffer();
        // ML: OPT: a lot of memory initializtion in this function, the haviest memset() is the call below,
        //    TODO: do we really need to zero this mem? if so, then
        //          try replacing with streaming stores to improve cache/mem badnwidth utilization
        //memset(m_TrCoeffY , 0, sizeof(H265CoeffsCommon) * Tmp);
        //memset(m_IPCMSampleY , 0, sizeof(H265PlaneYCommon) * Tmp);
        //Tmp >>= 2;
        //memset(m_TrCoeffCb, 0, sizeof(H265CoeffsCommon) * Tmp);
        //memset(m_TrCoeffCr, 0, sizeof(H265CoeffsCommon) * Tmp);
        //memset(m_IPCMSampleCb , 0, sizeof(H265PlaneYCommon) * Tmp);
        //memset(m_IPCMSampleCr , 0, sizeof(H265PlaneYCommon) * Tmp);
    }
    else
    {
        H265CodingUnit * From = Pic->getCU(iCUAddr);
        m_CUMVbuffer[0].copyFrom(&From->m_CUMVbuffer[0], m_NumPartition, 0);
        m_CUMVbuffer[1].copyFrom(&From->m_CUMVbuffer[1], m_NumPartition, 0);
        for (Ipp32u i = 0; i < Tmp; i++)
        {
            m_TrCoeffY[i] = From->m_TrCoeffY[i];
            m_IPCMSampleY[i] = From->m_IPCMSampleY[i];
        }
        for (Ipp32u i = 0; i < (Tmp>>2); i++)
        {
            m_TrCoeffCb[i] = From->m_TrCoeffCb[i];
            m_TrCoeffCr[i] = From->m_TrCoeffCr[i];
            m_IPCMSampleCb[i] = From->m_IPCMSampleCb[i];
            m_IPCMSampleCr[i] = From->m_IPCMSampleCr[i];
        }
    }

    // Setting neighbor CU
    m_CULeft = NULL;
    m_CUAbove = NULL;
    m_CUAboveLeft = NULL;
    m_CUAboveRight = NULL;

    m_CUColocated[0] = NULL;
    m_CUColocated[1] = NULL;

    Ipp32u WidthInCU = sps->WidthInCU;
    if (CUAddr % WidthInCU)
    {
        m_CULeft = Pic->getCU(CUAddr - 1);
    }

    if (CUAddr / WidthInCU)
    {
        m_CUAbove = Pic->getCU(CUAddr - WidthInCU);
    }

    if (m_CULeft && m_CUAbove)
    {
        m_CUAboveLeft = Pic->getCU(CUAddr - WidthInCU - 1);
    }

    if (m_CUAbove && ((CUAddr % WidthInCU) < (WidthInCU - 1)))
    {
        m_CUAboveRight = Pic->getCU(CUAddr - WidthInCU + 1);
    }

    if (m_SliceHeader->m_numRefIdx[REF_PIC_LIST_0] > 0)
    {
        m_CUColocated[0] = sd->m_pRefPicList[REF_PIC_LIST_0][0].refFrame->getCU(CUAddr);
    }

    if (m_SliceHeader->m_numRefIdx[REF_PIC_LIST_1] > 0)
    {
        m_CUColocated[1] = sd->m_pRefPicList[REF_PIC_LIST_1][0].refFrame->getCU(CUAddr);
    }
}

void H265CodingUnit::setOutsideCUPart(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u numPartition = m_NumPartition >> (Depth << 1);
    Ipp32u SizeInUchar = sizeof(Ipp8u) * numPartition;

    Ipp8u Width = (Ipp8u) (g_MaxCUWidth >> Depth);
    Ipp8u Height = (Ipp8u) (g_MaxCUHeight >> Depth);
    memset(m_DepthArray + AbsPartIdx, Depth,  SizeInUchar);
    memset(m_WidthArray + AbsPartIdx, Width,  SizeInUchar);
    memset(m_HeightArray + AbsPartIdx, Height, SizeInUchar);
}

// copy -------------------------------------------------------------------------------------------------------------------
void H265CodingUnit::copySubCU(H265CodingUnit* CU, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u PartIdx = AbsPartIdx;

    m_Frame = CU->m_Frame;
    m_SliceHeader = CU->m_SliceHeader;
    m_SliceIdx = CU->m_SliceIdx;
    CUAddr = CU->CUAddr;
    m_AbsIdxInLCU = AbsPartIdx;
    //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    m_CUPelX = CU->m_CUPelX + g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]];
    m_CUPelY = CU->m_CUPelY + g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]];

    Ipp32u Width = g_MaxCUWidth  >> Depth;
    Ipp32u Height = g_MaxCUHeight >> Depth;

    m_skipFlag = CU->m_skipFlag + PartIdx;

    m_QPArray = CU->m_QPArray + PartIdx;
    m_PartSizeArray = CU->m_PartSizeArray + PartIdx;
    m_PredModeArray = CU->m_PredModeArray + PartIdx;
    m_CUTransquantBypass = CU->m_CUTransquantBypass + PartIdx;

    m_MergeFlag = CU->m_MergeFlag + PartIdx;
    m_MergeIndex = CU->m_MergeIndex + PartIdx;

    m_LumaIntraDir = CU->m_LumaIntraDir + PartIdx;
    m_ChromaIntraDir = CU->m_ChromaIntraDir + PartIdx;
    m_InterDir = CU->m_InterDir + PartIdx;
    m_TrIdxArray = CU->m_TrIdxArray + PartIdx;
    m_TrStartArray = CU->m_TrStartArray + PartIdx;
    for (Ipp32s i = 0; i < 3; i++)
        m_TransformSkip[i] = CU->m_TransformSkip[i] + PartIdx;

    m_Cbf[0] = CU->getCbf(TEXT_LUMA) + PartIdx;
    m_Cbf[1] = CU->getCbf(TEXT_CHROMA_U) + PartIdx;
    m_Cbf[2] = CU->getCbf(TEXT_CHROMA_V) + PartIdx;

    m_DepthArray = CU->m_DepthArray + PartIdx;
    m_WidthArray = CU->m_WidthArray + PartIdx;
    m_HeightArray = CU->m_HeightArray + PartIdx;

    m_MVPIdx[0] = CU->m_MVPIdx[REF_PIC_LIST_0] + PartIdx;
    m_MVPIdx[1] = CU->m_MVPIdx[REF_PIC_LIST_1] + PartIdx;
    m_MVPNum[0] = CU->m_MVPNum[REF_PIC_LIST_0] + PartIdx;
    m_MVPNum[1] = CU->m_MVPNum[REF_PIC_LIST_1] + PartIdx;

    m_IPCMFlag = CU->m_IPCMFlag + PartIdx;

    m_CUAboveLeft = CU->m_CUAboveLeft;
    m_CUAboveRight = CU->m_CUAboveRight;
    m_CUAbove = CU->m_CUAbove;
    m_CULeft = CU->m_CULeft;

    m_CUColocated[0] = CU->m_CUColocated[REF_PIC_LIST_0];
    m_CUColocated[1] = CU->m_CUColocated[REF_PIC_LIST_1];

    Ipp32u Tmp = Width * Height;
    Ipp32u MaxCuWidth = CU->m_Frame->m_pSlicesInfo->GetSlice(0)->GetSeqParam()->MaxCUWidth;
    Ipp32u MaxCuHeight = CU->m_Frame->m_pSlicesInfo->GetSlice(0)->GetSeqParam()->MaxCUHeight;

    Ipp32u CoffOffset = MaxCuWidth * MaxCuHeight * AbsPartIdx / CU->m_Frame->getCD()->getNumPartInCU();

    m_TrCoeffY = CU->m_TrCoeffY + CoffOffset;
    m_IPCMSampleY = CU->m_IPCMSampleY + CoffOffset;

    Tmp >>= 2;
    CoffOffset >>=2;
    m_TrCoeffCb = CU->m_TrCoeffCb + CoffOffset;
    m_TrCoeffCr = CU->m_TrCoeffCr + CoffOffset;

    m_IPCMSampleCb = CU->m_IPCMSampleCb + CoffOffset;
    m_IPCMSampleCr = CU->m_IPCMSampleCr + CoffOffset;

    m_CUMVbuffer[0].linkToWithOffset(&(CU->m_CUMVbuffer[REF_PIC_LIST_0]), PartIdx);
    m_CUMVbuffer[1].linkToWithOffset(&(CU->m_CUMVbuffer[REF_PIC_LIST_1]), PartIdx);
    memcpy(m_SliceStartCU, CU->m_SliceStartCU + PartIdx, sizeof(Ipp32u) * m_NumPartition);
    memcpy(m_DependentSliceStartCU, CU->m_DependentSliceStartCU + PartIdx, sizeof(Ipp32u) * m_NumPartition);
}

// Other public functions ----------------------------------------------------------------------------------------------------------
H265CodingUnit* H265CodingUnit::getPULeft(Ipp32u& LPartUnitIdx, Ipp32u CurrPartUnitIdx,
                                          bool EnforceSliceRestriction, bool EnforceTileRestriction)
{
    Ipp32u AbsPartIdx = g_ZscanToRaster[CurrPartUnitIdx];
    Ipp32u AbsZorderCUIdx = g_ZscanToRaster[m_AbsIdxInLCU];
    Ipp32u NumPartInCUWidth = m_Frame->m_CodingData->m_NumPartInWidth;

    if (!RasterAddress::isZeroCol(AbsPartIdx, NumPartInCUWidth))
    {
        LPartUnitIdx = g_RasterToZscan[AbsPartIdx - 1];
        if (RasterAddress::isEqualCol(AbsPartIdx, AbsZorderCUIdx, NumPartInCUWidth))
        {
            return m_Frame->getCU(CUAddr);
        }
        else
        {
            LPartUnitIdx -= m_AbsIdxInLCU;
            return this;
        }
    }

    LPartUnitIdx = g_RasterToZscan[AbsPartIdx + NumPartInCUWidth - 1];

    if (
        (EnforceSliceRestriction && (m_CULeft == NULL || m_CULeft->m_SliceHeader == NULL || m_CULeft->getSCUAddr() + LPartUnitIdx < m_Frame->getCU(CUAddr)->getSliceStartCU(CurrPartUnitIdx)))
        ||
        (EnforceTileRestriction && (m_CULeft == NULL || m_CULeft->m_SliceHeader == NULL || m_Frame->m_CodingData->getTileIdxMap(m_CULeft->CUAddr) != m_Frame->m_CodingData->getTileIdxMap(CUAddr))))
    {
        return NULL;
    }
    return m_CULeft;
}

H265CodingUnit* H265CodingUnit::getPUAbove(Ipp32u& APartUnitIdx, Ipp32u CurrPartUnitIdx, bool EnforceSliceRestriction,
                                           bool planarAtLCUBoundary, bool EnforceTileRestriction)
{
    Ipp32u AbsPartIdx = g_ZscanToRaster[CurrPartUnitIdx];
    Ipp32u AbsZorderCUIdx = g_ZscanToRaster[m_AbsIdxInLCU];
    Ipp32u NumPartInCUWidth = m_Frame->m_CodingData->m_NumPartInWidth;

    if (!RasterAddress::isZeroRow(AbsPartIdx, NumPartInCUWidth))
    {
        APartUnitIdx = g_RasterToZscan[AbsPartIdx - NumPartInCUWidth];
        if (RasterAddress::isEqualRow(AbsPartIdx, AbsZorderCUIdx, NumPartInCUWidth))
        {
            return m_Frame->getCU(CUAddr);
        }
        else
        {
            APartUnitIdx -= m_AbsIdxInLCU;
            return this;
        }
    }

    if (planarAtLCUBoundary)
    {
        return NULL;
    }

    APartUnitIdx = g_RasterToZscan[AbsPartIdx + m_Frame->getCD()->getNumPartInCU() - NumPartInCUWidth];

    if (
        (EnforceSliceRestriction && (m_CUAbove == NULL || m_CUAbove->m_SliceHeader == NULL || m_CUAbove->getSCUAddr() + APartUnitIdx < m_Frame->getCU(CUAddr)->getSliceStartCU(CurrPartUnitIdx)))
        ||
        (EnforceTileRestriction && (m_CUAbove == NULL || m_CUAbove->m_SliceHeader == NULL || m_Frame->m_CodingData->getTileIdxMap(m_CUAbove->CUAddr) != m_Frame->m_CodingData->getTileIdxMap(CUAddr))))
    {
        return NULL;
    }
     return m_CUAbove;
}

H265CodingUnit* H265CodingUnit::getPUAboveLeft(Ipp32u& ALPartUnitIdx, Ipp32u CurrPartUnitIdx, bool EnforceSliceRestriction)
{
    Ipp32u AbsPartIdx = g_ZscanToRaster[CurrPartUnitIdx];
    Ipp32u AbsZorderCUIdx = g_ZscanToRaster[m_AbsIdxInLCU];
    Ipp32u NumPartInCUWidth = m_Frame->getNumPartInWidth();

    if (!RasterAddress::isZeroCol(AbsPartIdx, NumPartInCUWidth))
    {
        if (!RasterAddress::isZeroRow(AbsPartIdx, NumPartInCUWidth))
        {
            ALPartUnitIdx = g_RasterToZscan[AbsPartIdx - NumPartInCUWidth - 1];
            if (RasterAddress::isEqualRowOrCol(AbsPartIdx, AbsZorderCUIdx, NumPartInCUWidth))
            {
                return m_Frame->getCU(CUAddr);
            }
            else
            {
                ALPartUnitIdx -= m_AbsIdxInLCU;
                return this;
            }
        }

        ALPartUnitIdx = g_RasterToZscan[AbsPartIdx + m_Frame->getCD()->getNumPartInCU() - NumPartInCUWidth - 1];

        if ((EnforceSliceRestriction && (
                m_CUAbove == NULL ||
                m_CUAbove->m_SliceHeader == NULL ||
                m_CUAbove->getSCUAddr() + ALPartUnitIdx < m_Frame->getCU(CUAddr)->getSliceStartCU(CurrPartUnitIdx) ||
                m_Frame->m_CodingData->getTileIdxMap(m_CUAbove->CUAddr) != m_Frame->m_CodingData->getTileIdxMap(CUAddr))))
        {
            return NULL;
        }
        return m_CUAbove;
    }

    if (!RasterAddress::isZeroRow(AbsPartIdx, NumPartInCUWidth))
    {
        ALPartUnitIdx = g_RasterToZscan[AbsPartIdx - 1];
        if ((EnforceSliceRestriction && (
                m_CULeft == NULL ||
                m_CULeft->m_SliceHeader == NULL ||
                m_CULeft->getSCUAddr() + ALPartUnitIdx < m_Frame->getCU(CUAddr)->getSliceStartCU(CurrPartUnitIdx) ||
                m_Frame->m_CodingData->getTileIdxMap(m_CULeft->CUAddr) != m_Frame->m_CodingData->getTileIdxMap(CUAddr))))
        {
            return NULL;
        }
        return m_CULeft;
    }

    ALPartUnitIdx = g_RasterToZscan[m_Frame->getCD()->getNumPartInCU() - 1];

    if ((EnforceSliceRestriction && (
            m_CUAboveLeft == NULL ||
            m_CUAboveLeft->m_SliceHeader == NULL ||
            m_CUAboveLeft->getSCUAddr() + ALPartUnitIdx < m_Frame->getCU(CUAddr)->getSliceStartCU(CurrPartUnitIdx) ||
            m_Frame->m_CodingData->getTileIdxMap(m_CUAboveLeft->CUAddr) != m_Frame->m_CodingData->getTileIdxMap(CUAddr))))
    {
        return NULL;
    }
    return m_CUAboveLeft;
}

H265CodingUnit* H265CodingUnit::getPUAboveRight(Ipp32u& ARPartUnitIdx, Ipp32u CurrPartUnitIdx, bool EnforceSliceRestriction)
{
    Ipp32u AbsPartIdxRT = g_ZscanToRaster[CurrPartUnitIdx];
    Ipp32u AbsZorderCUIdx = g_ZscanToRaster[m_AbsIdxInLCU] + m_WidthArray[0] / m_Frame->getMinCUWidth() - 1;
    Ipp32u NumPartInCUWidth = m_Frame->getNumPartInWidth();

    if ((m_Frame->getCU(CUAddr)->m_CUPelX + g_RasterToPelX[AbsPartIdxRT] + m_Frame->getMinCUWidth()) >= m_Frame->m_pSlicesInfo->GetSlice(0)->GetSeqParam()->pic_width_in_luma_samples)
    {
        ARPartUnitIdx = MAX_UINT;
        return NULL;
    }

    if (RasterAddress::lessThanCol(AbsPartIdxRT, NumPartInCUWidth - 1, NumPartInCUWidth))
    {
        if (!RasterAddress::isZeroRow(AbsPartIdxRT, NumPartInCUWidth))
        {
            if (CurrPartUnitIdx > g_RasterToZscan[AbsPartIdxRT - NumPartInCUWidth + 1])
            {
                ARPartUnitIdx = g_RasterToZscan[AbsPartIdxRT - NumPartInCUWidth + 1];
                if (RasterAddress::isEqualRowOrCol(AbsPartIdxRT, AbsZorderCUIdx, NumPartInCUWidth))
                {
                    return m_Frame->getCU(CUAddr);
                }
                else
                {
                    ARPartUnitIdx -= m_AbsIdxInLCU;
                    return this;
                }
            }
            ARPartUnitIdx = MAX_UINT;
            return NULL;
        }

        ARPartUnitIdx = g_RasterToZscan[AbsPartIdxRT + m_Frame->getCD()->getNumPartInCU() - NumPartInCUWidth + 1];

        if ((EnforceSliceRestriction && (
                m_CUAbove == NULL ||
                m_CUAbove->m_SliceHeader == NULL ||
                m_CUAbove->getSCUAddr() + ARPartUnitIdx < m_Frame->getCU(CUAddr)->getSliceStartCU(CurrPartUnitIdx) ||
                m_Frame->m_CodingData->getTileIdxMap(m_CUAbove->CUAddr) != m_Frame->m_CodingData->getTileIdxMap(CUAddr))))
            {
                return NULL;
            }
        return m_CUAbove;
    }

    if (!RasterAddress::isZeroRow(AbsPartIdxRT, NumPartInCUWidth))
    {
        ARPartUnitIdx = MAX_UINT;
        return NULL;
    }

    ARPartUnitIdx = g_RasterToZscan[m_Frame->getCD()->getNumPartInCU() - NumPartInCUWidth];

    if ((EnforceSliceRestriction && (
            m_CUAboveRight == NULL ||
            m_CUAboveRight->m_SliceHeader == NULL ||
            m_Frame->m_CodingData->GetInverseCUOrderMap(m_CUAboveRight->CUAddr) > m_Frame->m_CodingData->GetInverseCUOrderMap(CUAddr) ||
            m_CUAboveRight->getSCUAddr() + ARPartUnitIdx < m_Frame->getCU(CUAddr)->getSliceStartCU(CurrPartUnitIdx) ||
            m_Frame->m_CodingData->getTileIdxMap(m_CUAboveRight->CUAddr) != m_Frame->m_CodingData->getTileIdxMap(CUAddr))))
    {
        return NULL;
    }
    return m_CUAboveRight;
}

H265CodingUnit* H265CodingUnit::getPUBelowLeft(Ipp32u& BLPartUnitIdx, Ipp32u CurrPartUnitIdx, bool EnforceSliceRestriction)
{
    Ipp32u AbsPartIdxLB = g_ZscanToRaster[CurrPartUnitIdx];
    Ipp32u AbsZorderCUIdxLB = g_ZscanToRaster[m_AbsIdxInLCU ] + (m_HeightArray[0] / m_Frame->getMinCUHeight() - 1) * m_Frame->getNumPartInWidth();
    Ipp32u NumPartInCUWidth = m_Frame->getNumPartInWidth();

    if ((m_Frame->getCU(CUAddr)->m_CUPelY + g_RasterToPelY[AbsPartIdxLB] + m_Frame->getMinCUHeight()) >= m_Frame->m_pSlicesInfo->GetSlice(0)->GetSeqParam()->pic_height_in_luma_samples)
    {
        BLPartUnitIdx = MAX_UINT;
        return NULL;
    }

    if (RasterAddress::lessThanRow(AbsPartIdxLB, m_Frame->getNumPartInHeight() - 1, NumPartInCUWidth))
    {
        if (!RasterAddress::isZeroCol(AbsPartIdxLB, NumPartInCUWidth))
        {
            if (CurrPartUnitIdx > g_RasterToZscan[AbsPartIdxLB + NumPartInCUWidth - 1])
            {
                BLPartUnitIdx = g_RasterToZscan[AbsPartIdxLB + NumPartInCUWidth - 1];
                if (RasterAddress::isEqualRowOrCol(AbsPartIdxLB, AbsZorderCUIdxLB, NumPartInCUWidth))
                {
                    return m_Frame->getCU(CUAddr);
                }
                else
                {
                    BLPartUnitIdx -= m_AbsIdxInLCU;
                    return this;
                }
            }
            BLPartUnitIdx = MAX_UINT;
            return NULL;
        }
        BLPartUnitIdx = g_RasterToZscan[AbsPartIdxLB + NumPartInCUWidth * 2 - 1];
        if (
            (EnforceSliceRestriction && (
                m_CULeft == NULL ||
                m_CULeft->m_SliceHeader == NULL ||
                m_CULeft->getSCUAddr() + BLPartUnitIdx < m_Frame->getCU(CUAddr)->getSliceStartCU(CurrPartUnitIdx) ||
                m_CULeft == NULL || m_CULeft->m_SliceHeader == NULL || m_CULeft->getSCUAddr() + BLPartUnitIdx < m_Frame->getCU(CUAddr)->getSliceStartCU(CurrPartUnitIdx) ||
                m_Frame->m_CodingData->getTileIdxMap(m_CULeft->CUAddr) != m_Frame->m_CodingData->getTileIdxMap(CUAddr))))
        {
            return NULL;
        }
        return m_CULeft;
    }

    BLPartUnitIdx = MAX_UINT;
    return NULL;
}

H265CodingUnit* H265CodingUnit::getPUBelowLeftAdi(Ipp32u& BLPartUnitIdx, Ipp32u CurrPartUnitIdx, Ipp32u PartUnitOffset, bool EnforceSliceRestriction)
{
    Ipp32u AbsPartIdxLB = g_ZscanToRaster[CurrPartUnitIdx];
    Ipp32u AbsZorderCUIdxLB = g_ZscanToRaster[m_AbsIdxInLCU] + ((m_HeightArray[0] / m_Frame->getMinCUHeight()) - 1) * m_Frame->getNumPartInWidth();
    Ipp32u NumPartInCUWidth = m_Frame->getNumPartInWidth();

    if ((m_Frame->getCU(CUAddr)->m_CUPelY + g_RasterToPelY[AbsPartIdxLB] + (m_Frame->m_CodingData->m_MinCUHeight * PartUnitOffset)) >= m_Frame->m_pSlicesInfo->GetSlice(0)->GetSeqParam()->pic_height_in_luma_samples)
    {
        BLPartUnitIdx = MAX_UINT;
        return NULL;
    }

    if (RasterAddress::lessThanRow(AbsPartIdxLB, m_Frame->getNumPartInHeight() - PartUnitOffset, NumPartInCUWidth))
    {
        if (!RasterAddress::isZeroCol(AbsPartIdxLB, NumPartInCUWidth))
        {
            if (CurrPartUnitIdx > g_RasterToZscan[AbsPartIdxLB + PartUnitOffset * NumPartInCUWidth - 1])
            {
                BLPartUnitIdx = g_RasterToZscan[AbsPartIdxLB + PartUnitOffset * NumPartInCUWidth - 1];
                if (RasterAddress::isEqualRowOrCol(AbsPartIdxLB, AbsZorderCUIdxLB, NumPartInCUWidth))
                {
                    return m_Frame->getCU(CUAddr);
                }
                else
                {
                    BLPartUnitIdx -= m_AbsIdxInLCU;
                    return this;
                }
            }
            BLPartUnitIdx = MAX_UINT;
            return NULL;
        }
        BLPartUnitIdx = g_RasterToZscan[AbsPartIdxLB + (1 + PartUnitOffset) * NumPartInCUWidth - 1];
    if ((EnforceSliceRestriction && (
            m_CULeft==NULL ||
            m_CULeft->m_SliceHeader == NULL ||
            m_CULeft->getSCUAddr() + BLPartUnitIdx < m_Frame->getCU(CUAddr)->getSliceStartCU(CurrPartUnitIdx) ||
            m_Frame->m_CodingData->getTileIdxMap(m_CULeft->CUAddr) != m_Frame->m_CodingData->getTileIdxMap(CUAddr))))
        {
            return NULL;
        }
        return m_CULeft;
    }

    BLPartUnitIdx = MAX_UINT;
    return NULL;
}

H265CodingUnit* H265CodingUnit::getPUAboveRightAdi(Ipp32u& ARPartUnitIdx, Ipp32u CurrPartUnitIdx, Ipp32u PartUnitOffset,
                                                   bool EnforceSliceRestriction)
{
    Ipp32u AbsPartIdxRT = g_ZscanToRaster[CurrPartUnitIdx];
    Ipp32u AbsZorderCUIdx = g_ZscanToRaster[m_AbsIdxInLCU] + (m_WidthArray[0] / m_Frame->getMinCUWidth()) - 1;
    Ipp32u NumPartInCUWidth = m_Frame->getNumPartInWidth();

    if ((m_Frame->getCU(CUAddr)->m_CUPelX + g_RasterToPelX[AbsPartIdxRT] + (m_Frame->m_CodingData->m_MinCUHeight * PartUnitOffset)) >= m_Frame->m_pSlicesInfo->GetSlice(0)->GetSeqParam()->pic_width_in_luma_samples)
    {
        ARPartUnitIdx = MAX_UINT;
        return NULL;
    }

    if (RasterAddress::lessThanCol(AbsPartIdxRT, NumPartInCUWidth - PartUnitOffset, NumPartInCUWidth))
    {
        if (!RasterAddress::isZeroRow(AbsPartIdxRT, NumPartInCUWidth))
        {
            if (CurrPartUnitIdx > g_RasterToZscan[AbsPartIdxRT - NumPartInCUWidth + PartUnitOffset])
            {
                ARPartUnitIdx = g_RasterToZscan[AbsPartIdxRT - NumPartInCUWidth + PartUnitOffset];
                if (RasterAddress::isEqualRowOrCol(AbsPartIdxRT, AbsZorderCUIdx, NumPartInCUWidth))
                {
                    return m_Frame->getCU(CUAddr);
                }
                else
                {
                    ARPartUnitIdx -= m_AbsIdxInLCU;
                    return this;
                }
            }
            ARPartUnitIdx = MAX_UINT;
            return NULL;
        }
        ARPartUnitIdx = g_RasterToZscan[AbsPartIdxRT + m_Frame->getCD()->getNumPartInCU() - NumPartInCUWidth + PartUnitOffset];
        if ((EnforceSliceRestriction && (
                m_CUAbove == NULL ||
                m_CUAbove->m_SliceHeader == NULL ||
                m_CUAbove->getSCUAddr() + ARPartUnitIdx < m_Frame->getCU(CUAddr)->getSliceStartCU(CurrPartUnitIdx) ||
                m_Frame->m_CodingData->getTileIdxMap(m_CUAbove->CUAddr) != m_Frame->m_CodingData->getTileIdxMap(CUAddr))))
        {
            return NULL;
        }
        return m_CUAbove;
    }

    if (!RasterAddress::isZeroRow(AbsPartIdxRT, NumPartInCUWidth))
    {
        ARPartUnitIdx = MAX_UINT;
        return NULL;
    }

    ARPartUnitIdx = g_RasterToZscan[m_Frame->getCD()->getNumPartInCU() - NumPartInCUWidth + PartUnitOffset - 1];
    if ((EnforceSliceRestriction && (
            m_CUAboveRight == NULL ||
            m_CUAboveRight->m_SliceHeader == NULL ||
            m_Frame->m_CodingData->GetInverseCUOrderMap(m_CUAboveRight->CUAddr) > m_Frame->m_CodingData->GetInverseCUOrderMap(CUAddr) ||
            m_CUAboveRight->getSCUAddr() + ARPartUnitIdx < m_Frame->getCU(CUAddr)->getSliceStartCU(CurrPartUnitIdx) ||
            m_Frame->m_CodingData->getTileIdxMap(m_CUAboveRight->CUAddr) != m_Frame->m_CodingData->getTileIdxMap(CUAddr))))
    {
        return NULL;
    }
    return m_CUAboveRight;
}

/** Get left QpMinCu
*\param   uiLPartUnitIdx
*\param   uiCurrAbsIdxInLCU
*\param   bEnforceSliceRestriction
*\param   bEnforceDependentSliceRestriction
*\returns H265CodingUnit*   point of H265CodingUnit of left QpMinCu
*/
H265CodingUnit* H265CodingUnit::getQPMinCULeft(Ipp32u& LPartUnitIdx, Ipp32u CurrAbsIdxInLCU)
{
    Ipp32u NumPartInCUWidth = m_Frame->getNumPartInWidth();
    Ipp32u absZorderQpMinCUIdx = (CurrAbsIdxInLCU >> ((g_MaxCUDepth - m_SliceHeader->m_PicParamSet->diff_cu_qp_delta_depth) << 1)) <<
        ((g_MaxCUDepth - m_SliceHeader->m_PicParamSet->diff_cu_qp_delta_depth) << 1);
    Ipp32u AbsRorderQpMinCUIdx = g_ZscanToRaster[absZorderQpMinCUIdx];

    // check for left LCU boundary
    if (RasterAddress::isZeroCol(AbsRorderQpMinCUIdx, NumPartInCUWidth))
    {
        return NULL;
    }

    // get index of left-CU relative to top-left corner of current quantization group
    LPartUnitIdx = g_RasterToZscan[AbsRorderQpMinCUIdx - 1];

    return m_Frame->getCU(CUAddr);
}

/** Get Above QpMinCu
*\param   aPartUnitIdx
*\param   currAbsIdxInLCU
*\param   enforceSliceRestriction
*\param   enforceDependentSliceRestriction
*\returns TComDataCU*   point of TComDataCU of above QpMinCu
*/
H265CodingUnit* H265CodingUnit::getQPMinCUAbove(Ipp32u& aPartUnitIdx, Ipp32u CurrAbsIdxInLCU)
{
    Ipp32u numPartInCUWidth = m_Frame->getNumPartInWidth();
    Ipp32u absZorderQpMinCUIdx = (CurrAbsIdxInLCU >> ((g_MaxCUDepth - m_SliceHeader->m_PicParamSet->diff_cu_qp_delta_depth) << 1)) <<
        ((g_MaxCUDepth - m_SliceHeader->m_PicParamSet->diff_cu_qp_delta_depth) << 1);
    Ipp32u AbsRorderQpMinCUIdx = g_ZscanToRaster[absZorderQpMinCUIdx];

    // check for top LCU boundary
    if (RasterAddress::isZeroRow(AbsRorderQpMinCUIdx, numPartInCUWidth))
    {
        return NULL;
    }

    // get index of top-CU relative to top-left corner of current quantization group
    aPartUnitIdx = g_RasterToZscan[AbsRorderQpMinCUIdx - numPartInCUWidth];

    // return pointer to current LCU
    return m_Frame->getCU(CUAddr);
}

/** Get reference QP from left QpMinCu or latest coded QP
*\param   uiCurrAbsIdxInLCU
*\returns Ipp8u   reference QP value
*/
Ipp8u H265CodingUnit::getRefQP(Ipp32u CurrAbsIdxInLCU)
{
    Ipp32u lPartIdx = 0, aPartIdx = 0;
    H265CodingUnit* cULeft  = getQPMinCULeft (lPartIdx, m_AbsIdxInLCU + CurrAbsIdxInLCU);
    H265CodingUnit* cUAbove = getQPMinCUAbove(aPartIdx, m_AbsIdxInLCU + CurrAbsIdxInLCU);
    return (((cULeft ? cULeft->m_QPArray[lPartIdx]: getLastCodedQP(CurrAbsIdxInLCU)) + (cUAbove ? cUAbove->m_QPArray[aPartIdx] : getLastCodedQP(CurrAbsIdxInLCU)) + 1) >> 1);
}

Ipp32s H265CodingUnit::getLastValidPartIdx(Ipp32s AbsPartIdx)
{
    Ipp32s LastValidPartIdx = AbsPartIdx - 1;
    while (LastValidPartIdx >= 0
       && m_PredModeArray[LastValidPartIdx] == MODE_NONE)
    {
        Ipp32u Depth = m_DepthArray[LastValidPartIdx];
        LastValidPartIdx -= m_NumPartition >> (Depth << 1);
    }
    return LastValidPartIdx;
}

Ipp8u H265CodingUnit::getLastCodedQP(Ipp32u AbsPartIdx)
{
    Ipp32u QUPartIdxMask = ~((1 << ((g_MaxCUDepth - m_Frame->m_pSlicesInfo->GetSlice(0)->GetPicParam()->diff_cu_qp_delta_depth) << 1)) - 1);
    Ipp32s LastValidPartIdx = getLastValidPartIdx(AbsPartIdx & QUPartIdxMask);
    if (AbsPartIdx < m_NumPartition
        && (getSCUAddr() + LastValidPartIdx < getSliceStartCU(m_AbsIdxInLCU + AbsPartIdx)))
    {
        return (Ipp8u)m_SliceHeader->SliceQP;
    }
    else
    if (LastValidPartIdx >= 0)
    {
        return m_QPArray[LastValidPartIdx];
    }
    else
    {
        // FIXME: Should just remember last valid coded QP for last valid part idx instead of all of the following code
        if (m_AbsIdxInLCU > 0)
        {
            return m_Frame->getCU(CUAddr)->getLastCodedQP(m_AbsIdxInLCU);
        }
        else if (m_Frame->m_CodingData->GetInverseCUOrderMap(CUAddr) > 0
            && m_Frame->m_CodingData->getTileIdxMap(CUAddr) ==
                m_Frame->m_CodingData->getTileIdxMap(m_Frame->m_CodingData->getCUOrderMap(m_Frame->m_CodingData->GetInverseCUOrderMap(CUAddr) - 1))
            && !(m_SliceHeader->m_PicParamSet->getEntropyCodingSyncEnabledFlag() && CUAddr % m_Frame->m_CodingData->m_WidthInCU == 0))
        {
            return m_Frame->getCU(m_Frame->m_CodingData->getCUOrderMap(m_Frame->m_CodingData->GetInverseCUOrderMap(CUAddr) - 1))->getLastCodedQP(m_Frame->getCD()->getNumPartInCU());
        }
        else
        {
            return (Ipp8u)m_SliceHeader->SliceQP;
        }
    }
}

/** Check whether the CU is coded in lossless coding mode
 * \param   uiAbsPartIdx
 * \returns true if the CU is coded in lossless coding mode; false if otherwise
 */
bool H265CodingUnit::isLosslessCoded(Ipp32u absPartIdx)
{
  return (m_SliceHeader->m_PicParamSet->transquant_bypass_enable_flag && m_CUTransquantBypass[absPartIdx]);
}

/** Get allowed chroma intra modes
*\param   uiAbsPartIdx
*\param   uiModeList  pointer to chroma intra modes array
*\returns
*- fill uiModeList with chroma intra modes
*/
void H265CodingUnit::getAllowedChromaDir(Ipp32u AbsPartIdx, Ipp32u* ModeList)
{
    ModeList[0] = PLANAR_IDX;
    ModeList[1] = VER_IDX;
    ModeList[2] = HOR_IDX;
    ModeList[3] = DC_IDX;
    ModeList[4] = DM_CHROMA_IDX;

    Ipp32u LumaMode = m_LumaIntraDir[AbsPartIdx];

    for (Ipp32s i = 0; i < NUM_CHROMA_MODE - 1; i++)
    {
        if (LumaMode == ModeList[i])
        {
            ModeList[i] = 34; // VER+8 mode
            break;
        }
    }
}

Ipp32u H265CodingUnit::getCtxQtCbf(EnumTextType Type, Ipp32u TrDepth)
{
    if (Type)
    {
        return TrDepth;
    }
    else
    {
        const Ipp32u Ctx = (TrDepth == 0) ? 1 : 0;
        return Ctx;
    }
}

Ipp32u H265CodingUnit::getQuadtreeTULog2MinSizeInCU(Ipp32u Idx)
{
    Ipp32u Log2CbSize = g_ConvertToBit[m_WidthArray[Idx]] + 2;
    EnumPartSize partSize = static_cast<EnumPartSize>(m_PartSizeArray[Idx]);
    Ipp32u QuadtreeTUMaxDepth = m_PredModeArray[Idx] == MODE_INTRA ? m_Frame->m_pSlicesInfo->GetSlice(0)->GetSeqParam()->max_transform_hierarchy_depth_intra : m_Frame->m_pSlicesInfo->GetSlice(0)->GetSeqParam()->max_transform_hierarchy_depth_inter;
    Ipp32s IntraSplitFlag = m_PredModeArray[Idx] == MODE_INTRA && partSize == SIZE_NxN;
    Ipp32s InterSplitFlag = ((QuadtreeTUMaxDepth == 1) && (m_PredModeArray[Idx] == MODE_INTER) && (partSize != SIZE_2Nx2N));

    Ipp32u Log2MinTUSizeInCU = 0;

    if (Log2CbSize < m_Frame->m_pSlicesInfo->GetSlice(0)->GetSeqParam()->log2_min_transform_block_size + QuadtreeTUMaxDepth - 1 + InterSplitFlag + IntraSplitFlag)
    {
        // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is < log2_min_transform_block_size
        Log2MinTUSizeInCU = m_Frame->m_pSlicesInfo->GetSlice(0)->GetSeqParam()->log2_min_transform_block_size;
    }
    else
    {
        // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is still >= log2_min_transform_block_size
        Log2MinTUSizeInCU = Log2CbSize - (QuadtreeTUMaxDepth - 1 + InterSplitFlag + IntraSplitFlag);

        if (Log2MinTUSizeInCU > m_Frame->m_pSlicesInfo->GetSlice(0)->GetSeqParam()->log2_max_transform_block_size)
            // when fully making use of signaled TUMaxDepth + inter/intraSplitFlag, resulting luma TB size is still > log2_max_transform_block_size
            Log2MinTUSizeInCU = m_Frame->m_pSlicesInfo->GetSlice(0)->GetSeqParam()->log2_max_transform_block_size;
    }

    return Log2MinTUSizeInCU;
}

Ipp32u H265CodingUnit::getCtxInterDir(Ipp32u AbsPartIdx)
{
    return m_DepthArray[AbsPartIdx];
}

void H265CodingUnit::setCbfSubParts(Ipp32u CbfY, Ipp32u CbfU, Ipp32u CbfV, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u CurrPartNumb = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);
    memset(m_Cbf[0] + AbsPartIdx, CbfY, sizeof(Ipp8u) * CurrPartNumb);
    memset(m_Cbf[1] + AbsPartIdx, CbfU, sizeof(Ipp8u) * CurrPartNumb);
    memset(m_Cbf[2] + AbsPartIdx, CbfV, sizeof(Ipp8u) * CurrPartNumb);
}

void H265CodingUnit::setCbfSubParts(Ipp32u uCbf, EnumTextType TType, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u CurrPartNumb = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);
    memset(m_Cbf[g_ConvertTxtTypeToIdx[TType]] + AbsPartIdx, uCbf, sizeof(Ipp8u) * CurrPartNumb);
}

/** Sets a coded block flag for all sub-partitions of a partition
 * \param uiCbf The value of the coded block flag to be set
 * \param eTType
 * \param uiAbsPartIdx
 * \param uiPartIdx
 * \param uiDepth
 * \returns Void
 */
void H265CodingUnit::setCbfSubParts (Ipp32u uCbf, EnumTextType TType, Ipp32u AbsPartIdx, Ipp32u PartIdx, Ipp32u Depth)
{
    setSubPart<Ipp8u>((Ipp8u)uCbf, m_Cbf[g_ConvertTxtTypeToIdx[TType]], AbsPartIdx, Depth, PartIdx);
}

void H265CodingUnit::setDepthSubParts(Ipp32u Depth, Ipp32u AbsPartIdx)
{
    Ipp32u CurrPartNumb = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);
    memset(m_DepthArray + AbsPartIdx, Depth, sizeof(Ipp8u) * CurrPartNumb);
}

void H265CodingUnit::setPartSizeSubParts(EnumPartSize Mode, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    VM_ASSERT(sizeof(*m_PartSizeArray) == 1);
    memset(m_PartSizeArray + AbsPartIdx, Mode, m_Frame->getCD()->getNumPartInCU() >> (2 * Depth));
}

void H265CodingUnit::setCUTransquantBypassSubParts(bool flag, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    memset(m_CUTransquantBypass + AbsPartIdx, flag, m_Frame->getCD()->getNumPartInCU() >> (2 * Depth));
}

void H265CodingUnit::setSkipFlagSubParts(bool skip, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    memset(m_skipFlag + AbsPartIdx, skip, m_Frame->getCD()->getNumPartInCU() >> (2 * Depth));
}

void H265CodingUnit::setPredModeSubParts(EnumPredMode Mode, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    VM_ASSERT(sizeof(*m_PartSizeArray) == 1);
    memset(m_PredModeArray + AbsPartIdx, Mode, m_Frame->getCD()->getNumPartInCU() >> (2 * Depth));
}

void H265CodingUnit::setTransformSkipSubParts(Ipp32u useTransformSkip, EnumTextType Type, Ipp32u AbsPartIdx, Ipp32u Depth)
{
  Ipp32u CurrPartNumb = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);

  memset(m_TransformSkip[g_ConvertTxtTypeToIdx[Type]] + AbsPartIdx, useTransformSkip, sizeof(Ipp8u) * CurrPartNumb);
}

void H265CodingUnit::setQPSubParts(Ipp32u QP, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u CurrPartNumb = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);

    for (Ipp32u SCUIdx = AbsPartIdx; SCUIdx < AbsPartIdx + CurrPartNumb; SCUIdx++)
    {
        if (m_Frame->getCU(CUAddr)->getSliceSegmentStartCU(SCUIdx + m_AbsIdxInLCU) == m_SliceHeader->m_sliceSegmentCurStartCUAddr)
        {
            m_QPArray[SCUIdx] = (Ipp8u) QP;
        }
    }
}

void H265CodingUnit::setLumaIntraDirSubParts(Ipp32u Dir, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u CurrPartNumb = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);

    memset(m_LumaIntraDir + AbsPartIdx, Dir, sizeof(Ipp8u) * CurrPartNumb);
}

template<typename T>
void H265CodingUnit::setSubPart(T Parameter, T* pBaseLCU, Ipp32u uCUAddr, Ipp32u uCUDepth, Ipp32u uPUIdx)
{
    VM_ASSERT(sizeof(T) == 1); // Using memset() works only for types of size 1
    VM_ASSERT(sizeof(uPUIdx) != 0); //WTF uPUIdx NOT USED IN THIS FUNCTION WTF
    Ipp32u CurrPartNumQ = (m_Frame->getCD()->getNumPartInCU() >> (2 * uCUDepth)) >> 2;
    switch (m_PartSizeArray[uCUAddr])
    {
        case SIZE_2Nx2N:
            memset(pBaseLCU + uCUAddr, Parameter, 4 * CurrPartNumQ);
            break;
        case SIZE_2NxN:
            memset(pBaseLCU + uCUAddr, Parameter, 2 * CurrPartNumQ);
            break;
        case SIZE_Nx2N:
            memset(pBaseLCU + uCUAddr, Parameter, CurrPartNumQ);
            memset(pBaseLCU + uCUAddr + 2 * CurrPartNumQ, Parameter, CurrPartNumQ);
            break;
        case SIZE_NxN:
            memset(pBaseLCU + uCUAddr, Parameter, CurrPartNumQ);
            break;
        case SIZE_2NxnU:
            if (uPUIdx == 0)
            {
                memset(pBaseLCU + uCUAddr, Parameter, (CurrPartNumQ >> 1));
                memset(pBaseLCU + uCUAddr + CurrPartNumQ, Parameter, (CurrPartNumQ >> 1));
            }
            else if (uPUIdx == 1)
            {
                memset(pBaseLCU + uCUAddr, Parameter, (CurrPartNumQ >> 1));
                memset(pBaseLCU + uCUAddr + CurrPartNumQ, Parameter, ((CurrPartNumQ >> 1) + (CurrPartNumQ << 1)));
            }
            else
            {
                VM_ASSERT(0);
            }
            break;
        case SIZE_2NxnD:
            if (uPUIdx == 0)
            {
                memset(pBaseLCU + uCUAddr, Parameter, ((CurrPartNumQ << 1) + (CurrPartNumQ >> 1)));
                memset(pBaseLCU + uCUAddr + (CurrPartNumQ << 1) + CurrPartNumQ, Parameter, (CurrPartNumQ >> 1));
            }
            else if (uPUIdx == 1)
            {
                memset(pBaseLCU + uCUAddr, Parameter, (CurrPartNumQ >> 1));
                memset(pBaseLCU + uCUAddr + CurrPartNumQ, Parameter, (CurrPartNumQ >> 1));
            }
            else
            {
                VM_ASSERT(0);
            }
            break;
        case SIZE_nLx2N:
            if (uPUIdx == 0)
            {
                memset(pBaseLCU + uCUAddr, Parameter, (CurrPartNumQ >> 2));
                memset(pBaseLCU + uCUAddr + (CurrPartNumQ >> 1), Parameter, (CurrPartNumQ >> 2));
                memset(pBaseLCU + uCUAddr + (CurrPartNumQ << 1), Parameter, (CurrPartNumQ >> 2));
                memset(pBaseLCU + uCUAddr + (CurrPartNumQ << 1) + (CurrPartNumQ >> 1), Parameter, (CurrPartNumQ >> 2));
            }
            else if (uPUIdx == 1)
            {
                memset(pBaseLCU + uCUAddr, Parameter, (CurrPartNumQ >> 2));
                memset(pBaseLCU + uCUAddr + (CurrPartNumQ >> 1), Parameter, (CurrPartNumQ + (CurrPartNumQ >> 2)));
                memset(pBaseLCU + uCUAddr + (CurrPartNumQ << 1), Parameter, (CurrPartNumQ >> 2));
                memset(pBaseLCU + uCUAddr + (CurrPartNumQ << 1) + (CurrPartNumQ >> 1), Parameter, (CurrPartNumQ + (CurrPartNumQ >> 2)));
            }
            else
            {
                VM_ASSERT(0);
            }
            break;
        case SIZE_nRx2N:
            if (uPUIdx == 0)
            {
                memset(pBaseLCU + uCUAddr, Parameter, (CurrPartNumQ + (CurrPartNumQ >> 2)));
                memset(pBaseLCU + uCUAddr + CurrPartNumQ + (CurrPartNumQ >> 1), Parameter, (CurrPartNumQ >> 2));
                memset(pBaseLCU + uCUAddr + (CurrPartNumQ << 1), Parameter, (CurrPartNumQ + (CurrPartNumQ >> 2)));
                memset(pBaseLCU + uCUAddr + (CurrPartNumQ << 1) + CurrPartNumQ + (CurrPartNumQ >> 1), Parameter, (CurrPartNumQ >> 2));
            }
            else if (uPUIdx == 1)
            {
                memset(pBaseLCU + uCUAddr, Parameter, (CurrPartNumQ >> 2));
                memset(pBaseLCU + uCUAddr + (CurrPartNumQ >> 1), Parameter, (CurrPartNumQ >> 2));
                memset(pBaseLCU + uCUAddr + (CurrPartNumQ << 1), Parameter, (CurrPartNumQ >> 2));
                memset(pBaseLCU + uCUAddr + (CurrPartNumQ << 1) + (CurrPartNumQ >> 1), Parameter, (CurrPartNumQ >> 2));
            }
            else
            {
                VM_ASSERT(0);
            }
            break;
        default:
            VM_ASSERT(0);
    }
}

void H265CodingUnit::setMergeFlagSubParts (bool bMergeFlag, Ipp32u AbsPartIdx, Ipp32u uPartIdx, Ipp32u Depth)
{
    setSubPart(bMergeFlag, m_MergeFlag, AbsPartIdx, Depth, uPartIdx);
}

void H265CodingUnit::setMergeIndexSubParts (Ipp32u uMergeIndex, Ipp32u AbsPartIdx, Ipp32u uPartIdx, Ipp32u Depth)
{
    setSubPart<Ipp8u>((Ipp8u) uMergeIndex, m_MergeIndex, AbsPartIdx, Depth, uPartIdx);
}

void H265CodingUnit::setChromIntraDirSubParts(Ipp32u uDir, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u uCurrPartNumb = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);

    memset(m_ChromaIntraDir + AbsPartIdx, uDir, sizeof(Ipp8u) * uCurrPartNumb);
}

void H265CodingUnit::setInterDirSubParts(Ipp32u uDir, Ipp32u AbsPartIdx, Ipp32u uPartIdx, Ipp32u Depth)
{
    setSubPart<Ipp8u>((Ipp8u) uDir, m_InterDir, AbsPartIdx, Depth, uPartIdx);
}

void H265CodingUnit::setMVPIdxSubParts(Ipp32s iMVPIdx, EnumRefPicList RefPicList, Ipp32u AbsPartIdx, Ipp32u uPartIdx, Ipp32u Depth)
{
    setSubPart<Ipp8s>((Ipp8s) iMVPIdx, m_MVPIdx[RefPicList], AbsPartIdx, Depth, uPartIdx);
}

void H265CodingUnit::setMVPNumSubParts(Ipp32s iMVPNum, EnumRefPicList RefPicList, Ipp32u AbsPartIdx, Ipp32u uPartIdx, Ipp32u Depth)
{
    setSubPart<Ipp8s>((Ipp8s) iMVPNum, m_MVPNum[RefPicList], AbsPartIdx, Depth, uPartIdx);
}


void H265CodingUnit::setTrIdxSubParts(Ipp32u uTrIdx, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u CurrPartNumb = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);

    memset(m_TrIdxArray + AbsPartIdx, uTrIdx, sizeof(Ipp8u) * CurrPartNumb);
}

void H265CodingUnit::setTrStartSubParts(Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u CurrPartNumb = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);

    memset(m_TrStartArray + AbsPartIdx, AbsPartIdx, sizeof(Ipp8u) * CurrPartNumb);
}

void H265CodingUnit::setSizeSubParts(Ipp32u Width, Ipp32u Height, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u CurrPartNumb = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);

    memset(m_WidthArray + AbsPartIdx, Width,  sizeof(Ipp8u) * CurrPartNumb);
    memset(m_HeightArray + AbsPartIdx, Height, sizeof(Ipp8u) * CurrPartNumb);
}

Ipp8u H265CodingUnit::getNumPartInter(Ipp32u AbsPartIdx)
{
    Ipp8u iNumPart = 0;

    switch (m_PartSizeArray[AbsPartIdx])
    {
        case SIZE_2Nx2N:
            iNumPart = 1;
            break;
        case SIZE_2NxN:
            iNumPart = 2;
            break;
        case SIZE_Nx2N:
            iNumPart = 2;
            break;
        case SIZE_NxN:
            iNumPart = 4;
            break;
        case SIZE_2NxnU:
            iNumPart = 2;
            break;
        case SIZE_2NxnD:
            iNumPart = 2;
            break;
        case SIZE_nLx2N:
            iNumPart = 2;
            break;
        case SIZE_nRx2N:
            iNumPart = 2;
            break;
        default:
            VM_ASSERT(0);
            break;
    }

    return  iNumPart;
}

void H265CodingUnit::getPartIndexAndSize(Ipp32u AbsPartIdx, Ipp32u uPartIdx, Ipp32u &PartAddr, Ipp32u &Width, Ipp32u &Height)
{
    switch (m_PartSizeArray[AbsPartIdx])
    {
        case SIZE_2NxN:
            Width = m_WidthArray[AbsPartIdx];
            Height = m_HeightArray[AbsPartIdx] >> 1;
            PartAddr = (uPartIdx == 0) ? 0 : m_NumPartition >> 1;
            break;
        case SIZE_Nx2N:
            Width = m_WidthArray[AbsPartIdx] >> 1;
            Height = m_HeightArray[AbsPartIdx];
            PartAddr = (uPartIdx == 0) ? 0 : m_NumPartition >> 2;
            break;
        case SIZE_NxN:
            Width = m_WidthArray[AbsPartIdx] >> 1;
            Height = m_HeightArray[AbsPartIdx] >> 1;
            PartAddr = (m_NumPartition >> 2) * uPartIdx;
            break;
        case SIZE_2NxnU:
            Width    = m_WidthArray[AbsPartIdx];
            Height   = (uPartIdx == 0) ? m_HeightArray[AbsPartIdx] >> 2 : (m_HeightArray[AbsPartIdx] >> 2) + (m_HeightArray[AbsPartIdx] >> 1);
            PartAddr = (uPartIdx == 0) ? 0 : m_NumPartition >> 3;
            break;
        case SIZE_2NxnD:
            Width    = m_WidthArray[AbsPartIdx];
            Height   = (uPartIdx == 0) ?  (m_HeightArray[AbsPartIdx] >> 2) + (m_HeightArray[AbsPartIdx] >> 1) : m_HeightArray[AbsPartIdx] >> 2;
            PartAddr = (uPartIdx == 0) ? 0 : (m_NumPartition >> 1) + (m_NumPartition >> 3);
            break;
        case SIZE_nLx2N:
            Width    = (uPartIdx == 0) ? m_WidthArray[AbsPartIdx] >> 2 : (m_WidthArray[AbsPartIdx] >> 2) + (m_WidthArray[AbsPartIdx] >> 1);
            Height   = m_HeightArray[AbsPartIdx];
            PartAddr = (uPartIdx == 0) ? 0 : m_NumPartition >> 4;
            break;
        case SIZE_nRx2N:
            Width    = (uPartIdx == 0) ? (m_WidthArray[AbsPartIdx] >> 2) + (m_WidthArray[AbsPartIdx] >> 1) : m_WidthArray[AbsPartIdx] >> 2;
            Height   = m_HeightArray[AbsPartIdx];
            PartAddr = (uPartIdx == 0) ? 0 : (m_NumPartition >> 2) + (m_NumPartition >> 4);
            break;
        default:
            VM_ASSERT(m_PartSizeArray[AbsPartIdx] == SIZE_2Nx2N);
            Width = m_WidthArray[AbsPartIdx];
            Height = m_HeightArray[AbsPartIdx];
            PartAddr = 0;
            break;
    }
}

void H265CodingUnit::getMVBuffer (H265CodingUnit* CU, Ipp32u AbsPartIdx, EnumRefPicList RefPicList, MVBuffer& MVbuffer)
{
    if (CU == NULL)  // OUT OF BOUNDARY
    {
        H265MotionVector ZeroMV;
        MVbuffer.MV = ZeroMV;
        MVbuffer.RefIdx = NOT_VALID;
        return;
    }

    CUMVBuffer*  pCUMVbuffer = &(CU->m_CUMVbuffer[RefPicList]);
    MVbuffer.MV = pCUMVbuffer->MV[AbsPartIdx];
    MVbuffer.RefIdx = pCUMVbuffer->RefIdx[AbsPartIdx];
}

void H265CodingUnit::deriveLeftRightTopIdxAdi (Ipp32u& PartIdxLT, Ipp32u& PartIdxRT, Ipp32u uPartOffset, Ipp32u uPartDepth)
{
    Ipp32u NumPartInWidth = (m_WidthArray[0] / m_Frame->getMinCUWidth()) >> uPartDepth;
    PartIdxLT = m_AbsIdxInLCU + uPartOffset;
    PartIdxRT = g_RasterToZscan[g_ZscanToRaster[PartIdxLT] + NumPartInWidth - 1];
}

void H265CodingUnit::deriveLeftBottomIdxAdi (Ipp32u& PartIdxLB, Ipp32u uPartOffset, Ipp32u uPartDepth)
{
    Ipp32u AbsIdx;
    Ipp32u MinCuWidth, WidthInMinCus;

    MinCuWidth = m_Frame->getMinCUWidth();
    WidthInMinCus = (m_WidthArray[0] / MinCuWidth) >> uPartDepth;
    AbsIdx = m_AbsIdxInLCU + uPartOffset + (m_NumPartition >> (uPartDepth << 1)) - 1;
    AbsIdx = g_ZscanToRaster[AbsIdx] - (WidthInMinCus - 1);
    PartIdxLB = g_RasterToZscan[AbsIdx];
}

bool H265CodingUnit::hasEqualMotion (Ipp32u AbsPartIdx, H265CodingUnit* CandCU, Ipp32u CandAbsPartIdx)
{
    VM_ASSERT( m_InterDir[AbsPartIdx] != 0 );

    if ( m_InterDir[AbsPartIdx] != CandCU->m_InterDir[CandAbsPartIdx] )
    {
        return false;
    }

    for ( Ipp32u uRefListIdx = 0; uRefListIdx < 2; uRefListIdx++ )
    {
        if ( m_InterDir[AbsPartIdx] & ( 1 << uRefListIdx ) )
        {
            if ( m_CUMVbuffer[EnumRefPicList(uRefListIdx)].MV[AbsPartIdx] != CandCU->m_CUMVbuffer[EnumRefPicList(uRefListIdx)].MV[CandAbsPartIdx] ||
                m_CUMVbuffer[EnumRefPicList(uRefListIdx)].RefIdx[AbsPartIdx] != CandCU->m_CUMVbuffer[EnumRefPicList(uRefListIdx)].RefIdx[CandAbsPartIdx])
            {
                return false;
            }
        }
    }

    return true;
}

void H265CodingUnit::getPartSize(Ipp32u AbsPartIdx, Ipp32u partIdx, Ipp32s &nPSW, Ipp32s &nPSH)
{
    switch (m_PartSizeArray[AbsPartIdx])
    {
    case SIZE_2NxN:
        nPSW = m_WidthArray[AbsPartIdx];
        nPSH = m_HeightArray[AbsPartIdx] >> 1;
        break;
    case SIZE_Nx2N:
        nPSW = m_WidthArray[AbsPartIdx] >> 1;
        nPSH = m_HeightArray[AbsPartIdx];
        break;
    case SIZE_NxN:
        nPSW = m_WidthArray[AbsPartIdx] >> 1;
        nPSH = m_HeightArray[AbsPartIdx] >> 1;
        break;
    case SIZE_2NxnU:
        nPSW = m_WidthArray[AbsPartIdx];
        nPSH = (partIdx == 0) ? m_HeightArray[AbsPartIdx] >> 2 : (m_HeightArray[AbsPartIdx] >> 2) + (m_HeightArray[AbsPartIdx] >> 1);
        break;
    case SIZE_2NxnD:
        nPSW = m_WidthArray[AbsPartIdx];
        nPSH = (partIdx == 0) ?  (m_HeightArray[AbsPartIdx] >> 2) + (m_HeightArray[AbsPartIdx] >> 1) : m_HeightArray[AbsPartIdx] >> 2;
        break;
    case SIZE_nLx2N:
        nPSW = (partIdx == 0) ? m_WidthArray[AbsPartIdx] >> 2 : (m_WidthArray[AbsPartIdx] >> 2) + (m_WidthArray[AbsPartIdx] >> 1);
        nPSH = m_HeightArray[AbsPartIdx];
        break;
    case SIZE_nRx2N:
        nPSW = (partIdx == 0) ? (m_WidthArray[AbsPartIdx] >> 2) + (m_WidthArray[AbsPartIdx] >> 1) : m_WidthArray[AbsPartIdx] >> 2;
        nPSH = m_HeightArray[AbsPartIdx];
        break;
    default:
        VM_ASSERT(m_PartSizeArray[AbsPartIdx] == SIZE_2Nx2N);
        nPSW = m_WidthArray[AbsPartIdx];
        nPSH = m_HeightArray[AbsPartIdx];
        break;
    }
}

Ipp32s H265CodingUnit::searchMVPIdx(H265MotionVector cMv, AMVPInfo* pInfo)
{
    for (Ipp32s i = 0; i < pInfo->NumbOfCands; i++)
    {
        if (cMv == pInfo->MVCandidate[i])
        return i;
    }

    VM_ASSERT(0);
    return -1;
}

/** Set a I_PCM flag for all sub-partitions of a partition.
 * \param bIpcmFlag I_PCM flag
 * \param uiAbsPartIdx patition index
 * \param uiDepth CU depth
 * \returns Void
 */
void H265CodingUnit::setIPCMFlagSubParts (bool IpcmFlag, Ipp32u AbsPartIdx, Ipp32u Depth)
{
    Ipp32u CurrPartNumb = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);

    memset(m_IPCMFlag + AbsPartIdx, IpcmFlag, sizeof(bool) * CurrPartNumb);
}

/** Test whether the current block is skipped
 * \param uiPartIdx Block index
 * \returns Flag indicating whether the block is skipped
 */
bool H265CodingUnit::isSkipped(Ipp32u PartIdx)
{
    return m_skipFlag[PartIdx];
}

bool H265CodingUnit::isBipredRestriction(Ipp32u AbsPartIdx, Ipp32u PartIdx)
{
    Ipp32s width = 0;
    Ipp32s height = 0;

    getPartSize(AbsPartIdx, PartIdx, width, height);
    if (m_WidthArray[AbsPartIdx] == 8 && (width < 8 || height < 8))
    {
        return true;
    }
    return false;
}

Ipp32u H265CodingUnit::getCoefScanIdx(Ipp32u AbsPartIdx, Ipp32u Width, bool IsLuma, bool IsIntra)
{
    Ipp32u CTXIdx;
    Ipp32u ScanIdx;
    Ipp32u DirMode;

    if (!IsIntra)
    {
        ScanIdx = SCAN_DIAG;
        return ScanIdx;
    }

    switch (Width)
    {
        case  2:
            CTXIdx = 6;
            break;
        case  4:
            CTXIdx = 5;
            break;
        case  8:
            CTXIdx = 4;
            break;
        case 16:
            CTXIdx = 3;
            break;
        case 32:
            CTXIdx = 2;
            break;
        case 64:
            CTXIdx = 1;
            break;
        default:
            CTXIdx = 0;
            break;
    }

    if (IsLuma)
    {
        DirMode = m_LumaIntraDir[AbsPartIdx];
        ScanIdx = SCAN_DIAG;
        if (CTXIdx > 3 && CTXIdx < 6) //if multiple scans supported for PU size
        {
            ScanIdx = abs((Ipp32s)DirMode - VER_IDX) < 5 ? SCAN_HOR : (abs((Ipp32s)DirMode - HOR_IDX) < 5 ? SCAN_VER : SCAN_DIAG);
        }
    }
    else
    {
        DirMode = m_ChromaIntraDir[AbsPartIdx];
        if (DirMode == DM_CHROMA_IDX)
        {
            // get number of partitions in current CU
            Ipp32u depth = m_DepthArray[AbsPartIdx];
            Ipp32u numParts = m_Frame->getCD()->getNumPartInCU() >> (2 * depth);

            // get luma mode from upper-left corner of current CU
            DirMode = m_LumaIntraDir[(AbsPartIdx / numParts) * numParts];
        }
        ScanIdx = SCAN_DIAG;
        if (CTXIdx > 4 && CTXIdx < 7) //if multiple scans supported for PU size
        {
            ScanIdx = abs((Ipp32s)DirMode - VER_IDX) < 5 ? SCAN_HOR : (abs((Ipp32s)DirMode - HOR_IDX) < 5 ? SCAN_VER : SCAN_DIAG);
        }
    }

    return ScanIdx;
}

Ipp32u H265CodingUnit::getSCUAddr()
{
    return m_Frame->m_CodingData->GetInverseCUOrderMap(CUAddr) * (1 << (m_SliceHeader->m_SeqParamSet->MaxCUDepth << 1)) + m_AbsIdxInLCU;
}

#define SET_BORDER_AVAILABILITY(borderIndex, picBoundary, refCUAddr)                           \
{                                                                                              \
    if (picBoundary)                                                                           \
    {                                                                                          \
        m_AvailBorder[borderIndex] = false;                                                    \
    }                                                                                          \
    else if (m_Frame->m_iNumberOfSlices == 1)                                                  \
    {                                                                                          \
        m_AvailBorder[borderIndex] = true;                                                     \
    }                                                                                          \
    else                                                                                       \
    {                                                                                          \
        H265CodingUnit* refCU = m_Frame->getCU(refCUAddr);                                     \
        Ipp32s          refID = refCU->m_SliceIdx;                                             \
                                                                                               \
        m_AvailBorder[borderIndex] = (refID == m_SliceIdx )? (true) : ((refID > m_SliceIdx) ?  \
                                     (refCU->m_SliceHeader->m_LFCrossSliceBoundaryFlag):       \
                                     (m_SliceHeader->m_LFCrossSliceBoundaryFlag));             \
    }                                                                                          \
}

void H265CodingUnit::setNDBFilterBlockBorderAvailability(bool independentTileBoundaryEnabled)
{
    bool picLBoundary = ((m_CUPelX == 0) ? true : false);
    bool picTBoundary = ((m_CUPelY == 0) ? true : false);
    bool picRBoundary = ((m_CUPelX + m_SliceHeader->m_SeqParamSet->MaxCUWidth >= m_SliceHeader->m_SeqParamSet->pic_width_in_luma_samples)   ? true : false);
    bool picBBoundary = ((m_CUPelY + m_SliceHeader->m_SeqParamSet->MaxCUHeight >= m_SliceHeader->m_SeqParamSet->pic_height_in_luma_samples) ? true : false);
    bool leftTileBoundary= false;
    bool rightTileBoundary= false;
    bool topTileBoundary = false;
    bool bottomTileBoundary= false;
    Ipp32s numLCUInPicWidth = m_Frame->m_CodingData->m_WidthInCU;
    Ipp32s tileID = m_Frame->m_CodingData->getTileIdxMap(CUAddr);

    if (independentTileBoundaryEnabled)
    {
        if (!picLBoundary)
        {
            leftTileBoundary = ((m_Frame->m_CodingData->getTileIdxMap(CUAddr - 1) != tileID) ? true : false);
        }

        if (!picRBoundary)
        {
            rightTileBoundary = ((m_Frame->m_CodingData->getTileIdxMap(CUAddr + 1) != tileID) ? true : false);
        }

        if (!picTBoundary)
        {
            topTileBoundary = ((m_Frame->m_CodingData->getTileIdxMap(CUAddr - numLCUInPicWidth) !=  tileID) ? true : false);
        }

        if (!picBBoundary)
        {
            bottomTileBoundary = ((m_Frame->m_CodingData->getTileIdxMap(CUAddr + numLCUInPicWidth) !=  tileID) ? true : false);
        }
    }

    SET_BORDER_AVAILABILITY(SGU_L, picLBoundary, CUAddr - 1)
    SET_BORDER_AVAILABILITY(SGU_R, picRBoundary, CUAddr + 1)
    SET_BORDER_AVAILABILITY(SGU_T, picTBoundary, CUAddr - numLCUInPicWidth)
    SET_BORDER_AVAILABILITY(SGU_B, picBBoundary, CUAddr + numLCUInPicWidth)
    SET_BORDER_AVAILABILITY(SGU_TL, picTBoundary || picLBoundary, CUAddr - numLCUInPicWidth - 1)
    SET_BORDER_AVAILABILITY(SGU_TR, picTBoundary || picRBoundary, CUAddr - numLCUInPicWidth + 1)
    SET_BORDER_AVAILABILITY(SGU_BL, picBBoundary || picLBoundary, CUAddr + numLCUInPicWidth - 1)
    SET_BORDER_AVAILABILITY(SGU_BR, picBBoundary || picRBoundary, CUAddr + numLCUInPicWidth + 1)

    if (independentTileBoundaryEnabled)
    {
        if (leftTileBoundary)
        {
            m_AvailBorder[SGU_L] = m_AvailBorder[SGU_TL] = m_AvailBorder[SGU_BL] = false;
        }

        if( rightTileBoundary)
        {
            m_AvailBorder[SGU_R] = m_AvailBorder[SGU_TR] = m_AvailBorder[SGU_BR] = false;
        }

        if (topTileBoundary)
        {
            m_AvailBorder[SGU_T] = m_AvailBorder[SGU_TL] = m_AvailBorder[SGU_TR] = false;
        }

        if (bottomTileBoundary)
        {
            m_AvailBorder[SGU_B] = m_AvailBorder[SGU_BL] = m_AvailBorder[SGU_BR] = false;
        }
    }
}

// set
template <typename T>
void H265CodingUnit::setAll(T *p, const T &val, Ipp32u PartWidth, Ipp32u PartHeight)
{
    Ipp32s stride = m_Frame->getNumPartInCUSize() * m_Frame->getFrameWidthInCU();

    for (Ipp32u y = 0; y < PartHeight; y++)
    {
        for (Ipp32u x = 0; x < PartWidth; x++)
            p[x] = val;

        p += stride;
    }
};

void H265CodingUnit::setAllColFlags(const Ipp8u flags, Ipp32u RefListIdx, Ipp32u PartX, Ipp32u PartY, Ipp32u PartWidth, Ipp32u PartHeight)
{
    setAll(m_Frame->m_CodingData->m_ColTUFlags[RefListIdx] + m_Frame->getNumPartInCUSize() * m_Frame->getFrameWidthInCU() * PartY + PartX,
        flags, PartWidth, PartHeight);
}

void H265CodingUnit::setAllColPOCDelta(const Ipp32s POCDelta, Ipp32u RefListIdx, Ipp32u PartX, Ipp32u PartY, Ipp32u PartWidth, Ipp32u PartHeight)
{
    setAll(m_Frame->m_CodingData->m_ColTUPOCDelta[RefListIdx] + m_Frame->getNumPartInCUSize() * m_Frame->getFrameWidthInCU() * PartY + PartX,
        POCDelta, PartWidth, PartHeight);
}

void H265CodingUnit::setAllColMV(const H265MotionVector &mv, Ipp32u RefListIdx, Ipp32u PartX, Ipp32u PartY, Ipp32u PartWidth, Ipp32u PartHeight)
{
    setAll(m_Frame->m_CodingData->m_ColTUMV[RefListIdx] + m_Frame->getNumPartInCUSize() * m_Frame->getFrameWidthInCU() * PartY + PartX,
        mv, PartWidth, PartHeight);
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
