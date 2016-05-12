/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#if defined (MFX_VA) && !defined(OSX)

#include "cm_mem_copy.h"
#include "cm_gpu_copy_code.h"

typedef const mfxU8 mfxUC8;

#define ALIGN128(X) (((mfxU32)((X)+127)) & (~ (mfxU32)127))

#define CHECK_CM_STATUS(_sts, _ret)              \
        if (CM_SUCCESS != _sts)                  \
        {                                           \
            return _ret;                        \
        }

#define CHECK_CM_STATUS_RET_NULL(_sts, _ret)              \
        if (CM_SUCCESS != _sts)                  \
        {                                           \
            return NULL;                        \
        }

#define CHECK_CM_NULL_PTR(_ptr, _ret)              \
        if (NULL == _ptr)                  \
        {                                           \
            return _ret;                        \
        }

#define THREAD_SPACE
CmCopyWrapper::CmCopyWrapper()
{
    m_pCmProgram = NULL;
    m_pCmDevice  = NULL;

    m_pCmSurface2D = NULL;
    m_pCmUserBuffer = NULL;

    m_pCmSrcIndex = NULL;
    m_pCmDstIndex = NULL;

    m_pCmQueue = NULL;
    m_pCmTask1 = NULL;
    m_pCmTask2 = NULL;

    m_pThreadSpace = NULL;


    m_cachedObjects.clear();

    m_tableCmRelations.clear();
    m_tableSysRelations.clear();

    m_tableCmIndex.clear();
    m_tableSysIndex.clear();

    m_tableCmRelations2.clear();
    m_tableSysRelations2.clear();

    m_tableCmIndex2.clear();
    m_tableSysIndex2.clear();

} // CmCopyWrapper::CmCopyWrapper(void)
CmCopyWrapper::~CmCopyWrapper(void)
{
    Release();

} // CmCopyWrapper::~CmCopyWrapper(void)

#define ADDRESS_PAGE_ALIGNMENT_MASK_X64             0xFFFFFFFFFFFFF000ULL
#define ADDRESS_PAGE_ALIGNMENT_MASK_X86             0xFFFFF000
#define INNER_LOOP                   (4)

#define CHECK_CM_HR(HR) \
    if(HR != CM_SUCCESS)\
    {\
        if(pTS)           m_pCmDevice->DestroyThreadSpace(pTS);\
        if(pGPUCopyTask)  m_pCmDevice->DestroyTask(pGPUCopyTask);\
        if(pCMBufferUP)   m_pCmDevice->DestroyBufferUP(pCMBufferUP);\
        if(pInternalEvent)m_pCmQueue->DestroyEvent(pInternalEvent);\
        return MFX_ERR_DEVICE_FAILED;\
    }

mfxStatus CmCopyWrapper::EnqueueCopySwapRBGPUtoCPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride, 
                                    const UINT heightStride,
                                    mfxU32 format, 
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    INT             hr                      = CM_SUCCESS;
    UINT            sizePerPixel            = (format==MFX_FOURCC_ARGB16||format==MFX_FOURCC_ABGR16)? 8: 4;//RGB now
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;
    CmKernel        *m_pCmKernel            = 0;
    CmBufferUP        *pCMBufferUP          = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_dword             = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0; 
    UINT            slice_copy_height_row   = 0;
    UINT            sliceCopyBufferUPSize   = 0;
    INT             totalBufferUPSize       = 0;
    UINT            start_x                 = 0;
    UINT            start_y                 = 0;

    

    if ( !pSurface )
    {
        return MFX_ERR_NULL_PTR;
    }
    width_byte                      = width * sizePerPixel;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region 
    copy_width_byte = IPP_MIN(stride_in_bytes, width_byte);
    copy_height_row = IPP_MIN(height_stride_in_rows, (UINT)height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory
    
    totalBufferUPSize = stride_in_bytes * height_stride_in_rows;

    pLinearAddress  = (size_t)pSysMem;
    

    while (totalBufferUPSize > 0)
    {
#if defined(WIN64) || defined(LINUX64)//64-bit 
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;
#else  //32-bit 
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X86;
#endif

        //Calculate  Left Shift offset
        AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
        totalBufferUPSize   += AddedShiftLeftOffset;
        if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH)
        {
            slice_copy_height_row = ((CM_MAX_1D_SURF_WIDTH - AddedShiftLeftOffset)/(stride_in_bytes*(BLOCK_HEIGHT * INNER_LOOP))) * (BLOCK_HEIGHT * INNER_LOOP);
            sliceCopyBufferUPSize = slice_copy_height_row * stride_in_bytes + AddedShiftLeftOffset;
        }
        else
        {
            slice_copy_height_row = copy_height_row;
            sliceCopyBufferUPSize = totalBufferUPSize;
        }
        hr = m_pCmDevice->CreateBufferUP(  sliceCopyBufferUPSize, ( void * )pLinearAddressAligned, pCMBufferUP );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_readswap_32x32), m_pCmKernel);
        CHECK_CM_HR(hr);
        MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

        MFX_CHECK(pCMBufferUP, MFX_ERR_DEVICE_FAILED);
        hr = pCMBufferUP->GetIndex( pBufferIndexCM );
        CHECK_CM_HR(hr);
        hr = pSurface->GetIndex( pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
        threadHeight = ( UINT )ceil( ( double )slice_copy_height_row/BLOCK_HEIGHT/INNER_LOOP );
        threadNum = threadWidth * threadHeight;
        hr = m_pCmKernel->SetThreadCount( threadNum );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pBufferIndexCM );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        width_dword = (UINT)ceil((double)width_byte / 4);
        stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

        hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &stride_in_dwords );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height_stride_in_rows );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &threadHeight );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &width_dword );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 7, sizeof( UINT ), &slice_copy_height_row );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 8, sizeof( UINT ), &sizePerPixel );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 9, sizeof( UINT ), &start_x );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 10, sizeof( UINT ), &start_y );
        CHECK_CM_HR(hr);

        hr = m_pCmDevice->CreateTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = pGPUCopyTask->AddKernel( m_pCmKernel );
        CHECK_CM_HR(hr);
        hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyThreadSpace(pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyBufferUP(pCMBufferUP);
        CHECK_CM_HR(hr);
        pLinearAddress += sliceCopyBufferUPSize - AddedShiftLeftOffset;
        totalBufferUPSize -= sliceCopyBufferUPSize;
        copy_height_row -= slice_copy_height_row;
        start_x = 0;
        start_y += slice_copy_height_row;
        if(totalBufferUPSize > 0)   //Intermediate event, we don't need it
        {
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        else //Last one event, need keep or destroy it
        {
            hr = pInternalEvent->WaitForTaskFinished();
            CHECK_CM_HR(hr);
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
            CHECK_CM_HR(hr);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopySwapRBCPUtoGPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride, 
                                    const UINT heightStride,
                                    mfxU32 format, 
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    INT             hr                      = CM_SUCCESS;
    UINT            sizePerPixel            = (format==MFX_FOURCC_ARGB16||format==MFX_FOURCC_ABGR16)? 8: 4;//RGB now
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;
    
    CmKernel        *m_pCmKernel            = 0;
    CmBufferUP      *pCMBufferUP          = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_dword             = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0;
    UINT            slice_copy_height_row   = 0;
    UINT            sliceCopyBufferUPSize   = 0;
    INT             totalBufferUPSize       = 0;
    UINT            start_x                 = 0;
    UINT            start_y                 = 0;

    

    if ( !pSurface )
    {
        return MFX_ERR_NULL_PTR;
    }
    width_byte                      = width * sizePerPixel;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region 
    copy_width_byte = IPP_MIN(stride_in_bytes, width_byte);
    copy_height_row = IPP_MIN(height_stride_in_rows, (UINT)height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory
    
    totalBufferUPSize = stride_in_bytes * height_stride_in_rows;

    pLinearAddress  = (size_t)pSysMem;
    

    while (totalBufferUPSize > 0)
    {
#if defined(WIN64) || defined(LINUX64)//64-bit 
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;
#else  //32-bit 
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X86;
#endif

        //Calculate  Left Shift offset
        AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
        totalBufferUPSize   += AddedShiftLeftOffset;
        if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH)
        {
            slice_copy_height_row = ((CM_MAX_1D_SURF_WIDTH - AddedShiftLeftOffset)/(stride_in_bytes*(BLOCK_HEIGHT * INNER_LOOP))) * (BLOCK_HEIGHT * INNER_LOOP);
            sliceCopyBufferUPSize = slice_copy_height_row * stride_in_bytes + AddedShiftLeftOffset;
        }
        else
        {
            slice_copy_height_row = copy_height_row;
            sliceCopyBufferUPSize = totalBufferUPSize;
        }
        hr = m_pCmDevice->CreateBufferUP(  sliceCopyBufferUPSize, ( void * )pLinearAddressAligned, pCMBufferUP );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_writeswap_32x32), m_pCmKernel);
        CHECK_CM_HR(hr);

        
        MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

        MFX_CHECK(pCMBufferUP, MFX_ERR_DEVICE_FAILED);
        hr = pCMBufferUP->GetIndex( pBufferIndexCM );
        CHECK_CM_HR(hr);
        hr = pSurface->GetIndex( pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
        threadHeight = ( UINT )ceil( ( double )slice_copy_height_row/BLOCK_HEIGHT/INNER_LOOP );
        threadNum = threadWidth * threadHeight;
        hr = m_pCmKernel->SetThreadCount( threadNum );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
        CHECK_CM_HR(hr);

        m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pBufferIndexCM);
        CHECK_CM_HR(hr);
        m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pSurf2DIndexCM);
        CHECK_CM_HR(hr);

            
        width_dword = (UINT)ceil((double)width_byte / 4);
        stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

        hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &stride_in_dwords );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &slice_copy_height_row );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &threadHeight );
        CHECK_CM_HR(hr);

        //this only works for the kernel surfaceCopy_write_32x32
        hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &sizePerPixel );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 7, sizeof( UINT ), &start_x );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 8, sizeof( UINT ), &start_y );
        CHECK_CM_HR(hr);

        hr = m_pCmDevice->CreateTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = pGPUCopyTask->AddKernel( m_pCmKernel );
        CHECK_CM_HR(hr);
        hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyThreadSpace(pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyBufferUP(pCMBufferUP);
        CHECK_CM_HR(hr);

        pLinearAddress += sliceCopyBufferUPSize - AddedShiftLeftOffset;
        totalBufferUPSize -= sliceCopyBufferUPSize;
        copy_height_row -= slice_copy_height_row;
        start_x = 0;
        start_y += slice_copy_height_row;
        if(totalBufferUPSize > 0)   //Intermediate event, we don't need it
        {
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        else //Last one event, need keep or destroy it
        {
            hr = pInternalEvent->WaitForTaskFinished();
            CHECK_CM_HR(hr);
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
            CHECK_CM_HR(hr);
        }

    }

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopySwapRBGPUtoGPU(   CmSurface2D* pSurfaceIn,
                                    CmSurface2D* pSurfaceOut,
                                    int width,
                                    int height,
                                    mfxU32 format, 
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    INT             hr                      = CM_SUCCESS;
    UINT            sizePerPixel            = (format==MFX_FOURCC_ARGB16||format==MFX_FOURCC_ABGR16)? 8: 4;//RGB now
    
    SurfaceIndex    *pSurf2DIndexCM_In  = NULL;
    SurfaceIndex    *pSurf2DIndexCM_Out = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;
    
    CmKernel        *m_pCmKernel            = 0;
    CmBufferUP      *pCMBufferUP            = 0;
    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;

    

    if ( !pSurfaceIn || !pSurfaceOut )
    {
        return MFX_ERR_NULL_PTR;
    }
    
    hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(SurfaceCopySwap_2DTo2D_32x32), m_pCmKernel);
    CHECK_CM_HR(hr);
    
    MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

    hr = pSurfaceOut->GetIndex( pSurf2DIndexCM_Out );
    CHECK_CM_HR(hr);
    hr = pSurfaceIn->GetIndex( pSurf2DIndexCM_In );
    CHECK_CM_HR(hr);

    threadWidth = ( UINT )ceil( ( double )width/BLOCK_PIXEL_WIDTH );
    threadHeight = ( UINT )ceil( ( double )height/BLOCK_HEIGHT/INNER_LOOP );
    threadNum = threadWidth * threadHeight;
    hr = m_pCmKernel->SetThreadCount( threadNum );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
    CHECK_CM_HR(hr);

    m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pSurf2DIndexCM_In);
    m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pSurf2DIndexCM_Out);
    
    hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &threadHeight );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &sizePerPixel );
    CHECK_CM_HR(hr);

    hr = m_pCmDevice->CreateTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = pGPUCopyTask->AddKernel( m_pCmKernel );
    CHECK_CM_HR(hr);
    hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
    CHECK_CM_HR(hr);

    hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyThreadSpace(pTS);
    CHECK_CM_HR(hr);

    hr = pInternalEvent->WaitForTaskFinished();
    CHECK_CM_HR(hr);
    hr = m_pCmQueue->DestroyEvent(pInternalEvent);
    CHECK_CM_HR(hr);
    
    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopyMirrorGPUtoGPU(   CmSurface2D* pSurfaceIn,
                                    CmSurface2D* pSurfaceOut,
                                    int width,
                                    int height,
                                    mfxU32 format, 
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    INT             hr                      = CM_SUCCESS;
//    UINT            sizePerPixel            = (format==MFX_FOURCC_ARGB16||format==MFX_FOURCC_ABGR16)? 8: 4;//RGB now
    
    SurfaceIndex    *pSurf2DIndexCM_In  = NULL;
    SurfaceIndex    *pSurf2DIndexCM_Out = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;
    
    CmKernel        *m_pCmKernel            = 0;
    CmBufferUP      *pCMBufferUP            = 0;
    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;

    

    if ( !pSurfaceIn || !pSurfaceOut )
    {
        return MFX_ERR_NULL_PTR;
    }
    
    hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(SurfaceMirror_2DTo2D_NV12), m_pCmKernel);
    CHECK_CM_HR(hr);
    
    MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

    hr = pSurfaceOut->GetIndex( pSurf2DIndexCM_Out );
    CHECK_CM_HR(hr);
    hr = pSurfaceIn->GetIndex( pSurf2DIndexCM_In );
    CHECK_CM_HR(hr);

    threadWidth = ( UINT )ceil( ( double )width/BLOCK_PIXEL_WIDTH );
    threadHeight = ( UINT )ceil( ( double )height/BLOCK_HEIGHT );
    threadNum = threadWidth * threadHeight;
    hr = m_pCmKernel->SetThreadCount( threadNum );
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
    CHECK_CM_HR(hr);

    m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pSurf2DIndexCM_In);
    m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pSurf2DIndexCM_Out);
    
    hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &width );
    CHECK_CM_HR(hr);
    hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height );
    CHECK_CM_HR(hr);

    hr = m_pCmDevice->CreateTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = pGPUCopyTask->AddKernel( m_pCmKernel );
    CHECK_CM_HR(hr);
    hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
    CHECK_CM_HR(hr);

    hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
    CHECK_CM_HR(hr);
    hr = m_pCmDevice->DestroyThreadSpace(pTS);
    CHECK_CM_HR(hr);

    hr = pInternalEvent->WaitForTaskFinished();
    CHECK_CM_HR(hr);
    hr = m_pCmQueue->DestroyEvent(pInternalEvent);
    CHECK_CM_HR(hr);
    
    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopyMirrorNV12GPUtoCPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride, 
                                    const UINT heightStride,
                                    mfxU32 format, 
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    INT             hr                      = CM_SUCCESS;
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;
    CmKernel        *m_pCmKernel            = 0;
    CmBufferUP        *pCMBufferUP          = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_dword             = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0; 
    UINT            slice_copy_height_row   = 0;
    UINT            sliceCopyBufferUPSize   = 0;
    INT             totalBufferUPSize       = 0;
    UINT            start_x                 = 0;
    UINT            start_y                 = 0;

    

    if ( !pSurface )
    {
        return MFX_ERR_NULL_PTR;
    }
    width_byte                      = width;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region 
    copy_width_byte = IPP_MIN(stride_in_bytes, width_byte);
    copy_height_row = IPP_MIN(height_stride_in_rows, (UINT)height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory
    
    totalBufferUPSize = stride_in_bytes * height_stride_in_rows + stride_in_bytes * height/2;

    pLinearAddress  = (size_t)pSysMem;
    

    while (totalBufferUPSize > 0)
    {
#if defined(WIN64) || defined(LINUX64)//64-bit 
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;
#else  //32-bit 
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X86;
#endif

        //Calculate  Left Shift offset
        AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
        totalBufferUPSize   += AddedShiftLeftOffset;
        if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH)
        {
            slice_copy_height_row = ((CM_MAX_1D_SURF_WIDTH - AddedShiftLeftOffset)/(stride_in_bytes*(BLOCK_HEIGHT * INNER_LOOP))) * (BLOCK_HEIGHT * INNER_LOOP);
            sliceCopyBufferUPSize = slice_copy_height_row * stride_in_bytes * 3 / 2  + AddedShiftLeftOffset;
            return MFX_ERR_DEVICE_FAILED;
        }
        else
        {
            slice_copy_height_row = copy_height_row;
            sliceCopyBufferUPSize = totalBufferUPSize;
        }
        hr = m_pCmDevice->CreateBufferUP(  sliceCopyBufferUPSize, ( void * )pLinearAddressAligned, pCMBufferUP );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceMirror_read_NV12), m_pCmKernel);
        CHECK_CM_HR(hr);
        MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

        MFX_CHECK(pCMBufferUP, MFX_ERR_DEVICE_FAILED);
        hr = pCMBufferUP->GetIndex( pBufferIndexCM );
        CHECK_CM_HR(hr);
        hr = pSurface->GetIndex( pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
        threadHeight = ( UINT )ceil( ( double )slice_copy_height_row/BLOCK_HEIGHT );
        threadNum = threadWidth * threadHeight;
        hr = m_pCmKernel->SetThreadCount( threadNum );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pBufferIndexCM );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        width_dword = (UINT)ceil((double)width_byte / 4);
        stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

        hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &stride_in_dwords );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &width_dword );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &height_stride_in_rows );
        CHECK_CM_HR(hr);
        //hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &threadHeight );
        //CHECK_CM_HR(hr);
        //hr = m_pCmKernel->SetKernelArg( 7, sizeof( UINT ), &slice_copy_height_row );
        //CHECK_CM_HR(hr);
        /*hr = m_pCmKernel->SetKernelArg( 9, sizeof( UINT ), &start_x );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 10, sizeof( UINT ), &start_y );
        CHECK_CM_HR(hr);*/

        hr = m_pCmDevice->CreateTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = pGPUCopyTask->AddKernel( m_pCmKernel );
        CHECK_CM_HR(hr);
        hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyThreadSpace(pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyBufferUP(pCMBufferUP);
        CHECK_CM_HR(hr);
        pLinearAddress += sliceCopyBufferUPSize - AddedShiftLeftOffset;
        totalBufferUPSize -= sliceCopyBufferUPSize;
        copy_height_row -= slice_copy_height_row;
        start_x = 0;
        start_y += slice_copy_height_row;
        if(totalBufferUPSize > 0)   //Intermediate event, we don't need it
        {
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        else //Last one event, need keep or destroy it
        {
            pInternalEvent->WaitForTaskFinished();

            CHECK_CM_HR(hr);
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
            CHECK_CM_HR(hr);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopyMirrorNV12CPUtoGPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride, 
                                    const UINT heightStride,
                                    mfxU32 format, 
                                    const UINT option,
                                    CmEvent* & pEvent )
{
    INT             hr                      = CM_SUCCESS;
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;
    CmKernel        *m_pCmKernel            = 0;
    CmBufferUP        *pCMBufferUP          = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_dword             = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0; 
    UINT            slice_copy_height_row   = 0;
    UINT            sliceCopyBufferUPSize   = 0;
    INT             totalBufferUPSize       = 0;
    UINT            start_x                 = 0;
    UINT            start_y                 = 0;

    

    if ( !pSurface )
    {
        return MFX_ERR_NULL_PTR;
    }
    width_byte                      = width;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region 
    copy_width_byte = IPP_MIN(stride_in_bytes, width_byte);
    copy_height_row = IPP_MIN(height_stride_in_rows, (UINT)height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory
    
    totalBufferUPSize = stride_in_bytes * height_stride_in_rows + stride_in_bytes * height/2;

    pLinearAddress  = (size_t)pSysMem;
    

    while (totalBufferUPSize > 0)
    {
#if defined(WIN64) || defined(LINUX64)//64-bit 
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;
#else  //32-bit 
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X86;
#endif

        //Calculate  Left Shift offset
        AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
        totalBufferUPSize   += AddedShiftLeftOffset;
        if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH)
        {
            slice_copy_height_row = ((CM_MAX_1D_SURF_WIDTH - AddedShiftLeftOffset)/(stride_in_bytes*(BLOCK_HEIGHT * INNER_LOOP))) * (BLOCK_HEIGHT * INNER_LOOP);
            sliceCopyBufferUPSize = slice_copy_height_row * stride_in_bytes * 3 / 2  + AddedShiftLeftOffset;
            return MFX_ERR_DEVICE_FAILED;
        }
        else
        {
            slice_copy_height_row = copy_height_row;
            sliceCopyBufferUPSize = totalBufferUPSize;
        }
        hr = m_pCmDevice->CreateBufferUP(  sliceCopyBufferUPSize, ( void * )pLinearAddressAligned, pCMBufferUP );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceMirror_write_NV12), m_pCmKernel);
        CHECK_CM_HR(hr);
        MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

        MFX_CHECK(pCMBufferUP, MFX_ERR_DEVICE_FAILED);
        hr = pCMBufferUP->GetIndex( pBufferIndexCM );
        CHECK_CM_HR(hr);
        hr = pSurface->GetIndex( pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
        threadHeight = ( UINT )ceil( ( double )slice_copy_height_row/BLOCK_HEIGHT );
        threadNum = threadWidth * threadHeight;
        hr = m_pCmKernel->SetThreadCount( threadNum );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pBufferIndexCM );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        width_dword = (UINT)ceil((double)width_byte / 4);
        stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

        hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &stride_in_dwords );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height_stride_in_rows);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &width_dword );
        CHECK_CM_HR(hr);
        //hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &threadHeight );
        //CHECK_CM_HR(hr);
        //hr = m_pCmKernel->SetKernelArg( 7, sizeof( UINT ), &slice_copy_height_row );
        //CHECK_CM_HR(hr);
        /*hr = m_pCmKernel->SetKernelArg( 9, sizeof( UINT ), &start_x );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 10, sizeof( UINT ), &start_y );
        CHECK_CM_HR(hr);*/

        hr = m_pCmDevice->CreateTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = pGPUCopyTask->AddKernel( m_pCmKernel );
        CHECK_CM_HR(hr);
        hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyThreadSpace(pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyBufferUP(pCMBufferUP);
        CHECK_CM_HR(hr);
        pLinearAddress += sliceCopyBufferUPSize - AddedShiftLeftOffset;
        totalBufferUPSize -= sliceCopyBufferUPSize;
        copy_height_row -= slice_copy_height_row;
        start_x = 0;
        start_y += slice_copy_height_row;
        if(totalBufferUPSize > 0)   //Intermediate event, we don't need it
        {
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        else //Last one event, need keep or destroy it
        {
            pInternalEvent->WaitForTaskFinished();

            CHECK_CM_HR(hr);
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
            CHECK_CM_HR(hr);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CmCopyWrapper::EnqueueCopyShiftP010GPUtoCPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride, 
                                    const UINT heightStride,
                                    mfxU32 format, 
                                    const UINT option,
                                    int bitshift,
                                    CmEvent* & pEvent )
{
    INT             hr                      = CM_SUCCESS;
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;
    CmKernel        *m_pCmKernel            = 0;
    CmBufferUP        *pCMBufferUP          = 0;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;//(CmEvent*)-1;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_dword             = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0; 
    UINT            slice_copy_height_row   = 0;
    UINT            sliceCopyBufferUPSize   = 0;
    INT             totalBufferUPSize       = 0;
    UINT            start_x                 = 0;
    UINT            start_y                 = 0;

    

    if ( !pSurface )
    {
        return MFX_ERR_NULL_PTR;
    }
    width_byte                      = width*2;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region 
    copy_width_byte = IPP_MIN(stride_in_bytes, width_byte);
    copy_height_row = IPP_MIN(height_stride_in_rows, (UINT)height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory
    
    totalBufferUPSize = stride_in_bytes * height_stride_in_rows + stride_in_bytes * height/2;

    pLinearAddress  = (size_t)pSysMem;
    

    while (totalBufferUPSize > 0)
    {
#if defined(WIN64) || defined(LINUX64)//64-bit 
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;
#else  //32-bit 
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X86;
#endif

        //Calculate  Left Shift offset
        AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
        totalBufferUPSize   += AddedShiftLeftOffset;
        if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH)
        {
            slice_copy_height_row = ((CM_MAX_1D_SURF_WIDTH - AddedShiftLeftOffset)/(stride_in_bytes*(BLOCK_HEIGHT * INNER_LOOP))) * (BLOCK_HEIGHT * INNER_LOOP);
            sliceCopyBufferUPSize = slice_copy_height_row * stride_in_bytes * 3 / 2  + AddedShiftLeftOffset;
            return MFX_ERR_DEVICE_FAILED;
        }
        else
        {
            slice_copy_height_row = copy_height_row;
            sliceCopyBufferUPSize = totalBufferUPSize;
        }
        hr = m_pCmDevice->CreateBufferUP(  sliceCopyBufferUPSize, ( void * )pLinearAddressAligned, pCMBufferUP );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_read_P010_shift), m_pCmKernel);
        CHECK_CM_HR(hr);
        MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

        MFX_CHECK(pCMBufferUP, MFX_ERR_DEVICE_FAILED);
        hr = pCMBufferUP->GetIndex( pBufferIndexCM );
        CHECK_CM_HR(hr);
        hr = pSurface->GetIndex( pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
        threadHeight = ( UINT )ceil( ( double )slice_copy_height_row/BLOCK_HEIGHT );
        threadNum = threadWidth * threadHeight;
        hr = m_pCmKernel->SetThreadCount( threadNum );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pBufferIndexCM );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        width_dword = (UINT)ceil((double)width_byte / 4);
        stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);

        hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &width_dword );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height);
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &bitshift );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &stride_in_dwords );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 7, sizeof( UINT ), &height_stride_in_rows );
        CHECK_CM_HR(hr);
        //hr = m_pCmKernel->SetKernelArg( 7, sizeof( UINT ), &slice_copy_height_row );
        //CHECK_CM_HR(hr);
        /*hr = m_pCmKernel->SetKernelArg( 9, sizeof( UINT ), &start_x );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 10, sizeof( UINT ), &start_y );
        CHECK_CM_HR(hr);*/

        hr = m_pCmDevice->CreateTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = pGPUCopyTask->AddKernel( m_pCmKernel );
        CHECK_CM_HR(hr);
        hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyThreadSpace(pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyBufferUP(pCMBufferUP);
        CHECK_CM_HR(hr);
        pLinearAddress += sliceCopyBufferUPSize - AddedShiftLeftOffset;
        totalBufferUPSize -= sliceCopyBufferUPSize;
        copy_height_row -= slice_copy_height_row;
        start_x = 0;
        start_y += slice_copy_height_row;
        if(totalBufferUPSize > 0)   //Intermediate event, we don't need it
        {
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        else //Last one event
        {
            pInternalEvent->WaitForTaskFinished();

            CHECK_CM_HR(hr);
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
            CHECK_CM_HR(hr);
        }
    }

    return MFX_ERR_NONE;
}
mfxStatus CmCopyWrapper::EnqueueCopyShiftP010CPUtoGPU(   CmSurface2D* pSurface,
                                    unsigned char* pSysMem,
                                    int width,
                                    int height,
                                    const UINT widthStride, 
                                    const UINT heightStride,
                                    mfxU32 format, 
                                    const UINT option,
                                    int bitshift,
                                    CmEvent* & pEvent )
{
    INT             hr                      = CM_SUCCESS;
    UINT            stride_in_bytes         = widthStride;
    UINT            stride_in_dwords        = 0;
    UINT            height_stride_in_rows   = heightStride;
    UINT            AddedShiftLeftOffset    = 0;
    size_t          pLinearAddress          = (size_t)pSysMem;
    size_t          pLinearAddressAligned   = 0;
    
    CmKernel        *m_pCmKernel        = NULL;
    CmBufferUP      *pCMBufferUP        = NULL;
    SurfaceIndex    *pBufferIndexCM     = NULL;
    SurfaceIndex    *pSurf2DIndexCM     = NULL;
    CmThreadSpace   *pTS                = NULL;
    CmTask          *pGPUCopyTask       = NULL;
    CmEvent         *pInternalEvent     = NULL;

    UINT            threadWidth             = 0;
    UINT            threadHeight            = 0;
    UINT            threadNum               = 0;
    UINT            width_dword             = 0;
    UINT            width_byte              = 0;
    UINT            copy_width_byte         = 0;
    UINT            copy_height_row         = 0;
    UINT            slice_copy_height_row   = 0;
    UINT            sliceCopyBufferUPSize   = 0;
    INT             totalBufferUPSize       = 0;
    UINT            start_x                 = 0;
    UINT            start_y                 = 0;

    

    if ( !pSurface )
    {
        return MFX_ERR_NULL_PTR;
    }
    width_byte                      = width * 2;

   //Align the width regarding stride
   if(stride_in_bytes == 0)
   {
        stride_in_bytes = width_byte;
   }

   if(height_stride_in_rows == 0)
   {
        height_stride_in_rows = height;
   }

    // the actual copy region 
    copy_width_byte = IPP_MIN(stride_in_bytes, width_byte);
    copy_height_row = IPP_MIN(height_stride_in_rows, (UINT)height);

    // Make sure stride and start address of system memory is 16-byte aligned.
    // if no padding in system memory , stride_in_bytes = width_byte.
    if(stride_in_bytes & 0xf)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if((pLinearAddress & 0xf) || (pLinearAddress == 0))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    //Calculate actual total size of system memory
    
    totalBufferUPSize = stride_in_bytes * height_stride_in_rows + stride_in_bytes * height/2;

    pLinearAddress  = (size_t)pSysMem;
    

    while (totalBufferUPSize > 0)
    {
#if defined(WIN64) || defined(LINUX64)//64-bit 
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X64;
#else  //32-bit 
            pLinearAddressAligned        = pLinearAddress & ADDRESS_PAGE_ALIGNMENT_MASK_X86;
#endif

        //Calculate  Left Shift offset
        AddedShiftLeftOffset = (UINT)(pLinearAddress - pLinearAddressAligned);
        totalBufferUPSize   += AddedShiftLeftOffset;
        if (totalBufferUPSize > CM_MAX_1D_SURF_WIDTH)
        {
            slice_copy_height_row = ((CM_MAX_1D_SURF_WIDTH - AddedShiftLeftOffset)/(stride_in_bytes*(BLOCK_HEIGHT * INNER_LOOP))) * (BLOCK_HEIGHT * INNER_LOOP);
            sliceCopyBufferUPSize = slice_copy_height_row * stride_in_bytes + AddedShiftLeftOffset;
        }
        else
        {
            slice_copy_height_row = copy_height_row;
            sliceCopyBufferUPSize = totalBufferUPSize;
        }
        hr = m_pCmDevice->CreateBufferUP(  sliceCopyBufferUPSize, ( void * )pLinearAddressAligned, pCMBufferUP );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateKernel(m_pCmProgram, CM_KERNEL_FUNCTION(surfaceCopy_write_P010_shift), m_pCmKernel);
        CHECK_CM_HR(hr);

        
        MFX_CHECK(m_pCmKernel, MFX_ERR_DEVICE_FAILED);

        MFX_CHECK(pCMBufferUP, MFX_ERR_DEVICE_FAILED);
        hr = pCMBufferUP->GetIndex( pBufferIndexCM );
        CHECK_CM_HR(hr);
        hr = pSurface->GetIndex( pSurf2DIndexCM );
        CHECK_CM_HR(hr);
        threadWidth = ( UINT )ceil( ( double )copy_width_byte/BLOCK_PIXEL_WIDTH/4 );
        threadHeight = ( UINT )ceil( ( double )slice_copy_height_row/BLOCK_HEIGHT );
        threadNum = threadWidth * threadHeight;
        hr = m_pCmKernel->SetThreadCount( threadNum );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->CreateThreadSpace( threadWidth, threadHeight, pTS );
        CHECK_CM_HR(hr);

        m_pCmKernel->SetKernelArg( 0, sizeof( SurfaceIndex ), pBufferIndexCM);
        CHECK_CM_HR(hr);
        m_pCmKernel->SetKernelArg( 1, sizeof( SurfaceIndex ), pSurf2DIndexCM);
        CHECK_CM_HR(hr);

            
        width_dword = (UINT)ceil((double)width_byte / 4);
        stride_in_dwords = (UINT)ceil((double)stride_in_bytes / 4);
        hr = m_pCmKernel->SetKernelArg( 2, sizeof( UINT ), &stride_in_dwords );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 3, sizeof( UINT ), &height_stride_in_rows );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 4, sizeof( UINT ), &AddedShiftLeftOffset );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 5, sizeof( UINT ), &bitshift );
        CHECK_CM_HR(hr);


        //this only works for the kernel surfaceCopy_write_32x32
        /*hr = m_pCmKernel->SetKernelArg( 6, sizeof( UINT ), &sizePerPixel );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 7, sizeof( UINT ), &start_x );
        CHECK_CM_HR(hr);
        hr = m_pCmKernel->SetKernelArg( 8, sizeof( UINT ), &start_y );
        CHECK_CM_HR(hr);*/

        hr = m_pCmDevice->CreateTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = pGPUCopyTask->AddKernel( m_pCmKernel );
        CHECK_CM_HR(hr);
        hr = m_pCmQueue->Enqueue( pGPUCopyTask, pInternalEvent, pTS );
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyTask(pGPUCopyTask);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyThreadSpace(pTS);
        CHECK_CM_HR(hr);
        hr = m_pCmDevice->DestroyBufferUP(pCMBufferUP);
        CHECK_CM_HR(hr);

        pLinearAddress += sliceCopyBufferUPSize - AddedShiftLeftOffset;
        totalBufferUPSize -= sliceCopyBufferUPSize;
        copy_height_row -= slice_copy_height_row;
        start_x = 0;
        start_y += slice_copy_height_row;
        if(totalBufferUPSize > 0)   //Intermediate event, we don't need it
        {
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
        }
        else //Last one event, need keep or destroy it
        {
            hr = pInternalEvent->WaitForTaskFinished();
            CHECK_CM_HR(hr);
            hr = m_pCmQueue->DestroyEvent(pInternalEvent);
            CHECK_CM_HR(hr);
        }

    }

    return MFX_ERR_NONE;
}
mfxStatus CmCopyWrapper::Initialize(eMFXHWType hwtype)
{
    cmStatus cmSts = CM_SUCCESS;


    if (!m_pCmDevice)
        return MFX_ERR_DEVICE_FAILED;



    //void *pCommonISACode = new mfxU32[CM_CODE_SIZE];

    //if (false == pCommonISACode)
    //{
    //    return MFX_ERR_DEVICE_FAILED;
    //}

    //memcpy((char*)pCommonISACode, cm_code, CM_CODE_SIZE);

    //cmSts = m_pCmDevice->LoadProgram(pCommonISACode, CM_CODE_SIZE, m_pCmProgram);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    //delete [] pCommonISACode;

#ifndef THREAD_SPACE

    cmSts = m_pCmDevice->CreateKernel(m_pCmProgram, _NAME(surfaceCopy_2DToBufferNV12_2) , m_pCmKernel);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

#else

    //cmSts = m_pCmDevice->CreateKernel(m_pCmProgram, _NAME(surfaceCopy_2DToBuffer_Single_nv12_128_full) , m_pCmKernel1);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    //cmSts = m_pCmDevice->CreateKernel(m_pCmProgram, _NAME(surfaceCopy_BufferTo2D_Single_nv12_128_full) , m_pCmKernel2);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

#endif

    //mfxU32 width_ = ALIGN128(width);

//    mfxU32 threadWidth = (unsigned int)ceil((double)width / 128);
//    mfxU32 threadHeight = (unsigned int)ceil((double)height / 8);

//    m_pCmKernel1->SetThreadCount(threadWidth * threadHeight);
//    m_pCmKernel2->SetThreadCount(threadWidth * threadHeight);

#ifndef THREAD_SPACE

    mfxU32 threadId = 0, block_y = 0, x, y;

    mfxU32 xxx = (int)ceil((float)height / 32) * 32;

    for (block_y = 0; block_y < xxx; block_y += 32)
    {
        for (x = 0; x < width; x += BLOCK_PIXEL_WIDTH)
        {
            for (y = block_y; y < block_y + 32; y += BLOCK_HEIGHT)
            {
                if (y < height)
                {
                    m_pCmKernel->SetThreadArg(threadId, 2, 4, &x);
                    m_pCmKernel->SetThreadArg(threadId, 3, 4, &y);

                    threadId++;
                }
            }
        }
    }
#else
 //   m_pCmDevice->CreateThreadSpace(threadWidth, threadHeight, m_pThreadSpace);
#endif

    cmSts = m_pCmDevice->CreateQueue(m_pCmQueue);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);
    m_tableCmRelations2.clear();
    m_tableCmIndex2.clear();
    //cmSts = m_pCmDevice->CreateTask(m_pCmTask1);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    //cmSts = m_pCmDevice->CreateTask(m_pCmTask2);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED)

    //cmSts = m_pCmTask1->AddKernel(m_pCmKernel1);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    //cmSts = m_pCmTask2->AddKernel(m_pCmKernel2);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;

} // mfxStatus CmCopyWrapper::Initialize(void)
mfxStatus CmCopyWrapper::InitializeSwapKernels(eMFXHWType hwtype)
{
    cmStatus cmSts = CM_SUCCESS;


    if (!m_pCmDevice)
        return MFX_ERR_DEVICE_FAILED;



    //void *pCommonISACode = new mfxU32[CM_CODE_SIZE];

    //if (false == pCommonISACode)
    //{
    //    return MFX_ERR_DEVICE_FAILED;
    //}

    //memcpy((char*)pCommonISACode, cm_code, CM_CODE_SIZE);

    //cmSts = m_pCmDevice->LoadProgram(pCommonISACode, CM_CODE_SIZE, m_pCmProgram);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    //delete [] pCommonISACode;

#ifndef THREAD_SPACE

    cmSts = m_pCmDevice->CreateKernel(m_pCmProgram, _NAME(surfaceCopy_2DToBufferNV12_2) , m_pCmKernel);
    CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

#else

    //cmSts = m_pCmDevice->CreateKernel(m_pCmProgram, _NAME(surfaceCopy_2DToBuffer_Single_nv12_128_full) , m_pCmKernel1);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    //cmSts = m_pCmDevice->CreateKernel(m_pCmProgram, _NAME(surfaceCopy_BufferTo2D_Single_nv12_128_full) , m_pCmKernel2);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

#endif

    //mfxU32 width_ = ALIGN128(width);

//    mfxU32 threadWidth = (unsigned int)ceil((double)width / 128);
//    mfxU32 threadHeight = (unsigned int)ceil((double)height / 8);

//    m_pCmKernel1->SetThreadCount(threadWidth * threadHeight);
//    m_pCmKernel2->SetThreadCount(threadWidth * threadHeight);

#ifndef THREAD_SPACE

    mfxU32 threadId = 0, block_y = 0, x, y;

    mfxU32 xxx = (int)ceil((float)height / 32) * 32;

    for (block_y = 0; block_y < xxx; block_y += 32)
    {
        for (x = 0; x < width; x += BLOCK_PIXEL_WIDTH)
        {
            for (y = block_y; y < block_y + 32; y += BLOCK_HEIGHT)
            {
                if (y < height)
                {
                    m_pCmKernel->SetThreadArg(threadId, 2, 4, &x);
                    m_pCmKernel->SetThreadArg(threadId, 3, 4, &y);

                    threadId++;
                }
            }
        }
    }
#else
 //   m_pCmDevice->CreateThreadSpace(threadWidth, threadHeight, m_pThreadSpace);
#endif
    #if defined(WIN64) || defined(WIN32)
        switch (hwtype)
        {
#if !(defined(AS_VPP_PLUGIN) || defined(UNIFIED_PLUGIN) || defined(AS_H265FEI_PLUGIN) || defined(AS_H264LA_PLUGIN))
        case MFX_HW_BDW:
        case MFX_HW_CHV:
            {
            cmSts = m_pCmDevice->LoadProgram((void*)cht_copy_kernel_genx,sizeof(cht_copy_kernel_genx),m_pCmProgram,"nojitter");
            break;
            }
#endif
        case MFX_HW_SCL:
        case MFX_HW_BXT:
        case MFX_HW_KBL:
            {
            cmSts = m_pCmDevice->LoadProgram((void*)skl_copy_kernel_genx,sizeof(skl_copy_kernel_genx),m_pCmProgram,"nojitter");
            break;
            }
        default:
            {
            cmSts = CM_FAILURE;
            break;
            }
        }
        CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);
    #endif


    //cmSts = m_pCmDevice->CreateTask(m_pCmTask1);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    //cmSts = m_pCmDevice->CreateTask(m_pCmTask2);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED)

    //cmSts = m_pCmTask1->AddKernel(m_pCmKernel1);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    //cmSts = m_pCmTask2->AddKernel(m_pCmKernel2);
    //CHECK_CM_STATUS(cmSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;

} // mfxStatus CmCopyWrapper::Initialize(void)

mfxStatus CmCopyWrapper::ReleaseCmSurfaces(void)
{
    std::map<void *, CmSurface2D *>::iterator itSrc;

    for (itSrc = m_tableCmRelations2.begin() ; itSrc != m_tableCmRelations2.end(); itSrc++)
    {
        CmSurface2D *temp = itSrc->second;
        m_pCmDevice->DestroySurface(temp);
    }
    m_tableCmRelations2.clear();
    return MFX_ERR_NONE;
}
mfxStatus CmCopyWrapper::Release(void)
{
    std::map<mfxU8 *, CmBufferUP *>::iterator itDst;
    ReleaseCmSurfaces();
    for (itDst = m_tableSysRelations.begin() ; itDst != m_tableSysRelations.end(); itDst++)
    {
        CmBufferUP *temp = itDst->second;
        m_pCmDevice->DestroyBufferUP(temp);
    }

    m_tableSysRelations.clear();

    if (m_pCmProgram)
    {
        m_pCmDevice->DestroyProgram(m_pCmProgram);
    }

    m_pCmProgram = NULL;

    if (m_pThreadSpace)
    {
        m_pCmDevice->DestroyThreadSpace(m_pThreadSpace);
    }

    m_pThreadSpace = NULL;

    if (m_pCmTask1)
    {
        m_pCmDevice->DestroyTask(m_pCmTask1);
    }

    m_pCmTask1 = NULL;

    if (m_pCmTask2)
    {
        m_pCmDevice->DestroyTask(m_pCmTask2);
    }

    m_pCmTask2 = NULL;

    if (m_pCmDevice)
    {
        DestroyCmDevice(m_pCmDevice);
    }

    m_pCmDevice = NULL;

    return MFX_ERR_NONE;

} // mfxStatus CmCopyWrapper::Release(void)

CmSurface2D * CmCopyWrapper::CreateCmSurface2D(void *pSrc, mfxU32 width, mfxU32 height, bool isSecondMode,
                                               std::map<void *, CmSurface2D *> & tableCmRelations,
                                               std::map<CmSurface2D *, SurfaceIndex *> & tableCmIndex)
{
    cmStatus cmSts = 0;

    CmSurface2D *pCmSurface2D;
    SurfaceIndex *pCmSrcIndex;

    std::map<void *, CmSurface2D *>::iterator it;

    it = tableCmRelations.find(pSrc);

    if (tableCmRelations.end() == it)
    {
        if (true == isSecondMode)
        {
#ifdef MFX_VA_WIN
            HRESULT hRes = S_OK;
            D3DLOCKED_RECT sLockRect;

            hRes |= ((IDirect3DSurface9 *)pSrc)->LockRect(&sLockRect, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);

            m_pCmDevice->CreateSurface2D(width, height, (D3DFORMAT)(MAKEFOURCC('N', 'V', '1', '2')), pCmSurface2D);
            pCmSurface2D->WriteSurface((mfxU8 *) sLockRect.pBits, NULL);

            ((IDirect3DSurface9 *)pSrc)->UnlockRect();
#endif // MFX_VA_WIN

#if defined(MFX_VA_LINUX) || defined(MFX_VA_ANDROID)
            m_pCmDevice->CreateSurface2D(width, height, (CM_SURFACE_FORMAT)(MFX_MAKEFOURCC('N', 'V', '1', '2')), pCmSurface2D);
#endif
        }
        else
        {
            cmSts = m_pCmDevice->CreateSurface2D((AbstractSurfaceHandle *) pSrc, pCmSurface2D);
            CHECK_CM_STATUS_RET_NULL(cmSts, MFX_ERR_DEVICE_FAILED);
            tableCmRelations.insert(std::pair<void *, CmSurface2D *>(pSrc, pCmSurface2D));
        }

        cmSts = pCmSurface2D->GetIndex(pCmSrcIndex);
        CHECK_CM_STATUS_RET_NULL(cmSts, MFX_ERR_DEVICE_FAILED);

        tableCmIndex.insert(std::pair<CmSurface2D *, SurfaceIndex *>(pCmSurface2D, pCmSrcIndex));
    }
    else
    {
        pCmSurface2D = it->second;
    }

    return pCmSurface2D;

} // CmSurface2D * CmCopyWrapper::CreateCmSurface2D(void *pSrc, mfxU32 width, mfxU32 height, bool isSecondMode)

CmBufferUP * CmCopyWrapper::CreateUpBuffer(mfxU8 *pDst, mfxU32 memSize,
                                           std::map<mfxU8 *, CmBufferUP *> & tableSysRelations,
                                           std::map<CmBufferUP *,  SurfaceIndex *> & tableSysIndex)
{
    cmStatus cmSts = 0;

    CmBufferUP *pCmUserBuffer;
    SurfaceIndex *pCmDstIndex;

    std::map<mfxU8 *, CmBufferUP *>::iterator it;
    it = tableSysRelations.find(pDst);

    if (tableSysRelations.end() == it)
    {
        cmSts = m_pCmDevice->CreateBufferUP(memSize, pDst, pCmUserBuffer);
        CHECK_CM_STATUS_RET_NULL(cmSts, MFX_ERR_DEVICE_FAILED);

        tableSysRelations.insert(std::pair<mfxU8 *, CmBufferUP *>(pDst, pCmUserBuffer));

        cmSts = pCmUserBuffer->GetIndex(pCmDstIndex);
        CHECK_CM_STATUS_RET_NULL(cmSts, MFX_ERR_DEVICE_FAILED);

        tableSysIndex.insert(std::pair<CmBufferUP *, SurfaceIndex *>(pCmUserBuffer, pCmDstIndex));
    }
    else
    {
        pCmUserBuffer = it->second;
    }

    return pCmUserBuffer;

} // CmBufferUP * CmCopyWrapper::CreateUpBuffer(mfxU8 *pDst, mfxU32 memSize)
mfxStatus CmCopyWrapper::IsCmCopySupported(mfxFrameSurface1 *pSurface, IppiSize roi)
{
    if ((roi.width & 15) || (roi.height & 7))
    {
        return MFX_ERR_UNSUPPORTED;
    }

    if(pSurface->Info.FourCC != MFX_FOURCC_NV12)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    if(pSurface->Data.UV - pSurface->Data.Y != pSurface->Data.Pitch * pSurface->Info.Height)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;

} // mfxStatus CmCopyWrapper::IsCmCopySupported(mfxFrameSurface1 *pSurface, IppiSize roi)

mfxStatus CmCopyWrapper::CopySystemToVideoMemoryAPI(void *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::CopySystemToVideoMemoryAPI");
    cmStatus cmSts = 0;

    CmEvent* e = (CmEvent*)(-1);//NULL;
    //CM_STATUS sts;
    mfxStatus status = MFX_ERR_NONE;

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

     // create or find already associated cm surface 2d
    CmSurface2D *pCmSurface2D;
    //m_pCmDevice->CreateSurface2D((AbstractSurfaceHandle*)pDst,pCmSurface2D);
    pCmSurface2D = CreateCmSurface2D(pDst, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmQueue->EnqueueCopyCPUToGPUFullStride(pCmSurface2D, pSrc, srcPitch, srcUVOffset, 1, e);

    if (CM_SUCCESS == cmSts )
    {
        return status;
    }else{
        status = MFX_ERR_DEVICE_FAILED;
    }
    //if(e) m_pCmQueue->DestroyEvent(e);
    //m_pCmDevice->DestroySurface(pCmSurface2D);
    return status;
}
mfxStatus CmCopyWrapper::CopySwapSystemToVideoMemory(void *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi, mfxU32 format)
{
    CmEvent* e = (CmEvent*)(-1);//NULL;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

     // create or find already associated cm surface 2d
    CmSurface2D *pCmSurface2D;
    pCmSurface2D = CreateCmSurface2D(pDst, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    return EnqueueCopySwapRBCPUtoGPU( pCmSurface2D, pSrc,roi.width,roi.height, srcPitch, srcUVOffset,format, 1, e);

}
mfxStatus CmCopyWrapper::CopyShiftSystemToVideoMemory(void *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi, mfxU32 bitshift)
{
    CmEvent* e = (CmEvent*)(-1);//NULL;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

     // create or find already associated cm surface 2d
    CmSurface2D *pCmSurface2D;
    pCmSurface2D = CreateCmSurface2D(pDst, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    return EnqueueCopyShiftP010CPUtoGPU( pCmSurface2D, pSrc,roi.width,roi.height, srcPitch, srcUVOffset,0, 1,bitshift, e);

}

mfxStatus CmCopyWrapper::CopyShiftVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, void *pSrc, mfxU32 srcPitch, IppiSize roi, mfxU32 bitshift)
{
    CmEvent* e = (CmEvent*)(-1);//NULL;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    // create or find already associated cm surface 2d
    CmSurface2D *pCmSurface2D;
    pCmSurface2D = CreateCmSurface2D(pSrc, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);

    return EnqueueCopyShiftP010GPUtoCPU( pCmSurface2D, pDst, roi.width, roi.height, dstPitch, dstUVOffset, 0, 1,bitshift, e);
}
mfxStatus CmCopyWrapper::CopyVideoToSystemMemoryAPI(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, void *pSrc, mfxU32 srcPitch, IppiSize roi)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::CopyVideoToSystemMemoryAPI");
    cmStatus cmSts = 0;
    CmEvent* e = (CmEvent*)-1;//NULL;
    mfxStatus status = MFX_ERR_NONE;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;
    CmSurface2D *pCmSurface2D;
    
    pCmSurface2D = CreateCmSurface2D(pSrc, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);
    
    cmSts = m_pCmQueue->EnqueueCopyGPUToCPUFullStride(pCmSurface2D, pDst, dstPitch, dstUVOffset, 1, e);
    
    if (CM_SUCCESS == cmSts)
    {
        return status;
    }else{
        status = MFX_ERR_DEVICE_FAILED;
    }

    return status;
}

mfxStatus CmCopyWrapper::CopySwapVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, void *pSrc, mfxU32 srcPitch, IppiSize roi, mfxU32 format)
{
    CmEvent* e = (CmEvent*)-1;//NULL;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    // create or find already associated cm surface 2d
    CmSurface2D *pCmSurface2D;

    pCmSurface2D = CreateCmSurface2D(pSrc, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);
    return EnqueueCopySwapRBGPUtoCPU(pCmSurface2D,pDst,roi.width,roi.height,dstPitch,dstUVOffset,format,0,e);

}
mfxStatus CmCopyWrapper::CopyMirrorVideoToSystemMemory(mfxU8 *pDst, mfxU32 dstPitch, mfxU32 dstUVOffset, void *pSrc, mfxU32 srcPitch, IppiSize roi, mfxU32 format)
{
    CmEvent* e = (CmEvent*)-1;//NULL;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    // create or find already associated cm surface 2d
    CmSurface2D *pCmSurface2D;

    pCmSurface2D = CreateCmSurface2D(pSrc, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);
    return EnqueueCopyMirrorNV12GPUtoCPU(pCmSurface2D,pDst,roi.width,roi.height,dstPitch,dstUVOffset,format,0,e);

}
mfxStatus CmCopyWrapper::CopyMirrorSystemToVideoMemory(void *pDst, mfxU32 dstPitch, mfxU8 *pSrc, mfxU32 srcPitch, mfxU32 srcUVOffset, IppiSize roi, mfxU32 format)
{
    CmEvent* e = (CmEvent*)-1;//NULL;
    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    // create or find already associated cm surface 2d
    CmSurface2D *pCmSurface2D;

    pCmSurface2D = CreateCmSurface2D(pDst, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pCmSurface2D, MFX_ERR_DEVICE_FAILED);
    return EnqueueCopyMirrorNV12CPUtoGPU(pCmSurface2D,pSrc,roi.width,roi.height,srcPitch,srcUVOffset,format,0,e);

}
mfxStatus CmCopyWrapper::CopyVideoToVideoMemoryAPI(void *pDst, void *pSrc, IppiSize roi)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "CmCopyWrapper::CopyVideoToVideoMemoryAPI");
    cmStatus cmSts = 0;

    CmEvent* e = (CmEvent*)-1;//NULL;
    mfxStatus status = MFX_ERR_NONE;

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

     // create or find already associated cm surface 2d
    CmSurface2D *pDstCmSurface2D;
    pDstCmSurface2D = CreateCmSurface2D(pDst, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pDstCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2D *pSrcCmSurface2D;
    pSrcCmSurface2D = CreateCmSurface2D(pSrc, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pSrcCmSurface2D, MFX_ERR_DEVICE_FAILED);

    cmSts = m_pCmQueue->EnqueueCopyGPUToGPU(pDstCmSurface2D, pSrcCmSurface2D, e);

    if (CM_SUCCESS == cmSts)
    {
        return status;
    }else{
        status = MFX_ERR_DEVICE_FAILED;
    }

    return status;
}
mfxStatus CmCopyWrapper::CopySwapVideoToVideoMemory(void *pDst, void *pSrc, IppiSize roi, mfxU32 format)
{
    CmEvent* e = NULL;

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    // create or find already associated cm surface 2d
    CmSurface2D *pDstCmSurface2D;
    pDstCmSurface2D = CreateCmSurface2D(pDst, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pDstCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2D *pSrcCmSurface2D;
    pSrcCmSurface2D = CreateCmSurface2D(pSrc, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pSrcCmSurface2D, MFX_ERR_DEVICE_FAILED);

    return EnqueueCopySwapRBGPUtoGPU(pSrcCmSurface2D, pDstCmSurface2D, width, height, format, 0, e);
}
mfxStatus CmCopyWrapper::CopyMirrorVideoToVideoMemory(void *pDst, void *pSrc, IppiSize roi, mfxU32 format)
{
    CmEvent* e = NULL;

    mfxU32 width  = roi.width;
    mfxU32 height = roi.height;

    // create or find already associated cm surface 2d
    CmSurface2D *pDstCmSurface2D;
    pDstCmSurface2D = CreateCmSurface2D(pDst, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pDstCmSurface2D, MFX_ERR_DEVICE_FAILED);

    CmSurface2D *pSrcCmSurface2D;
    pSrcCmSurface2D = CreateCmSurface2D(pSrc, width, height, false, m_tableCmRelations2, m_tableCmIndex2);
    CHECK_CM_NULL_PTR(pSrcCmSurface2D, MFX_ERR_DEVICE_FAILED);

    return EnqueueCopyMirrorGPUtoGPU(pSrcCmSurface2D, pDstCmSurface2D, width, height, format, 0, e);
}
#endif // defined (MFX_VA) && !defined(OSX)
